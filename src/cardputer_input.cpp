#include "cardputer_input.hpp"

#include <M5Cardputer.h>

#include <cctype>

namespace {

fmj::InputKey mapCharacter(char value) {
  const auto key = static_cast<char>(
      std::tolower(static_cast<unsigned char>(value)));
  switch (key) {
    case 'w':
      return fmj::InputKey::Up;
    case ';':
      return fmj::InputKey::Up;
    case 'a':
    case '.':
      return fmj::InputKey::Down;
    case ',':
      return fmj::InputKey::Left;
    case 's':
    case '/':
      return fmj::InputKey::Right;
    case 'q':
    case '[':
      return fmj::InputKey::PageUp;
    case 'e':
    case ']':
      return fmj::InputKey::PageDown;
    case 'x':
      return fmj::InputKey::Cancel;
    case 'm':
      return fmj::InputKey::Menu;
    default:
      return fmj::InputKey::None;
  }
}

}  // namespace

fmj::InputKey CardputerInput::poll(std::uint32_t nowMs) {
  M5Cardputer.update();
  if (M5Cardputer.Keyboard.isChange()) {
    const auto current = M5Cardputer.Keyboard.isPressed()
                             ? readCurrentKey()
                             : fmj::InputKey::None;
    if (current != held_) {
      held_ = current;
      repeatAtMs_ = nowMs + 350U;
      return held_;
    }
  }

  if (held_ != fmj::InputKey::None && repeatable(held_) &&
      static_cast<std::int32_t>(nowMs - repeatAtMs_) >= 0) {
    repeatAtMs_ = nowMs + 90U;
    return held_;
  }
  return fmj::InputKey::None;
}

bool CardputerInput::repeatable(fmj::InputKey key) {
  return key == fmj::InputKey::Up || key == fmj::InputKey::Down ||
         key == fmj::InputKey::Left || key == fmj::InputKey::Right ||
         key == fmj::InputKey::PageUp || key == fmj::InputKey::PageDown;
}

std::int8_t CardputerInput::takeBrightnessDelta() {
  const auto delta = brightnessDelta_;
  brightnessDelta_ = 0;
  return delta;
}

fmj::InputKey CardputerInput::readCurrentKey() {
  const Keyboard_Class::KeysState state = M5Cardputer.Keyboard.keysState();
  if (state.enter) return fmj::InputKey::Confirm;
  if (state.del) return fmj::InputKey::Cancel;
  if (state.tab) return fmj::InputKey::Menu;
  // The Cardputer rows are staggered into a natural diamond:
  //       W
  //   Aa  A  S
  // Treat a bare Aa (Shift) press as Left, while preserving Shift+key input.
  if (state.shift && state.word.empty()) return fmj::InputKey::Left;
  for (const char value : state.word) {
    if (value == '-') {
      brightnessDelta_ = -1;
      return fmj::InputKey::None;
    }
    if (value == '=' || value == '+') {
      brightnessDelta_ = 1;
      return fmj::InputKey::None;
    }
    const auto mapped = mapCharacter(value);
    if (mapped != fmj::InputKey::None) return mapped;
  }
  return fmj::InputKey::None;
}
