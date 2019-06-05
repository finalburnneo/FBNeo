// FB Alpha Mosaic / Gold Fire II driver module
// Based on MAME driver by Nicola Salmoria

#include "tiles_generic.h"
#include "z180_intf.h"
#include "burn_ym2203.h"

#define CPU_SPEED	(7000000)

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ180ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvBgRAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvZ180RAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT16 prot_val;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[1];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static void (*protection_write)(UINT8 data);

static struct BurnInputInfo MosaicInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 coin"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dips",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Mosaic)

static struct BurnInputInfo Gfire2InputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dips",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Gfire2)

static struct BurnDIPInfo MosaicDIPList[]=
{
	{0x0f, 0xff, 0xff, 0xbc, NULL			},

	{0   , 0xfe, 0   ,    2, "Sound"		},
	{0x0f, 0x01, 0x01, 0x01, "Off"			},
	{0x0f, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Music"		},
	{0x0f, 0x01, 0x02, 0x02, "Off"			},
	{0x0f, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{0x0f, 0x01, 0x0c, 0x00, "3 Coins 1 Credits"	},
	{0x0f, 0x01, 0x0c, 0x04, "2 Coins 1 Credits"	},
	{0x0f, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"	},
	{0x0f, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x0f, 0x01, 0x10, 0x00, "Off"			},
	{0x0f, 0x01, 0x10, 0x10, "On"			},

	{0   , 0xfe, 0   ,    2, "Speed"		},
	{0x0f, 0x01, 0x20, 0x20, "Low"			},
	{0x0f, 0x01, 0x20, 0x00, "High"			},

	{0   , 0xfe, 0   ,    2, "Bombs"		},
	{0x0f, 0x01, 0x40, 0x00, "3"			},
	{0x0f, 0x01, 0x40, 0x40, "5"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x0e, 0x01, 0x80, 0x00, "Off"			},
	{0x0e, 0x01, 0x80, 0x80, "On"			},
};

STDDIPINFO(Mosaic)

static struct BurnDIPInfo Gfire2DIPList[]=
{
	{0x0e, 0xff, 0xff, 0x7f, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x0e, 0x01, 0x01, 0x00, "Off"			},
	{0x0e, 0x01, 0x01, 0x01, "On"			},

	{0   , 0xfe, 0   ,    2, "Bonus Time"		},
	{0x0e, 0x01, 0x02, 0x00, "*2 +30"		},
	{0x0e, 0x01, 0x02, 0x02, "*2 +50"		},

	{0   , 0xfe, 0   ,    3, "Difficulty"		},
	{0x0e, 0x01, 0x0c, 0x0c, "Easy"			},
	{0x0e, 0x01, 0x0c, 0x08, "Normal"		},
	{0x0e, 0x01, 0x0c, 0x04, "Hard"			},

	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{0x0e, 0x01, 0x60, 0x00, "3 Coins 1 Credits"	},
	{0x0e, 0x01, 0x60, 0x20, "2 Coins 1 Credits"	},
	{0x0e, 0x01, 0x60, 0x60, "1 Coin  1 Credits"	},
	{0x0e, 0x01, 0x60, 0x40, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Language"		},
	{0x0e, 0x01, 0x80, 0x00, "English"		},
	{0x0e, 0x01, 0x80, 0x80, "Korean"		},
};

STDDIPINFO(Gfire2)

static void mosaic_protection_write(UINT8 data)
{
	if ((data & 0x80) == 0x00)
	{
		prot_val = (data + 1) << 8;
	}
	else
	{
		static const UINT16 jumptable[] =
		{
			0x02be, 0x0314, 0x0475, 0x0662, 0x0694, 0x08f3, 0x0959, 0x096f,
			0x0992, 0x09a4, 0x0a50, 0x0d69, 0x0eee, 0x0f98, 0x1040, 0x1075,
			0x10d8, 0x18b4, 0x1a27, 0x1a4a, 0x1ac6, 0x1ad1, 0x1ae2, 0x1b68,
			0x1c95, 0x1fd5, 0x20fc, 0x212d, 0x213a, 0x21b6, 0x2268, 0x22f3,
			0x231a, 0x24bb, 0x286b, 0x295f, 0x2a7f, 0x2fc6, 0x3064, 0x309f,
			0x3118, 0x31e1, 0x32d0, 0x35f7, 0x3687, 0x38ea, 0x3b86, 0x3c9a,
			0x411f, 0x473f
		};

		prot_val = jumptable[data & 0x7f];
	}
}

static void gfire2_protection_write(UINT8 data)
{
	static const UINT16 jumptable[] =
	{
		0x0000 /*ignored*/, 0x100a, 0x150a, 0xe380, 0x6509, 0xb404 // verify endianness
	};

	if (jumptable[data >> 1]) prot_val = jumptable[data >> 1];
}

static void __fastcall mosaic_write_port(UINT32 port, UINT8 data)
{
	port &= 0xff;
	if (port < 0x40) return;

	switch (port)
	{
		case 0x70:
		case 0x71:
			BurnYM2203Write(0, port & 1, data);
		return;

		case 0x72:
			protection_write(data);
		return;
	}
}

static UINT8 __fastcall mosaic_read_port(UINT32 port)
{
	port &= 0xff;
	if (port < 0x40) return 0;

	switch (port)
	{
		case 0x70:
		case 0x71:
			return BurnYM2203Read(0, port & 1);

		case 0x72:
		{
			UINT16 res = prot_val >> 8;
			prot_val <<= 8;
			return res;
		}

		case 0x74:
			return DrvInputs[0];

		case 0x76:
			return DrvInputs[1];
	}

	return 0;
}

static tilemap_callback( fg )
{
	TILE_SET_INFO(0, DrvFgRAM[offs * 2] + (DrvFgRAM[offs * 2 + 1] * 256), 0, 0);
}

static tilemap_callback( bg )
{
	TILE_SET_INFO(1, DrvBgRAM[offs * 2] + (DrvBgRAM[offs * 2 + 1] * 256), 0, 0);
}

static UINT8 ym2203_0_read_portA(UINT32 )
{
	return DrvDips[0];
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	Z180Open(0);
	Z180Reset();
	BurnYM2203Reset();
	Z180Close();

	prot_val = 0;

	return 0;
}

static INT32 MemIndex(INT32 select)
{
	UINT8 *Next; Next = AllMem;

	DrvZ180ROM		= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += select ? 0x100000 : 0x40000;
	DrvGfxROM1		= Next; Next += select ? 0x080000 : 0x40000;

	DrvPalette       	= (UINT32*)Next; Next += 0x0100 * sizeof(UINT32);

	AllRam            	= Next;

	DrvBgRAM		= Next; Next += 0x001000;
	DrvFgRAM		= Next; Next += 0x001000;
	DrvPalRAM		= Next; Next += 0x000200;
	DrvZ180RAM		= Next; Next += 0x008000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvInit(INT32 select)
{
	AllMem = NULL;
	MemIndex(select);
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex(select);

	{
		if (BurnLoadRom(DrvZ180ROM + 0x000000,  0, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000003,  1, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000002,  2, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000001,  3, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  4, 4)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000003,  5, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x000002,  6, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x000001,  7, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  8, 4)) return 1;
	}

	Z180Init(0);
	Z180Open(0);
	Z180MapMemory(DrvZ180ROM, 		0x00000, 0x0ffff, MAP_ROM);
	Z180MapMemory(DrvZ180RAM, 		0x10000, 0x17fff, MAP_RAM); // gfire2
	Z180MapMemory(DrvZ180RAM, 		0x20000, 0x21fff, MAP_RAM); // mosaic
	Z180MapMemory(DrvBgRAM, 		0x22000, 0x22fff, MAP_RAM);
	Z180MapMemory(DrvFgRAM, 		0x23000, 0x23fff, MAP_RAM);
	Z180MapMemory(DrvPalRAM, 		0x24000, 0x241ff, MAP_RAM);
	Z180SetWritePortHandler(mosaic_write_port);
	Z180SetReadPortHandler(mosaic_read_port);
	Z180Close();

	protection_write = (select) ? gfire2_protection_write : mosaic_protection_write;

	BurnYM2203Init(1, 3000000, NULL, 0);
	BurnYM2203SetPorts(0, &ym2203_0_read_portA, NULL, NULL, NULL);
	BurnTimerAttachZ180(7000000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE, 0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.50, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, fg_map_callback, 8, 8, 64, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 8, 8, 8, (select) ? 0x100000 : 0x40000, 0, 0);
	GenericTilemapSetGfx(1, DrvGfxROM1, 8, 8, 8, (select) ? 0x080000 : 0x40000, 0, 0);
	GenericTilemapSetOffsets(TMAP_GLOBAL, -64, -16);
	GenericTilemapSetTransparent(0, 0xff);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	Z180Exit();

	BurnYM2203Exit();
	
	GenericTilesExit();
	
	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteUpdate()
{
	UINT16 *p = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x100; i++)
	{
		UINT8 r = (p[i] >> 10) & 0x1f;
		UINT8 g = (p[i] >>  5) & 0x1f;
		UINT8 b = (p[i] >>  0) & 0x1f;

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static INT32 DrvDraw()
{
	//if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	//}

	if ((nBurnLayer & 1) == 0) BurnTransferClear();

	if (nBurnLayer & 1) GenericTilemapDraw(1, pTransDraw, 0);
	if (nBurnLayer & 2) GenericTilemapDraw(0, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	Z180NewFrame();

	{
		memset (DrvInputs, 0xff, 2);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[1] = { 7000000 / 60 };

	Z180Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		BurnTimerUpdate((i+1) * (nCyclesTotal[0] / nInterleave));
		if (i == 239) Z180SetIRQLine(0, CPU_IRQSTATUS_ACK);
		if (i == 240) Z180SetIRQLine(0, CPU_IRQSTATUS_NONE);
	}

	BurnTimerEndFrame(nCyclesTotal[0]);

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
	}

	Z180Close();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029707;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		Z180Scan(nAction);

		BurnYM2203Scan(nAction, pnMin);

		SCAN_VAR(prot_val);
	}

	return 0;
}


// Mosaic

static struct BurnRomInfo mosaicRomDesc[] = {
	{ "9.ua02",			0x10000, 0x5794dd39, 1 | BRF_PRG | BRF_ESS }, //  0 Z180 Code

	{ "1.u505",			0x10000, 0x05f4cc70, 2 | BRF_GRA },           //  1 Foreground Tiles
	{ "2.u506",			0x10000, 0x78907875, 2 | BRF_GRA },           //  2
	{ "3.u507",			0x10000, 0xf81294cd, 2 | BRF_GRA },           //  3
	{ "4.u508",			0x10000, 0xfff72536, 2 | BRF_GRA },           //  4

	{ "5.u305",			0x10000, 0x28513fbf, 3 | BRF_GRA },           //  5 Background Tiles
	{ "6.u306",			0x10000, 0x1b8854c4, 3 | BRF_GRA },           //  6
	{ "7.u307",			0x10000, 0x35674ac2, 3 | BRF_GRA },           //  7
	{ "8.u308",			0x10000, 0x6299c376, 3 | BRF_GRA },           //  8

	{ "pic16c55.uc02",	0x00400, 0x62d1d85d, 0 | BRF_OPT },           //  decapped, presumed to be 16C55
};

STD_ROM_PICK(mosaic)
STD_ROM_FN(mosaic)

static INT32 MosaicInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvMosaic = {
	"mosaic", NULL, NULL, NULL, "1990",
	"Mosaic\0", NULL, "Space", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, mosaicRomInfo, mosaicRomName, NULL, NULL, NULL, NULL, MosaicInputInfo, MosaicDIPInfo,
	MosaicInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 224, 4, 3
};


// Mosaic (Fuuki)

static struct BurnRomInfo mosaicaRomDesc[] = {
	{ "mosaic_9.a02",	0x10000, 0xecb4f8aa, 1 | BRF_PRG | BRF_ESS }, //  0 Z180 Code

	{ "1.u505",			0x10000, 0x05f4cc70, 2 | BRF_GRA },           //  1 Foreground Tiles
	{ "2.u506",			0x10000, 0x78907875, 2 | BRF_GRA },           //  2
	{ "3.u507",			0x10000, 0xf81294cd, 2 | BRF_GRA },           //  3
	{ "4.u508",			0x10000, 0xfff72536, 2 | BRF_GRA },           //  4

	{ "5.u305",			0x10000, 0x28513fbf, 3 | BRF_GRA },           //  5 Background Tiles
	{ "6.u306",			0x10000, 0x1b8854c4, 3 | BRF_GRA },           //  6
	{ "7.u307",			0x10000, 0x35674ac2, 3 | BRF_GRA },           //  7
	{ "8.u308",			0x10000, 0x6299c376, 3 | BRF_GRA },           //  8

	{ "pic16c55.uc02",	0x00400, 0x62d1d85d, 0 | BRF_OPT },           //  decapped, presumed to be 16C55
};

STD_ROM_PICK(mosaica)
STD_ROM_FN(mosaica)

struct BurnDriver BurnDrvMosaica = {
	"mosaica", "mosaic", NULL, NULL, "1990",
	"Mosaic (Fuuki)\0", NULL, "Space (Fuuki license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, mosaicaRomInfo, mosaicaRomName, NULL, NULL, NULL, NULL, MosaicInputInfo, MosaicDIPInfo,
	MosaicInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 224, 4, 3
};


// Golden Fire II

static struct BurnRomInfo gfire2RomDesc[] = {
	{ "goldf2_i.7e",	0x10000, 0xa102f7d0, 1 | BRF_PRG | BRF_ESS }, //  0 Z180 Code

	{ "goldf2_a.1k",	0x40000, 0x1f086472, 2 | BRF_GRA },           //  1 Foreground Tiles
	{ "goldf2_b.1j",	0x40000, 0xedb0d40c, 2 | BRF_GRA },           //  2
	{ "goldf2_c.1i",	0x40000, 0xd0ebd486, 2 | BRF_GRA },           //  3
	{ "goldf2_d.1h",	0x40000, 0x2b56ae2c, 2 | BRF_GRA },           //  4

	{ "goldf2_e.1e",	0x20000, 0x61b8accd, 3 | BRF_GRA },           //  5 Background Tiles
	{ "goldf2_f.1d",	0x20000, 0x49f77e53, 3 | BRF_GRA },           //  6
	{ "goldf2_g.1b",	0x20000, 0xaa79f3bf, 3 | BRF_GRA },           //  7
	{ "goldf2_h.1a",	0x20000, 0xa3519259, 3 | BRF_GRA },           //  8
};

STD_ROM_PICK(gfire2)
STD_ROM_FN(gfire2)

static INT32 Gfire2Init()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvGfire2 = {
	"gfire2", NULL, NULL, NULL, "1992",
	"Golden Fire II\0", NULL, "Topis Corp", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, gfire2RomInfo, gfire2RomName, NULL, NULL, NULL, NULL, Gfire2InputInfo, Gfire2DIPInfo,
	Gfire2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 224, 4, 3
};
