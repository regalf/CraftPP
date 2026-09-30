#pragma once

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "entity/items.hpp"
#include "entity/player.hpp"
#include "gui/crafting.hpp"
#include "gui/item_icons.hpp"
#include "world/block_place.hpp"
#include "world/blocks.hpp"

namespace craftpp::gui::ctn {

// Container/Slot port (Container.java + Slot/SlotArmor/SlotCrafting/
// SlotFurnace + the four concrete containers). Operates on plain backing
// arrays so tests need no world; the app wires tile storages + drop.
// Slot numbering mirrors the source exactly (see builders below).

enum class SlotKind {
  Normal,     // plain slot (craft inputs, storage, player)
  Armor,      // SlotArmor: type check + limit 1
  CraftOut,   // SlotCrafting: never valid + consumes matrix on pickup
  FurnaceOut  // SlotFurnace: never valid (output only)
};

// Backing store for one slot (IInventory slot access parity).
struct Backing {
  virtual ~Backing() = default;
  virtual std::optional<entity::ItemStack>* at(int idx) = 0;
  virtual int limit() const { return 64; }
  // Called after any write (crafting matrix recompute hook).
  virtual void changed(int idx) { (void)idx; }
};

struct VecBacking : Backing {
  std::vector<std::optional<entity::ItemStack>>* v = nullptr;
  explicit VecBacking(std::vector<std::optional<entity::ItemStack>>* vv) : v(vv) {}
  std::optional<entity::ItemStack>* at(int idx) override { return &(*v)[idx]; }
};

template <std::size_t N>
struct ArrBacking : Backing {
  std::array<std::optional<entity::ItemStack>, N>* a = nullptr;
  explicit ArrBacking(std::array<std::optional<entity::ItemStack>, N>* aa) : a(aa) {}
  std::optional<entity::ItemStack>* at(int idx) override { return &(*a)[idx]; }
};

struct Slot {
  Backing* backing = nullptr;
  int index = 0;
  int x = 0, y = 0;  // display position inside the panel
  SlotKind kind = SlotKind::Normal;
  int armor_type = -1;  // SlotArmor.armorType for Kind::Armor
  int number = 0;       // Container.slotNumber

  std::optional<entity::ItemStack>* stack() const { return backing->at(index); }
  bool has_stack() const {
    auto* s = stack();
    return s != nullptr && s->has_value();
  }
  bool valid(const entity::ItemStack& st) const {
    if (kind == SlotKind::CraftOut || kind == SlotKind::FurnaceOut) return false;
    if (kind == SlotKind::Armor)
      return world::edit::armor_type(st.item_id) == armor_type;
    return true;
  }
  int stack_limit() const {
    if (kind == SlotKind::Armor) return 1;
    return backing->limit();
  }
  void on_change() { backing->changed(index); }
};

using DropFn = std::function<void(entity::ItemStack&)>;

struct Container;

struct Container {
  std::vector<Slot> slots;
  Backing* matrix = nullptr;  // crafting grid (null for chest/furnace)
  int matrix_n = 0;
  int result_slot = -1;  // CraftOut slot number, -1 when absent
  // Consume 1 from each matrix cell + container-item return (SlotCrafting).
  std::function<void()> consume_craft;
  // Per-kind shift-click range dispatch (transferStackInSlot verbatim).
  std::function<std::optional<entity::ItemStack>(Container&, int)> shift;

  Slot* get(int n) {
    if (n < 0 || n >= static_cast<int>(slots.size())) return nullptr;
    return &slots[n];
  }

