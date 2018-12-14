// FB Alpha Tropical Angel driver module
// Based on MAME driver by Phil Stroffolino

#include "tiles_generic.h"
#include "z80_intf.h"
#include "irem_sound.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvM6803ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvScrRAM;
static UINT8 *DrvVidRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 flipscreen;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo TroangelInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 4,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 7,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Troangel)

static struct BurnDIPInfo TroangelDIPList[]=
{
	{0x0e, 0xff, 0xff, 0xff, NULL						},
	{0x0f, 0xff, 0xff, 0xfc, NULL						},

	{0   , 0xfe, 0   ,    4, "Time"						},
	{0x0e, 0x01, 0x03, 0x03, "B:180/A:160/M:140/BG:120"	},
	{0x0e, 0x01, 0x03, 0x02, "B:160/A:140/M:120/BG:100"	},
	{0x0e, 0x01, 0x03, 0x01, "B:140/A:120/M:100/BG:80"	},
	{0x0e, 0x01, 0x03, 0x00, "B:120/A:100/M:100/BG:80"	},

	{0   , 0xfe, 0   ,    2, "Crash Loss Time"			},
	{0x0e, 0x01, 0x04, 0x04, "5"						},
	{0x0e, 0x01, 0x04, 0x00, "10"						},

	{0   , 0xfe, 0   ,    2, "Background Sound"			},
	{0x0e, 0x01, 0x08, 0x08, "Boat Motor"				},
	{0x0e, 0x01, 0x08, 0x00, "Music"					},

	{0   , 0xfe, 0   ,    12, "Coinage"					},
	{0x0e, 0x01, 0xf0, 0xa0, "6 Coins 1 Credits"		},
	{0x0e, 0x01, 0xf0, 0xb0, "5 Coins 1 Credits"		},
	{0x0e, 0x01, 0xf0, 0xc0, "4 Coins 1 Credits"		},
	{0x0e, 0x01, 0xf0, 0xd0, "3 Coins 1 Credits"		},
	{0x0e, 0x01, 0xf0, 0xe0, "2 Coins 1 Credits"		},
	{0x0e, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x0e, 0x01, 0xf0, 0x70, "1 Coin  2 Credits"		},
	{0x0e, 0x01, 0xf0, 0x60, "1 Coin  3 Credits"		},
	{0x0e, 0x01, 0xf0, 0x50, "1 Coin  4 Credits"		},
	{0x0e, 0x01, 0xf0, 0x40, "1 Coin  5 Credits"		},
	{0x0e, 0x01, 0xf0, 0x30, "1 Coin  6 Credits"		},
	{0x0e, 0x01, 0xf0, 0x00, "Free Play"				},

	{0   , 0xfe, 0   ,    1, "Flip Screen"				},
	{0x0f, 0x01, 0x01, 0x00, "Off"						},
//	{0x0f, 0x01, 0x01, 0x01, "On"						},

	{0   , 0xfe, 0   ,    1, "Cabinet"					},
	{0x0f, 0x01, 0x02, 0x00, "Upright"					},
//	{0x0f, 0x01, 0x02, 0x02, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Coin Mode"				},
	{0x0f, 0x01, 0x04, 0x04, "Mode 1"					},
	{0x0f, 0x01, 0x04, 0x00, "Mode 2"					},

	{0   , 0xfe, 0   ,    2, "Analog Accelarator"		},
	{0x0f, 0x01, 0x08, 0x08, "No"						},
	{0x0f, 0x01, 0x08, 0x00, "Yes"						},

	{0   , 0xfe, 0   ,    2, "Stop Mode (Cheat)"		},
	{0x0f, 0x01, 0x10, 0x10, "Off"						},
	{0x0f, 0x01, 0x10, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Invulnerability (Cheat)"	},
	{0x0f, 0x01, 0x40, 0x40, "Off"						},
	{0x0f, 0x01, 0x40, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x0f, 0x01, 0x80, 0x80, "Off"						},
	{0x0f, 0x01, 0x80, 0x00, "On"						},
};

STDDIPINFO(Troangel)

static void __fastcall m57_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xd000:
			IremSoundWrite(data);
		return;

		case 0xd001:
			flipscreen = (data & 1) ^ (DrvDips[1] & 1);
		return;
	}
}

