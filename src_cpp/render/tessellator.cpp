#include "render/tessellator.hpp"

#include <epoxy/gl.h>

namespace craftpp::render {

Tessellator::Tessellator() {
  glGenVertexArrays(1, &vao_);
  glGenBuffers(1, &vbo_);
  glGenBuffers(1, &ebo_);
  glBindVertexArray(vao_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  // Layout: pos(3) color(3) uv(2), locations 0/1/2 like the terrain shader.
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(0));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        reinterpret_cast<void*>(offsetof(Vertex, r)));
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        reinterpret_cast<void*>(offsetof(Vertex, u)));
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
  glBindVertexArray(0);
}

Tessellator::~Tessellator() {
  glDeleteBuffers(1, &ebo_);
  glDeleteBuffers(1, &vbo_);
  glDeleteVertexArrays(1, &vao_);
}

void Tessellator::begin() {
  pending_.vertices.clear();
  pending_.indices.clear();
  color(1.0F, 1.0F, 1.0F);
}

void Tessellator::vertex(float x, float y, float z, float u, float v) {
  pending_.vertices.push_back(Vertex{x, y, z, cr_, gr_, bl_, u, v});
  // Tessellator.java drew quads; here every 4th vertex closes two triangles.
  const auto n = pending_.vertices.size();
  if (n % 4 == 0) {
    const auto b = static_cast<std::uint32_t>(n - 4);
    pending_.indices.insert(pending_.indices.end(), {b, b + 1, b + 2, b, b + 2, b + 3});
  }
}

void Tessellator::upload(const Mesh& mesh) {
  glBindVertexArray(vao_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(Vertex), mesh.vertices.data(),
               GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * sizeof(std::uint32_t),
               mesh.indices.data(), GL_STATIC_DRAW);
  glBindVertexArray(0);
  index_count_ = mesh.indices.size();
}

void Tessellator::draw() const {
  if (index_count_ == 0) return;
  glBindVertexArray(vao_);
  glDrawElements(GL_TRIANGLES, static_cast<int>(index_count_), GL_UNSIGNED_INT, nullptr);
  glBindVertexArray(0);
}

}  // namespace craftpp::render
