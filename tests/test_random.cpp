// Parity tests for JavaRandom against vectors produced by the real
// java.util.Random (OpenJDK 17, /tmp/genvectors/GenVectors.java — kept out
// of the repo so no Java lives in version control).
//
// Consumption order in every stream matches the generator:
//   N x nextInt(), 8 x nextInt(bound) per bound, 6 x nextLong,
//   6 x nextFloat, 6 x nextDouble, 16 x nextBoolean, 16 x nextBytes,
//   6 x nextGaussian.

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <cstring>
#include <vector>

#include "core/random.hpp"

namespace {

float bits_to_float(std::int32_t bits) {
  float f = 0.0F;
  std::memcpy(&f, &bits, sizeof(f));
  return f;
}

double bits_to_double(std::uint64_t bits) {
  double d = 0.0;
  const auto u = bits;
  std::memcpy(&d, &u, sizeof(d));
  return d;
}

std::uint64_t double_to_bits(double d) {
  std::uint64_t u = 0;
  std::memcpy(&u, &d, sizeof(u));
  return u;
}

}  // namespace

TEST_CASE("random seed 42 matches java.util.Random", "[random]") {
  craftpp::JavaRandom r(42);
  const std::int32_t i32[] = {-1170105035, 234785527, -1360544799, 205897768, 1325939940,
                              -248792245, 1190043011, -1255373459, -1436456258, 392236186};
  for (std::int32_t v : i32) CHECK(r.next_int() == v);

  const int bounds[] = {1, 2, 3, 5, 16, 100, 1000, 1 << 30, 2147483647};
  const std::int32_t expected[][8] = {
      {0, 0, 0, 0, 0, 0, 0, 0},
      {1, 1, 1, 0, 0, 0, 1, 1},
      {1, 1, 1, 0, 1, 0, 2, 2},
      {2, 2, 2, 3, 3, 4, 4, 0},
      {9, 1, 9, 1, 12, 12, 0, 5},
      {93, 40, 58, 56, 60, 10, 13, 59},
      {261, 3, 897, 819, 552, 601, 94, 794},
      {880996399, 202664859, 683591887, 406334749, 396341195, 146483750, 386820726, 468455214},
      {933427573, 1393273733, 982080986, 1494582460, 1015090625, 2042112093, 1007218200, 1178812594},
  };
  for (std::size_t b = 0; b < 9; ++b) {
    for (int v : expected[b]) CHECK(r.next_int(bounds[b]) == v);
  }

  const std::int64_t longs[] = {-3185338829648084755LL, 2786040548068544931LL, -3064627001040919599LL,
                                8491153766255516575LL, 5175639579568511616LL, 3614901514407423006LL};
  for (std::int64_t v : longs) CHECK(r.next_long() == v);

  const std::int32_t fbits[] = {1043829620, 1064723850, 1063099814, 1056338078, 1056514664, 1057772666};
  for (std::int32_t b : fbits) CHECK(r.next_float() == bits_to_float(b));

  const std::uint64_t dbits[] = {4601255172002374010ULL, 4603875603858301758ULL, 4604478985811549489ULL,
                                 4599352072278622128ULL, 4603323917535240840ULL, 4600355183156184560ULL};
  for (std::uint64_t b : dbits) CHECK(r.next_double() == bits_to_double(b));

  const int bools[] = {1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0};
  for (int v : bools) CHECK(r.next_boolean() == (v == 1));

  std::vector<std::uint8_t> bytes(16, 0);
  r.next_bytes(bytes);
  const int expected_bytes[] = {34, 60, -64, -96, -76, 84, -30, -57, -96, 85, -95, 118, -71, 89, 58, -88};
  for (std::size_t i = 0; i < 16; ++i) CHECK(static_cast<std::int8_t>(bytes[i]) == expected_bytes[i]);

  // Bit-exact gaussian deviates (java.util.Random uses StrictMath, and our
  // libm agrees on this host for these inputs — verified against OpenJDK).
  // Java prints signed int64; values below are the same bits as unsigned.
  const std::uint64_t gbits[] = {13834531926363917849ULL, 4600635373628685791ULL, 4599541238250069477ULL,
                                 13829665483994947463ULL, 13791698830938251429ULL, 13825581747669244556ULL};
  for (std::uint64_t b : gbits) CHECK(double_to_bits(r.next_gaussian()) == b);
}

