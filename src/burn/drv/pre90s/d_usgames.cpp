// FB Alpha US Games - Trivia / Quiz / 'Amusement Only' Gambling Games Driver Module
// Based on MAME driver by David Haywood and Nicola Salmoria

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "ay8910.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6809ROM;
static UINT8 *DrvCharExp;
static UINT8 *DrvNVRAM;
static UINT8 *DrvCharRAM;
static UINT8 *DrvVidRAM;

static UINT32 *DrvPalette;
static UINT8  DrvRecalc;

static UINT8 bankdata;
static INT32 vblank;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvInputs[2];
static UINT8 DrvDips[1];
static UINT8 DrvReset;

static struct BurnInputInfo DrvInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 coin"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 4"	},
	{"P1 Button 5",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 5"	},

	{"P1 Play",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 fire 6"	},
	{"P1 Cancel",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 7"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Drv)

static struct BurnDIPInfo DrvDIPList[]=
{
	DIP_OFFSET(0x09)
	{0x00, 0xff, 0xff, 0xfd, NULL				},

	{0   , 0xfe, 0   ,    2, "Service Keyboard Attached?"	},
	{0x00, 0x01, 0x01, 0x01, "No"				},
	{0x00, 0x01, 0x01, 0x00, "Yes"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x00, 0x01, 0x02, 0x00, "Off"				},
	{0x00, 0x01, 0x02, 0x02, "On"				},
};

STDDIPINFO(Drv)

static void bankswitch(INT32 data)
{
	INT32 bank = (data & 0xf) * 0x4000;

	bankdata = data;

	M6809MapMemory(DrvM6809ROM + 0x10000 + bank, 0x4000, 0x7fff, MAP_ROM);
}

static void decode_char(INT32 offset, UINT8 data)
{
	for (INT32 i = 0; i < 8; i++) {
		DrvCharExp[(offset * 8) + i] = (data >> (7 - i)) & 1;
	}
}

static void usgames_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf800) == 0x2800) {
		if (DrvCharRAM[address & 0x7ff] != data) {
			DrvCharRAM[address & 0x7ff] = data;
			decode_char(address & 0x7ff, data);
		}
		return;
	}

	switch (address & ~0x0400)
	{
		case 0x2000:
		case 0x2001:
			AY8910Write(0, address & 1, data);
		return;

		case 0x2020: // lamps1
		case 0x2030: // lamps2
		return;

		case 0x2040: // mc6845 address_w
		case 0x2041: // mc6845 register_w
		return;

		case 0x2060:
			bankswitch(data);
		return;
	}
}

static UINT8 usgames_read(UINT16 address)
{
	switch (address & ~0x0400)
	{
		case 0x2000:
			return (DrvDips[0] & ~0x98) | (DrvInputs[1] & 0x18) | (vblank ? 0x80 : 0);

		case 0x2010:
			return DrvInputs[0];

		case 0x2041:
		case 0x2070:
			return 0xff;
	}

	return 0;
}

