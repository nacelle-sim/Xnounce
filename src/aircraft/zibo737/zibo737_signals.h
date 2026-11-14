/// @file
/// How raw Zibo / LevelUp 737 dataref values become the aircraft-neutral
/// signals of `SimState`.
///
/// Pure code, shared by the live module and the scripted 737 flights of the
/// tests, so both decode a tick the same way. The datarefs and their encodings
/// are listed in docs/aircraft/zibo737.md.
#pragma once

#include <optional>
#include <vector>

#include "core/sim_state.h"

namespace xn::zibo737 {

/// The 737's own values of one tick. A value the aircraft doesn't publish
/// stays at its "missing" default (-1, or empty).
struct Raw {
  /// `laminar/B738/FMS/flight_phase`: 0 takeoff (also before the flight and
  /// after the rollout), 1 climb, 2 cruise, 3 step climb, 4 step descent,
  /// 5 descent, 6 approach, 7 go-around armed, 8 go-around.
  int flightPhase = -1;
  /// `laminar/B738/air_ground_sensor`: 1 on the ground, 0 in the air.
  int airGround = -1;
  /// `laminar/B738/autopilot/ap_takeoff`: TO/GA engaged.
  int apTakeoff = -1;
  /// Highest thrust lever ratio (`sim/cockpit2/engine/actuators/
  /// throttle_ratio`), or -1.
  float throttleMax = -1;
  /// `laminar/b738/fmodpack/flightphase_landed`: 1 from touchdown through
  /// the rollout.
  int landed = -1;
  /// `laminar/b738/fmodpack/seatbelt_on_light` (lit), and the switch
  /// `laminar/B738/toggle_switch/seatbelt_sign_pos` (0 OFF, 1 AUTO, 2 ON).
  int seatbeltLight = -1;
  int seatbeltSwitch = -1;
  /// `laminar/B738/toggle_switch/no_smoking_pos` (0 OFF, 1 AUTO, 2 ON).
  int noSmokingSwitch = -1;
  /// `laminar/B738/controls/gear_handle_down`: 0 up, 0.5 off, 1 down.
  float gearHandle = -1;
  /// Landing lights: the retractable ones (`switch/land_lights_ret_*_pos`,
  /// 0 retract, 1 extend, 2 on) and the fixed ones (`switch/land_lights_*_pos`,
  /// 0 or 1). One entry per light that exists.
  std::vector<float> retractableLights;
  std::vector<float> fixedLights;
  /// `laminar/B738/toggle_switch/position_light_pos`: -1 STEADY, 0 OFF,
  /// 1 STROBE & STEADY. -9 when missing.
  int positionLight = -9;
  /// `laminar/B738/parking_brake_pos` and `laminar/B738/fms/chock_status`.
  int parkBrake = -1;
  int chocks = -1;
  /// The passenger and service doors `737u/doors/L1`, `R1`, `L2`, `R2`,
  /// `L_mid`, `R_mid` that exist, L1 first: 0 closed to 1 open.
  std::vector<float> doors;
  /// Passengers in `laminar/B738/tab/zone1..5_payload` (the sum of the first
  /// three entries of each zone), or -1.
  int pax = -1;
  /// Seats of the cabin layout, the sum of `laminar/B738/std_pax_num`, or 0.
  int seats = 0;
  /// AC transfer buses (`laminar/B738/electric/ac_tnsbus1/2_status`, 1
  /// powered), and what feeds them: engine generator on the bus
  /// (`annunciator/gen_off_bus1/2` unlit, see `annunciatorLit`) and APU
  /// generator (`electrical/apu_power_bus1/2`).
  int bus1 = -1, bus2 = -1;
  int genOff1 = -1, genOff2 = -1;
  int apuBus1 = -1, apuBus2 = -1;
  /// `laminar/B738/annunciator/pax_oxy` (PASS OXY ON) or the failure
  /// `sim/operation/failures/rel_pass_o2_on` at 6.
  int masks = -1;
  /// `laminar/B738/door/flt_dk_door_ratio`: 0 closed to 1 open.
  float cockpitDoor = -1;
  /// `laminar/B738/autopilot/fmc_cruise_alt` (ft), or -1.
  float cruiseAlt = -1;
  /// The cabin interphone selected on an audio panel
  /// (`laminar/B738/audio/{capt,fo}/mic_button5`), or -1.
  int interphone = -1;
  /// The PA microphone selected on an audio panel
  /// (`laminar/B738/audio/{capt,fo}/mic_button6`), or -1. The 737 has no PA
  /// reception knob, so this is the cockpit listening to the PA (VP_SPEC
  /// § 4.2).
  int paMic = -1;
  /// Seconds left of the aircraft's own captain message
  /// (`laminar/b738/fmodpack/msg_playing`), 0 when silent, or -1.
  float msgPlaying = -1;
};

/// An annunciator dataref reads lit above this. The LevelUp's lights read
/// 0.05 when lit with the panel lights dimmed (Run1(XN)), the Zibo's up to 1.
constexpr float kAnnunciatorLit = 0.01f;
/// 1 when an annunciator reading is lit, 0 when not, -1 when unknown (< 0 or
/// not finite).
int annunciatorLit(float ratio);

/// The id of the source feeding one AC transfer bus: 3 an engine generator,
/// 2 the APU, 1 ground power (powered, but neither), 0 unpowered, -1 unknown.
int busSource(int powered, int genOff, int apuOnBus);

/// Follows the EFB's Airside Services (`laminar/B738/tab/auto_flight`) into
/// `SimState::svc` (VP_SPEC § 4.1c, D-29), as `B738.tablet.lua` runs it:
/// 0 idle, 1 started, 2 loading, 3 loaded (through the flight), 4 parked
/// after landing, 5 cargo doors, 6 the forward doors, 7 deboarding, then 0.
/// Boarding starts on the tablet's own rule: while loading, the NO SMOKING
/// switch ON or AUTO after it was OFF since the service started, or the
/// tablet's option for an aircraft without the switch; else the first
/// passenger counted on board.
class AirsideFollow {
 public:
  /// One tick.
  /// @param state `tab/auto_flight`, or -1.
  /// @param noSmoking `toggle_switch/no_smoking_pos` (0 off), or -1.
  /// @param option `fms/no_smoking` (0 switch, 1 chime only, 2 none), or -1.
  /// @param pax Passengers counted on board, or -1.
  void update(int state, int noSmoking, int option, int pax,
              AircraftServices& svc);
  /// A new flight (a reload or a teleport).
  void reset() { *this = AirsideFollow{}; }
  /// A line for the log when the service moves on, once.
  std::string takeNote();

