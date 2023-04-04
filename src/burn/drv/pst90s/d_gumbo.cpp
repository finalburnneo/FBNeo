// FB Neo Gumbo driver module
// Based on MAME driver by David Haywood

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "msm6295.h"
#include "burn_pal.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvGfxROM[2];
static UINT8 *Drv68KRAM;
static UINT8 *DrvVidRAM[2];

static UINT8 DrvRecalc;

static UINT8 DrvJoy1[16];
static UINT8 DrvDips[1];
static UINT8 DrvReset;
static UINT16 DrvInputs[2];

static struct BurnInputInfo GumboInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 2"	},

	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 12,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 13,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 14,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 15,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 10,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Gumbo)

static struct BurnInputInfo DblpointInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 3"	},

	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 12,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 13,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 14,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 15,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 9,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 10,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Dblpoint)

static struct BurnDIPInfo GumboDIPList[]=
{
	{0x0e, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x0e, 0x01, 0x03, 0x00, "3 Coins 1 Credits"	},
	{0x0e, 0x01, 0x03, 0x01, "2 Coins 1 Credits"	},
	{0x0e, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x0e, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Helps"				},
	{0x0e, 0x01, 0x04, 0x00, "0"					},
	{0x0e, 0x01, 0x04, 0x04, "1"					},

	{0   , 0xfe, 0   ,    2, "Bonus Bar Level"		},
	{0x0e, 0x01, 0x08, 0x08, "Normal"				},
	{0x0e, 0x01, 0x08, 0x00, "High"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x0e, 0x01, 0x30, 0x20, "Easy"					},
	{0x0e, 0x01, 0x30, 0x30, "Normal"				},
	{0x0e, 0x01, 0x30, 0x10, "Hard"					},
	{0x0e, 0x01, 0x30, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    2, "Picture View"			},
	{0x0e, 0x01, 0x40, 0x40, "Off"					},
	{0x0e, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x0e, 0x01, 0x80, 0x80, "Off"					},
	{0x0e, 0x01, 0x80, 0x00, "On"					},
};

STDDIPINFO(Gumbo)

static struct BurnDIPInfo MsbingoDIPList[]=
{
	{0x0e, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x0e, 0x01, 0x03, 0x00, "3 Coins 1 Credits"	},
	{0x0e, 0x01, 0x03, 0x01, "2 Coins 1 Credits"	},
	{0x0e, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x0e, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Chance Count"			},
	{0x0e, 0x01, 0x0c, 0x0c, "0"					},
	{0x0e, 0x01, 0x0c, 0x08, "1"					},
	{0x0e, 0x01, 0x0c, 0x04, "2"					},
	{0x0e, 0x01, 0x0c, 0x00, "3"					},

	{0   , 0xfe, 0   ,    2, "Play Level"			},
	{0x0e, 0x01, 0x10, 0x10, "Normal"				},
	{0x0e, 0x01, 0x10, 0x00, "Easy"					},

	{0   , 0xfe, 0   ,    2, "Play Speed"			},
	{0x0e, 0x01, 0x20, 0x20, "Normal"				},
	{0x0e, 0x01, 0x20, 0x00, "High"					},

	{0   , 0xfe, 0   ,    2, "Left Count"			},
	{0x0e, 0x01, 0x40, 0x40, "Normal"				},
	{0x0e, 0x01, 0x40, 0x00, "Low"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x0e, 0x01, 0x80, 0x80, "Off"					},
	{0x0e, 0x01, 0x80, 0x00, "On"					},
};

STDDIPINFO(Msbingo)

static struct BurnDIPInfo MspuzzleDIPList[]=
{
	{0x0e, 0xff, 0xff, 0xfe, NULL					},

