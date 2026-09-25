#include "entity/player.hpp"

#include <cmath>

#include "core/math_helper.hpp"

namespace craftpp::entity {

Player::Player(EntityWorld* world) : Living(world) {
  y_offset = 1.62f;
  fire_resistance = 20;
  set_size(0.6f, 1.8f);
  set_position(pos_x, pos_y, pos_z);
}

void Player::update_entity_action_state() {
  // EntityPlayer version: swing-arm progress only (no super call, no draws;
  // digestion duration 6 without haste/fatigue potions).
  constexpr int kDur = 6;
  if (is_swinging) {
    ++swing_int;
    if (swing_int >= kDur) {
      swing_int = 0;
      is_swinging = false;
    }
  } else {
    swing_int = 0;
  }
  swing = static_cast<float>(swing_int) / static_cast<float>(kDur);
}

void Player::on_living_update() {
  if (fly_toggle_timer > 0) --fly_toggle_timer;
  // Peaceful heal (difficulty 0) is M5 hunger-adjacent; test worlds are normal.
  for (auto& slot : inventory.main) {
    if (slot.has_value() && slot->animations_to_go > 0) --slot->animations_to_go;
  }
  prev_camera_yaw = camera_yaw;
  Living::on_living_update();
  land_movement_factor = speed_on_ground;
  jump_movement_factor = speed_in_air;
  if (is_sprinting()) {
    land_movement_factor =
        static_cast<float>(land_movement_factor + speed_on_ground * 0.3);
    jump_movement_factor =
        static_cast<float>(jump_movement_factor + speed_in_air * 0.3);
  }
  float planar = MathHelper::sqrt_double(motion_x * motion_x + motion_z * motion_z);
  float pitch_t = static_cast<float>(std::atan(-motion_y * 0.2)) * 15.0f;
  if (planar > 0.1f) planar = 0.1f;
  if (!on_ground || health <= 0) planar = 0.0f;
  if (on_ground || health <= 0) pitch_t = 0.0f;
  camera_yaw += (planar - camera_yaw) * 0.4f;
  camera_pitch += (pitch_t - camera_pitch) * 0.8f;
  // Item pickup sweep (empty in M4 tests; M5 mobs/items ride the hook).
  if (health > 0) {
    for (Entity* other : world->entities_excluding(*this, bbox.expand(1.0, 0.0, 1.0))) {
      (void)other;  // collideWithPlayer -> onCollideWithPlayer is M5 (items/mobs)
    }
  }
}

JavaRandom& Player::world_rand() {
  // M4 worlds implement EditWorld (which extends EntityWorld).
  return static_cast<world::edit::EditWorld*>(world)->world_rand();
}

bool Player::attack(DamageSource src, int amount) {
  return attack_ex(src, amount, nullptr);
}

bool Player::attack_ex(DamageSource src, int amount, Entity* attacker) {
  if (world->multiplayer()) return false;
  entity_age = 0;
  if (health <= 0) return false;
  // Sleeping wake-up is M5 (sleeping never true in M4).
  // Difficulty scaling applies to mob/arrow attackers only (none in M4).
  if (amount == 0) return false;
  if (attacker != nullptr) {
    if (auto* living = dynamic_cast<Living*>(attacker)) alert_wolves(*living, false);
  }
  // damageTakenStat hook (M5 stats): skipped, no RNG.
  return Living::attack_ex(src, amount, attacker);
}

void Player::damage_armor(int amount) {
  (void)amount;  // M5 (armor durability)
}

void Player::move_entity_with_heading(float strafe, float forward) {
  const double start_x = pos_x;
  const double start_y = pos_y;
  const double start_z = pos_z;
  if (capabilities.is_flying) {
    const double saved_my = motion_y;
    const float saved_jump = jump_movement_factor;
    jump_movement_factor = 0.05f;
    Living::move_entity_with_heading(strafe, forward);
    motion_y = saved_my * 0.6;
    jump_movement_factor = saved_jump;
  } else {
    Living::move_entity_with_heading(strafe, forward);
  }
  add_movement_stat(pos_x - start_x, pos_y - start_y, pos_z - start_z);
}

void Player::add_movement_stat(double dx, double dy, double dz) {
  // Riding is M5 (pointers always null here).
  if (is_inside_of_material_water()) {
    const int d =
        static_cast<int>(std::floor(MathHelper::sqrt_double(dx * dx + dy * dy + dz * dz) * 100.0f +
                                    0.5f));
    if (d > 0) add_exhaustion(0.015f * static_cast<float>(d) * 0.01f);
  } else if (is_in_water()) {
    const int d =
        static_cast<int>(std::floor(MathHelper::sqrt_double(dx * dx + dz * dz) * 100.0f + 0.5f));
    if (d > 0) add_exhaustion(0.015f * static_cast<float>(d) * 0.01f);
  } else if (is_on_ladder()) {
    (void)dy;  // climb stat (M5 stats)
  } else if (on_ground) {
    const int d =
        static_cast<int>(std::floor(MathHelper::sqrt_double(dx * dx + dz * dz) * 100.0f + 0.5f));
    if (d > 0) {
      if (is_sprinting()) {
        add_exhaustion(10.0f * 0.01f * static_cast<float>(d) * 0.01f);
      } else {
        add_exhaustion(0.01f * static_cast<float>(d) * 0.01f);
      }
    }
  } else {
    const int d =
        static_cast<int>(std::floor(MathHelper::sqrt_double(dx * dx + dz * dz) * 100.0f + 0.5f));
    (void)d;  // flown stat (M5 stats, no exhaustion)
  }
}

void Player::damage_entity(DamageSource src, int amount) {
  // EntityPlayer.damageEntity: halve when sword-blocking (never in M4),
  // armor formula, hunger exhaustion, then Living's (armor applies TWICE).
  if (!bypasses_armor(src) && blocking_with_sword()) amount = 1 + amount >> 1;
  if (!bypasses_armor(src)) amount = apply_armor(amount);
  add_exhaustion(hunger_damage(src));
  Living::damage_entity(src, amount);
}

void Player::attack_target(Entity& target) {
  int dmg = inventory.damage_vs();
  // DamageBoost/Weakness potions: none in M4. Sharpness/Knockback/FireAspect
  // enchantments: none in M4 (helpers would read empty enchant tags).
  int knock = 0;
  if (is_sprinting()) ++knock;
  if (dmg > 0) {
    const bool airborne = fall_distance > 0.0f && !on_ground && !is_on_ladder() && !is_in_water();
    int dealt = dmg;
    if (airborne) dealt += rand.next_int(dealt / 2 + 2);
    bool hit = false;
    if (auto* living = dynamic_cast<Living*>(&target)) {
      hit = living->attack_ex(DamageSource::kPlayer, dealt, this);
    } else {
      hit = target.attack(DamageSource::kPlayer, dealt);
    }
    if (hit) {
      if (knock > 0) {
        constexpr float kPi = 3.14159265358979323846f;
        target.add_velocity(-MathHelper::sin(rotation_yaw * kPi / 180.0f) * knock * 0.5f, 0.1,
                            MathHelper::cos(rotation_yaw * kPi / 180.0f) * knock * 0.5f);
        motion_x *= 0.6;
        motion_z *= 0.6;
        set_sprinting(false);
      }
      if (airborne) on_critical_hit(target);
      if (dealt >= 18) on_overkill();
    }
    if (auto* held = current_equipped()) {
      if (held->has_value()) {
        if (dynamic_cast<Living*>(&target) != nullptr) {
          const bool used = held->value().hit_entity(*this);
          (void)used;
          if (held->value().empty()) destroy_current_equipped_item();
        }
      }
    }
    if (auto* living = dynamic_cast<Living*>(&target)) {
      if (living->is_entity_alive()) alert_wolves(*living, true);
    }
    add_stat(2000000, dealt);  // damageDealtStat slot (M5 stats)
    add_exhaustion(0.3f);
  }
}

}  // namespace craftpp::entity
