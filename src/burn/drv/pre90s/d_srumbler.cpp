// FB Neo The Speed Rumbler driver module
// Based on MAME driver by Paul Leaman

#include "tiles_generic.h"
#include "z80_intf.h"
#include "m6809_intf.h"
#include "burn_ym2203.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvM6809ROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvPROM;
static UINT8 *DrvM6809RAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprBuf;
static UINT8 *DrvBgRAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvZ80RAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 *DrvBank;
static UINT8 *DrvScroll;
static UINT8 *flipscreen;
static UINT8 *soundlatch;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;
static INT32 nExtraCycles;

static struct BurnInputInfo SrumblerInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Srumbler)

static struct BurnDIPInfo SrumblerDIPList[]=
{
	DIP_OFFSET(0x11)

	{0x00, 0xff, 0xff, 0xff, NULL					},
	{0x01, 0xff, 0xff, 0x77, NULL					},

	{0   , 0xfe, 0   ,    8, "Coin B"				},
	{0x00, 0x01, 0x07, 0x00, "4 Coins 1 Credits"	},
	{0x00, 0x01, 0x07, 0x01, "3 Coins 1 Credits"	},
	{0x00, 0x01, 0x07, 0x02, "2 Coins 1 Credits"	},
	{0x00, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x00, 0x01, 0x07, 0x04, "1 Coin  4 Credits"	},
	{0x00, 0x01, 0x07, 0x03, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin A"				},
	{0x00, 0x01, 0x38, 0x00, "4 Coins 1 Credits"	},
	{0x00, 0x01, 0x38, 0x08, "3 Coins 1 Credits"	},
	{0x00, 0x01, 0x38, 0x10, "2 Coins 1 Credits"	},
	{0x00, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0x38, 0x30, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0x38, 0x28, "1 Coin  3 Credits"	},
	{0x00, 0x01, 0x38, 0x20, "1 Coin  4 Credits"	},
	{0x00, 0x01, 0x38, 0x18, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x00, 0x01, 0x40, 0x40, "Off"					},
	{0x00, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x00, 0x01, 0x80, 0x80, "Off"					},
	{0x00, 0x01, 0x80, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x01, 0x01, 0x03, 0x03, "3"					},
	{0x01, 0x01, 0x03, 0x02, "4"					},
	{0x01, 0x01, 0x03, 0x01, "5"					},
	{0x01, 0x01, 0x03, 0x00, "7"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x01, 0x01, 0x04, 0x00, "Upright"				},
	{0x01, 0x01, 0x04, 0x04, "Cocktail"				},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x01, 0x01, 0x18, 0x18, "20k 70k and every 70k"},
	{0x01, 0x01, 0x18, 0x10, "30k 80k and every 80k"},
	{0x01, 0x01, 0x18, 0x08, "20k 80k"				},
	{0x01, 0x01, 0x18, 0x00, "30k 80k"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x01, 0x01, 0x60, 0x40, "Easy"					},
	{0x01, 0x01, 0x60, 0x60, "Normal"				},
	{0x01, 0x01, 0x60, 0x20, "Difficult"			},
	{0x01, 0x01, 0x60, 0x00, "Very Difficult"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x01, 0x01, 0x80, 0x00, "No"					},
	{0x01, 0x01, 0x80, 0x80, "Yes"					},
};

STDDIPINFO(Srumbler)

static void bankswitch(INT32 data)
{
	DrvBank[0] = data;

	for (INT32 i = 0x05; i < 0x10; i++)
	{
		INT32 bank = DrvPROM[(data & 0xf0) | i] | DrvPROM[0x100 | ((data & 0x0f) << 4) | i];

		M6809MapMemory(DrvM6809ROM + bank * 0x1000, 0x1000 * i, 0x1000 * i + 0x0fff, MAP_ROM);
	}
}

static void srumbler_main_write(UINT16 address, UINT8 data)
{
	if (address >= 0x7000 && address <= 0x73ff) {
		DrvPalRAM[address - 0x7000] = data;
		DrvRecalc = 1;
	}
	switch (address)
	{
		case 0x4008:
			bankswitch(data);
		return;

		case 0x4009:
			*flipscreen = 0; // data & 1; ignore flipscreen
		return;

		case 0x400a:
		case 0x400b:
		case 0x400c:
		case 0x400d:
			DrvScroll[address - 0x400a] = data;
		return;

		case 0x400e:
			*soundlatch = data;
		return;
	}
}

