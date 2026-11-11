/// @file
/// What the purser tells the window: the stage and the "coming up" list.
#include <algorithm>

#include "core/log.h"
#include "core/purser/purser.h"
#include "core/purser/purser_internal.h"

namespace xn {

using namespace purser_detail;

std::string Purser::stageLabel() const {
  if (f_.decompression && f_.touchdownAt < 0) return "Decompression";
  if (f_.flightComplete) return "Flight complete";
  if (f_.deboarding)
    return f_.deboardComplete ? "Deboarding complete" : "Deboarding";
  if (f_.disarm) return "At the stand";
  if (f_.touchdownAt >= 0) return "Landed, taxiing in";
  if (f_.flown || f_.takeoff) {
    switch (phase_) {
      case Phase::Takeoff:
        return "Takeoff";
      case Phase::Climb:
        return "Climb";
      case Phase::Cruise:
        return "Cruise";
      case Phase::Descent:
        return "Descent";
      case Phase::Approach:
        return f_.landingCallsQueued     ? "Approach, crew seated"
               : f_.landingSecuredQueued ? "Approach, cabin secured"
                                         : "Approach";
      case Phase::GA:
        return "Go-around";
      default:
        return "Airborne";
    }
  }
  if (f_.takeoffCallsQueued) return "Crew seated for takeoff";
  if (f_.takeoffSecuredQueued) return "Cabin secured, waiting for line-up";
  if (f_.safetyFinished) return "Cabin check before takeoff";
  if (f_.safetyQueued) return "Safety briefing";
  if (f_.armDoors) return "Doors armed";
  if (f_.boardingComplete) return "Boarding complete";
  if (f_.boarding) return "Boarding";
  if (f_.turnaroundAt >= 0) return "Turnaround";
  return "At the gate";
}

std::vector<PendingItem> Purser::pending() const {
  std::vector<std::pair<double, PendingItem>> v;
  const double t = now_;
  auto timed = [&](double at, PendingItem it) {
    it.inSec = std::max(0.0, at - t);
    v.emplace_back(at, std::move(it));
  };
  auto waits = [&](double rank, PendingItem it) {
    it.inSec = -1;
    v.emplace_back(t + rank, std::move(it));
  };
  auto ann = [](AnnType a, CallContext c = CallContext::Any,
                std::string note = {}, std::string cond = {}) {
    PendingItem it;
    it.type = a;
    it.context = c;
    it.note = std::move(note);
    it.condition = std::move(cond);
    return it;
  };
  auto milestone = [](std::string what, std::string cond = {}) {
    PendingItem it;
    it.what = std::move(what);
    it.condition = std::move(cond);
    return it;
  };
  for (const Item& i : queue_)
    timed(std::max(i.notBefore, t), ann(i.type, i.ctx));
  if (f_.boarding && !f_.boardingComplete) {
    timed(f_.nextWelcome, ann(AnnType::BoardingWelcome));
    timed(f_.boardingEndAt, ann(AnnType::BoardingComplete));
    waits(1e6, ann(AnnType::BoardingComplete, CallContext::Any, {},
                   "sooner on beacon, pushback, engine start or a cabin call"));
  }
  if (!f_.armDoors && f_.boardingComplete)
    waits(1e6, ann(AnnType::ArmDoors, CallContext::Any, {},
                   "on beacon, pushback or engine start"));
  if (f_.hotStart)
    waits(2e6,
          milestone("Boarding", "after the engines are shut down at the gate"));
  if (f_.flightComplete)
    waits(2e6, milestone("Next flight",
                         "with a new SimBrief OFP from this airport (or switch "
                         "on Settings > Announcements > Turnaround)"));
  if (!f_.boarding && !f_.boardingComplete && !f_.armDoors && !f_.takeoff) {
    if (mode_ == BoardingStart::Schedule && f_.sched == Sched::Waiting &&
        zulu_ >= 0) {
      const double until = secondsUntil(plan_.boardingAtSec, zulu_);
      timed(t + std::max(0.0, until),
            milestone("Boarding at " + zuluLabel(plan_.boardingAtSec) +
                      " (SimBrief)"));
    } else if (mode_ == BoardingStart::Call) {
      waits(2e6, milestone("Boarding", "starts on a cabin call (" +
                                           terms().cabinCallButtons + ")"));
    } else {
      char buf[128];
      std::snprintf(buf, sizeof(buf),
                    "starts %.0f s after a door is open at the gate with "
                    "power, or on a cabin call",
                    static_cast<double>(doorSettle_));
      waits(2e6, milestone("Boarding", buf));
    }
  }
  if (f_.armDoorsFinished && !f_.safetyQueued && !f_.takeoff) {
    if (f_.movedAt >= 0)
      timed(safetyDue(), ann(AnnType::PreSafetyBriefing));
    else
      waits(1e6, ann(AnnType::PreSafetyBriefing, CallContext::Any, {},
                     "when the aircraft moves (pushback or taxi)"));
  }
  if (f_.takeoffCheckDone >= 0 && !f_.takeoffSecuredQueued)
    timed(f_.takeoffCheckDone, ann(AnnType::CabinSecured, CallContext::Takeoff,
                                   "when the cabin check ends"));
  if (f_.takeoffSecuredQueued && !f_.takeoffCallsQueued && !f_.takeoff)
    waits(1e6, ann(AnnType::CrewTakeSeats, CallContext::Takeoff, {},
                   f_.lineup ? "after the cabin call"
                             : "on line-up: landing lights ON, strobe ON, or a "
                               "cabin call while taxiing"));
  if (f_.afterTakeoffDue >= 0 && !f_.afterTakeoff)
    timed(f_.afterTakeoffDue, ann(AnnType::AfterTakeoff));
  if (f_.cabinServiceDue >= 0 && !f_.cabinService)
    timed(f_.cabinServiceDue, ann(AnnType::CabinService));
  if (f_.descentDue >= 0 && !f_.descentStarted)
    timed(f_.descentDue, ann(AnnType::DescentStarted));
  if (f_.landingCheckDone >= 0 && !f_.landingSecuredQueued)
    timed(f_.landingCheckDone, ann(AnnType::CabinSecured, CallContext::Landing,
                                   "when the cabin check ends, in the approach "
                                   "or below the ready height"));
  if (f_.landingSecuredQueued && !f_.landingCallsQueued && !f_.seatsLanding)
    waits(1e6, ann(AnnType::CrewTakeSeats, CallContext::Landing, {},
                   "on a NO SMOKING flick or the gear down"));
  if (f_.deboarding && !f_.deboardComplete)
    timed(f_.deboardDone, ann(AnnType::DeboardingComplete));
  if (f_.turnaroundAt >= 0 && !f_.boarding)
    timed(f_.turnaroundAt, milestone("Next boarding"));
  std::stable_sort(v.begin(), v.end(), [](const auto& a, const auto& b) {
    return a.first < b.first;
  });
  std::vector<PendingItem> out;
  out.reserve(v.size());
  for (auto& p : v) out.push_back(std::move(p.second));
  return out;
}

std::vector<std::string> Purser::pendingLabels() const {
  std::vector<std::string> out;
  for (const PendingItem& p : pending()) {
    std::string line = p.type ? annFenixLabel(*p.type, p.context) : p.what;
    if (!p.note.empty()) line += ", " + p.note;
    if (p.inSec >= 0)
      line += p.inSec > 0 ? " in " + mmss(p.inSec) : std::string(" now");
    else if (!p.condition.empty())
      line += " " + p.condition;
    out.push_back(std::move(line));
  }
  return out;
}

std::string Purser::nextLabel() const {
  const auto p = pendingLabels();
  return p.empty() ? std::string("-") : p.front();
}

std::string Purser::nowLabel() const {
  if (!current_) return "-";
  std::string s = annTypeName(current_->type);
  if (current_->ctx != CallContext::Any)
    s += std::string(" (") + contextName(current_->ctx) + ")";
  if (call_.active && !call_.talking)
    s += ": cabin calling, attempt " + std::to_string(call_.attempt + 1);
  return s;
}

}  // namespace xn
