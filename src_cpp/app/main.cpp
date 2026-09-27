// Craft++ client: title screens + live singleplayer game.
//
// Title flow (GuiScreen ports): Main -> Select/Create/Multi/Options,
// in-game ESC menu, death screen. Worlds live in saves/<Folder>/.
// Usage: ./craftpp [--assets DIR] [--seed N] [--save DIR] [--screenshot f.png]
// --save/--seed boot straight into the game (headless friendly).

#include <chrono>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <stb/stb_image_write.h>

#include "core/log.hpp"
#include "core/random.hpp"
#include "entity/controller.hpp"
#include "entity/pig.hpp"
#include "entity/player_sp.hpp"
#include "entity/zombie.hpp"
#include "gui/font.hpp"
#include "gui/hud.hpp"
#include "gui/lang.hpp"
#include "gui/screens.hpp"
#include "render/frustum.hpp"
#include "render/mesher.hpp"
#include "render/model.hpp"
#include "render/shader.hpp"
#include "render/shaders.hpp"
#include "render/tessellator.hpp"
#include "render/texture.hpp"
#include "world/chunk_manager.hpp"
#include "world/live.hpp"
#include "world/provider.hpp"
#include "world/save.hpp"

namespace {

using craftpp::JavaRandom;
using craftpp::entity::Controller;
using craftpp::entity::ControllerCreative;
using craftpp::entity::ControllerSP;
using craftpp::entity::PlayerSP;
using craftpp::world::BlockCollider;
using craftpp::world::LiveWorld;

// Full-cube voxel pick (most terrain here is full cubes; shapes are M5).
bool pick_block(LiveWorld& w, double ex, double ey, double ez, double dx, double dy, double dz,
                double reach, int& hx, int& hy, int& hz, int& side) {
  int cx = static_cast<int>(std::floor(ex));
  int cy = static_cast<int>(std::floor(ey));
  int cz = static_cast<int>(std::floor(ez));
  const double step = 0.05;
  double traveled = 0.0;
  int px = cx, py = cy, pz = cz;
  while (traveled <= reach) {
    if (!(px == cx && py == cy && pz == cz)) {
      const int id = w.block_id(cx, cy, cz);
      if (id != 0) {
        hx = cx;
        hy = cy;
        hz = cz;
        if (px != cx) {
          side = (px < cx) ? 4 : 5;
        } else if (py != cy) {
          side = (py < cy) ? 0 : 1;
        } else {
          side = (pz < cz) ? 2 : 3;
        }
        return true;
      }
      px = cx;
      py = cy;
      pz = cz;
    }
    ex += dx * step;
    ey += dy * step;
    ez += dz * step;
    traveled += step;
    cx = static_cast<int>(std::floor(ex));
    cy = static_cast<int>(std::floor(ey));
    cz = static_cast<int>(std::floor(ez));
  }
  return false;
}

// Flat-shaded box helper for drops (winding CCW front; culling stays off).
void add_quad(craftpp::render::Mesh& m, float x0, float y0, float z0, float x1, float y1, float z1,
              float x2, float y2, float z2, float x3, float y3, float z3, float r, float g,
              float b) {
  std::uint32_t base = static_cast<std::uint32_t>(m.vertices.size());
  m.vertices.push_back({x0, y0, z0, r, g, b});
  m.vertices.push_back({x1, y1, z1, r, g, b});
  m.vertices.push_back({x2, y2, z2, r, g, b});
  m.vertices.push_back({x3, y3, z3, r, g, b});
  m.indices.insert(m.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
}

// Crack overlay cube (destroy stages 240-249): slightly expanded to win
// depth against the block faces, full texture on all 6 sides.
void add_crack_cube(craftpp::render::Mesh& m, int x, int y, int z, int tile) {
  const float e = 0.01F;
  const float x0 = x - e, x1 = x + 1 + e;
  const float y0 = y - e, y1 = y + 1 + e;
  const float z0 = z - e, z1 = z + 1 + e;
  const float tx = static_cast<float>((tile & 15) * 16);
  const float ty = static_cast<float>(tile & 240);
  const float u0 = tx / 256.0F, u1 = (tx + 16.0F - 0.01F) / 256.0F;
  const float v0 = ty / 256.0F, v1 = (ty + 16.0F - 0.01F) / 256.0F;
  auto quad = [&](float ax, float ay, float az, float bx, float by, float bz, float cx, float cy,
                  float cz, float dx, float dy, float dz) {
    std::uint32_t base = static_cast<std::uint32_t>(m.vertices.size());
    m.vertices.push_back({ax, ay, az, 1, 1, 1, u0, v1});
    m.vertices.push_back({bx, by, bz, 1, 1, 1, u1, v1});
    m.vertices.push_back({cx, cy, cz, 1, 1, 1, u1, v0});
    m.vertices.push_back({dx, dy, dz, 1, 1, 1, u0, v0});
    m.indices.insert(m.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
  };
  quad(x0, y1, z1, x1, y1, z1, x1, y1, z0, x0, y1, z0);  // top
  quad(x0, y0, z0, x1, y0, z0, x1, y0, z1, x0, y0, z1);  // bottom
  quad(x0, y1, z0, x1, y1, z0, x1, y0, z0, x0, y0, z0);  // -z
  quad(x0, y1, z1, x0, y0, z1, x1, y0, z1, x1, y1, z1);  // +z
  quad(x0, y1, z1, x0, y1, z0, x0, y0, z0, x0, y0, z1);  // -x
  quad(x1, y1, z0, x1, y1, z1, x1, y0, z1, x1, y0, z0);  // +x
}

constexpr const char* kFlatVert = R"GLSL(
#version 330 core
layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_color;
layout(location = 2) in vec2 in_uv;
uniform mat4 u_mvp;
out vec3 v_color;
void main() {
  gl_Position = u_mvp * vec4(in_pos, 1.0);
  v_color = in_color;
}
)GLSL";

constexpr const char* kFlatFrag = R"GLSL(
#version 330 core
in vec3 v_color;
uniform float u_bright;
out vec4 out_color;
void main() {
  out_color = vec4(v_color * u_bright, 1.0);
}
)GLSL";

bool save_screenshot(const std::string& path, int w, int h) {
  glPixelStorei(GL_PACK_ALIGNMENT, 1);  // 854*3 is not a multiple of 4
  std::vector<std::uint8_t> px(static_cast<std::size_t>(w) * h * 3);
  glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px.data());
  std::vector<std::uint8_t> flipped(px.size());
  for (int y = 0; y < h; ++y) {
    std::memcpy(flipped.data() + static_cast<std::size_t>(y) * w * 3,
                px.data() + static_cast<std::size_t>(h - 1 - y) * w * 3,
                static_cast<std::size_t>(w) * 3);
  }
  stbi_flip_vertically_on_write(0);
  return stbi_write_png(path.c_str(), w, h, 3, flipped.data(), w * 3) != 0;
}

