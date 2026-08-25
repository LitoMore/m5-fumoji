/*
 * Cardputer-independent host layer for the FMJ C engine.
 * Copyright (C) 2026 LitoMore
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This replaces upstream's Win32 middle.c while preserving its BBK API.
 */
#include "middle.h"

#include "engine.h"

#include <stdio.h>
#include <string.h>

extern UINT8 MCU_memory_dummy[0x8000];
extern UINT8 MCU_memory[0x10000];

#define Mem_Start (*(UINT32*)(MCU_memory + 0x2BAB))
#define Mem_Len (*(UINT32*)(MCU_memory + 0x2BAF))
#define Mem_Flag MCU_memory[0x2BB3]

static FmjEngineHost host;
static UINT16 bank4 = 4;
static UINT16 bank9 = 0;
static UINT32 timer_period_ms = 0;
static UINT32 timer_due_ms = 0;
static UINT8 key_sound = 0;
static UINT8 running = 0;
static UINT8 stop_requested = 0;
static UINT8 battle_active = 0;
static UINT8 battle_overlay_tracking = 0;
static UINT8 battle_overlay_mask[FMJ_ENGINE_SCREEN_BYTES];
static UINT8 battle_snapshot_mask[FMJ_ENGINE_SCREEN_BYTES];
static UINT8 battle_snapshot_mask_valid = 0;
static UINT8 wide_map_overlay_tracking = 0;
static UINT8 wide_map_overlay_mask[FMJ_ENGINE_SCREEN_BYTES];
static UINT8 wide_map_snapshot_mask[FMJ_ENGINE_SCREEN_BYTES];
static UINT8 wide_map_snapshot_mask_valid = 0;

enum { BATTLE_MASK_SAVE_CAPACITY = 4 };

typedef struct BattleMaskSave {
  UINT8* screen_buffer;
  UINT8 first_byte;
  UINT8 last_byte;
  UINT8 y1;
  UINT8 y2;
  UINT16 length;
  UINT8 data[FMJ_ENGINE_SCREEN_BYTES];
  UINT8 wide_map_data[FMJ_ENGINE_SCREEN_BYTES];
} BattleMaskSave;

static BattleMaskSave battle_mask_saves[BATTLE_MASK_SAVE_CAPACITY];
static UINT8 battle_mask_save_cursor = 0;

static void clear_battle_mask_saves(void) {
  memset(battle_mask_saves, 0, sizeof(battle_mask_saves));
  battle_mask_save_cursor = 0;
}

static void track_battle_pixel(UINT8 x, UINT8 y) {
  UINT8 mask;
  UINT16 offset;
  if ((!battle_overlay_tracking && !wide_map_overlay_tracking) ||
      x >= FMJ_ENGINE_SCREEN_WIDTH || y >= FMJ_ENGINE_SCREEN_HEIGHT) {
    return;
  }
  offset = (UINT16)y * 20U + x / 8U;
  mask = (UINT8)(0x80U >> (x & 7U));
  if (battle_overlay_tracking) battle_overlay_mask[offset] |= mask;
  if (wide_map_overlay_tracking) wide_map_overlay_mask[offset] |= mask;
}

static UINT8 is_battle_draw_buffer(const UINT8* screen) {
  if ((!battle_overlay_tracking && !wide_map_overlay_tracking) ||
      screen == NULL) {
    return 0;
  }
  return screen == MCU_memory + 0x400 ||
         screen == MCU_memory + *(UINT16*)(MCU_memory + 0x1936);
}

static UINT32 host_millis(void) {
  return host.millis != NULL ? host.millis(host.context) : 0;
}

static void host_yield(void) {
  if (host.yield != NULL) host.yield(host.context);
}

static void screen_changed(void) {
  if (host.screen_changed != NULL) host.screen_changed(host.context);
}

static void screen_flush(void) {
  if (host.screen_flush != NULL)
    host.screen_flush(host.context);
  else
    screen_changed();
}

static UINT8 in_screen(UINT8 x, UINT8 y) {
  return x < FMJ_ENGINE_SCREEN_WIDTH && y < FMJ_ENGINE_SCREEN_HEIGHT;
}

static void normalize_rect(UINT8* x1, UINT8* y1, UINT8* x2, UINT8* y2) {
  UINT8 temporary;
  if (*x1 > *x2) {
    temporary = *x1;
    *x1 = *x2;
    *x2 = temporary;
  }
  if (*y1 > *y2) {
    temporary = *y1;
    *y1 = *y2;
    *y2 = temporary;
  }
}

void FmjEngineSetHost(const FmjEngineHost* new_host) {
  memset(&host, 0, sizeof(host));
  if (new_host != NULL) host = *new_host;
}

UINT8 FmjEnginePrepare(void) {
  UINT8 signature[3];
  if (host.read_game == NULL || host.millis == NULL || host.poll_key == NULL ||
      !host.read_game(host.context, 0, signature, sizeof(signature)) ||
      memcmp(signature, "LIB", sizeof(signature)) != 0) {
    return 0;
  }
  memset(MCU_memory, 0, sizeof(MCU_memory));
  memset(MCU_memory_dummy, 0, sizeof(MCU_memory_dummy));
  if (!host.read_game(host.context, 0, MCU_memory + 0x9000, 0x4000)) {
    return 0;
  }
  bank4 = 4;
  bank9 = 0;
  timer_period_ms = 0;
  timer_due_ms = 0;
  stop_requested = 0;
  battle_active = 0;
  battle_overlay_tracking = 0;
  memset(battle_overlay_mask, 0, sizeof(battle_overlay_mask));
  memset(battle_snapshot_mask, 0, sizeof(battle_snapshot_mask));
  battle_snapshot_mask_valid = 0;
  wide_map_overlay_tracking = 0;
  memset(wide_map_overlay_mask, 0, sizeof(wide_map_overlay_mask));
  memset(wide_map_snapshot_mask, 0, sizeof(wide_map_snapshot_mask));
  wide_map_snapshot_mask_valid = 0;
  clear_battle_mask_saves();
  return 1;
}

