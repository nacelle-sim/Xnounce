/// @file
/// Implements `src/core/settings.h`; the interface is documented there.
#include "core/settings.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "core/strings.h"

namespace xn {

bool Settings::eventEnabled(std::string_view type) const {
  for (const auto& e : disabledEvents)
    if (iequals(e, type)) return false;
  return true;
}

BoardingStart Settings::boarding() const {
  return static_cast<BoardingStart>(std::clamp(boardingStart, 0, 2));
}

namespace setting {

namespace {
SettingDef base(const char* sec, const char* key, const char* label,
                const char* help, SettingType type, float lo = 0,
                float hi = 0) {
  SettingDef d;
  d.section = sec;
  d.key = key;
  d.label = label;
  d.help = help;
  d.type = type;
  d.min = lo;
  d.max = hi;
  return d;
}
}  // namespace

SettingDef flag(const char* sec, const char* key, const char* label,
                const char* help, bool* value, bool hidden) {
  SettingDef d = base(sec, key, label, help, SettingType::Bool);
  d.b = value;
  d.hidden = hidden;
  return d;
}

SettingDef integer(const char* sec, const char* key, const char* label,
                   const char* help, int* value, float lo, float hi,
                   bool hidden) {
  SettingDef d = base(sec, key, label, help, SettingType::Int, lo, hi);
  d.i = value;
  d.hidden = hidden;
  return d;
}

SettingDef number(const char* sec, const char* key, const char* label,
                  const char* help, float* value, float lo, float hi) {
  SettingDef d = base(sec, key, label, help, SettingType::Float, lo, hi);
  d.f = value;
  return d;
}

SettingDef text(const char* sec, const char* key, const char* label,
                const char* help, std::string* value) {
  SettingDef d = base(sec, key, label, help, SettingType::String);
  d.s = value;
  return d;
}

SettingDef choice(const char* sec, const char* key, const char* label,
                  const char* help, int* value, std::vector<const char*> names,
                  std::vector<const char*> labels) {
  SettingDef d = base(sec, key, label, help, SettingType::Int, 0,
                      static_cast<float>(names.size() - 1));
  d.i = value;
  d.choices = std::move(names);
  d.choiceLabels = std::move(labels);
  return d;
}

}  // namespace setting

namespace {

/// The shared settings in the order they are written to a new ini file and
/// shown on their page.
std::vector<SettingDef> buildDefs(Settings& s) {
  using namespace setting;
  return {
      integer("general", "settings_version", "", "", &s.settingsVersion, 0, 0,
              true),
      flag("general", "enabled", "Announcements enabled",
           "Master switch for automatic announcements.", &s.enabled),
      number("general", "master_volume", "Master volume",
             "Scales every sound Xnounce plays.", &s.masterVolume, 0, 2),
      text("general", "pack_paths", "Extra pack folders",
           "Additional folders that contain Airline Packs, separated by ';'. "
           "Resources/plugins/AirlinePacks is always scanned.",
           &s.packPaths),
      flag(
          "ui", "show_log", "Show log",
          "Shows the Log tab (with the live sim inputs) and, on the Packs tab, "
          "what would play "
          "now and why.",
          &s.showLog),
      integer("general", "log_level", "Log detail",
              "0 errors, 1 warnings, 2 info, 3 debug (Xnounce.log).",
              &s.logLevel, 0, 3),
      flag("general", "recorder", "Record flight trace",
           "Writes the sim state and purser events to "
           "Output/Xnounce/*.xntrace.csv for debugging.",
           &s.recorder),

      number("hearing", "pa_volume", "PA volume", "Cabin announcement volume.",
             &s.paVolume, 0, 2),
      number("hearing", "music_volume", "Boarding music volume",
             "Boarding music level in the cabin.", &s.musicVolume, 0, 2),
      number("hearing", "interphone_volume", "Interphone volume",
             "Cabin-to-cockpit call level (times the CAB knob).",
             &s.interphoneVolume, 0, 2),
      number("hearing", "chime_volume", "Cockpit call buzzer",
             "Level of the buzzer when the cabin calls the cockpit.",
             &s.chimeVolume, 0, 2),
      number("hearing", "exterior_gain", "Exterior level",
             "Level of cabin PA in external views.", &s.exteriorGain, 0, 1),
      flag("hearing", "realistic", "Realistic hearing",
           "On: cabin PA is heard muffled through the closed cockpit door, "
           "clear "
           "through the open door, the ACP PA knob "
           "or from the cabin. Off: always at full PA volume.",
           &s.realisticHearing),
      flag(
          "hearing", "muffle", "Muffle through the door",
          "On: through the closed door the PA sounds muffled (low-passed), and "
          "clear when the door opens, the PA knob is "
          "pulled or you are in the cabin. Off: one unfiltered sound, never "
          "below the closed-door level.",
          &s.muffle),
      number("hearing", "floor", "Through the closed door",
             "Level of cabin PA in the cockpit with the door closed and the PA "
             "knob "
             "in. Announcements never go below it.",
             &s.hearingFloor, 0, 1),
      number("hearing", "door_leak", "Cockpit door leak",
             "Level heard through a fully open cockpit door.", &s.doorLeak, 0,
             1),
      number(
          "hearing", "muffle_cutoff_hz", "Muffle cutoff (Hz)",
          "Low-pass cutoff of the muffled sound. Lower is duller. Applies to "
          "sounds loaded after the change.",
          &s.muffleCutoffHz, 200, 4000),
      flag("hearing", "use_toliss_door_blend", "Door leak from ToLiss",
           "Use ToLiss's own cockpit-door sound blend for the door leak when "
           "the "
           "aircraft provides it (otherwise the door "
           "angle).",
           &s.useAircraftDoorBlend),
      flag(
          "hearing", "follow_toliss_volume", "Follow ToLiss volume",
          "When ToLiss's ISCS 'Ovrd XP Int/Ext Vol' is on, scale every cabin "
          "sound by ToLiss's internal volume. Needs "
          "the override dataref (Settings > ToLiss > Volume override dataref).",
          &s.followAircraftVolume),
      choice("hearing", "pa_system", "PA system",
             "How the cabin loudspeakers sound. Modern plays the recordings "
             "as they are. Worn and Old add a small speaker's tinny sound, "
             "hiss, hum, crackle and, on Old, dropouts and a burst of "
             "feedback. Random picks one per airframe (by tail number), so "
             "each aircraft keeps its PA. Applies to sounds loaded after the "
             "change.",
             &s.paPreset, {"modern", "worn", "old", "random"},
             {"Modern", "Worn", "Old", "Random per airframe"}),
      flag("hearing", "pa_cutout", "Cut out on power transfer",
           "The PA and the music go silent for one to two seconds when the "
           "aircraft's AC power moves to another source (ground power, APU, "
           "engine generators) or is lost for a moment, as on the real "
           "aircraft.",
           &s.paCutout),
      integer("hearing", "pa_bus", "PA audio bus",
              "X-Plane audio bus for cabin PA and music. 7 = interior "
              "(default), 8 = "
              "UI.",
              &s.paBus, 0, 10),
      integer(
          "hearing", "interphone_bus", "Interphone audio bus",
          "X-Plane audio bus for the interphone and the call buzzer. 2 = pilot "
          "headset (default), 7 = interior.",
          &s.interphoneBus, 0, 10),

      choice("cabin", "boarding_start", "Boarding starts",
             "SimBrief schedule: at the ETD of the simbrief_hub plan minus the "
             "lead "
             "time (Boarding section); after a late "
             "load or without a valid plan, when a door has been open at the "
             "gate. "
             "Doors open: when a passenger door has "
             "been open at the gate. Cabin call only: when you press FWD, AFT, "
             "ALL... on the overhead CALLS panel. A cabin "
             "call at the gate always starts boarding.",
             &s.boardingStart, {"schedule", "doors", "call"},
             {"SimBrief schedule", "Doors open", "Cabin call only"}),
      number("cabin", "expedite_s", "Cabin call expedites check to (s)",
             "A cabin call after ArmDoors or after the descent starts, or an "
             "early "
             "line-up or seats-for-landing signal, "
             "shortens the cabin safety check to this (Fenix).",
             &s.expediteSeconds, 0, 120),
      number("cabin", "boarding_expedite_s", "Cabin call ends boarding in (s)",
             "A cabin call during boarding completes it this soon.",
             &s.boardingExpediteS, 0, 600),
      flag(
          "cabin", "turnaround", "Turnaround",
          "On: after deboarding the purser prepares the next leg at this gate. "
          "Off: the flight is complete after "
          "deboarding; a new SimBrief OFP from this airport, a situation load "
          "or "
          "an aircraft reload starts the next one.",
          &s.turnaround),
      number("boarding", "jitter_s", "Lead time jitter (s)",
             "Random +/- added once per flight.", &s.leadJitterS, 0, 600),
      number("boarding", "ofp_origin_nm", "OFP origin distance (NM)",
             "A SimBrief plan is used only when its origin airport is this "
             "close to "
             "the aircraft.",
             &s.ofpOriginNm, 1, 50),
      number(
          "boarding", "ofp_return_nm", "Returning to the origin (NM)",
          "In flight, the Status shows 'returning' once the aircraft is this "
          "much closer to the OFP origin than at its "
          "farthest and this much farther from the destination than at its "
          "closest.",
          &s.ofpReturnNm, 1, 200),
      number("boarding", "end_before_etd_s", "Boarding complete before ETD (s)",
             "With a SimBrief plan, boarding aims to complete this long before "
             "the "
             "ETD (shortened when the ETD is close).",
             &s.endBeforeEtdS, 0, 3600),
      number(
          "boarding", "max_wait_min",
          "Longest wait for the boarding time (min)",
          "A SimBrief boarding time further ahead than this means the X-Plane "
          "clock isn't set to the flight's time: "
          "the doors rule is used instead.",
          &s.maxWaitMin, 5, 720),

      text("tags", "satcom_tags", "SatCom custom tags",
           "Custom tags switched on when the ToLiss SatCom antenna is fitted "
           "(comma-separated).",
           &s.satcomTags),
      flag("tags", "overwater_default", "Overwater by default",
           "Starting value of the per-flight Overwater toggle when the livery "
           "doesn't set it.",
           &s.overwaterDefault),

      number("timing", "check_multiplier", "Cabin check duration x",
             "Scales every simulated cabin check.", &s.checkMultiplier, 0.1f,
             3),
      number("timing", "welcome_min_s", "Welcome interval min (s)",
             "Shortest gap between boarding welcomes.", &s.welcomeMin, 30, 900),
      number("timing", "welcome_max_s", "Welcome interval max (s)",
             "Longest gap between boarding welcomes.", &s.welcomeMax, 30, 1200),
      number(
          "timing", "boarding_door_settle_s", "Boarding start delay (s)",
          "Doors rule: boarding starts once there has been AC power this long "
          "with a door open (or opened earlier at "
          "this gate).",
          &s.boardingDoorSettle, 0, 120),
      number(
          "timing", "boarding_base_s", "Boarding base time (s)",
          "Boarding takes this plus the per-passenger time, times the aircraft "
          "type factor (+/- 15 %).",
          &s.boardingBaseS, 0, 3600),
      number("timing", "boarding_per_pax_s", "Boarding per passenger (s)",
             "Boarding time per passenger.", &s.boardingPerPaxS, 0, 60),
      number(
          "timing", "boarding_min_s", "Shortest boarding (s)",
          "Boarding never takes less than this, even when the SimBrief ETD is "
          "close or has passed.",
          &s.boardingMinS, 30, 3600),
      number("timing", "arm_doors_wait_s", "Arm doors wait (s)",
             "How long ArmDoors waits for open doors to close.",
             &s.armDoorsWait, 0, 600),
      number(
          "timing", "presafety_delay_s", "Safety briefing after ArmDoors (s)",
          "The safety briefing never starts sooner than this after ArmDoors.",
          &s.preSafetyDelay, 0, 600),
      number(
          "timing", "presafety_after_move_min_s",
          "Safety briefing after pushback min (s)",
          "The safety briefing starts this long after the aircraft first moves "
          "once boarding is complete (minimum).",
          &s.preSafetyAfterMoveMin, 0, 600),
      number("timing", "presafety_after_move_max_s",
             "Safety briefing after pushback max (s)", "... (maximum).",
             &s.preSafetyAfterMoveMax, 0, 600),
      number("timing", "ns_flick_max_s", "NO SMOKING flick (s)",
             "NO SMOKING switched off and on again within this time is the "
             "pilots' "
             "signal to the cabin.",
             &s.nsFlickMaxS, 1, 60),
      number("timing", "chain_gap_s", "Gap between calls (s)",
             "Pause between announcements that follow each other.", &s.chainGap,
             0, 30),
      number("timing", "after_takeoff_min_s", "After takeoff min (s)",
             "Delay after acceleration altitude (minimum).", &s.afterTakeoffMin,
             0, 600),
      number("timing", "after_takeoff_max_s", "After takeoff max (s)",
             "Delay after acceleration altitude (maximum).", &s.afterTakeoffMax,
             0, 900),
      number("timing", "cabin_service_min_s", "Cabin service min (s)",
             "Delay after top of climb (minimum).", &s.cabinServiceMin, 0,
             3600),
      number("timing", "cabin_service_max_s", "Cabin service max (s)",
             "Delay after top of climb (maximum).", &s.cabinServiceMax, 0,
             3600),
      number("timing", "descent_min_s", "Descent call min (s)",
             "Delay after the descent starts (minimum).", &s.descentMin, 0,
             900),
      number("timing", "descent_max_s", "Descent call max (s)",
             "Delay after the descent starts (maximum).", &s.descentMax, 0,
             900),
      number(
          "timing", "landing_ready_agl_ft",
          "Cabin secured for landing below (ft AGL)",
          "The landing CabinSecured call waits for the approach phase or this "
          "height.",
          &s.landingReadyAglFt, 1000, 20000),
      number("timing", "landing_drop_agl_ft",
             "Drop landing calls below (ft AGL)",
             "Pending landing calls are dropped below this.",
             &s.landingDropAglFt, 0, 5000),
      number("timing", "after_landing_fallback_s", "After landing fallback (s)",
             "If spoilers or landing lights stay out, the after-landing call "
             "comes "
             "this long after touchdown.",
             &s.afterLandingFallback, 10, 900),
      number(
          "timing", "deboard_fallback_s", "Deboarding fallback (s)",
          "Deboarding starts this long after DisarmDoors when no door opens.",
          &s.deboardFallback, 10, 900),
      number("timing", "deboard_per_pax_s", "Deboarding per passenger (s)",
             "Deboarding time per passenger.", &s.deboardPerPax, 0, 10),
      number("timing", "deboard_min_s", "Deboarding minimum (s)",
             "Shortest deboarding.", &s.deboardMin, 0, 1800),
      number("timing", "turnaround_gap_s", "Turnaround gap (s)",
             "After deboarding, the next boarding starts this long later (if a "
             "door "
             "has been open since), or when a door "
             "closes and opens again.",
             &s.turnaroundGap, 0, 3600),
      number("timing", "seatbelt_debounce_s", "Seatbelt sign debounce (s)",
             "The sign must hold its new state this long.", &s.seatbeltDebounce,
             0, 60),
      number("timing", "interphone_retry_s", "Cabin call retry (s)",
             "Time before an unanswered cabin call buzzes again.",
             &s.interphoneRetry, 5, 120),
      integer("timing", "interphone_retries", "Cabin call retries",
              "Retries before the cabin gives up.", &s.interphoneRetries, 0,
              10),
      number("timing", "refuel_rate_kgs", "Refuel detection (kg/s)",
             "Fuel rising faster than this on the ground = refuelling.",
             &s.refuelRateKgs, 0.1f, 50),
      number("timing", "refuel_max_kgs", "Refuel detection max (kg/s)",
             "Faster rises are instant fuel loads (ISCS), not refuelling.",
             &s.refuelMaxKgs, 5, 2000),
      number("timing", "no_fmgs_after_s", "No-FMGS phase estimate after (s)",
             "If the FMGS phase still reads PreFlight this long after "
             "lift-off, the "
             "phase is estimated.",
             &s.fmsEstimateAfterS, 5, 600),
      number("timing", "dark_sun_deg", "Darkness below sun (deg)",
             "Sun elevation below which the cabin is dimmed.", &s.darkSunDeg,
             -18, 10),

      flag("ui", "visible", "", "", &s.windowVisible, true),
      integer("ui", "win_left", "", "", &s.winLeft, 0, 0, true),
      integer("ui", "win_top", "", "", &s.winTop, 0, 0, true),
      integer("ui", "win_right", "", "", &s.winRight, 0, 0, true),
      integer("ui", "win_bottom", "", "", &s.winBottom, 0, 0, true),
      integer("ui", "tab", "", "", &s.tab, 0, 0, true),
      choice("ui", "theme", "Theme",
             "Dark or White (also the switch in the titlebar).", &s.theme,
             {"dark", "white"}, {"Dark", "White"}),
      number("ui", "font_scale", "Text size", "Scales all text in the window.",
             &s.fontScale, 0.6f, 2.0f),
      flag("ui", "color_blind", "Colour-blind colours",
           "Okabe-Ito colours for played / problem / error.", &s.colorBlind),
      flag("ui", "show_timeline", "Show Timeline",
           "Shows the Timeline tab (also the checkbox in the titlebar).",
           &s.showTimeline),
  };
}

/// The Settings page of a shared setting. Pages mostly follow the ini
/// sections; the exceptions are listed here (docs/ARCHITECTURE.md § 7).
SettingPage pageFor(std::string_view sec, std::string_view key) {
  if (sec == "general") {
    if (key == "enabled") return SettingPage::Announcements;
    if (key == "master_volume") return SettingPage::Sound;
    return SettingPage::Advanced;
  }
  if (sec == "hearing")
    return key == "pa_bus" || key == "interphone_bus" ? SettingPage::Advanced
                                                      : SettingPage::Sound;
  if (sec == "cabin") {
    if (key == "expedite_s") return SettingPage::Timing;
    if (key == "boarding_expedite_s") return SettingPage::Boarding;
    return SettingPage::Announcements;
  }
  if (sec == "boarding") return SettingPage::Boarding;
  if (sec == "tags") return SettingPage::Announcements;
  if (sec == "timing") {
    for (const char* k :
         {"welcome_min_s", "welcome_max_s", "boarding_door_settle_s",
          "boarding_base_s", "boarding_per_pax_s", "boarding_min_s"})
      if (key == k) return SettingPage::Boarding;
    return SettingPage::Timing;
  }
  if (sec == "ui")
    return key == "show_log" ? SettingPage::Advanced : SettingPage::Display;
  return SettingPage::Advanced;
}

std::string formatFloat(float v) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.4g", static_cast<double>(v));
  return buf;
}

}  // namespace

