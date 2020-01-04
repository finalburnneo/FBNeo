// license:BSD-3-Clause
// copyright-holders:Fabio Priuli,Acho A. Tang, R. Belmont

#include "burnint.h"

static UINT8 K053251Ram[0x10];
static INT32 K053251PalIndex[6];

void K053251ResetIndexes()
{
	K053251PalIndex[0] = 32 * ((K053251Ram[9] >> 0) & 0x03);
	K053251PalIndex[1] = 32 * ((K053251Ram[9] >> 2) & 0x03);
	K053251PalIndex[2] = 32 * ((K053251Ram[9] >> 4) & 0x03);
	K053251PalIndex[3] = 16 * ((K053251Ram[10] >> 0) & 0x07);
	K053251PalIndex[4] = 16 * ((K053251Ram[10] >> 3) & 0x07);
}

void K053251Reset()
{
	memset (K053251Ram, 0, sizeof(K053251Ram));
	memset (K053251PalIndex, 0, sizeof(K053251PalIndex));
	K053251ResetIndexes();
}

void K053251Write(INT32 offset, INT32 data)
{
	data &= 0x3f;
	offset &= 0x0f;

	K053251Ram[offset] = data & 0xff;

	if (offset == 9)
	{
		for (INT32 i = 0; i < 3; i++) {
			K053251PalIndex[0+i] = 32 * ((data >> 2*i) & 0x03);
		}
	}
	else if (offset == 10)
	{
		for (INT32 i = 0; i < 2; i++) {
			K053251PalIndex[3+i] = 16 * ((data >> 3*i) & 0x07);
		}
	}
}

INT32 K053251GetPriority(INT32 idx)
{
	return K053251Ram[idx & 0x0f];
}

INT32 K053251GetPaletteIndex(INT32 idx)
{
	return K053251PalIndex[idx];
}

void K053251Scan(INT32 nAction)
{
	struct BurnArea ba;
	
	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = &K053251Ram;
		ba.nLen	  = sizeof(K053251Ram);
		ba.szName = "K053251 Ram";
		BurnAcb(&ba);

		SCAN_VAR(K053251PalIndex[0]);
		SCAN_VAR(K053251PalIndex[1]);
		SCAN_VAR(K053251PalIndex[2]);
		SCAN_VAR(K053251PalIndex[3]);
		SCAN_VAR(K053251PalIndex[4]);

		if (nAction & ACB_WRITE) {
			K053251ResetIndexes();
		}
	}
}
