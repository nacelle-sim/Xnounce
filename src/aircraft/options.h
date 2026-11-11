/// @file
/// The settings of all aircraft modules, kept together so they can be loaded,
/// saved and tested without X-Plane.
///
/// A new module adds its options struct here and its lines to the four
/// functions in options.cpp (docs/ADDING_AN_AIRCRAFT.md).
#pragma once

#include <string>
#include <vector>

#include "aircraft/toliss/toliss_options.h"
#include "core/settings.h"

namespace xn {

/// Every module's own settings.
struct AircraftOptions {
  toliss::Options toliss;
};

/// The definitions of all aircraft settings, bound to `o`.
std::vector<SettingDef> aircraftSettingDefs(AircraftOptions& o);
/// Loads the aircraft settings and repairs contradicting values.
void loadAircraftOptions(const Ini& ini, AircraftOptions& o);
/// Stores the aircraft settings. Other content of the ini is kept.
void storeAircraftOptions(const AircraftOptions& o, Ini& ini);
/// Upgrades defaults an older version got wrong. Returns lines for the log.
std::vector<std::string> migrateAircraftOptions(const Ini& ini,
                                                AircraftOptions& o);

}  // namespace xn
