#include "world/worldgen.hpp"

#include <cmath>

#include "core/math_helper.hpp"
#include "world/blocks.hpp"

namespace craftpp::world {

// Globally persistent BigTree heightLimit (BiomeGenBase singleton).
int FeatureGen::big_height_limit = 0;

// Every RNG draw is hoisted into a named temporary in source order (Java
// evaluates strictly left-to-right; C++ argument order is unspecified).

namespace {

int floor_d(double v) { return MathHelper::floor_double(v); }

}  // namespace

bool FeatureGen::flower_can_stay(int x, int y, int z) const {
  // getFullBlockLightValue >= 8 || canBlockSeeTheSky; fresh-world block
  // light is 0, so this reduces to sky light (mini-skylight == real thing
  // for placement: nothing above but air/leaves).
  return flower_lit(x, y, z) && flower_soil(w_.get_id(x, y - 1, z));
}

bool FeatureGen::reed_can_place(int x, int y, int z) const {
  const int below = w_.get_id(x, y - 1, z);
  if (below == bid::kReed) return true;
  if (below != bid::kGrass && below != bid::kDirt && below != bid::kSand) return false;
  auto water = [&](int id) { return id == bid::kWaterStill || id == bid::kWaterMoving; };
  return water(w_.get_id(x - 1, y - 1, z)) || water(w_.get_id(x + 1, y - 1, z)) ||
         water(w_.get_id(x, y - 1, z - 1)) || water(w_.get_id(x, y - 1, z + 1));
}

bool FeatureGen::cactus_can_stay(int x, int y, int z) const {
  auto solid = [&](int id) { return bid::material_solid(id); };
  if (solid(w_.get_id(x - 1, y, z))) return false;
  if (solid(w_.get_id(x + 1, y, z))) return false;
  if (solid(w_.get_id(x, y, z - 1))) return false;
  if (solid(w_.get_id(x, y, z + 1))) return false;
  const int below = w_.get_id(x, y - 1, z);
  return below == bid::kCactus || below == bid::kSand;
}

bool FeatureGen::cactus_can_place(int x, int y, int z) const {
  // super.canPlaceBlockAt: target air or ground cover.
  const int target = w_.get_id(x, y, z);
  if (target != 0 && !bid::is_ground_cover(target)) return false;
  return cactus_can_stay(x, y, z);
}

bool FeatureGen::pumpkin_can_place(int x, int y, int z) const {
  const int target = w_.get_id(x, y, z);
  if (target != 0 && !bid::is_ground_cover(target)) return false;
  return bid::is_normal_cube(w_.get_id(x, y - 1, z));
}

bool FeatureGen::lily_can_place(int x, int y, int z) const {
  const int target = w_.get_id(x, y, z);
  if (target != 0 && !bid::is_ground_cover(target)) return false;
  return w_.get_id(x, y - 1, z) == bid::kWaterStill;
}

bool FeatureGen::mushroom_can_place(int x, int y, int z) const {
  const int target = w_.get_id(x, y, z);
  if (target != 0 && !bid::is_ground_cover(target)) return false;
  return bid::is_opaque(w_.get_id(x, y - 1, z));
}

bool FeatureGen::minable(int x, int y, int z, int id, int size, JavaRandom& rand) {
  const float f_angle = rand.next_float();
  const float angle = f_angle * static_cast<float>(M_PI);
  const double x0 = (x + 8) + MathHelper::sin(angle) * size / 8.0F;
  const double x1 = (x + 8) - MathHelper::sin(angle) * size / 8.0F;
  const double z0 = (z + 8) + MathHelper::cos(angle) * size / 8.0F;
  const double z1 = (z + 8) - MathHelper::cos(angle) * size / 8.0F;
  const int y0draw = rand.next_int(3);
  const int y1draw = rand.next_int(3);
  const double y0 = y + y0draw - 2;
  const double y1 = y + y1draw - 2;
  for (int i = 0; i <= size; ++i) {
    const double cx = x0 + (x1 - x0) * i / size;
    const double cy = y0 + (y1 - y0) * i / size;
    const double cz = z0 + (z1 - z0) * i / size;
    const double rdraw = rand.next_double();
    const double r = rdraw * size / 16.0;
    const double rx = (MathHelper::sin(static_cast<float>(i) * static_cast<float>(M_PI) / size) + 1.0F) * r + 1.0;
    const double ry = (MathHelper::sin(static_cast<float>(i) * static_cast<float>(M_PI) / size) + 1.0F) * r + 1.0;
    const int xlo = floor_d(cx - rx / 2.0);
    const int ylo = floor_d(cy - ry / 2.0);
    const int zlo = floor_d(cz - rx / 2.0);
    const int xhi = floor_d(cx + rx / 2.0);
    const int yhi = floor_d(cy + ry / 2.0);
    const int zhi = floor_d(cz + rx / 2.0);
    for (int bx = xlo; bx <= xhi; ++bx) {
      const double dx = (bx + 0.5 - cx) / (rx / 2.0);
      if (dx * dx >= 1.0) continue;
      for (int by = ylo; by <= yhi; ++by) {
        const double dy = (by + 0.5 - cy) / (ry / 2.0);
        if (dx * dx + dy * dy >= 1.0) continue;
        for (int bz = zlo; bz <= zhi; ++bz) {
          const double dz = (bz + 0.5 - cz) / (rx / 2.0);
          if (dx * dx + dy * dy + dz * dz < 1.0 && w_.get_id(bx, by, bz) == bid::kStone) {
            w_.set_id(bx, by, bz, id);
          }
        }
      }
    }
  }
  return true;
}

bool FeatureGen::flowers(int x, int y, int z, int id, JavaRandom& rand) {
  for (int i = 0; i < 64; ++i) {
    const int dxa = rand.next_int(8);
    const int dxb = rand.next_int(8);
    const int dya = rand.next_int(4);
    const int dyb = rand.next_int(4);
    const int dza = rand.next_int(8);
    const int dzb = rand.next_int(8);
    const int px = x + dxa - dxb;
    const int py = y + dya - dyb;
    const int pz = z + dza - dzb;
    if (!w_.is_air(px, py, pz)) continue;
    if (id == bid::kFlowerYellow || id == bid::kFlowerRed) {
      if (flower_can_stay(px, py, pz)) w_.set_id(px, py, pz, id);
    } else {
      // BlockMushroom.canBlockStay: mycelium below, or (light < 13 and
      // opaque below). Fresh-world surface light is 15.
      const int below = w_.get_id(px, py - 1, pz);
      bool stay = false;
      if (below == bid::kMycelium) {
        stay = true;
      } else if (py >= 0 && py < RegionWorld::kHeight) {
        stay = w_.sky_light(px, py, pz) < 13 && bid::is_opaque(below);
      }
      if (stay) w_.set_id(px, py, pz, id);
    }
  }
  return true;
}

bool FeatureGen::tall_grass(int x, int y, int z, JavaRandom& rand) {
  while (true) {
    const int id = w_.get_id(x, y, z);
    if ((id != 0 && id != bid::kLeaves) || y <= 0) {
      for (int i = 0; i < 128; ++i) {
        const int dxa = rand.next_int(8);
        const int dxb = rand.next_int(8);
        const int dya = rand.next_int(4);
        const int dyb = rand.next_int(4);
        const int dza = rand.next_int(8);
        const int dzb = rand.next_int(8);
        const int px = x + dxa - dxb;
        const int py = y + dya - dyb;
        const int pz = z + dza - dzb;
        if (w_.is_air(px, py, pz) && flower_can_stay(px, py, pz)) {
          w_.set_id_meta(px, py, pz, bid::kTallGrass, 1);
        }
      }
      return true;
    }
    --y;
  }
}

bool FeatureGen::dead_bush(int x, int y, int z, JavaRandom& rand) {
  while (true) {
    const int id = w_.get_id(x, y, z);
    if ((id != 0 && id != bid::kLeaves) || y <= 0) {
      for (int i = 0; i < 4; ++i) {
        const int dxa = rand.next_int(8);
        const int dxb = rand.next_int(8);
        const int dya = rand.next_int(4);
        const int dyb = rand.next_int(4);
        const int dza = rand.next_int(8);
        const int dzb = rand.next_int(8);
        const int px = x + dxa - dxb;
        const int py = y + dya - dyb;
        const int pz = z + dza - dzb;
        // BlockDeadBush soil is sand ONLY (overrides flower soil).
        if (w_.is_air(px, py, pz) && flower_lit(px, py, pz) &&
            w_.get_id(px, py - 1, pz) == bid::kSand) {
          w_.set_id(px, py, pz, bid::kDeadBush);
        }
      }
      return true;
    }
    --y;
  }
}

bool FeatureGen::reed(int x, int y, int z, JavaRandom& rand) {
  for (int i = 0; i < 20; ++i) {
    const int dxa = rand.next_int(4);
    const int dxb = rand.next_int(4);
    const int dza = rand.next_int(4);
    const int dzb = rand.next_int(4);
    const int px = x + dxa - dxb;
    const int pz = z + dza - dzb;
    if (!w_.is_air(px, y, pz) || !reed_can_place(px, y, pz)) continue;
    const int h1 = rand.next_int(3);
    const int height = 2 + rand.next_int(h1 + 1);
    for (int k = 0; k < height; ++k) {
      if (reed_can_place(px, y + k, pz)) w_.set_id(px, y + k, pz, bid::kReed);
    }
  }
  return true;
}

bool FeatureGen::cactus(int x, int y, int z, JavaRandom& rand) {
  for (int i = 0; i < 10; ++i) {
    const int dxa = rand.next_int(8);
    const int dxb = rand.next_int(8);
    const int dya = rand.next_int(4);
    const int dyb = rand.next_int(4);
    const int dza = rand.next_int(8);
    const int dzb = rand.next_int(8);
    const int px = x + dxa - dxb;
    const int py = y + dya - dyb;
    const int pz = z + dza - dzb;
    if (!w_.is_air(px, py, pz)) continue;
    const int h1 = rand.next_int(3);
    const int height = 1 + rand.next_int(h1 + 1);
      for (int k = 0; k < height; ++k) {
        // NOTE: per-segment check is canBlockStay only (no air requirement),
        // unlike the initial placement gate.
        if (cactus_can_stay(px, py + k, pz)) w_.set_id(px, py + k, pz, bid::kCactus);
      }
  }
  return true;
}

bool FeatureGen::clay(int x, int y, int z, JavaRandom& rand) {
  const int mat = w_.get_id(x, y, z);
  if (mat != bid::kWaterStill && mat != bid::kWaterMoving) return false;
  const int radius = rand.next_int(4 - 2) + 2;
  for (int dx = x - radius; dx <= x + radius; ++dx) {
    for (int dz = z - radius; dz <= z + radius; ++dz) {
      const int ox = dx - x;
      const int oz = dz - z;
      if (ox * ox + oz * oz > radius * radius) continue;
      for (int dy = y - 1; dy <= y + 1; ++dy) {
        const int id = w_.get_id(dx, dy, dz);
        if (id == bid::kDirt || id == bid::kClay) w_.set_id(dx, dy, dz, bid::kClay);
      }
    }
  }
  return true;
}

bool FeatureGen::sand_patch(int x, int y, int z, int radius_param, int id, JavaRandom& rand) {
  const int mat = w_.get_id(x, y, z);
  if (mat != bid::kWaterStill && mat != bid::kWaterMoving) return false;
  const int radius = rand.next_int(radius_param - 2) + 2;
  for (int dx = x - radius; dx <= x + radius; ++dx) {
    for (int dz = z - radius; dz <= z + radius; ++dz) {
      const int ox = dx - x;
      const int oz = dz - z;
      if (ox * ox + oz * oz > radius * radius) continue;
      for (int dy = y - 2; dy <= y + 2; ++dy) {
        const int cur = w_.get_id(dx, dy, dz);
        if (cur == bid::kDirt || cur == bid::kGrass) w_.set_id(dx, dy, dz, id);
      }
    }
  }
  return true;
}

bool FeatureGen::pumpkin(int x, int y, int z, JavaRandom& rand) {
  for (int i = 0; i < 64; ++i) {
    const int dxa = rand.next_int(8);
    const int dxb = rand.next_int(8);
    const int dya = rand.next_int(4);
    const int dyb = rand.next_int(4);
    const int dza = rand.next_int(8);
    const int dzb = rand.next_int(8);
    const int px = x + dxa - dxb;
    const int py = y + dya - dyb;
    const int pz = z + dza - dzb;
    if (w_.is_air(px, py, pz) && w_.get_id(px, py - 1, pz) == bid::kGrass &&
        pumpkin_can_place(px, py, pz)) {
      w_.set_id_meta(px, py, pz, bid::kPumpkin, rand.next_int(4));
    }
  }
  return true;
}

bool FeatureGen::waterlily(int x, int y, int z, JavaRandom& rand) {
  for (int i = 0; i < 10; ++i) {
    const int dxa = rand.next_int(8);
    const int dxb = rand.next_int(8);
    const int dya = rand.next_int(4);
    const int dyb = rand.next_int(4);
    const int dza = rand.next_int(8);
    const int dzb = rand.next_int(8);
    const int px = x + dxa - dxb;
    const int py = y + dya - dyb;
    const int pz = z + dza - dzb;
    if (w_.is_air(px, py, pz) && lily_can_place(px, py, pz)) {
      w_.set_id(px, py, pz, bid::kLilyPad);
    }
  }
  return true;
}

bool FeatureGen::lake(int x, int y, int z, int id, JavaRandom& rand) {
  const int lx = x - 8;
  int ly = y;
  const int lz = z - 8;
  while (ly > 0 && w_.is_air(lx, ly, lz)) --ly;
  ly -= 4;
  bool mask[16 * 16 * 8] = {};
  const int ellipsoids = rand.next_int(4) + 4;
  for (int e = 0; e < ellipsoids; ++e) {
    const double d0 = rand.next_double();
    const double d1 = rand.next_double();
    const double d2 = rand.next_double();
    const double d3 = rand.next_double();
    const double d4 = rand.next_double();
    const double d5 = rand.next_double();
    const double ex = d0 * 6.0 + 3.0;
    const double ey = d1 * 4.0 + 2.0;
    const double ez = d2 * 6.0 + 3.0;
    const double exx = d3 * (16.0 - ex - 2.0) + 1.0 + ex / 2.0;
    const double eyy = d4 * (8.0 - ey - 4.0) + 2.0 + ey / 2.0;
    const double ezz = d5 * (16.0 - ez - 2.0) + 1.0 + ez / 2.0;
    for (int ix = 1; ix < 15; ++ix) {
      for (int iz = 1; iz < 15; ++iz) {
        for (int iy = 1; iy < 7; ++iy) {
          const double dx = (ix - exx) / (ex / 2.0);
          const double dy = (iy - eyy) / (ey / 2.0);
          const double dz = (iz - ezz) / (ez / 2.0);
          if (dx * dx + dy * dy + dz * dz < 1.0) mask[(ix * 16 + iz) * 8 + iy] = true;
        }
      }
    }
  }
  auto at = [&](int ix, int iy, int iz) -> bool {
    return mask[(ix * 16 + iz) * 8 + iy];
  };
  auto edge = [&](int ix, int iy, int iz) -> bool {
    if (at(ix, iy, iz)) return false;
    return (ix < 15 && at(ix + 1, iy, iz)) || (ix > 0 && at(ix - 1, iy, iz)) ||
           (iz < 15 && at(ix, iy, iz + 1)) || (iz > 0 && at(ix, iy, iz - 1)) ||
           (iy < 7 && at(ix, iy + 1, iz)) || (iy > 0 && at(ix, iy - 1, iz));
  };
  for (int ix = 0; ix < 16; ++ix) {
    for (int iz = 0; iz < 16; ++iz) {
      for (int iy = 0; iy < 8; ++iy) {
        if (!edge(ix, iy, iz)) continue;
        const int mid = w_.get_id(lx + ix, ly + iy, lz + iz);
        const bool liquid = mid == bid::kWaterStill || mid == bid::kWaterMoving ||
                            mid == bid::kLavaStill || mid == bid::kLavaMoving;
        if (iy >= 4 && liquid) return false;
        if (iy < 4 && !bid::material_solid(mid) && mid != id) return false;
      }
    }
  }
  for (int ix = 0; ix < 16; ++ix) {
    for (int iz = 0; iz < 16; ++iz) {
      for (int iy = 0; iy < 8; ++iy) {
        if (at(ix, iy, iz)) {
          w_.set_id(lx + ix, ly + iy, lz + iz, iy >= 4 ? 0 : id);
        }
      }
    }
  }
  for (int ix = 0; ix < 16; ++ix) {
    for (int iz = 0; iz < 16; ++iz) {
      for (int iy = 4; iy < 8; ++iy) {
        if (at(ix, iy, iz) && w_.get_id(lx + ix, ly + iy - 1, lz + iz) == bid::kDirt &&
            w_.sky_light(lx + ix, ly + iy, lz + iz) > 0) {
          const std::vector<BiomeId> b = chunks_.block_biomes(lx + ix, lz + iz, 1, 1);
          const int top = biome_def(b[0]).top_block;
          w_.set_id(lx + ix, ly + iy - 1, lz + iz,
                    top == bid::kMycelium ? bid::kMycelium : bid::kGrass);
        }
      }
    }
  }
  if (id == bid::kLavaStill || id == bid::kLavaMoving) {
    for (int ix = 0; ix < 16; ++ix) {
      for (int iz = 0; iz < 16; ++iz) {
        for (int iy = 0; iy < 8; ++iy) {
          if (!edge(ix, iy, iz)) continue;
          bool stone_it = iy < 4;
          if (!stone_it) {
            const int coin = rand.next_int(2);
            stone_it = coin != 0;
          }
          if (stone_it && bid::material_solid(w_.get_id(lx + ix, ly + iy, lz + iz))) {
            w_.set_id(lx + ix, ly + iy, lz + iz, bid::kStone);
          }
        }
      }
    }
  }
  if (id == bid::kWaterStill || id == bid::kWaterMoving) {
    // func_40471_p: cold biome + still water meta 0 (block light always 0
    // in a fresh world, like the harness world before lighting updates).
    const std::vector<float> temps = chunks_.temperatures(lx, 0, 16, 16);
    for (int ix = 0; ix < 16; ++ix) {
      for (int iz = 0; iz < 16; ++iz) {
        if (temps[static_cast<std::size_t>(ix + iz * 16)] >= 0.15F) continue;
        const int wx = lx + ix;
        const int wz = lz + iz;
        const int wy = ly + 4;
        const int mid = w_.get_id(wx, wy, wz);
        if ((mid == bid::kWaterStill || mid == bid::kWaterMoving) && w_.get_meta(wx, wy, wz) == 0) {
          w_.set_id(wx, wy, wz, bid::kIce);
        }
      }
    }
  }
  return true;
}

bool FeatureGen::liquid_spring(int x, int y, int z, int id, JavaRandom& rand) {
  if (w_.get_id(x, y + 1, z) != bid::kStone) return false;
  if (w_.get_id(x, y - 1, z) != bid::kStone) return false;
  const int cur = w_.get_id(x, y, z);
  if (cur != 0 && cur != bid::kStone) return false;
  int stone_n = 0;
  if (w_.get_id(x - 1, y, z) == bid::kStone) ++stone_n;
  if (w_.get_id(x + 1, y, z) == bid::kStone) ++stone_n;
  if (w_.get_id(x, y, z - 1) == bid::kStone) ++stone_n;
  if (w_.get_id(x, y, z + 1) == bid::kStone) ++stone_n;
  int air_n = 0;
  if (w_.is_air(x - 1, y, z)) ++air_n;
  if (w_.is_air(x + 1, y, z)) ++air_n;
  if (w_.is_air(x, y, z - 1)) ++air_n;
  if (w_.is_air(x, y, z + 1)) ++air_n;
  if (stone_n == 3 && air_n == 1) {
    // setBlockWithNotify (placement also runs onBlockAdded: harden check +
    // queued schedule, which never executes outside the cascade).
    fluid_.place_for_spring(x, y, z, id);
    // Immediate updateTick with the decorator RNG (nested scheduled updates
    // during the cascade draw World.rand instead).
    fluid_.spring_tick(x, y, z, rand);
  }
  return true;
}

bool FeatureGen::dungeon(int x, int y, int z, JavaRandom& rand) {
  const int xs = rand.next_int(2) + 2;
  const int zs = rand.next_int(2) + 2;
  int openings = 0;
  for (int bx = x - xs - 1; bx <= x + xs + 1; ++bx) {
    for (int by = y - 1; by <= y + 3 + 1; ++by) {
      for (int bz = z - zs - 1; bz <= z + zs + 1; ++bz) {
        const int id = w_.get_id(bx, by, bz);
        if (by == y - 1 && !bid::material_solid(id)) return false;
        if (by == y + 3 + 1 && !bid::material_solid(id)) return false;
        if ((bx == x - xs - 1 || bx == x + xs + 1 || bz == z - zs - 1 || bz == z + zs + 1) &&
            by == y && w_.is_air(bx, by, bz) && w_.is_air(bx, by + 1, bz)) {
          ++openings;
        }
      }
    }
  }
  if (openings < 1 || openings > 5) return false;
  for (int bx = x - xs - 1; bx <= x + xs + 1; ++bx) {
    for (int by = y + 3; by >= y - 1; --by) {
      for (int bz = z - zs - 1; bz <= z + zs + 1; ++bz) {
        if (bx != x - xs - 1 && by != y - 1 && bz != z - zs - 1 && bx != x + xs + 1 &&
            by != y + 3 + 1 && bz != z + zs + 1) {
          fluid_.place_notify(bx, by, bz, 0);
        } else if (by >= 0 && !bid::material_solid(w_.get_id(bx, by - 1, bz))) {
          fluid_.place_notify(bx, by, bz, 0);
        } else if (bid::material_solid(w_.get_id(bx, by, bz))) {
          if (by == y - 1 && rand.next_int(4) != 0) {
            fluid_.place_notify(bx, by, bz, bid::kCobbleMossy);
          } else {
            fluid_.place_notify(bx, by, bz, bid::kCobble);
          }
        }
      }
    }
  }
  for (int t = 0; t < 2; ++t) {
    for (int u = 0; u < 3; ++u) {
      const int dx = rand.next_int(xs * 2 + 1);
      const int dz = rand.next_int(zs * 2 + 1);
      const int cxp = x + dx - xs;
      const int czp = z + dz - zs;
      if (!w_.is_air(cxp, y, czp)) continue;
      int walls = 0;
      if (bid::material_solid(w_.get_id(cxp - 1, y, czp))) ++walls;
      if (bid::material_solid(w_.get_id(cxp + 1, y, czp))) ++walls;
      if (bid::material_solid(w_.get_id(cxp, y, czp - 1))) ++walls;
      if (bid::material_solid(w_.get_id(cxp, y, czp + 1))) ++walls;
      if (walls != 1) continue;
      fluid_.place_notify(cxp, y, czp, bid::kChest);
      // Loot draws replicated exactly, contents discarded (tile entities M5).
      // Chest has 27 slots (TileEntityChest.getSizeInventory).
      for (int s = 0; s < 8; ++s) {
        const int pick = rand.next_int(11);
        bool has_item = false;
        if (pick == 0 || pick == 2 || pick == 6 || pick == 10) {
          has_item = true;
        } else if (pick == 1 || pick == 3 || pick == 4 || pick == 5) {
          rand.next_int(4);
          has_item = true;
        } else if (pick == 7) {
          // Short-circuit chain: only the taken branch draws.
          has_item = rand.next_int(100) == 0;
        } else if (pick == 8) {
          const int r = rand.next_int(2);
          if (r == 0) {
            rand.next_int(4);  // redstone count
            has_item = true;
          } else {
            has_item = false;
          }
        } else if (pick == 9) {
          const int r = rand.next_int(10);
          if (r == 0) {
            rand.next_int(2);  // record pick
            has_item = true;
          } else {
            has_item = false;
          }
        }
        if (has_item) rand.next_int(27);
      }
    }
  }
  fluid_.place_notify(x, y, z, bid::kMobSpawner);
  rand.next_int(4);  // pickMobSpawner draw (mob id discarded, M5)
  return true;
}

namespace {

// Shared space-check for the canopy trees.
bool tree_space_ok(RegionWorld& w, int x, int y, int z, int height, int top_extra) {
  for (int yy = y; yy <= y + 1 + height; ++yy) {
    int r = 1;
    if (yy == y) r = 0;
    if (yy >= y + 1 + height - 2) r = 2 + top_extra;
    for (int xx = x - r; xx <= x + r; ++xx) {
      for (int zz = z - r; zz <= z + r; ++zz) {
        if (yy < 0 || yy >= RegionWorld::kHeight) return false;
        const int id = w.get_id(xx, yy, zz);
        if (id != 0 && id != bid::kLeaves) return false;
      }
    }
  }
  return true;
}

bool tree_soil_ok(RegionWorld& w, int x, int y, int z, int height) {
  const int below = w.get_id(x, y - 1, z);
  return (below == bid::kGrass || below == bid::kDirt) && y < RegionWorld::kHeight - height - 1;
}

// Corner mask with the exact draw order: the nextInt(2) fires ONLY for
// diagonal corners (mirrors the short-circuit || chain). set_meta selects
// setBlockAndMetadata (func_41060_a style) vs plain setBlock.
bool corner_place(RegionWorld& w, JavaRandom& rand, int ox, int oz, int r, int dy, int lx, int ly,
                  int lz, int leaf_id, int leaf_meta, bool set_meta) {
  const int ax = ox < 0 ? -ox : ox;
  const int az = oz < 0 ? -oz : oz;
  bool place = false;
  if (ax != r) {
    place = true;
  } else if (az != r) {
    place = true;
  } else {
    const int corner_draw = rand.next_int(2);
    if (corner_draw != 0 && dy != 0) place = true;
  }
  if (place && !bid::is_opaque(w.get_id(lx, ly, lz))) {
    if (set_meta) {
      w.set_id_meta(lx, ly, lz, leaf_id, leaf_meta);
    } else {
      w.set_id(lx, ly, lz, leaf_id);
    }
  }
  return place;
}

}  // namespace

bool FeatureGen::tree_normal(int x, int y, int z, JavaRandom& rand) {
  const int height = rand.next_int(3) + 4;
  if (y < 1 || y + height + 1 > RegionWorld::kHeight) return false;
  if (!tree_space_ok(w_, x, y, z, height, 0)) return false;
  if (!tree_soil_ok(w_, x, y, z, height)) return false;
  w_.set_id(x, y - 1, z, bid::kDirt);
  for (int ly = y - 3 + height; ly <= y + height; ++ly) {
    const int dy = ly - (y + height);
    const int r = 1 - dy / 2;
    for (int lx = x - r; lx <= x + r; ++lx) {
      for (int lz = z - r; lz <= z + r; ++lz) {
        corner_place(w_, rand, lx - x, lz - z, r, dy, lx, ly, lz, bid::kLeaves, 0, true);
      }
    }
  }
  for (int i = 0; i < height; ++i) {
    const int id = w_.get_id(x, y + i, z);
    if (id == 0 || id == bid::kLeaves) w_.set_id_meta(x, y + i, z, bid::kLog, 0);
  }
  return true;
}

bool FeatureGen::tree_forest(int x, int y, int z, JavaRandom& rand) {
  const int height = rand.next_int(3) + 5;
  if (y < 1 || y + height + 1 > RegionWorld::kHeight) return false;
  if (!tree_space_ok(w_, x, y, z, height, 0)) return false;
  if (!tree_soil_ok(w_, x, y, z, height)) return false;
  w_.set_id(x, y - 1, z, bid::kDirt);
  for (int ly = y - 3 + height; ly <= y + height; ++ly) {
    const int dy = ly - (y + height);
    const int r = 1 - dy / 2;
    for (int lx = x - r; lx <= x + r; ++lx) {
      for (int lz = z - r; lz <= z + r; ++lz) {
        corner_place(w_, rand, lx - x, lz - z, r, dy, lx, ly, lz, bid::kLeaves, 2, true);
      }
    }
  }
  for (int i = 0; i < height; ++i) {
    const int id = w_.get_id(x, y + i, z);
    if (id == 0 || id == bid::kLeaves) w_.set_id_meta(x, y + i, z, bid::kLog, 2);
  }
  return true;
}

void FeatureGen::vine_down(int x, int y, int z, int meta) {
  fluid_.place_meta_notify(x, y, z, bid::kVine, meta);
  int left = 4;
  int yy = y;
  while (true) {
    --yy;
    if (w_.get_id(x, yy, z) != 0 || left <= 0) return;
    fluid_.place_meta_notify(x, yy, z, bid::kVine, meta);
    --left;
  }
}

bool FeatureGen::tree_swamp(int x, int y, int z, JavaRandom& rand) {
  int yy = y;
  while (w_.get_id(x, yy - 1, z) == bid::kWaterStill ||
         w_.get_id(x, yy - 1, z) == bid::kWaterMoving) {
    --yy;
  }
  const int height = rand.next_int(4) + 5;
  if (yy < 1 || yy + height + 1 > RegionWorld::kHeight) return false;
  for (int ly = yy; ly <= yy + 1 + height; ++ly) {
    int r = 1;
    if (ly == yy) r = 0;
    if (ly >= yy + 1 + height - 2) r = 3;
    for (int lx = x - r; lx <= x + r; ++lx) {
      for (int lz = z - r; lz <= z + r; ++lz) {
        if (ly < 0 || ly >= RegionWorld::kHeight) return false;
        const int id = w_.get_id(lx, ly, lz);
        if (id != 0 && id != bid::kLeaves) {
          if (id != bid::kWaterStill && id != bid::kWaterMoving) return false;
          if (ly > yy) return false;
        }
      }
    }
  }
  if (!tree_soil_ok(w_, x, yy, z, height)) return false;
  w_.set_id(x, yy - 1, z, bid::kDirt);
  for (int ly = yy - 3 + height; ly <= yy + height; ++ly) {
    const int dy = ly - (yy + height);
    const int r = 2 - dy / 2;
    for (int lx = x - r; lx <= x + r; ++lx) {
      for (int lz = z - r; lz <= z + r; ++lz) {
        corner_place(w_, rand, lx - x, lz - z, r, dy, lx, ly, lz, bid::kLeaves, 0, false);
      }
    }
  }
  for (int i = 0; i < height; ++i) {
    const int id = w_.get_id(x, yy + i, z);
    if (id == 0 || id == bid::kLeaves || id == bid::kWaterMoving || id == bid::kWaterStill) {
      w_.set_id(x, yy + i, z, bid::kLog);
    }
  }
  for (int ly = yy - 3 + height; ly <= yy + height; ++ly) {
    const int dy = ly - (yy + height);
    const int r = 2 - dy / 2;
    for (int lx = x - r; lx <= x + r; ++lx) {
      for (int lz = z - r; lz <= z + r; ++lz) {
        if (w_.get_id(lx, ly, lz) != bid::kLeaves) continue;
        const int d0 = rand.next_int(4);
        if (d0 == 0 && w_.get_id(lx - 1, ly, lz) == 0) vine_down(lx - 1, ly, lz, 8);
        const int d1 = rand.next_int(4);
        if (d1 == 0 && w_.get_id(lx + 1, ly, lz) == 0) vine_down(lx + 1, ly, lz, 2);
        const int d2 = rand.next_int(4);
        if (d2 == 0 && w_.get_id(lx, ly, lz - 1) == 0) vine_down(lx, ly, lz - 1, 1);
        const int d3 = rand.next_int(4);
        if (d3 == 0 && w_.get_id(lx, ly, lz + 1) == 0) vine_down(lx, ly, lz + 1, 4);
      }
    }
  }
  return true;
}

bool FeatureGen::tree_taiga1(int x, int y, int z, JavaRandom& rand) {
  const int height = rand.next_int(5) + 7;
  const int h1 = height - rand.next_int(2) - 3;
  const int trunk = height - h1;
  const int top_r = 1 + rand.next_int(trunk + 1);
  if (y < 1 || y + height + 1 > RegionWorld::kHeight) return false;
  for (int ly = y; ly <= y + 1 + height; ++ly) {
    const int r = (ly - y < h1) ? 0 : top_r;
    for (int lx = x - r; lx <= x + r; ++lx) {
      for (int lz = z - r; lz <= z + r; ++lz) {
        if (ly < 0 || ly >= RegionWorld::kHeight) return false;
        const int id = w_.get_id(lx, ly, lz);
        if (id != 0 && id != bid::kLeaves) return false;
      }
    }
  }
  if (!tree_soil_ok(w_, x, y, z, height)) return false;
  w_.set_id(x, y - 1, z, bid::kDirt);
  int r = 0;
  for (int ly = y + height; ly >= y + h1; --ly) {
    for (int lx = x - r; lx <= x + r; ++lx) {
      const int ox = lx - x;
      for (int lz = z - r; lz <= z + r; ++lz) {
        const int oz = lz - z;
        const int ax = ox < 0 ? -ox : ox;
        const int az = oz < 0 ? -oz : oz;
        if ((ax != r || az != r || r <= 0) && !bid::is_opaque(w_.get_id(lx, ly, lz))) {
          w_.set_id_meta(lx, ly, lz, bid::kLeaves, 1);
        }
      }
    }
    if (r >= 1 && ly == y + h1 + 1) {
      --r;
    } else if (r < top_r) {
      ++r;
    }
  }
  for (int i = 0; i < height - 1; ++i) {
    const int id = w_.get_id(x, y + i, z);
    if (id == 0 || id == bid::kLeaves) w_.set_id_meta(x, y + i, z, bid::kLog, 1);
  }
  return true;
}

bool FeatureGen::tree_taiga2(int x, int y, int z, JavaRandom& rand) {
  const int height = rand.next_int(4) + 6;
  const int base = 1 + rand.next_int(2);
  const int top = height - base;
  const int top_r = 2 + rand.next_int(2);
  if (y < 1 || y + height + 1 > RegionWorld::kHeight) return false;
  for (int ly = y; ly <= y + 1 + height; ++ly) {
    const int r = (ly - y < base) ? 0 : top_r;
    for (int lx = x - r; lx <= x + r; ++lx) {
      for (int lz = z - r; lz <= z + r; ++lz) {
        if (ly < 0 || ly >= RegionWorld::kHeight) return false;
        const int id = w_.get_id(lx, ly, lz);
        if (id != 0 && id != bid::kLeaves) return false;
      }
    }
  }
  if (!tree_soil_ok(w_, x, y, z, height)) return false;
  w_.set_id(x, y - 1, z, bid::kDirt);
  const int rdraw = rand.next_int(2);
  int r = rdraw;  // var21 starts at the draw, not 1
  int step = 1;   // var13
  int reset = 0;  // var22
  for (int i = 0; i <= top; ++i) {
    const int ly = y + height - i;
    for (int lx = x - r; lx <= x + r; ++lx) {
      const int ox = lx - x;
      for (int lz = z - r; lz <= z + r; ++lz) {
        const int oz = lz - z;
        const int ax = ox < 0 ? -ox : ox;
        const int az = oz < 0 ? -oz : oz;
        if ((ax != r || az != r || r <= 0) && !bid::is_opaque(w_.get_id(lx, ly, lz))) {
          w_.set_id_meta(lx, ly, lz, bid::kLeaves, 1);
        }
      }
    }
    if (r >= step) {
      r = reset;
      reset = 1;
      ++step;
      if (step > top_r) step = top_r;
    } else {
      ++r;
    }
  }
  const int trunk_cut = rand.next_int(3);
  for (int i = 0; i < height - trunk_cut; ++i) {
    const int id = w_.get_id(x, y + i, z);
    if (id == 0 || id == bid::kLeaves) w_.set_id_meta(x, y + i, z, bid::kLog, 1);
  }
  return true;
}

bool FeatureGen::big_mushroom(int x, int y, int z, JavaRandom& rand) {
  const int type_draw = rand.next_int(2);
  const int type = type_draw;  // decorator uses field -1 (random type)
  const int height = rand.next_int(3) + 4;
  if (y < 1 || y + height + 1 > RegionWorld::kHeight) return false;
  for (int ly = y; ly <= y + 1 + height; ++ly) {
    int r = 3;
    if (ly == y) r = 0;
    for (int lx = x - r; lx <= x + r; ++lx) {
      for (int lz = z - r; lz <= z + r; ++lz) {
        if (ly < 0 || ly >= RegionWorld::kHeight) return false;
        const int id = w_.get_id(lx, ly, lz);
        if (id != 0 && id != bid::kLeaves) return false;
      }
    }
  }
  const int below = w_.get_id(x, y - 1, z);
  if (below != bid::kDirt && below != bid::kGrass && below != bid::kMycelium) return false;
  if (!mushroom_can_place(x, y, z)) return false;
  w_.set_id(x, y - 1, z, bid::kDirt);
  int cap_top = y + height;
  if (type == 1) cap_top = y + height - 3;
  for (int ly = cap_top; ly <= y + height; ++ly) {
    int r = 1;
    if (ly < y + height) ++r;
    if (type == 0) r = 3;
    for (int lx = x - r; lx <= x + r; ++lx) {
      for (int lz = z - r; lz <= z + r; ++lz) {
        int meta = 5;
        if (lx == x - r) --meta;
        if (lx == x + r) ++meta;
        if (lz == z - r) meta -= 3;
        if (lz == z + r) meta += 3;
        if (type == 0 || ly < y + height) {
          if ((lx == x - r || lx == x + r) && (lz == z - r || lz == z + r)) continue;
          if (lx == x - (r - 1) && lz == z - r) meta = 1;
          if (lx == x - r && lz == z - (r - 1)) meta = 1;
          if (lx == x + (r - 1) && lz == z - r) meta = 3;
          if (lx == x + r && lz == z - (r - 1)) meta = 3;
          if (lx == x - (r - 1) && lz == z + r) meta = 7;
          if (lx == x - r && lz == z + (r - 1)) meta = 7;
          if (lx == x + (r - 1) && lz == z + r) meta = 9;
          if (lx == x + r && lz == z + (r - 1)) meta = 9;
        }
        if (meta == 5 && ly < y + height) meta = 0;
        if ((meta != 0 || y >= y + height - 1) && !bid::is_opaque(w_.get_id(lx, ly, lz))) {
          w_.set_id_meta(lx, ly, lz,
                         type == 0 ? bid::kMushroomCapBrown : bid::kMushroomCapRed, meta);
        }
      }
    }
  }
  for (int i = 0; i < height; ++i) {
    const int id = w_.get_id(x, y + i, z);
    if (!bid::is_opaque(id)) {
      w_.set_id_meta(x, y + i, z, bid::kMushroomCapBrown + type, 10);
    }
  }
  return true;
}

namespace {

// Axis pairs for the big-tree line walker (otherCoordPairs).
int other_axis_1(int m) {
  if (m == 0) return 2;
  return 0;
}
int other_axis_2(int m) {
  if (m == 0) return 1;
  return m == 1 ? 2 : 1;
}

}  // namespace

// Returns -1 when the segment is clear, else |distance| of first blockage.
int big_check_line(RegionWorld& w, const int a[3], const int b[3]) {
  int d[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
  int m = 0;
  for (int k = 0; k < 3; ++k) {
    if (std::abs(d[k]) > std::abs(d[m])) m = k;
  }
  if (d[m] == 0) return -1;
  const int o1 = other_axis_1(m);
  const int o2 = other_axis_2(m);
  const int sgn = d[m] > 0 ? 1 : -1;
  const double s1 = static_cast<double>(d[o1]) / d[m];
  const double s2 = static_cast<double>(d[o2]) / d[m];
  int dist = 0;
  const int lim = d[m] + sgn;
  for (int i = d[m] + sgn; dist != lim; dist += sgn) {
    (void)i;
    int p[3];
    p[m] = a[m] + dist;
    p[o1] = floor_d(a[o1] + dist * s1);
    p[o2] = floor_d(a[o2] + dist * s2);
    const int id = w.get_id(p[0], p[1], p[2]);
    if (id != 0 && id != bid::kLeaves) return std::abs(dist);
  }
  return -1;
}

void big_place_line(RegionWorld& w, const int a[3], const int b[3], int id) {
  int d[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
  int m = 0;
  for (int k = 0; k < 3; ++k) {
    if (std::abs(d[k]) > std::abs(d[m])) m = k;
  }
  if (d[m] == 0) return;
  const int o1 = other_axis_1(m);
  const int o2 = other_axis_2(m);
  const int sgn = d[m] > 0 ? 1 : -1;
  const double s1 = static_cast<double>(d[o1]) / d[m];
  const double s2 = static_cast<double>(d[o2]) / d[m];
  for (int dist = 0, lim = d[m] + sgn; dist != lim; dist += sgn) {
    int p[3];
    p[m] = a[m] + dist;
    p[o1] = floor_d(a[o1] + dist * s1 + 0.5);
    p[o2] = floor_d(a[o2] + dist * s2 + 0.5);
    w.set_id_meta(p[0], p[1], p[2], id, 0);
  }
}

bool FeatureGen::tree_big(int x, int y, int z, JavaRandom& rand) {  // func_517_a(1,1,1) as invoked by the decorator.
  big_tree_.limit_limit = 12;
  big_tree_.leaf_dist = 5;
  BigTree& t = big_tree_;
  const long seed_draw = rand.next_long();
  t.rand.set_seed(seed_draw);
  t.base[0] = x;
  t.base[1] = y;
  t.base[2] = z;
  if (big_height_limit == 0) {
    const int hdraw = t.rand.next_int(t.limit_limit);
    big_height_limit = 5 + hdraw;
  }
  // validTreeLocation
  {
    const int soil = w_.get_id(x, y - 1, z);
    if (soil != bid::kGrass && soil != bid::kDirt) return false;
    const int top[3] = {x, y + big_height_limit - 1, z};
    const int base[3] = {x, y, z};
    const int blocked = big_check_line(w_, base, top);
    if (blocked == -1) {
      // clear
    } else if (blocked < 6) {
      return false;
    } else {
      big_height_limit = blocked;
    }
  }
  // generateLeafNodeList
  t.height = static_cast<int>(big_height_limit * 0.618);
  if (t.height >= big_height_limit) t.height = big_height_limit - 1;
  int node_cap = static_cast<int>(1.382 + std::pow(1.0 * big_height_limit / 13.0, 2.0));
  if (node_cap < 1) node_cap = 1;
  t.leaf_nodes.clear();
  int top_y = y + big_height_limit - t.leaf_dist;
  const int trunk_top = y + t.height;
  int remaining = top_y - y;
  t.leaf_nodes.push_back({x, top_y, z, trunk_top});
  --top_y;
  while (remaining >= 0) {
    float shape;
    if (remaining < big_height_limit * 0.3) {
      shape = -1.618F;
    } else {
      const float half = big_height_limit / 2.0F;
      const float dh = big_height_limit / 2.0F - remaining;
      float r;
      if (dh == 0.0F) {
        r = half;
      } else if (std::abs(dh) >= half) {
        r = 0.0F;
      } else {
        r = static_cast<float>(std::sqrt(std::abs(half) * std::abs(half) - std::abs(dh) * std::abs(dh)));
      }
      shape = r * 0.5F;
    }
    if (shape < 0.0F) {
      --top_y;
      --remaining;
    } else {
      for (int n = 0; n < node_cap; ++n) {
        const float f1 = t.rand.next_float();
        const float spread = 1.0F * shape * (f1 + 0.328F);
        const float f2 = t.rand.next_float();
        const float ang = f2 * 2.0F * 3.14159F;
        const int nx = floor_d(spread * std::sin(ang) + x + 0.5);
        const int nz = floor_d(spread * std::cos(ang) + z + 0.5);
        const int p0[3] = {nx, top_y, nz};
        const int p1[3] = {nx, top_y + t.leaf_dist, nz};
        if (big_check_line(w_, p0, p1) != -1) continue;
        const double dh = std::sqrt(std::pow(std::abs(x - nx), 2.0) + std::pow(std::abs(z - nz), 2.0));
        const double grow = dh * 0.381;
        int by = 0;
        if (top_y - grow > trunk_top) {
          by = trunk_top;
        } else {
          by = static_cast<int>(top_y - grow);
        }
        const int pb[3] = {x, by, z};
        const int pn[3] = {nx, top_y, nz};
        if (big_check_line(w_, pb, pn) != -1) continue;
        t.leaf_nodes.push_back({nx, top_y, nz, by});
      }
      --top_y;
      --remaining;
    }
  }
  // generateLeaves: disc per node via leaf_radius.
  for (const auto& node : t.leaf_nodes) {
    for (int ly = node.y; ly < node.y + t.leaf_dist; ++ly) {
      const int dy = ly - node.y;
      float rad;
      if (dy >= 0 && dy < t.leaf_dist) {
        rad = (dy != 0 && dy != t.leaf_dist - 1) ? 3.0F : 2.0F;
      } else {
        continue;
      }
      const int ir = static_cast<int>(rad + 0.618);
      for (int ox = -ir; ox <= ir; ++ox) {
        for (int oz = -ir; oz <= ir; ++oz) {
          const double dd = std::sqrt(std::pow(std::abs(ox) + 0.5, 2.0) +
                                      std::pow(std::abs(oz) + 0.5, 2.0));
          if (dd > rad) continue;
          const int id = w_.get_id(node.x + ox, ly, node.z + oz);
          if (id == 0 || id == bid::kLeaves) {
            w_.set_id_meta(node.x + ox, ly, node.z + oz, bid::kLeaves, 0);
          }
        }
      }
    }
  }
  // generateTrunk (trunkSize == 1).
  {
    const int a[3] = {x, y, z};
    const int b[3] = {x, y + t.height, z};
    big_place_line(w_, a, b, bid::kLog);
  }
  // generateLeafNodeBases.
  for (const auto& node : t.leaf_nodes) {
    if (node.base_y - y < big_height_limit * 0.2) continue;
    const int a[3] = {x, node.base_y, z};
    const int b[3] = {node.x, node.y, node.z};
    big_place_line(w_, a, b, bid::kLog);
  }
  return true;
}

void FeatureGen::decorate(BiomeId biome, int chunk_x, int chunk_z, JavaRandom& rand) {
  const DecorParams& p = decor_params(biome);
  auto ore1 = [&](int count, int id, int size, int ymin, int ymax) {
    for (int i = 0; i < count; ++i) {
      const int dx = rand.next_int(16);
      const int dy = rand.next_int(ymax - ymin) + ymin;
      const int dz = rand.next_int(16);
      minable(chunk_x + dx, dy, chunk_z + dz, id, size, rand);
    }
  };
  auto ore2 = [&](int count, int id, int size, int ymin, int ymax) {
    for (int i = 0; i < count; ++i) {
      const int dx = rand.next_int(16);
      const int dy1 = rand.next_int(ymax);
      const int dy2 = rand.next_int(ymax);
      const int dy = dy1 + dy2 + (ymin - ymax);
      const int dz = rand.next_int(16);
      minable(chunk_x + dx, dy, chunk_z + dz, id, size, rand);
    }
  };
  ore1(20, bid::kDirt, 32, 0, RegionWorld::kHeight);
  ore1(10, bid::kGravel, 32, 0, RegionWorld::kHeight);
  ore1(20, bid::kCoalOre, 16, 0, RegionWorld::kHeight);
  ore1(20, bid::kIronOre, 8, 0, RegionWorld::kHeight / 2);
  ore1(2, bid::kGoldOre, 8, 0, RegionWorld::kHeight / 4);
  ore1(8, bid::kRedstoneOre, 7, 0, RegionWorld::kHeight / 8);
  ore1(1, bid::kDiamondOre, 7, 0, RegionWorld::kHeight / 8);
  ore2(1, bid::kLapisOre, 6, RegionWorld::kHeight / 8, RegionWorld::kHeight / 8);

  for (int i = 0; i < p.sand2; ++i) {
    const int dx = rand.next_int(16);
    const int dz = rand.next_int(16);
    sand_patch(chunk_x + dx + 8, w_.top_solid_or_liquid(chunk_x + dx + 8, chunk_z + dz + 8),
               chunk_z + dz + 8, 7, bid::kSand, rand);
  }
  for (int i = 0; i < p.clay; ++i) {
    const int dx = rand.next_int(16);
    const int dz = rand.next_int(16);
    clay(chunk_x + dx + 8, w_.top_solid_or_liquid(chunk_x + dx + 8, chunk_z + dz + 8),
         chunk_z + dz + 8, rand);
  }
  for (int i = 0; i < p.sand; ++i) {
    const int dx = rand.next_int(16);
    const int dz = rand.next_int(16);
    sand_patch(chunk_x + dx + 8, w_.top_solid_or_liquid(chunk_x + dx + 8, chunk_z + dz + 8),
               chunk_z + dz + 8, 7, bid::kSand, rand);
  }

  int trees = p.trees;
  const int tree_gate = rand.next_int(10);
  if (tree_gate == 0) ++trees;
  for (int i = 0; i < trees; ++i) {
    const int dx = rand.next_int(16);
    const int dz = rand.next_int(16);
    const TreeKind kind = pick_tree(biome, rand);
    const int px = chunk_x + dx + 8;
    const int pz = chunk_z + dz + 8;
    const int py = w_.height_value(px, pz);
    switch (kind) {
      case TreeKind::Normal:
        tree_normal(px, py, pz, rand);
        break;
      case TreeKind::Big:
        tree_big(px, py, pz, rand);
        break;
      case TreeKind::Forest:
        tree_forest(px, py, pz, rand);
        break;
      case TreeKind::Swamp:
        tree_swamp(px, py, pz, rand);
        break;
      case TreeKind::Taiga1:
        // inline taiga-1 (WorldGenTaiga1 has no state)
        tree_taiga1(px, py, pz, rand);
        break;
      case TreeKind::Taiga2:
        tree_taiga2(px, py, pz, rand);
        break;
    }

  }

  for (int i = 0; i < p.big_mushroom; ++i) {
    const int dx = rand.next_int(16);
    const int dz = rand.next_int(16);
    big_mushroom(chunk_x + dx + 8, w_.height_value(chunk_x + dx + 8, chunk_z + dz + 8),
                 chunk_z + dz + 8, rand);
  }

  for (int i = 0; i < p.flowers; ++i) {
    const int dx = rand.next_int(16);
    const int dy = rand.next_int(RegionWorld::kHeight);
    const int dz = rand.next_int(16);
    flowers(chunk_x + dx + 8, dy, chunk_z + dz + 8, bid::kFlowerYellow, rand);
    const int red_gate = rand.next_int(4);
    if (red_gate == 0) {
      const int rx = rand.next_int(16);
      const int ry = rand.next_int(RegionWorld::kHeight);
      const int rz = rand.next_int(16);
      flowers(chunk_x + rx + 8, ry, chunk_z + rz + 8, bid::kFlowerRed, rand);
    }
  }

  for (int i = 0; i < p.grass; ++i) {
    const int dx = rand.next_int(16);
    const int dy = rand.next_int(RegionWorld::kHeight);
    const int dz = rand.next_int(16);
    tall_grass(chunk_x + dx + 8, dy, chunk_z + dz + 8, rand);
  }

  for (int i = 0; i < p.dead_bush; ++i) {
    const int dx = rand.next_int(16);
    const int dy = rand.next_int(RegionWorld::kHeight);
    const int dz = rand.next_int(16);
    dead_bush(chunk_x + dx + 8, dy, chunk_z + dz + 8, rand);
  }

  for (int i = 0; i < p.waterlily; ++i) {
    const int dx = rand.next_int(16);
    const int dz = rand.next_int(16);
    int wy = rand.next_int(RegionWorld::kHeight);
    while (wy > 0 && w_.get_id(chunk_x + dx + 8, wy - 1, chunk_z + dz + 8) == 0) --wy;
    waterlily(chunk_x + dx + 8, wy, chunk_z + dz + 8, rand);
  }

  for (int i = 0; i < p.mushrooms; ++i) {
    const int brown_gate = rand.next_int(4);
    if (brown_gate == 0) {
      const int dx = rand.next_int(16);
      const int dz = rand.next_int(16);
      const int px = chunk_x + dx + 8;
      const int pz = chunk_z + dz + 8;
      flowers(px, w_.height_value(px, pz), pz, bid::kMushroomBrown, rand);
    }
    const int red_gate = rand.next_int(8);
    if (red_gate == 0) {
      const int dx = rand.next_int(16);
      const int dz = rand.next_int(16);
      const int dy = rand.next_int(RegionWorld::kHeight);
      flowers(chunk_x + dx + 8, dy, chunk_z + dz + 8, bid::kMushroomRed, rand);
    }
  }

  {
    const int gate = rand.next_int(4);
    if (gate == 0) {
      const int dx = rand.next_int(16);
      const int dy = rand.next_int(RegionWorld::kHeight);
      const int dz = rand.next_int(16);
      flowers(chunk_x + dx + 8, dy, chunk_z + dz + 8, bid::kMushroomBrown, rand);
    }
  }
  {
    const int gate = rand.next_int(8);
    if (gate == 0) {
      const int dx = rand.next_int(16);
      const int dy = rand.next_int(RegionWorld::kHeight);
      const int dz = rand.next_int(16);
      flowers(chunk_x + dx + 8, dy, chunk_z + dz + 8, bid::kMushroomRed, rand);
    }
  }

  for (int i = 0; i < p.reeds; ++i) {
    const int dx = rand.next_int(16);
    const int dz = rand.next_int(16);
    const int dy = rand.next_int(RegionWorld::kHeight);
    reed(chunk_x + dx + 8, dy, chunk_z + dz + 8, rand);
  }

  for (int i = 0; i < 10; ++i) {
    const int dx = rand.next_int(16);
    const int dy = rand.next_int(RegionWorld::kHeight);
    const int dz = rand.next_int(16);
    reed(chunk_x + dx + 8, dy, chunk_z + dz + 8, rand);
  }

  {
    const int gate = rand.next_int(32);
    if (gate == 0) {
      const int dx = rand.next_int(16);
      const int dy = rand.next_int(RegionWorld::kHeight);
      const int dz = rand.next_int(16);
      pumpkin(chunk_x + dx + 8, dy, chunk_z + dz + 8, rand);
    }
  }

  for (int i = 0; i < p.cacti; ++i) {
    const int dx = rand.next_int(16);
    const int dy = rand.next_int(RegionWorld::kHeight);
    const int dz = rand.next_int(16);
    cactus(chunk_x + dx + 8, dy, chunk_z + dz + 8, rand);
  }

  // Fluid springs (WorldGenLiquids): placement + immediate updateTick spread
  // via FluidSim. Nested scheduled updates draw World.rand (test-pinned).
  for (int i = 0; i < 50; ++i) {
    const int dx = rand.next_int(16);
    const int dy = rand.next_int(rand.next_int(RegionWorld::kHeight - 8) + 8);
    const int dz = rand.next_int(16);
    liquid_spring(chunk_x + dx + 8, dy, chunk_z + dz + 8, bid::kWaterMoving, rand);
  }
  for (int i = 0; i < 20; ++i) {
    const int dx = rand.next_int(16);
    const int d1 = rand.next_int(RegionWorld::kHeight - 16);
    const int d2 = rand.next_int(d1 + 8);
    const int dy = rand.next_int(d2 + 8);
    const int dz = rand.next_int(16);
    liquid_spring(chunk_x + dx + 8, dy, chunk_z + dz + 8, bid::kLavaMoving, rand);
  }
}

void populate_chunk(RegionWorld& world, const ChunkManager& chunks, std::int64_t world_seed, int cx,
                    int cz, JavaRandom& world_rand) {
  JavaRandom rand(world_seed);
  const std::int64_t s1part = rand.next_long();
  const std::int64_t s1 = s1part / 2 * 2 + 1;
  const std::int64_t s2part = rand.next_long();
  const std::int64_t s2 = s2part / 2 * 2 + 1;
  // (long)cx * s1 + (long)cz * s2 ^ worldSeed, wrapping like Java.
  const std::uint64_t mixed = static_cast<std::uint64_t>(static_cast<std::int64_t>(cx) * s1) +
                              static_cast<std::uint64_t>(static_cast<std::int64_t>(cz) * s2);
  rand.set_seed(static_cast<std::int64_t>(
      mixed ^ static_cast<std::uint64_t>(world_seed)));
  FeatureGen gen(world, chunks, world_rand);

  // Lakes (structures skipped: mapFeatures=false keeps the false flag, so
  // these gates still run).
  {
    const int gate = rand.next_int(4);
    if (gate == 0) {
      const int lx = cx * 16 + rand.next_int(16) + 8;
      const int ly = rand.next_int(RegionWorld::kHeight);
      const int lz = cz * 16 + rand.next_int(16) + 8;
      gen.lake(lx, ly, lz, bid::kWaterStill, rand);
    }
  }
  {
    const int gate = rand.next_int(8);
    if (gate == 0) {
      const int lx = cx * 16 + rand.next_int(16) + 8;
      const int inner = rand.next_int(RegionWorld::kHeight - 8);
      const int ly = rand.next_int(inner + 8);
      const int lz = cz * 16 + rand.next_int(16) + 8;
      if (ly < 63 || rand.next_int(10) == 0) {
        gen.lake(lx, ly, lz, bid::kLavaStill, rand);
      }
    }
  }

  for (int i = 0; i < 8; ++i) {
    const int dx = cx * 16 + rand.next_int(16) + 8;
    const int dy = rand.next_int(RegionWorld::kHeight);
    const int dz = cz * 16 + rand.next_int(16) + 8;
    gen.dungeon(dx, dy, dz, rand);
  }

  // Biome at the chunk corner+16 like the source (var4+16, var5+16 pre-shift).
  const std::vector<BiomeId> b = chunks.block_biomes(cx * 16 + 16, cz * 16 + 16, 1, 1);
  gen.decorate(b[0], cx * 16, cz * 16, rand);

  // Ice/snow cap over the +8 quadrant (setBlockWithNotify: notifies like the
  // source; SpawnerAnimals has no block effects and is skipped).
  const std::vector<float> cap_temps = chunks.temperatures(cx * 16 + 8, cz * 16 + 8, 16, 16);
  for (int lx = 0; lx < 16; ++lx) {
    for (int lz = 0; lz < 16; ++lz) {
      const int x = cx * 16 + 8 + lx;
      const int z = cz * 16 + 8 + lz;
      const int top = world.height_value(x, z);
      const float temp = cap_temps[static_cast<std::size_t>(lx + lz * 16)];
      // func_40471_p at (x, top-1, z).
      if (temp <= 0.15F) {
        const int mid = world.get_id(x, top - 1, z);
        if ((mid == bid::kWaterStill || mid == bid::kWaterMoving) &&
            world.get_meta(x, top - 1, z) == 0) {
          gen.place_ice_snow(x, top - 1, z, bid::kIce);
        }
      }
      // func_40478_r at (x, top, z).
      if (temp <= 0.15F && world.get_id(x, top, z) == 0) {
        const int below = world.get_id(x, top - 1, z);
        if (below != 0 && below != bid::kIce && bid::is_opaque(below) &&
            bid::material_solid(below)) {
          gen.place_ice_snow(x, top, z, bid::kSnowCover);
        }
      }
    }
  }
}

}  // namespace craftpp::world
