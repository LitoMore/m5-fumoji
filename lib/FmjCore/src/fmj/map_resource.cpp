#include "fmj/map_resource.hpp"

#include <array>

namespace fmj {

bool MapResource::open(const ByteSource& source, std::size_t offset) {
  source_ = nullptr;
  std::array<std::uint8_t, 0x12> header{};
  if (!source.read(offset, header.data(), header.size())) return false;
  type_ = header[0];
  index_ = header[1];
  tileSetIndex_ = header[2];
  for (std::size_t i = 0; i < name_.size() - 1U && i + 3U < 0x10U; ++i) {
    name_[i] = static_cast<char>(header[i + 3U]);
    if (name_[i] == '\0') break;
  }
  name_.back() = '\0';
  width_ = header[0x10U];
  height_ = header[0x11U];
  if (width_ == 0U || height_ == 0U) return false;
  dataOffset_ = offset + header.size();
  const auto dataLength = static_cast<std::size_t>(width_) * height_ * 2U;
  if (dataOffset_ > source.size() || dataLength > source.size() - dataOffset_) {
    return false;
  }
  source_ = &source;
  return true;
}

bool MapResource::tile(int x, int y, std::uint8_t& tileIndex, bool& walkable,
                       std::uint8_t& event) const {
  if (source_ == nullptr || x < 0 || y < 0 || x >= width_ || y >= height_) {
    return false;
  }
  std::array<std::uint8_t, 2> bytes{};
  const auto index = static_cast<std::size_t>(y * width_ + x) * 2U;
  if (!source_->read(dataOffset_ + index, bytes.data(), bytes.size())) {
    return false;
  }
  tileIndex = static_cast<std::uint8_t>(bytes[0] & 0x7FU);
  walkable = (bytes[0] & 0x80U) != 0U;
  event = bytes[1];
  return true;
}

}  // namespace fmj

