#pragma once

#include <cstddef>
#include <cstdint>

namespace fmj {

std::uint32_t crc32(const void* data, std::size_t length);

}  // namespace fmj