void FmjEngineRun(void) {
  running = 1;
  _00200046();
  running = 0;
}

void FmjEngineRequestStop(void) { stop_requested = 1; }

const UINT8* FmjEngineScreen(void) { return MCU_memory + 0x400; }

UINT8 FmjEngineRunning(void) { return running; }

UINT8 FmjEngineGetWideMapState(FmjEngineWideMapState* state) {
  UINT8 player_index;
  UINT16 player;
  UINT8 index;
  if (state == NULL) return 0;
  memset(state, 0, sizeof(*state));
  if (MCU_memory[0x1935] != 0 || MCU_memory[0x197E] == 0 ||
      MCU_memory[0x197F] == 0 || MCU_memory[0x1983] == 0 ||
      MCU_memory[0x1984] == 0) {
    return 0;
  }

  state->base_bank = *(UINT16*)(MCU_memory + 0x1931);
  state->map_width = MCU_memory[0x197E];
  state->map_height = MCU_memory[0x197F];
  state->camera_x = MCU_memory[0x197C];
  state->camera_y = MCU_memory[0x197D];
  state->tile_width = MCU_memory[0x1983];
  state->tile_height = MCU_memory[0x1984];
  state->map_bank = MCU_memory[0x1980];
  state->map_offset = *(UINT16*)(MCU_memory + 0x1981);
  state->tile_bank = MCU_memory[0x1985];
  state->tile_offset = *(UINT16*)(MCU_memory + 0x1986);
  memcpy(state->overlay_mask, wide_map_overlay_mask,
         sizeof(state->overlay_mask));

  player_index = MCU_memory[0x1A94];
  player = (UINT16)(0x1988U + (UINT16)player_index * 0x19U);
  if (player <= 0xFFE6U && MCU_memory[player + 1U] != 0) {
    FmjEngineWideActor* actor = &state->actors[state->actor_count++];
    actor->type = 1;
    actor->world_x = (UINT8)(state->camera_x + MCU_memory[player + 5U]);
    actor->world_y = (UINT8)(state->camera_y + MCU_memory[player + 6U]);
    actor->direction = MCU_memory[player + 2U];
    actor->step = MCU_memory[player + 3U];
    actor->image_bank = MCU_memory[player + 0x0AU];
    actor->image_offset = *(UINT16*)(MCU_memory + player + 0x0BU);
  }

  for (index = 0; index < 0x28U &&
                  state->actor_count < FMJ_ENGINE_WIDE_ACTOR_CAPACITY;
       ++index) {
    UINT16 actor_address =
        *(UINT16*)(MCU_memory + 0x19D3U + (UINT16)index * 2U);
    FmjEngineWideActor* actor;
    if (actor_address == 0 || actor_address > 0xFFE6U) continue;
    actor = &state->actors[state->actor_count++];
    actor->type = MCU_memory[actor_address];
    actor->world_x = MCU_memory[actor_address + 5U];
    actor->world_y = MCU_memory[actor_address + 6U];
    actor->direction = MCU_memory[actor_address + 2U];
    actor->step = MCU_memory[actor_address + 3U];
    actor->image_bank = MCU_memory[actor_address + 0x17U];
    actor->image_offset =
        *(UINT16*)(MCU_memory + actor_address + 0x18U);
  }
  return 1;
}

UINT8 FmjEngineGetBattleState(FmjEngineBattleState* state) {
  if (state == NULL || !battle_active) return 0;
  state->background = MCU_memory[0x18DD];
  state->top_right = MCU_memory[0x18DE];
  state->bottom_left = MCU_memory[0x18DF];
  memcpy(state->overlay_mask, battle_overlay_mask,
         sizeof(state->overlay_mask));
  return 1;
}

void FmjEngineNotifyWideMapBegin(void) {
  wide_map_overlay_tracking = 0;
  memset(wide_map_overlay_mask, 0, sizeof(wide_map_overlay_mask));
  wide_map_snapshot_mask_valid = 0;
  clear_battle_mask_saves();
  if (host.wide_map_begin != NULL) host.wide_map_begin(host.context);
}

void FmjEngineNotifyWideMapReady(void) {
  memset(wide_map_overlay_mask, 0, sizeof(wide_map_overlay_mask));
  wide_map_snapshot_mask_valid = 0;
  clear_battle_mask_saves();
  wide_map_overlay_tracking = 1;
  if (host.wide_map_ready != NULL) host.wide_map_ready(host.context);
}

void FmjEngineNotifyWideMapEnd(void) {
  wide_map_overlay_tracking = 0;
  if (host.wide_map_end != NULL) host.wide_map_end(host.context);
}

void FmjEngineNotifyBattleBegin(void) {
  battle_active = 1;
  battle_overlay_tracking = 0;
  memset(battle_overlay_mask, 0, sizeof(battle_overlay_mask));
  battle_snapshot_mask_valid = 0;
  clear_battle_mask_saves();
  if (host.battle_begin != NULL) host.battle_begin(host.context);
}

