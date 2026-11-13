/// @file
/// Implements `src/core/cabin_crew/cabin_crew.h`; the interface is documented
/// there.
#include "core/cabin_crew/cabin_crew.h"

namespace xn {

const char* doorPlanName(DoorPlan p) {
  switch (p) {
    case DoorPlan::Leave:
      return "leave";
    case DoorPlan::Close:
      return "close";
    case DoorPlan::OpenForward:
      return "open forward";
  }
  return "?";
}

const char* cabinLightName(CabinLight l) {
  switch (l) {
    case CabinLight::Leave:
      return "leave";
    case CabinLight::Bright:
      return "bright";
    case CabinLight::Medium:
      return "medium";
    case CabinLight::TakeoffLanding:
      return "takeoff and landing";
    case CabinLight::NightCruise:
      return "night cruise";
  }
  return "?";
}

CabinCrew::CabinCrew(unsigned seed) : rng_(seed) {}

void CabinCrew::reset() {
  darkKnown_ = false;
  dark_ = false;
  delayFor_ = -1;
  openDelay_ = kOpenDelayMin;
}

CabinPlan CabinCrew::plan(const CabinStage& c, const SimState& s,
                          const Settings& set) {
  // D-15 with a little hysteresis.
  if (!darkKnown_) {
    dark_ = s.sunPitchDeg < set.darkSunDeg;
    darkKnown_ = true;
  } else if (dark_ && s.sunPitchDeg > set.darkSunDeg + kDarkHysteresisDeg) {
    dark_ = false;
  } else if (!dark_ && s.sunPitchDeg < set.darkSunDeg - kDarkHysteresisDeg) {
    dark_ = true;
  }
  CabinPlan p;
  p.doors = doors(c, s);
  p.light = light(c);
  return p;
}

DoorPlan CabinCrew::doors(const CabinStage& c, const SimState& s) {
  // After DisarmDoors and the cross-check, the forward door opens. Once
  // deboarding is complete the doors are the user's (the turnaround and the
  // next boarding).
  if (c.doorsDisarmed && !c.deboardingComplete && !c.flightComplete) {
    if (delayFor_ != c.disarmedAt) {
      delayFor_ = c.disarmedAt;
      openDelay_ = std::uniform_real_distribution<double>(kOpenDelayMin,
                                                          kOpenDelayMax)(rng_);
    }
    return s.t >= c.disarmedAt + openDelay_ ? DoorPlan::OpenForward
                                            : DoorPlan::Leave;
  }
  // From BoardingComplete until the aircraft is back on the ground, the doors
  // stay closed. After the touchdown they wait for DisarmDoors.
  if (c.boardingComplete && !c.touchdown) return DoorPlan::Close;
  return DoorPlan::Leave;
}

CabinLight CabinCrew::light(const CabinStage& c) const {
  if (!dark_) return CabinLight::Bright;
  if (c.touchdown)
    return c.afterLanding ? CabinLight::Bright : CabinLight::TakeoffLanding;
  if (c.seatedForLanding) return CabinLight::TakeoffLanding;
  if (c.afterTakeoff) {
    const bool service = c.cabinServiceAt >= 0 && !c.descentStarted;
    return service ? CabinLight::Medium : CabinLight::NightCruise;
  }
  if (c.takeoff || c.seatedForTakeoff) return CabinLight::TakeoffLanding;
  return CabinLight::Bright;
}

}  // namespace xn
