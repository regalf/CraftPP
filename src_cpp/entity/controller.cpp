#include "entity/controller.hpp"

namespace craftpp::entity {
namespace {

// blockActivated dispatch (M4: doors + trapdoors toggle; iron = no-op true;
// everything else (chests/furnaces/cake/beds/…) is M5 and returns false).
bool block_activated(world::edit::EditWorld& w, int x, int y, int z) {
  using namespace world::bid;
  const int id = w.block_id(x, y, z);
  const int meta = w.block_meta(x, y, z);
  if (id == kDoorWood) {
    if ((meta & 8) != 0) {
      if (w.block_id(x, y - 1, z) == id) return block_activated(w, x, y - 1, z);
      return true;
    }
    if (w.block_id(x, y + 1, z) == id)
      world::edit::set_meta_notify(w, x, y + 1, z, (meta ^ 4) + 8);
    world::edit::set_meta_notify(w, x, y, z, meta ^ 4);
    w.play_aux_sfx(1003, x, y, z, 0);
    return true;
  }
  if (id == kDoorSteel) return true;
  if (id == kTrapDoor) {
    world::edit::set_meta_notify(w, x, y, z, meta ^ 4);
    w.play_aux_sfx(1003, x, y, z, 0);
    return true;
  }
  return false;
}

// onBlockClicked dispatch (M4: door/trapdoor toggle via blockActivated;
// TNT flint&steel metadata; else empty).
void block_clicked(world::edit::EditWorld& w, Player& p, int x, int y, int z) {
  using namespace world::bid;
  const int id = w.block_id(x, y, z);
  if (id == kDoorWood || id == kDoorSteel || id == kTrapDoor) {
    block_activated(w, x, y, z);
    return;
  }
  if (id == kTnt) {
    const auto* held = p.current_equipped();
    if (held && held->has_value() && held->value().item_id == 259) {  // flint&steel
      world::edit::set_meta_notify(w, x, y, z, 1);
    }
  }
}

// World.onBlockHit (fire under the clicked face is put out).
void block_hit(world::edit::EditWorld& w, int x, int y, int z, int side) {
  int tx = x, ty = y, tz = z;
  if (side == 0) --ty;
  if (side == 1) ++ty;
  if (side == 2) --tz;
  if (side == 3) ++tz;
  if (side == 4) --tx;
  if (side == 5) ++tx;
  if (w.block_id(tx, ty, tz) == world::bid::kFire) {
    w.play_aux_sfx(1004, tx, ty, tz, 0);
    world::edit::break_to_air(w, tx, ty, tz);
  }
}

}  // namespace

bool Controller::send_block_removed(int x, int y, int z, int side) {
  (void)side;
  const int id = w.block_id(x, y, z);
  if (id <= 0) return false;
  const int meta = w.block_meta(x, y, z);
  w.play_aux_sfx(2001, x, y, z, id + meta * 256);
  world::edit::break_to_air(w, x, y, z);
  on_block_destroyed_by_player(id, x, y, z, meta);
  return true;
}

void ControllerSP::click_block(int x, int y, int z, int side) {
  block_hit(w, x, y, z, side);
  const int id = w.block_id(x, y, z);
  if (id > 0 && cur_damage_ == 0.0f) block_clicked(w, p, x, y, z);
  if (id > 0 && p.strength_vs_block(id) >= 1.0f) {
    send_block_removed(x, y, z, side);
  }
}

void ControllerSP::send_block_removing(int x, int y, int z, int side) {
  if (block_hit_wait_ > 0) {
    --block_hit_wait_;
    return;
  }
  if (x == cur_x && y == cur_y && z == cur_z) {
    const int id = w.block_id(x, y, z);
    if (id == 0) return;
    cur_damage_ += p.strength_vs_block(id);
    if (static_cast<int>(hit_ticks_) % 4 == 0) {
      // Dig sound hook (M6 audio): no RNG, deterministic params.
    }
    hit_ticks_ += 1.0f;
    if (cur_damage_ >= 1.0f) {
      send_block_removed(x, y, z, side);
      cur_damage_ = 0.0f;
      prev_damage_ = 0.0f;
      hit_ticks_ = 0.0f;
      block_hit_wait_ = 5;
    }
  } else {
    cur_damage_ = 0.0f;
    prev_damage_ = 0.0f;
    hit_ticks_ = 0.0f;
    cur_x = x;
    cur_y = y;
    cur_z = z;
  }
}

bool ControllerSP::send_block_removed(int x, int y, int z, int side) {
  const int id = w.block_id(x, y, z);
  const int meta = w.block_meta(x, y, z);
  const bool removed = Controller::send_block_removed(x, y, z, side);
  if (auto* held = p.current_equipped()) {
    if (held->has_value()) {
      const bool used = held->value().on_block_destroyed(x, y, z, p);
      (void)used;
      if (held->value().empty()) p.destroy_current_equipped_item();
    }
  }
  if (removed && p.can_harvest_block(id)) {
    world::edit::harvest_block(w, p, x, y, z, meta);
  }
  return removed;
}

void ControllerSP::reset_block_removing() {
  cur_damage_ = 0.0f;
  block_hit_wait_ = 0;
}

void ControllerSP::update_controller() { prev_damage_ = cur_damage_; }

bool ControllerSP::send_place_block(ItemStack& stack, int x, int y, int z, int side) {
  if (stack.empty()) return false;
  const int target = w.block_id(x, y, z);
  if (target > 0 && block_activated(w, x, y, z)) return true;
  if (stack.item_id == 324 || stack.item_id == 330) {  // doors place two blocks
    if (world::edit::use_door_item(w, collider(), p, stack.stack_size, stack.item_id, x, y, z,
                                   side)) {
      p.add_stat(3000000 + stack.item_id, 1);  // objectUseStats slot (M5)
      return true;
    }
    return false;
  }
  if (stack.item_id < 256) {
    if (world::edit::use_block_item(w, collider(), p, stack.stack_size, stack.item_id,
                                    stack.damage, x, y, z, side)) {
      p.add_stat(3000000 + stack.item_id, 1);
      return true;
    }
    return false;
  }
  return false;  // non-block items (food/tools/buckets/…) are M5
}

void ControllerCreative::enable_creative(Player& player) {
  player.capabilities.allow_flying = true;
  player.capabilities.deplete_buckets = true;
  player.capabilities.disable_damage = true;
}

void ControllerCreative::disable_creative(Player& player) {
  player.capabilities.allow_flying = false;
  player.capabilities.is_flying = false;
  player.capabilities.deplete_buckets = false;
  player.capabilities.disable_damage = false;
}

void ControllerCreative::click_block(int x, int y, int z, int side) {
  block_hit(w, x, y, z, side);
  send_block_removed(x, y, z, side);
  break_countdown_ = 5;
}

void ControllerCreative::send_block_removing(int x, int y, int z, int side) {
  if (--break_countdown_ <= 0) {
    break_countdown_ = 5;
    block_hit(w, x, y, z, side);
    send_block_removed(x, y, z, side);
  }
}

bool ControllerCreative::send_place_block(ItemStack& stack, int x, int y, int z, int side) {
  if (stack.empty()) return false;
  const int target = w.block_id(x, y, z);
  if (target > 0 && block_activated(w, x, y, z)) return true;
  const int saved_damage = stack.damage;
  const int saved_size = stack.stack_size;
  bool placed = false;
  if (stack.item_id == 324 || stack.item_id == 330) {
    placed = world::edit::use_door_item(w, collider(), p, stack.stack_size, stack.item_id, x, y,
                                        z, side);
  } else if (stack.item_id < 256) {
    placed = world::edit::use_block_item(w, collider(), p, stack.stack_size, stack.item_id,
                                         stack.damage, x, y, z, side);
  }
  stack.damage = saved_damage;
  stack.stack_size = saved_size;
  return placed;
}

}  // namespace craftpp::entity
