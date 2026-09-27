// HUD hotbar tests: item sprites, counts, damage bars.

#include <catch2/catch_test_macros.hpp>

#include "gui/font.hpp"
#include "gui/hud.hpp"
#include "render/texture.hpp"

namespace {

TEST_CASE("hotbar items emit sprites, counts and bars", "[hud]") {
  craftpp::render::Image img;
  std::string err;
  REQUIRE(craftpp::render::load_png("assets/font/default.png", img, err));
  craftpp::gui::Font font;
  REQUIRE(font.load_glyphs(img.rgba.data(), img.width, img.height));
  REQUIRE(font.load_allowed("assets/font.txt"));

  craftpp::gui::HudState hs;
  hs.hotbar[0] = {4, 32, 0, 0};     // cobble block sprite
  hs.hotbar[1] = {278, 1, 33, 250};  // diamond pickaxe, damaged
  hs.hotbar[2] = {319, 5, 0, 0};     // pork, count text
  const auto hud = craftpp::gui::build_hud(hs, font);
  // One isometric cube (6 faces) for the cobble block, sprites for items.
  CHECK(hud.blocks.vertices.size() == 6 * 4);
  CHECK(hud.blocks.indices.size() == 6 * 6);
  // Cube fits slot 0 exactly (x 339..355, y 461..477 at 854x480).
  for (const auto& v : hud.blocks.vertices) {
    CHECK(v.x >= 339.0F);
    CHECK(v.x <= 355.0F);
    CHECK(v.y >= 461.0F);
    CHECK(v.y <= 477.0F);
  }
  // Count "32" and "5" in the text mesh (with shadows).
  CHECK(hud.text.vertices.size() == 3 * 4);
  CHECK(hud.shadow.vertices.size() == 3 * 4);
  // Damage bar: 3 quads (bg + mid + fg).
  CHECK(hud.bars.vertices.size() == 3 * 4);
  // Block sprite UV: cobble tile 16 -> u0 = 0/256? tile 16: tx=0, ty=16.
  bool found = false;
  for (const auto& v : hud.blocks.vertices) {
    if (v.u == 0.0F && v.v == 16.0F / 256.0F) {
      found = true;
      break;
    }
  }
  CHECK(found);
}

}  // namespace
