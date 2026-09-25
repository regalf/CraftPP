#include "entity/items.hpp"

namespace craftpp::entity {
namespace {

int tool_kind(int shifted) {
  if (shifted == 257 || shifted == 270 || shifted == 274 || shifted == 278 || shifted == 285)
    return 1;  // pick
  if (shifted == 256 || shifted == 269 || shifted == 273 || shifted == 277 || shifted == 284)
    return 2;  // spade
  if (shifted == 258 || shifted == 271 || shifted == 275 || shifted == 279 || shifted == 286)
    return 3;  // axe
  if (shifted == 290 || shifted == 291 || shifted == 292 || shifted == 293 || shifted == 294)
    return 4;  // hoe
  if (shifted == 267 || shifted == 268 || shifted == 272 || shifted == 276 || shifted == 283)
    return 5;  // sword
  return 0;
}

}  // namespace

bool ItemStack::on_block_destroyed(int x, int y, int z, ItemUser& user) {
  (void)x;
  (void)y;
  (void)z;
  const int k = tool_kind(item_id);
  if (k >= 1 && k <= 4) {
    damage_item(1, user);
    return true;
  }
  if (k == 5) {
    damage_item(2, user);
    return true;
  }
  return false;
}

bool ItemStack::hit_entity(ItemUser& user) {
  const int k = tool_kind(item_id);
  if (k >= 1 && k <= 4) {
    damage_item(2, user);
    return true;
  }
  if (k == 5) {
    damage_item(1, user);
    return true;
  }
  return false;
}

}  // namespace craftpp::entity
