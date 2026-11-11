// xn_replay: prints what the purser decides for a recorded flight.
//
//   xn_replay <trace.xntrace.csv> [--summary]
//
// A trace is what Settings > Advanced > "Record flight trace" writes to
// X-Plane's Output/Xnounce folder. The full story has a line for every purser
// decision and every change of the stage; --summary prints only the decisions
// and the announcements, with the second they happened. Traces from every
// Xnounce and FXAnnounce version can be read.
#include <cstdio>
#include <cstring>
#include <filesystem>

#include "aircraft/trace_compat.h"
#include "core/files.h"
#include "replay.h"

int main(int argc, char** argv) {
  const char* path = nullptr;
  bool summary = false;
  for (int k = 1; k < argc; ++k) {
    if (std::strcmp(argv[k], "--summary") == 0)
      summary = true;
    else if (!path && argv[k][0] != '-')
      path = argv[k];
    else {
      path = nullptr;
      break;
    }
  }
  if (!path) {
    std::fprintf(stderr, "usage: xn_replay <trace.xntrace.csv> [--summary]\n");
    return 2;
  }
  // Traces of long flights are a few megabytes.
  const std::string text = xn::readTextFile(path, 256u << 20);
  const std::vector<xn::TraceRow> rows = xn::readTraceCompat(text);
  if (rows.empty()) {
    std::fprintf(stderr, "xn_replay: %s: no trace rows\n", path);
    return 1;
  }
  const xn::replay::Result r = xn::replay::replayTrace(
      rows, "replay of " + std::filesystem::path(path).filename().string());
  if (summary) {
    for (const std::string& line : r.summary) std::printf("%s\n", line.c_str());
  } else {
    std::fputs(r.story.c_str(), stdout);
  }
  return 0;
}
