#include "fmj/crc32.hpp"

namespace fmj {

std::uint32_t crc32(const void* data, std::size_t length) {
  const auto* bytes = static_cast<const std::uint8_t*>(data);
  std::uint32_t value = 0xFFFFFFFFU;
  for (std::size_t i = 0; i < length; ++i) {
    value ^= bytes[i];
    for (int bit = 0; bit < 8; ++bit) {
      const auto mask = static_cast<std::uint32_t>(
          -static_cast<std::int32_t>(value & 1U));
      value = (value >> 1U) ^ (0xEDB88320U & mask);
    }
  }
  return value ^ 0xFFFFFFFFU;
}

}  // namespace fmj

