// FB Alpha Master Boy (original) driver module
// Based on MAME driver by David Haywood

#include "tiles_generic.h"
#include "z80_intf.h"
#include "ay8910.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvColPROM;
static UINT8 *DrvNVRAM;
static UINT8 *DrvVidRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 z80_bank;

static UINT8 DrvJoy1[8];
static UINT8 DrvDips[1];
static UINT8 DrvInputs[1];
static UINT8 DrvReset;

static struct BurnInputInfo MastboyoInputList[] = {
	{"P1 Coin",						BIT_DIGITAL,	DrvJoy1 + 1,	"p1 coin"	},
	{"P1 Start",					BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Green / >>",				BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 1"	},
	{"P1 Red / <<",					BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 2"	},

	{"P2 Coin",						BIT_DIGITAL,	DrvJoy1 + 0,	"p2 coin"	},
	{"P2 Start",					BIT_DIGITAL,	DrvJoy1 + 2,	"p2 start"	},
	{"P2 Green / Delete Initial",	BIT_DIGITAL,	DrvJoy1 + 5,	"p2 fire 1"	},
	{"P2 Red / Enter Initial",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 fire 2"	},

	{"Reset",						BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dips",						BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Mastboyo)

static struct BurnDIPInfo MastboyoDIPList[]=
{
	{0x09, 0xff, 0xff, 0xe1, NULL						},

	{0   , 0xfe, 0   ,    4, "Coin A"					},
	{0x09, 0x01, 0x03, 0x00, "2 Coins 1 Credits"		},
	{0x09, 0x01, 0x03, 0x01, "1 Coin  1 Credits"		},
	{0x09, 0x01, 0x03, 0x02, "1 Coin  2 Credits"		},
	{0x09, 0x01, 0x03, 0x03, "Disabled"					},

	{0   , 0xfe, 0   ,    4, "Coin B"					},
	{0x09, 0x01, 0x0c, 0x00, "1 Coin  1 Credits"		},
	{0x09, 0x01, 0x0c, 0x04, "1 Coin  2 Credits"		},
	{0x09, 0x01, 0x0c, 0x0c, "1 Coin  3 Credits"		},
	{0x09, 0x01, 0x0c, 0x08, "1 Coin  4 Credits"		},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x09, 0x01, 0x30, 0x00, "2"						},
	{0x09, 0x01, 0x30, 0x10, "3"						},
	{0x09, 0x01, 0x30, 0x20, "4"						},
	{0x09, 0x01, 0x30, 0x30, "6"						},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x09, 0x01, 0x80, 0x80, "Off"						},
	{0x09, 0x01, 0x80, 0x00, "On"						},
};

STDDIPINFO(Mastboyo)

static void bankswitch(INT32 data)
{
	z80_bank = data & 0xf;

	ZetMapMemory(DrvZ80ROM + 0x10000 + ((data & 0xf) * 0x8000),	0x8000, 0xffff, MAP_RAM);
}

static void __fastcall mastboyo_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x6000:
			bankswitch(data);
		return;
	}
}

static void __fastcall mastboyo_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
			AY8910Write(0, port & 1, data);
		return;
	}
}

static UINT8 __fastcall mastboyo_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
			return AY8910Read(0);
	}

	return 0;
}

static UINT8 AY8910_portA(UINT32)
{
	return DrvDips[0];
}

static UINT8 AY8910_portB(UINT32)
{
	return DrvInputs[0];
}