  // Container.mergeItemStack verbatim (note: no isItemValid gate, like the
  // source — shift-click can land in armor/fuel slots the mouse forbids).
  bool merge(entity::ItemStack& st, int lo, int hi, bool rev) {
    bool moved = false;
    int i = rev ? hi - 1 : lo;
    if (st.stackable()) {
      while (st.stack_size > 0 && (!rev && i < hi || rev && i >= lo)) {
        Slot* s = get(i);
        auto* o = s != nullptr ? s->stack() : nullptr;
        if (s != nullptr && o != nullptr && o->has_value() && o->value().item_id == st.item_id &&
            (!st.has_subtypes() || st.damage == o->value().damage)) {
          const int max = world::edit::item_max_stack(st.item_id);
          const int sum = o->value().stack_size + st.stack_size;
          if (sum <= max) {
            st.stack_size = 0;
            o->value().stack_size = sum;
            s->on_change();
            moved = true;
          } else if (o->value().stack_size < max) {
            st.stack_size -= max - o->value().stack_size;
            o->value().stack_size = max;
            s->on_change();
            moved = true;
          }
        }
        i += rev ? -1 : 1;
      }
    }
    if (st.stack_size > 0) {
      i = rev ? hi - 1 : lo;
      while (!rev && i < hi || rev && i >= lo) {
        Slot* s = get(i);
        auto* o = s != nullptr ? s->stack() : nullptr;
        if (s != nullptr && o != nullptr && !o->has_value()) {
          *o = st;
          s->on_change();
          st.stack_size = 0;
          moved = true;
          break;
        }
        i += rev ? -1 : 1;
      }
    }
    return moved;
  }

  // Container.transferStackInSlot tail shared by all kinds: clear/refresh
  // the slot, report no-change, run the pickup hook. Returns the pre-copy
  // (nullopt = nothing moved).
  std::optional<entity::ItemStack> shift_tail(int n, entity::ItemStack& st,
                                              const entity::ItemStack& pre) {
    Slot* s = get(n);
    if (st.stack_size == 0) {
      auto* o = s->stack();
      *o = std::nullopt;
      s->on_change();
    } else {
      s->on_change();
    }
    if (st.stack_size == pre.stack_size) return std::nullopt;
    pickup_hook(n, st);
    return pre;
  }

  // SlotCrafting/SlotFurnace onPickupFromSlot (achievements + stats skipped:
  // M5 leftover; container-item return + matrix consume kept).
  void pickup_hook(int n, entity::ItemStack& taken) {
    Slot* s = get(n);
    if (s == nullptr) return;
    if (s->kind == SlotKind::CraftOut && consume_craft) consume_craft();
    (void)taken;
  }

  // Container.slotClick verbatim. cursor = InventoryPlayer.itemStack.
  // Returns the pre-click copy (like the source return, unused by the GUI).
  std::optional<entity::ItemStack> click(int n, int button, bool sh,
                                         std::optional<entity::ItemStack>& cursor,
                                         const DropFn& drop) {
    std::optional<entity::ItemStack> ret;
    if (button > 1) return ret;
    if (button != 0 && button != 1) return ret;
    if (n == -999) {
      if (cursor.has_value()) {
        if (button == 0) {
          drop(*cursor);
          cursor = std::nullopt;
        } else {
          entity::ItemStack one = cursor->split(1);
          drop(one);
          if (cursor->stack_size == 0) cursor = std::nullopt;
        }
      }
      return ret;
    }
    if (sh) {
      // Verbatim retry: re-transfer while the slot still holds the same
      // item (partial moves); bounded to two passes.
      std::optional<entity::ItemStack> moved;
      for (int pass = 0; pass < 2 && sh; ++pass) {
        Slot* s = get(n);
        if (s == nullptr || !s->has_stack()) break;
        const int id = s->stack()->value().item_id;
        auto r = this->shift(*this, n);
        if (!r.has_value()) break;
        moved = r;
        Slot* s2 = get(n);
        if (s2 == nullptr || !s2->has_stack() || s2->stack()->value().item_id != id) break;
      }
      return moved;
    }
    if (n < 0) return ret;
    Slot* sl = get(n);
    if (sl == nullptr) return ret;
    sl->on_change();
    auto* slot_opt = sl->stack();
    const bool slot_has = slot_opt != nullptr && slot_opt->has_value();
    if (slot_has) ret = **slot_opt;
    const bool cur_has = cursor.has_value();
    if (!slot_has) {
      if (cur_has && sl->valid(*cursor)) {
        int take = button == 0 ? cursor->stack_size : 1;
        const int lim = sl->stack_limit();
        if (take > lim) take = lim;
        entity::ItemStack placed = cursor->split(take);
        *slot_opt = placed;
        sl->on_change();
        if (cursor->stack_size == 0) cursor = std::nullopt;
      }
      return ret;
    }
    if (!cur_has) {
      entity::ItemStack& st = **slot_opt;
      const int take = button == 0 ? st.stack_size : (st.stack_size + 1) / 2;
      entity::ItemStack got = st.split(take);
      cursor = got;
      if (st.stack_size == 0) {
        *slot_opt = std::nullopt;
        sl->on_change();
      }
      // onPickupFromSlot(cursor) for craft/furnace outputs.
      if (cursor.has_value()) pickup_hook(n, *cursor);
      return ret;
    }
    // Both sides occupied.
    entity::ItemStack& st = **slot_opt;
    if (sl->valid(*cursor)) {
      if (cursor->item_id != st.item_id ||
          (st.has_subtypes() && st.damage != cursor->damage)) {
        if (cursor->stack_size <= sl->stack_limit()) {
          entity::ItemStack tmp = st;
          st = *cursor;
          cursor = tmp;
          sl->on_change();
        }
      } else {
        int take = button == 0 ? cursor->stack_size : 1;
        const int room = sl->stack_limit() - st.stack_size;
        if (take > room) take = room;
        const int maxroom = world::edit::item_max_stack(st.item_id) - st.stack_size;
        if (take > maxroom) take = maxroom;
        cursor->split(take);
        if (cursor->stack_size == 0) cursor = std::nullopt;
        st.stack_size += take;
        sl->on_change();
      }
      return ret;
    }
    // Invalid target but same stackable id (armor-slot top-up quirk).
    if (cursor->item_id == st.item_id && world::edit::item_max_stack(st.item_id) > 1 &&
        (!st.has_subtypes() || st.damage == cursor->damage)) {
      const int have = st.stack_size;
      if (have > 0 && have + cursor->stack_size <= world::edit::item_max_stack(st.item_id)) {
        cursor->stack_size += have;
        st.split(have);
        if (st.stack_size == 0) {
          *slot_opt = std::nullopt;
          sl->on_change();
        }
        if (cursor.has_value()) pickup_hook(n, *cursor);
      }
    }
    return ret;
  }

