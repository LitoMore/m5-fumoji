#include "fmj/res_image.hpp"

#include <array>

namespace fmj {

std::size_t ImageResource::rowStride() const {
  const auto bytePlanes = static_cast<std::size_t>((width_ + 7U) / 8U);
  return bytePlanes * static_cast<std::size_t>(mode_);
}

std::size_t ImageResource::serializedSize() const {
  if (source_ == nullptr) return 0U;
  return 6U + rowStride() * static_cast<std::size_t>(height_) *
                  static_cast<std::size_t>(sliceCount_);
}

bool ImageResource::open(const ByteSource& source, std::size_t offset) {
  source_ = nullptr;
  std::array<std::uint8_t, 6> header{};
  if (!source.read(offset, header.data(), header.size())) return false;
  type_ = header[0];
  index_ = header[1];
  width_ = header[2];
  height_ = header[3];
  sliceCount_ = header[4];
  mode_ = header[5];
  if (mode_ > 2U) {
    return false;
  }
  dataOffset_ = offset + header.size();
  if (width_ == 0U || height_ == 0U || sliceCount_ == 0U || mode_ == 0U) {
    source_ = &source;
    return true;
  }
  const auto dataLength = rowStride() * static_cast<std::size_t>(height_) *
                          static_cast<std::size_t>(sliceCount_);
  if (dataOffset_ > source.size() || dataLength > source.size() - dataOffset_) {
    return false;
  }
  source_ = &source;
  return true;
}

bool ImageResource::drawSlice(MonoCanvas& canvas, std::uint8_t slice, int x,
                              int y) const {
  if (source_ == nullptr || width_ == 0U || height_ == 0U || mode_ == 0U ||
      slice >= sliceCount_) {
    return false;
  }
  constexpr std::size_t kMaxRowBytes = 64;
  std::array<std::uint8_t, kMaxRowBytes> row{};
  const auto stride = rowStride();
  if (stride == 0U || stride > row.size()) return false;
  const auto sliceOffset = dataOffset_ +
                           static_cast<std::size_t>(slice) * stride * height_;
  for (std::size_t sourceY = 0; sourceY < height_; ++sourceY) {
    if (!source_->read(sliceOffset + sourceY * stride, row.data(), stride)) {
      return false;
    }
    for (std::size_t sourceX = 0; sourceX < width_; ++sourceX) {
      if (mode_ == 1U) {
        const auto byte = row[sourceX / 8U];
        const bool black = (byte & (0x80U >> (sourceX & 7U))) != 0U;
        canvas.setPixel(x + static_cast<int>(sourceX),
                        y + static_cast<int>(sourceY), black);
      } else {
        const auto byte = row[sourceX / 4U];
        const auto shift = static_cast<unsigned>(6U - (sourceX & 3U) * 2U);
        const auto pair = static_cast<std::uint8_t>((byte >> shift) & 0x03U);
        if ((pair & 0x02U) == 0U) {
          canvas.setPixel(x + static_cast<int>(sourceX),
                          y + static_cast<int>(sourceY),
                          (pair & 0x01U) != 0U);
        }
      }
    }
  }
  return true;
}

}  // namespace fmj
