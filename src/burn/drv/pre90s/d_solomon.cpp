// FInalBurn Neo Solomon's Key driver module
// Based on MAME driver by Mirko Buffoni

#include "tiles_generic.h"
#include "burn_pal.h"
#include "z80_intf.h"
#include "ay8910.h"
#include "watchdog.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM[2];
static UINT8 *DrvGfxROM[3];
static UINT8 *DrvZ80RAM[2];
static UINT8 *DrvColRAM[2];
static UINT8 *DrvVidRAM[2];
static UINT8 *DrvSprRAM;

static UINT8 flipscreen;
static UINT8 soundlatch;
static UINT8 nmi_mask;
static UINT8 protection_value;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo SolomonInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Solomon)

static struct BurnDIPInfo SolomonDIPList[]=
{
	{0x11, 0xff, 0xff, 0x02, NULL					},
	{0x12, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x11, 0x01, 0x01, 0x01, "Off"					},
	{0x11, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x11, 0x01, 0x02, 0x02, "Upright"				},
	{0x11, 0x01, 0x02, 0x00, "Cocktail"				},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x11, 0x01, 0x0c, 0x0c, "2"					},
	{0x11, 0x01, 0x0c, 0x00, "3"					},
	{0x11, 0x01, 0x0c, 0x08, "4"					},
	{0x11, 0x01, 0x0c, 0x04, "5"					},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x11, 0x01, 0x30, 0x20, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x30, 0x00, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x30, 0x10, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0x30, 0x30, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin A"				},
	{0x11, 0x01, 0xc0, 0x80, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0xc0, 0x00, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0xc0, 0x40, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0xc0, 0xc0, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x12, 0x01, 0x03, 0x02, "Easy"					},
	{0x12, 0x01, 0x03, 0x00, "Normal"				},
	{0x12, 0x01, 0x03, 0x01, "Harder"				},
	{0x12, 0x01, 0x03, 0x03, "Difficult"			},

	{0   , 0xfe, 0   ,    4, "Timer Speed"			},
	{0x12, 0x01, 0x0c, 0x08, "Slow"					},
	{0x12, 0x01, 0x0c, 0x00, "Normal"				},
	{0x12, 0x01, 0x0c, 0x04, "Faster"				},
	{0x12, 0x01, 0x0c, 0x0c, "Fastest"				},

	{0   , 0xfe, 0   ,    2, "Extra"				},
	{0x12, 0x01, 0x10, 0x00, "Normal"				},
	{0x12, 0x01, 0x10, 0x10, "Difficult"			},

	{0   , 0xfe, 0   ,    8, "Bonus Life"			},
	{0x12, 0x01, 0xe0, 0x00, "30k 200k 500k"		},
	{0x12, 0x01, 0xe0, 0x80, "100k 300k 800k"		},
	{0x12, 0x01, 0xe0, 0x40, "30k 200k"				},
	{0x12, 0x01, 0xe0, 0xc0, "100k 300k"			},
	{0x12, 0x01, 0xe0, 0x20, "30k"					},
	{0x12, 0x01, 0xe0, 0xa0, "100k"					},
	{0x12, 0x01, 0xe0, 0x60, "200k"					},
	{0x12, 0x01, 0xe0, 0xe0, "None"					},
};

STDDIPINFO(Solomon)

static void __fastcall solomon_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xe600:
			nmi_mask = data & 1;
		return;

		case 0xe604:
			flipscreen = data & 1;
		return;

		case 0xe800:
			soundlatch = data;
			ZetNmi(1);
		return;
		
		case 0xe802:
		return; // ?

		case 0xe803:
			protection_value = data & 0xfe;			// routine $161 just expects the lowest bit to be 0 (status?)
		return;
	}
}

