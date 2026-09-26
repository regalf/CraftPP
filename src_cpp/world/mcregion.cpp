#include "world/mcregion.hpp"

#include <ctime>
#include <fstream>
#include <stdexcept>

#include "core/nbt.hpp"

namespace craftpp::world {

namespace {

std::int32_t get_i32(const std::vector<std::uint8_t>& f, long pos) {
  return static_cast<std::int32_t>((static_cast<std::uint32_t>(f[pos]) << 24) |
                                   (static_cast<std::uint32_t>(f[pos + 1]) << 16) |
                                   (static_cast<std::uint32_t>(f[pos + 2]) << 8) |
                                   static_cast<std::uint32_t>(f[pos + 3]));
}

void set_i32(std::vector<std::uint8_t>& f, long pos, std::int32_t v) {
  const auto u = static_cast<std::uint32_t>(v);
  f[pos] = static_cast<std::uint8_t>(u >> 24);
  f[pos + 1] = static_cast<std::uint8_t>((u >> 16) & 0xFF);
  f[pos + 2] = static_cast<std::uint8_t>((u >> 8) & 0xFF);
  f[pos + 3] = static_cast<std::uint8_t>(u & 0xFF);
}

}  // namespace

std::string RegionFile::path_for(const std::string& save_dir, int cx, int cz) {
  return save_dir + "/region/r." + std::to_string(cx >> 5) + "." + std::to_string(cz >> 5) + ".mcr";
}

RegionFile::RegionFile(const std::string& path, bool create) : path_(path) {
  std::ifstream in(path, std::ios::binary);
  if (in) {
    file_ = std::vector<std::uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  } else {
    if (!create) return;
    file_.assign(2 * kSector, 0);
    dirty_ = true;
  }
  if (file_.size() < static_cast<std::size_t>(2 * kSector)) {
    // Mirrors the source: short file gets zeroed ints, then padded to 4K.
    file_.resize(2 * kSector, 0);
    dirty_ = true;
  }
  while (file_.size() % kSector != 0) file_.push_back(0);

  const long sectors = static_cast<long>(file_.size() / kSector);
  free_.assign(static_cast<std::size_t>(sectors), true);
  free_[0] = false;
  if (sectors > 1) free_[1] = false;
  for (int i = 0; i < 1024; ++i) {
    offsets_[i] = get_i32(file_, i * 4L);
    const std::int32_t u = static_cast<std::uint32_t>(offsets_[i]);
    const long start = (u >> 8) & 0xFFFFFF;
    const long count = u & 255;
    if (offsets_[i] != 0 && start + count <= sectors) {
      for (long s = 0; s < count; ++s) free_[start + s] = false;
    }
  }
  for (int i = 0; i < 1024; ++i) timestamps_[i] = get_i32(file_, kSector + i * 4L);
  open_ = true;
}

void RegionFile::close() {
  if (open_) flush();
  open_ = false;
}

void RegionFile::flush() {
  if (!dirty_ || path_.empty()) return;
  // Header back into the image, then a single atomic-ish rewrite.
  for (int i = 0; i < 1024; ++i) set_i32(file_, i * 4L, offsets_[i]);
  for (int i = 0; i < 1024; ++i) set_i32(file_, kSector + i * 4L, timestamps_[i]);
  std::ofstream out(path_, std::ios::binary | std::ios::trunc);
  if (!out) throw std::runtime_error("RegionFile: cannot write " + path_);
  out.write(reinterpret_cast<const char*>(file_.data()), static_cast<std::streamsize>(file_.size()));
  out.flush();
  dirty_ = false;
}

bool RegionFile::has_chunk(int lx, int lz) const {
  if (lx < 0 || lx >= 32 || lz < 0 || lz >= 32) return false;
  return offsets_[index(lx, lz)] != 0;
}

std::optional<std::vector<std::uint8_t>> RegionFile::read_chunk(int lx, int lz) {
  if (!open_ || lx < 0 || lx >= 32 || lz < 0 || lz >= 32) return std::nullopt;
  const std::uint32_t off = static_cast<std::uint32_t>(offsets_[index(lx, lz)]);
  if (off == 0) return std::nullopt;
  const long start = (off >> 8) & 0xFFFFFF;
  const long count = off & 255;
  if (start + count > static_cast<long>(free_.size())) return std::nullopt;
  const long pos = start * kSector;
  const std::int32_t len = get_i32(file_, pos);
  if (len > kSector * static_cast<int>(count) || len <= 0) return std::nullopt;
  const std::uint8_t version = file_[pos + 4];
  std::vector<std::uint8_t> raw(file_.begin() + pos + 5, file_.begin() + pos + 4 + len);
  try {
    if (version == 1 || version == 2) {
      // gzip_decompress auto-detects the wrapper (gzip or zlib).
      return nbt::gzip_decompress(raw.data(), raw.size());
    }
  } catch (const nbt::Error&) {
  }
  return std::nullopt;
}

void RegionFile::write_chunk(int lx, int lz, const std::uint8_t* data, std::size_t len) {
  if (!open_) throw std::runtime_error("RegionFile: not open");
  if (lx < 0 || lx >= 32 || lz < 0 || lz >= 32) return;
  const std::vector<std::uint8_t> payload =
      nbt::zlib_compress(data, len);
  const int plen = static_cast<int>(payload.size());
  const int need = (plen + 5) / kSector + 1;
  if (need >= 256) return;  // mirrors the source: oversize chunks are skipped

  const int idx = index(lx, lz);
  const std::uint32_t cur = static_cast<std::uint32_t>(offsets_[idx]);
  int start = static_cast<int>((cur >> 8) & 0xFFFFFF);
  const int have = static_cast<int>(cur & 255);
  if (start != 0 && have == need) {
    // rewrite in place
  } else {
    for (int s = 0; s < have; ++s) free_[start + s] = true;
    // First-fit run scan from the first free sector (mirrors the source).
    int run_start = -1, run = 0, found = -1;
    for (std::size_t s = 0; s < free_.size(); ++s) {
      if (free_[s]) {
        if (run == 0) run_start = static_cast<int>(s);
        ++run;
      } else {
        run = 0;
      }
      if (run >= need) {
        found = run_start;
        break;
      }
    }
    if (found >= 0) {
      start = found;
      offsets_[idx] = static_cast<std::int32_t>((static_cast<std::uint32_t>(start) << 8) |
                                                static_cast<std::uint32_t>(need));
      for (int s = 0; s < need; ++s) free_[start + s] = false;
    } else {
      // grow
      start = static_cast<int>(free_.size());
      file_.insert(file_.end(), static_cast<std::size_t>(need) * kSector, 0);
      for (int s = 0; s < need; ++s) free_.push_back(false);
      offsets_[idx] = static_cast<std::int32_t>((static_cast<std::uint32_t>(start) << 8) |
                                                static_cast<std::uint32_t>(need));
    }
  }
  const long pos = static_cast<long>(start) * kSector;
  set_i32(file_, pos, plen + 1);
  file_[pos + 4] = 2;
  std::copy(payload.begin(), payload.end(), file_.begin() + pos + 5);
  timestamps_[idx] = static_cast<std::int32_t>(std::time(nullptr));
  dirty_ = true;
}

}  // namespace craftpp::world
