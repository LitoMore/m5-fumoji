#pragma once

#include <array>
#include <cstdint>

#include "fmj/mono_canvas.hpp"
#include "wide_canvas.hpp"

class CardputerDisplay {
 public:
  enum class Mode : std::uint8_t { Scaled = 0, WideMap = 1 };

  void begin();
  void present(const fmj::MonoCanvas& canvas);
  void present(const WideCanvas& canvas);
  void adjustBrightness(std::int8_t delta, std::uint32_t nowMs);
  void toggleMode(std::uint32_t nowMs);
  void update(std::uint32_t nowMs);
  Mode mode() const { return mode_; }
  const WideCanvas& frame() const { return frame_; }

 private:
  static constexpr int kScaledWidth = 225;
  static constexpr int kScaledHeight = 135;
  static constexpr std::uint8_t kMinimumBrightness = 10;
  static constexpr std::uint8_t kMaximumBrightness = 100;
  static constexpr std::uint8_t kBrightnessStep = 10;

  void applyBrightness() const;
  void saveSettings();

  std::array<std::uint16_t, WideCanvas::kWidth> scanline_{};
  WideCanvas frame_{};
  std::uint8_t brightnessPercent_ = 70;
  Mode mode_ = Mode::WideMap;
  std::uint32_t settingsSaveAtMs_ = 0;
  bool settingsDirty_ = false;
};
