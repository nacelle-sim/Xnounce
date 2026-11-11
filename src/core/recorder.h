/// @file
/// The flight trace: a CSV recording of what the purser saw and did, and the
/// reader that turns a trace back into `SimState` rows for replay.
///
/// A trace has one "S" row per second with the whole `SimState` and one "E"
/// row for each event (an announcement, a purser decision, a cockpit button,
/// a raw aircraft value that changed). The first line names the columns. The
/// reader maps columns by those names, so a trace stays readable when columns
/// are added or renamed (docs/ARCHITECTURE.md § 2).
///
/// The file format knows no aircraft. Columns that older versions wrote for
/// one aircraft come back in `TraceRow::extra`, and `aircraft/trace_compat`
/// turns them into today's fields.
#pragma once

#include <cstdio>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "core/sim_state.h"

namespace xn {

/// Writes a trace file.
class Recorder {
 public:
  ~Recorder() { close(); }
  /// Creates the file and its folder and writes the header. False when the
  /// file can't be created.
  bool open(const std::string& path);
  void close();
  bool isOpen() const { return file_ != nullptr; }
  const std::string& path() const { return path_; }

  /// Names the aircraft module that fills the snapshots, for the `aircraft`
  /// column.
  void setAircraft(std::string id) { aircraft_ = std::move(id); }
  /// Writes a state row, at most one per `interval` seconds of purser time.
  void state(const SimState& s, double interval = 1.0);
  /// Writes an event row.
  void event(double t, const std::string& what);

 private:
  FILE* file_ = nullptr;
  std::string path_;
  std::string aircraft_;
  double lastState_ = -1e9;
};

/// One line of a trace.
struct TraceRow {
  bool isEvent = false;
  /// The snapshot of a state row. An event row only has `state.t`.
  SimState state;
  /// The text of an event row.
  std::string event;
  /// Id of the aircraft module that recorded a state row.
  std::string aircraft;
  /// True when the trace has no `aircraft` column: it is from before 0.3.5,
  /// when the plugin ran on ToLiss aircraft only, and its rows need
  /// `readTraceCompat`.
  bool legacy = false;
  /// Columns of a state row this version doesn't write any more, by name.
  std::map<std::string, std::string> extra;
};

/// The header line of the current format, without a line end. Hand-written
/// test fixtures use it too.
const char* traceHeader();
/// One state row in the current format, without a line end.
std::string traceLine(const SimState& s, std::string_view aircraft = {});
/// Parses a trace. Lines that can't be read are skipped. Old traces need
/// `readTraceCompat` (aircraft/trace_compat.h) to fill the fields that
/// replaced their aircraft-specific columns.
std::vector<TraceRow> readTrace(const std::string& csvText);

}  // namespace xn
