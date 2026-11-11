/// @file
/// The sound of an older cabin PA system (VP_SPEC § 4.6): a small loudspeaker's
/// band and resonance, an amplifier driven into soft clipping, hiss, mains hum,
/// crackle, short dropouts and a burst of feedback as the handset keys. Pure
/// C++ on mono 16-bit samples; it runs on the decode worker.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace xn {

/// How worn the airframe's PA sounds. Modern leaves the recording as it is.
enum class PaPreset { Modern = 0, Worn = 1, Old = 2 };

/// The setting "PA system": 0 Modern, 1 Worn, 2 Old, 3 Random per airframe.
/// Random picks Modern, Worn or Old from the tail number, so an airframe keeps
/// its PA from flight to flight.
PaPreset resolvePaPreset(int setting, const std::string& tailNumber);

/// A seed for one airframe and one file, so the same file on the same airframe
/// always sounds the same, and different files differ.
uint64_t paSeed(const std::string& tailNumber, const std::string& path);

/// The aged copy of `in`. Modern returns `in` unchanged. The level is matched
/// to the input's, the output is clipped to 16 bits, and the same seed gives
/// the same output.
std::vector<int16_t> agePa(const std::vector<int16_t>& in, int rate,
                           PaPreset preset, uint64_t seed);

}  // namespace xn
