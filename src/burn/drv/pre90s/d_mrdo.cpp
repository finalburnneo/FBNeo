// FB Alpha Mr. Do! driver module
// Based on MAME driver by Nicola Salmoria

#include "tiles_generic.h"
#include "z80_intf.h"
#include "sn76496.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;

static UINT8 *DrvROM;
static UINT8 *DrvRAM;
static UINT8 *DrvFGVidRAM;
static UINT8 *DrvBGVidRAM;
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
static UINT8 DrvDips[2];

static INT32 flipscreen, scroll_x, scroll_y;

static struct BurnInputInfo DrvInputList[] = {
	{"P1 Coin"      , BIT_DIGITAL  , DrvJoy2 + 6,	"p1 coin"  },
	{"P1 Start"     , BIT_DIGITAL  , DrvJoy1 + 5,	"p1 start" },
	{"P1 Up"	    , BIT_DIGITAL  , DrvJoy1 + 3,   "p1 up"    },
	{"P1 Down"      , BIT_DIGITAL  , DrvJoy1 + 1,   "p1 down"  },
	{"P1 Left"      , BIT_DIGITAL  , DrvJoy1 + 0, 	"p1 left"  },
	{"P1 Right"     , BIT_DIGITAL  , DrvJoy1 + 2, 	"p1 right" },
	{"P1 Button 1"  , BIT_DIGITAL  , DrvJoy1 + 4,	"p1 fire 1"},

	{"P2 Coin"      , BIT_DIGITAL  , DrvJoy2 + 7,	"p2 coin"  },
	{"P2 Start"     , BIT_DIGITAL  , DrvJoy1 + 6,	"p2 start" },
	{"P2 Up"	    , BIT_DIGITAL  , DrvJoy2 + 3,   "p2 up"    },
	{"P2 Down"      , BIT_DIGITAL,   DrvJoy2 + 1,   "p2 down", },
	{"P2 Left"      , BIT_DIGITAL  , DrvJoy2 + 0, 	"p2 left"  },
	{"P2 Right"     , BIT_DIGITAL  , DrvJoy2 + 2, 	"p2 right" },
	{"P2 Button 1"  , BIT_DIGITAL  , DrvJoy2 + 4,	"p2 fire 1"},

	{"Tilt"         , BIT_DIGITAL  , DrvJoy1 + 7,	"tilt"     },

	{"Reset",	  BIT_DIGITAL  , &DrvReset,	"reset"        },
	{"Dip 1",	  BIT_DIPSWITCH, DrvDips + 0,	"dip 1"	   },
	{"Dip 2",	  BIT_DIPSWITCH, DrvDips + 1,	"dip 2"	   },
};

STDINPUTINFO(Drv)

static struct BurnDIPInfo DrvDIPList[]=
{
	// Default Values
	{0x10, 0xff, 0xff, 0xdf, NULL                 },

	{0   , 0xfe, 0   , 4   , "Difficulty"         },
	{0x10, 0x01, 0x03, 0x03, "Easy"     		  },
	{0x10, 0x01, 0x03, 0x02, "Medium"	          },
	{0x10, 0x01, 0x03, 0x01, "Hard"     		  },
	{0x10, 0x01, 0x03, 0x00, "Hardest"	          },

	{0   , 0xfe, 0   , 2   , "Rack Test (cheat)"  },
	{0x10, 0x01, 0x04, 0x04, "Off"       		  },
	{0x10, 0x01, 0x04, 0x00, "On"       		  },

	{0   , 0xfe, 0   , 2   , "Special"            },
	{0x10, 0x01, 0x08, 0x08, "Easy"			      },
	{0x10, 0x01, 0x08, 0x00, "Hard"			      },

	{0   , 0xfe, 0   , 2   , "Extra"              },
	{0x10, 0x01, 0x10, 0x10, "Easy"         	  },
	{0x10, 0x01, 0x10, 0x00, "Hard"    		      },

