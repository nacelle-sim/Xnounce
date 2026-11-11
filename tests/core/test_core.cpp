// The small pieces of the core: strings, the log view, the ini format, the
// settings round trip, aircraft type tags and the trace recorder.
#include <doctest/doctest.h>

#include "core/aircraft_type.h"
#include "core/log.h"
#include "core/settings.h"
#include "core/strings.h"

using namespace xn;

TEST_CASE("strings") {
  CHECK(iequals("Refuelling", "REFUELLING"));
  CHECK(!iequals("A320", "A321"));
  CHECK(trim("  a b \t") == "a b");
  CHECK(splitList(" WiFi, USB ;;x") ==
        std::vector<std::string>{"WiFi", "USB", "x"});
  CHECK(globMatch("British Airways*",
                  "british airways (G-EUXG) crest livery 8K"));
  CHECK(globMatch("*(G-*", "[LEA] EasyJet (G-UZHH)"));
  CHECK(!globMatch("Lufthansa*", "Air Lufthansa"));
  CHECK(globMatch("A?2?", "A321"));
  const char raw[8] = {'D', 'L', 'H', '4', ' ', '\0', 'x', 'y'};
  CHECK(cleanCString(raw, sizeof(raw)) == "DLH4");
}

TEST_CASE("log view collapses exact repeats (ARCHITECTURE § 7)") {
  Log& log = Log::get();
  auto tail = [&](size_t n) {
    const auto r = log.recent();
    return std::vector<LogLine>(
        r.end() - static_cast<long>(std::min(n, r.size())), r.end());
  };
  logInfo("dedup A");
  logInfo("dedup B");
  logInfo("dedup A");  // a moves to the bottom
  auto t = tail(2);
  CHECK(t[0].msg == "dedup B");
  CHECK(t[0].count == 1);
  CHECK(t[1].msg == "dedup A");
  CHECK(t[1].count == 2);
  logInfo("dedup A");
  CHECK(tail(1)[0].count == 3);
  logWarn("dedup A");  // level is part of the key
  t = tail(2);
  CHECK(t[0].level == LogLevel::Info);
  CHECK(t[0].count == 3);
  CHECK(t[1].level == LogLevel::Warn);
  CHECK(t[1].count == 1);
  CHECK(std::string(logLevelName(LogLevel::Warn)) == "WARN");
  size_t seen = 0;
  for (const LogLine& l : log.recent()) seen += l.msg == "dedup A" ? 1 : 0;
  CHECK(seen == 2);
  for (int k = 0; k < 450; ++k) logInfo("dedup fill %d", k);
  CHECK(log.recent().size() == Log::kRecentLines);
  CHECK(tail(1)[0].msg == "dedup fill 449");
}

TEST_CASE("ini parse and serialize") {
  const Ini ini = Ini::parse(
      "\xEF\xBB\xBF# comment\nroot = 1\n[British Airways*]\nairline = BAW   # "
      "inline\n"
      "custom_tags = \"WiFi,USB\"\n\n[ x ]\nk=v\n");
  REQUIRE(ini.get("", "root"));
  CHECK(*ini.get("", "root") == "1");
  CHECK(*ini.get("british airways*", "AIRLINE") == "BAW");
  CHECK(*ini.get("British Airways*", "custom_tags") == "WiFi,USB");
  CHECK(*ini.get("x", "k") == "v");
  CHECK(ini.get("x", "missing") == nullptr);
  const Ini again = Ini::parse(ini.serialize());
  CHECK(*again.get("British Airways*", "airline") == "BAW");
}

