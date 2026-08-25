#include "wide_map_renderer.hpp"

#include <algorithm>
#include <array>

namespace {

constexpr std::size_t kBankBytes = 0x1000U;
constexpr std::size_t kImageHeaderBytes = 6U;
constexpr std::size_t kMapHeaderBytes = 0x12U;
constexpr int kOriginalWidth = fmj::MonoCanvas::kWidth;
constexpr int kOriginalHeight = fmj::MonoCanvas::kHeight;
constexpr int kOverlayOffsetX = (WideCanvas::kWidth - kOriginalWidth) / 2;
constexpr int kOverlayOffsetY = (WideCanvas::kHeight - kOriginalHeight) / 2;

int clampedOrigin(int center, int centerCell, int mapSize, int visibleCells) {
  const int maximum = std::max(0, mapSize - visibleCells);
  return std::max(0, std::min(maximum, center - centerCell));
}

}  // namespace

void WideMapRenderer::compositeOverlay(const WideMapState& state,
                                       const fmj::MonoCanvas& source,
                                       const WideCanvas& base,
                                       WideCanvas& destination) {
  destination = base;
  constexpr int kSourceRowBytes = kOriginalWidth / 8;
  constexpr int kDestinationByteX = kOverlayOffsetX / 8;
  for (int y = 0; y < kOriginalHeight; ++y) {
    const auto* sourceRow = source.data() + y * kSourceRowBytes;
    const auto* maskRow =
        state.overlayMask.data() + y * kSourceRowBytes;
    auto* destinationRow =
        destination.data() + (y + kOverlayOffsetY) * WideCanvas::kRowBytes +
        kDestinationByteX;
    for (int byte = 0; byte < kSourceRowBytes; ++byte) {
      const auto mask = maskRow[byte];
      destinationRow[byte] = static_cast<std::uint8_t>(
          (destinationRow[byte] & static_cast<std::uint8_t>(~mask)) |
          (sourceRow[byte] & mask));
    }
  }
}

bool WideMapRenderer::render(const fmj::ByteSource& game,
                             const WideMapState& state,
                             WideCanvas& canvas) {
  error_ = Error::None;
  if (state.mapWidth == 0U || state.mapHeight == 0U ||
      state.tileWidth == 0U || state.tileHeight == 0U) {
    error_ = Error::InvalidState;
    return false;
  }

  const auto mapOffset = resourceOffset(state, state.mapBank, state.mapOffset);
  const auto tileOffset =
      resourceOffset(state, state.tileBank, state.tileOffset);
  if (mapOffset > game.size() || kMapHeaderBytes > game.size() - mapOffset) {
    error_ = Error::MapOutOfRange;
    return false;
  }
  if (!loadTileSet(game, tileOffset)) {
    error_ = Error::TileSet;
    return false;
  }
  if (tileHeader_.width != state.tileWidth ||
      tileHeader_.height != state.tileHeight) {
    error_ = Error::TileGeometry;
    return false;
  }

  const int columns =
      (WideCanvas::kWidth + state.tileWidth - 1) / state.tileWidth;
  const int rows =
      (WideCanvas::kHeight + state.tileHeight - 1) / state.tileHeight;
  int focusX = static_cast<int>(state.cameraX) + 4;
  int focusY = static_cast<int>(state.cameraY) + 3;
  if (state.actorCount != 0U && state.actors[0].type == 1U) {
    focusX = state.actors[0].worldX;
    focusY = state.actors[0].worldY;
  }
  const int originX = clampedOrigin(focusX, columns / 2, state.mapWidth,
                                    columns);
  const int originY = clampedOrigin(focusY, rows / 2, state.mapHeight, rows);

  canvas.clear(false);
  std::array<std::uint8_t, WideCanvas::kWidth * 2> mapRow{};
  for (int row = 0; row < rows; ++row) {
    const int mapY = originY + row;
    if (mapY >= state.mapHeight) break;
    const int count = std::min(columns,
                               static_cast<int>(state.mapWidth) - originX);
    const auto rowOffset = mapOffset + kMapHeaderBytes +
                           static_cast<std::size_t>(mapY * state.mapWidth +
                                                    originX) *
                               2U;
    const auto byteCount = static_cast<std::size_t>(count) * 2U;
    if (byteCount > mapRow.size() ||
        !game.read(rowOffset, mapRow.data(), byteCount)) {
      error_ = Error::MapRead;
      return false;
    }
    for (int column = 0; column < count; ++column) {
      const auto tile =
          static_cast<std::uint8_t>(mapRow[static_cast<std::size_t>(column) *
                                               2U] &
                                    0x7FU);
      if (tile >= tileHeader_.slices) continue;
      const auto* pixels = tilePixels_.data() +
                           static_cast<std::size_t>(tile) *
                               tileHeader_.sliceBytes;
      drawSlice(canvas, pixels, tileHeader_, column * state.tileWidth,
                row * state.tileHeight);
    }
  }

  std::array<std::uint8_t, WideMapState::kActorCapacity> order{};
  for (std::uint8_t index = 0; index < state.actorCount; ++index) {
    order[index] = index;
  }
  std::stable_sort(order.begin(), order.begin() + state.actorCount,
                   [&state](std::uint8_t left, std::uint8_t right) {
                     const auto& a = state.actors[left];
                     const auto& b = state.actors[right];
                     if (a.worldY != b.worldY) return a.worldY < b.worldY;
                     return a.type != 1U && b.type == 1U;
                   });
  for (std::uint8_t position = 0; position < state.actorCount; ++position) {
    const auto& actor = state.actors[order[position]];
    const int actorX = actor.worldX;
    const int actorY = actor.worldY;
    if (actorX + 1 < originX || actorY + 1 < originY ||
        actorX >= originX + columns || actorY >= originY + rows) {
      continue;
    }
    if (!drawActor(game, state, actor, originX, originY, canvas) &&
        actor.type == 1U) {
      error_ = Error::PlayerImage;
      return false;
    }
  }
  return true;
}

