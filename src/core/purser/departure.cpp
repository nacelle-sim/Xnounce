/// @file
/// From the gate to takeoff thrust: boarding complete, ArmDoors, the safety
/// briefing, the cabin check and the calls before takeoff (VP_SPEC § 4.1;
/// D-6, D-22, D-23).
#include <algorithm>

#include "core/log.h"
#include "core/purser/purser.h"
#include "core/purser/purser_internal.h"

namespace xn {

using namespace purser_detail;

void Purser::departure(const SimState& s, const Settings& set) {
  if (f_.takeoff) return;
  // Takeoff thrust (§ 4.1): the FMS takeoff phase while rolling, the thrust
  // levers at takeoff thrust while rolling, the aircraft's takeoff power cue,
  // or engines running above 40 kt.
  std::string why;
  if (s.onGround) {
    const bool rolling = s.gsKt > 5;
    if (s.fmsPhase && *s.fmsPhase == Phase::Takeoff && rolling)
      why = terms().fms + " takeoff phase";
    else if (s.cues.leversAtTakeoff == 1 && rolling && s.enginesRunning > 0)
      why = terms().leversAtTakeoff;
    else if (s.cues.takeoffPower == 1)
      why = terms().takeoffPowerCue;
    else if (s.enginesRunning > 0 && s.gsKt > 40)
      why = "engines running above 40 kt";
  } else if (f_.flown) {
    why = "airborne";
  }
  if (!why.empty()) {
    f_.takeoff = true;
    boardingNote_.clear();
    note(s.t, "takeoff thrust (" + why + ")");
    // D-6: takeoff thrust stops what plays.
    if (current_) {
      if (call_.active && !call_.talking) {
        finishCurrent(Outcome::Expired, "takeoff");
      } else {
        out_.stop();
        finishCurrent(Outcome::Stopped, "takeoff thrust");
      }
    }
    dropWhere(ExpireAtTakeoff, Outcome::Expired);
    // D-23: no line-up signal came before takeoff.
    if (f_.takeoffSecuredQueued && !f_.takeoffCallsQueued)
      log(s.t, AnnType::CrewTakeSeats, CallContext::Takeoff, Outcome::Expired,
          "no line-up signal before takeoff thrust");
    f_.boardingComplete = f_.armDoors = true;
    return;
  }

  const bool atGate = s.onGround && s.enginesRunning == 0 && s.gsKt < 1;
  // § 4.4: after a hot start, shutting the engines down at the gate starts
  // the flight over.
  if (f_.hotStart && !f_.flown && atGate && (s.parkBrake || s.chocks)) {
    freshFlight(s);
    note(s.t, "engines shut down at the gate: boarding can start");
    return;
  }
  // § 4.4: the turnaround gap ends on time, or when a door closes and opens
  // again.
  if (f_.turnaroundAt >= 0) {
    if (s.doorMax < 0.05f) {
      f_.gapSawClosed = true;
    } else if (f_.gapSawClosed && s.doorMax > 0.5f) {
      f_.turnaroundAt = -1;
      note(s.t,
           "a door opened again after the turnaround: the turnaround gap ends");
    }
  }
  if (atGate && s.doorMax > 0.5f) f_.doorSeenOpenAtGate = true;

  // When boarding starts (§ 4.1a, D-18).
  if (!f_.boarding && !f_.boardingComplete && !f_.armDoors) {
    switch (mode_) {
      case BoardingStart::Schedule:
        scheduleBoarding(s, set, atGate);
        break;
      case BoardingStart::Doors:
        doorsBoarding(s, set, atGate);
        break;
      case BoardingStart::Call:
        break;
    }
  }
  if (f_.boarding && !f_.boardingComplete) {
    // A new OFP during boarding re-times its end (§ 4.1b).
    if (mode_ == BoardingStart::Schedule && plan_.valid &&
        plan_.seqno != f_.boardingPlanSeqno) {
      f_.boardingPlanSeqno = plan_.seqno;
      f_.boardingEndAt = std::max(s.t + kRetimeMinS, boardingEnd(s, set));
      note(s.t, "new SimBrief plan during boarding: boarding completes in " +
                    mmss(f_.boardingEndAt - s.t));
    }
    if (s.t >= f_.nextWelcome) {
      const bool already =
          std::any_of(queue_.begin(), queue_.end(),
                      [](const Item& i) {
                        return i.type == AnnType::BoardingWelcome;
                      }) ||
          (current_ && current_->type == AnnType::BoardingWelcome);
      if (!already)
        enqueue(AnnType::BoardingWelcome, CallContext::Any, ExpireAtTakeoff);
      f_.nextWelcome = s.t + randIn(set.welcomeMin, set.welcomeMax);
    }
    // D-1: door events never complete boarding; aircraft close their doors
    // on their own timers.
    const char* why = nullptr;
    if (f_.forceBoardingComplete)
      why = "advanced";
    else if (s.t >= f_.boardingEndAt)
      why = "boarding time up";
    else if (s.beacon)
      why = "beacon on";
    else if (s.gsKt > 1)
      why = "pushback";
    else if (s.enginesRunning > 0)
      why = "engine running";
    else if (s.slidesKnown && s.slidesArmed && !f_.slidesWere)
      why = "slides armed";
    if (why) {
      note(s.t, std::string("boarding complete (") + why + ")");
      if (!f_.clockMismatch) boardingNote_.clear();
      f_.boardingComplete = true;
      f_.forceBoardingComplete = false;
      queue_.erase(std::remove_if(queue_.begin(), queue_.end(),
                                  [](const Item& i) {
                                    return i.type == AnnType::BoardingWelcome;
                                  }),
                   queue_.end());
      enqueue(AnnType::BoardingComplete, CallContext::Any, ExpireAtTakeoff);
    }
  }

  // ArmDoors on the beacon, the pushback, an engine start or the slides
  // armed by the user.
  if (!f_.armDoors) {
    const bool slidesRising = s.slidesKnown && s.slidesArmed && !f_.slidesWere;
    if (s.beacon || s.gsKt > 1 || s.enginesRunning > 0 || slidesRising) {
      if (!f_.boarding) f_.boardingComplete = true;
      if (f_.armWaitSince < 0) f_.armWaitSince = s.t;
      if (s.doorMax < 0.05f || s.t - f_.armWaitSince >= set.armDoorsWait ||
          s.gsKt > 1) {
        f_.armDoors = true;
        enqueue(AnnType::ArmDoors, CallContext::Any, ExpireAtTakeoff);
      }
    }
  }

  // D-22: the first movement once boarding is complete times the briefing.
  if (f_.boardingComplete && f_.movedAt < 0 && s.onGround && s.gsKt > 1) {
    f_.movedAt = s.t;
    f_.safetyAfterMove =
        randIn(set.preSafetyAfterMoveMin, set.preSafetyAfterMoveMax);
    logInfo(
        "purser: the aircraft is moving: the safety briefing follows in about "
        "%.0f s",
        f_.safetyAfterMove);
  }
  if (f_.armDoors && !f_.taxied && s.onGround && s.enginesRunning > 0 &&
      s.gsKt > kTaxiKt) {
    f_.taxied = true;
    logInfo("purser: taxiing (a cabin call is now the line-up signal)");
  }

  if (f_.armDoorsFinished && !f_.safetyQueued && f_.movedAt >= 0 &&
      s.t >= safetyDue()) {
    f_.safetyQueued = true;
    enqueue(AnnType::PreSafetyBriefing, CallContext::Any, ExpireAtTakeoff);
    enqueue(AnnType::SafetyBriefing, CallContext::Any, ExpireAtTakeoff);
  }

  // D-23: the light signals count while they are on. The third signal, a
  // cabin call once taxied, is handled in onCabinCall.
  if (f_.armDoors && !f_.lineup && !f_.takeoffCallsQueued && s.onGround) {
    if (s.landingLights)
      signalLineup(s, set, "landing lights ON");
    else if (s.strobeOn == 1)
      signalLineup(s, set, "strobe ON");
  }

  // The chain: briefing, cabin check, CabinSecured while taxiing, then at
  // line-up CrewTakeSeats and CabinDim.
  if (f_.safetyFinished && f_.takeoffCheckDone < 0) {
    const double d = checkDuration(s.pax, set);
    f_.takeoffCheckDone = s.t + (f_.expediteTakeoffCheck
                                     ? std::min<double>(d, set.expediteSeconds)
                                     : d);
  }
  if (f_.takeoffCheckDone >= 0 && !f_.takeoffSecuredQueued &&
      s.t >= f_.takeoffCheckDone) {
    f_.takeoffSecuredQueued = true;
    enqueue(AnnType::CabinSecured, CallContext::Takeoff, ExpireAtTakeoff);
  }
  if (f_.takeoffSecuredQueued && f_.lineup && !f_.takeoffCallsQueued) {
    f_.takeoffCallsQueued = true;
    enqueue(AnnType::CrewTakeSeats, CallContext::Takeoff, ExpireAtTakeoff);
    if (dark(s, set))
      enqueue(AnnType::CabinDim, CallContext::Takeoff, ExpireAtTakeoff);
  }
}

}  // namespace xn
