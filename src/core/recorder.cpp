/// @file
/// Implements `src/core/recorder.h`; the interface is documented there.
#include "core/recorder.h"

#include <cstdlib>
#include <filesystem>
#include <functional>

namespace xn {

namespace {

/// One column of a state row: its name, how a `SimState` value is written
/// and how it is read back. The writer and the reader both walk the same
/// table, so they can't drift apart.
struct Column {
  const char* name;
  /// The name older versions used for this column, or null.
  const char* oldName;
  std::function<std::string(const SimState&)> put;
  std::function<void(SimState&, const std::string&)> get;
};

std::string num(const char* fmt, double v) {
  char buf[48];
  std::snprintf(buf, sizeof(buf), fmt, v);
  return buf;
}

Column flt(const char* name, float SimState::* m, const char* fmt,
           const char* oldName = nullptr) {
  return {name, oldName,
          [m, fmt](const SimState& s) {
            return num(fmt, static_cast<double>(s.*m));
          },
          [m](SimState& s, const std::string& v) {
            s.*m = static_cast<float>(std::atof(v.c_str()));
          }};
}

Column dbl(const char* name, double SimState::* m, const char* fmt) {
  return {
      name, nullptr, [m, fmt](const SimState& s) { return num(fmt, s.*m); },
      [m](SimState& s, const std::string& v) { s.*m = std::atof(v.c_str()); }};
}

Column integer(const char* name, int SimState::* m,
               const char* oldName = nullptr) {
  return {
      name, oldName, [m](const SimState& s) { return std::to_string(s.*m); },
      [m](SimState& s, const std::string& v) { s.*m = std::atoi(v.c_str()); }};
}

Column flag(const char* name, bool SimState::* m,
            const char* oldName = nullptr) {
  return {name, oldName,
          [m](const SimState& s) { return std::string(s.*m ? "1" : "0"); },
          [m](SimState& s, const std::string& v) {
            s.*m = std::atoi(v.c_str()) != 0;
          }};
}

Column cue(const char* name, int AircraftCues::* m) {
  return {name, nullptr,
          [m](const SimState& s) { return std::to_string(s.cues.*m); },
          [m](SimState& s, const std::string& v) {
            s.cues.*m = std::atoi(v.c_str());
          }};
}

std::string quote(const std::string& s) {
  if (s.find_first_of(",\"\n") == std::string::npos) return s;
  std::string out = "\"";
  for (char c : s)
    out +=
        (c == '"') ? std::string("\"\"") : std::string(1, c == '\n' ? ' ' : c);
  return out + "\"";
}

Column text(const char* name, std::string SimState::* m) {
  return {name, nullptr, [m](const SimState& s) { return quote(s.*m); },
          [m](SimState& s, const std::string& v) { s.*m = v; }};
}

/// A flight phase as a number: -1 for "none", else the `Phase` value.
std::string phaseOut(const std::optional<Phase>& p) {
  return std::to_string(p ? static_cast<int>(*p) : -1);
}
std::optional<Phase> phaseIn(const std::string& v) {
  const int k = std::atoi(v.c_str());
  if (k < 0 || k > static_cast<int>(Phase::Done)) return std::nullopt;
  return static_cast<Phase>(k);
}

/// The state columns, in file order. New columns go at the end. The "kind"
/// column before them and the "aircraft" and "event" columns after them are
/// written by `traceLine` and `Recorder::event`.
const std::vector<Column>& columns() {
  static const std::vector<Column> cols = {
      dbl("t", &SimState::t, "%.2f"),
      flag("paused", &SimState::paused),
      flag("replay", &SimState::replay),
      flag("on_ground", &SimState::onGround),
      flt("gs_kt", &SimState::gsKt, "%.1f"),
      flt("ias_kt", &SimState::iasKt, "%.1f"),
      flt("agl_ft", &SimState::aglFt, "%.0f"),
      flt("msl_ft", &SimState::mslFt, "%.0f"),
      flt("vs_fpm", &SimState::vsFpm, "%.0f"),
      dbl("lat", &SimState::lat, "%.6f"),
      dbl("lon", &SimState::lon, "%.6f"),
      integer("fms_phase_raw", &SimState::fmsPhaseRaw, "fmgs_phase_raw"),
      flt("cruise_alt_ft", &SimState::cruiseAltFt, "%.0f"),
      flag("beacon", &SimState::beacon),
      flag("seatbelt", &SimState::seatbelt),
      flag("landing_lights", &SimState::landingLights),
      flt("speedbrake", &SimState::speedbrake, "%.2f"),
      flag("park_brake", &SimState::parkBrake),
      flag("chocks", &SimState::chocks),
      integer("engines_running", &SimState::enginesRunning),
      integer("engine_count", &SimState::engineCount),
      flt("door_max", &SimState::doorMax, "%.2f"),
      flt("door_fwd", &SimState::doorFwd, "%.2f"),
      integer("door_count", &SimState::doorCount),
      flag("slides_armed", &SimState::slidesArmed),
      integer("pax", &SimState::pax),
      flt("fuel_kg", &SimState::fuelKg, "%.1f"),
      flag("ac_power", &SimState::acPower),
      flag("pax_masks", &SimState::paxMasks),
      flt("cabin_alt_ft", &SimState::cabinAltFt, "%.0f"),
      flag("pa_listen", &SimState::paListen, "pa_on"),
      flt("pa_listen_vol", &SimState::paListenVol, "%.2f", "pa_vol"),
      flag("interphone_on", &SimState::interphoneOn, "cab_on"),
      flt("interphone_vol", &SimState::interphoneVol, "%.2f", "cab_vol"),
      flt("cockpit_door", &SimState::cockpitDoor, "%.2f"),
      flag("view_external", &SimState::viewExternal),
      flag("camera_in_cabin", &SimState::cameraInCabin),
      flt("sun_pitch", &SimState::sunPitchDeg, "%.1f"),
      flt("local_time", &SimState::localTimeSec, "%.0f"),
      text("flight_number", &SimState::flightNumber),
      text("acf_icao", &SimState::acfIcao),
      flag("satcom", &SimState::satcom),
      integer("ground_spoilers", &SimState::groundSpoilers),
      dbl("zulu_sec", &SimState::zuluSec, "%.0f"),
      flt("door_blend", &SimState::doorBlend, "%.2f"),
      integer("strobe", &SimState::strobeOn),
      integer("no_smoking", &SimState::noSmoking),
      integer("gear_down", &SimState::gearLeverDown),
      {"fms_phase", nullptr,
       [](const SimState& s) { return phaseOut(s.fmsPhase); },
       [](SimState& s, const std::string& v) { s.fmsPhase = phaseIn(v); }},
      cue("cue_takeoff_power", &AircraftCues::takeoffPower),
      cue("cue_levers_takeoff", &AircraftCues::leversAtTakeoff),
      cue("cue_liftoff", &AircraftCues::liftoff),
      cue("cue_touchdown", &AircraftCues::touchdown),
      {"cue_phase_hint", nullptr,
       [](const SimState& s) { return phaseOut(s.cues.phaseHint); },
       [](SimState& s, const std::string& v) {
         s.cues.phaseHint = phaseIn(v);
       }},
      {"cue_phase_known", nullptr,
       [](const SimState& s) {
         return std::string(s.cues.phaseKnown ? "1" : "0");
       },
       [](SimState& s, const std::string& v) {
         s.cues.phaseKnown = std::atoi(v.c_str()) != 0;
       }},
      {"cue_liftoff_label", nullptr,
       [](const SimState& s) { return quote(s.cues.liftoffLabel); },
       [](SimState& s, const std::string& v) { s.cues.liftoffLabel = v; }},
      flt("aircraft_volume", &SimState::aircraftVolume, "%.2f"),
      integer("aircraft_pa", &SimState::aircraftPaPlaying),
      // The camera in the aircraft (VP_SPEC § 4.2): empty when unknown.
      {"cam_aft_m", nullptr,
       [](const SimState& s) {
         return s.cameraKnown ? num("%.2f", s.cameraAftM) : std::string();
       },
       [](SimState& s, const std::string& v) {
         if (v.empty()) return;
         s.cameraKnown = true;
         s.cameraAftM = static_cast<float>(std::atof(v.c_str()));
       }},
      {"cam_right_m", nullptr,
       [](const SimState& s) {
         return s.cameraKnown ? num("%.2f", s.cameraRightM) : std::string();
       },
       [](SimState& s, const std::string& v) {
         if (!v.empty())
           s.cameraRightM = static_cast<float>(std::atof(v.c_str()));
       }},
      // The aircraft's turnaround service (VP_SPEC § 4.1c): its state, and
      // its flags as bits (1 running, 2 boarding started, 4 loaded, 8
      // deboarding started, 16 deboarding complete).
      {"svc_state", nullptr,
       [](const SimState& s) { return std::to_string(s.svc.state); },
       [](SimState& s, const std::string& v) {
         if (!v.empty()) s.svc.state = std::atoi(v.c_str());
       }},
      {"svc_flags", nullptr,
       [](const SimState& s) {
         const AircraftServices& a = s.svc;
         return std::to_string(
             (a.running ? 1 : 0) | (a.boardingStarted ? 2 : 0) |
             (a.boardingComplete ? 4 : 0) | (a.deboardingStarted ? 8 : 0) |
             (a.deboardingComplete ? 16 : 0));
       },
       [](SimState& s, const std::string& v) {
         const int b = v.empty() ? 0 : std::atoi(v.c_str());
         s.svc.running = b & 1;
         s.svc.boardingStarted = b & 2;
         s.svc.boardingComplete = b & 4;
         s.svc.deboardingStarted = b & 8;
         s.svc.deboardingComplete = b & 16;
       }},
  };
  return cols;
}

/// "kind,<state columns>,aircraft,event"
const std::string& header() {
  static const std::string h = [] {
    std::string s = "kind";
    for (const Column& c : columns()) s += std::string(",") + c.name;
    return s + ",aircraft,event";
  }();
  return h;
}

/// A state row shorter than this was cut off while it was written. It is the
/// length of the oldest format (0.1.x), so every complete row passes.
constexpr size_t kShortestStateRow = 44;

std::vector<std::string> splitCsv(const std::string& line) {
  std::vector<std::string> out;
  std::string cur;
  bool quoted = false;
  for (size_t k = 0; k < line.size(); ++k) {
    const char c = line[k];
    if (quoted) {
      if (c == '"' && k + 1 < line.size() && line[k + 1] == '"') {
        cur += '"';
        ++k;
      } else if (c == '"') {
        quoted = false;
      } else {
        cur += c;
      }
    } else if (c == '"') {
      quoted = true;
    } else if (c == ',') {
      out.push_back(cur);
      cur.clear();
    } else if (c != '\r') {
      cur += c;
    }
  }
  out.push_back(cur);
  return out;
}

}  // namespace

const char* traceHeader() { return header().c_str(); }

std::string traceLine(const SimState& s, std::string_view aircraft) {
  std::string line = "S";
  for (const Column& c : columns()) line += "," + c.put(s);
  // The event column of a state row stays empty.
  return line + "," + quote(std::string(aircraft)) + ",";
}

bool Recorder::open(const std::string& path) {
  close();
  std::error_code ec;
  const auto p = std::filesystem::u8path(path);
  std::filesystem::create_directories(p.parent_path(), ec);
#if defined(_WIN32)
  file_ = _wfopen(p.wstring().c_str(), L"w");
#else
  file_ = std::fopen(p.string().c_str(), "w");
#endif
  if (!file_) return false;
  path_ = path;
  lastState_ = -1e9;
  std::fprintf(file_, "%s\n", traceHeader());
  return true;
}

void Recorder::close() {
  if (file_) std::fclose(file_);
  file_ = nullptr;
}

void Recorder::state(const SimState& s, double interval) {
  if (!file_ || s.t - lastState_ < interval) return;
  lastState_ = s.t;
  std::fprintf(file_, "%s\n", traceLine(s, aircraft_).c_str());
  std::fflush(file_);
}

void Recorder::event(double t, const std::string& what) {
  if (!file_) return;
  // An event row has its time, empty state columns and the text in the last
  // column.
  const size_t stateCols = columns().size();
  std::fprintf(file_, "E,%.2f%s%s\n", t,
               std::string(stateCols + 1, ',').c_str(), quote(what).c_str());
  std::fflush(file_);
}

std::vector<TraceRow> readTrace(const std::string& text) {
  std::vector<TraceRow> rows;
  // Column name -> index, from the trace's own header line.
  std::map<std::string, size_t> col;
  std::vector<std::string> names;
  bool legacy = false;
  auto useHeader = [&](const std::string& line) {
    col.clear();
    names = splitCsv(line);
    for (size_t k = 0; k < names.size(); ++k) col[names[k]] = k;
    legacy = col.find("aircraft") == col.end();
  };
  useHeader(header());  // a trace without a header has the current layout
  size_t pos = 0;
  while (pos < text.size()) {
    size_t end = text.find('\n', pos);
    if (end == std::string::npos) end = text.size();
    const std::string line = text.substr(pos, end - pos);
    pos = end + 1;
    if (line.empty() || line[0] == '#') continue;
    if (line.rfind("kind,", 0) == 0) {
      useHeader(line);
      continue;
    }
    const std::vector<std::string> c = splitCsv(line);
    if (c.size() < 3 || (c[0] != "S" && c[0] != "E")) continue;
    TraceRow r;
    r.isEvent = c[0] == "E";
    r.legacy = legacy;
    r.state.t = std::atof(c[1].c_str());
    if (r.isEvent) {
      r.event = c.back();
      rows.push_back(std::move(r));
      continue;
    }
    if (c.size() < kShortestStateRow) continue;
    // Known columns, under their current or their old name. A column that is
    // missing or empty leaves the field at its default.
    std::vector<bool> used(c.size(), false);
    auto field = [&](const char* name) -> const std::string* {
      if (!name) return nullptr;
      const auto it = col.find(name);
      if (it == col.end() || it->second >= c.size()) return nullptr;
      used[it->second] = true;
      return &c[it->second];
    };
    used[0] = true;
    for (const Column& column : columns()) {
      const std::string* v = field(column.name);
      if (!v) v = field(column.oldName);
      if (v && !v->empty()) column.get(r.state, *v);
    }
    if (const std::string* v = field("aircraft")) r.aircraft = *v;
    field("event");
    // Every trace so far came from an aircraft that reports its slides.
    r.state.slidesKnown = true;
    // What is left are columns this version no longer writes.
    for (size_t k = 0; k < c.size() && k < names.size(); ++k)
      if (!used[k] && !c[k].empty()) r.extra[names[k]] = c[k];
    rows.push_back(std::move(r));
  }
  return rows;
}

}  // namespace xn
