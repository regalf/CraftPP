#pragma once

#include <cstdint>
#include <vector>

namespace craftpp::render {

// CPU-side mesh: the handoff between the mesher (ChunkGen thread in M3+) and
// the Tessellator (Render thread). Plain data, freely movable across threads.
struct Vertex {
  float x = 0.0F;
  float y = 0.0F;
  float z = 0.0F;
  float r = 1.0F;
  float g = 1.0F;
  float b = 1.0F;
  float u = 0.0F;
  float v = 0.0F;
};

struct Mesh {
  std::vector<Vertex> vertices;
  std::vector<std::uint32_t> indices;  // triples, CCW front like GL quads
};

}  // namespace craftpp::render
