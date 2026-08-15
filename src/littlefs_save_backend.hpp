#pragma once

#include "fmj/save_format.hpp"

class LittleFsSaveBackend final : public fmj::SaveBackend {
 public:
  bool begin();
  bool hasSlot(std::uint8_t slot) const override;
  bool load(std::uint8_t slot, void* payload, std::size_t capacity,
            std::size_t& bytesRead) override;
  bool save(std::uint8_t slot, const void* payload,
            std::size_t payloadSize) override;

 private:
  static void makePath(char* destination, std::size_t capacity,
                       std::uint8_t slot, const char* suffix);
  bool ready_ = false;
};

