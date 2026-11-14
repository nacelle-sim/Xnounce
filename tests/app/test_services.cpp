// The services between the parts of the plugin, without X-Plane: the Airline
// Pack service, the flight plan service and the presenter that fills the
// window's model.
#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <thread>

#include "app/flight_plan_service.h"
#include "app/pack_service.h"
#include "app/ui_presenter.h"
#include "core/purser/purser.h"
#include "support/paths.h"

using namespace xn;

namespace {

// ---- packs
// -------------------------------------------------------------------

// A folder of packs made of empty .ogg files: a scan reads only the names.
std::string makePacks(
    const char* name,
    const std::map<std::string, std::vector<std::string>>& packs) {
  namespace fs = std::filesystem;
  const fs::path root = xn::testing::tmpDir() / name;
  std::error_code ec;
  fs::remove_all(root, ec);
  for (const auto& [pack, files] : packs) {
    fs::create_directories(root / pack, ec);
    for (const std::string& f : files) std::ofstream(root / pack / f);
  }
  return root.string() + "/";
}

// Starts a scan and waits for it.
void scan(PackService& packs) {
  packs.startScan("");
  for (int k = 0; k < 500 && !packs.poll({}); ++k)
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  REQUIRE(packs.library());
}

PackService::Request request(const char* flightNumber, bool locked = false,
                             const char* why = "test") {
  PackService::Request r;
  r.flightNumber = flightNumber;
  r.packLocked = locked;
  r.why = why;
  return r;
}

// ---- flight plans
// ------------------------------------------------------------

struct FakeHub : FlightPlanSource {
  bool hub = true;
  bool changed = false;
  OfpText ofp;
  std::map<std::string, std::pair<double, double>> airports = {
      {"EDDM", {48.3538, 11.7861}}, {"EGLL", {51.4706, -0.4619}}};
  int resets = 0;

  bool poll(double) override {
    const bool c = changed;
    changed = false;
    return c;
  }
  void reset() override { ++resets; }
  bool installed() const override { return hub; }
  const OfpText& text() const override { return ofp; }
  bool findAirport(const std::string& icao, double, double, double& lat,
                   double& lon) override {
    const auto it = airports.find(icao);
    if (it == airports.end()) return false;
    lat = it->second.first;
    lon = it->second.second;
    return true;
  }
  void load(int seqno) {
    ofp.seqno = seqno;
    ofp.estOut = "1793452500";  // 2026-10-31 13:15Z
    ofp.icaoAirline = "DLH";
    ofp.flightNumber = "400";
    ofp.origin = "EDDM";
    ofp.destination = "EGLL";
    ofp.paxCount = "150";
    ofp.maxPassengers = "180";
    ofp.aircraftIcao = "A20N";
    changed = true;
  }
};

AircraftType a320() {
  AircraftType t;
  t.label = "A320";
  t.designators = {"A320", "A20N"};
  t.lead = {30, 37, 180};
  return t;
}

// The aircraft at a gate at Munich, 1.5 NM from the airport's reference point.
SimState atMunich(double t) {
  SimState s;
  s.t = t;
  s.lat = 48.3530;
  s.lon = 11.7480;
  s.zuluSec = 12 * 3600;
  return s;
}

}  // namespace

