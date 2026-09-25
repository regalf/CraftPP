#include "world/block_place.hpp"

#include "core/math_helper.hpp"
#include "world/blocks.hpp"

namespace craftpp::world::edit {
namespace {

using bid::kAir;

// Material.getIsHarvestable: false only for rock/iron/leaves/snow/
// craftedSnow/piston families (setNoHarvest).
bool harvestable_mat(int id) {
  switch (id) {
    // rock family
    case bid::kStone:
    case bid::kCobble:
    case bid::kBedrock:
    case bid::kGoldOre:
    case bid::kIronOre:
    case bid::kCoalOre:
    case bid::kLapisOre:
    case bid::kSandstone:
    case bid::kDiamondOre:
    case bid::kRedstoneOre:
    case bid::kRedstoneOreGlit:
    case bid::kDispenser:
    case bid::kNoteBlock:
    case bid::kFurnaceIdle:
    case bid::kFurnaceBurn:
    case bid::kCobbleMossy:
    case bid::kObsidian:
    case bid::kMobSpawner:
    case bid::kStoneBrick:
    case bid::kSilverfish:
    case bid::kNetherBrick:
    case bid::kNetherrack:
    case bid::kEnchantTable:
    case bid::kWhiteStone:
    case bid::kStairsCobble:
    case bid::kStairsBrick:
    case bid::kStairsStoneBrick:
    case bid::kStairsNether:
    case bid::kStepDouble:
    case bid::kStepSingle:
    case bid::kBrick:
    // iron family
    case bid::kDoorSteel:
    case bid::kRail:
    case bid::kRailPowered:
    case bid::kRailDetector:
    case bid::kPlateStone:
    case bid::kLadder:
    case bid::kCauldron:
    case bid::kBrewingStand:
    case bid::kPaneIron:
    case bid::kButton:
    case bid::kLever:
    // snow families (leaves/piston harvest like everything else)
    case bid::kSnowCover:
    case bid::kSnowBlock:
      return false;
    default:
      return true;
  }
}

// Tool material of an item id (SHIFTED ids: raw + 256).
// 0=wood 1=stone 2=iron 3=diamond 4=gold, -1=n/a.
int tool_material(int item) {
  switch (item) {
    case 268:
    case 269:
    case 270:
    case 271:
    case 290:
      return 0;
    case 272:
    case 273:
    case 274:
    case 275:
    case 291:
      return 1;
    case 256:
    case 257:
    case 258:
    case 267:
    case 292:
      return 2;
    case 276:
    case 277:
    case 278:
    case 279:
    case 293:
      return 3;
    case 283:
    case 284:
    case 285:
    case 286:
    case 294:
      return 4;
    default:
      return -1;
  }
}

float tool_efficiency(int mat) {
  switch (mat) {
    case 0:
      return 2.0f;
    case 1:
      return 4.0f;
    case 2:
      return 6.0f;
    case 3:
      return 8.0f;
    case 4:
      return 12.0f;
    default:
      return 1.0f;
  }
}

bool is_pickaxe(int item) {
  return item == 257 || item == 270 || item == 274 || item == 278 || item == 285;
}
bool is_spade(int item) {
  return item == 256 || item == 269 || item == 273 || item == 277 || item == 284;
}
bool is_axe(int item) {
  return item == 258 || item == 271 || item == 275 || item == 279 || item == 286;
}
bool is_sword(int item) {
  return item == 267 || item == 268 || item == 272 || item == 276 || item == 283;
}

// ItemTool.blocksEffectiveAgainst per tool class (block ids).
bool spade_effective(int id) {
  switch (id) {
    case bid::kGrass:
    case bid::kDirt:
    case bid::kSand:
    case bid::kGravel:
    case bid::kSnowCover:
    case bid::kSnowBlock:
    case bid::kClay:
    case bid::kFarmland:
    case bid::kSoulSand:
    case bid::kMycelium:
      return true;
    default:
      return false;
  }
}

bool axe_effective(int id) {
  switch (id) {
    case bid::kWoodPlank:
    case bid::kBookshelf:
    case bid::kLog:
    case bid::kChest:
    case bid::kLockedChest:
    case bid::kStepDouble:
    case bid::kStepSingle:
    case bid::kPumpkin:
    case bid::kPumpkinLantern:
      return true;
    default:
      return false;
  }
}

bool pickaxe_material(int id) {
  const auto m = bid::material_of(id);
  // ItemPickaxe.getStrVsBlock: iron/rock materials (block, not id, based).
  // Rock/iron-material ids: enumerate via harvestable_mat complement is wrong;
  // list rock+iron ids explicitly below (same set as harvestable_mat false
  // minus leaves/snow/piston families).
  switch (id) {
    case bid::kStone:
    case bid::kCobble:
    case bid::kBedrock:
    case bid::kGoldOre:
    case bid::kIronOre:
    case bid::kCoalOre:
    case bid::kLapisOre:
    case bid::kSandstone:
    case bid::kDiamondOre:
    case bid::kRedstoneOre:
    case bid::kRedstoneOreGlit:
    case bid::kDispenser:
    case bid::kNoteBlock:
    case bid::kFurnaceIdle:
    case bid::kFurnaceBurn:
    case bid::kCobbleMossy:
    case bid::kObsidian:
    case bid::kMobSpawner:
    case bid::kStoneBrick:
    case bid::kSilverfish:
    case bid::kNetherBrick:
    case bid::kWhiteStone:
    case bid::kStairsCobble:
    case bid::kStairsBrick:
    case bid::kStairsStoneBrick:
    case bid::kStairsNether:
    case bid::kStepDouble:
    case bid::kStepSingle:
    case bid::kBrick:
    case bid::kNetherrack:
    case bid::kEnchantTable:
    case bid::kEndFrame:
    case bid::kCauldron:
    case bid::kBrewingStand:
    case bid::kPaneIron:
    case bid::kDoorSteel:
    case bid::kRail:
    case bid::kRailPowered:
    case bid::kRailDetector:
    case bid::kPlateStone:
    case bid::kLadder:
    case bid::kButton:
    case bid::kLever:
      return true;
    default:
      return false;
  }
}

}  // namespace

float hardness(int id) {
  // Transcribed from Block.java initializers + subclass ctors.
  // -1 = unbreakable (blockStrength 0). Stairs take their model hardness.
  switch (id) {
    case bid::kStone:
      return 1.5f;
    case bid::kGrass:
    case bid::kGravel:
    case bid::kSponge:
    case bid::kClay:
    case bid::kFarmland:
    case bid::kMycelium:
      return 0.6f;
    case bid::kDirt:
    case bid::kSand:
    case bid::kSoulSand:
      return 0.5f;
    case bid::kWoodPlank:
    case bid::kChest:
    case bid::kWorkbench:
    case bid::kFence:
    case bid::kNetherBrick:
    case bid::kNetherFence:
    case bid::kStairsWood:  // model planks
    case bid::kStairsCobble:  // model cobble
    case bid::kStairsBrick:  // model brick
    case bid::kStairsNether:  // model netherBrick
    case bid::kStepDouble:
    case bid::kStepSingle:
    case bid::kCauldron:
    case bid::kCobbleMossy:
    case bid::kCobble:
    case bid::kBrick:
    case bid::kBookshelf:
    case bid::kLog:
    case bid::kJukebox:
      return 2.0f;
    default:
      break;
  }
  switch (id) {
    case bid::kPumpkinLantern:
      return 1.0f;
    case bid::kGoldOre:
    case bid::kIronOre:
    case bid::kCoalOre:
    case bid::kLapisOre:
    case bid::kDiamondOre:
    case bid::kRedstoneOre:
    case bid::kRedstoneOreGlit:
    case bid::kDoorWood:
    case bid::kSignPost:
    case bid::kSignWall:
    case bid::kRail:
    case bid::kTrapDoor:
    case bid::kStoneBrick:
    case bid::kWhiteStone:
    case bid::kDragonEgg:
    case bid::kGoldBlock:
    case bid::kLapisBlock:
      return 3.0f;
    case bid::kBedrock:
    case bid::kPortal:
    case bid::kEndPortal:
    case bid::kEndFrame:
    case bid::kPistonMoving:
      return -1.0f;
    case bid::kWaterMoving:
    case bid::kWaterStill:
    case bid::kLavaStill:
      return 100.0f;
    case bid::kSteelBlock:
    case bid::kMobSpawner:
    case bid::kEnchantTable:
    case bid::kPaneIron:
    case bid::kDoorSteel:
      return 5.0f;
    case bid::kWeb:
      return 4.0f;
    case bid::kDispenser:
      return 3.5f;
    case bid::kFurnaceIdle:
    case bid::kFurnaceBurn:
      return 3.5f;
    case bid::kNoteBlock:
    case bid::kWool:
    case bid::kSandstone:
      return 0.8f;
    case bid::kRailPowered:
    case bid::kRailDetector:
      return 0.7f;
    case bid::kBed:
    case bid::kLeaves:
    case bid::kSnowBlock:
    case bid::kMushroomCapBrown:
    case bid::kMushroomCapRed:
    case bid::kVine:
    case bid::kFenceGate:
      return 0.2f;
    case bid::kPistonSticky:
    case bid::kPistonBase:
    case bid::kPistonExt:
    case bid::kPlateStone:
    case bid::kPlateWood:
    case bid::kBrewingStand:
    case bid::kCake:
    case bid::kIce:
      return 0.5f;
    case bid::kLadder:
    case bid::kCactus:
    case bid::kNetherrack:
      return 0.4f;
    case bid::kGlass:
    case bid::kPaneGlass:
    case bid::kGlowstone:
      return 0.3f;
    case bid::kStairsStoneBrick:  // model stoneBrick
      return 1.5f;
    case bid::kPumpkin:
    case bid::kMelon:
      return 1.0f;
    case bid::kSnowCover:
      return 0.1f;
    case bid::kObsidian:
      return 50.0f;
    case bid::kChest:
    case bid::kLockedChest:
      return 2.5f;
    default:
      return 0.0f;
  }
}

bool harvestable_material(int id) { return harvestable_mat(id); }

bool can_harvest(int block_id, int held_item) {
  if (harvestable_mat(block_id)) return true;
  if (held_item < 256) return false;  // hand (or block item): nothing more
  if (is_pickaxe(held_item)) {
    const int level = tool_material(held_item);
    // Transcribed ternary: obsidian needs 3; diamond/gold/iron/redstone
    // blocks+ores need 2/2/1/1/2/2; rock/iron materials always.
    switch (block_id) {
      case bid::kObsidian:
        return level == 3;
      case bid::kDiamondOre:
      case bid::kSteelBlock:
        return level >= 2;
      case bid::kGoldOre:
      case bid::kGoldBlock:
      case bid::kIronOre:
      case bid::kLapisOre:
      case bid::kLapisBlock:
      case bid::kRedstoneOre:
      case bid::kRedstoneOreGlit:
        return level >= (block_id == bid::kIronOre || block_id == bid::kLapisOre ||
                                         block_id == bid::kLapisBlock
                                     ? 1
                                     : 2);
      default:
        return pickaxe_material(block_id);
    }
  }
  if (is_spade(held_item)) {
    return block_id == bid::kSnowCover || block_id == bid::kSnowBlock;
  }
  if (is_sword(held_item)) {
    return block_id == bid::kWeb;
  }
  return false;
}

float str_vs(int item_id, int block_id) {
  if (item_id < 256) return 1.0f;
  if (is_pickaxe(item_id)) {
    return pickaxe_material(block_id) ? tool_efficiency(tool_material(item_id)) : 1.0f;
  }
  if (is_spade(item_id)) {
    return spade_effective(block_id) ? tool_efficiency(tool_material(item_id)) : 1.0f;
  }
  if (is_axe(item_id)) {
    return axe_effective(block_id) ? tool_efficiency(tool_material(item_id)) : 1.0f;
  }
  if (is_sword(item_id)) {
    return block_id == bid::kWeb ? 15.0f : 1.5f;
  }
  return 1.0f;
}

int damage_vs_entity(int item_id) {
  if (item_id < 256) return 1;
  const int mat = tool_material(item_id);
  const int bonus = mat < 0 ? 0 : (mat == 0 ? 0 : mat == 1 ? 1 : mat == 2 ? 2 : mat == 3 ? 3 : 0);
  if (is_sword(item_id)) return 4 + bonus;
  if (is_pickaxe(item_id)) return 2 + bonus;
  if (is_spade(item_id)) return 1 + bonus;
  if (is_axe(item_id)) return 3 + bonus;
  return 1;
}

int item_max_damage(int item_id) {
  if (item_id < 256) return 0;
  const int mat = tool_material(item_id);
  switch (mat) {
    case 0:
      return 59;
    case 1:
      return 131;
    case 2:
      return 250;
    case 3:
      return 1561;
    case 4:
      return 32;
    default:
      break;
  }
  // Bows/fishing/shears/flint&steel/armor have own values (M5 items).
  if (item_id >= 298 && item_id <= 317) {
    // Armor durability = base[slot] * material factor (func_40436_c * 40577_f).
    static const int kBase[4] = {11, 16, 15, 13};
    static const int kFact[5] = {5, 15, 15, 33, 7};  // cloth chain iron diamond gold
    int slot = (item_id - 298) % 4;
    int mat_i = (item_id - 298) / 4;
    if (mat_i == 0) return kBase[slot] * 5;
    if (mat_i == 1) return kBase[slot] * 15;
    if (mat_i == 2) return kBase[slot] * 15;
    if (mat_i == 3) return kBase[slot] * 33;
    return kBase[slot] * 7;
  }
  return 0;
}

int item_max_stack(int item_id) {
  if (item_id < 256) return 64;
  if (tool_material(item_id) >= 0 || is_sword(item_id)) return 1;
  if (item_id >= 298 && item_id <= 317) return 1;  // armor
  return 64;
}

int armor_value(int item_id) {
  if (item_id < 298 || item_id > 317) return 0;
  static const int kRed[5][4] = {
      {1, 3, 2, 1}, {2, 5, 4, 1}, {2, 6, 5, 2}, {3, 8, 6, 3}, {2, 5, 3, 1}};
  return kRed[(item_id - 298) / 4][(item_id - 298) % 4];
}

// ---- placement rules ----

bool soil_for_plants(int below_id) {
  return below_id == bid::kGrass || below_id == bid::kDirt || below_id == bid::kFarmland;
}

bool can_place_at(int id, int x, int y, int z, const BlockView& w) {
  const int below = w.block_id(x, y - 1, z);
  const auto normal = [&](int ax, int ay, int az) {
    return bid::material_opaque(w.block_id(ax, ay, az)) &&
           bid::renders_as_normal(w.block_id(ax, ay, az));
  };
  const auto solid_mat = [&](int ax, int ay, int az) {
    return bid::material_is_solid(w.block_id(ax, ay, az));
  };
  const auto replaceable = [&]() {
    const int cur = w.block_id(x, y, z);
    return cur == 0 || bid::is_ground_cover(cur);
  };
  switch (id) {
    case bid::kButton:
      return normal(x - 1, y, z) || normal(x + 1, y, z) || normal(x, y, z - 1) ||
             normal(x, y, z + 1);
    case bid::kLever:
      return normal(x - 1, y, z) || normal(x + 1, y, z) || normal(x, y, z - 1) ||
             normal(x, y, z + 1) || normal(x, y - 1, z);
    case bid::kLadder:
      return normal(x - 1, y, z) || normal(x + 1, y, z) || normal(x, y, z - 1) ||
             normal(x, y, z + 1);
    case bid::kTorch:
    case bid::kTorchRedIdle:
    case bid::kTorchRedOn: {
      // func_41082_b sides (missing-chunk default true like the source).
      const BlockView* ww = &w;
      auto side = [&](int ax, int ay, int az) {
        const int nid = ww->block_id(ax, ay, az);
        return bid::material_opaque(nid) && bid::renders_as_normal(nid);
      };
      if (side(x - 1, y, z) || side(x + 1, y, z) || side(x, y, z - 1) || side(x, y, z + 1))
        return true;
      const int bid_below = w.block_id(x, y - 1, z);
      return side(x, y - 1, z) || bid_below == bid::kFence || bid_below == bid::kNetherFence;
    }
    case bid::kRail:
    case bid::kRailPowered:
    case bid::kRailDetector:
    case bid::kRepeaterIdle:
    case bid::kRepeaterOn:
      if (!normal(x, y - 1, z)) return false;
      return replaceable();
    case bid::kPlateStone:
    case bid::kPlateWood:
      return normal(x, y - 1, z) || below == bid::kFence;
    case bid::kRedstoneWire:
      return normal(x, y - 1, z);
    case bid::kReed: {
      if (below == bid::kReed) return true;
      if (below != bid::kGrass && below != bid::kDirt && below != bid::kSand) return false;
      return w.block_id(x - 1, y - 1, z) == bid::kWaterMoving ||
             w.block_id(x - 1, y - 1, z) == bid::kWaterStill ||
             w.block_id(x + 1, y - 1, z) == bid::kWaterMoving ||
             w.block_id(x + 1, y - 1, z) == bid::kWaterStill ||
             w.block_id(x, y - 1, z - 1) == bid::kWaterMoving ||
             w.block_id(x, y - 1, z - 1) == bid::kWaterStill ||
             w.block_id(x, y - 1, z + 1) == bid::kWaterMoving ||
             w.block_id(x, y - 1, z + 1) == bid::kWaterStill;
    }
    case bid::kCactus:
    case bid::kCake:
      if (!replaceable()) return false;
      if (id == bid::kCactus) {
        // canBlockStay: no solid N/S/E/W neighbors; below cactus or sand.
        if (solid_mat(x - 1, y, z) || solid_mat(x + 1, y, z) || solid_mat(x, y, z - 1) ||
            solid_mat(x, y, z + 1))
          return false;
        return below == bid::kCactus || below == bid::kSand;
      }
      return solid_mat(x, y - 1, z);
    case bid::kSnowCover: {
      if (below == 0 || !bid::is_opaque(below)) return false;
      return solid_mat(x, y - 1, z);
    }
    case bid::kSapling:
    case bid::kTallGrass:
    case bid::kDeadBush:
    case bid::kFlowerYellow:
    case bid::kFlowerRed:
    case bid::kCrops:
    case bid::kPumpkinStem:
    case bid::kMelonStem:
      return replaceable() && soil_for_plants(below);
    case bid::kMushroomBrown:
    case bid::kMushroomRed:
      // canThisPlantGrowOnThisBlockID (mushroom): mycelium or solid below.
      // (Light level part of canBlockStay handled on ticks/M5.)
      return replaceable() && (below == bid::kMycelium || solid_mat(x, y - 1, z));
    case bid::kNetherWart:
      return replaceable() && below == bid::kSoulSand;
    case bid::kDoorWood:
    case bid::kDoorSteel:
      if (y >= 127) return false;
      return normal(x, y - 1, z) && replaceable() &&
             (w.block_id(x, y + 1, z) == 0 || bid::is_ground_cover(w.block_id(x, y + 1, z)));
    case bid::kFenceGate:
      if (!solid_mat(x, y - 1, z)) return false;
      return replaceable();
    case bid::kPumpkin:
    case bid::kPumpkinLantern: {
      const int cur = w.block_id(x, y, z);
      return (cur == 0 || bid::is_ground_cover(cur)) && normal(x, y - 1, z);
    }
    case bid::kFire:
      // canPlaceBlockAt: normal below OR flammable neighbors (func_263_h:
      // any N/S/E/W/U/D neighbor with burnable material). M4: full rule.
      if (normal(x, y - 1, z)) return true;
      for (int d = 0; d < 6; ++d) {
        const int nx[6] = {x - 1, x + 1, x, x, x, x};
        const int ny[6] = {y, y, y - 1, y + 1, y, y};
        const int nz[6] = {z, z, z, z, z - 1, z + 1};
        const int nid = w.block_id(nx[d], ny[d], nz[d]);
        // Material.getCanBurn: wood/leaves/cloth/tnt/vine families.
        switch (nid) {
          case bid::kLog:
          case bid::kLeaves:
          case bid::kWoodPlank:
          case bid::kTnt:
          case bid::kWool:
          case bid::kVine:
          case bid::kBookshelf:
            return true;
          default:
            break;
        }
      }
      return false;
    case bid::kLockedChest:
      return true;
    case bid::kChest: {
      // No adjacent chest on more than one side (double-chest rule).
      int n = 0;
      if (w.block_id(x - 1, y, z) == id) ++n;
      if (w.block_id(x + 1, y, z) == id) ++n;
      if (w.block_id(x, y, z - 1) == id) ++n;
      if (w.block_id(x, y, z + 1) == id) ++n;
      return n <= 1 && replaceable();
    }
    case bid::kPistonExt:
    case bid::kPistonMoving:
    case bid::kPortal:
    case bid::kEndPortal:
      return false;
    default:
      return replaceable();
  }
}

bool can_place_on_side(int id, int x, int y, int z, int side, const BlockView& w) {
  const auto normal = [&](int ax, int ay, int az) {
    return bid::material_opaque(w.block_id(ax, ay, az)) &&
           bid::renders_as_normal(w.block_id(ax, ay, az));
  };
  switch (id) {
    case bid::kButton:
      if (side == 2) return normal(x, y, z + 1);
      if (side == 3) return normal(x, y, z - 1);
      if (side == 4) return normal(x + 1, y, z);
      if (side == 5) return normal(x - 1, y, z);
      return false;
    case bid::kLever:
      if (side == 1) return normal(x, y - 1, z);
      if (side == 2) return normal(x, y, z + 1);
      if (side == 3) return normal(x, y, z - 1);
      if (side == 4) return normal(x + 1, y, z);
      if (side == 5) return normal(x - 1, y, z);
      return false;
    case bid::kPistonExt:
    case bid::kPistonMoving:
      return false;
    case bid::kTrapDoor: {
      if (side == 0 || side == 1) return false;
      int mx = x, my = y, mz = z;
      if (side == 2) ++mz;
      if (side == 3) --mz;
      if (side == 4) ++mx;
      if (side == 5) --mx;
      const int mid = w.block_id(mx, my, mz);
      if (mid <= 0) return false;
      return (bid::material_opaque(mid) && bid::renders_as_normal(mid)) ||
             mid == bid::kGlowstone;
    }
    case bid::kVine: {
      // canBePlacedOn: opaque full normal cube.
      auto hold = [&](int ax, int ay, int az) {
        const int nid = w.block_id(ax, ay, az);
        return bid::material_opaque(nid) && bid::renders_as_normal(nid);
      };
      switch (side) {
        case 1:
          return hold(x, y + 1, z);
        case 2:
          return hold(x, y, z + 1);
        case 3:
          return hold(x, y, z - 1);
        case 4:
          return hold(x + 1, y, z);
        case 5:
          return hold(x - 1, y, z);
        default:
          return false;
      }
    }
    default:
      return can_place_at(id, x, y, z, w);
  }
}

bool be_placed_at(EditWorld& w, BlockCollider& collider, int id, int x, int y, int z, int side) {
  // Sticky single path with the CURRENT meta at target, like the source.
  if (auto box = collider.collision_box(id, w.block_meta(x, y, z), x, y, z, w)) {
    if (w.entities_prevent_place(*box)) return false;
  }
  int cur = w.block_id(x, y, z);
  if (cur == bid::kWaterMoving || cur == bid::kWaterStill || cur == bid::kLavaMoving ||
      cur == bid::kLavaStill || cur == bid::kFire || cur == bid::kSnowCover || cur == bid::kVine)
    cur = 0;  // var8 = null
  if (id <= 0 || cur != 0) return false;
  return can_place_on_side(id, x, y, z, side, w);
}

// ---- editing pipeline ----

void set_and_notify(EditWorld& w, int x, int y, int z, int id, int meta) {
  w.set_raw(x, y, z, id, meta);
  // Light updates are M5; notify dispatch below mirrors the rest.
  if (!w.editing_blocks) notify_neighbors(w, x, y, z, id);
}

void set_meta_notify(EditWorld& w, int x, int y, int z, int meta) {
  const int id = w.block_id(x, y, z);
  w.set_raw(x, y, z, id, meta);
  if (!w.editing_blocks) notify_neighbors(w, x, y, z, id);
}

void break_to_air(EditWorld& w, int x, int y, int z) {
  w.set_raw(x, y, z, 0, 0);
  if (!w.editing_blocks) notify_neighbors(w, x, y, z, 0);
}

void notify_neighbors(EditWorld& w, int x, int y, int z, int changed_id) {
  if (w.editing_blocks) return;
  on_neighbor(w, w.block_id(x - 1, y, z), x - 1, y, z, changed_id);
  on_neighbor(w, w.block_id(x + 1, y, z), x + 1, y, z, changed_id);
  on_neighbor(w, w.block_id(x, y - 1, z), x, y - 1, z, changed_id);
  on_neighbor(w, w.block_id(x, y + 1, z), x, y + 1, z, changed_id);
  on_neighbor(w, w.block_id(x, y, z - 1), x, y, z - 1, changed_id);
  on_neighbor(w, w.block_id(x, y, z + 1), x, y, z + 1, changed_id);
}

void pop_with_drop(EditWorld& w, int x, int y, int z) {
  const int id = w.block_id(x, y, z);
  const int meta = w.block_meta(x, y, z);
  drop_as_item(w, x, y, z, id, meta, 0);
  break_to_air(w, x, y, z);
}

void on_neighbor(EditWorld& w, int id, int x, int y, int z, int from_id) {
  (void)from_id;  // only redstone/power logic reads it (M5)
  const auto normal = [&](int ax, int ay, int az) {
    return bid::material_opaque(w.block_id(ax, ay, az)) &&
           bid::renders_as_normal(w.block_id(ax, ay, az));
  };
  switch (id) {
    case bid::kTorch:
    case bid::kTorchRedIdle:
    case bid::kTorchRedOn: {
      // dropTorchIfCantStay + mount-face recheck (transcribed).
      if (!can_place_at(id, x, y, z, w)) {
        if (w.block_id(x, y, z) == id) pop_with_drop(w, x, y, z);
        return;
      }
      const int meta = w.block_meta(x, y, z);
      bool bad = false;
      if (!w.solid_side(x - 1, y, z, true) && meta == 1) bad = true;
      if (!w.solid_side(x + 1, y, z, true) && meta == 2) bad = true;
      if (!w.solid_side(x, y, z - 1, true) && meta == 3) bad = true;
      if (!w.solid_side(x, y, z + 1, true) && meta == 4) bad = true;
      const int below = w.block_id(x, y - 1, z);
      const bool hang_ok = w.solid_side(x, y - 1, z, true) || below == bid::kFence ||
                           below == bid::kNetherFence;
      if (!hang_ok && meta == 5) bad = true;
      if (bad) pop_with_drop(w, x, y, z);
      return;
    }
    case bid::kLadder: {
      const int meta = w.block_meta(x, y, z);
      bool ok = false;
      if (meta == 2 && normal(x, y, z + 1)) ok = true;
      if (meta == 3 && normal(x, y, z - 1)) ok = true;
      if (meta == 4 && normal(x + 1, y, z)) ok = true;
      if (meta == 5 && normal(x - 1, y, z)) ok = true;
      if (!ok) pop_with_drop(w, x, y, z);
      return;
    }
    case bid::kButton:
    case bid::kLever:
      if (!can_place_at(id, x, y, z, w)) pop_with_drop(w, x, y, z);
      return;  // power switching is M5
    case bid::kPlateStone:
    case bid::kPlateWood:
      if (!normal(x, y - 1, z) && w.block_id(x, y - 1, z) != bid::kFence)
        pop_with_drop(w, x, y, z);
      return;  // press logic is M5
    case bid::kReed:
      if (!can_place_at(id, x, y, z, w)) pop_with_drop(w, x, y, z);
      return;
    case bid::kCactus:
      if (!can_place_at(id, x, y, z, w)) pop_with_drop(w, x, y, z);
      return;
    case bid::kSnowCover:
      if (!can_place_at(id, x, y, z, w)) pop_with_drop(w, x, y, z);
      return;
    case bid::kVine: {
      // canVineStay: any flagged face still mounted (M4: meta-bit version).
      const int meta = w.block_meta(x, y, z);
      bool stay = false;
      if ((meta & 2) && normal(x, y, z + 1)) stay = true;
      if ((meta & 8) && normal(x, y, z - 1)) stay = true;
      if ((meta & 4) && normal(x + 1, y, z)) stay = true;
      if ((meta & 1) && normal(x - 1, y, z)) stay = true;
      if (!stay) pop_with_drop(w, x, y, z);
      return;
    }
    case bid::kCake:
      if (!bid::material_is_solid(w.block_id(x, y - 1, z))) pop_with_drop(w, x, y, z);
      return;
    case bid::kDoorWood:
    case bid::kDoorSteel: {
      // Half-break propagation (power part is M5).
      const int meta = w.block_meta(x, y, z);
      if ((meta & 8) != 0) {
        if (w.block_id(x, y - 1, z) != id) break_to_air(w, x, y, z);
      } else {
        bool gone = false;
        if (w.block_id(x, y + 1, z) != id) {
          break_to_air(w, x, y, z);
          gone = true;
        }
        if (!gone && !normal(x, y - 1, z)) {
          break_to_air(w, x, y, z);
          if (w.block_id(x, y + 1, z) == id) break_to_air(w, x, y + 1, z);
        }
      }
      return;
    }
    case bid::kTrapDoor: {
      // Mount recheck (power switching is M5).
      const int meta = w.block_meta(x, y, z);
      int mx = x, my = y, mz = z;
      const int dir = meta & 3;
      // Closed trapdoors rest on their frame; open ones hang on the mount.
      // The source re-derives the mount from orientation; approximate with
      // the placement mount rule (opaque/glow neighbor exists).
      bool mount = false;
      for (int d = 0; d < 6; ++d) {
        static const int kOX[6] = {-1, 1, 0, 0, 0, 0};
        static const int kOY[6] = {0, 0, -1, 1, 0, 0};
        static const int kOZ[6] = {0, 0, 0, 0, -1, 1};
        const int nid = w.block_id(x + kOX[d], y + kOY[d], z + kOZ[d]);
        if (nid > 0 && ((bid::material_opaque(nid) && bid::renders_as_normal(nid)) ||
                        nid == bid::kGlowstone)) {
          mount = true;
          break;
        }
      }
      (void)mx;
      (void)my;
      (void)mz;
      (void)dir;
      if (!mount) pop_with_drop(w, x, y, z);
      return;
    }
    case bid::kRail:
    case bid::kRailPowered:
    case bid::kRailDetector:
      // Track-shape refresh is M5; pop when unsupported (matches the
      // support half of refreshTrackShape).
      if (!normal(x, y - 1, z)) pop_with_drop(w, x, y, z);
      return;
    default:
      return;  // default blocks ignore neighbor change (redstone M5)
  }
}

// ---- drops (harvestBlock + dropBlockAsItem*) ----

void drop_one(EditWorld& w, int x, int y, int z, int item_id, int count, int damage) {
  JavaRandom& r = w.world_rand();
  constexpr float kSpread = 0.7f;
  const double px = x + r.next_float() * kSpread + (1.0f - kSpread) * 0.5;
  const double py = y + r.next_float() * kSpread + (1.0f - kSpread) * 0.5;
  const double pz = z + r.next_float() * kSpread + (1.0f - kSpread) * 0.5;
  // EntityItem motion/yaw use global Math.random (wild in the source too);
  // the hook records zeros there (position is the deterministic part).
  w.on_item_drop(item_id, count, damage, px, py, pz, 0.0, 0.0, 0.0);
}

// Block id dropped (idDropped), -1/0 = nothing.
int drop_id(int id, int meta, JavaRandom& r, int fortune) {
  (void)fortune;  // M4 tests use fortune 0; formulas below are fortune-0 exact
  using bid::kAir;
  switch (id) {
    case bid::kBed:
      return (meta & 8) != 0 ? 0 : 355;  // bed item
    case bid::kBookshelf:
      return 340;  // book
    case bid::kBrewingStand:
      return 379;
    case bid::kCake:
      return 0;
    default:
      break;
  }
  switch (id) {
    case bid::kWaterMoving:
    case bid::kWaterStill:
    case bid::kLavaMoving:
    case bid::kLavaStill:
    case bid::kMobSpawner:
    case bid::kEndPortal:
    case bid::kFire:
    case bid::kPortal:
    case bid::kPistonExt:
    case bid::kPistonMoving:
    case bid::kVine:
    case bid::kNetherWart:
    case bid::kGlass:
    case bid::kIce:
      return 0;
    case bid::kCauldron:
      return 380;
    case bid::kClay:
      return 337;
    case bid::kCrops:
      return meta == 7 ? 296 : -1;  // wheat
    case bid::kDeadBush:
    case bid::kPumpkinStem:
    case bid::kMelonStem:
      return -1;
    default:
      break;
  }
  switch (id) {
    case bid::kDeadBush:
    case bid::kPumpkinStem:
    case bid::kMelonStem:
      return -1;  // (kept; unreachable duplicate guard removed below)
    case bid::kDoorWood:
      return (meta & 8) != 0 ? 0 : 324;
    case bid::kDoorSteel:
      return (meta & 8) != 0 ? 0 : 330;
    case bid::kFarmland:
    case bid::kGrass:
    case bid::kMycelium:
      return bid::kDirt;
    case bid::kGravel:
      return r.next_int(10) == 0 ? 318 : bid::kGravel;  // flint 1/10
    case bid::kLeaves:
      return bid::kSapling;
    case bid::kLog:
      return bid::kLog;
    case bid::kMelon:
      return 360;
    case bid::kMushroomCapBrown:
      return bid::kMushroomBrown;
    case bid::kMushroomCapRed:
      return bid::kMushroomRed;
    case bid::kGoldOre:  // BlockOre
      return 263;        // coal
    case bid::kDiamondOre:
      return 264;  // diamond
    case bid::kLapisOre:
      return 351;  // dye
    case bid::kIronOre:
    case bid::kCoalOre:
    case bid::kRedstoneOreGlit:
      return id;
    case bid::kRedstoneOre:
      return 331;
    case bid::kPaneGlass:
      return 0;  // thin glass shatters (iron pane drops itself)
    case bid::kPaneIron:
      return bid::kPaneIron;
    case bid::kReed:
      return 338;
    case bid::kSignPost:
    case bid::kSignWall:
      return 323;
    case bid::kSnowCover:
      return 332;  // snowball
    case bid::kStepDouble:
    case bid::kStepSingle:
      return bid::kStepSingle;
    case bid::kStone:
      return bid::kCobble;
    case bid::kTallGrass:
      return r.next_int(8) == 0 ? 295 : -1;  // seeds 1/8
    case bid::kWeb:
      return 287;  // silk
    case bid::kRedstoneWire:
      return 331;
    case bid::kRepeaterIdle:
    case bid::kRepeaterOn:
      return 356;
    case bid::kTorchRedIdle:
    case bid::kTorchRedOn:
      return bid::kTorchRedOn;
    case bid::kFurnaceBurn:
      return bid::kFurnaceIdle;
    default:
      return id;  // base idDropped: the block itself
  }
}

int drop_damage(int id, int meta) {
  switch (id) {
    case bid::kWool:
      return meta;
    case bid::kLeaves:
      return meta & 3;
    case bid::kStepDouble:
    case bid::kStepSingle:
      return meta;
    case bid::kLapisOre:
      return 4;
    default:
      return 0;
  }
}

int drop_count(int id, JavaRandom& r) {
  switch (id) {
    case bid::kClay:
      return 4;
    case bid::kMelon:
      return 3 + r.next_int(5);
    case bid::kGlowstone:
      return 2 + r.next_int(3);
    case bid::kRedstoneOre:
    case bid::kRedstoneOreGlit:
      return 4 + r.next_int(2);
    case bid::kLapisOre:
      return 4 + r.next_int(5);
    case bid::kSnowCover:
      return 0;
    case bid::kLeaves:
      return r.next_int(20) == 0 ? 1 : 0;
    case bid::kBookshelf:
      return 3;
    case bid::kCake:
    case bid::kGlass:
    case bid::kIce:
    case bid::kMobSpawner:
    case bid::kEndPortal:
    case bid::kFire:
    case bid::kPortal:
    case bid::kPistonExt:
    case bid::kPistonMoving:
    case bid::kVine:
    case bid::kNetherWart:
    case bid::kTnt:
    case bid::kSilverfish:
    case bid::kWaterMoving:
    case bid::kWaterStill:
    case bid::kLavaMoving:
    case bid::kLavaStill:
      return 0;
    case bid::kStepDouble:
      return 2;
    default:
      return 1;
  }
}

void drop_as_item(EditWorld& w, int x, int y, int z, int block_id, int meta, int fortune) {
  JavaRandom& r = w.world_rand();
  // func_40198_a (fortune-0 exact; fortune formulas are M5).
  int n = drop_count(block_id, r);
  (void)fortune;
  for (int i = 0; i < n; ++i) {
    if (r.next_float() > 1.0f) continue;
    const int drop = drop_id(block_id, meta, r, fortune);
    if (drop > 0) drop_one(w, x, y, z, drop, 1, drop_damage(block_id, meta));
  }
}

void harvest_block(EditWorld& w, Breaker& br, int x, int y, int z, int meta) {
  const int id = w.block_id(x, y, z);
  br.add_stat(2000000 + id, 1);  // mineBlockStatArray slot (stat values M5)
  br.add_exhaustion(0.025f);
  // Silk touch / fortune need enchanted inventory (M5): plain drops here.
  // TallGrass shears shortcut (shears item 359).
  drop_as_item(w, x, y, z, id, meta, 0);
}

float block_strength(int id, int held_item) {
  const float h = hardness(id);
  if (h < 0.0f) return 0.0f;
  if (!can_harvest(id, held_item)) return 1.0f / h / 100.0f;
  return str_vs(held_item, id) / h / 30.0f;
}

int placed_meta(int item_id, int item_damage) {
  (void)item_id;
  return item_damage;  // ItemBlock.getPlacedBlockMetadata (slab/door types M5)
}

// onBlockPlaced: side -> metadata orientation for mounted blocks.
void on_placed(EditWorld& w, int id, int x, int y, int z, int side) {
  const auto normal = [&](int ax, int ay, int az) {
    return bid::material_opaque(w.block_id(ax, ay, az)) &&
           bid::renders_as_normal(w.block_id(ax, ay, az));
  };
  int meta = w.block_meta(x, y, z);
  switch (id) {
    case bid::kTorch:
    case bid::kTorchRedIdle:
    case bid::kTorchRedOn:
      if (side == 1 && w.solid_side(x, y - 1, z, true)) meta = 5;
      if (side == 2 && w.solid_side(x, y, z + 1, true)) meta = 4;
      if (side == 3 && w.solid_side(x, y, z - 1, true)) meta = 3;
      if (side == 4 && w.solid_side(x + 1, y, z, true)) meta = 2;
      if (side == 5 && w.solid_side(x - 1, y, z, true)) meta = 1;
      set_meta_notify(w, x, y, z, meta);
      return;
    case bid::kLadder:
      if ((meta == 0 || side == 2) && normal(x, y, z + 1)) meta = 2;
      if ((meta == 0 || side == 3) && normal(x, y, z - 1)) meta = 3;
      if ((meta == 0 || side == 4) && normal(x + 1, y, z)) meta = 4;
      if ((meta == 0 || side == 5) && normal(x - 1, y, z)) meta = 5;
      set_meta_notify(w, x, y, z, meta);
      return;
    case bid::kButton: {
      const int armed = meta & 8;
      if (side == 2 && normal(x, y, z + 1)) meta = 4 + armed;
      if (side == 3 && normal(x, y, z - 1)) meta = 3 + armed;
      if (side == 4 && normal(x + 1, y, z)) meta = 2 + armed;
      if (side == 5 && normal(x - 1, y, z)) meta = 1 + armed;
      set_meta_notify(w, x, y, z, meta);
      return;
    }
    case bid::kLever: {
      const int armed = meta & 8;
      if (side == 1 && normal(x, y - 1, z)) meta = 5 + armed;
      if (side == 2 && normal(x, y, z + 1)) meta = 4 + armed;
      if (side == 3 && normal(x, y, z - 1)) meta = 3 + armed;
      if (side == 4 && normal(x + 1, y, z)) meta = 2 + armed;
      if (side == 5 && normal(x - 1, y, z)) meta = 1 + armed;
      set_meta_notify(w, x, y, z, meta);
      return;
    }
    default:
      return;
  }
}

// onBlockPlacedBy: player-yaw orientation for stairs/furnace/pumpkin/dispenser.
void on_placed_by(EditWorld& w, int id, int x, int y, int z, float yaw) {
  const double q = static_cast<double>(yaw * 4.0f / 360.0f) + 0.5;
  const int facing = MathHelper::floor_double(q);
  switch (id) {
    case bid::kStairsWood:
    case bid::kStairsCobble:
    case bid::kStairsBrick:
    case bid::kStairsStoneBrick:
    case bid::kStairsNether:
      if (facing == 0) set_meta_notify(w, x, y, z, 2);
      if (facing == 1) set_meta_notify(w, x, y, z, 1);
      if (facing == 2) set_meta_notify(w, x, y, z, 3);
      if (facing == 3) set_meta_notify(w, x, y, z, 0);
      return;
    case bid::kFurnaceIdle:
    case bid::kFurnaceBurn:
    case bid::kDispenser:
      if (facing == 0) set_meta_notify(w, x, y, z, 2);
      if (facing == 1) set_meta_notify(w, x, y, z, 5);
      if (facing == 2) set_meta_notify(w, x, y, z, 3);
      if (facing == 3) set_meta_notify(w, x, y, z, 4);
      return;
    case bid::kPumpkin:
    case bid::kPumpkinLantern: {
      const double q2 = static_cast<double>(yaw * 4.0f / 360.0f) + 2.5;
      set_meta_notify(w, x, y, z, MathHelper::floor_double(q2) & 3);
      return;
    }
    default:
      return;
  }
}

bool use_block_item(EditWorld& w, BlockCollider& collider, Breaker& br, int& stack_size,
                    int item_id, int item_damage, int x, int y, int z, int side) {
  int tx = x, ty = y, tz = z;
  const int target = w.block_id(x, y, z);
  if (target == bid::kSnowCover) {
    side = 0;
  } else if (target != bid::kVine) {
    if (side == 0) --ty;
    if (side == 1) ++ty;
    if (side == 2) --tz;
    if (side == 3) ++tz;
    if (side == 4) --tx;
    if (side == 5) ++tx;
  }
  if (stack_size == 0) return false;
  if (!br.can_mine(tx, ty, tz)) return false;
  if (ty == 127 && bid::material_is_solid(item_id)) return false;
  if (!be_placed_at(w, collider, item_id, tx, ty, tz, side)) return false;
  // setBlockAndMetadataWithNotify + hooks.
  w.set_raw(tx, ty, tz, item_id, placed_meta(item_id, item_damage));
  if (!w.editing_blocks) notify_neighbors(w, tx, ty, tz, item_id);
  if (w.block_id(tx, ty, tz) == item_id) {
    on_placed(w, item_id, tx, ty, tz, side);
    on_placed_by(w, item_id, tx, ty, tz, br.yaw());
  }
  // StepSound place sound (M6 audio): volume/pitch computed, hook no-op.
  w.play_place_sound("step", tx + 0.5, ty + 0.5, tz + 0.5, 1.0f, 1.0f);
  --stack_size;
  return true;
}

bool use_door_item(EditWorld& w, BlockCollider& collider, Breaker& br, int& stack_size,
                   int item_id, int x, int y, int z, int side) {
  if (side != 1) return false;
  ++y;
  const int door = (item_id == 324) ? bid::kDoorWood : bid::kDoorSteel;
  if (!br.can_mine(x, y, z) || !br.can_mine(x, y + 1, z)) return false;
  if (!can_place_at(door, x, y, z, w)) return false;
  const double q = static_cast<double>(br.yaw() + 180.0f) * 4.0 / 360.0 - 0.5;
  int facing = MathHelper::floor_double(q) & 3;
  const auto normal = [&](int ax, int ay, int az) {
    return bid::material_opaque(w.block_id(ax, ay, az)) &&
           bid::renders_as_normal(w.block_id(ax, ay, az));
  };
  int dx = 0, dz = 0;
  if (facing == 0) dz = 1;
  if (facing == 1) dx = -1;
  if (facing == 2) dz = -1;
  if (facing == 3) dx = 1;
  const int left = (normal(x - dx, y, z - dz) ? 1 : 0) + (normal(x - dx, y + 1, z - dz) ? 1 : 0);
  const int right = (normal(x + dx, y, z + dz) ? 1 : 0) + (normal(x + dx, y + 1, z + dz) ? 1 : 0);
  const bool door_l = w.block_id(x - dx, y, z - dz) == door ||
                      w.block_id(x - dx, y + 1, z - dz) == door;
  const bool door_r = w.block_id(x + dx, y, z + dz) == door ||
                      w.block_id(x + dx, y + 1, z + dz) == door;
  bool hinge = false;
  if ((door_l && !door_r) || right > left) hinge = true;
  if (hinge) {
    facing = facing - 1 & 3;
    facing += 4;
  }
  const bool was_editing = w.editing_blocks;
  w.editing_blocks = true;
  w.set_raw(x, y, z, door, facing);
  w.set_raw(x, y + 1, z, door, facing + 8);
  w.editing_blocks = was_editing;
  notify_neighbors(w, x, y, z, door);
  notify_neighbors(w, x, y + 1, z, door);
  --stack_size;
  (void)collider;
  return true;
}

}  // namespace craftpp::world::edit
