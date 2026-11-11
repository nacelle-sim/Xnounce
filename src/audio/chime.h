/// @file
/// The cockpit call buzzer, generated in code (VP_SPEC § 4.3, D-7).
#pragma once

#include <memory>

#include "audio/decoder.h"

namespace xn {

std::shared_ptr<PcmBuffer> makeCockpitBuzzer(int rate = 22050);

}  // namespace xn
