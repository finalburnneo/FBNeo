// FB Alpha Space Stranger driver module
// Based on MAME driver by 

#include "tiles_generic.h"
#include "z80_intf.h"
#include "samples.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvI8080ROM;
static UINT8 *DrvColPROM;
static UINT8 *DrvI8080RAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 flipscreen;
static UINT8 previous_port_data42 = 0;
static UINT8 previous_port_data44 = 0;

static INT32 vblank;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static struct BurnInputInfo SstrangrInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 6,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 1"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 6,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0	,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Sstrangr)

static struct BurnInputInfo Sstrngr2InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 6,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 1"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 6,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Sstrngr2)

static struct BurnDIPInfo SstrangrDIPList[]=
{
	{0x0a, 0xff, 0xff, 0x19, NULL				},
	{0x0b, 0xff, 0xff, 0x10, NULL				},

	{0   , 0xfe, 0   ,    4, "Extra Play"		},
	{0x0a, 0x01, 0x03, 0x00, "Never"			},
	{0x0a, 0x01, 0x03, 0x01, "3000"				},
	{0x0a, 0x01, 0x03, 0x02, "4000"				},
	{0x0a, 0x01, 0x03, 0x03, "5000"				},

	{0   , 0xfe, 0   ,    2, "Lives"			},
	{0x0a, 0x01, 0x04, 0x00, "3"				},
	{0x0a, 0x01, 0x04, 0x04, "4"				},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x0a, 0x01, 0x08, 0x08, "1000"				},
	{0x0a, 0x01, 0x08, 0x00, "2000"				},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x0a, 0x01, 0x10, 0x10, "Upright"			},
	{0x0a, 0x01, 0x10, 0x00, "Cocktail"			},
};

STDDIPINFO(Sstrangr)

static struct BurnDIPInfo Sstrngr2DIPList[]=
{
	{0x0a, 0xff, 0xff, 0x19, NULL				},
	{0x0b, 0xff, 0xff, 0x10, NULL				},
	{0x0c, 0xff, 0xff, 0xfc, NULL				},

	{0   , 0xfe, 0   ,    4, "Extra Play"		},
	{0x0a, 0x01, 0x03, 0x00, "Never"			},
	{0x0a, 0x01, 0x03, 0x01, "3000"				},
	{0x0a, 0x01, 0x03, 0x02, "4000"				},
	{0x0a, 0x01, 0x03, 0x03, "5000"				},

	{0   , 0xfe, 0   ,    2, "Lives"			},
	{0x0a, 0x01, 0x04, 0x00, "3"				},
	{0x0a, 0x01, 0x04, 0x04, "4"				},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x0a, 0x01, 0x08, 0x08, "1000"				},
	{0x0a, 0x01, 0x08, 0x00, "2000"				},

	{0   , 0xfe, 0   ,    2, "Coinage"			},
	{0x0a, 0x01, 0x10, 0x10, "1 Coin  1 Credits"},
	{0x0a, 0x01, 0x10, 0x00, "1 Coin  2 Credits"},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x0a, 0x01, 0x10, 0x10, "Upright"			},
	{0x0a, 0x01, 0x10, 0x00, "Cocktail"			},

	{0   , 0xfe, 0   ,    2, "Player's Bullet Speed (Cheat)"	},
	{0x0b, 0x01, 0x02, 0x00, "Slow"				},
	{0x0b, 0x01, 0x02, 0x02, "Fast"				},
};

STDDIPINFO(Sstrngr2)

static void __fastcall sstrangr_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x42:
		{
			INT32 last = previous_port_data42;

			if ( data & 0x01 && ~last & 0x01) BurnSamplePlay(9);	// Ufo Sound
			if ( data & 0x02 && ~last & 0x02) BurnSamplePlay(0);	// Shot Sound
			if ( data & 0x04 && ~last & 0x04) BurnSamplePlay(1);	// Base Hit
			if (~data & 0x04 &&  last & 0x04) BurnSampleStop(1);
			if ( data & 0x08 && ~last & 0x08) BurnSamplePlay(2);	// Invader Hit
			if ( data & 0x10 && ~last & 0x10) BurnSamplePlay(8);	// Bonus Missle Base

			previous_port_data42 = data;
		}
		return;

		case 0x44:
		{
			INT32 last = previous_port_data44;

			if (data & 0x01 && ~last & 0x01) BurnSamplePlay(3);		// Fleet 1
			if (data & 0x02 && ~last & 0x02) BurnSamplePlay(4);		// Fleet 2
			if (data & 0x04 && ~last & 0x04) BurnSamplePlay(5);		// Fleet 3
			if (data & 0x08 && ~last & 0x08) BurnSamplePlay(6);		// Fleet 4
			if (data & 0x10 && ~last & 0x10) BurnSamplePlay(7);		// Saucer Hit

			flipscreen = data & 0x20;

			previous_port_data44 = data;
		}
		return;
	}
}

