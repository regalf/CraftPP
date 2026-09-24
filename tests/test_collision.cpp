// Differential test for block collision shapes vs the GenCollision Java
// oracle (/tmp/genworld/GenCollision.java -> /tmp/collision_vectors.txt).
//
// 113 CASEs (single BOX + multi HIT, queried in oracle order on ONE shared
// BlockCollider so the sticky per-type bounds leftovers match) + 5 FLOW
// vectors. Expected values are GENERATED (see expected_table.inc origin);
// doubles are compared bit-for-bit (expected strings parsed with strtod,
// which round-trips the %.17g oracle output exactly).
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

#include "world/block_collision.hpp"

namespace {

using craftpp::Aabb;
using craftpp::Vec3;
using craftpp::world::BlockCollider;
using craftpp::world::BlockView;

struct MapView : BlockView {
  std::unordered_map<long, int> ids;
  std::unordered_map<long, int> metas;
  static long key(int x, int y, int z) {
    return (static_cast<long>(x + 64) << 32) | (static_cast<long>(y + 64) << 16) |
           static_cast<long>(z + 64);
  }
  void clear() {
    ids.clear();
    metas.clear();
  }
  void put(int x, int y, int z, int id, int meta) {
    ids[key(x, y, z)] = id;
    metas[key(x, y, z)] = meta;
  }
  int block_id(int x, int y, int z) const override {
    auto it = ids.find(key(x, y, z));
    return it == ids.end() ? 0 : it->second;
  }
  int block_meta(int x, int y, int z) const override {
    auto it = metas.find(key(x, y, z));
    return it == metas.end() ? 0 : it->second;
  }
};

std::array<double, 6> parse6(const std::string& s) {
  std::array<double, 6> v{};
  std::sscanf(s.c_str(), "%lf %lf %lf %lf %lf %lf", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]);
  return v;
}

std::array<double, 6> of_box(const Aabb& b) {
  return {b.min_x, b.min_y, b.min_z, b.max_x, b.max_y, b.max_z};
}

struct Expect {
  const char* name;
  const char* box;  // "NULL" or 6 doubles
  std::vector<std::string> hits;
};

}  // namespace

