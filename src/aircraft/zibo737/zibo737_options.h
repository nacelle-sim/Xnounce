/// @file
/// Settings that only the Zibo / LevelUp 737 has.
#pragma once

#include <string>
#include <vector>

#include "core/aircraft_type.h"
#include "core/settings.h"

namespace xn::zibo737 {

/// The `[zibo737]` section of `Xnounce.ini` and the 737 rows of `[boarding]`.
struct Options {
  /// The camera is in the cabin when it is this far aft of the eye point.
  float cabinViewAftM = 2.5f;
  /// A thrust lever at or above this ratio counts as takeoff thrust.
  float takeoffLever = 0.85f;

  /// Cabin actions (VP_SPEC § 4.7), each off by default. While any is on, the
  /// aircraft's own crew announcements are switched off as far as it allows.
  /// The crew closes the doors at BoardingComplete and opens L1 after
  /// DisarmDoors.
  bool actDoors = false;
  /// The crew sets the cabin lights through the flight.
  bool actLights = false;
  /// The cabin lights in darkness between takeoff and landing: 0 Cruise,
  /// 1 Night, 2 Sleep.
  int nightCruiseLights = 1;
  /// The cabin's call to the cockpit sounds the aircraft's own chime (Zibo).
  bool actChime = false;

  /// Boarding lead and default seats per model (VP_SPEC § 4.1a). The seats
  /// are used only when neither the SimBrief plan nor the aircraft's cabin
  /// layout gives them.
  BoardingLead b736{26, 31, 120};
  BoardingLead b737{28, 34, 140};
  BoardingLead b738{30, 37, 172};
  BoardingLead b739{32, 39, 189};
};

/// The definitions of these settings, bound to `o`.
std::vector<SettingDef> settingDefs(Options& o);
/// Repairs values that contradict each other after loading.
void normalize(Options& o);

}  // namespace xn::zibo737
