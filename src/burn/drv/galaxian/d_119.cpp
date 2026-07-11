// FinalBurn Neo Sega 119 driver module
// Based on MAME driver by David Haywood

#ifdef _MSC_VER
#include "../burn/drv/galaxian/gal.h"
#else
#include "gal.h"
#endif // _MSC_VER

static UINT8 *AllMem;
static UINT8 *MemEnd;

static UINT8 DrvRecalc;

static UINT8 character_bank;
static UINT8 sprite_bank;
static UINT8 nmi_enable;
static INT32 nCyclesExtra[2];

static struct BurnInputInfo DrvInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	GalInputPort1 + 5,	"p1 coin"	},
	{"P1 Start",	BIT_DIGITAL,	GalInputPort0 + 5,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	GalInputPort0 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	GalInputPort0 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	GalInputPort0 + 1,	"p1 left"	},
	{"P1 Right",	BIT_DIGITAL,	GalInputPort0 + 0,	"p1 right"	},
	{"P1 Button",	BIT_DIGITAL,	GalInputPort0 + 4,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	GalInputPort1 + 6,	"p2 coin"	},
	{"P2 Start",	BIT_DIGITAL,	GalInputPort0 + 6,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	GalInputPort1 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	GalInputPort1 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	GalInputPort1 + 1,	"p2 left"	},
	{"P2 Right",	BIT_DIGITAL,	GalInputPort1 + 0,	"p2 right"	},
	{"P2 Button",	BIT_DIGITAL,	GalInputPort1 + 4,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&GalReset,			"reset"		},
	{"Service",		BIT_DIGITAL,	GalInputPort0 + 7,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	GalDip + 0,			"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	GalDip + 1,			"dip"		},
};

STDINPUTINFO(Drv)

static struct BurnDIPInfo DrvDIPList[]=
{
	DIP_OFFSET(0x10)
	{0x00, 0xff, 0xff, 0xfc, NULL					},
	{0x01, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x00, 0x01, 0x01, 0x00, "Upright"				},
	{0x00, 0x01, 0x01, 0x01, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x00, 0x01, 0x02, 0x02, "Off"					},
	{0x00, 0x01, 0x02, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x00, 0x01, 0x0c, 0x0c, "3"					},
	{0x00, 0x01, 0x0c, 0x08, "4"					},
	{0x00, 0x01, 0x0c, 0x04, "5"					},
	{0x00, 0x01, 0x0c, 0x00, "Free"					},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x00, 0x01, 0x10, 0x10, "Off"					},
	{0x00, 0x01, 0x10, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x00, 0x01, 0x20, 0x20, "Off"					},
	{0x00, 0x01, 0x20, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x00, 0x01, 0x40, 0x40, "Off"					},
	{0x00, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x00, 0x01, 0x80, 0x80, "Off"					},
	{0x00, 0x01, 0x80, 0x00, "On"					},

	{0   , 0xfe, 0   ,    8, "Coin B"				},
	{0x01, 0x01, 0x07, 0x00, "3 Coins 1 Credits"	},
	{0x01, 0x01, 0x07, 0x03, "2 Coins 1 Credits"	},
	{0x01, 0x01, 0x07, 0x02, "2 Coins 2 Credits"	},
	{0x01, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x01, 0x01, 0x07, 0x01, "2 Coins 3 Credits"	},
	{0x01, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x01, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x01, 0x01, 0x07, 0x04, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin A"				},
	{0x01, 0x01, 0x38, 0x00, "3 Coins 1 Credits"	},
	{0x01, 0x01, 0x38, 0x18, "2 Coins 1 Credits"	},
	{0x01, 0x01, 0x38, 0x10, "2 Coins 2 Credits"	},
	{0x01, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},
	{0x01, 0x01, 0x38, 0x08, "2 Coins 3 Credits"	},
	{0x01, 0x01, 0x38, 0x30, "1 Coin  2 Credits"	},
	{0x01, 0x01, 0x38, 0x28, "1 Coin  3 Credits"	},
	{0x01, 0x01, 0x38, 0x20, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x01, 0x01, 0x40, 0x40, "Off"					},
	{0x01, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x01, 0x01, 0x80, 0x80, "Off"					},
	{0x01, 0x01, 0x80, 0x00, "On"					},
};

STDDIPINFO(Drv)

static void __fastcall _119_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xa000:
			GalSoundLatch = data;
			ZetSetIRQLine(1, 0, CPU_IRQSTATUS_HOLD);
		return;

		case 0xb000:
			GalFlipScreenX = data & 0x01;
			GalFlipScreenY = data & 0x02;
			sprite_bank    =(data & 0x04) >> 2;
			character_bank =(data & 0x08) >> 3;
		return;
	}
}

static UINT8 __fastcall _119_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xb000:
			return GalInput[0];

		case 0xb001:
			return (GalInput[1] & 0x7f) | (GalVBlank ? 0x80 : 0);

		case 0xb002:
			return GalDip[0];

		case 0xb003:
			return GalDip[1];
	}

	return 0;
}

