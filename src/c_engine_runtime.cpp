/* Copyright (C) 2026 LitoMore; SPDX-License-Identifier: GPL-2.0-only */
#include "c_engine_runtime.hpp"

#include <Arduino.h>
#include <LittleFS.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace {

constexpr std::uint8_t kKeyHomeMenu = 0x01;
constexpr std::uint8_t kKeyEnter = 0x2F;
constexpr std::uint8_t kKeyExit = 0x2E;
constexpr std::uint8_t kKeyUp = 0x35;
constexpr std::uint8_t kKeyLeft = 0x37;
constexpr std::uint8_t kKeyDown = 0x38;
constexpr std::uint8_t kKeyRight = 0x39;
constexpr std::uint8_t kKeyPageUp = 0x3A;
constexpr std::uint8_t kKeyPageDown = 0x3B;
constexpr std::uint8_t kReadOnly = 0x01;
constexpr std::uint8_t kFromCurrent = 0x02;
constexpr std::uint8_t kFromEnd = 0x03;

std::uint8_t toEngineKey(fmj::InputKey key) {
  switch (key) {
    case fmj::InputKey::Up:
      return kKeyUp;
    case fmj::InputKey::Down:
      return kKeyDown;
    case fmj::InputKey::Left:
      return kKeyLeft;
    case fmj::InputKey::Right:
      return kKeyRight;
    case fmj::InputKey::PageUp:
      return kKeyPageUp;
    case fmj::InputKey::PageDown:
      return kKeyPageDown;
    case fmj::InputKey::Confirm:
      return kKeyEnter;
    case fmj::InputKey::Cancel:
      return kKeyExit;
    case fmj::InputKey::Menu:
      return kKeyHomeMenu;
    case fmj::InputKey::None:
      return 0xFF;
  }
  return 0xFF;
}

int hexDigit(char value) {
  if (value >= '0' && value <= '9') return value - '0';
  if (value >= 'a' && value <= 'f') return value - 'a' + 10;
  if (value >= 'A' && value <= 'F') return value - 'A' + 10;
  return -1;
}

WideMapState toWideMapState(const FmjEngineWideMapState& source) {
  WideMapState state{};
  state.baseBank = source.base_bank;
  state.mapWidth = source.map_width;
  state.mapHeight = source.map_height;
  state.cameraX = source.camera_x;
  state.cameraY = source.camera_y;
  state.tileWidth = source.tile_width;
  state.tileHeight = source.tile_height;
  state.mapBank = source.map_bank;
  state.mapOffset = source.map_offset;
  state.tileBank = source.tile_bank;
  state.tileOffset = source.tile_offset;
  state.actorCount = std::min<std::uint8_t>(
      source.actor_count,
      static_cast<std::uint8_t>(WideMapState::kActorCapacity));
  for (std::uint8_t index = 0; index < state.actorCount; ++index) {
    const auto& input = source.actors[index];
    auto& output = state.actors[index];
    output.type = input.type;
    output.worldX = input.world_x;
    output.worldY = input.world_y;
    output.direction = input.direction;
    output.step = input.step;
    output.imageBank = input.image_bank;
    output.imageOffset = input.image_offset;
  }
  std::memcpy(state.overlayMask.data(), source.overlay_mask,
              state.overlayMask.size());
  return state;
}

}  // namespace

bool CEngineRuntime::begin(LittleFsByteSource& game,
                           LittleFsByteSource& hzk16,
                           LittleFsByteSource& asc16) {
  game_ = &game;
  hzk16_ = &hzk16;
  asc16_ = &asc16;
  scanSaves();
  if (!battleRenderer_.begin(game)) {
    error_ = "Unable to index battle resources";
    return false;
  }
  frameMutex_ = xSemaphoreCreateMutex();
  if (frameMutex_ == nullptr) {
    error_ = "Unable to create frame mutex";
    return false;
  }
  battleRendererMutex_ = xSemaphoreCreateMutex();
  if (battleRendererMutex_ == nullptr) {
    error_ = "Unable to create battle renderer mutex";
    return false;
  }

  FmjEngineHost host{};
  host.context = this;
  host.read_game = readGame;
  host.millis = currentMillis;
  host.yield = yieldTask;
  host.poll_key = pollKey;
  host.load_glyph = loadGlyph;
  host.screen_changed = screenChanged;
  host.screen_flush = screenFlush;
  host.wide_map_begin = wideMapBegin;
  host.wide_map_ready = wideMapReady;
  host.wide_map_end = wideMapEnd;
  host.wide_map_clear = wideMapClear;
  host.battle_begin = battleBegin;
  host.battle_end = battleEnd;
  host.play_melody = playMelody;
  host.stop_melody = stopMelody;
  host.file_create = fileCreate;
  host.file_open = fileOpen;
  host.file_delete = fileDelete;
  host.file_write = fileWrite;
  host.file_close = fileClose;
  host.file_read = fileRead;
  host.file_seek = fileSeek;
  host.file_count = fileCount;
  host.file_search = fileSearch;
  FmjEngineSetHost(&host);
  if (!FmjEnginePrepare()) {
    error_ = "FMJ.LIB is invalid or unreadable";
    return false;
  }

  dirty_ = true;
  constexpr std::uint32_t kStackBytes = 32768U;
  if (xTaskCreatePinnedToCore(taskEntry, "fmj-engine", kStackBytes, this, 2,
                              nullptr, 0) != pdPASS) {
    error_ = "Unable to start engine task";
    return false;
  }
  return true;
}

