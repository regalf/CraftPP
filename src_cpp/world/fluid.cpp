#include "world/fluid.hpp"


#include "world/blocks.hpp"

namespace craftpp::world {

// 0=air 1=water 2=lava 3=other. Mirrors World.getBlockMaterial.
static int mat_of(int id) {
  if (id == 0) return 0;
  if (id == bid::kWaterMoving || id == bid::kWaterStill) return 1;
  if (id == bid::kLavaMoving || id == bid::kLavaStill) return 2;
  return 3;
}

static bool mat_solid(int id) { return bid::material_solid(id); }

void FluidSim::spring_tick(int x, int y, int z, JavaRandom& deco_rand) {
  // WorldGenLiquids placed the source via setBlockWithNotify while immediate
  // was false (queued, never runs); now run the direct updateTick.
  immediate_ = true;
  update_tick(x, y, z, deco_rand);
  immediate_ = false;
}

void FluidSim::schedule(int x, int y, int z, int id, JavaRandom& r) {
  if (!immediate_) return;  // queued: never executes during worldgen
  // checkChunksExist(x-8..x+8): skip when outside the built region.
  for (int cx = (x - 8) >> 4; cx <= (x + 8) >> 4; ++cx)
    for (int cz = (z - 8) >> 4; cz <= (z + 8) >> 4; ++cz)
      if (!w_.has_chunk(cx, cz)) return;
  if (w_.get_id(x, y, z) != id || id <= 0) return;
  update_tick(x, y, z, r);
}

void FluidSim::update_tick(int x, int y, int z, JavaRandom& r) {
  const int id = w_.get_id(x, y, z);
  // Dispatches on the CURRENT block like Block.blocksList[id].updateTick:
  // moving fluids run the flow logic, still water is a no-op, still lava
  // runs the fire-attempt tick. (Sending still water through update_flowing
  // would corrupt it into lavaMoving via the id+1 conversion -- real bug.)
  if (id == bid::kWaterMoving || id == bid::kLavaMoving) {
    update_flowing(x, y, z, r);
  } else if (id == bid::kLavaStill) {
    update_stationary_lava(x, y, z, r);
  }
}

// BlockStationary.updateTick (lava branch only; water is a no-op).
void FluidSim::update_stationary_lava(int x, int y, int z, JavaRandom& r) {
  const int tries = r.next_int(3);
  int cx = x, cy = y, cz = z;
  for (int i = 0; i < tries; ++i) {
    cx += r.next_int(3) - 1;
    ++cy;
    cz += r.next_int(3) - 1;
    const int id = w_.get_id(cx, cy, cz);
    if (id == 0) {
      if (burnable(w_, cx - 1, cy, cz) || burnable(w_, cx + 1, cy, cz) || burnable(w_, cx, cy, cz - 1) ||
          burnable(w_, cx, cy, cz + 1) || burnable(w_, cx, cy - 1, cz) || burnable(w_, cx, cy + 1, cz)) {
        set_notify(cx, cy, cz, bid::kFire);
        return;
      }
    } else if (mat_solid(id)) {
      return;
    }
  }
}

void FluidSim::update_flowing(int x, int y, int z, JavaRandom& r) {
  const int id = w_.get_id(x, y, z);
  const bool water = is_water(id);
  int decay = flow_decay(x, y, z, water);
  const int fall_inc = (!water) ? 2 : 1;  // lava slows outside hell (var7)
  bool to_still = true;
  if (decay > 0) {
    int best = -100;
    num_sources_ = 0;
    best = smallest_flow_decay(x - 1, y, z, water, best);
    best = smallest_flow_decay(x + 1, y, z, water, best);
    best = smallest_flow_decay(x, y, z - 1, water, best);
    best = smallest_flow_decay(x, y, z + 1, water, best);
    int nd = best + fall_inc;
    if (nd >= 8 || best < 0) nd = -1;
    if (flow_decay(x, y + 1, z, water) >= 0) {
      const int up = flow_decay(x, y + 1, z, water);
      nd = (up >= 8) ? up : up + 8;
    }
    if (num_sources_ >= 2 && water) {
      const int below = w_.get_id(x, y - 1, z);
      if (mat_solid(below)) {
        nd = 0;
      } else if (mat_of(below) == 1 && w_.get_meta(x, y - 1, z) == 0) {
        nd = 0;
      }
    }
    // NOTE: r is deco_rand on the spring's direct call, world_rand on nested
    // scheduled calls -- the stream choice is load-bearing.
    if (!water && decay < 8 && nd < 8 && nd > decay && r.next_int(4) != 0) {
      nd = decay;
      to_still = false;
    }
    if (nd != decay) {
      decay = nd;
      if (nd < 0) {
        set_notify(x, y, z, 0);
      } else {
        set_meta_only(x, y, z, nd);
        schedule(x, y, z, id, wrand_);
        notify_neighbors(x, y, z, id);
      }
    } else if (to_still) {
      // func_30003_j: moving -> still. setBlockAndMetadata fires onBlockAdded
      // (harden check + schedule for moving fluids).
      set_silent(x, y, z, id + 1, w_.get_meta(x, y, z));
      fluid_added(x, y, z);
    }
  } else {
    // func_30003_j (decay == 0 source).
    set_silent(x, y, z, id + 1, w_.get_meta(x, y, z));
    fluid_added(x, y, z);
  }

  if (displaceable(x, y - 1, z, water)) {
    if (!water && mat_of(w_.get_id(x, y - 1, z)) == 1) {
      set_notify(x, y - 1, z, bid::kStone);
      mix_effects(x, y - 1, z);
      return;
    }
    if (decay >= 8) {
      set_meta_notify(x, y - 1, z, id, decay);
    } else {
      set_meta_notify(x, y - 1, z, id, decay + 8);
    }
  } else if (decay >= 0 && (decay == 0 || blocks_flow(x, y - 1, z))) {
    optimal_dirs(x, y, z, water);
    int nd = decay + fall_inc;
    if (decay >= 8) nd = 1;
    if (nd >= 8) return;
    // NOTE: opt_dirs_ is the SHARED member (like the source's
    // isOptimalFlowDirection): nested ticks clobber it, and the reads below
    // use whatever values survive -- load-bearing, do NOT copy to locals.
    if (opt_dirs_[0]) flow_into(x - 1, y, z, id, nd);
    if (opt_dirs_[1]) flow_into(x + 1, y, z, id, nd);
    if (opt_dirs_[2]) flow_into(x, y, z - 1, id, nd);
    if (opt_dirs_[3]) flow_into(x, y, z + 1, id, nd);
  }
}

void FluidSim::flow_into(int x, int y, int z, int fluid_id, int meta) {
  if (!displaceable(x, y, z, is_water(fluid_id))) return;
  const int old = w_.get_id(x, y, z);
  if (old > 0) {
    if (is_lava(fluid_id)) {
      mix_effects(x, y, z);
    }  // else: dropBlockAsItem spawns entities only (Math.random) -- skipped.
  }
  set_meta_notify(x, y, z, fluid_id, meta);
}

int FluidSim::flow_decay(int x, int y, int z, bool water) const {
  const int id = w_.get_id(x, y, z);
  const int m = mat_of(id);
  if ((water && m != 1) || (!water && m != 2)) return -1;
  return w_.get_meta(x, y, z);
}

int FluidSim::smallest_flow_decay(int x, int y, int z, bool water, int cur) {
  const int d = flow_decay(x, y, z, water);
  if (d < 0) return cur;
  if (d == 0) ++num_sources_;
  int e = d >= 8 ? 0 : d;
  return (cur >= 0 && e >= cur) ? cur : e;
}

bool FluidSim::blocks_flow(int x, int y, int z) const {
  const int id = w_.get_id(x, y, z);
  if (id == bid::kDoorWood || id == bid::kDoorSteel || id == bid::kSignPost || id == bid::kLadder ||
      id == bid::kReed) {
    return true;
  }
  if (id == 0) return false;
  if (id == bid::kPortal) return true;
  return mat_solid(id);
}

bool FluidSim::displaceable(int x, int y, int z, bool water) const {
  const int id = w_.get_id(x, y, z);
  const int m = mat_of(id);
  if (water ? (m == 1) : (m == 2)) return false;
  if (m == 2) return false;  // lava displaces nothing lava-side either
  return !blocks_flow(x, y, z);
}

int FluidSim::flow_cost(int x, int y, int z, bool water, int depth, int from_dir) const {
  int best = 1000;
  for (int d = 0; d < 4; ++d) {
    if ((d == 0 && from_dir == 1) || (d == 1 && from_dir == 0) || (d == 2 && from_dir == 3) ||
        (d == 3 && from_dir == 2)) {
      continue;
    }
    int nx = x, nz = z;
    if (d == 0) --nx;
    if (d == 1) ++nx;
    if (d == 2) --nz;
    if (d == 3) ++nz;
    const int nid = w_.get_id(nx, y, nz);
    const int nm = mat_of(nid);
    const bool same_source =
        (water ? (nm == 1) : (nm == 2)) && w_.get_meta(nx, y, nz) == 0;
    if (!blocks_flow(nx, y, nz) && !same_source) {
      if (!blocks_flow(nx, y - 1, nz)) return depth;
      if (depth < 4) {
        const int c = flow_cost(nx, y, nz, water, depth + 1, d);
        if (c < best) best = c;
      }
    }
  }
  return best;
}

void FluidSim::optimal_dirs(int x, int y, int z, bool water) {
  for (int d = 0; d < 4; ++d) {
    flow_cost_[d] = 1000;
    int nx = x, nz = z;
    if (d == 0) --nx;
    if (d == 1) ++nx;
    if (d == 2) --nz;
    if (d == 3) ++nz;
    const int nid = w_.get_id(nx, y, nz);
    const int nm = mat_of(nid);
    const bool same_source =
        (water ? (nm == 1) : (nm == 2)) && w_.get_meta(nx, y, nz) == 0;
    if (!blocks_flow(nx, y, nz) && !same_source) {
      flow_cost_[d] = !blocks_flow(nx, y - 1, nz) ? 0 : flow_cost(nx, y, nz, water, 1, d);
    }
  }
  int best = flow_cost_[0];
  for (int d = 1; d < 4; ++d)
    if (flow_cost_[d] < best) best = flow_cost_[d];
  for (int d = 0; d < 4; ++d) opt_dirs_[d] = (flow_cost_[d] == best);
}

// ---- world-write primitives ----

bool FluidSim::set_notify(int x, int y, int z, int id) {
  // setBlockWithNotify = setBlock (early-out on same id) + notify.
  const int old = w_.get_id(x, y, z);
  if (old == id) return false;
  w_.set_id(x, y, z, id);  // fires removal, zeroes meta
  fluid_added(x, y, z);
  notify_neighbors(x, y, z, id);
  return true;
}

bool FluidSim::set_meta_notify(int x, int y, int z, int id, int m) {
  const int old = w_.get_id(x, y, z);
  const int oldm = w_.get_meta(x, y, z);
  bool changed;
  if (old == id && oldm == m) {
    changed = false;
  } else {
    w_.set_id_meta(x, y, z, id, m);  // fires removal
    fluid_added(x, y, z);
    changed = true;
  }
  if (changed) notify_neighbors(x, y, z, id);
  return changed;
}

void FluidSim::set_meta_only(int x, int y, int z, int m) {
  // setBlockMetadataWithNotify: fluids never require self-notify.
  const int oldm = w_.get_meta(x, y, z);
  if (oldm == m) return;
  w_.set_meta_raw(x, y, z, m);
  notify_neighbors(x, y, z, w_.get_id(x, y, z));
}

void FluidSim::set_silent(int x, int y, int z, int id, int m) {
  // setBlockAndMetadata: removal fires, no notify, no schedule here
  // (callers schedule explicitly like the source).
  w_.set_id_meta(x, y, z, id, m);
}

void FluidSim::notify_neighbors(int x, int y, int z, int id) {
  on_neighbor(x - 1, y, z, id);
  on_neighbor(x + 1, y, z, id);
  on_neighbor(x, y - 1, z, id);
  on_neighbor(x, y + 1, z, id);
  on_neighbor(x, y, z - 1, id);
  on_neighbor(x, y, z + 1, id);
}

void FluidSim::on_neighbor(int x, int y, int z, int nid) {
  (void)nid;
  if (editing_) return;  // notifyBlockOfNeighborChange gate
  const int id = w_.get_id(x, y, z);
  if (is_fluid(id)) {
    check_harden(x, y, z);
    if (!is_moving(id)) {
      // BlockStationary conversion: id-1, same meta, silent, then schedule.
      editing_ = true;
      set_silent(x, y, z, id - 1, w_.get_meta(x, y, z));
      schedule(x, y, z, id - 1, wrand_);
      editing_ = false;
    }
  } else if (id == bid::kSand || id == bid::kGravel) {
    schedule_sand(x, y, z);
  }
  // All other onNeighborBlockChange impls are block-neutral in worldgen
  // (leaves/plant pops only trigger on support loss, which fluids never
  // cause here -- verified against the oracle; extended if diffs demand).
}

void FluidSim::check_harden(int x, int y, int z) {
  const int id = w_.get_id(x, y, z);
  if (!is_lava(id)) return;
  const bool adj_water = mat_of(w_.get_id(x, y, z - 1)) == 1 || mat_of(w_.get_id(x, y, z + 1)) == 1 ||
                         mat_of(w_.get_id(x - 1, y, z)) == 1 || mat_of(w_.get_id(x + 1, y, z)) == 1 ||
                         mat_of(w_.get_id(x, y + 1, z)) == 1;
  if (!adj_water) return;
  const int meta = w_.get_meta(x, y, z);
  if (meta == 0) {
    set_notify(x, y, z, bid::kObsidian);
  } else if (meta <= 4) {
    set_notify(x, y, z, bid::kCobble);
  }
  mix_effects(x, y, z);
}

void FluidSim::mix_effects(int x, int y, int z) {
  (void)x;
  (void)y;
  (void)z;
  // triggerLavaMixEffects: sound (no draws) + 2 world.rand pitch floats.
  // Particles use Math.random (block-neutral, skipped).
  wrand_.next_float();
  wrand_.next_float();
}

void FluidSim::sand_try_fall(int x, int y, int z) {
  const int id = w_.get_id(x, y, z);
  if (!can_fall_below(x, y - 1, z) || y < 0) return;
  set_notify(x, y, z, 0);
  int yy = y;
  while (can_fall_below(x, yy - 1, z) && yy > 0) --yy;
  if (yy > 0) set_notify(x, yy, z, id);
}

bool FluidSim::can_fall_below(int x, int y, int z) const {
  const int id = w_.get_id(x, y, z);
  if (id == 0 || id == bid::kFire) return true;
  const int m = mat_of(id);
  return m == 1 || m == 2;
}

bool FluidSim::burnable(RegionWorld& w, int x, int y, int z) {
  // Blocks with Material.canBurn (wood, leaves, cloth, tnt, vine).
  const int id = w.get_id(x, y, z);
  return id == bid::kLog || id == bid::kWoodPlank || id == bid::kLeaves || id == bid::kWool ||
         id == bid::kTnt || id == bid::kVine;
}

// BlockFluid.onBlockAdded = checkForHarden only; BlockFlowing adds the
// schedule. (Stationary conversions therefore tick nothing further.)
void FluidSim::fluid_added(int x, int y, int z) {
  const int id = w_.get_id(x, y, z);
  if (!is_fluid(id)) {
    if (id == bid::kSand || id == bid::kGravel) schedule_sand(x, y, z);
    return;
  }
  check_harden(x, y, z);
  if (id == bid::kWaterMoving || id == bid::kLavaMoving) schedule(x, y, z, id, wrand_);
}

void FluidSim::schedule_sand(int x, int y, int z) {
  if (!immediate_) return;
  sand_try_fall(x, y, z);  // fallInstantly is always true during populate
}

}  // namespace craftpp::world
