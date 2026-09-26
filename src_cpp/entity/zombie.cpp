#include "entity/zombie.hpp"

#include <cmath>

#include "core/math_helper.hpp"
#include "entity/mob_loot.hpp"
#include "world/block_place.hpp"
#include "world/blocks.hpp"
#include "world/tick.hpp"

namespace craftpp::entity {

namespace {
world::edit::EditWorld* as_edit(EntityWorld* w) {
  return dynamic_cast<world::edit::EditWorld*>(w);
}

// lightBrightnessTable (WorldProviderSurface, var1 = 0.2).
float brightness_table(int light) {
  const float v = 1.0f - static_cast<float>(light) / 15.0f;
  return (1.0f - v) / (v * 3.0f + 1.0f) * 0.8f + 0.2f;
}
}  // namespace

void Zombie::on_death(DamageSource src) {
  drop_mob_loot(*this, get_drop_item_id());
  Living::on_death(src);
}

void Zombie::on_living_update() {
  // Daylight burn (EntityZombie.onLivingUpdate).
  auto* w = as_edit(world);
  if (w != nullptr && w->skylight_sub() < 4) {
    const int x = MathHelper::floor_double(pos_x);
    const int z = MathHelper::floor_double(pos_z);
    const int y = MathHelper::floor_double(pos_y - y_offset + height * 0.66);
    const float b = brightness_table(world::tick::block_light_value(*w, x, y, z));
    if (b > 0.5f && w->can_see_sky(x, MathHelper::floor_double(pos_y),
                                   z) &&
        rand.next_float() * 30.0f < (b - 0.4f) * 2.0f) {
      const int ticks = 8 * 20;
      if (fire < ticks) fire = ticks;
    }
  }
  Living::on_living_update();
}

void Zombie::update_entity_action_state() {
  Living::update_entity_action_state();  // wander baseline + entityAge
  // EntityCreature resets isJumping every tick; jump only when blocked by
  // something taller than one step (head-height probe ahead).
  is_jumping = false;
  if (collided_horizontally) {
    auto* w = as_edit(world);
    if (w != nullptr) {
      const double rad = rotation_yaw * 3.141592653589793 / 180.0;
      const int ax = MathHelper::floor_double(pos_x - std::sin(rad) * (width + 0.5));
      const int az = MathHelper::floor_double(pos_z + std::cos(rad) * (width + 0.5));
      const int feet = MathHelper::floor_double(bbox.min_y);
      // Hop only onto a single step: solid at feet, air for two above.
      if (w->is_normal_cube(ax, feet, az) && !w->is_normal_cube(ax, feet + 1, az) &&
          !w->is_normal_cube(ax, feet + 2, az))
        is_jumping = true;
    }
  }
  // Despawn like the source (needs a player; null-safe).
  Entity* p0 = world->closest_player_to(*this, -1.0);
  if (p0 != nullptr) {
    const double dx = p0->pos_x - pos_x, dy = p0->pos_y - pos_y, dz = p0->pos_z - pos_z;
    const double d2 = dx * dx + dy * dy + dz * dz;
    if (d2 > 16384.0) {
      set_entity_dead();
      return;
    }
    if (entity_age > 600 && rand.next_int(800) == 0 && d2 > 1024.0) {
      set_entity_dead();
      return;
    }
    if (d2 < 1024.0) entity_age = 0;
  }
  // Seek the nearest player within 16 blocks.
  Entity* target = world->closest_player_to(*this, 16.0);
  if (target == nullptr) {
    // No target: stroll like pigs do (vanilla wanders via paths).
    if (move_forward <= 0.0f) {
      if (rand.next_float() < 0.05f) move_forward = move_speed;
    } else if (rand.next_float() < 0.02f) {
      move_forward = 0.0f;
    }
    if (collided_horizontally) {
      // Bump into a wall: turn to slide along it instead of pushing
      // forever (poor man's wall following; real paths are M5+).
      rotation_yaw += (rand.next_float() - 0.5f) * 120.0f;
      if (move_forward <= 0.0f) move_forward = move_speed;
    }
    return;
  }
  const double dx = target->pos_x - pos_x;
  const double dz = target->pos_z - pos_z;
  const double d2 = dx * dx + dz * dz;
  rotation_yaw =
      static_cast<float>(std::atan2(dz, dx) * 180.0 / 3.141592653589793) - 90.0f;
  move_forward = move_speed;
  if (attack_time <= 0 && d2 < 4.0 &&
      target->bbox.max_y > bbox.min_y && target->bbox.min_y < bbox.max_y) {
    attack_time = 20;
    // attack_ex applies damage + knockback itself (all living-vs-living
    // hits knock back; environmental hits have no attacker and don't).
    if (auto* living = dynamic_cast<Living*>(target)) {
      living->attack_ex(DamageSource::kMob, attack_strength, this);
    }
  }
}

bool Zombie::can_spawn_here() {
  auto* w = as_edit(world);
  if (w == nullptr) return false;
  const int x = MathHelper::floor_double(pos_x);
  const int y = MathHelper::floor_double(bbox.min_y);
  const int z = MathHelper::floor_double(pos_z);
  // func_40147_Y (no thunder in M5: skip the skylight override branch).
  if (w->saved_sky(x, y, z) > rand.next_int(32)) return false;
  if (world::tick::block_light_value(*w, x, y, z) > rand.next_int(8)) return false;
  if (world::bid::is_opaque(w->block_id(x, y, z))) return false;
  if (world::bid::is_opaque(w->block_id(x, y + 1, z))) return false;
  return true;
}

}  // namespace craftpp::entity
