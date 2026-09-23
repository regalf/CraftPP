// M2 application: renders a flat 16x16 chunk with the 1.0 look —
// terrain.png atlas, per-face shading, biome grass tint, distance fog.
// Usage: ./craftpp [--assets DIR] [--screenshot out.png]

#include <epoxy/gl.h>

#include <GLFW/glfw3.h>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include <stb/stb_image_write.h>

#include "core/log.hpp"
#include "render/frustum.hpp"
#include "render/mesher.hpp"
#include "render/shader.hpp"
#include "render/shaders.hpp"
#include "render/tessellator.hpp"
#include "render/texture.hpp"
#include "world/chunk.hpp"

namespace {

constexpr double kTickSeconds = 1.0 / 20.0;
constexpr int kWindowWidth = 854;
constexpr int kWindowHeight = 480;
std::uint64_t g_tick_count = 0;
void tick_game() { ++g_tick_count; }

struct Args {
  std::string assets = "assets";
  std::string screenshot;
};

Args parse_args(int argc, char** argv) {
  Args a;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--assets") == 0 && i + 1 < argc) {
      a.assets = argv[++i];
    } else if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
      a.screenshot = argv[++i];
    }
  }
  return a;
}

bool save_screenshot(const std::string& path, int w, int h) {
  glPixelStorei(GL_PACK_ALIGNMENT, 1);  // 854*3 is not a multiple of 4
  std::vector<std::uint8_t> px(static_cast<std::size_t>(w) * h * 3);
  glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px.data());
  // Flip vertically for a conventional top-left origin PNG.
  std::vector<std::uint8_t> flipped(px.size());
  for (int y = 0; y < h; ++y) {
    std::memcpy(flipped.data() + static_cast<std::size_t>(y) * w * 3,
                px.data() + static_cast<std::size_t>(h - 1 - y) * w * 3,
                static_cast<std::size_t>(w) * 3);
  }
  stbi_flip_vertically_on_write(0);
  return stbi_write_png(path.c_str(), w, h, 3, flipped.data(), w * 3) != 0;
}

}  // namespace

int main(int argc, char** argv) {
  using clock = std::chrono::steady_clock;
  const Args args = parse_args(argc, argv);

  if (glfwInit() == GLFW_FALSE) {
    craftpp::log_error("glfwInit failed");
    return 1;
  }
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  GLFWwindow* window =
      glfwCreateWindow(kWindowWidth, kWindowHeight, "Craft++ (M2)", nullptr, nullptr);
  if (window == nullptr) {
    craftpp::log_error("glfwCreateWindow failed");
    glfwTerminate();
    return 1;
  }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  craftpp::log_info(std::string("GL: ") + reinterpret_cast<const char*>(glGetString(GL_VERSION)));

  // --- atlas + grass tint (user-supplied local assets, never committed) ---
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
      craftpp::render::grass_tint_from_map(grass_map, mesher.tint_r, mesher.tint_g, mesher.tint_b)) {
    char buf[96];
    std::snprintf(buf, sizeof(buf), "grass tint: %.3f %.3f %.3f", mesher.tint_r, mesher.tint_g,
                  mesher.tint_b);
    craftpp::log_info(buf);
  } else {
    // Fallback plains green (close to ColorizerGrass default region).
    mesher.tint_r = 0.486F;
    mesher.tint_g = 0.741F;
    mesher.tint_b = 0.349F;
    craftpp::log_info("grasscolor.png missing, using fallback tint");
  }

  // --- world + mesh ---
  craftpp::world::Chunk chunk;
  craftpp::world::fill_flat(chunk);
  const craftpp::render::Mesh mesh = mesher.mesh_chunk(chunk);
  char buf[96];
  std::snprintf(buf, sizeof(buf), "meshed %zu verts %zu indices", mesh.vertices.size(),
                mesh.indices.size());
  craftpp::log_info(buf);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);

  craftpp::render::Frustum frustum;
  const craftpp::Aabb chunk_box(0.0, 0.0, 0.0, 16.0, 5.0, 16.0);

  // GL-owned objects (atlas, tess, prog) must die before glfwTerminate, hence
  // the inner scope.
  int exit_code = 0;
  {
    craftpp::render::Texture atlas;
    if (!atlas.upload_nearest(atlas_img)) {
      craftpp::log_error("atlas upload failed");
      exit_code = 1;
    } else {
      craftpp::render::Tessellator tess;
      tess.upload(mesh);

      craftpp::render::ShaderProgram prog;
      if (!prog.link(craftpp::render::kTerrainVert, craftpp::render::kTerrainFrag, err)) {
        craftpp::log_error("shader link failed: " + err);
        exit_code = 1;
      } else {
        const int u_mvp = prog.uniform("u_mvp");
        const int u_view = prog.uniform("u_view");
        const int u_fog_start = prog.uniform("u_fog_start");
        const int u_fog_end = prog.uniform("u_fog_end");
        const int u_fog_color = prog.uniform("u_fog_color");
        const int u_tex = prog.uniform("u_tex");

        auto last = clock::now();
  double accumulator = 0.0;
  double elapsed = 0.0;
  int frames = 0;
  bool screenshotted = false;

  while (glfwWindowShouldClose(window) == GLFW_FALSE) {
    const auto now = clock::now();
    const double frame = std::chrono::duration<double>(now - last).count();
    last = now;
    accumulator += frame > 0.25 ? 0.25 : frame;
    elapsed += frame > 0.25 ? 0.25 : frame;

    while (accumulator >= kTickSeconds) {
      tick_game();
      accumulator -= kTickSeconds;
    }

    int w = 0;
    int h = 0;
    glfwGetFramebufferSize(window, &w, &h);
    glViewport(0, 0, w, h);

    // Slow orbit around the chunk centre.
    const float angle = static_cast<float>(elapsed * 0.15);
    const glm::vec3 eye(8.0F + std::cos(angle) * 22.0F, 13.0F, 8.0F + std::sin(angle) * 22.0F);
    const glm::vec3 center(8.0F, 2.5F, 8.0F);
    const glm::mat4 view = glm::lookAt(eye, center, glm::vec3(0.0F, 1.0F, 0.0F));
    const glm::mat4 proj =
        glm::perspective(glm::radians(70.0F), static_cast<float>(w) / h, 0.1F, 256.0F);
    const glm::mat4 vp = proj * view;
    frustum.update(&vp[0][0]);

    glClearColor(0.74F, 0.84F, 1.0F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    prog.use();
    prog.set_mat4(u_mvp, &vp[0][0]);
    prog.set_mat4(u_view, &view[0][0]);
    prog.set_float(u_fog_start, 28.0F);
    prog.set_float(u_fog_end, 96.0F);
    prog.set_vec3(u_fog_color, 0.74F, 0.84F, 1.0F);
    prog.set_int(u_tex, 0);
    atlas.bind(0);

    if (frustum.box_visible(chunk_box)) tess.draw();

    glfwSwapBuffers(window);
    glfwPollEvents();
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) break;

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
    }  // end game loop
      }  // end prog scope
    }  // inner GL scope ends here: atlas/tess/prog destroyed while context lives
  }

  glfwDestroyWindow(window);
  glfwTerminate();
  return exit_code;
}
