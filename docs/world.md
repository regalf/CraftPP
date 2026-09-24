# World subsystem (`src_cpp/world/`)

Terrain pipeline, from seed to decorated chunk bytes. All ports are
behavioral (original C++), validated differentially against OpenJDK harnesses
kept in `/tmp` (never committed).

## Data model

- `block.hpp/cpp` — minimal typed registry (`BlockId`: air/stone/grass/
  dirt/bedrock/water/sand/ice) with `terrain.png` tiles per face and the
  grass-tint flag. Grows in M4.
- `blocks.hpp` — raw numeric ids (`bid::`, mirrors `Block.java` constants)
  plus material classification (solid/leaves/water/lava/nonsolid),
  `is_opaque` (mirrors `opaqueCubeLookup`), `light_opacity`
  (leaves 1, fluids/ice 3, snow cover 0, else opaque ? 255 : 0),
  `is_ground_cover` (vine + snow layer, for `canPlaceBlockAt`) and
  `is_normal_cube` (pumpkin soil rule).
- `biome.hpp/cpp` — `BiomeId` 0–15, static table (heights, temperature,
  rainfall, top/filler blocks; only Desert=sand and Mushroom=mycelium
  override the grass/dirt defaults), decorator params per biome
  (`DecorParams`), and the per-biome tree picker (`pick_tree`, exact draw
  sequences).
- `chunk.hpp/cpp` — 16×128×16 `BlockId` store (out-of-bounds reads return
  air), `fill_flat` (M2 demo), `fill_from_raw` (generator bytes → store).

## Seed → biomes

- `genlayer.hpp/cpp` — all 18 GenLayer types + the `func_35497_a` chain
  builder (`make_layers`, returns biome/voronoi/temperature/rainfall like the
  source's 5-element array). 64-bit wrapping LCG, 32-bit wrapping cell
  coordinates (`i32_add`/`i32_shl` — plain signed overflow would be UB),
  Zoom majority vote including the two verbatim `return var3` quirks,
  Voronoi 4× jitter, SmoothZoom interpolation, temperature/downfall mixes.
  Test: 6720 ints over 3 seeds (biome/voronoi/temp/rain regions) from real
  OpenJDK, bit-identical.
- `chunk_manager.hpp/cpp` — `ChunkManager`: `block_biomes` (voronoi, like
  `loadBlockGeneratorData`), `coarse_biomes` (1:4 grid for noise shaping),
  `temperatures` (`min(raw,65536)/65536`). (BiomeCache memoization is a pure
  optimization, deferred to M5.)

## Seed → terrain

- `noise.hpp/cpp` — `Perlin` (2D and 3D branches with the exact gradient
  mix, lattice masking, fade polynomial, amplitude scaling) + `Octaves`
  (per-octave coordinate wrap `% 16777216`, `func_4109_a` 2D form; note its
  unused 8th argument). Construction consumes `JavaRandom` exactly like the
  source (3 doubles + Fisher–Yates per Perlin).
- `provider.hpp/cpp` — `TerrainProvider`: `set_chunk_seed`
  (`cx*341873128712 + cz*132897987541`, like `provideChunk`),
  `generate_terrain` (trilinear noise interpolation over the 5×17×5 field,
  stone/water/air fill, sea level 63) and `replace_biome_blocks` (dirt depth
  from `stoneNoise` — including the transposed-index quirk — bedrock
  randomization, top/filler/ice/water rules, sand→sandstone transition).
  Output: raw 16×128×16 bytes in Java layout. Test: heightmaps +
  `Arrays.hashCode` over 8 chunks + all 32768 bytes of one chunk, from real
  OpenJDK — identical.

## Carvers & features (M3c, in progress)

- `region.hpp/cpp` — `RegionWorld`: multi-chunk id+metadata store,
  `top_solid_or_liquid` and `height_value` replicating the exact scans,
  fresh-world mini-skylight (`15 − opacity above`) for placement checks.
- `mapgen.hpp/cpp` — `MapGenCaves`/`MapGenRavine`: neighbor-range carving
  with per-neighbor seeding, full node paths (bifurcations, large nodes,
  water abort, lava depth, grass-top fixup via biome lookup).   Tests pass for
  the covered chunks; one open fixup-boundary bug remains (see
  `docs/known-issues.md`).
- `worldgen.hpp/cpp` — `FeatureGen`: all `WorldGen*` (ores, sand/clay
  patches, 6 tree types incl. persistent `BigTree` state, big mushrooms,
  flowers/grass/bushes/reeds/cacti/pumpkins/lilies, lakes, dungeons with
  loot-draw replication, liquid-spring placement), `decorate()` (full
  `decorate_do` order minus fluid spread), `populate_chunk()` (populate
  seeding, lakes, dungeons, biome-at-corner+16, ice/snow cap).
  Validation via `test_populate` (5 sites × 3×3 chunks) is still failing —
  see known issues.

Deferred to later milestones: fluid spread (M5 fluid sim), spawner entities
(M4/M5), structures/mineshafts/villages/strongholds (M5), skylight engine
(M5), save format (M5).
