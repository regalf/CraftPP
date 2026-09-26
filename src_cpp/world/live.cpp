#include "world/live.hpp"

namespace craftpp::world {

void LiveWorld::provide_area(int cx0, int cz0, int cx1, int cz1) {
  for (int cz = cz0; cz <= cz1; ++cz) {
    for (int cx = cx0; cx <= cx1; ++cx) {
      if (region_.has_chunk(cx, cz)) continue;
      provider_.set_chunk_seed(cx, cz);
      std::vector<std::int8_t> b;
      provider_.generate_terrain(cx, cz, b);
      provider_.replace_biome_blocks(cx, cz, b,
                                     manager_.block_biomes(cx * 16, cz * 16, 16, 16));
      caves_.generate(seed_, cx, cz, b);
      ravine_.generate(seed_, cx, cz, b);
      region_.ensure_chunk(cx, cz, b.data());
      dirty_[{cx, cz}] = true;
    }
  }
  for (int cz = cz0; cz <= cz1; ++cz) {
    for (int cx = cx0; cx <= cx1; ++cx) {
      if (populated_.count({cx, cz}) != 0) continue;
      populate_chunk(region_, manager_, seed_, cx, cz, wrand_);
      populated_[{cx, cz}] = true;
      dirty_[{cx, cz}] = true;
    }
  }
}

void LiveWorld::tick() {
  ++time_;
  for (entity::Entity* e : entities_) {
    if (e != nullptr) e->on_update();
  }
}

void LiveWorld::set_raw(int x, int y, int z, int id, int meta) {
  // World.setBlockAndMetadata path: Chunk write (relight + light updates via
  // RegionWorld) ; neighbor notify is the caller's (set_and_notify).
  if (meta == 0 && region_.get_meta(x, y, z) == 0) {
    region_.set_id(x, y, z, id);
  } else {
    region_.set_id_meta(x, y, z, id, meta);
  }
  if (y >= 0 && y < RegionWorld::kHeight) dirty_[{x >> 4, z >> 4}] = true;
}

bool LiveWorld::chunks_exist(int x0, int y0, int z0, int x1, int y1, int z1) const {
  // Mirrors World.checkChunksExist y-range gate.
  if (y1 < 0 || y0 >= RegionWorld::kHeight) return false;
  for (int cx = x0 >> 4; cx <= x1 >> 4; ++cx)
    for (int cz = z0 >> 4; cz <= z1 >> 4; ++cz)
      if (!region_.has_chunk(cx, cz)) return false;
  return true;
}

bool LiveWorld::is_normal_cube(int x, int y, int z) const {
  const int id = block_id(x, y, z);
  return bid::is_opaque(id) && bid::renders_as_normal(id);
}

bool LiveWorld::solid_side(int x, int y, int z, bool missing_default) const {
  (void)missing_default;
  return is_normal_cube(x, y, z);
}

bool LiveWorld::material_solid_at(int x, int y, int z) const {
  return bid::material_is_solid(block_id(x, y, z));
}

}  // namespace craftpp::world
