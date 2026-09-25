// Differential test for EntityLiving vs the GenLiving Java oracle
// (/tmp/genworld/GenLiving.java -> /tmp/living_vectors.txt).
//
// ONE Living reused across 12 scenarios in oracle order. Movement inputs are
// injected PlayerSP-style (override update_entity_action_state: super first,
// then scripted inputs). attackedAtYaw is never asserted (global Math.random
// in the source, nondeterministic even between JVM runs).
#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

#include "entity/living.hpp"

namespace {

using craftpp::entity::DamageSource;
using craftpp::entity::Entity;
using craftpp::entity::EntityWorld;
using craftpp::entity::Living;

struct TestWorld : EntityWorld {
  std::unordered_map<long, int> ids;
  std::unordered_map<long, int> metas;
  static long key(int x, int y, int z) {
    return (static_cast<long>(x + 1024) << 32) | (static_cast<long>(y + 1024) << 16) |
           static_cast<long>(z + 1024);
  }
  void put(int x, int y, int z, int id, int meta) {
    ids[key(x, y, z)] = id;
    metas[key(x, y, z)] = meta;
  }
  void floor_y(int x0, int x1, int z0, int z1, int top) {
    for (int x = x0; x <= x1; ++x)
      for (int z = z0; z <= z1; ++z)
        for (int y = 40; y <= top; ++y) put(x, y, z, 1, 0);
  }
  int block_id(int x, int y, int z) const override {
    auto it = ids.find(key(x, y, z));
    return it == ids.end() ? 0 : it->second;
  }
  int block_meta(int x, int y, int z) const override {
    auto it = metas.find(key(x, y, z));
    return it == metas.end() ? 0 : it->second;
  }
  bool chunks_exist(int, int, int, int, int, int) const override { return true; }
};

struct MobEnt : Living {
  float s_strafe = 0.0f, s_forward = 0.0f;
  bool s_jump = false;
  struct Hit {
    std::string scen;
    int tick;
    std::string src;
    int amt;
    bool ret;
  };
  std::vector<Hit> dmg_log;
  const char* cur_scen = "";
  int cur_tick = 0;

  explicit MobEnt(EntityWorld* w) : Living(w) { set_size(0.6f, 1.8f); }
  int max_health() const override { return 20; }
  void update_entity_action_state() override {
    Living::update_entity_action_state();
    move_strafing = s_strafe;
    move_forward = s_forward;
    is_jumping = s_jump;
  }
  bool attack(DamageSource src, int amt) override {
    const char* n = "?";
    if (src == DamageSource::kInFire) n = "inFire";
    if (src == DamageSource::kOnFire) n = "onFire";
    if (src == DamageSource::kLava) n = "lava";
    if (src == DamageSource::kCactus) n = "cactus";
    if (src == DamageSource::kFall) n = "fall";
    if (src == DamageSource::kDrown) n = "drown";
    if (src == DamageSource::kInWall) n = "inWall";
    bool r = Living::attack(src, amt);
    dmg_log.push_back({cur_scen, cur_tick, n, amt, r});
    return r;
  }
};

struct PlainEnt : Entity {
  explicit PlainEnt(EntityWorld* w) : Entity(w) {}
};

struct Row {
  double v[26];  // px..mz fall dist | strafe..pitch(12) | bbox(6)
  bool on_ground;
  int fire;
  bool in_water;
  int ticks;
  int live[9];  // health hurt death attack hearts rating age air jumping
  long long rseed;
};
struct Dmg {
  const char* scen;
  int tick;
  const char* src;
  int amt;
  bool ret;
};

}  // namespace

