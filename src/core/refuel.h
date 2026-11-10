// refuel detection 4.2 ground fuel rising between rate and max faster iscs
// instant load held 60s
#pragma once

#include "core/settings.h"
#include "core/sim_state.h"

namespace xn {

class RefuelDetector {
 public:
  void reset();
  // rate when newly detected this call else negative
  double update(const SimState& s, const Settings& set);
  bool refueling(double t) const { return t < until_; }

 private:
  double sampleT_ = -1;
  float sampleKg_ = 0;
  double until_ = -1;
};

}  // namespace xn
