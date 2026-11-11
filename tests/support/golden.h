/// @file
/// Golden stories: what the purser decided and played during a scripted or
/// recorded flight, saved as text and compared on every run. A story
/// (tools/replay/replay.h) records every purser note, timeline change, call
/// into the player and change of the stage, and the log lines of the run, so
/// any change of behaviour shows up as a difference.
///
/// Set the environment variable `XN_UPDATE_GOLDEN` to write the stories
/// instead of comparing them, after a change of behaviour that was intended.
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

/// Compares a story with `<dir>/<name>.story.txt`, or writes that file when
/// `XN_UPDATE_GOLDEN` is set. A missing file fails the test.
inline void checkGolden(const std::string& text,
                        const std::filesystem::path& dir,
                        const std::string& name) {
  if (!goldenComparable()) return;
  const std::filesystem::path file = dir / (name + ".story.txt");
  if (std::getenv("XN_UPDATE_GOLDEN")) {
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    out << text;
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
  if (want == text) return;
  // Report the first line that differs.
  std::istringstream a(want), b(text);
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

}  // namespace xn::testing