bool CEngineRuntime::update(CardputerInput& input, CardputerDisplay& display,
                            fmj::MonoCanvas& canvas) {
  const auto now = millis();
  const auto key = toEngineKey(input.poll(now));
  if (input.takeDisplayModeToggle()) {
    display.toggleMode(now);
    dirty_ = true;
    forcePresent_ = true;
    const bool useWideBattle =
        display.mode() == CardputerDisplay::Mode::WideMap && battleActive_ &&
        battleReady_;
    const bool useWideMap =
        display.mode() == CardputerDisplay::Mode::WideMap && wideMapReady_;
    Serial.printf("FMJ display mode: %s ready=%s canvas=%s\n",
                  display.mode() == CardputerDisplay::Mode::WideMap
                      ? "wide-map"
                      : "scaled",
                  wideMapReady_ ? "yes" : "no",
                  useWideBattle ? "wide-battle" :
                  useWideMap ? "wide-map" : "scaled");
  }
  if (key != 0xFF && pendingKey_ == 0xFF) pendingKey_ = key;
  if (dirty_ && now - lastFrameMs_ >= 16U) {
    if (display.mode() == CardputerDisplay::Mode::WideMap && battleActive_ &&
        !battleReady_ && battleCaptureRevision_ == battleRenderRevision_) {
      return false;
    }
    dirty_ = false;
    bool renderedBattleFrame = false;
    if (display.mode() == CardputerDisplay::Mode::WideMap && battleActive_ &&
        battleCaptureRevision_ != battleRenderRevision_) {
      xSemaphoreTake(frameMutex_, portMAX_DELAY);
      const auto captureRevision = battleCaptureRevision_;
      battleRenderSourceCanvas_ = battleSourceCanvas_;
      battleRenderState_ = battlePendingState_;
      xSemaphoreGive(frameMutex_);
      xSemaphoreTake(battleRendererMutex_, portMAX_DELAY);
      const bool rendered = battleRenderer_.render(
          *game_, battleRenderState_, battleRenderSourceCanvas_,
          presentationCanvas_);
      xSemaphoreGive(battleRendererMutex_);
      if (rendered) {
        battleRenderRevision_ = captureRevision;
        battleReady_ = true;
        renderedBattleFrame = true;
      }
    }
    if (display.mode() == CardputerDisplay::Mode::WideMap && battleActive_ &&
        battleReady_) {
      if (!renderedBattleFrame && !forcePresent_) return false;
      display.present(presentationCanvas_);
    } else if (display.mode() == CardputerDisplay::Mode::WideMap &&
               wideMapReady_) {
      xSemaphoreTake(frameMutex_, portMAX_DELAY);
      presentationCanvas_ = wideCanvases_[wideCanvasIndex_];
      xSemaphoreGive(frameMutex_);
      display.present(presentationCanvas_);
    } else {
      std::memcpy(canvas.data(), FmjEngineScreen(),
                  fmj::MonoCanvas::kBufferSize);
      display.present(canvas);
    }
    forcePresent_ = false;
    if (battleActive_ && battleCaptureRevision_ != battleRenderRevision_) {
      dirty_ = true;
    }
    lastFrameMs_ = now;
    return true;
  }
  return false;
}

CEngineRuntime::MelodyState CEngineRuntime::melodyState() const {
  MelodyState state{};
  std::uint32_t finalRevision = 0;
  do {
    state.revision = melodyRevision_;
    state.number = melodyNumber_;
    state.playing = melodyPlaying_;
    finalRevision = melodyRevision_;
  } while (state.revision != finalRevision);
  return state;
}

