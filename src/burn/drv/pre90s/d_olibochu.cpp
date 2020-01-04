// FB Alpha Oli-Boo-Chu driver module
// Based on MAME driver by Nicola Salmoria

#include "tiles_generic.h"
#include "z80_intf.h"
#include "ay8910.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvVidRAM;
static UINT8 *DrvUnkRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT16 sound_command;
static UINT8 soundlatch;
static UINT8 flipscreen;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo OlibochuInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"	},
	{"P1 Start",	BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 coin"	},
	{"P2 Start",	BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 left"	},
	{"P2 Right",	BIT_DIGITAL,	DrvJoy3 + 0,	"p2 right"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Olibochu)

static struct BurnDIPInfo OlibochuDIPList[]=
{
	{0x0e, 0xff, 0xff, 0xff, NULL					},
	{0x0f, 0xff, 0xff, 0xff, NULL					},
	{0x10, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x0e, 0x01, 0x03, 0x00, "2"					},
	{0x0e, 0x01, 0x03, 0x03, "3"					},
	{0x0e, 0x01, 0x03, 0x02, "4"					},
	{0x0e, 0x01, 0x03, 0x01, "5"					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x0e, 0x01, 0x0c, 0x0c, "5000"					},
	{0x0e, 0x01, 0x0c, 0x08, "10000"				},
	{0x0e, 0x01, 0x0c, 0x04, "15000"				},
	{0x0e, 0x01, 0x0c, 0x00, "None"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x0e, 0x01, 0x40, 0x00, "Upright"				},
	{0x0e, 0x01, 0x40, 0x40, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Cross Hatch Pattern"	},
	{0x0e, 0x01, 0x80, 0x80, "Off"					},
	{0x0e, 0x01, 0x80, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Stop Mode (Cheat)"	},
	{0x10, 0x01, 0x01, 0x01, "Off"					},
	{0x10, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    8, "Coin A"				},
	{0x10, 0x01, 0x0e, 0x00, "4 Coins 1 Credits"	},
	{0x10, 0x01, 0x0e, 0x02, "3 Coins 1 Credits"	},
	{0x10, 0x01, 0x0e, 0x04, "2 Coins 1 Credits"	},
	{0x10, 0x01, 0x0e, 0x0e, "1 Coin  1 Credits"	},
	{0x10, 0x01, 0x0e, 0x0c, "1 Coin  2 Credits"	},
	{0x10, 0x01, 0x0e, 0x0a, "1 Coin  3 Credits"	},
	{0x10, 0x01, 0x0e, 0x08, "1 Coin  4 Credits"	},
	{0x10, 0x01, 0x0e, 0x06, "1 Coin  5 Credits"	},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x10, 0x01, 0x10, 0x10, "Off"					},
	{0x10, 0x01, 0x10, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Invulnerability"		},
	{0x10, 0x01, 0x20, 0x20, "Off"					},
	{0x10, 0x01, 0x20, 0x00, "On"					},
};

STDDIPINFO(Olibochu)

static inline void update_soundlatch()
{
	for (INT32 i = 15; i >= 0; i--)
	{
		if (sound_command & (1 << i)) {
			soundlatch = i ^ 0xf;
			break;
		}
	}
}

static void __fastcall olibochu_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xa800:
			sound_command = (sound_command & 0x00ff) | (data << 8);
			update_soundlatch();
		return;

		case 0xa801:
			sound_command = (sound_command & 0xff00) | (data << 0);
			update_soundlatch();
		return;

		case 0xa802:
			flipscreen = data & 0x80;
		return;
	}
}

static UINT8 __fastcall olibochu_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xa000:
			return DrvInputs[0];

		case 0xa001:
			return DrvInputs[1];

		case 0xa002:
			return DrvInputs[2];

		case 0xa003:
			return DrvDips[0];

		case 0xa004:
			return DrvDips[1];

		case 0xa005:
			return DrvDips[2];
	}

	return 0;
}

static void __fastcall olibochu_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x7000:
		case 0x7001:
			AY8910Write(0, address & 1, data);
		return;

		case 0x7004:
		case 0x7006:
		return;
	}
}

static UINT8 __fastcall olibochu_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x7000:
			return soundlatch;
	}

	return 0;
}

