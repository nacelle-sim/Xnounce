/// @file
/// Implements `src/app/settings_store.h`; the interface is documented there.
#include "app/settings_store.h"

#include <cstring>
#include <filesystem>
#include <fstream>

#include "core/files.h"
#include "core/strings.h"

namespace xn {

SettingsStore::SettingsStore() {
  defs_ = coreSettingDefs(core_);
  for (SettingDef& d : aircraftSettingDefs(aircraft_)) {
    // Before the first shared setting of the same ini section, or at the end
    // when the section is the aircraft's own.
    auto at = defs_.end();
    for (auto it = defs_.begin(); it != defs_.end(); ++it)
      if (!*it->group && std::strcmp(it->section, d.section) == 0) {
        at = it;
        break;
      }
    defs_.insert(at, std::move(d));
  }
}

std::vector<std::string> SettingsStore::loadIni(const Ini& ini) {
  ini_ = ini;
  loadSettings(ini_, core_);
  loadAircraftOptions(ini_, aircraft_);
  std::vector<std::string> notes = migrateAircraftOptions(ini_, aircraft_);
  for (std::string& n : migrateSettings(ini_, core_))
    notes.push_back(std::move(n));
  return notes;
}

SettingsStore::Loaded SettingsStore::load(const std::string& path,
                                          const std::string& legacyPath) {
  Loaded result;
  std::error_code ec;
  result.fromLegacy =
      !std::filesystem::exists(std::filesystem::u8path(path), ec) &&
      std::filesystem::is_regular_file(std::filesystem::u8path(legacyPath), ec);
  result.migrated =
      loadIni(Ini::parse(readTextFile(result.fromLegacy ? legacyPath : path)));
  return result;
}

bool SettingsStore::save(const std::string& path) {
  storeSettingDefs(defs_, ini_);
  ini_.set("events", "disabled",
           join(std::vector<std::string>(core_.disabledEvents.begin(),
                                         core_.disabledEvents.end()),
                ","));
  std::error_code ec;
  const auto p = std::filesystem::u8path(path);
  std::filesystem::create_directories(p.parent_path(), ec);
  std::ofstream out(p, std::ios::binary | std::ios::trunc);
  if (!out) return false;
  out << "# Xnounce settings (edited from the Xnounce window; see "
         "docs/VP_SPEC.md)\n"
      << ini_.serialize();
  return static_cast<bool>(out);
}

void SettingsStore::resetToDefaults() {
  const Settings keep = core_;
  core_ = Settings{};
  aircraft_ = AircraftOptions{};
  core_.windowVisible = keep.windowVisible;
  core_.winLeft = keep.winLeft;
  core_.winTop = keep.winTop;
  core_.winRight = keep.winRight;
  core_.winBottom = keep.winBottom;
  core_.tab = keep.tab;
  core_.theme = keep.theme;
  core_.fontScale = keep.fontScale;
}

}  // namespace xn
