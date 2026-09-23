#include "world/genlayer.hpp"

#include "world/biome.hpp"

namespace craftpp::world {

GenLayer::GenLayer(std::int64_t base_seed) {
  std::uint64_t b = static_cast<std::uint64_t>(base_seed);
  const std::uint64_t s = b;
  for (int i = 0; i < 3; ++i) {
    b = lcg_step(b);
    b += s;
  }
  base_seed_ = b;
}

void GenLayer::init_world_seed(std::int64_t world_seed) {
  std::uint64_t w = static_cast<std::uint64_t>(world_seed);
  if (parent_) parent_->init_world_seed(world_seed);
  for (int i = 0; i < 3; ++i) {
    w = lcg_step(w);
    w += base_seed_;
  }
  world_seed_ = w;
}

void GenLayer::init_chunk_seed(std::int32_t x, std::int32_t z) {
  std::uint64_t c = world_seed_;
  const std::uint64_t ux = static_cast<std::uint64_t>(static_cast<std::int64_t>(x));
  const std::uint64_t uz = static_cast<std::uint64_t>(static_cast<std::int64_t>(z));
  c = lcg_step(c);
  c += ux;
  c = lcg_step(c);
  c += uz;
  c = lcg_step(c);
  c += ux;
  c = lcg_step(c);
  c += uz;
  chunk_seed_ = c;
}

std::int32_t GenLayer::next_int(std::int32_t bound) {
  // (int)((chunkSeed >> 24) % bound), corrected to [0, bound).
  const std::int64_t shifted = static_cast<std::int64_t>(chunk_seed_) >> 24;
  std::int32_t r = static_cast<std::int32_t>(shifted % bound);
  if (r < 0) r += bound;
  chunk_seed_ = lcg_step(chunk_seed_);
  chunk_seed_ += world_seed_;
  return r;
}

namespace {

constexpr std::int32_t kOcean = 0;
constexpr std::int32_t kPlains = 1;
constexpr std::int32_t kRiver = 7;
constexpr std::int32_t kFrozenOcean = 10;
constexpr std::int32_t kFrozenRiver = 11;
constexpr std::int32_t kIcePlains = 12;
constexpr std::int32_t kMushroom = 14;
constexpr std::int32_t kMushroomShore = 15;

// --- concrete layers ---

class BaseIsland : public GenLayer {
 public:
  explicit BaseIsland(std::int64_t s) : GenLayer(s) {}
  std::vector<std::int32_t> generate(std::int32_t x, std::int32_t z, int w, int h) override {
    std::vector<std::int32_t> out(static_cast<std::size_t>(w) * h);
    for (int j = 0; j < h; ++j) {
      for (int i = 0; i < w; ++i) {
        init_chunk_seed(i32_add(x, i), i32_add(z, j));
        out[static_cast<std::size_t>(i + j * w)] = next_int(10) == 0 ? 1 : 0;
      }
    }
    if (x > -w && x <= 0 && z > -h && z <= 0) {
      out[static_cast<std::size_t>(-x + -z * w)] = 1;
    }
    return out;
  }
};

class Island : public GenLayer {
 public:
  Island(std::int64_t s, std::shared_ptr<GenLayer> p) : GenLayer(s) { parent_ = std::move(p); }
  std::vector<std::int32_t> generate(std::int32_t x, std::int32_t z, int w, int h) override {
    const std::int32_t px = x - 1;
    const std::int32_t pz = z - 1;
    const int pw = w + 2;
    const int ph = h + 2;
    const std::vector<std::int32_t> parent = parent_->generate(px, pz, pw, ph);
    std::vector<std::int32_t> out(static_cast<std::size_t>(w) * h);
    for (int j = 0; j < h; ++j) {
      for (int i = 0; i < w; ++i) {
        const std::int32_t c0 = parent[static_cast<std::size_t>(i + j * pw)];
        const std::int32_t c1 = parent[static_cast<std::size_t>(i + 2 + j * pw)];
        const std::int32_t c2 = parent[static_cast<std::size_t>(i + (j + 2) * pw)];
        const std::int32_t c3 = parent[static_cast<std::size_t>(i + 2 + (j + 2) * pw)];
        const std::int32_t center = parent[static_cast<std::size_t>(i + 1 + (j + 1) * pw)];
        init_chunk_seed(i32_add(x, i), i32_add(z, j));
        std::int32_t v = 0;
        if (center != 0 || (c0 == 0 && c1 == 0 && c2 == 0 && c3 == 0)) {
          if (center > 0 && (c0 == 0 || c1 == 0 || c2 == 0 || c3 == 0)) {
            v = (next_int(5) == 0) ? (center == kIcePlains ? kFrozenOcean : 0) : center;
          } else {
            v = center;
          }
        } else {
          std::int32_t pick = 1;
          int n = 1;
          if (c0 != 0 && next_int(n++) == 0) pick = c0;
          if (c1 != 0 && next_int(n++) == 0) pick = c1;
          if (c2 != 0 && next_int(n++) == 0) pick = c2;
          if (c3 != 0 && next_int(n++) == 0) pick = c3;
          if (next_int(3) == 0) {
            v = pick;
          } else if (pick == kIcePlains) {
            v = kFrozenOcean;
          } else {
            v = 0;
          }
        }
        out[static_cast<std::size_t>(i + j * w)] = v;
      }
    }
    return out;
  }
};

// Shared zoom-grid expansion; `pick4` selects the bottom-right cell value:
// majority vote (Zoom) or pure random (ZoomFuzzy).
class ZoomBase : public GenLayer {
 public:
  ZoomBase(std::int64_t s, std::shared_ptr<GenLayer> p) : GenLayer(s) { parent_ = std::move(p); }
  std::vector<std::int32_t> generate(std::int32_t x, std::int32_t z, int w, int h) override {
    const std::int32_t px = x >> 1;
    const std::int32_t pz = z >> 1;
    const int pw = (w >> 1) + 3;
    const int ph = (h >> 1) + 3;
    const std::vector<std::int32_t> parent = parent_->generate(px, pz, pw, ph);
    const int stride = pw << 1;
    std::vector<std::int32_t> grid(static_cast<std::size_t>(stride) * (ph << 1));
    for (int j = 0; j < ph - 1; ++j) {
      int row = (j << 1) * stride;
      std::int32_t left = parent[static_cast<std::size_t>(j * pw)];
      std::int32_t left_below = parent[static_cast<std::size_t>((j + 1) * pw)];
      for (int i = 0; i < pw - 1; ++i) {
        init_chunk_seed(i32_shl(i32_add(i, px), 1), i32_shl(i32_add(j, pz), 1));
        const std::int32_t top_right = parent[static_cast<std::size_t>(i + 1 + j * pw)];
        const std::int32_t bot_right = parent[static_cast<std::size_t>(i + 1 + (j + 1) * pw)];
        grid[static_cast<std::size_t>(row)] = left;
        grid[static_cast<std::size_t>(row++ + stride)] = choose2(left, left_below);
        grid[static_cast<std::size_t>(row)] = choose2(left, top_right);
        grid[static_cast<std::size_t>(row++ + stride)] = pick4(left, top_right, left_below, bot_right);
        left = top_right;
        left_below = bot_right;
      }
    }
    std::vector<std::int32_t> out(static_cast<std::size_t>(w) * h);
    const int ox = x & 1;
    const int oz = z & 1;
    for (int j = 0; j < h; ++j) {
      const std::int32_t* src = grid.data() + static_cast<std::size_t>((j + oz) * stride + ox);
      std::int32_t* dst = out.data() + static_cast<std::size_t>(j * w);
      for (int i = 0; i < w; ++i) dst[i] = src[i];
    }
    return out;
  }