void FmjEngineNotifyBattleBackgroundReady(void) {
  if (!battle_active) return;
  memset(battle_overlay_mask, 0, sizeof(battle_overlay_mask));
  battle_snapshot_mask_valid = 0;
  clear_battle_mask_saves();
  battle_overlay_tracking = 1;
}

void FmjEngineNotifyBattleEnd(void) {
  battle_active = 0;
  battle_overlay_tracking = 0;
  if (host.battle_end != NULL) host.battle_end(host.context);
}

void FmjEngineTrackTransparentPicture(UINT8 x, UINT8 y, UINT8 width,
                                      UINT8 height, const UINT8* picture) {
  UINT16 row_stride;
  UINT16 iy;
  UINT16 ix;
  INT16 destination_y;
  if ((!battle_overlay_tracking && !wide_map_overlay_tracking) ||
      picture == NULL || width == 0 || height == 0) {
    return;
  }
  row_stride = (UINT16)(((UINT16)width + 7U) / 8U * 2U);
  destination_y = (y & 0x80U) != 0U ? (INT16)y - 0x100 : y;
  for (iy = 0; iy < height; ++iy) {
    INT16 output_y = destination_y + (INT16)iy;
    if (output_y < 0 || output_y >= FMJ_ENGINE_SCREEN_HEIGHT) continue;
    for (ix = 0; ix < width; ++ix) {
      const UINT8 encoded = picture[iy * row_stride + ix / 4U];
      const UINT8 pair =
          (UINT8)((encoded >> (6U - (ix & 3U) * 2U)) & 3U);
      if ((pair & 2U) == 0U && (UINT16)x + ix < FMJ_ENGINE_SCREEN_WIDTH) {
        track_battle_pixel((UINT8)((UINT16)x + ix), (UINT8)output_y);
      }
    }
  }
}

void FmjEngineExcludeBattleOverlayRect(UINT8 x1, UINT8 y1, UINT8 x2,
                                       UINT8 y2) {
  UINT16 x;
  UINT16 y;
  if (!battle_overlay_tracking || !in_screen(x1, y1) ||
      !in_screen(x2, y2)) {
    return;
  }
  normalize_rect(&x1, &y1, &x2, &y2);
  for (y = y1; y <= y2; ++y) {
    for (x = x1; x <= x2; ++x) {
      battle_overlay_mask[y * 20U + x / 8U] &=
          (UINT8)~(0x80U >> (x & 7U));
    }
  }
}

void fillmem(UINT8* destination, UINT16 size, UINT8 value) {
  memset(destination, value, size);
}

void SysPutPixel(UINT8 x, UINT8 y, UINT8 value) {
  UINT8* pixel;
  UINT8 mask;
  if (!in_screen(x, y)) return;
  pixel = MCU_memory + 0x400 + 20 * y + x / 8;
  mask = (UINT8)(0x80U >> (x & 7));
  if (value != 0)
    *pixel |= mask;
  else
    *pixel &= (UINT8)~mask;
  track_battle_pixel(x, y);
}

void SysLine(UINT8 x1, UINT8 y1, UINT8 x2, UINT8 y2) {
  INT16 x = x1;
  INT16 y = y1;
  INT16 dx = x1 < x2 ? (INT16)(x2 - x1) : (INT16)(x1 - x2);
  INT16 sx = x1 < x2 ? 1 : -1;
  INT16 dy = y1 < y2 ? (INT16)(y1 - y2) : (INT16)(y2 - y1);
  INT16 sy = y1 < y2 ? 1 : -1;
  INT16 error = (INT16)(dx + dy);
  for (;;) {
    SysPutPixel((UINT8)x, (UINT8)y, 1);
    if (x == x2 && y == y2) break;
    {
      INT16 twice = (INT16)(2 * error);
      if (twice >= dy) {
        error = (INT16)(error + dy);
        x = (INT16)(x + sx);
      }
      if (twice <= dx) {
        error = (INT16)(error + dx);
        y = (INT16)(y + sy);
      }
    }
  }
  screen_changed();
}

void SysRect(UINT8 x1, UINT8 y1, UINT8 x2, UINT8 y2) {
  if (!in_screen(x1, y1) || !in_screen(x2, y2)) return;
  SysLine(x1, y1, x2, y1);
  SysLine(x2, y1, x2, y2);
  SysLine(x2, y2, x1, y2);
  SysLine(x1, y2, x1, y1);
}

void SysFillRect(UINT8 x1, UINT8 y1, UINT8 x2, UINT8 y2) {
  UINT16 x;
  UINT16 y;
  if (!in_screen(x1, y1) || !in_screen(x2, y2)) return;
  normalize_rect(&x1, &y1, &x2, &y2);
  for (y = y1; y <= y2; ++y)
    for (x = x1; x <= x2; ++x) SysPutPixel((UINT8)x, (UINT8)y, 1);
  screen_changed();
}

void SysLcdPartClear(UINT8 x1, UINT8 y1, UINT8 x2, UINT8 y2) {
  UINT16 x;
  UINT16 y;
  if (!in_screen(x1, y1) || !in_screen(x2, y2)) return;
  normalize_rect(&x1, &y1, &x2, &y2);
  for (y = y1; y <= y2; ++y)
    for (x = x1; x <= x2; ++x) SysPutPixel((UINT8)x, (UINT8)y, 0);
  screen_changed();
}

