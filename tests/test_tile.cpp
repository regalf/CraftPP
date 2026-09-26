// TileEntity: furnace smelting, burn times, chest storage, block linkage.
#include <catch2/catch_test_macros.hpp>
#include "world/item_ids.hpp"
#include "world/live.hpp"
#include "world/tile.hpp"

using namespace craftpp::world;
using namespace craftpp::iid;

TEST_CASE("furnace burn times match Java", "[tile]") {
  CHECK(tile::burn_time_for(bid::kLog) == 300);
  CHECK(tile::burn_time_for(bid::kWoodPlank) == 300);
  CHECK(tile::burn_time_for(bid::kSapling) == 100);
  CHECK(tile::burn_time_for(kStick) == 100);
  CHECK(tile::burn_time_for(kCoal) == 1600);
  CHECK(tile::burn_time_for(kBucketLava) == 20000);
  CHECK(tile::burn_time_for(kBlazeRod) == 2400);
  CHECK(tile::burn_time_for(bid::kStone) == 0);
  CHECK(tile::burn_time_for(bid::kTorch) == 0);  // circuits, not wood
  CHECK(tile::burn_time_for(bid::kLadder) == 0);
}

TEST_CASE("furnace smelting map", "[tile]") {
  craftpp::entity::ItemStack out(0, 0, 0);
  CHECK(tile::smelt_result(bid::kIronOre, out));
  CHECK(out.item_id == kIngotIron);
  CHECK(tile::smelt_result(bid::kCobble, out));
  CHECK(out.item_id == bid::kStone);
  CHECK(tile::smelt_result(bid::kLog, out));
  CHECK(out.item_id == kCoal);
  CHECK(out.damage == 1);  // charcoal
  CHECK(tile::smelt_result(kPorkRaw, out));
  CHECK(out.item_id == kPorkCooked);
  CHECK(!tile::smelt_result(bid::kDirt, out));
  CHECK(!tile::smelt_result(kStick, out));
}

TEST_CASE("furnace smelts iron with coal", "[tile]") {
  LiveWorld w(3LL);
  w.provide_area(0, 0, 0, 0);
  w.set_raw(4, 70, 4, bid::kFurnaceIdle, 0);
  auto* t = dynamic_cast<tile::FurnaceEntity*>(w.tile_at(4, 70, 4));
  REQUIRE(t != nullptr);
  t->items[0] = craftpp::entity::ItemStack(bid::kIronOre, 2, 0);
  t->items[1] = craftpp::entity::ItemStack(kCoal, 1, 0);
  // 200 ticks per item: first ingot at t=200, second at t=400.
  for (int i = 0; i < 200; ++i) t->update(w);
  REQUIRE(t->items[2].has_value());
  CHECK(t->items[2]->item_id == kIngotIron);
  CHECK(t->items[2]->stack_size == 1);
  CHECK(t->items[0]->stack_size == 1);
  for (int i = 0; i < 200; ++i) t->update(w);
  CHECK(t->items[2]->stack_size == 2);
  CHECK(!t->items[0].has_value());
  // Burn state block swapped to lit while burning.
  CHECK(w.block_id(4, 70, 4) == bid::kFurnaceBurn);
}

TEST_CASE("furnace tile survives lit swap", "[tile]") {
  LiveWorld w(3LL);
  w.provide_area(0, 0, 0, 0);
  w.set_raw(4, 70, 4, bid::kFurnaceIdle, 0);
  auto* t = dynamic_cast<tile::FurnaceEntity*>(w.tile_at(4, 70, 4));
  REQUIRE(t != nullptr);
  CHECK(w.block_id(4, 70, 4) == bid::kFurnaceIdle);
  t->items[0] = craftpp::entity::ItemStack(bid::kSand, 1, 0);
  t->items[1] = craftpp::entity::ItemStack(bid::kLog, 1, 0);
  t->update(w);
  CHECK(w.block_id(4, 70, 4) == bid::kFurnaceBurn);
  CHECK(w.tile_at(4, 70, 4) == t);  // same tile kept
}

TEST_CASE("chest stores and drops on break", "[tile]") {
  LiveWorld w(3LL);
  w.provide_area(0, 0, 0, 0);
  w.set_raw(5, 70, 5, bid::kChest, 0);
  auto* c = dynamic_cast<tile::ChestEntity*>(w.tile_at(5, 70, 5));
  REQUIRE(c != nullptr);
  c->items[0] = craftpp::entity::ItemStack(bid::kCobble, 17, 0);
  CHECK(w.items().empty());
  w.set_raw(5, 70, 5, 0, 0);  // break
  CHECK(w.tile_at(5, 70, 5) == nullptr);
  REQUIRE(w.items().size() == 1);
  CHECK(w.items()[0]->item.item_id == bid::kCobble);
  CHECK(w.items()[0]->item.stack_size == 17);
}
