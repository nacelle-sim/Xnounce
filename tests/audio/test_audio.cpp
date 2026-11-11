// Audio without X-Plane: the decoder (also on the developer's local packs),
// the decode cache, the muffle filter, the call buzzer and the hearing model
// (VP_SPEC § 4.5).
#include <doctest/doctest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>

#include "audio/chime.h"
#include "audio/decode_cache.h"
#include "audio/decoder.h"
#include "audio/hearing.h"
#include "audio/muffle.h"
#include "support/paths.h"

using namespace xn;

namespace {
// the developers local airline packs empty when there are none
std::string packsDir() { return xn::testing::localDir("packs").string(); }
bool havePacks() { return !packsDir().empty(); }
}  // namespace

TEST_CASE("decoder rejects what is not Ogg Vorbis") {
  const std::string junk =
      (xn::testing::tmpDir() / "xn_junk_test.ogg").string();
  {
    std::ofstream f(junk, std::ios::binary);
    f << "this is not an ogg file at all";
  }
  const DecodeResult r = decodeOgg(junk);
  CHECK(!r.pcm);
  CHECK(!r.error.empty());
  CHECK(!decodeOgg("/nonexistent/x.ogg").pcm);
  std::filesystem::remove(junk);
}

TEST_CASE("decoder on the sample packs (local only)") {
  if (!havePacks()) return;
  const DecodeResult r =
      decodeOgg(packsDir() + "/EJU/BoardingWelcome.ogg");  // 48k stereo 34s
  REQUIRE(r.pcm);
  CHECK(r.pcm->rate == 48000);
  CHECK(r.pcm->seconds() == doctest::Approx(33.97).epsilon(0.01));
  int16_t peak = 0;
  for (int16_t v : r.pcm->samples)
    peak = std::max<int16_t>(peak, static_cast<int16_t>(std::abs(v)));
  CHECK(peak > 1000);  // real audio downmixed
  // stray video in the ual sample gets rejected
  CHECK(!decodeOgg(packsDir() + "/UAL/CrewSeatsLanding[1].mp4").pcm);
  // short limit bails before decoding the whole thing
  DecodeLimits lim;
  lim.maxSeconds = 10;
  CHECK(!decodeOgg(packsDir() + "/EJU/BoardingWelcome.ogg", lim).pcm);
}

TEST_CASE("decode cache: background decode, failures remembered") {
  if (!havePacks()) return;
  DecodeCache cache(64u << 20);
  const std::string ok = packsDir() + "/EJU/ArmDoors[Morning].ogg";
  const std::string bad = packsDir() + "/UAL/CrewSeatsLanding[1].mp4";
  CHECK(!cache.get(ok));
  CHECK(!cache.get(bad));
  std::shared_ptr<PcmBuffer> pcm;
  for (int k = 0; k < 200 && !pcm; ++k) {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    pcm = cache.get(ok);
  }
  REQUIRE(pcm);
  for (int k = 0; k < 100 && !cache.failed(bad); ++k)
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  std::string err;
  CHECK(cache.failed(bad, &err));
  CHECK(!err.empty());
  CHECK(cache.bytes() == pcm->bytes());
  CHECK(pcm->muffled.empty());  // no cutoff no muffled copy
  // w a cutoff every decode carries the muffled copy too cache counts both
  cache.setMuffleCutoff(800);
  CHECK(cache.bytes() == 0);  // cache emptied our buffer still valid
  CHECK(!pcm->samples.empty());
  std::shared_ptr<PcmBuffer> again;
  for (int k = 0; k < 200 && !again; ++k) {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    again = cache.get(ok);
  }
  REQUIRE(again);
  CHECK(again->muffled.size() == again->samples.size());
  CHECK(again->bytes() == 2 * pcm->bytes());
  CHECK(cache.bytes() == again->bytes());
  cache.shutdown();
}