TEST_CASE("living physics match Java oracle", "[living]") {
  std::vector<std::vector<Row>> expect;
  std::vector<Dmg> dmg;
#include "living_expected.inc"

  TestWorld w;
  MobEnt e(&w);
  size_t scen_idx = 0;
  size_t dmg_idx = 0;

  auto check_at = [&](const char* scen, int row, int tick) {
    INFO("scenario " << scen << " tick " << tick);
    REQUIRE(scen_idx < expect.size());
    REQUIRE(row < static_cast<int>(expect[scen_idx].size()));
    const Row& r = expect[scen_idx][row];
    CHECK(e.pos_x == r.v[0]);
    CHECK(e.pos_y == r.v[1]);
    CHECK(e.pos_z == r.v[2]);
    CHECK(e.motion_x == r.v[3]);
    CHECK(e.motion_y == r.v[4]);
    CHECK(e.motion_z == r.v[5]);
    CHECK(e.on_ground == r.on_ground);
    CHECK(static_cast<double>(e.fall_distance) == r.v[6]);
    CHECK(static_cast<double>(e.distance_walked) == r.v[7]);
    CHECK(e.fire == r.fire);
    CHECK(e.in_water == r.in_water);
    CHECK(e.ticks_existed == r.ticks);
    CHECK(e.health == r.live[0]);
    CHECK(e.hurt_time == r.live[1]);
    CHECK(e.death_time == r.live[2]);
    CHECK(e.attack_time == r.live[3]);
    CHECK(e.hearts_life == r.live[4]);
    CHECK(e.natural_armor_rating == r.live[5]);
    CHECK(e.entity_age == r.live[6]);
    CHECK(e.air_supply == r.live[7]);
    CHECK((e.is_jumping ? 1 : 0) == r.live[8]);
    CHECK(static_cast<double>(e.move_strafing) == r.v[8]);
    CHECK(static_cast<double>(e.move_forward) == r.v[9]);
    CHECK(static_cast<double>(e.land_movement_factor) == r.v[10]);
    CHECK(static_cast<double>(e.render_yaw_offset) == r.v[11]);
    CHECK(static_cast<double>(e.limb_speed) == r.v[12]);
    CHECK(static_cast<double>(e.limb_phase) == r.v[13]);
    CHECK(static_cast<double>(e.anim_speed) == r.v[14]);
    CHECK(static_cast<double>(e.anim_t) == r.v[15]);
    CHECK(static_cast<double>(e.prev_anim_speed) == r.v[16]);
    CHECK(static_cast<double>(e.camera_pitch) == r.v[17]);
    CHECK(static_cast<double>(e.rotation_yaw) == r.v[18]);
    CHECK(static_cast<double>(e.rotation_pitch) == r.v[19]);
    CHECK(e.bbox.min_x == r.v[20]);
    CHECK(e.bbox.min_y == r.v[21]);
    CHECK(e.bbox.min_z == r.v[22]);
    CHECK(e.bbox.max_x == r.v[23]);
    CHECK(e.bbox.max_y == r.v[24]);
    CHECK(e.bbox.max_z == r.v[25]);
    CHECK(static_cast<long long>(e.rand.raw_state()) == r.rseed);
    while (dmg_idx < dmg.size() && dmg[dmg_idx].tick == tick &&
           std::string(dmg[dmg_idx].scen) == scen) {
      INFO("dmg idx " << dmg_idx);
      REQUIRE(e.dmg_log.size() > dmg_idx);
      CHECK(e.dmg_log[dmg_idx].scen == dmg[dmg_idx].scen);
      CHECK(e.dmg_log[dmg_idx].tick == dmg[dmg_idx].tick);
      CHECK(e.dmg_log[dmg_idx].src == dmg[dmg_idx].src);
      CHECK(e.dmg_log[dmg_idx].amt == dmg[dmg_idx].amt);
      CHECK(e.dmg_log[dmg_idx].ret == dmg[dmg_idx].ret);
      ++dmg_idx;
    }
  };

  auto teleport = [&](double x, double y, double z, float yaw) {
    e.set_position_and_rotation(x, y, z, yaw, 0.0f);
    e.motion_x = e.motion_y = e.motion_z = 0.0;
    e.fall_distance = 0.0f;
    e.on_ground = false;
  };
  auto begin = [&](const char* scen, int t) {
    e.cur_scen = scen;
    e.cur_tick = t;
  };

  // walk
  {
    int bx = 0;
    w.floor_y(bx - 8, bx + 8, 0, 24, 63);
    e.rand.set_seed(777);
    teleport(bx + 0.5, 64.0, 8.5, 0.0f);
    e.s_strafe = 0.0f;
    e.s_forward = 1.0f;
    e.s_jump = false;
    for (int t = 0; t < 40; ++t) {
      begin("walk", t);
      e.on_update();
      check_at("walk", t, t);
    }
    ++scen_idx;
  }
  // jump (plain 0..24, sprint 25..44)
  {
    int bx = 64;
    w.floor_y(bx - 8, bx + 8, 0, 24, 63);
    e.rand.set_seed(777);
    teleport(bx + 0.5, 64.0, 8.5, 0.0f);
    e.s_strafe = 0.0f;
    e.s_forward = 0.5f;
    e.s_jump = true;
    for (int t = 0; t < 25; ++t) {
      begin("jump", t);
      e.on_update();
      check_at("jump", t, t);
    }
    teleport(bx + 0.5, 64.0, 8.5, 90.0f);
    e.set_sprinting(true);
    e.s_forward = 1.0f;
    for (int t = 25; t < 45; ++t) {
      begin("jump", t);
      e.on_update();
      check_at("jump", t, t);
    }
    e.set_sprinting(false);
    ++scen_idx;
  }
  // swim
  {
    int bx = 128;
    w.floor_y(bx - 8, bx + 8, 0, 24, 62);
    for (int x = bx - 2; x <= bx + 6; ++x)
      for (int z = 6; z <= 14; ++z) {
        w.put(x, 63, z, 9, 0);
        w.put(x, 64, z, 9, 0);
      }
    e.rand.set_seed(777);
    teleport(bx + 0.5, 64.0, 10.5, 0.0f);
    e.s_strafe = 0.0f;
    e.s_forward = 1.0f;
    e.s_jump = false;
    for (int t = 0; t < 30; ++t) {
      begin("swim", t);
      e.on_update();
      check_at("swim", t, t);
    }
    ++scen_idx;
  }
  // lavaswim
  {
    int bx = 192;
    w.floor_y(bx - 8, bx + 8, 0, 24, 63);
    for (int x = bx - 2; x <= bx + 6; ++x)
      for (int z = 6; z <= 14; ++z) w.put(x, 64, z, 11, 0);
    e.rand.set_seed(777);
    teleport(bx + 0.5, 64.0, 10.5, 0.0f);
    e.s_strafe = 0.0f;
    e.s_forward = 1.0f;
    e.s_jump = false;
    for (int t = 0; t < 15; ++t) {
      begin("lavaswim", t);
      e.on_update();
      check_at("lavaswim", t, t);
    }
    ++scen_idx;
  }
  // ladder
  {
    int bx = 256;
    w.floor_y(bx - 8, bx + 16, 0, 24, 63);
    for (int y = 64; y <= 70; ++y) {
      w.put(bx + 6, y, 10, 1, 0);
      w.put(bx + 5, y, 10, 65, 4);
    }
    e.rand.set_seed(777);
    teleport(bx + 0.5, 64.0, 10.5, -90.0f);
    e.s_strafe = 0.0f;
    e.s_forward = 1.0f;
    e.s_jump = false;
    for (int t = 0; t < 20; ++t) {
      begin("ladder", t);
      e.on_update();
      check_at("ladder", t, t);
    }
    ++scen_idx;
  }
  // falldmg 0..24, falldeath 25..79
  {
    int bx = 320;
    w.floor_y(bx - 8, bx + 8, 0, 24, 63);
    e.rand.set_seed(777);
    teleport(bx + 0.5, 76.0, 10.5, 0.0f);
    e.s_strafe = 0.0f;
    e.s_forward = 0.0f;
    e.s_jump = false;
    for (int t = 0; t < 25; ++t) {
      begin("falldmg", t);
      e.on_update();
      check_at("falldmg", t, t);
    }
    ++scen_idx;
    teleport(bx + 0.5, 94.0, 10.5, 0.0f);
    for (int t = 25; t < 80; ++t) {
      begin("falldeath", t);
      e.on_update();
      check_at("falldeath", t - 25, t);
    }
    ++scen_idx;
  }
  // drown
  {
    int bx = 384;
    w.floor_y(bx - 8, bx + 8, 0, 24, 60);
    for (int x = bx - 2; x <= bx + 2; ++x)
      for (int z = 8; z <= 12; ++z)
        for (int y = 61; y <= 66; ++y) w.put(x, y, z, 9, 0);
    e.rand.set_seed(777);
    teleport(bx + 0.5, 62.0, 10.5, 0.0f);
    e.air_supply = 10;
    e.s_strafe = 0.0f;
    e.s_forward = 0.0f;
    e.s_jump = false;
    for (int t = 0; t < 35; ++t) {
      begin("drown", t);
      e.on_update();
      check_at("drown", t, t);
    }
    ++scen_idx;
  }
  // suffoc
  {
    int bx = 448;
    w.floor_y(bx - 8, bx + 8, 0, 24, 63);
    w.put(bx, 65, 10, 1, 0);
    w.put(bx, 66, 10, 1, 0);
    e.rand.set_seed(777);
    teleport(bx + 0.5, 64.0, 10.5, 0.0f);
    e.s_strafe = 0.0f;
    e.s_forward = 0.0f;
    e.s_jump = false;
    for (int t = 0; t < 10; ++t) {
      begin("suffoc", t);
      e.on_update();
      check_at("suffoc", t, t);
    }
    ++scen_idx;
  }
  // cactus
  {
    int bx = 512;
    w.floor_y(bx - 8, bx + 8, 0, 24, 63);
    w.put(bx + 2, 63, 10, 12, 0);
    w.put(bx + 2, 64, 10, 81, 0);
    e.rand.set_seed(777);
    teleport(bx + 0.5, 64.0, 10.5, -90.0f);
    e.s_strafe = 0.0f;
    e.s_forward = 1.0f;
    e.s_jump = false;
    for (int t = 0; t < 30; ++t) {
      begin("cactus", t);
      e.on_update();
      check_at("cactus", t, t);
    }
    ++scen_idx;
  }
  // knock
  {
    int bx = 576;
    w.floor_y(bx - 8, bx + 8, 0, 24, 63);
    e.rand.set_seed(777);
    teleport(bx + 0.5, 64.0, 10.5, 0.0f);
    PlainEnt atk(&w);
    atk.set_position_and_rotation(bx + 2.5, 64.0, 10.5, 0.0f, 0.0f);
    e.s_strafe = 0.0f;
    e.s_forward = 0.0f;
    e.s_jump = false;
    begin("knock", 0);
    e.knock_back(atk, 4, atk.pos_x - e.pos_x, atk.pos_z - e.pos_z);
    check_at("knock", 0, 0);
    for (int t = 1; t < 10; ++t) {
      begin("knock", t);
      e.on_update();
      check_at("knock", t, t);
    }
    ++scen_idx;
  }
  // ice
  {
    int bx = 640;
    w.floor_y(bx - 8, bx + 16, 0, 24, 62);
    for (int x = bx - 8; x <= bx + 16; ++x)
      for (int z = 0; z <= 24; ++z) w.put(x, 63, z, 79, 0);
    e.rand.set_seed(777);
    teleport(bx + 0.5, 64.0, 10.5, 0.0f);
    e.s_strafe = 0.0f;
    e.s_forward = 1.0f;
    e.s_jump = false;
    for (int t = 0; t < 25; ++t) {
      begin("ice", t);
      e.on_update();
      check_at("ice", t, t);
    }
    ++scen_idx;
  }

  CHECK(scen_idx == expect.size());
  CHECK(dmg_idx == dmg.size());
  CHECK(e.dmg_log.size() == dmg.size());
}
