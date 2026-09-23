#include <catch2/catch_test_macros.hpp>
#include <glm/mat4x4.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include "render/frustum.hpp"

using craftpp::render::Frustum;

namespace {

// 70-degree perspective like the 1.0 default FOV, camera at origin looking -Z.
glm::mat4 test_vp() {
  const glm::mat4 proj = glm::perspective(glm::radians(70.0F), 1.0F, 0.1F, 100.0F);
  return proj;  // view = identity
}

}  // namespace

TEST_CASE("frustum culls outside boxes", "[frustum]") {
  Frustum f;
  const glm::mat4 vp = test_vp();
  f.update(&vp[0][0]);

  CHECK(f.box_visible(craftpp::Aabb(-1.0, -1.0, -10.0, 1.0, 1.0, -8.0)));
  CHECK_FALSE(f.box_visible(craftpp::Aabb(-1.0, -1.0, 10.0, 1.0, 1.0, 12.0)));  // behind
  CHECK_FALSE(f.box_visible(craftpp::Aabb(50.0, -1.0, -10.0, 52.0, 1.0, -8.0)));  // far right
  CHECK_FALSE(f.box_visible(craftpp::Aabb(-1.0, -1.0, -200.0, 1.0, 1.0, -150.0)));  // past far
  CHECK_FALSE(f.box_visible(craftpp::Aabb(-1.0, -1.0, -0.05, 1.0, 1.0, 0.0)));  // before near
  // Straddling the near plane counts as visible.
  CHECK(f.box_visible(craftpp::Aabb(-1.0, -1.0, -5.0, 1.0, 1.0, 5.0)));
  // Whole chunk box in front of a level camera.
  CHECK(f.box_visible(craftpp::Aabb(0.0, 0.0, -32.0, 16.0, 5.0, -16.0)));
}