TEST_CASE("pack service: scan, resolve, lock and keep the pack by name") {
  PackService packs;
  packs.setPacksDir(makePacks(
      "packs-a",
      {{"DLH", {"BoardingWelcome.ogg", "ArmDoors[1].ogg", "ArmDoors[2].ogg"}},
       {"Default", {"BoardingWelcome.ogg"}}}));
  CHECK(!packs.library());
  scan(packs);
  CHECK(packs.library()->packs.size() == 2);
  CHECK(!packs.pack());  // nothing chosen before resolve

  packs.resolve(request("DLH400", false, "activated"));
  REQUIRE(packs.pack());
  CHECK(packs.pack()->name == "DLH");
  CHECK(packs.resolvedFlightNumber() == "DLH400");
  // DLH has variants 1 and 2 only, so the session variant is one of them
  CHECK((packs.variant() == 1 || packs.variant() == 2));

  // locked: a new flight number changes nothing but is remembered
  packs.resolve(request("BAW12", true));
  CHECK(packs.pack()->name == "DLH");
  CHECK(packs.resolvedFlightNumber() == "BAW12");
  packs.resolve(request("BAW12"));
  CHECK(packs.pack()->name == "Default");
  CHECK(packs.variant() == 0);

  // a rescan drops the pack pointer; the caller keeps it by name
  packs.resolve(request("DLH400"));
  bool stopped = false;
  packs.startScan("");
  for (int k = 0; k < 500 && !packs.poll([&] { stopped = true; }); ++k)
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  CHECK(stopped);
  CHECK(!packs.pack());
  CHECK(packs.packBeforeScan() == "DLH");
  CHECK(packs.keepPack("DLH"));
  CHECK(packs.pack()->name == "DLH");
  CHECK(!packs.keepPack("BAW"));

  packs.clear();
  CHECK(!packs.pack());
  packs.dropLibrary();
  CHECK(!packs.library());
}

TEST_CASE(
    "pack service: custom tags from the livery and the satellite antenna") {
  PackService packs;
  packs.setPacksDir(makePacks("packs-b", {{"DLH", {"BoardingWelcome.ogg"}}}));
  const std::filesystem::path livery =
      xn::testing::tmpDir() / "packs-b" / "livery";
  std::filesystem::create_directories(livery);
  std::ofstream(livery / "airlinepack.cfg")
      << "airline = DLH\ncustom_tags = Premium, WiFi\n";
  packs.loadLiveryConfig(livery.string() + "/", "livery");
  CHECK(packs.liveryConfig().airline == "DLH");
  CHECK(packs.customTags(false, "WiFi,USB") ==
        std::vector<std::string>{"Premium", "WiFi"});
  // WiFi is there already, so only USB is added
  CHECK(packs.customTags(true, "WiFi,USB") ==
        std::vector<std::string>{"Premium", "WiFi", "USB"});
}

TEST_CASE("flight plan service: no hub, a plan here, a plan elsewhere") {
  Settings set;
  set.leadJitterS = 0;
  FakeHub hub;
  FlightPlanService plans(hub, set);
  std::vector<std::string> trace;
  plans.setEventSink(
      [&](double, const std::string& line) { trace.push_back(line); });

  hub.hub = false;
  plans.tick(atMunich(1), false, a320(), "A20N");
  CHECK(!plans.installed());
  CHECK(!plans.plan().valid);
  CHECK(plans.plan().summary == "simbrief_hub not installed");
  CHECK(plans.callsign().empty());

  hub.hub = true;
  hub.load(3);
  plans.tick(atMunich(2), false, a320(), "A20N");
  CHECK(plans.accepted());
  CHECK(plans.rejection().empty());
  CHECK(plans.callsign() == "DLH400");
  REQUIRE(plans.plan().valid);
  CHECK(plans.plan().pax == 150);
  CHECK(plans.plan().etdSec == doctest::Approx(13 * 3600 + 15 * 60));
  CHECK(plans.placesCurrent());
  CHECK(plans.origin().found);
  CHECK(std::find(trace.begin(), trace.end(), "simbrief accepted") !=
        trace.end());
  const std::vector<std::string> intro = plans.traceIntro();
  REQUIRE(intro.size() == 2);
  CHECK(intro[0] == "simbrief accepted");
  CHECK(intro[1].rfind("boarding plan: DLH400 EDDM-EGLL", 0) == 0);

  // a teleport to Frankfurt: checked again at once, then ignored
  plans.resetFlight();
  SimState frankfurt = atMunich(3);
  frankfurt.lat = 50.0333;
  frankfurt.lon = 8.5706;
  plans.tick(frankfurt, false, a320(), "A20N");
  CHECK(!plans.accepted());
  CHECK(plans.rejection().find("NM away") != std::string::npos);
  CHECK(plans.callsign().empty());
  CHECK(!plans.plan().valid);
  CHECK(plans.plan().summary.rfind("OFP ignored: origin EDDM", 0) == 0);

  plans.reset();
  CHECK(hub.resets == 1);
  CHECK(!plans.ofp());
}

