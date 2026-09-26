#pragma once

#include "gui/font.hpp"
#include "render/mesh.hpp"

namespace craftpp::gui {

// CPU-side port of GuiIngame.renderGameOverlay (1.0), survival subset:
// crosshair + hotbar (gui.png), hearts/armor/food/air/xp (icons.png),
// XP level text. Creative shows hotbar + crosshair only (shouldDrawHUD).
// Potions/hardcore/regen-flicker need gameplay state we don't simulate.
struct HudSlot {
  int id = 0;  // shifted item id (blocks < 256 draw the terrain sprite)
  int count = 0;
  int damage = 0;
  int max_damage = 0;
};

struct HudState {
  int width = 854, height = 480;
  int tick = 0;  // updateCounter (jitter/flicker seed, visual only)
  bool survival_hud = true;
  int health = 20;
  int food = 20;
  float saturation = 5.0F;
  int armor = 0;
  int air = 300;
  bool in_water = false;
  int current_item = 0;
  float xp_frac = 0.0F;
  int xp_level = 0;
  HudSlot hotbar[9];
};

struct HudMeshes {
  render::Mesh icons;   // icons.png (256x256), white vertex color
  render::Mesh chrome;  // gui.png (256x256), white vertex color
  render::Mesh items;   // items.png (256x256) sprites for non-block items
  render::Mesh blocks;  // terrain.png sprites for block items (flat interim;
                        // vanilla renders 3D cubes via renderBlockIntoGUI)
  render::Mesh bars;    // damage bars (flat color, no texture)
  render::Mesh shadow;  // font atlas, dark pass
  render::Mesh text;    // font atlas, main pass
};

HudMeshes build_hud(const HudState& s, const Font& font);

}  // namespace craftpp::gui
