/// @file
/// One Airline Pack file and its name, `Type[tag]…[tag]~Display Name.ogg`
/// (VP_SPEC § 1.2, § 3.1).
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/announcement.h"

namespace xn {

struct SoundFile {
  std::string path;
  std::string relPath;  ///< Path inside the pack, '/'-separated.
  std::string typeName;
  std::optional<AnnType>
      type;  ///< Empty for a custom (FAP-only) sound: never played.
  CallContext context = CallContext::Any;
  bool legacy = false;
  std::string displayName;

  // The tags by category (VP_SPEC § 3.1).
  std::vector<std::string> aircraft;  ///< Aircraft tags, upper case.
  bool refuel = false;
  std::vector<TimeOfDay> times;
  std::vector<Phase> phases;
  std::optional<Mode> mode;
  bool overwater = false;
  std::vector<int> variants;  ///< Variant numbers; none means variant 0.
  std::vector<std::string> custom;
  std::vector<std::string> rawTags;
  std::vector<std::string> warnings;  ///< Problems in the name: stray text,
                                      ///< empty tags.

  Mode effectiveMode() const {
    return mode ? *mode : (type ? defaultMode(*type) : Mode::Manual);
  }
  bool hasVariant(int v) const;
};

/// Parses a file name; the extension is optional. Nothing when the name has
/// no type.
std::optional<SoundFile> parseSoundFileName(std::string_view fileName);

/// The tags written out again, for the window and the log.
std::string describeTags(const SoundFile& f);

}  // namespace xn
