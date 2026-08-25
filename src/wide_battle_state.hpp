#pragma once

#include <array>
#include <cstdint>

#include "fmj/mono_canvas.hpp"

struct WideBattleState {
  std::uint8_t background = 0;
  std::uint8_t topRight = 0;
  std::uint8_t bottomLeft = 0;
  std::array<std::uint8_t, fmj::MonoCanvas::kBufferSize> overlayMask{};
};
