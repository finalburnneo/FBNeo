// FB Alpha Warp Speed driver module
// Based on MAME driver by Mariusz Wojcieszek

// to do:
//	hook up analog inputs

#include "tiles_generic.h"
#include "z80_intf.h"
#include "math.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvStarROM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvZ80RAM;
static UINT8 *vidregs;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 vblank;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[1];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo WarpspeedInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 7,	"p1 coin"	},
	{"P1 Accelerate",	BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 1"	},
	{"P1 Brake",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 2"	},
// analog placeholders
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 4"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dips",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Warpspeed)

static struct BurnDIPInfo WarpspeedDIPList[]=
{
	{0x06, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    8, "Coin/Time"			},
	{0x06, 0x01, 0x07, 0x00, "50 sec"				},
	{0x06, 0x01, 0x07, 0x01, "75 sec"				},
	{0x06, 0x01, 0x07, 0x02, "100 sec"				},
	{0x06, 0x01, 0x07, 0x03, "125 sec"				},
	{0x06, 0x01, 0x07, 0x04, "150 sec"				},
	{0x06, 0x01, 0x07, 0x05, "175 sec"				},
	{0x06, 0x01, 0x07, 0x06, "200 sec"				},
	{0x06, 0x01, 0x07, 0x07, "225 sec"				},

	{0   , 0xfe, 0   ,    2, "Coinage"				},
	{0x06, 0x01, 0x20, 0x20, "2 Coins 1 Credits"	},
	{0x06, 0x01, 0x20, 0x00, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x06, 0x01, 0x40, 0x00, "Off"					},
	{0x06, 0x01, 0x40, 0x40, "On"					},
};

STDDIPINFO(Warpspeed)

static void __fastcall warpspeed_write_port(UINT16 port, UINT8 data)
{
	if ((port & 0xff) <= 0x27) {
		vidregs[port & 0xff] = data;
		return;
	}
}

static UINT8 __fastcall warpspeed_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
			return ((DrvInputs[0] & 0xc0) ^ 0xc0) | 0; // analog x

		case 0x01:
			return (DrvInputs[1] & 0xc0) | 0; // analog y

		case 0x02:
			return DrvDips[0];

		case 0x03:
			return (DrvInputs[2] & 0xfe) | (vblank ? 0x01 : 0);
	}

	return 0;
}

static tilemap_callback( text )
{
	TILE_SET_INFO(0, DrvVidRAM[offs], 0, 0);
}

static tilemap_callback( star )
{
	UINT8 code = 0xff;

	if (offs & 1)
	{
		code = DrvStarROM[offs / 2];
	}

	TILE_SET_INFO(1, code, 0, 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();


	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x000e00;

	DrvStarROM		= Next; Next += 0x000200;

	DrvGfxROM0		= Next; Next += 0x001000;
	DrvGfxROM1		= Next; Next += 0x001000;

	DrvPalette		= (UINT32*)Next; Next += 0x000a * sizeof(UINT32);

	AllRam			= Next;

	DrvVidRAM		= Next; Next += 0x000400;
	DrvZ80RAM		= Next; Next += 0x000100;

	vidregs			= Next; Next += 0x000028;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[1] = { 0 };
	INT32 XOffs[8] = { STEP8(0,8) };
	INT32 YOffs[8] = { STEP8(7,-1) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x200);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x200);

	GfxDecode(0x0040, 1, 8, 8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x200);

	GfxDecode(0x0040, 1, 8, 8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM1);

	BurnFree(tmp);

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
		if (BurnLoadRom(DrvZ80ROM + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x0200,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x0400,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x0600,  3, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x0800,  4, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x0a00,  5, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x0c00,  6, 1)) return 1;

		if (BurnLoadRom(DrvStarROM + 0x0000, 7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x0000, 8, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x0000, 9, 1)) return 1;

		// sprites and unknown?

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x0dff, MAP_ROM);
	ZetMapMemory(DrvVidRAM,		0x1800, 0x1bff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM,		0x1c00, 0x1cff, MAP_RAM);
	ZetSetOutHandler(warpspeed_write_port);
	ZetSetInHandler(warpspeed_read_port);
	ZetClose();

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, text_map_callback, 8, 8, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, star_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 1, 8, 8, 0x01000, 0, 0);
	GenericTilemapSetGfx(1, DrvGfxROM1, 1, 8, 8, 0x01000, 0, 0);
	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetOffsets(TMAP_GLOBAL, -32, -64);

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
	DrvPalette[0] = 0;
	DrvPalette[1] = ~0;

	for (INT32 i = 0; i < 8; i++) {
		DrvPalette[i+2] = BurnHighCol((i&1)?0xff:0, (i&2)?0xff:0, (i&4)?0xff:0, 0);
	}
}

