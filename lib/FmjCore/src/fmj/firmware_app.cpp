#include "fmj/firmware_app.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>

#include "fmj/tiny_font.hpp"

namespace fmj {
namespace {

constexpr const char* kMenuItems[] = {"NEW GAME", "LOAD GAME", "ROM STATUS"};
constexpr std::size_t kMenuItemCount = sizeof(kMenuItems) / sizeof(kMenuItems[0]);
constexpr std::array<std::uint8_t, 4> kStandingFrame = {0U, 3U, 6U, 9U};
constexpr std::array<std::uint8_t, 4> kStepOffset = {0U, 1U, 2U, 1U};

std::uint16_t commandU16(const ScriptResource& script,
                         const ScriptCommand& command, std::uint16_t offset) {
  std::uint16_t value = 0;
  script.readU16(static_cast<std::uint16_t>(command.operandOffset + offset),
                 value);
  return value;
}

std::uint32_t commandU32(const ScriptResource& script,
                         const ScriptCommand& command, std::uint16_t offset) {
  std::uint32_t value = 0;
  script.readU32(static_cast<std::uint16_t>(command.operandOffset + offset),
                 value);
  return value;
}

}  // namespace

void FirmwareApp::begin(const ByteSource* rom, const ByteSource* hzk16,
                        const ByteSource* asc16) {
  rom_ = nullptr;
  sceneReady_ = false;
  resources_.close();
  font_.open(hzk16, asc16);
  if (rom != nullptr && resources_.open(*rom)) {
    rom_ = rom;
    screen_ = Screen::Splash;
  } else {
    screen_ = Screen::Menu;
  }
}

bool FirmwareApp::update(InputKey key) {
  if (key == InputKey::None) return false;
  switch (screen_) {
    case Screen::Splash:
      screen_ = Screen::Menu;
      return true;
    case Screen::Menu:
      if (key == InputKey::Up) {
        menuIndex_ = static_cast<std::uint8_t>(
            (menuIndex_ + kMenuItemCount - 1U) % kMenuItemCount);
      } else if (key == InputKey::Down) {
        menuIndex_ = static_cast<std::uint8_t>(
            (menuIndex_ + 1U) % kMenuItemCount);
      } else if (key == InputKey::Confirm) {
        if (menuIndex_ == 0U) {
          startNewGame();
        } else if (menuIndex_ == 1U) {
          loadGame();
        } else {
          screen_ = Screen::RomStatus;
        }
      }
      return true;
    case Screen::RomStatus:
      if (key == InputKey::Cancel || key == InputKey::Confirm ||
          key == InputKey::Menu) {
        screen_ = Screen::Menu;
      }
      return true;
    case Screen::Game:
      break;
  }

  if (waitState_ == WaitState::Dialogue) {
    if (key == InputKey::Confirm || key == InputKey::Cancel) {
      if (!nextDialoguePage()) {
        waitState_ = WaitState::None;
        processScript();
      }
    }
    return true;
  }
  if (waitState_ == WaitState::Movie) {
    if ((movieControl_ == 1U || movieControl_ == 3U) &&
        (key == InputKey::Confirm || key == InputKey::Cancel)) {
      movieLayers_.fill(MovieLayer{});
      waitState_ = WaitState::None;
      processScript();
    }
    return true;
  }
  if (waitState_ != WaitState::None || scriptRunning_) return true;

  Direction direction = playerActor_.direction;
  bool movement = true;
  if (key == InputKey::Up) direction = Direction::North;
  else if (key == InputKey::Down) direction = Direction::South;
  else if (key == InputKey::Left) direction = Direction::West;
  else if (key == InputKey::Right) direction = Direction::East;
  else movement = false;

  if (movement && playerActor_.active) {
    movePlayer(direction);
    return true;
  }
  if (key == InputKey::Confirm && playerActor_.active) {
    auto x = playerActor_.x;
    auto y = playerActor_.y;
    if (playerActor_.direction == Direction::North) --y;
    if (playerActor_.direction == Direction::South) ++y;
    if (playerActor_.direction == Direction::West) --x;
    if (playerActor_.direction == Direction::East) ++x;
    for (std::uint8_t id = 1; id < npcs_.size(); ++id) {
      if (npcs_[id].active && npcs_[id].x == x && npcs_[id].y == y) {
        triggerEvent(id);
        break;
      }
    }
    return true;
  }
  if (key == InputKey::Menu || key == InputKey::Cancel) {
    saveGame();
    screen_ = Screen::Menu;
    return true;
  }
  return false;
}

bool FirmwareApp::tick(std::uint32_t deltaMs) {
  if (screen_ != Screen::Game) return false;
  if (waitState_ == WaitState::Movie) return advanceMovie(deltaMs);
  if (waitState_ == WaitState::Move) return advanceMove(deltaMs);
  if (waitState_ == WaitState::Pose || waitState_ == WaitState::SceneName) {
    waitElapsedMs_ += deltaMs;
    const auto duration = waitState_ == WaitState::Pose ? 300U : 1000U;
    if (waitElapsedMs_ >= duration) {
      waitState_ = WaitState::None;
      waitElapsedMs_ = 0;
      processScript();
    }
    return true;
  }
  if (scriptRunning_ && waitState_ == WaitState::None) {
    return processScript();
  }
  return false;
}

void FirmwareApp::render(MonoCanvas& canvas) const {
  canvas.clear(false);
  switch (screen_) {
    case Screen::Splash: renderSplash(canvas); break;
    case Screen::Menu: renderMenu(canvas); break;
    case Screen::Game: renderGame(canvas); break;
    case Screen::RomStatus: renderRomStatus(canvas); break;
  }
}

void FirmwareApp::renderSplash(MonoCanvas& canvas) const {
  static constexpr std::uint8_t kTitle[] = {
      0xB7U, 0xFCU,  // 伏
      0xC4U, 0xA7U,  // 魔
      0xBCU, 0xC7U,  // 记
  };
  canvas.rect(1, 1, 158, 94);
  if (font_.valid()) {
    font_.drawBytes(canvas, kTitle, sizeof(kTitle), 56, 22);
  } else {
    TinyFont::drawText(canvas, "FMJ", 50, 22, true, 4);
  }
  canvas.hLine(30, 43, 100);
  TinyFont::drawText(canvas, "CARDPUTER ADV", 45, 55);
  TinyFont::drawText(canvas, "PRESS ENTER", 52, 74);
}

void FirmwareApp::renderMenu(MonoCanvas& canvas) const {
  canvas.rect(1, 1, 158, 94);
  TinyFont::drawText(canvas, "FMJ CARDPUTER", 28, 10, true, 2);
  canvas.hLine(12, 25, 136);
  for (std::size_t i = 0; i < kMenuItemCount; ++i) {
    const int y = 36 + static_cast<int>(i) * 15;
    if (i == menuIndex_) {
      canvas.fillRect(24, y - 4, 112, 13, true);
      TinyFont::drawText(canvas, kMenuItems[i], 40, y, false);
      TinyFont::drawText(canvas, ">", 29, y, false);
    } else {
      TinyFont::drawText(canvas, kMenuItems[i], 40, y);
    }
  }
  TinyFont::drawText(canvas, romAvailable() ? "ROM OK" : "ROM MISSING", 4,
                     87);
}

void FirmwareApp::renderGame(MonoCanvas& canvas) const {
  renderScene(canvas);
  if (waitState_ == WaitState::Movie) renderMovie(canvas);
  if (waitState_ == WaitState::Dialogue) renderDialogue(canvas);
  if (waitState_ == WaitState::SceneName) renderSceneName(canvas);
}

void FirmwareApp::renderScene(MonoCanvas& canvas) const {
  if (!sceneReady_) {
    TinyFont::drawText(canvas, "SCRIPT SCENE ERROR", 28, 44);
    return;
  }
  for (int sy = 0; sy < 6; ++sy) {
    for (int sx = 0; sx < 10; ++sx) {
      std::uint8_t tile = 0;
      std::uint8_t event = 0;
      bool walkable = false;
      if (map_.tile(mapLeft_ + sx, mapTop_ + sy, tile, walkable, event)) {
        tiles_.drawSlice(canvas, tile, sx * 16, sy * 16);
      }
    }
  }
  for (int y = mapTop_ - 2; y <= mapTop_ + 5; ++y) {
    for (std::size_t id = 1; id < npcs_.size(); ++id) {
      if (npcs_[id].active && npcs_[id].y == y) renderActor(canvas, npcs_[id]);
    }
    if (playerActor_.active && playerActor_.y == y) {
      renderActor(canvas, playerActor_);
    }
  }
}

void FirmwareApp::renderActor(MonoCanvas& canvas,
                              const SceneActor& actor) const {
  const auto direction = static_cast<std::size_t>(actor.direction);
  const auto slice = static_cast<std::uint8_t>(
      kStandingFrame[direction] + kStepOffset[actor.step & 3U]);
  const int x = (actor.x - mapLeft_) * 16;
  const int y = (actor.y - mapTop_) * 16 + 16 - actor.sprite.height();
  if (!actor.sprite.drawSlice(canvas, slice, x, y)) {
    canvas.fillRect(x + 4, (actor.y - mapTop_) * 16 + 3, 8, 12, true);
  }
}

void FirmwareApp::renderDialogue(MonoCanvas& canvas) const {
  const bool hasPortrait = dialoguePortrait_ != 0U && portrait_.valid();
  canvas.fillRect(2, 48, 156, 47, false);
  canvas.rect(2, 48, 156, 47, true);
  if (hasPortrait) portrait_.drawSlice(canvas, 0U, 7, 53);

  std::size_t cursor = dialoguePageOffset_;
  for (int line = 0; line < 2 && cursor < dialogueLength_; ++line) {
    int x = hasPortrait && line == 0 ? 38 : 7;
    const int y = line == 0 ? 53 : 73;
    const int maxX = 153;
    while (cursor < dialogueLength_ && dialogue_[cursor] != 0U) {
      const auto length = dialogue_[cursor] >= 0xA1U &&
                                  cursor + 1U < dialogueLength_
                              ? 2U
                              : 1U;
      const int width = length == 2U ? 16 : 8;
      if (x + width > maxX) break;
      font_.drawBytes(canvas, dialogue_.data() + cursor, length, x, y);
      x += width;
      cursor += length;
    }
  }
  TinyFont::drawText(canvas, ">", 151, 88);
}

void FirmwareApp::renderSceneName(MonoCanvas& canvas) const {
  int byteLength = 0;
  while (byteLength < static_cast<int>(sceneName_.size()) &&
         sceneName_[byteLength] != 0U) {
    ++byteLength;
  }
  int pixels = 0;
  for (int i = 0; i < byteLength;) {
    if (sceneName_[i] >= 0xA1U && i + 1 < byteLength) {
      pixels += 16;
      i += 2;
    } else {
      pixels += 8;
      ++i;
    }
  }
  const int x = std::max(2, (160 - pixels) / 2);
  canvas.fillRect(x - 3, 37, pixels + 6, 22, false);
  canvas.rect(x - 3, 37, pixels + 6, 22, true);
  font_.drawBytes(canvas, sceneName_.data(), byteLength, x, 40);
}

void FirmwareApp::renderMovie(MonoCanvas& canvas) const {
  for (const auto& layer : movieLayers_) {
    if (layer.active) movie_.drawFrame(canvas, layer.frame, movieX_, movieY_);
  }
}

void FirmwareApp::renderRomStatus(MonoCanvas& canvas) const {
  canvas.rect(5, 5, 150, 86);
  TinyFont::drawText(canvas, "RESOURCE STATUS", 38, 14);
  canvas.hLine(18, 24, 124);
  TinyFont::drawText(canvas,
                     romAvailable() ? "FMJ.LIB VALID" : "FMJ.LIB NOT FOUND",
                     16, 36);
  char count[28]{};
  std::snprintf(count, sizeof(count), "INDEXED %lu RESOURCES",
                static_cast<unsigned long>(resourceCount()));
  TinyFont::drawText(canvas, count, 16, 49);
  TinyFont::drawText(canvas, "GUT STORY ENGINE", 16, 62);
  TinyFont::drawText(canvas, "DEL TO RETURN", 16, 77);
}

void FirmwareApp::startNewGame() {
  player_ = PlayerState{};
  events_.fill(0U);
  variables_.fill(0U);
  clearSceneActors();
  sceneReady_ = false;
  waitState_ = WaitState::None;
  screen_ = Screen::Game;
  if (!startChapter(1U, 1U)) {
    initializeFallbackScene();
  } else {
    processScript();
  }
}

void FirmwareApp::loadGame() {
  std::size_t bytesRead = 0;
  saveBuffer_ = GameSaveState{};
  if (!saves_.load(0U, &saveBuffer_, sizeof(saveBuffer_), bytesRead) ||
      bytesRead != sizeof(saveBuffer_) ||
      saveBuffer_.magic != GameSaveState{}.magic ||
      saveBuffer_.version != GameSaveState{}.version) {
    startNewGame();
    return;
  }
  screen_ = Screen::Game;
  waitState_ = WaitState::None;
  scriptRunning_ = false;
  events_ = saveBuffer_.events;
  variables_ = saveBuffer_.variables;
  sceneName_ = saveBuffer_.sceneName;
  player_.steps = saveBuffer_.steps;
  clearSceneActors();
  if (!loadMap(saveBuffer_.mapType, saveBuffer_.mapIndex,
               static_cast<std::uint16_t>(saveBuffer_.mapLeft),
               static_cast<std::uint16_t>(saveBuffer_.mapTop))) {
    startNewGame();
    return;
  }
  restoreActor(playerActor_, saveBuffer_.player, 0U);
  for (std::size_t id = 1; id < npcs_.size(); ++id) {
    restoreActor(npcs_[id], saveBuffer_.npcs[id],
                 static_cast<std::uint8_t>(id));
  }
  const auto* entry = resources_.find(ResourceType::Script,
                                      saveBuffer_.scriptType,
                                      saveBuffer_.scriptIndex);
  if (entry == nullptr || rom_ == nullptr || !script_.open(*rom_, entry->offset)) {
    startNewGame();
    return;
  }
  scriptType_ = saveBuffer_.scriptType;
  scriptIndex_ = saveBuffer_.scriptIndex;
}

void FirmwareApp::saveGame() {
  if (!sceneReady_ || !script_.valid() || storyBusy()) return;
  saveBuffer_ = GameSaveState{};
  saveBuffer_.mapType = mapType_;
  saveBuffer_.mapIndex = mapIndex_;
  saveBuffer_.mapLeft = mapLeft_;
  saveBuffer_.mapTop = mapTop_;
  saveBuffer_.scriptType = scriptType_;
  saveBuffer_.scriptIndex = scriptIndex_;
  saveBuffer_.steps = player_.steps;
  saveBuffer_.player = persistActor(playerActor_);
  for (std::size_t id = 1; id < npcs_.size(); ++id) {
    saveBuffer_.npcs[id] = persistActor(npcs_[id]);
  }
  saveBuffer_.events = events_;
  saveBuffer_.variables = variables_;
  saveBuffer_.sceneName = sceneName_;
  saves_.save(0U, &saveBuffer_, sizeof(saveBuffer_));
}

FirmwareApp::PersistedActor FirmwareApp::persistActor(
    const SceneActor& actor) {
  PersistedActor persisted;
  persisted.active = actor.active ? 1U : 0U;
  persisted.actorType = actor.actorType;
  persisted.actorIndex = actor.actorIndex;
  persisted.x = actor.x;
  persisted.y = actor.y;
  persisted.direction = static_cast<std::uint8_t>(actor.direction);
  persisted.step = actor.step;
  return persisted;
}

bool FirmwareApp::restoreActor(SceneActor& actor,
                               const PersistedActor& persisted,
                               std::uint8_t id) {
  if (persisted.active == 0U) {
    actor = SceneActor{};
    return true;
  }
  if (!loadActor(actor, persisted.actorType, persisted.actorIndex, id,
                 persisted.x, persisted.y)) {
    return false;
  }
  actor.direction = static_cast<Direction>(persisted.direction & 3U);
  actor.step = persisted.step & 3U;
  return true;
}

void FirmwareApp::initializeFallbackScene() {
  if (rom_ == nullptr) return;
  const auto* entry = resources_.first(ResourceType::Map);
  if (entry == nullptr || !map_.open(*rom_, entry->offset)) return;
  const auto* tile = resources_.find(ResourceType::Tile, 1U, map_.tileSetIndex());
  if (tile == nullptr || !tiles_.open(*rom_, tile->offset)) return;
  mapLeft_ = 0;
  mapTop_ = 0;
  sceneReady_ = true;
  for (int y = 0; y < map_.height(); ++y) {
    for (int x = 0; x < map_.width(); ++x) {
      std::uint8_t tileIndex = 0;
      std::uint8_t event = 0;
      bool walkable = false;
      if (map_.tile(x, y, tileIndex, walkable, event) && walkable) {
        playerActor_.active = true;
        playerActor_.x = x;
        playerActor_.y = y;
        return;
      }
    }
  }
}

void FirmwareApp::clearSceneActors() {
  playerActor_ = SceneActor{};
  npcs_.fill(SceneActor{});
}

bool FirmwareApp::loadMap(std::uint8_t type, std::uint8_t index,
                          std::uint16_t left, std::uint16_t top) {
  if (rom_ == nullptr) return false;
  const auto* entry = resources_.find(ResourceType::Map, type, index);
  if (entry == nullptr || !map_.open(*rom_, entry->offset)) return false;
  const auto* tile = resources_.find(ResourceType::Tile, 1U, map_.tileSetIndex());
  if (tile == nullptr || !tiles_.open(*rom_, tile->offset) ||
      tiles_.width() != 16U || tiles_.height() != 16U) {
    return false;
  }
  mapType_ = type;
  mapIndex_ = index;
  mapLeft_ = static_cast<std::int16_t>(left);
  mapTop_ = static_cast<std::int16_t>(top);
  sceneReady_ = true;
  if (playerActor_.active) {
    playerActor_.x = mapLeft_ + 4;
    playerActor_.y = mapTop_ + 3;
  }
  return true;
}

bool FirmwareApp::loadActor(SceneActor& actor, std::uint8_t actorType,
                            std::uint8_t actorIndex, std::uint8_t id,
                            std::int16_t x, std::int16_t y) {
  if (rom_ == nullptr) return false;
  const auto* entry = resources_.find(ResourceType::Actor, actorType, actorIndex);
  if (entry == nullptr) return false;
  std::array<std::uint8_t, 0x17> header{};
  if (!rom_->read(entry->offset, header.data(), header.size())) return false;

  SceneActor loaded;
  loaded.active = true;
  loaded.id = id;
  loaded.actorType = actorType;
  loaded.actorIndex = actorIndex;
  loaded.x = x;
  loaded.y = y;
  loaded.direction = header[2] >= 1U && header[2] <= 4U
                         ? static_cast<Direction>(header[2] - 1U)
                         : Direction::South;
  loaded.step = header[3] & 3U;
  loaded.spriteId = header[0x16U];
  const auto* sprite = resources_.find(ResourceType::ActorPicture, actorType,
                                       loaded.spriteId);
  if (sprite != nullptr) loaded.sprite.open(*rom_, sprite->offset);
  actor = loaded;
  return true;
}

bool FirmwareApp::createPlayer(std::uint8_t actorIndex, std::uint16_t x,
                               std::uint16_t y) {
  return loadActor(playerActor_, 1U, actorIndex, 0U,
                   static_cast<std::int16_t>(mapLeft_ + x),
                   static_cast<std::int16_t>(mapTop_ + y));
}

bool FirmwareApp::createNpc(std::uint8_t id, std::uint8_t actorType,
                            std::uint8_t actorIndex, std::uint16_t x,
                            std::uint16_t y) {
  if (id == 0U || id >= npcs_.size()) return false;
  return loadActor(npcs_[id], actorType, actorIndex, id,
                   static_cast<std::int16_t>(x), static_cast<std::int16_t>(y));
}

void FirmwareApp::deleteNpc(std::uint8_t id) {
  if (id > 0U && id < npcs_.size()) npcs_[id] = SceneActor{};
}

FirmwareApp::SceneActor* FirmwareApp::actorById(std::uint8_t id) {
  if (id == 0U) return playerActor_.active ? &playerActor_ : nullptr;
  return id < npcs_.size() && npcs_[id].active ? &npcs_[id] : nullptr;
}

const FirmwareApp::SceneActor* FirmwareApp::actorAt(std::int16_t x,
                                                     std::int16_t y) const {
  for (std::size_t id = 1; id < npcs_.size(); ++id) {
    if (npcs_[id].active && npcs_[id].x == x && npcs_[id].y == y) {
      return &npcs_[id];
    }
  }
  return nullptr;
}

bool FirmwareApp::movePlayer(Direction direction) {
  playerActor_.direction = direction;
  auto x = playerActor_.x;
  auto y = playerActor_.y;
  if (direction == Direction::North) --y;
  if (direction == Direction::South) ++y;
  if (direction == Direction::West) --x;
  if (direction == Direction::East) ++x;

  std::uint8_t tile = 0;
  std::uint8_t event = 0;
  bool walkable = false;
  if (!map_.tile(x, y, tile, walkable, event)) return false;
  if (event != 0U) triggerEvent(static_cast<std::uint8_t>(event + 40U));
  if (!walkable || actorAt(x, y) != nullptr) return false;
  playerActor_.x = x;
  playerActor_.y = y;
  playerActor_.step = static_cast<std::uint8_t>((playerActor_.step + 1U) & 3U);
  player_.x = static_cast<std::uint8_t>(x);
  player_.y = static_cast<std::uint8_t>(y);
  ++player_.steps;
  keepPlayerInViewport();
  return true;
}

void FirmwareApp::keepPlayerInViewport() {
  if (!sceneReady_) return;
  if (playerActor_.x - mapLeft_ < 4) --mapLeft_;
  if (playerActor_.x - mapLeft_ > 5) ++mapLeft_;
  if (playerActor_.y - mapTop_ < 2) --mapTop_;
  if (playerActor_.y - mapTop_ > 3) ++mapTop_;
  mapLeft_ = std::clamp<std::int16_t>(mapLeft_, 0,
      std::max<std::int16_t>(0, static_cast<std::int16_t>(map_.width()) - 10));
  mapTop_ = std::clamp<std::int16_t>(mapTop_, 0,
      std::max<std::int16_t>(0, static_cast<std::int16_t>(map_.height()) - 6));
}

bool FirmwareApp::startChapter(std::uint8_t type, std::uint8_t index) {
  if (rom_ == nullptr) return false;
  const auto* entry = resources_.find(ResourceType::Script, type, index);
  if (entry == nullptr || !script_.open(*rom_, entry->offset)) return false;
  npcs_.fill(SceneActor{});
  scriptType_ = type;
  scriptIndex_ = index;
  scriptPc_ = 0;
  scriptRunning_ = true;
  waitState_ = WaitState::None;
  return true;
}

bool FirmwareApp::triggerEvent(std::uint8_t eventId) {
  if (!script_.valid() || waitState_ != WaitState::None) return false;
  std::uint16_t offset = 0;
  if (!script_.eventCodeOffset(eventId, offset)) return false;
  scriptPc_ = offset;
  scriptRunning_ = true;
  return processScript();
}

bool FirmwareApp::jumpAddress(std::uint16_t address) {
  if (address < script_.headerSize()) return false;
  const auto target = static_cast<std::uint32_t>(address) - script_.headerSize();
  if (target >= script_.codeSize()) return false;
  scriptPc_ = static_cast<std::uint16_t>(target);
  return true;
}

bool FirmwareApp::processScript() {
  bool changed = false;
  for (int budget = 0; scriptRunning_ && waitState_ == WaitState::None &&
                       budget < 96; ++budget) {
    if (scriptPc_ >= script_.codeSize()) {
      scriptRunning_ = false;
      break;
    }
    ScriptCommand command;
    if (!script_.decode(scriptPc_, command)) {
      scriptRunning_ = false;
      break;
    }
    scriptPc_ = command.nextOffset;
    changed = true;
    switch (command.opcode) {
      case 1:  // LOADMAP
        loadMap(static_cast<std::uint8_t>(commandU16(script_, command, 0)),
                static_cast<std::uint8_t>(commandU16(script_, command, 2)),
                commandU16(script_, command, 4), commandU16(script_, command, 6));
        break;
      case 2:  // CREATEACTOR
        createPlayer(static_cast<std::uint8_t>(commandU16(script_, command, 0)),
                     commandU16(script_, command, 2),
                     commandU16(script_, command, 4));
        break;
      case 3: deleteNpc(static_cast<std::uint8_t>(commandU16(script_, command, 0))); break;
      case 6:  // MOVE
        moveActorId_ = static_cast<std::uint8_t>(commandU16(script_, command, 0));
        moveTargetX_ = static_cast<std::int16_t>(commandU16(script_, command, 2));
        moveTargetY_ = static_cast<std::int16_t>(commandU16(script_, command, 4));
        waitElapsedMs_ = 100U;
        waitState_ = WaitState::Move;
        break;
      case 9:  // CALLBACK
        scriptRunning_ = false;
        break;
      case 10: jumpAddress(commandU16(script_, command, 0)); break;
      case 11:
        if (eventValue(commandU16(script_, command, 0))) {
          jumpAddress(commandU16(script_, command, 2));
        }
        break;
      case 12: setVariable(commandU16(script_, command, 0),
                           commandU16(script_, command, 2)); break;
      case 13:
        beginDialogue(command, 2U, commandU16(script_, command, 0));
        break;
      case 14: {
        const auto type = static_cast<std::uint8_t>(commandU16(script_, command, 0));
        const auto index = static_cast<std::uint8_t>(commandU16(script_, command, 2));
        startChapter(type, index);
        break;
      }
      case 16:
        mapLeft_ = static_cast<std::int16_t>(commandU16(script_, command, 0));
        mapTop_ = static_cast<std::int16_t>(commandU16(script_, command, 2));
        break;
      case 20: screen_ = Screen::Menu; scriptRunning_ = false; break;
      case 21:
        if (variable(commandU16(script_, command, 0)) ==
            commandU16(script_, command, 2)) {
          jumpAddress(commandU16(script_, command, 4));
        }
        break;
      case 22:
        setVariable(commandU16(script_, command, 0),
                    variable(commandU16(script_, command, 0)) +
                        commandU16(script_, command, 2));
        break;
      case 23:
        setVariable(commandU16(script_, command, 0),
                    variable(commandU16(script_, command, 0)) -
                        commandU16(script_, command, 2));
        break;
      case 26: setEventValue(commandU16(script_, command, 0), true); break;
      case 27: setEventValue(commandU16(script_, command, 0), false); break;
      case 30:
        beginMovie(commandU16(script_, command, 0),
                   commandU16(script_, command, 2),
                   commandU16(script_, command, 4),
                   commandU16(script_, command, 6),
                   commandU16(script_, command, 8));
        break;
      case 32:
        createNpc(static_cast<std::uint8_t>(commandU16(script_, command, 0)), 4U,
                  static_cast<std::uint8_t>(commandU16(script_, command, 2)),
                  commandU16(script_, command, 4), commandU16(script_, command, 6));
        break;
      case 33: deleteNpc(static_cast<std::uint8_t>(commandU16(script_, command, 0))); break;
      case 38:
        createNpc(static_cast<std::uint8_t>(commandU16(script_, command, 0)), 2U,
                  static_cast<std::uint8_t>(commandU16(script_, command, 2)),
                  commandU16(script_, command, 4), commandU16(script_, command, 6));
        break;
      case 40:
        if (playerActor_.actorIndex == commandU16(script_, command, 0)) {
          playerActor_ = SceneActor{};
        }
        break;
      case 43: player_.steps = commandU32(script_, command, 0); break;
      case 47: beginDialogue(command, 0U, 0U); break;
      case 52: npcs_.fill(SceneActor{}); break;
      case 53: {
        auto* actor = actorById(static_cast<std::uint8_t>(commandU16(script_, command, 0)));
        if (actor != nullptr) {
          actor->direction = static_cast<Direction>(commandU16(script_, command, 2) & 3U);
          actor->step = static_cast<std::uint8_t>(commandU16(script_, command, 4) & 3U);
        }
        waitElapsedMs_ = 0;
        waitState_ = WaitState::Pose;
        break;
      }
      case 54: {
        sceneName_.fill(0U);
        const auto length = std::min<std::size_t>(command.operandLength,
                                                  sceneName_.size() - 1U);
        script_.readCode(command.operandOffset, sceneName_.data(), length);
        break;
      }
      case 55:
        waitElapsedMs_ = 0;
        waitState_ = WaitState::SceneName;
        break;
      case 65:
        if (player_.steps < commandU32(script_, command, 0)) {
          jumpAddress(commandU16(script_, command, 4));
        }
        break;
      case 67: {
        const auto value = variable(commandU16(script_, command, 0));
        const auto other = commandU16(script_, command, 2);
        if (value < other) jumpAddress(commandU16(script_, command, 4));
        else if (value > other) jumpAddress(commandU16(script_, command, 6));
        break;
      }
      case 68: scriptRunning_ = false; break;
      case 69:
        beginDialogue(command, 2U, 0U);
        break;
      case 76:
        setVariable(commandU16(script_, command, 2),
                    variable(commandU16(script_, command, 0)));
        break;
      default:
        // The decoder understands the complete 0..79 format. Commands outside
        // the current exploration/story milestone are safely skipped here.
        break;
    }
  }
  return changed;
}

bool FirmwareApp::beginDialogue(const ScriptCommand& command,
                                std::uint16_t prefix,
                                std::uint16_t portrait) {
  dialogue_.fill(0U);
  if (command.operandLength <= prefix) return false;
  dialogueLength_ = std::min<std::size_t>(command.operandLength - prefix,
                                          dialogue_.size() - 1U);
  if (!script_.readCode(static_cast<std::uint16_t>(command.operandOffset + prefix),
                        dialogue_.data(), dialogueLength_)) {
    dialogueLength_ = 0;
    return false;
  }
  if (dialogueLength_ > 0U && dialogue_[dialogueLength_ - 1U] == 0U) {
    --dialogueLength_;
  }
  dialoguePageOffset_ = 0;
  dialoguePortrait_ = portrait;
  portrait_ = ImageResource{};
  if (portrait != 0U && rom_ != nullptr) {
    const auto* entry = resources_.find(ResourceType::Picture, 1U,
                                        static_cast<std::uint8_t>(portrait));
    if (entry != nullptr) portrait_.open(*rom_, entry->offset);
  }
  waitState_ = WaitState::Dialogue;
  return true;
}

std::size_t FirmwareApp::dialoguePageEnd(std::size_t start,
                                         std::uint8_t firstLineUnits,
                                         std::uint8_t secondLineUnits) const {
  auto cursor = start;
  for (const auto capacity : {firstLineUnits, secondLineUnits}) {
    std::uint8_t used = 0;
    while (cursor < dialogueLength_) {
      const bool doubleByte = dialogue_[cursor] >= 0xA1U &&
                              cursor + 1U < dialogueLength_;
      const std::uint8_t units = doubleByte ? 2U : 1U;
      if (used + units > capacity) break;
      used = static_cast<std::uint8_t>(used + units);
      cursor += doubleByte ? 2U : 1U;
    }
  }
  return cursor;
}

bool FirmwareApp::nextDialoguePage() {
  const auto next = dialoguePageEnd(dialoguePageOffset_,
                                     dialoguePortrait_ != 0U ? 14U : 18U,
                                     18U);
  if (next >= dialogueLength_) return false;
  dialoguePageOffset_ = next;
  return true;
}

bool FirmwareApp::beginMovie(std::uint16_t type, std::uint16_t index,
                             std::uint16_t x, std::uint16_t y,
                             std::uint16_t control) {
  if (rom_ == nullptr) return false;
  const auto* entry = resources_.find(ResourceType::Effect,
                                      static_cast<std::uint8_t>(type),
                                      static_cast<std::uint8_t>(index));
  if (entry == nullptr || !movie_.open(*rom_, entry->offset)) return false;
  movieLayers_.fill(MovieLayer{});
  movieLayers_[0] = MovieLayer{true, 0U,
                               static_cast<std::int16_t>(movie_.frameShow(0U)),
                               static_cast<std::int16_t>(movie_.frameNextShow(0U))};
  movieX_ = static_cast<std::int16_t>(x);
  movieY_ = static_cast<std::int16_t>(y);
  movieControl_ = static_cast<std::uint8_t>(control);
  movieAccumulatorMs_ = 0;
  waitState_ = WaitState::Movie;
  return true;
}

bool FirmwareApp::advanceMovie(std::uint32_t deltaMs) {
  movieAccumulatorMs_ += deltaMs;
  bool changed = false;
  while (movieAccumulatorMs_ >= 40U) {
    movieAccumulatorMs_ -= 40U;
    changed = true;
    for (int iteration = 0; iteration < 5; ++iteration) {
      for (auto& layer : movieLayers_) {
        if (!layer.active) continue;
        --layer.show;
        --layer.nextShow;
        if (layer.nextShow == 0 && layer.frame + 1U < movie_.frameCount()) {
          for (auto& candidate : movieLayers_) {
            if (!candidate.active) {
              const auto next = static_cast<std::uint8_t>(layer.frame + 1U);
              candidate = MovieLayer{
                  true, next, static_cast<std::int16_t>(movie_.frameShow(next)),
                  static_cast<std::int16_t>(movie_.frameNextShow(next))};
              break;
            }
          }
        }
      }
      for (auto& layer : movieLayers_) {
        if (layer.active && layer.show <= 0) layer.active = false;
      }
    }
  }
  const bool any = std::any_of(movieLayers_.begin(), movieLayers_.end(),
                               [](const MovieLayer& layer) { return layer.active; });
  if (!any) {
    waitState_ = WaitState::None;
    processScript();
    return true;
  }
  return changed;
}

bool FirmwareApp::advanceMove(std::uint32_t deltaMs) {
  waitElapsedMs_ += deltaMs;
  if (waitElapsedMs_ < 100U) return false;
  waitElapsedMs_ -= 100U;
  auto* actor = actorById(moveActorId_);
  if (actor == nullptr) {
    waitState_ = WaitState::None;
    processScript();
    return true;
  }
  if (actor->x < moveTargetX_) {
    ++actor->x;
    actor->direction = Direction::East;
  } else if (actor->x > moveTargetX_) {
    --actor->x;
    actor->direction = Direction::West;
  } else if (actor->y < moveTargetY_) {
    ++actor->y;
    actor->direction = Direction::South;
  } else if (actor->y > moveTargetY_) {
    --actor->y;
    actor->direction = Direction::North;
  } else {
    waitState_ = WaitState::None;
    processScript();
    return true;
  }
  actor->step = static_cast<std::uint8_t>((actor->step + 1U) & 3U);
  if (actor->x == moveTargetX_ && actor->y == moveTargetY_) {
    waitState_ = WaitState::None;
    processScript();
  }
  return true;
}

bool FirmwareApp::eventValue(std::uint16_t id) const {
  const auto byte = static_cast<std::size_t>(id >> 3U);
  return byte < events_.size() &&
         (events_[byte] & static_cast<std::uint8_t>(1U << (id & 7U))) != 0U;
}

void FirmwareApp::setEventValue(std::uint16_t id, bool value) {
  const auto byte = static_cast<std::size_t>(id >> 3U);
  if (byte >= events_.size()) return;
  const auto mask = static_cast<std::uint8_t>(1U << (id & 7U));
  if (value) events_[byte] |= mask;
  else events_[byte] &= static_cast<std::uint8_t>(~mask);
}

std::uint16_t FirmwareApp::variable(std::uint16_t id) const {
  return id < variables_.size() ? variables_[id] : 0U;
}

void FirmwareApp::setVariable(std::uint16_t id, std::uint16_t value) {
  if (id < variables_.size()) variables_[id] = value;
}

}  // namespace fmj
