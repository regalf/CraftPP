#pragma once

#include <array>
#include <cstdint>

namespace craftpp::world {

// Minimal block registry for M2. Ids and terrain.png tiles mirror Block.java
// / BlockGrass.java; the full registry (all ~90 blocks, bounds, materials)
// lands with M3-M4.
enum class BlockId : std::uint8_t {
  Air = 0,
  Stone = 1,
  Grass = 2,
  Dirt = 3,
  Bedrock = 7,
  Water = 9,
  Sand = 12,
  Ice = 79,
};

// Block faces, same ids as getBlockTexture(..., side): 0=bottom 1=top
// 2..5=sides (see Aabb::RayHit faces).
enum class Face : std::uint8_t { Bottom = 0, Top = 1, ZMin = 2, ZMax = 3, XMin = 4, XMax = 5 };

struct BlockDef {
  BlockId id;
  const char* name;
  bool opaque;
  // terrain.png tile per face (grass differs per face like BlockGrass).
  std::array<int, 6> tiles;
  // True when top/side faces are multiplied by the biome grass tint.
  bool grass_tinted;
};

const BlockDef& block_def(BlockId id);
int tile_for(BlockId id, Face face);

}  // namespace craftpp::world
