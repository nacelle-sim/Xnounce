/// @file
/// Replaying the purser without X-Plane.
///
/// Used by `xn_replay`, which prints what the purser decides for a recorded
/// flight, and by the tests, which compare those decisions with saved
/// "golden stories" (tests/support/golden.h). A story has one line for every
/// purser note, every change on its timeline, every call into the audio
/// player and every change of the stage and the "coming up" list, each
/// stamped with the purser clock, followed by the log lines of the run.
#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "core/log.h"
#include "core/purser/purser.h"
#include "core/recorder.h"

namespace xn::replay {

/// The text of a story.
class Story {
 public:
  /// Adds a line stamped with the purser clock and a short kind ("note",
  /// "event", "out", "state", "next", "call").
  void line(double t, const char* kind, const std::string& text);
  /// Adds a line as it is.
  void raw(const std::string& text) { text_ += text + "\n"; }
  const std::string& text() const { return text_; }

 private:
  std::string text_;
};

/// Captures every log line written while it is alive, at debug level, by
/// pointing the log at a temporary file.
class LogCapture {
 public:
  LogCapture();
  LogCapture(const LogCapture&) = delete;
  LogCapture& operator=(const LogCapture&) = delete;
  ~LogCapture() { finish(); }

  /// Stops capturing and returns the lines without their wall-clock time.
  std::vector<std::string> finish();

 private:
  std::filesystem::path path_;
  LogLevel level_ = LogLevel::Info;
  bool done_ = false;
};

/// A stand-in for the audio player. Every announcement "plays" for a fixed
/// time, CabinSecured is an interphone call and everything else cabin PA,
/// and every call is written to the story.
struct StoryPlayer : PurserOutput {
  /// The purser clock. Must be set before the purser runs.
  const double* clock = nullptr;
  /// Announcement types the pretend pack has no file for.
  std::set<AnnType> missing;
  /// How long each type plays, in seconds. Others play 8 s.
  std::map<AnnType, double> durations;
  /// Labels of what played, for example "CabinSecured(takeoff)#ip".
  std::vector<std::string> played;
  /// Music switched on (true) or off, with the time.
  std::vector<std::pair<double, bool>> music;
  int chimes = 0;
  int stops = 0;
  double endsAt = -1;
  /// Where calls are written, or null.
  Story* story = nullptr;

  std::optional<std::string> play(AnnType t, CallContext c,
                                  bool interphone) override;
  bool playing() const override { return *clock < endsAt; }
  void stop() override;
  void chime() override;
  void setMusic(bool on) override;
  std::optional<Mode> peek(AnnType t, CallContext) const override;

 private:
  void tell(const std::string& what);
};

/// A timeline row as one story line.
std::string eventLine(const TimelineRow& r);
/// Everything the window and the plugin read from the purser between ticks.
std::string statusLine(const Purser& p);
/// The "coming up" list on one line.
std::string nextLine(const Purser& p);

/// What a replay of a recorded flight produced.
struct Result {
  /// The whole story, log lines included.
  std::string story;
  /// What the replay played (`StoryPlayer::played`).
  std::vector<std::string> played;
  /// The announcement types the trace says played in the simulator, one
  /// entry per change.
  std::vector<std::string> recorded;
  /// A short version of the story: "<second>  <what>" for each decision and
  /// each announcement that played, expired or had no sound.
  std::vector<std::string> summary;
};

/// Replays a trace through a fresh purser. `rows` come from
/// `readTraceCompat`. Ticks run at 4 Hz between the recorded 1 Hz rows, so
/// the purser's timers run as they did in the simulator. Boarding starts in
/// the "cabin call only" mode when the trace has cabin call presses and in
/// the "doors open" mode otherwise. `title` is the story's first line.
Result replayTrace(const std::vector<TraceRow>& rows, const std::string& title);

}  // namespace xn::replay