struct Args {
  std::string assets = "assets";
  std::string saves = "saves";
  std::string save_dir;
  std::string screenshot;
  int shot_frames = 5;
  std::int64_t seed = 1;
};

Args parse_args(int argc, char** argv) {
  Args a;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--assets") == 0 && i + 1 < argc) {
      a.assets = argv[++i];
    } else if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
      a.seed = std::atoll(argv[++i]);
    } else if (std::strcmp(argv[i], "--save") == 0 && i + 1 < argc) {
      a.save_dir = argv[++i];
    } else if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
      a.screenshot = argv[++i];
    } else if (std::strcmp(argv[i], "--shot-frames") == 0 && i + 1 < argc) {
      a.shot_frames = std::atoi(argv[++i]);
    }
  }
  return a;
}

// ---- frame input (filled by GLFW callbacks, drained in the loop) ----
int g_wheel = 0;
std::string g_chars;
bool g_backspace = false;
bool g_enter = false;

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
  (void)window;
  (void)xoffset;
  if (yoffset > 0) ++g_wheel;
  if (yoffset < 0) --g_wheel;
}

void char_callback(GLFWwindow* window, unsigned int codepoint) {
  (void)window;
  char buf[8] = {};
  if (codepoint < 0x80) {
    buf[0] = static_cast<char>(codepoint);
  } else if (codepoint < 0x800) {
    buf[0] = static_cast<char>(0xC0 | (codepoint >> 6));
    buf[1] = static_cast<char>(0x80 | (codepoint & 0x3F));
  } else if (codepoint < 0x10000) {
    buf[0] = static_cast<char>(0xE0 | (codepoint >> 12));
    buf[1] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
    buf[2] = static_cast<char>(0x80 | (codepoint & 0x3F));
  } else {
    return;
  }
  g_chars += buf;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
  (void)window;
  (void)scancode;
  (void)mods;
  if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
  if (key == GLFW_KEY_BACKSPACE) g_backspace = true;
  if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) g_enter = true;
}

// ---- live game session (owned while playing, torn down to title) ----
struct Session {
  std::unique_ptr<LiveWorld> world;
  std::unique_ptr<PlayerSP> player;
  std::unique_ptr<ControllerSP> csp;
  std::unique_ptr<ControllerCreative> ccr;
  Controller* controller = nullptr;
  bool creative = false;
  std::string save_dir;
  int sx = 8, sz = 8, ground = 64;
  float yaw = 0.0F, pitch = 0.0F;
  std::map<std::pair<int, int>, craftpp::render::Tessellator> tess_map;
  // Climate prefetch for tinting (set by start_game, used by remeshing).
  std::function<void(int, int)> refill_tint = [](int, int) {};
  // Async world build (loading screen): gen queue then populate queue.
  std::vector<std::pair<int, int>> load_gen, load_pop;
  int load_phase = 0;  // 0 gen, 1 spawn+player, 2 populate+mesh
  std::string pending_name;
  int pending_mode = 0;
  bool pending_hardcore = false;
  std::optional<craftpp::world::WorldInfoData> pending_info;
  // Per-frame input edges.
  double last_x = 0.0, last_y = 0.0;
  bool have_mouse = false;
  bool lmb_was = false, rmb_was = false, g_was = false, esc_was = false;
  double accumulator = 0.0;
  int tick_count = 0;
  double tps_window = 0.0;
  int tps_ticks = 0;
};

}  // namespace

