#include "core/ini.h"

#include "core/strings.h"

namespace xn {

Ini Ini::parse(std::string_view text) {
  Ini ini;
  std::string current;
  size_t pos = 0;
  // Skip a UTF-8 byte order mark.
  if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
      static_cast<unsigned char>(text[1]) == 0xBB &&
      static_cast<unsigned char>(text[2]) == 0xBF)
    pos = 3;
  while (pos <= text.size()) {
    size_t end = text.find('\n', pos);
    if (end == std::string_view::npos) end = text.size();
    std::string_view line = trim(text.substr(pos, end - pos));
    pos = end + 1;
    if (line.empty() || line[0] == '#' || line[0] == ';') continue;
    if (line.front() == '[' && line.back() == ']') {
      current = std::string(trim(line.substr(1, line.size() - 2)));
      ini.sectionMut(current);
      continue;
    }
    const size_t eq = line.find('=');
    if (eq == std::string_view::npos) continue;
    std::string key(trim(line.substr(0, eq)));
    std::string_view value = trim(line.substr(eq + 1));
    // Drop an inline comment: " #" or " ;" and what follows.
    for (size_t k = 1; k < value.size(); ++k) {
      if ((value[k] == '#' || value[k] == ';') &&
          (value[k - 1] == ' ' || value[k - 1] == '\t')) {
        value = trim(value.substr(0, k));
        break;
      }
    }
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
      value = value.substr(1, value.size() - 2);
    if (!key.empty()) ini.set(current, key, std::string(value));
    if (end == text.size()) break;
  }
  return ini;
}

std::string Ini::serialize() const {
  std::string out;
  for (const auto& [name, sect] : sections_) {
    if (sect.empty() && name.empty()) continue;
    if (!name.empty()) {
      if (!out.empty()) out += '\n';
      out += "[" + name + "]\n";
    }
    for (const auto& [k, v] : sect) out += k + " = " + v + "\n";
  }
  return out;
}

const Ini::Section* Ini::section(std::string_view name) const {
  for (const auto& [n, s] : sections_)
    if (iequals(n, name)) return &s;
  return nullptr;
}

const std::string* Ini::get(std::string_view sect, std::string_view key) const {
  const Section* s = section(sect);
  if (!s) return nullptr;
  for (const auto& [k, v] : *s)
    if (iequals(k, key)) return &v;
  return nullptr;
}

Ini::Section& Ini::sectionMut(std::string_view name) {
  for (auto& [n, s] : sections_)
    if (iequals(n, name)) return s;
  sections_.emplace_back(std::string(name), Section{});
  return sections_.back().second;
}

void Ini::set(std::string_view sect, std::string_view key, std::string value) {
  Section& s = sectionMut(sect);
  for (auto& [k, v] : s)
    if (iequals(k, key)) {
      v = std::move(value);
      return;
    }
  s.emplace_back(std::string(key), std::move(value));
}

bool parseBool(std::string_view v, bool fallback) {
  v = trim(v);
  if (iequals(v, "1") || iequals(v, "yes") || iequals(v, "true") ||
      iequals(v, "on"))
    return true;
  if (iequals(v, "0") || iequals(v, "no") || iequals(v, "false") ||
      iequals(v, "off"))
    return false;
  return fallback;
}

}  // namespace xn
