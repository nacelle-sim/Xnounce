/// @file
/// Implements `src/sim/fmod_studio.h`; the interface is documented there.
#include "sim/fmod_studio.h"

#include <XPLMUtilities.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace xn {

namespace {

/// A function of the FMOD Studio library X-Plane loaded, or null.
void* fmodSymbol(const char* name) {
#if defined(_WIN32)
  for (const char* lib : {"fmodstudio.dll", "fmodstudioL.dll"})
    if (HMODULE m = GetModuleHandleA(lib))
      if (FARPROC f = GetProcAddress(m, name))
        return reinterpret_cast<void*>(f);
  return nullptr;
#else
  // Already loaded by X-Plane: the global search finds it, else the library
  // by name, without loading anything new.
  if (void* f = dlsym(RTLD_DEFAULT, name)) return f;
  for (const char* lib : {"libfmodstudio.so.13", "libfmodstudio.so",
                          "libfmodstudio.dylib", "libfmodstudioL.dylib"})
    if (void* h = dlopen(lib, RTLD_LAZY | RTLD_NOLOAD)) {
      void* f = dlsym(h, name);
      dlclose(h);  // only drops the reference this call added
      if (f) return f;
    }
  return nullptr;
#endif
}

template <typename F>
bool bind(F& fn, const char* name, std::string* why) {
  fn = reinterpret_cast<F>(fmodSymbol(name));
  if (!fn && why && why->empty()) *why = std::string("no ") + name;
  return fn != nullptr;
}

}  // namespace

FmodApi findFmodStudio(std::string* why) {
  FmodApi api;
  if (why) why->clear();
  using GetStudio = void* (*)();
  auto get = reinterpret_cast<GetStudio>(XPLMFindSymbol("XPLMGetFMODStudio"));
  if (!get) {
    if (why) *why = "this X-Plane doesn't share its FMOD Studio";
    return api;
  }
  api.system = get();
  if (!api.system) {
    if (why) *why = "X-Plane's FMOD Studio isn't running";
    return api;
  }
  bind(api.getEvent, "FMOD_Studio_System_GetEvent", why);
  bind(api.getInstanceList, "FMOD_Studio_EventDescription_GetInstanceList",
       why);
  bind(api.setVolume, "FMOD_Studio_EventInstance_SetVolume", why);
  bind(api.getPlaybackState, "FMOD_Studio_EventInstance_GetPlaybackState", why);
  return api;
}

}  // namespace xn
