/// @file
/// The settings in memory and in `Xnounce.ini`.
///
/// The store owns the shared settings (`Settings`), every aircraft module's
/// own settings (`AircraftOptions`) and the ini file they were read from, and
/// offers one list of definitions for all of them. It needs no X-Plane.
#pragma once

#include <string>
#include <vector>

#include "aircraft/options.h"
#include "core/ini.h"
#include "core/settings.h"

namespace xn {

class SettingsStore {
 public:
  SettingsStore();
  // The definitions point at this object's members.
  SettingsStore(const SettingsStore&) = delete;
  SettingsStore& operator=(const SettingsStore&) = delete;

  Settings& core() { return core_; }
  const Settings& core() const { return core_; }
  AircraftOptions& aircraft() { return aircraft_; }
  const AircraftOptions& aircraft() const { return aircraft_; }

  /// Every setting, shared and per aircraft, in the order of the ini file and
  /// of the Settings pages. An aircraft's rows stand where the shared rows of
  /// the same ini section begin, so `[boarding]` lists the per-type lead
  /// times first, as it always has.
  const std::vector<SettingDef>& defs() const { return defs_; }

  /// What `load` did.
  struct Loaded {
    /// The settings came from the file of the plugin's old name.
    bool fromLegacy = false;
    /// One line for each default of an older version that was upgraded.
    std::vector<std::string> migrated;
  };
  /// Reads the settings file. When `path` doesn't exist and `legacyPath`
  /// does, the old file is read instead (docs/ARCHITECTURE.md § 5.1); it is
  /// never changed. A missing file leaves the defaults.
  Loaded load(const std::string& path, const std::string& legacyPath);
  /// Takes the settings from ini text and upgrades old defaults, as `load`
  /// does for a file. Returns the upgrade lines.
  std::vector<std::string> loadIni(const Ini& ini);
  /// Writes the settings to `path`, creating its folder. Keys the plugin
  /// doesn't know stay in the file. False when the file can't be written.
  bool save(const std::string& path);

  /// Puts every setting back to its default, except the window's position,
  /// tab, theme and text size.
  void resetToDefaults();

 private:
  Settings core_;
  AircraftOptions aircraft_;
  std::vector<SettingDef> defs_;
  Ini ini_;
};

}  // namespace xn
