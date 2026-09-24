#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace craftpp {

// Bit-identical reimplementation of java.util.Random (48-bit LCG).
//
// Used everywhere in 1.0 worldgen, so the output must match Java exactly:
// seeds are per-thread private instances (never share one across threads;
// reseed per chunk as source does: worldSeed mixed with chunk coords).
class JavaRandom {
 public:
  explicit JavaRandom(std::int64_t seed = 0) { set_seed(seed); }

  void set_seed(std::int64_t seed);

  // Core primitive: next `bits` random bits (0 < bits <= 32).
  std::int32_t next(int bits);

  std::int32_t next_int() { return next(32); }
  std::int32_t next_int(int bound);
  std::int64_t next_long();
  bool next_boolean() { return next(1) != 0; }
  float next_float();
  double next_double();
  void next_bytes(std::uint8_t* out, std::size_t len);
  void next_bytes(std::vector<std::uint8_t>& out) { next_bytes(out.data(), out.size()); }
  double next_gaussian();

  // Test hook: raw 48-bit LCG state (mirrors Random.seed for stream parity).
  std::uint64_t raw_state() const { return seed_; }

 private:
  static constexpr std::uint64_t kMultiplier = 0x5DEECE66DULL;
  static constexpr std::uint64_t kAddend = 0xBULL;
  static constexpr std::uint64_t kMask = (1ULL << 48) - 1;

  std::uint64_t seed_ = 0;  // 48-bit state
  bool have_next_gaussian_ = false;
  double next_gaussian_ = 0.0;
};

}  // namespace craftpp
