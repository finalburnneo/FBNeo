// FB Neo Gun.Smoke driver module
// Based on MAME driver by Paul Leaman

#include "tiles_generic.h"
#include "z80_intf.h"
#include "burn_ym2203.h"

static UINT8 *Mem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvColPROM;

static UINT8 *DrvColRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvSprRAM;

static UINT32 *DrvPalette;
static UINT8 DrvCalcPal;

static UINT8 soundlatch;
static UINT8 flipscreen;
static INT32 nGunsmokeBank;

static UINT8 sprite3bank;
static UINT8 chon;
static UINT8 bgon;
static UINT8 objon;
static UINT8 scrollx[2];
static UINT8 scrolly;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo DrvInputList[] = {
	{"P1 Coin", 		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 coin"  },
	{"P1 start", 		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start" },
	{"P1 Right", 		BIT_DIGITAL,	DrvJoy2 + 0, 	"p1 right" },
	{"P1 Left", 		BIT_DIGITAL,	DrvJoy2 + 1, 	"p1 left"  },
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 2, 	"p1 down"  },
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 3, 	"p1 up"    },
	{"P1 Button 1", 	BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"},
	{"P1 Button 2", 	BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"},
	{"P1 Button 3", 	BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 3"},

	{"P2 Coin", 		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 coin"  },
	{"P2 start", 		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start" },
	{"P2 Right", 		BIT_DIGITAL,	DrvJoy3 + 0, 	"p2 right" },
	{"P2 Left", 		BIT_DIGITAL,	DrvJoy3 + 1, 	"p2 left"  },
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 2, 	"p2 down"  },
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 3, 	"p2 up"    },
	{"P2 Button 1", 	BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"},
	{"P2 Button 2", 	BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"},
	{"P2 Button 3", 	BIT_DIGITAL,	DrvJoy3 + 6,	"p2 fire 3"},

	{"Service",			BIT_DIGITAL,	DrvJoy1 + 4,	"service"  },

	{"Reset",			BIT_DIGITAL,	&DrvReset  ,	"reset"    },
	{"Dip 1",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"	   },
	{"Dip 2",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"	   },
};

STDINPUTINFO(Drv)

static struct BurnDIPInfo DrvDIPList[]=
{
	// Default Values
	{0x14, 0xff, 0xff, 0xff, NULL               },

	{0   , 0xfe, 0   , 4   , "Bonus Life"       },
	{0x14, 0x01, 0x03, 0x01, "30k 80k 80k+"     },
	{0x14, 0x01, 0x03, 0x03, "30k 100k 100k+"   },
	{0x14, 0x01, 0x03, 0x00, "30k 100k 150k+"   },
	{0x14, 0x01, 0x03, 0x02, "30k 100k"    	    },

	{0   , 0xfe, 0   , 2   , "Demo"             },
	{0x14, 0x01, 0x04, 0x00, "Off"     	        },
	{0x14, 0x01, 0x04, 0x04, "On"    	        },
};

static struct BurnDIPInfo gunsmokeuDIPList[]=
{
	// Default Values
	{0x14, 0xff, 0xff, 0xff, NULL               },

	{0   , 0xfe, 0   , 4   , "Bonus Life"       },
	{0x14, 0x01, 0x03, 0x01, "30k 80k 80k+"     },
	{0x14, 0x01, 0x03, 0x03, "30k 100k 100k+"   },
	{0x14, 0x01, 0x03, 0x00, "30k 100k 150k+"   },
	{0x14, 0x01, 0x03, 0x02, "30k 100k"    	    },

	{0   , 0xfe, 0   , 2   , "Lives"            },
	{0x14, 0x01, 0x04, 0x04, "3"     	        },
	{0x14, 0x01, 0x04, 0x00, "5"    	        },
};

static struct BurnDIPInfo gunsmokeDIPList[]=
{
	{0   , 0xfe, 0   , 2   , "Cabinet"          },
	{0x14, 0x01, 0x08, 0x00, "Upright"     	    },
	{0x14, 0x01, 0x08, 0x08, "Cocktail"    	    },

	{0   , 0xfe, 0   , 4   , "Difficulty"       },
	{0x14, 0x01, 0x30, 0x20, "Easy"     	    },
	{0x14, 0x01, 0x30, 0x30, "Normal"    	    },
	{0x14, 0x01, 0x30, 0x10, "Difficult"   	    },
	{0x14, 0x01, 0x30, 0x00, "Very Difficult"   },

	{0   , 0xfe, 0   , 2   , "Freeze"           },
	{0x14, 0x01, 0x40, 0x40, "Off"     	        },
	{0x14, 0x01, 0x40, 0x00, "On"    	        },

	{0   , 0xfe, 0   , 2   , "Service Mode"     },
	{0x14, 0x01, 0x80, 0x80, "Off"     	        },
	{0x14, 0x01, 0x80, 0x00, "On"    	        },

	// Default Values
	{0x15, 0xff, 0xff, 0xff, NULL               },

	{0   , 0xfe, 0   , 8  ,  "Coin A"           },
	{0x15, 0x01, 0x07, 0x00, "4 Coins 1 Credit" },
	{0x15, 0x01, 0x07, 0x01, "3 Coins 1 Credit" },
	{0x15, 0x01, 0x07, 0x02, "2 Coins 1 Credit" },
	{0x15, 0x01, 0x07, 0x07, "1 Coin 1 Credit"  },
	{0x15, 0x01, 0x07, 0x06, "1 Coin 2 Credits" },
	{0x15, 0x01, 0x07, 0x05, "1 Coin 3 Credits" },
	{0x15, 0x01, 0x07, 0x04, "1 Coin 4 Credits" },
	{0x15, 0x01, 0x07, 0x03, "1 Coin 6 Credits" },

	{0   , 0xfe, 0   , 8  ,  "Coin B"           },
	{0x15, 0x01, 0x38, 0x00, "4 Coins 1 Credit" },
	{0x15, 0x01, 0x38, 0x08, "3 Coins 1 Credit" },
	{0x15, 0x01, 0x38, 0x10, "2 Coins 1 Credit" },
	{0x15, 0x01, 0x38, 0x38, "1 Coin 1 Credit"  },
	{0x15, 0x01, 0x38, 0x30, "1 Coin 2 Credits" },
	{0x15, 0x01, 0x38, 0x28, "1 Coin 3 Credits" },
	{0x15, 0x01, 0x38, 0x20, "1 Coin 4 Credits" },
	{0x15, 0x01, 0x38, 0x18, "1 Coin 6 Credits" },

	{0   , 0xfe, 0   , 2   , "Allow Continue"   },
	{0x15, 0x01, 0x40, 0x00, "No"    	        },
	{0x15, 0x01, 0x40, 0x40, "Yes"              },

	{0   , 0xfe, 0   , 2   , "Demo Sounds"      },
	{0x15, 0x01, 0x80, 0x00, "Off"    	        },
	{0x15, 0x01, 0x80, 0x80, "On"     	        },
};

STDDIPINFOEXT(Drv, Drv, gunsmoke)
STDDIPINFOEXT(gunsmokeu, gunsmokeu, gunsmoke)

static inline void gunsmoke_bankswitch(INT32 nBank)
{
	nGunsmokeBank = nBank;

	ZetMapMemory(DrvZ80ROM0 + 0x10000 + nBank * 0x4000, 0x8000, 0xbfff, MAP_ROM);
}

static void __fastcall gunsmoke_cpu0_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xc800:
			soundlatch = data;
		break;

		case 0xc804:
			gunsmoke_bankswitch((data >> 2) & 3);
			flipscreen = 0; // data & 0x40; ignore flipscreen
			chon       = data & 0x80;
		break;

		case 0xc806:
		break;

		case 0xd800:
		case 0xd801:
			scrollx[address & 1] = data;
		break;

		case 0xd802:
		case 0xd803:
			scrolly = data;
		break;

		case 0xd806:
			sprite3bank = data & 0x07;
			bgon        = data & 0x10;
			objon       = data & 0x20;
		break;
	}
}

static UINT8 __fastcall gunsmoke_cpu0_read(UINT16 address)
{
	switch (address)
	{
		case 0xc000:
		case 0xc001:
		case 0xc002:
			return DrvInputs[address & 3];

		case 0xc003:
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

static void __fastcall gunsmoke_cpu1_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xe000:
		case 0xe001:
		case 0xe002:
		case 0xe003:
			BurnYM2203Write((address / 2) & 1, address & 1, data);
		break;
	}
}

static UINT8 __fastcall gunsmoke_cpu1_read(UINT16 address)
{
	if (address == 0xc800) return soundlatch;

	return 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	gunsmoke_bankswitch(0);
	ZetClose();

	ZetOpen(1);
	ZetReset();
	ZetClose();

	BurnYM2203Reset();

	HiscoreReset();

	soundlatch = 0;
	flipscreen = 0;
	sprite3bank = 0;
	chon = bgon = objon = 0;
	scrollx[0] = 0;
	scrollx[1] = 0;
	scrolly = 0;

	return 0;
}

static INT32 gunsmoke_gfx_decode()
{
	UINT8 *tmp = (UINT8*)BurnMalloc(0x80000);
	if (!tmp) return 1;

	static INT32 Planes[4]     = { 0x100004, 0x100000, 4, 0 };

	static INT32 CharXOffs[8]  = { STEP4(11,-1), STEP4(3,-1) };
	static INT32 CharYOffs[8]  = { STEP8(112,-16) };

	static INT32 TileXOffs[32] = { STEP4(0,1), STEP4(8,1), STEP4(512,1), STEP4(520,1), STEP4(1024,1), STEP4(1032,1), STEP4(1536,1), STEP4(1544,1) };

	static INT32 TileYOffs[32] = { STEP32(0,16) };
	static INT32 SpriXOffs[16] = { STEP4(0,1), STEP4(8,1), STEP4(256,1), STEP4(264,1) };

	memcpy (tmp, DrvGfxROM0, 0x04000);

	GfxDecode(0x400, 2,  8,  8, Planes + 2, CharXOffs, CharYOffs, 0x080, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x40000);

	GfxDecode(0x200, 4, 32, 32, Planes + 0, TileXOffs, TileYOffs, 0x800, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x40000);

	GfxDecode(0x800, 4, 16, 16, Planes + 0, SpriXOffs, TileYOffs, 0x200, tmp, DrvGfxROM2);

	BurnFree (tmp);

	return 0;
}

static tilemap_callback( bg )
{
	INT32 attr = DrvGfxROM3[offs * 2 + 1];
	INT32 code = DrvGfxROM3[offs * 2 + 0] + (attr << 8);

	TILE_SET_INFO(0, code, attr >> 2, TILE_FLIPYX(attr >> 6));
}

static tilemap_callback( fg )
{
	INT32 attr = DrvColRAM[offs];
	INT32 code = DrvVidRAM[offs] + ((attr & 0xe0) << 2);

	TILE_SET_INFO(1, code, attr & 0x1f, 0);
	sTile->category = attr & 0x1f;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = Mem;

	DrvZ80ROM0		= Next; Next += 0x20000;
	DrvZ80ROM1		= Next; Next += 0x08000;
	DrvGfxROM0		= Next; Next += 0x10000;
	DrvGfxROM1		= Next; Next += 0x80000;
	DrvGfxROM2		= Next; Next += 0x80000;
	DrvGfxROM3		= Next; Next += 0x08000;
	DrvColPROM		= Next; Next += 0x00800;

	DrvPalette		= (UINT32*)Next; Next += 0x00300 * sizeof(UINT32);

	AllRam			= Next;

	DrvColRAM		= Next; Next += 0x00400;
	DrvVidRAM		= Next; Next += 0x00400;
	DrvZ80RAM0		= Next; Next += 0x01000;
	DrvZ80RAM1		= Next; Next += 0x00800;
	DrvSprRAM		= Next; Next += 0x01000;

	RamEnd			= Next;

	MemEnd			= Next;

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
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x10000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x18000,  2, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x00000, 21, 1)) return 1;

		for (INT32 i = 0; i < 8; i++) {
			if (BurnLoadRom(DrvGfxROM1 + i * 0x8000,  5 + i, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + i * 0x8000, 13 + i, 1)) return 1;
			if (BurnLoadRom(DrvColPROM + i * 0x0100, 22 + i, 1)) return 1;
		}

		gunsmoke_gfx_decode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x7fff, MAP_ROM);
//	bank 8000-bfff
	ZetMapMemory(DrvVidRAM,		0xd000, 0xd3ff, MAP_RAM);
	ZetMapMemory(DrvColRAM,		0xd400, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM0,	0xe000, 0xefff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xf000, 0xffff, MAP_RAM);
	ZetSetReadHandler(gunsmoke_cpu0_read);
	ZetSetWriteHandler(gunsmoke_cpu0_write);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,	0xc000, 0xc7ff, MAP_RAM);
	ZetSetReadHandler(gunsmoke_cpu1_read);
	ZetSetWriteHandler(gunsmoke_cpu1_write);
	ZetClose();

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_COLS, bg_map_callback, 32, 32, 2048,  8);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, fg_map_callback,  8,  8,   32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM1, 4, 32, 32, 0x80000, 0x100, 0x0f);
	GenericTilemapSetGfx(1, DrvGfxROM0, 2,  8,  8, 0x10000, 0x000, 0x1f);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);
	GenericTilemapCategoryConfig(1, 0x20);
	for (INT32 i = 0; i < 0x20 * 4; i++) {
		GenericTilemapSetCategoryEntry(1, i / 4, i % 4, (DrvColPROM[0x300 + i] == 0xf) ? 1 : 0);
	}

	BurnYM2203Init(2, 1500000, NULL, 0);
	BurnTimerAttachZet(3000000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE, 0.12, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.20, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.20, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.20, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_YM2203_ROUTE, 0.12, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_1, 0.20, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_2, 0.20, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_3, 0.20, BURN_SND_ROUTE_BOTH);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	BurnYM2203Exit();

	BurnFree (Mem);

	return 0;
}

