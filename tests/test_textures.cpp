// Block texture/render-type table tests: spot checks against the 1.0
// Block registration (constructor params) and getBlockTexture overrides.

#include <catch2/catch_test_macros.hpp>

#include "world/blocks.hpp"

namespace {

TEST_CASE("block_texture base indices match 1.0 registration", "[textures]") {
  using namespace craftpp::world::bid;
  CHECK(block_texture(kStone, 0, 0) == 1);
  CHECK(block_texture(kCobble, 3, 0) == 16);
  CHECK(block_texture(kDiamondOre, 1, 0) == 50);
  CHECK(block_texture(kGlass, 5, 0) == 49);
  CHECK(block_texture(kTorch, 2, 0) == 80);
  CHECK(block_texture(kObsidian, 0, 0) == 37);
  CHECK(block_texture(kWhiteStone, 1, 0) == 175);
  CHECK(block_texture(kDragonEgg, 1, 0) == 167);
}

TEST_CASE("block_texture special faces", "[textures]") {
  using namespace craftpp::world::bid;
  // grass
  CHECK(block_texture(kGrass, 1, 0) == 0);
  CHECK(block_texture(kGrass, 0, 0) == 2);
  CHECK(block_texture(kGrass, 4, 0) == 3);
  // log
  CHECK(block_texture(kLog, 1, 0) == 21);
  CHECK(block_texture(kLog, 2, 0) == 20);
  CHECK(block_texture(kLog, 2, 1) == 116);
  CHECK(block_texture(kLog, 2, 2) == 117);
  // furnace facing (meta 3 = +z front)
  CHECK(block_texture(kFurnaceIdle, 3, 3) == 44);
  CHECK(block_texture(kFurnaceBurn, 3, 3) == 61);
  CHECK(block_texture(kFurnaceIdle, 2, 3) == 45);
  CHECK(block_texture(kFurnaceIdle, 1, 3) == 62);
  // dispenser front
  CHECK(block_texture(kDispenser, 3, 3) == 46);
  // sandstone / bookshelf / workbench / tnt
  CHECK(block_texture(kSandstone, 1, 0) == 176);
  CHECK(block_texture(kSandstone, 0, 0) == 208);
  CHECK(block_texture(kBookshelf, 1, 0) == 4);
  CHECK(block_texture(kBookshelf, 2, 0) == 35);
  CHECK(block_texture(kWorkbench, 1, 0) == 43);
  CHECK(block_texture(kTnt, 0, 0) == 10);
  CHECK(block_texture(kTnt, 1, 0) == 9);
  CHECK(block_texture(kTnt, 2, 0) == 8);
  // wool formula: meta 1 (orange) -> 113 + 0 + 14*16 = 337? (vanilla math)
  CHECK(block_texture(kWool, 2, 0) == 64);
  CHECK(block_texture(kWool, 2, 1) == 113 + ((~1 & 8) >> 3) + ((~1 & 7)) * 16);
  // slabs
  CHECK(block_texture(kStepSingle, 2, 0) == 5);
  CHECK(block_texture(kStepSingle, 1, 1) == 176);
  CHECK(block_texture(kStepSingle, 2, 4) == 7);
  // pumpkin face on matching side
  CHECK(block_texture(kPumpkin, 2, 2) == 119);
  CHECK(block_texture(kPumpkinLantern, 4, 4) == 120);
  CHECK(block_texture(kPumpkin, 3, 2) == 102);
  // fluids / melon / mycelium / cactus
  CHECK(block_texture(kWaterStill, 1, 0) == 205);
  CHECK(block_texture(kWaterStill, 2, 0) == 206);
  CHECK(block_texture(kLavaStill, 2, 0) == 238);
  CHECK(block_texture(kMelon, 2, 0) == 136);
  CHECK(block_texture(kMelon, 1, 0) == 137);
  CHECK(block_texture(kMycelium, 1, 0) == 78);
  CHECK(block_texture(kCactus, 1, 0) == 69);
  CHECK(block_texture(kLeaves, 2, 1) == 132);
  CHECK(block_texture(kSapling, 2, 2) == 79);
}

TEST_CASE("render_type matches 1.0 classes", "[textures]") {
  using namespace craftpp::world::bid;
  CHECK(render_type(kStone) == 0);
  CHECK(render_type(kFlowerYellow) == 1);
  CHECK(render_type(kReed) == 1);
  CHECK(render_type(kTorch) == 2);
  CHECK(render_type(kFire) == 3);
  CHECK(render_type(kWaterStill) == 4);
  CHECK(render_type(kLavaMoving) == 4);
  CHECK(render_type(kRail) == 9);
  CHECK(render_type(kStairsWood) == 10);
  CHECK(render_type(kChest) == 22);
  CHECK(render_type(kSignPost) == -1);
  CHECK(render_type(kPumpkin) == 0);
}

}  // namespace
