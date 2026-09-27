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
      // top shade 1.0, full daylight, plains grass tint (~0.486 red)
      CHECK(v.r == Catch::Approx(0.486F).margin(0.01));
    }
  }
  CHECK(found_grass_top);
}

TEST_CASE("mesh_live tints tall grass with the biome callback", "[mesher]") {
  craftpp::render::Mesher m;
  m.tint = [](int, int, bool foliage, float& r, float& g, float& b) {
    r = foliage ? 0.0F : 1.0F;
    g = foliage ? 1.0F : 0.0F;
    b = 0.0F;
  };
  auto w = flat_world();
  w.set_id(5, 65, 5, 31);  // tall grass
  const auto mesh = m.mesh_live(w, 0, 0);
  // Tall-grass top verts (y=66 over the tuft cell) carry the grass tint.
  bool tinted = false;
  for (const auto& v : mesh.vertices) {
    if (v.y == 66.0F && v.x >= 5.0F && v.x <= 6.0F && v.z >= 5.0F && v.z <= 6.0F && v.r > 0.9F &&
        v.g < 0.1F) {
      tinted = true;
      break;
    }
  }
  CHECK(tinted);
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

TEST_CASE("mesh_live renders faces behind leaves (fancy culling)", "[mesher]") {
  craftpp::render::Mesher m;
  craftpp::world::RegionWorld w;
  std::vector<std::int8_t> raw(16 * 128 * 16, 0);
  raw[(8 * 16 + 8) * 128 + 64] = 1;   // stone
  raw[(8 * 16 + 8) * 128 + 65] = 18;  // leaves on top
  w.ensure_chunk(0, 0, raw.data());
  const auto mesh = m.mesh_live(w, 0, 0);
  // Stone top face at y=65 must exist (visible through leaf holes).
  bool stone_top = false;
  for (std::size_t i = 0; i < mesh.indices.size(); i += 3) {
    const auto& a = mesh.vertices[mesh.indices[i]];
    const auto& b = mesh.vertices[mesh.indices[i + 1]];
    const auto& c = mesh.vertices[mesh.indices[i + 2]];
    if (a.y == 65.0F && b.y == 65.0F && c.y == 65.0F) {
      // Stone tile 1 (u in [16/256, 32/256)), not leaves tile 52.
      if (a.u >= 0.0626F && a.u < 0.125F) stone_top = true;
    }
  }
  CHECK(stone_top);
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

TEST_CASE("cross quads are double-sided with upright UVs", "[mesher]") {
  craftpp::render::Mesher m;
  auto w = flat_world();
  w.set_id(4, 65, 4, 37);
  const auto mesh = m.mesh_live(w, 0, 0);
  // Isolate flower verts (tile 13 u-range).
  std::vector<craftpp::render::Vertex> fv;
  for (const auto& v : mesh.vertices) {
    if (v.u > 0.81F && v.u < 0.88F) fv.push_back(v);
  }
  // 4 quads (2 diagonals x 2 windings) x 4 verts.
  CHECK(fv.size() == 16);
  // Top verts (y=66) carry v0 (texture top), bottom verts (y=65) v1.
  for (const auto& v : fv) {
    if (v.y == 66.0F) CHECK(v.v < 0.06F);
    if (v.y == 65.0F) CHECK(v.v > 0.06F);
  }
  // Both windings of the diagonals exist (face normals point both ways).
  bool pos = false, neg = false;
  for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
    const auto& a = mesh.vertices[mesh.indices[i]];
    if (a.u < 0.81F || a.u > 0.88F) continue;
    const auto& b = mesh.vertices[mesh.indices[i + 1]];
    const auto& c = mesh.vertices[mesh.indices[i + 2]];
    const float nx = (b.y - a.y) * (c.z - a.z) - (b.z - a.z) * (c.y - a.y);
    const float nz = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    const float s = nx + nz;
    if (s > 0) pos = true;
    if (s < 0) neg = true;
  }
  CHECK(pos);
  CHECK(neg);
}

}  // namespace
