/// @file
/// The Zibo / LevelUp 737 module. In 0.3.5 it recognises the aircraft and
/// nothing more: the plugin stays dormant on it and says why. The purser
/// support is the 0.4.0 work (docs/aircraft/zibo737.md).
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "aircraft/module.h"

namespace xn::zibo737 {

class Module : public AircraftModule {
 public:
  const char* id() const override { return "zibo737"; }
  const char* name() const override { return "737"; }
  /// Nothing yet: the module supports no aircraft until the purser runs on
  /// the 737.
  const char* supportedAircraft() const override { return ""; }
  const char* loadHint() const override { return ""; }

  Recognition recognise(const LoadedAircraft& aircraft) override;

  // The module never reports an aircraft as supported, so the plugin never
  // activates it and the methods below are never called. They do nothing.
  std::vector<std::string> activate(const LoadedAircraft&) override {
    return {};
  }
  void deactivate() override {}
  float cabinViewAftM() const override { return 2.5f; }
  void read(SimState&) override {}
  AircraftType type(const SimState&) const override { return {}; }
  std::vector<ObservedCommand> observedCommands() const override { return {}; }
  std::vector<std::pair<std::string, std::string>> rawSnapshot()
      const override {
    return {};
  }
  std::vector<LiveRow> liveInputs(const SimState&) const override { return {}; }
};

}  // namespace xn::zibo737
