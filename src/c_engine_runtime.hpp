/* Copyright (C) 2026 LitoMore; SPDX-License-Identifier: GPL-2.0-only */
#pragma once

#include <FS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include "cardputer_display.hpp"
#include "cardputer_input.hpp"
#include "fmj/mono_canvas.hpp"
#include "littlefs_byte_source.hpp"
#include "wide_canvas.hpp"
#include "wide_battle_renderer.hpp"
#include "wide_map_renderer.hpp"

extern "C" {
#include "fmj_c_engine/engine_port.h"
}

class CEngineRuntime {
 public:
  struct MelodyState {
    std::uint32_t revision = 0;
    std::uint8_t number = 0;
    bool playing = false;
  };

  bool begin(LittleFsByteSource& game, LittleFsByteSource& hzk16,
             LittleFsByteSource& asc16);
  bool update(CardputerInput& input, CardputerDisplay& display,
              fmj::MonoCanvas& canvas);
  MelodyState melodyState() const;
  const char* error() const { return error_; }

 private:
  static constexpr std::size_t kFileCapacity = 16;

  struct SaveFile {
    bool used = false;
    std::uint8_t type = 0;
    std::array<std::uint8_t, 10> information{};
    fs::File file;
  };

  static void taskEntry(void* context);
  static std::uint8_t readGame(void* context, std::uint32_t offset,
                               std::uint8_t* destination,
                               std::uint32_t length);
  static std::uint32_t currentMillis(void* context);
  static void yieldTask(void* context);
  static std::uint8_t pollKey(void* context);
  static std::uint8_t loadGlyph(void* context, const std::uint8_t* encoded,
                                std::uint8_t glyph[32], std::uint8_t* width,
                                std::uint8_t* consumed);
  static void screenChanged(void* context);
  static void screenFlush(void* context);
  static void wideMapBegin(void* context);
  static void wideMapReady(void* context);
  static void wideMapEnd(void* context);
  static void wideMapClear(void* context);
  static void battleBegin(void* context);
  static void battleEnd(void* context);
  static void playMelody(void* context, std::uint8_t melody);
  static void stopMelody(void* context);
  static std::uint8_t fileCreate(void* context, std::uint8_t filetype,
                                 std::uint32_t length,
                                 const std::uint8_t information[10],
                                 std::uint16_t* filename,
                                 std::uint8_t* handle);
  static std::uint8_t fileOpen(void* context, std::uint16_t filename,
                               std::uint8_t filetype, std::uint8_t openmode,
                               std::uint8_t* handle, std::uint32_t* length);
  static std::uint8_t fileDelete(void* context, std::uint8_t handle);
  static std::uint8_t fileWrite(void* context, std::uint8_t handle,
                                std::uint8_t length,
                                const std::uint8_t* source);
  static std::uint8_t fileClose(void* context, std::uint8_t handle);
  static std::uint8_t fileRead(void* context, std::uint8_t handle,
                               std::uint8_t length,
                               std::uint8_t* destination);
  static std::uint8_t fileSeek(void* context, std::uint8_t handle,
                               std::uint32_t offset, std::uint8_t origin);
  static std::uint8_t fileCount(void* context, std::uint8_t filetype,
                                std::uint16_t* count);
  static std::uint8_t fileSearch(void* context, std::uint8_t filetype,
                                 std::uint16_t order, std::uint16_t* filename,
                                 std::uint8_t information[10]);

  void scanSaves();
  static bool decodeSaveName(const char* name, std::uint8_t& type,
                             std::array<std::uint8_t, 10>& information);
  static void makeSavePath(char* path, std::size_t capacity,
                           std::uint8_t type,
                           const std::uint8_t information[10]);
  SaveFile* byHandle(std::uint8_t handle);
  const SaveFile* byHandle(std::uint8_t handle) const;

  LittleFsByteSource* game_ = nullptr;
  LittleFsByteSource* hzk16_ = nullptr;
  LittleFsByteSource* asc16_ = nullptr;
  std::array<WideCanvas, 2> wideCanvases_{};
  WideCanvas wideMapBaseCanvas_{};
  WideCanvas presentationCanvas_{};
  WideMapRenderer wideRenderer_{};
  WideBattleRenderer battleRenderer_{};
  fmj::MonoCanvas battleSourceCanvas_{};
  fmj::MonoCanvas battleRenderSourceCanvas_{};
  WideBattleState battlePendingState_{};
  WideBattleState battleRenderState_{};
  std::array<SaveFile, kFileCapacity> files_{};
  volatile std::uint8_t pendingKey_ = 0xFF;
  volatile bool dirty_ = false;
  volatile bool wideMapRendering_ = false;
  volatile bool wideMapReady_ = false;
  volatile std::uint8_t wideCanvasIndex_ = 0;
  volatile bool battleActive_ = false;
  volatile bool battleReady_ = false;
  volatile bool battleBackgroundPrepared_ = false;
  volatile std::uint32_t battleCaptureRevision_ = 0;
  volatile std::uint32_t battleRenderRevision_ = 0;
  SemaphoreHandle_t frameMutex_ = nullptr;
  SemaphoreHandle_t battleRendererMutex_ = nullptr;
  volatile std::uint32_t melodyRevision_ = 0;
  volatile std::uint8_t melodyNumber_ = 0;
  volatile bool melodyPlaying_ = false;
  std::uint32_t lastFrameMs_ = 0;
  bool forcePresent_ = false;
  const char* error_ = nullptr;
};
