#pragma once

#include <memory>

#include "entity/player.hpp"

namespace craftpp::entity {

// Scripted movement input (mirrors MovementInput.java fields; the
// FromOptions subclass reads real keys in the app layer, M5+).
struct MovementInput {
  float move_strafe = 0.0f;
  float move_forward = 0.0f;
  bool jump = false;
  bool sneak = false;
  virtual ~MovementInput() = default;
  virtual void update_player_move_state() {}  // base impl is empty
};

// EntityPlayerSP M4 surface: input glue, sprint state, push-out, portal
// timers. Client-only systems (GUI, sounds, stats file, respawn) are M5+.
class PlayerSP : public Player {
 public:
  std::unique_ptr<MovementInput> movement_input = std::make_unique<MovementInput>();
  float render_arm_yaw = 0.0f;
  float render_arm_pitch = 0.0f;
  float prev_render_arm_yaw = 0.0f;
  float prev_render_arm_pitch = 0.0f;
  int sprint_toggle_timer = 0;
  int sprinting_ticks_left = 0;
  float time_in_portal = 0.0f;
  float prev_time_in_portal = 0.0f;
  bool in_portal = false;
  int time_until_portal = 0;

  PlayerSP(EntityWorld* world, std::string name, int dim);

  void update_entity_action_state() override;
  void on_living_update() override;
  bool is_sneaking() const override;
  void set_sprinting(bool v) override;
  // SP damage routing (server health packets in the real game).
  void set_health(int v);
  bool push_out_of_blocks(double x, double y, double z);

 private:
  bool is_block_solid_for_push(int x, int y, int z) const;
};

}  // namespace craftpp::entity
