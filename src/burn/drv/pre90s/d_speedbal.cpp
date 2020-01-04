// FB Alpha Speed Ball / Music Ball driver module
// Based on MAME driver by Joseba Epalza and Andreas Naive

#include "tiles_generic.h"
#include "z80_intf.h"
#include "burn_ym3812.h"
#include "bitswap.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvShareRAM;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvBgRAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvSprRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 flipscreen;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static struct BurnInputInfo SpeedbalInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 4"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 6,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 4"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy2 + 4,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Speedbal)

static struct BurnDIPInfo SpeedbalDIPList[]=
{
	{0x0e, 0xff, 0xff, 0xbf, NULL					},
	{0x0f, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x0e, 0x01, 0x03, 0x03, "1 Coin  2 Credits"	},
	{0x0e, 0x01, 0x03, 0x02, "1 Coin  3 Credits"	},
	{0x0e, 0x01, 0x03, 0x01, "1 Coin  4 Credits"	},
	{0x0e, 0x01, 0x03, 0x00, "1 Coin  5 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin A"				},
	{0x0e, 0x01, 0x0c, 0x00, "4 Coins 1 Credits"	},
	{0x0e, 0x01, 0x0c, 0x04, "3 Coins 1 Credits"	},
	{0x0e, 0x01, 0x0c, 0x08, "2 Coins 1 Credits"	},
	{0x0e, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x0e, 0x01, 0x30, 0x00, "2"					},
	{0x0e, 0x01, 0x30, 0x30, "3"					},
	{0x0e, 0x01, 0x30, 0x20, "4"					},
	{0x0e, 0x01, 0x30, 0x10, "5"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x0e, 0x01, 0x40, 0x00, "Upright"				},
	{0x0e, 0x01, 0x40, 0x40, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x0e, 0x01, 0x80, 0x00, "Off"					},
	{0x0e, 0x01, 0x80, 0x80, "On"					},

	{0   , 0xfe, 0   ,    8, "Bonus Life"			},
	{0x0f, 0x01, 0x07, 0x06, "70000 200000 1M"		},
	{0x0f, 0x01, 0x07, 0x07, "70000 200000"			},
	{0x0f, 0x01, 0x07, 0x03, "100000 300000 1M"		},
	{0x0f, 0x01, 0x07, 0x04, "100000 300000"		},
	{0x0f, 0x01, 0x07, 0x01, "200000 1M"			},
	{0x0f, 0x01, 0x07, 0x05, "200000"				},
	{0x0f, 0x01, 0x07, 0x02, "200000 (duplicate)"	},
	{0x0f, 0x01, 0x07, 0x00, "None"					},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x0f, 0x01, 0x08, 0x08, "Off"					},
	{0x0f, 0x01, 0x08, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Difficulty 1"			},
	{0x0f, 0x01, 0x30, 0x30, "Very Easy"			},
	{0x0f, 0x01, 0x30, 0x20, "Easy"					},
	{0x0f, 0x01, 0x30, 0x10, "Difficult"			},
	{0x0f, 0x01, 0x30, 0x00, "Very Difficult"		},

	{0   , 0xfe, 0   ,    4, "Difficulty 2"			},
	{0x0f, 0x01, 0xc0, 0xc0, "Very Easy"			},
	{0x0f, 0x01, 0xc0, 0x80, "Easy"					},
	{0x0f, 0x01, 0xc0, 0x40, "Difficult"			},
	{0x0f, 0x01, 0xc0, 0x00, "Very Difficult"		},
};

STDDIPINFO(Speedbal)

