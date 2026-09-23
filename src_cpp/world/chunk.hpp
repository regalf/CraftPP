#pragma once

#include <array>
#include <cstdint>

#include "world/block.hpp"

namespace craftpp::world {

// Minimal fixed-height chunk store for M2 (16 x Height x 16 like Chunk.java).
// Out-of-bounds reads return Air so meshing treats the world edge as open sky.
// The full Chunk (sections, lighting, entities, dirty flags) lands with M3.
class Chunk {
 public:
  static constexpr int kSize = 16;
  static constexpr int kHeight = 128;

  BlockId get(int x, int y, int z) const {
    if (x < 0 || x >= kSize || y < 0 || y >= kHeight || z < 0 || z >= kSize) return BlockId::Air;
    return blocks_[index(x, y, z)];
  }
  void set(int x, int y, int z, BlockId id) {
    if (x < 0 || x >= kSize || y < 0 || y >= kHeight || z < 0 || z >= kSize) return;
    blocks_[index(x, y, z)] = id;
  }

 private:
  static std::size_t index(int x, int y, int z) {
    return static_cast<std::size_t>((y * kSize + z) * kSize + x);
  }
  std::array<BlockId, kSize * kHeight * kSize> blocks_{};
};

// Classic flat layering used by the M2 demo: bedrock, stone, dirt, grass.
void fill_flat(Chunk& chunk);

}  // namespace craftpp::world
