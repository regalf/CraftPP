#include "world/block.hpp"

namespace craftpp::world {

namespace {

const BlockDef kAir{BlockId::Air, "air", false, {0, 0, 0, 0, 0, 0}, false};
// Tiles from Block.java: stone(1, tile 1), dirt(3, tile 2), bedrock(7, tile 17).
const BlockDef kStone{BlockId::Stone, "stone", true, {1, 1, 1, 1, 1, 1}, false};
// BlockGrass.getBlockTextureFromSideAndMetadata: top=0, bottom=2, side=3.
const BlockDef kGrass{BlockId::Grass, "grass", true, {2, 0, 3, 3, 3, 3}, true};
const BlockDef kDirt{BlockId::Dirt, "dirt", true, {2, 2, 2, 2, 2, 2}, false};
const BlockDef kBedrock{BlockId::Bedrock, "bedrock", true, {17, 17, 17, 17, 17, 17}, false};
// Water (still tile 32) is rendered but never occludes, like the source.
const BlockDef kWater{BlockId::Water, "water", false, {32, 32, 32, 32, 32, 32}, false};
const BlockDef kSand{BlockId::Sand, "sand", true, {18, 18, 18, 18, 18, 18}, false};
const BlockDef kIce{BlockId::Ice, "ice", true, {67, 67, 67, 67, 67, 67}, false};

}  // namespace

const BlockDef& block_def(BlockId id) {
  switch (id) {
    case BlockId::Stone:
      return kStone;
    case BlockId::Grass:
      return kGrass;
    case BlockId::Dirt:
      return kDirt;
    case BlockId::Bedrock:
      return kBedrock;
    case BlockId::Water:
      return kWater;
    case BlockId::Sand:
      return kSand;
    case BlockId::Ice:
      return kIce;
    case BlockId::Air:
    default:
      return kAir;
  }
}

int tile_for(BlockId id, Face face) {
  return block_def(id).tiles[static_cast<int>(face)];
}

}  // namespace craftpp::world