	{0   , 0xfe, 0   , 2   , "Cabinet" 	          },
	{0x10, 0x01, 0x20, 0x00, "Upright"		      },
	{0x10, 0x01, 0x20, 0x20, "Cocktail"    		  },

	{0   , 0xfe, 0   , 4   , "Lives" 	          },
	{0x10, 0x01, 0xc0, 0x00, "2"	        	  },
	{0x10, 0x01, 0xc0, 0xc0, "3"    		      },
	{0x10, 0x01, 0xc0, 0x80, "4"    		      },
	{0x10, 0x01, 0xc0, 0x40, "5"    		      },

	// Default Values
	{0x11, 0xff, 0xff, 0xff, NULL                 },

	{0   , 0xfe, 0   , 11  , "Coin B" 	          },
	{0x11, 0x01, 0x0f, 0x06, "4 Coins / 1 Credit"                  },
	{0x11, 0x01, 0x0f, 0x08, "3 Coins / 1 Credit"                  },
	{0x11, 0x01, 0x0f, 0x0a, "2 Coins / 1 Credit"                  },
	{0x11, 0x01, 0x0f, 0x07, "3 Coins / 2 Credits"                 },
	{0x11, 0x01, 0x0f, 0x0f, "1 Coin / 1 Credit"                   },
	{0x11, 0x01, 0x0f, 0x09, "2 Coins / 3 Credits"                 },
	{0x11, 0x01, 0x0f, 0x0e, "1 Coin / 2 Credits"                  },
	{0x11, 0x01, 0x0f, 0x0d, "1 Coin / 3 Credits"                  },
	{0x11, 0x01, 0x0f, 0x0c, "1 Coin / 4 Credits"                  },
	{0x11, 0x01, 0x0f, 0x0b, "1 Coin / 5 Credits"                  },
	{0x11, 0x01, 0x0f, 0x00, "Free_Play"                           },

	{0   , 0xfe, 0   , 11  , "Coin A" 	                           },
	{0x11, 0x01, 0xf0, 0x60, "4 Coins / 1 Credit"                  },
	{0x11, 0x01, 0xf0, 0x80, "3 Coins / 1 Credit"                  },
	{0x11, 0x01, 0xf0, 0xa0, "2 Coins / 1 Credit"                  },
	{0x11, 0x01, 0xf0, 0x70, "3 Coins / 2 Credits"                 },
	{0x11, 0x01, 0xf0, 0xf0, "1 Coin / 1 Credit"                   },
	{0x11, 0x01, 0xf0, 0x90, "2 Coins / 3 Credits"                 },
	{0x11, 0x01, 0xf0, 0xe0, "1 Coin / 2 Credits"                  },
	{0x11, 0x01, 0xf0, 0xd0, "1 Coin / 3 Credits"                  },
	{0x11, 0x01, 0xf0, 0xc0, "1 Coin / 4 Credits"                  },
	{0x11, 0x01, 0xf0, 0xb0, "1 Coin / 5 Credits"                  },
	{0x11, 0x01, 0xf0, 0x00, "Free_Play"                           },
};

STDDIPINFO(Drv)

void __fastcall mrdo_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf000) == 0xf000) address &= 0xf800;

	switch (address)
	{
		case 0x9800:
			flipscreen = data & 1;
		break;

		case 0x9801: 
			SN76496Write(0, data);
		break;

		case 0x9802:		
			SN76496Write(1, data);
		break;

		case 0xf000:
			scroll_x = data;
		break;

		case 0xf800:
			scroll_y = data ^ (flipscreen ? 0xff : 0);
		break;
	}
}

UINT8 __fastcall mrdo_read(UINT16 address)
{
	UINT8 ret = 0xff;

	switch (address)
	{
		case 0x9803: // Protection
			return DrvROM[ZetHL(-1)];

		case 0xa000:
		{
			for (INT32 i = 0; i < 8; i++) ret ^= DrvJoy1[i] << i;
			return ret;
		}

		case 0xa001:
		{
			for (INT32 i = 0; i < 8; i++) ret ^= DrvJoy2[i] << i;
			return ret;
		}

		case 0xa002:
			return DrvDips[0];

		case 0xa003:
			return DrvDips[1];
	}

	return 0;
}

