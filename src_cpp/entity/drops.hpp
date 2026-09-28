#pragma once

#include "core/random.hpp"
#include "entity/entity.hpp"
#include "entity/items.hpp"
#include "world/blocks.hpp"

namespace craftpp::entity {

// Dropped item stack (EntityItem port, singleplayer subset). Physics reuses
// Entity.move_entity; magnet/pickup sweep runs in LiveWorld::tick (the
// source does it via entity collision with the player).
class DroppedItem : public Entity {
 public:
  ItemStack item;
  int age = 0;
  int pickup_delay = 0;
  int health = 5;
  // Hover/spin phase (EntityItem.field_804_d): random per drop, visual
  // only. Live drops draw from world rand (already perturbed by scatter);
  // loaded drops get 0 (the source re-randomizes on load, also visual).
  float hover_phase = 0.0f;

  DroppedItem(EntityWorld* w, double x, double y, double z, const ItemStack& stack)
      : Entity(w), item(stack) {
    width = 0.25f;
    height = 0.25f;
    y_offset = height / 2.0f;
    set_position(x, y, z);
    // Drop scatter (vanilla uses wild Math.random here; pinned to world
    // rand for determinism — never asserted, no gameplay effect).
    motion_x = (world->world_rand().next_float() - 0.5) * 0.2;
    motion_y = 0.2;
    motion_z = (world->world_rand().next_float() - 0.5) * 0.2;
    hover_phase =
        world->world_rand().next_float() * 2.0f * 3.14159265f;  // visual only
  }

  // Load path: explicit motion, no RNG draws (loading must not perturb the
  // world RNG the way live drops do).
  DroppedItem(EntityWorld* w, double x, double y, double z, const ItemStack& stack, double mx,
              double my, double mz)
      : Entity(w), item(stack) {
    width = 0.25f;
    height = 0.25f;
    y_offset = height / 2.0f;
    set_position(x, y, z);
    motion_x = mx;
    motion_y = my;
    motion_z = mz;
  }

  void on_update() override {
    if (pickup_delay > 0) --pickup_delay;
    motion_y -= 0.04;
    const int feet = world->block_id(floor_int(pos_x), floor_int(pos_y), floor_int(pos_z));
    if (world::bid::material_of(feet) == world::bid::Material::Lava) {
      motion_y = 0.2;
      motion_x = (world->world_rand().next_float() - world->world_rand().next_float()) * 0.2;
      motion_z = (world->world_rand().next_float() - world->world_rand().next_float()) * 0.2;
    }
    move_entity(motion_x, motion_y, motion_z);
    double friction = 0.98;
    if (on_ground) {
      friction = 0.1 * 0.1 * 58.8;
      const int ground =
          world->block_id(floor_int(pos_x), floor_int(bbox.min_y) - 1, floor_int(pos_z));
      if (ground > 0) friction = world::bid::block_slipperiness(ground) * 0.98;
    }
    motion_x *= friction;
    motion_y *= 0.98;
    motion_z *= friction;
    if (on_ground) motion_y *= -0.5;
    ++age;
    if (age >= 6000) set_entity_dead();
  }

  void damage(int n) {
    health -= n;
    if (health <= 0) set_entity_dead();
  }

  // Vanilla EntityItem spawn pop (visual arc out of the mined block/mob).
  // Wild Math.random in the source; pinned draws here (never asserted).
  static void spawn_pop(EntityWorld* w, double& mx, double& my, double& mz) {
    JavaRandom& r = w->world_rand();
    mx = (r.next_float() - 0.5) * 0.2;
    my = 0.2;
    mz = (r.next_float() - 0.5) * 0.2;
  }

 private:
  static int floor_int(double v) {
    const int i = static_cast<int>(v);
    return v < 0.0 && v != i ? i - 1 : i;
  }
};

}  // namespace craftpp::entity
