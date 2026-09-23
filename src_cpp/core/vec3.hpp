#pragma once

#include <optional>

namespace craftpp {

// Value-type port of Vec3D.java.
//
// The source pooled mutable instances; here every op returns a value, which is
// cheaper and thread-safe. Method semantics mirror the source exactly — note
// `subtract(other)` returns `other - this` (a quirk of the original).
struct Vec3 {
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;

  Vec3() = default;
  Vec3(double x_, double y_, double z_) : x(normalize_zero(x_)), y(normalize_zero(y_)), z(normalize_zero(z_)) {}

  static double normalize_zero(double v) { return v == 0.0 ? 0.0 : v; }  // kills -0.0 like the source ctor

  Vec3 subtract(const Vec3& o) const { return {o.x - x, o.y - y, o.z - z}; }  // NOTE: reversed, as in source
  Vec3 normalize() const;
  double dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
  Vec3 cross(const Vec3& o) const {
    return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x};
  }
  Vec3 add(double dx, double dy, double dz) const { return {x + dx, y + dy, z + dz}; }
  double distance_to(const Vec3& o) const;
  double square_distance_to(const Vec3& o) const;
  double square_distance_to(double ox, double oy, double oz) const;
  double length() const;

  std::optional<Vec3> intermediate_with_x(const Vec3& o, double x_) const;
  std::optional<Vec3> intermediate_with_y(const Vec3& o, double y_) const;
  std::optional<Vec3> intermediate_with_z(const Vec3& o, double z_) const;

  void rotate_around_x(float angle);
  void rotate_around_y(float angle);

  bool operator==(const Vec3& o) const { return x == o.x && y == o.y && z == o.z; }
};

}  // namespace craftpp
