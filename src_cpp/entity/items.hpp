#pragma once

#include "core/random.hpp"
#include "world/block_place.hpp"

namespace craftpp::entity {

// Item use context (implemented by Player; lets ItemStack stay decoupled).
struct ItemUser {
  virtual ~ItemUser() = default;
  virtual int unbreaking_level() const { return 0; }  // M5 enchantments
  virtual JavaRandom& world_rand() = 0;
  virtual void on_item_broken_anim() {}
  virtual void on_item_stat(int item_id, int kind) {}  // 0=use 1=break
};

// Minimal ItemStack mirroring ItemStack.java (count/damage/use/durability).
// Item identity is the raw shifted id (0-255 blocks, 256+ items).
struct ItemStack {
  int item_id = 0;
  int stack_size = 1;
  int damage = 0;
  int animations_to_go = 0;  // swing/use anim counter (decremented per tick)

  ItemStack() = default;
  ItemStack(int id, int count, int dmg) : item_id(id), stack_size(count), damage(dmg) {}

  bool empty() const { return stack_size <= 0; }
  bool damageable() const { return world::edit::item_max_damage(item_id) > 0; }
  int max_damage() const { return world::edit::item_max_damage(item_id); }

  float str_vs_block(int block_id) const {
    return world::edit::str_vs(item_id, block_id);
  }
  bool can_harvest(int block_id) const {
    return world::edit::can_harvest(block_id, item_id);
  }
  int damage_vs_entity() const { return world::edit::damage_vs_entity(item_id); }

  // ItemStack.damageItem (unbreaking roll via world rand; break anim + stat
  // hooks; stack shrink + damage reset on break).
  void damage_item(int n, ItemUser& user) {
    if (!damageable()) return;
    if (n > 0) {
      const int unb = user.unbreaking_level();
      if (unb > 0 && user.world_rand().next_int(unb + 1) > 0) return;
    }
    damage += n;
    if (damage > max_damage()) {
      user.on_item_broken_anim();
      user.on_item_stat(item_id, 1);
      --stack_size;
      if (stack_size < 0) stack_size = 0;
      damage = 0;
    }
  }

  // Item.onBlockDestroyed dispatch (Tool -1, Sword -2, else false).
  bool on_block_destroyed(int x, int y, int z, ItemUser& user);
  // Item.hitEntity dispatch (Tool -2, Sword -1, else false).
  bool hit_entity(ItemUser& user);

  // ---- inventory helpers (InventoryPlayer/Container parity) ----
  int max_stack() const { return world::edit::item_max_stack(item_id); }
  bool stackable() const {
    return max_stack() > 1 && (!damageable() || damage == 0);
  }
  // Items with subtypes only merge on equal damage (coal/charcoal, dyes).
  bool has_subtypes() const {
    return item_id == 263 || item_id == 351;  // coal, dye
  }
  bool is_equal(const ItemStack& o) const {
    return item_id == o.item_id && (!has_subtypes() || damage == o.damage);
  }
  ItemStack copy() const { return *this; }
  // splitStack(var2): removes n, returns the taken stack.
  ItemStack split(int n) {
    if (n >= stack_size) {
      ItemStack out = *this;
      stack_size = 0;
      return out;
    }
    ItemStack out(item_id, n, damage);
    stack_size -= n;
    return out;
  }
};

}  // namespace craftpp::entity
