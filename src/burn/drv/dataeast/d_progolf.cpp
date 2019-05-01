// FB Alpha 18 Holes Pro Golf driver module
// Based on MAME driver by Angelo Salese and Roberto Zandona'

// TOFIX:
//   Little map is broken (on right side of playfield)

#include "tiles_generic.h"
#include "m6502_intf.h"
#include "ay8910.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvMainROM;
static UINT8 *DrvSoundROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvMainRAM;
static UINT8 *DrvCharRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSoundRAM;
static UINT8 *DrvFgBuffer;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 char_pen;
static UINT8 gfx_bank;
static UINT16 scrollx;
static UINT8 flipscreen;
static UINT8 soundlatch;

static INT32 vblank;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[4];
static UINT8 DrvReset;

static struct BurnInputInfo ProgolfInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy4 + 6,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Progolf)

static struct BurnDIPInfo ProgolfDIPList[]=
{
	{0x10, 0xff, 0xff, 0x10, NULL					},
	{0x11, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    5, "Coin B"				},
	{0x10, 0x01, 0x03, 0x03, "2 Coins 1 Credits"	},
	{0x10, 0x01, 0x03, 0x00, "1 Coin  1 Credits"	},
	{0x10, 0x01, 0x03, 0x01, "1 Coin  2 Credits"	},
	{0x10, 0x01, 0x03, 0x02, "1 Coin  3 Credits"	},
	{0x10, 0x01, 0x03, 0x02, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    5, "Coin A"				},
	{0x10, 0x01, 0x0c, 0x0c, "2 Coins 1 Credits"	},
	{0x10, 0x01, 0x0c, 0x00, "1 Coin  1 Credits"	},
	{0x10, 0x01, 0x0c, 0x04, "1 Coin  2 Credits"	},
	{0x10, 0x01, 0x0c, 0x08, "1 Coin  3 Credits"	},
	{0x10, 0x01, 0x0c, 0x08, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x10, 0x01, 0x10, 0x10, "Upright"				},
	{0x10, 0x01, 0x10, 0x00, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Lives"				},
	{0x11, 0x01, 0x01, 0x00, "1"					},
	{0x11, 0x01, 0x01, 0x01, "2"					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x11, 0x01, 0x06, 0x00, "10000"				},
	{0x11, 0x01, 0x06, 0x02, "30000"				},
	{0x11, 0x01, 0x06, 0x04, "50000"				},
	{0x11, 0x01, 0x06, 0x06, "None"					},

	{0   , 0xfe, 0   ,    2, "Difficulty"			},
	{0x11, 0x01, 0x08, 0x00, "Easy"					},
	{0x11, 0x01, 0x08, 0x08, "Hard"					},

	{0   , 0xfe, 0   ,    2, "Display Strength and Position"	},
	{0x11, 0x01, 0x10, 0x10, "No"					},
	{0x11, 0x01, 0x10, 0x00, "Yes"					},

	{0   , 0xfe, 0   ,    2, "Force Coinage = A 1C/3C - B 1C/8C"	},
	{0x11, 0x01, 0x20, 0x00, "No"					},
	{0x11, 0x01, 0x20, 0x20, "Yes"					},

	{0   , 0xfe, 0   ,    2, "Coin Mode"			},
	{0x11, 0x01, 0x40, 0x00, "Mode 1"				},
	{0x11, 0x01, 0x40, 0x40, "Mode 2"				},
};

STDDIPINFO(Progolf)

static void charram_write(INT32 offset, UINT8 data) // to do - simplify this so that it's written in as a bitmap not 8x8 tiles
{
	if (char_pen == 7)
	{
		for (INT32 i = 0; i < 8; i++)
			DrvFgBuffer[offset + i] = 0;
	}
	else
	{
		for(INT32 i = 0; i < 8; i++)
		{
			if (DrvFgBuffer[offset + i] == char_pen)
				DrvFgBuffer[offset + i] = data & (1<<(7-i)) ? char_pen : 0;
			else
				DrvFgBuffer[offset + i] = data & (1<<(7-i)) ? (DrvFgBuffer[offset + i] | char_pen) : DrvFgBuffer[offset + i];
		}
	}
}

static void bankswitch()
{
	M6502MapMemory(DrvVidRAM,		0x8000, 0x8fff, MAP_RAM);

	if ((gfx_bank & 8) || (gfx_bank & 3) == 0) return;

	INT32 offs0 = ((gfx_bank & 4) * 0x200);
	INT32 offs1 = (((gfx_bank - 1) & 3) * 0x1000) + offs0;

	M6502MapMemory(DrvGfxROM0 + offs1, 0x8000 + offs0, 0x87ff + offs0, MAP_ROM);
}

static void progolf_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xe000) == 0x6000) {
		DrvCharRAM[address & 0x1fff] = data;
		charram_write((address & 0x1fff) * 8, data);
		return;
	}

	switch (address)
	{
		case 0x9000:
			char_pen = data & 0x07;
			gfx_bank = data >> 4;
			bankswitch();
		return;

		case 0x9200:
			scrollx = (scrollx & 0x00ff) + (data * 256);
		return;

		case 0x9400:
			scrollx = (scrollx & 0xff00) + (data);
		return;

		case 0x9600:
			flipscreen = data & 0x01;
		return;

		case 0x9800:
			// mc6845 address_w
		return;

		case 0x9801:
			// mc6845 register_w
		return;

		case 0x9a00:
			soundlatch = data;
			M6502Close();
			M6502Open(1);
			M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
			M6502Close();
			M6502Open(0);
		return;

		case 0x9e00:
			// nop
		return;
	}
}

