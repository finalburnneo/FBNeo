#pragma once
#ifndef SVP_H
#define SVP_H

// Sega Virtua Processor (315-5750, Samsung SSP1601 core), Virtua Racing cartridge

extern INT32 SVPMode;
extern INT32 (*SVPMainCycles)(); // main cpu cycles done in the current frame

// main cpu (cpu #0) must be open when calling these
INT32 SVPInit(UINT8 *rom, INT32 rom_size);
void SVPExit();
void SVPReset();
void SVPSetPAL(INT32 pal);

void SVPRun(INT32 main_cycles);
void SVPEndFrame(INT32 main_cycles);

// vdp dma from the svp dram responds with a one-access delay
INT32 SVPDmaSource(UINT32 source);

INT32 SVPScan(INT32 nAction, INT32 *pnMin);

#endif
