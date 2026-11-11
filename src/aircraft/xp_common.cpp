/// @file
/// Implements `src/aircraft/xp_common.h`; the interface is documented there.
#include "aircraft/xp_common.h"

#include <algorithm>
#include <cmath>

namespace xn {

namespace {

constexpr float kMsToKt = 1.943844f;
constexpr float kMToFt = 3.280840f;
/// An aircraft can change its ICAO designator after it loaded (an engine
/// option, for example), so it is read again this often, in seconds.
constexpr float kIcaoRefresh = 5.0f;

}  // namespace

bool truthy(const DataRef& r) {
  const auto v = r.getFloat();
  return v && std::fabs(*v) > 0.01f;
}

float clamp01(float v) {
  return std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.0f;
}

XpCommon::XpCommon() {
  refs_ = {&onGround_,   &gs_,         &ias_,      &agl_,      &msl_,
           &vs_,         &lat_,        &lon_,      &beacon_,   &landing_,
           &speedbrake_, &engRunning_, &engCount_, &fuel_,     &localTime_,
           &sun_,        &paused_,     &replay_,   &external_, &headZ_,
           &eyeZ_,       &icaoRef_,    &zulu_};
}

std::vector<std::string> XpCommon::resolve() {
  std::vector<std::string> missing;
  for (DataRef* r : refs_)
    if (!r->resolve()) missing.emplace_back(r->name());
  icaoTimer_ = kIcaoRefresh;  // read the designator on the first tick
  return missing;
}

SimState XpCommon::read(float dt, float cabinViewAftM) {
  SimState s;
  s.paused = truthy(paused_);
  s.replay = truthy(replay_);
  if (!s.paused && std::isfinite(dt) && dt > 0 && dt < 10) clock_ += dt;
  s.t = clock_;

  s.onGround = onGround_.good() ? truthy(onGround_) : true;
  s.gsKt = gs_.getFloat().value_or(0) * kMsToKt;
  s.iasKt = ias_.getFloat().value_or(0);
  s.aglFt = agl_.getFloat().value_or(0) * kMToFt;
  s.mslFt = msl_.getFloat().value_or(0) * kMToFt;
  s.vsFpm = vs_.getFloat().value_or(0);
  s.lat = lat_.getDouble().value_or(0);
  s.lon = lon_.getDouble().value_or(0);

  s.beacon = truthy(beacon_);
  s.landingLights = truthy(landing_);
  s.speedbrake = speedbrake_.getFloat().value_or(0);

  s.engineCount = std::clamp(engCount_.getInt().value_or(2), 1, 8);
  const std::vector<float> eng = engRunning_.getArray(8);
  for (int k = 0;
       k < std::min<int>(s.engineCount, static_cast<int>(eng.size())); ++k)
    if (eng[static_cast<size_t>(k)] > 0.5f) ++s.enginesRunning;

  s.fuelKg = fuel_.getFloat().value_or(0);

  s.viewExternal = truthy(external_);
  if (!s.viewExternal && headZ_.good() && eyeZ_.good())
    s.cameraInCabin =
        headZ_.getFloat().value_or(0) - eyeZ_.getFloat().value_or(0) >
        cabinViewAftM;
  s.sunPitchDeg = sun_.getFloat().value_or(45);
  s.localTimeSec = localTime_.getFloat().value_or(43200);
  if (auto z = zulu_.getFloat(); z && std::isfinite(*z)) s.zuluSec = *z;

  icaoTimer_ += std::max(0.0f, dt);
  if (icaoTimer_ >= kIcaoRefresh) {
    icaoTimer_ = 0;
    icao_ = icaoRef_.getString(40);
  }
  s.acfIcao = icao_;
  return s;
}

}  // namespace xn
