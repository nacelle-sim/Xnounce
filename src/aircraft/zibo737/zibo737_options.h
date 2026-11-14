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
  /// The camera is in the cabin when it is this far aft of the eye point:
  /// the cockpit's rear wall is about 1.2 m behind it (the LevelUp's sound
  /// spaces, docs/aircraft/zibo737.md § 3).
  float cockpitDepthM = 1.2f;
  /// A thrust lever at or above this ratio counts as takeoff thrust.
  float takeoffLever = 0.85f;

  /// Cabin actions (VP_SPEC § 4.7), each off by default. The crew closes the
  /// doors at BoardingComplete and opens L1 after DisarmDoors.
  bool actDoors = false;
  /// The crew sets the cabin lights through the flight.
  bool actLights = false;
  /// The cabin lights in darkness between takeoff and landing: 0 Cruise,
  /// 1 Night, 2 Sleep.
  int nightCruiseLights = 1;
  /// The cabin's call to the cockpit sounds the aircraft's own chime (Zibo).
  bool actChime = false;
  /// Silence the aircraft's own cabin crew announcements (VP_SPEC § 4.7,
  /// D-32). The only action that is on by default; the captain's messages
  /// still play.
  bool silenceAircraftPa = true;

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
