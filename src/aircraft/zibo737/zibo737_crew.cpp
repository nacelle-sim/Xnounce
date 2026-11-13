/// @file
/// Implements `src/aircraft/zibo737/zibo737_crew.h`; the interface is
/// documented there.
#include "aircraft/zibo737/zibo737_crew.h"

namespace xn::zibo737 {

namespace {

constexpr int kNone = ActionTracker::kNone;

// The main doors take 7 s and the mid exits 2 s (docs/aircraft/zibo737.md
// § 6); the times below leave a margin for a slow frame rate.
const DoorInfo kDoors[kDoorCount] = {
    {"L1", "737u/doors/L1", "sim/flight_controls/door_open_1",
     "sim/flight_controls/door_close_1", 15, false},
    {"R1", "737u/doors/R1", "sim/flight_controls/door_open_5",
     "sim/flight_controls/door_close_5", 15, false},
    {"L2", "737u/doors/L2", "sim/flight_controls/door_open_4",
     "sim/flight_controls/door_close_4", 15, false},
    {"R2", "737u/doors/R2", "sim/flight_controls/door_open_8",
     "sim/flight_controls/door_close_8", 15, false},
    {"left mid exit", "737u/doors/L_mid", "laminar/B738/door/L_mid_exit_open",
     "laminar/B738/door/L_mid_exit_close", 6, false},
    {"right mid exit", "737u/doors/R_mid", "laminar/B738/door/R_mid_exit_open",
     "laminar/B738/door/R_mid_exit_close", 6, false},
    {"forward cargo", "737u/doors/Fwd_Cargo", "sim/flight_controls/door_open_9",
     "sim/flight_controls/door_close_9", 15, true},
    {"aft cargo", "737u/doors/aft_Cargo", "sim/flight_controls/door_open_10",
     "sim/flight_controls/door_close_10", 15, true},
};

/// Stowing takes 32 s, 55 s on the battery alone.
constexpr double kStairsSettleS = 75;
/// The light commands act at once.
constexpr double kLightSettleS = 3;

/// A position at either end: 0 closed or 1 open.
bool atEnd(float ratio) {
  return ratio >= 0 && (ratio <= 0.001f || ratio >= 0.999f);
}

}  // namespace

const char* const kAirstairsToggle = "laminar/B738/airstairs_toggle";

const DoorInfo& doorInfo(Door d) { return kDoors[static_cast<int>(d)]; }

const char* paxLightCommand(int light) {
  switch (light) {
    case kWhiteBright:
      return "laminar/B738/cabin/pax_white_bright";
    case kTakeoffLanding:
      return "laminar/B738/cabin/pax_to_landing";
    case kCruise:
      return "laminar/B738/cabin/pax_cruise";
    case kWhiteMedium:
      return "laminar/B738/cabin/pax_white_medium";
    case kNight:
    case kSleep:
      return "laminar/B738/cabin/pax_night_sleep";
    default:
      return "";
  }
}

const char* paxLightName(int light) {
  switch (light) {
    case kWhiteBright:
      return "white bright";
    case kTakeoffLanding:
      return "takeoff and landing";
    case kCruise:
      return "cruise";
    case kWhiteMedium:
      return "white medium";
    case kNight:
      return "night";
    case kSleep:
      return "sleep";
    default:
      return "unknown";
  }
}

int nightPresetLight(int preset) {
  return preset <= 0 ? kCruise : preset == 1 ? kNight : kSleep;
}

Crew::Crew() : light_(kLightSettleS, 2) {
  for (int k = 0; k < kDoorCount; ++k)
    door_[static_cast<size_t>(k)] = ActionTracker(kDoors[k].settleS, 1);
  stairs_ = ActionTracker(kStairsSettleS, 1);
}

void Crew::reset() {
  for (ActionTracker& t : door_) t.reset();
  stairs_.reset();
  light_.reset();
  doorsNote_.clear();
  lightsNote_.clear();
  doorsWhy_.clear();
}

Crew::Step Crew::tick(const CabinPlan& plan, const CrewReadings& r,
                      const SimState& s, const Switches& sw) {
  Step out;
  doors(plan, r, s, sw.doors, out);
  lights(plan, r, s, sw, out);
  return out;
}

void Crew::doors(const CabinPlan& plan, const CrewReadings& r,
                 const SimState& s, bool on, Step& out) {
  std::array<int, kDoorCount> want;
  want.fill(kNone);
  int stairsWant = kNone;
  std::string why;  // why the crew leaves (some of) the doors alone
  if (!on) {
    doorsNote_.clear();
  } else if (r.autoFlight > 0) {
    why = "the doors are left to the aircraft's Airside Services";
  } else if (plan.doors == DoorPlan::Close) {
    for (int k = 0; k < kDoorCount; ++k) {
      if (r.door[static_cast<size_t>(k)] < 0) continue;  // not on this model
      if (kDoors[k].cargo && r.cargoLoading == 1) {
        why =
            "the cargo doors stay open while the aircraft's cargo-loading "
            "sound plays";
        continue;
      }
      want[static_cast<size_t>(k)] = 0;
    }
    // The aircraft keeps L1 open over its airstairs: they are stowed first,
    // and they stay stowed while the doors are closed.
    if (want[0] == 0 && r.airstairsHidden >= 0) {
      stairsWant = 1;
      if (r.airstairsHidden == 0) want[0] = kNone;
    }
  } else if (plan.doors == DoorPlan::OpenForward && r.door[0] >= 0) {
    if (s.enginesRunning > 0 || !s.onGround)
      why = "door L1 waits for the engines to stop";
    else
      want[0] = 1;
  }
  if (why != doorsWhy_) {
    doorsWhy_ = why;
    if (!why.empty()) out.events.push_back(why);
  }

  // The airstairs first: their command toggles, so it is sent only while the
  // reading shows them out and not moving.
  {
    const bool settled = r.airstairsHidden >= 0 &&
                         (r.airstairRatio < 0 || atEnd(r.airstairRatio));
    switch (stairs_.update(stairsWant, r.airstairsHidden, settled, s.t)) {
      case ActionTracker::Event::Act:
        out.commands.emplace_back(kAirstairsToggle);
        out.events.emplace_back("stow the airstairs");
        break;
      case ActionTracker::Event::Reached:
        out.events.emplace_back("airstairs stowed");
        break;
      case ActionTracker::Event::Failed:
        out.events.emplace_back(
            "the airstairs didn't stow: door L1 stays open until the next "
            "stage");
        break;
      case ActionTracker::Event::UserTook:
        out.events.emplace_back(
            "the airstairs were moved by the user or the aircraft: left to "
            "them until the next stage");
        break;
      case ActionTracker::Event::None:
        break;
    }
  }

  int acting = 0;
  for (int k = 0; k < kDoorCount; ++k) {
    const size_t i = static_cast<size_t>(k);
    const float ratio = r.door[i];
    const int reading = ratio < 0 ? kNone : (ratio >= 0.5f ? 1 : 0);
    const DoorInfo& d = kDoors[k];
    const std::string name = std::string("door ") + d.name;
    switch (door_[i].update(want[i], reading, atEnd(ratio), s.t)) {
      case ActionTracker::Event::Act:
        out.commands.emplace_back(want[i] == 1 ? d.open : d.close);
        out.events.push_back((want[i] == 1 ? "open " : "close ") + name);
        break;
      case ActionTracker::Event::Reached:
        out.events.push_back(name + (want[i] == 1 ? " open" : " closed"));
        break;
      case ActionTracker::Event::Failed:
        out.events.push_back(name +
                             (want[i] == 1 ? " didn't open" : " didn't close") +
                             ": left until the next stage");
        break;
      case ActionTracker::Event::UserTook:
        out.events.push_back(
            name +
            " was moved by the user or the aircraft: left to them until the "
            "next stage");
        break;
      case ActionTracker::Event::None:
        break;
    }
    if (door_[i].state() == ActionTracker::State::Acting) ++acting;
  }
  if (!on) return;
  if (stairs_.state() == ActionTracker::State::Acting)
    doorsNote_ = "stowing the airstairs";
  else if (acting > 0)
    doorsNote_ = plan.doors == DoorPlan::OpenForward ? "opening L1"
                                                     : "closing the doors";
  else if (!why.empty())
    doorsNote_ = why;
  else
    doorsNote_ = plan.doors == DoorPlan::Close ? "closed for the flight"
                 : plan.doors == DoorPlan::OpenForward
                     ? "L1 open for deboarding"
                     : "";
}

void Crew::lights(const CabinPlan& plan, const CrewReadings& r,
                  const SimState& s, const Switches& sw, Step& out) {
  int want = kNone;
  if (sw.lights) {
    switch (plan.light) {
      case CabinLight::Leave:
        break;
      case CabinLight::Bright:
        want = kWhiteBright;
        break;
      case CabinLight::Medium:
        want = kWhiteMedium;
        break;
      case CabinLight::TakeoffLanding:
        want = kTakeoffLanding;
        break;
      case CabinLight::NightCruise:
        want = nightPresetLight(sw.nightPreset);
        if (want == kSleep && noSleep_) want = kNight;
        break;
    }
  }
  const int reading = r.paxLighting;
  switch (light_.update(want, reading, reading >= 0, s.t)) {
    case ActionTracker::Event::Act:
      out.commands.emplace_back(paxLightCommand(want));
      out.events.push_back(std::string("cabin lights: ") + paxLightName(want));
      break;
    case ActionTracker::Event::Reached:
      out.events.push_back(std::string("cabin lights now ") +
                           paxLightName(want));
      break;
    case ActionTracker::Event::Failed:
      if (want == kSleep && reading == kNight) {
        // The -900's button gives night only.
        noSleep_ = true;
        out.events.emplace_back(
            "this cabin has no sleep setting: night instead");
      } else {
        out.events.push_back(std::string("cabin lights didn't change to ") +
                             paxLightName(want) +
                             ": left until the next stage");
      }
      break;
    case ActionTracker::Event::UserTook:
      out.events.emplace_back(
          "cabin lights changed by the user: left to the user until the next "
          "stage");
      break;
    case ActionTracker::Event::None:
      break;
  }
  if (!sw.lights) {
    lightsNote_.clear();
  } else if (light_.state() == ActionTracker::State::User) {
    lightsNote_ = "left to you until the next stage";
  } else if (want != kNone) {
    lightsNote_ = paxLightName(want);
  }
}

}  // namespace xn::zibo737