static tilemap_callback( bg )
{
	TILE_SET_INFO(0, DrvVidRAM[offs], DrvVidRAM[offs + 0x400], 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	bankswitch(1);
	ZetReset();
	ZetClose();

	AY8910Reset(0);

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x090000;

	DrvGfxROM		= Next; Next += 0x008000;

	DrvColPROM		= Next; Next += 0x000200;

	DrvPalette		= (UINT32*)Next; Next += 0x100 * sizeof(UINT32);

	DrvNVRAM		= Next; Next += 0x000800;

	AllRam			= Next;

	DrvVidRAM		= Next; Next += 0x000800;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[4] = { STEP4(0,1) };
	INT32 XOffs[8] = { 24, 28, 0, 4, 8, 12, 16, 20 };
	INT32 YOffs[8] = { STEP8(0,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x4000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM, 0x4000);

	GfxDecode(0x0200, 4, 8, 8, Plane, XOffs, YOffs, 0x100, tmp, DrvGfxROM);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit(INT32 which)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (which == 0) {
		memset (DrvZ80ROM, 0xff, 0x90000);

		if (BurnLoadRom(DrvZ80ROM  + 0x00000, 0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x50000, 1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x60000, 2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x70000, 3, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x80000, 4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM  + 0x00000, 5, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00100, 6, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00000, 7, 1)) return 1;

		for (INT32 i = 0; i < 0x100; i++) {
			DrvColPROM[i] = (DrvColPROM[i] & 0xf) | (DrvColPROM[0x100 + i] << 4);
		}

		DrvGfxDecode();
	} else {
		memset (DrvZ80ROM, 0xff, 0x90000);

		if (BurnLoadRom(DrvZ80ROM  + 0x00000, 0, 1)) return 1;
		memcpy(DrvZ80ROM + 0x0000, DrvZ80ROM + 0x4000, 0x4000);
		memset(DrvZ80ROM + 0x4000, 0, 0x4000);
		
		if (BurnLoadRom(DrvZ80ROM  + 0x50000, 1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x58000, 2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x60000, 3, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x70000, 4, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x78000, 5, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x80000, 6, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x88000, 7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM  + 0x00000, 8, 1)) return 1;
		memcpy(DrvGfxROM + 0x0000, DrvGfxROM + 0x4000, 0x4000);
		memset(DrvGfxROM + 0x4000, 0, 0x4000);

		if (BurnLoadRom(DrvColPROM + 0x00100, 9, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00000, 10, 1)) return 1;

		for (INT32 i = 0; i < 0x100; i++) {
			DrvColPROM[i] = (DrvColPROM[i] & 0xf) | (DrvColPROM[0x100 + i] << 4);
		}

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvNVRAM,			0x4000, 0x47ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,			0x5000, 0x57ff, MAP_RAM);
	ZetSetWriteHandler(mastboyo_write);
	ZetSetOutHandler(mastboyo_write_port);
	ZetSetInHandler(mastboyo_read_port);
	ZetClose();

	AY8910Init(0, 5000000, 0);
	AY8910SetPorts(0, &AY8910_portA, &AY8910_portB, NULL, NULL);
	AY8910SetAllRoutes(0, 0.30, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM, 4, 8, 8, 0x8000, 0, 0xf);
	GenericTilemapSetOffsets(0, 0, -16);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	ZetExit();
	AY8910Exit(0);

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x100; i++)
	{
		UINT8 r =(DrvColPROM[i] >> 3) & 7;
		UINT8 g = DrvColPROM[i] & 7;
		UINT8 b = DrvColPROM[i] >> 6;

		r = (r << 5) | (r << 2) | (r >> 1);
		g = (g << 5) | (g << 2) | (b >> 1);
		b = (b << 6) | (b << 4) | (b << 2) | b;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	GenericTilemapDraw(0, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		DrvInputs[0] = 0xf3;
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
		}
	}

	INT32 nInterleave = 4; // irq 4.2x per sec
	INT32 nCyclesTotal = 3333334 / 60;
	INT32 nCyclesDone = 0;

	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		INT32 nSegment = nCyclesTotal / nInterleave;

		nCyclesDone += ZetRun(nSegment);

		ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
	}

	ZetClose();

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
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
		AY8910Scan(nAction, pnMin);

		SCAN_VAR(z80_bank);
	}

	if (nAction & ACB_NVRAM) {
		ba.Data		= DrvNVRAM;
		ba.nLen		= 0x00800;
		ba.nAddress	= 0;
		ba.szName	= "NV RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		bankswitch(z80_bank);
		ZetClose();
	}

	return 0;
}


