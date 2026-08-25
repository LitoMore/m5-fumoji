/* Copyright (C) 2026 LitoMore; SPDX-License-Identifier: GPL-2.0-only */
#pragma once

#include <Arduino.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include "wide_canvas.hpp"

class ScreenStreamer {
 public:
  void update(const WideCanvas& canvas, bool frameChanged);
  bool active() const { return enabled_; }

 private:
  static constexpr std::uint32_t kFramePeriodMs = 50U;
  static constexpr std::uint32_t kClientTimeoutMs = 3500U;
  static constexpr std::size_t kCommandCapacity = 32U;

  void readCommands();
  void handleCommand();
  void sendFrame(const WideCanvas& canvas);

  std::array<char, kCommandCapacity> command_{};
  std::size_t commandLength_ = 0;
  std::uint16_t sequence_ = 0;
  std::uint32_t lastClientMs_ = 0;
  std::uint32_t lastFrameMs_ = 0;
  bool enabled_ = false;
  bool pendingFrame_ = false;
};
