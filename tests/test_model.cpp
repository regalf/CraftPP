// Entity model tests: part counts, UV layout vs ModelBox, bbox sanity.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "render/model.hpp"
#include "render/firstperson.hpp"

namespace {

craftpp::render::Mesh scaled(const craftpp::render::Mesh& m) {
  return m;  // model units; scale applied by the renderer (1/16)
}

TEST_CASE("pig model has 6 parts with 64x32 skin layout", "[model]") {
  const auto parts = craftpp::render::pig_parts(0.0F, 0.0F, 0.0F, 0.0F);
  CHECK(parts.size() == 6);
  const auto mesh = craftpp::render::build_model(parts, 64, 32, 1.0F);
  // 7 boxes (head + snout + body + 4 legs) x 6 quads x 4 verts.
  CHECK(mesh.vertices.size() == 7 * 6 * 4);
  CHECK(mesh.indices.size() == 7 * 6 * 6);
  // All UVs inside the skin.
  for (const auto& v : mesh.vertices) {
    CHECK(v.u >= 0.0F);
    CHECK(v.u <= 1.0F);
    CHECK(v.v >= 0.0F);
    CHECK(v.v <= 1.0F);
  }
}

TEST_CASE("zombie model has 6 parts", "[model]") {
  const auto parts = craftpp::render::zombie_parts(0.0F, 0.0F, 0.0F, 0, 0.0F, 0.0F);
  CHECK(parts.size() == 6);
  const auto mesh = craftpp::render::build_model(parts, 64, 32, 1.0F);
  CHECK(mesh.vertices.size() == 6 * 6 * 4);
  // Raised-arm pose: right arm pitched forward (~-90 deg).
  CHECK(parts[2].rx < -1.0F);
}

TEST_CASE("entity_mesh grounds at feet with entity heights", "[model]") {
  auto bbox = [](const craftpp::render::Mesh& m) {
    float mn = 1e9F, mx = -1e9F;
    for (const auto& v : m.vertices) {
      mn = std::min(mn, v.y);
      mx = std::max(mx, v.y);
    }
    return std::make_pair(mn, mx);
  };
  const auto pig = craftpp::render::entity_mesh(craftpp::render::pig_parts(0, 0, 0, 0), 64, 32, 0, 1);
  const auto zom =
      craftpp::render::entity_mesh(craftpp::render::zombie_parts(0, 0, 0, 0, 0, 0), 64, 32, 0, 1);
  const auto [pig_min, pig_max] = bbox(pig);
  const auto [zom_min, zom_max] = bbox(zom);
  INFO("pig y " << pig_min << " " << pig_max);
  INFO("zom y " << zom_min << " " << zom_max);
  // RenderLiving grounding: model y=24 -> feet+0.008.
  CHECK(pig_min == Catch::Approx(0.008F).margin(0.01));
  CHECK(zom_min == Catch::Approx(0.008F).margin(0.01));
  // Pig ~1.0 tall, zombie ~2.0 (vanilla model proportions).
  CHECK(pig_max == Catch::Approx(1.01F).margin(0.05));
  CHECK(zom_max == Catch::Approx(2.01F).margin(0.05));
}

}  // namespace

