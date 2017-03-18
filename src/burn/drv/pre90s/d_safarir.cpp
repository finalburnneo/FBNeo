// FB Alpha Safari Rally driver module
// Based on MAME driver by Zsolt Vasvari

#include "tiles_generic.h"
#include "z80_intf.h"
#include "samples.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvI8080ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvI8080RAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 nRamBank;
static UINT8 scrollx;
static UINT8 m_port_last;
static UINT8 m_port_last2;

static INT32 vblank;

static UINT8  DrvJoy1[8];
static UINT8  DrvDips[1];
static UINT8  DrvInputs[1];
static UINT8  DrvReset;

static struct BurnInputInfo SafarirInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 start"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Safarir)

static struct BurnDIPInfo SafarirDIPList[]=
{
	{0x06, 0xff, 0xff, 0x24, NULL			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x06, 0x01, 0x03, 0x00, "3"			},
	{0x06, 0x01, 0x03, 0x01, "4"			},
	{0x06, 0x01, 0x03, 0x02, "5"			},
	{0x06, 0x01, 0x03, 0x03, "6"			},

	{0   , 0xfe, 0   ,    4, "Acceleration Rate"	},
	{0x06, 0x01, 0x0c, 0x00, "Slowest"		},
	{0x06, 0x01, 0x0c, 0x04, "Slow"			},
	{0x06, 0x01, 0x0c, 0x08, "Fast"			},
	{0x06, 0x01, 0x0c, 0x0c, "Fastest"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x06, 0x01, 0x10, 0x00, "Off"			},
	{0x06, 0x01, 0x10, 0x10, "On"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x06, 0x01, 0x60, 0x00, "3000"			},
	{0x06, 0x01, 0x60, 0x20, "5000"			},
	{0x06, 0x01, 0x60, 0x40, "7000"			},
	{0x06, 0x01, 0x60, 0x60, "9000"			},
};

STDDIPINFO(Safarir)

#define SAMPLE_SOUND1_1     0
#define SAMPLE_SOUND1_2     1
#define SAMPLE_SOUND2       2
#define SAMPLE_SOUND3       3
#define SAMPLE_SOUND4_1     4
#define SAMPLE_SOUND4_2     5
#define SAMPLE_SOUND5_1     6
#define SAMPLE_SOUND5_2     7
#define SAMPLE_SOUND6       8
#define SAMPLE_SOUND7       9
#define SAMPLE_SOUND8       10

static void safarir_audio_write(UINT8 data)
{
	UINT8 rising_bits = data & ~m_port_last;

	if (rising_bits == 0x12) BurnSamplePlay(SAMPLE_SOUND1_1);
	if (rising_bits == 0x02) BurnSamplePlay(SAMPLE_SOUND1_2);
	if (rising_bits == 0x95) BurnSamplePlay(SAMPLE_SOUND6);

	if (rising_bits == 0x04 && (data == 0x15 || data ==0x16)) BurnSamplePlay(SAMPLE_SOUND2);

	if (data == 0x5f && (rising_bits == 0x49 || rising_bits == 0x5f)) BurnSamplePlay(SAMPLE_SOUND3);
	if (data == 0x00 || rising_bits == 0x01) BurnSampleStop(SAMPLE_SOUND3);

	if (data == 0x13)
	{
		if ((rising_bits == 0x13 && m_port_last != 0x04) || (rising_bits == 0x01 && m_port_last == 0x12))
		{
			BurnSamplePlay(SAMPLE_SOUND7);
		}
		else if (rising_bits == 0x03 && m_port_last2 == 0x15 && !BurnSampleGetStatus(SAMPLE_SOUND4_1))
		{
			BurnSamplePlay(SAMPLE_SOUND4_1);
		}
	}
	if (data == 0x53 && m_port_last == 0x55) BurnSamplePlay(SAMPLE_SOUND4_2);

	if (data == 0x1f && rising_bits == 0x1f) BurnSamplePlay(SAMPLE_SOUND5_1);
	if (data == 0x14 && (rising_bits == 0x14 || rising_bits == 0x04)) BurnSamplePlay(SAMPLE_SOUND5_2);

	if (data == 0x07 && rising_bits == 0x07 && !BurnSampleGetStatus(SAMPLE_SOUND8))
		BurnSamplePlay(SAMPLE_SOUND8);

	m_port_last2 = m_port_last;
	m_port_last = data;
}

