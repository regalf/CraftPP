// M4 minimal playable demo (THROWAWAY: wiring only, will be redone).
//
// Real generated terrain (3x3 chunks, fixed seed), real PlayerSP physics,
// real SP controller mining/placing. No HUD/mobs/save/menus; picking is
// full-cube only; placed blocks limited to what the M2 mesher renders.
//
// Controls: WASD move, mouse look, Space jump, Shift sneak, LMB mine,
// RMB place stone, ESC quit.
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <utility>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "core/log.hpp"
#include "core/random.hpp"
#include "entity/controller.hpp"
#include "entity/player_sp.hpp"
#include "render/frustum.hpp"
#include "render/mesher.hpp"
#include "render/shader.hpp"
#include "render/shaders.hpp"
#include "render/tessellator.hpp"
#include "render/texture.hpp"
#include "world/chunk_manager.hpp"
#include "world/live.hpp"
#include "world/provider.hpp"

namespace {

using craftpp::JavaRandom;
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

// Minimal flat-shaded mesher over raw ids (full cubes only, like M2's look
// without the atlas). Winding: quads emitted CCW front; culling stays off.
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

void base_color(int id, float& r, float& g, float& b) {
  switch (id) {
    case 2:
      r = 0.42f;
      g = 0.68f;
      b = 0.28f;
      return;  // grass top
    case 3:
      r = 0.53f;
      g = 0.38f;
      b = 0.24f;
      return;  // dirt
    case 1:
      r = 0.5f;
      g = 0.5f;
      b = 0.5f;
      return;  // stone
    case 12:
      r = 0.85f;
      g = 0.79f;
      b = 0.59f;
      return;  // sand
    case 7:
      r = 0.15f;
      g = 0.15f;
      b = 0.15f;
      return;  // bedrock
    case 9:
      r = 0.2f;
      g = 0.35f;
      b = 0.9f;
      return;  // water
    case 79:
      r = 0.7f;
      g = 0.85f;
      b = 0.95f;
      return;  // ice
    case 17:
      r = 0.35f;
      g = 0.25f;
      b = 0.12f;
      return;  // log
    case 18:
      r = 0.2f;
      g = 0.45f;
      b = 0.15f;
      return;  // leaves
    case 8:
      r = 0.2f;
      g = 0.35f;
      b = 0.9f;
      return;  // water (moving)
    default:
      r = 0.53f;
      g = 0.38f;
      b = 0.24f;
      return;
  }
}

craftpp::render::Mesh mesh_demo_chunk(LiveWorld& w, int cx, int cz) {
  using craftpp::render::Mesh;
  Mesh m;
  for (int lx = 0; lx < 16; ++lx) {
    for (int lz = 0; lz < 16; ++lz) {
      const int x = cx * 16 + lx, z = cz * 16 + lz;
      for (int y = 0; y < 128; ++y) {
        const int id = w.block_id(x, y, z);
        if (id == 0) continue;
        float r, g, b;
        base_color(id, r, g, b);
        const float x0 = x, x1 = x + 1, y0 = y, y1 = y + 1, z0 = z, z1 = z + 1;
        if (w.block_id(x, y + 1, z) == 0)
          add_quad(m, x0, y1, z1, x1, y1, z1, x1, y1, z0, x0, y1, z0, r, g, b);
        if (w.block_id(x, y - 1, z) == 0)
          add_quad(m, x0, y0, z0, x1, y0, z0, x1, y0, z1, x0, y0, z1, r * 0.5f, g * 0.5f, b * 0.5f);
        const float sr = (id == 2 ? 0.53f : r), sg = (id == 2 ? 0.38f : g),
                    sb = (id == 2 ? 0.24f : b);  // grass sides read dirt
        if (w.block_id(x, y, z - 1) == 0)
          add_quad(m, x1, y1, z0, x0, y1, z0, x0, y0, z0, x1, y0, z0, sr * 0.8f, sg * 0.8f,
                   sb * 0.8f);
        if (w.block_id(x, y, z + 1) == 0)
          add_quad(m, x0, y1, z1, x1, y1, z1, x1, y0, z1, x0, y0, z1, sr * 0.8f, sg * 0.8f,
                   sb * 0.8f);
        if (w.block_id(x - 1, y, z) == 0)
          add_quad(m, x0, y1, z1, x0, y1, z0, x0, y0, z0, x0, y0, z1, sr * 0.6f, sg * 0.6f,
                   sb * 0.6f);
        if (w.block_id(x + 1, y, z) == 0)
          add_quad(m, x1, y1, z0, x1, y1, z1, x1, y0, z1, x1, y0, z0, sr * 0.6f, sg * 0.6f,
                   sb * 0.6f);
      }
    }
  }
  return m;
}

constexpr const char* kDemoVert = R"GLSL(
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

constexpr const char* kDemoFrag = R"GLSL(
#version 330 core
in vec3 v_color;
out vec4 out_color;
void main() {
  out_color = vec4(v_color, 1.0);
}
)GLSL";

}  // namespace

