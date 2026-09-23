#include "core/vec3.hpp"

#include "core/math_helper.hpp"

namespace craftpp {

Vec3 Vec3::normalize() const {
  const double len = MathHelper::sqrt_double(x * x + y * y + z * z);
  if (len < 1e-4) return {};
  return {x / len, y / len, z / len};
}

double Vec3::distance_to(const Vec3& o) const {
  const double dx = o.x - x;
  const double dy = o.y - y;
  const double dz = o.z - z;
  return MathHelper::sqrt_double(dx * dx + dy * dy + dz * dz);
}

double Vec3::square_distance_to(const Vec3& o) const {
  const double dx = o.x - x;
  const double dy = o.y - y;
  const double dz = o.z - z;
  return dx * dx + dy * dy + dz * dz;
}

double Vec3::square_distance_to(double ox, double oy, double oz) const {
  const double dx = ox - x;
  const double dy = oy - y;
  const double dz = oz - z;
  return dx * dx + dy * dy + dz * dz;
}

double Vec3::length() const {
  return MathHelper::sqrt_double(x * x + y * y + z * z);
}

std::optional<Vec3> Vec3::intermediate_with_x(const Vec3& o, double x_) const {
  const double dx = o.x - x;
  const double dy = o.y - y;
  const double dz = o.z - z;
  if (dx * dx < 1e-7) return std::nullopt;
  const double t = (x_ - x) / dx;
  if (t < 0.0 || t > 1.0) return std::nullopt;
  return Vec3{x + dx * t, y + dy * t, z + dz * t};
}

std::optional<Vec3> Vec3::intermediate_with_y(const Vec3& o, double y_) const {
  const double dx = o.x - x;
  const double dy = o.y - y;
  const double dz = o.z - z;
  if (dy * dy < 1e-7) return std::nullopt;
  const double t = (y_ - y) / dy;
  if (t < 0.0 || t > 1.0) return std::nullopt;
  return Vec3{x + dx * t, y + dy * t, z + dz * t};
}

std::optional<Vec3> Vec3::intermediate_with_z(const Vec3& o, double z_) const {
  const double dx = o.x - x;
  const double dy = o.y - y;
  const double dz = o.z - z;
  if (dz * dz < 1e-7) return std::nullopt;
  const double t = (z_ - z) / dz;
  if (t < 0.0 || t > 1.0) return std::nullopt;
  return Vec3{x + dx * t, y + dy * t, z + dz * t};
}

void Vec3::rotate_around_x(float angle) {
  const float c = MathHelper::cos(angle);
  const float s = MathHelper::sin(angle);
  const double nx = x;
  const double ny = y * c + z * s;
  const double nz = z * c - y * s;
  x = nx;
  y = ny;
  z = nz;
}

void Vec3::rotate_around_y(float angle) {
  const float c = MathHelper::cos(angle);
  const float s = MathHelper::sin(angle);
  const double nx = x * c + z * s;
  const double nz = z * c - x * s;
  x = nx;
  z = nz;
}

}  // namespace craftpp
