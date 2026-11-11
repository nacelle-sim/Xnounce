/// @file
/// Implements `src/aircraft/trace_compat.h`; the interface is documented there.
#include "aircraft/trace_compat.h"

#include "aircraft/toliss/toliss_signals.h"

namespace xn {

std::vector<TraceRow> readTraceCompat(const std::string& csvText) {
  std::vector<TraceRow> rows = readTrace(csvText);
  // A trace from before 0.3.5 was recorded on a ToLiss: there was no other
  // aircraft module.
  toliss::LegacyTrace toliss;
  for (TraceRow& r : rows)
    if (r.legacy) toliss.apply(r);
  return rows;
}

}  // namespace xn