TEST_CASE("collision shapes match Java oracle", "[collision]") {
  static const std::vector<Expect> kExpect = {
#include "collision_expected.inc"
  };

  MapView w;
  BlockCollider col;
  const Aabb ebox(7.9, 63.9, 7.9, 9.1, 66.1, 9.1);
  size_t idx = 0;
  auto check = [&](const std::string& name, int id, int meta) {
    REQUIRE(idx < kExpect.size());
    const Expect& e = kExpect[idx++];
    INFO("case " << name << " (expected entry " << e.name << ")");
    REQUIRE(name == e.name);
    auto got = col.collision_box(id, meta, 8, 64, 8, w);
    if (std::string(e.box) == "NULL") {
      CHECK(!got.has_value());
    } else {
      REQUIRE(got.has_value());
      CHECK(of_box(*got) == parse6(e.box));
    }
    std::vector<Aabb> hits;
    col.colliding_boxes(id, meta, 8, 64, 8, ebox, w, hits);
    REQUIRE(hits.size() == e.hits.size());
    for (size_t i = 0; i < hits.size(); ++i) CHECK(of_box(hits[i]) == parse6(e.hits[i]));
  };

  w.clear();
  w.put(8, 64, 8, 1, 0);
  check("stone", 1, 0);
  w.clear();
  w.put(8, 64, 8, 60, 0);
  check("farmland", 60, 0);
  w.clear();
  w.put(8, 64, 8, 81, 0);
  check("cactus", 81, 0);
  for (int m = 0; m <= 6; ++m) {
    w.clear();
    w.put(8, 64, 8, 92, m);
    check("cake" + std::to_string(m), 92, m);
  }
  w.clear();
  w.put(8, 64, 8, 88, 0);
  check("soulsand", 88, 0);
  w.put(8, 64, 8, 111, 0);
  check("lilypad", 111, 0);
  w.put(8, 64, 8, 26, 0);
  check("bed", 26, 0);
  for (int m = 0; m <= 7; ++m) {
    w.clear();
    w.put(8, 64, 8, 64, m);
    check("door" + std::to_string(m), 64, m);
  }
  w.clear();
  w.put(8, 64, 8, 71, 2);
  check("doorsteel", 71, 2);
  for (int m = 0; m <= 7; ++m) {
    w.clear();
    w.put(8, 64, 8, 96, m);
    check("trap" + std::to_string(m), 96, m);
  }
  // Fence series accumulates like the harness (clear once, then pile on).
  w.clear();
  w.put(8, 64, 8, 85, 0);
  check("fence_iso", 85, 0);
  w.put(8, 64, 7, 85, 0);
  check("fence_N", 85, 0);
  w.put(9, 64, 8, 1, 0);
  check("fence_Estone", 85, 0);
  w.put(7, 64, 8, 86, 0);
  check("fence_Wpump", 85, 0);
  w.put(8, 64, 9, 20, 0);
  check("fence_Sglass", 85, 0);
  w.clear();
  w.put(8, 64, 8, 85, 0);
  w.put(8, 64, 7, 107, 0);
  check("fence_Ngate", 85, 0);
  for (int m = 0; m <= 5; ++m) {
    w.clear();
    w.put(8, 64, 8, 107, m);
    check("gate" + std::to_string(m), 107, m);
  }
  for (int m = 0; m <= 5; ++m) {
    w.clear();
    w.put(8, 64, 8, 65, m);
    check("ladder" + std::to_string(m), 65, m);
  }
  for (int m = 0; m <= 7; ++m) {
    w.clear();
    w.put(8, 64, 8, 78, m);
    check("snow" + std::to_string(m), 78, m);
  }
  const int stairs[5] = {53, 67, 108, 109, 114};
  for (int s = 0; s < 5; ++s)
    for (int m = 0; m <= 4; ++m) {
      w.clear();
      w.put(8, 64, 8, stairs[s], m);
      check("st" + std::to_string(stairs[s]) + "_" + std::to_string(m), stairs[s], m);
    }
  // Pane series (mirrors harness accumulation).
  w.clear();
  w.put(8, 64, 8, 102, 0);
  check("pane_iso", 102, 0);
  w.put(9, 64, 8, 20, 0);
  check("pane_Eglass", 102, 0);
  w.clear();
  w.put(8, 64, 8, 102, 0);
  w.put(9, 64, 8, 102, 0);
  check("pane_Esame", 102, 0);
  w.clear();
  w.put(8, 64, 8, 102, 0);
  w.put(9, 64, 8, 101, 0);
  check("pane_Eiron", 102, 0);
  w.put(7, 64, 8, 1, 0);
  check("pane_Wstone", 102, 0);
  w.clear();
  w.put(8, 64, 8, 102, 0);
  w.put(8, 64, 7, 20, 0);
  w.put(8, 64, 9, 20, 0);
  check("pane_NSglass", 102, 0);
  w.clear();
  w.put(8, 64, 8, 101, 0);
  w.put(9, 64, 8, 102, 0);
  check("iron_Eglasspane", 101, 0);
  w.clear();
  w.put(8, 64, 8, 33, 3);
  check("pistonbase", 33, 3);
  w.put(8, 64, 8, 29, 3);
  check("pistonsticky", 29, 3);
  for (int m = 0; m <= 5; ++m) {
    w.clear();
    w.put(8, 64, 8, 34, m);
    check("pistext" + std::to_string(m), 34, m);
  }
  w.clear();
  w.put(8, 64, 8, 36, 3);
  check("pistonmoving", 36, 3);
  w.clear();
  w.put(8, 64, 8, 118, 0);
  check("cauldron", 118, 0);
  w.put(8, 64, 8, 117, 0);
  check("brewing", 117, 0);
  w.put(8, 64, 8, 119, 0);
  check("endportal", 119, 0);
  for (int m = 0; m <= 7; ++m) {
    w.clear();
    w.put(8, 64, 8, 120, m);
    check("endframe" + std::to_string(m), 120, m);
  }
  w.clear();
  w.put(8, 64, 8, 90, 0);
  check("portal", 90, 0);
  w.put(8, 64, 8, 50, 5);
  check("torch", 50, 5);
  w.put(8, 64, 8, 8, 0);
  check("water", 8, 0);
  w.put(8, 64, 8, 63, 2);
  check("signpost", 63, 2);
  w.put(8, 64, 8, 30, 0);
  check("web", 30, 0);
  CHECK(idx == kExpect.size());
}

TEST_CASE("fluid flow vectors match Java oracle", "[collision]") {
  MapView w;
  auto vec = [&](int id, int x, int y, int z) {
    return craftpp::world::fluid_flow_vector(id, x, y, z, w);
  };
  auto close = [](const Vec3& v, double x, double y, double z) {
    CHECK(v.x == x);
    CHECK(v.y == y);
    CHECK(v.z == z);
  };
  w.clear();
  w.put(8, 64, 8, 9, 0);
  close(vec(9, 8, 64, 8), 0.0, 0.0, 0.0);
  w.clear();
  w.put(8, 64, 8, 8, 1);
  w.put(7, 64, 8, 8, 0);
  w.put(9, 64, 8, 8, 3);
  w.put(8, 64, 7, 1, 0);
  w.put(8, 64, 9, 8, 2);
  close(vec(8, 8, 64, 8), 0.948683286545974, 0.0, 0.31622776218199133);
  w.clear();
  w.put(8, 64, 8, 8, 8);
  w.put(7, 64, 8, 1, 0);
  w.put(9, 64, 8, 1, 0);
  w.put(8, 64, 7, 1, 0);
  w.put(8, 64, 9, 1, 0);
  close(vec(8, 8, 64, 8), 0.0, -1.0, 0.0);
  w.clear();
  w.put(8, 64, 8, 10, 8);
  w.put(8, 63, 8, 10, 0);
  close(vec(10, 8, 64, 8), 0.0, 0.0, 0.0);
  w.clear();
  w.put(8, 64, 8, 8, 8);
  w.put(8, 63, 8, 8, 0);
  close(vec(8, 8, 64, 8), 0.0, 0.0, 0.0);
}
