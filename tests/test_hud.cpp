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
  // One 16x16 sprite per slotted item (items + blocks meshes).
  CHECK(hud.items.vertices.size() == 2 * 4);
  CHECK(hud.blocks.vertices.size() == 1 * 4);
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
