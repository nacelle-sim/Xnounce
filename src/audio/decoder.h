/// @file
/// Decoding Ogg Vorbis files into mono 16-bit PCM, with the limits that
/// protect the plugin from bad pack files (docs/ARCHITECTURE.md § 4, rule 5).

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace xn {

struct PcmBuffer {
  std::vector<int16_t> samples;  ///< Mono.
  std::vector<int16_t> muffled;
  int rate = 44100;
  double seconds() const {
    return rate > 0 ? static_cast<double>(samples.size()) / rate : 0.0;
  }
  size_t bytes() const {
    return (samples.size() + muffled.size()) * sizeof(int16_t);
  }
};

struct DecodeLimits {
  uint64_t maxFileBytes = 256ull << 20;
  double maxSeconds = 30 * 60;
  int minRate = 8000, maxRate = 192000;
  int maxChannels = 8;
};

struct DecodeResult {
  std::shared_ptr<PcmBuffer> pcm;  ///< Null when decoding failed.
  std::string error;
};

DecodeResult decodeOgg(const std::string& utf8Path,
                       const DecodeLimits& limits = {});

/// Decodes an uncompressed PCM WAV file (8 or 16 bit, any channel count up to
/// the limit) into mono 16-bit PCM. Used for X-Plane's own sounds, such as the
/// 737's cabin chime.
DecodeResult decodeWav(const std::string& utf8Path,
                       const DecodeLimits& limits = {});
/// The same from the file's bytes. Every size in the file is checked against
/// the bytes there are, so a damaged file gives an error, never a crash.
DecodeResult decodeWavBytes(const std::vector<uint8_t>& bytes,
                            const DecodeLimits& limits = {});

}  // namespace xn
