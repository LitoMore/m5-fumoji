#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "fmj/mono_canvas.hpp"

struct WideMapActor {
  std::uint8_t type = 0;
  std::uint8_t worldX = 0;
  std::uint8_t worldY = 0;
  std::uint8_t direction = 0;
  std::uint8_t step = 0;
  std::uint8_t imageBank = 0;
  std::uint16_t imageOffset = 0;
};

struct WideMapState {
  static constexpr std::size_t kActorCapacity = 41;

  std::uint16_t baseBank = 0;
  std::uint8_t mapWidth = 0;
  std::uint8_t mapHeight = 0;
  std::uint8_t cameraX = 0;
  std::uint8_t cameraY = 0;
  std::uint8_t tileWidth = 0;
  std::uint8_t tileHeight = 0;
  std::uint8_t mapBank = 0;
  std::uint16_t mapOffset = 0;
  std::uint8_t tileBank = 0;
  std::uint16_t tileOffset = 0;
  std::uint8_t actorCount = 0;
  std::array<WideMapActor, kActorCapacity> actors{};
  std::array<std::uint8_t, fmj::MonoCanvas::kBufferSize> overlayMask{};
};
