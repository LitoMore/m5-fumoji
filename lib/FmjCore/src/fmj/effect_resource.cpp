#include "fmj/effect_resource.hpp"

#include <array>

namespace fmj {

bool EffectResource::open(const ByteSource& source, std::size_t offset) {
  source_ = nullptr;
  std::array<std::uint8_t, 6> header{};
  if (!source.read(offset, header.data(), header.size())) return false;
  frameCount_ = header[2];
  imageCount_ = header[3];
  startFrame_ = header[4];
  endFrame_ = header[5];
  if (frameCount_ == 0U || frameCount_ > frames_.size() ||
      imageCount_ == 0U || imageCount_ > images_.size()) {
    return false;
  }

  std::size_t cursor = offset + header.size();
  for (std::size_t i = 0; i < frameCount_; ++i) {
    std::array<std::uint8_t, 5> frame{};
    if (!source.read(cursor, frame.data(), frame.size())) return false;
    frames_[i] = Frame{frame[0], frame[1], frame[2], frame[3], frame[4]};
    if (frames_[i].imageIndex >= imageCount_) return false;
    cursor += frame.size();
  }
  for (std::size_t i = 0; i < imageCount_; ++i) {
    if (!images_[i].open(source, cursor)) return false;
    cursor += images_[i].serializedSize();
  }
  source_ = &source;
  return true;
}

bool EffectResource::drawFrame(MonoCanvas& canvas, std::uint8_t frame, int dx,
                               int dy) const {
  if (source_ == nullptr || frame >= frameCount_) return false;
  const auto& descriptor = frames_[frame];
  return images_[descriptor.imageIndex].drawSlice(
      canvas, 0U, dx + descriptor.x, dy + descriptor.y);
}

std::uint8_t EffectResource::frameShow(std::uint8_t frame) const {
  return source_ != nullptr && frame < frameCount_ ? frames_[frame].show : 0U;
}

std::uint8_t EffectResource::frameNextShow(std::uint8_t frame) const {
  return source_ != nullptr && frame < frameCount_ ? frames_[frame].nextShow
                                                   : 0U;
}

}  // namespace fmj