static INT32 DrvDoReset()
{
	DrvReset = 0;

	memset (AllRam, 0, RamEnd - AllRam);

	flipscreen = 0;
	scroll_x = scroll_y = 0;

	ZetOpen(0);
	ZetReset();
	ZetClose();

	HiscoreReset();

	return 0;
}

static void mrdo_palette_init()
{
	const UINT8 *color_prom = Prom;
	int i;

	const int R1 = 150;
	const int R2 = 120;
	const int R3 = 100;
	const int R4 = 75;
	const int pull = 220;
	float pot[16];
	int weight[16];
	const float potadjust = 0.7f;

	for (i = 0x0f; i >= 0; i--)
	{
		float par = 0;

		if (i & 1) par += 1.0f/(float)R1;
		if (i & 2) par += 1.0f/(float)R2;
		if (i & 4) par += 1.0f/(float)R3;
		if (i & 8) par += 1.0f/(float)R4;
		if (par)
		{
			par = 1/par;
			pot[i] = pull/(pull+par) - potadjust;
		}
		else pot[i] = 0;

		weight[i] = (INT32)(0xff * pot[i] / pot[0x0f]);
		if (weight[i] < 0) weight[i] = 0;
	}

	for (i = 0; i < 0x100; i++)
	{
		int a1,a2;
		int bits0, bits2;
		int r, g, b;

		a1 = ((i >> 3) & 0x1c) + (i & 0x03) + 0x20;
		a2 = ((i >> 0) & 0x1c) + (i & 0x03);

		bits0 = (color_prom[a1] >> 0) & 0x03;
		bits2 = (color_prom[a2] >> 0) & 0x03;
		r = weight[bits0 + (bits2 << 2)];

		bits0 = (color_prom[a1] >> 2) & 0x03;
		bits2 = (color_prom[a2] >> 2) & 0x03;
		g = weight[bits0 + (bits2 << 2)];

		bits0 = (color_prom[a1] >> 4) & 0x03;
		bits2 = (color_prom[a2] >> 4) & 0x03;
		b = weight[bits0 + (bits2 << 2)];

		Palette[i] = BurnHighCol(r, g, b, 0);
	}

	color_prom += 0x40;

	for (i = 0x100; i < 0x140; i++)
	{
		UINT8 ctabentry = color_prom[(i - 0x100) & 0x1f];

		if ((i - 0x100) & 0x20)
			ctabentry >>= 4; 
		else
			ctabentry &= 0x0f;

		Palette[i] = Palette[ctabentry + ((ctabentry & 0x0c) << 3)];
	}
}

