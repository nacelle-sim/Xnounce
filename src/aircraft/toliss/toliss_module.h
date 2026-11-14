/// @file
/// The ToLiss Airbus module: reads the ToLiss datarefs into the snapshot.
///
/// It is read-only toward ToLiss (docs/ARCHITECTURE.md § 4, rule 4): no
/// dataref is written and no ToLiss command is run. Every dataref it reads is
/// listed with its encoding in docs/aircraft/toliss.md.
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "aircraft/module.h"
#include "aircraft/toliss/toliss_options.h"
#include "sim/dataref.h"

namespace xn::toliss {

class Module : public AircraftModule {
 public:
  /// `options` are the user's ToLiss settings. They are read every tick, so a
  /// change in the Settings tab applies at once.
  explicit Module(const Options& options);
  Module(const Module&) = delete;  // `raw_` points into this object
  Module& operator=(const Module&) = delete;

  const char* id() const override { return "toliss"; }
  const char* name() const override { return "ToLiss"; }
  const char* supportedAircraft() const override {
    return "the ToLiss A319, A320 and A321";
  }
  const char* loadHint() const override {
    return "a ToLiss A319, A320 or A321";
  }

  Recognition recognise(const LoadedAircraft& aircraft) override;
  std::vector<std::string> activate(const LoadedAircraft& aircraft) override;
  void deactivate() override;
  void reloadFiles() override;

  CabinGeometry cabinGeometry() const override;
  void read(SimState& s) override;
  AircraftType type(const SimState& s) const override;

  std::vector<ObservedCommand> observedCommands() const override;
  std::vector<std::pair<std::string, std::string>> rawSnapshot() const override;
  std::vector<LiveRow> liveInputs(const SimState& s) const override;

 private:
  /// Which of the three landing-light datarefs say "on": 1, 0, or -1 when
  /// that dataref is missing.
  std::vector<float> landingCandidates() const;
  /// Follows the user's "volume override dataref" setting.
  void useOverrideRef(const std::string& name);

  const Options& options_;
  LoadedAircraft aircraft_;
  /// `eng_type` from the livery's `livery.tlscfg`.
  std::string tlscfgEngine_;
  std::string overrideName_;

  // The last raw values, for the Log tab.
  int ecamPhase_ = -1, qpacPhase_ = -1;
  float throttleMax_ = 0;
  float volInternal_ = -1;
  int volOverride_ = -1;

  DataRef apPhase_{"AirbusFBW/APPhase"};
  DataRef seatbelt_{"AirbusFBW/SeatBeltSignsOn"};
  DataRef noSmoking_{"AirbusFBW/NoSmokingSignsOn"};
  DataRef paxDoors_{"AirbusFBW/PaxDoorArray"};
  DataRef paxDoorModes_{"AirbusFBW/PaxDoorModeArray"};  // trace only
  DataRef slides_{"AirbusFBW/SlideArmedArray"};
  DataRef noPax_{"AirbusFBW/NoPax"};
  DataRef masks_{"AirbusFBW/PaxOxyMask"};
  DataRef cabinAlt_{"AirbusFBW/CabinAlt"};
  DataRef acVolts_{"AirbusFBW/ACBusVoltages"};
  DataRef acpKnob_[3] = {DataRef("AirbusFBW/ACP1KnobPush"),
                         DataRef("AirbusFBW/ACP2KnobPush"),
                         DataRef("AirbusFBW/ACP3KnobPush")};
  DataRef acpVol_[3] = {DataRef("AirbusFBW/ACP1RotaryPositions"),
                        DataRef("AirbusFBW/ACP2RotaryPositions"),
                        DataRef("AirbusFBW/ACP3RotaryPositions")};
  // The A319/A320/A321 volume knobs, 0 to 270 degrees. Where they don't
  // exist the RotaryPositions arrays above give the volume.
  DataRef acpPaKnob_[3] = {DataRef("ckpt/oh/pa/1/anim"),
                           DataRef("ckpt/oh/pa/2/anim"),
                           DataRef("ckpt/oh/pa/3/anim")};
  DataRef acpCabKnob_[3] = {DataRef("ckpt/oh/cab/1/anim"),
                            DataRef("ckpt/oh/cab/2/anim"),
                            DataRef("ckpt/oh/cab/3/anim")};
  DataRef doorAngle_{"AirbusFBW/CockpitDoorAngle"};
  DataRef ckptDoor_{"ckpt/door"};
  DataRef satcomInhibit_{"AirbusFBW/SatComObjInhibit"};
  DataRef flightNo_{"toliss_airbus/init/flight_no"};
  DataRef cruiseAlt_{"toliss_airbus/init/cruise_alt"};
  DataRef parkBrake_{"AirbusFBW/ParkBrake"};
  DataRef chocks_{"AirbusFBW/Chocks"};
  DataRef ohpLights_{"AirbusFBW/OHPLightSwitches"};
  DataRef ecam_{"AirbusFBW/ECAMFlightPhase"};
  DataRef qpac_{"AirbusFBW/QPACFlightPhase"};
  DataRef throttle_{"AirbusFBW/throttle_input"};
  DataRef sdSpoilers_{"AirbusFBW/SDSpoilerArray"};
  DataRef gearLever_{"AirbusFBW/GearLever"};
  DataRef flapLever_{"AirbusFBW/FlapLeverRatio"};   // trace only
  DataRef cargoDoors_{"AirbusFBW/CargoDoorArray"};  // trace only
  // Landing lights: X-Plane's two datarefs and ToLiss's own.
  DataRef landing_{"sim/cockpit2/switches/landing_lights_on"};
  DataRef landingSwitch_{"sim/cockpit2/switches/landing_lights_switch"};
  DataRef leftLand_{"AirbusFBW/LeftLandLightExtended"};
  DataRef rightLand_{"AirbusFBW/RightLandLightExtended"};
  // ToLiss's sound mix.
  DataRef volInternalRef_{"AirbusFBW/fmod/vol/Internal"};
  DataRef volCockpit_{"AirbusFBW/fmod/vol/cockpit"};  // trace only
  DataRef volSpkrs_{"AirbusFBW/fmod/vol/spkrs"};      // trace only
  DataRef doorBlend_{"AirbusFBW/fmod/vol/FwdCockpitDoorBlend"};
  DataRef volOverrideRef_;

  /// Every dataref above, for `activate`.
  std::vector<DataRef*> refs_;
  /// The ToLiss datarefs that go into the trace, in the order they are
  /// written.
  std::vector<const DataRef*> raw_;
};

}  // namespace xn::toliss
