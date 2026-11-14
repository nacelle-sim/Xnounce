/// @file
/// Finds X-Plane's FMOD Studio system and the FMOD Studio C functions Xnounce
/// uses, at runtime (docs/ARCHITECTURE.md § 4): `XPLMGetFMODStudio` through
/// `XPLMFindSymbol`, the functions in the FMOD Studio library X-Plane has
/// already loaded. No FMOD header or library is part of the build.
#pragma once

#include <string>

#include "audio/fmod_events.h"

namespace xn {

/// The table, complete or not; `why` says what was missing. Main thread only.
FmodApi findFmodStudio(std::string* why = nullptr);

}  // namespace xn
