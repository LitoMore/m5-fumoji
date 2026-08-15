#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "fmj/byte_source.hpp"

namespace fmj {

enum class ResourceType : std::uint8_t {
  Script = 1,
  Map = 2,
  Actor = 3,
  Magic = 4,
  Effect = 5,
  Goods = 6,
  Tile = 7,
  ActorPicture = 8,
  GoodsPicture = 9,
  EffectPicture = 10,
  Picture = 11,
  Chain = 12,
};

struct ResourceEntry {
  std::uint32_t key = 0;
  std::uint32_t offset = 0;
};

class DatLibIndex {
 public:
  static constexpr std::size_t kMaxEntries = 1536;

  bool open(const ByteSource& source);
  void close();
  const ResourceEntry* find(ResourceType resourceType, std::uint8_t type,
                            std::uint8_t index) const;
  const ResourceEntry* first(ResourceType resourceType) const;
  const ResourceEntry* entryAt(std::size_t index) const {
    return index < entryCount_ ? &entries_[index] : nullptr;
  }
  std::size_t entryCount() const { return entryCount_; }
  bool valid() const { return valid_; }

  static constexpr std::uint32_t makeKey(ResourceType resourceType,
                                         std::uint8_t type,
                                         std::uint8_t index) {
    return (static_cast<std::uint32_t>(resourceType) << 16U) |
           (static_cast<std::uint32_t>(type) << 8U) |
           static_cast<std::uint32_t>(index);
  }

 private:
  std::array<ResourceEntry, kMaxEntries> entries_{};
  std::size_t entryCount_ = 0;
  bool valid_ = false;
};

}  // namespace fmj
