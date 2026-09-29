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

Status: **done** (split for sanity, all verified differential vs OpenJDK).
- M3a GenLayer stack + biomes — **done** (6720 ints vs OpenJDK).
- M3b noise + terrain + surface — **done** (heightmaps + full chunk bytes).
- M3c caves/ravines — **done** (7x7 carved base byte-identical; fixed the
  sticky `var49` grass-top fixup bug, see docs/known-issues.md F6).
- M3c decorator + populate (ores/trees/plants/lakes/dungeons/springs) —
  **done**: 5 sites x 3x3 populated chunks bit-identical except ONE documented
  cell (M5 light-engine gap, see below). Fixed along the way: taiga1 heights,
  lava-spring draw depth, leaves removal marking, setBlock parity, opaque
  leaves, fluid spread sim, still-water dispatch, stationary scheduling,
  shared flow-dir members, deadbush soil, WithNotify placements
  (vines/dungeons/ice-snow), BigTree heightLimit persistence, lava opacity.
  Details in `docs/world.md`, `docs/testing.md`, `docs/known-issues.md`.

## Open bugs (M3c) — detail in `docs/known-issues.md`

1. **Light-engine gaps fixed**: the M5 engine closed all of them — the
   original (-32,19) cell plus 4 marginal threshold cells that briefly
   diverged due to a real engine bug (increase-spread nested in the decrease
   branch; one-brace fix, see OPEN ISSUE 4). Populate suite fully green with
   zero gaps.

### M4 — Player physics + interaction
`Entity`, `EntityLiving`, `EntityPlayerSP`, `PlayerController` (survival +
creative), AABB collision, block break/place, day/night tick. Exit: walk/jump/
fall/mine/place feel identical at 20 TPS.

Status: **done** (verified differential vs OpenJDK, suite fully green —
the old M5 light cell is closed, see M5 status below).
- Entity base: full `moveEntity` (sneak edge, axis clamps, step-up,
  fall-state, walk distance, web/soul/cactus hooks, burning box),
  `onEntityUpdate` with all RNG draws, water push, lava, `moveFlying`.
- Block collision shapes for all 122 ids incl. the shared-mutable-bounds
  state machine (sticky per-type bounds, pane leftovers, piston-extension
  trailing reset) + fluid flow vectors.
- EntityLiving: `onUpdate` yaw interpolation, living `onEntityUpdate`
  (sound draw, suffocation, drown air/bubbles, death branch), `moveEntity-
  WithHeading` (water/lava/friction/slipperiness/ladder/auto-step),
  jump (+sprint boost), fall damage, attack/health/armor (double armor
  application), knockback, `updateEntityActionState`, movement-stat
  exhaustion, `EntityPlayer.onLivingUpdate` (speed factors, camera).
- PlayerSP: input glue, sprint state machine, push-out, portal timers,
  fly toggle/motion/dismount, `setHealth` routing, eye 0.12 / yOffset 1.62.
- Items/inventory: tool matrix (strength/harvest/durability/damage),
  armor values, hardness table, drop tables, ItemBlock/ItemDoor placement,
  orientation hooks, neighbor-pop cascade, door toggles.
- Controllers: SP mining (damage accumulation, wait, retarget, drops,
  durability) + placement, creative insta-break/countdown/no-consume.
- Oracle-assisted (no client in headless JVM): SP `onLivingUpdate`,
  controller glue — both transcribed line-by-line + behavior-tested.
- Day/night tick deferred to M5 (needs the live World + light engine).
- Throwaway demo: `craftpp_demo` (DEPRECATED, unmaintained — use `craftpp`;
  prints a deprecation notice at startup) on 3×3 generated chunks.

### M5 — Full singleplayer survival
`TileEntity` (chest, furnace, signs), `Container/Slot` + crafting/furnace
recipes, mobs + spawning, weather, `McRegion` save/load, `GuiMainMenu/Ingame/
Inventory` + `FontRenderer`, options. Exit: create world, play 10 min, save,
reopen the same save in Java 1.0 without corruption.

