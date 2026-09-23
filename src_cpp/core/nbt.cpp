#include "core/nbt.hpp"

#include <cstring>
#include <zlib.h>

namespace craftpp::nbt {

const Tag* TagCompound::find(const std::string& key) const {
  for (const auto& [k, v] : entries) {
    if (k == key) return &v;
  }
  return nullptr;
}

Tag* TagCompound::find(const std::string& key) {
  for (auto& [k, v] : entries) {
    if (k == key) return &v;
  }
  return nullptr;
}

void TagCompound::set(std::string key, Tag tag) {
  if (Tag* t = find(key)) {
    *t = std::move(tag);
    return;
  }
  entries.emplace_back(std::move(key), std::move(tag));
}

std::uint8_t Reader::u8() {
  if (pos_ >= len_) throw Error("nbt: unexpected end of buffer");
  return data_[pos_++];
}

std::uint16_t Reader::u16() {
  const auto hi = static_cast<std::uint16_t>(u8());
  const auto lo = static_cast<std::uint16_t>(u8());
  return static_cast<std::uint16_t>((hi << 8) | lo);
}

std::int32_t Reader::i32() {
  std::uint32_t v = 0;
  for (int i = 0; i < 4; ++i) v = (v << 8) | u8();
  return static_cast<std::int32_t>(v);
}

std::int64_t Reader::i64() {
  std::uint64_t v = 0;
  for (int i = 0; i < 8; ++i) v = (v << 8) | u8();
  return static_cast<std::int64_t>(v);
}

float Reader::f32() {
  std::int32_t bits = i32();
  float f = 0.0F;
  std::memcpy(&f, &bits, sizeof(f));
  return f;
}

double Reader::f64() {
  std::int64_t bits = i64();
  double d = 0.0;
  std::memcpy(&d, &bits, sizeof(d));
  return d;
}

namespace {

// Modified UTF-8 (as used by Java writeUTF/readUTF):
// - U+0000 encodes as 0xC0 0x80 (never a literal zero byte)
// - BMP chars encode as 1/2/3 bytes (CESU-8 style)
// Decode to standard UTF-8 in std::string.
void append_utf8(std::string& out, std::uint32_t cp) {
  if (cp < 0x80) {
    out.push_back(static_cast<char>(cp));
  } else if (cp < 0x800) {
    out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else if (cp < 0x10000) {
    out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else {
    out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  }
}

// Decode one UTF-8 code point from `s` at `i`, advancing `i`.
std::uint32_t decode_utf8_char(const std::string& s, std::size_t& i) {
  const auto c0 = static_cast<std::uint8_t>(s[i]);
  if (c0 < 0x80) {
    ++i;
    return c0;
  }
  if ((c0 & 0xE0) == 0xC0) {
    if (i + 1 >= s.size()) throw Error("nbt: truncated utf-8");
    const auto c1 = static_cast<std::uint8_t>(s[i + 1]);
    i += 2;
    return (static_cast<std::uint32_t>(c0 & 0x1F) << 6) | (c1 & 0x3F);
  }
  if ((c0 & 0xF0) == 0xE0) {
    if (i + 2 >= s.size()) throw Error("nbt: truncated utf-8");
    const auto c1 = static_cast<std::uint8_t>(s[i + 1]);
    const auto c2 = static_cast<std::uint8_t>(s[i + 2]);
    i += 3;
    return (static_cast<std::uint32_t>(c0 & 0x0F) << 12) | ((c1 & 0x3F) << 6) | (c2 & 0x3F);
  }
  if (i + 3 >= s.size()) throw Error("nbt: truncated utf-8");
  const auto c1 = static_cast<std::uint8_t>(s[i + 1]);
  const auto c2 = static_cast<std::uint8_t>(s[i + 2]);
  const auto c3 = static_cast<std::uint8_t>(s[i + 3]);
  i += 4;
  return (static_cast<std::uint32_t>(c0 & 0x07) << 18) | ((c1 & 0x3F) << 12) |
         ((c2 & 0x3F) << 6) | (c3 & 0x3F);
}

}  // namespace

std::string Reader::utf() {
  const std::uint16_t byte_len = u16();
  if (remaining() < byte_len) throw Error("nbt: truncated utf string");
  std::string out;
  out.reserve(byte_len);
  std::size_t end = pos_ + byte_len;
  while (pos_ < end) {
    const auto c = data_[pos_];
    std::uint32_t cp = 0;
    if (c < 0x80) {
      cp = c;
      ++pos_;
    } else if ((c & 0xE0) == 0xC0) {
      if (pos_ + 1 >= end) throw Error("nbt: truncated mutf-8");
      cp = ((c & 0x1F) << 6) | (data_[pos_ + 1] & 0x3F);
      pos_ += 2;
    } else if ((c & 0xF0) == 0xE0) {
      if (pos_ + 2 >= end) throw Error("nbt: truncated mutf-8");
      cp = ((c & 0x0F) << 12) | ((data_[pos_ + 1] & 0x3F) << 6) | (data_[pos_ + 2] & 0x3F);
      pos_ += 3;
    } else {
      throw Error("nbt: unsupported mutf-8 sequence (writeUTF never emits 4-byte forms)");
    }
    append_utf8(out, cp);
  }
  return out;
}

void Writer::i32(std::int32_t v) {
  const auto u = static_cast<std::uint32_t>(v);
  buf_.push_back(static_cast<std::uint8_t>(u >> 24));
  buf_.push_back(static_cast<std::uint8_t>((u >> 16) & 0xFF));
  buf_.push_back(static_cast<std::uint8_t>((u >> 8) & 0xFF));
  buf_.push_back(static_cast<std::uint8_t>(u & 0xFF));
}

void Writer::i64(std::int64_t v) {
  const auto u = static_cast<std::uint64_t>(v);
  for (int i = 7; i >= 0; --i) buf_.push_back(static_cast<std::uint8_t>((u >> (i * 8)) & 0xFF));
}

void Writer::f32(float v) {
  std::int32_t bits = 0;
  std::memcpy(&bits, &v, sizeof(bits));
  i32(bits);
}

void Writer::f64(double v) {
  std::int64_t bits = 0;
  std::memcpy(&bits, &v, sizeof(bits));
  i64(bits);
}

void Writer::utf(const std::string& s) {
  // Encode as modified UTF-8; length prefix counts *encoded bytes*.
  std::string enc;
  enc.reserve(s.size());
  std::size_t i = 0;
  while (i < s.size()) {
    const std::uint32_t cp = decode_utf8_char(s, i);
    if (cp == 0) {
      enc.push_back(static_cast<char>(0xC0));
      enc.push_back(static_cast<char>(0x80));
    } else if (cp < 0x80) {
      enc.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
      enc.push_back(static_cast<char>(0xC0 | (cp >> 6)));
      enc.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
      enc.push_back(static_cast<char>(0xE0 | (cp >> 12)));
      enc.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
      enc.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
      // writeUTF has no 4-byte form; Java would emit a surrogate pair
      // (6 bytes). Re-encode as CESU-8 surrogate pair for wire parity.
      const std::uint32_t v = cp - 0x10000;
      const std::uint32_t hi = 0xD800 + (v >> 10);
      const std::uint32_t lo = 0xDC00 + (v & 0x3FF);
      for (std::uint32_t su : {hi, lo}) {
        enc.push_back(static_cast<char>(0xE0 | (su >> 12)));
        enc.push_back(static_cast<char>(0x80 | ((su >> 6) & 0x3F)));
        enc.push_back(static_cast<char>(0x80 | (su & 0x3F)));
      }
    }
  }
  if (enc.size() > 0xFFFF) throw Error("nbt: utf string too long for writeUTF");
  u16(static_cast<std::uint16_t>(enc.size()));
  raw(reinterpret_cast<const std::uint8_t*>(enc.data()), enc.size());
}

namespace {

TagType read_type(Reader& r) {
  const std::uint8_t b = r.u8();
  if (b > 10) throw Error("nbt: unknown tag type");
  return static_cast<TagType>(b);
}

Tag read_payload(Reader& r, TagType type) {
  Tag t;
  t.type = type;
  switch (type) {
    case TagType::End:
      break;
    case TagType::Byte:
      t.i8 = static_cast<std::int8_t>(r.u8());
      break;
    case TagType::Short:
      t.i16 = r.i16();
      break;
    case TagType::Int:
      t.i32 = r.i32();
      break;
    case TagType::Long:
      t.i64 = r.i64();
      break;
    case TagType::Float:
      t.f32 = r.f32();
      break;
    case TagType::Double:
      t.f64 = r.f64();
      break;
    case TagType::ByteArray: {
      const std::int32_t n = r.i32();
      if (n < 0 || static_cast<std::size_t>(n) > r.remaining()) throw Error("nbt: bad byte array length");
      t.bytes.resize(static_cast<std::size_t>(n));
      for (std::int32_t i = 0; i < n; ++i) t.bytes[static_cast<std::size_t>(i)] = static_cast<std::int8_t>(r.u8());
      break;
    }
    case TagType::String:
      t.str = r.utf();
      break;
    case TagType::List: {
      const TagType elem = read_type(r);
      const std::int32_t n = r.i32();
      if (n < 0) throw Error("nbt: negative list length");
      auto list = std::make_shared<TagList>();
      list->element = elem;
      list->items.reserve(static_cast<std::size_t>(n));
      for (std::int32_t i = 0; i < n; ++i) list->items.push_back(read_payload(r, elem));
      t.list = std::move(list);
      break;
    }
    case TagType::Compound: {
      auto comp = std::make_shared<TagCompound>();
      while (true) {
        const TagType et = read_type(r);
        if (et == TagType::End) break;
        const std::string name = r.utf();
        comp->set(name, read_payload(r, et));
      }
      t.compound = std::move(comp);
      break;
    }
  }
  return t;
}

void write_payload(Writer& w, const Tag& t) {
  switch (t.type) {
    case TagType::End:
      break;
    case TagType::Byte:
      w.u8(static_cast<std::uint8_t>(t.i8));
      break;
    case TagType::Short:
      w.i16(t.i16);
      break;
    case TagType::Int:
      w.i32(t.i32);
      break;
    case TagType::Long:
      w.i64(t.i64);
      break;
    case TagType::Float:
      w.f32(t.f32);
      break;
    case TagType::Double:
      w.f64(t.f64);
      break;
    case TagType::ByteArray:
      w.i32(static_cast<std::int32_t>(t.bytes.size()));
      for (std::int8_t b : t.bytes) w.u8(static_cast<std::uint8_t>(b));
      break;
    case TagType::String:
      w.utf(t.str);
      break;
    case TagType::List: {
      if (!t.list) throw Error("nbt: null list");
      // Mirrors NBTTagList.writeTagContents: empty list declares type 1.
      const TagType elem = t.list->items.empty() ? TagType::Byte : t.list->element;
      w.u8(static_cast<std::uint8_t>(elem));
      w.i32(static_cast<std::int32_t>(t.list->items.size()));
      for (const Tag& item : t.list->items) write_payload(w, item);
      break;
    }
    case TagType::Compound: {
      if (!t.compound) throw Error("nbt: null compound");
      for (const auto& [name, child] : t.compound->entries) write_named(w, name, child);
      w.u8(0);
      break;
    }
  }
}

}  // namespace

Tag read_named(Reader& r) {
  const TagType type = read_type(r);
  if (type == TagType::End) return Tag::make_end();
  r.utf();  // name is framing only; Tag carries no key (see set())
  return read_payload(r, type);
}

void write_named(Writer& w, const std::string& name, const Tag& tag) {
  w.u8(static_cast<std::uint8_t>(tag.type));
  if (tag.type == TagType::End) return;
  w.utf(name);
  write_payload(w, tag);
}

Tag read_root(Reader& r) {
  Tag root = read_named(r);
  if (root.type != TagType::Compound) throw Error("nbt: root tag must be a named compound tag");
  return root;
}

void write_root(Writer& w, const std::string& name, const Tag& compound) {
  if (compound.type != TagType::Compound) throw Error("nbt: root tag must be a named compound tag");
  write_named(w, name, compound);
}

std::vector<std::uint8_t> gzip_compress(const std::uint8_t* data, std::size_t len) {
  std::vector<std::uint8_t> out;
  z_stream strm{};
  if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
    throw Error("nbt: deflateInit2 failed");
  }
  strm.next_in = const_cast<Bytef*>(data);
  strm.avail_in = static_cast<uInt>(len);
  std::uint8_t chunk[32768];
  int ret = Z_OK;
  while (ret == Z_OK) {
    strm.next_out = chunk;
    strm.avail_out = sizeof(chunk);
    ret = deflate(&strm, Z_FINISH);
    out.insert(out.end(), chunk, chunk + (sizeof(chunk) - strm.avail_out));
  }
  deflateEnd(&strm);
  if (ret != Z_STREAM_END) throw Error("nbt: gzip compress failed");
  return out;
}

std::vector<std::uint8_t> gzip_decompress(const std::uint8_t* data, std::size_t len) {
  std::vector<std::uint8_t> out;
  z_stream strm{};
  strm.next_in = const_cast<Bytef*>(data);
  strm.avail_in = static_cast<uInt>(len);
  if (inflateInit2(&strm, 15 + 32) != Z_OK) throw Error("nbt: inflateInit failed");
  std::uint8_t chunk[32768];
  int ret = Z_OK;
  while (ret == Z_OK) {
    strm.next_out = chunk;
    strm.avail_out = sizeof(chunk);
    ret = inflate(&strm, Z_NO_FLUSH);
    if (ret != Z_OK && ret != Z_STREAM_END) {
      inflateEnd(&strm);
      throw Error("nbt: gzip decompress failed");
    }
    out.insert(out.end(), chunk, chunk + (sizeof(chunk) - strm.avail_out));
  }
  inflateEnd(&strm);
  return out;
}

}  // namespace craftpp::nbt
