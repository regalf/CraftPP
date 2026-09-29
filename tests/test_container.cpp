#include <catch2/catch_test_macros.hpp>

#include <map>

#include "entity/player.hpp"
#include "gui/container.hpp"
#include "gui/containers.hpp"
#include "gui/font.hpp"
#include "render/texture.hpp"

using craftpp::entity::Inventory;
using craftpp::entity::ItemStack;
using craftpp::gui::ctn::Kit;

namespace {
ItemStack st(int id, int n, int dmg = 0) { return ItemStack(id, n, dmg); }

// Drop sink: collects dropped stacks.
struct Sink {
  std::vector<ItemStack> got;
  craftpp::gui::ctn::DropFn fn() {
    return [this](ItemStack& s) { got.push_back(s); };
  }
};
}  // namespace

TEST_CASE("container LMB pickup and place") {
  Inventory inv;
  Kit k;
  craftpp::gui::ctn::build_player(k, inv);
  Sink sink;
  // Main slot 9 (container slot 9) gets 10 dirt (id 3).
  inv.main[9] = st(3, 10);
  auto r = k.c.click(9, 0, false, inv.cursor, sink.fn());
  REQUIRE(r.has_value());
  CHECK(r->item_id == 3);
  CHECK(r->stack_size == 10);
  REQUIRE(inv.cursor.has_value());
  CHECK(inv.cursor->stack_size == 10);
  CHECK(!inv.main[9].has_value());
  // Place into empty main slot 10 (container slot 10).
  r = k.c.click(10, 0, false, inv.cursor, sink.fn());
  CHECK(!inv.cursor.has_value());
  REQUIRE(inv.main[10].has_value());
  CHECK(inv.main[10]->stack_size == 10);
  CHECK(sink.got.empty());
}

TEST_CASE("container RMB half pickup and single place") {
  Inventory inv;
  Kit k;
  craftpp::gui::ctn::build_player(k, inv);
  Sink sink;
  inv.main[9] = st(3, 10);
  k.c.click(9, 1, false, inv.cursor, sink.fn());
  REQUIRE(inv.cursor.has_value());
  CHECK(inv.cursor->stack_size == 5);  // (10+1)/2
  CHECK(inv.main[9]->stack_size == 5);
  // Place single into empty slot.
  k.c.click(10, 1, false, inv.cursor, sink.fn());
  CHECK(inv.cursor->stack_size == 4);
  REQUIRE(inv.main[10].has_value());
  CHECK(inv.main[10]->stack_size == 1);
}

TEST_CASE("container swap on different id") {
  Inventory inv;
  Kit k;
  craftpp::gui::ctn::build_player(k, inv);
  Sink sink;
  inv.main[9] = st(3, 10);
  inv.main[10] = st(4, 5);
  k.c.click(9, 0, false, inv.cursor, sink.fn());
  k.c.click(10, 0, false, inv.cursor, sink.fn());
  REQUIRE(inv.cursor.has_value());
  CHECK(inv.cursor->item_id == 4);
  REQUIRE(inv.main[10].has_value());
  CHECK(inv.main[10]->item_id == 3);
}

TEST_CASE("container merge same id") {
  Inventory inv;
  Kit k;
  craftpp::gui::ctn::build_player(k, inv);
  Sink sink;
  inv.main[9] = st(3, 10);
  inv.main[10] = st(3, 60);
  k.c.click(9, 0, false, inv.cursor, sink.fn());
  k.c.click(10, 0, false, inv.cursor, sink.fn());
  REQUIRE(inv.main[10]->stack_size == 64);
  REQUIRE(inv.cursor.has_value());
  CHECK(inv.cursor->stack_size == 6);
}

TEST_CASE("container drop outside (-999)") {
  Inventory inv;
  Kit k;
  craftpp::gui::ctn::build_player(k, inv);
  Sink sink;
  inv.cursor = st(3, 7);
  k.c.click(-999, 0, false, inv.cursor, sink.fn());
  CHECK(!inv.cursor.has_value());
  REQUIRE(sink.got.size() == 1);
  CHECK(sink.got[0].stack_size == 7);
  // RMB drops a single.
  inv.cursor = st(3, 7);
  k.c.click(-999, 1, false, inv.cursor, sink.fn());
  REQUIRE(inv.cursor.has_value());
  CHECK(inv.cursor->stack_size == 6);
  REQUIRE(sink.got.size() == 2);
  CHECK(sink.got[1].stack_size == 1);
}

TEST_CASE("container armor validity") {
  Inventory inv;
  Kit k;
  craftpp::gui::ctn::build_player(k, inv);
  Sink sink;
  // Armor slot 5 = helm (type 0); leather helm = 298.
  inv.cursor = st(298, 1);
  k.c.click(5, 0, false, inv.cursor, sink.fn());
  CHECK(!inv.cursor.has_value());
  CHECK(inv.armor[3].has_value());
  // Chestplate (299) does not fit the helm slot.
  inv.cursor = st(299, 1);
  k.c.click(5, 0, false, inv.cursor, sink.fn());
  CHECK(inv.cursor.has_value());
  CHECK(inv.armor[3]->item_id == 298);
}

TEST_CASE("container player 2x2 craft and consume") {
  Inventory inv;
  Kit k;
  craftpp::gui::ctn::build_player(k, inv);
  Sink sink;
  // Oak planks (5) 2x2 -> crafting table (58)? Use sticks: 2 planks
  // vertical -> 4 sticks (280). Grid slots 1..4 map 2x2 corner.
  k.grid[0] = st(5, 1);
  k.grid[2] = st(5, 1);
  k.recompute();
  REQUIRE(k.result.has_value());
  CHECK(k.result->item_id == 280);
  // Take the result via the output slot (0).
  auto r = k.c.click(0, 0, false, inv.cursor, sink.fn());
  REQUIRE(r.has_value());
  REQUIRE(inv.cursor.has_value());
  CHECK(inv.cursor->item_id == 280);
  // Matrix consumed.
  CHECK(!k.grid[0].has_value());
  CHECK(!k.grid[2].has_value());
  CHECK(!k.result.has_value());
}

