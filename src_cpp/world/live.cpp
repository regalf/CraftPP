#include "world/live.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "entity/mob.hpp"
#include "entity/player.hpp"

namespace craftpp::world {

void LiveWorld::provide_area(int cx0, int cz0, int cx1, int cz1) {
  for (int cz = cz0; cz <= cz1; ++cz) {
    for (int cx = cx0; cx <= cx1; ++cx) {
      if (region_.has_chunk(cx, cz)) continue;
      provider_.set_chunk_seed(cx, cz);
      std::vector<std::int8_t> b;
      provider_.generate_terrain(cx, cz, b);
      provider_.replace_biome_blocks(cx, cz, b,
                                     manager_.block_biomes(cx * 16, cz * 16, 16, 16));
      caves_.generate(seed_, cx, cz, b);
      ravine_.generate(seed_, cx, cz, b);
      region_.ensure_chunk(cx, cz, b.data());
      dirty_[{cx, cz}] = true;
    }
  }
  for (int cz = cz0; cz <= cz1; ++cz) {
    for (int cx = cx0; cx <= cx1; ++cx) {
      if (populated_.count({cx, cz}) != 0) continue;
      populate_chunk(region_, manager_, seed_, cx, cz, wrand_);
      populated_[{cx, cz}] = true;
      dirty_[{cx, cz}] = true;
    }
  }
}

void LiveWorld::tick() {
  ++time_;
  sky_sub_ = tick::skylight_subtracted(time_);
  // Drain due scheduled ticks (fire).
  for (std::size_t i = 0; i < sched_.size();) {
    if (sched_[i].time <= time_) {
      const tick::ScheduledTick t = sched_[i];
      sched_.erase(sched_.begin() + static_cast<std::ptrdiff_t>(i));
      if (block_id(t.x, t.y, t.z) == t.id) {
        tick::update_tick(*this, sched_, time_, wrand_, t.id, t.x, t.y, t.z);
      }
    } else {
      ++i;
    }
  }
  for (entity::Entity* e : entities_) {
    if (e != nullptr) e->on_update();
  }
  // Tile entities (furnaces burn/cook here).
  for (auto& [key, tile] : tiles_) {
    (void)key;
    if (tile) tile->update(*this);
  }
  // Dropped items tick + player pickup sweep (entity-collision equivalent:
  // within ~1 block of a living player, no delay left).
  for (auto& it : items_) {
    if (it && !it->is_dead) it->on_update();
  }
  for (auto& it : items_) {
    if (!it || it->is_dead || it->pickup_delay > 0 || it->item.stack_size <= 0) continue;
    for (entity::Entity* e : entities_) {
      auto* p = dynamic_cast<entity::Player*>(e);
      if (p == nullptr || p->is_dead) continue;
      const double dx = p->pos_x - it->pos_x;
      const double dy = (p->pos_y + p->height * 0.5) - it->pos_y;
      const double dz = p->pos_z - it->pos_z;
      if (dx * dx + dy * dy + dz * dz > 1.0) continue;
      if (p->inventory.add_stack(it->item) && it->item.stack_size <= 0) {
        it->set_entity_dead();
        break;
      }
    }
  }
  items_.erase(std::remove_if(items_.begin(), items_.end(),
                              [](const std::unique_ptr<entity::DroppedItem>& e) {
                                return !e || e->is_dead;
                              }),
               items_.end());
  // Spawn cycle + dead mob sweep.
  perform_spawning();
  mobs_.erase(std::remove_if(mobs_.begin(), mobs_.end(),
                             [](const std::unique_ptr<entity::Living>& e) {
                               return !e || e->is_dead;
                             }),
              mobs_.end());
  entities_.erase(std::remove_if(entities_.begin(), entities_.end(),
                                 [](entity::Entity* e) {
                                   if (e == nullptr || !e->is_dead) return false;
                                   // App-owned players stay registered (respawn UI is M5+).
                                   return dynamic_cast<entity::Player*>(e) == nullptr;
                                 }),
                  entities_.end());
  // Random ticks on populated chunks near entities (7-chunk radius stream
  // like positionsToUpdate; demo scale ticks everything provided).
  for (const auto& [key, was] : populated_) {
    (void)was;
    bool near = entities_.empty();
    for (const entity::Entity* e : entities_) {
      if (e == nullptr) continue;
      const int ecx = static_cast<int>(std::floor(e->pos_x)) >> 4;
      const int ecz = static_cast<int>(std::floor(e->pos_z)) >> 4;
      int dx = key.first - ecx;
      int dz = key.second - ecz;
      if (dx < 0) dx = -dx;
      if (dz < 0) dz = -dz;
      if (dx <= 7 && dz <= 7) {
        near = true;
        break;
      }
    }
    if (near) tick::tick_chunk(*this, sched_, time_, wrand_, update_lcg_, key.first, key.second);
  }
}

float LiveWorld::daylight() const { return tick::daylight_factor(time_); }

entity::Entity* LiveWorld::closest_player_to(const entity::Entity& e, double max_dist) {
  entity::Entity* best = nullptr;
  double best2 = max_dist < 0 ? 1e30 : max_dist * max_dist;
  for (entity::Entity* c : entities_) {
    auto* p = dynamic_cast<entity::Player*>(c);
    if (p == nullptr || p == &e || p->is_dead) continue;
    const double dx = p->pos_x - e.pos_x, dy = p->pos_y - e.pos_y, dz = p->pos_z - e.pos_z;
    const double d2 = dx * dx + dy * dy + dz * dz;
    if (d2 < best2) {
      best2 = d2;
      best = p;
    }
  }
  return best;
}

namespace {
// Eligible-chunk set for spawning (17x17 around players, border flagged).
struct Eligible {
  std::map<std::pair<int, int>, bool> border;
};
}  // namespace

int LiveWorld::perform_spawning() {
  if (!spawn_hostile_ && !spawn_peaceful_) return 0;
  Eligible elig;
  for (entity::Entity* e : entities_) {
    auto* p = dynamic_cast<entity::Player*>(e);
    if (p == nullptr || p->is_dead) continue;
    const int pcx = static_cast<int>(std::floor(p->pos_x)) >> 4;
    const int pcz = static_cast<int>(std::floor(p->pos_z)) >> 4;
    for (int dx = -8; dx <= 8; ++dx)
      for (int dz = -8; dz <= 8; ++dz) {
        const bool edge = (dx == -8 || dx == 8 || dz == -8 || dz == 8);
        auto key = std::make_pair(pcx + dx, pcz + dz);
        if (!edge) {
          elig.border[key] = false;
        } else if (elig.border.count(key) == 0) {
          elig.border[key] = true;
        }
      }
  }
  if (elig.border.empty()) return 0;
  int spawned = 0;
  // Creature types: hostile zombies (cap 70), peaceful pigs every 400 ticks
  // (cap 10). Biome spawn lists are fixed pairs for now (M5+ biomes).
  struct Type {
    bool peaceful;
    int cap;
    bool gate;
  };
  const Type types[2] = {{false, 70, true}, {true, 10, (time_ % 400LL) == 0LL}};
  for (const Type& t : types) {
    if (t.peaceful && (!spawn_peaceful_ || !t.gate)) continue;
    if (!t.peaceful && !spawn_hostile_) continue;
    int count = 0;
    for (auto& m : mobs_) {
      if (!m || m->is_dead) continue;
      const bool is_z = dynamic_cast<entity::Zombie*>(m.get()) != nullptr;
      if ((!t.peaceful && is_z) || (t.peaceful && !is_z)) ++count;
    }
    if (count > t.cap * static_cast<int>(elig.border.size()) / 256) continue;
    for (auto& [cc, is_border] : elig.border) {
      if (is_border) continue;
      int px = cc.first * 16 + wrand_.next_int(16);
      int py = wrand_.next_int(128);
      int pz = cc.second * 16 + wrand_.next_int(16);
      if (is_normal_cube(px, py, pz)) continue;
      // Material gate (land creatures need air here).
      if (block_id(px, py, pz) != 0) continue;
      int group = 0;
      for (int g = 0; g < 3; ++g) {
        int wx = px, wy = py, wz = pz;
        for (int a = 0; a < 4; ++a) {
          wx += wrand_.next_int(6) - wrand_.next_int(6);
          wy += wrand_.next_int(1) - wrand_.next_int(1);
          wz += wrand_.next_int(6) - wrand_.next_int(6);
          if (!is_normal_cube(wx, wy - 1, wz) || is_normal_cube(wx, wy, wz) ||
              bid::material_liquid(block_id(wx, wy, wz)) || is_normal_cube(wx, wy + 1, wz))
            continue;
          // 24-block player clearance + spawn-point clearance (576 = 24^2).
          if (closest_player_at(wx + 0.5, wy, wz + 0.5, 24.0) != nullptr) continue;
          const double sx = wx + 0.5 - spawn_x_, sy = wy - spawn_y_, sz = wz + 0.5 - spawn_z_;
          if (sx * sx + sy * sy + sz * sz < 576.0) continue;
          std::unique_ptr<entity::Living> mob;
          if (t.peaceful) {
            mob = std::make_unique<entity::Pig>(this);
          } else {
            mob = std::make_unique<entity::Zombie>(this);
          }
          mob->set_position_and_rotation(wx + 0.5, wy, wz + 0.5,
                                         wrand_.next_float() * 360.0f, 0.0f);
          const bool ok = t.peaceful ? static_cast<entity::Pig*>(mob.get())->can_spawn_here()
                                     : static_cast<entity::Zombie*>(mob.get())->can_spawn_here();
          if (!ok) continue;
          ++group;
          entities_.push_back(mob.get());
          mobs_.push_back(std::move(mob));
          ++spawned;
          if (group >= 4) break;
        }
      }
    }
  }
  return spawned;
}

entity::Entity* LiveWorld::closest_player_at(double x, double y, double z, double r) {
  entity::Entity* best = nullptr;
  double best2 = r * r;
  for (entity::Entity* c : entities_) {
    auto* p = dynamic_cast<entity::Player*>(c);
    if (p == nullptr || p->is_dead) continue;
    const double dx = p->pos_x - x, dy = p->pos_y - y, dz = p->pos_z - z;
    const double d2 = dx * dx + dy * dy + dz * dz;
    if (d2 < best2) {
      best2 = d2;
      best = p;
    }
  }
  return best;
}

std::vector<entity::Entity*> LiveWorld::entities_excluding(const entity::Entity& e,
                                                           const Aabb& box) {
  std::vector<entity::Entity*> out;
  auto consider = [&](entity::Entity* c) {
    if (c == nullptr || c == &e || c->is_dead) return;
    if (c->bbox.intersects(box)) out.push_back(c);
  };
  for (entity::Entity* c : entities_) consider(c);
  for (auto& it : items_) consider(it.get());
  return out;
}

void LiveWorld::set_raw(int x, int y, int z, int id, int meta) {
  write_raw(x, y, z, id, meta);
  if (y < 0 || y >= RegionWorld::kHeight) return;
  // Tile entity sync (Chunk.setBlockID container paths).
  const auto key = std::make_tuple(x, y, z);
  auto it = tiles_.find(key);
  const int now_id = region_.get_id(x, y, z);
  auto same_family = [](int a, int b) {
    if (a == b) return true;  // furnace idle/burn swap keeps the tile
    return (a == bid::kFurnaceIdle || a == bid::kFurnaceBurn) &&
           (b == bid::kFurnaceIdle || b == bid::kFurnaceBurn);
  };
  if (it != tiles_.end()) {
    if (!same_family(now_id, it->second->kind_id())) {
      // Container broken: drop chest contents like the source.
      if (auto* chest = dynamic_cast<tile::ChestEntity*>(it->second.get())) {
        for (auto& s : chest->items) {
          if (s.has_value() && s->stack_size > 0) {
            on_item_drop(s->item_id, s->stack_size, s->damage, x + 0.5, y + 0.5, z + 0.5, 0, 0,
                         0);
          }
        }
      }
      tiles_.erase(it);
    }
  } else {
    if (now_id == bid::kChest) {
      auto t = std::make_unique<tile::ChestEntity>();
      t->x = x;
      t->y = y;
      t->z = z;
      tiles_.emplace(key, std::move(t));
    } else if (now_id == bid::kFurnaceIdle || now_id == bid::kFurnaceBurn) {
      auto t = std::make_unique<tile::FurnaceEntity>();
      t->x = x;
      t->y = y;
      t->z = z;
      tiles_.emplace(key, std::move(t));
    } else if (now_id == bid::kSignPost || now_id == bid::kSignWall) {
      auto t = std::make_unique<tile::SignEntity>();
      t->x = x;
      t->y = y;
      t->z = z;
      tiles_.emplace(key, std::move(t));
    }
  }
}

void LiveWorld::write_raw(int x, int y, int z, int id, int meta) {
  // World.setBlockAndMetadata path: Chunk write (relight + light updates via
  // RegionWorld) ; neighbor notify is the caller's (set_and_notify).
  if (meta == 0 && region_.get_meta(x, y, z) == 0) {
    region_.set_id(x, y, z, id);
  } else {
    region_.set_id_meta(x, y, z, id, meta);
  }
  if (y >= 0 && y < RegionWorld::kHeight) dirty_[{x >> 4, z >> 4}] = true;
}

tile::TileEntity* LiveWorld::tile_at(int x, int y, int z) {
  auto it = tiles_.find(std::make_tuple(x, y, z));
  return it == tiles_.end() ? nullptr : it->second.get();
}

void LiveWorld::on_item_drop(int item_id, int count, int damage, double px, double py, double pz,
                             double mx, double my, double mz) {
  auto e = std::make_unique<entity::DroppedItem>(this, px, py, pz,
                                                 entity::ItemStack(item_id, count, damage));
  e->motion_x = mx;
  e->motion_y = my;
  e->motion_z = mz;
  e->pickup_delay = 10;
  items_.push_back(std::move(e));
}

bool LiveWorld::chunks_exist(int x0, int y0, int z0, int x1, int y1, int z1) const {
  // Mirrors World.checkChunksExist y-range gate.
  if (y1 < 0 || y0 >= RegionWorld::kHeight) return false;
  for (int cx = x0 >> 4; cx <= x1 >> 4; ++cx)
    for (int cz = z0 >> 4; cz <= z1 >> 4; ++cz)
      if (!region_.has_chunk(cx, cz)) return false;
  return true;
}

bool LiveWorld::is_normal_cube(int x, int y, int z) const {
  const int id = block_id(x, y, z);
  return bid::is_opaque(id) && bid::renders_as_normal(id);
}

bool LiveWorld::solid_side(int x, int y, int z, bool missing_default) const {
  (void)missing_default;
  return is_normal_cube(x, y, z);
}

bool LiveWorld::material_solid_at(int x, int y, int z) const {
  return bid::material_is_solid(block_id(x, y, z));
}

}  // namespace craftpp::world
