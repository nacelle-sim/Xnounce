/// @file
/// The SimBrief flight plan (OFP) that simbrief_hub publishes: parsing it, the
/// origin check, following it through the flight, and the boarding schedule
/// it gives (VP_SPEC § 4.1a, § 4.2a). Reading the datarefs is
/// `sim/simbrief_source`.
#pragma once

#include <optional>
#include <string>

#include "core/aircraft_type.h"
#include "core/settings.h"

namespace xn {

/// The `sbh/*` datarefs as text, as simbrief_hub publishes them.
struct OfpText {
  int seqno = 0;       ///< 0 until the first download.
  bool stale = false;  ///< The last download failed.
  std::string estOut;  ///< The ETD, UTC epoch seconds.
  std::string icaoAirline, flightNumber, origin, destination, aircraftIcao;
  std::string paxCount, maxPassengers;
};

struct Ofp {
  int seqno = 0;
  bool stale = false;
  long long estOut = 0;  ///< The ETD, UTC epoch seconds.
  std::string callsign;  ///< "DLH400"; may be empty.
  std::string origin, destination, aircraftIcao;
  int pax = -1;  ///< -1 when the OFP doesn't say.
  int seats = -1;
};

std::optional<Ofp> parseOfp(const OfpText& t, std::string* why = nullptr);

/// Great-circle distance in nautical miles.
double distanceNm(double lat1, double lon1, double lat2, double lon2);

/// Checks that the OFP starts where the aircraft is. Returns an empty string
/// when it does, else why not. `originFound` is false when X-Plane doesn't
/// know the origin's code.
std::string checkOfp(const Ofp& ofp, bool originFound, double originLat,
                     double originLon, double acLat, double acLon,
                     const Settings& set);

struct OfpPlace {
  bool found = false;
  double lat = 0, lon = 0;
};

/// Follows the OFP through the flight (VP_SPEC § 4.2a): checked against the
/// origin before takeoff, kept from takeoff on and followed by distance (en
/// route, returning, arrived). A newer OFP that arrives in flight is the next
/// leg's and is checked once the aircraft is on the ground.
class OfpTracker {
 public:
  enum class Stage { NoOfp, AtOrigin, NotHere, EnRoute, Returning, Arrived };

  /// One check. `tookOff` is the purser's state.
  void update(const std::optional<Ofp>& ofp, const OfpPlace& origin,
              const OfpPlace& dest, double lat, double lon, bool onGround,
              bool tookOff, const Settings& set);
  void reset() { *this = OfpTracker{}; }

  bool usable() const { return usable_; }
  bool currentIsLatched() const { return currentLatched_; }
  bool latched() const { return latched_.has_value(); }
  Stage stage() const { return stage_; }
  const std::string& why() const { return why_; }
  /// The Status line of the flight being flown ("DLH400 EDDM-EGLL: en route
  /// to EGLL, 312 NM"), or empty.
  std::string status() const;

 private:
  struct Latched {
    Ofp ofp;
    OfpPlace origin, dest;
    double maxOrigin = 0, minDest = 1e12;
  };
  std::optional<Latched> latched_;
  int flownSeqno_ = -1;
  Stage stage_ = Stage::NoOfp;
  bool usable_ = false, currentLatched_ = false;
  std::string why_, next_;
  double distOrigin_ = -1, distDest_ = -1;
};

/// Seconds since midnight, 0 to 86400.
double secondOfDay(double s);
/// Seconds from `nowSec` to `atSec`, wrapped to -12 h … +12 h. X-Plane has no
/// year, so times of day are compared.
double secondsUntil(double atSec, double nowSec);

struct LeadRange {
  double minS = 0, maxS = 0;  ///< With an empty and with a full cabin.
  int seats = 1;
};
LeadRange leadRange(const AircraftType& type);
/// Boarding lead in seconds: min + (max - min) x passengers / seats, plus the
/// jitter, never negative (VP_SPEC § 4.1a, D-19).
double boardingLead(const AircraftType& type, int pax, int ofpSeats,
                    double jitterS);

/// When boarding starts and the flight departs, from the OFP (VP_SPEC § 4.1a).
struct BoardingPlan {
  bool valid = false;
  double boardingAtSec = 0;  ///< UTC seconds since midnight.
  double etdSec = 0;
  int seqno = 0;
  int pax = 0;          ///< The aircraft's passengers, else the OFP's.
  std::string summary;  ///< The plan in words, or why there is none.
};
BoardingPlan makeBoardingPlan(const Ofp& ofp, const AircraftType& type, int pax,
                              double jitterS);

std::string zuluLabel(double sec);

}  // namespace xn
