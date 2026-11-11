/// @file
/// The closed cockpit door as a filter (VP_SPEC § 4.5): a second-order
/// Butterworth low-pass on mono 16-bit samples. It runs on the decode worker.
#pragma once

#include <cstdint>
#include <vector>

namespace xn {

/// Filters `in` into a new buffer. The cutoff is clamped to 20 Hz … 0.45 x
/// the sample rate; the output is clipped to 16 bits.
std::vector<int16_t> lowPass(const std::vector<int16_t>& in, int rate,
                             float cutoffHz);

}  // namespace xn