static UINT8 __fastcall m57_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xd000:
		case 0xd001:
		case 0xd002:
			return DrvInputs[address & 3];

		case 0xd003:
		case 0xd004:
			return DrvDips[(address & 1) ^ 1];
	}

	return 0;
}

static tilemap_callback( bg )
{
	INT32 attr = DrvVidRAM[offs * 2 + 0];
	INT32 code = DrvVidRAM[offs * 2 + 1] | ((attr & 0xc0) << 2);

	TILE_SET_INFO(0, code, attr, TILE_FLIPXY(attr >> 4));
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	IremSoundReset();

	flipscreen = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x008000;

	DrvM6803ROM		= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += 0x010000;
	DrvGfxROM1		= Next; Next += 0x020000;

	DrvColPROM		= Next; Next += 0x000210;

	DrvPalette		= (UINT32*)Next; Next += 0x200 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x000800;
	DrvVidRAM		= Next; Next += 0x000800;
	DrvScrRAM		= Next; Next += 0x000200;
	DrvSprRAM		= Next; Next += 0x000100;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode() // 0, 100
{
	INT32 Plane0[3] = { 0x2000*8*2, 0x2000*8*1, 0x2000*8*0 };
	INT32 Plane1[3] = { 0x4000*8*0, 0x4000*8*1, 0x4000*8*2 };
	INT32 XOffs[16] = { STEP8(0,1), STEP8(128,1) };
	INT32 YOffs[32] = { STEP16(0,8), STEP16(0x4000,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0xc000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x6000);

	GfxDecode(0x0400, 3,  8,  8, Plane0, XOffs, YOffs, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0xc000);

	GfxDecode(0x0040, 3, 16, 32, Plane1, XOffs, YOffs, 0x100, tmp + 0x0000, DrvGfxROM1 + 0x00000);
	GfxDecode(0x0040, 3, 16, 32, Plane1, XOffs, YOffs, 0x100, tmp + 0x1000, DrvGfxROM1 + 0x08000);
	GfxDecode(0x0040, 3, 16, 32, Plane1, XOffs, YOffs, 0x100, tmp + 0x2000, DrvGfxROM1 + 0x10000);
	GfxDecode(0x0040, 3, 16, 32, Plane1, XOffs, YOffs, 0x100, tmp + 0x3000, DrvGfxROM1 + 0x18000);

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
		if (BurnLoadRom(DrvZ80ROM   + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM   + 0x2000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM   + 0x4000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM   + 0x6000,  3, 1)) return 1;

		if (BurnLoadRom(DrvM6803ROM + 0x6000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0  + 0x0000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x2000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x4000,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1  + 0x0000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x2000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x4000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x6000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x8000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0xa000, 13, 1)) return 1;

		if (BurnLoadRom(DrvColPROM  + 0x0000, 14, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x0100, 15, 1)) return 1;

		for (INT32 i = 0; i < 0x100; i++) {
			DrvColPROM[i] = (DrvColPROM[i] & 0xf) | (DrvColPROM[0x100+i] << 4);
		}

		if (BurnLoadRom(DrvColPROM  + 0x0100, 16, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x0110, 17, 1)) return 1;
		
		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvVidRAM,		0x8000, 0x87ff, MAP_RAM);
	ZetMapMemory(DrvScrRAM,		0x9000, 0x91ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xc800, 0xc8ff, MAP_WRITE); // 0x20-ff
	ZetMapMemory(DrvZ80RAM,		0xe000, 0xe7ff, MAP_RAM);
	ZetSetWriteHandler(m57_main_write);
	ZetSetReadHandler(m57_main_read);
	ZetClose();

	IremSoundInit(DrvM6803ROM, 0, 3072000);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 3, 8, 8, 0x10000, 0, 0xf);
	GenericTilemapSetScrollRows(0, 256);
//	GenericTilemapSetOffsets(0, -8, -8); // apparently not hooked up in line scrolling

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	IremSoundExit();

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInitOne(UINT8 *prom, UINT32 *dst, INT32 len)
{
	for (INT32 i = 0; i < len; i++)
	{
		INT32 bit0 = 0;
		INT32 bit1 = (*prom >> 6) & 0x01;
		INT32 bit2 = (*prom >> 7) & 0x01;
		INT32 r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = (*prom >> 3) & 0x01;
		bit1 = (*prom >> 4) & 0x01;
		bit2 = (*prom >> 5) & 0x01;
		INT32 g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = (*prom >> 0) & 0x01;
		bit1 = (*prom >> 1) & 0x01;
		bit2 = (*prom >> 2) & 0x01;
		INT32 b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		dst[i] = BurnHighCol(r,g,b,0);
		prom++;
	}
}

static void DrvPaletteInit()
{
	UINT32 tmp[16];

	DrvPaletteInitOne(DrvColPROM + 0x000, DrvPalette, 0x100);
	DrvPaletteInitOne(DrvColPROM + 0x100, tmp, 0x10);

	for (INT32 i = 0; i < 256; i++) {
		DrvPalette[0x100 + i] = tmp[(DrvColPROM[0x110 + i] & 0xf) ^ 0xf];
	}
}

static void draw_background()
{
	for (INT32 y = 64; y < 128; y++) {
		GenericTilemapSetScrollRow(0, y, DrvScrRAM[0x40] + 8);
	}

	for (INT32 y = 128; y < nScreenHeight+8; y++) {
		INT16 scrollx = DrvScrRAM[y] + (DrvScrRAM[y + 0x100] * 256);
		GenericTilemapSetScrollRow(0, y, scrollx + 8);
	}
	
	GenericTilemapDraw(0, pTransDraw, 0);

	// the water should not wrap and should keep drawing the last pixel in the line
	// do this on both sides. GenericTilemap does not support this.
	for (INT32 y = 128; y < nScreenHeight; y++)
	{
		INT16 scrollx = DrvScrRAM[y] + (DrvScrRAM[y + 0x100] * 256) + 8;

		if (scrollx > 0)
		{
			INT32 sx = nScreenWidth - scrollx;

			if (sx > 0)
			{
				UINT16 *dst = pTransDraw + y * nScreenWidth;

				for (INT32 x = sx; x < nScreenWidth; x++) {
					dst[x] = dst[sx-1];
				}
			}
		}
		else if (scrollx < 0)
		{
			INT32 sx = (nScreenWidth - scrollx) - 256;

			if (sx > 0)
			{
				UINT16 *dst = pTransDraw + y * nScreenWidth;

				for (INT32 x = 0; x < sx; x++)
				{
					dst[x] = dst[sx];
				}
			}
		}
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0x100 - 4; offs >= 0x20; offs -= 4)
	{
		INT32 attr  = DrvSprRAM[offs + 1];
		INT32 sx    = DrvSprRAM[offs + 3];
		INT32 sy    = ((224 - DrvSprRAM[offs + 0] - 32) & 0xff) + 32;
		INT32 code  = DrvSprRAM[offs + 2];
		INT32 color = attr & 0x1f;
		INT32 flipy = attr & 0x80;
		INT32 flipx = attr & 0x40;

		code = (code & 0x3f) | ((code & 0x80) >> 1) | ((attr & 0x20) << 2);

		if (flipscreen)
		{
			sx = 240 - sx;
			sy = 224 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		DrawCustomMaskTile(pTransDraw, 16, 32, code, sx - 8, sy - 8, flipx, flipy, color, 3, 0, 0x100, DrvGfxROM1);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	GenericTilemapSetScrollY(0, 8); // hack
	draw_background();
	draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();
	M6803NewFrame();

	{
		DrvInputs[0] = 0xff;
		DrvInputs[1] = 0xff;
		DrvInputs[2] = 0xff;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = MSM5205CalcInterleave(0, 3072000);
	INT32 nCyclesTotal[2] = { 3072000 / 57, 894886 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	ZetOpen(0);
	M6803Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);

		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);

		nCyclesDone[1] = M6803Run(nCyclesTotal[1] / nInterleave);

		MSM5205Update();
		IremSoundClockSlave();
	}

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
		MSM5205Render(0, pBurnSoundOut, nBurnSoundLen);
		MSM5205Render(1, pBurnSoundOut, nBurnSoundLen);
	}

	M6803Close();
	ZetClose();

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
		IremSoundScan(nAction, pnMin);

		SCAN_VAR(flipscreen);
	}

	return 0;
}