const char* settingPageName(SettingPage p) {
  static const char* names[kSettingPageCount] = {
      "Announcements", "Sound",   "Boarding", "Timing",
      "Aircraft",      "Display", "Advanced"};
  const int k = static_cast<int>(p);
  return k >= 0 && k < kSettingPageCount ? names[k] : "?";
}

std::vector<SettingDef> coreSettingDefs(Settings& s) {
  std::vector<SettingDef> v = buildDefs(s);
  for (SettingDef& d : v) d.page = pageFor(d.section, d.key);
  return v;
}

void loadSettingDefs(const Ini& ini, const std::vector<SettingDef>& defs) {
  for (const SettingDef& d : defs) {
    const std::string* v = ini.get(d.section, d.key);
    if (!v) continue;
    switch (d.type) {
      case SettingType::Bool:
        *d.b = parseBool(*v, *d.b);
        break;
      case SettingType::Int: {
        // A named value first, then a number.
        bool named = false;
        for (size_t k = 0; k < d.choices.size(); ++k)
          if (iequals(trim(*v), d.choices[k])) {
            *d.i = static_cast<int>(k);
            named = true;
          }
        if (named) break;
        char* end = nullptr;
        long n = std::strtol(v->c_str(), &end, 10);
        if (end != v->c_str()) {
          if (d.max > d.min)
            n = std::clamp<long>(n, static_cast<long>(d.min),
                                 static_cast<long>(d.max));
          *d.i = static_cast<int>(n);
        }
        break;
      }
      case SettingType::Float: {
        char* end = nullptr;
        double x = std::strtod(v->c_str(), &end);
        if (end != v->c_str() && std::isfinite(x)) {
          float fx = static_cast<float>(x);
          if (d.max > d.min) fx = std::clamp(fx, d.min, d.max);
          *d.f = fx;
        }
        break;
      }
      case SettingType::String:
        *d.s = *v;
        break;
    }
  }
}