void SysLcdReverse(UINT8 x1, UINT8 y1, UINT8 x2, UINT8 y2) {
  UINT16 x;
  UINT16 y;
  if (!in_screen(x1, y1) || !in_screen(x2, y2)) return;
  normalize_rect(&x1, &y1, &x2, &y2);
  for (y = y1; y <= y2; ++y) {
    for (x = x1; x <= x2; ++x) {
      UINT8* pixel = MCU_memory + 0x400 + 20 * y + x / 8;
      *pixel ^= (UINT8)(0x80U >> (x & 7));
      track_battle_pixel((UINT8)x, (UINT8)y);
    }
  }
  screen_changed();
}

void SysSaveScreen(UINT8 x1, UINT8 y1, UINT8 x2, UINT8 y2, UINT8* buffer) {
  UINT16 row;
  UINT8 first_byte;
  UINT8 last_byte;
  if (buffer == NULL || !in_screen(x1, y1) || !in_screen(x2, y2)) return;
  normalize_rect(&x1, &y1, &x2, &y2);
  first_byte = (UINT8)(x1 / 8);
  last_byte = (UINT8)(x2 / 8);
  for (row = y1; row <= y2; ++row) {
    UINT16 width = (UINT16)(last_byte - first_byte + 1);
    memcpy(buffer + (row - y1) * width,
           MCU_memory + 0x400 + row * 20 + first_byte, width);
  }
  if (battle_overlay_tracking || wide_map_overlay_tracking) {
    BattleMaskSave* saved = NULL;
    UINT8 index;
    UINT16 offset = 0;
    for (index = 0; index < BATTLE_MASK_SAVE_CAPACITY; ++index) {
      if (battle_mask_saves[index].screen_buffer == buffer) {
        saved = &battle_mask_saves[index];
        break;
      }
    }
    if (saved == NULL) {
      saved = &battle_mask_saves[battle_mask_save_cursor];
      battle_mask_save_cursor =
          (UINT8)((battle_mask_save_cursor + 1U) % BATTLE_MASK_SAVE_CAPACITY);
    }
    saved->screen_buffer = buffer;
    saved->first_byte = first_byte;
    saved->last_byte = last_byte;
    saved->y1 = y1;
    saved->y2 = y2;
    for (row = y1; row <= y2; ++row) {
      UINT16 width = (UINT16)(last_byte - first_byte + 1U);
      memcpy(saved->data + offset,
             battle_overlay_mask + row * 20U + first_byte, width);
      memcpy(saved->wide_map_data + offset,
             wide_map_overlay_mask + row * 20U + first_byte, width);
      offset = (UINT16)(offset + width);
    }
    saved->length = offset;
  }
}

void SysRestoreScreen(UINT8 x1, UINT8 y1, UINT8 x2, UINT8 y2,
                      UINT8* buffer) {
  UINT16 row;
  UINT8 first_byte;
  UINT8 last_byte;
  if (buffer == NULL || !in_screen(x1, y1) || !in_screen(x2, y2)) return;
  normalize_rect(&x1, &y1, &x2, &y2);
  first_byte = (UINT8)(x1 / 8);
  last_byte = (UINT8)(x2 / 8);
  for (row = y1; row <= y2; ++row) {
    UINT16 width = (UINT16)(last_byte - first_byte + 1);
    memcpy(MCU_memory + 0x400 + row * 20 + first_byte,
           buffer + (row - y1) * width, width);
  }
  if (battle_overlay_tracking || wide_map_overlay_tracking) {
    BattleMaskSave* saved = NULL;
    UINT8 index;
    UINT16 offset = 0;
    for (index = 0; index < BATTLE_MASK_SAVE_CAPACITY; ++index) {
      if (battle_mask_saves[index].screen_buffer == buffer &&
          battle_mask_saves[index].first_byte == first_byte &&
          battle_mask_saves[index].last_byte == last_byte &&
          battle_mask_saves[index].y1 == y1 &&
          battle_mask_saves[index].y2 == y2) {
        saved = &battle_mask_saves[index];
        break;
      }
    }
    for (row = y1; row <= y2; ++row) {
      UINT16 width = (UINT16)(last_byte - first_byte + 1U);
      if (battle_overlay_tracking) {
        if (saved != NULL && offset + width <= saved->length) {
          memcpy(battle_overlay_mask + row * 20U + first_byte,
                 saved->data + offset, width);
        } else {
          memset(battle_overlay_mask + row * 20U + first_byte, 0xFF, width);
        }
      }
      if (wide_map_overlay_tracking) {
        if (saved != NULL && offset + width <= saved->length) {
          memcpy(wide_map_overlay_mask + row * 20U + first_byte,
                 saved->wide_map_data + offset, width);
        } else {
          memset(wide_map_overlay_mask + row * 20U + first_byte, 0xFF,
                 width);
        }
      }
      offset = (UINT16)(offset + width);
    }
  }
  screen_changed();
}

