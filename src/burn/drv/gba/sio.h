#pragma once

#ifndef GBA_SIO_H
#define GBA_SIO_H

#include "gba.h"

static inline void gba_tick_sio(gba_t* gba)
{
	//Just a stub for now;
	UINT16 siocnt = gba_io_read16(gba, GBA_SIOCNT);
	bool active      = SB_BFE(siocnt,  7, 1);
	bool irq_enabled = SB_BFE(siocnt, 14, 1);
	if (active) {
		if (gba->sio.last_active == false) {
			gba->sio.last_active = true;
			gba->sio.ticks_till_transfer_done = 8 * 8;
		}
		bool internal_clock = SB_BFE(siocnt, 0, 1);
		if (internal_clock)
			gba->sio.ticks_till_transfer_done--;
		if (gba->sio.ticks_till_transfer_done <= 0) {
			if (irq_enabled) {
				UINT16 if_bit = 1 << (GBA_INT_SERIAL);
				gba_send_interrupt(gba, 4, if_bit);
			}
			siocnt &= ~(1 << 7);
			gba_io_store16(gba, GBA_SIOCNT,    siocnt);
			gba->sio.last_active = false;
			gba_io_store8( gba, GBA_SIODATA8,  0);
			gba_io_store32(gba, GBA_SIODATA32, 0);
		}

	}
}

#endif
