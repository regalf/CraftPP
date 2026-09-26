#pragma once

#include <array>
#include <optional>
#include <vector>

#include "entity/items.hpp"

namespace craftpp::craft {

// Crafting recipe engine (ShapedRecipes/ShapelessRecipes + CraftingManager
// matching, including mirroring and the -1 damage wildcard). Grids are 3x3
// row-major like InventoryCrafting (player 2x2 crafting maps into a corner).
struct Cell {
  int id = 0;      // 0 = empty slot
  int damage = -1;  // -1 = any damage
};

struct Recipe {
  bool shaped = true;
  int out_id = 0;
  int out_count = 1;
  int out_damage = 0;
  int w = 0, h = 0;  // shaped dims
  std::vector<Cell> cells;  // shaped: row-major w*h; shapeless: ingredient list
  int size() const { return shaped ? w * h : static_cast<int>(cells.size()); }
};

// Full 1.0 table, sorted like RecipeSorter (shaped first, larger first).
const std::vector<Recipe>& table();

// First matching recipe output for the grid (nullopt = no match).
std::optional<entity::ItemStack> find_match(
    const std::array<std::optional<entity::ItemStack>, 9>& grid);

}  // namespace craftpp::craft
