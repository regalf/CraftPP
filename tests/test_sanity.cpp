#include <catch2/catch_test_macros.hpp>

// M0 smoke test: proves the test harness works. Real parity tests
// (JavaRandom vectors, NBT round-trips) land in M1.
TEST_CASE("m0 harness is alive", "[m0]") {
  constexpr int kTicksPerSecond = 20;
  CHECK(kTicksPerSecond == 20);
}
