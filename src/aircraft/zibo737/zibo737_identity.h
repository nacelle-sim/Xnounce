/// @file
/// Recognising the Zibo and LevelUp Boeing 737.
///
/// Both are built on the same systems (`zibomod.xpl` and the same xlua
/// scripts), so one module covers them (docs/aircraft/zibo737.md).
#pragma once

#include <string>

#include "aircraft/module.h"

namespace xn::zibo737 {

/// Who made the loaded 737.
enum class Maker { Unknown, Zibo, LevelUp };

/// The maker, from the .acf author and studio and the aircraft's folder.
Maker makerOf(const LoadedAircraft& aircraft);
/// "737-600" to "737-900ER" from the ICAO designator and the .acf file name,
/// or empty when it is no 737NG.
std::string modelOf(const LoadedAircraft& aircraft);

/// Decides whether the loaded aircraft is a Zibo or LevelUp 737.
/// `markerPresent` says whether a dataref that only their systems register
/// exists.
Recognition classify(const LoadedAircraft& aircraft, bool markerPresent);

}  // namespace xn::zibo737