static struct BurnDIPInfo MusicbalDIPList[]=
{
	{0x0e, 0xff, 0xff, 0xbf, NULL					},
	{0x0f, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x0e, 0x01, 0x03, 0x03, "1 Coin  2 Credits"	},
	{0x0e, 0x01, 0x03, 0x02, "1 Coin  3 Credits"	},
	{0x0e, 0x01, 0x03, 0x01, "1 Coin  4 Credits"	},
	{0x0e, 0x01, 0x03, 0x00, "1 Coin  5 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin A"				},
	{0x0e, 0x01, 0x0c, 0x00, "4 Coins 1 Credits"	},
	{0x0e, 0x01, 0x0c, 0x04, "3 Coins 1 Credits"	},
	{0x0e, 0x01, 0x0c, 0x08, "2 Coins 1 Credits"	},
	{0x0e, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x0e, 0x01, 0x30, 0x00, "2"					},
	{0x0e, 0x01, 0x30, 0x30, "3"					},
	{0x0e, 0x01, 0x30, 0x20, "4"					},
	{0x0e, 0x01, 0x30, 0x10, "5"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x0e, 0x01, 0x40, 0x00, "Upright"				},
	{0x0e, 0x01, 0x40, 0x40, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x0e, 0x01, 0x80, 0x00, "Off"					},
	{0x0e, 0x01, 0x80, 0x80, "On"					},

	{0   , 0xfe, 0   ,    8, "Bonus Life"			},
	{0x0f, 0x01, 0x07, 0x03, "1M 2M 2.5M"			},
	{0x0f, 0x01, 0x07, 0x06, "1.2M 1.8M 2.5M"		},
	{0x0f, 0x01, 0x07, 0x07, "1.2M 1.8M"			},
	{0x0f, 0x01, 0x07, 0x04, "1.5M 2M"				},
	{0x0f, 0x01, 0x07, 0x05, "1.5M"					},
	{0x0f, 0x01, 0x07, 0x01, "1.8M 2.5M"			},
	{0x0f, 0x01, 0x07, 0x02, "1.8M"					},
	{0x0f, 0x01, 0x07, 0x00, "None"					},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x0f, 0x01, 0x08, 0x08, "Off"					},
	{0x0f, 0x01, 0x08, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Difficulty 1"			},
	{0x0f, 0x01, 0x30, 0x30, "Very Easy"			},
	{0x0f, 0x01, 0x30, 0x20, "Easy"					},
	{0x0f, 0x01, 0x30, 0x10, "Difficult"			},
	{0x0f, 0x01, 0x30, 0x00, "Very Difficult"		},

	{0   , 0xfe, 0   ,    4, "Difficulty 2"			},
	{0x0f, 0x01, 0xc0, 0xc0, "Very Easy"			},
	{0x0f, 0x01, 0xc0, 0x80, "Easy"					},
	{0x0f, 0x01, 0xc0, 0x40, "Difficult"			},
	{0x0f, 0x01, 0xc0, 0x00, "Very Difficult"		},
};

STDDIPINFO(Musicbal)

static void __fastcall speedbal_main_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x40:
			// coin counter = data & 0xc0
			flipscreen = data & 0x08;
		return;

		case 0x50:
			// ?
		return;
	}
}

static UINT8 __fastcall speedbal_main_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
			return DrvDips[1];

		case 0x10:
			return DrvDips[0];

		case 0x20:
			return DrvInputs[0];

		case 0x30:
			return DrvInputs[1];
	}

	return 0;
}

static void __fastcall speedbal_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
			BurnYM3812Write(0, port & 1, data);
		return;

		case 0x40: // led controls
		case 0x80:
		case 0xc1:
		return;

		case 0x82:
		return;	// nop
	}
}

static UINT8 __fastcall speedbal_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
			return BurnYM3812Read(0, port & 1);
	}

	return 0;
}

static tilemap_callback( bg )
{
	offs ^= 0xf0; // flipx
	INT32 attr = DrvBgRAM[offs * 2 + 1];
	INT32 code = DrvBgRAM[offs * 2 + 0] + ((attr & 0x30) << 4);
	INT32 color = attr & 0xf;

	TILE_SET_INFO(1, code, color, 0);
	*category = (color == 8) ? 1 : 0; // always comes _after_ TILE_SET_INFO()!
}

