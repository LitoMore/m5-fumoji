#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

class WideCanvas {
 public:
  static constexpr int kWidth = 240;
  static constexpr int kHeight = 135;
  static constexpr int kRowBytes = kWidth / 8;
  static constexpr std::size_t kBufferSize =
      static_cast<std::size_t>(kRowBytes * kHeight);

  void clear(bool black = false) {
    pixels_.fill(black ? 0xFFU : 0x00U);
  }

  void setPixel(int x, int y, bool black) {
    if (x < 0 || y < 0 || x >= kWidth || y >= kHeight) return;
    auto& packed = pixels_[static_cast<std::size_t>(y * kRowBytes + x / 8)];
    const auto mask = static_cast<std::uint8_t>(0x80U >> (x & 7));
    if (black) {
      packed |= mask;
    } else {
      packed &= static_cast<std::uint8_t>(~mask);
    }
  }

  bool pixel(int x, int y) const {
    if (x < 0 || y < 0 || x >= kWidth || y >= kHeight) return false;
    const auto packed =
        pixels_[static_cast<std::size_t>(y * kRowBytes + x / 8)];
    return (packed & static_cast<std::uint8_t>(0x80U >> (x & 7))) != 0U;
  }

  const std::uint8_t* data() const { return pixels_.data(); }
  std::uint8_t* data() { return pixels_.data(); }

 private:
  std::array<std::uint8_t, kBufferSize> pixels_{};
};
