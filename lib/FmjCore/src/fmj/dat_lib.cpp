#include "fmj/dat_lib.hpp"

#include <algorithm>
#include <array>

namespace fmj {

bool DatLibIndex::open(const ByteSource& source) {
  close();
  if (source.size() < 0x2003U) return false;

  std::array<std::uint8_t, 3> signature{};
  if (!source.read(0, signature.data(), signature.size()) ||
      signature[0] != 'L' || signature[1] != 'I' || signature[2] != 'B') {
    return false;
  }

  // Read both LIB tables in modest sequential chunks. Alternating thousands
  // of seeks between 0x10 and 0x2000 works for memory-backed files but is
  // needlessly expensive and unreliable on embedded LittleFS streams.
  constexpr std::size_t kEntriesPerChunk = 64U;
  constexpr std::size_t kChunkBytes = kEntriesPerChunk * 3U;
  std::array<std::uint8_t, kChunkBytes> directoryChunk{};
  std::array<std::uint8_t, kChunkBytes> addressChunk{};
  for (std::size_t base = 0; base < entries_.size();
       base += kEntriesPerChunk) {
    const auto count = std::min(kEntriesPerChunk, entries_.size() - base);
    const auto bytes = count * 3U;
    if (!source.read(0x10U + base * 3U, directoryChunk.data(), bytes) ||
        !source.read(0x2000U + base * 3U, addressChunk.data(), bytes)) {
      return false;
    }
    for (std::size_t item = 0; item < count; ++item) {
      const auto* directory = directoryChunk.data() + item * 3U;
      const auto* address = addressChunk.data() + item * 3U;
      if (directory[0] == 0xFFU) {
        valid_ = true;
        return true;
      }
      if (directory[0] < static_cast<std::uint8_t>(ResourceType::Script) ||
          directory[0] > static_cast<std::uint8_t>(ResourceType::Chain)) {
        return false;
      }

      const auto resourceType = static_cast<ResourceType>(directory[0]);
      const auto key = makeKey(resourceType, directory[1], directory[2]);
      const auto resourceOffset =
          static_cast<std::uint32_t>(address[0]) * 0x4000U |
          static_cast<std::uint32_t>(address[2]) << 8U |
          static_cast<std::uint32_t>(address[1]);

      if (resourceOffset < source.size()) {
        bool replaced = false;
        for (std::size_t i = 0; i < entryCount_; ++i) {
          if (entries_[i].key == key) {
            entries_[i].offset = resourceOffset;
            replaced = true;
            break;
          }
        }
        if (!replaced) {
          if (entryCount_ >= entries_.size()) return false;
          entries_[entryCount_++] = ResourceEntry{key, resourceOffset};
        }
      }
    }
  }
  return false;
}

void DatLibIndex::close() {
  entryCount_ = 0;
  valid_ = false;
}

const ResourceEntry* DatLibIndex::find(ResourceType resourceType,
                                       std::uint8_t type,
                                       std::uint8_t index) const {
  const auto key = makeKey(resourceType, type, index);
  for (std::size_t i = 0; i < entryCount_; ++i) {
    if (entries_[i].key == key) return &entries_[i];
  }
  return nullptr;
}

const ResourceEntry* DatLibIndex::first(ResourceType resourceType) const {
  const auto prefix = static_cast<std::uint32_t>(resourceType) << 16U;
  for (std::size_t i = 0; i < entryCount_; ++i) {
    if ((entries_[i].key & 0x00FF0000U) == prefix) return &entries_[i];
  }
  return nullptr;
}

}  // namespace fmj
