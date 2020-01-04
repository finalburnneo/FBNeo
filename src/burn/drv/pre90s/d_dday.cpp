// FB Alpha D-Day (Olympia) driver module
// Based on MAME driver by Zsolt Vasvari

#include "tiles_generic.h"
#include "z80_intf.h"
#include "ay8910.h"
#include "burn_pal.h"
#include "burn_gun.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvMapROM;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvBgRAM;
static UINT8 *DrvTxRAM;
static UINT8 *DrvColRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 sl_image;
static UINT8 sl_enable;
static UINT8 sl_control;

static INT32 countdown60fps;
static INT32 countdown;

static UINT8 DrvJoy1[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[1];
static UINT8 DrvReset;

static INT32 DrvAnalogPort0 = 0;

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo DdayInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 1"	},

	A("P1 X Axis",      BIT_ANALOG_REL, &DrvAnalogPort0, "mouse x-axis"),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Dday)

static struct BurnInputInfo DdaycInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	A("P1 X Axis",      BIT_ANALOG_REL, &DrvAnalogPort0, "mouse x-axis"),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Ddayc)
#undef A

static struct BurnDIPInfo DdayDIPList[]=
{
	{0x05, 0xff, 0xff, 0x81, NULL					},
	{0x06, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    8, "Lives"				},
	{0x05, 0x01, 0x03, 0x00, "2"					},
	{0x05, 0x01, 0x03, 0x01, "3"					},
	{0x05, 0x01, 0x03, 0x02, "4"					},
	{0x05, 0x01, 0x03, 0x03, "5"					},
	{0x05, 0x01, 0x03, 0x00, "5"					},
	{0x05, 0x01, 0x03, 0x01, "6"					},
	{0x05, 0x01, 0x03, 0x02, "7"					},
	{0x05, 0x01, 0x03, 0x03, "8"					},

	{0   , 0xfe, 0   ,    4, "Extended Play At"		},
	{0x05, 0x01, 0x0c, 0x00, "10000"				},
	{0x05, 0x01, 0x0c, 0x04, "15000"				},
	{0x05, 0x01, 0x0c, 0x08, "20000"				},
	{0x05, 0x01, 0x0c, 0x0c, "25000"				},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x05, 0x01, 0x10, 0x00, "Off"					},
	{0x05, 0x01, 0x10, 0x10, "On"					},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x05, 0x01, 0x20, 0x00, "Off"					},
	{0x05, 0x01, 0x20, 0x20, "On"					},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x05, 0x01, 0x40, 0x00, "Off"					},
	{0x05, 0x01, 0x40, 0x40, "On"					},

	{0   , 0xfe, 0   ,    2, "Start with 20000 Pts"	},
	{0x05, 0x01, 0x80, 0x80, "No"					},
	{0x05, 0x01, 0x80, 0x00, "Yes"					},

	{0   , 0xfe, 0   ,    16, "Coin A"				},
	{0x06, 0x01, 0x0f, 0x0e, "2 Coins 1 Credits"	},
	{0x06, 0x01, 0x0f, 0x0c, "2 Coins 2 Credits"	},
	{0x06, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x06, 0x01, 0x0f, 0x0a, "2 Coins 3 Credits"	},
	{0x06, 0x01, 0x0f, 0x08, "2 Coins 4 Credits"	},
	{0x06, 0x01, 0x0f, 0x0d, "1 Coin  2 Credits"	},
	{0x06, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x06, 0x01, 0x0f, 0x04, "2 Coins 6 Credits"	},
	{0x06, 0x01, 0x0f, 0x0b, "1 Coin  3 Credits"	},
	{0x06, 0x01, 0x0f, 0x02, "2 Coins 7 Credits"	},
	{0x06, 0x01, 0x0f, 0x00, "2 Coins 8 Credits"	},
	{0x06, 0x01, 0x0f, 0x09, "1 Coin  4 Credits"	},
	{0x06, 0x01, 0x0f, 0x07, "1 Coin  5 Credits"	},
	{0x06, 0x01, 0x0f, 0x05, "1 Coin  6 Credits"	},
	{0x06, 0x01, 0x0f, 0x03, "1 Coin  7 Credits"	},
	{0x06, 0x01, 0x0f, 0x01, "1 Coin  8 Credits"	},

	{0   , 0xfe, 0   ,    16, "Coin B"				},
	{0x06, 0x01, 0xf0, 0xe0, "2 Coins 1 Credits"	},
	{0x06, 0x01, 0xf0, 0xc0, "2 Coins 2 Credits"	},
	{0x06, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x06, 0x01, 0xf0, 0xa0, "2 Coins 3 Credits"	},
	{0x06, 0x01, 0xf0, 0x80, "2 Coins 4 Credits"	},
	{0x06, 0x01, 0xf0, 0xd0, "1 Coin  2 Credits"	},
	{0x06, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x06, 0x01, 0xf0, 0x40, "2 Coins 6 Credits"	},
	{0x06, 0x01, 0xf0, 0xb0, "1 Coin  3 Credits"	},
	{0x06, 0x01, 0xf0, 0x20, "2 Coins 7 Credits"	},
	{0x06, 0x01, 0xf0, 0x00, "2 Coins 8 Credits"	},
	{0x06, 0x01, 0xf0, 0x90, "1 Coin  4 Credits"	},
	{0x06, 0x01, 0xf0, 0x70, "1 Coin  5 Credits"	},
	{0x06, 0x01, 0xf0, 0x50, "1 Coin  6 Credits"	},
	{0x06, 0x01, 0xf0, 0x30, "1 Coin  7 Credits"	},
	{0x06, 0x01, 0xf0, 0x10, "1 Coin  8 Credits"	},
};

