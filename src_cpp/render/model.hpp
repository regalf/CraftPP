#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

#include "render/mesh.hpp"

namespace craftpp::render {

// CPU-side port of ModelBox/ModelRenderer for entity models (1.0 code
// models: pig, zombie, ...). Boxes are authored in model units (pixels);
// the caller scales by 1/16 and places the mesh at the entity.
//
// Quad emission mirrors ModelBox exactly: 6 TexturedQuads with the
// (u+d+w, v+d, ...) offset layout on a textureWidth x textureHeight skin,
// vertex order preserved. Part transform mirrors ModelRenderer.render:
// translate(pivot), then rotate Z, Y, X (GL post-multiply order).
struct ModelBox {
  int tex_u = 0, tex_v = 0;
  float x = 0, y = 0, z = 0;
  int w = 1, h = 1, d = 1;
  float expand = 0.0F;
  bool mirror = false;
};

struct ModelPart {
  float px = 0, py = 0, pz = 0;  // rotation point (model units)
  float rx = 0, ry = 0, rz = 0;  // angles, radians
  std::vector<ModelBox> boxes;
};

// Builds the transformed mesh in MODEL units (caller scales/translates).
// Brightness multiplies the vertex color (entity lighting is flat here).
Mesh build_model(const std::vector<ModelPart>& parts, int tex_w, int tex_h, float brightness);

// Final entity mesh in WORLD units: scale 1/16 with the RenderLiving
// placement (mirror X, ground at model y=24.125 like the
// glScalef(-1,-1,1) + glTranslatef(0, -24/16 - 0.0078125, 0) pair),
// yaw-rotated by yaw_deg about Y. Culling should be off (the mirror flips
// winding, like the source which relies on display-list state).
// model_scale scales model units first (players render at 15/16 via
// renderPlayerScale); roll_deg applies a Z roll about the feet afterwards
// (death fall-over via rotateCorpse).
Mesh entity_mesh(const std::vector<ModelPart>& parts, int tex_w, int tex_h, float yaw_deg,
                 float brightness, float model_scale = 1.0F, float roll_deg = 0.0F);

// Applies the entity_mesh placement (scale, mirror, ground, roll, yaw,
// translate) to an existing model-unit mesh in place.
void place_mesh(Mesh& m, float model_scale, float roll_deg, float yaw_deg, float ox, float oy,
                float oz);

// Held item at the right hand (RenderPlayer renderSpecials, no use-poses,
// no glint, no fish-stick swap: M5 leftovers). Model-unit mesh; the caller
// runs place_mesh with the same params as the body. Meshes split by
// texture (atlas/items); empty when item_id <= 0.
struct EquippedMeshes {
  Mesh atlas;
  Mesh items;
};
void build_equipped(EquippedMeshes& out, const ModelPart& arm, int item_id, int damage);

// Pig (ModelPig/ModelQuadruped, leg height 6): walk phase + head look.
std::vector<ModelPart> pig_parts(float limb_swing, float swing_amount, float head_yaw_deg,
                                 float head_pitch_deg);
// Zombie (ModelZombie/ModelBiped): walk + attack swing + idle sway.
std::vector<ModelPart> zombie_parts(float limb_swing, float swing_amount, float attack_t,
                                    int age_ticks, float head_yaw_deg, float head_pitch_deg);
// Player (ModelBiped on char.png): walk, head look, sneak pose, held-item
// arm pose, attack swing (onGround 0..1), idle sway. field_40333_u (bow
// aim) needs the item-use state machine (M5 leftover): not modeled.
std::vector<ModelPart> player_parts(float limb_swing, float swing_amount, float attack_t,
                                    float head_yaw_deg, float head_pitch_deg, int age_ticks,
                                    bool sneaking, int held_pose);

}  // namespace craftpp::render
