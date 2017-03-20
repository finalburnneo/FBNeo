// Time pilot, pooyan, rallyx, tutankhm, and rocnrope sound 
// Based on MAME driver by Nicola Salmoria

#include "burnint.h"
#include "z80_intf.h"
#include "flt_rc.h"
#include "driver.h"
extern "C" {
#include "ay8910.h"
}

static UINT8 soundlatch;
static UINT8 *z80rom;
static UINT8 *z80ram;
static INT32 z80_select = 0;
static INT32 locomotnmode = 0;

static void filter_write(INT32 num, UINT8 d)
{
	INT32 C = 0;
	if (d & 1) C += 220000;	/* 220000pF = 0.220uF */
	if (d & 2) C +=  47000;	/*  47000pF = 0.047uF */
	filter_rc_set_RC(num, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_P(C));
}

static void __fastcall timeplt_sound_write(UINT16 address, UINT8 data)
{
	//bprintf(0, _T("tpsw %X %X\n"), address, data);
	if (!locomotnmode && address >= 0x8000 /*&& address <= 0xffff*/) {
		INT32 Offset = address & 0xfff;
		filter_write(3, (Offset >>  0) & 3);
		filter_write(4, (Offset >>  2) & 3);
		filter_write(5, (Offset >>  4) & 3);
		filter_write(0, (Offset >>  6) & 3);
		filter_write(1, (Offset >>  8) & 3);
		filter_write(2, (Offset >> 10) & 3);
		return;
	}

	if (locomotnmode && address >= 0x3000 && address <= 0x3fff) {
		INT32 Offset = address & 0xfff;
		filter_write(3, (Offset >>  0) & 3);
		filter_write(4, (Offset >>  2) & 3);
		filter_write(5, (Offset >>  4) & 3);
		filter_write(0, (Offset >>  6) & 3);
		filter_write(1, (Offset >>  8) & 3);
		filter_write(2, (Offset >> 10) & 3);
		return;
	}

	switch (address & 0xf000)
	{
		case 0x4000:
			AY8910Write(0, 1, data);
		return;

		case 0x5000:
			AY8910Write(0, 0, data);
		return;

		case 0x6000:
			AY8910Write(1, 1, data);
		return;

		case 0x7000:
			AY8910Write(1, 0, data);
		return;
	}
}

static UINT8 __fastcall timeplt_sound_read(UINT16 address)
{
	switch (address & 0xf000)
	{
		case 0x4000:
			return AY8910Read(0);

		case 0x6000:
			return AY8910Read(1);
	}

	return 0;
}

static UINT8 AY8910_0_portA(UINT32)
{
	ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
	return soundlatch;
}

static UINT8 AY8910_0_portB(UINT32)
{
	static const int timeplt_timer[10] = {
		0x00, 0x10, 0x20, 0x30, 0x40, 0x90, 0xa0, 0xb0, 0xa0, 0xd0
	};

	return timeplt_timer[(ZetTotalCycles() >> 9) % 10];
}

void TimepltSndSoundlatch(UINT8 data)
{
	soundlatch = data;
}

void TimepltSndReset()
{
	ZetOpen(z80_select);
	ZetReset();
	ZetClose();

	AY8910Reset(0);
	AY8910Reset(1);
	soundlatch = 0;
}

