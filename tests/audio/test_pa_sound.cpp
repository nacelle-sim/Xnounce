// The old-PA sound (pa_effects) and the PA's cut-out on a power transfer
// (pa_power), VP_SPEC § 4.6.
#include <doctest/doctest.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include "audio/decode_cache.h"
#include "audio/pa_effects.h"
#include "audio/pa_power.h"
#include "support/paths.h"

using namespace xn;

namespace {

constexpr int kRate = 44100;

// The developer's local Airline Packs; empty when there are none.
std::string packsDir() { return xn::testing::localDir("packs").string(); }
constexpr double kPi = 3.14159265358979323846;

std::vector<int16_t> tone(double hz, double seconds, double amp = 0.3) {
  std::vector<int16_t> v(static_cast<size_t>(seconds * kRate));
  for (size_t k = 0; k < v.size(); ++k)
    v[k] = static_cast<int16_t>(
        amp * 32767 * std::sin(2 * kPi * hz * static_cast<double>(k) / kRate));
  return v;
}

/// The level of `hz` in `v` (a single-bin DFT), skipping the first `skip`
/// seconds, where the feedback burst is.
double levelAt(const std::vector<int16_t>& v, double hz, double skip = 0.5) {
  double re = 0, im = 0;
  const size_t from = static_cast<size_t>(skip * kRate);
  for (size_t k = from; k < v.size(); ++k) {
    const double ph = 2 * kPi * hz * static_cast<double>(k) / kRate;
    re += v[k] * std::cos(ph);
    im += v[k] * std::sin(ph);
  }
  return std::sqrt(re * re + im * im) / static_cast<double>(v.size() - from);
}

double rmsOf(const std::vector<int16_t>& v, size_t from = 0) {
  double s = 0;
  for (size_t k = from; k < v.size(); ++k) s += double(v[k]) * v[k];
  return std::sqrt(s / static_cast<double>(v.size() - from));
}

}  // namespace

TEST_CASE("PA presets: Modern is untouched, Random follows the tail number") {
  const std::vector<int16_t> in = tone(1000, 0.2);
  CHECK(agePa(in, kRate, PaPreset::Modern, 7) == in);
  CHECK(resolvePaPreset(0, "D-AIZA") == PaPreset::Modern);
  CHECK(resolvePaPreset(1, "D-AIZA") == PaPreset::Worn);
  CHECK(resolvePaPreset(2, "D-AIZA") == PaPreset::Old);
  // Random is fixed per tail number, and the tails of a fleet spread over
  // all three presets.
  CHECK(resolvePaPreset(3, "D-AIZA") == resolvePaPreset(3, "D-AIZA"));
  int seen[3] = {};
  for (int k = 0; k < 60; ++k)
    ++seen[static_cast<int>(resolvePaPreset(3, "G-EU" + std::to_string(k)))];
  CHECK(seen[0] > 0);
  CHECK(seen[1] > 0);
  CHECK(seen[2] > 0);
  CHECK(paSeed("D-AIZA", "a.ogg") != paSeed("D-AIZA", "b.ogg"));
  CHECK(paSeed("D-AIZA", "a.ogg") != paSeed("D-AIZB", "a.ogg"));
}

TEST_CASE("PA sound: a small speaker's band, the same level, repeatable") {
  for (PaPreset p : {PaPreset::Worn, PaPreset::Old}) {
    CAPTURE(static_cast<int>(p));
    // Low bass and high treble are cut, the voice band passes: three equal
    // tones in, the voice tone well above the other two out.
    const auto bass = tone(80, 1.0, 0.1), voice = tone(1500, 1.0, 0.1),
               treble = tone(9000, 1.0, 0.1);
    std::vector<int16_t> mix(bass.size());
    for (size_t k = 0; k < mix.size(); ++k)
      mix[k] = static_cast<int16_t>(bass[k] + voice[k] + treble[k]);
    const auto out = agePa(mix, kRate, p, 1);
    CHECK(levelAt(out, 80) < 0.2 * levelAt(out, 1500));
    CHECK(levelAt(out, 9000) < 0.2 * levelAt(out, 1500));
    const auto mid = agePa(tone(1500, 1.0), kRate, p, 1);
    // The voice comes out about as loud as it went in.
    const auto in = tone(1500, 1.0);
    const double ratio = rmsOf(mid, kRate / 2) / rmsOf(in, kRate / 2);
    CHECK(ratio > 0.6);
    CHECK(ratio < 1.6);
    // The same seed, the same sound; another seed, another sound.
    CHECK(agePa(in, kRate, p, 1) == mid);
    CHECK(agePa(in, kRate, p, 2) != mid);
  }
}