void CEngineRuntime::taskEntry(void* context) {
  auto* runtime = static_cast<CEngineRuntime*>(context);
  FmjEngineRun();
  runtime->error_ = "Engine stopped";
  runtime->dirty_ = true;
  vTaskDelete(nullptr);
}

std::uint8_t CEngineRuntime::readGame(void* context, std::uint32_t offset,
                                      std::uint8_t* destination,
                                      std::uint32_t length) {
  auto* runtime = static_cast<CEngineRuntime*>(context);
  return runtime->game_ != nullptr &&
                 runtime->game_->read(offset, destination, length)
             ? 1
             : 0;
}

std::uint32_t CEngineRuntime::currentMillis(void*) { return millis(); }

void CEngineRuntime::yieldTask(void*) { vTaskDelay(1); }

std::uint8_t CEngineRuntime::pollKey(void* context) {
  auto* runtime = static_cast<CEngineRuntime*>(context);
  const auto key = runtime->pendingKey_;
  runtime->pendingKey_ = 0xFF;
  return key;
}

std::uint8_t CEngineRuntime::loadGlyph(void* context,
                                       const std::uint8_t* encoded,
                                       std::uint8_t glyph[32],
                                       std::uint8_t* width,
                                       std::uint8_t* consumed) {
  auto* runtime = static_cast<CEngineRuntime*>(context);
  if (encoded == nullptr || glyph == nullptr || width == nullptr ||
      consumed == nullptr)
    return 0;
  if (*encoded < 0x80U) {
    *width = 8;
    *consumed = 1;
    return runtime->asc16_ != nullptr &&
                   runtime->asc16_->read(static_cast<std::size_t>(*encoded) *
                                             16U,
                                         glyph, 16U)
               ? 1
               : 0;
  }
  *width = 16;
  *consumed = 2;
  const auto first = encoded[0];
  const auto second = encoded[1];
  std::size_t glyphIndex = 0;
  if (first >= 0xA1U && first <= 0xF7U && second >= 0xA1U &&
      second <= 0xFEU) {
    glyphIndex = 94U * static_cast<std::size_t>(first - 0xA1U) +
                 static_cast<std::size_t>(second - 0xA1U);
  }
  return runtime->hzk16_ != nullptr &&
                 runtime->hzk16_->read(glyphIndex * 32U, glyph, 32U)
             ? 1
             : 0;
}

void CEngineRuntime::screenChanged(void* context) {
  auto* runtime = static_cast<CEngineRuntime*>(context);
  // Map drawing emits many intermediate screen updates. Publish only the
  // complete frame from wideMapReady() so scaled and wide frames cannot
  // alternate while one logical map frame is being assembled.
  if (runtime->wideMapRendering_) return;
  runtime->dirty_ = true;
}

void CEngineRuntime::screenFlush(void* context) {
  // GuiGetMsg uses this to publish direct pixel writes before sleeping. It
  // does not mean that the current wide-map frame has become invalid.
  auto* runtime = static_cast<CEngineRuntime*>(context);
  if (runtime->battleActive_ && runtime->game_ != nullptr) {
    FmjEngineBattleState engineState{};
    if (FmjEngineGetBattleState(&engineState) != 0) {
      WideBattleState state{};
      state.background = engineState.background;
      state.topRight = engineState.top_right;
      state.bottomLeft = engineState.bottom_left;
      std::memcpy(state.overlayMask.data(), engineState.overlay_mask,
                  state.overlayMask.size());
      if (!runtime->battleBackgroundPrepared_) {
        xSemaphoreTake(runtime->battleRendererMutex_, portMAX_DELAY);
        runtime->battleBackgroundPrepared_ =
            runtime->battleRenderer_.prepare(*runtime->game_, state);
        xSemaphoreGive(runtime->battleRendererMutex_);
      }
      if (runtime->battleBackgroundPrepared_) {
        xSemaphoreTake(runtime->frameMutex_, portMAX_DELAY);
        std::memcpy(runtime->battleSourceCanvas_.data(), FmjEngineScreen(),
                    fmj::MonoCanvas::kBufferSize);
        runtime->battlePendingState_ = state;
        ++runtime->battleCaptureRevision_;
        xSemaphoreGive(runtime->frameMutex_);
      }
    }
  } else if (runtime->wideMapReady_ && !runtime->wideMapRendering_) {
    FmjEngineWideMapState engineState{};
    if (FmjEngineGetWideMapState(&engineState) != 0) {
      const auto state = toWideMapState(engineState);
      fmj::MonoCanvas source{};
      std::memcpy(source.data(), FmjEngineScreen(), source.kBufferSize);
      xSemaphoreTake(runtime->frameMutex_, portMAX_DELAY);
      const auto renderCanvasIndex =
          static_cast<std::uint8_t>(runtime->wideCanvasIndex_ ^ 1U);
      WideMapRenderer::compositeOverlay(
          state, source, runtime->wideMapBaseCanvas_,
          runtime->wideCanvases_[renderCanvasIndex]);
      runtime->wideCanvasIndex_ = renderCanvasIndex;
      xSemaphoreGive(runtime->frameMutex_);
    }
  }
  runtime->dirty_ = true;
}

