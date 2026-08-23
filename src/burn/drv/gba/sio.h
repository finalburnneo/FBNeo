#pragma once

#ifndef GBA_SIO_H
#define GBA_SIO_H

#include "gba.h"

#define GBA_SIO_TRANSFER_TICKS	(8 * 8)

static inline void gba_sio_event(gba_t* gba, sb_emu_state_t* /*emu*/, UINT32 /*cycles_late*/)
{
	UINT16 siocnt = gba_io_read16(gba, GBA_SIOCNT);
	bool irq_enabled = SB_BFE(siocnt, 14, 1);
	if (irq_enabled)
		gba_send_interrupt(gba, 4, 1 << GBA_INT_SERIAL);
	siocnt &= ~(1 << 7);
	gba_io_store16(gba, GBA_SIOCNT,    siocnt);
	gba->sio.last_active = false;
	gba_io_store8( gba, GBA_SIODATA8,  0);
	gba_io_store32(gba, GBA_SIODATA32, 0);
}

#endif
