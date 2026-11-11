# Adding an aircraft

**Scope:** the steps to support a new aircraft, using the ToLiss module (`src/aircraft/toliss/`) as the example. The
purser, the packs and the window don't change: an aircraft module turns the aircraft's own datarefs into the neutral
snapshot the purser reads (`ARCHITECTURE.md` § 8).

## 1. Document the aircraft first
Create `docs/aircraft/<id>.md` (`<id>` is a short lower-case name such as `toliss` or `zibo737`):
- the models, their ICAO designators, and what identifies them (.acf author and studio, a dataref only this
  aircraft registers);
- for each signal of VP_SPEC § 4.2, the dataref or command that provides it and its encoding, each marked *assumed*
  until a flight with the recorder on confirms it;
- the commands of the cockpit's cabin call buttons;
- anything the aircraft does by itself that Xnounce must respect (its own announcements, doors that close on a
  timer).

Read the aircraft's files (its datarefs list, scripts, plugin strings) to find the names; `zibo737.md` is an example.

## 2. The pure parts
In `src/aircraft/<id>/`, everything that needs no X-Plane, so it can be tested:
- **Recognition** (`<id>_identity` or `<id>_types`): a function that takes the `LoadedAircraft` and whether the
  marker dataref exists and returns a `Recognition`: `ours`, `supported`, a model name for the log and, for a model
  that is recognised but not supported, the dormant reason, the `Log.txt` line and the note for the Status card.
- **Types** (`<id>_types`): the `AircraftType` of each model: `id`, `label`, the aircraft tags with the most specific
  last, the ICAO designators, the type factor, the boarding lead and default seats, and the `CockpitTerms` (the words
  the purser uses for the FMS, answering the interphone, the cabin call buttons and the cues).
- **Signal decoding** (`<id>_signals`): functions from raw dataref values to the snapshot's fields and cues, for
  anything beyond a plain read (an array index, a switch position, a phase table).
- **Settings** (`<id>_options`): a struct with the aircraft's own settings, their `SettingDef`s (built with the
  `setting::` builders; put them on `SettingPage::Aircraft` and set `group`/`groupName`), and `normalize`/`migrate`
  if needed. Add the struct to `AircraftOptions` and its four functions in `src/aircraft/options.{h,cpp}`. A key, once
  released, never changes.

Add these files to `xn_aircraft` in `CMakeLists.txt`.

## 3. The module
`src/aircraft/<id>/<id>_module.{h,cpp}` implements `AircraftModule` (`src/aircraft/module.h`):
- `id`, `name` (shown as the Settings page name), `supportedAircraft` and `loadHint` (the phrases of the Status card
  and the Timeline; empty while nothing is supported);
- `recognise`: look for the marker dataref, call the pure recognition;
- `activate`: resolve every dataref (`DataRef` from `src/sim/dataref.h`), return the names of the missing ones, read
  the aircraft's own files; `deactivate`, `reloadFiles`;
- `read`: fill the aircraft's part of the `SimState` (the shared part is already there from `XpCommon`). Leave a
  signal the aircraft can't provide at its default ("unknown");
- `type`: the current `AircraftType`, from the pure type table;
- `observedCommands`: the cabin call buttons (`cabinCall = true`) and other commands worth logging;
- `rawSnapshot`: the aircraft's raw values for the trace (leave out values that drift);
- `liveInputs`: extra rows for the Log tab, each placed after a named row;
- `chimeSound` (optional): an X-Plane sound file for the cabin's call to the cockpit (a PCM WAV under X-Plane's
  folder); the default is the generated Airbus buzzer;
- the AC source (optional): set `SimState::acSource` to an id of what feeds the cabin's AC buses, so the PA drops
  out on a power transfer (VP_SPEC § 4.6). Leave it at -1 when the aircraft doesn't say.

Scripted flights: like `tests/aircraft/test_zibo737_flights.cpp`, a fixture that sets the aircraft's raw values and
decodes them each tick with the module's pure signal code, so the golden stories show the purser on that aircraft.

The module is read-only toward the aircraft: no dataref writes, no commands run (`ARCHITECTURE.md` § 4).
Add the module to the plugin target in `CMakeLists.txt` and one line to `makeAircraftModules` in
`src/aircraft/registry.cpp`.

## 4. Tests
`tests/aircraft/test_<id>.cpp`, added to `xn_tests`:
- recognition with the real identity strings of every model, and with the aircraft it could be mistaken for;
- the type table (tags, most specific, factors, leads);
- every decoder and cue with the values seen in a flight.

Fly the aircraft with the recorder on, then replay the trace with `xn_replay` and check the story. Once the
encodings are verified, put the trace in your local test data, so the `recorded-flights` test guards it.

## 5. Acceptance and release
Add the aircraft's rows to `docs/ACCEPTANCE.md` (the owner runs them in X-Plane), its name to the README, and the
user-facing change to `CHANGELOG.md`.
