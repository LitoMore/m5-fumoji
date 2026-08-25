/*
 * Host interface for the portable FMJ C engine.
 * Copyright (C) 2026 LitoMore
 * SPDX-License-Identifier: GPL-2.0-only
 */
#pragma once

#include "framework.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
  FMJ_ENGINE_SCREEN_WIDTH = 160,
  FMJ_ENGINE_SCREEN_HEIGHT = 96,
  FMJ_ENGINE_SCREEN_BYTES = 160 * 96 / 8,
  FMJ_ENGINE_WIDE_ACTOR_CAPACITY = 41,
};

typedef struct FmjEngineWideActor {
  UINT8 type;
  UINT8 world_x;
  UINT8 world_y;
  UINT8 direction;
  UINT8 step;
  UINT8 image_bank;
  UINT16 image_offset;
} FmjEngineWideActor;

typedef struct FmjEngineWideMapState {
  UINT16 base_bank;
  UINT8 map_width;
  UINT8 map_height;
  UINT8 camera_x;
  UINT8 camera_y;
  UINT8 tile_width;
  UINT8 tile_height;
  UINT8 map_bank;
  UINT16 map_offset;
  UINT8 tile_bank;
  UINT16 tile_offset;
  UINT8 actor_count;
  FmjEngineWideActor actors[FMJ_ENGINE_WIDE_ACTOR_CAPACITY];
  /* UI pixels written after the current map frame was completed. */
  UINT8 overlay_mask[FMJ_ENGINE_SCREEN_BYTES];
} FmjEngineWideMapState;

typedef struct FmjEngineBattleState {
  UINT8 background;
  UINT8 top_right;
  UINT8 bottom_left;
  /* Pixels written after the three static battle-background pictures. */
  UINT8 overlay_mask[FMJ_ENGINE_SCREEN_BYTES];
} FmjEngineBattleState;

typedef struct FmjEngineHost {
  void* context;
  UINT8 (*read_game)(void* context, UINT32 offset, UINT8* destination,
                     UINT32 length);
  UINT32 (*millis)(void* context);
  void (*yield)(void* context);
  UINT8 (*poll_key)(void* context);
  UINT8 (*load_glyph)(void* context, const UINT8* encoded,
                      UINT8 glyph[32], UINT8* width, UINT8* consumed);
  void (*screen_changed)(void* context);
  void (*screen_flush)(void* context);
  void (*wide_map_begin)(void* context);
  void (*wide_map_ready)(void* context);
  void (*wide_map_end)(void* context);
  void (*battle_begin)(void* context);
  void (*battle_end)(void* context);
  void (*play_melody)(void* context, UINT8 melody);
  void (*stop_melody)(void* context);

  UINT8 (*file_create)(void* context, UINT8 filetype, UINT32 length,
                       const UINT8 information[10], UINT16* filename,
                       UINT8* handle);
  UINT8 (*file_open)(void* context, UINT16 filename, UINT8 filetype,
                     UINT8 openmode, UINT8* handle, UINT32* length);
  UINT8 (*file_delete)(void* context, UINT8 handle);
  UINT8 (*file_write)(void* context, UINT8 handle, UINT8 length,
                      const UINT8* source);
  UINT8 (*file_close)(void* context, UINT8 handle);
  UINT8 (*file_read)(void* context, UINT8 handle, UINT8 length,
                     UINT8* destination);
  UINT8 (*file_seek)(void* context, UINT8 handle, UINT32 offset, UINT8 origin);
  UINT8 (*file_count)(void* context, UINT8 filetype, UINT16* count);
  UINT8 (*file_search)(void* context, UINT8 filetype, UINT16 order,
                       UINT16* filename, UINT8 information[10]);
} FmjEngineHost;

void FmjEngineSetHost(const FmjEngineHost* host);
UINT8 FmjEnginePrepare(void);
void FmjEngineRun(void);
void FmjEngineRequestStop(void);
const UINT8* FmjEngineScreen(void);
UINT8 FmjEngineRunning(void);
UINT8 FmjEngineGetWideMapState(FmjEngineWideMapState* state);
UINT8 FmjEngineGetBattleState(FmjEngineBattleState* state);

#ifdef __cplusplus
}
#endif
