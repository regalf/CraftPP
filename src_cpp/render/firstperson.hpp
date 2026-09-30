#pragma once

#include <cmath>

#include "gui/item_icons.hpp"
#include "render/item_mesh.hpp"
#include "render/mesh.hpp"
#include "render/model.hpp"
#include "world/blocks.hpp"

namespace craftpp::render {

// First-person hand + held item (ItemRenderer.renderItemInFirstPerson port,
// minus maps/eat/drink/block/bow use-poses which need the item-use state
// machine (M5 leftover), and minus the renderArm view-lag micro-rotation).
// Chains run in camera space; brightness is forced fullbright like the
// source (var6 = 1.0F). Meshes split by texture: atlas (block items),
// items (flat icons), skin (arm).
struct FirstPersonMeshes {
  Mesh atlas;
  Mesh items;
  Mesh skin;
};

// held_id <= 0: empty hand (arm only). swing/equip in 0..1 (already
// partial-interpolated by the caller).
inline void build_first_person(FirstPersonMeshes& out, int held_id, int held_damage, float equip,
                               float swing) {
  using namespace item_mesh;
  const float sswing = std::sin(swing * 3.14159265F);
  const float sswing_sqrt = std::sin(std::sqrt(std::max(swing, 0.0F)) * 3.14159265F);
  const float sswing_sq = std::sin(swing * swing * 3.14159265F);
  if (held_id > 0) {
    Mat4 t;
    t.translate(-sswing_sqrt * 0.4F,
                std::sin(std::sqrt(std::max(swing, 0.0F)) * 3.14159265F * 2.0F) * 0.2F,
                -sswing * 0.2F);
    t.translate(0.7F * 0.8F, -0.65F * 0.8F - (1.0F - equip) * 0.6F, -0.9F * 0.8F);
    t.rotate(45.0F, 0.0F, 1.0F, 0.0F);
    t.rotate(-sswing_sq * 20.0F, 0.0F, 1.0F, 0.0F);
    t.rotate(-sswing_sqrt * 20.0F, 0.0F, 0.0F, 1.0F);
    t.rotate(-sswing_sqrt * 80.0F, 1.0F, 0.0F, 0.0F);
    t.scale(0.4F, 0.4F, 0.4F);
    if (held_id == 346) t.rotate(180.0F, 0.0F, 1.0F, 0.0F);  // fishing rod
    const bool is_block = held_id < 256;
    const int rt = is_block ? world::bid::render_type(held_id) : -1;
    const bool as_cube =
        is_block && world::bid::render_item_in_3d(rt);
    if (as_cube) {
      emit_unit_cube(out.atlas, t, held_id, held_damage, 1.0F);
    } else {
      int tile = 0;
      Mesh* mesh = &out.items;
      if (is_block) {
        tile = world::bid::block_texture(held_id, 2, held_damage);
        mesh = &out.atlas;
      } else {
        tile = craftpp::gui::item_sprite_index(held_id);
        if (tile < 0) return;
      }
      // renderItem inner chain + func_40686_a extrusion (1/16 thick).
      Mat4 it = t;
      it.translate(0.0F, -0.3F, 0.0F);
      it.scale(1.5F, 1.5F, 1.5F);
      it.rotate(50.0F, 0.0F, 1.0F, 0.0F);
      it.rotate(335.0F, 0.0F, 0.0F, 1.0F);
      it.translate(-15.0F / 16.0F, -1.0F / 16.0F, 0.0F);
      emit_extruded(*mesh, it, tile, 1.0F);
    }
    return;
  }
  // Empty hand: swing bob + arm (drawFirstPersonHand, straight pose).
  Mat4 t;
  t.translate(-sswing_sqrt * 0.3F,
              std::sin(std::sqrt(std::max(swing, 0.0F)) * 3.14159265F * 2.0F) * 0.4F,
              -sswing * 0.4F);
  t.translate(0.8F * 0.8F, -(12.0F / 16.0F) * 0.8F - (1.0F - equip) * 0.6F, -0.9F * 0.8F);
  t.rotate(45.0F, 0.0F, 1.0F, 0.0F);
  t.rotate(sswing_sqrt * 70.0F, 0.0F, 1.0F, 0.0F);
  t.rotate(-sswing_sq * 20.0F, 0.0F, 0.0F, 1.0F);
  t.translate(-1.0F, 3.6F, 3.5F);
  t.rotate(120.0F, 0.0F, 0.0F, 1.0F);
  t.rotate(200.0F, 1.0F, 0.0F, 0.0F);
  t.rotate(-135.0F, 0.0F, 1.0F, 0.0F);
  t.translate(5.6F, 0.0F, 0.0F);
  // Arm box (char 40,16) at pivot (-5,2,0), straight (onGround=0 pose).
  ModelPart arm;
  arm.px = -5.0F;
  arm.py = 2.0F;
  arm.boxes.push_back(ModelBox{40, 16, -3, -2, -2, 4, 12, 4});
  const Mesh arm_mesh = build_model({arm}, 64, 32, 1.0F);
  Mat4 arm_t = t;
  arm_t.scale(1.0F / 16.0F, 1.0F / 16.0F, 1.0F / 16.0F);
  for (const Vertex& v : arm_mesh.vertices) {
    const float* mm = arm_t.m;
    out.skin.vertices.push_back(
        {mm[0] * v.x + mm[4] * v.y + mm[8] * v.z + mm[12],
         mm[1] * v.x + mm[5] * v.y + mm[9] * v.z + mm[13],
         mm[2] * v.x + mm[6] * v.y + mm[10] * v.z + mm[14], v.r, v.g, v.b, v.u, v.v});
  }
  for (std::uint32_t i : arm_mesh.indices) out.skin.indices.push_back(i);
}

}  // namespace craftpp::render
