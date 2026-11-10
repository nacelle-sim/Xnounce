// how loud stuff is in the cockpit 4.5 pure function of settings snapshot
#pragma once

#include "core/config.h"
#include "core/sim_state.h"

namespace xn {

enum class SoundKind { Pa, Music, Interphone, Chime };

// cabin at the camera 0 to 1 clear open door pa knob in the cabin and muffled
// thru the closed door
struct Hearing {
  float clear = 0;
  float muffled = 0;
};

// the aircrafts own door sound blend or the door angle
float doorOpenFraction(const Settings& set, const SimState& s);
// the aircrafts own interior volume when it asks us to follow it else 1
float aircraftVolume(const Settings& set, const SimState& s);
// muffling off muffled 0 and clear never under the floor
Hearing cabinHearing(const Settings& set, const SimState& s);
// 0 when muted pa/music the clear voice
float soundGain(SoundKind kind, const Settings& set, const SimState& s,
                bool muted);
// muffled twin gain 0 for anything else
float muffledGain(SoundKind kind, const Settings& set, const SimState& s,
                  bool muted);

}  // namespace xn
