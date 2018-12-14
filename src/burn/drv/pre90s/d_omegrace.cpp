// FB Alpha Omega Race driver moulde
// Based on MAME driver by Bernd Wiebelt

// Todo/tofix:
// Player 2's spinner input is encrypted. :( (no known decryption table)

#include "tiles_generic.h"
#include "z80_intf.h"
#include "ay8910.h"
#include "watchdog.h"
#include "avgdvg.h"
#include "vector.h"
#include "bitswap.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvVidPROM;
static UINT8 *DrvNVRAM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvVectorRAM;
static UINT8 *DrvVectorROM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 avgletsgo;
static UINT8 soundlatch;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static INT32 DrvPaddle[2];

static struct BurnInputInfo OmegraceInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 start"	},
	{"P1 Left",		    BIT_DIGITAL,	DrvJoy3 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 start"	},
	{"P2 Left",		    BIT_DIGITAL,	DrvJoy3 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 7,	"service"	},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 4,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Omegrace)

static struct BurnDIPInfo OmegraceDIPList[]=
{
	{0x0f, 0xff, 0xff, 0xff, NULL						},
	{0x10, 0xff, 0xff, 0x3f, NULL						},
	{0x11, 0xff, 0xff, 0x80, NULL						},

	{0   , 0xfe, 0   ,    4, "1st Bonus Life"			},
	{0x0f, 0x01, 0x03, 0x00, "40k"						},
	{0x0f, 0x01, 0x03, 0x01, "50k"						},
	{0x0f, 0x01, 0x03, 0x02, "70k"						},
	{0x0f, 0x01, 0x03, 0x03, "100k"						},

	{0   , 0xfe, 0   ,    4, "2nd & 3rd Bonus Life"		},
	{0x0f, 0x01, 0x0c, 0x00, "150k 250k"				},
	{0x0f, 0x01, 0x0c, 0x04, "250k 500k"				},
	{0x0f, 0x01, 0x0c, 0x08, "500k 750k"				},
	{0x0f, 0x01, 0x0c, 0x0c, "750k 1500k"				},

	{0   , 0xfe, 0   ,    4, "Credit(s)/Ships"			},
	{0x0f, 0x01, 0x30, 0x00, "1C/2S 2C/4S"				},
	{0x0f, 0x01, 0x30, 0x10, "1C/2S 2C/5S"				},
	{0x0f, 0x01, 0x30, 0x20, "1C/3S 2C/6S"				},
	{0x0f, 0x01, 0x30, 0x30, "1C/3S 2C/7S"				},

	{0   , 0xfe, 0   ,    8, "Coin A"					},
	{0x10, 0x01, 0x07, 0x06, "2 Coins 1 Credits"		},
	{0x10, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x10, 0x01, 0x07, 0x03, "4 Coins 5 Credits"		},
	{0x10, 0x01, 0x07, 0x04, "3 Coins 4 Credits"		},
	{0x10, 0x01, 0x07, 0x05, "2 Coins 3 Credits"		},
	{0x10, 0x01, 0x07, 0x00, "1 Coin  2 Credits"		},
	{0x10, 0x01, 0x07, 0x01, "1 Coin  3 Credits"		},
	{0x10, 0x01, 0x07, 0x02, "1 Coin  5 Credits"		},

	{0   , 0xfe, 0   ,    8, "Coin B"					},
	{0x10, 0x01, 0x38, 0x30, "2 Coins 1 Credits"		},
	{0x10, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x10, 0x01, 0x38, 0x18, "4 Coins 5 Credits"		},
	{0x10, 0x01, 0x38, 0x20, "3 Coins 4 Credits"		},
	{0x10, 0x01, 0x38, 0x28, "2 Coins 3 Credits"		},
	{0x10, 0x01, 0x38, 0x00, "1 Coin  2 Credits"		},
	{0x10, 0x01, 0x38, 0x08, "1 Coin  3 Credits"		},
	{0x10, 0x01, 0x38, 0x10, "1 Coin  5 Credits"		},

