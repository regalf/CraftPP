#include "world/biome.hpp"

#include <stdexcept>

namespace craftpp::world {

namespace {

// Block ids needed for top/filler (full Block registry lands with M4;
// values mirror Block.java: grass 2, dirt 3, sand 12, sandstone 24,
// gravel 13, snow 78, ice 79, clay 82).
constexpr std::int8_t kGrass = 2;
constexpr std::int8_t kDirt = 3;
constexpr std::int8_t kSand = 12;
constexpr std::int8_t kMycelium = 110;

const std::array<BiomeDef, 16> kBiomes = {{
    {BiomeId::Ocean, -1.0F, 0.4F, 0.5F, 0.5F, kGrass, kDirt},           // 0
    {BiomeId::Plains, 0.1F, 0.3F, 0.8F, 0.4F, kGrass, kDirt},           // 1
    {BiomeId::Desert, 0.1F, 0.2F, 2.0F, 0.0F, kSand, kSand},            // 2
    {BiomeId::Hills, 0.2F, 1.8F, 0.2F, 0.3F, kGrass, kDirt},            // 3
    {BiomeId::Forest, 0.1F, 0.3F, 0.7F, 0.8F, kGrass, kDirt},           // 4
    {BiomeId::Taiga, 0.1F, 0.4F, 0.3F, 0.8F, kGrass, kDirt},            // 5
    {BiomeId::Swampland, -0.2F, 0.1F, 0.8F, 0.9F, kGrass, kDirt},       // 6
    {BiomeId::River, -0.5F, 0.0F, 0.5F, 0.5F, kGrass, kDirt},           // 7
    {BiomeId::Hell, 0.1F, 0.3F, 2.0F, 0.0F, kGrass, kDirt},             // 8 (unused overworld)
    {BiomeId::Sky, 0.1F, 0.3F, 0.5F, 0.5F, kGrass, kDirt},              // 9 (unused overworld)
    {BiomeId::FrozenOcean, -1.0F, 0.5F, 0.0F, 0.5F, kGrass, kDirt},     // 10
    {BiomeId::FrozenRiver, -0.5F, 0.0F, 0.0F, 0.5F, kGrass, kDirt},     // 11
    {BiomeId::IcePlains, 0.1F, 0.3F, 0.0F, 0.5F, kGrass, kDirt},        // 12 (snow via populate)
    {BiomeId::IceMountains, 0.2F, 1.8F, 0.0F, 0.5F, kGrass, kDirt},     // 13
    {BiomeId::MushroomIsland, 0.2F, 1.0F, 0.9F, 1.0F, kMycelium, kDirt}, // 14
    {BiomeId::MushroomIslandShore, -1.0F, 0.1F, 0.9F, 1.0F, kGrass, kDirt}, // 15
}};

}  // namespace

const BiomeDef& biome_def(BiomeId id) {
  return kBiomes.at(static_cast<std::size_t>(id));
}

const BiomeDef& biome_def_by_index(std::int32_t index) {
  if (index < 0 || index > 15) throw std::out_of_range("biome index");
  return kBiomes[static_cast<std::size_t>(index)];
}

const DecorParams& decor_params(BiomeId id) {
  static const DecorParams kDefault{};
  static const DecorParams kPlains = [] {
    DecorParams p;
    p.trees = -999;
    p.flowers = 4;
    p.grass = 10;
    return p;
  }();
  static const DecorParams kDesert = [] {
    DecorParams p;
    p.trees = -999;
    p.dead_bush = 2;
    p.reeds = 50;
    p.cacti = 10;
    return p;
  }();
  static const DecorParams kForest = [] {
    DecorParams p;
    p.trees = 10;
    p.grass = 2;
    return p;
  }();
  static const DecorParams kSwamp = [] {
    DecorParams p;
    p.trees = 2;
    p.flowers = -999;
    p.dead_bush = 1;
    p.mushrooms = 8;
    p.reeds = 10;
    p.clay = 1;
    p.waterlily = 4;
    return p;
  }();
  static const DecorParams kTaiga = [] {
    DecorParams p;
    p.trees = 10;
    p.grass = 1;
    return p;
  }();
  static const DecorParams kMushroom = [] {
    DecorParams p;
    p.trees = -100;
    p.flowers = -100;
    p.grass = -100;
    p.mushrooms = 1;
    p.big_mushroom = 1;
    return p;
  }();
  switch (id) {
    case BiomeId::Plains:
      return kPlains;
    case BiomeId::Desert:
      return kDesert;
    case BiomeId::Forest:
      return kForest;
    case BiomeId::Swampland:
      return kSwamp;
    case BiomeId::Taiga:
      return kTaiga;
    case BiomeId::MushroomIsland:
      return kMushroom;
    default:
      return kDefault;
  }
}

TreeKind pick_tree(BiomeId id, JavaRandom& rand) {
  switch (id) {
    case BiomeId::Forest:
      if (rand.next_int(5) == 0) return TreeKind::Forest;
      if (rand.next_int(10) == 0) return TreeKind::Big;
      return TreeKind::Normal;
    case BiomeId::Swampland:
      return TreeKind::Swamp;
    case BiomeId::Taiga:
      if (rand.next_int(3) == 0) return TreeKind::Taiga1;
      return TreeKind::Taiga2;
    default:
      if (rand.next_int(10) == 0) return TreeKind::Big;
      return TreeKind::Normal;
  }
}

}  // namespace craftpp::world
