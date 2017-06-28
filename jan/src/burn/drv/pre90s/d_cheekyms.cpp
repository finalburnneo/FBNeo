// FB Alpha Mr. Do! driver module
// Based on MAME driver by Nicola Salmoria

#include "tiles_generic.h"
#include "z80_intf.h"
#include "dac.h"
#define USE_SAMPLE_HACK // allow use of sampled SFX.

#ifdef USE_SAMPLE_HACK
#include "samples.h"
#endif

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;

static UINT8 *DrvROM;
static UINT8 *DrvRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSpriteRAM;
static UINT8 *Gfx0;
static UINT8 *Gfx1;
static UINT8 *Gfx2;
static UINT8 *Prom;
static UINT32 *Palette;
static UINT8 DrvRecalc;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvReset;
static UINT8 DrvDip[2];

static INT32 palettebnk, scrolly, flip;
static INT32 prevcoin;
static INT32 irqmask;

static UINT32 bHasSamples = 0;

static struct BurnInputInfo CheekymsInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 1"},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 fire 1"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDip + 0,	"dip"},
};

STDINPUTINFO(Cheekyms)


static struct BurnDIPInfo CheekymsDIPList[]=
{
	{0x0a, 0xff, 0xff, 0x75, NULL		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0a, 0x01, 0x03, 0x00, "2"		},
	{0x0a, 0x01, 0x03, 0x01, "3"		},
	{0x0a, 0x01, 0x03, 0x02, "4"		},
	{0x0a, 0x01, 0x03, 0x03, "5"		},

	{0   , 0xfe, 0   ,    3, "Coinage"		},
	{0x0a, 0x01, 0x0c, 0x08, "2 Coins 1 Credits"		},
	{0x0a, 0x01, 0x0c, 0x04, "1 Coin  1 Credits"		},
	{0x0a, 0x01, 0x0c, 0x00, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0a, 0x01, 0x10, 0x10, "Upright"		},
	{0x0a, 0x01, 0x10, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x0a, 0x01, 0x20, 0x00, "Off"		},
	{0x0a, 0x01, 0x20, 0x20, "On"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x0a, 0x01, 0xc0, 0x40, "3000"		},
	{0x0a, 0x01, 0xc0, 0x80, "4500"		},
	{0x0a, 0x01, 0xc0, 0xc0, "6000"		},
	{0x0a, 0x01, 0xc0, 0x00, "None"		},
};

STDDIPINFO(Cheekyms)

static void __fastcall port_write(UINT16 port, UINT8 data)
{
	static UINT8 lastdata = 0;

	port &= 0xff;

	if (port >= 0x20 && port <= 0x3f) { // sprite ram
		DrvSpriteRAM[port - 0x20] = data;
		return;
	}

	switch (port)
	{
		case 0x40:
			if (data != lastdata)
			{
				//if (data != 0x80 && data != 0x00)
				//	bprintf(0, _T("%X,"), data & ~0x80);

				if (data & 0x02) // short squeek / mouse counter
					BurnSamplePlay(0);
				if (data & 0x04) // squeak
					BurnSamplePlay(4);
				if (data & 0x10) // dead.
					BurnSamplePlay(0);
				if (data & 0x20) // 
					BurnSamplePlay(3);
				if (data & 0x30) // dead.
					BurnSamplePlay(2);
				if (data & 0x40) // 
					BurnSamplePlay(1);
			}
			lastdata = data;
			DACWrite(0, (data ? 0x80 : 0));
		return;

		case 0x80:
			palettebnk = (data >> 2) & 0x10;
			scrolly = ((data >> 3) & 0x07);
			flip = data & 0x80;
			irqmask = data & 0x04;
		return;
	}
}

static UINT8 __fastcall port_read(UINT16 port)
{
	UINT8 ret = 0xff;

	switch (port & 0xff)
	{
		case 0x00:
			return DrvDip[0];

		case 0x01:
			for (INT32 i = 0; i < 8; i++) ret ^= DrvJoy1[i] << i;
			return ret;
	}

	return 0;
}

