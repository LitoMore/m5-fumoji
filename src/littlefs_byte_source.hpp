#pragma once

#include <FS.h>

#include <cstddef>

#include "fmj/byte_source.hpp"

class LittleFsByteSource final : public fmj::ByteSource {
 public:
  bool open(const char* path);
  void close();
  std::size_t size() const override { return size_; }
  bool read(std::size_t offset, void* destination,
            std::size_t length) const override;

 private:
  mutable fs::File file_;
  std::size_t size_ = 0;
};

