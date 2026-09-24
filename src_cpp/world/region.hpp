#pragma once

#include <cstdint>
#include <map>
#include <tuple>
#include <vector>

namespace craftpp::world {

// Multi-chunk block region used by M3c worldgen (caves, ravines, decorators).
// Stores ids + metadata, answers the World queries generators need, and
// replicates height/skylight scans exactly (no lighting engine yet — block
// light is 0 everywhere like a fresh world, sky light is 15 minus opacity).
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

  // Mirrors World.getTopSolidOrLiquidBlock: topmost solid non-leaves + 1.
  int top_solid_or_liquid(int x, int z) const;
  // Mirrors Chunk heightMap: topmost y whose below-block is non-transparent.
  int height_value(int x, int z) const;
  // Saved skylight value (frozen at install like the engine's skylightMap).
  int saved_sky(int x, int y, int z) const;
  // 15 minus summed light opacity above (fresh-world skylight equivalent).
  int sky_light(int x, int y, int z) const;
  bool can_see_sky(int x, int y, int z) const;

  // Dumps raw chunk bytes (Java layout) for hashing/comparison.
  std::vector<std::int8_t> chunk_bytes(int cx, int cz) const;
  std::vector<std::uint8_t> chunk_meta(int cx, int cz) const;

 private:
  struct ChunkData {
    std::vector<std::int8_t> ids;
    std::vector<std::uint8_t> meta;
    // Install-time skylight (generateSkylightMap equivalent). The engine
    // never recomputes skylight during worldgen (relight only touches
    // heightMap; scheduled lighting never runs), so this stays frozen and
    // plant light checks read it -- load-bearing for shaded spots.
    std::vector<std::uint8_t> sky;
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

  std::map<std::pair<int, int>, ChunkData> chunks_;
};

}  // namespace craftpp::world