	{0   , 0xfe, 0   ,    2, "Free Play"				},
	{0x10, 0x01, 0x40, 0x00, "Off"						},
	{0x10, 0x01, 0x40, 0x40, "On"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x10, 0x01, 0x80, 0x00, "Upright"					},
	{0x10, 0x01, 0x80, 0x80, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x11, 0x01, 0x80, 0x80, "Off"				},
	{0x11, 0x01, 0x80, 0x00, "On"				},
};

STDDIPINFO(Omegrace)

static void __fastcall omegrace_main_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x0a:
			avgdvg_reset();
		return;

		case 0x13:
			// omegrace_leds_w (don't bother)
		return;

		case 0x14:
			soundlatch = data;
			ZetClose();
			ZetOpen(1);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
			ZetClose();
			ZetOpen(0);
		return;
	}
}

static inline UINT8 spinner1_read()
{
	const UINT8 spinnerTable[0x40] = {
		0x00, 0x04, 0x14, 0x10, 0x18, 0x1c, 0x5c, 0x58,
		0x50, 0x54, 0x44, 0x40, 0x48, 0x4c, 0x6c, 0x68,
		0x60, 0x64, 0x74, 0x70, 0x78, 0x7c, 0xfc, 0xf8,
		0xf0, 0xf4, 0xe4, 0xe0, 0xe8, 0xec, 0xcc, 0xc8,
		0xc0, 0xc4, 0xd4, 0xd0, 0xd8, 0xdc, 0x9c, 0x98,
		0x90, 0x94, 0x84, 0x80, 0x88, 0x8c, 0xac, 0xa8,
		0xa0, 0xa4, 0xb4, 0xb0, 0xb8, 0xbc, 0x3c, 0x38,
		0x30, 0x34, 0x24, 0x20, 0x28, 0x2c, 0x0c, 0x08
	};

	return spinnerTable[DrvPaddle[0] & 0x3f];
}

static UINT8 __fastcall omegrace_main_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x08:
			avgdvg_go();
			avgletsgo = 1;
			return 0;

		case 0x09:
			return BurnWatchdogRead();

		case 0x0b:
			return (avgdvg_done() ? 0 : 0x80);

		case 0x10:
			return DrvDips[0];

		case 0x17:
			return DrvDips[1];

		case 0x11:
			return (DrvInputs[0] & ~0x80) | (DrvDips[2] & 0x80);

		case 0x12:
			return DrvInputs[1] ^ 0xcc;

		case 0x15:
			return spinner1_read();

		case 0x16: DrvPaddle[1];
	}

	return 0;
}

static void __fastcall omegrace_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
			AY8910Write((port/2)&1, port & 1, data);
		return;
	}
}

static UINT8 __fastcall omegrace_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
			return soundlatch;
	}

	return 0;
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	ZetOpen(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	ZetClose();

	avgdvg_reset();

	BurnWatchdogReset();

	AY8910Reset(0);
	AY8910Reset(1);

	soundlatch = 0;
	avgletsgo = 0;
	DrvPaddle[0] = DrvPaddle[1] = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x004000;
	DrvZ80ROM1		= Next; Next += 0x000800;

	DrvVidPROM		= Next; Next += 0x000100;

	DrvPalette		= (UINT32*)Next; Next += 0x8000 * sizeof(UINT32);

	DrvNVRAM		= Next; Next += 0x000100;

	AllRam			= Next;

	DrvZ80RAM0		= Next; Next += 0x000c00;
	DrvZ80RAM1		= Next; Next += 0x000400;

	// these need to overlap for the avgdvg device! leave them alone!
	DrvVectorRAM	= Next; Next += 0x001000;
	RamEnd			= Next;
	DrvVectorROM	= Next; Next += 0x001000;

	MemEnd			= Next;

	return 0;
}

static void DrvPROMDescramble()
{
	for (INT32 i = 0; i < 0x100; i++)
	{
		DrvVidPROM[i] = (DrvVidPROM[i] & 0xf0) | ((DrvVidPROM[i] & 3) << 2) | ((DrvVidPROM[i] >> 2) & 3);
	}
}