STDDIPINFO(Dday)

static struct BurnDIPInfo DdaycDIPList[]=
{
	{0x06, 0xff, 0xff, 0xd1, NULL					},
	{0x07, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    8, "Lives"				},
	{0x06, 0x01, 0x03, 0x00, "2"					},
	{0x06, 0x01, 0x03, 0x01, "3"					},
	{0x06, 0x01, 0x03, 0x02, "4"					},
	{0x06, 0x01, 0x03, 0x03, "5"					},
	{0x06, 0x01, 0x03, 0x00, "5"					},
	{0x06, 0x01, 0x03, 0x01, "6"					},
	{0x06, 0x01, 0x03, 0x02, "7"					},
	{0x06, 0x01, 0x03, 0x03, "8"					},

	{0   , 0xfe, 0   ,    4, "Extended Play At"		},
	{0x06, 0x01, 0x0c, 0x00, "4000"					},
	{0x06, 0x01, 0x0c, 0x04, "6000"					},
	{0x06, 0x01, 0x0c, 0x08, "8000"					},
	{0x06, 0x01, 0x0c, 0x0c, "10000"				},

	{0   , 0xfe, 0   ,    3, "Difficulty"			},
	{0x06, 0x01, 0x30, 0x30, "Easy"					},
	{0x06, 0x01, 0x30, 0x20, "Normal"				},
	{0x06, 0x01, 0x30, 0x10, "Hard"					},

	{0   , 0xfe, 0   ,    2, "Start with 20000 Pts"	},
	{0x06, 0x01, 0x80, 0x80, "No"					},
	{0x06, 0x01, 0x80, 0x00, "Yes"					},

	{0   , 0xfe, 0   ,    16, "Coin A"				},
	{0x07, 0x01, 0x0f, 0x0e, "2 Coins 1 Credits"	},
	{0x07, 0x01, 0x0f, 0x0c, "2 Coins 2 Credits"	},
	{0x07, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x07, 0x01, 0x0f, 0x0a, "2 Coins 3 Credits"	},
	{0x07, 0x01, 0x0f, 0x08, "2 Coins 4 Credits"	},
	{0x07, 0x01, 0x0f, 0x0d, "1 Coin  2 Credits"	},
	{0x07, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x07, 0x01, 0x0f, 0x04, "2 Coins 6 Credits"	},
	{0x07, 0x01, 0x0f, 0x0b, "1 Coin  3 Credits"	},
	{0x07, 0x01, 0x0f, 0x02, "2 Coins 7 Credits"	},
	{0x07, 0x01, 0x0f, 0x00, "2 Coins 8 Credits"	},
	{0x07, 0x01, 0x0f, 0x09, "1 Coin  4 Credits"	},
	{0x07, 0x01, 0x0f, 0x07, "1 Coin  5 Credits"	},
	{0x07, 0x01, 0x0f, 0x05, "1 Coin  6 Credits"	},
	{0x07, 0x01, 0x0f, 0x03, "1 Coin  7 Credits"	},
	{0x07, 0x01, 0x0f, 0x01, "1 Coin  8 Credits"	},