static tilemap_callback( fg )
{
	offs ^= 0x3e0; // flipx
	INT32 attr = DrvFgRAM[offs * 2 + 1];
	INT32 code = DrvFgRAM[offs * 2 + 0] + ((attr & 0x30) << 4);
	INT32 color = attr & 0xf;

	TILE_SET_INFO(0, code, color, 0);
	*category = (color == 9) ? 1 : 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	BurnYM3812Reset();
	ZetClose();

	flipscreen = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0	= Next; Next += 0x010000;
	DrvZ80ROM1	= Next; Next += 0x008000;

	DrvGfxROM0	= Next; Next += 0x010000;
	DrvGfxROM1	= Next; Next += 0x040000;
	DrvGfxROM2	= Next; Next += 0x020000;

	DrvPalette	= (UINT32*)Next; Next += 0x00300 * sizeof(UINT32);

	AllRam		= Next;

	DrvShareRAM	= Next; Next += 0x00400;
	DrvZ80RAM1	= Next; Next += 0x00400;
	DrvBgRAM	= Next; Next += 0x00200;
	DrvFgRAM	= Next; Next += 0x00800;

	DrvPalRAM	= Next; Next += 0x00f00;
	DrvSprRAM	= Next; Next += 0x00100;

	RamEnd		= Next;
	MemEnd		= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	UINT8 *tmp = (UINT8*)BurnMalloc(0x80000);
	if (tmp == NULL) {
		return 1;
	}
	INT32 Planes0[4] = { 0x4000*8+4, 0x4000*8, 4, 0 };
	INT32 XOffs0[ 8] = { STEP4(8+3,-1), STEP4(3,-1) };
	INT32 YOffs0[ 8] = { STEP8(0,16) };

	INT32 Planes1[4] = { STEP4(0,2) };
	INT32 XOffs1[16] = { 0, 1, 56, 57, 48, 49, 40, 41, 32, 33, 24, 25, 16, 17, 8, 9 };
	INT32 YOffs1[16] = { STEP16(0,64) };

	INT32 Planes2[4] = { STEP4(0,2) };
	INT32 XOffs2[16] = { 57, 56, 49, 48, 41, 40, 33, 32, 25, 24, 17, 16, 9, 8, 1, 0 };
	INT32 YOffs2[16] = { STEP16(0,64) };

	memcpy (tmp, DrvGfxROM0, 0x08000);

	GfxDecode(0x0400, 4,  8,  8, Planes0, XOffs0, YOffs0, 0x080, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x20000);

	GfxDecode(0x0400, 4, 16, 16, Planes1, XOffs1, YOffs1, 0x400, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x10000);

	GfxDecode(0x0200, 4, 16, 16, Planes2, XOffs2, YOffs2, 0x400, tmp, DrvGfxROM2);

	BurnFree (tmp);

	return 0;
}

static void musicbalPrgDecode()
{
	UINT8 xt[8] = { 0x05, 0x06, 0x84, 0x84, 0x00, 0x87, 0x84, 0x84 };

	INT32 swap[4][4] = {
		{ 1, 0, 7, 2 },
		{ 2, 7, 0, 1 },
		{ 7, 2, 1, 0 },
		{ 0, 2, 1, 7 }
	};

	for (INT32 i = 0; i < 0x8000; i++)
	{
		INT32 aidx = BIT(i,3)^(BIT(i,5)<<1)^(BIT(i,9)<<2);
		INT32 *st = swap[xt[aidx] & 3];

		DrvZ80ROM0[i] = BITSWAP08(DrvZ80ROM0[i], st[3], 6,5,4,3, st[2], st[1], st[0]) ^ xt[aidx];
	}
}

static void DrvGfxDescramble()
{
	UINT8 *tmp = (UINT8*)BurnMalloc(0x10000);

	for (INT32 i = 0; i < 0x200; i++)
	{
		INT32 j = BITSWAP16(i, 15,14,13,12,11,10,9,8,0,1,2,3,4,5,6,7);

		memcpy (tmp + i * 128, DrvGfxROM2 + j * 128, 128);
	}

	for (INT32 i = 0; i < 0x10000; i++) {
		DrvGfxROM2[i] = tmp[i] ^ 0xff; // copy & invert
	}

	BurnFree (tmp);
}

