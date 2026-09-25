# Craft++ — Project Overview

Clean-room C++20 reimplementation of Minecraft 1.0 gameplay. Goal: a client
indistinguishable from the original Java client (singleplayer first, vanilla
1.0 server interop as the final test). See `plan.md` and `LEGAL_NOTICE.md`.

## Status (2026-09-24)

| Milestone | State | Proof |
|---|---|---|
| M0 foundations (CMake/C++20/GLFW loop/Catch2) | done, committed `69cdeea` | window opens, `ctest` green |
| M1 deterministic core (JavaRandom, MathHelper, Vec3, AABB, NBT) | done, committed `69cdeea` | 2994 assertions vs OpenJDK vectors |
| M2 chunk rendering (mesher, shaders, fog, frustum) | done, committed `115b092` | screenshot verified |
| M3a GenLayer stack + biomes | done, committed `c35395a` | 6720 biome ints vs OpenJDK |
| M3b noise + terrain + surface | done, committed `c35395a` | heightmaps + full 32 KB chunk bytes vs OpenJDK |
| M3c caves/ravines | **done** | 7×7 carved base byte-identical, see `known-issues.md` F6 |
| M3c decorator + populate | **done (1 documented cell)** | 5 sites × 3×3 bit-identical vs engine oracle; 17868/17870 assertions (M5 light gap) |
| M4 player physics + interaction | **done** | Entity/Living/PlayerSP/controllers bit-identical vs oracles (40482/40484); SP.onLivingUpdate + controller glue oracle-assisted (client stubs) |
| M4+ | not started | — |

Current test totals: 40484 assertions, 40482 green; the 2 failures are one
documented cell (M5 light-engine gap, true Java golden kept).

## Architecture (target)

Three threads (not yet split — M0–M3 run single-threaded):

- **Main/Tick**: world state, 20 TPS fixed timestep.
- **Render**: owns the GL 3.3 core context (GLFW + libepoxy); sole GL caller.
- **ChunkGen/IO**: worldgen + CPU meshing on snapshots.

All game logic is GL-free and thread-movable by construction: the mesher
emits plain `Mesh` data, worldgen works on raw byte arrays.

## Repository layout

```text
src_cpp/core/    log, random (JavaRandom), math_helper, vec3, aabb, nbt
src_cpp/world/   block, blocks (raw ids), biome, chunk, chunk_manager,
                 genlayer, noise, provider (terrain), region, mapgen (caves),
                 worldgen (features/decorator)
src_cpp/render/  mesh, mesher, tessellator, shader(s), texture, frustum
src_cpp/app/     main.cpp (window + demo orbit camera + --screenshot)
tests/           Catch2 parity tests (many GENERATED from OpenJDK output)
third_party/stb  stb_image + stb_image_write (public domain)
docs/            this documentation
src/             ORIGINAL deobfuscated Java, local reference ONLY (git-ignored)
assets/          ORIGINAL jar textures/audio, local testing ONLY (git-ignored)
```

## Build & test (Arch)

```sh
# deps (one-off): glfw glm libepoxy openal catch2 cmake ninja zlib
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/craftpp_tests            # full suite
ctest --test-dir build           # same via CTest
./build/craftpp --seed 1 --screenshot /tmp/shot.png   # visual check
./build/craftpp --flat           # M2 flat chunk
```

Commits: private repo `CraftPP`, `main` only. Rule: commit + push at the
end of every completed step.
