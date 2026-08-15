#pragma once

#include <cstddef>
#include <cstdint>

#include "fmj/byte_source.hpp"
#include "fmj/mono_canvas.hpp"

namespace fmj {

class ImageResource {
 public:
  bool open(const ByteSource& source, std::size_t offset);
  bool drawSlice(MonoCanvas& canvas, std::uint8_t slice, int x, int y) const;

  std::uint8_t type() const { return type_; }
  std::uint8_t index() const { return index_; }
  std::uint8_t width() const { return width_; }
  std::uint8_t height() const { return height_; }
  std::uint8_t sliceCount() const { return sliceCount_; }
  bool transparent() const { return mode_ == 2U; }
  bool valid() const { return source_ != nullptr; }
  std::size_t serializedSize() const;

 private:
  std::size_t rowStride() const;

  const ByteSource* source_ = nullptr;
  std::size_t dataOffset_ = 0;
  std::uint8_t type_ = 0;
  std::uint8_t index_ = 0;
  std::uint8_t width_ = 0;
  std::uint8_t height_ = 0;
  std::uint8_t sliceCount_ = 0;
  std::uint8_t mode_ = 0;
};

}  // namespace fmj