static void mrdo_gfx_decode()
{
	static INT32 CharPlane[2]  = { 0, 0x8000 };
	static INT32 CharXOffs[8]  = { 7, 6, 5, 4, 3, 2, 1, 0 };
	static INT32 CharYOffs[8]  = { 0, 8, 16, 24, 32, 40, 48, 56 };

	static INT32 SpriPlane[2]  = { 4, 0 };
	static INT32 SpriXOffs[16] = { 3, 2, 1, 0, 11, 10, 9, 8, 19, 18, 17, 16, 27, 26, 25, 24 };
	static INT32 SpriYOffs[16] = { 0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 480 };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x2000);
	if (!tmp) return;

	memcpy (tmp, Gfx0, 0x2000);

	GfxDecode(0x200, 2,  8,  8, CharPlane, CharXOffs, CharYOffs, 0x040, tmp, Gfx0);

	memcpy (tmp, Gfx1, 0x2000);

	GfxDecode(0x200, 2,  8,  8, CharPlane, CharXOffs, CharYOffs, 0x040, tmp, Gfx1);

	memcpy (tmp, Gfx2, 0x2000);

	GfxDecode(0x080, 2, 16, 16, SpriPlane, SpriXOffs, SpriYOffs, 0x200, tmp, Gfx2);

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
	DrvFGVidRAM     = Next; Next += 0x00800;
	DrvBGVidRAM     = Next; Next += 0x00800;
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
		for (INT32 i = 0; i < 4; i++) {
			if(BurnLoadRom(DrvROM + (i * 0x2000),  0 + i, 1)) return 1;
			if(BurnLoadRom(Prom + (i * 0x0020), 10 + i, 1)) return 1;
		}

		for (INT32 i = 0; i < 2; i++) {
			if(BurnLoadRom(Gfx0 + (i * 0x1000),  4 + i, 1)) return 1;
			if(BurnLoadRom(Gfx1 + (i * 0x1000),  6 + i, 1)) return 1;
			if(BurnLoadRom(Gfx2 + (i * 0x1000),  8 + i, 1)) return 1;
		}

		mrdo_palette_init();
		mrdo_gfx_decode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetSetReadHandler(mrdo_read);
	ZetSetWriteHandler(mrdo_write);
	ZetMapMemory(DrvROM,            0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvBGVidRAM,       0x8000, 0x87ff, MAP_RAM);
	ZetMapMemory(DrvFGVidRAM,       0x8800, 0x8fff, MAP_RAM);
	ZetMapMemory(DrvSpriteRAM,      0x9000, 0x90ff, MAP_RAM);
	ZetMapMemory(DrvRAM,            0xe000, 0xefff, MAP_RAM);
	ZetClose();

	SN76489Init(0, 4000000, 0);
	SN76489Init(1, 4000000, 1);
	SN76496SetRoute(0, 0.45, BURN_SND_ROUTE_BOTH);
	SN76496SetRoute(1, 0.45, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
 
	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	ZetExit();
	SN76496Exit();
	GenericTilesExit();

	BurnFree(AllMem);

	flipscreen = scroll_x = scroll_y = 0;

	return 0;
}

static void draw_sprites()
{
	for (INT32 offs = 0x100 - 4; offs >= 0; offs -= 4)
	{
		if (DrvSpriteRAM[offs + 1])
		{
			INT32 sx = DrvSpriteRAM[offs + 3];
			INT32 sy = DrvSpriteRAM[offs + 1] ^ 0xff;

			INT32 code = DrvSpriteRAM[offs] & 0x7f;
			INT32 color = DrvSpriteRAM[offs + 2] & 0x0f;

			INT32 flipx = DrvSpriteRAM[offs + 2] & 0x10;
			INT32 flipy = DrvSpriteRAM[offs + 2] & 0x20;

			sx -= 8;
			sy -= 31; // sy is offset by 1 pixel from the bg/fg

			if (sx < 0 || sy < -7 || sx >= nScreenWidth || sy >= nScreenHeight) continue;

			if (flipy) {
				if (flipx) {
					Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0x100, Gfx2);
				} else {
					Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0x100, Gfx2);
				}
			} else {
				if (flipx) {
					Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0x100, Gfx2);
				} else {
					Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0x100, Gfx2);
				}
			}
		}
	}
}

