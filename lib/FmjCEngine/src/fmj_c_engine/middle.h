/*
 * Portable BBK system-call declarations for the FMJ C engine.
 * Copyright (C) 2026 LitoMore
 * SPDX-License-Identifier: GPL-2.0-only
 */
#pragma once

#include "dictsys.h"
#include "engine_port.h"
#include "keytable.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MEM_OK 0
#define MEM_MCB_ERROR 1
#define MCB_BLANK 'b'
#define MCB_USE 'u'
#define MCB_END 'e'
#define MCB_NORMAL 'n'
#define MCB_LENGTH 4
#define MIN_BLK_BYTES 4
#define MIN_BLK_MASK 0x03
#define MIN_BLK_NMASK 0xfffc

typedef struct tagMCB {
  UINT8 use_flag;
  UINT8 end_flag;
  UINT16 len;
} MCB;

void fillmem(UINT8* destination, UINT16 size, UINT8 value);
void SysPrintString(UINT8 x, UINT8 y, const UINT8* text);
void SysPutPixel(UINT8 x, UINT8 y, UINT8 value);
void SysLine(UINT8 x1, UINT8 y1, UINT8 x2, UINT8 y2);
void SysRect(UINT8 x1, UINT8 y1, UINT8 x2, UINT8 y2);
void SysFillRect(UINT8 x1, UINT8 y1, UINT8 x2, UINT8 y2);
void SysSaveScreen(UINT8 x1, UINT8 y1, UINT8 x2, UINT8 y2, UINT8* buffer);
void SysRestoreScreen(UINT8 x1, UINT8 y1, UINT8 x2, UINT8 y2,
                      UINT8* buffer);
void SysPicture(UINT8 x1, UINT8 y1, UINT8 x2, UINT8 y2, UINT8* picture,
                UINT8 flag);
void SysPictureDummy(UINT8 x1, UINT8 y1, UINT8 x2, UINT8 y2, UINT8* picture,
                     UINT8* screen, UINT8 flag);
void SysLcdPartClear(UINT8 x1, UINT8 y1, UINT8 x2, UINT8 y2);
void SysAscii(UINT8 x, UINT8 y, UINT8 ascii);
void SysLcdReverse(UINT8 x1, UINT8 y1, UINT8 x2, UINT8 y2);
UINT8 SysGetSecond(void);
void SysTimer1Open(UINT8 times);
void SysTimer1Close(void);
void SysIconAllClear(void);
void DataBankSwitch(UINT8 logicStartBank, UINT8 bankNumber,
                    UINT16 physicalStartBank);
void GetDataBankNumber(UINT8 logicStartBank, UINT16* physicalBankNumber);
void SysSetKeySound(UINT8 enabled);
UINT8 SysGetKeySound(void);
UINT8 SysGetKey(void);
void SysPlayMelody(UINT8 melody);
void SysStopMelody(void);
void FmjEngineNotifyWideMapBegin(void);
void FmjEngineNotifyWideMapReady(void);
void FmjEngineNotifyWideMapEnd(void);
void FmjEngineNotifyBattleBegin(void);
void FmjEngineNotifyBattleBackgroundReady(void);
void FmjEngineNotifyBattleEnd(void);
void FmjEngineTrackTransparentPicture(UINT8 x, UINT8 y, UINT8 width,
                                      UINT8 height, const UINT8* picture);
void FmjEngineExcludeBattleOverlayRect(UINT8 x1, UINT8 y1, UINT8 x2,
                                       UINT8 y2);
void SysMemInit(UINT16 start, UINT16 length);
UINT8* SysMemAllocate(UINT16 length);
UINT8 SysMemFree(UINT8* pointer);
UINT16 SysRand(PtrRandEnv environment);
void SysSrand(PtrRandEnv environment, UINT16 seed, UINT16 maximum);
void SysMemcpy(UINT8* destination, const UINT8* source, UINT16 length);
UINT8 SysMemcmp(UINT8* destination, const UINT8* source, UINT16 length);
void GuiSetInputFilter(UINT8 filter);
void GuiSetKbdType(UINT8 type);
UINT8 GuiPushMsg(PtrMsg message);
UINT8 GuiGetMsg(PtrMsg message);
UINT8 GuiTranslateMsg(PtrMsg message);
UINT8 GuiInit(void);
UINT16 GuiGetKbdState(void);
void GuiSetKbdState(UINT16 state);
void SysCalcScrBufSize(UINT8 x1, UINT8 y1, UINT8 x2, UINT8 y2,
                       UINT16* byteCount);
UINT8 GuiMsgBox(UINT8* message, UINT16 timeout);

#define NoOpen 0x00
#define ReadOnly 0x01
#define ReadAndWrite 0x02
#define FromTop 0x01
#define FromCurrent 0x02
#define FromEnd 0x03

UINT8 FileCreat(UINT8 filetype, UINT32 length, UINT8* information,
                UINT16* filename, UINT8* handle);
UINT8 FileOpen(UINT16 filename, UINT8 filetype, UINT8 openmode,
               UINT8* handle, UINT32* length);
UINT8 FileDel(UINT8 handle);
UINT8 FileWrite(UINT8 handle, UINT8 length, UINT8* source);
UINT8 FileClose(UINT8 handle);
UINT8 FileRead(UINT8 handle, UINT8 length, UINT8* destination);
UINT8 FileSeek(UINT8 handle, UINT32 offset, UINT8 origin);
void FlashInit(void);
UINT8 FileNum(UINT8 filetype, UINT16* count);
UINT8 FileSearch(UINT8 filetype, UINT16 order, UINT16* filename,
                 UINT8* information);

#ifdef __cplusplus
}
#endif
