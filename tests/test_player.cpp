// Differential test for Player/PlayerSP/controllers vs the GenPlayer Java
// oracle (/tmp/genworld/GenPlayer.java -> /tmp/player_vectors.txt).
//
// Locomotion/strength/harvest/melee/armor call REAL methods on both sides.
// Controller glue (SP damage accumulation, creative countdown) is
// hand-replicated in the harness over real Block/World calls; the C++ side
// runs the real controllers. PlayerSP.onLivingUpdate has no Java oracle
// (needs the Minecraft client); it gets C++ logic tests, flagged in
// docs/known-issues.md.
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

#include "entity/controller.hpp"
#include "entity/player_sp.hpp"
#include "core/random.hpp"

namespace {

using craftpp::entity::ControllerCreative;
using craftpp::entity::ControllerSP;
using craftpp::entity::MovementInput;
using craftpp::JavaRandom;using craftpp::entity::DamageSource;
using craftpp::entity::Entity;
using craftpp::entity::EntityWorld;
using craftpp::entity::ItemStack;
using craftpp::entity::Living;
using craftpp::entity::Player;
using craftpp::entity::PlayerSP;
using craftpp::world::BlockCollider;
using craftpp::world::edit::EditWorld;

struct TestWorld : EditWorld {
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
  void set_raw(int x, int y, int z, int id, int meta) override { put(x, y, z, id, meta); }
  bool chunks_exist(int, int, int, int, int, int) const override { return true; }
  bool is_normal_cube(int x, int y, int z) const override {
    const int id = block_id(x, y, z);
    return craftpp::world::bid::material_opaque(id) &&
           craftpp::world::bid::renders_as_normal(id);
  }
  bool solid_side(int x, int y, int z, bool missing_default) const override {
    (void)missing_default;  // test world is fully present
    return is_normal_cube(x, y, z);
  }
  bool material_solid_at(int x, int y, int z) const override {
    return craftpp::world::bid::material_is_solid(block_id(x, y, z));
  }
  JavaRandom& world_rand() override { return wrand; }
  BlockCollider shared_collider;
  BlockCollider& collider() override { return shared_collider; }
  JavaRandom wrand;

  struct Drop {
    int id, count, dmg;
    double px, py, pz;
  };
  std::vector<Drop> drops;
  void on_item_drop(int item_id, int count, int damage, double px, double py, double pz, double,
                    double, double) override {
    drops.push_back({item_id, count, damage, px, py, pz});
  }
  struct Aux {
    int id, x, y, z, data;
  };
  std::vector<Aux> auxs;
  void play_aux_sfx(int id, int x, int y, int z, int data) override {
    auxs.push_back({id, x, y, z, data});
  }
};

struct TestPlayer : Player {
  float s_strafe = 0.0f, s_forward = 0.0f;
  bool s_jump = false;
  explicit TestPlayer(EntityWorld* w) : Player(w) {}
  void update_entity_action_state() override {
    Player::update_entity_action_state();
    move_strafing = s_strafe;
    move_forward = s_forward;
    is_jumping = s_jump;
  }
};

struct TestMob : Living {
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
  explicit TestMob(EntityWorld* w) : Living(w) { set_size(0.6f, 1.8f); }
  bool attack(DamageSource src, int amt) override {
    log(src, amt);
    bool r = Living::attack(src, amt);
    dmg_log.back().ret = r;
    return r;
  }
  bool attack_ex(DamageSource src, int amt, Entity* attacker) override {
    log(src, amt);
    bool r = Living::attack_ex(src, amt, attacker);
    dmg_log.back().ret = r;
    return r;
  }
  void log(DamageSource src, int amt) {
    const char* n = "?";
    if (src == DamageSource::kPlayer) n = "player";
    if (src == DamageSource::kInFire) n = "inFire";
    if (src == DamageSource::kFall) n = "fall";
    dmg_log.push_back({cur_scen, cur_tick, n, amt, false});
  }
  int hp() const { return health; }
};

struct Row {
  double v[28];  // pos6 fall dist | living13 | exhaust | bbox6
  bool on_ground;
  int fire;
  bool in_water;
  int ticks;
  int live[9];
  int food;
  long long rseed;
};
struct Dmg {
  const char* scen;
  int tick;
  const char* src;
  int amt;
  bool ret;
};
struct StrLine {
  int held;
  const char* vals;  // 11 space-separated %g floats
};

}  // namespace

