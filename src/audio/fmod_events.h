/// @file
/// Silencing and watching an aircraft's own FMOD sound events (VP_SPEC § 4.7,
/// D-32): the LevelUp 737 plays its cabin crew announcements as FMOD Studio
/// events that no dataref can switch off. X-Plane shares its FMOD Studio
/// system with plugins (`XPLMGetFMODStudio`); this class mutes every instance
/// of the listed events through it and reports whether a watched event plays.
///
/// Pure C++: the FMOD functions come in a table (`FmodApi`) that
/// `src/sim/fmod_studio.cpp` fills at runtime, so no FMOD header or library
/// is part of the build, and tests can pass their own.
#pragma once

#include <string>
#include <vector>

namespace xn {

/// The few FMOD Studio C functions Xnounce calls, with opaque handles. The
/// return value is FMOD's result code: 0 is OK.
struct FmodApi {
  void* system = nullptr;
  int (*getEvent)(void* system, const char* path, void** description) = nullptr;
  int (*getInstanceList)(void* description, void** instances, int capacity,
                         int* count) = nullptr;
  int (*setVolume)(void* instance, float volume) = nullptr;
  int (*getPlaybackState)(void* instance, int* state) = nullptr;

  bool complete() const {
    return system && getEvent && getInstanceList && setVolume &&
           getPlaybackState;
  }
};

class FmodEvents {
 public:
  /// FMOD's result codes Xnounce tells apart.
  static constexpr int kOk = 0;
  static constexpr int kInvalidHandle = 30;
  static constexpr int kEventNotFound = 74;

  /// @param muted Event paths silenced while `tick` is asked to mute.
  /// @param watched Event paths that only count as the aircraft speaking.
  FmodEvents(FmodApi api, std::vector<std::string> muted,
             std::vector<std::string> watched);

  /// Once per frame at time `t` (seconds). Looks up the events the loaded
  /// banks don't have yet every few seconds, mutes every instance of the
  /// muted events when `mute` is true (and gives back the ones it muted when
  /// it turns false), and notes which events play.
  void tick(double t, bool mute);
  /// A watched event, or an unmuted listed one, is playing: the aircraft is
  /// speaking.
  bool aircraftSpeaking() const { return speaking_; }
  /// Gives every instance it muted its volume back.
  void restore();
  /// The sound banks are being rebuilt: every handle is void.
  void forget();

  /// Usable: the table is complete and FMOD hasn't failed.
  bool ready() const { return api_.complete() && !failed_; }
  /// How many of the muted events the loaded banks have.
  int mutedFound() const;
  /// Why it stopped working, or empty.
  const std::string& why() const { return why_; }
  /// A line for the log, once, when something changed worth saying.
  std::string takeNote();

 private:
  struct Event {
    std::string path;
    bool mute = false;
    void* description = nullptr;
    std::vector<void*> silenced;  ///< Instances this class set to 0.
  };
  void lookUp(double t);
  bool check(int result, Event& e, const char* what);
  void unmute(Event& e);

  FmodApi api_;
  std::vector<Event> events_;
  double nextLookup_ = -1;
  bool speaking_ = false;
  bool failed_ = false;
  int errors_ = 0;
  std::string why_, note_;
};

}  // namespace xn
