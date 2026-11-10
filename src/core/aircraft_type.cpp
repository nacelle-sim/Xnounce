#include "core/aircraft_type.h"

#include <cctype>

#include "core/strings.h"

namespace xn {

bool isAircraftTypeTag(std::string_view tag) {
  const std::string t = toUpper(trim(tag));
  const size_t n = t.size();
  if (n < 3 || n > 4) return false;
  auto digit = [&](size_t k) {
    return k < n && std::isdigit(static_cast<unsigned char>(t[k]));
  };
  auto alnum = [&](size_t k) {
    return k < n && std::isalnum(static_cast<unsigned char>(t[k]));
  };
  // A letter, two digits and an optional letter or digit: A320, A21N, B38M,
  // E75L.
  if ((t[0] == 'A' || t[0] == 'B' || t[0] == 'E') && digit(1) && digit(2) &&
      (n == 3 || alnum(3)))
    return true;
  // Designators that don't follow that pattern.
  static const char* prefixes[] = {"CRJ", "DH8", "AT4",  "AT7", "BCS", "MD8",
                                   "MD9", "MD1", "C919", "SU9", "A3ST"};
  for (const char* p : prefixes)
    if (t.rfind(p, 0) == 0) return true;
  return false;
}

}  // namespace xn
