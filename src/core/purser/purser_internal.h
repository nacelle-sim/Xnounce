/// @file
/// Constants and helpers shared by the purser's source files. Not part of the
/// purser's interface.
#pragma once

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <string>

#include "core/announcement.h"

namespace xn::purser_detail {

constexpr size_t kTimelineRows = 300;
constexpr double kSeatbeltDescentWindow =
    180;  // no SeatbeltSignOn this close to DescentStarted (§ 4.1)

inline bool atLeast(Phase p, Phase q) {
  return static_cast<int>(p) >= static_cast<int>(q);
}

inline std::string mmss(double s) {
  if (s < 0) s = 0;
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%d:%02d", static_cast<int>(s) / 60,
                static_cast<int>(s) % 60);
  return buf;
}

inline std::string hmm(double s) {
  if (s < 0) s = 0;
  const int m = static_cast<int>(std::lround(s / 60.0));
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%d:%02d h", m / 60, m % 60);
  return buf;
}

constexpr double kBoardingJitter = 0.15;  // +/- of the boarding length (§ 4.1b)
constexpr double kBoardingMaxFactor = 1.5;
constexpr double kRetimeMinS = 60;
constexpr double kClockJumpS = 300;
constexpr double kLateStartS = 60;  // later than this is a late start
constexpr double kSettleS = 10;     // settling after a load (§ 4.4)
constexpr double kTaxiKt = 5;       // above this the aircraft taxies (§ 4.2)
/// A loss of AC power longer than this is a power loss, not a transfer
/// (VP_SPEC § 4.6, D-13).
constexpr double kPowerLossS = 3.0;
/// A time that never comes: a followed boarding ends when the aircraft says
/// so (§ 4.1c).
constexpr double kNever = 1e12;

}  // namespace xn::purser_detail