	{0   , 0xfe, 0   ,    16, "Coin B"				},
	{0x07, 0x01, 0xf0, 0xe0, "2 Coins 1 Credits"	},
	{0x07, 0x01, 0xf0, 0xc0, "2 Coins 2 Credits"	},
	{0x07, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x07, 0x01, 0xf0, 0xa0, "2 Coins 3 Credits"	},
	{0x07, 0x01, 0xf0, 0x80, "2 Coins 4 Credits"	},
	{0x07, 0x01, 0xf0, 0xd0, "1 Coin  2 Credits"	},
	{0x07, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x07, 0x01, 0xf0, 0x40, "2 Coins 6 Credits"	},
	{0x07, 0x01, 0xf0, 0xb0, "1 Coin  3 Credits"	},
	{0x07, 0x01, 0xf0, 0x20, "2 Coins 7 Credits"	},
	{0x07, 0x01, 0xf0, 0x00, "2 Coins 8 Credits"	},
	{0x07, 0x01, 0xf0, 0x90, "1 Coin  4 Credits"	},
	{0x07, 0x01, 0xf0, 0x70, "1 Coin  5 Credits"	},
	{0x07, 0x01, 0xf0, 0x50, "1 Coin  6 Credits"	},
	{0x07, 0x01, 0xf0, 0x30, "1 Coin  7 Credits"	},
	{0x07, 0x01, 0xf0, 0x10, "1 Coin  8 Credits"	},
};

STDDIPINFO(Ddayc)

static void __fastcall dday_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfc00) == 0x5c00) {
		DrvColRAM[(address & 0x3e0) >> 5] = data;
		return;
	}

	if ((address & 0xfff0) == 0x6400) address &= ~0x000e; // ay0 mirror

	switch (address)
	{
		case 0x4000:
			sl_image = data;
		return;

		case 0x6400:
		case 0x6401:
			AY8910Write(0, address & 1, data);
		return;

		case 0x6800:
		case 0x6801:
			AY8910Write(1, address & 1, data);
		return;

		case 0x7800:
			// coin counter = data & 3;
			if ((data & 0x10) == 0x00 && (sl_control & 0x10) == 0x10) AY8910Reset(0);
			sl_enable = data & 0x40;
			sl_control = data;
		return;
	}
}

static UINT8 __fastcall dday_read(UINT16 address)
{
	if ((address & 0xfc00) == 0x5c00) {
		return DrvColRAM[(address & 0x3e0) >> 5];
	}

	switch (address)
	{
		case 0x6c00:
			return DrvInputs[0];

		case 0x7000:
			return DrvDips[0];

		case 0x7400:
			return DrvDips[1];

		case 0x7800:
			return ((countdown / 10) * 16) | (countdown % 10);

		case 0x7c00:
			return (BurnGunReturnX(0) * 0xbf) / 0x100;
	}

	return 0;
}

static tilemap_callback( bg )
{
	UINT8 code = DrvBgRAM[offs];

	TILE_SET_INFO(0, code, code >> 5, 0);
}

static tilemap_callback( fg )
{
	UINT8 flipx = DrvColRAM[offs / 0x20] & 1;
	UINT8 code  = DrvFgRAM[offs ^ (flipx ? 0x1f : 0)];

	TILE_SET_INFO(2, code, code >> 5, flipx ? TILE_FLIPX : 0);
}

