#include "world/chunk.hpp"

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

}  // namespace craftpp::world
