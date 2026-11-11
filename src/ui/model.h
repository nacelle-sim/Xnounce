/// @file
/// What the window shows and what its buttons do. The plugin fills it every
/// frame (app/ui_presenter.cpp). Plain data and `std::function` actions, so
/// the headless UI test can drive the window without X-Plane.
#pragma once

#include <algorithm>
#include <deque>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "core/log.h"
#include "core/pack_library.h"
#include "core/settings.h"

namespace xn::ui {

/// Everything the window shows.
struct UiModel {
  std::string version;
  /// Seconds of simulator time since the plugin last filled the model. The
  /// window counts its timers on by it, so countdowns and the playing
  /// position move every frame between the plugin's updates.
  double ageSec = 0;
  /// How long the plugin took to fill the model last time, in milliseconds
  /// (shown on the Log tab).
  double modelMs = 0;
  bool active = false;  ///< A supported aircraft is loaded; the purser runs.
  bool safeMode = false;
  bool oldPlugin = false;  ///< FXAnnounce is still installed (ARCHITECTURE
                           ///< § 5.1).
  std::string lastFault;
  bool muted = false;
  bool overwater = false;  ///< The overwater toggle of this flight.

  /// While dormant: the loaded aircraft's name for the Status card.
  std::string loadedAircraft;
  /// Which aircraft work: "Xnounce works with the ToLiss A319, A320 and
  /// A321."
  std::string supportedText;
  /// The same as a request: "Load a ToLiss A319, A320 or A321."
  std::string loadHint;

  // The Cabin card of the Status tab.
  std::string stage;
  bool calling = false;  ///< The cabin is calling the cockpit.
  int callAttempt = 0;
  /// How to take the call in this cockpit: "Pull CAB on the ACP to answer".
  std::string answerHint;
  std::string playing, playingFenix;
  std::string boardingNote;  ///< A shortened boarding, a clock mismatch, a
                             ///< hot start.
  float boardingFrac = -1;   ///< -1 when not boarding.
  double boardingLeftSec = -1;
  int boardingPax = 0;

  /// The Flight card of the Status tab.
  struct Flight {
    int state = 0;  ///< 0 no plan, 1 in use, 2 ignored.
    std::string callsign, origin, destination;
    std::string etd, boarding;
    std::string line;
    std::string airline;   ///< Empty when the airline table doesn't know it.
    float routeFrac = -1;  ///< Part of the great circle flown; -1 unknown.
    double toGoNm = -1;
    std::string reason;
  } flight;

  // The Aircraft card.
  std::string aircraft;
  std::string livery;
  std::string pack;  ///< The active pack, "none" or "scanning...".

  // The Timeline.
  struct Pending {
    std::string name, fenix;  ///< `fenix` is empty for milestones.
    double inSec = -1;        ///< -1 when it waits for `condition`.
    std::string condition;
    std::string note;
  };
  std::vector<Pending> pending;
  struct EventRow {
    double zuluSec = -1, localSec = -1;
    std::string name, fenix, outcome, detail;
    int tone = 0;  ///< 0 played, 1 a problem, 2 neutral.
  };
  std::vector<EventRow> events;  ///< Newest first.

  /// The Packs tab's Now Playing card.
  struct Now {
    std::string file;         ///< Empty when nothing plays.
    double pos = 0, dur = 0;  ///< Seconds.
    bool decoding = false, paused = false, interphone = false;
  } now;
  std::string music;
  std::string customTags;
  const PackLibrary* library = nullptr;  ///< Null until the first scan is
                                         ///< done.
  bool scanning = false;
  std::string packsDir;
  /// The explainer of the Packs tab (only with Show log on).
  std::function<std::vector<std::string>(int annType, int context)> explain;

  // The Log tab (only with Show log on).
  std::deque<LogLine> log;
  std::vector<std::pair<std::string, std::string>> liveInputs;

  std::string recorderPath;

  Settings* settings = nullptr;  ///< Edited in place by the Settings tab and
                                 ///< the titlebar.
  /// Every setting, shared and per aircraft, as the Settings tab shows them.
  const std::vector<SettingDef>* settingDefs = nullptr;
  /// The settings group and name of the loaded aircraft. Empty while
  /// dormant: the aircraft page then shows every aircraft's settings.
  std::string aircraftGroup;
  std::string aircraftName;

  // The actions. Any of them can be empty.
  std::function<void()> onSkip, onAdvance, onToggleMute, onToggleOverwater,
      onRescan, onReenable;
  std::function<void()> onSettingsChanged;  ///< Apply and save.
  std::function<void()> onResetSettings;    ///< Everything back to defaults.
};

/// A countdown of the model, counted on by `age` seconds. Zero ("now") and
/// negative values ("waiting for a condition") stay as they are.
inline double countdown(double sec, double age) {
  return sec > 0 ? std::max(0.0, sec - age) : sec;
}

/// The playing position of `now`, counted on by `age` seconds while it plays.
inline double nowPosition(const UiModel::Now& now, double age) {
  if (now.paused || now.decoding || now.dur <= 0) return now.pos;
  return std::min(now.dur, now.pos + age);
}

/// The boarding progress counted on by `age` seconds, from the progress and
/// the time left when the model was filled.
inline float boardingProgress(float frac, double leftSec, double age) {
  if (frac < 0 || frac >= 1 || leftSec <= 0) return frac;
  const double total = leftSec / (1.0 - frac);
  return static_cast<float>(
      std::clamp(1.0 - countdown(leftSec, age) / total, 0.0, 1.0));
}

/// State of the window itself, kept between frames.
struct UiState {
  int tab = 0;  ///< A tab id from panel.h.
  bool tabRestored = false;
  int settingsPage = 0;
  char logFilter[64] = {};
  char packFilter[64] = {};
  int explainType = 1;
  int explainContext = 0;
  std::vector<std::string> explainLines;
  /// How long the window takes to build a frame, in milliseconds, smoothed
  /// (shown on the Log tab).
  float frameMs = 0;
  float cardRowH[4] = {};  ///< Heights measured last frame: [2] the Aircraft
                           ///< card, [3] the band table's overhead.
};

}  // namespace xn::ui