int main(int argc, char** argv) {
  static volatile std::sig_atomic_t quit_flag = 0;
  std::signal(SIGTERM, [](int) { quit_flag = 1; });
  std::signal(SIGINT, [](int) { quit_flag = 1; });
  const Args args = parse_args(argc, argv);

  if (glfwInit() == GLFW_FALSE) {
    craftpp::log_error("glfw init failed");
    return 1;
  }
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  GLFWwindow* window = glfwCreateWindow(854, 480, "Craft++", nullptr, nullptr);
  if (window == nullptr) {
    craftpp::log_error("window failed");
    glfwTerminate();
    return 1;
  }
  glfwMakeContextCurrent(window);
  glfwSetScrollCallback(window, scroll_callback);
  glfwSetCharCallback(window, char_callback);
  glfwSetKeyCallback(window, key_callback);

  // --- assets (local, never committed) ---
  craftpp::render::Image atlas_img;
  std::string err;
  if (!craftpp::render::load_png((args.assets + "/terrain.png").c_str(), atlas_img, err)) {
    craftpp::log_error("cannot load terrain.png: " + err);
    return 1;
  }
  craftpp::render::Mesher mesher;
  craftpp::render::Image grass_map, foliage_map;
  if (!craftpp::render::load_png((args.assets + "/misc/grasscolor.png").c_str(), grass_map,
                                 err)) {
    craftpp::log_error("cannot load grasscolor.png: " + err);
    return 1;
  }
  if (!craftpp::render::load_png((args.assets + "/misc/foliagecolor.png").c_str(), foliage_map,
                                 err)) {
    craftpp::log_error("cannot load foliagecolor.png: " + err);
    return 1;
  }
  auto lang = craftpp::gui::load_lang(args.assets + "/lang/en_US.lang");
  craftpp::gui::ScreenCtx sctx;
  sctx.saves_root = args.saves;
  sctx.assets_root = args.assets;
  sctx.lang = lang;

  int exit_code = 0;
  {
    craftpp::render::Texture atlas, pig_tex, zombie_tex, gui_tex, icons_tex, font_tex, items_tex,
        bg_tex, logo_tex;
    craftpp::gui::Font font;
    auto must_load = [&](const std::string& name, craftpp::render::Texture& tex, int w = 0,
                         int h = 0) {
      craftpp::render::Image img;
      if (!craftpp::render::load_png((args.assets + name).c_str(), img, err) ||
          (w > 0 && (img.width != w || img.height != h)) || !tex.upload_nearest(img)) {
        craftpp::log_error("cannot load " + name + ": " + err);
        exit_code = 1;
      }
    };
    must_load("/terrain.png", atlas);
    must_load("/mob/pig.png", pig_tex, 64, 32);
    must_load("/mob/zombie.png", zombie_tex, 64, 32);
    must_load("/gui/gui.png", gui_tex);
    must_load("/gui/icons.png", icons_tex);
    must_load("/gui/background.png", bg_tex);
    must_load("/title/mclogo.png", logo_tex);
    must_load("/gui/items.png", items_tex);
    {
      craftpp::render::Image img;
      if (!craftpp::render::load_png((args.assets + "/font/default.png").c_str(), img, err) ||
          !font.load_glyphs(img.rgba.data(), img.width, img.height) ||
          !font_tex.upload_nearest(img) || !font.load_allowed(args.assets + "/font.txt")) {
        craftpp::log_error("cannot load font: " + err);
        exit_code = 1;
      }
    }
    if (exit_code != 0) return 1;

    craftpp::render::ShaderProgram terrain_prog, flat_prog;
    if (!terrain_prog.link(craftpp::render::kTerrainVert, craftpp::render::kTerrainFrag, err) ||
        !flat_prog.link(kFlatVert, kFlatFrag, err)) {
      craftpp::log_error("shader link failed: " + err);
      return 1;
    }
    const int t_mvp = terrain_prog.uniform("u_mvp");
    const int t_view = terrain_prog.uniform("u_view");
    const int t_fog_start = terrain_prog.uniform("u_fog_start");
    const int t_fog_end = terrain_prog.uniform("u_fog_end");
    const int t_fog_color = terrain_prog.uniform("u_fog_color");
    const int t_tex = terrain_prog.uniform("u_tex");
    const int f_mvp = flat_prog.uniform("u_mvp");
    const int f_bright = flat_prog.uniform("u_bright");
    craftpp::render::Frustum frustum;

    // ---- UI + session state ----
    craftpp::gui::ScreenUi ui;
    ui.width = 854;
    ui.height = 480;
    // Random splash (excluding the missingno hash like the source).
    {
      std::ifstream splash_in(args.assets + "/title/splashes.txt");
      std::string line;
      std::vector<std::string> options;
      while (std::getline(splash_in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) options.push_back(line);
      }
      if (!options.empty()) {
        std::srand(static_cast<unsigned>(std::time(nullptr)));
        do {
          ui.splash = options[std::rand() % options.size()];
        } while (static_cast<int>(craftpp::gui::java_string_hash(ui.splash)) == 125780783);
      }
    }
    std::unique_ptr<Session> game;
    bool quit_app = false;
    int last_row_click = -1;
    double last_row_time = 0.0;
    int frames = 0;
    bool screenshotted = false;
    auto set_screen_cursor = [&]() {
      glfwSetInputMode(window, GLFW_CURSOR,
                       ui.cur == craftpp::gui::Screen::None ? GLFW_CURSOR_DISABLED
                                                           : GLFW_CURSOR_NORMAL);
    };

    std::function<void(const std::string&, long, const std::string&, int, bool)> start_game;
    std::function<void(bool)> stop_to_title;

    auto apply_difficulty = [&]() {
      if (game && game->world) {
        game->world->set_spawn_flags(ui.difficulty > 0, true);
      }
    };

    auto do_respawn = [&]() {
      if (!game) return;
      LiveWorld& world = *game->world;
      PlayerSP& player = *game->player;
      for (auto& s : player.inventory.main) {
        if (s.has_value() && s->stack_size > 0) {
          world.on_item_drop(s->item_id, s->stack_size, s->damage, player.pos_x, player.pos_y + 1.0,
                             player.pos_z, 0.0, 0.0, 0.0);
        }
        s = std::nullopt;
      }
      for (auto& s : player.inventory.armor) s = std::nullopt;
      player.health = 20;
      player.food = craftpp::entity::FoodStats();
      player.fire = 0;
      player.air_supply = 300;
      player.motion_x = player.motion_y = player.motion_z = 0.0;
      player.fall_distance = 0.0f;
      player.death_time = 0;
      player.is_dead = false;
      player.set_position_and_rotation(game->sx + 0.5, game->ground + 12.0 + 1.62, game->sz + 0.5,
                                      0.0f, 0.0f);
      craftpp::log_info("respawned (drops left where you died)");
    };

    start_game = [&](const std::string& save_dir, long seed, const std::string& level_name,
                     int mode_idx, bool hardcore) {
      auto info = craftpp::world::read_level_dat(save_dir);
      auto session = std::make_unique<Session>();
      session->save_dir = save_dir;
      session->world = std::make_unique<LiveWorld>(info.has_value() ? info->seed : seed);
      LiveWorld& world = *session->world;
      struct TintCache {
        int ox = INT_MAX, oz = INT_MAX;
        float t[18][18] = {};
        float h[18][18] = {};
      };
      auto tint_cache = std::make_shared<TintCache>();
      auto refill_tint = [tint_cache, &world](int cx, int cz) {
        std::vector<float> t, h;
        world.climate_rect(cx * 16 - 1, cz * 16 - 1, 18, 18, t, h);
        for (int dz = 0; dz < 18; ++dz)
          for (int dx = 0; dx < 18; ++dx) {
            tint_cache->t[dz][dx] = t[dz * 18 + dx];
            tint_cache->h[dz][dx] = h[dz * 18 + dx];
          }
        tint_cache->ox = cx * 16 - 1;
        tint_cache->oz = cz * 16 - 1;
      };
      mesher.tint = [tint_cache, &grass_map, &foliage_map](int x, int z, bool foliage, float& r,
                                                           float& g, float& b) {
        const int dx = x - tint_cache->ox, dz = z - tint_cache->oz;
        float t = 0.5F, h = 1.0F;
        if (dx >= 0 && dx < 18 && dz >= 0 && dz < 18) {
          t = tint_cache->t[dz][dx];
          h = tint_cache->h[dz][dx];
        }
        if (!craftpp::render::sample_colormap(foliage ? foliage_map : grass_map, t, h, r, g,
                                              b)) {
          r = g = b = 1.0F;
        }
      };
      session->refill_tint = refill_tint;
      if (info.has_value()) {
        world.load(save_dir);
        craftpp::log_info("loaded save from " + save_dir);
      } else {
        world.set_level_name(level_name);
        world.set_game_type(mode_idx == 2 ? 1 : 0);
        world.set_hardcore(hardcore);
      }
      // Async build (loading screen): gen queue, then populate+mesh queue.
      session->load_gen.clear();
      session->load_pop.clear();
      for (int cz = -1; cz <= 1; ++cz)
        for (int cx = -1; cx <= 1; ++cx) {
          if (!world.is_provided(cx, cz)) session->load_gen.emplace_back(cx, cz);
          if (!world.is_populated(cx, cz)) session->load_pop.emplace_back(cx, cz);
        }
      session->load_phase = 0;
      session->pending_name = level_name;
      session->pending_mode = mode_idx;
      session->pending_hardcore = hardcore;
      session->pending_info = info;
      game = std::move(session);
      ui.loading_title =
          info.has_value() ? "Loading level" : "Generating level";  // (lang has no key)
      ui.loading_sub = "Building terrain";
      ui.loading_progress = game->load_gen.empty() && game->load_pop.empty() ? 100 : 0;
      craftpp::gui::open_screen(ui, craftpp::gui::Screen::Loading, sctx);
      set_screen_cursor();
    };

    // One loading step per frame (gen, then spawn+populate+mesh). Returns
    // true while work remains.
    std::function<bool()> pump_loading;
    pump_loading = [&]() {
      if (!game) return false;
      LiveWorld& world = *game->world;
      const int total = 9 + 9;
      const int done = (9 - (int)game->load_gen.size()) + (9 - (int)game->load_pop.size());
      if (!game->load_gen.empty()) {
        auto [cx, cz] = game->load_gen.back();
        game->load_gen.pop_back();
        world.gen_chunk(cx, cz);
        ui.loading_progress = (done + 1) * 100 / total;
        return true;
      }
      if (game->load_phase == 0) {
        // Spawn search + player + controllers (needs finished terrain).
        game->load_phase = 1;
        auto surface_at = [&](int x, int z) {
          for (int y = 127; y > 0; --y) {
            const int id = world.block_id(x, y, z);
            if (id != 0 && id != 9) return y;
          }
          return 63;
        };
        int sx = 8, sz = 8;
        for (int ox = -8; ox <= 8; ++ox) {
          for (int oz = -8; oz <= 8; ++oz) {
            const int h0 = surface_at(8 + ox, 8 + oz);
            bool flat = true;
            for (int ax = -1; ax <= 1 && flat; ++ax)
              for (int az = -1; az <= 1 && flat; ++az)
                if (surface_at(8 + ox + ax, 8 + oz + az) != h0) flat = false;
            if (flat) {
              sx = 8 + ox;
              sz = 8 + oz;
              ox = 9;
              break;
            }
          }
        }
        game->sx = sx;
        game->sz = sz;
        game->ground = surface_at(sx, sz);
        game->player = std::make_unique<PlayerSP>(&world, "Player", 0);
        PlayerSP& player = *game->player;
        world.add_entity(&player);
        bool restored = false;
        if (game->pending_info.has_value() && game->pending_info->player.has_value()) {
          craftpp::world::apply_player_tag(player, *game->pending_info->player);
          restored = true;
        }
        if (!restored) {
          world.set_spawn_point(sx + 0.5, game->ground, sz + 0.5);
          player.set_position_and_rotation(sx + 0.5, game->ground + 12.0 + 1.62, sz + 0.5, 0.0f,
                                          0.0f);
        }
        game->csp = std::make_unique<ControllerSP>(world, player);
        game->ccr = std::make_unique<ControllerCreative>(world, player);
        game->controller = game->csp.get();
        game->creative = (game->pending_mode == 2) ||
                         (game->pending_info.has_value() && game->pending_info->game_type == 1);
        if (game->creative) {
          ControllerCreative::enable_creative(player);
          game->controller = game->ccr.get();
        }
        apply_difficulty();
        ui.loading_progress = done * 100 / total;
        return true;
      }
      if (!game->load_pop.empty()) {
        auto [cx, cz] = game->load_pop.back();
        game->load_pop.pop_back();
        world.populate_one(cx, cz);
        game->refill_tint(cx, cz);
        game->tess_map[{cx, cz}].upload(mesher.mesh_live(world.region(), cx, cz));
        world.clear_dirty(cx, cz);
        ui.loading_progress = (done + 1) * 100 / total;
        return !game->load_pop.empty();
      }
      game->      load_phase = 2;
      // Mesh every provided chunk (loaded saves skip populate entirely,
      // so their meshes would otherwise never be built).
      for (const auto& [cx, cz] : world.provided_chunks()) {
        if (game->tess_map.count({cx, cz}) != 0) continue;
        game->refill_tint(cx, cz);
        game->tess_map[{cx, cz}].upload(mesher.mesh_live(world.region(), cx, cz));
        world.clear_dirty(cx, cz);
      }
      craftpp::gui::open_screen(ui, craftpp::gui::Screen::None, sctx);
      set_screen_cursor();
      craftpp::log_info(
          "controls: WASD move, mouse look, Space jump, Shift sneak, LMB mine, "
          "RMB place held, 1-9 hotbar + wheel, G creative, ESC menu");
      return false;
    };

    stop_to_title = [&](bool save) {
      if (game && save && !game->save_dir.empty()) {
        game->world->save(game->save_dir, game->player.get());
        craftpp::log_info("saved world to " + game->save_dir);
      }
      game.reset();
      craftpp::gui::open_screen(ui, craftpp::gui::Screen::Main, sctx);
      set_screen_cursor();
    };

    // ---- button dispatch (vanilla ids per screen) ----
    std::function<void(int)> press_button;
    press_button = [&](int id) {
      using S = craftpp::gui::Screen;
      const auto& L = sctx.lang;
      switch (ui.cur) {
        case S::Main:
          if (id == 1) craftpp::gui::open_screen(ui, S::Select, sctx);
          if (id == 2) craftpp::gui::open_screen(ui, S::Multi, sctx);
          if (id == 0) {
            ui.parent = S::Main;
            craftpp::gui::open_screen(ui, S::Options, sctx);
          }
          if (id == 4) quit_app = true;
          break;
        case S::Select:
          if (id == 1 && ui.selected >= 0) {
            start_game(sctx.saves_root + "/" + ui.worlds[ui.selected].dir, 0, "", 0, false);
          }
          if (id == 3) craftpp::gui::open_screen(ui, S::Create, sctx);
          if (id == 0) craftpp::gui::open_screen(ui, S::Main, sctx);
          if (id == 6 && ui.selected >= 0) {
            ui.rename_field.text = ui.worlds[ui.selected].title;
            ui.rename_field.focused = true;
            craftpp::gui::open_screen(ui, S::Rename, sctx);
          }
          if (id == 2 && ui.selected >= 0) {
            ui.confirm_title = craftpp::gui::tr(L, "selectWorld.deleteQuestion");
            ui.confirm_line = "'" + ui.worlds[ui.selected].title + "' " +
                              craftpp::gui::tr(L, "selectWorld.deleteWarning");
            ui.confirm_id = 1;
            craftpp::gui::open_screen(ui, S::Confirm, sctx);
          }
          break;
        case S::Create:
          if (id == 1) craftpp::gui::open_screen(ui, S::Select, sctx);
          if (id == 2) {
            ui.mode_idx = (ui.mode_idx + 1) % 3;
            craftpp::gui::refresh_create_labels(ui, sctx);
          }
          if (id == 3) {
            ui.more = !ui.more;
            craftpp::gui::refresh_create_labels(ui, sctx);
          }
          if (id == 4) {
            ui.features = !ui.features;
            craftpp::gui::refresh_create_labels(ui, sctx);
          }
          if (id == 5) {
            ui.notice = "Only Normal type (M5)";
            craftpp::gui::refresh_create_labels(ui, sctx);
          }
          if (id == 0) {
            std::error_code ec;
            std::filesystem::create_directories(sctx.saves_root, ec);
            ui.folder = craftpp::gui::make_world_folder(sctx.saves_root, ui.name_field.text);
            const std::string dir = sctx.saves_root + "/" + ui.folder;
            std::filesystem::create_directories(dir, ec);
            long seed = craftpp::gui::parse_seed(
                ui.seed_field.text, static_cast<long>(std::time(nullptr)));
            const bool hardcore = ui.mode_idx == 1;
            start_game(dir, seed, ui.name_field.text, ui.mode_idx, hardcore);
          }
          break;
        case S::Multi:
          if (id == 0) craftpp::gui::open_screen(ui, S::Main, sctx);
          if (id == 1 || id == 4) ui.notice = "Multiplayer lands with M7";
          if (id == 3) {
            ui.servers.push_back("Local server :25565 (M7)");
            ui.notice.clear();
          }
          if (id == 2 && ui.server_sel >= 0 &&
              ui.server_sel < (int)ui.servers.size()) {
            ui.servers.erase(ui.servers.begin() + ui.server_sel);
            ui.server_sel = -1;
          }
          if (id == 7 && ui.server_sel >= 0) ui.notice = "Edit lands with M7";
          if (id == 8) ui.notice.clear();
          break;
        case S::Options:
          if (id == 200) {
            apply_difficulty();
            craftpp::gui::open_screen(ui, ui.parent, sctx);
          }
          if (id == 25) {
            ui.difficulty = (ui.difficulty + 1) % 4;
            craftpp::gui::refresh_options_labels(ui, sctx);
            apply_difficulty();
          }
          if (id == 22) {
            ui.invert = !ui.invert;
            craftpp::gui::refresh_options_labels(ui, sctx);
          }
          if (id == 100 || id == 101) {
            ui.placeholder_title = id == 100 ? "Controls (M5)" : "Video Settings (M5)";
            ui.notice = "Key bindings land with GameSettings backend";
            ui.parent = S::Options;
            craftpp::gui::open_screen(ui, S::Placeholder, sctx);
          }
          break;
        case S::Ingame:
          if (id == 4) {
            craftpp::gui::open_screen(ui, S::None, sctx);
            set_screen_cursor();
          }
          if (id == 0) {
            ui.parent = S::Ingame;
            craftpp::gui::open_screen(ui, S::Options, sctx);
          }
          if (id == 1) stop_to_title(true);
          if (id == 5 || id == 6) {
            ui.placeholder_title = id == 5 ? "Achievements (M5)" : "Statistics (M5)";
            ui.notice = "Stats backend lands with M5 leftovers";
            ui.parent = S::Ingame;
            craftpp::gui::open_screen(ui, S::Placeholder, sctx);
          }
          break;
        case S::GameOver:
          if (id == 1 && !ui.hardcore) do_respawn();
          if (id == 1 && ui.hardcore && game) {
            std::error_code ec;
            std::filesystem::remove_all(game->save_dir, ec);
            stop_to_title(false);
            break;
          }
          if (id == 1) {
            craftpp::gui::open_screen(ui, S::None, sctx);
            set_screen_cursor();
          }
          if (id == 2) stop_to_title(true);
          break;
        case S::Confirm:
          if (id == 1 && ui.confirm_id == 1 && ui.selected >= 0) {
            std::error_code ec;
            std::filesystem::remove_all(
                sctx.saves_root + "/" + ui.worlds[ui.selected].dir, ec);
          }
          craftpp::gui::open_screen(ui, S::Select, sctx);
          break;
        case S::Rename: {
          if (id == 0 && ui.selected >= 0) {
            auto info = craftpp::world::read_level_dat(sctx.saves_root + "/" +
                                                       ui.worlds[ui.selected].dir);
            if (info.has_value() && !ui.rename_field.text.empty()) {
              info->level_name = ui.rename_field.text;
              craftpp::world::write_level_dat(sctx.saves_root + "/" + ui.worlds[ui.selected].dir,
                                             *info);
            }
          }
          craftpp::gui::open_screen(ui, S::Select, sctx);
          break;
        }
        case S::Placeholder:
          if (id == 0) craftpp::gui::open_screen(ui, ui.parent, sctx);
          break;
        case S::None:
          break;
      }
      set_screen_cursor();
    };

    auto focused_field = [&]() -> craftpp::gui::TextField* {
      using S = craftpp::gui::Screen;
      if (ui.cur == S::Create) {
        if (ui.name_field.focused) return &ui.name_field;
        if (ui.seed_field.focused) return &ui.seed_field;
      }
      if (ui.cur == S::Rename && ui.rename_field.focused) return &ui.rename_field;
      return nullptr;
    };

    // ---- boot: direct save/seed skips the title (headless friendly) ----
    if (!args.save_dir.empty() || args.seed != 1) {
      start_game(args.save_dir.empty() ? (args.saves + "/World1") : args.save_dir, args.seed,
                 "World1", 0, false);
    } else {
      craftpp::gui::open_screen(ui, craftpp::gui::Screen::Main, sctx);
      set_screen_cursor();
    }

    craftpp::log_info("ESC opens the game menu; text fields take keyboard input");

    using clock = std::chrono::steady_clock;
    auto last = clock::now();
    constexpr double kTick = 1.0 / 20.0;
    bool lmb_down = false;

    while (glfwWindowShouldClose(window) == GLFW_FALSE && !quit_app) {
      const auto now = clock::now();
      double frame = std::chrono::duration<double>(now - last).count();
      last = now;
      if (frame > 0.25) frame = 0.25;
      int w = 0, h = 0;
      glfwGetFramebufferSize(window, &w, &h);
      if (w != ui.width || h != ui.height) {
        ui.width = w;
        ui.height = h;
        if (ui.cur != craftpp::gui::Screen::None) {
          const auto cur = ui.cur;
          craftpp::gui::open_screen(ui, cur, sctx);
        }
      }
      glViewport(0, 0, w, h);

      // ---- input routing ----
      double mx = 0.0, my = 0.0;
      glfwGetCursorPos(window, &mx, &my);
      ui.mouse_x = mx;
      ui.mouse_y = my;
      const bool esc = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
      const bool lmb = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

      if (ui.cur != craftpp::gui::Screen::None) {
        // Loading screen pumps the async world build (one chunk/frame).
        if (ui.cur == craftpp::gui::Screen::Loading) {
          pump_loading();
          lmb_down = lmb;
          g_chars.clear();
          g_backspace = false;
          g_enter = false;
          g_wheel = 0;
        } else {
        // Title/menu branch (game world frozen behind Ingame).
        // Slider drag.
        if (lmb && !lmb_down) {
          const int sid = craftpp::gui::slider_at(ui, mx, my);
          if (sid >= 0) ui.drag_id = sid;
        }
        if (!lmb) ui.drag_id = -1;
        if (ui.drag_id >= 0) {
          for (const auto& b : ui.buttons) {
            if (b.id == ui.drag_id) {
              craftpp::gui::apply_slider(ui, b.id,
                                        static_cast<float>((mx - b.x - 4) / (b.w - 8)));
              craftpp::gui::refresh_options_labels(ui, sctx);
            }
          }
        }
        // Clicks (edge).
        if (lmb && !lmb_down) {
          // Text field focus (create + rename screens).
          if (ui.cur == craftpp::gui::Screen::Create) {
            ui.name_field.focused =
                (mx >= ui.width / 2 - 100 && mx < ui.width / 2 + 100 && my >= 60 && my < 80);
            ui.seed_field.focused =
                (ui.more && mx >= ui.width / 2 - 100 && mx < ui.width / 2 + 100 && my >= 108 &&
                 my < 128);
          }
          if (ui.cur == craftpp::gui::Screen::Rename) {
            ui.rename_field.focused =
                (mx >= ui.width / 2 - 100 && mx < ui.width / 2 + 100 && my >= 60 && my < 80);
          }
          // World/server rows.
          if (ui.cur == craftpp::gui::Screen::Select) {
            const int row = static_cast<int>(my - 32) / 36;
            if (row >= 0 && row < (int)ui.worlds.size() && my < h - 64) {
              const double t = now.time_since_epoch().count() / 1e9;
              if (row == last_row_click && t - last_row_time < 0.5 && ui.selected == row) {
                ui.selected = row;
                press_button(1);  // double-click plays
              } else {
                ui.selected = row;
                last_row_click = row;
                last_row_time = t;
              }
            }
          }
          if (ui.cur == craftpp::gui::Screen::Multi) {
            const int row = static_cast<int>(my - 32) / 36;
            if (row >= 0 && row < (int)ui.servers.size() && my < h - 64) ui.server_sel = row;
          }
          const int id = craftpp::gui::button_at(ui, mx, my);
          if (id >= 0) press_button(id);
        }
        lmb_down = lmb;
        // Text input.
        if (auto* f = focused_field()) {
          for (char c : g_chars) {
            if (c >= 32 && c < 127 && (int)f->text.size() < f->max_len) f->text += c;
          }
          if (g_backspace && !f->text.empty()) f->text.pop_back();
          if (ui.cur == craftpp::gui::Screen::Create) {
            ui.folder = craftpp::gui::make_world_folder(sctx.saves_root, ui.name_field.text);
          }
        }
        if (g_enter) {
          if (ui.cur == craftpp::gui::Screen::Create) press_button(0);
          if (ui.cur == craftpp::gui::Screen::Rename) press_button(0);
        }
        // ESC backs out (edge).
        if (game && esc && !game->esc_was) {
          using S = craftpp::gui::Screen;
          if (ui.cur == S::Ingame) press_button(4);
          if (ui.cur == S::Select) press_button(0);
          if (ui.cur == S::Create) press_button(1);
          if (ui.cur == S::Multi) press_button(0);
          if (ui.cur == S::Options) {
            apply_difficulty();
            craftpp::gui::open_screen(ui, ui.parent, sctx);
            set_screen_cursor();
          }
          if (ui.cur == S::Confirm) craftpp::gui::open_screen(ui, S::Select, sctx);
          if (ui.cur == S::Rename) press_button(1);
          if (ui.cur == S::Placeholder) press_button(0);
        }
        }  // end non-loading menu branch
        if (game) game->esc_was = esc;
        g_chars.clear();
        g_backspace = false;
        g_enter = false;
        g_wheel = 0;
      }  // end menu branch
      if (game && ui.cur == craftpp::gui::Screen::None) {
        // ---- live game branch (existing client logic) ----
        LiveWorld& world = *game->world;
        PlayerSP& player = *game->player;
        if (esc && !game->esc_was) {
          craftpp::gui::open_screen(ui, craftpp::gui::Screen::Ingame, sctx);
          set_screen_cursor();
        }
        game->esc_was = esc;
        if (ui.cur != craftpp::gui::Screen::None) {
          // (menu opened above: skip game input this frame)
        } else {
          // Mouse look.
          if (!game->have_mouse) {
            game->last_x = mx;
            game->last_y = my;
            game->have_mouse = true;
          }
          const float sens = 0.15F * (0.5F + ui.sensitivity);
          game->yaw += static_cast<float>(mx - game->last_x) * sens;
          float dpitch = static_cast<float>(my - game->last_y) * sens;
          if (ui.invert) dpitch = -dpitch;
          game->pitch += dpitch;
          if (game->pitch < -89.9f) game->pitch = -89.9f;
          if (game->pitch > 89.9f) game->pitch = 89.9f;
          game->last_x = mx;
          game->last_y = my;
          player.rotation_yaw = game->yaw;
          player.rotation_pitch = game->pitch;

          auto& in = player.movement_input;
          in->move_forward =
              (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS ? 1.0f : 0.0f) -
              (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS ? 1.0f : 0.0f);
          in->move_strafe =
              (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS ? 1.0f : 0.0f) -
              (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS ? 1.0f : 0.0f);
          in->jump = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
          in->sneak = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;

          game->accumulator += frame;
          while (game->accumulator >= kTick) {
            const float yr = game->yaw * 3.14159265f / 180.0f;
            const float pr = game->pitch * 3.14159265f / 180.0f;
            const double lx = -std::sin(yr) * std::cos(pr);
            const double ly = -std::sin(pr);
            const double lz = std::cos(yr) * std::cos(pr);
            const double ex = player.pos_x;
            const double ey = player.pos_y + 0.12;
            const double ez = player.pos_z;
            int hx = 0, hy = 0, hz = 0, side = 0;
            const bool hit = pick_block(world, ex, ey, ez, lx, ly, lz, 4.0, hx, hy, hz, side);
            const bool rmb = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
            craftpp::entity::Living* foe = nullptr;
            double foe_d2 = 16.0;
            for (auto& m : world.mobs()) {
              if (!m || m->is_dead) continue;
              const double fmx = m->pos_x - ex, fmy = (m->pos_y + m->height * 0.5) - ey,
                           fmz = m->pos_z - ez;
              const double d2 = fmx * fmx + fmy * fmy + fmz * fmz;
              if (d2 >= foe_d2) continue;
              const double len = std::sqrt(d2);
              if (len < 1e-6) continue;
              if ((fmx * lx + fmy * ly + fmz * lz) / len < 0.85) continue;
              foe = m.get();
              foe_d2 = d2;
            }
            if (foe != nullptr && lmb && !game->lmb_was) {
              player.attack_target(*foe);
            } else {
              if (hit && lmb && !game->lmb_was)
                game->controller->click_block(hx, hy, hz, side);
              if (hit && lmb) game->controller->send_block_removing(hx, hy, hz, side);
              if (!lmb) game->controller->reset_block_removing();
            }
            if (hit && rmb && !game->rmb_was) {
              if (auto* held = player.inventory.held()) {
                if (held->has_value())
                  game->controller->send_place_block(held->value(), hx, hy, hz, side);
              }
            }
            game->lmb_was = lmb;
            game->rmb_was = rmb;
            const bool g_now = glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS;
            if (g_now && !game->g_was) {
              game->creative = !game->creative;
              if (game->creative) {
                ControllerCreative::enable_creative(player);
                game->controller = game->ccr.get();
                craftpp::log_info("creative mode (fly: double-Space, instabreak)");
              } else {
                ControllerCreative::disable_creative(player);
                game->controller = game->csp.get();
                craftpp::log_info("survival mode");
              }
            }
            game->g_was = g_now;
            for (int k = 0; k < 9; ++k) {
              if (glfwGetKey(window, GLFW_KEY_1 + k) == GLFW_PRESS)
                player.inventory.current = k;
            }
            while (g_wheel > 0) {
              player.inventory.current = (player.inventory.current + 8) % 9;
              --g_wheel;
            }
            while (g_wheel < 0) {
              player.inventory.current = (player.inventory.current + 1) % 9;
              ++g_wheel;
            }

            world.tick();
            if (!game->creative) game->csp->update_controller();
            // Death opens the death screen (no auto-respawn).
            if (player.is_dead && ui.cur == craftpp::gui::Screen::None) {
              ui.score = player.score;
              ui.hardcore = world.hardcore();
              craftpp::gui::open_screen(ui, craftpp::gui::Screen::GameOver, sctx);
              set_screen_cursor();
            }
            ++game->tick_count;
            ++game->tps_ticks;
            if (game->tick_count % 20 == 0) {
              int dir = static_cast<int>(std::floor(game->yaw / 90.0F + 0.5F)) % 4;
              if (dir < 0) dir += 4;
              const char* face = dir == 0 ? "S" : (dir == 1 ? "W" : (dir == 2 ? "N" : "E"));
              char buf[192];
              std::snprintf(buf, sizeof buf, "pos %.1f %.1f %.1f face %s (%.0f) onGround %d hp %d tps %.1f",
                            player.pos_x, player.pos_y, player.pos_z, face, game->yaw,
                            (int)player.on_ground, player.health,
                            game->tps_ticks / (game->tps_window > 0.0 ? game->tps_window : 1.0));
              craftpp::log_info(buf);
              game->tps_window = 0.0;
              game->tps_ticks = 0;
            }
            game->accumulator -= kTick;
          }
          game->tps_window += frame;
        }
        g_chars.clear();
        g_backspace = false;
        g_enter = false;
      }

      // ---- render ----
      const float daylight = game ? game->world->daylight() : 1.0F;
      glClearColor(0.74F * daylight, 0.84F * daylight, 1.0F * daylight, 1.0F);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      if (game && game->player) {
        LiveWorld& world = *game->world;
        PlayerSP& player = *game->player;
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        const float yr = game->yaw * 3.14159265f / 180.0f;
        const float pr = game->pitch * 3.14159265f / 180.0f;
        glm::vec3 eye(player.pos_x, player.pos_y + 0.12, player.pos_z);
        glm::vec3 look(-std::sin(yr) * std::cos(pr), -std::sin(pr), std::cos(yr) * std::cos(pr));
        glm::mat4 view = glm::lookAt(eye, eye + look, glm::vec3(0.0F, 1.0F, 0.0F));
        glm::mat4 proj =
            glm::perspective(glm::radians(ui.fov), static_cast<float>(w) / h, 0.1F, 256.0F);
        glm::mat4 vp = proj * view;
        frustum.update(&vp[0][0]);

        terrain_prog.use();
        terrain_prog.set_mat4(t_mvp, &vp[0][0]);
        terrain_prog.set_mat4(t_view, &view[0][0]);
        terrain_prog.set_float(t_fog_start, 60.0F);
        terrain_prog.set_float(t_fog_end, 220.0F);
        terrain_prog.set_vec3(t_fog_color, 0.74F * daylight, 0.84F * daylight, 1.0F * daylight);
        terrain_prog.set_int(t_tex, 0);
        atlas.bind(0);
        for (auto& [key, tess] : game->tess_map) {
          if (world.is_dirty(key.first, key.second)) {
            game->refill_tint(key.first, key.second);
            tess.upload(mesher.mesh_live(world.region(), key.first, key.second));
            world.clear_dirty(key.first, key.second);
          }
          const craftpp::Aabb box(key.first * 16.0, 0.0, key.second * 16.0,
                                  key.first * 16.0 + 16.0, 128.0, key.second * 16.0 + 16.0);
          if (frustum.box_visible(box)) tess.draw();
        }

        // Crack overlay while mining.
        if (!game->creative && ui.cur == craftpp::gui::Screen::None) {
          int tx = 0, ty = 0, tz = 0;
          if (game->csp->damage_target(tx, ty, tz)) {
            int stage = static_cast<int>(game->csp->cur_damage() * 10.0F);
            if (stage < 0) stage = 0;
            if (stage > 9) stage = 9;
            craftpp::render::Mesh crack;
            add_crack_cube(crack, tx, ty, tz, 240 + stage);
            craftpp::render::Tessellator ctess;
            ctess.upload(crack);
            ctess.draw();
          }
        }

        // Mobs as textured code models; drops stay boxes.
        glDisable(GL_CULL_FACE);
        for (auto& m : world.mobs()) {
          if (!m || m->is_dead) continue;
          const bool pig = dynamic_cast<craftpp::entity::Pig*>(m.get()) != nullptr;
          const float moving = std::abs(m->move_forward) > 0.01F ? 1.0F : 0.0F;
          craftpp::render::Mesh mm;
          if (pig) {
            mm = craftpp::render::entity_mesh(
                craftpp::render::pig_parts(m->distance_walked, moving, 0.0F, 0.0F), 64, 32,
                180.0F - m->rotation_yaw, 1.0F);
            pig_tex.bind(0);
          } else {
            mm = craftpp::render::entity_mesh(
                craftpp::render::zombie_parts(m->distance_walked, moving, 0.0F, m->ticks_existed,
                                              0.0F, 0.0F),
                64, 32, 180.0F - m->rotation_yaw, 1.0F);
            zombie_tex.bind(0);
          }
          for (auto& v : mm.vertices) {
            v.x += static_cast<float>(m->pos_x);
            v.y += static_cast<float>(m->pos_y);
            v.z += static_cast<float>(m->pos_z);
          }
          terrain_prog.use();
          terrain_prog.set_mat4(t_mvp, &vp[0][0]);
          terrain_prog.set_mat4(t_view, &view[0][0]);
          terrain_prog.set_float(t_fog_start, 60.0F);
          terrain_prog.set_float(t_fog_end, 220.0F);
          terrain_prog.set_vec3(t_fog_color, 0.74F * daylight, 0.84F * daylight, 1.0F * daylight);
          terrain_prog.set_int(t_tex, 0);
          craftpp::render::Tessellator mtess;
          mtess.upload(mm);
          mtess.draw();
        }
        glEnable(GL_CULL_FACE);
        flat_prog.use();
        flat_prog.set_mat4(f_mvp, &vp[0][0]);
        flat_prog.set_float(f_bright, daylight);
        {
          craftpp::render::Mesh em;
          auto box = [&](double ccx, double ccy, double ccz, double hw, double hh, float r,
                         float g, float b) {
            const float x0 = ccx - hw, x1 = ccx + hw, y0 = ccy, y1 = ccy + hh, z0 = ccz - hw,
                        z1 = ccz + hw;
            add_quad(em, x0, y1, z1, x1, y1, z1, x1, y1, z0, x0, y1, z0, r, g, b);
            add_quad(em, x0, y0, z0, x1, y0, z0, x1, y0, z1, x0, y0, z1, r * 0.5f, g * 0.5f,
                     b * 0.5f);
            add_quad(em, x1, y1, z0, x0, y1, z0, x0, y0, z0, x1, y0, z0, r * 0.8f, g * 0.8f,
                     b * 0.8f);
            add_quad(em, x0, y1, z1, x1, y1, z1, x1, y0, z1, x0, y0, z1, r * 0.8f, g * 0.8f,
                     b * 0.8f);
            add_quad(em, x0, y1, z1, x0, y1, z0, x0, y0, z0, x0, y0, z1, r * 0.6f, g * 0.6f,
                     b * 0.6f);
            add_quad(em, x1, y1, z0, x1, y1, z1, x1, y0, z1, x1, y0, z0, r * 0.6f, g * 0.6f,
                     b * 0.6f);
          };
          for (auto& it : world.items()) {
            if (!it || it->is_dead) continue;
            box(it->pos_x, it->pos_y, it->pos_z, 0.12, 0.25, 0.95f, 0.85f, 0.3f);
          }
          if (!em.vertices.empty()) {
            craftpp::render::Tessellator etess;
            etess.upload(em);
            etess.draw();
          }
        }

        // HUD (hidden under menus).
        if (ui.cur == craftpp::gui::Screen::None) {
          craftpp::gui::HudState hs;
          hs.width = w;
          hs.height = h;
          hs.tick = game->tick_count;
          hs.survival_hud = !game->creative;
          hs.health = player.health;
          hs.food = player.food.food_level;
          hs.saturation = player.food.saturation;
          hs.armor = player.inventory.armor_value();
          hs.air = player.air_supply;
          hs.in_water = player.in_water;
          hs.current_item = player.inventory.current;
          hs.xp_frac = player.current_xp;
          hs.xp_level = player.player_level;
          for (int i = 0; i < 9; ++i) {
            const auto& sl = player.inventory.main[i];
            if (sl.has_value() && sl->stack_size > 0) {
              hs.hotbar[i].id = sl->item_id;
              hs.hotbar[i].count = sl->stack_size;
              hs.hotbar[i].damage = sl->damage;
              hs.hotbar[i].max_damage = sl->max_damage();
            }
          }
          const auto hud = craftpp::gui::build_hud(hs, font);
          const glm::mat4 ortho =
              glm::ortho(0.0F, static_cast<float>(w), static_cast<float>(h), 0.0F, -1.0F, 1.0F);
          const glm::mat4 ident(1.0F);
          glDisable(GL_DEPTH_TEST);
          glEnable(GL_BLEND);
          glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
          terrain_prog.use();
          terrain_prog.set_mat4(t_mvp, &ortho[0][0]);
          terrain_prog.set_mat4(t_view, &ident[0][0]);
          terrain_prog.set_float(t_fog_start, 1.0e9F);
          terrain_prog.set_float(t_fog_end, 2.0e9F);
          terrain_prog.set_int(t_tex, 0);
          auto draw_2d = [&](const craftpp::render::Mesh& mm,
                             const craftpp::render::Texture& tex) {
            if (mm.vertices.empty()) return;
            tex.bind(0);
            craftpp::render::Tessellator tess;
            tess.upload(mm);
            tess.draw();
          };
          draw_2d(hud.chrome, gui_tex);
          draw_2d(hud.icons, icons_tex);
          draw_2d(hud.items, items_tex);
          atlas.bind(0);
          if (!hud.blocks.vertices.empty()) {
            craftpp::render::Tessellator tess;
            tess.upload(hud.blocks);
            tess.draw();
          }
          draw_2d(hud.shadow, font_tex);
          draw_2d(hud.text, font_tex);
          if (!hud.bars.vertices.empty()) {
            flat_prog.use();
            flat_prog.set_mat4(f_mvp, &ortho[0][0]);
            flat_prog.set_float(f_bright, 1.0F);
            craftpp::render::Tessellator tess;
            tess.upload(hud.bars);
            tess.draw();
          }
          glDisable(GL_BLEND);
          glEnable(GL_DEPTH_TEST);
        }
      }

      // ---- screens on top ----
      if (ui.cur != craftpp::gui::Screen::None) {
        const auto sm = craftpp::gui::draw_screen(ui, sctx, font, frames);
        const glm::mat4 ortho =
            glm::ortho(0.0F, static_cast<float>(w), static_cast<float>(h), 0.0F, -1.0F, 1.0F);
        const glm::mat4 ident(1.0F);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        terrain_prog.use();
        terrain_prog.set_mat4(t_mvp, &ortho[0][0]);
        terrain_prog.set_mat4(t_view, &ident[0][0]);
        terrain_prog.set_float(t_fog_start, 1.0e9F);
        terrain_prog.set_float(t_fog_end, 2.0e9F);
        terrain_prog.set_int(t_tex, 0);
        auto draw_2d = [&](const craftpp::render::Mesh& mm,
                           const craftpp::render::Texture& tex) {
          if (mm.vertices.empty()) return;
          tex.bind(0);
          craftpp::render::Tessellator tess;
          tess.upload(mm);
          tess.draw();
        };
        draw_2d(sm.bg, bg_tex);
        draw_2d(sm.chrome, gui_tex);
        draw_2d(sm.logo, logo_tex);
        if (!sm.flat.vertices.empty()) {
          flat_prog.use();
          flat_prog.set_mat4(f_mvp, &ortho[0][0]);
          flat_prog.set_float(f_bright, 1.0F);
          craftpp::render::Tessellator tess;
          tess.upload(sm.flat);
          tess.draw();
          terrain_prog.use();
          terrain_prog.set_mat4(t_mvp, &ortho[0][0]);
          terrain_prog.set_mat4(t_view, &ident[0][0]);
          terrain_prog.set_float(t_fog_start, 1.0e9F);
          terrain_prog.set_float(t_fog_end, 2.0e9F);
          terrain_prog.set_int(t_tex, 0);
        }
        draw_2d(sm.shadow, font_tex);
        draw_2d(sm.text, font_tex);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
      }

      glfwSwapBuffers(window);
      glfwPollEvents();
      if (quit_flag != 0) break;

      if (!args.screenshot.empty() && !screenshotted && ++frames >= args.shot_frames) {
        if (save_screenshot(args.screenshot, w, h)) {
          craftpp::log_info("screenshot saved: " + args.screenshot);
        } else {
          craftpp::log_error("screenshot failed");
          exit_code = 1;
        }
        screenshotted = true;
        break;
      }
      ++frames;
    }
  }  // end GL scope

  glfwDestroyWindow(window);
  glfwTerminate();
  return exit_code;
}
