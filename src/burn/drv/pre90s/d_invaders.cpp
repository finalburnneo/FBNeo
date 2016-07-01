// FB Alpha Space Invaders driver module
// Based on MAME driver by Michael Strutts, Nicola Salmoria, Tormod Tjaberg, Mirko Buffoni,
// Lee Taylor, Valerio Verrando, Marco Cassili, Zsolt Vasvari and others

#include "tiles_generic.h"
#include "z80_intf.h"
#include "samples.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvI8080ROM;
static UINT8 *DrvMainRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 *prev_snd_data;

static UINT16 shift_data;
static UINT8 shift_count;
static INT32 watchdog;

static UINT8  DrvJoy1[8];
static UINT8  DrvJoy2[8];
static UINT8  DrvJoy3[8];
static UINT8  DrvDips[1];
static UINT8  DrvInputs[3];
static UINT8  DrvReset;
static UINT32 inputxor=0;

static struct BurnInputInfo InvadersInputList[] = {
	{"Coin",	BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},

	{"P1 Start",	BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"	},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Left",	BIT_DIGITAL,	DrvJoy2 + 5,	"p1 left"	},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy2 + 6,	"p1 right"	},

	{"P2 Start",	BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	{"P2 Button 1",	BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Left",	BIT_DIGITAL,	DrvJoy3 + 5,	"p2 left"	},
	{"P2 Right",	BIT_DIGITAL,	DrvJoy3 + 6,	"p2 right"	},

	{"Tilt",        BIT_DIGITAL,	DrvJoy3 + 2,	"tilt"		},
	{"Reset",	BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dips",	BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Invaders)

static struct BurnDIPInfo InvadersDIPList[]=
{
	{0x0b, 0xff, 0xff, 0x00, NULL           },

	{0   , 0xfe, 0   , 4   , "Lives"        },
	{0x0b, 0x01, 0x03, 0x00, "3"       	},
	{0x0b, 0x01, 0x03, 0x01, "4"       	},
	{0x0b, 0x01, 0x03, 0x02, "5"       	},
	{0x0b, 0x01, 0x03, 0x03, "6"       	},

	{0   , 0xfe, 0   , 2   , "Bonus Life"   },
	{0x0b, 0x01, 0x0c, 0x00, "1000"   	},
	{0x0b, 0x01, 0x0c, 0x08, "5000"     	},

	{0   , 0xfe, 0   , 2   , "Coin Info" 	},
	{0x0b, 0x01, 0x80, 0x80, "Off"       	},
	{0x0b, 0x01, 0x80, 0x00, "On"       	},
};

STDDIPINFO(Invaders)

static struct BurnInputInfo SitvInputList[] = {
	{"Coin",	BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},

	{"P1 Start",	BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"	},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Left",	BIT_DIGITAL,	DrvJoy2 + 5,	"p1 left"	},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy2 + 6,	"p1 right"	},

	{"P2 Start",	BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	{"P2 Button 1",	BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Left",	BIT_DIGITAL,	DrvJoy3 + 5,	"p2 left"	},
	{"P2 Right",	BIT_DIGITAL,	DrvJoy3 + 6,	"p2 right"	},

	{"Service",     BIT_DIGITAL,	DrvJoy1 + 0,	"service"	},
	{"Tilt",        BIT_DIGITAL,	DrvJoy3 + 2,	"tilt"		},
	{"Reset",	BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dips",	BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Sitv)

static struct BurnDIPInfo SitvDIPList[]=
{
	{0x0c, 0xff, 0xff, 0x00, NULL           },

	{0   , 0xfe, 0   , 4   , "Lives"        },
	{0x0c, 0x01, 0x03, 0x00, "3"       	},
	{0x0c, 0x01, 0x03, 0x01, "4"       	},
	{0x0c, 0x01, 0x03, 0x02, "5"       	},
	{0x0c, 0x01, 0x03, 0x03, "6"       	},

	{0   , 0xfe, 0   , 2   , "Bonus Life"   },
	{0x0c, 0x01, 0x0c, 0x00, "1000"   	},
	{0x0c, 0x01, 0x0c, 0x08, "1500"     	},

	{0   , 0xfe, 0   , 2   , "Coin Info" 	},
	{0x0c, 0x01, 0x80, 0x80, "Off"       	},
	{0x0c, 0x01, 0x80, 0x00, "On"       	},
};

STDDIPINFO(Sitv)

static struct BurnDIPInfo OzmawarsDIPList[]=
{
	{0x0b, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    4, "Energy"		},
	{0x0b, 0x01, 0x03, 0x00, "15000"		},
	{0x0b, 0x01, 0x03, 0x01, "20000"		},
	{0x0b, 0x01, 0x03, 0x02, "25000"		},
	{0x0b, 0x01, 0x03, 0x03, "35000"		},

	{0   , 0xfe, 0   ,    2, "Bonus Energy"		},
	{0x0b, 0x01, 0x08, 0x00, "15000"		},
	{0x0b, 0x01, 0x08, 0x08, "10000"		},

	{0   , 0xfe, 0   ,    2, "Coinage"		},
	{0x0b, 0x01, 0x80, 0x00, "1 Coin  1 Credits"	},
	{0x0b, 0x01, 0x80, 0x80, "1 Coin  2 Credits"	},
};

STDDIPINFO(Ozmawars)

static void invaders_sh_1_write(UINT8 data, UINT8 *last)
{
	if ( data & 0x01 && ~*last & 0x01) BurnSamplePlay(9);	// Ufo Sound
	if ( data & 0x02 && ~*last & 0x02) BurnSamplePlay(0);	// Shot Sound
	if ( data & 0x04 && ~*last & 0x04) BurnSamplePlay(1);	// Base Hit
	if (~data & 0x04 &&  *last & 0x04) BurnSampleStop(1);
	if ( data & 0x08 && ~*last & 0x08) BurnSamplePlay(2);	// Invader Hit
	if ( data & 0x10 && ~*last & 0x10) BurnSamplePlay(8);	// Bonus Missle Base

	*last = data;
}

static void invaders_sh_2_write(UINT8 data, UINT8 *last)
{
	if (data & 0x01 && ~*last & 0x01) BurnSamplePlay(3);	// Fleet 1
	if (data & 0x02 && ~*last & 0x02) BurnSamplePlay(4);	// Fleet 2
	if (data & 0x04 && ~*last & 0x04) BurnSamplePlay(5);	// Fleet 3
	if (data & 0x08 && ~*last & 0x08) BurnSamplePlay(6);	// Fleet 4
	if (data & 0x10 && ~*last & 0x10) BurnSamplePlay(7);	// Saucer Hit

	*last = data;
}

static void __fastcall invaders_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0x07)
	{
		case 0x02:
			shift_count = ~data & 0x07;
		return;

		case 0x03:
			invaders_sh_1_write(data, &prev_snd_data[0]);
		return;

		case 0x04:
			shift_data = (data << 7) | (shift_data >> 8);
		return;

		case 0x05:
			invaders_sh_2_write(data, &prev_snd_data[0]);
		return;

		case 0x06:
			watchdog = 0;
		return;
	}
}

static UINT8 __fastcall invaders_read_port(UINT16 port)
{
	switch (port & 0x03)
	{
		case 0x00:
			return DrvInputs[0] ^ (inputxor & 0xff);

		case 0x01:
			return DrvInputs[1] ^ ((inputxor >> 8) & 0xff);

		case 0x02:
			return ((DrvInputs[2] ^ ((inputxor >> 16) & 0xff)) & 0x74) | (DrvDips[0] & 0x8b);

		case 0x03:
			return (shift_data >> shift_count) & 0xff;
	}

	return 0;
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset(AllRam, 0, RamEnd - AllRam);
	}

	ZetOpen(0);
	ZetReset();
	ZetClose();

	BurnSampleReset();

	watchdog = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvI8080ROM		= Next; Next += 0x006000;

	DrvPalette		= (UINT32*)Next; Next += 0x0002 * sizeof(UINT32);

	AllRam			= Next;

	DrvMainRAM		= Next; Next += 0x002000;

	prev_snd_data		= Next; Next += 0x000002;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvInit(INT32 rom_size, INT32 rom_count, UINT32 input_xor)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		INT32 load_offset = 0;
		for (INT32 i = 0; i < rom_count; i++) {
			if (BurnLoadRom(DrvI8080ROM + load_offset, i, 1)) return 1;
			load_offset += rom_size;
			if (load_offset == 0x0c00 && rom_size == 0x400 && rom_count == 6) load_offset = 0x1400;
			if (load_offset == 0x2000) load_offset = 0x4000;
		}
	}

	ZetInit(0); // really i8080
	ZetOpen(0);
	ZetMapMemory(DrvI8080ROM,		0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvMainRAM,		0x2000, 0x3fff, MAP_RAM);
	ZetMapMemory(DrvI8080ROM + 0x4000,	0x4000, 0x5fff, MAP_ROM);
	ZetMapMemory(DrvMainRAM,		0x6000, 0x7fff, MAP_RAM); // mirror
	ZetMapMemory(DrvI8080ROM,		0x8000, 0x9fff, MAP_ROM); // mirror
	ZetMapMemory(DrvMainRAM,		0xa000, 0xbfff, MAP_RAM); // mirror
	ZetMapMemory(DrvI8080ROM + 0x4000,	0xc000, 0xdfff, MAP_ROM); // mirror
	ZetMapMemory(DrvMainRAM,		0xe000, 0xffff, MAP_RAM); // mirror
	ZetSetOutHandler(invaders_write_port);
	ZetSetInHandler(invaders_read_port);
	ZetClose();

	BurnSampleInit(0);
	BurnSampleSetAllRoutesAllSamples(0.25, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	inputxor = input_xor;

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	BurnSampleExit();

	BurnFree (AllMem);

	return 0;
}

static void draw_invaders_bitmap()
{
	UINT8 x = 0;
	UINT8 y = 0x20;
	UINT8 video_data = 0;

	while (1)
	{
		pTransDraw[((y - 0x20) * nScreenWidth) + x] = (video_data & 0x01);

		video_data >>= 1;
		x++;

		if (x == 0)
		{
			for (INT32 i = 0; i < 4; i++)
			{
				pTransDraw[((y - 0x20) * nScreenWidth) + (256 + i)] = (video_data & 0x01);
				video_data >>= 1;
			}

			y++;

			if (y == 0) break;
		}
		else if ((x & 0x07) == 0x04)
		{
			video_data = DrvMainRAM[((y << 5) | (x >> 3)) & 0x1fff];
		}
	}
}

static INT32 DrvDraw()
{
	DrvPalette[0] = BurnHighCol(0x00,0x00,0x00,0);
	DrvPalette[1] = BurnHighCol(0xff,0xff,0xff,0);

	draw_invaders_bitmap();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	watchdog++;
	if (watchdog >= 180) {
		DrvDoReset(0);
	}

	if (DrvReset) {
		DrvDoReset(1);
	}

	{
		memset (DrvInputs, 0, 3);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[1] = { 1996800 / 60 };
	INT32 nCyclesDone[1] = { 0 };

	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		INT32 nSegment = nCyclesTotal[0] / nInterleave;

		nCyclesDone[0] += ZetRun(nSegment);
		if (i == 96) {
			ZetSetVector(0xd7);
			ZetSetIRQLine(0, CPU_IRQSTATUS_AUTO);
		}
		if (i == 224) {
			ZetSetVector(0xcf);
			ZetSetIRQLine(0, CPU_IRQSTATUS_AUTO);	
		}
	}

	ZetClose();

	if (pBurnSoundOut) {
		BurnSampleRender(pBurnSoundOut, nBurnSoundLen);
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
		*pnMin = 0x029695;
	}

	if (nAction & ACB_VOLATILE) {		
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);

		SCAN_VAR(shift_data);
		SCAN_VAR(shift_count);
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


static struct BurnSampleInfo OzmawarsSampleDesc[] = {
	{ "1", SAMPLE_NOLOOP },	// Shot/Missle
	{ "2", SAMPLE_NOLOOP },	// Base Hit/Explosion
	{ "3", SAMPLE_NOLOOP },	// Invader Hit
	{ "4", SAMPLE_NOLOOP },	// Fleet move 1
	{ "5", SAMPLE_NOLOOP },	// Fleet move 2
	{ "6", SAMPLE_NOLOOP },	// Fleet move 3
	{ "7", SAMPLE_NOLOOP },	// Fleet move 4
	{ "8", SAMPLE_NOLOOP },	// UFO/Saucer Hit
	{ "9", SAMPLE_NOLOOP },	// Bonus Base
	{ "", 0 }
};

STD_SAMPLE_PICK(Ozmawars)
STD_SAMPLE_FN(Ozmawars)


// Space Invaders / Space Invaders M

static struct BurnRomInfo invadersRomDesc[] = {
	{ "invaders.h",		0x0800, 0x734f5ad8, 1 | BRF_ESS | BRF_PRG }, //  0 i8080 code
	{ "invaders.g",		0x0800, 0x6bfaca4a, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "invaders.f",		0x0800, 0x0ccead96, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "invaders.e",		0x0800, 0x14e538b0, 1 | BRF_ESS | BRF_PRG }, //  3
};

STD_ROM_PICK(invaders)
STD_ROM_FN(invaders)

static INT32 InvadersInit()
{
	return DrvInit(0x800, 4, 0x000100);
}

struct BurnDriver BurnDrvInvaders = {
	"invaders", NULL, NULL, "invaders", "1978",
	"Space Invaders / Space Invaders M\0", NULL, "Taito / Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, invadersRomInfo, invadersRomName, InvadersSampleInfo, InvadersSampleName, InvadersInputInfo, InvadersDIPInfo,
	InvadersInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x02,
	224, 260, 3, 4
};


// Space Invaders (SV Version rev 1)

static struct BurnRomInfo sisv1RomDesc[] = {
	{ "sv01.36",		0x0400, 0xd0c32d72, 1 | BRF_ESS | BRF_PRG }, //  0 i8080 Code
	{ "sv02.35",		0x0400, 0x0e159534, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "sv03.34",		0x0400, 0x00000000, 1 | BRF_NODUMP | BRF_ESS | BRF_PRG }, //  2
	{ "sv04.31",		0x0400, 0x1293b826, 1 | BRF_ESS | BRF_PRG }, //  3
	{ "sv05.42",		0x0400, 0x00000000, 1 | BRF_NODUMP | BRF_ESS | BRF_PRG }, //  4
	{ "sv06.41",		0x0400, 0x2c68e0b4, 1 | BRF_ESS | BRF_PRG }, //  5
};

STD_ROM_PICK(sisv1)
STD_ROM_FN(sisv1)

static INT32 Sisv1Init()
{
	return DrvInit(0x400, 6, 0x000100);
}

struct BurnDriver BurnDrvSisv1 = {
	"sisv1", "invaders", NULL, "invaders", "1978",
	"Space Invaders (SV Version rev 1)\0", NULL, "Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, sisv1RomInfo, sisv1RomName, InvadersSampleInfo, InvadersSampleName, InvadersInputInfo, InvadersDIPInfo,
	Sisv1Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 2,
	224, 260, 3, 4
};


// Space Invaders (SV Version rev 2)

static struct BurnRomInfo sisv2RomDesc[] = {
	{ "sv01.36",		0x0400, 0xd0c32d72, 1 | BRF_ESS | BRF_PRG }, //  0 i8080 Code
	{ "sv02.35",		0x0400, 0x0e159534, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "sv10.34",		0x0400, 0x483e651e, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "sv04.31",		0x0400, 0x1293b826, 1 | BRF_ESS | BRF_PRG }, //  3
	{ "sv09.42",		0x0400, 0xcd80b13f, 1 | BRF_ESS | BRF_PRG }, //  4
	{ "sv06.41",		0x0400, 0x2c68e0b4, 1 | BRF_ESS | BRF_PRG }, //  5
};

STD_ROM_PICK(sisv2)
STD_ROM_FN(sisv2)

struct BurnDriver BurnDrvSisv2 = {
	"sisv2", "invaders", NULL, "invaders", "1978",
	"Space Invaders (SV Version rev 2)\0", NULL, "Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, sisv2RomInfo, sisv2RomName, InvadersSampleInfo, InvadersSampleName, InvadersInputInfo, InvadersDIPInfo,
	Sisv1Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 2,
	224, 260, 3, 4
};


// Space Invaders (SV Version rev 3)

static struct BurnRomInfo sisv3RomDesc[] = {
	{ "sv0h.36",		0x0400, 0x86bb8cb6, 1 | BRF_ESS | BRF_PRG }, //  0 i8080 Code
	{ "sv02.35",		0x0400, 0x0e159534, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "sv10.34",		0x0400, 0x483e651e, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "sv04.31",		0x0400, 0x1293b826, 1 | BRF_ESS | BRF_PRG }, //  3
	{ "sv09.42",		0x0400, 0xcd80b13f, 1 | BRF_ESS | BRF_PRG }, //  4
	{ "sv06.41",		0x0400, 0x2c68e0b4, 1 | BRF_ESS | BRF_PRG }, //  5
};

STD_ROM_PICK(sisv3)
STD_ROM_FN(sisv3)

struct BurnDriver BurnDrvSisv3 = {
	"sisv3", "invaders", NULL, "invaders", "1978",
	"Space Invaders (SV Version rev 3)\0", NULL, "Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, sisv3RomInfo, sisv3RomName, InvadersSampleInfo, InvadersSampleName, InvadersInputInfo, InvadersDIPInfo,
	Sisv1Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 2,
	224, 260, 3, 4
};


// Space Invaders (SV Version rev 4)

static struct BurnRomInfo sisvRomDesc[] = {
	{ "sv0h.36",		0x0400, 0x86bb8cb6, 1 | BRF_ESS | BRF_PRG }, //  0 i8080 Code
	{ "sv11.35",		0x0400, 0xfebe6d1a, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "sv12.34",		0x0400, 0xa08e7202, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "sv04.31",		0x0400, 0x1293b826, 1 | BRF_ESS | BRF_PRG }, //  3
	{ "sv13.42",		0x0400, 0xa9011634, 1 | BRF_ESS | BRF_PRG }, //  4
	{ "sv14.41",		0x0400, 0x58730370, 1 | BRF_ESS | BRF_PRG }, //  5
};

STD_ROM_PICK(sisv)
STD_ROM_FN(sisv)

struct BurnDriver BurnDrvSisv = {
	"sisv", "invaders", NULL, "invaders", "1978",
	"Space Invaders (SV Version rev 4)\0", NULL, "Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, sisvRomInfo, sisvRomName, InvadersSampleInfo, InvadersSampleName, InvadersInputInfo, InvadersDIPInfo,
	Sisv1Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 2,
	224, 260, 3, 4
};


// Space Invaders (TV Version rev 1)

static struct BurnRomInfo sitv1RomDesc[] = {
	{ "tv01.s1",		0x0800, 0x9f37b146, 1 | BRF_ESS | BRF_PRG }, //  0 i8080 Code
	{ "tv02.rp1",		0x0800, 0x3c759a90, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "tv03.n1",		0x0800, 0x0ad3657f, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "tv04.m1",		0x0800, 0xcd2c67f6, 1 | BRF_ESS | BRF_PRG }, //  3
};

STD_ROM_PICK(sitv1)
STD_ROM_FN(sitv1)

static INT32 Sitv1Init()
{
	return DrvInit(0x800, 4, 0x000101);
}

struct BurnDriver BurnDrvSitv1 = {
	"sitv1", "invaders", NULL, "invaders", "1978",
	"Space Invaders (TV Version rev 1)\0", NULL, "Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, sitv1RomInfo, sitv1RomName, InvadersSampleInfo, InvadersSampleName, SitvInputInfo, SitvDIPInfo,
	Sitv1Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 2,
	224, 260, 3, 4
};


// Space Invaders (TV Version rev 2)

static struct BurnRomInfo sitvRomDesc[] = {
	{ "tv0h.s1",		0x0800, 0xfef18aad, 1 | BRF_ESS | BRF_PRG }, //  0 i8080 Code
	{ "tv02.rp1",		0x0800, 0x3c759a90, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "tv03.n1",		0x0800, 0x0ad3657f, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "tv04.m1",		0x0800, 0xcd2c67f6, 1 | BRF_ESS | BRF_PRG }, //  3
};

STD_ROM_PICK(sitv)
STD_ROM_FN(sitv)

struct BurnDriver BurnDrvSitv = {
	"sitv", "invaders", NULL, "invaders", "1978",
	"Space Invaders (TV Version rev 2)\0", NULL, "Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, sitvRomInfo, sitvRomName, InvadersSampleInfo, InvadersSampleName, SitvInputInfo, SitvDIPInfo,
	Sitv1Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 2,
	224, 260, 3, 4
};


// Ozma Wars (set 1)

static struct BurnRomInfo ozmawarsRomDesc[] = {
	{ "mw01",		0x0800, 0x31f4397d, 1 | BRF_ESS | BRF_PRG }, //  0 i8080 Code
	{ "mw02",		0x0800, 0xd8e77c62, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "mw03",		0x0800, 0x3bfa418f, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "mw04",		0x0800, 0xe190ce6c, 1 | BRF_ESS | BRF_PRG }, //  3
	{ "mw05",		0x0800, 0x3bc7d4c7, 1 | BRF_ESS | BRF_PRG }, //  4
	{ "mw06",		0x0800, 0x99ca2eae, 1 | BRF_ESS | BRF_PRG }, //  5
	
	{ "01.1",		0x0400, 0xaac24f34, 0 | BRF_OPT },
	{ "02.2",		0x0400, 0x2bdf83a0, 0 | BRF_OPT },
};

STD_ROM_PICK(ozmawars)
STD_ROM_FN(ozmawars)

static INT32 OzmawarsInit()
{
	return DrvInit(0x800, 6, 0x000000);
}

struct BurnDriver BurnDrvOzmawars = {
	"ozmawars", NULL, NULL, "invaders", "1979",
	"Ozma Wars (set 1)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, ozmawarsRomInfo, ozmawarsRomName, OzmawarsSampleInfo, OzmawarsSampleName, InvadersInputInfo, OzmawarsDIPInfo,
	OzmawarsInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 2,
	224, 260, 3, 4
};


// Ozma Wars (set 2)

static struct BurnRomInfo ozmawars2RomDesc[] = {
	{ "oz1",		0x0400, 0x9300830e, 1 | BRF_ESS | BRF_PRG }, //  0 i8080 Code
	{ "oz2",		0x0400, 0x957fc661, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "oz3",		0x0400, 0xcf8f4d6c, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "oz4",		0x0400, 0xf51544a5, 1 | BRF_ESS | BRF_PRG }, //  3
	{ "oz5",		0x0400, 0x5597bf52, 1 | BRF_ESS | BRF_PRG }, //  4
	{ "oz6",		0x0400, 0x19b43578, 1 | BRF_ESS | BRF_PRG }, //  5
	{ "oz7",		0x0400, 0xa285bfde, 1 | BRF_ESS | BRF_PRG }, //  6
	{ "oz8",		0x0400, 0xae59a629, 1 | BRF_ESS | BRF_PRG }, //  7
	{ "oz9",		0x0400, 0xdf0cc633, 1 | BRF_ESS | BRF_PRG }, //  8
	{ "oz10",		0x0400, 0x31b7692e, 1 | BRF_ESS | BRF_PRG }, //  9
	{ "oz11",		0x0400, 0x660e934c, 1 | BRF_ESS | BRF_PRG }, //  10
	{ "oz12",		0x0400, 0x8b969f61, 1 | BRF_ESS | BRF_PRG }, //  11
	
	{ "01.1",		0x0400, 0xaac24f34, 0 | BRF_OPT },
	{ "02.2",		0x0400, 0x2bdf83a0, 0 | BRF_OPT },
};

STD_ROM_PICK(ozmawars2)
STD_ROM_FN(ozmawars2)

static INT32 Ozmawars2Init()
{
	INT32 nRet = DrvInit(0x400, 12, 0x000000);

	return nRet;
}

struct BurnDriver BurnDrvOzmawars2 = {
	"ozmawars2", "ozmawars", NULL, "invaders", "1979",
	"Ozma Wars (set 2)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, ozmawars2RomInfo, ozmawars2RomName, OzmawarsSampleInfo, OzmawarsSampleName, InvadersInputInfo, OzmawarsDIPInfo,
	Ozmawars2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 2,
	224, 260, 3, 4
};
