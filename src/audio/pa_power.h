/// @file
/// The cabin PA and its power (VP_SPEC § 4.6): without AC power the PA and the
/// music are silent; on a transfer (the source powering the AC buses changes,
/// or the power is lost for under 3 s) they go silent for one to two seconds.
/// A pure state machine fed every frame; the player multiplies its cabin
/// volumes by its gain.
#pragma once

#include <cstdint>

namespace xn {

class PaPowerCut {
 public:
  /// @param seed Picks the length of each cut-out, repeatably.
  explicit PaPowerCut(uint64_t seed = 1);

  /// Advances to time `t` (seconds) and returns the gain, 0 to 1.
  /// @param powered The AC buses that feed the cabin have power.
  /// @param source Which source powers them (aircraft-specific id), or -1
  ///        when the aircraft doesn't say.
  /// @param enabled The setting "Cut out on power transfer". Off, transfers
  ///        don't cut out, but without power the PA is still silent.
  float update(double t, bool powered, int source, bool enabled);
  float gain() const { return gain_; }
  /// True while a cut-out holds the PA silent.
  bool cutting() const { return cutUntil_ > lastT_ || !lastPowered_; }

 private:
  double pickLength();

  uint64_t rng_;
  double lastT_ = -1;
  bool lastPowered_ = true;
  int lastSource_ = -1;
  double lossAt_ = -1;
  double cutUntil_ = -1;
  float gain_ = 1;
};

}  // namespace xn