	{0   , 0xfe, 0   ,    4, "Time Mode"			},
	{0x0e, 0x01, 0x03, 0x03, "0"					},
	{0x0e, 0x01, 0x03, 0x02, "1"					},
	{0x0e, 0x01, 0x03, 0x01, "2"					},
	{0x0e, 0x01, 0x03, 0x00, "3"					},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x0e, 0x01, 0x0c, 0x00, "2 Coins 1 Credits"	},
	{0x0e, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"	},
	{0x0e, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"	},
	{0x0e, 0x01, 0x0c, 0x04, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    2, "Sound Test"			},
	{0x0e, 0x01, 0x10, 0x10, "Off"					},
	{0x0e, 0x01, 0x10, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "View Staff Credits"	},
	{0x0e, 0x01, 0x20, 0x20, "Off"					},
	{0x0e, 0x01, 0x20, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Picture View"			},
	{0x0e, 0x01, 0x40, 0x40, "Off"					},
	{0x0e, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x0e, 0x01, 0x80, 0x80, "Off"					},
	{0x0e, 0x01, 0x80, 0x00, "On"					},
};

STDDIPINFO(Mspuzzle)

static struct BurnDIPInfo DblpointDIPList[]=
{
	{0x10, 0xff, 0xff, 0xfb, NULL					},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x10, 0x01, 0x03, 0x00, "3 Coins 1 Credits"	},
	{0x10, 0x01, 0x03, 0x01, "2 Coins 1 Credits"	},
	{0x10, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x10, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x10, 0x01, 0x0c, 0x0c, "Easy"					},
	{0x10, 0x01, 0x0c, 0x08, "Normal"				},
	{0x10, 0x01, 0x0c, 0x04, "Hard"					},
	{0x10, 0x01, 0x0c, 0x00, "Very Hard"			},

	{0   , 0xfe, 0   ,    2, "Sound Test"			},
	{0x10, 0x01, 0x10, 0x10, "Off"					},
	{0x10, 0x01, 0x10, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Picture View"			},
	{0x10, 0x01, 0x20, 0x20, "Off"					},
	{0x10, 0x01, 0x20, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x10, 0x01, 0x80, 0x80, "Off"					},
	{0x10, 0x01, 0x80, 0x00, "On"					},
};

STDDIPINFO(Dblpoint)

static void __fastcall gumbo_write_word(UINT32 address, UINT16 data)
{
	switch (address & 0xf8ffff)
	{
		case 0x180300:
			MSM6295Write(0, data);
		return;
	}
}

static UINT16 __fastcall gumbo_read_word(UINT32 address)
{
	switch (address & 0xf8ffff)
	{
		case 0x180100:
			return DrvInputs[0];

		case 0x180200:
			return DrvInputs[1];

		case 0x180300:
			return MSM6295Read(0);
	}

	return 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekReset(0);

	MSM6295Reset(0);

	HiscoreReset();

	return 0;
}

static tilemap_callback( bg )
{
	INT32 code = BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvVidRAM[0] + offs * 2)));

