#include "entity/entity.hpp"

#include <cmath>

#include "core/math_helper.hpp"
#include "world/blocks.hpp"

namespace craftpp::entity {
namespace {

int g_next_entity_id = 0;

}  // namespace

void colliding_boxes_for(EntityWorld* w, world::BlockCollider& collider, const Aabb& box,
                         std::vector<Aabb>& out) {
  const int x0 = MathHelper::floor_double(box.min_x);
  const int x1 = MathHelper::floor_double(box.max_x + 1.0);
  const int y0 = MathHelper::floor_double(box.min_y);
  const int y1 = MathHelper::floor_double(box.max_y + 1.0);
  const int z0 = MathHelper::floor_double(box.min_z);
  const int z1 = MathHelper::floor_double(box.max_z + 1.0);
  for (int x = x0; x < x1; ++x) {
    for (int z = z0; z < z1; ++z) {
      for (int y = y0 - 1; y < y1; ++y) {
        const int id = w->block_id(x, y, z);
        if (id == 0) continue;  // blocksList[0] == null
        collider.colliding_boxes(id, w->block_meta(x, y, z), x, y, z, box, *w, out);
      }
    }
  }
}

// Mirrors World.handleMaterialAcceleration for water.
bool handle_water_push(EntityWorld* w, const Aabb& box, Entity& e) {
  const int x0 = MathHelper::floor_double(box.min_x);
  const int x1 = MathHelper::floor_double(box.max_x + 1.0);
  const int y0 = MathHelper::floor_double(box.min_y);
  const int y1 = MathHelper::floor_double(box.max_y + 1.0);
  const int z0 = MathHelper::floor_double(box.min_z);
  const int z1 = MathHelper::floor_double(box.max_z + 1.0);
  if (!w->chunks_exist(x0, y0, z0, x1, y1, z1)) return false;
  bool pushed = false;
  Vec3 push(0.0, 0.0, 0.0);
  for (int x = x0; x < x1; ++x) {
    for (int y = y0; y < y1; ++y) {
      for (int z = z0; z < z1; ++z) {
        const int id = w->block_id(x, y, z);
        if (id != world::bid::kWaterMoving && id != world::bid::kWaterStill) continue;
        const double surface =
            (y + 1) - world::fluid_height_percent(w->block_meta(x, y, z));
        if (y1 >= surface) {
          pushed = true;
          const Vec3 f = world::fluid_flow_vector(id, x, y, z, *w);
          push = push.add(f.x, f.y, f.z);
        }
      }
    }
  }
  if (push.length() > 0.0) {
    const Vec3 n = push.normalize();
    e.motion_x += n.x * 0.014;
    e.motion_y += n.y * 0.014;
    e.motion_z += n.z * 0.014;
  }
  return pushed;
}

// Mirrors World.getIsAnyLiquid (with its odd negative-min decrements).
bool is_any_liquid(EntityWorld* w, const Aabb& box) {
  int x0 = MathHelper::floor_double(box.min_x);
  const int x1 = MathHelper::floor_double(box.max_x + 1.0);
  int y0 = MathHelper::floor_double(box.min_y);
  const int y1 = MathHelper::floor_double(box.max_y + 1.0);
  int z0 = MathHelper::floor_double(box.min_z);
  const int z1 = MathHelper::floor_double(box.max_z + 1.0);
  if (box.min_x < 0.0) --x0;
  if (box.min_y < 0.0) --y0;
  if (box.min_z < 0.0) --z0;
  for (int x = x0; x < x1; ++x)
    for (int y = y0; y < y1; ++y)
      for (int z = z0; z < z1; ++z) {
        const int id = w->block_id(x, y, z);
        if (id == world::bid::kWaterMoving || id == world::bid::kWaterStill ||
            id == world::bid::kLavaMoving || id == world::bid::kLavaStill)
          return true;
      }
  return false;
}

// Mirrors World.isMaterialInBB for lava.
bool is_lava_in_bb(EntityWorld* w, const Aabb& box) {
  const int x0 = MathHelper::floor_double(box.min_x);
  const int x1 = MathHelper::floor_double(box.max_x + 1.0);
  const int y0 = MathHelper::floor_double(box.min_y);
  const int y1 = MathHelper::floor_double(box.max_y + 1.0);
  const int z0 = MathHelper::floor_double(box.min_z);
  const int z1 = MathHelper::floor_double(box.max_z + 1.0);
  for (int x = x0; x < x1; ++x)
    for (int y = y0; y < y1; ++y)
      for (int z = z0; z < z1; ++z) {
        const int id = w->block_id(x, y, z);
        if (id == world::bid::kLavaMoving || id == world::bid::kLavaStill) return true;
      }
  return false;
}

// Mirrors World.isBoundingBoxBurning (fire + both lavas, chunks-gated).
bool is_box_burning(EntityWorld* w, const Aabb& box) {
  const int x0 = MathHelper::floor_double(box.min_x);
  const int x1 = MathHelper::floor_double(box.max_x + 1.0);
  const int y0 = MathHelper::floor_double(box.min_y);
  const int y1 = MathHelper::floor_double(box.max_y + 1.0);
  const int z0 = MathHelper::floor_double(box.min_z);
  const int z1 = MathHelper::floor_double(box.max_z + 1.0);
  if (!w->chunks_exist(x0, y0, z0, x1, y1, z1)) return false;
  for (int x = x0; x < x1; ++x)
    for (int y = y0; y < y1; ++y)
      for (int z = z0; z < z1; ++z) {
        const int id = w->block_id(x, y, z);
        if (id == world::bid::kFire || id == world::bid::kLavaMoving || id == world::bid::kLavaStill)
          return true;
      }
  return false;
}

Entity::Entity(EntityWorld* w) : world(w), entity_id(g_next_entity_id++) {
  set_position(0.0, 0.0, 0.0);
}

void Entity::set_position(double x, double y, double z) {
  pos_x = x;
  pos_y = y;
  pos_z = z;
  const float hw = width / 2.0f;
  const float h = height;
  bbox.set(Aabb(x - hw, y - y_offset + y_size, z - hw, x + hw, y - y_offset + y_size + h, z + hw));
}

void Entity::set_rotation(float yaw, float pitch) {
  rotation_yaw = std::fmod(yaw, 360.0f);
  rotation_pitch = std::fmod(pitch, 360.0f);
}

void Entity::set_position_and_rotation(double x, double y, double z, float yaw, float pitch) {
  prev_pos_x = pos_x = x;
  prev_pos_y = pos_y = y;
  prev_pos_z = pos_z = z;
  prev_rotation_yaw = rotation_yaw = yaw;
  prev_rotation_pitch = rotation_pitch = pitch;
  y_size = 0.0f;
  const double dyaw = prev_rotation_yaw - yaw;
  if (dyaw < -180.0) prev_rotation_yaw += 360.0f;
  if (dyaw >= 180.0) prev_rotation_yaw -= 360.0f;
  set_position(pos_x, pos_y, pos_z);
  set_rotation(yaw, pitch);
}

void Entity::set_location_and_angles(double x, double y, double z, float yaw, float pitch) {
  last_tick_pos_x = prev_pos_x = pos_x = x;
  last_tick_pos_y = prev_pos_y = pos_y = y + y_offset;
  last_tick_pos_z = prev_pos_z = pos_z = z;
  rotation_yaw = yaw;
  rotation_pitch = pitch;
  set_position(pos_x, pos_y, pos_z);
}

bool Entity::is_wet() const {
  if (in_water) return true;
  return world->can_lightning_strike(MathHelper::floor_double(pos_x),
                                    MathHelper::floor_double(pos_y),
                                    MathHelper::floor_double(pos_z));
}

bool Entity::handle_water_movement() {
  return handle_water_push(world, bbox.expand(0.0, -0.4, 0.0).contract(0.001, 0.001, 0.001),
                           *this);
}

bool Entity::handle_lava_movement() {
  return is_lava_in_bb(world, bbox.expand(-0.1, -0.4, -0.1));
}

bool Entity::is_offset_in_liquid(double dx, double dy, double dz) {
  const Aabb moved = bbox.offset_copy(dx, dy, dz);
  std::vector<Aabb> hits;
  colliding_boxes_for(world, world->collider(), moved, hits);
  if (!hits.empty()) return false;
  return !is_any_liquid(world, moved);
}

bool Entity::is_inside_of_material_water() const {
  const double eye = pos_y + eye_height();
  const int x = MathHelper::floor_double(pos_x);
  const int y = MathHelper::floor_double(eye);
  const int z = MathHelper::floor_double(pos_z);
  const int id = world->block_id(x, y, z);
  if ((id == world::bid::kWaterMoving || id == world::bid::kWaterStill)) {
    // Mirrors isInsideOfMaterial exactly (extra -1/9 on the height percent).
    const float var8 = world::fluid_height_percent(world->block_meta(x, y, z)) - 1.0f / 9.0f;
    const float surface = static_cast<float>(y + 1) - var8;
    return eye < surface;
  }
  return false;
}

bool Entity::is_inside_opaque_block() const {
  for (int i = 0; i < 8; ++i) {
    const float ox = (static_cast<float>((i >> 0) % 2) - 0.5f) * width * 0.8f;
    const float oy = (static_cast<float>((i >> 1) % 2) - 0.5f) * 0.1f;
    const float oz = (static_cast<float>((i >> 2) % 2) - 0.5f) * width * 0.8f;
    const int bx = MathHelper::floor_double(pos_x + ox);
    const int by = MathHelper::floor_double(pos_y + eye_height() + oy);
    const int bz = MathHelper::floor_double(pos_z + oz);
    if (world::bid::is_normal_cube(world->block_id(bx, by, bz))) return true;
  }
  return false;
}

void Entity::move_flying(float strafe, float forward, float friction) {
  float len = MathHelper::sqrt_float(strafe * strafe + forward * forward);
  if (len >= 0.01f) {
    if (len < 1.0f) len = 1.0f;
    const float k = friction / len;
    strafe *= k;
    forward *= k;
    const float kPi = 3.14159265358979323846f;
    const float s = MathHelper::sin(rotation_yaw * kPi / 180.0f);
    const float c = MathHelper::cos(rotation_yaw * kPi / 180.0f);
    motion_x += strafe * c - forward * s;
    motion_z += forward * c + strafe * s;
  }
}

void Entity::update_fall_state(double dy, bool grounded) {
  if (grounded) {
    if (fall_distance > 0.0f) {
      fall(fall_distance);
      fall_distance = 0.0f;
    }
  } else if (dy < 0.0) {
    fall_distance = static_cast<float>(fall_distance - dy);
  }
}

void Entity::deal_fire_damage(int amount) {
  if (!is_immune_to_fire) attack(DamageSource::kInFire, amount);
}

void Entity::set_on_fire_from_lava() {
  if (!is_immune_to_fire) {
    attack(DamageSource::kLava, 4);
    const int ticks = 15 * 20;
    if (fire < ticks) fire = ticks;
  }
}

void Entity::on_entity_update() {
  ++ticks_existed;
  prev_distance_walked = distance_walked;
  prev_pos_x = pos_x;
  prev_pos_y = pos_y;
  prev_pos_z = pos_z;
  prev_rotation_pitch = rotation_pitch;
  prev_rotation_yaw = rotation_yaw;
  if (is_sprinting()) {
    const int bx = MathHelper::floor_double(pos_x);
    const int by = MathHelper::floor_double(pos_y - 0.2 - y_offset);
    const int bz = MathHelper::floor_double(pos_z);
    const int id = world->block_id(bx, by, bz);
    if (id > 0) {
      // RNG draws preserved; particle itself is a test no-op.
      const double px = pos_x + (rand.next_float() - 0.5) * width;
      const double pz = pos_z + (rand.next_float() - 0.5) * width;
      world->spawn_particle("tilecrack", px, bbox.min_y + 0.1, pz, -motion_x * 4.0, 1.5,
                            -motion_z * 4.0);
    }
  }
  if (handle_water_movement()) {
    if (!in_water && !first_update) {
      float splash = MathHelper::sqrt_double(motion_x * motion_x * 0.2 + motion_y * motion_y +
                                             motion_z * motion_z * 0.2) *
                     0.2f;
      if (splash > 1.0f) splash = 1.0f;
      world->play_sound("random.splash", splash, 1.0f + (rand.next_float() - rand.next_float()) * 0.4f);
      const float floor_y = static_cast<float>(MathHelper::floor_double(bbox.min_y));
      const int n = static_cast<int>(1.0f + width * 20.0f);
      for (int i = 0; i < n; ++i) {
        const float ox = (rand.next_float() * 2.0f - 1.0f) * width;
        const float oz = (rand.next_float() * 2.0f - 1.0f) * width;
        world->spawn_particle("bubble", pos_x + ox, floor_y + 1.0f, pos_z + oz, motion_x,
                              motion_y - rand.next_float() * 0.2, motion_z);
      }
      for (int i = 0; i < n; ++i) {
        const float ox = (rand.next_float() * 2.0f - 1.0f) * width;
        const float oz = (rand.next_float() * 2.0f - 1.0f) * width;
        world->spawn_particle("splash", pos_x + ox, floor_y + 1.0f, pos_z + oz, motion_x, motion_y,
                              motion_z);
      }
    }
    fall_distance = 0.0f;
    in_water = true;
    fire = 0;
  } else {
    in_water = false;
  }
  if (world->multiplayer()) {
    fire = 0;
  } else if (fire > 0) {
    if (is_immune_to_fire) {
      fire -= 4;
      if (fire < 0) fire = 0;
    } else {
      if (fire % 20 == 0) attack(DamageSource::kOnFire, 1);
      --fire;
    }
  }
  if (handle_lava_movement()) {
    set_on_fire_from_lava();
    fall_distance *= 0.5f;
  }
  if (pos_y < -64.0) kill();
  if (!world->multiplayer()) {
    flag_burning = fire > 0;
    flag_riding = false;
  }
  first_update = false;
}

void Entity::move_entity(double dx, double dy, double dz) {
  if (no_clip) {
    bbox.offset(dx, dy, dz);
    pos_x = (bbox.min_x + bbox.max_x) / 2.0;
    pos_y = bbox.min_y + y_offset - y_size;
    pos_z = (bbox.min_z + bbox.max_z) / 2.0;
    return;
  }
  y_size *= 0.4f;
  const double start_x = pos_x;
  const double start_z = pos_z;
  if (is_in_web) {
    is_in_web = false;
    dx *= 0.25;
    dy *= static_cast<double>(0.05f);
    dz *= 0.25;
    motion_x = 0.0;
    motion_y = 0.0;
    motion_z = 0.0;
  }
  double want_x = dx, want_y = dy, want_z = dz;
  const Aabb start_box = bbox;
  const bool sneaking = on_ground && is_sneaking();
  if (sneaking) {
    constexpr double kStep = 0.05;
    while (dx != 0.0) {
      std::vector<Aabb> hits;
      colliding_boxes_for(world, world->collider(), bbox.offset_copy(dx, -1.0, 0.0), hits);
      if (!hits.empty()) break;
      if (dx < kStep && dx >= -kStep) {
        dx = 0.0;
      } else if (dx > 0.0) {
        dx -= kStep;
      } else {
        dx += kStep;
      }
      want_x = dx;  // update-expression assignment AFTER the body (F18)
    }
    while (dz != 0.0) {
      std::vector<Aabb> hits;
      colliding_boxes_for(world, world->collider(), bbox.offset_copy(0.0, -1.0, dz), hits);
      if (!hits.empty()) break;
      if (dz < kStep && dz >= -kStep) {
        dz = 0.0;
      } else if (dz > 0.0) {
        dz -= kStep;
      } else {
        dz += kStep;
      }
      want_z = dz;
    }
  }

  std::vector<Aabb> hits;
  colliding_boxes_for(world, world->collider(), bbox.add_coord(dx, dy, dz), hits);
  for (const Aabb& b : hits) dy = b.clamp_y(bbox, dy);
  bbox.offset(0.0, dy, 0.0);
  if (!field_9293_aM && want_y != dy) {
    dx = dy = dz = 0.0;
  }
  const bool stepped_or_falling = on_ground || (want_y != dy && want_y < 0.0);
  for (const Aabb& b : hits) dx = b.clamp_x(bbox, dx);
  bbox.offset(dx, 0.0, 0.0);
  if (!field_9293_aM && want_x != dx) {
    dx = dy = dz = 0.0;
  }
  for (const Aabb& b : hits) dz = b.clamp_z(bbox, dz);
  bbox.offset(0.0, 0.0, dz);
  if (!field_9293_aM && want_z != dz) {
    dx = dy = dz = 0.0;
  }

  if (step_height > 0.0f && stepped_or_falling && (sneaking || y_size < 0.05f) &&
      (want_x != dx || want_z != dz)) {
    const double orig_x = dx, orig_y = dy, orig_z = dz;
    dx = want_x;
    dy = step_height;
    dz = want_z;
    const Aabb pre_step = bbox;
    bbox.set(start_box);
    std::vector<Aabb> step_hits;
    colliding_boxes_for(world, world->collider(), bbox.add_coord(dx, dy, dz), step_hits);
    for (const Aabb& b : step_hits) dy = b.clamp_y(bbox, dy);
    bbox.offset(0.0, dy, 0.0);
    if (!field_9293_aM && want_y != dy) {
      dx = dy = dz = 0.0;
    }
    for (const Aabb& b : step_hits) dx = b.clamp_x(bbox, dx);
    bbox.offset(dx, 0.0, 0.0);
    if (!field_9293_aM && want_x != dx) {
      dx = dy = dz = 0.0;
    }
    for (const Aabb& b : step_hits) dz = b.clamp_z(bbox, dz);
    bbox.offset(0.0, 0.0, dz);
    if (!field_9293_aM && want_z != dz) {
      dx = dy = dz = 0.0;
    }
    if (!field_9293_aM && want_y != dy) {
      dx = dy = dz = 0.0;
    } else {
      dy = -step_height;
      for (const Aabb& b : step_hits) dy = b.clamp_y(bbox, dy);
      bbox.offset(0.0, dy, 0.0);
    }
    if (orig_x * orig_x + orig_z * orig_z >= dx * dx + dz * dz) {
      dx = orig_x;
      dy = orig_y;
      dz = orig_z;
      bbox.set(pre_step);
    } else {
      const double frac = bbox.min_y - static_cast<double>(static_cast<int>(bbox.min_y));
      if (frac > 0.0) y_size = static_cast<float>(y_size + frac + 0.01);
    }
  }

  pos_x = (bbox.min_x + bbox.max_x) / 2.0;
  pos_y = bbox.min_y + y_offset - y_size;
  pos_z = (bbox.min_z + bbox.max_z) / 2.0;
  collided_horizontally = want_x != dx || want_z != dz;
  collided_vertically = want_y != dy;
  on_ground = want_y != dy && want_y < 0.0;
  collided = collided_horizontally || collided_vertically;
  update_fall_state(dy, on_ground);
  if (want_x != dx) motion_x = 0.0;
  if (want_y != dy) motion_y = 0.0;
  if (want_z != dz) motion_z = 0.0;

  const double trav_x = pos_x - start_x;
  const double trav_z = pos_z - start_z;
  if (can_trigger_walking() && !sneaking) {
    distance_walked = static_cast<float>(
        distance_walked + static_cast<double>(MathHelper::sqrt_double(trav_x * trav_x + trav_z * trav_z)) * 0.6);
    const int bx = MathHelper::floor_double(pos_x);
    const int by = MathHelper::floor_double(pos_y - 0.2 - y_offset);
    const int bz = MathHelper::floor_double(pos_z);
    int id = world->block_id(bx, by, bz);
    if (id == 0 && world->block_id(bx, by - 1, bz) == world::bid::kFence) {
      id = world->block_id(bx, by - 1, bz);
    }
    if (distance_walked > next_step_distance && id > 0) {
      next_step_distance = static_cast<int>(distance_walked) + 1;
      world->on_entity_walk(id, bx, by, bz);
    }
  }

  const int cx0 = MathHelper::floor_double(bbox.min_x + 0.001);
  const int cy0 = MathHelper::floor_double(bbox.min_y + 0.001);
  const int cz0 = MathHelper::floor_double(bbox.min_z + 0.001);
  const int cx1 = MathHelper::floor_double(bbox.max_x - 0.001);
  const int cy1 = MathHelper::floor_double(bbox.max_y - 0.001);
  const int cz1 = MathHelper::floor_double(bbox.max_z - 0.001);
  if (world->chunks_exist(cx0, cy0, cz0, cx1, cy1, cz1)) {
    for (int x = cx0; x <= cx1; ++x)
      for (int y = cy0; y <= cy1; ++y)
        for (int z = cz0; z <= cz1; ++z) {
          const int id = world->block_id(x, y, z);
          if (id == world::bid::kWeb) {
            is_in_web = true;
          } else if (id == world::bid::kSoulSand) {
            motion_x *= 0.4;
            motion_z *= 0.4;
          } else if (id == world::bid::kCactus) {
            attack(DamageSource::kCactus, 1);
          } else if (id > 0) {
            world->on_entity_collided_cell(id, x, y, z);
          }
        }
  }

  const bool wet = is_wet();
  if (is_box_burning(world, bbox.contract(0.001, 0.001, 0.001))) {
    deal_fire_damage(1);
    if (!wet) {
      ++fire;
      if (fire == 0) {
        const int ticks = 8 * 20;
        if (fire < ticks) fire = ticks;
      }
    }
  } else if (fire <= 0) {
    fire = -fire_resistance;
  }
  if (wet && fire > 0) {
    world->play_sound("random.fizz", 0.7f,
                      1.6f + (rand.next_float() - rand.next_float()) * 0.4f);
    fire = -fire_resistance;
  }
}

}  // namespace craftpp::entity
