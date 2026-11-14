/// @file
/// Implements `src/app/app.h`; the interface is documented there.
#include "app/app.h"

#include <XPLMDataAccess.h>
#include <XPLMPlugin.h>
#include <XPLMUtilities.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <ctime>

#include "app/ui_presenter.h"
#include "audio/chime.h"
#include "core/guard.h"
#include "core/log.h"
#include "core/strings.h"
#include "sim/loaded_aircraft.h"
#include "ui/window.h"

namespace xn {

namespace {

constexpr float kDetectInterval = 0.5f;
/// How long to keep looking at a freshly loaded aircraft. Some aircraft
/// register their datarefs several seconds after loading.
constexpr float kDetectWindow = 12.0f;
/// The purser tick.
constexpr float kMainInterval = 0.25f;

double simSeconds() {
  static XPLMDataRef t = XPLMFindDataRef("sim/time/total_running_time_sec");
  return t ? XPLMGetDataf(t) : 0.0;
}

float detectCallback(float, float, int, void*) {
  return guarded("detect", 0.0f, [] { return App::get().detectTick(); });
}

float mainCallback(float elapsed, float, int, void*) {
  return guarded("tick", 0.0f, [&] { return App::get().mainTick(elapsed); });
}

float audioCallback(float elapsed, float, int, void*) {
  return guarded("audio", 0.0f, [&] { return App::get().audioTick(elapsed); });
}

std::string withSep(std::string p) {
  if (!p.empty() && p.back() != '/' && p.back() != '\\')
    p += XPLMGetDirectorySeparator();
  return p;
}

}  // namespace

App& App::get() {
  static App app;
  return app;
}

App::App()
    : modules_(makeAircraftModules(store_.aircraft())),
      commands_(Commands::Actions{
          [this] { toggleWindow(); },
          [this] { skip(); },
          [this] { advance(); },
          [this] { toggleMute(); },
          [this] { toggleOverwater(); },
          [this] { rescan(); },
          [this] { reenable(); },
          [this](const ObservedCommand& c) { onObservedCommand(c); },
      }) {}

App::~App() {
  // XPluginDisable has normally stopped everything. If it hasn't, nothing
  // here may touch X-Plane: the player and the purser are leaked on purpose,
  // because destroying them would. The pack scan thread is joined by
  // PackService's destructor.
  (void)purser_.release();
  (void)player_.release();
  if (cache_) cache_->shutdown();
}

// ---- lifecycle
// ---------------------------------------------------------------

void App::start() {
  XPLMEnableFeature("XPLM_USE_NATIVE_PATHS", 1);
  char buf[2048] = {};
  XPLMGetSystemPath(buf);
  xpRoot_ = withSep(buf);
  const char* sep = XPLMGetDirectorySeparator();
  pluginsDir_ = xpRoot_ + "Resources" + sep + "plugins" + sep;
  outputDir_ = xpRoot_ + "Output" + sep;
  char prefs[2048] = {};
  XPLMGetPrefsPath(prefs);
  std::string pp = prefs;
  const size_t cut = pp.find_last_of("/\\");
  prefsDir_ = cut == std::string::npos ? outputDir_ + "preferences" + sep
                                       : pp.substr(0, cut + 1);
  packs_.setPacksDir(pluginsDir_ + "AirlinePacks" + sep);
  settingsPath_ = prefsDir_ + "Xnounce.ini";
  legacySettingsPath_ = prefsDir_ + "FXAnnounce.ini";

  Log::get().open(outputDir_ + "Xnounce.log");
  Log::get().setSummarySink([](const char* line) { XPLMDebugString(line); });
  logSummary("Xnounce %s starting (packs: %s)", XN_VERSION,
             packs_.packsDir().c_str());
  loadSettingsFile();
  FaultGuard::get().setSafeModeHook([this] { enterSafeMode(); });
}

void App::enable() {
  enabled_ = true;
  model_.version = XN_VERSION;
  model_.settings = &settings_;
  model_.settingDefs = &store_.defs();
  model_.supportedText =
      "Xnounce works with " + supportedAircraft(modules_) + ".";
  model_.loadHint = "Load " + loadHint(modules_) + ".";
  model_.onSkip = [this] { skip(); };
  model_.onAdvance = [this] { advance(); };
  model_.onToggleMute = [this] { toggleMute(); };
  model_.onToggleOverwater = [this] { toggleOverwater(); };
  model_.onRescan = [this] { rescan(); };
  model_.onReenable = [this] { reenable(); };
  model_.explain = [this](int type, int ctx) {
    return explainSelection(packs_.pack(), type, ctx, selectContext());
  };
  model_.onSettingsChanged = [this] {
    modelDirty_ = true;
    applySettings();
    saveSettingsFile();
    // The pack folders changed.
    if (settings_.packPaths != packs_.scannedPaths()) rescan();
  };
  model_.onResetSettings = [this] { store_.resetToDefaults(); };
  model_.onTryCheck = [this](int row) {
    // Carried out by the next main tick (docs/ARCHITECTURE.md § 4, rule 4).
    if (active_ && module_)
      if (CabinActions* acts = module_->actions()) acts->tryCheck(row);
    modelDirty_ = true;
  };
  ui::setModelProvider([] {
    App& a = App::get();
    a.uiRefresh();
    return a.model();
  });

  XPLMCreateFlightLoop_t d{sizeof(XPLMCreateFlightLoop_t),
                           xplm_FlightLoop_Phase_AfterFlightModel,
                           detectCallback, nullptr};
  detectLoop_ = XPLMCreateFlightLoop(&d);
  XPLMCreateFlightLoop_t m{sizeof(XPLMCreateFlightLoop_t),
                           xplm_FlightLoop_Phase_AfterFlightModel, mainCallback,
                           nullptr};
  mainLoop_ = XPLMCreateFlightLoop(&m);
  XPLMCreateFlightLoop_t a{sizeof(XPLMCreateFlightLoop_t),
                           xplm_FlightLoop_Phase_AfterFlightModel,
                           audioCallback, nullptr};
  audioLoop_ = XPLMCreateFlightLoop(&a);

  cache_ = std::make_unique<DecodeCache>();
  applySettings();  // gives the new cache its muffle cutoff
  player_ = std::make_unique<Player>(*cache_);
  purser_ = std::make_unique<Purser>(*player_);
  purser_->setEventSink([this](const TimelineRow& r) {
    recorder_.event(r.t, std::string(annTypeName(r.type)) + " " +
                             outcomeName(r.outcome) +
                             (r.detail.empty() ? "" : ": " + r.detail));
  });
  purser_->setNoteSink([this](double t, const std::string& text) {
    recorder_.event(t, "purser " + text);
  });
  plans_.setEventSink(
      [this](double t, const std::string& text) { recorder_.event(t, text); });
  // The menu, the commands and the window work on every aircraft, and the
  // packs are scanned once now, so the Packs tab has content while dormant.
  commands_.enable();
  updateMenu();
  startScan();
  refreshModel();
  scheduleDetection(kDetectWindow);
}

void App::disable() {
  deactivate("plugin disabled");
  const bool reopen = ui::windowVisible() || hiddenWhenDormant_;
  commands_.disable();
  ui::shutdownUi();
  // Destroying the hidden window would otherwise record it as closed.
  settings_.windowVisible = reopen;
  packs_.stopScan();
  purser_.reset();
  player_.reset();
  if (cache_) cache_->shutdown();
  cache_.reset();
  if (detectLoop_) XPLMDestroyFlightLoop(detectLoop_);
  if (mainLoop_) XPLMDestroyFlightLoop(mainLoop_);
  if (audioLoop_) XPLMDestroyFlightLoop(audioLoop_);
  detectLoop_ = mainLoop_ = audioLoop_ = nullptr;
  saveSettingsFile();
  ui::setModelProvider(nullptr);
  packs_.dropLibrary();
  enabled_ = false;
}

void App::stop() {
  releaseParkedAudio();
  logSummary("stopped");
  Log::get().setSummarySink(nullptr);
  Log::get().close();
}

void App::onMessage(int msg, void* param) {
  // A parameter of 0 means the user's aircraft.
  const bool usersAircraft = reinterpret_cast<intptr_t>(param) == 0;
  switch (msg) {
    case XPLM_MSG_PLANE_LOADED:
    case XPLM_MSG_LIVERY_LOADED:
      if (usersAircraft) {
        if (msg == XPLM_MSG_PLANE_LOADED) deactivate("aircraft reloaded", true);
        scheduleDetection(kDetectWindow);
      }
      break;
    case XPLM_MSG_PLANE_UNLOADED:
      if (usersAircraft) deactivate("aircraft unloaded", true);
      break;
    case XPLM_MSG_WILL_WRITE_PREFS:
      saveSettingsFile();
      break;
    case XPLM_MSG_AIRPORT_LOADED:
      if (active_) resetFlight("new airport or situation loaded");
      break;
    case XPLM_MSG_FMOD_BANK_UNLOADING:
      // X-Plane is rebuilding its sound banks: our channels are gone, and
      // the aircraft's sound handles too.
      if (player_) player_->stopAll();
      if (module_)
        if (CabinActions* acts = module_->actions())
          acts->soundBanksUnloading();
      break;
    default:
      break;
  }
}

// ---- which aircraft
// ------------------------------------------------------------

void App::scheduleDetection(float seconds) {
  if (!detectLoop_ || FaultGuard::get().safeMode()) return;
  detectUntil_ = simSeconds() + seconds;
  XPLMScheduleFlightLoop(detectLoop_, kDetectInterval, 1);
}

float App::detectTick() {
  const LoadedAircraft aircraft = readLoadedAircraft();
  loaded_ = aircraft;
  dormantNote_.clear();
  // The plugin under its old name is still installed. Both would play every
  // announcement, so this one stays dormant (docs/ARCHITECTURE.md § 5.1).
  const XPLMPluginID old = XPLMFindPluginBySignature("yosh.fxannounce");
  const bool oldRunning = old != XPLM_NO_PLUGIN_ID && XPLMIsPluginEnabled(old);
  if (oldRunning != oldPlugin_) {
    oldPlugin_ = oldRunning;
    if (oldRunning)
      logSummary(
          "dormant: the old FXAnnounce plugin is still installed; delete "
          "Resources/plugins/FXAnnounce and restart X-Plane");
    refreshModel();
  }
  if (oldRunning) {
    if (active_) deactivate("old FXAnnounce plugin still installed");
    return 0.0f;
  }

  // The first module that says "mine" takes the aircraft.
  AircraftModule* found = nullptr;
  Recognition recognition;
  for (const auto& m : modules_) {
    recognition = m->recognise(aircraft);
    if (recognition.ours) {
      found = m.get();
      break;
    }
  }
  if (found && !recognition.supported) {
    // A model the module knows but can't run on yet. Logged once per
    // aircraft.
    if (active_) deactivate(recognition.dormantReason.c_str());
    dormantNote_ = recognition.cardNote;
    if (unsupportedLogged_ != aircraft.acfPath) {
      unsupportedLogged_ = aircraft.acfPath;
      logSummary("%s", recognition.logLine.c_str());
    }
    return 0.0f;
  }
  if (found) {
    if (active_ && found != module_) deactivate("another aircraft loaded");
    if (!active_ || aircraft.acfPath != aircraft_.acfPath ||
        aircraft.liveryPath != aircraft_.liveryPath)
      activate(*found, aircraft, recognition);
    return 0.0f;  // done until the next aircraft or livery load
  }
  if (simSeconds() < detectUntil_) return kDetectInterval;
  if (active_) deactivate("not a supported aircraft");
  logInfo("dormant: %s (%s) is not a supported aircraft",
          aircraft.acfFile.c_str(), aircraft.acfIcao.c_str());
  return 0.0f;
}

void App::activate(AircraftModule& module, const LoadedAircraft& aircraft,
                   const Recognition& recognition) {
  const bool wasActive = active_;
  const bool liveryChanged = aircraft.liveryPath != aircraft_.liveryPath;
  aircraft_ = aircraft;
  module_ = &module;
  active_ = true;
  logSummary(
      "active on %s (%s, %s), livery '%s'", aircraft.acfFile.c_str(),
      aircraft.acfIcao.c_str(), recognition.model.c_str(),
      aircraft.liveryName.empty() ? "default" : aircraft.liveryName.c_str());
  std::vector<std::string> missing = common_.resolve();
  for (std::string& name : module.activate(aircraft))
    missing.push_back(std::move(name));
  if (!missing.empty())
    logWarn("datarefs not available (features that need them are off): %s",
            join(missing, ", ").c_str());
  recorder_.setAircraft(module.id());
  crew_.reset();
  if (CabinActions* acts = module.actions())
    acts->setEventSink([this](const std::string& line) {
      logInfo("cabin crew: %s", line.c_str());
      recorder_.event(sim_.t, "crew " + line);
    });
  packs_.loadLiveryConfig(aircraft_.liveryPath, aircraft_.liveryName);
  overwater_ =
      packs_.liveryConfig().overwater.value_or(settings_.overwaterDefault);
  powerCut_ = PaPowerCut(paSeed(aircraft_.tailNumber, "power"));
  if (player_) {
    // The cabin's call chime: the aircraft's own sound, or the buzzer.
    std::shared_ptr<PcmBuffer> chime;
    if (const std::string rel = module.chimeSound(); !rel.empty()) {
      DecodeResult d = decodeWav(xpRoot_ + rel);
      if (d.pcm) {
        chime = d.pcm;
      } else {
        logWarn("chime %s: %s; a generated chime plays instead", rel.c_str(),
                d.error.c_str());
        chime = makeSingleChime();
      }
    }
    player_->setChimeSound(chime);
  }
  applySettings();  // the PA preset of this airframe and livery
  if (liveryChanged && wasActive) resetFlight("livery changed");
  updateAircraftType();
  if (!wasActive) {
    plans_.reset();
    commands_.observe(module.observedCommands());
    if (!packs_.library() && !packs_.scanning()) startScan();
    if (!FaultGuard::get().safeMode()) {
      XPLMScheduleFlightLoop(mainLoop_, kMainInterval, 1);
      XPLMScheduleFlightLoop(audioLoop_, -1.0f, 1);
    }
    if (settings_.windowVisible) ui::showWindow(true);
    hiddenWhenDormant_ = false;
  }
  resolvePackNow(wasActive ? "livery changed" : "activated");
  updateMenu();
  refreshModel();
}

void App::deactivate(const char* why, bool aircraftGone) {
  if (!active_) return;
  logInfo("dormant: %s", why);
  active_ = false;
  if (mainLoop_) XPLMScheduleFlightLoop(mainLoop_, 0, 0);
  if (audioLoop_) XPLMScheduleFlightLoop(audioLoop_, 0, 0);
  commands_.stopObserving();
  // The window hides when the supported aircraft goes and reopens on the
  // next one. The menu stays, for the dormant Status card.
  const bool visible = ui::windowVisible();
  ui::showWindow(false);
  settings_.windowVisible = visible;
  hiddenWhenDormant_ = visible;
  recorder_.close();
  if (player_) player_->stopAll();
  if (purser_) purser_->reset();
  if (player_) player_->setOwnChime(false);
  if (module_) {
    // What the cabin actions changed in the aircraft goes back, unless the
    // aircraft is gone.
    if (CabinActions* acts = module_->actions()) acts->release(aircraftGone);
    module_->deactivate();
  }
  module_ = nullptr;
  aircraft_ = LoadedAircraft{};
  packs_.clear();
  havePos_ = false;
  updateMenu();
  refreshModel();
}

void App::enterSafeMode() {
  // Called by the fault guard after repeated faults: stop everything that
  // runs on its own. The menu and the window stay for "Re-enable".
  if (mainLoop_) XPLMScheduleFlightLoop(mainLoop_, 0, 0);
  if (audioLoop_) XPLMScheduleFlightLoop(audioLoop_, 0, 0);
  if (detectLoop_) XPLMScheduleFlightLoop(detectLoop_, 0, 0);
  if (active_ && module_)
    if (CabinActions* acts = module_->actions()) acts->release(false);
  if (player_) player_->setOwnChime(false);
  recorder_.close();
  if (player_) player_->stopAll();
  updateMenu();
}

void App::reenable() {
  if (!FaultGuard::get().safeMode()) return;
  FaultGuard::get().reset();
  updateMenu();
  scheduleDetection(kDetectWindow);
  if (active_) {
    XPLMScheduleFlightLoop(mainLoop_, kMainInterval, 1);
    XPLMScheduleFlightLoop(audioLoop_, -1.0f, 1);
  }
}

void App::updateAircraftType() {
  if (!module_) return;
  const AircraftType t = module_->type(sim_);
  if (t.tags != type_.tags)
    logInfo("aircraft type tags: %s", join(t.tags, ", ").c_str());
  type_ = t;
}

std::string App::flyingIcao() const {
  return toUpper(sim_.acfIcao.empty() ? aircraft_.acfIcao : sim_.acfIcao);
}

// ---- packs
// ---------------------------------------------------------------------

void App::startScan() { packs_.startScan(settings_.packPaths); }

void App::pollScan() {
  const bool fresh = packs_.poll([this] {
    if (player_) player_->stopAll();
  });
  if (!fresh || !active_) return;
  // A locked pack stays the same pack in the new library when it still
  // exists. Otherwise the pack is chosen again.
  if (purser_ && purser_->packLocked() &&
      packs_.keepPack(packs_.packBeforeScan()))
    return;
  resolvePackNow("packs scanned");
}

void App::resolvePackNow(const char* why) {
  PackService::Request request;
  request.liveryName = aircraft_.liveryName;
  request.flightNumber = sim_.flightNumber;
  request.ofpCallsign = plans_.callsign();
  request.packLocked = purser_ && purser_->packLocked();
  request.why = why;
  packs_.resolve(request);
}

SelectContext App::selectContext() const {
  SelectContext c;
  c.aircraft = type_;
  c.refueling = refuel_.refueling(sim_.t);
  c.time = timeOfDayAt(sim_.localTimeSec);
  if (std::isfinite(sim_.localTimeSec)) {
    c.localHour = std::fmod(sim_.localTimeSec / 3600.0, 24.0);
    if (c.localHour < 0) c.localHour += 24.0;
  }
  c.phase = purser_ ? purser_->phase() : Phase::PreFlight;
  c.overwater = overwater_;
  c.customTags = packs_.customTags(sim_.satcom, settings_.satcomTags);
  c.variant = packs_.variant();
  return c;
}

// ---- the flight
// ----------------------------------------------------------------

void App::resetFlight(const char* why) {
  logInfo("flight reset: %s", why);
  if (player_) player_->stopAll();
  if (purser_) purser_->reset();
  havePos_ = false;
  refuel_.reset();
  plans_.resetFlight();
  crew_.reset();
  if (module_)
    if (CabinActions* acts = module_->actions()) acts->newFlight();
  resolvePackNow(why);
}

void App::trackRefuel() {
  const double rate = refuel_.update(sim_, settings_);
  if (rate > 0) logInfo("refuelling detected (%.1f kg/s)", rate);
}

void App::trackPosition() {
  if (sim_.lat == 0 && sim_.lon == 0) return;
  // Replay moves the aircraft along the recording and back: that is not a new
  // flight (VP_SPEC § 4.4, Run1(XN)). The position is taken afresh when the
  // replay ends.
  if (sim_.replay) {
    havePos_ = false;
    return;
  }
  if (havePos_) {
    const double dLat = (sim_.lat - lastLat_) * 60.0;
    const double dLon = (sim_.lon - lastLon_) * 60.0 *
                        std::cos(sim_.lat * 3.14159265358979 / 180.0);
    if (std::sqrt(dLat * dLat + dLon * dLon) > 1.0) {
      lastLat_ = sim_.lat;
      lastLon_ = sim_.lon;
      resetFlight("aircraft moved more than 1 NM at once");
      havePos_ = true;
      return;
    }
  }
  havePos_ = true;
  lastLat_ = sim_.lat;
  lastLon_ = sim_.lon;
}

float App::mainTick(float elapsed) {
  if (!active_ || !module_ || FaultGuard::get().safeMode()) return 0.0f;
  pollScan();

  // The snapshot: X-Plane's own values, then the aircraft's.
  sim_ = common_.read(elapsed, module_->cabinGeometry());
  module_->read(sim_);
  updateAircraftType();
  if (sim_.flightNumber != packs_.resolvedFlightNumber())
    resolvePackNow("flight number changed");
  trackRefuel();
  trackPosition();

  // The flight plan: its check here, the boarding plan, the pack it names.
  plans_.setBoardingBegun(purser_ && purser_->boardingBegun());
  plans_.tick(sim_, purser_ && purser_->tookOff(), type_, flyingIcao());
  if (plans_.callsign() != packs_.resolvedCallsign())
    resolvePackNow("SimBrief OFP changed");

  // The purser decides and the player plays from the active pack with the
  // tags that hold right now.
  player_->setSource(packs_.pack(), selectContext(), &settings_, &sim_,
                     muted_ || !settings_.enabled);
  purser_->setAircraft(type_);
  purser_->setBoardingPlan(plans_.plan());
  {
    // The aircraft may sound the cabin's chime itself (VP_SPEC § 4.7).
    CabinActions* acts = module_->actions();
    player_->setOwnChime(acts && acts->ownChime());
  }
  purser_->tick(sim_, settings_);
  runCabinActions();
  {
    // The next flight unlocks the pack (VP_SPEC § 1.4).
    const bool locked = purser_->packLocked();
    if (packWasLocked_ && !locked) resolvePackNow("next flight");
    packWasLocked_ = locked;
  }

  if (auto next = purser_->nextQueued())
    player_->prefetch(next->first, next->second);
  // The audio loop updates the player every frame (audioTick).

  recordTrace();
  refreshModel();
  return kMainInterval;
}

void App::runCabinActions() {
  CabinActions* acts = module_ ? module_->actions() : nullptr;
  const bool chimeAsked = player_ && player_->takeChimeRequest();
  if (!acts) return;
  if (chimeAsked) acts->chime();
  // Nothing moves while the sim is paused or replaying.
  if (sim_.paused || sim_.replay) return;
  acts->tick(crew_.plan(purser_->cabinStage(), sim_, settings_), sim_);
}

float App::audioTick(float elapsed) {
  if (!active_ || !module_ || !player_ || FaultGuard::get().safeMode())
    return 0.0f;
  player_->setPowerGain(powerCut_.update(simSeconds(), sim_.acPower,
                                         sim_.acSource, settings_.paCutout));
  player_->update(elapsed);
  // The aircraft's own announcements are silenced as each one starts.
  if (CabinActions* acts = module_->actions()) acts->everyFrame(simSeconds());
  return -1.0f;  // every frame
}

void App::recordTrace() {
  if (settings_.recorder && !recorder_.isOpen()) {
    char name[64];
    const std::time_t now = std::time(nullptr);
    std::strftime(name, sizeof(name), "%Y%m%d-%H%M%S.xntrace.csv",
                  std::localtime(&now));
    const std::string path =
        outputDir_ + "Xnounce" + XPLMGetDirectorySeparator() + name;
    if (recorder_.open(path)) {
      logInfo("recording trace to %s", path.c_str());
      // A new trace starts with the plan's current state.
      for (const std::string& line : plans_.traceIntro())
        recorder_.event(sim_.t, line);
    } else {
      logWarn("could not open %s; recorder off", path.c_str());
      settings_.recorder = false;
    }
  } else if (!settings_.recorder && recorder_.isOpen()) {
    recorder_.close();
  }
  recorder_.state(sim_);
  if (recorder_.isOpen() && sim_.t - rawAt_ >= 1.0) {
    // The aircraft's raw values, written when they change, so the trace
    // shows which index of an array moved.
    rawAt_ = sim_.t;
    for (const auto& [name, value] : module_->rawSnapshot()) {
      auto it = rawLast_.find(name);
      if (it != rawLast_.end() && it->second == value) continue;
      recorder_.event(sim_.t, "raw " + name + " = " + value);
      rawLast_[name] = value;
    }
  } else if (!recorder_.isOpen()) {
    rawLast_.clear();
  }
}

void App::onObservedCommand(const ObservedCommand& command) {
  logInfo("cabin call button: %s", command.name.c_str());
  recorder_.event(sim_.t, "call button " + command.name);
  if (command.cabinCall && purser_ && active_ && !FaultGuard::get().safeMode())
    purser_->cabinCall();
}

// ---- the window
// ----------------------------------------------------------------

void App::uiRefresh() {
  if (!enabled_) return;
  if (!active_) {
    pollScan();
    refreshModel();
    return;
  }
  // The main loop refreshes the model four times a second. A click must not
  // wait for it, and the timers count on between its refreshes. Half a second
  // at most, so a stalled loop never runs them far ahead.
  if (modelDirty_) refreshModel();
  model_.ageSec = sim_.paused || modelAt_ < 0
                      ? 0.0
                      : std::clamp(simSeconds() - modelAt_, 0.0, 0.5);
}

void App::refreshModel() {
  const auto t0 = std::chrono::steady_clock::now();
  struct Timed {
    std::chrono::steady_clock::time_point t0;
    double& out;
    ~Timed() {
      out = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - t0)
                .count();
    }
  } timed{t0, model_.modelMs};
  modelDirty_ = false;
  modelAt_ = simSeconds();
  model_.ageSec = 0;
  model_.active = active_;
  model_.safeMode = FaultGuard::get().safeMode();
  model_.oldPlugin = oldPlugin_;
  model_.lastFault = FaultGuard::get().lastFault();
  model_.muted = muted_;
  model_.overwater = overwater_;
  model_.library = packs_.library();
  model_.scanning = packs_.scanning();
  model_.packsDir = packs_.packsDir();
  model_.recorderPath = recorder_.isOpen() ? recorder_.path() : std::string();
  model_.log = Log::get().recent();
  model_.aircraftGroup = active_ && module_ ? module_->id() : "";
  model_.aircraftName = active_ && module_ ? module_->name() : "";
  if (!active_ || !module_) {
    presentDormant(model_, loaded_, dormantNote_);
    return;
  }
  ActiveView view(settings_, sim_, type_, packs_, plans_);
  view.aircraftName = module_->name();
  view.liveryName = aircraft_.liveryName;
  view.purser = purser_ && player_ ? purser_.get() : nullptr;
  if (player_) {
    const Player::NowPlaying now = player_->nowPlaying();
    view.now.file = now.label;
    view.now.pos = now.pos;
    view.now.dur = now.dur;
    view.now.decoding = now.decoding;
    view.now.paused = now.paused;
    view.now.interphone = now.interphone;
    view.now.music = player_->musicLabel();
  }
  view.refueling = refuel_.refueling(sim_.t);
  view.overwater = overwater_;
  view.aircraftRows = module_->liveInputs(sim_);
  if (CabinActions* acts = module_->actions()) {
    view.actions = acts->status();
    view.announcementNote = acts->announcementNote();
    if (settings_.showLog) view.checks = acts->checks();
  }
  view.recorderPath = model_.recorderPath;
  presentActive(model_, view);
}

