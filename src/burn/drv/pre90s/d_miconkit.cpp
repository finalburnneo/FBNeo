// FinalBurn Neo SNK Micon-Kit driver module
// Based on MAME driver by hap

#include "tiles_generic.h"
#include "z80_intf.h" // actually i8080A
#include "8255ppi.h"
#include "burn_gun.h"
#include "beep.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvI8080ROM;
static UINT8 *DrvI8080RAM;
static UINT8 *DrvVideoRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 vblank;
static INT32 input_select;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[1];
static INT16 Analog[4];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static UINT32 is_game = 0; // 1, 2 (micon2, smiconk)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo Micon2InputList[] = {
	{"Coin",			BIT_DIGITAL,	DrvJoy3 + 3,	"p1 coin"	},
	{"Start",			BIT_DIGITAL,	DrvJoy3 + 1,	"p1 start"	},

	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	A("P1 Paddle", 		BIT_ANALOG_REL, &Analog[0],		"p1 x-axis"),

	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 1"	},
	A("P2 Paddle",  	BIT_ANALOG_REL, &Analog[1],		"p2 x-axis"),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dips",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Micon2)
#undef A

static struct BurnDIPInfo Micon2DIPList[]=
{
	DIP_OFFSET(0x07)
	{0x00, 0xff, 0xff, 0xdf, NULL					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x00, 0x01, 0x04, 0x00, "Cocktail"				},
	{0x00, 0x01, 0x04, 0x04, "Upright"				},

	{0   , 0xfe, 0   ,    4, "Replay"				},
	{0x00, 0x01, 0x30, 0x00, "None"					},
	{0x00, 0x01, 0x30, 0x10, "400"					},
	{0x00, 0x01, 0x30, 0x20, "500"					},
	{0x00, 0x01, 0x30, 0x30, "600"					},

	{0   , 0xfe, 0   ,    2, "Coinage"				},
	{0x00, 0x01, 0x40, 0x40, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0x40, 0x00, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Lives"				},
	{0x00, 0x01, 0x80, 0x00, "3"					},
	{0x00, 0x01, 0x80, 0x80, "5"					},
};

STDDIPINFO(Micon2)

static struct BurnDIPInfo SmiconkDIPList[]=
{
	DIP_OFFSET(0x07)
	{0x00, 0xff, 0xff, 0xdf, NULL					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x00, 0x01, 0x04, 0x00, "Cocktail"				},
	{0x00, 0x01, 0x04, 0x04, "Upright"				},

	{0   , 0xfe, 0   ,    4, "Replay"				},
	{0x00, 0x01, 0x30, 0x00, "None"					},
	{0x00, 0x01, 0x30, 0x10, "800"					},
	{0x00, 0x01, 0x30, 0x20, "1000"					},
	{0x00, 0x01, 0x30, 0x30, "1200"					},

	{0   , 0xfe, 0   ,    2, "Coinage"				},
	{0x00, 0x01, 0x40, 0x40, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0x40, 0x00, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Lives"				},
	{0x00, 0x01, 0x80, 0x00, "3"					},
	{0x00, 0x01, 0x80, 0x80, "5"					},
};

STDDIPINFO(Smiconk)

static void __fastcall micronkit_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xf)
	{
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
			ppi8255_w(0, port & 0x03, data);
		return;
	}
}

static UINT8 __fastcall micronkit_read_port(UINT16 port)
{
	switch (port & 0xf)
	{
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
			return ppi8255_r(0, port & 0x03);

		case 0x04:
			return DrvInputs[input_select];
	}

	return 0;
}

static UINT8 ppiread_A()
{
	return BurnTrackballRead(input_select);
}

static UINT8 ppiread_C()
{
	return ~(vblank << 6);
}

static void ppiwrite_B(UINT8 data)
{
	data &= 0xf;
	beep_set_state(data ? true : false);
	beep_set_clock(248 * data);
}

