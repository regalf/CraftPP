#pragma once

#include "render/mesh.hpp"
#include "world/chunk.hpp"

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

  Mesh mesh_chunk(const world::Chunk& chunk) const;
};

}  // namespace craftpp::render
