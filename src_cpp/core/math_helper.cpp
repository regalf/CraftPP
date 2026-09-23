#include "core/math_helper.hpp"

#include <array>

namespace craftpp {

const float* MathHelper::sin_table() {
  // 65536-entry table of sin(i * 2π / 65536), built once like the static
  // initializer in MathHelper.java.
  static const std::array<float, 65536> kTable = [] {
    std::array<float, 65536> t{};
    for (int i = 0; i < 65536; ++i) {
      t[i] = static_cast<float>(std::sin(i * 3.14159265358979323846 * 2.0 / 65536.0));
    }
    return t;
  }();
  return kTable.data();
}

float MathHelper::sin(float v) {
  return sin_table()[static_cast<int>(v * 10430.378F) & 0xFFFF];
}

float MathHelper::cos(float v) {
  return sin_table()[static_cast<int>(v * 10430.378F + 16384.0F) & 0xFFFF];
}

}  // namespace craftpp
