// FB Alpha Gun.Smoke driver module
// Based on MAME driver by Paul Leaman

#include "tiles_generic.h"
#include "z80_intf.h"
#include "burn_ym2203.h"

static UINT8 *Mem, *MemEnd, *Rom0, *Rom1, *Ram;
static UINT8 *Gfx0, *Gfx1, *Gfx2, *Gfx3, *Prom;
static UINT8 DrvJoy1[8], DrvJoy2[8], DrvJoy3[8], DrvDips[2], DrvReset;
static UINT32 *DrvPalette;
static UINT8 DrvCalcPal;
static UINT8 *SprTrnsp;

static UINT8 soundlatch;
static UINT8 flipscreen;
static INT32 nGunsmokeBank;

static UINT8 sprite3bank;
static UINT8 chon, bgon, objon;
static UINT8 gunsmoke_scrollx[2], gunsmoke_scrolly;

static struct BurnInputInfo DrvInputList[] = {
	{"P1 Coin"      , BIT_DIGITAL  , DrvJoy1 + 6,	"p1 coin"  },
	{"P1 start"     , BIT_DIGITAL  , DrvJoy1 + 0,	"p1 start" },
	{"P1 Right"     , BIT_DIGITAL  , DrvJoy2 + 0, "p1 right" },
	{"P1 Left"      , BIT_DIGITAL  , DrvJoy2 + 1, "p1 left"  },
	{"P1 Down"      ,	BIT_DIGITAL  , DrvJoy2 + 2, "p1 down"  },
	{"P1 Up"        ,	BIT_DIGITAL  , DrvJoy2 + 3, "p1 up"    },
	{"P1 Button 1"  , BIT_DIGITAL  , DrvJoy2 + 4,	"p1 fire 1"},
	{"P1 Button 2"  , BIT_DIGITAL  , DrvJoy2 + 5,	"p1 fire 2"},
	{"P1 Button 3"  , BIT_DIGITAL  , DrvJoy2 + 6,	"p1 fire 3"},

	{"P2 Coin"      , BIT_DIGITAL  , DrvJoy1 + 7,	"p2 coin"  },
	{"P2 start"     , BIT_DIGITAL  , DrvJoy1 + 1,	"p2 start" },
	{"P2 Right"     , BIT_DIGITAL  , DrvJoy3 + 0, "p2 right" },
	{"P2 Left"      , BIT_DIGITAL  , DrvJoy3 + 1, "p2 left"  },
	{"P2 Down"      ,	BIT_DIGITAL  , DrvJoy3 + 2, "p2 down"  },
	{"P2 Up"        ,	BIT_DIGITAL  , DrvJoy3 + 3, "p2 up"    },
	{"P2 Button 1"  , BIT_DIGITAL  , DrvJoy3 + 4,	"p2 fire 1"},
	{"P2 Button 2"  , BIT_DIGITAL  , DrvJoy3 + 5,	"p2 fire 2"},
	{"P2 Button 3"  , BIT_DIGITAL  , DrvJoy3 + 6,	"p2 fire 3"},

	{"Service"      ,	BIT_DIGITAL  , DrvJoy1 + 4,	"service"  },

	{"Reset"        ,	BIT_DIGITAL  , &DrvReset  ,	"reset"    },
	{"Dip 1"        ,	BIT_DIPSWITCH, DrvDips + 0,	"dip"	     },
	{"Dip 2"        ,	BIT_DIPSWITCH, DrvDips + 1,	"dip"	     },

};

STDINPUTINFO(Drv)

static struct BurnDIPInfo DrvDIPList[]=
{
	// Default Values
	{0x14, 0xff, 0xff, 0xf7, NULL               },

	{0   , 0xfe, 0   , 4   , "Bonus Life"       },
	{0x14, 0x01, 0x03, 0x01, "30k 80k 80k+"     },
	{0x14, 0x01, 0x03, 0x03, "30k 100k 100k+"	  },
	{0x14, 0x01, 0x03, 0x00, "30k 100k 150k+"	  },
	{0x14, 0x01, 0x03, 0x02, "30k 100k"    		  },

	{0   , 0xfe, 0   , 2   , "Demo"             },
	{0x14, 0x01, 0x04, 0x00, "Off"     		      },
	{0x14, 0x01, 0x04, 0x04, "On"    		        },
};

static struct BurnDIPInfo gunsmokeuDIPList[]=
{
	// Default Values
	{0x14, 0xff, 0xff, 0xf7, NULL               },

	{0   , 0xfe, 0   , 4   , "Bonus Life"       },
	{0x14, 0x01, 0x03, 0x01, "30k 80k 80k+"     },
	{0x14, 0x01, 0x03, 0x03, "30k 100k 100k+"	  },
	{0x14, 0x01, 0x03, 0x00, "30k 100k 150k+"	  },
	{0x14, 0x01, 0x03, 0x02, "30k 100k"    		  },

	{0   , 0xfe, 0   , 2   , "Lifes"            },
	{0x14, 0x01, 0x04, 0x04, "3"     		        },
	{0x14, 0x01, 0x04, 0x00, "5"    		        },
};

static struct BurnDIPInfo gunsmokeDIPList[]=
{
	{0   , 0xfe, 0   , 2   , "Cabinet"          },
	{0x14, 0x01, 0x08, 0x00, "Upright"     		  },
	{0x14, 0x01, 0x08, 0x08, "Cocktail"    		  },

	{0   , 0xfe, 0   , 4   , "Difficulty"       },
	{0x14, 0x01, 0x30, 0x20, "Easy"     		    },
	{0x14, 0x01, 0x30, 0x30, "Normal"    		    },
	{0x14, 0x01, 0x30, 0x10, "Difficult"   		  },
	{0x14, 0x01, 0x30, 0x00, "Very Difficult"	  },

	{0   , 0xfe, 0   , 2   , "Freeze"           },
	{0x14, 0x01, 0x40, 0x40, "Off"     		      },
	{0x14, 0x01, 0x40, 0x00, "On"    		        },

	{0   , 0xfe, 0   , 2   , "Service Mode"     },
	{0x14, 0x01, 0x80, 0x80, "Off"     		      },
	{0x14, 0x01, 0x80, 0x00, "On"    		        },

	// Default Values
	{0x15, 0xff, 0xff, 0xff, NULL               },