static INT32 DrvPaletteInit()
{
	UINT32 tmp[0x100];

	for (INT32 i = 0; i < 0x100; i++)
	{
		UINT8 r  = DrvColPROM[i + 0x000] & 0x0f;
		UINT8 g  = DrvColPROM[i + 0x100] & 0x0f;
		UINT8 b  = DrvColPROM[i + 0x200] & 0x0f;

		tmp[i] = BurnHighCol((r*16)+r,(g*16)+g,(b*16)+b,0);
	}

	for (INT32 i = 0; i < 0x100; i++)
	{
		DrvPalette[0x000 + i] = tmp[DrvColPROM[0x300 + i] | 0x40];
		DrvPalette[0x100 + i] = tmp[DrvColPROM[0x400 + i] | ((DrvColPROM[0x500 + i] & 0x03) << 4)];
		DrvPalette[0x200 + i] = tmp[DrvColPROM[0x600 + i] | ((DrvColPROM[0x700 + i] & 0x07) << 4) | 0x80];
	}

	return 0;
}

static void draw_sprites()
{
	for (INT32 offs = 0x1000 - 32; offs >= 0; offs -= 32)
	{
		INT32 attr = DrvSprRAM[1 + offs];
		INT32 bank = (attr & 0xc0) >> 6;
		INT32 code = DrvSprRAM[0 + offs];
		INT32 color = attr & 0x0f;
		INT32 flipx = 0;
		INT32 flipy = attr & 0x10;
		INT32 sx = DrvSprRAM[3 + offs] - ((attr & 0x20) << 3);
		INT32 sy = DrvSprRAM[2 + offs];

		if (sy == 0 || sy > 0xef) continue;

		if (bank == 3) bank += sprite3bank;
		code += 256 * bank;

		if (flipscreen)
		{
			sx = 240 - sx;
			sy = 240 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		sy -= 16;

		Draw16x16MaskTile(pTransDraw, code, sx, sy, flipx, flipy, color, 4, 0, 0x200, DrvGfxROM2);
	}
}

static INT32 DrvDraw()
{
	if (DrvCalcPal) {
		DrvPaletteInit();
		DrvCalcPal = 0;
	}

	GenericTilemapSetScrollX(0, scrollx[0] + (scrollx[1] * 256));
	GenericTilemapSetScrollY(0, scrolly);

	if (!bgon || ~nBurnLayer & 1) BurnTransferClear();
	if (bgon  && nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);
	if (objon && nSpriteEnable & 1) draw_sprites();
	if (chon  && nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 3);

		for (INT32 i = 0; i < 8; i++)
		{
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 4000000 / 60, 3000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++) {
		ZetOpen(0);
		CPU_RUN(0, Zet);
		if (i == 240) {
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);

			if (pBurnDraw) {
				DrvDraw();
			}
		}
		ZetClose();

		ZetOpen(1);
		BurnTimerUpdate((i + 1) * (nCyclesTotal[1] / nInterleave));
		if (i%64 == 63) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();
	}
	
	ZetOpen(1);
	BurnTimerEndFrame(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
	}
	ZetClose();

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

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All RAM";
		BurnAcb(&ba);

		ZetScan(nAction);
		BurnYM2203Scan(nAction, pnMin);

		SCAN_VAR(soundlatch);
		SCAN_VAR(flipscreen);
		SCAN_VAR(nGunsmokeBank);
		SCAN_VAR(sprite3bank);
		SCAN_VAR(chon);
		SCAN_VAR(bgon);
		SCAN_VAR(objon);
		SCAN_VAR(scrollx);
		SCAN_VAR(scrolly);

		if (nAction & ACB_WRITE) {
			ZetOpen(0);
			gunsmoke_bankswitch(nGunsmokeBank);
			ZetClose();
		}
	}

	return 0;
}


