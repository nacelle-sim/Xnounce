/// @file
/// Implements `src/aircraft/registry.h`; the interface is documented there.
#include "aircraft/registry.h"

#include "aircraft/toliss/toliss_module.h"
#include "aircraft/zibo737/zibo737_module.h"

namespace xn {

namespace {

/// Joins the modules' phrases: "a", "a and b", "a, b and c".
std::string joinPhrases(const std::vector<std::string>& parts,
                        const char* last) {
  std::string out;
  for (size_t k = 0; k < parts.size(); ++k) {
    if (k > 0) out += k + 1 == parts.size() ? last : ", ";
    out += parts[k];
  }
  return out;
}

}  // namespace

AircraftModules makeAircraftModules(const AircraftOptions& options) {
  AircraftModules modules;
  modules.push_back(std::make_unique<toliss::Module>(options.toliss));
  modules.push_back(std::make_unique<zibo737::Module>(options.zibo737));
  return modules;
}

std::string supportedAircraft(const AircraftModules& modules) {
  std::vector<std::string> parts;
  for (const auto& m : modules)
    if (*m->supportedAircraft()) parts.emplace_back(m->supportedAircraft());
  return joinPhrases(parts, " and ");
}

std::string loadHint(const AircraftModules& modules) {
  std::vector<std::string> parts;
  for (const auto& m : modules)
    if (*m->loadHint()) parts.emplace_back(m->loadHint());
  return joinPhrases(parts, " or ");
}

}  // namespace xn
