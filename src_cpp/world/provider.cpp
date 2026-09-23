#include "world/provider.hpp"

#include "core/math_helper.hpp"

namespace craftpp::world {

namespace {

// Raw block ids (mirror Block.java).
constexpr std::int8_t kAir = 0;
constexpr std::int8_t kStone = 1;
constexpr std::int8_t kGrass = 2;
constexpr std::int8_t kDirt = 3;
constexpr std::int8_t kBedrock = 7;
constexpr std::int8_t kWater = 9;
constexpr std::int8_t kSand = 12;
constexpr std::int8_t kSandstone = 24;
constexpr std::int8_t kIce = 79;

inline std::size_t raw_index(int x, int y, int z) {
  return static_cast<std::size_t>((x * 16 + z) * TerrainProvider::kHeight + y);
}

}  // namespace

TerrainProvider::TerrainProvider(const ChunkManager& manager, std::int64_t world_seed)
    : manager_(manager),
      rand_(world_seed),
      noise1_(rand_, 16),
      noise2_(rand_, 16),
      noise3_(rand_, 8),
      noise4_(rand_, 4),
      noise5_(rand_, 10),
      noise6_(rand_, 16) {}

void TerrainProvider::set_chunk_seed(int cx, int cz) {
  rand_.set_seed(static_cast<std::int64_t>(cx) * 341873128712LL +
                 static_cast<std::int64_t>(cz) * 132897987541LL);
}

std::vector<double> TerrainProvider::init_noise_field(int nx, int nz, int xs, int ys,
                                                      int zs) const {
  if (parabolic_.empty()) {
    parabolic_.resize(25);
    for (int dx = -2; dx <= 2; ++dx) {
      for (int dz = -2; dz <= 2; ++dz) {
        const float w = 10.0F / MathHelper::sqrt_float(static_cast<float>(dx * dx + dz * dz) + 0.2F);
        parabolic_[static_cast<std::size_t>(dx + 2 + (dz + 2) * 5)] = w;
      }
    }
  }

  // NOTE: func_4109_a takes a trailing 8th arg that the source never uses.
  n5_ = noise5_.generate_2d(std::move(n5_), nx, nz, xs, zs, 1.121, 1.121);
  n6_ = noise6_.generate_2d(std::move(n6_), nx, nz, xs, zs, 200.0, 200.0);
  n3_ = noise3_.generate(std::move(n3_), nx, 0, nz, xs, ys, zs, 684.412 / 80.0, 684.412 / 160.0,
                         684.412 / 80.0);
  n1_ = noise1_.generate(std::move(n1_), nx, 0, nz, xs, ys, zs, 684.412, 684.412, 684.412);
  n2_ = noise2_.generate(std::move(n2_), nx, 0, nz, xs, ys, zs, 684.412, 684.412, 684.412);

  const std::vector<BiomeId> biomes = manager_.coarse_biomes(nx - 2, nz - 2, xs + 5, zs + 5);
  std::vector<double> field(static_cast<std::size_t>(xs) * ys * zs);
  std::size_t n6_idx = 0;
  std::size_t f_idx = 0;

  for (int ix = 0; ix < xs; ++ix) {
    for (int iz = 0; iz < zs; ++iz) {
      float sum_h = 0.0F;
      float sum_b = 0.0F;
      float sum_w = 0.0F;
      const BiomeDef& center = biome_def(biomes[static_cast<std::size_t>(ix + 2 + (iz + 2) * (xs + 5))]);
      for (int dx = -2; dx <= 2; ++dx) {
        for (int dz = -2; dz <= 2; ++dz) {
          const BiomeDef& b = biome_def(biomes[static_cast<std::size_t>(ix + dx + 2 + (iz + dz + 2) * (xs + 5))]);
          float wgt = parabolic_[static_cast<std::size_t>(dx + 2 + (dz + 2) * 5)] / (b.min_height + 2.0F);
          if (b.min_height > center.min_height) wgt /= 2.0F;
          sum_h += b.max_height * wgt;
          sum_b += b.min_height * wgt;
          sum_w += wgt;
        }
      }
      sum_h /= sum_w;
      sum_b /= sum_w;
      sum_h = sum_h * 0.9F + 0.1F;
      sum_b = (sum_b * 4.0F - 1.0F) / 8.0F;

      double depth = n6_[n6_idx++] / 8000.0;
      if (depth < 0.0) depth = -depth * 0.3;
      depth = depth * 3.0 - 2.0;
      if (depth < 0.0) {
        depth /= 2.0;
        if (depth < -1.0) depth = -1.0;
        depth /= 1.4;
        depth /= 2.0;
      } else {
        if (depth > 1.0) depth = 1.0;
        depth /= 8.0;
      }

      for (int iy = 0; iy < ys; ++iy) {
        double base = sum_b;
        double var = sum_h;
        base += depth * 0.2;
        base = base * ys / 16.0;
        const double surface = ys / 2.0 + base * 4.0;
        double falloff = 0.0;
        double dist = (iy - surface) * 12.0 * 128.0 / kHeight / var;
        if (dist < 0.0) dist *= 4.0;
        const double nA = n1_[f_idx] / 512.0;
        const double nB = n2_[f_idx] / 512.0;
        const double mix = (n3_[f_idx] / 10.0 + 1.0) / 2.0;
        double density = 0.0;
        if (mix < 0.0) {
          density = nA;
        } else if (mix > 1.0) {
          density = nB;
        } else {
          density = nA + (nB - nA) * mix;
        }
        density -= dist;
        if (iy > ys - 4) {
          const double t = (iy - (ys - 4)) / 3.0;
          density = density * (1.0 - t) + -10.0 * t;
        }
        field[f_idx++] = density + falloff;
      }
    }
  }
  return field;
}

void TerrainProvider::generate_terrain(int cx, int cz, std::vector<std::int8_t>& blocks) const {
  constexpr int kCell = 4;
  constexpr int kYCells = kHeight / 8;
  blocks.assign(16 * kHeight * 16, kAir);
  field_n_ = init_noise_field(cx * kCell, cz * kCell, kCell + 1, kYCells + 1, kCell + 1);

  constexpr int kXS = kCell + 1;
  constexpr int kYS = kYCells + 1;
  for (int gx = 0; gx < kCell; ++gx) {
    for (int gz = 0; gz < kCell; ++gz) {
      for (int gy = 0; gy < kYCells; ++gy) {
        double n000 = field_n_[static_cast<std::size_t>(((gx) * kXS + gz) * kYS + gy)];
        double n001 = field_n_[static_cast<std::size_t>(((gx) * kXS + gz + 1) * kYS + gy)];
        double n100 = field_n_[static_cast<std::size_t>(((gx + 1) * kXS + gz) * kYS + gy)];
        double n101 = field_n_[static_cast<std::size_t>(((gx + 1) * kXS + gz + 1) * kYS + gy)];
        const double dy000 = (field_n_[static_cast<std::size_t>(((gx) * kXS + gz) * kYS + gy + 1)] - n000) * 0.125;
        const double dy001 = (field_n_[static_cast<std::size_t>(((gx) * kXS + gz + 1) * kYS + gy + 1)] - n001) * 0.125;
        const double dy100 = (field_n_[static_cast<std::size_t>(((gx + 1) * kXS + gz) * kYS + gy + 1)] - n100) * 0.125;
        const double dy101 = (field_n_[static_cast<std::size_t>(((gx + 1) * kXS + gz + 1) * kYS + gy + 1)] - n101) * 0.125;

        for (int sy = 0; sy < 8; ++sy) {
          double c00 = n000;
          double c01 = n001;
          const double dx0 = (n100 - n000) * 0.25;
          const double dx1 = (n101 - n001) * 0.25;
          for (int sx = 0; sx < 4; ++sx) {
            // Mirrors: var43 = x<<11 | z<<7 | y, then per-y += 128.
            int idx = ((sx + gx * 4) << 11) | ((gz * 4) << 7) | (gy * 8 + sy);
            idx -= 128;
            double v = c00;
            const double dz = (c01 - c00) * 0.25;
            v -= dz;
            for (int sz = 0; sz < 4; ++sz) {
              v += dz;
              idx += 128;
              if (v > 0.0) {
                blocks[static_cast<std::size_t>(idx)] = kStone;
              } else if (gy * 8 + sy < kSeaLevel) {
                blocks[static_cast<std::size_t>(idx)] = kWater;
              } else {
                blocks[static_cast<std::size_t>(idx)] = kAir;
              }
            }
            c00 += dx0;
            c01 += dx1;
          }
          // Advance the corners down one y-slice (mirrors var15 += var23 etc.).
          n000 += dy000;
          n001 += dy001;
          n100 += dy100;
          n101 += dy101;
        }
      }
    }
  }
}

void TerrainProvider::replace_biome_blocks(int cx, int cz, std::vector<std::int8_t>& blocks,
                                           const std::vector<BiomeId>& biomes) {
  // NOTE the arg order: (x=cx*16, y=cz*16, z=0) — the second chunk coord feeds
  // the noise Y axis, not Z (mirrors the source call verbatim).
  stone_noise_ = noise4_.generate(std::move(stone_noise_), cx * 16, cz * 16, 0, 16, 16, 1,
                                  1.0 / 32.0 * 2.0, 1.0 / 32.0 * 2.0, 1.0 / 32.0 * 2.0);
  const std::vector<float> temps = manager_.temperatures(cx * 16, cz * 16, 16, 16);

  for (int ix = 0; ix < 16; ++ix) {
    for (int iz = 0; iz < 16; ++iz) {
      const float temp = temps[static_cast<std::size_t>(iz + ix * 16)];
      const BiomeDef& biome = biome_def(biomes[static_cast<std::size_t>(iz + ix * 16)]);
      // NOTE: transposed stoneNoise index mirrors the source quirk.
      int depth = static_cast<int>(stone_noise_[static_cast<std::size_t>(ix + iz * 16)] / 3.0 + 3.0 +
                                   rand_.next_double() * 0.25);
      int filler_left = -1;
      std::int8_t top = biome.top_block;
      std::int8_t filler = biome.filler_block;

      for (int y = kHeight - 1; y >= 0; --y) {
        const std::size_t idx = raw_index(iz, y, ix);
        if (y <= rand_.next_int(5)) {
          blocks[idx] = kBedrock;
        } else {
          const std::int8_t cur = blocks[idx];
          if (cur == kAir) {
            filler_left = -1;
          } else if (cur == kStone) {
            if (filler_left == -1) {
              if (depth <= 0) {
                top = kAir;
                filler = kStone;
              } else if (y >= kSeaLevel - 4 && y <= kSeaLevel + 1) {
                top = biome.top_block;
                filler = biome.filler_block;
              }
              if (y < kSeaLevel && top == kAir) {
                top = (temp < 0.15F) ? kIce : kWater;
              }
              filler_left = depth;
              blocks[idx] = (y >= kSeaLevel - 1) ? top : filler;
            } else if (filler_left > 0) {
              --filler_left;
              blocks[idx] = filler;
              if (filler_left == 0 && filler == kSand) {
                filler_left = rand_.next_int(4);
                filler = kSandstone;
              }
            }
          }
        }
      }
    }
  }
}

}  // namespace craftpp::world