static UINT8 srumbler_main_read(UINT16 address)
{
	switch (address)
	{
		case 0x4008:
			return DrvInputs[0];

		case 0x4009:
			return DrvInputs[1];

		case 0x400a:
			return DrvInputs[2];

		case 0x400b:
			return DrvDips[0];

		case 0x400c:
			return DrvDips[1];
	}

	return 0;
}

static void __fastcall srumbler_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x8000:
		case 0x8001:
			BurnYM2203Write(0, address & 1, data);
		return;

		case 0xa000:
		case 0xa001:
			BurnYM2203Write(1, address & 1, data);
		return;
	}
}

static UINT8 __fastcall srumbler_sound_read(UINT16 address)
{
	if (address == 0xe000) {
		return *soundlatch;
	}

	return 0;
}

static void DrvPalRAMInit()
{
	// init the palette ram with a pattern to make
	// the bootup messages visible on first boot
	for (INT32 i = 0; i < 0x200; i++) {
		const UINT8 r = ((i & 1) ? 0xff : 0);
		const UINT8 g = ((i & 2) ? 0xff : 0);
		const UINT8 b = ((i & 4) ? 0xff : 0);
		const UINT32 d = (r & 0xf) << 12 | (g & 0xf) << 8 | (b & 0xf) << 4;

		DrvPalRAM[i * 2 + 0] = d >> 8;
		DrvPalRAM[i * 2 + 1] = d & 0xff;
	}
	DrvRecalc = 1;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	M6809Open(0);
	bankswitch(0);
	M6809Reset();
	M6809Close();

	ZetOpen(0);
	ZetReset();
	BurnYM2203Reset();
	ZetClose();

	nExtraCycles = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6809ROM	= Next; Next += 0x040000;
	DrvZ80ROM	= Next; Next += 0x010000;

	DrvGfxROM0	= Next; Next += 0x020000;
	DrvGfxROM1	= Next; Next += 0x080000;
	DrvGfxROM2	= Next; Next += 0x080000;

	DrvPROM		= Next; Next += 0x000200;

	DrvPalette	= (UINT32*)Next; Next += 0x00200 * sizeof(UINT32);

	AllRam		= Next;

	DrvM6809RAM	= Next; Next += 0x001e00;
	DrvSprRAM	= Next; Next += 0x000200;
	DrvSprBuf	= Next; Next += 0x000200;

	DrvBgRAM	= Next; Next += 0x002000;
	DrvFgRAM	= Next; Next += 0x001000;
	DrvPalRAM	= Next; Next += 0x000400;

	DrvZ80RAM	= Next; Next += 0x000800;

	DrvBank		= Next; Next += 0x000001;
	DrvScroll	= Next; Next += 0x000004;

	flipscreen	= Next; Next += 0x000001;
	soundlatch	= Next; Next += 0x000001;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[2]  = { 0x000004,  0x000000 };
	INT32 Plane1[4]  = { 0x100004,  0x100000, 0x000004, 0x000000 };
	INT32 Plane2[4]  = { 0x180000,  0x100000, 0x080000, 0x000000 };
	INT32 XOffs0[16] = { 0x000, 0x001, 0x002, 0x003, 0x008, 0x009, 0x00a, 0x00b,
			   0x100, 0x101, 0x102, 0x103, 0x108, 0x109, 0x10a, 0x10b };
	INT32 XOffs1[16] = { 0x000, 0x001, 0x002, 0x003, 0x004, 0x005, 0x006, 0x007,
			   0x080, 0x081, 0x082, 0x083, 0x084, 0x085, 0x086, 0x087 };
	INT32 YOffs0[16] = { 0x000, 0x010, 0x020, 0x030, 0x040, 0x050, 0x060, 0x070,
			   0x080, 0x090, 0x0a0, 0x0b0, 0x0c0, 0x0d0, 0x0e0, 0x0f0 };
	INT32 YOffs1[16] = { 0x000, 0x008, 0x010, 0x018, 0x020, 0x028, 0x030, 0x038,
			   0x040, 0x048, 0x050, 0x058, 0x060, 0x068, 0x070, 0x078 };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x40000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x08000); // fg

	GfxDecode(0x400, 2,  8,  8, Plane0, XOffs0, YOffs0, 0x080, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x40000); // bg

	GfxDecode(0x800, 4, 16, 16, Plane1, XOffs0, YOffs0, 0x200, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x40000); // spr

	GfxDecode(0x800, 4, 16, 16, Plane2, XOffs1, YOffs1, 0x100, tmp, DrvGfxROM2);

	BurnFree (tmp);

	return 0;
}