static INT32 DrvSyncDAC()
{
	return (INT32)(float)(nBurnSoundLen * (ZetTotalCycles() / (2500000.000 / (nBurnFPS / 100.000))));
}

static INT32 DrvDoReset()
{
	DrvReset = 0;

	memset (AllRam, 0, RamEnd - AllRam);

	palettebnk = scrolly = flip = 0;
	prevcoin = 0;

	ZetOpen(0);
	ZetReset();
	ZetClose();
	DACReset();

#ifdef USE_SAMPLE_HACK
	BurnSampleReset();
#endif

	HiscoreReset();

	return 0;
}

static void palette_init()
{
	const UINT8 *color_prom = Prom;
	int i, j, bit, r, g, b;

	for (i = 0; i < 6; i++)
	{
		for (j = 0; j < 0x20; j++)
		{
			/* red component */
			bit = (color_prom[0x20 * (i / 2) + j] >> ((4 * (i & 1)) + 0)) & 0x01;
			r = 0xff * bit;
			/* green component */
			bit = (color_prom[0x20 * (i / 2) + j] >> ((4 * (i & 1)) + 1)) & 0x01;
			g = 0xff * bit;
			/* blue component */
			bit = (color_prom[0x20 * (i / 2) + j] >> ((4 * (i & 1)) + 2)) & 0x01;
			b = 0xff * bit;

			Palette[(i * 0x20) + j] = BurnHighCol(r, g, b, 0);
		}
	}
}

