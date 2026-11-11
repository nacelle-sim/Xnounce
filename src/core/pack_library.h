/// @file
/// Finding the Airline Packs on disk (VP_SPEC § 1.1, § 1.2). Filesystem only:
/// it runs on the scan worker.
#pragma once

#include <atomic>
#include <string>
#include <string_view>
#include <vector>

#include "core/sound_file.h"

namespace xn {

struct Pack {
  std::string name;  ///< "DLH", "Default", "UALES"…
  std::string root;
  std::string wrapper;       ///< The folder it sits in, as "DLH OLD PA SYSTEM".
  bool mappingOnly = false;  ///< Only a livery mapping can choose it.
  std::vector<SoundFile> files;
  std::vector<std::string> warnings;
  int maxVariant = 0;

  bool isDefault() const;
  int countOf(AnnType t) const;
  std::vector<int> variantChoices() const;
  std::vector<std::string> customTypes() const;
};

struct PackLibrary {
  std::vector<Pack> packs;
  std::vector<std::string> warnings;
  std::vector<std::string> roots;

  const Pack* find(std::string_view name) const;
  const Pack* defaultPack() const { return find("Default"); }
  size_t soundCount() const;
};

/// A pack folder name: 2 to 4 letters or digits, or "Default".
bool isPackIdName(std::string_view folderName);

/// Scans the folders. A missing folder is a warning. `cancel`, which may be
/// null, stops the scan early.
PackLibrary scanPacks(const std::vector<std::string>& roots,
                      const std::atomic<bool>* cancel = nullptr);

}  // namespace xn