static void ppiwrite_C(UINT8 data)
{
	input_select = data & 1;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	input_select = 0;

	beep_reset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvI8080ROM		= Next; Next += 0x002000;

	DrvPalette		= (UINT32*)Next; Next += 0x0008 * sizeof(UINT32);

	AllRam			= Next;

	DrvI8080RAM		= Next; Next += 0x000400;
	DrvVideoRAM		= Next; Next += 0x001000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		memset (DrvI8080ROM, 0xff, 0x2000);

		if (BurnLoadRom(DrvI8080ROM + 0x0000, 0, 1)) return 1;
		if (BurnLoadRom(DrvI8080ROM + 0x0400, 1, 1)) return 1;
		if (BurnLoadRom(DrvI8080ROM + 0x0800, 2, 1)) return 1;
		if (BurnLoadRom(DrvI8080ROM + 0x0c00, 3, 1)) return 1;
		if (BurnLoadRom(DrvI8080ROM + 0x1000, 4, 1)) return 1;
	}

	ZetInit(0); // i8080
	ZetOpen(0);
	ZetMapMemory(DrvI8080ROM,	0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvI8080RAM,	0x4000, 0x43ff, MAP_RAM);
	ZetMapMemory(DrvVideoRAM,	0x7000, 0x7fff, MAP_RAM);
	ZetSetOutHandler(micronkit_write_port);
	ZetSetInHandler(micronkit_read_port);
	ZetClose();

	ppi8255_init(1);
	ppi8255_set_read_ports(0, ppiread_A, NULL, ppiread_C);
	ppi8255_set_write_ports(0, NULL, ppiwrite_B, ppiwrite_C);

	beep_init(3250);
	beep_set_volume(0.25);

	BurnTrackballInit(1, 0x70/2); // set default value to centered
	BurnTrackballConfigStartStopPoints(0, 0x00, 0x70, 0x00, 0x70);
	BurnTrackballReadReset();

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	ppi8255_exit();
	BurnTrackballExit();

	beep_exit();

	BurnFreeMemIndex();

	return 0;
}

static void draw_layer()
{
	for (INT32 y = 0; y < nScreenHeight; y++)
	{
		for (INT32 x = 0; x < nScreenWidth; x++)
		{
			pTransDraw[(y * nScreenWidth) + x] = (DrvVideoRAM[(((y + 24) & 0xfe) << 4) | ((x & 0xf8) >> 3)] >> (x & 7)) & 1;
		}
	}
}

static void do_color(INT32 x, INT32 y, INT32 color, INT32 bgcolor)
{
	UINT16 *p = (UINT16*)&pTransDraw[(x * nScreenWidth) + y]; // screen flipped!
	if (p[0] == 1) p[0] = color;
	if (p[0] == 0) p[0] = bgcolor;
}

static void colorize_micon2()
{
	for (INT32 y = 0; y < nScreenWidth; y++) {
		for (INT32 x = 0; x < nScreenHeight; x++) {
			if (y >= 0 && y <= 46) do_color(x, y, 2, 0);
			if (y >= 47 && y <= 54) do_color(x, y, 3, 0);
			if (y >= 55 && y <= 62) do_color(x, y, 2, 0);
			if (y >= 63 && y <= 71) do_color(x, y, 4, 0);
			if (y >= 72 && y <= 79) do_color(x, y, 5, 0);

			// flipped
			if (y >= 192 && y <= 239) do_color(x, y, 2, 0);
			if (y >= 184 && y <= 191) do_color(x, y, 3, 0);
			if (y >= 177 && y <= 183) do_color(x, y, 2, 0);
			if (y >= 168 && y <= 176) do_color(x, y, 4, 0);
			if (y >= 160 && y <= 167) do_color(x, y, 5, 0);
		}
	}
}

