/// @file
/// From takeoff to the landing calls: AfterTakeoff, the seatbelt sign,
/// CabinService, DescentStarted and the calls before landing (VP_SPEC § 4.1;
/// D-24, D-25).
#include <cmath>

#include "core/log.h"
#include "core/purser/purser.h"
#include "core/purser/purser_internal.h"

namespace xn {

using namespace purser_detail;

void Purser::flight(const SimState& s, const Settings& set) {
  if (!f_.takeoff) return;
  const bool airborne = !s.onGround;

  // D-25: a flick in the climb means "crew may leave their seats". In the
  // descent it is the seats signal instead.
  const bool climbFlick =
      flick_ && airborne && f_.flown && !f_.afterTakeoff && !f_.descentEntered;
  if (climbFlick) signalAfterTakeoff(s, "NO SMOKING flick");
  if (!f_.climbEntered && airborne &&
      (phase_ == Phase::Climb || phase_ == Phase::Cruise ||
       (estimating_ && s.aglFt > 1500))) {
    f_.climbEntered = true;
    f_.afterTakeoffDue = s.t + randIn(set.afterTakeoffMin, set.afterTakeoffMax);
  }
  if (f_.afterTakeoffDue >= 0 && !f_.afterTakeoff &&
      s.t >= f_.afterTakeoffDue) {
    f_.afterTakeoff = true;
    enqueue(AnnType::AfterTakeoff, CallContext::Any, ExpireOnGround);
  }

  if (airborne && f_.flown) {
    if (s.seatbelt != f_.beltState) {
      if (!f_.beltPending) {
        f_.beltPending = true;
        f_.beltChangedAt = s.t;
      }
      if (s.t - f_.beltChangedAt >= set.seatbeltDebounce) {
        f_.beltState = s.seatbelt;
        f_.beltPending = false;
        const double after = f_.afterTakeoff ? 0
                                             : (f_.afterTakeoffDue >= 0
                                                    ? f_.afterTakeoffDue + 0.1
                                                    : s.t + 60);
        if (s.seatbelt) {
          const bool nearDescent =
              atLeast(phase_, Phase::Descent) ||
              (f_.descentAt >= 0 &&
               std::fabs(s.t - f_.descentAt) < kSeatbeltDescentWindow);
          if (!nearDescent)
            enqueue(AnnType::SeatbeltSignOn, CallContext::Any, ExpireOnGround,
                    after);
          else
            logDebug(
                "purser: seatbelt sign on near the descent: covered by "
                "DescentStarted");
        } else {
          enqueue(AnnType::SeatbeltSignOff, CallContext::Any, ExpireOnGround,
                  after);
        }
      }
    } else {
      f_.beltPending = false;
    }
  } else {
    f_.beltState = s.seatbelt;
    f_.beltPending = false;
  }

  if (!f_.cruiseEntered && airborne && phase_ == Phase::Cruise) {
    f_.cruiseEntered = true;
    f_.cabinServiceDue = s.t + randIn(set.cabinServiceMin, set.cabinServiceMax);
  }
  if (f_.cabinServiceDue >= 0 && !f_.cabinService &&
      s.t >= f_.cabinServiceDue) {
    f_.cabinService = true;
    if (phase_ == Phase::Cruise && !f_.decompression) {
      f_.cabinServiceAt = s.t;
      enqueue(AnnType::CabinService, CallContext::Any, ExpireOnGround);
    } else
      log(s.t, AnnType::CabinService, CallContext::Any, Outcome::Skipped,
          "no longer in cruise");
  }

  if (!f_.descentEntered && airborne &&
      (phase_ == Phase::Descent || phase_ == Phase::Approach)) {
    f_.descentEntered = true;
    f_.descentAt = s.t;
    f_.descentDue = s.t + randIn(set.descentMin, set.descentMax);
    if (!f_.cabinService && f_.cabinServiceDue >= 0) {
      f_.cabinService = true;
      log(s.t, AnnType::CabinService, CallContext::Any, Outcome::Skipped,
          "descent started first");
    }
  }
  if (f_.descentDue >= 0 && !f_.descentStarted && s.t >= f_.descentDue) {
    f_.descentStarted = true;
    enqueue(AnnType::DescentStarted, CallContext::Any, ExpireOnGround);
  }

  if (f_.descentFinished && f_.landingCheckDone < 0) {
    const double d = checkDuration(s.pax, set);
    f_.landingCheckDone = s.t + (f_.expediteLandingCheck
                                     ? std::min<double>(d, set.expediteSeconds)
                                     : d);
  }
  // D-24: the landing CabinSecured comes in the approach window.
  const bool window =
      airborne &&
      (phase_ == Phase::Approach || s.aglFt < set.landingReadyAglFt ||
       f_.forceLandingCalls) &&
      s.aglFt > set.landingDropAglFt;
  if (f_.landingCheckDone >= 0 && !f_.landingSecuredQueued &&
      s.t >= f_.landingCheckDone && window) {
    f_.landingSecuredQueued = true;
    f_.forceLandingCalls = false;
    enqueue(AnnType::CabinSecured, CallContext::Landing, ExpireLandingWindow);
  }
  if (f_.descentEntered && airborne && !f_.seatsLanding &&
      !f_.landingCallsQueued) {
    if (flick_)
      signalSeatsLanding(s, set, "NO SMOKING flick");
    else if (s.gearLeverDown == 1)
      signalSeatsLanding(s, set, "gear down");
  } else if (flick_ && !climbFlick && airborne && f_.flown &&
             !f_.descentEntered) {
    logInfo("purser: NO SMOKING flick in cruise: nothing to signal");
  }
  if (f_.landingSecuredQueued && f_.seatsLanding && !f_.landingCallsQueued &&
      airborne && s.aglFt > set.landingDropAglFt) {
    f_.landingCallsQueued = true;
    enqueue(AnnType::CrewTakeSeats, CallContext::Landing, ExpireLandingWindow);
    if (dark(s, set))
      enqueue(AnnType::CabinDim, CallContext::Landing, ExpireLandingWindow);
  }

  if (phase_ == Phase::GA && prevPhase_ != Phase::GA) {
    logInfo("purser: go-around: pending landing calls dropped");
    dropWhere(ExpireLandingWindow, Outcome::Expired);
    // Nothing repeats after a go-around (§ 4.1).
    if (f_.landingSecuredQueued) f_.landingCallsQueued = true;
  }
}

}  // namespace xn