// ---- actions
// -------------------------------------------------------------------

void App::toggleWindow() {
  hiddenWhenDormant_ = false;
  ui::toggleWindow();
}

void App::skip() {
  if (active_ && purser_) purser_->skip();
  modelDirty_ = true;
}

void App::advance() {
  if (active_ && purser_) purser_->advance();
  modelDirty_ = true;
}

void App::toggleMute() {
  muted_ = !muted_;
  modelDirty_ = true;
  logInfo("%s", muted_ ? "muted" : "unmuted");
  updateMenu();
}

void App::toggleOverwater() {
  if (!active_) return;  // the livery's configuration sets it per flight
  overwater_ = !overwater_;
  modelDirty_ = true;
  logInfo("overwater flight: %s", overwater_ ? "yes" : "no");
  updateMenu();
}

void App::rescan() {
  if (active_) {
    packs_.loadLiveryConfig(aircraft_.liveryPath, aircraft_.liveryName);
    if (module_) module_->reloadFiles();
    applySettings();  // the livery may have changed its pa_effects
  }
  startScan();
  modelDirty_ = true;
}

void App::updateMenu() {
  Commands::MenuState state;
  state.muted = muted_;
  state.overwater = overwater_;
  state.safeMode = FaultGuard::get().safeMode();
  state.active = active_;
  commands_.updateMenu(state);
}

