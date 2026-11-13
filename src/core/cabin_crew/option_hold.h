/// @file
/// Holds an option of the aircraft at a value while a cabin action is on, and
/// puts the aircraft's own value back afterwards (docs/ARCHITECTURE.md § 4,
/// rule 4). The 737's crew announcement switches are held this way.
#pragma once

#include <cmath>
#include <optional>

namespace xn {

/// Pure: the owner reads and writes the dataref; this decides what to write.
class OptionHold {
 public:
  explicit OptionHold(float target = 0) : target_(target) {}

  /// One tick. `hold`: the option should be held now. `reading`: the
  /// aircraft's value, or nothing when it is unknown. Returns the value to
  /// write now, if any.
  ///
  /// When the reading leaves the held value (the user, or the aircraft
  /// reloading its configuration), the option is the user's: it is no longer
  /// held, and not put back, until `hold` goes false and true again.
  std::optional<float> update(bool hold, std::optional<float> reading) {
    if (!hold) {
      userTook_ = false;
      return release();
    }
    if (userTook_ || !reading) return std::nullopt;
    if (!holding_) {
      holding_ = true;
      original_ = *reading;
      wrote_ = !same(*reading, target_);
      return wrote_ ? std::optional<float>(target_) : std::nullopt;
    }
    if (!same(*reading, target_)) {
      holding_ = false;
      userTook_ = true;
      changed_ = true;
    }
    return std::nullopt;
  }

  /// Stop holding. Returns the aircraft's value to put back, if the option
  /// was changed by the plugin and is still held.
  std::optional<float> release() {
    const bool restore = holding_ && wrote_;
    holding_ = false;
    wrote_ = false;
    return restore ? std::optional<float>(original_) : std::nullopt;
  }
  /// Forgets without putting back: the aircraft is gone.
  void forget() { holding_ = wrote_ = userTook_ = false; }

  bool holding() const { return holding_; }
  /// The value the aircraft had before the plugin held it.
  float original() const { return original_; }
  /// True once, after the user took the option over (for the log).
  bool takeChanged() {
    const bool c = changed_;
    changed_ = false;
    return c;
  }

 private:
  static bool same(float a, float b) { return std::fabs(a - b) < 0.01f; }

  float target_;
  float original_ = 0;
  bool holding_ = false;
  bool wrote_ = false;
  bool userTook_ = false;
  bool changed_ = false;
};

}  // namespace xn
