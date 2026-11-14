/// @file
/// Implements `src/audio/pa_effects.h`; the interface is documented there.
#include "audio/pa_effects.h"

#include <algorithm>
#include <cmath>

namespace xn {

namespace {

constexpr double kPi = 3.14159265358979323846;

/// One biquad of the RBJ audio EQ cookbook, direct form 1, in doubles.
struct Biquad {
  double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
  double x1 = 0, x2 = 0, y1 = 0, y2 = 0;

  static Biquad make(double b0, double b1, double b2, double a0, double a1,
                     double a2) {
    Biquad q;
    q.b0 = b0 / a0;
    q.b1 = b1 / a0;
    q.b2 = b2 / a0;
    q.a1 = a1 / a0;
    q.a2 = a2 / a0;
    return q;
  }
  static Biquad highPass(double fc, int rate, double qf = 0.7071) {
    const double w0 = 2 * kPi * fc / rate, cw = std::cos(w0);
    const double al = std::sin(w0) / (2 * qf);
    return make((1 + cw) / 2, -(1 + cw), (1 + cw) / 2, 1 + al, -2 * cw, 1 - al);
  }
  static Biquad lowPass(double fc, int rate, double qf = 0.7071) {
    const double w0 = 2 * kPi * fc / rate, cw = std::cos(w0);
    const double al = std::sin(w0) / (2 * qf);
    return make((1 - cw) / 2, 1 - cw, (1 - cw) / 2, 1 + al, -2 * cw, 1 - al);
  }
  static Biquad lowShelf(double fc, int rate, double gainDb) {
    const double a = std::pow(10.0, gainDb / 40.0);
    const double w0 = 2 * kPi * fc / rate, cw = std::cos(w0);
    const double al = std::sin(w0) / 2 * std::sqrt(2.0);
    const double sa = 2 * std::sqrt(a) * al;
    return make(a * ((a + 1) - (a - 1) * cw + sa),
                2 * a * ((a - 1) - (a + 1) * cw),
                a * ((a + 1) - (a - 1) * cw - sa), (a + 1) + (a - 1) * cw + sa,
                -2 * ((a - 1) + (a + 1) * cw), (a + 1) + (a - 1) * cw - sa);
  }
  static Biquad highShelf(double fc, int rate, double gainDb) {
    const double a = std::pow(10.0, gainDb / 40.0);
    const double w0 = 2 * kPi * fc / rate, cw = std::cos(w0);
    const double al = std::sin(w0) / 2 * std::sqrt(2.0);
    const double sa = 2 * std::sqrt(a) * al;
    return make(a * ((a + 1) + (a - 1) * cw + sa),
                -2 * a * ((a - 1) + (a + 1) * cw),
                a * ((a + 1) + (a - 1) * cw - sa), (a + 1) - (a - 1) * cw + sa,
                2 * ((a - 1) - (a + 1) * cw), (a + 1) - (a - 1) * cw - sa);
  }
  static Biquad peak(double fc, int rate, double gainDb, double qf) {
    const double w0 = 2 * kPi * fc / rate, cw = std::cos(w0);
    const double al = std::sin(w0) / (2 * qf);
    const double a = std::pow(10.0, gainDb / 40.0);
    return make(1 + al * a, -2 * cw, 1 - al * a, 1 + al / a, -2 * cw,
                1 - al / a);
  }
  double run(double x) {
    double y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
    if (std::fabs(y) < 1e-12) y = 0;  // no denormals in a long silence
    x2 = x1;
    x1 = x;
    y2 = y1;
    y1 = y;
    return y;
  }
};

/// A small, fast and repeatable random source (xorshift64*).
struct Rand {
  uint64_t s;
  explicit Rand(uint64_t seed) : s(seed ? seed : 0x9E3779B97F4A7C15ull) {}
  uint64_t next() {
    s ^= s >> 12;
    s ^= s << 25;
    s ^= s >> 27;
    return s * 2685821657736338717ull;
  }
  /// Uniform in [0, 1).
  double unit() { return static_cast<double>(next() >> 11) * 0x1.0p-53; }
  /// Uniform in [-1, 1).
  double sym() { return unit() * 2 - 1; }
};

/// What a preset does. Levels are fractions of full scale.
struct Profile {
  double lowHz, highHz;       ///< The speaker's band.
  double resHz, resDb, resQ;  ///< Its resonance.
  double drive;               ///< Soft-clip drive; 1 is clean.
  double hiss, hum;           ///< Noise floor and 400 Hz hum.
  double cracklesPerSec;
  double dropoutsPerMin;  ///< Short dips of the signal.
  double squeal;          ///< Feedback burst level at the start; 0 none.
};

Profile profileFor(PaPreset p, bool fullBody) {
  // Crackle is a soft tick now, and rarer (Run1(XN): the clicks were too much).
  switch (p) {
    case PaPreset::Worn:
      return {fullBody ? 170.0 : 250.0,
              5000,
              2200,
              4,
              1.2,
              1.6,
              0.004,
              0.002,
              0.15,
              0,
              0};
    case PaPreset::Old:
      return {fullBody ? 280.0 : 420.0,
              3300,
              1800,
              7,
              1.4,
              2.6,
              0.011,
              0.006,
              0.8,
              2.5,
              0.06};
    case PaPreset::Modern:
      break;
  }
  return {};
}

/// A two-pole resonator: an impulse into it rings as a short, band-limited
/// tick instead of a click.
struct Ping {
  double c1 = 0, c2 = 0, y1 = 0, y2 = 0, kick = 0;
  Ping(double hz, double decayS, int rate) {
    const double r = std::exp(-1.0 / (decayS * rate));
    const double w = 2 * kPi * hz / rate;
    c1 = 2 * r * std::cos(w);
    c2 = -r * r;
    kick = std::sin(w);  // so an impulse of 1 peaks near 1
  }
  double run(double impulse) {
    const double y = c1 * y1 + c2 * y2 + impulse * kick;
    y2 = y1;
    y1 = std::fabs(y) < 1e-12 ? 0 : y;
    return y1;
  }
};

std::vector<double> toDoubles(const std::vector<int16_t>& in) {
  std::vector<double> x(in.size());
  for (size_t k = 0; k < in.size(); ++k) x[k] = in[k] / 32768.0;
  return x;
}

int16_t toSample(double v) {
  return static_cast<int16_t>(
      std::clamp(std::lround(softLimit(v) * 32767.0), -32767L, 32767L));
}

double rms(const std::vector<double>& v) {
  if (v.empty()) return 0;
  double sum = 0;
  for (double x : v) sum += x * x;
  return std::sqrt(sum / static_cast<double>(v.size()));
}

uint64_t fnv(const std::string& s, uint64_t h = 1469598103934665603ull) {
  for (unsigned char c : s) h = (h ^ c) * 1099511628211ull;
  return h;
}

}  // namespace

PaPreset resolvePaPreset(int setting, const std::string& tailNumber) {
  switch (setting) {
    case 1:
      return PaPreset::Worn;
    case 2:
      return PaPreset::Old;
    case 3:
      return static_cast<PaPreset>(fnv(tailNumber) % 3);
    default:
      return PaPreset::Modern;
  }
}

uint64_t paSeed(const std::string& tailNumber, const std::string& path) {
  return fnv(path, fnv(tailNumber));
}

double softLimit(double x) {
  constexpr double kKnee = 0.7;
  const double a = std::fabs(x);
  if (!(a > kKnee)) return std::isfinite(x) ? x : 0.0;
  return std::copysign(
      kKnee + (1 - kKnee) * std::tanh((a - kKnee) / (1 - kKnee)), x);
}

std::vector<int16_t> agePa(const std::vector<int16_t>& in, int rate,
                           PaPreset preset, uint64_t seed, bool fullBody) {
  if (preset == PaPreset::Modern || in.empty() || rate < 8000) return in;
  const Profile pf = profileFor(preset, fullBody);
  Rand rnd(seed);
  const size_t n = in.size();

  // The voice through the speaker and the amplifier.
  std::vector<double> x = toDoubles(in);
  const double inLevel = rms(x);
  const double nyq = 0.45 * rate;
  Biquad hp1 = Biquad::highPass(pf.lowHz, rate),
         hp2 = Biquad::highPass(pf.lowHz, rate);
  Biquad lp1 = Biquad::lowPass(std::min(pf.highHz, nyq), rate),
         lp2 = Biquad::lowPass(std::min(pf.highHz, nyq), rate);
  Biquad res = Biquad::peak(std::min(pf.resHz, nyq), rate, pf.resDb, pf.resQ);
  const double norm = std::tanh(pf.drive);
  for (size_t k = 0; k < n; ++k) {
    double y = lp2.run(lp1.run(hp2.run(hp1.run(x[k]))));
    y = res.run(y);
    x[k] = std::tanh(y * pf.drive) / norm;
  }
  // The band-limited voice is quieter: bring it back to the input's level,
  // by no more than 4x.
  const double outLevel = rms(x);
  const double makeup =
      outLevel > 1e-9 ? std::min(4.0, inLevel / outLevel) : 1.0;
  for (double& v : x) v *= makeup;

  // Dropouts: the level dips to 15 % for 60 to 150 ms, with 20 ms edges.
  const double perSample = pf.dropoutsPerMin / 60.0 / rate;
  if (perSample > 0) {
    const size_t edge = static_cast<size_t>(0.02 * rate);
    for (size_t k = 0; k < n; ++k) {
      if (rnd.unit() >= perSample) continue;
      const size_t len = static_cast<size_t>((0.06 + 0.09 * rnd.unit()) * rate);
      for (size_t j = 0; j < len && k + j < n; ++j) {
        const size_t fromEdge = std::min(j, len - 1 - j);
        const double depth =
            fromEdge >= edge ? 1.0 : static_cast<double>(fromEdge) / edge;
        x[k + j] *= 1.0 - 0.85 * depth;
      }
      k += len;
    }
  }

  // The noise floor: hiss through the speaker's band, the hum with its
  // second and third harmonics, and crackle as soft ticks. It fades in and
  // out with the recording over 80 ms, so it never starts or stops with a
  // step.
  Biquad hissHp = Biquad::highPass(pf.lowHz, rate),
         hissLp = Biquad::lowPass(std::min(pf.highHz * 1.3, nyq), rate);
  const double crackleChance = pf.cracklesPerSec / rate;
  Ping tick(std::min(2600.0, nyq), 0.0015, rate);
  const double fade = std::max(1.0, 0.08 * rate);
  // The 400 Hz hum as a rotating phasor (cos, sin); its harmonics follow from
  // sin 2a = 2 sin a cos a and sin 3a = 3 sin a - 4 sin^3 a.
  const double w = 2 * kPi * 400 / rate, cw = std::cos(w), sw = std::sin(w);
  double hc = 1, hs = 0;
  for (size_t k = 0; k < n; ++k) {
    const double s1 = hs, s2 = 2 * hs * hc, s3 = 3 * hs - 4 * hs * hs * hs;
    const double hum = pf.hum * (s1 + 0.5 * s2 + 0.25 * s3);
    const double nc = hc * cw - hs * sw;
    hs = hs * cw + hc * sw;
    hc = nc;
    if ((k & 4095) == 0) {  // keep the phasor on the unit circle
      const double r = std::sqrt(hc * hc + hs * hs);
      hc /= r;
      hs /= r;
    }
    const double hiss = pf.hiss * 3.0 * hissLp.run(hissHp.run(rnd.sym()));
    double impulse = 0;
    if (rnd.unit() < crackleChance)
      impulse = (0.01 + 0.02 * rnd.unit()) * (rnd.unit() < 0.5 ? -1 : 1);
    const double crackle = tick.run(impulse);
    const double env = std::min({1.0, static_cast<double>(k) / fade,
                                 static_cast<double>(n - 1 - k) / fade});
    x[k] += env * (hum + hiss + crackle);
  }

  // Feedback as the handset keys: a tone near the speaker's resonance that
  // swells and dies within 0.4 s.
  if (pf.squeal > 0) {
    const double f = pf.resHz * (1.3 + 0.3 * rnd.unit());
    const size_t len = std::min(n, static_cast<size_t>(0.4 * rate));
    for (size_t k = 0; k < len; ++k) {
      const double t = static_cast<double>(k) / rate;
      const double env = std::sin(kPi * t / 0.4);
      x[k] += pf.squeal * env * env * std::sin(2 * kPi * f * t);
    }
  }

  std::vector<int16_t> out(n);
  for (size_t k = 0; k < n; ++k) out[k] = toSample(x[k]);
  return out;
}

double loudnessDbfs(const std::vector<int16_t>& pcm, int rate) {
  if (pcm.empty() || rate < 8000) return -100;
  // Weighted like hearing: less below 60 Hz, more in the presence range.
  Biquad hp = Biquad::highPass(60, rate, 0.5);
  Biquad shelf = Biquad::highShelf(1500, rate, 4);
  const size_t block = static_cast<size_t>(0.4 * rate);
  std::vector<double> blocks;
  double sum = 0;
  size_t inBlock = 0;
  for (int16_t v : pcm) {
    const double y = shelf.run(hp.run(v / 32768.0));
    sum += y * y;
    if (++inBlock == block) {
      blocks.push_back(sum / static_cast<double>(block));
      sum = 0;
      inBlock = 0;
    }
  }
  if (inBlock > block / 4) blocks.push_back(sum / static_cast<double>(inBlock));
  // Gates: silence (below -70 dBFS), then everything 10 dB under the rest's
  // mean, so pauses between sentences don't count.
  auto meanAbove = [&](double floor) {
    double s = 0;
    int c = 0;
    for (double b : blocks)
      if (b > floor) s += b, ++c;
    return c > 0 ? s / c : 0.0;
  };
  const double ungated = meanAbove(1e-7);
  if (!(ungated > 0)) return -100;
  const double gated = meanAbove(ungated * 0.1);
  return 10 * std::log10(gated > 0 ? gated : ungated);
}

void normalizeLoudness(std::vector<int16_t>& pcm, int rate, double targetDbfs) {
  const double now = loudnessDbfs(pcm, rate);
  if (now <= -99) return;
  const double db = std::clamp(targetDbfs - now, -20.0, 15.0);
  if (std::fabs(db) < 0.1) return;
  const double g = std::pow(10.0, db / 20.0);
  for (int16_t& v : pcm) v = toSample(v / 32768.0 * g);
}

std::vector<int16_t> cabinAcoustics(const std::vector<int16_t>& in, int rate) {
  if (in.empty() || rate < 8000) {
    std::vector<int16_t> out(in.size() * 2);
    for (size_t k = 0; k < in.size(); ++k) out[2 * k] = out[2 * k + 1] = in[k];
    return out;
  }
  const size_t n = in.size();
  std::vector<double> x = toDoubles(in);
  const double inLevel = rms(x);

  // Body: a low shelf below about 220 Hz.
  Biquad body = Biquad::lowShelf(220, rate, 3.5);
  for (double& v : x) v = body.run(v);

  // Ceiling speakers spread along the cabin: each ear hears the nearest one
  // first and the others a few milliseconds later, a little duller.
  struct Tap {
    double ms, gain;
  };
  static const Tap kLeft[] = {
      {3.1, 0.42}, {7.3, 0.33}, {11.9, 0.26}, {17.2, 0.19}, {23.5, 0.13}};
  static const Tap kRight[] = {
      {4.4, 0.40}, {8.9, 0.31}, {13.6, 0.24}, {19.8, 0.17}, {25.1, 0.12}};
  const size_t ring = static_cast<size_t>(0.03 * rate) + 2;
  std::vector<double> hist(ring, 0.0);
  size_t head = 0;
  auto at = [&](double ms) {
    const size_t d = std::min(ring - 1, static_cast<size_t>(ms * 0.001 * rate));
    return hist[(head + ring - d) % ring];
  };
  // The room: four delay lines mixed by a Hadamard matrix, each losing what
  // a 0.5 s decay asks for and its highs on every pass.
  constexpr double kRt60 = 0.5;
  const double scale = rate / 44100.0;
  const size_t lens[4] = {static_cast<size_t>(1301 * scale) + 1,
                          static_cast<size_t>(1693 * scale) + 1,
                          static_cast<size_t>(1951 * scale) + 1,
                          static_cast<size_t>(2213 * scale) + 1};
  std::vector<double> line[4];
  size_t pos[4] = {0, 0, 0, 0};
  double fb[4], damp[4] = {0, 0, 0, 0};
  for (int i = 0; i < 4; ++i) {
    line[i].assign(lens[i], 0.0);
    fb[i] =
        std::pow(10.0, -3.0 * static_cast<double>(lens[i]) / (rate * kRt60));
  }
  constexpr double kDry = 0.85, kWet = 0.2, kDamp = 0.35;
  const double toneA = std::exp(-2 * kPi * 5000.0 / rate);
  double toneL = 0, toneR = 0;

  std::vector<double> left(n), right(n);
  for (size_t k = 0; k < n; ++k) {
    hist[head] = x[k];
    double el = 0, er = 0;
    for (const Tap& t : kLeft) el += t.gain * at(t.ms);
    for (const Tap& t : kRight) er += t.gain * at(t.ms);
    toneL = el + toneA * (toneL - el);
    toneR = er + toneA * (toneR - er);
    head = (head + 1) % ring;

    double o[4];
    for (int i = 0; i < 4; ++i) o[i] = line[i][pos[i]];
    const double h[4] = {
        (o[0] + o[1] + o[2] + o[3]) * 0.5, (o[0] - o[1] + o[2] - o[3]) * 0.5,
        (o[0] + o[1] - o[2] - o[3]) * 0.5, (o[0] - o[1] - o[2] + o[3]) * 0.5};
    const double feed = (toneL + toneR) * 0.5;
    for (int i = 0; i < 4; ++i) {
      damp[i] = h[i] * fb[i] * (1 - kDamp) + damp[i] * kDamp;
      line[i][pos[i]] = feed + damp[i];
      pos[i] = (pos[i] + 1) % lens[i];
    }
    left[k] = kDry * x[k] + toneL + kWet * (o[0] + o[2]);
    right[k] = kDry * x[k] + toneR + kWet * (o[1] + o[3]);
  }

  // Back to the input's loudness, then the soft limiter.
  double sum = 0;
  for (size_t k = 0; k < n; ++k)
    sum += 0.5 * (left[k] * left[k] + right[k] * right[k]);
  const double outLevel = std::sqrt(sum / static_cast<double>(n));
  const double g = outLevel > 1e-9 ? inLevel / outLevel : 1.0;
  std::vector<int16_t> out(2 * n);
  for (size_t k = 0; k < n; ++k) {
    out[2 * k] = toSample(left[k] * g);
    out[2 * k + 1] = toSample(right[k] * g);
  }
  return out;
}

}  // namespace xn