	TILE_SET_INFO(0, code, 0, 0);
}

static tilemap_callback( fg )
{
	INT32 code = BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvVidRAM[1] + offs * 2)));

	TILE_SET_INFO(1, code, 0, 0);
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x080000;

	DrvGfxROM[0]	= Next; Next += 0x200000;
	DrvGfxROM[1]	= Next; Next += 0x080000;

	MSM6295ROM		= Next; Next += 0x040000;

	BurnPalette		= (UINT32*)Next; Next += 0x0200 * sizeof(UINT32);

	AllRam			= Next;

	Drv68KRAM		= Next; Next += 0x004000;
	BurnPalRAM		= Next; Next += 0x000400;
	DrvVidRAM[0]	= Next; Next += 0x002000;
	DrvVidRAM[1]	= Next; Next += 0x008000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvInit(INT32 game_select)
{
	BurnAllocMemIndex();

	INT32 dblpoin = (game_select == 1) ? 0x10000 : 0;

	INT32 k = 0;
	switch (game_select)
	{
		case 0: // gumbo & msbingo
		case 1: // dblpoin
		case 3: // msbingo
		{
			if (BurnLoadRom(Drv68KROM    + 0x000000, k++, 2)) return 1;
			if (BurnLoadRom(Drv68KROM    + 0x000001, k++, 2)) return 1;

			if (BurnLoadRom(MSM6295ROM   + 0x000000, k++, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM[1] + 0x000000, k++, 2)) return 1;
			if (BurnLoadRom(DrvGfxROM[1] + 0x000001, k++, 2)) return 1;

			if (BurnLoadRom(DrvGfxROM[0] + 0x000000, k++, 2)) return 1;
			if (BurnLoadRom(DrvGfxROM[0] + 0x000001, k++, 2)) return 1;
		}
		break;

		case 2: // mspuzzle
		{
			if (BurnLoadRom(Drv68KROM    + 0x000000, k++, 2)) return 1;
			if (BurnLoadRom(Drv68KROM    + 0x000001, k++, 2)) return 1;

			if (BurnLoadRom(MSM6295ROM   + 0x000000, k++, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM[1] + 0x000000, k++, 2)) return 1;
			if (BurnLoadRom(DrvGfxROM[1] + 0x000001, k++, 2)) return 1;

			if (BurnLoadRom(DrvGfxROM[0] + 0x000000, k++, 2)) return 1;
			if (BurnLoadRom(DrvGfxROM[0] + 0x100000, k++, 2)) return 1;
			if (BurnLoadRom(DrvGfxROM[0] + 0x000001, k++, 2)) return 1;
			if (BurnLoadRom(DrvGfxROM[0] + 0x100001, k++, 2)) return 1;
		}
		break;
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,		0x080000, 0x083fff, MAP_RAM);
	switch (game_select) {
		case 2: // mspuzzle
		case 3: // msbingo
			SekMapMemory(Drv68KRAM,		0x100000, 0x103fff, MAP_RAM); // mspuzzle
			SekMapMemory(DrvVidRAM[1],	0x190000, 0x197fff, MAP_RAM); // mspuzzle
			SekMapMemory(DrvVidRAM[0],	0x1c0000, 0x1c1fff, MAP_RAM); // mspuzzle
			SekMapMemory(BurnPalRAM,	0x1a0000, 0x1a03ff, MAP_RAM); // mspuzzle
			break;
		default: // everything else
			SekMapMemory(BurnPalRAM,	0x1b0000, 0x1b03ff, MAP_RAM);
			break;
	}
	SekMapMemory(DrvVidRAM[0],	0x1e0000 ^ dblpoin, 0x1e0fff ^ dblpoin, MAP_RAM);
	SekMapMemory(DrvVidRAM[1],	0x1f0000 ^ dblpoin, 0x1f3fff ^ dblpoin, MAP_RAM);
	SekSetWriteWordHandler(0,	gumbo_write_word);
	SekSetReadWordHandler(0,	gumbo_read_word);
	SekClose();

	MSM6295Init(0, 894886 / MSM6295_PIN7_HIGH, 0);
	MSM6295SetRoute(0, 0.47, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 64, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, fg_map_callback, 4, 4, 128, 64);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 8, 8, 8, (game_select == 2) ? 0x200000 : 0x100000, 0x000, 0);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 8, 4, 4, (game_select == 0) ? 0x040000 : 0x080000, 0x100, 0);
	GenericTilemapSetTransparent(1, 0xff);
	GenericTilemapSetOffsets(TMAP_GLOBAL, -64, -16);

	DrvDoReset();

	return 0;
}

static INT32 GumboInit()
{
	return DrvInit(0);
}

static INT32 DblpointInit()
{
	return DrvInit(1);
}

static INT32 MspuzzleInit()
{
	return DrvInit(2);
}

static INT32 MbingoInit()
{
	return DrvInit(3);
}

