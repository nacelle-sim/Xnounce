/// @file
/// Reading small text files.
#pragma once

#include <cstddef>
#include <string>

namespace xn {

/// The content of a file, or an empty string when it is missing, not a
/// regular file or larger than `maxBytes`. `path` is UTF-8. Never throws.
std::string readTextFile(const std::string& path, size_t maxBytes = 4u << 20);

}  // namespace xn
