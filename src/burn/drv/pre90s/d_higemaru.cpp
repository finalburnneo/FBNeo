// FB Alpha Pirate Ship Higemaru driver module
// Based on MAME driver by Mirko Buffoni

#include "tiles_generic.h"
#include "z80_intf.h"
#include "driver.h"
#include "ay8910.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvColRAM;

static UINT32 *DrvPalette;
static UINT8  DrvRecalc;

static UINT8 flipscreen;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo HigemaruInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Higemaru)

static struct BurnDIPInfo HigemaruDIPList[]=
{
	{0x0f, 0xff, 0xff, 0xff, NULL				},
	{0x10, 0xff, 0xff, 0xfe, NULL				},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x0f, 0x01, 0x07, 0x01, "5 Coins 1 Credits"		},
	{0x0f, 0x01, 0x07, 0x02, "4 Coins 1 Credits"		},
	{0x0f, 0x01, 0x07, 0x03, "3 Coins 1 Credits"		},
	{0x0f, 0x01, 0x07, 0x04, "2 Coins 1 Credits"		},
	{0x0f, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x0f, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x0f, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x0f, 0x01, 0x07, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x0f, 0x01, 0x38, 0x08, "5 Coins 1 Credits"		},
	{0x0f, 0x01, 0x38, 0x10, "4 Coins 1 Credits"		},
	{0x0f, 0x01, 0x38, 0x18, "3 Coins 1 Credits"		},
	{0x0f, 0x01, 0x38, 0x20, "2 Coins 1 Credits"		},
	{0x0f, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x0f, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x0f, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x0f, 0x01, 0x38, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x0f, 0x01, 0xc0, 0x80, "1"				},
	{0x0f, 0x01, 0xc0, 0x40, "2"				},
	{0x0f, 0x01, 0xc0, 0xc0, "3"				},
	{0x0f, 0x01, 0xc0, 0x00, "5"				},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x10, 0x01, 0x01, 0x00, "Upright"			},
	{0x10, 0x01, 0x01, 0x01, "Cocktail"			},

	{0   , 0xfe, 0   ,    8, "Bonus Life"			},
	{0x10, 0x01, 0x0e, 0x0e, "10k 50k 50k+"			},
	{0x10, 0x01, 0x0e, 0x0c, "10k 60k 60k+"			},
	{0x10, 0x01, 0x0e, 0x0a, "20k 60k 60k+"			},
	{0x10, 0x01, 0x0e, 0x08, "20k 70k 70k+"			},
	{0x10, 0x01, 0x0e, 0x06, "30k 70k 70k+"			},
	{0x10, 0x01, 0x0e, 0x04, "30k 80k 80k+"			},
	{0x10, 0x01, 0x0e, 0x02, "40k 100k 100k+"		},
	{0x10, 0x01, 0x0e, 0x00, "None"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x10, 0x01, 0x10, 0x00, "Off"				},
	{0x10, 0x01, 0x10, 0x10, "On"				},

	{0   , 0xfe, 0   ,    2, "Demo Music"			},
	{0x10, 0x01, 0x20, 0x00, "Off"				},
	{0x10, 0x01, 0x20, 0x20, "On"				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x10, 0x01, 0x40, 0x40, "Off"				},
	{0x10, 0x01, 0x40, 0x00, "On"				},
};

STDDIPINFO(Higemaru)

static void __fastcall higemaru_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xc800:
			// coin counters = data & 3
			flipscreen = data & 0x80;
		return;

		case 0xc801:
			AY8910Write(0, 0, data);
		return;

		case 0xc802:
			AY8910Write(0, 1, data);
		return;

		case 0xc803:
			AY8910Write(1, 0, data);
		return;

		case 0xc804:
			AY8910Write(1, 1, data);
		return;
	}
}

static UINT8 __fastcall higemaru_read(UINT16 address)
{
	switch (address)
	{
		case 0xc000:
		case 0xc001:
		case 0xc002:
			return DrvInputs[address & 3];

		case 0xc003:
			return DrvDips[0];

		case 0xc004:
			return DrvDips[1];
	}

	return 0;
}