static INT32 DrvExit()
{
	GenericTilesExit();

	MSM6295Exit(0);
	SekExit();

	BurnFreeMemIndex();

	MSM6295ROM = NULL;

	return 0;
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		BurnPaletteUpdate_xRRRRRGGGGGBBBBB();
		DrvRecalc = 1; // force update
	}

	if ((nBurnLayer & 1) == 0) BurnTransferClear();

	if ((nBurnLayer & 1) != 0) GenericTilemapDraw(0, 0, 0);
	if ((nBurnLayer & 2) != 0) GenericTilemapDraw(1, 0, 0);

	BurnTransferCopy(BurnPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		DrvInputs[0] = 0xffff;
		DrvInputs[1] = (DrvDips[0] << 8) | 0x00ff;

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
		}
	}

	SekOpen(0);
	SekRun(7159090 / 60);
	SekSetIRQLine(1, CPU_IRQSTATUS_AUTO);
	SekClose();

	if (pBurnSoundOut) {
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin != NULL) {
		*pnMin = 0x029702;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);

		MSM6295Scan(nAction, pnMin);
	}

	return 0;
}


// Gumbo

static struct BurnRomInfo gumboRomDesc[] = {
	{ "u1.bin",		0x20000, 0xe09899e4, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "u2.bin",		0x20000, 0x60e59acb, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "u210.bin",	0x40000, 0x16fbe06b, 2 | BRF_SND },           //  2 Samples

	{ "u512.bin",	0x20000, 0x97741798, 3 | BRF_GRA },           //  3 Foreground Tiles
	{ "u511.bin",	0x20000, 0x1411451b, 3 | BRF_GRA },           //  4

	{ "u421.bin",	0x80000, 0x42445132, 4 | BRF_GRA },           //  5 Background Tiles
	{ "u420.bin",	0x80000, 0xde1f0e2f, 4 | BRF_GRA },           //  6
};

STD_ROM_PICK(gumbo)
STD_ROM_FN(gumbo)

struct BurnDriver BurnDrvGumbo = {
	"gumbo", NULL, NULL, NULL, "1994",
	"Gumbo\0", NULL, "Min Corp.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, gumboRomInfo, gumboRomName, NULL, NULL, NULL, NULL, GumboInputInfo, GumboDIPInfo,
	GumboInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	320, 224, 4, 3
};


// Miss Puzzle (Clone of Gumbo)

static struct BurnRomInfo mspuzzlgRomDesc[] = {
	{ "u1",			0x20000, 0x95218ff1, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "u2",			0x20000, 0x7ea7d96c, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "u210.bin",	0x40000, 0x16fbe06b, 2 | BRF_SND },           //  2 Samples

	{ "u512.bin",	0x20000, 0x97741798, 3 | BRF_GRA },           //  3 Foreground Tiles
	{ "u511.bin",	0x20000, 0x1411451b, 3 | BRF_GRA },           //  4

	{ "u420",		0x80000, 0x2b387153, 4 | BRF_GRA },           //  5 Background Tiles
	{ "u421",		0x80000, 0xafa06a93, 4 | BRF_GRA },           //  6
};

STD_ROM_PICK(mspuzzlg)
STD_ROM_FN(mspuzzlg)

struct BurnDriver BurnDrvMspuzzlg = {
	"mspuzzleg", "gumbo", NULL, NULL, "1994",
	"Miss Puzzle (Clone of Gumbo)\0", NULL, "Min Corp.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, mspuzzlgRomInfo, mspuzzlgRomName, NULL, NULL, NULL, NULL, GumboInputInfo, GumboDIPInfo,
	GumboInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	320, 224, 4, 3
};


// Miss Bingo

static struct BurnRomInfo msbingoRomDesc[] = {
	{ "u1.bin",		0x20000, 0x6eeb6d89, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "u2.bin",		0x20000, 0xf15dd4b5, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "u210.bin",	0x40000, 0x55011f69, 2 | BRF_SND },           //  2 Samples

	{ "u512.bin",	0x40000, 0x8a46d467, 3 | BRF_GRA },           //  3 Foreground Tiles
	{ "u511.bin",	0x40000, 0xd5fd3e2e, 3 | BRF_GRA },           //  4

	{ "u421.bin",	0x80000, 0xb73f21ab, 4 | BRF_GRA },           //  5 Background Tiles
	{ "u420.bin",	0x80000, 0xc2fe9175, 4 | BRF_GRA },           //  6
};

STD_ROM_PICK(msbingo)
STD_ROM_FN(msbingo)

