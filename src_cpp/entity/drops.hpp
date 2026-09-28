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
    // Vanilla EntityItem sets prevPos = pos every tick (before integrating
    // motion); without this the render lerp oscillates spawn<->pos.
    prev_pos_x = pos_x;
    prev_pos_y = pos_y;
    prev_pos_z = pos_z;
    motion_y -= 0.04;
    const int feet = world->block_id(floor_int(pos_x), floor_int(pos_y), floor_int(pos_z));
    if (world::bid::material_of(feet) == world::bid::Material::Lava) {
      motion_y = 0.2;
      motion_x = (world->world_rand().next_float() - world->world_rand().next_float()) * 0.2;
      motion_z = (world->world_rand().next_float() - world->world_rand().next_float()) * 0.2;
    }
    push_out_of_blocks();
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

  static bool is_normal_cube_at(EntityWorld* w, int x, int y, int z) {
    const int id = w->block_id(x, y, z);
    return id != 0 && world::bid::is_opaque(id) && world::bid::renders_as_normal(id);
  }

  // Entity.pushOutOfBlocks: when the item center sits inside a normal cube,
  // shove motion toward the nearest open face (visual draws only; the
  // source uses wild rand, pinned here).
  void push_out_of_blocks() {
    const double cx = pos_x, cyy = (bbox.min_y + bbox.max_y) / 2.0, cz = pos_z;
    const int bx = floor_int(cx), by = floor_int(cyy), bz = floor_int(cz);
    if (!is_normal_cube_at(world, bx, by, bz)) return;
    const double fx = cx - bx, fy = cyy - by, fz = cz - bz;
    int dir = -1;
    double best = 9999.0;
    if (!is_normal_cube_at(world, bx - 1, by, bz) && fx < best) {
      best = fx;
      dir = 0;
    }
    if (!is_normal_cube_at(world, bx + 1, by, bz) && 1.0 - fx < best) {
      best = 1.0 - fx;
      dir = 1;
    }
    if (!is_normal_cube_at(world, bx, by - 1, bz) && fy < best) {
      best = fy;
      dir = 2;
    }
    if (!is_normal_cube_at(world, bx, by + 1, bz) && 1.0 - fy < best) {
      best = 1.0 - fy;
      dir = 3;
    }
    if (!is_normal_cube_at(world, bx, by, bz - 1) && fz < best) {
      best = fz;
      dir = 4;
    }
    if (!is_normal_cube_at(world, bx, by, bz + 1) && 1.0 - fz < best) {
      best = 1.0 - fz;
      dir = 5;
    }
    if (dir < 0) return;
    const double push = world->world_rand().next_float() * 0.2 + 0.1;
    if (dir == 0) motion_x = -push;
    if (dir == 1) motion_x = push;
    if (dir == 2) motion_y = -push;
    if (dir == 3) motion_y = push;
    if (dir == 4) motion_z = -push;
    if (dir == 5) motion_z = push;
  }
};

}  // namespace craftpp::entity