static UINT8 __fastcall sstrangr_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x41:
			return (DrvDips[0] & 0x0f) ^ DrvInputs[0];

		case 0x42:
			return (DrvDips[1] & 0x18) ^ DrvInputs[1];

		case 0x44:
			return (DrvDips[2] & 0xfe) ^ (vblank ? 0 : 1);
	}

	return 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	BurnSampleReset();

	flipscreen = 0;
	previous_port_data42 = 0;
	previous_port_data44 = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvI8080ROM		= Next; Next += 0x002400;

	DrvColPROM		= Next; Next += 0x000400;

	DrvPalette		= (UINT32*)Next; Next += 0x0008 * sizeof(UINT32);

	AllRam			= Next;

	DrvI8080RAM		= Next; Next += 0x002000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvInit(INT32 game)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (game == 0) // sstrangr
	{
		if (BurnLoadRom(DrvI8080ROM + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvI8080ROM + 0x0400,  1, 1)) return 1;
		if (BurnLoadRom(DrvI8080ROM + 0x0800,  2, 1)) return 1;
		if (BurnLoadRom(DrvI8080ROM + 0x0c00,  3, 1)) return 1;
		if (BurnLoadRom(DrvI8080ROM + 0x1000,  4, 1)) return 1;
		if (BurnLoadRom(DrvI8080ROM + 0x1400,  5, 1)) return 1;
		if (BurnLoadRom(DrvI8080ROM + 0x1800,  6, 1)) return 1;
		if (BurnLoadRom(DrvI8080ROM + 0x1c00,  7, 1)) return 1;
	} else {
		if (BurnLoadRom(DrvI8080ROM + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvI8080ROM + 0x2000,  1, 1)) return 1;

		if (BurnLoadRom(DrvColPROM  + 0x0000,  2, 1)) return 1;
	}

	ZetInit(0);
	ZetOpen(0);
	for (INT32 i = 0; i < 0x10000; i+= 0x08000)
	{
		ZetMapMemory(DrvI8080ROM,			0x0000 + i, 0x1fff + i, MAP_ROM);
		ZetMapMemory(DrvI8080RAM,			0x2000 + i, 0x3fff + i, MAP_RAM);
		ZetMapMemory(DrvI8080ROM + 0x2000,	0x6000 + i, 0x63ff + i, MAP_ROM);
	}
	ZetSetOutHandler(sstrangr_write_port);
	ZetSetInHandler(sstrangr_read_port);
	ZetClose();

	BurnSampleInit(0);
	BurnSampleSetAllRoutesAllSamples(0.25, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	BurnSampleExit();

	BurnFree(AllMem);

	return 0;
}

static INT32 SstrangrDraw()
{
	if (DrvRecalc) {
		DrvPalette[0] = 0;
		DrvPalette[1] = BurnHighCol(0xff,0xff,0xff,0);
		DrvRecalc = 0;
	}

	for (INT32 offs = 0x400; offs < 0x2000; offs++)
	{
		UINT8 y = (offs >> 5) - 32;
		UINT8 x = offs << 3;

		UINT8 data = DrvI8080RAM[offs];
		UINT16 *dst = pTransDraw + (y * nScreenWidth) + x;

		for (INT32 i = 0; i < 8; i++)
		{
			if (flipscreen)
			{
				dst[i] = data >> 7;
				data <<= 1;
			}
			else
			{
				dst[i] = data & 0x01;
				data >>= 1;
			}
		}
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 Sstrangr2Draw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 8; i++) {
			DrvPalette[i] = BurnHighCol((i&1)?0xff:0, (i&4)?0xff:0, (i&2)?0xff:0, 0);
		}
		DrvRecalc = 0;
	}

	INT32 color_base = flipscreen ? 0x0000 : 0x0200;

	for (INT32 offs = 0x400; offs < 0x2000; offs++)
	{
		UINT8 y = (offs >> 5) - 32;
		UINT8 x = offs << 3;

		UINT8 data = DrvI8080RAM[offs];
		UINT8 fore_color = DrvColPROM[color_base + ((offs >> 9) << 5) + (offs & 0x1f)] & 0x07;

		UINT16 *dst = pTransDraw + (y * nScreenWidth) + x;

		for (int i = 0; i < 8; i++)
		{
			if (flipscreen)
			{
				dst[i] = (data & 0x80) ? fore_color : 0;
				data <<= 1;
			}
			else
			{
				dst[i] = (data & 0x01) ? fore_color : 0;
				data >>= 1;
			}
		}
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
		DrvInputs[0] = 0x10;
		DrvInputs[1] = 0x01;
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 16;
	INT32 nCyclesTotal = 1996800 / 60;
	INT32 nCyclesDone = 0;

	ZetOpen(0);

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		INT32 nSegment = nCyclesTotal / nInterleave;

		nCyclesDone += ZetRun(nSegment);

		if (i == 7 || i == 12) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		if (i == 12) vblank = 1; // ??
	}

	ZetClose();

	if (pBurnSoundOut) {
		BurnSampleRender(pBurnSoundOut, nBurnSoundLen);
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

		SCAN_VAR(flipscreen);
		SCAN_VAR(previous_port_data42);
		SCAN_VAR(previous_port_data44);
	}

	return 0;
} 


