/// @file
/// Follows one item the cabin crew sets in the aircraft, such as a door or the
/// cabin lights (docs/ARCHITECTURE.md § 4, rule 4).
#pragma once

#include <limits>

namespace xn {

/// Decides when to act on one item of the aircraft and whether it worked.
///
/// Every tick the owner says what the item should read (`want`), what the
/// aircraft reads now and whether the item has stopped moving. The tracker
/// answers whether to act now. It acts only on a settled item that differs
/// from the wish, then waits up to `settleS` for the reading to follow. It
/// gives up after `maxActs` acts for one wish, and it leaves the item to the
/// user once the reading moves away from a wish that was reached. A new wish
/// starts afresh, which is how "until the next stage of the flight" works.
///
/// Pure: no X-Plane, so the aircraft modules' decisions can be tested.
class ActionTracker {
 public:
  /// "No wish": the crew leaves the item alone.
  static constexpr int kNone = std::numeric_limits<int>::min();

  enum class State {
    Idle,    ///< No wish, or waiting for the item to settle before acting.
    Acting,  ///< Acted; waiting for the reading to follow.
    Done,    ///< The reading matches the wish.
    Failed,  ///< The acts didn't take; nothing more until the wish changes.
    User,    ///< Someone changed the item after the crew set it.
  };
  /// What happened on this tick, for the log and the trace.
  enum class Event { None, Act, Reached, Failed, UserTook };

  /// @param settleS How long the aircraft may take to follow one act.
  /// @param maxActs Acts allowed for one wish. A control that steps through
  ///        its settings (the 737's night/sleep button) needs more than one.
  explicit ActionTracker(double settleS = 10, int maxActs = 1)
      : settleS_(settleS), maxActs_(maxActs) {}

  /// One tick. `reading` is what the aircraft says now; `settled` is false
  /// while the item moves (a door half open) or its reading is unknown.
  /// Returns `Event::Act` when the owner should act now.
  Event update(int want, int reading, bool settled, double t);

  State state() const { return state_; }
  int want() const { return want_; }
  /// How many acts the current wish took so far.
  int acts() const { return acts_; }
  /// Forgets the wish: a new flight, or the action was switched off.
  void reset();

 private:
  double settleS_;
  int maxActs_;
  int want_ = kNone;
  State state_ = State::Idle;
  int acts_ = 0;
  double actedAt_ = 0;
  int readingAtAct_ = kNone;
};

/// The state in words: "idle", "acting", "done", "failed", "left to the user".
const char* actionStateName(ActionTracker::State s);

}  // namespace xn
