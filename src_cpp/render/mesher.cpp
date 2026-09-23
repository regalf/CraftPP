#include "render/mesher.hpp"

#include "world/block.hpp"

namespace craftpp::render {

namespace {

using world::BlockId;
using world::Face;

struct QuadCorner {
  float dx;
  float dy;
  float dz;
  float u;  // 0 -> tile u0, 1 -> tile u1 (with -0.01 inset)
  float v;  // 0 -> tile v0, 1 -> tile v1
};

// Quad corners in RenderBlocks emission order; triangulated (0,1,2)(0,2,3)
// preserves the original GL_QUADS winding (CCW front).
constexpr QuadCorner kBottom[4] = {{0, 0, 1, 0, 1}, {0, 0, 0, 0, 0}, {1, 0, 0, 1, 0}, {1, 0, 1, 1, 1}};
constexpr QuadCorner kTop[4] = {{1, 1, 1, 1, 1}, {1, 1, 0, 1, 0}, {0, 1, 0, 0, 0}, {0, 1, 1, 0, 1}};
constexpr QuadCorner kZMin[4] = {{0, 1, 0, 1, 0}, {1, 1, 0, 0, 0}, {1, 0, 0, 0, 1}, {0, 0, 0, 1, 1}};
constexpr QuadCorner kZMax[4] = {{0, 1, 1, 0, 0}, {0, 0, 1, 0, 1}, {1, 0, 1, 1, 1}, {1, 1, 1, 1, 0}};
constexpr QuadCorner kXMin[4] = {{0, 1, 1, 1, 0}, {0, 1, 0, 0, 0}, {0, 0, 0, 0, 1}, {0, 0, 1, 1, 1}};
constexpr QuadCorner kXMax[4] = {{1, 0, 1, 0, 1}, {1, 0, 0, 1, 1}, {1, 1, 0, 1, 0}, {1, 1, 1, 0, 0}};

struct FaceDesc {
  Face face;
  const QuadCorner* corners;
  float shade;
  int nx;
  int ny;
  int nz;
};

constexpr FaceDesc kFaces[6] = {
    {Face::Bottom, kBottom, 0.5F, 0, -1, 0},
    {Face::Top, kTop, 1.0F, 0, 1, 0},
    {Face::ZMin, kZMin, 0.8F, 0, 0, -1},
    {Face::ZMax, kZMax, 0.8F, 0, 0, 1},
    {Face::XMin, kXMin, 0.6F, -1, 0, 0},
    {Face::XMax, kXMax, 0.6F, 1, 0, 0},
};

void tile_uv(int tile, float& u0, float& u1, float& v0, float& v1) {
  const float tx = static_cast<float>((tile & 15) * 16);
  const float ty = static_cast<float>(tile & 240);
  u0 = tx / 256.0F;
  u1 = (tx + 16.0F - 0.01F) / 256.0F;
  v0 = ty / 256.0F;
  v1 = (ty + 16.0F - 0.01F) / 256.0F;
}

}  // namespace

Mesh Mesher::mesh_chunk(const world::Chunk& chunk) const {
  Mesh mesh;
  mesh.vertices.reserve(4096);
  mesh.indices.reserve(6144);

  for (int y = 0; y < world::Chunk::kHeight; ++y) {
    for (int z = 0; z < world::Chunk::kSize; ++z) {
      for (int x = 0; x < world::Chunk::kSize; ++x) {
        const BlockId id = chunk.get(x, y, z);
        if (id == BlockId::Air) continue;
        const world::BlockDef& def = world::block_def(id);
        if (!def.opaque) continue;  // M2 has opaque cubes only

        for (const FaceDesc& f : kFaces) {
          if (world::block_def(chunk.get(x + f.nx, y + f.ny, z + f.nz)).opaque) continue;

          const int tile = world::tile_for(id, f.face);
          float u0 = 0.0F;
          float u1 = 0.0F;
          float v0 = 0.0F;
          float v1 = 0.0F;
          tile_uv(tile, u0, u1, v0, v1);

          float r = f.shade;
          float g = f.shade;
          float b = f.shade;
          if (def.grass_tinted && f.face != Face::Bottom) {
            r *= tint_r;
            g *= tint_g;
            b *= tint_b;
          }

          const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
          for (int i = 0; i < 4; ++i) {
            const QuadCorner& c = f.corners[i];
            mesh.vertices.push_back(Vertex{
                x + c.dx, y + c.dy, z + c.dz, r, g, b,
                c.u == 0.0F ? u0 : u1, c.v == 0.0F ? v0 : v1,
            });
          }
          mesh.indices.insert(mesh.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
        }
      }
    }
  }
  return mesh;
}

}  // namespace craftpp::render
