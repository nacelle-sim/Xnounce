/// @file
/// Implements `src/aircraft/zibo737/zibo737_actions.h`; the interface is
/// documented there.
#include "aircraft/zibo737/zibo737_actions.h"

#include <cmath>
#include <cstdio>
#include <iterator>
#include <optional>

#include "core/log.h"
#include "sim/fmod_studio.h"

namespace xn::zibo737 {

namespace {

std::string fmt2(float v) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.2f", static_cast<double>(v));
  return buf;
}

std::optional<float> readOpt(const DataRef& r) {
  const auto v = r.getFloat();
  if (v && std::isfinite(*v)) return v;
  return std::nullopt;
}

float ratioOf(const DataRef& r) {
  const auto v = readOpt(r);
  return v ? *v : -1.0f;
}

int wholeOf(const DataRef& r) {
  const auto v = readOpt(r);
  return v ? static_cast<int>(std::lround(*v)) : -1;
}

/// The light settings the Aircraft check can try, one row each; night and
/// sleep share a button.
constexpr int kCheckLights[] = {kWhiteBright, kTakeoffLanding, kCruise,
                                kWhiteMedium, kNight};
constexpr double kChimeSettleS = 2;
constexpr double kLightSettleS = 3;
constexpr double kStairsSettleS = 75;

/// The LevelUp's cabin crew announcements (fmod/GUIDS.txt): silenced
/// (docs/aircraft/zibo737.md § 5). The flush stays.
const char* const kCrewEvents[] = {
    "event:/b737ng/equipment/alerts/crew/cabin/AT_safety_demo",
    "event:/b737ng/equipment/alerts/crew/cabin/CRW_arrivexcheck",
    "event:/b737ng/equipment/alerts/crew/cabin/CRW_gate_departure",
    "event:/b737ng/equipment/alerts/crew/cabin/CRW_seatbelts_on",
    "event:/b737ng/equipment/alerts/crew/cabin/CRW_seatbelts_on_gnd",
    "event:/b737ng/equipment/alerts/crew/cabin/CRW_seatbelts_on_trb",
    "event:/b737ng/equipment/alerts/crew/cabin/CRW_takeoff",
    "event:/b737ng/equipment/alerts/crew/cabin/CRW_taxi2gate",
    "event:/b737ng/equipment/alerts/crew/cabin/CRW_x10k",
    "event:/b737ng/equipment/alerts/crew/cabin/FA_secured",
    "event:/b737ng/equipment/alerts/crew/cabin/FA_start_service",
    "event:/b737ng/equipment/alerts/crew/cabin/FA_strapped_in",
};
/// The captain's messages: they play, and Xnounce waits for them.
const char* const kCaptainEvents[] = {
    "event:/b737ng/equipment/alerts/crew/cabin/CPT_cruise",
    "event:/b737ng/equipment/alerts/crew/cabin/CPT_descent",
    "event:/b737ng/equipment/alerts/crew/cabin/CPT_preland",
    "event:/b737ng/equipment/alerts/crew/cabin/CPT_takeoff",
    "event:/b737ng/equipment/alerts/crew/cabin/CPT_welcome",
};

}  // namespace

Actions::Actions(const Options& options)
    : options_(options),
      held_{{"crew announcements volume",
             DataRef("laminar/b738/fmodpack/fmod_vol_crew"), OptionHold(0),
             true}} {
  for (int k = 0; k < kDoorCount; ++k)
    door_[k] = DataRef(doorInfo(static_cast<Door>(k)).position);
}