TEST_CASE("settings round trip and clamping") {
  Settings s;
  s.masterVolume = 0.5f;
  s.satcomTags = "WiFi";
  s.interphoneRetries = 2;
  s.disabledEvents = {"CabinService"};
  Ini ini;
  storeSettings(s, ini);
  Settings t;
  loadSettings(Ini::parse(ini.serialize()), t);
  CHECK(t.masterVolume == doctest::Approx(0.5f));
  CHECK(t.satcomTags == "WiFi");
  CHECK(t.interphoneRetries == 2);
  CHECK(!t.eventEnabled("cabinservice"));
  CHECK(t.eventEnabled("AfterTakeoff"));

  Settings u;
  loadSettings(
      Ini::parse("[general]\nmaster_volume = 9\nlog_level = "
                 "x\n[timing]\nwelcome_min_s = 500\nwelcome_max_s = 200\n"),
      u);
  CHECK(u.masterVolume == doctest::Approx(2.0f));  // clamped to the ui range
  CHECK(u.logLevel == 2);                          // bad value keeps default
  CHECK(u.welcomeMin == doctest::Approx(200));     // swapped back
  CHECK(u.welcomeMax == doctest::Approx(500));
}

TEST_CASE("aircraft type tags and the bracket code of a livery name") {
  CHECK(leadingBracketCode("[IAEST] Lufthansa") == "IAEST");
  CHECK(leadingBracketCode("  [least]Lufthansa") == "LEAST");
  CHECK(leadingBracketCode("Lufthansa (D-AIJB) 8K").empty());
  CHECK(leadingBracketCode("[unclosed").empty());

  for (const char* t : {"A320", "A21N", "A20N", "A319", "B738", "B38M", "B77W",
                        "B788", "E190", "E75L", "CRJ9", "DH8D"})
    CHECK_MESSAGE(isAircraftTypeTag(t), t);
  for (const char* t : {"Morning", "1", "WiFi", "USB", "Refueling", "Overwater",
                        "GA", "Done", "KDFW", "ARR"})
    CHECK_MESSAGE(!isAircraftTypeTag(t), t);
}

#include "core/recorder.h"

TEST_CASE("recorder trace round trip") {
  SimState s;
  s.t = 12.5;
  s.onGround = false;
  s.gsKt = 250;
  s.aglFt = 9000;
  s.fmsPhaseRaw = 2;
  s.fmsPhase = Phase::Climb;
  s.seatbelt = true;
  s.doorMax = 0.5f;
  s.pax = 180;
  s.paListen = true;
  s.paListenVol = 0.75f;
  s.cues.takeoffPower = 0;
  s.cues.liftoff = 1;
  s.cues.phaseHint = Phase::Takeoff;
  s.cues.phaseKnown = true;
  s.cues.liftoffLabel = "phase 5, lift-off";
  s.aircraftVolume = 0.5f;
  s.flightNumber = "BAW12,3";  // comma forces quotes
  s.acfIcao = "A321";
  s.satcom = true;
  const std::string text = std::string(traceHeader()) + "\n" +
                           traceLine(s, "toliss") + "\nE,13.00" +
                           std::string(41, ',') + ",ArmDoors played\n";
  const std::vector<TraceRow> rows = readTrace(text);
  REQUIRE(rows.size() == 2);
  const SimState& r = rows[0].state;
  CHECK(!rows[0].isEvent);
  CHECK(r.t == doctest::Approx(12.5));
  CHECK(!r.onGround);
  CHECK(r.gsKt == doctest::Approx(250));
  CHECK(r.fmsPhase == Phase::Climb);
  CHECK(r.fmsPhaseRaw == 2);
  CHECK(!rows[0].legacy);
  CHECK(rows[0].aircraft == "toliss");
  CHECK(rows[0].extra.empty());
  CHECK(r.cues.takeoffPower == 0);
  CHECK(r.cues.leversAtTakeoff == -1);
  CHECK(r.cues.liftoff == 1);
  CHECK(r.cues.touchdown == -1);
  CHECK(r.cues.phaseHint == Phase::Takeoff);
  CHECK(r.cues.phaseKnown);
  CHECK(r.cues.liftoffLabel == "phase 5, lift-off");
  CHECK(r.aircraftVolume == doctest::Approx(0.5f));
  CHECK(r.paListen);
  CHECK(r.seatbelt);
  CHECK(r.pax == 180);
  CHECK(r.paListenVol == doctest::Approx(0.75f));
  CHECK(r.flightNumber == "BAW12,3");
  CHECK(r.acfIcao == "A321");
  CHECK(r.satcom);
  CHECK(rows[1].isEvent);
  CHECK(rows[1].event == "ArmDoors played");
}
