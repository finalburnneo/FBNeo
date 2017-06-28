// FB Alpha Mr. Jong driver module
// Based on MAME driver by Takahiro Nogi (nogi@kt.rim.or.jp) 2000/03/20

#include "tiles_generic.h"
#include "z80_intf.h"
#include "sn76496.h"

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvInputs[2];
static UINT8 DrvDip[2];
static UINT8 DrvReset;
static UINT8 DrvRecalc;

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvRAM0;
static UINT8 *DrvRAM1;
static UINT8 *DrvBgVidRAM;
static UINT8 *DrvBgColRAM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxTMP0;
static UINT8 *DrvColPROM;
static UINT8 *DrvMainROM;
static UINT32 *DrvPalette;

static UINT8 flipscreen;

static struct BurnInputInfo MrjongInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 fire 1"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDip + 0,	"dip"},
};

STDINPUTINFO(Mrjong)


static struct BurnDIPInfo MrjongDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x01, NULL		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0f, 0x01, 0x01, 0x01, "Upright"		},
	{0x0f, 0x01, 0x01, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x0f, 0x01, 0x02, 0x00, "Off"		},
	{0x0f, 0x01, 0x02, 0x02, "On"		},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x0f, 0x01, 0x04, 0x00, "30k"		},
	{0x0f, 0x01, 0x04, 0x04, "50k"		},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x0f, 0x01, 0x08, 0x00, "Normal"		},
	{0x0f, 0x01, 0x08, 0x08, "Hard"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0f, 0x01, 0x30, 0x00, "3"		},
	{0x0f, 0x01, 0x30, 0x10, "4"		},
	{0x0f, 0x01, 0x30, 0x20, "5"		},
	{0x0f, 0x01, 0x30, 0x30, "6"		},

	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{0x0f, 0x01, 0xc0, 0xc0, "2 Coins 1 Credits"		},
	{0x0f, 0x01, 0xc0, 0x00, "1 Coin  1 Credits"		},
	{0x0f, 0x01, 0xc0, 0x40, "1 Coin  2 Credits"		},
	{0x0f, 0x01, 0xc0, 0x80, "1 Coin  3 Credits"		},
};

STDDIPINFO(Mrjong)

static void DrvPaletteInit()
{
	UINT32 t_pal[16];

	for (INT32 i = 0; i < 0x10; i++)
	{
		int bit0, bit1, bit2;
		int r, g, b;

		/* red component */
		bit0 = (DrvColPROM[i] >> 0) & 0x01;
		bit1 = (DrvColPROM[i] >> 1) & 0x01;
		bit2 = (DrvColPROM[i] >> 2) & 0x01;
		r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		/* green component */
		bit0 = (DrvColPROM[i] >> 3) & 0x01;
		bit1 = (DrvColPROM[i] >> 4) & 0x01;
		bit2 = (DrvColPROM[i] >> 5) & 0x01;
		g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		/* blue component */
		bit0 = 0;
		bit1 = (DrvColPROM[i] >> 6) & 0x01;
		bit2 = (DrvColPROM[i] >> 7) & 0x01;
		b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		t_pal[i] = BurnHighCol(r,g,b,0);
	}

	/* prom now points to the beginning of the lookup table */
	UINT8 *prom = DrvColPROM + 0x20;

	/* characters/sprites */
	for (INT32 i = 0; i < 0x80; i++)
	{
		UINT8 ctabentry = prom[i] & 0x0f;
		DrvPalette[i] = t_pal[ctabentry];
	}
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvMainROM	 	= Next; Next += 0x10000;

	AllRam			= DrvMainROM + 0x8000;
	DrvRAM0			= DrvMainROM + 0x8000;
	DrvRAM1			= DrvMainROM + 0xa000;
	DrvBgVidRAM	    = DrvMainROM + 0xe000;
	DrvBgColRAM	    = DrvMainROM + 0xe400;
	RamEnd			= DrvMainROM + 0x10000;

	DrvColPROM		= Next; Next += 0x00120;
	DrvPalette		= (UINT32*)Next; Next += 0x00120 * sizeof(UINT32);
	DrvGfxTMP0		= Next; Next += 0x02000;
	DrvGfxROM0		= Next; Next += 0x01000*8;
	DrvGfxROM1		= Next; Next += 0x01000*8;

	MemEnd			= Next;

	return 0;
}

