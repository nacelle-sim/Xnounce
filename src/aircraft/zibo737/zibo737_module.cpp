/// @file
/// Implements `src/aircraft/zibo737/zibo737_module.h`; the interface is
/// documented there.
#include "aircraft/zibo737/zibo737_module.h"

#include <XPLMUtilities.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "aircraft/xp_common.h"
#include "aircraft/zibo737/zibo737_identity.h"

namespace xn::zibo737 {

namespace {

int whole(const DataRef& r) {
  const auto v = r.getFloat();
  return v && std::isfinite(*v) ? static_cast<int>(std::lround(*v)) : -1;
}

float ratio(const DataRef& r) {
  const auto v = r.getFloat();
  return v && std::isfinite(*v) ? *v : -1.0f;
}

std::string fmt(const char* f, double v) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), f, v);
  return buf;
}

}  // namespace

Module::Module(const Options& options) : options_(options) {
  refs_ = {&flightPhase_,     &airGround_,  &apTakeoff_,     &n1Mode_,
           &throttle_,        &landed_,     &seatbeltLight_, &seatbeltSwitch_,
           &noSmokingSwitch_, &gearHandle_, &landRetL_,      &landRetR_,
           &landFixL_,        &landFixR_,   &positionLight_, &parkBrake_,
           &chocks_,          &seats_,      &bus1_,          &bus2_,
           &genOff1_,         &genOff2_,    &apuBus1_,       &apuBus2_,
           &paxOxy_,          &o2Failure_,  &cabinAlt_,      &cockpitDoor_,
           &cruiseAlt_,       &autoFlight_, &msgPlaying_,    &cabinReady_,
           &paxLighting_,     &callLight_};
  for (auto& r : doors_) refs_.push_back(&r);
  for (auto& r : zones_) refs_.push_back(&r);
  for (auto& r : micCab_) refs_.push_back(&r);
}

Recognition Module::recognise(const LoadedAircraft& aircraft) {
  // The Zibo FMC publishes its flight phase here; Laminar's own 737-800 uses
  // X-Plane's FMS and has no such dataref (seen on the LevelUp, 2026-11-11).
  DataRef marker("laminar/B738/FMS/flight_phase");
  return classify(aircraft, marker.resolve());
}

std::vector<std::string> Module::activate(const LoadedAircraft& aircraft) {
  aircraft_ = aircraft;
  model_ = modelFrom(aircraft.acfIcao, aircraft.acfFile);
  name_ = makerOf(aircraft) == Maker::LevelUp ? "LevelUp" : "Zibo";
  signals_.reset();
  actions_.activate(makerOf(aircraft) != Maker::LevelUp);
  std::vector<std::string> missing;
  for (DataRef* r : refs_)
    if (!r->resolve()) missing.emplace_back(r->name());
  // Only the -900 and -900ER have mid exits: on the others they are expected
  // to be missing.
  missing.erase(std::remove_if(missing.begin(), missing.end(),
                               [](const std::string& n) {
                                 return n.find("_mid") != std::string::npos;
                               }),
                missing.end());
  return missing;
}

void Module::deactivate() {
  actions_.deactivate();
  aircraft_ = LoadedAircraft{};
  name_ = "Zibo / LevelUp";
  signals_.reset();
}

