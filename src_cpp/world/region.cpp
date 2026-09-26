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
  // Install = Chunk.generateSkylightMap: stored heightMap + skylight.
  d.sky.assign(16 * kHeight * 16, 0);
  d.block.assign(16 * kHeight * 16, 0);
  d.height.assign(256, 0);
  d.precip.assign(256, -999);
  d.occl.assign(256, false);
  d.lowest = kHeight - 1;
  for (int lx = 0; lx < 16; ++lx) {
    for (int lz = 0; lz < 16; ++lz) {
      int h = kHeight - 1;
      for (; h > 0 && bid::light_opacity(d.ids[raw_index(lx, h - 1, lz)]) == 0; --h) {
      }
      d.height[col_index(lx, lz)] = static_cast<std::uint8_t>(h);
      if (h < d.lowest) d.lowest = h;
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
  // Chunk.setBlockID early-out: no removal, meta kept — but World.setBlock
  // still runs updateAllLightTypes afterwards, so light runs regardless.
  if (old != id) {
    if (old != 0) note_leaves_removal(x, y, z, id, 0);
    c->ids[i] = static_cast<std::int8_t>(id);
    c->meta[i] = 0;  // Chunk.setBlockID zeroes metadata.
    if (y >= c->precip[col_index(x & 15, z & 15)] - 1) {
      c->precip[col_index(x & 15, z & 15)] = -999;
    }
    // Chunk.setBlockID lighting (no hasNoSky guard in this variant).
    if (bid::light_opacity(id) != 0) {
      if (y >= c->height[col_index(x & 15, z & 15)]) relight_block(x, y + 1, z);
    } else if (y == static_cast<int>(c->height[col_index(x & 15, z & 15)]) - 1) {
      relight_block(x, y, z);
    }
    c->occl[col_index(x & 15, z & 15)] = true;  // propagateSkylightOcclusion
  }
  update_all_light_types(x, y, z);
}

void RegionWorld::set_id_meta(int x, int y, int z, int id, int meta) {
  if (y < 0 || y >= kHeight) return;
  ChunkData* c = find(x >> 4, z >> 4);
  if (c == nullptr) return;
  const std::size_t i = raw_index(x & 15, y, z & 15);
  const int old = c->ids[i];
  const int old_meta = c->meta[i];
  // setBlockIDWithMetadata no-op — updateAllLightTypes still runs.
  if (old != id || old_meta != meta) {
    if (old != 0) note_leaves_removal(x, y, z, id, meta);
    c->ids[i] = static_cast<std::int8_t>(id);
    c->meta[i] = static_cast<std::uint8_t>(meta);
    if (y >= c->precip[col_index(x & 15, z & 15)] - 1) {
      c->precip[col_index(x & 15, z & 15)] = -999;
    }
    // Chunk.setBlockIDWithMetadata lighting (hasNoSky is false: runs).
    if (bid::light_opacity(id) != 0) {
      if (y >= c->height[col_index(x & 15, z & 15)]) relight_block(x, y + 1, z);
    } else if (y == static_cast<int>(c->height[col_index(x & 15, z & 15)]) - 1) {
      relight_block(x, y, z);
    }
    c->occl[col_index(x & 15, z & 15)] = true;  // propagateSkylightOcclusion
  }
  update_all_light_types(x, y, z);
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
  const ChunkData* c = find(x >> 4, z >> 4);
  if (c == nullptr) return 0;  // World.getHeightValue: missing chunk reads 0.
  return c->height[col_index(x & 15, z & 15)];
}

int RegionWorld::get_saved(bool sky, int x, int y, int z) const {
  // Mirrors World.getSavedLightValue (4-arg): y<0 reads 0, y>=128 reads 127.
  if (y < 0) y = 0;
  if (y >= kHeight) y = kHeight - 1;
  const ChunkData* c = find(x >> 4, z >> 4);
  if (c == nullptr) return 0;
  const std::size_t i = raw_index(x & 15, y, z & 15);
  return sky ? c->sky[i] : c->block[i];
}

void RegionWorld::set_saved(bool sky, int x, int y, int z, int v) {
  if (y < 0 || y >= kHeight) return;
  ChunkData* c = find(x >> 4, z >> 4);
  if (c == nullptr) return;
  const std::size_t i = raw_index(x & 15, y, z & 15);
  if (sky) {
    c->sky[i] = static_cast<std::uint8_t>(v);
  } else {
    c->block[i] = static_cast<std::uint8_t>(v);
  }
}

int RegionWorld::saved_sky(int x, int y, int z) const { return get_saved(true, x, y, z); }

int RegionWorld::saved_block(int x, int y, int z) const { return get_saved(false, x, y, z); }

int RegionWorld::full_light(int x, int y, int z) const {
  // Mirrors World.getFullBlockLightValue (y clamp + max of the two nibbles).
  if (y < 0) return 0;
  if (y >= kHeight) y = kHeight - 1;
  const int s = get_saved(true, x, y, z);
  const int b = get_saved(false, x, y, z);
  return s > b ? s : b;
}

int RegionWorld::sky_light(int x, int y, int z) const {  int light = 15;
  for (int yy = kHeight - 1; yy > y; --yy) {
    light -= bid::light_opacity(get_id(x, yy, z));
    if (light <= 0) return 0;
  }
  return light;
}

bool RegionWorld::can_see_sky(int x, int y, int z) const {
  // Mirrors Chunk.canBlockSeeTheSky: y >= STORED heightMap.
  const ChunkData* c = find(x >> 4, z >> 4);
  if (c == nullptr) return true;
  return y >= static_cast<int>(c->height[col_index(x & 15, z & 15)]);
}

bool RegionWorld::chunks_near_exist(int x, int y, int z, int r) const {
  // Mirrors World.checkChunksExist (y-range always holds for our calls).
  if (y + r < 0 || y - r >= kHeight) return false;
  for (int cx = (x - r) >> 4; cx <= (x + r) >> 4; ++cx)
    for (int cz = (z - r) >> 4; cz <= (z + r) >> 4; ++cz)
      if (!has_chunk(cx, cz)) return false;
  return true;
}

void RegionWorld::update_all_light_types(int x, int y, int z) {
  // Mirrors World.updateAllLightTypes (hasNoSky is false).
  update_light_by_type(true, x, y, z);
  update_light_by_type(false, x, y, z);
}

int RegionWorld::compute_sky(int cur, int x, int y, int z, int id, int opacity) const {
  (void)cur;
  (void)id;
  if (can_see_sky(x, y, z)) return 15;
  if (opacity == 0) opacity = 1;
  int best = 0;
  // Same neighbor order as the source (z-, z+, y-, y+, x-, x+; max: order
  // is irrelevant, kept for readability).
  const int dx[6] = {0, 0, 0, 0, -1, 1};
  const int dy[6] = {0, 0, -1, 1, 0, 0};
  const int dz[6] = {-1, 1, 0, 0, 0, 0};
  for (int k = 0; k < 6; ++k) {
    const int v = get_saved(true, x + dx[k], y + dy[k], z + dz[k]) - opacity;
    if (v > best) best = v;
  }
  return best;
}

int RegionWorld::compute_block(int cur, int x, int y, int z, int id, int opacity) const {
  (void)cur;
  int best = bid::light_value(id);
  const int dx[6] = {-1, 1, 0, 0, 0, 0};
  const int dy[6] = {0, 0, -1, 1, 0, 0};
  const int dz[6] = {0, 0, 0, 0, -1, 1};
  for (int k = 0; k < 6; ++k) {
    const int v = get_saved(false, x + dx[k], y + dy[k], z + dz[k]) - opacity;
    if (v > best) best = v;
  }
  return best;
}

void RegionWorld::mark_blocks_dirty_vertical(int lx, int lz, int y0, int y1) {
  // Mirrors World.markBlocksDirtyVertical. NOTE: relightBlock passes LOCAL
  // chunk coords here and the source uses them as-is (vanilla quirk) — the
  // updates land on world column (lx, lz), replicated exactly.
  if (y0 > y1) {
    const int t = y1;
    y1 = y0;
    y0 = t;
  }
  for (int y = y0; y <= y1; ++y) update_light_by_type(true, lx, y, lz);
}

void RegionWorld::relight_block(int x, int y, int z) {
  // Mirrors Chunk.relightBlock (world coords in, local coords derived).
  ChunkData* c = find(x >> 4, z >> 4);
  if (c == nullptr) return;
  const int lx = x & 15, lz = z & 15;
  const std::size_t col = col_index(lx, lz);
  const std::size_t base = static_cast<std::size_t>((lx * 16 + lz) * kHeight);
  const int old_h = c->height[col];
  int new_h = old_h;
  if (y > old_h) new_h = y;
  while (new_h > 0 && bid::light_opacity(c->ids[base + static_cast<std::size_t>(new_h - 1)]) == 0) {
    --new_h;
  }
  if (new_h == old_h) return;
  mark_blocks_dirty_vertical(lx, lz, new_h, old_h);
  c->height[col] = static_cast<std::uint8_t>(new_h);
  if (new_h < c->lowest) {
    c->lowest = new_h;
  } else {
    int m = kHeight - 1;
    for (std::size_t k = 0; k < 256; ++k)
      if (static_cast<int>(c->height[k]) < m) m = c->height[k];
    c->lowest = m;
  }
  // Skylight fill of the changed span (hasNoSky is false).
  if (new_h < old_h) {
    for (int yy = new_h; yy < old_h; ++yy) c->sky[base + static_cast<std::size_t>(yy)] = 15;
  } else {
    for (int yy = old_h; yy < new_h; ++yy) c->sky[base + static_cast<std::size_t>(yy)] = 0;
  }
  // Column relight from the top down.
  {
    int lv = 15, yy = new_h;
    while (yy > 0 && lv > 0) {
      --yy;
      const int wx = (x >> 4) * 16 + lx, wz = (z >> 4) * 16 + lz;
      int op = bid::light_opacity(get_id(wx, yy, wz));
      if (op == 0) op = 1;
      lv -= op;
      if (lv < 0) lv = 0;
      c->sky[base + static_cast<std::size_t>(yy)] = static_cast<std::uint8_t>(lv);
    }
  }
  const int lo = new_h < old_h ? new_h : old_h;
  const int hi = new_h < old_h ? old_h : new_h;
  const int wx = (x >> 4) * 16 + lx, wz = (z >> 4) * 16 + lz;
  // updateSkylightNeighborHeight with WORLD coords (hasNoSky is false).
  const int nx[5] = {wx - 1, wx + 1, wx, wx, wx};
  const int nz[5] = {wz, wz, wz - 1, wz + 1, wz};
  for (int k = 0; k < 5; ++k) {
    if (hi > lo && chunks_near_exist(nx[k], kHeight / 2, nz[k], 16)) {
      for (int yy = lo; yy < hi; ++yy) update_light_by_type(true, nx[k], yy, nz[k]);
    }
  }
}

int RegionWorld::precip_height(int x, int z) const {
  // Mirrors Chunk.func_35840_c with lazy -999 rescan (Material.isSolid raw:
  // material_is_solid matches except web, which never generates).
  const ChunkData* c = find(x >> 4, z >> 4);
  if (c == nullptr) return -1;
  const std::size_t col = col_index(x & 15, z & 15);
  int v = c->precip[col];
  if (v == -999) {
    int yy = kHeight - 1;
    v = -1;
    while (yy > 0 && v == -1) {
      const int id = c->ids[raw_index(x & 15, yy, z & 15)];
      if (!bid::material_is_solid(id) && !bid::material_liquid(id)) {
        --yy;
      } else {
        v = yy + 1;
      }
    }
    c->precip[col] = v;
  }
  return v;
}

std::vector<std::int8_t> RegionWorld::chunk_bytes(int cx, int cz) const {
  const ChunkData* c = find(cx, cz);
  return c == nullptr ? std::vector<std::int8_t>{} : c->ids;
}

std::vector<std::uint8_t> RegionWorld::chunk_meta(int cx, int cz) const {
  const ChunkData* c = find(cx, cz);
  return c == nullptr ? std::vector<std::uint8_t>{} : c->meta;
}

void RegionWorld::update_light_by_type(bool sky, int x, int y, int z) {
  // Mirrors World.updateLightByType (both the decrease flood and the spread
  // phase) over light_list_. doChunksNearChunkExist radius is 17.
  if (!chunks_near_exist(x, y, z, 17)) return;
  int rd = 0, wr = 0;
  int saved = get_saved(sky, x, y, z);
  const int id = get_id(x, y, z);
  int op = bid::light_opacity(id);
  if (op == 0) op = 1;
  const int want =
      sky ? compute_sky(saved, x, y, z, id, op) : compute_block(saved, x, y, z, id, op);
  if (want > saved) {
    light_list_[wr++] = 133152;
  } else if (want < saved) {
    light_list_[wr++] = 133152 + (saved << 18);
    // Decrease flood (label129 in the source).
    bool spread = false;
    while (!spread) {
      int nx, ny, nz, lv, got;
      // Pop entries until one still holds its queued level.
      for (;;) {
        if (rd >= wr) {
          rd = 0;
          spread = true;
          break;
        }
        const int e = light_list_[rd++];
        nx = (e & 63) - 32 + x;
        ny = ((e >> 6) & 63) - 32 + y;
        nz = ((e >> 12) & 63) - 32 + z;
        lv = (e >> 18) & 15;
        got = get_saved(sky, nx, ny, nz);
        if (got == lv) break;
      }
      if (spread) break;
      set_saved(sky, nx, ny, nz, 0);
      if (lv == 0) continue;  // do..while(lv <= 0): zero entries just drain.
      // Skip far cells (do..while(dist >= 17)).
      int dx = nx - x, dy = ny - y, dz = nz - z;
      if (dx < 0) dx = -dx;
      if (dy < 0) dy = -dy;
      if (dz < 0) dz = -dz;
      if (dx + dy + dz >= 17) continue;
      // Spread the decrease to neighbors holding lv - opacity.
      const int ox[6] = {0, 0, 0, 0, -1, 1};
      const int oy[6] = {0, 0, -1, 1, 0, 0};
      const int oz[6] = {-1, 1, 0, 0, 0, 0};
      for (int k = 0; k < 6; ++k) {
        const int mx = nx + ox[k], my = ny + oy[k], mz = nz + oz[k];
        const int mv = get_saved(sky, mx, my, mz);
        int mop = bid::light_opacity(get_id(mx, my, mz));
        if (mop == 0) mop = 1;
        if (mv == lv - mop) {
          light_list_[wr++] = (mx - x + 32) + ((my - y + 32) << 6) +
                              ((mz - z + 32) << 12) + ((lv - mop) << 18);
        }
      }
    }
  }  // end decrease flood (else-if want < saved)
  // Increase spread (runs for both branches, like the source while loop).
  while (rd < wr) {
    const int e = light_list_[rd++];
    const int px = (e & 63) - 32 + x;
    const int py = ((e >> 6) & 63) - 32 + y;
    const int pz = ((e >> 12) & 63) - 32 + z;
    const int cur = get_saved(sky, px, py, pz);
    const int pid = get_id(px, py, pz);
    int pop = bid::light_opacity(pid);
    if (pop == 0) pop = 1;
    const int nv =
        sky ? compute_sky(cur, px, py, pz, pid, pop) : compute_block(cur, px, py, pz, pid, pop);
    if (nv == cur) continue;
    set_saved(sky, px, py, pz, nv);
    if (nv <= cur) continue;
    int ddx = px - x, ddy = py - y, ddz = pz - z;
    if (ddx < 0) ddx = -ddx;
    if (ddy < 0) ddy = -ddy;
    if (ddz < 0) ddz = -ddz;
    if (ddx + ddy + ddz >= 17) continue;
    if (wr >= static_cast<int>(light_list_.size()) - 6) continue;
    const int qx[6] = {px - 1, px + 1, px, px, px, px};
    const int qy[6] = {py, py, py - 1, py + 1, py, py};
    const int qz[6] = {pz, pz, pz, pz, pz - 1, pz + 1};
    for (int k = 0; k < 6; ++k) {
      if (get_saved(sky, qx[k], qy[k], qz[k]) < nv) {
        light_list_[wr++] =
            (qx[k] - x + 32) + ((qy[k] - y + 32) << 6) + ((qz[k] - z + 32) << 12);
      }
    }
  }
}

}  // namespace craftpp::world
