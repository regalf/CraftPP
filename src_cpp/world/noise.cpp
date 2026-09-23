#include "world/noise.hpp"

#include "core/math_helper.hpp"

namespace craftpp::world {

Perlin::Perlin(JavaRandom& rand) {
  ox_ = rand.next_double() * 256.0;
  oy_ = rand.next_double() * 256.0;
  oz_ = rand.next_double() * 256.0;
  for (int i = 0; i < 256; ++i) perm_[static_cast<std::size_t>(i)] = i;
  for (int i = 0; i < 256; ++i) {
    const int j = rand.next_int(256 - i) + i;
    const int t = perm_[static_cast<std::size_t>(i)];
    perm_[static_cast<std::size_t>(i)] = perm_[static_cast<std::size_t>(j)];
    perm_[static_cast<std::size_t>(j)] = t;
    perm_[static_cast<std::size_t>(i + 256)] = perm_[static_cast<std::size_t>(i)];
  }
}

double Perlin::grad2(int hash, double x, double z) const {
  const int h = hash & 15;
  const double u = (1 - ((h & 8) >> 3)) * x;
  const double v = h < 4 ? 0.0 : ((h != 12 && h != 14) ? z : x);
  return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

double Perlin::grad3(int hash, double x, double y, double z) const {
  const int h = hash & 15;
  const double u = h < 8 ? x : y;
  const double v = h < 4 ? y : ((h != 12 && h != 14) ? z : x);
  return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

namespace {

inline double fade(double t) { return t * t * t * (t * (t * 6.0 - 15.0) + 10.0); }

}  // namespace

void Perlin::add_noise(double* out, double x, double y, double z, int x_size, int y_size,
                       int z_size, double x_scale, double y_scale, double z_scale,
                       double amplitude) const {
  const double inv_amp = 1.0 / amplitude;
  if (y_size == 1) {
    std::size_t idx = 0;
    for (int ix = 0; ix < x_size; ++ix) {
      double fx = x + ix * x_scale + ox_;
      int lattice_x = MathHelper::floor_double(fx);
      const int px = lattice_x & 255;
      fx -= lattice_x;
      const double tx = fade(fx);
      for (int iz = 0; iz < z_size; ++iz) {
        double fz = z + iz * z_scale + oz_;
        int lattice_z = MathHelper::floor_double(fz);
        const int pz = lattice_z & 255;
        fz -= lattice_z;
        const double tz = fade(fz);
        const int a00 = perm_[static_cast<std::size_t>(px)] + 0;
        const int b00 = perm_[static_cast<std::size_t>(a00)] + pz;
        const int a10 = perm_[static_cast<std::size_t>(px + 1)] + 0;
        const int b10 = perm_[static_cast<std::size_t>(a10)] + pz;
        const double x0 = lerp(tx, grad2(perm_[static_cast<std::size_t>(b00)], fx, fz),
                               grad3(perm_[static_cast<std::size_t>(b10)], fx - 1.0, 0.0, fz));
        const double x1 =
            lerp(tx, grad3(perm_[static_cast<std::size_t>(b00 + 1)], fx, 0.0, fz - 1.0),
                 grad3(perm_[static_cast<std::size_t>(b10 + 1)], fx - 1.0, 0.0, fz - 1.0));
        out[idx++] += lerp(tz, x0, x1) * inv_amp;
      }
    }
    return;
  }

  std::size_t idx = 0;
  int prev_y_lattice = -1;
  double g000 = 0.0, g100 = 0.0, g010 = 0.0, g110 = 0.0;
  for (int ix = 0; ix < x_size; ++ix) {
    double fx = x + ix * x_scale + ox_;
    int lattice_x = MathHelper::floor_double(fx);
    const int px = lattice_x & 255;
    fx -= lattice_x;
    const double tx = fade(fx);
    for (int iz = 0; iz < z_size; ++iz) {
      double fz = z + iz * z_scale + oz_;
      int lattice_z = MathHelper::floor_double(fz);
      const int pz = lattice_z & 255;
      fz -= lattice_z;
      const double tz = fade(fz);
      for (int iy = 0; iy < y_size; ++iy) {
        double fy = y + iy * y_scale + oy_;
        int lattice_y = MathHelper::floor_double(fy);
        const int py = lattice_y & 255;
        fy -= lattice_y;
        const double ty = fade(fy);
        if (iy == 0 || lattice_y != prev_y_lattice) {
          prev_y_lattice = lattice_y;
          const int c00 = perm_[static_cast<std::size_t>(px)] + py;
          const int c01 = perm_[static_cast<std::size_t>(c00)] + pz;
          const int c02 = perm_[static_cast<std::size_t>(c00 + 1)] + pz;
          const int c10 = perm_[static_cast<std::size_t>(px + 1)] + py;
          const int c03 = perm_[static_cast<std::size_t>(c10)] + pz;
          const int c04 = perm_[static_cast<std::size_t>(c10 + 1)] + pz;
          g000 = lerp(tx, grad3(perm_[static_cast<std::size_t>(c01)], fx, fy, fz),
                      grad3(perm_[static_cast<std::size_t>(c03)], fx - 1.0, fy, fz));
          g100 = lerp(tx, grad3(perm_[static_cast<std::size_t>(c02)], fx, fy - 1.0, fz),
                      grad3(perm_[static_cast<std::size_t>(c04)], fx - 1.0, fy - 1.0, fz));
          g010 = lerp(tx, grad3(perm_[static_cast<std::size_t>(c01 + 1)], fx, fy, fz - 1.0),
                      grad3(perm_[static_cast<std::size_t>(c03 + 1)], fx - 1.0, fy, fz - 1.0));
          g110 = lerp(tx, grad3(perm_[static_cast<std::size_t>(c02 + 1)], fx, fy - 1.0, fz - 1.0),
                      grad3(perm_[static_cast<std::size_t>(c04 + 1)], fx - 1.0, fy - 1.0, fz - 1.0));
        }
        const double xy0 = lerp(ty, g000, g100);
        const double xy1 = lerp(ty, g010, g110);
        out[idx++] += lerp(tz, xy0, xy1) * inv_amp;
      }
    }
  }
}

Octaves::Octaves(JavaRandom& rand, int octaves) {
  gens_.reserve(static_cast<std::size_t>(octaves));
  for (int i = 0; i < octaves; ++i) gens_.emplace_back(rand);
}

std::vector<double> Octaves::generate(std::vector<double> out, int x, int y, int z, int x_size,
                                      int y_size, int z_size, double x_scale, double y_scale,
                                      double z_scale) const {
  const std::size_t n = static_cast<std::size_t>(x_size) * y_size * z_size;
  if (out.size() != n) {
    out.assign(n, 0.0);
  } else {
    for (double& v : out) v = 0.0;
  }
  double amp = 1.0;
  for (const Perlin& g : gens_) {
    double fx = x * amp * x_scale;
    double fy = y * amp * y_scale;
    double fz = z * amp * z_scale;
    std::int64_t lx = MathHelper::floor_double_long(fx);
    std::int64_t lz = MathHelper::floor_double_long(fz);
    fx -= lx;
    fz -= lz;
    lx %= 16777216LL;
    lz %= 16777216LL;
    fx += lx;
    fz += lz;
    g.add_noise(out.data(), fx, fy, fz, x_size, y_size, z_size, x_scale * amp, y_scale * amp,
                z_scale * amp, amp);
    amp /= 2.0;
  }
  return out;
}

std::vector<double> Octaves::generate_2d(std::vector<double> out, int x, int z, int x_size,
                                         int z_size, double x_scale, double z_scale) const {
  return generate(std::move(out), x, 10, z, x_size, 1, z_size, x_scale, 1.0, z_scale);
}

}  // namespace craftpp::world