TEST_CASE("player model has 7 biped parts with char layout", "[model]") {
  const auto parts = craftpp::render::player_parts(0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0, false, 0);
  CHECK(parts.size() == 7);
  const auto mesh = craftpp::render::build_model(parts, 64, 32, 1.0F);
  CHECK(mesh.vertices.size() == 7 * 6 * 4);
  for (const auto& v : mesh.vertices) {
    CHECK(v.u >= 0.0F);
    CHECK(v.u <= 1.0F);
    CHECK(v.v >= 0.0F);
    CHECK(v.v <= 1.0F);
  }
  // Sneak bends the body and lowers the head pivots.
  const auto snuck = craftpp::render::player_parts(0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0, true, 0);
  CHECK(snuck[2].rx == Catch::Approx(0.5F));
  CHECK(snuck[0].py == Catch::Approx(1.0F));
  CHECK(parts[2].rx == Catch::Approx(0.0F));
  // Held item halves the arm swing and offsets it.
  const auto held = craftpp::render::player_parts(1.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0, false, 1);
  const auto bare = craftpp::render::player_parts(1.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0, false, 0);
  CHECK(held[3].rx != Catch::Approx(bare[3].rx));
  // Attack twist moves the right arm pivot.
  const auto atk = craftpp::render::player_parts(1.0F, 1.0F, 0.5F, 0.0F, 0.0F, 0, false, 1);
  CHECK(atk[3].px != Catch::Approx(bare[3].px));
  // Walk swing moves legs symmetrically.
  const auto w0 = craftpp::render::player_parts(0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0, false, 0);
  CHECK(w0[5].rx == Catch::Approx(-w0[6].rx));
}

TEST_CASE("entity mesh scale/roll default to legacy placement", "[model]") {
  const auto parts = craftpp::render::zombie_parts(0.0F, 0.0F, 0.0F, 0, 0.0F, 0.0F);
  const auto a = craftpp::render::entity_mesh(parts, 64, 32, 30.0F, 1.0F);
  const auto b = craftpp::render::entity_mesh(parts, 64, 32, 30.0F, 1.0F, 1.0F, 0.0F);
  REQUIRE(a.vertices.size() == b.vertices.size());
  for (std::size_t i = 0; i < a.vertices.size(); ++i) {
    CHECK(a.vertices[i].x == b.vertices[i].x);
    CHECK(a.vertices[i].y == b.vertices[i].y);
    CHECK(a.vertices[i].z == b.vertices[i].z);
  }
  // 15/16 player scale shrinks toward the feet origin.
  const auto c = craftpp::render::entity_mesh(parts, 64, 32, 30.0F, 1.0F, 15.0F / 16.0F, 0.0F);
  CHECK(c.vertices[0].y < a.vertices[0].y);
}

TEST_CASE("first-person hand and held item meshes", "[model]") {
  // Empty hand: skin arm only.
  {
    craftpp::render::FirstPersonMeshes fm;
    craftpp::render::build_first_person(fm, 0, 0, 1.0F, 0.0F);
    CHECK(!fm.skin.vertices.empty());
    CHECK(fm.atlas.vertices.empty());
    CHECK(fm.items.vertices.empty());
    for (const auto& v : fm.skin.vertices) {
      CHECK(v.u >= 0.0F);
      CHECK(v.u <= 1.0F);
    }
  }
  // Held dirt block: atlas cube, 24 verts.
  {
    craftpp::render::FirstPersonMeshes fm;
    craftpp::render::build_first_person(fm, 3, 0, 1.0F, 0.0F);
    CHECK(fm.atlas.vertices.size() == 24);
    CHECK(fm.skin.vertices.empty());
  }
  // Held stick: items sprite, 8 verts (double-sided).
  {
    craftpp::render::FirstPersonMeshes fm;
    craftpp::render::build_first_person(fm, 280, 0, 1.0F, 0.0F);
    CHECK(fm.items.vertices.size() == 8);
    CHECK(fm.skin.vertices.empty());
  }
  // Swing moves the item.
  {
    craftpp::render::FirstPersonMeshes a, b;
    craftpp::render::build_first_person(a, 3, 0, 1.0F, 0.0F);
    craftpp::render::build_first_person(b, 3, 0, 1.0F, 0.5F);
    REQUIRE(a.atlas.vertices.size() == b.atlas.vertices.size());
    bool moved = false;
    for (std::size_t i = 0; i < a.atlas.vertices.size(); ++i) {
      if (std::abs(a.atlas.vertices[i].x - b.atlas.vertices[i].x) > 1e-4F ||
          std::abs(a.atlas.vertices[i].y - b.atlas.vertices[i].y) > 1e-4F) {
        moved = true;
      }
    }
    CHECK(moved);
  }
}
