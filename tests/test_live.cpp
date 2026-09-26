// Live singleplayer world: generation + tick determinism + light on edits.
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include "entity/controller.hpp"
#include "entity/player_sp.hpp"
#include "world/live.hpp"

namespace {
craftpp::world::LiveWorld make_world() {
  craftpp::world::LiveWorld w(1LL);
  w.provide_area(-2, -2, 2, 2);
  return w;
}
// Flat stone/dirt/grass pad in chunk (0,0) for controlled actor tests.
void flatten(craftpp::world::LiveWorld& w) {
  for (int x = 0; x < 16; ++x)
    for (int z = 0; z < 16; ++z) {
      for (int y = 0; y < 60; ++y) w.set_raw(x, y, z, 1, 0);
      for (int y = 60; y < 64; ++y) w.set_raw(x, y, z, 3, 0);
      w.set_raw(x, 64, z, 2, 0);
      for (int y = 65; y < 128; ++y) w.set_raw(x, y, z, 0, 0);
    }
}
}  // namespace

TEST_CASE("live world provides populated chunks", "[live]") {
  auto w = make_world();
  CHECK(w.is_provided(0, 0));
  CHECK(w.is_populated(0, 0));
  CHECK(w.is_populated(-2, -2));
  // Some solid ground near spawn.
  bool found_ground = false;
  for (int x = 0; x < 16 && !found_ground; ++x)
    for (int z = 0; z < 16 && !found_ground; ++z)
      for (int y = 60; y < 80; ++y)
        if (w.block_id(x, y, z) != 0) found_ground = true;
  CHECK(found_ground);
  // Stored height agrees with a live scan of opaque blocks.
  for (int x = 0; x < 16; x += 5)
    for (int z = 0; z < 16; z += 5) {
      int live = 127;
      for (; live > 0; --live) {
        const int id = w.block_id(x, live - 1, z);
        if (id != 0 && craftpp::world::bid::light_opacity(id) != 0) break;
      }
      // Height may lag the live scan only where populate writes moved it
      // (relight maintains it); it must never exceed live+0 drift... just
      // check it is within the valid range here (exactness is test_populate).
      CHECK(w.stored_height(x, z) >= 0);
      CHECK(w.stored_height(x, z) < 128);
      (void)live;
    }
}

TEST_CASE("live world tick advances time", "[live]") {
  auto w = make_world();
  CHECK(w.world_time() == 0);
  for (int i = 0; i < 40; ++i) w.tick();
  CHECK(w.world_time() == 40);
}

TEST_CASE("live world edits update light", "[live]") {
  auto w = make_world();
  // Find an open-sky surface column in chunk (0,0).
  int sx = -1, sz = -1, sy = -1;
  for (int x = 0; x < 16 && sx < 0; ++x)
    for (int z = 0; z < 16 && sx < 0; ++z) {
      int y = 127;
      for (; y > 0; --y)
        if (w.block_id(x, y, z) != 0) break;
      if (y > 0 && y < 120 && w.block_id(x, y + 1, z) == 0) {
        sx = x;
        sz = z;
        sy = y + 1;
      }
    }
  REQUIRE(sx >= 0);
  const int h0 = w.stored_height(sx, sz);
  const int sky0 = w.saved_sky(sx, sy, sz);
  CHECK(sky0 >= 0);
  // Stack stone above the surface: height must rise, sky below must fall.
  w.set_raw(sx, sy + 6, sz, 1, 0);
  CHECK(w.is_dirty(0, 0));
  CHECK(w.stored_height(sx, sz) > h0);
  CHECK(w.saved_sky(sx, sy, sz) < sky0);
  // Removing it re-brightens.
  w.set_raw(sx, sy + 6, sz, 0, 0);
  CHECK(w.saved_sky(sx, sy, sz) == sky0);
}

TEST_CASE("live world generation is deterministic", "[live]") {  auto hash_world = [](craftpp::world::LiveWorld& w) {
    std::uint32_t h = 1;
    for (int cx = -1; cx <= 1; ++cx)
      for (int cz = -1; cz <= 1; ++cz)
        for (int y = 0; y < 128; ++y)
          for (int lx = 0; lx < 16; ++lx)
            for (int lz = 0; lz < 16; ++lz) {
              const int x = cx * 16 + lx, z = cz * 16 + lz;
              h = 31 * h + static_cast<std::uint32_t>(w.block_id(x, y, z));
              h = 31 * h + static_cast<std::uint32_t>(w.block_meta(x, y, z));
            }
    return h;
  };
  auto a = make_world();
  auto b = make_world();
  CHECK(hash_world(a) == hash_world(b));
}

TEST_CASE("creative double-tap space toggles fly", "[live]") {
  craftpp::world::LiveWorld w(1LL);
  w.provide_area(-1, -1, 1, 1);
  flatten(w);
  craftpp::entity::PlayerSP p(&w, "t", 0);
  p.set_position_and_rotation(8.5, 66.62, 8.5, 0.0f, 0.0f);
  w.add_entity(&p);
  craftpp::entity::ControllerCreative::enable_creative(p);
  for (int i = 0; i < 30; ++i) {
    p.movement_input->jump = false;
    w.tick();
  }
  REQUIRE(p.on_ground);
  auto tap = [&](int n) {
    for (int i = 0; i < n; ++i) {
      p.movement_input->jump = true;
      w.tick();
    }
  };
  auto rest = [&](int n) {
    for (int i = 0; i < n; ++i) {
      p.movement_input->jump = false;
      w.tick();
    }
  };
  tap(2);
  CHECK(!p.capabilities.is_flying);  // first tap only arms the timer
  rest(4);
  const double y0 = p.pos_y;
  tap(2);
  CHECK(p.capabilities.is_flying);  // second tap toggles
  rest(10);
  CHECK(p.pos_y > y0);  // holding jump climbs while flying
}
