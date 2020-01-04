// license:BSD-3-Clause
// copyright-holders:Alex W. Jackson
// NMK112 - NMK custom IC for bankswitching the sample ROMs of a pair of OKI6295 ADPCM chips

#include "burnint.h"
#include "msm6295.h"

#define TABLESIZE   0x100
#define BANKSIZE    0x10000

static UINT8 page_mask;
static UINT8 current_bank[8];
static UINT32 romlen[2];
static UINT8 *romdata[2];

void NMK112_okibank_write(INT32 offset, UINT8 data)
{
	current_bank[offset] = data;

	INT32 chip = (offset & 4) / 4;
	INT32 size = romlen[chip];
	if (size == 0) return;

	INT32 banknum = offset & 3;
	INT32 paged = (page_mask & (1 << chip));

	UINT8 *rom = romdata[chip];

	INT32 bankaddr = (data * BANKSIZE) % size;

	if ((paged) && (banknum == 0))
	{
		MSM6295SetBank(chip, rom + bankaddr + 0x400, 0x00400, (BANKSIZE - 1));
	}
	else
	{
		MSM6295SetBank(chip, rom + bankaddr, 0x00000 + (banknum * BANKSIZE), (banknum * BANKSIZE) + (BANKSIZE - 1));
	}

	if (paged)
	{
		MSM6295SetBank(chip, rom + bankaddr + (banknum * TABLESIZE), (banknum * TABLESIZE), (banknum * TABLESIZE) + (TABLESIZE - 1));
	}
}

void NMK112Reset()
{
	for (INT32 i = 0; i < 8; i++) {
		NMK112_okibank_write(i, 0);
	}
}

void NMK112_init(UINT8 disable_page_mask, UINT8 *region0, UINT8 *region1, INT32 length0, INT32 length1)
{
	romdata[0] = region0;
	romdata[1] = region1;
	romlen[0]  = length0;
	romlen[1]  = length1;
	page_mask  = ~disable_page_mask;

	NMK112Reset();
}

INT32 NMK112_Scan(INT32 nAction)
{
	SCAN_VAR(current_bank);

	if (nAction & ACB_WRITE) {
		for (INT32 i = 0; i < 8; i++) {
			NMK112_okibank_write(i, current_bank[i]);
		}
	}

	return 0;
}