	{0   , 0xfe, 0   , 8  ,  "Coin A"           },
	{0x15, 0x01, 0x07, 0x00, "4 Coins 1 Credit" },
	{0x15, 0x01, 0x07, 0x01, "3 Coins 1 Credit"	},
	{0x15, 0x01, 0x07, 0x02, "2 Coins 1 Credit"	},
	{0x15, 0x01, 0x07, 0x07, "1 Coin 1 Credit"  },
	{0x15, 0x01, 0x07, 0x06, "1 Coin 2 Credits" },
	{0x15, 0x01, 0x07, 0x05, "1 Coin 3 Credits"	},
	{0x15, 0x01, 0x07, 0x04, "1 Coin 4 Credits"	},
	{0x15, 0x01, 0x07, 0x03, "1 Coin 6 Credits" },

	{0   , 0xfe, 0   , 8  ,  "Coin B"           },
	{0x15, 0x01, 0x38, 0x00, "4 Coins 1 Credit" },
	{0x15, 0x01, 0x38, 0x08, "3 Coins 1 Credit"	},
	{0x15, 0x01, 0x38, 0x10, "2 Coins 1 Credit"	},
	{0x15, 0x01, 0x38, 0x38, "1 Coin 1 Credit"  },
	{0x15, 0x01, 0x38, 0x30, "1 Coin 2 Credits" },
	{0x15, 0x01, 0x38, 0x28, "1 Coin 3 Credits"	},
	{0x15, 0x01, 0x38, 0x20, "1 Coin 4 Credits"	},
	{0x15, 0x01, 0x38, 0x18, "1 Coin 6 Credits" },

	{0   , 0xfe, 0   , 2   , "Allow Continue"   },
	{0x15, 0x01, 0x40, 0x00, "No"    		        },
	{0x15, 0x01, 0x40, 0x40, "Yes"     		      },

	{0   , 0xfe, 0   , 2   , "Demo Sounds"      },
	{0x15, 0x01, 0x80, 0x00, "Off"    		      },
	{0x15, 0x01, 0x80, 0x80, "On"     		      },
};

STDDIPINFOEXT(Drv, Drv, gunsmoke)
STDDIPINFOEXT(gunsmokeu, gunsmokeu, gunsmoke)


static inline void gunsmoke_bankswitch(INT32 nBank)
{
	if (nGunsmokeBank != nBank) {
		nGunsmokeBank = nBank;

		ZetMapArea(0x8000, 0xbfff, 0, Rom0 + 0x10000 + nBank * 0x04000);
		ZetMapArea(0x8000, 0xbfff, 2, Rom0 + 0x10000 + nBank * 0x04000);
	}
}

void __fastcall gunsmoke_cpu0_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xc800:
			soundlatch = data;
		break;

		case 0xc804:
			gunsmoke_bankswitch((data >> 2) & 3);

			flipscreen = data & 0x40;
			chon       = data & 0x80;
		break;

		case 0xc806:
		break;

		case 0xd800:
		case 0xd801:
			gunsmoke_scrollx[address & 1] = data;
		break;

		case 0xd802:
		case 0xd803:
			gunsmoke_scrolly = data;
		break;

		case 0xd806:
			sprite3bank = data & 0x07;
			
			bgon        = data & 0x10;
			objon       = data & 0x20;
		break;
	}
}

UINT8 __fastcall gunsmoke_cpu0_read(UINT16 address)
{
	UINT8 ret = 0xff;

	switch (address)
	{
		case 0xc000:
		{
			for (INT32 i = 0; i < 8; i++)
				ret ^= DrvJoy1[i] << i;

			return ret | 0x08;
		}

		case 0xc001:
		{
			for (INT32 i = 0; i < 8; i++)
				ret ^= DrvJoy2[i] << i;

			return ret;
		}

		case 0xc002:
		{
			for (INT32 i = 0; i < 8; i++)
				ret ^= DrvJoy3[i] << i;

			return ret;
		}

		case 0xc003: // dips
			return DrvDips[0];

		case 0xc004:
			return DrvDips[1];

		// reads at c4c9 - c4cb are part of some sort of protection or bug
		case 0xc4c9:
			return 0xff;
		case 0xc4ca:
		case 0xc4cb:
			return 0;
	}

	return 0;
}

void __fastcall gunsmoke_cpu1_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xe000: // control 0
			BurnYM2203Write(0, 0, data);
		break;

		case 0xe001: // write 0
			BurnYM2203Write(0, 1, data);
		break;

		case 0xe002: // control 1
			BurnYM2203Write(1, 0, data);
		break;

		case 0xe003: // write 1
			BurnYM2203Write(1, 1, data);
		break;
	}
}

UINT8 __fastcall gunsmoke_cpu1_read(UINT16 address)
{
	if (address == 0xc800) return soundlatch;

	return 0;
}


static INT32 DrvDoReset()
{
	DrvReset = 0;

	memset (Ram, 0, 0x4000);

	nGunsmokeBank = -1;
	soundlatch = 0;
	flipscreen = 0;

	sprite3bank = 0;
	chon = bgon = objon = 0;
	gunsmoke_scrollx[0] = gunsmoke_scrollx[1] = 0;
	gunsmoke_scrolly = 0;

	ZetOpen(0);
	ZetReset();
	gunsmoke_bankswitch(0);
	ZetClose();

	ZetOpen(1);
	ZetReset();
	ZetClose();

	BurnYM2203Reset();

	HiscoreReset();

	return 0;
}

static INT32 DrvPaletteInit()
{
	UINT32 tmp[0x100];

	for (INT32 i = 0; i < 0x100; i++)
	{
		UINT8 r  = Prom[i + 0x000] & 0x0f;
		UINT8 g  = Prom[i + 0x100] & 0x0f;
		UINT8 b  = Prom[i + 0x200] & 0x0f;

		tmp[i] = BurnHighCol((r*16)+r,(g*16)+g,(b*16)+b,0);
	}

	for (INT32 i = 0; i < 0x100; i++)
	{
		DrvPalette[0x000 + i] = tmp[Prom[0x300 + i] | 0x40];
		DrvPalette[0x100 + i] = tmp[Prom[0x400 + i] | ((Prom[0x500 + i] & 0x03) << 4)];
		DrvPalette[0x200 + i] = tmp[Prom[0x600 + i] | ((Prom[0x700 + i] & 0x07) << 4) | 0x80];
	}

	return 0;
}