TEST_CASE("player physics match Java oracle", "[player]") {
  std::vector<std::vector<Row>> expect;
  std::vector<Dmg> dmg;
  std::vector<StrLine> strlines;
  std::vector<std::string> events;
#include "player_expected.inc"

  TestWorld w;
  TestPlayer p(&w);
  TestMob mob(&w);
  size_t scen_idx = 0;
  size_t dmg_idx = 0;
  size_t ev_idx = 0;
  size_t str_idx = 0;
  auto mob_log = [&]() -> const std::vector<TestMob::Hit>& { return mob.dmg_log; };

  auto check = [&](const char* scen, int row, int tick) {
    INFO("scenario " << scen << " tick " << tick);
    REQUIRE(scen_idx < expect.size());
    REQUIRE(row < static_cast<int>(expect[scen_idx].size()));
    const Row& r = expect[scen_idx][row];
    CHECK(p.pos_x == r.v[0]);
    CHECK(p.pos_y == r.v[1]);
    CHECK(p.pos_z == r.v[2]);
    CHECK(p.motion_x == r.v[3]);
    CHECK(p.motion_y == r.v[4]);
    CHECK(p.motion_z == r.v[5]);
    CHECK(p.on_ground == r.on_ground);
    CHECK(static_cast<double>(p.fall_distance) == r.v[6]);
    CHECK(static_cast<double>(p.distance_walked) == r.v[7]);
    CHECK(p.fire == r.fire);
    CHECK(p.in_water == r.in_water);
    CHECK(p.ticks_existed == r.ticks);
    CHECK(p.health == r.live[0]);
    CHECK(p.hurt_time == r.live[1]);
    CHECK(p.death_time == r.live[2]);
    CHECK(p.attack_time == r.live[3]);
    CHECK(p.hearts_life == r.live[4]);
    CHECK(p.natural_armor_rating == r.live[5]);
    CHECK(p.entity_age == r.live[6]);
    CHECK(p.air_supply == r.live[7]);
    CHECK((p.is_jumping ? 1 : 0) == r.live[8]);
    CHECK(static_cast<double>(p.move_strafing) == r.v[8]);
    CHECK(static_cast<double>(p.move_forward) == r.v[9]);
    CHECK(static_cast<double>(p.land_movement_factor) == r.v[10]);
    CHECK(static_cast<double>(p.render_yaw_offset) == r.v[11]);
    CHECK(static_cast<double>(p.limb_speed) == r.v[12]);
    CHECK(static_cast<double>(p.limb_phase) == r.v[13]);
    CHECK(static_cast<double>(p.anim_speed) == r.v[14]);
    CHECK(static_cast<double>(p.anim_t) == r.v[15]);
    CHECK(static_cast<double>(p.prev_anim_speed) == r.v[16]);
    // camera_yaw/pitch smooth the render camera through Math.atan, whose
    // HotSpot intrinsic differs 1 ulp from libm: tight tolerance here only.
    CHECK(std::fabs(static_cast<double>(p.camera_pitch) - r.v[17]) < 1e-6);
    CHECK(std::fabs(static_cast<double>(p.camera_yaw) - r.v[18]) < 1e-6);
    CHECK(static_cast<double>(p.rotation_yaw) == r.v[19]);
    CHECK(static_cast<double>(p.rotation_pitch) == r.v[20]);
    CHECK(p.food_level() == r.food);
    CHECK(static_cast<double>(p.food.exhaustion) == r.v[21]);
    CHECK(p.bbox.min_x == r.v[22]);
    CHECK(p.bbox.min_y == r.v[23]);
    CHECK(p.bbox.min_z == r.v[24]);
    CHECK(p.bbox.max_x == r.v[25]);
    CHECK(p.bbox.max_y == r.v[26]);
    CHECK(p.bbox.max_z == r.v[27]);
    CHECK(static_cast<long long>(p.rand.raw_state()) == r.rseed);
    while (dmg_idx < dmg.size() && dmg[dmg_idx].tick == tick &&
           std::string(dmg[dmg_idx].scen) == scen) {
      INFO("dmg idx " << dmg_idx);
      REQUIRE(mob_log().size() > dmg_idx);
      CHECK(mob_log()[dmg_idx].scen == dmg[dmg_idx].scen);
      CHECK(mob_log()[dmg_idx].tick == dmg[dmg_idx].tick);
      CHECK(mob_log()[dmg_idx].src == dmg[dmg_idx].src);
      CHECK(mob_log()[dmg_idx].amt == dmg[dmg_idx].amt);
      CHECK(mob_log()[dmg_idx].ret == dmg[dmg_idx].ret);
      ++dmg_idx;
    }
  };

  auto teleport = [&](double x, double y, double z, float yaw) {
    p.set_position_and_rotation(x, y, z, yaw, 0.0f);
    p.motion_x = p.motion_y = p.motion_z = 0.0;
    p.fall_distance = 0.0f;
    p.on_ground = false;
  };

  // pwalk (yOffset 1.62 eye path) + pjump (global ticks continue 30..54)
  {
    int bx = 0;
    w.floor_y(bx - 8, bx + 8, 0, 24, 63);
    p.rand.set_seed(4242);
    teleport(bx + 0.5, 65.62, 8.5, 0.0f);
    p.s_strafe = 0.0f;
    p.s_forward = 1.0f;
    p.s_jump = false;
    for (int t = 0; t < 30; ++t) {
      p.on_update();
      check("pwalk", t, t);
    }
    ++scen_idx;
    p.s_forward = 0.5f;
    p.s_jump = true;
    for (int t = 30; t < 55; ++t) {
      p.on_update();
      check("pjump", t - 30, t);
    }
    ++scen_idx;
  }
  // psprint
  {
    int bx = 64;
    w.floor_y(bx - 8, bx + 16, 0, 24, 63);
    p.rand.set_seed(4242);
    teleport(bx + 0.5, 65.62, 8.5, 0.0f);
    p.set_sprinting(true);
    p.s_strafe = 0.0f;
    p.s_forward = 1.0f;
    p.s_jump = false;
    for (int t = 0; t < 20; ++t) {
      p.on_update();
      check("psprint", t, t);
    }
    p.set_sprinting(false);
    ++scen_idx;
  }
  CHECK(scen_idx == expect.size());

  // STR table (player standing, on ground, dry: no /5 penalties).
  {
    p.on_ground = true;
    REQUIRE(str_idx == 0);
    const int helds[6] = {-1, 270, 257, 275, 258, 268};
    for (int h = 0; h < 6; ++h) {
      if (helds[h] < 0) {
        p.inventory.main[0] = std::nullopt;
      } else {
        p.inventory.main[0] = ItemStack(helds[h], 1, 0);
      }
      INFO("held " << helds[h]);
      REQUIRE(str_idx < strlines.size());
      CHECK(strlines[str_idx].held == helds[h]);
      // Harness prints %.9g (exact float round-trip); compare as floats.
      float vals[11];
      std::sscanf(strlines[str_idx].vals, "%f %f %f %f %f %f %f %f %f %f %f", &vals[0],
                  &vals[1], &vals[2], &vals[3], &vals[4], &vals[5], &vals[6], &vals[7], &vals[8],
                  &vals[9], &vals[10]);
      const int blocks[11] = {3, 1, 17, 18, 49, 7, 88, 12, 20, 46, 87};
      for (int b = 0; b < 11; ++b) CHECK(p.strength_vs_block(blocks[b]) == vals[b]);
      ++str_idx;
    }
    CHECK(str_idx == strlines.size());
  }

  auto expect_event = [&](const std::string& want) {
    INFO("event want: " << want);
    REQUIRE(ev_idx < events.size());
    CHECK(events[ev_idx] == want);
    ++ev_idx;
  };

  // Mine (real ControllerSP; harness hand-replication must agree).
  {
    int bx = 128;
    w.floor_y(bx - 8, bx + 8, 0, 24, 63);
    w.put(bx + 2, 64, 10, 3, 0);
    w.put(bx + 3, 64, 10, 1, 0);
    w.put(bx + 4, 64, 10, 50, 5);
    w.put(bx + 5, 64, 10, 7, 0);
    p.rand.set_seed(4242);
    w.wrand.set_seed(99);
    ControllerSP ctl(w, p);
    auto removing = [&](int x, int y, int z) { ctl.send_block_removing(x, y, z, 1); };
    p.inventory.main[0] = std::nullopt;
    int t = 0;
    for (; t < 20; ++t) {
      removing(bx + 2, 64, 10);
      if (w.block_id(bx + 2, 64, 10) == 0) break;
    }
    expect_event("BROKE dirt at " + std::to_string(t));
    p.inventory.main[0] = ItemStack(270, 1, 0);
    for (; t < 200; ++t) {
      removing(bx + 3, 64, 10);
      if (w.block_id(bx + 3, 64, 10) == 0) break;
    }
    expect_event("BROKE stone at " + std::to_string(t));
    {
      const auto* held = p.current_equipped();
      std::string got = "PICK null";
      if (held && held->has_value())
        got = "PICK " + std::to_string(held->value().item_id) + " " +
              std::to_string(held->value().stack_size) + " " +
              std::to_string(held->value().damage);
      expect_event(got);
    }
    ctl.click_block(bx + 4, 64, 10, 1);
    expect_event(std::string("TORCHGONE ") + (w.block_id(bx + 4, 64, 10) == 0 ? "true" : "false"));
    for (int i = 0; i < 30; ++i) ctl.send_block_removing(bx + 5, 64, 10, 1);
    expect_event(std::string("BEDROCK ") + std::to_string(w.block_id(bx + 5, 64, 10)));
  }

  // Place (real ControllerSP).
  {
    int bx = 192;
    w.floor_y(bx - 8, bx + 16, 0, 24, 63);
    for (int y = 64; y <= 67; ++y) w.put(bx + 6, y, 10, 1, 0);
    p.rand.set_seed(4242);
    w.wrand.set_seed(77);
    p.rotation_yaw = 90.0f;
    ControllerSP ctl(w, p);
    ItemStack stone(1, 64, 0);
    bool ok = ctl.send_place_block(stone, bx + 0, 63, 10, 1);
    expect_event("PUTSTONE " + std::string(ok ? "true" : "false") + " " +
                 std::to_string(stone.stack_size) + " id=" + std::to_string(w.block_id(bx, 64, 10)));
    ItemStack torch(50, 64, 0);
    ok = ctl.send_place_block(torch, bx + 6, 65, 10, 4);
    expect_event("PUTTORCH " + std::string(ok ? "true" : "false") + " " +
                 std::to_string(torch.stack_size) + " id=" + std::to_string(w.block_id(bx + 5, 65, 10)) +
                 " meta=" + std::to_string(w.block_meta(bx + 5, 65, 10)));
    ItemStack ladder(65, 64, 0);
    ok = ctl.send_place_block(ladder, bx + 6, 66, 10, 4);
    expect_event("PUTLADDER " + std::string(ok ? "true" : "false") + " " +
                 std::to_string(ladder.stack_size) + " id=" +
                 std::to_string(w.block_id(bx + 5, 66, 10)) + " meta=" +
                 std::to_string(w.block_meta(bx + 5, 66, 10)));
    ItemStack stairs(67, 64, 0);
    ok = ctl.send_place_block(stairs, bx + 1, 63, 10, 1);
    expect_event("PUTSTAIR " + std::string(ok ? "true" : "false") + " " +
                 std::to_string(stairs.stack_size) + " id=" +
                 std::to_string(w.block_id(bx + 1, 64, 10)) + " meta=" +
                 std::to_string(w.block_meta(bx + 1, 64, 10)));
    ItemStack door(324, 1, 0);
    p.rotation_yaw = 180.0f;
    ok = ctl.send_place_block(door, bx + 2, 63, 10, 1);
    expect_event("PUTDOOR " + std::string(ok ? "true" : "false") + " " +
                 std::to_string(door.stack_size) + " lo=" + std::to_string(w.block_id(bx + 2, 64, 10)) +
                 "/" + std::to_string(w.block_meta(bx + 2, 64, 10)) + " hi=" +
                 std::to_string(w.block_id(bx + 2, 65, 10)) + "/" +
                 std::to_string(w.block_meta(bx + 2, 65, 10)));
    ctl.click_block(bx + 2, 64, 10, 1);
    expect_event("DOORTOGGLE lo=" + std::to_string(w.block_meta(bx + 2, 64, 10)) + " hi=" +
                 std::to_string(w.block_meta(bx + 2, 65, 10)));
  }

  // Melee (real attack_target vs MobEnt oracle).
  {
    int bx = 256;
    w.floor_y(bx - 8, bx + 8, 0, 24, 63);
    p.rand.set_seed(4242);
    teleport(bx + 0.5, 65.62, 10.5, -90.0f);
    mob.set_position_and_rotation(bx + 3.5, 64.0, 10.5, 0.0f, 0.0f);
    mob.rand.set_seed(4242);
    mob.cur_scen = "melee";
    p.inventory.main[0] = ItemStack(267, 1, 0);
    p.s_strafe = 0.0f;
    p.s_forward = 0.0f;
    p.s_jump = false;
    mob.cur_tick = 0;
    p.attack_target(mob);
    {
      // Parsed numerically (C vs Java %g formatting differs textually).
      REQUIRE(ev_idx < events.size());
      int ehp = 0, sid = 0, scnt = 0, sdlg = 0;
      double emx = 0, emy = 0, emz = 0;
      INFO("event: " << events[ev_idx]);
      REQUIRE(std::sscanf(events[ev_idx].c_str(), "MELEE mobhp=%d mobm=%lf,%lf,%lf sword=%d,%d,%d",
                          &ehp, &emx, &emy, &emz, &sid, &scnt, &sdlg) == 7);
      CHECK(mob.hp() == ehp);
      CHECK(mob.motion_x == emx);
      CHECK(mob.motion_y == emy);
      CHECK(mob.motion_z == emz);
      REQUIRE(p.inventory.main[0].has_value());
      CHECK(p.inventory.main[0]->item_id == sid);
      CHECK(p.inventory.main[0]->stack_size == scnt);
      CHECK(p.inventory.main[0]->damage == sdlg);
      ++ev_idx;
    }
    p.set_sprinting(true);
    mob.cur_tick = 1;
    p.attack_target(mob);
    {
      REQUIRE(ev_idx < events.size());
      int ehp = 0;
      double emx = 0, emy = 0, emz = 0;
      INFO("event: " << events[ev_idx]);
      REQUIRE(std::sscanf(events[ev_idx].c_str(), "MELEE2 mobhp=%d mobm=%lf,%lf,%lf", &ehp, &emx,
                          &emy, &emz) == 4);
      CHECK(mob.hp() == ehp);
      CHECK(mob.motion_x == emx);
      CHECK(mob.motion_y == emy);
      CHECK(mob.motion_z == emz);
      ++ev_idx;
    }
    p.set_sprinting(false);
    for (int k = 0; k < 2; ++k) {
      INFO("dmg idx " << dmg_idx);
      REQUIRE(dmg_idx < dmg.size());
      REQUIRE(mob.dmg_log.size() > dmg_idx);
      CHECK(mob.dmg_log[dmg_idx].scen == dmg[dmg_idx].scen);
      CHECK(mob.dmg_log[dmg_idx].tick == dmg[dmg_idx].tick);
      CHECK(mob.dmg_log[dmg_idx].src == dmg[dmg_idx].src);
      CHECK(mob.dmg_log[dmg_idx].amt == dmg[dmg_idx].amt);
      CHECK(mob.dmg_log[dmg_idx].ret == dmg[dmg_idx].ret);
      ++dmg_idx;
    }
  }

  // Armor (iron suit vs cactus + fall).
  {
    int bx = 320;
    w.floor_y(bx - 8, bx + 8, 0, 24, 63);
    p.rand.set_seed(4242);
    teleport(bx + 0.5, 65.62, 10.5, 0.0f);
    p.inventory.armor[0] = ItemStack(306, 1, 0);
    p.inventory.armor[1] = ItemStack(307, 1, 0);
    p.inventory.armor[2] = ItemStack(308, 1, 0);
    p.inventory.armor[3] = ItemStack(309, 1, 0);
    p.s_strafe = 0.0f;
    p.s_forward = 0.0f;
    p.s_jump = false;
    expect_event("ARMORVAL " + std::to_string(p.inventory.armor_value()));
    p.attack(DamageSource::kCactus, 6);
    expect_event("ARMORHIT hp=" + std::to_string(p.health));
    p.attack(DamageSource::kFall, 6);
    expect_event("FALLHIT hp=" + std::to_string(p.health));
  }

  // SPEnt mc-free methods on the real PlayerSP.
  {
    PlayerSP sp(&w, "Test", 0);
    sp.rand.set_seed(4242);
    sp.set_position_and_rotation(384.5, 65.62, 10.5, 0.0f, 0.0f);
    sp.motion_x = sp.motion_y = sp.motion_z = 0.0;
    sp.fall_distance = 0.0f;
    sp.on_ground = false;
    sp.movement_input->move_forward = 1.0f;
    sp.update_entity_action_state();
    {
      REQUIRE(ev_idx < events.size());
      double ef = 0, es = 0, ay = 0, ap = 0;
      char ej[16] = {0};
      INFO("event: " << events[ev_idx]);
      REQUIRE(std::sscanf(events[ev_idx].c_str(), "SP fwd=%lf strafe=%lf jump=%15s arm=%lf,%lf",
                          &ef, &es, ej, &ay, &ap) == 5);
      CHECK(static_cast<double>(sp.move_forward) == ef);
      CHECK(static_cast<double>(sp.move_strafing) == es);
      CHECK(std::string(sp.is_jumping ? "true" : "false") == ej);
      CHECK(static_cast<double>(sp.render_arm_yaw) == ay);
      CHECK(static_cast<double>(sp.render_arm_pitch) == ap);
      ++ev_idx;
    }
    sp.movement_input->sneak = true;
    expect_event(std::string("SNEAK ") + (sp.is_sneaking() ? "true" : "false"));
    sp.set_sprinting(true);
    expect_event("SPRINT " + std::string(sp.is_sprinting() ? "true" : "false") + " " +
                 std::to_string(sp.sprinting_ticks_left));
    w.floor_y(376, 392, 0, 24, 63);
    for (int y = 64; y <= 65; ++y) {
      w.put(385, y, 10, 1, 0);
      w.put(385, y, 11, 1, 0);
    }
    sp.set_position_and_rotation(385.5, 64.0, 10.5, 0.0f, 0.0f);
    sp.motion_x = sp.motion_y = sp.motion_z = 0.0;
    const double px = sp.pos_x - sp.width * 0.35;
    const double pz0 = sp.pos_z + sp.width * 0.35;
    const double pz1 = sp.pos_z - sp.width * 0.35;
    const double qx = sp.pos_x + sp.width * 0.35;
    const double my = sp.bbox.min_y + 0.5;
    sp.push_out_of_blocks(px, my, pz0);
    sp.push_out_of_blocks(px, my, pz1);
    sp.push_out_of_blocks(qx, my, pz1);
    sp.push_out_of_blocks(qx, my, pz0);
    {
      REQUIRE(ev_idx < events.size());
      double emx = 0, emz = 0;
      INFO("event: " << events[ev_idx]);
      REQUIRE(std::sscanf(events[ev_idx].c_str(), "PUSHOUT m=%lf,%lf", &emx, &emz) == 2);
      CHECK(sp.motion_x == emx);
      CHECK(sp.motion_z == emz);
      ++ev_idx;
    }
    sp.set_entity_health(20);
    sp.set_health(15);
    expect_event("SETHEALTH hp=" + std::to_string(sp.health) + " hearts=" +
                 std::to_string(sp.hearts_life));
    sp.set_health(25);
    expect_event("SETHEALTH2 hp=" + std::to_string(sp.health));
  }

  CHECK(ev_idx == events.size());
  CHECK(dmg_idx == dmg.size());
}

