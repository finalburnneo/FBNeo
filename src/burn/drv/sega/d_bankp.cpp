// FB Alpha Bank Panic Driver Module
// Based on MAME driver by Nicola Salmoria

#include "tiles_generic.h"
#include "z80_intf.h"
#include "sn76496.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvVidRAM0;
static UINT8 *DrvVidRAM1;
static UINT8 *DrvColRAM0;
static UINT8 *DrvColRAM1;
static UINT8 *DrvColPROM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 scrollx;
static UINT8 priority;
static UINT8 nmi_enable;
static UINT8 flipscreen;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[1];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo bankpInputList[] = {
	{"P1 Coin"      , BIT_DIGITAL  , DrvJoy1 + 5,	"p1 coin"  },
	{"P1 Start"     , BIT_DIGITAL  , DrvJoy2 + 5,	"p1 start" },
	{"P1 Left"      , BIT_DIGITAL  , DrvJoy1 + 3, 	"p1 left"  },
	{"P1 Right"     , BIT_DIGITAL  , DrvJoy1 + 1, 	"p1 right" },
	{"P1 Button 1"  , BIT_DIGITAL  , DrvJoy1 + 4,	"p1 fire 1"},
	{"P1 Button 2"  , BIT_DIGITAL  , DrvJoy1 + 7,	"p1 fire 2"},
	{"P1 Button 3"  , BIT_DIGITAL  , DrvJoy3 + 0,	"p1 fire 3"},

	{"P2 Coin"      , BIT_DIGITAL  , DrvJoy1 + 6,	"p2 coin"  },
	{"P2 Start"     , BIT_DIGITAL  , DrvJoy2 + 6,	"p2 start" },
	{"P2 Left"      , BIT_DIGITAL  , DrvJoy2 + 3, 	"p2 left"  },
	{"P2 Right"     , BIT_DIGITAL  , DrvJoy2 + 1, 	"p2 right" },
	{"P2 Button 1"  , BIT_DIGITAL  , DrvJoy2 + 4,	"p2 fire 1"},
	{"P2 Button 2"  , BIT_DIGITAL  , DrvJoy2 + 7,	"p2 fire 2"},
	{"P2 Button 3"  , BIT_DIGITAL  , DrvJoy3 + 1,	"p2 fire 3"},

	{"P3 Coin"      , BIT_DIGITAL  , DrvJoy3 + 2,	"p3 coin"  },

	{"Reset"        , BIT_DIGITAL  , &DrvReset  ,	"reset"    },
	{"Dip 1"        , BIT_DIPSWITCH, DrvDips + 0,   "dip"      },
};

STDINPUTINFO(bankp)

static struct BurnInputInfo combhInputList[] = {
	{"P1 Coin"      , BIT_DIGITAL  , DrvJoy1 + 5,	"p1 coin"  },
	{"P1 Start"     , BIT_DIGITAL  , DrvJoy2 + 5,	"p1 start" },
	{"P1 Up"        , BIT_DIGITAL  , DrvJoy1 + 0, 	"p1 up"    },
	{"P1 Down"      , BIT_DIGITAL  , DrvJoy1 + 2, 	"p1 down"  },
	{"P1 Button 1"  , BIT_DIGITAL  , DrvJoy1 + 4,	"p1 fire 1"},
	{"P1 Button 2"  , BIT_DIGITAL  , DrvJoy1 + 7,	"p1 fire 2"},
	{"P1 Button 3"  , BIT_DIGITAL  , DrvJoy3 + 0,	"p1 fire 3"},

	{"P2 Coin"      , BIT_DIGITAL  , DrvJoy1 + 6,	"p2 coin"  },
	{"P2 Start"     , BIT_DIGITAL  , DrvJoy2 + 6,	"p2 start" },
	{"P2 Up"        , BIT_DIGITAL  , DrvJoy2 + 0, 	"p2 up"    },
	{"P2 Down"      , BIT_DIGITAL  , DrvJoy2 + 2, 	"p2 down"  },
	{"P2 Button 1"  , BIT_DIGITAL  , DrvJoy2 + 4,	"p2 fire 1"},
	{"P2 Button 2"  , BIT_DIGITAL  , DrvJoy2 + 7,	"p2 fire 2"},
	{"P2 Button 3"  , BIT_DIGITAL  , DrvJoy3 + 1,	"p2 fire 3"},