static INT32 gunsmoke_gfx_decode()
{
	UINT8 *tmp = (UINT8*)BurnMalloc(0x80000);
	if (!tmp) return 1;

	static INT32 Planes[4]     = { 0x100004, 0x100000, 4, 0 };

	static INT32 CharXOffs[8]  = { 11, 10, 9, 8, 3, 2, 1, 0 };
	static INT32 CharYOffs[8]  = { 112, 96, 80, 64, 48, 32, 16, 0 };

	static INT32 TileXOffs[32] = {    0,    1,    2,    3,    8,    9,   10,   11,
				      512,  513,  514,  515,  520,  521,  522,  523,
				     1024, 1025, 1026, 1027, 1032, 1033, 1034, 1035,
				     1536, 1537, 1538, 1539, 1544, 1545, 1546, 1547 };

	static INT32 TileYOffs[32] = {   0,  16,  32,  48,  64,  80,  96, 112,
				     128, 144, 160, 176, 192, 208, 224, 240,
				     256, 272, 288, 304, 320, 336, 352, 368,
				     384, 400, 416, 432, 448, 464, 480, 496 };

	static INT32 SpriXOffs[16] = {   0,   1,   2,   3,   8,   9,  10,  11,
				     256, 257, 258, 259, 264, 265, 266, 267 };

	memcpy (tmp, Gfx0, 0x04000);
	GfxDecode(0x400, 2,  8,  8, Planes + 2, CharXOffs, CharYOffs, 0x080, tmp, Gfx0);

	memcpy (tmp, Gfx1, 0x40000);
	GfxDecode(0x200, 4, 32, 32, Planes + 0, TileXOffs, TileYOffs, 0x800, tmp, Gfx1);

	memcpy (tmp, Gfx2, 0x40000);
	GfxDecode(0x800, 4, 16, 16, Planes + 0, SpriXOffs, TileYOffs, 0x200, tmp, Gfx2);

	BurnFree (tmp);

	{
		memset (SprTrnsp, 1, 0x800);

		for (INT32 i = 0; i < 0x80000; i++)
			if (Gfx2[i]) SprTrnsp[i >> 8] = 0;
	}

	return 0;
}

static INT32 gunsmokeSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)ZetTotalCycles() * nSoundRate / 3000000;
}

static double gunsmokeGetTime()
{
	return (double)ZetTotalCycles() / 3000000;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = Mem;

	Rom0           = Next; Next += 0x20000;
	Rom1           = Next; Next += 0x08000;
	Ram            = Next; Next += 0x04000;
	Gfx0           = Next; Next += 0x10000;
	Gfx1           = Next; Next += 0x80000;
	Gfx2           = Next; Next += 0x80000;
	Gfx3           = Next; Next += 0x08000;
	Prom           = Next; Next += 0x00800;

	SprTrnsp       = Next; Next += 0x00800;

	DrvPalette     = (UINT32*)Next; Next += 0x00300 * sizeof(UINT32);

	MemEnd         = Next;

	return 0;
}

static INT32 DrvInit()
{
	INT32 nLen;

	Mem = NULL;
	MemIndex();
	nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(Mem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Rom0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(Rom0 + 0x10000,  1, 1)) return 1;
		if (BurnLoadRom(Rom0 + 0x18000,  2, 1)) return 1;

		if (BurnLoadRom(Rom1 + 0x00000,  3, 1)) return 1;

		if (BurnLoadRom(Gfx0 + 0x00000,  4, 1)) return 1;

		if (BurnLoadRom(Gfx3 + 0x00000, 21, 1)) return 1;

		for (INT32 i = 0; i < 8; i++) {
			if (BurnLoadRom(Gfx1 + i * 0x8000,  5 + i, 1)) return 1;
			if (BurnLoadRom(Gfx2 + i * 0x8000, 13 + i, 1)) return 1;
			if (BurnLoadRom(Prom + i * 0x0100, 22 + i, 1)) return 1;
		}

		gunsmoke_gfx_decode();
		DrvPaletteInit();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapArea(0x0000, 0x7fff, 0, Rom0 + 0x00000);
	ZetMapArea(0x0000, 0x7fff, 2, Rom0 + 0x00000);
	ZetMapArea(0x8000, 0xbfff, 0, Rom0 + 0x10000);
	ZetMapArea(0x8000, 0xbfff, 2, Rom0 + 0x10000);
	ZetMapArea(0xd000, 0xd7ff, 0, Ram  + 0x00000);
	ZetMapArea(0xd000, 0xd7ff, 1, Ram  + 0x00000);
	ZetMapArea(0xe000, 0xefff, 0, Ram  + 0x01000);
	ZetMapArea(0xe000, 0xefff, 1, Ram  + 0x01000);
	ZetMapArea(0xe000, 0xefff, 2, Ram  + 0x01000);
	ZetMapArea(0xf000, 0xffff, 0, Ram  + 0x02000);
	ZetMapArea(0xf000, 0xffff, 1, Ram  + 0x02000);
	ZetSetReadHandler(gunsmoke_cpu0_read);
	ZetSetWriteHandler(gunsmoke_cpu0_write);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapArea(0x0000, 0x7fff, 0, Rom1 + 0x00000);
	ZetMapArea(0x0000, 0x7fff, 2, Rom1 + 0x00000);
	ZetMapArea(0xc000, 0xc7ff, 0, Ram  + 0x03000);
	ZetMapArea(0xc000, 0xc7ff, 1, Ram  + 0x03000);
	ZetMapArea(0xc000, 0xc7ff, 2, Ram  + 0x03000);
	ZetSetReadHandler(gunsmoke_cpu1_read);
	ZetSetWriteHandler(gunsmoke_cpu1_write);
	ZetClose();

	GenericTilesInit();

	BurnYM2203Init(2, 1500000, NULL, gunsmokeSynchroniseStream, gunsmokeGetTime, 0);
	BurnTimerAttachZet(3000000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE, 0.14, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.22, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.22, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.22, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_YM2203_ROUTE, 0.14, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_1, 0.22, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_2, 0.22, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_3, 0.22, BURN_SND_ROUTE_BOTH);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	BurnYM2203Exit();

	BurnFree (Mem);

	Mem = MemEnd = Rom0 = Rom1 = Ram = NULL;
	Gfx0 = Gfx1 = Gfx2 = Gfx3 = Prom = NULL;
	SprTrnsp = NULL;
	DrvPalette = NULL;

	soundlatch = flipscreen = nGunsmokeBank = 0;

	sprite3bank = chon = bgon = objon = 0;
	gunsmoke_scrollx[0] = gunsmoke_scrollx[1] = 0;
	gunsmoke_scrolly = 0;

	return 0;
}

