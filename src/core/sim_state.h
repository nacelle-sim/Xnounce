/// @file
/// The snapshot of the simulator that the purser sees.
///
/// Every tick the plugin fills one `SimState`: the standard X-Plane values
/// first (`aircraft/xp_common`), then the loaded aircraft's module adds what
/// only that aircraft knows. The purser, the hearing model and the recorder
/// read nothing else, so a recorded trace can be replayed through the same
/// code in the tests (docs/ARCHITECTURE.md § 2).
///
/// Nothing here names an aircraft. A value an aircraft cannot provide stays
/// at its "unknown" default and only the features that need it are off.
#pragma once

#include <optional>
#include <string>

#include "core/announcement.h"

namespace xn {

/// What the aircraft's own systems say about the flight.
///
/// These back up the purser's own detection of takeoff, lift-off and touchdown
/// (VP_SPEC § 4.1, § 4.2). An integer cue is 1 when the aircraft says yes,
/// 0 when it says no and -1 when the aircraft has no such signal.
struct AircraftCues {
  /// Takeoff power is set, or the takeoff roll has passed 80 kt.
  int takeoffPower = -1;
  /// A thrust lever is at or above the takeoff detent.
  int leversAtTakeoff = -1;
  /// The aircraft is airborne after takeoff and below the climb.
  int liftoff = -1;
  /// The aircraft has touched down or is rolling out.
  int touchdown = -1;
  /// The flight phase to use when the FMS gives none (D-20). Empty when the
  /// aircraft has no opinion right now.
  std::optional<Phase> phaseHint;
  /// The aircraft has a flight-phase computer of its own, so the purser's
  /// altitude rule for "just took off" is not needed.
  bool phaseKnown = false;
  /// Names the lift-off cue in the log, for example "ECAM flight phase 5".
  std::string liftoffLabel;
};

/// One tick of simulator state.
struct SimState {
  /// The purser clock in seconds. It stops while the sim is paused.
  double t = 0;
  bool paused = false;
  bool replay = false;

  bool onGround = true;
  float gsKt = 0, iasKt = 0, aglFt = 0, mslFt = 0, vsFpm = 0;
  double lat = 0, lon = 0;

  /// Flight phase from the aircraft's FMS. Empty when the aircraft has none
  /// or the value is out of range.
  std::optional<Phase> fmsPhase;
  /// The FMS phase as the aircraft encodes it, for the log and the trace.
  int fmsPhaseRaw = -1;
  /// Cruise altitude entered in the FMS, or 0.
  float cruiseAltFt = 0;

  bool beacon = false;
  bool seatbelt = false;
  /// Landing lights on, not merely extended.
  bool landingLights = false;
  /// Strobe switch at ON: 1 yes, 0 no, -1 unknown. AUTO does not count.
  int strobeOn = -1;
  /// NO SMOKING signs: 1 lit, 0 unlit, -1 unknown.
  int noSmoking = -1;
  /// Speedbrake lever: at most 0.01 is stowed, negative is armed.
  float speedbrake = 0;
  bool parkBrake = false;
  bool chocks = false;
  int enginesRunning = 0;
  int engineCount = 2;

  /// The widest open passenger door, 0 closed to 1 open.
  float doorMax = 0;
  /// The forward left passenger door, 0 closed to 1 open.
  float doorFwd = 0;
  int doorCount = 0;
  bool slidesArmed = false;
  /// False when the aircraft doesn't report its slides.
  bool slidesKnown = false;

  /// Passengers on board, or 0 when the aircraft doesn't say.
  int pax = 0;
  float fuelKg = 0;
  bool acPower = true;
  /// Which source powers the AC buses, as an aircraft-specific id (GPU, APU,
  /// generator...), or -1 when the aircraft doesn't say. Only the PA's power
  /// cut-out reads it; the purser never does.
  int acSource = -1;
  bool paxMasks = false;
  float cabinAltFt = 0;

  /// The cockpit is listening to the cabin PA (VP_SPEC § 4.5), and how loud.
  bool paListen = false;
  float paListenVol = 0;
  /// The cockpit has taken the cabin interphone (VP_SPEC § 4.3), and how
  /// loud.
  bool interphoneOn = false;
  float interphoneVol = 0;
  /// Cockpit door, 0 closed to 1 fully open.
  float cockpitDoor = 0;

  bool viewExternal = false;
  bool cameraInCabin = false;
  float sunPitchDeg = 45;
  float localTimeSec = 43200;

  /// Flight number entered in the aircraft, for example "DLH400".
  std::string flightNumber;
  /// ICAO type designator X-Plane reports for the aircraft.
  std::string acfIcao;
  /// A satellite antenna is fitted (D-5).
  bool satcom = false;

  /// Cross-checks from the aircraft's own systems.
  AircraftCues cues;
  /// Ground spoilers: 1 out, 0 in, -1 unknown.
  int groundSpoilers = -1;
  /// Gear lever: 1 down, 0 up, -1 unknown.
  int gearLeverDown = -1;
  /// UTC seconds since midnight, or -1.
  double zuluSec = -1;

  /// The aircraft's own interior volume, 0 to 1, when it asks other sounds to
  /// follow it. -1 when it has none or doesn't ask (VP_SPEC § 4.5).
  float aircraftVolume = -1;
  /// The aircraft's own cockpit-door sound blend, 0 closed to 1 open, or -1
  /// when it has none (VP_SPEC § 4.5).
  float doorBlend = -1;
};

}  // namespace xn
