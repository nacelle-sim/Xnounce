/// @file
/// Fills the window's model from the state of the plugin.
///
/// The window draws a `ui::UiModel` and knows nothing else
/// (docs/ARCHITECTURE.md § 7). These functions turn the purser, the packs,
/// the flight plan and the simulator snapshot into the strings and numbers
/// of that model. They need no X-Plane, so they can be tested.
#pragma once

#include <string>
#include <vector>

#include "aircraft/module.h"
#include "app/flight_plan_service.h"
#include "app/pack_service.h"
#include "core/purser/purser.h"
#include "core/selector.h"
#include "core/settings.h"
#include "core/sim_state.h"
#include "ui/model.h"

namespace xn {

/// What plays right now, as the player reports it.
struct NowPlayingView {
  std::string file;  ///< Empty when nothing plays.
  double pos = 0, dur = 0;
  bool decoding = false, paused = false, interphone = false;
  /// The music track, or empty.
  std::string music;
};

/// Everything the Status, Timeline, Packs and Log tabs show while the purser
/// runs.
struct ActiveView {
  ActiveView(const Settings& settings, const SimState& sim,
             const AircraftType& type, const PackService& packs,
             const FlightPlanService& plans)
      : settings(settings), sim(sim), type(type), packs(packs), plans(plans) {}

  const Settings& settings;
  const SimState& sim;
  const AircraftType& type;
  const PackService& packs;
  const FlightPlanService& plans;
  /// The aircraft module's name, for example "ToLiss".
  std::string aircraftName;
  std::string liveryName;
  /// Null only between plugin enable and the first tick.
  const Purser* purser = nullptr;
  NowPlayingView now;
  bool refueling = false;
  bool overwater = false;
  /// The aircraft module's rows for the Live inputs table.
  std::vector<LiveRow> aircraftRows;
  /// The cabin crew's actions on this aircraft, the note about the aircraft's
  /// own announcements, and the Aircraft check rows (VP_SPEC § 4.7). Empty
  /// for an aircraft without actions.
  std::vector<ActionStatus> actions;
  std::string announcementNote;
  std::vector<CheckRow> checks;
  /// Path of the trace being recorded, or empty.
  std::string recorderPath;
};

/// Fills the parts of the model that describe a running flight.
void presentActive(ui::UiModel& m, const ActiveView& v);
/// Clears those parts and names the loaded aircraft for the "unsupported
/// aircraft" card. `cardNote` is appended to the name (`Recognition`).
void presentDormant(ui::UiModel& m, const LoadedAircraft& aircraft,
                    const std::string& cardNote);

/// The Flight card: the plan, where the aircraft is on its route and what
/// the plan is used for (VP_SPEC § 4.2a).
/// @param boardingNote The purser's note about a shortened boarding or a
///        clock mismatch. While it is shown, the countdown is not.
void fillFlight(ui::UiModel::Flight& f, const FlightPlanService& plans,
                const AirlineTable& airlines, const SimState& sim,
                const Settings& settings, const std::string& boardingNote);

/// The Packs tab's explainer: which file of `pack` would play for an
/// announcement now, and why each other file would not.
/// @param annType An `AnnType` value.
/// @param callContext A `CallContext` value.
std::vector<std::string> explainSelection(const Pack* pack, int annType,
                                          int callContext, SelectContext c);

}  // namespace xn
