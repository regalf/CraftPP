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

## OPEN ISSUE 3 — single-cell light-engine gap (M5, accepted) — CLOSED

- The M5 synchronous light engine (`RegionWorld`: stored heightMap +
  skylight/blocklight nibbles, `relightBlock`, `updateLightByType` BFS with
  the vanilla local-coords quirk, `updateAllLightTypes` on every write,
  stored-height `canBlockSeeTheSky`, lazy precipitation heights) reproduces
  Java's 7 at site (-32,20) chunk (-32,19) local (4,72,11): it is live canopy
  shade from leaves at 76-79, not stale pre-carve shade as first diagnosed
  (both theories predict 7; the live one is what the engine computes, and the
  chunk now hashes green). Correction to the M3c diagnosis is kept here so
  nobody re-"fixes" it.
- Method note: `scheduleLightingUpdate` is a no-op in 1.0 and the
  skylight-occlusion flags only drain in the tick loop, so populate parity
  needs just the synchronous part. `getSavedLightValue` y>=128 clamps to 127
  (fixed during the port); `getBlockLightValue` block branch covers lava
  springs for the ice/snow `<10` checks; the cap reads precipitation height.

## OPEN ISSUE 4 — three oracle-divergent light cells — CLOSED (was my bug)

- The 4 marginal cells ((-32,20) grass, (-31,21) 3 grass, (-31,-22)
  mushroom) are GREEN with the true goldens. Root cause was a real engine
  bug, not oracle artifacts: the `updateLightByType` increase-spread phase
  was nested inside the `want < saved` decrease branch, so light *increases*
  never propagated (only decreases did). Found via the M5 ice-melt live
  test (lava block light stayed 0). One-brace fix in `region.cpp`.
- Retraction: the earlier "QED unreachable, must be oracle artifacts"
  analysis was wrong — it assumed the spread ran. The bisection correctly
  showed per-set BFS was a no-op at those cells, but the live re-brightening
  arrives through relight-adjacent increase paths that need the working
  spread phase. Lesson: when mechanism and goldens disagree, suspect the
  transcription structure (brace nesting!), not the oracle.
- Net M5 light-engine result: suite fully green including all 45 populated
  chunks (~1.5M cells) with zero gaps.

## Fixed bugs, M4 round (for the record)

### F17. Sticky per-type block bounds (collision shapes)
- Several `getCollidingBoundingBoxes` overrides mutate the shared Block
  instance bounds (`setBlockBounds`) as a side effect, so the single-box
  query returns leftovers of previous calls — PER BLOCK TYPE. Proven with
  a probe: panes show leftovers, piston-extension ends with a trailing
  full-cube reset (a `grep -A25` had cut it off), stairs/cauldron reset,
  brewing ends item-bounds, end frame ends 13/16.
- Fix: `BlockCollider` keeps sticky local bounds per id, updated at exactly
  the source `setBlockBounds` points. 113 oracle CASEs green in order.

### F18. Sneak-loop assigns want AFTER the body
- Java assigns `var11 = var1` in the for-update slot. 0.3 − 6×0.05 leaves
  1.4e-17 float dust, which the `< 0.05` threshold zeroes — Java's want
  becomes 0.0, a pre-shrink assignment keeps 1.4e-17 (0.05 too far).

### F19. Environmental hits must route through virtual attack()
- `dealFireDamage`/`setOnFireFromLava`/fire-tick/cactus call
  `this.attackEntityFrom` (the Living override with health logic), not a
  world hook. Caught by lavaswim (fire=301, health untouched).

### F20. Living eye height is height×0.85, heartsHalvesLife 20
- Not 1.62 (that's nearer the player path, which uses yOffset 1.62 +
  eye 0.12). Drown timing off by one tick otherwise.

### F21. isInsideOfMaterial subtracts an extra 1/9
- `var8 = heightPercent - 1/9`, surface = (y+1) − var8 (full height for
  still water). Without it the swim air meter lags one tick.

### F22. EntityPlayer overrides updateEntityActionState (swing only)
- No super call: no entityAge++, no input zeroing, no rand draws. The
  Living version must NOT run for players.

### F23. Double armor application
- `EntityPlayer.damageEntity` applies the armor formula, then calls
  `super.damageEntity` which applies it AGAIN with the shared carry.
  Unblockable (fall/drown/fire-tick/...) sources skip both.

### F24. setEntityHealth clamps a discarded local
- `this.health = var1` runs first; the `maxHealth` clamp applies to the
  parameter copy. Overheal sticks (verified: hp=25).

### F25. getCurrentPlayerStrVsBlock water/airborne ÷5 + movement exhaustion
- Underwater (no aqua affinity) and airborne both divide strength by 5;
  `EntityPlayer.moveEntityWithHeading` adds walk/swim/dive exhaustion per
  tick. Sprint needs the +30% speed factors from `EntityPlayer.
  onLivingUpdate` (missed at first).

### F26. Tool ids in stacks are SHIFTED (+256)
- Pickaxe tables keyed on raw ids (1,14,…) never match stack ids
  (257,270,…). Caught by inspection before tests ran.

### Harness findings (documented, not port bugs)
- `Math.random()` (attackedAtYaw jitter, EntityItem motion/yaw, Living
  ctor render fields) is wild per JVM run: never asserted.
- `World.spawnParticle` draws `world.rand` (contained in particles, no
  entity feedback); drop item motion/yaw likewise wild (position exact).
- `EntityPlayerSP`/controllers need the Minecraft client: SP mc-free
  methods + full `onLivingUpdate` run against hand-written client stubs
  (real game code); controller glue is hand-replicated over real
  Block/World calls. Both flagged oracle-assisted, behavior-tested.
- Breaking piston-extension meta 6/7 crashes vanilla (`Facing` table).

## Flaky JVM harness launches

`java -cp classes ... | grep/pipe` intermittently yields empty stdout.
Mitigation (already practice): redirect to a file, check byte size, then
process. Never generate tests from an unchecked pipe.
