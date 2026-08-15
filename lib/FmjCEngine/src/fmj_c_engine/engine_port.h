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
};

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

#ifdef __cplusplus
}
#endif
