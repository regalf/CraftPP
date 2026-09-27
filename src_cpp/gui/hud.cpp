#include "gui/hud.hpp"

#include <cmath>

#include "gui/item_icons.hpp"
#include "world/blocks.hpp"

namespace craftpp::gui {

namespace {

void blit(render::Mesh& m, float x, float y, float u, float v, float w, float h) {  const std::uint32_t base = static_cast<std::uint32_t>(m.vertices.size());
  const float uu0 = u / 256.0F, vv0 = v / 256.0F;
  const float uu1 = (u + w) / 256.0F, vv1 = (v + h) / 256.0F;
  m.vertices.push_back({x, y + h, 0, 1, 1, 1, uu0, vv1});
  m.vertices.push_back({x + w, y + h, 0, 1, 1, 1, uu1, vv1});
  m.vertices.push_back({x + w, y, 0, 1, 1, 1, uu1, vv0});
  m.vertices.push_back({x, y, 0, 1, 1, 1, uu0, vv0});
  m.indices.insert(m.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
}

int xp_cap(int level) { return 7 + (level * 7 >> 1); }

// Solid-color quad for damage bars (flat shader path, no texture).
void solid(render::Mesh& m, float x, float y, float w, float h, float r, float g, float b) {
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
void build_item_cube(render::Mesh& m, float sx, float sy, int id, int meta) {
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
    const float fx = sx + (p.x - mnx) / (mxx - mnx) * 16.0F;
    const float fy = sy + 16.0F - (p.y - mny) / (mxy - mny) * 16.0F;
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
void sprite(render::Mesh& m, float x, float y, int tile) {  const float tx = static_cast<float>((tile & 15) * 16);
  const float ty = static_cast<float>(tile & 240);
  blit(m, x, y, tx, ty, 16, 16);
}

}  // namespace

HudMeshes build_hud(const HudState& s, const Font& font) {
  HudMeshes out;
  const float w = static_cast<float>(s.width), h = static_cast<float>(s.height);

  // Hotbar (gui.png) + crosshair (icons.png): always drawn.
  blit(out.chrome, w / 2 - 91, h - 22, 0, 0, 182, 22);
  blit(out.chrome, w / 2 - 91 - 1 + s.current_item * 20, h - 22 - 1, 0, 22, 24, 24);
  blit(out.icons, w / 2 - 7, h / 2 - 7, 0, 0, 16, 16);

  // Hotbar items (renderInventorySlot x3 + renderItemOverlayIntoGUI).
  for (int i = 0; i < 9; ++i) {
    const HudSlot& sl = s.hotbar[i];
    if (sl.id == 0 || sl.count <= 0) continue;
    const float x = w / 2 - 90 + i * 20 + 2;
    const float y = h - 16 - 3;
    if (sl.id < 256) {
      if (world::bid::render_type(sl.id) == 0) {
        // renderBlockOnInventory: real isometric cube (top + 6 faces,
        // depth-tested) with the drawItemIntoGui transform chain.
        build_item_cube(out.blocks, x, y, sl.id, sl.damage);
      } else {
        // Flat terrain sprite interim (stairs/fence/cactus/chest/...).
        sprite(out.blocks, x, y, world::bid::block_texture(sl.id, 2, sl.damage));
      }
    } else {
      const int icon = item_sprite_index(sl.id);
      if (icon >= 0) sprite(out.items, x, y, icon);
    }
    if (sl.count > 1) {
      const std::string n = std::to_string(sl.count);
      const float tx = x + 19 - 2 - font.string_width(n);
      const float ty = y + 6 + 3;
      auto sh = font.build_text(n, tx + 1, ty + 1, 0xFFFFFFFF, true);
      auto fg = font.build_text(n, tx, ty, 0xFFFFFFFF, false);
      auto base = static_cast<std::uint32_t>(out.shadow.vertices.size());
      out.shadow.vertices.insert(out.shadow.vertices.end(), sh.vertices.begin(), sh.vertices.end());
      for (auto ix : sh.indices) out.shadow.indices.push_back(base + ix);
      base = static_cast<std::uint32_t>(out.text.vertices.size());
      out.text.vertices.insert(out.text.vertices.end(), fg.vertices.begin(), fg.vertices.end());
      for (auto ix : fg.indices) out.text.indices.push_back(base + ix);
    }
    if (sl.max_damage > 0 && sl.damage > 0) {
      const int bar_w = static_cast<int>(
          std::round(13.0 - static_cast<double>(sl.damage) * 13.0 / sl.max_damage));
      const int col = static_cast<int>(
          std::round(255.0 - static_cast<double>(sl.damage) * 255.0 / sl.max_damage));
      const float fr = ((255 - col) >> 0 & 255) / 255.0F;  // (255-c)<<16 | c<<8
      const float fg = (col & 255) / 255.0F;
      const float br = (((255 - col) / 4) & 255) / 255.0F;  // (255-c)/4<<16 | 64
      solid(out.bars, x + 2, y + 13, 13, 2, 0, 0, 0);
      solid(out.bars, x + 2, y + 13, 12, 1, br, 63.0F / 255.0F, 0);
      solid(out.bars, x + 2, y + 13, bar_w, 1, fr, fg, 0);
    }
  }

  if (!s.survival_hud) return out;

  // XP bar + level.
  const int cap = xp_cap(s.xp_level);
  if (cap > 0) {
    const float bw = s.xp_frac * 183.0F;
    blit(out.icons, w / 2 - 91, h - 32 + 3, 0, 64, 182, 5);
    if (bw > 0) blit(out.icons, w / 2 - 91, h - 32 + 3, 0, 69, bw, 5);
  }
  if (s.xp_level > 0) {
    const std::string lv = std::to_string(s.xp_level);
    const float lx = (w - font.string_width(lv)) / 2;
    const float ly = h - 31 - 4;
    auto dark = font.build_text(lv, lx + 1, ly, 0xFF000000, false);
    auto dark2 = font.build_text(lv, lx - 1, ly, 0xFF000000, false);
    auto dark3 = font.build_text(lv, lx, ly + 1, 0xFF000000, false);
    auto dark4 = font.build_text(lv, lx, ly - 1, 0xFF000000, false);
    for (const auto* d : {&dark, &dark2, &dark3, &dark4}) {
      const auto base = static_cast<std::uint32_t>(out.shadow.vertices.size());
      out.shadow.vertices.insert(out.shadow.vertices.end(), d->vertices.begin(), d->vertices.end());
      for (auto ix : d->indices) out.shadow.indices.push_back(base + ix);
    }
    auto main = font.build_text(lv, lx, ly, 0xFF80FF20, false);
    const auto base = static_cast<std::uint32_t>(out.text.vertices.size());
    out.text.vertices.insert(out.text.vertices.end(), main.vertices.begin(), main.vertices.end());
    for (auto ix : main.indices) out.text.indices.push_back(base + ix);
  }

  const float bar_y = h - 39;       // hearts/food row
  const float upper_y = bar_y - 10;  // armor/air row
  // Armor.
  for (int i = 0; i < 10; ++i) {
    if (s.armor <= 0) break;
    const float x = w / 2 - 91 + i * 8;
    if (i * 2 + 1 < s.armor) {
      blit(out.icons, x, upper_y, 34, 9, 9, 9);
    } else if (i * 2 + 1 == s.armor) {
      blit(out.icons, x, upper_y, 25, 9, 9, 9);
    } else {
      blit(out.icons, x, upper_y, 16, 9, 9, 9);
    }
  }
  // Hearts (no damage-flash: prev == current in our sim).
  for (int i = 0; i < 10; ++i) {
    float x = w / 2 - 91 + i * 8;
    float y = bar_y;
    if (s.health <= 4) y += (s.tick * 7 + i * 13) % 2;  // low-hp jitter
    blit(out.icons, x, y, 16, 0, 9, 9);  // container
    if (i * 2 + 1 < s.health) {
      blit(out.icons, x, y, 52, 0, 9, 9);
    } else if (i * 2 + 1 == s.health) {
      blit(out.icons, x, y, 61, 0, 9, 9);
    }
  }
  // Food (mirrored from the right).
  for (int i = 0; i < 10; ++i) {
    float y = bar_y;
    if (s.saturation <= 0.0F && s.tick % (s.food * 3 + 1) == 0) {
      y += (s.tick * 3 + i * 11) % 3 - 1;
    }
    const float x = w / 2 + 91 - i * 8 - 9;
    blit(out.icons, x, y, 16, 27, 9, 9);  // container
    if (i * 2 + 1 < s.food) {
      blit(out.icons, x, y, 52, 27, 9, 9);
    } else if (i * 2 + 1 == s.food) {
      blit(out.icons, x, y, 61, 27, 9, 9);
    }
  }
  // Air bubbles.
  if (s.in_water) {
    const int full = (s.air - 2) * 10 / 300;
    const int popped = s.air * 10 / 300 - full;
    for (int i = 0; i < full + popped; ++i) {
      const float x = w / 2 + 91 - i * 8 - 9;
      blit(out.icons, x, upper_y, i < full ? 16 : 25, 18, 9, 9);
    }
  }
  return out;
}

}  // namespace craftpp::gui