static void colorize_smiconk()
{
	for (INT32 y = 0; y < nScreenWidth; y++) {
		for (INT32 x = 0; x < nScreenHeight; x++) {
			do_color(x, y, 2, 0); // all, amber
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPalette[0] = 0;
		DrvPalette[1] = BurnHighCol(0xff, 0xff, 0xff, 0);
		if (is_game == 1) { // micon2
			DrvPalette[2] = BurnHighCol(0xff, 0xff, 0x33, 0);
			DrvPalette[3] = BurnHighCol(0xff, 0x33, 0x33, 0);
			DrvPalette[4] = BurnHighCol(0x33, 0xff, 0x33, 0);
			DrvPalette[5] = BurnHighCol(0xff, 0x99, 0x33, 0); // amber
		} if (is_game == 2) { // smiconk
			DrvPalette[2] = BurnHighCol(0xff, 0x99, 0x33, 0); // amber
		}
		DrvRecalc = 0;
	}

	draw_layer();

	switch (is_game) {
		case 1: colorize_micon2(); break;
		case 2: colorize_smiconk(); break;
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
		UINT8 tmp = DrvDips[0] & 0xff;

		for (INT32 i = 0; i < 8; i++) {
			tmp ^= (DrvJoy3[i] & 1) << i;
		}

		DrvInputs[0] = tmp ^ (DrvJoy1[0] & 1);
		DrvInputs[1] = tmp ^ (DrvJoy2[0] & 1);

		BurnTrackballConfig(0, AXIS_REVERSED, AXIS_REVERSED);
		BurnTrackballFrame(0, Analog[0], Analog[1], 0x01, 0x1f);
		BurnTrackballUpdate(0);
	}

	INT32 nInterleave = 256/4;
	INT32 nCyclesTotal[1] = { 2048000 / 60 };
	INT32 nCyclesDone[1] = { 0 };

	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		if (i < 24/4) vblank = 1;
		if (i >= 24/4 && i < 232/4) vblank = 0;
		if (i >= 232/4) vblank = 1;

		CPU_RUN(0, Zet);
	}

	ZetClose();

	if (pBurnSoundOut) {
		beep_update(pBurnSoundOut, nBurnSoundLen);
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
		BurnTrackballScan();
		beep_scan(nAction, pnMin);

		SCAN_VAR(input_select);
	}

	return 0;
}


static INT32 micon2Init()
{
	is_game = 1;

	return DrvInit();
}

static INT32 smiconkInit()
{
	is_game = 2;

	return DrvInit();
}

// Micon-Kit Part II

static struct BurnRomInfo micon2RomDesc[] = {
	{ "ufo_no_0",	0x0400, 0x3eb5a299, 1 | BRF_PRG | BRF_ESS }, //  0 I8080 Code
	{ "ufo_no_1",	0x0400, 0xe796338e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ufo_no_2",	0x0400, 0xbf246cd7, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ufo_no_3",	0x0400, 0x0e93b4f0, 1 | BRF_PRG | BRF_ESS }, //  3
};

STD_ROM_PICK(micon2)
STD_ROM_FN(micon2)

struct BurnDriver BurnDrvMicon2 = {
	"micon2", NULL, NULL, NULL, "1978",
	"Micon-Kit Part II\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, micon2RomInfo, micon2RomName, NULL, NULL, NULL, NULL, Micon2InputInfo, Micon2DIPInfo,
	micon2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 8,
	208, 240, 3, 4
};


// Space Micon Kit

static struct BurnRomInfo smiconkRomDesc[] = {
	{ "1.1a",	0x0400, 0x927dfbb8, 1 | BRF_PRG | BRF_ESS }, //  0 I8080 Code
	{ "2.1b",	0x0400, 0x1d0f1474, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.1c",	0x0400, 0xb89cb388, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.1d",	0x0400, 0xa5897e6e, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "5.1e",	0x0400, 0x98db7810, 1 | BRF_PRG | BRF_ESS }, //  4
};

STD_ROM_PICK(smiconk)
STD_ROM_FN(smiconk)

struct BurnDriver BurnDrvSmiconk = {
	"smiconk", NULL, NULL, NULL, "1978",
	"Space Micon Kit\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, smiconkRomInfo, smiconkRomName, NULL, NULL, NULL, NULL, Micon2InputInfo, SmiconkDIPInfo,
	smiconkInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 8,
	208, 240, 3, 4
};
