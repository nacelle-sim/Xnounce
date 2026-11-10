/// @file
/// Settings that only the ToLiss Airbus has.
#pragma once

#include <string>
#include <vector>

#include "core/aircraft_type.h"
#include "core/settings.h"

namespace xn::toliss {

/// The `[toliss]` section of `Xnounce.ini` and the Airbus rows of
/// `[boarding]`. Which index or dataref each value selects is listed in
/// docs/aircraft/toliss.md.
struct Options {
  /// Index of the PA reception knob in the ACP arrays.
  int acpPaIndex = 15;
  /// Index of the CAB (interphone) reception knob.
  int acpCabIndex = 6;
  /// The camera is in the cabin when it is this far aft of the eye point.
  float cabinViewAftM = 2.5f;
  /// A thrust lever at or above this is takeoff thrust (0.88 is FLX/MCT).
  float throttleFlxDetent = 0.85f;
  /// Which dataref says the landing lights are on: 0 `landing_lights_on`,
  /// 1 `landing_lights_switch`, 2 ToLiss left/right extended.
  int landingLightSource = 0;
  /// Entry of the strobe switch in `AirbusFBW/OHPLightSwitches`.
  int strobeSwitchIndex = 7;
  /// Dataref of the ISCS "Ovrd XP Int/Ext Vol" switch. Empty: not known.
  std::string volumeOverrideRef;

  /// Boarding lead and default seats per type (VP_SPEC § 4.1a).
  BoardingLead a319{30, 34, 144};
  BoardingLead a320{30, 37, 180};
  BoardingLead a321{34, 40, 220};
};

/// The definitions of these settings, bound to `o`.
std::vector<SettingDef> settingDefs(Options& o);
/// Repairs values that contradict each other after loading.
void normalize(Options& o);
/// Replaces defaults that an older version got wrong, in a file that still
/// has them. Returns a line for the log for each value it changed.
std::vector<std::string> migrate(const Ini& ini, Options& o);

}  // namespace xn::toliss