  // onCraftGuiClosed verbatim: drop cursor + matrix contents.
  void close(const std::optional<entity::ItemStack>& cur, const DropFn& drop) {
    if (cur.has_value()) {
      entity::ItemStack c = *cur;
      drop(c);
    }
    if (matrix != nullptr) {
      for (int i = 0; i < matrix_n; ++i) {
        auto* o = matrix->at(i);
        if (o != nullptr && o->has_value()) {
          entity::ItemStack c = **o;
          drop(c);
          *o = std::nullopt;
        }
      }
    }
  }
};

// Shared shift-click core: transferStackInSlot bodies are verbatim range
// dispatches over this (copy, merge, clear/refresh, no-change check,
// pickup hook). Returns the pre-copy, nullopt = nothing moved.
inline std::optional<entity::ItemStack> shift_take(Container& c, int n, int lo, int hi,
                                                  bool rev) {
  Slot* s = c.get(n);
  if (s == nullptr || !s->has_stack()) return std::nullopt;
  entity::ItemStack& st = **s->stack();
  entity::ItemStack pre = st;
  if (!c.merge(st, lo, hi, rev)) return std::nullopt;
  return c.shift_tail(n, st, pre);
}

// Single optional backing (craft result).
struct SingleBacking : Backing {
  std::optional<entity::ItemStack>* s = nullptr;
  explicit SingleBacking(std::optional<entity::ItemStack>* ss) : s(ss) {}
  std::optional<entity::ItemStack>* at(int) override { return s; }
};

// Grid backing with a change hook (crafting result recompute).
struct GridBacking : VecBacking {
  std::function<void()> hook;
  GridBacking(std::vector<std::optional<entity::ItemStack>>* vv, std::function<void()> h)
      : VecBacking(vv), hook(h) {}
  void changed(int idx) override {
    (void)idx;
    if (hook) hook();
  }
};

// Open-container kit: owns the ephemeral grid/result + backing lifetime.
// Player inventory/armor and chest/furnace arrays stay owned by the world.
struct Kit {
  Container c;
  std::vector<std::optional<entity::ItemStack>> grid;
  std::optional<entity::ItemStack> result;
  std::vector<std::unique_ptr<Backing>> owned;
  bool small_grid = true;  // 2x2 player grid vs 3x3 bench

