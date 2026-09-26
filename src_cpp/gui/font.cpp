#include "gui/font.hpp"

#include <cctype>
#include <fstream>
#include <sstream>

namespace craftpp::gui {

namespace {

// Minimal UTF-8 decoder (overlong/invalid -> U+FFFD, like Java's decoder).
std::vector<std::uint32_t> decode_utf8(const std::string& s) {
  std::vector<std::uint32_t> out;
  for (std::size_t i = 0; i < s.size();) {
    const auto c = static_cast<unsigned char>(s[i]);
    std::uint32_t cp = 0xFFFDu;
    std::size_t n = 1;
    if (c < 0x80) {
      cp = c;
    } else if ((c & 0xE0) == 0xC0 && i + 1 < s.size()) {
      cp = ((c & 0x1F) << 6) | (s[i + 1] & 0x3F);
      n = 2;
    } else if ((c & 0xF0) == 0xE0 && i + 2 < s.size()) {
      cp = ((c & 0x0F) << 12) | ((s[i + 1] & 0x3F) << 6) | (s[i + 2] & 0x3F);
      n = 3;
    } else if ((c & 0xF8) == 0xF0 && i + 3 < s.size()) {
      cp = ((c & 0x07) << 18) | ((s[i + 1] & 0x3F) << 12) | ((s[i + 2] & 0x3F) << 6) |
           (s[i + 3] & 0x3F);
      n = 4;
    }
    out.push_back(cp);
    i += n;
  }
  return out;
}

}  // namespace

bool Font::load_glyphs(const std::uint8_t* rgba, int w, int h) {
  if (rgba == nullptr || w != 128 || h != 128) return false;
  for (int g = 0; g < 256; ++g) {
    const int cx = g % 16, cy = g / 16;
    int last = -1;
    for (int col = 7; col >= 0; --col) {
      bool lit = false;
      for (int row = 0; row < 8 && !lit; ++row) {
        const int px = cx * 8 + col, py = cy * 8 + row;
        if (rgba[(py * w + px) * 4 + 3] > 0) lit = true;
      }
      if (lit) {
        last = col;
        break;
      }
    }
    widths[g] = (g == 32) ? 4 : last + 2;
  }
  return true;
}

bool Font::load_allowed(const std::string& font_txt) {
  std::ifstream in(font_txt, std::ios::binary);
  if (!in) return false;
  std::ostringstream ss;
  ss << in.rdbuf();
  // Mirrors ChatAllowedCharacters: UTF-8 lines, '#' comments and line
  // breaks dropped, code points concatenated.
  allowed_cps.clear();
  std::string line;
  std::istringstream lines(ss.str());
  while (std::getline(lines, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (!line.empty() && line[0] == '#') continue;
    const auto cps = decode_utf8(line);
    allowed_cps.insert(allowed_cps.end(), cps.begin(), cps.end());
  }
  return !allowed_cps.empty();
}

int Font::glyph_index(std::uint32_t cp) const {
  for (std::size_t i = 0; i < allowed_cps.size(); ++i) {
    if (allowed_cps[i] == cp) return static_cast<int>(i) + 32;
  }
  return -1;
}

int Font::string_width(const std::string& text) const {
  // Single pass with § lookahead (codes take no space).
  int w = 0;
  const auto cps = decode_utf8(text);
  for (std::size_t i = 0; i < cps.size(); ++i) {
    if (cps[i] == 0xA7 && i + 1 < cps.size()) {
      ++i;
    } else {
      const int g = glyph_index(cps[i]);
      if (g >= 0) w += widths[g];
    }
  }
  return w;
}

void Font::palette(float (&rgb)[32][3]) {
  for (int i = 0; i < 32; ++i) {
    const int base = ((i >> 3) & 1) * 85;
    int r = ((i >> 2) & 1) * 170 + base;
    int g = ((i >> 1) & 1) * 170 + base;
    int b = (i & 1) * 170 + base;
    if (i == 6) r += 85;
    if (i >= 16) {
      r /= 4;
      g /= 4;
      b /= 4;
    }
    rgb[i][0] = r / 255.0F;
    rgb[i][1] = g / 255.0F;
    rgb[i][2] = b / 255.0F;
  }
}

render::Mesh Font::build_text(const std::string& text, float x, float y, std::uint32_t argb,
                              bool shadow) const {
  render::Mesh m;
  float pal[32][3];
  palette(pal);
  int color = 15;
  bool obf = false;
  float cx = x;
  const float cr = ((argb >> 16) & 255) / 255.0F;
  const float cg = ((argb >> 8) & 255) / 255.0F;
  const float cb = (argb & 255) / 255.0F;
  const auto cps = decode_utf8(text);
  std::size_t obf_cursor = 0;
  for (std::size_t i = 0; i < cps.size(); ++i) {
    if (cps[i] == 0xA7 && i + 1 < cps.size()) {
      const std::uint32_t code = cps[i + 1] | 0x20u;  // toLower, ASCII only
      i += 2;
      if (code == 'k') {
        obf = true;
        continue;
      }
      obf = false;
      if (code >= '0' && code <= '9') {
        color = code - '0';
      } else if (code >= 'a' && code <= 'f') {
        color = code - 'a' + 10;
      } else {
        color = 15;
      }
      continue;
    }
    int g = glyph_index(cps[i]);
    if (g < 0) continue;
    if (obf) {
      // Same-width substitute (the source picks a random one; we scan
      // forward deterministically from a rotating cursor).
      for (std::size_t k = 0; k < allowed_cps.size(); ++k) {
        obf_cursor = (obf_cursor + 1) % allowed_cps.size();
        const int cand = static_cast<int>(obf_cursor) + 32;
        if (widths[cand] == widths[g]) {
          g = cand;
          break;
        }
      }
    }
    const float r = (color == 15 && !shadow) ? cr : pal[color + (shadow ? 16 : 0)][0];
    const float gg = (color == 15 && !shadow) ? cg : pal[color + (shadow ? 16 : 0)][1];
    const float b = (color == 15 && !shadow) ? cb : pal[color + (shadow ? 16 : 0)][2];
    const int gx = g % 16, gy = g / 16;
    const float u0 = gx * 8.0F / 128.0F, v0 = gy * 8.0F / 128.0F;
    const float u1 = (gx * 8.0F + 7.99F) / 128.0F, v1 = (gy * 8.0F + 7.99F) / 128.0F;
    const std::uint32_t base = static_cast<std::uint32_t>(m.vertices.size());
    m.vertices.push_back({cx, y + 7.99F, 0, r, gg, b, u0, v1});
    m.vertices.push_back({cx + 7.99F, y + 7.99F, 0, r, gg, b, u1, v1});
    m.vertices.push_back({cx + 7.99F, y, 0, r, gg, b, u1, v0});
    m.vertices.push_back({cx, y, 0, r, gg, b, u0, v0});
    m.indices.insert(m.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    cx += widths[g];
  }
  return m;
}

}  // namespace craftpp::gui
