// Differential test for Entity physics vs the GenPhysics Java oracle
// (/tmp/genworld/GenPhysics.java -> /tmp/physics_vectors.txt).
//
// ONE Entity reused across 15 scenarios in oracle order (sticky block bounds
// + firstUpdate + fire all carry over exactly like the JVM statics). All
// doubles compared bit-for-bit (expected %.17g strings parsed with strtod,
// which round-trips exactly), plus the RNG stream state and damage log.
#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

#include "entity/entity.hpp"

namespace {

using craftpp::entity::DamageSource;
using craftpp::entity::Entity;
using craftpp::entity::EntityWorld;

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

  struct Hit {
    std::string scen;
    int tick;
    std::string src;
    int amt;
  };
  std::vector<Hit> dmg_log;
  const char* cur_scen = "";
  int cur_tick = 0;

  void attack_entity_from(DamageSource src, int amt) override {
    const char* n = "?";
    if (src == DamageSource::kInFire) n = "inFire";
    if (src == DamageSource::kOnFire) n = "onFire";
    if (src == DamageSource::kLava) n = "lava";
    if (src == DamageSource::kCactus) n = "cactus";
    dmg_log.push_back({cur_scen, cur_tick, n, amt});
  }
};

struct TestEnt : Entity {
  explicit TestEnt(EntityWorld* w) : Entity(w) {
    set_size(0.6f, 1.8f);
    step_height = 0.5f;
  }
};

struct Row {
  double v[8];  // px py pz mx my mz fall dist
  bool on_ground;
  int fire;
  bool in_water;
  int ticks;
  const char* bbox;  // 6 doubles
  long long rseed;
};
struct Dmg {
  const char* scen;
  int tick;
  const char* src;
  int amt;
};

double num(const char* s) {
  char* end = nullptr;
  return std::strtod(s, &end);
}

}  // namespace