 protected:
  virtual std::int32_t pick4(std::int32_t a, std::int32_t b, std::int32_t c, std::int32_t d) {
    return choose4(a, b, c, d);
  }
  std::int32_t choose2(std::int32_t a, std::int32_t b) { return next_int(2) == 0 ? a : b; }
  std::int32_t choose4(std::int32_t a, std::int32_t b, std::int32_t c, std::int32_t d) {
    const int r = next_int(4);
    return r == 0 ? a : (r == 1 ? b : (r == 2 ? c : d));
  }
};

class Zoom : public ZoomBase {
 public:
  using ZoomBase::ZoomBase;

 protected:
  std::int32_t pick4(std::int32_t a, std::int32_t b, std::int32_t c, std::int32_t d) override {
    if (b == c && c == d) return b;
    if (a == b && a == c) return a;
    if (a == b && a == d) return a;
    if (a == c && a == d) return a;
    if (a == b && c != d) return a;
    if (a == c && b != d) return a;
    if (a == d && b != c) return a;
    if (b == a && c != d) return b;
    if (b == c && a != d) return b;
    if (b == d && a != c) return b;
    if (c == a && b != d) return c;
    if (c == b && a != d) return c;
    if (c == d && a != b) return c;
    if (d == a && b != c) return c;  // verbatim source quirk (returns c)
    if (d == b && a != c) return c;  // verbatim source quirk (returns c)
    if (d == c && a != b) return c;
    return choose4(a, b, c, d);
  }
};

class ZoomFuzzy : public ZoomBase {
 public:
  using ZoomBase::ZoomBase;
};

class Snow : public GenLayer {
 public:
  Snow(std::int64_t s, std::shared_ptr<GenLayer> p) : GenLayer(s) { parent_ = std::move(p); }
  std::vector<std::int32_t> generate(std::int32_t x, std::int32_t z, int w, int h) override {
    const std::vector<std::int32_t> parent = parent_->generate(x - 1, z - 1, w + 2, h + 2);
    std::vector<std::int32_t> out(static_cast<std::size_t>(w) * h);
    const int pw = w + 2;
    for (int j = 0; j < h; ++j) {
      for (int i = 0; i < w; ++i) {
        const std::int32_t center = parent[static_cast<std::size_t>(i + 1 + (j + 1) * pw)];
        init_chunk_seed(i32_add(x, i), i32_add(z, j));
        std::int32_t v = 0;
        if (center != 0) v = (next_int(5) == 0) ? kIcePlains : 1;
        out[static_cast<std::size_t>(i + j * w)] = v;
      }
    }
    return out;
  }
};

class Mushroom : public GenLayer {
 public:
  Mushroom(std::int64_t s, std::shared_ptr<GenLayer> p) : GenLayer(s) { parent_ = std::move(p); }
  std::vector<std::int32_t> generate(std::int32_t x, std::int32_t z, int w, int h) override {
    const std::vector<std::int32_t> parent = parent_->generate(x - 1, z - 1, w + 2, h + 2);
    std::vector<std::int32_t> out(static_cast<std::size_t>(w) * h);
    const int pw = w + 2;
    for (int j = 0; j < h; ++j) {
      for (int i = 0; i < w; ++i) {
        const std::int32_t c0 = parent[static_cast<std::size_t>(i + j * pw)];
        const std::int32_t c1 = parent[static_cast<std::size_t>(i + 2 + j * pw)];
        const std::int32_t c2 = parent[static_cast<std::size_t>(i + (j + 2) * pw)];
        const std::int32_t c3 = parent[static_cast<std::size_t>(i + 2 + (j + 2) * pw)];
        const std::int32_t center = parent[static_cast<std::size_t>(i + 1 + (j + 1) * pw)];
        init_chunk_seed(i32_add(x, i), i32_add(z, j));
        std::int32_t v = center;
        if (center == 0 && c0 == 0 && c1 == 0 && c2 == 0 && c3 == 0 && next_int(100) == 0) {
          v = kMushroom;
        }
        out[static_cast<std::size_t>(i + j * w)] = v;
      }
    }
    return out;
  }
};

class RiverInit : public GenLayer {
 public:
  RiverInit(std::int64_t s, std::shared_ptr<GenLayer> p) : GenLayer(s) { parent_ = std::move(p); }
  std::vector<std::int32_t> generate(std::int32_t x, std::int32_t z, int w, int h) override {
    const std::vector<std::int32_t> parent = parent_->generate(x, z, w, h);
    std::vector<std::int32_t> out(static_cast<std::size_t>(w) * h);
    for (int j = 0; j < h; ++j) {
      for (int i = 0; i < w; ++i) {
        init_chunk_seed(i32_add(x, i), i32_add(z, j));
        out[static_cast<std::size_t>(i + j * w)] =
            parent[static_cast<std::size_t>(i + j * w)] > 0 ? next_int(2) + 2 : 0;
      }
    }
    return out;
  }
};

class River : public GenLayer {
 public:
  River(std::int64_t s, std::shared_ptr<GenLayer> p) : GenLayer(s) { parent_ = std::move(p); }
  std::vector<std::int32_t> generate(std::int32_t x, std::int32_t z, int w, int h) override {
    const std::vector<std::int32_t> parent = parent_->generate(x - 1, z - 1, w + 2, h + 2);
    std::vector<std::int32_t> out(static_cast<std::size_t>(w) * h);
    const int pw = w + 2;
    for (int j = 0; j < h; ++j) {
      for (int i = 0; i < w; ++i) {
        const std::int32_t west = parent[static_cast<std::size_t>(i + (j + 1) * pw)];
        const std::int32_t east = parent[static_cast<std::size_t>(i + 2 + (j + 1) * pw)];
        const std::int32_t north = parent[static_cast<std::size_t>(i + 1 + j * pw)];
        const std::int32_t south = parent[static_cast<std::size_t>(i + 1 + (j + 2) * pw)];
        const std::int32_t center = parent[static_cast<std::size_t>(i + 1 + (j + 1) * pw)];
        std::int32_t v = kRiver;
        if (center != 0 && west != 0 && east != 0 && north != 0 && south != 0 &&
            center == west && center == north && center == east && center == south) {
          v = -1;
        }
        out[static_cast<std::size_t>(i + j * w)] = v;
      }
    }
    return out;
  }
};

class RiverMix : public GenLayer {
 public:
  RiverMix(std::int64_t s, std::shared_ptr<GenLayer> biome, std::shared_ptr<GenLayer> river)
      : GenLayer(s), river_(std::move(river)) {
    parent_ = std::move(biome);
  }
  void init_world_seed(std::int64_t world_seed) override {
    parent_->init_world_seed(world_seed);
    river_->init_world_seed(world_seed);
    GenLayer::init_world_seed(world_seed);
  }
  std::vector<std::int32_t> generate(std::int32_t x, std::int32_t z, int w, int h) override {
    const std::vector<std::int32_t> biome = parent_->generate(x, z, w, h);
    const std::vector<std::int32_t> river = river_->generate(x, z, w, h);
    std::vector<std::int32_t> out(static_cast<std::size_t>(w) * h);
    for (std::size_t i = 0; i < out.size(); ++i) {
      const std::int32_t b = biome[i];
      const std::int32_t r = river[i];
      std::int32_t v = 0;
      if (b == kOcean) {
        v = b;
      } else if (r >= 0) {
        if (b == kIcePlains) {
          v = kFrozenRiver;
        } else if (b != kMushroom && b != kMushroomShore) {
          v = r;
        } else {
          v = kMushroomShore;
        }
      } else {
        v = b;
      }
      out[i] = v;
    }
    return out;
  }