void CEngineRuntime::wideMapBegin(void* context) {
  static_cast<CEngineRuntime*>(context)->wideMapRendering_ = true;
}

void CEngineRuntime::wideMapReady(void* context) {
  auto* runtime = static_cast<CEngineRuntime*>(context);
  xSemaphoreTake(runtime->frameMutex_, portMAX_DELAY);
  FmjEngineWideMapState engineState{};
  const bool stateReady = FmjEngineGetWideMapState(&engineState) != 0;
  const bool rendered = runtime->game_ != nullptr && stateReady &&
                        runtime->wideRenderer_.render(
                            *runtime->game_, toWideMapState(engineState),
                            runtime->wideMapBaseCanvas_);
  if (rendered) {
    const auto renderCanvasIndex =
        static_cast<std::uint8_t>(runtime->wideCanvasIndex_ ^ 1U);
    runtime->wideCanvases_[renderCanvasIndex] = runtime->wideMapBaseCanvas_;
    runtime->wideCanvasIndex_ = renderCanvasIndex;
    runtime->wideMapReady_ = true;
  }
  runtime->dirty_ = true;
  static int previousResult = -1;
  const int result = rendered ? 2 : stateReady ? 1 : 0;
  if (result != previousResult) {
    Serial.printf(
        "FMJ wide map: state=%s render=%s error=%u map=%ux%u tile=%ux%u "
        "actors=%u base=%u mapres=%u:%u tileres=%u:%u\n",
        stateReady ? "ok" : "failed", rendered ? "ok" : "failed",
        static_cast<unsigned>(runtime->wideRenderer_.error()),
        engineState.map_width, engineState.map_height,
        engineState.tile_width, engineState.tile_height,
        engineState.actor_count, engineState.base_bank,
        engineState.map_bank, engineState.map_offset,
        engineState.tile_bank, engineState.tile_offset);
    previousResult = result;
  }
  xSemaphoreGive(runtime->frameMutex_);
  runtime->wideMapRendering_ = false;
}

void CEngineRuntime::wideMapEnd(void* context) {
  auto* runtime = static_cast<CEngineRuntime*>(context);
  runtime->wideMapRendering_ = false;
  // A map event can end the current wide-map update before LOADMAP has
  // produced the replacement frame.  Keep the last complete wide frame as
  // the transition frame so the display task cannot briefly fall back to a
  // scaled 160x96 framebuffer.  Battle rendering takes precedence as soon as
  // battleBegin() runs, and the next wideMapReady() replaces this frame
  // atomically with the new map.
  runtime->dirty_ = true;
}

void CEngineRuntime::wideMapClear(void* context) {
  auto* runtime = static_cast<CEngineRuntime*>(context);
  runtime->wideMapRendering_ = false;
  runtime->wideMapReady_ = false;
  runtime->dirty_ = true;
}

void CEngineRuntime::battleBegin(void* context) {
  auto* runtime = static_cast<CEngineRuntime*>(context);
  xSemaphoreTake(runtime->frameMutex_, portMAX_DELAY);
  runtime->battleRenderRevision_ = runtime->battleCaptureRevision_;
  xSemaphoreGive(runtime->frameMutex_);
  runtime->battleActive_ = true;
  runtime->battleReady_ = false;
  runtime->battleBackgroundPrepared_ = false;
  runtime->dirty_ = true;
}

void CEngineRuntime::battleEnd(void* context) {
  auto* runtime = static_cast<CEngineRuntime*>(context);
  runtime->battleActive_ = false;
  runtime->battleReady_ = false;
  runtime->dirty_ = true;
}

