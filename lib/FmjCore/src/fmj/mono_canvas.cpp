#include "fmj/mono_canvas.hpp"

#include <algorithm>
#include <cstdlib>

namespace fmj {

void MonoCanvas::clear(bool black) {
  pixels_.fill(black ? 0xFFU : 0x00U);
}

void MonoCanvas::setPixel(int x, int y, bool black) {
  if (x < 0 || x >= kWidth || y < 0 || y >= kHeight) {
    return;
  }
  const auto bit = static_cast<std::size_t>(y * kWidth + x);
  const auto mask = static_cast<std::uint8_t>(0x80U >> (bit & 7U));
  auto& byte = pixels_[bit >> 3U];
  if (black) {
    byte = static_cast<std::uint8_t>(byte | mask);
  } else {
    byte = static_cast<std::uint8_t>(byte & static_cast<std::uint8_t>(~mask));
  }
}

bool MonoCanvas::pixel(int x, int y) const {
  if (x < 0 || x >= kWidth || y < 0 || y >= kHeight) {
    return false;
  }
  const auto bit = static_cast<std::size_t>(y * kWidth + x);
  return (pixels_[bit >> 3U] & static_cast<std::uint8_t>(0x80U >> (bit & 7U))) != 0;
}

void MonoCanvas::hLine(int x, int y, int width, bool black) {
  if (width <= 0 || y < 0 || y >= kHeight) return;
  const int start = std::max(0, x);
  const int end = std::min(kWidth, x + width);
  for (int px = start; px < end; ++px) setPixel(px, y, black);
}

void MonoCanvas::vLine(int x, int y, int height, bool black) {
  if (height <= 0 || x < 0 || x >= kWidth) return;
  const int start = std::max(0, y);
  const int end = std::min(kHeight, y + height);
  for (int py = start; py < end; ++py) setPixel(x, py, black);
}

void MonoCanvas::rect(int x, int y, int width, int height, bool black) {
  if (width <= 0 || height <= 0) return;
  hLine(x, y, width, black);
  hLine(x, y + height - 1, width, black);
  vLine(x, y, height, black);
  vLine(x + width - 1, y, height, black);
}

void MonoCanvas::fillRect(int x, int y, int width, int height, bool black) {
  if (width <= 0 || height <= 0) return;
  const int start = std::max(0, y);
  const int end = std::min(kHeight, y + height);
  for (int py = start; py < end; ++py) hLine(x, py, width, black);
}

void MonoCanvas::line(int x0, int y0, int x1, int y1, bool black) {
  const int dx = std::abs(x1 - x0);
  const int sx = x0 < x1 ? 1 : -1;
  const int dy = -std::abs(y1 - y0);
  const int sy = y0 < y1 ? 1 : -1;
  int error = dx + dy;
  while (true) {
    setPixel(x0, y0, black);
    if (x0 == x1 && y0 == y1) break;
    const int twice = error * 2;
    if (twice >= dy) {
      error += dy;
      x0 += sx;
    }
    if (twice <= dx) {
      error += dx;
      y0 += sy;
    }
  }
}

}  // namespace fmj

