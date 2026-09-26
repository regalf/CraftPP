// Day/night math + random block ticks (grass/leaves/ice/fire).
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include "world/live.hpp"
#include "world/tick.hpp"

using namespace craftpp::world;

TEST_CASE("daylight cycle matches Java anchor points", "[tick]") {
  // Noon (t=6000): full sun. Midnight (t=18000): full dark.
  CHECK(tick::skylight_subtracted(6000) == 0);
  CHECK(tick::skylight_subtracted(18000) == 11);
  CHECK(tick::daylight_factor(6000) == 1.0f);
  CHECK(tick::daylight_factor(18000) == 0.2f);
  // Dusk (t=13000): sun just set, partial subtract.
  const int dusk = tick::skylight_subtracted(13000);
  CHECK(dusk > 0);
  CHECK(dusk < 11);
  const float f = tick::daylight_factor(0);
  CHECK(f > 0.2f);
  CHECK(f < 1.0f);
}

namespace {
// Controlled single-chunk world: flat stone base + dirt top at y=64.
LiveWorld flat_world() {
  LiveWorld w(7LL);
  w.provide_area(-1, -1, 1, 1);  // 3x3 so the light BFS guards pass
  for (int x = 0; x < 16; ++x)
    for (int z = 0; z < 16; ++z) {
      for (int y = 0; y < 60; ++y) w.set_raw(x, y, z, bid::kStone, 0);
      for (int y = 60; y < 64; ++y) w.set_raw(x, y, z, bid::kDirt, 0);
      for (int y = 64; y < 128; ++y) w.set_raw(x, y, z, 0, 0);
    }
  return w;
}
}  // namespace

TEST_CASE("grass dies under cover", "[tick]") {
  auto w = flat_world();
  w.set_raw(4, 64, 4, bid::kGrass, 0);
  w.set_raw(4, 65, 4, bid::kStone, 0);  // opaque cover kills the light
  craftpp::JavaRandom r(1L);
  std::vector<tick::ScheduledTick> q;
  // Force midday light so the check is about the cover, not the hour.
  for (int i = 0; i < 6000; ++i) w.tick();
  tick::update_tick(w, q, w.world_time(), r, bid::kGrass, 4, 64, 4);
  CHECK(w.block_id(4, 64, 4) == bid::kDirt);
}

TEST_CASE("grass spreads to lit dirt", "[tick]") {
  bool spread = false;
  for (std::int64_t s = 0; s < 40 && !spread; ++s) {
    auto w = flat_world();
    w.set_raw(8, 64, 8, bid::kGrass, 0);
    for (int i = 0; i < 6000; ++i) w.tick();  // midday
    craftpp::JavaRandom r(s);
    std::vector<tick::ScheduledTick> q;
    tick::update_tick(w, q, w.world_time(), r, bid::kGrass, 8, 64, 8);
    for (int dx = -1; dx <= 1 && !spread; ++dx)
      for (int dz = -1; dz <= 1 && !spread; ++dz)
        for (int dy = -3; dy <= 1 && !spread; ++dy)
          if ((dx != 0 || dy != 0 || dz != 0) && w.block_id(8 + dx, 64 + dy, 8 + dz) == bid::kGrass)
            spread = true;
  }
  CHECK(spread);
}

TEST_CASE("lone marked leaves decay", "[tick]") {
  auto w = flat_world();
  w.set_raw(5, 70, 5, bid::kLeaves, 8);  // decay-check marked, no log near
  craftpp::JavaRandom r(3L);
  std::vector<tick::ScheduledTick> q;
  tick::update_tick(w, q, w.world_time(), r, bid::kLeaves, 5, 70, 5);
  CHECK(w.block_id(5, 70, 5) == 0);
}

TEST_CASE("leaves near logs persist", "[tick]") {
  auto w = flat_world();
  w.set_raw(5, 64, 5, bid::kLog, 0);
  w.set_raw(5, 65, 5, bid::kLeaves, 8);
  craftpp::JavaRandom r(3L);
  std::vector<tick::ScheduledTick> q;
  tick::update_tick(w, q, w.world_time(), r, bid::kLeaves, 5, 65, 5);
  CHECK(w.block_id(5, 65, 5) == bid::kLeaves);
  CHECK((w.block_meta(5, 65, 5) & 8) == 0);  // mark cleared, connected
}

TEST_CASE("ice melts near block light", "[tick]") {
  auto w = flat_world();
  w.set_raw(6, 70, 6, bid::kLavaStill, 0);
  w.set_raw(6, 70, 8, bid::kIce, 0);
  craftpp::JavaRandom r(9L);
  std::vector<tick::ScheduledTick> q;
  CHECK(w.block_id(6, 70, 6) == bid::kLavaStill);
  CHECK(w.saved_block(6, 70, 6) == 15);
  CHECK(w.saved_block(6, 70, 7) > 8);
  CHECK(w.saved_block(6, 70, 8) > 8);
  tick::update_tick(w, q, w.world_time(), r, bid::kIce, 6, 70, 8);
  CHECK(w.block_id(6, 70, 8) == bid::kWaterStill);
}

TEST_CASE("fire reschedules itself", "[tick]") {
  auto w = flat_world();
  w.set_raw(3, 64, 3, bid::kStone, 0);
  w.set_raw(3, 65, 3, bid::kFire, 0);
  craftpp::JavaRandom r(11L);
  std::vector<tick::ScheduledTick> q;
  tick::update_tick(w, q, w.world_time(), r, bid::kFire, 3, 65, 3);
  CHECK(!q.empty());
  CHECK(q[0].id == bid::kFire);
}
