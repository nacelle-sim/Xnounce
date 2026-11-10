/// @file
/// A small ini reader and writer.
///
/// Used for the settings file (`Xnounce.ini`), the central livery mapping
/// (`AirlinePacks/liveries.ini`), a livery's `airlinepack.cfg` and other
/// aircraft configuration files in the same format.
#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace xn {

/// An ini file in memory. Sections and keys keep their file order and are
/// matched without regard to case. Keys before the first section belong to
/// the section named "". Lines starting with `#` or `;` are comments, and so
/// is the rest of a value after ` #` or ` ;`.
class Ini {
 public:
  using Section = std::vector<std::pair<std::string, std::string>>;

  /// Parses ini text. Lines it can't read are skipped.
  static Ini parse(std::string_view text);
  /// The file text: `[section]` headings and `key = value` lines.
  std::string serialize() const;

  /// A section by name, or null.
  const Section* section(std::string_view name) const;
  /// A value, or null when the section or the key is missing.
  const std::string* get(std::string_view sect, std::string_view key) const;
  /// Sets a value, adding the section and the key when they are new.
  void set(std::string_view sect, std::string_view key, std::string value);
  const std::vector<std::pair<std::string, Section>>& sections() const {
    return sections_;
  }

 private:
  Section& sectionMut(std::string_view name);
  std::vector<std::pair<std::string, Section>> sections_;
};

/// Reads "1/yes/true/on" and "0/no/false/off" in any case. Anything else
/// gives `fallback`.
bool parseBool(std::string_view v, bool fallback);

}  // namespace xn