void Actions::activate(bool zibo) {
  // A livery change activates again without a deactivation: what was held is
  // put back first, so the aircraft's own values are what is remembered.
  if (active_) release(false);
  zibo_ = zibo;
  active_ = true;
  crew_.reset();
  commands_.clear();
  pendingCheck_ = -1;
  tried_ = Tried{};
  for (DataRef& d : door_) d.resolve();
  for (DataRef* r : {&stairsHidden_, &stairsRatio_, &paxLighting_, &autoFlight_,
                     &cargoLoading_, &callChime_})
    r->resolve();
  for (Held& h : held_) {
    h.ref.resolve();
    h.hold.forget();
  }
  // The LevelUp's crew announcements are FMOD events no dataref switches
  // off: they are silenced through X-Plane's FMOD Studio.
  fmod_.reset();
  fmodWhy_.clear();
  silencing_ = false;
  if (!zibo_) {
    const FmodApi api = findFmodStudio(&fmodWhy_);
    if (api.complete())
      fmod_ = std::make_unique<FmodEvents>(
          api,
          std::vector<std::string>(std::begin(kCrewEvents),
                                   std::end(kCrewEvents)),
          std::vector<std::string>(std::begin(kCaptainEvents),
                                   std::end(kCaptainEvents)));
    else
      logInfo("aircraft sounds: can't be reached (%s)", fmodWhy_.c_str());
  }

  // The commands the actions need, looked up once.
  std::vector<std::string> names = {kAirstairsToggle};
  for (int k = 0; k < kDoorCount; ++k) {
    names.emplace_back(doorInfo(static_cast<Door>(k)).open);
    names.emplace_back(doorInfo(static_cast<Door>(k)).close);
  }
  for (int l : kCheckLights) names.emplace_back(paxLightCommand(l));
  for (const std::string& n : names)
    if (XPLMCommandRef c = XPLMFindCommand(n.c_str())) commands_[n] = c;

  const DoorInfo& l1 = doorInfo(Door::L1);
  doorsOk_ = door_[0].good() && has(l1.open) && has(l1.close);
  doorsWhy_ = doorsOk_ ? "" : "this aircraft has no door commands";
  lightsOk_ = paxLighting_.good();
  for (int l : kCheckLights) lightsOk_ = lightsOk_ && has(paxLightCommand(l));
  lightsWhy_ = lightsOk_ ? "" : "this aircraft has no cabin light commands";
  chimeOk_ = zibo_ && callChime_.canWrite();
  chimeWhy_ = chimeOk_ ? ""
              : zibo_  ? "the aircraft's chime can't be sounded"
                       : "LevelUp's sounds have no cabin chime a plugin can "
                         "sound: X-Plane's chime plays";

  // The Aircraft check: every door the model has, the airstairs, each light
  // setting, the chime and the announcement options.
  checkList_.clear();
  for (int k = 0; k < kDoorCount; ++k)
    if (door_[k].good()) checkList_.push_back({Check::DoorRow, k});
  if (stairsHidden_.good()) checkList_.push_back({Check::Stairs, 0});
  for (int i = 0; i < static_cast<int>(std::size(kCheckLights)); ++i)
    checkList_.push_back({Check::Light, i});
  checkList_.push_back({Check::Chime, 0});
  for (int i = 0; i < static_cast<int>(std::size(held_)); ++i)
    if (held_[i].ref.good() && (!held_[i].zibOnly || zibo_))
      checkList_.push_back({Check::Option, i});

  logInfo("cabin actions: doors %s, cabin lights %s, aircraft chime %s",
          doorsOk_ ? "available" : doorsWhy_.c_str(),
          lightsOk_ ? "available" : lightsWhy_.c_str(),
          chimeOk_ ? "available" : chimeWhy_.c_str());
}

void Actions::deactivate() {
  active_ = false;
  fmod_.reset();
  commands_.clear();
  checkList_.clear();
  crew_.reset();
  pendingCheck_ = -1;
  for (Held& h : held_) h.hold.forget();
}

bool Actions::has(const std::string& name) const {
  return commands_.count(name) > 0;
}

bool Actions::run(const std::string& name) {
  auto it = commands_.find(name);
  if (it == commands_.end()) return false;
  XPLMCommandOnce(it->second);
  return true;
}

std::vector<ActionStatus> Actions::status() const {
  bool silenceOk = false;
  std::string silenceNote;
  if (zibo_) {
    silenceOk = held_[0].ref.canWrite();
    silenceNote = silenceOk ? "crew volume" : "no crew volume to hold";
  } else if (fmod_ && fmod_->ready()) {
    silenceOk = true;
    silenceNote = std::to_string(fmod_->mutedFound()) + " of " +
                  std::to_string(std::size(kCrewEvents)) +
                  " crew announcements found";
  } else {
    silenceNote = fmod_ ? fmod_->why() : fmodWhy_;
  }
  return {
      {"Doors", options_.actDoors, doorsOk_,
       doorsOk_ ? crew_.doorsNote() : doorsWhy_},
      {"Cabin lights", options_.actLights, lightsOk_,
       lightsOk_ ? crew_.lightsNote() : lightsWhy_},
      {"Aircraft's chime", options_.actChime, chimeOk_, chimeWhy_},
      {"Aircraft's cabin announcements silenced", options_.silenceAircraftPa,
       silenceOk, silenceNote},
  };
}

