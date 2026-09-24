#include "world/region.hpp"

#include <cstring>

#include "world/blocks.hpp"

namespace craftpp::world {

void RegionWorld::ensure_chunk(int cx, int cz, const std::int8_t* raw_blocks) {
  auto key = std::make_pair(cx, cz);
  if (chunks_.count(key) != 0) return;
  ChunkData d;
  d.ids.assign(raw_blocks, raw_blocks + 16 * kHeight * 16);
  d.meta.assign(16 * kHeight * 16, 0);
  // Frozen install-time skylight (Chunk.generateSkylightMap skylight part).
  d.sky.assign(16 * kHeight * 16, 0);
  for (int lx = 0; lx < 16; ++lx) {
    for (int lz = 0; lz < 16; ++lz) {
      int light = 15;
      for (int y = kHeight - 1; y > 0 && light > 0; --y) {
        light -= bid::light_opacity(d.ids[raw_index(lx, y, lz)]);
        if (light > 0) d.sky[raw_index(lx, y, lz)] = static_cast<std::uint8_t>(light);
      }
    }
  }
  chunks_.emplace(key, std::move(d));
}

bool RegionWorld::has_chunk(int cx, int cz) const {
  return chunks_.count(std::make_pair(cx, cz)) != 0;
}

const RegionWorld::ChunkData* RegionWorld::find(int cx, int cz) const {
  auto it = chunks_.find(std::make_pair(cx, cz));
  return it == chunks_.end() ? nullptr : &it->second;
}

RegionWorld::ChunkData* RegionWorld::find(int cx, int cz) {
  auto it = chunks_.find(std::make_pair(cx, cz));
  return it == chunks_.end() ? nullptr : &it->second;
}

int RegionWorld::get_id(int x, int y, int z) const {
  if (y < 0 || y >= kHeight) return 0;
  const ChunkData* c = find(x >> 4, z >> 4);
  if (c == nullptr) return 0;
  return c->ids[raw_index(x & 15, y, z & 15)];
}

int RegionWorld::get_meta(int x, int y, int z) const {
  if (y < 0 || y >= kHeight) return 0;
  const ChunkData* c = find(x >> 4, z >> 4);
  if (c == nullptr) return 0;
  return c->meta[raw_index(x & 15, y, z & 15)];
}

void RegionWorld::set_id(int x, int y, int z, int id) {
  if (y < 0 || y >= kHeight) return;
  ChunkData* c = find(x >> 4, z >> 4);
  if (c == nullptr) return;
  const std::size_t i = raw_index(x & 15, y, z & 15);
  const int old = c->ids[i];
  if (old == id) return;  // Chunk.setBlockID early-out: no removal, meta kept.
  if (old != 0) note_leaves_removal(x, y, z, id, 0);
  c->ids[i] = static_cast<std::int8_t>(id);
  c->meta[i] = 0;  // Chunk.setBlockID zeroes metadata.
}

void RegionWorld::set_id_meta(int x, int y, int z, int id, int meta) {
  if (y < 0 || y >= kHeight) return;
  ChunkData* c = find(x >> 4, z >> 4);
  if (c == nullptr) return;
  const std::size_t i = raw_index(x & 15, y, z & 15);
  const int old = c->ids[i];
  const int old_meta = c->meta[i];
  if (old == id && old_meta == meta) return;  // setBlockIDWithMetadata no-op.
  if (old != 0) note_leaves_removal(x, y, z, id, meta);
  c->ids[i] = static_cast<std::int8_t>(id);
  c->meta[i] = static_cast<std::uint8_t>(meta);
}

void RegionWorld::set_meta_raw(int x, int y, int z, int meta) {
  if (y < 0 || y >= kHeight) return;
  ChunkData* c = find(x >> 4, z >> 4);
  if (c == nullptr) return;
  c->meta[raw_index(x & 15, y, z & 15)] = static_cast<std::uint8_t>(meta);
}

void RegionWorld::note_leaves_removal(int x, int y, int z, int new_id, int new_meta) {
  // Only leaves removal has block effects in worldgen (other
  // onBlockRemoval impls are no-ops here; tile entities are an M5 gap).
  if (get_id(x, y, z) != bid::kLeaves) return;
  // checkChunksExist(x-2..x+2): the whole neighborhood must be present.
  for (int cx = (x - 2) >> 4; cx <= (x + 2) >> 4; ++cx)
    for (int cz = (z - 2) >> 4; cz <= (z + 2) >> 4; ++cz)
      if (!has_chunk(cx, cz)) return;
  for (int dx = -1; dx <= 1; ++dx)
    for (int dy = -1; dy <= 1; ++dy)
      for (int dz = -1; dz <= 1; ++dz) {
        const int nx = x + dx, ny = y + dy, nz = z + dz;
        if (ny < 0 || ny >= kHeight) continue;
        ChunkData* c = find(nx >> 4, nz >> 4);
        if (c == nullptr) continue;
        const std::size_t i = raw_index(nx & 15, ny, nz & 15);
        if (c->ids[i] == bid::kLeaves) c->meta[i] |= 8;
      }
}

int RegionWorld::top_solid_or_liquid(int x, int z) const {
  for (int y = kHeight - 1; y > 0; --y) {
    const int id = get_id(x, y, z);
    if (id != 0 && bid::material_solid(id) && id != bid::kLeaves) return y + 1;
  }
  return -1;
}

int RegionWorld::height_value(int x, int z) const {
  int y = kHeight - 1;
  for (; y > 0 && bid::light_opacity(get_id(x, y - 1, z)) == 0; --y) {
  }
  return y;
}

int RegionWorld::saved_sky(int x, int y, int z) const {
  if (y < 0 || y >= kHeight) return 0;
  const ChunkData* c = find(x >> 4, z >> 4);
  if (c == nullptr) return 0;
  return c->sky[raw_index(x & 15, y, z & 15)];
}

int RegionWorld::sky_light(int x, int y, int z) const {  int light = 15;
  for (int yy = kHeight - 1; yy > y; --yy) {
    light -= bid::light_opacity(get_id(x, yy, z));
    if (light <= 0) return 0;
  }
  return light;
}

bool RegionWorld::can_see_sky(int x, int y, int z) const {
  for (int yy = kHeight - 1; yy > y; --yy) {
    if (bid::light_opacity(get_id(x, yy, z)) != 0) return false;
  }
  return true;
}

std::vector<std::int8_t> RegionWorld::chunk_bytes(int cx, int cz) const {
  const ChunkData* c = find(cx, cz);
  return c == nullptr ? std::vector<std::int8_t>{} : c->ids;
}

std::vector<std::uint8_t> RegionWorld::chunk_meta(int cx, int cz) const {
  const ChunkData* c = find(cx, cz);
  return c == nullptr ? std::vector<std::uint8_t>{} : c->meta;
}

}  // namespace craftpp::world
