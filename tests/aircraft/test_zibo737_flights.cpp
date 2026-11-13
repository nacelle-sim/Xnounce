// Whole 737 flights through the purser. Each tick sets the 737's raw values
// and decodes them with the same code as the live module
// (aircraft/zibo737/zibo737_signals.h), so these stories show how the purser
// runs on a Zibo or LevelUp 737. Every flight's story is compared with its
// golden story (tests/golden/737-*.story.txt, docs/DEVELOPING.md § 3).
#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <functional>
#include <string>

#include "aircraft/zibo737/zibo737_crew.h"
#include "aircraft/zibo737/zibo737_signals.h"
#include "aircraft/zibo737/zibo737_types.h"
#include "core/cabin_crew/cabin_crew.h"
#include "core/purser/purser.h"
#include "replay.h"
#include "support/golden.h"
#include "support/paths.h"

using namespace xn;

namespace {

using zibo737::Raw;

/// A 737 that carries out the cabin crew's commands as the real one does
/// (docs/aircraft/zibo737.md § 6): the main doors take 7 s, the airstairs
/// 32 s, L1 won't close over the airstairs, and no main door opens with an
/// engine running or in the air.
struct Sim737 {
  zibo737::CrewReadings r;
  std::array<float, zibo737::kDoorCount> target{};
  float stairsTarget = 0;
  std::vector<std::string> ran;  ///< Every command, for the checks.

  Sim737() {
    r.door.fill(0);
    r.door[static_cast<size_t>(zibo737::Door::LMid)] = -1;  // a -800
    r.door[static_cast<size_t>(zibo737::Door::RMid)] = -1;
    target = r.door;
    r.airstairsHidden = 1;
    r.airstairRatio = 0;
    r.paxLighting = zibo737::kWhiteBright;
    r.autoFlight = 0;
    r.cargoLoading = 0;
  }
  void command(const std::string& c, const SimState& s) {
    ran.push_back(c);
    using namespace zibo737;
    for (int k = 0; k < kDoorCount; ++k) {
      const DoorInfo& d = doorInfo(static_cast<Door>(k));
      const size_t i = static_cast<size_t>(k);
      if (c == d.open && s.enginesRunning == 0 && s.onGround) target[i] = 1;
      if (c == d.close && (k != 0 || r.airstairsHidden == 1)) target[i] = 0;
    }
    if (c == kAirstairsToggle)
      stairsTarget = r.airstairsHidden == 1 ? 1.0f : 0.0f;
    const int l = r.paxLighting;
    if (c == paxLightCommand(kWhiteBright)) r.paxLighting = kWhiteBright;
    if (c == paxLightCommand(kTakeoffLanding)) r.paxLighting = kTakeoffLanding;
    if (c == paxLightCommand(kCruise)) r.paxLighting = kCruise;
    if (c == paxLightCommand(kWhiteMedium)) r.paxLighting = kWhiteMedium;
    if (c == paxLightCommand(kNight))
      r.paxLighting = l == kNight ? kSleep : kNight;
  }
  void move(double dt) {
    auto toward = [dt](float& v, float to, double seconds) {
      const float step = static_cast<float>(dt / seconds);
      v = v < to ? std::min(to, v + step) : std::max(to, v - step);
    };
    for (size_t i = 0; i < r.door.size(); ++i)
      if (r.door[i] >= 0) toward(r.door[i], target[i], 7.0);
    toward(r.airstairRatio, stairsTarget, 32.0);
    r.airstairsHidden = r.airstairRatio <= 0 ? 1 : 0;
  }
};

struct Flight737 {
  replay::StoryPlayer out;
  Settings set;
  /// X-Plane's own values (what aircraft/xp_common reads).
  SimState x;
  /// The 737's values.
  Raw r;
  zibo737::Signals signals;
  Purser purser{out, 42};
  double t = 0;
  bool settled = false;
  replay::Story story;
  replay::LogCapture logs;
  std::string storyName = xn::testing::nextGoldenName();
  std::string lastStatus;
  // The cabin crew (VP_SPEC § 4.7). Off unless a test switches it on: the
  // crew's plan and the 737's decisions then run every tick against `sim`,
  // and the passenger doors the purser sees are the simulated ones.
  bool crewOn = false;
  CabinCrew crew{7};
  zibo737::Crew crew737;
  zibo737::Crew::Switches sw;
  Sim737 sim;