bool Actions::anyOn() const {
  return active_ && (doorsOn() || lightsOn() || chimeOn());
}

std::string Actions::announcementNote() const {
  if (!active_) return {};
  if (!options_.silenceAircraftPa)
    return "The aircraft's own cabin announcements play; Xnounce waits for "
           "them.";
  if (zibo_)
    return "The Zibo's crew announcements are silenced. Its captain's "
           "messages play, and Xnounce waits for them.";
  if (fmod_ && fmod_->ready())
    return "The LevelUp's cabin crew announcements are silenced. Its "
           "captain's messages play, and Xnounce waits for them.";
  return "The LevelUp's cabin crew announcements can't be silenced (" +
         (fmod_ ? fmod_->why() : fmodWhy_) +
         "); Xnounce waits while one plays.";
}

CrewReadings Actions::readings() const {
  CrewReadings r;
  for (int k = 0; k < kDoorCount; ++k)
    r.door[static_cast<size_t>(k)] = door_[k].good() ? ratioOf(door_[k]) : -1;
  r.airstairsHidden = wholeOf(stairsHidden_);
  r.airstairRatio = ratioOf(stairsRatio_);
  r.paxLighting = wholeOf(paxLighting_);
  r.autoFlight = wholeOf(autoFlight_);
  r.cargoLoading = wholeOf(cargoLoading_);
  return r;
}

void Actions::tick(const CabinPlan& plan, const SimState& s) {
  if (!active_) return;
  now_ = s.t;

  // The Zibo's crew announcements, while they are silenced.
  const bool hold = silenceOn();
  for (Held& h : held_) {
    if (!h.ref.good() || (h.zibOnly && !zibo_)) continue;
    const std::optional<float> before = readOpt(h.ref);
    if (auto w = h.hold.update(hold, before)) {
      if (h.ref.set(*w))
        emit(std::string(h.what) + (hold ? " off" : " put back") + " (" +
             h.ref.name() + " " + (before ? fmt2(*before) : "-") + " -> " +
             fmt2(*w) + ")");
    }
    if (h.hold.takeChanged())
      emit(std::string(h.what) + " changed by the user or the aircraft (" +
           h.ref.name() + "): left as it is");
  }

  Crew::Switches sw;
  sw.doors = doorsOn();
  sw.lights = lightsOn();
  sw.nightPreset = options_.nightCruiseLights;
  const Crew::Step step = crew_.tick(plan, readings(), s, sw);
  for (const std::string& c : step.commands)
    if (!run(c)) emit("no command " + c);
  for (const std::string& e : step.events) emit(e);

  if (pendingCheck_ >= 0) {
    runCheck(pendingCheck_, s.t);
    pendingCheck_ = -1;
  }
  if (tried_.live && tried_.row >= 0 &&
      tried_.row < static_cast<int>(checkList_.size())) {
    const std::string now =
        reading(checkList_[static_cast<size_t>(tried_.row)]);
    const bool done = s.t - tried_.at >= tried_.settle;
    tried_.result =
        "before " + tried_.before + (done ? ", after " : ", now ") + now;
    if (done) {
      emit("check: " + tried_.result);
      tried_.live = false;
    }
  }
}

void Actions::newFlight() { crew_.reset(); }

void Actions::everyFrame(double t) {
  if (!active_ || !fmod_) return;
  const bool silence = silenceOn();
  fmod_->tick(t, silence);
  if (silence != silencing_ && fmod_->ready()) {
    silencing_ = silence;
    emit(silence ? "the aircraft's cabin crew announcements silenced"
                 : "the aircraft's cabin crew announcements given back");
  }
  const std::string note = fmod_->takeNote();
  if (!note.empty()) emit(note);
}

void Actions::soundBanksUnloading() {
  if (fmod_) fmod_->forget();
}

bool Actions::aircraftSpeaking() const {
  return active_ && fmod_ && fmod_->aircraftSpeaking();
}

bool Actions::ownChime() const { return active_ && chimeOn(); }

void Actions::chime() {
  if (!ownChime()) return;
  if (callChime_.set(1)) emit("cabin chime (the aircraft's own)");
}