static tilemap_callback( background )
{
	TILE_SET_INFO(0, DrvVidRAM[offs*2], DrvVidRAM[offs*2+1], 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);
	memset (DrvCharExp, 0, 0x4000);

	M6809Open(0);
	M6809Reset();
	bankswitch(0);
	M6809Close();

	AY8910Reset(0);

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6809ROM		= Next; Next += 0x080000;

	DrvCharExp		= Next; Next += 0x004000;

	DrvPalette		= (UINT32*)Next; Next += 0x200 * sizeof(UINT32);

	DrvNVRAM		= Next; Next += 0x002000;

	AllRam			= Next;

	DrvCharRAM		= Next; Next += 0x000800;
	DrvVidRAM		= Next; Next += 0x001000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvInit(INT32 game_select)
{
	BurnAllocMemIndex();

	if (BurnLoadRom(DrvM6809ROM + 0x00000,  0, 1)) return 1;

	switch (game_select)
	{
		case 0: // usg32
		{
			if (BurnLoadRom(DrvM6809ROM + 0x18000,  1, 1)) return 1;
			if (BurnLoadRom(DrvM6809ROM + 0x28000,  2, 1)) return 1;
			if (BurnLoadRom(DrvM6809ROM + 0x38000,  3, 1)) return 1;
		}
		break;

		case 1: // superten, usg83x, usg82
		{
			if (BurnLoadRom(DrvM6809ROM + 0x18000,  1, 1)) return 1;
			if (BurnLoadRom(DrvM6809ROM + 0x28000,  2, 1)) return 1;
			if (BurnLoadRom(DrvM6809ROM + 0x38000,  3, 1)) return 1;
			if (BurnLoadRom(DrvM6809ROM + 0x48000,  4, 1)) return 1;
		}
		break;

		case 2: // usg187c, usg211c
		{
			if (BurnLoadRom(DrvM6809ROM + 0x10000,  1, 1)) return 1;
			if (BurnLoadRom(DrvM6809ROM + 0x20000,  2, 1)) return 1;
			if (BurnLoadRom(DrvM6809ROM + 0x30000,  3, 1)) return 1;
			if (BurnLoadRom(DrvM6809ROM + 0x40000,  4, 1)) return 1;
		}
		break;

		case 3: // usg182, usg185
		{
			if (BurnLoadRom(DrvM6809ROM + 0x10000,  1, 1)) return 1;
			if (BurnLoadRom(DrvM6809ROM + 0x20000,  2, 1)) return 1;
			if (BurnLoadRom(DrvM6809ROM + 0x30000,  3, 1)) return 1;
			if (BurnLoadRom(DrvM6809ROM + 0x48000,  4, 1)) return 1;
		}
		break;

		case 4: // usgames (0x4000-sized chunks loaded in reverse order)
		{
			if (BurnLoadRom(DrvM6809ROM + 0x70000,  1, 1)) return 1;
			for (INT32 i = 0; i < 0x10000; i++)
				DrvM6809ROM[0x10000 + i] = DrvM6809ROM[0x7c000^i];

			if (BurnLoadRom(DrvM6809ROM + 0x70000,  2, 1)) return 1;
			for (INT32 i = 0; i < 0x10000; i++)
				DrvM6809ROM[0x20000 + i] = DrvM6809ROM[0x7c000^i];

			if (BurnLoadRom(DrvM6809ROM + 0x70000,  3, 1)) return 1;
			for (INT32 i = 0; i < 0x10000; i++)
				DrvM6809ROM[0x30000 + i] = DrvM6809ROM[0x7c000^i];

			if (BurnLoadRom(DrvM6809ROM + 0x70000,  4, 1)) return 1;
			for (INT32 i = 0; i < 0x10000; i++)
				DrvM6809ROM[0x40000 + i] = DrvM6809ROM[0x7c000^i];
		}
		break;
	}

	M6809Init(0);
	M6809Open(0);
	M6809MapMemory(DrvNVRAM,	0x0000, 0x1fff, MAP_RAM);
	M6809MapMemory(DrvCharRAM,	0x2800, 0x2fff, MAP_ROM);
	M6809MapMemory(DrvVidRAM,	0x3000, 0x3fff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM,	0x8000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(usgames_write);
	M6809SetReadHandler(usgames_read);
	M6809Close();

	AY8910Init(0, 2000000, 0);
	AY8910SetAllRoutes(0, 0.30, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, background_map_callback, 8, 8, 64, 32);
	GenericTilemapSetGfx(0, DrvCharExp, 1, 8, 8, 0x800*8, 0, 0xff);
	GenericTilemapSetOffsets(0, -56, 0);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6809Exit();

	AY8910Exit(0);

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 j = 0; j < 0x200; j++)
	{
		UINT8 data = (j >> ((j & 1) ? 5 : 1)) & 0x0f;

		UINT8 r = (data & 1) >> 0;
		UINT8 g = (data & 2) >> 1;
		UINT8 b = (data & 4) >> 2;
		UINT8 i = (data & 8) >> 3;

		r = 0xff * r;
		g = 0x7f * g * (i + 1);
		b = 0x7f * b * (i + 1);

		DrvPalette[j] = BurnHighCol(r,g,b,0);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

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
		DrvInputs[0] = 0xff;
		DrvInputs[1] = 0xff;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 10;
	INT32 nCyclesTotal[1] = { 2000000 / 60 };
	INT32 nCyclesDone[1]  = { 0 };

	M6809Open(0);

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, M6809);

		if (i & 1) M6809SetIRQLine(0, CPU_IRQSTATUS_HOLD);

		if (i == 7) vblank = 1;
	}

	M6809Close();

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

		M6809Scan(nAction);
		AY8910Scan(nAction, pnMin);

		SCAN_VAR(bankdata);
	}

	if (nAction & ACB_NVRAM) {
		ba.Data		= DrvNVRAM;
		ba.nLen		= 0x02000;
		ba.nAddress	= 0;
		ba.szName	= "NV RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_WRITE) {
		M6809Open(0);
		bankswitch(bankdata);
		M6809Close();

		for (INT32 i = 0; i < 0x800; i++) {
			usgames_write(0x2800 + i, DrvCharRAM[i]);
		}
	}

	return 0;
}