static void draw_bg_layer()
{
	UINT16 scroll = gunsmoke_scrollx[0] + (gunsmoke_scrollx[1] << 8);

 	UINT8 *tilerom = Gfx3 + ((scroll >> 1) & ~0x0f);

	for (INT32 offs = 0; offs < 0x50; offs++)
	{
		INT32 attr = tilerom[1];
		INT32 code = tilerom[0] + ((attr & 1) << 8);
		INT32 color = (attr & 0x3c) >> 2;
		INT32 flipy = attr & 0x80;
		INT32 flipx = attr & 0x40;

		INT32 sy = (offs & 7) << 5;
		INT32 sx = (offs >> 3) << 5;

		sy -= gunsmoke_scrolly;
		sx -= (scroll & 0x1f);

		if (flipscreen) {
			flipy ^= 0x80;
			flipx ^= 0x40;

			sy = 224 - sy;
			sx = 224 - sx;
		}

		sy -= 16;

		if (flipy) {
			if (flipx) {
				Render32x32Tile_FlipXY_Clip(pTransDraw, code, sx, sy, color, 4, 0x100, Gfx1);
			} else {
				Render32x32Tile_FlipY_Clip(pTransDraw,  code, sx, sy, color, 4, 0x100, Gfx1);
			}
		} else {
			if (flipx) {
				Render32x32Tile_FlipX_Clip(pTransDraw,  code, sx, sy, color, 4, 0x100, Gfx1);
			} else {
				Render32x32Tile_Clip(pTransDraw,        code, sx, sy, color, 4, 0x100, Gfx1);
			}
		}

		tilerom += 2;
	}
}

static void draw_fg_layer()
{
	for (INT32 offs = 0; offs < 0x400; offs++)
	{
		INT32 sx = (offs << 3) & 0xf8;
		INT32 sy = (offs >> 2) & 0xf8;

		INT32 attr = Ram[0x0400 + offs];
		INT32 code = Ram[0x0000 + offs] + ((attr & 0xe0) << 2);
		INT32 color = attr & 0x1f;

		if (code == 0x0024) continue;

		UINT8 *src = Gfx0 + (code << 6);
		color <<= 2;

		if (flipscreen) {
			sy = 240 - sy;
			sx = 240 - sx;

			sy -= 8;

			for (INT32 y = sy + 7; y >= sy; y--)
			{
				for (INT32 x = sx + 7; x >= sx; x--, src++)
				{
					if (y < 0 || x < 0 || y > 223 || x > 255) continue;
					if (!DrvPalette[color|*src]) continue;

					pTransDraw[(y << 8) | x] = color | *src;
				}
			}
		} else {
			sy -= 16;

			for (INT32 y = sy; y < sy + 8; y++)
			{
				for (INT32 x = sx; x < sx + 8; x++, src++)
				{
					if (y < 0 || x < 0 || y > 223 || x > 255) continue;
					if (!DrvPalette[color|*src]) continue;

					pTransDraw[(y << 8) | x] = color | *src;
				}
			}
		}
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0x1000 - 32; offs >= 0; offs -= 32)
	{
		INT32 attr = Ram[0x2001 + offs];
		INT32 bank = (attr & 0xc0) >> 6;
		INT32 code = Ram[0x2000 + offs];
		INT32 color = attr & 0x0f;
		INT32 flipx = 0;
		INT32 flipy = attr & 0x10;
		INT32 sx = Ram[0x2003 + offs] - ((attr & 0x20) << 3);
		INT32 sy = Ram[0x2002 + offs];

		if (sy == 0 || sy > 0xef) continue;

		if (bank == 3) bank += sprite3bank;
		code += 256 * bank;

		if (SprTrnsp[code]) continue;

		if (flipscreen)
		{
			sx = 240 - sx;
			sy = 240 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		sy -= 16;

		if (flipy) {
			if (flipx) {
				Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0x200, Gfx2);
			} else {
				Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0x200, Gfx2);
			}
		} else {
			if (flipx) {
				Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0x200, Gfx2);
			} else {
				Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0x200, Gfx2);
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvCalcPal) {
		DrvPaletteInit();
		DrvCalcPal = 0;
	}

	if (!bgon || ~nBurnLayer & 1)   BurnTransferClear();
	if (bgon  && nBurnLayer & 1)    draw_bg_layer();
	if (objon && nSpriteEnable & 1) draw_sprites();
	if (chon  && nBurnLayer & 2)    draw_fg_layer();

	BurnTransferCopy(DrvPalette);

	return 0;
}


static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();

	INT32 nInterleave = 256;

	INT32 nCyclesSegment;
	INT32 nCyclesDone[2] = { 0, 0 };
	INT32 nCyclesTotal[2] = { 4000000 / 60, 3000000 / 60 };

	for (INT32 i = 0; i < nInterleave; i++) {
		INT32 nCurrentCPU, nNext;

		// Run Z80 #0
		nCurrentCPU = 0;
		ZetOpen(nCurrentCPU);
		nNext = (i + 1) * nCyclesTotal[nCurrentCPU] / nInterleave;
		nCyclesSegment = nNext - nCyclesDone[nCurrentCPU];
		nCyclesDone[nCurrentCPU] += ZetRun(nCyclesSegment);
		if (i == 240) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		// Run Z80 #1
		nCurrentCPU = 1;
		ZetOpen(nCurrentCPU);
		BurnTimerUpdate((i + 1) * (nCyclesTotal[nCurrentCPU] / nInterleave));
		if (i%64 == 63) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();
	}
	
	ZetOpen(1);
	BurnTimerEndFrame(nCyclesTotal[1]);
	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
	}
	ZetClose();
	
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

		ba.Data	  = Ram;
		ba.nLen	  = 0x4000;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);
		BurnYM2203Scan(nAction, pnMin);

		// Scan critical driver variables
		SCAN_VAR(soundlatch);
		SCAN_VAR(flipscreen);
		SCAN_VAR(nGunsmokeBank);
		SCAN_VAR(sprite3bank);
		SCAN_VAR(chon);
		SCAN_VAR(bgon);
		SCAN_VAR(objon);
		SCAN_VAR(gunsmoke_scrollx);
		SCAN_VAR(gunsmoke_scrolly);

		if (nAction & ACB_WRITE) {
			INT32 banktemp = nGunsmokeBank;
			ZetOpen(0);
			gunsmoke_bankswitch(0);
			gunsmoke_bankswitch(banktemp);
			ZetClose();
		}
	}

	return 0;
}