void CEngineRuntime::playMelody(void* context, std::uint8_t melody) {
  auto* runtime = static_cast<CEngineRuntime*>(context);
  runtime->melodyNumber_ = melody;
  runtime->melodyPlaying_ = melody != 0;
  ++runtime->melodyRevision_;
}

void CEngineRuntime::stopMelody(void* context) {
  auto* runtime = static_cast<CEngineRuntime*>(context);
  runtime->melodyNumber_ = 0;
  runtime->melodyPlaying_ = false;
  ++runtime->melodyRevision_;
}

std::uint8_t CEngineRuntime::fileCreate(
    void* context, std::uint8_t filetype, std::uint32_t length,
    const std::uint8_t information[10], std::uint16_t* filename,
    std::uint8_t* handle) {
  auto* runtime = static_cast<CEngineRuntime*>(context);
  if (information == nullptr || filename == nullptr || handle == nullptr)
    return 0;
  std::size_t index = 0;
  while (index < runtime->files_.size() && runtime->files_[index].used) ++index;
  if (index == runtime->files_.size()) return 0;
  char path[48]{};
  makeSavePath(path, sizeof(path), filetype, information);
  auto file = LittleFS.open(path, "w+");
  if (!file) return 0;
  if (length != 0) {
    if (!file.seek(length - 1U) || file.write(static_cast<std::uint8_t>(0)) != 1U ||
        !file.seek(0)) {
      file.close();
      LittleFS.remove(path);
      return 0;
    }
  }
  auto& record = runtime->files_[index];
  record.used = true;
  record.type = filetype;
  std::memcpy(record.information.data(), information, 10);
  record.file = file;
  *filename = static_cast<std::uint16_t>(index + 1U);
  *handle = static_cast<std::uint8_t>(index + 1U);
  return 1;
}

std::uint8_t CEngineRuntime::fileOpen(void* context, std::uint16_t filename,
                                      std::uint8_t filetype,
                                      std::uint8_t openmode,
                                      std::uint8_t* handle,
                                      std::uint32_t* length) {
  auto* runtime = static_cast<CEngineRuntime*>(context);
  if (filename == 0 || filename > runtime->files_.size() || handle == nullptr ||
      length == nullptr)
    return 0;
  auto& record = runtime->files_[filename - 1U];
  if (!record.used || record.type != filetype) return 0;
  if (record.file) record.file.close();
  char path[48]{};
  makeSavePath(path, sizeof(path), record.type, record.information.data());
  record.file = LittleFS.open(path, openmode == kReadOnly ? "r" : "r+");
  if (!record.file) return 0;
  *handle = static_cast<std::uint8_t>(filename);
  *length = record.file.size();
  return 1;
}

std::uint8_t CEngineRuntime::fileDelete(void* context, std::uint8_t handle) {
  auto* runtime = static_cast<CEngineRuntime*>(context);
  auto* record = runtime->byHandle(handle);
  if (record == nullptr) return 0;
  if (record->file) record->file.close();
  char path[48]{};
  makeSavePath(path, sizeof(path), record->type, record->information.data());
  const bool removed = LittleFS.remove(path);
  *record = SaveFile{};
  return removed ? 1 : 0;
}

std::uint8_t CEngineRuntime::fileWrite(void* context, std::uint8_t handle,
                                       std::uint8_t length,
                                       const std::uint8_t* source) {
  auto* record = static_cast<CEngineRuntime*>(context)->byHandle(handle);
  if (record == nullptr || !record->file || source == nullptr) return 0;
  const bool success = record->file.write(source, length) == length;
  if (success) record->file.flush();
  return success ? 1 : 0;
}

std::uint8_t CEngineRuntime::fileClose(void* context, std::uint8_t handle) {
  auto* record = static_cast<CEngineRuntime*>(context)->byHandle(handle);
  if (record == nullptr) return 0;
  if (record->file) record->file.close();
  return 1;
}

std::uint8_t CEngineRuntime::fileRead(void* context, std::uint8_t handle,
                                      std::uint8_t length,
                                      std::uint8_t* destination) {
  auto* record = static_cast<CEngineRuntime*>(context)->byHandle(handle);
  return record != nullptr && record->file && destination != nullptr &&
                 record->file.read(destination, length) == length
             ? 1
             : 0;
}

