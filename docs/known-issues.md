# Known issues

## Fixed bugs (for the record)

### F1. C++ unspecified argument evaluation order swaps RNG draws
- **Symptom**: cave bifurcations produced different child seeds/sizes than
  Java; dozens of scope diverge after the first bifurcation.
- **Root cause**: `cave_node(lr, lr.next_long(), ..., lr.next_float()*0.5F+0.5F,
  ...)` — C++ may evaluate function arguments in any order, while Java is
  strictly left-to-right. GCC evaluated the float before the long, swapping
  the values the two draws received.
- **Fix**: hoist **every** RNG draw into a named temporary in source order
  (`const long seed_a = lr.next_long(); const float size_a = ...;` then
  call). Applied to cave/ravine bifurcation calls, velocity updates (6
  floats), size computations, and the ravine width profile.
- **Rule** (also in `plan.md`): one RNG draw per statement, everywhere,
  forever. Single-draw statements are safe; anything with ≥2 draws in one
  expression is a latent wild bug.

### F2. Ravine height used the jittered width
- **Symptom**: ravine carve too narrow/short vs Java (52 missing cells in
  the isolated test).
- **Root cause**: source computes `var30 = var54 * var17` (height from the
  **unjittered** width) *before* jittering `var54`; the port multiplied the
  already-jittered width. Found by printing real Java `width`/`height` at
  iteration 1 (4.818 vs 3.810 — the latter impossible since jitter ≤ 1.0).
- **Fix**: keep `width_base`, jitter a copy.

### F3. `stoneNoise` sampled on the wrong axis
- **Symptom**: chunks with `cz != 0` got wrong dirt depth (stone↔dirt flips).
- **Root cause**: source calls
  `generateNoiseOctaves(..., cx*16, cz*16, 0, ...)` — the second chunk coord
  feeds noise **Y**, not Z. The port passed it as Z (invisible when `cz=0`).
- **Fix**: one-line argument reorder. Lesson: transcribe call signatures
  positionally, never "by meaning".

### F4. Screenshot channel rotation + teardown crash (M2)
- **Symptom**: rainbow-shifted screenshot with a diagonal artifact line;
  abort in libepoxy after screenshot.
- **Root causes**: (a) `glReadPixels` default `GL_PACK_ALIGNMENT=4` with
  854×3-byte rows (2562, not multiple of 4) shifts every row by 2 bytes;
  (b) GL objects destroyed after `glfwTerminate` (dead context).
- **Fix**: `glPixelStorei(GL_PACK_ALIGNMENT, 1)`; inner scope for GL-owned
  objects. See `docs/render.md`.

### F5. Test-harness mistakes (not port bugs)
- Harness calling `generateTerrain`/`replaceBlocksForBiome` directly without
  the `rand.setSeed` that `provideChunk` performs (fixed via reflection).
- Hand-built NBT golden payload with malformed list-of-compound framing.
- Forgetting that `GenLayer` returns oversized `IntCache` arrays (only the
  first `w*h` entries are valid).

## OPEN BUG 1 — cave grass-top fixup divergence (M3c blocker) — FIXED

- **Root cause** (proven via chronological N/T/W/F logs on both sides):
  `var49` in `MapGenCaves.generateCaveNode` is declared per-column OUTSIDE
  the y loop and never reset — once grass is seen above in the same column
  scan, dirt carved below gets the biome top block even if the carved cell
  itself is dirt. The port reset it per cell. One-line fix (hoist out of the
  loop) in `cave_node` + the identical `var48` in `ravine_node`.
- **Proof**: full 7x7 carved base byte-identical to Java afterwards.
- Lesson: transcribe loop-carried flags with their exact scope; a flag that
  looks "per iteration" may be sticky by declaration placement.

## OPEN ISSUE 2 — populate/decorator validation (M3c second half) — FIXED

- **Status**: done. `test_populate` (5 sites × 3×3 = 45 chunks, ~1.5M cells)
  is bit-identical to the fixed-seed Java engine oracle except ONE
  documented cell (M5 light-engine gap, below). Suite: 17868/17870.
- **Method**: built a live engine oracle (`GenPopulate2`, /tmp only) driving
  the REAL `ChunkProviderGenerate` pipeline stages on prebuilt chunks, then
  fixed the port chunk-by-chunk in populate order with cell-level diffs.
- **Bugs found via the oracle** (all fixed):
  - F6 cave sticky fixup (above); F7 taiga1 `h1`/`top_r` derivation;
    F8 lava-spring y draws (3 nested levels, not 2+const);
    F9 leaves removal marking (`BlockLeaves.onBlockRemoval` 3×3×3 `|8`)
    + `setBlockID` parity (zero meta, early-out);
    F10 WithNotify placements (swamp vines, dungeon room/walls/chest/
    spawner, ice/snow cap) must notify neighbors (fluid convert/harden);
    F11 opaque leaves (`opaqueCubeLookup[18]`, fast graphics) in disc checks;
    F12 still-water dispatch (must not run flow logic) + schedule only for
    moving fluids (`BlockFluid.onBlockAdded` never schedules);
    F13 shared `isOptimalFlowDirection`/`flowCost` members (nested ticks
    clobber them — the fringe divergence);
    F14 deadbush soil is sand-only; F15 BigTree `heightLimit` global
    persistence; F16 lava `lightOpacity` 255.
- **Harness findings** (documented, not port bugs):
  - `World.rand` (`new Random()`) seeds nondeterministically per JVM; nested
    fluid ticks draw it, so lava outcomes vary run-to-run. Goldens + C++
    tests pin it to 0 (mechanism validated, samples reproducible).
  - `SpawnerAnimals` is block-neutral and skipped on both sides (its
    `World.rand` draws would otherwise need replication).
- **Known accepted gaps** (unchanged, M4/M5): spawner entities, structures
  (mineshaft/village/stronghold), chest/spawner tile-entity contents.

## OPEN ISSUE 3 — single-cell light-engine gap (M5, accepted)

- **Status**: 1 cell in 45 populated chunks (~1.5M cells verified):
  site (-32,20) chunk (-32,19) local (4,72,11) — tall grass Java lacks,
  Craft++ grows. 2 assertions (`hash_bytes`, `hash_meta`) keep the true
  Java golden with a KNOWN-GAP comment.
- **Root cause** (proven with a light probe): Java's saved skylight there
  is stale-low (7, pre-carve hill shade never relit — cave carve writes raw
  arrays without relight, tree leaves below heightMap skip it) so
  `canBlockStay` (light ≥ 8) fails. Craft++ uses live opacity (11).
- **Fix (M5)**: synchronous light engine — `updateLightByType` BFS +
  `relightBlock` heightMap maintenance on every write (currently only
  `generateSkylightMap` at install + frozen skylight for plant checks).
  Deliberately deferred: it touches every write path and risks the
  currently-green mushroom/ice placements; M5 owns it per plan.md.

## Flaky JVM harness launches

`java -cp classes ... | grep/pipe` intermittently yields empty stdout.
Mitigation (already practice): redirect to a file, check byte size, then
process. Never generate tests from an unchecked pipe.