  Backing* keep(std::unique_ptr<Backing> b) {
    owned.push_back(std::move(b));
    return owned.back().get();
  }

  void recompute() {
    if (c.result_slot < 0) return;
    std::array<std::optional<entity::ItemStack>, 9> nine{};
    if (small_grid) {
      for (int r = 0; r < 2; ++r)
        for (int q = 0; q < 2; ++q) nine[r * 3 + q] = grid[r * 2 + q];
    } else {
      for (int i = 0; i < 9 && i < static_cast<int>(grid.size()); ++i) nine[i] = grid[i];
    }
    result = craft::find_match(nine);
  }

  void add(Backing* b, int idx, int x, int y, SlotKind k = SlotKind::Normal, int at = -1) {
    Slot s;
    s.backing = b;
    s.index = idx;
    s.x = x;
    s.y = y;
    s.kind = k;
    s.armor_type = at;
    s.number = static_cast<int>(c.slots.size());
    c.slots.push_back(s);
  }
};

inline void consume_matrix(Kit& k) {
  for (int i = 0; i < static_cast<int>(k.grid.size()); ++i) {
    auto& o = k.grid[i];
    if (!o.has_value()) continue;
    entity::ItemStack& st = *o;
    st.split(1);
    if (st.stack_size == 0) o = std::nullopt;
    // hasContainerItem: the remainder becomes the container item.
    const int cont = world::edit::container_item(st.item_id);
    if (cont != 0) o = entity::ItemStack(cont, 1, 0);
    if (o.has_value() && o->stack_size == 0) o = std::nullopt;
  }
  k.recompute();
}

// Verbose builders mirror the source constructors slot for slot.
inline void build_player(Kit& k, entity::Inventory& inv) {
  // Rebuilds are idempotent: reopening drops old slots//backings
  // (stale backings would dangle after grid reassignment).
  k.c.slots.clear();
  k.owned.clear();
  k.c.shift = nullptr;
  k.c.consume_craft = nullptr;
  k.c.matrix = nullptr;
  k.c.result_slot = -1;
  k.small_grid = true;
  k.grid.assign(4, std::nullopt);
  k.result = std::nullopt;
  auto* main_b = k.keep(std::make_unique<ArrBacking<36>>(&inv.main));
  auto* armor_b = k.keep(std::make_unique<ArrBacking<4>>(&inv.armor));
  auto* res_b = k.keep(std::make_unique<SingleBacking>(&k.result));
  auto* grid_b = k.keep(std::make_unique<GridBacking>(
      &k.grid, [&k]() { k.recompute(); }));
  k.c.matrix = grid_b;
  k.c.matrix_n = 4;
  k.c.result_slot = 0;
  k.c.consume_craft = [&k]() { consume_matrix(k); };
  k.add(res_b, 0, 144, 36, SlotKind::CraftOut);
  for (int r = 0; r < 2; ++r)
    for (int q = 0; q < 2; ++q) k.add(grid_b, q + r * 2, 88 + q * 18, 26 + r * 18);
  for (int r = 0; r < 4; ++r) k.add(armor_b, 3 - r, 8, 8 + r * 18, SlotKind::Armor, r);
  for (int r = 0; r < 3; ++r)
    for (int q = 0; q < 9; ++q) k.add(main_b, q + (r + 1) * 9, 8 + q * 18, 84 + r * 18);
  for (int q = 0; q < 9; ++q) k.add(main_b, q, 8 + q * 18, 142);
  k.c.shift = [](Container& c, int n) -> std::optional<entity::ItemStack> {
    if (n == 0) return shift_take(c, n, 9, 45, true);
    if (n >= 9 && n < 36) return shift_take(c, n, 36, 45, false);
    if (n >= 36 && n < 45) return shift_take(c, n, 9, 36, false);
    return shift_take(c, n, 9, 45, false);
  };
  k.recompute();
}

inline void build_workbench(Kit& k, entity::Inventory& inv) {
  // Rebuilds are idempotent: reopening drops old slots//backings
  // (stale backings would dangle after grid reassignment).
  k.c.slots.clear();
  k.owned.clear();
  k.c.shift = nullptr;
  k.c.consume_craft = nullptr;
  k.c.matrix = nullptr;
  k.c.result_slot = -1;
  k.small_grid = false;
  k.grid.assign(9, std::nullopt);
  k.result = std::nullopt;
  auto* main_b = k.keep(std::make_unique<ArrBacking<36>>(&inv.main));
  auto* res_b = k.keep(std::make_unique<SingleBacking>(&k.result));
  auto* grid_b = k.keep(std::make_unique<GridBacking>(
      &k.grid, [&k]() { k.recompute(); }));
  k.c.matrix = grid_b;
  k.c.matrix_n = 9;
  k.c.result_slot = 0;
  k.c.consume_craft = [&k]() { consume_matrix(k); };
  k.add(res_b, 0, 124, 35, SlotKind::CraftOut);
  for (int r = 0; r < 3; ++r)
    for (int q = 0; q < 3; ++q) k.add(grid_b, q + r * 3, 30 + q * 18, 17 + r * 18);
  for (int r = 0; r < 3; ++r)
    for (int q = 0; q < 9; ++q) k.add(main_b, q + r * 9 + 9, 8 + q * 18, 84 + r * 18);
  for (int q = 0; q < 9; ++q) k.add(main_b, q, 8 + q * 18, 142);
  k.c.shift = [](Container& c, int n) -> std::optional<entity::ItemStack> {
    if (n == 0) return shift_take(c, n, 10, 46, true);
    if (n >= 10 && n < 37) return shift_take(c, n, 37, 46, false);
    if (n >= 37 && n < 46) return shift_take(c, n, 10, 37, false);
    return shift_take(c, n, 10, 46, false);
  };
  k.recompute();
}

inline void build_furnace(Kit& k, entity::Inventory& inv,
                           std::array<std::optional<entity::ItemStack>, 3>& fz) {
  // Rebuilds are idempotent: reopening drops old slots//backings
  // (stale backings would dangle after grid reassignment).
  k.c.slots.clear();
  k.owned.clear();
  k.c.shift = nullptr;
  k.c.consume_craft = nullptr;
  k.c.matrix = nullptr;
  k.c.result_slot = -1;
  k.small_grid = false;
  k.grid.clear();
  k.result = std::nullopt;
  auto* main_b = k.keep(std::make_unique<ArrBacking<36>>(&inv.main));
  auto* fz_b = k.keep(std::make_unique<ArrBacking<3>>(&fz));
  k.c.matrix = nullptr;
  k.c.matrix_n = 0;
  k.c.result_slot = -1;
  k.add(fz_b, 0, 56, 17);
  k.add(fz_b, 1, 56, 53);
  k.add(fz_b, 2, 116, 35, SlotKind::FurnaceOut);
  for (int r = 0; r < 3; ++r)
    for (int q = 0; q < 9; ++q) k.add(main_b, q + r * 9 + 9, 8 + q * 18, 84 + r * 18);
  for (int q = 0; q < 9; ++q) k.add(main_b, q, 8 + q * 18, 142);
  k.c.shift = [](Container& c, int n) -> std::optional<entity::ItemStack> {
    if (n == 2) return shift_take(c, n, 3, 39, true);
    if (n >= 3 && n < 30) return shift_take(c, n, 30, 39, false);
    if (n >= 30 && n < 39) return shift_take(c, n, 3, 30, false);
    return shift_take(c, n, 3, 39, false);
  };
}

inline void build_chest(Kit& k, entity::Inventory& inv,
                         std::array<std::optional<entity::ItemStack>, 27>& ch) {
  // Rebuilds are idempotent: reopening drops old slots//backings
  // (stale backings would dangle after grid reassignment).
  k.c.slots.clear();
  k.owned.clear();
  k.c.shift = nullptr;
  k.c.consume_craft = nullptr;
  k.c.matrix = nullptr;
  k.c.result_slot = -1;
  k.small_grid = false;
  k.grid.clear();
  k.result = std::nullopt;
  auto* main_b = k.keep(std::make_unique<ArrBacking<36>>(&inv.main));
  auto* ch_b = k.keep(std::make_unique<ArrBacking<27>>(&ch));
  k.c.matrix = nullptr;
  k.c.matrix_n = 0;
  k.c.result_slot = -1;
  for (int r = 0; r < 3; ++r)
    for (int q = 0; q < 9; ++q) k.add(ch_b, q + r * 9, 8 + q * 18, 18 + r * 18);
  for (int r = 0; r < 3; ++r)
    for (int q = 0; q < 9; ++q)
      k.add(main_b, q + r * 9 + 9, 8 + q * 18, 103 + r * 18);
  for (int q = 0; q < 9; ++q) k.add(main_b, q, 8 + q * 18, 161);
  k.c.shift = [](Container& c, int n) -> std::optional<entity::ItemStack> {
    if (n < 27) return shift_take(c, n, 27, 63, true);
    return shift_take(c, n, 0, 27, false);
  };
}

// ---- creative picker (ContainerCreative/GuiContainerCreative) ----
// 72-cell visible window over the full item list + player hotbar.
// Grid clicks use the custom take/grow logic (NOT slotClick); shift is a
// no-op (func_35373_b empty override); outside drops like -999.
inline std::optional<entity::ItemStack> creative_click(
    Container& c, int n, int button, bool sh, std::optional<entity::ItemStack>& cursor,
    const DropFn& drop) {
  (void)sh;  // shift only matters for take-all-into-cursor (handled below)
  Slot* s = c.get(n);
  if (s == nullptr) return std::nullopt;
  const bool is_grid = n < 72;
  if (!is_grid) return c.click(n, button, false, cursor, drop);
  auto* cell_opt = s->stack();
  const bool cell_has = cell_opt != nullptr && cell_opt->has_value();
  const bool cur_has = cursor.has_value();
  entity::ItemStack* cell = cell_has ? &**cell_opt : nullptr;
  if (cur_has && cell_has && cursor->item_id == cell->item_id) {
    const int max = world::edit::item_max_stack(cursor->item_id);
    if (button == 0) {
      if (sh) {
        cursor->stack_size = max;
      } else if (cursor->stack_size < max) {
        ++cursor->stack_size;
      }
    } else {
      if (cursor->stack_size <= 1) {
        cursor = std::nullopt;
      } else {
        --cursor->stack_size;
      }
    }
    return std::nullopt;
  }
  if (cur_has) {
    cursor = std::nullopt;
    return std::nullopt;
  }
  if (!cell_has) {
    cursor = std::nullopt;
    return std::nullopt;
  }
  cursor = *cell;
  if (sh) cursor->stack_size = world::edit::item_max_stack(cursor->item_id);
  return std::nullopt;
}

// Scroll window refill (func_35374_a verbatim): rows = size/8-8+1,
// start = round(frac*rows) clamped >= 0.
inline void creative_scroll(Kit& k, const std::vector<entity::ItemStack>& list, float frac) {
  const int rows = static_cast<int>(list.size()) / 8 - 8 + 1;
  int start = static_cast<int>(frac * rows + 0.5F);
  if (start < 0) start = 0;
  for (int r = 0; r < 9; ++r) {
    for (int q = 0; q < 8; ++q) {
      const int li = q + (r + start) * 8;
      auto* o = k.c.get(r * 8 + q)->stack();
      if (li >= 0 && li < static_cast<int>(list.size()))
        *o = list[li];
      else
        *o = std::nullopt;
    }
  }
}

// Full creative item list in vanilla order (ContainerCreative verbatim:
// fixed block order with damage variants, all items except potion, dyes).
inline std::vector<entity::ItemStack> creative_item_list() {
  using namespace world::bid;
  std::vector<entity::ItemStack> out;
  struct E {
    int id, n;
  };
  const E blocks[] = {
      {kCobble, 1}, {kStone, 1}, {kDiamondOre, 1}, {kGoldOre, 1}, {kIronOre, 1},
      {kCoalOre, 1}, {kLapisOre, 1}, {kRedstoneOre, 1}, {kStoneBrick, 3}, {kClay, 1},
      {kDiamondBlock, 1}, {kGoldBlock, 1}, {kSteelBlock, 1}, {kBedrock, 1}, {kLapisBlock, 1},
      {kBrick, 1}, {kCobbleMossy, 1}, {kStepSingle, 6}, {kObsidian, 1}, {kNetherrack, 1},
      {kSoulSand, 1}, {kGlowstone, 1}, {kLog, 3}, {kLeaves, 3}, {kDirt, 1}, {kGrass, 1},
      {kSand, 1}, {kSandstone, 1}, {kGravel, 1}, {kWeb, 1}, {kWoodPlank, 1}, {kSapling, 3},
      {kDeadBush, 1}, {kSponge, 1}, {kIce, 1}, {kSnowBlock, 1}, {kFlowerYellow, 1},
      {kFlowerRed, 1}, {kMushroomBrown, 1}, {kMushroomRed, 1}, {kCactus, 1}, {kMelon, 1},
      {kPumpkin, 1}, {kPumpkinLantern, 1}, {kVine, 1}, {kPaneIron, 1}, {kPaneGlass, 1},
      {kNetherBrick, 1}, {kNetherFence, 1}, {kStairsNether, 1}, {kWhiteStone, 1},
      {kMycelium, 1}, {kLilyPad, 1}, {kTallGrass, 2}, {kChest, 1}, {kWorkbench, 1},
      {kGlass, 1}, {kTnt, 1}, {kBookshelf, 1}, {kWool, 16}, {kDispenser, 1},
      {kFurnaceIdle, 1}, {kNoteBlock, 1}, {kJukebox, 1}, {kPistonSticky, 1}, {kPistonBase, 1},
      {kFence, 1}, {kFenceGate, 1}, {kLadder, 1}, {kRail, 1}, {kRailPowered, 1},
      {kRailDetector, 1}, {kTorch, 1}, {kStairsWood, 1}, {kStairsCobble, 1}, {kStairsBrick, 1},
      {kStairsStoneBrick, 1}, {kLever, 1}, {kPlateStone, 1}, {kPlateWood, 1},
      {kTorchRedOn, 1}, {kButton, 1}, {kTrapDoor, 1}, {kEnchantTable, 1},
  };
  int cloth = 0, slab = 0, wood = 0, sapling = 0, brick = 0, grass = 1, leaves = 0;
  for (const E& e : blocks) {
    for (int k = 0; k < e.n; ++k) {
      int dmg = 0;
      if (e.id == kWool)
        dmg = cloth++;
      else if (e.id == kStepSingle)
        dmg = slab++;
      else if (e.id == kLog)
        dmg = wood++;
      else if (e.id == kSapling)
        dmg = sapling++;
      else if (e.id == kStoneBrick)
        dmg = brick++;
      else if (e.id == kTallGrass)
        dmg = grass++;
      else if (e.id == kLeaves)
        dmg = leaves++;
      out.emplace_back(e.id, 1, dmg);
    }
  }
  for (int id = 256; id <= 382; ++id) {
    if (id == 373) continue;  // potion excluded like the source
    if (craftpp::gui::item_sprite_index(id) < 0) continue;
    out.emplace_back(id, 1, 0);
  }
  for (int dmg = 1; dmg < 16; ++dmg) out.emplace_back(351, 1, dmg);
  return out;
}

inline void build_creative(Kit& k, entity::Inventory& inv) {
  // Rebuilds are idempotent: reopening drops old slots//backings
  // (stale backings would dangle after grid reassignment).
  k.c.slots.clear();
  k.owned.clear();
  k.c.shift = nullptr;
  k.c.consume_craft = nullptr;
  k.c.matrix = nullptr;
  k.c.result_slot = -1;
  k.small_grid = false;
  k.grid.assign(72, std::nullopt);
  k.result = std::nullopt;
  auto* main_b = k.keep(std::make_unique<ArrBacking<36>>(&inv.main));
  auto* grid_b = k.keep(std::make_unique<GridBacking>(&k.grid, []() {}));
  k.c.matrix = nullptr;
  k.c.matrix_n = 0;
  k.c.result_slot = -1;
  for (int r = 0; r < 9; ++r)
    for (int q = 0; q < 8; ++q) k.add(grid_b, q + r * 8, 8 + q * 18, 18 + r * 18);
  for (int q = 0; q < 9; ++q) k.add(main_b, q, 8 + q * 18, 184);
  // Shift-click is a no-op in the picker (empty func_35373_b override).
  k.c.shift = [](Container&, int) -> std::optional<entity::ItemStack> { return std::nullopt; };
}

}  // namespace craftpp::gui::ctn
