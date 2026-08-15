#pragma once

#include <cstdint>

#include "fmj/firmware_app.hpp"

class CardputerInput {
 public:
  fmj::InputKey poll(std::uint32_t nowMs);
  std::int8_t takeBrightnessDelta();

 private:
  static bool repeatable(fmj::InputKey key);
  fmj::InputKey readCurrentKey();

  fmj::InputKey held_ = fmj::InputKey::None;
  std::uint32_t repeatAtMs_ = 0;
  std::int8_t brightnessDelta_ = 0;
};
