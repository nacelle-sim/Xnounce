/// @file
/// Which Airline Pack is active (VP_SPEC § 1.4).
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/airlines.h"
#include "core/pack_library.h"
#include "core/settings.h"

namespace xn {

/// The pack configuration of a livery: its `airlinepack.cfg`, or a section
/// of `AirlinePacks/liveries.ini` whose glob matches the livery's folder.
struct LiveryConfig {
  std::string airline;
  std::vector<std::string> customTags;
  std::optional<bool> overwater;
  std::string source;
};

/// Reads `airlinepack.cfg`, in any section; the Fenix key names work too.
LiveryConfig liveryConfigFromSidecar(const Ini& sidecar);
/// The section of `liveries.ini` for a livery: the first glob that matches.
LiveryConfig liveryConfigFromCentral(const Ini& central,
                                     const std::string& liveryName);
/// Combines both, key by key; the livery's own file wins.
LiveryConfig mergeLiveryConfig(const LiveryConfig& sidecar,
                               const LiveryConfig& central);

struct ResolveInput {
  std::string liveryName;  ///< Empty for the default livery.
  std::string flightNumber;
  LiveryConfig livery;
  std::string ofpCallsign;  ///< Only while the OFP passes its check.
};

struct Resolution {
  const Pack* pack = nullptr;  ///< Null: no pack fits; nothing plays.
  std::string reason;
};

Resolution resolvePack(const PackLibrary& lib, const AirlineTable& airlines,
                       const ResolveInput& in);

/// The ICAO codes a flight number can mean: "DLH400" gives DLH, "LH400" the
/// ICAO code of IATA LH.
std::vector<std::string> airlineCodesFromFlightNumber(
    const AirlineTable& airlines, const std::string& flightNumber);

}  // namespace xn
