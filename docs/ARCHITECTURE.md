# Architecture

**Scope:** how Xnounce is built: the source tree, threads, data flow, the aircraft modules and the crash-safety
rules. Behaviour is specified in `VP_SPEC.md`. Each supported aircraft has its own document in `aircraft/`
(datarefs, encodings, how they were verified). Building, testing and day-to-day work are in `DEVELOPING.md`.

Xnounce runs on the aircraft its modules support and stays dormant on everything else. In 1.0.0 that is the ToLiss
A319, A320 and A321, the Zibo 737-800 and the LevelUp 737-600 to -900ER.

## 1. Source tree
| Folder | What | Depends on |
|---|---|---|
| `src/core/` | Airline Packs (scan, file names, legacy aliases, tags), file selection, which pack is active, settings and the ini format, the SimBrief OFP (check, tracker, boarding schedule), the flight trace recorder and reader, the log, the aircraft type (`aircraft_type.h`: tags, factors, boarding lead, cockpit wording) and the simulator snapshot (`sim_state.h`) | C++17 only |
| `src/core/purser/` | The Virtual Purser: one class, one file per stage of the flight (boarding, departure, the pilots' signals, flight, arrival, the queue and interphone, the labels the window shows) | C++17 only |
| `src/core/cabin_crew/` | The cabin crew's plan for the aircraft (`CabinCrew`: doors and cabin lights from the purser's stage) and the follow-up of each action (`ActionTracker`) (§ 4 rule 4, VP_SPEC § 4.7) | C++17 only |
| `src/aircraft/` | The interface to an aircraft (`module.h`) and to its cabin actions (`actions.h`), the list of modules (`registry.cpp`), the X-Plane values every aircraft shares (`xp_common`), the settings of all modules (`options`), reading traces of any version (`trace_compat`) | `module.h`, `actions.h`, `options`, `trace_compat`: C++17 only. `registry`, `xp_common`: XPLM |
| `src/aircraft/<id>/` | One supported aircraft (§ 8): `toliss/` (ToLiss A319/A320/A321), `zibo737/` (Zibo and LevelUp 737). The pure parts (types, settings, decoding, recognition rules, the cabin crew's decisions) are separate from the module that reads datarefs and runs commands | pure parts: C++17 only. `*_module.cpp`: XPLM |
| `src/app/` | `App` (the plugin's lifecycle, the flight loops, which aircraft is active) and `Commands` (menu, commands, observed aircraft commands); and the services they use: `PackService` (scan worker, livery configuration, active pack, variant), `FlightPlanService` (the OFP through the flight), `SettingsStore` (`Xnounce.ini`), the presenter that fills the window's model | `app`, `commands`: XPLM. The services: C++17 only |
| `src/sim/` | The `DataRef` wrapper, what X-Plane says about the loaded aircraft, the simbrief_hub reader, X-Plane's FMOD Studio found at runtime (`fmod_studio`, § 4 rule 9) | XPLM |
| `src/audio/` | Ogg Vorbis and PCM WAV decoders, decode worker and cache, the closed-door low-pass filter (`muffle`), the PA's sound (`pa_effects`: loudness, the older PA, the cabin acoustics) and its power (`pa_power`), the hearing model with the cabin's ceiling speakers, silencing an aircraft's own FMOD events (`fmod_events`, through a table of functions), the generated call buzzer and chime, playback through XPLM (`player`) | libvorbis; `player`: XPLM |
| `src/ui/` | The window: ImgWindow host, themes, widgets and one file per tab, `tab_*.cpp` (pure ImGui, except `window.cpp` and `fonts.cpp`; § 7) | Dear ImGui, ImgWindow |
| `src/plugin.cpp` | X-Plane's entry points | everything |
| `tools/replay/` | Replaying the purser on a recorded flight: the `xn_replay` command and the golden stories of the tests | C++17 only |
| `tests/` | Unit tests (`core/`, `audio/`, `aircraft/`, `app/`), the headless UI test (`ui/`), golden stories (`golden/`), fixtures, helpers (`support/`) | doctest, Dear ImGui |

Rules, enforced by the build where possible:
- The pure libraries (`xn_core`, `xn_audio`, `xn_aircraft`, `xn_app`, `xn_ui`) are compiled without the X-Plane SDK
  on their include path, so they cannot start to depend on it, and all behaviour can be tested without X-Plane.
- A dataref or command name of an aircraft appears only in that aircraft's folder under `src/aircraft/`. The rest
  of the plugin talks to the aircraft through `AircraftModule` and never names one.
- The tabs in `ui/` never include XPLM, so the headless UI test can draw them.

## 2. Data flow (main thread, two flight loops)
```
X-Plane datarefs ─► XpCommon ─┐
                              ├─► SimState ─► Purser (state machine) ─► queue ─► Player ─► XPLMPlayPCMOnBus
aircraft datarefs ─► module ──┘       │            ▲                               ▲           ▲
                     (AircraftType)   ▼            │ BoardingPlan                  │           │ PCM
sbh/* datarefs ─► SimbriefSource ─► FlightPlanService ─► OFP check, boarding plan  │    decode worker + cache
                                               │ OFP callsign                       │
                     PackService: scan ─► library ─► active pack ─► Selector ─► chosen file
```
- The main flight loop runs at about 4 Hz: the snapshot, the purser and the window's model. A second, light loop
  runs every frame while an aircraft is active and updates only the player (volumes, pause and resume, the PA's
  power cut-out, stopping silenced channels) and the cabin actions' per-frame part (silencing the aircraft's own
  announcements as each starts, § 4 rule 9), so volumes follow the hearing model and the cut-out's fades smoothly.
  Both are scheduled and unscheduled together.
- The window draws every frame from the model. An action from the window or a command marks the model stale, and
  the next frame refreshes it instead of waiting for the main loop; between refreshes the window counts its
  countdowns and the playing position on by the model's age.
- The snapshot is built in two steps: `XpCommon` reads X-Plane's own datarefs (position, speeds, clocks, sun,
  camera, engines, fuel, beacon), then the active module adds what only its aircraft knows (doors, signs, FMS
  phase, power, the interphone and PA reception, and the *cues*: takeoff power, lift-off, touchdown and a phase
  hint, computed from the aircraft's own systems).
- Cabin sounds (PA, music) play as two channels started in the same frame from the same position: the clear voice
  and its *twin*, the muffled copy. With the cabin acoustics on, the clear voice is stereo and the twin stays mono;
  positions are counted in sample frames. Pause, resume, skip and stop act on both; the hearing model gives each its
  own gain (VP_SPEC § 4.5). A channel starts at volume 0 and gets its level on the next frame, and a stopped channel
  is set to 0 and stopped a frame later, so FMOD's volume ramp keeps both from clicking.
- **The camera** is X-Plane's camera position (`view_x/y/z`) taken into the aircraft's axes (`core/camera.h`), so
  views placed by a camera plugin count (VP_SPEC § 4.2). The module gives the cabin's geometry per type
  (`AircraftModule::cabinGeometry`).
- **The aircraft's turnaround service** (VP_SPEC § 4.1c): the module follows it into `SimState::svc`; the purser
  follows that only while the service runs, so aircraft without one are unchanged.
- **Cabin actions** (VP_SPEC § 4.7): after the purser's tick, `CabinCrew` (core, pure) turns the purser's
  `CabinStage` and the snapshot into a `CabinPlan` (doors, cabin lights). The module's `CabinActions` carries out the
  parts the user switched on and reports every action to the log and the trace. The purser never sees the plan, so
  the golden stories don't change with it. The cabin's chime goes the same way when the aircraft sounds its own:
  the player raises a flag instead of playing, and the app hands it to the actions.
- The `SimState` snapshot, the `AircraftType` and the `BoardingPlan` are the only inputs to the purser. Recorded
  traces replay through the same purser (`tools/replay`, the golden stories in `tests/`). The trace reader maps
  columns by their header names, so older traces stay readable when columns are added, and `trace_compat` turns
  the aircraft-specific columns of traces from before 0.3.5 into today's fields.
- **simbrief_hub** (optional, separate plugin) is a read-only peer: Xnounce reads its `sbh/*` datarefs, resolving
  them again every 10 s until they exist, and re-reads the OFP when `sbh/seqno` changes. The origin and destination
  airports are looked up with `XPLMFindNavAid` on the main thread, once per OFP. `OfpTracker` (core, pure) follows the
  OFP through the flight from the aircraft position, and the purser tells the app when the aircraft has taken off.

## 3. Threads
- **Main thread:** all XPLM calls, the purser, the queue, the UI.
- **Workers:**
  - a pack scan worker: filesystem only;
  - a decode worker: file read and Vorbis decode into a PCM buffer, plus its muffled (low-passed) copy for the
    closed-door voice (VP_SPEC § 4.5). Every file is brought to its loudness first (announcements and music each to
    their own). Announcements and music are kept as the airframe's PA copy (VP_SPEC § 4.6): with the Worn or Old
    preset the worker ages the sound, the muffled copy is made from the aged mono sound, and the cabin acoustics turn
    the clear copy into stereo last. One PA copy of a file is kept; the interphone asks for the clean (levelled,
    mono) sound. A stereo PA copy takes twice the memory: a six-minute music track is about 100 MB with its muffled
    copy, within the 192 MB cache. Announcements are queued ahead of music tracks. Changing the cutoff, the preset,
    the airframe, the loudness or the acoustics empties the cache (a decode started before the change is dropped);
    voices that are playing keep their buffer.
  They hand results back through mutex-protected queues that the flight loop drains. They never call XPLM.
- On plugin disable, workers are cancelled and joined before anything they reference is destroyed.

## 4. Crash safety
1. Every function X-Plane calls into (`XPlugin*`, flight loops, menu and command handlers, audio completion
   callbacks, window build) runs inside `xn::guard(name, fn)`.
   - `guard` catches `std::exception` and `...`, logs the fault, and counts it.
   - No exception ever crosses into X-Plane.
2. **Circuit breaker:** after 3 faults the plugin enters *safe mode*. Audio stops, flight loops are unregistered,
   and the window shows a red banner with a Re-enable button. The plugin never takes the sim down with it.
3. **Datarefs** go through `DataRef<T>`:
   - resolved when an aircraft activates and again on every aircraft load;
   - type checked against `XPLMGetDataRefTypes`, array reads bounds checked, strings NUL-terminated.
   - A missing dataref turns off only the features that need it, with one log line.
4. **Read-only toward the aircraft unless the user switched an action on.** Observing aircraft commands is allowed
   with handlers registered before the aircraft's (`inBefore = 1`) that always return 1, so the press reaches the
   aircraft unchanged. They are registered only while the aircraft is active and removed on deactivation. That's how
   the cockpit's cabin call buttons are seen (VP_SPEC § 4.3a). Acting on the aircraft (VP_SPEC § 4.7) goes through
   the module's `CabinActions` (`src/aircraft/actions.h`) and nothing else:
   - only code under `src/aircraft/` writes aircraft datarefs (`DataRef::set`) or runs aircraft commands
     (`XPLMCommandOnce`);
   - only from the main flight loop, on the main thread, and only for an action the user switched on or a check the
     user tried (the Log tab's Aircraft check);
   - one command or write per item per tick; each is checked against the aircraft's reading after its settle time
     (`ActionTracker`); a failure is logged once and the item is left until the plan for it changes; an item the
     user changes afterwards is the user's until then;
   - an aircraft option the plugin changed (the 737's crew announcement switches) is remembered and put back when
     the action is switched off, when the plugin goes dormant or into safe mode, and when it is disabled. It is not
     put back when the aircraft is unloading or reloading (`deactivate(…, aircraftGone)`): its handles are stale and
     the aircraft starts with its own values anyway. A value the user changes while it is held is left as the user
     set it.

   The Settings tab writes Xnounce's own settings only.
5. **Untrusted inputs** (pack files):
   - the size, duration, sample rate and channel count are validated before decoding;
   - decode errors are logged once and the file is marked bad;
   - filesystem calls use the `std::error_code` overloads.
6. **Audio buffers** are owned by the player until X-Plane's completion callback for that channel arrives (it can
   arrive inside `XPLMStopAudio`). Buffers of stopped channels are parked until then. The callback looks its voice
   up by id; it never dereferences a stale pointer. A twin channel plays from its parent's `PcmBuffer` (the muffled
   copy lives in the same buffer) and is parked under its own id, so either completion can arrive first. Only the
   parent's completion ends an announcement.
7. **Nothing calls into X-Plane from static destructors.** Everything that owns XPLM resources is released in
   XPluginDisable: windows, ImgWindow's shared font atlas and textures (`ui::shutdownUi`), audio channels, flight
   loops, the menu and command handlers. `App::~App` only stops threads and deliberately leaks what would call XPLM.
8. `.xpl` linkage:
   - hidden visibility, only `XPlugin*` exported (`cmake/exports.map`);
   - static libstdc++ on Linux release builds;
   - all third-party code compiled into the plugin.
9. **X-Plane's FMOD Studio** (VP_SPEC § 4.7, D-32): silencing an aircraft's own announcements goes through
   X-Plane's FMOD Studio system. No FMOD header or library is part of the build: `src/sim/fmod_studio` finds
   `XPLMGetFMODStudio` with `XPLMFindSymbol` (absent on an X-Plane without it) and four FMOD Studio C functions in
   the library X-Plane has already loaded (`dlsym`/`dlopen` with `RTLD_NOLOAD`, `GetModuleHandle` on Windows); it
   loads nothing. `FmodEvents` (pure, `src/audio/`) calls them through that table with opaque handles: main thread
   only, every frame from the audio loop, as a cabin action of the module that names the events. FMOD validates
   its handles, so a stale one returns an error instead of crashing; a void handle (the banks rebuilt) is looked
   up again, other errors twenty times in a row turn the feature off for the session, and
   `XPLM_MSG_FMOD_BANK_UNLOADING` drops every handle. Only volumes are set, and they are given back when the action
   is switched off or the plugin lets go.

## 5. Files at runtime
| Path | What |
|---|---|
| `Resources/plugins/Xnounce/{lin,win,mac}_x64/Xnounce.xpl` | the plugin, signature `nacelle.xnounce` |
| `Resources/plugins/AirlinePacks/` | packs (user-installed), plus optional `liveries.ini` and `airlines.tsv`. The release zip ships it empty except for `PUT_YOUR_PACKS_HERE.txt`; only sub-folders are scanned, so loose files are ignored |
| `<livery>/airlinepack.cfg` | optional per-livery mapping |
| `Output/preferences/Xnounce.ini` | settings (written by the UI) |
| `Output/Xnounce.log` | the plugin's own log (rotated at 1 MB, one backup). `Log.txt` gets one-line summaries |
| `Output/Xnounce/*.xntrace.csv` | recorder output (when enabled); `xn_replay` reads it |
| `Resources/plugins/simbrief_hub/` | optional companion plugin (not ours); Xnounce only reads its datarefs |

### 5.1 Upgrading from FXAnnounce
Xnounce was called FXAnnounce up to 0.3.1 (plugin folder `FXAnnounce`, signature `yosh.fxannounce`). From 0.3.5:

- **Settings:** when `Output/preferences/Xnounce.ini` doesn't exist and `FXAnnounce.ini` does, the old file is read
  once and the settings are saved to `Xnounce.ini`. The old file is never changed or deleted. Every ini key is
  unchanged, so a 0.3.1 file loads and saves to the same values (`tests/fixtures/settings-0.3.1-*.ini`).
- **Commands:** every `xnounce/...` command is also registered under its old `fxannounce/...` name, so existing key
  and joystick bindings keep working. New bindings should use `xnounce/...`.
- **Old plugin still installed:** if a plugin with the signature `yosh.fxannounce` is enabled, both would play every
  announcement. Xnounce then stays dormant (no purser, no audio), Status shows an OLD VERSION INSTALLED card and
  `Log.txt` gets one line telling the user to delete `Resources/plugins/FXAnnounce`. The check runs with aircraft
  detection, so it clears on the next aircraft load after the old plugin is gone.
- The old log (`Output/FXAnnounce.log`) and traces (`Output/FXAnnounce/*.fxtrace.csv`) are left where they are;
  `xn_replay` reads the old traces as well.

## 6. Build, CI and releases
CMake builds everything from the repository root; the presets in `CMakePresets.json` (`dev`, `release`, `asan`)
and CTest are the everyday commands (`DEVELOPING.md`). Third-party code is fetched at configure time at pinned
versions (`cmake/XnDependencies.cmake`) and compiled in.

`.github/workflows/build.yml` runs on every push, pull request and tag.

| Job | Runner | Does |
|---|---|---|
| `linux` | ubuntu-22.04 (oldest glibc we support) | Release build with static libstdc++, all tests through CTest |
| `windows` | windows-2022 | MSVC Release build, all tests through CTest |
| `macos` | macos-15 | universal (x86_64 + arm64) Release build, checked with `lipo`, then all tests through CTest |
| `sanitizers` | ubuntu-24.04 | the tests again under AddressSanitizer and UndefinedBehaviorSanitizer (`asan` preset) |
| `format` | ubuntu-24.04 | `tools/format.sh --check` with clang-format 23 |
| `apidocs` | ubuntu-24.04 | the API reference from the source comments (Doxygen), uploaded as the `apidocs` artifact |
| `package` | ubuntu-22.04 | after the three builds: `tools/package.sh` assembles the install layout below. It is uploaded as the `Xnounce` artifact on every run, so testers can grab any build |
| `release` | ubuntu-22.04 | tags only: creates a **draft** GitHub release with the zip, its checksum and the notes. The owner publishes it |

- **Test gate:** a release needs every job green, and the tests run on all three platforms. `xn_ui_tests` keeps
  assertions on in Release builds on every compiler (`-UNDEBUG`, also under MSVC). `tests/ui/test_ui.cpp` refuses to
  compile if `NDEBUG` is still defined, so the gate can't silently test nothing. The golden stories are compared
  on Linux only, because the purser's random timings come from the standard library's distributions, which differ
  between standard libraries.
- **Install layout** (`Xnounce-<version>.zip`). The user copies both folders into `X-Plane 12/Resources/plugins/`:
  ```
  Xnounce/{lin_x64,win_x64,mac_x64}/Xnounce.xpl
  Xnounce/README.md, Xnounce/LICENSE, Xnounce/licenses/B612-OFL.txt
  AirlinePacks/PUT_YOUR_PACKS_HERE.txt
  ```
  The plugin is self-contained: fonts and the airline table are compiled in, so nothing else ships except the
  font licence (B612, SIL OFL 1.1, which must travel with the font).
- **Cutting a release:** bump `project(Xnounce VERSION x.y.z)` in `CMakeLists.txt`, add a `## x.y.z` section to
  `CHANGELOG.md`, commit, then push the tag `vx.y.z`. `package` fails if the tag isn't `v` + the CMake version or if
  the CHANGELOG section is missing. The release notes are that section followed by install steps, the zip's SHA-256
  and a link to the tagged source (GPL-3.0). Re-running the workflow on the same tag replaces that tag's draft; a
  published release is never changed (the job fails instead, so bump the version).
- **macOS signing:** the Mac build is not notarized; only the linker's ad-hoc signature is present. The notes and the
  README give the `xattr` command that clears the download quarantine.

## 7. User interface
The window is one ImgWindow (`ui/window.cpp`) that draws `ui::drawPanel` from a `UiModel` the app fills each
frame (`app/ui_presenter.cpp`). The tabs only read the model and call its `std::function` actions, so the headless
UI test draws every one of them without X-Plane. Wording that depends on the aircraft (how to answer the cabin's
call, which aircraft are supported) comes from the aircraft module through the model.

- **Any aircraft:** the menu (Show/hide window, Rescan) and the window exist on every aircraft. When Xnounce is
  dormant, Status shows an UNSUPPORTED AIRCRAFT card with the loaded aircraft's name (and, for an aircraft a module
  recognises but doesn't support yet, why) and the list of supported aircraft from the registry. Packs and Settings
  work, and no flight loop runs except detection. Leaving a supported aircraft hides the window; it reopens on the
  next supported aircraft if it was open.
- **Titlebar row:** name, version, "by Nacelle" and a state chip (`ACTIVE`, `MUTED`, `SAFE MODE`, `UNSUPPORTED`,
  `OLD VERSION`). On the right: the `Timeline` checkbox (`[ui] show_timeline`) and the `White / Dark` switch
  (`[ui] theme`), saved at once.
- **Tabs and their ids** (`[ui] tab`, stable across versions): Status 0, Timeline 1 (only with `show_timeline`),
  Packs 2, Settings 3, Log 5 (only with `[ui] show_log`). Id 4 was the Inspector (removed in 0.3.0). A saved id
  that is hidden or unknown opens Status.
- **Themes:** `ui/theme.cpp` holds one palette per theme ("Dark": navy page, Delta blue `#003268` and red
  `#9B1631` chrome; "White": white page, Emirates red `#D71A21` chrome). Every ImGui colour comes from the palette.
  Status colours follow the ECAM convention (green normal, amber caution, red only for errors and safe mode);
  `[ui] color_blind` swaps them for Okabe-Ito colours. Important content sits in *cards*: black rounded panels with
  a coloured top rule, the same in both themes.
- **Fonts:** B612 and B612 Mono (Airbus/Intactile, SIL OFL 1.1), compiled in as compressed arrays
  (`fonts/*.inc`, made with Dear ImGui's `binary_to_compressed_c`). The licence ships in the zip.
- **Status** is one page in bands, all cards: Cabin (stage, what plays, boarding progress: modeled time, not a
  passenger flow) and Flight (airline name from the airline table, callsign, a route bar from the great-circle
  distance between the SimBrief airports and the aircraft, ETD and boarding time) side by side; then Recent and
  Coming up, which stretch down to the Aircraft card (livery, pack) at the bottom and show as many rows as fit (at
  least 1 and 2, at most 8). Text in the cards wraps; the rows are measured wrapped. Narrow windows stack the cards
  (and show 3 rows).
- **Names:** announcements are shown in plain English (`annTypeLabel`); the Fenix name is in the tooltip.
- **Settings tab:** a sidebar of pages (`SettingDef::page`: Announcements, Sound, Boarding, Timing, the aircraft page,
  Display, Advanced); the ini sections don't depend on it. The aircraft page is named after the loaded aircraft's
  module ("ToLiss") and shows that module's settings; while dormant it lists every module's settings under its name.
  Changes apply at once: a checkbox or combo on click, a slider or text field when it is released
  (`onSettingsChanged`: apply, save the ini, rescan if the pack folders changed).
- **Log tab:** the last 400 entries of `Log::recent()`. An entry that repeats an earlier one (same level and text)
  replaces it at the bottom with a count, so a repeating line takes one row. `Xnounce.log` keeps every line. The
  Live inputs table shows the snapshot; the aircraft module adds its own rows (`AircraftModule::liveInputs`).

## 8. Aircraft modules
An aircraft module implements `AircraftModule` (`src/aircraft/module.h`). `docs/ADDING_AN_AIRCRAFT.md` walks through
writing one.

- **Recognition:** when an aircraft or livery loads, the app offers what X-Plane says about it (`LoadedAircraft`:
  .acf path, ICAO, author, studio, livery) to each module in registry order, for up to 12 s, because some aircraft
  register their datarefs a few seconds after loading. The first module that recognises it takes it. A module can
  recognise an aircraft without supporting it; the plugin then stays dormant and says why.
- **Activation:** the module resolves its datarefs (missing ones are logged once and only their features are off),
  reads the aircraft's own files, and lists the commands to observe. The app registers the observers and starts the
  purser.
- **Each tick:** `read` adds the aircraft's values and cues to the snapshot `XpCommon` started; `type` returns the
  aircraft type, which may change at runtime (the ToLiss engine option switches A321 and A21N).
- **Cabin actions:** `actions()` returns the module's `CabinActions`, or null for an aircraft Xnounce only reads
  (the ToLiss). It works out at activation which actions the aircraft allows (commands present, datarefs
  writable), carries out the `CabinPlan` each tick, puts back what it changed (rule 4 of § 4), and lists its checks
  for the Log tab's *Aircraft check* (each with a Try button the flight loop carries out, showing the reading before
  and after). The aircraft-specific decisions (which command for which door, the light encoding) are pure code in
  the module's folder so the tests can run them.
- **Settings:** each module owns its settings struct and declares them with the same `SettingDef` builders as the
  shared settings (`aircraft/options.h`). Their ini sections and keys never change, so old settings files keep
  working.
- **Traces:** the trace names the module in its `aircraft` column. A trace from before 0.3.5 is upgraded through
  the ToLiss module's own decoders (`aircraft/trace_compat.cpp`).

| Module | Folder | Aircraft | Status |
|---|---|---|---|
| ToLiss | `src/aircraft/toliss/` | ToLiss A319, A320, A321 (CEO and NEO); A330-900 and A340-600 recognised, dormant | supported; `aircraft/toliss.md` |
| 737 | `src/aircraft/zibo737/` | Zibo 737-800, LevelUp 737-600/700/800/900/900ER | supported (1.0.0); `aircraft/zibo737.md` |
