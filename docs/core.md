# Core library (`src_cpp/core/`)

Deterministic primitives. Everything here is GL-free, dependency-free
(except zlib for NBT gzip) and covered by bit-exact parity tests.

## `random.hpp/cpp` — JavaRandom

Bit-identical reimplementation of `java.util.Random` (48-bit LCG:
multiplier `0x5DEECE66D`, addend `0xB`, 48-bit mask).

- `set_seed`, `next(bits)`, `next_int()`, `next_int(bound)` (including the
  rejection loop for non-powers of two and the fast path for powers of two),
  `next_long`, `next_float`, `next_double`, `next_boolean`, `next_bytes`,
  `next_gaussian` (polar/Box-Muller with cached second deviate, like the
  source).
- Signed overflow is avoided everywhere: state lives in `uint64_t`, results
  are cast on the way out.
- Tests (`tests/test_random.cpp`): full draw streams for seeds 42, 12345, 0,
  -1 plus `setSeed`, generated with real OpenJDK (`GenVectors.java`, kept in
  `/tmp`, never committed). Gaussian deviates match bit-exact on this host's
  libm.

## `math_helper.hpp/cpp` — MathHelper

Mirror of `MathHelper.java`:

- 65536-entry sin table (`sin(i*2π/65536)` as float), built once;
  `sin`/`cos` use the `* 10430.378F` scale and `& 0xFFFF` mask exactly.
- `sqrt_float/sqrt_double` (float-returning!), truncation floors
  (`floor_float/floor_double/floor_double_long`), `fast_floor`
  (`(int)(v+1024)-1024`), `clamp`, `bucket_int`, `random_int_in_range`.
- Verified: all 65536 table entries bit-identical to OpenJDK output.

## `vec3.hpp/cpp` — Vec3

Value-type port of `Vec3D.java` (the source pooled mutable instances; values
are cheaper and thread-safe). Same ops, same quirks — notably
`subtract(other)` returns `other - this`, documented at the declaration.
Ray helpers return `std::optional<Vec3>`.

## `aabb.hpp/cpp` — Aabb

Value-type port of `AxisAlignedBB.java`: `add_coord`, `expand`,
`contract`, `offset`, `clamp_x/y/z` (collision clamps), `intersects`,
`contains`, `average_edge_length`, and `clip()` (ray vs box, returns face id
0–5 with the source's numbering). Tested against hand-computed cases.

## `nbt.hpp/cpp` — NBT

Wire-compatible port of `NBTBase`/`NBTTag*`/`CompressedStreamTools`:

- Big-endian reader/writer, Modified UTF-8 (`writeUTF`/`readUTF`, including
  `NUL` as `C0 80` and astral planes as CESU-8 surrogate pairs).
- All 11 tag types; compounds preserve file order (unlike Java's `HashMap`)
  so decode→encode round-trips are byte-identical.
- Empty lists declare element type 1, like `NBTTagList.writeTagContents`.
- `gzip_compress`/`gzip_decompress` via zlib (deflate/inflate with gzip
  wrapping), `read_root`/`write_root` (root must be a named compound).
- Tests: golden payload built with real `DataOutputStream`/`GZIPOutputStream`
  (decode checks, byte-identical re-encode, gunzip of Java's bytes).

## `log.hpp/cpp` — logging

Minimal stderr logger (`log_info`/`log_error`). Placeholder for a real sink.