static void DrvGfxDecode()
{

	UINT8 *tmp = (UINT8*)BurnMalloc(0x2000);
	if (tmp == NULL) {
		return;
	}

	memcpy (tmp, DrvGfxTMP0, 0x2000);

	INT32 Planes0[2] = { 0, 512*8*8 };			/* the two bitplanes are separated */
	INT32 XOffs0[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };	/* pretty straightforward layout */
	INT32 YOffs0[8] = { 7*8, 6*8, 5*8, 4*8, 3*8, 2*8, 1*8, 0*8 };

	GfxDecode(0x0200, 2,  8,  8, Planes0, XOffs0, YOffs0, 0x040, tmp, DrvGfxROM0); // modulo 0x040 to verify !!!

	INT32 Planes1[2] = { 0, 128*16*16 };		/* the bitplanes are separated */
	INT32 XOffs1[16] = { 8*8+0, 8*8+1, 8*8+2, 8*8+3, 8*8+4, 8*8+5, 8*8+6, 8*8+7, 0, 1, 2, 3, 4, 5, 6, 7 };	/* pretty straightforward layout */
	INT32 YOffs1[16] = { 23*8, 22*8, 21*8, 20*8, 19*8, 18*8, 17*8, 16*8,	7*8, 6*8, 5*8, 4*8, 3*8, 2*8, 1*8, 0*8 };

	GfxDecode(0x0080, 2, 16, 16, Planes1, XOffs1, YOffs1, 0x100, tmp, DrvGfxROM1);

	BurnFree (tmp);
}

static void __fastcall mrjong_write(unsigned short address, unsigned char data)
{
	DrvMainROM[address] = data;
}

static unsigned char __fastcall mrjong_read(unsigned short address)
{
	return DrvMainROM[address];
}

UINT8 __fastcall mrjong_in(UINT16 address)
{
	switch (address & 0xff)
	{
		case 0x00:
		{
			return DrvInputs[0] | 0x80;
		}

		case 0x01:
		{
			return DrvInputs[1];
		}

		case 0x02:
		{
			return DrvDip[0];
		}

		case 0x03:
			return 0x00;
	}
	return 0;
}

void __fastcall mrjong_out(UINT16 address, UINT8 data)
{
	switch (address & 0xff)
	{
		case 0x00:
			flipscreen = ((data & 0x04) > 2);
		break;
		
		case 0x01:
			SN76496Write(0, data);
		break;
		
		case 0x02:
			SN76496Write(1, data);
		break;
	}
}

static void DrawSprites()
{

	for (INT32 offs = (0x40 - 4); offs >= 0; offs -= 4)	{
		int code;
		int color;
		int sx, sy;
		int flipx, flipy;

		code = (((DrvBgVidRAM[offs + 1] >> 2) & 0x3f) | ((DrvBgVidRAM[offs + 3] & 0x20) << 1));
		flipx = !((DrvBgVidRAM[offs + 1] & 0x01) >> 0);
		flipy = !((DrvBgVidRAM[offs + 1] & 0x02) >> 1);
		color = (DrvBgVidRAM[offs + 3] & 0x1f);

		sx = DrvBgVidRAM[offs + 2] + 16;
		sy = 240-DrvBgVidRAM[offs + 0];

		if (flipscreen)	{
			sx = 208 - sx;
			sy = 240 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		sy -= 16;
		sx -= 16;

 		if (flipy) {
			if (flipx) {
				Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 2 /*2 bits*/, 0, 0, DrvGfxROM1);
			} else {
				Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 2 /*2 bits*/, 0, 0, DrvGfxROM1);
			}
		} else {
			if (flipx) {
				Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 2 /*2 bits*/, 0, 0, DrvGfxROM1);
			} else {
				Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 2 /*2 bits*/, 0, 0, DrvGfxROM1);
			}
		}
	}
}

