#include "core/random.hpp"

#include <stdexcept>

namespace craftpp {

void JavaRandom::set_seed(std::int64_t seed) {
  seed_ = (static_cast<std::uint64_t>(seed) ^ kMultiplier) & kMask;
  have_next_gaussian_ = false;
}

std::int32_t JavaRandom::next(int bits) {
  seed_ = (seed_ * kMultiplier + kAddend) & kMask;
  return static_cast<std::int32_t>(seed_ >> (48 - bits));
}

std::int32_t JavaRandom::next_int(int bound) {
  if (bound <= 0) throw std::invalid_argument("bound must be positive");
  std::int32_t r = next(31);
  const std::int32_t m = bound - 1;
  if ((bound & m) == 0) {
    // Power of two: fast path.
    r = static_cast<std::int32_t>((static_cast<std::int64_t>(bound) * r) >> 31);
  } else {
    std::int32_t u = r;
    while (u - (r = u % bound) + m < 0) {
      u = next(31);
    }
  }
  return r;
}

std::int64_t JavaRandom::next_long() {
  return (static_cast<std::int64_t>(next(32)) << 32) + next(32);
}

float JavaRandom::next_float() {
  // next(24) / 2^24 exactly representable in float.
  return static_cast<float>(next(24)) / static_cast<float>(1 << 24);
}

double JavaRandom::next_double() {
  // (next(26) << 27 + next(27)) / 2^53 exactly representable in double.
  const std::int64_t hi = next(26);
  const std::int64_t lo = next(27);
  return static_cast<double>((hi << 27) + lo) / static_cast<double>(1ULL << 53);
}

void JavaRandom::next_bytes(std::uint8_t* out, std::size_t len) {
  std::size_t i = 0;
  while (i < len) {
    // Matches Random.nextBytes: one nextInt() serves up to 4 bytes.
    std::int32_t rnd = next_int();
    int n = len - i < 4 ? static_cast<int>(len - i) : 4;
    for (int j = 0; j < n; ++j) {
      out[i++] = static_cast<std::uint8_t>(rnd & 0xFF);
      rnd >>= 8;
    }
  }
}

double JavaRandom::next_gaussian() {
  // Polar (Box-Muller) form, mirroring java.util.Random including the
  // cached second deviate.
  if (have_next_gaussian_) {
    have_next_gaussian_ = false;
    return next_gaussian_;
  }
  double v1 = 0.0;
  double v2 = 0.0;
  double s = 0.0;
  do {
    v1 = 2 * next_double() - 1;
    v2 = 2 * next_double() - 1;
    s = v1 * v1 + v2 * v2;
  } while (s >= 1 || s == 0);
  const double multiplier = std::sqrt(-2 * std::log(s) / s);
  next_gaussian_ = v2 * multiplier;
  have_next_gaussian_ = true;
  return v1 * multiplier;
}

}  // namespace craftpp
