#pragma once

namespace craftpp::world::bid {

// Raw block ids mirroring Block.java. Worldgen works on raw ids (like the
// source byte arrays); the typed BlockId registry grows with M4.
inline constexpr int kAir = 0;
inline constexpr int kStone = 1;
inline constexpr int kGrass = 2;
inline constexpr int kDirt = 3;
inline constexpr int kCobble = 4;
inline constexpr int kWoodPlank = 5;
inline constexpr int kBedrock = 7;inline constexpr int kWaterMoving = 8;
inline constexpr int kWaterStill = 9;
inline constexpr int kLavaMoving = 10;
inline constexpr int kLavaStill = 11;
inline constexpr int kSand = 12;
inline constexpr int kGravel = 13;
inline constexpr int kGoldOre = 14;
inline constexpr int kIronOre = 15;
inline constexpr int kCoalOre = 16;
inline constexpr int kLog = 17;
inline constexpr int kLeaves = 18;
inline constexpr int kLapisOre = 21;
inline constexpr int kSandstone = 24;
inline constexpr int kTallGrass = 31;
inline constexpr int kDeadBush = 32;
inline constexpr int kWool = 35;
inline constexpr int kFlowerYellow = 37;
inline constexpr int kFlowerRed = 38;
inline constexpr int kMushroomBrown = 39;
inline constexpr int kMushroomRed = 40;
inline constexpr int kCobbleMossy = 48;
inline constexpr int kObsidian = 49;
inline constexpr int kFire = 51;
inline constexpr int kChest = 54;
inline constexpr int kDiamondOre = 56;
inline constexpr int kDiamondBlock = 57;
inline constexpr int kTnt = 46;inline constexpr int kFarmland = 60;
inline constexpr int kSignPost = 63;
inline constexpr int kDoorWood = 64;
inline constexpr int kLadder = 65;
inline constexpr int kDoorSteel = 71;
inline constexpr int kMobSpawner = 52;
inline constexpr int kRedstoneOre = 73;
inline constexpr int kSnowCover = 78;
inline constexpr int kIce = 79;
inline constexpr int kCactus = 81;
inline constexpr int kClay = 82;
inline constexpr int kReed = 83;
inline constexpr int kPumpkin = 86;
inline constexpr int kPortal = 90;
inline constexpr int kGlass = 20;
inline constexpr int kTorch = 50;
inline constexpr int kMushroomCapBrown = 99;
inline constexpr int kMushroomCapRed = 100;
inline constexpr int kVine = 106;
inline constexpr int kMycelium = 110;
inline constexpr int kLilyPad = 111;
// M4: full id table (mirrors Block.java static fields).
inline constexpr int kSapling = 6;
inline constexpr int kSponge = 19;
inline constexpr int kLapisBlock = 22;
inline constexpr int kDispenser = 23;
inline constexpr int kNoteBlock = 25;
inline constexpr int kBed = 26;
inline constexpr int kRailPowered = 27;
inline constexpr int kRailDetector = 28;
inline constexpr int kPistonSticky = 29;
inline constexpr int kWeb = 30;
inline constexpr int kPistonBase = 33;
inline constexpr int kPistonExt = 34;
inline constexpr int kPistonMoving = 36;
inline constexpr int kGoldBlock = 41;
inline constexpr int kSteelBlock = 42;
inline constexpr int kStepDouble = 43;
inline constexpr int kStepSingle = 44;
inline constexpr int kBrick = 45;
inline constexpr int kBookshelf = 47;
inline constexpr int kStairsWood = 53;
inline constexpr int kRedstoneWire = 55;
inline constexpr int kWorkbench = 58;
inline constexpr int kCrops = 59;
inline constexpr int kFurnaceIdle = 61;
inline constexpr int kFurnaceBurn = 62;
inline constexpr int kRail = 66;
inline constexpr int kStairsCobble = 67;
inline constexpr int kSignWall = 68;
inline constexpr int kLever = 69;
inline constexpr int kPlateStone = 70;
inline constexpr int kPlateWood = 72;
inline constexpr int kRedstoneOreGlit = 74;
inline constexpr int kTorchRedIdle = 75;
inline constexpr int kTorchRedOn = 76;
inline constexpr int kButton = 77;
inline constexpr int kSnowBlock = 80;
inline constexpr int kJukebox = 84;
inline constexpr int kFence = 85;
inline constexpr int kNetherrack = 87;
inline constexpr int kSoulSand = 88;
inline constexpr int kGlowstone = 89;
inline constexpr int kPumpkinLantern = 91;
inline constexpr int kCake = 92;
inline constexpr int kRepeaterIdle = 93;
inline constexpr int kRepeaterOn = 94;
inline constexpr int kLockedChest = 95;
inline constexpr int kTrapDoor = 96;
inline constexpr int kSilverfish = 97;
inline constexpr int kStoneBrick = 98;
inline constexpr int kPaneIron = 101;
inline constexpr int kPaneGlass = 102;
inline constexpr int kMelon = 103;
inline constexpr int kPumpkinStem = 104;
inline constexpr int kMelonStem = 105;
inline constexpr int kFenceGate = 107;
inline constexpr int kStairsBrick = 108;
inline constexpr int kStairsStoneBrick = 109;
inline constexpr int kNetherBrick = 112;
inline constexpr int kNetherFence = 113;
inline constexpr int kStairsNether = 114;
inline constexpr int kNetherWart = 115;
inline constexpr int kEnchantTable = 116;
inline constexpr int kBrewingStand = 117;
inline constexpr int kCauldron = 118;
inline constexpr int kEndPortal = 119;
inline constexpr int kEndFrame = 120;
inline constexpr int kWhiteStone = 121;
inline constexpr int kDragonEgg = 122;

enum class Material {
  Solid,      // rock, ground, wood, sand, clay, ores, ice, pumpkin, ...
  Leaves,     // solid to entitiesorous checks but not an occluder
  Water,
  Lava,
  Nonsolid,   // air, plants, vine, circuits, snow cover, fire, web
};

inline Material material_of(int id) {
  switch (id) {
    case kAir:
    case kTallGrass:
    case kDeadBush:
    case kFlowerYellow:
    case kFlowerRed:
    case kMushroomBrown:
    case kMushroomRed:
    case kSnowCover:
    case kReed:
    case kLilyPad:
    case kVine:
    case kFire:
      return Material::Nonsolid;
    case kWaterMoving:
    case kWaterStill:
      return Material::Water;
    case kLavaMoving:
    case kLavaStill:
      return Material::Lava;
    case kLeaves:
      return Material::Leaves;
    default:
      return Material::Solid;
  }
}

inline bool material_solid(int id) {
  const Material m = material_of(id);
  return m == Material::Solid || m == Material::Leaves;
}

inline bool material_liquid(int id) {
  const Material m = material_of(id);
  return m == Material::Water || m == Material::Lava;
}

// Mirrors Block.opaqueCubeLookup (isOpaqueCube): everything solid except
// fluids are handled by material, chest/spawner/vine/plants/snow.
// NOTE: leaves are OPAQUE here (opaqueCubeLookup[18] == !graphicsLevel, false
// by default in headless/server). This matters for worldgen: tree discs skip
// cells that already contain leaves.
inline bool is_opaque(int id) {
  switch (id) {
    case kAir:
    case kWaterMoving:
    case kWaterStill:
    case kLavaMoving:
    case kLavaStill:
    case kTallGrass:
    case kDeadBush:
    case kFlowerYellow:
    case kFlowerRed:
    case kMushroomBrown:
    case kMushroomRed:
    case kChest:
    case kMobSpawner:
    case kSnowCover:
    case kReed:
    case kCactus:
    case kVine:
    case kIce:
    case kGlass:
    case kTorch:
    case kFire:
    case kPortal:
    case kSignPost:
    case kSignWall:
    case kDoorWood:
    case kDoorSteel:
    case kLadder:
    case kRail:
    case kRailPowered:
    case kRailDetector:
    case kLever:
    case kPlateStone:
    case kPlateWood:
    case kTorchRedIdle:
    case kTorchRedOn:
    case kButton:
    case kCake:
    case kRepeaterIdle:
    case kRepeaterOn:
    case kTrapDoor:
    case kPaneIron:
    case kPaneGlass:
    case kPumpkinStem:
    case kMelonStem:
    case kNetherWart:
    case kSapling:
    case kCrops:
    case kRedstoneWire:
    case kWeb:
    case kBed:
    case kPistonSticky:
    case kPistonBase:
    case kPistonExt:
    case kPistonMoving:
    case kStairsWood:
    case kStairsCobble:
    case kStairsBrick:
    case kStairsStoneBrick:
    case kStairsNether:
    case kStepSingle:  // BlockStep.isOpaqueCube = blockType (single=false)
    case kFenceGate:
    case kEnchantTable:
    case kBrewingStand:
    case kCauldron:
    case kEndPortal:
    case kEndFrame:
    case kDragonEgg:
      return false;
    default:
      return true;
  }
}

// Mirrors Block.lightOpacity: leaves 1, web 1, water/ice 3, lava 255,
// everything else opaque (255) or transparent (0).
inline int light_opacity(int id) {
  if (id == kLeaves || id == kWeb) return 1;
  if (id == kWaterMoving || id == kWaterStill || id == kIce) return 3;
  if (id == kLavaMoving || id == kLavaStill) return 255;
  return is_opaque(id) ? 255 : 0;
}

// Mirrors Block.lightValue (int(15 * f)): lava/fire/glowstone/lantern 15,
// torch 14, furnace-on 13, portal 11, redstone-ore/torch 9/7, rest small.
inline int light_value(int id) {
  switch (id) {
    case kLavaMoving:
    case kLavaStill:
    case kFire:
    case kGlowstone:
    case kPumpkinLantern:
    case kLockedChest:
      return 15;
    case kTorch:
      return 14;
    case kFurnaceBurn:
      return 13;
    case kPortal:
      return 11;
    case kRedstoneOreGlit:
      return 9;
    case kRepeaterOn:
      return 9;
    case kTorchRedOn:
      return 7;
    case kMushroomBrown:
    case kMushroomRed:
    case kBrewingStand:
    case kEndFrame:
    case kDragonEgg:
      return 1;
    default:
      return 0;
  }
}

// Mirrors Block.slipperiness (default 0.6; ice 0.98). Nothing else overrides.
inline float block_slipperiness(int id) { return id == kIce ? 0.98f : 0.6f; }

// Material.getIsGroundCover for Block.canPlaceBlockAt: MaterialTransparent
// (air, fire) + MaterialLiquid (water, lava) set it in the constructor,
// plus explicit vine + snow cover. Everything else (leaves, glass, plants,
// circuits, ice, cactus, ...) is NOT ground cover.
inline bool is_ground_cover(int id) {
  return id == kVine || id == kSnowCover || id == kWaterMoving || id == kWaterStill ||
         id == kLavaMoving || id == kLavaStill || id == kFire;
}

// Mirrors Block.getBlockTextureFromSideAndMetadata (+ the facing-aware
// getBlockTexture for furnace/dispenser/pumpkin): terrain.png tile index
// for one cube face. side: 0 bottom, 1 top, 2 -z, 3 +z, 4 -x, 5 +x.
inline int block_texture(int id, int side, int meta) {
  switch (id) {
    case kGrass:
      return side == 1 ? 0 : (side == 0 ? 2 : 3);
    case kLog:
      if (side == 1 || side == 0) return 21;
      return meta == 1 ? 116 : (meta == 2 ? 117 : 20);
    case kLeaves:
      return ((meta & 3) == 1) ? 132 : 52;
    case kFurnaceIdle:
    case kFurnaceBurn: {
      if (side == 1 || side == 0) return 62;
      const int front = (id == kFurnaceBurn) ? 61 : 44;
      return (side == meta && meta >= 2 && meta <= 5) ? front : 45;
    }
    case kDispenser: {
      if (side == 1 || side == 0) return 62;
      return (side == meta && meta >= 2 && meta <= 5) ? 46 : 45;
    }
    case kSandstone:
      return side == 1 ? 176 : (side == 0 ? 208 : 192);
    case kWool:
      if (meta == 0) return 64;
      {
        const int m = ~meta & 15;
        return 113 + ((m & 8) >> 3) + (m & 7) * 16;
      }
    case kStepSingle:
    case kStepDouble:
      switch (meta) {
        case 0:
          return side <= 1 ? 6 : 5;
        case 1:
          return side == 0 ? 208 : (side == 1 ? 176 : 192);
        case 2:
          return 4;
        case 3:
          return 16;
        case 4:
          return 7;
        case 5:
          return 54;
        default:
          return 6;
      }
    case kTnt:
      return side == 0 ? 10 : (side == 1 ? 9 : 8);
    case kBookshelf:
      return side <= 1 ? 4 : 35;
    case kStairsWood:
      return 4;
    case kStairsCobble:
      return 16;
    case kStairsBrick:
      return 7;
    case kStairsStoneBrick:
      return 54;
    case kStairsNether:
      return 224;
    case kWorkbench:
      if (side == 1) return 43;
      if (side == 0) return 4;
      return (side == 2 || side == 4) ? 59 : 60;
    case kFarmland:
      if (side == 1) return meta > 0 ? 86 : 87;
      return 2;
    case kPumpkin:
    case kPumpkinLantern: {
      if (side == 1 || side == 0) return 102;
      if (side == meta && meta >= 2 && meta <= 5) {
        return id == kPumpkinLantern ? 120 : 119;
      }
      return 102;
    }
    case kCake:
      if (side == 1) return 121;
      if (side == 0) return 124;
      return 122;
    case kRepeaterIdle:
    case kRepeaterOn:
      if (side == 0) return id == kRepeaterOn ? 99 : 115;
      if (side == 1) return id == kRepeaterOn ? 147 : 131;
      return 5;
    case kCactus:
      return side == 1 ? 69 : (side == 0 ? 71 : 70);
    case kReed:
      return 73;
    case kJukebox:
      return side == 1 ? 75 : 74;
    case kMelon:
      return (side == 1 || side == 0) ? 137 : 136;
    case kMycelium:
      return side == 1 ? 78 : (side == 0 ? 2 : 77);
    case kCauldron:
      return side == 1 ? 138 : (side == 0 ? 155 : 154);
    case kEndFrame:
      return side == 1 ? 158 : (side == 0 ? 175 : 159);
    case kSapling: {
      const int m = meta & 3;
      return m == 1 ? 63 : (m == 2 ? 79 : 15);
    }
    case kCrops:
      return 88 + (meta < 0 ? 7 : (meta > 7 ? 7 : meta));
    case kWaterMoving:
    case kWaterStill:
      return (side == 0 || side == 1) ? 205 : 206;
    case kLavaMoving:
    case kLavaStill:
      return (side == 0 || side == 1) ? 237 : 238;
    default:
      break;
  }
  // Constructor base indices (Block.java registration order).
  switch (id) {
    case kStone:
      return 1;
    case kDirt:
      return 2;
    case kCobble:
      return 16;
    case kWoodPlank:
      return 4;
    case kBedrock:
      return 17;
    case kSand:
      return 18;
    case kGravel:
      return 19;
    case kGoldOre:
      return 32;
    case kIronOre:
      return 33;
    case kCoalOre:
      return 34;
    case kSponge:
      return 48;
    case kGlass:
      return 49;
    case kLapisOre:
      return 160;
    case kLapisBlock:
      return 144;
    case kDiamondOre:
      return 50;
    case kDiamondBlock:
      return 24;
    case kGoldBlock:
      return 23;
    case kSteelBlock:
      return 22;
    case kBrick:
      return 7;
    case kCobbleMossy:
      return 36;
    case kObsidian:
      return 37;
    case kTorch:
      return 80;
    case kFire:
      return 31;
    case kMobSpawner:
      return 65;
    case kChest:
    case kLockedChest:
      return 26;
    case kRedstoneWire:
      return 164;
    case kSignPost:
    case kSignWall:
      return 4;
    case kDoorWood:
      return 97;
    case kDoorSteel:
      return 98;
    case kLadder:
      return 83;
    case kRail:
      return 128;
    case kRailPowered:
      return ((meta & 8) == 0) ? 179 : 163;
    case kRailDetector:
      return 195;
    case kWeb:
      return 11;
    case kTallGrass:
      return 39;
    case kDeadBush:
      return 55;
    case kPistonSticky:
    case kPistonBase:
      return side <= 1 ? 106 : 107;
    case kPistonExt:
      return 107;
    case kFlowerYellow:
      return 13;
    case kFlowerRed:
      return 12;
    case kMushroomBrown:
      return 29;
    case kMushroomRed:
      return 28;
    case kLever:
      return 96;
    case kPlateStone:
      return 1;
    case kPlateWood:
      return 4;
    case kRedstoneOre:
    case kRedstoneOreGlit:
      return 51;
    case kTorchRedIdle:
      return 115;
    case kTorchRedOn:
      return 99;
    case kButton:
      return 1;
    case kSnowCover:
      return 66;
    case kIce:
      return 67;
    case kSnowBlock:
      return 66;
    case kClay:
      return 72;
    case kNetherrack:
      return 103;
    case kSoulSand:
      return 104;
    case kGlowstone:
      return 105;
    case kPortal:
      return 14;
    case kFence:
    case kFenceGate:
      return 4;
    case kNetherFence:
      return 224;
    case kTrapDoor:
      return 84;
    case kSilverfish:
      return meta == 1 ? 16 : (meta == 2 ? 54 : 1);
    case kStoneBrick:
      return 54;
    case kMushroomCapBrown:
    case kMushroomCapRed:
      return 142;
    case kPaneIron:
      return 85;
    case kPaneGlass:
      return 49;
    case kPumpkinStem:
    case kMelonStem:
      return 111;
    case kVine:
      return 143;
    case kLilyPad:
      return 76;
    case kNetherBrick:
      return 224;
    case kNetherWart:
      return meta >= 3 ? 228 : (meta > 0 ? 227 : 226);
    case kEnchantTable:
      return 166;
    case kBrewingStand:
      return 157;
    case kEndPortal:
      return 14;
    case kWhiteStone:
      return 175;
    case kDragonEgg:
      return 167;
    default:
      return 0;
  }
}

// Mirrors Block.getRenderType per block id (0 cube, 1 cross, 2 torch,
// 3 fire, 4 fluid, else special geometry handled by later milestones).
inline int render_type(int id) {
  switch (id) {
    case kSapling:
    case kWeb:
    case kTallGrass:
    case kDeadBush:
    case kFlowerYellow:
    case kFlowerRed:
    case kMushroomBrown:
    case kMushroomRed:
    case kReed:
      return 1;
    case kTorch:
    case kTorchRedIdle:
    case kTorchRedOn:
      return 2;
    case kFire:
      return 3;
    case kWaterMoving:
    case kWaterStill:
    case kLavaMoving:
    case kLavaStill:
      return 4;
    case kRedstoneWire:
      return 5;
    case kCrops:
    case kNetherWart:
      return 6;
    case kDoorWood:
    case kDoorSteel:
      return 7;
    case kLadder:
      return 8;
    case kRail:
    case kRailPowered:
    case kRailDetector:
      return 9;
    case kStairsWood:
    case kStairsCobble:
    case kStairsBrick:
    case kStairsStoneBrick:
    case kStairsNether:
      return 10;
    case kFence:
    case kNetherFence:
      return 11;
    case kLever:
      return 12;
    case kCactus:
      return 13;
    case kBed:
      return 14;
    case kRepeaterIdle:
    case kRepeaterOn:
      return 15;
    case kPistonSticky:
    case kPistonBase:
      return 16;
    case kPistonExt:
      return 17;
    case kPaneIron:
    case kPaneGlass:
      return 18;
    case kPumpkinStem:
    case kMelonStem:
      return 19;
    case kVine:
      return 20;
    case kFenceGate:
      return 21;
    case kChest:
      return 22;
    case kLilyPad:
      return 23;
    case kCauldron:
      return 24;
    case kBrewingStand:
      return 25;
    case kEndFrame:
      return 26;
    case kDragonEgg:
      return 27;
    case kPistonMoving:
    case kSignPost:
    case kSignWall:
    case kEndPortal:
      return -1;
    default:
      return 0;
  }
}
inline bool is_normal_cube(int id) {
  if (!is_opaque(id)) return false;
  return id != kLeaves;
}

// Mirrors Material.isSolid (false only for air/water/lava/plants/vine/
// circuits/snow/fire/portal/web families). Used by Block.getIsBlockSolid
// (fluid flow push) — NOT the same as is_opaque.
inline bool material_is_solid(int id) {
  switch (id) {
    case kAir:  // Material.air is Transparent: isSolid false (needed by fluid push)
    case kWaterMoving:
    case kWaterStill:
    case kLavaMoving:
    case kLavaStill:
    case kSapling:
    case kRailPowered:
    case kRailDetector:
    case kWeb:
    case kTallGrass:
    case kDeadBush:
    case kFlowerYellow:
    case kFlowerRed:
    case kMushroomBrown:
    case kMushroomRed:
    case kTorch:
    case kFire:
    case kRedstoneWire:
    case kCrops:
    case kRail:
    case kLever:
    case kPlateStone:
    case kPlateWood:
    case kTorchRedIdle:
    case kTorchRedOn:
    case kButton:
    case kSnowCover:
    case kReed:
    case kPortal:
    case kRepeaterIdle:
    case kRepeaterOn:
    case kPumpkinStem:
    case kMelonStem:
    case kVine:
    case kNetherWart:
      return false;
    default:
      return true;
  }
}

// Mirrors Material.getIsOpaque = isSolid && !translucent. Translucent set:
// leaves, glass, tnt, ice, snow, cactus, glowstone. Used by fence shaping.
inline bool material_opaque(int id) {
  if (!material_is_solid(id)) return false;
  switch (id) {
    case kLeaves:
    case kGlass:
    case kTnt:
    case kSnowCover:
    case kIce:
    case kCactus:
    case kGlowstone:
      return false;
    default:
      return true;
  }
}

// Mirrors Block.renderAsNormalBlock (default true; false-list transcribed
// from the Block* overrides — leaves/glass/ice/steps render as cubes here).
inline bool renders_as_normal(int id) {
  switch (id) {
    case kSapling:
    case kWeb:
    case kTallGrass:
    case kDeadBush:
    case kFlowerYellow:
    case kFlowerRed:
    case kMushroomBrown:
    case kMushroomRed:
    case kTorch:
    case kFire:
    case kBed:
    case kRailPowered:
    case kRailDetector:
    case kCrops:
    case kFarmland:
    case kSignPost:
    case kDoorWood:
    case kLadder:
    case kRail:
    case kSignWall:
    case kLever:
    case kPlateStone:
    case kDoorSteel:
    case kPlateWood:
    case kTorchRedIdle:
    case kTorchRedOn:
    case kButton:
    case kSnowCover:
    case kReed:
    case kFence:
    case kPortal:
    case kCake:
    case kRepeaterIdle:
    case kRepeaterOn:
    case kTrapDoor:
    case kPumpkinStem:
    case kMelonStem:
    case kVine:
    case kNetherWart:
    case kEnchantTable:
    case kBrewingStand:
    case kCauldron:
    case kEndPortal:
    case kDragonEgg:
    case kChest:
    case kLockedChest:
    case kStairsWood:
    case kStairsCobble:
    case kStairsBrick:
    case kStairsStoneBrick:
    case kStairsNether:
    case kRedstoneWire:
    case kPistonSticky:
    case kPistonBase:
    case kPistonExt:
    case kPistonMoving:
    case kPaneIron:
    case kPaneGlass:
    case kFenceGate:
    case kWaterMoving:
    case kWaterStill:
    case kLavaMoving:
    case kLavaStill:
    case kCactus:
      return false;
    default:
      return true;
  }
}

}  // namespace craftpp::world::bid
