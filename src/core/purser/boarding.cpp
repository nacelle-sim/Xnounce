/// @file
/// When boarding starts and how long it lasts (VP_SPEC § 4.1a, § 4.1b; D-1,
/// D-18, D-19).
#include <algorithm>
#include <cmath>
#include <cstdio>

#include "core/log.h"
#include "core/purser/purser.h"
#include "core/purser/purser_internal.h"

namespace xn {

using namespace purser_detail;

void Purser::scheduleBoarding(const SimState& s, const Settings& set,
                              bool atGate) {
  const bool usable = plan_.valid && s.zuluSec >= 0;
  if (!usable) {
    if (f_.clockMismatch) {
      f_.clockMismatch = false;
      boardingNote_.clear();
    }
    if (f_.sched != Sched::Doors || f_.planSeqno != -2) {
      logInfo(
          "purser: no valid SimBrief plan: boarding starts when a door has "
          "been open %.0f s",
          static_cast<double>(set.boardingDoorSettle));
      f_.planSeqno = -2;
      f_.planAt = -1;
    }
    f_.sched = Sched::Doors;
    doorsBoarding(s, set, atGate);
    return;
  }
  if (plan_.seqno != f_.planSeqno || plan_.boardingAtSec != f_.planAt) {
    // A plan seen for the first time: before its boarding time we wait;
    // after it (a late load) the doors rule applies. A plan re-timed while
    // we wait keeps waiting, so a time that slid into the past boards now.
    f_.planSeqno = plan_.seqno;
    f_.planAt = plan_.boardingAtSec;
    const double until = secondsUntil(plan_.boardingAtSec, s.zuluSec);
    if (f_.clockMismatch) boardingNote_.clear();
    f_.clockMismatch = until > set.maxWaitMin * 60.0;
    if (f_.clockMismatch) {
      // X-Plane's clock isn't on the flight's time (§ 4.1a): the doors rule
      // applies, but the OFP still picks the pack.
      f_.sched = Sched::Doors;
      boardingNote_ = "SimBrief boarding time is " + hmm(until) +
                      " away: is the X-Plane clock set to the flight's time? "
                      "Using the doors rule.";
      note(s.t, boardingNote_);
      doorsBoarding(s, set, atGate);
      return;
    }
    f_.sched = (until > 0 || f_.sched == Sched::Waiting) ? Sched::Waiting
                                                         : Sched::Doors;
    if (f_.sched == Sched::Waiting) {
      note(s.t, "boarding at " + zuluLabel(plan_.boardingAtSec) +
                    " (SimBrief, in " + mmss(until) + ")");
    } else {
      char buf[160];
      std::snprintf(buf, sizeof(buf),
                    "the SimBrief boarding time %s has passed: boarding starts "
                    "when a door has been open %.0f s",
                    zuluLabel(plan_.boardingAtSec).c_str(),
                    static_cast<double>(set.boardingDoorSettle));
      note(s.t, buf);
    }
  }
  if (f_.sched == Sched::Waiting) {
    if (secondsUntil(plan_.boardingAtSec, s.zuluSec) > 0) return;
    if (atGate) {
      f_.sched = Sched::Unknown;
      startBoarding(s, set, "SimBrief boarding time");
      return;
    }
    note(s.t, "SimBrief boarding time reached away from the gate: doors rule");
    f_.sched = Sched::Doors;
  }
  doorsBoarding(s, set, atGate);
}

void Purser::doorsBoarding(const SimState& s, const Settings& set,
                           bool atGate) {
  // D-18: boarding starts once there has been AC power, after any
  // turnaround gap, for boarding_door_settle_s while a door is open or has
  // been open at this gate (an AUTO door can close before the power is on).
  const bool gapOk = f_.turnaroundAt < 0 || s.t >= f_.turnaroundAt;
  if (!atGate || !s.acPower || !f_.doorSeenOpenAtGate || !gapOk) {
    f_.readySince = -1;
    return;
  }
  if (f_.readySince < 0) f_.readySince = s.t;
  if (s.t - f_.readySince >= set.boardingDoorSettle)
    startBoarding(s, set,
                  s.doorMax > 0.5f
                      ? "doors open"
                      : "AC power on, a door was open at this gate");
}

double Purser::boardingEnd(const SimState& s, const Settings& set) {
  // § 4.1b. The plan counts only in the schedule mode with a sane clock.
  const double minLen = std::max(30.0f, set.boardingMinS);
  const double natural = std::max(minLen, f_.boardingNatural);
  const bool clockOff = f_.clockMismatch;
  if (!clockOff) boardingNote_.clear();
  if (mode_ != BoardingStart::Schedule || !plan_.valid || clockOff ||
      s.zuluSec < 0)
    return f_.boardingStartAt + natural;
  const double toEtd = secondsUntil(plan_.etdSec, s.zuluSec);
  const double len = (s.t - f_.boardingStartAt) + toEtd - set.endBeforeEtdS;
  // On schedule, boarding follows the schedule. Only a late start that
  // leaves less than the natural length warns (an OBT left at SimBrief's
  // 30-minute default, for example).
  const bool late =
      f_.boardingStartZulu >= 0 &&
      secondsUntil(plan_.boardingAtSec, f_.boardingStartZulu) < -kLateStartS;
  if (len >= natural || !late)
    return f_.boardingStartAt +
           std::clamp(len, minLen, kBoardingMaxFactor * natural);
  const double shortened = std::max(minLen, len);
  const int minutes = static_cast<int>(std::lround(shortened / 60.0));
  char buf[220];
  if (toEtd <= 0)
    std::snprintf(buf, sizeof(buf),
                  "Boarding shortened to %d min: the SimBrief ETD %s has "
                  "passed. Set the OBT later in "
                  "SimBrief for a full boarding.",
                  minutes, zuluLabel(plan_.etdSec).c_str());
  else
    std::snprintf(buf, sizeof(buf),
                  "Boarding shortened to %d min: the SimBrief ETD is only %d "
                  "min away. Set the OBT later "
                  "in SimBrief for a full boarding.",
                  minutes, static_cast<int>(toEtd / 60.0));
  boardingNote_ = buf;
  note(s.t, buf);
  return f_.boardingStartAt + shortened;
}

void Purser::startBoarding(const SimState& s, const Settings& set,
                           const char* why) {
  f_.boarding = true;
  f_.nextWelcome = s.t + randIn(20, 45);
  f_.turnaroundAt = -1;
  // The passengers the aircraft reports, else the plan's.
  const int pax = s.pax > 0 ? s.pax : std::max(0, plan_.pax);
  f_.boardingNatural = (set.boardingBaseS + set.boardingPerPaxS * pax) *
                       type_.typeFactor *
                       randIn(1.0 - kBoardingJitter, 1.0 + kBoardingJitter);
  f_.boardingStartAt = s.t;
  f_.boardingStartZulu = s.zuluSec;
  f_.boardingPlanSeqno = plan_.valid ? plan_.seqno : -1;
  f_.boardingEndAt = boardingEnd(s, set);
  note(s.t, std::string("boarding started (") + why + "), complete in " +
                mmss(f_.boardingEndAt - s.t));
}

void Purser::watchClock(const SimState& s) {
  // § 4.1a: a jump of X-Plane's clock before boarding evaluates the plan
  // again.
  if (s.zuluSec < 0) {
    lastZulu_ = lastZuluT_ = -1;
    return;
  }
  if (lastZulu_ >= 0 && lastZuluT_ >= 0) {
    const double expected = lastZulu_ + (s.t - lastZuluT_);
    const double jump = std::fabs(secondsUntil(s.zuluSec, expected));
    if (jump > kClockJumpS && !f_.boarding && !f_.boardingComplete &&
        !f_.armDoors && !f_.takeoff && f_.planSeqno >= 0) {
      note(s.t, "the X-Plane clock jumped by " + mmss(jump) +
                    ": the SimBrief plan is evaluated again");
      f_.planSeqno = -1;
      f_.planAt = -1;
    }
  }
  lastZulu_ = s.zuluSec;
  lastZuluT_ = s.t;
}

}  // namespace xn
