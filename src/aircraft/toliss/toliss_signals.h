/// @file
/// How raw ToLiss dataref values become the aircraft-neutral signals of
/// `SimState`.
///
/// These are pure functions, shared by the live module and by the upgrade of
/// traces recorded before 0.3.5, so a replayed flight is decoded exactly like
/// a live one. The encodings and how each was verified are in
/// docs/aircraft/toliss.md.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "aircraft/toliss/toliss_options.h"
#include "core/recorder.h"
#include "core/sim_state.h"

namespace xn::toliss {

/// `AirbusFBW/APPhase`: 0 PreFlight to 7 Done, the same order as `Phase`.
/// Anything else gives no phase.
std::optional<Phase> fmgsPhase(int raw);

/// Ground spoilers from `AirbusFBW/SDSpoilerArray`, which holds the ECAM
/// status of each panel: 0 retracted, 1 extended, 2 no hydraulic pressure.
/// Entries 0 and 1 are spoiler 1 left and right, which only the ground
/// spoiler function moves. Returns 1 out, 0 in, -1 unknown.
int groundSpoilersFromSd(const std::vector<float>& sd);

/// Whether the switch at `index` of a switch-position array is at
/// `position`: 1 yes, 0 no, -1 when the array has no such entry. Used for
/// the strobe switch in `AirbusFBW/OHPLightSwitches` (0 off, 1 auto, 2 on).
int switchAt(const std::vector<float>& switches, int index, int position);

/// Volume of an ACP knob, 0 to 1. The cockpit knobs report their rotation in
/// degrees (0 to 270); a value up to 1.5 is taken as a ratio already.
float acpKnobVolume(float raw);

/// "No thrust lever value": what the highest lever position is set to when
/// the dataref is missing.
constexpr float kUnknownLever = -9.0f;

/// The cues the purser cross-checks (VP_SPEC § 4.2).
/// @param ecamPhase `AirbusFBW/ECAMFlightPhase`, or -1.
/// @param qpacPhase `AirbusFBW/QPACFlightPhase`, or -1.
/// @param throttleMax Highest entry of `AirbusFBW/throttle_input`, or
///        `kUnknownLever`.
/// @param flxDetent Lever position that counts as takeoff thrust.
AircraftCues cuesFrom(int ecamPhase, int qpacPhase, float throttleMax,
                      float flxDetent);

/// `SimState::aircraftVolume`: ToLiss's internal volume when the ISCS
/// "Ovrd XP Int/Ext Vol" switch is on, else -1.
/// @param internal `AirbusFBW/fmod/vol/Internal` (0 to 1), or -1.
/// @param overrideOn The override switch: 1 on, 0 off, -1 unknown.
float volumeFactor(float internal, int overrideOn);

/// Upgrades the rows of a trace recorded before 0.3.5 to today's fields.
///
/// Those traces carry ToLiss columns (`ecam_phase`, `qpac_phase`,
/// `throttle_max`, `toliss_vol_internal`, `toliss_vol_override`) and their
/// raw FMGS phase. Their oldest ones lack the strobe, NO SMOKING and gear
/// columns, and 0.2.1 decoded the ground spoilers wrongly; those values are
/// recovered from the "raw ..." event rows the recorder also wrote. Feed it
/// every row in file order.
class LegacyTrace {
 public:
  explicit LegacyTrace(const Options& o = Options()) : options_(o) {}
  void apply(TraceRow& row);

 private:
  Options options_;
  int rawStrobe_ = -1, rawNoSmoking_ = -1, rawGear_ = -1;
  /// -2 until an SDSpoilerArray event was seen.
  int rawSpoilers_ = -2;
};

}  // namespace xn::toliss
