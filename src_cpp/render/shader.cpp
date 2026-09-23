#include "render/shader.hpp"

#include <epoxy/gl.h>

namespace craftpp::render {

ShaderProgram::~ShaderProgram() {
  if (id_ != 0) glDeleteProgram(id_);
}

namespace {

unsigned compile(unsigned type, const char* src, std::string& error) {
  const unsigned sh = glCreateShader(type);
  glShaderSource(sh, 1, &src, nullptr);
  glCompileShader(sh);
  int ok = 0;
  glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
  if (ok == 0) {
    char log[1024] = {};
    glGetShaderInfoLog(sh, sizeof(log), nullptr, log);
    error = log;
    glDeleteShader(sh);
    return 0;
  }
  return sh;
}

}  // namespace

bool ShaderProgram::link(const char* vert_src, const char* frag_src, std::string& error) {
  const unsigned vs = compile(GL_VERTEX_SHADER, vert_src, error);
  if (vs == 0) return false;
  const unsigned fs = compile(GL_FRAGMENT_SHADER, frag_src, error);
  if (fs == 0) {
    glDeleteShader(vs);
    return false;
  }
  id_ = glCreateProgram();
  glAttachShader(id_, vs);
  glAttachShader(id_, fs);
  glLinkProgram(id_);
  glDeleteShader(vs);
  glDeleteShader(fs);
  int ok = 0;
  glGetProgramiv(id_, GL_LINK_STATUS, &ok);
  if (ok == 0) {
    char log[1024] = {};
    glGetProgramInfoLog(id_, sizeof(log), nullptr, log);
    error = log;
    glDeleteProgram(id_);
    id_ = 0;
    return false;
  }
  return true;
}

void ShaderProgram::use() const { glUseProgram(id_); }

int ShaderProgram::uniform(const char* name) const {
  return glGetUniformLocation(id_, name);
}

void ShaderProgram::set_mat4(int loc, const float* column_major) const {
  glUniformMatrix4fv(loc, 1, GL_FALSE, column_major);
}

void ShaderProgram::set_float(int loc, float v) const { glUniform1f(loc, v); }

void ShaderProgram::set_vec3(int loc, float x, float y, float z) const {
  glUniform3f(loc, x, y, z);
}

void ShaderProgram::set_int(int loc, int v) const { glUniform1i(loc, v); }

}  // namespace craftpp::render