TEST_CASE(
    "flight plan service: the plan accepted at takeoff stays the flight's") {
  Settings set;
  set.leadJitterS = 0;
  FakeHub hub;
  FlightPlanService plans(hub, set);
  hub.load(3);
  plans.tick(atMunich(1), false, a320(), "A20N");
  REQUIRE(plans.accepted());
  plans.tick(atMunich(2), true, a320(), "A20N");  // takeoff: latched now
  CHECK(plans.tracker().latched());
  SimState cruise = atMunich(20);
  cruise.onGround = false;
  cruise.lat = 50.0333;
  cruise.lon = 8.5706;
  plans.tick(cruise, true, a320(), "A20N");
  CHECK(plans.callsign() == "DLH400");  // still this flight's plan
  CHECK(plans.plan().summary == "the OFP is in use for this flight");
  CHECK(plans.tracker().status().find("en route to EGLL") != std::string::npos);

  // the Flight card while en route
  ui::UiModel::Flight f;
  fillFlight(f, plans, AirlineTable::builtin(), cruise, set, "");
  CHECK(f.state == 1);
  CHECK(f.callsign == "DLH400");
  CHECK(f.line.find("en route to EGLL") != std::string::npos);
  CHECK(f.routeFrac > 0.0f);
  CHECK(f.routeFrac < 1.0f);
}

TEST_CASE(
    "flight plan service: an airport X-Plane can't find yet is looked up again "
    "for 2 min (Run1(XN))") {
  Settings set;
  set.leadJitterS = 0;
  FakeHub hub;
  const auto eddm = hub.airports["EDDM"];
  hub.airports.erase("EDDM");  // the nav data isn't ready after the load
  FlightPlanService plans(hub, set);
  std::vector<std::string> trace;
  plans.setEventSink(
      [&](double, const std::string& line) { trace.push_back(line); });
  hub.load(3);
  plans.tick(atMunich(1), false, a320(), "A20N");
  CHECK(!plans.accepted());
  CHECK(plans.plan().summary == "looking up EDDM in X-Plane's airport data");
  CHECK(std::none_of(trace.begin(), trace.end(), [](const std::string& l) {
    return l.rfind("simbrief ignored", 0) == 0;
  }));
  hub.airports["EDDM"] = eddm;  // ready now
  plans.tick(atMunich(12), false, a320(), "A20N");
  CHECK(plans.accepted());
  CHECK(plans.plan().valid);

  // A miss that lasts: ignored after 2 min, then not looked up again.
  FakeHub hub2;
  hub2.airports.erase("EDDM");
  FlightPlanService plans2(hub2, set);
  hub2.load(3);
  for (double t = 1; t <= 130; t += 10)
    plans2.tick(atMunich(t), false, a320(), "A20N");
  CHECK(!plans2.accepted());
  CHECK(plans2.rejection().find("not in X-Plane's airport data") !=
        std::string::npos);
}

TEST_CASE(
    "flight plan service: passengers coming aboard log no new plan lines "
    "(Run1(XN))") {
  Settings set;
  set.leadJitterS = 0;
  FakeHub hub;
  FlightPlanService plans(hub, set);
  std::vector<std::string> trace;
  plans.setEventSink([&](double, const std::string& line) {
    if (line.rfind("boarding plan:", 0) == 0) trace.push_back(line);
  });
  hub.load(3);
  plans.tick(atMunich(1), false, a320(), "A20N");
  REQUIRE(plans.plan().valid);
  const double boardingAt = plans.plan().boardingAtSec;
  plans.setBoardingBegun(true);
  for (int pax = 1; pax <= 150; ++pax) {
    SimState s = atMunich(10 + pax);
    s.pax = pax;
    plans.tick(s, false, a320(), "A20N");
  }
  CHECK(trace.size() == 1);
  CHECK(plans.plan().boardingAtSec == doctest::Approx(boardingAt));
}

