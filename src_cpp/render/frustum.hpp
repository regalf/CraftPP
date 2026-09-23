#pragma once

#include <array>

#include "core/aabb.hpp"

namespace craftpp::render {

// Six-plane frustum extracted from a view-projection matrix (column-major,
// GL convention). Used by WorldRenderer to skip off-screen chunks.
class Frustum {
 public:
  // `vp` points to 16 floats in GL column-major order.
  void update(const float* vp);
  bool box_visible(const Aabb& box) const;

 private:
  // Normalized planes: (a, b, c, d) with inside = ax+by+cz+d >= 0.
  std::array<std::array<float, 4>, 6> planes_{};
};

}  // namespace craftpp::render