// Gun.Smoke (World, 1985-11-15)

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

	{ "g-01.03b", 	  0x0100, 0x02f55589, 7 | BRF_GRA },	       // 22 Color DrvColPROMs
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
	"Gun.Smoke (World, 1985-11-15)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_RUNGUN, 0,
	NULL, gunsmokeRomInfo, gunsmokeRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvCalcPal, 0x300,
	224, 256, 3, 4
};


// Gun.Smoke (World, 1985-11-15) (bootleg)
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

	{ "prom.ic3", 	  0x0100, 0x02f55589, 7 | BRF_GRA },	       // 22 Color DrvColPROMs
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
	"Gun.Smoke (World, 1985-11-15) (bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_RUNGUN, 0,
	NULL, gunsmokebRomInfo, gunsmokebRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvCalcPal, 0x300,
	224, 256, 3, 4
};


// Gun.Smoke (Japan, 1985-11-15)

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

	{ "g-01.03b", 	  0x0100, 0x02f55589, 7 | BRF_GRA },	       // 22 Color DrvColPROMs
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
	"Gun.Smoke (Japan, 1985-11-15)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_RUNGUN, 0,
	NULL, gunsmokejRomInfo, gunsmokejRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvCalcPal, 0x300,
	224, 256, 3, 4
};


