#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace craftpp::nbt {

// Wire-compatible port of NBTBase/NBTTag*/CompressedStreamTools.
//
// Binary layout (big-endian, DataInput/DataOutput semantics):
//   tag      := type:u8 + (name:utf + payload)?      -- no payload for TAG_End
//   utf      := len:u16 + modified-UTF-8 bytes       -- mirrors writeUTF/readUTF
//   compound := tag* + 0x00
//   list     := elem_type:u8 + count:i32 + payload*
// Root must be a named TAG_Compound. Gzip helpers mirror
// CompressedStreamTools (level.dat is gzipped NBT).

enum class TagType : std::uint8_t {
  End = 0,
  Byte = 1,
  Short = 2,
  Int = 3,
  Long = 4,
  Float = 5,
  Double = 6,
  ByteArray = 7,
  String = 8,
  List = 9,
  Compound = 10,
};

struct Tag;
struct TagList {
  TagType element = TagType::End;
  std::vector<Tag> items;
};

// Ordered compound: preserves file order so a decode -> encode round trip is
// byte-identical. (The source used a HashMap with unspecified order; we
// deliberately do better while staying wire-compatible.)
struct TagCompound {
  std::vector<std::pair<std::string, Tag>> entries;

  const Tag* find(const std::string& key) const;
  Tag* find(const std::string& key);
  void set(std::string key, Tag tag);
  std::size_t size() const { return entries.size(); }
};

struct Tag {
  TagType type = TagType::End;
  // Payload by type: Byte->i8, Short->i16, Int->i32, Long->i64,
  // Float->float, Double->double, ByteArray->bytes, String->str,
  // List->list, Compound->compound, End->monostate.
  std::int8_t i8 = 0;
  std::int16_t i16 = 0;
  std::int32_t i32 = 0;
  std::int64_t i64 = 0;
  float f32 = 0.0F;
  double f64 = 0.0;
  std::vector<std::int8_t> bytes;
  std::string str;
  std::shared_ptr<TagList> list;
  std::shared_ptr<TagCompound> compound;

  static Tag make_end() { return Tag{}; }
  static Tag make_byte(std::int8_t v) { Tag t; t.type = TagType::Byte; t.i8 = v; return t; }
  static Tag make_short(std::int16_t v) { Tag t; t.type = TagType::Short; t.i16 = v; return t; }
  static Tag make_int(std::int32_t v) { Tag t; t.type = TagType::Int; t.i32 = v; return t; }
  static Tag make_long(std::int64_t v) { Tag t; t.type = TagType::Long; t.i64 = v; return t; }
  static Tag make_float(float v) { Tag t; t.type = TagType::Float; t.f32 = v; return t; }
  static Tag make_double(double v) { Tag t; t.type = TagType::Double; t.f64 = v; return t; }
  static Tag make_byte_array(std::vector<std::int8_t> v) {
    Tag t;
    t.type = TagType::ByteArray;
    t.bytes = std::move(v);
    return t;
  }
  static Tag make_string(std::string v) {
    if (v.empty()) throw std::invalid_argument("empty string not allowed (mirrors NBTTagString)");
    Tag t;
    t.type = TagType::String;
    t.str = std::move(v);
    return t;
  }
  static Tag make_list(TagType element, std::vector<Tag> items = {}) {
    Tag t;
    t.type = TagType::List;
    t.list = std::make_shared<TagList>(TagList{element, std::move(items)});
    return t;
  }
  static Tag make_compound(TagCompound c = {}) {
    Tag t;
    t.type = TagType::Compound;
    t.compound = std::make_shared<TagCompound>(std::move(c));
    return t;
  }
};

class Error : public std::runtime_error {
 public:
  explicit Error(const std::string& msg) : std::runtime_error(msg) {}
};

// Big-endian cursor reader over a byte buffer.
class Reader {
 public:
  Reader(const std::uint8_t* data, std::size_t len) : data_(data), len_(len) {}

  std::uint8_t u8();
  std::uint16_t u16();
  std::int16_t i16() { return static_cast<std::int16_t>(u16()); }
  std::int32_t i32();
  std::int64_t i64();
  float f32();
  double f64();
  std::string utf();  // modified UTF-8, mirrors DataInput.readUTF
  std::size_t remaining() const { return len_ - pos_; }

 private:
  const std::uint8_t* data_;
  std::size_t len_;
  std::size_t pos_ = 0;
};

class Writer {
 public:
  void u8(std::uint8_t v) { buf_.push_back(v); }
  void u16(std::uint16_t v) {
    buf_.push_back(static_cast<std::uint8_t>(v >> 8));
    buf_.push_back(static_cast<std::uint8_t>(v & 0xFF));
  }
  void i16(std::int16_t v) { u16(static_cast<std::uint16_t>(v)); }
  void i32(std::int32_t v);
  void i64(std::int64_t v);
  void f32(float v);
  void f64(double v);
  void utf(const std::string& s);  // modified UTF-8, mirrors DataOutput.writeUTF
  void raw(const std::uint8_t* data, std::size_t len) { buf_.insert(buf_.end(), data, data + len); }

  const std::vector<std::uint8_t>& bytes() const { return buf_; }

 private:
  std::vector<std::uint8_t> buf_;
};

// Named-tag framing (mirrors NBTBase.readTag/writeTag).
Tag read_named(Reader& r);
void write_named(Writer& w, const std::string& name, const Tag& tag);
Tag read_root(Reader& r);  // throws unless root is a named compound
void write_root(Writer& w, const std::string& name, const Tag& compound);

// Gzip container (mirrors CompressedStreamTools gzip methods).
std::vector<std::uint8_t> gzip_compress(const std::uint8_t* data, std::size_t len);
std::vector<std::uint8_t> gzip_decompress(const std::uint8_t* data, std::size_t len);

}  // namespace craftpp::nbt
