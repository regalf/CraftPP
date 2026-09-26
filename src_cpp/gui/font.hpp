#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "render/mesh.hpp"

namespace craftpp::gui {

// CPU-side port of FontRenderer (1.0): glyph widths measured from
// font/default.png alpha (rightmost lit column + 2, space = 4), 16x16
// cells of 8px on the 128x128 atlas, advance = width. §0-f color codes
// with the 32-entry palette (upper 16 = shadow pass, pre-darkened).
// §k (obfuscate) renders a same-width substitute glyph. Text is UTF-8;
// font.txt code points map to glyphs (index = position + 32).
struct Font {
  // allowedCharacters code points from font.txt (glyph index = pos + 32).
  std::vector<std::uint32_t> allowed_cps;
  int widths[256] = {};

  // Measures widths from the loaded font image (RGBA, 128x128).
  bool load_glyphs(const std::uint8_t* rgba, int w, int h);
  bool load_allowed(const std::string& font_txt);

  int glyph_index(std::uint32_t cp) const;  // -1 when not drawable
  int string_width(const std::string& text) const;  // § codes take no space

  // Builds screen-space quads (x right, y down) for the string at (x, y)
  // with an ARGB base color. When shadow=true the darkened palette is used
  // directly (the caller draws the normal pass on top, offset by (-1,-1)
  // like drawStringWithShadow).
  render::Mesh build_text(const std::string& text, float x, float y, std::uint32_t argb,
                          bool shadow) const;

  // 32-entry § palette, 0-15 normal, 16-31 shadow (mirrors the GL lists).
  static void palette(float (&rgb)[32][3]);
};

}  // namespace craftpp::gui