TEST_CASE("muffle: the closed-door low-pass") {
  constexpr int rate = 44100;
  auto tone = [&](double hz, double amp) {
    std::vector<int16_t> v(rate);  // 1s
    for (size_t k = 0; k < v.size(); ++k)
      v[k] =
          static_cast<int16_t>(amp * std::sin(2 * 3.14159265358979 * hz *
                                              static_cast<double>(k) / rate));
    return v;
  };
  // rms of the 2nd half once the filter settles
  auto rms = [](const std::vector<int16_t>& v) {
    double sum = 0;
    for (size_t k = v.size() / 2; k < v.size(); ++k)
      sum += static_cast<double>(v[k]) * v[k];
    return std::sqrt(sum / static_cast<double>(v.size() - v.size() / 2));
  };
  const auto low = tone(200, 10000), high = tone(5000, 10000);
  const double lowDb = 20 * std::log10(rms(lowPass(low, rate, 800)) / rms(low));
  const double highDb =
      20 * std::log10(rms(lowPass(high, rate, 800)) / rms(high));
  CHECK(lowDb > -1.0);    // speech fundamentals pass
  CHECK(highDb < -20.0);  // sibilance gets cut
  // full scale square overshoots must clip not wrap
  std::vector<int16_t> square(rate);
  for (size_t k = 0; k < square.size(); ++k)
    square[k] = (k / 200) % 2 ? 32767 : -32768;
  const auto sq = lowPass(square, rate, 800);
  REQUIRE(sq.size() == square.size());
  int biggestStep = 0;  // a wrap would be a 65000 step
  for (size_t k = 1; k < sq.size(); ++k)
    biggestStep =
        std::max(biggestStep, std::abs(static_cast<int>(sq[k]) - sq[k - 1]));
  CHECK(biggestStep < 20000);
  // degenerate input
  CHECK(lowPass({}, rate, 800).empty());
  CHECK(lowPass(low, 0, 800).size() == low.size());
}

namespace {

// level in db vs the loudest of freqs over from to direct dft w hann window
std::vector<double> levelsDb(const PcmBuffer& p, size_t from, size_t to,
                             const std::vector<double>& freqs) {
  const double pi = 3.14159265358979323846;
  std::vector<double> mag;
  const size_t n = to - from;
  for (double f : freqs) {
    double re = 0, im = 0;
    for (size_t i = 0; i < n; ++i) {
      const double w = 0.5 - 0.5 * std::cos(2 * pi * static_cast<double>(i) /
                                            static_cast<double>(n - 1));
      const double x = p.samples[from + i] * w;
      re += x * std::cos(2 * pi * f * static_cast<double>(i) / p.rate);
      im -= x * std::sin(2 * pi * f * static_cast<double>(i) / p.rate);
    }
    mag.push_back(std::hypot(re, im));
  }
  const double top = *std::max_element(mag.begin(), mag.end());
  std::vector<double> db;
  for (double m : mag) db.push_back(20 * std::log10(m / top + 1e-12));
  return db;
}

void writeWav(const std::string& path, const PcmBuffer& p) {
  std::ofstream o(path, std::ios::binary);
  auto u32 = [&](uint32_t v) { o.write(reinterpret_cast<const char*>(&v), 4); };
  auto u16 = [&](uint16_t v) { o.write(reinterpret_cast<const char*>(&v), 2); };
  const uint32_t bytes = static_cast<uint32_t>(p.samples.size() * 2);
  o.write("RIFF", 4);
  u32(36 + bytes);
  o.write("WAVEfmt ", 8);
  u32(16);
  u16(1);
  u16(1);
  u32(static_cast<uint32_t>(p.rate));
  u32(static_cast<uint32_t>(p.rate * 2));
  u16(2);
  u16(16);
  o.write("data", 4);
  u32(bytes);
  o.write(reinterpret_cast<const char*>(p.samples.data()), bytes);
}

}  // namespace