// Super Duper Casino (California V3.2)

static struct BurnRomInfo usg32RomDesc[] = {
	{ "usg32-0.bin",			0x08000, 0xbc313387, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "usg32-1.bin",			0x08000, 0xbaaea800, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "usg32-2.bin",			0x08000, 0xd73d7f48, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "usg32-3.bin",			0x08000, 0x22747804, 1 | BRF_PRG | BRF_ESS }, //  3
};

STD_ROM_PICK(usg32)
STD_ROM_FN(usg32)

static INT32 Usg32Init()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvUsg32 = {
	"usg32", NULL, NULL, NULL, "1987",
	"Super Duper Casino (California V3.2)\0", NULL, "U.S. Games, Inc.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_QUIZ, 0,
	NULL, usg32RomInfo, usg32RomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	Usg32Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	400, 256, 4, 3
};


// Super Ten V8.3

static struct BurnRomInfo supertenRomDesc[] = {
	{ "usg83-rom0.bin",			0x08000, 0xaae84186, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "usg83-rom1.bin",			0x08000, 0x7b520b6f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "usg83-rom2.bin",			0x08000, 0x29fbb23b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "usg83-rom3.bin",			0x10000, 0x4e110844, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "usg83-rom4.bin",			0x08000, 0x437697c4, 1 | BRF_PRG | BRF_ESS }, //  4
};

STD_ROM_PICK(superten)
STD_ROM_FN(superten)

static INT32 SupertenInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvSuperten = {
	"superten", NULL, NULL, NULL, "1988",
	"Super Ten V8.3\0", NULL, "U.S. Games, Inc.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_QUIZ, 0,
	NULL, supertenRomInfo, supertenRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	SupertenInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	400, 256, 4, 3
};


// Super Ten V8.3X

static struct BurnRomInfo usg83xRomDesc[] = {
	{ "usg83x-rom0.bin",			0x8000, 0x4ad9b6e0, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "usg83-rom1.bin",			0x8000, 0x7b520b6f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "usg83-rom2.bin",			0x8000, 0x29fbb23b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "usg83x-rom3.bin",			0x8000, 0x41c475ac, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "usg83-rom4.bin",			0x8000, 0x437697c4, 1 | BRF_PRG | BRF_ESS }, //  4
};

STD_ROM_PICK(usg83x)
STD_ROM_FN(usg83x)

struct BurnDriver BurnDrvUsg83x = {
	"usg83x", "superten", NULL, NULL, "1988",
	"Super Ten V8.3X\0", NULL, "U.S. Games, Inc.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_QUIZ, 0,
	NULL, usg83xRomInfo, usg83xRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	SupertenInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	400, 256, 4, 3
};


// Super Ten V8.2

static struct BurnRomInfo usg82RomDesc[] = {
	{ "usg82-rom0.bin",			0x08000, 0x09c20b78, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "usg82-rom1.bin",			0x08000, 0x915a9ff4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "usg82-rom2.bin",			0x08000, 0x29fbb23b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "usg82-rom3.bin",			0x10000, 0x4e110844, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "usg82-rom4.bin",			0x08000, 0x437697c4, 1 | BRF_PRG | BRF_ESS }, //  4
};

