#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace fmj {

class MonoCanvas {
 public:
  static constexpr int kWidth = 160;
  static constexpr int kHeight = 96;
  static constexpr std::size_t kBufferSize =
      static_cast<std::size_t>(kWidth * kHeight / 8);

  void clear(bool black = false);
  void setPixel(int x, int y, bool black = true);
  bool pixel(int x, int y) const;
  void hLine(int x, int y, int width, bool black = true);
  void vLine(int x, int y, int height, bool black = true);
  void rect(int x, int y, int width, int height, bool black = true);
  void fillRect(int x, int y, int width, int height, bool black = true);
  void line(int x0, int y0, int x1, int y1, bool black = true);

  const std::uint8_t* data() const { return pixels_.data(); }
  std::uint8_t* data() { return pixels_.data(); }

 private:
  std::array<std::uint8_t, kBufferSize> pixels_{};
};

}  // namespace fmj

