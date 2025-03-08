// FB Alpha driver module
// Based on MAME driver by Phil Stroffolino

#include "tiles_generic.h"
#include "m6502_intf.h"
#include "ay8910.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6502ROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvM6502RAM;
static UINT16 *DrvVidRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT16 tile_bank;
static UINT8 flipscreen;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo MoleInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 start"	},

	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 3"	},

	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 4"	},
	{"P1 Button 5",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 5"	},
	{"P1 Button 6",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 6"	},

	{"P1 Button 7",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 7"	},
	{"P1 Button 8",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 8"	},
	{"P1 Button 9",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 9"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 start"	},

	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 fire 3"	},

	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 fire 4"	},
	{"P2 Button 5",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 5"	},
	{"P2 Button 6",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 6"	},

	{"P2 Button 7",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 7"	},
	{"P2 Button 8",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 8"	},
	{"P2 Button 9",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 9"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Mole)

static struct BurnDIPInfo MoleDIPList[]=
{
	{0x16, 0xff, 0xff, 0x10, NULL				},
	{0x17, 0xff, 0xff, 0x00, NULL				},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x16, 0x01, 0x10, 0x00, "Upright"			},
	{0x16, 0x01, 0x10, 0x10, "Cocktail"			},

	{0   , 0xfe, 0   ,    2, "Round Points"		},
	{0x17, 0x01, 0x01, 0x00, "Off"				},
	{0x17, 0x01, 0x01, 0x01, "On"				},

	{0   , 0xfe, 0   ,    2, "Coinage"			},
	{0x17, 0x01, 0x02, 0x00, "1 Coin  1 Credits"},
	{0x17, 0x01, 0x02, 0x02, "1 Coin  2 Credits"},
};

STDDIPINFO(Mole)

static UINT8 mole_protection_read(UINT8 offset)
{
	switch (offset)
	{
		case 0x08: return 0xb0;
		case 0x26:
			if (M6502GetPC(0) == 0x53d7)
			{
				return 0x06;
			}
			else
			{
				return 0xc6;
			}
		case 0x86: return 0x91;
		case 0xae: return 0x32;
	}

	return 0;
}

static UINT8 mole_read(UINT16 address)
{
	if ((address & 0xff00) == 0x0800) {
		return mole_protection_read(address);
	}

	if ((address & 0xfc00) == 0x8000) {
		return 0;
	}

	switch (address)
	{
		case 0x8d00:
			return DrvDips[1];

		case 0x8d40:
			return DrvInputs[0];

		case 0x8d80:
			return DrvInputs[1];

		case 0x8dc0:
			return DrvInputs[2];
	}

	return 0;
}

static void mole_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfc00) == 0x8000) {
		DrvVidRAM[(address & 0x3ff)] = data + tile_bank;
		return;
	}

	switch (address)
	{
		case 0x0800:
		case 0x0820:
		case 0x8c40:
		case 0x8c80:
		case 0x8c81:
		return;	// nop

		case 0x8400:
			tile_bank = data * 256;
		return;

		case 0x8c00:
			AY8910Write(0, 1, data);
		return;

		case 0x8c01:
			AY8910Write(0, 0, data);
		return;

		case 0x8d00:
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;

		case 0x8dc0:
			flipscreen = data & 1;
		return;
	}
}

static tilemap_callback( background )
{
	TILE_SET_INFO(0, DrvVidRAM[offs], 0, 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	M6502Open(0);
	M6502Reset();
	M6502Close();

	AY8910Reset(0);

	HiscoreReset();

	tile_bank = 0;
	flipscreen = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6502ROM		= Next; Next += 0x003000;

	DrvGfxROM		= Next; Next += 0x010000;

	DrvPalette		= (UINT32*)Next; Next += 0x0008 * sizeof(UINT32);

	AllRam			= Next;

	DrvM6502RAM		= Next; Next += 0x000400;
	DrvVidRAM		= (UINT16*)Next; Next += 0x000400 * sizeof(UINT16);

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[3] = { 0, 0x2000*8, 0x2000*8*2 };
	INT32 XOffs[8] = { STEP8(0,1) };
	INT32 YOffs[8] = { STEP8(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x6000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM, 0x6000);

	GfxDecode(0x0400, 3, 8, 8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvM6502ROM + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x1000,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x2000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM   + 0x0000,  3, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM   + 0x1000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM   + 0x2000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM   + 0x3000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM   + 0x4000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM   + 0x5000,  8, 1)) return 1;

		DrvGfxDecode();
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvM6502RAM,	0x0000, 0x03ff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM,	0x5000, 0x7fff, MAP_ROM); // mirror
	M6502MapMemory(DrvM6502ROM,	0xd000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(mole_write);
	M6502SetReadHandler(mole_read);
	M6502Close();

	AY8910Init(0, 2000000, 0);
	AY8910SetAllRoutes(0, 0.30, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, background_map_callback, 8, 8, 40, 25);
	GenericTilemapSetGfx(0, DrvGfxROM, 3, 8, 8, 0x10000, 0, 0);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6502Exit();
	AY8910Exit(0);

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 8; i++) {
		DrvPalette[i] = BurnHighCol((i&1)?0xff:0, (i&4)?0xff:0, (i&2)?0xff:0, 0);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	//GenericTilemapSetFlip(TMAP_GLOBAL, flipscreen ? TMAP_FLIPXY : 0);

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
		DrvInputs[1] = DrvDips[0];

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	M6502Open(0);
	M6502Run(62500); // 4mhz / 60hz / (240/256)
	M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
	M6502Run(4166); // 4mhz / 60hz / (16/256)
	M6502Close();

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

		SCAN_VAR(tile_bank);
		SCAN_VAR(flipscreen);
	}
	return 0;
}


// Mole Attack

static struct BurnRomInfo moleRomDesc[] = {
	{ "m3a.5h",	0x1000, 0x5fbbdfef, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "m2a.7h",	0x1000, 0xf2a90642, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "m1a.8h",	0x1000, 0xcff0119a, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "mea.4a",	0x1000, 0x49d89116, 2 | BRF_GRA },           //  3 Graphics
	{ "mca.6a",	0x1000, 0x04e90300, 2 | BRF_GRA },           //  4
	{ "maa.9a",	0x1000, 0x6ce9442b, 2 | BRF_GRA },           //  5
	{ "mfa.3a",	0x1000, 0x0d0c7d13, 2 | BRF_GRA },           //  6
	{ "mda.5a",	0x1000, 0x41ae1842, 2 | BRF_GRA },           //  7
	{ "mba.8a",	0x1000, 0x50c43fc9, 2 | BRF_GRA },           //  8
};

STD_ROM_PICK(mole)
STD_ROM_FN(mole)

struct BurnDriver BurnDrvMole = {
	"mole", NULL, NULL, NULL, "1982",
	"Mole Attack\0", NULL, "Yachiyo Electronics", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, moleRomInfo, moleRomName, NULL, NULL, NULL, NULL, MoleInputInfo, MoleDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 8,
	320, 200, 4, 3
};
