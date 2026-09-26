#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace craftpp::world {

// Byte-exact port of RegionFile (McRegion, 1.0): 4 KiB sectors, 8 KiB
// header (1024 big-endian offsets + 1024 timestamps), zlib chunk payloads
// (compression version 2; gzip version 1 accepted on read). Sector
// rewrite/reuse/grow policy mirrors the source so files stay
// interchangeable with Java 1.0.
class RegionFile {
 public:
  static constexpr int kSector = 4096;
  // Region file path: <save_dir>/region/r.<cx>>5>.<cz>>5>.mcr
  // (Java arithmetic shift; identical under C++20 two's complement).
  static std::string path_for(const std::string& save_dir, int cx, int cz);
  static int local(int c) { return c & 31; }

  RegionFile() = default;
  // Opens (or creates when create=true) the file at path.
  explicit RegionFile(const std::string& path, bool create = true);
  RegionFile(const RegionFile&) = delete;
  RegionFile& operator=(const RegionFile&) = delete;
  ~RegionFile() { close(); }

  bool is_open() const { return open_; }
  void close();

  bool has_chunk(int lx, int lz) const;
  // Decompressed NBT payload, or nullopt when absent/unreadable.
  std::optional<std::vector<std::uint8_t>> read_chunk(int lx, int lz);
  // Compresses (zlib) and stores the payload.
  void write_chunk(int lx, int lz, const std::uint8_t* data, std::size_t len);

 private:
  static int index(int lx, int lz) { return lx + lz * 32; }

  bool open_ = false;
  std::string path_;
  // Raw file bytes (whole file held in memory; region files are small).
  std::vector<std::uint8_t> file_;
  std::int32_t offsets_[1024] = {};
  std::int32_t timestamps_[1024] = {};
  std::vector<bool> free_;
  bool dirty_ = false;

  void flush();
};

}  // namespace craftpp::world