// Gun.Smoke (World, 851115)

static struct BurnRomInfo gunsmokeRomDesc[] = {
	{ "gs03.09n", 	  0x8000, 0x40a06cef, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "gs04.10n", 	  0x8000, 0x8d4b423f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gs05.12n", 	  0x8000, 0x2b5667fb, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "gs02.14h", 	  0x8000, 0xcd7a2c38, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "gs01.11f", 	  0x4000, 0xb61ece9b, 3 | BRF_GRA },	       //  4 Character Tiles

	{ "gs13.06c", 	  0x8000, 0xf6769fc5, 4 | BRF_GRA },	       //  5 32x32 Tiles
	{ "gs12.05c", 	  0x8000, 0xd997b78c, 4 | BRF_GRA },	       //  6
	{ "gs11.04c", 	  0x8000, 0x125ba58e, 4 | BRF_GRA },	       //  7
	{ "gs10.02c", 	  0x8000, 0xf469c13c, 4 | BRF_GRA },	       //  8
	{ "gs09.06a", 	  0x8000, 0x539f182d, 4 | BRF_GRA },	       //  9
	{ "gs08.05a", 	  0x8000, 0xe87e526d, 4 | BRF_GRA },	       // 10
	{ "gs07.04a", 	  0x8000, 0x4382c0d2, 4 | BRF_GRA },	       // 11 
	{ "gs06.02a", 	  0x8000, 0x4cafe7a6, 4 | BRF_GRA },	       // 12 

	{ "gs22.06n", 	  0x8000, 0xdc9c508c, 5 | BRF_GRA },	       // 13 Sprites
	{ "gs21.04n", 	  0x8000, 0x68883749, 5 | BRF_GRA },	       // 14
	{ "gs20.03n", 	  0x8000, 0x0be932ed, 5 | BRF_GRA },	       // 15
	{ "gs19.01n", 	  0x8000, 0x63072f93, 5 | BRF_GRA },	       // 16
	{ "gs18.06l", 	  0x8000, 0xf69a3c7c, 5 | BRF_GRA },	       // 17
	{ "gs17.04l", 	  0x8000, 0x4e98562a, 5 | BRF_GRA },	       // 18
	{ "gs16.03l", 	  0x8000, 0x0d99c3b3, 5 | BRF_GRA },	       // 19
	{ "gs15.01l", 	  0x8000, 0x7f14270e, 5 | BRF_GRA },	       // 20

	{ "gs14.11c", 	  0x8000, 0x0af4f7eb, 6 | BRF_GRA },	       // 21 Background Tilemaps

	{ "g-01.03b", 	  0x0100, 0x02f55589, 7 | BRF_GRA },	       // 22 Color Proms
	{ "g-02.04b", 	  0x0100, 0xe1e36dd9, 7 | BRF_GRA },	       // 23
	{ "g-03.05b", 	  0x0100, 0x989399c0, 7 | BRF_GRA },	       // 24
	{ "g-04.09d", 	  0x0100, 0x906612b5, 7 | BRF_GRA },	       // 25
	{ "g-06.14a", 	  0x0100, 0x4a9da18b, 7 | BRF_GRA },	       // 26
	{ "g-07.15a", 	  0x0100, 0xcb9394fc, 7 | BRF_GRA },	       // 27
	{ "g-09.09f", 	  0x0100, 0x3cee181e, 7 | BRF_GRA },	       // 28
	{ "g-08.08f", 	  0x0100, 0xef91cdd2, 7 | BRF_GRA },	       // 29

	{ "g-10.02j", 	  0x0100, 0x0eaf5158, 0 | BRF_OPT },	       // 30 Video Timing
	{ "g-05.01f", 	  0x0100, 0x25c90c2a, 0 | BRF_OPT },	       // 31 Priority
};

STD_ROM_PICK(gunsmoke)
STD_ROM_FN(gunsmoke)

struct BurnDriver BurnDrvGunsmoke = {
	"gunsmoke", NULL, NULL, NULL, "1985",
	"Gun.Smoke (World, 851115)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_VERSHOOT, 0,
	NULL, gunsmokeRomInfo, gunsmokeRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvCalcPal, 0x300,
	224, 256, 3, 4
};


// Gun.Smoke (World, 851115)(bootleg)
// based on world version, warning message patched out

static struct BurnRomInfo gunsmokebRomDesc[] = {
	{ "3.ic85", 	  0x8000, 0xae6f4b75, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "4.ic86", 	  0x8000, 0x8d4b423f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "5.ic87", 	  0x8000, 0x2b5667fb, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "2.ic41", 	  0x8000, 0xcd7a2c38, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "1.ic39", 	  0x4000, 0xb61ece9b, 3 | BRF_GRA },	       //  4 Character Tiles

	{ "13.ic21", 	  0x8000, 0xf6769fc5, 4 | BRF_GRA },	       //  5 32x32 Tiles
	{ "12.ic20", 	  0x8000, 0xd997b78c, 4 | BRF_GRA },	       //  6
	{ "11.ic19", 	  0x8000, 0x125ba58e, 4 | BRF_GRA },	       //  7
	{ "10.ic18", 	  0x8000, 0xf469c13c, 4 | BRF_GRA },	       //  8
	{ "9.ic04", 	  0x8000, 0x539f182d, 4 | BRF_GRA },	       //  9
	{ "8.ic03", 	  0x8000, 0xe87e526d, 4 | BRF_GRA },	       // 10
	{ "7.ic02", 	  0x8000, 0x4382c0d2, 4 | BRF_GRA },	       // 11 
	{ "6.ic01", 	  0x8000, 0x4cafe7a6, 4 | BRF_GRA },	       // 12 

	{ "22.ic134", 	  0x8000, 0xdc9c508c, 5 | BRF_GRA },	       // 13 Sprites
	{ "21.ic133", 	  0x8000, 0x68883749, 5 | BRF_GRA },	       // 14
	{ "20.ic132", 	  0x8000, 0x0be932ed, 5 | BRF_GRA },	       // 15
	{ "19.ic131", 	  0x8000, 0x63072f93, 5 | BRF_GRA },	       // 16
	{ "18.ic115", 	  0x8000, 0xf69a3c7c, 5 | BRF_GRA },	       // 17
	{ "17.ic114", 	  0x8000, 0x4e98562a, 5 | BRF_GRA },	       // 18
	{ "16.ic113", 	  0x8000, 0x0d99c3b3, 5 | BRF_GRA },	       // 19
	{ "15.ic112", 	  0x8000, 0x7f14270e, 5 | BRF_GRA },	       // 20

