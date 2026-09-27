#include "gui/lang.hpp"

#include <cstdint>
#include <fstream>
#include <sstream>

namespace craftpp::gui {

std::map<std::string, std::string> load_lang(const std::string& path) {
  std::map<std::string, std::string> out;
  std::ifstream in(path);
  if (!in) return out;
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty() || line[0] == '#') continue;
    const auto eq = line.find('=');
    if (eq == std::string::npos) continue;
    out[line.substr(0, eq)] = line.substr(eq + 1);
  }
  return out;
}

std::string tr(const std::map<std::string, std::string>& lang, const std::string& key) {
  auto it = lang.find(key);
  return it == lang.end() ? key : it->second;
}

long java_string_hash(const std::string& utf8) {
  // Decode to UTF-16 code units (BMP only + surrogate pairs), then h=31h+c.
  std::int32_t h = 0;
  for (std::size_t i = 0; i < utf8.size();) {
    const auto c = static_cast<unsigned char>(utf8[i]);
    std::uint32_t cp = 0xFFFDu;
    std::size_t n = 1;
    if (c < 0x80) {
      cp = c;
    } else if ((c & 0xE0) == 0xC0 && i + 1 < utf8.size()) {
      cp = ((c & 0x1F) << 6) | (utf8[i + 1] & 0x3F);
      n = 2;
    } else if ((c & 0xF0) == 0xE0 && i + 2 < utf8.size()) {
      cp = ((c & 0x0F) << 12) | ((utf8[i + 1] & 0x3F) << 6) | (utf8[i + 2] & 0x3F);
      n = 3;
    } else if ((c & 0xF8) == 0xF0 && i + 3 < utf8.size()) {
      cp = ((c & 0x07) << 18) | ((utf8[i + 1] & 0x3F) << 12) | ((utf8[i + 2] & 0x3F) << 6) |
           (utf8[i + 3] & 0x3F);
      n = 4;
    }
    if (cp < 0x10000) {
      h = 31 * h + static_cast<std::int32_t>(cp);
    } else {
      const std::uint32_t v = cp - 0x10000;
      h = 31 * h + static_cast<std::int32_t>(0xD800 + (v >> 10));
      h = 31 * h + static_cast<std::int32_t>(0xDC00 + (v & 0x3FF));
    }
    i += n;
  }
  return h;
}

}  // namespace craftpp::gui
