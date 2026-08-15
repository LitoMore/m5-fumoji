/* Copyright (C) 2026 LitoMore; SPDX-License-Identifier: GPL-2.0-only */
#include "engine_port.h"
#include "middle.h"

#include <assert.h>
#include <string.h>

static UINT8 game[0x8000];
static UINT32 clock_ms;

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
  UINT8* first;
  UINT8* second;
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

  SysMemInit(0x2C00, 0x1400);
  first = SysMemAllocate(5);
  second = SysMemAllocate(64);
  assert(first != NULL && second != NULL && first != second);
  assert(SysMemFree(first) == 1);
  assert(SysMemFree(second) == 1);
  assert(SysMemAllocate(0x1300) != NULL);
  return 0;
}
