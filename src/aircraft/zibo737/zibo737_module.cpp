/// @file
/// Implements `src/aircraft/zibo737/zibo737_module.h`; the interface is
/// documented there.
#include "aircraft/zibo737/zibo737_module.h"

#include "aircraft/zibo737/zibo737_identity.h"
#include "sim/dataref.h"

namespace xn::zibo737 {

Recognition Module::recognise(const LoadedAircraft& aircraft) {
  // The Zibo FMC publishes its flight phase here. Laminar's own 737-800 uses
  // X-Plane's FMS and has no such dataref. (Assumed from the aircraft's
  // scripts; not yet confirmed in the simulator.)
  DataRef marker("laminar/B738/FMS/flight_phase");
  return classify(aircraft, marker.resolve());
}

}  // namespace xn::zibo737