void Module::read(SimState& s) {
  Raw r;
  r.flightPhase = whole(flightPhase_);
  r.airGround = whole(airGround_);
  r.apTakeoff = whole(apTakeoff_);
  if (throttle_.good()) {
    for (float x : throttle_.getArray(2))
      if (std::isfinite(x)) r.throttleMax = std::max(r.throttleMax, x);
  }
  r.landed = whole(landed_);
  r.seatbeltLight = whole(seatbeltLight_);
  r.seatbeltSwitch = whole(seatbeltSwitch_);
  r.noSmokingSwitch = whole(noSmokingSwitch_);
  r.gearHandle = ratio(gearHandle_);
  for (const DataRef* l : {&landRetL_, &landRetR_})
    if (l->good()) r.retractableLights.push_back(ratio(*l));
  for (const DataRef* l : {&landFixL_, &landFixR_})
    if (l->good()) r.fixedLights.push_back(ratio(*l));
  if (positionLight_.good()) r.positionLight = whole(positionLight_);
  r.parkBrake = whole(parkBrake_);
  r.chocks = whole(chocks_);
  for (const DataRef& d : doors_)
    if (d.good()) r.doors.push_back(std::max(0.0f, ratio(d)));
  bool anyZone = false;
  int pax = 0;
  for (const DataRef& z : zones_) {
    if (!z.good()) continue;
    anyZone = true;
    const std::vector<float> v = z.getArray(3);  // men, women, children
    for (float x : v)
      if (std::isfinite(x) && x > 0) pax += static_cast<int>(std::lround(x));
  }
  r.pax = anyZone ? pax : -1;
  if (seats_.good())
    for (float x : seats_.getArray(8))
      if (std::isfinite(x) && x > 0)
        r.seats += static_cast<int>(std::lround(x));
  r.bus1 = whole(bus1_) >= 0 ? (whole(bus1_) > 0 ? 1 : 0) : -1;
  r.bus2 = whole(bus2_) >= 0 ? (whole(bus2_) > 0 ? 1 : 0) : -1;
  r.genOff1 = whole(genOff1_) >= 0 ? (ratio(genOff1_) > 0.5f ? 1 : 0) : -1;
  r.genOff2 = whole(genOff2_) >= 0 ? (ratio(genOff2_) > 0.5f ? 1 : 0) : -1;
  r.apuBus1 = whole(apuBus1_) >= 0 ? (ratio(apuBus1_) > 0.5f ? 1 : 0) : -1;
  r.apuBus2 = whole(apuBus2_) >= 0 ? (ratio(apuBus2_) > 0.5f ? 1 : 0) : -1;
  if (paxOxy_.good() || o2Failure_.good())
    r.masks = ratio(paxOxy_) > 0.5f || whole(o2Failure_) == 6 ? 1 : 0;
  r.cockpitDoor = ratio(cockpitDoor_);
  r.cruiseAlt = ratio(cruiseAlt_);
  if (micCab_[0].good() || micCab_[1].good())
    r.interphone = ratio(micCab_[0]) > 0.5f || ratio(micCab_[1]) > 0.5f ? 1 : 0;
  r.msgPlaying = ratio(msgPlaying_);

  signals_.setTakeoffLever(options_.takeoffLever);
  signals_.apply(r, s);
  if (auto a = cabinAlt_.getFloat(); a && std::isfinite(*a)) s.cabinAltFt = *a;
  raw_ = std::move(r);
}

std::string Module::chimeSound() const {
  const std::string sep = XPLMGetDirectorySeparator();
  return "Resources" + sep + "sounds" + sep + "alert" + sep + "seatbelt.wav";
}

AircraftType Module::type(const SimState&) const {
  return makeType(model_, options_, raw_.seats);
}

std::vector<ObservedCommand> Module::observedCommands() const {
  // The overhead ATTEND button calls the cabin; GRD CALL calls the ground
  // crew and is only logged.
  return {{"laminar/B738/push_button/attend", true},
          {"laminar/B738/push_button/grd_call", false}};
}

std::vector<std::pair<std::string, std::string>> Module::rawSnapshot() const {
  std::vector<std::pair<std::string, std::string>> rows;
  for (const DataRef* r : refs_) {
    std::string v;
    char buf[32];
    if (!r->good()) continue;  // a missing one was logged at activation
    if (r->arraySize() > 0) {
      for (float x : r->getArray(16)) {
        std::snprintf(buf, sizeof(buf), "%s%.2f", v.empty() ? "" : " ",
                      static_cast<double>(x));
        v += buf;
      }
    } else if (auto f = r->getFloat()) {
      std::snprintf(buf, sizeof(buf), "%.2f", static_cast<double>(*f));
      v = buf;
    }
    rows.emplace_back(r->name(), v);
  }
  return rows;
}

std::vector<LiveRow> Module::liveInputs(const SimState&) const {
  auto num = [](int v) { return v >= 0 ? std::to_string(v) : "-"; };
  return {
      {"FMC phase / air-ground",
       num(raw_.flightPhase) + " / " +
           (raw_.airGround < 0 ? "-"
            : raw_.airGround   ? "ground"
                               : "air"),
       "Purser phase"},
      {"TO/GA / thrust levers",
       num(raw_.apTakeoff) + " / " +
           (raw_.throttleMax < 0 ? std::string("-")
                                 : fmt("%.2f", raw_.throttleMax)),
       "FMC phase / air-ground"},
      {"Seat belts / no smoking switch",
       num(raw_.seatbeltSwitch) + " / " + num(raw_.noSmokingSwitch) +
           " (0 off, 1 auto, 2 on)",
       "Beacon / seatbelts"},
      {"AC source",
       num(raw_.bus1 < 0 ? -1
                         : busSource(raw_.bus1, raw_.genOff1, raw_.apuBus1)) +
           " / " +
           num(raw_.bus2 < 0
                   ? -1
                   : busSource(raw_.bus2, raw_.genOff2, raw_.apuBus2)) +
           " (3 gen, 2 APU, 1 ground, 0 off)",
       "AC power"},
      {"Cabin seats", raw_.seats > 0 ? std::to_string(raw_.seats) : "-",
       "Passengers"},
  };
}

}  // namespace xn::zibo737