void SysPictureDummy(UINT8 x1, UINT8 y1, UINT8 x2, UINT8 y2, UINT8* picture,
                     UINT8* screen, UINT8 flag) {
  UINT16 x;
  UINT16 y;
  UINT16 stride;
  if (picture == NULL || screen == NULL || !in_screen(x1, y1) ||
      !in_screen(x2, y2))
    return;
  normalize_rect(&x1, &y1, &x2, &y2);
  {
    UINT8* draw_buffer = MCU_memory + *(UINT16*)(MCU_memory + 0x1936);
    UINT8 is_full_frame =
        x1 == 0 && y1 == 0 && x2 == FMJ_ENGINE_SCREEN_WIDTH - 2 &&
        y2 == FMJ_ENGINE_SCREEN_HEIGHT - 1;
    UINT8 is_frame_presentation =
        is_full_frame && screen == MCU_memory + 0x400 &&
        picture == draw_buffer;
    UINT8 is_snapshot_restore = is_full_frame && screen == draw_buffer &&
                                picture == MCU_memory + 0x4000 &&
                                (battle_snapshot_mask_valid ||
                                 wide_map_snapshot_mask_valid);
    if (is_snapshot_restore) {
      if (battle_overlay_tracking && battle_snapshot_mask_valid) {
        memcpy(battle_overlay_mask, battle_snapshot_mask,
               sizeof(battle_overlay_mask));
      }
      if (wide_map_overlay_tracking && wide_map_snapshot_mask_valid) {
        memcpy(wide_map_overlay_mask, wide_map_snapshot_mask,
               sizeof(wide_map_overlay_mask));
      }
    } else if (is_battle_draw_buffer(screen) && !is_frame_presentation) {
      for (y = y1; y <= y2; ++y)
        for (x = x1; x <= x2; ++x)
          track_battle_pixel((UINT8)x, (UINT8)y);
    }
  }
  stride = (UINT16)(((x2 - x1 + 1) + 7) & 0xF8);
  for (y = y1; y <= y2; ++y) {
    for (x = x1; x <= x2; ++x) {
      UINT16 source_bit = (UINT16)((y - y1) * stride + (x - x1));
      UINT8 source = (UINT8)((picture[source_bit / 8] >>
                              (7 - (source_bit & 7))) &
                             1);
      UINT8* destination = screen + y * 20 + x / 8;
      UINT8 mask = (UINT8)(0x80U >> (x & 7));
      if ((source != 0) == (flag == 0))
        *destination |= mask;
      else
        *destination &= (UINT8)~mask;
    }
  }
}

void SysPicture(UINT8 x1, UINT8 y1, UINT8 x2, UINT8 y2, UINT8* picture,
                UINT8 flag) {
  SysPictureDummy(x1, y1, x2, y2, picture, MCU_memory + 0x400, flag);
  screen_changed();
}

void SysPrintString(UINT8 x, UINT8 y, const UINT8* text) {
  UINT8 glyph[32];
  while (text != NULL && *text != 0 && y < 0x51) {
    UINT8 width = *text < 0x80 ? 8 : 16;
    UINT8 consumed = *text < 0x80 ? 1 : 2;
    if ((UINT16)x + width > FMJ_ENGINE_SCREEN_WIDTH) {
      x = 0;
      y = (UINT8)(y + 16);
      continue;
    }
    memset(glyph, 0, sizeof(glyph));
    if (host.load_glyph != NULL &&
        host.load_glyph(host.context, text, glyph, &width, &consumed)) {
      SysPicture(x, y, (UINT8)(x + width - 1), (UINT8)(y + 15), glyph, 0);
    }
    x = (UINT8)(x + width);
    text += consumed != 0 ? consumed : 1;
  }
}

void SysAscii(UINT8 x, UINT8 y, UINT8 ascii) {
  UINT8 text[2] = {ascii, 0};
  SysPrintString(x, y, text);
}

UINT8 SysGetSecond(void) { return (UINT8)((host_millis() / 1000U) % 60U); }

void SysTimer1Open(UINT8 times) {
  timer_period_ms = (UINT32)times * 10U;
  timer_due_ms = host_millis() + timer_period_ms;
}

void SysTimer1Close(void) { timer_period_ms = 0; }

void SysIconAllClear(void) {}

void DataBankSwitch(UINT8 logicStartBank, UINT8 bankNumber,
                    UINT16 physicalStartBank) {
  if (logicStartBank == 0x04 && bankNumber == 0x01) {
    memcpy(MCU_memory_dummy + bank4 * 0x1000, MCU_memory + 0x4000, 0x1000);
    bank4 = physicalStartBank;
    memcpy(MCU_memory + 0x4000, MCU_memory_dummy + bank4 * 0x1000, 0x1000);
  } else if (logicStartBank == 0x09 && bankNumber == 0x04 &&
             host.read_game != NULL) {
    bank9 = physicalStartBank;
    if (!host.read_game(host.context, (UINT32)physicalStartBank * 0x1000U,
                        MCU_memory + 0x9000, 0x4000U)) {
      memset(MCU_memory + 0x9000, 0, 0x4000);
    }
  }
}

void GetDataBankNumber(UINT8 logicStartBank, UINT16* physicalBankNumber) {
  if (logicStartBank == 0x09 && physicalBankNumber != NULL)
    *physicalBankNumber = bank9;
}

void SysSetKeySound(UINT8 enabled) { key_sound = enabled; }

UINT8 SysGetKeySound(void) { return key_sound; }

UINT8 SysGetKey(void) {
  if (stop_requested) return KEY_EXIT;
  return host.poll_key != NULL ? host.poll_key(host.context) : 0xFF;
}

void SysPlayMelody(UINT8 melody) {
  if (host.play_melody != NULL) host.play_melody(host.context, melody);
}

void SysStopMelody(void) {
  if (host.stop_melody != NULL) host.stop_melody(host.context);
}

