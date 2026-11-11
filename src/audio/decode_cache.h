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
  std::shared_ptr<PcmBuffer> get(const std::string& path);
  /// Starts decoding a file that will be needed soon.
  void request(const std::string& path);
  bool failed(const std::string& path, std::string* error = nullptr) const;
  void shutdown();
  size_t bytes() const;

 private:
  void worker();
  void evictLocked();

  mutable std::mutex mu_;
  std::condition_variable cv_;
  std::deque<std::string> jobs_;
  std::set<std::string> pending_;
  std::map<std::string, std::shared_ptr<PcmBuffer>> ready_;
  std::list<std::string> lru_;  // the newest at the front
  std::map<std::string, std::string> bad_;
  size_t bytes_ = 0, maxBytes_;
  float muffleHz_ = 0;
  bool stop_ = false;
  std::thread thread_;
};

}  // namespace xn
