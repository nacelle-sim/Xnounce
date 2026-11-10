/// @file
/// What the aircraft-neutral code needs to know about the aircraft being
/// flown.
///
/// An aircraft module (`src/aircraft/<id>/`) builds one `AircraftType` for the
/// loaded aircraft. Nothing in `core/` knows which aircraft exist.
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "core/cockpit_terms.h"

namespace xn {

/// How long before the departure time boarding starts on one aircraft type
/// (VP_SPEC § 4.1a, D-19).
struct BoardingLead {
  float minMin = 30;  ///< Minutes before the ETD with an empty cabin.
  float maxMin = 37;  ///< Minutes before the ETD with a full cabin.
  int seats = 180;    ///< Seats assumed when the flight plan gives none.
};

/// The aircraft being flown, as the purser and the file selection see it.
struct AircraftType {
  /// Short stable name of the type, for example "a320".
  std::string id = "generic";
  /// Name shown to the user, for example "A321neo".
  std::string label = "unknown";
  /// Aircraft tags of Airline Pack files this aircraft may play, family tag
  /// first, for example {"A321", "A21N"} (VP_SPEC § 3.1).
  std::vector<std::string> tags;
  /// The most specific of `tags`. A file carrying it beats a file with only
  /// the family tag (VP_SPEC § 3.3, D-4).
  std::string specific;
  /// Every ICAO type designator of this aircraft family, for comparing with
  /// the aircraft a flight plan was made for.
  std::vector<std::string> designators;
  /// Scales the cabin check, boarding and deboarding times with the size of
  /// the cabin (VP_SPEC § 4.1).
  float typeFactor = 1.0f;
  /// When boarding starts before a scheduled departure.
  BoardingLead lead;
  /// Cockpit wording for messages.
  CockpitTerms terms;
};

/// True when `tag` looks like an ICAO aircraft type designator ("A21N",
/// "B738", "E75L"). Airline Packs may tag files for any aircraft, so any
/// designator counts as an aircraft tag (VP_SPEC § 3.1, D-4).
bool isAircraftTypeTag(std::string_view tag);

}  // namespace xn