TEST_CASE("random seed 12345 matches java.util.Random", "[random]") {
  craftpp::JavaRandom r(12345);
  const std::int32_t i32[] = {1553932502, -2090749135, -287790814, -355989640, -716867186,
                              161804169, 1402202751, 535445604, 1011567003, 151766778};
  for (std::int32_t v : i32) CHECK(r.next_int() == v);

  const int bounds[] = {1, 2, 3, 5, 16, 100, 1000, 1 << 30, 2147483647};
  const std::int32_t expected[][8] = {
      {0, 0, 0, 0, 0, 0, 0, 0},
      {1, 0, 1, 0, 0, 1, 1, 0},
      {2, 2, 2, 0, 0, 2, 0, 1},
      {3, 0, 2, 1, 0, 3, 1, 2},
      {15, 2, 3, 10, 4, 15, 11, 9},
      {74, 44, 54, 8, 80, 99, 58, 55},
      {594, 497, 294, 87, 553, 318, 583, 499},
      {635146185, 122960886, 405631995, 162585759, 532209538, 210675560, 640931459, 895079528},
      {1957520188, 859304826, 192552849, 730199992, 406673552, 2143760875, 1131129003, 254488900},
  };
  for (std::size_t b = 0; b < 9; ++b) {
    for (int v : expected[b]) CHECK(r.next_int(bounds[b]) == v);
  }

  const std::int64_t longs[] = {-701297860731463640LL, 8850753136911882592LL, -8954055381677511378LL,
                                -3600730930140391064LL, 8915619276898549583LL, 34763064450124911LL};
  for (std::int64_t v : longs) CHECK(r.next_long() == v);

  const std::int32_t fbits[] = {1058798379, 1060271876, 1058388232, 1061430144, 1060204071, 1042330928};
  for (std::int32_t b : fbits) CHECK(r.next_float() == bits_to_float(b));

  const std::uint64_t dbits[] = {4603947021154318309ULL, 4593557422913948128ULL, 4585173928037493152ULL,
                                 4605604157873873221ULL, 4601310765887229156ULL, 4595723315150791404ULL};
  for (std::uint64_t b : dbits) CHECK(r.next_double() == bits_to_double(b));

  const int bools[] = {0, 0, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 1};
  for (int v : bools) CHECK(r.next_boolean() == (v == 1));

  std::vector<std::uint8_t> bytes(16, 0);
  r.next_bytes(bytes);
  const int expected_bytes[] = {94, -24, 40, -92, 35, -71, -69, -83, 112, -68, -24, -82, -19, 33, 35, 44};
  for (std::size_t i = 0; i < 16; ++i) CHECK(static_cast<std::int8_t>(bytes[i]) == expected_bytes[i]);

  // Exact gaussian bits as emitted by OpenJDK on this host:
  const std::uint64_t gexact[] = {4612860961237808632ULL, 4605102539622155792ULL, 4597995741122015931ULL,
                                  4607446331823594733ULL, 4608626487882293307ULL, 4598884711973257375ULL};
  for (std::uint64_t b : gexact) CHECK(double_to_bits(r.next_gaussian()) == b);
}

TEST_CASE("random edge seeds and setSeed", "[random]") {
  SECTION("seed 0") {
    craftpp::JavaRandom r(0);
    const std::int32_t i32[] = {-1155484576, -723955400, 1033096058, -1690734402, -1557280266};
    for (std::int32_t v : i32) CHECK(r.next_int() == v);
  }
  SECTION("seed -1") {
    craftpp::JavaRandom r(-1);
    const std::int32_t i32[] = {1155099827, 1887904451, 52699159, -1941176418, -1451336087};
    for (std::int32_t v : i32) CHECK(r.next_int() == v);
  }
  SECTION("setSeed reproduces") {
    craftpp::JavaRandom r;
    r.set_seed(987654321LL);
    CHECK(r.next_int() == 1314001072);
    CHECK(r.next_int(1000) == 927);
    r.set_seed(987654321LL);
    CHECK(r.next_int() == 1314001072);
  }
  SECTION("bounded range holds") {
    craftpp::JavaRandom r(7);
    for (int b : {1, 2, 3, 17, 1000, 1 << 20}) {
      for (int i = 0; i < 200; ++i) {
        const int v = r.next_int(b);
        CHECK(v >= 0);
        CHECK(v < b);
      }
    }
  }
}
