/// @file
/// The Zibo and LevelUp 737 models: what Xnounce needs to know about each
/// (VP_SPEC § 3.1, docs/aircraft/zibo737.md).
#pragma once

#include <string_view>

#include "aircraft/zibo737/zibo737_options.h"
#include "core/aircraft_type.h"
#include "core/sim_state.h"

namespace xn::zibo737 {

/// The 737NG models of the two aircraft.
enum class Model { Unknown, B736, B737, B738, B739, B739ER };

/// The model from the ICAO designator and the .acf file name. LevelUp's
/// -900ER is `737_9ENG.acf`; it shares B739 with the -900.
Model modelFrom(std::string_view acfIcao, std::string_view acfFile);
/// "737-800" and so on; empty for Unknown.
const char* modelName(Model m);

/// The passenger cabin of a model, aft of the pilot's eye (VP_SPEC § 4.5):
/// the -800's from the LevelUp's cabin sound space, the others scaled by
/// their fuselage length.
CabinGeometry cabinOf(Model m, const Options& o);

/// The words of the 737 cockpit.
const CockpitTerms& terms();
/// Type tags, factors, boarding lead and wording of one model.
/// @param cabinSeats The seats of the aircraft's cabin layout (the sum of
///        `laminar/B738/std_pax_num`), or 0 when unknown: then the setting's
///        seats are used.
AircraftType makeType(Model m, const Options& o, int cabinSeats = 0);

}  // namespace xn::zibo737