Status: **light engine done** (`RegionWorld`: stored heightMap + sky/block
nibbles, `relightBlock` with the vanilla local-coords quirk,
`updateLightByType` BFS, `updateAllLightTypes` on every write, stored-height
`canBlockSeeTheSky`, lazy precipitation heights, lava `lightValue` for the
ice/snow cap). **Live world skeleton done** (`LiveWorld`: real
terrain+carve+populate+light chunks behind `EditWorld`, world time,
entity registry at 20 TPS; demo migrated to it). **Day/night + random
ticks done** (`world/tick.*`: celestial angle, skylight subtracted,
grass spread/kill, leaves decay, ice melt, flower/mushroom pops, fire
spread with a scheduled queue). Suite fully green, zero gaps. **TileEntity
done** (chest/furnace/sign + smelting + drops). **Drops/pickup done.**
**Mobs done** (pig+zombie+spawn; wall-follow pending real paths).
**Crafting done** (full table). **Creative done.** Demo is a playable
survival slice (daylight, mob boxes, hotbar, held placing, G creative,
fly double-tap, TPS meter, auto-respawn).

## Remaining M5 (dependency order — updated 2026-09-29)

1. **Playable inventory GUI — NEXT**: crafting grid 3x3 (table + player),
   furnace/chest mouse interaction (drag/drop, shift-click, progress
   arrows/flames). Engine ready (recipes, tile entities, Container
   drafts); only the interaction layer is missing. This is the M5
   blocker: everything playable hangs off it.
2. **Options/keybindings — partial**: difficulty functional (spawn
   flags), sliders (music/sound/sensitivity/fov/invert) in memory.
   LEFT: GameSettings backend + remappable keys. (Mouse sensitivity
   follows the vanilla cubic curve only at default 0.5; exact curve
   pending with the backend.)
3. **Weather** — rain/snow/thunderstorms (touches light, spawning,
   lightning). Optional for "playable", needed for fidelity.
4. **More mobs + real pathfinding** — skeleton (arrows),
   spider/creeper, remaining animals. Pathfinding lands HERE with the
   mobs that need it (so far: direct seek + wall-follow + single-step
   hop). Render interpolation done (partial tick).
5. **Mechanics leftovers** — food backend (FoodStats is a stub: hunger
   never depletes), buckets/bow-use states, armor visuals/durability,
   redstone/rails, TNT, sleep/XP/riding/achievements/stats,
   enchanting, silverfish/ice, vine spread ticks, swamp water tint,
   maps in hand, potion overlay, nameplates, entity shadows + entity
   lighting (mobs/player render fullbright), enchant glint,
   stairs-as-3D in hand. DONE since replan: live fluids (flow/harden/
   scheduling + full render: lowered surfaces, flow UVs, animated
   TextureFX, transparent pass, underwater fog+overlay), shape blocks
   (slabs, snow layers, ladder/vine quads, precise picking), textured
   EntityItem drops (pop motion, pushOutOfBlocks, idDropped pops),
   sneak 0.3x + sprint 1.3x (double-tap + frame edges, FOV-less like
   1.0), player rendering (first-person hand+item/swing/equip,
   third-person F5 + biped model + char.png), placement audit
   (groundcover fluids, chest/rail, lilypad raycast, RMB repeat),
   pickup bbox, drops no-push, harvest order, loaded-mob ticks,
   entity placement check.

Known debts: `craftpp_demo` deprecated (unmaintained, notice at
startup); zombie feel pending real paths (interpolation done).

### M5b — Streaming + multithread (after playable singleplayer)
Out of the fixed 3×3: chunks generate around the walking player
(on-demand provide + unload beyond radius), and generation moves
off-thread so it never blocks the tick.

Split order (one at a time, green suite between steps):
1. **ChunkGen/IO worker**: one `std::jthread` + job queue (provide
   requests per (cx,cz,seed)). The worker generates terrain+carve+
   install+populate on private copies and hands the finished chunk over
   via `std::move`; Main installs it and marks it dirty for remeshing.
   Never share RNGs (architecture rule: one private `JavaRandom` per
   job, seed `worldSeed ^ hash`).
2. **Off-thread meshing**: CPU meshing on the `16x16 + 1 border`
   snapshot (never the live chunk), VBO upload on the Render thread.
3. **Render split** (last): dedicated GL context; Main stays the sole
   `World` writer with Java-identical 20 TPS ticks so a future server
   session does not desync.

Stay single-threaded until needed: all M0–M5 parity work requires total
determinism — light/physics update order must be 100% reproducible.

Exit: endless walk on the same seed without hitches; same chunks as
single-threaded generation (byte-identical for equal seeds);
green `ctest` with a clean thread sanitizer where available.

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