TEST_CASE("presenter: the Flight card before boarding") {
  Settings set;
  set.leadJitterS = 0;
  FakeHub hub;
  FlightPlanService plans(hub, set);
  ui::UiModel::Flight f;

  hub.hub = false;
  plans.tick(atMunich(1), false, a320(), "A20N");
  fillFlight(f, plans, AirlineTable::builtin(), atMunich(1), set, "");
  CHECK(f.state == 0);
  CHECK(f.reason.find("simbrief_hub is not installed") == 0);

  hub.hub = true;
  plans.tick(atMunich(2), false, a320(), "A20N");
  fillFlight(f, plans, AirlineTable::builtin(), atMunich(2), set, "");
  CHECK(f.reason == "No OFP in simbrief_hub yet.");

  hub.load(3);
  plans.tick(atMunich(3), false, a320(), "A20N");
  fillFlight(f, plans, AirlineTable::builtin(), atMunich(3), set, "");
  CHECK(f.state == 1);
  CHECK(f.callsign == "DLH400");
  CHECK(f.airline == "Lufthansa");
  CHECK(f.etd == "13:15Z");
  CHECK(f.boarding == "12:39Z");  // 13:15 less 30 + 7 x 150/180 min
  CHECK(f.line == "Boarding in 39:10");
  // a boarding note replaces the countdown
  fillFlight(f, plans, AirlineTable::builtin(), atMunich(3), set, "note");
  CHECK(f.line.empty());
  set.boardingStart = static_cast<int>(BoardingStart::Doors);
  fillFlight(f, plans, AirlineTable::builtin(), atMunich(3), set, "");
  CHECK(f.line == "Not used for boarding (Boarding starts: Doors open)");
}

TEST_CASE("presenter: the dormant card and the explainer without a pack") {
  ui::UiModel m;
  m.stage = "Boarding";
  m.events.resize(3);
  LoadedAircraft ac;
  ac.uiName = "ToLiss A339";
  ac.acfFile = "A330-900.acf";
  presentDormant(m, ac, ": a ToLiss model this version doesn't support yet");
  CHECK(m.loadedAircraft ==
        "ToLiss A339 (A330-900.acf): a ToLiss model this version doesn't "
        "support yet");
  CHECK(m.stage.empty());
  CHECK(m.events.empty());
  CHECK(explainSelection(nullptr, 1, 0, SelectContext{}) ==
        std::vector<std::string>{"No active pack."});
}

TEST_CASE("presenter: an active flight uses the aircraft's words") {
  Settings set;
  SimState sim = atMunich(5);
  sim.fmsPhase = Phase::PreFlight;
  sim.fmsPhaseRaw = 0;
  AircraftType type = a320();
  type.terms.fms = "FMGS";
  type.terms.answerInterphone = "pull CAB on the ACP";
  PackService packs;
  FakeHub hub;
  FlightPlanService plans(hub, set);
  ActiveView view(set, sim, type, packs, plans);
  view.aircraftName = "ToLiss";
  view.aircraftRows = {{"ECAM / QPAC phase", "1 / 1", "Purser phase"},
                       {"Somewhere", "x", "no such row"}};
  ui::UiModel m;
  presentActive(m, view);
  CHECK(m.aircraft == "ToLiss A320");
  CHECK(m.livery == "(default livery)");
  CHECK(m.pack == "scanning...");
  CHECK(m.answerHint == "Pull CAB on the ACP to answer");
  REQUIRE(m.liveInputs.size() >= 3);
  CHECK(m.liveInputs[0].first == "FMGS phase");
  CHECK(m.liveInputs[0].second == "PreFlight (0)");
  CHECK(m.liveInputs[2].first == "ECAM / QPAC phase");  // after Purser phase
  CHECK(m.liveInputs.back().first == "Somewhere");      // unknown anchor: end
}
