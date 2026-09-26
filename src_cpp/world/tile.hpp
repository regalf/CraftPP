#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/random.hpp"
#include "entity/items.hpp"

namespace craftpp::world::tile {

// TileEntity family (chest / furnace / sign). Positions mirror xCoord/yCoord/
// zCoord; update() runs each tick for furnaces. NBT save/load lands with
// McRegion (fields are plain and documented for it).
struct TileEntity {
  int x = 0, y = 0, z = 0;
  virtual ~TileEntity() = default;
  virtual int kind_id() const = 0;  // block id this entity belongs to
  virtual void update(class TileWorld& w) {}
};

struct ChestEntity : TileEntity {
  std::array<std::optional<entity::ItemStack>, 27> items{};
  int kind_id() const override;
};

struct FurnaceEntity : TileEntity {
  std::array<std::optional<entity::ItemStack>, 3> items{};
  int burn_time = 0;
  int current_burn = 0;
  int cook_time = 0;
  int kind_id() const override;
  bool burning() const { return burn_time > 0; }
  void update(TileWorld& w) override;
};

// Minimal services a TileEntity needs (implemented by LiveWorld).
struct TileWorld {
  virtual ~TileWorld() = default;
  virtual JavaRandom& world_rand() = 0;
  virtual void set_block_raw(int x, int y, int z, int id, int meta) = 0;
  virtual void mark_dirty(int x, int y, int z) = 0;
};

struct SignEntity : TileEntity {
  std::array<std::string, 4> lines{};
  int kind_id() const override;
};

// Burn times (TileEntityFurnace.getItemBurnTime): wood material 300,
// stick/sapling 100, coal 1600, lava bucket 20000, blaze rod 2400.
int burn_time_for(int shifted_item_id);
// Furnace smelting map (FurnaceRecipes): input shifted id -> output stack.
bool smelt_result(int shifted_input, entity::ItemStack& out);

}  // namespace craftpp::world::tile