	{ "14.ic25", 	  0x8000, 0x0af4f7eb, 6 | BRF_GRA },	       // 21 Background Tilemaps

	{ "prom.ic3", 	  0x0100, 0x02f55589, 7 | BRF_GRA },	       // 22 Color Proms
	{ "prom.ic4", 	  0x0100, 0xe1e36dd9, 7 | BRF_GRA },	       // 23
	{ "prom.ic5", 	  0x0100, 0x989399c0, 7 | BRF_GRA },	       // 24
	{ "g-04.09d", 	  0x0100, 0x906612b5, 7 | BRF_GRA },	       // 25
	{ "g-06.14a", 	  0x0100, 0x4a9da18b, 7 | BRF_GRA },	       // 26
	{ "g-07.15a", 	  0x0100, 0xcb9394fc, 7 | BRF_GRA },	       // 27
	{ "g-09.09f", 	  0x0100, 0x3cee181e, 7 | BRF_GRA },	       // 28
	{ "g-08.08f", 	  0x0100, 0xef91cdd2, 7 | BRF_GRA },	       // 29

	{ "g-10.02j", 	  0x0100, 0x0eaf5158, 0 | BRF_OPT },	       // 30 Video Timing
	{ "g-05.01f", 	  0x0100, 0x25c90c2a, 0 | BRF_OPT },	       // 31 Priority
};

STD_ROM_PICK(gunsmokeb)
STD_ROM_FN(gunsmokeb)

struct BurnDriver BurnDrvGunsmokeb = {
	"gunsmokeb", "gunsmoke", NULL, NULL, "1985",
	"Gun.Smoke (World, 851115) (bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_VERSHOOT, 0,
	NULL, gunsmokebRomInfo, gunsmokebRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvCalcPal, 0x300,
	224, 256, 3, 4
};


// Gun.Smoke (Japan, 851115)

static struct BurnRomInfo gunsmokejRomDesc[] = {
	{ "gsj_03.09n",   0x8000, 0xb56b5df6, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "gs04.10n", 	  0x8000, 0x8d4b423f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gs05.12n", 	  0x8000, 0x2b5667fb, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "gs02.14h", 	  0x8000, 0xcd7a2c38, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "gs01.11f", 	  0x4000, 0xb61ece9b, 3 | BRF_GRA },	       //  4 Character Tiles

	{ "gs13.06c", 	  0x8000, 0xf6769fc5, 4 | BRF_GRA },	       //  5 32x32 Tiles
	{ "gs12.05c", 	  0x8000, 0xd997b78c, 4 | BRF_GRA },	       //  6
	{ "gs11.04c", 	  0x8000, 0x125ba58e, 4 | BRF_GRA },	       //  7
	{ "gs10.02c", 	  0x8000, 0xf469c13c, 4 | BRF_GRA },	       //  8
	{ "gs09.06a", 	  0x8000, 0x539f182d, 4 | BRF_GRA },	       //  9
	{ "gs08.05a", 	  0x8000, 0xe87e526d, 4 | BRF_GRA },	       // 10
	{ "gs07.04a", 	  0x8000, 0x4382c0d2, 4 | BRF_GRA },	       // 11 
	{ "gs06.02a", 	  0x8000, 0x4cafe7a6, 4 | BRF_GRA },	       // 12 

	{ "gs22.06n", 	  0x8000, 0xdc9c508c, 5 | BRF_GRA },	       // 13 Sprites
	{ "gs21.04n", 	  0x8000, 0x68883749, 5 | BRF_GRA },	       // 14
	{ "gs20.03n", 	  0x8000, 0x0be932ed, 5 | BRF_GRA },	       // 15
	{ "gs19.01n", 	  0x8000, 0x63072f93, 5 | BRF_GRA },	       // 16
	{ "gs18.06l", 	  0x8000, 0xf69a3c7c, 5 | BRF_GRA },	       // 17
	{ "gs17.04l", 	  0x8000, 0x4e98562a, 5 | BRF_GRA },	       // 18
	{ "gs16.03l", 	  0x8000, 0x0d99c3b3, 5 | BRF_GRA },	       // 19
	{ "gs15.01l", 	  0x8000, 0x7f14270e, 5 | BRF_GRA },	       // 20

	{ "gs14.11c", 	  0x8000, 0x0af4f7eb, 6 | BRF_GRA },	       // 21 Background Tilemaps

	{ "g-01.03b", 	  0x0100, 0x02f55589, 7 | BRF_GRA },	       // 22 Color Proms
	{ "g-02.04b", 	  0x0100, 0xe1e36dd9, 7 | BRF_GRA },	       // 23
	{ "g-03.05b", 	  0x0100, 0x989399c0, 7 | BRF_GRA },	       // 24
	{ "g-04.09d", 	  0x0100, 0x906612b5, 7 | BRF_GRA },	       // 25
	{ "g-06.14a", 	  0x0100, 0x4a9da18b, 7 | BRF_GRA },	       // 26
	{ "g-07.15a", 	  0x0100, 0xcb9394fc, 7 | BRF_GRA },	       // 27
	{ "g-09.09f", 	  0x0100, 0x3cee181e, 7 | BRF_GRA },	       // 28
	{ "g-08.08f", 	  0x0100, 0xef91cdd2, 7 | BRF_GRA },	       // 29

	{ "g-10.02j", 	  0x0100, 0x0eaf5158, 0 | BRF_OPT },	       // 30 Video Timing
	{ "g-05.01f", 	  0x0100, 0x25c90c2a, 0 | BRF_OPT },	       // 31 Priority
};

STD_ROM_PICK(gunsmokej)
STD_ROM_FN(gunsmokej)

struct BurnDriver BurnDrvGunsmokej = {
	"gunsmokej", "gunsmoke", NULL, NULL, "1985",
	"Gun.Smoke (Japan, 851115)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_VERSHOOT, 0,
	NULL, gunsmokejRomInfo, gunsmokejRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvCalcPal, 0x300,
	224, 256, 3, 4
};


// Gun.Smoke (US, 860408)

static struct BurnRomInfo gunsmokeuRomDesc[] = {
	{ "gsa_03.9n",    0x8000, 0x51dc3f76, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "gs04.10n",	  0x8000, 0x5ecf31b8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gs05.12n", 	  0x8000, 0x1c9aca13, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "gs02.14h", 	  0x8000, 0xcd7a2c38, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "gs01.11f", 	  0x4000, 0xb61ece9b, 3 | BRF_GRA },	       //  4 Character Tiles

