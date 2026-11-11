/// @file
/// Implements `src/audio/pa_power.h`; the interface is documented there.
#include "audio/pa_power.h"

#include <algorithm>

namespace xn {

namespace {
constexpr double kRampSec = 0.04;      // fade out and in, against clicks
constexpr double kShortLossSec = 3.0;  // longer is a power failure, not a
                                       // transfer
}  // namespace

PaPowerCut::PaPowerCut(uint64_t seed) : rng_(seed ? seed : 1) {}

double PaPowerCut::pickLength() {
  rng_ = rng_ * 6364136223846793005ull + 1442695040888963407ull;
  const double u = static_cast<double>(rng_ >> 11) * 0x1.0p-53;
  return 1.0 + u;  // 1 to 2 s
}

float PaPowerCut::update(double t, bool powered, int source, bool enabled) {
  const double dt = lastT_ < 0 ? 0.0 : std::max(0.0, t - lastT_);
  if (enabled && lastT_ >= 0) {
    if (powered && lastPowered_ && source >= 0 && lastSource_ >= 0 &&
        source != lastSource_)
      cutUntil_ = std::max(cutUntil_, t + pickLength());  // a transfer
    if (!powered && lastPowered_) lossAt_ = t;
    if (powered && !lastPowered_ && lossAt_ >= 0 && t - lossAt_ < kShortLossSec)
      // A short loss: silent from the loss for one to two seconds, and at
      // least a moment after the power is back.
      cutUntil_ = std::max(lossAt_ + pickLength(), t + 0.2);
  }
  if (!enabled) cutUntil_ = -1;
  lastT_ = t;
  lastPowered_ = powered;
  lastSource_ = source;  // unknown in between never counts as a transfer

  const bool silent = enabled && (!powered || t < cutUntil_);
  const float target = silent ? 0.0f : 1.0f;
  const float step = static_cast<float>(dt / kRampSec);
  gain_ = target < gain_ ? std::max(target, gain_ - step)
                         : std::min(target, gain_ + step);
  if (!enabled) gain_ = 1;
  return gain_;
}

}  // namespace xn
