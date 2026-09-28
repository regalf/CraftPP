#pragma once

#include <functional>
#include <optional>
#include <vector>

#include "core/aabb.hpp"
#include "core/random.hpp"
#include "entity/entity.hpp"
#include "world/block_collision.hpp"
#include "world/blocks.hpp"

namespace craftpp::world::edit {

// Breaker-side hooks (implemented by Player; stats/exhaustion are M5-owned
// accumulators, mining permission is always true in M4).
struct Breaker {
  virtual ~Breaker() = default;
  virtual float yaw() const { return 0.0f; }
  virtual float pitch() const { return 0.0f; }
  // Eye position for aimed item use (lilypad/bucket raycast). The player
  // pos already carries the +1.62 eye height like the source render view.
  virtual double eye_x() const { return 0.0; }
  virtual double eye_y() const { return 0.0; }
  virtual double eye_z() const { return 0.0; }
  virtual bool can_mine(int x, int y, int z) const { return true; }  // func_35190_e
  virtual void add_stat(int stat, int n) {}
  virtual void add_exhaustion(float f) {}
};

// Editable world backing break/place. The M4 test world implements this over
// a hash map; the live World (M5) over chunks. Notify dispatch mirrors
// World.notifyBlocksOfNeighborChange (editingBlocks suppression included);
// lighting updates are M5 (no-ops here).
struct EditWorld : public craftpp::entity::EntityWorld {
  bool editing_blocks = false;
  virtual JavaRandom& world_rand() = 0;
  virtual void set_raw(int x, int y, int z, int id, int meta) = 0;
  virtual bool is_normal_cube(int x, int y, int z) const = 0;
  // func_41082_b: opaque && renderAsNormal (chunk-missing default below).
  virtual bool solid_side(int x, int y, int z, bool missing_default) const = 0;
  virtual bool material_solid_at(int x, int y, int z) const = 0;
  virtual bool entities_prevent_place(const Aabb& box) const { return false; }
  virtual void on_item_drop(int item_id, int count, int damage, double px, double py, double pz,
                            double mx, double my, double mz) {}
  virtual void play_aux_sfx(int id, int x, int y, int z, int data) {}
  virtual void play_place_sound(const char* name, double x, double y, double z, float vol,
                                float pitch) {}
  // Live light queries for the M5 tick engine (stored nibbles like the
  // engine; TestWorld keeps the fresh-world defaults).
  virtual int saved_sky(int x, int y, int z) const { return 15; }
  virtual int saved_block(int x, int y, int z) const { return 0; }
  virtual bool can_see_sky(int x, int y, int z) const { return true; }
  virtual int skylight_sub() const { return 0; }
  // Synchronous light recompute hook (World.updateAllLightTypes); the live
  // world runs the real BFS, test worlds keep the no-op.
  virtual void relight_at(int x, int y, int z) {
    (void)x;
    (void)y;
    (void)z;
  }
  // Precipitation height + biome temperature for live ice/snow (test
  // worlds: -1/warm = no formation).
  virtual int precip_height(int x, int z) const {
    (void)x;
    (void)z;
    return -1;
  }
  virtual float temperature(int x, int z) const {
    (void)x;
    (void)z;
    return 1.0f;
  }
  // Queued block update (scheduleBlockUpdate). Test worlds ignore it;
  // LiveWorld appends to the scheduled-tick queue.
  virtual void schedule_block_tick(int x, int y, int z, int id, int delay) {
    (void)x;
    (void)y;
    (void)z;
    (void)id;
    (void)delay;
  }
};

// ---- block tables (transcribed from Block.java + subclasses) ----
float hardness(int id);              // blockHardness (-1 = unbreakable)
bool harvestable_material(int id);   // Material.getIsHarvestable
bool can_harvest(int block_id, int held_item);  // InventoryPlayer + Item matrix
float str_vs(int item_id, int block_id);        // tool speeds (1.0 default)
int damage_vs_entity(int item_id);              // 1 default, tools/swords more
int item_max_damage(int item_id);               // 0 = undamageable
int item_max_stack(int item_id);
int armor_value(int item_id);  // damageReduceAmount (0 for non-armor)

// ---- placement rules ----
bool can_place_at(int id, int x, int y, int z, const BlockView& w);
bool can_place_on_side(int id, int x, int y, int z, int side, const BlockView& w);
// The World.canBlockBePlacedAt replaceable set (water/lava/fire/snow/vine):
// the only ids a placed block may overwrite. Everything else consults the
// per-block canPlaceBlockAt (air/groundcover for plain blocks, support-only
// for mounted ones like torch/rail/chest).
bool is_replaceable_by_blocks(int id);
// canBlockBePlacedAt (needs the placer collider for the sticky single path).
bool be_placed_at(EditWorld& w, BlockCollider& collider, int id, int x, int y, int z, int side);

// ---- editing pipeline (mirrors World.setBlock*/notify*) ----
void set_and_notify(EditWorld& w, int x, int y, int z, int id, int meta);
void set_meta_notify(EditWorld& w, int x, int y, int z, int meta);
void break_to_air(EditWorld& w, int x, int y, int z);  // setBlockWithNotify(x,y,z,0)
void notify_neighbors(EditWorld& w, int x, int y, int z, int changed_id);
void on_neighbor(EditWorld& w, int id, int x, int y, int z, int from_id);
void pop_with_drop(EditWorld& w, int x, int y, int z);
void drop_one(EditWorld& w, int x, int y, int z, int item_id, int count, int damage);
void drop_as_item(EditWorld& w, int x, int y, int z, int block_id, int meta, int fortune);
int drop_id(int id, int meta, JavaRandom& r, int fortune);
int drop_damage(int id, int meta);
int drop_count(int id, JavaRandom& r);

// ---- break/place gameplay ----
float block_strength(int id, int held_item);  // Block.blockStrength (0 = instant-ish)
// Block.harvestBlock: drops for the (already-read) block id. Call AFTER the
// cell goes to air like sendBlockRemoved does, so drops spawn in air.
void harvest_block(EditWorld& w, Breaker& br, int id, int x, int y, int z, int meta);
// ItemBlock.onItemUse (generic placement for all block items).
bool use_block_item(EditWorld& w, BlockCollider& collider, Breaker& br, int& stack_size,
                    int item_id, int item_damage, int x, int y, int z, int side);
// ItemDoor.onItemUse (two-block doors) + shared placeDoorBlock.
bool use_door_item(EditWorld& w, BlockCollider& collider, Breaker& br, int& stack_size,
                   int item_id, int x, int y, int z, int side);
// ItemLilyPad.onItemRightClick: fluid-hitting raycast for a water source
// (meta 0) with air above, places the pad on top. Ignores x/y/z/side (the
// aimed block), like the source right-click path.
bool use_lilypad_item(EditWorld& w, Breaker& br, int& stack_size, double reach);
// onBlockPlaced (side -> meta) + onBlockPlacedBy (yaw -> meta) orientation.
void on_placed(EditWorld& w, int id, int x, int y, int z, int side);
void on_placed_by(EditWorld& w, int id, int x, int y, int z, float yaw);
int placed_meta(int item_id, int item_damage);  // ItemBlock.getPlacedBlockMetadata

}  // namespace craftpp::world::edit