 private:
  int last_ = -1;
  bool sawOff_ = false;
  int firstPax_ = -1;
  AircraftServices out_;
  std::string note_;
};

/// Decodes the 737's values into the snapshot. Remembers whether this flight
/// has been airborne, because the FMC's phase 0 serves before the flight, for
/// the takeoff and after the rollout.
class Signals {
 public:
  explicit Signals(float takeoffLever = 0.85f) : takeoffLever_(takeoffLever) {}
  void setTakeoffLever(float l) { takeoffLever_ = l; }
  /// Forgets the flight: a new aircraft or a reload.
  void reset();
  /// Adds the 737's signals to `s`, which already holds X-Plane's own values
  /// (on ground, speeds, height, engines running).
  void apply(const Raw& r, SimState& s);

  /// The FMC phase as Xnounce's phase.
  /// - 1 to 8 map directly (3 and 4, the step climbs and descents, are
  ///   Cruise; 7, go-around armed, is still Approach).
  /// - 0 on the ground: PreFlight before the flight (Takeoff once rolling
  ///   with the engines running), Done after it.
  /// - 0 in the air (just after lift-off, or all flight when the FMC was not
  ///   set up): no phase, so the purser estimates it from the flight itself
  ///   until the FMC moves on (D-20).
  std::optional<Phase> phase(int raw, const SimState& s) const;

 private:
  float takeoffLever_;
  /// Airborne since the aircraft was last parked with the engines off.
  bool flown_ = false;
};

}  // namespace xn::zibo737