  ~Flight737() {
    story.raw("--- log ---");
    for (const std::string& l : logs.finish()) story.raw(l);
    xn::testing::checkGolden(story.text(), xn::testing::goldenDir(), storyName);
  }

  explicit Flight737(zibo737::Model model = zibo737::Model::B738) {
    story.raw("# " + xn::testing::currentTestName());
    out.story = &story;
    purser.setNoteSink([this](double, const std::string& text) {
      story.line(t, "note", text);
    });
    purser.setEventSink([this](const TimelineRow& row) {
      story.line(t, "event", replay::eventLine(row));
    });
    out.clock = &t;
    out.durations[AnnType::SafetyBriefing] = 150;
    out.durations[AnnType::BoardingMusic] = 0;
    set.chainGap = 2;
    set.boardingStart = static_cast<int>(BoardingStart::Doors);
    x.onGround = true;
    x.sunPitchDeg = 30;
    x.acfIcao = "B738";
    // A LevelUp 737-800 at the gate on APU power, cold and dark otherwise.
    r.flightPhase = 0;
    r.airGround = 1;
    r.apTakeoff = 0;
    r.throttleMax = 0;
    r.landed = 0;
    r.seatbeltLight = 0;
    r.seatbeltSwitch = 0;
    r.noSmokingSwitch = 1;  // AUTO
    r.gearHandle = 1;
    r.retractableLights = {0, 0};
    r.fixedLights = {0, 0};
    r.positionLight = 0;
    r.parkBrake = 1;
    r.chocks = 0;
    r.doors = {0, 0, 0, 0};
    r.pax = 150;
    r.seats = 189;
    r.bus1 = r.bus2 = 1;
    r.genOff1 = r.genOff2 = 1;
    r.apuBus1 = r.apuBus2 = 1;
    r.masks = 0;
    r.cockpitDoor = 0;
    r.cruiseAlt = 36000;
    r.interphone = 0;
    purser.setAircraft(zibo737::makeType(model, zibo737::Options{}, r.seats));
  }

  /// Applies `change`, then holds the state for `seconds`, ticking at 4 Hz.
  void run(double seconds,
           const std::function<void(SimState&, Raw&)>& change = {}) {
    if (change) change(x, r);
    if (crewOn) {
      // A door the scene opens or closes is the user's or the ground crew's.
      for (size_t i = 0; i < 4 && i < r.doors.size(); ++i)
        if (r.doors[i] != sim.r.door[i])
          sim.r.door[i] = sim.target[i] = r.doors[i];
    }
    if (!settled) {
      settled = true;  // the purser takes the start state after 10 s
      seconds += 10.25;
    }
    for (double e = 0; e < seconds; e += 0.25) {
      t += 0.25;
      x.t = t;
      if (crewOn) {
        sim.move(0.25);
        for (size_t i = 0; i < 4 && i < r.doors.size(); ++i)
          r.doors[i] = sim.r.door[i];
      }
      SimState s = x;
      signals.apply(r, s);
      purser.tick(s, set);
      if (crewOn) {
        const CabinPlan plan = crew.plan(purser.cabinStage(), s, set);
        const zibo737::Crew::Step step = crew737.tick(plan, sim.r, s, sw);
        for (const std::string& c : step.commands) sim.command(c, s);
        for (const std::string& e : step.events) story.line(t, "crew", e);
      }
      const std::string status = replay::statusLine(purser);
      if (status != lastStatus) {
        lastStatus = status;
        story.line(t, "state", status);
        story.line(t, "next", replay::nextLine(purser));
      }
    }
  }
  bool runUntil(const std::function<bool()>& pred, double maxSeconds) {
    for (double e = 0; e < maxSeconds; e += 0.25) {
      if (pred()) return true;
      run(0.25);
    }
    return pred();
  }
  int count(const std::string& label) const {
    return static_cast<int>(
        std::count(out.played.begin(), out.played.end(), label));
  }
  size_t indexOf(const std::string& label) const {
    return static_cast<size_t>(
        std::find(out.played.begin(), out.played.end(), label) -
        out.played.begin());
  }

