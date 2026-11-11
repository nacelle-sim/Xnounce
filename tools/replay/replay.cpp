#include "replay.h"

#include <algorithm>
#include <cstdio>
#include <fstream>

#include "aircraft/toliss/toliss_types.h"
#include "aircraft/zibo737/zibo737_types.h"

namespace xn::replay {

void Story::line(double t, const char* kind, const std::string& text) {
  char head[48];
  std::snprintf(head, sizeof(head), "%9.2f  %-6s ", t, kind);
  text_ += head + text + "\n";
}

LogCapture::LogCapture() {
  static int serial = 0;
  std::error_code ec;
  path_ = std::filesystem::temp_directory_path(ec) /
          ("xn_story_log_" + std::to_string(++serial) + ".txt");
  std::filesystem::remove(path_, ec);
  level_ = Log::get().level();
  Log::get().setLevel(LogLevel::Debug);
  Log::get().open(path_.string());
}

std::vector<std::string> LogCapture::finish() {
  std::vector<std::string> lines;
  if (done_) return lines;
  done_ = true;
  Log::get().open("");
  Log::get().setLevel(level_);
  std::ifstream in(path_);
  // A log line is "HH:MM:SS LEVEL message"; the time is dropped.
  for (std::string line; std::getline(in, line);)
    lines.push_back(line.size() > 9 ? line.substr(9) : line);
  in.close();
  std::error_code ec;
  std::filesystem::remove(path_, ec);
  return lines;
}

void StoryPlayer::tell(const std::string& what) {
  if (story) story->line(*clock, "out", what);
}

std::optional<std::string> StoryPlayer::play(AnnType t, CallContext c,
                                             bool interphone) {
  std::string label = annTypeName(t);
  if (c != CallContext::Any) label += std::string("(") + contextName(c) + ")";
  if (interphone) label += "#ip";
  if (missing.count(t)) {
    tell("play " + label + " -> no sound");
    return std::nullopt;
  }
  tell("play " + label);
  played.push_back(label);
  const auto d = durations.find(t);
  endsAt = *clock + (d != durations.end() ? d->second : 8.0);
  return label;
}

void StoryPlayer::stop() {
  tell("stop");
  endsAt = -1;
  ++stops;
}

void StoryPlayer::chime() {
  tell("chime");
  ++chimes;
}

void StoryPlayer::setMusic(bool on) {
  tell(on ? "music on" : "music off");
  music.emplace_back(*clock, on);
}

std::optional<Mode> StoryPlayer::peek(AnnType t, CallContext) const {
  if (missing.count(t)) return std::nullopt;
  return t == AnnType::CabinSecured ? Mode::Interphone : Mode::Manual;
}

std::string eventLine(const TimelineRow& r) {
  char clocks[48];
  std::snprintf(clocks, sizeof(clocks), " @z%.0f/l%.0f", r.zuluSec, r.localSec);
  return std::string(annTypeName(r.type)) +
         (r.context != CallContext::Any
              ? std::string("(") + contextName(r.context) + ")"
              : std::string()) +
         " " + outcomeName(r.outcome) +
         (r.detail.empty() ? std::string() : ": " + r.detail) + clocks;
}

std::string statusLine(const Purser& p) {
  char buf[96];
  std::snprintf(buf, sizeof(buf), " | boarding %.2f..%.2f | attempt %d",
                p.boardingStartedAt(), p.boardingEndsAt(), p.callAttempt());
  return p.stageLabel() + " | " + phaseName(p.phase()) +
         (p.phaseEstimated() ? " (estimated)" : "") +
         (p.calling() ? " | calling" : "") +
         (p.packLocked() ? " | pack locked" : "") +
         (p.tookOff() ? " | took off" : "") +
         (p.flightComplete() ? " | flight complete" : "") + buf + " | now " +
         p.nowLabel() +
         (p.boardingNote().empty() ? std::string()
                                   : " | note: " + p.boardingNote());
}

std::string nextLine(const Purser& p) {
  std::string all;
  for (const std::string& l : p.pendingLabels())
    all += (all.empty() ? "" : " | ") + l;
  return all.empty() ? "-" : all;
}

namespace {

bool isCabinCall(const std::string& event) {
  return event.rfind("call button ", 0) == 0 &&
         event.find("/mech") == std::string::npos;
}

/// The aircraft type for a trace. `module` is the `aircraft` column of its
/// first state row; traces from before 0.3.5 have none and were all recorded
/// on a ToLiss. The type comes from the ICAO designator of `first`, the
/// trace's first row. For a ToLiss trace that row is kept as it always was
/// (an event row gives the A320), so the recorded stories don't change.
AircraftType typeFor(const std::string& module, const SimState& first) {
  if (module == "zibo737")
    return zibo737::makeType(zibo737::modelFrom(first.acfIcao, ""),
                             zibo737::Options{});
  const toliss::Family fam = toliss::familyFrom(first.acfIcao, "");
  return toliss::makeType(
      fam == toliss::Family::Unknown ? toliss::Family::A320 : fam,
      first.acfIcao == "A20N" || first.acfIcao == "A21N", toliss::Options{});
}

}  // namespace

Result replayTrace(const std::vector<TraceRow>& rows,
                   const std::string& title) {
  Result result;
  if (rows.empty()) return result;
  bool calls = false;
  for (const TraceRow& r : rows) {
    if (!r.isEvent) continue;
    if (isCabinCall(r.event)) calls = true;
    const size_t sp = r.event.find(" playing");
    if (sp == std::string::npos) continue;
    const std::string type = r.event.substr(0, sp);
    if (result.recorded.empty() || result.recorded.back() != type)
      result.recorded.push_back(type);
  }

  StoryPlayer out;
  double clock = 0;
  out.clock = &clock;
  Story story;
  LogCapture logs;
  story.raw("# " + title);
  out.story = &story;
  std::string lastStatus;
  Purser p(out, 7);
  Settings set;
  set.boardingStart =
      static_cast<int>(calls ? BoardingStart::Call : BoardingStart::Doors);
  // A trace starts with the flight plan's event rows; the first state row
  // names the aircraft.
  const auto firstState = std::find_if(
      rows.begin(), rows.end(), [](const TraceRow& r) { return !r.isEvent; });
  p.setAircraft(
      typeFor(firstState != rows.end() ? firstState->aircraft : std::string(),
              firstState != rows.end() && firstState->aircraft == "zibo737"
                  ? firstState->state
                  : rows.front().state));
  p.setNoteSink([&](double at, const std::string& text) {
    story.line(clock, "note", text);
    result.summary.push_back(std::to_string(static_cast<int>(at)) + "  " +
                             text);
  });
  p.setEventSink([&](const TimelineRow& row) {
    story.line(clock, "event", eventLine(row));
    if (row.outcome == Outcome::Playing || row.outcome == Outcome::Expired ||
        row.outcome == Outcome::Unanswered || row.outcome == Outcome::NoSound)
      result.summary.push_back(
          std::to_string(static_cast<int>(row.t)) + "  " +
          annTypeName(row.type) +
          (row.context != CallContext::Any
               ? std::string("(") + contextName(row.context) + ")"
               : "") +
          " " + outcomeName(row.outcome));
  });
  double last = -1;
  for (const TraceRow& r : rows) {
    if (r.isEvent) {
      if (isCabinCall(r.event)) {
        story.line(clock, "call", r.event);
        p.cabinCall();
      }
      continue;
    }
    SimState s = r.state;
    const double from = last < 0 ? s.t : last;
    for (double t = from + 0.25; t <= s.t + 1e-6; t += 0.25) {
      clock = t;
      s.t = t;
      p.tick(s, set);
      const std::string status = statusLine(p);
      if (status != lastStatus) {
        lastStatus = status;
        story.line(clock, "state", status);
        story.line(clock, "next", nextLine(p));
      }
    }
    last = r.state.t;
  }
  story.raw("--- log ---");
  for (const std::string& l : logs.finish()) story.raw(l);
  result.story = story.text();
  result.played = out.played;
  return result;
}

}  // namespace xn::replay
