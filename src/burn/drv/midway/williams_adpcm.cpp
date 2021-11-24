// Williams ADPCM sound emulation
// Based on MAME sources by Aaron Giles

#include "burnint.h"
#include "m6809_intf.h"
#include "burn_ym2151.h"
#include "msm6295.h"
#include "dac.h"

static UINT8 *rom;
static UINT8 *ram;

static INT32 talkback = 0;
static INT32 sound_int_state = 0;
static INT32 latch = 0;
static INT32 oki_bank;
static INT32 rom_bank;
static INT32 sound_in_reset;
static INT32 protection_start;
static INT32 protection_end;

static void bankswitch(INT32 data)
{
	rom_bank = data & 0x07;

	M6809MapMemory(rom + 0x10000 + rom_bank * 0x8000, 0x4000, 0xbfff, MAP_ROM);
}

static void set_oki_bank(INT32 data)
{
	oki_bank = data & 0x7;
	INT32 banks[8] = { 0x40000, 0x40000, 0x20000, 0x00000, 0xe0000, 0xc0000, 0xa0000, 0x80000 };

	MSM6295SetBank(0, MSM6295ROM + banks[oki_bank], 0x00000, 0x1ffff);
	MSM6295SetBank(0, MSM6295ROM + 0x60000,         0x20000, 0x3ffff);
}

static void adpcm_write(UINT16 address, UINT8 data)
{
	if (address >= protection_start && address <= protection_end) {
		rom[0x40000 + address] = data;
		return;
	}

	switch (address & ~0x3ff)
	{
		case 0x2000:
			bankswitch(data);
		return;

		case 0x2400:
			BurnYM2151Write(address & 1, data);
		return;

		case 0x2800:
			DACWrite(0, data);
		return;

		case 0x2c00:
			MSM6295Write(0, data);
		return;

		case 0x3400:
			set_oki_bank(data);
		return;

		case 0x3c00:
			talkback = data;
		return;
	}
}

static UINT8 adpcm_read(UINT16 address)
{
	switch (address & ~0x3ff)
	{
		case 0x2400:
			return BurnYM2151Read();

		case 0x2c00:
			return MSM6295Read(0);

		case 0x3000:
			M6809SetIRQLine(0, CPU_IRQSTATUS_NONE);
			sound_int_state = 0;
			return latch;
	}

	return 0;
}

void williams_adpcm_sound_write(UINT16 data)
{
	latch = data & 0xff;
	if (~data & 0x200)
	{
		M6809SetIRQLine(0, CPU_IRQSTATUS_ACK);
		sound_int_state = 1;
	}
}

void williams_adpcm_reset_write(UINT16 state)
{
	if (state)
	{
		bankswitch(0);
		sound_int_state = 0;
		M6809Reset();
		sound_in_reset = 1;
	}
	else
		sound_in_reset = 0;
}

UINT16 williams_adpcm_sound_irq_read()
{
	return sound_int_state;
}

static void YM2151IrqHandler(INT32 state)
{
	M6809SetIRQLine(1, state ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

void williams_adpcm_reset()
{
	memset (ram, 0, 0x2000);

	M6809Reset(0);

	BurnYM2151Reset();
	MSM6295Reset(0);
	DACReset();

	sound_in_reset = 0;
	talkback = 0;
	sound_int_state = 0;
	latch = 0;
}

void williams_adpcm_init(UINT8 *prgrom, UINT8 *samples, INT32 prot_start, INT32 prot_end)
{
	rom = prgrom;
	ram = (UINT8*)BurnMalloc(0x2000);
	MSM6295ROM = samples;
	protection_start = prot_start;
	protection_end = prot_end;

	M6809Init(0);
	M6809Open(0);
	M6809MapMemory(ram,		0x0000, 0x1fff, MAP_RAM);
//	bank 4000-bfff
	M6809MapMemory(rom + 0x4c000,	0xc000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(adpcm_write);
	M6809SetReadHandler(adpcm_read);
	M6809Close();

	BurnYM2151InitBuffered(3579545, 1, NULL, 0);
	BurnYM2151SetIrqHandler(&YM2151IrqHandler);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.10, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.10, BURN_SND_ROUTE_RIGHT);

	BurnTimerAttachM6809(2000000);

	MSM6295Init(0, 1000000 / MSM6295_PIN7_HIGH, 1);
	MSM6295SetRoute(0, 0.47, BURN_SND_ROUTE_BOTH);

	DACInit(0, 0, 1, M6809TotalCycles, 2000000);
	DACSetRoute(0, 0.25, BURN_SND_ROUTE_BOTH);
}

void williams_adpcm_exit()
{
	BurnFree (ram);

	M6809Exit();
	DACExit();
	MSM6295Exit();
	BurnYM2151Exit();
}

INT32 williams_adpcm_sound_in_reset()
{
	return sound_in_reset;
}

void williams_adpcm_update(INT16 *output, INT32 length)
{
	BurnYM2151Render(output, length);
	MSM6295Render(0, output, length);
	DACUpdate(output, length);
}

INT32 williams_adpcm_scan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029698;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = ram;
		ba.nLen	  = 0x2000;
		ba.szName = "sound Ram";
		BurnAcb(&ba);

		memset(&ba, 0, sizeof(ba));
		ba.Data	  = rom + 0x40000 + protection_start;
		ba.nLen	  = (protection_end - protection_start) + 1;
		ba.szName = "sound Ram protection";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		M6809Scan(nAction);

		BurnYM2151Scan(nAction, pnMin);
		MSM6295Scan(nAction, pnMin);

		SCAN_VAR(talkback);
		SCAN_VAR(sound_int_state);
		SCAN_VAR(latch);
		SCAN_VAR(oki_bank);
		SCAN_VAR(rom_bank);
		SCAN_VAR(sound_in_reset);
	}

	if (nAction & ACB_WRITE) {
		M6809Open(0);
		bankswitch(rom_bank);
		M6809Close();

		set_oki_bank(oki_bank);
	}

	return 0;
}
