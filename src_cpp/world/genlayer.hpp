#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace craftpp::world {

// Port of the GenLayer stack (GenLayer.java + 18 subclasses).
//
// All seed arithmetic is 64-bit wrapping like Java long (implemented with
// uint64_t); all cell-coordinate math is 32-bit wrapping like Java int
// (helpers below — plain signed ops would be UB on overflow).
// Output vectors are row-major w*h ints, like the first w*h entries of the
// IntCache arrays the source returns.
class GenLayer {
 public:
  virtual ~GenLayer() = default;

  // Mirrors func_35500_a(x, z, w, h). Returned values are biome ids, except
  // the temperature/rainfall layers which return fixed-point ints and the
  // river branch which may return -1 for "no river here".
  virtual std::vector<std::int32_t> generate(std::int32_t x, std::int32_t z, int w, int h) = 0;

  // Mirrors func_35496_b: mixes the world seed down the parent chain.
  virtual void init_world_seed(std::int64_t world_seed);

 protected:
  explicit GenLayer(std::int64_t base_seed);

  // Mirrors func_35499_a. Takes 32-bit cell coords (callers widen Java int
  // exprs, including their wraparound, exactly like the source).
  void init_chunk_seed(std::int32_t x, std::int32_t z);
  std::int32_t next_int(std::int32_t bound);

  std::shared_ptr<GenLayer> parent_;

 private:
  static std::uint64_t lcg_step(std::uint64_t s) {
    return s * (s * 6364136223846793005ULL + 1442695040888963407ULL);
  }
  std::uint64_t base_seed_ = 0;
  std::uint64_t world_seed_ = 0;
  std::uint64_t chunk_seed_ = 0;
};

// Java-int wrapping helpers (the source relies on 32-bit overflow in
// coordinate expressions such as (x + var5 << 1)).
inline std::int32_t i32_add(std::int32_t a, std::int32_t b) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}
inline std::int32_t i32_shl(std::int32_t a, int bits) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) << bits);
}

// Layer constructors mirror the subclasses; most take (baseSeed, parent).
std::shared_ptr<GenLayer> make_layer_island(std::int64_t seed, std::shared_ptr<GenLayer> parent);
std::shared_ptr<GenLayer> make_layer_zoom(std::int64_t seed, std::shared_ptr<GenLayer> parent);
std::shared_ptr<GenLayer> make_layer_zoom_fuzzy(std::int64_t seed, std::shared_ptr<GenLayer> parent);
std::shared_ptr<GenLayer> make_layer_snow(std::int64_t seed, std::shared_ptr<GenLayer> parent);
std::shared_ptr<GenLayer> make_layer_mushroom(std::int64_t seed, std::shared_ptr<GenLayer> parent);
std::shared_ptr<GenLayer> make_layer_river_init(std::int64_t seed, std::shared_ptr<GenLayer> parent);
std::shared_ptr<GenLayer> make_layer_river(std::int64_t seed, std::shared_ptr<GenLayer> parent);
std::shared_ptr<GenLayer> make_layer_river_mix(std::int64_t seed, std::shared_ptr<GenLayer> biome,
                                               std::shared_ptr<GenLayer> river);
std::shared_ptr<GenLayer> make_layer_smooth(std::int64_t seed, std::shared_ptr<GenLayer> parent);
std::shared_ptr<GenLayer> make_layer_smooth_zoom(std::int64_t seed, std::shared_ptr<GenLayer> parent);
std::shared_ptr<GenLayer> make_layer_voronoi(std::int64_t seed, std::shared_ptr<GenLayer> parent);
std::shared_ptr<GenLayer> make_layer_shore(std::int64_t seed, std::shared_ptr<GenLayer> parent);
std::shared_ptr<GenLayer> make_layer_village(std::int64_t seed, std::shared_ptr<GenLayer> parent);
std::shared_ptr<GenLayer> make_layer_temperature(std::shared_ptr<GenLayer> parent);
std::shared_ptr<GenLayer> make_layer_temperature_mix(std::shared_ptr<GenLayer> smooth_temp,
                                                     std::shared_ptr<GenLayer> biome, int zoom_index);
std::shared_ptr<GenLayer> make_layer_downfall(std::shared_ptr<GenLayer> parent);
std::shared_ptr<GenLayer> make_layer_downfall_mix(std::shared_ptr<GenLayer> smooth_rain,
                                                  std::shared_ptr<GenLayer> biome, int zoom_index);
std::shared_ptr<GenLayer> make_layer_base_island(std::int64_t seed);  // LayerIsland (no parent)

std::shared_ptr<GenLayer> stack_zoom(std::int64_t seed, std::shared_ptr<GenLayer> layer, int count);
std::shared_ptr<GenLayer> stack_smooth_zoom(std::int64_t seed, std::shared_ptr<GenLayer> layer, int count);

// Mirrors GenLayer.func_35497_a: builds the full chain and inits world seeds.
// Returns {biome, voronoi, temperature, rainfall, biome} like the source.
struct LayerSet {
  std::shared_ptr<GenLayer> biome;
  std::shared_ptr<GenLayer> voronoi;
  std::shared_ptr<GenLayer> temperature;
  std::shared_ptr<GenLayer> rainfall;
};
LayerSet make_layers(std::int64_t world_seed);

}  // namespace craftpp::world
