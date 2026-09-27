#pragma once

#include <functional>

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
  // Per-column biome tint (ColorizerGrass/ColorizerFoliage): grass for
  // block 2 + tall grass, foliage for leaves. Defaults to the plains
  // sample; the app installs the colormap lookup.
  std::function<void(int x, int z, bool foliage, float& r, float& g, float& b)> tint =
      [](int, int, bool foliage, float& r, float& g, float& b) {
        if (foliage) {
          r = 0.282F;
          g = 0.478F;
          b = 0.141F;
        } else {
          r = 0.486F;
          g = 0.741F;
          b = 0.349F;
        }
      };

  Mesh mesh_chunk(const world::Chunk& chunk) const;

  // Live-world mesher for the client: textured cubes from a RegionWorld
  // chunk (world coords via RegionWorld, so borders cull against real
  // neighbours), per-face stored light, biome grass tint, and cross quads
  // for render-type 1/2/3 (plants/torch/fire). Complex geometry types
  // render as textured cubes for now (later milestones).
  //
  // Fluids are excluded: they go in mesh_fluid_live (the transparent
  // pass — like the engine's renderPass 1, drawn after all opaque).
  Mesh mesh_live(const world::RegionWorld& world, int cx, int cz) const;

  // Fluid-only pass (renderBlockFluids): lowered surfaces, flow UVs.
  // Drawn after every opaque chunk with blending on.
  Mesh mesh_fluid_live(const world::RegionWorld& world, int cx, int cz) const;

 private:
  Mesh mesh_live_impl(const world::RegionWorld& world, int cx, int cz, bool fluids_only) const;
};

}  // namespace craftpp::render
