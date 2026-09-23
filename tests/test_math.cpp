#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "core/aabb.hpp"
#include "core/math_helper.hpp"
#include "core/random.hpp"
#include "core/vec3.hpp"

using craftpp::Aabb;
using craftpp::JavaRandom;
using craftpp::MathHelper;
using craftpp::Vec3;

TEST_CASE("sin/cos table matches std trig", "[math]") {
  // The 65536-entry table quantizes the angle, so resolution error up to
  // ~5e-5 is inherent (same in Java and C++); check within that band.
  const float angles[] = {0.0F, 0.5F, 1.0F, 3.14159265F, -1.0F, 100.0F, -1000.0F};
  for (float a : angles) {
    CHECK_THAT(static_cast<double>(MathHelper::sin(a)),
               Catch::Matchers::WithinAbs(std::sin(a), 1e-4));
    CHECK_THAT(static_cast<double>(MathHelper::cos(a)),
               Catch::Matchers::WithinAbs(std::cos(a), 1e-4));
  }
  // Exact bit patterns dumped from the real MathHelper table (OpenJDK 17):
  // proves the table base, scaling factor and mask match, not just the band.
  auto bits = [](float f) {
    std::int32_t b = 0;
    std::memcpy(&b, &f, sizeof(b));
    return b;
  };
  CHECK(bits(MathHelper::sin(0.5F)) == 1056273710);
  CHECK(bits(MathHelper::cos(0.5F)) == 1063299538);
  CHECK(bits(MathHelper::sin(1.0F)) == 1062693212);
  CHECK(bits(MathHelper::cos(1.0F)) == 1057641281);
  CHECK(bits(MathHelper::sin(3.14159265F)) == 621621554);
  CHECK(bits(MathHelper::cos(3.14159265F)) == -1082130432);
  CHECK(bits(MathHelper::sin(-1.0F)) == -1084790436);
  CHECK(bits(MathHelper::cos(-1.0F)) == 1057639927);
  CHECK(bits(MathHelper::sin(100.0F)) == -1090411084);
  CHECK(bits(MathHelper::cos(100.0F)) == 1063042630);
  CHECK(bits(MathHelper::sin(0.0F)) == 0);
  CHECK(bits(MathHelper::cos(0.0F)) == 1065353216);  // table[16384] == 1.0f
}

TEST_CASE("floors, clamp, bucket", "[math]") {
  CHECK(MathHelper::floor_float(1.9F) == 1);
  CHECK(MathHelper::floor_float(-1.1F) == -2);
  CHECK(MathHelper::floor_double(-1.1) == -2);
  CHECK(MathHelper::floor_double_long(-1.1) == -2);
  CHECK(MathHelper::fast_floor(1.9) == 1);
  CHECK(MathHelper::clamp(5, 0, 3) == 3);
  CHECK(MathHelper::clamp(-2, 0, 3) == 0);
  CHECK(MathHelper::clamp(2, 0, 3) == 2);
  CHECK(MathHelper::bucket_int(15, 16) == 0);
  CHECK(MathHelper::bucket_int(-1, 16) == -1);
  CHECK(MathHelper::bucket_int(-16, 16) == -1);
  CHECK(MathHelper::bucket_int(-17, 16) == -2);
  CHECK(MathHelper::sqrt_float(4.0F) == 2.0F);

  JavaRandom r(99);
  CHECK(MathHelper::random_int_in_range(r, 5, 5) == 5);
  for (int i = 0; i < 100; ++i) {
    const int v = MathHelper::random_int_in_range(r, 3, 7);
    CHECK(v >= 3);
    CHECK(v <= 7);
  }
}

TEST_CASE("vec3 ops", "[math]") {
  const Vec3 a(1.0, 2.0, 3.0);
  const Vec3 b(4.0, 6.0, 8.0);
  // Source quirk: subtract returns other - this.
  CHECK(a.subtract(b) == Vec3(3.0, 4.0, 5.0));
  CHECK(a.dot(b) == 1.0 * 4.0 + 2.0 * 6.0 + 3.0 * 8.0);
  CHECK(a.cross(b) == Vec3(2.0 * 8.0 - 3.0 * 6.0, 3.0 * 4.0 - 1.0 * 8.0, 1.0 * 6.0 - 2.0 * 4.0));
  CHECK_THAT(a.normalize().length(), Catch::Matchers::WithinAbs(1.0, 1e-9));
  // distance_to goes through float sqrt like sqrt_double in the source.
  const double expected_dist = static_cast<double>(static_cast<float>(std::sqrt(50.0)));
  CHECK(a.distance_to(b) == expected_dist);
  CHECK(a.square_distance_to(b) == 9.0 + 16.0 + 25.0);

  const auto mid = a.intermediate_with_x(b, 2.5);
  REQUIRE(mid.has_value());
  CHECK(mid->x == 2.5);
  CHECK_FALSE(a.intermediate_with_x(a, 5.0).has_value());  // degenerate axis

  Vec3 v(0.0, 1.0, 0.0);
  v.rotate_around_x(0.0F);
  CHECK_THAT(v.y, Catch::Matchers::WithinAbs(1.0, 1e-6));
}

TEST_CASE("aabb collision and picking", "[math]") {
  const Aabb box(0.0, 0.0, 0.0, 1.0, 1.0, 1.0);
  CHECK(box.intersects(Aabb(0.5, 0.5, 0.5, 1.5, 1.5, 1.5)));
  CHECK_FALSE(box.intersects(Aabb(1.0, 1.0, 1.0, 2.0, 2.0, 2.0)));  // touching is not intersecting
  CHECK(box.contains(Vec3(0.5, 0.5, 0.5)));
  CHECK(box.expand(1.0, 0.0, 0.0) == Aabb(-1.0, 0.0, 0.0, 2.0, 1.0, 1.0));
  CHECK(box.contract(0.25, 0.25, 0.25) == Aabb(0.25, 0.25, 0.25, 0.75, 0.75, 0.75));

  // Falling entity 1 block above the box, moving down 2: clamped to -1.
  const Aabb falling(0.2, 2.0, 0.2, 0.8, 3.0, 0.8);
  CHECK(box.clamp_y(falling, -2.0) == -1.0);
  // Already touching the top: no downward motion allowed (source returns 0).
  const Aabb resting(0.2, 1.0, 0.2, 0.8, 2.0, 0.8);
  CHECK(box.clamp_y(resting, -2.0) == 0.0);
  // Side collision.
  const Aabb sliding(-1.0, 0.0, 0.2, 0.0, 1.0, 0.8);
  CHECK(box.clamp_x(sliding, 2.0) == 0.0);

  // Ray through the box along +X hits minX face (id 4).
  const auto hit = box.clip(Vec3(-2.0, 0.5, 0.5), Vec3(2.0, 0.5, 0.5));
  REQUIRE(hit.has_value());
  CHECK(hit->face == 4);
  CHECK(hit->hit.x == 0.0);
  // Ray that misses everything.
  CHECK_FALSE(box.clip(Vec3(-2.0, 5.0, 5.0), Vec3(2.0, 5.0, 5.0)).has_value());
}