	{"P3 Coin"      , BIT_DIGITAL  , DrvJoy3 + 2,	"p3 coin"  },

	{"Reset"        , BIT_DIGITAL  , &DrvReset  ,	"reset"    },
	{"Dip 1"        , BIT_DIPSWITCH, DrvDips + 0,   "dip"      },

};

STDINPUTINFO(combh)

static struct BurnDIPInfo bankpDIPList[]=
{
	{0x10, 0xff, 0xff, 0xc0, NULL					},

	{0   , 0xfe, 0   , 4   , "Coin A/B"				},
	{0x10, 0x01, 0x03, 0x03, "3C 1C"				},
	{0x10, 0x01, 0x03, 0x02, "2C 1C"				},
	{0x10, 0x01, 0x03, 0x00, "1C 1C"				},
	{0x10, 0x01, 0x03, 0x01, "1C 2C"				},

	{0   , 0xfe, 0   , 2   , "Coin C"				},
	{0x10, 0x01, 0x04, 0x04, "2C 1C"				},
	{0x10, 0x01, 0x04, 0x00, "1C 1C"				},

	{0   , 0xfe, 0   , 2   , "Lives"				},
	{0x10, 0x01, 0x08, 0x00, "3"					},
	{0x10, 0x01, 0x08, 0x08, "4"					},

	{0   , 0xfe, 0   , 2   , "Bonus Life"	 		},
	{0x10, 0x01, 0x10, 0x00, "70K 200K 500K..."		},
	{0x10, 0x01, 0x10, 0x10, "100K 400K 800K..."	},

	{0   , 0xfe, 0   , 2   , "Difficulty"			},
	{0x10, 0x01, 0x20, 0x00, "Easy"					},
	{0x10, 0x01, 0x20, 0x20, "Hard"					},

	{0   , 0xfe, 0   , 2   , "Demo Sounds"			},
	{0x10, 0x01, 0x40, 0x00, "Off"					},
	{0x10, 0x01, 0x40, 0x40, "On"					},

	{0   , 0xfe, 0   , 2   , "Cabinet"				},
	{0x10, 0x01, 0x80, 0x80, "Upright"				},
	{0x10, 0x01, 0x80, 0x00, "Cocktail"				},
};

STDDIPINFO(bankp)

static struct BurnDIPInfo combhDIPList[]=
{
	{0x10, 0xff, 0xff, 0x10, NULL					},

	{0   , 0xfe, 0   , 2   , "Flip Screen"			},
	{0x10, 0x01, 0x01, 0x00, "Off"					},
	{0x10, 0x01, 0x01, 0x01, "On"					},

	{0   , 0xfe, 0   , 4   , "Coinage"				},
	{0x10, 0x01, 0x06, 0x06, "2C 1C"				},
	{0x10, 0x01, 0x06, 0x00, "1C 1C"				},
	{0x10, 0x01, 0x06, 0x02, "1C 2C"				},
	{0x10, 0x01, 0x06, 0x04, "1C 3C"				},

	{0   , 0xfe, 0   , 2   , "Lives"				},
	{0x10, 0x01, 0x08, 0x00, "3"					},
	{0x10, 0x01, 0x08, 0x08, "4"					},

	{0   , 0xfe, 0   , 2   , "Cabinet"				},
	{0x10, 0x01, 0x10, 0x10, "Upright" 				},
	{0x10, 0x01, 0x10, 0x00, "Cocktail"				},

