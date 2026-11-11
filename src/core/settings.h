/// @file
/// The user's settings and how they are stored.
///
/// `Settings` holds every setting that applies to all aircraft. Settings that
/// only one aircraft has (which knob index, which switch) belong to that
/// aircraft's module (`aircraft/options.h`).
///
/// One list of `SettingDef` describes each setting once: its ini section and
/// key, the label and help text of the Settings tab, its range and the
/// variable that holds it. Loading, saving and the Settings tab all walk that
/// list, so a new setting is one new entry.
///
/// Values marked *(tunable)* in docs/VP_SPEC.md are these settings.
#pragma once

#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "core/ini.h"

namespace xn {

/// What starts boarding (VP_SPEC § 4.1a). Stored by name in the ini.
enum class BoardingStart : int {
  /// At the SimBrief boarding time. A late load or an unusable flight plan
  /// falls back to the doors rule.
  Schedule = 0,
  /// When a passenger door has been open at the gate.
  Doors = 1,
  /// Only on a cabin call from the cockpit.
  Call = 2,
};

/// Version of the settings layout, written to `[general] settings_version`.
/// Raise it when a default or a key changes its meaning, and upgrade old
/// files in `migrateSettings`.
constexpr int kSettingsVersion = 2;

/// Settings shared by all aircraft. Times are seconds unless the name says
/// otherwise.
struct Settings {
  // General
  int settingsVersion = kSettingsVersion;
  bool enabled = true;
  float masterVolume = 1.0f;
  /// Extra folders with Airline Packs, separated by ';'.
  std::string packPaths;
  int logLevel = 2;  ///< 0 errors, 1 warnings, 2 info, 3 debug.
  bool recorder = false;

  // Hearing the cabin in the cockpit (VP_SPEC § 4.5)
  bool realisticHearing = true;
  float paVolume = 1.0f;
  /// Level of the muffled voice through the closed cockpit door.
  float hearingFloor = 0.10f;
  /// Level of the clear voice through a fully open cockpit door.
  float doorLeak = 0.6f;
  /// Low-pass the cabin behind the closed door. Off: one plain voice that
  /// never goes below `hearingFloor`.
  bool muffle = true;
  float muffleCutoffHz = 800;
  float exteriorGain = 0.0f;
  float musicVolume = 0.6f;
  float interphoneVolume = 1.0f;
  /// Level of the cockpit call buzzer. The ini key still says "chime".
  float chimeVolume = 0.8f;
  /// How the cabin PA sounds (VP_SPEC § 4.6): 0 Modern, 1 Worn, 2 Old,
  /// 3 Random per airframe.
  int paPreset = 0;
  /// The PA and the music drop out for a moment on an AC power transfer.
  bool paCutout = true;
  int paBus = 7;          ///< X-Plane audio bus: 7 is the interior.
  int interphoneBus = 2;  ///< X-Plane audio bus: 2 is the pilot headset.
  /// Take the door opening from the aircraft's own door sound blend when it
  /// has one.
  bool useAircraftDoorBlend = true;
  /// Scale every sound by the aircraft's own interior volume when the
  /// aircraft asks for that.
  bool followAircraftVolume = true;

  // Tags (VP_SPEC § 3.1)
  /// Custom tags switched on when a satellite antenna is fitted (D-5).
  std::string satcomTags = "WiFi,USB";
  bool overwaterDefault = false;

  // Cabin
  /// A `BoardingStart` value. A cabin call starts boarding in every mode.
  int boardingStart = static_cast<int>(BoardingStart::Schedule);
  /// A cabin call shortens the cabin check to this (VP_SPEC § 4.3a).
  float expediteSeconds = 15;
  /// A cabin call during boarding completes it this soon (D-21).
  float boardingExpediteS = 60;
  /// Prepare the next leg after deboarding (D-26).
  bool turnaround = false;

  // SimBrief schedule (VP_SPEC § 4.1a, § 4.2a)
  /// Random change of the boarding lead time, rolled once per flight.
  float leadJitterS = 60;
  /// The flight plan is used when its origin is this close.
  float ofpOriginNm = 5;
  /// How far the aircraft must turn back before Status says "returning".
  float ofpReturnNm = 10;
  /// Boarding aims to complete this long before the ETD (VP_SPEC § 4.1b).
  float endBeforeEtdS = 600;
  /// A boarding time further ahead than this means X-Plane's clock isn't
  /// set to the flight's time.
  float maxWaitMin = 60;

