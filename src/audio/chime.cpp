/// @file
/// Implements `src/audio/chime.h`: a 2.8 s buzz at 300 Hz with odd harmonics
/// and a 25 Hz flutter, modeled on a recording of the A320's.

#include "audio/chime.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace xn {

namespace {

constexpr double kPi = 3.14159265358979323846;

// The buzzer's shape.
constexpr double kF0 = 300.0;  // Hz
constexpr int kHarmonics = 15;
constexpr double kLevelDb[kHarmonics] = {0.0,   -32.4, -12.3, -46.9, -22.1,
                                         -43.6, -24.6, -45.0, -45.7, -53.7,
                                         -43.2, -54.0, -40.2, -59.8, -43.3};
constexpr double kPhase[kHarmonics] = {2.496,  -0.792, 2.926, 0.084,  3.012,
                                       0.078,  -0.478, 1.297, 1.769,  -0.056,
                                       -1.498, -0.783, 1.179, -1.626, 2.312};
constexpr double kFlutterHz = 25.0;
constexpr double kFlutterDepth[3] = {0.211, 0.014, 0.073};
constexpr double kFlutterPhase[3] = {-2.121, -0.364, 2.96};
constexpr double kLengthS = 2.78, kAttackS = 0.20, kReleaseS = 0.24;
constexpr double kPeak = 0.7;  // of full scale

}  // namespace

std::shared_ptr<PcmBuffer> makeCockpitBuzzer(int rate) {
  rate = std::max(8000, rate);
  const size_t n = static_cast<size_t>(kLengthS * rate);
  const size_t attack = static_cast<size_t>(kAttackS * rate);
  const size_t release = static_cast<size_t>(kReleaseS * rate);
  double amp[kHarmonics];
  for (int k = 0; k < kHarmonics; ++k)
    amp[k] = std::pow(10.0, kLevelDb[k] / 20.0);
  const int top =
      std::min(kHarmonics, static_cast<int>((rate / 2.0 - 100.0) / kF0));

  std::vector<double> mix(n);
  double peak = 0;
  for (size_t i = 0; i < n; ++i) {
    const double t = static_cast<double>(i) / rate;
    double v = 0;
    for (int k = 0; k < top; ++k)
      v += amp[k] * std::cos(2 * kPi * (k + 1) * kF0 * t + kPhase[k]);
    double flutter = 1.0;
    for (int m = 0; m < 3; ++m)
      flutter +=
          kFlutterDepth[m] *
          std::cos(2 * kPi * (m + 1) * kFlutterHz * t + kFlutterPhase[m]);
    double env = 1.0;  // the fade in and out
    if (i < attack)
      env = 0.5 - 0.5 * std::cos(kPi * static_cast<double>(i) /
                                 static_cast<double>(attack));
    else if (i >= n - release)
      env = 0.5 - 0.5 * std::cos(kPi * static_cast<double>(n - 1 - i) /
                                 static_cast<double>(release));
    mix[i] = v * flutter * env;
    peak = std::max(peak, std::fabs(mix[i]));
  }
  auto pcm = std::make_shared<PcmBuffer>();
  pcm->rate = rate;
  pcm->samples.reserve(n);
  const double scale = peak > 0 ? kPeak / peak : 0.0;
  for (double v : mix)
    pcm->samples.push_back(
        static_cast<int16_t>(std::lrint(v * scale * 32767.0)));
  return pcm;
}

std::shared_ptr<PcmBuffer> makeSingleChime(int rate) {
  // A struck tone near 1.2 kHz with two inharmonic partials, decaying over
  // a second and a half.
  rate = std::max(8000, rate);
  const size_t n = static_cast<size_t>(1.5 * rate);
  const double partials[3][2] = {{1180.0, 1.0}, {2950.0, 0.35}, {4130.0, 0.15}};
  auto pcm = std::make_shared<PcmBuffer>();
  pcm->rate = rate;
  pcm->samples.resize(n);
  for (size_t k = 0; k < n; ++k) {
    const double t = static_cast<double>(k) / rate;
    const double attack = std::min(1.0, t / 0.004);
    double v = 0;
    for (const auto& p : partials)
      if (p[0] < rate / 2.0 - 100.0)
        v += p[1] * std::exp(-t * (2.5 + p[0] / 1500.0)) *
             std::sin(2 * kPi * p[0] * t);
    pcm->samples[k] = static_cast<int16_t>(
        std::clamp(v * attack * 0.5 * 32767.0, -32768.0, 32767.0));
  }
  return pcm;
}

}  // namespace xn
