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
    case kLilyPad:
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
// NOTE (deviations kept from M3, both unobservable in worldgen input):
// - kTnt is opaque in source (BlockTNT keeps the default) but listed above
//   as non-opaque; TNT never generates, suite is green, M5 owns lighting.
// - kLilyPad is opaque in source (extends Block) but listed non-opaque;
//   changing it shifts swamp skylight columns, so it waits for M5 too.

// Mirrors Block.lightOpacity: leaves 1, water/ice 3, lava 255, snow cover 0,
// non-opaque plants 0, everything else opaque (255).
inline int light_opacity(int id) {
  if (id == kLeaves) return 1;
  if (id == kWaterMoving || id == kWaterStill || id == kIce) return 3;
  if (id == kLavaMoving || id == kLavaStill) return 255;
  return is_opaque(id) ? 255 : 0;
}

// Ground-cover materials for Block.canPlaceBlockAt (vine + snow cover).
inline bool is_ground_cover(int id) { return id == kVine || id == kSnowCover; }

// Mirrors World.isBlockNormalCube: opaque material && renders as normal block.
// (Leaves and other cross-textured blocks excluded; caps render as cubes.)
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
