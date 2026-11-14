#pragma once
#ifndef S32X_H
#define S32X_H

extern INT32 S32XMode;
extern INT32 (*S32XMainCycles)(); // main cpu cycles done in the current frame
extern void (*S32XCartRemap)();    // restores the cartridge handlers (sram...) when the rom is mapped at 0
extern void (*S32XCartWrite)(UINT32 address, UINT8 data); // $a130xx writes

// main cpu (cpu #0) must be initialised and closed when calling these
INT32 S32XInit(UINT8 *rom, INT32 rom_size);
void S32XExit();
void S32XReset();
void S32XSetPAL(INT32 pal);

// frame handling, called with cpu #0 open
void S32XLineStart(INT32 line, INT32 lines_vis, INT32 main_cycles);
void S32XRunSh2(INT32 main_cycles);
void S32XEndFrame(INT32 main_cycles);

// video / sound
void S32XRecalcPal();
void S32XDrawLine(INT32 line, UINT8 *md_line, UINT16 *dest, UINT32 *md_pal, INT32 backdrop, INT32 h40);
void S32XSoundUpdate(INT16 *output, INT32 samples);

INT32 S32XScan(INT32 nAction, INT32 *pnMin);

#endif