static void ram_bank(INT32 data)
{
	nRamBank = data & 0x01;
	ZetMapMemory(DrvI8080RAM + (nRamBank * 0x800), 0x2000, 0x27ff, MAP_RAM);
}

static void __fastcall safarir_write(UINT16 address, UINT8 data)
{
	switch (address & ~0x03ff)
	{
		case 0x2800:
			ram_bank(data);
		return;

		case 0x2c00:
			scrollx = data;

		case 0x3000:
			safarir_audio_write(data);
		return;

		case 0x3400:
		case 0x3800:
		case 0x3c00:
		return;	// nop
	}
}

static UINT8 __fastcall safarir_read(UINT16 address)
{
	switch (address & ~0x03ff)
	{
		case 0x3800:
			return DrvInputs[0];

		case 0x3c00:
			return (DrvDips[0] & 0x7f) | vblank;
	}

	return 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ram_bank(0);
	ZetClose();

	BurnSampleReset();

	scrollx = 0;
	m_port_last = 0;
	m_port_last2 = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvI8080ROM		= Next; Next += 0x001800;

	DrvGfxROM0		= Next; Next += 0x002000;
	DrvGfxROM1		= Next; Next += 0x002000;

	DrvPalette		= (UINT32*)Next; Next += 0x0010 * sizeof(UINT32);

	AllRam			= Next;

	DrvI8080RAM		= Next; Next += 0x001000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[1] = { 0 };
	INT32 XOffs[8] = { STEP8(7,-1) };
	INT32 YOffs[8] = { STEP8(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x400);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x400);

	GfxDecode(0x80, 1, 8, 8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x400);

	GfxDecode(0x80, 1, 8, 8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM1);

	BurnFree(tmp);

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
		if (BurnLoadRom(DrvI8080ROM + 0x0000, 0, 1)) return 1;
		if (BurnLoadRom(DrvI8080ROM + 0x0400, 1, 1)) return 1;
		if (BurnLoadRom(DrvI8080ROM + 0x0800, 2, 1)) return 1;
		if (BurnLoadRom(DrvI8080ROM + 0x0c00, 3, 1)) return 1;
		if (BurnLoadRom(DrvI8080ROM + 0x1000, 4, 1)) return 1;
		if (BurnLoadRom(DrvI8080ROM + 0x1400, 5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0  + 0x0000, 6, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1  + 0x0000, 7, 1)) return 1;

		DrvGfxDecode();
	}	

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvI8080ROM,	0x0000, 0x17ff, MAP_ROM);
	ZetMapMemory(DrvI8080RAM,	0x2000, 0x27ff, MAP_RAM);
	ZetSetWriteHandler(safarir_write);
	ZetSetReadHandler(safarir_read);
	ZetClose();

	BurnSampleInit(0);
	BurnSampleSetAllRoutesAllSamples(0.50, BURN_SND_ROUTE_BOTH);

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

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x10; i+=2)
	{
		DrvPalette[i + 0] = BurnHighCol(0, 0, 0, 0); // black
		DrvPalette[i + 1] = BurnHighCol((i & 8) ? 0xff : 0, (i & 4) ? 0xff : 0, (i & 2) ? 0xff : 0, 0);
	}
}

static void draw_layer_bg()
{
	UINT8 *ram = DrvI8080RAM + (nRamBank ? 0x800 : 0) + 0x400;

	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sx = (offs & 0x1f) * 8;
		INT32 sy = (offs / 0x20) * 8;

		sx -= scrollx;
		if (sx < -7) sx += 256;

		INT32 code = ram[offs];

		INT32 color;
		if (code & 0x80) {
			color = 6;
		} else {
			color = ((~offs & 0x04) >> 2) | ((offs & 0x04) >> 1);
			if (offs & 0x100) {
				color |= ((offs & 0xc0) != 0x00) ? 1 : 0;
			} else {
				color |= ((code & 0xc0) == 0x80) ? 1 : 0;
			}
		}

		Render8x8Tile_Clip(pTransDraw, code & 0x7f, sx, sy, color, 1, 0, DrvGfxROM0);
	}
}

