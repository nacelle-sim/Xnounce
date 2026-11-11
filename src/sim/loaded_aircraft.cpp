/// @file
/// Implements `src/sim/loaded_aircraft.h`; the interface is documented there.
#include "sim/loaded_aircraft.h"

#include <XPLMPlanes.h>

#include "core/strings.h"
#include "sim/dataref.h"

namespace xn {

namespace {

std::string lastPathPart(std::string p) {
  while (!p.empty() && (p.back() == '/' || p.back() == '\\')) p.pop_back();
  const size_t k = p.find_last_of("/\\");
  return k == std::string::npos ? p : p.substr(k + 1);
}

}  // namespace

LoadedAircraft readLoadedAircraft() {
  LoadedAircraft ac;
  char file[512] = {}, path[2048] = {};
  XPLMGetNthAircraftModel(0, file, path);
  ac.acfFile = cleanCString(file, sizeof(file));
  ac.acfPath = cleanCString(path, sizeof(path));
  const size_t cut = ac.acfPath.find_last_of("/\\");
  if (cut != std::string::npos) ac.acfDir = ac.acfPath.substr(0, cut + 1);

  DataRef icao("sim/aircraft/view/acf_ICAO");
  DataRef author("sim/aircraft/view/acf_author");
  DataRef studio("sim/aircraft/view/acf_studio");
  DataRef livery("sim/aircraft/view/acf_livery_path");
  DataRef uiName("sim/aircraft/view/acf_ui_name");
  DataRef tail("sim/aircraft/view/acf_tailnum");
  tail.resolve();
  icao.resolve();
  uiName.resolve();
  author.resolve();
  studio.resolve();
  livery.resolve();
  ac.acfIcao = icao.getString(40);
  ac.author = author.getString(500);
  ac.studio = studio.getString(500);
  ac.liveryPath = livery.getString(1024);
  ac.uiName = uiName.getString(260);
  ac.tailNumber = tail.getString(40);
  if (!ac.liveryPath.empty()) ac.liveryName = lastPathPart(ac.liveryPath);
  return ac;
}

}  // namespace xn
