/* Copyright (C) 2026 LitoMore; SPDX-License-Identifier: GPL-2.0-only */
#include "engine_port.h"
#include "engine.h"
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

static void test_magic_selection_moves_up(void) {
  MsgType enter = {DICT_WM_CHAR_FUN, CHAR_ENTER};
  MsgType up = {DICT_WM_CHAR_FUN, CHAR_UP};
  UINT8* first_magic = game + 0x4010;
  UINT8* second_magic = game + 0x4040;

  game[0x0C] = 6;
  game[0x0D] = 0;
  game[0x10] = DAT_MGICRES;
  game[0x11] = 1;
  game[0x12] = 1;
  game[0x13] = DAT_MGICRES;
  game[0x14] = 1;
  game[0x15] = 2;
  game[0x2000] = 1;
  game[0x2001] = 0x10;
  game[0x2002] = 0;
  game[0x2003] = 1;
  game[0x2004] = 0x40;
  game[0x2005] = 0;
  memcpy(game + 0x4000, "MRS", 3);
  memset(first_magic, 0, 0x30);
  first_magic[0] = 1;
  first_magic[1] = 1;
  memcpy(first_magic + 6, "FIRST", 6);
  memset(second_magic, 0, 0x30);
  second_magic[0] = 1;
  second_magic[1] = 2;
  memcpy(second_magic + 6, "SECOND", 7);
  MCU_memory[0x2800] = 1;
  MCU_memory[0x2801] = 1;
  MCU_memory[0x2802] = 1;
  MCU_memory[0x2803] = 2;
  MCU_memory[0x1935] = 0;
  assert(GuiPushMsg(&enter) == 1);
  assert(GuiPushMsg(&up) == 1);
  assert(_00210000(0x2800, 2, 1) == 0);
}

int main(void) {
  FmjEngineHost host;
  FmjEngineBattleState battle;
  FmjEngineWideMapState wide_map;
  UINT8 actor_save[0xB1];
  UINT8* first;
  UINT8* magic;
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
  test_magic_selection_moves_up();
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

  SysMemInit(0x2C00, 0x1400);
  assert(_0020EB2D(0) == 1);
  memset(actor_save, 0, sizeof(actor_save));
  actor_save[0x0E] = 4;
  actor_save[0x3D] = 3;
  actor_save[0x3E] = 1;
  actor_save[0x3F] = 1;
  actor_save[0x40] = 1;
  actor_save[0x41] = 3;
  actor_save[0x42] = 1;
  actor_save[0x43] = 1;
  actor_save[0x44] = 8;
  assert(_00245A87(0, actor_save) == 1);
  magic = MCU_memory + *(UINT16*)(MCU_memory + 0x1988 + 0x15);
  assert(MCU_memory[0x1988 + 0x0E] == 3);
  assert(magic[0] == 3 && magic[1] == 1);
  assert(magic[2] == 1 && magic[3] == 1);
  assert(magic[4] == 1 && magic[5] == 8);
  _0022E873(0, 3, 1);
  assert(MCU_memory[0x1988 + 0x0E] == 3);
  return 0;
}
