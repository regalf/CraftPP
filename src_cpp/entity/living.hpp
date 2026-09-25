#pragma once

#include "entity/entity.hpp"

namespace craftpp::entity {

// Mirror of EntityLiving.java (mob/player shared base). Potions, AI targets,
// XP/drops and riding are M5: the active-potion map stays empty (every
// isPotionActive is false), currentTarget stays null, drops/XP hooks no-op.
// All RNG draws still run in source order.
class Living : public Entity {
 public:
  Living(EntityWorld* world);

  int health = max_health();
  int prev_health = 0;
  int hurt_time = 0;
  int max_hurt_time = 0;
  int death_time = 0;
  int attack_time = 0;
  float attacked_at_yaw = 0.0f;
  int hearts_life = 0;
  int hearts_halves_life = 20;
  int natural_armor_rating = 0;
  int entity_age = 0;
  int air_supply = 300;  // dataWatcher short 1 (default 300)
  float move_strafing = 0.0f;
  float move_forward = 0.0f;
  float random_yaw_velocity = 0.0f;
  bool is_jumping = false;
  float land_movement_factor = 0.1f;
  float jump_movement_factor = 0.02f;
  float render_yaw_offset = 0.0f;
  float prev_render_yaw_offset = 0.0f;
  float prev_swing = 0.0f;
  float swing = 0.0f;
  float prev_camera_pitch = 0.0f;
  float camera_pitch = 0.0f;
  float limb_phase = 0.0f;   // field_9360_w
  float prev_limb_phase = 0.0f;  // field_9359_x
  float limb_speed = 0.0f;   // field_9361_v
  float prev_limb_speed = 0.0f;
  float anim_t = 0.0f;       // field_703_S
  float anim_speed = 0.0f;    // field_704_R
  float prev_anim_speed = 0.0f;  // field_705_Q
  float default_pitch = 0.0f;
  bool is_multiplayer_entity = false;
  float render_wobble_a = 0.0f;  // field_9363_r (render only, global-random)
  float render_wobble_b = 0.0f;  // field_9365_p (render only, global-random)

  virtual int max_health() const { return 20; }
  float eye_height() const override { return height * 0.85f; }  // NOT 1.62 (player differs)

  void on_update() override;
  void on_entity_update() override;
  virtual   void on_living_update();
  virtual void update_entity_action_state();
  virtual void move_entity_with_heading(float strafe, float forward);
  void fall(float distance) override;
  bool attack(DamageSource src, int amount) override;
  // Full Living.attackEntityFrom with an explicit attacker (null for
  // environmental damage). Player melee passes the player (knockback path).
  virtual bool attack_ex(DamageSource src, int amount, Entity* attacker);
  void knock_back(Entity& attacker, int amount, double dx, double dz);
  void heal(int amount);
  int get_entity_health() const { return health; }
  void set_entity_health(int v) {
    health = v;
    if (v > max_health()) v = max_health();  // source quirk: clamped local is discarded
  }
  bool is_entity_alive() const { return !is_dead && health > 0; }
  bool is_on_ladder() const;
  void set_position_and_rotation2(double x, double y, double z, float yaw, float pitch, int steps);

 protected:
  int jump_cooldown = 0;      // field_39003_d
  int revenge_timer = 0;      // field_34905_c (M5: + field_34904_b player ref)
  int armor_carry = 0;        // field_40129_bA
  int hurt_cooldown_b = 0;    // field_35172_bP
  int hurt_cooldown_q = 0;    // field_35173_bQ
  bool potion_dirty = true;   // field_39001_b
  bool unused_death_flag = false;  // unused_flag (set in on_death)
  int living_sound_time = 0;
  double new_pos_x = 0.0, new_pos_y = 0.0, new_pos_z = 0.0;
  double new_rot_yaw = 0.0, new_rot_pitch = 0.0;
  int new_pos_steps = 0;

  virtual bool is_movement_blocked() const { return health <= 0; }
  virtual float speed_factor() const { return 1.0f; }  // func_35166_t_ (potions M5)
  virtual bool can_breathe_underwater() const { return false; }
  virtual bool is_potion_active(int id) const { return false; }  // M5
  virtual int armor_points() const { return 0; }  // func_40119_ar (player: armor)
  virtual int get_drop_item_id() const { return 0; }             // M5 drops
  virtual void on_entity_death() {}
  virtual void on_death(DamageSource src);
  virtual float get_sound_volume() const { return 1.0f; }
  virtual const char* hurt_sound() const { return "damage.hurtflesh"; }
  virtual const char* death_sound() const { return "damage.hurtflesh"; }
  virtual const char* living_sound() const { return nullptr; }
  virtual int talk_interval() const { return 80; }
  virtual void jump();
  virtual void damage_entity(DamageSource src, int amount);
  // setDamageBypassesArmor sources skip the armor formula (func_40115_d).
  static bool bypasses_armor(DamageSource src) {
    return src == DamageSource::kOnFire || src == DamageSource::kInWall ||
           src == DamageSource::kDrown || src == DamageSource::kFall;
  }
  // Hunger cost of damage (setDamageBypassesArmor zeroes it).
  static float hunger_damage(DamageSource src) { return bypasses_armor(src) ? 0.0f : 0.3f; }
  // Shared armor formula (func_40115_d) with the carry accumulator.
  int apply_armor(int amount) {
    const int keep = 25 - armor_points();
    const int scaled = amount * keep + armor_carry;
    armor_carry = scaled % 25;
    return scaled / 25;
  }
  float hurt_pitch();  // draws rand (non-const)
  void update_potion_effects();
};

}  // namespace craftpp::entity
