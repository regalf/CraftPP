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

## OPEN BUG 1 — cave grass-top fixup divergence (M3c blocker)

- **Status**: under investigation. Narrowed to a single mechanism, root
  cause not yet proven.
- **Symptom**: carved base differs from Java in 4 of 49 tested chunks
  (seed 1: `(-2,0)`, `(-2,1)`, `(-1,0)`, `(1,0)`). Example: chunk `(-1,0)`,
  cell `(8,60,1)` — Java writes grass via the cave grass-top fixup, the
  port leaves dirt. Other diffs are the same signature (stone↔dirt at
  carve fringes, e.g. `(2..3,26..35,5..6)`).
- **What is proven identical** (differential tests, all green):
  - cave spawn sets (neighbor, count, positions, sizes, node seeds — 98–129
    caves per target, exact match);
  - single-node carves on synthetic content (normal, large, bifurcating,
    water-abort, lava-depth, grass-fixup configurations);
  - node trajectories bit-exact over dozens of iterations (positions,
    widths, heights, `% .9f` / integer bit patterns);
  - width profiles, sin table (all 65536 entries), RNG streams.
- **Leading hypothesis**: an evaluation-order-style draw bug in a rarely
  hit branch (large-node entry? `thin` flag path? distance-cull edge?) that
  shifts one node's path by ~1 ulp, flipping an ellipsoid-boundary
  decision (`nx²+ny²+nz² < 1.0`) and cascading through order-dependent
  carve/fixup interactions. The `(8,60,1)` case analysis shows the two runs
  must have diverged *before* iteration 80 of the responsible node.
- **Alternative hypothesis**: a biome-lookup difference at fixup coordinates
  (`biome_top_at` uses direct voronoi; Java uses `BiomeCache` — values
  should be equal but negative-coordinate regions are less tested).
- **Next steps**:
  1. Chronological per-write diff already built (`W` logs both sides);
     attribute the first diverging write to its node (NODE + trajectory
     logs exist) instead of comparing by `(start, ...)` counters, which
     repeat across nodes.
  2. Re-audit `recursive_generate` and the large-node entry for multi-draw
     expressions (rule F1) — especially `nextInt(nextInt(120)+8)` nesting
     (forced order, safe) vs any remaining same-expression siblings.
  3. Extend isolated node tests with large-node + negative-coordinate
     starts (current isolation tests all start inside 0..16, positive).
  4. If the trigger is a 1-ulp libm difference (`sqrt`/`pow` in BigTree
     paths don't apply here; `log` in gaussian — N/A), pin it with a
     bit-level trajectory diff of the *responsible* node (identified in
     step 1), not of convenient ones.

## OPEN ISSUE 2 — populate/decorator validation (M3c second half)

- **Status**: implemented, never green. `test_populate` (5 sites × 3×3
  chunks: taiga/desert/forest/swamp/plains around seed 1) fails ~875
  assertions, starting with tree-height stripes (e.g. tops 65 vs 63 in full
  rows — classic wrong/missing-tree signature) and meta-hash mismatches.
- **Causes** (to be separated once Bug 1 is fixed):
  1. The 4 bad base chunks above poison everything downstream (trees read
     heights, ores read stone, etc.).
  2. Genuine decorator bugs not yet hunted (tree variants, ore
     placement, lakes, dungeons, reed/cactus gates). Note the port
     already encodes several verified subtleties (taiga2 radius/state
     machine, corner-draw short-circuit, dungeon loot-draw replication with
     TileEntityChest size 27, mushroom light rule, pumpkin/reed/cactus
     stay-vs-place distinction).
  3. Test-harness risk: `GenPopulate` replicates `populate`+`decorate_do`
     manually (skipping spawner entities, ice/snow is *included*, fluid
     *spread* excluded); the real engine's lazy chunk-load cascade
     (`populateChunk`) is bypassed via pre-loading + `isTerrainPopulated`
     pre-flagging. Any mismatch between this staging and the C++ test
     driver (`build_site` in `test_populate.cpp`) shows up as diffs.
- **Known accepted gaps** (documented, M4/M5): fluid *spread* after spring
  placement (placement itself is replicated; `updateTick` can draw rand in
  the lava-hardening path), spawner entities, structures
  (mineshaft/village/stronghold), chest/spawner tile-entity contents.
- **Next steps**: fix Bug 1 → re-run → triage remaining diffs per feature
  (ores-only run, trees-only run) using the same isolate-and-compare ladder
  as `docs/testing.md`.

## Flaky JVM harness launches

`java -cp classes ... | grep/pipe` intermittently yields empty stdout.
Mitigation (already practice): redirect to a file, check byte size, then
process. Never generate tests from an unchecked pipe.
