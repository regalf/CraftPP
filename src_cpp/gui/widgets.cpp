#include "gui/widgets.hpp"

#include <algorithm>

namespace craftpp::gui {

void blit_256(render::Mesh& m, float x, float y, float u, float v, float w, float h) {
  const std::uint32_t base = static_cast<std::uint32_t>(m.vertices.size());
  const float uu0 = u / 256.0F, vv0 = v / 256.0F;
  const float uu1 = (u + w) / 256.0F, vv1 = (v + h) / 256.0F;
  m.vertices.push_back({x, y + h, 0, 1, 1, 1, uu0, vv1});
  m.vertices.push_back({x + w, y + h, 0, 1, 1, 1, uu1, vv1});
  m.vertices.push_back({x + w, y, 0, 1, 1, 1, uu1, vv0});
  m.vertices.push_back({x, y, 0, 1, 1, 1, uu0, vv0});
  m.indices.insert(m.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
}

void draw_button(render::Mesh& out_chrome, const Button& b, bool hovered, const Font& font,
                 render::Mesh& out_shadow, render::Mesh& out_text) {
  if (!b.visible) return;
  const int state = !b.enabled ? 0 : (hovered ? 2 : 1);
  const float v = 46.0F + state * 20.0F;
  const float half = b.w / 2.0F;
  blit_256(out_chrome, b.x, b.y, 0, v, half, b.h);
  blit_256(out_chrome, b.x + half, b.y, 200 - half, v, half, b.h);
  const std::uint32_t color =
      !b.enabled ? 0xFFA0A0A0 : (hovered ? 0xFFFFFFA0 : 0xFFE0E0E0);
  const float cx = b.x + b.w / 2.0F - font.string_width(b.label) / 2.0F;
  const float cy = b.y + (b.h - 8) / 2.0F;
  auto sh = font.build_text(b.label, cx + 1, cy + 1, color, true);
  auto fg = font.build_text(b.label, cx, cy, color, false);
  auto base = static_cast<std::uint32_t>(out_shadow.vertices.size());
  out_shadow.vertices.insert(out_shadow.vertices.end(), sh.vertices.begin(), sh.vertices.end());
  for (auto ix : sh.indices) out_shadow.indices.push_back(base + ix);
  base = static_cast<std::uint32_t>(out_text.vertices.size());
  out_text.vertices.insert(out_text.vertices.end(), fg.vertices.begin(), fg.vertices.end());
  for (auto ix : fg.indices) out_text.indices.push_back(base + ix);
}

void draw_background(render::Mesh& out, int width, int height) {
  // background.png tiled at 32px with the 0x404040 tint. Our textures clamp,
  // so tile with one quad per cell (UV 0..1 each) instead of UV repeats.
  const float g = 0x40 / 255.0F;
  for (int ty = 0; ty * 32 < height; ty += 1) {
    for (int tx = 0; tx * 32 < width; tx += 1) {
      const float x0 = tx * 32.0F, y0 = ty * 32.0F;
      const float x1 = std::min(x0 + 32.0F, (float)width);
      const float y1 = std::min(y0 + 32.0F, (float)height);
      const float u1 = (x1 - x0) / 32.0F, v1 = (y1 - y0) / 32.0F;
      const std::uint32_t base = static_cast<std::uint32_t>(out.vertices.size());
      out.vertices.push_back({x0, y1, 0, g, g, g, 0, v1});
      out.vertices.push_back({x1, y1, 0, g, g, g, u1, v1});
      out.vertices.push_back({x1, y0, 0, g, g, g, u1, 0});
      out.vertices.push_back({x0, y0, 0, g, g, g, 0, 0});
      out.indices.insert(out.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    }
  }
}

void draw_rect(render::Mesh& out, float x0, float y0, float x1, float y1, std::uint32_t argb) {
  const float a = ((argb >> 24) & 255) / 255.0F;
  const float r = ((argb >> 16) & 255) / 255.0F;
  const float g = ((argb >> 8) & 255) / 255.0F;
  const float b = (argb & 255) / 255.0F;
  const std::uint32_t base = static_cast<std::uint32_t>(out.vertices.size());
  out.vertices.push_back({x0, y1, 0, r, g, b, 0, 0});
  out.vertices.push_back({x1, y1, 0, r, g, b, 0, 0});
  out.vertices.push_back({x1, y0, 0, r, g, b, 0, 0});
  out.vertices.push_back({x0, y0, 0, r, g, b, 0, 0});
  out.indices.insert(out.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
  (void)a;  // alpha needs blending, enabled by the caller for 2D tint meshes
}

void draw_centered(const Font& font, render::Mesh& out_shadow, render::Mesh& out_text,
                   const std::string& s, float cx, float y, std::uint32_t argb) {
  const float x = cx - font.string_width(s) / 2.0F;
  auto sh = font.build_text(s, x + 1, y + 1, argb, true);
  auto fg = font.build_text(s, x, y, argb, false);
  auto base = static_cast<std::uint32_t>(out_shadow.vertices.size());
  out_shadow.vertices.insert(out_shadow.vertices.end(), sh.vertices.begin(), sh.vertices.end());
  for (auto ix : sh.indices) out_shadow.indices.push_back(base + ix);
  base = static_cast<std::uint32_t>(out_text.vertices.size());
  out_text.vertices.insert(out_text.vertices.end(), fg.vertices.begin(), fg.vertices.end());
  for (auto ix : fg.indices) out_text.indices.push_back(base + ix);
}

}  // namespace craftpp::gui