STD_ROM_PICK(usg82)
STD_ROM_FN(usg82)

struct BurnDriver BurnDrvUsg82 = {
	"usg82", "superten", NULL, NULL, "1988",
	"Super Ten V8.2\0", NULL, "U.S. Games, Inc.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_QUIZ, 0,
	NULL, usg82RomInfo, usg82RomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	SupertenInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	400, 256, 4, 3
};


// Games V25.4X

static struct BurnRomInfo usgamesRomDesc[] = {
	{ "version_25.4x_rom0_cs=324591.u12",	0x08000, 0x766a855a, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "version_25.4x_rom2_cs=6a42e7.u28",	0x10000, 0xd44d2ffa, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "version_25.4x_rom1_cs=31a223.u18",	0x10000, 0x2fff1da2, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "version_25.4x_rom4_cs=5a447e.u36",	0x10000, 0xb6d007be, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "version_25.4x_rom3_cs=7924ba.u35",	0x10000, 0x9542295b, 1 | BRF_PRG | BRF_ESS }, //  4
};

STD_ROM_PICK(usgames)
STD_ROM_FN(usgames)

static INT32 UsgamesInit()
{
	return DrvInit(4);
}

struct BurnDriver BurnDrvUsgames = {
	"usgames", NULL, NULL, NULL, "1992",
	"Games V25.4X\0", NULL, "U.S. Games, Inc.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_QUIZ, 0,
	NULL, usgamesRomInfo, usgamesRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	UsgamesInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	400, 256, 4, 3
};

// Games V25.1
// Version 25.1 - all ROMs dumped matched the printed checksum

static struct BurnRomInfo usg251RomDesc[] = {
	{ "bb_rom0.251_4e36_u12.u12",	0x08000, 0xb9c6e9c6, 1 | BRF_PRG | BRF_ESS }, 	//  0 M6809 Code
	{ "gamerom_1-a_e9fd_u18.u18",	0x10000, 0x8feabf59, 1 | BRF_PRG | BRF_ESS }, 	//  1
	{ "gamerom_2-a_c61a_u28.u28",	0x10000, 0xeb225ef4, 1 | BRF_PRG | BRF_ESS }, 	//  2
	{ "gamerom_3-a_24ba_u35.u35",	0x10000, 0x9542295b, 1 | BRF_PRG | BRF_ESS }, 	//  3
	{ "gamerom_4-a_447e_u36.u36",	0x10000, 0xb6d007be, 1 | BRF_PRG | BRF_ESS }, 	//  4
	
	{ "pal16l8.u19",				0x00104, 0x00000000, 0 | BRF_OPT | BRF_NODUMP }, // 5 plds
	{ "bb_sec1.u46",				0x00104, 0x00000000, 0 | BRF_OPT | BRF_NODUMP }, // 6
};

STD_ROM_PICK(usg251)
STD_ROM_FN(usg251)

static INT32 Usg251Init()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvUsg251 = {
	"usg251", "usgames", NULL, NULL, "1991",
	"Games V25.1\0", NULL, "U.S. Games, Inc.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_QUIZ, 0,
	NULL, usg251RomInfo, usg251RomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	Usg251Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	400, 256, 4, 3
};

// Games V21.1C
// Version 21.1C - all ROMs dumped matched the printed checksum

static struct BurnRomInfo usg211cRomDesc[] = {
	{ "sdrom0c.211_4e17_u12.u12",	0x08000, 0x54986073, 1 | BRF_PRG | BRF_ESS }, 	//  0 M6809 Code
	{ "gamerom_1-a_e9fd_u18.u18",	0x10000, 0x8feabf59, 1 | BRF_PRG | BRF_ESS }, 	//  1
	{ "gamerom_2-a_c61a_u28.u28",	0x10000, 0xeb225ef4, 1 | BRF_PRG | BRF_ESS }, 	//  2
	{ "gamerom_3-a_24ba_u35.u35",	0x10000, 0x9542295b, 1 | BRF_PRG | BRF_ESS }, 	//  3
	{ "gamerom_4-a_447e_u36.u36",	0x10000, 0xb6d007be, 1 | BRF_PRG | BRF_ESS }, 	//  4
	
