#pragma once

#include "render/mesh.hpp"

namespace craftpp::render {

// Tessellator: API-compatible spirit of Tessellator.java (accumulate colored
// UV vertices, then flush), but the buffer uploads once to a VAO/VBO pair and
// draws indexed triangles instead of immediate-mode quads. Render thread only.
class Tessellator {
 public:
  Tessellator();
  Tessellator(const Tessellator&) = delete;
  Tessellator& operator=(const Tessellator&) = delete;
  ~Tessellator();

  void begin();
  void color(float r, float g, float b) {
    cr_ = r;
    gr_ = g;
    bl_ = b;
  }
  void vertex(float x, float y, float z, float u, float v);
  // Uploads a pre-built mesh (mesher output) in one shot.
  void upload(const Mesh& mesh);
  void draw() const;
  std::size_t index_count() const { return index_count_; }

 private:
  unsigned int vao_ = 0;
  unsigned int vbo_ = 0;
  unsigned int ebo_ = 0;
  std::size_t index_count_ = 0;
  float cr_ = 1.0F;
  float gr_ = 1.0F;
  float bl_ = 1.0F;
  Mesh pending_;
};

}  // namespace craftpp::render