// Gun.Smoke (USA and Canada, 1986-04-08)

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

	{ "g-01.03b", 	  0x0100, 0x02f55589, 7 | BRF_GRA },	       // 22 Color DrvColPROMs
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
	"gunsmokeu", "gunsmoke", NULL, NULL, "1986",
	"Gun.Smoke (USA and Canada, 1986-04-08)\0", NULL, "Capcom (Romstar license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_RUNGUN, 0,
	NULL, gunsmokeuRomInfo, gunsmokeuRomName, NULL, NULL, NULL, NULL, DrvInputInfo, gunsmokeuDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvCalcPal, 0x300,
	224, 256, 3, 4
};


// Gun.Smoke (USA and Canada, 1986-01-20)

static struct BurnRomInfo gunsmokeuaRomDesc[] = {
	{ "gsr_03y.09n",	0x8000, 0x1b42423f, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "gsr_04y.10n",	0x8000, 0xa5ee595b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gsr_05y.12n",	0x8000, 0x1c9aca13, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "gs02.14h",		0x8000, 0xcd7a2c38, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "gs01.11f",		0x4000, 0xb61ece9b, 3 | BRF_GRA },           //  4 Character Tiles

	{ "gs13.06c",		0x8000, 0xf6769fc5, 4 | BRF_GRA },           //  5 32x32 Tiles
	{ "gs12.05c",		0x8000, 0xd997b78c, 4 | BRF_GRA },           //  6
	{ "gs11.04c",		0x8000, 0x125ba58e, 4 | BRF_GRA },           //  7
	{ "gs10.02c",		0x8000, 0xf469c13c, 4 | BRF_GRA },           //  8
	{ "gs09.06a",		0x8000, 0x539f182d, 4 | BRF_GRA },           //  9
	{ "gs08.05a",		0x8000, 0xe87e526d, 4 | BRF_GRA },           // 10
	{ "gs07.04a",		0x8000, 0x4382c0d2, 4 | BRF_GRA },           // 11 
	{ "gs06.02a",		0x8000, 0x4cafe7a6, 4 | BRF_GRA },           // 12 