	{ "pal16l8.u19",				0x00104, 0x00000000, 0 | BRF_OPT | BRF_NODUMP }, // 5 plds
	{ "bb_sec1.u46",				0x00104, 0x00000000, 0 | BRF_OPT | BRF_NODUMP }, // 6
};

STD_ROM_PICK(usg211c)
STD_ROM_FN(usg211c)

static INT32 Usg211cInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvUsg211c = {
	"usg211c", "usgames", NULL, NULL, "1991",
	"Games V21.1C\0", NULL, "U.S. Games, Inc.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_QUIZ, 0,
	NULL, usg211cRomInfo, usg211cRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	Usg211cInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	400, 256, 4, 3
};


// Games V18.7C

static struct BurnRomInfo usg187cRomDesc[] = {
	{ "version_18.7c_rom0_cs=30a6ba.u12",	0x08000, 0x2f4ed125, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "version_18.7c_rom4_cs=90b95e.u36",	0x10000, 0xb104744d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "version_18.7c_rom3_cs=76aebf.u35",	0x10000, 0x795e71c8, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "version_18.7c_rom2_cs=8973c0.u28",	0x10000, 0xc6ba8a81, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "version_18.7c_rom1_cs=6dcfd3.u18",	0x10000, 0x1cfd934d, 1 | BRF_PRG | BRF_ESS }, //  4
};

STD_ROM_PICK(usg187c)
STD_ROM_FN(usg187c)

static INT32 Usg187cInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvUsg187c = {
	"usg187c", "usgames", NULL, NULL, "1991",
	"Games V18.7C\0", NULL, "U.S. Games, Inc.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_QUIZ, 0,
	NULL, usg187cRomInfo, usg187cRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	Usg187cInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	400, 256, 4, 3
};


// Games V18.5

static struct BurnRomInfo usg185RomDesc[] = {
	{ "version_18.5_rom0_cs=315d5c.u12",	0x08000, 0x2cc68502, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "version_18.5_rom4_cs=90b95e.u36",	0x10000, 0xb104744d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "version_18.5_rom3_cs=76aebf.u35",	0x10000, 0x795e71c8, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "version_18.5_rom2_cs=8973c0.u28",	0x10000, 0xc6ba8a81, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "version_18.5_rom1_cs=2cb91d.u18",	0x08000, 0xbd384e5a, 1 | BRF_PRG | BRF_ESS }, //  4
};

STD_ROM_PICK(usg185)
STD_ROM_FN(usg185)

static INT32 Usg185Init()
{
	return DrvInit(3);
}

struct BurnDriver BurnDrvUsg185 = {
	"usg185", "usgames", NULL, NULL, "1990",
	"Games V18.5\0", NULL, "U.S. Games, Inc.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_QUIZ, 0,
	NULL, usg185RomInfo, usg185RomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	Usg185Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	400, 256, 4, 3
};


// Games V18.2

static struct BurnRomInfo usg182RomDesc[] = {
	{ "version_18.2_rom0_cs=2e6ae3.u12",	0x08000, 0xf5a053c1, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "version_18.2_rom4_cs=90b95e.u36",	0x10000, 0xb104744d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "version_18.2_rom3_cs=76aebf.u35",	0x10000, 0x795e71c8, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "version_18.2_rom2_cs=8973c0.u28",	0x10000, 0xc6ba8a81, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "version_18.2_rom1_cs=2bf00d.u18",	0x08000, 0x73bbc1c8, 1 | BRF_PRG | BRF_ESS }, //  4
};

STD_ROM_PICK(usg182)
STD_ROM_FN(usg182)

struct BurnDriver BurnDrvUsg182 = {
	"usg182", "usgames", NULL, NULL, "1989",
	"Games V18.2\0", NULL, "U.S. Games, Inc.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_QUIZ, 0,
	NULL, usg182RomInfo, usg182RomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	Usg185Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	400, 256, 4, 3
};
