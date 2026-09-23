// NBT parity tests. Golden payloads were produced by real Java
// DataOutputStream/GZIPOutputStream code (/tmp/genvectors/GenVectors.java,
// kept out of the repo) mirroring NBTBase.writeTag framing.

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <string>
#include <vector>

#include "core/nbt.hpp"

namespace {

std::vector<std::uint8_t> from_hex(const std::string& hex) {
  std::vector<std::uint8_t> out;
  out.reserve(hex.size() / 2);
  for (std::size_t i = 0; i < hex.size(); i += 2) {
    out.push_back(static_cast<std::uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
  }
  return out;
}

// Raw (uncompressed) golden root compound, 178 bytes.
const char* kGoldenHex =
    "0a0000010001627f02000173cfc703000169123456780400016c1122334455667788050001664048f5c306000164c0040000"
    "0000000007000361727200000004010203ff080003737472000568656c6c6f080003756e69000963616666c3a8e298830900"
    "04696e7473030000000200000064ffffff38090005636f6d70730a00000002010001780100060001643ff8000000000000000"
    "90005656d70747901000000000a00037375620800016b0001760000";

// Same payload gzipped by Java GZIPOutputStream, 178 bytes.
const char* kGoldenGzHex =
    "1f8b08000000000000ffe362606064604caa6762602c3e7f9c99813153c824ac828581314750c9d82534adbc83958131cdc1"
    "e3eb613606c694032c0c60c0cec09c58540464b0303231ffe760602e2e296260cd48cdc9c907724af33219389313d3d20eaf7"
    "834a39993812533afa49819a89a098853feffff6fc1c9c09a9c9f5b50cc051204da5fc1c80032defe07c47806a07c6a6e4149"
    "252388c30534be34898381319b81b18c810100b67bff11b2000000";

const craftpp::nbt::Tag* get(const craftpp::nbt::TagCompound& c, const std::string& key) {
  const auto* t = c.find(key);
  REQUIRE(t != nullptr);
  return t;
}

}  // namespace

TEST_CASE("nbt decodes java golden payload", "[nbt]") {
  const auto bytes = from_hex(kGoldenHex);
  REQUIRE(bytes.size() == 178);
  craftpp::nbt::Reader r(bytes.data(), bytes.size());
  const auto root = craftpp::nbt::read_root(r);
  REQUIRE(root.type == craftpp::nbt::TagType::Compound);
  REQUIRE(r.remaining() == 0);
  const auto& c = *root.compound;

  CHECK(get(c, "b")->i8 == 0x7F);
  CHECK(get(c, "s")->i16 == -12345);
  CHECK(get(c, "i")->i32 == 0x12345678);
  CHECK(get(c, "l")->i64 == 0x1122334455667788LL);
  CHECK(get(c, "f")->f32 == 3.14F);
  CHECK(get(c, "d")->f64 == -2.5);
  REQUIRE(get(c, "arr")->bytes == std::vector<std::int8_t>{1, 2, 3, -1});
  CHECK(get(c, "str")->str == "hello");
  CHECK(get(c, "uni")->str == "caff\xc3\xa8\xe2\x98\x83");  // "caffè☃", exercises modified UTF-8

  const auto* ints = get(c, "ints");
  REQUIRE(ints->type == craftpp::nbt::TagType::List);
  REQUIRE(ints->list->element == craftpp::nbt::TagType::Int);
  REQUIRE(ints->list->items.size() == 2);
  CHECK(ints->list->items[0].i32 == 100);
  CHECK(ints->list->items[1].i32 == -200);

  const auto* comps = get(c, "comps");
  REQUIRE(comps->list->element == craftpp::nbt::TagType::Compound);
  REQUIRE(comps->list->items.size() == 2);
  CHECK(comps->list->items[0].compound->find("x")->i8 == 1);
  CHECK(comps->list->items[1].compound->find("d")->f64 == 1.5);

  const auto* empty = get(c, "empty");
  REQUIRE(empty->list->items.empty());
  CHECK(get(c, "sub")->compound->find("k")->str == "v");
}

TEST_CASE("nbt re-encode is byte-identical", "[nbt]") {
  const auto bytes = from_hex(kGoldenHex);
  craftpp::nbt::Reader r(bytes.data(), bytes.size());
  const auto root = craftpp::nbt::read_root(r);

  craftpp::nbt::Writer w;
  craftpp::nbt::write_root(w, "", root);
  CHECK(w.bytes() == bytes);
}

TEST_CASE("nbt gunzips java payload", "[nbt]") {
  const auto gz = from_hex(kGoldenGzHex);
  const auto raw = craftpp::nbt::gzip_decompress(gz.data(), gz.size());
  CHECK(raw == from_hex(kGoldenHex));

  // And our own compressor round-trips through our decompressor.
  const auto recompressed = craftpp::nbt::gzip_compress(raw.data(), raw.size());
  CHECK(craftpp::nbt::gzip_decompress(recompressed.data(), recompressed.size()) == raw);
}

TEST_CASE("nbt rejects bad input", "[nbt]") {
  const std::uint8_t bad_type[] = {0x63, 0x00, 0x00};  // type 99
  craftpp::nbt::Reader r1(bad_type, sizeof(bad_type));
  CHECK_THROWS_AS(craftpp::nbt::read_named(r1), craftpp::nbt::Error);

  const std::uint8_t not_compound[] = {0x01, 0x00, 0x01, 'x', 0x00};  // TAG_Byte root
  craftpp::nbt::Reader r2(not_compound, sizeof(not_compound));
  CHECK_THROWS_AS(craftpp::nbt::read_root(r2), craftpp::nbt::Error);

  CHECK_THROWS_AS(craftpp::nbt::Tag::make_string(""), std::invalid_argument);
}
