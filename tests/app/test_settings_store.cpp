// The settings store: loading and saving Xnounce.ini, the file of the old
// plugin name, upgrades of old defaults, and the Settings pages.
#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "app/settings_store.h"
#include "core/files.h"
#include "support/paths.h"

using namespace xn;

namespace {

// A fixture as Ini text. Parsing and serializing it again makes the
// comparison independent of the line ends the file was checked out with.
std::string fixture(const char* name) {
  const std::string text = readTextFile(
      (std::filesystem::path(XN_TEST_DATA_DIR) / "fixtures" / name).string());
  return text.empty() ? text : Ini::parse(text).serialize();
}

std::string tmpFile(const char* name) {
  const std::filesystem::path p = xn::testing::tmpDir() / name;
  std::error_code ec;
  std::filesystem::remove(p, ec);
  return p.string();
}

// The settings added after 0.3.1, as section and key. A file written today
// is a 0.3.1 file plus these.
const std::vector<std::pair<std::string, std::string>> kNewSince031 = {
    {"hearing", "pa_system"},
    {"hearing", "pa_cutout"},
    {"boarding", "ofp_back_at_origin_s"},
    {"hearing", "loudness"},
    {"hearing", "cabin_acoustics"},
    {"hearing", "speaker_pitch_m"},
};

// True for a setting added after 0.3.1: the list above, the 737's section and
// its boarding rows (b736_min ... b739_seats).
bool newSince031(const std::string& sect, const std::string& key) {
  if (sect == "zibo737") return true;
  if (sect == "boarding" && key.rfind("b73", 0) == 0) return true;
  return std::find(kNewSince031.begin(), kNewSince031.end(),
                   std::make_pair(sect, key)) != kNewSince031.end();
}

// The ini a store writes, without the comment line at the top.
Ini savedIni(SettingsStore& store, const char* name) {
  const std::string path = tmpFile(name);
  REQUIRE(store.save(path));
  return Ini::parse(readTextFile(path));
}

// The same, without the settings added after 0.3.1, to compare with a file of
// 0.3.1.
std::string saved(SettingsStore& store, const char* name) {
  const Ini ini = savedIni(store, name);
  Ini old;
  for (const auto& [sect, keys] : ini.sections())
    for (const auto& [key, value] : keys)
      if (!newSince031(sect, key)) old.set(sect, key, value);
  return old.serialize();
}

const SettingDef* defOf(const SettingsStore& store, const char* key) {
  for (const SettingDef& d : store.defs())
    if (std::string(d.key) == key) return &d;
  return nullptr;
}

}  // namespace

TEST_CASE("settings store: a new file is written exactly as 0.3.1 wrote it") {
  // The fixtures were written by the settings code of 0.3.1 itself: once with
  // the defaults, once with every setting moved off its default.
  const std::string defaults = fixture("settings-0.3.1-defaults.ini");
  REQUIRE(!defaults.empty());
  SettingsStore fresh;
  CHECK(saved(fresh, "defaults.ini") == defaults);
  // The newer settings are written with their defaults.
  const Ini ini = savedIni(fresh, "defaults-new.ini");
  for (const auto& entry : kNewSince031) {
    CAPTURE(entry.second);
    CHECK(ini.get(entry.first, entry.second) != nullptr);
  }
  CHECK(*ini.get("hearing", "pa_system") == "modern");
  CHECK(ini.get("zibo737", "takeoff_lever") != nullptr);
  CHECK(ini.get("boarding", "b738_seats") != nullptr);
}

TEST_CASE(
    "settings store: every value of a 0.3.1 file survives load and save") {
  const std::string changed = fixture("settings-0.3.1-changed.ini");
  REQUIRE(!changed.empty());
  SettingsStore store;
  CHECK(store.loadIni(Ini::parse(changed)).empty());  // nothing to upgrade
  CHECK(saved(store, "changed.ini") == changed);
  // a few values, to be sure they reached the variables and not just the ini
  CHECK(!store.core().enabled);
  CHECK(store.core().theme == 1);
  CHECK(store.core().disabledEvents.count("SafetyBriefing") == 1);
  CHECK(store.aircraft().toliss.acpPaIndex == 8);
  CHECK(store.aircraft().toliss.a321.seats == 201);
  CHECK(store.aircraft().toliss.volumeOverrideRef == "x;volume_override_ref");
}

TEST_CASE("settings store: 0.2.0 defaults are upgraded once") {
  // A 0.2.0 file has every key and no settings_version.
  SettingsStore store;
  const std::vector<std::string> notes = store.loadIni(Ini::parse(
      "[hearing]\nfloor = 0.25\n[toliss]\nacp_pa_index = 7\nacp_cab_index = "
      "6\n"));
  CHECK(notes.size() == 2);
  CHECK(store.aircraft().toliss.acpPaIndex == 15);
  CHECK(store.aircraft().toliss.acpCabIndex == 6);
  CHECK(store.core().hearingFloor == doctest::Approx(0.10f));
  CHECK(store.core().settingsVersion == kSettingsVersion);

  // values the user chose are kept
  SettingsStore mine;
  CHECK(
      mine.loadIni(Ini::parse(
                       "[hearing]\nfloor = 0.3\n[toliss]\nacp_pa_index = 12\n"))
          .empty());
  CHECK(mine.aircraft().toliss.acpPaIndex == 12);
  CHECK(mine.core().hearingFloor == doctest::Approx(0.3f));

  // a current file isn't touched again, even with the old values
  SettingsStore now;
  CHECK(now.loadIni(Ini::parse("[general]\nsettings_version = 2\n[hearing]\n"
                               "floor = 0.25\n[toliss]\nacp_pa_index = 7\n"))
            .empty());
  CHECK(now.aircraft().toliss.acpPaIndex == 7);
  CHECK(now.core().hearingFloor == doctest::Approx(0.25f));
}

