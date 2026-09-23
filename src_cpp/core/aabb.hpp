#pragma once

#include <optional>

#include "core/vec3.hpp"

namespace craftpp {

// Value-type port of AxisAlignedBB.java.
//
// The source rented instances from a static pool (not thread-safe); here
// every op returns a value, so boxes can cross the Main/Render/ChunkGen
// thread boundary freely. Collision math mirrors the source exactly.
struct Aabb {
  double min_x = 0.0;
  double min_y = 0.0;
  double min_z = 0.0;
  double max_x = 0.0;
  double max_y = 0.0;
  double max_z = 0.0;

  Aabb() = default;
  Aabb(double min_x_, double min_y_, double min_z_, double max_x_, double max_y_, double max_z_)
      : min_x(min_x_), min_y(min_y_), min_z(min_z_), max_x(max_x_), max_y(max_y_), max_z(max_z_) {}

  Aabb add_coord(double dx, double dy, double dz) const;
  Aabb expand(double dx, double dy, double dz) const;
  Aabb contract(double dx, double dy, double dz) const;
  Aabb offset_copy(double dx, double dy, double dz) const;

  // Mutating offset (mirrors source `offset`, which mutated in place).
  Aabb& offset(double dx, double dy, double dz) {
    min_x += dx;
    min_y += dy;
    min_z += dz;
    max_x += dx;
    max_y += dy;
    max_z += dz;
    return *this;
  }

  void set(const Aabb& o) { *this = o; }

  bool operator==(const Aabb& o) const {
    return min_x == o.min_x && min_y == o.min_y && min_z == o.min_z && max_x == o.max_x &&
           max_y == o.max_y && max_z == o.max_z;
  }

  // Collision clamps: how far `other` may move along one axis before
  // touching this box. Mirror calculateXOffset/YOffset/ZOffset.
  double clamp_x(const Aabb& other, double dx) const;
  double clamp_y(const Aabb& other, double dy) const;
  double clamp_z(const Aabb& other, double dz) const;

  bool intersects(const Aabb& o) const {
    return o.max_x > min_x && o.min_x < max_x
        ? (o.max_y > min_y && o.min_y < max_y ? o.max_z > min_z && o.min_z < max_z : false)
        : false;
  }
  bool contains(const Vec3& v) const {
    return v.x > min_x && v.x < max_x
        ? (v.y > min_y && v.y < max_y ? v.z > min_z && v.z < max_z : false)
        : false;
  }
  double average_edge_length() const {
    return (max_x - min_x + (max_y - min_y) + (max_z - min_z)) / 3.0;
  }

  struct RayHit {
    Vec3 hit;
    int face;  // 0=minY 1=maxY 2=minZ 3=maxZ 4=minX 5=maxX (source ids)
  };

  // Ray clip used by block picking. Mirrors func_1169_a (null -> nullopt).
  std::optional<RayHit> clip(const Vec3& from, const Vec3& to) const;
};

}  // namespace craftpp
