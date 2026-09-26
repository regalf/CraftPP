// mesh_live tests: textured cube emission, cross quads, border culling.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "render/mesher.hpp"
#include "world/region.hpp"

namespace {

craftpp::world::RegionWorld flat_world() {
  craftpp::world::RegionWorld w;
  std::vector<std::int8_t> raw(16 * 128 * 16, 0);
  for (int lx = 0; lx < 16; ++lx)
    for (int lz = 0; lz < 16; ++lz) {
      raw[(lx * 16 + lz) * 128 + 63] = 3;  // dirt
      raw[(lx * 16 + lz) * 128 + 64] = 2;  // grass
    }
  w.ensure_chunk(0, 0, raw.data());
  return w;
}

TEST_CASE("mesh_live emits textured top faces with grass tile", "[mesher]") {
  craftpp::render::Mesher m;
  auto w = flat_world();
  const auto mesh = m.mesh_live(w, 0, 0);
  REQUIRE(!mesh.vertices.empty());
  // Grass top tile 0 -> u in [0, 16/256), v in [0, 16/256).
  bool found_grass_top = false;
  for (const auto& v : mesh.vertices) {
    if (v.y == 65.0F && v.u < 0.0626F && v.v < 0.0626F) {
      found_grass_top = true;
      // top shade 1.0, full daylight brightness 1.0
      CHECK(v.r == Catch::Approx(1.0F * m.tint_r).margin(0.01));
    }
  }
  CHECK(found_grass_top);
}

TEST_CASE("mesh_live culls borders against provided neighbours", "[mesher]") {
  craftpp::render::Mesher m;
  auto w = flat_world();
  const auto solo = m.mesh_live(w, 0, 0);
  // Provide the +x neighbour with identical terrain: shared faces vanish.
  std::vector<std::int8_t> raw(16 * 128 * 16, 0);
  for (int lx = 0; lx < 16; ++lx)
    for (int lz = 0; lz < 16; ++lz) {
      raw[(lx * 16 + lz) * 128 + 63] = 3;
      raw[(lx * 16 + lz) * 128 + 64] = 2;
    }
  w.ensure_chunk(1, 0, raw.data());
  const auto joined = m.mesh_live(w, 0, 0);
  CHECK(joined.vertices.size() < solo.vertices.size());
}

TEST_CASE("mesh_live renders plants as cross quads", "[mesher]") {
  craftpp::render::Mesher m;
  auto w = flat_world();
  w.set_id(4, 65, 4, 37);  // yellow flower above grass
  const auto mesh = m.mesh_live(w, 0, 0);
  // Flower tile 13 -> u0 = 13*16/256 = 0.8125.
  bool found = false;
  for (const auto& v : mesh.vertices) {
    if (v.u > 0.81F && v.u < 0.88F) {
      found = true;
      break;
    }
  }
  CHECK(found);
}

}  // namespace
