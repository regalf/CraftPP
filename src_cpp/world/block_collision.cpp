#include "world/block_collision.hpp"

#include "world/blocks.hpp"

namespace craftpp::world {
namespace {

using bid::kBed;
using bid::kBrewingStand;
using bid::kButton;
using bid::kCactus;
using bid::kCake;
using bid::kCauldron;
using bid::kDoorSteel;
using bid::kDoorWood;
using bid::kDragonEgg;
using bid::kEnchantTable;
using bid::kEndFrame;
using bid::kEndPortal;
using bid::kFarmland;
using bid::kFence;
using bid::kFenceGate;
using bid::kFire;
using bid::kLadder;
using bid::kLavaMoving;
using bid::kLavaStill;
using bid::kLever;
using bid::kLilyPad;
using bid::kPaneGlass;
using bid::kPaneIron;
using bid::kPistonBase;
using bid::kPistonExt;
using bid::kPistonMoving;
using bid::kPistonSticky;
using bid::kPlateStone;
using bid::kPlateWood;
using bid::kPortal;
using bid::kRail;
using bid::kRailDetector;
using bid::kRailPowered;
using bid::kRedstoneWire;
using bid::kRepeaterIdle;
using bid::kRepeaterOn;
using bid::kSignPost;
using bid::kSignWall;
using bid::kSnowCover;
using bid::kSoulSand;
using bid::kStairsBrick;
using bid::kStairsCobble;
using bid::kStairsNether;
using bid::kStairsStoneBrick;
using bid::kStairsWood;
using bid::kTorch;
using bid::kTorchRedIdle;
using bid::kTorchRedOn;
using bid::kTrapDoor;
using bid::kVine;
using bid::kWaterMoving;
using bid::kWaterStill;
using bid::kWeb;

bool is_fluid(int id) {
  return id == kWaterMoving || id == kWaterStill || id == kLavaMoving || id == kLavaStill;
}

// BlockFlower family (all null collision): sapling/tallgrass/deadbush/
// flowers/mushrooms/crops/stems/reed/netherwart. Reed is Block-based but
// also returns null.
bool is_flower_null(int id) {
  switch (id) {
    case bid::kSapling:
    case bid::kTallGrass:
    case bid::kDeadBush:
    case bid::kFlowerYellow:
    case bid::kFlowerRed:
    case bid::kMushroomBrown:
    case bid::kMushroomRed:
    case bid::kCrops:
    case bid::kReed:
    case bid::kPumpkinStem:
    case bid::kMelonStem:
    case bid::kNetherWart:
    case bid::kVine:
    case bid::kWeb:
    case bid::kFire:
    case bid::kPortal:
    case bid::kSignPost:
    case bid::kSignWall:
    case bid::kTorch:
    case bid::kTorchRedIdle:
    case bid::kTorchRedOn:
    case bid::kLever:
    case bid::kButton:
    case bid::kPlateStone:
    case bid::kPlateWood:
    case bid::kRail:
    case bid::kRailPowered:
    case bid::kRailDetector:
    case bid::kRedstoneWire:
    case bid::kRepeaterIdle:
    case bid::kRepeaterOn:
      return true;
    default:
      return false;
  }
}

// Mirrors BlockFence.isFenceAt.
bool is_fence_at(const BlockView& w, int x, int y, int z) {
  const int id = w.block_id(x, y, z);
  if (id == kFence || id == kFenceGate) return true;
  if (id == 0) return false;  // blocksList[0] == null
  return bid::material_opaque(id) && bid::renders_as_normal(id) && id != bid::kPumpkin;
}

// Mirrors BlockPane.func_35298_d for pane block `self`.
bool pane_connects(int self, int id) {
  return bid::is_opaque(id) || id == self || id == bid::kGlass;
}

void add_if_hit(const Aabb& entity_box, const BlockView& w, int x, int y, int z, double x0, double y0,
                double z0, double x1, double y1, double z1, std::vector<Aabb>& out) {
  Aabb b(x + x0, y + y0, z + z0, x + x1, y + y1, z + z1);
  if (entity_box.intersects(b)) out.push_back(b);
}

}  // namespace

float fluid_height_percent(int meta) {
  if (meta >= 8) meta = 0;
  return static_cast<float>(meta + 1) / 9.0f;
}

bool fluid_is_block_solid(int fluid_id, int nx, int ny, int nz, int side, const BlockView& w) {
  const int id = w.block_id(nx, ny, nz);
  const bool same = (fluid_id == kWaterMoving || fluid_id == kWaterStill)
                        ? (id == kWaterMoving || id == kWaterStill)
                        : (id == kLavaMoving || id == kLavaStill);
  if (same) return false;
  if (side == 1) return true;
  if (id == bid::kIce) return false;
  return bid::material_is_solid(id);
}

namespace {
// Mirrors BlockFluid.getEffectiveFlowDecay.
int effective_flow_decay(int fluid_id, const BlockView& w, int x, int y, int z) {
  const int id = w.block_id(x, y, z);
  const bool same = (fluid_id == kWaterMoving || fluid_id == kWaterStill)
                        ? (id == kWaterMoving || id == kWaterStill)
                        : (id == kLavaMoving || id == kLavaStill);
  if (!same) return -1;
  int meta = w.block_meta(x, y, z);
  if (meta >= 8) meta = 0;
  return meta;
}
}  // namespace

Vec3 fluid_flow_vector(int fluid_id, int x, int y, int z, const BlockView& w) {
  Vec3 flow(0.0, 0.0, 0.0);
  const int center = effective_flow_decay(fluid_id, w, x, y, z);
  for (int dir = 0; dir < 4; ++dir) {
    int nx = x, nz = z;
    if (dir == 0) nx = x - 1;
    if (dir == 1) nz = z - 1;
    if (dir == 2) ++nx;
    if (dir == 3) ++nz;
    const int nd = effective_flow_decay(fluid_id, w, nx, y, nz);
    if (nd < 0) {
      if (!bid::material_is_solid(w.block_id(nx, y, nz))) {
        const int below = effective_flow_decay(fluid_id, w, nx, y - 1, nz);
        if (below >= 0) {
          const int push = below - (center - 8);
          flow = flow.add((nx - x) * push, (y - y) * push, (nz - z) * push);
        }
      }
    } else {
      const int push = nd - center;
      flow = flow.add((nx - x) * push, (y - y) * push, (nz - z) * push);
    }
  }
  if (w.block_meta(x, y, z) >= 8) {
    bool falling = false;
    if (!falling && fluid_is_block_solid(fluid_id, x, y, z - 1, 2, w)) falling = true;
    if (!falling && fluid_is_block_solid(fluid_id, x, y, z + 1, 3, w)) falling = true;
    if (!falling && fluid_is_block_solid(fluid_id, x - 1, y, z, 4, w)) falling = true;
    if (!falling && fluid_is_block_solid(fluid_id, x + 1, y, z, 5, w)) falling = true;
    if (!falling && fluid_is_block_solid(fluid_id, x, y + 1, z - 1, 2, w)) falling = true;
    if (!falling && fluid_is_block_solid(fluid_id, x, y + 1, z + 1, 3, w)) falling = true;
    if (!falling && fluid_is_block_solid(fluid_id, x - 1, y + 1, z, 4, w)) falling = true;
    if (!falling && fluid_is_block_solid(fluid_id, x + 1, y + 1, z, 5, w)) falling = true;
    if (falling) flow = flow.normalize().add(0.0, -6.0, 0.0);
  }
  return flow.normalize();  // source normalizes again on exit
}

BlockCollider::BlockCollider() {
  for (int i = 0; i < 256; ++i) sticky_[i] = Aabb(0, 0, 0, 1, 1, 1);
  sticky_[bid::kBed] = Aabb(0, 0, 0, 1, 9.0 / 16.0, 1);  // BlockBed ctor setBounds
}

std::optional<Aabb> BlockCollider::collision_box(int id, int meta, int x, int y, int z,
                                                 const BlockView& w) {
  const double xd = x, yd = y, zd = z;
  if (is_flower_null(id) || is_fluid(id)) return std::nullopt;
  switch (id) {
    case kCactus: {
      const double e = 1.0 / 16.0;
      return Aabb(xd + e, yd, zd + e, xd + 1.0 - e, yd + 1.0 - e, zd + 1.0 - e);
    }
    case kCake: {
      const double e = 1.0 / 16.0;
      const double cut = (1 + meta * 2) / 16.0;
      return Aabb(xd + cut, yd, zd + e, xd + 1.0 - e, yd + 0.5 - e, zd + 1.0 - e);
    }
    case kDoorWood:
    case kDoorSteel: {
      // setDoorRotation(getState(meta)); initial full box always overwritten.
      const int state = (meta & 4) == 0 ? (meta - 1) & 3 : meta & 3;
      const double t = 3.0 / 16.0;
      if (state == 0) return Aabb(xd, yd, zd, xd + 1.0, yd + 1.0, zd + t);
      if (state == 1) return Aabb(xd + 1.0 - t, yd, zd, xd + 1.0, yd + 1.0, zd + 1.0);
      if (state == 2) return Aabb(xd, yd, zd + 1.0 - t, xd + 1.0, yd + 1.0, zd + 1.0);
      return Aabb(xd, yd, zd, xd + t, yd + 1.0, zd + 1.0);
    }
    case kFarmland:
      return Aabb(xd, yd, zd, xd + 1.0, yd + 1.0, zd + 1.0);
    case kFence: {
      const bool mz0 = is_fence_at(w, x, y, z - 1);
      const bool mz1 = is_fence_at(w, x, y, z + 1);
      const bool mx0 = is_fence_at(w, x - 1, y, z);
      const bool mx1 = is_fence_at(w, x + 1, y, z);
      double ax0 = 6.0 / 16.0, ax1 = 10.0 / 16.0, az0 = 6.0 / 16.0, az1 = 10.0 / 16.0;
      if (mz0) az0 = 0.0;
      if (mz1) az1 = 1.0;
      if (mx0) ax0 = 0.0;
      if (mx1) ax1 = 1.0;
      return Aabb(xd + ax0, yd, zd + az0, xd + ax1, yd + 1.5, zd + az1);
    }
    case kFenceGate:
      if ((meta & 4) != 0) return std::nullopt;
      return Aabb(xd, yd, zd, xd + 1.0, yd + 1.5, zd + 1.0);
    case kLadder: {
      // Bounds start full (ctor default), per-meta faces overwrite.
      const double t = 2.0 / 16.0;
      if (meta == 2) return Aabb(xd, yd, zd + 1.0 - t, xd + 1.0, yd + 1.0, zd + 1.0);
      if (meta == 3) return Aabb(xd, yd, zd, xd + 1.0, yd + 1.0, zd + t);
      if (meta == 4) return Aabb(xd + 1.0 - t, yd, zd, xd + 1.0, yd + 1.0, zd + 1.0);
      if (meta == 5) return Aabb(xd, yd, zd, xd + t, yd + 1.0, zd + 1.0);
      return Aabb(xd, yd, zd, xd + 1.0, yd + 1.0, zd + 1.0);
    }
    case kLilyPad:
      return Aabb(xd, yd, zd, xd + 1.0, yd + 0.015625, zd + 1.0);
    case kTrapDoor: {
      const double t = 3.0 / 16.0;
      if ((meta & 4) == 0) return Aabb(xd, yd, zd, xd + 1.0, yd + t, zd + 1.0);
      switch (meta & 3) {
        case 0:
          return Aabb(xd, yd, zd + 1.0 - t, xd + 1.0, yd + 1.0, zd + 1.0);
        case 1:
          return Aabb(xd, yd, zd, xd + 1.0, yd + 1.0, zd + t);
        case 2:
          return Aabb(xd + 1.0 - t, yd, zd, xd + 1.0, yd + 1.0, zd + 1.0);
        default:
          return Aabb(xd, yd, zd, xd + t, yd + 1.0, zd + 1.0);
      }
    }
    case kSoulSand: {
      const double t = 2.0 / 16.0;
      return Aabb(xd, yd, zd, xd + 1.0, yd + 1.0 - t, zd + 1.0);
    }
    case kSnowCover: {
      if ((meta & 7) < 3) return std::nullopt;
      return Aabb(xd, yd, zd, xd + 1.0, yd + 0.5, zd + 1.0);
    }
    case kPistonMoving:
      return std::nullopt;  // needs TileEntityPiston (M5); Java also null without TE
    default:
      break;
  }
  // Default: sticky-bounds read (full cube for static blocks; bed 9/16;
  // pane/brewing/pistonext/endframe carry getColliding leftovers per id).
  if (id == 0) return std::nullopt;  // blocksList[0] == null: no box at all
  const Aabb& s = sticky_[id];
  return Aabb(xd + s.min_x, yd + s.min_y, zd + s.min_z, xd + s.max_x, yd + s.max_y, zd + s.max_z);
}

void BlockCollider::colliding_boxes(int id, int meta, int x, int y, int z, const Aabb& entity_box,
                                  const BlockView& w, std::vector<Aabb>& out) {
  switch (id) {
    case kStairsWood:
    case kStairsCobble:
    case kStairsBrick:
    case kStairsStoneBrick:
    case kStairsNether: {
      // Two shaped boxes per orientation; meta outside 0-3 adds nothing.
      // Sticky reset to full at the end, exactly like the source.
      if (meta == 0) {
        set_sticky(id, 0, 0, 0, 0.5, 0.5, 1);
        add_if_hit(entity_box, w, x, y, z, 0, 0, 0, 0.5, 0.5, 1, out);
        set_sticky(id, 0.5, 0, 0, 1, 1, 1);
        add_if_hit(entity_box, w, x, y, z, 0.5, 0, 0, 1, 1, 1, out);
      } else if (meta == 1) {
        set_sticky(id, 0, 0, 0, 0.5, 1, 1);
        add_if_hit(entity_box, w, x, y, z, 0, 0, 0, 0.5, 1, 1, out);
        set_sticky(id, 0.5, 0, 0, 1, 0.5, 1);
        add_if_hit(entity_box, w, x, y, z, 0.5, 0, 0, 1, 0.5, 1, out);
      } else if (meta == 2) {
        set_sticky(id, 0, 0, 0, 1, 0.5, 0.5);
        add_if_hit(entity_box, w, x, y, z, 0, 0, 0, 1, 0.5, 0.5, out);
        set_sticky(id, 0, 0, 0.5, 1, 1, 1);
        add_if_hit(entity_box, w, x, y, z, 0, 0, 0.5, 1, 1, 1, out);
      } else if (meta == 3) {
        set_sticky(id, 0, 0, 0, 1, 1, 0.5);
        add_if_hit(entity_box, w, x, y, z, 0, 0, 0, 1, 1, 0.5, out);
        set_sticky(id, 0, 0, 0.5, 1, 0.5, 1);
        add_if_hit(entity_box, w, x, y, z, 0, 0, 0.5, 1, 0.5, 1, out);
      }
      set_sticky(id, 0, 0, 0, 1, 1, 1);
      return;
    }
    case kPaneIron:
    case kPaneGlass: {
      const bool mz0 = pane_connects(id, w.block_id(x, y, z - 1));
      const bool mz1 = pane_connects(id, w.block_id(x, y, z + 1));
      const bool mx0 = pane_connects(id, w.block_id(x - 1, y, z));
      const bool mx1 = pane_connects(id, w.block_id(x + 1, y, z));
      const double e0 = 7.0 / 16.0, e1 = 9.0 / 16.0;
      if ((!mx0 || !mx1) && (mx0 || mx1 || mz0 || mz1)) {
        if (mx0 && !mx1) {
          set_sticky(id, 0, 0, e0, 0.5, 1, e1);
          add_if_hit(entity_box, w, x, y, z, 0, 0, e0, 0.5, 1, e1, out);
        } else if (!mx0 && mx1) {
          set_sticky(id, 0.5, 0, e0, 1, 1, e1);
          add_if_hit(entity_box, w, x, y, z, 0.5, 0, e0, 1, 1, e1, out);
        }
      } else {
        set_sticky(id, 0, 0, e0, 1, 1, e1);
        add_if_hit(entity_box, w, x, y, z, 0, 0, e0, 1, 1, e1, out);
      }
      if ((!mz0 || !mz1) && (mx0 || mx1 || mz0 || mz1)) {
        if (mz0 && !mz1) {
          set_sticky(id, e0, 0, 0, e1, 1, 0.5);
          add_if_hit(entity_box, w, x, y, z, e0, 0, 0, e1, 1, 0.5, out);
        } else if (!mz0 && mz1) {
          set_sticky(id, e0, 0, 0.5, e1, 1, 1);
          add_if_hit(entity_box, w, x, y, z, e0, 0, 0.5, e1, 1, 1, out);
        }
      } else {
        set_sticky(id, e0, 0, 0, e1, 1, 1);
        add_if_hit(entity_box, w, x, y, z, e0, 0, 0, e1, 1, 1, out);
      }
      return;
    }
    case kPistonBase:
    case kPistonSticky:
      set_sticky(id, 0, 0, 0, 1, 1, 1);
      add_if_hit(entity_box, w, x, y, z, 0, 0, 0, 1, 1, 1, out);
      return;
    case kPistonExt: {
      const double s = 6.0 / 16.0, e = 10.0 / 16.0, q = 0.25, q3 = 12.0 / 16.0;
      switch (meta & 7) {
        case 0:
          set_sticky(id, 0, 0, 0, 1, q, 1);
          add_if_hit(entity_box, w, x, y, z, 0, 0, 0, 1, q, 1, out);
          set_sticky(id, s, q, s, e, 1, e);
          add_if_hit(entity_box, w, x, y, z, s, q, s, e, 1, e, out);
          break;
        case 1:
          set_sticky(id, 0, q3, 0, 1, 1, 1);
          add_if_hit(entity_box, w, x, y, z, 0, q3, 0, 1, 1, 1, out);
          set_sticky(id, s, 0, s, e, q3, e);
          add_if_hit(entity_box, w, x, y, z, s, 0, s, e, q3, e, out);
          break;
        case 2:
          set_sticky(id, 0, 0, 0, 1, 1, q);
          add_if_hit(entity_box, w, x, y, z, 0, 0, 0, 1, 1, q, out);
          set_sticky(id, q, s, q, q3, e, 1);
          add_if_hit(entity_box, w, x, y, z, q, s, q, q3, e, 1, out);
          break;
        case 3:
          set_sticky(id, 0, 0, q3, 1, 1, 1);
          add_if_hit(entity_box, w, x, y, z, 0, 0, q3, 1, 1, 1, out);
          set_sticky(id, q, s, 0, q3, e, q3);
          add_if_hit(entity_box, w, x, y, z, q, s, 0, q3, e, q3, out);
          break;
        case 4:
          set_sticky(id, 0, 0, 0, q, 1, 1);
          add_if_hit(entity_box, w, x, y, z, 0, 0, 0, q, 1, 1, out);
          set_sticky(id, s, q, q, e, q3, 1);
          add_if_hit(entity_box, w, x, y, z, s, q, q, e, q3, 1, out);
          break;
        case 5:
          set_sticky(id, q3, 0, 0, 1, 1, 1);
          add_if_hit(entity_box, w, x, y, z, q3, 0, 0, 1, 1, 1, out);
          set_sticky(id, 0, s, q, q3, e, q3);
          add_if_hit(entity_box, w, x, y, z, 0, s, q, q3, e, q3, out);
          break;
        default:
          break;
      }
      set_sticky(id, 0, 0, 0, 1, 1, 1);  // trailing reset after the switch
      return;
    }
    case kCauldron: {
      const double t = 2.0 / 16.0;
      set_sticky(id, 0, 0, 0, 1, 5.0 / 16.0, 1);
      add_if_hit(entity_box, w, x, y, z, 0, 0, 0, 1, 5.0 / 16.0, 1, out);
      set_sticky(id, 0, 0, 0, t, 1, 1);
      add_if_hit(entity_box, w, x, y, z, 0, 0, 0, t, 1, 1, out);
      set_sticky(id, 0, 0, 0, 1, 1, t);
      add_if_hit(entity_box, w, x, y, z, 0, 0, 0, 1, 1, t, out);
      set_sticky(id, 1 - t, 0, 0, 1, 1, 1);
      add_if_hit(entity_box, w, x, y, z, 1 - t, 0, 0, 1, 1, 1, out);
      set_sticky(id, 0, 0, 1 - t, 1, 1, 1);
      add_if_hit(entity_box, w, x, y, z, 0, 0, 1 - t, 1, 1, 1, out);
      set_sticky(id, 0, 0, 0, 1, 1, 1);  // setBlockBoundsForItemRender
      return;
    }
    case kBrewingStand:
      set_sticky(id, 7.0 / 16.0, 0, 7.0 / 16.0, 9.0 / 16.0, 14.0 / 16.0, 9.0 / 16.0);
      add_if_hit(entity_box, w, x, y, z, 7.0 / 16.0, 0, 7.0 / 16.0, 9.0 / 16.0, 14.0 / 16.0,
                 9.0 / 16.0, out);
      set_sticky(id, 0, 0, 0, 1, 2.0 / 16.0, 1);  // setBlockBoundsForItemRender
      add_if_hit(entity_box, w, x, y, z, 0, 0, 0, 1, 2.0 / 16.0, 1, out);
      return;
    case kEndPortal:
      return;  // adds nothing (but single-box path is a full cube)
    case kEndFrame: {
      set_sticky(id, 0, 0, 0, 1, 13.0 / 16.0, 1);
      add_if_hit(entity_box, w, x, y, z, 0, 0, 0, 1, 13.0 / 16.0, 1, out);
      if ((meta & 4) != 0) {
        set_sticky(id, 5.0 / 16.0, 13.0 / 16.0, 5.0 / 16.0, 11.0 / 16.0, 1, 11.0 / 16.0);
        add_if_hit(entity_box, w, x, y, z, 5.0 / 16.0, 13.0 / 16.0, 5.0 / 16.0, 11.0 / 16.0, 1,
                   11.0 / 16.0, out);
      }
      set_sticky(id, 0, 0, 0, 1, 13.0 / 16.0, 1);  // setBlockBoundsForItemRender
      return;
    }
    default:
      break;
  }
  if (auto b = collision_box(id, meta, x, y, z, w)) {
    if (entity_box.intersects(*b)) out.push_back(*b);
  }
}

}  // namespace craftpp::world
