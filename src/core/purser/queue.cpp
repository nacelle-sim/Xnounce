/// @file
/// Playing what is queued, and the interphone call to the cockpit (VP_SPEC
/// § 4.3).
#include <algorithm>

#include "core/log.h"
#include "core/purser/purser.h"
#include "core/purser/purser_internal.h"

namespace xn {

using namespace purser_detail;

void Purser::runCall(const SimState& s, const Settings& set) {
  const Item it = call_.item;
  if (call_.talking) {
    if (skipRequested_) {
      out_.stop();
      finishCurrent(Outcome::Stopped);
      gapUntil_ = s.t + set.chainGap;
    } else if (!out_.playing()) {
      finishCurrent(Outcome::Played);
      gapUntil_ = s.t + set.chainGap;
    }
    return;
  }
  if (expired(it, s, set)) {
    finishCurrent(Outcome::Expired);
    return;
  }
  if (skipRequested_) {
    finishCurrent(Outcome::Stopped, "call cancelled");
    return;
  }
  if (s.interphoneOn || call_.forceAnswer) {
    const auto label = out_.play(it.type, it.ctx, true);
    if (!label) {
      finishCurrent(Outcome::NoSound);
      return;
    }
    call_.talking = true;
    for (auto r = timeline_.rbegin(); r != timeline_.rend(); ++r)
      if (r->type == it.type && r->outcome == Outcome::Queued) {
        r->outcome = Outcome::Playing;
        r->detail = *label + (call_.forceAnswer
                                  ? std::string(" (advanced)")
                                  : " (" + terms().interphoneAnswered + ")");
        if (sink_) sink_(*r);
        break;
      }
    return;
  }
  if (s.t >= call_.nextChime) {
    if (++call_.attempt > set.interphoneRetries) {
      finishCurrent(Outcome::Unanswered,
                    "no answer after " + std::to_string(set.interphoneRetries) +
                        " retries");
      return;
    }
    out_.chime();
    call_.nextChime = s.t + set.interphoneRetry;
  }
}

void Purser::runQueue(const SimState& s, const Settings& set) {
  if (current_) {
    if (call_.active) {
      runCall(s, set);
      skipRequested_ = false;
      return;
    }
    if (skipRequested_) {
      out_.stop();
      finishCurrent(Outcome::Stopped, "skipped");
      gapUntil_ = s.t;
    } else if (!out_.playing()) {
      finishCurrent(Outcome::Played);
      gapUntil_ = s.t + set.chainGap;
    } else if (acLostAt_ >= 0 && s.t - acLostAt_ >= kPowerLossS) {
      // A power loss, not a transfer: the PA went silent with the power and
      // the announcement is dropped. It counts as played (D-13).
      out_.stop();
      finishCurrent(Outcome::PowerLost);
      gapUntil_ = s.t;
    } else {
      return;
    }
  }
  skipRequested_ = false;

  for (auto it = queue_.begin(); it != queue_.end();) {
    if (expired(*it, s, set)) {
      log(s.t, it->type, it->ctx, Outcome::Expired);
      onFinished(*it);
      it = queue_.erase(it);
    } else {
      ++it;
    }
  }
  // D-13: nothing plays without AC power.
  if (!s.acPower || s.replay || s.t < gapUntil_) return;
  // § 4.7: the aircraft's own announcement plays to its end first.
  const bool aircraftPa = s.aircraftPaPlaying == 1;
  if (aircraftPa != aircraftPaHeld_) {
    aircraftPaHeld_ = aircraftPa;
    if (aircraftPa && !queue_.empty())
      note(s.t, "waiting for the aircraft's own announcement");
  }
  if (aircraftPa) return;

  while (true) {
    auto ready = std::find_if(queue_.begin(), queue_.end(), [&](const Item& i) {
      return i.notBefore <= s.t;
    });
    if (ready == queue_.end()) return;
    const Item it = *ready;
    queue_.erase(ready);
    if (!set.eventEnabled(annTypeName(it.type))) {
      log(s.t, it.type, it.ctx, Outcome::Disabled);
      onFinished(it);
      continue;
    }
    const std::optional<Mode> mode = out_.peek(it.type, it.ctx);
    if (!mode) {
      log(s.t, it.type, it.ctx, Outcome::NoSound);
      onFinished(it);
      continue;
    }
    if (*mode == Mode::Interphone) {
      // § 4.3: the buzzer, then wait for the cockpit to answer.
      current_ = it;
      call_ = Call{};
      call_.active = true;
      call_.item = it;
      call_.attempt = 0;
      call_.nextChime = s.t + set.interphoneRetry;
      out_.chime();
      log(s.t, it.type, it.ctx, Outcome::Queued,
          "cabin calling: " + terms().answerInterphone);
      return;
    }
    const auto label = out_.play(it.type, it.ctx, false);
    if (!label) {
      log(s.t, it.type, it.ctx, Outcome::NoSound);
      onFinished(it);
      continue;
    }
    current_ = it;
    log(s.t, it.type, it.ctx, Outcome::Playing, *label);
    return;
  }
}

}  // namespace xn
