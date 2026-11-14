/// @file
/// The cabin crew's hands in one aircraft (VP_SPEC § 4.7).
///
/// An aircraft module that can act on its aircraft (open a door, set the
/// cabin lights) returns a `CabinActions` from `AircraftModule::actions()`.
/// It is the only way the plugin changes the aircraft (docs/ARCHITECTURE.md
/// § 4, rule 4): it acts only on what the user switched on, from the main
/// flight loop, one step per item and tick, and puts back the aircraft
/// options it changed.
///
/// This header needs no X-Plane SDK. An implementation does.
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "core/cabin_crew/cabin_crew.h"
#include "core/sim_state.h"

namespace xn {

/// One action as the window shows it: its name, whether the user switched it
/// on, and whether the aircraft allows it.
struct ActionStatus {
  std::string name;  ///< "Doors", "Cabin lights"...
  bool on = false;   ///< The user's setting.
  bool available = false;
  /// Why it isn't available ("LevelUp's sounds have no cabin chime"), or
  /// what it is doing now.
  std::string note;
};

/// One row of the Log tab's Aircraft check: something the module can try,
/// with the aircraft's reading and the result of the last try.
struct CheckRow {
  std::string label;    ///< "Close door L1"
  std::string reading;  ///< "L1 0.00"
  std::string result;   ///< "before 1.00, after 0.00", or empty.
  bool available = true;
};

class CabinActions {
 public:
  virtual ~CabinActions() = default;

  /// Receives one line per action and result, for the log and the trace
  /// ("close door L1", "door L1 closed", "door R1 didn't close").
  void setEventSink(std::function<void(const std::string&)> sink) {
    sink_ = std::move(sink);
  }

  /// The actions of this aircraft and their state, for the window.
  virtual std::vector<ActionStatus> status() const = 0;
  /// The user switched at least one action on.
  virtual bool anyOn() const = 0;
  /// What the window says about the aircraft's own announcements (silenced,
  /// or why they can't be), or empty.
  virtual std::string announcementNote() const = 0;

  /// One tick of the main flight loop: carry out the parts of `plan` the user
  /// switched on.
  virtual void tick(const CabinPlan& plan, const SimState& s) = 0;
  /// A new flight: forget what was done, so the plan applies afresh.
  virtual void newFlight() = 0;
  /// Every frame, from the audio flight loop on the main thread, at sim time
  /// `t`: silencing the aircraft's own cabin announcements must catch each
  /// one as it starts (VP_SPEC § 4.7).
  virtual void everyFrame(double t) { (void)t; }
  /// X-Plane is rebuilding its sound banks: sound handles are void.
  virtual void soundBanksUnloading() {}
  /// The aircraft is playing a cabin announcement of its own that wasn't
  /// silenced (VP_SPEC § 4.7).
  virtual bool aircraftSpeaking() const { return false; }

  /// True when the aircraft sounds the cabin's call to the cockpit itself
  /// (the action is on and the aircraft allows it). The player then doesn't
  /// play its chime, and the app calls `chime()`.
  virtual bool ownChime() const = 0;
  virtual void chime() = 0;

  /// The plugin stops acting: puts back the aircraft options it changed,
  /// unless the aircraft is gone (unloading or reloading), whose handles are
  /// stale.
  virtual void release(bool aircraftGone) = 0;

  /// The Aircraft check (Log tab).
  virtual std::vector<CheckRow> checks() const = 0;
  /// Queues the check `row`; the next tick carries it out.
  virtual void tryCheck(int row) = 0;

 protected:
  void emit(const std::string& line) const {
    if (sink_) sink_(line);
  }

 private:
  std::function<void(const std::string&)> sink_;
};

}  // namespace xn
