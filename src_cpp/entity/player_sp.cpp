#include "entity/player_sp.hpp"

#include "core/math_helper.hpp"
#include "world/blocks.hpp"
#include "world/block_place.hpp"

namespace craftpp::entity {

PlayerSP::PlayerSP(EntityWorld* world, std::string name, int dim) : Player(world) {
  username = std::move(name);
  dimension = dim;
}

void PlayerSP::update_entity_action_state() {
  Player::update_entity_action_state();
  move_strafing = movement_input->move_strafe;
  move_forward = movement_input->move_forward;
  is_jumping = movement_input->jump;
  prev_render_arm_yaw = render_arm_yaw;
  prev_render_arm_pitch = render_arm_pitch;
  render_arm_pitch =
      static_cast<float>(render_arm_pitch + (rotation_pitch - render_arm_pitch) * 0.5);
  render_arm_yaw =
      static_cast<float>(render_arm_yaw + (rotation_yaw - render_arm_yaw) * 0.5);
}

bool PlayerSP::is_sneaking() const { return movement_input->sneak && !sleeping; }

void PlayerSP::set_sprinting(bool v) {
  Living::set_sprinting(v);
  sprinting_ticks_left = v ? 600 : 0;
}

void PlayerSP::set_health(int v) {
  const int diff = get_entity_health() - v;
  if (diff <= 0) {
    set_entity_health(v);
    if (diff < 0) hearts_life = hearts_halves_life / 2;
  } else {
    natural_armor_rating = diff;
    set_entity_health(get_entity_health());
    hearts_life = hearts_halves_life;
    damage_entity(DamageSource::kPlayer, diff);
    hurt_time = max_hurt_time = 10;
  }
}

bool PlayerSP::is_block_solid_for_push(int x, int y, int z) const {
  // isBlockTranslucent (misnomer): isBlockNormalCube.
  const int id = world->block_id(x, y, z);
  return world::bid::material_opaque(id) && world::bid::renders_as_normal(id);
}

bool PlayerSP::push_out_of_blocks(double x, double y, double z) {
  const int bx = MathHelper::floor_double(x);
  const int by = MathHelper::floor_double(y);
  const int bz = MathHelper::floor_double(z);
  const double fx = x - bx;
  const double fz = z - bz;
  if (is_block_solid_for_push(bx, by, bz) || is_block_solid_for_push(bx, by + 1, bz)) {
    const bool free_x0 =
        !is_block_solid_for_push(bx - 1, by, bz) && !is_block_solid_for_push(bx - 1, by + 1, bz);
    const bool free_x1 =
        !is_block_solid_for_push(bx + 1, by, bz) && !is_block_solid_for_push(bx + 1, by + 1, bz);
    const bool free_z0 =
        !is_block_solid_for_push(bx, by, bz - 1) && !is_block_solid_for_push(bx, by + 1, bz - 1);
    const bool free_z1 =
        !is_block_solid_for_push(bx, by, bz + 1) && !is_block_solid_for_push(bx, by + 1, bz + 1);
    int dir = -1;
    double best = 9999.0;
    if (free_x0 && fx < best) {
      best = fx;
      dir = 0;
    }
    if (free_x1 && 1.0 - fx < best) {
      best = 1.0 - fx;
      dir = 1;
    }
    if (free_z0 && fz < best) {
      best = fz;
      dir = 4;
    }
    if (free_z1 && 1.0 - fz < best) {
      best = 1.0 - fz;
      dir = 5;
    }
    constexpr float kPush = 0.1f;
    if (dir == 0) motion_x = -kPush;
    if (dir == 1) motion_x = kPush;
    if (dir == 4) motion_z = -kPush;
    if (dir == 5) motion_z = kPush;
  }
  return false;
}

void PlayerSP::on_living_update() {
  if (sprinting_ticks_left > 0) {
    --sprinting_ticks_left;
    if (sprinting_ticks_left == 0) set_sprinting(false);
  }
  if (sprint_toggle_timer > 0) --sprint_toggle_timer;
  // Creative-mode demon (func_35643_e) is M5; always the else branch here.
  prev_time_in_portal = time_in_portal;
  if (in_portal) {
    // Portal travel is M5 (needs dimension/teleporter); timers only.
    time_in_portal += 0.0125f;
    if (time_in_portal >= 1.0f) time_in_portal = 1.0f;
    in_portal = false;
  } else {
    if (time_in_portal > 0.0f) time_in_portal -= 0.05f;
    if (time_in_portal < 0.0f) time_in_portal = 0.0f;
  }
  if (time_until_portal > 0) --time_until_portal;

  const bool was_jump = prev_jump_held;
  constexpr float kSprintFwd = 0.8f;
  const bool wants_sprint_fwd = movement_input->move_forward >= kSprintFwd;
  movement_input->update_player_move_state();
  prev_jump_held = movement_input->jump;
  if (is_using_item()) {
    movement_input->move_strafe *= 0.2f;
    movement_input->move_forward *= 0.2f;
    sprint_toggle_timer = 0;
  }
  if (movement_input->sneak && y_size < 0.2f) y_size = 0.2f;

  push_out_of_blocks(pos_x - width * 0.35, bbox.min_y + 0.5, pos_z + width * 0.35);
  push_out_of_blocks(pos_x - width * 0.35, bbox.min_y + 0.5, pos_z - width * 0.35);
  push_out_of_blocks(pos_x + width * 0.35, bbox.min_y + 0.5, pos_z - width * 0.35);
  push_out_of_blocks(pos_x + width * 0.35, bbox.min_y + 0.5, pos_z + width * 0.35);

  const bool can_sprint = static_cast<float>(food_level()) > 6.0f;
  if (on_ground && !wants_sprint_fwd && movement_input->move_forward >= kSprintFwd &&
      !is_sprinting() && can_sprint && !is_using_item()) {
    if (sprint_toggle_timer == 0) {
      sprint_toggle_timer = 7;
    } else {
      set_sprinting(true);
      sprint_toggle_timer = 0;
    }
  }
  if (is_sneaking()) sprint_toggle_timer = 0;
  if (is_sprinting() &&
      (movement_input->move_forward < kSprintFwd || collided_horizontally || !can_sprint)) {
    set_sprinting(false);
  }
  if (capabilities.allow_flying && !was_jump && movement_input->jump) {
    if (fly_toggle_timer == 0) {
      fly_toggle_timer = 7;
    } else {
      capabilities.is_flying = !capabilities.is_flying;
      fly_toggle_timer = 0;
    }
  }
  if (capabilities.is_flying) {
    if (movement_input->sneak) motion_y -= 0.15;
    if (movement_input->jump) motion_y += 0.15;
  }
  // EntityPlayer.onLivingUpdate fly-timer decay (SP skips Player's update
  // like the source skips to Living's; the decay runs here, after the check).
  if (fly_toggle_timer > 0) --fly_toggle_timer;
  Living::on_living_update();
  if (on_ground && capabilities.is_flying) capabilities.is_flying = false;
}

}  // namespace craftpp::entity
