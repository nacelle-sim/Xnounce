/// @file
/// Implements `src/core/files.h`; the interface is documented there.
#include "core/files.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace xn {

std::string readTextFile(const std::string& path, size_t maxBytes) {
  std::error_code ec;
  const auto p = std::filesystem::u8path(path);
  if (!std::filesystem::is_regular_file(p, ec) ||
      std::filesystem::file_size(p, ec) > maxBytes)
    return {};
  std::ifstream in(p, std::ios::binary);
  if (!in) return {};
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

}  // namespace xn