void SysMemInit(UINT16 start, UINT16 length) {
  UINT16 misalignment;
  MCB* block;
  fillmem(MCU_memory + start, length, 0);
  misalignment = (UINT16)(start & MIN_BLK_MASK);
  if (misalignment != 0) {
    UINT16 adjustment = (UINT16)(MIN_BLK_BYTES - misalignment);
    start = (UINT16)(start + adjustment);
    length = length > adjustment ? (UINT16)(length - adjustment) : 0;
  }
  length &= MIN_BLK_NMASK;
  Mem_Start = start;
  Mem_Len = length;
  block = (MCB*)(MCU_memory + start);
  block->use_flag = MCB_BLANK;
  block->end_flag = MCB_END;
  block->len = (UINT16)(length - MCB_LENGTH);
  Mem_Flag = MEM_OK;
}

static UINT8 block_valid(MCB* block) {
  UINT8* end;
  if (block == NULL ||
      (block->end_flag != MCB_NORMAL && block->end_flag != MCB_END)) {
    Mem_Flag = MEM_MCB_ERROR;
    return 0;
  }
  end = (UINT8*)block + MCB_LENGTH + block->len;
  if ((UINT8*)block < MCU_memory + Mem_Start ||
      end > MCU_memory + Mem_Start + Mem_Len) {
    Mem_Flag = MEM_MCB_ERROR;
    return 0;
  }
  return 1;
}

static MCB* next_block(MCB* block) {
  MCB* next;
  if (!block_valid(block) || block->end_flag == MCB_END) return NULL;
  next = (MCB*)((UINT8*)block + MCB_LENGTH + block->len);
  return block_valid(next) ? next : NULL;
}

static void merge_free_blocks(void) {
  MCB* block = (MCB*)(MCU_memory + Mem_Start);
  while (block != NULL && block->end_flag == MCB_NORMAL) {
    MCB* next = next_block(block);
    if (next == NULL) return;
    if (block->use_flag == MCB_BLANK && next->use_flag == MCB_BLANK) {
      block->len = (UINT16)(block->len + MCB_LENGTH + next->len);
      block->end_flag = next->end_flag;
    } else {
      block = next;
    }
  }
}

UINT8* SysMemAllocate(UINT16 length) {
  MCB* block;
  if ((length & MIN_BLK_MASK) != 0)
    length = (UINT16)((length + MIN_BLK_BYTES) & MIN_BLK_NMASK);
  block = (MCB*)(MCU_memory + Mem_Start);
  while (block != NULL) {
    if (block->use_flag == MCB_BLANK && block->len >= length) {
      if ((UINT16)(block->len - length) > MCB_LENGTH) {
        UINT8 old_end = block->end_flag;
        UINT16 old_length = block->len;
        MCB* remainder;
        block->len = length;
        block->end_flag = MCB_NORMAL;
        remainder = (MCB*)((UINT8*)block + MCB_LENGTH + length);
        remainder->use_flag = MCB_BLANK;
        remainder->end_flag = old_end;
        remainder->len = (UINT16)(old_length - length - MCB_LENGTH);
      }
      block->use_flag = MCB_USE;
      return (UINT8*)block + MCB_LENGTH;
    }
    block = next_block(block);
  }
  return NULL;
}

UINT8 SysMemFree(UINT8* pointer) {
  MCB* block;
  if (pointer == NULL || pointer < MCU_memory + Mem_Start + MCB_LENGTH ||
      pointer > MCU_memory + Mem_Start + Mem_Len)
    return 0;
  block = (MCB*)(MCU_memory + Mem_Start);
  while (block != NULL) {
    if ((UINT8*)block + MCB_LENGTH == pointer) {
      block->use_flag = MCB_BLANK;
      merge_free_blocks();
      return 1;
    }
    block = next_block(block);
  }
  return 0;
}

UINT16 SysRand(PtrRandEnv environment) {
  environment->next = environment->next * 0x41C64E6DU + 0x3039U;
  return (UINT16)((environment->next / 0x10000U) %
                  ((UINT32)environment->randMax + 1U));
}

void SysSrand(PtrRandEnv environment, UINT16 seed, UINT16 maximum) {
  environment->next = seed;
  environment->randMax = maximum;
}

void SysMemcpy(UINT8* destination, const UINT8* source, UINT16 length) {
  memcpy(destination, source, length);
  if ((battle_overlay_tracking || wide_map_overlay_tracking) &&
      length == FMJ_ENGINE_SCREEN_BYTES) {
    UINT8* draw_buffer = MCU_memory + *(UINT16*)(MCU_memory + 0x1936);
    if (destination == MCU_memory + 0x4000 && source == draw_buffer) {
      if (battle_overlay_tracking) {
        memcpy(battle_snapshot_mask, battle_overlay_mask,
               sizeof(battle_snapshot_mask));
        battle_snapshot_mask_valid = 1;
      }
      if (wide_map_overlay_tracking) {
        memcpy(wide_map_snapshot_mask, wide_map_overlay_mask,
               sizeof(wide_map_snapshot_mask));
        wide_map_snapshot_mask_valid = 1;
      }
    } else if (battle_overlay_tracking && destination == draw_buffer &&
               source == MCU_memory + 0x4000 &&
               battle_snapshot_mask_valid) {
      memcpy(battle_overlay_mask, battle_snapshot_mask,
             sizeof(battle_overlay_mask));
    }
    if (wide_map_overlay_tracking &&
        destination == draw_buffer && source == MCU_memory + 0x4000 &&
        wide_map_snapshot_mask_valid) {
      memcpy(wide_map_overlay_mask, wide_map_snapshot_mask,
             sizeof(wide_map_overlay_mask));
    }
  }
}

