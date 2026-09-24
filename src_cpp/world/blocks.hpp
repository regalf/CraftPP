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
    case kDoorWood:
    case kDoorSteel:
    case kLadder:
      return false;
    default:
      return true;
  }
}

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

}  // namespace craftpp::world::bid
