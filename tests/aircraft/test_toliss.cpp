// The ToLiss module's pure parts: recognising the aircraft, the type table,
// the dataref decoders, the cues and the upgrade of traces recorded before
// 0.3.5 (docs/aircraft/toliss.md).
#include <doctest/doctest.h>

#include <cmath>

#include "aircraft/toliss/toliss_options.h"
#include "aircraft/toliss/toliss_signals.h"
#include "aircraft/toliss/toliss_types.h"
#include "aircraft/trace_compat.h"
#include "core/recorder.h"

using namespace xn;
using namespace xn::toliss;

namespace {

LoadedAircraft tolissA321() {
  LoadedAircraft a;
  a.acfPath = "/X-Plane 12/Aircraft/ToLissA321_V1p8/a321.acf";
  a.acfFile = "a321.acf";
  a.acfIcao = "A321";
  a.author = "Gliding Kiwi";
  return a;
}

}  // namespace

TEST_CASE("ToLiss: families, engine codes and type tags") {
  CHECK(familyFrom("A321", "a321.acf") == Family::A321);
  CHECK(familyFrom("A20N", "a320.acf") == Family::A320);
  CHECK(familyFrom("A339", "A330-900.acf") == Family::A330);
  CHECK(familyFrom("", "a321_StdDef.acf") == Family::A321);
  CHECK(familyFrom("C172", "Cessna_172SP.acf") == Family::Unknown);

  CHECK(engineIsNeo("LEA") == 1);
  CHECK(engineIsNeo("PWG") == 1);
  CHECK(engineIsNeo("IAE") == 0);
  CHECK(engineIsNeo("") == -1);

  const Options o;
  const AircraftType n = makeType(Family::A321, true, o);
  CHECK(n.tags == std::vector<std::string>{"A321", "A21N"});
  CHECK(n.specific == "A21N");
  CHECK(n.label == "A321neo");
  CHECK(n.typeFactor == doctest::Approx(1.25f));
  CHECK(n.lead.minMin == doctest::Approx(34));
  CHECK(n.lead.seats == 220);
  CHECK(n.terms.fms == "FMGS");
  CHECK(makeType(Family::A321, false, o).specific == "A321");
  CHECK(makeType(Family::A330, false, o).tags ==
        std::vector<std::string>{"A330", "A339"});
  // the A330 has no boarding rows of its own and uses the A320's
  CHECK(makeType(Family::A330, false, o).lead.maxMin == doctest::Approx(37));
}

TEST_CASE(
    "ToLiss: NEO or CEO from the live ICAO, livery.tlscfg or the livery") {
  const Options o;
  TypeInputs in;
  in.loadedIcao = "A321";
  in.acfFile = "a321.acf";
  CHECK(currentType(in, o).label == "A321");
  in.liveIcao = "A21N";  // the engine option changed in the ISCS
  CHECK(currentType(in, o).label == "A321neo");
  in.liveIcao.clear();
  in.loadedIcao.clear();
  in.tlscfgEngine = "PWG";
  CHECK(currentType(in, o).label == "A321neo");
  in.tlscfgEngine.clear();
  in.liveryName = "[CFMST] British Airways";
  CHECK(currentType(in, o).label == "A321");
  in.liveryName = "[LEAST] Lufthansa";
  CHECK(currentType(in, o).label == "A321neo");
  // user settings reach the type
  Options mine;
  mine.a321.minMin = 50;
  CHECK(currentType(in, mine).lead.minMin == doctest::Approx(50));
}

TEST_CASE("ToLiss: recognising the aircraft") {
  const Recognition a321 = classify(tolissA321(), true);
  CHECK(a321.ours);
  CHECK(a321.supported);
  CHECK(a321.model == "A321");

  LoadedAircraft a339 = tolissA321();
  a339.acfFile = "A330-900.acf";
  a339.acfIcao = "A339";
  const Recognition wide = classify(a339, true);
  CHECK(wide.ours);
  CHECK(!wide.supported);
  CHECK(wide.dormantReason == "not a supported ToLiss model");
  CHECK(wide.logLine ==
        "dormant: ToLiss A330-900.acf (A330) is not supported yet; this "
        "version runs on the A319, A320 and A321");
  CHECK(wide.cardNote == ": a ToLiss model this version doesn't support yet");

  CHECK(!classify(tolissA321(), false).ours);  // the systems plugin isn't up
  LoadedAircraft other = tolissA321();
  other.author = "Someone";
  other.acfPath = "/X-Plane 12/Aircraft/Other A321/a321.acf";
  CHECK(!classify(other, true).ours);  // same datarefs, not by ToLiss
  LoadedAircraft cessna = tolissA321();
  cessna.acfIcao = "C172";
  cessna.acfFile = "Cessna_172SP.acf";
  CHECK(!classify(cessna, true).ours);
}

