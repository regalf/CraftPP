// McRegion RegionFile tests: zlib payloads, sector alloc (rewrite/reuse/
// grow), header layout, and interop with real Java-written region files.

#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "core/nbt.hpp"
#include "world/mcregion.hpp"

namespace {

std::string scratch(const char* name) {
  auto p = std::filesystem::temp_directory_path() / name;
  std::remove(p.c_str());
  return p.string();
}

TEST_CASE("zlib round-trips through the gzip auto-detect reader", "[mcregion]") {
  const std::string msg =
      "McRegion payload: blocks, nibbles, heightmap. Repeated repeated repeated.";
  const auto c =
      craftpp::nbt::zlib_compress(reinterpret_cast<const std::uint8_t*>(msg.data()), msg.size());
  REQUIRE(c.size() >= 2);
  // zlib wrapper magic, not gzip.
  CHECK(c[0] == 0x78);
  const auto d = craftpp::nbt::gzip_decompress(c.data(), c.size());
  CHECK(std::string(d.begin(), d.end()) == msg);
}

TEST_CASE("RegionFile write/read round-trip with realloc paths", "[mcregion]") {
  using craftpp::world::RegionFile;
  const auto path = scratch("craftpp_region_test.mcr");
  std::vector<std::uint8_t> small(100, 7);
  std::vector<std::uint8_t> big(9000, 9);
  std::vector<std::uint8_t> small2(120, 5);
  {
    RegionFile rf(path, true);
    REQUIRE(rf.is_open());
    CHECK(!rf.has_chunk(0, 0));
    rf.write_chunk(0, 0, small.data(), small.size());
    rf.write_chunk(1, 0, big.data(), big.size());    // multi-sector grow
    rf.write_chunk(0, 0, small2.data(), small2.size());  // rewrite/reuse
    CHECK(rf.has_chunk(0, 0));
    CHECK(rf.has_chunk(1, 0));
    CHECK(!rf.has_chunk(31, 31));
    auto a = rf.read_chunk(0, 0);
    auto b = rf.read_chunk(1, 0);
    REQUIRE(a.has_value());
    REQUIRE(b.has_value());
    CHECK(*a == small2);
    CHECK(*b == big);
    CHECK(!rf.read_chunk(31, 31).has_value());
    CHECK(!rf.read_chunk(-1, 0).has_value());
  }
  // Reopen from disk: header + payloads survive.
  {
    RegionFile rf(path, false);
    REQUIRE(rf.is_open());
    CHECK(rf.has_chunk(0, 0));
    CHECK(rf.read_chunk(0, 0) == small2);
    CHECK(rf.read_chunk(1, 0) == big);
  }
  // File size is a whole number of 4 KiB sectors.
  CHECK(std::filesystem::file_size(path) % 4096 == 0);
  std::remove(path.c_str());
}

TEST_CASE("RegionFile path naming matches Java (r.x.z.mcr, floor div)", "[mcregion]") {
  using craftpp::world::RegionFile;
  CHECK(RegionFile::path_for("w", 0, 0) == "w/region/r.0.0.mcr");
  CHECK(RegionFile::path_for("w", 33, -1) == "w/region/r.1.-1.mcr");
  CHECK(RegionFile::path_for("w", -1, -33) == "w/region/r.-1.-2.mcr");
  CHECK(RegionFile::local(-1) == 31);
  CHECK(RegionFile::local(33) == 1);
}

}  // namespace
