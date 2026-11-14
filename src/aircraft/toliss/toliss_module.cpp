/// @file
/// Implements `src/aircraft/toliss/toliss_module.h`; the interface is
/// documented there.
#include "aircraft/toliss/toliss_module.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "aircraft/toliss/toliss_signals.h"
#include "aircraft/toliss/toliss_types.h"
#include "aircraft/xp_common.h"
#include "core/files.h"
#include "core/log.h"

namespace xn::toliss {

namespace {

std::string fmt(const char* f, double v) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), f, v);
  return buf;
}

}  // namespace

Module::Module(const Options& options) : options_(options) {
  raw_ = {&apPhase_,      &seatbelt_,  &noSmoking_,      &paxDoors_,
          &paxDoorModes_, &slides_,    &noPax_,          &masks_,
          &acVolts_,      &doorAngle_, &ckptDoor_,       &satcomInhibit_,
          &flightNo_,     &cruiseAlt_, &parkBrake_,      &chocks_,
          &ohpLights_,    &ecam_,      &qpac_,           &throttle_,
          &sdSpoilers_,   &gearLever_, &flapLever_,      &cargoDoors_,
          &leftLand_,     &rightLand_, &volInternalRef_, &volCockpit_,
          &volSpkrs_,     &doorBlend_};
  for (auto& r : acpKnob_) raw_.push_back(&r);
  for (auto& r : acpVol_) raw_.push_back(&r);
  for (auto& r : acpPaKnob_) raw_.push_back(&r);
  for (auto& r : acpCabKnob_) raw_.push_back(&r);

  refs_ = {&apPhase_,       &seatbelt_,       &noSmoking_,     &paxDoors_,
           &paxDoorModes_,  &slides_,         &noPax_,         &masks_,
           &cabinAlt_,      &acVolts_,        &doorAngle_,     &ckptDoor_,
           &satcomInhibit_, &flightNo_,       &cruiseAlt_,     &parkBrake_,
           &chocks_,        &ohpLights_,      &ecam_,          &qpac_,
           &throttle_,      &sdSpoilers_,     &gearLever_,     &flapLever_,
           &cargoDoors_,    &landing_,        &landingSwitch_, &leftLand_,
           &rightLand_,     &volInternalRef_, &volCockpit_,    &volSpkrs_,
           &doorBlend_};
  for (auto& r : acpKnob_) refs_.push_back(&r);
  for (auto& r : acpVol_) refs_.push_back(&r);
  for (auto& r : acpPaKnob_) refs_.push_back(&r);
  for (auto& r : acpCabKnob_) refs_.push_back(&r);
}

Recognition Module::recognise(const LoadedAircraft& aircraft) {
  // Either dataref exists as soon as the ToLiss systems plugin is running.
  DataRef phase("AirbusFBW/APPhase");
  DataRef v1("toliss_airbus/performance/V1");
  const bool marker = phase.resolve() || v1.resolve();
  return classify(aircraft, marker);
}

std::vector<std::string> Module::activate(const LoadedAircraft& aircraft) {
  aircraft_ = aircraft;
  std::vector<std::string> missing;
  for (DataRef* r : refs_)
    if (!r->resolve()) missing.emplace_back(r->name());
  // The override dataref is looked up again on the next tick.
  overrideName_.clear();
  volOverrideRef_ = DataRef();
  reloadFiles();
  return missing;
}

void Module::deactivate() { aircraft_ = LoadedAircraft{}; }

void Module::reloadFiles() {
  tlscfgEngine_.clear();
  if (aircraft_.liveryPath.empty()) return;
  const Ini cfg =
      Ini::parse(readTextFile(aircraft_.liveryPath + "livery.tlscfg"));
  if (const std::string* e = cfg.get("", "eng_type")) tlscfgEngine_ = *e;
}

void Module::useOverrideRef(const std::string& name) {
  // Looked up once per name, and again after an aircraft load.
  if (name == overrideName_) return;
  overrideName_ = name;
  volOverrideRef_ = name.empty() ? DataRef() : DataRef(overrideName_.c_str());
  if (!name.empty() && !volOverrideRef_.resolve())
    logWarn("volume override dataref %s not found", name.c_str());
}