static INT32 DrvInit(INT32 game)
{
	BurnSetRefreshRate(56.4);

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x08000,  1, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x08000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x10000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x18000,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x08000,  9, 1)) return 1;

		if (game) musicbalPrgDecode();
		DrvGfxDescramble();
		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0xdbff, MAP_ROM);
	ZetMapMemory(DrvShareRAM,	0xdc00, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvBgRAM,		0xe000, 0xe1ff, MAP_RAM);
	ZetMapMemory(DrvFgRAM,		0xe800, 0xefff, MAP_RAM);
	ZetMapMemory(DrvPalRAM,		0xf000, 0xfeff, MAP_RAM); // 0-5ff
	ZetMapMemory(DrvSprRAM,		0xff00, 0xffff, MAP_RAM);
	ZetSetOutHandler(speedbal_main_write_port);
	ZetSetInHandler(speedbal_main_read_port);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,	0xd800, 0xdbff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,	0xdc00, 0xdfff, MAP_RAM);
	ZetSetOutHandler(speedbal_sound_write_port);
	ZetSetInHandler(speedbal_sound_read_port);
	ZetClose();

	BurnYM3812Init(1, 4000000, NULL, 0);
	BurnTimerAttachYM3812(&ZetConfig, 4000000);
	BurnYM3812SetRoute(0, BURN_SND_YM3812_ROUTE, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	// foreground
	GenericTilemapInit(0,  TILEMAP_SCAN_COLS, fg_map_callback,  8,  8, 32, 32);
	GenericTilemapCategoryConfig(0, 4);
	GenericTilemapSetTransMask(0, 0, 0xffff); // fg
	GenericTilemapSetTransMask(0, 1, 0x0001); // fg
	GenericTilemapSetTransMask(0, 2, 0x0001); // bg
	GenericTilemapSetTransMask(0, 3, 0x0001); // bg

	// background
	GenericTilemapInit(1,  TILEMAP_SCAN_COLS, bg_map_callback, 16, 16, 16, 16);
	GenericTilemapCategoryConfig(1, 4);
	GenericTilemapSetTransMask(1, 0, 0xffff); // fg
	GenericTilemapSetTransMask(1, 1, 0x00f7); // fg
	GenericTilemapSetTransMask(1, 2, 0x0000); // bg
	GenericTilemapSetTransMask(1, 3, 0x0000); // bg

	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);

	GenericTilemapSetGfx(0, DrvGfxROM0, 4,  8,  8, 0x10000, 0x100, 0xf);
	GenericTilemapSetGfx(1, DrvGfxROM1, 4, 16, 16, 0x40000, 0x200, 0xf);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	BurnYM3812Exit();

	ZetExit();

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteUpdate() // RRRRGGGGBBBBxxxx BE
{
	for (INT32 i = 0; i < 0x600; i+=2)
	{
		UINT8 r = DrvPalRAM[i+0] >> 4;
		UINT8 g = DrvPalRAM[i+0] & 0xf;
		UINT8 b = DrvPalRAM[i+1] >> 4;

		DrvPalette[i/2] = BurnHighCol(r+r*16, g+g*16, b+b*16, 0);
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0; offs < 0x100; offs += 4)
	{
		if ((DrvSprRAM[offs + 2] & 0x80) == 0)
			continue;

		INT32 sx = 243 - DrvSprRAM[offs + 3];
		INT32 sy = 239 - DrvSprRAM[offs + 0];

		INT32 code = (DrvSprRAM[offs + 1]) | ((DrvSprRAM[offs + 2] & 0x40) << 2);
		INT32 color = DrvSprRAM[offs + 2] & 0x0f;

		if (flipscreen)
		{
			sx = 246 - sx;
			sy = 238 - sy;
		}

		Draw16x16MaskTile(pTransDraw, code, sx, sy - 16, flipscreen, flipscreen, color, 4, 0, 0, DrvGfxROM2);
	}
}

