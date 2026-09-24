#pragma once

#include "core/random.hpp"
#include "world/blocks.hpp"
#include "world/region.hpp"

namespace craftpp::world {

// Immediate-mode 1.0 fluid simulation for worldgen (BlockFlowing/BlockFluid/
// BlockStationary as exercised by WorldGenLiquids springs).
//
// The source runs spring updateTicks with World.scheduledUpdatesAreImmediate,
// so the whole cascade executes synchronously depth-first. Scheduled updates
// draw World.rand (the world's own stream); the spring's own direct
// updateTick call draws the decorator RNG. Both streams must be replicated
// exactly, hence the two RNG references.
//
// Only block effects are replicated (no entities, particles, sounds, light
// engine). Triggered lava-mix effects still consume their World.rand draws.
class FluidSim {
 public:
  FluidSim(RegionWorld& world, JavaRandom& world_rand) : w_(world), wrand_(world_rand) {}

  // Mirrors the WorldGenLiquids tail: direct updateTick with the decorator
  // RNG (which also drives the lava line-59 draw on the top call).
  void spring_tick(int x, int y, int z, JavaRandom& deco_rand);
  // Spring source placement (setBlockWithNotify outside the cascade).
  void place_for_spring(int x, int y, int z, int id) { set_notify(x, y, z, id); }
  // setBlockWithNotify / setBlockAndMetadataWithNotify for non-fluid worldgen
  // (dungeons, vines, ice/snow cap): early-out + write + onBlockAdded effects
  // + neighbor notification. Neighbor reactions replicate onNeighborBlockChange
  // for fluids (harden/convert) and sand (fall); all other blocks are
  // block-neutral in these scenarios (verified against the oracle).
  bool place_notify(int x, int y, int z, int id) { return set_notify(x, y, z, id); }
  bool place_meta_notify(int x, int y, int z, int id, int meta) {
    return set_meta_notify(x, y, z, id, meta);
  }

 private:
  static bool is_water(int id) { return id == bid::kWaterMoving || id == bid::kWaterStill; }
  static bool is_lava(int id) { return id == bid::kLavaMoving || id == bid::kLavaStill; }
  static bool is_fluid(int id) { return is_water(id) || is_lava(id); }
  static bool is_moving(int id) { return id == bid::kWaterMoving || id == bid::kLavaMoving; }

  // Immediate-mode scheduleBlockUpdate: runs update_tick now when the block
  // still has the expected id (chunks are always present in worldgen tests).
  void schedule(int x, int y, int z, int id, JavaRandom& r);
  void update_tick(int x, int y, int z, JavaRandom& r);
  void update_flowing(int x, int y, int z, JavaRandom& r);
  void update_stationary_lava(int x, int y, int z, JavaRandom& r);

  // World-write primitives with 1.0 notify semantics (editingBlocks honored).
  bool set_notify(int x, int y, int z, int id);             // setBlockWithNotify
  bool set_meta_notify(int x, int y, int z, int id, int m);  // setBlockAndMetadataWithNotify
  void set_meta_only(int x, int y, int z, int m);            // setBlockMetadataWithNotify
  void set_silent(int x, int y, int z, int id, int m);       // setBlockAndMetadata
  void notify_neighbors(int x, int y, int z, int id);
  void on_neighbor(int x, int y, int z, int nid);
  void check_harden(int x, int y, int z);
  void mix_effects(int x, int y, int z);  // world.rand draws only
  void fluid_added(int x, int y, int z);  // onBlockAdded for fluids (+sand)
  void schedule_sand(int x, int y, int z);
  void sand_try_fall(int x, int y, int z);
  bool can_fall_below(int x, int y, int z) const;
  static bool burnable(RegionWorld& w, int x, int y, int z);

  // Queries mirroring BlockFluid/BlockFlowing helpers.
  int flow_decay(int x, int y, int z, bool water) const;
  int smallest_flow_decay(int x, int y, int z, bool water, int cur);
  bool blocks_flow(int x, int y, int z) const;
  bool displaceable(int x, int y, int z, bool water) const;
  void flow_into(int x, int y, int z, int fluid_id, int meta);
  int flow_cost(int x, int y, int z, bool water, int depth, int from_dir) const;
  void optimal_dirs(int x, int y, int z, bool water);

  RegionWorld& w_;
  JavaRandom& wrand_;
  bool editing_ = false;
  bool immediate_ = false;  // mirrors World.scheduledUpdatesAreImmediate
  // Shared mutable flow state, mirroring BlockFlowing's instance fields
  // (numAdjacentSources, flowCost, isOptimalFlowDirection). These MUST be
  // members, not locals: nested updateTicks overwrite them, and the outer
  // tick then reads the CLOBBERED values (load-bearing 1.0 quirk: a tick
  // that flows in 2+ directions uses corrupted dirs for the 2nd+ flow).
  int num_sources_ = 0;
  int flow_cost_[4] = {1000, 1000, 1000, 1000};
  bool opt_dirs_[4] = {false, false, false, false};
};

}  // namespace craftpp::world