static void draw_layer_fg()
{
	UINT8 *ram = DrvI8080RAM + (nRamBank ? 0x800 : 0);

	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sx = (offs & 0x1f) * 8;
		INT32 sy = (offs / 0x20) * 8;

		INT32 code = ram[offs];

		INT32 color;
		if (code & 0x80) {
			color = 7;
		} else {
			//color = ((~offs & 0x04) >> 2) | ((offs & 0x06) >> 1); bad iq! :)
			color = (~offs & 0x04) | ((offs >> 1) & 0x03);
		}

		INT32 opaque = ((offs & 0x1f) >= 0x03) ? 0 : 1;

		if (opaque) {
			Render8x8Tile_Clip(pTransDraw, code & 0x7f, sx, sy, color, 1, 0, DrvGfxROM1);
		} else {
			Render8x8Tile_Mask_Clip(pTransDraw, code & 0x7f, sx, sy, color, 1, 0, 0, DrvGfxROM1);
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	draw_layer_bg();
	draw_layer_fg();

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
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;

	ZetOpen(0);

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetRun(1500000 / 60 / nInterleave);

		if (i == 240) vblank = 0x80;
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
	
	if (pnMin != NULL) {
		*pnMin = 0x029708;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}
	
	if (nAction & ACB_DRIVER_DATA) {
		ZetScan(nAction);
		BurnSampleScan(nAction, pnMin);

		SCAN_VAR(nRamBank);
		SCAN_VAR(scrollx);
		SCAN_VAR(m_port_last);
		SCAN_VAR(m_port_last2);
	}
	
	return 0;
}

static struct BurnSampleInfo SafarirSampleDesc[] = {
	{ "sound1-1", SAMPLE_NOLOOP },
	{ "sound1-2", SAMPLE_NOLOOP },
	{ "sound2",   SAMPLE_NOLOOP },
	{ "sound3",   SAMPLE_NOLOOP },
	{ "sound4-1", SAMPLE_NOLOOP },
	{ "sound4-2", SAMPLE_NOLOOP },
	{ "sound5-1", SAMPLE_NOLOOP },
	{ "sound5-2", SAMPLE_NOLOOP },
	{ "sound6",   SAMPLE_NOLOOP },
	{ "sound7",   SAMPLE_NOLOOP },
	{ "sound8",   SAMPLE_NOLOOP },
	{ "", 0 }
};

STD_SAMPLE_PICK(Safarir)
STD_SAMPLE_FN(Safarir)

// Safari Rally (World)

static struct BurnRomInfo safarirRomDesc[] = {
	{ "rl-01.9",	0x0400, 0xcf7703c9, 1 | BRF_PRG | BRF_ESS }, //  0 I8080 Code
	{ "rl-02.1",	0x0400, 0x1013ecd3, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rl-03.10",	0x0400, 0x84545894, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rl-04.2",	0x0400, 0x5dd12f96, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "rl-09.11",	0x0400, 0xd066b382, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "rl-06.3",	0x0400, 0x24c1cd42, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "rl-10.43",	0x0400, 0xc04466c6, 2 | BRF_GRA },           //  6 Background Tiles

	{ "rl-07.40",	0x0400, 0xba525203, 3 | BRF_GRA },           //  7 Foreground Tiles
};

STD_ROM_PICK(safarir)
STD_ROM_FN(safarir)

struct BurnDriver BurnDrvSafarir = {
	"safarir", NULL, NULL, "safarir", "1979",
	"Safari Rally (World)\0", NULL, "SNK (Taito license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, safarirRomInfo, safarirRomName, SafarirSampleInfo, SafarirSampleName, SafarirInputInfo, SafarirDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	208, 256, 3, 4
};


// Safari Rally (Japan)

static struct BurnRomInfo safarirjRomDesc[] = {
	{ "rl-01.9",	0x0400, 0xcf7703c9, 1 | BRF_PRG | BRF_ESS }, //  0 I8080 Code
	{ "rl-02.1",	0x0400, 0x1013ecd3, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rl-03.10",	0x0400, 0x84545894, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rl-04.2",	0x0400, 0x5dd12f96, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "rl-05.11",	0x0400, 0x935ed469, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "rl-06.3",	0x0400, 0x24c1cd42, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "rl-08.43",	0x0400, 0xd6a50aac, 2 | BRF_GRA },           //  6 Background Tiles

	{ "rl-07.40",	0x0400, 0xba525203, 3 | BRF_GRA },           //  7 Foreground Tiles
};

STD_ROM_PICK(safarirj)
STD_ROM_FN(safarirj)

struct BurnDriver BurnDrvSafarirj = {
	"safarirj", "safarir", NULL, "safarir", "1979",
	"Safari Rally (Japan)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, safarirjRomInfo, safarirjRomName, SafarirSampleInfo, SafarirSampleName, SafarirInputInfo, SafarirDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	208, 256, 3, 4
};