	{0   , 0xfe, 0   , 2   , "Difficulty"			},
	{0x10, 0x01, 0x40, 0x00, "Easy"					},
	{0x10, 0x01, 0x40, 0x40, "Hard"					},

	{0   , 0xfe, 0   , 2   , "Fuel"					},
	{0x10, 0x01, 0x80, 0x00, "120 Units" 			},
	{0x10, 0x01, 0x80, 0x80, "90 Units"				},
};

STDDIPINFO(combh)

static void __fastcall bankp_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
		case 0x02:
			SN76496Write(port & 3, data);
		return;

		case 0x05:
			scrollx = data;
		return;

		case 0x07:
			priority   = data & 0x03;
			nmi_enable = data & 0x10;
			flipscreen = data & 0x20;
		return;
	}
}

static UINT8 __fastcall bankp_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
		case 0x02:
			return DrvInputs[port & 3];

		case 0x04:
			return DrvDips[0];
	}

	return 0;
}

static tilemap_callback( bg )
{
	INT32 attr = DrvColRAM1[offs];
	INT32 code = DrvVidRAM1[offs] + ((attr & 7) * 256);
	INT32 color = attr >> 4;

	TILE_SET_INFO(0, code, color, (attr & 0x08) ? TILE_FLIPX : 0);
	*category = color;
}

static tilemap_callback( fg )
{
	INT32 attr = DrvColRAM0[offs];
	INT32 code = DrvVidRAM0[offs] + ((attr & 3) * 256);
	INT32 color = attr >> 3;

	TILE_SET_INFO(1, code, color, (attr & 0x04) ? TILE_FLIPX : 0);
	*category = color;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	SN76496Reset();

	nmi_enable = 0;
	scrollx = 0;
	flipscreen = 0;
	priority = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x00e000;

	DrvGfxROM0		= Next; Next += 0x010000;
	DrvGfxROM1		= Next; Next += 0x020000;

	DrvColPROM		= Next; Next += 0x000220;

	DrvPalette		= (UINT32*)Next; Next += 0x100 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x001000;
	DrvVidRAM0		= Next; Next += 0x000400;
	DrvColRAM0		= Next; Next += 0x000400;
	DrvVidRAM1		= Next; Next += 0x000400;
	DrvColRAM1		= Next; Next += 0x000400;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode() // 0, 0x80
{
	INT32 Plane0[2] = { 0, 4 };
	INT32 Plane1[3] = { 0, 0x20000, 0x40000 };
	INT32 XOffs0[8] = { STEP4(64+3,-1), STEP4(0+3,-1) };
	INT32 XOffs1[8] = { STEP8(7,-1) };
	INT32 YOffs0[8] = { STEP8(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x10000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x10000);

	GfxDecode(0x400, 2, 8, 8, Plane0, XOffs0, YOffs0, 0x080, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x10000);

	GfxDecode(0x800, 3, 8, 8, Plane1, XOffs1, YOffs0, 0x040, tmp, DrvGfxROM1);

	BurnFree (tmp);

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
		INT32 k = 0;
		if (BurnLoadRom(DrvZ80ROM  + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x4000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x8000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0xc000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x2000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x2000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x4000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x6000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x8000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0xa000, k++, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0020, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0120, k++, 1)) return 1;

	    DrvGfxDecode();
	}
	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0xdfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,		0xe000, 0xefff, MAP_RAM);
	ZetMapMemory(DrvVidRAM0,	0xf000, 0xf3ff, MAP_RAM);
	ZetMapMemory(DrvColRAM0,	0xf400, 0xf7ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM1,	0xf800, 0xfbff, MAP_RAM);
	ZetMapMemory(DrvColRAM1,	0xfc00, 0xffff, MAP_RAM);
	ZetSetOutHandler(bankp_write_port);
	ZetSetInHandler(bankp_read_port);
	ZetClose();

	SN76489Init(0, 15468000 / 6, 0);
	SN76489Init(1, 15468000 / 6, 1);
	SN76489Init(2, 15468000 / 6, 1);
	SN76496SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);
	SN76496SetRoute(1, 1.00, BURN_SND_ROUTE_BOTH);
	SN76496SetRoute(2, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, fg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM1, 3, 8, 8, 0x20000, 0x80, 0x0f);
	GenericTilemapSetGfx(1, DrvGfxROM0, 2, 8, 8, 0x10000, 0x00, 0x1f);
	GenericTilemapCategoryConfig(0, 0x10);
	GenericTilemapCategoryConfig(1, 0x20);
	for (INT32 i = 0; i < 0x80; i++) {
		GenericTilemapSetCategoryEntry(0, i/0x08, i & 7, (DrvColPROM[0x120 + i] == 0) ? 1 : 0);
		GenericTilemapSetCategoryEntry(1, i/0x04, i & 3, (DrvColPROM[0x020 + i] == 0) ? 1 : 0);
	}
	GenericTilemapSetOffsets(TMAP_GLOBAL, -24, -16);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	SN76496Exit();

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	UINT32 pal[16];

	for (INT32 i = 0; i < 16; i++)
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

	for (INT32 i = 0; i < 0x80; i++) {
		DrvPalette[i + 0x00] = pal[DrvColPROM[0x020 + i] & 0xf];
	}

	for (INT32 i = 0; i < 0x80; i++) {
		DrvPalette[i + 0x80] = pal[DrvColPROM[0x120 + i] & 0xf];
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	GenericTilemapSetScrollX(1, scrollx);

	BurnTransferClear();
	if (priority & 2)
	{
		if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, TMAP_FORCEOPAQUE);
		if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);
	}
	else
	{
		if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, TMAP_FORCEOPAQUE);
		if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 0);
	}
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

	INT32 nCyclesTotal = 2578000 / 60;

	ZetOpen(0);
	ZetRun(nCyclesTotal);
	if (nmi_enable) ZetNmi();
	ZetClose();
	
	if (pBurnSoundOut) {
		SN76496Update(0, pBurnSoundOut, nBurnSoundLen);
		SN76496Update(1, pBurnSoundOut, nBurnSoundLen);
		SN76496Update(2, pBurnSoundOut, nBurnSoundLen);
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
		*pnMin = 0x029521;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);
		SN76496Scan(nAction, pnMin);

		SCAN_VAR(scrollx);
		SCAN_VAR(priority);
		SCAN_VAR(flipscreen);
		SCAN_VAR(nmi_enable);
	}

	return 0;
}


