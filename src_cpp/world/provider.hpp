#pragma once

#include <cstdint>
#include <vector>

#include "core/random.hpp"
#include "world/biome.hpp"
#include "world/chunk_manager.hpp"
#include "world/noise.hpp"

namespace craftpp::world {

// Port of ChunkProviderGenerate.generateTerrain + replaceBlocksForBiome.
//
// Output is a raw 16*128*16 byte array in Java layout: index (x*16+z)*128+y.
// Block ids are raw Minecraft ids (stone 1, grass 2, dirt 3, sand 12, water 9,
// bedrock 7, ice 79, ...). Caves/ravines/structures/skylight are M3c/M5.
class TerrainProvider {
 public:
  static constexpr int kHeight = 128;
  static constexpr int kSeaLevel = 63;  // field_35470_e = 128/2-1

  TerrainProvider(const ChunkManager& manager, std::int64_t world_seed);

  // Mirrors provideChunk seeding (must precede the two steps below).
  void set_chunk_seed(int cx, int cz);

  // Stone/water/air carving from the interpolated noise field.
  void generate_terrain(int cx, int cz, std::vector<std::int8_t>& blocks) const;
  // Biome surface replacement (dirt/grass/sand, bedrock floor, ice/water).
  void replace_biome_blocks(int cx, int cz, std::vector<std::int8_t>& blocks,
                            const std::vector<BiomeId>& biomes);

 private:
  std::vector<double> init_noise_field(int nx, int nz, int xs, int ys, int zs) const;

  const ChunkManager& manager_;
  JavaRandom rand_;
  Octaves noise1_;
  Octaves noise2_;
  Octaves noise3_;
  Octaves noise4_;
  Octaves noise5_;
  Octaves noise6_;
  mutable std::vector<double> field_n_;
  mutable std::vector<double> n1_, n2_, n3_, n5_, n6_;
  mutable std::vector<double> stone_noise_;
  mutable std::vector<float> parabolic_;
};

}  // namespace craftpp::world
