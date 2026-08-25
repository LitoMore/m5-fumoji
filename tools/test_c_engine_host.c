/* Copyright (C) 2026 LitoMore; SPDX-License-Identifier: GPL-2.0-only */
#include "engine_port.h"
#include "middle.h"

#include <assert.h>
#include <string.h>

static UINT8 game[0x8000];
static UINT32 clock_ms;
extern UINT8 MCU_memory[0x10000];

static UINT8 read_game(void* context, UINT32 offset, UINT8* destination,
                       UINT32 length) {
  (void)context;
  if (offset > sizeof(game) || length > sizeof(game) - offset) return 0;
  memcpy(destination, game + offset, length);
  return 1;
}

static UINT32 read_clock(void* context) {
  (void)context;
  return clock_ms;
}

static void yield_clock(void* context) {
  (void)context;
  ++clock_ms;
}

static UINT8 no_key(void* context) {
  (void)context;
  return 0xFF;
}

int main(void) {
  FmjEngineHost host;
  FmjEngineBattleState battle;
  FmjEngineWideMapState wide_map;
  UINT8* first;
  UINT8* second;
  UINT8 saved_pixel[1];
  UINT8 transparent_picture[2] = {0x1B, 0x00};
  memset(&host, 0, sizeof(host));
  memcpy(game, "LIBhost-test", 12);
  host.read_game = read_game;
  host.millis = read_clock;
  host.yield = yield_clock;
  host.poll_key = no_key;
  FmjEngineSetHost(&host);
  assert(FmjEnginePrepare() == 1);

  GuiInit();
  SysLcdPartClear(0, 0, 159, 95);
  SysPutPixel(159, 95, 1);
  assert((FmjEngineScreen()[95 * 20 + 19] & 1U) != 0);
  SysLcdReverse(159, 95, 159, 95);
  assert((FmjEngineScreen()[95 * 20 + 19] & 1U) == 0);

  MCU_memory[0x197E] = 9;
  MCU_memory[0x197F] = 6;
  MCU_memory[0x1983] = 16;
  MCU_memory[0x1984] = 16;
  FmjEngineNotifyWideMapBegin();
  FmjEngineNotifyWideMapReady();
  SysPutPixel(12, 11, 0);
  assert(FmjEngineGetWideMapState(&wide_map) == 1);
  assert((wide_map.overlay_mask[11 * 20 + 1] & 0x08U) != 0);
  FmjEngineNotifyWideMapEnd();

  FmjEngineNotifyBattleBegin();
  FmjEngineNotifyBattleBackgroundReady();
  SysPutPixel(10, 10, 0);
  FmjEngineTrackTransparentPicture(20, 10, 4, 1,
                                   transparent_picture);
  assert(FmjEngineGetBattleState(&battle) == 1);
  assert((battle.overlay_mask[10 * 20 + 1] & 0x20U) != 0);
  assert((battle.overlay_mask[10 * 20 + 2] & 0x0CU) == 0x0CU);
  assert((battle.overlay_mask[10 * 20 + 2] & 0x03U) == 0);
  SysSaveScreen(10, 10, 10, 10, saved_pixel);
  SysLcdReverse(11, 10, 11, 10);
  SysRestoreScreen(10, 10, 10, 10, saved_pixel);
  assert(FmjEngineGetBattleState(&battle) == 1);
  assert((battle.overlay_mask[10 * 20 + 1] & 0x30U) == 0x20U);
  *(UINT16*)(MCU_memory + 0x1936) = 0x3000;
  SysMemcpy(MCU_memory + 0x4000, MCU_memory + 0x3000,
            FMJ_ENGINE_SCREEN_BYTES);
  SysPutPixel(11, 10, 0);
  SysMemcpy(MCU_memory + 0x3000, MCU_memory + 0x4000,
            FMJ_ENGINE_SCREEN_BYTES);
  assert(FmjEngineGetBattleState(&battle) == 1);
  assert((battle.overlay_mask[10 * 20 + 1] & 0x30U) == 0x20U);
  FmjEngineExcludeBattleOverlayRect(10, 10, 10, 10);
  assert(FmjEngineGetBattleState(&battle) == 1);
  assert((battle.overlay_mask[10 * 20 + 1] & 0x20U) == 0);
  FmjEngineNotifyBattleEnd();

  SysMemInit(0x2C00, 0x1400);
  first = SysMemAllocate(5);
  second = SysMemAllocate(64);
  assert(first != NULL && second != NULL && first != second);
  assert(SysMemFree(first) == 1);
  assert(SysMemFree(second) == 1);
  assert(SysMemAllocate(0x1300) != NULL);
  return 0;
}