static tilemap_callback( bg )
{
	UINT8 attr = DrvColRAM[offs];

	TILE_SET_INFO(0, DrvVidRAM[offs] + ((attr & 0x80) << 1), attr, 0);
}

static INT32 DrvDoReset()
{
	memset(AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	AY8910Reset(0);
	AY8910Reset(1);

	HiscoreReset();

	flipscreen = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM	= Next; Next += 0x0080000;

	DrvGfxROM0	= Next; Next += 0x0080000;
	DrvGfxROM1	= Next; Next += 0x0080000;

	DrvColPROM	= Next; Next += 0x0002200;

	DrvPalette	= (UINT32*)Next; Next += 0x01800 * sizeof(UINT32);

	AllRam		= Next;

	DrvVidRAM	= Next; Next += 0x0004000;
	DrvColRAM	= Next; Next += 0x0004000;
	DrvSprRAM	= Next; Next += 0x0002000;
	DrvZ80RAM	= Next; Next += 0x0020000;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static void DrvGfxDecode()
{
	static INT32 Plane[4]  = { (0x2000*8)+4, (0x2000*8)+0, 4, 0 };
	static INT32 XOffs[16] = { STEP4(0,1), STEP4(8,1), STEP4(256,1), STEP4(256+8,1) };
	static INT32 YOffs[16] = { STEP16(0,16) };

	UINT8 *tmp = (UINT8 *)BurnMalloc(0x4000);
	if (tmp == NULL) {
		return;
	}

	memcpy (tmp, DrvGfxROM0, 0x2000);

	GfxDecode(0x0200, 2,  8,  8, Plane + 2, XOffs, YOffs, 0x080, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x4000);

	GfxDecode(0x0080, 4, 16, 16, Plane + 0, XOffs, YOffs, 0x200, tmp, DrvGfxROM1);

	BurnFree (tmp);
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
		if (BurnLoadRom(DrvZ80ROM  + 0x2000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x4000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x6000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x0000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x0000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x2000,  6, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x0000,  7, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0020,  8, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0120,  9, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvVidRAM,		0xd000, 0xd3ff, MAP_RAM);
	ZetMapMemory(DrvColRAM,		0xd400, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xd800, 0xd9ff, MAP_RAM); // 80-1ff
	ZetMapMemory(DrvZ80RAM,		0xe000, 0xffff, MAP_RAM);
	ZetSetWriteHandler(higemaru_write);
	ZetSetReadHandler(higemaru_read);
	ZetClose();

	AY8910Init(0, 1500000, 0);
	AY8910Init(1, 1500000, 1);
	AY8910SetAllRoutes(0, 0.25, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.25, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 2, 8, 8, 0x8000, 0x100, 0x1f);
	GenericTilemapSetOffsets(0, 0, -16);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	AY8910Exit(0);
	AY8910Exit(1);

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	UINT32 pal[0x20];

	for (INT32 i = 0; i < 0x20; i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 0x01;
		INT32 r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = (DrvColPROM[i] >> 3) & 0x01;
		bit1 = (DrvColPROM[i] >> 4) & 0x01;
		bit2 = (DrvColPROM[i] >> 5) & 0x01;
		INT32 g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = 0;
		bit1 = (DrvColPROM[i] >> 6) & 0x01;
		bit2 = (DrvColPROM[i] >> 7) & 0x01;
		INT32 b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		pal[i] = BurnHighCol(r,g,b,0);
	}

	for (INT32 i = 0; i < 0x100; i++) {
		DrvPalette[i] = pal[(DrvColPROM[0x120 + i] & 0xf) + 0x10];
	}

	for (INT32 i = 0; i < 0x80; i++) {
		DrvPalette[i+0x100] = pal[(DrvColPROM[0x20 + i] & 0xf)];
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0x200 - 16; offs >= 0x80; offs -= 16)
	{
		INT32 code  = DrvSprRAM[offs + 0] & 0x7f;
		INT32 color = DrvSprRAM[offs + 4] & 0x0f;
		INT32 sx    = DrvSprRAM[offs + 12];
		INT32 sy    = DrvSprRAM[offs + 8];
		INT32 flipx = DrvSprRAM[offs + 4] & 0x10;
		INT32 flipy = DrvSprRAM[offs + 4] & 0x20;

		if (flipscreen)
		{
			sx = 240 - sx;
			sy = 240 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		if (flipy) {
			if (flipx) {
				Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy - 16, color, 4, 0xf, 0, DrvGfxROM1);
			} else {
				Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy - 16, color, 4, 0xf, 0, DrvGfxROM1);
			}
		} else {
			if (flipx) {
				Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy - 16, color, 4, 0xf, 0, DrvGfxROM1);
			} else {
				Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy - 16, color, 4, 0xf, 0, DrvGfxROM1);
			}
		}

		if (flipy) {
			if (flipx) {
				Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx - 256, sy - 16, color, 4, 0xf, 0, DrvGfxROM1);
			} else {
				Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx - 256, sy - 16, color, 4, 0xf, 0, DrvGfxROM1);
			}
		} else {
			if (flipx) {
				Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx - 256, sy - 16, color, 4, 0xf, 0, DrvGfxROM1);
			} else {
				Render16x16Tile_Mask_Clip(pTransDraw, code, sx - 256, sy - 16, color, 4, 0xf, 0, DrvGfxROM1);
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	GenericTilemapSetFlip(0, flipscreen ? TMAP_FLIPXY : 0);

	GenericTilemapDraw(0, pTransDraw, 0);

	draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		DrvInputs[0] = DrvInputs[1] = 0x00; // active low, but ProcessJoy* needs active high to start
		DrvInputs[2] = 0xff;
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
		ProcessJoystick(&DrvInputs[0], 0, 3,2,1,0, INPUT_4WAY | INPUT_MAKEACTIVELOW);
		ProcessJoystick(&DrvInputs[1], 1, 3,2,1,0, INPUT_4WAY | INPUT_MAKEACTIVELOW);
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[1] = { 3000000 / 60 };
	INT32 nCyclesDone[1] = { 0 };

	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);

		if (i == 0) {
			ZetSetVector(0xd7);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
		if (i == 240) {
			ZetSetVector(0xcf);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
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

		SCAN_VAR(flipscreen);
	}

	return 0;
}