static tilemap_callback( bg )
{
	INT32 Attr = DrvBgRAM[2 * offs + 0];
	INT32 Code = DrvBgRAM[2 * offs + 1] + ((Attr & 0x07) << 8);

	TILE_SET_INFO(0, Code, (Attr >> 5) & 7, TILE_FLIPXY((Attr >> 3) & 1));
	sTile->category = (Attr >> 4) & 1;
}

static tilemap_callback( fg )
{
	INT32 Attr = DrvFgRAM[2 * offs + 0];
	INT32 Code = DrvFgRAM[2 * offs + 1] + ((Attr & 0x03) << 8);

	TILE_SET_INFO(1, Code, (Attr >> 2) & 0xf, (Attr & 0x40) ? TILE_OPAQUE : 0);
}

static void tmap_init()
{
	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_COLS, bg_map_callback, 16, 16, 64, 64);
	GenericTilemapInit(1, TILEMAP_SCAN_COLS, fg_map_callback,  8,  8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM1, 4, 16, 16, 0x800 * 16 * 16, 0x80, 0x07);
	GenericTilemapSetGfx(1, DrvGfxROM0, 2,  8,  8, 0x400 *  8 *  8, 0x1c0, 0x0f);
	GenericTilemapSetTransSplit(0, 0, 0xffff, 0x0000);
	GenericTilemapSetTransSplit(0, 1, 0x07ff, 0xf800);
	GenericTilemapSetTransparent(1, 3);
	GenericTilemapSetOffsets(TMAP_GLOBAL, -80,-8);
}

static INT32 DrvInit()
{
	BurnSetRefreshRate(57.5);
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvM6809ROM + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x08000,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x10000,  2, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x18000,  3, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x20000,  4, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x28000,  5, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x30000,  6, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x38000,  7, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM   + 0x00000,  8, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0  + 0x00000,  9, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1  + 0x00000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x08000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x10000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x18000, 13, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x20000, 14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x28000, 15, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x30000, 16, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x38000, 17, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2  + 0x00000, 18, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2  + 0x08000, 19, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2  + 0x10000, 20, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2  + 0x18000, 21, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2  + 0x20000, 22, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2  + 0x28000, 23, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2  + 0x30000, 24, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2  + 0x38000, 25, 1)) return 1;

		if (BurnLoadRom(DrvPROM  + 0x00000, 26, 1)) return 1;
		if (BurnLoadRom(DrvPROM  + 0x00100, 27, 1)) return 1;

		for (INT32 i = 0; i < 0x100; i++) {
			DrvPROM[i + 0x000]  = (DrvPROM[i] & 0x03) << 4;
			DrvPROM[i + 0x100] &= 0x0f;
		}

		DrvGfxDecode();
	}

	M6809Init(0);
	M6809Open(0);
	M6809MapMemory(DrvM6809RAM,		0x0000, 0x1dff, MAP_RAM);
	M6809MapMemory(DrvSprRAM,		0x1e00, 0x1fff, MAP_RAM);
	M6809MapMemory(DrvBgRAM,		0x2000, 0x3fff, MAP_RAM);
	M6809MapMemory(DrvFgRAM,		0x5000, 0x5fff, MAP_WRITE);
	//M6809MapMemory(DrvPalRAM,		0x7000, 0x73ff, MAP_WRITE); // in handler
	M6809SetReadHandler(srumbler_main_read);
	M6809SetWriteHandler(srumbler_main_write);
	M6809Close();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM, 0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM, 0xc000, 0xc7ff, MAP_RAM);
	ZetSetWriteHandler(srumbler_sound_write);
	ZetSetReadHandler(srumbler_sound_read);
	ZetClose();

	BurnYM2203Init(2, 4000000, NULL, 0);
	BurnTimerAttachZet(4000000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE, 0.30, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.10, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.10, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.10, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_YM2203_ROUTE, 0.30, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_1, 0.10, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_2, 0.10, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_3, 0.10, BURN_SND_ROUTE_BOTH);

	tmap_init();

	DrvDoReset();

	DrvPalRAMInit(); // only once @ init, after reset!

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6809Exit();
	ZetExit();

	BurnYM2203Exit();

	BurnFreeMemIndex();

	return 0;
}

