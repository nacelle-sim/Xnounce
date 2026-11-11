/// @file
/// Asks X-Plane which aircraft the user has loaded.
#pragma once

#include "aircraft/loaded_aircraft.h"

namespace xn {

/// Reads the identity of the user's aircraft. Main thread only. One of the
/// datarefs it reads (the livery path) is slow, so call it when an aircraft
/// or livery loads, not every frame.
LoadedAircraft readLoadedAircraft();

}  // namespace xn