bool WideMapRenderer::loadTileSet(const fmj::ByteSource& game,
                                  std::size_t offset) {
  if (tileSetOffset_ == offset && !tilePixels_.empty()) return true;
  ImageHeader header{};
  if (!readImageHeader(game, offset, header)) return false;
  const auto dataBytes = header.sliceBytes * header.slices;
  if (offset > game.size() || kImageHeaderBytes > game.size() - offset ||
      dataBytes > game.size() - offset - kImageHeaderBytes) {
    return false;
  }
  std::vector<std::uint8_t> pixels(dataBytes);
  if (dataBytes != 0U &&
      !game.read(offset + kImageHeaderBytes, pixels.data(), dataBytes)) {
    return false;
  }
  tileSetOffset_ = offset;
  tileHeader_ = header;
  tilePixels_ = std::move(pixels);
  return true;
}

bool WideMapRenderer::readImageHeader(const fmj::ByteSource& game,
                                      std::size_t offset,
                                      ImageHeader& header) {
  std::array<std::uint8_t, kImageHeaderBytes> bytes{};
  if (!game.read(offset, bytes.data(), bytes.size())) return false;
  header.width = bytes[2];
  header.height = bytes[3];
  header.slices = bytes[4];
  header.mode = bytes[5];
  if (header.width == 0U || header.height == 0U || header.slices == 0U ||
      (header.mode != 1U && header.mode != 2U)) {
    return false;
  }
  const auto bytePlanes = static_cast<std::size_t>((header.width + 7U) / 8U);
  header.rowStride = bytePlanes * header.mode;
  header.sliceBytes = header.rowStride * header.height;
  return header.sliceBytes != 0U;
}

void WideMapRenderer::drawSlice(WideCanvas& canvas,
                                const std::uint8_t* pixels,
                                const ImageHeader& header, int x, int y) {
  if (pixels == nullptr) return;
  for (std::size_t sourceY = 0; sourceY < header.height; ++sourceY) {
    const auto* row = pixels + sourceY * header.rowStride;
    for (std::size_t sourceX = 0; sourceX < header.width; ++sourceX) {
      if (header.mode == 1U) {
        const bool black =
            (row[sourceX / 8U] & (0x80U >> (sourceX & 7U))) != 0U;
        canvas.setPixel(x + static_cast<int>(sourceX),
                        y + static_cast<int>(sourceY), black);
      } else {
        const auto shift = static_cast<unsigned>(6U - (sourceX & 3U) * 2U);
        const auto pair =
            static_cast<std::uint8_t>((row[sourceX / 4U] >> shift) & 0x03U);
        if ((pair & 0x02U) == 0U) {
          canvas.setPixel(x + static_cast<int>(sourceX),
                          y + static_cast<int>(sourceY),
                          (pair & 0x01U) != 0U);
        }
      }
    }
  }
}

bool WideMapRenderer::drawActor(const fmj::ByteSource& game,
                                const WideMapState& state,
                                const WideMapActor& actor, int originX,
                                int originY, WideCanvas& canvas) {
  const auto offset = resourceOffset(state, actor.imageBank,
                                     actor.imageOffset);
  ImageHeader header{};
  if (!readImageHeader(game, offset, header)) return false;
  const auto direction = actor.direction >= 1U && actor.direction <= 4U
                             ? actor.direction - 1U
                             : 0U;
  const auto step = actor.step == 3U ? 1U : std::min<std::uint8_t>(actor.step, 2U);
  auto slice = static_cast<std::uint16_t>(direction) * 3U + step;
  if (slice >= header.slices) slice = 0;
  actorPixels_.resize(header.sliceBytes);
  const auto pixelsOffset = offset + kImageHeaderBytes +
                            static_cast<std::size_t>(slice) *
                                header.sliceBytes;
  if (!game.read(pixelsOffset, actorPixels_.data(), actorPixels_.size())) {
    return false;
  }
  const int x = (static_cast<int>(actor.worldX) - originX) *
                state.tileWidth;
  const int y = (static_cast<int>(actor.worldY) - originY + 1) *
                    state.tileHeight -
                header.height;
  drawSlice(canvas, actorPixels_.data(), header, x, y);
  return true;
}

std::size_t WideMapRenderer::resourceOffset(
    const WideMapState& state, std::uint8_t bank,
    std::uint16_t offset) {
  return (static_cast<std::size_t>(state.baseBank) +
          static_cast<std::size_t>(bank) * 4U) *
             kBankBytes +
         offset;
}
