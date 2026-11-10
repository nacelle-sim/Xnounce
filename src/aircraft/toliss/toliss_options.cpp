#include "aircraft/toliss/toliss_options.h"

#include <algorithm>

#include "core/strings.h"

namespace xn::toliss {

std::vector<SettingDef> settingDefs(Options& o) {
  using namespace setting;
  std::vector<SettingDef> boarding = {
      number("boarding", "a319_min", "A319 lead min (min)",
             "Boarding starts this long before the ETD with an empty cabin.",
             &o.a319.minMin, 0, 120),
      number("boarding", "a319_max", "A319 lead max (min)",
             "... and this long before it with a full cabin.", &o.a319.maxMin,
             0, 120),
      integer("boarding", "a319_seats", "A319 seats",
              "Seats when the SimBrief plan doesn't give them.", &o.a319.seats,
              1, 400),
      number("boarding", "a320_min", "A320 lead min (min)",
             "Boarding starts this long before the ETD with an empty cabin.",
             &o.a320.minMin, 0, 120),
      number("boarding", "a320_max", "A320 lead max (min)",
             "... and this long before it with a full cabin.", &o.a320.maxMin,
             0, 120),
      integer("boarding", "a320_seats", "A320 seats",
              "Seats when the SimBrief plan doesn't give them.", &o.a320.seats,
              1, 400),
      number("boarding", "a321_min", "A321 lead min (min)",
             "Boarding starts this long before the ETD with an empty cabin.",
             &o.a321.minMin, 0, 120),
      number("boarding", "a321_max", "A321 lead max (min)",
             "... and this long before it with a full cabin.", &o.a321.maxMin,
             0, 120),
      integer("boarding", "a321_seats", "A321 seats",
              "Seats when the SimBrief plan doesn't give them.", &o.a321.seats,
              1, 400),
  };
  std::vector<SettingDef> cockpit = {
      integer("toliss", "acp_pa_index", "ACP PA knob index",
              "Index of the PA reception knob in the ToLiss ACP arrays (see "
              "docs/aircraft/toliss.md).",
              &o.acpPaIndex, 0, 15),
      integer("toliss", "acp_cab_index", "ACP CAB knob index",
              "Index of the CAB (interphone) reception knob.", &o.acpCabIndex,
              0, 15),
      number("toliss", "cabin_view_aft_m", "Cabin view distance (m)",
             "The camera counts as in the cabin when it is this far aft of the "
             "pilot's eye point.",
             &o.cabinViewAftM, 0.5f, 20),
      number("toliss", "throttle_flx_detent", "Takeoff thrust lever",
             "A thrust lever at or above this (ToLiss throttle_input: 0.88 "
             "FLX/MCT, 1.0 TOGA) counts as takeoff thrust.",
             &o.throttleFlxDetent, 0.5f, 1.0f),
      integer("toliss", "landing_light_source", "Landing light source",
              "Which dataref says the landing lights are on (see "
              "docs/aircraft/toliss.md): 0 sim landing_lights_on, 1 sim "
              "landing_lights_switch, 2 ToLiss left/right extended.",
              &o.landingLightSource, 0, 2),
      integer("toliss", "strobe_switch_index", "Strobe switch index",
              "Entry of the strobe switch in AirbusFBW/OHPLightSwitches (0 "
              "off, 1 auto, 2 on; see docs/aircraft/toliss.md).",
              &o.strobeSwitchIndex, 0, 31),
      text("toliss", "volume_override_ref", "Volume override dataref",
           "Dataref that holds ToLiss's ISCS 'Ovrd XP Int/Ext Vol' switch, "
           "found by hand (docs/aircraft/toliss.md). Empty: unknown.",
           &o.volumeOverrideRef),
  };
  std::vector<SettingDef> all;
  for (SettingDef& d : boarding) {
    d.page = SettingPage::Boarding;
    all.push_back(std::move(d));
  }
  for (SettingDef& d : cockpit) {
    d.page = SettingPage::Aircraft;
    all.push_back(std::move(d));
  }
  for (SettingDef& d : all) {
    d.group = "toliss";
    d.groupName = "ToLiss";
  }
  return all;
}

void normalize(Options& o) {
  for (BoardingLead* lead : {&o.a319, &o.a320, &o.a321})
    if (lead->maxMin < lead->minMin) std::swap(lead->minMin, lead->maxMin);
}

std::vector<std::string> migrate(const Ini& ini, Options& o) {
  std::vector<std::string> notes;
  if (settingsVersionOf(ini) < 2) {
    // 0.2.0 shipped index 7 as the PA knob. It is the ILS knob.
    if (const std::string* v = ini.get("toliss", "acp_pa_index");
        v && trim(*v) == "7") {
      o.acpPaIndex = 15;
      notes.emplace_back(
          "[toliss] acp_pa_index 7 (ILS, the old default) -> 15 (PA)");
    }
  }
  return notes;
}

}  // namespace xn::toliss
