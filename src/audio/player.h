/// @file
/// Plays announcements, music and the buzzer through `XPLMPlayPCMOnBus`.
/// Main thread only (docs/ARCHITECTURE.md § 3, § 4 rule 6).
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>

#include "audio/decode_cache.h"
#include "audio/hearing.h"
#include "core/pack_library.h"
#include "core/purser/purser.h"
#include "core/selector.h"

namespace xn {

class Player : public PurserOutput {
 public:
  explicit Player(DecodeCache& cache);
  ~Player() override;
  Player(const Player&) = delete;
  Player& operator=(const Player&) = delete;

  /// What to play from and how loud: refreshed every tick.
  void setSource(const Pack* pack, const SelectContext& base,
                 const Settings* set, const SimState* sim, bool muted);
  /// Volumes, pause and resume, waiting for decodes, the music playlist.
  void update(float dt);
  void stopAll();
  void prefetch(AnnType type, CallContext ctx);
  /// The aircraft's cockpit call chime; null brings back the generated
  /// Airbus buzzer.
  void setChimeSound(std::shared_ptr<PcmBuffer> pcm);
  /// The power cut-out's gain (`PaPowerCut`), 0 to 1, for the cabin PA and
  /// the music. The interphone and the buzzer don't follow it.
  void setPowerGain(float g) { powerGain_ = g; }
  /// The aircraft sounds the cabin's chime itself (VP_SPEC § 4.7): `chime()`
  /// then only raises a request for the app instead of playing.
  void setOwnChime(bool own) { ownChime_ = own; }
  /// True once per chime requested while the aircraft sounds its own.
  bool takeChimeRequest() {
    const bool r = chimeRequested_;
    chimeRequested_ = false;
    return r;
  }

  /// What plays now, for the window.
  struct NowPlaying {
    std::string label;  ///< Empty when nothing plays.
    bool decoding = false, paused = false;
    bool interphone = false;  ///< The interphone, not the cabin PA.
    double pos = 0, dur = 0;  ///< Seconds.
  };
  NowPlaying nowPlaying() const;
  float progress() const;
  std::string musicLabel() const;
  float lastGain() const { return lastPaGain_; }

  /// X-Plane's completion callback. `key` packs the slot (2 bits), the
  /// twin bit and the generation, so a late callback can be recognised.
  void onChannelDone(uint64_t key);

  std::optional<std::string> play(AnnType type, CallContext ctx,
                                  bool interphone) override;
  bool playing() const override;
  void stop() override;
  void chime() override;
  void setMusic(bool on) override;
  std::optional<Mode> peek(AnnType type, CallContext ctx) const override;

 private:
  struct Voice {
    int slot = 0;
    SoundKind kind = SoundKind::Pa;
    std::shared_ptr<PcmBuffer> pcm;
    std::string path, label;
    void* channel = nullptr;  // the FMOD channel
    unsigned gen = 0;         // a callback of an older generation is ignored
    void* twin = nullptr;     // the muffled twin's channel, or null (§ 4.5)
    unsigned twinGen = 0;
    double pos = 0;       // seconds played
    bool active = false;  // playing, waiting for its decode, or paused
    bool held = false;    // stopped by us, to resume later
    bool done = false;    // ended or failed
  };
  enum Slot { kPa = 0, kMusic = 1, kChime = 2, kSlots = 3 };
  // The callback key: the slot in 2 bits, the twin bit, the generation.
  static constexpr int kTwinBit = 4;

  bool startVoice(Voice& v, double from);
  void haltVoice(Voice& v);  // stops, keeping the position
  void haltTwin(Voice& v);
  /// Silences a channel and stops it a moment later, so it never clicks
  /// (VP_SPEC § 4.6). `key` is its callback key.
  void fadeOut(void* channel, uint64_t key);
  /// Stops the channels whose fade is over; all of them with `now`.
  void stopFaded(float dt, bool now);
  bool wantsTwin(const Voice& v) const;
  float twinGainFor(const Voice& v) const;
  void clearVoice(Voice& v);
  int busFor(const Voice& v) const;
  float gainFor(const Voice& v) const;
  void nextMusicTrack();
  SelectContext ctxFor(CallContext c) const;

  DecodeCache& cache_;
  Voice voices_[kSlots];
  const Pack* pack_ = nullptr;
  SelectContext base_;
  const Settings* set_ = nullptr;
  const SimState* sim_ = nullptr;
  bool muted_ = false;
  bool musicWanted_ = false;
  std::string lastMusic_;
  std::map<int, std::string> lastByType_;
  mutable Rng rng_{12345};
  std::shared_ptr<PcmBuffer> chimePcm_;
  float lastPaGain_ = 0;
  float powerGain_ = 1;
  bool ownChime_ = false;
  bool chimeRequested_ = false;
  /// Channels silenced and waiting to be stopped.
  struct Fading {
    void* channel = nullptr;
    uint64_t key = 0;
    float left = 0;  ///< Seconds until the stop.
  };
  std::vector<Fading> fading_;
};

/// Frees the buffers whose completion callback never came. XPluginStop
/// only.
void releaseParkedAudio();

}  // namespace xn
