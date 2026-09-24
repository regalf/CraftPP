#pragma once

#include <array>
#include <cstdint>

#include "core/random.hpp"

namespace craftpp::world {

// Biome ids mirror BiomeGenBase static fields.
enum class BiomeId : std::int32_t {
  Ocean = 0,
  Plains = 1,
  Desert = 2,
  Hills = 3,
  Forest = 4,
  Taiga = 5,
  Swampland = 6,
  River = 7,
  Hell = 8,
  Sky = 9,
  FrozenOcean = 10,
  FrozenRiver = 11,
  IcePlains = 12,
  IceMountains = 13,
  MushroomIsland = 14,
  MushroomIslandShore = 15,
};

struct BiomeDef {
  BiomeId id;
  float min_height;  // setMinMaxHeight (defaults 0.1/0.3)
  float max_height;
  float temperature;  // setTemperatureRainfall (defaults 0.5/0.5)
  float rainfall;
  std::int8_t top_block;     // block id used by replaceBlocksForBiome
  std::int8_t filler_block;
};

// Static table mirroring the BiomeGenBase.<clinit> chain.
const BiomeDef& biome_def(BiomeId id);
const BiomeDef& biome_def_by_index(std::int32_t index);

// (int)(temperature * 65536), mirrors func_35474_f.
inline std::int32_t biome_temp_int(const BiomeDef& b) {
  return static_cast<std::int32_t>(b.temperature * 65536.0F);
}
// (int)(rainfall * 65536), mirrors func_35476_e.
inline std::int32_t biome_rain_int(const BiomeDef& b) {
  return static_cast<std::int32_t>(b.rainfall * 65536.0F);
}

// Per-biome decoration parameters mirroring the BiomeGen* constructors.
// Negative counts mean "none, loop skipped" (the treesPerChunk +1 gate draw
// still happens — it is unconditional in decorate_do).
struct DecorParams {
  int waterlily = 0;
  int trees = 0;
  int flowers = 2;
  int grass = 1;
  int dead_bush = 0;
  int mushrooms = 0;
  int reeds = 0;
  int cacti = 0;
  int sand = 1;
  int sand2 = 3;
  int clay = 1;
  int big_mushroom = 0;
};

const DecorParams& decor_params(BiomeId id);

enum class TreeKind { Normal, Big, Forest, Swamp, Taiga1, Taiga2 };

// Mirrors BiomeGenBase.getRandomWorldGenForTrees (+ overrides), including
// the exact draw sequences per biome.
TreeKind pick_tree(BiomeId id, JavaRandom& rand);

}  // namespace craftpp::world