static void __fastcall _119_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
			AY8910Write(0, ~port & 1, data);
		return;

		case 0x02:
		case 0x04:
			nmi_enable = data & 0x40;
		return;

		case 0x03:
			DACWrite(0, data); // mc1408
		return;
	}
}

static UINT8 __fastcall _119_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x04:
			return GalSoundLatch;
	}

	return 0;
}

static tilemap_callback( bg )
{
	INT32 code  = GalVideoRam[offs];
	INT32 color = GalSpriteRam[(offs & 0x1f) * 2 + 1];

	TILE_SET_INFO(character_bank, code, color, 0);
}

static INT32 DrvDoReset()
{
	memset (GalRamStart, 0, GalRamEnd - GalRamStart);

	ZetReset(0);
	ZetReset(1);

	AY8910Reset(0);
	DACReset();

	nmi_enable = 0;
	character_bank = 0;
	sprite_bank = 0;
	GalSoundLatch = 0;
	GalFlipScreenX = 0;
	GalFlipScreenY = 0;

	nCyclesExtra[0] = nCyclesExtra[1] = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	GalZ80Rom1		= Next; Next += 0x010000;
	GalZ80Rom2		= Next; Next += 0x010000;

	GalChars		= Next; Next += 0x010000;
	GalSprites		= Next; Next += 0x010000;

	GalProm			= Next; Next += 0x000040;

	GalPalette		= (UINT32*)Next; Next += 0x0088 * sizeof(UINT32);

	GalRamStart		= Next;

	GalZ80Ram1		= Next; Next += 0x001000;
	GalZ80Ram2		= Next; Next += 0x000400;
	GalVideoRam		= Next; Next += 0x000400;
	GalSpriteRam	= Next; Next += 0x000100;

	GalRamEnd		= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[3]  = { 0x20000, 0x10000, 0x00000 };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x6000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, GalChars, 0x6000);

	GfxDecode(0x0400, 3,  8,  8, Plane, CharXOffsets,   CharYOffsets,   0x040, tmp, GalChars);
	GfxDecode(0x0100, 3, 16, 16, Plane, SpriteXOffsets, SpriteYOffsets, 0x100, tmp, GalSprites);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(GalZ80Rom1 + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(GalZ80Rom1 + 0x02000, k++, 1)) return 1;
		if (BurnLoadRom(GalZ80Rom1 + 0x04000, k++, 1)) return 1;
		if (BurnLoadRom(GalZ80Rom1 + 0x06000, k++, 1)) return 1;

		if (BurnLoadRom(GalChars   + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(GalChars   + 0x02000, k++, 1)) return 1;
		if (BurnLoadRom(GalChars   + 0x04000, k++, 1)) return 1;

		if (BurnLoadRom(GalZ80Rom2 + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(GalZ80Rom2 + 0x02000, k++, 1)) return 1;

		if (BurnLoadRom(GalProm    + 0x00000, k++, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(GalZ80Rom1,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(GalVideoRam,	0x8000, 0x83ff, MAP_RAM);
	ZetMapMemory(GalSpriteRam,	0x9000, 0x90ff, MAP_RAM);
	ZetMapMemory(GalZ80Ram1,	0xe000, 0xefff, MAP_RAM);
	ZetSetWriteHandler(_119_main_write);
	ZetSetReadHandler(_119_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(GalZ80Rom2,	0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(GalZ80Ram2,	0x8000, 0x83ff, MAP_RAM);
	ZetSetOutHandler(_119_sound_write_port);
	ZetSetInHandler(_119_sound_read_port);
	ZetClose();

	AY8910Init(0, 2000000, 0);
	AY8910SetAllRoutes(0, 0.30, BURN_SND_ROUTE_BOTH);
	AY8910SetBuffered(ZetTotalCycles, 4000000);

	DACInit(0, 0, 1, ZetTotalCycles, 4000000);
	DACSetRoute(0, 0.70, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, GalChars   + 0x00000, 3,   8, 8, 0x4000, 0, 3);
	GenericTilemapSetGfx(1, GalChars   + 0x08000, 3,   8, 8, 0x4000, 0, 3);
	GenericTilemapSetGfx(2, GalSprites + 0x04000, 3, 16, 16, 0x4000, 0, 3);
	GenericTilemapSetGfx(3, GalSprites + 0x0c000, 3, 16, 16, 0x4000, 0, 3);
	GenericTilemapSetScrollCols(0, 32);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	ZetExit();
	AY8910Exit(0);
	DACExit();

	BurnFreeMemIndex();

	return 0;
}

static void draw_bullets()
{
	const UINT8 *base = &GalSpriteRam[0x60];

	for (INT32 y = 0; y < nScreenHeight; y++)
	{
		UINT8 shell = 0xff, missile = 0xff;

		UINT8 effy = GalFlipScreenY ? ((y - 1) ^ 255) : (y - 1);
		for (INT32 which = 0; which < 3; which++)
			if ((UINT8)(base[which*4+1] + effy) == 0xff)
				shell = which;

		effy = GalFlipScreenY ? (y ^ 255) : y;
		for (INT32 which = 3; which < 8; which++)
			if ((UINT8)(base[which*4+1] + effy) == 0xff)
			{
				if (which != 7)
					shell = which;
				else
					missile = which;
			}

		if (shell != 0xff) GalaxianDrawBullets(shell, 255 - base[shell*4+3], y - 16);
		if (missile != 0xff) GalaxianDrawBullets(missile, 255 - base[missile*4+3], y - 16);
	}
}

static void draw_sprites()
{
	for (INT32 sprnum = 7; sprnum >= 0; sprnum--)
	{
		UINT8 *base = GalSpriteRam + 0x40 + (sprnum * 4);

		UINT8 sy = 240 - (base[0] - (sprnum < 3));
		INT32 code  = base[1] & 0x3f;
		INT32 flipx = base[1] & 0x40;
		INT32 flipy = base[1] & 0x80;
		INT32 color = base[2] & 7;
		UINT8 sx    = base[3] + 1;

		if (GalFlipScreenX)
		{
			sx = 240 - (sx - 2);
			flipx = !flipx;
		}

		if (GalFlipScreenY)
		{
			sy = 240 - sy;
			flipy = !flipy;
		}

		DrawGfxMaskTile(0, 2 + sprite_bank, code, sx, sy - 16, flipx, flipy, color, 0);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		GalaxianCalcPalette();
		DrvRecalc = 0;
	}

	GenericTilemapSetFlip(0, (GalFlipScreenY ? TMAP_FLIPY : 0) | (GalFlipScreenX ? TMAP_FLIPX : 0));

	for (INT32 i = 0; i < 32; i++) {
		GenericTilemapSetScrollCol(0, i, GalSpriteRam[i * 2 + 0] + 16);
	}

	if ( nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);
	if (~nBurnLayer & 1) BurnTransferClear();

	if (nSpriteEnable & 1) draw_sprites();
	if (nSpriteEnable & 2) draw_bullets();

	BurnTransferCopy(GalPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (GalReset) {
		DrvDoReset();
	}

	ZetNewFrame();

	{
		memset (GalInput, 0, sizeof(GalInput));

		for (INT32 i = 0; i < 8; i++) {
			GalInput[0] ^= (GalInputPort0[i] & 1) << i;
			GalInput[1] ^= (GalInputPort1[i] & 1) << i;
		}
	}

	INT32 nInterleave = 264;
	INT32 nCyclesTotal[2] = { 3072000 / 60, 4000000 / 60 };
	INT32 nCyclesDone[2] = { nCyclesExtra[0], nCyclesExtra[1] };

	GalVBlank = 1;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		CPU_RUN(0, Zet);
		ZetClose();

		ZetOpen(1);
		CPU_RUN(1, Zet);
		if (nmi_enable && (i & 3) == 3) {
			ZetNmi();
		}
		ZetClose();

		if (i == 16) GalVBlank = 0;

		if (i == 240) {
			GalVBlank = 1;

			if (pBurnDraw)
			{
				BurnDrvRedraw();
			}
		}
	}
	nCyclesExtra[0] = nCyclesDone[0] - nCyclesTotal[0];
	nCyclesExtra[1] = nCyclesDone[1] - nCyclesTotal[1];

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
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

		ba.Data	  = GalRamStart;
		ba.nLen	  = GalRamEnd - GalRamStart;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);
		AY8910Scan(nAction, pnMin);
		DACScan(nAction, pnMin);

		SCAN_VAR(character_bank);
		SCAN_VAR(sprite_bank);
		SCAN_VAR(GalFlipScreenX);
		SCAN_VAR(GalFlipScreenY);
		SCAN_VAR(GalSoundLatch);
		SCAN_VAR(nmi_enable);

		SCAN_VAR(nCyclesExtra);
	}

	return 0;
}


// 119 (bootleg?)

static struct BurnRomInfo _119RomDesc[] = {
	{ "119_4",	0x2000, 0xb614229e, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "119_3",	0x2000, 0xd2a984bf, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "119_2",	0x2000, 0xd96611bc, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "119_1",	0x2000, 0x368723e2, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "119_6",	0x2000, 0x1a7490a4, 2 | BRF_GRA },           //  4 Graphics
	{ "119_7",	0x2000, 0xfcff7f59, 2 | BRF_GRA },           //  5
	{ "119_5",	0x2000, 0x1b08c881, 2 | BRF_GRA },           //  6

	{ "119_8",	0x2000, 0x6570149c, 3 | BRF_PRG | BRF_ESS }, //  7 Z80 #0 Code
	{ "119_9",	0x2000, 0xb917e2c2, 3 | BRF_PRG | BRF_ESS }, //  8

	{ "6331",	0x0020, 0xb73e79f3, 4 | BRF_GRA },           //  9 Color PROM

	{ "7502",	0x0020, 0x52bdbe39, 0 | BRF_OPT },           // 10 Unknown PROM
};

STD_ROM_PICK(_119)
STD_ROM_FN(_119)

struct BurnDriver BurnDrv119 = {
	"119", NULL, NULL, NULL, "1986",
	"119 (bootleg?)\0", "This game is buggy!", "Coreland / Sega", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_PLATFORM, 0,
	NULL, _119RomInfo, _119RomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x88,
	256, 224, 4, 3
};
