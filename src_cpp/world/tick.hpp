#pragma once

#include <cstdint>
#include <vector>

#include "core/random.hpp"
#include "world/block_place.hpp"

namespace craftpp::world::tick {

// Daylight cycle (WorldProvider.calculateCelestialAngle + World skylight
// math, clear skies — weather scales it later like the source).
float celestial_angle(std::int64_t time);  // 0..1
int skylight_subtracted(std::int64_t time);  // 0..11
float daylight_factor(std::int64_t time);  // 0.2..1.0 for the renderer

// getBlockLightValue_do equivalent (neighbor-brightness blocks + subtract).
int block_light_value(const edit::EditWorld& w, int x, int y, int z);
// getFullBlockLightValue equivalent (max of saved nibbles).
int full_light_value(const edit::EditWorld& w, int x, int y, int z);

// Scheduled block ticks (fire reschedules itself through here).
struct ScheduledTick {
  std::int64_t time;
  int x, y, z, id;
};
void schedule_tick(std::vector<ScheduledTick>& q, std::int64_t now, int x, int y, int z, int id,
                   int delay);

// Block updateTick ports (grass spread/kill, leaves decay, ice melt,
// flower/mushroom pops, fire spread). rand is the world RNG.
void update_tick(edit::EditWorld& w, std::vector<ScheduledTick>& sched, std::int64_t now,
                 JavaRandom& rand, int id, int x, int y, int z);

// Per-chunk random ticks (ice/snow pick, light check, 20 LCG updateTicks).
// update_lcg mirrors World.updateLCG (seeded nondeterministically in vanilla;
// tests pin it). rain/thunder are 0 (no weather yet).
void tick_chunk(edit::EditWorld& w, std::vector<ScheduledTick>& sched, std::int64_t now,
                JavaRandom& rand, std::int32_t& update_lcg, int cx, int cz);

}  // namespace craftpp::world::tick