static UINT8 progolf_main_read(UINT16 address)
{
	switch (address)
	{
		case 0x9000:
			return DrvInputs[2] ^ 0xc0;

		case 0x9200:
			return DrvInputs[0];

		case 0x9400:
			return DrvInputs[1];

		case 0x9600:
			return vblank;

		case 0x9800:
			return (DrvDips[0] & 0x1f) | (DrvInputs[3] & 0xc0);

		case 0x9a00:
			return DrvDips[1];
	}

	return 0;
}

static void progolf_sound_write(UINT16 address, UINT8 data)
{
	switch (address >> 12)
	{
		case 0x4:
		case 0x5:
		case 0x6:
		case 0x7:
			AY8910Write((address >> 13) & 1, (~address >> 12) & 1, data);
		return;

		case 0x8:
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;
	}
}

static UINT8 progolf_sound_read(UINT16 address)
{
	switch (address >> 12)
	{
		case 0x4:
		case 0x6:
			return AY8910Read((address >> 13) & 1);

		case 0x8:
			return soundlatch;
	}

	return 0;
}

static tilemap_callback( bg )
{
	TILE_SET_INFO(0, DrvVidRAM[offs], 0, TILE_FLIPX);
}

static tilemap_callback( fg )
{
	TILE_SET_INFO(1, offs, 0, TILE_FLIPX);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	M6502Open(0);
	M6502Reset();
	M6502Close();

	M6502Open(1);
	M6502Reset();
	M6502Close();

	AY8910Reset(0);
	AY8910Reset(1);

	char_pen = 0;
	gfx_bank = 0;
	scrollx = 0;
	flipscreen = 0;
	soundlatch = 0;
	
	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvMainROM		= Next; Next += 0x005000;
	DrvSoundROM		= Next; Next += 0x001000;

	DrvGfxROM0		= Next; Next += 0x003000;
	DrvGfxROM1		= Next; Next += 0x004000;

	DrvColPROM		= Next; Next += 0x000020;

	DrvPalette		= (UINT32*)Next; Next += 0x0010 * sizeof(UINT32);

	AllRam			= Next;

	DrvMainRAM		= Next; Next += 0x006000;
	DrvCharRAM		= Next; Next += 0x002000;
	DrvVidRAM		= Next; Next += 0x002000;

	DrvSoundRAM		= Next; Next += 0x006000;

	DrvFgBuffer		= Next; Next += 0x010000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void DrvGfxDecode()
{
	INT32 Plane[3] = { 0x1000*2*8, 0x1000*1*8, 0x1000*0*8 };
	INT32 XOffs[8] = { STEP8(0,1) };
	INT32 YOffs[8] = { STEP8(0,8) };

	GfxDecode(0x0100, 3, 8, 8, Plane, XOffs, YOffs, 0x040, DrvGfxROM0, DrvGfxROM1);
}

static INT32 DrvInit(INT32 cputype)
{
	if (cputype == 1) return 1; // deco CPU6 not supported

	BurnSetRefreshRate(57.00);

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvMainROM + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x1000,  1, 1)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x2000,  2, 1)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x3000,  3, 1)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x4000,  4, 1)) return 1;

		if (BurnLoadRom(DrvSoundROM + 0x0000, 5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x0000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x1000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x2000,  8, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x0000,  9, 1)) return 1;
	//	if (BurnLoadRom(DrvColPROM + 0x0020, 10, 1)) return 1;
	//	if (BurnLoadRom(DrvColPROM + 0x0040, 11, 1)) return 1;

		DrvGfxDecode();
	}

	M6502Init(0, TYPE_DECO222);
	M6502Open(0);
	M6502MapMemory(DrvMainRAM,		0x0000, 0x5fff, MAP_RAM);
	M6502MapMemory(DrvCharRAM,		0x6000, 0x7fff, MAP_ROM);
	M6502MapMemory(DrvVidRAM,		0x8000, 0x8fff, MAP_WRITE);
	M6502MapMemory(DrvMainROM,		0xb000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(progolf_main_write);
	M6502SetReadHandler(progolf_main_read);
	M6502Close();

	M6502Init(1, TYPE_M6502);
	M6502Open(1);
	M6502MapMemory(DrvSoundRAM,		0x0000, 0x0fff, MAP_RAM);
	M6502MapMemory(DrvSoundROM,		0xf000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(progolf_sound_write);
	M6502SetReadHandler(progolf_sound_read);
	M6502Close();

	AY8910Init(0, 1500000, 0);
	AY8910Init(1, 1500000, 1);
	AY8910SetAllRoutes(0, 0.23, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.23, BURN_SND_ROUTE_BOTH);
    AY8910SetBuffered(M6502TotalCycles, 500000);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_COLS, bg_map_callback, 8, 8, 128, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_COLS, fg_map_callback, 8, 8,  32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM1,  3, 8, 8, 0x04000, 8, 0);
	GenericTilemapSetGfx(1, DrvFgBuffer, 3, 8, 8, 0x10000, 0, 0);
	GenericTilemapSetTransparent(1, 0);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6502Exit();

	AY8910Exit(0);
	AY8910Exit(1);

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x10; i++)
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

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	GenericTilemapSetFlip(TMAP_GLOBAL, TMAP_FLIPY);

	GenericTilemapSetScrollX(0, scrollx);

	GenericTilemapDraw(0, pTransDraw, 0);
	GenericTilemapDraw(1, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

    M6502NewFrame();

	{
		UINT8 prev_coin = (DrvInputs[3] | DrvInputs[2]) & 0xc0;

		memset (DrvInputs, 0, 4);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}

		if (prev_coin == 0 && ((DrvInputs[2] | DrvInputs[3]) & 0xc0) != 0) {
			M6502Open(0);
			M6502SetIRQLine(0x20, CPU_IRQSTATUS_AUTO);
			M6502Close();
		}
	}

	vblank = 0x00;

	INT32 nInterleave = 272/8;
	INT32 nCyclesTotal[2] = { 1500000 / 57, 500000 / 57 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		M6502Open(0);
		nCyclesDone[0] += M6502Run(((i + 1) * nCyclesTotal[0] / nInterleave) - nCyclesDone[0]);
		M6502Close();

		M6502Open(1);
		nCyclesDone[1] += M6502Run(((i + 1) * nCyclesTotal[1] / nInterleave) - nCyclesDone[1]);
		M6502Close();

		if (i == 248/8) vblank = 0x00;
		if (i == 8/8) vblank = 0x80;
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

		M6502Scan(nAction);
		AY8910Scan(nAction, pnMin);

		SCAN_VAR(soundlatch);
		SCAN_VAR(char_pen);
		SCAN_VAR(gfx_bank);
		SCAN_VAR(scrollx);
		SCAN_VAR(flipscreen);
		SCAN_VAR(soundlatch);
	}

	return 0;
}