// Tropical Angel

static struct BurnRomInfo troangelRomDesc[] = {
	{ "ta-a-3k",	0x2000, 0xf21f8196, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "ta-a-3m",	0x2000, 0x58801e55, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ta-a-3n",	0x2000, 0xde3dea44, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ta-a-3q",	0x2000, 0xfff0fc2a, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "ta-s-1a",	0x2000, 0x15a83210, 2 | BRF_PRG | BRF_ESS }, //  4 M6803 Code

	{ "ta-a-3e",	0x2000, 0xe49f7ad8, 3 | BRF_GRA },           //  5 Background Tiles
	{ "ta-a-3d",	0x2000, 0x06eef241, 3 | BRF_GRA },           //  6
	{ "ta-a-3c",	0x2000, 0x7ff5482f, 3 | BRF_GRA },           //  7

	{ "ta-b-5j",	0x2000, 0x86895c0c, 4 | BRF_GRA },           //  8 Sprites
	{ "ta-b-5h",	0x2000, 0xf8cff29d, 4 | BRF_GRA },           //  9
	{ "ta-b-5e",	0x2000, 0x8b21ee9a, 4 | BRF_GRA },           // 10
	{ "ta-b-5d",	0x2000, 0xcd473d47, 4 | BRF_GRA },           // 11
	{ "ta-b-5c",	0x2000, 0xc19134c9, 4 | BRF_GRA },           // 12
	{ "ta-b-5a",	0x2000, 0x0012792a, 4 | BRF_GRA },           // 13

