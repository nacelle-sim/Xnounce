/// @file
/// What X-Plane says about the aircraft the user has loaded.
#pragma once

#include <string>

namespace xn {

/// The identity of the user's aircraft, read once after it loads
/// (`sim/loaded_aircraft`). Aircraft modules decide from it whether the
/// aircraft is theirs.
struct LoadedAircraft {
  std::string acfPath;  ///< Full path of the .acf file.
  std::string acfDir;   ///< Its folder, with a trailing separator.
  std::string acfFile;  ///< Its file name, for example "a321.acf".
  std::string acfIcao;  ///< ICAO type designator from the .acf.
  std::string uiName;   ///< Name X-Plane shows for the aircraft.
  /// The livery's tail number (registration), for example "D-AIZA".
  std::string tailNumber;
  std::string author;
  std::string studio;
  /// Folder of the selected livery with a trailing separator. Empty for the
  /// default livery.
  std::string liveryPath;
  /// Name of that folder.
  std::string liveryName;
};

}  // namespace xn
