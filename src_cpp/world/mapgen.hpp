#pragma once

#include <cstdint>
#include <vector>

#include "core/random.hpp"
#include "world/chunk_manager.hpp"

namespace craftpp::world {

// Port of MapGenBase/MapGenCaves/MapGenRavine: carve caves and ravines into
// a single chunk's raw byte array (Java layout, like generate()).
//
// Needs the chunk manager only for the grass-top fixup biome lookup
// (getBiomeGenAt == voronoi layer), mirroring the source.
class MapGenCaves {
 public:
  explicit MapGenCaves(const ChunkManager& manager) : manager_(manager) {}

  void generate(std::int64_t world_seed, int cx, int cz, std::vector<std::int8_t>& blocks);

  // Test hook: carve a single node into the array (mirrors the protected
  // generateCaveNode, exposed for differential testing).
  void debug_cave_node(long seed, int cx, int cz, std::vector<std::int8_t>& blocks, double x,
                       double y, double z, float size, float yaw, float pitch, int start = 0,
                       int end = 0);
  // Test hook for the large-node entry point (draws its size from the shared
  // stream like the source, so seed it first via debug_seed).
  void debug_large_node(long seed, int cx, int cz, std::vector<std::int8_t>& blocks, double x,
                        double y, double z);
  void debug_seed(long seed);

 private:
  void recursive_generate(JavaRandom& rand, int nx, int nz, int cx, int cz,
                          std::vector<std::int8_t>& blocks);
  void cave_node(JavaRandom& local, long seed, int cx, int cz, std::vector<std::int8_t>& blocks,
                 double x, double y, double z, float size, float yaw, float pitch, int start,
                 int end, double height_scale);
  void large_node(long seed, int cx, int cz, std::vector<std::int8_t>& blocks, double x, double y,
                  double z);

  const ChunkManager& manager_;
  JavaRandom rand_;
};

class MapGenRavine {
 public:
  explicit MapGenRavine(const ChunkManager& manager) : manager_(manager) {}

  void generate(std::int64_t world_seed, int cx, int cz, std::vector<std::int8_t>& blocks);

  // Test hook mirroring the protected ravine entry (func_35626_a).
  void debug_ravine_node(long seed, int cx, int cz, std::vector<std::int8_t>& blocks, double x,
                         double y, double z, float size, float yaw, float pitch);

 private:
  void recursive_generate(JavaRandom& rand, int nx, int nz, int cx, int cz,
                          std::vector<std::int8_t>& blocks);
  void ravine_node(JavaRandom& local, long seed, int cx, int cz, std::vector<std::int8_t>& blocks,
                   double x, double y, double z, float size, float yaw, float pitch, int start,
                   int end, double height_scale, float* width_profile);

  const ChunkManager& manager_;
  JavaRandom rand_;
};

}  // namespace craftpp::world
