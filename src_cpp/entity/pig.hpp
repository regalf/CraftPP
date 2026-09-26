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

}  // namespace craftpp::entity
