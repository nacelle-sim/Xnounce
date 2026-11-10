/// @file
/// The words an aircraft's cockpit uses for things the purser talks about.
///
/// The purser's logic is the same on every aircraft, but its messages name
/// cockpit controls ("pull CAB on the ACP") and systems ("FMGS"). Those words
/// live here, and each aircraft module fills them in. The defaults are plain
/// descriptions that fit any airliner.
#pragma once

#include <string>

namespace xn {

/// Cockpit wording of one aircraft, used in log lines, the timeline and the
/// "coming up" list.
struct CockpitTerms {
  /// The flight management system whose flight phase the purser follows, as
  /// in "the FMGS phase is still PreFlight" (VP_SPEC § 4.2, D-20).
  std::string fms = "FMS";
  /// What the pilot does to take the cabin's interphone call, as in "cabin
  /// calling: pull CAB on the ACP" (VP_SPEC § 4.3).
  std::string answerInterphone = "answer the cabin interphone";
  /// The short form shown once the call was taken, as in "(CAB answered)".
  std::string interphoneAnswered = "answered";
  /// Where the cockpit's cabin call buttons are, as in "starts on a cabin call
  /// (overhead CALLS: FWD, AFT, ALL...)" (VP_SPEC § 4.3a).
  std::string cabinCallButtons = "the cabin call button";
  /// The takeoff-thrust signal read from the thrust levers (VP_SPEC § 4.1).
  std::string leversAtTakeoff = "thrust levers at takeoff thrust";
  /// The takeoff-thrust signal the aircraft computes itself (VP_SPEC § 4.1).
  std::string takeoffPowerCue = "the aircraft reports takeoff power";
  /// The touchdown signal the aircraft computes itself (VP_SPEC § 4.1).
  std::string touchdownCue = "the aircraft reports touchdown";
};

}  // namespace xn
