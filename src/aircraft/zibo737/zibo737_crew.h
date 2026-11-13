/// @file
/// What the cabin crew does on the Zibo / LevelUp 737: which command for which
/// door and light setting, and when (VP_SPEC § 4.7, docs/aircraft/zibo737.md
/// § 6).
///
/// Pure code: it takes the 737's readings and returns the commands to run, so
/// the scripted 737 flights of the tests run the same decisions as the live
/// module (`zibo737_actions`), which only reads the datarefs and runs the
/// commands.
#pragma once

#include <array>
#include <string>
#include <vector>

#include "core/cabin_crew/action_tracker.h"
#include "core/cabin_crew/cabin_crew.h"
#include "core/sim_state.h"

namespace xn::zibo737 {

/// The doors the crew works, in the order they are logged.
enum class Door { L1, R1, L2, R2, LMid, RMid, FwdCargo, AftCargo, Count };
constexpr int kDoorCount = static_cast<int>(Door::Count);

/// One door: its name, its position dataref, its commands, how long it takes.
struct DoorInfo {
  const char* name;      ///< "L1", "forward cargo"
  const char* position;  ///< `737u/doors/L1`: 0 closed to 1 open.
  const char* open;      ///< The explicit open command.
  const char* close;     ///< The explicit close command.
  double settleS;        ///< Time allowed to move, with a margin.
  bool cargo;
};
const DoorInfo& doorInfo(Door d);

/// `laminar/B738/airstairs_toggle`, stowing or deploying the built-in stairs.
extern const char* const kAirstairsToggle;

/// The cabin light settings, as `737x/cabin/set_pax_lighting` reads them.
enum PaxLight : int {
  kWhiteBright = 0,
  kTakeoffLanding = 1,
  kCruise = 2,
  kWhiteMedium = 3,
  kNight = 4,
  kSleep = 5,
};
/// The command that selects `light` (night and sleep share one).
const char* paxLightCommand(int light);
/// "white bright", "takeoff and landing", "cruise", "white medium", "night",
/// "sleep".
const char* paxLightName(int light);

/// The night cruise preset of the settings: 0 Cruise, 1 Night, 2 Sleep.
int nightPresetLight(int preset);

/// The 737's readings the crew needs. -1 (or a negative ratio) is "missing".
struct CrewReadings {
  /// Door positions in `Door` order, 0 closed to 1 open.
  std::array<float, kDoorCount> door{};
  /// `laminar/B738/airstairs_hide`: 1 stowed or not fitted, 0 out.
  int airstairsHidden = -1;
  /// `laminar/B738/door/airstair_ratio`: 0 stowed to 1 out.
  float airstairRatio = -1;
  /// `737x/cabin/set_pax_lighting` (`PaxLight`).
  int paxLighting = -1;
  /// `laminar/B738/tab/auto_flight`: the aircraft's Airside Services, 0 idle.
  int autoFlight = -1;
  /// `laminar/b738/fmodpack/fmod_play_cargo`: the cargo-loading sound, which
  /// keeps the cargo doors open while Airside Services is off.
  int cargoLoading = -1;
  CrewReadings() { door.fill(-1); }
};

/// The 737 crew's decisions.
class Crew {
 public:
  /// Which actions the user switched on.
  struct Switches {
    bool doors = false;
    bool lights = false;
    int nightPreset = 1;  ///< 0 Cruise, 1 Night, 2 Sleep.
  };
  /// What to do on this tick.
  struct Step {
    std::vector<std::string> commands;  ///< Run in this order.
    std::vector<std::string> events;    ///< For the log and the trace.
  };

  Crew();
  /// One tick: the plan, the readings, the snapshot, the switches.
  Step tick(const CabinPlan& plan, const CrewReadings& r, const SimState& s,
            const Switches& sw);
  /// A new flight, or an action switched off: forget every wish.
  void reset();

  /// The doors and lights in words, for the window: "closing", "left to
  /// Airside Services"...
  std::string doorsNote() const { return doorsNote_; }
  std::string lightsNote() const { return lightsNote_; }

 private:
  void doors(const CabinPlan& plan, const CrewReadings& r, const SimState& s,
             bool on, Step& out);
  void lights(const CabinPlan& plan, const CrewReadings& r, const SimState& s,
              const Switches& sw, Step& out);

  std::array<ActionTracker, kDoorCount> door_;
  ActionTracker stairs_;
  ActionTracker light_;
  /// This cabin has no sleep setting (the -900): Night instead.
  bool noSleep_ = false;
  std::string doorsNote_, lightsNote_;
  /// The last note logged, so a reason is logged once.
  std::string doorsWhy_;
};

}  // namespace xn::zibo737
