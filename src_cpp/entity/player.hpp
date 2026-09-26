#pragma once

#include <array>
#include <optional>
#include <string>

#include "entity/items.hpp"
#include "entity/living.hpp"

namespace craftpp::entity {

// Player capabilities (creative flight toggles; M5 menus flip these).
struct Capabilities {
  bool disable_damage = false;
  bool is_flying = false;
  bool allow_flying = false;
  bool deplete_buckets = false;
};

// Food stub: level/saturation/exhaustion accumulate; hunger effects
// (starve/regen) are M5. Default level 20 keeps sprint logic live.
struct FoodStats {
  int food_level = 20;
  float saturation = 5.0f;
  float exhaustion = 0.0f;
  void add_exhaustion(float f) { exhaustion += f; }
};

// 36 main + 4 armor slots (mirrors InventoryPlayer layout).
struct Inventory {
  std::array<std::optional<ItemStack>, 36> main{};
  std::array<std::optional<ItemStack>, 4> armor{};
  int current = 0;

  std::optional<ItemStack>* held() {
    if (current < 0 || current >= 36) return nullptr;
    return &main[current];
  }
  const std::optional<ItemStack>* held() const {
    if (current < 0 || current >= 36) return nullptr;
    return &main[current];
  }
  int held_id() const {
    const auto* h = held();
    return (h && h->has_value()) ? h->value().item_id : -1;
  }
  bool can_harvest(int block_id) const {
    const auto* h = held();
    const int id = (h && h->has_value()) ? h->value().item_id : -1;
    return world::edit::can_harvest(block_id, id);
  }
  float str_vs(int block_id) const {
    const auto* h = held();
    const int id = (h && h->has_value()) ? h->value().item_id : -1;
    return id < 0 ? 1.0f : world::edit::str_vs(id, block_id);
  }
  int damage_vs() const {
    const auto* h = held();
    const int id = (h && h->has_value()) ? h->value().item_id : -1;
    return id < 0 ? 1 : world::edit::damage_vs_entity(id);
  }
  int armor_value() const {
    int total = 0;
    for (const auto& s : armor) {
      if (s.has_value()) total += world::edit::armor_value(s->item_id);
    }
    return total;
  }
  void destroy_held() {
    if (auto* h = held()) *h = std::nullopt;
  }
  // InventoryPlayer.addItemStackToInventory (merges then empties; returns
  // true if anything moved; creative bucket-deplete skipped: survival).
  bool add_stack(ItemStack& stack) {
    if (stack.stack_size <= 0) return true;
    if (stack.damageable() && stack.damage != 0) {
      const int empty = first_empty();
      if (empty < 0) return false;
      main[empty] = stack.copy();
      stack.stack_size = 0;
      return true;
    }
    const int before = stack.stack_size;
    int left = before, prev = before + 1;
    while (left > 0 && left < prev) {
      prev = left;
      stack.stack_size = left;
      left = store_partial(stack);
    }
    stack.stack_size = left;
    return left < before;
  }

 private:
  int first_empty() const {
    for (int i = 0; i < 36; ++i)
      if (!main[i].has_value()) return i;
    return -1;
  }
  int store_existing(const ItemStack& stack) const {
    for (int i = 0; i < 36; ++i) {
      const auto& s = main[i];
      if (s.has_value() && s->item_id == stack.item_id && s->stackable() &&
          s->stack_size < s->max_stack() && s->stack_size < 64 &&
          (!s->has_subtypes() || s->damage == stack.damage))
        return i;
    }
    return -1;
  }
  // storePartialItemStack: returns the leftover count.
  int store_partial(ItemStack& stack) {
    const int id = stack.item_id;
    int left = stack.stack_size;
    if (stack.max_stack() == 1) {
      const int e = first_empty();
      if (e < 0) return left;
      main[e] = stack.copy();
      main[e]->stack_size = 1;
      return left - 1;
    }
    int slot = store_existing(stack);
    if (slot < 0) slot = first_empty();
    if (slot < 0) return left;
    if (!main[slot].has_value()) main[slot] = ItemStack(id, 0, stack.damage);
    auto& s = *main[slot];
    int room = s.max_stack() - s.stack_size;
    if (room > 64 - s.stack_size) room = 64 - s.stack_size;
    int move = left < room ? left : room;
    s.stack_size += move;
    return left - move;
  }
};

// EntityPlayer M4 surface: inventory, capabilities, food stub, item use,
// armor, melee. NBT/crafting/sleep/stats-score/XP-drops are M5.
class Player : public Living, public world::edit::Breaker, public ItemUser {
 public:
  explicit Player(EntityWorld* world);

