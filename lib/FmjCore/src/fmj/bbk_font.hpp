#pragma once

#include <cstddef>
#include <cstdint>

#include "fmj/byte_source.hpp"
#include "fmj/mono_canvas.hpp"

namespace fmj {

class BbkFont {
 public:
  bool open(const ByteSource* hzk16, const ByteSource* asc16);
  int drawBytes(MonoCanvas& canvas, const std::uint8_t* bytes,
                std::size_t length, int x, int y, bool inverted = false) const;
  bool valid() const { return hzk16_ != nullptr && asc16_ != nullptr; }

 private:
  bool drawGlyph(MonoCanvas& canvas, const ByteSource& source,
                 std::size_t offset, int width, int x, int y,
                 bool inverted) const;

  const ByteSource* hzk16_ = nullptr;
  const ByteSource* asc16_ = nullptr;
};

}  // namespace fmj