// Bank Panic

static struct BurnRomInfo bankpRomDesc[] = {
	{ "epr-6175.7e",       0x4000, 0x044552b8, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 Code	
	{ "epr-6174.7f",       0x4000, 0xd29b1598, 1 | BRF_ESS | BRF_PRG }, //  1	
	{ "epr-6173.7h",       0x4000, 0xb8405d38, 1 | BRF_ESS | BRF_PRG }, //  2	
	{ "epr-6176.7d",       0x2000, 0xc98ac200, 1 | BRF_ESS | BRF_PRG }, //  3	

	{ "epr-6165.5l",       0x2000, 0xaef34a93, 2 | BRF_GRA },	        //  4 Foreground Characters
	{ "epr-6166.5k",       0x2000, 0xca13cb11, 2 | BRF_GRA },	        //  5

	{ "epr-6172.5b",       0x2000, 0xc4c4878b, 3 | BRF_GRA },	        //  6 Background Characters
	{ "epr-6171.5d",       0x2000, 0xa18165a1, 3 | BRF_GRA },	        //  7
	{ "epr-6170.5e",       0x2000, 0xb58aa8fa, 3 | BRF_GRA },	        //  8
	{ "epr-6169.5f",       0x2000, 0x1aa37fce, 3 | BRF_GRA },	        //  9
	{ "epr-6168.5h",       0x2000, 0x05f3a867, 3 | BRF_GRA },	        // 10
	{ "epr-6167.5i",       0x2000, 0x3fa337e1, 3 | BRF_GRA },	        // 11

