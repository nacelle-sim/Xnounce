/// @file
/// Implements `src/audio/decode_cache.h`; the interface is documented there.
#include "audio/decode_cache.h"

#include <algorithm>

#include "audio/muffle.h"

namespace xn {

DecodeCache::DecodeCache(size_t maxBytes) : maxBytes_(maxBytes) {
  thread_ = std::thread([this] { worker(); });
}

DecodeCache::~DecodeCache() { shutdown(); }

void DecodeCache::shutdown() {
  {
    std::lock_guard<std::mutex> lock(mu_);
    stop_ = true;
    jobs_.clear();
  }
  cv_.notify_all();
  if (thread_.joinable()) thread_.join();
}

void DecodeCache::setMaxBytes(size_t b) {
  std::lock_guard<std::mutex> lock(mu_);
  maxBytes_ = b;
  evictLocked();
}

void DecodeCache::clearLocked() {
  ready_.clear();
  lru_.clear();
  bytes_ = 0;
  ++epoch_;
}

void DecodeCache::setMuffleCutoff(float hz) {
  std::lock_guard<std::mutex> lock(mu_);
  if (!(hz > 0)) hz = 0;
  if (hz == muffleHz_) return;
  muffleHz_ = hz;
  clearLocked();
}

void DecodeCache::setPaAging(PaPreset preset, const std::string& tailNumber) {
  std::lock_guard<std::mutex> lock(mu_);
  if (preset == preset_ && tailNumber == tail_) return;
  preset_ = preset;
  tail_ = tailNumber;
  clearLocked();
}

std::string DecodeCache::keyFor(const std::string& path, bool pa) const {
  // A Modern PA copy is the clean sound: the same entry serves both.
  return pa && preset_ != PaPreset::Modern ? path + "\x1F" + "pa" : path;
}

size_t DecodeCache::bytes() const {
  std::lock_guard<std::mutex> lock(mu_);
  return bytes_;
}

std::shared_ptr<PcmBuffer> DecodeCache::get(const std::string& path, bool pa,
                                            bool urgent) {
  std::unique_lock<std::mutex> lock(mu_);
  const std::string key = keyFor(path, pa);
  auto it = ready_.find(key);
  if (it != ready_.end()) {
    lru_.remove(key);
    lru_.push_front(key);
    return it->second;
  }
  if (stop_ || bad_.count(path)) return nullptr;
  if (pending_.count(key)) {
    // Already queued: an announcement moves ahead of the music.
    if (urgent) {
      auto j = std::find_if(jobs_.begin(), jobs_.end(),
                            [&](const Job& q) { return q.key == key; });
      if (j != jobs_.end() && j != jobs_.begin()) {
        Job moved = *j;
        jobs_.erase(j);
        jobs_.push_front(std::move(moved));
      }
    }
    return nullptr;
  }
  pending_.insert(key);
  Job job{path, key, pa && preset_ != PaPreset::Modern};
  if (urgent)
    jobs_.push_front(std::move(job));
  else
    jobs_.push_back(std::move(job));
  lock.unlock();
  cv_.notify_one();
  return nullptr;
}

void DecodeCache::request(const std::string& path, bool pa) {
  (void)get(path, pa);
}

bool DecodeCache::failed(const std::string& path, std::string* error) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = bad_.find(path);
  if (it == bad_.end()) return false;
  if (error) *error = it->second;
  return true;
}

void DecodeCache::evictLocked() {
  // Drop the least recently used until the cache is under its limit.
  for (auto it = lru_.end(); bytes_ > maxBytes_ && it != lru_.begin();) {
    --it;
    if (it == lru_.begin()) break;
    auto r = ready_.find(*it);
    if (r != ready_.end() && r->second.use_count() == 1) {
      bytes_ -= r->second->bytes();
      ready_.erase(r);
      it = lru_.erase(it);
    }
  }
}

void DecodeCache::worker() {
  while (true) {
    Job job;
    float muffleHz = 0;
    PaPreset preset = PaPreset::Modern;
    std::string tail;
    unsigned epoch = 0;
    {
      std::unique_lock<std::mutex> lock(mu_);
      cv_.wait(lock, [&] { return stop_ || !jobs_.empty(); });
      if (stop_) return;
      job = jobs_.front();
      jobs_.pop_front();
      muffleHz = muffleHz_;
      preset = preset_;
      tail = tail_;
      epoch = epoch_;
    }
    const std::string& path = job.path;
    DecodeResult r;
    try {
      r = decodeOgg(path);
      // The aged copy first; the muffled twin is that copy through the door.
      if (r.pcm && job.pa)
        r.pcm->samples =
            agePa(r.pcm->samples, r.pcm->rate, preset, paSeed(tail, path));
      if (r.pcm && muffleHz > 0)
        r.pcm->muffled = lowPass(r.pcm->samples, r.pcm->rate, muffleHz);
    } catch (const std::exception& e) {
      r.error = std::string("decoder exception: ") + e.what();
    } catch (...) {
      r.error = "decoder exception";
    }
    std::lock_guard<std::mutex> lock(mu_);
    pending_.erase(job.key);
    if (stop_) return;
    if (!r.pcm) {
      bad_[path] = r.error;
      continue;
    }
    if (epoch != epoch_) continue;  // the muffle or the aging changed
    bytes_ += r.pcm->bytes();
    ready_[job.key] = r.pcm;
    lru_.push_front(job.key);
    evictLocked();
  }
}

}  // namespace xn
