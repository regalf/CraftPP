#include "render/mesher.hpp"

#include <cmath>

#include "world/block.hpp"
#include "world/block_collision.hpp"
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

// Ambient-occlusion taps (renderStandardBlockWithAmbientOcclusion verbatim,
// fancy path). Corners follow kFaces emission order (V0..V3); all offsets
// are absolute from the block: C center (= face-neighbour cell), per corner
// FB fallback-side, OT other side, DG diagonal, G1/G2 canBlockGrass gates
// (unshifted frame). ao averages the 4 occlusion values; brightness mixes
// the 4 (sky, block) pairs with the packed-zero fallback to the center.
struct AoTap {
  int8_t fb[3], ot[3], dg[3], g1[3], g2[3];
};
struct AoFace {
  int8_t ox, oy, oz;  // center (= face neighbour) offset
  float shade;
  AoTap c[4];
};
constexpr AoFace kAo[6] = {
    // Bottom (side 0).
    {0, -1, 0, 0.5F,
     {{{-1, -1, 0}, {0, -1, 1}, {-1, -1, 1}, {0, -1, 1}, {-1, -1, 0}},
      {{-1, -1, 0}, {0, -1, -1}, {-1, -1, -1}, {0, -1, -1}, {-1, -1, 0}},
      {{1, -1, 0}, {0, -1, -1}, {1, -1, -1}, {0, -1, -1}, {1, -1, 0}},
      {{1, -1, 0}, {0, -1, 1}, {1, -1, 1}, {0, -1, 1}, {1, -1, 0}}}},
    // Top (side 1).
    {0, 1, 0, 1.0F,
     {{{1, 1, 0}, {0, 1, 1}, {1, 1, 1}, {0, 1, 1}, {1, 1, 0}},
      {{1, 1, 0}, {0, 1, -1}, {1, 1, -1}, {0, 1, -1}, {1, 1, 0}},
      {{-1, 1, 0}, {0, 1, -1}, {-1, 1, -1}, {0, 1, -1}, {-1, 1, 0}},
      {{-1, 1, 0}, {0, 1, 1}, {-1, 1, 1}, {0, 1, 1}, {-1, 1, 0}}}},
    // ZMin (side 2).
    {0, 0, -1, 0.8F,
     {{{-1, 0, -1}, {0, 1, -1}, {-1, 1, -1}, {-1, 0, -1}, {0, 1, -1}},
      {{1, 0, -1}, {0, 1, -1}, {1, 1, -1}, {1, 0, -1}, {0, 1, -1}},
      {{1, 0, -1}, {0, -1, -1}, {1, -1, -1}, {1, 0, -1}, {0, -1, -1}},
      {{-1, 0, -1}, {0, -1, -1}, {-1, -1, -1}, {-1, 0, -1}, {0, -1, -1}}}},
    // ZMax (side 3).
    {0, 0, 1, 0.8F,
     {{{-1, 0, 1}, {0, 1, 1}, {-1, 1, 1}, {-1, 0, 1}, {0, 1, 1}},
      {{-1, 0, 1}, {0, -1, 1}, {-1, -1, 1}, {-1, 0, 1}, {0, -1, 1}},
      {{1, 0, 1}, {0, -1, 1}, {1, -1, 1}, {1, 0, 1}, {0, -1, 1}},
      {{1, 0, 1}, {0, 1, 1}, {1, 1, 1}, {1, 0, 1}, {0, 1, 1}}}},
    // XMin (side 4).
    {-1, 0, 0, 0.6F,
     {{{-1, 0, 1}, {-1, 1, 0}, {-1, 1, 1}, {-1, 0, 1}, {-1, 1, 0}},
      {{-1, 0, -1}, {-1, 1, 0}, {-1, 1, -1}, {-1, 0, -1}, {-1, 1, 0}},
      {{-1, 0, -1}, {-1, -1, 0}, {-1, -1, -1}, {-1, 0, -1}, {-1, -1, 0}},
      {{-1, 0, 1}, {-1, -1, 0}, {-1, -1, 1}, {-1, 0, 1}, {-1, -1, 0}}}},
    // XMax (side 5).
    {1, 0, 0, 0.6F,
     {{{1, 0, 1}, {1, -1, 0}, {1, -1, 1}, {1, 0, -1}, {1, 1, 0}},
      {{1, 0, -1}, {1, -1, 0}, {1, -1, -1}, {1, 0, -1}, {1, -1, 0}},
      {{1, 0, -1}, {1, 1, 0}, {1, 1, -1}, {1, 0, 1}, {1, -1, 0}},
      {{1, 0, 1}, {1, 1, 0}, {1, 1, 1}, {1, 0, 1}, {1, 1, 0}}}},
};



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

