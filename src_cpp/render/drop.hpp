#pragma once

#include <cmath>

#include "core/random.hpp"
#include "gui/item_icons.hpp"
#include "render/mesh.hpp"
#include "world/blocks.hpp"

namespace craftpp::render {

// World-space EntityItem renderer (RenderItem.doRenderItem port): bobbing +
// spinning textured drops. Blocks in the renderItemIn3d set draw as 0.25
// mini-cubes (atlas); everything else draws as a view-billboarded icon
// quad (items.png, terrain tile for flat block items like the HUD). Stack
// layers (1/2/3/4) use a reseeded JavaRandom(187) like the source.
struct DropMeshes {
  Mesh atlas;  // terrain.png mini-cubes
  Mesh items;  // items.png / terrain flat sprites
};

inline void build_drop(DropMeshes& out, int item_id, int damage, int count, float x, float y,
                       float z, float age_partial, float hover, float view_yaw_deg,
                       float brightness) {
  const int layers = count > 20 ? 4 : (count > 5 ? 3 : (count > 1 ? 2 : 1));
  const float bob =
      static_cast<float>(std::sin(age_partial / 10.0 + hover)) * 0.1F + 0.1F;
  const float cy = y + bob;
  craftpp::JavaRandom lr(187L);
  auto rot_y = [](float lx, float lz, float deg, float& ox, float& oz) {
    const float r = deg * 3.14159265F / 180.0F;
    const float c = std::cos(r), s = std::sin(r);
    ox = lx * c + lz * s;
    oz = -lx * s + lz * c;
  };
  auto quad = [](Mesh& m, float x0, float y0, float z0, float x1, float y1, float z1, float x2,
                 float y2, float z2, float x3, float y3, float z3, float r, float g, float b,
                 float u0, float v0, float u1, float v1, float u2, float v2, float u3, float v3) {
    const std::uint32_t base = static_cast<std::uint32_t>(m.vertices.size());
    m.vertices.push_back({x0, y0, z0, r, g, b, u0, v0});
    m.vertices.push_back({x1, y1, z1, r, g, b, u1, v1});
    m.vertices.push_back({x2, y2, z2, r, g, b, u2, v2});
    m.vertices.push_back({x3, y3, z3, r, g, b, u3, v3});
    m.indices.insert(m.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
  };

  const bool is_block = item_id < 256 && item_id > 0;
  const int rt = is_block ? world::bid::render_type(item_id) : -1;
  const bool as_cube =
      is_block && world::bid::render_item_in_3d(rt);  // renderItemIn3d
  if (as_cube) {
    // Mini-cube 0.25, spun around Y (vanilla scale + glRotatef(spin)).
    const float spin = (age_partial / 20.0F + hover) * (180.0F / 3.14159265F);
    const float h = 0.125F;
    // Faces in mesher corner order (CCW front) with mesher UV assignment.
    struct Face {
      float c[4][3];
      float uv[4][2];
      float shade;
      int side;
    };
    const Face faces[6] = {
        {{{-h, -h, h}, {-h, -h, -h}, {h, -h, -h}, {h, -h, h}},
         {{0, 1}, {0, 0}, {1, 0}, {1, 1}},
         0.5F,
         0},
        {{{h, h, h}, {h, h, -h}, {-h, h, -h}, {-h, h, h}},
         {{0, 1}, {0, 0}, {1, 0}, {1, 1}},
         1.0F,
         1},
        {{{-h, h, -h}, {h, h, -h}, {h, -h, -h}, {-h, -h, -h}},
         {{0, 1}, {0, 0}, {1, 0}, {1, 1}},
         0.8F,
         2},
        {{{-h, h, h}, {-h, -h, h}, {h, -h, h}, {h, h, h}},
         {{0, 1}, {0, 0}, {1, 0}, {1, 1}},
         0.8F,
         3},
        {{{-h, h, h}, {-h, h, -h}, {-h, -h, -h}, {-h, -h, h}},
         {{0, 1}, {0, 0}, {1, 0}, {1, 1}},
         0.6F,
         4},
        {{{h, -h, h}, {h, -h, -h}, {h, h, -h}, {h, h, h}},
         {{0, 1}, {0, 0}, {1, 0}, {1, 1}},
         0.6F,
         5},
    };
    for (int l = 0; l < layers; ++l) {
      float ox = 0, oy = 0, oz = 0;
      if (l > 0) {
        ox = (lr.next_float() * 2.0F - 1.0F) * 0.2F;
        oy = (lr.next_float() * 2.0F - 1.0F) * 0.2F;
        oz = (lr.next_float() * 2.0F - 1.0F) * 0.2F;
      }
      for (const Face& f : faces) {
        const int tile = world::bid::block_texture(item_id, f.side, damage);
        const float tx = static_cast<float>((tile & 15) * 16);
        const float ty = static_cast<float>(tile & 240);
        const float u0 = tx / 256.0F, u1 = (tx + 16.0F - 0.01F) / 256.0F;
        const float v0 = ty / 256.0F, v1 = (ty + 16.0F - 0.01F) / 256.0F;
        const float sh = brightness * f.shade;
        float px[4], py[4], pz[4], qu[4], qv[4];
        for (int i = 0; i < 4; ++i) {
          rot_y(f.c[i][0], f.c[i][2], spin, px[i], pz[i]);
          py[i] = f.c[i][1];
          qu[i] = f.uv[i][0] == 0.0F ? u0 : u1;
          qv[i] = f.uv[i][1] == 0.0F ? v0 : v1;
        }
        quad(out.atlas, x + ox + px[0], cy + oy + py[0], z + oz + pz[0], x + ox + px[1],
             cy + oy + py[1], z + oz + pz[1], x + ox + px[2], cy + oy + py[2], z + oz + pz[2],
             x + ox + px[3], cy + oy + py[3], z + oz + pz[3], sh, sh, sh, qu[0], qv[0], qu[1],
             qv[1], qu[2], qv[2], qu[3], qv[3]);
      }
    }
    return;
  }
  // Flat icon billboard (no spin, faces the player like the source).
  int tile = 0;
  Mesh* mesh = &out.items;
  if (is_block) {
    tile = world::bid::block_texture(item_id, 2, damage);
    mesh = &out.atlas;
  } else {
    tile = craftpp::gui::item_sprite_index(item_id);
    if (tile < 0) return;
  }
  const float tx = static_cast<float>((tile & 15) * 16);
  const float ty = static_cast<float>(tile & 240);
  const float u0 = tx / 256.0F, u1 = (tx + 16.0F) / 256.0F;
  const float v0 = ty / 256.0F, v1 = (ty + 16.0F) / 256.0F;
  const float bb = 180.0F - view_yaw_deg;
  // Quad (-0.5,-0.25)..(0.5,0.75) at 0.5 scale, exact-edge UVs.
  const float corners[4][2] = {{-0.25F, -0.125F}, {0.25F, -0.125F}, {0.25F, 0.375F}, {-0.25F, 0.375F}};
  for (int l = 0; l < layers; ++l) {
    float ox = 0, oy = 0, oz = 0;
    if (l > 0) {
      ox = (lr.next_float() * 2.0F - 1.0F) * 0.15F;
      oy = (lr.next_float() * 2.0F - 1.0F) * 0.15F;
      oz = (lr.next_float() * 2.0F - 1.0F) * 0.15F;
    }
    float px[4], pz[4];
    for (int i = 0; i < 4; ++i) rot_y(corners[i][0], 0.0F, bb, px[i], pz[i]);
    quad(*mesh, x + ox + px[0], cy + oy + corners[0][1], z + oz + pz[0], x + ox + px[1],
         cy + oy + corners[1][1], z + oz + pz[1], x + ox + px[2], cy + oy + corners[2][1],
         z + oz + pz[2], x + ox + px[3], cy + oy + corners[3][1], z + oz + pz[3], brightness,
         brightness, brightness, u0, v1, u1, v1, u1, v0, u0, v0);
  }
}

}  // namespace craftpp::render
