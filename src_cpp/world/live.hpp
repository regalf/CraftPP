#pragma once

#include <cstdint>
#include <map>
#include <utility>
#include <vector>

#include "core/random.hpp"
#include "entity/entity.hpp"
#include "world/block_collision.hpp"
#include "world/block_place.hpp"
#include "world/blocks.hpp"
#include "world/chunk_manager.hpp"
#include "world/mapgen.hpp"
#include "world/provider.hpp"
#include "world/region.hpp"
#include "world/worldgen.hpp"

namespace craftpp::world {

// Live singleplayer world (M5): real generated chunks (terrain + carvers +
// populate with the synchronous light engine) behind the EditWorld interface
// the M4 entity/controller/placement code already speaks, plus world time
// and a minimal entity registry ticked at 20 TPS.
//
// Storage is RegionWorld (full ids + metadata + stored height/sky/block
// light); generation reuses the tested M3 pipeline stage by stage.
class LiveWorld : public edit::EditWorld {
 public:
  explicit LiveWorld(std::int64_t seed)
      : seed_(seed), manager_(seed), provider_(manager_, seed), caves_(manager_), ravine_(manager_) {
    wrand_.set_seed(0L);  // pinned like the fixed-seed harness (vanilla seeds it per-run)
  }

  std::int64_t seed() const { return seed_; }

  // Generates (terrain + carvers + install) every chunk in the rect, then
  // populates each exactly once. Chunks must be provided with enough margin
  // for populate scatter (tests use +3, populate the inner part).
  void provide_area(int cx0, int cz0, int cx1, int cz1);
  bool is_provided(int cx, int cz) const { return region_.has_chunk(cx, cz); }
  bool is_populated(int cx, int cz) const { return populated_.count({cx, cz}) != 0; }

  // 20 TPS world tick: advances time, ticks registered entities.
  // (Random block ticks, weather, spawning land here as M5 progresses.)
  void tick();
  std::int64_t world_time() const { return time_; }

  void add_entity(entity::Entity* e) { entities_.push_back(e); }

  // Remesh dirty tracking for the renderer.
  bool is_dirty(int cx, int cz) const {
    auto it = dirty_.find({cx, cz});
    return it != dirty_.end() && it->second;
  }
  void clear_dirty(int cx, int cz) { dirty_[{cx, cz}] = false; }

  // ---- EditWorld (backed by RegionWorld; writes run light + relight) ----
  int block_id(int x, int y, int z) const override { return region_.get_id(x, y, z); }
  int block_meta(int x, int y, int z) const override { return region_.get_meta(x, y, z); }
  void set_raw(int x, int y, int z, int id, int meta) override;
  bool chunks_exist(int x0, int y0, int z0, int x1, int y1, int z1) const override;
  bool is_normal_cube(int x, int y, int z) const override;
  bool solid_side(int x, int y, int z, bool missing_default) const override;
  bool material_solid_at(int x, int y, int z) const override;
  JavaRandom& world_rand() override { return wrand_; }
  BlockCollider& collider() override { return collider_; }

  // Light queries for renderer/gameplay (stored nibbles, like the engine).
  int saved_sky(int x, int y, int z) const { return region_.saved_sky(x, y, z); }
  int saved_block(int x, int y, int z) const { return region_.saved_block(x, y, z); }
  int stored_height(int x, int z) const { return region_.height_value(x, z); }

 private:
  std::int64_t seed_;
  std::int64_t time_ = 0;
  ChunkManager manager_;
  TerrainProvider provider_;
  MapGenCaves caves_;
  MapGenRavine ravine_;
  RegionWorld region_;
  JavaRandom wrand_;
  BlockCollider collider_;
  std::map<std::pair<int, int>, bool> populated_;
  std::map<std::pair<int, int>, bool> dirty_;
  std::vector<entity::Entity*> entities_;  // non-owning; app owns the player/mobs
};

}  // namespace craftpp::world