static void draw_sprites()
{
	for (INT32 offs = 0x200-4; offs >= 0; offs -= 4)
	{
		INT32 attr  = DrvSprBuf[offs + 1];
		INT32 code  = DrvSprBuf[offs + 0] | ((attr & 0xe0) << 3);
		INT32 sy    = DrvSprBuf[offs + 2];
		INT32 sx    = DrvSprBuf[offs + 3] | ((attr & 0x01) << 8);
		INT32 color = (attr & 0x1c) >> 2;
		INT32 flipy = (attr & 0x02);
		INT32 flipx = 0;

		if (*flipscreen)
		{
			sx = 496 - sx;
			sy = 240 - sy;
			flipy ^= 2;
			flipx = 1;
		}

		sx -= 80;
		sy -= 8;

		Draw16x16MaskTile(pTransDraw, code, sx, sy, flipx, flipy, color, 4, 15, 0x100, DrvGfxROM2);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x400; i+=2) {
			INT32 d = DrvPalRAM[i + 1] | (DrvPalRAM[i + 0] << 8);

			UINT8 r = (d >> 12);
			UINT8 g = (d >>  8) & 0x0f;
			UINT8 b = (d >>  4) & 0x0f;

			DrvPalette[i >> 1] = BurnHighCol((r << 4) | r, (g << 4) | g, (b << 4) | b, 0);
		}
		DrvRecalc = 0;
	}

	BurnTransferClear();

	GenericTilemapSetScrollX(0, (DrvScroll[1] << 8) | DrvScroll[0]);
	GenericTilemapSetScrollY(0, (DrvScroll[3] << 8) | DrvScroll[2]);

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, TMAP_DRAWLAYER1);
	if (nSpriteEnable & 1) draw_sprites();
	if (nBurnLayer & 2) GenericTilemapDraw(0, pTransDraw, TMAP_DRAWLAYER0);
	if (nBurnLayer & 4) GenericTilemapDraw(1, pTransDraw, 0);

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
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { ((double)8000000 * 100 / nBurnFPS), ((double)4000000 * 100 / nBurnFPS) };
	INT32 nCyclesDone[2] = { nExtraCycles, 0 };

	M6809Open(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++) {
		CPU_RUN(0, M6809);

		if (i == 8) M6809SetIRQLine(1, CPU_IRQSTATUS_HOLD);
		if (i == nInterleave - 1) M6809SetIRQLine(0, CPU_IRQSTATUS_HOLD);

		CPU_RUN_TIMER(1);

		if (i % (nInterleave/4) == (nInterleave/4) - 1) {
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
	}

	nExtraCycles = nCyclesDone[0] - nCyclesTotal[0];

	if (pBurnSoundOut) BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);

	ZetClose();
	M6809Close();

	if (pBurnDraw) {
		DrvDraw();
	}

	memcpy (DrvSprBuf, DrvSprRAM, 0x200);

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029706;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		M6809Scan(nAction);
		ZetScan(nAction);

		BurnYM2203Scan(nAction, pnMin);

		SCAN_VAR(nExtraCycles);
	}

	if (nAction & ACB_WRITE) {
		M6809Open(0);
		bankswitch(DrvBank[0]);
		M6809Close();
	}

	return 0;
}


// The Speed Rumbler (set 1)

static struct BurnRomInfo srumblerRomDesc[] = {
	{ "rc04.14e",		0x8000, 0xa68ce89c, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "rc03.13e", 		0x8000, 0x87bda812, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rc02.12e", 		0x8000, 0xd8609cca, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rc01.11e", 		0x8000, 0x27ec4776, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "rc09.14f", 		0x8000, 0x2146101d, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "rc08.13f", 		0x8000, 0x838369a6, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "rc07.12f",		0x8000, 0xde785076, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "rc06.11f",		0x8000, 0xa70f4fd4, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "rc05.2f", 		0x8000, 0x0177cebe, 2 | BRF_PRG | BRF_ESS }, //  8 Z80 Code

	{ "rc10.6g",		0x4000, 0xadabe271, 3 | BRF_GRA },           //  9 Characters

