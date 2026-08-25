#include "cardputer_display.hpp"

#include <M5Cardputer.h>
#include <Preferences.h>

#include <algorithm>

namespace {

constexpr char kPreferencesNamespace[] = "m5-fumoji";
constexpr char kBrightnessKey[] = "brightness";
constexpr char kDisplayModeKey[] = "display-mode";
constexpr std::uint32_t kSaveDelayMs = 1500U;

bool reached(std::uint32_t nowMs, std::uint32_t targetMs) {
  return static_cast<std::int32_t>(nowMs - targetMs) >= 0;
}

}  // namespace

void CardputerDisplay::begin() {
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.fillScreen(0x0000U);
  frame_.clear(true);

  Preferences preferences;
  if (preferences.begin(kPreferencesNamespace, true)) {
    brightnessPercent_ = preferences.getUChar(kBrightnessKey, 70);
    const auto storedMode = preferences.getUChar(
        kDisplayModeKey, static_cast<std::uint8_t>(Mode::WideMap));
    mode_ = storedMode == static_cast<std::uint8_t>(Mode::Scaled)
                ? Mode::Scaled
                : Mode::WideMap;
    preferences.end();
  }
  brightnessPercent_ = std::max(
      kMinimumBrightness,
      std::min(kMaximumBrightness, brightnessPercent_));
  applyBrightness();
}

void CardputerDisplay::present(const fmj::MonoCanvas& canvas) {
  const int left = (M5Cardputer.Display.width() - kScaledWidth) / 2;
  frame_.clear(true);
  M5Cardputer.Display.startWrite();
  for (int destinationY = 0; destinationY < kScaledHeight; ++destinationY) {
    scanline_.fill(0x0000U);
    const int sourceY = destinationY * fmj::MonoCanvas::kHeight / kScaledHeight;
    for (int destinationX = 0; destinationX < kScaledWidth; ++destinationX) {
      const int sourceX =
          destinationX * fmj::MonoCanvas::kWidth / kScaledWidth;
      scanline_[static_cast<std::size_t>(left + destinationX)] =
          canvas.pixel(sourceX, sourceY) ? 0x0000U : 0xFFFFU;
      frame_.setPixel(left + destinationX, destinationY,
                      canvas.pixel(sourceX, sourceY));
    }
    M5Cardputer.Display.pushImage(0, destinationY, WideCanvas::kWidth, 1,
                                  scanline_.data());
  }
  M5Cardputer.Display.endWrite();
}

void CardputerDisplay::present(const WideCanvas& canvas) {
  frame_ = canvas;
  M5Cardputer.Display.startWrite();
  for (int y = 0; y < WideCanvas::kHeight; ++y) {
    for (int x = 0; x < WideCanvas::kWidth; ++x) {
      scanline_[static_cast<std::size_t>(x)] =
          canvas.pixel(x, y) ? 0x0000U : 0xFFFFU;
    }
    M5Cardputer.Display.pushImage(0, y, WideCanvas::kWidth, 1,
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
    settingsDirty_ = true;
    settingsSaveAtMs_ = nowMs + kSaveDelayMs;
  }
}

void CardputerDisplay::toggleMode(std::uint32_t nowMs) {
  mode_ = mode_ == Mode::WideMap ? Mode::Scaled : Mode::WideMap;
  settingsDirty_ = true;
  settingsSaveAtMs_ = nowMs + kSaveDelayMs;
}

void CardputerDisplay::update(std::uint32_t nowMs) {
  if (settingsDirty_ && reached(nowMs, settingsSaveAtMs_)) {
    saveSettings();
  }
}

void CardputerDisplay::applyBrightness() const {
  const auto hardwareBrightness = static_cast<std::uint8_t>(
      (static_cast<std::uint16_t>(brightnessPercent_) * 255U + 50U) / 100U);
  M5Cardputer.Display.setBrightness(hardwareBrightness);
}

void CardputerDisplay::saveSettings() {
  Preferences preferences;
  if (preferences.begin(kPreferencesNamespace, false)) {
    preferences.putUChar(kBrightnessKey, brightnessPercent_);
    preferences.putUChar(kDisplayModeKey, static_cast<std::uint8_t>(mode_));
    preferences.end();
    settingsDirty_ = false;
  } else {
    settingsSaveAtMs_ = millis() + kSaveDelayMs;
  }
}
