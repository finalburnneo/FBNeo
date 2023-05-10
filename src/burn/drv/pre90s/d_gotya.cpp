// FB Neo Got-ya! / The Hand driver module
// Based on MAME driver by Zsolt Vasvari

#include "tiles_generic.h"
#include "z80_intf.h"
#include "samples.h"
#include "watchdog.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvVidRAM0;
static UINT8 *DrvVidRAM1;
static UINT8 *DrvColRAM;
static UINT8 *DrvSprRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT16 scroll;
static UINT8 flipscreen;
static INT32 tune_timer;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo GotyaInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 3"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Gotya)

static struct BurnDIPInfo GotyaDIPList[]=
{
	{0x12, 0xff, 0xff, 0x10, NULL					},
	{0x13, 0xff, 0xff, 0x10, NULL					},
	{0x14, 0xff, 0xff, 0xa0, NULL					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x12, 0x01, 0x10, 0x10, "Off"					},
	{0x12, 0x01, 0x10, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Sound Test"			},
	{0x13, 0x01, 0x10, 0x10, "Off"					},
	{0x13, 0x01, 0x10, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x14, 0x01, 0x02, 0x00, "Upright"				},
	{0x14, 0x01, 0x02, 0x02, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Difficulty"			},
	{0x14, 0x01, 0x10, 0x00, "Easy"					},
	{0x14, 0x01, 0x10, 0x10, "Hard"					},

	{0   , 0xfe, 0   ,    2, "Bonus Life"			},
	{0x14, 0x01, 0x20, 0x00, "None"					},
	{0x14, 0x01, 0x20, 0x20, "15000"				},

	{0   , 0xfe, 0   ,    2, "Lives"				},
	{0x14, 0x01, 0x40, 0x00, "3"					},
	{0x14, 0x01, 0x40, 0x40, "5"					},

	{0   , 0xfe, 0   ,    2, "Game Type"			},
	{0x14, 0x01, 0x80, 0x80, "Normal"				},
	{0x14, 0x01, 0x80, 0x00, "Endless"				},
};

STDDIPINFO(Gotya)

static void sound_write(UINT8 data)
{
	static const UINT8 sample_order[20] = {
		0x01, 0x02, 0x03, 0x05, 0x06, 0x07, 0x08, 0x0a,
		0x0b, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
		0x17, 0x18, 0x19, 0x1a
	};
	static const UINT8 sample_attr_channel[20] = {
		0x00, 0x01, 0x02, 0x02, 0x83, 0x04, 0x00, 0x00,
		0x00, 0x03, 0x03, 0x40, 0x03, 0x03, 0x03, 0x03,
		0x03, 0x03, 0x03, 0x03
	};

	if (data == 0)
	{
		BurnSampleReset(); // stop all samples
		return;
	}

	for (INT32 i = 0; i < 20; i++)
	{
		if (sample_order[i] == data)
		{
			if (i == 6 && BurnSampleGetStatus(6) == SAMPLE_PLAYING) {
				return;
			}
			BurnSampleChannelPlay(sample_attr_channel[i] & 0xf, i, (i==6) ? 1 : 0);

			if (sample_attr_channel[i] & 0x80) {
				tune_timer = 60+40; // a little over 1.5 seconds
			}
			if (sample_attr_channel[i] & 0x40) {
				tune_timer = 0; // reset timer
			}

			return;
		}
	}
}

static void __fastcall gotya_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x6004:
			scroll = (scroll & 0xff) | ((data & 1) << 8);
			flipscreen = data & 2;
		return;

		case 0x6005:
			sound_write(data);
		return;

		case 0x6006:
			scroll = (scroll & 0x100) | data;
		return;

		case 0x6007:
			BurnWatchdogWrite();
		return;
	}
}

static UINT8 __fastcall gotya_read(UINT16 address)
{
	switch (address)
	{
		case 0x6000:
			return DrvInputs[0];

		case 0x6001:
			return DrvInputs[1];

		case 0x6002:
			return DrvInputs[2];
	}

	return 0;
}

static tilemap_callback( bg )
{
	TILE_SET_INFO(0, DrvVidRAM0[offs], DrvColRAM[offs], 0);
}

