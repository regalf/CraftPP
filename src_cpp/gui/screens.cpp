#include "gui/screens.hpp"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <string>

#include "gui/lang.hpp"
#include "world/save.hpp"

namespace craftpp::gui {

namespace {

Button make_button(int id, int x, int y, int w, int h, const std::string& label) {
  Button b;
  b.id = id;
  b.x = x;
  b.y = y;
  b.w = w;
  b.h = h;
  b.label = label;
  return b;
}

const char* mode_key(int idx) {
  return idx == 0 ? "survival" : (idx == 1 ? "hardcore" : "creative");
}

}  // namespace

std::string make_world_folder(const std::string& saves_root, const std::string& name) {
  static const char* bad = "/\n\r\t\f`?*\\<>|\":";
  std::string folder;
  for (char c : name) {
    folder += (std::string(bad).find(c) == std::string::npos) ? c : '_';
  }
  // Trim spaces like the source trim().
  while (!folder.empty() && folder.front() == ' ') folder.erase(folder.begin());
  while (!folder.empty() && folder.back() == ' ') folder.pop_back();
  if (folder.empty()) folder = "World";
  std::string cand = folder;
  while (std::filesystem::exists(saves_root + "/" + cand + "/level.dat")) cand += "-";
  return cand;
}

long parse_seed(const std::string& text, long random_fallback) {
  std::string t = text;
  while (!t.empty() && t.front() == ' ') t.erase(t.begin());
  while (!t.empty() && t.back() == ' ') t.pop_back();
  if (t.empty()) return random_fallback;
  char* end = nullptr;
  long v = std::strtol(t.c_str(), &end, 10);
  if (end != nullptr && *end == '\0') return v != 0L ? v : random_fallback;
  return java_string_hash(t);
}

void refresh_worlds(ScreenUi& ui, const ScreenCtx& ctx) {
  ui.worlds.clear();
  ui.selected = -1;
  std::error_code ec;
  if (!std::filesystem::is_directory(ctx.saves_root, ec)) return;
  for (const auto& e : std::filesystem::directory_iterator(ctx.saves_root, ec)) {
    if (!e.is_directory(ec)) continue;
    auto info = world::read_level_dat(e.path().string());
    if (!info.has_value()) continue;
    WorldEntry w;
    w.title = info->level_name;
    w.dir = e.path().filename().string();
    w.mode = info->game_type;
    w.subtitle = tr(ctx.lang, w.mode == 2   ? "selectWorld.gameMode.creative"
                              : w.mode == 1 ? "selectWorld.gameMode.hardcore"
                                            : "selectWorld.gameMode.survival");
    if (w.title.empty()) w.title = w.dir;
    ui.worlds.push_back(w);
  }
}

void open_screen(ScreenUi& ui, Screen s, const ScreenCtx& ctx) {
  ui.cur = s;
  ui.buttons.clear();
  ui.drag_id = -1;
  ui.notice.clear();
  const int w = ui.width, h = ui.height;
  const auto& L = ctx.lang;
  switch (s) {
    case Screen::Main: {
      const int y = h / 4 + 48;
      ui.buttons.push_back(make_button(1, w / 2 - 100, y, 200, 20, tr(L, "menu.singleplayer")));
      ui.buttons.push_back(make_button(2, w / 2 - 100, y + 24, 200, 20, tr(L, "menu.multiplayer")));
      ui.buttons.push_back(make_button(0, w / 2 - 100, y + 72, 200, 20, tr(L, "menu.options")));
      ui.buttons.push_back(make_button(4, w / 2 - 100, y + 96, 200, 20, tr(L, "menu.quit")));
      break;
    }
    case Screen::Select: {
      refresh_worlds(ui, ctx);
      ui.buttons.push_back(
          make_button(1, w / 2 - 154, h - 52, 150, 20, tr(L, "selectWorld.select")));
      ui.buttons.push_back(
          make_button(6, w / 2 - 154, h - 28, 70, 20, tr(L, "selectWorld.rename")));
      ui.buttons.push_back(
          make_button(2, w / 2 - 74, h - 28, 70, 20, tr(L, "selectWorld.delete")));
      ui.buttons.push_back(
          make_button(3, w / 2 + 4, h - 52, 150, 20, tr(L, "selectWorld.create")));
      ui.buttons.push_back(make_button(0, w / 2 + 4, h - 28, 150, 20, tr(L, "gui.cancel")));
      break;
    }
    case Screen::Create: {
      ui.buttons.push_back(
          make_button(0, w / 2 - 155, h - 28, 150, 20, tr(L, "selectWorld.create")));
      ui.buttons.push_back(make_button(1, w / 2 + 5, h - 28, 150, 20, tr(L, "gui.cancel")));
      ui.buttons.push_back(make_button(2, w / 2 - 75, 100, 150, 20, ""));
      ui.buttons.push_back(make_button(3, w / 2 - 75, 172, 150, 20, ""));
      ui.buttons.push_back(make_button(4, w / 2 - 155, 100, 150, 20, ""));
      ui.buttons.push_back(make_button(5, w / 2 + 5, 100, 150, 20, ""));
      ui.name_field = TextField{"New World", 32, true};
      ui.seed_field = TextField{"", 32, false};
      ui.more = false;
      ui.folder = make_world_folder(ctx.saves_root, ui.name_field.text);
      refresh_create_labels(ui, ctx);
      break;
    }
    case Screen::Multi: {
      ui.buttons.push_back(
          make_button(1, w / 2 - 154, h - 52, 100, 20, tr(L, "selectServer.select")));
      ui.buttons.push_back(
          make_button(4, w / 2 - 50, h - 52, 100, 20, tr(L, "selectServer.direct")));
      ui.buttons.push_back(
          make_button(3, w / 2 + 4 + 50, h - 52, 100, 20, tr(L, "selectServer.add")));
      ui.buttons.push_back(
          make_button(7, w / 2 - 154, h - 28, 70, 20, tr(L, "selectServer.edit")));
      ui.buttons.push_back(
          make_button(2, w / 2 - 74, h - 28, 70, 20, tr(L, "selectServer.delete")));
      ui.buttons.push_back(
          make_button(8, w / 2 + 4, h - 28, 70, 20, tr(L, "selectServer.refresh")));
      ui.buttons.push_back(
          make_button(0, w / 2 + 4 + 76, h - 28, 75, 20, tr(L, "gui.cancel")));
      ui.address_field = TextField{"", 32, false};
      break;
    }
    case Screen::Options: {
      // Vanilla order: music, sound, invert, sensitivity, fov, difficulty.
      ui.buttons.push_back(make_button(20, w / 2 - 155 + (0 % 2) * 160, h / 6 + 24 * (0 >> 1), 150,
                                       20, ""));
      ui.buttons.push_back(make_button(21, w / 2 - 155 + (1 % 2) * 160, h / 6 + 24 * (1 >> 1), 150,
                                       20, ""));
      ui.buttons.push_back(make_button(22, w / 2 - 155 + (2 % 2) * 160, h / 6 + 24 * (2 >> 1), 150,
                                       20, ""));
      ui.buttons.push_back(make_button(23, w / 2 - 155 + (3 % 2) * 160, h / 6 + 24 * (3 >> 1), 150,
                                       20, ""));
      ui.buttons.push_back(make_button(24, w / 2 - 155 + (4 % 2) * 160, h / 6 + 24 * (4 >> 1), 150,
                                       20, ""));
      ui.buttons.push_back(make_button(25, w / 2 - 155 + (5 % 2) * 160, h / 6 + 24 * (5 >> 1), 150,
                                       20, ""));
      ui.buttons.push_back(make_button(101, w / 2 - 100, h / 6 + 96 + 12, 200, 20,
                                       tr(L, "options.video")));
      ui.buttons.push_back(make_button(100, w / 2 - 100, h / 6 + 120 + 12, 200, 20,
                                       tr(L, "options.controls")));
      ui.buttons.push_back(
          make_button(200, w / 2 - 100, h / 6 + 168, 200, 20, tr(L, "gui.done")));
      refresh_options_labels(ui, ctx);
      break;
    }
    case Screen::Ingame: {
      // Labels are hardcoded literals in the source (not lang keys).
      const int o = -16;
      ui.buttons.push_back(make_button(1, w / 2 - 100, h / 4 + 120 + o, 200, 20,
                                       "Save and quit to title"));
      ui.buttons.push_back(
          make_button(4, w / 2 - 100, h / 4 + 24 + o, 200, 20, "Back to game"));
      ui.buttons.push_back(
          make_button(0, w / 2 - 100, h / 4 + 96 + o, 200, 20, tr(L, "menu.options")));
      ui.buttons.push_back(
          make_button(5, w / 2 - 100, h / 4 + 48 + o, 98, 20, "Achievements"));
      ui.buttons.push_back(
          make_button(6, w / 2 + 2, h / 4 + 48 + o, 98, 20, "Statistics"));
      break;
    }
    case Screen::GameOver: {
      if (ui.hardcore) {
        ui.buttons.push_back(make_button(1, w / 2 - 100, h / 4 + 96, 200, 20,
                                         tr(L, "deathScreen.deleteWorld")));
      } else {
        ui.buttons.push_back(make_button(1, w / 2 - 100, h / 4 + 72, 200, 20,
                                         tr(L, "deathScreen.respawn")));
        ui.buttons.push_back(make_button(2, w / 2 - 100, h / 4 + 96, 200, 20,
                                         tr(L, "deathScreen.titleScreen")));
      }
      break;
    }
    case Screen::Confirm: {
      ui.buttons.push_back(make_button(0, w / 2 - 155, h / 6 + 96, 150, 20, tr(L, "gui.no")));
      ui.buttons.push_back(make_button(1, w / 2 - 155 + 160, h / 6 + 96, 150, 20, tr(L, "gui.yes")));
      break;
    }
    case Screen::Rename: {
      ui.rename_field = TextField{"", 32, true};
      ui.buttons.push_back(
          make_button(0, w / 2 - 155, h - 28, 150, 20, tr(L, "selectWorld.renameButton")));
      ui.buttons.push_back(make_button(1, w / 2 + 5, h - 28, 150, 20, tr(L, "gui.cancel")));
      break;
    }
    case Screen::Placeholder: {
      ui.buttons.push_back(
          make_button(0, w / 2 - 100, h - 28, 200, 20, tr(L, "gui.done")));
      break;
    }
    case Screen::None:
      break;
  }
}

int button_at(const ScreenUi& ui, double x, double y) {
  for (const Button& b : ui.buttons) {
    if (!b.visible || !b.enabled) continue;
    if (x >= b.x && y >= b.y && x < b.x + b.w && y < b.y + b.h) return b.id;
  }
  return -1;
}
int slider_at(const ScreenUi& ui, double x, double y) {
  if (ui.cur != Screen::Options) return -1;
  for (const Button& b : ui.buttons) {
    if ((b.id == 20 || b.id == 21 || b.id == 23 || b.id == 24) && b.visible &&
        x >= b.x && y >= b.y && x < b.x + b.w && y < b.y + b.h) {
      return b.id;
    }
  }
  return -1;
}

void refresh_create_labels(ScreenUi& ui, const ScreenCtx& ctx) {  const auto& L = ctx.lang;
  for (Button& b : ui.buttons) {
    if (b.id == 2) {
      b.label = tr(L, "selectWorld.gameMode") + " " + tr(L, std::string("selectWorld.gameMode.") + mode_key(ui.mode_idx));
      b.visible = !ui.more;
    } else if (b.id == 3) {
      b.label = tr(L, "selectWorld.moreWorldOptions");
    } else if (b.id == 4) {
      b.label = tr(L, "selectWorld.mapFeatures") + " " +
                tr(L, ui.features ? "options.on" : "options.off");
      b.visible = ui.more;
    } else if (b.id == 5) {
      b.label =
          tr(L, "selectWorld.mapType") + " " + tr(L, "selectWorld.mapType.normal");
      b.visible = ui.more;
    }
  }
}

void apply_slider(ScreenUi& ui, int slider_id, float v) {
  if (v < 0) v = 0;
  if (v > 1) v = 1;
  if (slider_id == 20) ui.music = v;
  if (slider_id == 21) ui.sound = v;
  if (slider_id == 23) ui.sensitivity = v;
  if (slider_id == 24) ui.fov = 70.0F - 40.0F + v * 110.0F;
}

void refresh_options_labels(ScreenUi& ui, const ScreenCtx& ctx) {
  const auto& L = ctx.lang;
  const char* diffs[] = {"options.difficulty.peaceful", "options.difficulty.easy",
                         "options.difficulty.normal", "options.difficulty.hard"};
  for (Button& b : ui.buttons) {
    if (b.id == 25) {
      b.label = tr(L, "options.difficulty") + ": " + tr(L, diffs[ui.difficulty]);
    } else if (b.id == 20) {
      b.label = tr(L, "options.music") + ": " + std::to_string(int(ui.music * 100)) + "%";
    } else if (b.id == 21) {
      b.label = tr(L, "options.sound") + ": " + std::to_string(int(ui.sound * 100)) + "%";
    } else if (b.id == 22) {
      b.label = tr(L, "options.invertMouse") + ": " + tr(L, ui.invert ? "options.on" : "options.off");
    } else if (b.id == 23) {
      b.label = tr(L, "options.sensitivity") + ": " + std::to_string(int(ui.sensitivity * 200)) + "%";
    } else if (b.id == 24) {
      b.label = "FOV: " + std::to_string(int(ui.fov));
    }
  }
}

ScreenMeshes draw_screen(ScreenUi& ui, const ScreenCtx& ctx, const Font& font, int tick) {
  ScreenMeshes m;
  const int w = ui.width, h = ui.height;
  const auto& L = ctx.lang;
  const float mx = static_cast<float>(ui.mouse_x), my = static_cast<float>(ui.mouse_y);

  auto buttons = [&]() {
    for (const Button& b : ui.buttons) {
      draw_button(m.chrome, b, button_at(ui, mx, my) == b.id && b.enabled, font, m.shadow,
                  m.text);
    }
  };
  auto title = [&](const std::string& s, float y) {
    draw_centered(font, m.shadow, m.text, s, w / 2.0F, y, 0xFFFFFFFF);
  };
  auto field = [&](const TextField& f, int x, int y, int fw) {
    draw_rect(m.flat, x - 1, y - 1, x + fw + 1, y + 21, 0xFFA0A0A0);
    draw_rect(m.flat, x, y, x + fw, y + 20, 0xFF000000);
    std::string shown = f.text;
    while (!shown.empty() && font.string_width(shown) > fw - 8) shown.erase(shown.begin());
    auto fg = font.build_text(shown, x + 4.0F, y + 6.0F, 0xFFE0E0E0, false);
    auto base = static_cast<std::uint32_t>(m.text.vertices.size());
    m.text.vertices.insert(m.text.vertices.end(), fg.vertices.begin(), fg.vertices.end());
    for (auto ix : fg.indices) m.text.indices.push_back(base + ix);
    if (f.focused && (tick / 6) % 2 == 0) {
      auto cur = font.build_text("_", x + 4.0F + font.string_width(shown), y + 6.0F, 0xFFE0E0E0,
                                 false);
      base = static_cast<std::uint32_t>(m.text.vertices.size());
      m.text.vertices.insert(m.text.vertices.end(), cur.vertices.begin(), cur.vertices.end());
      for (auto ix : cur.indices) m.text.indices.push_back(base + ix);
    }
  };

  switch (ui.cur) {
    case Screen::Main: {
      draw_background(m.bg, w, h);
      // Logo (mclogo.png, two 155x44 rows).
      const float lx = w / 2.0F - 137.0F, ly = 30.0F;
      blit_256(m.logo, lx, ly, 0, 0, 155, 44);
      blit_256(m.logo, lx + 155, ly, 0, 45, 155, 44);
      // Splash (yellow, rotated -20 deg, pulsing).
      {
        const float pulse =
            (1.8F - std::abs(std::sin(tick / 1000.0F * 3.14159265F * 2.0F)) * 0.1F) * 100.0F /
            (font.string_width(ui.splash) + 32);
        auto sh = font.build_text(ui.splash, 0, 0, 0xFFFFFF00, true);
        auto fg = font.build_text(ui.splash, 0, 0, 0xFFFFFF00, false);
        const float ang = -20.0F * 3.14159265F / 180.0F;
        const float ca = std::cos(ang) * pulse, sa = std::sin(ang) * pulse;
        for (auto* mm : {&sh, &fg}) {
          for (auto& v : mm->vertices) {
            const float nx = v.x * ca - v.y * sa + (w / 2.0F + 90);
            const float ny = v.x * sa + v.y * ca + 70.0F;
            v.x = nx;
            v.y = ny;
          }
        }
        auto base = static_cast<std::uint32_t>(m.shadow.vertices.size());
        m.shadow.vertices.insert(m.shadow.vertices.end(), sh.vertices.begin(), sh.vertices.end());
        for (auto ix : sh.indices) m.shadow.indices.push_back(base + ix);
        base = static_cast<std::uint32_t>(m.text.vertices.size());
        m.text.vertices.insert(m.text.vertices.end(), fg.vertices.begin(), fg.vertices.end());
        for (auto ix : fg.indices) m.text.indices.push_back(base + ix);
      }
      auto v1 = font.build_text("Craft++ 1.0.0", 2, h - 10.0F, 0xFFFFFFFF, false);
      auto v2 = font.build_text("Clean-room singleplayer", w - font.string_width("Clean-room singleplayer") - 2.0F, h - 10.0F, 0xFFFFFFFF, false);
      for (auto* mm : {&v1, &v2}) {
        const auto base = static_cast<std::uint32_t>(m.text.vertices.size());
        m.text.vertices.insert(m.text.vertices.end(), mm->vertices.begin(), mm->vertices.end());
        for (auto ix : mm->indices) m.text.indices.push_back(base + ix);
      }
      buttons();
      break;
    }
    case Screen::Select: {
      draw_background(m.bg, w, h);
      title(tr(L, "selectWorld.title"), 20);
      // World rows (36px each from y=32, like GuiSlotWorld).
      int y = 32;
      for (std::size_t i = 0; i < ui.worlds.size() && y + 36 < h - 64; ++i, y += 36) {
        if (static_cast<int>(i) == ui.selected) {
          draw_rect(m.flat, 0, y, w, y + 36, 0xFF202020);
        }
        auto t1 = font.build_text(ui.worlds[i].title, w / 2.0F - 150, y + 3, 0xFFFFFFFF, false);
        auto t2 = font.build_text(ui.worlds[i].subtitle, w / 2.0F - 150, y + 3 + 12, 0xFF808080,
                                  false);
        for (auto* mm : {&t1, &t2}) {
          const auto base = static_cast<std::uint32_t>(m.text.vertices.size());
          m.text.vertices.insert(m.text.vertices.end(), mm->vertices.begin(), mm->vertices.end());
          for (auto ix : mm->indices) m.text.indices.push_back(base + ix);
        }
      }
      if (ui.worlds.empty()) {
        draw_centered(font, m.shadow, m.text, tr(L, "selectWorld.empty"), w / 2.0F, h / 2.0F,
                      0xFFFFFFFF);
      }
      // Enable Play/Rename/Delete only with a selection.
      for (const Button& b : ui.buttons) {
        Button c = b;
        if ((c.id == 1 || c.id == 6 || c.id == 2) && ui.selected < 0) c.enabled = false;
        draw_button(m.chrome, c, button_at(ui, mx, my) == c.id && c.enabled, font, m.shadow,
                    m.text);
      }
      break;
    }
    case Screen::Create: {
      draw_background(m.bg, w, h);
      title(tr(L, "selectWorld.create"), 20);
      auto l1 = font.build_text(tr(L, "selectWorld.enterName"), w / 2.0F - 100, 47, 0xFFA0A0A0,
                                false);
      auto base = static_cast<std::uint32_t>(m.text.vertices.size());
      m.text.vertices.insert(m.text.vertices.end(), l1.vertices.begin(), l1.vertices.end());
      for (auto ix : l1.indices) m.text.indices.push_back(base + ix);
      field(ui.name_field, w / 2 - 100, 60, 200);
      if (!ui.more) {
        auto mode_line1 = font.build_text(tr(L, std::string("selectWorld.gameMode.") + mode_key(ui.mode_idx) + ".line1"), w / 2.0F - 100, 122, 0xFFA0A0A0, false);
        auto mode_line2 = font.build_text(tr(L, std::string("selectWorld.gameMode.") + mode_key(ui.mode_idx) + ".line2"), w / 2.0F - 100, 134, 0xFFA0A0A0, false);
        for (auto* mm : {&mode_line1, &mode_line2}) {
          base = static_cast<std::uint32_t>(m.text.vertices.size());
          m.text.vertices.insert(m.text.vertices.end(), mm->vertices.begin(), mm->vertices.end());
          for (auto ix : mm->indices) m.text.indices.push_back(base + ix);
        }
      } else {
        auto sl = font.build_text(tr(L, "selectWorld.enterSeed"), w / 2.0F - 100, 47 - 40 + 88, 0xFFA0A0A0, false);
        base = static_cast<std::uint32_t>(m.text.vertices.size());
        m.text.vertices.insert(m.text.vertices.end(), sl.vertices.begin(), sl.vertices.end());
        for (auto ix : sl.indices) m.text.indices.push_back(base + ix);
        field(ui.seed_field, w / 2 - 100, 60 + 48, 200);
      }
      buttons();
      break;
    }
    case Screen::Multi: {
      draw_background(m.bg, w, h);
      title(tr(L, "multiplayer.title"), 20);
      int y = 32;
      for (std::size_t i = 0; i < ui.servers.size() && y + 36 < h - 64; ++i, y += 36) {
        if (static_cast<int>(i) == ui.server_sel) {
          draw_rect(m.flat, 0, y, w, y + 36, 0xFF202020);
        }
        auto t1 = font.build_text(ui.servers[i], w / 2.0F - 150, y + 3, 0xFFFFFFFF, false);
        auto t2 = font.build_text("net (M7)", w / 2.0F - 150, y + 3 + 12, 0xFF808080, false);
        for (auto* mm : {&t1, &t2}) {
          const auto base = static_cast<std::uint32_t>(m.text.vertices.size());
          m.text.vertices.insert(m.text.vertices.end(), mm->vertices.begin(), mm->vertices.end());
          for (auto ix : mm->indices) m.text.indices.push_back(base + ix);
        }
      }
      if (!ui.notice.empty()) {
        draw_centered(font, m.shadow, m.text, ui.notice, w / 2.0F, h - 70.0F, 0xFFFF5555);
      }
      buttons();
      break;
    }
    case Screen::Options: {
      draw_background(m.bg, w, h);
      title(tr(L, "options.title"), 20);
      // Sliders (music/sound/sensitivity/fov): track + knob.
      auto slider = [&](int id, float v) {
        for (const Button& b : ui.buttons) {
          if (b.id != id) continue;
          draw_rect(m.flat, b.x, b.y, b.x + b.w, b.y + b.h, 0xFF000000);
          const float kx = b.x + v * (b.w - 8);
          draw_rect(m.flat, kx, b.y, kx + 8, b.y + b.h, 0xFF808080);
        }
      };
      slider(20, ui.music);
      slider(21, ui.sound);
      slider(23, ui.sensitivity);
      slider(24, (ui.fov - 70.0F + 40.0F) / 110.0F);
      buttons();
      break;
    }
    case Screen::Ingame: {
      // No dim quad (flat shader is opaque): frozen world stays visible
      // behind the buttons like the source's translucent gradient.
      title("Game menu", 40);
      buttons();
      break;
    }
    case Screen::GameOver: {
      draw_rect(m.flat, 0, 0, w, h, 0xFF000000);
      // 2x scaled title.
      auto t = font.build_text(tr(L, "deathScreen.title"), 0, 0, 0xFFFFFFFF, false);
      for (auto& v : t.vertices) {
        v.x = v.x * 2 + w / 2.0F - font.string_width(tr(L, "deathScreen.title"));
        v.y = v.y * 2 + 30.0F;
      }
      auto base = static_cast<std::uint32_t>(m.text.vertices.size());
      m.text.vertices.insert(m.text.vertices.end(), t.vertices.begin(), t.vertices.end());
      for (auto ix : t.indices) m.text.indices.push_back(base + ix);
      draw_centered(font, m.shadow, m.text,
                    tr(L, "deathScreen.score") + ": \xC2\xA7e" + std::to_string(ui.score),
                    w / 2.0F, 100, 0xFFFFFFFF);
      buttons();
      break;
    }
    case Screen::Confirm: {
      draw_background(m.bg, w, h);
      draw_centered(font, m.shadow, m.text, ui.confirm_title, w / 2.0F, 70, 0xFFFFFFFF);
      // Word-wrap the line crudely at 60 chars.
      std::string line = ui.confirm_line;
      float ly = 90;
      while (!line.empty()) {
        std::string part = line.substr(0, 60);
        line = line.size() > 60 ? line.substr(60) : "";
        draw_centered(font, m.shadow, m.text, part, w / 2.0F, ly, 0xFFFFFFFF);
        ly += 12;
      }
      buttons();
      break;
    }
    case Screen::Rename: {
      draw_background(m.bg, w, h);
      title(tr(L, "selectWorld.renameTitle"), 20);
      field(ui.rename_field, w / 2 - 100, 60, 200);
      buttons();
      break;
    }
    case Screen::Placeholder: {
      draw_background(m.bg, w, h);
      title(ui.placeholder_title, 40);
      if (!ui.notice.empty()) {
        draw_centered(font, m.shadow, m.text, ui.notice, w / 2.0F, 80, 0xFFA0A0A0);
      }
      buttons();
      break;
    }
    case Screen::None:
      break;
  }
  return m;
}

}  // namespace craftpp::gui
