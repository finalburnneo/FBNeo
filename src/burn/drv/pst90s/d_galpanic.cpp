// FinalBurn Neo Gals Panic driver module
// Based on MAME driver by Nicola Salmoria

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "msm6295.h"
#include "pandora.h"
#include "kaneko_hit.h"
#include "watchdog.h"
#include "burn_pal.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvGfxROM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvBgRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvPandoraRAM;

static INT32 nOKIBank;

static INT32 nExtraCycles;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvDips[2];
static UINT16 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo DrvInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 10,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 8,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 8,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 9,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 10,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 11,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 9,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 8,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 9,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 13,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 14,	"service"	},
	{"Tilt",			BIT_DIGITAL,	DrvJoy3 + 13,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Drv)

static struct BurnDIPInfo GalpanicDIPList[]=
{
	{0x13, 0xff, 0xff, 0xfe, NULL					},
	{0x14, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x13, 0x01, 0x02, 0x02, "Off"					},
	{0x13, 0x01, 0x02, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x13, 0x01, 0x04, 0x04, "Off"					},
	{0x13, 0x01, 0x04, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Coin A"				},
	{0x13, 0x01, 0x30, 0x00, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x10, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x20, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x13, 0x01, 0xc0, 0xc0, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xc0, 0x80, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0xc0, 0x40, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0xc0, 0x00, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x14, 0x01, 0x03, 0x02, "Easy"					},
	{0x14, 0x01, 0x03, 0x03, "Normal"				},
	{0x14, 0x01, 0x03, 0x01, "Hard"					},
	{0x14, 0x01, 0x03, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x14, 0x01, 0x30, 0x10, "2"					},
	{0x14, 0x01, 0x30, 0x30, "3"					},
	{0x14, 0x01, 0x30, 0x20, "4"					},
	{0x14, 0x01, 0x30, 0x00, "5"					},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x14, 0x01, 0x40, 0x40, "Off"					},
	{0x14, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Character Test"		},
	{0x14, 0x01, 0x80, 0x80, "Off"					},
	{0x14, 0x01, 0x80, 0x00, "On"					},
};

STDDIPINFO(Galpanic)


static struct BurnDIPInfo GalpanicaDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL					},
	{0x14, 0xff, 0xff, 0xf7, NULL					},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x13, 0x01, 0x02, 0x02, "Off"					},
	{0x13, 0x01, 0x02, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x13, 0x01, 0x04, 0x04, "Off"					},
	{0x13, 0x01, 0x04, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Coin Mode"			},
	{0x13, 0x01, 0x08, 0x08, "Mode 1"				},
	{0x13, 0x01, 0x08, 0x00, "Mode 2"				},

	{0   , 0xfe, 0   ,    6, "Coin A"				},
	{0x13, 0x01, 0x30, 0x20, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x30, 0x10, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x30, 0x10, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0x30, 0x00, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0x30, 0x00, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    6, "Coin B"				},
	{0x13, 0x01, 0xc0, 0x80, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xc0, 0x40, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xc0, 0x40, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0xc0, 0x00, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0xc0, 0x00, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x14, 0x01, 0x03, 0x02, "Easy"					},
	{0x14, 0x01, 0x03, 0x03, "Normal"				},
	{0x14, 0x01, 0x03, 0x01, "Hard"					},
	{0x14, 0x01, 0x03, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x14, 0x01, 0x30, 0x10, "2"					},
	{0x14, 0x01, 0x30, 0x30, "3"					},
	{0x14, 0x01, 0x30, 0x20, "4"					},
	{0x14, 0x01, 0x30, 0x00, "5"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x14, 0x01, 0x40, 0x00, "Off"					},
	{0x14, 0x01, 0x40, 0x40, "On"					},
};

STDDIPINFO(Galpanica)

static void set_oki_bank(INT32 data)
{
	nOKIBank = data & 0xf;
	MSM6295SetBank(0, MSM6295ROM + nOKIBank * 0x10000, 0x30000, 0x3ffff);
}

static void __fastcall galpanic_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x900000:
			set_oki_bank(data);
			pandora_set_clear(data & 0x8000);
		return;

		case 0xa00000:
			// coin counter
		return;

		case 0xb00000:
		case 0xc00000:
		case 0xd00000:
		return;
	}
}

static void __fastcall galpanic_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x400001:
			MSM6295Write(0, data);
		return;

		case 0x900000:
			set_oki_bank(data);
			pandora_set_clear(data & 0x80);
		return;

		case 0xa00000:
		case 0xa00001:
			// coin counter
		return;

		case 0xb00000:
		case 0xb00001:
		case 0xc00000:
		case 0xc00001:
		case 0xd00000:
		case 0xd00001:
		return;
	}
}

