#include "littlefs_save_backend.hpp"

#include <FS.h>
#include <LittleFS.h>

#include <cstdio>

bool LittleFsSaveBackend::begin() {
  // Never format automatically: this partition contains the user's imported
  // ROM and font assets as well as saves. Use the explicit partition label so
  // mounting does not depend on partition enumeration order.
  ready_ = LittleFS.begin(false, "/littlefs", 10U, "littlefs");
  if (ready_ && !LittleFS.exists("/saves")) {
    ready_ = LittleFS.mkdir("/saves");
  }
  return ready_;
}

bool LittleFsSaveBackend::hasSlot(std::uint8_t slot) const {
  if (!ready_) return false;
  char path[32]{};
  makePath(path, sizeof(path), slot, ".sav");
  return LittleFS.exists(path);
}

bool LittleFsSaveBackend::load(std::uint8_t slot, void* payload,
                               std::size_t capacity, std::size_t& bytesRead) {
  bytesRead = 0;
  if (!ready_ || payload == nullptr) return false;
  char path[32]{};
  makePath(path, sizeof(path), slot, ".sav");
  auto file = LittleFS.open(path, "r");
  if (!file) return false;

  fmj::SaveHeader header{};
  if (file.read(reinterpret_cast<std::uint8_t*>(&header), sizeof(header)) !=
          sizeof(header) ||
      header.payloadSize > capacity ||
      file.size() != sizeof(header) + header.payloadSize) {
    file.close();
    return false;
  }
  bytesRead = file.read(static_cast<std::uint8_t*>(payload), header.payloadSize);
  file.close();
  return bytesRead == header.payloadSize &&
         fmj::validateSaveHeader(header, payload, bytesRead);
}

bool LittleFsSaveBackend::save(std::uint8_t slot, const void* payload,
                               std::size_t payloadSize) {
  if (!ready_ || payload == nullptr) return false;
  char current[32]{};
  char temporary[32]{};
  char backup[32]{};
  makePath(current, sizeof(current), slot, ".sav");
  makePath(temporary, sizeof(temporary), slot, ".tmp");
  makePath(backup, sizeof(backup), slot, ".bak");

  if (LittleFS.exists(temporary)) LittleFS.remove(temporary);
  auto file = LittleFS.open(temporary, "w");
  if (!file) return false;
  const auto header = fmj::makeSaveHeader(payload, payloadSize);
  const bool wrote =
      file.write(reinterpret_cast<const std::uint8_t*>(&header), sizeof(header)) ==
          sizeof(header) &&
      file.write(static_cast<const std::uint8_t*>(payload), payloadSize) ==
          payloadSize;
  file.flush();
  file.close();
  if (!wrote) {
    LittleFS.remove(temporary);
    return false;
  }

  if (LittleFS.exists(backup)) LittleFS.remove(backup);
  const bool hadCurrent = LittleFS.exists(current);
  if (hadCurrent && !LittleFS.rename(current, backup)) {
    LittleFS.remove(temporary);
    return false;
  }
  if (!LittleFS.rename(temporary, current)) {
    if (hadCurrent) LittleFS.rename(backup, current);
    LittleFS.remove(temporary);
    return false;
  }
  if (LittleFS.exists(backup)) LittleFS.remove(backup);
  return true;
}

void LittleFsSaveBackend::makePath(char* destination, std::size_t capacity,
                                   std::uint8_t slot, const char* suffix) {
  std::snprintf(destination, capacity, "/saves/fmj%u%s",
                static_cast<unsigned>(slot), suffix);
}