static tilemap_callback( bg )
{
	INT32 attr = DrvVidRAM[offs + 0x400];
	INT32 code = DrvVidRAM[offs] + ((attr & 0x20) << 3);

	TILE_SET_INFO(0, code, attr, TILE_FLIPYX(attr >> 6));
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	ZetClose();

	AY8910Reset(0);

	soundlatch = 0;
	sound_command = 0;
	flipscreen = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x008000;
	DrvZ80ROM1		= Next; Next += 0x002000;

	DrvGfxROM0		= Next; Next += 0x008000;
	DrvGfxROM1		= Next; Next += 0x010000;

	DrvColPROM		= Next; Next += 0x000220;

	DrvPalette		= (UINT32*)Next; Next += 0x0200 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM0		= Next; Next += 0x001000;
	DrvZ80RAM1		= Next; Next += 0x000400;
	DrvVidRAM		= Next; Next += 0x000800;
	DrvUnkRAM		= Next; Next += 0x001000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[2]  = { 0x2000*8, 0x0000 };
	INT32 XOffs[16] = { STEP8(7,-1), STEP8(128+7,-1) };
	INT32 YOffs[16] = { STEP16(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x4000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x4000);

	GfxDecode(0x0200, 2,  8,  8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x4000);

	GfxDecode(0x0100, 2, 16, 16, Plane, XOffs, YOffs, 0x100, tmp, DrvGfxROM1);

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
		if (BurnLoadRom(DrvZ80ROM0 + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x1000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x2000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x3000,  3, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x4000,  4, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x5000,  5, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x6000,  6, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x7000,  7, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x0000,  8, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM1 + 0x1000,  9, 1)) return 1;

//		if (BurnLoadRom(DrvSamROM  + 0x0000, 10, 1)) return 1;
//		if (BurnLoadRom(DrvSamROM  + 0x0000, 11, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x0000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x2000, 13, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x0000, 14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x1000, 15, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x2000, 16, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x3000, 17, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x0000, 18, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0020, 19, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0120, 20, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvVidRAM,			0x8000, 0x87ff, MAP_RAM);
	ZetMapMemory(DrvUnkRAM,			0x9000, 0x9fff, MAP_RAM); // 9000-903f and 9800-983f
	ZetMapMemory(DrvZ80RAM0,		0xf000, 0xffff, MAP_RAM);
	ZetSetWriteHandler(olibochu_main_write);
	ZetSetReadHandler(olibochu_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,		0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,		0x6000, 0x63ff, MAP_RAM);
	ZetSetWriteHandler(olibochu_sound_write);
	ZetSetReadHandler(olibochu_sound_read);
	ZetClose();

	AY8910Init(0, 2000000, 0);
	AY8910SetAllRoutes(0, 0.50, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 2, 8, 8, 0x8000, 0x80, 0x1f);
	GenericTilemapSetOffsets(0, 0, -8);

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
	UINT32 pal[32];

	for (INT32 i = 0; i < 0x20; i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 1;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 1;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 1;
		INT32 r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = (DrvColPROM[i] >> 3) & 1;
		bit1 = (DrvColPROM[i] >> 4) & 1;
		bit2 = (DrvColPROM[i] >> 5) & 1;
		INT32 g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = (DrvColPROM[i] >> 6) & 1;
		bit1 = (DrvColPROM[i] >> 7) & 1;
		INT32 b = 0x4f * bit0 + 0xa8 * bit1;

		pal[i] = BurnHighCol(r,g,b,0);
	}

	for (INT32 i = 0; i < 0x200; i++)
	{
		DrvPalette[i] = pal[(DrvColPROM[0x020 + i] & 0x0f) | ((~i >> 4) & 0x10)];
	}
}

#define RenderTile(func, rend_to, code, sx, sy, color, bit, trans, offs, rom)   \
	if (flipy) {                                                                \
    	if (flipx) {                                                            \
            	func ## _FlipXY_Clip(rend_to, code, sx, sy, color, bit, trans, offs, rom);  \
			} else {                                                                        \
	            func ## _FlipY_Clip(rend_to, code, sx, sy, color, bit, trans, offs, rom);   \
			}                                                                               \
		} else {                                                                            \
			if (flipx) {                                                                    \
	            func ## _FlipX_Clip(rend_to, code, sx, sy, color, bit, trans, offs, rom);   \
			} else {                                                                        \
	            func ## _Clip(rend_to, code, sx, sy, color, bit, trans, offs, rom);         \
			}                                                                               \
		}

static void draw_sprites_16x16()
{
	UINT8 *spriteram = DrvZ80RAM0 + 0x400;

	for (INT32 offs = 0; offs < 0x20; offs += 4)
	{
		INT32 attr = spriteram[offs + 1];
		INT32 code = spriteram[offs];
		INT32 color = attr & 0x3f;
		INT32 flipx = attr & 0x40;
		INT32 flipy = attr & 0x80;
		INT32 sx = spriteram[offs + 3];
		INT32 sy = ((spriteram[offs + 2] + 8) & 0xff) - 8;

		if (flipscreen)
		{
			sx = 240 - sx;
			sy = 240 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		sy -= 8;

		RenderTile(Render16x16Tile_Mask, pTransDraw, code, sx, sy, color, 2, 0, 0x100, DrvGfxROM1)
	}
}

static void draw_sprites_8x8()
{
	UINT8 *spriteram = DrvZ80RAM0 + 0x440;

	for (INT32 offs = 0; offs < 0x40; offs += 4)
	{
		INT32 attr = spriteram[offs + 1];
		INT32 code = spriteram[offs];
		INT32 color = attr & 0x3f;
		INT32 flipx = attr & 0x40;
		INT32 flipy = attr & 0x80;
		INT32 sx = spriteram[offs + 3];
		INT32 sy = spriteram[offs + 2];

		if (flipscreen)
		{
			sx = 248 - sx;
			sy = 248 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		sy -= 8;

		RenderTile(Render8x8Tile_Mask, pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM0)
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	BurnTransferClear();
	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);

	if (nBurnLayer & 2) draw_sprites_16x16();
	if (nBurnLayer & 4) draw_sprites_8x8();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 4000000 / 60, 4000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);

		if (i == 0) {
			ZetSetVector(0xcf);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}

		if (i == 248) {
			ZetSetVector(0xd7);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}

		ZetClose();

		ZetOpen(1);
		nCyclesDone[1] += ZetRun(nCyclesTotal[1] / nInterleave);
		if (i == 255) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
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
		AY8910Scan(nAction, pnMin);

		SCAN_VAR(soundlatch);
		SCAN_VAR(sound_command);
		SCAN_VAR(flipscreen);
	}

	return 0;
}


// Oli-Boo-Chu

static struct BurnRomInfo olibochuRomDesc[] = {
	{ "1b.3n",	0x1000, 0xbf17f4f4, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "2b.3lm",	0x1000, 0x63833b0d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3b.3k",	0x1000, 0xa4038e8b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4b.3j",	0x1000, 0xaad4bec4, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "5b.3h",	0x1000, 0x66efa79f, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "6b.3f",	0x1000, 0x1123d1ef, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "7c.3e",	0x1000, 0x89c26fb4, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "8b.3d",	0x1000, 0xaf19e5a5, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "17.4j",	0x1000, 0x57f07402, 2 | BRF_PRG | BRF_ESS }, //  8 Z80 #1 Code
	{ "18.4l",	0x1000, 0x0a903e9c, 2 | BRF_PRG | BRF_ESS }, //  9

	{ "15.1k",	0x1000, 0xfb5dd281, 3 | BRF_SND },           // 10 Samples?
	{ "16.1m",	0x1000, 0xc07614a5, 3 | BRF_SND },           // 11

	{ "13.6n",	0x1000, 0xb4fcf9af, 4 | BRF_GRA },           // 12 Characters
	{ "14.4n",	0x1000, 0xaf54407e, 4 | BRF_GRA },           // 13

	{ "9.6a",	0x1000, 0xfa69e16e, 5 | BRF_GRA },           // 14 Sprites
	{ "10.2a",	0x1000, 0x10359f84, 5 | BRF_GRA },           // 15
	{ "11.4a",	0x1000, 0x1d968f5f, 5 | BRF_GRA },           // 16
	{ "12.2a",	0x1000, 0xd8f0c157, 5 | BRF_GRA },           // 17

	{ "c-1",	0x0020, 0xe488e831, 6 | BRF_GRA },           // 18 Color data
	{ "c-2",	0x0100, 0x698a3ba0, 6 | BRF_GRA },           // 19
	{ "c-3",	0x0100, 0xefc4e408, 6 | BRF_GRA },           // 20
};

STD_ROM_PICK(olibochu)
STD_ROM_FN(olibochu)

struct BurnDriver BurnDrvOlibochu = {
	"olibochu", NULL, NULL, NULL, "1981",
	"Oli-Boo-Chu\0", NULL, "Irem / GDI", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_IREM_MISC, GBF_MAZE, 0,
	NULL, olibochuRomInfo, olibochuRomName, NULL, NULL, NULL, NULL, OlibochuInputInfo, OlibochuDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	240, 256, 3, 4
};
