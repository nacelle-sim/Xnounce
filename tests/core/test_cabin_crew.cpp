// The cabin crew's plan and the follow-up of each action (VP_SPEC § 4.7,
// docs/ARCHITECTURE.md § 4 rule 4).
#include <doctest/doctest.h>

#include "core/cabin_crew/action_tracker.h"
#include "core/cabin_crew/cabin_crew.h"
#include "core/cabin_crew/option_hold.h"

using namespace xn;
using Ev = ActionTracker::Event;
using St = ActionTracker::State;

TEST_CASE("action tracker: acts once on a settled item, then waits for it") {
  ActionTracker a(10, 1);
  CHECK(a.update(ActionTracker::kNone, 1, true, 0) == Ev::None);
  CHECK(a.update(0, 1, false, 1) == Ev::None);  // still moving: wait
  CHECK(a.update(0, 1, true, 2) == Ev::Act);
  CHECK(a.update(0, 1, true, 3) == Ev::None);   // no second act
  CHECK(a.update(0, 1, false, 4) == Ev::None);  // moving
  CHECK(a.update(0, 0, true, 9) == Ev::Reached);
  CHECK(a.state() == St::Done);
  CHECK(a.acts() == 1);
}

TEST_CASE("action tracker: an item already as wished needs nothing") {
  ActionTracker a(10, 1);
  CHECK(a.update(0, 0, true, 0) == Ev::None);
  CHECK(a.state() == St::Done);
  CHECK(a.acts() == 0);
}

TEST_CASE("action tracker: an act that doesn't take fails once, no loop") {
  ActionTracker a(10, 1);
  CHECK(a.update(0, 1, true, 0) == Ev::Act);
  CHECK(a.update(0, 1, true, 5) == Ev::None);
  CHECK(a.update(0, 1, true, 10) == Ev::Failed);
  for (double t = 11; t < 100; t += 1)
    CHECK(a.update(0, 1, true, t) == Ev::None);
  CHECK(a.state() == St::Failed);
  // The next stage brings a new wish and a new try.
  CHECK(a.update(1, 1, true, 101) == Ev::None);
  CHECK(a.state() == St::Done);
  CHECK(a.update(0, 1, true, 102) == Ev::Act);
}

TEST_CASE(
    "action tracker: an item changed after it was reached is the user's") {
  ActionTracker a(10, 1);
  a.update(0, 1, true, 0);
  a.update(0, 0, true, 8);
  REQUIRE(a.state() == St::Done);
  CHECK(a.update(0, 0, false, 9) == Ev::None);  // the user opens it again
  CHECK(a.update(0, 1, true, 16) == Ev::UserTook);
  CHECK(a.update(0, 1, true, 30) == Ev::None);
  CHECK(a.state() == St::User);
  // Until the wish changes.
  CHECK(a.update(1, 1, true, 31) == Ev::None);
  CHECK(a.state() == St::Done);
}

TEST_CASE("action tracker: a stepping control gets one more act per step") {
  // The 737's night/sleep button: 2 -> 4 -> 5.
  ActionTracker a(3, 2);
  CHECK(a.update(5, 2, true, 0) == Ev::Act);
  CHECK(a.update(5, 4, true, 0.25) == Ev::None);  // moved: may act again
  CHECK(a.update(5, 4, true, 0.5) == Ev::Act);
  CHECK(a.update(5, 5, true, 0.75) == Ev::Reached);
  // A button that stops at 4 (the -900) runs out of acts.
  ActionTracker b(3, 2);
  b.update(5, 2, true, 0);
  b.update(5, 4, true, 0.25);
  CHECK(b.update(5, 4, true, 0.5) == Ev::Act);
  CHECK(b.update(5, 4, true, 2) == Ev::None);
  CHECK(b.update(5, 4, true, 3.5) == Ev::Failed);
}

TEST_CASE("option hold: holds while on, puts the aircraft's value back") {
  OptionHold h(0);
  CHECK(!h.update(false, 1.0f));
  auto w = h.update(true, 1.0f);
  REQUIRE(w);
  CHECK(*w == 0.0f);
  CHECK(!h.update(true, 0.0f));
  CHECK(h.holding());
  auto back = h.update(false, 0.0f);
  REQUIRE(back);
  CHECK(*back == 1.0f);
  CHECK(!h.holding());
}

TEST_CASE("option hold: an option already at the value isn't put back") {
  OptionHold h(0);
  CHECK(!h.update(true, 0.0f));
  CHECK(h.holding());
  CHECK(!h.release());
}

TEST_CASE("option hold: a value the user changes is left as the user set it") {
  OptionHold h(0);
  h.update(true, 1.0f);
  h.update(true, 0.0f);
  CHECK(!h.update(true, 2.0f));  // the user picked another setting
  CHECK(h.takeChanged());
  CHECK(!h.takeChanged());
  CHECK(!h.update(true, 2.0f));   // not taken back while held
  CHECK(!h.update(false, 2.0f));  // and not put back
  // The next time the actions come on, it is held again.
  auto w = h.update(true, 2.0f);
  REQUIRE(w);
  CHECK(*w == 0.0f);
}