void TimepltSndInit(UINT8 *rom, UINT8 *ram, INT32 z80number)
{
	z80rom = rom;
	z80ram = ram;
	z80_select = z80number;

	ZetInit(z80_select);
	ZetOpen(z80_select);
	ZetMapMemory(z80rom,	0x0000, 0x2fff, MAP_ROM);
	ZetMapMemory(z80ram,	0x3000, 0x33ff, MAP_RAM);
	ZetMapMemory(z80ram,	0x3400, 0x37ff, MAP_RAM);
	ZetMapMemory(z80ram,	0x3800, 0x3bff, MAP_RAM);
	ZetMapMemory(z80ram,	0x3c00, 0x3fff, MAP_RAM);
	ZetSetWriteHandler(timeplt_sound_write);
	ZetSetReadHandler(timeplt_sound_read);
	ZetClose();

	AY8910Init(0, 1789772, nBurnSoundRate, &AY8910_0_portA, &AY8910_0_portB, NULL, NULL);
	AY8910Init(1, 1789772, nBurnSoundRate,NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.60, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.60, BURN_SND_ROUTE_BOTH);

	filter_rc_init(0, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_P(0), 0);
	filter_rc_init(1, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_P(0), 1);
	filter_rc_init(2, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_P(0), 1);
	filter_rc_init(3, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_P(0), 1);
	filter_rc_init(4, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_P(0), 1);
	filter_rc_init(5, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_P(0), 1);

	filter_rc_set_route(0, 1.00, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(1, 1.00, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(2, 1.00, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(3, 1.00, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(4, 1.00, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(5, 1.00, BURN_SND_ROUTE_BOTH);
	locomotnmode = 0;
}

void LocomotnSndInit(UINT8 *rom, UINT8 *ram, INT32 z80number)
{
	z80rom = rom;
	z80ram = ram;
	z80_select = z80number;

	ZetInit(z80_select);
	ZetOpen(z80_select);
	ZetMapMemory(z80rom,	0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(z80ram,	0x2000, 0x23ff, MAP_RAM);
	ZetMapMemory(z80ram,	0x2400, 0x27ff, MAP_RAM);
	ZetMapMemory(z80ram,	0x2800, 0x2bff, MAP_RAM);
	ZetMapMemory(z80ram,	0x2c00, 0x2fff, MAP_RAM);
	ZetSetWriteHandler(timeplt_sound_write);
	ZetSetReadHandler(timeplt_sound_read);
	ZetClose();

	AY8910Init(0, 1789772, nBurnSoundRate, &AY8910_0_portA, &AY8910_0_portB, NULL, NULL);
	AY8910Init(1, 1789772, nBurnSoundRate,NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.60, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.60, BURN_SND_ROUTE_BOTH);

	filter_rc_init(0, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_P(0), 0);
	filter_rc_init(1, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_P(0), 1);
	filter_rc_init(2, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_P(0), 1);
	filter_rc_init(3, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_P(0), 1);
	filter_rc_init(4, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_P(0), 1);
	filter_rc_init(5, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_P(0), 1);

	filter_rc_set_route(0, 1.00, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(1, 1.00, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(2, 1.00, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(3, 1.00, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(4, 1.00, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(5, 1.00, BURN_SND_ROUTE_BOTH);
	locomotnmode = 1;
}

void TimepltSndVol(double vol0, double vol1)
{
	filter_rc_set_route(0, vol0, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(1, vol0, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(2, vol0, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(3, vol1, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(4, vol1, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(5, vol1, BURN_SND_ROUTE_BOTH);
}

void TimepltSndExit()
{
	if (z80_select == 0) ZetExit();

	AY8910Exit(0);
	AY8910Exit(1);
	filter_rc_exit();

	z80_select = 0;
	z80rom = NULL;
	z80ram = NULL;
	locomotnmode = 0;
}

void TimepltSndUpdate(INT16 **pAY8910Buffer, INT16 *pSoundBuf, INT32 nSegmentLength)
{
	if (nSegmentLength <= 0) return;

	AY8910Update(0, &pAY8910Buffer[0], nSegmentLength);
	AY8910Update(1, &pAY8910Buffer[3], nSegmentLength);

	filter_rc_update(0, pAY8910Buffer[0], pSoundBuf, nSegmentLength);
	filter_rc_update(1, pAY8910Buffer[1], pSoundBuf, nSegmentLength);
	filter_rc_update(2, pAY8910Buffer[2], pSoundBuf, nSegmentLength);
	filter_rc_update(3, pAY8910Buffer[3], pSoundBuf, nSegmentLength);
	filter_rc_update(4, pAY8910Buffer[4], pSoundBuf, nSegmentLength);
	filter_rc_update(5, pAY8910Buffer[5], pSoundBuf, nSegmentLength);
}

INT32 TimepltSndScan(INT32 nAction, INT32 *pnMin)
{
	if (nAction & ACB_VOLATILE) {
		if (z80_select == 0) ZetScan(nAction);
		AY8910Scan(nAction, pnMin);

		SCAN_VAR(soundlatch);
	}

	return 0;
}