	{ "pr-6177.8a",        0x0020, 0xeb70c5ae, 4 | BRF_GRA },	        // 12 Color PROM
	{ "pr-6178.6f",        0x0100, 0x0acca001, 4 | BRF_GRA },	        // 13 Foreground Color LUT
	{ "pr-6179.5a",        0x0100, 0xe53bafdb, 4 | BRF_GRA },	        // 14 Background Color LUT

	{ "315-5074.2c.bin",   0x025b, 0x2e57bbba, 0 | BRF_OPT },	        // 15 PALs
	{ "315-5073.pal16l4",  0x0001, 0x00000000, 0 | BRF_OPT | BRF_NODUMP }, // 16
};

STD_ROM_PICK(bankp)
STD_ROM_FN(bankp)

struct BurnDriver BurnDrvbankp = {
	"bankp", NULL, NULL, NULL, "1984",
	"Bank Panic\0", NULL, "[Sanritsu] Sega", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 3, HARDWARE_SEGA_MISC, GBF_SHOOT, 0,
	NULL, bankpRomInfo, bankpRomName, NULL, NULL, NULL, NULL, bankpInputInfo, bankpDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 224, 4, 3
};


// Combat Hawk

static struct BurnRomInfo combhRomDesc[] = {
	{ "epr-10904.7e",      0x4000, 0x4b106335, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 Code	
	{ "epr-10905.7f",      0x4000, 0xa76fc390, 1 | BRF_ESS | BRF_PRG }, //  1	
	{ "epr-10906.7h",      0x4000, 0x16d54885, 1 | BRF_ESS | BRF_PRG }, //  2	
	{ "epr-10903.7d",      0x2000, 0xb7a59cab, 1 | BRF_ESS | BRF_PRG }, //  3	

	{ "epr-10914.5l",      0x2000, 0x7d7a2340, 2 | BRF_GRA },	        //  4 Foreground Characters
	{ "epr-10913.5k",      0x2000, 0xd5c1a8ae, 2 | BRF_GRA },	        //  5

	{ "epr-10907.5b",      0x2000, 0x08e5eea3, 3 | BRF_GRA },	        //  6 Background Characters
	{ "epr-10908.5d",      0x2000, 0xd9e413f5, 3 | BRF_GRA },	        //  7
	{ "epr-10909.5e",      0x2000, 0xfec7962c, 3 | BRF_GRA },	        //  8
	{ "epr-10910.5f",      0x2000, 0x33db0fa7, 3 | BRF_GRA },	        //  9
	{ "epr-10911.5h",      0x2000, 0x565d9e6d, 3 | BRF_GRA },	        // 10
	{ "epr-10912.5i",      0x2000, 0xcbe22738, 3 | BRF_GRA },	        // 11

	{ "pr-10900.8a",       0x0020, 0xf95fcd66, 4 | BRF_GRA },	        // 12 Color PROM
	{ "pr-10901.6f",       0x0100, 0x6fd981c8, 4 | BRF_GRA },	        // 13 Foreground Color LUT
	{ "pr-10902.5a",       0x0100, 0x84d6bded, 4 | BRF_GRA },	        // 14 Background Color LUT

	{ "315-5074.2c.bin",   0x025b, 0x2e57bbba, 0 | BRF_OPT },	        // 15 PALs
	{ "315-5073.pal16l4",  0x0001, 0x00000000, 0 | BRF_OPT | BRF_NODUMP }, // 16
};

STD_ROM_PICK(combh)
STD_ROM_FN(combh)

struct BurnDriver BurnDrvcombh = {
	"combh", NULL, NULL, NULL, "1987",
	"Combat Hawk\0", NULL, "Sega / Sanritsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 3, HARDWARE_SEGA_MISC, GBF_SHOOT, 0,
	NULL, combhRomInfo, combhRomName, NULL, NULL, NULL, NULL, combhInputInfo, combhDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 224, 3, 4
};
