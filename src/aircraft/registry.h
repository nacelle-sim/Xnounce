/// @file
/// The list of aircraft modules.
///
/// This is the one place the plugin learns which aircraft exist. Adding an
/// aircraft is a new folder under `src/aircraft/` and one line in
/// registry.cpp (docs/ADDING_AN_AIRCRAFT.md).
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "aircraft/module.h"
#include "aircraft/options.h"

namespace xn {

using AircraftModules = std::vector<std::unique_ptr<AircraftModule>>;

/// Creates every module. A loaded aircraft is offered to them in this order
/// and the first that recognises it takes it. `options` must outlive the
/// modules: they read their settings from it every tick.
AircraftModules makeAircraftModules(const AircraftOptions& options);

/// "the ToLiss A319, A320 and A321": what the modules support, for the
/// sentence "Xnounce works with …" on the Status card.
std::string supportedAircraft(const AircraftModules& modules);
/// "a ToLiss A319, A320 or A321": the same as a request, for "Load …".
std::string loadHint(const AircraftModules& modules);

}  // namespace xn