TEST_CASE("cockpit call buzzer: the A320 buzz (VP_SPEC 4.3, D-7)") {
  const auto c = makeCockpitBuzzer(22050);
  REQUIRE(c);
  CHECK(c->seconds() == doctest::Approx(2.78).epsilon(0.01));
  int16_t peak = 0;
  for (int16_t v : c->samples)
    peak = std::max<int16_t>(peak, static_cast<int16_t>(std::abs(v)));
  CHECK(peak > 20000);
  CHECK(peak < 32767);
  // fades no click
  CHECK(std::abs(c->samples.front()) < 200);
  CHECK(std::abs(c->samples.back()) < 200);
  // steady 2s 300hz fundamental odd harmonics like the reference 25hz flutter
  // sidebands
  const size_t from = static_cast<size_t>(0.4 * c->rate),
               to = static_cast<size_t>(2.4 * c->rate);
  const auto db = levelsDb(*c, from, to, {300, 600, 900, 1500, 2100, 275, 325});
  CHECK(db[0] == doctest::Approx(0.0).epsilon(0.001));
  CHECK(db[1] < -28);                                    // evens weak
  CHECK(db[2] == doctest::Approx(-12.3).epsilon(0.25));  // 3db
  CHECK(db[3] == doctest::Approx(-22.1).epsilon(0.15));
  CHECK(db[4] == doctest::Approx(-24.6).epsilon(0.15));
  CHECK(db[5] > -26);  // ref -17/ 20 db
  CHECK(db[5] < -14);
  CHECK(db[6] > -26);
  // xn_dump_dir dir dumps the render to listen to
  if (const char* dir = std::getenv("XN_DUMP_DIR")) {
    const std::string path = std::string(dir) + "/cockpit_buzzer.wav";
    writeWav(path, *makeCockpitBuzzer(44100));
    const std::string wrote = "wrote " + path;
    MESSAGE(wrote);
  }
}

TEST_CASE("hearing model: clear and muffled through the cockpit door") {
  Settings set;
  SimState s;
  set.hearingFloor = 0.1f;
  set.doorLeak = 0.6f;
  REQUIRE(set.muffle);
  Hearing h = cabinHearing(set, s);  // door closed pa in muffled only
  CHECK(h.clear == doctest::Approx(0.0f));
  CHECK(h.muffled == doctest::Approx(0.1f));
  s.cockpitDoor = 0.5f;  // half open crossfade
  h = cabinHearing(set, s);
  CHECK(h.clear == doctest::Approx(0.3f));
  CHECK(h.muffled == doctest::Approx(0.05f));
  s.cockpitDoor = 1;  // open all clear
  h = cabinHearing(set, s);
  CHECK(h.clear == doctest::Approx(0.6f));
  CHECK(h.muffled == doctest::Approx(0.0f));
  s.cockpitDoor = 0;
  s.paListen = true;  // pa pulled clear door bleed
  s.paListenVol = 0.9f;
  h = cabinHearing(set, s);
  CHECK(h.clear == doctest::Approx(0.9f));
  CHECK(h.muffled == doctest::Approx(0.1f));
  s.cameraInCabin = true;  // in the cabin
  h = cabinHearing(set, s);
  CHECK(h.clear == doctest::Approx(1.0f));
  CHECK(h.muffled == doctest::Approx(0.0f));
  s.viewExternal = true;
  set.exteriorGain = 0.1f;
  h = cabinHearing(set, s);
  CHECK(h.clear == doctest::Approx(0.1f));
  CHECK(h.muffled == doctest::Approx(0.0f));
  set.realisticHearing = false;
  CHECK(cabinHearing(set, s).clear == doctest::Approx(1.0f));
  CHECK(soundGain(SoundKind::Pa, set, s, true) == 0.0f);  // muted
  CHECK(muffledGain(SoundKind::Pa, set, s, true) == 0.0f);

  // muffling off one plain sound never under the floor 0.2.0 behaviour
  Settings plain;
  plain.muffle = false;
  plain.hearingFloor = 0.2f;
  SimState c;
  h = cabinHearing(plain, c);
  CHECK(h.clear == doctest::Approx(0.2f));
  CHECK(h.muffled == doctest::Approx(0.0f));
  CHECK(muffledGain(SoundKind::Pa, plain, c, false) == 0.0f);

  // twin gets pa/music volume other kinds have no twin
  Settings g;
  g.masterVolume = 1;
  g.paVolume = 0.5f;
  g.musicVolume = 0.4f;
  g.hearingFloor = 0.1f;
  CHECK(muffledGain(SoundKind::Pa, g, c, false) == doctest::Approx(0.05f));
  CHECK(muffledGain(SoundKind::Music, g, c, false) == doctest::Approx(0.04f));
  CHECK(muffledGain(SoundKind::Interphone, g, c, false) == 0.0f);
  CHECK(muffledGain(SoundKind::Chime, g, c, false) == 0.0f);
  CHECK(soundGain(SoundKind::Pa, g, c, false) == doctest::Approx(0.0f));

  // interphone follows cab floor 0.15
  s.interphoneOn = true;
  s.interphoneVol = 0.5f;
  set.masterVolume = 1;
  set.interphoneVolume = 1;
  CHECK(soundGain(SoundKind::Interphone, set, s, false) ==
        doctest::Approx(0.5f));
  s.interphoneVol = 0;
  CHECK(soundGain(SoundKind::Interphone, set, s, false) ==
        doctest::Approx(0.15f));
}

