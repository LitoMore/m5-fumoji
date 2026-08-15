#include "fmj/save_format.hpp"

#include <cstring>
#include <limits>

#include "fmj/crc32.hpp"

namespace fmj {

SaveHeader makeSaveHeader(const void* payload, std::size_t payloadSize) {
  SaveHeader header{{'F', 'M', 'J', 'S'}, kSaveFormatVersion,
                    static_cast<std::uint16_t>(sizeof(SaveHeader)), 0U, 0U};
  if (payloadSize <= std::numeric_limits<std::uint32_t>::max()) {
    header.payloadSize = static_cast<std::uint32_t>(payloadSize);
    header.payloadCrc32 = crc32(payload, payloadSize);
  }
  return header;
}

bool validateSaveHeader(const SaveHeader& header, const void* payload,
                        std::size_t payloadSize) {
  return std::memcmp(header.magic, "FMJS", 4) == 0 &&
         header.version == kSaveFormatVersion &&
         header.headerSize == sizeof(SaveHeader) &&
         header.payloadSize == payloadSize &&
         header.payloadCrc32 == crc32(payload, payloadSize);
}

}  // namespace fmj

