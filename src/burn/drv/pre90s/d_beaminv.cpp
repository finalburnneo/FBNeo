// FB Alpha Beam Invader driver module
// Based on MAME driver by Zsolt Vasvari

#include "tiles_generic.h"
#include "z80_intf.h"

// to do:
//	hook up analog inputs
//  sound

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvVidRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 input_select;
static UINT8 *prev_snd_data;

static UINT8 DrvJoy1[8];
static UINT8 DrvDips[1];
static UINT8 DrvInputs[1];
static UINT8 DrvReset;

static struct BurnInputInfo BeaminvInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 1"	},
//	placeholder for analog input
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 2,	"p2 start"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 fire 1"	},
//	placeholder for analog input
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dips",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Beaminv)

static struct BurnDIPInfo BeaminvDIPList[]=
{
	{0x08, 0xff, 0xff, 0x04, NULL				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x08, 0x01, 0x03, 0x00, "3"				},
	{0x08, 0x01, 0x03, 0x01, "4"				},
	{0x08, 0x01, 0x03, 0x02, "5"				},
	{0x08, 0x01, 0x03, 0x03, "6"				},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x08, 0x01, 0x0c, 0x00, "1000"				},
	{0x08, 0x01, 0x0c, 0x04, "2000"				},
	{0x08, 0x01, 0x0c, 0x08, "3000"				},
	{0x08, 0x01, 0x0c, 0x0c, "4000"				},

	{0   , 0xfe, 0   ,    4, "Faster Bombs At"	},
	{0x08, 0x01, 0x60, 0x00, "49 Enemies"		},
	{0x08, 0x01, 0x60, 0x20, "39 Enemies"		},
	{0x08, 0x01, 0x60, 0x40, "29 Enemies"		},
	{0x08, 0x01, 0x60, 0x60, "Never"			},
};

STDDIPINFO(Beaminv)

static struct BurnDIPInfo PacominvDIPList[]=
{
	{0x08, 0xff, 0xff, 0x04, NULL				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x08, 0x01, 0x03, 0x00, "3"				},
	{0x08, 0x01, 0x03, 0x01, "4"				},
	{0x08, 0x01, 0x03, 0x02, "5"				},
	{0x08, 0x01, 0x03, 0x03, "6"				},

	{0   , 0xfe, 0   ,    5, "Bonus Life"		},
	{0x08, 0x01, 0x0c, 0x00, "1500"				},
	{0x08, 0x01, 0x0c, 0x04, "2000"				},
	{0x08, 0x01, 0x0c, 0x08, "2500"				},
	{0x08, 0x01, 0x0c, 0x0c, "3000"				},
	{0x08, 0x01, 0x0c, 0x0c, "4000"				},

	{0   , 0xfe, 0   ,    4, "Faster Bombs At"	},
	{0x08, 0x01, 0x60, 0x00, "44 Enemies"		},
	{0x08, 0x01, 0x60, 0x20, "39 Enemies"		},
	{0x08, 0x01, 0x60, 0x40, "34 Enemies"		},
	{0x08, 0x01, 0x60, 0x40, "29 Enemies"		},
};

STDDIPINFO(Pacominv)

static UINT8 __fastcall beaminv_read(UINT16 address)
{
	switch (address & ~0x03ff)
	{
		case 0x2400:
			return DrvDips[0];

		case 0x2800:
			return DrvInputs[0];

		case 0x3400:
			return 0; // analog inputs

		case 0x3800:
			return (ZetTotalCycles() >= 16666) ? 1 : 0;
	}

	return 0;
}

static void __fastcall beaminv_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			input_select = (data - 1) & 1;
		return;
	}
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	input_select = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x001800;

	DrvPalette		= (UINT32*)Next; Next += 0x0002 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x000800;
	DrvVidRAM		= Next; Next += 0x002000;

	prev_snd_data	= Next; Next += 0x000002;

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
		if (BurnLoadRom(DrvZ80ROM + 0x0000, 0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x0400, 1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x0800, 2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x0C00, 3, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x1000, 4, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x1400, 5, 1)) return 1;
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x17ff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,		0x1800, 0x1fff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,		0x4000, 0x5fff, MAP_RAM);
	ZetSetReadHandler(beaminv_read);
	ZetSetOutHandler(beaminv_write_port);
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

static void draw_bitmap()
{
	for (INT32 offs = 0; offs < 0x2000; offs++)
	{
		UINT8 sy = offs;
		UINT8 sx = (offs >> 8) << 3;

		if (sy < 16 || sy >= 231 || sx > 247) continue;

		UINT16 *dst = pTransDraw + ((sy - 16) * nScreenWidth) + sx;
		UINT8 pixeldata = DrvVidRAM[offs];

		for (INT32 i = 0; i < 8; i++)
		{
			dst[i] = (pixeldata >> i) & 1;
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPalette[0] = 0;
		DrvPalette[1] = ~0;
		DrvRecalc = 0;
	}

	draw_bitmap();

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
		DrvInputs[0] = 0xff;
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
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

		SCAN_VAR(input_select);
	}

	return 0;
}

// Beam Invader

static struct BurnRomInfo beaminvRomDesc[] = {
	{ "0a",		0x0400, 0x17503086, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "1a",		0x0400, 0xaa9e1666, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "2a",		0x0400, 0xebaa2fc8, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "3a",		0x0400, 0x4f62c2e6, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "4a",		0x0400, 0x3eebf757, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "5a",		0x0400, 0xec08bc1f, 1 | BRF_PRG | BRF_ESS }, //  5
};

STD_ROM_PICK(beaminv)
STD_ROM_FN(beaminv)

struct BurnDriverD BurnDrvBeaminv = {
	"beaminv", NULL, NULL, NULL, "1979",
	"Beam Invader\0", "No sound", "Teknon Kogyo", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, beaminvRomInfo, beaminvRomName, NULL, NULL, NULL, NULL, BeaminvInputInfo, BeaminvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 2,
	216, 248, 3, 4
};


// Pacom Invader

static struct BurnRomInfo pacominvRomDesc[] = {
	{ "rom_0",	0x0400, 0x67e100dd, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "rom_1",	0x0400, 0x442bbe98, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rom_2",	0x0400, 0x5d5d2f68, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rom_3",	0x0400, 0x527906b8, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "rom_4",	0x0400, 0x920bb3f0, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "rom_5",	0x0400, 0x3f6980e4, 1 | BRF_PRG | BRF_ESS }, //  5
};

STD_ROM_PICK(pacominv)
STD_ROM_FN(pacominv)

struct BurnDriverD BurnDrvPacominv = {
	"pacominv", "beaminv", NULL, NULL, "1979",
	"Pacom Invader\0", "No sound", "Pacom Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, pacominvRomInfo, pacominvRomName, NULL, NULL, NULL, NULL, BeaminvInputInfo, PacominvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 2,
	216, 248, 3, 4
};