static struct BurnSampleInfo InvadersSampleDesc[] = {
#if !defined (ROM_VERIFY)
	{ "1", SAMPLE_NOLOOP },	// Shot/Missle
	{ "2", SAMPLE_NOLOOP },	// Base Hit/Explosion
	{ "3", SAMPLE_NOLOOP },	// Invader Hit
	{ "4", SAMPLE_NOLOOP },	// Fleet move 1
	{ "5", SAMPLE_NOLOOP },	// Fleet move 2
	{ "6", SAMPLE_NOLOOP },	// Fleet move 3
	{ "7", SAMPLE_NOLOOP },	// Fleet move 4
	{ "8", SAMPLE_NOLOOP },	// UFO/Saucer Hit
	{ "9", SAMPLE_NOLOOP },	// Bonus Base
	{ "18",SAMPLE_NOLOOP },	// UFO Sound
#endif
	{ "", 0 }
};

STD_SAMPLE_PICK(Invaders)
STD_SAMPLE_FN(Invaders)


// Space Stranger

static struct BurnRomInfo sstrangrRomDesc[] = {
	{ "hss-01.58",	0x0400, 0xfeec7600, 1 | BRF_PRG | BRF_ESS }, //  0 I8080 Code
	{ "hss-02.59",	0x0400, 0x7281ff0b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "hss-03.60",	0x0400, 0xa09ec572, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "hss-04.61",	0x0400, 0xec411aca, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "hss-05.62",	0x0400, 0x7b1b81dd, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "hss-06.63",	0x0400, 0xde383625, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "hss-07.64",	0x0400, 0x2e41d0f0, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "hss-08.65",	0x0400, 0xbd14d0b0, 1 | BRF_PRG | BRF_ESS }, //  7
};

STD_ROM_PICK(sstrangr)
STD_ROM_FN(sstrangr)

static INT32 SstrangrInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvSstrangr = {
	"sstrangr", NULL, NULL, "invaders", "1978",
	"Space Stranger\0", NULL, "Yachiyo Electronics, Ltd.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, sstrangrRomInfo, sstrangrRomName, NULL, NULL, InvadersSampleInfo, InvadersSampleName, SstrangrInputInfo, SstrangrDIPInfo,
	SstrangrInit, DrvExit, DrvFrame, SstrangrDraw, DrvScan, &DrvRecalc, 2,
	224, 262, 3, 4
};


// Space Stranger 2

static struct BurnRomInfo sstrangr2RomDesc[] = {
	{ "4764.09",	0x2000, 0xd88f86cc, 1 | BRF_PRG | BRF_ESS }, //  0 I8080 Code
	{ "2708.10",	0x0400, 0xeba304c1, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "2708.15",	0x0400, 0xc176a89d, 2 | BRF_GRA },           //  2 Color data
};

STD_ROM_PICK(sstrangr2)
STD_ROM_FN(sstrangr2)

static INT32 Sstrangr2Init()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvSstrangr2 = {
	"sstrangr2", "sstrangr", NULL, "invaders", "1979",
	"Space Stranger 2\0", NULL, "Yachiyo Electronics, Ltd.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, sstrangr2RomInfo, sstrangr2RomName, NULL, NULL, InvadersSampleInfo, InvadersSampleName, Sstrngr2InputInfo, Sstrngr2DIPInfo,
	Sstrangr2Init, DrvExit, DrvFrame, Sstrangr2Draw, DrvScan, &DrvRecalc, 8,
	224, 262, 3, 4
};