  Inventory inventory;
  Capabilities capabilities;
  FoodStats food;
  std::string username;
  int dimension = 0;
  int score = 0;
  float current_xp = 0.0f;
  int total_xp = 0;
  int player_level = 0;
  bool sleeping = false;
  int item_in_use_count = 0;  // >0 while using (never in M4 tests)
  bool is_swinging = false;
  int swing_int = 0;
  float speed_on_ground = 0.1f;
  float speed_in_air = 0.02f;
  float camera_yaw = 0.0f;
  float prev_camera_yaw = 0.0f;
  int fly_toggle_timer = 0;

  float eye_height() const override { return 0.12f; }

  void on_living_update() override;
  void update_entity_action_state() override;

  // Breaker hooks
  float yaw() const override { return rotation_yaw; }
  void add_stat(int stat, int n) override { (void)stat; (void)n; }  // M5 stats
  void add_exhaustion(float f) override {
    if (!capabilities.disable_damage && !world->multiplayer()) food.add_exhaustion(f);
  }
  // ItemUser hooks
  JavaRandom& world_rand() override;
  void on_item_stat(int item_id, int kind) override {
    (void)item_id;
    (void)kind;
  }

  std::optional<ItemStack>* current_equipped() { return inventory.held(); }
  bool can_harvest_block(int block_id) const { return inventory.can_harvest(block_id); }
  float current_str_vs(int block_id) const {
    float v = inventory.str_vs(block_id);
    // Efficiency/haste need enchanted gear/potions (M5). Aqua affinity never
    // applies in M4 (no enchanted helmets).
    if (is_inside_of_material_water()) v /= 5.0f;
    if (!on_ground) v /= 5.0f;
    return v;
  }
  float strength_vs_block(int block_id) const {
    const float h = world::edit::hardness(block_id);
    if (h < 0.0f) return 0.0f;
    if (!can_harvest_block(block_id)) return 1.0f / h / 100.0f;
    return current_str_vs(block_id) / h / 30.0f;
  }
  int armor_points() const override { return inventory.armor_value(); }
  bool is_using_item() const { return item_in_use_count > 0; }
  int food_level() const { return food.food_level; }
  void destroy_current_equipped_item() { inventory.destroy_held(); }

  bool attack(DamageSource src, int amount) override;
  bool attack_ex(DamageSource src, int amount, Entity* attacker);
  void attack_target(Entity& target);
  void damage_armor(int amount);  // M5 (armor durability)
  void move_entity_with_heading(float strafe, float forward) override;
  void add_movement_stat(double dx, double dy, double dz);

 protected:
  void jump() override {
    Living::jump();
    add_exhaustion(is_sprinting() ? 0.8f : 0.2f);
  }

 protected:
  void damage_entity(DamageSource src, int amount) override;
  bool blocking_with_sword() const { return false; }  // M5 (needs itemInUse)

 protected:
  virtual void alert_wolves(Living& target, bool retaliate) {}  // M5 AI
  virtual void on_critical_hit(Entity& target) {}               // M6 particles
  virtual void on_magic_crit(Entity& target) {}                 // M6 particles
  virtual void on_overkill() {}                                 // M5 achievements
};

}  // namespace craftpp::entity
