/// @file
/// Implements `src/app/flight_plan_service.h`; the interface is documented
/// there.
#include "app/flight_plan_service.h"

#include <algorithm>

#include "core/log.h"
#include "core/strings.h"

namespace xn {

namespace {

/// How long a missed airport is looked up again (VP_SPEC § 4.2a).
constexpr double kLookupRetryS = 120;

}  // namespace

FlightPlanService::FlightPlanService(FlightPlanSource& source,
                                     const Settings& settings)
    : source_(source), settings_(settings) {}

void FlightPlanService::reset() {
  source_.reset();
  ofp_.reset();
  ofpOk_ = false;
  ofpCheck_.clear();
  tracker_.reset();
  originSeqno_ = jitterSeqno_ = loggedSeqno_ = -1;
  lookupSince_ = -1;
  lookupPending_ = false;
  boardingPax_ = -1;
  nextCheck_ = -1;
  plan_ = BoardingPlan{};
  planLogged_.clear();
  planLogKey_.clear();
}

void FlightPlanService::resetFlight() {
  nextCheck_ = -1;  // the aircraft may be at another airport now
  tracker_.reset();
  boardingPax_ = -1;
  // An airport that wasn't found is looked up again from the new position.
  if (!originPlace_.found) originSeqno_ = -1;
}

void FlightPlanService::tick(const SimState& sim, bool tookOff,
                             const AircraftType& type,
                             const std::string& flyingIcao) {
  if (source_.poll(sim.t)) readPlan(type, flyingIcao);
  check(sim, tookOff, false);
  updateBoardingPlan(sim, type);
}

void FlightPlanService::readPlan(const AircraftType& type,
                                 const std::string& flyingIcao) {
  std::string why;
  ofp_ = parseOfp(source_.text(), &why);
  ofpWhy_ = ofp_ ? std::string() : why;
  nextCheck_ = -1;  // check it on this tick
  if (!ofp_) {
    logInfo("SimBrief: %s", why.c_str());
    return;
  }
  logInfo("SimBrief OFP #%d: %s %s-%s, ETD %s%s", ofp_->seqno,
          ofp_->callsign.empty() ? "(no callsign)" : ofp_->callsign.c_str(),
          ofp_->origin.c_str(), ofp_->destination.c_str(),
          zuluLabel(static_cast<double>(ofp_->estOut)).c_str(),
          ofp_->stale ? " (simbrief_hub: last download failed)" : "");
  const bool sameFamily = std::any_of(
      type.designators.begin(), type.designators.end(),
      [&](const std::string& d) { return iequals(d, ofp_->aircraftIcao); });
  if (!ofp_->aircraftIcao.empty() && !flyingIcao.empty() && !sameFamily)
    logInfo("SimBrief OFP is planned for a %s; flying a %s",
            ofp_->aircraftIcao.c_str(), flyingIcao.c_str());
}

void FlightPlanService::check(const SimState& sim, bool tookOff, bool force) {
  // Until takeoff the plan must start where the aircraft is. From takeoff on
  // the tracker follows the plan that was accepted (VP_SPEC § 4.2a).
  if (tookOff != lastTookOff_) {
    lastTookOff_ = tookOff;
    force = true;  // latch the plan while the aircraft is still at the origin
  }
  if (!ofp_) {
    tracker_.update(std::nullopt, {}, {}, sim.lat, sim.lon, sim.onGround,
                    sim.gsKt, sim.t, tookOff, settings_);
    ofpOk_ = false;
    ofpCheck_.clear();
    return;
  }
  if (!force && nextCheck_ >= 0 && sim.t < nextCheck_) return;
  nextCheck_ = sim.t + 10;
  if (sim.lat == 0 && sim.lon == 0) return;
  if (originSeqno_ != ofp_->seqno || lookupPending_) {
    // One search of the navigation data per airport and plan. Right after an
    // aircraft loads X-Plane may not answer yet, so a missed origin is looked
    // up again on each check for a while (VP_SPEC § 4.2a).
    if (originSeqno_ != ofp_->seqno) {
      originPlace_ = destPlace_ = OfpPlace{};
      originSeqno_ = ofp_->seqno;
      lookupSince_ = sim.t;
    }
    if (!originPlace_.found)
      originPlace_.found = source_.findAirport(
          ofp_->origin, sim.lat, sim.lon, originPlace_.lat, originPlace_.lon);
    if (!destPlace_.found && !ofp_->destination.empty())
      destPlace_.found = source_.findAirport(
          ofp_->destination, sim.lat, sim.lon, destPlace_.lat, destPlace_.lon);
    lookupPending_ = !originPlace_.found && !ofp_->origin.empty() &&
                     sim.t - lookupSince_ < kLookupRetryS;
    if (lookupPending_) {
      ofpOk_ = false;
      ofpCheck_ = "looking up " + ofp_->origin + " in X-Plane's airport data";
      return;
    }
  }
  const bool wasLatched = tracker_.latched();
  const OfpTracker::Stage before = tracker_.stage();
  tracker_.update(ofp_, originPlace_, destPlace_, sim.lat, sim.lon,
                  sim.onGround, sim.gsKt, sim.t, tookOff, settings_);
  const bool ok = tracker_.usable();
  const std::string why = tracker_.why();
  if (tracker_.latched()) {
    // Logged when the stage changes. The distance in it changes all the time.
    if (!wasLatched || tracker_.stage() != before) {
      logInfo("SimBrief: %s", tracker_.status().c_str());
      event(sim.t, "simbrief " + tracker_.status());
    }
    if (!tracker_.currentIsLatched() &&
        (loggedSeqno_ != ofp_->seqno || ok != ofpOk_)) {
      loggedSeqno_ = ofp_->seqno;
      logInfo("SimBrief OFP #%d is for the next leg: %s", ofp_->seqno,
              ok ? "accepted here" : why.c_str());
    }
  } else if (ok != ofpOk_ || loggedSeqno_ != ofp_->seqno || wasLatched) {
    loggedSeqno_ = ofp_->seqno;
    if (ok)
      logInfo("SimBrief OFP accepted: the aircraft is at %s",
              ofp_->origin.c_str());
    else
      logInfo("SimBrief OFP ignored: %s", why.c_str());
    event(sim.t,
          std::string("simbrief ") + (ok ? "accepted" : "ignored: " + why));
  }
  ofpOk_ = ok;
  ofpCheck_ = why;
}

void FlightPlanService::updateBoardingPlan(const SimState& sim,
                                           const AircraftType& type) {
  BoardingPlan p;
  if (ofp_ && ofpOk_) {
    if (jitterSeqno_ != ofp_->seqno) {
      // Rolled once per plan (VP_SPEC § 4.1a).
      std::uniform_real_distribution<double> d(-settings_.leadJitterS,
                                               settings_.leadJitterS);
      leadJitter_ = settings_.leadJitterS > 0 ? d(rng_) : 0.0;
      jitterSeqno_ = ofp_->seqno;
    }
    // While boarding, the plan keeps the count it had: the passengers coming
    // aboard are not the planned load.
    if (!boardingBegun_)
      boardingPax_ = -1;
    else if (boardingPax_ < 0)
      boardingPax_ = plan_.valid ? plan_.pax : sim.pax;
    p = makeBoardingPlan(*ofp_, type, boardingBegun_ ? boardingPax_ : sim.pax,
                         leadJitter_);
  } else if (tracker_.latched()) {
    // The Status line has the details.
    p.summary = "the OFP is in use for this flight";
  } else if (!source_.installed()) {
    p.summary = "simbrief_hub not installed";
  } else if (!ofp_) {
    p.summary = ofpWhy_;
  } else if (lookupPending_) {
    p.summary = ofpCheck_;
  } else {
    p.summary = "OFP ignored: " + ofpCheck_;
  }
  plan_ = p;
  // The passenger count alone doesn't make a new log line (Run1(XN) logged one
  // per passenger); the Status tab shows the live plan.
  std::string key = plan_.summary;
  if (plan_.valid) {
    const size_t at = key.find(" for ");
    const size_t pax = key.find(" pax", at == std::string::npos ? 0 : at);
    if (at != std::string::npos && pax != std::string::npos)
      key.erase(at, pax + 4 - at);
  }
  planLogged_ = plan_.summary;
  if (key != planLogKey_) {
    planLogKey_ = key;
    if (plan_.valid) logInfo("boarding plan: %s", plan_.summary.c_str());
    event(sim.t, "boarding plan: " + plan_.summary);
  }
}

std::vector<std::string> FlightPlanService::traceIntro() const {
  std::vector<std::string> lines;
  if (ofp_)
    lines.push_back(std::string("simbrief ") +
                    (ofpOk_ ? "accepted" : "ignored: " + ofpCheck_));
  if (!planLogged_.empty()) lines.push_back("boarding plan: " + planLogged_);
  return lines;
}

}  // namespace xn