	{ "gs22.06n",		0x8000, 0xdc9c508c, 5 | BRF_GRA },           // 13 Sprites
	{ "gs21.04n",		0x8000, 0x68883749, 5 | BRF_GRA },           // 14
	{ "gs20.03n",		0x8000, 0x0be932ed, 5 | BRF_GRA },           // 15
	{ "gs19.01n",		0x8000, 0x63072f93, 5 | BRF_GRA },           // 16
	{ "gs18.06l",		0x8000, 0xf69a3c7c, 5 | BRF_GRA },           // 17
	{ "gs17.04l",		0x8000, 0x4e98562a, 5 | BRF_GRA },           // 18
	{ "gs16.03l",		0x8000, 0x0d99c3b3, 5 | BRF_GRA },           // 19
	{ "gs15.01l",		0x8000, 0x7f14270e, 5 | BRF_GRA },           // 20

	{ "gs14.11c",		0x8000, 0x0af4f7eb, 6 | BRF_GRA },           // 21 Background Tilemaps

	{ "g-01.03b",		0x0100, 0x02f55589, 7 | BRF_GRA },           // 22 Color DrvColPROMs
	{ "g-02.04b",		0x0100, 0xe1e36dd9, 7 | BRF_GRA },           // 23
	{ "g-03.05b",		0x0100, 0x989399c0, 7 | BRF_GRA },           // 24
	{ "g-04.09d",		0x0100, 0x906612b5, 7 | BRF_GRA },           // 25
	{ "g-06.14a",		0x0100, 0x4a9da18b, 7 | BRF_GRA },           // 26
	{ "g-07.15a",		0x0100, 0xcb9394fc, 7 | BRF_GRA },           // 27
	{ "g-09.09f",		0x0100, 0x3cee181e, 7 | BRF_GRA },           // 28
	{ "g-08.08f",		0x0100, 0xef91cdd2, 7 | BRF_GRA },           // 29

