/* Copyright (C) 2026 LitoMore; SPDX-License-Identifier: GPL-2.0-only */
#include "screen_streamer.hpp"

#include <cstring>

#include "fmj/crc32.hpp"

namespace {

constexpr std::uint8_t kMagic[] = {'F', 'M', 'J', 'F'};
constexpr std::uint8_t kProtocolVersion = 1;
constexpr std::size_t kHeaderSize = 14;

void putU16(std::uint8_t* destination, std::uint16_t value) {
  destination[0] = static_cast<std::uint8_t>(value);
  destination[1] = static_cast<std::uint8_t>(value >> 8U);
}

void putU32(std::uint8_t* destination, std::uint32_t value) {
  destination[0] = static_cast<std::uint8_t>(value);
  destination[1] = static_cast<std::uint8_t>(value >> 8U);
  destination[2] = static_cast<std::uint8_t>(value >> 16U);
  destination[3] = static_cast<std::uint8_t>(value >> 24U);
}

}  // namespace

void ScreenStreamer::update(const WideCanvas& canvas,
                            bool frameChanged) {
  readCommands();
  const auto now = millis();
  if (enabled_ && now - lastClientMs_ > kClientTimeoutMs) {
    enabled_ = false;
    pendingFrame_ = false;
  }
  pendingFrame_ = pendingFrame_ || frameChanged;
  if (enabled_ && pendingFrame_ && now - lastFrameMs_ >= kFramePeriodMs) {
    sendFrame(canvas);
    pendingFrame_ = false;
    lastFrameMs_ = now;
  }
}

void ScreenStreamer::readCommands() {
  while (Serial.available() > 0) {
    const int value = Serial.read();
    if (value < 0) break;
    if (value == '\r') continue;
    if (value == '\n') {
      command_[commandLength_] = '\0';
      handleCommand();
      commandLength_ = 0;
      continue;
    }
    if (commandLength_ + 1U < command_.size()) {
      command_[commandLength_++] = static_cast<char>(value);
    } else {
      commandLength_ = 0;
    }
  }
}

void ScreenStreamer::handleCommand() {
  const auto now = millis();
  if (std::strcmp(command_.data(), "FMJSTREAM ON") == 0) {
    enabled_ = true;
    pendingFrame_ = true;
    lastClientMs_ = now;
    Serial.println("FMJSTREAM READY 240 135 1");
  } else if (std::strcmp(command_.data(), "FMJSTREAM PING") == 0) {
    if (enabled_) lastClientMs_ = now;
  } else if (std::strcmp(command_.data(), "FMJSTREAM OFF") == 0) {
    enabled_ = false;
    pendingFrame_ = false;
    Serial.println("FMJSTREAM STOPPED");
  }
}

void ScreenStreamer::sendFrame(const WideCanvas& canvas) {
  std::array<std::uint8_t, kHeaderSize> header{};
  std::memcpy(header.data(), kMagic, sizeof(kMagic));
  header[4] = kProtocolVersion;
  header[5] = 0;
  putU16(header.data() + 6, sequence_++);
  putU16(header.data() + 8,
         static_cast<std::uint16_t>(WideCanvas::kBufferSize));
  putU32(header.data() + 10,
         fmj::crc32(canvas.data(), WideCanvas::kBufferSize));
  Serial.write(header.data(), header.size());
  Serial.write(canvas.data(), WideCanvas::kBufferSize);
}
