// The SimBrief flight plan without X-Plane: parsing, the origin check, the
// tracker through the flight and the boarding schedule (VP_SPEC § 4.1a,
// § 4.2a).
#include <doctest/doctest.h>

#include "core/settings.h"
#include "core/simbrief.h"

using namespace xn;

namespace {

OfpText dlh400() {
  OfpText t;
  t.seqno = 3;
  t.estOut = "1793452500";  // 2026-10-31 13:15z
  t.icaoAirline = "DLH";
  t.flightNumber = "400";
  t.origin = "eddm";
  t.destination = "EGLL";
  t.paxCount = "150";
  t.maxPassengers = "180";
  t.aircraftIcao = "A20N";
  return t;
}

// an aircraft type with only its boarding lead set
AircraftType withLead(float minMin, float maxMin, int seats) {
  AircraftType t;
  t.lead = {minMin, maxMin, seats};
  return t;
}

}  // namespace

TEST_CASE("simbrief: OFP parsing") {
  std::string why;
  const auto o = parseOfp(dlh400(), &why);
  REQUIRE(o);
  CHECK(why.empty());
  CHECK(o->callsign == "DLH400");
  CHECK(o->origin == "EDDM");
  CHECK(o->pax == 150);
  CHECK(o->seats == 180);
  CHECK(secondOfDay(static_cast<double>(o->estOut)) ==
        doctest::Approx(13 * 3600 + 15 * 60));

  OfpText none;
  CHECK(!parseOfp(none, &why));
  CHECK(why.find("no OFP") != std::string::npos);
  OfpText bad = dlh400();
  bad.estOut = "13:15";
  CHECK(!parseOfp(bad, &why));
  CHECK(why.find("ETD") != std::string::npos);
  OfpText noNumbers = dlh400();
  noNumbers.paxCount = "";
  noNumbers.maxPassengers = "lots";
  noNumbers.flightNumber = "";
  const auto p = parseOfp(noNumbers);
  REQUIRE(p);
  CHECK(p->pax == -1);
  CHECK(p->seats == -1);
  CHECK(p->callsign.empty());
}

TEST_CASE("simbrief: time of day, wrapped around midnight") {
  CHECK(secondOfDay(86400 + 60) == doctest::Approx(60));
  CHECK(secondOfDay(-60) == doctest::Approx(86340));
  CHECK(secondsUntil(13 * 3600, 12 * 3600) == doctest::Approx(3600));
  CHECK(secondsUntil(12 * 3600, 13 * 3600) == doctest::Approx(-3600));
  CHECK(secondsUntil(300, 86100) ==
        doctest::Approx(600));  // 00:05 is 10 min after 23:55
  CHECK(secondsUntil(86100, 300) == doctest::Approx(-600));
  CHECK(zuluLabel(13 * 3600 + 15 * 60) == "13:15Z");
  CHECK(zuluLabel(-60) == "23:59Z");
}

TEST_CASE("simbrief: lead time by type and load (D-19)") {
  const AircraftType a319 = withLead(30, 34, 144);
  const AircraftType a320 = withLead(30, 37, 180);
  const AircraftType a321 = withLead(34, 40, 220);
  CHECK(boardingLead(a320, 0, -1, 0) == doctest::Approx(30 * 60));
  CHECK(boardingLead(a320, 180, -1, 0) == doctest::Approx(37 * 60));
  CHECK(boardingLead(a320, 150, 180, 0) ==
        doctest::Approx((30 + 7 * 150.0 / 180) * 60));
  CHECK(boardingLead(a321, 110, -1, 0) ==
        doctest::Approx(37 * 60));  // half of 220
  CHECK(boardingLead(a319, 500, -1, 0) ==
        doctest::Approx(34 * 60));  // clamped to 1
  CHECK(boardingLead(a319, 0, -1, -60) == doctest::Approx(29 * 60));
  CHECK(boardingLead(a320, 0, -1, -1e6) == 0);
  // a range given the wrong way round is swapped
  CHECK(leadRange(withLead(50, 40, 220)).minS == doctest::Approx(40 * 60));
}

TEST_CASE("simbrief: OFP check and boarding plan") {
  Settings set;
  const Ofp o = *parseOfp(dlh400());
  // eddm ref point aircraft at a gate 1.5 nm out
  CHECK(checkOfp(o, true, 48.3538, 11.7861, 48.3530, 11.7480, set).empty());
  const std::string far =
      checkOfp(o, true, 48.3538, 11.7861, 50.0333, 8.5706, set);  // at eddf
  CHECK(far.find("NM away") != std::string::npos);
  CHECK(checkOfp(o, false, 0, 0, 48.35, 11.78, set).find("not in X-Plane") !=
        std::string::npos);
  Ofp noOrigin = o;
  noOrigin.origin.clear();
  CHECK(!checkOfp(noOrigin, true, 0, 0, 0, 0, set).empty());
  CHECK(distanceNm(0, 0, 1, 0) == doctest::Approx(60.0).epsilon(0.01));

  // the passengers the aircraft reports win over the plan's
  const BoardingPlan p = makeBoardingPlan(o, withLead(30, 37, 180), 0, 0);
  CHECK(p.valid);
  CHECK(p.seqno == 3);
  CHECK(p.etdSec == doctest::Approx(13 * 3600 + 15 * 60));
  CHECK(p.boardingAtSec ==
        doctest::Approx(13 * 3600 + 15 * 60 - (30 + 7 * 150.0 / 180) * 60));
  CHECK(p.summary.find("DLH400 EDDM-EGLL") != std::string::npos);
  const BoardingPlan full = makeBoardingPlan(o, withLead(30, 37, 180), 180, 0);
  CHECK(full.boardingAtSec == doctest::Approx(13 * 3600 + 15 * 60 - 37 * 60));
}

