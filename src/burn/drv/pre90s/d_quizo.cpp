// FB Alpha  Quiz Olympic driver module
// Based on MAME driver by Tomasz Slanina

#include "tiles_generic.h"
#include "z80_intf.h"
#include "driver.h"
extern "C" {
#include "ay8910.h"
}

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvVidRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT16 *pAY8910Buffer[3];

static UINT8 port60;
static UINT8 port70;

static UINT8  DrvJoy1[8];
static UINT8  DrvJoy2[8];
static UINT8  DrvDips[1];
static UINT8  DrvInputs[2];
static UINT8  DrvReset;

static struct BurnInputInfo QuizoInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Tilt",		BIT_DIGITAL,	DrvJoy1 + 3,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Quizo)

static struct BurnDIPInfo QuizoDIPList[]=
{
	{0x07, 0xff, 0xff, 0x40, NULL			},

	{0   , 0xfe, 0   ,    2, "Test Mode"		},
	{0x07, 0x01, 0x08, 0x00, "Off"			},
	{0x07, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    2, "Coin A"		},
	{0x07, 0x01, 0x40, 0x00, "2 Coins 1 Credits"	},
	{0x07, 0x01, 0x40, 0x40, "1 Coin  1 Credits"	},
};

STDDIPINFO(Quizo)

static void set_rom_bank(UINT8 data)
{
	INT32 lut[10] = { 2, 3, 4, 4, 4, 4, 4, 5, 0, 1};
	if (data > 9) data = 0;
	port60 = data;

	ZetMapMemory(DrvZ80ROM + 0x4000 + lut[data] * 0x4000, 0x8000, 0xbfff, MAP_ROM);
}

static void set_ram_bank(UINT8 data)
{
	port70 = data;
	ZetMapMemory(DrvVidRAM + ((data & 8) ? 0x4000 : 0), 0xc000, 0xffff, MAP_RAM);
}

static void __fastcall quizo_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x50:
			AY8910Write(0, 0, data);
		break;

		case 0x51:
			AY8910Write(0, 1, data);
		break;

		case 0x60:
			set_rom_bank(data);
		break;

		case 0x70:
			set_ram_bank(data);
		break;
	}
}

static UINT8 __fastcall quizo_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
			return DrvInputs[0];

		case 0x10:
			return DrvInputs[1];

		case 0x40:
			return DrvDips[0];
	}

	return 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	set_rom_bank(0);
	set_ram_bank(0);
	ZetClose();

	AY8910Reset(0);

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; 		Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x01c000;

	DrvColPROM		= Next; Next += 0x000020;

	DrvPalette		= (UINT32*)Next; Next += 0x0010 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x000800;
	DrvVidRAM		= Next; Next += 0x008000;

	RamEnd			= Next;

	pAY8910Buffer[0]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[1]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[2]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);

	MemEnd			= Next;

	return 0;
}

static INT32 DrvInit(INT32 select)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (select == 0)
	{
		if (BurnLoadRom(DrvZ80ROM + 0x00000, 0, 1)) return 1;
		memcpy (DrvZ80ROM + 0x0000, DrvZ80ROM + 0x4000, 0x4000);

		if (BurnLoadRom(DrvZ80ROM + 0x04000, 1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x0c000, 2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x14000, 3, 1)) return 1;

		if (BurnLoadRom(DrvColPROM         , 4, 1)) return 1;
	}
	else if (select == 1)
	{
		if (BurnLoadRom(DrvZ80ROM + 0x00000, 0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x04000, 1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x08000, 2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x0c000, 3, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x10000, 4, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x14000, 5, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x18000, 6, 1)) return 1;

		if (BurnLoadRom(DrvColPROM         , 7, 1)) return 1;
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,		0x4000, 0x47ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,		0xc000, 0xffff, MAP_RAM);
	ZetSetOutHandler(quizo_write_port);
	ZetSetInHandler(quizo_read_port);
	ZetClose();

	AY8910Init(0, 1342329, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.50, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	ZetExit();
	AY8910Exit(0);

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x10; i++)
	{
		INT32 bit0 = 0;
		INT32 bit1 = (DrvColPROM[i] >> 0) & 0x01;
		INT32 bit2 = (DrvColPROM[i] >> 1) & 0x01;
		INT32 b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = (DrvColPROM[i] >> 2) & 0x01;
		bit1 = (DrvColPROM[i] >> 3) & 0x01;
		bit2 = (DrvColPROM[i] >> 4) & 0x01;
		INT32 g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = (DrvColPROM[i] >> 5) & 0x01;
		bit1 = (DrvColPROM[i] >> 6) & 0x01;
		bit2 = (DrvColPROM[i] >> 7) & 0x01;
		INT32 r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_layer()
{
	for (INT32 y = 0; y < 200; y++)
	{
		UINT16 *dst = pTransDraw + y * 320;

		for (INT32 x = 0; x < 320; x+=4)
		{
			INT32 a = DrvVidRAM[y * 80 + x/4];
			INT32 b = DrvVidRAM[y * 80 + 0x4000 + x/4];

			dst[x+3] = ((a & 1) >> 0) | ((a >> 3) & 2) | ((b & 1) << 2) | ((b >> 1) & 8);
			dst[x+2] = ((a & 2) >> 1) | ((a >> 4) & 2) | ((b & 2) << 1) | ((b >> 2) & 8);
			dst[x+1] = ((a & 4) >> 2) | ((a >> 5) & 2) | ((b & 4) << 0) | ((b >> 3) & 8);
			dst[x+0] = ((a & 8) >> 3) | ((a >> 6) & 2) | ((b & 8) >> 1) | ((b >> 4) & 8);
		}
	}
}

static INT32 DrvDraw()
{
	//if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	//}

	draw_layer();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		DrvInputs[0] = 0x18;
		DrvInputs[1] = 0xff;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	ZetOpen(0);
	ZetRun(4000000 / 60);
	ZetSetIRQLine(0, CPU_IRQSTATUS_AUTO);
	ZetClose();

	if (pBurnSoundOut) {
		AY8910Render(&pAY8910Buffer[0], pBurnSoundOut, nBurnSoundLen, 0);
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
		*pnMin = 0x029521;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);
		AY8910Scan(nAction, pnMin);

		SCAN_VAR(port60);
		SCAN_VAR(port70);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		set_rom_bank(port60);
		set_ram_bank(port70);
		ZetClose();
	}

	return 0;
}


