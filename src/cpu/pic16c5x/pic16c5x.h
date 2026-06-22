// license:BSD-3-Clause
// copyright-holders:Tony La Porta
/*

  Microchip PIC16C5x Emulator

  Copyright Tony La Porta
  Originally written for the MAME project.

*/

#ifndef __PIC16C5X_H__
#define __PIC16C5X_H__

#include "pic16c5x_intf.h"

// input lines
enum
{
	PIC16C5x_RTCC = 0
};

void pic16c5xSetIRQLine(INT32 line, INT32 state);
void pic16c5xSetCallback(INT32 (*cb)(INT32));

// below is USUALLY used by the interface, only (not for driver-use)
void pic16c5xDoReset(INT32 type, INT32 *rom, INT32 *ram);
INT32 pic16c5xScanCpu(INT32 nAction, INT32* pnMin);
void pic16c5xSetConfig(UINT16 data);

#endif	/* __PIC16C5X_H__ */