  // Segments of a normal 737 flight.
  void boardAndDepart() {
    run(30, [](SimState&, Raw& r) { r.doors[0] = 1; });  // L1 open
    run(600);
    run(20, [](SimState&, Raw& r) { r.doors[0] = 0; });
    run(10, [](SimState& x, Raw& r) {
      x.beacon = true;
      r.seatbeltSwitch = 2, r.seatbeltLight = 1;
    });
    run(60, [](SimState& x, Raw&) { x.gsKt = 2; });  // pushback
    run(60, [](SimState& x, Raw& r) {
      x.enginesRunning = 2, x.gsKt = 0;
      r.genOff1 = r.genOff2 = 0;  // the engine generators take the buses
      r.parkBrake = 0;
    });
    x.gsKt = 15;
    runUntil([&] { return purser.calling(); }, 600);
  }
  void answerCab() {
    run(5, [](SimState&, Raw& r) { r.interphone = 1; });
    run(10, [](SimState&, Raw& r) { r.interphone = 0; });
  }
  // Lining up: the strobes and the landing lights on.
  void lineUp() {
    run(20, [](SimState&, Raw& r) {
      r.positionLight = 1;
      r.retractableLights = {2, 2};
      r.fixedLights = {1, 1};
    });
  }
  void takeoffAndClimb(bool fmcSetUp = true) {
    run(30, [](SimState& x, Raw& r) {
      x.gsKt = 140;
      r.apTakeoff = 1, r.throttleMax = 0.95f;
    });
    run(20, [](SimState& x, Raw& r) {
      x.onGround = false, x.aglFt = 800, x.vsFpm = 2500;
      r.airGround = 0, r.gearHandle = 0, r.apTakeoff = 0, r.throttleMax = 0.8f;
    });
    const int climb = fmcSetUp ? 1 : 0, cruise = fmcSetUp ? 2 : 0;
    run(200, [climb](SimState& x, Raw& r) {
      x.aglFt = 8000, x.mslFt = 8000;
      r.flightPhase = climb;
      r.retractableLights = {0, 0}, r.fixedLights = {0, 0};
    });
    run(30, [](SimState& x, Raw& r) {
      x.aglFt = 12000, x.mslFt = 12000;
      r.seatbeltSwitch = 0, r.seatbeltLight = 0;
    });
    run(900, [cruise](SimState& x, Raw& r) {
      x.aglFt = 36000, x.mslFt = 36000, x.vsFpm = 0;
      r.flightPhase = cruise;
    });
  }
  void descendAndLand(bool fmcSetUp = true) {
    const int descent = fmcSetUp ? 5 : 0, approach = fmcSetUp ? 6 : 0;
    run(300, [descent](SimState& x, Raw& r) {
      x.vsFpm = -2000, x.aglFt = 20000, x.mslFt = 20000;
      r.flightPhase = descent;
      r.seatbeltSwitch = 2, r.seatbeltLight = 1;
    });
    run(400, [](SimState& x, Raw&) { x.aglFt = 9000, x.mslFt = 9000; });
    run(300, [approach](SimState& x, Raw& r) {
      x.aglFt = 3000, x.mslFt = 3000;
      r.flightPhase = approach;
      r.gearHandle = 1;  // NO SMOKING at AUTO lights
      r.retractableLights = {2, 2};
    });
    run(30, [](SimState& x, Raw&) { x.aglFt = 500; });
    run(20, [](SimState& x, Raw& r) {
      x.onGround = true, x.aglFt = 0, x.gsKt = 120, x.speedbrake = 1,
      x.vsFpm = 0;
      r.airGround = 1, r.landed = 1, r.flightPhase = 0;
    });
    run(150, [](SimState& x, Raw& r) {
      x.gsKt = 15, x.speedbrake = 0;
      r.landed = 0, r.retractableLights = {0, 0}, r.positionLight = -1;
    });
    run(30, [](SimState& x, Raw& r) {
      x.gsKt = 0, x.enginesRunning = 0, x.beacon = false;
      r.parkBrake = 1, r.genOff1 = r.genOff2 = 1;
      r.seatbeltSwitch = 0, r.seatbeltLight = 0;
    });
    run(400, [](SimState&, Raw& r) { r.doors[0] = 1; });
  }
};

const std::vector<std::string> kOrder = {"BoardingComplete",
                                         "ArmDoors",
                                         "PreSafetyBriefing",
                                         "SafetyBriefing",
                                         "CabinSecured(takeoff)#ip",
                                         "CrewTakeSeats(takeoff)",
                                         "AfterTakeoff",
                                         "DescentStarted",
                                         "AfterLanding",
                                         "DisarmDoors",
                                         "DeboardingStarted",
                                         "DeboardingComplete"};

void checkOrder(const Flight737& f) {
  size_t last = 0;
  for (const std::string& label : kOrder) {
    const size_t at = f.indexOf(label);
    CHECK_MESSAGE(at < f.out.played.size(), label);
    CHECK_MESSAGE(at >= last, label);
    last = at;
  }
}

}  // namespace

