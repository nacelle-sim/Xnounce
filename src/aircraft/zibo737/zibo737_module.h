/// @file
/// The Zibo / LevelUp 737 module: reads the 737's datarefs into the snapshot.
///
/// It changes the aircraft only through its cabin actions, when the user
/// switched them on (`zibo737_actions`, docs/ARCHITECTURE.md § 4, rule 4).
/// Every dataref it reads is listed with its encoding in
/// docs/aircraft/zibo737.md.
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "aircraft/module.h"
#include "aircraft/zibo737/zibo737_actions.h"
#include "aircraft/zibo737/zibo737_options.h"
#include "aircraft/zibo737/zibo737_signals.h"
#include "aircraft/zibo737/zibo737_types.h"
#include "sim/dataref.h"

namespace xn::zibo737 {

class Module : public AircraftModule {
 public:
  /// `options` are the user's 737 settings, read every tick.
  explicit Module(const Options& options);
  Module(const Module&) = delete;  // `refs_` points into this object
  Module& operator=(const Module&) = delete;

  const char* id() const override { return "zibo737"; }
  /// The maker of the loaded aircraft ("LevelUp", "Zibo"), so the Status card
  /// reads "LevelUp 737-800"; "Zibo / LevelUp" while none is loaded.
  const char* name() const override { return name_; }
  const char* supportedAircraft() const override {
    return "the Zibo 737-800 and the LevelUp 737-600 to -900ER";
  }
  const char* loadHint() const override { return "a Zibo or LevelUp 737"; }

  Recognition recognise(const LoadedAircraft& aircraft) override;
  std::vector<std::string> activate(const LoadedAircraft& aircraft) override;
  void deactivate() override;

  CabinActions* actions() override { return &actions_; }
  /// X-Plane's own cabin chime, which the 737 uses as well.
  std::string chimeSound() const override;
  CabinGeometry cabinGeometry() const override {
    return cabinOf(model_, options_);
  }
  void read(SimState& s) override;
  AircraftType type(const SimState& s) const override;

  std::vector<ObservedCommand> observedCommands() const override;
  std::vector<std::pair<std::string, std::string>> rawSnapshot() const override;
  std::vector<LiveRow> liveInputs(const SimState& s) const override;

 private:
  const Options& options_;
  LoadedAircraft aircraft_;
  Model model_ = Model::Unknown;
  const char* name_ = "Zibo / LevelUp";
  Signals signals_;
  Raw raw_;  ///< The last tick's values, for the Log tab.
  Actions actions_{options_};

  DataRef flightPhase_{"laminar/B738/FMS/flight_phase"};
  DataRef airGround_{"laminar/B738/air_ground_sensor"};
  DataRef apTakeoff_{"laminar/B738/autopilot/ap_takeoff"};
  DataRef n1Mode_{"laminar/B738/FMS/N1_mode"};  // trace only
  DataRef throttle_{"sim/cockpit2/engine/actuators/throttle_ratio"};
  DataRef landed_{"laminar/b738/fmodpack/flightphase_landed"};
  DataRef seatbeltLight_{"laminar/b738/fmodpack/seatbelt_on_light"};
  DataRef seatbeltSwitch_{"laminar/B738/toggle_switch/seatbelt_sign_pos"};
  DataRef noSmokingSwitch_{"laminar/B738/toggle_switch/no_smoking_pos"};
  DataRef gearHandle_{"laminar/B738/controls/gear_handle_down"};
  DataRef landRetL_{"laminar/B738/switch/land_lights_ret_left_pos"};
  DataRef landRetR_{"laminar/B738/switch/land_lights_ret_right_pos"};
  DataRef landFixL_{"laminar/B738/switch/land_lights_left_pos"};
  DataRef landFixR_{"laminar/B738/switch/land_lights_right_pos"};
  DataRef positionLight_{"laminar/B738/toggle_switch/position_light_pos"};
  DataRef parkBrake_{"laminar/B738/parking_brake_pos"};
  DataRef chocks_{"laminar/B738/fms/chock_status"};
  DataRef doors_[6] = {
      DataRef("737u/doors/L1"),    DataRef("737u/doors/R1"),
      DataRef("737u/doors/L2"),    DataRef("737u/doors/R2"),
      DataRef("737u/doors/L_mid"), DataRef("737u/doors/R_mid")};
  DataRef zones_[5] = {DataRef("laminar/B738/tab/zone1_payload"),
                       DataRef("laminar/B738/tab/zone2_payload"),
                       DataRef("laminar/B738/tab/zone3_payload"),
                       DataRef("laminar/B738/tab/zone4_payload"),
                       DataRef("laminar/B738/tab/zone5_payload")};
  DataRef seats_{"laminar/B738/std_pax_num"};
  DataRef bus1_{"laminar/B738/electric/ac_tnsbus1_status"};
  DataRef bus2_{"laminar/B738/electric/ac_tnsbus2_status"};
  DataRef genOff1_{"laminar/B738/annunciator/gen_off_bus1"};
  DataRef genOff2_{"laminar/B738/annunciator/gen_off_bus2"};
  DataRef apuBus1_{"laminar/B738/electrical/apu_power_bus1"};
  DataRef apuBus2_{"laminar/B738/electrical/apu_power_bus2"};
  DataRef paxOxy_{"laminar/B738/annunciator/pax_oxy"};
  DataRef o2Failure_{"sim/operation/failures/rel_pass_o2_on"};
  DataRef cabinAlt_{"sim/cockpit2/pressurization/indicators/cabin_altitude_ft"};
  DataRef cockpitDoor_{"laminar/B738/door/flt_dk_door_ratio"};
  DataRef cruiseAlt_{"laminar/B738/autopilot/fmc_cruise_alt"};
  DataRef micCab_[2] = {DataRef("laminar/B738/audio/capt/mic_button5"),
                        DataRef("laminar/B738/audio/fo/mic_button5")};
  DataRef micPa_[2] = {DataRef("laminar/B738/audio/capt/mic_button6"),
                       DataRef("laminar/B738/audio/fo/mic_button6")};
  // The aircraft's own turnaround and announcements (docs/aircraft/zibo737.md
  // § 4, § 5). `msg_playing` holds Xnounce's next announcement.
  DataRef autoFlight_{"laminar/B738/tab/auto_flight"};
  /// The tablet's NO SMOKING option: 0 the switch, 1 chime only, 2 none.
  DataRef noSmokingOption_{"laminar/B738/fms/no_smoking"};
  AirsideFollow airside_;
  DataRef msgPlaying_{"laminar/b738/fmodpack/msg_playing"};
  DataRef cabinReady_{"laminar/B738/system/cabin_ready"};
  DataRef paxLighting_{"737x/cabin/set_pax_lighting"};
  DataRef callLight_{"laminar/B738/annunciator/cabin_call_annun"};

  /// Every dataref above, for `activate` and the trace.
  std::vector<DataRef*> refs_;
};

}  // namespace xn::zibo737