void storeSettingDefs(const std::vector<SettingDef>& defs, Ini& ini) {
  for (const SettingDef& d : defs) {
    switch (d.type) {
      case SettingType::Bool:
        ini.set(d.section, d.key, *d.b ? "yes" : "no");
        break;
      case SettingType::Int:
        if (!d.choices.empty())
          ini.set(d.section, d.key,
                  d.choices[static_cast<size_t>(std::clamp<int>(
                      *d.i, 0, static_cast<int>(d.choices.size()) - 1))]);
        else
          ini.set(d.section, d.key, std::to_string(*d.i));
        break;
      case SettingType::Float:
        ini.set(d.section, d.key, formatFloat(*d.f));
        break;
      case SettingType::String:
        ini.set(d.section, d.key, *d.s);
        break;
    }
  }
}

void loadSettings(const Ini& ini, Settings& s) {
  loadSettingDefs(ini, coreSettingDefs(s));
  // 0.1.x had a yes/no key instead of the boarding start mode.
  if (!ini.get("cabin", "boarding_start"))
    if (const std::string* v = ini.get("cabin", "boarding_on_call"))
      s.boardingStart = static_cast<int>(
          parseBool(*v, true) ? BoardingStart::Schedule : BoardingStart::Doors);
  if (const std::string* v = ini.get("events", "disabled")) {
    s.disabledEvents.clear();
    for (auto& e : splitList(*v)) s.disabledEvents.insert(e);
  }
  s.fontScale = std::clamp(s.fontScale, 0.6f, 2.0f);
  // Tab 4 was the Inspector, removed in 0.3.0.
  if (s.tab == 4 || s.tab < 0 || s.tab > 5) s.tab = 0;
  if (s.welcomeMax < s.welcomeMin) std::swap(s.welcomeMin, s.welcomeMax);
  if (s.refuelMaxKgs < s.refuelRateKgs) s.refuelMaxKgs = s.refuelRateKgs;
}