UINT8 SysMemcmp(UINT8* destination, const UINT8* source, UINT16 length) {
  return (UINT8)memcmp(destination, source, length);
}

void GuiSetInputFilter(UINT8 filter) { (void)filter; }
void GuiSetKbdType(UINT8 type) { (void)type; }

UINT8 GuiPushMsg(PtrMsg message) {
  PtrMsg queue = (PtrMsg)(MCU_memory + 0x2B0F);
  if (message == NULL || MCU_memory[0x2B0B] >= 8) return 0;
  MCU_memory[0x2B09] = (UINT8)((MCU_memory[0x2B09] - 1) & 7);
  queue[MCU_memory[0x2B09]] = *message;
  ++MCU_memory[0x2B0B];
  return 1;
}

UINT8 GuiGetMsg(PtrMsg message) {
  PtrMsg queue = (PtrMsg)(MCU_memory + 0x2B0F);
  if (message == NULL) return 0;
  /* Flush direct SysPutPixel users before the engine blocks for an event. */
  screen_flush();
  for (;;) {
    if (timer_period_ms != 0 &&
        (INT32)(host_millis() - timer_due_ms) >= 0) {
      do {
        timer_due_ms += timer_period_ms;
      } while ((INT32)(host_millis() - timer_due_ms) >= 0);
      message->type = DICT_WM_TIMER;
      message->param = 0;
      return 1;
    }
    if (MCU_memory[0x2B0B] != 0) {
      *message = queue[MCU_memory[0x2B09]];
      --MCU_memory[0x2B0B];
      MCU_memory[0x2B09] = (UINT8)((MCU_memory[0x2B09] + 1) & 7);
      return 1;
    }
    {
      UINT8 key = SysGetKey();
      if (key != 0xFF) {
        message->type = DICT_WM_KEY;
        message->param = key;
        return 1;
      }
    }
    host_yield();
  }
}

UINT8 GuiTranslateMsg(PtrMsg message) {
  static const UINT8 keymap[0x40][2] = {
      {DICT_WM_CHAR_FUN, CHAR_ON_OFF}, {DICT_WM_CHAR_FUN, CHAR_HOME_MENU},
      {DICT_WM_CHAR_FUN, CHAR_EC_DICT}, {DICT_WM_CHAR_FUN, CHAR_CE_DICT},
      {0, 0}, {0, 0}, {0, 0}, {0, 0},
      {DICT_WM_CHAR_ASC, '1'}, {DICT_WM_CHAR_ASC, '2'},
      {DICT_WM_CHAR_ASC, '3'}, {DICT_WM_CHAR_ASC, '4'},
      {DICT_WM_CHAR_ASC, '5'}, {DICT_WM_CHAR_ASC, '6'},
      {DICT_WM_CHAR_ASC, '7'}, {DICT_WM_CHAR_ASC, '8'},
      {DICT_WM_CHAR_ASC, 'q'}, {DICT_WM_CHAR_ASC, 'w'},
      {DICT_WM_CHAR_ASC, 'e'}, {DICT_WM_CHAR_ASC, 'r'},
      {DICT_WM_CHAR_ASC, 't'}, {DICT_WM_CHAR_ASC, 'y'},
      {DICT_WM_CHAR_ASC, 'u'}, {DICT_WM_CHAR_ASC, 'i'},
      {DICT_WM_CHAR_ASC, 'a'}, {DICT_WM_CHAR_ASC, 's'},
      {DICT_WM_CHAR_ASC, 'd'}, {DICT_WM_CHAR_ASC, 'f'},
      {DICT_WM_CHAR_ASC, 'g'}, {DICT_WM_CHAR_ASC, 'h'},
      {DICT_WM_CHAR_ASC, 'j'}, {DICT_WM_CHAR_ASC, 'k'},
      {DICT_WM_CHAR_FUN, CHAR_INPUT}, {DICT_WM_CHAR_ASC, 'z'},
      {DICT_WM_CHAR_ASC, 'x'}, {DICT_WM_CHAR_ASC, 'c'},
      {DICT_WM_CHAR_ASC, 'v'}, {DICT_WM_CHAR_ASC, 'b'},
      {DICT_WM_CHAR_ASC, 'n'}, {DICT_WM_CHAR_ASC, 'm'},
      {DICT_WM_CHAR_FUN, CHAR_ZY}, {0, 0}, {0, 0}, {0, 0},
      {0, 0}, {DICT_WM_CHAR_FUN, CHAR_SHIFT},
      {DICT_WM_CHAR_FUN, CHAR_EXIT}, {DICT_WM_CHAR_FUN, CHAR_ENTER},
      {DICT_WM_CHAR_ASC, '9'}, {DICT_WM_CHAR_ASC, '0'},
      {DICT_WM_CHAR_ASC, 'o'}, {DICT_WM_CHAR_ASC, 'p'},
      {DICT_WM_CHAR_ASC, 'l'}, {DICT_WM_CHAR_FUN, CHAR_UP},
      {DICT_WM_CHAR_ASC, ' '}, {DICT_WM_CHAR_FUN, CHAR_LEFT},
      {DICT_WM_CHAR_FUN, CHAR_DOWN}, {DICT_WM_CHAR_FUN, CHAR_RIGHT},
      {DICT_WM_CHAR_FUN, CHAR_PGUP}, {DICT_WM_CHAR_FUN, CHAR_PGDN},
      {0, 0}, {0, 0}, {0, 0}, {0, 0}};
  UINT8 key;
  if (message == NULL || message->type != DICT_WM_KEY) return 1;
  key = (UINT8)(message->param & 0x3F);
  message->type = keymap[key][0];
  message->param = keymap[key][1];
  return 1;
}

