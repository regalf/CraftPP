#include "world/chunk.hpp"

#include "world/block.hpp"

namespace craftpp::world {

void fill_flat(Chunk& chunk) {
  for (int z = 0; z < Chunk::kSize; ++z) {
    for (int x = 0; x < Chunk::kSize; ++x) {
      chunk.set(x, 0, z, BlockId::Bedrock);
      chunk.set(x, 1, z, BlockId::Stone);
      chunk.set(x, 2, z, BlockId::Stone);
      chunk.set(x, 3, z, BlockId::Dirt);
      chunk.set(x, 4, z, BlockId::Grass);
    }
  }
}

void fill_from_raw(Chunk& chunk, const std::int8_t* raw) {
  for (int x = 0; x < Chunk::kSize; ++x) {
    for (int z = 0; z < Chunk::kSize; ++z) {
      for (int y = 0; y < Chunk::kHeight; ++y) {
        const std::int8_t v = raw[(x * 16 + z) * Chunk::kHeight + y];
        BlockId id = BlockId::Air;
        switch (v) {
          case 1:
            id = BlockId::Stone;
            break;
          case 2:
            id = BlockId::Grass;
            break;
          case 3:
            id = BlockId::Dirt;
            break;
          case 7:
            id = BlockId::Bedrock;
            break;
          case 9:
            id = BlockId::Water;
            break;
          case 12:
            id = BlockId::Sand;
            break;
          case 79:
            id = BlockId::Ice;
            break;
          default:
            break;
        }
        chunk.set(x, y, z, id);
      }
    }
  }
}

}  // namespace craftpp::world