static tilemap_scan( bg )
{
	row ^= 0x1f;
	col ^= 0x3f;
	return (row << 5) | (col & 0x1f) | ((col >> 5) << 10);
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	ZetOpen(0);
	ZetReset();
	ZetClose();

	BurnSampleReset();

	BurnWatchdogReset();

	scroll = 0;
	flipscreen = 0;
	tune_timer = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x004000;

	DrvGfxROM0		= Next; Next += 0x004000;
	DrvGfxROM1		= Next; Next += 0x004000;

	DrvColPROM		= Next; Next += 0x000120;

	DrvPalette		= (UINT32*)Next; Next += 0x040 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x001000;
	DrvVidRAM0		= Next; Next += 0x000800;
	DrvVidRAM1		= Next; Next += 0x000400;
	DrvColRAM		= Next; Next += 0x000800;
	DrvSprRAM		= DrvVidRAM1 + 0x3e0;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[2]   = { 0, 4 };
	INT32 XOffs0[8]  = { STEP4(0,1), STEP4(8*8,1) };
	INT32 YOffs0[8]  = { STEP8(7*8,-8) };
	INT32 XOffs1[16] = { STEP4(0,1), STEP4(24*8,1), STEP4(16*8,1), STEP4(8*8,1) };
	INT32 YOffs1[16] = { STEP8(39*8,-8), STEP8(7*8,-8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x1000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x1000);

	GfxDecode(0x0100, 2,  8,  8, Plane, XOffs0, YOffs0, 0x080, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x1000);

	GfxDecode(0x0040, 2, 16, 16, Plane, XOffs1, YOffs1, 0x200, tmp, DrvGfxROM1);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM  + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x1000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x2000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x3000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x0000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x0000,  5, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x0000,  6, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0020,  7, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,			0x5000, 0x5fff, MAP_RAM);
	ZetMapMemory(DrvVidRAM0,		0xc000, 0xc7ff, MAP_RAM);
	ZetMapMemory(DrvColRAM,			0xc800, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvVidRAM1,		0xd000, 0xd3ff, MAP_RAM); // spr 3e0+
	ZetSetWriteHandler(gotya_write);
	ZetSetReadHandler(gotya_read);
	ZetClose();

	BurnWatchdogInit(DrvDoReset, 180);

	BurnSampleInit(0);
	BurnSampleSetAllRoutesAllSamples(0.16, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, bg_map_scan, bg_map_callback, 8, 8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 2, 8, 8, 0x4000, 0, 0xf);
	GenericTilemapSetOffsets(0, 16, -16);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	BurnSampleExit();

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteInit()
{
	UINT32 tab[0x20];

	for (INT32 i = 0; i < 0x20; i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 0x01;
		INT32 r = (((bit0 * 1000) + (bit1 * 470) + (bit2 * 220)) * 255) / 1690;

		bit0 = (DrvColPROM[i] >> 3) & 0x01;
		bit1 = (DrvColPROM[i] >> 4) & 0x01;
		bit2 = (DrvColPROM[i] >> 5) & 0x01;
		INT32 g = (((bit0 * 1000) + (bit1 * 470) + (bit2 * 220)) * 255) / 1690;

		bit0 = (DrvColPROM[i] >> 6) & 0x01;
		bit1 = (DrvColPROM[i] >> 7) & 0x01;
		INT32 b = (((bit0 * 470) + (bit1 * 220)) * 255) / 690;

		tab[i] = BurnHighCol(r,g,b,0);
	}

	for (INT32 i = 0; i < 0x40; i++)
	{
		DrvPalette[i] = tab[DrvColPROM[0x20 + i]];
	}
}

static void draw_status(INT32 sx, int column)
{
	if (flipscreen) sx = 35 - sx;

	for (INT32 row = 29; row >= 0; row--)
	{
		INT32 sy = 31 - row;
		if (flipscreen) sy = row;

		UINT8 code  = DrvVidRAM1[(row * 32) + column + 0x00];
		UINT8 color = DrvVidRAM1[(row * 32) + column + 0x10] & 0xf;

		Draw8x8Tile(pTransDraw, code, sx * 8, (sy * 8) - 16, flipscreen, flipscreen, color, 2, 0, DrvGfxROM0);
	}
}

static void draw_sprites()
{
	for (INT32 offs = 2; offs < 0x0e; offs += 2)
	{
		INT32 code  = DrvSprRAM[offs + 0x01] >> 2;
		INT32 color = DrvSprRAM[offs + 0x11] & 0x0f;
		INT32 sx    = 256 - DrvSprRAM[offs + 0x10] + (DrvSprRAM[offs + 0x01] & 0x01) * 256;
		INT32 sy    = DrvSprRAM[offs + 0x00];
		if (flipscreen) sy = 240 - sy;

		Draw16x16MaskTile(pTransDraw, code, sx, sy - 16, flipscreen, flipscreen, color, 2, 0, 0, DrvGfxROM1);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	BurnTransferClear();

	GenericTilemapSetFlip(0, flipscreen ? TMAP_FLIPXY : 0);
	GenericTilemapSetScrollX(0, -scroll);

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);

	if (nSpriteEnable & 0x01) draw_sprites();

	if (nSpriteEnable & 0x02) draw_status( 0,  1);
	if (nSpriteEnable & 0x04) draw_status( 1,  0);
	if (nSpriteEnable & 0x08) draw_status( 2,  2);
	if (nSpriteEnable & 0x10) draw_status(33, 13);
	if (nSpriteEnable & 0x20) draw_status(35, 14);
	if (nSpriteEnable & 0x40) draw_status(34, 15);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	{
		DrvInputs[0] = (0xff & ~0x10) | (DrvDips[0] & 0x10);
		DrvInputs[1] = (0xff & ~0x10) | (DrvDips[1] & 0x10);
		DrvInputs[2] = (0xff & ~0xf2) | (DrvDips[2] & 0xf2);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		ProcessJoystick(&DrvInputs[0], 0, 3,2,0,1, INPUT_4WAY | INPUT_ISACTIVELOW);
		ProcessJoystick(&DrvInputs[1], 1, 3,2,0,1, INPUT_4WAY | INPUT_ISACTIVELOW);
	}

	ZetOpen(0);
	ZetRun(3072000 / 60);
	ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
	ZetClose();

	HiscoreReset();


	if (tune_timer > 0) {
		tune_timer--;
		if (tune_timer == 0) {
			sound_write(0x08);
		}
	}

	if (pBurnSoundOut) {
		BurnSampleRender(pBurnSoundOut, nBurnSoundLen);
		BurnSoundDCFilter();
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
		*pnMin = 0x029695;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);

		BurnSampleScan(nAction, pnMin);

		BurnWatchdogScan(nAction);

		SCAN_VAR(scroll);
		SCAN_VAR(flipscreen);
		SCAN_VAR(tune_timer);
	}

	return 0;
}

