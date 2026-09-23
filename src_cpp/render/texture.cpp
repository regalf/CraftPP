#include "render/texture.hpp"

#include <epoxy/gl.h>
#include <stb/stb_image.h>

namespace craftpp::render {

bool load_png(const char* path, Image& out, std::string& error) {
  int w = 0;
  int h = 0;
  int channels = 0;
  stbi_uc* px = stbi_load(path, &w, &h, &channels, 4);
  if (px == nullptr) {
    error = stbi_failure_reason() != nullptr ? stbi_failure_reason() : "unknown stb error";
    return false;
  }
  out.width = w;
  out.height = h;
  out.rgba.assign(px, px + static_cast<std::size_t>(w) * h * 4);
  stbi_image_free(px);
  return true;
}

Texture::~Texture() {
  if (id_ != 0) glDeleteTextures(1, &id_);
}

bool Texture::upload_nearest(const Image& img) {
  if (img.rgba.empty()) return false;
  glGenTextures(1, &id_);
  glBindTexture(GL_TEXTURE_2D, id_);
  // Faithful to 1.0: pixelated, no mipmaps (RenderEngine used GL_NEAREST).
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, img.width, img.height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               img.rgba.data());
  return glGetError() == GL_NO_ERROR;
}

void Texture::bind(unsigned unit) const {
  glActiveTexture(GL_TEXTURE0 + unit);
  glBindTexture(GL_TEXTURE_2D, id_);
}

bool grass_tint_from_map(const Image& colormap, float& r, float& g, float& b) {
  if (colormap.width != 256 || colormap.height != 256 || colormap.rgba.size() < 256 * 256 * 4) {
    return false;
  }
  // ColorizerGrass.getGrassColor(0.5, 1.0): x=(1-0.5)*255=127, y=(1-0.25... (1-0.5)*255=127.
  const std::size_t i = static_cast<std::size_t>((127 * 256 + 127) * 4);
  r = colormap.rgba[i] / 255.0F;
  g = colormap.rgba[i + 1] / 255.0F;
  b = colormap.rgba[i + 2] / 255.0F;
  return true;
}

}  // namespace craftpp::render
