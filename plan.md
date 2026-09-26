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

Status: **done** (verified differential vs OpenJDK, suite 40482/40484 —
the 2 failures are the known M5 light cell).
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
- Throwaway demo: `craftpp_demo` (WASD/mouse/Space/Shift, LMB mine,
  RMB place stone, ESC quit) on 3×3 generated chunks with real physics.
  Wiring only — picking is full-cube, placing is stone-only, no HUD/mobs.
  Movement is basic and slightly choppy by design; the real implementation
  (menus, keybinding wiring, options, smooth input handling, sprint/fly
  keys, inventory keys, GameSettings) lands with the proper client in M5+.

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
spread with a scheduled queue). Suite fully green, zero gaps. Next:
TileEntity, mobs.

### M5b — Streaming + multithread (dopo il singleplayer giocabile)
Il mondo esce dal 3×3 fisso: chunk generati attorno al giocatore mentre
cammina (provide on demand + unload oltre il raggio), e la generazione va
off-thread perché non deve mai bloccare il tick.

Ordine di split (uno alla volta, suite verde tra l'uno e l'altro):
1. **ChunkGen/IO worker**: un `std::jthread` + coda jobs (richieste provide
   per (cx,cz,seed)). Il worker genera terrain+carve+install+populate su
   copie private e consegna il chunk pronto via `std::move`; il Main lo
   installa e lo marca dirty per il remesh. Mai RNG condivisi (regola
   architettura: un `JavaRandom` privato per job, seed `worldSeed ^ hash`).
2. **Meshing off-thread**: CPU meshing sullo snapshot `16x16 + 1 bordo`
   (mai sul chunk live), upload VBO sul thread Render.
3. **Render split** (ultimo): context GL dedicato; il Main resta l'unico
   writer del `World` con tick Java-identico a 20 TPS così una futura
   sessione server non desynca.

Resta single-thread finché non serve: tutto il lavoro M0–M5 (parità
differenziale) richiede determinismo totale — l'ordine degli update
luce/fisica deve essere riproducibile al 100%.

Exit: camminata infinita sullo stesso seed senza hitches; stessi chunk
della generazione single-thread (byte-identici a parità di seed);
`ctest` verde con thread sanitizer pulito dove disponibile.

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
