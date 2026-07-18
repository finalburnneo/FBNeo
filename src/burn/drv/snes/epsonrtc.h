#ifndef EPSONRTC_H
#define EPSONRTC_H

#include "statehandler.h"

// Epson RTC-4513 Real-Time Clock (used by SPC7110 board Tengai Makyou Zero)
// Ported from ares (ares/sfc/coprocessor/epsonrtc), FBNeo flat-C style.
// Driven from host wall-clock time (advances in real time); the ares clock thread
// is replaced by an on-access real-time catch-up.
//
// address is the low 2 bits of the bus address ($4840-4842 -> 0..2).

void  snes_epsonrtc_power();	// cold power-on (cartridge inserted): clear + seed clock from host wall-clock
void  snes_epsonrtc_reset();	// warm reset (console reset): serial interface only; clock is battery-backed
UINT8 snes_epsonrtc_read(UINT8 address);
void  snes_epsonrtc_write(UINT8 address, UINT8 data);
void  snes_epsonrtc_handleState(StateHandler* sh);

#endif
