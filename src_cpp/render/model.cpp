#include "render/model.hpp"

#include "gui/item_icons.hpp"
#include "render/item_mesh.hpp"
#include "world/block_place.hpp"
#include "world/blocks.hpp"

namespace craftpp::render {

namespace {

struct Pt {
  float x, y, z;
};

Pt rotate_part(Pt v, float rx, float ry, float rz) {
  // Mirrors GL post-multiply order: v' = Rz * Ry * Rx * v.
  if (rx != 0.0F) {
    const float c = std::cos(rx), s = std::sin(rx);
    v = Pt{v.x, c * v.y - s * v.z, s * v.y + c * v.z};
  }
  if (ry != 0.0F) {
    const float c = std::cos(ry), s = std::sin(ry);
    v = Pt{c * v.x + s * v.z, v.y, -s * v.x + c * v.z};
  }
  if (rz != 0.0F) {
    const float c = std::cos(rz), s = std::sin(rz);
    v = Pt{c * v.x - s * v.y, s * v.x + c * v.y, v.z};
  }
  return v;
}

void emit_box(Mesh& mesh, const ModelBox& b, const ModelPart& p, float tex_w, float tex_h,
              float brightness) {
  float x0 = b.x - b.expand, y0 = b.y - b.expand, z0 = b.z - b.expand;
  float x1 = b.x + b.w + b.expand, y1 = b.y + b.h + b.expand, z1 = b.z + b.d + b.expand;
  if (b.mirror) std::swap(x0, x1);
  const Pt c[8] = {{x0, y0, z0}, {x1, y0, z0}, {x1, y1, z0}, {x0, y1, z0},
                   {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}};
  const float u = static_cast<float>(b.tex_u), v = static_cast<float>(b.tex_v);
  const float w = static_cast<float>(b.w), h = static_cast<float>(b.h), d = static_cast<float>(b.d);
  // (corner indices, uv rect) per ModelBox quad table.
  const int qi[6][4] = {{5, 1, 2, 6}, {0, 4, 7, 3}, {5, 4, 0, 1},
                        {2, 3, 7, 6}, {1, 0, 3, 2}, {4, 5, 6, 7}};
  const float qr[6][4] = {{u + d + w, v + d, u + d + w + d, v + d + h},
                          {u, v + d, u + d, v + d + h},
                          {u + d, v, u + d + w, v + d},
                          {u + d + w, v + d, u + d + w + w, v},
                          {u + d, v + d, u + d + w, v + d + h},
                          {u + d + w + d, v + d, u + d + w + d + w, v + d + h}};
  for (int q = 0; q < 6; ++q) {
    // TexturedQuad UVs: v0=(u2,v1), v1=(u1,v1), v2=(u1,v2), v3=(u2,v2).
    const float u1 = qr[q][0] / tex_w, v1 = qr[q][1] / tex_h;
    const float u2 = qr[q][2] / tex_w, v2 = qr[q][3] / tex_h;
    const float qu[4] = {u2, u1, u1, u2};
    const float qv[4] = {v1, v1, v2, v2};
    const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
    for (int k = 0; k < 4; ++k) {
      Pt t = rotate_part(c[qi[q][k]], p.rx, p.ry, p.rz);
      mesh.vertices.push_back(
          Vertex{t.x + p.px, t.y + p.py, t.z + p.pz, brightness, brightness, brightness, qu[k], qv[k]});
    }
    if (!b.mirror) {
      mesh.indices.insert(mesh.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    } else {
      mesh.indices.insert(mesh.indices.end(), {base, base + 2, base + 1, base, base + 3, base + 2});
    }
  }
}

}  // namespace

Mesh build_model(const std::vector<ModelPart>& parts, int tex_w, int tex_h, float brightness) {
  Mesh mesh;
  mesh.vertices.reserve(parts.size() * 24);
  mesh.indices.reserve(parts.size() * 36);
  const float tw = static_cast<float>(tex_w), th = static_cast<float>(tex_h);
  for (const ModelPart& p : parts) {
    for (const ModelBox& b : p.boxes) emit_box(mesh, b, p, tw, th, brightness);
  }
  return mesh;
}

Mesh entity_mesh(const std::vector<ModelPart>& parts, int tex_w, int tex_h, float yaw_deg,
                 float brightness, float model_scale, float roll_deg) {
  Mesh m = build_model(parts, tex_w, tex_h, brightness);
  place_mesh(m, model_scale, roll_deg, yaw_deg, 0.0F, 0.0F, 0.0F);
  return m;
}

void build_equipped(EquippedMeshes& out, const ModelPart& arm, int item_id, int damage) {
  if (item_id <= 0) return;
  using namespace item_mesh;
  // Arm postRender in model units: translate pivot, rotate Z/Y/X.
  Mat4 m;
  m.translate(arm.px, arm.py, arm.pz);
  m.rotate(arm.rz * 180.0F / 3.14159265F, 0.0F, 0.0F, 1.0F);
  m.rotate(arm.ry * 180.0F / 3.14159265F, 0.0F, 1.0F, 0.0F);
  m.rotate(arm.rx * 180.0F / 3.14159265F, 1.0F, 0.0F, 0.0F);
  // postRender scale is 1/16 (model -> block units for the chains below).
  constexpr float kU = 1.0F / 16.0F;
  const bool is_block = item_id < 256;
  const int rt = is_block ? world::bid::render_type(item_id) : -1;
  const bool cube3d =
      is_block && (rt == 0 || rt == 10 || rt == 11 || rt == 13 || rt == 16 || rt == 21 ||
                   rt == 22 || rt == 27);
  // Geometry helpers work in 0..1 block units; scale up to model units.
  Mat4 to_model;
  to_model.scale(16.0F, 16.0F, 16.0F);
  if (cube3d) {
    Mat4 t = m;
    t.translate(-1.0F, 7.0F, 1.0F);
    t.translate(0.0F, 3.0F * kU, -5.0F * kU);
    t.scale(0.5F * 12.0F * kU, -0.5F * 12.0F * kU, 0.5F * 12.0F * kU);
    t.rotate(20.0F, 1.0F, 0.0F, 0.0F);
    t.rotate(45.0F, 0.0F, 1.0F, 0.0F);
    // renderBlockOnInventory unit cube (0..1) -> model units.
    Mat4 u = t;
    u.scale(16.0F, 16.0F, 16.0F);
    Mesh tmp;
    emit_unit_cube(tmp, Mat4(), item_id, damage, 1.0F);
    for (auto& v : tmp.vertices) {
      const float* mm = u.m;
      const float x = mm[0] * v.x + mm[4] * v.y + mm[8] * v.z + mm[12];
      const float y = mm[1] * v.x + mm[5] * v.y + mm[9] * v.z + mm[13];
      const float z = mm[2] * v.x + mm[6] * v.y + mm[10] * v.z + mm[14];
      out.atlas.vertices.push_back({x, y, z, v.r, v.g, v.b, v.u, v.v});
    }
    const std::uint32_t base =
        static_cast<std::uint32_t>(out.atlas.vertices.size()) - tmp.vertices.size();
    for (std::uint32_t i : tmp.indices) out.atlas.indices.push_back(base + i);
    return;
  }
  // Icon tile (blocks: side-2 terrain tile like renderItem).
  int tile = 0;
  Mesh* mesh = &out.items;
  if (is_block) {
    tile = world::bid::block_texture(item_id, 2, damage);
    mesh = &out.atlas;
  } else {
    tile = gui::item_sprite_index(item_id);
    if (tile < 0) return;
  }
  Mat4 t = m;
  if (item_id == 261) {
    // Bow branch (no use-draw pose: M5).
    t.translate(0.0F, 2.0F * kU, 5.0F * kU);
    t.rotate(-20.0F, 0.0F, 1.0F, 0.0F);
    t.scale(10.0F * kU, -10.0F * kU, 10.0F * kU);
    t.rotate(-100.0F, 1.0F, 0.0F, 0.0F);
    t.rotate(45.0F, 0.0F, 1.0F, 0.0F);
  } else if (world::edit::item_full_3d(item_id)) {
    if (item_id == 346) {
      t.rotate(180.0F, 0.0F, 0.0F, 1.0F);
      t.translate(0.0F, -2.0F * kU, 0.0F);
    }
    t.translate(0.0F, 3.0F * kU, 0.0F);
    t.scale(10.0F * kU, -10.0F * kU, 10.0F * kU);
    t.rotate(-100.0F, 1.0F, 0.0F, 0.0F);
    t.rotate(45.0F, 0.0F, 1.0F, 0.0F);
  } else {
    t.translate(0.25F, 3.0F * kU, -3.0F * kU);
    t.scale(6.0F * kU, 6.0F * kU, 6.0F * kU);
    t.rotate(60.0F, 0.0F, 0.0F, 1.0F);
    t.rotate(-90.0F, 1.0F, 0.0F, 0.0F);
    t.rotate(20.0F, 0.0F, 0.0F, 1.0F);
  }
  // renderItem extrusion works in 0..1 units: scale output to model units.
  Mat4 u = t;
  u.scale(16.0F, 16.0F, 16.0F);
  Mesh tmp;
  emit_extruded(tmp, Mat4(), tile, 1.0F);
  for (auto& v : tmp.vertices) {
    const float* mm = u.m;
    const float x = mm[0] * v.x + mm[4] * v.y + mm[8] * v.z + mm[12];
    const float y = mm[1] * v.x + mm[5] * v.y + mm[9] * v.z + mm[13];
    const float z = mm[2] * v.x + mm[6] * v.y + mm[10] * v.z + mm[14];
    mesh->vertices.push_back({x, y, z, v.r, v.g, v.b, v.u, v.v});
  }
  const std::uint32_t base = static_cast<std::uint32_t>(mesh->vertices.size()) - tmp.vertices.size();
  for (std::uint32_t i : tmp.indices) mesh->indices.push_back(base + i);
}

void place_mesh(Mesh& m, float model_scale, float roll_deg, float yaw_deg, float ox, float oy,
                float oz) {
  constexpr float kScale = 1.0F / 16.0F;
  const float yr = yaw_deg * 3.14159265F / 180.0F;
  const float c = std::cos(yr), s = std::sin(yr);
  const float rr = roll_deg * 3.14159265F / 180.0F;
  const float rc = std::cos(rr), rs = std::sin(rr);
  for (Vertex& v : m.vertices) {
    // Model-unit scale, mirror X + ground at 24.125 (RenderLiving pair),
    // death roll about the feet, then yaw about Y, then translate.
    const float lx = -v.x * model_scale * kScale;
    const float ly = (24.125F - v.y * model_scale) * kScale;
    const float lz = v.z * model_scale * kScale;
    const float qx = lx * rc - ly * rs;
    const float qy = lx * rs + ly * rc;
    v.x = qx * c + lz * s + ox;
    v.y = qy + oy;
    v.z = -qx * s + lz * c + oz;
  }
}

namespace {

constexpr float kDeg = 3.14159265F / 180.0F;

ModelPart part(float px, float py, float pz, float rx, float ry, float rz, int tu, int tv, float x,
              float y, float z, int w, int h, int d) {
  ModelPart p;
  p.px = px;
  p.py = py;
  p.pz = pz;
  p.rx = rx;
  p.ry = ry;
  p.rz = rz;
  p.boxes.push_back(ModelBox{tu, tv, x, y, z, w, h, d});
  return p;
}

}  // namespace

std::vector<ModelPart> pig_parts(float limb_swing, float swing_amount, float head_yaw_deg,
                                 float head_pitch_deg) {
  constexpr int leg_h = 6;
  const float head_rx = head_pitch_deg * kDeg;
  const float head_ry = head_yaw_deg * kDeg;
  const float w1 = std::cos(limb_swing * 0.6662F) * 1.4F * swing_amount;
  const float w2 = std::cos(limb_swing * 0.6662F + 3.14159265F) * 1.4F * swing_amount;
  std::vector<ModelPart> parts;
  parts.push_back(part(0, 18 - leg_h, -6, head_rx, head_ry, 0, 0, 0, -4, -4, -8, 8, 8, 8));
  parts.back().boxes.push_back(ModelBox{16, 16, -2, 0, -9, 4, 3, 1});  // snout
  parts.push_back(part(0, 17 - leg_h, 2, 3.14159265F * 0.5F, 0, 0, 28, 8, -5, -10, -7, 10, 16, 8));
  parts.push_back(part(-3, 24 - leg_h, 7, w1, 0, 0, 0, 16, -2, 0, -2, 4, leg_h, 4));
  parts.push_back(part(3, 24 - leg_h, 7, w2, 0, 0, 0, 16, -2, 0, -2, 4, leg_h, 4));
  parts.push_back(part(-3, 24 - leg_h, -5, w2, 0, 0, 0, 16, -2, 0, -2, 4, leg_h, 4));
  parts.push_back(part(3, 24 - leg_h, -5, w1, 0, 0, 0, 16, -2, 0, -2, 4, leg_h, 4));
  return parts;
}

std::vector<ModelPart> player_parts(float limb_swing, float swing_amount, float attack_t,
                                    float head_yaw_deg, float head_pitch_deg, int age_ticks,
                                    bool sneaking, int held_pose) {
  constexpr float kPi = 3.14159265F;
  const float t = static_cast<float>(age_ticks);
  const float head_ry = head_yaw_deg * kDeg;
  const float head_rx = head_pitch_deg * kDeg;
  float arm_r_rx = std::cos(limb_swing * 0.6662F + kPi) * 2.0F * swing_amount * 0.5F;
  float arm_l_rx = std::cos(limb_swing * 0.6662F) * 2.0F * swing_amount * 0.5F;
  float arm_r_ry = 0.0F, arm_l_ry = 0.0F;
  float arm_r_px = -5.0F, arm_l_px = 5.0F, arm_r_pz = 0.0F, arm_l_pz = 0.0F;
  float leg_r_rx = std::cos(limb_swing * 0.6662F) * 1.4F * swing_amount;
  float leg_l_rx = std::cos(limb_swing * 0.6662F + kPi) * 1.4F * swing_amount;
  if (held_pose != 0) {
    // field_1278_i (held item) bends the RIGHT arm only.
    arm_r_rx = arm_r_rx * 0.5F - kPi * 0.1F * held_pose;
  }
  if (attack_t > 0.0F) {
    // Attack body-twist (onGround swing block; identity at 0).
    const float body_ry = std::sin(std::sqrt(attack_t) * kPi * 2.0F) * 0.2F;
    arm_r_pz = std::sin(body_ry) * 5.0F;
    arm_r_px = -std::cos(body_ry) * 5.0F;
    arm_l_pz = -std::sin(body_ry) * 5.0F;
    arm_l_px = std::cos(body_ry) * 5.0F;
    arm_r_ry += body_ry;
    arm_l_ry += body_ry;
    arm_l_rx += body_ry;
    float w = 1.0F - attack_t;
    w *= w;
    w *= w;
    w = 1.0F - w;
    const float s = std::sin(w * kPi);
    const float w2 = std::sin(attack_t * kPi) * -(head_rx - 0.7F) * 0.75F;
    arm_r_rx -= (s * 1.2F + w2);
    arm_r_ry += body_ry * 2.0F;
    float arm_r_rz = std::sin(attack_t * kPi) * -0.4F;
    const float sway = std::cos(t * 0.09F) * 0.05F + 0.05F;
    const float bob = std::sin(t * 0.067F) * 0.05F;
    std::vector<ModelPart> parts;
    parts.push_back(part(0, sneaking ? 1.0F : 0.0F, 0, head_rx, head_ry, 0, 0, 0, -4, -8, -4, 8,
                         8, 8));
    parts.push_back(
        part(0, sneaking ? 1.0F : 0.0F, 0, head_rx, head_ry, 0, 32, 0, -4, -8, -4, 8, 8, 8));
    parts.back().boxes[0].expand = 0.5F;
    parts.push_back(
        part(0, 0, 0, sneaking ? 0.5F : 0.0F, body_ry, 0, 16, 16, -4, 0, -2, 8, 12, 4));
    parts.push_back(part(arm_r_px, 2, arm_r_pz, arm_r_rx + bob, arm_r_ry, arm_r_rz + sway, 40, 16,
                         -3, -2, -2, 4, 12, 4));
    parts.push_back(part(arm_l_px, 2, arm_l_pz, arm_l_rx - bob, arm_l_ry, -sway, 40, 16, -1, -2,
                         -2, 4, 12, 4));
    parts.back().boxes[0].mirror = true;
    if (sneaking) {
      parts.push_back(part(-2, 9, 4, leg_r_rx, 0, 0, 0, 16, -2, 0, -2, 4, 12, 4));
      parts.push_back(part(2, 9, 4, leg_l_rx, 0, 0, 0, 16, -2, 0, -2, 4, 12, 4));
    } else {
      parts.push_back(part(-2, 12, 0, leg_r_rx, 0, 0, 0, 16, -2, 0, -2, 4, 12, 4));
      parts.push_back(part(2, 12, 0, leg_l_rx, 0, 0, 0, 16, -2, 0, -2, 4, 12, 4));
    }
    parts.back().boxes[0].mirror = true;
    if (sneaking) {
      parts[2].rx += 0.4F;
      parts[3].rx += 0.4F;
    }
    return parts;
  }
  const float sway = std::cos(t * 0.09F) * 0.05F + 0.05F;
  const float bob = std::sin(t * 0.067F) * 0.05F;
  std::vector<ModelPart> parts;
  parts.push_back(part(0, sneaking ? 1.0F : 0.0F, 0, head_rx, head_ry, 0, 0, 0, -4, -8, -4, 8,
                       8, 8));
  parts.push_back(part(0, sneaking ? 1.0F : 0.0F, 0, head_rx, head_ry, 0, 32, 0, -4, -8, -4, 8, 8,
                       8));
  parts.back().boxes[0].expand = 0.5F;
  parts.push_back(part(0, 0, 0, sneaking ? 0.5F : 0.0F, 0, 0, 16, 16, -4, 0, -2, 8, 12, 4));
  parts.push_back(
      part(-5, 2, 0, arm_r_rx + bob, 0, sway, 40, 16, -3, -2, -2, 4, 12, 4));
  parts.push_back(part(5, 2, 0, arm_l_rx - bob, 0, -sway, 40, 16, -1, -2, -2, 4, 12, 4));
  parts.back().boxes[0].mirror = true;
  if (sneaking) {
    parts.push_back(part(-2, 9, 4, leg_r_rx, 0, 0, 0, 16, -2, 0, -2, 4, 12, 4));
    parts.push_back(part(2, 9, 4, leg_l_rx, 0, 0, 0, 16, -2, 0, -2, 4, 12, 4));
  } else {
    parts.push_back(part(-2, 12, 0, leg_r_rx, 0, 0, 0, 16, -2, 0, -2, 4, 12, 4));
    parts.push_back(part(2, 12, 0, leg_l_rx, 0, 0, 0, 16, -2, 0, -2, 4, 12, 4));
  }
  parts.back().boxes[0].mirror = true;
  if (sneaking) {
    parts[3].rx += 0.4F;
    parts[4].rx += 0.4F;
  }
  return parts;
}

std::vector<ModelPart> zombie_parts(float limb_swing, float swing_amount, float attack_t,
                                    int age_ticks, float head_yaw_deg, float head_pitch_deg) {
  const float t = static_cast<float>(age_ticks);
  const float head_ry = head_yaw_deg * kDeg;
  const float head_rx = head_pitch_deg * kDeg;
  const float leg_r = std::cos(limb_swing * 0.6662F) * 1.4F * swing_amount;
  const float leg_l = std::cos(limb_swing * 0.6662F + 3.14159265F) * 1.4F * swing_amount;
  // Zombie attack pose (ModelZombie over the biped walk).
  const float s1 = std::sin(attack_t * 3.14159265F);
  const float s2 = std::sin((1.0F - (1.0F - attack_t) * (1.0F - attack_t)) * 3.14159265F);
  const float arm_rx = -3.14159265F * 0.5F - (s1 * 1.2F - s2 * 0.4F);
  const float arm_ry_r = -(0.1F - s1 * 0.6F);
  const float arm_ry_l = 0.1F - s1 * 0.6F;
  const float sway = std::cos(t * 0.09F) * 0.05F + 0.05F;
  const float bob = std::sin(t * 0.067F) * 0.05F;
  std::vector<ModelPart> parts;
  parts.push_back(part(0, 0, 0, head_rx, head_ry, 0, 0, 0, -4, -8, -4, 8, 8, 8));
  parts.push_back(part(0, 0, 0, 0, 0, 0, 16, 16, -4, 0, -2, 8, 12, 4));
  // ModelZombie OVERWRITES the biped walk swing with the raised-arm pose.
  parts.push_back(part(-5, 2, 0, arm_rx + bob, arm_ry_r, sway, 40, 16, -3, -2, -2, 4, 12, 4));
  parts.push_back(part(5, 2, 0, arm_rx - bob, arm_ry_l, -sway, 40, 16, -1, -2, -2, 4, 12, 4));
  parts.push_back(part(-2, 12, 0, leg_r, 0, 0, 0, 16, -2, 0, -2, 4, 12, 4));
  parts.push_back(part(2, 12, 0, leg_l, 0, 0, 0, 16, -2, 0, -2, 4, 12, 4));
  return parts;
}

}  // namespace craftpp::render