static void gfx_decode()
{
	static INT32 CharPlane[2]  = { 0, RGN_FRAC(0x1000, 1, 2) };
	static INT32 CharXOffs[8]  = { 0, 1, 2, 3, 4, 5, 6, 7 };
	static INT32 CharYOffs[8]  = { 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 };

	static INT32 SpriPlane[2]  = { RGN_FRAC(0x1000, 1, 2), 0 };
	static INT32 SpriXOffs[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
	static INT32 SpriYOffs[16] = { 0*16, 1*16,  2*16,  3*16,  4*16,  5*16,  6*16,  7*16,  8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16 };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x2000);
	if (!tmp) return;

	memcpy (tmp, Gfx0, 0x1000);

	GfxDecode(0x100, 2,  8,  8, CharPlane, CharXOffs, CharYOffs, 0x040, tmp, Gfx0);

	memcpy (tmp, Gfx1, 0x1000);

	GfxDecode(0x040, 2, 16, 16, SpriPlane, SpriXOffs, SpriYOffs, 0x100, tmp, Gfx1);

	BurnFree (tmp);
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvROM          = Next; Next += 0x10000;

	Gfx0            = Next; Next += 0x08000;
	Gfx1            = Next; Next += 0x08000;
	Gfx2            = Next; Next += 0x08000;

	Prom            = Next; Next += 0x00080;

	Palette	        = (UINT32 *)Next; Next += 0x00140 * sizeof(UINT32);

	AllRam			= Next;

	DrvRAM          = Next; Next += 0x01000;
	DrvVidRAM       = Next; Next += 0x00800;
	DrvSpriteRAM    = Next; Next += 0x00100;

	RamEnd			= Next;

	MemEnd          = Next;

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
		if(BurnLoadRom(DrvROM + 0x0000,  0, 1)) return 1;
		if(BurnLoadRom(DrvROM + 0x0800,  1, 1)) return 1;
		if(BurnLoadRom(DrvROM + 0x1000,  2, 1)) return 1;
		if(BurnLoadRom(DrvROM + 0x1800,  3, 1)) return 1;

		if(BurnLoadRom(Gfx0 + 0x0000,  4, 1)) return 1;
		if(BurnLoadRom(Gfx0 + 0x0800,  5, 1)) return 1;

		if(BurnLoadRom(Gfx1 + 0x0000,  6, 1)) return 1;
		if(BurnLoadRom(Gfx1 + 0x0800,  7, 1)) return 1;

		if(BurnLoadRom(Prom + 0x0000,  8, 1)) return 1;
		if(BurnLoadRom(Prom + 0x0020,  9, 1)) return 1;
		if(BurnLoadRom(Prom + 0x0040,  10, 1)) return 1;

		palette_init();
		gfx_decode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetSetInHandler(port_read);
	ZetSetOutHandler(port_write);
	ZetMapMemory(DrvROM,            0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvRAM,            0x3000, 0x33ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,         0x3800, 0x3bff, MAP_RAM);
	ZetClose();

	DACInit(0, 0, 0, DrvSyncDAC);
	DACSetRoute(0, 0.50, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

#ifdef USE_SAMPLE_HACK
	BurnUpdateProgress(0.0, _T("Loading samples..."), 0);
	bBurnSampleTrimSampleEnd = 1;
	BurnSampleInit(1);
	BurnSampleSetAllRoutesAllSamples(0.40, BURN_SND_ROUTE_BOTH);
    bHasSamples = BurnSampleGetStatus(0) != -1;

	if (!bHasSamples) { // Samples not found
		BurnSampleSetAllRoutesAllSamples(0.00, BURN_SND_ROUTE_BOTH);
	} else {
		bprintf(0, _T("Using Cheeky Mouse SFX samples!\n"));
	}
#endif
 
	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	ZetExit();
	DACExit();
#ifdef USE_SAMPLE_HACK
	BurnSampleExit();
#endif
	GenericTilesExit();

	BurnFree(AllMem);

	flip = scrolly = palettebnk = 0;

	return 0;
}

static void draw_sprites()
{
	for (INT32 offs = 0; offs < 0x20; offs += 4)
	{
		INT32 x, y, code, color;
		UINT8 poffset = 0x10;

		if ((DrvSpriteRAM[offs + 3] & 0x08) == 0x00) continue;

		x  = 256 - DrvSpriteRAM[offs + 2];
		y  = DrvSpriteRAM[offs + 1] - 32;
		code =  (~DrvSpriteRAM[offs + 0] & 0x0f) << 1;
		color = (~DrvSpriteRAM[offs + 3] & 0x07);

		if (x < 0 || y < -7 || x >= nScreenWidth || y >= nScreenHeight) continue;

		if (DrvSpriteRAM[offs + 0] & 0x80)
		{
			if (!flip)
				code++;

			Render16x16Tile_Mask_Clip(pTransDraw, code, x, y, color, 2, 0, poffset, Gfx1);
		}
		else
		{
			if (DrvSpriteRAM[offs + 0] & 0x02)
			{
				Render16x16Tile_Mask_Clip(pTransDraw, code | 0x20, x, y, color, 2, 0, poffset, Gfx1);
				Render16x16Tile_Mask_Clip(pTransDraw, code | 0x21, x + 0x10, y, color, 2, 0, poffset, Gfx1);
			}
			else
			{
				Render16x16Tile_Mask_Clip(pTransDraw, code | 0x20, x, y, color, 2, 0, poffset, Gfx1);
				Render16x16Tile_Mask_Clip(pTransDraw, code | 0x21, x, y + 0x10, color, 2, 0, poffset, Gfx1);
			}
		}
	}
}

static void draw_tiles()
{
	for (INT32 offs = 0x3ff; offs >= 0; offs--) {
		INT32 sx, sy, man_area;

		sx = offs % 32;
		sy = offs / 32;

		if (flip)
		{
			man_area = ((sy >=  5) && (sy <= 25) && (sx >=  8) && (sx <= 12));
		}
		else
		{
			man_area = ((sy >=  6) && (sy <= 26) && (sx >=  8) && (sx <= 12));
		}

		if (flip) {
			sx = 31 - sx;
			sy = 31 - sy;
		}

		INT32 code = DrvVidRAM[offs];
		INT32 color = palettebnk;

		if (sx >= 0x1e) {
			if (sy < 0x0c)
				color = 0x15;
			else if (sy < 0x14)
				color = 0x16;
			else
				color = 0x14;
		}
		else
		{
			if ((sy == 0x04) || (sy == 0x1b))
				color = palettebnk | 0x0c;
			else
				color = palettebnk | (sx >> 1);
		}

		sx *= 8;
		sy = (sy * 8) - (man_area ? scrolly : 0);
		sy -= 32; //offset

		Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, Gfx0);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		palette_init();
		DrvRecalc = 0;
	}

	BurnTransferClear();

	if (nBurnLayer & 2) draw_sprites();
	if (nBurnLayer & 1) draw_tiles();

	BurnTransferCopy(Palette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	INT32 nInterleave = 10;
	INT32 nCyclesTotal = 2500000 / 60;

	ZetNewFrame();
	ZetOpen(0);

	if ((DrvJoy2[0]) && (prevcoin != DrvJoy2[0])) {
		ZetNmi();
	}
	prevcoin = DrvJoy2[0] & 1;

	for (INT32 i = 0; i < nInterleave; i++) {
		ZetRun(nCyclesTotal / nInterleave);

		if ((i == nInterleave-1) && irqmask)
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
	}
	ZetClose();

	if (pBurnDraw) {
		DrvDraw();
	}
	
	if (pBurnSoundOut) {
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
#ifdef USE_SAMPLE_HACK
		if(bHasSamples)
			BurnSampleRender(pBurnSoundOut, nBurnSoundLen);
#endif
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029736;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);
		DACScan(nAction, pnMin);

		SCAN_VAR(flip);
		SCAN_VAR(palettebnk);
		SCAN_VAR(scrolly);
		SCAN_VAR(irqmask);
	}

	return 0;
}

