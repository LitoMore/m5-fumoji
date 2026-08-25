#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "fmj/byte_source.hpp"
#include "fmj/dat_lib.hpp"
#include "fmj/mono_canvas.hpp"
#include "wide_battle_state.hpp"
#include "wide_canvas.hpp"

class WideBattleRenderer {
 public:
  bool begin(const fmj::ByteSource& game);
  bool prepare(const fmj::ByteSource& game, const WideBattleState& state);
  bool render(const fmj::ByteSource& game, const WideBattleState& state,
              const fmj::MonoCanvas& source, WideCanvas& destination);

 private:
  struct Image {
    std::uint8_t width = 0;
    std::uint8_t height = 0;
    std::uint8_t mode = 0;
    std::size_t rowStride = 0;
    std::vector<std::uint8_t> pixels;
  };

  bool loadPicture(const fmj::ByteSource& game, std::uint8_t type,
                   std::uint8_t index, Image& image);
  bool loadBackground(const fmj::ByteSource& game,
                      const WideBattleState& state);
  static void draw(const Image& image, fmj::MonoCanvas& canvas, int x, int y);
  static void draw(const Image& image, WideCanvas& canvas, int x, int y);
  static bool imagePixel(const Image& image, int x, int y, bool& black);
  static int horizontalPeriod(const Image& image);
  static int verticalPeriod(const Image& image);

  fmj::DatLibIndex index_{};
  fmj::MonoCanvas baseBackground_{};
  WideCanvas wideBackground_{};
  Image base_{};
  Image topRight_{};
  Image bottomLeft_{};
  WideBattleState cachedState_{};
  int backgroundPeriodX_ = 0;
  int backgroundPeriodY_ = 0;
  bool cached_ = false;
};
