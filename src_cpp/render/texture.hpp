#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace craftpp::render {

// PNG atlas loader (stb_image): uploads terrain.png with NEAREST filtering
// like the 1.0 RenderEngine (GL_NEAREST, no mipmaps). Render thread only.
// Also exposes raw pixels so the app can sample biome color maps.
struct Image {
  int width = 0;
  int height = 0;
  std::vector<std::uint8_t> rgba;  // always 4 channels
};

bool load_png(const char* path, Image& out, std::string& error);

class Texture {
 public:
  Texture() = default;
  Texture(const Texture&) = delete;
  Texture& operator=(const Texture&) = delete;
  ~Texture();

  bool upload_nearest(const Image& img);
  void bind(unsigned unit = 0) const;
  unsigned int id() const { return id_; }

 private:
  unsigned int id_ = 0;
};

// Samples a 256x256 color map the way ColorizerGrass does:
// getGrassColor(0.5, 1.0) -> index (127 << 8) | 127. Returns rgb in 0..1.
bool grass_tint_from_map(const Image& colormap, float& r, float& g, float& b);

}  // namespace craftpp::render