 private:
  std::shared_ptr<GenLayer> river_;
};

class Smooth : public GenLayer {
 public:
  Smooth(std::int64_t s, std::shared_ptr<GenLayer> p) : GenLayer(s) { parent_ = std::move(p); }
  std::vector<std::int32_t> generate(std::int32_t x, std::int32_t z, int w, int h) override {
    const std::vector<std::int32_t> parent = parent_->generate(x - 1, z - 1, w + 2, h + 2);
    std::vector<std::int32_t> out(static_cast<std::size_t>(w) * h);
    const int pw = w + 2;
    for (int j = 0; j < h; ++j) {
      for (int i = 0; i < w; ++i) {
        const std::int32_t west = parent[static_cast<std::size_t>(i + (j + 1) * pw)];
        const std::int32_t east = parent[static_cast<std::size_t>(i + 2 + (j + 1) * pw)];
        const std::int32_t north = parent[static_cast<std::size_t>(i + 1 + j * pw)];
        const std::int32_t south = parent[static_cast<std::size_t>(i + 1 + (j + 2) * pw)];
        std::int32_t center = parent[static_cast<std::size_t>(i + 1 + (j + 1) * pw)];
        if (west == east && north == south) {
          init_chunk_seed(i32_add(x, i), i32_add(z, j));
          center = (next_int(2) == 0) ? west : north;
        } else {
          if (west == east) center = west;
          if (north == south) center = north;
        }
        out[static_cast<std::size_t>(i + j * w)] = center;
      }
    }
    return out;
  }
};

class SmoothZoom : public GenLayer {
 public:
  SmoothZoom(std::int64_t s, std::shared_ptr<GenLayer> p) : GenLayer(s) { parent_ = std::move(p); }
  std::vector<std::int32_t> generate(std::int32_t x, std::int32_t z, int w, int h) override {
    const std::int32_t px = x >> 1;
    const std::int32_t pz = z >> 1;
    const int pw = (w >> 1) + 3;
    const int ph = (h >> 1) + 3;
    const std::vector<std::int32_t> parent = parent_->generate(px, pz, pw, ph);
    const int stride = pw << 1;
    std::vector<std::int32_t> grid(static_cast<std::size_t>(stride) * (ph << 1));
    for (int j = 0; j < ph - 1; ++j) {
      int row = (j << 1) * stride;
      std::int32_t left = parent[static_cast<std::size_t>(j * pw)];
      std::int32_t left_below = parent[static_cast<std::size_t>((j + 1) * pw)];
      for (int i = 0; i < pw - 1; ++i) {
        init_chunk_seed(i32_shl(i32_add(i, px), 1), i32_shl(i32_add(j, pz), 1));
        const std::int32_t top_right = parent[static_cast<std::size_t>(i + 1 + j * pw)];
        const std::int32_t bot_right = parent[static_cast<std::size_t>(i + 1 + (j + 1) * pw)];
        grid[static_cast<std::size_t>(row)] = left;
        grid[static_cast<std::size_t>(row++ + stride)] = left + (left_below - left) * next_int(256) / 256;
        grid[static_cast<std::size_t>(row)] = left + (top_right - left) * next_int(256) / 256;
        const std::int32_t e0 = left + (top_right - left) * next_int(256) / 256;
        const std::int32_t e1 = left_below + (bot_right - left_below) * next_int(256) / 256;
        grid[static_cast<std::size_t>(row++ + stride)] = e0 + (e1 - e0) * next_int(256) / 256;
        left = top_right;
        left_below = bot_right;
      }
    }
    std::vector<std::int32_t> out(static_cast<std::size_t>(w) * h);
    const int ox = x & 1;
    const int oz = z & 1;
    for (int j = 0; j < h; ++j) {
      const std::int32_t* src = grid.data() + static_cast<std::size_t>((j + oz) * stride + ox);
      std::int32_t* dst = out.data() + static_cast<std::size_t>(j * w);
      for (int i = 0; i < w; ++i) dst[i] = src[i];
    }
    return out;
  }
};

class Voronoi : public GenLayer {
 public:
  Voronoi(std::int64_t s, std::shared_ptr<GenLayer> p) : GenLayer(s) { parent_ = std::move(p); }
  std::vector<std::int32_t> generate(std::int32_t x, std::int32_t z, int w, int h) override {
    const std::int32_t qx = x - 2;
    const std::int32_t qz = z - 2;
    constexpr int kCell = 4;
    const std::int32_t px = qx >> 2;
    const std::int32_t pz = qz >> 2;
    const int pw = (w >> 2) + 3;
    const int ph = (h >> 2) + 3;
    const std::vector<std::int32_t> parent = parent_->generate(px, pz, pw, ph);
    const int stride = pw << 2;
    std::vector<std::int32_t> grid(static_cast<std::size_t>(stride) * (ph << 2));
    constexpr double kJitter = kCell * 0.9;
    for (int j = 0; j < ph - 1; ++j) {
      std::int32_t top_left = parent[static_cast<std::size_t>(j * pw)];
      std::int32_t bot_left = parent[static_cast<std::size_t>((j + 1) * pw)];
      for (int i = 0; i < pw - 1; ++i) {
        init_chunk_seed(i32_shl(i32_add(i, px), 2), i32_shl(i32_add(j, pz), 2));
        const double jx0 = (next_int(1024) / 1024.0 - 0.5) * kJitter;
        const double jz0 = (next_int(1024) / 1024.0 - 0.5) * kJitter;
        init_chunk_seed(i32_shl(i32_add(i32_add(i, px), 1), 2), i32_shl(i32_add(j, pz), 2));
        const double jx1 = (next_int(1024) / 1024.0 - 0.5) * kJitter + kCell;
        const double jz1 = (next_int(1024) / 1024.0 - 0.5) * kJitter;
        init_chunk_seed(i32_shl(i32_add(i, px), 2), i32_shl(i32_add(i32_add(j, pz), 1), 2));
        const double jx2 = (next_int(1024) / 1024.0 - 0.5) * kJitter;
        const double jz2 = (next_int(1024) / 1024.0 - 0.5) * kJitter + kCell;
        init_chunk_seed(i32_shl(i32_add(i32_add(i, px), 1), 2), i32_shl(i32_add(i32_add(j, pz), 1), 2));
        const double jx3 = (next_int(1024) / 1024.0 - 0.5) * kJitter + kCell;
        const double jz3 = (next_int(1024) / 1024.0 - 0.5) * kJitter + kCell;
        const std::int32_t top_right = parent[static_cast<std::size_t>(i + 1 + j * pw)];
        const std::int32_t bot_right = parent[static_cast<std::size_t>(i + 1 + (j + 1) * pw)];
        for (int cj = 0; cj < kCell; ++cj) {
          int row = ((j << 2) + cj) * stride + (i << 2);
          for (int ci = 0; ci < kCell; ++ci) {
            const double dx0 = cj - jz0;
            const double dz0 = ci - jx0;
            const double d0 = dx0 * dx0 + dz0 * dz0;
            const double dx1 = cj - jz1;
            const double dz1 = ci - jx1;
            const double d1 = dx1 * dx1 + dz1 * dz1;
            const double dx2 = cj - jz2;
            const double dz2 = ci - jx2;
            const double d2 = dx2 * dx2 + dz2 * dz2;
            const double dx3 = cj - jz3;
            const double dz3 = ci - jx3;
            const double d3 = dx3 * dx3 + dz3 * dz3;
            std::int32_t v = 0;
            if (d0 < d1 && d0 < d2 && d0 < d3) {
              v = top_left;
            } else if (d1 < d0 && d1 < d2 && d1 < d3) {
              v = top_right;
            } else if (d2 < d0 && d2 < d1 && d2 < d3) {
              v = bot_left;
            } else {
              v = bot_right;
            }
            grid[static_cast<std::size_t>(row++)] = v;
          }
        }
        top_left = top_right;
        bot_left = bot_right;
      }
    }
    std::vector<std::int32_t> out(static_cast<std::size_t>(w) * h);
    const int ox = static_cast<int>(static_cast<std::uint32_t>(qx) & 3U);
    const int oz = static_cast<int>(static_cast<std::uint32_t>(qz) & 3U);
    for (int j = 0; j < h; ++j) {
      const std::int32_t* src = grid.data() + static_cast<std::size_t>((j + oz) * stride + ox);
      std::int32_t* dst = out.data() + static_cast<std::size_t>(j * w);
      for (int i = 0; i < w; ++i) dst[i] = src[i];
    }
    return out;
  }
};

class Shore : public GenLayer {
 public:
  Shore(std::int64_t s, std::shared_ptr<GenLayer> p) : GenLayer(s) { parent_ = std::move(p); }
  std::vector<std::int32_t> generate(std::int32_t x, std::int32_t z, int w, int h) override {
    const std::vector<std::int32_t> parent = parent_->generate(x - 1, z - 1, w + 2, h + 2);
    std::vector<std::int32_t> out(static_cast<std::size_t>(w) * h);
    const int pw = w + 2;
    for (int j = 0; j < h; ++j) {
      for (int i = 0; i < w; ++i) {
        init_chunk_seed(i32_add(x, i), i32_add(z, j));  // seeded, no draws (mirrors source)
        const std::int32_t center = parent[static_cast<std::size_t>(i + 1 + (j + 1) * pw)];
        std::int32_t v = center;
        if (center == kMushroom) {
          const std::int32_t n = parent[static_cast<std::size_t>(i + 1 + j * pw)];
          const std::int32_t s = parent[static_cast<std::size_t>(i + 1 + (j + 2) * pw)];
          const std::int32_t we = parent[static_cast<std::size_t>(i + (j + 1) * pw)];
          const std::int32_t ea = parent[static_cast<std::size_t>(i + 2 + (j + 1) * pw)];
          v = (n != kOcean && s != kOcean && we != kOcean && ea != kOcean) ? center : kMushroomShore;
        }
        out[static_cast<std::size_t>(i + j * w)] = v;
      }
    }
    return out;
  }
};

class Village : public GenLayer {
 public:
  Village(std::int64_t s, std::shared_ptr<GenLayer> p) : GenLayer(s) { parent_ = std::move(p); }
  std::vector<std::int32_t> generate(std::int32_t x, std::int32_t z, int w, int h) override {
    static constexpr std::int32_t kAllowed[] = {2, 4, 3, 6, 1, 5};  // desert forest hills swamp plains taiga
    const std::vector<std::int32_t> parent = parent_->generate(x, z, w, h);
    std::vector<std::int32_t> out(static_cast<std::size_t>(w) * h);
    for (int j = 0; j < h; ++j) {
      for (int i = 0; i < w; ++i) {
        init_chunk_seed(i32_add(x, i), i32_add(z, j));
        const std::int32_t v0 = parent[static_cast<std::size_t>(i + j * w)];
        std::int32_t v = 0;
        if (v0 != 0) {
          if (v0 == kMushroom) {
            v = v0;
          } else if (v0 == kPlains) {
            v = kAllowed[static_cast<std::size_t>(next_int(6))];
          } else {
            v = kIcePlains;
          }
        }
        out[static_cast<std::size_t>(i + j * w)] = v;
      }
    }
    return out;
  }
};

class Temperature : public GenLayer {
 public:
  explicit Temperature(std::shared_ptr<GenLayer> p) : GenLayer(0) { parent_ = std::move(p); }
  std::vector<std::int32_t> generate(std::int32_t x, std::int32_t z, int w, int h) override {
    const std::vector<std::int32_t> parent = parent_->generate(x, z, w, h);
    std::vector<std::int32_t> out(static_cast<std::size_t>(w) * h);
    for (std::size_t i = 0; i < out.size(); ++i) {
      out[i] = biome_temp_int(biome_def_by_index(parent[i]));
    }
    return out;
  }
};

class TemperatureMix : public GenLayer {
 public:
  TemperatureMix(std::shared_ptr<GenLayer> smooth, std::shared_ptr<GenLayer> biome, int zoom)
      : GenLayer(0), smooth_(std::move(smooth)), zoom_(zoom) {
    parent_ = std::move(biome);
  }
  std::vector<std::int32_t> generate(std::int32_t x, std::int32_t z, int w, int h) override {
    const std::vector<std::int32_t> biome = parent_->generate(x, z, w, h);
    const std::vector<std::int32_t> smooth = smooth_->generate(x, z, w, h);
    std::vector<std::int32_t> out(static_cast<std::size_t>(w) * h);
    for (std::size_t i = 0; i < out.size(); ++i) {
      const std::int32_t target = biome_temp_int(biome_def_by_index(biome[i]));
      out[i] = smooth[i] + (target - smooth[i]) / (zoom_ * 2 + 1);
    }
    return out;
  }