TEST_CASE("creative controller logic (no oracle: needs the client)", "[player][creative]") {
  TestWorld w;
  TestPlayer p(&w);
  w.floor_y(0, 16, 0, 16, 63);
  w.put(4, 64, 8, 1, 0);
  w.put(5, 64, 8, 49, 0);
  ControllerCreative ctl(w, p);
  ControllerCreative::enable_creative(p);
  CHECK(p.capabilities.allow_flying);
  // Click insta-removes without drops.
  ctl.click_block(4, 64, 8, 1);
  CHECK(w.block_id(4, 64, 8) == 0);
  CHECK(w.drops.empty());
  // Removing-ticks break every 5th call.
  for (int i = 0; i < 4; ++i) ctl.send_block_removing(5, 64, 8, 1);
  CHECK(w.block_id(5, 64, 8) == 49);
  ctl.send_block_removing(5, 64, 8, 1);
  CHECK(w.block_id(5, 64, 8) == 0);
  // Place does not consume.
  ItemStack stone(1, 3, 0);
  CHECK(ctl.send_place_block(stone, 4, 63, 8, 1));
  CHECK(w.block_id(4, 64, 8) == 1);
  CHECK(stone.stack_size == 3);
  CHECK(stone.damage == 0);
  ControllerCreative::disable_creative(p);
  CHECK(!p.capabilities.allow_flying);
  CHECK(!p.capabilities.is_flying);
}