// Master Boy (1987, Z80 hardware, set 1)

static struct BurnRomInfo mastboyoRomDesc[] = {
	{ "mastboy_27256.ic14",			0x08000, 0xa212ff85, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "mastboy_27256.ic7",			0x08000, 0x3a214efd, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mastboy_27256.ic6",			0x08000, 0x4d682cfb, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "mastboy_27256.ic8",			0x08000, 0x40b07eeb, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "mastboy_27256.ic10",			0x08000, 0xb92ffd4f, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "mastboy_27256.ic9",			0x08000, 0x266e7d37, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "mastboy_27256.ic12",			0x08000, 0xefb4b2f9, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "mastboy_27256.ic11",			0x08000, 0xf2611186, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "mastboy_27256.ic36",			0x08000, 0xd862ca23, 2 | BRF_GRA },           //  8 Graphics

	{ "h_82s129.ic39",				0x00100, 0x8e965fc3, 3 | BRF_GRA },           //  9 Color data
	{ "l_82s129.ic40",				0x00100, 0x4d061216, 3 | BRF_GRA },           //  10

	{ "d_82s129.ic23",				0x00100, 0xd5fd2dfd, 0 | BRF_OPT },           //  11 Unused PROM
};

STD_ROM_PICK(mastboyo)
STD_ROM_FN(mastboyo)

static INT32 mastboyoInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvMastboyo = {
	"mastboyo", NULL, NULL, NULL, "1987",
	"Master Boy (1987, Z80 hardware, set 1)\0", NULL, "Gaelco (Covielsa license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_QUIZ, 0,
	NULL, mastboyoRomInfo, mastboyoRomName, NULL, NULL, NULL, NULL, MastboyoInputInfo, MastboyoDIPInfo,
	mastboyoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 224, 4, 3
};

// Master Boy (1987, Z80 hardware, set 2)

static struct BurnRomInfo mastboyoaRomDesc[] = {
	{ "masterboy-1987-27128-ic14.bin",			0x04000, 0xd05a22eb, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "masterboy-1987-27c512-ic10.bin",			0x10000, 0x66da2826, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "masterboy-1987-27256-ic11.bin",			0x08000, 0x40b07eeb, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "masterboy-1987-27c512-ic12.bin",			0x10000, 0xb2819e38, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "masterboy-1987-27c512-ic13.bin",			0x10000, 0x71df82e9, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "masterboy-1987-27128-mbfij-ic36.bin",	0x04000, 0xaa2a174d, 2 | BRF_GRA },           //  5 Graphics

	{ "masterboy-1987-82s129-h-ic39.bin",		0x00100, 0x8e965fc3, 3 | BRF_GRA },           //  6 Color data
	{ "masterboy-1987-82s129-l-ic40.bin",		0x00100, 0x4d061216, 3 | BRF_GRA },           //  7

	{ "masterboy-1987-82s129-d-ic23.bin",		0x00100, 0xd5fd2dfd, 0 | BRF_OPT },           //  8 Unused PROM
};

STD_ROM_PICK(mastboyoa)
STD_ROM_FN(mastboyoa)

static INT32 mastboyoaInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvMastboyoa = {
	"mastboyoa", "mastboyo", NULL, NULL, "1987",
	"Master Boy (1987, Z80 hardware, set 2)\0", NULL, "Gaelco (Covielsa license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_QUIZ, 0,
	NULL, mastboyoaRomInfo, mastboyoaRomName, NULL, NULL, NULL, NULL, MastboyoInputInfo, MastboyoDIPInfo,
	mastboyoaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 224, 4, 3
};
