// Shape blocks: slab/snow/ladder/vine collision, selection, and render.
// Collision boxes mirror BlockStep/BlockSnow/BlockLadder sources; selection
// mirrors getSelectedBoundingBoxFromPool; mesh_live geometry mirrors
// renderStandardBlock-with-bounds / renderBlockLadder / renderBlockVine.
#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "render/mesher.hpp"
#include "world/block_collision.hpp"
#include "world/blocks.hpp"
#include "world/region.hpp"

namespace {

using craftpp::Aabb;
using craftpp::world::BlockCollider;
using craftpp::world::BlockView;
namespace bid = craftpp::world::bid;

struct MapView : BlockView {
  int id = 0, meta = 0;
  int block_id(int, int, int) const override { return id; }
  int block_meta(int, int, int) const override { return meta; }
};

craftpp::world::RegionWorld empty_world() {
  craftpp::world::RegionWorld w;
  std::vector<std::int8_t> raw(16 * 128 * 16, 0);
  w.ensure_chunk(0, 0, raw.data());
  return w;
}

TEST_CASE("slab collision is the bottom half (1.0 has no top slabs)", "[shapes]") {
  BlockCollider c;
  MapView v;
  // Single slab: bottom half for every meta (BlockStep ctor bounds).
  for (int meta = 0; meta < 6; ++meta) {
    auto b = c.collision_box(bid::kStepSingle, meta, 8, 65, 8, v);
    REQUIRE(b.has_value());
    CHECK(b->min_y == 65.0);
    CHECK(b->max_y == 65.5);
  }
  // Double slab: full cube.
  auto d = c.collision_box(bid::kStepDouble, 0, 8, 65, 8, v);
  REQUIRE(d.has_value());
  CHECK(d->max_y == 66.0);
}

TEST_CASE("selection boxes: ladder/lily thin, vine full, fluids skipped", "[shapes]") {
  BlockCollider c;
  MapView v;
  std::vector<Aabb> out;
  // Ladder meta 5 (west wall): thin slice at x+0..2/16.
  c.selection_boxes(bid::kLadder, 5, 8, 65, 8, v, out);
  REQUIRE(out.size() == 1);
  CHECK(out[0].min_x == 8.0);
  CHECK(out[0].max_x == 8.0 + 2.0 / 16.0);
  CHECK(out[0].max_y == 66.0);
  // Ladder meta 2 (north wall): thin slice at z+1-2/16..z+1.
  out.clear();
  c.selection_boxes(bid::kLadder, 2, 8, 65, 8, v, out);
  REQUIRE(out.size() == 1);
  CHECK(out[0].min_z == 9.0 - 2.0 / 16.0);
  CHECK(out[0].max_z == 9.0);
  // Lilypad: full xz, 0.015625 high (vanilla ctor bounds).
  out.clear();
  c.selection_boxes(bid::kLilyPad, 0, 8, 65, 8, v, out);
  REQUIRE(out.size() == 1);
  CHECK(out[0].max_y == 65.0 + 0.015625);
  // Vine: vanilla full-cube selection (no override in BlockVine).
  out.clear();
  c.selection_boxes(bid::kVine, 15, 8, 65, 8, v, out);
  REQUIRE(out.size() == 1);
  CHECK(out[0].min_y == 65.0);
  CHECK(out[0].max_y == 66.0);
  // Fluids: not pickable (BlockFluid.canCollideCheck).
  out.clear();
  c.selection_boxes(bid::kWaterMoving, 0, 8, 65, 8, v, out);
  CHECK(out.empty());
  out.clear();
  c.selection_boxes(bid::kLavaStill, 0, 8, 65, 8, v, out);
  CHECK(out.empty());
  // Slab single: bottom half; snow: state height.
  out.clear();
  c.selection_boxes(bid::kStepSingle, 3, 8, 65, 8, v, out);
  REQUIRE(out.size() == 1);
  CHECK(out[0].max_y == 65.5);
  out.clear();
  c.selection_boxes(bid::kSnowCover, 5, 8, 65, 8, v, out);
  REQUIRE(out.size() == 1);
  CHECK(out[0].max_y == 65.0 + 12.0 / 16.0);
  // Stairs: both step boxes.
  out.clear();
  c.selection_boxes(bid::kStairsCobble, 0, 8, 65, 8, v, out);
  CHECK(out.size() == 2);
}

TEST_CASE("mesh_live renders slab as half box", "[shapes]") {
  craftpp::render::Mesher m;
  auto w = empty_world();
  w.set_id_meta(8, 65, 8, bid::kStepSingle, 0);
  const auto mesh = m.mesh_live(w, 0, 0);
  REQUIRE(!mesh.vertices.empty());
  for (const auto& v : mesh.vertices) {
    CHECK(v.y >= 65.0F);
    CHECK(v.y <= 65.5F);
  }
  // Top face present at slab height.
  bool top = false;
  for (const auto& v : mesh.vertices)
    if (v.y == 65.5F) top = true;
  CHECK(top);
}

TEST_CASE("mesh_live renders snow layers at state height", "[shapes]") {
  craftpp::render::Mesher m;
  auto w = empty_world();
  w.set_id_meta(8, 65, 8, bid::kSnowCover, 0);  // 2/16 high
  const auto mesh = m.mesh_live(w, 0, 0);
  REQUIRE(!mesh.vertices.empty());
  for (const auto& v : mesh.vertices) {
    CHECK(v.y >= 65.0F);
    CHECK(v.y <= 65.0F + 2.0F / 16.0F);
  }
}

TEST_CASE("mesh_live renders ladder as single wall quad", "[shapes]") {
  craftpp::render::Mesher m;
  auto w = empty_world();
  w.set_id_meta(7, 65, 8, bid::kLadder, 5);
  const auto mesh = m.mesh_live(w, 0, 0);
  // Exactly one quad (4 verts) on the x+0.05 plane.
  CHECK(mesh.vertices.size() == 4);
  for (const auto& v : mesh.vertices) CHECK(v.x == 7.05F);
}

TEST_CASE("mesh_live renders vine faces per meta bit", "[shapes]") {
  craftpp::render::Mesher m;
  auto w = empty_world();
  w.set_id_meta(8, 65, 8, bid::kVine, 2);  // west face only
  const auto mesh = m.mesh_live(w, 0, 0);
  // One double-wound quad = 8 verts on the x+0.05 plane.
  CHECK(mesh.vertices.size() == 8);
  for (const auto& v : mesh.vertices) CHECK(v.x == 8.05F);
}

}  // namespace
