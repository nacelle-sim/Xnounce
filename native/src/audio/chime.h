// cockpit buzzer pcm at rate
#pragma once

#include <memory>

#include "audio/decoder.h"

namespace xn {

std::shared_ptr<PcmBuffer> makeCockpitBuzzer(int rate = 22050);

}  // namespace xn
