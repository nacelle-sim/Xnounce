/// @file
/// Refuelling detection (VP_SPEC § 4.2): on the ground, fuel rising faster
/// than the rate setting and slower than the maximum (a faster rise is an
/// instant load). A detection holds for 60 s.
#pragma once

#include "core/settings.h"
#include "core/sim_state.h"

namespace xn {

class RefuelDetector {
 public:
  void reset();
  /// One sample. Returns the rate when refuelling was newly detected, else a
  /// negative number.
  double update(const SimState& s, const Settings& set);
  bool refueling(double t) const { return t < until_; }

 private:
  double sampleT_ = -1;
  float sampleKg_ = 0;
  double until_ = -1;
};

}  // namespace xn