 private:
  std::shared_ptr<GenLayer> smooth_;
  int zoom_;
};

class Downfall : public GenLayer {
 public:
  explicit Downfall(std::shared_ptr<GenLayer> p) : GenLayer(0) { parent_ = std::move(p); }
  std::vector<std::int32_t> generate(std::int32_t x, std::int32_t z, int w, int h) override {
    const std::vector<std::int32_t> parent = parent_->generate(x, z, w, h);
    std::vector<std::int32_t> out(static_cast<std::size_t>(w) * h);
    for (std::size_t i = 0; i < out.size(); ++i) {
      out[i] = biome_rain_int(biome_def_by_index(parent[i]));
    }
    return out;
  }
};

class DownfallMix : public GenLayer {
 public:
  DownfallMix(std::shared_ptr<GenLayer> smooth, std::shared_ptr<GenLayer> biome, int zoom)
      : GenLayer(0), smooth_(std::move(smooth)), zoom_(zoom) {
    parent_ = std::move(biome);
  }
  std::vector<std::int32_t> generate(std::int32_t x, std::int32_t z, int w, int h) override {
    const std::vector<std::int32_t> biome = parent_->generate(x, z, w, h);
    const std::vector<std::int32_t> smooth = smooth_->generate(x, z, w, h);
    std::vector<std::int32_t> out(static_cast<std::size_t>(w) * h);
    for (std::size_t i = 0; i < out.size(); ++i) {
      const std::int32_t target = biome_rain_int(biome_def_by_index(biome[i]));
      out[i] = smooth[i] + (target - smooth[i]) / (zoom_ + 1);
    }
    return out;
  }

