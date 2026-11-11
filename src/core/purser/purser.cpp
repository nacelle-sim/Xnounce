/// @file
/// The purser's tick, its queue bookkeeping, the phase estimate (D-20) and
/// working out how far a flight already is when the plugin starts mid-flight
/// (VP_SPEC § 4.4).
#include "core/purser/purser.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "core/log.h"
#include "core/purser/purser_internal.h"

namespace xn {

using namespace purser_detail;

const char* outcomeName(Outcome o) {
  switch (o) {
    case Outcome::Queued:
      return "queued";
    case Outcome::Playing:
      return "playing";
    case Outcome::Played:
      return "played";
    case Outcome::Skipped:
      return "skipped";
    case Outcome::NoSound:
      return "no sound in pack";
    case Outcome::Expired:
      return "expired";
    case Outcome::Disabled:
      return "disabled";
    case Outcome::Stopped:
      return "stopped";
    case Outcome::Unanswered:
      return "call unanswered";
  }
  return "?";
}

Purser::Purser(PurserOutput& out, unsigned seed) : out_(out), rng_(seed) {}

double Purser::randIn(double lo, double hi) {
  if (hi < lo) std::swap(lo, hi);
  std::uniform_real_distribution<double> d(lo, hi);
  return d(rng_);
}

double Purser::checkDuration(int pax, const Settings& set) {
  // § 4.1, D-12: (45 s + 0.35 s per passenger) x type factor x (1 +/- 20 %)
  // x the user's multiplier.
  return (45.0 + 0.35 * std::max(0, pax)) * type_.typeFactor *
         randIn(0.8, 1.2) * std::max(0.1f, set.checkMultiplier);
}

void Purser::reset() {
  if (current_ && !call_.active) out_.stop();
  queue_.clear();
  current_.reset();
  call_ = Call{};
  f_ = Flags{};
  gapUntil_ = 0;
  callPressed_ = false;
  boardingNote_.clear();
  lastZulu_ = lastZuluT_ = -1;
  if (music_) out_.setMusic(false);
  music_ = false;
  acLostAt_ = -1;
}

void Purser::note(double t, const std::string& text) {
  logInfo("purser: %s", text.c_str());
  if (noteSink_) noteSink_(t, text);
}

void Purser::log(double t, AnnType type, CallContext c, Outcome o,
                 std::string detail) {
  TimelineRow r{t, type, c, o, std::move(detail)};
  auto clock = [&](double base) {
    return base < 0 ? -1.0
                    : std::fmod(std::fmod(base + (t - now_), 86400.0) + 86400.0,
                                86400.0);
  };
  r.zuluSec = clock(zulu_);
  r.localSec = clock(local_);
  timeline_.push_back(r);
  while (timeline_.size() > kTimelineRows) timeline_.pop_front();
  logInfo("purser: %s%s%s%s %s%s", annTypeName(type),
          c != CallContext::Any ? " (" : "",
          c != CallContext::Any ? contextName(c) : "",
          c != CallContext::Any ? ")" : "", outcomeName(o),
          r.detail.empty() ? "" : (": " + r.detail).c_str());
  if (sink_) sink_(r);
}

void Purser::enqueue(AnnType t, CallContext c, int expire, double notBefore) {
  queue_.push_back(Item{t, c, expire, notBefore});
}

bool Purser::expired(const Item& it, const SimState& s,
                     const Settings& set) const {
  if ((it.expire & ExpireAtTakeoff) && f_.takeoff) return true;
  if ((it.expire & ExpireOnGround) && s.onGround && f_.flown) return true;
  if ((it.expire & ExpireAirborne) && !s.onGround) return true;
  if ((it.expire & ExpireLandingWindow) &&
      (s.onGround || s.aglFt < set.landingDropAglFt || phase_ == Phase::GA))
    return true;
  return false;
}

void Purser::dropWhere(int mask, Outcome why) {
  for (auto it = queue_.begin(); it != queue_.end();) {
    if (it->expire & mask) {
      log(now_, it->type, it->ctx, why);
      onFinished(*it);
      it = queue_.erase(it);
    } else {
      ++it;
    }
  }
}

void Purser::onFinished(const Item& it) {
  switch (it.type) {
    case AnnType::ArmDoors:
      f_.armDoorsFinished = true;
      f_.armDoorsAt = now_;
      break;
    case AnnType::SafetyBriefing:
      f_.safetyFinished = true;
      break;
    case AnnType::DescentStarted:
      f_.descentFinished = true;
      break;
    default:
      break;
  }
}

void Purser::finishCurrent(Outcome o, std::string detail) {
  if (!current_) return;
  for (auto r = timeline_.rbegin(); r != timeline_.rend(); ++r) {
    if (r->type == current_->type && r->context == current_->ctx &&
        (r->outcome == Outcome::Playing || r->outcome == Outcome::Queued)) {
      r->outcome = o;
      if (!detail.empty()) r->detail = detail;
      if (sink_) sink_(*r);
      break;
    }
  }
  const Item done = *current_;
  current_.reset();
  call_ = Call{};
  onFinished(done);
}

Phase Purser::estimatePhase(const SimState& s) {
  // § 4.2, D-20: no FMS phase, or the FMS was never initialized.
  if (s.onGround) {
    f_.levelSince = -1;
    if (!f_.flown)
      return (s.enginesRunning > 0 && s.gsKt > 40) ? Phase::Takeoff
                                                   : Phase::PreFlight;
    return Phase::Done;
  }
  // The aircraft's own phase hint first, when it gives one.
  if (s.cues.phaseHint) return *s.cues.phaseHint;
  if (!s.cues.phaseKnown && !f_.climbEntered && s.aglFt < 1500 && s.vsFpm > 0)
    return Phase::Takeoff;
  // Cruise: level at the FMS cruise altitude, or without one, level above
  // 10,000 ft for 60 s.
  const bool level = std::fabs(s.vsFpm) < 300;
  if (level) {
    if (f_.levelSince < 0) f_.levelSince = s.t;
  } else {
    f_.levelSince = -1;
  }
  if (s.cruiseAltFt > 1000 && std::fabs(s.mslFt - s.cruiseAltFt) < 1000 &&
      std::fabs(s.vsFpm) < 500)
    return Phase::Cruise;
  if (s.cruiseAltFt <= 1000 && level && s.mslFt > 10000 && f_.levelSince >= 0 &&
      s.t - f_.levelSince >= 60)
    return Phase::Cruise;
  if (s.vsFpm < -500) return s.aglFt < 4000 ? Phase::Approach : Phase::Descent;
  if (s.vsFpm > 300) return Phase::Climb;
  return phase_ == Phase::PreFlight || phase_ == Phase::Done ||
                 phase_ == Phase::Takeoff
             ? Phase::Climb
             : phase_;
}

bool Purser::estimateWanted(const SimState& s, const Settings& set) const {
  if (!s.fmsPhase) return true;
  // An FMS that was never initialized reads PreFlight all flight. The ToLiss
  // also drops from Done to PreFlight at shutdown (Run2), which doesn't
  // count.
  return *s.fmsPhase == Phase::PreFlight && !f_.fmsLeftPreflight && f_.flown &&
         f_.liftoffAt >= 0 && s.t - f_.liftoffAt >= set.fmsEstimateAfterS;
}

void Purser::infer(const SimState& s) {
  f_.inferred = true;
  f_.beltState = s.seatbelt;
  f_.slidesWere = s.slidesArmed;
  const bool airborne = !s.onGround && s.aglFt > 50;
  const Phase p =
      s.fmsPhase ? *s.fmsPhase : (airborne ? Phase::Climb : Phase::PreFlight);
  if (airborne) {
    f_.boardingComplete = f_.armDoors = f_.armDoorsFinished = f_.safetyQueued =
        f_.safetyFinished = true;
    f_.takeoffSecuredQueued = f_.lineup = f_.taxied = true;
    f_.takeoffCallsQueued = f_.takeoff = f_.flown = true;
    f_.takeoffPowerSeen = true;
    f_.liftoffAt = s.t;
    f_.takeoffCheckDone = 0;
    f_.climbEntered = true;
    f_.afterTakeoff = atLeast(p, Phase::Cruise) || s.aglFt > 5000;
    if (atLeast(p, Phase::Cruise)) f_.cruiseEntered = true;
    if (atLeast(p, Phase::Descent)) {
      f_.cabinService = true;
      f_.descentEntered = f_.descentStarted = f_.descentFinished = true;
      f_.descentAt = s.t - 600;
    }
    if (atLeast(p, Phase::Approach)) {
      f_.landingCheckDone = 0;
      f_.landingSecuredQueued = f_.seatsLanding = f_.landingCallsQueued = true;
    }
    logInfo("purser: started airborne (%s): earlier events marked done",
            phaseName(p));
  } else if (s.gsKt > 1) {
    f_.boardingComplete = f_.armDoors = f_.armDoorsFinished = true;
    f_.armDoorsAt = f_.movedAt = s.t;
    f_.safetyQueued = f_.safetyFinished = f_.takeoffSecuredQueued = f_.lineup =
        f_.takeoffCallsQueued = true;
    f_.takeoffCheckDone = 0;
    f_.taxied = s.enginesRunning > 0;
    logInfo(
        "purser: started moving on the ground: boarding, doors and the safety "
        "briefing marked done");
  } else if (s.enginesRunning > 0) {
    // A hot start: no boarding with engines running. Shutting them down at
    // the gate starts over; taxiing out runs the departure from here.
    f_.boardingComplete = f_.armDoors = f_.armDoorsFinished = true;
    f_.armDoorsAt = s.t;
    f_.hotStart = true;
    boardingNote_ =
        "Started with engines running: boarding skipped. Shut the engines down "
        "at the gate to board.";
    note(s.t,
         "started parked with engines running: boarding skipped until the "
         "engines are shut down at the gate");
  }
  prevPhase_ = phase_ = p;
}

void Purser::freshFlight(const SimState& s) {
  Flags fresh;
  fresh.inferred = true;
  fresh.beltState = s.seatbelt;
  fresh.slidesWere = s.slidesArmed;
  fresh.nsLast = s.noSmoking;
  f_ = fresh;
  boardingNote_.clear();
}

void Purser::updateMusic(const SimState& s) {
  // A short loss is a power transfer: the PA's cut-out silences the music
  // for it, and the playlist goes on (D-31).
  if (s.acPower)
    acLostAt_ = -1;
  else if (acLostAt_ < 0)
    acLostAt_ = s.t;
  const bool power = s.acPower || s.t - acLostAt_ < 3.0;
  const bool want = power && !s.replay &&
                    ((f_.boarding && !f_.boardingComplete) ||
                     (f_.deboarding && !f_.deboardComplete));
  if (want != music_) {
    music_ = want;
    out_.setMusic(want);
  }
}

void Purser::applyAdvance(const SimState& s, const Settings& set) {
  const double t = s.t;
  gapUntil_ = t;
  if (call_.active && !call_.talking) {
    call_.forceAnswer = true;
    return;
  }
  double* best = nullptr;
  double bestAt = 1e300;
  auto consider = [&](double& at, bool live) {
    if (live && at > t && at < bestAt) {
      bestAt = at;
      best = &at;
    }
  };
  double preSafetyAt = f_.movedAt >= 0 ? safetyDue() : -1;
  consider(preSafetyAt,
           f_.armDoorsFinished && !f_.safetyQueued && f_.movedAt >= 0);
  consider(f_.takeoffCheckDone,
           !f_.takeoffSecuredQueued && f_.takeoffCheckDone >= 0);
  consider(f_.afterTakeoffDue, !f_.afterTakeoff && f_.afterTakeoffDue >= 0);
  consider(f_.cabinServiceDue, !f_.cabinService && f_.cabinServiceDue >= 0);
  consider(f_.descentDue, !f_.descentStarted && f_.descentDue >= 0);
  consider(f_.landingCheckDone,
           !f_.landingSecuredQueued && f_.landingCheckDone >= 0);
  consider(f_.deboardDone, f_.deboarding && !f_.deboardComplete);
  consider(f_.turnaroundAt, f_.turnaroundAt >= 0);
  for (Item& i : queue_) consider(i.notBefore, true);
  if (best) {
    if (best == &preSafetyAt) {
      f_.movedAt = std::min(f_.movedAt, t - f_.safetyAfterMove);
      f_.armDoorsAt =
          std::min(f_.armDoorsAt, t - static_cast<double>(set.preSafetyDelay));
    } else {
      *best = t;
    }
    return;
  }
  // Nothing is timed: Advance stands in for what the purser waits for.
  if (f_.armDoorsFinished && !f_.safetyQueued && f_.movedAt < 0 &&
      !f_.takeoff) {
    f_.movedAt = t - f_.safetyAfterMove;
    f_.armDoorsAt =
        std::min(f_.armDoorsAt, t - static_cast<double>(set.preSafetyDelay));
    return;
  }
  if (f_.takeoffSecuredQueued && !f_.lineup && !f_.takeoffCallsQueued &&
      !f_.takeoff) {
    signalLineup(s, set, "advanced");
    return;
  }
  if (!f_.boarding && !f_.boardingComplete && !f_.armDoors && !f_.takeoff &&
      s.onGround && s.acPower) {
    startBoarding(s, set, "advanced");
    return;
  }
  if (f_.boarding && !f_.boardingComplete) {
    f_.forceBoardingComplete = true;
    return;
  }
  if (f_.landingCheckDone >= 0 && !f_.landingSecuredQueued) {
    f_.forceLandingCalls = true;
    return;
  }
  if (f_.landingSecuredQueued && !f_.seatsLanding && !f_.landingCallsQueued) {
    signalSeatsLanding(s, set, "advanced");
    return;
  }
  if (f_.disarm && !f_.deboarding) {
    f_.disarmAt = t - set.deboardFallback;
    return;
  }
  logInfo("purser: nothing to advance");
}

void Purser::tick(const SimState& s, const Settings& set) {
  now_ = s.t;
  zulu_ = s.zuluSec;
  local_ = s.localTimeSec;
  preSafetyDelay_ = set.preSafetyDelay;
  doorSettle_ = set.boardingDoorSettle;
  mode_ = set.boarding();
  if (!f_.inferred) {
    // X-Plane is still settling the aircraft right after a load (Run2 showed
    // 0.7 kt on the first tick), so the start state is taken kSettleS later
    // (§ 4.4).
    if (f_.settleFrom < 0) f_.settleFrom = s.t;
    if (s.t - f_.settleFrom < kSettleS) return;
    infer(s);
  }
  watchClock(s);
  flick_ = detectFlick(s, set);
  if (s.fmsPhase && *s.fmsPhase != Phase::PreFlight) f_.fmsLeftPreflight = true;
  // D-26: a new OFP after a completed flight starts the next one.
  if (!f_.takeoff && !f_.flown && plan_.valid) f_.flightSeqno = plan_.seqno;
  if (f_.flightComplete && plan_.valid && plan_.seqno != f_.flightSeqno &&
      s.onGround) {
    const int seqno = plan_.seqno;
    freshFlight(s);
    f_.flightSeqno = seqno;
    note(s.t, "new SimBrief OFP for this airport: the next flight begins");
  }

  // Lift-off (§ 4.2): 5 s above 50 ft AGL, or the aircraft's lift-off cue
  // after its takeoff power cue.
  if (s.cues.takeoffPower == 1) f_.takeoffPowerSeen = true;
  if (!s.onGround && s.aglFt > 50) {
    if (f_.airborneSince < 0) f_.airborneSince = s.t;
    if (s.t - f_.airborneSince >= 5 && !f_.flown) {
      f_.flown = true;
      f_.liftoffAt = f_.airborneSince;
    }
  } else if (s.onGround) {
    f_.airborneSince = -1;
  }
  if (!f_.flown && f_.takeoffPowerSeen && s.cues.liftoff == 1 && !s.onGround) {
    f_.flown = true;
    f_.liftoffAt = s.t;
    note(s.t, "lift-off (" + s.cues.liftoffLabel + ")");
  }

  // D-20: estimate the phase when the FMS gives none.
  const bool wantEstimate = estimateWanted(s, set);
  if (wantEstimate && !estimating_ && !f_.estimateLogged && s.fmsPhase) {
    f_.estimateLogged = true;
    char buf[192];
    std::snprintf(buf, sizeof(buf),
                  "the %s phase is still PreFlight %.0f s after lift-off "
                  "(%s not initialized): estimating the phase",
                  terms().fms.c_str(),
                  static_cast<double>(set.fmsEstimateAfterS),
                  terms().fms.c_str());
    note(s.t, buf);
  }
  estimating_ = wantEstimate;
  phase_ = estimating_ ? estimatePhase(s) : *s.fmsPhase;

  if (!f_.decompression && s.paxMasks && s.mslFt > 10000) {
    // § 4.1: a decompression pre-empts everything.
    f_.decompression = true;
    if (current_) {
      out_.stop();
      finishCurrent(Outcome::Stopped, "decompression");
    }
    for (const Item& i : queue_)
      log(s.t, i.type, i.ctx, Outcome::Expired, "decompression");
    queue_.clear();
    f_.cabinService = true;
    queue_.push_front(
        Item{AnnType::Decompression, CallContext::Any, ExpireOnGround, 0});
    gapUntil_ = s.t;
  }

  if (advanceRequested_) {
    advanceRequested_ = false;
    applyAdvance(s, set);
  }
  if (callPressed_) {
    callPressed_ = false;
    onCabinCall(s, set);
  }
  departure(s, set);
  flight(s, set);
  arrival(s, set);
  updateMusic(s);
  runQueue(s, set);
  f_.slidesWere = s.slidesArmed;
  prevPhase_ = phase_;
}

void Purser::skip() { skipRequested_ = true; }

void Purser::advance() { advanceRequested_ = true; }

}  // namespace xn