	{ "gs13.06c", 	  0x8000, 0xf6769fc5, 4 | BRF_GRA },	       //  5 32x32 Tiles
	{ "gs12.05c", 	  0x8000, 0xd997b78c, 4 | BRF_GRA },	       //  6
	{ "gs11.04c", 	  0x8000, 0x125ba58e, 4 | BRF_GRA },	       //  7
	{ "gs10.02c", 	  0x8000, 0xf469c13c, 4 | BRF_GRA },	       //  8
	{ "gs09.06a", 	  0x8000, 0x539f182d, 4 | BRF_GRA },	       //  9
	{ "gs08.05a", 	  0x8000, 0xe87e526d, 4 | BRF_GRA },	       // 10
	{ "gs07.04a", 	  0x8000, 0x4382c0d2, 4 | BRF_GRA },	       // 11 
	{ "gs06.02a", 	  0x8000, 0x4cafe7a6, 4 | BRF_GRA },	       // 12 

	{ "gs22.06n", 	  0x8000, 0xdc9c508c, 5 | BRF_GRA },	       // 13 Sprites
	{ "gs21.04n", 	  0x8000, 0x68883749, 5 | BRF_GRA },	       // 14
	{ "gs20.03n", 	  0x8000, 0x0be932ed, 5 | BRF_GRA },	       // 15
	{ "gs19.01n", 	  0x8000, 0x63072f93, 5 | BRF_GRA },	       // 16
	{ "gs18.06l", 	  0x8000, 0xf69a3c7c, 5 | BRF_GRA },	       // 17
	{ "gs17.04l", 	  0x8000, 0x4e98562a, 5 | BRF_GRA },	       // 18
	{ "gs16.03l", 	  0x8000, 0x0d99c3b3, 5 | BRF_GRA },	       // 19
	{ "gs15.01l", 	  0x8000, 0x7f14270e, 5 | BRF_GRA },	       // 20

	{ "gs14.11c", 	  0x8000, 0x0af4f7eb, 6 | BRF_GRA },	       // 21 Background Tilemaps

	{ "g-01.03b", 	  0x0100, 0x02f55589, 7 | BRF_GRA },	       // 22 Color Proms
	{ "g-02.04b", 	  0x0100, 0xe1e36dd9, 7 | BRF_GRA },	       // 23
	{ "g-03.05b", 	  0x0100, 0x989399c0, 7 | BRF_GRA },	       // 24
	{ "g-04.09d", 	  0x0100, 0x906612b5, 7 | BRF_GRA },	       // 25
	{ "g-06.14a", 	  0x0100, 0x4a9da18b, 7 | BRF_GRA },	       // 26
	{ "g-07.15a", 	  0x0100, 0xcb9394fc, 7 | BRF_GRA },	       // 27
	{ "g-09.09f", 	  0x0100, 0x3cee181e, 7 | BRF_GRA },	       // 28
	{ "g-08.08f", 	  0x0100, 0xef91cdd2, 7 | BRF_GRA },	       // 29

	{ "g-10.02j", 	  0x0100, 0x0eaf5158, 0 | BRF_OPT },	       // 30 Video Timing
	{ "g-05.01f", 	  0x0100, 0x25c90c2a, 0 | BRF_OPT },	       // 31 Priority
};

STD_ROM_PICK(gunsmokeu)
STD_ROM_FN(gunsmokeu)

struct BurnDriver BurnDrvGunsmokeu = {
	"gunsmokeu", "gunsmoke", NULL, NULL, "1985",
	"Gun.Smoke (US, 860408)\0", NULL, "Capcom (Romstar License)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_VERSHOOT, 0,
	NULL, gunsmokeuRomInfo, gunsmokeuRomName, NULL, NULL, DrvInputInfo, gunsmokeuDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvCalcPal, 0x300,
	224, 256, 3, 4
};


// Gun.Smoke (US, 851115, set 1)
// has a small extra piece of code at 0x2f00 and a jump to it at 0x297b, otherwise the same as gunsmokeub including the datecode, chip had an 'A' stamped on it, bugfix?

static struct BurnRomInfo gunsmokeuaRomDesc[] = {
	{ "gsr_03a.9n",	  0x8000, 0x2f6e6ad7, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "gs04.10n",	  0x8000, 0x8d4b423f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gs05.12n",     0x8000, 0x2b5667fb, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "gs02.14h", 	  0x8000, 0xcd7a2c38, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "gs01.11f", 	  0x4000, 0xb61ece9b, 3 | BRF_GRA },	       //  4 Character Tiles

	{ "gs13.06c", 	  0x8000, 0xf6769fc5, 4 | BRF_GRA },	       //  5 32x32 Tiles
	{ "gs12.05c", 	  0x8000, 0xd997b78c, 4 | BRF_GRA },	       //  6
	{ "gs11.04c", 	  0x8000, 0x125ba58e, 4 | BRF_GRA },	       //  7
	{ "gs10.02c", 	  0x8000, 0xf469c13c, 4 | BRF_GRA },	       //  8
	{ "gs09.06a", 	  0x8000, 0x539f182d, 4 | BRF_GRA },	       //  9
	{ "gs08.05a", 	  0x8000, 0xe87e526d, 4 | BRF_GRA },	       // 10
	{ "gs07.04a", 	  0x8000, 0x4382c0d2, 4 | BRF_GRA },	       // 11 
	{ "gs06.02a", 	  0x8000, 0x4cafe7a6, 4 | BRF_GRA },	       // 12 

	{ "gs22.06n", 	  0x8000, 0xdc9c508c, 5 | BRF_GRA },	       // 13 Sprites
	{ "gs21.04n", 	  0x8000, 0x68883749, 5 | BRF_GRA },	       // 14
	{ "gs20.03n", 	  0x8000, 0x0be932ed, 5 | BRF_GRA },	       // 15
	{ "gs19.01n", 	  0x8000, 0x63072f93, 5 | BRF_GRA },	       // 16
	{ "gs18.06l", 	  0x8000, 0xf69a3c7c, 5 | BRF_GRA },	       // 17
	{ "gs17.04l", 	  0x8000, 0x4e98562a, 5 | BRF_GRA },	       // 18
	{ "gs16.03l", 	  0x8000, 0x0d99c3b3, 5 | BRF_GRA },	       // 19
	{ "gs15.01l", 	  0x8000, 0x7f14270e, 5 | BRF_GRA },	       // 20