// ---- settings
// ------------------------------------------------------------------

void App::loadSettingsFile() {
  const SettingsStore::Loaded loaded =
      store_.load(settingsPath_, legacySettingsPath_);
  applySettings();
  logInfo("settings: %s", settingsPath_.c_str());
  if (loaded.fromLegacy)
    logSummary("settings carried over from %s", legacySettingsPath_.c_str());
  for (const std::string& m : loaded.migrated)
    logInfo("settings: updated %s", m.c_str());
  if (loaded.fromLegacy || !loaded.migrated.empty()) saveSettingsFile();
}

void App::saveSettingsFile() {
  if (!store_.save(settingsPath_))
    logWarn("could not write %s", settingsPath_.c_str());
}

void App::applySettings() {
  Log::get().setLevel(
      static_cast<LogLevel>(std::clamp(settings_.logLevel, 0, 3)));
  // The muffled copies are only needed while the door actually muffles.
  if (cache_)
    cache_->setMuffleCutoff(settings_.muffle && settings_.realisticHearing
                                ? settings_.muffleCutoffHz
                                : 0.0f);
  // The PA of this airframe; a livery can keep its PA clean.
  if (cache_) {
    const bool clean =
        active_ && packs_.liveryConfig().paEffects.value_or(true) == false;
    cache_->setPaAging(
        clean ? PaPreset::Modern
              : resolvePaPreset(settings_.paPreset, aircraft_.tailNumber),
        aircraft_.tailNumber);
    cache_->setShaping(settings_.loudness, settings_.cabinAcoustics);
  }
}

}  // namespace xn
