#include "render/mesher.hpp"

#include "world/block.hpp"
#include "world/blocks.hpp"

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
        // Water renders its own faces but never occludes (source fluid logic,
        // simplified for M3: faces against air only).
        const bool occluding = def.opaque && id != BlockId::Water;

        for (const FaceDesc& f : kFaces) {
          const BlockId neighbor = chunk.get(x + f.nx, y + f.ny, z + f.nz);
          if (occluding) {
            if (world::block_def(neighbor).opaque && neighbor != BlockId::Water) continue;
          } else {
            if (neighbor != BlockId::Air) continue;
          }

          const int tile = world::tile_for(id, f.face);
          float u0 = 0.0F;
          float u1 = 0.0F;
          float v0 = 0.0F;
          float v1 = 0.0F;
          tile_uv(tile, u0, u1, v0, v1);

          float r = f.shade;
          float g = f.shade;
          float b = f.shade;
          if (id == BlockId::Water) {
            r *= 0.15F;
            g *= 0.35F;
            b *= 0.85F;
          } else if (def.grass_tinted && f.face != Face::Bottom) {
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

Mesh Mesher::mesh_live(const world::RegionWorld& world, int cx, int cz) const {
  Mesh mesh;
  mesh.vertices.reserve(8192);
  mesh.indices.reserve(12288);

  auto emit_quad = [&](float x0, float y0, float z0, float x1, float y1, float z1, float x2,
                       float y2, float z2, float x3, float y3, float z3, float r, float g, float b,
                       float u0, float u1, float v0, float v1, const float* us, const float* vs) {
    const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
    const float xs[4] = {x0, x1, x2, x3};
    const float ys[4] = {y0, y1, y2, y3};
    const float zs[4] = {z0, z1, z2, z3};
    for (int i = 0; i < 4; ++i) {
      mesh.vertices.push_back(Vertex{xs[i], ys[i], zs[i], r, g, b, us[i] == 0.0F ? u0 : u1,
                                    vs[i] == 0.0F ? v0 : v1});
    }
    mesh.indices.insert(mesh.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
  };
  auto emit_face = [&](int x, int y, int z, const FaceDesc& f, float r, float g, float b, int tile) {
    float u0, u1, v0, v1;
    tile_uv(tile, u0, u1, v0, v1);
    const float us[4] = {f.corners[0].u, f.corners[1].u, f.corners[2].u, f.corners[3].u};
    const float vs[4] = {f.corners[0].v, f.corners[1].v, f.corners[2].v, f.corners[3].v};
    emit_quad(x + f.corners[0].dx, y + f.corners[0].dy, z + f.corners[0].dz, x + f.corners[1].dx,
              y + f.corners[1].dy, z + f.corners[1].dz, x + f.corners[2].dx, y + f.corners[2].dy,
              z + f.corners[2].dz, x + f.corners[3].dx, y + f.corners[3].dy, z + f.corners[3].dz, r,
              g, b, u0, u1, v0, v1, us, vs);
  };
  // Stored-light brightness for one face (sampled in the neighbour cell,
  // like the engine does; floor keeps caves readable).
  auto brightness = [&](int nx, int ny, int nz) {
    const int l = world.full_light(nx, ny, nz);
    const int c = l < 4 ? 4 : l;
    return static_cast<float>(c) / 15.0F;
  };

  for (int lz = 0; lz < 16; ++lz) {
    for (int lx = 0; lx < 16; ++lx) {
      const int x = cx * 16 + lx, z = cz * 16 + lz;
      for (int y = 0; y < world::RegionWorld::kHeight; ++y) {
        const int id = world.get_id(x, y, z);
        if (id == 0) continue;
        const int meta = world.get_meta(x, y, z);
        const int rt = world::bid::render_type(id);
        const bool water = (id == 8 || id == 9);
        if (rt == 1 || rt == 2 || rt == 3) {
          // Cross quads (plants, torch, fire): two diagonals, full texture.
          const int tile = world::bid::block_texture(id, 2, meta);
          float u0, u1, v0, v1;
          tile_uv(tile, u0, u1, v0, v1);
          const float b = brightness(x, y, z);
          const float us[4] = {0, 0, 1, 1};
          const float vs[4] = {0, 1, 1, 0};
          const float o = 0.15F;
          emit_quad(x + o, y, z + o, x + o, y + 1, z + o, x + 1 - o, y + 1, z + 1 - o, x + 1 - o, y,
                    z + 1 - o, b, b, b, u0, u1, v0, v1, us, vs);
          emit_quad(x + 1 - o, y, z + o, x + 1 - o, y + 1, z + o, x + o, y + 1, z + 1 - o, x + o, y,
                    z + 1 - o, b, b, b, u0, u1, v0, v1, us, vs);
          continue;
        }
        const bool occluding = world::bid::is_opaque(id) && !water;
        for (const FaceDesc& f : kFaces) {
          const int nx = x + f.nx, ny = y + f.ny, nz = z + f.nz;
          const int nid = (ny < 0 || ny >= world::RegionWorld::kHeight)
                              ? 0
                              : world.get_id(nx, ny, nz);
          bool emit = false;
          if (occluding) {
            emit = !world::bid::is_opaque(nid) || nid == 8 || nid == 9;
          } else if (water) {
            emit = nid == 0;
          } else {
            emit = nid == 0 || (!world::bid::is_opaque(nid) && nid != id);
          }
          if (!emit) continue;
          const int side = (f.face == Face::Bottom) ? 0
                           : (f.face == Face::Top)  ? 1
                           : (f.face == Face::ZMin) ? 2
                           : (f.face == Face::ZMax) ? 3
                           : (f.face == Face::XMin) ? 4
                                                    : 5;
          const int tile = world::bid::block_texture(id, side, meta);
          const float b = brightness(nx, ny, nz) * f.shade;
          float r = b, g = b, bl = b;
          if (water) {
            r *= 0.15F;
            g *= 0.35F;
            bl *= 0.85F;
          } else if (id == 2 && side != 0) {
            r *= tint_r;
            g *= tint_g;
            bl *= tint_b;
          } else if (id == 18) {
            r *= foliage_r;
            g *= foliage_g;
            bl *= foliage_b;
          }
          emit_face(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), f, r, g,
                    bl, tile);
        }
      }
    }
  }
  return mesh;
}

}  // namespace craftpp::render