TEST_CASE(
    "737: a normal flight with the FMC set up plays every call in order") {
  Flight737 f;
  f.run(5);
  f.boardAndDepart();
  CHECK(f.count("BoardingWelcome") >= 2);
  CHECK(f.count("BoardingComplete") == 1);
  CHECK(f.count("ArmDoors") == 1);  // no slides: the beacon arms the doors
  REQUIRE(f.purser.calling());
  f.answerCab();
  CHECK(f.count("CabinSecured(takeoff)#ip") == 1);
  f.lineUp();
  CHECK(f.count("CrewTakeSeats(takeoff)") == 1);
  f.takeoffAndClimb();
  CHECK(f.count("AfterTakeoff") == 1);
  CHECK(f.count("SeatbeltSignOff") == 1);
  CHECK(f.purser.phase() == Phase::Cruise);
  CHECK(!f.purser.phaseEstimated());
  f.descendAndLand();
  CHECK(f.count("DescentStarted") == 1);
  CHECK(f.count("CrewTakeSeats(landing)") == 1);
  CHECK(f.count("AfterLanding") == 1);
  CHECK(f.count("DisarmDoors") == 1);
  CHECK(f.count("DeboardingComplete") == 1);
  checkOrder(f);
}

TEST_CASE("737: an FMC that was never set up: the purser estimates the phase") {
  Flight737 f;
  f.run(5);
  f.boardAndDepart();
  f.answerCab();
  f.lineUp();
  f.takeoffAndClimb(false);
  CHECK(f.purser.phaseEstimated());
  CHECK(f.purser.phase() == Phase::Cruise);
  CHECK(f.count("AfterTakeoff") == 1);
  f.descendAndLand(false);
  CHECK(f.count("DescentStarted") == 1);
  CHECK(f.count("AfterLanding") == 1);
  CHECK(f.count("DeboardingComplete") == 1);
  checkOrder(f);
}

TEST_CASE("737: the cabin call is the overhead ATTEND button") {
  Flight737 f;
  f.run(5);
  f.run(30, [](SimState&, Raw& r) { r.doors[0] = 1; });
  REQUIRE(f.purser.stageLabel() == "Boarding");
  f.purser.cabinCall();  // the module reports laminar/B738/push_button/attend
  f.run(90);
  CHECK(f.count("BoardingComplete") == 1);  // D-21: a call speeds boarding
}

TEST_CASE("737: a go-around and a second approach") {
  Flight737 f;
  f.run(5);
  f.boardAndDepart();
  f.answerCab();
  f.lineUp();
  f.takeoffAndClimb();
  f.run(300, [](SimState& x, Raw& r) {
    x.vsFpm = -2000, x.aglFt = 9000, x.mslFt = 9000;
    r.flightPhase = 5, r.seatbeltSwitch = 2, r.seatbeltLight = 1;
  });
  f.run(300, [](SimState& x, Raw& r) {
    x.aglFt = 1500, x.mslFt = 1500;
    r.flightPhase = 6, r.gearHandle = 1;
  });
  f.run(120, [](SimState& x, Raw& r) {
    x.vsFpm = 2000, x.aglFt = 3000, x.mslFt = 3000;
    r.flightPhase = 8;  // TO/GA pressed
  });
  CHECK(f.purser.phase() == Phase::GA);
  f.run(400, [](SimState& x, Raw& r) {
    x.vsFpm = -800, x.aglFt = 800, x.mslFt = 800;
    r.flightPhase = 6;
  });
  CHECK(f.count("AfterLanding") == 0);
  f.run(20, [](SimState& x, Raw& r) {
    x.onGround = true, x.aglFt = 0, x.gsKt = 120, x.speedbrake = 1;
    r.airGround = 1, r.landed = 1, r.flightPhase = 0;
  });
  f.run(150, [](SimState& x, Raw& r) {
    x.gsKt = 15, x.speedbrake = 0;
    r.landed = 0, r.retractableLights = {0, 0};
  });
  CHECK(f.count("AfterLanding") == 1);
  CHECK(f.count("DescentStarted") == 1);
}