	{ "rc11.11a",		0x8000, 0x5fa042ba, 4 | BRF_GRA },           // 10 Tiles
	{ "rc12.13a",		0x8000, 0xa2db64af, 4 | BRF_GRA },           // 11
	{ "rc13.14a",		0x8000, 0xf1df5499, 4 | BRF_GRA },           // 12
	{ "rc14.15a",		0x8000, 0xb22b31b3, 4 | BRF_GRA },           // 13
	{ "rc15.11c",		0x8000, 0xca3a3af3, 4 | BRF_GRA },           // 14
	{ "rc16.13c",		0x8000, 0xc49a4a11, 4 | BRF_GRA },           // 15
	{ "rc17.14c",		0x8000, 0xaa80aaab, 4 | BRF_GRA },           // 16
	{ "rc18.15c",		0x8000, 0xce67868e, 4 | BRF_GRA },           // 17

	{ "rc20.15e",		0x8000, 0x3924c861, 5 | BRF_GRA },           // 18 Sprites
	{ "rc19.14e",		0x8000, 0xff8f9129, 5 | BRF_GRA },           // 19
	{ "rc22.15f",		0x8000, 0xab64161c, 5 | BRF_GRA },           // 20
	{ "rc21.14f",		0x8000, 0xfd64bcd1, 5 | BRF_GRA },           // 21
	{ "rc24.15h",		0x8000, 0xc972af3e, 5 | BRF_GRA },           // 22
	{ "rc23.14h",		0x8000, 0x8c9abf57, 5 | BRF_GRA },           // 23
	{ "rc26.15j",		0x8000, 0xd4f1732f, 5 | BRF_GRA },           // 24
	{ "rc25.14j",		0x8000, 0xd2a4ea4f, 5 | BRF_GRA },           // 25

	{ "63s141.12a",		0x0100, 0x8421786f, 6 | BRF_PRG | BRF_ESS }, // 26 Rom Bank Proms
	{ "63s141.13a",		0x0100, 0x6048583f, 6 | BRF_PRG | BRF_ESS }, // 27

	{ "63s141.8j",		0x0100, 0x1a89a7ff, 0 | BRF_OPT },           // 28 Priority Prom
};

STD_ROM_PICK(srumbler)
STD_ROM_FN(srumbler)

struct BurnDriver BurnDrvSrumbler = {
	"srumbler", NULL, NULL, NULL, "1986",
	"The Speed Rumbler (set 1)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_RUNGUN, 0,
	NULL, srumblerRomInfo, srumblerRomName, NULL, NULL, NULL, NULL, SrumblerInputInfo, SrumblerDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x200, 240, 352, 3, 4
};


// The Speed Rumbler (set 2)

static struct BurnRomInfo srumblr2RomDesc[] = {
	{ "rc04.14e",		0x8000, 0xa68ce89c, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "rc03.13e", 		0x8000, 0xe82f78d4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rc02.12e",		0x8000, 0x009a62d8, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rc01.11e",		0x8000, 0x2ac48d1d, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "rc09.14f",		0x8000, 0x64f23e72, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "rc08.13f", 		0x8000, 0x74c71007, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "rc07.12f",		0x8000, 0xde785076, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "rc06.11f",		0x8000, 0xa70f4fd4, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "rc05.2f",		0x8000, 0xea04fa07, 2 | BRF_PRG | BRF_ESS }, //  8 Z80 Code

	{ "rc10.6g",		0x4000, 0xadabe271, 3 | BRF_GRA },           //  9 Characters

	{ "rc11.11a",		0x8000, 0x5fa042ba, 4 | BRF_GRA },           // 10 Tiles
	{ "rc12.13a",		0x8000, 0xa2db64af, 4 | BRF_GRA },           // 11
	{ "rc13.14a",		0x8000, 0xf1df5499, 4 | BRF_GRA },           // 12
	{ "rc14.15a",		0x8000, 0xb22b31b3, 4 | BRF_GRA },           // 13
	{ "rc15.11c",		0x8000, 0xca3a3af3, 4 | BRF_GRA },           // 14
	{ "rc16.13c",		0x8000, 0xc49a4a11, 4 | BRF_GRA },           // 15
	{ "rc17.14c",		0x8000, 0xaa80aaab, 4 | BRF_GRA },           // 16
	{ "rc18.15c",		0x8000, 0xce67868e, 4 | BRF_GRA },           // 17

