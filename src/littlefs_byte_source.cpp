#include "littlefs_byte_source.hpp"

#include <LittleFS.h>

bool LittleFsByteSource::open(const char* path) {
  close();
  file_ = LittleFS.open(path, "r");
  if (!file_) return false;
  size_ = file_.size();
  return true;
}

void LittleFsByteSource::close() {
  if (file_) file_.close();
  size_ = 0;
}

bool LittleFsByteSource::read(std::size_t offset, void* destination,
                              std::size_t length) const {
  if (!file_ || destination == nullptr || offset > size_ ||
      length > size_ - offset || offset > UINT32_MAX) {
    return false;
  }
  if (!file_.seek(static_cast<std::uint32_t>(offset), SeekSet)) return false;
  return file_.read(static_cast<std::uint8_t*>(destination), length) == length;
}

