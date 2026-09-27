#pragma once

#include <cstdint>
#include <random>
#include <vector>

namespace craftpp::render {

// Port of TextureWaterFX / TextureWaterFlowFX / TextureLavaFX /
// TextureLavaFlowFX: procedural 16x16 RGBA animations uploaded into the
// terrain atlas each tick (RenderEngine.updateDynamicTextures). Purely
// visual: the noise pump uses a fixed-seed RNG (the source uses
// Math.random(), unseeded and gameplay-irrelevant).
class FluidTextureFx {
 public:
  enum class Kind { Water, WaterFlow, Lava, LavaFlow };

  explicit FluidTextureFx(Kind kind) : kind_(kind), rng_(0xC0FFEEu) {}

  // Advances one tick and returns the 16x16 RGBA tile (1024 bytes).
  const std::vector<std::uint8_t>& tick();

  // Atlas tile index (water top 205, flow side 206, lava top 237, flow 238:
  // BlockFluid ctor (lava ? 14 : 12) * 16 + 13, flows + 1).
  int tile() const {
    switch (kind_) {
      case Kind::Water:
        return 205;
      case Kind::WaterFlow:
        return 206;
      case Kind::Lava:
        return 237;
      case Kind::LavaFlow:
        return 238;
    }
    return 205;
  }

 private:
  Kind kind_;
  std::mt19937 rng_;
  int tick_counter_ = 0;
  float g_[256] = {};
  float h_[256] = {};
  float i_[256] = {};
  float j_[256] = {};
  std::vector<std::uint8_t> rgba_ = std::vector<std::uint8_t>(1024, 0);

  bool rand_lt(double p) {
    return std::uniform_real_distribution<double>(0.0, 1.0)(rng_) < p;
  }
};

}  // namespace craftpp::render