static UINT16 __fastcall galpanic_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x800000:
			return (DrvInputs[0] & 0xff00) | (DrvDips[0]);

		case 0x800002:
			return (DrvInputs[1] & 0xff00) | (DrvDips[1]);

		case 0x800004:
			return DrvInputs[2];
	}

	return 0;
}

static UINT8 __fastcall galpanic_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x400001:
			return MSM6295Read(0);

		case 0x800000:
			return DrvInputs[0] >> 8;

		case 0x800001:
			return DrvDips[0];

		case 0x800002:
			return DrvInputs[1] >> 8;

		case 0x800003:
			return DrvDips[1];

		case 0x800004:
			return DrvInputs[2] >> 8;

		case 0x800005:
			return DrvInputs[2] >> 0;
	}

	return 0;
}

static void __fastcall galpanic_pandora_write_word(UINT32 address, UINT16 data)
{
	address &= 0x1ffe;

	data &= 0xff;

	DrvSprRAM[address + 0] = data;
	DrvSprRAM[address + 1] = data;

	DrvPandoraRAM[address/2] = data;
}

static void __fastcall galpanic_pandora_write_byte(UINT32 address, UINT8 data)
{
	address &= 0x1ffe;

	DrvSprRAM[address + 0] = data;
	DrvSprRAM[address + 1] = data;

	DrvPandoraRAM[address/2] = data;
}

static INT32 DrvDoReset(INT32 full_reset)
{
	if (full_reset) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	SekOpen(0);
	SekReset();
	SekClose();

	MSM6295Reset(0);
	set_oki_bank(3);

	BurnWatchdogResetEnable();
	kaneko_hit_calc_reset();

	nExtraCycles = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x400000;

	DrvGfxROM		= Next; Next += 0x200000;

	MSM6295ROM		= Next; Next += 0x100000;

	BurnPalette		= (UINT32*)Next; Next += 0x8400 * sizeof(UINT32);

	AllRam			= Next;

	Drv68KRAM		= Next; Next += 0x010000;
	DrvFgRAM		= Next; Next += 0x020000;
	DrvBgRAM		= Next; Next += 0x020000;
	DrvSprRAM		= Next; Next += 0x005000;
	DrvPandoraRAM	= Next; Next += 0x002000;
	BurnPalRAM		= Next; Next += 0x000800;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	static INT32 Plane[4]  = { STEP4(0,1) };
	static INT32 XOffs[16] = { STEP8(0,4), STEP8(256,4) };
	static INT32 YOffs[16] = { STEP8(0,32), STEP8(512,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x100000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM, 0x100000);

	GfxDecode(0x2000, 4, 16, 16, Plane, XOffs, YOffs, 0x400, tmp, DrvGfxROM);

	BurnFree (tmp);

	return 0;
}

static INT32 DrvInit(INT32 nLoadType)
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;

		if (BurnLoadRom(Drv68KROM  + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x100001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x100000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x200001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x200000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x300001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x300000, k++, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM  + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(MSM6295ROM + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(MSM6295ROM + 0x080000, k++, 1)) return 1;

		if (nLoadType == 1) {// galpanic
			if (BurnLoadRom(Drv68KROM  + 0x080000, k++, 1)) return 1;
		}
		if (nLoadType == 2) {// galpanica
			if (BurnLoadRom(Drv68KROM  + 0x000001, k++, 2)) return 1;
			if (BurnLoadRom(Drv68KROM  + 0x000000, k++, 2)) return 1;
		}

		DrvGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x3fffff, MAP_ROM);
	SekMapMemory(DrvFgRAM,		0x500000, 0x51ffff, MAP_RAM);
	SekMapMemory(DrvBgRAM,		0x520000, 0x53ffff, MAP_RAM);
	SekMapMemory(BurnPalRAM,	0x600000, 0x6007ff, MAP_RAM);
	SekMapMemory(DrvSprRAM,		0x700000, 0x704fff, MAP_RAM); // 0-1fff
	SekSetWriteWordHandler(0,	galpanic_write_word);
	SekSetWriteByteHandler(0,	galpanic_write_byte);
	SekSetReadWordHandler(0,	galpanic_read_word);
	SekSetReadByteHandler(0,	galpanic_read_byte);

	SekMapHandler(1,			0x700000, 0x701fff, MAP_WRITE);
	SekSetWriteWordHandler(1,	galpanic_pandora_write_word);
	SekSetWriteByteHandler(1,	galpanic_pandora_write_byte);

	kaneko_hit_calc_init(2, 	0xe00000);
	SekClose();

	BurnWatchdogInit(DrvDoReset, (nLoadType) ? -1 : 180);

	MSM6295Init(0, 2000000 / MSM6295_PIN7_LOW, 0);
	MSM6295SetRoute(0, 0.80, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	pandora_init(DrvPandoraRAM, DrvGfxROM, (0x200000 / (16 * 16)) - 1, 0x100, 0, -16);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	MSM6295Exit(0);
	MSM6295ROM = NULL;

	SekExit();

	pandora_exit();

	GenericTilesExit();

	BurnFreeMemIndex();

	return 0;
}

static void BuildPalette()
{
	for (INT32 i = 0; i < 0x8000; i++)
	{
		BurnPalette[0x400 + i] = BurnHighCol(pal5bit(i >> 5), pal5bit(i >> 10), pal5bit(i >> 0), 0);
	}
}

static void draw_bitmaps()
{
	UINT16 *bgram = (UINT16*)DrvBgRAM;
	UINT16 *fgram = (UINT16*)DrvFgRAM;
	UINT16 *dst = pTransDraw;

	for (INT32 i = 0; i < 224 * 256; i++)
	{
		INT32 p = fgram[i] & 0xff;

		dst[i] = p ? p : ((bgram[i] >> 1) + 0x400);
	}
}

static INT32 DrvDraw()
{
	if (BurnRecalc) {
		BuildPalette();
		BurnRecalc = 0;
	}

	BurnPaletteUpdate_GGGGGRRRRRBBBBBx();

	BurnTransferClear();

	if (nBurnLayer & 1) draw_bitmaps();

	pandora_update(pTransDraw);

	BurnTransferCopy(BurnPalette);

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	{
		memset (DrvInputs, 0xff, 3 * sizeof(UINT16));

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[1] =  { 12000000 / 60 };
	INT32 nCyclesDone[1] = { nExtraCycles };

	SekOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);

		if (i ==  64) SekSetIRQLine(5, CPU_IRQSTATUS_AUTO);
		if (i == nInterleave-1) SekSetIRQLine(3, CPU_IRQSTATUS_AUTO);
	}

	if (pBurnSoundOut) {
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
	}

	SekClose();

	nExtraCycles = nCyclesDone[0] - nCyclesTotal[0];

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	pandora_buffer_sprites();

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

		SekScan(nAction);
		MSM6295Scan(nAction, pnMin);
		BurnWatchdogScan(nAction);
		kaneko_hit_calc_scan(nAction);

		SCAN_VAR(nOKIBank);

		SCAN_VAR(nExtraCycles);
	}

	if (nAction & ACB_WRITE) {
		set_oki_bank(nOKIBank);
	}

	return 0;
}