TEST_CASE("container shift-click routes") {
  Inventory inv;
  Kit k;
  craftpp::gui::ctn::build_player(k, inv);
  Sink sink;
  // Main slot -> hotbar: fill hotbar partially, shift from main.
  inv.main[0] = st(3, 64);  // hotbar full dirt
  inv.main[9] = st(3, 10);
  k.c.click(9, 0, true, inv.cursor, sink.fn());
  // Hotbar slot 0 full; should land in hotbar slot 1 (container 37).
  CHECK(inv.main[1].has_value());
  CHECK(inv.main[1]->stack_size == 10);
  CHECK(!inv.main[9].has_value());
}

TEST_CASE("container workbench shift output") {
  Inventory inv;
  Kit k;
  craftpp::gui::ctn::build_workbench(k, inv);
  Sink sink;
  CHECK(k.c.slots.size() == 46);
  k.grid[0] = st(5, 1);
  k.grid[3] = st(5, 1);
  k.recompute();
  REQUIRE(k.result.has_value());
  // Shift-click output -> hotbar (reverse).
  k.c.click(0, 0, true, inv.cursor, sink.fn());
  CHECK(inv.main[8].has_value());
  CHECK(inv.main[8]->item_id == 280);
}

TEST_CASE("container chest shift both ways") {
  Inventory inv;
  Kit k;
  std::array<std::optional<ItemStack>, 27> ch{};
  craftpp::gui::ctn::build_chest(k, inv, ch);
  Sink sink;
  CHECK(k.c.slots.size() == 63);
  ch[0] = st(3, 5);
  k.c.click(0, 0, true, inv.cursor, sink.fn());
  CHECK(inv.main[8].has_value());
  CHECK(!ch[0].has_value());
  // Back into the chest.
  k.c.click(54 + 8, 0, true, inv.cursor, sink.fn());
  CHECK(ch[0].has_value());
}

TEST_CASE("container furnace output invalid + shift") {
  Inventory inv;
  Kit k;
  std::array<std::optional<ItemStack>, 3> fz{};
  craftpp::gui::ctn::build_furnace(k, inv, fz);
  Sink sink;
  // Cannot place into the output slot by hand.
  inv.cursor = st(265, 1);  // iron ingot
  k.c.click(2, 0, false, inv.cursor, sink.fn());
  CHECK(inv.cursor.has_value());
  CHECK(!fz[2].has_value());
  // Shift-click output moves to inventory.
  fz[2] = st(265, 3);
  k.c.click(2, 0, true, inv.cursor, sink.fn());
  CHECK(inv.main[8].has_value());
  CHECK(!fz[2].has_value());
}

TEST_CASE("container close drops cursor and matrix") {
  Inventory inv;
  Kit k;
  craftpp::gui::ctn::build_player(k, inv);
  Sink sink;
  inv.cursor = st(3, 2);
  k.grid[0] = st(5, 1);
  k.c.close(inv.cursor, sink.fn());
  REQUIRE(sink.got.size() == 2);
  inv.cursor = std::nullopt;
  k.c.close(inv.cursor, sink.fn());
  REQUIRE(sink.got.size() == 2);  // matrix already cleared
}

TEST_CASE("container screen draw does not crash") {
  // CPU-side draw path (meshes only, no GL): panel + slots + cursor +
  // tooltip for a hovered stack.
  craftpp::render::Image img;
  std::string err;
  REQUIRE(craftpp::render::load_png("assets/font/default.png", img, err));
  craftpp::gui::Font font;
  REQUIRE(font.load_glyphs(img.rgba.data(), img.width, img.height));
  REQUIRE(font.load_allowed("assets/font.txt"));
  Inventory inv;
  inv.main[9] = st(351, 3, 1);  // rose red: subtype name override
  craftpp::gui::OpenGui g;
  g.kind = craftpp::gui::OpenGui::Kind::Inventory;
  craftpp::gui::ctn::build_player(g.kit, inv);
  std::map<std::string, std::string> lang = {{"item.dyePowder.red.name", "Rose Red"}};
  // Hover the dye slot: panel origin centers 176x166 in 854x480.
  const float px = (854 - 176) / 2.0F, py = (480 - 166) / 2.0F;
  const double mx = px + 8 + 0 * 18 + 8, my = py + 84 + 8;
  auto m = craftpp::gui::draw_open_gui(g, inv, font, lang, mx, my, 854, 480);
  CHECK(!m.panel.vertices.empty());
  CHECK(!m.items.vertices.empty());
  CHECK(!m.text.vertices.empty());  // labels + tooltip
  CHECK(!m.hl.vertices.empty());    // hover wash + tooltip box
  // Name keys: base + subtype overrides.
  CHECK(std::string(craftpp::gui::name_key_for(3, 0)) == "tile.dirt.name");
  CHECK(std::string(craftpp::gui::name_key_for(351, 1)) == "item.dyePowder.red.name");
  CHECK(std::string(craftpp::gui::name_key_for(35, 0)) == "tile.cloth.white.name");
  CHECK(std::string(craftpp::gui::name_key_for(263, 1)) == "item.charcoal.name");
  CHECK(std::string(craftpp::gui::name_key_for(44, 3)) == "tile.stoneSlab.cobble.name");
}
