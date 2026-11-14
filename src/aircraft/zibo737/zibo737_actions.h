/// @file
/// The Zibo / LevelUp 737's cabin actions (VP_SPEC § 4.7): the doors, the
/// airstairs, the cabin lights, the Zibo's chime, and silencing the
/// aircraft's own cabin crew announcements (on the LevelUp through X-Plane's
/// FMOD Studio, on the Zibo through its crew volume).
///
/// The decisions are in `zibo737_crew` (pure); this class reads the 737's
/// datarefs, runs the commands and writes the options, on the main thread
/// from the main flight loop only (docs/ARCHITECTURE.md § 4, rule 4). The
/// datarefs and commands are listed in docs/aircraft/zibo737.md § 4 and § 6.
#pragma once

#include <XPLMUtilities.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "aircraft/actions.h"
#include "aircraft/zibo737/zibo737_crew.h"
#include "aircraft/zibo737/zibo737_options.h"
#include "audio/fmod_events.h"
#include "core/cabin_crew/option_hold.h"
#include "sim/dataref.h"

namespace xn::zibo737 {

class Actions : public CabinActions {
 public:
  explicit Actions(const Options& options);
  Actions(const Actions&) = delete;  // the check list points into this object
  Actions& operator=(const Actions&) = delete;

  /// The aircraft is active: find the datarefs and commands and work out
  /// which actions it allows. `zibo` is true on the Zibo, whose sound set has
  /// a crew volume and a chime a plugin can use.
  void activate(bool zibo);
  /// The aircraft is gone: forget every handle. `release` came first.
  void deactivate();

  std::vector<ActionStatus> status() const override;
  bool anyOn() const override;
  std::string announcementNote() const override;
  void tick(const CabinPlan& plan, const SimState& s) override;
  void newFlight() override;
  void everyFrame(double t) override;
  void soundBanksUnloading() override;
  bool aircraftSpeaking() const override;
  bool ownChime() const override;
  void chime() override;
  void release(bool aircraftGone) override;
  std::vector<CheckRow> checks() const override;
  void tryCheck(int row) override;

 private:
  /// One option of the aircraft's own announcements, held at 0 while its
  /// crew announcements are silenced.
  struct Held {
    const char* what;  ///< "captain's messages"
    DataRef ref;
    OptionHold hold{0};
    bool zibOnly = false;
  };
  /// One row of the Aircraft check.
  struct Check {
    enum Kind { DoorRow, Stairs, Light, Chime, Option } kind;
    int index = 0;  ///< The door, the light setting or the held option.
  };
  struct Tried {
    int row = -1;
    bool live = false;  ///< Still waiting for the item to settle.
    std::string before;
    double at = 0, settle = 0;
    std::string result;
  };

  bool doorsOn() const { return options_.actDoors && doorsOk_; }
  bool lightsOn() const { return options_.actLights && lightsOk_; }
  bool chimeOn() const { return options_.actChime && chimeOk_; }
  bool silenceOn() const { return active_ && options_.silenceAircraftPa; }
  CrewReadings readings() const;
  /// Runs an aircraft command once. False when the aircraft hasn't got it.
  bool run(const std::string& name);
  bool has(const std::string& name) const;
  std::string reading(const Check& c) const;
  void runCheck(int row, double t);

  const Options& options_;
  bool active_ = false;
  bool zibo_ = false;
  Crew crew_;
  bool doorsOk_ = false, lightsOk_ = false, chimeOk_ = false;
  std::string doorsWhy_, lightsWhy_, chimeWhy_;

  DataRef door_[kDoorCount];
  DataRef stairsHidden_{"laminar/B738/airstairs_hide"};
  DataRef stairsRatio_{"laminar/B738/door/airstair_ratio"};
  DataRef paxLighting_{"737x/cabin/set_pax_lighting"};
  DataRef autoFlight_{"laminar/B738/tab/auto_flight"};
  DataRef cargoLoading_{"laminar/b738/fmodpack/fmod_play_cargo"};
  DataRef callChime_{"laminar/b738/fmodpack/call_chime"};
  Held held_[1];
  /// The LevelUp's cabin crew events, silenced through FMOD; null on the
  /// Zibo or when X-Plane's FMOD Studio can't be reached (`fmodWhy_`).
  std::unique_ptr<FmodEvents> fmod_;
  std::string fmodWhy_;
  bool silencing_ = false;  ///< What the log last said.

  std::map<std::string, XPLMCommandRef> commands_;
  std::vector<Check> checkList_;
  int pendingCheck_ = -1;
  Tried tried_;
  double now_ = 0;
};

}  // namespace xn::zibo737