TEST_CASE("hearing follows the aircraft's door blend and interior volume") {
  Settings set;
  set.hearingFloor = 0.1f;
  set.doorLeak = 0.8f;
  SimState s;
  s.cockpitDoor = 0;   // angle says closed
  s.doorBlend = 0.5f;  // but the blend says half open
  CHECK(doorOpenFraction(set, s) == doctest::Approx(0.5f));
  CHECK(cabinHearing(set, s).clear == doctest::Approx(0.4f));
  CHECK(cabinHearing(set, s).muffled == doctest::Approx(0.05f));
  set.useAircraftDoorBlend = false;
  CHECK(doorOpenFraction(set, s) == doctest::Approx(0.0f));
  set.useAircraftDoorBlend = true;
  s.doorBlend = -1;  // no blend use the angle
  s.cockpitDoor = 1;
  CHECK(doorOpenFraction(set, s) == doctest::Approx(1.0f));

  set.masterVolume = 1;
  set.chimeVolume = 1;
  s.aircraftVolume = -1;  // the aircraft doesn't ask to be followed
  CHECK(soundGain(SoundKind::Chime, set, s, false) == doctest::Approx(1.0f));
  s.aircraftVolume = 0.5f;
  CHECK(soundGain(SoundKind::Chime, set, s, false) == doctest::Approx(0.5f));
  CHECK(soundGain(SoundKind::Pa, set, s, false) ==
        doctest::Approx(0.5f * cabinHearing(set, s).clear));
  set.followAircraftVolume = false;
  CHECK(soundGain(SoundKind::Chime, set, s, false) == doctest::Approx(1.0f));
}

