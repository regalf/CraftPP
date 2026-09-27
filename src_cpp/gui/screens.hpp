#pragma once

#include <map>
#include <string>
#include <vector>

#include "gui/font.hpp"
#include "gui/widgets.hpp"
#include "render/mesh.hpp"

namespace craftpp::gui {

// Retained-state port of the 1.0 GuiScreen family (layouts, button ids and
// strings mirror the source; see plan M5 item 2). Backend wiring lives in
// the app: it interprets ScreenAction results and owns the world.
// Screens with no backend yet (multiplayer net, video/controls detail,
// achievements/stats) render and navigate but report Action::Placeholder.
enum class Screen {
  None,
  Main,
  Select,
  Create,
  Multi,
  Options,
  Ingame,
  GameOver,
  Confirm,
  Rename,
  Placeholder,
  Loading,
};

struct TextField {
  std::string text;
  int max_len = 32;
  bool focused = false;
};

struct WorldEntry {
  std::string title;
  std::string dir;
  std::string subtitle;
  int mode = 0;  // 0 survival, 1 hardcore, 2 creative
};

struct ScreenUi {
  Screen cur = Screen::Main;
  Screen parent = Screen::Main;
  int width = 854, height = 480;
  std::vector<Button> buttons;
  double mouse_x = 0, mouse_y = 0;
  // Main
  std::string splash = "missingno";
  // Select
  std::vector<WorldEntry> worlds;
  int selected = -1;
  // Create
  TextField name_field, seed_field;
  int mode_idx = 0;  // 0 survival, 1 hardcore, 2 creative
  bool features = true;
  bool more = false;
  std::string folder;  // computed unused folder name
  // Confirm (yes/no) + Rename
  std::string confirm_title, confirm_line;
  int confirm_id = -1;  // ScreenUi-local: 1 delete-world, 2 stub
  TextField rename_field;
  // Options (GameSettings subset, in-memory until M5 options backend)
  int difficulty = 2;  // 0 peaceful, 1 easy, 2 normal, 3 hard
  float music = 1.0F, sound = 1.0F, sensitivity = 0.5F, fov = 70.0F;
  bool invert = false;
  int drag_id = -1;  // slider being dragged, else -1
  // Multiplayer placeholder (session list; net lands with M7)
  std::vector<std::string> servers;
  int server_sel = -1;
  TextField address_field;
  std::string notice;
  // GameOver
  int score = 0;
  bool hardcore = false;
  // Loading (LoadingScreenRenderer port: title + subtitle + 0-100 bar)
  std::string loading_title;
  std::string loading_sub;
  int loading_progress = -1;
  // Placeholder screens
  std::string placeholder_title;
};

struct ScreenCtx {
  std::string saves_root = "saves";
  std::string assets_root = "assets";
  std::map<std::string, std::string> lang;
};

// (Re)builds buttons + per-screen state when opening a screen.
void open_screen(ScreenUi& ui, Screen s, const ScreenCtx& ctx);
// Scan saves_root for worlds with level.dat.
void refresh_worlds(ScreenUi& ui, const ScreenCtx& ctx);
// Button id under the cursor, or -1.
int button_at(const ScreenUi& ui, double x, double y);
// Slider under the cursor on the Options screen, or -1.
int slider_at(const ScreenUi& ui, double x, double y);
// Refreshes the create-screen button labels after a toggle.
void refresh_create_labels(ScreenUi& ui, const ScreenCtx& ctx);
// Refreshes the options-screen labels after a change.
void refresh_options_labels(ScreenUi& ui, const ScreenCtx& ctx);
// Applies a slider drag position (0..1) to music/sound/sensitivity/fov.
void apply_slider(ScreenUi& ui, int slider_id, float v);
// Sanitized unused folder name for a new world (create screen).
std::string make_world_folder(const std::string& saves_root, const std::string& name);
// Parse the seed field (empty = random, numeric, else Java hash).
long parse_seed(const std::string& text, long random_fallback);

struct ScreenMeshes {
  render::Mesh bg;      // background.png
  render::Mesh chrome;  // gui.png (buttons, logo handled separately)
  render::Mesh logo;    // mclogo.png
  render::Mesh shadow;  // font atlas, dark pass
  render::Mesh text;    // font atlas, main pass
  render::Mesh flat;    // solid quads (selection highlight, fields, sliders)
};

// Builds all meshes for the current screen (text via font).
ScreenMeshes draw_screen(ScreenUi& ui, const ScreenCtx& ctx, const Font& font, int tick);

}  // namespace craftpp::gui
