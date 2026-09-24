# Craft++ Implementation Plan

Goal: a clean-room C++20 reimplementation of Minecraft 1.0 gameplay that is
indistinguishable from the original Java client. Singleplayer first; final
validation is joining a vanilla 1.0 Java server.

Reference (local only, never committed): `src/` (~930 `.java`, ~91k LOC),
`assets/` (jar textures + `resources/` audio). See `LEGAL_NOTICE.md`.

## Architecture

Three threads (C++20 `std::jthread` + lock-free SPSC/MPMC queues):

| Thread | Owns | Does |
|---|---|---|
| Main/Tick | `World` state (sole writer), 20 TPS fixed timestep | entities, physics, player, containers, save scheduling |
| Render | OpenGL 3.3+ context (GLFW + glad), sole GL caller | mesh upload, draw, GUI |
| ChunkGen/IO | nothing shared (works on copies/snapshots) | `GenLayer`/biome/worldgen, CPU meshing, McRegion load/save |

Rules: deterministic per-chunk seeding (`worldSeed ^ hash(x,z)`, private
`JavaRandom` reimplementation — never share one RNG across threads); meshing
reads a `16x16 + 1 border` snapshot, never the live chunk; chunk lifecycle
`Empty -> Pending -> Generating -> Meshing -> Ready -> Upload -> Active` moves
ownership via `std::move`; Main keeps Java-identical tick order so a future
server session does not desync.

## Milestones

### M0 — Foundations (now)
CMake + C++20 + Ninja, `src_cpp/app` window (`GLFW`, 854x480, game loop with
fixed-timestep accumulator), `src_cpp/core` logging/math stubs, `tests/` with
Catch2, CI-less local build on Arch. Exit: window opens, `ctest` green.

### M1 — Deterministic core
`util/JavaRandom` (bit-identical LCG to `java.util.Random`), `MathHelper`,
`Vec3D`, `AABB`, full NBT read/write (big-endian + zlib). Exit: unit tests —
100k `nextInt()` match Java vectors, NBT round-trip byte-identical on a real
1.0 `level.dat`.

### M2 — Chunk rendering
`Tessellator` API-compatible batcher (`addVertex/setColor/setUV`) flushing to
VBO/VAO; shaders emulating 1.0 fixed-function (nearest textures, fog,
alpha-test, lightmap, `ColorizerGrass/Foliage/Water`); `WorldRenderer` per-chunk
meshes + frustum culling. Exit: flat 16x16 world renders matching Java
screenshots.

### M3 — World + worldgen
`Block` registry, `Chunk`, `World`, `ChunkProviderLoadOrGenerate`,
`GenLayer*`, `Biome*`, `WorldGen*` (trees, ores, caves, structures). Exit: same
seed as Java 1.0 yields same heightmap/trees/ores (automated diff on sampled
chunks).

Status: **in progress, split for sanity**.
- M3a GenLayer stack + biomes — **done** (6720 ints vs OpenJDK).
- M3b noise + terrain + surface — **done** (heightmaps + full chunk bytes).
- M3c caves/ravines — **nearly done, 1 open bug** (see below).
- M3c decorator (ores/trees/plants/lakes/dungeons) — implemented, validation
  blocked on the open bug + untriaged. Details in `docs/world.md`.

## Open bugs (M3c) — detail in `docs/known-issues.md`

1. **Cave grass-top fixup divergence** (blocker): carved base differs from
   Java in 4/49 tested chunks (seed 1: `(-2,0)`, `(-2,1)`, `(-1,0)`,
   `(1,0)`), sparse single-cell diffs at carve fringes, e.g. `(8,60,1)`
   grass-vs-dirt. Spawn sets, node trajectories, width profiles and
   isolated node carves are all proven identical; the trigger is an
   unproven ~1-ulp path difference or fixup-order interaction in one node.
   Next: attribute the first diverging write to its node via the existing
   chronological W-logs, then bit-level trajectory diff of that node.
2. **Populate/decorator validation**: `test_populate` (5 sites × 3×3) fails
   ~875 assertions (tree-height stripes, meta hashes). Partly poisoned by
   bug 1 (bad base chunks), rest untriaged. Accepted gaps (M4/M5): fluid
   spread after spring placement, spawner entities, structures, tile-entity
   contents.

### M4 — Player physics + interaction
`Entity`, `EntityLiving`, `EntityPlayerSP`, `PlayerController` (survival +
creative), AABB collision, block break/place, day/night tick. Exit: walk/jump/
fall/mine/place feel identical at 20 TPS.

### M5 — Full singleplayer survival
`TileEntity` (chest, furnace, signs), `Container/Slot` + crafting/furnace
recipes, mobs + spawning, weather, `McRegion` save/load, `GuiMainMenu/Ingame/
Inventory` + `FontRenderer`, options. Exit: create world, play 10 min, save,
reopen the same save in Java 1.0 without corruption.

### M6 — Audio + polish
`SoundManager` on OpenAL-soft (`stb_vorbis`, `dr_wav`), `CodecMus`/`MusInputStream`
XOR decode for `streaming/*.mus` jukebox discs, positional audio, particles,
menus/credits/splashes. Exit: all 1.0 sounds/music/discs play correctly.

### M7 — Networking (final test)
67 `Packet*` byte-identical codecs, `NetClientHandler`, handshake/login,
`EntityClientPlayerMP`, chunk/entity streaming. Exit: **Craft++ joins a vanilla
1.0 Java server; chat, movement and block edits visible from a Java client.**

## Dependency map (Arch pacman / bundled)

`glfw` + `glad` (render), `glm` (math), `openal` (audio), `zlib` (NBT/region),
`stb` (image/vorbis, bundled), `Catch2` (tests), `cmake` + `ninja` + `gcc`.

## Layout

```text
src_cpp/core/    MathHelper, Vec3D, AABB, JavaRandom, NBT, logging
src_cpp/world/   Block, Item, Chunk, World, providers, GenLayer, Biome, WorldGen
src_cpp/render/  Tessellator, shaders, WorldRenderer, EntityRenderer, Font
src_cpp/entity/  Entity, EntityLiving, Player, TileEntity, mobs
src_cpp/gui/     screens, containers, slots
src_cpp/audio/   SoundManager, codecs (ogg/wav/mus)
src_cpp/net/     packets, client handler (M7)
src_cpp/app/     MinecraftApp, GameSettings, save format, main loop
tests/           Catch2 parity tests per milestone
docs/            design notes
```

## Porting rules (learned the hard way)

1. **One RNG draw per statement.** Java evaluates strictly left-to-right;
   C++ leaves argument/subexpression order unspecified. Hoist every draw into
   a named temporary in source order (this caused real seed/size swaps in
   cave bifurcations).
2. **Wrapping arithmetic**: 64-bit seeds via `uint64_t`, 32-bit cell coords
   via unsigned math + sign-preserving casts.
3. **Read the decompiled source literally**: integer division truncation,
   transposed indices, off-by-one frame indexing, and seemingly dead code
   (e.g. ravine height from the *unjittered* width) are all load-bearing.

## Non-goals

Redistributing Mojang code or assets; supporting newer versions; mod APIs
before M5 is green.