	{ "gs14.11c", 	  0x8000, 0x0af4f7eb, 6 | BRF_GRA },	       // 21 Background Tilemaps

	{ "g-01.03b", 	  0x0100, 0x02f55589, 7 | BRF_GRA },	       // 22 Color Proms
	{ "g-02.04b", 	  0x0100, 0xe1e36dd9, 7 | BRF_GRA },	       // 23
	{ "g-03.05b", 	  0x0100, 0x989399c0, 7 | BRF_GRA },	       // 24
	{ "g-04.09d", 	  0x0100, 0x906612b5, 7 | BRF_GRA },	       // 25
	{ "g-06.14a", 	  0x0100, 0x4a9da18b, 7 | BRF_GRA },	       // 26
	{ "g-07.15a", 	  0x0100, 0xcb9394fc, 7 | BRF_GRA },	       // 27
	{ "g-09.09f", 	  0x0100, 0x3cee181e, 7 | BRF_GRA },	       // 28
	{ "g-08.08f", 	  0x0100, 0xef91cdd2, 7 | BRF_GRA },	       // 29

	{ "g-10.02j", 	  0x0100, 0x0eaf5158, 0 | BRF_OPT },	       // 30 Video Timing
	{ "g-05.01f", 	  0x0100, 0x25c90c2a, 0 | BRF_OPT },	       // 31 Priority
};

STD_ROM_PICK(gunsmokeua)
STD_ROM_FN(gunsmokeua)

struct BurnDriver BurnDrvGunsmokeua = {
	"gunsmokeua", "gunsmoke", NULL, NULL, "1986",
	"Gun.Smoke (US, 851115, set 1)\0", NULL, "Capcom (Romstar License)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_VERSHOOT, 0,
	NULL, gunsmokeuaRomInfo, gunsmokeuaRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvCalcPal, 0x300,
	224, 256, 3, 4
};


// Gun.Smoke (US, 851115, set 2)

static struct BurnRomInfo gunsmokeubRomDesc[] = {
	{ "gsr_03.9n",    0x8000, 0x592f211b, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "gs04.10n",     0x8000, 0x8d4b423f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gs05.12n",     0x8000, 0x2b5667fb, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "gs02.14h", 	  0x8000, 0xcd7a2c38, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "gs01.11f", 	  0x4000, 0xb61ece9b, 3 | BRF_GRA },	       //  4 Character Tiles

	{ "gs13.06c", 	  0x8000, 0xf6769fc5, 4 | BRF_GRA },	       //  5 32x32 Tiles
	{ "gs12.05c", 	  0x8000, 0xd997b78c, 4 | BRF_GRA },	       //  6
	{ "gs11.04c", 	  0x8000, 0x125ba58e, 4 | BRF_GRA },	       //  7
	{ "gs10.02c", 	  0x8000, 0xf469c13c, 4 | BRF_GRA },	       //  8
	{ "gs09.06a", 	  0x8000, 0x539f182d, 4 | BRF_GRA },	       //  9
	{ "gs08.05a", 	  0x8000, 0xe87e526d, 4 | BRF_GRA },	       // 10
	{ "gs07.04a", 	  0x8000, 0x4382c0d2, 4 | BRF_GRA },	       // 11 
	{ "gs06.02a", 	  0x8000, 0x4cafe7a6, 4 | BRF_GRA },	       // 12 

	{ "gs22.06n", 	  0x8000, 0xdc9c508c, 5 | BRF_GRA },	       // 13 Sprites
	{ "gs21.04n", 	  0x8000, 0x68883749, 5 | BRF_GRA },	       // 14
	{ "gs20.03n", 	  0x8000, 0x0be932ed, 5 | BRF_GRA },	       // 15
	{ "gs19.01n", 	  0x8000, 0x63072f93, 5 | BRF_GRA },	       // 16
	{ "gs18.06l", 	  0x8000, 0xf69a3c7c, 5 | BRF_GRA },	       // 17
	{ "gs17.04l", 	  0x8000, 0x4e98562a, 5 | BRF_GRA },	       // 18
	{ "gs16.03l", 	  0x8000, 0x0d99c3b3, 5 | BRF_GRA },	       // 19
	{ "gs15.01l", 	  0x8000, 0x7f14270e, 5 | BRF_GRA },	       // 20

	{ "gs14.11c", 	  0x8000, 0x0af4f7eb, 6 | BRF_GRA },	       // 21 Background Tilemaps

	{ "g-01.03b", 	  0x0100, 0x02f55589, 7 | BRF_GRA },	       // 22 Color Proms
	{ "g-02.04b", 	  0x0100, 0xe1e36dd9, 7 | BRF_GRA },	       // 23
	{ "g-03.05b", 	  0x0100, 0x989399c0, 7 | BRF_GRA },	       // 24
	{ "g-04.09d", 	  0x0100, 0x906612b5, 7 | BRF_GRA },	       // 25
	{ "g-06.14a", 	  0x0100, 0x4a9da18b, 7 | BRF_GRA },	       // 26
	{ "g-07.15a", 	  0x0100, 0xcb9394fc, 7 | BRF_GRA },	       // 27
	{ "g-09.09f", 	  0x0100, 0x3cee181e, 7 | BRF_GRA },	       // 28
	{ "g-08.08f", 	  0x0100, 0xef91cdd2, 7 | BRF_GRA },	       // 29

	{ "g-10.02j", 	  0x0100, 0x0eaf5158, 0 | BRF_OPT },	       // 30 Video Timing
	{ "g-05.01f", 	  0x0100, 0x25c90c2a, 0 | BRF_OPT },	       // 31 Priority
};

STD_ROM_PICK(gunsmokeub)
STD_ROM_FN(gunsmokeub)

struct BurnDriver BurnDrvGunsmokeub = {
	"gunsmokeub", "gunsmoke", NULL, NULL, "1986",
	"Gun.Smoke (US, 851115, set 2)\0", NULL, "Capcom (Romstar License)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_VERSHOOT, 0,
	NULL, gunsmokeubRomInfo, gunsmokeubRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvCalcPal, 0x300,
	224, 256, 3, 4
};
