#include "world/chunk_manager.hpp"

namespace craftpp::world {

std::vector<BiomeId> ChunkManager::block_biomes(int x, int z, int w, int h) const {
  const std::vector<std::int32_t> ids = layers_.voronoi->generate(x, z, w, h);
  std::vector<BiomeId> out;
  out.reserve(ids.size());
  for (std::int32_t id : ids) out.push_back(static_cast<BiomeId>(id));
  return out;
}

std::vector<BiomeId> ChunkManager::coarse_biomes(int x, int z, int w, int h) const {
  const std::vector<std::int32_t> ids = layers_.biome->generate(x, z, w, h);
  std::vector<BiomeId> out;
  out.reserve(ids.size());
  for (std::int32_t id : ids) out.push_back(static_cast<BiomeId>(id));
  return out;
}

std::vector<float> ChunkManager::temperatures(int x, int z, int w, int h) const {
  const std::vector<std::int32_t> raw = layers_.temperature->generate(x, z, w, h);
  std::vector<float> out;
  out.reserve(raw.size());
  for (std::int32_t v : raw) {
    float f = static_cast<float>(v) / 65536.0F;
    if (f > 1.0F) f = 1.0F;
    out.push_back(f);
  }
  return out;
}

}  // namespace craftpp::world
