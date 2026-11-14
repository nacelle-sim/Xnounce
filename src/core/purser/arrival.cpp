/// @file
/// From touchdown to the end of the flight: AfterLanding, DisarmDoors,
/// deboarding and the turnaround (VP_SPEC § 4.1, § 4.4; D-26).
#include <algorithm>

#include "core/log.h"
#include "core/purser/purser.h"
#include "core/purser/purser_internal.h"

namespace xn {

using namespace purser_detail;

void Purser::arrival(const SimState& s, const Settings& set) {
  if (!f_.flown) return;
  // Touchdown (VP_SPEC § 4.1): on the ground after the flight, or the
  // aircraft's touchdown cue below 50 ft.
  const bool down = s.onGround || (s.cues.touchdown == 1 && s.aglFt < 50);
  if (!down && s.aglFt > 50 && f_.touchdownAt >= 0 && !f_.afterLanding)
    f_.touchdownAt = -1;  // a touch-and-go: airborne again
  if (down && f_.touchdownAt < 0 && !f_.afterLanding) {
    f_.touchdownAt = s.t;
    note(s.t,
         "touchdown (" +
             (s.onGround ? std::string("on ground") : terms().touchdownCue) +
             ")");
    dropWhere(ExpireOnGround | ExpireLandingWindow, Outcome::Expired);
  }
  if (f_.touchdownAt < 0 || !down) return;

  if (!f_.afterLanding) {
    const bool stowed = s.speedbrake <= 0.01f && s.groundSpoilers != 1;
    const bool normal = stowed && !s.landingLights && s.gsKt < 40;
    const bool fallback =
        s.t - f_.touchdownAt >= set.afterLandingFallback && s.gsKt < 30;
    if (normal || fallback) {
      f_.afterLanding = true;
      enqueue(AnnType::AfterLanding);
    }
  }
  if (!f_.disarm) {
    const bool slidesFalling = s.slidesKnown && !s.slidesArmed && f_.slidesWere;
    const bool atStand =
        s.gsKt < 1 && (s.parkBrake || s.chocks) && (!s.beacon || !s.seatbelt);
    if (atStand || slidesFalling) {
      if (!f_.afterLanding) {
        f_.afterLanding = true;
        enqueue(AnnType::AfterLanding);
      }
      f_.disarm = true;
      f_.disarmAt = s.t;
      enqueue(AnnType::DisarmDoors);
    }
  }
  // § 4.1c: the aircraft's turnaround service starts the deboarding when it
  // runs; its doors take a while, so the fallback waits for it.
  if (f_.disarm && !f_.deboarding &&
      (s.doorFwd > 0.5f || s.doorMax > 0.5f || s.svc.deboardingStarted ||
       (!s.svc.running && s.t - f_.disarmAt >= set.deboardFallback))) {
    f_.deboarding = true;
    f_.deboardFollowed = s.svc.running || s.svc.deboardingStarted;
    f_.deboardAt = s.t;
    f_.deboardDone =
        s.t + std::max<double>(set.deboardMin,
                               s.pax * set.deboardPerPax * type_.typeFactor);
    enqueue(AnnType::DeboardingStarted);
  }
  if (f_.deboarding && !f_.deboardComplete) {
    // Followed: when the service is done, or by the model if it stopped
    // without finishing.
    const bool followed =
        f_.deboardFollowed && (s.svc.running || s.svc.deboardingComplete);
    if (followed ? s.svc.deboardingComplete : s.t >= f_.deboardDone) {
      f_.deboardComplete = true;
      enqueue(AnnType::DeboardingComplete);
    }
  }
  if (f_.deboardComplete && !f_.flightComplete && queue_.empty() && !current_) {
    if (!set.turnaround) {
      // D-26: with the turnaround off the flight ends here.
      f_.flightComplete = true;
      boardingNote_.clear();
      note(s.t,
           "flight complete: a new SimBrief OFP from this airport starts the "
           "next flight (Turnaround is off)");
      return;
    }
    // The turnaround: a new flight whose boarding starts after the gap, or
    // when a door closes and opens again.
    logInfo(
        "purser: turnaround, next boarding in %.0f s or when a door closes and "
        "opens again",
        static_cast<double>(set.turnaroundGap));
    freshFlight(s);
    f_.turnaroundAt = s.t + set.turnaroundGap;
  }
}

}  // namespace xn
