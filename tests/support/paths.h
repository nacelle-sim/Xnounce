/// @file
/// Where the tests find their data.
///
/// Data that ships with the repository lives under `tests/`. Airline Packs and
/// recorded flights are third-party or personal, so they stay outside the
/// repository in a folder the developer names with the CMake variable
/// `XN_LOCAL_DATA`. Tests that need them skip when it is not set.
#pragma once

#include <filesystem>
#include <string>

#include "paths_config.h"

namespace xn::testing {

/// Golden stories of the scripted flights (`tests/golden`).
inline std::filesystem::path goldenDir() {
  return std::filesystem::path(XN_TEST_DATA_DIR) / "golden";
}

/// A scratch folder in the build tree, created on first use.
inline std::filesystem::path tmpDir() {
  const std::filesystem::path dir(XN_TEST_TMP_DIR);
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  return dir;
}

/// The folder `<XN_LOCAL_DATA>/<sub>`. Empty when no local data is configured
/// or the folder does not exist, in which case the caller skips its test.
inline std::filesystem::path localDir(const char* sub) {
  const std::string root = XN_LOCAL_DATA_DIR;
  if (root.empty()) return {};
  const std::filesystem::path dir = std::filesystem::path(root) / sub;
  std::error_code ec;
  return std::filesystem::is_directory(dir, ec) ? dir : std::filesystem::path();
}

}  // namespace xn::testing
