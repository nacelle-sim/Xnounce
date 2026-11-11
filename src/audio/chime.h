/// @file
/// The cockpit call buzzer, generated in code (VP_SPEC § 4.3, D-7).
#pragma once

#include <memory>

#include "audio/decoder.h"

namespace xn {

std::shared_ptr<PcmBuffer> makeCockpitBuzzer(int rate = 22050);

/// A single high chime, as a Boeing cabin calls the cockpit: used when the
/// aircraft's own chime sound can't be read.
std::shared_ptr<PcmBuffer> makeSingleChime(int rate = 22050);

}  // namespace xn
