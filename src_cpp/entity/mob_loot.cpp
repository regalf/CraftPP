#include "entity/mob_loot.hpp"

#include "entity/drops.hpp"
#include "world/block_place.hpp"

namespace craftpp::entity {

void drop_mob_loot(Living& self, int item_id) {
  auto* w = dynamic_cast<world::edit::EditWorld*>(self.world);
  if (w == nullptr) return;
  const int n = self.rand.next_int(3);
  for (int i = 0; i < n; ++i) {
    double mx = 0.0, my = 0.0, mz = 0.0;
    DroppedItem::spawn_pop(w, mx, my, mz);
    w->on_item_drop(item_id, 1, 0, self.pos_x, self.pos_y, self.pos_z, mx, my, mz);
  }
}

}  // namespace craftpp::entity
