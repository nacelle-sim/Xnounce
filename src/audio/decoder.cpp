/// @file
/// Implements `src/audio/decoder.h`; the interface is documented there.
#include "audio/decoder.h"

#define OV_EXCLUDE_STATIC_CALLBACKS
#include <vorbis/vorbisfile.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace xn {

namespace {

size_t readCb(void* ptr, size_t size, size_t n, void* src) {
  return std::fread(ptr, size, n, static_cast<FILE*>(src));
}

int seekCb(void* src, ogg_int64_t off, int whence) {
#if defined(_WIN32)
  return _fseeki64(static_cast<FILE*>(src), off, whence);
#else
  return fseeko(static_cast<FILE*>(src), static_cast<off_t>(off), whence);
#endif
}

long tellCb(void* src) {
#if defined(_WIN32)
  return static_cast<long>(_ftelli64(static_cast<FILE*>(src)));
#else
  return static_cast<long>(ftello(static_cast<FILE*>(src)));
#endif
}

struct FileCloser {
  void operator()(FILE* f) const {
    if (f) std::fclose(f);
  }
};

}  // namespace

DecodeResult decodeOgg(const std::string& utf8Path, const DecodeLimits& lim) {
  DecodeResult r;
  std::error_code ec;
  const auto path = std::filesystem::u8path(utf8Path);
  const uint64_t size = std::filesystem::file_size(path, ec);
  if (ec) {
    r.error = "cannot read file";
    return r;
  }
  if (size > lim.maxFileBytes) {
    r.error = "file too large";
    return r;
  }
#if defined(_WIN32)
  std::unique_ptr<FILE, FileCloser> file(
      _wfopen(path.wstring().c_str(), L"rb"));
#else
  std::unique_ptr<FILE, FileCloser> file(
      std::fopen(path.string().c_str(), "rb"));
#endif
  if (!file) {
    r.error = "cannot open file";
    return r;
  }
  OggVorbis_File vf;
  ov_callbacks cb{readCb, seekCb, nullptr, tellCb};  // the file is closed here
  if (ov_open_callbacks(file.get(), &vf, nullptr, 0, cb) != 0) {
    r.error = "not an Ogg Vorbis file";
    return r;
  }
  struct VfGuard {
    OggVorbis_File* v;
    ~VfGuard() { ov_clear(v); }
  } guard{&vf};

  const vorbis_info* info = ov_info(&vf, -1);
  if (!info || info->channels < 1 || info->channels > lim.maxChannels ||
      info->rate < lim.minRate || info->rate > lim.maxRate) {
    r.error = "unsupported channels or sample rate";
    return r;
  }
  const int channels = info->channels;
  const int rate = static_cast<int>(info->rate);
  const ogg_int64_t total = ov_pcm_total(&vf, -1);
  const double maxSamples = lim.maxSeconds * rate;
  if (total > 0 && static_cast<double>(total) > maxSamples) {
    r.error = "longer than the limit";
    return r;
  }

  auto pcm = std::make_shared<PcmBuffer>();
  pcm->rate = rate;
  if (total > 0) pcm->samples.reserve(static_cast<size_t>(total));
  int section = 0;
  int holes = 0;
  while (true) {
    float** data = nullptr;
    const long n = ov_read_float(&vf, &data, 4096, &section);
    if (n == 0) break;
    if (n == OV_HOLE) {  // a gap the decoder recovers from
      if (++holes > 100) {
        r.error = "stream too damaged";
        return r;
      }
      continue;
    }
    if (n < 0) {
      r.error = "decode error";
      return r;
    }
    const vorbis_info* cur = ov_info(&vf, section);
    const int ch = cur ? std::min(cur->channels, channels) : channels;
    for (long k = 0; k < n; ++k) {
      float mix = 0;
      for (int c = 0; c < ch; ++c) mix += data[c][k];
      mix /= static_cast<float>(ch);
      if (!std::isfinite(mix)) mix = 0;
      mix = std::clamp(mix, -1.0f, 1.0f);
      pcm->samples.push_back(static_cast<int16_t>(std::lrint(mix * 32767.0f)));
    }
    if (static_cast<double>(pcm->samples.size()) > maxSamples) {
      r.error = "longer than the limit";
      return r;
    }
  }
  if (pcm->samples.empty()) {
    r.error = "no audio";
    return r;
  }
  r.pcm = std::move(pcm);
  return r;
}

