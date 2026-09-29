#pragma once

#include <cmath>
#include <cstdint>
#include <string>

#include "gui/font.hpp"
#include "gui/item_icons.hpp"
#include "render/mesh.hpp"
#include "world/blocks.hpp"

namespace craftpp::gui::paint {

// Shared 2D item painters (moved verbatim from hud.cpp): hotbar and the
// container screens paint slots through these so icons/counts/bars match.

inline void blit(render::Mesh& m, float x, float y, float u, float v, float w, float h) {
  const std::uint32_t base = static_cast<std::uint32_t>(m.vertices.size());
  const float uu0 = u / 256.0F, vv0 = v / 256.0F;
  const float uu1 = (u + w) / 256.0F, vv1 = (v + h) / 256.0F;
  m.vertices.push_back({x, y + h, 0, 1, 1, 1, uu0, vv1});
  m.vertices.push_back({x + w, y + h, 0, 1, 1, 1, uu1, vv1});
  m.vertices.push_back({x + w, y, 0, 1, 1, 1, uu1, vv0});
  m.vertices.push_back({x, y, 0, 1, 1, 1, uu0, vv0});
  m.indices.insert(m.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
}

// Solid-color quad for damage bars (flat shader path, no texture).
inline void solid(render::Mesh& m, float x, float y, float w, float h, float r, float g,
                  float b) {
  const std::uint32_t base = static_cast<std::uint32_t>(m.vertices.size());
  m.vertices.push_back({x, y + h, 0, r, g, b, 0, 0});
  m.vertices.push_back({x + w, y + h, 0, r, g, b, 0, 0});
  m.vertices.push_back({x + w, y, 0, r, g, b, 0, 0});
  m.vertices.push_back({x, y, 0, r, g, b, 0, 0});
  m.indices.insert(m.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
}

// Isometric inventory cube (drawItemIntoGui transform chain, baked on
// CPU): T(slot) S(10) T(1,0.5,1) Sz(-1) Rx(210) Ry(45) Ry(-90) over the
// unit cube, fitted into the 16x16 slot. Faces/textures/shades mirror
// renderBlockOnInventory type 0 (grass untinted, leaves pine/birch).
inline void build_item_cube(render::Mesh& m, float sx, float sy, int id, int meta) {
  constexpr float kD = 3.14159265F / 180.0F;
  const float c1 = std::cos(-90.0F * kD), s1 = std::sin(-90.0F * kD);
  const float c2 = std::cos(45.0F * kD), s2 = std::sin(45.0F * kD);
  const float c3 = std::cos(210.0F * kD), s3 = std::sin(210.0F * kD);
  struct P {
    float x, y, z;
  };
  auto xform = [&](float x, float y, float z) {
    // Ry(-90), Ry(45), Rx(210), Sz(-1), T(1,0.5,1), S(10).
    float x1 = x * c1 + z * s1, z1 = -x * s1 + z * c1;
    float x2 = x1 * c2 + z1 * s2, z2 = -x1 * s2 + z1 * c2;
    float y3 = y * c3 - z2 * s3, z3 = y * s3 + z2 * c3;
    return P{(x2 + 1) * 10.0F, (y3 + 0.5F) * 10.0F, (-z3 + 1) * 10.0F};
  };
  // Fit the transformed cube into the slot (screen y grows downward).
  P corners[8];
  int k = 0;
  for (int ix = 0; ix < 2; ++ix)
    for (int iy = 0; iy < 2; ++iy)
      for (int iz = 0; iz < 2; ++iz) corners[k++] = xform(ix, iy, iz);
  float mnx = 1e9F, mxx = -1e9F, mny = 1e9F, mxy = -1e9F, mnz = 1e9F, mxz = -1e9F;
  for (const P& c : corners) {
    mnx = std::min(mnx, c.x);
    mxx = std::max(mxx, c.x);
    mny = std::min(mny, c.y);
    mxy = std::max(mxy, c.y);
    mnz = std::min(mnz, c.z);
    mxz = std::max(mxz, c.z);
  }
  auto fit = [&](P p) {
    // Vanilla GUI space has +y pointing DOWN (glOrtho(0,w,h,0)): larger
    // transformed y sits lower in the slot, no flip.
    const float fx = sx + (p.x - mnx) / (mxx - mnx) * 16.0F;
    const float fy = sy + (p.y - mny) / (mxy - mny) * 16.0F;
    const float fz = (p.z - mnz) / (mxz - mnz) - 0.5F;
    return P{fx, fy, fz};
  };
  // (corners, side, shade) for the 6 faces.
  struct Face {
    int side;
    float shade;
    float v[4][3];
  };
  const Face ff[6] = {
      {0, 0.5F, {{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}}},
      {1, 1.0F, {{0, 1, 0}, {1, 1, 0}, {1, 1, 1}, {0, 1, 1}}},
      {2, 0.8F, {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}}},
      {3, 0.8F, {{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}}},
      {4, 0.6F, {{0, 0, 0}, {0, 0, 1}, {0, 1, 1}, {0, 1, 0}}},
      {5, 0.6F, {{1, 0, 0}, {1, 0, 1}, {1, 1, 1}, {1, 1, 0}}},
  };
  float tint = 1.0F, tint_g = 1.0F, tint_b = 1.0F;
  if (id == 18) {
    if ((meta & 3) == 1) {
      tint = 0x61 / 255.0F;
      tint_g = 0x99 / 255.0F;
      tint_b = 0x41 / 255.0F;
    } else if ((meta & 3) == 2) {
      tint = 0x80 / 255.0F;
      tint_g = 0xA7 / 255.0F;
      tint_b = 0x25 / 255.0F;
    }
  }
  for (const Face& f : ff) {
    const int tile = world::bid::block_texture(id, f.side, meta);
    const float tx = static_cast<float>((tile & 15) * 16);
    const float ty = static_cast<float>(tile & 240);
    const float u0 = tx / 256.0F, u1 = (tx + 16.0F - 0.01F) / 256.0F;
    const float v0 = ty / 256.0F, v1 = (ty + 16.0F - 0.01F) / 256.0F;
    // Winding/UVs follow the mesher cube faces (CCW front).
    const float qu[4] = {u0, u1, u1, u0};
    const float qv[4] = {v1, v1, v0, v0};
    const std::uint32_t base = static_cast<std::uint32_t>(m.vertices.size());
    for (int i = 0; i < 4; ++i) {
      P p = fit(xform(f.v[i][0], f.v[i][1], f.v[i][2]));
      m.vertices.push_back(
          {p.x, p.y, p.z, f.shade * tint, f.shade * tint_g, f.shade * tint_b, qu[i], qv[i]});
    }
    m.indices.insert(m.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
  }
}

// 16x16 sprite from a 256x256 atlas tile.
inline void sprite(render::Mesh& m, float x, float y, int tile) {
  const float tx = static_cast<float>((tile & 15) * 16);
  const float ty = static_cast<float>(tile & 240);
  blit(m, x, y, tx, ty, 16, 16);
}

struct PaintOut {
  render::Mesh* items = nullptr;   // gui/items.png layer
  render::Mesh* blocks = nullptr;  // terrain.png layer (cubes + flat sprites)
  render::Mesh* shadow = nullptr;  // font shadow layer (counts)
  render::Mesh* text = nullptr;    // font layer (counts)
  render::Mesh* bars = nullptr;    // flat bars layer (durability)
};

// One slot stack: icon/cube + count + damage bar (renderItemIntoGUI +
// renderItemOverlayIntoGUI parity). Meshes may be null to skip a layer.
inline void paint_stack(PaintOut o, const Font& font, float x, float y, int id, int count,
                        int damage, int max_damage) {
  if (id == 0 || count <= 0) return;
  if (id < 256) {
    if (world::bid::render_type(id) == 0) {
      if (o.blocks != nullptr) build_item_cube(*o.blocks, x, y, id, damage);
    } else {
      if (o.blocks != nullptr) sprite(*o.blocks, x, y, world::bid::block_texture(id, 2, damage));
    }
  } else {
    const int icon = item_sprite_index(id);
    if (icon >= 0 && o.items != nullptr) sprite(*o.items, x, y, icon);
  }
  if (count > 1 && o.shadow != nullptr && o.text != nullptr) {
    const std::string n = std::to_string(count);
    const float tx = x + 19 - 2 - font.string_width(n);
    const float ty = y + 6 + 3;
    auto append = [](render::Mesh& dst, render::Mesh&& src) {
      const auto base = static_cast<std::uint32_t>(dst.vertices.size());
      dst.vertices.insert(dst.vertices.end(), src.vertices.begin(), src.vertices.end());
      for (auto ix : src.indices) dst.indices.push_back(base + ix);
    };
    append(*o.shadow, font.build_text(n, tx + 1, ty + 1, 0xFFFFFFFF, true));
    append(*o.text, font.build_text(n, tx, ty, 0xFFFFFFFF, false));
  }
  if (max_damage > 0 && damage > 0 && o.bars != nullptr) {
    const int bar_w =
        static_cast<int>(std::round(13.0 - static_cast<double>(damage) * 13.0 / max_damage));
    const int col =
        static_cast<int>(std::round(255.0 - static_cast<double>(damage) * 255.0 / max_damage));
    const float fr = ((255 - col) >> 0 & 255) / 255.0F;  // (255-c)<<16 | c<<8
    const float fg = (col & 255) / 255.0F;
    const float br = (((255 - col) / 4) & 255) / 255.0F;  // (255-c)/4<<16 | 64
    solid(*o.bars, x + 2, y + 13, 13, 2, 0, 0, 0);
    solid(*o.bars, x + 2, y + 13, 12, 1, br, 63.0F / 255.0F, 0);
    solid(*o.bars, x + 2, y + 13, bar_w, 1, fr, fg, 0);
  }
}

}  // namespace craftpp::gui::paint
