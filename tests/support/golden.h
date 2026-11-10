/// @file
/// Golden stories: what the purser decided and played during a scripted or
/// recorded flight, saved as text and compared on every run.
///
/// A story is the behaviour freeze for a refactor. It records every purser
/// note, every timeline change, every call into the player, every change of
/// the stage label with the "coming up" list at that moment, and the log lines
/// the flight produced. Set the environment variable `XN_UPDATE_GOLDEN` to
/// write the stories instead of comparing them.
///
/// The purser's timings come from `std::uniform_real_distribution`, whose
/// output differs between standard libraries, so stories are only compared
/// when built with libstdc++ (the Linux reference build).
#pragma once

#include <doctest/doctest.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "core/log.h"

namespace xn::testing {

/// True when this build can compare stories (see the file comment).
inline bool goldenComparable() {
#if defined(__GLIBCXX__)
  return true;
#else
  return false;
#endif
}

/// Turns a test case name into a file name: lower case, runs of other
/// characters become one dash, at most 90 characters.
inline std::string goldenSlug(const std::string& name) {
  std::string out;
  for (const char c : name) {
    const unsigned char u = static_cast<unsigned char>(c);
    if (std::isalnum(u)) {
      out += static_cast<char>(std::tolower(u));
    } else if (!out.empty() && out.back() != '-') {
      out += '-';
    }
  }
  while (!out.empty() && out.back() == '-') out.pop_back();
  if (out.size() > 90) out.resize(90);
  return out;
}

/// Name of the doctest test case that is running, or "unnamed".
inline std::string currentTestName() {
  const doctest::ContextOptions* opts = doctest::getContextOptions();
  if (!opts || !opts->currentTest || !opts->currentTest->m_name)
    return "unnamed";
  return opts->currentTest->m_name;
}

/// A file name for the next story of the running test case. The first story
/// is the slug of the test name; further ones get "-2", "-3"…
inline std::string nextGoldenName() {
  static std::map<std::string, int> seen;
  const std::string slug = goldenSlug(currentTestName());
  const int n = ++seen[slug];
  return n == 1 ? slug : slug + "-" + std::to_string(n);
}

/// Captures every log line written while it is alive, at debug level, by
/// pointing the log at a temporary file.
class LogCapture {
 public:
  LogCapture() {
    static int serial = 0;
    std::error_code ec;
    path_ = std::filesystem::temp_directory_path(ec) /
            ("xn_golden_log_" + std::to_string(++serial) + ".txt");
    std::filesystem::remove(path_, ec);
    level_ = Log::get().level();
    Log::get().setLevel(LogLevel::Debug);
    Log::get().open(path_.string());
  }
  LogCapture(const LogCapture&) = delete;
  LogCapture& operator=(const LogCapture&) = delete;
  ~LogCapture() { finish(); }

  /// Stops capturing and returns the lines without their wall-clock time.
  std::vector<std::string> finish() {
    std::vector<std::string> lines;
    if (done_) return lines;
    done_ = true;
    Log::get().open("");
    Log::get().setLevel(level_);
    std::ifstream in(path_);
    // A log line is "HH:MM:SS LEVEL message".
    for (std::string line; std::getline(in, line);)
      lines.push_back(line.size() > 9 ? line.substr(9) : line);
    in.close();
    std::error_code ec;
    std::filesystem::remove(path_, ec);
    return lines;
  }

 private:
  std::filesystem::path path_;
  LogLevel level_ = LogLevel::Info;
  bool done_ = false;
};

/// The text of one story.
class Story {
 public:
  /// Adds a line stamped with the purser clock.
  void line(double t, const char* kind, const std::string& text) {
    char head[48];
    std::snprintf(head, sizeof(head), "%9.2f  %-6s ", t, kind);
    text_ += head + text + "\n";
  }
  /// Adds a line as it is.
  void raw(const std::string& text) { text_ += text + "\n"; }
  const std::string& text() const { return text_; }

  /// Compares the story with `<dir>/<name>.story.txt`, or writes that file
  /// when `XN_UPDATE_GOLDEN` is set. A missing file fails the test.
  void check(const std::filesystem::path& dir, const std::string& name) const {
    if (!goldenComparable()) return;
    const std::filesystem::path file = dir / (name + ".story.txt");
    if (std::getenv("XN_UPDATE_GOLDEN")) {
      std::error_code ec;
      std::filesystem::create_directories(dir, ec);
      std::ofstream out(file, std::ios::binary | std::ios::trunc);
      out << text_;
      return;
    }
    std::ifstream in(file, std::ios::binary);
    if (!in) {
      FAIL_CHECK("no golden story " << file.string()
                                    << " (run with XN_UPDATE_GOLDEN=1)");
      return;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string want = ss.str();
    if (want == text_) return;
    // Report the first line that differs.
    std::istringstream a(want), b(text_);
    std::string la, lb;
    int n = 0;
    while (true) {
      const bool ga = static_cast<bool>(std::getline(a, la));
      const bool gb = static_cast<bool>(std::getline(b, lb));
      ++n;
      if (!ga && !gb) break;
      if (ga != gb || la != lb) {
        FAIL_CHECK("story " << name << " differs at line " << n
                            << "\n  golden: " << (ga ? la : "(end)")
                            << "\n  now:    " << (gb ? lb : "(end)"));
        return;
      }
    }
  }

 private:
  std::string text_;
};

}  // namespace xn::testing