// 18 Holes Pro Golf (set 1)

static struct BurnRomInfo progolfRomDesc[] = {
	{ "g4-m.2a",	0x1000, 0x8f06ebc0, 1 | BRF_PRG | BRF_ESS }, //  0 DECO222 Code (Encrypted M6502)
	{ "g3-m.4a",	0x1000, 0x8101b231, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "g2-m.6a",	0x1000, 0xa4a0d8dc, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "g1-m.8a",	0x1000, 0x749032eb, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "g0-m.9a",	0x1000, 0x8f8b1e8e, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "g6-m.1b",	0x1000, 0x0c6fadf5, 2 | BRF_PRG | BRF_ESS }, //  5 M6502 Code

	{ "g7-m.7a",	0x1000, 0x16b42975, 3 | BRF_GRA },           //  6 Graphics
	{ "g8-m.9a",	0x1000, 0xcf3f35da, 3 | BRF_GRA },           //  7
	{ "g9-m.10a",	0x1000, 0x7712e248, 3 | BRF_GRA },           //  8

	{ "gcm.a14",	0x0020, 0x8259e7db, 4 | BRF_GRA },           //  9 Color data
	{ "gbm.k4",		0x0020, 0x1ea3319f, 4 | BRF_GRA },           // 10
	{ "gam.k11",	0x0020, 0xb9665de3, 4 | BRF_GRA },           // 11
};

