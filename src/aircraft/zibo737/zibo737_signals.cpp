/// @file
/// Implements `src/aircraft/zibo737/zibo737_signals.h`; the interface is
/// documented there.
#include "aircraft/zibo737/zibo737_signals.h"

#include <algorithm>

namespace xn::zibo737 {

int busSource(int powered, int genOff, int apuOnBus) {
  if (powered < 0) return -1;
  if (powered == 0) return 0;
  if (genOff == 0) return 3;
  if (apuOnBus == 1) return 2;
  return 1;
}

void Signals::reset() { flown_ = false; }

std::optional<Phase> Signals::phase(int raw, const SimState& s) const {
  switch (raw) {
    case 0:
      if (!s.onGround) return std::nullopt;
      if (flown_) return Phase::Done;
      return s.enginesRunning > 0 && s.gsKt > 40 ? Phase::Takeoff
                                                 : Phase::PreFlight;
    case 1:
      return Phase::Climb;
    case 2:
    case 3:
    case 4:
      return Phase::Cruise;
    case 5:
      return Phase::Descent;
    case 6:
    case 7:
      return Phase::Approach;
    case 8:
      return Phase::GA;
    default:
      return std::nullopt;
  }
}

void Signals::apply(const Raw& r, SimState& s) {
  // The flight: airborne once above 50 ft, over when parked with the engines
  // off. That is when the FMC's phase 0 means "after the flight".
  if (!s.onGround && s.aglFt > 50) flown_ = true;
  if (s.onGround && s.enginesRunning == 0 && s.gsKt < 1) flown_ = false;

  if (r.flightPhase >= 0) {
    s.fmsPhaseRaw = r.flightPhase;
    s.fmsPhase = phase(r.flightPhase, s);
  }
  if (r.cruiseAlt > 0) s.cruiseAltFt = r.cruiseAlt;

  // Signs. The seatbelt light follows the switch and, at AUTO, the aircraft's
  // own logic. NO SMOKING has no light of its own: at AUTO it is lit with the
  // gear lever down, as on the aircraft.
  if (r.seatbeltLight >= 0)
    s.seatbelt = r.seatbeltLight > 0;
  else if (r.seatbeltSwitch >= 0)
    s.seatbelt =
        r.seatbeltSwitch == 2 || (r.seatbeltSwitch == 1 && r.gearHandle > 0.9f);
  if (r.noSmokingSwitch >= 0)
    s.noSmoking = (r.noSmokingSwitch == 2 ||
                   (r.noSmokingSwitch == 1 && r.gearHandle > 0.9f))
                      ? 1
                      : 0;
  if (!r.retractableLights.empty() || !r.fixedLights.empty())
    s.landingLights =
        std::any_of(r.retractableLights.begin(), r.retractableLights.end(),
                    [](float v) { return v > 1.5f; }) ||
        std::any_of(r.fixedLights.begin(), r.fixedLights.end(),
                    [](float v) { return v > 0.5f; });
  if (r.positionLight > -9) s.strobeOn = r.positionLight == 1 ? 1 : 0;
  if (r.gearHandle >= 0) s.gearLeverDown = r.gearHandle > 0.9f ? 1 : 0;
  if (r.parkBrake >= 0) s.parkBrake = r.parkBrake > 0;
  if (r.chocks >= 0) s.chocks = r.chocks > 0;

  // Doors. The 737 has no slides to report.
  s.doorCount = static_cast<int>(r.doors.size());
  for (size_t k = 0; k < r.doors.size(); ++k) {
    const float d = std::clamp(r.doors[k], 0.0f, 1.0f);
    s.doorMax = std::max(s.doorMax, d);
    if (k == 0) s.doorFwd = d;
  }
  s.slidesKnown = false;
  s.slidesArmed = false;

  if (r.pax >= 0) s.pax = r.pax;
  if (r.bus1 >= 0 || r.bus2 >= 0) {
    s.acPower = r.bus1 == 1 || r.bus2 == 1;
    const int a = busSource(r.bus1, r.genOff1, r.apuBus1);
    const int b = busSource(r.bus2, r.genOff2, r.apuBus2);
    s.acSource = a < 0 || b < 0 ? -1 : a * 10 + b;
  }
  if (r.masks >= 0) s.paxMasks = r.masks > 0;
  if (r.cockpitDoor >= 0) s.cockpitDoor = std::clamp(r.cockpitDoor, 0.0f, 1.0f);
  if (r.interphone >= 0) {
    s.interphoneOn = r.interphone > 0;
    s.interphoneVol = s.interphoneOn ? 1.0f : 0.0f;
  }

  // Cross-checks (VP_SPEC § 4.2): TO/GA engaged on the ground is takeoff
  // power; the air/ground sensor and the FMOD "landed" flag give lift-off
  // and touchdown.
  AircraftCues c;
  // In the air, phase 0 is no opinion: the purser's own estimate decides.
  c.phaseKnown = r.flightPhase > 0 || (r.flightPhase == 0 && s.onGround);
  if (r.apTakeoff >= 0) c.takeoffPower = s.onGround && r.apTakeoff > 0 ? 1 : 0;
  if (r.throttleMax >= 0)
    c.leversAtTakeoff = r.throttleMax >= takeoffLever_ ? 1 : 0;
  if (r.airGround >= 0) {
    c.liftoff = r.airGround == 0 && r.flightPhase == 0 ? 1 : 0;
    if (c.liftoff == 1) c.liftoffLabel = "air/ground sensor";
  }
  if (r.landed >= 0)
    c.touchdown = r.landed > 0 ? 1 : 0;
  else if (r.airGround >= 0)
    c.touchdown = flown_ && r.airGround == 1 ? 1 : 0;
  s.cues = c;
}

}  // namespace xn::zibo737