static void draw_8x8_tiles(UINT8 *vram, UINT8 *gfx_base, INT32 scrollx, INT32 scrolly)
{
	for (INT32 offs = 0; offs < 0x400; offs++)
	{
		INT32 sx = (offs & 0x1f) << 3;
		INT32 sy = (offs >> 2) & 0xf8; 

		INT32 code = (vram[0x400 + offs] | ((vram[offs] & 0x80) << 1)) & 0x1ff;
		INT32 color = vram[offs] & 0x3f;
		INT32 forcelayer0 = vram[offs] & 0x40;

		sx = (sx - scrollx) & 0xff;
		sy = (sy - scrolly) & 0xff;

		sx -= 8;
		sy -= 32;

		if (sx < 0 || sy < -7 || sx >= nScreenWidth || sy >= nScreenHeight) continue;

		if (forcelayer0) {
			Render8x8Tile_Clip(pTransDraw, code, sx, sy, color, 2, 0, gfx_base);
		} else {
			Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, gfx_base);
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		mrdo_palette_init();
		DrvRecalc = 0;
	}

	BurnTransferClear();

	if (nBurnLayer & 2) draw_8x8_tiles(DrvBGVidRAM, Gfx1, scroll_x, scroll_y);
	if (nBurnLayer & 4) draw_8x8_tiles(DrvFGVidRAM, Gfx0, 0, 0);
	if (nBurnLayer & 8) draw_sprites();

	BurnTransferCopy(Palette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	INT32 nInterleave = 10;
	INT32 nCyclesTotal = 4000000 / 60;

	ZetOpen(0);
	for (INT32 i = 0; i < nInterleave; i++) {
		ZetRun(nCyclesTotal / nInterleave);

		if (i == nInterleave-1)
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
	}
	ZetClose();

	if (pBurnDraw) {
		DrvDraw();
	}
	
	if (pBurnSoundOut) {
		SN76496Update(0, pBurnSoundOut, nBurnSoundLen);
		SN76496Update(1, pBurnSoundOut, nBurnSoundLen);
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

		SCAN_VAR(flipscreen);
		SCAN_VAR(scroll_x);
		SCAN_VAR(scroll_y);
	}

	return 0;
}


// Mr. Do!

static struct BurnRomInfo mrdoRomDesc[] = {
	{ "a4-01.bin",    0x2000, 0x03dcfba2, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 Code
	{ "c4-02.bin",    0x2000, 0x0ecdd39c, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "e4-03.bin",    0x2000, 0x358f5dc2, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "f4-04.bin",    0x2000, 0xf4190cfc, 1 | BRF_ESS | BRF_PRG }, //  3

	{ "s8-09.bin",    0x1000, 0xaa80c5b6, 2 | BRF_GRA },	       //  4 FG Tiles
	{ "u8-10.bin",    0x1000, 0xd20ec85b, 2 | BRF_GRA },	       //  5

	{ "r8-08.bin",    0x1000, 0xdbdc9ffa, 3 | BRF_GRA },	       //  6 BG Tiles
	{ "n8-07.bin",    0x1000, 0x4b9973db, 3 | BRF_GRA },	       //  7

	{ "h5-05.bin",    0x1000, 0xe1218cc5, 4 | BRF_GRA },	       //  8 Sprite Tiles
	{ "k5-06.bin",    0x1000, 0xb1f68b04, 4 | BRF_GRA },	       //  9

	{ "u02--2.bin",   0x0020, 0x238a65d7, 5 | BRF_GRA },	       // 10 Palette (high bits)
	{ "t02--3.bin",   0x0020, 0xae263dc0, 5 | BRF_GRA },	       // 11 Palette (low bits)
	{ "f10--1.bin",   0x0020, 0x16ee4ca2, 5 | BRF_GRA },	       // 12 Sprite color lookup table
	{ "j10--4.bin",   0x0020, 0xff7fe284, 5 | BRF_GRA },	       // 13 Timing (not used)
};

STD_ROM_PICK(mrdo)
STD_ROM_FN(mrdo)

struct BurnDriver BurnDrvmrdo = {
	"mrdo", NULL, NULL, NULL, "1982",
	"Mr. Do!\0", NULL, "Universal", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, mrdoRomInfo, mrdoRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x140,
	192, 240, 3, 4
};


// Mr. Do! (Taito license)

static struct BurnRomInfo mrdotRomDesc[] = {
	{ "d1",           0x2000, 0x3dcd9359, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 Code
	{ "d2",           0x2000, 0x710058d8, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "d3",           0x2000, 0x467d12d8, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "d4",           0x2000, 0xfce9afeb, 1 | BRF_ESS | BRF_PRG }, //  3

	{ "d9",           0x1000, 0xde4cfe66, 2 | BRF_GRA },	       //  4 FG Tiles
	{ "d10",          0x1000, 0xa6c2f38b, 2 | BRF_GRA },	       //  5

	{ "r8-08.bin",    0x1000, 0xdbdc9ffa, 3 | BRF_GRA },	       //  6 BG Tiles
	{ "n8-07.bin",    0x1000, 0x4b9973db, 3 | BRF_GRA },	       //  7

	{ "h5-05.bin",    0x1000, 0xe1218cc5, 4 | BRF_GRA },	       //  8 Sprite Tiles
	{ "k5-06.bin",    0x1000, 0xb1f68b04, 4 | BRF_GRA },	       //  9

	{ "u02--2.bin",   0x0020, 0x238a65d7, 5 | BRF_GRA },	       // 10 Palette (high bits)
	{ "t02--3.bin",   0x0020, 0xae263dc0, 5 | BRF_GRA },	       // 11 Palette (low bits)
	{ "f10--1.bin",   0x0020, 0x16ee4ca2, 5 | BRF_GRA },	       // 12 Sprite color lookup table
	{ "j10--4.bin",   0x0020, 0xff7fe284, 5 | BRF_GRA },	       // 13 Timing (not used)
};

STD_ROM_PICK(mrdot)
STD_ROM_FN(mrdot)

struct BurnDriver BurnDrvmrdot = {
	"mrdot", "mrdo", NULL, NULL, "1982",
	"Mr. Do! (Taito license)\0", NULL, "Universal (Taito license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, mrdotRomInfo, mrdotRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x140,
	192, 240, 3, 4
};

// Mr. Do! (bugfixed)

static struct BurnRomInfo mrdofixRomDesc[] = {
	{ "d1",           0x2000, 0x3dcd9359, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 Code
	{ "d2",           0x2000, 0x710058d8, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "dofix.d3",     0x2000, 0x3a7d039b, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "dofix.d4",     0x2000, 0x32db845f, 1 | BRF_ESS | BRF_PRG }, //  3

	{ "d9",           0x1000, 0xde4cfe66, 2 | BRF_GRA },	       //  4 FG Tiles
	{ "d10",          0x1000, 0xa6c2f38b, 2 | BRF_GRA },	       //  5

	{ "r8-08.bin",    0x1000, 0xdbdc9ffa, 3 | BRF_GRA },	       //  6 BG Tiles
	{ "n8-07.bin",    0x1000, 0x4b9973db, 3 | BRF_GRA },	       //  7

	{ "h5-05.bin",    0x1000, 0xe1218cc5, 4 | BRF_GRA },	       //  8 Sprite Tiles
	{ "k5-06.bin",    0x1000, 0xb1f68b04, 4 | BRF_GRA },	       //  9

	{ "u02--2.bin",   0x0020, 0x238a65d7, 5 | BRF_GRA },	       // 10 Palette (high bits)
	{ "t02--3.bin",   0x0020, 0xae263dc0, 5 | BRF_GRA },	       // 11 Palette (low bits)
	{ "f10--1.bin",   0x0020, 0x16ee4ca2, 5 | BRF_GRA },	       // 12 Sprite color lookup table
	{ "j10--4.bin",   0x0020, 0xff7fe284, 5 | BRF_GRA },	       // 13 Timing (not used)
};

STD_ROM_PICK(mrdofix)
STD_ROM_FN(mrdofix)

struct BurnDriver BurnDrvmrdofix = {
	"mrdofix", "mrdo", NULL, NULL, "1982",
	"Mr. Do! (bugfixed)\0", NULL, "Universal (Taito license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, mrdofixRomInfo, mrdofixRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x140,
	192, 240, 3, 4
};


// Mr. Lo!

static struct BurnRomInfo mrloRomDesc[] = {
	{ "mrlo01.bin",   0x2000, 0x6f455e7d, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 Code
	{ "d2",           0x2000, 0x710058d8, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "dofix.d3",     0x2000, 0x3a7d039b, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "mrlo04.bin",   0x2000, 0x49c10274, 1 | BRF_ESS | BRF_PRG }, //  3

	{ "mrlo09.bin",   0x1000, 0xfdb60d0d, 2 | BRF_GRA },	       //  4 FG Tiles
	{ "mrlo10.bin",   0x1000, 0x0492c10e, 2 | BRF_GRA },	       //  5

	{ "r8-08.bin",    0x1000, 0xdbdc9ffa, 3 | BRF_GRA },	       //  6 BG Tiles
	{ "n8-07.bin",    0x1000, 0x4b9973db, 3 | BRF_GRA },	       //  7

	{ "h5-05.bin",    0x1000, 0xe1218cc5, 4 | BRF_GRA },	       //  8 Sprite Tiles
	{ "k5-06.bin",    0x1000, 0xb1f68b04, 4 | BRF_GRA },	       //  9

	{ "u02--2.bin",   0x0020, 0x238a65d7, 5 | BRF_GRA },	       // 10 Palette (high bits)
	{ "t02--3.bin",   0x0020, 0xae263dc0, 5 | BRF_GRA },	       // 11 Palette (low bits)
	{ "f10--1.bin",   0x0020, 0x16ee4ca2, 5 | BRF_GRA },	       // 12 Sprite color lookup table
	{ "j10--4.bin",   0x0020, 0xff7fe284, 5 | BRF_GRA },	       // 13 Timing (not used)
};

STD_ROM_PICK(mrlo)
STD_ROM_FN(mrlo)

struct BurnDriver BurnDrvmrlo = {
	"mrlo", "mrdo", NULL, NULL, "1982",
	"Mr. Lo!\0", NULL, "Bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, mrloRomInfo, mrloRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x140,
	192, 240, 3, 4
};


// Mr. Du!

static struct BurnRomInfo mrduRomDesc[] = {
	{ "d1",           0x2000, 0x3dcd9359, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 Code
	{ "d2",           0x2000, 0x710058d8, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "d3",           0x2000, 0x467d12d8, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "du4.bin",      0x2000, 0x893bc218, 1 | BRF_ESS | BRF_PRG }, //  3

	{ "du9.bin",      0x1000, 0x4090dcdc, 2 | BRF_GRA },	       //  4 FG Tiles
	{ "du10.bin",     0x1000, 0x1e63ab69, 2 | BRF_GRA },	       //  5

	{ "r8-08.bin",    0x1000, 0xdbdc9ffa, 3 | BRF_GRA },	       //  6 BG Tiles
	{ "n8-07.bin",    0x1000, 0x4b9973db, 3 | BRF_GRA },	       //  7

	{ "h5-05.bin",    0x1000, 0xe1218cc5, 4 | BRF_GRA },	       //  8 Sprite Tiles
	{ "k5-06.bin",    0x1000, 0xb1f68b04, 4 | BRF_GRA },	       //  9

	{ "u02--2.bin",   0x0020, 0x238a65d7, 5 | BRF_GRA },	       // 10 Palette (high bits)
	{ "t02--3.bin",   0x0020, 0xae263dc0, 5 | BRF_GRA },	       // 11 Palette (low bits)
	{ "f10--1.bin",   0x0020, 0x16ee4ca2, 5 | BRF_GRA },	       // 12 Sprite color lookup table
	{ "j10--4.bin",   0x0020, 0xff7fe284, 5 | BRF_GRA },	       // 13 Timing (not used)
};

STD_ROM_PICK(mrdu)
STD_ROM_FN(mrdu)

struct BurnDriver BurnDrvmrdu = {
	"mrdu", "mrdo", NULL, NULL, "1982",
	"Mr. Du!\0", NULL, "Bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, mrduRomInfo, mrduRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x140,
	192, 240, 3, 4
};


// Mr. Do! (prototype)

static struct BurnRomInfo mrdoyRomDesc[] = {
	{ "dosnow.1",     0x2000, 0xd3454e2c, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 Code
	{ "dosnow.2",     0x2000, 0x5120a6b2, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "dosnow.3",     0x2000, 0x96416dbe, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "dosnow.4",     0x2000, 0xc05051b6, 1 | BRF_ESS | BRF_PRG }, //  3

	{ "dosnow.9",     0x1000, 0x85d16217, 2 | BRF_GRA },	       //  4 FG Tiles
	{ "dosnow.10",    0x1000, 0x61a7f54b, 2 | BRF_GRA },	       //  5

	{ "dosnow.8",     0x1000, 0x2bd1239a, 3 | BRF_GRA },	       //  6 BG Tiles
	{ "dosnow.7",     0x1000, 0xac8ffddf, 3 | BRF_GRA },	       //  7

	{ "dosnow.5",     0x1000, 0x7662d828, 4 | BRF_GRA },	       //  8 Sprite Tiles
	{ "dosnow.6",     0x1000, 0x413f88d1, 4 | BRF_GRA },	       //  9

	{ "u02--2.bin",   0x0020, 0x238a65d7, 5 | BRF_GRA },	       // 10 Palette (high bits)
	{ "t02--3.bin",   0x0020, 0xae263dc0, 5 | BRF_GRA },	       // 11 Palette (low bits)
	{ "f10--1.bin",   0x0020, 0x16ee4ca2, 5 | BRF_GRA },	       // 12 Sprite color lookup table
	{ "j10--4.bin",   0x0020, 0xff7fe284, 5 | BRF_GRA },	       // 13 Timing (not used)
};

STD_ROM_PICK(mrdoy)
STD_ROM_FN(mrdoy)

struct BurnDriver BurnDrvmrdoy = {
	"mrdoy", "mrdo", NULL, NULL, "1982",
	"Mr. Do! (prototype)\0", NULL, "Universal", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, mrdoyRomInfo, mrdoyRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x140,
	192, 240, 3, 4
};


// Yankee DO!

static struct BurnRomInfo yankeedoRomDesc[] = {
	{ "a4-01.bin",    0x2000, 0x03dcfba2, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 Code
	{ "yd_d2.c4",     0x2000, 0x7c9d7ce0, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "e4-03.bin",    0x2000, 0x358f5dc2, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "f4-04.bin",    0x2000, 0xf4190cfc, 1 | BRF_ESS | BRF_PRG }, //  3

	{ "s8-09.bin",    0x1000, 0xaa80c5b6, 2 | BRF_GRA },	       //  4 FG Tiles
	{ "u8-10.bin",    0x1000, 0xd20ec85b, 2 | BRF_GRA },	       //  5

	{ "r8-08.bin",    0x1000, 0xdbdc9ffa, 3 | BRF_GRA },	       //  6 BG Tiles
	{ "n8-07.bin",    0x1000, 0x4b9973db, 3 | BRF_GRA },	       //  7

	{ "yd_d5.h5",     0x1000, 0xf530b79b, 4 | BRF_GRA },	       //  8 Sprite Tiles
	{ "yd_d6.k5",     0x1000, 0x790579aa, 4 | BRF_GRA },	       //  9

	{ "u02--2.bin",   0x0020, 0x238a65d7, 5 | BRF_GRA },	       // 10 Palette (high bits)
	{ "t02--3.bin",   0x0020, 0xae263dc0, 5 | BRF_GRA },	       // 11 Palette (low bits)
	{ "f10--1.bin",   0x0020, 0x16ee4ca2, 5 | BRF_GRA },	       // 12 Sprite color lookup table
	{ "j10--4.bin",   0x0020, 0xff7fe284, 5 | BRF_GRA },	       // 13 Timing (not used)
};

STD_ROM_PICK(yankeedo)
STD_ROM_FN(yankeedo)

struct BurnDriver BurnDrvyankeedo = {
	"yankeedo", "mrdo", NULL, NULL, "1982",
	"Yankee DO!\0", NULL, "hack", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, yankeedoRomInfo, yankeedoRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x140,
	192, 240, 3, 4
};

