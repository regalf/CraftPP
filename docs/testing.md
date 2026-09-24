# Parity testing methodology

Every ported subsystem is validated **differentially against real OpenJDK**
running the deobfuscated 1.0 sources, not against hand-written expectations.

## How it works

1. A small Java harness (in `/tmp`, **never committed** — the repo must not
   contain Java/Mojang code) exercises the real classes and prints vectors:
   - `GenVectors.java` → `java.util.Random` streams (seeds 42/12345/0/−1),
     NBT golden bytes (real `DataOutputStream` framing + `GZIPOutputStream`).
   - `GenHarness.java` → GenLayer biome/voronoi/temperature/rainfall regions
     (compiled against `src/` with `javac -sourcepath`, plus `lang/` +
     `achievement/map.txt` resources on the classpath).
   - `GenTerrain.java`, `GenCaves.java`, `GenPopulate.java`, … → full
     chunk pipelines via reflection into private fields
     (`rand`, `caveGenerator`, …), with `NullSaveHandler` fakes where the
     engine requires an `ISaveHandler`.
2. A Python script converts the output into a `tests/test_*.cpp` file with
   embedded vectors (marked `GENERATED`, with regeneration instructions).
3. The C++ test replays the same calls and compares bit-exactly (ints,
   float/double bit patterns, full byte arrays, `Arrays.hashCode`
   equivalents).

## Rules that keep the tests honest

- Ground truth always comes from **executing** Java, never from memory or
  from a second implementation of the same algorithm.
- When a test fails, the bug is **assumed to be on the C++ side first** —
  but verify the harness too (it bit us twice: missing `setSeed` that
  `provideChunk` performs, and a malformed hand-built NBT payload).
- Beware `IntCache`: `GenLayer` returns arrays **larger** than requested;
  only the first `w*h` entries are valid (the game reads only those).
- `Arrays.hashCode(byte[])` over full chunk arrays is the cheapest
  whole-chunk oracle; add heightmaps for debuggability and full bytes for at
  least one chunk per feature.
- Flaky JVM launches (empty stdout when piped) were observed: always write
  harness output to a file and check its size before generating tests.
- Keep harnesses out of the repo: `/tmp/genvectors`, `/tmp/genworld` (local
  only). Test files embed the *vectors*, never Mojang code.

## Debugging ladder (what worked)

1. Compare hashes → 2. narrow to subsystem (caves-only vs ravine-only via
   targeted harnesses) → 3. isolate one node/feature on synthetic content
   (all-stone arrays) → 4. replicate the exact draw sequence standalone on
   both sides → 5. bit-level (`%.9f` / integer bit patterns) trajectory
   comparison → 6. per-write logs with iteration attribution. The bug is
   usually in step 4–5 territory (draw order, jittered-vs-base values).
