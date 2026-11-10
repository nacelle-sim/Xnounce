// xplm entry points everything forwards to app inside guarded so nothing throws
// into xp
#include <XPLMDefs.h>

#include <cstring>

#include "app.h"
#include "core/guard.h"

PLUGIN_API int XPluginStart(char* outName, char* outSig, char* outDesc) {
  std::strncpy(outName, "Xnounce " XN_VERSION, 255);
  std::strncpy(outSig, "yosh.xnounce", 255);
  std::strncpy(outDesc,
               "Fenix-style Airline Pack cabin announcements for the ToLiss "
               "Airbus family",
               255);
  return xn::guarded("XPluginStart", 0, [] {
    xn::App::get().start();
    return 1;
  });
}

PLUGIN_API void XPluginStop(void) {
  xn::guarded("XPluginStop", [] { xn::App::get().stop(); });
}

PLUGIN_API int XPluginEnable(void) {
  return xn::guarded("XPluginEnable", 0, [] {
    xn::App::get().enable();
    return 1;
  });
}

PLUGIN_API void XPluginDisable(void) {
  xn::guarded("XPluginDisable", [] { xn::App::get().disable(); });
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID, int msg, void* param) {
  xn::guarded("XPluginReceiveMessage",
              [&] { xn::App::get().onMessage(msg, param); });
}
