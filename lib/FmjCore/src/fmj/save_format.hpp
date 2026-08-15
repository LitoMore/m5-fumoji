#pragma once

#include <cstddef>
#include <cstdint>

namespace fmj {

constexpr std::uint16_t kSaveFormatVersion = 1;

#pragma pack(push, 1)
struct SaveHeader {
  char magic[4];
  std::uint16_t version;
  std::uint16_t headerSize;
  std::uint32_t payloadSize;
  std::uint32_t payloadCrc32;
};
#pragma pack(pop)

static_assert(sizeof(SaveHeader) == 16, "SaveHeader layout changed");

SaveHeader makeSaveHeader(const void* payload, std::size_t payloadSize);
bool validateSaveHeader(const SaveHeader& header, const void* payload,
                        std::size_t payloadSize);

class SaveBackend {
 public:
  virtual ~SaveBackend() = default;
  virtual bool hasSlot(std::uint8_t slot) const = 0;
  virtual bool load(std::uint8_t slot, void* payload, std::size_t capacity,
                    std::size_t& bytesRead) = 0;
  virtual bool save(std::uint8_t slot, const void* payload,
                    std::size_t payloadSize) = 0;
};

}  // namespace fmj