struct BurnDriver BurnDrvMsbingo = {
	"msbingo", NULL, NULL, NULL, "1994",
	"Miss Bingo\0", NULL, "Min Corp.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, msbingoRomInfo, msbingoRomName, NULL, NULL, NULL, NULL, GumboInputInfo, MsbingoDIPInfo,
	MbingoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	320, 224, 4, 3
};


// Miss Puzzle

static struct BurnRomInfo mspuzzleRomDesc[] = {
	{ "u1.bin",		0x40000, 0xd9e63f12, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "u2.bin",		0x40000, 0x9c3fc677, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "u210.bin",	0x40000, 0x0a223a38, 2 | BRF_SND },           //  2 Samples

	{ "u512.bin",	0x40000, 0x505ee3c2, 3 | BRF_GRA },           //  3 Foreground Tiles
	{ "u511.bin",	0x40000, 0x3d6b6c78, 3 | BRF_GRA },           //  4

	{ "u421.bin",	0x80000, 0x5387ab3a, 4 | BRF_GRA },           //  5 Background Tiles
	{ "u425.bin",	0x80000, 0xf53a9042, 4 | BRF_GRA },           //  6
	{ "u420.bin",	0x80000, 0xc3f892e6, 4 | BRF_GRA },           //  7
	{ "u426.bin",	0x80000, 0xc927e8da, 4 | BRF_GRA },           //  8
};

STD_ROM_PICK(mspuzzle)
STD_ROM_FN(mspuzzle)

struct BurnDriver BurnDrvMspuzzle = {
	"mspuzzle", NULL, NULL, NULL, "1994",
	"Miss Puzzle\0", NULL, "Min Corp.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, mspuzzleRomInfo, mspuzzleRomName, NULL, NULL, NULL, NULL, GumboInputInfo, MspuzzleDIPInfo,
	MspuzzleInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 320, 3, 4
};


// Miss Puzzle (Nudes, more explicit))
/* sticker on PCB stated:  MISS PUZZLE  V ..8 */

static struct BurnRomInfo mspuzzleaRomDesc[] = {
	{ "u1.u1",		0x40000, 0x5e96ea17, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "u2.u2",		0x40000, 0x8f161b0c, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "u210.bin",	0x40000, 0x0a223a38, 2 | BRF_SND },  		  //  2 Samples

	{ "u512.bin",	0x40000, 0x505ee3c2, 3 | BRF_GRA },		      //  3 Foreground Tiles
	{ "u511.bin",	0x40000, 0x3d6b6c78, 3 | BRF_GRA },   		  //  4

	{ "u421.u421",	0x80000, 0x52b67ee5, 4 | BRF_GRA },     	  //  5 Background Tiles
	{ "u425.u425",	0x80000, 0x933544e3, 4 | BRF_GRA },           //  6
	{ "u420.u420",	0x80000, 0x3565696e, 4 | BRF_GRA }, 		  //  7
	{ "u426.u426",	0x80000, 0xe458eb9d, 4 | BRF_GRA },		      //  8
};

STD_ROM_PICK(mspuzzlea)
STD_ROM_FN(mspuzzlea)

struct BurnDriver BurnDrvMspuzzlea = {
	"mspuzzlea", "mspuzzle", NULL, NULL, "1994",
	"Miss Puzzle (Nudes, less explicit)\0", NULL, "Min Corp.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, mspuzzleaRomInfo, mspuzzleaRomName, NULL, NULL, NULL, NULL, GumboInputInfo, MspuzzleDIPInfo,
	MspuzzleInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 320, 3, 4
};


// Miss Puzzle (Nudes, more explicit))

static struct BurnRomInfo mspuzzlebRomDesc[] = {
	/* all the roms for this set needs a full redump, the PCB was in pretty bad condition and data reads were not consistent */
//	{ "u1.rom",		0x20000, 0xec940df4, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
//	{ "u2.rom",		0x20000, 0x7b9cac82, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "u1.u1",		0x40000, 0x5e96ea17, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "u2.u2",		0x40000, 0x8f161b0c, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "u210.rom",	0x40000, 0x8826b018, 2 | BRF_SND },  		  //  2 Samples