TEST_CASE(
    "simbrief: the OFP through the flight: latched at takeoff, en route, "
    "returning, arrived (VP_SPEC 4.2a)") {
  Settings set;
  const Ofp o = *parseOfp(dlh400());  // dlh400 eddm-egll seqno 3
  const OfpPlace eddm{true, 48.3538, 11.7861}, egll{true, 51.4706, -0.4619};
  OfpTracker tr;
  // gate origin check
  tr.update(o, eddm, egll, 48.3530, 11.7480, true, 0, 0, false, set);
  CHECK(tr.usable());
  CHECK(!tr.latched());
  CHECK(tr.stage() == OfpTracker::Stage::AtOrigin);
  // takeoff latched
  tr.update(o, eddm, egll, 48.36, 11.70, true, 0, 0, true, set);
  CHECK(tr.latched());
  CHECK(tr.currentIsLatched());
  CHECK(!tr.usable());
  // cruising over fra 209 nm out en route not ignored
  tr.update(o, eddm, egll, 50.0333, 8.5706, false, 0, 0, true, set);
  CHECK(tr.stage() == OfpTracker::Stage::EnRoute);
  CHECK(tr.why().empty());
  CHECK(tr.status().find("DLH400 EDDM-EGLL: en route to EGLL, ") == 0);
  // turning back 10 nm closer to eddm and 10 farther from egll
  tr.update(o, eddm, egll, 49.6, 9.8, false, 0, 0, true, set);
  CHECK(tr.stage() == OfpTracker::Stage::Returning);
  CHECK(tr.status().find("returning to EDDM") != std::string::npos);
  // back at eddm passes again
  tr.update(o, eddm, egll, 48.3530, 11.7480, true, 5, 1000, true, set);
  CHECK(tr.stage() == OfpTracker::Stage::Returning);  // not yet: 60 s slow
  tr.update(o, eddm, egll, 48.3530, 11.7480, true, 5, 1060, true, set);
  CHECK(tr.stage() == OfpTracker::Stage::AtOrigin);
  CHECK(tr.usable());

  OfpTracker t2;  // normal flight next legs ofp loaded in flight
  t2.update(o, eddm, egll, 48.3530, 11.7480, true, 0, 0, true, set);
  REQUIRE(t2.latched());
  t2.update(o, eddm, egll, 50.0333, 8.5706, false, 0, 0, true, set);
  Ofp next = o;
  next.seqno = 4;
  next.origin = "EGLL";
  next.destination = "EDDM";
  next.callsign = "DLH401";
  t2.update(next, egll, eddm, 50.5, 5.0, false, 0, 0, true, set);
  CHECK(!t2.usable());
  CHECK(t2.status().find("next OFP DLH401 EGLL-EDDM") != std::string::npos);
  t2.update(next, egll, eddm, 51.4710, -0.4500, true, 0, 0, true, set);
  CHECK(t2.stage() == OfpTracker::Stage::Arrived);
  CHECK(t2.status().find("arrived at EGLL") != std::string::npos);
  CHECK(t2.usable());  // next leg passes at its origin
  t2.update(next, egll, eddm, 51.4710, -0.4500, true, 0, 0, false,
            set);  // purser starts the next flight
  CHECK(!t2.latched());
  CHECK(t2.usable());

  OfpTracker t3;  // same ofp after the flight the one just flown
  t3.update(o, eddm, egll, 48.3530, 11.7480, true, 0, 0, true, set);
  t3.update(o, eddm, egll, 51.4710, -0.4500, true, 0, 0, true, set);
  t3.update(o, eddm, egll, 51.4710, -0.4500, true, 0, 0, false, set);
  CHECK(!t3.usable());
  CHECK(t3.why().find("the flight just flown") != std::string::npos);
}

TEST_CASE(
    "simbrief: the takeoff roll stays en route; a rejected takeoff is back at "
    "the origin after 60 s slow (Run1(XN))") {
  Settings set;
  const Ofp o = *parseOfp(dlh400());
  const OfpPlace eddm{true, 48.3538, 11.7861}, egll{true, 51.4706, -0.4619};
  OfpTracker tr;
  tr.update(o, eddm, egll, 48.3530, 11.7480, true, 60, 100, true, set);
  REQUIRE(tr.latched());
  tr.update(o, eddm, egll, 48.3560, 11.7200, true, 140, 110, true, set);
  CHECK(tr.stage() == OfpTracker::Stage::EnRoute);
  CHECK(!tr.usable());
  // Rejected: stopped on the runway, then taxiing back slowly.
  tr.update(o, eddm, egll, 48.3560, 11.7200, true, 0, 120, true, set);
  tr.update(o, eddm, egll, 48.3560, 11.7210, true, 15, 170, true, set);
  CHECK(tr.stage() == OfpTracker::Stage::EnRoute);
  tr.update(o, eddm, egll, 48.3560, 11.7220, true, 12, 180, true, set);
  CHECK(tr.stage() == OfpTracker::Stage::AtOrigin);
}

TEST_CASE(
    "simbrief: a departure route looping back over the airport is not a "
    "return") {
  Settings set;
  const Ofp o = *parseOfp(dlh400());
  const OfpPlace eddm{true, 48.3538, 11.7861}, egll{true, 51.4706, -0.4619};
  OfpTracker tr;
  tr.update(o, eddm, egll, 48.3530, 11.7480, true, 0, 0, true, set);
  tr.update(o, eddm, egll, 48.35, 12.30, false, 0, 0, true,
            set);  // east first 21 nm out
  tr.update(o, eddm, egll, 48.36, 11.75, false, 0, 0, true,
            set);  // back over the field heading west
  CHECK(tr.stage() ==
        OfpTracker::Stage::EnRoute);  // egll closer than ever not returning
}
