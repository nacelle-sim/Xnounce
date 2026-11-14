/// @file
/// Implements `src/core/simbrief.h`; the interface is documented there.
#include "core/simbrief.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "core/strings.h"

namespace xn {

namespace {

constexpr double kDay = 86400.0;
constexpr double kPi = 3.14159265358979323846;

// A whole number of digits only, else nothing.
std::optional<long long> parseWhole(const std::string& text) {
  const std::string t(trim(text));
  if (t.empty() || t.size() > 18) return std::nullopt;
  for (char c : t)
    if (c < '0' || c > '9') return std::nullopt;
  errno = 0;
  const long long v = std::strtoll(t.c_str(), nullptr, 10);
  if (errno != 0) return std::nullopt;
  return v;
}

}  // namespace

std::optional<Ofp> parseOfp(const OfpText& t, std::string* why) {
  auto fail = [&](const char* reason) -> std::optional<Ofp> {
    if (why) *why = reason;
    return std::nullopt;
  };
  if (t.seqno <= 0) return fail("simbrief_hub has no OFP yet");
  const auto etd = parseWhole(t.estOut);
  if (!etd || *etd < 946684800)
    return fail("the OFP has no usable ETD (est_out)");  // before 2000: junk
  Ofp o;
  o.seqno = t.seqno;
  o.stale = t.stale;
  o.estOut = *etd;
  const std::string airline = toUpper(trim(t.icaoAirline));
  const std::string number = toUpper(trim(t.flightNumber));
  if (!airline.empty() && !number.empty()) o.callsign = airline + number;
  o.origin = toUpper(trim(t.origin));
  o.destination = toUpper(trim(t.destination));
  o.aircraftIcao = toUpper(trim(t.aircraftIcao));
  if (const auto p = parseWhole(t.paxCount); p && *p <= 2000)
    o.pax = static_cast<int>(*p);
  if (const auto m = parseWhole(t.maxPassengers); m && *m > 0 && *m <= 2000)
    o.seats = static_cast<int>(*m);
  if (why) why->clear();
  return o;
}

namespace {

/// Below this groundspeed on the ground at the origin, a latched flight may be
/// back at the stand (VP_SPEC § 4.2a).
constexpr double kBackAtOriginKt = 30;

}  // namespace

double distanceNm(double lat1, double lon1, double lat2, double lon2) {
  const double r = kPi / 180.0;
  const double dLat = (lat2 - lat1) * r, dLon = (lon2 - lon1) * r;
  const double a = std::sin(dLat / 2) * std::sin(dLat / 2) +
                   std::cos(lat1 * r) * std::cos(lat2 * r) *
                       std::sin(dLon / 2) * std::sin(dLon / 2);
  return 2.0 * std::atan2(std::sqrt(a), std::sqrt(std::max(0.0, 1.0 - a))) *
         3440.065;
}

std::string checkOfp(const Ofp& ofp, bool originFound, double originLat,
                     double originLon, double acLat, double acLon,
                     const Settings& set) {
  if (ofp.origin.empty()) return "the OFP has no origin";
  if (!originFound)
    return "origin " + ofp.origin + " is not in X-Plane's airport data";
  const double d = distanceNm(originLat, originLon, acLat, acLon);
  if (!std::isfinite(d) || d > set.ofpOriginNm) {
    char buf[96];
    std::snprintf(buf, sizeof(buf), "origin %s is %.0f NM away",
                  ofp.origin.c_str(), std::isfinite(d) ? d : 0.0);
    return buf;
  }
  return {};
}

void OfpTracker::update(const std::optional<Ofp>& ofp, const OfpPlace& origin,
                        const OfpPlace& dest, double lat, double lon,
                        bool onGround, double gsKt, double t, bool tookOff,
                        const Settings& set) {
  if (!tookOff && latched_) {
    // The kept OFP was flown: a new flight.
    flownSeqno_ = latched_->ofp.seqno;
    latched_.reset();
  }
  usable_ = currentLatched_ = false;
  why_.clear();
  next_.clear();
  auto dist = [&](const OfpPlace& p) {
    return p.found ? distanceNm(lat, lon, p.lat, p.lon) : -1.0;
  };

  if (latched_) {
    Latched& l = *latched_;
    distOrigin_ = dist(l.origin);
    distDest_ = dist(l.dest);
    const double near = set.ofpOriginNm;
    const bool atOrigin = onGround && distOrigin_ >= 0 && distOrigin_ <= near;
    if (!atOrigin || gsKt >= kBackAtOriginKt)
      slowAtOriginSince_ = -1;
    else if (slowAtOriginSince_ < 0)
      slowAtOriginSince_ = t;
    if (onGround && distDest_ >= 0 && distDest_ <= near) {
      stage_ = Stage::Arrived;
    } else if (atOrigin) {
      // Back at the origin, where the OFP passes again: a rejected takeoff or
      // a return to the stand. The takeoff roll itself stays en route.
      if (slowAtOriginSince_ >= 0 &&
          t - slowAtOriginSince_ >= set.ofpBackAtOriginS)
        stage_ = Stage::AtOrigin;
    } else if (!onGround) {
      if (distOrigin_ >= 0) l.maxOrigin = std::max(l.maxOrigin, distOrigin_);
      if (distDest_ >= 0) l.minDest = std::min(l.minDest, distDest_);
      // Returning needs both: closer to the origin than at the farthest, and
      // farther from the destination than at the closest. A departure that
      // loops back over the field still moves toward the destination.
      const bool back = distOrigin_ >= 0 && distDest_ >= 0 &&
                        distOrigin_ <= l.maxOrigin - set.ofpReturnNm &&
                        distDest_ >= l.minDest + set.ofpReturnNm;
      stage_ = back ? Stage::Returning : Stage::EnRoute;
    }
    if (ofp && ofp->seqno == l.ofp.seqno) {
      currentLatched_ = true;
      usable_ = stage_ == Stage::AtOrigin;
    } else if (ofp) {
      // A newer OFP: the next leg's.
      next_ = (ofp->callsign.empty() ? std::string("OFP") : ofp->callsign) +
              " " + ofp->origin + "-" + ofp->destination;
      if (onGround) {
        why_ =
            checkOfp(*ofp, origin.found, origin.lat, origin.lon, lat, lon, set);
        usable_ = why_.empty();
      } else {
        why_ = "next leg, checked at " + ofp->origin;
      }
    }
    return;
  }

  distOrigin_ = distDest_ = -1;
  if (!ofp) {
    stage_ = Stage::NoOfp;
    return;
  }
  why_ = checkOfp(*ofp, origin.found, origin.lat, origin.lon, lat, lon, set);
  usable_ = why_.empty();
  stage_ = usable_ ? Stage::AtOrigin : Stage::NotHere;
  if (!usable_ && ofp->seqno == flownSeqno_)
    why_ += " (the OFP of the flight just flown: load the next leg's)";
  if (tookOff && usable_) {
    Latched l;
    l.ofp = *ofp;
    l.origin = origin;
    l.dest = dest;
    latched_ = l;
    stage_ = Stage::EnRoute;
    slowAtOriginSince_ = -1;
    usable_ = false;
    currentLatched_ = true;
  }
}

std::string OfpTracker::status() const {
  if (!latched_) return {};
  const Latched& l = *latched_;
  std::string s =
      (l.ofp.callsign.empty() ? std::string("OFP") : l.ofp.callsign) + " " +
      l.ofp.origin + "-" + l.ofp.destination + ": ";
  char nm[32] = "";
  switch (stage_) {
    case Stage::Arrived:
      s += "arrived at " + l.ofp.destination;
      break;
    case Stage::AtOrigin:
      s += "back at " + l.ofp.origin;
      break;
    case Stage::Returning:
      if (distOrigin_ >= 0)
        std::snprintf(nm, sizeof(nm), ", %.0f NM", distOrigin_);
      s += "returning to " + l.ofp.origin + nm;
      break;
    default:
      if (distDest_ >= 0) std::snprintf(nm, sizeof(nm), ", %.0f NM", distDest_);
      s += "en route to " + l.ofp.destination + nm;
      break;
  }
  if (!next_.empty()) s += "; next OFP " + next_ + (usable_ ? " (ready)" : "");
  return s;
}

double secondOfDay(double s) {
  if (!std::isfinite(s)) return 0;
  double r = std::fmod(s, kDay);
  if (r < 0) r += kDay;
  return r;
}

double secondsUntil(double atSec, double nowSec) {
  double d = secondOfDay(atSec) - secondOfDay(nowSec);
  if (d > kDay / 2) d -= kDay;
  if (d <= -kDay / 2) d += kDay;
  return d;
}

LeadRange leadRange(const AircraftType& type) {
  LeadRange r{type.lead.minMin * 60.0, type.lead.maxMin * 60.0,
              type.lead.seats};
  if (r.maxS < r.minS) std::swap(r.minS, r.maxS);
  r.seats = std::max(1, r.seats);
  return r;
}

double boardingLead(const AircraftType& type, int pax, int ofpSeats,
                    double jitterS) {
  const LeadRange r = leadRange(type);
  const int seats = ofpSeats > 0 ? ofpSeats : r.seats;
  const double load =
      std::clamp(static_cast<double>(std::max(0, pax)) / seats, 0.0, 1.0);
  return std::max(0.0, r.minS + (r.maxS - r.minS) * load + jitterS);
}

std::string zuluLabel(double sec) {
  const int m = static_cast<int>(secondOfDay(sec) / 60.0);
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%02d:%02dZ", (m / 60) % 24, m % 60);
  return buf;
}

BoardingPlan makeBoardingPlan(const Ofp& ofp, const AircraftType& type, int pax,
                              double jitterS) {
  BoardingPlan p;
  p.valid = true;
  p.seqno = ofp.seqno;
  p.etdSec = secondOfDay(static_cast<double>(ofp.estOut));
  // The aircraft's passenger count wins over the OFP's.
  const int people = pax > 0 ? pax : std::max(0, ofp.pax);
  p.pax = people;
  const double lead = boardingLead(type, people, ofp.seats, jitterS);
  p.boardingAtSec = secondOfDay(p.etdSec - lead);
  char buf[64];
  std::snprintf(buf, sizeof(buf), ", lead %.0f min for %d pax", lead / 60.0,
                people);
  p.summary =
      (ofp.callsign.empty() ? std::string("OFP") : ofp.callsign) + " " +
      ofp.origin + "-" + ofp.destination + ", ETD " + zuluLabel(p.etdSec) +
      ", boarding " + zuluLabel(p.boardingAtSec) + buf +
      (ofp.stale ? " (simbrief_hub: last download failed, older plan)" : "");
  return p;
}

}  // namespace xn
