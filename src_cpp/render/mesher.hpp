#pragma once

#include "render/mesh.hpp"
#include "world/chunk.hpp"
#include "world/region.hpp"

namespace craftpp::render {

// Greedy-free full-cube mesher for M2: one quad per visible face.
//
// Vertex order, UV mapping and per-face shading mirror
// RenderBlocks.render{Bottom,Top,East,West,North,South}Face for full cubes:
//   shade: bottom 0.5, top 1.0, z sides 0.8, x sides 0.6
//   uv: u0=(tile&15)*16/256, u1=(+16-0.01)/256 (same for v), -0.01 bleed inset
// A face is emitted only when the neighbour is non-opaque
// (mirrors shouldSideBeRendered for opaque cubes).
struct Mesher {
  // RGB tint multiplied on grass top/side faces (biome color from
  // ColorizerGrass; loader in app samples grasscolor.png, default plains).
  float tint_r = 1.0F;
  float tint_g = 1.0F;
  float tint_b = 1.0F;
  // Foliage tint for leaves (ColorizerFoliage; default sampled likewise).
  float foliage_r = 1.0F;
  float foliage_g = 1.0F;
  float foliage_b = 1.0F;

  Mesh mesh_chunk(const world::Chunk& chunk) const;

  // Live-world mesher for the client: textured cubes from a RegionWorld
  // chunk (world coords via RegionWorld, so borders cull against real
  // neighbours), per-face stored light, biome grass tint, and cross quads
  // for render-type 1/2/3 (plants/torch/fire). Complex geometry types
  // render as textured cubes for now (later milestones).
  Mesh mesh_live(const world::RegionWorld& world, int cx, int cz) const;
};

}  // namespace craftpp::render
