#pragma once

// Item sprite indices into gui/items.png (16x16 tiles, index = x + y*16).
// Generated from Item.java setIconCoord chains; blocks (< 256) use the
// terrain atlas instead. Returns -1 for unknown ids.
namespace craftpp::gui {

inline int item_sprite_index(int shifted_id) {
  static const int kDense[] = {
    82, 98, 114, 5, 10, 21, 37, 7, 55, 23, 39, 66, 64, 80, 96, 112,
    65, 81, 97, 113, 67, 83, 99, 115, 53, 71, 72, 68, 84, 100, 116, 8,
    24, 40, 128, 129, 130, 131, 132, 9, 25, 41, 0, 16, 32, 48, 1, 17,
    33, 49, 2, 18, 34, 50, 3, 19, 35, 51, 4, 20, 36, 52, 6, 87,
    88, 26, 11, 42, 43, 74, 75, 76, 135, 104, 44, 56, 14, 136, 103, 77,
    22, 57, 27, 58, 59, 30, 151, 167, 12, 54, 69, 70, 73, 89, 90, 78,
    28, 13, 29, 45, 86, 92, 60, 93, 109, 61, 62, 105, 106, 121, 122, 91,
    107, 108, 123, 124, 125, 141, 140, 139, 138, 157, 173, 172, 156, 155, 137,
  };
  if (shifted_id >= 256 && shifted_id <= 382) return kDense[shifted_id - 256];
  if (shifted_id == 2256) return 240;
  if (shifted_id == 2257) return 241;
  if (shifted_id == 2258) return 242;
  if (shifted_id == 2259) return 243;
  if (shifted_id == 2260) return 244;
  if (shifted_id == 2261) return 245;
  if (shifted_id == 2262) return 246;
  if (shifted_id == 2263) return 247;
  if (shifted_id == 2264) return 248;
  if (shifted_id == 2265) return 249;
  if (shifted_id == 2266) return 250;
  return -1;
}

}  // namespace craftpp::gui