static INT32 DrvInit()
{
	BurnSetRefreshRate(40.00);

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0   + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0   + 0x1000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0   + 0x2000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0   + 0x3000,  3, 1)) return 1;

		if (BurnLoadRom(DrvVectorROM + 0x0000,  4, 1)) return 1;
		if (BurnLoadRom(DrvVectorROM + 0x0800,  5, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1   + 0x0000,  6, 1)) return 1;

		if (BurnLoadRom(DrvVidPROM   + 0x0000,  7, 1)) return 1;

		DrvPROMDescramble();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,		0x4000, 0x4bff, MAP_RAM);
	ZetMapMemory(DrvNVRAM,			0x5c00, 0x5cff, MAP_RAM);
	ZetMapMemory(DrvVectorRAM,		0x8000, 0x8fff, MAP_RAM);
	ZetMapMemory(DrvVectorROM,		0x9000, 0x9fff, MAP_ROM);
	ZetSetOutHandler(omegrace_main_write_port);
	ZetSetInHandler(omegrace_main_read_port);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,		0x0000, 0x07ff, MAP_ROM);
	ZetMapMemory(DrvZ80ROM1,		0x0800, 0x0fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,		0x1000, 0x13ff, MAP_RAM);
	ZetSetOutHandler(omegrace_sound_write_port);
	ZetSetInHandler(omegrace_sound_read_port);
	ZetClose();

	BurnWatchdogInit(DrvDoReset, 120);

	AY8910Init(0, 1000000, 0);
	AY8910Init(1, 1000000, 1);
	AY8910SetAllRoutes(0, 0.25, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.25, BURN_SND_ROUTE_BOTH);

	avgdvg_init(USE_DVG, DrvVectorRAM, 0x2000, ZetTotalCycles, 1044, 1044);
	vector_set_offsets(11, 0);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	ZetExit();
	AY8910Exit(0);
	AY8910Exit(1);
	avgdvg_exit();

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	UINT32 colors[2] = { 0x000000, 0xffffff };

    for (INT32 i = 0; i < 0x2; i++) // color
	{
		for (INT32 j = 0; j < 256; j++) // intensity
		{
			int r = (colors[i] >> 16) & 0xff;
			int g = (colors[i] >> 8) & 0xff;
			int b = (colors[i] >> 0) & 0xff;

			r = (r * j) / 255;
			g = (g * j) / 255;
			b = (b * j) / 255;

			DrvPalette[i * 256 + j] = (r << 16) | (g << 8) | b; // must be 32bit palette! -dink (see vector.cpp)
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	draw_vector(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	{
		DrvInputs[0] = 0xff;
		DrvInputs[1] = 0xff;
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}

		if (DrvJoy3[0]) DrvPaddle[0] -= 1;
		if (DrvJoy3[1]) DrvPaddle[0] += 1;

		if (DrvJoy3[2]) DrvPaddle[1] -= 1;
		if (DrvJoy3[3]) DrvPaddle[1] += 1;

		if (DrvPaddle[0] > 0x3f) DrvPaddle[0] = 0x00;
		if (DrvPaddle[0] < 0x00) DrvPaddle[0] = 0x3f;
		if (DrvPaddle[1] > 0x3f) DrvPaddle[1] = 0x00;
		if (DrvPaddle[1] < 0x00) DrvPaddle[1] = 0x3f;
	}

	INT32 nInterleave = 60;
	INT32 nCyclesTotal[2] = { 3000000 / 40, 1500000 / 40 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);
		if ((i % 10) == 9) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD); // 6x (really 6.25 per frame
		ZetClose();

		ZetOpen(1);
		nCyclesDone[1] += ZetRun(nCyclesTotal[1] / nInterleave);
		if ((i % 10) == 9) ZetNmi(); // 6x (really 6.25 per frame
		ZetClose();
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

		ZetScan(nAction);

		avgdvg_scan(nAction, pnMin);
		AY8910Scan(nAction, pnMin);
		BurnWatchdogScan(nAction);

		SCAN_VAR(DrvPaddle);
		SCAN_VAR(soundlatch);
		SCAN_VAR(avgletsgo);
	}

	if (nAction & ACB_NVRAM) {
		ba.Data		= DrvNVRAM;
		ba.nLen		= 0x00100;
		ba.nAddress	= 0;
		ba.szName	= "NV RAM";
		BurnAcb(&ba);
	}

	return 0;
}