std::vector<float> Module::landingCandidates() const {
  auto anyOn = [](const std::vector<float>& v) {
    return std::any_of(v.begin(), v.end(), [](float x) { return x > 0.5f; });
  };
  std::vector<float> c(3, -1.0f);
  if (landing_.good()) c[0] = truthy(landing_) ? 1.0f : 0.0f;
  if (landingSwitch_.good())
    c[1] = anyOn(landingSwitch_.getArray(16)) ? 1.0f : 0.0f;
  if (leftLand_.good() || rightLand_.good())
    c[2] = (leftLand_.getFloat().value_or(0) > 0.5f ||
            rightLand_.getFloat().value_or(0) > 0.5f)
               ? 1.0f
               : 0.0f;
  return c;
}

void Module::read(SimState& s) {
  const Options& set = options_;

  if (auto p = apPhase_.getInt()) {
    s.fmsPhaseRaw = *p;
    s.fmsPhase = fmgsPhase(*p);
  }
  s.cruiseAltFt = cruiseAlt_.getFloat().value_or(0);

  s.seatbelt = truthy(seatbelt_);
  if (noSmoking_.good()) s.noSmoking = truthy(noSmoking_) ? 1 : 0;
  // The strobe switch at ON (2). AUTO (1) is not a line-up signal.
  if (ohpLights_.good())
    s.strobeOn = switchAt(ohpLights_.getArray(32), set.strobeSwitchIndex, 2);
  {
    const std::vector<float> c = landingCandidates();
    const size_t pick = static_cast<size_t>(
        std::clamp(set.landingLightSource, 0, static_cast<int>(c.size()) - 1));
    s.landingLights = c[pick] >= 0 ? c[pick] > 0.5f : c[0] > 0.5f;
  }
  s.parkBrake = truthy(parkBrake_);
  s.chocks = truthy(chocks_);

  const std::vector<float> doors = paxDoors_.getArray(20);
  s.doorCount = static_cast<int>(doors.size());
  for (size_t k = 0; k < doors.size(); ++k) {
    const float d = clamp01(doors[k]);
    s.doorMax = std::max(s.doorMax, d);
    if (k == 0) s.doorFwd = d;
  }
  const std::vector<float> slides = slides_.getArray(20);
  s.slidesKnown = !slides.empty();
  s.slidesArmed = std::any_of(slides.begin(), slides.end(),
                              [](float v) { return std::fabs(v) > 0.01f; });

  s.pax = std::max(0, noPax_.getInt().value_or(0));
  if (acVolts_.good()) {
    const std::vector<float> v = acVolts_.getArray(8);
    s.acPower = v.empty() || std::any_of(v.begin(), v.end(),
                                         [](float x) { return x > 90.0f; });
  }
  s.paxMasks = truthy(masks_);
  s.cabinAltFt = cabinAlt_.getFloat().value_or(0);

  // A reception channel is on when its knob is pulled on any of the three
  // ACPs. The loudest of those knobs gives the volume.
  for (int a = 0; a < 3; ++a) {
    const std::vector<float> knobs = acpKnob_[a].getArray(16);
    const std::vector<float> vols = acpVol_[a].getArray(16);
    auto at = [](const std::vector<float>& v, int i) {
      return (i >= 0 && i < static_cast<int>(v.size()))
                 ? v[static_cast<size_t>(i)]
                 : 0.0f;
    };
    auto volume = [&](const DataRef& knob, int index) {
      if (knob.good())
        if (auto v = knob.getFloat(); v && std::isfinite(*v))
          return acpKnobVolume(*v);
      return clamp01(at(vols, index));
    };
    if (std::fabs(at(knobs, set.acpPaIndex)) > 0.01f) {
      s.paListen = true;
      s.paListenVol =
          std::max(s.paListenVol, volume(acpPaKnob_[a], set.acpPaIndex));
    }
    if (std::fabs(at(knobs, set.acpCabIndex)) > 0.01f) {
      s.interphoneOn = true;
      s.interphoneVol =
          std::max(s.interphoneVol, volume(acpCabKnob_[a], set.acpCabIndex));
    }
  }
  // The door angle is in degrees (0 to 80) on some models and a ratio on
  // others. On the A320neo `CockpitDoorAngle` stays 0 and `ckpt/door` moves.
  float door = 0;
  if (auto a = doorAngle_.getFloat())
    door = std::max(door, *a > 1.5f ? *a / 80.0f : *a);
  if (auto c = ckptDoor_.getFloat())
    door = std::max(door, *c > 1.5f ? *c / 80.0f : *c);
  s.cockpitDoor = clamp01(door);

  s.flightNumber = flightNo_.getString(16);
  if (auto inh = satcomInhibit_.getInt()) s.satcom = *inh == 0;

  // Cross-checks (VP_SPEC § 4.2).
  ecamPhase_ = ecam_.getInt().value_or(-1);
  qpacPhase_ = qpac_.getInt().value_or(-1);
  throttleMax_ = kUnknownLever;
  if (throttle_.good()) {
    // One entry per lever; the highest of the first five covers every model.
    for (float x : throttle_.getArray(5))
      if (std::isfinite(x)) throttleMax_ = std::max(throttleMax_, x);
  }
  s.cues =
      cuesFrom(ecamPhase_, qpacPhase_, throttleMax_, set.throttleFlxDetent);
  if (sdSpoilers_.good())
    s.groundSpoilers = groundSpoilersFromSd(sdSpoilers_.getArray(16));
  if (auto v = gearLever_.getInt()) s.gearLeverDown = *v != 0 ? 1 : 0;

  // ToLiss's own sound mix (VP_SPEC § 4.5).
  volInternal_ = -1;
  if (auto v = volInternalRef_.getFloat(); v && std::isfinite(*v))
    volInternal_ = clamp01(*v);
  if (auto v = doorBlend_.getFloat(); v && std::isfinite(*v))
    s.doorBlend = clamp01(*v);
  useOverrideRef(set.volumeOverrideRef);
  volOverride_ =
      volOverrideRef_.good() ? (truthy(volOverrideRef_) ? 1 : 0) : -1;
  s.aircraftVolume = volumeFactor(volInternal_, volOverride_);
}

