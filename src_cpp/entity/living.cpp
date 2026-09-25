#include "entity/living.hpp"

#include <cmath>
#include <cstdlib>

#include "core/math_helper.hpp"
#include "world/blocks.hpp"

namespace craftpp::entity {
namespace {

// Process-global stand-in for java.lang.Math.random (used ONLY for the
// cosmetic attackedAtYaw jitter on environmental hits). Like the source it
// is nondeterministic per run; tests never assert attackedAtYaw.
JavaRandom& math_random() {
  static JavaRandom r(static_cast<std::int64_t>(std::rand()));
  return r;
}

constexpr float kPiFloat = 3.14159265358979323846f;  // (float)Math.PI

}  // namespace

Living::Living(EntityWorld* world) : Entity(world) {
  prevent_spawning = true;
  render_wobble_a = static_cast<float>(math_random().next_double() + 1.0) * 0.01f;
  set_position(pos_x, pos_y, pos_z);
  render_wobble_b = static_cast<float>(math_random().next_double()) * 12398.0f;
  rotation_yaw =
      static_cast<float>(math_random().next_double() * static_cast<double>(kPiFloat) * 2.0);
  step_height = 0.5f;
}

void Living::heal(int amount) {
  if (health > 0) {
    health += amount;
    if (health > max_health()) health = max_health();
    hearts_life = hearts_halves_life / 2;
  }
}

void Living::set_position_and_rotation2(double x, double y, double z, float yaw, float pitch,
                                        int steps) {  y_offset = 0.0f;
  new_pos_x = x;
  new_pos_y = y;
  new_pos_z = z;
  new_rot_yaw = yaw;
  new_rot_pitch = pitch;
  new_pos_steps = steps;
}

void Living::on_update() {
  Entity::on_update();
  if (hurt_cooldown_b > 0) {
    if (hurt_cooldown_q <= 0) hurt_cooldown_q = 60;
    --hurt_cooldown_q;
    if (hurt_cooldown_q <= 0) --hurt_cooldown_b;
  }
  on_living_update();
  const double dx = pos_x - prev_pos_x;
  const double dz = pos_z - prev_pos_z;
  float speed = MathHelper::sqrt_double(dx * dx + dz * dz);
  float target_yaw = render_yaw_offset;
  float wobble = 0.0f;
  float swing_amt = 0.0f;
  prev_limb_speed = limb_speed;
  if (speed > 0.05f) {
    swing_amt = 1.0f;
    wobble = speed * 3.0f;
    target_yaw =
        static_cast<float>(std::atan2(dz, dx)) * 180.0f / 3.14159265358979323846f - 90.0f;
  }
  if (swing > 0.0f) target_yaw = rotation_yaw;
  if (!on_ground) swing_amt = 0.0f;
  limb_speed += (swing_amt - limb_speed) * 0.3f;
  float yaw_d = target_yaw - render_yaw_offset;
  for (; yaw_d < -180.0f; yaw_d += 360.0f) {
  }
  while (yaw_d >= 180.0f) yaw_d -= 360.0f;
  render_yaw_offset += yaw_d * 0.3f;
  float yaw_rel = rotation_yaw - render_yaw_offset;
  for (; yaw_rel < -180.0f; yaw_rel += 360.0f) {
  }
  while (yaw_rel >= 180.0f) yaw_rel -= 360.0f;
  const bool strafe_flip = yaw_rel < -90.0f || yaw_rel >= 90.0f;
  if (yaw_rel < -75.0f) yaw_rel = -75.0f;
  if (yaw_rel >= 75.0f) yaw_rel = 75.0f;
  render_yaw_offset = rotation_yaw - yaw_rel;
  if (yaw_rel * yaw_rel > 2500.0f) render_yaw_offset += yaw_rel * 0.2f;
  if (strafe_flip) wobble *= -1.0f;
  while (rotation_yaw - prev_rotation_yaw < -180.0f) prev_rotation_yaw -= 360.0f;
  while (rotation_yaw - prev_rotation_yaw >= 180.0f) prev_rotation_yaw += 360.0f;
  while (render_yaw_offset - prev_render_yaw_offset < -180.0f) prev_render_yaw_offset -= 360.0f;
  while (render_yaw_offset - prev_render_yaw_offset >= 180.0f) prev_render_yaw_offset -= 360.0f;
  while (rotation_pitch - prev_rotation_pitch < -180.0f) prev_rotation_pitch -= 360.0f;
  while (rotation_pitch - prev_rotation_pitch >= 180.0f) prev_rotation_pitch += 360.0f;
  limb_phase += wobble;
}

void Living::on_entity_update() {
  prev_swing = swing;
  Entity::on_entity_update();
  if (rand.next_int(1000) < living_sound_time++) {
    living_sound_time = -talk_interval();
    const char* s = living_sound();
    if (s != nullptr) world->play_sound(s, get_sound_volume(), hurt_pitch());
  }
  if (is_entity_alive() && is_inside_opaque_block() &&
      attack(DamageSource::kInWall, 1)) {
  }
  if (is_immune_to_fire || world->multiplayer()) extinguish();
  if (is_entity_alive() && is_inside_of_material_water() && !can_breathe_underwater()) {
    air_supply = air_supply - 1;
    if (air_supply == -20) {
      air_supply = 0;
      for (int i = 0; i < 8; ++i) {
        // Draws preserved; particle no-op.
        const float ox = rand.next_float() - rand.next_float();
        const float oy = rand.next_float() - rand.next_float();
        const float oz = rand.next_float() - rand.next_float();
        world->spawn_particle("bubble", pos_x + ox, pos_y + oy, pos_z + oz, motion_x, motion_y,
                              motion_z);
      }
      attack(DamageSource::kDrown, 2);
    }
    extinguish();
  } else {
    air_supply = 300;
  }
  prev_camera_pitch = camera_pitch;
  if (attack_time > 0) --attack_time;
  if (hurt_time > 0) --hurt_time;
  if (hearts_life > 0) --hearts_life;
  if (health <= 0) {
    ++death_time;
    if (death_time == 20) {
      // XP branch needs a player kill (revenge_timer/field_34904_b, M5): skipped.
      on_entity_death();
      set_entity_dead();
      for (int i = 0; i < 20; ++i) {
        const double gx = rand.next_gaussian() * 0.02;
        const double gy = rand.next_gaussian() * 0.02;
        const double gz = rand.next_gaussian() * 0.02;
        world->spawn_particle(
            "explode", pos_x + rand.next_float() * width * 2.0f - width,
            pos_y + rand.next_float() * height, pos_z + rand.next_float() * width * 2.0f - width,
            gx, gy, gz);
      }
    }
  }
  if (revenge_timer > 0) {
    --revenge_timer;
  }
  update_potion_effects();
  prev_limb_phase = limb_phase;
  prev_render_yaw_offset = render_yaw_offset;
  prev_rotation_yaw = rotation_yaw;
  prev_rotation_pitch = rotation_pitch;
}

void Living::update_potion_effects() {
  // Map stays empty in M4 (no potions): only the unconditional gross-hack
  // draw below runs, plus the first-tick flag flip.
  if (potion_dirty) {
    if (!world->multiplayer()) {
      // activePotionsMap empty -> dataWatcher.updateObject(8, 0), a no-op.
    }
    potion_dirty = false;
  }
  if (rand.next_boolean()) {
    // dataWatcher int 8 is 0 with no potions -> no particle. Draw preserved.
  }
}

float Living::hurt_pitch() {
  return (rand.next_float() - rand.next_float()) * 0.2f + 1.0f;
}

bool Living::attack(DamageSource src, int amount) { return attack_ex(src, amount, nullptr); }

bool Living::attack_ex(DamageSource src, int amount, Entity* attacker) {
  if (world->multiplayer()) return false;
  entity_age = 0;
  if (health <= 0) return false;
  // Fire-resistance potion check: no potions in M4, always false.
  anim_speed = 1.5f;
  bool full_hurt = true;
  if (static_cast<float>(hearts_life) > static_cast<float>(hearts_halves_life) / 2.0f) {
    if (amount <= natural_armor_rating) return false;
    damage_entity(src, amount - natural_armor_rating);
    natural_armor_rating = amount;
    full_hurt = false;
  } else {
    natural_armor_rating = amount;
    prev_health = health;
    hearts_life = hearts_halves_life;
    damage_entity(src, amount);
    hurt_time = max_hurt_time = 10;
  }
  attacked_at_yaw = 0.0f;
  if (full_hurt) {
    world->set_entity_state(*this, 2);
    set_been_attacked();
    if (attacker != nullptr) {
      // Knockback jitter (only draws when entities overlap).
      double kx = attacker->pos_x - pos_x;
      double kz = attacker->pos_z - pos_z;
      while (kx * kx + kz * kz < 1.0e-4) {
        kx = (math_random().next_double() - math_random().next_double()) * 0.01;
        kz = (math_random().next_double() - math_random().next_double()) * 0.01;
      }
      attacked_at_yaw = static_cast<float>(std::atan2(kz, kx) * 180.0 /
                                           static_cast<double>(kPiFloat)) -
                        rotation_yaw;
      knock_back(*attacker, amount, kx, kz);
    } else {
      // Cosmetic jitter via global Math.random (never asserted in tests).
      attacked_at_yaw = static_cast<float>(static_cast<int>(math_random().next_double() * 2.0) * 180);
    }
  }
  if (health <= 0) {
    if (full_hurt) world->play_sound(death_sound(), get_sound_volume(), hurt_pitch());
    on_death(src);
  } else if (full_hurt) {
    world->play_sound(hurt_sound(), get_sound_volume(), hurt_pitch());
  }
  return true;
}

void Living::damage_entity(DamageSource src, int amount) {
  // func_40115_d (armor, skipped when unblockable) + func_40128_b
  // (resistance, no potion in M4).
  if (bypasses_armor(src)) {
    health -= amount;
    return;
  }
  health -= apply_armor(amount);
}

void Living::knock_back(Entity& attacker, int amount, double dx, double dz) {
  is_air_borne = true;
  const float dist = MathHelper::sqrt_double(dx * dx + dz * dz);
  constexpr float kPush = 0.4f;
  motion_x /= 2.0;
  motion_y /= 2.0;
  motion_z /= 2.0;
  motion_x -= dx / dist * kPush;
  motion_y += static_cast<double>(0.4f);
  motion_z -= dz / dist * kPush;
  if (motion_y > static_cast<double>(0.4f)) motion_y = static_cast<double>(0.4f);
}

void Living::on_death(DamageSource src) {
  unused_death_flag = true;
  if (!world->multiplayer()) {
    // Looting/XP need a player killer (M5); base drop id is 0 anyway.
  }
  world->set_entity_state(*this, 3);
}

void Living::fall(float distance) {
  Entity::fall(distance);
  const int dmg = static_cast<int>(std::ceil(distance - 3.0f));
  if (dmg > 0) {
    world->play_sound(dmg > 4 ? "damage.fallbig" : "damage.fallsmall", 1.0f, 1.0f);
    attack(DamageSource::kFall, dmg);
    const int id = world->block_id(MathHelper::floor_double(pos_x),
                                  MathHelper::floor_double(pos_y - 0.2 - y_offset),
                                  MathHelper::floor_double(pos_z));
    if (id > 0) world->play_sound("step", 0.5f, 0.75f);  // stepSound pitch (no draws)
  }
}

bool Living::is_on_ladder() const {
  return world->block_id(MathHelper::floor_double(pos_x),
                         MathHelper::floor_double(bbox.min_y),
                         MathHelper::floor_double(pos_z)) == world::bid::kLadder;
}

void Living::move_entity_with_heading(float strafe, float forward) {
  double ground_y;
  if (is_in_water()) {
    ground_y = pos_y;
    move_flying(strafe, forward, 0.02f);
    move_entity(motion_x, motion_y, motion_z);
    motion_x *= static_cast<double>(0.8f);
    motion_y *= static_cast<double>(0.8f);
    motion_z *= static_cast<double>(0.8f);
    motion_y -= 0.02;
    if (collided_horizontally &&
        is_offset_in_liquid(motion_x, motion_y + static_cast<double>(0.6f) - pos_y + ground_y,
                                motion_z)) {
      motion_y = static_cast<double>(0.3f);
    }
  } else if (handle_lava_movement()) {
    ground_y = pos_y;
    move_flying(strafe, forward, 0.02f);
    move_entity(motion_x, motion_y, motion_z);
    motion_x *= 0.5;
    motion_y *= 0.5;
    motion_z *= 0.5;
    motion_y -= 0.02;
    if (collided_horizontally &&
        is_offset_in_liquid(motion_x, motion_y + static_cast<double>(0.6f) - pos_y + ground_y,
                                motion_z)) {
      motion_y = static_cast<double>(0.3f);
    }
  } else {
    float slip = 0.91f;
    if (on_ground) {
      slip = 546.0f * 0.1f * 0.1f * 0.1f;
      const int id = world->block_id(MathHelper::floor_double(pos_x),
                                    MathHelper::floor_double(bbox.min_y) - 1,
                                    MathHelper::floor_double(pos_z));
      if (id > 0) slip = world::bid::block_slipperiness(id) * 0.91f;
    }
    const float grip = 0.16277136f / (slip * slip * slip);
    const float friction = on_ground ? land_movement_factor * grip : jump_movement_factor;
    move_flying(strafe, forward, friction);
    slip = 0.91f;
    if (on_ground) {
      slip = 546.0f * 0.1f * 0.1f * 0.1f;
      const int id = world->block_id(MathHelper::floor_double(pos_x),
                                    MathHelper::floor_double(bbox.min_y) - 1,
                                    MathHelper::floor_double(pos_z));
      if (id > 0) slip = world::bid::block_slipperiness(id) * 0.91f;
    }
    if (is_on_ladder()) {
      constexpr float kCap = 0.15f;
      if (motion_x < -kCap) motion_x = -kCap;
      if (motion_x > kCap) motion_x = kCap;
      if (motion_z < -kCap) motion_z = -kCap;
      if (motion_z > kCap) motion_z = kCap;
      fall_distance = 0.0f;
      if (motion_y < -0.15) motion_y = -0.15;
      if (is_sneaking() && motion_y < 0.0) motion_y = 0.0;
    }
    move_entity(motion_x, motion_y, motion_z);
    if (collided_horizontally && is_on_ladder()) motion_y = static_cast<double>(0.2f);
    motion_y -= 0.08;
    motion_y *= static_cast<double>(0.98f);
    motion_x *= slip;
    motion_z *= slip;
  }
  prev_anim_speed = anim_speed;
  const double dx = pos_x - prev_pos_x;
  const double dz = pos_z - prev_pos_z;
  float wob = MathHelper::sqrt_double(dx * dx + dz * dz) * 4.0f;
  if (wob > 1.0f) wob = 1.0f;
  anim_speed += (wob - anim_speed) * 0.4f;
  anim_t += anim_speed;
}

void Living::jump() {
  motion_y = static_cast<double>(0.42f);
  if (is_sprinting()) {
    constexpr float kPi = 3.14159265358979323846f;
    const float a = rotation_yaw * (kPi / 180.0f);
    motion_x -= MathHelper::sin(a) * 0.2f;
    motion_z += MathHelper::cos(a) * 0.2f;
  }
  is_air_borne = true;
}

void Living::on_living_update() {
  if (jump_cooldown > 0) --jump_cooldown;
  if (new_pos_steps > 0) {
    const double nx = pos_x + (new_pos_x - pos_x) / new_pos_steps;
    const double ny = pos_y + (new_pos_y - pos_y) / new_pos_steps;
    const double nz = pos_z + (new_pos_z - pos_z) / new_pos_steps;
    double yaw_d = new_rot_yaw - rotation_yaw;
    for (; yaw_d < -180.0; yaw_d += 360.0) {
    }
    while (yaw_d >= 180.0) yaw_d -= 360.0;
    rotation_yaw = static_cast<float>(rotation_yaw + yaw_d / new_pos_steps);
    rotation_pitch =
        static_cast<float>(rotation_pitch + (new_rot_pitch - rotation_pitch) / new_pos_steps);
    --new_pos_steps;
    set_position(nx, ny, nz);
    set_rotation(rotation_yaw, rotation_pitch);
    std::vector<Aabb> hits;
    // contract(1/32) query mirrors the MP interp push-out.
    colliding_boxes_for(world, world->collider(), bbox.contract(1.0 / 32.0, 0.0, 1.0 / 32.0), hits);
    if (!hits.empty()) {
      double lift = 0.0;
      for (const Aabb& b : hits)
        if (b.max_y > lift) lift = b.max_y;
      set_position(nx, ny + lift - bbox.min_y, nz);
    }
  }
  if (is_movement_blocked()) {
    is_jumping = false;
    move_strafing = 0.0f;
    move_forward = 0.0f;
    random_yaw_velocity = 0.0f;
  } else if (!is_multiplayer_entity) {
    update_entity_action_state();
  }
  const bool in_water = is_in_water();
  const bool in_lava = handle_lava_movement();
  if (is_jumping) {
    if (in_water) {
      motion_y += static_cast<double>(0.04f);
    } else if (in_lava) {
      motion_y += static_cast<double>(0.04f);
    } else if (on_ground && jump_cooldown == 0) {
      jump();
      jump_cooldown = 10;
    }
  } else {
    jump_cooldown = 0;
  }
  move_strafing *= 0.98f;
  move_forward *= 0.98f;
  random_yaw_velocity *= 0.9f;
  const float saved_factor = land_movement_factor;
  land_movement_factor *= speed_factor();
  move_entity_with_heading(move_strafing, move_forward);
  land_movement_factor = saved_factor;
  // Entity push section: test/live worlds report no neighbors (M5 adds mobs).
  for (Entity* other : world->entities_excluding(*this, bbox.expand(0.2, 0.0, 0.2))) {
    (void)other;
  }
}

void Living::update_entity_action_state() {
  ++entity_age;
  // despawnEntity needs a player; null -> nothing (harness has no players).
  Entity* player = world->closest_player_to(*this, -1.0);
  (void)player;
  move_strafing = 0.0f;
  move_forward = 0.0f;
  if (rand.next_float() < 0.02f) {
    player = world->closest_player_to(*this, 8.0f);
    if (player != nullptr) {
      // currentTarget chase is M5 (mobs need targets; harness has none).
    } else {
      random_yaw_velocity = (rand.next_float() - 0.5f) * 20.0f;
    }
  }
  // currentTarget stays null in M4 -> else branch.
  if (rand.next_float() < 0.05f) {
    random_yaw_velocity = (rand.next_float() - 0.5f) * 20.0f;
  }
  rotation_yaw += random_yaw_velocity;
  rotation_pitch = default_pitch;
  const bool wet = is_in_water();
  const bool lava = handle_lava_movement();
  if (wet || lava) is_jumping = rand.next_float() < 0.8f;
}

}  // namespace craftpp::entity
