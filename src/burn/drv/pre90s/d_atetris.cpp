// FB Alpha driver module Atari Tetris driver module
// Based on MAME driver by Zsolt Vasvari

// To do:
//	verify bootleg set 2 sound

#include "tiles_generic.h"
#include "m6502_intf.h"
#include "slapstic.h"
#include "sn76496.h"
#include "pokey.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv6502ROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvNVRAM;
static UINT8 *Drv6502RAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvPalRAM;

static INT32 nvram_enable;

static UINT32 *DrvPalette;
static UINT8   DrvRecalc;

static INT32 watchdog;
static INT32 master_clock;
static INT32 vblank;
static INT32 is_Bootleg;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoyF[8];
static UINT8 DrvDips[1];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static INT32 nExtraCycles;

static struct BurnInputInfo AtetrisInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoyF + 0,	"p1 start"	},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 right"	},
	{"P1 Button",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoyF + 4,	"p2 start"	},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 right"	},
	{"P2 Button",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dips",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Atetris)

static struct BurnDIPInfo AtetrisDIPList[]=
{
	DIP_OFFSET(0x0d)
	{0x00, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x00, 0x01, 0x04, 0x00, "Off"			},
	{0x00, 0x01, 0x04, 0x04, "On"			},

	{0   , 0xfe, 0   ,    2, "Freeze Step"	},
	{0x00, 0x01, 0x08, 0x00, "Off"			},
	{0x00, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"	},
	{0x00, 0x01, 0x80, 0x00, "Off"			},
	{0x00, 0x01, 0x80, 0x80, "On"			},
};

STDDIPINFO(Atetris)

static struct BurnDIPInfo AtetriscDIPList[]=
{
	DIP_OFFSET(0x0d)
	{0x00, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x00, 0x01, 0x04, 0x00, "Off"			},
	{0x00, 0x01, 0x04, 0x04, "On"			},

	{0   , 0xfe, 0   ,    2, "Freeze Step"	},
	{0x00, 0x01, 0x08, 0x00, "Off"			},
	{0x00, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    0, "Flip Controls"},
	{0x00, 0x01, 0x20, 0x00, "Off"			},
	{0x00, 0x01, 0x20, 0x20, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"	},
	{0x00, 0x01, 0x80, 0x00, "Off"			},
	{0x00, 0x01, 0x80, 0x80, "On"			},
};

STDDIPINFO(Atetrisc)

static inline UINT8 atetris_slapstic_read(UINT16 offset)
{
	UINT8 ret = Drv6502ROM[((SlapsticBank() & 1) * 0x4000) + (offset & 0x3fff)];

	if (offset & 0x2000) SlapsticTweak(offset & 0x1fff);

	return ret;
}

static inline void DrvPaletteUpdate(UINT16 offset)
{
	INT32 r = (DrvPalRAM[offset] >> 5) & 7;
	INT32 g = (DrvPalRAM[offset] >> 2) & 7;
	INT32 b = (DrvPalRAM[offset] >> 0) & 3;

	r = (r << 5) | (r << 2) | (r >> 1);
	g = (g << 5) | (g << 2) | (g >> 1);
	b = (b << 6) | (b << 4) | (b << 2) | (b << 0);

	DrvPalette[offset] = BurnHighCol(r, g, b, 0);
}

static INT32 allpot0(INT32 offs)
{
    return (DrvInputs[0] & ~0x40) | (vblank << 6);
}

static INT32 allpot1(INT32 offs)
{
    return DrvInputs[1];
}

static UINT8 atetris_read(UINT16 address)
{
	if ((address & 0xc000) == 0x4000) {
		return atetris_slapstic_read(address);
	}

	if (is_Bootleg)
	{
		switch (address & ~0x03e0)
		{
			case 0x2808:
                return (DrvInputs[0] & ~0x40) | (vblank << 6);

			case 0x2818:
				return DrvInputs[1];
		}
	}
	else
	{
		switch (address & ~0x03ef)
		{
			case 0x2800:
				return pokey_read(0, address & 0xf);

			case 0x2810:
				return pokey_read(1, address & 0xf);
		}
	}
	return 0;
}

static void atetris_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfc00) == 0x2000) {
		DrvPalRAM[address & 0x00ff] = data;
		DrvPaletteUpdate(address & 0x00ff);
		return;
	}

	if ((address & 0xfc00) == 0x2400) {
		if (nvram_enable) {
			DrvNVRAM[address & 0x01ff] = data;
		}
		nvram_enable = 0;
		return;
	}

	if (is_Bootleg) // Bootleg set 2 sound system
	{
		switch (address)
		{
			case 0x2802:
				SN76496Write(0, data);
			return;

			case 0x2804:
				SN76496Write(1, data);
			return;

			case 0x2806:
				SN76496Write(2, data);
			return;
		}
	}
	else
	{
		switch (address & ~0x03ef)
		{
			case 0x2800:
				pokey1_w(address & 0xf, data);
			return;

			case 0x2810:
				pokey2_w(address & 0xf, data);
			return;
		}
    }

	switch (address & ~0x03ff)
	{
		case 0x3000:
			watchdog = 0;
		return;

		case 0x3400:
			nvram_enable = 1;
		return;

		case 0x3800:
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;

		case 0x3c00:
			// coin counter - (data & 0x20) -> 0, (data & 0x10) -> 1
		return;
	}
}

