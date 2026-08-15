#include "cardputer_display.hpp"

#include <M5Cardputer.h>
#include <Preferences.h>

#include <algorithm>

namespace {

constexpr char kPreferencesNamespace[] = "m5-fumoji";
constexpr char kBrightnessKey[] = "brightness";
constexpr std::uint32_t kSaveDelayMs = 1500U;

bool reached(std::uint32_t nowMs, std::uint32_t targetMs) {
  return static_cast<std::int32_t>(nowMs - targetMs) >= 0;
}

}  // namespace

void CardputerDisplay::begin() {
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.fillScreen(0x0000U);

  Preferences preferences;
  if (preferences.begin(kPreferencesNamespace, true)) {
    brightnessPercent_ = preferences.getUChar(kBrightnessKey, 70);
    preferences.end();
  }
  brightnessPercent_ = std::max(
      kMinimumBrightness,
      std::min(kMaximumBrightness, brightnessPercent_));
  applyBrightness();
}

void CardputerDisplay::present(const fmj::MonoCanvas& canvas) {
  const int left = (M5Cardputer.Display.width() - kScaledWidth) / 2;
  M5Cardputer.Display.startWrite();
  for (int destinationY = 0; destinationY < kScaledHeight; ++destinationY) {
    const int sourceY = destinationY * fmj::MonoCanvas::kHeight / kScaledHeight;
    for (int destinationX = 0; destinationX < kScaledWidth; ++destinationX) {
      const int sourceX =
          destinationX * fmj::MonoCanvas::kWidth / kScaledWidth;
      scanline_[static_cast<std::size_t>(destinationX)] =
          canvas.pixel(sourceX, sourceY) ? 0x0000U : 0xFFFFU;
    }
    M5Cardputer.Display.pushImage(left, destinationY, kScaledWidth, 1,
                                  scanline_.data());
  }
  M5Cardputer.Display.endWrite();
}

void CardputerDisplay::adjustBrightness(std::int8_t delta,
                                        std::uint32_t nowMs) {
  const int adjusted = static_cast<int>(brightnessPercent_) +
                       static_cast<int>(delta) * kBrightnessStep;
  const auto next = static_cast<std::uint8_t>(
      std::max<int>(kMinimumBrightness,
                    std::min<int>(kMaximumBrightness, adjusted)));
  if (next != brightnessPercent_) {
    brightnessPercent_ = next;
    applyBrightness();
    brightnessDirty_ = true;
    brightnessSaveAtMs_ = nowMs + kSaveDelayMs;
  }
}

void CardputerDisplay::update(std::uint32_t nowMs) {
  if (brightnessDirty_ && reached(nowMs, brightnessSaveAtMs_)) {
    saveBrightness();
  }
}

void CardputerDisplay::applyBrightness() const {
  const auto hardwareBrightness = static_cast<std::uint8_t>(
      (static_cast<std::uint16_t>(brightnessPercent_) * 255U + 50U) / 100U);
  M5Cardputer.Display.setBrightness(hardwareBrightness);
}

void CardputerDisplay::saveBrightness() {
  Preferences preferences;
  if (preferences.begin(kPreferencesNamespace, false)) {
    preferences.putUChar(kBrightnessKey, brightnessPercent_);
    preferences.end();
    brightnessDirty_ = false;
  } else {
    brightnessSaveAtMs_ = millis() + kSaveDelayMs;
  }
}
