// Mobs: pig/zombie spawn validity, zombie daylight burn, drops, spawning.
#include <catch2/catch_test_macros.hpp>
#include "entity/controller.hpp"
#include "entity/mob.hpp"
#include "entity/player.hpp"
#include "entity/player_sp.hpp"
#include "world/live.hpp"

using namespace craftpp;

namespace {
world::LiveWorld flat_world() {
  world::LiveWorld w(11LL);
  w.provide_area(-1, -1, 1, 1);
  for (int x = 0; x < 16; ++x)
    for (int z = 0; z < 16; ++z) {
      for (int y = 0; y < 60; ++y) w.set_raw(x, y, z, world::bid::kStone, 0);
      for (int y = 60; y < 64; ++y) w.set_raw(x, y, z, world::bid::kDirt, 0);
      w.set_raw(x, 64, z, world::bid::kGrass, 0);
      for (int y = 65; y < 128; ++y) w.set_raw(x, y, z, 0, 0);
    }
  return w;
}
}  // namespace

TEST_CASE("pig spawns on lit grass", "[mob]") {
  auto w = flat_world();
  entity::Pig p(&w);
  p.set_position_and_rotation(8.5, 65.0, 8.5, 0.0f, 0.0f);
  CHECK(p.can_spawn_here());
  // On dirt: no.
  w.set_raw(8, 64, 8, world::bid::kDirt, 0);
  CHECK(!p.can_spawn_here());
}

TEST_CASE("zombie burns in daylight", "[mob]") {
  auto w = flat_world();  // t=0, skylight_sub 0 < 4: daytime
  entity::Zombie z(&w);
  z.set_position_and_rotation(8.5, 65.0, 8.5, 0.0f, 0.0f);
  bool burned = false;
  for (int i = 0; i < 600 && !burned; ++i) {
    z.on_living_update();
    if (z.fire > 0) burned = true;
  }
  CHECK(burned);
}

TEST_CASE("zombie needs darkness to spawn", "[mob]") {
  auto w = flat_world();
  // Dark room: stone walls + roof around the feet (light must not leak).
  // Block light stays 0 inside (passes: 0 <= rand(8) always).
  for (int dx = -2; dx <= 2; ++dx)
    for (int dz = -2; dz <= 2; ++dz)
      for (int dy = 65; dy <= 70; ++dy) {
        if (dx == 0 && dz == 0 && (dy == 65 || dy == 66)) continue;  // feet + head
        w.set_raw(8 + dx, dy, 8 + dz, world::bid::kStone, 0);
      }
  bool ok = false;
  for (int i = 0; i < 300 && !ok; ++i) {
    entity::Zombie z(&w);
    z.set_position_and_rotation(8.5, 65.0, 8.5, 0.0f, 0.0f);
    if (z.can_spawn_here()) ok = true;
  }
  CHECK(ok);
  // Open field at day: block light 0 -> never.
  bool open_ok = false;
  for (int i = 0; i < 50 && !open_ok; ++i) {
    entity::Zombie z(&w);
    z.set_position_and_rotation(2.5, 65.0, 2.5, 0.0f, 0.0f);
    if (z.can_spawn_here()) open_ok = true;
  }
  CHECK(!open_ok);
}

TEST_CASE("pig drops pork on death", "[mob]") {
  auto w = flat_world();
  int pork = 0;
  for (int i = 0; i < 6; ++i) {
    entity::Pig p(&w);
    p.set_position_and_rotation(8.5, 65.0, 8.5, 0.0f, 0.0f);
    p.attack(entity::DamageSource::kPlayer, 100);
    for (int t = 0; t < 40 && !p.is_dead; ++t) p.on_update();
    REQUIRE(p.is_dead);
  }
  for (auto& it : w.items()) {
    if (it->item.item_id == 319 || it->item.item_id == 320) pork += it->item.stack_size;
  }
  CHECK(pork > 0);
}

TEST_CASE("zombie climbs one-high wall", "[mob]") {
  auto w = flat_world();
  for (int z = 4; z < 13; ++z) w.set_raw(8, 65, z, world::bid::kStone, 0);
  entity::PlayerSP player(&w, "t", 0);
  player.set_position_and_rotation(14.5, 66.62, 8.5, 0.0f, 0.0f);
  w.add_entity(&player);
  entity::Zombie z(&w);
  z.set_position_and_rotation(0.5, 65.0, 8.5, 0.0f, 0.0f);
  w.add_entity(&z);
  bool crossed = false;
  for (int i = 0; i < 400 && !crossed; ++i) {
    z.on_update();
    if (z.pos_x > 10.0) crossed = true;
  }
  CHECK(crossed);
}

TEST_CASE("creative player takes no damage", "[mob]") {
  auto w = flat_world();
  entity::PlayerSP player(&w, "t", 0);
  player.set_position_and_rotation(8.5, 65.0, 8.5, 0.0f, 0.0f);
  entity::Zombie z(&w);
  z.set_position_and_rotation(9.5, 65.0, 8.5, 0.0f, 0.0f);
  CHECK(player.health == 20);
  z.attack_time = 0;
  // Force melee range Plenty of ticks: zombie seeks and hits.
  bool hurt = false;
  for (int i = 0; i < 400 && !hurt; ++i) {
    z.on_update();
    player.on_update();
    if (player.health < 20) hurt = true;
  }
  CHECK(hurt);  // survival: zombie mauled the player
  // Creative: immune.
  entity::ControllerCreative::enable_creative(player);
  player.health = 20;
  z.attack_time = 0;
  z.set_position_and_rotation(9.5, 65.0, 8.5, 0.0f, 0.0f);
  for (int i = 0; i < 400; ++i) {
    z.on_update();
    player.on_update();
  }
  CHECK(player.health == 20);
  entity::ControllerCreative::disable_creative(player);
}

TEST_CASE("player picks up drops", "[mob]") {
  auto w = flat_world();
  entity::PlayerSP player(&w, "t", 0);
  player.set_position_and_rotation(8.5, 65.0, 8.5, 0.0f, 0.0f);
  w.add_entity(&player);
  w.on_item_drop(5, 3, 0, 8.5, 65.5, 8.5, 0, 0, 0);
  REQUIRE(!w.items().empty());
  w.items()[0]->pickup_delay = 0;
  w.items()[0]->set_position(8.5, 65.5, 8.5);
  for (int i = 0; i < 5; ++i) w.tick();
  int planks = 0;
  for (auto& s : player.inventory.main) {
    if (s.has_value() && s->item_id == 5) planks += s->stack_size;
  }
  CHECK(planks == 3);
  CHECK(w.items().empty());
}
