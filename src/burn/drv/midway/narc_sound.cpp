// Williams NARC sound emulation
// Based on MAME sources by Aaron Giles

#include "burnint.h"
#include "m6809_intf.h"
#include "burn_ym2151.h"
#include "dac.h"
#include "hc55516.h"

static UINT8 *rom[2];
static UINT8 *ram[2];

static INT32 latch[2];
static INT32 talkback;
static INT32 bankdata[2];
static INT32 sound_int_state;
static INT32 sound_in_reset;
static UINT8 audio_sync;

static INT32 if_clk;
static INT32 if_seq;

static void bankswitch(INT32 cpu, INT32 data)
{
	INT32 bank = 0x10000 + 0x8000 * (data & 1) + 0x10000 * ((data >> 3) & 1) + 0x20000 * ((data >> 1) & 3);
	bankdata[cpu] = data;

	M6809MapMemory(rom[cpu] + bank, 0x4000, 0xbfff, MAP_ROM);
}

UINT16 narc_sound_response_read()
{
	return talkback | (audio_sync << 8);
}

void narc_sound_write(UINT16 data)
{
	latch[0] = data & 0xff;

	M6809SetIRQLine(0, 0x20, (data & 0x100) ? CPU_IRQSTATUS_NONE : CPU_IRQSTATUS_ACK);

	if (~data & 0x200)
	{
		M6809SetIRQLine(0, 0, CPU_IRQSTATUS_ACK);
		sound_int_state = 1;
	}
}

void narc_sound_reset_write(INT32 state)
{
	if (state)
	{
		M6809Open(0);
		bankswitch(0, 0);
		M6809Reset();
		M6809Close();

		M6809Open(1);
		bankswitch(1, 0);
		M6809Reset();
		M6809Close();

		sound_in_reset = 1;
	}
	else
	{
		sound_in_reset = 0;
	}
}

static void sync_slave()
{
	INT32 cyc = M6809TotalCycles(0) - M6809TotalCycles(1);
	if (cyc > 0) {
		M6809Run(1, cyc);
	}
}

static void narc_sound0_write(UINT16 address, UINT8 data)
{
	if (address >= 0xcdff && address <= 0xce29) {
		rom[0][0x80000 + address] = data;
		return;
	}

	switch (address & ~0x3ff)
	{
		case 0x2000:
			BurnYM2151Write(address & 1, data);
		return;

		case 0x2800:
			talkback = data;
		return;

		case 0x2c00:
			latch[1] = data;
			sync_slave();
			M6809SetIRQLine(1, 1, CPU_IRQSTATUS_ACK);
		return;

		case 0x3000:
			DACSignedWrite(0, data);
		return;

		case 0x3800:
			bankswitch(0, data & 0xf);
		return;

		case 0x3c00:
			audio_sync |= 1;
			audio_sync &= ~1; // should be delayed
		return;
	}
}

static UINT8 narc_sound0_read(UINT16 address)
{
	switch (address & ~0x3ff)
	{
		case 0x2000:
			return BurnYM2151Read();

		case 0x3400:
			M6809SetIRQLine(0, CPU_IRQSTATUS_NONE);
			sound_int_state = 0;
			return latch[0];
	}

	return 0;
}

// hc55516: m6809 cpu1 sends constant stream of bytes to the chip while idle.
// this creates a very nasty high-pitched digital noise not unsimilar to
// that of burger time.  This bytefilter effectively removes the "idle stream" -dink jan26, 2021
static INT32 hc_idlefilter_check(UINT8 in)
{
	const UINT8 crap[8] = { 0x01, 0x00, 0x55, 0x2a, 0x15, 0x0a, 0x05, 0x02 };

	if (in == crap[if_clk]) {
		if_clk = (if_clk + 1) & 7;
		if_seq++;
	} else {
		if_seq = 0;
	}

	return (if_seq > 2);
}

static void narc_sound1_write(UINT16 address, UINT8 data)
{
	//bprintf(0, _T("s1 wb %x  %x\n"), address, data);
	switch (address & ~0x3ff)
	{
		case 0x2000:
			hc55516_clock_w(1);
		return;

		case 0x2400:
			if (!hc_idlefilter_check(data))
			{
				hc55516_clock_w(0);
				hc55516_digit_w(data & 1);
			}
		return;

		case 0x2800:
			// nop - talkback
		return;

		case 0x3000:
			DACSignedWrite(1, data);
		return;

		case 0x3800:
			bankswitch(1, data & 0xf);
		return;

		case 0x3c00:
			audio_sync |= 2;
			audio_sync &= ~2; // should be delayed
		return;
	}
}

