#pragma once
#ifndef SEGACD_H
#define SEGACD_H

extern INT32 SegaCDMode;
extern INT32 (*SegaCDMainCycles)(); // main cpu cycles done in the current frame

// bios / region
INT32 SegaCDLoadBios(UINT8 *dest, INT32 base, INT32 region); // region -1: dip switch / disc
UINT8 SegaCDGetHardware();
INT32 SegaCDDiscNeeds32X();

// main cpu (cpu #0) must be initialised and closed when calling these
INT32 SegaCDInit(UINT8 *bios, INT32 mode1);
void SegaCDExit();
void SegaCDReset();
void SegaCDSetPAL(INT32 pal);

// frame handling, called with cpu #0 closed
void SegaCDNewFrame();
// called with cpu #0 open
void SegaCDRunSub(INT32 main_cycles);
void SegaCDEndFrame(INT32 main_cycles);
void SegaCDSoundUpdate(INT16 *output, INT32 samples);

INT32 SegaCDScan(INT32 nAction, INT32 *pnMin);

#endif
