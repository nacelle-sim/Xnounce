# Xnounce: instructions for AI agents

Xnounce is a GPL-3.0 native X-Plane 12 plugin by Nacelle that plays Fenix-format *Airline Pack* cabin announcements,
driven automatically by the flight (a "Virtual Purser"). It supports the aircraft its modules in `src/aircraft/`
support (the ToLiss A319/A320/A321, the Zibo 737-800 and the LevelUp 737-600 to -900ER). Agents write the code and
docs. The owner tests in X-Plane, reviews, commits and publishes.

1. **The Fenix guides are the reference.** `docs/VP_SPEC.md` restates them and maps them onto the aircraft signals;
   `docs/aircraft/<id>.md` says how each aircraft provides those signals. Where the spec and Fenix disagree, Fenix
   wins unless the difference is a listed deviation (D-n). New deviations need the owner's approval.
2. **Spec before code.** Change `docs/VP_SPEC.md` (behaviour) or `docs/ARCHITECTURE.md` (structure) first, then the
   code, in the same session. Datarefs and their verified encodings go in `docs/aircraft/<id>.md`.
3. **Crash safety is a hard rule** (`docs/ARCHITECTURE.md` § 4): every XPLM callback goes through `xn::guard`, no XPLM
   calls off the main thread, no writes to aircraft datarefs and no aircraft commands run, `src/core/` and the other
   pure libraries stay free of XPLM and ImGui (the build enforces it).
4. **Aircraft code only under `src/aircraft/<id>/`.** Nothing else names an aircraft's dataref or command. A new
   aircraft follows `docs/ADDING_AN_AIRCRAFT.md`.
5. **Behaviour is guarded by golden stories** (`docs/DEVELOPING.md` § 3). A refactor must leave them identical. Only
   rewrite them (`XN_UPDATE_GOLDEN=1`) for a change of behaviour the owner asked for, and show the diff.
6. **Versioning:** semver in `CMakeLists.txt` (`project(Xnounce VERSION ...)`). Bump it with every change under `src/`
   or `data/`. `CHANGELOG.md` has a `## x.y.z` section for every version the owner releases on GitHub, test builds
   included (the owner tests only from GitHub releases), in user-facing words: it becomes the release notes, and a
   tag without its section is refused. Releases come from pushing tag `vx.y.z`, or `vx.y.z-rc.N` for a release
   candidate (a prerelease that uses the `x.y.z` section; 1.0.0 is tested this way) (`docs/ARCHITECTURE.md` § 6);
   the owner tags and publishes, never an agent.
7. **Build and tests:** `cmake --preset dev && cmake --build --preset dev && ctest --preset dev`. Format with
   `tools/format.sh`. Comments are full sentences in Doxygen form (`docs/DEVELOPING.md` § 5).
8. **Local data stays local.** Airline Packs, recorded flights and other third-party or personal files live outside
   the repository (`XN_LOCAL_DATA`, `docs/DEVELOPING.md` § 2). Never commit them, and never point the repository's
   files at a local folder.
9. **Never write into the X-Plane install** (`/mnt/data/X-Plane 12`) or an aircraft's folder without the owner's
   explicit OK. Read-only inspection is fine.
10. **Never commit, tag or push.** The owner does.
11. In-sim acceptance (`docs/ACCEPTANCE.md`) is the owner's job. Never mark it passed yourself.