  // Timing (VP_SPEC § 4.1)
  float checkMultiplier = 1.0f;
  float welcomeMin = 120, welcomeMax = 360;
  float boardingDoorSettle = 15;
  /// Boarding lasts base + per passenger, times the type factor (§ 4.1b).
  float boardingBaseS = 300;
  float boardingPerPaxS = 6;
  float boardingMinS = 300;
  float armDoorsWait = 120;
  /// The safety briefing starts no sooner than this after ArmDoors (D-22)...
  float preSafetyDelay = 60;
  /// ...and this long after the aircraft first moves.
  float preSafetyAfterMoveMin = 20, preSafetyAfterMoveMax = 40;
  /// Longest time the NO SMOKING signs are off in a "flick" (VP_SPEC § 4.2).
  float nsFlickMaxS = 10;
  float chainGap = 4;
  float afterTakeoffMin = 20, afterTakeoffMax = 60;
  float cabinServiceMin = 180, cabinServiceMax = 600;
  float descentMin = 30, descentMax = 90;
  float landingReadyAglFt = 10000;
  float landingDropAglFt = 1000;
  float afterLandingFallback = 120;
  float deboardFallback = 90;
  float deboardPerPax = 1.2f;
  float deboardMin = 60;
  float turnaroundGap = 300;
  float seatbeltDebounce = 5;
  float interphoneRetry = 30;
  int interphoneRetries = 3;
  float refuelRateKgs = 2.0f;
  /// A faster rise is an instant fuel load, not refuelling.
  float refuelMaxKgs = 150.0f;
  float darkSunDeg = 0.0f;
  /// The flight phase is estimated when the FMS still reads PreFlight this
  /// long after lift-off (D-20).
  float fmsEstimateAfterS = 30;

  /// Announcement types the user switched off, by their Fenix names.
  std::set<std::string> disabledEvents;

  // Window
  bool windowVisible = false;
  int winLeft = 0, winTop = 0, winRight = 0, winBottom = 0;
  float fontScale = 1.0f;
  /// Tab id as in docs/ARCHITECTURE.md § 7. Id 4 was the Inspector.
  int tab = 0;
  bool colorBlind = false;
  int theme = 0;  ///< 0 Dark, 1 White.
  bool showTimeline = true;
  bool showLog = false;

  /// False when the user switched this announcement type off.
  bool eventEnabled(std::string_view type) const;
  /// `boardingStart` as the enum, clamped to a valid value.
  BoardingStart boarding() const;
};

enum class SettingType { Bool, Int, Float, String };

/// The pages of the Settings tab. They are independent of the ini sections.
/// `Aircraft` shows the settings of the loaded aircraft's module.
enum class SettingPage {
  Announcements,
  Sound,
  Boarding,
  Timing,
  Aircraft,
  Display,
  Advanced,
  Count
};
constexpr int kSettingPageCount = static_cast<int>(SettingPage::Count);
/// The page's name in the sidebar. The Aircraft page is shown under the
/// loaded aircraft's own name instead (docs/ARCHITECTURE.md § 7).
const char* settingPageName(SettingPage p);

/// One setting: where it is stored, how the Settings tab shows it and the
/// variable that holds it.
struct SettingDef {
  const char* section = "";
  const char* key = "";
  const char* label = "";
  const char* help = "";
  SettingType type = SettingType::Bool;
  /// Range of a number. Not clamped when `max <= min`.
  float min = 0, max = 0;
  /// The variable. Exactly one pointer is set, the one that matches `type`.
  /// It points into a `Settings` or into an aircraft module's options and
  /// must outlive the definition.
  bool* b = nullptr;
  int* i = nullptr;
  float* f = nullptr;
  std::string* s = nullptr;
  /// Stored but not shown: the window position and the like.
  bool hidden = false;
  SettingPage page = SettingPage::Advanced;
  /// For a setting of one aircraft: the id of its module ("toliss") and the
  /// name to show for it ("ToLiss"). Empty for shared settings.
  const char* group = "";
  const char* groupName = "";
  /// For an integer with named values: the names stored in the ini and the
  /// labels of the combo box. The value is the index.
  std::vector<const char*> choices;
  std::vector<const char*> choiceLabels;
};

/// Builders for `SettingDef`, one per kind of value. An aircraft module uses
/// them to declare its own settings.
namespace setting {
SettingDef flag(const char* sec, const char* key, const char* label,
                const char* help, bool* value, bool hidden = false);
SettingDef integer(const char* sec, const char* key, const char* label,
                   const char* help, int* value, float lo, float hi,
                   bool hidden = false);
SettingDef number(const char* sec, const char* key, const char* label,
                  const char* help, float* value, float lo, float hi);
SettingDef text(const char* sec, const char* key, const char* label,
                const char* help, std::string* value);
SettingDef choice(const char* sec, const char* key, const char* label,
                  const char* help, int* value, std::vector<const char*> names,
                  std::vector<const char*> labels);
}  // namespace setting

/// The definitions of the shared settings, bound to `s`.
std::vector<SettingDef> coreSettingDefs(Settings& s);

/// Reads every defined value that the ini has. Unknown keys are ignored, a
/// value that can't be read keeps the variable as it is, numbers are clamped
/// to their range.
void loadSettingDefs(const Ini& ini, const std::vector<SettingDef>& defs);
/// Writes every defined value. Other content of the ini is kept.
void storeSettingDefs(const std::vector<SettingDef>& defs, Ini& ini);

/// Loads the shared settings, including the switched-off announcements and
/// the keys of older versions, and repairs values that contradict each other.
void loadSettings(const Ini& ini, Settings& s);
/// Stores the shared settings. Other content of the ini is kept.
void storeSettings(const Settings& s, Ini& ini);
/// Replaces defaults that an older version got wrong, in a file that still
/// has them. Call it once after loading a whole file. Returns a line for the
/// log for each value it changed.
std::vector<std::string> migrateSettings(const Ini& ini, Settings& s);
/// The `settings_version` an ini was written with. Files from before 0.2.1
/// have none and count as 1.
int settingsVersionOf(const Ini& ini);

}  // namespace xn
