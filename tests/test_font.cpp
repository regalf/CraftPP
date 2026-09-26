// FontRenderer tests: widths from real pixels, § palette, layout.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <string>
#include <vector>

#include "gui/font.hpp"
#include "render/texture.hpp"

namespace {

std::string assets(const char* f) { return std::string("assets/") + f; }

TEST_CASE("font widths match pixel measurement", "[font]") {
  craftpp::render::Image img;
  std::string err;
  REQUIRE(craftpp::render::load_png(assets("font/default.png").c_str(), img, err));
  craftpp::gui::Font font;
  REQUIRE(font.load_glyphs(img.rgba.data(), img.width, img.height));
  REQUIRE(font.load_allowed(assets("font.txt")));
  CHECK(font.widths[32] == 4);  // space
  CHECK(font.glyph_index(' ') == 32);
  CHECK(font.glyph_index('A') > 32);
  // 'A' width: rightmost lit column + 2 (measured independently here).
  const int g = font.glyph_index('A');
  const int cx = g % 16, cy = g / 16;
  int last = -1;
  for (int col = 7; col >= 0; --col) {
    bool lit = false;
    for (int row = 0; row < 8 && !lit; ++row) {
      if (img.rgba[((cy * 8 + row) * 128 + cx * 8 + col) * 4 + 3] > 0) lit = true;
    }
    if (lit) {
      last = col;
      break;
    }
  }
  CHECK(font.widths[g] == last + 2);
  // Hello width = sum of advances.
  int expect = 0;
  for (char c : std::string("Hello")) expect += font.widths[font.glyph_index(c)];
  CHECK(font.string_width("Hello") == expect);
  // § codes take no space.
  CHECK(font.string_width("\xC2\xA7" "aHello") == expect);
}

TEST_CASE("font palette matches 1.0 color lists", "[font]") {
  float pal[32][3];
  craftpp::gui::Font::palette(pal);
  // white (§f = 15): r=(0>>3&1)*85=0... compute: i=15: r=85? (15>>3&1)=1*85=85;
  // g=(15>>2&1)*170+85=255; b=(15>>1&1)*170+85=255; rr=(15&1)*170+85=255.
  CHECK(pal[15][0] == Catch::Approx(1.0F));
  CHECK(pal[15][1] == Catch::Approx(1.0F));
  CHECK(pal[15][2] == Catch::Approx(1.0F));
  // shadow white (31): quartered (63/255).
  CHECK(pal[31][0] == Catch::Approx(63.0F / 255.0F).margin(0.001));
  // red (§c = 12): vanilla #FF5555.
  CHECK(pal[12][0] == Catch::Approx(1.0F));
  CHECK(pal[12][1] == Catch::Approx(85.0F / 255.0F).margin(0.001));
  CHECK(pal[12][2] == Catch::Approx(85.0F / 255.0F).margin(0.001));
}

TEST_CASE("build_text emits one quad per glyph", "[font]") {
  craftpp::render::Image img;
  std::string err;
  REQUIRE(craftpp::render::load_png(assets("font/default.png").c_str(), img, err));
  craftpp::gui::Font font;
  REQUIRE(font.load_glyphs(img.rgba.data(), img.width, img.height));
  REQUIRE(font.load_allowed(assets("font.txt")));
  const auto m = font.build_text("Hi!", 10.0F, 20.0F, 0xFFFFFFFF, false);
  CHECK(m.vertices.size() == 3 * 4);
  CHECK(m.indices.size() == 3 * 6);
  // First glyph x starts at 10, advances accumulate.
  CHECK(m.vertices[0].x == Catch::Approx(10.0F));
  const int w0 = font.widths[font.glyph_index('H')];
  CHECK(m.vertices[4].x == Catch::Approx(10.0F + w0));
}

}  // namespace
