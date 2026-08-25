#include "wide_battle_renderer.hpp"

#include <algorithm>
#include <array>
#include <cstring>

namespace {

constexpr std::size_t kHeaderBytes = 6U;
constexpr int kOriginalWidth = fmj::MonoCanvas::kWidth;
constexpr int kOriginalHeight = fmj::MonoCanvas::kHeight;
constexpr int kOffsetX = (WideCanvas::kWidth - kOriginalWidth) / 2;
constexpr int kOffsetY = (WideCanvas::kHeight - kOriginalHeight) / 2;

bool sameState(const WideBattleState& left, const WideBattleState& right) {
  return left.background == right.background &&
         left.topRight == right.topRight &&
         left.bottomLeft == right.bottomLeft;
}

int wrapped(int value, int period) {
  const int remainder = value % period;
  return remainder < 0 ? remainder + period : remainder;
}

}  // namespace

bool WideBattleRenderer::begin(const fmj::ByteSource& game) {
  return index_.open(game);
}

bool WideBattleRenderer::prepare(const fmj::ByteSource& game,
                                 const WideBattleState& state) {
  return cached_ && sameState(state, cachedState_)
             ? true
             : loadBackground(game, state);
}

bool WideBattleRenderer::render(const fmj::ByteSource& game,
                                const WideBattleState& state,
                                const fmj::MonoCanvas& source,
                                WideCanvas& destination) {
  if (!prepare(game, state)) return false;

  destination = wideBackground_;
  // Background decorations are rebuilt at the physical wide-screen corners.
  // The engine-provided mask identifies every opaque dynamic pixel, including
  // white text and reverse-video pixels that cannot be recovered by comparing
  // colours with the old background.
  constexpr int kSourceRowBytes = kOriginalWidth / 8;
  constexpr int kDestinationRowBytes = WideCanvas::kRowBytes;
  constexpr int kDestinationByteX = kOffsetX / 8;
  for (int y = 0; y < kOriginalHeight; ++y) {
    const auto* sourceRow = source.data() + y * kSourceRowBytes;
    const auto* maskRow =
        state.overlayMask.data() + y * kSourceRowBytes;
    auto* destinationRow =
        destination.data() + (y + kOffsetY) * kDestinationRowBytes +
        kDestinationByteX;
    for (int byte = 0; byte < kSourceRowBytes; ++byte) {
      const auto mask = maskRow[byte];
      destinationRow[byte] = static_cast<std::uint8_t>(
          (destinationRow[byte] & static_cast<std::uint8_t>(~mask)) |
          (sourceRow[byte] & mask));
    }
  }
  return true;
}

bool WideBattleRenderer::loadPicture(const fmj::ByteSource& game,
                                     std::uint8_t type,
                                     std::uint8_t pictureIndex,
                                     Image& image) {
  const auto* entry = index_.find(fmj::ResourceType::Picture, type,
                                  pictureIndex);
  if (entry == nullptr) return false;
  std::array<std::uint8_t, kHeaderBytes> header{};
  if (!game.read(entry->offset, header.data(), header.size())) return false;
  image.width = header[2];
  image.height = header[3];
  const auto slices = header[4];
  image.mode = header[5];
  if (image.width == 0U || image.height == 0U || slices == 0U ||
      (image.mode != 1U && image.mode != 2U)) {
    return false;
  }
  image.rowStride =
      static_cast<std::size_t>((image.width + 7U) / 8U) * image.mode;
  const auto bytes = image.rowStride * image.height;
  image.pixels.resize(bytes);
  return game.read(entry->offset + kHeaderBytes, image.pixels.data(), bytes);
}