 private:
  std::shared_ptr<GenLayer> smooth_;
  int zoom_;
};

}  // namespace

std::shared_ptr<GenLayer> make_layer_base_island(std::int64_t seed) {
  return std::make_shared<BaseIsland>(seed);
}
std::shared_ptr<GenLayer> make_layer_island(std::int64_t seed, std::shared_ptr<GenLayer> parent) {
  return std::make_shared<Island>(seed, std::move(parent));
}
std::shared_ptr<GenLayer> make_layer_zoom(std::int64_t seed, std::shared_ptr<GenLayer> parent) {
  return std::make_shared<Zoom>(seed, std::move(parent));
}
std::shared_ptr<GenLayer> make_layer_zoom_fuzzy(std::int64_t seed, std::shared_ptr<GenLayer> parent) {
  return std::make_shared<ZoomFuzzy>(seed, std::move(parent));
}
std::shared_ptr<GenLayer> make_layer_snow(std::int64_t seed, std::shared_ptr<GenLayer> parent) {
  return std::make_shared<Snow>(seed, std::move(parent));
}
std::shared_ptr<GenLayer> make_layer_mushroom(std::int64_t seed, std::shared_ptr<GenLayer> parent) {
  return std::make_shared<Mushroom>(seed, std::move(parent));
}
std::shared_ptr<GenLayer> make_layer_river_init(std::int64_t seed, std::shared_ptr<GenLayer> parent) {
  return std::make_shared<RiverInit>(seed, std::move(parent));
}
std::shared_ptr<GenLayer> make_layer_river(std::int64_t seed, std::shared_ptr<GenLayer> parent) {
  return std::make_shared<River>(seed, std::move(parent));
}
std::shared_ptr<GenLayer> make_layer_river_mix(std::int64_t seed, std::shared_ptr<GenLayer> biome,
                                               std::shared_ptr<GenLayer> river) {
  return std::make_shared<RiverMix>(seed, std::move(biome), std::move(river));
}
std::shared_ptr<GenLayer> make_layer_smooth(std::int64_t seed, std::shared_ptr<GenLayer> parent) {
  return std::make_shared<Smooth>(seed, std::move(parent));
}
std::shared_ptr<GenLayer> make_layer_smooth_zoom(std::int64_t seed, std::shared_ptr<GenLayer> parent) {
  return std::make_shared<SmoothZoom>(seed, std::move(parent));
}
std::shared_ptr<GenLayer> make_layer_voronoi(std::int64_t seed, std::shared_ptr<GenLayer> parent) {
  return std::make_shared<Voronoi>(seed, std::move(parent));
}
std::shared_ptr<GenLayer> make_layer_shore(std::int64_t seed, std::shared_ptr<GenLayer> parent) {
  return std::make_shared<Shore>(seed, std::move(parent));
}
std::shared_ptr<GenLayer> make_layer_village(std::int64_t seed, std::shared_ptr<GenLayer> parent) {
  return std::make_shared<Village>(seed, std::move(parent));
}
std::shared_ptr<GenLayer> make_layer_temperature(std::shared_ptr<GenLayer> parent) {
  return std::make_shared<Temperature>(std::move(parent));
}
std::shared_ptr<GenLayer> make_layer_temperature_mix(std::shared_ptr<GenLayer> smooth,
                                                     std::shared_ptr<GenLayer> biome, int zoom) {
  return std::make_shared<TemperatureMix>(std::move(smooth), std::move(biome), zoom);
}
std::shared_ptr<GenLayer> make_layer_downfall(std::shared_ptr<GenLayer> parent) {
  return std::make_shared<Downfall>(std::move(parent));
}
std::shared_ptr<GenLayer> make_layer_downfall_mix(std::shared_ptr<GenLayer> smooth,
                                                  std::shared_ptr<GenLayer> biome, int zoom) {
  return std::make_shared<DownfallMix>(std::move(smooth), std::move(biome), zoom);
}

std::shared_ptr<GenLayer> stack_zoom(std::int64_t seed, std::shared_ptr<GenLayer> layer, int count) {
  for (int i = 0; i < count; ++i) layer = make_layer_zoom(seed + i, std::move(layer));
  return layer;
}

std::shared_ptr<GenLayer> stack_smooth_zoom(std::int64_t seed, std::shared_ptr<GenLayer> layer,
                                            int count) {
  for (int i = 0; i < count; ++i) layer = make_layer_smooth_zoom(seed + i, std::move(layer));
  return layer;
}

LayerSet make_layers(std::int64_t world_seed) {
  auto base = make_layer_base_island(1);
  auto fuzzy = make_layer_zoom_fuzzy(2000, std::move(base));
  auto isl1 = make_layer_island(1, std::move(fuzzy));
  auto zoom1 = make_layer_zoom(2001, std::move(isl1));
  auto isl2 = make_layer_island(2, std::move(zoom1));
  auto snow = make_layer_snow(2, std::move(isl2));
  auto zoom2 = make_layer_zoom(2002, std::move(snow));
  auto isl3 = make_layer_island(3, std::move(zoom2));
  auto zoom3 = make_layer_zoom(2003, std::move(isl3));
  auto isl4 = make_layer_island(4, std::move(zoom3));
  auto mushroom = make_layer_mushroom(5, std::move(isl4));

  // stack_zoom(..., 0) returns its input, mirroring `var4 = zoom(1000, x, 0)`.
  // Both branches share the same mushroom object, like the source.
  auto river_init = make_layer_river_init(100, mushroom);
  auto river_zoomed = stack_zoom(1000, std::move(river_init), 6);
  auto river = make_layer_river(1, std::move(river_zoomed));
  auto river_smooth = make_layer_smooth(1000, std::move(river));

  auto village = make_layer_village(200, mushroom);
  auto village_zoomed = stack_zoom(1000, std::move(village), 2);

  auto temp = make_layer_temperature(village_zoomed);
  auto rain = make_layer_downfall(village_zoomed);

  std::shared_ptr<GenLayer> biome_chain = village_zoomed;
  std::shared_ptr<GenLayer> temp_chain = temp;
  std::shared_ptr<GenLayer> rain_chain = rain;
  for (int i = 0; i < 4; ++i) {
    biome_chain = make_layer_zoom(1000 + i, std::move(biome_chain));
    if (i == 0) biome_chain = make_layer_island(3, std::move(biome_chain));
    if (i == 0) biome_chain = make_layer_shore(1000, std::move(biome_chain));
    auto st = make_layer_smooth_zoom(1000 + i, std::move(temp_chain));
    temp_chain = make_layer_temperature_mix(std::move(st), biome_chain, i);
    auto sr = make_layer_smooth_zoom(1000 + i, std::move(rain_chain));
    rain_chain = make_layer_downfall_mix(std::move(sr), biome_chain, i);
  }

  auto biome_smooth = make_layer_smooth(1000, std::move(biome_chain));
  auto biome_mix = make_layer_river_mix(100, std::move(biome_smooth), std::move(river_smooth));
  auto temp_final = stack_smooth_zoom(1000, std::move(temp_chain), 2);
  auto rain_final = stack_smooth_zoom(1000, std::move(rain_chain), 2);
  auto voronoi = make_layer_voronoi(10, biome_mix);

  // Seed init order mirrors the source (var24, var21, var25, var9).
  // Recursion makes this idempotent where branches are shared.
  biome_mix->init_world_seed(world_seed);
  temp_final->init_world_seed(world_seed);
  rain_final->init_world_seed(world_seed);
  voronoi->init_world_seed(world_seed);
  return LayerSet{biome_mix, voronoi, temp_final, rain_final};
}

}  // namespace craftpp::world
