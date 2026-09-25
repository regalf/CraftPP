#pragma once

#include <vector>

#include "core/aabb.hpp"
#include "core/random.hpp"
#include "world/block_collision.hpp"

namespace craftpp::entity {

// Damage sources mirror DamageSource.java (only the ones M4 entities take).
enum class DamageSource {
  kInFire,   // inFire (burning box / fire block contact)
  kOnFire,   // onFire (fire ticks)
  kLava,     // lava exposure
  kCactus,   // cactus contact
  kInWall,   // inWall (suffocation)
  kDrown,    // drown (no air)
  kFall,     // fall (impact)
  kPlayer,   // causePlayerDamage (carries the attacker via attack_ex)
};

// World services an Entity needs. The M4 test world implements this over a
// block map; the live World (M5) will implement it over chunks + entities.
// Particle/sound hooks are no-ops in tests — but every RNG draw in Entity
// code still runs (same order), so streams stay identical to Java.
class Entity;  // fwd for world hooks

class EntityWorld : public world::BlockView {
 public:
  virtual bool chunks_exist(int x0, int y0, int z0, int x1, int y1, int z1) const = 0;
  virtual bool multiplayer() const { return false; }
  virtual bool can_lightning_strike(int x, int y, int z) const { return false; }
  virtual void spawn_particle(const char* name, double x, double y, double z, double mx, double my,
                              double mz) {}
  virtual void play_sound(const char* name, float volume, float pitch) {}
  // Step split: func_41002_a (step sound, snow-aware) + onEntityWalking.
  virtual void on_entity_walk(int block_id, int x, int y, int z) {}
  // Per-cell post-move hook mirror onEntityCollidedWithBlock for the ids
  // with motion effects (web slow flag, soul sand drag); damage goes
  // through attack_entity_from.
  virtual void on_entity_collided_cell(int block_id, int x, int y, int z) {}
  virtual void attack_entity_from(DamageSource src, int amount) {}
  virtual void set_entity_state(Entity& e, int state) {}
  virtual Entity* closest_player_to(const Entity& e, double max_dist) { return nullptr; }
  virtual std::vector<Entity*> entities_excluding(const Entity& e, const Aabb& box) {
    return {};
  }
  // World.rand equivalent (unbreaking rolls, drop rolls). Owned by the world.
  virtual JavaRandom& world_rand() = 0;
  // Shared sticky block-bounds state (mirrors the JVM-global Block statics;
  // per-world here so worlds stay independent). Entity movement AND block
  // placement MUST use this one instance, or the single-box reads diverge.
  virtual world::BlockCollider& collider() = 0;
};

// Block-part of World.getCollidingBoundingBoxes (entity-entity part is M5;
// the base getBoundingBox() is null anyway). Shared by Entity/Living.
void colliding_boxes_for(EntityWorld* w, world::BlockCollider& collider, const Aabb& box,
                         std::vector<Aabb>& out);

// Base entity mirroring Entity.java (singleplayer-relevant subset; DataWatcher
// flag bits 0/1/2/3 live as plain bools with identical observable behavior).
// Riding (ridingEntity/riddenByEntity) is M5: pointers stay null here.
class Entity {
 public:
  explicit Entity(EntityWorld* world);
  virtual ~Entity() = default;

  // ---- dimensions ----
  float width = 0.6f;
  float height = 1.8f;
  float y_offset = 0.0f;
  float y_size = 0.0f;
  float step_height = 0.0f;
  bool no_clip = false;
  bool field_9293_aM = true;  // false only for boats (M5)
  float entity_collision_reduction = 0.0f;

  // ---- state ----
  double pos_x = 0.0, pos_y = 0.0, pos_z = 0.0;
  double prev_pos_x = 0.0, prev_pos_y = 0.0, prev_pos_z = 0.0;
  double last_tick_pos_x = 0.0, last_tick_pos_y = 0.0, last_tick_pos_z = 0.0;
  double motion_x = 0.0, motion_y = 0.0, motion_z = 0.0;
  float rotation_yaw = 0.0f, rotation_pitch = 0.0f;
  float prev_rotation_yaw = 0.0f, prev_rotation_pitch = 0.0f;
  Aabb bbox;
  bool on_ground = false;
  bool collided_horizontally = false;
  bool collided_vertically = false;
  bool collided = false;
  bool is_dead = false;
  float fall_distance = 0.0f;
  float distance_walked = 0.0f;
  float prev_distance_walked = 0.0f;
  int next_step_distance = 1;
  int ticks_existed = 0;
  int fire = 0;
  int fire_resistance = 1;
  bool in_water = false;
  bool is_in_web = false;
  bool first_update = true;
  bool is_immune_to_fire = false;
  bool is_air_borne = false;
  bool been_attacked = false;
  bool prevent_spawning = false;  // preventEntitySpawning (mobs set true)
  JavaRandom rand;

  int entity_id = 0;

  EntityWorld* world = nullptr;

  void set_size(float w, float h) {
    width = w;
    height = h;
  }
  void set_position(double x, double y, double z);
  void set_rotation(float yaw, float pitch);
  void set_position_and_rotation(double x, double y, double z, float yaw, float pitch);
  void set_location_and_angles(double x, double y, double z, float yaw, float pitch);

  virtual bool is_sneaking() const { return flag_sneak; }
  bool is_sprinting() const { return flag_sprint; }
  void set_sneaking(bool v) { flag_sneak = v; }
  virtual void set_sprinting(bool v) { flag_sprint = v; }

  void set_entity_dead() { is_dead = true; }

  virtual void on_update() { on_entity_update(); }
  virtual void on_entity_update();
  void move_entity(double dx, double dy, double dz);
  void move_flying(float strafe, float forward, float friction);
  bool handle_water_movement();
  bool handle_lava_movement();
  bool is_in_water() const { return in_water; }
  bool is_wet() const;
  bool is_offset_in_liquid(double dx, double dy, double dz);
  bool is_inside_of_material_water() const;  // isInsideOfMaterial(water)
  virtual float eye_height() const { return 0.0f; }
  virtual bool can_trigger_walking() const { return true; }
  virtual void fall(float distance) {}
  // Mirrors Entity.attackEntityFrom (base: just sets beenAttacked). Living
  // overrides with the health/armor/knockback logic. The world hook inside
  // the base version exists so single-entity tests can log the call.
  virtual bool attack(DamageSource src, int amount) {
    world->attack_entity_from(src, amount);
    set_been_attacked();
    return false;
  }
  void set_been_attacked() { been_attacked = true; }
  void extinguish() { fire = 0; }  // func_40045_B
  void add_velocity(double dx, double dy, double dz) {
    motion_x += dx;
    motion_y += dy;
    motion_z += dz;
    is_air_borne = true;
  }
  bool is_inside_opaque_block() const;  // Entity.java (8 eye samples)

 protected:
  bool flag_burning = false;  // dataWatcher bit 0 (fire>0), cosmetic here
  bool flag_sneak = false;    // bit 1
  bool flag_riding = false;   // bit 2 (always false, no riding in M4)
  bool flag_sprint = false;   // bit 3

  void update_fall_state(double dy, bool grounded);
  void deal_fire_damage(int amount);
  void set_on_fire_from_lava();
  void kill() { set_entity_dead(); }
};

}  // namespace craftpp::entity
