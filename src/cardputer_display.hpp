#pragma once

#include <array>
#include <cstdint>

#include "fmj/mono_canvas.hpp"

class CardputerDisplay {
 public:
  void begin();
  void present(const fmj::MonoCanvas& canvas);
  void adjustBrightness(std::int8_t delta, std::uint32_t nowMs);
  void update(std::uint32_t nowMs);

 private:
  static constexpr int kScaledWidth = 225;
  static constexpr int kScaledHeight = 135;
  static constexpr std::uint8_t kMinimumBrightness = 10;
  static constexpr std::uint8_t kMaximumBrightness = 100;
  static constexpr std::uint8_t kBrightnessStep = 10;

  void applyBrightness() const;
  void saveBrightness();

  std::array<std::uint16_t, kScaledWidth> scanline_{};
  std::uint8_t brightnessPercent_ = 70;
  std::uint32_t brightnessSaveAtMs_ = 0;
  bool brightnessDirty_ = false;
};
