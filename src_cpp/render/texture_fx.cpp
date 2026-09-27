#include "render/texture_fx.hpp"

#include <cmath>

namespace craftpp::render {

const std::vector<std::uint8_t>& FluidTextureFx::tick() {
  ++tick_counter_;
  const bool lava = (kind_ == Kind::Lava || kind_ == Kind::LavaFlow);
  const bool flow = (kind_ == Kind::WaterFlow || kind_ == Kind::LavaFlow);

  for (int x = 0; x < 16; ++x) {
    for (int y = 0; y < 16; ++y) {
      float acc = 0.0F;
      if (lava) {
        // Sinus-warped 3x3 blur (TextureLavaFX).
        const int ox = static_cast<int>(std::sin(y * M_PI * 2.0F / 16.0F) * 1.2F);
        const int oy = static_cast<int>(std::sin(x * M_PI * 2.0F / 16.0F) * 1.2F);
        for (int ix = x - 1; ix <= x + 1; ++ix)
          for (int iy = y - 1; iy <= y + 1; ++iy) acc += g_[((ix + ox) & 15) + ((iy + oy) & 15) * 16];
        h_[x + y * 16] =
            acc / 10.0F +
            (i_[((x + 0) & 15) + ((y + 0) & 15) * 16] + i_[((x + 1) & 15) + ((y + 0) & 15) * 16] +
             i_[((x + 1) & 15) + ((y + 1) & 15) * 16] + i_[((x + 0) & 15) + ((y + 1) & 15) * 16]) /
                4.0F * 0.8F;
      } else if (flow) {
        // Vertical 3-tap blur (TextureWaterFlowFX).
        for (int iy = y - 2; iy <= y; ++iy) acc += g_[(x & 15) + ((iy) & 15) * 16];
        h_[x + y * 16] = acc / 3.2F + i_[x + y * 16] * 0.8F;
      } else {
        // Horizontal 3-tap blur (TextureWaterFX).
        for (int ix = x - 1; ix <= x + 1; ++ix) acc += g_[((ix) & 15) + (y & 15) * 16];
        h_[x + y * 16] = acc / 3.3F + i_[x + y * 16] * 0.8F;
      }
      if (lava) {
        i_[x + y * 16] += j_[x + y * 16] * 0.01F;
        if (i_[x + y * 16] < 0.0F) i_[x + y * 16] = 0.0F;
        j_[x + y * 16] -= 0.06F;
        if (rand_lt(0.005)) j_[x + y * 16] = 1.5F;
      } else {
        i_[x + y * 16] += j_[x + y * 16] * 0.05F;
        if (i_[x + y * 16] < 0.0F) i_[x + y * 16] = 0.0F;
        j_[x + y * 16] -= flow ? 0.3F : 0.1F;
        if (rand_lt(flow ? 0.2 : 0.05)) j_[x + y * 16] = 0.5F;
      }
    }
  }
  // Swap h_/g_ (reference swap in the source).
  for (int k = 0; k < 256; ++k) {
    const float t = h_[k];
    h_[k] = g_[k];
    g_[k] = t;
  }

  for (int k = 0; k < 256; ++k) {
    // Flow tiles scroll the pattern vertically (tick/1 and tick/3 rows).
    int src = k;
    if (kind_ == Kind::WaterFlow) src = (k - tick_counter_ * 16) & 255;
    if (kind_ == Kind::LavaFlow) src = (k - (tick_counter_ / 3) * 16) & 255;
    float v = lava ? g_[src] * 2.0F : g_[src];
    if (v > 1.0F) v = 1.0F;
    if (v < 0.0F) v = 0.0F;
    int r, g, b, a;
    if (lava) {
      r = static_cast<int>(v * 100.0F + 155.0F);
      g = static_cast<int>(v * v * 255.0F);
      b = static_cast<int>(v * v * v * v * 128.0F);
      a = 255;
    } else {
      const float v2 = v * v;
      r = static_cast<int>(32.0F + v2 * 32.0F);
      g = static_cast<int>(50.0F + v2 * 64.0F);
      b = 255;
      a = static_cast<int>(146.0F + v2 * 50.0F);
    }
    rgba_[k * 4 + 0] = static_cast<std::uint8_t>(r);
    rgba_[k * 4 + 1] = static_cast<std::uint8_t>(g);
    rgba_[k * 4 + 2] = static_cast<std::uint8_t>(b);
    rgba_[k * 4 + 3] = static_cast<std::uint8_t>(a);
  }
  return rgba_;
}

}  // namespace craftpp::render
