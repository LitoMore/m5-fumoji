#include "fmj/bbk_font.hpp"

#include <array>

namespace fmj {

bool BbkFont::open(const ByteSource* hzk16, const ByteSource* asc16) {
  hzk16_ = nullptr;
  asc16_ = nullptr;
  if (hzk16 == nullptr || asc16 == nullptr || hzk16->size() < 32U ||
      asc16->size() < 16U) {
    return false;
  }
  hzk16_ = hzk16;
  asc16_ = asc16;
  return true;
}

int BbkFont::drawBytes(MonoCanvas& canvas, const std::uint8_t* bytes,
                       std::size_t length, int x, int y, bool inverted) const {
  if (!valid() || bytes == nullptr) return x;
  std::size_t cursor = 0;
  while (cursor < length && bytes[cursor] != 0U) {
    const auto first = bytes[cursor];
    if (first >= 0xA1U && cursor + 1U < length && bytes[cursor + 1U] >= 0xA1U) {
      const auto second = bytes[cursor + 1U];
      const auto glyph = 94U * static_cast<std::size_t>(first - 0xA1U) +
                         static_cast<std::size_t>(second - 0xA1U);
      drawGlyph(canvas, *hzk16_, glyph * 32U, 16, x, y, inverted);
      x += 16;
      cursor += 2U;
    } else if (first < 128U) {
      drawGlyph(canvas, *asc16_, static_cast<std::size_t>(first) * 16U, 8, x,
                y, inverted);
      x += 8;
      ++cursor;
    } else {
      x += 8;
      ++cursor;
    }
  }
  return x;
}

bool BbkFont::drawGlyph(MonoCanvas& canvas, const ByteSource& source,
                        std::size_t offset, int width, int x, int y,
                        bool inverted) const {
  std::array<std::uint8_t, 32> glyph{};
  const auto length = static_cast<std::size_t>(width / 8 * 16);
  if (!source.read(offset, glyph.data(), length)) return false;
  for (int row = 0; row < 16; ++row) {
    for (int column = 0; column < width; ++column) {
      const auto byte = glyph[static_cast<std::size_t>(row * (width / 8) +
                                                       column / 8)];
      const bool ink = (byte & (0x80U >> (column & 7))) != 0U;
      canvas.setPixel(x + column, y + row, inverted ? !ink : ink);
    }
  }
  return true;
}

}  // namespace fmj
