// Recognising the Zibo and LevelUp 737 from what X-Plane reports about the
// loaded aircraft. The identity strings are those of ZiboMod 4.05.35 and the
// LevelUp 737NG series 2.S1.51C (docs/aircraft/zibo737.md).
#include <doctest/doctest.h>

#include "aircraft/zibo737/zibo737_identity.h"

using namespace xn;
using namespace xn::zibo737;

namespace {

LoadedAircraft acf(const char* path, const char* file, const char* icao,
                   const char* author, const char* studio) {
  LoadedAircraft a;
  a.acfPath = std::string(path) + file;
  a.acfFile = file;
  a.acfIcao = icao;
  a.author = author;
  a.studio = studio;
  return a;
}

const char* kZiboStudio = "Laminar Research modified by Zibo and Twkster";
const char* kLevelUpStudio = "LevelUp, Laminar Research, ZiboMod";

}  // namespace

TEST_CASE("737: the Zibo 737-800 and every LevelUp model are recognised") {
  const LoadedAircraft zibo = acf("/X-Plane 12/Aircraft/B737-800X/", "b738.acf",
                                  "B738", "Alex Unruh", kZiboStudio);
  const Recognition z = classify(zibo, true);
  CHECK(z.ours);
  CHECK(!z.supported);  // 0.3.5 recognises the 737 and stays dormant
  CHECK(z.model == "Zibo 737-800");
  CHECK(z.dormantReason == "737 support is in development");
  CHECK(z.logLine ==
        "dormant: Zibo 737-800 (b738.acf) recognised; 737 support is in "
        "development");
  CHECK(z.cardNote == ": 737 support is in development");
  CHECK(classify(acf("/X-Plane 12/Aircraft/B737-800X/", "b738_4k.acf", "B738",
                     "Alex Unruh", kZiboStudio),
                 true)
            .model == "Zibo 737-800");

  const char* lu = "/X-Plane 12/Aircraft/737NG Series/";
  struct {
    const char* file;
    const char* icao;
    const char* model;
  } const models[] = {{"737_60NG.acf", "B736", "LevelUp 737-600"},
                      {"737_70NG.acf", "B737", "LevelUp 737-700"},
                      {"737_80NG.acf", "B738", "LevelUp 737-800"},
                      {"737_90NG.acf", "B739", "LevelUp 737-900"},
                      {"737_9ENG.acf", "B739", "LevelUp 737-900ER"}};
  for (const auto& m : models) {
    const Recognition r =
        classify(acf(lu, m.file, m.icao, "LevelUp", kLevelUpStudio), true);
    CHECK_MESSAGE(r.ours, m.file);
    CHECK_MESSAGE(!r.supported, m.file);
    CHECK_MESSAGE(r.model == m.model, m.file);
  }
}

TEST_CASE("737: Laminar's own 737-800 and other aircraft are not ours") {
  // Laminar's default 737 has the same ICAO and author, but neither the Zibo
  // studio nor the Zibo flight-phase dataref.
  const LoadedAircraft laminar =
      acf("/X-Plane 12/Aircraft/Laminar Research/Boeing 737-800/", "b738.acf",
          "B738", "Alex Unruh", "Laminar Research");
  CHECK(makerOf(laminar) == Maker::Unknown);
  CHECK(!classify(laminar, false).ours);
  CHECK(!classify(laminar, true).ours);
  // a Zibo whose systems plugin hasn't registered its datarefs yet
  CHECK(!classify(acf("/X-Plane 12/Aircraft/B737-800X/", "b738.acf", "B738",
                      "Alex Unruh", kZiboStudio),
                  false)
             .ours);
  // a LevelUp file that isn't a 737NG
  CHECK(!classify(acf("/x/", "other.acf", "B752", "LevelUp", kLevelUpStudio),
                  true)
             .ours);
  CHECK(modelOf(acf("/x/", "a.acf", "A320", "", "")).empty());
}
