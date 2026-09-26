#include "entity/pig.hpp"

#include <cmath>

#include "core/math_helper.hpp"
#include "entity/mob_loot.hpp"
#include "world/block_place.hpp"
#include "world/blocks.hpp"
#include "world/item_ids.hpp"
#include "world/tick.hpp"

namespace craftpp::entity {

namespace {
world::edit::EditWorld* as_edit(EntityWorld* w) {
  return dynamic_cast<world::edit::EditWorld*>(w);
}
}  // namespace

void Pig::on_death(DamageSource src) {
  const int id = (fire > 0) ? iid::kPorkCooked : get_drop_item_id();
  drop_mob_loot(*this, id);
  Living::on_death(src);
}

void Pig::update_entity_action_state() {
  Living::update_entity_action_state();  // yaw wander baseline
  // EntityCreature resets isJumping every tick and only jumps when the
  // path rises; approximate with a head-height probe ahead.
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
  // Creature stroll approximation: walk at moveSpeed in bursts, pause on
  // bumps (vanilla uses real paths; same pace, same pauses).
  if (move_forward <= 0.0f) {
    if (rand.next_float() < 0.05f) move_forward = move_speed;
  } else if (collided_horizontally || rand.next_float() < 0.02f) {
    move_forward = 0.0f;
  }
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

}  // namespace craftpp::entity
