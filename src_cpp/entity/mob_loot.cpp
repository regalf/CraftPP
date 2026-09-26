#include "entity/mob_loot.hpp"

#include "world/block_place.hpp"

namespace craftpp::entity {

void drop_mob_loot(Living& self, int item_id) {
  auto* w = dynamic_cast<world::edit::EditWorld*>(self.world);
  if (w == nullptr) return;
  const int n = self.rand.next_int(3);
  for (int i = 0; i < n; ++i) {
    w->on_item_drop(item_id, 1, 0, self.pos_x, self.pos_y, self.pos_z, 0.0, 0.0, 0.0);
  }
}

}  // namespace craftpp::entity
