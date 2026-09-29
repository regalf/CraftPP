#pragma once

#include <map>
#include <string>

#include "entity/player.hpp"
#include "gui/container.hpp"
#include "gui/font.hpp"
#include "gui/item_names.hpp"
#include "gui/slot_paint.hpp"
#include "gui/widgets.hpp"
#include "render/mesh.hpp"

namespace craftpp::gui {

// Open-container overlay state (owned by the app; the world keeps ticking
// while open — doesGuiPauseGame=false parity). Drawn in the 2D pass like
// the HUD; clicks route to Container::click instead of mining.
struct OpenGui {
  enum class Kind { None, Inventory, Workbench, Furnace, Chest };
  Kind kind = Kind::None;
  ctn::Kit kit;
  int bx = 0, by = 0, bz = 0;  // block pos for bench/furnace/chest
  const void* tile = nullptr;  // expected tile ptr (furnace/chest validity)
  int x_size = 176, y_size = 166;
  float px = 0.0F, py = 0.0F;  // panel origin (recomputed per draw)
  // Furnace progress, refreshed from the tile each frame (12/24 scaled).
  int burn_scaled = 0, cook_scaled = 0;
};

inline bool gui_open(const OpenGui& g) { return g.kind != OpenGui::Kind::None; }

// Slot under the cursor (container slot number), or -999 outside the panel
// (drop, like GuiContainer.mouseClicked).
inline int slot_at(const OpenGui& g, double mx, double my) {
  const double dx = mx - g.px, dy = my - g.py;
  for (const auto& s : g.kit.c.slots) {
    if (dx >= s.x - 1 && dx < s.x + 16 + 1 && dy >= s.y - 1 && dy < s.y + 16 + 1)
      return s.number;
  }
  return -999;
}

struct ContainerMeshes {
  render::Mesh panel;   // per-kind panel texture (inventory/...)
  render::Mesh items;   // gui/items.png
  render::Mesh blocks;  // terrain.png (cubes + flat sprites)
  render::Mesh shadow;  // font shadow (counts)
  render::Mesh text;    // font (counts + labels)
  render::Mesh bars;    // flat_prog (durability)
  render::Mesh hl;      // flat_prog (hover highlight, alpha 0.5)
};

inline ContainerMeshes draw_open_gui(OpenGui& g, entity::Inventory& inv, const Font& font,
                                     const std::map<std::string, std::string>& lang, double mx,
                                     double my, int w, int h) {
  ContainerMeshes out;
  if (!gui_open(g)) return out;
  g.px = (w - g.x_size) / 2.0F;
  g.py = (h - g.y_size) / 2.0F;
  const float px = g.px, py = g.py;
  constexpr std::uint32_t kDark = 0xFF404040;
  auto label = [&](const std::string& s, float x, float y) {
    auto t = font.build_text(s, px + x, py + y, kDark, false);
    const auto base = static_cast<std::uint32_t>(out.text.vertices.size());
    out.text.vertices.insert(out.text.vertices.end(), t.vertices.begin(), t.vertices.end());
    for (auto ix : t.indices) out.text.indices.push_back(base + ix);
  };
  switch (g.kind) {
    case OpenGui::Kind::Inventory:
      blit_256(out.panel, px, py, 0, 0, 176, 166);
      label("Crafting", 86, 16);
      break;
    case OpenGui::Kind::Workbench:
      blit_256(out.panel, px, py, 0, 0, 176, 166);
      label("Crafting", 28, 6);
      label("Inventory", 8, 166 - 94);
      break;
    case OpenGui::Kind::Furnace:
      blit_256(out.panel, px, py, 0, 0, 176, 166);
      if (g.burn_scaled > 0) {
        const float b = static_cast<float>(g.burn_scaled);
        blit_256(out.panel, px + 56, py + 36 + 12 - b, 176, 12 - b, 14, b + 2);
      }
      blit_256(out.panel, px + 79, py + 34, 176, 14, static_cast<float>(g.cook_scaled) + 1,
               16);
      label("Furnace", 60, 6);
      label("Inventory", 8, 166 - 94);
      break;
    case OpenGui::Kind::Chest: {
      const int rows = 3;
      blit_256(out.panel, px, py, 0, 0, 176, rows * 18 + 17);
      blit_256(out.panel, px, py + rows * 18 + 17, 0, 126, 176, 96);
      label("Chest", 8, 6);
      label("Inventory", 8, g.y_size - 94);
      break;
    }
    case OpenGui::Kind::None:
      break;
  }
  paint::PaintOut po{&out.items, &out.blocks, &out.shadow, &out.text, &out.bars};
  int hover = -2;
  {
    const double dx = mx - px, dy = my - py;
    for (const auto& s : g.kit.c.slots) {
      if (dx >= s.x - 1 && dx < s.x + 16 + 1 && dy >= s.y - 1 && dy < s.y + 16 + 1) {
        hover = s.number;
        break;
      }
    }
  }
  for (const auto& s : g.kit.c.slots) {
    auto* o = s.stack();
    if (o == nullptr || !o->has_value()) continue;
    const auto& st = o->value();
    paint::paint_stack(po, font, px + s.x, py + s.y, st.item_id, st.stack_size, st.damage,
                       st.max_damage());
    if (s.number == hover) {
      // Hover wash (drawGradientRect -2130706433 = 0x80FFFFFF).
      const std::uint32_t base = static_cast<std::uint32_t>(out.hl.vertices.size());
      const float x0 = px + s.x, y0 = py + s.y;
      out.hl.vertices.push_back({x0, y0 + 16, 0, 1, 1, 1, 0, 0, 0.5F});
      out.hl.vertices.push_back({x0 + 16, y0 + 16, 0, 1, 1, 1, 0, 0, 0.5F});
      out.hl.vertices.push_back({x0 + 16, y0, 0, 1, 1, 1, 0, 0, 0.5F});
      out.hl.vertices.push_back({x0, y0, 0, 1, 1, 1, 0, 0, 0.5F});
      out.hl.indices.insert(out.hl.indices.end(), {base, base + 1, base + 2, base, base + 2,
                                                   base + 3});
    }
  }
  // Cursor stack at the mouse (vanilla -8 offset).
  if (inv.cursor.has_value()) {
    const auto& st = *inv.cursor;
    paint::paint_stack(po, font, static_cast<float>(mx) - 8, static_cast<float>(my) - 8,
                       st.item_id, st.stack_size, st.damage, st.max_damage());
  }
  // Tooltip (item display name) when the cursor is empty and hovering a
  // filled slot. Missing keys show the key itself (StatCollector parity).
  if (!inv.cursor.has_value() && hover >= 0) {
    const ctn::Slot* hs = g.kit.c.get(hover);
    const auto* ho = hs != nullptr ? hs->stack() : nullptr;
    if (ho != nullptr && ho->has_value()) {
      const std::string key = name_key_for(ho->value().item_id, ho->value().damage);
      if (!key.empty()) {
        auto it = lang.find(key);
        const std::string& name = it != lang.end() ? it->second : key;
        const float tw = static_cast<float>(font.string_width(name));
        const float tx = static_cast<float>(mx) + 12, ty = static_cast<float>(my) - 12;
        // Dark wash behind the text (opaque-ish like the vanilla box).
        const std::uint32_t base = static_cast<std::uint32_t>(out.hl.vertices.size());
        out.hl.vertices.push_back({tx - 3, ty + 8 + 3, 0, 0.06F, 0.06F, 0.06F, 0, 0, 0.94F});
        out.hl.vertices.push_back({tx + tw + 3, ty + 8 + 3, 0, 0.06F, 0.06F, 0.06F, 0, 0, 0.94F});
        out.hl.vertices.push_back({tx + tw + 3, ty - 3, 0, 0.06F, 0.06F, 0.06F, 0, 0, 0.94F});
        out.hl.vertices.push_back({tx - 3, ty - 3, 0, 0.06F, 0.06F, 0.06F, 0, 0, 0.94F});
        out.hl.indices.insert(out.hl.indices.end(), {base, base + 1, base + 2, base, base + 2,
                                                     base + 3});
        auto t = font.build_text(name, tx, ty, 0xFFFFFFFF, false);
        const auto b2 = static_cast<std::uint32_t>(out.text.vertices.size());
        out.text.vertices.insert(out.text.vertices.end(), t.vertices.begin(), t.vertices.end());
        for (auto ix : t.indices) out.text.indices.push_back(b2 + ix);
      }
    }
  }
  return out;
}

}  // namespace craftpp::gui