bool WideBattleRenderer::loadBackground(const fmj::ByteSource& game,
                                        const WideBattleState& state) {
  if (!loadPicture(game, 4U, state.background, base_) ||
      !loadPicture(game, 4U, state.topRight, topRight_) ||
      !loadPicture(game, 4U, state.bottomLeft, bottomLeft_)) {
    return false;
  }
  baseBackground_.clear(false);
  draw(base_, baseBackground_, 0, 0);
  backgroundPeriodX_ = horizontalPeriod(base_);
  backgroundPeriodY_ = verticalPeriod(base_);
  wideBackground_.clear(false);
  for (int y = 0; y < WideCanvas::kHeight; ++y) {
    const int relativeY = y - kOffsetY;
    const int sourceY = backgroundPeriodY_ != 0
                            ? wrapped(relativeY, backgroundPeriodY_)
                            : std::max(0, std::min(kOriginalHeight - 1,
                                                  relativeY));
    for (int x = 0; x < WideCanvas::kWidth; ++x) {
      const int relativeX = x - kOffsetX;
      const int sourceX = backgroundPeriodX_ != 0
                              ? wrapped(relativeX, backgroundPeriodX_)
                              : std::max(0, std::min(kOriginalWidth - 1,
                                                    relativeX));
      bool black = baseBackground_.pixel(sourceX, sourceY);
      if (backgroundPeriodX_ != 0 || backgroundPeriodY_ != 0) {
        imagePixel(base_, sourceX, sourceY, black);
      }
      wideBackground_.setPixel(x, y, black);
    }
  }
  if (backgroundPeriodX_ == 0 && backgroundPeriodY_ == 0) {
    for (int y = 0; y < kOriginalHeight; ++y) {
      for (int x = 0; x < kOriginalWidth; ++x) {
        wideBackground_.setPixel(x + kOffsetX, y + kOffsetY,
                                 baseBackground_.pixel(x, y));
      }
    }
  }
  draw(topRight_, wideBackground_, WideCanvas::kWidth - topRight_.width, 0);
  draw(bottomLeft_, wideBackground_, 0,
       WideCanvas::kHeight - bottomLeft_.height);
  cachedState_ = state;
  cached_ = true;
  return true;
}

bool WideBattleRenderer::imagePixel(const Image& image, int x, int y,
                                    bool& black) {
  if (x < 0 || y < 0 || x >= image.width || y >= image.height) return false;
  const auto* row = image.pixels.data() +
                    static_cast<std::size_t>(y) * image.rowStride;
  if (image.mode == 1U) {
    black = (row[x / 8] & (0x80U >> (x & 7))) != 0U;
    return true;
  }
  const auto shift = static_cast<unsigned>(6U - (x & 3) * 2U);
  const auto pair = static_cast<std::uint8_t>((row[x / 4] >> shift) & 3U);
  if ((pair & 2U) != 0U) return false;
  black = (pair & 1U) != 0U;
  return true;
}

int WideBattleRenderer::horizontalPeriod(const Image& image) {
  for (int period = 1; period <= image.width / 2; ++period) {
    bool matches = true;
    for (int y = 0; matches && y < image.height; ++y) {
      for (int x = 0; x + period < image.width; ++x) {
        bool left = false;
        bool right = false;
        const bool leftVisible = imagePixel(image, x, y, left);
        const bool rightVisible = imagePixel(image, x + period, y, right);
        if (leftVisible != rightVisible || (leftVisible && left != right)) {
          matches = false;
          break;
        }
      }
    }
    if (matches) return period;
  }
  return 0;
}

int WideBattleRenderer::verticalPeriod(const Image& image) {
  for (int period = 1; period <= image.height / 2; ++period) {
    bool matches = true;
    for (int y = 0; matches && y + period < image.height; ++y) {
      for (int x = 0; x < image.width; ++x) {
        bool top = false;
        bool bottom = false;
        const bool topVisible = imagePixel(image, x, y, top);
        const bool bottomVisible = imagePixel(image, x, y + period, bottom);
        if (topVisible != bottomVisible || (topVisible && top != bottom)) {
          matches = false;
          break;
        }
      }
    }
    if (matches) return period;
  }
  return 0;
}

void WideBattleRenderer::draw(const Image& image, fmj::MonoCanvas& canvas,
                              int x, int y) {
  for (int iy = 0; iy < image.height; ++iy) {
    for (int ix = 0; ix < image.width; ++ix) {
      bool black = false;
      if (imagePixel(image, ix, iy, black)) canvas.setPixel(x + ix, y + iy, black);
    }
  }
}

void WideBattleRenderer::draw(const Image& image, WideCanvas& canvas,
                              int x, int y) {
  for (int iy = 0; iy < image.height; ++iy) {
    for (int ix = 0; ix < image.width; ++ix) {
      bool black = false;
      if (imagePixel(image, ix, iy, black)) canvas.setPixel(x + ix, y + iy, black);
    }
  }
}