static INT32 DrvDraw()
{
//	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
//	}

	BurnTransferClear();

	if (nBurnLayer & 1) GenericTilemapDraw(1, pTransDraw, TMAP_DRAWLAYER1); // bg bg_mask
	if (nBurnLayer & 2) GenericTilemapDraw(0, pTransDraw, TMAP_DRAWLAYER1); // fg bg_mask
	if (nSpriteEnable & 1) draw_sprites();
	if (nBurnLayer & 4) GenericTilemapDraw(1, pTransDraw, TMAP_DRAWLAYER0); // bg fg_mask
	if (nBurnLayer & 8) GenericTilemapDraw(0, pTransDraw, TMAP_DRAWLAYER0); // fg fg_mask

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, 2);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	ZetNewFrame();

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 40000000 / 564, 40000000 / 564 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun((nCyclesTotal[0] * (i + 1) / nInterleave) - nCyclesDone[0]);
		if (i == 240) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		INT32 nCycles = ZetTotalCycles();
		ZetClose();

		ZetOpen(1);
		BurnTimerUpdateYM3812(nCycles);
		if ((i & 0x1f) == 0x1f) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD); // 8x / frame
		ZetClose();
	}

	ZetOpen(1);

	BurnTimerEndFrameYM3812(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		BurnYM3812Update(pBurnSoundOut, nBurnSoundLen);
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
		*pnMin = 0x029705;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);

		BurnYM3812Scan(nAction, pnMin);

		SCAN_VAR(flipscreen);
	}

	return 0;
}


// Speed Ball

static struct BurnRomInfo speedbalRomDesc[] = {
	{ "sb1.bin",	0x8000, 0x1c242e34, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "sb3.bin",	0x8000, 0x7682326a, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "sb2.bin",	0x8000, 0xe6a6d9b7, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "sb10.bin",	0x8000, 0x36dea4bf, 3 | BRF_GRA },           //  3 Characters

	{ "sb9.bin",	0x8000, 0xb567e85e, 4 | BRF_GRA },           //  4 Background Tiles
	{ "sb5.bin",	0x8000, 0xb0eae4ba, 4 | BRF_GRA },           //  5
	{ "sb8.bin",	0x8000, 0xd2bfbdb6, 4 | BRF_GRA },           //  6
	{ "sb4.bin",	0x8000, 0x1d23a130, 4 | BRF_GRA },           //  7

	{ "sb7.bin",	0x8000, 0x9f1b33d1, 5 | BRF_GRA },           //  8 Sprites (scrambled)
	{ "sb6.bin",	0x8000, 0x0e2506eb, 5 | BRF_GRA },           //  9
};

STD_ROM_PICK(speedbal)
STD_ROM_FN(speedbal)

static INT32 SpeedbalInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvSpeedbal = {
	"speedbal", NULL, NULL, NULL, "1987",
	"Speed Ball\0", NULL, "Tecfri / Desystem S.A.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_PINBALL, 0,
	NULL, speedbalRomInfo, speedbalRomName, NULL, NULL, NULL, NULL, SpeedbalInputInfo, SpeedbalDIPInfo,
	SpeedbalInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	224, 256, 3, 4
};


// Music Ball

static struct BurnRomInfo musicbalRomDesc[] = {
	{ "01.bin",		0x8000, 0x412298a2, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code (encrypted)
	{ "03.bin",		0x8000, 0xfdf14446, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "02.bin",		0x8000, 0xb7d3840d, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "10.bin",		0x8000, 0x5afd3c42, 3 | BRF_GRA },           //  3 Characters

	{ "09.bin",		0x8000, 0xdcde4233, 4 | BRF_GRA },           //  4 Background Tiles
	{ "05.bin",		0x8000, 0xe1eec437, 4 | BRF_GRA },           //  5
	{ "08.bin",		0x8000, 0x7e7af52b, 4 | BRF_GRA },           //  6
	{ "04.bin",		0x8000, 0xbf931a33, 4 | BRF_GRA },           //  7

	{ "07.bin",		0x8000, 0x310e1e23, 5 | BRF_GRA },           //  8 Sprites (scrambled)
	{ "06.bin",		0x8000, 0x2e7772f8, 5 | BRF_GRA },           //  9
};

STD_ROM_PICK(musicbal)
STD_ROM_FN(musicbal)

static INT32 MusicbalInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvMusicbal = {
	"musicbal", NULL, NULL, NULL, "1988",
	"Music Ball\0", NULL, "Tecfri / Desystem S.A.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_PINBALL, 0,
	NULL, musicbalRomInfo, musicbalRomName, NULL, NULL, NULL, NULL, SpeedbalInputInfo, MusicbalDIPInfo,
	MusicbalInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	224, 256, 3, 4
};