static tilemap_callback( tx )
{
	UINT8 code = DrvTxRAM[offs];

	TILE_SET_INFO(1, code, code >> 5, 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	AY8910Reset(0);
	AY8910Reset(1);

	sl_image = 0;
	sl_enable = 0;
	sl_control = 0;

	countdown60fps = 0;
	countdown = 99;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x0040000;

	DrvGfxROM0		= Next; Next += 0x0040000;
	DrvGfxROM1		= Next; Next += 0x0040000;
	DrvGfxROM2		= Next; Next += 0x0040000;
	DrvGfxROM3		= Next; Next += 0x0010000;

	DrvMapROM		= Next; Next += 0x0010000;

	DrvColPROM		= Next; Next += 0x0003000;

	DrvPalette		= (UINT32*)Next; Next += 0x2000 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x0004000;
	DrvFgRAM		= Next; Next += 0x0004000;
	DrvBgRAM		= Next; Next += 0x0004000;
	DrvTxRAM		= Next; Next += 0x0004000;
	DrvColRAM		= Next; Next += 0x0000200;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[3] = { 0 * 0x800 * 8, 1 * 0x800 * 8, 2 * 0x800 * 8 };
	INT32 XOffs[8] = { STEP8(7, -1) };
	INT32 YOffs[8] = { STEP8(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x1800);
	if (tmp == NULL) {
		return 1;
	}
	memcpy (tmp, DrvGfxROM0, 0x1800);

	GfxDecode(0x0100, 3, 8, 8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x1000);

	GfxDecode(0x0100, 2, 8, 8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x1000);

	GfxDecode(0x0100, 2, 8, 8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM2);

	memcpy (tmp, DrvGfxROM3, 0x0800);

	GfxDecode(0x0040, 1, 8, 8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM3);
	
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
		if (BurnLoadRom(DrvZ80ROM  + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x1000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x2000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x3000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x0000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0800,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x1000,  6, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x0000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x0800,  8, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x0000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x0800, 10, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x0000, 11, 1)) return 1;

		if (BurnLoadRom(DrvMapROM  + 0x0000, 12, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x0000, 13, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0100, 14, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0200, 15, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvTxRAM,			0x5000, 0x53ff, MAP_RAM);
	ZetMapMemory(DrvFgRAM,			0x5400, 0x57ff, MAP_RAM);
	ZetMapMemory(DrvBgRAM,			0x5800, 0x5bff, MAP_RAM);
//	ZetMapMemory(DrvColRAM,			0x5c00, 0x5fff, MAP_RAM); // handler
	ZetMapMemory(DrvZ80RAM,			0x6000, 0x63ff, MAP_RAM);
	ZetSetWriteHandler(dday_write);
	ZetSetReadHandler(dday_read);
	ZetClose();

	AY8910Init(0, 1000000, 0);
	AY8910Init(1, 1000000, 1);
	AY8910SetAllRoutes(0, 0.20, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.20, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, fg_map_callback, 8, 8, 32, 32);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, tx_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 3, 8, 8, 0x4000, 0x00, 0x1f);
	GenericTilemapSetGfx(1, DrvGfxROM1, 2, 8, 8, 0x4000, 0x20, 0x07);
	GenericTilemapSetGfx(2, DrvGfxROM2, 2, 8, 8, 0x4000, 0x40, 0x07);
	GenericTilemapCategoryConfig(0, 2);
//	GenericTilemapSetTransMask(0, 0, 0x00f0 ^ ~0);
//	GenericTilemapSetTransMask(0, 1, 0xff0f ^ ~0);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetTransparent(2, 0);

	BurnGunInit(1, false);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	AY8910Exit(0);
	AY8910Exit(1);

	BurnGunExit();

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	static UINT8 lut[0x40] = {
		0x00, 0x01, 0x15, 0x02, 0x00, 0x01, 0x15, 0x02, 0x04, 0x05, 0x03, 0x07, 0x04, 0x05, 0x03, 0x07,
		0x08, 0x15, 0x0a, 0x03, 0x08, 0x15, 0x0a, 0x03, 0x08, 0x15, 0x0a, 0x03, 0x08, 0x15, 0x0a, 0x03,
		0x10, 0x11, 0x12, 0x07, 0x10, 0x11, 0x12, 0x07, 0x1d, 0x15, 0x16, 0x1b, 0x1d, 0x15, 0x16, 0x1b,
		0x1d, 0x15, 0x1a, 0x1b, 0x1d, 0x15, 0x1a, 0x1b, 0x1d, 0x02, 0x04, 0x1b, 0x1d, 0x02, 0x04, 0x1b
	};

	for (INT32 i = 0; i < 0x100; i++)
	{
		INT32 j = (i < 0x40) ? lut[i] : i;

		UINT8 r = pal4bit(DrvColPROM[j + 0x000]);
		UINT8 g = pal4bit(DrvColPROM[j + 0x100]);
		UINT8 b = pal4bit(DrvColPROM[j + 0x200]);

		DrvPalette[i + 0x000] = BurnHighCol(r, g, b, 0);
		DrvPalette[i + 0x100] = BurnHighCol(r/8, g/8, b/8, 0); // shadows
	}
}

static void sl_draw()
{
	if (sl_enable == 0) return;

	INT32 bank  = (sl_image & 0x07) * 0x200;
	INT32 sl_flipx = (sl_image & 0x08) ? 7 : 0;

	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sx = (offs & 0x1f) * 8;
		INT32 sy = (offs / 0x20) * 8;

		INT32 flipx = (offs & 0x10) ? 7 : 0;

		INT32 ofst = ((offs & 0x03e0) >> 1) | (offs & 0x0f);

		UINT8 code = DrvMapROM[bank + (ofst ^ (flipx ? 0xf : 0))];

		if ((sl_flipx != flipx) && (code & 0x80))
			code = 1;

		{
			UINT8 *gfx = DrvGfxROM3 + (code & 0x3f) * 0x40;
			UINT16 *dst = pTransDraw + (sy * nScreenWidth) + sx;

			for (INT32 y = 0; y < 8; y++)
			{
				if (sy+y >= nScreenHeight) return;
				for (INT32 x = 0; x < 8; x++)
				{
					if (sx+x >= nScreenWidth) return;
					if (gfx[(y * 8) + (x ^ flipx)])
					{
						dst[x] += 0x100;
					}
				}
				
				dst += nScreenWidth;
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

	BurnTransferClear();

	GenericTilemapSetTransMask(0, 0, 0xff0f);
	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, TMAP_DRAWLAYER0); // split
	if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 0);
	GenericTilemapSetTransMask(0, 0, 0x00f0);
	if (nBurnLayer & 4) GenericTilemapDraw(0, pTransDraw, TMAP_DRAWLAYER1); // split
	if (nBurnLayer & 8) GenericTilemapDraw(2, pTransDraw, 0);

	if (nSpriteEnable & 1) sl_draw();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		DrvInputs[0] = 0;
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
		}

		BurnGunMakeInputs(0, DrvAnalogPort0, 0);

		// countdown timer
		countdown60fps++;

		if (countdown60fps >= 60) {
			countdown60fps = 0;

			countdown--;

			if (countdown < 0)
				countdown = 99;
		}

	}

	ZetOpen(0);
	ZetRun(((2000000 * 240) / 256) / 60);
	ZetRun(((2000000 *  16) / 256) / 60); // vblank
	ZetClose();

	if (pBurnSoundOut) {
		if (sl_control & 0x10) {
			AY8910Render(pBurnSoundOut, nBurnSoundLen);
		} else {
			BurnSoundClear();
		}
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

		BurnGunScan();

		SCAN_VAR(sl_image);
		SCAN_VAR(sl_enable);
		SCAN_VAR(sl_control);
		SCAN_VAR(countdown60fps);
		SCAN_VAR(countdown);
	}

	return 0;
}