TEST_CASE("737: a power transfer at the gate doesn't stop the boarding music") {
  Flight737 f;
  f.run(5);
  f.run(60, [](SimState&, Raw& r) { r.doors[0] = 1; });
  REQUIRE(f.purser.stageLabel() == "Boarding");
  REQUIRE(!f.out.music.empty());
  const size_t changes = f.out.music.size();
  // Ground power to APU: the buses drop for a moment.
  f.run(1, [](SimState&, Raw& r) { r.bus1 = r.bus2 = 0; });
  f.run(30, [](SimState&, Raw& r) {
    r.bus1 = r.bus2 = 1, r.apuBus1 = r.apuBus2 = 1;
  });
  CHECK(f.out.music.size() == changes);
  CHECK(f.out.music.back().second);
}

TEST_CASE("737: the -600 boards its smaller cabin sooner") {
  Flight737 small(zibo737::Model::B736);
  small.r.pax = 110;
  small.r.seats = 110;
  small.run(5);
  small.run(30, [](SimState&, Raw& r) { r.doors[0] = 1; });
  Flight737 big;
  big.run(5);
  big.run(30, [](SimState&, Raw& r) { r.doors[0] = 1; });
  REQUIRE(small.purser.boardingEndsAt() > 0);
  REQUIRE(big.purser.boardingEndsAt() > 0);
  CHECK(small.purser.boardingEndsAt() < big.purser.boardingEndsAt());
}