CabinGeometry Module::cabinGeometry() const {
  return cabinOf(familyFrom(aircraft_.acfIcao, aircraft_.acfFile), options_);
}

AircraftType Module::type(const SimState& s) const {
  return currentType({s.acfIcao, aircraft_.acfIcao, aircraft_.acfFile,
                      aircraft_.liveryName, tlscfgEngine_},
                     options_);
}

std::vector<ObservedCommand> Module::observedCommands() const {
  // The overhead CALLS buttons. MECH calls the ground mechanic, not the cabin.
  return {{"AirbusFBW/purser/fwd", true},  {"AirbusFBW/purser/aft", true},
          {"AirbusFBW/purser/all", true},  {"AirbusFBW/purser/mid", true},
          {"AirbusFBW/purser/exit", true}, {"AirbusFBW/purser/mech", false}};
}

std::vector<std::pair<std::string, std::string>> Module::rawSnapshot() const {
  std::vector<std::pair<std::string, std::string>> rows;
  for (const DataRef* r : raw_) {
    std::string v;
    char buf[32];
    if (!r->good())
      v = "(missing)";
    else if (r->types() & xplmType_Data)
      v = "\"" + r->getString(64) + "\"";
    else if (r->arraySize() > 0) {
      for (float x : r->getArray(24)) {
        std::snprintf(buf, sizeof(buf), "%s%.2f", v.empty() ? "" : " ",
                      static_cast<double>(x));
        v += buf;
      }
    } else if (auto f = r->getFloat()) {
      std::snprintf(buf, sizeof(buf), "%.2f", static_cast<double>(*f));
      v = buf;
    }
    rows.emplace_back(r->name(), v);
  }
  return rows;
}

std::vector<LiveRow> Module::liveInputs(const SimState& s) const {
  auto phase = [](int p) { return p >= 0 ? std::to_string(p) : "-"; };
  return {
      {"ECAM / QPAC phase", phase(ecamPhase_) + " / " + phase(qpacPhase_),
       "Purser phase"},
      {"Thrust levers",
       throttleMax_ > -1.5f ? fmt("%.2f", throttleMax_) : std::string("-"),
       "ECAM / QPAC phase"},
      {"ACP PA / CAB",
       std::string(s.paListen ? fmt("PA %.0f%%", s.paListenVol * 100)
                              : "PA in") +
           " / " +
           (s.interphoneOn ? fmt("CAB %.0f%%", s.interphoneVol * 100)
                           : "CAB in"),
       "AC power"},
      {"ToLiss volume",
       volInternal_ < 0
           ? std::string("-")
           : fmt("internal %.0f%%", volInternal_ * 100) +
                 (volOverride_ == 1   ? ", ISCS override on"
                  : volOverride_ == 0 ? ", ISCS override off"
                                      : ", override switch unknown (Settings "
                                        "> ToLiss)"),
       "Cabin PA heard"},
  };
}

}  // namespace xn::toliss
