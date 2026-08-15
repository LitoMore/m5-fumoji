#include "fmj/tiny_font.hpp"

#include <cctype>
#include <cstdint>

namespace fmj {
namespace {

struct Glyph {
  char value;
  std::uint16_t bits;
};

// Three pixels wide, five pixels high. Bit 14 is the upper-left pixel.
constexpr Glyph kGlyphs[] = {
    {' ', 0x0000}, {'-', 0x01C0}, {'.', 0x0001}, {'/', 0x0924},
    {':', 0x0404}, {'>', 0x4488}, {'0', 0x7B6F}, {'1', 0x2492},
    {'2', 0x73E7}, {'3', 0x73CF}, {'4', 0x5BC9}, {'5', 0x79CF},
    {'6', 0x79EF}, {'7', 0x7249}, {'8', 0x7BEF}, {'9', 0x7BCF},
    {'A', 0x2BED}, {'B', 0x6BAE}, {'C', 0x3927}, {'D', 0x6B6E},
    {'E', 0x79A7}, {'F', 0x79A4}, {'G', 0x39EB}, {'H', 0x5BED},
    {'I', 0x7497}, {'J', 0x124E}, {'K', 0x5AAD}, {'L', 0x4927},
    {'M', 0x5FED}, {'N', 0x5FAD}, {'O', 0x3B6E}, {'P', 0x6BE4},
    {'Q', 0x3B7F}, {'R', 0x6BED}, {'S', 0x39CF}, {'T', 0x7492},
    {'U', 0x5B6F}, {'V', 0x5B6A}, {'W', 0x5BFD}, {'X', 0x5AAD},
    {'Y', 0x5A92}, {'Z', 0x72A7}, {'?', 0x7282},
};

std::uint16_t glyphBits(char value) {
  const auto upper = static_cast<char>(
      std::toupper(static_cast<unsigned char>(value)));
  for (const auto& glyph : kGlyphs) {
    if (glyph.value == upper) return glyph.bits;
  }
  return 0x7282;
}

}  // namespace

void TinyFont::drawChar(MonoCanvas& canvas, char value, int x, int y,
                        bool black, int scale) {
  if (scale < 1) scale = 1;
  const auto bits = glyphBits(value);
  for (int row = 0; row < kGlyphHeight; ++row) {
    for (int column = 0; column < kGlyphWidth; ++column) {
      const int bit = 14 - (row * kGlyphWidth + column);
      if ((bits & static_cast<std::uint16_t>(1U << bit)) != 0) {
        canvas.fillRect(x + column * scale, y + row * scale, scale, scale,
                        black);
      }
    }
  }
}

void TinyFont::drawText(MonoCanvas& canvas, std::string_view text, int x, int y,
                        bool black, int scale) {
  for (const char value : text) {
    drawChar(canvas, value, x, y, black, scale);
    x += kAdvance * scale;
  }
}

int TinyFont::textWidth(std::string_view text, int scale) {
  if (text.empty()) return 0;
  return (static_cast<int>(text.size()) * kAdvance - 1) * scale;
}

}  // namespace fmj

