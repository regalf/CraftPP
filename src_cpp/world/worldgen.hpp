#pragma once

#include <cstdint>

#include "core/random.hpp"
#include "world/biome.hpp"
#include "world/blocks.hpp"
#include "world/chunk_manager.hpp"
#include "world/fluid.hpp"
#include "world/region.hpp"

#include <vector>

namespace craftpp::world {

// Port of the WorldGenerator family + BiomeDecorator + the populate pipeline
// (ChunkProviderGenerate.populate minus structures, animals, snow/ice and
// fluid springs, which belong to M4/M5).
//
// All generators work against RegionWorld and draw from a caller-owned
// JavaRandom in exact source order (one draw per statement — see plan.md).
class FeatureGen {
 public:
  FeatureGen(RegionWorld& world, const ChunkManager& chunks, JavaRandom& world_rand)
      : w_(world), chunks_(chunks), fluid_(world, world_rand) {}

  // Individual features (mirrors WorldGen*.generate).
  bool minable(int x, int y, int z, int id, int size, JavaRandom& rand);
  bool flowers(int x, int y, int z, int id, JavaRandom& rand);
  bool tall_grass(int x, int y, int z, JavaRandom& rand);
  bool dead_bush(int x, int y, int z, JavaRandom& rand);
  bool reed(int x, int y, int z, JavaRandom& rand);
  bool cactus(int x, int y, int z, JavaRandom& rand);
  bool clay(int x, int y, int z, JavaRandom& rand);
  bool sand_patch(int x, int y, int z, int radius, int id, JavaRandom& rand);
  bool pumpkin(int x, int y, int z, JavaRandom& rand);
  bool waterlily(int x, int y, int z, JavaRandom& rand);
  bool lake(int x, int y, int z, int id, JavaRandom& rand);
  // Fluid spring placement only (no updateTick spread: M5 fluid sim).
  bool liquid_spring(int x, int y, int z, int id, JavaRandom& rand);
  bool dungeon(int x, int y, int z, JavaRandom& rand);
  bool tree_normal(int x, int y, int z, JavaRandom& rand);
  bool tree_forest(int x, int y, int z, JavaRandom& rand);
  bool tree_swamp(int x, int y, int z, JavaRandom& rand);
  bool tree_taiga1(int x, int y, int z, JavaRandom& rand);
  bool tree_taiga2(int x, int y, int z, JavaRandom& rand);
  bool big_mushroom(int x, int y, int z, JavaRandom& rand);

  // Persistent big-tree instance (heightLimit persists across generates like
  // the shared BiomeGenBase.worldGenBigTree; owns its own Random too).
  bool tree_big(int x, int y, int z, JavaRandom& rand);

  // Mirrors BiomeDecorator.decorate_do (minus the fluid section).
  void decorate(BiomeId biome, int chunk_x, int chunk_z, JavaRandom& rand);

  // setBlockWithNotify for the populate ice/snow cap.
  void place_ice_snow(int x, int y, int z, int id) { fluid_.place_notify(x, y, z, id); }

 private:
  void vine_down(int x, int y, int z, int meta);

  // Owned big-tree state (mirrors the shared BiomeGenBase.worldGenBigTree:
  // heightLimit persists across generates GLOBALLY (per biome singleton),
  // owns its own Random).
  struct BigTree {
    JavaRandom rand;
    int base[3] = {0, 0, 0};
    int height = 0;
    int limit_limit = 12;
    int leaf_dist = 4;
    struct Node {
      int x;
      int y;
      int z;
      int base_y;
    };
    std::vector<Node> leaf_nodes;
  };
  BigTree big_tree_;
  // Globally persistent heightLimit (BiomeGenBase.worldGenBigTree singleton).
  // Persists across chunks AND sites within a run, exactly like the source.
  static int big_height_limit;

 private:
  // Placement predicates mirroring Block.canBlockStay/canPlaceBlockAt.
  bool flower_can_stay(int x, int y, int z) const;
  bool flower_lit(int x, int y, int z) const {
    // getFullBlockLightValue uses the SAVED (install-time frozen) skylight;
    // block light is 0 in a fresh world. canBlockSeeTheSky is heightMap-based.
    return w_.saved_sky(x, y, z) >= 8 || w_.can_see_sky(x, y, z);
  }
  bool flower_soil(int id) const { return id == bid::kGrass || id == bid::kDirt || id == bid::kFarmland; }
  bool reed_can_place(int x, int y, int z) const;
  bool cactus_can_place(int x, int y, int z) const;
  bool cactus_can_stay(int x, int y, int z) const;
  bool pumpkin_can_place(int x, int y, int z) const;
  bool lily_can_place(int x, int y, int z) const;
  bool mushroom_can_place(int x, int y, int z) const;

  RegionWorld& w_;
  const ChunkManager& chunks_;
  FluidSim fluid_;

  // Owned big-tree state (mirrors the shared instance).
  struct BigTree;
};

// Mirrors ChunkProviderGenerate.populate minus structures (mapFeatures=false),
// animals and ice/snow. Seeds rand exactly like the source. world_rand is the
// shared World.rand stream (nested fluid updates draw it; tests pin it to 0
// like the fixed-seed harness).
void populate_chunk(RegionWorld& world, const ChunkManager& chunks, std::int64_t world_seed, int cx,
                    int cz, JavaRandom& world_rand);

}  // namespace craftpp::world
