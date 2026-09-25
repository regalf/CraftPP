#pragma once

#include "entity/player.hpp"
#include "world/block_place.hpp"

namespace craftpp::entity {

// PlayerController SP/Creative (survival mining + creative insta-break/place).
// The Minecraft-client glue (mc.theWorld/thePlayer/sounds/GUI) is replaced by
// explicit world+player references; render-only partial-time state is kept as
// plain fields. Multiplayer controllers are M7.
class Controller {
 public:
  Controller(world::edit::EditWorld& world, Player& player) : w(world), p(player) {}
  virtual ~Controller() = default;

  virtual void click_block(int x, int y, int z, int side) = 0;
  virtual void send_block_removing(int x, int y, int z, int side) = 0;
  virtual void reset_block_removing() = 0;
  virtual bool send_place_block(ItemStack& stack, int x, int y, int z, int side) = 0;
  virtual float reach_distance() const = 0;
  virtual bool creative() const { return false; }

  // PlayerController.sendBlockRemoved (shared): aux FX + clear + hook.
  bool send_block_removed(int x, int y, int z, int side);

 protected:
  world::edit::EditWorld& w;
  Player& p;
  world::BlockCollider& collider() { return w.collider(); }
  // Block.onBlockDestroyedByPlayer (default empty; ice->water needs light=M5).
  virtual void on_block_destroyed_by_player(int id, int x, int y, int z, int meta) {}
};

class ControllerSP : public Controller {
 public:
  using Controller::Controller;

  void click_block(int x, int y, int z, int side) override;
  void send_block_removing(int x, int y, int z, int side) override;
  bool send_block_removed(int x, int y, int z, int side);
  void reset_block_removing() override;
  bool send_place_block(ItemStack& stack, int x, int y, int z, int side) override;
  float reach_distance() const override { return 4.0f; }
  void update_controller();  // prev damage copy (music hook skipped)

  // Test introspection (mirrors the private damage state).
  float cur_damage() const { return cur_damage_; }

 private:
  int cur_x = -1, cur_y = -1, cur_z = -1;
  float cur_damage_ = 0.0f;
  float prev_damage_ = 0.0f;
  float hit_ticks_ = 0.0f;
  int block_hit_wait_ = 0;
};

class ControllerCreative : public Controller {
 public:
  using Controller::Controller;

  void click_block(int x, int y, int z, int side) override;
  void send_block_removing(int x, int y, int z, int side) override;
  void reset_block_removing() override {}
  bool send_place_block(ItemStack& stack, int x, int y, int z, int side) override;
  float reach_distance() const override { return 5.0f; }
  bool creative() const override { return true; }

  static void enable_creative(Player& player);  // func_35646_d
  static void disable_creative(Player& player);

 private:
  int break_countdown_ = 0;
};

}  // namespace craftpp::entity
