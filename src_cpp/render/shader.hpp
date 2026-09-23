#pragma once

#include <string>

namespace craftpp::render {

// Minimal GLSL program wrapper (Render thread only). Uniform locations for
// the terrain shader are resolved once at link time.
class ShaderProgram {
 public:
  ShaderProgram() = default;
  ShaderProgram(const ShaderProgram&) = delete;
  ShaderProgram& operator=(const ShaderProgram&) = delete;
  ~ShaderProgram();

  bool link(const char* vert_src, const char* frag_src, std::string& error);
  void use() const;

  int uniform(const char* name) const;
  void set_mat4(int loc, const float* column_major) const;
  void set_float(int loc, float v) const;
  void set_vec3(int loc, float x, float y, float z) const;
  void set_int(int loc, int v) const;

  unsigned int id() const { return id_; }

 private:
  unsigned int id_ = 0;
};

}  // namespace craftpp::render
