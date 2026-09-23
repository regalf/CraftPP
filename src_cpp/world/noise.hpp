#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "core/random.hpp"

namespace craftpp::world {

// Port of NoiseGeneratorPerlin + NoiseGeneratorOctaves.
//
// Floating point is IEEE-754 double on both sides; every operation order,
// fade polynomial and lattice-hash expression mirrors the source so output
// is bit-identical. Construction consumes java.util.Random draws exactly
// like the source (3 doubles + Fisher-Yates per Perlin).
class Perlin {
 public:
  explicit Perlin(JavaRandom& rand);

  double lerp(double t, double a, double b) const { return a + t * (b - a); }
  double grad2(int hash, double x, double z) const;
  double grad3(int hash, double x, double y, double z) const;

  // Mirrors func_805_a. `out` is zero-filled first when reused (like the
  // source null-check/else branch), sized xSize*ySize*zSize.
  void add_noise(double* out, double x, double y, double z, int x_size, int y_size, int z_size,
                 double x_scale, double y_scale, double z_scale, double amplitude) const;

 private:
  std::array<int, 512> perm_{};
  double ox_ = 0.0;
  double oy_ = 0.0;
  double oz_ = 0.0;
};

class Octaves {
 public:
  Octaves(JavaRandom& rand, int octaves);

  // Mirrors generateNoiseOctaves. Reused `out` is zeroed first.
  std::vector<double> generate(std::vector<double> out, int x, int y, int z, int x_size, int y_size,
                               int z_size, double x_scale, double y_scale, double z_scale) const;
  // Mirrors func_4109_a (2D form used for biome noises).
  std::vector<double> generate_2d(std::vector<double> out, int x, int z, int x_size, int z_size,
                                  double x_scale, double z_scale) const;

 private:
  std::vector<Perlin> gens_;
};

}  // namespace craftpp::world