namespace {

// A WAV file in memory: the RIFF header, a fmt chunk and a data chunk.
std::vector<uint8_t> wav(int format, int channels, int rate, int bits,
                         const std::vector<uint8_t>& data,
                         uint32_t dataSizeField = 0xFFFFFFFF) {
  std::vector<uint8_t> b;
  auto u32 = [&](uint32_t v) {
    for (int k = 0; k < 4; ++k) b.push_back(static_cast<uint8_t>(v >> (8 * k)));
  };
  auto u16 = [&](uint16_t v) {
    b.push_back(static_cast<uint8_t>(v));
    b.push_back(static_cast<uint8_t>(v >> 8));
  };
  b.insert(b.end(), {'R', 'I', 'F', 'F'});
  u32(static_cast<uint32_t>(36 + data.size()));
  b.insert(b.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
  u32(16);
  u16(static_cast<uint16_t>(format));
  u16(static_cast<uint16_t>(channels));
  u32(static_cast<uint32_t>(rate));
  u32(static_cast<uint32_t>(rate * channels * bits / 8));
  u16(static_cast<uint16_t>(channels * bits / 8));
  u16(static_cast<uint16_t>(bits));
  b.insert(b.end(), {'d', 'a', 't', 'a'});
  u32(dataSizeField == 0xFFFFFFFF ? static_cast<uint32_t>(data.size())
                                  : dataSizeField);
  b.insert(b.end(), data.begin(), data.end());
  return b;
}

}  // namespace

TEST_CASE("WAV reader: PCM files become mono 16-bit") {
  // 16-bit stereo: each frame is the mean of left and right.
  const std::vector<uint8_t> stereo = {0x00, 0x10, 0x00, 0x30,   // 4096, 12288
                                       0x00, 0xF0, 0x00, 0xF0};  // -4096 x2
  DecodeResult r = decodeWavBytes(wav(1, 2, 22050, 16, stereo));
  REQUIRE(r.pcm);
  CHECK(r.pcm->rate == 22050);
  CHECK(r.pcm->samples == std::vector<int16_t>{8192, -4096});
  // 8-bit is unsigned around 128.
  r = decodeWavBytes(wav(1, 1, 8000, 8, {128, 255, 0}));
  REQUIRE(r.pcm);
  CHECK(r.pcm->samples == std::vector<int16_t>{0, 32512, -32768});
  // A data chunk cut short at the end of the file plays what is there.
  r = decodeWavBytes(wav(1, 1, 8000, 16, {0x01, 0x00, 0x02, 0x00}, 4000));
  REQUIRE(r.pcm);
  CHECK(r.pcm->samples.size() == 2);
}

TEST_CASE("WAV reader: damaged or unsupported files give an error") {
  const std::vector<uint8_t> pcm = {0, 0, 0, 0};
  CHECK(!decodeWavBytes({}).pcm);
  CHECK(!decodeWavBytes({'R', 'I', 'F', 'F'}).pcm);
  std::vector<uint8_t> notWave = wav(1, 1, 8000, 16, pcm);
  notWave[8] = 'X';
  CHECK(!decodeWavBytes(notWave).pcm);
  CHECK(!decodeWavBytes(wav(3, 1, 8000, 32, pcm)).pcm);  // float
  CHECK(!decodeWavBytes(wav(1, 0, 8000, 16, pcm)).pcm);  // no channels
  CHECK(!decodeWavBytes(wav(1, 9, 8000, 16, pcm)).pcm);  // too many
  CHECK(!decodeWavBytes(wav(1, 1, 1000, 16, pcm)).pcm);  // rate
  CHECK(!decodeWavBytes(wav(1, 1, 8000, 24, pcm)).pcm);  // 24-bit
  CHECK(!decodeWavBytes(wav(1, 1, 8000, 16, {})).pcm);   // no samples
  // A fmt chunk claiming more bytes than the file has: no data chunk found.
  std::vector<uint8_t> lying = wav(1, 1, 8000, 16, pcm);
  lying[16] = 0xFF, lying[17] = 0xFF, lying[18] = 0xFF, lying[19] = 0x7F;
  DecodeResult r = decodeWavBytes(lying);
  CHECK(!r.pcm);
  CHECK(!r.error.empty());
  // Every truncation of a good file is handled.
  const std::vector<uint8_t> good = wav(1, 2, 22050, 16, {1, 2, 3, 4, 5, 6});
  for (size_t n = 0; n < good.size(); ++n)
    (void)decodeWavBytes(std::vector<uint8_t>(good.begin(), good.begin() + n));
  CHECK(!decodeWav("/no/such/file.wav").pcm);
}

TEST_CASE("the generated single chime for a 737 without its chime sound") {
  const auto c = makeSingleChime(22050);
  REQUIRE(c);
  CHECK(c->rate == 22050);
  CHECK(c->seconds() == doctest::Approx(1.5));
  int16_t peak = 0;
  for (int16_t v : c->samples) peak = std::max<int16_t>(peak, std::abs(v));
  CHECK(peak > 8000);
  CHECK(std::abs(c->samples.back()) < 800);  // it has died away
}
