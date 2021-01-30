
#include "burnint.h"
#include "z80_intf.h"
#include "msm6295.h"

static UINT8 *yawdim_ram;
static UINT8 *yawdim_rom;
static INT32 yawdim_oki_bank;
static UINT8 yawdim_soundlatch;

static INT32 is_yawdim2 = 0;

static void yawdim_set_oki_bank(UINT8 data)
{
	if (is_yawdim2)
	{
		yawdim_oki_bank = data;
		INT32 bank = ((data >> 1) & 4) + (data & 3);

		MSM6295SetBank(0, yawdim_rom + (bank * 0x40000), 0x00000, 0x3ffff);
	}
	else
	{
		if (data & 0x04) {
			yawdim_oki_bank = data & 7;
			MSM6295SetBank(0, yawdim_rom + (data & 3) * 0x40000, 0x00000, 0x3ffff);
		}
	}
}

static void __fastcall yawdim_sound_write(UINT16 address, UINT8 data)
{
	switch (address & ~0x7ff)
	{
		case 0x9000:
			if (is_yawdim2 && ~data & 4) MSM6295Reset(0);
			yawdim_set_oki_bank(data);
		return;

		case 0x9800:
			MSM6295Write(0, data);
		return;
	}
}

static UINT8 __fastcall yawdim_sound_read(UINT16 address)
{
	switch (address & ~0x7ff)
	{
		case 0x9800:
			return MSM6295Read(0);

		case 0xa000:
			return yawdim_soundlatch;
	}

	return 0;
}

void yawdim_sound_reset()
{
	memset (yawdim_ram, 0, 0x800);

	ZetReset(0);

	MSM6295Reset(0);

	yawdim_soundlatch = 0;
}

INT32 yawdim_sound_in_reset()
{
	return 0;
}

void yawdim_soundlatch_write(UINT16 data)
{
	yawdim_soundlatch = data;
	ZetNmi();
}

void yawdim_sound_init(UINT8 *prgrom, UINT8 *samples, INT32 yawdim2)
{
	is_yawdim2 = yawdim2;

	yawdim_ram = (UINT8*)BurnMalloc(0x800);
	yawdim_rom = samples;

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(prgrom,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(yawdim_ram,	0x8000, 0x87ff, MAP_RAM);
	ZetSetWriteHandler(yawdim_sound_write);
	ZetSetReadHandler(yawdim_sound_read);
	ZetClose();

	MSM6295Init(0, 1000000 / MSM6295_PIN7_HIGH, 0);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	if (is_yawdim2) {
		// try to avoid bad clipping in this one
		MSM6295SetRoute(0, 0.45, BURN_SND_ROUTE_BOTH);
	}
}

void yawdim_sound_exit()
{
	ZetExit();

	MSM6295Exit();

	BurnFree(yawdim_ram);
}

void yawdim_sound_update(INT16 *output, INT32 length)
{
	MSM6295Render(0, output, length);
}

INT32 yawdim_sound_scan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin != NULL) {
		*pnMin = 0x029698;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = yawdim_ram;
		ba.nLen	  = 0x800;
		ba.szName = "sound Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		ZetScan(nAction);

		MSM6295Scan(nAction, pnMin);

		SCAN_VAR(yawdim_soundlatch);
		SCAN_VAR(yawdim_oki_bank);
	}

	if (nAction & ACB_WRITE) {
		yawdim_set_oki_bank(yawdim_oki_bank);
	}

	return 0;
}