// D-Day

static struct BurnRomInfo ddayRomDesc[] = {
	{ "e8_63co.bin",	0x1000, 0x13d53793, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "e7_64co.bin",	0x1000, 0xe1ef2a70, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "e6_65co.bin",	0x1000, 0xfe414a83, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "e5_66co.bin",	0x1000, 0xfc9f7774, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "k4_73.bin",		0x0800, 0xfa6237e4, 2 | BRF_GRA },           //  4 Background Tiles
	{ "k2_71.bin",		0x0800, 0xf85461de, 2 | BRF_GRA },           //  5
	{ "k3_72.bin",		0x0800, 0xfdfe88b6, 2 | BRF_GRA },           //  6

	{ "j8_70co.bin",	0x0800, 0x0c60e94c, 3 | BRF_GRA },           //  7 Foreground Tiles
	{ "j9_69co.bin",	0x0800, 0xba341c10, 3 | BRF_GRA },           //  8

	{ "k6_74o.bin",		0x0800, 0x66719aea, 4 | BRF_GRA },           //  9 Characters
	{ "k7_75o.bin",		0x0800, 0x5f8772e2, 4 | BRF_GRA },           // 10

	{ "d4_68.bin",		0x0800, 0xf3649264, 5 | BRF_GRA },           // 11 Search Light Tiles

