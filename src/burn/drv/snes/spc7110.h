#ifndef SPC7110_H
#define SPC7110_H

#include "statehandler.h"

// SPC7110 coprocessor (Hudson) - ported from ares (primary) / bsnes-mercury (compare)
// FBNeo flat-C style, immediate-execution model (no separate clock thread).
//
// prom = program ROM, drom = data ROM (split from the cart image), ram = save SRAM.
// erom = expansion ROM (EXSPC7110 board, mapped $40-4f, bypasses the chip); eromSize 0 = absent.
// hasRTC selects whether an Epson RTC-4513 is present at $4840-4842 (Tengai Makyou Zero).

void  snes_spc7110_init(UINT8* rom, INT32 romSize, UINT8* ram, INT32 ramSize, INT32 promSize, INT32 eromSize, INT32 hasRTC);
void  snes_spc7110_exit();
void  snes_spc7110_reset();
void  snes_spc7110_handleState(StateHandler* sh);

UINT8 snes_spc7110_cart_read(UINT32 address, UINT8 openbus);
void  snes_spc7110_cart_write(UINT32 address, UINT8 data);

#endif