// Gals Panic (unprotected, ver. 2.0)

static struct BurnRomInfo galpanicRomDesc[] = {
	{ "ver.2.0_pm_112-subic5.subic5",	0x020000, 0x8bf38add, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "ver.2.0_pm_111-subic6.subic6",	0x020000, 0x6dc4b075, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "pm005e.7",						0x080000, 0xd7ec650c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "pm004e.8",						0x080000, 0xd3af52bc, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "pm001e.14",						0x080000, 0x90433eb1, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "pm000e.15",						0x080000, 0x5d220f3f, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "pm003e.16",						0x080000, 0x6bb060fd, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "pm002e.17",						0x080000, 0x713ee898, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "pm006e.67",						0x100000, 0x57aec037, 2 | BRF_GRA },           //  8 Sprites

	{ "pm008e.l",						0x080000, 0xd9379ba8, 3 | BRF_SND },           //  9 Samples
	{ "pm007e.u",						0x080000, 0xc7ed7950, 3 | BRF_SND },           // 10

	{ "pm017e",							0x080000, 0xbc41b6ca, 1 | BRF_PRG | BRF_ESS }, // 11 68K Code (Extra)
};

STD_ROM_PICK(galpanic)
STD_ROM_FN(galpanic)

static INT32 GalpanicInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvGalpanic = {
	"galpanic", NULL, NULL, NULL, "1990",
	"Gals Panic (unprotected, ver. 2.0)\0", NULL, "Kaneko", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, galpanicRomInfo, galpanicRomName, NULL, NULL, NULL, NULL, DrvInputInfo, GalpanicDIPInfo,
	GalpanicInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x400,
	224, 256, 3, 4
};


// Gals Panic (unprotected)

