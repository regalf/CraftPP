#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "core/random.hpp"
#include "entity/drops.hpp"
#include "entity/entity.hpp"
#include "entity/living.hpp"
#include "world/block_collision.hpp"
#include "world/block_place.hpp"
#include "world/blocks.hpp"
#include "world/chunk_manager.hpp"
#include "world/mapgen.hpp"
#include "world/provider.hpp"
#include "world/region.hpp"
#include "world/tick.hpp"
#include "world/tile.hpp"
#include "world/worldgen.hpp"

namespace craftpp::entity {
class Player;
}  // namespace craftpp::entity

namespace craftpp::world {

// Live singleplayer world (M5): real generated chunks (terrain + carvers +
// populate with the synchronous light engine) behind the EditWorld interface
// the M4 entity/controller/placement code already speaks, plus world time
// and a minimal entity registry ticked at 20 TPS.
//
// Storage is RegionWorld (full ids + metadata + stored height/sky/block
// light); generation reuses the tested M3 pipeline stage by stage.
class LiveWorld : public edit::EditWorld, public tile::TileWorld {
 public:
  explicit LiveWorld(std::int64_t seed)
      : seed_(seed), manager_(seed), provider_(manager_, seed), caves_(manager_), ravine_(manager_) {
    wrand_.set_seed(0L);  // pinned like the fixed-seed harness (vanilla seeds it per-run)
    JavaRandom lr(seed);
    update_lcg_ = lr.next_int();  // vanilla uses new Random() (per-run); pinned here
  }

  std::int64_t seed() const { return seed_; }

  // Generates (terrain + carvers + install) every chunk in the rect, then
  // populates each exactly once. Chunks must be provided with enough margin
  // for populate scatter (tests use +3, populate the inner part).
  void provide_area(int cx0, int cz0, int cx1, int cz1);
  // Stepwise form for the loading screen (same work, chunk by chunk).
  void gen_chunk(int cx, int cz);
  void populate_one(int cx, int cz);
  bool is_provided(int cx, int cz) const { return region_.has_chunk(cx, cz); }
  bool is_populated(int cx, int cz) const { return populated_.count({cx, cz}) != 0; }

  // 20 TPS world tick: advances time, ticks registered entities.
  // (Random block ticks, weather, spawning land here as M5 progresses.)
  void tick();
  std::int64_t world_time() const { return time_; }
  // Daylight 0.2..1.0 for the renderer (func_35464_b, clear skies for now).
  float daylight() const;
  // Scheduled block ticks (fire); drained each tick.
  void schedule_tick(int x, int y, int z, int id, int delay) {
    tick::schedule_tick(sched_, time_, x, y, z, id, delay);
  }

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
  // Raw write without tile sync (furnace idle/burn swaps use this).
  void write_raw(int x, int y, int z, int id, int meta);
  // ---- tile::TileWorld ----
  void set_block_raw(int x, int y, int z, int id, int meta) override { write_raw(x, y, z, id, meta); }
  void mark_dirty(int x, int y, int z) override {
    if (y >= 0 && y < RegionWorld::kHeight) dirty_[{x >> 4, z >> 4}] = true;
  }
  tile::TileEntity* tile_at(int x, int y, int z);
  // Drops (EditWorld hook): spawns an owned item entity, pickup delay 10.
  void on_item_drop(int item_id, int count, int damage, double px, double py, double pz, double mx,
                    double my, double mz) override;
  const std::vector<std::unique_ptr<entity::DroppedItem>>& items() const { return items_; }
  // Owned mobs (spawned by perform_spawning, also registered for ticks).
  const std::vector<std::unique_ptr<entity::Living>>& mobs() const { return mobs_; }
  // ---- save/load access (world/save.*) ----
  std::vector<const tile::TileEntity*> tile_entities() const;
  const std::vector<tick::ScheduledTick>& scheduled_ticks() const { return sched_; }
  void adopt_item(std::unique_ptr<entity::DroppedItem> e) { items_.push_back(std::move(e)); }
  void adopt_mob(std::unique_ptr<entity::Living> e) { mobs_.push_back(std::move(e)); }
  void set_tile(std::unique_ptr<tile::TileEntity> t);
  void schedule_loaded_tick(int x, int y, int z, int id, int delay) {
    tick::schedule_tick(sched_, time_, x, y, z, id, delay);
  }
  void set_spawn_point(double x, double y, double z) {
    spawn_x_ = x;
    spawn_y_ = y;
    spawn_z_ = z;
  }
  void set_spawn_flags(bool hostile, bool peaceful) {
    spawn_hostile_ = hostile;
    spawn_peaceful_ = peaceful;
  }
  // ---- McRegion save/load (world/save.*) ----
  void set_level_name(std::string name) { level_name_ = std::move(name); }
  const std::string& level_name() const { return level_name_; }
  void set_world_time(std::int64_t t) { time_ = t; }
  void set_game_type(int t) { game_type_ = t; }
  int game_type() const { return game_type_; }
  void set_hardcore(bool h) { hardcore_ = h; }
  bool hardcore() const { return hardcore_; }
  std::vector<std::int8_t> chunk_ids(int cx, int cz) const { return region_.chunk_bytes(cx, cz); }
  std::vector<std::uint8_t> chunk_metadata(int cx, int cz) const {
    return region_.chunk_meta(cx, cz);
  }
  std::vector<std::uint8_t> chunk_skylight(int cx, int cz) const {
    return region_.chunk_light(cx, cz, true);
  }
  std::vector<std::uint8_t> chunk_blocklight(int cx, int cz) const {
    return region_.chunk_light(cx, cz, false);
  }
  std::vector<std::uint8_t> chunk_heightmap(int cx, int cz) const {
    return region_.chunk_height(cx, cz);
  }
  std::vector<std::pair<int, int>> provided_chunks() const;
  void install_chunk(int cx, int cz, const std::int8_t* ids, const std::uint8_t* meta,
                     const std::uint8_t* sky, const std::uint8_t* block,
                     const std::uint8_t* height) {
    region_.install_saved(cx, cz, ids, meta, sky, block, height);
  }
  void mark_populated(int cx, int cz) { populated_[{cx, cz}] = true; }
  // Writes region/*.mcr for every provided chunk + level.dat rotation.
  // When player != nullptr its state is stored as the level.dat Player
  // compound (feet convention: pos_y - y_offset).
  void save(const std::string& save_dir, const entity::Player* player = nullptr) const;
  // Loads level.dat (seed must match) + all region chunks. Returns false
  // when no save exists or the seed differs.
  bool load(const std::string& save_dir);
  // SpawnerAnimals.performSpawning (pigs + zombies; biome lists are M5+).
  int perform_spawning();
  entity::Entity* closest_player_to(const entity::Entity& e, double max_dist) override;
  entity::Entity* closest_player_at(double x, double y, double z, double r);
  std::vector<entity::Entity*> entities_excluding(const entity::Entity& e,
                                                  const Aabb& box) override;
  bool chunks_exist(int x0, int y0, int z0, int x1, int y1, int z1) const override;
  bool is_normal_cube(int x, int y, int z) const override;
  bool solid_side(int x, int y, int z, bool missing_default) const override;
  bool material_solid_at(int x, int y, int z) const override;
  JavaRandom& world_rand() override { return wrand_; }
  BlockCollider& collider() override { return collider_; }

