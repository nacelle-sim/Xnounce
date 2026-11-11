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

Profile profileFor(PaPreset p) {
  switch (p) {
    case PaPreset::Worn:
      return {250, 5000, 2200, 4, 1.2, 1.6, 0.004, 0.002, 0.4, 0, 0};
    case PaPreset::Old:
      return {420, 3300, 1800, 7, 1.4, 2.6, 0.011, 0.006, 2.0, 2.5, 0.06};
    case PaPreset::Modern:
      break;
  }
  return {};
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

std::vector<int16_t> agePa(const std::vector<int16_t>& in, int rate,
                           PaPreset preset, uint64_t seed) {
  if (preset == PaPreset::Modern || in.empty() || rate < 8000) return in;
  const Profile pf = profileFor(preset);
  Rand rnd(seed);
  const size_t n = in.size();

  // The voice through the speaker and the amplifier.
  std::vector<double> x(n);
  for (size_t k = 0; k < n; ++k) x[k] = in[k] / 32768.0;
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

  // Dropouts: the level dips to 15 % for 60 to 150 ms, with 5 ms edges.
  const double perSample = pf.dropoutsPerMin / 60.0 / rate;
  if (perSample > 0) {
    const size_t edge = static_cast<size_t>(0.005 * rate);
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
  // second and third harmonics, and crackle.
  Biquad hissHp = Biquad::highPass(pf.lowHz, rate),
         hissLp = Biquad::lowPass(std::min(pf.highHz * 1.3, nyq), rate);
  const double crackleChance = pf.cracklesPerSec / rate;
  double crackle = 0;
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
    if (rnd.unit() < crackleChance)
      crackle = (0.05 + 0.1 * rnd.unit()) * (rnd.unit() < 0.5 ? -1 : 1);
    x[k] += hum + hiss + crackle;
    crackle *= 0.6;  // a crackle is a short decaying spike
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
  for (size_t k = 0; k < n; ++k)
    out[k] = static_cast<int16_t>(
        std::clamp(std::lround(x[k] * 32767.0), -32768L, 32767L));
  return out;
}

}  // namespace xn