TEST_CASE("entity physics match Java oracle", "[physics]") {
  std::vector<std::vector<Row>> expect;
  std::vector<Dmg> dmg;
#include "physics_expected.inc"

  TestWorld w;
  TestEnt e(&w);
  size_t scen_idx = 0;
  size_t dmg_idx = 0;

  auto check = [&](const char* scen, int tick) {
    w.cur_scen = scen;
    w.cur_tick = tick;
    INFO("scenario " << scen << " tick " << tick);
    REQUIRE(scen_idx < expect.size());
    REQUIRE(tick < static_cast<int>(expect[scen_idx].size()));
    const Row& r = expect[scen_idx][tick];
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
    double b[6];
    std::sscanf(r.bbox, "%lf %lf %lf %lf %lf %lf", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]);
    CHECK(e.bbox.min_x == b[0]);
    CHECK(e.bbox.min_y == b[1]);
    CHECK(e.bbox.min_z == b[2]);
    CHECK(e.bbox.max_x == b[3]);
    CHECK(e.bbox.max_y == b[4]);
    CHECK(e.bbox.max_z == b[5]);
    CHECK(static_cast<long long>(e.rand.raw_state()) == r.rseed);
    // Drain damage log entries for this tick.
    while (dmg_idx < dmg.size() && dmg[dmg_idx].tick == tick &&
           std::string(dmg[dmg_idx].scen) == scen) {
      INFO("dmg idx " << dmg_idx);
      REQUIRE(w.dmg_log.size() > dmg_idx);
      CHECK(w.dmg_log[dmg_idx].scen == dmg[dmg_idx].scen);
      CHECK(w.dmg_log[dmg_idx].tick == dmg[dmg_idx].tick);
      CHECK(w.dmg_log[dmg_idx].src == dmg[dmg_idx].src);
      CHECK(w.dmg_log[dmg_idx].amt == dmg[dmg_idx].amt);
      ++dmg_idx;
    }
  };

  auto teleport = [&](double x, double y, double z) {
    e.set_position_and_rotation(x, y, z, 0.0f, 0.0f);
    e.motion_x = e.motion_y = e.motion_z = 0.0;
    e.fall_distance = 0.0f;
    e.on_ground = false;
  };
  auto gravity = [&]() {
    e.motion_y -= 0.08;
    e.motion_y *= 0.9800000190734863;
  };

  // S1 fall
  {
    int bx = 0;
    w.floor_y(bx - 8, bx + 8, 0, 16, 63);
    e.rand.set_seed(12345);
    teleport(bx + 0.5, 80.0, 8.5);
    for (int t = 0; t < 30; ++t) {
      w.cur_scen = "fall"; w.cur_tick = t;
      gravity();
      e.move_entity(e.motion_x, e.motion_y, e.motion_z);
      check("fall", t);
    }
    ++scen_idx;
  }
  // S2 walk
  {
    int bx = 48;
    w.floor_y(bx - 8, bx + 16, 0, 16, 63);
    e.rand.set_seed(12345);
    teleport(bx + 0.5, 64.0, 8.5);
    for (int t = 0; t < 20; ++t) {
      w.cur_scen = "walk"; w.cur_tick = t;
      e.move_entity(0.2, -0.05, 0.0);
      check("walk", t);
    }
    ++scen_idx;
  }
  // S3 wall
  {
    int bx = 96;
    w.floor_y(bx - 8, bx + 16, 0, 16, 63);
    for (int y = 64; y <= 65; ++y) w.put(bx + 6, y, 8, 1, 0);
    e.rand.set_seed(12345);
    teleport(bx + 0.5, 64.0, 8.5);
    for (int t = 0; t < 6; ++t) {
      w.cur_scen = "wall"; w.cur_tick = t;
      e.move_entity(0.6, -0.05, 0.0);
      check("wall", t);
    }
    ++scen_idx;
  }
  // S4 step
  {
    int bx = 144;
    w.floor_y(bx - 8, bx + 16, 0, 16, 63);
    w.put(bx + 4, 64, 8, 1, 0);
    e.rand.set_seed(12345);
    teleport(bx + 0.5, 64.0, 8.5);
    for (int t = 0; t < 6; ++t) {
      w.cur_scen = "step"; w.cur_tick = t;
      e.move_entity(0.35, -0.05, 0.0);
      check("step", t);
    }
    ++scen_idx;
  }
  // S5 stairs
  {
    int bx = 192;
    w.floor_y(bx - 8, bx + 16, 0, 16, 63);
    w.put(bx + 4, 64, 8, 67, 1);
    w.put(bx + 5, 64, 8, 67, 1);
    w.put(bx + 6, 64, 8, 1, 0);
    w.put(bx + 6, 65, 8, 1, 0);
    e.rand.set_seed(12345);
    teleport(bx + 0.5, 64.0, 8.5);
    for (int t = 0; t < 10; ++t) {
      w.cur_scen = "stairs"; w.cur_tick = t;
      e.move_entity(0.3, -0.05, 0.0);
      check("stairs", t);
    }
    ++scen_idx;
  }
  // S6 sneak
  {
    int bx = 240;
    w.floor_y(bx - 8, bx + 8, 0, 16, 63);
    e.rand.set_seed(12345);
    teleport(bx + 6.5, 64.0, 8.5);
    e.set_sneaking(true);
    for (int t = 0; t < 12; ++t) {
      w.cur_scen = "sneak"; w.cur_tick = t;
      e.move_entity(0.3, -0.05, 0.0);
      check("sneak", t);
    }
    e.set_sneaking(false);
    ++scen_idx;
  }
  // S7 water
  {
    int bx = 288;
    w.floor_y(bx - 8, bx + 8, 0, 16, 62);
    for (int x = bx - 2; x <= bx + 2; ++x)
      for (int z = 6; z <= 10; ++z) {
        w.put(x, 63, z, 9, 0);
        w.put(x, 64, z, 9, 0);
      }
    e.rand.set_seed(12345);
    teleport(bx + 0.5, 72.0, 8.5);
    int t = 0;
    for (; t < 25; ++t) {
      w.cur_scen = "water"; w.cur_tick = t;
      gravity();
      e.move_entity(e.motion_x, e.motion_y, e.motion_z);
      e.on_update();
      check("water", t);
    }
    for (; t < 30; ++t) {
      w.cur_scen = "water"; w.cur_tick = t;
      e.move_entity(0.0, 0.25, 0.0);
      e.on_update();
      check("water", t);
    }
    ++scen_idx;
  }
  // S8 lava
  {
    int bx = 336;
    w.floor_y(bx - 8, bx + 16, 0, 16, 63);
    for (int x = bx + 3; x <= bx + 6; ++x) w.put(x, 64, 8, 11, 0);
    e.rand.set_seed(12345);
    teleport(bx + 0.5, 64.0, 8.5);
    for (int t = 0; t < 10; ++t) {
      w.cur_scen = "lava"; w.cur_tick = t;
      e.move_entity(0.3, -0.05, 0.0);
      e.on_update();
      check("lava", t);
    }
    ++scen_idx;
  }
  // S9 panes
  {
    int bx = 384;
    w.floor_y(bx - 8, bx + 16, 0, 16, 63);
    for (int y = 64; y <= 65; ++y) {
      w.put(bx + 5, y, 7, 102, 0);
      w.put(bx + 5, y, 8, 102, 0);
      w.put(bx + 5, y, 9, 102, 0);
    }
    e.rand.set_seed(12345);
    teleport(bx + 0.5, 64.0, 8.5);
    for (int t = 0; t < 4; ++t) {
      w.cur_scen = "pane"; w.cur_tick = t;
      e.move_entity(0.4, -0.05, 0.0);
      check("pane", t);
    }
    ++scen_idx;
  }
  // S10 soul sand
  {
    int bx = 432;
    w.floor_y(bx - 8, bx + 16, 0, 16, 63);
    for (int x = bx + 2; x <= bx + 7; ++x) w.put(x, 64, 8, 88, 0);
    e.rand.set_seed(12345);
    teleport(bx + 0.5, 64.0, 8.5);
    e.motion_x = 0.3;
    for (int t = 0; t < 8; ++t) {
      w.cur_scen = "soul"; w.cur_tick = t;
      e.move_entity(e.motion_x, -0.05, 0.0);
      check("soul", t);
    }
    ++scen_idx;
  }
  // S11 web
  {
    int bx = 480;
    w.floor_y(bx - 8, bx + 16, 0, 16, 63);
    w.put(bx + 4, 64, 8, 30, 0);
    e.rand.set_seed(12345);
    teleport(bx + 0.5, 64.0, 8.5);
    for (int t = 0; t < 6; ++t) {
      w.cur_scen = "web"; w.cur_tick = t;
      e.move_entity(0.35, -0.05, 0.0);
      check("web", t);
    }
    ++scen_idx;
  }
  // S12 cactus
  {
    int bx = 528;
    w.floor_y(bx - 8, bx + 16, 0, 16, 63);
    w.put(bx + 4, 63, 8, 12, 0);
    w.put(bx + 4, 64, 8, 81, 0);
    e.rand.set_seed(12345);
    teleport(bx + 0.5, 64.0, 8.5);
    for (int t = 0; t < 4; ++t) {
      w.cur_scen = "cactus"; w.cur_tick = t;
      e.move_entity(0.35, -0.05, 0.0);
      e.on_update();
      check("cactus", t);
    }
    ++scen_idx;
  }
  // S13 jump
  {
    int bx = 576;
    w.floor_y(bx - 8, bx + 2, 0, 16, 63);
    w.floor_y(bx + 6, bx + 16, 0, 16, 63);
    e.rand.set_seed(12345);
    teleport(bx + 0.5, 64.0, 8.5);
    e.motion_y = 0.42;
    for (int t = 0; t < 20; ++t) {
      w.cur_scen = "jump"; w.cur_tick = t;
      gravity();
      e.move_entity(0.25, e.motion_y, 0.0);
      check("jump", t);
    }
    ++scen_idx;
  }
  // S14 ladder
  {
    int bx = 624;
    w.floor_y(bx - 8, bx + 16, 0, 16, 63);
    for (int y = 64; y <= 66; ++y) {
      w.put(bx + 5, y, 8, 1, 0);
      w.put(bx + 4, y, 8, 65, 5);
    }
    e.rand.set_seed(12345);
    teleport(bx + 0.5, 64.0, 8.5);
    for (int t = 0; t < 3; ++t) {
      w.cur_scen = "ladder"; w.cur_tick = t;
      e.move_entity(0.4, -0.05, 0.0);
      check("ladder", t);
    }
    ++scen_idx;
  }
  // S15 fence
  {
    int bx = 672;
    w.floor_y(bx - 8, bx + 16, 0, 16, 63);
    w.put(bx + 4, 64, 8, 85, 0);
    e.rand.set_seed(12345);
    teleport(bx + 4.5, 68.0, 8.5);
    for (int t = 0; t < 12; ++t) {
      w.cur_scen = "fence"; w.cur_tick = t;
      gravity();
      e.move_entity(0.15, e.motion_y, 0.0);
      check("fence", t);
    }
    ++scen_idx;
  }

  CHECK(scen_idx == expect.size());
  CHECK(dmg_idx == dmg.size());
  CHECK(w.dmg_log.size() == dmg.size());
}