TEST_CASE("option hold: an unknown reading or a gone aircraft writes nothing") {
  OptionHold h(0);
  CHECK(!h.update(true, std::nullopt));
  CHECK(!h.holding());
  h.update(true, 1.0f);
  h.forget();
  CHECK(!h.release());
}

namespace {

struct Plan {
  CabinCrew crew{3};
  CabinStage c;
  SimState s;
  Settings set;
  CabinPlan at(double t) {
    s.t = t;
    return crew.plan(c, s, set);
  }
};

}  // namespace

TEST_CASE(
    "cabin crew: doors close at BoardingComplete, L1 opens after DisarmDoors") {
  Plan p;
  CHECK(p.at(0).doors == DoorPlan::Leave);
  p.c.boarding = true;
  CHECK(p.at(10).doors == DoorPlan::Leave);  // boarding: the doors are open
  p.c.boardingComplete = true;
  CHECK(p.at(20).doors == DoorPlan::Close);
  p.c.takeoff = p.c.afterTakeoff = true;
  CHECK(p.at(1000).doors == DoorPlan::Close);
  p.c.touchdown = true;
  CHECK(p.at(5000).doors == DoorPlan::Leave);  // until DisarmDoors
  p.c.doorsDisarmed = true;
  p.c.disarmedAt = 5100;
  CHECK(p.at(5100).doors == DoorPlan::Leave);
  CHECK(p.at(5100 + CabinCrew::kOpenDelayMin - 1).doors == DoorPlan::Leave);
  CHECK(p.at(5100 + CabinCrew::kOpenDelayMax).doors == DoorPlan::OpenForward);
  p.c.deboarding = true;
  CHECK(p.at(5200).doors == DoorPlan::OpenForward);
  p.c.deboardingComplete = true;
  CHECK(p.at(5400).doors == DoorPlan::Leave);  // the turnaround is the user's
}

TEST_CASE("cabin crew: white bright all day, whatever the stage") {
  Plan p;
  p.s.sunPitchDeg = 20;
  CHECK(p.at(0).light == CabinLight::Bright);
  p.c.boardingComplete = p.c.seatedForTakeoff = p.c.takeoff = true;
  CHECK(p.at(1).light == CabinLight::Bright);
  p.c.afterTakeoff = true;
  p.c.cabinServiceAt = 2;
  CHECK(p.at(3).light == CabinLight::Bright);
}

TEST_CASE("cabin crew: the lights through a night flight (Fenix rules)") {
  Plan p;
  p.s.sunPitchDeg = -10;
  CHECK(p.at(0).light == CabinLight::Bright);  // gate and boarding
  p.c.boardingComplete = p.c.doorsArmed = p.c.securedForTakeoff = true;
  CHECK(p.at(1).light == CabinLight::Bright);  // taxi
  p.c.seatedForTakeoff = true;
  CHECK(p.at(2).light == CabinLight::TakeoffLanding);
  p.c.takeoff = true;
  CHECK(p.at(3).light == CabinLight::TakeoffLanding);
  p.c.afterTakeoff = true;
  CHECK(p.at(4).light == CabinLight::NightCruise);
  p.c.cabinServiceAt = 5;
  CHECK(p.at(5).light == CabinLight::Medium);
  p.c.descentStarted = true;
  CHECK(p.at(6).light == CabinLight::NightCruise);
  p.c.securedForLanding = true;
  CHECK(p.at(7).light == CabinLight::NightCruise);
  p.c.seatedForLanding = true;
  CHECK(p.at(8).light == CabinLight::TakeoffLanding);
  p.c.touchdown = true;
  CHECK(p.at(9).light == CabinLight::TakeoffLanding);
  p.c.afterLanding = true;
  CHECK(p.at(10).light == CabinLight::Bright);
}

TEST_CASE("cabin crew: no line-up signal: takeoff thrust dims the lights") {
  Plan p;
  p.s.sunPitchDeg = -10;
  p.c.boardingComplete = p.c.securedForTakeoff = true;
  CHECK(p.at(0).light == CabinLight::Bright);
  p.c.takeoff = true;
  CHECK(p.at(1).light == CabinLight::TakeoffLanding);
}

TEST_CASE("cabin crew: the lights follow a sunset without flickering") {
  Plan p;
  p.c.boardingComplete = p.c.takeoff = p.c.afterTakeoff = true;
  p.s.sunPitchDeg = 1;
  CHECK(p.at(0).light == CabinLight::Bright);
  p.s.sunPitchDeg = -0.2f;  // within the hysteresis
  CHECK(p.at(1).light == CabinLight::Bright);
  p.s.sunPitchDeg = -0.6f;
  CHECK(p.at(2).light == CabinLight::NightCruise);
  p.s.sunPitchDeg = 0.3f;  // back up a little: still dark
  CHECK(p.at(3).light == CabinLight::NightCruise);
  p.s.sunPitchDeg = 0.6f;  // sunrise
  CHECK(p.at(4).light == CabinLight::Bright);
}
