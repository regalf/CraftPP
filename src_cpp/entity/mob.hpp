#pragma once

#include "entity/living.hpp"
#include "world/blocks.hpp"

namespace craftpp::entity {

// Passive pig (EntityPig subset): wanders via Living AI, pork drops.
class Pig : public Living {
 public:
  explicit Pig(EntityWorld* w) : Living(w) {
    width = 0.9f;
    height = 0.9f;
    health = max_health();
  }
  int max_health() const override { return 10; }
  int get_drop_item_id() const override { return 319; }  // porkRaw (cooked if burning)
  void on_death(DamageSource src) override;
  void update_entity_action_state() override;

  bool can_spawn_here();

  float move_speed = 0.7f;  // EntityLiving default (wander stroll pace)
};

// Hostile zombie (EntityZombie subset): seeks players, burns in daylight.
class Zombie : public Living {
 public:
  explicit Zombie(EntityWorld* w) : Living(w) {
    width = 0.6f;
    height = 1.8f;
    health = max_health();
  }
  int max_health() const override { return 20; }
  int get_drop_item_id() const override { return 367; }  // rottenFlesh
  void on_death(DamageSource src) override;
  void on_living_update() override;
  void update_entity_action_state() override;

  bool can_spawn_here();

  // EntityZombie.moveSpeed (path pace). Applied to move_forward when
  // seeking; the base Living wander leaves it 0.
  float move_speed = 0.5f;

 private:
  int attack_strength = 4;
};

}  // namespace craftpp::entity
