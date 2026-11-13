/// @file
/// The interface between Xnounce and one supported aircraft.
///
/// Everything that knows an aircraft's datarefs, commands, cockpit wording or
/// type table lives in that aircraft's folder under `src/aircraft/`. The rest
/// of the plugin talks to it through `AircraftModule` and never names an
/// aircraft. docs/ADDING_AN_AIRCRAFT.md walks through writing a module.
///
/// This header needs no X-Plane SDK. A module's implementation does.
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "aircraft/actions.h"
#include "aircraft/loaded_aircraft.h"
#include "core/aircraft_type.h"
#include "core/sim_state.h"

namespace xn {

/// A module's answer to "is the loaded aircraft yours?".
struct Recognition {
  /// The aircraft belongs to this module.
  bool ours = false;
  /// The purser can run on it. False for a model the module knows but does
  /// not support yet.
  bool supported = false;
  /// Short name of the model for the log, for example "A320".
  std::string model;
  /// For an aircraft that is ours but not supported: why the plugin stays
  /// dormant ("not a supported ToLiss model"), the line for `Log.txt`, and
  /// what to append to the aircraft's name on the Status card.
  std::string dormantReason;
  std::string logLine;
  std::string cardNote;
};

/// A command of the aircraft that the plugin watches without changing it
/// (docs/ARCHITECTURE.md § 4, rule 4).
struct ObservedCommand {
  std::string name;
  /// True when a press is a cabin call (VP_SPEC § 4.3a). Other commands are
  /// only logged.
  bool cabinCall = false;
};

/// A row an aircraft adds to the Log tab's "Live inputs" table.
struct LiveRow {
  std::string label;
  std::string value;
  /// Label of the row this one goes below. Unknown or empty: at the end.
  std::string after;
};

/// One supported aircraft, or family of aircraft that share their systems.
///
/// All methods are called on X-Plane's main thread.
class AircraftModule {
 public:
  virtual ~AircraftModule() = default;

  /// Stable short name: the settings group and the `aircraft` column of a
  /// trace. Lower case, no spaces.
  virtual const char* id() const = 0;
  /// Name shown to the user, for example "ToLiss".
  virtual const char* name() const = 0;
  /// The aircraft this module supports, as the Status card words it: "the
  /// ToLiss A319, A320 and A321". Empty while it supports none.
  virtual const char* supportedAircraft() const = 0;
  /// The same list as a request: "a ToLiss A319, A320 or A321".
  virtual const char* loadHint() const = 0;

  /// Decides whether the loaded aircraft belongs to this module. It may look
  /// for the aircraft's datarefs, which some aircraft register a few seconds
  /// after loading, so the plugin asks repeatedly for a while.
  virtual Recognition recognise(const LoadedAircraft& aircraft) = 0;

  /// The aircraft was recognised and is supported: find its datarefs and read
  /// its files. Returns the names of datarefs that are missing; the features
  /// that need them stay off.
  virtual std::vector<std::string> activate(const LoadedAircraft& aircraft) = 0;
  /// The aircraft is gone or the plugin is going dormant.
  virtual void deactivate() = 0;
  /// The user asked for a rescan: read the aircraft's own files again.
  virtual void reloadFiles() {}

  /// The cabin crew's actions in this aircraft (VP_SPEC § 4.7), or null when
  /// Xnounce only reads it. Valid while the module is active.
  virtual CabinActions* actions() { return nullptr; }

  /// The sound of the cabin's call to the cockpit: a file under X-Plane's
  /// folder, for example "Resources/sounds/alert/seatbelt.wav" (a PCM WAV).
  /// Empty: the generated Airbus buzzer.
  virtual std::string chimeSound() const { return {}; }

  /// How far aft of the pilot's eye point the camera must be to count as
  /// "in the cabin", in metres (VP_SPEC § 4.5).
  virtual float cabinViewAftM() const = 0;
  /// Adds what only this aircraft knows to a snapshot that already holds the
  /// standard X-Plane values.
  virtual void read(SimState& s) = 0;
  /// The type being flown: tags, factors, boarding lead, wording. Called
  /// every tick, because an option of the aircraft or a setting can change
  /// it.
  virtual AircraftType type(const SimState& s) const = 0;

  /// The cockpit's cabin call buttons and other commands worth logging.
  virtual std::vector<ObservedCommand> observedCommands() const = 0;
  /// The aircraft's own dataref values as text, for the trace. The recorder
  /// logs each one when it changes, so leave out values that drift.
  virtual std::vector<std::pair<std::string, std::string>> rawSnapshot()
      const = 0;
  /// Rows for the Log tab's "Live inputs" table.
  virtual std::vector<LiveRow> liveInputs(const SimState& s) const = 0;
};

}  // namespace xn
