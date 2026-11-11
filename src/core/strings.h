/// @file
/// Small string helpers. ASCII only: pack, tag and dataref names are ASCII.
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace xn {

std::string toLower(std::string_view s);
std::string toUpper(std::string_view s);
bool iequals(std::string_view a, std::string_view b);
bool istartsWith(std::string_view s, std::string_view prefix);
bool icontains(std::string_view haystack, std::string_view needle);
std::string_view trim(std::string_view s);
/// Splits at any of `seps`, trims each part and drops empty parts.
std::vector<std::string> splitList(std::string_view s,
                                   std::string_view seps = ",;");
std::string join(const std::vector<std::string>& parts, std::string_view sep);
/// Matches a glob pattern: `*` is any run of characters, `?` one character.
bool globMatch(std::string_view pattern, std::string_view text);
/// A C string from a byte buffer that may lack its terminating NUL
/// (dataref byte arrays).
std::string cleanCString(const char* data, size_t maxLen);
/// The code in square brackets at the start of a name, upper case: "CFMST"
/// for "[cfmst] Lufthansa". Empty when there is none.
std::string leadingBracketCode(std::string_view name);

}  // namespace xn
