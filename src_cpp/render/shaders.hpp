#pragma once

namespace craftpp::render {

// Terrain shaders: emulate the 1.0 fixed-function pipeline that RenderBlocks
// relied on — texture * vertex color, alpha-test discard, linear distance fog.
// GLSL 330 core, attributes match Tessellator layout (pos/color/uv).
inline constexpr const char* kTerrainVert = R"GLSL(
#version 330 core
layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_color;
layout(location = 2) in vec2 in_uv;
uniform mat4 u_mvp;
uniform mat4 u_view;
uniform float u_fog_start;
uniform float u_fog_end;
out vec3 v_color;
out vec2 v_uv;
out float v_fog;
void main() {
  gl_Position = u_mvp * vec4(in_pos, 1.0);
  v_color = in_color;
  v_uv = in_uv;
  float dist = length((u_view * vec4(in_pos, 1.0)).xyz);
  v_fog = clamp((dist - u_fog_start) / (u_fog_end - u_fog_start), 0.0, 1.0);
}
)GLSL";

inline constexpr const char* kTerrainFrag = R"GLSL(
#version 330 core
in vec3 v_color;
in vec2 v_uv;
in float v_fog;
uniform sampler2D u_tex;
uniform vec3 u_fog_color;
out vec4 out_color;
void main() {
  vec4 tex = texture(u_tex, v_uv);
  if (tex.a < 0.5) discard;  // alpha test like cutout foliage (M3+)
  vec3 col = tex.rgb * v_color;
  out_color = vec4(mix(col, u_fog_color, v_fog), tex.a);
}
)GLSL";

}  // namespace craftpp::render