static UINT8 __fastcall solomon_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xe600:
			return DrvInputs[0];

		case 0xe601:
			return DrvInputs[1];

		case 0xe602:
			return DrvInputs[2];

		case 0xe603:
			return protection_value;

		case 0xe604:
			return DrvDips[0];

		case 0xe605:
			return DrvDips[1];

		case 0xe606:
			return BurnWatchdogRead(); // ?
	}

	return 0;
}

static void __fastcall solomon_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xffff:
			BurnWatchdogWrite(); // ?
		return;

		default:
			bprintf (0, _T("solomon_sound_write(0x%4.4x, %2.2x);\n"), address, data);
		return;
	}
}

static UINT8 __fastcall solomon_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x8000:
			return soundlatch;
	}

	return 0;
}

static void __fastcall solomon_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x10:
		case 0x11:
		case 0x20:
		case 0x21:
		case 0x30:
		case 0x31:
			AY8910Write(((port >> 4) - 1) & 3, port & 1, data);
		return;
	}
}

static tilemap_callback( bg )
{
	INT32 attr  = DrvColRAM[1][offs];
	INT32 code  = DrvVidRAM[1][offs] + (attr << 8);
	INT32 color = (attr >> 4) & 7;
	INT32 flip  = ((attr & 0x80) ? TILE_FLIPX : 0) | ((attr & 0x08) ? TILE_FLIPY : 0);

	TILE_SET_INFO(1, code, color, flip);
}

