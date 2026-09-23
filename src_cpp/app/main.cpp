// M0 application shell: opens the window and runs the game loop.
// Render thread owns the GL context (single-threaded for M0; the split into
// Main/Render/ChunkGen threads lands with M2-M3 behind the same tick API).

#include "core/log.hpp"

#include <GLFW/glfw3.h>

#include <chrono>
#include <cstdint>
#include <string>

namespace {

// Minecraft 1.0 ran the simulation at 20 ticks per second.
constexpr double kTickSeconds = 1.0 / 20.0;
constexpr int kWindowWidth = 854;
constexpr int kWindowHeight = 480;

std::uint64_t g_tick_count = 0;

// Placeholder for the M4 simulation step. Must stay deterministic and cheap;
// ordering here defines the tick order the server-interop milestone relies on.
void tick_game() { ++g_tick_count; }

}  // namespace

int main() {
  using clock = std::chrono::steady_clock;

  if (glfwInit() == GLFW_FALSE) {
    craftpp::log_error("glfwInit failed");
    return 1;
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
  GLFWwindow* window =
      glfwCreateWindow(kWindowWidth, kWindowHeight, "Craft++ (M0)", nullptr, nullptr);
  if (window == nullptr) {
    craftpp::log_error("glfwCreateWindow failed");
    glfwTerminate();
    return 1;
  }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);
  craftpp::log_info("Craft++ M0: window open, entering game loop");

  auto last = clock::now();
  double accumulator = 0.0;

  while (glfwWindowShouldClose(window) == GLFW_FALSE) {
    const auto now = clock::now();
    const double frame = std::chrono::duration<double>(now - last).count();
    last = now;
    accumulator += frame > 0.25 ? 0.25 : frame;  // clamp spiral of death

    while (accumulator >= kTickSeconds) {
      tick_game();
      accumulator -= kTickSeconds;
    }

    int w = 0;
    int h = 0;
    glfwGetFramebufferSize(window, &w, &h);
    glViewport(0, 0, w, h);
    glClearColor(0.47F, 0.65F, 1.0F, 1.0F);  // sky-blue placeholder
    glClear(GL_COLOR_BUFFER_BIT);
    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glfwDestroyWindow(window);
  glfwTerminate();
  craftpp::log_info("Craft++ M0: clean shutdown after " + std::to_string(g_tick_count) + " ticks");
  return 0;
}
