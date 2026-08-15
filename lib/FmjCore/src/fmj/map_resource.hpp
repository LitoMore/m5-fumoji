#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "fmj/byte_source.hpp"

namespace fmj {

class MapResource {
 public:
  bool open(const ByteSource& source, std::size_t offset);
  bool tile(int x, int y, std::uint8_t& tileIndex, bool& walkable,
            std::uint8_t& event) const;

  std::uint8_t type() const { return type_; }
  std::uint8_t index() const { return index_; }
  std::uint8_t tileSetIndex() const { return tileSetIndex_; }
  std::uint8_t width() const { return width_; }
  std::uint8_t height() const { return height_; }
  const char* nameBytes() const { return name_.data(); }
  bool valid() const { return source_ != nullptr; }

 private:
  const ByteSource* source_ = nullptr;
  std::size_t dataOffset_ = 0;
  std::uint8_t type_ = 0;
  std::uint8_t index_ = 0;
  std::uint8_t tileSetIndex_ = 0;
  std::uint8_t width_ = 0;
  std::uint8_t height_ = 0;
  std::array<char, 14> name_{};
};

}  // namespace fmj

