/// @file
/// The fault guard around every function X-Plane calls. Exceptions never
/// reach the simulator; after kMaxFaults faults the plugin enters safe mode
/// (docs/ARCHITECTURE.md § 4).
#pragma once

#include <exception>
#include <functional>
#include <string>

namespace xn {

class FaultGuard {
 public:
  static constexpr int kMaxFaults = 3;
  static FaultGuard& get();

  int faults() const { return faults_; }
  bool safeMode() const { return safeMode_; }
  const std::string& lastFault() const { return lastFault_; }
  /// Called when safe mode starts; the plugin stops its audio and flight
  /// loops there.
  void setSafeModeHook(std::function<void()> hook) { hook_ = std::move(hook); }
  /// Leaves safe mode (the Re-enable button).
  void reset();

  void recordFault(const char* where, const char* what);

 private:
  FaultGuard() = default;
  int faults_ = 0;
  bool safeMode_ = false;
  bool inHook_ = false;
  std::string lastFault_;
  std::function<void()> hook_;
};

template <typename R, typename F>
R guarded(const char* where, R fallback, F&& fn) noexcept {
  try {
    return fn();
  } catch (const std::exception& e) {
    FaultGuard::get().recordFault(where, e.what());
  } catch (...) {
    FaultGuard::get().recordFault(where, "unknown exception");
  }
  return fallback;
}

template <typename F>
void guarded(const char* where, F&& fn) noexcept {
  try {
    fn();
  } catch (const std::exception& e) {
    FaultGuard::get().recordFault(where, e.what());
  } catch (...) {
    FaultGuard::get().recordFault(where, "unknown exception");
  }
}

}  // namespace xn
