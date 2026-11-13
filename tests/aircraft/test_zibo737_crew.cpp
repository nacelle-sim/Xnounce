// The Zibo / LevelUp 737 crew's decisions: which command for which door and
// light setting, and when (VP_SPEC § 4.7, docs/aircraft/zibo737.md § 6).
#include <doctest/doctest.h>

#include <algorithm>
#include <string>

#include "aircraft/zibo737/zibo737_crew.h"

using namespace xn;
using namespace xn::zibo737;

namespace {

size_t at(Door d) { return static_cast<size_t>(d); }

struct Fixture {
  Crew crew;
  CrewReadings r;
  SimState s;
  Crew::Switches sw;
  CabinPlan plan;
  Fixture() {
    r.door.fill(0);
    r.door[at(Door::LMid)] = r.door[at(Door::RMid)] = -1;
    r.airstairsHidden = 1;
    r.airstairRatio = 0;
    r.paxLighting = kWhiteBright;
    r.autoFlight = 0;
    r.cargoLoading = 0;
    sw.doors = sw.lights = true;
  }
  Crew::Step tick(double t) {
    s.t = t;
    return crew.tick(plan, r, s, sw);
  }
};

bool has(const Crew::Step& st, const std::string& c) {
  return std::find(st.commands.begin(), st.commands.end(), c) !=
         st.commands.end();
}

}  // namespace

TEST_CASE("737 crew: closing sends the explicit close of each open door once") {
  Fixture f;
  f.r.door[at(Door::L1)] = 1;
  f.r.door[at(Door::R1)] = 1;
  f.plan.doors = DoorPlan::Close;
  Crew::Step st = f.tick(0);
  CHECK(st.commands.size() == 2);
  CHECK(has(st, "sim/flight_controls/door_close_1"));
  CHECK(has(st, "sim/flight_controls/door_close_5"));
  f.r.door[at(Door::L1)] = 0.5f;  // moving
  CHECK(f.tick(1).commands.empty());
  f.r.door[at(Door::L1)] = 0;
  f.r.door[at(Door::R1)] = 0;
  st = f.tick(8);
  CHECK(st.commands.empty());
  CHECK(std::count(st.events.begin(), st.events.end(), "door L1 closed") == 1);
}

TEST_CASE("737 crew: L1 waits until the airstairs are stowed") {
  Fixture f;
  f.r.door[at(Door::L1)] = 1;
  f.r.airstairsHidden = 0;
  f.r.airstairRatio = 1;
  f.plan.doors = DoorPlan::Close;
  Crew::Step st = f.tick(0);
  REQUIRE(st.commands.size() == 1);
  CHECK(st.commands[0] == kAirstairsToggle);
  f.r.airstairRatio = 0.5f;  // stowing
  CHECK(f.tick(10).commands.empty());
  f.r.airstairRatio = 0;
  f.r.airstairsHidden = 1;
  st = f.tick(32);
  CHECK(has(st, "sim/flight_controls/door_close_1"));
  CHECK(!has(st, kAirstairsToggle));
}

TEST_CASE("737 crew: no door moves while Airside Services runs") {
  Fixture f;
  f.r.door[at(Door::L1)] = 1;
  f.r.autoFlight = 3;
  f.plan.doors = DoorPlan::Close;
  const Crew::Step st = f.tick(0);
  CHECK(st.commands.empty());
  REQUIRE(st.events.size() == 1);
  CHECK(f.tick(1).events.empty());  // said once
}

TEST_CASE(
    "737 crew: cargo doors stay open while the cargo-loading sound plays") {
  Fixture f;
  f.r.door[at(Door::FwdCargo)] = 1;
  f.r.door[at(Door::R2)] = 1;
  f.r.cargoLoading = 1;
  f.plan.doors = DoorPlan::Close;
  const Crew::Step st = f.tick(0);
  CHECK(has(st, "sim/flight_controls/door_close_8"));
  CHECK(!has(st, "sim/flight_controls/door_close_9"));
}

TEST_CASE("737 crew: L1 opens for deboarding only with the engines off") {
  Fixture f;
  f.plan.doors = DoorPlan::OpenForward;
  f.s.enginesRunning = 1;
  CHECK(f.tick(0).commands.empty());
  f.s.enginesRunning = 0;
  const Crew::Step st = f.tick(1);
  REQUIRE(st.commands.size() == 1);
  CHECK(st.commands[0] == "sim/flight_controls/door_open_1");
}

TEST_CASE("737 crew: a door the user opens again is left to the user") {
  Fixture f;
  f.r.door[at(Door::R1)] = 1;
  f.plan.doors = DoorPlan::Close;
  f.tick(0);
  f.r.door[at(Door::R1)] = 0;
  f.tick(8);
  f.r.door[at(Door::R1)] = 1;  // catering wants it open
  const Crew::Step st = f.tick(20);
  CHECK(st.commands.empty());
  CHECK(f.tick(60).commands.empty());
}

TEST_CASE("737 crew: switched off, nothing is sent") {
  Fixture f;
  f.sw.doors = f.sw.lights = false;
  f.r.door[at(Door::L1)] = 1;
  f.plan.doors = DoorPlan::Close;
  f.plan.light = CabinLight::TakeoffLanding;
  CHECK(f.tick(0).commands.empty());
}

TEST_CASE("737 crew: each light setting has its command") {
  Fixture f;
  f.plan.light = CabinLight::TakeoffLanding;
  Crew::Step st = f.tick(0);
  REQUIRE(st.commands.size() == 1);
  CHECK(st.commands[0] == "laminar/B738/cabin/pax_to_landing");
  f.r.paxLighting = kTakeoffLanding;
  f.tick(0.25);
  f.plan.light = CabinLight::Medium;
  st = f.tick(0.5);
  CHECK(st.commands.at(0) == "laminar/B738/cabin/pax_white_medium");
  f.r.paxLighting = kWhiteMedium;
  f.sw.nightPreset = 0;
  f.plan.light = CabinLight::NightCruise;
  st = f.tick(1);
  CHECK(st.commands.at(0) == "laminar/B738/cabin/pax_cruise");
}

TEST_CASE("737 crew: the Sleep preset steps the night/sleep button twice") {
  Fixture f;
  f.sw.nightPreset = 2;
  f.r.paxLighting = kCruise;
  f.plan.light = CabinLight::NightCruise;
  CHECK(f.tick(0).commands.at(0) == "laminar/B738/cabin/pax_night_sleep");
  f.r.paxLighting = kNight;
  CHECK(f.tick(0.25).commands.empty());  // settles first
  CHECK(f.tick(0.5).commands.at(0) == "laminar/B738/cabin/pax_night_sleep");
  f.r.paxLighting = kSleep;
  const Crew::Step st = f.tick(0.75);
  CHECK(st.commands.empty());
  CHECK(st.events.at(0) == "cabin lights now sleep");
}

TEST_CASE("737 crew: a cabin without a sleep setting gets night instead") {
  Fixture f;
  f.sw.nightPreset = 2;
  f.r.paxLighting = kCruise;
  f.plan.light = CabinLight::NightCruise;
  f.tick(0);
  f.r.paxLighting = kNight;  // the -900's button stays at night
  f.tick(0.25);
  f.tick(0.5);
  for (double t = 0.75; t < 5; t += 0.25) f.tick(t);
  // Night is accepted, and nothing more is pressed.
  for (double t = 5; t < 20; t += 0.25) CHECK(f.tick(t).commands.empty());
}
