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
  virtual bool entities_prevent_place(const Aabb& box) { return false; }  // M5 mobs
  virtual void on_item_drop(int item_id, int count, int damage, double px, double py, double pz,
                            double mx, double my, double mz) {}
  virtual void play_aux_sfx(int id, int x, int y, int z, int data) {}
  virtual void play_place_sound(const char* name, double x, double y, double z, float vol,
                                float pitch) {}
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
void harvest_block(EditWorld& w, Breaker& br, int x, int y, int z, int meta);
// ItemBlock.onItemUse (generic placement for all block items).
bool use_block_item(EditWorld& w, BlockCollider& collider, Breaker& br, int& stack_size,
                    int item_id, int item_damage, int x, int y, int z, int side);
// ItemDoor.onItemUse (two-block doors) + shared placeDoorBlock.
bool use_door_item(EditWorld& w, BlockCollider& collider, Breaker& br, int& stack_size,
                   int item_id, int x, int y, int z, int side);
// onBlockPlaced (side -> meta) + onBlockPlacedBy (yaw -> meta) orientation.
void on_placed(EditWorld& w, int id, int x, int y, int z, int side);
void on_placed_by(EditWorld& w, int id, int x, int y, int z, float yaw);
int placed_meta(int item_id, int item_damage);  // ItemBlock.getPlacedBlockMetadata

}  // namespace craftpp::world::edit