// Pirate Ship Higemaru

static struct BurnRomInfo higemaruRomDesc[] = {
	{ "hg4.p12",	0x2000, 0xdc67a7f9, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "hg5.m12",	0x2000, 0xf65a4b68, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "hg6.p11",	0x2000, 0x5f5296aa, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "hg7.m11",	0x2000, 0xdc5d455d, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "hg3.m1",	0x2000, 0xb37b88c8, 2 | BRF_GRA },           //  4 Characters

	{ "hg1.c14",	0x2000, 0xef4c2f5d, 3 | BRF_GRA },           //  5 Sprites
	{ "hg2.e14",	0x2000, 0x9133f804, 3 | BRF_GRA },           //  6

	{ "hgb3.l6",	0x0020, 0x629cebd8, 4 | BRF_GRA },           //  7 Color Data
	{ "hgb5.m4",	0x0100, 0xdbaa4443, 4 | BRF_GRA },           //  8
	{ "hgb1.h7",	0x0100, 0x07c607ce, 4 | BRF_GRA },           //  9

	{ "hgb4.l9",	0x0100, 0x712ac508, 0 | BRF_OPT },           // 10
	{ "hgb2.k7",	0x0100, 0x4921635c, 0 | BRF_OPT },           // 11
};

STD_ROM_PICK(higemaru)
STD_ROM_FN(higemaru)

struct BurnDriver BurnDrvHigemaru = {
	"higemaru", NULL, NULL, NULL, "1984",
	"Pirate Ship Higemaru\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_ACTION, 0,
	NULL, higemaruRomInfo, higemaruRomName, NULL, NULL, NULL, NULL, HigemaruInputInfo, HigemaruDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x180,
	256, 224, 4, 3
};
