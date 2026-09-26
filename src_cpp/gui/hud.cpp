#include "gui/hud.hpp"

namespace craftpp::gui {

namespace {

void blit(render::Mesh& m, float x, float y, float u, float v, float w, float h) {
  const std::uint32_t base = static_cast<std::uint32_t>(m.vertices.size());
  const float uu0 = u / 256.0F, vv0 = v / 256.0F;
  const float uu1 = (u + w) / 256.0F, vv1 = (v + h) / 256.0F;
  m.vertices.push_back({x, y + h, 0, 1, 1, 1, uu0, vv1});
  m.vertices.push_back({x + w, y + h, 0, 1, 1, 1, uu1, vv1});
  m.vertices.push_back({x + w, y, 0, 1, 1, 1, uu1, vv0});
  m.vertices.push_back({x, y, 0, 1, 1, 1, uu0, vv0});
  m.indices.insert(m.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
}

int xp_cap(int level) { return 7 + (level * 7 >> 1); }

}  // namespace

HudMeshes build_hud(const HudState& s, const Font& font) {
  HudMeshes out;
  const float w = static_cast<float>(s.width), h = static_cast<float>(s.height);

  // Hotbar (gui.png) + crosshair (icons.png): always drawn.
  blit(out.chrome, w / 2 - 91, h - 22, 0, 0, 182, 22);
  blit(out.chrome, w / 2 - 91 - 1 + s.current_item * 20, h - 22 - 1, 0, 22, 24, 24);
  blit(out.icons, w / 2 - 7, h / 2 - 7, 0, 0, 16, 16);

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
