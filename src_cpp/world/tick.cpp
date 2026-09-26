#include "world/tick.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

#include "core/math_helper.hpp"
#include "core/random.hpp"
#include "world/block_place.hpp"
#include "world/blocks.hpp"

namespace craftpp::world::tick {

float celestial_angle(std::int64_t time) {
  // Mirrors WorldProvider.calculateCelestialAngle (partial tick 0).
  const int day = static_cast<int>(time % 24000LL);
  float v = static_cast<float>(day) / 24000.0f - 0.25f;
  if (v < 0.0f) v += 1.0f;
  if (v > 1.0f) v -= 1.0f;
  const float orig = v;
  // Math.cos exact (java.lang, not the sin table).
  v = 1.0f - static_cast<float>((std::cos(static_cast<double>(v) * 3.141592653589793) + 1.0) / 2.0);
  v = orig + (v - orig) / 3.0f;
  return v;
}

int skylight_subtracted(std::int64_t time) {
  // Mirrors World.calculateSkylightSubtracted (rain/thunder strength 0).
  const float ang = celestial_angle(time);
  float v = 1.0f - (MathHelper::cos(ang * 3.14159274f * 2.0f) * 2.0f + 0.5f);
  if (v < 0.0f) v = 0.0f;
  if (v > 1.0f) v = 1.0f;
  v = 1.0f - v;
  v = 1.0f - v;  // rain/thunder factors are 1.0 without weather
  return static_cast<int>(v * 11.0f);
}

float daylight_factor(std::int64_t time) {
  // Mirrors World.func_35464_b (clear skies).
  const float ang = celestial_angle(time);
  float v = 1.0f - (MathHelper::cos(ang * 3.14159274f * 2.0f) * 2.0f + 0.2f);
  if (v < 0.0f) v = 0.0f;
  if (v > 1.0f) v = 1.0f;
  v = 1.0f - v;
  return v * 0.8f + 0.2f;
}

int full_light_value(const edit::EditWorld& w, int x, int y, int z) {
  if (y < 0) return 0;
  if (y >= 128) y = 127;
  const int s = w.saved_sky(x, y, z);
  const int b = w.saved_block(x, y, z);
  return s > b ? s : b;
}

int block_light_value(const edit::EditWorld& w, int x, int y, int z) {
  // Mirrors World.getBlockLightValue_do (with neighbor-brightness ids).
  const int id = w.block_id(x, y, z);
  if (id == bid::kStepSingle || id == bid::kFarmland || id == bid::kStairsCobble ||
      id == bid::kStairsWood) {
    int best = block_light_value(w, x, y + 1, z);
    const int dx[4] = {1, -1, 0, 0};
    const int dz[4] = {0, 0, 1, -1};
    for (int k = 0; k < 4; ++k) {
      const int v = block_light_value(w, x + dx[k], y, z + dz[k]);
      if (v > best) best = v;
    }
    return best;
  }
  if (y < 0) return 0;
  if (y >= 128) y = 127;
  // Chunk.getBlockLightValue(lx, y, lz, skylightSubtracted).
  int sky = w.saved_sky(x, y, z);
  sky -= w.skylight_sub();
  const int blk = w.saved_block(x, y, z);
  return blk > sky ? blk : sky;
}

void schedule_tick(std::vector<ScheduledTick>& q, std::int64_t now, int x, int y, int z, int id,
                   int delay) {
  q.push_back(ScheduledTick{now + delay, x, y, z, id});
}

// ---- flammability tables (BlockFire.initializeBlock) ----
int encourage_fire(int id) {
  using namespace bid;
  switch (id) {
    case kWoodPlank:
      return 5;
    case kFence:
      return 5;
    case kStairsWood:
      return 5;
    case kLog:
      return 5;
    case kLeaves:
      return 30;
    case kBookshelf:
      return 30;
    case kTnt:
      return 15;
    case kTallGrass:
      return 60;
    case kWool:
      return 30;
    case kVine:
      return 15;
    default:
      return 0;
  }
}
int catch_fire(int id) {
  using namespace bid;
  switch (id) {
    case kWoodPlank:
      return 20;
    case kFence:
      return 20;
    case kStairsWood:
      return 20;
    case kLog:
      return 5;
    case kLeaves:
      return 60;
    case kBookshelf:
      return 20;
    case kTnt:
      return 100;
    case kTallGrass:
      return 100;
    case kWool:
      return 60;
    case kVine:
      return 100;
    default:
      return 0;
  }
}

void grass_tick(edit::EditWorld& w, JavaRandom& rand, int x, int y, int z) {
  // Mirrors BlockGrass.updateTick.
  if (block_light_value(w, x, y + 1, z) < 4 && bid::light_opacity(w.block_id(x, y + 1, z)) > 2) {
    edit::set_and_notify(w, x, y, z, bid::kDirt, 0);
  } else if (block_light_value(w, x, y + 1, z) >= 9) {
    for (int i = 0; i < 4; ++i) {
      const int px = x + rand.next_int(3) - 1;
      const int py = y + rand.next_int(5) - 3;
      const int pz = z + rand.next_int(3) - 1;
      const int above = w.block_id(px, py + 1, pz);
      if (w.block_id(px, py, pz) == bid::kDirt && block_light_value(w, px, py + 1, pz) >= 4 &&
          bid::light_opacity(above) <= 2) {
        edit::set_and_notify(w, px, py, pz, bid::kGrass, 0);
      }
    }
  }
}

void leaves_tick(edit::EditWorld& w, JavaRandom& rand, int x, int y, int z) {
  // Mirrors BlockLeaves.updateTick (decay check; sapling drop roll).
  const int meta = w.block_meta(x, y, z);
  if ((meta & 8) == 0 || (meta & 4) != 0) return;
  if (!w.chunks_exist(x - 5, y - 5, z - 5, x + 5, y + 5, z + 5)) return;
  // adjacentTreeBlocks[32^3], center stride: (dx+16)*1024 + (dy+16)*32 + dz+16.
  int adj[32 * 32 * 32];
  for (int dx = -4; dx <= 4; ++dx)
    for (int dy = -4; dy <= 4; ++dy)
      for (int dz = -4; dz <= 4; ++dz) {
        const int id = w.block_id(x + dx, y + dy, z + dz);
        adj[(dx + 16) * 1024 + (dy + 16) * 32 + dz + 16] =
            (id == bid::kLog ? 0 : (id == bid::kLeaves ? -2 : -1));
      }
  for (int d = 1; d <= 4; ++d) {
    for (int dx = -4; dx <= 4; ++dx)
      for (int dy = -4; dy <= 4; ++dy)
        for (int dz = -4; dz <= 4; ++dz) {
          const std::size_t i =
              static_cast<std::size_t>((dx + 16) * 1024 + (dy + 16) * 32 + dz + 16);
          if (adj[i] != d - 1) continue;
          const std::size_t ix0 = static_cast<std::size_t>((dx + 16 - 1) * 1024 + (dy + 16) * 32 + dz + 16);
          const std::size_t ix1 = static_cast<std::size_t>((dx + 16 + 1) * 1024 + (dy + 16) * 32 + dz + 16);
          const std::size_t iy0 = static_cast<std::size_t>((dx + 16) * 1024 + (dy + 16 - 1) * 32 + dz + 16);
          const std::size_t iy1 = static_cast<std::size_t>((dx + 16) * 1024 + (dy + 16 + 1) * 32 + dz + 16);
          const std::size_t iz0 = static_cast<std::size_t>((dx + 16) * 1024 + (dy + 16) * 32 + dz + 16 - 1);
          const std::size_t iz1 = static_cast<std::size_t>((dx + 16) * 1024 + (dy + 16) * 32 + dz + 16 + 1);
          if (adj[ix0] == -2) adj[ix0] = d;
          if (adj[ix1] == -2) adj[ix1] = d;
          if (adj[iy0] == -2) adj[iy0] = d;
          if (adj[iy1] == -2) adj[iy1] = d;
          if (adj[iz0] == -2) adj[iz0] = d;
          if (adj[iz1] == -2) adj[iz1] = d;
        }
  }
  const int center = adj[16 * 1024 + 16 * 32 + 16];
  if (center >= 0) {
    edit::set_meta_notify(w, x, y, z, meta & -9);
  } else {
    // removeLeaves: 1/20 sapling drop (meta & 3), then air.
    if (rand.next_int(20) == 0) {
      edit::drop_one(w, x, y, z, bid::kSapling, 1, meta & 3);
    }
    edit::break_to_air(w, x, y, z);
  }
}

void ice_tick(edit::EditWorld& w, JavaRandom& rand, int x, int y, int z) {
  (void)rand;
  // Mirrors BlockIce.updateTick (melt near block light).
  if (w.saved_block(x, y, z) > 11 - bid::light_opacity(bid::kIce)) {
    edit::set_and_notify(w, x, y, z, bid::kWaterStill, 0);
  }
}

bool flower_can_stay_tick(const edit::EditWorld& w, int id, int x, int y, int z) {
  // BlockFlower.canBlockStay for the tick pops (flowers/grass/deadbush).
  const int below = w.block_id(x, y - 1, z);
  bool soil = false;
  if (id == bid::kDeadBush) {
    soil = (below == bid::kSand);
  } else {
    soil = (below == bid::kGrass || below == bid::kDirt || below == bid::kFarmland);
  }
  if (!soil) return false;
  return full_light_value(w, x, y, z) >= 8 || w.can_see_sky(x, y, z);
}

void flower_tick(edit::EditWorld& w, JavaRandom& rand, int id, int x, int y, int z) {
  (void)rand;
  // Mirrors BlockFlower.checkFlowerChange (drop + pop).
  if (!flower_can_stay_tick(w, id, x, y, z)) {
    edit::drop_one(w, x, y, z, id, 1, w.block_meta(x, y, z));
    edit::break_to_air(w, x, y, z);
  }
}

void mushroom_tick(edit::EditWorld& w, JavaRandom& rand, int id, int x, int y, int z) {
  // Mirrors BlockMushroom.updateTick (spread when uncrowded, 1/25 gate).
  if (rand.next_int(25) != 0) return;
  int count = 5;
  for (int px = x - 4; px <= x + 4 && count > 0; ++px)
    for (int pz = z - 4; pz <= z + 4 && count > 0; ++pz)
      for (int py = y - 1; py <= y + 1 && count > 0; ++py)
        if (w.block_id(px, py, pz) == id) --count;
  if (count <= 0) return;
  const int px = x + rand.next_int(3) - 1;
  const int py = y + rand.next_int(2) - rand.next_int(2);
  const int pz = z + rand.next_int(3) - 1;
  if (w.block_id(px, py, pz) != 0) return;
  const int below = w.block_id(px, py - 1, pz);
  if (below != bid::kMycelium) {
    if (full_light_value(w, px, py, pz) >= 13) return;
    if (!(below == bid::kGrass || below == bid::kDirt || below == bid::kFarmland)) return;
  }
  edit::set_and_notify(w, px, py, pz, id, 0);
}

bool fire_can_place(const edit::EditWorld& w, int x, int y, int z) {
  // Mirrors BlockFire.canPlaceBlockAt (overworld: no Endportal clause).
  const int below = w.block_id(x, y - 1, z);
  if (below == bid::kNetherrack) return true;
  if (w.block_id(x, y, z) != 0) return false;
  return bid::is_opaque(below) || catch_fire(below) > 0;
}

int fire_neighbor_encourage(const edit::EditWorld& w, int x, int y, int z) {
  int best = 0;
  const int dx[6] = {-1, 1, 0, 0, 0, 0};
  const int dy[6] = {0, 0, -1, 1, 0, 0};
  const int dz[6] = {0, 0, 0, 0, -1, 1};
  for (int k = 0; k < 6; ++k) {
    const int v = encourage_fire(w.block_id(x + dx[k], y + dy[k], z + dz[k]));
    if (v > best) best = v;
  }
  return best;
}

void fire_tick(edit::EditWorld& w, std::vector<ScheduledTick>& sched, std::int64_t now,
               JavaRandom& rand, int x, int y, int z) {
  // Mirrors BlockFire.updateTick (no rain here: isRaining false).
  const int below = w.block_id(x, y - 1, z);
  const bool eternal = (below == bid::kNetherrack);
  if (!fire_can_place(w, x, y, z)) {
    edit::break_to_air(w, x, y, z);
  }
  if (!eternal) {
    int meta = w.block_meta(x, y, z);
    if (meta < 15) {
      edit::set_meta_notify(w, x, y, z, meta + rand.next_int(3) / 2);
      meta = w.block_meta(x, y, z);
    }
    schedule_tick(sched, now, x, y, z, bid::kFire, 40);
    bool nearby_fuel = false;
    {
      const int dx[6] = {1, -1, 0, 0, 0, 0};
      const int dy[6] = {0, 0, -1, 1, 0, 0};
      const int dz[6] = {0, 0, 0, 0, -1, 1};
      for (int k = 0; k < 6; ++k)
        if (catch_fire(w.block_id(x + dx[k], y + dy[k], z + dz[k])) > 0) nearby_fuel = true;
    }
    if (!nearby_fuel) {
      if (!w.is_normal_cube(x, y - 1, z) || meta > 3) {
        edit::break_to_air(w, x, y, z);
      }
    } else if (!eternal && catch_fire(below) <= 0 && meta == 15 && rand.next_int(4) == 0) {
      edit::break_to_air(w, x, y, z);
    } else {
      const int dx[6] = {1, -1, 0, 0, 0, 0};
      const int dy[6] = {0, 0, -1, 1, 0, 0};
      const int dz[6] = {0, 0, 0, 0, -1, 1};
      for (int k = 0; k < 6; ++k) {
        const int nx = x + dx[k], ny = y + dy[k], nz = z + dz[k];
        const int nid = w.block_id(nx, ny, nz);
        // tryToCatchBlockOnFire(n, chance, rand, meta).
        const int cf = catch_fire(nid);
        if (rand.next_int(300) < cf && meta > 0) {
          edit::set_and_notify(w, nx, ny, nz, bid::kFire, 0);
        }
      }
      for (int px = x - 1; px <= x + 1; ++px)
        for (int pz = z - 1; pz <= z + 1; ++pz)
          for (int py = y - 1; py <= y + 4; ++py) {
            if (px == x && py == y && pz == z) continue;
            int chance = 100;
            if (py > y + 1) chance += (py - (y + 1)) * 100;
            const int enc = fire_neighbor_encourage(w, px, py, pz);
            if (enc <= 0) continue;
            const int odds = (enc + 40) / (meta + 30);
            if (odds > 0 && rand.next_int(chance) <= odds) {
              int nm = meta + rand.next_int(5) / 4;
              if (nm > 15) nm = 15;
              edit::set_and_notify(w, px, py, pz, bid::kFire, nm);
            }
          }
    }
  }
}

void update_tick(edit::EditWorld& w, std::vector<ScheduledTick>& sched, std::int64_t now,
                 JavaRandom& rand, int id, int x, int y, int z) {
  using namespace bid;
  switch (id) {
    case kGrass:
      grass_tick(w, rand, x, y, z);
      break;
    case kLeaves:
      leaves_tick(w, rand, x, y, z);
      break;
    case kIce:
      ice_tick(w, rand, x, y, z);
      break;
    case kFlowerYellow:
    case kFlowerRed:
    case kTallGrass:
    case kDeadBush:
      flower_tick(w, rand, id, x, y, z);
      break;
    case kMushroomBrown:
    case kMushroomRed:
      mushroom_tick(w, rand, id, x, y, z);
      break;
    case kFire:
      fire_tick(w, sched, now, rand, x, y, z);
      break;
    default:
      break;  // crops/cactus/farmland/fluids handled elsewhere or deferred
  }
}

void tick_chunk(edit::EditWorld& w, std::vector<ScheduledTick>& sched, std::int64_t now,
                JavaRandom& rand, std::int32_t& update_lcg, int cx, int cz) {
  using namespace bid;
  const int bx = cx * 16, bz = cz * 16;
  // Ice pick (mirrors the iceandsnow section; snow needs rain: skipped).
  {
    const std::uint32_t u =
        static_cast<std::uint32_t>(update_lcg) * 3u + 1013904223u;
    update_lcg = static_cast<std::int32_t>(u);
    const int v = update_lcg >> 2;
    const int px = bx + (v & 15), pz = bz + ((v >> 8) & 15);
    const int top = w.precip_height(px, pz);
    if (top > 0 && w.temperature(px, pz) <= 0.15f && w.saved_block(px, top - 1, pz) < 10) {
      const int mid = w.block_id(px, top - 1, pz);
      if ((mid == bid::kWaterStill || mid == bid::kWaterMoving) &&
          w.block_meta(px, top - 1, pz) == 0) {
        edit::set_and_notify(w, px, top - 1, pz, bid::kIce, 0);
      }
    }
  }
  // checkLight pick.
  {
    const int lx = rand.next_int(16);
    const int ly = rand.next_int(128);
    const int lz = rand.next_int(16);
    w.relight_at(bx + lx, ly, bz + lz);
  }
  // 20 random updateTicks.
  for (int i = 0; i < 20; ++i) {
    const std::uint32_t u =
        static_cast<std::uint32_t>(update_lcg) * 3u + 1013904223u;
    update_lcg = static_cast<std::int32_t>(u);
    const int v = update_lcg >> 2;
    const int lx = v & 15;
    const int lz = (v >> 8) & 15;
    const int ly = (v >> 16) & 127;
    const int id = w.block_id(bx + lx, ly, bz + lz);
    if (id == kGrass || id == kLeaves || id == kIce || id == kFlowerYellow || id == kFlowerRed ||
        id == kTallGrass || id == kDeadBush || id == kMushroomBrown || id == kMushroomRed ||
        id == kFire) {
      update_tick(w, sched, now, rand, id, bx + lx, ly, bz + lz);
    }
  }
}

}  // namespace craftpp::world::tick