static struct BurnSampleInfo ThehandSampleDesc[] = {
	{ "01", SAMPLE_NOLOOP },	// game start tune
	{ "02", SAMPLE_NOLOOP },	// coin in
	{ "03", SAMPLE_NOLOOP },	// eat dot
	{ "05", SAMPLE_NOLOOP },	// eat dollar sign
	{ "06", SAMPLE_NOLOOP },	// door open
	{ "07", SAMPLE_NOLOOP },	// door close
	{ "08", SAMPLE_AUTOLOOP },	// theme song
	{ "0a", SAMPLE_NOLOOP },	// piccolo
	{ "0b", SAMPLE_NOLOOP },	// tune
	{ "10", SAMPLE_NOLOOP },	// 'We're even. Bye Bye!'
	{ "11", SAMPLE_NOLOOP },	// 'You got me!'
	{ "12", SAMPLE_NOLOOP },	// 'You have lost out'
	{ "13", SAMPLE_NOLOOP },	// 'Rock'
	{ "14", SAMPLE_NOLOOP },	// 'Scissors'
	{ "15", SAMPLE_NOLOOP },	// 'Paper'
	{ "16", SAMPLE_NOLOOP },	// 'Very good!'
	{ "17", SAMPLE_NOLOOP },	// 'Wonderful!
	{ "18", SAMPLE_NOLOOP },	// 'Come on!'
	{ "19", SAMPLE_NOLOOP },	// 'I love you!'
	{ "1a", SAMPLE_NOLOOP },	// 'See you again!'
	{ "", 0 }
};

