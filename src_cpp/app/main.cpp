// Craft++ client (M5+): the real game entry point.
//
// Live generated world, PlayerSP physics + controllers, textured chunk
// rendering from terrain.png, entity boxes, McRegion save/load.
// Usage: ./craftpp [--assets DIR] [--seed N] [--save DIR] [--screenshot f.png]

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <stb/stb_image_write.h>

#include "core/log.hpp"
#include "core/random.hpp"
#include "entity/controller.hpp"
#include "entity/mob.hpp"
#include "entity/player_sp.hpp"
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

// Flat-shaded box helper for entities (winding CCW front; culling stays off).
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
  std::string save_dir;
  std::string screenshot;
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
    }
  }
  return a;
}

}  // namespace

int main(int argc, char** argv) {
  // Clean exit (with save) on SIGTERM/SIGINT.
  static volatile std::sig_atomic_t quit_flag = 0;
  std::signal(SIGTERM, [](int) { quit_flag = 1; });
  std::signal(SIGINT, [](int) { quit_flag = 1; });
  const Args args = parse_args(argc, argv);
  std::int64_t seed = args.seed;

  // McRegion save: when level.dat exists the saved seed wins over --seed.
  std::optional<craftpp::world::WorldInfoData> saved_info;
  if (!args.save_dir.empty()) saved_info = craftpp::world::read_level_dat(args.save_dir);
  if (saved_info.has_value()) seed = saved_info->seed;

  LiveWorld world(seed);
  bool loaded = false;
  if (saved_info.has_value()) {
    loaded = world.load(args.save_dir);
    if (loaded) craftpp::log_info("loaded save from " + args.save_dir);
  }
  world.provide_area(-1, -1, 1, 1);  // terrain + caves + populate + light
  craftpp::log_info("world ready (3x3 live chunks: caves + populate + light)");

  // Spawn: first flat 3x3 around the origin (avoids wedge push-out drift).
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
  const int ground = surface_at(sx, sz);
  PlayerSP player(&world, "demo", 0);
  world.add_entity(&player);
  if (loaded && saved_info->player.has_value()) {
    craftpp::world::apply_player_tag(player, *saved_info->player);
  } else {
    world.set_spawn_point(sx + 0.5, ground, sz + 0.5);
    // Drop in from the sky (also demos falling); settles on its own.
    player.set_position_and_rotation(sx + 0.5, ground + 12.0 + 1.62, sz + 0.5, 0.0f, 0.0f);
  }
  ControllerSP controller_sp(world, player);
  ControllerCreative controller_cr(world, player);
  craftpp::entity::Controller* controller = &controller_sp;
  bool creative = false;

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
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  // --- atlas + grass tint (local assets, never committed) ---
  craftpp::render::Image atlas_img;
  std::string err;
  const std::string atlas_path = args.assets + "/terrain.png";
  if (!craftpp::render::load_png(atlas_path.c_str(), atlas_img, err)) {
    craftpp::log_error("cannot load " + atlas_path + ": " + err);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 1;
  }
  craftpp::render::Mesher mesher;
  craftpp::render::Image grass_map;
  if (craftpp::render::load_png((args.assets + "/misc/grasscolor.png").c_str(), grass_map, err) &&
      craftpp::render::grass_tint_from_map(grass_map, mesher.tint_r, mesher.tint_g,
                                           mesher.tint_b)) {
    char buf[96];
    std::snprintf(buf, sizeof(buf), "grass tint: %.3f %.3f %.3f", mesher.tint_r, mesher.tint_g,
                  mesher.tint_b);
    craftpp::log_info(buf);
  } else {
    mesher.tint_r = 0.486F;
    mesher.tint_g = 0.741F;
    mesher.tint_b = 0.349F;
    craftpp::log_info("grasscolor.png missing, using fallback tint");
  }
  craftpp::render::Image foliage_map;
  if (craftpp::render::load_png((args.assets + "/misc/foliagecolor.png").c_str(), foliage_map,
                                err) &&
      craftpp::render::grass_tint_from_map(foliage_map, mesher.foliage_r, mesher.foliage_g,
                                           mesher.foliage_b)) {
    char buf[96];
    std::snprintf(buf, sizeof(buf), "foliage tint: %.3f %.3f %.3f", mesher.foliage_r,
                  mesher.foliage_g, mesher.foliage_b);
    craftpp::log_info(buf);
  } else {
    mesher.foliage_r = 0.282F;
    mesher.foliage_g = 0.478F;
    mesher.foliage_b = 0.141F;
    craftpp::log_info("foliagecolor.png missing, using fallback tint");
  }

  int exit_code = 0;
  {
  craftpp::render::Texture atlas;
  if (!atlas.upload_nearest(atlas_img)) {
    craftpp::log_error("atlas upload failed");
    return 1;
  }
  // Mob skins (local assets; mirroring RenderLiving texture binds).
  craftpp::render::Texture pig_tex, zombie_tex;
  {
    craftpp::render::Image img;
    auto load_skin = [&](const std::string& name, craftpp::render::Texture& tex) {
      if (!craftpp::render::load_png((args.assets + name).c_str(), img, err) || img.width != 64 ||
          img.height != 32 || !tex.upload_nearest(img)) {
        craftpp::log_error("cannot load skin " + name + ": " + err);
      }
    };
    load_skin("/mob/pig.png", pig_tex);
    load_skin("/mob/zombie.png", zombie_tex);
  }
    craftpp::render::ShaderProgram terrain_prog;
    if (!terrain_prog.link(craftpp::render::kTerrainVert, craftpp::render::kTerrainFrag, err)) {
      craftpp::log_error("terrain shader link failed: " + err);
      return 1;
    }
    craftpp::render::ShaderProgram flat_prog;
    if (!flat_prog.link(kFlatVert, kFlatFrag, err)) {
      craftpp::log_error("flat shader link failed: " + err);
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

    std::map<std::pair<int, int>, craftpp::render::Tessellator> tess_map;
    for (int cx = -1; cx <= 1; ++cx) {
      for (int cz = -1; cz <= 1; ++cz) {
        tess_map[{cx, cz}].upload(mesher.mesh_live(world.region(), cx, cz));
        world.clear_dirty(cx, cz);
      }
    }
    craftpp::render::Frustum frustum;

    craftpp::log_info(
        "controls: WASD move, mouse look, Space jump, Shift sneak, LMB mine, "
        "RMB place held, 1-9 hotbar, G creative, ESC quit");

    double last_x = 0.0, last_y = 0.0;
    bool have_mouse = false;
    bool lmb_was = false, rmb_was = false;
    float yaw = 0.0f, pitch = 0.0f;

    using clock = std::chrono::steady_clock;
    auto last = clock::now();
    double accumulator = 0.0;
    constexpr double kTick = 1.0 / 20.0;
    int tick_count = 0;
    double tps_window = 0.0;
    int tps_ticks = 0;
    int frames = 0;
    bool screenshotted = false;

    while (glfwWindowShouldClose(window) == GLFW_FALSE) {
      const auto now = clock::now();
      double frame = std::chrono::duration<double>(now - last).count();
      last = now;
      if (frame > 0.25) frame = 0.25;

      // Mouse look.
      double mx = 0.0, my = 0.0;
      glfwGetCursorPos(window, &mx, &my);
      if (!have_mouse) {
        last_x = mx;
        last_y = my;
        have_mouse = true;
      }
      yaw += static_cast<float>(mx - last_x) * 0.15f;
      pitch += static_cast<float>(my - last_y) * 0.15f;
      if (pitch < -90.0f) pitch = -90.0f;
      if (pitch > 90.0f) pitch = 90.0f;
      last_x = mx;
      last_y = my;
      player.rotation_yaw = yaw;
      player.rotation_pitch = pitch;

      auto& in = player.movement_input;
      in->move_forward =
          (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS ? 1.0f : 0.0f) -
          (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS ? 1.0f : 0.0f);
      in->move_strafe =
          (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS ? 1.0f : 0.0f) -
          (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS ? 1.0f : 0.0f);
      in->jump = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
      in->sneak = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;

      accumulator += frame;
      while (accumulator >= kTick) {
        const float yr = yaw * 3.14159265f / 180.0f;
        const float pr = pitch * 3.14159265f / 180.0f;
        const double lx = -std::sin(yr) * std::cos(pr);
        const double ly = -std::sin(pr);
        const double lz = std::cos(yr) * std::cos(pr);
        const double ex = player.pos_x;
        const double ey = player.pos_y + 0.12;
        const double ez = player.pos_z;
        int hx = 0, hy = 0, hz = 0, side = 0;
        const bool hit = pick_block(world, ex, ey, ez, lx, ly, lz, 4.0, hx, hy, hz, side);
        const bool lmb = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
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
        if (foe != nullptr && lmb && !lmb_was) {
          player.attack_target(*foe);
        } else {
          if (hit && lmb && !lmb_was) controller->click_block(hx, hy, hz, side);
          if (hit && lmb) controller->send_block_removing(hx, hy, hz, side);
          if (!lmb) controller->reset_block_removing();
        }
        if (hit && rmb && !rmb_was) {
          if (auto* held = player.inventory.held()) {
            if (held->has_value()) controller->send_place_block(held->value(), hx, hy, hz, side);
          }
        }
        lmb_was = lmb;
        rmb_was = rmb;
        static bool g_was = false;
        const bool g_now = glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS;
        if (g_now && !g_was) {
          creative = !creative;
          if (creative) {
            ControllerCreative::enable_creative(player);
            controller = &controller_cr;
            craftpp::log_info("creative mode (fly: double-Space, instabreak)");
          } else {
            ControllerCreative::disable_creative(player);
            controller = &controller_sp;
            craftpp::log_info("survival mode");
          }
        }
        g_was = g_now;
        for (int k = 0; k < 9; ++k) {
          if (glfwGetKey(window, GLFW_KEY_1 + k) == GLFW_PRESS) player.inventory.current = k;
        }

        world.tick();
        if (!creative) controller_sp.update_controller();
        if (player.is_dead) {
          for (auto& s : player.inventory.main) {
            if (s.has_value() && s->stack_size > 0) {
              world.on_item_drop(s->item_id, s->stack_size, s->damage, player.pos_x,
                                 player.pos_y + 1.0, player.pos_z, 0.0, 0.0, 0.0);
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
          player.set_position_and_rotation(sx + 0.5, ground + 12.0 + 1.62, sz + 0.5, 0.0f, 0.0f);
          craftpp::log_info("respawned (drops left where you died)");
        }
        ++tick_count;
        ++tps_ticks;
        if (tick_count % 20 == 0) {
          char buf[160];
          std::snprintf(buf, sizeof buf, "pos %.1f %.1f %.1f onGround %d hp %d tps %.1f",
                        player.pos_x, player.pos_y, player.pos_z, (int)player.on_ground,
                        player.health, tps_ticks / (tps_window > 0.0 ? tps_window : 1.0));
          craftpp::log_info(buf);
          tps_window = 0.0;
          tps_ticks = 0;
        }
        accumulator -= kTick;
      }
      tps_window += frame;

      // Render: eye camera, textured chunks (remeshed when dirty), boxes.
      int w = 0, h = 0;
      glfwGetFramebufferSize(window, &w, &h);
      glViewport(0, 0, w, h);
      const float daylight = world.daylight();
      glClearColor(0.74F * daylight, 0.84F * daylight, 1.0F * daylight, 1.0F);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      glEnable(GL_DEPTH_TEST);
      glEnable(GL_CULL_FACE);
      glCullFace(GL_BACK);

      const float yr = yaw * 3.14159265f / 180.0f;
      const float pr = pitch * 3.14159265f / 180.0f;
      glm::vec3 eye(player.pos_x, player.pos_y + 0.12, player.pos_z);
      glm::vec3 look(-std::sin(yr) * std::cos(pr), -std::sin(pr), std::cos(yr) * std::cos(pr));
      glm::mat4 view = glm::lookAt(eye, eye + look, glm::vec3(0.0F, 1.0F, 0.0F));
      glm::mat4 proj = glm::perspective(glm::radians(70.0F), static_cast<float>(w) / h, 0.1F, 256.0F);
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
      for (auto& [key, tess] : tess_map) {
        if (world.is_dirty(key.first, key.second)) {
          tess.upload(mesher.mesh_live(world.region(), key.first, key.second));
          world.clear_dirty(key.first, key.second);
        }
        const craftpp::Aabb box(key.first * 16.0, 0.0, key.second * 16.0,
                                key.first * 16.0 + 16.0, 128.0, key.second * 16.0 + 16.0);
        if (frustum.box_visible(box)) tess.draw();
      }

      // Mobs as textured code models (pig/zombie skins); drops stay boxes.
      glDisable(GL_CULL_FACE);  // entity meshes mirror X (winding flips)
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
        // Translate to feet (mesh is entity-local).
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
        auto box = [&](double ccx, double ccy, double ccz, double hw, double hh, float r, float g,
                       float b) {
          const float x0 = ccx - hw, x1 = ccx + hw, y0 = ccy, y1 = ccy + hh, z0 = ccz - hw,
                      z1 = ccz + hw;
          add_quad(em, x0, y1, z1, x1, y1, z1, x1, y1, z0, x0, y1, z0, r, g, b);
          add_quad(em, x0, y0, z0, x1, y0, z0, x1, y0, z1, x0, y0, z1, r * 0.5f, g * 0.5f, b * 0.5f);
          add_quad(em, x1, y1, z0, x0, y1, z0, x0, y0, z0, x1, y0, z0, r * 0.8f, g * 0.8f, b * 0.8f);
          add_quad(em, x0, y1, z1, x1, y1, z1, x1, y0, z1, x0, y0, z1, r * 0.8f, g * 0.8f, b * 0.8f);
          add_quad(em, x0, y1, z1, x0, y1, z0, x0, y0, z0, x0, y0, z1, r * 0.6f, g * 0.6f, b * 0.6f);
          add_quad(em, x1, y1, z0, x1, y1, z1, x1, y0, z1, x1, y0, z0, r * 0.6f, g * 0.6f, b * 0.6f);
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

      glfwSwapBuffers(window);
      glfwPollEvents();
      if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) break;
      if (quit_flag != 0) break;

      if (!args.screenshot.empty() && !screenshotted && ++frames >= 5) {
        if (save_screenshot(args.screenshot, w, h)) {
          craftpp::log_info("screenshot saved: " + args.screenshot);
        } else {
          craftpp::log_error("screenshot failed");
          exit_code = 1;
        }
        screenshotted = true;
        break;
      }
    }
  }  // end GL scope (shader/meshes die before glfwTerminate)

  if (!args.save_dir.empty()) {
    world.save(args.save_dir, &player);
    craftpp::log_info("saved world to " + args.save_dir);
  }
  glfwDestroyWindow(window);
  glfwTerminate();
  return exit_code;
}
