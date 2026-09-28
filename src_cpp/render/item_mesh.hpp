#pragma once

#include <cmath>
#include <cstdint>

#include "gui/item_icons.hpp"
#include "render/mesh.hpp"
#include "world/blocks.hpp"

namespace craftpp::render::item_mesh {

// Column-major GL-style matrix (post-multiply like the source chains).
struct Mat4 {
  float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  static Mat4 mul(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for (int c = 0; c < 4; ++c)
      for (int r2 = 0; r2 < 4; ++r2) {
        float s = 0;
        for (int k = 0; k < 4; ++k) s += a.m[k * 4 + r2] * b.m[c * 4 + k];
        r.m[c * 4 + r2] = s;
      }
    return r;
  }
  void translate(float x, float y, float z) {
    Mat4 t;
    t.m[12] = x;
    t.m[13] = y;
    t.m[14] = z;
    *this = mul(*this, t);
  }
  void rotate(float deg, float x, float y, float z) {
    const float r = deg * 3.14159265F / 180.0F;
    const float c = std::cos(r), s = std::sin(r);
    Mat4 m;
    if (x > 0.5F) {
      m.m[5] = c;
      m.m[6] = s;
      m.m[9] = -s;
      m.m[10] = c;
    } else if (y > 0.5F) {
      m.m[0] = c;
      m.m[2] = -s;
      m.m[8] = s;
      m.m[10] = c;
    } else {
      m.m[0] = c;
      m.m[1] = s;
      m.m[4] = -s;
      m.m[5] = c;
    }
    *this = mul(*this, m);
  }
  void scale(float x, float y, float z) {
    Mat4 m;
    m.m[0] = x;
    m.m[5] = y;
    m.m[10] = z;
    *this = mul(*this, m);
  }
};

inline void emit_quad_uv(Mesh& m, const Mat4& t, float x0, float y0, float z0, float x1, float y1,
                         float z1, float x2, float y2, float z2, float x3, float y3, float z3,
                         float r, float g, float b, float u0, float v0, float u1, float v1,
                         float u2, float v2, float u3, float v3) {
  const float p[4][3] = {{x0, y0, z0}, {x1, y1, z1}, {x2, y2, z2}, {x3, y3, z3}};
  const float uv[4][2] = {{u0, v0}, {u1, v1}, {u2, v2}, {u3, v3}};
  const std::uint32_t base = static_cast<std::uint32_t>(m.vertices.size());
  for (int i = 0; i < 4; ++i) {
    const float* mm = t.m;
    const float x = mm[0] * p[i][0] + mm[4] * p[i][1] + mm[8] * p[i][2] + mm[12];
    const float y = mm[1] * p[i][0] + mm[5] * p[i][1] + mm[9] * p[i][2] + mm[13];
    const float z = mm[2] * p[i][0] + mm[6] * p[i][1] + mm[10] * p[i][2] + mm[14];
    m.vertices.push_back({x, y, z, r, g, b, uv[i][0], uv[i][1]});
  }
  m.indices.insert(m.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
}

// Full unit cube centered on the origin, mesher corner order (CCW front).
inline void emit_unit_cube(Mesh& m, const Mat4& t, int id, int damage, float bright) {
  struct Face {
    float c[4][3];
    int side;
  };
  const float h = 0.5F;
  const Face faces[6] = {
      {{{-h, -h, h}, {-h, -h, -h}, {h, -h, -h}, {h, -h, h}}, 0},
      {{{h, h, h}, {h, h, -h}, {-h, h, -h}, {-h, h, h}}, 1},
      {{{-h, h, -h}, {h, h, -h}, {h, -h, -h}, {-h, -h, -h}}, 2},
      {{{-h, h, h}, {-h, -h, h}, {h, -h, h}, {h, h, h}}, 3},
      {{{-h, h, h}, {-h, h, -h}, {-h, -h, -h}, {-h, -h, h}}, 4},
      {{{h, -h, h}, {h, -h, -h}, {h, h, -h}, {h, h, h}}, 5},
  };
  for (const Face& f : faces) {
    const int tile = world::bid::block_texture(id, f.side, damage);
    const float tx = static_cast<float>((tile & 15) * 16);
    const float ty = static_cast<float>(tile & 240);
    const float u0 = tx / 256.0F, u1 = (tx + 16.0F - 0.01F) / 256.0F;
    const float v0 = ty / 256.0F, v1 = (ty + 16.0F - 0.01F) / 256.0F;
    emit_quad_uv(m, t, f.c[0][0], f.c[0][1], f.c[0][2], f.c[1][0], f.c[1][1], f.c[1][2], f.c[2][0],
                 f.c[2][1], f.c[2][2], f.c[3][0], f.c[3][1], f.c[3][2], bright, bright, bright,
                 u0, v1, u0, v0, u1, v0, u1, v1);
  }
}

// func_40686_a extrusion (1/16 thick icon): front/back + 16-slice sides.
// UVs are texel-centered (vanilla uses exact edges, which is equivalent
// under its LINEAR filtering; under our NEAREST, edge texels would bleed
// into neighbor tiles and fringe the silhouette).
inline void emit_extruded(Mesh& m, const Mat4& t, int tile, float bright) {
  const float tx = static_cast<float>((tile & 15) * 16);
  const float ty = static_cast<float>(tile & 240);
  const float u0 = (tx + 0.5F) / 256.0F, u1 = (tx + 15.5F) / 256.0F;
  const float v0 = (ty + 0.5F) / 256.0F, v1 = (ty + 15.5F) / 256.0F;
  constexpr float kTh = 1.0F / 16.0F;
  emit_quad_uv(m, t, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, bright, bright, bright, u0, v1, u1,
               v1, u1, v0, u0, v0);
  emit_quad_uv(m, t, 0, 1, -kTh, 1, 1, -kTh, 1, 0, -kTh, 0, 0, -kTh, bright, bright, bright,
               u0, v0, u1, v0, u1, v1, u0, v1);
  for (int i = 0; i < 16; ++i) {
    const float f = (static_cast<float>(i) + 0.5F) / 16.0F;
    const float uu = u0 + (u1 - u0) * f;
    const float xx = f;
    emit_quad_uv(m, t, xx, 0, -kTh, xx, 0, 0, xx, 1, 0, xx, 1, -kTh, bright, bright, bright,
                 uu, v1, uu, v1, uu, v0, uu, v0);
    emit_quad_uv(m, t, xx + 1.0F / 16.0F, 1, -kTh, xx + 1.0F / 16.0F, 1, 0, xx + 1.0F / 16.0F,
                 0, 0, xx + 1.0F / 16.0F, 0, -kTh, bright, bright, bright, uu, v0, uu, v0, uu,
                 v1, uu, v1);
    const float vv = v1 + (v0 - v1) * f;
    const float yy = f + 1.0F / 16.0F;
    emit_quad_uv(m, t, 0, yy, 0, 1, yy, 0, 1, yy, -kTh, 0, yy, -kTh, bright, bright, bright,
                 u0, vv, u1, vv, u1, vv, u0, vv);
    emit_quad_uv(m, t, 1, f, 0, 0, f, 0, 0, f, -kTh, 1, f, -kTh, bright, bright, bright, u1,
                 vv, u0, vv, u0, vv, u1, vv);
  }
}

}  // namespace craftpp::render::item_mesh