TEST_CASE(
    "ToLiss decoders: SDSpoilerArray status codes and the strobe switch "
    "(Run2)") {
  // Run2 values: unpowered, hydraulics up, roll spoilers in a turn, ground
  // spoilers at touchdown
  CHECK(groundSpoilersFromSd(std::vector<float>(16, 2.0f)) == -1);
  CHECK(groundSpoilersFromSd(
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2}) == 0);
  CHECK(groundSpoilersFromSd(
            {0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 2, 2, 2, 2, 2, 2}) == 0);
  CHECK(groundSpoilersFromSd(
            {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2}) == 1);
  CHECK(groundSpoilersFromSd({}) == -1);
  // strobe at index 7: 1 auto, 2 on
  const std::vector<float> taxi = {1, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 2};
  const std::vector<float> lineup = {1, 0, 1, 2, 2, 2, 1, 2, 0, 0, 1, 1, 2};
  CHECK(switchAt(taxi, 7, 2) == 0);
  CHECK(switchAt(lineup, 7, 2) == 1);
  CHECK(switchAt(lineup, 40, 2) == -1);
}

TEST_CASE("ToLiss: ACP knob volume in degrees or as a ratio") {
  CHECK(acpKnobVolume(270) == doctest::Approx(1.0f));  // fully clockwise
  CHECK(acpKnobVolume(135) == doctest::Approx(0.5f));
  CHECK(acpKnobVolume(400) == doctest::Approx(1.0f));  // clamped
  // the A339's RotaryPositions are already 0 to 1
  CHECK(acpKnobVolume(0.8f) == doctest::Approx(0.8f));
  CHECK(acpKnobVolume(-3) == doctest::Approx(0.0f));
  CHECK(acpKnobVolume(std::nanf("")) == doctest::Approx(0.0f));
}

TEST_CASE("ToLiss: cues from the ECAM and QPAC phases and the thrust levers") {
  const AircraftCues none = cuesFrom(-1, -1, kUnknownLever, 0.85f);
  CHECK(none.takeoffPower == -1);
  CHECK(none.leversAtTakeoff == -1);
  CHECK(none.liftoff == -1);
  CHECK(none.touchdown == -1);
  CHECK(!none.phaseHint);
  CHECK(!none.phaseKnown);

  CHECK(cuesFrom(3, 4, 0.88f, 0.85f).takeoffPower == 1);
  CHECK(cuesFrom(4, 5, 1.0f, 0.85f).takeoffPower == 1);
  CHECK(cuesFrom(2, 3, 0.2f, 0.85f).takeoffPower == 0);
  CHECK(cuesFrom(2, 3, 0.88f, 0.85f).leversAtTakeoff == 1);
  CHECK(cuesFrom(2, 3, 0.7f, 0.85f).leversAtTakeoff == 0);
  CHECK(cuesFrom(2, 3, -1.0f, 0.85f).leversAtTakeoff == 0);  // reverse
  const AircraftCues up = cuesFrom(5, 6, 0.88f, 0.85f);
  CHECK(up.liftoff == 1);
  CHECK(up.liftoffLabel == "ECAM flight phase 5");
  CHECK(up.phaseHint == Phase::Takeoff);
  CHECK(up.phaseKnown);
  CHECK(!cuesFrom(6, 8, 0.7f, 0.85f).phaseHint);  // climb: the purser decides
  CHECK(cuesFrom(7, 9, 0, 0.85f).phaseHint == Phase::Approach);
  CHECK(cuesFrom(7, 10, 0, 0.85f).phaseHint == Phase::Approach);
  CHECK(cuesFrom(8, 11, 0, 0.85f).touchdown == 1);
  CHECK(cuesFrom(9, 12, 0, 0.85f).touchdown == 1);
  CHECK(cuesFrom(10, 14, 0, 0.85f).touchdown == 0);
}

TEST_CASE("ToLiss: the internal volume counts only with the ISCS override on") {
  CHECK(volumeFactor(0.5f, -1) == doctest::Approx(-1.0f));  // switch unknown
  CHECK(volumeFactor(0.5f, 0) == doctest::Approx(-1.0f));
  CHECK(volumeFactor(0.5f, 1) == doctest::Approx(0.5f));
  CHECK(volumeFactor(-1, 1) == doctest::Approx(-1.0f));  // no slider
}

TEST_CASE("ToLiss: the 0.2.0 ACP index is migrated once") {
  Options o;
  const Ini old = Ini::parse("[toliss]\nacp_pa_index = 7\n");
  loadSettingDefs(old, settingDefs(o));
  CHECK(o.acpPaIndex == 7);  // loading alone never migrates
  CHECK(migrate(old, o).size() == 1);
  CHECK(o.acpPaIndex == 15);
  // a current file keeps a 7 the user chose
  Options now;
  const Ini mine = Ini::parse(
      "[general]\nsettings_version = 2\n[toliss]\nacp_pa_index = 7\n");
  loadSettingDefs(mine, settingDefs(now));
  CHECK(migrate(mine, now).empty());
  CHECK(now.acpPaIndex == 7);

  Options swapped;
  swapped.a320 = {40, 30, 180};
  normalize(swapped);
  CHECK(swapped.a320.minMin == doctest::Approx(30));
  CHECK(swapped.a320.maxMin == doctest::Approx(40));
}