std::uint8_t CEngineRuntime::fileSeek(void* context, std::uint8_t handle,
                                      std::uint32_t offset,
                                      std::uint8_t origin) {
  auto* record = static_cast<CEngineRuntime*>(context)->byHandle(handle);
  if (record == nullptr || !record->file) return 0;
  const auto signedOffset = static_cast<std::int32_t>(offset);
  std::int64_t base = 0;
  if (origin == kFromCurrent) base = record->file.position();
  if (origin == kFromEnd) base = record->file.size();
  const auto target = base + signedOffset;
  if (target < 0) return 0;
  return record->file.seek(static_cast<std::uint32_t>(target), SeekSet) ? 1 : 0;
}

std::uint8_t CEngineRuntime::fileCount(void* context, std::uint8_t filetype,
                                       std::uint16_t* count) {
  auto* runtime = static_cast<CEngineRuntime*>(context);
  if (count == nullptr) return 0;
  *count = 0;
  for (const auto& record : runtime->files_)
    if (record.used && record.type == filetype) ++*count;
  return *count != 0 ? 1 : 0;
}

std::uint8_t CEngineRuntime::fileSearch(
    void* context, std::uint8_t filetype, std::uint16_t order,
    std::uint16_t* filename, std::uint8_t information[10]) {
  auto* runtime = static_cast<CEngineRuntime*>(context);
  if (order == 0 || filename == nullptr || information == nullptr) return 0;
  for (std::size_t index = 0; index < runtime->files_.size(); ++index) {
    const auto& record = runtime->files_[index];
    if (!record.used || record.type != filetype) continue;
    if (--order == 0) {
      *filename = static_cast<std::uint16_t>(index + 1U);
      std::memcpy(information, record.information.data(), 10);
      return 1;
    }
  }
  return 0;
}

void CEngineRuntime::scanSaves() {
  for (auto& record : files_) {
    if (record.file) record.file.close();
    record = SaveFile{};
  }
  auto directory = LittleFS.open("/saves");
  if (!directory || !directory.isDirectory()) return;
  auto entry = directory.openNextFile();
  std::size_t index = 0;
  while (entry && index < files_.size()) {
    std::uint8_t type = 0;
    std::array<std::uint8_t, 10> information{};
    if (!entry.isDirectory() && decodeSaveName(entry.name(), type, information)) {
      files_[index].used = true;
      files_[index].type = type;
      files_[index].information = information;
      ++index;
    }
    entry = directory.openNextFile();
  }
}

bool CEngineRuntime::decodeSaveName(
    const char* name, std::uint8_t& type,
    std::array<std::uint8_t, 10>& information) {
  if (name == nullptr) return false;
  const char* base = std::strrchr(name, '/');
  base = base == nullptr ? name : base + 1;
  if (std::strlen(base) != 27U || std::strncmp(base, "fmj_", 4) != 0 ||
      base[24] != '.')
    return false;
  for (std::size_t index = 0; index < information.size(); ++index) {
    const int high = hexDigit(base[4 + index * 2]);
    const int low = hexDigit(base[5 + index * 2]);
    if (high < 0 || low < 0) return false;
    information[index] = static_cast<std::uint8_t>((high << 4) | low);
  }
  const int high = hexDigit(base[25]);
  const int low = hexDigit(base[26]);
  if (high < 0 || low < 0) return false;
  type = static_cast<std::uint8_t>((high << 4) | low);
  return true;
}

void CEngineRuntime::makeSavePath(char* path, std::size_t capacity,
                                  std::uint8_t type,
                                  const std::uint8_t information[10]) {
  static constexpr char kHex[] = "0123456789ABCDEF";
  if (path == nullptr || capacity < 36U) return;
  std::memcpy(path, "/saves/fmj_", 11);
  std::size_t cursor = 11;
  for (std::size_t index = 0; index < 10; ++index) {
    path[cursor++] = kHex[information[index] >> 4];
    path[cursor++] = kHex[information[index] & 0x0F];
  }
  path[cursor++] = '.';
  path[cursor++] = kHex[type >> 4];
  path[cursor++] = kHex[type & 0x0F];
  path[cursor] = '\0';
}

CEngineRuntime::SaveFile* CEngineRuntime::byHandle(std::uint8_t handle) {
  if (handle == 0 || handle > files_.size() || !files_[handle - 1U].used)
    return nullptr;
  return &files_[handle - 1U];
}

const CEngineRuntime::SaveFile* CEngineRuntime::byHandle(
    std::uint8_t handle) const {
  if (handle == 0 || handle > files_.size() || !files_[handle - 1U].used)
    return nullptr;
  return &files_[handle - 1U];
}
