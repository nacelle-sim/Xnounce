/// @file
/// Implements `src/aircraft/zibo737/zibo737_identity.h`; the interface is
/// documented there.
#include "aircraft/zibo737/zibo737_identity.h"

#include "core/strings.h"

namespace xn::zibo737 {

Maker makerOf(const LoadedAircraft& ac) {
  // LevelUp signs its .acf files as author. Zibo keeps Laminar's author and
  // names itself in the studio field; its folder is "B737-800X".
  if (icontains(ac.author, "levelup") || icontains(ac.studio, "levelup"))
    return Maker::LevelUp;
  if (icontains(ac.studio, "zibo") || icontains(ac.acfPath, "B737-800X") ||
      icontains(ac.acfPath, "zibo"))
    return Maker::Zibo;
  return Maker::Unknown;
}

std::string modelOf(const LoadedAircraft& ac) {
  const std::string icao = toUpper(trim(ac.acfIcao));
  if (icao == "B736") return "737-600";
  if (icao == "B737") return "737-700";
  if (icao == "B738") return "737-800";
  // The -900 and the -900ER share B739. LevelUp's files are 737_90NG.acf and
  // 737_9ENG.acf.
  if (icao == "B739")
    return icontains(ac.acfFile, "9E") ? "737-900ER" : "737-900";
  return {};
}

Recognition classify(const LoadedAircraft& ac, bool markerPresent) {
  Recognition r;
  const Maker maker = makerOf(ac);
  const std::string model = modelOf(ac);
  // Laminar's own 737-800 has the same ICAO designator and author, so the
  // maker and the marker both have to say Zibo.
  if (!markerPresent || maker == Maker::Unknown || model.empty()) return r;
  r.ours = true;
  r.supported = true;
  r.model = std::string(maker == Maker::LevelUp ? "LevelUp " : "Zibo ") + model;
  return r;
}

}  // namespace xn::zibo737
