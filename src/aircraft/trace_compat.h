/// @file
/// Reading traces of any version.
///
/// `readTrace` (core/recorder.h) parses the file. Traces recorded before
/// 0.3.5 hold aircraft-specific columns instead of today's neutral fields;
/// this is where the aircraft module that wrote them fills those fields in.
#pragma once

#include <string>
#include <vector>

#include "core/recorder.h"

namespace xn {

/// Parses a trace and upgrades old rows, so that every `SimState` is what
/// the current code would have recorded. Use this for replay.
std::vector<TraceRow> readTraceCompat(const std::string& csvText);

}  // namespace xn
