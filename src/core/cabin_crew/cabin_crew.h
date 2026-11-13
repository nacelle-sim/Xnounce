/// @file
/// The cabin crew's plan for the aircraft: what the doors and the cabin lights
/// should be at this stage of the flight (VP_SPEC § 4.7).
///
/// The purser decides the announcements; `CabinCrew` reads how far the purser
/// has come (`CabinStage`) and the snapshot, and says what the crew would do
/// with the doors and the lights. Whether anything is done, and how, is the
/// aircraft module's business (`src/aircraft/actions.h`): it acts only on what
/// the user switched on. The purser never sees the plan.
#pragma once

#include <random>

#include "core/purser/purser.h"
#include "core/settings.h"
#include "core/sim_state.h"

namespace xn {

/// What the crew wants of the doors.
enum class DoorPlan {
  Leave,        ///< Nothing: the doors are the user's or the aircraft's.
  Close,        ///< Every passenger, service and cargo door closed.
  OpenForward,  ///< The forward left door open, for deboarding.
};

/// What the crew wants of the cabin lights. The aircraft maps each to its own
/// settings (on the 737: white bright, white medium, takeoff and landing, and
/// the night cruise preset chosen in its settings).
enum class CabinLight {
  Leave,           ///< Nothing.
  Bright,          ///< Full white light (Fenix BRT).
  Medium,          ///< White medium: a cabin service in darkness.
  TakeoffLanding,  ///< Dimmed for takeoff and landing in darkness (DIM 2).
  NightCruise,     ///< The night cruise preset (DIM 1 by default in Fenix).
};

/// The crew's plan for one tick.
struct CabinPlan {
  DoorPlan doors = DoorPlan::Leave;
  CabinLight light = CabinLight::Leave;
};

/// "leave", "close", "open forward".
const char* doorPlanName(DoorPlan p);
/// "leave", "bright", "medium", "takeoff and landing", "night cruise".
const char* cabinLightName(CabinLight l);

/// Turns the purser's stage into the crew's plan (VP_SPEC § 4.7). Main thread
/// or tests; no X-Plane.
class CabinCrew {
 public:
  /// The forward door opens this long after DisarmDoors: a random time in
  /// this range, rolled once per arrival (the crew's cross-check).
  static constexpr double kOpenDelayMin = 20, kOpenDelayMax = 40;
  /// Darkness changes only when the sun is this far past the setting
  /// (D-15), so the lights don't flicker at sunset.
  static constexpr float kDarkHysteresisDeg = 0.5f;

  explicit CabinCrew(unsigned seed = std::random_device{}());
  /// A new flight, or the plugin starts over.
  void reset();
  CabinPlan plan(const CabinStage& c, const SimState& s, const Settings& set);

  /// True while the crew counts it as dark (the sun below the setting).
  bool dark() const { return dark_; }

 private:
  DoorPlan doors(const CabinStage& c, const SimState& s);
  CabinLight light(const CabinStage& c) const;

  std::mt19937 rng_;
  bool darkKnown_ = false;
  bool dark_ = false;
  /// The DisarmDoors time the open delay was rolled for, and the delay.
  double delayFor_ = -1;
  double openDelay_ = kOpenDelayMin;
};

}  // namespace xn
