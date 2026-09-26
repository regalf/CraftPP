#pragma once

#include <cstdint>
#include <map>
#include <tuple>
#include <vector>

namespace craftpp::world {

// Multi-chunk block region used by M3c worldgen (caves, ravines, decorators).
// Stores ids + metadata, answers the World queries generators need, and
// replicates the 1.0 lighting engine synchronously: stored heightMap +
// skylight/blocklight nibbles per chunk, relightBlock on writes, and the
// updateLightByType BFS (scheduleLightingUpdate is a no-op in 1.0, the
// skylight-occlusion flags only drain in the tick loop, so populate parity
// needs just the synchronous part).
class RegionWorld {
 public:
  static constexpr int kHeight = 128;

  // Generates base terrain (generateTerrain + replaceBlocksForBiome, no
  // carvers) for chunks [cx0,cx1) x [cz0,cz1) using an external generator
  // callback so this class stays independent of TerrainProvider.
  void ensure_chunk(int cx, int cz, const std::int8_t* raw_blocks);

  bool has_chunk(int cx, int cz) const;

  // Outside the region or y range: get returns air (Java loads/generates on
  // demand; tests pre-generate a margin so this never fires mid-pipeline).
  int get_id(int x, int y, int z) const;
  int get_meta(int x, int y, int z) const;
  void set_id(int x, int y, int z, int id);
  void set_id_meta(int x, int y, int z, int id, int meta);
  // Raw metadata write (Chunk.setBlockMetadata path: no removal, no notify).
  void set_meta_raw(int x, int y, int z, int meta);

  bool is_air(int x, int y, int z) const { return get_id(x, y, z) == 0; }

  // Mirrors World.getTopSolidOrLiquidBlock: topmost solid non-leaves + 1
  // (live scan, never the stored heightMap).
  int top_solid_or_liquid(int x, int z) const;
  // Mirrors Chunk.getHeightValue: the STORED heightMap, maintained by
  // relightBlock on writes (stale across raw carve writes by design).
  int height_value(int x, int z) const;
  // Saved skylight / blocklight nibbles (Chunk.skylightMap/blocklightMap,
  // maintained by relight + updateLightByType; missing chunk reads 0).
  int saved_sky(int x, int y, int z) const;
  int saved_block(int x, int y, int z) const;
  // Full light like getFullBlockLightValue (max of saved sky/block).
  int full_light(int x, int y, int z) const;
  // 15 minus summed light opacity above (live scan, for diagnostics only).
  int sky_light(int x, int y, int z) const;
  // Mirrors Chunk.canBlockSeeTheSky: y >= stored heightMap.
  bool can_see_sky(int x, int y, int z) const;
  // Mirrors Chunk.func_35840_c (getPrecipitationHeight): topmost solid or
  // liquid + 1, with -999 invalidation on writes and lazy rescan.
  int precip_height(int x, int z);

  // Dumps raw chunk bytes (Java layout) for hashing/comparison.
  std::vector<std::int8_t> chunk_bytes(int cx, int cz) const;
  std::vector<std::uint8_t> chunk_meta(int cx, int cz) const;

 private:
  struct ChunkData {
    std::vector<std::int8_t> ids;
    std::vector<std::uint8_t> meta;
    // Chunk.skylightMap / blocklightMap (install = generateSkylightMap, then
    // maintained by relightBlock + updateLightByType).
    std::vector<std::uint8_t> sky;
    std::vector<std::uint8_t> block;
    // Chunk.heightMap + precipitationHeightMap + lowestBlockHeight.
    std::vector<std::uint8_t> height;
    std::vector<int> precip;
    int lowest = kHeight - 1;
    // Chunk.updateSkylightColumns (set on writes; drained by the tick loop
    // via func_35841_j, which never runs during populate — stored only).
    std::vector<bool> occl;
  };
  const ChunkData* find(int cx, int cz) const;
  ChunkData* find(int cx, int cz);
  // Mirrors BlockLeaves.onBlockRemoval: when a leaves block is replaced, all
  // leaves in the 3x3x3 cube around it get meta bit 8 (decay-check marker).
  // Only runs when the full +/-2 chunk neighborhood exists (checkChunksExist).
  void note_leaves_removal(int x, int y, int z, int new_id, int new_meta);

  static std::size_t raw_index(int lx, int y, int lz) {
    return static_cast<std::size_t>((lx * 16 + lz) * kHeight + y);
  }
  static std::size_t col_index(int lx, int lz) {
    return static_cast<std::size_t>(lz * 16 + lx);
  }

  // Mirrors World.doChunksNearChunkExist: every chunk in (x±r, z±r) present.
  bool chunks_near_exist(int x, int y, int z, int r) const;
  // Mirrors World.getSavedLightValue / setLightValue (missing chunk: read 0,
  // write no-op; y>=128 reads 15 for sky, 0 for block).
  int get_saved(bool sky, int x, int y, int z) const;
  void set_saved(bool sky, int x, int y, int z, int v);
  // Mirrors Chunk.relightBlock / World.markBlocksDirtyVertical /
  // World.updateLightByType + compute helpers.
  void relight_block(int x, int y, int z);
  void mark_blocks_dirty_vertical(int lx, int lz, int y0, int y1);
  void update_light_by_type(bool sky, int x, int y, int z);
  int compute_sky(int cur, int x, int y, int z, int id, int opacity) const;
  int compute_block(int cur, int x, int y, int z, int id, int opacity) const;
  void update_all_light_types(int x, int y, int z);

  // Scratch BFS list (World.lightUpdateBlockList, 32768 ints, reused).
  std::vector<int> light_list_ = std::vector<int>(32768, 0);

  std::map<std::pair<int, int>, ChunkData> chunks_;
};

}  // namespace craftpp::world
