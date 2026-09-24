# Render subsystem (`src_cpp/render/`)

OpenGL 3.3 core reimplementation of the 1.0 fixed-function look. Only this
directory (plus `app`) touches GL; everything else is pure CPU data.

## Meshing (CPU, testable headless)

- `mesh.hpp` — `Vertex{pos, color, uv}` + indexed `Mesh`. Plain data that
  crosses the future ChunkGen→Render thread boundary by move.
- `mesher.cpp` — full-cube mesher. Quad corner order, UV mapping
  (`u0=(tile&15)*16/256`, `u1=(+16−0.01)/256`, same for v) and per-face
  shading (bottom 0.5, top 1.0, z sides 0.8, x sides 0.6) mirror
  `RenderBlocks.render*Face` exactly; quads become triangles `(0,1,2)(0,2,3)`
  preserving GL_QUADS winding (CCW front). Faces are emitted only against
  non-occluding neighbors (`shouldSideBeRendered` for opaque cubes); water
  renders but never occludes. Grass top/sides get the biome tint.
- `frustum.hpp/cpp` — 6-plane frustum from a column-major view-projection
  matrix (Gribb/Hartmann extraction), `box_visible` per chunk.

## GPU upload & state (Render thread only)

- `tessellator.hpp/cpp` — spirit of `Tessellator.java`: accumulate colored
  UV vertices (or `upload()` a whole `Mesh`), one VBO/VAO pair, single
  indexed `GL_TRIANGLES` draw. Quads fed vertex-by-vertex auto-triangulate.
- `shader.hpp/cpp`, `shaders.hpp` — minimal program wrapper + embedded
  terrain shaders (GLSL 330): texture × vertex color, alpha-test discard,
  linear distance fog computed in **view space** (not clip space).
- `texture.hpp/cpp` — PNG atlas via `stb_image`, `GL_NEAREST`, no mipmaps
  (like 1.0's `RenderEngine`); `grass_tint_from_map` samples
  `grasscolor.png` exactly like `ColorizerGrass.getGrassColor(0.5, 1.0)`
  (index `127<<8|127`), with a fallback plains green.
- GL loader: **libepoxy** (`<epoxy/gl.h>`), matrix math via **glm**.

## App (`src_cpp/app/main.cpp`)

M2/M3 demo shell (not the final client): loads local `assets/terrain.png`
(+ `misc/grasscolor.png`), meshes chunk(s), orbit camera (FOV 70 like 1.0),
fog + sky color, frustum-culled draw, 20 TPS tick accumulator.

```sh
./build/craftpp --seed 1 --screenshot /tmp/shot.png  # generated chunk
./build/craftpp --flat                               # M2 flat chunk
```

## Gotchas already hit (see also `known-issues.md`)

- `glReadPixels` needs `GL_PACK_ALIGNMENT, 1` (854×3-byte rows are not
  multiple-of-4; default alignment 4 rotates channels row by row).
- GL-owned objects (`Texture`, `Tessellator`, `ShaderProgram`) must be
  destroyed **before** `glfwTerminate` (inner scope), else libepoxy aborts
  on a dead context.
- Fog distance must use the view matrix, not clip coordinates.
