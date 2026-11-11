/// @file
/// Reads the OFP that the simbrief_hub plugin publishes (`sbh/*`) and looks
/// airports up in X-Plane's navigation data. The hub is optional and may load
/// after Xnounce. Main thread only.
#pragma once

#include <string>
#include <vector>

#include "app/flight_plan_service.h"
#include "core/simbrief.h"
#include "sim/dataref.h"

namespace xn {

class SimbriefSource : public FlightPlanSource {
 public:
  SimbriefSource();
  SimbriefSource(const SimbriefSource&) =
      delete;  // refs_ points into this object
  SimbriefSource& operator=(const SimbriefSource&) = delete;

  /// Looks for the hub every 10 s until it shows up, then reads the OFP
  /// again whenever `sbh/seqno` changes. True when `text()` changed.
  bool poll(double now) override;
  /// Forgets what was read (a new aircraft was loaded).
  void reset() override;
  bool installed() const override { return installed_; }
  const OfpText& text() const override { return text_; }

  /// The nearest airport with exactly this ICAO code.
  bool findAirport(const std::string& icao, double nearLat, double nearLon,
                   double& lat, double& lon) override;

 private:
  DataRef seqno_{"sbh/seqno"}, stale_{"sbh/stale"}, estOut_{"sbh/est_out"},
      airline_{"sbh/icao_airline"}, flightNo_{"sbh/flight_number"},
      origin_{"sbh/origin"}, dest_{"sbh/destination"}, pax_{"sbh/pax_count"},
      maxPax_{"sbh/max_passengers"}, acIcao_{"sbh/aircraft_icao"};
  std::vector<DataRef*> refs_;
  double nextResolve_ = -1;
  bool installed_ = false;
  int seenSeqno_ = -1;
  int seenStale_ = -1;
  OfpText text_;
};

}  // namespace xn