TEST_CASE("playerSP onLivingUpdate logic (no oracle: needs the client)", "[player][sp]") {
  // Key-state input model (mirrors MovementInputFromOptions: the per-tick
  // refresh is what makes the double-tap detectors see stale-then-new).
  struct KeyInput : MovementInput {
    bool key_fwd = false;
    bool key_jump = false;
    bool key_sneak = false;
    void update_player_move_state() override {
      move_forward = key_fwd ? 1.0f : 0.0f;
      jump = key_jump;
      sneak = key_sneak;
    }
  };
  TestWorld w;
  w.floor_y(0, 32, 0, 32, 63);
  PlayerSP sp(&w, "Test", 0);
  auto keys = std::make_unique<KeyInput>();
  KeyInput* k = keys.get();
  sp.movement_input = std::move(keys);
  sp.rand.set_seed(4242);
  sp.set_position_and_rotation(8.5, 65.63, 8.5, 0.0f, 0.0f);  // feet 64.01 (clear of dust)  // feet 64 (posY is eye-level)
  sp.motion_x = sp.motion_y = sp.motion_z = 0.0;
  sp.fall_distance = 0.0f;
  sp.on_ground = true;
  // Double-tap forward starts sprinting (stale stopped, refreshed moving).
  k->key_fwd = true;
  sp.on_living_update();
  CHECK(sp.sprint_toggle_timer == 7);
  CHECK(!sp.is_sprinting());
  k->key_fwd = false;
  sp.on_living_update();
  k->key_fwd = true;
  sp.on_living_update();
  CHECK(sp.is_sprinting());
  CHECK(sp.sprinting_ticks_left == 600);
  // Releasing forward stops the sprint.
  k->key_fwd = false;
  sp.on_living_update();
  CHECK(!sp.is_sprinting());
  // Sneak grows ySize (then moveEntity decays it x0.4 on flat ground).
  sp.y_size = 0.0f;
  k->key_sneak = true;
  sp.on_living_update();
  CHECK(sp.y_size == 0.2f * 0.4f);
  k->key_sneak = false;
  // Fly toggle with double-tap jump (creative flight allowed).
  sp.capabilities.allow_flying = true;
  k->key_jump = true;
  sp.on_living_update();
  CHECK(sp.fly_toggle_timer == 7);
  CHECK(!sp.capabilities.is_flying);
  k->key_jump = false;
  sp.on_living_update();
  k->key_jump = true;
  sp.on_living_update();
  CHECK(sp.capabilities.is_flying);
  // Rising while airborne holding jump (ground jump would override).
  sp.on_ground = false;
  sp.motion_y = 0.0;
  sp.on_living_update();
  CHECK(sp.motion_y > 0.05);
  // Landing dismounts flight: sneak-descend onto the floor while flying
  // (flight zeroes gravity, so falling alone never lands).
  sp.set_position_and_rotation(8.5, 70.0, 8.5, 0.0f, 0.0f);
  sp.motion_x = sp.motion_y = sp.motion_z = 0.0;
  sp.on_ground = false;
  k->key_jump = false;
  k->key_sneak = true;
  CHECK(sp.capabilities.is_flying);
  for (int i = 0; i < 60 && !sp.on_ground; ++i) sp.on_living_update();
  k->key_sneak = false;
  CHECK(sp.on_ground);
  CHECK(!sp.capabilities.is_flying);
  // Sprinting timer expiry (keep moving so the stop-condition stays clear).
  k->key_jump = false;
  k->key_fwd = true;
  sp.on_living_update();  // refresh keys so move_forward is live
  sp.set_sprinting(true);
  sp.sprinting_ticks_left = 2;
  sp.on_living_update();
  CHECK(sp.is_sprinting());
  sp.on_living_update();
  CHECK(!sp.is_sprinting());
}
