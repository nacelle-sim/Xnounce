// Silencing the aircraft's own FMOD sound events with a pretend FMOD Studio
// (VP_SPEC § 4.7, D-32).
#include <doctest/doctest.h>

#include <map>
#include <string>
#include <vector>

#include "audio/fmod_events.h"

using namespace xn;

namespace {

/// A pretend FMOD Studio: events by path, each with its instances, their
/// volumes and playback states.
struct Fake {
  struct Instance {
    float volume = 1;
    int state = 0;  // playing
  };
  std::map<std::string, std::vector<Instance>> events;
  std::map<std::string, bool> loaded;  // the bank has the event
  int failWith = 0;                    // every call returns this when set
};
Fake* gFake = nullptr;

int getEvent(void*, const char* path, void** d) {
  if (gFake->failWith) return gFake->failWith;
  auto it = gFake->events.find(path);
  if (it == gFake->events.end() || !gFake->loaded[path])
    return FmodEvents::kEventNotFound;
  *d = &it->second;
  return 0;
}
int getInstanceList(void* d, void** arr, int cap, int* count) {
  if (gFake->failWith) return gFake->failWith;
  auto* list = static_cast<std::vector<Fake::Instance>*>(d);
  *count = 0;
  for (auto& i : *list)
    if (*count < cap) arr[(*count)++] = &i;
  return 0;
}
int setVolume(void* i, float v) {
  static_cast<Fake::Instance*>(i)->volume = v;
  return 0;
}
int getPlaybackState(void* i, int* s) {
  *s = static_cast<Fake::Instance*>(i)->state;
  return 0;
}

FmodApi api() {
  static int system = 0;
  FmodApi a;
  a.system = &system;
  a.getEvent = getEvent;
  a.getInstanceList = getInstanceList;
  a.setVolume = setVolume;
  a.getPlaybackState = getPlaybackState;
  return a;
}

const char* kDemo = "event:/crew/AT_safety_demo";
const char* kCaptain = "event:/crew/CPT_cruise";

}  // namespace

TEST_CASE("aircraft sounds: crew events muted, the captain's watched") {
  Fake fake;
  gFake = &fake;
  fake.events[kDemo] = {};
  fake.events[kCaptain] = {};
  FmodEvents ev(api(), {kDemo}, {kCaptain});
  REQUIRE(ev.ready());
  // The bank isn't loaded yet: nothing found, looked up again later.
  ev.tick(0, true);
  CHECK(ev.mutedFound() == 0);
  fake.loaded[kDemo] = fake.loaded[kCaptain] = true;
  ev.tick(1, true);
  CHECK(ev.mutedFound() == 0);  // not before 5 s
  ev.tick(5.1, true);
  CHECK(ev.mutedFound() == 1);
  CHECK(ev.takeNote().find("1 of 1") != std::string::npos);
  // The safety demo starts: silenced on the next frame, and it doesn't count
  // as the aircraft speaking.
  fake.events[kDemo].push_back({});
  ev.tick(5.2, true);
  CHECK(fake.events[kDemo][0].volume == 0.0f);
  CHECK(!ev.aircraftSpeaking());
  // The captain speaks: played, and Xnounce waits.
  fake.events[kCaptain].push_back({});
  ev.tick(5.3, true);
  CHECK(fake.events[kCaptain][0].volume == 1.0f);
  CHECK(ev.aircraftSpeaking());
  fake.events[kCaptain][0].state = 2;  // stopped
  ev.tick(5.4, true);
  CHECK(!ev.aircraftSpeaking());
  // Switched off: the demo gets its volume back and counts as speaking.
  ev.tick(5.5, false);
  CHECK(fake.events[kDemo][0].volume == 1.0f);
  CHECK(ev.aircraftSpeaking());
  // Back on, then restored when the plugin lets go.
  ev.tick(5.6, true);
  CHECK(fake.events[kDemo][0].volume == 0.0f);
  ev.restore();
  CHECK(fake.events[kDemo][0].volume == 1.0f);
}

TEST_CASE("aircraft sounds: void handles and lasting errors") {
  Fake fake;
  gFake = &fake;
  fake.events[kDemo] = {Fake::Instance{}};
  fake.loaded[kDemo] = true;
  FmodEvents ev(api(), {kDemo}, {});
  ev.tick(0, true);
  CHECK(ev.mutedFound() == 1);
  // The banks are rebuilt: the handle is void and looked up again.
  fake.failWith = FmodEvents::kInvalidHandle;
  ev.tick(0.1, true);
  CHECK(ev.mutedFound() == 0);
  CHECK(ev.ready());
  fake.failWith = 0;
  ev.tick(6, true);
  CHECK(ev.mutedFound() == 1);
  // Any other error, again and again: given up for the session, once.
  fake.failWith = 19;
  for (double t = 7; t < 9 && ev.ready(); t += 0.05) ev.tick(t, true);
  CHECK(!ev.ready());
  CHECK(ev.why().find("FMOD error 19") != std::string::npos);
  CHECK(!ev.takeNote().empty());
  // An incomplete table is never ready.
  FmodApi none;
  FmodEvents off(none, {kDemo}, {});
  CHECK(!off.ready());
  off.tick(0, true);
  CHECK(!off.aircraftSpeaking());
}
