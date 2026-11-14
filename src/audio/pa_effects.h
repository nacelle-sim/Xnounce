/// @file
/// The sound of the cabin PA system (VP_SPEC § 4.6): even loudness; an older
/// PA's small loudspeaker band and resonance, an amplifier driven into soft
/// clipping, hiss, mains hum, crackle, short dropouts and a burst of feedback
/// as the handset keys; and the cabin's acoustics (body, the reflections of
/// ceiling speakers spread along the cabin, a small room) in stereo. Pure C++
/// on 16-bit samples; it runs on the decode worker.
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

/// The aged copy of mono `in`. Modern returns `in` unchanged. The level is
/// matched to the input's through a soft limiter (never hard clipping), and the
/// same seed gives the same output.
/// @param fullBody The speaker's band reaches lower (Worn 170 Hz, Old 280 Hz
///        instead of 250 and 420), for the cabin acoustics.
std::vector<int16_t> agePa(const std::vector<int16_t>& in, int rate,
                           PaPreset preset, uint64_t seed,
                           bool fullBody = false);

/// The loudness of mono `pcm` in dBFS: a gated mean square, weighted like
/// hearing (low frequencies count less, the presence range more), over 400 ms
/// blocks. -100 for silence.
double loudnessDbfs(const std::vector<int16_t>& pcm, int rate);
/// Brings mono `pcm` to `targetDbfs` (VP_SPEC § 4.6): announcements -18,
/// music -20. The change is held within -20 dB and +15 dB, and peaks are
/// rounded off by a soft limiter.
void normalizeLoudness(std::vector<int16_t>& pcm, int rate, double targetDbfs);

/// The cabin's acoustics on mono `in` (VP_SPEC § 4.6): some body below about
/// 220 Hz, the short reflections of ceiling speakers spread along the cabin,
/// and a small room of about 0.5 s. Returns interleaved stereo (left, right)
/// at the input's loudness, through a soft limiter.
std::vector<int16_t> cabinAcoustics(const std::vector<int16_t>& in, int rate);

/// A soft limiter on a sample in -1…1 and beyond: unchanged up to 0.7, then
/// rounded towards 1, which it never reaches.
double softLimit(double x);

}  // namespace xn
