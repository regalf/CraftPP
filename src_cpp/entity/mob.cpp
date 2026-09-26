#include "entity/mob.hpp"

#include <cmath>

#include "core/math_helper.hpp"
#include "entity/player.hpp"
#include "world/block_place.hpp"
#include "world/blocks.hpp"
#include "world/item_ids.hpp"
#include "world/tick.hpp"

namespace craftpp::entity {

namespace {
world::edit::EditWorld* as_edit(EntityWorld* w) {
  return dynamic_cast<world::edit::EditWorld*>(w);
}

void drop_loot(Living& self, int item_id) {
  auto* w = as_edit(self.world);
  if (w == nullptr) return;
  const int n = self.rand.next_int(3);
  for (int i = 0; i < n; ++i) {
    w->on_item_drop(item_id, 1, 0, self.pos_x, self.pos_y, self.pos_z, 0.0, 0.0, 0.0);
  }
}

// lightBrightnessTable (WorldProviderSurface, var1 = 0.2).
float brightness_table(int light) {
  const float v = 1.0f - static_cast<float>(light) / 15.0f;
  return (1.0f - v) / (v * 3.0f + 1.0f) * 0.8f + 0.2f;
}
}  // namespace

void Pig::on_death(DamageSource src) {
  const int id = (fire > 0) ? iid::kPorkCooked : get_drop_item_id();
  drop_loot(*this, id);
  Living::on_death(src);
}

bool Pig::can_spawn_here() {
  auto* w = as_edit(world);
  if (w == nullptr) return false;
  const int x = MathHelper::floor_double(pos_x);
  const int y = MathHelper::floor_double(bbox.min_y);
  const int z = MathHelper::floor_double(pos_z);
  if (w->block_id(x, y - 1, z) != world::bid::kGrass) return false;
  if (world::tick::full_light_value(*w, x, y, z) <= 8) return false;
  // Base AABB checks approximated: feet + head must be non-opaque.
  if (world::bid::is_opaque(w->block_id(x, y, z))) return false;
  if (world::bid::is_opaque(w->block_id(x, y + 1, z))) return false;
  return true;
}

void Zombie::on_death(DamageSource src) {
  drop_loot(*this, get_drop_item_id());
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
  if (target == nullptr) return;
  const double dx = target->pos_x - pos_x;
  const double dz = target->pos_z - pos_z;
  const double d2 = dx * dx + dz * dz;
  rotation_yaw =
      static_cast<float>(std::atan2(dz, dx) * 180.0 / 3.141592653589793) - 90.0f;
  move_forward = 1.0f;
  if (attack_time <= 0 && d2 < 4.0 &&
      target->bbox.max_y > bbox.min_y && target->bbox.min_y < bbox.max_y) {
    attack_time = 20;
    if (auto* living = dynamic_cast<Living*>(target)) {
      living->attack_ex(DamageSource::kMob, attack_strength, this);
      living->knock_back(*this, attack_strength, dx, dz);
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
