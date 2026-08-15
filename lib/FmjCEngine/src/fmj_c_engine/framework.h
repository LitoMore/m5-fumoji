/*
 * Portable type layer for the FMJ C engine.
 * Copyright (C) 2026 LitoMore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef int16_t INT16;
typedef int32_t INT32;

UINT8* fmj_engine_itoa(int value, UINT8* destination, int radix);

#define _itoa fmj_engine_itoa
