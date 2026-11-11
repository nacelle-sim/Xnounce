/// @file
/// How loud each sound is where the camera is (VP_SPEC § 4.5): pure
/// functions of the settings and the snapshot.
#pragma once

#include "core/settings.h"
#include "core/sim_state.h"

namespace xn {

enum class SoundKind { Pa, Music, Interphone, Chime };

/// The cabin as heard at the camera, 0 to 1: the clear voice (open door, PA
/// listened to, camera in the cabin) and the muffled voice through the closed
/// door.
struct Hearing {
  float clear = 0;
  float muffled = 0;
};

/// How far the cockpit door is open: the aircraft's own door sound blend, or
/// the door's opening.
float doorOpenFraction(const Settings& set, const SimState& s);
/// The aircraft's own interior volume when it asks other sounds to follow
/// it, else 1.
float aircraftVolume(const Settings& set, const SimState& s);
/// The two voices of the cabin. With muffling off, the muffled voice is 0 and
/// the clear one never below the closed-door level.
Hearing cabinHearing(const Settings& set, const SimState& s);
/// The gain of a sound (of the clear voice for PA and music); 0 when muted.
float soundGain(SoundKind kind, const Settings& set, const SimState& s,
                bool muted);
/// The gain of the muffled twin of PA and music; 0 for other sounds.
float muffledGain(SoundKind kind, const Settings& set, const SimState& s,
                  bool muted);

}  // namespace xn
