#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

#include "gui/item_icons.hpp"
#include "render/mesh.hpp"
#include "render/model.hpp"
#include "world/blocks.hpp"

namespace craftpp::render {

// First-person hand + held item (ItemRenderer.renderItemInFirstPerson port,
// minus maps/eat/drink/block/bow use-poses which need the item-use state
// machine (M5 leftover), and minus the renderArm view-lag micro-rotation).
// Chains run in camera space; brightness is forced fullbright like the
// source (var6 = 1.0F). Meshes split by texture: atlas (block items),
// items (flat icons), skin (arm).
struct FirstPersonMeshes {
  Mesh atlas;
  Mesh items;
  Mesh skin;
};

namespace first_person_detail {

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

// Full unit cube centered on the origin (renderBlockOnInventory shape).
inline void emit_unit_cube(Mesh& m, const Mat4& t, int id, int damage, float bright) {
  struct Face {
    float c[4][3];
    int side;
  };
  const float h = 0.5F;
  const Face faces[6] = {
      {{{-h, -h, h}, {h, -h, h}, {h, -h, -h}, {-h, -h, -h}}, 0},
      {{{-h, h, -h}, {h, h, -h}, {h, h, h}, {-h, h, h}}, 1},
      {{{-h, h, -h}, {-h, -h, -h}, {h, -h, -h}, {h, h, -h}}, 2},
      {{{h, h, h}, {h, -h, h}, {-h, -h, h}, {-h, h, h}}, 3},
      {{{-h, h, h}, {-h, h, -h}, {-h, -h, -h}, {-h, -h, h}}, 4},
      {{{h, -h, h}, {h, -h, -h}, {h, h, -h}, {h, h, h}}, 5},
  };
  for (const Face& f : faces) {
    const int tile = world::bid::block_texture(id, f.side, damage);
    const float tx = static_cast<float>((tile & 15) * 16);
    const float ty = static_cast<float>(tile & 240);
    const float u0 = tx / 256.0F, u1 = (tx + 16.0F - 0.01F) / 256.0F;
    const float v0 = ty / 256.0F, v1 = (ty + 16.0F - 0.01F) / 256.0F;
    // Mesher UV assignment per corner order.
    emit_quad_uv(m, t, f.c[0][0], f.c[0][1], f.c[0][2], f.c[1][0], f.c[1][1], f.c[1][2], f.c[2][0],
                 f.c[2][1], f.c[2][2], f.c[3][0], f.c[3][1], f.c[3][2], bright, bright, bright,
                 u0, v1, u0, v0, u1, v0, u1, v1);
  }
}

}  // namespace first_person_detail

// held_id <= 0: empty hand (arm only). swing/equip in 0..1 (already
// partial-interpolated by the caller).
inline void build_first_person(FirstPersonMeshes& out, int held_id, int held_damage, float equip,
                               float swing) {
  using namespace first_person_detail;
  const float sswing = std::sin(swing * 3.14159265F);
  const float sswing_sqrt = std::sin(std::sqrt(std::max(swing, 0.0F)) * 3.14159265F);
  const float sswing_sq = std::sin(swing * swing * 3.14159265F);
  if (held_id > 0) {
    Mat4 t;
    t.translate(-sswing_sqrt * 0.4F,
                std::sin(std::sqrt(std::max(swing, 0.0F)) * 3.14159265F * 2.0F) * 0.2F,
                -sswing * 0.2F);
    t.translate(0.7F * 0.8F, -0.65F * 0.8F - (1.0F - equip) * 0.6F, -0.9F * 0.8F);
    t.rotate(45.0F, 0.0F, 1.0F, 0.0F);
    t.rotate(-sswing_sq * 20.0F, 0.0F, 1.0F, 0.0F);
    t.rotate(-sswing_sqrt * 20.0F, 0.0F, 0.0F, 1.0F);
    t.rotate(-sswing_sqrt * 80.0F, 1.0F, 0.0F, 0.0F);
    t.scale(0.4F, 0.4F, 0.4F);
    if (held_id == 346) t.rotate(180.0F, 0.0F, 1.0F, 0.0F);  // fishing rod
    const bool is_block = held_id < 256;
    const int rt = is_block ? world::bid::render_type(held_id) : -1;
    const bool as_cube =
        is_block && (rt == 0 || rt == 10 || rt == 11 || rt == 13 || rt == 16 || rt == 21 ||
                     rt == 22 || rt == 27);
    if (as_cube) {
      emit_unit_cube(out.atlas, t, held_id, held_damage, 1.0F);
    } else {
      int tile = 0;
      Mesh* mesh = &out.items;
      if (is_block) {
        tile = world::bid::block_texture(held_id, 2, held_damage);
        mesh = &out.atlas;
      } else {
        tile = craftpp::gui::item_sprite_index(held_id);
        if (tile < 0) return;
      }
      const float tx = static_cast<float>((tile & 15) * 16);
      const float ty = static_cast<float>(tile & 240);
      const float u0 = tx / 256.0F, u1 = (tx + 16.0F) / 256.0F;
      const float v0 = ty / 256.0F, v1 = (ty + 16.0F) / 256.0F;
      // Double-sided icon quad (fixed orientation like renderItem).
      emit_quad_uv(*mesh, t, -0.5F, -0.5F, 0, 0.5F, -0.5F, 0, 0.5F, 0.5F, 0, -0.5F, 0.5F, 0,
                   1.0F, 1.0F, 1.0F, u0, v1, u1, v1, u1, v0, u0, v0);
      emit_quad_uv(*mesh, t, -0.5F, 0.5F, 0, 0.5F, 0.5F, 0, 0.5F, -0.5F, 0, -0.5F, -0.5F, 0,
                   1.0F, 1.0F, 1.0F, u0, v0, u1, v0, u1, v1, u0, v1);
    }
    return;
  }
  // Empty hand: swing bob + arm (drawFirstPersonHand, straight pose).
  Mat4 t;
  t.translate(-sswing_sqrt * 0.3F,
              std::sin(std::sqrt(std::max(swing, 0.0F)) * 3.14159265F * 2.0F) * 0.4F,
              -sswing * 0.4F);
  t.translate(0.8F * 0.8F, -(12.0F / 16.0F) * 0.8F - (1.0F - equip) * 0.6F, -0.9F * 0.8F);
  t.rotate(45.0F, 0.0F, 1.0F, 0.0F);
  t.rotate(sswing_sqrt * 70.0F, 0.0F, 1.0F, 0.0F);
  t.rotate(-sswing_sq * 20.0F, 0.0F, 0.0F, 1.0F);
  t.translate(-1.0F, 3.6F, 3.5F);
  t.rotate(120.0F, 0.0F, 0.0F, 1.0F);
  t.rotate(200.0F, 1.0F, 0.0F, 0.0F);
  t.rotate(-135.0F, 0.0F, 1.0F, 0.0F);
  t.translate(5.6F, 0.0F, 0.0F);
  // Arm box (char 40,16) at pivot (-5,2,0), straight (onGround=0 pose).
  ModelPart arm;
  arm.px = -5.0F;
  arm.py = 2.0F;
  arm.boxes.push_back(ModelBox{40, 16, -3, -2, -2, 4, 12, 4});
  const Mesh arm_mesh = build_model({arm}, 64, 32, 1.0F);
  Mat4 arm_t = t;
  arm_t.scale(1.0F / 16.0F, 1.0F / 16.0F, 1.0F / 16.0F);
  for (const Vertex& v : arm_mesh.vertices) {
    const float* mm = arm_t.m;
    out.skin.vertices.push_back(
        {mm[0] * v.x + mm[4] * v.y + mm[8] * v.z + mm[12],
         mm[1] * v.x + mm[5] * v.y + mm[9] * v.z + mm[13],
         mm[2] * v.x + mm[6] * v.y + mm[10] * v.z + mm[14], v.r, v.g, v.b, v.u, v.v});
  }
  for (std::uint32_t i : arm_mesh.indices) out.skin.indices.push_back(i);
}

}  // namespace craftpp::render