// Exact-edge tile UVs (fluids use no bleed inset, like the source).
void tile_uv_exact(int tile, float& u0, float& u1, float& v0, float& v1) {
  const float tx = static_cast<float>((tile & 15) * 16);
  const float ty = static_cast<float>(tile & 240);
  u0 = tx / 256.0F;
  u1 = (tx + 16.0F) / 256.0F;
  v0 = ty / 256.0F;
  v1 = (ty + 16.0F) / 256.0F;
}

// BlockView over a RegionWorld for the flow-vector query (out of bounds =
// air, like World.getBlockId; y outside 0..127 reads 0 meta).
struct RegionView : world::BlockView {
  const world::RegionWorld& w;
  explicit RegionView(const world::RegionWorld& w_) : w(w_) {}
  int block_id(int x, int y, int z) const override {
    if (y < 0 || y >= world::RegionWorld::kHeight) return 0;
    return w.get_id(x, y, z);
  }
  int block_meta(int x, int y, int z) const override {
    if (y < 0 || y >= world::RegionWorld::kHeight) return 0;
    return w.get_meta(x, y, z);
  }
};

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

Mesh Mesher::mesh_live(const world::RegionWorld& world, int cx, int cz, int sky_sub) const {
  return mesh_live_impl(world, cx, cz, false, sky_sub);
}

Mesh Mesher::mesh_fluid_live(const world::RegionWorld& world, int cx, int cz, int sky_sub) const {
  return mesh_live_impl(world, cx, cz, true, sky_sub);
}

