/// @file
/// Implements `src/aircraft/toliss/toliss_signals.h`; the interface is
/// documented there.
#include "aircraft/toliss/toliss_signals.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace xn::toliss {

namespace {

/// Full travel of an ACP volume knob in degrees.
constexpr float kKnobFullDeg = 270.0f;

/// The numbers of a "raw <name> = v v v" event row, or nothing when the
/// event is about another dataref.
std::optional<std::vector<float>> rawValues(const std::string& event,
                                            const char* name) {
  const std::string head = std::string("raw ") + name + " = ";
  if (event.rfind(head, 0) != 0) return std::nullopt;
  std::vector<float> v;
  std::istringstream in(event.substr(head.size()));
  for (float x; in >> x;) v.push_back(x);
  return v;
}

}  // namespace

std::optional<Phase> fmgsPhase(int raw) {
  if (raw < 0 || raw > 7) return std::nullopt;
  return static_cast<Phase>(raw);
}

int groundSpoilersFromSd(const std::vector<float>& sd) {
  if (sd.size() < 2 || !std::isfinite(sd[0]) || !std::isfinite(sd[1]))
    return -1;
  const long a = std::lround(sd[0]), b = std::lround(sd[1]);
  if (a == 2 || b == 2) return -1;
  return (a == 1 && b == 1) ? 1 : 0;
}

int switchAt(const std::vector<float>& switches, int index, int position) {
  if (index < 0 || index >= static_cast<int>(switches.size()) ||
      !std::isfinite(switches[static_cast<size_t>(index)]))
    return -1;
  return std::lround(switches[static_cast<size_t>(index)]) == position ? 1 : 0;
}

float acpKnobVolume(float raw) {
  if (!std::isfinite(raw)) return 0.0f;
  return std::clamp(raw > 1.5f ? raw / kKnobFullDeg : raw, 0.0f, 1.0f);
}

AircraftCues cuesFrom(int ecamPhase, int qpacPhase, float throttleMax,
                      float flxDetent) {
  AircraftCues c;
  // ECAM flight phase: 3 takeoff power, 4 80 kt, 5 lift-off, 6 1500 ft,
  // 7 800 ft on the approach, 8 touchdown, 9 80 kt on the rollout.
  if (ecamPhase >= 0) {
    c.takeoffPower = (ecamPhase == 3 || ecamPhase == 4) ? 1 : 0;
    c.liftoff = (ecamPhase >= 5 && ecamPhase <= 7) ? 1 : 0;
    c.touchdown = (ecamPhase == 8 || ecamPhase == 9) ? 1 : 0;
    if (c.liftoff == 1)
      c.liftoffLabel = "ECAM flight phase " + std::to_string(ecamPhase);
  }
  // A real lever position is never below -1.5 (full reverse is about -1).
  if (throttleMax > -1.5f) c.leversAtTakeoff = throttleMax >= flxDetent ? 1 : 0;
  // QPAC flight phase: 6 lift-off, 9 and 10 the approach. 7 and 8 (the climb)
  // are left to the purser's own estimate, which tells climb from cruise.
  c.phaseKnown = qpacPhase >= 1;
  if (qpacPhase == 6)
    c.phaseHint = Phase::Takeoff;
  else if (qpacPhase == 9 || qpacPhase == 10)
    c.phaseHint = Phase::Approach;
  return c;
}

float volumeFactor(float internal, int overrideOn) {
  return overrideOn == 1 && internal >= 0 ? internal : -1.0f;
}

void LegacyTrace::apply(TraceRow& row) {
  if (row.isEvent) {
    if (auto v = rawValues(row.event, "AirbusFBW/OHPLightSwitches"))
      rawStrobe_ = switchAt(*v, options_.strobeSwitchIndex, 2);
    if (auto v = rawValues(row.event, "AirbusFBW/NoSmokingSignsOn");
        v && !v->empty())
      rawNoSmoking_ = v->front() > 0.5f ? 1 : 0;
    if (auto v = rawValues(row.event, "AirbusFBW/GearLever"); v && !v->empty())
      rawGear_ = v->front() > 0.5f ? 1 : 0;
    if (auto v = rawValues(row.event, "AirbusFBW/SDSpoilerArray"))
      rawSpoilers_ = groundSpoilersFromSd(*v);
    return;
  }
  SimState& s = row.state;
  auto extra = [&](const char* name) -> const std::string* {
    const auto it = row.extra.find(name);
    return it == row.extra.end() ? nullptr : &it->second;
  };
  auto number = [&](const char* name, float fallback) {
    const std::string* v = extra(name);
    return v ? static_cast<float>(std::atof(v->c_str())) : fallback;
  };
  auto whole = [&](const char* name, int fallback) {
    const std::string* v = extra(name);
    return v ? std::atoi(v->c_str()) : fallback;
  };
  s.fmsPhase = fmgsPhase(s.fmsPhaseRaw);
  s.cues = cuesFrom(whole("ecam_phase", -1), whole("qpac_phase", -1),
                    number("throttle_max", kUnknownLever),
                    options_.throttleFlxDetent);
  s.aircraftVolume = volumeFactor(number("toliss_vol_internal", -1.0f),
                                  whole("toliss_vol_override", -1));
  if (s.strobeOn < 0) s.strobeOn = rawStrobe_;
  if (s.noSmoking < 0) s.noSmoking = rawNoSmoking_;
  if (s.gearLeverDown < 0) s.gearLeverDown = rawGear_;
  if (rawSpoilers_ > -2) s.groundSpoilers = rawSpoilers_;
}

}  // namespace xn::toliss
