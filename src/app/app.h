/// @file
/// The plugin between XPluginEnable and XPluginDisable.
///
/// `plugin.cpp` forwards X-Plane's entry points here. `App` owns the parts
/// and runs them in order each tick (docs/ARCHITECTURE.md § 2):
///
///     simulator -> aircraft module -> SimState -> purser -> player
///
/// The work itself is in the parts: `PackService` (Airline Packs),
/// `FlightPlanService` (SimBrief), `Purser` (when to announce what), `Player`
/// (audio), `Commands` (menu and commands), `SettingsStore`, and the
/// presenter that fills the window's model. `App` decides which aircraft
/// module is active and when the flight starts over.
///
/// Everything here runs on X-Plane's main thread.
#pragma once

#include <XPLMProcessing.h>

#include <map>
#include <memory>
#include <string>

#include "aircraft/registry.h"
#include "aircraft/xp_common.h"
#include "app/commands.h"
#include "app/flight_plan_service.h"
#include "app/pack_service.h"
#include "app/settings_store.h"
#include "audio/decode_cache.h"
#include "audio/player.h"
#include "core/purser/purser.h"
#include "core/recorder.h"
#include "core/refuel.h"
#include "core/sim_state.h"
#include "sim/simbrief_source.h"
#include "ui/model.h"

namespace xn {

class App {
 public:
  static App& get();
  /// Runs when the library is unloaded. It never calls into X-Plane
  /// (docs/ARCHITECTURE.md § 4, rule 7).
  ~App();

  // X-Plane's entry points.
  void start();    ///< XPluginStart: paths, the log, the settings.
  void enable();   ///< XPluginEnable: flight loops, menu, audio, first scan.
  void disable();  ///< XPluginDisable: releases everything X-Plane owns.
  void stop();     ///< XPluginStop.
  void onMessage(int msg, void* param);

  // What the menu, the commands and the window's buttons do.
  void toggleWindow();
  void skip();     ///< Stops the announcement that is playing.
  void advance();  ///< Fires the next pending purser event now.
  void toggleMute();
  void toggleOverwater();
  void rescan();    ///< Scans the Airline Packs again.
  void reenable();  ///< Leaves safe mode after repeated faults.

  // Flight loop bodies. They return X-Plane's "call me again in" value.
  /// Looks at the loaded aircraft until a module recognises it or the wait
  /// for late datarefs is over.
  float detectTick();
  /// One purser tick. Runs only while an aircraft module is active.
  float mainTick(float elapsed);

  /// The model the window draws, or null while the plugin is disabled.
  ui::UiModel* model() { return enabled_ ? &model_ : nullptr; }
  /// Called by the window before it draws. While dormant no flight loop
  /// refreshes the model or picks up a finished pack scan, so this does.
  void uiRefresh();

 private:
  App();
  void scheduleDetection(float seconds);
  void activate(AircraftModule& module, const LoadedAircraft& aircraft,
                const Recognition& recognition);
  void deactivate(const char* why);
  void enterSafeMode();
  void updateMenu();
  void loadSettingsFile();
  void saveSettingsFile();
  /// Applies settings that other parts cache: the log level and the muffle
  /// filter.
  void applySettings();
  void refreshModel();

  void startScan();
  void pollScan();
  void updateAircraftType();
  void resolvePackNow(const char* why);
  SelectContext selectContext() const;
  /// A new flight begins: stops audio and makes the purser work out the
  /// state of the flight again (VP_SPEC § 4.4).
  void resetFlight(const char* why);
  void trackRefuel();
  /// Notices a teleport or situation load: a jump of more than 1 NM in one
  /// tick resets the flight.
  void trackPosition();
  void recordTrace();
  void onObservedCommand(const ObservedCommand& command);
  /// ICAO designator of the aircraft as flown now.
  std::string flyingIcao() const;

  // Folders, all with a trailing separator.
  std::string xpRoot_, pluginsDir_, outputDir_, prefsDir_;
  std::string settingsPath_;
  /// `FXAnnounce.ini`, read once when there is no `Xnounce.ini` yet.
  std::string legacySettingsPath_;

  SettingsStore store_;
  Settings& settings_ = store_.core();
  ui::UiModel model_;

  bool enabled_ = false;
  /// A supported aircraft is loaded and the purser runs.
  bool active_ = false;
  bool muted_ = false;
  /// This flight may play [Overwater] files (VP_SPEC § 3.1, D-2).
  bool overwater_ = false;

  // The aircraft.
  AircraftModules modules_;
  AircraftModule* module_ = nullptr;  ///< The active module, else null.
  LoadedAircraft aircraft_;           ///< The aircraft the purser runs on.
  LoadedAircraft loaded_;             ///< Whatever is loaded, for Status.
  std::string dormantNote_;           ///< Why a recognised aircraft is dormant.
  AircraftType type_;

  // The simulator.
  XpCommon common_;
  SimState sim_;
  Recorder recorder_;
  /// The raw aircraft values last written to the trace, to write changes
  /// only.
  std::map<std::string, std::string> rawLast_;
  double rawAt_ = -1;

  PackService packs_;
  SimbriefSource sbh_;
  FlightPlanService plans_{sbh_, settings_};

  // Created at enable. Destroyed in this order: purser, player, cache.
  std::unique_ptr<DecodeCache> cache_;
  std::unique_ptr<Player> player_;
  std::unique_ptr<Purser> purser_;

  RefuelDetector refuel_;
  bool havePos_ = false;
  double lastLat_ = 0, lastLon_ = 0;
  /// To notice the purser unlocking the pack for the next flight.
  bool packWasLocked_ = false;

  /// The window was open when the plugin went dormant: open it again on the
  /// next supported aircraft.
  bool hiddenWhenDormant_ = false;
  /// Path of the aircraft the "not supported yet" line was logged for.
  std::string unsupportedLogged_;
  /// The plugin under its old name (FXAnnounce) is installed and enabled.
  /// Both would play every announcement, so this one stays dormant.
  bool oldPlugin_ = false;

  XPLMFlightLoopID detectLoop_ = nullptr;
  XPLMFlightLoopID mainLoop_ = nullptr;
  double detectUntil_ = 0;

  Commands commands_;
};

}  // namespace xn