	{ "g-10.02j",		0x0100, 0x0eaf5158, 0 | BRF_OPT },           // 30 Video Timing
	{ "g-05.01f",		0x0100, 0x25c90c2a, 0 | BRF_OPT },           // 31 Priority
};

STD_ROM_PICK(gunsmokeua)
STD_ROM_FN(gunsmokeua)

struct BurnDriver BurnDrvGunsmokeua = {
	"gunsmokeua", "gunsmoke", NULL, NULL, "1985",
	"Gun.Smoke (USA and Canada, 1986-01-20)\0", NULL, "Capcom (Romstar license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_RUNGUN, 0,
	NULL, gunsmokeuaRomInfo, gunsmokeuaRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvCalcPal, 0x300,
	224, 256, 3, 4
};


// Gun.Smoke (USA and Canada, 1985-11-15, set 1)
// has a small extra piece of code at 0x2f00 and a jump to it at 0x297b, otherwise the same as gunsmokeub including the datecode, chip had an 'A' stamped on it, bugfix?

static struct BurnRomInfo gunsmokeubRomDesc[] = {
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

	{ "g-01.03b", 	  0x0100, 0x02f55589, 7 | BRF_GRA },	       // 22 Color DrvColPROMs
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
	"gunsmokeub", "gunsmoke", NULL, NULL, "1985",
	"Gun.Smoke (USA and Canada, 1985-11-15, set 1)\0", NULL, "Capcom (Romstar license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_RUNGUN, 0,
	NULL, gunsmokeubRomInfo, gunsmokeubRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvCalcPal, 0x300,
	224, 256, 3, 4
};


// Gun.Smoke (USA and Canada, 1985-11-15, set 2)

static struct BurnRomInfo gunsmokeucRomDesc[] = {
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