namespace {

uint32_t le32(const std::vector<uint8_t>& b, size_t at) {
  return static_cast<uint32_t>(b[at]) | static_cast<uint32_t>(b[at + 1]) << 8 |
         static_cast<uint32_t>(b[at + 2]) << 16 |
         static_cast<uint32_t>(b[at + 3]) << 24;
}

uint16_t le16(const std::vector<uint8_t>& b, size_t at) {
  return static_cast<uint16_t>(b[at] | b[at + 1] << 8);
}

}  // namespace

DecodeResult decodeWavBytes(const std::vector<uint8_t>& b,
                            const DecodeLimits& lim) {
  DecodeResult r;
  if (b.size() < 12 || std::memcmp(b.data(), "RIFF", 4) != 0 ||
      std::memcmp(b.data() + 8, "WAVE", 4) != 0) {
    r.error = "not a WAV file";
    return r;
  }
  // Walk the chunks: "fmt " says how the samples are stored, "data" holds
  // them. Each chunk is padded to an even size.
  int format = 0, channels = 0, bits = 0;
  uint32_t rate = 0;
  size_t dataAt = 0, dataBytes = 0;
  bool haveFmt = false, haveData = false;
  for (size_t at = 12; at + 8 <= b.size();) {
    const uint32_t size = le32(b, at + 4);
    const size_t body = at + 8;
    if (size > b.size() - body) {
      // A data chunk cut short at the end of the file still plays.
      if (std::memcmp(b.data() + at, "data", 4) == 0) {
        dataAt = body;
        dataBytes = b.size() - body;
        haveData = true;
      }
      break;
    }
    if (std::memcmp(b.data() + at, "fmt ", 4) == 0 && size >= 16) {
      format = le16(b, body);
      channels = le16(b, body + 2);
      rate = le32(b, body + 4);
      bits = le16(b, body + 14);
      haveFmt = true;
    } else if (std::memcmp(b.data() + at, "data", 4) == 0) {
      dataAt = body;
      dataBytes = size;
      haveData = true;
    }
    at = body + size + (size & 1);
  }
  if (!haveFmt || !haveData) {
    r.error = "WAV file without format or data";
    return r;
  }
  if (format != 1 || (bits != 8 && bits != 16)) {
    r.error = "WAV file is not 8- or 16-bit PCM";
    return r;
  }
  if (channels < 1 || channels > lim.maxChannels) {
    r.error = "unsupported channel count";
    return r;
  }
  if (rate < static_cast<uint32_t>(lim.minRate) ||
      rate > static_cast<uint32_t>(lim.maxRate)) {
    r.error = "unsupported sample rate";
    return r;
  }
  const size_t frameBytes = static_cast<size_t>(channels) * (bits / 8);
  const size_t frames = dataBytes / frameBytes;
  if (frames == 0) {
    r.error = "WAV file has no samples";
    return r;
  }
  if (static_cast<double>(frames) / rate > lim.maxSeconds) {
    r.error = "sound too long";
    return r;
  }
  auto pcm = std::make_shared<PcmBuffer>();
  pcm->rate = static_cast<int>(rate);
  pcm->samples.resize(frames);
  for (size_t f = 0; f < frames; ++f) {
    long sum = 0;
    for (int c = 0; c < channels; ++c) {
      const size_t at =
          dataAt + f * frameBytes + static_cast<size_t>(c) * (bits / 8);
      sum += bits == 16 ? static_cast<int16_t>(le16(b, at))
                        : (static_cast<long>(b[at]) - 128) * 256;
    }
    pcm->samples[f] = static_cast<int16_t>(sum / channels);
  }
  r.pcm = std::move(pcm);
  return r;
}

DecodeResult decodeWav(const std::string& utf8Path, const DecodeLimits& lim) {
  DecodeResult r;
  std::error_code ec;
  const auto path = std::filesystem::u8path(utf8Path);
  const uint64_t size = std::filesystem::file_size(path, ec);
  if (ec) {
    r.error = "cannot read file";
    return r;
  }
  if (size > lim.maxFileBytes) {
    r.error = "file too large";
    return r;
  }
  std::ifstream in(path, std::ios::binary);
  std::vector<uint8_t> bytes(static_cast<size_t>(size));
  if (!in.read(reinterpret_cast<char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()))) {
    r.error = "cannot read file";
    return r;
  }
  return decodeWavBytes(bytes, lim);
}

}  // namespace xn