	{ "u512.bin",	0x40000, 0x505ee3c2, 3 | BRF_GRA },		      //  3 Foreground Tiles
	{ "u511.bin",	0x40000, 0x3d6b6c78, 3 | BRF_GRA },   		  //  4

	{ "u421.rom",	0x80000, 0x3c567c55, 4 | BRF_GRA },     	  //  5 Background Tiles
	/* 0x000000-0x3FFFF of u421.rom == 0x000000-0x3FFFF of u421 in mspuzzlea, all other data is corrupt to some degree */
	{ "u425.rom",	0x80000, 0x1c4c8fc1, 4 | BRF_GRA },           //  6
	{ "u420.rom",	0x80000, 0xf52ab7fd, 4 | BRF_GRA }, 		  //  7
	{ "u426.rom",	0x80000, 0xc28b2743, 4 | BRF_GRA },		      //  8
};

STD_ROM_PICK(mspuzzleb)
STD_ROM_FN(mspuzzleb)

struct BurnDriver BurnDrvMspuzzleb = {
	"mspuzzleb", "mspuzzle", NULL, NULL, "1994",
	"Miss Puzzle (Nudes, more explicit)\0", "imperfect graphics", "Min Corp.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, mspuzzlebRomInfo, mspuzzlebRomName, NULL, NULL, NULL, NULL, GumboInputInfo, MspuzzleDIPInfo,
	MspuzzleInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 320, 3, 4
};


// Double Point

static struct BurnRomInfo dblpointRomDesc[] = {
	{ "u1.bin",		0x20000, 0xb05c9e02, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "u2.bin",		0x20000, 0xcab35cbe, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "u210.rom",	0x40000, 0xd35f975c, 2 | BRF_SND },           //  2 Samples

	{ "u512.bin",	0x40000, 0xb57b8534, 3 | BRF_GRA },           //  3 Foreground Tiles
	{ "u511.bin",	0x40000, 0x74ed13ff, 3 | BRF_GRA },           //  4

	{ "u421.bin",	0x80000, 0xb0e9271f, 4 | BRF_GRA },           //  5 Background Tiles
	{ "u420.bin",	0x80000, 0x252789e8, 4 | BRF_GRA },           //  6
};

STD_ROM_PICK(dblpoint)
STD_ROM_FN(dblpoint)

struct BurnDriver BurnDrvDblpoint = {
	"dblpoint", NULL, NULL, NULL, "1995",
	"Double Point\0", NULL, "Min Corp.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, dblpointRomInfo, dblpointRomName, NULL, NULL, NULL, NULL, DblpointInputInfo, DblpointDIPInfo,
	DblpointInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	320, 224, 4, 3
};


// Double Point (Dong Bang Electron, bootleg?)

static struct BurnRomInfo dblpoindRomDesc[] = {
	{ "d12.bin",	0x20000, 0x44bc1bd9, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "d13.bin",	0x20000, 0x625a311b, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "d11.bin",	0x40000, 0xd35f975c, 2 | BRF_SND },           //  2 Samples

	{ "d14.bin",	0x40000, 0x41943db5, 3 | BRF_GRA },           //  3 Foreground Tiles
	{ "d15.bin",	0x40000, 0x6b899a51, 3 | BRF_GRA },           //  4

	{ "d16.bin",	0x80000, 0xafea0158, 4 | BRF_GRA },           //  5 Background Tiles
	{ "d17.bin",	0x80000, 0xc971dcb5, 4 | BRF_GRA },           //  6
};

STD_ROM_PICK(dblpoind)
STD_ROM_FN(dblpoind)

struct BurnDriver BurnDrvDblpoind = {
	"dblpointd", "dblpoint", NULL, NULL, "1995",
	"Double Point (Dong Bang Electron, bootleg?)\0", NULL, "Dong Bang Electron", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, dblpoindRomInfo, dblpoindRomName, NULL, NULL, NULL, NULL, DblpointInputInfo, DblpointDIPInfo,
	DblpointInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	320, 224, 4, 3
};
