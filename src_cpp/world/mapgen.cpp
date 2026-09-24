#include "world/mapgen.hpp"

#include <cmath>

#include "core/math_helper.hpp"
#include "world/biome.hpp"
#include "world/blocks.hpp"

namespace craftpp::world {

namespace {

inline std::size_t raw_index(int lx, int y, int lz) {
  return static_cast<std::size_t>((lx * 16 + lz) * 128 + y);
}

int biome_top_at(const ChunkManager& manager, int x, int z) {
  const std::vector<BiomeId> b = manager.block_biomes(x, z, 1, 1);
  return biome_def(b[0]).top_block;
}

}  // namespace

void MapGenCaves::generate(std::int64_t world_seed, int cx, int cz,
                           std::vector<std::int8_t>& blocks) {
  constexpr int kRange = 8;
  rand_.set_seed(world_seed);
  const std::int64_t s1 = rand_.next_long();
  const std::int64_t s2 = rand_.next_long();
  for (int nx = cx - kRange; nx <= cx + kRange; ++nx) {
    for (int nz = cz - kRange; nz <= cz + kRange; ++nz) {
      // (long)nx * s1 ^ (long)nz * s2 ^ worldSeed, wrapping like Java.
      const std::uint64_t mixed = static_cast<std::uint64_t>(static_cast<std::int64_t>(nx) * s1) ^
                                  static_cast<std::uint64_t>(static_cast<std::int64_t>(nz) * s2) ^
                                  static_cast<std::uint64_t>(world_seed);
      rand_.set_seed(static_cast<std::int64_t>(mixed));
      recursive_generate(rand_, nx, nz, cx, cz, blocks);
    }
  }
}

void MapGenCaves::recursive_generate(JavaRandom& rand, int nx, int nz, int cx, int cz,
                                     std::vector<std::int8_t>& blocks) {
  int count = rand.next_int(rand.next_int(rand.next_int(40) + 1) + 1);
  if (rand.next_int(15) != 0) count = 0;
  for (int i = 0; i < count; ++i) {
    const double x = nx * 16 + rand.next_int(16);
    const double y = rand.next_int(rand.next_int(120) + 8);
    const double z = nz * 16 + rand.next_int(16);
    int branches = 1;
    if (rand.next_int(4) == 0) {
      large_node(rand.next_long(), cx, cz, blocks, x, y, z);
      branches += rand.next_int(4);
    }
    for (int b = 0; b < branches; ++b) {
      const float yaw = rand.next_float() * static_cast<float>(M_PI) * 2.0F;
      const float pitch = (rand.next_float() - 0.5F) * 2.0F / 8.0F;
      const float size_a = rand.next_float();
      const float size_b = rand.next_float();
      float size = size_a * 2.0F + size_b;
      if (rand.next_int(10) == 0) {
        const float boost_a = rand.next_float();
        const float boost_b = rand.next_float();
        size *= boost_a * boost_b * 3.0F + 1.0F;
      }
      cave_node(rand, rand.next_long(), cx, cz, blocks, x, y, z, size, yaw, pitch, 0, 0, 1.0);
    }
  }
}

void MapGenCaves::large_node(long seed, int cx, int cz, std::vector<std::int8_t>& blocks, double x,
                             double y, double z) {
  cave_node(rand_, seed, cx, cz, blocks, x, y, z, 1.0F + rand_.next_float() * 6.0F, 0.0F, 0.0F, -1,
            -1, 0.5);
}

void MapGenCaves::debug_cave_node(long seed, int cx, int cz, std::vector<std::int8_t>& blocks,
                                  double x, double y, double z, float size, float yaw,
                                  float pitch, int start, int end) {
  JavaRandom unused(0);
  cave_node(unused, seed, cx, cz, blocks, x, y, z, size, yaw, pitch, start, end, 1.0);
}

void MapGenCaves::debug_large_node(long seed, int cx, int cz, std::vector<std::int8_t>& blocks,
                                   double x, double y, double z) {
  large_node(seed, cx, cz, blocks, x, y, z);
}

void MapGenCaves::debug_seed(long seed) { rand_.set_seed(seed); }

void MapGenCaves::cave_node(JavaRandom& local, long seed, int cx, int cz,
                            std::vector<std::int8_t>& blocks, double x, double y, double z,
                            float size, float yaw, float pitch, int start, int end,
                            double height_scale) {
  (void)local;  // draws below use rand_ for the large node and a fresh local stream otherwise
  const double cx8 = cx * 16 + 8;
  const double cz8 = cz * 16 + 8;
  float yaw_vel = 0.0F;
  float pitch_vel = 0.0F;
  JavaRandom lr(seed);
  if (end <= 0) {
    const int range = 8 * 16 - 16;
    end = range - lr.next_int(range / 4);
  }
  bool no_bifurcation = false;
  if (start == -1) {
    start = end / 2;
    no_bifurcation = true;
  }
  const int branch_at = lr.next_int(end / 2) + end / 4;
  const bool thin = lr.next_int(6) == 0;

  for (; start < end; ++start) {
    const double width =
        1.5 + MathHelper::sin(static_cast<float>(start) * static_cast<float>(M_PI) / end) * size;
    const double height = width * height_scale;
    const float cos_pitch = MathHelper::cos(pitch);
    const float sin_pitch = MathHelper::sin(pitch);
    x += MathHelper::cos(yaw) * cos_pitch;
    y += sin_pitch;
    z += MathHelper::sin(yaw) * cos_pitch;
    pitch = thin ? pitch * 0.92F : pitch * 0.7F;
    pitch += pitch_vel * 0.1F;
    yaw += yaw_vel * 0.1F;
    pitch_vel *= 0.9F;
    yaw_vel *= 12.0F / 16.0F;
    // NOTE: every RNG draw is hoisted into a named temporary in source order.
    // C++ leaves argument evaluation order unspecified while Java evaluates
    // strictly left-to-right; sequencing draws explicitly is load-bearing.
    {
      const float pv_a = lr.next_float();
      const float pv_b = lr.next_float();
      const float pv_c = lr.next_float();
      const float yv_a = lr.next_float();
      const float yv_b = lr.next_float();
      const float yv_c = lr.next_float();
      pitch_vel += (pv_a - pv_b) * pv_c * 2.0F;
      yaw_vel += (yv_a - yv_b) * yv_c * 4.0F;
    }


    if (!no_bifurcation && start == branch_at && size > 1.0F && end > 0) {
      const long seed_a = lr.next_long();
      const float size_a = lr.next_float() * 0.5F + 0.5F;
      const long seed_b = lr.next_long();
      const float size_b = lr.next_float() * 0.5F + 0.5F;
      cave_node(lr, seed_a, cx, cz, blocks, x, y, z, size_a,
                yaw - static_cast<float>(M_PI) * 0.5F, pitch / 3.0F, start, end, 1.0);
      cave_node(lr, seed_b, cx, cz, blocks, x, y, z, size_b,
                yaw + static_cast<float>(M_PI) * 0.5F, pitch / 3.0F, start, end, 1.0);
      return;
    }
    if (no_bifurcation || lr.next_int(4) != 0) {
      const double dx = x - cx8;
      const double dz = z - cz8;
      const double remaining = end - start;
      const double max_dist = size + 2.0F + 16.0F;
      if (dx * dx + dz * dz - remaining * remaining > max_dist * max_dist) return;
      if (x >= cx8 - 16.0 - width * 2.0 && z >= cz8 - 16.0 - width * 2.0 && x <= cx8 + 16.0 + width * 2.0 &&
          z <= cz8 + 16.0 + width * 2.0) {
        int x0 = MathHelper::floor_double(x - width) - cx * 16 - 1;
        int x1 = MathHelper::floor_double(x + width) - cx * 16 + 1;
        int y0 = MathHelper::floor_double(y - height) - 1;
        int y1 = MathHelper::floor_double(y + height) + 1;
        int z0 = MathHelper::floor_double(z - width) - cz * 16 - 1;
        int z1 = MathHelper::floor_double(z + width) - cz * 16 + 1;
        if (x0 < 0) x0 = 0;
        if (x1 > 16) x1 = 16;
        if (y0 < 1) y0 = 1;
        if (y1 > 120) y1 = 120;
        if (z0 < 0) z0 = 0;
        if (z1 > 16) z1 = 16;

        bool hit_water = false;
        for (int lx = x0; !hit_water && lx < x1; ++lx) {
          for (int lz = z0; !hit_water && lz < z1; ++lz) {
            for (int ly = y1 + 1; !hit_water && ly >= y0 - 1; --ly) {
              const std::size_t idx = raw_index(lx, 0, lz);  // y checked below
              (void)idx;
              if (ly >= 0 && ly < 128) {
                const int id = blocks[raw_index(lx, ly, lz)];
                if (id == bid::kWaterMoving || id == bid::kWaterStill) hit_water = true;
                if (ly != y0 - 1 && lx != x0 && lx != x1 - 1 && lz != z0 && lz != z1 - 1) ly = y0;
              }
            }
          }
        }
        if (!hit_water) {
          for (int lx = x0; lx < x1; ++lx) {
            const double nx = (lx + cx * 16 + 0.5 - x) / width;
            for (int lz = z0; lz < z1; ++lz) {
              const double nz = (lz + cz * 16 + 0.5 - z) / width;
              std::size_t idx = raw_index(lx, y1, lz);
              if (nx * nx + nz * nz < 1.0) {
                // NOTE: grass_above is sticky per column (mirrors var49, which
                // is declared outside the y loop and never reset): once grass
                // is seen above in the same column scan, dirt carved below
                // gets the biome top block even if the carved cell is dirt.
                bool grass_above = false;
                for (int ly = y1 - 1; ly >= y0; --ly) {
                  const double ny = (ly + 0.5 - y) / height;
                  if (ny > -0.7 && nx * nx + ny * ny + nz * nz < 1.0) {
                    const int id = blocks[idx];
                    if (id == bid::kGrass) grass_above = true;
                    if (id == bid::kStone || id == bid::kDirt || id == bid::kGrass) {
                      if (ly < 10) {
                        blocks[idx] = bid::kLavaMoving;
                      } else {
                        blocks[idx] = 0;
                        if (grass_above && blocks[idx - 1] == bid::kDirt) {
                          blocks[idx - 1] = static_cast<std::int8_t>(
                              biome_top_at(manager_, lx + cx * 16, lz + cz * 16));
                        }
                      }
                    }
                  }
                  --idx;
                }
              }
            }
          }
          if (no_bifurcation) break;
        }
      }
    }
  }
}

void MapGenRavine::generate(std::int64_t world_seed, int cx, int cz,
                            std::vector<std::int8_t>& blocks) {
  constexpr int kRange = 8;
  rand_.set_seed(world_seed);
  const std::int64_t s1 = rand_.next_long();
  const std::int64_t s2 = rand_.next_long();
  for (int nx = cx - kRange; nx <= cx + kRange; ++nx) {
    for (int nz = cz - kRange; nz <= cz + kRange; ++nz) {
      const std::uint64_t mixed = static_cast<std::uint64_t>(static_cast<std::int64_t>(nx) * s1) ^
                                  static_cast<std::uint64_t>(static_cast<std::int64_t>(nz) * s2) ^
                                  static_cast<std::uint64_t>(world_seed);
      rand_.set_seed(static_cast<std::int64_t>(mixed));
      recursive_generate(rand_, nx, nz, cx, cz, blocks);
    }
  }
}

void MapGenRavine::debug_ravine_node(long seed, int cx, int cz,
                                     std::vector<std::int8_t>& blocks, double x, double y,
                                     double z, float size, float yaw, float pitch) {
  JavaRandom unused(0);
  float widths[1024];
  ravine_node(unused, seed, cx, cz, blocks, x, y, z, size, yaw, pitch, 0, 0, 3.0, widths);
}

void MapGenRavine::recursive_generate(JavaRandom& rand, int nx, int nz, int cx, int cz,
                                      std::vector<std::int8_t>& blocks) {
  if (rand.next_int(50) != 0) return;
  const double x = nx * 16 + rand.next_int(16);
  const double y = rand.next_int(rand.next_int(40) + 8) + 20;
  const double z = nz * 16 + rand.next_int(16);
  const float yaw = rand.next_float() * static_cast<float>(M_PI) * 2.0F;
  const float pitch = (rand.next_float() - 0.5F) * 2.0F / 8.0F;
  const float rsize_a = rand.next_float();
  const float rsize_b = rand.next_float();
  const float size = (rsize_a * 2.0F + rsize_b) * 2.0F;
  float widths[1024];
  ravine_node(rand, rand.next_long(), cx, cz, blocks, x, y, z, size, yaw, pitch, 0, 0, 3.0,
              widths);
}

void MapGenRavine::ravine_node(JavaRandom& local, long seed, int cx, int cz,
                               std::vector<std::int8_t>& blocks, double x, double y, double z,
                               float size, float yaw, float pitch, int start, int end,
                               double height_scale, float* width_profile) {
  (void)local;
  const double cx8 = cx * 16 + 8;
  const double cz8 = cz * 16 + 8;
  float yaw_vel = 0.0F;
  float pitch_vel = 0.0F;
  JavaRandom lr(seed);
  if (end <= 0) {
    const int range = 8 * 16 - 16;
    end = range - lr.next_int(range / 4);
  }
  bool no_bifurcation = false;
  if (start == -1) {
    start = end / 2;
    no_bifurcation = true;
  }
  // Width profile keeps the previous value unless redrawn (mirrors var27).
  float running_width = 1.0F;
  for (int yy = 0; yy < 128; ++yy) {
    if (yy == 0 || lr.next_int(3) == 0) {
      const float w_a = lr.next_float();
      const float w_b = lr.next_float();
      running_width = 1.0F + w_a * w_b;
    }
    width_profile[yy] = running_width * running_width;
  }
  for (; start < end; ++start) {
    // NOTE: height derives from the UNJITTERED width (the source computes
    // var30 = var54 * var17 before jittering var54).
    const double width_base =
        1.5 + MathHelper::sin(static_cast<float>(start) * static_cast<float>(M_PI) / end) * size;
    const float wjitter_a = lr.next_float();
    double width = width_base;
    width *= wjitter_a * 0.25 + 0.75;
    const double height_base = width_base * height_scale;
    const float hjitter_a = lr.next_float();
    const double height = height_base * (hjitter_a * 0.25 + 0.75);
    const float cos_pitch = MathHelper::cos(pitch);
    const float sin_pitch = MathHelper::sin(pitch);
    x += MathHelper::cos(yaw) * cos_pitch;
    y += sin_pitch;
    z += MathHelper::sin(yaw) * cos_pitch;
    pitch *= 0.7F;
    pitch += pitch_vel * 0.05F;
    yaw += yaw_vel * 0.05F;
    pitch_vel *= 0.8F;
    yaw_vel *= 0.5F;
    {
      const float pv_a = lr.next_float();
      const float pv_b = lr.next_float();
      const float pv_c = lr.next_float();
      const float yv_a = lr.next_float();
      const float yv_b = lr.next_float();
      const float yv_c = lr.next_float();
      pitch_vel += (pv_a - pv_b) * pv_c * 2.0F;
      yaw_vel += (yv_a - yv_b) * yv_c * 4.0F;
    }
    if (no_bifurcation || lr.next_int(4) != 0) {
      const double dx = x - cx8;
      const double dz = z - cz8;
      const double remaining = end - start;
      const double max_dist = size + 2.0F + 16.0F;
      if (dx * dx + dz * dz - remaining * remaining > max_dist * max_dist) return;
      if (x >= cx8 - 16.0 - width * 2.0 && z >= cz8 - 16.0 - width * 2.0 && x <= cx8 + 16.0 + width * 2.0 &&
          z <= cz8 + 16.0 + width * 2.0) {
        // Same carve body as caves, plus the width-profile term.
        int x0 = MathHelper::floor_double(x - width) - cx * 16 - 1;
        int x1 = MathHelper::floor_double(x + width) - cx * 16 + 1;
        int y0 = MathHelper::floor_double(y - height) - 1;
        int y1 = MathHelper::floor_double(y + height) + 1;
        if (x0 < 0) x0 = 0;
        if (x1 > 16) x1 = 16;
        if (y0 < 1) y0 = 1;
        if (y1 > 120) y1 = 120;
        if (true) {
          int z0 = MathHelper::floor_double(z - width) - cz * 16 - 1;
          int z1 = MathHelper::floor_double(z + width) - cz * 16 + 1;
          if (z0 < 0) z0 = 0;
          if (z1 > 16) z1 = 16;
          bool hit_water = false;
          for (int lx = x0; !hit_water && lx < x1; ++lx) {
            for (int lz = z0; !hit_water && lz < z1; ++lz) {
              for (int ly = y1 + 1; !hit_water && ly >= y0 - 1; --ly) {
                if (ly >= 0 && ly < 128) {
                  const int id = blocks[raw_index(lx, ly, lz)];
                  if (id == bid::kWaterMoving || id == bid::kWaterStill) hit_water = true;
                  if (ly != y0 - 1 && lx != x0 && lx != x1 - 1 && lz != z0 && lz != z1 - 1) ly = y0;
                }
              }
            }
          }
          if (!hit_water) {
            for (int lx = x0; lx < x1; ++lx) {
              const double nx = (lx + cx * 16 + 0.5 - x) / width;
              for (int lz = z0; lz < z1; ++lz) {
                const double nz = (lz + cz * 16 + 0.5 - z) / width;
                std::size_t idx = raw_index(lx, y1, lz);
                if (nx * nx + nz * nz < 1.0) {
                  // Sticky per column like the source's var48 (see cave fix above).
                  bool grass_above = false;
                  for (int ly = y1 - 1; ly >= y0; --ly) {
                    const double ny = (ly + 0.5 - y) / height;
                    if ((nx * nx + nz * nz) * width_profile[ly] + ny * ny / 6.0 < 1.0) {
                      const int id = blocks[idx];
                      if (id == bid::kGrass) grass_above = true;
                      if (id == bid::kStone || id == bid::kDirt || id == bid::kGrass) {
                        if (ly < 10) {
                          blocks[idx] = bid::kLavaMoving;
                        } else {
                          blocks[idx] = 0;

                          if (grass_above && blocks[idx - 1] == bid::kDirt) {
                            blocks[idx - 1] = static_cast<std::int8_t>(
                                biome_top_at(manager_, lx + cx * 16, lz + cz * 16));
                          }
                        }
                      }
                    }
                    --idx;
                  }
                }
              }
            }
            if (no_bifurcation) break;
          }
        }
      }
    }
  }
}

}  // namespace craftpp::world
