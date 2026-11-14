/// @file
/// Implements `src/audio/fmod_events.h`; the interface is documented there.
#include "audio/fmod_events.h"

#include <algorithm>
#include <utility>

namespace xn {

namespace {

/// How often events the banks don't have are looked up again (s).
constexpr double kLookupEvery = 5;
/// The most instances of one event looked at per frame.
constexpr int kMaxInstances = 16;
/// Consecutive FMOD errors (other than a void handle) before giving up for
/// the session.
constexpr int kMaxErrors = 20;
/// FMOD_STUDIO_PLAYBACK_STATE: playing, sustaining, starting.
bool sounding(int state) { return state == 0 || state == 1 || state == 3; }

}  // namespace

FmodEvents::FmodEvents(FmodApi api, std::vector<std::string> muted,
                       std::vector<std::string> watched)
    : api_(api) {
  for (std::string& p : muted) {
    Event e;
    e.path = std::move(p);
    e.mute = true;
    events_.push_back(std::move(e));
  }
  for (std::string& p : watched) {
    Event e;
    e.path = std::move(p);
    events_.push_back(std::move(e));
  }
}

int FmodEvents::mutedFound() const {
  return static_cast<int>(
      std::count_if(events_.begin(), events_.end(),
                    [](const Event& e) { return e.mute && e.description; }));
}

std::string FmodEvents::takeNote() {
  std::string n;
  std::swap(n, note_);
  return n;
}

bool FmodEvents::check(int result, Event& e, const char* what) {
  if (result == kOk) {
    errors_ = 0;
    return true;
  }
  if (result == kInvalidHandle) {
    // The bank was unloaded or rebuilt: look the event up again later.
    e.description = nullptr;
    e.silenced.clear();
    return false;
  }
  if (++errors_ >= kMaxErrors) {
    failed_ = true;
    why_ = std::string("FMOD error ") + std::to_string(result) + " in " + what;
    note_ = "aircraft sounds: " + why_ + ", no longer touched";
  }
  return false;
}

void FmodEvents::lookUp(double t) {
  if (t < nextLookup_) return;
  nextLookup_ = t + kLookupEvery;
  const int before = mutedFound();
  for (Event& e : events_) {
    if (e.description) continue;
    void* d = nullptr;
    const int r = api_.getEvent(api_.system, e.path.c_str(), &d);
    if (r == kOk && d)
      e.description = d;
    else if (r != kEventNotFound && r != kOk)
      check(r, e, "getEvent");
  }
  const int found = mutedFound();
  if (found != before)
    note_ =
        "aircraft sounds: " + std::to_string(found) + " of " +
        std::to_string(std::count_if(events_.begin(), events_.end(),
                                     [](const Event& e) { return e.mute; })) +
        " cabin crew announcements found";
}

void FmodEvents::unmute(Event& e) {
  for (void* i : e.silenced) api_.setVolume(i, 1.0f);
  e.silenced.clear();
}

void FmodEvents::tick(double t, bool mute) {
  speaking_ = false;
  if (!ready()) return;
  lookUp(t);
  void* list[kMaxInstances];
  for (Event& e : events_) {
    if (!e.description) continue;
    int count = 0;
    if (!check(api_.getInstanceList(e.description, list, kMaxInstances, &count),
               e, "getInstanceList"))
      continue;
    count = std::clamp(count, 0, kMaxInstances);
    const bool silence = e.mute && mute;
    if (!silence && !e.silenced.empty()) unmute(e);
    std::vector<void*> kept;
    for (int k = 0; k < count; ++k) {
      void* inst = list[k];
      if (silence) {
        if (check(api_.setVolume(inst, 0.0f), e, "setVolume"))
          kept.push_back(inst);
        continue;
      }
      int state = 2;
      if (check(api_.getPlaybackState(inst, &state), e, "getPlaybackState") &&
          sounding(state))
        speaking_ = true;
    }
    if (silence) e.silenced = std::move(kept);
    if (failed_) return;
  }
}

void FmodEvents::restore() {
  if (!api_.complete()) return;
  for (Event& e : events_) unmute(e);
}

void FmodEvents::forget() {
  for (Event& e : events_) {
    e.description = nullptr;
    e.silenced.clear();
  }
  nextLookup_ = -1;
  speaking_ = false;
}

}  // namespace xn