// Omega Race (set 1)

static struct BurnRomInfo omegraceRomDesc[] = {
	{ "omega.m7",					0x1000, 0x0424d46e, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "omega.l7",					0x1000, 0xedcd7a7d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "omega.k7",					0x1000, 0x6d10f197, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "omega.j7",					0x1000, 0x8e8d4b54, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "omega.e1",					0x0800, 0x1d0fdf3a, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "omega.f1",					0x0800, 0xd44c0814, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "sound.k5",					0x0800, 0x7d426017, 2 | BRF_PRG | BRF_ESS }, //  6 Z80 #1 Code

	{ "dvgprom.bin",				0x0100, 0xd481e958, 3 | BRF_GRA },           //  7 Video PROM
};

STD_ROM_PICK(omegrace)
STD_ROM_FN(omegrace)

struct BurnDriver BurnDrvOmegrace = {
	"omegrace", NULL, NULL, NULL, "1981",
	"Omega Race (set 1)\0", NULL, "Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, omegraceRomInfo, omegraceRomName, NULL, NULL, NULL, NULL, OmegraceInputInfo, OmegraceDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	800, 800, 4, 3
};


// Omega Race (set 2)

static struct BurnRomInfo omegrace2RomDesc[] = {
	{ "o.r._1a.m7",					0x1000, 0xf8539d46, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "o.r._2a.l7",					0x1000, 0x0ff70783, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "o.r._3a.k7",					0x1000, 0x6349130d, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "o.r._4a.j7",					0x1000, 0x0a5ef64a, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "o.r._vector_i_6-1-81.e1",	0x0800, 0x1d0fdf3a, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "o.r._vector_ii_6-1-81.f1",	0x0800, 0xd44c0814, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "o.r.r._audio_6-1-81.k5",		0x0800, 0x7d426017, 2 | BRF_PRG | BRF_ESS }, //  6 Z80 #1 Code
		
	{ "dvgprom.bin",				0x0100, 0xd481e958, 3 | BRF_GRA },           //  7 Video PROM
};

STD_ROM_PICK(omegrace2)
STD_ROM_FN(omegrace2)

struct BurnDriver BurnDrvOmegrace2 = {
	"omegrace2", "omegrace", NULL, NULL, "1981",
	"Omega Race (set 2)\0", NULL, "Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, omegrace2RomInfo, omegrace2RomName, NULL, NULL, NULL, NULL, OmegraceInputInfo, OmegraceDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	800, 800, 4, 3
};


// Delta Race

static struct BurnRomInfo deltraceRomDesc[] = {
	{ "omega.m7",					0x1000, 0x0424d46e, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "omega.l7",					0x1000, 0xedcd7a7d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "omega.k7",					0x1000, 0x6d10f197, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "delta.j7",					0x1000, 0x8ef9541e, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "omega.e1",					0x0800, 0x1d0fdf3a, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "omega.f1",					0x0800, 0xd44c0814, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "sound.k5",					0x0800, 0x7d426017, 2 | BRF_PRG | BRF_ESS }, //  6 Z80 #1 Code

	{ "dvgprom.bin",				0x0100, 0xd481e958, 3 | BRF_GRA },           //  7 Video PROM
};

STD_ROM_PICK(deltrace)
STD_ROM_FN(deltrace)

struct BurnDriver BurnDrvDeltrace = {
	"deltrace", "omegrace", NULL, NULL, "1981",
	"Delta Race\0", NULL, "bootleg (Allied Leisure)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, deltraceRomInfo, deltraceRomName, NULL, NULL, NULL, NULL, OmegraceInputInfo, OmegraceDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	800, 800, 4, 3
};