STD_SAMPLE_PICK(Thehand)
STD_SAMPLE_FN(Thehand)


// The Hand

static struct BurnRomInfo thehandRomDesc[] = {
	{ "hand6.bin",	0x1000, 0xa33b806c, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "hand5.bin",	0x1000, 0x89bcde82, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "hand4.bin",	0x1000, 0xc6844a83, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gb-03.bin",	0x1000, 0xf34d90ab, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "hand12.bin",	0x1000, 0x95773b46, 2 | BRF_GRA },           //  4 Background Tiles

	{ "gb-11.bin",	0x1000, 0x5d5eca1b, 3 | BRF_GRA },           //  5 Sprites

	{ "prom.1a",	0x0020, 0x4864a5a0, 4 | BRF_GRA },           //  6 Color Data
	{ "prom.4c",	0x0100, 0x4745b5f6, 4 | BRF_GRA },           //  7

	{ "hand1.bin",	0x0800, 0xccc537e0, 0 | BRF_OPT },           //  8 Unknown
	{ "gb-02.bin",	0x0800, 0x65a7e284, 0 | BRF_OPT },           //  9
	{ "gb-10.bin",	0x1000, 0x8101915f, 0 | BRF_OPT },           // 10
	{ "gb-09.bin",	0x1000, 0x619bba76, 0 | BRF_OPT },           // 11
	{ "gb-08.bin",	0x1000, 0x82f59528, 0 | BRF_OPT },           // 12
	{ "hand7.bin",	0x1000, 0xfbf1c5de, 0 | BRF_OPT },           // 13
};

STD_ROM_PICK(thehand)
STD_ROM_FN(thehand)

struct BurnDriver BurnDrvThehand = {
	"thehand", NULL, NULL, "thehand", "1981",
	"The Hand\0", NULL, "T.I.C.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, thehandRomInfo, thehandRomName, NULL, NULL, ThehandSampleInfo, ThehandSampleName, GotyaInputInfo, GotyaDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 288, 3, 4
};


// Got-Ya (12/24/1981, prototype?)

static struct BurnRomInfo gotyaRomDesc[] = {
	{ "gb-06.bin",	0x1000, 0x7793985a, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "gb-05.bin",	0x1000, 0x683d188b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gb-04.bin",	0x1000, 0x15b72f09, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gb-03.bin",	0x1000, 0xf34d90ab, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "gb-12.bin",	0x1000, 0x4993d735, 2 | BRF_GRA },           //  4 Background Tiles

	{ "gb-11.bin",	0x1000, 0x5d5eca1b, 3 | BRF_GRA },           //  5 Sprites

	{ "prom.1a",	0x0020, 0x4864a5a0, 4 | BRF_GRA },           //  6 Color Data
	{ "prom.4c",	0x0100, 0x4745b5f6, 4 | BRF_GRA },           //  7

	{ "gb-01.bin",	0x0800, 0xc31dba64, 0 | BRF_OPT },           //  8 Unknown
	{ "gb-02.bin",	0x0800, 0x65a7e284, 0 | BRF_OPT },           //  9
	{ "gb-10.bin",	0x1000, 0x8101915f, 0 | BRF_OPT },           // 10
	{ "gb-09.bin",	0x1000, 0x619bba76, 0 | BRF_OPT },           // 11
	{ "gb-08.bin",	0x1000, 0x82f59528, 0 | BRF_OPT },           // 12
	{ "gb-07.bin",	0x1000, 0x92a9f8bf, 0 | BRF_OPT },           // 13
};

STD_ROM_PICK(gotya)
STD_ROM_FN(gotya)

struct BurnDriver BurnDrvGotya = {
	"gotya", "thehand", NULL, "thehand", "1981",
	"Got-Ya (12/24/1981, prototype?)\0", NULL, "Game-A-Tron", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, gotyaRomInfo, gotyaRomName, NULL, NULL, ThehandSampleInfo, ThehandSampleName, GotyaInputInfo, GotyaDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 288, 3, 4
};
