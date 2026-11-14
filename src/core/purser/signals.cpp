/// @file
/// The pilots' signals to the cabin: the cabin call buttons, the NO SMOKING
/// flick, the line-up and seats-for-landing signals (VP_SPEC § 4.2, § 4.3a;
/// D-21, D-23, D-24, D-25).
#include <algorithm>

#include "core/log.h"
#include "core/purser/purser.h"
#include "core/purser/purser_internal.h"

namespace xn {

using namespace purser_detail;

void Purser::signalLineup(const SimState& s, const Settings& set,
                          const std::string& why) {
  f_.lineup = true;
  std::string text = "line-up signal (" + why + ")";
  // D-23: a signal before the cabin check ends hurries it.
  if (!f_.takeoffSecuredQueued) {
    f_.expediteTakeoffCheck = true;
    if (f_.takeoffCheckDone >= 0)
      f_.takeoffCheckDone =
          std::min<double>(f_.takeoffCheckDone, s.t + set.expediteSeconds);
    text += ": the cabin check is hurried";
  }
  note(s.t, text);
}

void Purser::signalSeatsLanding(const SimState& s, const Settings& set,
                                const std::string& why) {
  f_.seatsLanding = true;
  std::string text = "seats-for-landing signal (" + why + ")";
  if (!f_.landingSecuredQueued) {
    f_.expediteLandingCheck = true;
    if (f_.landingCheckDone >= 0)
      f_.landingCheckDone =
          std::min<double>(f_.landingCheckDone, s.t + set.expediteSeconds);
    text += ": the cabin check is hurried";
  }
  note(s.t, text);
}

void Purser::signalAfterTakeoff(const SimState& s, const std::string& why) {
  // D-25: "crew may leave their seats" plays AfterTakeoff now.
  f_.climbEntered = true;
  f_.afterTakeoffDue = s.t;
  note(s.t, "crew may leave their seats (" + why + "): AfterTakeoff now");
}

bool Purser::detectFlick(const SimState& s, const Settings& set) {
  if (s.noSmoking < 0) {
    f_.nsLast = -1;
    f_.nsOffAt = -1;
    return false;
  }
  bool flick = false;
  if (f_.nsLast == 1 && s.noSmoking == 0) {
    f_.nsOffAt = s.t;
  } else if (f_.nsLast == 0 && s.noSmoking == 1) {
    flick = f_.nsOffAt >= 0 && s.t - f_.nsOffAt <= set.nsFlickMaxS;
    f_.nsOffAt = -1;
  }
  f_.nsLast = s.noSmoking;
  return flick;
}

void Purser::onCabinCall(const SimState& s, const Settings& set) {
  // VP_SPEC § 4.3a: what a cabin call does depends on the stage.
  const bool atGate = s.onGround && s.enginesRunning == 0 && s.gsKt < 1;
  // § 4.1c: the aircraft's turnaround service runs the boarding.
  if (s.svc.running && !f_.takeoff && !f_.boardingComplete && !f_.armDoors) {
    logInfo(
        "purser: cabin call: the aircraft's turnaround service runs the "
        "boarding");
    return;
  }
  if (!f_.takeoff && !f_.boarding && !f_.boardingComplete && !f_.armDoors &&
      atGate) {
    if (!s.acPower) {
      logInfo("purser: cabin call ignored: no AC power in the cabin");
      return;
    }
    startBoarding(s, set, "cabin call");
    return;
  }
  if (f_.hotStart && f_.movedAt < 0 && s.onGround) {
    logInfo(
        "purser: cabin call: started with engines running; shut the engines "
        "down at the gate to board");
    return;
  }
  if (!f_.takeoff && f_.boarding && !f_.boardingComplete) {
    // D-21: a call during boarding completes it soon.
    const double by = s.t + set.boardingExpediteS;
    if (by < f_.boardingEndAt) {
      f_.boardingEndAt = by;
      note(s.t, "cabin call: boarding completes in " + mmss(by - s.t));
    } else {
      logInfo("purser: cabin call: boarding completes in %s anyway",
              mmss(f_.boardingEndAt - s.t).c_str());
    }
    return;
  }
  const double quick = s.t + set.expediteSeconds;
  if (!f_.takeoff && f_.armDoors && !f_.takeoffCallsQueued) {
    // Once taxied it is the line-up signal (D-23); before that it only
    // hurries the cabin check, as in Fenix.
    if (f_.taxied) {
      if (!f_.lineup)
        signalLineup(s, set, "cabin call while taxiing");
      else
        logInfo("purser: cabin call: the line-up signal was already given");
      return;
    }
    if (f_.takeoffSecuredQueued) {
      logInfo(
          "purser: cabin call: the cabin is secured; a call once taxiing is "
          "the line-up signal");
    } else if (f_.takeoffCheckDone >= 0) {
      f_.takeoffCheckDone = std::min(f_.takeoffCheckDone, quick);
      logInfo("purser: cabin call: cabin check before takeoff expedited");
    } else {
      f_.expediteTakeoffCheck = true;
      logInfo(
          "purser: cabin call: the cabin check after the safety briefing will "
          "be quick");
    }
    return;
  }
  if (f_.flown && !s.onGround && !f_.afterTakeoff && !f_.descentEntered) {
    signalAfterTakeoff(s, "cabin call");
    return;
  }
  if (f_.flown && f_.descentEntered && !f_.landingSecuredQueued) {
    if (f_.landingCheckDone >= 0) {
      f_.landingCheckDone = std::min(f_.landingCheckDone, quick);
      logInfo("purser: cabin call: cabin check before landing expedited");
    } else {
      f_.expediteLandingCheck = true;
      logInfo(
          "purser: cabin call: the cabin check before landing will be quick");
    }
    return;
  }
  logInfo("purser: cabin call: nothing to start or expedite now");
}

}  // namespace xn