// Quiz Olympic (set 1)

static struct BurnRomInfo quizoRomDesc[] = {
	{ "rom1",  	0x8000, 0x6731735f, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 code
	{ "rom2",  	0x8000, 0xa700eb30, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "rom3",   	0x8000, 0xd344f97e, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "rom4",   	0x8000, 0xab1eb174, 1 | BRF_ESS | BRF_PRG }, //  3

	{ "82s123", 	0x0020, 0xc3f15914, 2 | BRF_GRA },	     //  4 Color Prom
};

STD_ROM_PICK(quizo)
STD_ROM_FN(quizo)

static INT32 QuizoInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvQuizo = {
	"quizo", NULL, NULL, NULL, "1985",
	"Quiz Olympic (set 1)\0", NULL, "Seoul Coin Corp.", "Miscellaneous",
	L"\uD034\uC988\uC62C\uB9BC\uD53D (set 1)\0Quiz Olympic (set 1)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_MISC_PRE90S, GBF_QUIZ, 0,
	NULL, quizoRomInfo, quizoRomName, NULL, NULL, QuizoInputInfo, QuizoDIPInfo,
	QuizoInit, DrvExit, DrvFrame, NULL, DrvScan, NULL, 0x10,
	320, 200, 4, 3
};


// Quiz Olympic (set 2)

static struct BurnRomInfo quizoaRomDesc[] = {
	{ "7.bin",	0x4000, 0x1579ae31, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 code
	{ "6.bin",	0x4000, 0xf00f6356, 1 | BRF_GRA },           //  1
	{ "5.bin",	0x4000, 0x39e577e3, 1 | BRF_GRA },           //  2
	{ "4.bin",	0x4000, 0xa977bd3a, 1 | BRF_GRA },           //  3
	{ "3.bin",	0x4000, 0x4411bcff, 1 | BRF_GRA },           //  4
	{ "2.bin",	0x4000, 0x4a0df776, 1 | BRF_GRA },           //  5
	{ "1.bin",	0x4000, 0xd9566c1a, 1 | BRF_GRA },           //  6

	{ "82s123",	0x0020, 0xc3f15914, 2 | BRF_GRA },           //  7 Color Prom
};

STD_ROM_PICK(quizoa)
STD_ROM_FN(quizoa)

static INT32 QuizoaInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvQuizoa = {
	"quizoa", "quizo", NULL, NULL, "1985",
	"Quiz Olympic (set 2)\0", NULL, "Seoul Coin Corp.", "Miscellaneous",
	L"\uD034\uC988\uC62C\uB9BC\uD53D (set 2)\0Quiz Olympic (set 2)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_QUIZ, 0,
	NULL, quizoaRomInfo, quizoaRomName, NULL, NULL, QuizoInputInfo, QuizoDIPInfo,
	QuizoaInit, DrvExit, DrvFrame, NULL, DrvScan, NULL, 0x10,
	320, 200, 4, 3
};