int main(int argc, char** argv) {
  std::int64_t seed = 1;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) seed = std::atoll(argv[++i]);
  }

  LiveWorld world(seed);
  world.provide_area(-1, -1, 1, 1);  // terrain + caves + populate + light
  craftpp::log_info("demo world ready (3x3 live chunks: caves + populate + light)");

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
  // Drop in from the sky (also demos falling); settles on its own.
  player.set_position_and_rotation(sx + 0.5, ground + 12.0 + 1.62, sz + 0.5, 0.0f, 0.0f);
  ControllerSP controller(world, player);
  craftpp::entity::ItemStack stone(1, 999, 0);

  if (glfwInit() == GLFW_FALSE) {
    craftpp::log_error("glfw init failed");
    return 1;
  }
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  GLFWwindow* window = glfwCreateWindow(854, 480, "Craft++ M4 demo", nullptr, nullptr);
  if (window == nullptr) {
    craftpp::log_error("window failed");
    glfwTerminate();
    return 1;
  }
  glfwMakeContextCurrent(window);
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  int exit_code = 0;
  {
    craftpp::render::ShaderProgram prog;
    std::string err;
    if (!prog.link(kDemoVert, kDemoFrag, err)) {
      craftpp::log_error("demo shader link failed: " + err);
      return 1;
    }
    const int u_mvp = prog.uniform("u_mvp");
    std::map<std::pair<int, int>, craftpp::render::Tessellator> tess_map;
    for (int cx = -1; cx <= 1; ++cx) {
      for (int cz = -1; cz <= 1; ++cz) {
        tess_map[{cx, cz}].upload(mesh_demo_chunk(world, cx, cz));
        world.clear_dirty(cx, cz);
      }
    }

  // Minimal atlas: reuse M2's terrain.png loading path is app-side; the demo
  // renders untextured quads with per-face shade (no assets needed).
  craftpp::log_info("controls: WASD move, mouse look, Space jump, Shift sneak, LMB mine, RMB place, ESC quit");

  double last_x = 0.0, last_y = 0.0;
  bool have_mouse = false;
  bool lmb_was = false, rmb_was = false;
  float yaw = 0.0f, pitch = 0.0f;

  using clock = std::chrono::steady_clock;
  auto last = clock::now();
  double accumulator = 0.0;
  constexpr double kTick = 1.0 / 20.0;
  int tick_count = 0;

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
    yaw += static_cast<float>(mx - last_x) * 0.15f;  // mouse right looks right
    pitch += static_cast<float>(my - last_y) * 0.15f;  // mouse up (dy<0) looks up
    if (pitch < -90.0f) pitch = -90.0f;
    if (pitch > 90.0f) pitch = 90.0f;
    last_x = mx;
    last_y = my;
    player.rotation_yaw = yaw;
    player.rotation_pitch = pitch;

    // Held keys into movement input.
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
      // Eye + look dir (matches moveFlying yaw convention).
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
      if (hit && lmb && !lmb_was) controller.click_block(hx, hy, hz, side);
      if (hit && lmb) controller.send_block_removing(hx, hy, hz, side);
      if (!lmb) controller.reset_block_removing();
      if (hit && rmb && !rmb_was) controller.send_place_block(stone, hx, hy, hz, side);
      lmb_was = lmb;
      rmb_was = rmb;

      world.tick();  // world time + registered entities (player)
      controller.update_controller();
      if (++tick_count % 20 == 0) {
        char buf[128];
        std::snprintf(buf, sizeof buf, "pos %.1f %.1f %.1f onGround %d", player.pos_x, player.pos_y,
                      player.pos_z, (int)player.on_ground);
        craftpp::log_info(buf);
      }
      accumulator -= kTick;
    }

    // Render: eye camera + per-chunk meshes (remeshed when dirty).
    int w = 0, h = 0;
    glfwGetFramebufferSize(window, &w, &h);
    glViewport(0, 0, w, h);
    glClearColor(0.74F, 0.84F, 1.0F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    const float yr = yaw * 3.14159265f / 180.0f;
    const float pr = pitch * 3.14159265f / 180.0f;
    glm::vec3 eye(player.pos_x, player.pos_y + 0.12, player.pos_z);
    glm::vec3 look(-std::sin(yr) * std::cos(pr), -std::sin(pr), std::cos(yr) * std::cos(pr));
    glm::mat4 view = glm::lookAt(eye, eye + look, glm::vec3(0.0F, 1.0F, 0.0F));
    glm::mat4 proj = glm::perspective(glm::radians(70.0F), static_cast<float>(w) / h, 0.1F, 256.0F);
    glm::mat4 vp = proj * view;

    prog.use();
    prog.set_mat4(u_mvp, &vp[0][0]);
    for (auto& [key, tess] : tess_map) {
      if (world.is_dirty(key.first, key.second)) {
        tess.upload(mesh_demo_chunk(world, key.first, key.second));
        world.clear_dirty(key.first, key.second);
      }
      tess.draw();
    }

    glfwSwapBuffers(window);
    glfwPollEvents();
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) break;
  }
  }  // end GL scope (shader/meshes die before glfwTerminate)

  glfwDestroyWindow(window);
  glfwTerminate();
  return exit_code;
}
