#include "render/frustum.hpp"

#include <cmath>

namespace craftpp::render {

void Frustum::update(const float* m) {
  // Gribb/Hartmann extraction, rows/cols in GL column-major layout.
  std::array<std::array<float, 4>, 6> p = {{
      {m[3] + m[0], m[7] + m[4], m[11] + m[8], m[15] + m[12]},    // left
      {m[3] - m[0], m[7] - m[4], m[11] - m[8], m[15] - m[12]},    // right
      {m[3] + m[1], m[7] + m[5], m[11] + m[9], m[15] + m[13]},    // bottom
      {m[3] - m[1], m[7] - m[5], m[11] - m[9], m[15] - m[13]},    // top
      {m[3] + m[2], m[7] + m[6], m[11] + m[10], m[15] + m[14]},   // near
      {m[3] - m[2], m[7] - m[6], m[11] - m[10], m[15] - m[14]},   // far
  }};
  for (auto& plane : p) {
    const float len = std::sqrt(plane[0] * plane[0] + plane[1] * plane[1] + plane[2] * plane[2]);
    for (float& c : plane) c /= len;
  }
  planes_ = p;
}

bool Frustum::box_visible(const Aabb& box) const {
  for (const auto& pl : planes_) {
    // Reject when the positive vertex is outside.
    const float px = pl[0] >= 0.0F ? box.max_x : box.min_x;
    const float py = pl[1] >= 0.0F ? box.max_y : box.min_y;
    const float pz = pl[2] >= 0.0F ? box.max_z : box.min_z;
    if (pl[0] * px + pl[1] * py + pl[2] * pz + pl[3] < 0.0F) return false;
  }
  return true;
}

}  // namespace craftpp::render