	{ "rc20.15e",		0x8000, 0x3924c861, 5 | BRF_GRA },           // 18 Sprites
	{ "rc19.14e",		0x8000, 0xff8f9129, 5 | BRF_GRA },           // 19
	{ "rc22.15f",		0x8000, 0xab64161c, 5 | BRF_GRA },           // 20
	{ "rc21.14f",		0x8000, 0xfd64bcd1, 5 | BRF_GRA },           // 21
	{ "rc24.15h",		0x8000, 0xc972af3e, 5 | BRF_GRA },           // 22
	{ "rc23.14h",		0x8000, 0x8c9abf57, 5 | BRF_GRA },           // 23
	{ "rc26.15j",		0x8000, 0xd4f1732f, 5 | BRF_GRA },           // 24
	{ "rc25.14j",		0x8000, 0xd2a4ea4f, 5 | BRF_GRA },           // 25

	{ "63s141.12a",		0x0100, 0x8421786f, 6 | BRF_PRG | BRF_ESS }, // 26 Rom Bank Proms
	{ "63s141.13a",		0x0100, 0x6048583f, 6 | BRF_PRG | BRF_ESS }, // 27

	{ "63s141.8j",		0x0100, 0x1a89a7ff, 0 | BRF_OPT },           // 28 Priority Prom
};

STD_ROM_PICK(srumblr2)
STD_ROM_FN(srumblr2)

struct BurnDriver BurnDrvSrumblr2 = {
	"srumbler2", "srumbler", NULL, NULL, "1986",
	"The Speed Rumbler (set 2)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_RUNGUN, 0,
	NULL, srumblr2RomInfo, srumblr2RomName, NULL, NULL, NULL, NULL, SrumblerInputInfo, SrumblerDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x200, 240, 352, 3, 4
};


// The Speed Rumbler (set 3)

static struct BurnRomInfo srumblr3RomDesc[] = {
	{ "rc04.14e",		0x8000, 0xa68ce89c, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "rc03.13e", 		0x8000, 0x0a21992b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rc02.12e",		0x8000, 0x009a62d8, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rc01.11e",		0x8000, 0x2ac48d1d, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "rc09.14f",		0x8000, 0x64f23e72, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "rc08.13f", 		0x8000, 0xe361b55c, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "rc07.12f",		0x8000, 0xde785076, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "rc06.11f",		0x8000, 0xa70f4fd4, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "rc05.2f",		0x8000, 0xea04fa07, 2 | BRF_PRG | BRF_ESS }, //  8 Z80 Code

	{ "rc10.6g",		0x4000, 0xadabe271, 3 | BRF_GRA },           //  9 Characters

	{ "rc11.11a",		0x8000, 0x5fa042ba, 4 | BRF_GRA },           // 10 Tiles
	{ "rc12.13a",		0x8000, 0xa2db64af, 4 | BRF_GRA },           // 11
	{ "rc13.14a",		0x8000, 0xf1df5499, 4 | BRF_GRA },           // 12
	{ "rc14.15a",		0x8000, 0xb22b31b3, 4 | BRF_GRA },           // 13
	{ "rc15.11c",		0x8000, 0xca3a3af3, 4 | BRF_GRA },           // 14
	{ "rc16.13c",		0x8000, 0xc49a4a11, 4 | BRF_GRA },           // 15
	{ "rc17.14c",		0x8000, 0xaa80aaab, 4 | BRF_GRA },           // 16
	{ "rc18.15c",		0x8000, 0xce67868e, 4 | BRF_GRA },           // 17

	{ "rc20.15e",		0x8000, 0x3924c861, 5 | BRF_GRA },           // 18 Sprites
	{ "rc19.14e",		0x8000, 0xff8f9129, 5 | BRF_GRA },           // 19
	{ "rc22.15f",		0x8000, 0xab64161c, 5 | BRF_GRA },           // 20
	{ "rc21.14f",		0x8000, 0xfd64bcd1, 5 | BRF_GRA },           // 21
	{ "rc24.15h",		0x8000, 0xc972af3e, 5 | BRF_GRA },           // 22
	{ "rc23.14h",		0x8000, 0x8c9abf57, 5 | BRF_GRA },           // 23
	{ "rc26.15j",		0x8000, 0xd4f1732f, 5 | BRF_GRA },           // 24
	{ "rc25.14j",		0x8000, 0xd2a4ea4f, 5 | BRF_GRA },           // 25

