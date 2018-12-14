// FB Alpha Dora-chan driver module
// Based on MAME driver by Tomasz Slanina

#include "tiles_generic.h"
#include "z80_intf.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvVidRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 flipscreen;
static UINT16 protection_value;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static struct BurnInputInfo DorachanInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 5,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 4,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 6,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 1"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 2,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
};

STDINPUTINFO(Dorachan)

static UINT8 protection_read()
{
	switch (protection_value)
	{
		case 0xfbf7:
			return 0xf2;

		case 0xf9f7:
			return 0xd5;

		case 0xf7f4:
			return 0xcb;

		default:
			bprintf (0, _T("Prot value: %8.8x\n"), protection_value);
			return 0;
	}

	return 0;
}

static UINT8 __fastcall dorachan_read(UINT16 address)
{
	switch (address & ~0x3ff)
	{
		case 0x2400:
			return protection_read();

		case 0x2800:
			return DrvInputs[0];

		case 0x2c00:
			return DrvInputs[1];

		case 0x3800:
			return 0xfe | ((ZetTotalCycles() / 16667) ^ flipscreen);
	}

	return 0;
}

static void __fastcall dorachan_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x02:
			if (data != 0xf3 && data != 0xe0) {
				protection_value <<= 8;
				protection_value |= data;
			}
		return;

		case 0x03:
			flipscreen = (data & 0x40) >> 6;
		return;
	}
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	flipscreen = 0;
	protection_value = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x008000;

	DrvColPROM		= Next; Next += 0x000400;

	DrvPalette		= (UINT32*)Next; Next += 0x0008 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x000800;
	DrvVidRAM		= Next; Next += 0x002000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM  + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x0400,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x0800,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x0c00,  3, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x1000,  4, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x1400,  5, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x2000,  6, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x6000,  7, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x6400,  8, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x6800,  9, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x6c00, 10, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x7000, 11, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x7400, 12, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x0000, 13, 1)) return 1;
	}

	ZetInit(0);	
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM + 0x0000,	0x0000, 0x17ff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,				0x1800, 0x1fff, MAP_RAM);
	ZetMapMemory(DrvZ80ROM + 0x2000,	0x2000, 0x23ff, MAP_ROM);
	ZetMapMemory(DrvVidRAM,				0x4000, 0x5fff, MAP_RAM);
	ZetMapMemory(DrvZ80ROM + 0x6000,	0x6000, 0x77ff, MAP_ROM);
	ZetSetReadHandler(dorachan_read);
	ZetSetOutHandler(dorachan_write_port);
	ZetClose();

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 8; i++) {
		DrvPalette[i] = BurnHighCol((i&1)?0xff:0, (i&2)?0xff:0, (i&4)?0xff:0, 0);
	}
}

static void draw_layer()
{
	INT32 shift = (flipscreen) ? 3 : 0;

	for (INT32 offs = 0; offs < (256 * 32); offs++)
	{
		INT32 x = (offs / 0x100) * 8;
		INT32 y = offs & 0xff;
		y -= 8;

		if (x < 0 || y < 0 || x >= nScreenWidth || y >= nScreenHeight) continue;

		UINT16 color_address = ((((offs << 2) & 0x03e0) | (offs >> 8)) + 1) & 0x03ff;

		UINT8 data = DrvVidRAM[offs];
		UINT8 fore_color = (DrvColPROM[color_address] >> shift) & 0x07;
		UINT16 *dst = pTransDraw + (y * nScreenWidth) + x;

		for (INT32 i = 0; i < 8; i++, data>>=1, x++)
		{
			dst[i] = (data & 0x01) ? fore_color : 0;
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	draw_layer();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 2);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 2;
	INT32 nCyclesTotal = 2000000 / 60;
	INT32 nCyclesDone = 0;

	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		INT32 nSegment = nCyclesTotal / nInterleave;

		nCyclesDone += ZetRun(nSegment);

		ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD); // 2x per frame
	}

	ZetClose();

	if (pBurnSoundOut) {

	}

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029702;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);

		SCAN_VAR(flipscreen);
		SCAN_VAR(protection_value);
	}

	return 0;
}


// Dora-chan (Japan)

static struct BurnRomInfo dorachanRomDesc[] = {
	{ "c1.e1",		0x0400, 0x29d66a96, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "d2.e2",		0x0400, 0x144b6cd1, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "d3.e3",		0x0400, 0xa9a1bed7, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "d4.e5",		0x0400, 0x099ddf4b, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "c5.e6",		0x0400, 0x49449dab, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "d6.e7",		0x0400, 0x5e409680, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "c7.e8",		0x0400, 0xb331a5ff, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "d8.rom",		0x0400, 0x5fe1e731, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "d9.rom",		0x0400, 0x338881a8, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "d10.rom",	0x0400, 0xf8c59517, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "d11.rom",	0x0400, 0xc2e0f066, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "d12.rom",	0x0400, 0x275e5dc1, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "d13.rom",	0x0400, 0x24ccfcf9, 1 | BRF_PRG | BRF_ESS }, // 12

	{ "d14.rom",	0x0400, 0xc0d3ee84, 2 | BRF_GRA },           // 13 Colors
};

STD_ROM_PICK(dorachan)
STD_ROM_FN(dorachan)

struct BurnDriver BurnDrvDorachan = {
	"dorachan", NULL, NULL, NULL, "1980",
	"Dora-chan (Japan)\0", "No sound", "Alpha Denshi Co. / Craul Denshi", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, dorachanRomInfo, dorachanRomName, NULL, NULL, NULL, NULL, DorachanInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 8,
	240, 256, 3, 4
};
