#include "core/aabb.hpp"

namespace craftpp {

Aabb Aabb::add_coord(double dx, double dy, double dz) const {
  double x0 = min_x;
  double y0 = min_y;
  double z0 = min_z;
  double x1 = max_x;
  double y1 = max_y;
  double z1 = max_z;
  if (dx < 0.0) x0 += dx;
  if (dx > 0.0) x1 += dx;
  if (dy < 0.0) y0 += dy;
  if (dy > 0.0) y1 += dy;
  if (dz < 0.0) z0 += dz;
  if (dz > 0.0) z1 += dz;
  return {x0, y0, z0, x1, y1, z1};
}

Aabb Aabb::expand(double dx, double dy, double dz) const {
  return {min_x - dx, min_y - dy, min_z - dz, max_x + dx, max_y + dy, max_z + dz};
}

Aabb Aabb::contract(double dx, double dy, double dz) const {
  return {min_x + dx, min_y + dy, min_z + dz, max_x - dx, max_y - dy, max_z - dz};
}

Aabb Aabb::offset_copy(double dx, double dy, double dz) const {
  return {min_x + dx, min_y + dy, min_z + dz, max_x + dx, max_y + dy, max_z + dz};
}

double Aabb::clamp_x(const Aabb& o, double dx) const {
  if (o.max_y > min_y && o.min_y < max_y) {
    if (o.max_z > min_z && o.min_z < max_z) {
      double gap = 0.0;
      if (dx > 0.0 && o.max_x <= min_x) {
        gap = min_x - o.max_x;
        if (gap < dx) dx = gap;
      }
      if (dx < 0.0 && o.min_x >= max_x) {
        gap = max_x - o.min_x;
        if (gap > dx) dx = gap;
      }
      return dx;
    }
    return dx;
  }
  return dx;
}

double Aabb::clamp_y(const Aabb& o, double dy) const {
  if (o.max_x > min_x && o.min_x < max_x) {
    if (o.max_z > min_z && o.min_z < max_z) {
      double gap = 0.0;
      if (dy > 0.0 && o.max_y <= min_y) {
        gap = min_y - o.max_y;
        if (gap < dy) dy = gap;
      }
      if (dy < 0.0 && o.min_y >= max_y) {
        gap = max_y - o.min_y;
        if (gap > dy) dy = gap;
      }
      return dy;
    }
    return dy;
  }
  return dy;
}

double Aabb::clamp_z(const Aabb& o, double dz) const {
  if (o.max_x > min_x && o.min_x < max_x) {
    if (o.max_y > min_y && o.min_y < max_y) {
      double gap = 0.0;
      if (dz > 0.0 && o.max_z <= min_z) {
        gap = min_z - o.max_z;
        if (gap < dz) dz = gap;
      }
      if (dz < 0.0 && o.min_z >= max_z) {
        gap = max_z - o.min_z;
        if (gap > dz) dz = gap;
      }
      return dz;
    }
    return dz;
  }
  return dz;
}

namespace {

bool in_yz(const Aabb& b, const std::optional<Vec3>& v) {
  return v.has_value() && v->y >= b.min_y && v->y <= b.max_y && v->z >= b.min_z && v->z <= b.max_z;
}
bool in_xz(const Aabb& b, const std::optional<Vec3>& v) {
  return v.has_value() && v->x >= b.min_x && v->x <= b.max_x && v->z >= b.min_z && v->z <= b.max_z;
}
bool in_xy(const Aabb& b, const std::optional<Vec3>& v) {
  return v.has_value() && v->x >= b.min_x && v->x <= b.max_x && v->y >= b.min_y && v->y <= b.max_y;
}

}  // namespace

std::optional<Aabb::RayHit> Aabb::clip(const Vec3& from, const Vec3& to) const {
  std::optional<Vec3> hx0 = from.intermediate_with_x(to, min_x);
  std::optional<Vec3> hx1 = from.intermediate_with_x(to, max_x);
  std::optional<Vec3> hy0 = from.intermediate_with_y(to, min_y);
  std::optional<Vec3> hy1 = from.intermediate_with_y(to, max_y);
  std::optional<Vec3> hz0 = from.intermediate_with_z(to, min_z);
  std::optional<Vec3> hz1 = from.intermediate_with_z(to, max_z);
  if (!in_yz(*this, hx0)) hx0.reset();
  if (!in_yz(*this, hx1)) hx1.reset();
  if (!in_xz(*this, hy0)) hy0.reset();
  if (!in_xz(*this, hy1)) hy1.reset();
  if (!in_xy(*this, hz0)) hz0.reset();
  if (!in_xy(*this, hz1)) hz1.reset();

  // Candidates paired with source face ids; index identity replaces the
  // reference comparisons of the source ray clip.
  const std::optional<Vec3> cands[6] = {hx0, hx1, hy0, hy1, hz0, hz1};
  const int faces[6] = {4, 5, 0, 1, 2, 3};
  int best_idx = -1;
  for (int i = 0; i < 6; ++i) {
    if (!cands[i].has_value()) continue;
    if (best_idx < 0 ||
        from.square_distance_to(*cands[i]) < from.square_distance_to(*cands[best_idx])) {
      best_idx = i;
    }
  }
  if (best_idx < 0) return std::nullopt;
  return RayHit{*cands[best_idx], faces[best_idx]};
}

}  // namespace craftpp