int settingsVersionOf(const Ini& ini) {
  if (const std::string* v = ini.get("general", "settings_version"))
    return std::atoi(v->c_str());
  return 1;
}

std::vector<std::string> migrateSettings(const Ini& ini, Settings& s) {
  std::vector<std::string> notes;
  if (settingsVersionOf(ini) < 2) {
    // 0.2.0 shipped 0.25 as the closed-door level. A file that still has it
    // gets the level that goes with the muffled voice.
    if (const std::string* v = ini.get("hearing", "floor");
        v && std::fabs(std::strtod(v->c_str(), nullptr) - 0.25) < 1e-6) {
      s.hearingFloor = 0.10f;
      notes.emplace_back(
          "[hearing] floor 0.25 (the old default) -> 0.1 "
          "(through the closed door, muffled)");
    }
  }
  s.settingsVersion = kSettingsVersion;
  return notes;
}

void storeSettings(const Settings& s, Ini& ini) {
  // The definitions point at variables they may change, so they are bound to
  // a copy.
  Settings copy = s;
  storeSettingDefs(coreSettingDefs(copy), ini);
  ini.set("events", "disabled",
          join(std::vector<std::string>(s.disabledEvents.begin(),
                                        s.disabledEvents.end()),
               ","));
}

}  // namespace xn
