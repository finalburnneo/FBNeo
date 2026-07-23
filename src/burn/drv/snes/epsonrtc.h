#ifndef EPSONRTC_H
#define EPSONRTC_H

#include "statehandler.h"
void  snes_epsonrtc_power();	// cold power-on (cartridge inserted): clear + seed clock from host wall-clock
void  snes_epsonrtc_reset();	// warm reset (console reset): serial interface only; clock is battery-backed
UINT8 snes_epsonrtc_read(UINT8 address);
void  snes_epsonrtc_write(UINT8 address, UINT8 data);
void  snes_epsonrtc_handleState(StateHandler* sh);

#endif
