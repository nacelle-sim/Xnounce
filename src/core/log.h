/// @file
/// The plugin's log, `Output/Xnounce.log`. Warnings, errors and summary lines
/// also go to X-Plane's `Log.txt` through a sink. Pure C++, so the tests log
/// too.
#pragma once

#include <cstdarg>
#include <deque>
#include <functional>
#include <mutex>
#include <string>

namespace xn {

enum class LogLevel { Error = 0, Warn = 1, Info = 2, Debug = 3 };

/// A row of the Log tab. A line that repeats an earlier one exactly becomes
/// the newest row, with a count.
struct LogLine {
  std::string time;  ///< Local time, HH:MM:SS.
  LogLevel level = LogLevel::Info;
  std::string msg;
  int count = 1;
};
const char* logLevelName(LogLevel lvl);

class Log {
 public:
  static Log& get();

  /// Opens the log file, keeping one backup when it is larger than 1 MB. An
  /// empty path closes it.
  void open(const std::string& path);
  void close();
  void setLevel(LogLevel lvl) { level_ = lvl; }
  LogLevel level() const { return level_; }
  /// Where warnings, errors and summary lines also go (X-Plane's `Log.txt`).
  void setSummarySink(std::function<void(const char*)> sink);

  void write(LogLevel lvl, bool summary, const char* fmt, va_list ap);
  /// The last rows for the Log tab, oldest first. The file keeps every line.
  std::deque<LogLine> recent() const;
  static constexpr size_t kRecentLines = 400;

 private:
  Log() = default;
  mutable std::mutex mu_;
  LogLevel level_ = LogLevel::Info;
  std::string path_;
  void* file_ = nullptr;  // the open FILE*
  std::function<void(const char*)> sink_;
  std::deque<LogLine> recent_;
};

void logError(const char* fmt, ...);
void logWarn(const char* fmt, ...);
void logInfo(const char* fmt, ...);
void logDebug(const char* fmt, ...);
/// An info line that also goes to X-Plane's `Log.txt`.
void logSummary(const char* fmt, ...);

}  // namespace xn