	{ "d2_67.bin",		0x1000, 0x2b693e42, 6 | BRF_GRA },           // 12 Search Light Tilemap

	{ "dday.m11",		0x0100, 0xaef6bbfc, 7 | BRF_GRA },           // 13 Color Data
	{ "dday.m8",		0x0100, 0xad3314b9, 7 | BRF_GRA },           // 14
	{ "dday.m3",		0x0100, 0xe877ab82, 7 | BRF_GRA },           // 15
};

STD_ROM_PICK(dday)
STD_ROM_FN(dday)

struct BurnDriver BurnDrvDday = {
	"dday", NULL, NULL, NULL, "1982",
	"D-Day\0", NULL, "Olympia", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, ddayRomInfo, ddayRomName, NULL, NULL, NULL, NULL, DdayInputInfo, DdayDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// D-Day (Centuri)

static struct BurnRomInfo ddaycRomDesc[] = {
	{ "e8_63-c.bin",	0x1000, 0xd4fa3ae3, 1 | BRF_PRG | BRF_ESS }, //  0 Background Tiles
	{ "e7_64-c.bin",	0x1000, 0x9fb8b1a7, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "e6_65-c.bin",	0x1000, 0x4c210686, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "e5_66-c.bin",	0x1000, 0xe7e832f9, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "k4_73.bin",		0x0800, 0xfa6237e4, 2 | BRF_GRA },           //  4 Background Tiles
	{ "k2_71.bin",		0x0800, 0xf85461de, 2 | BRF_GRA },           //  5
	{ "k3_72.bin",		0x0800, 0xfdfe88b6, 2 | BRF_GRA },           //  6

	{ "j8_70-c.bin",	0x0800, 0xa0c6b86b, 3 | BRF_GRA },           //  7 Foreground Tiles
	{ "j9_69-c.bin",	0x0800, 0xd352a3d6, 3 | BRF_GRA },           //  8

	{ "k6_74.bin",		0x0800, 0xd21a3e22, 4 | BRF_GRA },           //  9 Characters
	{ "k7_75.bin",		0x0800, 0xa5e5058c, 4 | BRF_GRA },           // 10

	{ "d4_68.bin",		0x0800, 0xf3649264, 5 | BRF_GRA },           // 11 Search Light Tiles

	{ "d2_67.bin",		0x1000, 0x2b693e42, 6 | BRF_GRA },           // 12 Search Light Tilemap

	{ "dday.m11",		0x0100, 0xaef6bbfc, 7 | BRF_GRA },           // 13 Color Data
	{ "dday.m8",		0x0100, 0xad3314b9, 7 | BRF_GRA },           // 14
	{ "dday.m3",		0x0100, 0xe877ab82, 7 | BRF_GRA },           // 15
};

STD_ROM_PICK(ddayc)
STD_ROM_FN(ddayc)

struct BurnDriver BurnDrvDdayc = {
	"ddayc", "dday", NULL, NULL, "1982",
	"D-Day (Centuri)\0", NULL, "Olympia (Centuri license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, ddaycRomInfo, ddaycRomName, NULL, NULL, NULL, NULL, DdaycInputInfo, DdaycDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};

