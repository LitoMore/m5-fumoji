#include "fmj/script_resource.hpp"

#include <array>
#include <limits>

namespace fmj {
namespace {

constexpr std::int8_t kCString = -1;
constexpr std::int8_t kChoice = -2;

// Operand sizes from the original GUT instruction format. Variable-length
// entries contain one zero-terminated BBK/GBK string, except CHOICE (31),
// which contains two strings followed by a 16-bit branch address.
constexpr std::array<std::int8_t, ScriptResource::kOpcodeCount> kOperandSizes = {
    4,  8,  6,  2,  2,  4,  6,  6,  4,  0,  2,  4,  4,  kCString,
    4,  1,  4,  1,  2,  4,  0,  6,  4,  4,  2,  4,  2,  2,
    kCString, 4, 10, kChoice, 8,  2,  4, 22,  0,  0,  8, 30,  2,  4,
    4,  4,  6,  0,  4,  kCString, 6,  4,  4,  2,  0,  6,
    kCString, 0,  0,  6, 10,  6,  6,  kCString, 8,  4,
    kCString, 6, 4, 8, 0, kCString, 0, 0, 0, 4, 0, 0, 4, 10, 2, 2};

constexpr std::array<const char*, ScriptResource::kOpcodeCount> kOpcodeNames = {
    "MUSIC",       "LOADMAP",       "CREATEACTOR",   "DELETENPC",
    "MAPEVENT",    "ACTOREVENT",    "MOVE",          "ACTORMOVE",
    "ACTORSPEED",  "CALLBACK",      "GOTO",          "IF",
    "SET",         "SAY",           "STARTCHAPTER",  "SCREENR",
    "SCREENS",     "SCREENA",       "EVENT",         "MONEY",
    "GAMEOVER",    "IFCMP",         "ADD",           "SUB",
    "SETCONTROLID", "GUTEVENT",     "SETEVENT",      "CLREVENT",
    "BUY",         "FACETOFACE",    "MOVIE",         "CHOICE",
    "CREATEBOX",   "DELETEBOX",     "GAINGOODS",     "INITFIGHT",
    "FIGHTENABLE", "FIGHTDISENABLE", "CREATENPC",    "ENTERFIGHT",
    "DELETEACTOR", "GAINMONEY",     "USEMONEY",      "SETMONEY",
    "LEARNMAGIC",  "SALE",          "NPCMOVEMOD",    "MESSAGE",
    "DELETEGOODS", "RESUMEACTORHP", "ACTORLAYERUP",  "BOXOPEN",
    "DELALLNPC",   "NPCSTEP",       "SETSCENENAME",  "SHOWSCENENAME",
    "SHOWSCREEN",  "USEGOODS",      "ATTRIBTEST",    "ATTRIBSET",
    "ATTRIBADD",   "SHOWGUT",       "USEGOODSNUM",   "RANDRADE",
    "MENU",        "TESTMONEY",     "CALLCHAPTER",   "DISCMP",
    "RETURN",      "TIMEMSG",       "DISABLESAVE",   "ENABLESAVE",
    "GAMESAVE",    "SETEVENTTIMER", "ENABLESHOWPOS", "DISABLESHOWPOS",
    "SETTO",       "TESTGOODSNUM",  "SETFIGHTMISS",  "SETARMSTOSS"};

std::uint16_t little16(const std::uint8_t* bytes) {
  return static_cast<std::uint16_t>(bytes[0]) |
         static_cast<std::uint16_t>(bytes[1]) << 8U;
}

}  // namespace

bool ScriptResource::open(const ByteSource& source, std::size_t offset) {
  source_ = nullptr;
  std::array<std::uint8_t, 0x1B> header{};
  if (!source.read(offset, header.data(), header.size())) return false;

  const auto length = little16(header.data() + 0x18U);
  const auto eventCount = header[0x1AU];
  const auto headerSize = static_cast<std::uint32_t>(eventCount) * 2U + 3U;
  if (length < headerSize) return false;
  const auto codeSize = static_cast<std::uint32_t>(length) - headerSize;
  const auto dataOffset = offset + 0x1BU + eventCount * 2U;
  if (codeSize > std::numeric_limits<std::uint16_t>::max() ||
      dataOffset > source.size() || codeSize > source.size() - dataOffset) {
    return false;
  }

  type_ = header[0];
  index_ = header[1];
  description_.fill(0);
  for (std::size_t i = 0; i + 1U < description_.size() && i < 0x16U; ++i) {
    description_[i] = static_cast<char>(header[i + 2U]);
    if (description_[i] == '\0') break;
  }
  resourceOffset_ = offset;
  resourceLength_ = length;
  eventCount_ = eventCount;
  headerSize_ = static_cast<std::uint16_t>(headerSize);
  codeSize_ = static_cast<std::uint16_t>(codeSize);
  eventTableOffset_ = offset + 0x1BU;
  codeDataOffset_ = dataOffset;
  source_ = &source;
  return true;
}

bool ScriptResource::findCStringEnd(std::uint16_t start,
                                    std::uint16_t& end) const {
  if (source_ == nullptr || start >= codeSize_) return false;
  for (std::uint32_t cursor = start; cursor < codeSize_; ++cursor) {
    std::uint8_t byte = 0;
    if (!source_->read(codeDataOffset_ + cursor, &byte, 1U)) return false;
    if (byte == 0U) {
      end = static_cast<std::uint16_t>(cursor + 1U);
      return true;
    }
  }
  return false;
}

bool ScriptResource::decode(std::uint16_t codeOffset,
                            ScriptCommand& command) const {
  command = ScriptCommand{};
  std::uint8_t opcode = 0;
  if (!readU8(codeOffset, opcode) || opcode >= kOpcodeCount) return false;

  const auto operandOffset = static_cast<std::uint32_t>(codeOffset) + 1U;
  std::uint32_t operandLength = 0;
  const auto fixedSize = kOperandSizes[opcode];
  if (fixedSize >= 0) {
    operandLength = static_cast<std::uint8_t>(fixedSize);
  } else if (fixedSize == kCString) {
    const std::uint16_t prefix =
        opcode == 13U || opcode == 64U || opcode == 69U ? 2U :
        opcode == 61U ? 4U : 0U;
    std::uint16_t end = 0;
    if (!findCStringEnd(static_cast<std::uint16_t>(operandOffset + prefix),
                        end)) {
      return false;
    }
    operandLength = end - operandOffset;
  } else if (fixedSize == kChoice) {
    std::uint16_t firstEnd = 0;
    std::uint16_t secondEnd = 0;
    if (!findCStringEnd(static_cast<std::uint16_t>(operandOffset), firstEnd) ||
        !findCStringEnd(firstEnd, secondEnd)) {
      return false;
    }
    operandLength = static_cast<std::uint32_t>(secondEnd) - operandOffset + 2U;
  } else {
    return false;
  }

  const auto next = operandOffset + operandLength;
  if (next > codeSize_ || next > std::numeric_limits<std::uint16_t>::max()) {
    return false;
  }
  command.opcode = opcode;
  command.offset = codeOffset;
  command.operandOffset = static_cast<std::uint16_t>(operandOffset);
  command.operandLength = static_cast<std::uint16_t>(operandLength);
  command.nextOffset = static_cast<std::uint16_t>(next);
  return true;
}

bool ScriptResource::eventCodeOffset(std::uint8_t eventId,
                                     std::uint16_t& codeOffset) const {
  if (source_ == nullptr || eventId == 0U || eventId > eventCount_) return false;
  std::array<std::uint8_t, 2> bytes{};
  if (!source_->read(eventTableOffset_ + (eventId - 1U) * 2U, bytes.data(),
                     bytes.size())) {
    return false;
  }
  const auto address = little16(bytes.data());
  if (address == 0U || address < headerSize_) return false;
  const auto relative = static_cast<std::uint32_t>(address) - headerSize_;
  if (relative >= codeSize_) return false;
  codeOffset = static_cast<std::uint16_t>(relative);
  return true;
}

bool ScriptResource::readCode(std::uint16_t codeOffset, void* destination,
                              std::size_t length) const {
  if (source_ == nullptr || codeOffset > codeSize_ ||
      length > static_cast<std::size_t>(codeSize_ - codeOffset)) {
    return false;
  }
  return source_->read(codeDataOffset_ + codeOffset, destination, length);
}

bool ScriptResource::readU8(std::uint16_t codeOffset,
                            std::uint8_t& value) const {
  return readCode(codeOffset, &value, 1U);
}

bool ScriptResource::readU16(std::uint16_t codeOffset,
                             std::uint16_t& value) const {
  std::array<std::uint8_t, 2> bytes{};
  if (!readCode(codeOffset, bytes.data(), bytes.size())) return false;
  value = little16(bytes.data());
  return true;
}

bool ScriptResource::readU32(std::uint16_t codeOffset,
                             std::uint32_t& value) const {
  std::array<std::uint8_t, 4> bytes{};
  if (!readCode(codeOffset, bytes.data(), bytes.size())) return false;
  value = static_cast<std::uint32_t>(bytes[0]) |
          static_cast<std::uint32_t>(bytes[1]) << 8U |
          static_cast<std::uint32_t>(bytes[2]) << 16U |
          static_cast<std::uint32_t>(bytes[3]) << 24U;
  return true;
}

const char* ScriptResource::opcodeName(std::uint8_t opcode) {
  return opcode < kOpcodeCount ? kOpcodeNames[opcode] : "INVALID";
}

}  // namespace fmj