static INT32 DrvDoReset(INT32 full_reset)
{
	if (full_reset) {
		memset(AllRam, 0, RamEnd - AllRam);
	}

	M6502Open(0);
	M6502Reset();
	M6502Close();

	SlapsticReset();

	HiscoreReset();

	watchdog = 0;
	nvram_enable = 0;

	nExtraCycles = 0;

	return 0;
}

static tilemap_callback( atetris )
{
	INT32 attr = DrvVidRAM[offs * 2 + 1];

	TILE_SET_INFO(0, DrvVidRAM[offs * 2 + 0] | ((attr & 0x07) << 8), attr >> 4, 0);
}

static void DrvGfxExpand()
{
	for (INT32 i = (0x10000 - 1) * 2; i >= 0; i-=2) {
		DrvGfxROM[i + 1] = DrvGfxROM[i/2] & 0x0f;
		DrvGfxROM[i + 0] = DrvGfxROM[i/2] >> 4;
	}
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv6502ROM		= Next; Next += 0x010000;

	DrvGfxROM		= Next; Next += 0x020000;

	DrvPalette		= (UINT32*)Next; Next += 0x0100 * sizeof(UINT32);

	DrvNVRAM		= Next; Next += 0x000200;

	AllRam			= Next;

	DrvVidRAM		= Next; Next += 0x001000;
	Drv6502RAM		= Next; Next += 0x001000;
	DrvPalRAM		= Next; Next += 0x000100;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 CommonInit(INT32 boot)
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(Drv6502ROM, 0, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM , 1, 1)) return 1;

		DrvGfxExpand();
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(Drv6502RAM,		0x0000, 0x0fff, MAP_RAM);
	M6502MapMemory(DrvVidRAM,		0x1000, 0x1fff, MAP_RAM);
	M6502MapMemory(DrvPalRAM,		0x2000, 0x20ff, MAP_ROM);
	M6502MapMemory(DrvPalRAM,		0x2100, 0x21ff, MAP_ROM);
	M6502MapMemory(DrvPalRAM,		0x2200, 0x22ff, MAP_ROM);
	M6502MapMemory(DrvPalRAM,		0x2300, 0x23ff, MAP_ROM);
	M6502MapMemory(DrvNVRAM,		0x2400, 0x25ff, MAP_ROM);
	M6502MapMemory(DrvNVRAM,		0x2600, 0x27ff, MAP_ROM);
	M6502MapMemory(Drv6502ROM + 0x8000,	0x8000, 0xffff, MAP_ROM);
	M6502SetReadHandler(atetris_read);
	M6502SetWriteHandler(atetris_write);
	M6502Close();

	SlapsticInit(101);

	is_Bootleg = boot;
	master_clock = boot ? (14745600 / 8) : (14318180 / 8);

	if (is_Bootleg) { // Bootleg set 2 sound system
		SN76496Init(0, master_clock / 2, 0);
		SN76496Init(1, master_clock / 2, 1);
		SN76496Init(2, master_clock / 2, 1);
		SN76496SetRoute(0, 0.50, BURN_SND_ROUTE_BOTH);
		SN76496SetRoute(1, 0.50, BURN_SND_ROUTE_BOTH);
		SN76496SetRoute(2, 0.50, BURN_SND_ROUTE_BOTH);
	} else {
        PokeyInit(master_clock, 2, 0.45, 0);
        PokeyAllPotCallback(0, allpot0);
        PokeyAllPotCallback(1, allpot1);
	}

	GenericTilesInit();
	GenericTilemapInit(0, scan_rows_map_scan, atetris_map_callback, 8, 8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM, 4, 8, 8, 0x20000, 0, 0xf);

	memset (DrvNVRAM, 0xff, 0x200);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6502Exit();

	if (is_Bootleg) { // Bootleg set 2 sound system
		SN76496Exit();
	} else {
		PokeyExit();
	}
	SlapsticExit();

	BurnFreeMemIndex();

	return 0;
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x100; i++) {
			DrvPaletteUpdate(i);
		}

		DrvRecalc = 0;
	}

	GenericTilemapDraw(0, pTransDraw, -1);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset(1);
	}

	watchdog++;
	if (watchdog >= 180) {
		DrvDoReset(0);
	}

	{
		if (DrvJoyF[0]) DrvJoy2[0] = 1; // fake start buttons
		if (DrvJoyF[4]) DrvJoy2[4] = 1;

		DrvInputs[0] = DrvDips[0] & 0xbc;
		DrvInputs[1] = 0;
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 262;
	INT32 nCyclesTotal[1] = { master_clock / 60 };
	INT32 nCyclesDone[1] = { nExtraCycles };
	INT32 nSoundBufferPos = 0;

	M6502Open(0);

	vblank = 1;

	for (INT32 i = 0; i < nInterleave; i++)
	{
        CPU_RUN(0, M6502);

        if (i == 48 || i == 112 || i == 176 || i == 240) {
            M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
        }

		if (i == 240) vblank = 0;
		if (pBurnSoundOut && !is_Bootleg) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			pokey_update(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	M6502Close();

	nExtraCycles = nCyclesDone[0] - nCyclesTotal[0];

	if (pBurnSoundOut) {
		if (is_Bootleg) { // Bootleg set 2 sound system
			SN76496Update(pBurnSoundOut, nBurnSoundLen);
		} else {
			INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			if (nSegmentLength) {
				pokey_update(pSoundBuf, nSegmentLength);
			}
		}
	}

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction,INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029727;
	}

	if (nAction & ACB_NVRAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data		= DrvNVRAM;
		ba.nLen		= 0x000200;
		ba.szName	= "Nonvolatile RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		M6502Scan(nAction);
		SlapsticScan(nAction);

		if (is_Bootleg)	// Bootleg set 2 sound system
		{
			SN76496Scan(nAction, pnMin);
		} else {
			pokey_scan(nAction, pnMin);
		}

		SCAN_VAR(watchdog);
		SCAN_VAR(nvram_enable);

		SCAN_VAR(nExtraCycles);
	}

	return 0;
}


// Tetris (set 1)

static struct BurnRomInfo atetrisRomDesc[] = {
	{ "136066-1100.45f",	0x10000, 0x2acbdb09, 1 | BRF_ESS | BRF_PRG }, //  0 6502 Code

	{ "136066-1101.35a",	0x10000, 0x84a1939f, 2 | BRF_GRA },           //  1 Graphics Tiles
};

STD_ROM_PICK(atetris)
STD_ROM_FN(atetris)

static INT32 DrvInit()
{
	return CommonInit(0);
}

struct BurnDriver BurnDrvAtetris = {
	"atetris", NULL, NULL, NULL, "1988",
	"Tetris (set 1)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PUZZLE, 0,
	NULL, atetrisRomInfo, atetrisRomName, NULL, NULL, NULL, NULL, AtetrisInputInfo, AtetrisDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};


// Tetris (set 2)

static struct BurnRomInfo atetrisaRomDesc[] = {
	{ "d1",			0x10000, 0x2bcab107, 1 | BRF_ESS | BRF_PRG }, //  0 6502 Code

	{ "136066-1101.35a",	0x10000, 0x84a1939f, 2 | BRF_GRA },           //  1 Graphics Tiles
};

STD_ROM_PICK(atetrisa)
STD_ROM_FN(atetrisa)

struct BurnDriver BurnDrvAtetrisa = {
	"atetrisa", "atetris", NULL, NULL, "1988",
	"Tetris (set 2)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PUZZLE, 0,
	NULL, atetrisaRomInfo, atetrisaRomName, NULL, NULL, NULL, NULL, AtetrisInputInfo, AtetrisDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};


// Tetris (bootleg set 1)

static struct BurnRomInfo atetrisbRomDesc[] = {
	{ "tetris.01",		0x10000, 0x944d15f6, 1 | BRF_ESS | BRF_PRG }, //  0 6502 Code

	{ "tetris.02",		0x10000, 0x5c4e7258, 2 | BRF_GRA },           //  1 Graphics Tiles

	{ "tetris.03",		0x00800, 0x26618c0b, 0 | BRF_OPT },           //  2 Unknown Prom
};

STD_ROM_PICK(atetrisb)
STD_ROM_FN(atetrisb)

struct BurnDriver BurnDrvAtetrisb = {
	"atetrisb", "atetris", NULL, NULL, "1988",
	"Tetris (bootleg set 1)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PUZZLE, 0,
	NULL, atetrisbRomInfo, atetrisbRomName, NULL, NULL, NULL, NULL, AtetrisInputInfo, AtetrisDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};


// Tetris (bootleg set 2)

static struct BurnRomInfo atetrisb2RomDesc[] = {
	{ "k1-01",		0x10000, 0xfa056809, 1 | BRF_ESS | BRF_PRG }, //  0 6502 Code

	{ "136066-1101.35a",	0x10000, 0x84a1939f, 2 | BRF_GRA },           //  1 Graphics Tiles
	
	{ "4-pal16l8-a.9n",		0x00104, 0x3630e734, 0 | BRF_OPT },
	{ "5-pal16l8-a.9m",		0x00104, 0x53b64be1, 0 | BRF_OPT },
	{ "a-gal16v8-b.bin",	0x00117, 0xb1dfab0f, 0 | BRF_OPT },
	{ "b-gal16v8-b.bin",	0x00117, 0xb1dfab0f, 0 | BRF_OPT },
	{ "2-pal16r4-a.3r",		0x00104, 0xd71bdf27, 0 | BRF_OPT },
	{ "1-pal16l8-a.3g",		0x00104, 0xdcf0d2fe, 0 | BRF_OPT },
	{ "3-pal16r4-a.8p",		0x00104, 0xe007edf2, 0 | BRF_OPT },
	{ "c-gal16v8-b.bin",	0x00117, 0xe1a9db0b, 0 | BRF_OPT },
	
	{ "m3-7603-5.prom1",		0x00020, 0x79656af3, 0 | BRF_OPT },
};

STD_ROM_PICK(atetrisb2)
STD_ROM_FN(atetrisb2)

static INT32 BootInit()
{
	return CommonInit(1);
}

struct BurnDriver BurnDrvAtetrisb2 = {
	"atetrisb2", "atetris", NULL, NULL, "1988",
	"Tetris (bootleg set 2)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PUZZLE, 0,
	NULL, atetrisb2RomInfo, atetrisb2RomName, NULL, NULL, NULL, NULL, AtetrisInputInfo, AtetrisDIPInfo,
	BootInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};


// Tetris (cocktail set 1)

static struct BurnRomInfo atetriscRomDesc[] = {
	{ "tetcktl1.rom",	0x10000, 0x9afd1f4a, 1 | BRF_ESS | BRF_PRG }, //  0 6502 Code

	{ "136066-1103.35a",	0x10000, 0xec2a7f93, 2 | BRF_GRA },           //  1 Graphics Tiles
};

STD_ROM_PICK(atetrisc)
STD_ROM_FN(atetrisc)

struct BurnDriver BurnDrvAtetrisc = {
	"atetrisc", "atetris", NULL, NULL, "1989",
	"Tetris (cocktail set 1)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PUZZLE, 0,
	NULL, atetriscRomInfo, atetriscRomName, NULL, NULL, NULL, NULL, AtetrisInputInfo, AtetriscDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	240, 336, 3, 4
};


// Tetris (cocktail set 2)

static struct BurnRomInfo atetrisc2RomDesc[] = {
	{ "136066-1102.45f",	0x10000, 0x1bd28902, 1 | BRF_ESS | BRF_PRG }, //  0 6502 Code

	{ "136066-1103.35a",	0x10000, 0xec2a7f93, 2 | BRF_GRA },           //  1 Graphics Tiles
};

STD_ROM_PICK(atetrisc2)
STD_ROM_FN(atetrisc2)

struct BurnDriver BurnDrvAtetrisc2 = {
	"atetrisc2", "atetris", NULL, NULL, "1989",
	"Tetris (cocktail set 2)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PUZZLE, 0,
	NULL, atetrisc2RomInfo, atetrisc2RomName, NULL, NULL, NULL, NULL, AtetrisInputInfo, AtetriscDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	240, 336, 3, 4
};

