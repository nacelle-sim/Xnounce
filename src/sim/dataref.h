/// @file
/// A safe dataref wrapper (docs/ARCHITECTURE.md § 4, rule 3): type and bounds
/// checked, never throws. A missing dataref reads as nothing or empty.
#pragma once

#include <XPLMDataAccess.h>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

namespace xn {

class DataRef {
 public:
  DataRef() = default;
  explicit DataRef(const char* name) : name_(name) {}

  /// Looks the dataref up. True when it exists and is good.
  bool resolve() {
    ref_ = name_ ? XPLMFindDataRef(name_) : nullptr;
    types_ = ref_ ? XPLMGetDataRefTypes(ref_) : 0;
    return good();
  }
  bool good() const { return ref_ && XPLMIsDataRefGood(ref_); }
  const char* name() const { return name_; }
  XPLMDataTypeID types() const { return types_; }

  std::optional<float> getFloat() const {
    if (!good()) return std::nullopt;
    if (types_ & xplmType_Float) return XPLMGetDataf(ref_);
    if (types_ & xplmType_Double) return static_cast<float>(XPLMGetDatad(ref_));
    if (types_ & xplmType_Int) return static_cast<float>(XPLMGetDatai(ref_));
    if (types_ & xplmType_FloatArray) {
      float v = 0;
      return XPLMGetDatavf(ref_, &v, 0, 1) == 1 ? std::optional<float>(v)
                                                : std::nullopt;
    }
    if (types_ & xplmType_IntArray) {
      int v = 0;
      return XPLMGetDatavi(ref_, &v, 0, 1) == 1
                 ? std::optional<float>(static_cast<float>(v))
                 : std::nullopt;
    }
    return std::nullopt;
  }
  std::optional<double> getDouble() const {
    if (!good()) return std::nullopt;
    if (types_ & xplmType_Double) return XPLMGetDatad(ref_);
    if (auto f = getFloat()) return static_cast<double>(*f);
    return std::nullopt;
  }
  std::optional<int> getInt() const {
    if (!good()) return std::nullopt;
    if (types_ & xplmType_Int) return XPLMGetDatai(ref_);
    if (auto f = getFloat())
      return static_cast<int>(*f + (*f >= 0 ? 0.5f : -0.5f));
    return std::nullopt;
  }
  /// The number of entries, or 0 when it isn't an array.
  int arraySize() const {
    if (!good()) return 0;
    if (types_ & xplmType_FloatArray) return XPLMGetDatavf(ref_, nullptr, 0, 0);
    if (types_ & xplmType_IntArray) return XPLMGetDatavi(ref_, nullptr, 0, 0);
    return 0;
  }
  /// Up to `maxCount` entries; integer arrays are converted.
  std::vector<float> getArray(int maxCount = 64) const {
    std::vector<float> out;
    const int n = std::min(arraySize(), maxCount);
    if (n <= 0) return out;
    out.resize(static_cast<size_t>(n));
    if (types_ & xplmType_FloatArray) {
      const int got = XPLMGetDatavf(ref_, out.data(), 0, n);
      out.resize(static_cast<size_t>(std::max(0, std::min(got, n))));
    } else {
      std::vector<int> tmp(static_cast<size_t>(n));
      const int got = XPLMGetDatavi(ref_, tmp.data(), 0, n);
      const int m = std::max(0, std::min(got, n));
      out.resize(static_cast<size_t>(m));
      for (int k = 0; k < m; ++k)
        out[static_cast<size_t>(k)] =
            static_cast<float>(tmp[static_cast<size_t>(k)]);
    }
    return out;
  }
  /// True when X-Plane accepts writes to it. Some aircraft accept a write
  /// and discard it, so a write is always checked by reading back.
  bool canWrite() const { return good() && XPLMCanWriteDataRef(ref_); }
  /// Writes a number (entry 0 of an array). Only aircraft modules' cabin
  /// actions write aircraft datarefs (docs/ARCHITECTURE.md § 4, rule 4).
  /// Returns false when the dataref is missing or not writable.
  bool set(float v) const {
    if (!canWrite()) return false;
    if (types_ & xplmType_Float) {
      XPLMSetDataf(ref_, v);
    } else if (types_ & xplmType_Double) {
      XPLMSetDatad(ref_, static_cast<double>(v));
    } else if (types_ & xplmType_Int) {
      XPLMSetDatai(ref_, static_cast<int>(v + (v >= 0 ? 0.5f : -0.5f)));
    } else if (types_ & xplmType_FloatArray) {
      XPLMSetDatavf(ref_, &v, 0, 1);
    } else {
      return false;
    }
    return true;
  }

  /// A byte dataref as text, trimmed; empty when absent.
  std::string getString(int maxBytes = 512) const {
    if (!good() || !(types_ & xplmType_Data)) return {};
    std::vector<char> buf(static_cast<size_t>(maxBytes) + 1, '\0');
    const int got = XPLMGetDatab(ref_, buf.data(), 0, maxBytes);
    if (got <= 0) return {};
    buf[static_cast<size_t>(std::min(got, maxBytes))] = '\0';
    std::string s(buf.data());
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' ||
                          s.back() == '\r' || s.back() == '\n'))
      s.pop_back();
    size_t a = 0;
    while (a < s.size() && (s[a] == ' ' || s[a] == '\t')) ++a;
    return s.substr(a);
  }

 private:
  const char* name_ = nullptr;
  XPLMDataRef ref_ = nullptr;
  XPLMDataTypeID types_ = 0;
};

}  // namespace xn
