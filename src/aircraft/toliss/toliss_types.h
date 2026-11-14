/// @file
/// The ToLiss Airbus models: how one is recognised and what Xnounce needs to
/// know about each (VP_SPEC § 3.1, docs/aircraft/toliss.md).
#pragma once

#include <string>
#include <string_view>

#include "aircraft/module.h"
#include "aircraft/toliss/toliss_options.h"

namespace xn::toliss {

/// The Airbus families ToLiss makes. Xnounce runs on the A319, A320 and A321;
/// the A330 and A340 are recognised and stay dormant.
enum class Family { Unknown, A319, A320, A321, A330, A340 };

const char* familyName(Family f);
/// The family from the ICAO type designator, or failing that from the .acf
/// file name.
Family familyFrom(std::string_view acfIcao, std::string_view acfFile);
/// Reads a ToLiss engine code: 1 for a NEO engine (PW..., LEA...), 0 for a
/// CEO engine (CFM..., IAE...), -1 when it is neither.
int engineIsNeo(std::string_view engineCode);

/// The words of the Airbus cockpit.
const CockpitTerms& terms();
/// Type tags, factors, boarding lead and wording of one model.
AircraftType makeType(Family f, bool neo, const Options& o);
/// The passenger cabin of a family, aft of the pilot's eye (VP_SPEC § 4.5),
/// from Airbus's published cabin lengths.
CabinGeometry cabinOf(Family f, const Options& o);

/// What decides which type is being flown right now.
struct TypeInputs {
  /// ICAO designator X-Plane reports now. ToLiss changes it when the engine
  /// option changes (A321 <-> A21N).
  std::string liveIcao;
  /// ICAO designator and file name from when the aircraft loaded.
  std::string loadedIcao;
  std::string acfFile;
  /// Livery folder name. ToLiss liveries start with an engine code in square
  /// brackets, for example "[LEAST] Lufthansa".
  std::string liveryName;
  /// `eng_type` from the livery's `livery.tlscfg`, or empty.
  std::string tlscfgEngine;
};
/// The type being flown. NEO or CEO comes from the live ICAO designator, then
/// the livery's `livery.tlscfg`, then the engine code in the livery name.
AircraftType currentType(const TypeInputs& in, const Options& o);

/// Decides whether the loaded aircraft is a ToLiss. `markerPresent` says
/// whether a dataref that only the ToLiss systems plugin registers exists.
Recognition classify(const LoadedAircraft& aircraft, bool markerPresent);

}  // namespace xn::toliss