  // Light queries for renderer/gameplay (stored nibbles, like the engine).
  int saved_sky(int x, int y, int z) const override { return region_.saved_sky(x, y, z); }
  int saved_block(int x, int y, int z) const override { return region_.saved_block(x, y, z); }
  bool can_see_sky(int x, int y, int z) const override { return region_.can_see_sky(x, y, z); }
  int skylight_sub() const override { return sky_sub_; }
  void relight_at(int x, int y, int z) override { region_.refresh_light(x, y, z); }
  int precip_height(int x, int z) const override { return region_.precip_height(x, z); }
  float temperature(int x, int z) const override {
    return manager_.temperatures(x, z, 1, 1)[0];
  }
  float rainfall(int x, int z) const { return manager_.rainfalls(x, z, 1, 1)[0]; }
  // Bulk climate for meshing (one GenLayer rect query, not per-column).
  void climate_rect(int x0, int z0, int w, int h, std::vector<float>& t,
                    std::vector<float>& r) const {
    t = manager_.temperatures(x0, z0, w, h);
    r = manager_.rainfalls(x0, z0, w, h);
  }
  int stored_height(int x, int z) const { return region_.height_value(x, z); }
  // Raw region access for the client mesher (read-only).
  const RegionWorld& region() const { return region_; }

 private:
  std::int64_t seed_;
  std::int64_t time_ = 0;
  int sky_sub_ = 0;
  std::int32_t update_lcg_ = 0;
  ChunkManager manager_;
  TerrainProvider provider_;
  MapGenCaves caves_;
  MapGenRavine ravine_;
  RegionWorld region_;
  JavaRandom wrand_;
  BlockCollider collider_;
  std::vector<tick::ScheduledTick> sched_;
  std::map<std::pair<int, int>, bool> populated_;
  std::map<std::pair<int, int>, bool> dirty_;
  std::map<std::tuple<int, int, int>, std::unique_ptr<tile::TileEntity>> tiles_;
  std::vector<std::unique_ptr<entity::DroppedItem>> items_;
  std::vector<std::unique_ptr<entity::Living>> mobs_;
  double spawn_x_ = 0.0, spawn_y_ = 64.0, spawn_z_ = 0.0;
  std::string level_name_ = "Craft++";
  int game_type_ = 0;  // 0 survival, 1 creative (level.dat GameType)
  bool hardcore_ = false;
  bool spawn_hostile_ = true;
  bool spawn_peaceful_ = true;
  std::vector<entity::Entity*> entities_;  // non-owning; app owns the player/mobs
};

}  // namespace craftpp::world
