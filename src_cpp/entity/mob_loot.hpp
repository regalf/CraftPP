#pragma once

#include "entity/living.hpp"

namespace craftpp::entity {

// Shared mob drop roll (EntityLiving.dropFewItems equivalent, fortune 0):
// drops 0-2 of one item id at the mob's feet. No-op without an EditWorld.
void drop_mob_loot(Living& self, int item_id);

}  // namespace craftpp::entity