static struct BurnRomInfo galpanicaRomDesc[] = {
	{ "pm110.4m2",						0x080000, 0xae6b17a8, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "pm109.4m1",						0x080000, 0xb85d792d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "pm005e.7",						0x080000, 0xd7ec650c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "pm004e.8",						0x080000, 0xd3af52bc, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "pm001e.14",						0x080000, 0x90433eb1, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "pm000e.15",						0x080000, 0x5d220f3f, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "pm003e.16",						0x080000, 0x6bb060fd, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "pm002e.17",						0x080000, 0x713ee898, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "pm006e.67",						0x100000, 0x57aec037, 2 | BRF_GRA },           //  8 Sprites

	{ "pm008e.l",						0x080000, 0xd9379ba8, 3 | BRF_SND },           //  9 Samples
	{ "pm007e.u",						0x080000, 0xc7ed7950, 3 | BRF_SND },           // 10

	{ "pm112.subic6",					0x020000, 0x7b972b58, 1 | BRF_PRG | BRF_ESS }, // 11 68K Code (Extra)
	{ "pm111.subic5",					0x020000, 0x4eb7298d, 1 | BRF_PRG | BRF_ESS }, // 12
};

STD_ROM_PICK(galpanica)
STD_ROM_FN(galpanica)

static INT32 GalpanicaInit()
{
	return DrvInit(2);
}

struct BurnDriver BurnDrvGalpanica = {
	"galpanica", "galpanic", NULL, NULL, "1990",
	"Gals Panic (unprotected)\0", NULL, "Kaneko", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, galpanicaRomInfo, galpanicaRomName, NULL, NULL, NULL, NULL, DrvInputInfo, GalpanicDIPInfo,
	GalpanicaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x400,
	224, 256, 3, 4
};


// Gals Panic (ULA protected, set 1)

static struct BurnRomInfo galpanicbRomDesc[] = {
	{ "pm110.4m2",						0x080000, 0xae6b17a8, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "pm109.4m1",						0x080000, 0xb85d792d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "pm005e.7",						0x080000, 0xd7ec650c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "pm004e.8",						0x080000, 0xd3af52bc, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "pm001e.14",						0x080000, 0x90433eb1, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "pm000e.15",						0x080000, 0x5d220f3f, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "pm003e.16",						0x080000, 0x6bb060fd, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "pm002e.17",						0x080000, 0x713ee898, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "pm006e.67",						0x100000, 0x57aec037, 2 | BRF_GRA },           //  8 Sprites

	{ "pm008e.l",						0x080000, 0xd9379ba8, 3 | BRF_SND },           //  9 Samples
	{ "pm007e.u",						0x080000, 0xc7ed7950, 3 | BRF_SND },           // 10
};

STD_ROM_PICK(galpanicb)
STD_ROM_FN(galpanicb)

static INT32 GalpanicbInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvGalpanicb = {
	"galpanicb", "galpanic", NULL, NULL, "1990",
	"Gals Panic (ULA protected, set 1)\0", NULL, "Kaneko", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, galpanicbRomInfo, galpanicbRomName, NULL, NULL, NULL, NULL, DrvInputInfo, GalpanicaDIPInfo,
	GalpanicbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x400,
	224, 256, 3, 4
};


// Gals Panic (ULA protected, set 2)

static struct BurnRomInfo galpaniccRomDesc[] = {
	{ "pm109p.u88-01.ic6",				0x020000, 0xa6d60dba, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "pm110p.u87-01.ic5",				0x020000, 0x3214fd48, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "pm005e.7",						0x080000, 0xd7ec650c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "pm004e.8",						0x080000, 0xd3af52bc, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "pm001e.14",						0x080000, 0x90433eb1, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "pm000e.15",						0x080000, 0x5d220f3f, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "pm003e.16",						0x080000, 0x6bb060fd, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "pm002e.17",						0x080000, 0x713ee898, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "pm006e.67",						0x100000, 0x57aec037, 2 | BRF_GRA },           //  8 Sprites

	{ "pm008e.l",						0x080000, 0xd9379ba8, 3 | BRF_SND },           //  9 Samples
	{ "pm007e.u",						0x080000, 0xc7ed7950, 3 | BRF_SND },           // 10
};

STD_ROM_PICK(galpanicc)
STD_ROM_FN(galpanicc)

struct BurnDriver BurnDrvGalpanicc = {
	"galpanicc", "galpanic", NULL, NULL, "1990",
	"Gals Panic (ULA protected, set 2)\0", NULL, "Kaneko", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, galpaniccRomInfo, galpaniccRomName, NULL, NULL, NULL, NULL, DrvInputInfo, GalpanicaDIPInfo,
	GalpanicbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x400,
	224, 256, 3, 4
};