Mesh Mesher::mesh_live_impl(const world::RegionWorld& world, int cx, int cz, bool fluids_only,
                            int sky_sub) const {
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
  // Same face with per-corner colors (AO bake).
  auto emit_face_c = [&](int x, int y, int z, const FaceDesc& f, const float* cr,
                         const float* cg, const float* cb, int tile) {
    float u0, u1, v0, v1;
    tile_uv(tile, u0, u1, v0, v1);
    const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
    for (int i = 0; i < 4; ++i) {
      const float uu = f.corners[i].u == 0.0F ? u0 : u1;
      const float vv = f.corners[i].v == 0.0F ? v0 : v1;
      mesh.vertices.push_back({static_cast<float>(x) + f.corners[i].dx,
                               static_cast<float>(y) + f.corners[i].dy,
                               static_cast<float>(z) + f.corners[i].dz, cr[i], cg[i], cb[i], uu,
                               vv});
    }
    mesh.indices.insert(mesh.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
  };
  // Stored-light brightness for one face (sampled in the neighbour cell,
  // like the engine does): Chunk.getBlockLightValue with the daytime
  // subtraction, mapped through the WorldProvider lightBrightnessTable
  // (lightLevel 0: (1-f)/(3f+1), f = 1-i/15). No readability floor: caves
  // go properly black like vanilla; gradation comes from the engine BFS.
  auto brightness = [&](int nx, int ny, int nz) {
    const int sky = world.saved_sky(nx, ny, nz) - sky_sub;
    const int blk = world.saved_block(nx, ny, nz);
    return world::bid::light_brightness(sky > blk ? sky : blk);
  };
  // Per-corner AO bake (renderStandardBlockWithAmbientOcclusion, fancy
  // path): each corner mixes its 2 side cells + diagonal + face center
  // (getAoBrightness zero-fallback + int-division averages), mapped
  // through the brightness table like brightness() above. cc[i] includes
  // the face shade but no biome tint (applied by the caller per channel).
  auto ao_cell = [&](int cx, int cy, int cz, float& ao, int& s, int& b) {
    const int gid = (cy < 0 || cy >= world::RegionWorld::kHeight)
                        ? 0
                        : world.get_id(cx, cy, cz);
    ao = (world::bid::material_opaque(gid) && world::bid::renders_as_normal(gid)) ? 0.2F
                                                                                   : 1.0F;
    s = world.saved_sky(cx, cy, cz);
    b = world.saved_block(cx, cy, cz);
  };
  auto ao_grass = [&](int cx, int cy, int cz) {
    // Block.canBlockGrass gate parity: vanilla falls back to the side
    // cell when BOTH gate cells are getCanBlockGrass() (solid sides).
    // can_block_grass() mirrors Material.getCanBlockGrass exactly
    // (false only for air/fire/plants/vine/snow/circuits materials).
    const int gid = (cy < 0 || cy >= world::RegionWorld::kHeight)
                        ? 0
                        : world.get_id(cx, cy, cz);
    return world::bid::can_block_grass(gid);
  };
  auto ao_corners = [&](int bx, int by, int bz, const FaceDesc& f, float* cc) {
    const AoFace& af = kAo[static_cast<int>(f.face)];
    for (int i = 0; i < 4; ++i) {
      const AoTap& t = af.c[i];
      float aF = -1, aO = -1, aD = -1, aC = -1;
      int sF = -1, bF = -1, sO = -1, bO = -1, sD = -1, bD = -1, sC = -1, bC = -1;
      ao_cell(bx + t.fb[0], by + t.fb[1], bz + t.fb[2], aF, sF, bF);
      ao_cell(bx + t.ot[0], by + t.ot[1], bz + t.ot[2], aO, sO, bO);
      ao_cell(bx + af.ox, by + af.oy, bz + af.oz, aC, sC, bC);
      // Diagonal gate (verbatim): vanilla tests !canBlockGrass[] where the
      // field is the NEGATION of Material.getCanBlockGrass, i.e. falls back
      // when both gate cells ARE getCanBlockGrass (solid sides).
      if (ao_grass(bx + t.g1[0], by + t.g1[1], bz + t.g1[2]) &&
          ao_grass(bx + t.g2[0], by + t.g2[1], bz + t.g2[2])) {
        aD = aF;
        sD = sF;
        bD = bF;
      } else {
        ao_cell(bx + t.dg[0], by + t.dg[1], bz + t.dg[2], aD, sD, bD);
      }
      // Packed-zero fallback (getAoBrightness verbatim): brightness-only.
      // The ao averages (var9-12) never fall back.
      if (sF == 0 && bF == 0) {
        sF = sC;
        bF = bC;
      }
      if (sO == 0 && bO == 0) {
        sO = sC;
        bO = bC;
      }
      if (sD == 0 && bD == 0) {
        sD = sC;
        bD = bC;
      }
      const float ao_avg = (aF + aO + aD + aC) * 0.25F;
      const int sky_c = (sF + sO + sD + sC) / 4;
      const int blk_c = (bF + bO + bD + bC) / 4;
      const float base = world::bid::light_brightness(sky_c - sky_sub > blk_c ? sky_c - sky_sub
                                                                               : blk_c);
      cc[i] = af.shade * ao_avg * base;

    }
  };

  for (int lz = 0; lz < 16; ++lz) {
    for (int lx = 0; lx < 16; ++lx) {
      const int x = cx * 16 + lx, z = cz * 16 + lz;
      for (int y = 0; y < world::RegionWorld::kHeight; ++y) {
        const int id = world.get_id(x, y, z);
        if (id == 0) continue;
        const bool is_fluid = (id == 8 || id == 9 || id == 10 || id == 11);
        if (is_fluid != fluids_only) continue;
        const int meta = world.get_meta(x, y, z);
        const int rt = world::bid::render_type(id);
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
        if (is_fluid) {
          // Verbatim RenderBlocks.renderBlockFluids: corner heights from
          // the 2x2 getFluidHeight average (fluid above forces 1.0, still
          // cells weigh x10, air counts 1.0, solids are skipped), top quad
          // with flow-rotated UVs, bottom face, and sides squeezed from
          // the corner heights down to the block base. Tint is white
          // (non-swamp water and lava both return 0xFFFFFF).
          const RegionView view(world);
          const bool is_water = (id == 8 || id == 9);
          auto same = [&](int nid) {
            return is_water ? (nid == 8 || nid == 9) : (nid == 10 || nid == 11);
          };
          auto mat_id = [&](int bx, int by, int bz) {
            if (by < 0 || by >= world::RegionWorld::kHeight) return 0;
            return world.get_id(bx, by, bz);
          };
          auto fluid_height = [&](int bx, int by, int bz) {
            int count = 0;
            float sum = 0.0F;
            for (int s = 0; s < 4; ++s) {
              const int sx = bx - (s & 1), sz = bz - ((s >> 1) & 1);
              if (same(mat_id(sx, by + 1, sz))) return 1.0F;
              const int nid = mat_id(sx, by, sz);
              if (same(nid)) {
                const int m = world.get_meta(sx, by, sz);
                if (m >= 8 || m == 0) {
                  sum += world::fluid_height_percent(m) * 10.0F;
                  count += 10;
                }
                sum += world::fluid_height_percent(m);
                ++count;
              } else if (!world::bid::material_is_solid(nid)) {
                sum += 1.0F;
                ++count;
              }
            }
            return 1.0F - sum / static_cast<float>(count);
          };
          const float h00 = fluid_height(x, y, z) - 0.001F;
          const float h01 = fluid_height(x, y, z + 1) - 0.001F;
          const float h11 = fluid_height(x + 1, y, z + 1) - 0.001F;
          const float h10 = fluid_height(x + 1, y, z) - 0.001F;
          const float xf = static_cast<float>(x), yf = static_cast<float>(y),
                      zf = static_cast<float>(z);
          const auto flow = world::fluid_flow_vector(id, x, y, z, view);
          const bool still = (flow.x == 0.0 && flow.z == 0.0);
          const double ang =
              still ? 0.0 : std::atan2(flow.z, flow.x) - M_PI * 0.5;
          const int top_tile = world::bid::block_texture(id, still ? 1 : 2, meta);
          float tu0, tu1, tv0, tv1;
          tile_uv_exact(top_tile, tu0, tu1, tv0, tv1);
          float cu, cv;
          if (still) {
            cu = (tu0 + tu1) * 0.5F;
            cv = (tv0 + tv1) * 0.5F;
          } else {
            cu = tu1;
            cv = tv1;
          }
          const float su = static_cast<float>(std::sin(ang)) * 8.0F / 256.0F;
          const float co = static_cast<float>(std::cos(ang)) * 8.0F / 256.0F;
          if (!same(mat_id(x, y + 1, z))) {
            const float b = brightness(x, y, z);
            emit_quad_uv(xf, yf + h00, zf, cu - co - su, cv - co + su, xf, yf + h01, zf + 1,
                         cu - co + su, cv + co + su, xf + 1, yf + h11, zf + 1, cu + co + su,
                         cv + co - su, xf + 1, yf + h10, zf, cu + co - su, cv - co - su, b, b,
                         b);
          }
          const int below = mat_id(x, y - 1, z);
          if (!same(below) && below != world::bid::kIce && !world::bid::is_opaque(below)) {
            const float b = brightness(x, y - 1, z) * 0.5F;
            float bu0, bu1, bv0, bv1;
            tile_uv_exact(world::bid::block_texture(id, 0, meta), bu0, bu1, bv0, bv1);
            const float e = yf + 0.001F;
            emit_quad_uv(xf, e, zf + 1, bu0, bv1, xf, e, zf, bu0, bv0, xf + 1, e, zf, bu1, bv0,
                         xf + 1, e, zf + 1, bu1, bv1, b, b, b);
          }
          const int side_tile = world::bid::block_texture(id, 2, meta);
          float su0, su1, sv0, sv1;
          tile_uv_exact(side_tile, su0, su1, sv0, sv1);
          const float svb = sv1 - 0.01F / 256.0F;
          const float sue = su1 - 0.01F / 256.0F;
          const float e = 0.001F;
          const int nb[4][3] = {{x, y, z - 1}, {x, y, z + 1}, {x - 1, y, z}, {x + 1, y, z}};
          for (int s = 0; s < 4; ++s) {
            const int nid = mat_id(nb[s][0], nb[s][1], nb[s][2]);
            if (same(nid) || nid == world::bid::kIce || world::bid::is_opaque(nid)) continue;
            const float b = brightness(nb[s][0], nb[s][1], nb[s][2]) * (s < 2 ? 0.8F : 0.6F);
            float ha, hb;
            if (s == 0) {
              ha = h00;
              hb = h10;
              const float vt0 = (sv0 * 256.0F + (1.0F - ha) * 16.0F) / 256.0F;
              const float vt1 = (sv0 * 256.0F + (1.0F - hb) * 16.0F) / 256.0F;
              emit_quad_uv(xf, yf + ha, zf + e, su0, vt0, xf + 1, yf + hb, zf + e, sue, vt1,
                           xf + 1, yf, zf + e, sue, svb, xf, yf, zf + e, su0, svb, b, b, b);
            } else if (s == 1) {
              ha = h11;
              hb = h01;
              const float vt0 = (sv0 * 256.0F + (1.0F - ha) * 16.0F) / 256.0F;
              const float vt1 = (sv0 * 256.0F + (1.0F - hb) * 16.0F) / 256.0F;
              emit_quad_uv(xf + 1, yf + ha, zf + 1 - e, su0, vt0, xf, yf + hb, zf + 1 - e, sue,
                           vt1, xf, yf, zf + 1 - e, sue, svb, xf + 1, yf, zf + 1 - e, su0, svb,
                           b, b, b);
            } else if (s == 2) {
              ha = h01;
              hb = h00;
              const float vt0 = (sv0 * 256.0F + (1.0F - ha) * 16.0F) / 256.0F;
              const float vt1 = (sv0 * 256.0F + (1.0F - hb) * 16.0F) / 256.0F;
              emit_quad_uv(xf + e, yf + ha, zf + 1, su0, vt0, xf + e, yf + hb, zf, sue, vt1,
                           xf + e, yf, zf, sue, svb, xf + e, yf, zf + 1, su0, svb, b, b, b);
            } else {
              ha = h10;
              hb = h11;
              const float vt0 = (sv0 * 256.0F + (1.0F - ha) * 16.0F) / 256.0F;
              const float vt1 = (sv0 * 256.0F + (1.0F - hb) * 16.0F) / 256.0F;
              emit_quad_uv(xf + 1 - e, yf + ha, zf, su0, vt0, xf + 1 - e, yf + hb, zf + 1, sue,
                           vt1, xf + 1 - e, yf, zf + 1, sue, svb, xf + 1 - e, yf, zf, su0, svb,
                           b, b, b);
            }
          }
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
          const bool occluding = world::bid::is_opaque(id) && !leaves;
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
          // Fancy AO (renderStandardBlockWithAmbientOcclusion): non-emissive
          // full cubes bake per-corner light; emissive blocks keep the flat
          // path below like renderStandardBlockWithColorMultiplier.
          if (world::bid::light_value(id) == 0) {
            float cc[4];
            ao_corners(x, y, z, f, cc);
            float cr[4], cg[4], cb[4];
            if (id == 2) {
              if (side == 1) {
                float tr = 1, tg = 1, tb = 1;
                tint(x, z, false, tr, tg, tb);
                for (int i = 0; i < 4; ++i) {
                  cr[i] = cc[i] * tr;
                  cg[i] = cc[i] * tg;
                  cb[i] = cc[i] * tb;
                }
                emit_face_c(x, y, z, f, cr, cg, cb, tile);
              } else if (side == 0) {
                for (int i = 0; i < 4; ++i) {
                  cr[i] = cc[i];
                  cg[i] = cc[i];
                  cb[i] = cc[i];
                }
                emit_face_c(x, y, z, f, cr, cg, cb, tile);
              } else {
                for (int i = 0; i < 4; ++i) {
                  cr[i] = cc[i];
                  cg[i] = cc[i];
                  cb[i] = cc[i];
                }
                emit_face_c(x, y, z, f, cr, cg, cb, tile);
                float tr = 1, tg = 1, tb = 1;
                tint(x, z, false, tr, tg, tb);
                for (int i = 0; i < 4; ++i) {
                  cr[i] = cc[i] * tr;
                  cg[i] = cc[i] * tg;
                  cb[i] = cc[i] * tb;
                }
                emit_face_c(x, y, z, f, cr, cg, cb, 38);
              }
              continue;
            }
            float tr = 1, tg = 1, tb = 1;
            if (id == 18) {
              if ((meta & 3) == 1) {
                tr = 0x61 / 255.0F;
                tg = 0x99 / 255.0F;
                tb = 0x41 / 255.0F;
              } else if ((meta & 3) == 2) {
                tr = 0x80 / 255.0F;
                tg = 0xA7 / 255.0F;
                tb = 0x25 / 255.0F;
              } else {
                tint(x, z, true, tr, tg, tb);
              }
            }
            for (int i = 0; i < 4; ++i) {
              cr[i] = cc[i] * tr;
              cg[i] = cc[i] * tg;
              cb[i] = cc[i] * tb;
            }
            emit_face_c(x, y, z, f, cr, cg, cb, tile);
            continue;
          }
          if (id == 2) {
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
