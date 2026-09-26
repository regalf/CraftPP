// McRegion save/load tests: chunk NBT round-trip through region files,
// level.dat rotation, and full LiveWorld save -> load identity.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <filesystem>
#include <string>

#include "core/nbt.hpp"
#include "entity/mob.hpp"
#include "entity/player.hpp"
#include "world/blocks.hpp"
#include "world/live.hpp"
#include "world/mcregion.hpp"
#include "world/save.hpp"

namespace {

struct TestPlayer : craftpp::entity::Player {
  explicit TestPlayer(craftpp::entity::EntityWorld* w) : Player(w) {}
};

std::string scratch_dir(const char* name) {
  auto p = std::filesystem::temp_directory_path() / name;
  std::error_code ec;
  std::filesystem::remove_all(p, ec);
  return p.string();
}

TEST_CASE("chunk NBT round-trips through a region file", "[save]") {
  using namespace craftpp::world;
  LiveWorld w(1LL);
  w.provide_area(-2, -2, 2, 2);
  // Touch a block + metadata so Data/relight paths are exercised.
  w.set_raw(3, 70, 5, 1, 0);
  w.set_raw(3, 71, 5, 50, 0);  // torch
  const craftpp::nbt::Tag tag = chunk_to_tag(w, 0, 0);
  craftpp::nbt::Writer ser;
  craftpp::nbt::write_root(ser, "", tag);
  RegionFile rf(scratch_dir("x") + ".mcr", true);
  // scratch path hack: RegionFile takes a file path directly.
  const std::vector<std::uint8_t>& raw = ser.bytes();
  rf.write_chunk(0, 0, raw.data(), raw.size());
  auto back = rf.read_chunk(0, 0);
  REQUIRE(back.has_value());
  craftpp::nbt::Reader r(back->data(), back->size());
  LiveWorld w2(1LL);
  REQUIRE(chunk_from_tag(craftpp::nbt::read_root(r), w2, 0, 0));
  CHECK(w2.chunk_ids(0, 0) == w.chunk_ids(0, 0));
  CHECK(w2.chunk_metadata(0, 0) == w.chunk_metadata(0, 0));
  CHECK(w2.chunk_skylight(0, 0) == w.chunk_skylight(0, 0));
  CHECK(w2.chunk_blocklight(0, 0) == w.chunk_blocklight(0, 0));
  CHECK(w2.chunk_heightmap(0, 0) == w.chunk_heightmap(0, 0));
  CHECK(w2.is_populated(0, 0) == w.is_populated(0, 0));
  CHECK(w2.block_id(3, 70, 5) == 1);
  CHECK(w2.block_id(3, 71, 5) == 50);
}

TEST_CASE("LiveWorld save/load is identical (blocks, light, time, spawn)", "[save]") {
  using namespace craftpp::world;
  const std::string dir = scratch_dir("craftpp_save_test");
  LiveWorld w(7LL);
  w.set_level_name("TestWorld");
  w.set_spawn_point(8.5, 70.0, 8.5);
  w.provide_area(-2, -2, 2, 2);
  w.set_raw(-4, 70, 9, 45, 0);  // player edit must survive
  for (int i = 0; i < 100; ++i) w.tick();
  w.save(dir);
  REQUIRE(std::filesystem::exists(dir + "/level.dat"));
  REQUIRE(std::filesystem::exists(dir + "/region/r.-1.-1.mcr"));

  LiveWorld loaded(7LL);
  REQUIRE(loaded.load(dir));
  CHECK(loaded.world_time() == w.world_time());
  CHECK(loaded.level_name() == "TestWorld");
  for (int cx = -2; cx <= 2; ++cx) {
    for (int cz = -2; cz <= 2; ++cz) {
      INFO("chunk " << cx << "," << cz);
      CHECK(loaded.chunk_ids(cx, cz) == w.chunk_ids(cx, cz));
      CHECK(loaded.chunk_metadata(cx, cz) == w.chunk_metadata(cx, cz));
      CHECK(loaded.chunk_skylight(cx, cz) == w.chunk_skylight(cx, cz));
      CHECK(loaded.chunk_blocklight(cx, cz) == w.chunk_blocklight(cx, cz));
      CHECK(loaded.chunk_heightmap(cx, cz) == w.chunk_heightmap(cx, cz));
      CHECK(loaded.is_populated(cx, cz) == w.is_populated(cx, cz));
    }
  }
  CHECK(loaded.block_id(-4, 70, 9) == 45);

  // Wrong seed refuses to load; missing dir refuses too.
  LiveWorld other(8LL);
  CHECK(!other.load(dir));
  LiveWorld missing(7LL);
  CHECK(!missing.load(dir + "_nope"));
  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
}

TEST_CASE("level.dat survives the _new/_old rotation", "[save]") {  using namespace craftpp::world;
  const std::string dir = scratch_dir("craftpp_level_test");
  WorldInfoData info;
  info.seed = 1234;
  info.spawn_x = 10;
  info.spawn_y = 65;
  info.spawn_z = -3;
  info.time = 42;
  info.level_name = "Rot";
  write_level_dat(dir, info);
  write_level_dat(dir, info);  // second save rotates level.dat -> _old
  const auto back = read_level_dat(dir);
  REQUIRE(back.has_value());
  CHECK(back->seed == 1234);
  CHECK(back->spawn_x == 10);
  CHECK(back->spawn_y == 65);
  CHECK(back->spawn_z == -3);
  CHECK(back->time == 42);
  CHECK(back->level_name == "Rot");
  CHECK(back->version == 19132);
  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
}

TEST_CASE("entities, tiles, tileticks and player survive save/load", "[save]") {
  using namespace craftpp::world;
  const std::string dir = scratch_dir("craftpp_save_ent_test");
  LiveWorld w(7LL);
  w.set_level_name("Ent");
  w.provide_area(-1, -1, 1, 1);

  // Chest with contents + furnace burning + sign text.
  w.set_raw(2, 66, 2, bid::kChest, 0);
  auto* chest = dynamic_cast<tile::ChestEntity*>(w.tile_at(2, 66, 2));
  REQUIRE(chest != nullptr);
  chest->items[0] = craftpp::entity::ItemStack(4, 32, 0);   // cobble
  chest->items[26] = craftpp::entity::ItemStack(319, 5, 0);  // pork
  w.set_raw(-3, 66, 1, bid::kFurnaceIdle, 0);
  auto* furn = dynamic_cast<tile::FurnaceEntity*>(w.tile_at(-3, 66, 1));
  REQUIRE(furn != nullptr);
  furn->items[0] = craftpp::entity::ItemStack(15, 3, 0);  // iron ore
  furn->items[1] = craftpp::entity::ItemStack(263, 2, 0);  // coal
  furn->burn_time = 100;
  furn->cook_time = 37;
  w.set_raw(0, 67, -4, bid::kSignPost, 0);
  auto* sign = dynamic_cast<tile::SignEntity*>(w.tile_at(0, 67, -4));
  REQUIRE(sign != nullptr);
  sign->lines[0] = "hello";
  sign->lines[2] = "world";

  // A drop and a mob with non-default state.
  w.on_item_drop(4, 17, 0, 1.5, 68.0, 1.5, 0.1, 0.2, 0.3);
  REQUIRE(w.items().size() == 1);
  w.items()[0]->age = 123;
  auto zb = std::make_unique<craftpp::entity::Zombie>(&w);
  zb->set_position(5.5, 68.0, -2.5);
  zb->health = 13;
  zb->hurt_time = 4;
  w.adopt_mob(std::move(zb));

  // A scheduled fire tick inside chunk (0,0): real fire on stone so the
  // engine runs (and reschedules) it like vanilla instead of dropping it.
  w.set_raw(1, 69, 1, 1, 0);  // stone below
  w.set_raw(1, 70, 1, bid::kFire, 0);
  w.schedule_tick(1, 70, 1, bid::kFire, 30);

  // Player with inventory, armor, food and xp.
  TestPlayer p(&w);
  p.set_position(8.5, 70.0, 8.5);  // pos_y includes +1.62
  p.rotation_yaw = 45.0f;
  p.health = 17;
  p.inventory.main[0] = craftpp::entity::ItemStack(278, 1, 33);  // pickaxe dmg
  p.inventory.main[9] = craftpp::entity::ItemStack(4, 64, 0);
  p.inventory.armor[0] = craftpp::entity::ItemStack(298, 1, 0);  // helmet
  p.inventory.current = 3;
  p.food.food_level = 14;
  p.current_xp = 0.5f;
  p.total_xp = 11;

  w.save(dir, &p);

  LiveWorld l(7LL);
  REQUIRE(l.load(dir));
  // Tiles.
  auto* lc = dynamic_cast<tile::ChestEntity*>(l.tile_at(2, 66, 2));
  REQUIRE(lc != nullptr);
  CHECK(lc->items[0]->item_id == 4);
  CHECK(lc->items[0]->stack_size == 32);
  CHECK(lc->items[26]->item_id == 319);
  CHECK(lc->items[26]->stack_size == 5);
  CHECK(!lc->items[1].has_value());
  auto* lf = dynamic_cast<tile::FurnaceEntity*>(l.tile_at(-3, 66, 1));
  REQUIRE(lf != nullptr);
  CHECK(lf->items[0]->item_id == 15);
  CHECK(lf->items[1]->item_id == 263);
  CHECK(lf->burn_time == 100);
  CHECK(lf->cook_time == 37);
  auto* ls = dynamic_cast<tile::SignEntity*>(l.tile_at(0, 67, -4));
  REQUIRE(ls != nullptr);
  CHECK(ls->lines[0] == "hello");
  CHECK(ls->lines[1] == "");
  CHECK(ls->lines[2] == "world");
  // Drop + mob.
  REQUIRE(l.items().size() == 1);
  CHECK(l.items()[0]->item.item_id == 4);
  CHECK(l.items()[0]->item.stack_size == 17);
  CHECK(l.items()[0]->age == 123);
  CHECK(l.items()[0]->motion_x == Catch::Approx(0.1));
  CHECK(l.items()[0]->pos_x == Catch::Approx(1.5));
  CHECK(l.items()[0]->pos_y == Catch::Approx(68.0));
  CHECK(l.items()[0]->pos_z == Catch::Approx(1.5));
  REQUIRE(l.mobs().size() == 1);
  auto* lz = dynamic_cast<craftpp::entity::Zombie*>(l.mobs()[0].get());
  REQUIRE(lz != nullptr);
  CHECK(lz->health == 13);
  CHECK(lz->hurt_time == 4);
  CHECK(lz->pos_x == Catch::Approx(5.5));
  CHECK(lz->pos_y == Catch::Approx(68.0));
  CHECK(lz->pos_z == Catch::Approx(-2.5));
  // Scheduled tick persisted with relative delay (save time 0 + 30).
  REQUIRE(l.scheduled_ticks().size() == 1);
  CHECK(l.scheduled_ticks()[0].x == 1);
  CHECK(l.scheduled_ticks()[0].id == bid::kFire);
  CHECK(l.scheduled_ticks()[0].time == 30);
  // After 35 ticks the loaded entry fired (fire reschedules itself at +40).
  for (int i = 0; i < 35; ++i) l.tick();
  REQUIRE(l.scheduled_ticks().size() == 1);
  CHECK(l.scheduled_ticks()[0].time == 70);
  // Player.
  const auto info = read_level_dat(dir);
  REQUIRE(info.has_value());
  REQUIRE(info->player.has_value());
  TestPlayer p2(&l);
  apply_player_tag(p2, *info->player);
  // Round-trip identity (pos_y raw); the tag itself stores feet (vanilla
  // Pos convention: raw - y_offset).
  CHECK(p2.pos_y == Catch::Approx(70.0));
  CHECK(info->player->compound->find("Pos")->list->items[1].f64 ==
        Catch::Approx(70.0 - 1.62));
  CHECK(p2.rotation_yaw == Catch::Approx(45.0f));
  CHECK(p2.health == 17);
  CHECK(p2.inventory.main[0]->item_id == 278);
  CHECK(p2.inventory.main[0]->damage == 33);
  CHECK(p2.inventory.main[9]->stack_size == 64);
  CHECK(p2.inventory.armor[0]->item_id == 298);
  CHECK(p2.food.food_level == 14);
  CHECK(p2.current_xp == Catch::Approx(0.5f));
  CHECK(p2.total_xp == 11);
  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
}

}  // namespace
