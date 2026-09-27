#pragma once

#include <string>
#include <vector>

#include "gui/font.hpp"
#include "render/mesh.hpp"

namespace craftpp::gui {

// Shared 2D widget builders (256x256 atlases). Vanilla Gui/GuiButton ports.
struct Button {
  int id = -1;
  int x = 0, y = 0, w = 200, h = 20;
  std::string label;
  bool enabled = true;
  bool visible = true;
};

// 256-atlas blit (u right, v down), white vertex color.
void blit_256(render::Mesh& m, float x, float y, float u, float v, float w, float h);

// GuiButton.drawButton: two halves from gui.png row (46 + state*20),
// hover state 0 disabled / 1 normal / 2 hovered. Text via out_text/out_shadow.
void draw_button(render::Mesh& out_chrome, const Button& b, bool hovered, const Font& font,
                 render::Mesh& out_shadow, render::Mesh& out_text);

// drawBackground: background.png tiled at 32px with the 0x404040 tint.
void draw_background(render::Mesh& out, int width, int height);

// drawTexturedModalRect-style dirt/text bg is per-screen; gradient rects:
void draw_rect(render::Mesh& out, float x0, float y0, float x1, float y1, std::uint32_t argb);

// Centered string with shadow (drawCenteredString + shadow pass).
void draw_centered(const Font& font, render::Mesh& out_shadow, render::Mesh& out_text,
                   const std::string& s, float cx, float y, std::uint32_t argb);

}  // namespace craftpp::gui