void Actions::release(bool aircraftGone) {
  if (fmod_) {
    if (aircraftGone)
      fmod_->forget();
    else
      fmod_->restore();
    silencing_ = false;
  }
  for (Held& h : held_) {
    if (aircraftGone || !h.ref.good()) {
      h.hold.forget();
      continue;
    }
    if (auto v = h.hold.release()) {
      if (h.ref.set(*v))
        emit(std::string(h.what) + " put back (" + h.ref.name() + " " +
             fmt2(*v) + ")");
    }
  }
  crew_.reset();
  pendingCheck_ = -1;
}

std::string Actions::reading(const Check& c) const {
  switch (c.kind) {
    case Check::DoorRow:
      return door_[c.index].good() ? fmt2(ratioOf(door_[c.index])) : "-";
    case Check::Stairs: {
      const int hidden = wholeOf(stairsHidden_);
      if (hidden < 0) return "-";
      return hidden == 1 ? "stowed" : "out " + fmt2(ratioOf(stairsRatio_));
    }
    case Check::Light:
      return paxLightName(wholeOf(paxLighting_));
    case Check::Chime:
      return callChime_.good() ? fmt2(ratioOf(callChime_)) : "-";
    case Check::Option: {
      const Held& h = held_[c.index];
      return h.ref.good() ? fmt2(ratioOf(h.ref)) : "-";
    }
  }
  return "-";
}

std::vector<CheckRow> Actions::checks() const {
  std::vector<CheckRow> rows;
  for (size_t k = 0; k < checkList_.size(); ++k) {
    const Check& c = checkList_[k];
    CheckRow r;
    r.reading = reading(c);
    switch (c.kind) {
      case Check::DoorRow:
        r.label = std::string("Door ") +
                  doorInfo(static_cast<Door>(c.index)).name + ": open or close";
        r.available = has(doorInfo(static_cast<Door>(c.index)).open);
        break;
      case Check::Stairs:
        r.label = "Airstairs: stow or deploy";
        r.available = has(kAirstairsToggle);
        break;
      case Check::Light: {
        const int l = kCheckLights[c.index];
        r.label = std::string("Cabin lights: ") +
                  (l == kNight ? "night / sleep" : paxLightName(l));
        r.available = has(paxLightCommand(l));
        break;
      }
      case Check::Chime:
        r.label = "Cabin chime (call_chime)";
        r.available = callChime_.canWrite();
        break;
      case Check::Option:
        r.label = std::string(held_[c.index].what) + " (" +
                  held_[c.index].ref.name() + ")";
        r.available = false;  // shown only; held by the actions
        break;
    }
    if (static_cast<int>(k) == tried_.row) r.result = tried_.result;
    rows.push_back(std::move(r));
  }
  return rows;
}

void Actions::tryCheck(int row) {
  if (row >= 0 && row < static_cast<int>(checkList_.size()))
    pendingCheck_ = row;
}

void Actions::runCheck(int row, double t) {
  const Check& c = checkList_[static_cast<size_t>(row)];
  tried_ = Tried{};
  tried_.row = row;
  tried_.live = true;
  tried_.at = t;
  tried_.before = reading(c);
  bool ok = false;
  switch (c.kind) {
    case Check::DoorRow: {
      const DoorInfo& d = doorInfo(static_cast<Door>(c.index));
      const bool open = ratioOf(door_[c.index]) >= 0.5f;
      ok = run(open ? d.close : d.open);
      tried_.settle = d.settleS;
      emit(std::string("check: ") + (open ? "close door " : "open door ") +
           d.name);
      break;
    }
    case Check::Stairs:
      ok = run(kAirstairsToggle);
      tried_.settle = kStairsSettleS;
      emit("check: airstairs toggle");
      break;
    case Check::Light: {
      const int l = kCheckLights[c.index];
      ok = run(paxLightCommand(l));
      tried_.settle = kLightSettleS;
      emit(std::string("check: cabin lights ") + paxLightName(l));
      break;
    }
    case Check::Chime:
      ok = callChime_.set(1);
      tried_.settle = kChimeSettleS;
      emit("check: cabin chime");
      break;
    case Check::Option:
      break;
  }
  if (!ok) {
    tried_.result = "the aircraft didn't take it";
    tried_.live = false;
    emit("check: the aircraft didn't take it");
  }
}

}  // namespace xn::zibo737
