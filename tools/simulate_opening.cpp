#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

#include "fmj/byte_source.hpp"
#include "fmj/firmware_app.hpp"
#include "fmj/mono_canvas.hpp"

namespace {

class MemorySaves final : public fmj::SaveBackend {
 public:
  bool hasSlot(std::uint8_t) const override { return !bytes_.empty(); }
  bool save(std::uint8_t, const void* payload, std::size_t size) override {
    const auto* first = static_cast<const std::uint8_t*>(payload);
    bytes_.assign(first, first + size);
    return true;
  }
  bool load(std::uint8_t, void* payload, std::size_t capacity,
            std::size_t& bytesRead) override {
    if (bytes_.empty() || bytes_.size() > capacity) {
      bytesRead = 0;
      return false;
    }
    std::memcpy(payload, bytes_.data(), bytes_.size());
    bytesRead = bytes_.size();
    return true;
  }

 private:
  std::vector<std::uint8_t> bytes_;
};

std::vector<std::uint8_t> readFile(const char* path) {
  std::ifstream stream(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(stream),
          std::istreambuf_iterator<char>()};
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 4 || argc > 6) {
    std::cerr << "usage: simulate_opening FMJ.LIB HZK16 ASC16 [scene.pbm] [splash.pbm]\n";
    return 2;
  }
  const auto romBytes = readFile(argv[1]);
  const auto hzkBytes = readFile(argv[2]);
  const auto ascBytes = readFile(argv[3]);
  if (romBytes.empty() || hzkBytes.empty() || ascBytes.empty()) return 2;
  fmj::MemoryByteSource rom(romBytes.data(), romBytes.size());
  fmj::MemoryByteSource hzk(hzkBytes.data(), hzkBytes.size());
  fmj::MemoryByteSource asc(ascBytes.data(), ascBytes.size());
  MemorySaves saves;
  fmj::FirmwareApp app(saves);
  fmj::MonoCanvas canvas;
  app.begin(&rom, &hzk, &asc);
  if (argc == 6) {
    app.render(canvas);
    std::ofstream splash(argv[5], std::ios::binary);
    splash << "P4\n160 96\n";
    splash.write(reinterpret_cast<const char*>(canvas.data()),
                 fmj::MonoCanvas::kBufferSize);
  }
  app.update(fmj::InputKey::Confirm);  // splash -> menu
  app.update(fmj::InputKey::Confirm);  // new game

  bool reachedFreeMovement = false;
  for (int step = 0; step < 1000; ++step) {
    app.tick(40U);
    app.update(fmj::InputKey::Confirm);
    app.render(canvas);
    if (app.chapterType() == 2U && app.chapterIndex() == 12U &&
        !app.storyBusy()) {
      reachedFreeMovement = true;
      break;
    }
  }
  std::cout << "chapter=" << static_cast<unsigned>(app.chapterType()) << ':'
            << static_cast<unsigned>(app.chapterIndex()) << " map="
            << static_cast<unsigned>(app.currentMapType()) << ':'
            << static_cast<unsigned>(app.currentMapIndex()) << " player="
            << app.playerX() << ',' << app.playerY() << " busy="
            << app.storyBusy() << '\n';
  const auto savedX = app.playerX();
  const auto savedY = app.playerY();
  app.update(fmj::InputKey::Menu);
  app.update(fmj::InputKey::Down);
  app.update(fmj::InputKey::Confirm);
  const bool restored = app.chapterType() == 2U &&
                        app.chapterIndex() == 12U &&
                        app.playerX() == savedX && app.playerY() == savedY &&
                        !app.storyBusy();
  std::cout << "save-restore=" << (restored ? "ok" : "failed") << '\n';
  if (argc == 5) {
    std::ofstream image(argv[4], std::ios::binary);
    image << "P4\n160 96\n";
    image.write(reinterpret_cast<const char*>(canvas.data()),
                fmj::MonoCanvas::kBufferSize);
  }
  return reachedFreeMovement && restored ? 0 : 1;
}
