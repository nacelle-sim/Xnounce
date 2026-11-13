/// @file
/// Implements `src/core/cabin_crew/action_tracker.h`; the interface is
/// documented there.
#include "core/cabin_crew/action_tracker.h"

namespace xn {

ActionTracker::Event ActionTracker::update(int want, int reading, bool settled,
                                           double t) {
  if (want != want_) {
    want_ = want;
    state_ = State::Idle;
    acts_ = 0;
  }
  if (want_ == kNone) return Event::None;
  switch (state_) {
    case State::Idle:
      if (!settled) return Event::None;
      if (reading == want_) {
        state_ = State::Done;
        return acts_ > 0 ? Event::Reached : Event::None;
      }
      if (acts_ >= maxActs_) {
        state_ = State::Failed;
        return Event::Failed;
      }
      ++acts_;
      actedAt_ = t;
      readingAtAct_ = reading;
      state_ = State::Acting;
      return Event::Act;
    case State::Acting:
      if (settled && reading == want_) {
        state_ = State::Done;
        return Event::Reached;
      }
      // The item moved but not far enough (a stepping control): the next
      // act may follow once it has settled.
      if (settled && reading != readingAtAct_ && acts_ < maxActs_) {
        state_ = State::Idle;
        return Event::None;
      }
      if (t - actedAt_ >= settleS_) {
        if (acts_ < maxActs_) {
          state_ = State::Idle;
          return Event::None;
        }
        state_ = State::Failed;
        return Event::Failed;
      }
      return Event::None;
    case State::Done:
      if (settled && reading != want_) {
        state_ = State::User;
        return Event::UserTook;
      }
      return Event::None;
    case State::Failed:
    case State::User:
      return Event::None;
  }
  return Event::None;
}

void ActionTracker::reset() {
  want_ = kNone;
  state_ = State::Idle;
  acts_ = 0;
  readingAtAct_ = kNone;
}

const char* actionStateName(ActionTracker::State s) {
  switch (s) {
    case ActionTracker::State::Idle:
      return "idle";
    case ActionTracker::State::Acting:
      return "acting";
    case ActionTracker::State::Done:
      return "done";
    case ActionTracker::State::Failed:
      return "failed";
    case ActionTracker::State::User:
      return "left to the user";
  }
  return "?";
}

}  // namespace xn