TEST_CASE("737 cabin crew: doors and cabin lights through a night flight") {
  using zibo737::Door;
  const auto at = [](Door d) { return static_cast<size_t>(d); };
  Flight737 f;
  f.crewOn = true;
  f.sw.doors = f.sw.lights = true;
  f.sw.nightPreset = 1;  // Night
  f.x.sunPitchDeg = -12;
  // At the gate the airstairs are out and the forward cargo door is open.
  f.sim.r.airstairsHidden = 0;
  f.sim.r.airstairRatio = f.sim.stairsTarget = 1;
  f.sim.r.door[at(Door::FwdCargo)] = f.sim.target[at(Door::FwdCargo)] = 1;
  f.run(5);
  f.run(30, [](SimState&, Raw& r) { r.doors[0] = 1; });  // L1 open: boarding
  CHECK(f.sim.r.paxLighting == zibo737::kWhiteBright);
  f.run(600);
  CHECK(f.sim.ran.empty());  // nothing moves while boarding

  // The beacon completes boarding with L1 still open: the airstairs are
  // stowed first, then every door closes.
  f.run(10, [](SimState& x, Raw& r) {
    x.beacon = true;
    r.seatbeltSwitch = 2, r.seatbeltLight = 1;
  });
  REQUIRE(f.count("BoardingComplete") == 1);
  f.run(60);
  CHECK(f.sim.r.airstairsHidden == 1);
  CHECK(f.sim.r.door[at(Door::L1)] == 0);
  CHECK(f.sim.r.door[at(Door::FwdCargo)] == 0);
  CHECK(f.count("ArmDoors") == 1);

  f.run(60, [](SimState& x, Raw&) { x.gsKt = 2; });  // pushback
  f.run(60, [](SimState& x, Raw& r) {
    x.enginesRunning = 2, x.gsKt = 0;
    r.genOff1 = r.genOff2 = 0;
    r.parkBrake = 0;
  });
  f.x.gsKt = 15;
  f.runUntil([&] { return f.purser.calling(); }, 600);
  f.answerCab();
  CHECK(f.sim.r.paxLighting == zibo737::kWhiteBright);  // taxiing
  f.lineUp();
  CHECK(f.count("CabinDim(takeoff)") == 1);
  CHECK(f.sim.r.paxLighting == zibo737::kTakeoffLanding);

  f.takeoffAndClimb();
  REQUIRE(f.count("AfterTakeoff") == 1);
  REQUIRE(f.count("CabinService") == 1);
  CHECK(f.sim.r.paxLighting == zibo737::kWhiteMedium);  // the service

  f.run(300, [](SimState& x, Raw& r) {
    x.vsFpm = -2000, x.aglFt = 20000, x.mslFt = 20000;
    r.flightPhase = 5;
    r.seatbeltSwitch = 2, r.seatbeltLight = 1;
  });
  REQUIRE(f.count("DescentStarted") == 1);
  CHECK(f.sim.r.paxLighting == zibo737::kNight);  // the service is over
  f.run(400, [](SimState& x, Raw&) { x.aglFt = 9000, x.mslFt = 9000; });
  f.run(300, [](SimState& x, Raw& r) {
    x.aglFt = 3000, x.mslFt = 3000;
    r.flightPhase = 6;
    r.gearHandle = 1;
    r.retractableLights = {2, 2};
  });
  CHECK(f.sim.r.paxLighting == zibo737::kTakeoffLanding);
  f.run(30, [](SimState& x, Raw&) { x.aglFt = 500; });
  f.run(20, [](SimState& x, Raw& r) {
    x.onGround = true, x.aglFt = 0, x.gsKt = 120, x.speedbrake = 1, x.vsFpm = 0;
    r.airGround = 1, r.landed = 1, r.flightPhase = 0;
  });
  f.run(150, [](SimState& x, Raw& r) {
    x.gsKt = 15, x.speedbrake = 0;
    r.landed = 0, r.retractableLights = {0, 0}, r.positionLight = -1;
  });
  REQUIRE(f.count("AfterLanding") == 1);
  CHECK(f.sim.r.paxLighting == zibo737::kWhiteBright);

  // At the stand nobody opens a door: the crew opens L1 after DisarmDoors,
  // and that starts deboarding.
  f.run(30, [](SimState& x, Raw& r) {
    x.gsKt = 0, x.enginesRunning = 0, x.beacon = false;
    r.parkBrake = 1, r.genOff1 = r.genOff2 = 1;
    r.seatbeltSwitch = 0, r.seatbeltLight = 0;
  });
  REQUIRE(f.count("DisarmDoors") == 1);
  f.run(400);
  CHECK(f.sim.r.door[at(Door::L1)] == 1);
  CHECK(f.count("DeboardingStarted") == 1);
  CHECK(f.count("DeboardingComplete") == 1);
  checkOrder(f);
}

TEST_CASE(
    "737 cabin crew: with Airside Services the doors are the aircraft's") {
  Flight737 f;
  f.crewOn = true;
  f.sw.doors = true;
  f.sim.r.autoFlight = 2;  // the EFB's turnaround is loading the aircraft
  f.run(5);
  f.run(30, [](SimState&, Raw& r) { r.doors[0] = 1; });
  f.run(10, [](SimState& x, Raw& r) {
    x.beacon = true;
    r.seatbeltSwitch = 2, r.seatbeltLight = 1;
  });
  REQUIRE(f.count("BoardingComplete") == 1);
  f.run(60);
  CHECK(f.sim.ran.empty());
  CHECK(f.sim.r.door[0] == 1);
}

TEST_CASE("737: a call waits while the aircraft's own captain message plays") {
  Flight737 f;
  f.run(5);
  // Boarding starts while the captain's welcome (55 s) plays.
  f.run(90, [](SimState&, Raw& r) {
    r.doors[0] = 1;
    r.msgPlaying = 40;
  });
  REQUIRE(f.purser.stageLabel() == "Boarding");
  CHECK(f.count("BoardingWelcome") == 0);
  f.run(10, [](SimState&, Raw& r) { r.msgPlaying = 0; });
  CHECK(f.count("BoardingWelcome") == 1);
}