TEST_CASE("PA sound: a noise floor with the hum, and no wrap-around") {
  const std::vector<int16_t> silence(kRate, 0);
  const auto worn = agePa(silence, kRate, PaPreset::Worn, 3);
  const auto old = agePa(silence, kRate, PaPreset::Old, 3);
  // Silence picks up hiss and hum; Old is noisier than Worn.
  CHECK(rmsOf(worn, kRate / 2) > 20);
  CHECK(rmsOf(old, kRate / 2) > rmsOf(worn, kRate / 2));
  CHECK(levelAt(old, 400) > 3 * levelAt(old, 600));  // the 400 Hz hum
  // Full-scale input is soft-clipped into range, never wrapped.
  std::vector<int16_t> loud(kRate);
  for (size_t k = 0; k < loud.size(); ++k)
    loud[k] = (k / 50) % 2 ? 32767 : -32768;
  const auto out = agePa(loud, kRate, PaPreset::Old, 4);
  int flips = 0;
  for (size_t k = 1; k < out.size(); ++k)
    if ((out[k - 1] > 30000 && out[k] < -30000) ||
        (out[k - 1] < -30000 && out[k] > 30000))
      ++flips;
  CHECK(flips == 0);
}

TEST_CASE("PA sound: Old keys with a burst of feedback") {
  const std::vector<int16_t> silence(kRate, 0);
  const auto old = agePa(silence, kRate, PaPreset::Old, 5);
  const auto worn = agePa(silence, kRate, PaPreset::Worn, 5);
  const size_t burst = kRate / 5;
  CHECK(rmsOf(std::vector<int16_t>(old.begin(), old.begin() + burst)) >
        4 * rmsOf(std::vector<int16_t>(worn.begin(), worn.begin() + burst)));
}

TEST_CASE("power cut-out: a transfer silences the PA for one to two seconds") {
  PaPowerCut cut(9);
  double t = 0;
  auto run = [&](double until, bool powered, int source) {
    float g = 1;
    for (; t < until; t += 0.02) g = cut.update(t, powered, source, true);
    return g;
  };
  CHECK(run(5, true, 1) == 1.0f);  // on the GPU
  run(5.2, true, 2);               // to the APU
  CHECK(cut.gain() == 0.0f);
  CHECK(cut.cutting());
  CHECK(run(6.0, true, 2) == 0.0f);  // still silent after 0.8 s
  CHECK(run(7.2, true, 2) == 1.0f);  // back within 2 s and the ramp
  // A source the aircraft doesn't report never cuts.
  CHECK(run(9, true, -1) == 1.0f);
  CHECK(run(10, true, 3) == 1.0f);
}

TEST_CASE("power cut-out: a short loss, a long loss, and the switch") {
  PaPowerCut cut(11);
  double t = 0;
  auto run = [&](double until, bool powered, bool on = true) {
    float g = 1;
    for (; t < until; t += 0.02) g = cut.update(t, powered, -1, on);
    return g;
  };
  run(2, true);
  CHECK(run(2.3, false) == 0.0f);  // a loss of 0.3 s
  CHECK(run(2.6, true) == 0.0f);   // stays silent at least a second
  CHECK(run(4.4, true) == 1.0f);
  CHECK(run(10, false) == 0.0f);  // no power, no PA
  CHECK(run(10.5, true) > 0.9f);  // a long failure: back with the power
  // Off: the PA never cuts, as before.
  CHECK(run(11, false, false) == 1.0f);
  CHECK(run(12, true, false) == 1.0f);
}

TEST_CASE("decode cache: aged PA copies and announcements first") {
  if (packsDir().empty()) return;
  DecodeCache cache(64u << 20);
  const std::string a = packsDir() + "/EJU/CallCabinSecureTakeoff.ogg";
  auto wait = [&](bool pa) {
    std::shared_ptr<PcmBuffer> p;
    for (int k = 0; k < 300 && !p; ++k) {
      p = cache.get(a, pa);
      if (!p) std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return p;
  };
  // Modern: the PA and the interphone share the clean copy.
  auto clean = wait(false);
  REQUIRE(clean);
  CHECK(wait(true) == clean);
  // Old: the PA gets its own aged copy, the interphone keeps the clean one.
  cache.setPaAging(PaPreset::Old, "D-AIZA");
  auto aged = wait(true);
  REQUIRE(aged);
  CHECK(aged->samples.size() == clean->samples.size());
  CHECK(aged->samples != clean->samples);
  auto again = wait(false);
  REQUIRE(again);
  CHECK(again->samples == clean->samples);
  // The muffled twin is made from the aged copy.
  cache.setMuffleCutoff(800);
  auto both = wait(true);
  REQUIRE(both);
  CHECK(both->muffled.size() == both->samples.size());
  CHECK(both->samples == aged->samples);
  cache.shutdown();
}