	{ "ta-a-5a",	0x0100, 0x01de1167, 5 | BRF_GRA },           // 14 Color Data
	{ "ta-a-5b",	0x0100, 0xefd11d4b, 5 | BRF_GRA },           // 15
	{ "ta-b-1b",	0x0020, 0xf94911ea, 5 | BRF_GRA },           // 16
	{ "ta-b-3d",	0x0100, 0xed3e2aa4, 5 | BRF_GRA },           // 17
};

STD_ROM_PICK(troangel)
STD_ROM_FN(troangel)

struct BurnDriver BurnDrvTroangel = {
	"troangel", NULL, NULL, NULL, "1983",
	"Tropical Angel\0", NULL, "Irem", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_IREM_MISC, GBF_MISC, 0,
	NULL, troangelRomInfo, troangelRomName, NULL, NULL, NULL, NULL, TroangelInputInfo, TroangelDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	240, 240, 4, 3
};


// New Tropical Angel

static struct BurnRomInfo newtanglRomDesc[] = {
	{ "3k",			0x2000, 0x3c6299a8, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "3m",			0x2000, 0x8d09056c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3n",			0x2000, 0x17b5a775, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "3q",			0x2000, 0x2e5fa773, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "ta-s-1a-",	0x2000, 0xea8a05cb, 2 | BRF_PRG | BRF_ESS }, //  4 M6803 Code

	{ "ta-a-3e",	0x2000, 0xe49f7ad8, 3 | BRF_GRA },           //  5 Background Tiles
	{ "ta-a-3d",	0x2000, 0x06eef241, 3 | BRF_GRA },           //  6
	{ "ta-a-3c",	0x2000, 0x7ff5482f, 3 | BRF_GRA },           //  7

	{ "5j",			0x2000, 0x89409130, 4 | BRF_GRA },           //  8 Sprites
	{ "ta-b-5h",	0x2000, 0xf8cff29d, 4 | BRF_GRA },           //  9
	{ "5e",			0x2000, 0x5460a467, 4 | BRF_GRA },           // 10
	{ "ta-b-5d",	0x2000, 0xcd473d47, 4 | BRF_GRA },           // 11
	{ "5c",			0x2000, 0x4a20637a, 4 | BRF_GRA },           // 12
	{ "ta-b-5a",	0x2000, 0x0012792a, 4 | BRF_GRA },           // 13

	{ "ta-a-5a",	0x0100, 0x01de1167, 5 | BRF_GRA },           // 14 Color Data
	{ "ta-a-5b",	0x0100, 0xefd11d4b, 5 | BRF_GRA },           // 15
	{ "ta-b-1b",	0x0020, 0xf94911ea, 5 | BRF_GRA },           // 16
	{ "ta-b-3d",	0x0100, 0xed3e2aa4, 5 | BRF_GRA },           // 17
};

STD_ROM_PICK(newtangl)
STD_ROM_FN(newtangl)

struct BurnDriver BurnDrvNewtangl = {
	"newtangl", "troangel", NULL, NULL, "1983",
	"New Tropical Angel\0", NULL, "Irem", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_IREM_MISC, GBF_MISC, 0,
	NULL, newtanglRomInfo, newtanglRomName, NULL, NULL, NULL, NULL, TroangelInputInfo, TroangelDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	240, 240, 4, 3
};
