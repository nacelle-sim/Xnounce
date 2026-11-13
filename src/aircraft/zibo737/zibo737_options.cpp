/// @file
/// Implements `src/aircraft/zibo737/zibo737_options.h`; the interface is
/// documented there.
#include "aircraft/zibo737/zibo737_options.h"

#include <algorithm>

namespace xn::zibo737 {

std::vector<SettingDef> settingDefs(Options& o) {
  using namespace setting;
  struct Row {
    const char *min, *max, *seats, *minLabel, *maxLabel, *seatsLabel;
    BoardingLead* lead;
  };
  const Row rows[] = {
      {"b736_min", "b736_max", "b736_seats", "737-600 lead min (min)",
       "737-600 lead max (min)", "737-600 seats", &o.b736},
      {"b737_min", "b737_max", "b737_seats", "737-700 lead min (min)",
       "737-700 lead max (min)", "737-700 seats", &o.b737},
      {"b738_min", "b738_max", "b738_seats", "737-800 lead min (min)",
       "737-800 lead max (min)", "737-800 seats", &o.b738},
      {"b739_min", "b739_max", "b739_seats", "737-900 lead min (min)",
       "737-900 lead max (min)", "737-900 seats", &o.b739},
  };
  std::vector<SettingDef> all;
  for (const Row& r : rows) {
    for (SettingDef d :
         {number("boarding", r.min, r.minLabel,
                 "Boarding starts this long before the ETD with an empty "
                 "cabin.",
                 &r.lead->minMin, 0, 120),
          number("boarding", r.max, r.maxLabel,
                 "... and this long before it with a full cabin.",
                 &r.lead->maxMin, 0, 120),
          integer("boarding", r.seats, r.seatsLabel,
                  "Seats when neither the SimBrief plan nor the aircraft's "
                  "cabin layout gives them.",
                  &r.lead->seats, 1, 400)}) {
      d.page = SettingPage::Boarding;
      all.push_back(std::move(d));
    }
  }
  for (SettingDef d :
       {number("zibo737", "cabin_view_aft_m", "Cabin view distance (m)",
               "The camera counts as in the cabin when it is this far aft of "
               "the pilot's eye point.",
               &o.cabinViewAftM, 0.5f, 20),
        number("zibo737", "takeoff_lever", "Takeoff thrust lever",
               "A thrust lever at or above this ratio counts as takeoff "
               "thrust.",
               &o.takeoffLever, 0.5f, 1.0f),
        flag("zibo737", "act_doors", "Cabin crew: doors",
             "The cabin crew closes the doors (stowing the airstairs first) "
             "when boarding is complete, and opens the forward left door for "
             "deboarding. Off: Xnounce never moves a door. While any cabin "
             "crew action is on, the aircraft's own crew announcements are "
             "switched off as far as it allows.",
             &o.actDoors),
        flag("zibo737", "act_lights", "Cabin crew: cabin lights",
             "The cabin crew sets the cabin lights: white bright by day; at "
             "night dimmed for takeoff and landing, the night cruise preset "
             "in flight and white medium for the cabin service.",
             &o.actLights),
        choice("zibo737", "night_cruise_lights", "Night cruise lights",
               "The cabin lights in darkness between takeoff and landing, "
               "when the cabin crew sets the lights.",
               &o.nightCruiseLights, {"cruise", "night", "sleep"},
               {"Cruise", "Night", "Sleep"}),
        flag("zibo737", "act_chime", "Cabin crew: aircraft's chime",
             "The cabin's call to the cockpit sounds the aircraft's own "
             "chime. Only the Zibo has one a plugin can sound; the LevelUp "
             "keeps X-Plane's chime.",
             &o.actChime)}) {
    d.page = SettingPage::Aircraft;
    all.push_back(std::move(d));
  }
  for (SettingDef& d : all) {
    d.group = "zibo737";
    d.groupName = "Zibo / LevelUp";
  }
  return all;
}

void normalize(Options& o) {
  for (BoardingLead* lead : {&o.b736, &o.b737, &o.b738, &o.b739})
    if (lead->maxMin < lead->minMin) std::swap(lead->minMin, lead->maxMin);
}

}  // namespace xn::zibo737
