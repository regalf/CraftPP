#include "world/tile.hpp"

#include "entity/items.hpp"
#include "world/blocks.hpp"
#include "world/item_ids.hpp"

namespace craftpp::world::tile {

int ChestEntity::kind_id() const { return bid::kChest; }
int FurnaceEntity::kind_id() const { return bid::kFurnaceIdle; }
int SignEntity::kind_id() const { return bid::kSignPost; }

int burn_time_for(int id) {
  using namespace bid;
  if (id < 256) {
    // Mirrors blockMaterial == Material.wood (planks/log/bookshelf/chest/
    // workbench/sign/door/plates/trapdoor/jukebox/fence/gates/noteblock/
    // mushroom caps/wood stairs); sapling is special-cased to 100.
    if (id == kSapling) return 100;
    switch (id) {
      case kWoodPlank:
      case kLog:
      case kNoteBlock:
      case kBookshelf:
      case kChest:
      case kWorkbench:
      case kSignPost:
      case kSignWall:
      case kDoorWood:
      case kPlateWood:
      case kTrapDoor:
      case kJukebox:
      case kFence:
      case kFenceGate:
      case kLockedChest:
      case kMushroomCapBrown:
      case kMushroomCapRed:
      case kStairsWood:
        return 300;
      default:
        return 0;
    }
  }
  using namespace iid;
  if (id == kStick) return 100;
  if (id == kCoal) return 1600;
  if (id == kBucketLava) return 20000;
  if (id == kBlazeRod) return 2400;
  return 0;
}

bool smelt_result(int in, entity::ItemStack& out) {
  using namespace bid;
  using namespace iid;
  switch (in) {
    case kIronOre:
      out = entity::ItemStack(kIngotIron, 1, 0);
      return true;
    case kGoldOre:
      out = entity::ItemStack(kIngotGold, 1, 0);
      return true;
    case kDiamondOre:
      out = entity::ItemStack(kDiamond, 1, 0);
      return true;
    case kSand:
      out = entity::ItemStack(kGlass, 1, 0);
      return true;
    case kCobble:
      out = entity::ItemStack(kStone, 1, 0);
      return true;
    case kLog:
      out = entity::ItemStack(kCoal, 1, 1);
      return true;  // charcoal
    case kCoalOre:
      out = entity::ItemStack(kCoal, 1, 0);
      return true;
    case kRedstoneOre:
      out = entity::ItemStack(kRedstone, 1, 0);
      return true;
    case kLapisOre:
      out = entity::ItemStack(kDyePowder, 1, 4);
      return true;
    case kCactus:
      out = entity::ItemStack(kDyePowder, 1, 2);
      return true;
    case kClay:
      out = entity::ItemStack(kBrick, 1, 0);
      return true;
    case kPorkRaw:
      out = entity::ItemStack(kPorkCooked, 1, 0);
      return true;
    case kBeefRaw:
      out = entity::ItemStack(kBeefCooked, 1, 0);
      return true;
    case kChickenRaw:
      out = entity::ItemStack(kChickenCooked, 1, 0);
      return true;
    case kFishRaw:
      out = entity::ItemStack(kFishCooked, 1, 0);
      return true;
    default:
      return false;
  }
}

static bool furnace_can_smelt(const FurnaceEntity& f) {
  if (!f.items[0].has_value()) return false;
  entity::ItemStack out(0, 0, 0);
  if (!smelt_result(f.items[0]->item_id, out)) return false;
  if (!f.items[2].has_value()) return true;
  const auto& dst = *f.items[2];
  if (dst.item_id != out.item_id) return false;
  if (dst.damage != out.damage) return false;
  return dst.stack_size < 64 && dst.stack_size < out.max_stack();
}

static void furnace_do_smelt(FurnaceEntity& f) {
  if (!furnace_can_smelt(f)) return;
  entity::ItemStack out(0, 0, 0);
  smelt_result(f.items[0]->item_id, out);
  if (!f.items[2].has_value()) {
    f.items[2] = out;
  } else {
    f.items[2]->stack_size += 1;
  }
  f.items[0]->stack_size -= 1;
  if (f.items[0]->stack_size <= 0) f.items[0] = std::nullopt;
}

void FurnaceEntity::update(TileWorld& w) {
  (void)w;
  const bool was_burning = burn_time > 0;
  if (burn_time > 0) --burn_time;
  bool dirty = false;
  if (burn_time == 0 && furnace_can_smelt(*this)) {
    const auto& fuel = items[1];
    current_burn = burn_time = fuel.has_value() ? burn_time_for(fuel->item_id) : 0;
    if (burn_time > 0) {
      dirty = true;
      items[1]->stack_size -= 1;
      // Lava bucket leaves an empty bucket behind like the source? The
      // source just consumes; buckets handled by container items (M5 gap:
      // consume plainly).
      if (items[1]->stack_size <= 0) items[1] = std::nullopt;
    }
  }
  if (burning() && furnace_can_smelt(*this)) {
    if (++cook_time >= 200) {
      cook_time = 0;
      furnace_do_smelt(*this);
      dirty = true;
    }
  } else {
    cook_time = 0;
  }
  if (was_burning != burning()) {
    // BlockFurnace.updateFurnaceBlockState: swap idle/burn, keep the tile.
    w.set_block_raw(x, y, z, burning() ? bid::kFurnaceBurn : bid::kFurnaceIdle,
                    0);  // meta preserved by caller (always 0 here)
    dirty = true;
  }
  if (dirty) w.mark_dirty(x, y, z);
}

}  // namespace craftpp::world::tile
