#include "gui/crafting.hpp"

#include <algorithm>
#include <string>
#include <tuple>

#include "world/blocks.hpp"
#include "world/item_ids.hpp"

namespace craftpp::craft {
namespace {

using namespace craftpp::world::bid;
using namespace craftpp::iid;
using entity::ItemStack;

struct Builder {
  std::vector<Recipe> out;
  // Shaped: rows + (char,id,damage) triples.
  void shaped(int oid, int n, int od, std::vector<std::string> rows,
              std::vector<std::tuple<char, int, int>> map) {
    Recipe r;
    r.shaped = true;
    r.out_id = oid;
    r.out_count = n;
    r.out_damage = od;
    r.h = static_cast<int>(rows.size());
    r.w = 0;
    for (auto& s : rows) r.w = std::max(r.w, static_cast<int>(s.size()));
    for (auto& s : rows) {
      for (int i = 0; i < r.w; ++i) {
        Cell c;
        if (i < (int)s.size() && s[i] != ' ') {
          for (auto& [ch, id, dg] : map)
            if (ch == s[i]) {
              c.id = id;
              c.damage = dg;
            }
        }
        r.cells.push_back(c);
      }
    }
    out.push_back(r);
  }
  void shapeless(int oid, int n, int od, std::vector<std::pair<int, int>> items) {
    Recipe r;
    r.shaped = false;
    r.out_id = oid;
    r.out_count = n;
    r.out_damage = od;
    for (auto& [id, dg] : items) r.cells.push_back(Cell{id, dg});
    out.push_back(r);
  }
};

std::vector<Recipe> build_table() {
  Builder b;
  // ---- tools (RecipesTools patterns x materials) ----
  // Expanded explicitly (materials: planks/cobble/iron/diamond/gold).
  const int mat_ids[5] = {kWoodPlank, kCobble, kIngotIron, kDiamond, kIngotGold};
  const int pick_ids[5] = {kPickaxeWood, kPickaxeStone, kPickaxeSteel, kPickaxeDiamond,
                           kPickaxeGold};
  const int shovel_ids[5] = {kShovelWood, kShovelStone, kShovelSteel, kShovelDiamond,
                             kShovelGold};
  const int axe_ids[5] = {kAxeWood, kAxeStone, kAxeSteel, kAxeDiamond, kAxeGold};
  const int hoe_ids[5] = {kHoeWood, kHoeStone, kHoeSteel, kHoeDiamond, kHoeGold};
  const int sword_ids[5] = {kSwordWood, kSwordStone, kSwordSteel, kSwordDiamond, kSwordGold};
  for (int m = 0; m < 5; ++m) {
    const int M = mat_ids[m];
    b.shaped(pick_ids[m], 1, 0, {"XXX", " # ", " # "}, {{'X', M, -1}, {'#', kStick, -1}});
    b.shaped(shovel_ids[m], 1, 0, {"X", "#", "#"}, {{'X', M, -1}, {'#', kStick, -1}});
    b.shaped(axe_ids[m], 1, 0, {"XX", "X#", " #"}, {{'X', M, -1}, {'#', kStick, -1}});
    b.shaped(hoe_ids[m], 1, 0, {"XX", " #", " #"}, {{'X', M, -1}, {'#', kStick, -1}});
    b.shaped(sword_ids[m], 1, 0, {"X", "X", "#"}, {{'X', M, -1}, {'#', kStick, -1}});
  }
  b.shaped(kShears, 1, 0, {" #", "# "}, {{'#', kIngotIron, -1}});
  b.shaped(kBow, 1, 0, {" #X", "# X", " #X"}, {{'X', kSilk, -1}, {'#', kStick, -1}});
  b.shaped(kArrow, 4, 0, {"X", "#", "Y"},
           {{'Y', kFeather, -1}, {'X', kFlint, -1}, {'#', kStick, -1}});
  // ---- ingots/blocks ----
  const int blocks4[4] = {kGoldBlock, kSteelBlock, kDiamondBlock, kLapisBlock};
  const int ing9[4] = {kIngotGold, kIngotIron, kDiamond, kDyePowder};
  const int ing9d[4] = {-1, -1, -1, 4};
  for (int m = 0; m < 4; ++m) {
    b.shaped(blocks4[m], 1, 0, {"###", "###", "###"}, {{'#', ing9[m], ing9d[m]}});
    b.shaped(ing9[m], ing9d[m] < 0 ? 9 : 9, ing9d[m] < 0 ? 0 : ing9d[m], {"#"},
             {{'#', blocks4[m], -1}});
  }
  b.shaped(kIngotGold, 1, 0, {"###", "###", "###"}, {{'#', kGoldNugget, -1}});
  b.shaped(kGoldNugget, 9, 0, {"#"}, {{'#', kIngotGold, -1}});
  // ---- armor (patterns x materials; chain = fire block) ----
  const int arm_mat[5] = {kLeather, kFire, kIngotIron, kDiamond, kIngotGold};
  const int helm[5] = {kHelmetLeather, kHelmetChain, kHelmetSteel, kHelmetDiamond, kHelmetGold};
  const int chest[5] = {kPlateLeather, kPlateChain, kPlateSteel, kPlateDiamond, kPlateGold};
  const int legs[5] = {kLegsLeather, kLegsChain, kLegsSteel, kLegsDiamond, kLegsGold};
  const int boots[5] = {kBootsLeather, kBootsChain, kBootsSteel, kBootsDiamond, kBootsGold};
  for (int m = 0; m < 5; ++m) {
    const int M = arm_mat[m];
    b.shaped(helm[m], 1, 0, {"XXX", "X X"}, {{'X', M, -1}});
    b.shaped(chest[m], 1, 0, {"X X", "XXX", "XXX"}, {{'X', M, -1}});
    b.shaped(legs[m], 1, 0, {"XXX", "X X", "X X"}, {{'X', M, -1}});
    b.shaped(boots[m], 1, 0, {"X X", "X X"}, {{'X', M, -1}});
  }
  // ---- food ----
  b.shapeless(kBowlSoup, 1, 0, {{kMushroomBrown, -1}, {kMushroomRed, -1}, {kBowlEmpty, -1}});
  b.shaped(kCookie, 8, 0, {"#X#"}, {{'X', kDyePowder, 3}, {'#', kWheat, -1}});
  b.shaped(craftpp::world::bid::kMelon, 1, 0, {"MMM", "MMM", "MMM"}, {{'M', iid::kMelon, -1}});
  b.shaped(kMelonSeeds, 1, 0, {"M"}, {{'M', iid::kMelon, -1}});
  b.shaped(kPumpkinSeeds, 4, 0, {"M"}, {{'M', kPumpkin, -1}});
  b.shapeless(kFermentedSpiderEye, 1, 0,
              {{kSpiderEye, -1}, {kMushroomBrown, -1}, {kSugar, -1}});
  b.shapeless(kSpeckledMelon, 1, 0, {{iid::kMelon, -1}, {kGoldNugget, -1}});
  b.shapeless(kBlazePowder, 2, 0, {{kBlazeRod, -1}});
  b.shapeless(kMagmaCream, 1, 0, {{kBlazePowder, -1}, {kSlimeBall, -1}});
  // ---- dyes ----
  for (int d = 0; d < 16; ++d) {
    // BlockCloth.getDyeFromBlock: wool damage = dye index color mapping.
    b.shapeless(kWool, 1, 15 - d, {{kDyePowder, d}, {kWool, 0}});
  }
  b.shapeless(kDyePowder, 2, 11, {{kFlowerYellow, -1}});
  b.shapeless(kDyePowder, 2, 1, {{kFlowerRed, -1}});
  b.shapeless(kDyePowder, 3, 15, {{kBone, -1}});
  auto dye2 = [&](int o, int a, int c) {
    b.shapeless(kDyePowder, 2, o, {{kDyePowder, a}, {kDyePowder, c}});
  };
  dye2(9, 1, 15);
  dye2(14, 1, 11);
  dye2(10, 2, 15);
  dye2(8, 0, 15);
  dye2(7, 8, 15);
  b.shapeless(kDyePowder, 3, 7, {{kDyePowder, 0}, {kDyePowder, 15}, {kDyePowder, 15}});
  dye2(12, 4, 15);
  dye2(6, 4, 2);
  dye2(5, 4, 1);
  dye2(13, 5, 9);
  b.shapeless(kDyePowder, 3, 13, {{kDyePowder, 4}, {kDyePowder, 1}, {kDyePowder, 9}});
  b.shapeless(kDyePowder, 4, 13,
              {{kDyePowder, 4}, {kDyePowder, 1}, {kDyePowder, 1}, {kDyePowder, 15}});
  // ---- crafting blocks ----
  b.shaped(kChest, 1, 0, {"###", "# #", "###"}, {{'#', kWoodPlank, -1}});
  b.shaped(kFurnaceIdle, 1, 0, {"###", "# #", "###"}, {{'#', kCobble, -1}});
  b.shaped(kWorkbench, 1, 0, {"##", "##"}, {{'#', kWoodPlank, -1}});
  b.shaped(kSandstone, 1, 0, {"##", "##"}, {{'#', kSand, -1}});
  b.shaped(kStoneBrick, 4, 0, {"##", "##"}, {{'#', kStone, -1}});
  b.shaped(kPaneIron, 16, 0, {"###", "###"}, {{'#', kIngotIron, -1}});
  b.shaped(kPaneGlass, 16, 0, {"###", "###"}, {{'#', kGlass, -1}});
  // ---- the long CraftingManager tail ----
  b.shaped(kPaper, 3, 0, {"###"}, {{'#', iid::kReed, -1}});
  b.shaped(kBook, 1, 0, {"#", "#", "#"}, {{'#', kPaper, -1}});
  b.shaped(kFence, 2, 0, {"###", "###"}, {{'#', kStick, -1}});
  b.shaped(kNetherFence, 6, 0, {"###", "###"}, {{'#', kNetherBrick, -1}});
  b.shaped(kFenceGate, 1, 0, {"#W#", "#W#"}, {{'#', kStick, -1}, {'W', kWoodPlank, -1}});
  b.shaped(kJukebox, 1, 0, {"###", "#X#", "###"}, {{'#', kWoodPlank, -1}, {'X', kDiamond, -1}});
  b.shaped(kNoteBlock, 1, 0, {"###", "#X#", "###"}, {{'#', kWoodPlank, -1}, {'X', kRedstone, -1}});
  b.shaped(kBookshelf, 1, 0, {"###", "XXX", "###"}, {{'#', kWoodPlank, -1}, {'X', kBook, -1}});
  b.shaped(kSnowBlock, 1, 0, {"##", "##"}, {{'#', kSnowball, -1}});
  // Clay/brick blocks from their items (different ids than the blocks).
  b.shaped(craftpp::world::bid::kClay, 1, 0, {"##", "##"}, {{'#', iid::kClay, -1}});
  b.shaped(craftpp::world::bid::kBrick, 1, 0, {"##", "##"}, {{'#', iid::kBrick, -1}});
  b.shaped(kGlowstone, 1, 0, {"##", "##"}, {{'#', kLightStoneDust, -1}});
  b.shaped(kWool, 1, 0, {"##", "##"}, {{'#', kSilk, -1}});
  b.shaped(kTnt, 1, 0, {"X#X", "#X#", "X#X"}, {{'X', kGunpowder, -1}, {'#', kSand, -1}});
  b.shaped(kStepSingle, 3, 3, {"###"}, {{'#', kCobble, -1}});
  b.shaped(kStepSingle, 3, 0, {"###"}, {{'#', kStone, -1}});
  b.shaped(kStepSingle, 3, 1, {"###"}, {{'#', kSandstone, -1}});
  b.shaped(kStepSingle, 3, 2, {"###"}, {{'#', kWoodPlank, -1}});
  b.shaped(kStepSingle, 3, 4, {"###"}, {{'#', craftpp::world::bid::kBrick, -1}});
  b.shaped(kStepSingle, 3, 5, {"###"}, {{'#', kStoneBrick, -1}});
  b.shaped(kLadder, 2, 0, {"# #", "###", "# #"}, {{'#', kStick, -1}});
  b.shaped(iid::kDoorWood, 1, 0, {"##", "##", "##"}, {{'#', kWoodPlank, -1}});
  b.shaped(kTrapDoor, 2, 0, {"###", "###"}, {{'#', kWoodPlank, -1}});
  b.shaped(iid::kDoorSteel, 1, 0, {"##", "##", "##"}, {{'#', kIngotIron, -1}});
  b.shaped(kSign, 1, 0, {"###", "###", " X "}, {{'#', kWoodPlank, -1}, {'X', kStick, -1}});
  b.shaped(iid::kCake, 1, 0, {"AAA", "BEB", "CCC"},
           {{'A', kBucketMilk, -1}, {'B', kSugar, -1}, {'C', kWheat, -1}, {'E', kEgg, -1}});
  b.shaped(kSugar, 1, 0, {"#"}, {{'#', iid::kReed, -1}});
  b.shaped(kWoodPlank, 4, 0, {"#"}, {{'#', kLog, -1}});
  b.shaped(kStick, 4, 0, {"#", "#"}, {{'#', kWoodPlank, -1}});
  b.shaped(kTorch, 4, 0, {"X", "#"}, {{'X', kCoal, -1}, {'#', kStick, -1}});
  b.shaped(kTorch, 4, 0, {"X", "#"}, {{'X', kCoal, 1}, {'#', kStick, -1}});
  b.shaped(kBowlEmpty, 4, 0, {"# #", " # "}, {{'#', kWoodPlank, -1}});
  b.shaped(kGlassBottle, 3, 0, {"# #", " # "}, {{'#', kGlass, -1}});
  b.shaped(kRail, 16, 0, {"X X", "X#X", "X X"}, {{'X', kIngotIron, -1}, {'#', kStick, -1}});
  b.shaped(kRailPowered, 6, 0, {"X X", "X#X", "XRX"},
           {{'X', kIngotGold, -1}, {'R', kRedstone, -1}, {'#', kStick, -1}});
  b.shaped(kRailDetector, 6, 0, {"X X", "X#X", "XRX"},
           {{'X', kIngotIron, -1}, {'R', kRedstone, -1}, {'#', kPlateStone, -1}});
  b.shaped(kMinecartEmpty, 1, 0, {"# #", "###"}, {{'#', kIngotIron, -1}});
  b.shaped(iid::kCauldron, 1, 0, {"# #", "# #", "###"}, {{'#', kIngotIron, -1}});
  b.shaped(iid::kBrewingStand, 1, 0, {" B ", "###"}, {{'#', kCobble, -1}, {'B', kBlazeRod, -1}});
  b.shaped(kPumpkinLantern, 1, 0, {"A", "B"}, {{'A', kPumpkin, -1}, {'B', kTorch, -1}});
  b.shaped(kMinecartCrate, 1, 0, {"A", "B"}, {{'A', kChest, -1}, {'B', kMinecartEmpty, -1}});
  b.shaped(kMinecartPowered, 1, 0, {"A", "B"}, {{'A', kFurnaceIdle, -1}, {'B', kMinecartEmpty, -1}});
  b.shaped(kBoat, 1, 0, {"# #", "###"}, {{'#', kWoodPlank, -1}});
  b.shaped(kBucketEmpty, 1, 0, {"# #", " # "}, {{'#', kIngotIron, -1}});
  b.shaped(kFlintAndSteel, 1, 0, {"A ", " B"}, {{'A', kIngotIron, -1}, {'B', kFlint, -1}});
  b.shaped(kBread, 1, 0, {"###"}, {{'#', kWheat, -1}});
  b.shaped(kStairsWood, 4, 0, {"#  ", "## ", "###"}, {{'#', kWoodPlank, -1}});
  b.shaped(kFishingRod, 1, 0, {"  #", " #X", "# X"}, {{'#', kStick, -1}, {'X', kSilk, -1}});
  b.shaped(kStairsCobble, 4, 0, {"#  ", "## ", "###"}, {{'#', kCobble, -1}});
  b.shaped(kStairsBrick, 4, 0, {"#  ", "## ", "###"}, {{'#', craftpp::world::bid::kBrick, -1}});
  b.shaped(kStairsStoneBrick, 4, 0, {"#  ", "## ", "###"}, {{'#', kStoneBrick, -1}});
  b.shaped(kStairsNether, 4, 0, {"#  ", "## ", "###"}, {{'#', kNetherBrick, -1}});
  b.shaped(kPainting, 1, 0, {"###", "#X#", "###"}, {{'#', kStick, -1}, {'X', kWool, -1}});
  b.shaped(kAppleGold, 1, 0, {"###", "#X#", "###"}, {{'#', kGoldBlock, -1}, {'X', kAppleRed, -1}});
  b.shaped(kLever, 1, 0, {"X", "#"}, {{'#', kCobble, -1}, {'X', kStick, -1}});
  b.shaped(kTorchRedOn, 1, 0, {"X", "#"}, {{'#', kStick, -1}, {'X', kRedstone, -1}});
  b.shaped(iid::kRedstoneRepeater, 1, 0, {"#X#", "III"},
           {{'#', kTorchRedOn, -1}, {'X', kRedstone, -1}, {'I', kStone, -1}});
  // NOTE: source pattern is {"#X#","III"} (2 rows); kept exact.
  b.shaped(kPocketSundial, 1, 0, {" # ", "#X#", " # "},
           {{'#', kIngotGold, -1}, {'X', kRedstone, -1}});
  b.shaped(kCompass, 1, 0, {" # ", "#X#", " # "}, {{'#', kIngotIron, -1}, {'X', kRedstone, -1}});
  b.shaped(kMap, 1, 0, {"###", "#X#", "###"}, {{'#', kPaper, -1}, {'X', kCompass, -1}});
  b.shaped(kButton, 1, 0, {"#", "#"}, {{'#', kStone, -1}});
  b.shaped(kPlateStone, 1, 0, {"##"}, {{'#', kStone, -1}});
  b.shaped(kPlateWood, 1, 0, {"##"}, {{'#', kWoodPlank, -1}});
  b.shaped(kDispenser, 1, 0, {"###", "#X#", "#R#"},
           {{'#', kCobble, -1}, {'X', kBow, -1}, {'R', kRedstone, -1}});
  b.shaped(kPistonBase, 1, 0, {"TTT", "#X#", "#R#"},
           {{'#', kCobble, -1}, {'X', kIngotIron, -1}, {'R', kRedstone, -1}, {'T', kWoodPlank, -1}});
  b.shaped(kPistonSticky, 1, 0, {"S", "P"}, {{'S', kSlimeBall, -1}, {'P', kPistonBase, -1}});
  b.shaped(iid::kBed, 1, 0, {"###", "XXX"}, {{'#', kWool, -1}, {'X', kWoodPlank, -1}});
  b.shaped(kEnchantTable, 1, 0, {" B ", "D#D", "###"},
           {{'#', kObsidian, -1}, {'B', kBook, -1}, {'D', kDiamond, -1}});
  b.shapeless(kEyeOfEnder, 1, 0, {{kEnderPearl, -1}, {kBlazePowder, -1}});
  // RecipeSorter: shaped before shapeless, larger size first (stable).
  std::stable_sort(b.out.begin(), b.out.end(), [](const Recipe& a, const Recipe& c) {
    if (a.shaped != c.shaped) return a.shaped > c.shaped;
    return a.size() > c.size();
  });
  return b.out;
}

bool match_shaped(const Recipe& r,
                  const std::array<std::optional<ItemStack>, 9>& grid) {
  auto cell = [&](int col, int row) -> const std::optional<ItemStack>& { return grid[row * 3 + col]; };
  for (int ox = 0; ox <= 3 - r.w; ++ox)
    for (int oy = 0; oy <= 3 - r.h; ++oy)
      for (int mirror = 0; mirror < 2; ++mirror) {
        bool ok = true;
        for (int x = 0; x < 3 && ok; ++x)
          for (int y = 0; y < 3 && ok; ++y) {
            const int rx = x - ox, ry = y - oy;
            const Cell* want = nullptr;
            if (rx >= 0 && ry >= 0 && rx < r.w && ry < r.h) {
              want = mirror ? &r.cells[(r.w - rx - 1) + ry * r.w] : &r.cells[rx + ry * r.w];
            }
            const auto& got = cell(x, y);
            const bool want_empty = (want == nullptr || want->id == 0);
            if (!got.has_value() || got->stack_size <= 0) {
              if (!want_empty) ok = false;
            } else if (want_empty) {
              ok = false;
            } else if (want->id != got->item_id) {
              ok = false;
            } else if (want->damage != -1 && want->damage != got->damage) {
              ok = false;
            }
          }
        if (ok) return true;
      }
  return false;
}

bool match_shapeless(const Recipe& r,
                     const std::array<std::optional<ItemStack>, 9>& grid) {
  std::vector<Cell> need = r.cells;
  for (auto& s : grid) {
    if (!s.has_value() || s->stack_size <= 0) continue;
    bool hit = false;
    for (auto it = need.begin(); it != need.end(); ++it) {
      if (s->item_id == it->id && (it->damage == -1 || s->damage == it->damage)) {
        need.erase(it);
        hit = true;
        break;
      }
    }
    if (!hit) return false;
  }
  return need.empty();
}

}  // namespace

const std::vector<Recipe>& table() {
  static const std::vector<Recipe> t = build_table();
  return t;
}

std::optional<entity::ItemStack> find_match(
    const std::array<std::optional<entity::ItemStack>, 9>& grid) {
  for (auto& r : table()) {
    const bool ok = r.shaped ? match_shaped(r, grid) : match_shapeless(r, grid);
    if (ok) return entity::ItemStack(r.out_id, r.out_count, r.out_damage);
  }
  return std::nullopt;
}

}  // namespace craftpp::craft
