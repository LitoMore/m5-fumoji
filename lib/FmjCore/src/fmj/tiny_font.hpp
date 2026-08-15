#pragma once

#include <string_view>

#include "fmj/mono_canvas.hpp"

namespace fmj {

class TinyFont {
 public:
  static constexpr int kGlyphWidth = 3;
  static constexpr int kGlyphHeight = 5;
  static constexpr int kAdvance = 4;

  static void drawChar(MonoCanvas& canvas, char value, int x, int y,
                       bool black = true, int scale = 1);
  static void drawText(MonoCanvas& canvas, std::string_view text, int x, int y,
                       bool black = true, int scale = 1);
  static int textWidth(std::string_view text, int scale = 1);
};

}  // namespace fmj

