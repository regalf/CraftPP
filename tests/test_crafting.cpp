// Crafting engine: table size, shaped/shapeless/mirrored matching.
#include <catch2/catch_test_macros.hpp>
#include "gui/crafting.hpp"
#include "world/blocks.hpp"
#include "world/item_ids.hpp"

namespace {
using craftpp::entity::ItemStack;
using Grid = std::array<std::optional<ItemStack>, 9>;
Grid empty_grid() { return Grid{}; }
void put(Grid& g, int col, int row, int id, int n = 1, int dmg = 0) {
  g[row * 3 + col] = ItemStack(id, n, dmg);
}
}  // namespace

TEST_CASE("crafting table size sanity", "[craft]") {
  // 1.0 ships ~150 recipes (source prints the count at boot).
  CHECK(craftpp::craft::table().size() > 140);
}

TEST_CASE("wooden pickaxe matches", "[craft]") {
  Grid g = empty_grid();
  put(g, 0, 0, 5);
  put(g, 1, 0, 5);
  put(g, 2, 0, 5);
  put(g, 1, 1, 280);
  put(g, 1, 2, 280);
  auto r = craftpp::craft::find_match(g);
  REQUIRE(r.has_value());
  CHECK(r->item_id == 270);
  CHECK(r->stack_size == 1);
}

TEST_CASE("torch needs coal over stick", "[craft]") {
  Grid g = empty_grid();
  put(g, 1, 0, 263);
  put(g, 1, 1, 280);
  auto r = craftpp::craft::find_match(g);
  REQUIRE(r.has_value());
  CHECK(r->item_id == 50);
  CHECK(r->stack_size == 4);
}

TEST_CASE("charcoal works for torches", "[craft]") {
  Grid g = empty_grid();
  put(g, 1, 0, 263, 1, 1);  // charcoal
  put(g, 1, 1, 280);
  auto r = craftpp::craft::find_match(g);
  REQUIRE(r.has_value());
  CHECK(r->item_id == 50);
}

TEST_CASE("axe matches mirrored", "[craft]") {
  Grid g = empty_grid();
  put(g, 0, 0, 5);
  put(g, 1, 0, 5);
  put(g, 0, 1, 280);
  put(g, 1, 1, 5);
  put(g, 0, 2, 280);
  auto r = craftpp::craft::find_match(g);
  REQUIRE(r.has_value());
  CHECK(r->item_id == 271);
}

TEST_CASE("mushroom soup is shapeless", "[craft]") {
  Grid g = empty_grid();
  put(g, 2, 1, 39);
  put(g, 0, 0, 281);
  put(g, 1, 2, 40);
  auto r = craftpp::craft::find_match(g);
  REQUIRE(r.has_value());
  CHECK(r->item_id == 282);
}

TEST_CASE("chest and furnace and workbench", "[craft]") {
  {
    Grid g = empty_grid();
    for (int x = 0; x < 3; ++x)
      for (int y = 0; y < 3; ++y)
        if (x != 1 || y != 1) put(g, x, y, 5);
    auto r = craftpp::craft::find_match(g);
    REQUIRE(r.has_value());
    CHECK(r->item_id == 54);
  }
  {
    Grid g = empty_grid();
    for (int x = 0; x < 3; ++x)
      for (int y = 0; y < 3; ++y)
        if (x != 1 || y != 1) put(g, x, y, 4);
    auto r = craftpp::craft::find_match(g);
    REQUIRE(r.has_value());
    CHECK(r->item_id == 61);
  }
}

TEST_CASE("no match on junk", "[craft]") {
  Grid g = empty_grid();
  put(g, 0, 0, 1);
  put(g, 2, 2, 287);
  CHECK(!craftpp::craft::find_match(g).has_value());
  CHECK(!craftpp::craft::find_match(empty_grid()).has_value());
}