static tilemap_callback( fg )
{
	INT32 attr  = DrvColRAM[0][offs];
	INT32 code  = DrvVidRAM[0][offs] + (attr << 8);
	INT32 color = (attr >> 4) & 7;

	TILE_SET_INFO(0, code, color, 0);
} 

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	ZetReset(0);
	ZetReset(1);

	AY8910Reset(0);
	AY8910Reset(1);
	AY8910Reset(2);

	BurnWatchdogReset();

	HiscoreReset();

	flipscreen = 0;
	soundlatch = 0;
	nmi_mask   = 0;
	protection_value = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM[0]		= Next; Next += 0x018000;
	DrvZ80ROM[1]		= Next; Next += 0x004000;

	DrvGfxROM[0]		= Next; Next += 0x020000;
	DrvGfxROM[1]		= Next; Next += 0x020000;
	DrvGfxROM[2]		= Next; Next += 0x020000;

	BurnPalette			= (UINT32*)Next; Next += 0x0100 * sizeof(UINT32);

	AllRam				= Next;

	DrvZ80RAM[0]		= Next; Next += 0x001000;
	DrvZ80RAM[1]		= Next; Next += 0x000800;

	DrvSprRAM			= Next; Next += 0x000100;
	BurnPalRAM			= Next; Next += 0x000200;

	DrvVidRAM[0]		= Next; Next += 0x000400;
	DrvVidRAM[1]		= Next; Next += 0x000400;
	DrvColRAM[0]		= Next; Next += 0x000400;
	DrvColRAM[1]		= Next; Next += 0x000400;

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[4]  = { STEP4(0,0x20000) };
	INT32 XOffs[16] = { STEP8(0,1), STEP8(64,1) };
	INT32 YOffs[16] = { STEP8(0,8), STEP8(128,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x10000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM[2], 0x10000);

	GfxDecode(0x0200, 4, 16, 16, Plane, XOffs, YOffs, 0x100, tmp, DrvGfxROM[2]);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x8000, k++, 1)) return 1;
		memcpy (DrvZ80ROM[0] + 0x4000, DrvZ80ROM[0] + 0xc000, 0x4000);
		if (BurnLoadRom(DrvZ80ROM[0] + 0xf000, k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM[1] + 0x0000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[0] + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x8000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[1] + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x8000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[2] + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x4000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x8000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0xc000, k++, 1)) return 1;

		BurnNibbleExpand(DrvGfxROM[0], NULL, 0x10000, 0, 0);
		BurnNibbleExpand(DrvGfxROM[1], NULL, 0x10000, 0, 0);
		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM[0],			0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM[0],			0xc000, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvColRAM[0],			0xd000, 0xd3ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM[0],			0xd400, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvColRAM[1],			0xd800, 0xdbff, MAP_RAM);
	ZetMapMemory(DrvVidRAM[1],			0xdc00, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,				0xe000, 0xe0ff, MAP_RAM); // 0-7f
	ZetMapMemory(BurnPalRAM,			0xe400, 0xe5ff, MAP_RAM);
	ZetMapMemory(DrvZ80ROM[0] + 0xf000,	0xf000, 0xffff, MAP_RAM);
	ZetSetWriteHandler(solomon_main_write);
	ZetSetReadHandler(solomon_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM[1],			0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM[1],			0x4000, 0x47ff, MAP_RAM);
	ZetSetWriteHandler(solomon_sound_write);
	ZetSetReadHandler(solomon_sound_read);
	ZetSetOutHandler(solomon_sound_write_port);
	ZetClose();

	BurnWatchdogInit(DrvDoReset, -1); // disable

	AY8910Init(0, 1500000, 0);
	AY8910Init(1, 1500000, 0);
	AY8910Init(2, 1500000, 0);
	AY8910SetAllRoutes(0, 0.12, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.12, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(2, 0.12, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, fg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 4,  8,  8, 0x20000, 0x000, 7);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 4,  8,  8, 0x20000, 0x080, 7);
	GenericTilemapSetGfx(2, DrvGfxROM[2], 4, 16, 16, 0x20000, 0x000, 7);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	AY8910Exit(0);
	AY8910Exit(1);
	AY8910Exit(2);
	BurnWatchdogExit();

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteUpdate()
{
	for (INT32 i = 0; i < 0x200; i+=2)
	{
		UINT8 r = BurnPalRAM[i+0] & 0xf;
		UINT8 g = BurnPalRAM[i+0] >> 4;
		UINT8 b = BurnPalRAM[i+1] & 0xf;
		
		BurnPalette[i/2] = BurnHighCol(pal4bit(r), pal4bit(g), pal4bit(b), 0);
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0x80 - 4; offs >= 0; offs -= 4)
	{
		INT32 sx    = DrvSprRAM[offs + 3];
		INT32 sy    = DrvSprRAM[offs + 2];
		INT32 attr  = DrvSprRAM[offs + 1];
		INT32 code  = DrvSprRAM[offs + 0] + ((attr & 0x10) << 4);
		INT32 color =(attr & 0x0e) >> 1;
		INT32 flipx = attr & 0x40;
		INT32 flipy = attr & 0x80;

		sy = 241 - sy;

		if (flipscreen)
		{
			sx = 240 - sx;
			sy = 242 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		DrawGfxMaskTile(0, 2, code, sx, sy - 16, flipx, flipy, color, 0);
	}
}

static INT32 DrvDraw()
{
	if (BurnRecalc) {
		DrvPaletteUpdate();
		BurnRecalc = 1;
	}

	GenericTilemapSetFlip(TMAP_GLOBAL, flipscreen ? (TMAP_FLIPX | TMAP_FLIPY) : 0);

	if (~nBurnLayer & 1) BurnTransferClear();
	if ( nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);
	if ( nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 0);
	if ( nSpriteEnable & 1) draw_sprites();

	BurnTransferCopy(BurnPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset(1);
	}

	{
		memset (DrvInputs, 0, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 16;
	INT32 nCyclesTotal[2] = { 4000000 / 60, 3072000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		CPU_RUN(0, Zet);
		if (i == (240 / 16) && nmi_mask) ZetNmi(); // vblank
		ZetClose();

		ZetOpen(1);
		CPU_RUN(1, Zet);
		if ((i % (nInterleave / 2)) == ((nInterleave / 2) - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD); // 2x per frame
		ZetClose();
	}

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
		BurnWatchdogScan(nAction);

		SCAN_VAR(flipscreen);
		SCAN_VAR(soundlatch);
		SCAN_VAR(nmi_mask);
		SCAN_VAR(protection_value);
	}

	return 0;
}


// Solomon's Key (US)

static struct BurnRomInfo solomonRomDesc[] = {
	{ "6.3f",			0x4000, 0x645eb0f3, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "7.3h",			0x8000, 0x1bf5c482, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "8.3jk",			0x8000, 0x0a6cdefc, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "1.3jk",			0x4000, 0xfa6e562e, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "12.3t",			0x8000, 0xb371291c, 3 | BRF_GRA },           //  4 Foreground Tiles
	{ "11.3r",			0x8000, 0x6f94d2af, 3 | BRF_GRA },           //  5

	{ "10.3p",			0x8000, 0x8310c2a1, 4 | BRF_GRA },           //  6 Background Tiles
	{ "9.3m",			0x8000, 0xab7e6c42, 4 | BRF_GRA },           //  7

	{ "2.5lm",			0x4000, 0x80fa2be3, 5 | BRF_GRA },           //  8 Sprites
	{ "3.6lm",			0x4000, 0x236106b4, 5 | BRF_GRA },           //  9
	{ "4.7lm",			0x4000, 0x088fe5d9, 5 | BRF_GRA },           // 10
	{ "5.8lm",			0x4000, 0x8366232a, 5 | BRF_GRA },           // 11
};

STD_ROM_PICK(solomon)
STD_ROM_FN(solomon)

struct BurnDriver BurnDrvSolomon = {
	"solomon", NULL, NULL, NULL, "1986",
	"Solomon's Key (US)\0", NULL, "Tecmo", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PUZZLE, 0,
	NULL, solomonRomInfo, solomonRomName, NULL, NULL, NULL, NULL, SolomonInputInfo, SolomonDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	256, 224, 4, 3
};


// Solomon no Kagi (Japan)

static struct BurnRomInfo solomonjRomDesc[] = {
	{ "slmn_06.bin",	0x4000, 0xe4d421ff, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "slmn_07.bin",	0x8000, 0xd52d7e38, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "slmn_08.bin",	0x1000, 0xb924d162, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "slmn_01.bin",	0x4000, 0xfa6e562e, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "slmn_12.bin",	0x8000, 0xaa26dfcb, 3 | BRF_GRA },           //  4 Foreground Tiles
	{ "slmn_11.bin",	0x8000, 0x6f94d2af, 3 | BRF_GRA },           //  5

	{ "slmn_10.bin",	0x8000, 0x8310c2a1, 4 | BRF_GRA },           //  6 Background Tiles
	{ "slmn_09.bin",	0x8000, 0xab7e6c42, 4 | BRF_GRA },           //  7

	{ "slmn_02.bin",	0x4000, 0x80fa2be3, 5 | BRF_GRA },           //  8 Sprites
	{ "slmn_03.bin",	0x4000, 0x236106b4, 5 | BRF_GRA },           //  9
	{ "slmn_04.bin",	0x4000, 0x088fe5d9, 5 | BRF_GRA },           // 10
	{ "slmn_05.bin",	0x4000, 0x8366232a, 5 | BRF_GRA },           // 11
};

STD_ROM_PICK(solomonj)
STD_ROM_FN(solomonj)

struct BurnDriver BurnDrvSolomonj = {
	"solomonj", "solomon", NULL, NULL, "1986",
	"Solomon no Kagi (Japan)\0", NULL, "Tecmo", "Miscellaneous",
	L"Solomon's Key (Japan)\0Solomon's Key \u30BD\u30ED\u30E2\u30F3\u306E\u9375\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PUZZLE, 0,
	NULL, solomonjRomInfo, solomonjRomName, NULL, NULL, NULL, NULL, SolomonInputInfo, SolomonDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	256, 224, 4, 3
};