TEST_CASE(
    "recorder: traces from before 0.3.5 are upgraded through the ToLiss "
    "decoders") {
  // the 0.1.x layout (Run0): no aircraft, cue or ToLiss cross-check columns
  const std::string run0 =
      "kind,t,paused,replay,on_ground,gs_kt,ias_kt,agl_ft,msl_ft,vs_fpm,lat,"
      "lon,fmgs_phase_raw,cruise_alt_ft,beacon,"
      "seatbelt,landing_lights,speedbrake,park_brake,chocks,engines_running,"
      "engine_count,door_max,door_fwd,door_count,"
      "slides_armed,pax,fuel_kg,ac_power,pax_masks,cabin_alt_ft,pa_on,pa_vol,"
      "cab_on,cab_vol,cockpit_door,view_external,"
      "camera_in_cabin,sun_pitch,local_time,flight_number,acf_icao,satcom,"
      "event\n"
      "S,8.86,0,0,1,0.8,-0.1,-0,1471,-1,48.352232,11.777046,0,0,0,0,0,0.00,1,1,"
      "0,2,1.00,1.00,20,0,121,12084.8,0,0,0,0,0.00,1,0.40,0.00,0,0,12.9,55182,,"
      "A20N,0,\n"
      "E,112.69,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,BoardingWelcome "
      "playing: DLH/BoardingWelcome[Afternoon].ogg\n";
  const auto rows = readTraceCompat(run0);
  REQUIRE(rows.size() == 2);
  CHECK(rows[0].legacy);
  const SimState& a = rows[0].state;
  CHECK(a.onGround);
  CHECK(a.pax == 121);
  CHECK(a.doorFwd == doctest::Approx(1.0f));
  CHECK(a.fuelKg == doctest::Approx(12084.8f));
  CHECK(a.acfIcao == "A20N");
  CHECK(a.fmsPhaseRaw == 0);
  CHECK(a.fmsPhase == Phase::PreFlight);
  CHECK(a.interphoneOn);  // the old cab_on column
  CHECK(a.interphoneVol == doctest::Approx(0.4f));
  CHECK(a.cues.takeoffPower == -1);  // no column, unknown
  CHECK(a.cues.leversAtTakeoff == -1);
  CHECK(a.zuluSec == doctest::Approx(-1));
  CHECK(a.strobeOn == -1);
  CHECK(a.aircraftVolume == doctest::Approx(-1));
  CHECK(rows[1].isEvent);
  CHECK(rows[1].event ==
        "BoardingWelcome playing: DLH/BoardingWelcome[Afternoon].ogg");

  // the 0.3.x layout: ToLiss cross-checks and sound mix in their own columns,
  // the strobe missing, its raw value logged in an event row before
  const std::string v03 =
      "kind,t,on_ground,fmgs_phase_raw,acf_icao,ecam_phase,qpac_phase,"
      "throttle_max,ground_spoilers,toliss_vol_internal,toliss_vol_override,"
      "a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,u,v,w,x,y,z,aa,bb,cc,dd,ee,ff,gg,"
      "event\n"
      "E,1.00,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,raw "
      "AirbusFBW/OHPLightSwitches = 1.00 0.00 1.00 2.00 2.00 2.00 1.00 2.00\n"
      "S,2.00,1,1,A21N,3,4,0.88,0,0.50,1"
      ",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,\n";
  const auto t = readTraceCompat(v03);
  REQUIRE(t.size() == 2);
  const SimState& b = t[1].state;
  CHECK(b.fmsPhase == Phase::Takeoff);
  CHECK(b.cues.takeoffPower == 1);
  CHECK(b.cues.leversAtTakeoff == 1);
  CHECK(b.cues.phaseKnown);
  CHECK(b.groundSpoilers == 0);
  CHECK(b.aircraftVolume == doctest::Approx(0.5f));
  CHECK(b.strobeOn == 1);  // from the raw event row
  CHECK(t[1].extra.count("ecam_phase") == 1);

  // a current trace is left as it is
  SimState s;
  s.t = 3;
  s.cues.takeoffPower = 1;
  const auto now = readTraceCompat(std::string(traceHeader()) + "\n" +
                                   traceLine(s, "toliss") + "\n");
  REQUIRE(now.size() == 1);
  CHECK(!now[0].legacy);
  CHECK(now[0].state.cues.takeoffPower == 1);
  CHECK(!now[0].state.fmsPhase);
}
