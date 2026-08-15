/* Portable debug macros for the FMJ C engine. GPL-2.0-only. */
#pragma once

#ifdef FMJ_C_ENGINE_DEBUG
#include <stdio.h>
#define LOG(...) printf(__VA_ARGS__)
#else
#define LOG(...) ((void)0)
#endif

#define INIT_CONSOLE() ((void)0)
#define RELEASE_CONSOLE() ((void)0)