static void draw_circle_line(INT32 x, INT32 y, INT32 l, INT32 color)
{
	if (y >= 64 && y < 256) // 32, 256, 64, 256
	{
		UINT16 *pLine = pTransDraw + ((y - 64) * nScreenWidth) - 32;

		int h1 = x - l;
		int h2 = x + l;

		if (h1 <  32) h1 = 32;
		if (h2 > 255) h2 = 255;

		for (x = h1; x <= h2; x++) {

			pLine[x] = color;
		}
	}
}

static void draw_circle(INT16 cx, INT16 cy, UINT16 radius, UINT8 color)
{
	int x = 0;
	int y = radius;

	int d = 3 - 2 * radius;

	while (x <= y)
	{
		draw_circle_line(cx, cy - x, y, color);
		draw_circle_line(cx, cy + x, y, color);
		draw_circle_line(cx, cy - y, x, color);
		draw_circle_line(cx, cy + y, x, color);

		x++;

		if (d < 0)
			d += 4 * x + 6;
		else
			d += 4 * (x - y--) + 10;
	}
}

static void draw_circles()
{
	for (INT32 i = 0; i < 4; i++)
	{
		UINT16 radius = vidregs[i * 8] + vidregs[i * 8 + 1] * 256;
		radius = (UINT16)sqrt((float)(0xffff - radius));

		INT16 midx = (vidregs[i * 8 + 2] + vidregs[i * 8 + 3] * 256) - 0xe70;
		INT16 midy = (vidregs[i * 8 + 4] + vidregs[i * 8 + 5] * 256) - 0xe70;

		if (radius == 0 || radius == 0xffff)
		{
			continue;
		}

		draw_circle(midx + 128 + 16, midy + 128 + 16, radius, (vidregs[i*8 + 6] & 0x07) + 2);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	GenericTilemapDraw(1, pTransDraw, 0);

	draw_circles();

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
		memset (DrvInputs, 0, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256/8;
	INT32 nCyclesTotal[1] = { 2500000 / 60 };
	INT32 nCyclesDone[1] = { 0 };

	ZetOpen(0);

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);

		if (i == (240/8)) {
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
			vblank = 1;
		}
	}

	ZetClose();

	if (pBurnSoundOut) {

	}

	if (pBurnDraw) {
		BurnDrvRedraw();
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


	}

	return 0;
}


// Warp Speed (prototype)

static struct BurnRomInfo warpspedRomDesc[] = {
	{ "m16 pro 0.c5",			0x0200, 0x81f33dfb, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "m16 pro 1.e5",			0x0200, 0x135f7421, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "m16 pro 2.c6",			0x0200, 0x0a36d152, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "m16 pro 3.e6",			0x0200, 0xba416cca, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "m16 pro 4.c4",			0x0200, 0xfc44f25b, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "m16 pro 5.e4",			0x0200, 0x7a16bc2b, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "m16 pro 6.c3",			0x0200, 0xe2e7940f, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "e10.e10",				0x0200, 0xe0d4b72c, 2 | BRF_GRA },           //  7 Starfield Layout

	{ "k1.g1",					0x0200, 0x63d4fa84, 3 | BRF_GRA },           //  8 Text Tiles

	{ "k1.k1",					0x0200, 0x76b10d47, 4 | BRF_GRA },           //  9 Star Tiles

	{ "l9 l13 l17 g17.g17",		0x0200, 0x7449aae9, 5 | BRF_GRA },           // 10 Sprite (not used?)
	{ "l10 l15 l18 g18.g18",	0x0200, 0x5829699c, 5 | BRF_GRA },           // 11
	{ "l9 l13 l17 g17.l9",		0x0200, 0x7449aae9, 5 | BRF_GRA },           // 12
	{ "l10 l15 l18 g18.l10",	0x0200, 0x5829699c, 5 | BRF_GRA },           // 13
	{ "l9 l13 l17 g17.l13",		0x0200, 0x7449aae9, 5 | BRF_GRA },           // 14
	{ "l10 l15 l18 g18.l15",	0x0200, 0x5829699c, 5 | BRF_GRA },           // 15
	{ "l9 l13 l17 g17.l17",		0x0200, 0x7449aae9, 5 | BRF_GRA },           // 16
	{ "l10 l15 l18 g18.l18",	0x0200, 0x5829699c, 5 | BRF_GRA },           // 17

	{ "c12.c12",				0x0200, 0x88a8db15, 6 | BRF_GRA },           // 18 Unknown
	{ "e8.e8",					0x0200, 0x3ef3a576, 6 | BRF_GRA },           // 19
};

STD_ROM_PICK(warpsped)
STD_ROM_FN(warpsped)

struct BurnDriverD BurnDrvWarpsped = {
	"warpsped", NULL, NULL, NULL, "1979",
	"Warp Speed (prototype)\0", "No sound, bad colors", "Meadows Games, Inc.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, warpspedRomInfo, warpspedRomName, NULL, NULL, NULL, NULL, WarpspeedInputInfo, WarpspeedDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 10,
	224, 192, 4, 3
};