static UINT8 narc_sound1_read(UINT16 address)
{
	switch (address & ~0x3ff)
	{
		case 0x3400:
			M6809SetIRQLine(1, CPU_IRQSTATUS_NONE);
			return latch[1];
	}

	return 0;
}

static void YM2151IrqHandler(INT32 state)
{
	M6809SetIRQLine(1, state ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

void narc_sound_reset()
{
	narc_sound_reset_write(1);
	narc_sound_reset_write(0);

	M6809Open(0);
	BurnYM2151Reset();
	DACReset();
	hc55516_reset();
	M6809Close();

	sound_int_state = 0;
	sound_in_reset = 0;

	if_clk = 0;
	if_seq = 0;
}

void narc_sound_init(UINT8 *rom0, UINT8 *rom1)
{
	rom[0] = rom0;
	rom[1] = rom1;
	ram[0] = (UINT8*)BurnMalloc(0x4000);
	ram[1] = ram[0] + 0x2000;

	M6809Init(0);
	M6809Open(0);
	M6809MapMemory(ram[0],				0x0000, 0x1fff, MAP_RAM);
	M6809MapMemory(rom[0] + 0x8c000,	0xc000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(narc_sound0_write);
	M6809SetReadHandler(narc_sound0_read);
	M6809Close();

	M6809Init(1);
	M6809Open(1);
	M6809MapMemory(ram[1],				0x0000, 0x1fff, MAP_RAM);
	M6809MapMemory(rom[1] + 0x8c000,	0xc000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(narc_sound1_write);
	M6809SetReadHandler(narc_sound1_read);
	M6809Close();

	BurnYM2151InitBuffered(3579545, 1, NULL, 0);
	BurnYM2151SetIrqHandler(&YM2151IrqHandler);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.20, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.20, BURN_SND_ROUTE_RIGHT);

	BurnTimerAttachM6809(2000000);

	hc55516_init(M6809TotalCycles, 2000000);
	hc55516_volume(0.60);

	DACInit(0, 0, 1, M6809TotalCycles, 2000000);
	DACInit(1, 0, 1, M6809TotalCycles, 2000000);
	DACSetRoute(0, 0.25, BURN_SND_ROUTE_BOTH);
	DACSetRoute(1, 0.25, BURN_SND_ROUTE_BOTH);
	DACDCBlock(1);
}

void narc_sound_exit()
{
	M6809Exit();
	BurnYM2151Exit();
	DACExit();
	hc55516_exit();

	BurnFree (ram[0]);
}

INT32 narc_sound_in_reset()
{
	return sound_in_reset;
}

void narc_sound_update(INT16 *stream, INT32 length)
{
	BurnYM2151Render(stream, length);
	hc55516_update(stream, length);
	DACUpdate(stream, length);
}

INT32 narc_sound_scan(INT32 nAction, INT32 *pnMin)
{
	if (nAction & ACB_MEMORY_RAM) {
		struct BurnArea ba;
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = ram[0];
		ba.nLen	  = 0x4000;
		ba.szName = "Sound Ram";
		BurnAcb(&ba);

		memset(&ba, 0, sizeof(ba));
		ba.Data	  = rom[0] + 0x8cdff;
		ba.nLen	  = 0x2b;
		ba.szName = "Prot Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		M6809Scan(nAction);

		BurnYM2151Scan(nAction, pnMin);
		hc55516_scan(nAction, pnMin);
		DACScan(nAction, pnMin);

		SCAN_VAR(latch);
		SCAN_VAR(talkback);
		SCAN_VAR(bankdata);
		SCAN_VAR(sound_int_state);
		SCAN_VAR(sound_in_reset);

		SCAN_VAR(if_clk);
		SCAN_VAR(if_seq);
	}

	if (nAction & ACB_WRITE)
	{
		M6809Open(0);
		bankswitch(0, bankdata[0]);
		M6809Close();

		M6809Open(1);
		bankswitch(1, bankdata[1]);
		M6809Close();
	}

	return 0;
}