TEST_CASE("settings store: FXAnnounce.ini is read once and never changed") {
  const std::string path = tmpFile("Xnounce.ini");
  const std::string legacy = tmpFile("FXAnnounce.ini");
  {
    std::ofstream out(legacy, std::ios::binary);  // LF on Windows too
    out << "[general]\nsettings_version = 2\nmaster_volume = 0.5\n";
  }
  SettingsStore store;
  SettingsStore::Loaded loaded = store.load(path, legacy);
  CHECK(loaded.fromLegacy);
  CHECK(store.core().masterVolume == doctest::Approx(0.5f));
  REQUIRE(store.save(path));
  CHECK(readTextFile(legacy) ==
        "[general]\nsettings_version = 2\nmaster_volume = 0.5\n");

  // once Xnounce.ini exists the old file is ignored
  {
    std::ofstream out(legacy, std::ios::binary);  // LF on Windows too
    out << "[general]\nsettings_version = 2\nmaster_volume = 0.2\n";
  }
  SettingsStore again;
  loaded = again.load(path, legacy);
  CHECK(!loaded.fromLegacy);
  CHECK(again.core().masterVolume == doctest::Approx(0.5f));

  // no file at all: the defaults
  SettingsStore none;
  loaded = none.load(tmpFile("missing.ini"), tmpFile("missing-too.ini"));
  CHECK(!loaded.fromLegacy);
  CHECK(none.core().masterVolume == doctest::Approx(1.0f));
}

TEST_CASE("settings store: reset keeps the window and the theme") {
  SettingsStore store;
  store.core().masterVolume = 0.3f;
  store.core().theme = 1;
  store.core().winLeft = 40;
  store.core().tab = 3;
  store.aircraft().toliss.acpPaIndex = 9;
  store.resetToDefaults();
  CHECK(store.core().masterVolume == doctest::Approx(1.0f));
  CHECK(store.aircraft().toliss.acpPaIndex == 15);
  CHECK(store.core().theme == 1);
  CHECK(store.core().winLeft == 40);
  CHECK(store.core().tab == 3);
}

TEST_CASE(
    "settings store: 0.3.0 window keys, the Inspector tab id and the Settings "
    "pages (ARCHITECTURE 7)") {
  Settings s;
  CHECK(s.theme == 0);
  CHECK(s.showTimeline);
  CHECK(!s.showLog);
  s.theme = 1;
  s.showTimeline = false;
  s.showLog = true;
  s.fontScale = 1.4f;
  Ini ini;
  storeSettings(s, ini);
  CHECK(*ini.get("ui", "theme") == "white");
  Settings back;
  loadSettings(ini, back);
  CHECK(back.theme == 1);
  CHECK(!back.showTimeline);
  CHECK(back.showLog);
  CHECK(back.fontScale == doctest::Approx(1.4f));  // 0.2.x never read this back

  Settings t;
  loadSettings(Ini::parse("[ui]\ntab = 4\n"), t);  // the old Inspector id
  CHECK(t.tab == 0);
  loadSettings(Ini::parse("[ui]\ntab = 5\n"), t);
  CHECK(t.tab == 5);

  // every page has settings, no help text mentions removed features
  SettingsStore store;
  int perPage[kSettingPageCount] = {};
  for (const SettingDef& d : store.defs()) {
    if (d.hidden) continue;
    const int k = static_cast<int>(d.page);
    REQUIRE(k >= 0);
    REQUIRE(k < kSettingPageCount);
    ++perPage[k];
    CHECK_MESSAGE(std::string(d.help).find("Inspector") == std::string::npos,
                  d.key);
    CHECK_MESSAGE(std::string(d.help).find("probe") == std::string::npos,
                  d.key);
  }
  for (int k = 0; k < kSettingPageCount; ++k)
    CHECK_MESSAGE(perPage[k] > 0, settingPageName(static_cast<SettingPage>(k)));
  auto pageOf = [&](const char* key) {
    const SettingDef* d = defOf(store, key);
    return d ? d->page : SettingPage::Count;
  };
  CHECK(pageOf("enabled") == SettingPage::Announcements);
  CHECK(pageOf("master_volume") == SettingPage::Sound);
  CHECK(pageOf("pa_bus") == SettingPage::Advanced);
  CHECK(pageOf("show_log") == SettingPage::Advanced);
  CHECK(pageOf("welcome_min_s") == SettingPage::Boarding);
  CHECK(pageOf("a320_min") == SettingPage::Boarding);
  CHECK(pageOf("expedite_s") == SettingPage::Timing);
  CHECK(pageOf("acp_pa_index") == SettingPage::Aircraft);
  CHECK(pageOf("theme") == SettingPage::Display);
  // aircraft settings carry their group, shared ones don't
  CHECK(std::string(defOf(store, "acp_pa_index")->group) == "toliss");
  CHECK(std::string(defOf(store, "a320_min")->groupName) == "ToLiss");
  CHECK(std::string(defOf(store, "enabled")->group).empty());
  // the definitions point at the store's own variables
  *defOf(store, "acp_pa_index")->i = 11;
  CHECK(store.aircraft().toliss.acpPaIndex == 11);
}
