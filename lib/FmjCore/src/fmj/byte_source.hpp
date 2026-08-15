#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace fmj {

class ByteSource {
 public:
  virtual ~ByteSource() = default;
  virtual std::size_t size() const = 0;
  virtual bool read(std::size_t offset, void* destination,
                    std::size_t length) const = 0;

  bool readU8(std::size_t offset, std::uint8_t& value) const {
    return read(offset, &value, sizeof(value));
  }
};

class MemoryByteSource final : public ByteSource {
 public:
  MemoryByteSource(const std::uint8_t* data, std::size_t length)
      : data_(data), length_(length) {}

  std::size_t size() const override { return length_; }

  bool read(std::size_t offset, void* destination,
            std::size_t length) const override {
    if (destination == nullptr || data_ == nullptr || offset > length_ ||
        length > length_ - offset) {
      return false;
    }
    std::memcpy(destination, data_ + offset, length);
    return true;
  }

 private:
  const std::uint8_t* data_;
  std::size_t length_;
};

}  // namespace fmj

