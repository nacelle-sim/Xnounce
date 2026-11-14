/// @file
/// The SimBrief flight plan through a flight: reading it, checking that it
/// belongs to where the aircraft is, following it once airborne, and turning
/// it into the boarding plan (VP_SPEC § 4.1a, § 4.2a).
///
/// The plan comes from the simbrief_hub plugin and the airport positions from
/// X-Plane's navigation data. Both sit behind `FlightPlanSource`, so the
/// service itself needs no X-Plane and can be tested with a stand-in.
#pragma once

#include <functional>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "core/aircraft_type.h"
#include "core/settings.h"
#include "core/sim_state.h"
#include "core/simbrief.h"

namespace xn {

/// Where the flight plan and the airports come from.
class FlightPlanSource {
 public:
  virtual ~FlightPlanSource() = default;
  /// Looks for a new plan. True when `text()` changed.
  virtual bool poll(double now) = 0;
  /// Forgets what was read (a new aircraft was loaded).
  virtual void reset() = 0;
  /// The simbrief_hub plugin is running.
  virtual bool installed() const = 0;
  virtual const OfpText& text() const = 0;
  /// Position of the airport with exactly this ICAO code, searched near the
  /// given point. False when X-Plane doesn't know the code.
  virtual bool findAirport(const std::string& icao, double nearLat,
                           double nearLon, double& lat, double& lon) = 0;
};

class FlightPlanService {
 public:
  /// Both arguments must outlive the service.
  FlightPlanService(FlightPlanSource& source, const Settings& settings);

  /// Receives lines for the flight trace, with the purser clock.
  void setEventSink(std::function<void(double, const std::string&)> sink) {
    sink_ = std::move(sink);
  }

  /// An aircraft was activated: forget the plan and start over.
  void reset();
  /// The flight was reset (teleport, new situation): check the plan again
  /// right away and forget how far the aircraft had flown.
  void resetFlight();

  /// One tick: reads a new plan, checks it against the aircraft's position
  /// and updates the boarding plan.
  /// @param tookOff The purser's "took off" state. A plan accepted at
  ///        takeoff stays this flight's plan from then on.
  /// @param flyingIcao ICAO designator of the loaded aircraft, for the log
  ///        line about a plan made for another type.
  void tick(const SimState& sim, bool tookOff, const AircraftType& type,
            const std::string& flyingIcao);

  /// Tells the service that boarding has begun on this flight. From then on the
  /// plan keeps the passenger count it had, so passengers coming aboard don't
  /// move the boarding time (Run1(XN)).
  void setBoardingBegun(bool begun) { boardingBegun_ = begun; }

  /// When boarding starts and the flight departs. Not valid without a usable
  /// plan; its summary then says why.
  const BoardingPlan& plan() const { return plan_; }
  /// The plan's callsign while the plan may choose the Airline Pack: it
  /// passed its check, or it is this flight's plan. Otherwise empty.
  std::string callsign() const {
    return ofp_ && (ofpOk_ || tracker_.currentIsLatched()) ? ofp_->callsign
                                                           : std::string();
  }
  /// Lines that describe the current state, for the start of a new trace.
  std::vector<std::string> traceIntro() const;

  // For the Flight card of the Status tab.
  bool installed() const { return source_.installed(); }
  const std::optional<Ofp>& ofp() const { return ofp_; }
  /// Why there is no plan. Empty when there is one.
  const std::string& noPlanReason() const { return ofpWhy_; }
  /// The plan passed its origin check.
  bool accepted() const { return ofpOk_; }
  /// Why the plan failed its check. Empty when it passed.
  const std::string& rejection() const { return ofpCheck_; }
  const OfpTracker& tracker() const { return tracker_; }
  /// Positions of the plan's airports. Unset until the plan was checked.
  const OfpPlace& origin() const { return originPlace_; }
  const OfpPlace& destination() const { return destPlace_; }
  /// True when `origin()` and `destination()` belong to the current plan.
  bool placesCurrent() const { return ofp_ && originSeqno_ == ofp_->seqno; }

 private:
  void readPlan(const AircraftType& type, const std::string& flyingIcao);
  void check(const SimState& sim, bool tookOff, bool force);
  void updateBoardingPlan(const SimState& sim, const AircraftType& type);
  void event(double t, const std::string& what) {
    if (sink_) sink_(t, what);
  }

  FlightPlanSource& source_;
  const Settings& settings_;
  std::function<void(double, const std::string&)> sink_;
  std::mt19937 rng_{std::random_device{}()};

  std::optional<Ofp> ofp_;
  std::string ofpWhy_;
  bool ofpOk_ = false;
  std::string ofpCheck_;
  OfpPlace originPlace_, destPlace_;
  /// Plan number the airport positions were looked up for.
  int originSeqno_ = -1;
  /// When the airport lookup for `originSeqno_` began, and whether a missed
  /// origin is still being looked up again (VP_SPEC § 4.2a).
  double lookupSince_ = -1;
  bool lookupPending_ = false;
  bool boardingBegun_ = false;
  /// The passenger count the plan keeps while boarding, or -1.
  int boardingPax_ = -1;
  OfpTracker tracker_;
  bool lastTookOff_ = false;
  double nextCheck_ = -1;
  BoardingPlan plan_;
  /// Random part of the boarding lead, rolled once per plan.
  double leadJitter_ = 0;
  int jitterSeqno_ = -1;
  std::string planLogged_;
  /// The logged plan without its passenger count: a new line only when the
  /// times or the plan change.
  std::string planLogKey_;
  int loggedSeqno_ = -1;
};

}  // namespace xn