static struct BurnSampleInfo CheekymsSampleDesc[] = {
#ifdef USE_SAMPLE_HACK
#if !defined ROM_VERIFY
	{ "squeek", SAMPLE_NOLOOP },
	{ "hammer", SAMPLE_NOLOOP },
	{ "squeekdead", SAMPLE_NOLOOP },
	{ "two", SAMPLE_NOLOOP },
	{ "squeek2", SAMPLE_NOLOOP },
#endif
#endif
	{ "", 0 }
};

STD_SAMPLE_PICK(Cheekyms)
STD_SAMPLE_FN(Cheekyms)

// Cheeky Mouse

static struct BurnRomInfo cheekymsRomDesc[] = {
	{ "cm03.c5",	0x0800, 0x1ad0cb40, 1 }, //  0 maincpu
	{ "cm04.c6",	0x0800, 0x2238f607, 1 }, //  1
	{ "cm05.c7",	0x0800, 0x4169eba8, 1 }, //  2
	{ "cm06.c8",	0x0800, 0x7031660c, 1 }, //  3

	{ "cm01.c1",	0x0800, 0x26f73bd7, 2 }, //  4 gfx1
	{ "cm02.c2",	0x0800, 0x885887c3, 2 }, //  5

	{ "cm07.n5",	0x0800, 0x2738c88d, 3 }, //  6 gfx2
	{ "cm08.n6",	0x0800, 0xb3fbd4ac, 3 }, //  7

	{ "cm.m9",	0x0020, 0xdb9c59a5, 4 }, //  8 proms
	{ "cm.m8",	0x0020, 0x2386bc68, 4 }, //  9
	{ "cm.p3",	0x0020, 0x6ac41516, 4 }, // 10
};

STD_ROM_PICK(cheekyms)
STD_ROM_FN(cheekyms)

struct BurnDriver BurnDrvCheekyms = {
	"cheekyms", NULL, NULL, "cheekyms", "1980",
	"Cheeky Mouse\0", NULL, "Universal", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, cheekymsRomInfo, cheekymsRomName, CheekymsSampleInfo, CheekymsSampleName, CheekymsInputInfo, CheekymsDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	192, 256, 3, 4
};

