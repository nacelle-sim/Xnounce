# Developing Xnounce

**Scope:** building, testing and day-to-day work on the plugin. The structure is in `ARCHITECTURE.md`, the behaviour
in `VP_SPEC.md`, the aircraft in `aircraft/`.

## 1. What you need
- CMake 3.21 or later, Ninja and a C++17 compiler (GCC, Clang or MSVC). On Linux also the OpenGL headers
  (`libgl1-mesa-dev` or your distribution's equivalent).
- Network access the first time you configure: the X-Plane SDK, Dear ImGui, ImgWindow, libogg, libvorbis and doctest
  are downloaded at pinned versions (`cmake/XnDependencies.cmake`) into `build/_deps`, which every preset shares.
- For formatting, clang-format 23 (CI pins 23.1.4). For the API reference, Doxygen and Graphviz.
- For the `asan` preset with GCC, its sanitizer runtimes (`libasan`, `libubsan` on Fedora). Clang brings its own:
  `cmake --preset asan -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++`.

## 2. Build and test
```
cmake --preset dev            # configure: build/dev, Debug, tests on
cmake --build --preset dev    # the plugin, the tests and xn_replay
ctest --preset dev            # every test
```
| Preset | Folder | For |
|---|---|---|
| `dev` | `build/dev` | everyday work: Debug, all tests |
| `release` | `build/release` | an optimised build like CI's (CI also links the C++ runtime statically on Linux) |
| `asan` | `build/asan` | the tests under AddressSanitizer and UndefinedBehaviorSanitizer |

The plugin lands in `build/<preset>/Xnounce/<platform>/Xnounce.xpl`. To try it in X-Plane, copy the
`build/<preset>/Xnounce` folder into `X-Plane 12/Resources/plugins/`.

### Local test data
Some tests use files that can't be in the repository: community Airline Packs and recorded flights. Keep them in a
folder of your own with this layout and name it in `CMakeUserPresets.json` (git ignores that file):
```
<folder>/packs/     Airline Packs, one folder per airline, as in X-Plane's AirlinePacks
<folder>/traces/    recorded flights (*.xntrace.csv, any depth)
<folder>/golden/    the golden stories of those flights (written by the test, see § 3)
```
```json
{
  "version": 3,
  "configurePresets": [{ "name": "local", "inherits": "dev", "binaryDir": "${sourceDir}/build/dev",
                         "cacheVariables": { "XN_LOCAL_DATA": "/path/to/folder" } }],
  "buildPresets": [{ "name": "local", "configurePreset": "local" }],
  "testPresets": [{ "name": "local", "configurePreset": "local" }]
}
```
Then use `--preset local` instead of `--preset dev`. Without it, the tests that need the data skip, and the CTest
entry `recorded-flights` isn't registered.

## 3. Tests
| CTest name | Program | What |
|---|---|---|
| `unit` | `xn_tests` | doctest unit tests: `tests/core` (packs, selection, the purser's scripted flights, SimBrief, settings), `tests/audio`, `tests/aircraft` (each module's pure parts), `tests/app` (the services, the settings store) |
| `recorded-flights` | `xn_tests` | the recorded flights of the local test data, replayed through the purser |
| `ui` | `xn_ui_tests` | every tab of the window, many frames, both themes, narrow to wide, with ImGui's assertions on |

Run a single test case with `./build/dev/xn_tests --test-case='*cabin call*'`.

**Golden stories:** every scripted flight in `tests/core/test_purser.cpp` and every recorded flight writes a
*story*: each purser decision, each change of its timeline, each call into the (pretend) player, each change of the
stage and of the "coming up" list, and the log lines. The test compares it with the saved story
(`tests/golden/<test name>.story.txt`, `<local data>/golden/<trace>.story.txt`) and reports the first line that
differs. This is how a refactor proves that the behaviour didn't change. After a change of behaviour that was
intended, write the stories again and review their diff before you commit:
```
XN_UPDATE_GOLDEN=1 ./build/dev/xn_tests
git diff tests/golden
```
The stories are compared only with libstdc++ (Linux): the purser's random timings come from the standard library's
distributions, which differ between standard libraries.

**Settings files:** `tests/fixtures/settings-0.3.1-*.ini` were written by the settings code of 0.3.1. The settings
store must read them and write them back unchanged, so a user's settings survive every update.

## 4. Replaying a recorded flight
```
./build/dev/xn_replay Output/Xnounce/20261102-224735.xntrace.csv            # the whole story
./build/dev/xn_replay Output/Xnounce/20261102-224735.xntrace.csv --summary  # decisions and announcements only
```
A trace is what Settings → Advanced → "Record flight trace" writes. `xn_replay` reads traces of every version,
FXAnnounce's included, and runs them through the current purser, so it answers "what would this version have done on
that flight?". Use it for bug reports and to design the triggers of a new aircraft from real flights.

## 5. Code
- **Where code goes:** see the rules in `ARCHITECTURE.md` § 1. In short: anything that names an aircraft's dataref or
  command lives in `src/aircraft/<id>/`; everything that can be pure (no X-Plane) is pure and tested.
- **Comments** are full sentences in Doxygen form: `///` before every file (`@file`), type and public function,
  saying what it is for, with references to the spec written out (VP_SPEC § 4.1a, D-18). Inside functions, comment
  the reason where it isn't obvious.
- **Formatting:** `tools/format.sh` formats every source file; `tools/format.sh --check` is what CI runs.
- **API reference:** `cmake --build --preset dev --target apidocs` (needs Doxygen) writes
  `build/apidocs/html/index.html`. CI uploads it as the `apidocs` artifact of every run.
- **Spec before code:** a change of behaviour starts in `VP_SPEC.md`, a change of structure in `ARCHITECTURE.md`, a new
  dataref in the aircraft's document.
- **Versions:** the version is `project(Xnounce VERSION ...)` in `CMakeLists.txt`. `CHANGELOG.md` has one section per
  version released on GitHub (test builds too), in the words of its release notes; a tag without its section is
  refused.

## 6. Continuous integration
`.github/workflows/build.yml` builds and tests on Linux, Windows and macOS, runs the sanitizers, the format check and
the API reference, and packages a zip on every push (`ARCHITECTURE.md` § 6). Pushing a tag `vx.y.z` makes a draft
release; the owner publishes it.
