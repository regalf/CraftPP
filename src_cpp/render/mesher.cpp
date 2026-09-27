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
            // M2 path keeps the flat plains tint (no column lookup here).
            r *= 0.486F;
            g *= 0.741F;
            b *= 0.349F;
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
  // Quad with explicit UVs per corner (for cross-quads below).
  auto emit_quad_uv = [&](float x0, float y0, float z0, float u0, float v0, float x1, float y1,
                          float z1, float u1, float v1, float x2, float y2, float z2, float u2,
                          float v2, float x3, float y3, float z3, float u3, float v3, float r,
                          float g, float b) {
    const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back(Vertex{x0, y0, z0, r, g, b, u0, v0});
    mesh.vertices.push_back(Vertex{x1, y1, z1, r, g, b, u1, v1});
    mesh.vertices.push_back(Vertex{x2, y2, z2, r, g, b, u2, v2});
    mesh.vertices.push_back(Vertex{x3, y3, z3, r, g, b, u3, v3});
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
          // renderCrossedSquares: two diagonals x two windings (visible
          // from every side under any culling), inset +-0.45, exact UVs.
          const int tile = world::bid::block_texture(id, 2, meta);
          float u0, u1, v0, v1;
          tile_uv(tile, u0, u1, v0, v1);
          const float b = brightness(x, y, z);
          float tr = b, tg = b, tb = b;
          if (id == 31) {
            // Tall grass uses the biome grass color (colorMultiplier).
            tint(x, z, false, tr, tg, tb);
            tr *= b;
            tg *= b;
            tb *= b;
          }
          const float xa = x + 0.05F, xb = x + 0.95F;
          const float za = z + 0.05F, zb = z + 0.95F;
          const float y0 = y, y1 = y + 1;
          emit_quad_uv(xa, y1, za, u0, v0, xa, y0, za, u0, v1, xb, y0, zb, u1, v1, xb, y1, zb,
                       u1, v0, tr, tg, tb);
          emit_quad_uv(xb, y1, zb, u0, v0, xb, y0, zb, u0, v1, xa, y0, za, u1, v1, xa, y1, za,
                       u1, v0, tr, tg, tb);
          emit_quad_uv(xa, y1, zb, u0, v0, xa, y0, zb, u0, v1, xb, y0, za, u1, v1, xb, y1, za,
                       u1, v0, tr, tg, tb);
          emit_quad_uv(xb, y1, za, u0, v0, xb, y0, za, u0, v1, xa, y0, zb, u1, v1, xa, y1, zb,
                       u1, v0, tr, tg, tb);
          continue;
        }
        if (id == world::bid::kStepSingle || id == world::bid::kSnowCover) {
          // renderStandardBlock with state bounds: clipped box, full-tile
          // UVs squeezed onto each face, vanilla face shading. Culling is
          // the generic non-occluder rule (slab sides additionally hide
          // against the same slab, like BlockStep.shouldSideBeRendered;
          // the always-emit top/bottom of the source are depth-hidden
          // anyway, so skipping them against opaque cubes is identical).
          const double h = id == world::bid::kStepSingle ? 0.5 : (2.0 * (1 + (meta & 7))) / 16.0;
          for (const FaceDesc& f : kFaces) {
            const int nx = x + f.nx, ny = y + f.ny, nz = z + f.nz;
            const int nid = (ny < 0 || ny >= world::RegionWorld::kHeight)
                                ? 0
                                : world.get_id(nx, ny, nz);
            if (!(nid == 0 || (!world::bid::is_opaque(nid) && nid != id))) continue;
            const int side = (f.face == Face::Bottom) ? 0
                             : (f.face == Face::Top)  ? 1
                             : (f.face == Face::ZMin) ? 2
                             : (f.face == Face::ZMax) ? 3
                             : (f.face == Face::XMin) ? 4
                                                      : 5;
            const int tile = world::bid::block_texture(id, side, meta);
            float u0, u1, v0, v1;
            tile_uv(tile, u0, u1, v0, v1);
            const float b = brightness(nx, ny, nz) * f.shade;
            const float us[4] = {f.corners[0].u, f.corners[1].u, f.corners[2].u, f.corners[3].u};
            const float vs[4] = {f.corners[0].v, f.corners[1].v, f.corners[2].v, f.corners[3].v};
            emit_quad(
                x + f.corners[0].dx, static_cast<float>(y + f.corners[0].dy * h),
                z + f.corners[0].dz, x + f.corners[1].dx,
                static_cast<float>(y + f.corners[1].dy * h), z + f.corners[1].dz,
                x + f.corners[2].dx, static_cast<float>(y + f.corners[2].dy * h),
                z + f.corners[2].dz, x + f.corners[3].dx,
                static_cast<float>(y + f.corners[3].dy * h), z + f.corners[3].dz, b, b, b, u0, u1,
                v0, v1, us, vs);
          }
          continue;
        }
        if (id == world::bid::kLadder) {
          // Verbatim RenderBlocks.renderBlockLadder: one wall quad per
          // meta (single winding — invisible from behind, like vanilla),
          // full brightness, no face shading.
          const int tile = world::bid::block_texture(id, 0, meta);
          float u0, u1, v0, v1;
          tile_uv(tile, u0, u1, v0, v1);
          const float b = brightness(x, y, z);
          const float t = 0.05F, xf = static_cast<float>(x), yf = static_cast<float>(y),
                      zf = static_cast<float>(z);
          if (meta == 5) {
            emit_quad_uv(xf + t, yf + 1, zf + 1, u0, v0, xf + t, yf, zf + 1, u0, v1, xf + t, yf,
                         zf, u1, v1, xf + t, yf + 1, zf, u1, v0, b, b, b);
          } else if (meta == 4) {
            emit_quad_uv(xf + 1 - t, yf, zf + 1, u1, v1, xf + 1 - t, yf + 1, zf + 1, u1, v0,
                         xf + 1 - t, yf + 1, zf, u0, v0, xf + 1 - t, yf, zf, u0, v1, b, b, b);
          } else if (meta == 3) {
            emit_quad_uv(xf + 1, yf, zf + t, u1, v1, xf + 1, yf + 1, zf + t, u1, v0, xf, yf + 1,
                         zf + t, u0, v0, xf, yf, zf + t, u0, v1, b, b, b);
          } else if (meta == 2) {
            emit_quad_uv(xf + 1, yf + 1, zf + 1 - t, u0, v0, xf + 1, yf, zf + 1 - t, u0, v1, xf,
                         yf, zf + 1 - t, u1, v1, xf, yf + 1, zf + 1 - t, u1, v0, b, b, b);
          }
          continue;
        }
        if (id == world::bid::kVine) {
          // Verbatim RenderBlocks.renderBlockVine: one double-wound face
          // per set meta bit + top face under a normal cube, foliage tint.
          const int tile = world::bid::block_texture(id, 0, meta);
          float u0, u1, v0, v1;
          tile_uv(tile, u0, u1, v0, v1);
          const float b = brightness(x, y, z);
          float tr = 1, tg = 1, tb = 1;
          tint(x, z, true, tr, tg, tb);
          const float r = b * tr, g = b * tg, bl = b * tb;
          const float t = 0.05F, xf = static_cast<float>(x), yf = static_cast<float>(y),
                      zf = static_cast<float>(z);
          if ((meta & 2) != 0) {
            emit_quad_uv(xf + t, yf + 1, zf + 1, u0, v0, xf + t, yf, zf + 1, u0, v1, xf + t, yf,
                         zf, u1, v1, xf + t, yf + 1, zf, u1, v0, r, g, bl);
            emit_quad_uv(xf + t, yf + 1, zf, u1, v0, xf + t, yf, zf, u1, v1, xf + t, yf, zf + 1,
                         u0, v1, xf + t, yf + 1, zf + 1, u0, v0, r, g, bl);
          }
          if ((meta & 8) != 0) {
            emit_quad_uv(xf + 1 - t, yf, zf + 1, u1, v1, xf + 1 - t, yf + 1, zf + 1, u1, v0,
                         xf + 1 - t, yf + 1, zf, u0, v0, xf + 1 - t, yf, zf, u0, v1, r, g, bl);
            emit_quad_uv(xf + 1 - t, yf, zf, u0, v1, xf + 1 - t, yf + 1, zf, u0, v0,
                         xf + 1 - t, yf + 1, zf + 1, u1, v0, xf + 1 - t, yf, zf + 1, u1, v1, r, g,
                         bl);
          }
          if ((meta & 4) != 0) {
            emit_quad_uv(xf + 1, yf, zf + t, u1, v1, xf + 1, yf + 1, zf + t, u1, v0, xf, yf + 1,
                         zf + t, u0, v0, xf, yf, zf + t, u0, v1, r, g, bl);
            emit_quad_uv(xf, yf, zf + t, u0, v1, xf, yf + 1, zf + t, u0, v0, xf + 1, yf + 1,
                         zf + t, u1, v0, xf + 1, yf, zf + t, u1, v1, r, g, bl);
          }
          if ((meta & 1) != 0) {
            emit_quad_uv(xf + 1, yf + 1, zf + 1 - t, u0, v0, xf + 1, yf, zf + 1 - t, u0, v1, xf,
                         yf, zf + 1 - t, u1, v1, xf, yf + 1, zf + 1 - t, u1, v0, r, g, bl);
            emit_quad_uv(xf, yf + 1, zf + 1 - t, u1, v0, xf, yf, zf + 1 - t, u1, v1, xf + 1, yf,
                         zf + 1 - t, u0, v1, xf + 1, yf + 1, zf + 1 - t, u0, v0, r, g, bl);
          }
          const int above =
              (y + 1 >= world::RegionWorld::kHeight) ? 0 : world.get_id(x, y + 1, z);
          if (above != 0 && world::bid::is_opaque(above) && world::bid::renders_as_normal(above)) {
            emit_quad_uv(xf + 1, yf + 1 - t, zf, u0, v0, xf + 1, yf + 1 - t, zf + 1, u0, v1, xf,
                         yf + 1 - t, zf + 1, u1, v1, xf, yf + 1 - t, zf, u1, v0, r, g, bl);
          }
          continue;
        }
        const bool leaves = (id == 18);
        const bool occluding = world::bid::is_opaque(id) && !leaves && !water;
        for (const FaceDesc& f : kFaces) {
          const int nx = x + f.nx, ny = y + f.ny, nz = z + f.nz;
          const int nid = (ny < 0 || ny >= world::RegionWorld::kHeight)
                              ? 0
                              : world.get_id(nx, ny, nz);
          bool emit = false;
          if (occluding) {
            // shouldSideBeRendered: render unless the neighbour is an
            // opaque cube; fancy leaves never occlude (cutout holes).
            emit = !world::bid::is_opaque(nid) || nid == 18 || nid == 8 || nid == 9;
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
          } else if (id == 2) {
            if (side == 1) {
              // Top: grayscale tile tinted with the biome color.
              float tr = 1, tg = 1, tb = 1;
              tint(x, z, false, tr, tg, tb);
              emit_face(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), f,
                        b * tr, b * tg, b * tb, tile);
            } else if (side == 0) {
              emit_face(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), f,
                        b, b, b, tile);
            } else {
              // Sides: dirt tile untinted, then the fancy-grass overlay
              // (tile 38, transparent dirt area) with the biome tint.
              emit_face(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), f,
                        b, b, b, tile);
              float tr = 1, tg = 1, tb = 1;
              tint(x, z, false, tr, tg, tb);
              emit_face(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), f,
                        b * tr, b * tg, b * tb, 38);
            }
            continue;  // grass emits its own faces above
          } else if (id == 18) {
            if ((meta & 3) == 1) {
              // Pine (ColorizerFoliage.getFoliageColorPine = 6396257).
              r *= 0x61 / 255.0F;
              g *= 0x99 / 255.0F;
              bl *= 0x41 / 255.0F;
            } else if ((meta & 3) == 2) {
              // Birch (getFoliageColorBirch = 8431445).
              r *= 0x80 / 255.0F;
              g *= 0xA7 / 255.0F;
              bl *= 0x25 / 255.0F;
            } else {
              float tr = 1, tg = 1, tb = 1;
              tint(x, z, true, tr, tg, tb);
              r *= tr;
              g *= tg;
              bl *= tb;
            }
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
