#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "fmj/byte_source.hpp"

namespace fmj {

struct ScriptCommand {
  std::uint8_t opcode = 0;
  std::uint16_t offset = 0;
  std::uint16_t operandOffset = 0;
  std::uint16_t operandLength = 0;
  std::uint16_t nextOffset = 0;
};

class ScriptResource {
 public:
  static constexpr std::uint8_t kOpcodeCount = 80;

  bool open(const ByteSource& source, std::size_t offset);
  bool decode(std::uint16_t codeOffset, ScriptCommand& command) const;
  bool eventCodeOffset(std::uint8_t eventId,
                       std::uint16_t& codeOffset) const;
  bool readCode(std::uint16_t codeOffset, void* destination,
                std::size_t length) const;
  bool readU8(std::uint16_t codeOffset, std::uint8_t& value) const;
  bool readU16(std::uint16_t codeOffset, std::uint16_t& value) const;
  bool readU32(std::uint16_t codeOffset, std::uint32_t& value) const;

  std::uint8_t type() const { return type_; }
  std::uint8_t index() const { return index_; }
  std::uint8_t eventCount() const { return eventCount_; }
  std::uint16_t codeSize() const { return codeSize_; }
  std::uint16_t headerSize() const { return headerSize_; }
  const char* descriptionBytes() const { return description_.data(); }
  bool valid() const { return source_ != nullptr; }

  static const char* opcodeName(std::uint8_t opcode);

 private:
  bool findCStringEnd(std::uint16_t start, std::uint16_t& end) const;

  const ByteSource* source_ = nullptr;
  std::size_t resourceOffset_ = 0;
  std::size_t eventTableOffset_ = 0;
  std::size_t codeDataOffset_ = 0;
  std::uint8_t type_ = 0;
  std::uint8_t index_ = 0;
  std::uint8_t eventCount_ = 0;
  std::uint16_t resourceLength_ = 0;
  std::uint16_t headerSize_ = 0;
  std::uint16_t codeSize_ = 0;
  std::array<char, 23> description_{};
};

}  // namespace fmj
