/// @file
/// The part of the simulator snapshot that is the same on every aircraft.
#pragma once

#include <string>
#include <vector>

#include "core/sim_state.h"
#include "sim/dataref.h"

namespace xn {

/// Reads X-Plane's own datarefs into a `SimState`: position, speeds, clocks,
/// the sun, pause and replay, the camera, engines, fuel and the lights X-Plane
/// models itself. It also keeps the purser clock. The loaded aircraft's
/// module adds the rest.
class XpCommon {
 public:
  XpCommon();
  XpCommon(const XpCommon&) = delete;  // `refs_` points into this object
  XpCommon& operator=(const XpCommon&) = delete;

  /// Looks the datarefs up again. Call it when an aircraft activates.
  /// Returns the names that are missing.
  std::vector<std::string> resolve();
  /// Starts a new snapshot.
  /// @param dt Seconds since the last tick. The purser clock advances by it
  ///        unless the sim is paused.
  /// @param cabin The loaded aircraft's cabin
  ///        (`AircraftModule::cabinGeometry`): where the cockpit ends.
  SimState read(float dt, const CabinGeometry& cabin);

 private:
  /// Fills the camera's place in the aircraft (`SimState::cameraAftM`...).
  void readCamera(SimState& s) const;

  double clock_ = 0;
  float icaoTimer_ = 0;
  std::string icao_;

  DataRef onGround_{"sim/flightmodel/failures/onground_any"};
  DataRef gs_{"sim/flightmodel/position/groundspeed"};
  DataRef ias_{"sim/flightmodel/position/indicated_airspeed"};
  DataRef agl_{"sim/flightmodel/position/y_agl"};
  DataRef msl_{"sim/flightmodel/position/elevation"};
  DataRef vs_{"sim/flightmodel/position/vh_ind_fpm"};
  DataRef lat_{"sim/flightmodel/position/latitude"};
  DataRef lon_{"sim/flightmodel/position/longitude"};
  DataRef beacon_{"sim/cockpit2/switches/beacon_on"};
  DataRef landing_{"sim/cockpit2/switches/landing_lights_on"};
  DataRef speedbrake_{"sim/cockpit2/controls/speedbrake_ratio"};
  DataRef engRunning_{"sim/flightmodel/engine/ENGN_running"};
  DataRef engCount_{"sim/aircraft/engine/acf_num_engines"};
  DataRef fuel_{"sim/flightmodel/weight/m_fuel_total"};
  DataRef localTime_{"sim/time/local_time_sec"};
  DataRef zulu_{"sim/time/zulu_time_sec"};
  DataRef sun_{"sim/graphics/scenery/sun_pitch_degrees"};
  DataRef paused_{"sim/time/paused"};
  DataRef replay_{"sim/time/is_in_replay"};
  DataRef external_{"sim/graphics/view/view_is_external"};
  DataRef headZ_{"sim/graphics/view/pilots_head_z"};
  DataRef eyeZ_{"sim/aircraft/view/acf_peZ"};
  // X-Plane's camera and the aircraft's place and attitude, for the camera
  // in the aircraft's axes. A camera plugin (X-Camera) moves the camera
  // without moving `pilots_head_z` (Run1(XN)).
  DataRef viewX_{"sim/graphics/view/view_x"};
  DataRef viewY_{"sim/graphics/view/view_y"};
  DataRef viewZ_{"sim/graphics/view/view_z"};
  DataRef localX_{"sim/flightmodel/position/local_x"};
  DataRef localY_{"sim/flightmodel/position/local_y"};
  DataRef localZ_{"sim/flightmodel/position/local_z"};
  DataRef psi_{"sim/flightmodel/position/psi"};
  DataRef theta_{"sim/flightmodel/position/theta"};
  DataRef phi_{"sim/flightmodel/position/phi"};
  DataRef icaoRef_{"sim/aircraft/view/acf_ICAO"};

  std::vector<DataRef*> refs_;
};

/// True when a dataref holds a value other than zero.
bool truthy(const DataRef& r);
/// Limits a value to 0…1. Not-a-number becomes 0.
float clamp01(float v);

}  // namespace xn