	{ "63s141.12a",		0x0100, 0x8421786f, 6 | BRF_PRG | BRF_ESS }, // 26 Rom Bank Proms
	{ "63s141.13a",		0x0100, 0x6048583f, 6 | BRF_PRG | BRF_ESS }, // 27

	{ "63s141.8j",		0x0100, 0x1a89a7ff, 0 | BRF_OPT },           // 28 Priority Prom
};

STD_ROM_PICK(srumblr3)
STD_ROM_FN(srumblr3)

struct BurnDriver BurnDrvSrumblr3 = {
	"srumbler3", "srumbler", NULL, NULL, "1986",
	"The Speed Rumbler (set 3)\0", NULL, "Capcom (Tecfri license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_RUNGUN, 0,
	NULL, srumblr3RomInfo, srumblr3RomName, NULL, NULL, NULL, NULL, SrumblerInputInfo, SrumblerDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x200, 240, 352, 3, 4
};


// Rush & Crash (Japan)

static struct BurnRomInfo rushcrshRomDesc[] = {
	{ "rc04.14e",		0x8000, 0xa68ce89c, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "rc03.13e",		0x8000, 0xa49c9be0, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rc02.12e",		0x8000, 0x009a62d8, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rc01.11e",		0x8000, 0x2ac48d1d, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "rc09.14f",		0x8000, 0x64f23e72, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "rc08.13f",		0x8000, 0x2c25874b, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "rc07.12f",		0x8000, 0xde785076, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "rc06.11f",		0x8000, 0xa70f4fd4, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "rc05.2f",		0x8000, 0xea04fa07, 2 | BRF_PRG | BRF_ESS }, //  8 Z80 Code

	{ "rc10.6g", 		0x4000, 0x0a3c0b0d, 3 | BRF_GRA },           //  9 Characters

	{ "rc11.11a",		0x8000, 0x5fa042ba, 4 | BRF_GRA },           // 10 Tiles
	{ "rc12.13a",		0x8000, 0xa2db64af, 4 | BRF_GRA },           // 11
	{ "rc13.14a",		0x8000, 0xf1df5499, 4 | BRF_GRA },           // 12
	{ "rc14.15a",		0x8000, 0xb22b31b3, 4 | BRF_GRA },           // 13
	{ "rc15.11c",		0x8000, 0xca3a3af3, 4 | BRF_GRA },           // 14
	{ "rc16.13c",		0x8000, 0xc49a4a11, 4 | BRF_GRA },           // 15
	{ "rc17.14c",		0x8000, 0xaa80aaab, 4 | BRF_GRA },           // 16
	{ "rc18.15c",		0x8000, 0xce67868e, 4 | BRF_GRA },           // 17

	{ "rc20.15e",		0x8000, 0x3924c861, 5 | BRF_GRA },           // 18 Sprites
	{ "rc19.14e",		0x8000, 0xff8f9129, 5 | BRF_GRA },           // 19
	{ "rc22.15f",		0x8000, 0xab64161c, 5 | BRF_GRA },           // 20
	{ "rc21.14f",		0x8000, 0xfd64bcd1, 5 | BRF_GRA },           // 21
	{ "rc24.15h",		0x8000, 0xc972af3e, 5 | BRF_GRA },           // 22
	{ "rc23.14h",		0x8000, 0x8c9abf57, 5 | BRF_GRA },           // 23
	{ "rc26.15j",		0x8000, 0xd4f1732f, 5 | BRF_GRA },           // 24
	{ "rc25.14j",		0x8000, 0xd2a4ea4f, 5 | BRF_GRA },           // 25

	{ "63s141.12a",		0x0100, 0x8421786f, 6 | BRF_PRG | BRF_ESS }, // 26 Rom Bank Proms
	{ "63s141.13a",		0x0100, 0x6048583f, 6 | BRF_PRG | BRF_ESS }, // 27

	{ "63s141.8j",		0x0100, 0x1a89a7ff, 0 | BRF_OPT },           // 28 Priority Prom
};

STD_ROM_PICK(rushcrsh)
STD_ROM_FN(rushcrsh)

struct BurnDriver BurnDrvRushcrsh = {
	"rushcrsh", "srumbler", NULL, NULL, "1986",
	"Rush & Crash (Japan)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_RUNGUN, 0,
	NULL, rushcrshRomInfo, rushcrshRomName, NULL, NULL, NULL, NULL, SrumblerInputInfo, SrumblerDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x200, 240, 352, 3, 4
};

