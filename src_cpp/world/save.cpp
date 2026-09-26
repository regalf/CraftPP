#include "world/save.hpp"

#include <cmath>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>

#include "entity/pig.hpp"
#include "entity/zombie.hpp"
#include "entity/player.hpp"
#include "world/live.hpp"
#include "world/mcregion.hpp"

namespace craftpp::world {

namespace {

nbt::Tag byte_array_tag(const std::uint8_t* data, std::size_t len) {
  std::vector<std::int8_t> v;
  v.reserve(len);
  for (std::size_t i = 0; i < len; ++i) v.push_back(static_cast<std::int8_t>(data[i]));
  return nbt::Tag::make_byte_array(std::move(v));
}

// Packs per-block bytes (meta) into a 16 KiB NibbleArray image.
std::vector<std::uint8_t> pack_nibbles(const std::vector<std::uint8_t>& cells) {
  std::vector<std::uint8_t> out(16384, 0);
  for (int lx = 0; lx < 16; ++lx) {
    for (int lz = 0; lz < 16; ++lz) {
      for (int y = 0; y < RegionWorld::kHeight; ++y) {
        const std::size_t flat = static_cast<std::size_t>((lx * 16 + lz) * RegionWorld::kHeight + y);
        const std::size_t ni =
            (static_cast<std::size_t>(lx) << 11) | (static_cast<std::size_t>(lz) << 7) |
            static_cast<std::size_t>(y);
        const int v = cells[flat] & 15;
        if ((ni & 1) == 0) {
          out[ni >> 1] = static_cast<std::uint8_t>(out[ni >> 1] | v);
        } else {
          out[ni >> 1] = static_cast<std::uint8_t>(out[ni >> 1] | (v << 4));
        }
      }
    }
  }
  return out;
}

std::vector<std::uint8_t> unpack_bytes(const nbt::Tag& t, std::size_t want) {
  if (t.type != nbt::TagType::ByteArray || t.bytes.size() != want) {
    throw std::runtime_error("save: bad byte array");
  }
  std::vector<std::uint8_t> out(want);
  for (std::size_t i = 0; i < want; ++i) out[i] = static_cast<std::uint8_t>(t.bytes[i]);
  return out;
}

const nbt::Tag& need(const nbt::TagCompound& c, const std::string& key) {
  const nbt::Tag* t = c.find(key);
  if (t == nullptr) throw std::runtime_error("save: missing " + key);
  return *t;
}

// ---- entity/tile NBT (mirrors Entity/TileEntity/ItemStack write paths) ----

nbt::Tag double_list(double a, double b, double c) {
  return nbt::Tag::make_list(nbt::TagType::Double,
                             {nbt::Tag::make_double(a), nbt::Tag::make_double(b),
                              nbt::Tag::make_double(c)});
}

nbt::Tag float_list(float a, float b) {
  return nbt::Tag::make_list(nbt::TagType::Float,
                             {nbt::Tag::make_float(a), nbt::Tag::make_float(b)});
}

// Entity.writeToNBT base tags; feet_y is the vanilla posY (feet) convention.
void set_entity_base(nbt::TagCompound& c, const entity::Entity& e, double feet_y, int air) {
  c.set("Pos", double_list(e.pos_x, feet_y, e.pos_z));
  c.set("Motion", double_list(e.motion_x, e.motion_y, e.motion_z));
  c.set("Rotation", float_list(e.rotation_yaw, e.rotation_pitch));
  c.set("FallDistance", nbt::Tag::make_float(e.fall_distance));
  c.set("Fire", nbt::Tag::make_short(static_cast<std::int16_t>(e.fire)));
  c.set("Air", nbt::Tag::make_short(static_cast<std::int16_t>(air)));
  c.set("OnGround", nbt::Tag::make_byte(e.on_ground ? 1 : 0));
}

void set_living_tags(nbt::TagCompound& c, const entity::Living& m) {
  c.set("Health", nbt::Tag::make_short(static_cast<std::int16_t>(m.health)));
  c.set("HurtTime", nbt::Tag::make_short(static_cast<std::int16_t>(m.hurt_time)));
  c.set("DeathTime", nbt::Tag::make_short(static_cast<std::int16_t>(m.death_time)));
  c.set("AttackTime", nbt::Tag::make_short(static_cast<std::int16_t>(m.attack_time)));
}

nbt::Tag item_stack_tag(const entity::ItemStack& s) {
  nbt::TagCompound c;
  c.set("id", nbt::Tag::make_short(static_cast<std::int16_t>(s.item_id)));
  c.set("Count", nbt::Tag::make_byte(static_cast<std::int8_t>(s.stack_size)));
  c.set("Damage", nbt::Tag::make_short(static_cast<std::int16_t>(s.damage)));
  return nbt::Tag::make_compound(std::move(c));
}

entity::ItemStack item_stack_from_tag(const nbt::Tag& t) {
  const nbt::TagCompound& c = *t.compound;
  return entity::ItemStack(need(c, "id").i16, need(c, "Count").i8, need(c, "Damage").i16);
}

// addEntityID mapping for the kinds we simulate.
const char* entity_kind_id(const entity::Entity& e) {
  if (dynamic_cast<const entity::Zombie*>(&e) != nullptr) return "Zombie";
  if (dynamic_cast<const entity::Pig*>(&e) != nullptr) return "Pig";
  if (dynamic_cast<const entity::DroppedItem*>(&e) != nullptr) return "Item";
  return nullptr;
}

nbt::Tag mob_tag(const entity::Living& m, const char* id) {
  nbt::TagCompound c;
  c.set("id", nbt::Tag::make_string(id));
  set_entity_base(c, m, m.pos_y, m.air_supply);
  set_living_tags(c, m);
  return nbt::Tag::make_compound(std::move(c));
}

nbt::Tag drop_tag(const entity::DroppedItem& d) {
  nbt::TagCompound c;
  c.set("id", nbt::Tag::make_string("Item"));
  set_entity_base(c, d, d.pos_y, 300);
  c.set("Health", nbt::Tag::make_short(static_cast<std::int16_t>(d.health & 255)));
  c.set("Age", nbt::Tag::make_short(static_cast<std::int16_t>(d.age)));
  c.set("Item", item_stack_tag(d.item));
  return nbt::Tag::make_compound(std::move(c));
}

nbt::Tag tile_tag(const tile::TileEntity& t) {
  nbt::TagCompound c;
  if (const auto* ch = dynamic_cast<const tile::ChestEntity*>(&t)) {
    c.set("id", nbt::Tag::make_string("Chest"));
    c.set("x", nbt::Tag::make_int(ch->x));
    c.set("y", nbt::Tag::make_int(ch->y));
    c.set("z", nbt::Tag::make_int(ch->z));
    std::vector<nbt::Tag> items;
    for (std::size_t s = 0; s < ch->items.size(); ++s) {
      if (!ch->items[s].has_value()) continue;
      nbt::TagCompound sc;
      sc.set("Slot", nbt::Tag::make_byte(static_cast<std::int8_t>(s)));
      nbt::Tag stack = item_stack_tag(*ch->items[s]);
      for (auto& [k, v] : stack.compound->entries) sc.set(k, v);
      items.push_back(nbt::Tag::make_compound(std::move(sc)));
    }
    c.set("Items", nbt::Tag::make_list(nbt::TagType::Compound, std::move(items)));
  } else if (const auto* fu = dynamic_cast<const tile::FurnaceEntity*>(&t)) {
    c.set("id", nbt::Tag::make_string("Furnace"));
    c.set("x", nbt::Tag::make_int(fu->x));
    c.set("y", nbt::Tag::make_int(fu->y));
    c.set("z", nbt::Tag::make_int(fu->z));
    c.set("BurnTime", nbt::Tag::make_short(static_cast<std::int16_t>(fu->burn_time)));
    c.set("CookTime", nbt::Tag::make_short(static_cast<std::int16_t>(fu->cook_time)));
    std::vector<nbt::Tag> items;
    for (std::size_t s = 0; s < fu->items.size(); ++s) {
      if (!fu->items[s].has_value()) continue;
      nbt::TagCompound sc;
      sc.set("Slot", nbt::Tag::make_byte(static_cast<std::int8_t>(s)));
      nbt::Tag stack = item_stack_tag(*fu->items[s]);
      for (auto& [k, v] : stack.compound->entries) sc.set(k, v);
      items.push_back(nbt::Tag::make_compound(std::move(sc)));
    }
    c.set("Items", nbt::Tag::make_list(nbt::TagType::Compound, std::move(items)));
  } else if (const auto* sg = dynamic_cast<const tile::SignEntity*>(&t)) {
    c.set("id", nbt::Tag::make_string("Sign"));
    c.set("x", nbt::Tag::make_int(sg->x));
    c.set("y", nbt::Tag::make_int(sg->y));
    c.set("z", nbt::Tag::make_int(sg->z));
    for (int i = 0; i < 4; ++i) {
      c.set("Text" + std::to_string(i + 1), nbt::Tag::make_string(sg->lines[i]));
    }
  } else {
    throw std::runtime_error("save: unknown tile kind");
  }
  return nbt::Tag::make_compound(std::move(c));
}

void read_entity_base(const nbt::TagCompound& c, entity::Entity& e, double& feet_y) {
  const nbt::Tag& pos = need(c, "Pos");
  const nbt::Tag& mot = need(c, "Motion");
  const nbt::Tag& rot = need(c, "Rotation");
  feet_y = pos.list->items[1].f64;
  e.pos_x = pos.list->items[0].f64;
  e.pos_y = feet_y;
  e.pos_z = pos.list->items[2].f64;
  e.prev_pos_x = e.last_tick_pos_x = e.pos_x;
  e.prev_pos_y = e.last_tick_pos_y = e.pos_y;
  e.prev_pos_z = e.last_tick_pos_z = e.pos_z;
  e.motion_x = mot.list->items[0].f64;
  e.motion_y = mot.list->items[1].f64;
  e.motion_z = mot.list->items[2].f64;
  e.rotation_yaw = e.prev_rotation_yaw = rot.list->items[0].f32;
  e.rotation_pitch = e.prev_rotation_pitch = rot.list->items[1].f32;
  e.fall_distance = need(c, "FallDistance").f32;
  e.fire = need(c, "Fire").i16;
  e.on_ground = need(c, "OnGround").i8 != 0;
  // Motion clamp mirrors readFromNBT (>10 -> 0).
  if (std::abs(e.motion_x) > 10.0) e.motion_x = 0.0;
  if (std::abs(e.motion_y) > 10.0) e.motion_y = 0.0;
  if (std::abs(e.motion_z) > 10.0) e.motion_z = 0.0;
}

}  // namespace

nbt::Tag level_dat_tag(const WorldInfoData& info) {
  nbt::TagCompound data;
  data.set("RandomSeed", nbt::Tag::make_long(info.seed));
  data.set("GameType", nbt::Tag::make_int(info.game_type));
  data.set("MapFeatures", nbt::Tag::make_byte(info.map_features ? 1 : 0));
  data.set("SpawnX", nbt::Tag::make_int(info.spawn_x));
  data.set("SpawnY", nbt::Tag::make_int(info.spawn_y));
  data.set("SpawnZ", nbt::Tag::make_int(info.spawn_z));
  data.set("Time", nbt::Tag::make_long(info.time));
  data.set("SizeOnDisk", nbt::Tag::make_long(0));
  data.set("LastPlayed", nbt::Tag::make_long(static_cast<std::int64_t>(std::time(nullptr)) * 1000));
  data.set("LevelName", nbt::Tag::make_string(info.level_name));
  data.set("version", nbt::Tag::make_int(info.version));
  data.set("rainTime", nbt::Tag::make_int(0));
  data.set("raining", nbt::Tag::make_byte(0));
  data.set("thunderTime", nbt::Tag::make_int(0));
  data.set("thundering", nbt::Tag::make_byte(0));
  data.set("hardcore", nbt::Tag::make_byte(0));
  if (info.player.has_value()) data.set("Player", *info.player);
  nbt::TagCompound root;
  root.set("Data", nbt::Tag::make_compound(std::move(data)));
  return nbt::Tag::make_compound(std::move(root));
}

WorldInfoData world_info_from_tag(const nbt::Tag& root) {
  if (root.type != nbt::TagType::Compound) throw std::runtime_error("save: bad level.dat root");
  const nbt::Tag& data = need(*root.compound, "Data");
  const nbt::TagCompound& c = *data.compound;
  WorldInfoData info;
  info.seed = need(c, "RandomSeed").i64;
  info.game_type = need(c, "GameType").i32;
  info.map_features = need(c, "MapFeatures").i8 != 0;
  info.spawn_x = need(c, "SpawnX").i32;
  info.spawn_y = need(c, "SpawnY").i32;
  info.spawn_z = need(c, "SpawnZ").i32;
  info.time = need(c, "Time").i64;
  info.level_name = need(c, "LevelName").str;
  info.version = need(c, "version").i32;
  if (const nbt::Tag* p = c.find("Player"); p != nullptr) info.player = *p;
  return info;
}

nbt::Tag chunk_to_tag(const LiveWorld& world, int cx, int cz) {
  nbt::TagCompound level;
  level.set("xPos", nbt::Tag::make_int(cx));
  level.set("zPos", nbt::Tag::make_int(cz));
  level.set("LastUpdate", nbt::Tag::make_long(world.world_time()));
  const std::vector<std::int8_t> ids = world.chunk_ids(cx, cz);
  if (ids.size() != 32768) throw std::runtime_error("save: chunk not provided");
  level.set("Blocks", nbt::Tag::make_byte_array(ids));
  const std::vector<std::uint8_t> meta = world.chunk_metadata(cx, cz);
  const std::vector<std::uint8_t> packed_meta = pack_nibbles(meta);
  level.set("Data", byte_array_tag(packed_meta.data(), packed_meta.size()));
  const std::vector<std::uint8_t> sky = world.chunk_skylight(cx, cz);
  level.set("SkyLight", byte_array_tag(sky.data(), sky.size()));
  const std::vector<std::uint8_t> block = world.chunk_blocklight(cx, cz);
  level.set("BlockLight", byte_array_tag(block.data(), block.size()));
  const std::vector<std::uint8_t> height = world.chunk_heightmap(cx, cz);
  level.set("HeightMap", byte_array_tag(height.data(), height.size()));
  level.set("TerrainPopulated", nbt::Tag::make_byte(world.is_populated(cx, cz) ? 1 : 0));
  // Entities whose block position falls in this chunk (vanilla stores them
  // in the chunk entity lists).
  std::vector<nbt::Tag> entities;
  for (const auto& m : world.mobs()) {
    if ((static_cast<int>(std::floor(m->pos_x)) >> 4) != cx ||
        (static_cast<int>(std::floor(m->pos_z)) >> 4) != cz) {
      continue;
    }
    if (const char* id = entity_kind_id(*m); id != nullptr) {
      entities.push_back(mob_tag(*m, id));
    }
  }
  for (const auto& d : world.items()) {
    if ((static_cast<int>(std::floor(d->pos_x)) >> 4) != cx ||
        (static_cast<int>(std::floor(d->pos_z)) >> 4) != cz) {
      continue;
    }
    entities.push_back(drop_tag(*d));
  }
  level.set("Entities", nbt::Tag::make_list(nbt::TagType::Compound, std::move(entities)));
  // Tile entities in this chunk.
  std::vector<nbt::Tag> tiles;
  for (const tile::TileEntity* t : world.tile_entities()) {
    if ((t->x >> 4) != cx || (t->z >> 4) != cz) continue;
    tiles.push_back(tile_tag(*t));
  }
  level.set("TileEntities", nbt::Tag::make_list(nbt::TagType::Compound, std::move(tiles)));
  // Scheduled ticks in this chunk, relative to world time.
  std::vector<nbt::Tag> ticks;
  for (const tick::ScheduledTick& s : world.scheduled_ticks()) {
    if ((s.x >> 4) != cx || (s.z >> 4) != cz) continue;
    nbt::TagCompound tc;
    tc.set("i", nbt::Tag::make_int(s.id));
    tc.set("x", nbt::Tag::make_int(s.x));
    tc.set("y", nbt::Tag::make_int(s.y));
    tc.set("z", nbt::Tag::make_int(s.z));
    tc.set("t", nbt::Tag::make_int(static_cast<int>(s.time - world.world_time())));
    ticks.push_back(nbt::Tag::make_compound(std::move(tc)));
  }
  level.set("TileTicks", nbt::Tag::make_list(nbt::TagType::Compound, std::move(ticks)));
  nbt::TagCompound root;
  root.set("Level", nbt::Tag::make_compound(std::move(level)));
  return nbt::Tag::make_compound(std::move(root));
}

bool chunk_from_tag(const nbt::Tag& root, LiveWorld& world, int cx, int cz) {
  if (root.type != nbt::TagType::Compound) return false;
  const nbt::Tag* l = root.compound->find("Level");
  if (l == nullptr || l->type != nbt::TagType::Compound) return false;
  const nbt::TagCompound& c = *l->compound;
  try {
    const int x = need(c, "xPos").i32;
    const int z = need(c, "zPos").i32;
    if (x != cx || z != cz) return false;
    const std::vector<std::uint8_t> blocks = unpack_bytes(need(c, "Blocks"), 32768);
    const std::vector<std::uint8_t> data = unpack_bytes(need(c, "Data"), 16384);
    const std::vector<std::uint8_t> sky = unpack_bytes(need(c, "SkyLight"), 16384);
    const std::vector<std::uint8_t> bl = unpack_bytes(need(c, "BlockLight"), 16384);
    const std::vector<std::uint8_t> height = unpack_bytes(need(c, "HeightMap"), 256);
    std::vector<std::int8_t> ids(32768);
    for (std::size_t i = 0; i < ids.size(); ++i) ids[i] = static_cast<std::int8_t>(blocks[i]);
    // Unpack Data nibbles into per-block metadata.
    std::vector<std::uint8_t> meta(32768, 0);
    for (int lx = 0; lx < 16; ++lx) {
      for (int lz = 0; lz < 16; ++lz) {
        for (int y = 0; y < RegionWorld::kHeight; ++y) {
          const std::size_t flat =
              static_cast<std::size_t>((lx * 16 + lz) * RegionWorld::kHeight + y);
          const std::size_t ni = (static_cast<std::size_t>(lx) << 11) |
                                 (static_cast<std::size_t>(lz) << 7) | static_cast<std::size_t>(y);
          const int b = data[ni >> 1];
          meta[flat] = static_cast<std::uint8_t>((ni & 1) == 0 ? (b & 15) : ((b >> 4) & 15));
        }
      }
    }
    world.install_chunk(cx, cz, ids.data(), meta.data(), sky.data(), bl.data(), height.data());
    if (need(c, "TerrainPopulated").i8 != 0) world.mark_populated(cx, cz);
    // Entities (EntityList ids we write: Zombie, Pig, Item).
    if (const nbt::Tag* el = c.find("Entities");
        el != nullptr && el->type == nbt::TagType::List) {
      for (const nbt::Tag& e : el->list->items) {
        if (e.type != nbt::TagType::Compound) continue;
        const nbt::TagCompound& ec = *e.compound;
        try {
          const std::string& id = need(ec, "id").str;
          double feet = 0.0;
          if (id == "Zombie" || id == "Pig") {
            auto m = id == "Zombie" ? static_cast<std::unique_ptr<entity::Living>>(
                                          std::make_unique<entity::Zombie>(&world))
                                    : static_cast<std::unique_ptr<entity::Living>>(
                                          std::make_unique<entity::Pig>(&world));
            read_entity_base(ec, *m, feet);
            m->pos_y = feet;  // mob y_offset is 0
            m->health = need(ec, "Health").i16;
            m->hurt_time = need(ec, "HurtTime").i16;
            m->death_time = need(ec, "DeathTime").i16;
            m->attack_time = need(ec, "AttackTime").i16;
            if (const nbt::Tag* a = ec.find("Air"); a != nullptr) m->air_supply = a->i16;
            world.adopt_mob(std::move(m));
          } else if (id == "Item") {
            entity::ItemStack stack = item_stack_from_tag(need(ec, "Item"));
            auto d = std::make_unique<entity::DroppedItem>(&world, 0.0, -100.0, 0.0, stack,
                                                           0.0, 0.0, 0.0);
            read_entity_base(ec, *d, feet);
            d->pos_y = feet;
            d->prev_pos_y = d->last_tick_pos_y = feet;
            d->health = need(ec, "Health").i16 & 255;
            d->age = need(ec, "Age").i16;
            d->pickup_delay = 0;  // not persisted (matches the source)
            world.adopt_item(std::move(d));
          }
        } catch (const std::runtime_error&) {
        }
      }
    }
    // Tile entities.
    if (const nbt::Tag* tl = c.find("TileEntities");
        tl != nullptr && tl->type == nbt::TagType::List) {
      for (const nbt::Tag& e : tl->list->items) {
        if (e.type != nbt::TagType::Compound) continue;
        const nbt::TagCompound& ec = *e.compound;
        try {
          const std::string& id = need(ec, "id").str;
          const int tx = need(ec, "x").i32;
          const int ty = need(ec, "y").i32;
          const int tz = need(ec, "z").i32;
          auto read_items = [&](auto& slots) {
            if (const nbt::Tag* il = ec.find("Items");
                il != nullptr && il->type == nbt::TagType::List) {
              for (const nbt::Tag& s : il->list->items) {
                if (s.type != nbt::TagType::Compound) continue;
                const int slot = need(*s.compound, "Slot").i8;
                if (slot < 0 || static_cast<std::size_t>(slot) >= slots.size()) continue;
                slots[slot] = item_stack_from_tag(s);
              }
            }
          };
          if (id == "Chest") {
            auto t = std::make_unique<tile::ChestEntity>();
            t->x = tx;
            t->y = ty;
            t->z = tz;
            read_items(t->items);
            world.set_tile(std::move(t));
          } else if (id == "Furnace") {
            auto t = std::make_unique<tile::FurnaceEntity>();
            t->x = tx;
            t->y = ty;
            t->z = tz;
            t->burn_time = need(ec, "BurnTime").i16;
            t->cook_time = need(ec, "CookTime").i16;
            read_items(t->items);
            world.set_tile(std::move(t));
          } else if (id == "Sign") {
            auto t = std::make_unique<tile::SignEntity>();
            t->x = tx;
            t->y = ty;
            t->z = tz;
            for (int i = 0; i < 4; ++i) {
              if (const nbt::Tag* l = ec.find("Text" + std::to_string(i + 1)); l != nullptr) {
                t->lines[i] = l->str;
              }
            }
            world.set_tile(std::move(t));
          }
        } catch (const std::runtime_error&) {
        }
      }
    }
    // Scheduled ticks (relative delays from load time, like the source).
    if (const nbt::Tag* kl = c.find("TileTicks");
        kl != nullptr && kl->type == nbt::TagType::List) {
      for (const nbt::Tag& e : kl->list->items) {
        if (e.type != nbt::TagType::Compound) continue;
        const nbt::TagCompound& kc = *e.compound;
        try {
          world.schedule_loaded_tick(need(kc, "x").i32, need(kc, "y").i32, need(kc, "z").i32,
                                     need(kc, "i").i32, need(kc, "t").i32);
        } catch (const std::runtime_error&) {
        }
      }
    }
    return true;
  } catch (const std::runtime_error&) {
    return false;
  }
}

std::optional<WorldInfoData> read_level_dat(const std::string& save_dir) {
  for (const char* name : {"level.dat", "level.dat_old"}) {
    std::ifstream in(save_dir + "/" + name, std::ios::binary);
    if (!in) continue;
    const std::vector<std::uint8_t> gz{std::istreambuf_iterator<char>(in),
                                         std::istreambuf_iterator<char>()};
    try {
      const std::vector<std::uint8_t> raw = nbt::gzip_decompress(gz.data(), gz.size());
      nbt::Reader r(raw.data(), raw.size());
      return world_info_from_tag(nbt::read_root(r));
    } catch (const std::exception&) {
    }
  }
  return std::nullopt;
}

void write_level_dat(const std::string& save_dir, const WorldInfoData& info) {
  std::filesystem::create_directories(save_dir);
  nbt::Writer w;
  nbt::write_root(w, "", level_dat_tag(info));
  const std::vector<std::uint8_t>& raw = w.bytes();
  const std::vector<std::uint8_t> gz = nbt::gzip_compress(raw.data(), raw.size());
  const std::string dat = save_dir + "/level.dat";
  const std::string old = save_dir + "/level.dat_old";
  const std::string cur = save_dir + "/level.dat_new";
  {
    std::ofstream out(cur, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("save: cannot write " + cur);
    out.write(reinterpret_cast<const char*>(gz.data()), static_cast<std::streamsize>(gz.size()));
  }
  std::error_code ec;
  std::filesystem::remove(old, ec);
  std::filesystem::rename(dat, old, ec);
  std::filesystem::rename(cur, dat, ec);
}

nbt::Tag player_tag(const entity::Player& p) {
  nbt::TagCompound c;
  c.set("id", nbt::Tag::make_string("Player"));
  set_entity_base(c, p, p.pos_y - p.y_offset, p.air_supply);
  set_living_tags(c, p);
  // InventoryPlayer.writeToNBT: main slots 0-35, armor 100-103.
  std::vector<nbt::Tag> inv;
  for (std::size_t s = 0; s < p.inventory.main.size(); ++s) {
    if (!p.inventory.main[s].has_value()) continue;
    nbt::TagCompound sc;
    sc.set("Slot", nbt::Tag::make_byte(static_cast<std::int8_t>(s)));
    nbt::Tag stack = item_stack_tag(*p.inventory.main[s]);
    for (auto& [k, v] : stack.compound->entries) sc.set(k, v);
    inv.push_back(nbt::Tag::make_compound(std::move(sc)));
  }
  for (std::size_t s = 0; s < p.inventory.armor.size(); ++s) {
    if (!p.inventory.armor[s].has_value()) continue;
    nbt::TagCompound sc;
    sc.set("Slot", nbt::Tag::make_byte(static_cast<std::int8_t>(s + 100)));
    nbt::Tag stack = item_stack_tag(*p.inventory.armor[s]);
    for (auto& [k, v] : stack.compound->entries) sc.set(k, v);
    inv.push_back(nbt::Tag::make_compound(std::move(sc)));
  }
  c.set("Inventory", nbt::Tag::make_list(nbt::TagType::Compound, std::move(inv)));
  c.set("Dimension", nbt::Tag::make_int(p.dimension));
  c.set("Sleeping", nbt::Tag::make_byte(p.sleeping ? 1 : 0));
  c.set("SleepTimer", nbt::Tag::make_short(0));
  c.set("XpP", nbt::Tag::make_float(p.current_xp));
  c.set("XpLevel", nbt::Tag::make_int(p.player_level));
  c.set("XpTotal", nbt::Tag::make_int(p.total_xp));
  c.set("Score", nbt::Tag::make_int(p.score));
  c.set("foodLevel", nbt::Tag::make_int(p.food.food_level));
  c.set("foodTickTimer", nbt::Tag::make_int(0));
  c.set("foodSaturationLevel", nbt::Tag::make_float(p.food.saturation));
  c.set("foodExhaustionLevel", nbt::Tag::make_float(p.food.exhaustion));
  return nbt::Tag::make_compound(std::move(c));
}

void apply_player_tag(entity::Player& p, const nbt::Tag& tag) {
  if (tag.type != nbt::TagType::Compound) return;
  const nbt::TagCompound& c = *tag.compound;
  try {
    double feet = 0.0;
    read_entity_base(c, p, feet);
    p.pos_y = feet + p.y_offset;  // feet + 1.62 convention
    p.prev_pos_y = p.last_tick_pos_y = p.pos_y;
    p.health = need(c, "Health").i16;
    p.hurt_time = need(c, "HurtTime").i16;
    p.death_time = need(c, "DeathTime").i16;
    p.attack_time = need(c, "AttackTime").i16;
    if (const nbt::Tag* a = c.find("Air"); a != nullptr) p.air_supply = a->i16;
    if (const nbt::Tag* inv = c.find("Inventory");
        inv != nullptr && inv->type == nbt::TagType::List) {
      for (auto& s : p.inventory.main) s.reset();
      for (auto& s : p.inventory.armor) s.reset();
      for (const nbt::Tag& s : inv->list->items) {
        if (s.type != nbt::TagType::Compound) continue;
        const int slot = need(*s.compound, "Slot").i8;
        entity::ItemStack stack = item_stack_from_tag(s);
        if (slot >= 0 && slot < 36) {
          p.inventory.main[slot] = stack;
        } else if (slot >= 100 && slot < 104) {
          p.inventory.armor[slot - 100] = stack;
        }
      }
    }
    if (const nbt::Tag* d = c.find("Dimension"); d != nullptr) p.dimension = d->i32;
    if (const nbt::Tag* f = c.find("foodLevel"); f != nullptr) p.food.food_level = f->i32;
    if (const nbt::Tag* f = c.find("foodSaturationLevel"); f != nullptr) {
      p.food.saturation = f->f32;
    }
    if (const nbt::Tag* f = c.find("foodExhaustionLevel"); f != nullptr) {
      p.food.exhaustion = f->f32;
    }
    if (const nbt::Tag* x = c.find("XpP"); x != nullptr) p.current_xp = x->f32;
    if (const nbt::Tag* x = c.find("XpLevel"); x != nullptr) p.player_level = x->i32;
    if (const nbt::Tag* x = c.find("XpTotal"); x != nullptr) p.total_xp = x->i32;
  } catch (const std::runtime_error&) {
  }
}

std::vector<const tile::TileEntity*> LiveWorld::tile_entities() const {
  std::vector<const tile::TileEntity*> out;
  out.reserve(tiles_.size());
  for (const auto& [_, t] : tiles_) out.push_back(t.get());
  return out;
}

void LiveWorld::set_tile(std::unique_ptr<tile::TileEntity> t) {
  tiles_[std::make_tuple(t->x, t->y, t->z)] = std::move(t);
}

std::vector<std::pair<int, int>> LiveWorld::provided_chunks() const {
  return region_.chunk_keys();
}

void LiveWorld::save(const std::string& save_dir, const entity::Player* player) const {
  std::filesystem::create_directories(save_dir + "/region");
  // One RegionFile per touched region; closed (flushed) at scope end.
  std::map<std::pair<int, int>, std::unique_ptr<RegionFile>> regions;
  for (const auto& [cx, cz] : region_.chunk_keys()) {
    const auto rk = std::make_pair(cx >> 5, cz >> 5);
    auto it = regions.find(rk);
    if (it == regions.end()) {
      auto rf = std::make_unique<RegionFile>(RegionFile::path_for(save_dir, cx, cz), true);
      if (!rf->is_open()) throw std::runtime_error("save: cannot open region");
      it = regions.emplace(rk, std::move(rf)).first;
    }
    nbt::Writer w;
    nbt::write_root(w, "", chunk_to_tag(*this, cx, cz));
    const std::vector<std::uint8_t>& raw = w.bytes();
    it->second->write_chunk(RegionFile::local(cx), RegionFile::local(cz), raw.data(), raw.size());
  }
  regions.clear();
  WorldInfoData info;
  info.seed = seed_;
  info.spawn_x = static_cast<int>(spawn_x_);
  info.spawn_y = static_cast<int>(spawn_y_);
  info.spawn_z = static_cast<int>(spawn_z_);
  info.time = time_;
  info.level_name = level_name_;
  if (player != nullptr) info.player = player_tag(*player);
  write_level_dat(save_dir, info);
}

bool LiveWorld::load(const std::string& save_dir) {
  const std::optional<WorldInfoData> info = read_level_dat(save_dir);
  if (!info.has_value() || info->seed != seed_) return false;
  spawn_x_ = info->spawn_x;
  spawn_y_ = info->spawn_y;
  spawn_z_ = info->spawn_z;
  time_ = info->time;
  level_name_ = info->level_name;
  // Walk region files present on disk.
  const std::string rdir = save_dir + "/region";
  std::error_code ec;
  if (!std::filesystem::is_directory(rdir, ec)) return true;
  for (const auto& entry : std::filesystem::directory_iterator(rdir, ec)) {
    const std::string name = entry.path().filename().string();
    int rx = 0, rz = 0;
    if (std::sscanf(name.c_str(), "r.%d.%d.mcr", &rx, &rz) != 2) continue;
    RegionFile rf(entry.path().string(), false);
    if (!rf.is_open()) continue;
    for (int lx = 0; lx < 32; ++lx) {
      for (int lz = 0; lz < 32; ++lz) {
        auto payload = rf.read_chunk(lx, lz);
        if (!payload.has_value()) continue;
        try {
          nbt::Reader r(payload->data(), payload->size());
          chunk_from_tag(nbt::read_root(r), *this, rx * 32 + lx, rz * 32 + lz);
        } catch (const std::exception&) {
        }
      }
    }
  }
  return true;
}

}  // namespace craftpp::world
