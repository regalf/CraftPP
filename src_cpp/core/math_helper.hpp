#pragma once

#include <cmath>
#include <cstdint>

namespace craftpp {

// Mirror of MathHelper.java: lookup-table trig plus truncation-based floors.
// Float precision and table indexing match the source exactly so worldgen and
// physics agree with 1.0 bit-for-bit where floats are involved.
class MathHelper {
 public:
  static float sin(float v);
  static float cos(float v);
  static float sqrt_float(float v) { return static_cast<float>(std::sqrt(v)); }
  static float sqrt_double(double v) { return static_cast<float>(std::sqrt(v)); }

  static int floor_float(float v) {
    const int i = static_cast<int>(v);
    return v < static_cast<float>(i) ? i - 1 : i;
  }
  static int floor_double(double v) {
    const int i = static_cast<int>(v);
    return v < static_cast<double>(i) ? i - 1 : i;
  }
  static std::int64_t floor_double_long(double v) {
    const std::int64_t i = static_cast<std::int64_t>(v);
    return v < static_cast<double>(i) ? i - 1 : i;
  }
  // Fast floor valid for the ranges the engine uses it in.
  static int fast_floor(double v) { return static_cast<int>(v + 1024.0) - 1024; }

  static float abs_f(float v) { return v >= 0.0F ? v : -v; }
  static int abs_int(int v) { return v >= 0 ? v : -v; }
  static int clamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

  static double abs_max(double a, double b) {
    if (a < 0.0) a = -a;
    if (b < 0.0) b = -b;
    return a > b ? a : b;
  }

  static int bucket_int(int v, int bucket) {
    return v < 0 ? -((-v - 1) / bucket) - 1 : v / bucket;
  }

  template <typename Rng>
  static int random_int_in_range(Rng& rng, int lo, int hi) {
    return lo >= hi ? lo : rng.next_int(hi - lo + 1) + lo;
  }

 private:
  static const float* sin_table();
};

}  // namespace craftpp
