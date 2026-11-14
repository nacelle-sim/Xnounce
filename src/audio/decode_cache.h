/// @file
/// Decoded sounds, kept in memory and decoded on a background worker
/// (docs/ARCHITECTURE.md § 3).

#pragma once

#include <condition_variable>
#include <deque>
#include <list>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>

#include "audio/decoder.h"
#include "audio/pa_effects.h"

namespace xn {

class DecodeCache {
 public:
  explicit DecodeCache(size_t maxBytes = 192u << 20);
  ~DecodeCache();
  DecodeCache(const DecodeCache&) = delete;
  DecodeCache& operator=(const DecodeCache&) = delete;

  void setMaxBytes(size_t b);
  /// The low-pass cutoff for the muffled copies; 0 makes none. A change
  /// empties the cache.
  void setMuffleCutoff(float hz);
  /// How the cabin PA sounds on this airframe. Announcements and music
  /// (`pa`) are kept as aged copies, decoded once; a change empties the
  /// cache. The interphone and the buzzer stay clean.
  void setPaAging(PaPreset preset, const std::string& tailNumber);
  /// Even loudness for everything decoded here, and the cabin acoustics for
  /// the cabin PA copies (VP_SPEC § 4.6). Both off by default; a change
  /// empties the cache.
  void setShaping(bool loudness, bool cabinAcoustics);
  /// The decoded file, or null while it decodes (the request is queued).
  /// @param pa The cabin PA copy (aged with the airframe's preset); false for
  ///        the clean sound of the interphone.
  /// @param urgent Decode before the queued music tracks (an announcement).
  /// @param music The boarding music, levelled to its own loudness.
  std::shared_ptr<PcmBuffer> get(const std::string& path, bool pa = false,
                                 bool urgent = false, bool music = false);
  /// Starts decoding a file that will be needed soon.
  void request(const std::string& path, bool pa = false);
  bool failed(const std::string& path, std::string* error = nullptr) const;
  void shutdown();
  size_t bytes() const;

 private:
  struct Job {
    std::string path, key;
    bool pa = false;
    bool music = false;
  };
  /// The cabin PA copy differs from the clean sound.
  bool paDiffers() const { return preset_ != PaPreset::Modern || acoustics_; }
  void worker();
  void evictLocked();
  void clearLocked();
  std::string keyFor(const std::string& path, bool pa) const;

  mutable std::mutex mu_;
  std::condition_variable cv_;
  std::deque<Job> jobs_;
  std::set<std::string> pending_;
  std::map<std::string, std::shared_ptr<PcmBuffer>> ready_;
  std::list<std::string> lru_;  // the newest at the front
  std::map<std::string, std::string> bad_;
  size_t bytes_ = 0, maxBytes_;
  float muffleHz_ = 0;
  PaPreset preset_ = PaPreset::Modern;
  std::string tail_;
  bool loudness_ = false, acoustics_ = false;
  /// Bumped by every change of the muffle or the aging, so a decode that
  /// started before it is dropped.
  unsigned epoch_ = 0;
  bool stop_ = false;
  std::thread thread_;
};

}  // namespace xn
