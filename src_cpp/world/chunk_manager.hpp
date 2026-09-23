#pragma once

#include <cstdint>
#include <vector>

#include "world/biome.hpp"
#include "world/genlayer.hpp"

namespace craftpp::world {

// Thin WorldChunkManager equivalent for terrain generation: owns the layer
// stack and answers biome/temperature queries in block coordinates.
// (BiomeCache memoization is a pure optimization and lands with M5.)
class ChunkManager {
 public:
  explicit ChunkManager(std::int64_t world_seed) : layers_(make_layers(world_seed)) {}

  // Biome ids for a w*h block region (voronoi layer, like loadBlockGeneratorData).
  std::vector<BiomeId> block_biomes(int x, int z, int w, int h) const;
  // Biome ids on the coarse 1:4 grid (like func_35557_b for noise shaping).
  std::vector<BiomeId> coarse_biomes(int x, int z, int w, int h) const;
  // Temperatures in 0..1 (like getTemperatures: min(raw, 65536) / 65536).
  std::vector<float> temperatures(int x, int z, int w, int h) const;

 private:
  LayerSet layers_;
};

}  // namespace craftpp::world
