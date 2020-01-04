// Atari EA-ROM, impl. by dink

#include "burnint.h"
#include "earom.h"

#define EAROM_SIZE	0x40
static UINT8 earom_offset;
static UINT8 earom_data;
static UINT8 earom[EAROM_SIZE];

UINT8 earom_read(UINT16 /*address*/)
{
	return (earom_data);
}

void earom_write(UINT16 offset, UINT8 data)
{
	earom_offset = offset;
	earom_data = data;
}

void earom_ctrl_write(UINT16 /*offset*/, UINT8 data)
{
	if (data & 0x01)
		earom_data = earom[earom_offset];
	if ((data & 0x0c) == 0x0c) {
		earom[earom_offset] = earom_data;
	}
}

void earom_reset()
{
	earom_offset = 0;
	earom_data = 0;
}

void earom_init()
{
	memset(&earom, 0, sizeof(earom));
	earom_reset();
}

void earom_exit()
{
	// N/A
}

void earom_scan(INT32 nAction, INT32 *pnMin)
{
	if (nAction & ACB_VOLATILE) {
		SCAN_VAR(earom_offset);
		SCAN_VAR(earom_data);
	}

	if (nAction & ACB_NVRAM) {
		struct BurnArea ba;

		memset(&ba, 0, sizeof(ba));
		ba.Data		= earom;
		ba.nLen		= sizeof(earom);
		ba.szName	= "NV RAM";
		BurnAcb(&ba);
	}
}
