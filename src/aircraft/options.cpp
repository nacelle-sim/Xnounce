/// @file
/// Implements `src/aircraft/options.h`; the interface is documented there.
#include "aircraft/options.h"

namespace xn {

std::vector<SettingDef> aircraftSettingDefs(AircraftOptions& o) {
  return toliss::settingDefs(o.toliss);
}

void loadAircraftOptions(const Ini& ini, AircraftOptions& o) {
  loadSettingDefs(ini, aircraftSettingDefs(o));
  toliss::normalize(o.toliss);
}

void storeAircraftOptions(const AircraftOptions& o, Ini& ini) {
  // The definitions point at variables they may change, so they are bound to
  // a copy.
  AircraftOptions copy = o;
  storeSettingDefs(aircraftSettingDefs(copy), ini);
}

std::vector<std::string> migrateAircraftOptions(const Ini& ini,
                                                AircraftOptions& o) {
  return toliss::migrate(ini, o.toliss);
}

}  // namespace xn
