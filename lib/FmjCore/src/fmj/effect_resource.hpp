#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "fmj/byte_source.hpp"
#include "fmj/mono_canvas.hpp"
#include "fmj/res_image.hpp"

namespace fmj {

class EffectResource {
 public:
  static constexpr std::size_t kMaxFrames = 128;
  static constexpr std::size_t kMaxImages = 32;

  bool open(const ByteSource& source, std::size_t offset);
  bool drawFrame(MonoCanvas& canvas, std::uint8_t frame, int dx = 0,
                 int dy = 0) const;
  std::uint8_t frameShow(std::uint8_t frame) const;
  std::uint8_t frameNextShow(std::uint8_t frame) const;

  std::uint8_t frameCount() const { return frameCount_; }
  std::uint8_t imageCount() const { return imageCount_; }
  std::uint8_t startFrame() const { return startFrame_; }
  std::uint8_t endFrame() const { return endFrame_; }
  bool valid() const { return source_ != nullptr; }

 private:
  struct Frame {
    std::uint8_t x = 0;
    std::uint8_t y = 0;
    std::uint8_t show = 0;
    std::uint8_t nextShow = 0;
    std::uint8_t imageIndex = 0;
  };

  const ByteSource* source_ = nullptr;
  std::uint8_t frameCount_ = 0;
  std::uint8_t imageCount_ = 0;
  std::uint8_t startFrame_ = 0;
  std::uint8_t endFrame_ = 0;
  std::array<Frame, kMaxFrames> frames_{};
  std::array<ImageResource, kMaxImages> images_{};
};

}  // namespace fmj
