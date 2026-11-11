// Recognising the Zibo and LevelUp 737 from what X-Plane reports about the
// loaded aircraft. The identity strings are those of ZiboMod 4.05.35 and the
// LevelUp 737NG series 2.S1.51C (docs/aircraft/zibo737.md).
#include <doctest/doctest.h>

#include "aircraft/zibo737/zibo737_identity.h"
#include "aircraft/zibo737/zibo737_signals.h"
#include "aircraft/zibo737/zibo737_types.h"

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
  CHECK(z.supported);
  CHECK(z.model == "Zibo 737-800");
  CHECK(z.dormantReason.empty());
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
    CHECK_MESSAGE(r.supported, m.file);
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

// ---- the 737's signals and types
// ----------------------------------------------

TEST_CASE("737 types: models, tags, cabin sizes and wording") {
  CHECK(modelFrom("B736", "737_60NG.acf") == Model::B736);
  CHECK(modelFrom("B738", "b738.acf") == Model::B738);
  CHECK(modelFrom("B739", "737_90NG.acf") == Model::B739);
  CHECK(modelFrom("B739", "737_9ENG.acf") == Model::B739ER);
  CHECK(modelFrom("A320", "a320.acf") == Model::Unknown);
  const Options o;
  const AircraftType b738 = makeType(Model::B738, o, 189);
  CHECK(b738.label == "737-800");
  CHECK(b738.tags == std::vector<std::string>{"B738"});
  CHECK(b738.specific == "B738");
  CHECK(b738.designators == std::vector<std::string>{"B738"});
  CHECK(b738.typeFactor == 1.0f);
  CHECK(b738.lead.seats == 189);  // the aircraft's cabin layout
  CHECK(makeType(Model::B738, o).lead.seats == o.b738.seats);
  const AircraftType er = makeType(Model::B739ER, o);
  CHECK(er.label == "737-900ER");
  CHECK(er.tags == std::vector<std::string>{"B739"});
  CHECK(er.typeFactor > makeType(Model::B739, o).typeFactor);
  CHECK(makeType(Model::B736, o).typeFactor < b738.typeFactor);
  CHECK(b738.terms.fms == "FMC");
  CHECK(b738.terms.cabinCallButtons == "the overhead ATTEND button");
}

TEST_CASE("737 signals: the FMC phase 0 before, during and after a flight") {
  Signals sig;
  SimState s;
  CHECK(sig.phase(0, s) == Phase::PreFlight);
  s.enginesRunning = 2;
  s.gsKt = 60;
  CHECK(sig.phase(0, s) == Phase::Takeoff);  // rolling with power
  s.onGround = false;
  s.aglFt = 400;
  CHECK(!sig.phase(0, s));  // airborne: the purser estimates
  CHECK(sig.phase(1, s) == Phase::Climb);
  CHECK(sig.phase(2, s) == Phase::Cruise);
  CHECK(sig.phase(3, s) == Phase::Cruise);
  CHECK(sig.phase(4, s) == Phase::Cruise);
  CHECK(sig.phase(5, s) == Phase::Descent);
  CHECK(sig.phase(6, s) == Phase::Approach);
  CHECK(sig.phase(7, s) == Phase::Approach);
  CHECK(sig.phase(8, s) == Phase::GA);
  CHECK(!sig.phase(9, s));

  // After a flight phase 0 is Done, until the engines stop at the stand.
  Raw r;
  r.flightPhase = 2;
  s.aglFt = 30000;
  sig.apply(r, s);
  SimState g;
  g.enginesRunning = 2;
  g.gsKt = 20;
  r.flightPhase = 0;
  sig.apply(r, g);
  CHECK(g.fmsPhase == Phase::Done);
  g = SimState();
  g.enginesRunning = 0;
  sig.apply(r, g);
  CHECK(g.fmsPhase == Phase::PreFlight);
}

TEST_CASE("737 signals: signs, lights, doors, passengers and power") {
  Signals sig;
  Raw r;
  r.flightPhase = 0;
  r.seatbeltSwitch = 1;  // AUTO, no light published
  r.noSmokingSwitch = 1;
  r.gearHandle = 1;
  r.retractableLights = {1, 1};  // extended, not on
  r.fixedLights = {0, 0};
  r.positionLight = -1;  // STEADY
  r.doors = {0.4f, 0, 1, 0};
  r.pax = 152;
  r.bus1 = r.bus2 = 1;
  r.genOff1 = r.genOff2 = 1;
  r.apuBus1 = r.apuBus2 = 1;
  SimState s;
  sig.apply(r, s);
  CHECK(s.seatbelt);        // AUTO with the gear down
  CHECK(s.noSmoking == 1);  // the same for NO SMOKING
  CHECK(!s.landingLights);
  CHECK(s.strobeOn == 0);
  CHECK(s.gearLeverDown == 1);
  CHECK(s.doorFwd == doctest::Approx(0.4f));
  CHECK(s.doorMax == 1.0f);
  CHECK(s.doorCount == 4);
  CHECK(!s.slidesKnown);
  CHECK(s.pax == 152);
  CHECK(s.acPower);
  CHECK(s.acSource == 22);  // the APU on both buses

  r.gearHandle = 0;
  r.seatbeltLight = 0;  // the aircraft's light wins over the switch
  r.retractableLights = {2, 1};
  r.positionLight = 1;
  r.genOff1 = 0;  // engine generator 1 takes bus 1
  s = SimState();
  sig.apply(r, s);
  CHECK(!s.seatbelt);
  CHECK(s.noSmoking == 0);
  CHECK(s.landingLights);
  CHECK(s.strobeOn == 1);
  CHECK(s.acSource == 32);

  r.bus1 = r.bus2 = 0;
  s = SimState();
  sig.apply(r, s);
  CHECK(!s.acPower);
  CHECK(s.acSource == 0);
  CHECK(busSource(1, 1, 0) == 1);  // powered by neither: ground power
  CHECK(busSource(-1, 0, 0) == -1);
}

TEST_CASE("737 signals: takeoff power, lift-off and touchdown cues") {
  Signals sig(0.85f);
  Raw r;
  r.flightPhase = 0;
  r.airGround = 1;
  r.apTakeoff = 1;
  r.throttleMax = 0.9f;
  r.landed = 0;
  SimState s;
  sig.apply(r, s);
  CHECK(s.cues.takeoffPower == 1);
  CHECK(s.cues.leversAtTakeoff == 1);
  CHECK(s.cues.liftoff == 0);
  CHECK(s.cues.touchdown == 0);
  CHECK(s.cues.phaseKnown);
  r.airGround = 0;
  s = SimState();
  s.onGround = false;
  s.aglFt = 300;
  sig.apply(r, s);
  CHECK(s.cues.takeoffPower == 0);  // TO/GA in the air is no takeoff cue
  CHECK(s.cues.liftoff == 1);
  CHECK(s.cues.liftoffLabel == "air/ground sensor");
  CHECK(!s.cues.phaseKnown);  // phase 0 in the air: no opinion
  r.flightPhase = 6;
  r.airGround = 1;
  r.landed = 1;
  r.throttleMax = 0;
  s = SimState();
  sig.apply(r, s);
  CHECK(s.cues.touchdown == 1);
  CHECK(s.cues.leversAtTakeoff == 0);
}