STD_ROM_PICK(progolf)
STD_ROM_FN(progolf)

static INT32 ProgolfInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvProgolf = {
	"progolf", NULL, NULL, NULL, "1981",
	"18 Holes Pro Golf (set 1)\0", NULL, "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, progolfRomInfo, progolfRomName, NULL, NULL, NULL, NULL, ProgolfInputInfo, ProgolfDIPInfo,
	ProgolfInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	256, 256, 3, 4
};


// 18 Holes Pro Golf (set 2)

static struct BurnRomInfo progolfaRomDesc[] = {
	{ "g4-m.a3",	0x1000, 0x015a08d9, 1 | BRF_PRG | BRF_ESS }, //  0 DECO CPU6 Code (Encrypted M6502)
	{ "g3-m.a4",	0x1000, 0xc1339da5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "g2-m.a6",	0x1000, 0xfafec36e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "g1-m.a8",	0x1000, 0x749032eb, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "g0-m.a9",	0x1000, 0xa03c533f, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "g5-m.b1",	0x1000, 0x0c6fadf5, 2 | BRF_PRG | BRF_ESS }, //  5 M6502 Code

	{ "g7-m.a8",	0x1000, 0x16b42975, 3 | BRF_GRA },           //  6 Graphics
	{ "g8-m.a9",	0x1000, 0xcf3f35da, 3 | BRF_GRA },           //  7
	{ "g9-m.a10",	0x1000, 0x7712e248, 3 | BRF_GRA },           //  8

	{ "gcm.a14",	0x0020, 0x8259e7db, 4 | BRF_GRA },           //  9 Color data
	{ "gbm.k4",		0x0020, 0x1ea3319f, 4 | BRF_GRA },           // 10
	{ "gam.k11",	0x0020, 0xb9665de3, 4 | BRF_GRA },           // 11
};

STD_ROM_PICK(progolfa)
STD_ROM_FN(progolfa)

static INT32 ProgolfaInit()
{
	return DrvInit(1);
}

struct BurnDriverD BurnDrvProgolfa = {
	"progolfa", "progolf", NULL, NULL, "1981",
	"18 Holes Pro Golf (set 2)\0", NULL, "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, progolfaRomInfo, progolfaRomName, NULL, NULL, NULL, NULL, ProgolfInputInfo, ProgolfDIPInfo,
	ProgolfaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	256, 256, 3, 4
};