UINT8 GuiInit(void) {
  memset(MCU_memory + 0x400, 0xFF, FMJ_ENGINE_SCREEN_BYTES);
  MCU_memory[0x2B09] = 0;
  MCU_memory[0x2B0B] = 0;
  screen_changed();
  return 0;
}

UINT16 GuiGetKbdState(void) { return 0; }
void GuiSetKbdState(UINT16 state) { (void)state; }

void SysCalcScrBufSize(UINT8 x1, UINT8 y1, UINT8 x2, UINT8 y2,
                       UINT16* byteCount) {
  if (byteCount == NULL) return;
  if (!in_screen(x1, y1) || !in_screen(x2, y2)) {
    *byteCount = 0;
    return;
  }
  normalize_rect(&x1, &y1, &x2, &y2);
  *byteCount = (UINT16)((x2 / 8 - x1 / 8 + 1) * (y2 - y1 + 1));
}

UINT8 GuiMsgBox(UINT8* message, UINT16 timeout) {
  MsgType event;
  UINT16 ticks = 0;
  UINT16 saved_bytes = 0;
  UINT32 saved_period = timer_period_ms;
  UINT8* saved_screen;
  if (message == NULL || *message == 0) return 0xFF;
  SysCalcScrBufSize(11, 7, 148, 88, &saved_bytes);
  saved_screen = SysMemAllocate(saved_bytes);
  if (saved_screen == NULL) return 0xFE;
  SysSaveScreen(11, 7, 148, 88, saved_screen);
  SysLcdPartClear(11, 7, 148, 88);
  SysRect(11, 7, 148, 88);
  SysPrintString(15, 11, message);
  if (timeout != 0) SysTimer1Open(1);
  for (;;) {
    GuiGetMsg(&event);
    if (event.type == DICT_WM_KEY) break;
    if (event.type == DICT_WM_TIMER && timeout != 0 && ++ticks >= timeout)
      break;
  }
  if (saved_period != 0)
    SysTimer1Open((UINT8)(saved_period / 10U));
  else
    SysTimer1Close();
  SysRestoreScreen(11, 7, 148, 88, saved_screen);
  SysMemFree(saved_screen);
  return 1;
}

UINT8 FileCreat(UINT8 filetype, UINT32 length, UINT8* information,
                UINT16* filename, UINT8* handle) {
  return host.file_create != NULL
             ? host.file_create(host.context, filetype, length, information,
                                filename, handle)
             : 0;
}

UINT8 FileOpen(UINT16 filename, UINT8 filetype, UINT8 openmode, UINT8* handle,
               UINT32* length) {
  return host.file_open != NULL
             ? host.file_open(host.context, filename, filetype, openmode,
                              handle, length)
             : 0;
}

UINT8 FileDel(UINT8 handle) {
  return host.file_delete != NULL ? host.file_delete(host.context, handle) : 0;
}

UINT8 FileWrite(UINT8 handle, UINT8 length, UINT8* source) {
  return host.file_write != NULL
             ? host.file_write(host.context, handle, length, source)
             : 0;
}

UINT8 FileClose(UINT8 handle) {
  return host.file_close != NULL ? host.file_close(host.context, handle) : 0;
}

UINT8 FileRead(UINT8 handle, UINT8 length, UINT8* destination) {
  return host.file_read != NULL
             ? host.file_read(host.context, handle, length, destination)
             : 0;
}

UINT8 FileSeek(UINT8 handle, UINT32 offset, UINT8 origin) {
  return host.file_seek != NULL
             ? host.file_seek(host.context, handle, offset, origin)
             : 0;
}

void FlashInit(void) {}

UINT8 FileNum(UINT8 filetype, UINT16* count) {
  if (count != NULL) *count = 0;
  return host.file_count != NULL
             ? host.file_count(host.context, filetype, count)
             : 0;
}

UINT8 FileSearch(UINT8 filetype, UINT16 order, UINT16* filename,
                 UINT8* information) {
  return host.file_search != NULL
             ? host.file_search(host.context, filetype, order, filename,
                                information)
             : 0;
}

UINT8* fmj_engine_itoa(int value, UINT8* destination, int radix) {
  UINT8 temporary[34];
  UINT8* cursor = temporary;
  unsigned int magnitude;
  UINT8 negative = 0;
  if (destination == NULL || radix < 2 || radix > 36) return destination;
  if (value < 0 && radix == 10) {
    negative = 1;
    magnitude = (unsigned int)(-(value + 1)) + 1U;
  } else {
    magnitude = (unsigned int)value;
  }
  do {
    unsigned int digit = magnitude % (unsigned int)radix;
    *cursor++ = (UINT8)(digit < 10 ? '0' + digit : 'a' + digit - 10);
    magnitude /= (unsigned int)radix;
  } while (magnitude != 0);
  if (negative) *cursor++ = '-';
  {
    UINT8* output = destination;
    while (cursor != temporary) *output++ = *--cursor;
    *output = 0;
  }
  return destination;
}
