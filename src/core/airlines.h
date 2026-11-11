/// @file
/// The airline table: IATA to ICAO codes for flight numbers, and airline names
/// to ICAO codes for livery names (VP_SPEC § 1.4).
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace xn {

struct Airline {
  std::string icao;
  std::string iata;
  std::vector<std::string> names;  ///< Names, normalized for matching.
  std::string displayName;         ///< The name as the window shows it.
  std::vector<std::string> regPrefixes;
};

class AirlineTable {
 public:
  /// The table compiled in from `data/airlines.tsv`.
  static AirlineTable builtin();
  /// Parses a table in the same format. Lines it can't read are skipped.
  static AirlineTable parse(std::string_view tsv);
  void prepend(const AirlineTable& front);

  /// The airline's name for an ICAO code, or empty when unknown.
  std::string nameFor(std::string_view icao) const;
  std::vector<std::string> icaoForIata(std::string_view iata) const;
  /// The ICAO codes an airline name in a livery name points to: the longest
  /// name first, then one whose registration prefix matches, then table
  /// order.
  std::vector<std::string> icaoForLiveryName(std::string_view liveryName) const;

  size_t size() const { return rows_.size(); }

 private:
  std::vector<Airline> rows_;
};

std::string normalizeWords(std::string_view s);
/// The aircraft registrations in a text ("G-UZHH"), upper case.
std::vector<std::string> findRegistrations(std::string_view s);

}  // namespace xn
