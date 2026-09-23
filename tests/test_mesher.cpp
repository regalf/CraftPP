#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "render/mesher.hpp"
#include "world/block.hpp"
#include "world/chunk.hpp"

using craftpp::render::Mesh;
using craftpp::render::Mesher;
using craftpp::world::BlockId;
using craftpp::world::Chunk;

namespace {

struct TriNormal {
  float x;
  float y;
  float z;
};

// Outward normal of the first triangle of quad `q` (quads are 4 verts).
TriNormal quad_normal(const Mesh& m, std::size_t q) {
  const auto& a = m.vertices[q * 4];
  const auto& b = m.vertices[q * 4 + 1];
  const auto& c = m.vertices[q * 4 + 2];
  const float e1x = b.x - a.x;
  const float e1y = b.y - a.y;
  const float e1z = b.z - a.z;
  const float e2x = c.x - a.x;
  const float e2y = c.y - a.y;
  const float e2z = c.z - a.z;
  return {e1y * e2z - e1z * e2y, e1z * e2x - e1x * e2z, e1x * e2y - e1y * e2x};
}

}  // namespace

TEST_CASE("single block emits six shaded quads", "[mesher]") {
  Chunk chunk;
  chunk.set(0, 0, 0, BlockId::Stone);
  const Mesh m = Mesher().mesh_chunk(chunk);

  REQUIRE(m.vertices.size() == 24);
  REQUIRE(m.indices.size() == 36);
  // Indices triangulate each quad as (0,1,2)(0,2,3).
  CHECK(m.indices[0] == 0);
  CHECK(m.indices[1] == 1);
  CHECK(m.indices[2] == 2);
  CHECK(m.indices[3] == 0);
  CHECK(m.indices[4] == 2);
  CHECK(m.indices[5] == 3);

  // Face order: bottom, top, zmin, zmax, xmin, xmax.
  const float shades[] = {0.5F, 1.0F, 0.8F, 0.8F, 0.6F, 0.6F};
  for (int q = 0; q < 6; ++q) {
    for (int i = 0; i < 4; ++i) {
      const auto& v = m.vertices[static_cast<std::size_t>(q) * 4 + i];
      CHECK(v.r == shades[q]);
      CHECK(v.g == shades[q]);
      CHECK(v.b == shades[q]);
    }
  }

  // Winding: outward normals (CCW front).
  CHECK(quad_normal(m, 0).y == -1.0F);  // bottom
  CHECK(quad_normal(m, 1).y == 1.0F);   // top
  CHECK(quad_normal(m, 2).z == -1.0F);  // zmin
  CHECK(quad_normal(m, 3).z == 1.0F);   // zmax
  CHECK(quad_normal(m, 4).x == -1.0F);  // xmin
  CHECK(quad_normal(m, 5).x == 1.0F);   // xmax

  // Stone tile 1: u0 = 16/256, u1 = (32-0.01)/256, row 0 for v.
  const float u0 = 16.0F / 256.0F;
  const float u1 = (32.0F - 0.01F) / 256.0F;
  const float v1 = (16.0F - 0.01F) / 256.0F;
  const auto& t0 = m.vertices[4];  // top quad first vertex
  CHECK_THAT(t0.u, Catch::Matchers::WithinAbs(u1, 1e-6));
  CHECK_THAT(t0.v, Catch::Matchers::WithinAbs(v1, 1e-6));
  CHECK(t0.x == 1.0F);
  CHECK(t0.y == 1.0F);
  CHECK(t0.z == 1.0F);
}

TEST_CASE("shared faces are culled", "[mesher]") {
  Chunk chunk;
  chunk.set(0, 0, 0, BlockId::Stone);
  chunk.set(1, 0, 0, BlockId::Stone);
  const Mesh m = Mesher().mesh_chunk(chunk);
  // 6 + 6 - 2 shared = 10 quads.
  CHECK(m.vertices.size() == 40);
  CHECK(m.indices.size() == 60);
}

TEST_CASE("flat chunk exposes only its shell", "[mesher]") {
  Chunk chunk;
  craftpp::world::fill_flat(chunk);
  const Mesh m = Mesher().mesh_chunk(chunk);
  // 256 grass tops + 256 bedrock bottoms + 5 layers x 64 side quads.
  CHECK(m.vertices.size() == 832 * 4);
  CHECK(m.indices.size() == 832 * 6);
}

TEST_CASE("grass tint applies to top and sides only", "[mesher]") {
  Chunk chunk;
  chunk.set(0, 1, 0, BlockId::Grass);  // y=1: bottom culled by nothing? air below -> visible
  Mesher mesher;
  mesher.tint_r = 0.5F;
  mesher.tint_g = 0.8F;
  mesher.tint_b = 0.25F;
  const Mesh m = mesher.mesh_chunk(chunk);
  REQUIRE(m.vertices.size() == 24);
  // Quad 0 = bottom: untinted dirt tile shade 0.5.
  CHECK(m.vertices[0].r == 0.5F);
  // Quad 1 = top: shade 1.0 * tint.
  CHECK(m.vertices[4].r == 0.5F);
  CHECK(m.vertices[4].g == 0.8F);
  CHECK(m.vertices[4].b == 0.25F);
  // Quad 2 = side: shade 0.8 * tint.
  CHECK_THAT(m.vertices[8].r, Catch::Matchers::WithinAbs(0.4F, 1e-6));
  // Grass top uses tile 0, bottom uses dirt tile 2.
  CHECK_THAT(m.vertices[4].u, Catch::Matchers::WithinAbs((16.0F - 0.01F) / 256.0F, 1e-6));
}