	{ "g-01.03b", 	  0x0100, 0x02f55589, 7 | BRF_GRA },	       // 22 Color DrvColPROMs
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

STD_ROM_PICK(gunsmokeuc)
STD_ROM_FN(gunsmokeuc)

struct BurnDriver BurnDrvGunsmokeuc = {
	"gunsmokeuc", "gunsmoke", NULL, NULL, "1985",
	"Gun.Smoke (USA and Canada, 1985-11-15, set 2)\0", NULL, "Capcom (Romstar license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_RUNGUN, 0,
	NULL, gunsmokeucRomInfo, gunsmokeucRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvCalcPal, 0x300,
	224, 256, 3, 4
};


// Gun.Smoke (Germany, 1985-11-15, censored)

static struct BurnRomInfo gunsmokegRomDesc[] = {
	{ "gsg03.09n",    0x8000, 0x8ad2754e, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "gs04.10n",     0x8000, 0x8d4b423f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gs05.12n",     0x8000, 0x2b5667fb, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "gs02.14h", 	  0x8000, 0xcd7a2c38, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "gs01.11f", 	  0x4000, 0xb61ece9b, 3 | BRF_GRA },	       //  4 Character Tiles

	{ "gs13.06c", 	  0x8000, 0xf6769fc5, 4 | BRF_GRA },	       //  5 32x32 Tiles
	{ "gs12.05c", 	  0x8000, 0xd997b78c, 4 | BRF_GRA },	       //  6
	{ "gs11.04c", 	  0x8000, 0x125ba58e, 4 | BRF_GRA },	       //  7
	{ "gsg10.02c", 	  0x8000, 0x0674ff4d, 4 | BRF_GRA },	       //  8
	{ "gs09.06a", 	  0x8000, 0x539f182d, 4 | BRF_GRA },	       //  9
	{ "gs08.05a", 	  0x8000, 0xe87e526d, 4 | BRF_GRA },	       // 10
	{ "gs07.04a", 	  0x8000, 0x4382c0d2, 4 | BRF_GRA },	       // 11 
	{ "gsg06.02a", 	  0x8000, 0x5cb850a7, 4 | BRF_GRA },	       // 12 

	{ "gsg22.06n", 	  0x8000, 0x96779c38, 5 | BRF_GRA },	       // 13 Sprites
	{ "gsg21.04n", 	  0x8000, 0x6e8a02c7, 5 | BRF_GRA },	       // 14
	{ "gsg20.03n", 	  0x8000, 0x139bf927, 5 | BRF_GRA },	       // 15
	{ "gsg19.01n", 	  0x8000, 0x8f249573, 5 | BRF_GRA },	       // 16
	{ "gsg18.06l", 	  0x8000, 0xb290451c, 5 | BRF_GRA },	       // 17
	{ "gsg17.04l", 	  0x8000, 0x61c9bd10, 5 | BRF_GRA },	       // 18
	{ "gsg16.03l", 	  0x8000, 0x6620103b, 5 | BRF_GRA },	       // 19
	{ "gsg15.01l", 	  0x8000, 0xccc1c1b6, 5 | BRF_GRA },	       // 20

	{ "gs14.11c", 	  0x8000, 0x0af4f7eb, 6 | BRF_GRA },	       // 21 Background Tilemaps

	{ "g-01.03b", 	  0x0100, 0x02f55589, 7 | BRF_GRA },	       // 22 Color DrvColPROMs
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

STD_ROM_PICK(gunsmokeg)
STD_ROM_FN(gunsmokeg)

struct BurnDriver BurnDrvGunsmokeg = {
	"gunsmokeg", "gunsmoke", NULL, NULL, "1985",
	"Gun.Smoke (Germany, 1985-11-15, censored)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_RUNGUN, 0,
	NULL, gunsmokegRomInfo, gunsmokegRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvCalcPal, 0x300,
	224, 256, 3, 4
};