static void DrawBgTiles()
{
	for (INT32 offs = 0x400 - 1;offs >= 0;offs--) {
		INT32  sx,sy,code,color,flipx,flipy;

		sx = offs % 32;
		sy = offs / 32;
		code = DrvBgVidRAM[offs] | ((DrvBgColRAM[offs] & 0x20) << 3);
		code &= 0x1ff;
		color = DrvBgColRAM[offs] & 0x1f;
		flipx = !(DrvBgColRAM[offs] & 0x40); //TILEMAP_SCAN_ROWS_FLIPXY (inverted flipxy)
		flipy = !(DrvBgColRAM[offs] & 0x80);

		if (flipscreen) {
			sx = 31 - sx;
			sy = 31 - sy;
			flipx = !flipx;
		}

		sy -= 2;
		sx -= 2;

		if (flipy) {
			if (flipx) {
				Render8x8Tile_FlipXY_Clip(pTransDraw, code, sx*8, sy*8, color, 2 /*2 bits*/, 0, DrvGfxROM0);
			} else {
				Render8x8Tile_FlipY_Clip(pTransDraw, code, sx*8, sy*8, color, 2 /*2 bits*/, 0, DrvGfxROM0);
			}
		} else {
			if (flipx) {
				Render8x8Tile_FlipX_Clip(pTransDraw, code, sx*8, sy*8, color, 2 /*2 bits*/, 0, DrvGfxROM0);
			} else {
				Render8x8Tile_Clip(pTransDraw, code, sx*8, sy*8, color, 2 /*2 bits*/, 0, DrvGfxROM0);
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	DrawBgTiles();
	DrawSprites();
	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);
	flipscreen = 0;

	ZetOpen(0);
	ZetReset();
	ZetClose();

	return 0;
}

static INT32 DrvLoadRoms()
{
	for (INT32 i = 0; i < 4; i++)
		if (BurnLoadRom(DrvMainROM + i * 0x2000, i +  0, 1)) return 1;

	if (BurnLoadRom(DrvGfxTMP0 + 0x00000, 4, 1)) return 1;	// mj21
	if (BurnLoadRom(DrvGfxTMP0 + 0x01000, 5, 1)) return 1;	// mj20

	if (BurnLoadRom(DrvColPROM + 0x0000, 6, 1)) return 1;	// mj61
	if (BurnLoadRom(DrvColPROM + 0x0020, 7, 1)) return 1;	// mj60

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
	if(DrvLoadRoms()) return 1;

	DrvPaletteInit();
	DrvGfxDecode();

	ZetInit(0);
	ZetOpen(0);
	ZetSetInHandler(mrjong_in);
	ZetSetOutHandler(mrjong_out);

	ZetMapMemory(DrvMainROM,    0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvRAM0,       0x8000, 0x87ff, MAP_RAM);
	ZetMapMemory(DrvRAM1,       0xa000, 0xa7ff, MAP_RAM);
	ZetMapMemory(DrvBgVidRAM,   0xe000, 0xe3ff, MAP_RAM);
	ZetMapMemory(DrvBgColRAM,   0xe400, 0xe7ff, MAP_RAM);

 	ZetSetWriteHandler(mrjong_write);
	ZetSetReadHandler(mrjong_read);

	ZetClose();
	
	SN76489Init(0, 2578000, 0);
	SN76489Init(1, 2578000, 1);

	SN76496SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);
	SN76496SetRoute(1, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	ZetExit();

	SN76496Exit();

	BurnFree (AllMem);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	memset (DrvInputs, 0x00, 2);

	for (INT32 i = 0; i < 8; i++) {
		DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
		DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
	}

	INT32 nCyclesTotal = 2578000 / 60;

	ZetOpen(0);
	ZetRun(nCyclesTotal);
	ZetNmi();
	ZetClose();

	if (pBurnSoundOut) {
		SN76496Update(0, pBurnSoundOut, nBurnSoundLen);
		SN76496Update(1, pBurnSoundOut, nBurnSoundLen);
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
		*pnMin = 0x029702;
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

		SN76496Scan(nAction, pnMin);

		SCAN_VAR(flipscreen);
	}

	return 0;
}

// Mr. Jong (Japan)

static struct BurnRomInfo mrjongRomDesc[] = {
	{ "mj00",	0x2000, 0xd211aed3, 1 }, //  0 maincpu
	{ "mj01",	0x2000, 0x49a9ca7e, 1 }, //  1
	{ "mj02",	0x2000, 0x4b50ae6a, 1 }, //  2
	{ "mj03",	0x2000, 0x2c375a17, 1 }, //  3

	{ "mj21",	0x1000, 0x1ea99dab, 2 }, //  4 gfx1
	{ "mj20",	0x1000, 0x7eb1d381, 2 }, //  5

	{ "mj61",	0x0020, 0xa85e9b27, 3 }, //  6 proms
	{ "mj60",	0x0100, 0xdd2b304f, 3 }, //  7
};

STD_ROM_PICK(mrjong)
STD_ROM_FN(mrjong)


static struct BurnRomInfo crazyblkRomDesc[] = {
	{ "c1.a6",	0x2000, 0xe2a211a2, 1 }, //  0 maincpu
	{ "c2.a7",	0x2000, 0x75070978, 1 }, //  1
	{ "c3.a7",	0x2000, 0x696ca502, 1 }, //  2
	{ "c4.a8",	0x2000, 0xc7f5a247, 1 }, //  3

	{ "c6.h5",	0x1000, 0x2b2af794, 2 }, //  4 gfx1
	{ "c5.h4",	0x1000, 0x98d13915, 2 }, //  5

	{ "clr.j7",	0x0020, 0xee1cf1d5, 3 }, //  6 proms
	{ "clr.g5",	0x0100, 0xbcb1e2e3, 3 }, //  7
};

STD_ROM_PICK(crazyblk)
STD_ROM_FN(crazyblk)

static struct BurnRomInfo blkbustrRomDesc[] = {
	{ "6a.bin",	0x2000, 0x9e4b426c, 1 }, //  0 maincpu
	{ "c2.a7",	0x2000, 0x75070978, 1 }, //  1
	{ "8a.bin",	0x2000, 0x0e803777, 1 }, //  2
	{ "c4.a8",	0x2000, 0xc7f5a247, 1 }, //  3

	{ "4h.bin",	0x1000, 0x67dd6c19, 2 }, //  4 gfx1
	{ "5h.bin",	0x1000, 0x50fba1d4, 2 }, //  5

	{ "clr.j7",	0x0020, 0xee1cf1d5, 3 }, //  6 proms
	{ "clr.g5",	0x0100, 0xbcb1e2e3, 3 }, //  7
};

STD_ROM_PICK(blkbustr)
STD_ROM_FN(blkbustr)

struct BurnDriver BurnDrvMrjong = {
	"mrjong", NULL, NULL, NULL, "1983",
	"Mr. Jong (Japan)\0", NULL, "Kiwako", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, mrjongRomInfo, mrjongRomName, NULL, NULL, MrjongInputInfo, MrjongDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 512,
	224, 240, 3, 4
};

struct BurnDriver BurnDrvCrazyblk = {
	"crazyblk", "mrjong", NULL, NULL, "1983",
	"Crazy Blocks\0", NULL, "Kiwako (ECI license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, crazyblkRomInfo, crazyblkRomName, NULL, NULL, MrjongInputInfo, MrjongDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 512,
	224, 240, 3, 4
};

struct BurnDriver BurnDrvBlkbustr = {
	"blkbustr", "mrjong", NULL, NULL, "1983",
	"BlockBuster\0", NULL, "Kiwako (ECI license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, blkbustrRomInfo, blkbustrRomName, NULL, NULL, MrjongInputInfo, MrjongDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 512,
	224, 240, 3, 4
};
