#pragma once

#include <optional>
#include <vector>

#include "core/aabb.hpp"
#include "core/vec3.hpp"

namespace craftpp::world {

// Read-only block view backing collision queries (RegionWorld adapts to
// this; the live World will too in M5). Mirrors IBlockAccess getBlockId /
// getBlockMetadata.
struct BlockView {
  virtual ~BlockView() = default;
  virtual int block_id(int x, int y, int z) const = 0;
  virtual int block_meta(int x, int y, int z) const = 0;
};

// Collision query engine mirroring Block.getCollisionBoundingBoxFromPool /
// getCollidingBoundingBoxes.
//
// Faithfulness note: the source stores bounds ON the shared Block instance
// (setBlockBounds mutates this.minX...), so the single-box query for blocks
// that shape their bounds inside getCollidingBoundingBoxes (panes, brewing
// stand, piston extension, end frame) returns whatever a previous
// getCollidingBoundingBoxes call left behind — PER BLOCK TYPE. This class
// keeps that sticky state per block id (ctor bounds at start: full cube
// everywhere except bed 9/16), updated at exactly the points where the
// source calls setBlockBounds. Same call order => same boxes, bit-for-bit
// (proven against the GenCollision oracle, which exhibits the leftovers).
class BlockCollider {
 public:
  BlockCollider();

  // Mirrors Block.getCollisionBoundingBoxFromPool. Pure for shaped blocks
  // (cactus/cake/door/fence/gate/ladder/lily/snow/soul/trapdoor/...),
  // sticky-bounds read for the rest. nullopt = Java null.
  std::optional<Aabb> collision_box(int id, int meta, int x, int y, int z, const BlockView& w);

  // Mirrors Block.getCollidingBoundingBoxes (including the multi-box
  // overrides). Appends each shaped box intersecting entity_box in source
  // call order, and updates the sticky bounds exactly like the source.
  void colliding_boxes(int id, int meta, int x, int y, int z, const Aabb& entity_box,
                       const BlockView& w, std::vector<Aabb>& out);

 private:
  // Local (0..1-ish) sticky bounds per block id.
  Aabb sticky_[256];
  void set_sticky(int id, double x0, double y0, double z0, double x1, double y1, double z1) {
    sticky_[id] = Aabb(x0, y0, z0, x1, y1, z1);
  }
};

// Mirrors BlockFluid.getFluidHeightPercent (static in source).
float fluid_height_percent(int meta);

// Mirrors BlockFluid.getFlowVector (+velocityToAddToEntity accumulation).
// fluid_id selects the material (water ids vs lava ids).
Vec3 fluid_flow_vector(int fluid_id, int x, int y, int z, const BlockView& w);

// Mirrors Block.getIsBlockSolid default (material.isSolid()), used by the
// falling-water branch of getFlowVector via BlockFluid.getIsBlockSolid
// (fluid->false, side==1->true, ice->false, else this).
bool fluid_is_block_solid(int fluid_id, int nx, int ny, int nz, int side, const BlockView& w);

}  // namespace craftpp::world
