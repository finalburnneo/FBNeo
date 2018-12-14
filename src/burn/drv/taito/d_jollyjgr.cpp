// FB Alpha Jolly Jogger driver module
// Based on MAME driver by Pierpaolo Prazzoli

#include "tiles_generic.h"
#include "z80_intf.h"
#include "ay8910.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvAttRAM;
static UINT8 *DrvBmpRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 flipyx;
static UINT8 priority;
static UINT8 tilemap_bank;
static UINT8 bitmap_disable;
static UINT8 nmi_enable;

static INT16 *dcbuf; // for dc offset removal, fspiderb has a huge dc offset (ay8910)
static INT16 dc_lastin;
static INT16 dc_lastout;

static INT32 jollyjgrmode = 0;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static struct BurnInputInfo JollyjgrInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 4,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 5,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 right"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 0,	"service"	},
	{"Tilt",			BIT_DIGITAL,	DrvJoy2 + 7,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Jollyjgr)

static struct BurnInputInfo FspiderInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 start"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 start"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 4,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 0,	"service"	},
	{"Tilt",			BIT_DIGITAL,	DrvJoy2 + 7,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Fspider)

static struct BurnDIPInfo JollyjgrDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x92, NULL					},
	{0x10, 0xff, 0xff, 0x0f, NULL					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x0f, 0x01, 0x03, 0x03, "10000"				},
	{0x0f, 0x01, 0x03, 0x02, "20000"				},
	{0x0f, 0x01, 0x03, 0x01, "30000"				},
	{0x0f, 0x01, 0x03, 0x00, "40000"				},

	{0   , 0xfe, 0   ,    2, "Free Play"			},
	{0x0f, 0x01, 0x04, 0x00, "Off"					},
	{0x0f, 0x01, 0x04, 0x04, "On"					},

	{0   , 0xfe, 0   ,    4, "Timer"				},
	{0x0f, 0x01, 0x18, 0x18, "2 min 20 sec"			},
	{0x0f, 0x01, 0x18, 0x10, "2 min 40 sec"			},
	{0x0f, 0x01, 0x18, 0x08, "3 min"				},
	{0x0f, 0x01, 0x18, 0x00, "3 min 20 sec"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x0f, 0x01, 0x20, 0x00, "Off"					},
	{0x0f, 0x01, 0x20, 0x20, "On"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x0f, 0x01, 0x40, 0x00, "Off"					},
	{0x0f, 0x01, 0x40, 0x40, "On"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x0f, 0x01, 0x80, 0x80, "Upright"				},
	{0x0f, 0x01, 0x80, 0x00, "Cocktail"				},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x10, 0x01, 0x03, 0x00, "3 Coins 1 Credits"	},
	{0x10, 0x01, 0x03, 0x01, "2 Coins 1 Credits"	},
	{0x10, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x10, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x10, 0x01, 0x0c, 0x0c, "3"					},
	{0x10, 0x01, 0x0c, 0x08, "4"					},
	{0x10, 0x01, 0x0c, 0x04, "5"					},
	{0x10, 0x01, 0x0c, 0x00, "6"					},

	{0   , 0xfe, 0   ,    2, "Display Coinage"		},
	{0x10, 0x01, 0x10, 0x10, "No"					},
	{0x10, 0x01, 0x10, 0x00, "Yes"					},

	{0   , 0xfe, 0   ,    2, "Display Year"			},
	{0x10, 0x01, 0x20, 0x20, "No"					},
	{0x10, 0x01, 0x20, 0x00, "Yes"					},

	{0   , 0xfe, 0   ,    2, "No Hit"				},
	{0x10, 0x01, 0x40, 0x00, "Off"					},
	{0x10, 0x01, 0x40, 0x40, "On"					},

	{0   , 0xfe, 0   ,    2, "# of Coin Switches"	},
	{0x10, 0x01, 0x80, 0x80, "1"					},
	{0x10, 0x01, 0x80, 0x00, "2"					},
};

STDDIPINFO(Jollyjgr)

static struct BurnDIPInfo FspiderDIPList[]=
{
	{0x0f, 0xff, 0xff, 0xff, NULL					},
	{0x10, 0xff, 0xff, 0x30, NULL					},

	{0   , 0xfe, 0   ,    16, "Coin A"				},
	{0x0f, 0x01, 0x0f, 0x00, "9 Coins 1 Credits"	},
	{0x0f, 0x01, 0x0f, 0x01, "8 Coins 1 Credits"	},
	{0x0f, 0x01, 0x0f, 0x02, "7 Coins 1 Credits"	},
	{0x0f, 0x01, 0x0f, 0x03, "6 Coins 1 Credits"	},
	{0x0f, 0x01, 0x0f, 0x04, "5 Coins 1 Credits"	},
	{0x0f, 0x01, 0x0f, 0x05, "4 Coins 1 Credits"	},
	{0x0f, 0x01, 0x0f, 0x06, "3 Coins 1 Credits"	},
	{0x0f, 0x01, 0x0f, 0x07, "2 Coins 1 Credits"	},
	{0x0f, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x0f, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x0f, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x0f, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x0f, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x0f, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x0f, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x0f, 0x01, 0x0f, 0x08, "1 Coin  8 Credits"	},

	{0   , 0xfe, 0   ,    16, "Coin B"				},
	{0x0f, 0x01, 0xf0, 0x00, "9 Coins 1 Credits"	},
	{0x0f, 0x01, 0xf0, 0x10, "8 Coins 1 Credits"	},
	{0x0f, 0x01, 0xf0, 0x20, "7 Coins 1 Credits"	},
	{0x0f, 0x01, 0xf0, 0x30, "6 Coins 1 Credits"	},
	{0x0f, 0x01, 0xf0, 0x40, "5 Coins 1 Credits"	},
	{0x0f, 0x01, 0xf0, 0x50, "4 Coins 1 Credits"	},
	{0x0f, 0x01, 0xf0, 0x60, "3 Coins 1 Credits"	},
	{0x0f, 0x01, 0xf0, 0x70, "2 Coins 1 Credits"	},
	{0x0f, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x0f, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x0f, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x0f, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x0f, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x0f, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x0f, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x0f, 0x01, 0xf0, 0x80, "1 Coin  8 Credits"	},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x10, 0x01, 0x03, 0x00, "3"					},
	{0x10, 0x01, 0x03, 0x01, "4"					},
	{0x10, 0x01, 0x03, 0x02, "5"					},
	{0x10, 0x01, 0x03, 0x03, "6"					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x10, 0x01, 0x0c, 0x00, "10000"				},
	{0x10, 0x01, 0x0c, 0x04, "20000"				},
	{0x10, 0x01, 0x0c, 0x08, "30000"				},
	{0x10, 0x01, 0x0c, 0x0c, "40000"				},

	{0   , 0xfe, 0   ,    2, "Show Coinage Settings"},
	{0x10, 0x01, 0x10, 0x00, "No"					},
	{0x10, 0x01, 0x10, 0x10, "Yes"					},

	{0   , 0xfe, 0   ,    2, "Show only 1P Coinage"	},
	{0x10, 0x01, 0x20, 0x20, "No"					},
	{0x10, 0x01, 0x20, 0x00, "Yes"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x10, 0x01, 0x40, 0x40, "Upright"				},
	{0x10, 0x01, 0x40, 0x00, "Cocktail"				},
	
	{0   , 0xfe, 0   ,    2, "Free Play"			},
	{0x10, 0x01, 0x80, 0x00, "Off"					},
	{0x10, 0x01, 0x80, 0x80, "On"					},
};

STDDIPINFO(Fspider)

static void __fastcall jollyjgr_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x8ff8:
		case 0x8ffa:
			AY8910Write(0, (address / 2) & 1, data);
		return;

		case 0x8ffc:
			flipyx = 			data & 0x03;
			priority = 			data & 0x04;
			tilemap_bank = 		(data & 0x20) >> 5;
			bitmap_disable = 	data & 0x40;
			nmi_enable = 		data & 0x80;
			if (nmi_enable == 0) ZetSetIRQLine(0x20, CPU_IRQSTATUS_NONE);
		return;

		case 0x8ffd:
			// coin_lockout_w
		return;
	}
}

static UINT8 __fastcall jollyjgr_read(UINT16 address)
{
	switch (address)
	{
		case 0x8ff8:
			return DrvDips[0];

		case 0x8ff9:
			return DrvInputs[0];

		case 0x8ffa:
			return DrvInputs[1];

		case 0x8fff:
			return DrvDips[1];
	}

	return 0;
}

static tilemap_callback( bg )
{
	INT32 code  = DrvVidRAM[offs] + (tilemap_bank * 256);
	INT32 color = DrvAttRAM[(offs & 0x1f) * 2 + 1];

	TILE_SET_INFO(0, code, color, 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	AY8910Reset(0);

	flipyx = 0;
	priority = 0;
	tilemap_bank = 0;
	bitmap_disable = 0;
	nmi_enable = 0;

	dc_lastin = 0;
	dc_lastout = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x008000;

	DrvGfxROM0		= Next; Next += 0x008000;
	DrvGfxROM1		= Next; Next += 0x004000;

	DrvColPROM		= Next; Next += 0x001000;

	DrvPalette		= (UINT32*)Next; Next += 0x0028 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x000800;
	DrvVidRAM		= Next; Next += 0x000400;
	DrvAttRAM		= Next; Next += 0x000400;
	DrvBmpRAM		= Next; Next += 0x006000;

	RamEnd			= Next;

	dcbuf          = (INT16*)Next; Next += nBurnSoundLen * 2 * sizeof(INT16);

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[2] = { 0, 0x1000*8 };
	INT32 XOffs[16] = { STEP8(0,1), STEP8(64,1) };
	INT32 YOffs[16] = { STEP8(0,8), STEP8(128,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x2000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x2000);

	GfxDecode(0x0200, 2,  8,  8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x2000);

	GfxDecode(0x0040, 2, 16, 16, Plane, XOffs, YOffs, 0x100, tmp, DrvGfxROM1);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit(INT32 game)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (game == 0)
		{
			if (BurnLoadRom(DrvZ80ROM  + 0x0000,  0, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM  + 0x1000,  1, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM  + 0x2000,  2, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM  + 0x3000,  3, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM  + 0x4000,  4, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM  + 0x5000,  5, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM  + 0x6000,  6, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM  + 0x7000,  7, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM0 + 0x0000,  8, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x1000,  9, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM1 + 0x0000, 10, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x1000, 11, 1)) return 1;

			if (BurnLoadRom(DrvColPROM + 0x0000, 12, 1)) return 1;
		}
		else
		{
			if (BurnLoadRom(DrvZ80ROM  + 0x0000,  0, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM  + 0x7000,  1, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM  + 0x6000,  2, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM  + 0x5000,  3, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM  + 0x4000,  4, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM  + 0x2000,  5, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM  + 0x3000,  6, 1)) return 1;
			memcpy (DrvZ80ROM  + 0x1000, DrvZ80ROM  + 0x3000, 0x1000);

			if (BurnLoadRom(DrvGfxROM0 + 0x0000,  7, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x1000,  8, 1)) return 1;
			memcpy (DrvGfxROM0 + 0x0c00, DrvGfxROM0 + 0x0800, 0x0400);
			memcpy (DrvGfxROM0 + 0x1c00, DrvGfxROM0 + 0x1800, 0x0400);
			memset (DrvGfxROM0 + 0x0800, 0, 0x400);
			memset (DrvGfxROM0 + 0x1800, 0, 0x400);

			if (BurnLoadRom(DrvGfxROM1 + 0x0000,  9, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x1000, 10, 1)) return 1;

			if (BurnLoadRom(DrvColPROM + 0x0000, 11, 1)) return 1;
		}

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,		0x8000, 0x87ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,		0x9000, 0x93ff, MAP_RAM);
	ZetMapMemory(DrvAttRAM,		0x9800, 0x9bff, MAP_RAM); // col 0-3f, spr 40-7f, 80+ unk
	ZetMapMemory(DrvBmpRAM,		0xa000, 0xffff, MAP_RAM);
	ZetSetWriteHandler(jollyjgr_write);
	ZetSetReadHandler(jollyjgr_read);
	ZetClose();

	AY8910Init(0, 1789772, 0);
	AY8910SetAllRoutes(0, (jollyjgrmode) ? 0.20 : 0.30, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 2, 8, 8, 0x08000, 0, 0);
	GenericTilemapSetOffsets(0, 0, -16);
	GenericTilemapSetScrollCols(0, 32);
	GenericTilemapSetTransparent(0,0);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	ZetExit();
	AY8910Exit(0);

	BurnFree(AllMem);

	jollyjgrmode = 0;

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 32; i++)
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

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}

	for (INT32 i = 0; i < 8; i++) {
		DrvPalette[0x20 + i] = BurnHighCol((i&1)?0xff:0, (i&2)?0xff:0, (i&4)?0xff:0, 0);
	}
}

static void draw_bitmap()
{
	INT32 count = 16 * (256 / 8);

	for (INT32 y = 16; y < 240; y++)
	{
		UINT16 *dst = pTransDraw + ((255 - y) - 16) * nScreenWidth;
		if (flipyx & 2) dst = pTransDraw + (y - 16) * nScreenWidth;

		for (INT32 x = 0; x < 256 / 8; x++)
		{
			for (INT32 i = 0; i < 8; i++)
			{
				INT32 bit0 = (DrvBmpRAM[count] >> i) & 1;
				INT32 bit1 = (DrvBmpRAM[count + 0x2000] >> i) & 1;
				INT32 bit2 = (DrvBmpRAM[count + 0x4000] >> i) & 1;
				INT32 color = bit0 | (bit1 << 1) | (bit2 << 2);

				if (color)
				{
					switch (flipyx & 1)
					{
						case 1: dst[x * 8 + i] = 0x20 + color;
						case 0: dst[255 - (x * 8 + i)] = 0x20 + color;
					}
				}
			}

			count++;
		}
	}
}

static void draw_sprites()
{
	UINT8 *spriteram = DrvAttRAM + 0x40;

	for (INT32 offs = 0; offs < 0x40; offs += 4)
	{
		INT32 sx = spriteram[offs + 3] + 1;
		INT32 sy = spriteram[offs];
		INT32 flipx = spriteram[offs + 1] & 0x40;
		INT32 flipy = spriteram[offs + 1] & 0x80;
		INT32 code = spriteram[offs + 1] & 0x3f;
		INT32 color = spriteram[offs + 2] & 7;

		if (flipyx & 1)
		{
			sx = 240 - sx;
			flipx = !flipx;
		}

		if (flipyx & 2)
			flipy = !flipy;
		else
			sy = 240 - sy;

		if (offs < 3 * 4)
			sy++;

		Draw16x16MaskTile(pTransDraw, code, sx, sy - 16, flipx, flipy, color, 2, 0, 0, DrvGfxROM1);
	}
}

static void common_draw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	GenericTilemapSetFlip(0, ((flipyx & 2) ? TMAP_FLIPY : 0) | ((flipyx & 1) ? TMAP_FLIPX : 0));
	
	for (INT32 i = 0; i < 32; i++) {
		GenericTilemapSetScrollCol(0, i, DrvAttRAM[(i * 2) + 0]);
	}

	BurnTransferClear(); // 0 fill

	if (priority != 0 && bitmap_disable == 0 && (nBurnLayer & 1)) draw_bitmap();

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);

	if (priority == 0 && bitmap_disable == 0 && (nBurnLayer & 4)) draw_bitmap();

	if (nSpriteEnable & 1) draw_sprites();
}

static INT32 JollyjgrDraw()
{
	common_draw();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void draw_bullets()
{
	UINT8 *bulletram = DrvAttRAM + 0xa0;

	for (INT32 offs = 0; offs < 0x10; offs+=2)
	{
		UINT8 sy = ~bulletram[offs];
		UINT8 sx = ~bulletram[offs|1];
		UINT8 color = (offs < 4) ? 7: 3;

		if (flipyx & 2) sy^=0xff;
		if (flipyx & 1) sx+=8;

		if (sy >= 16 && sy < 240)
			for (INT32 x = sx-4; x < sx; x++)
				if (x >= 0 && x < 256)
					pTransDraw[(sy - 16) * nScreenWidth + x] = color;
	}
}

static INT32 FspiderbDraw()
{
	common_draw();

	draw_bullets();

	BurnTransferCopy(DrvPalette);
	
	return 0;
}

static void dcfilter()
{
	for (INT32 i = 0; i < nBurnSoundLen; i++) {
		INT16 r = dcbuf[i*2+0]; // stream is mono, ignore 'l'.
		//INT16 l = dcbuf[i*2+1];

		INT16 out = r - dc_lastin + 0.995 * dc_lastout;

		dc_lastin = r;
		dc_lastout = out;
		pBurnSoundOut[i*2+0] = out;
		pBurnSoundOut[i*2+1] = out;
	}
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		DrvInputs[0] = 0;
		DrvInputs[1] = 0;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}

		if (jollyjgrmode) {
			ProcessJoystick(&DrvInputs[0], 0, 0,1,3,2, INPUT_4WAY);
			ProcessJoystick(&DrvInputs[0], 1, 4,5,7,6, INPUT_4WAY);
		}
	}

	ZetOpen(0);
	ZetRun(3000000/60);
	if (nmi_enable) ZetSetIRQLine(0x20, CPU_IRQSTATUS_ACK);
	ZetClose();

	if (pBurnSoundOut) {
		AY8910Render(dcbuf, nBurnSoundLen);
		dcfilter();
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
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

		SCAN_VAR(flipyx);
		SCAN_VAR(priority);
		SCAN_VAR(tilemap_bank);
		SCAN_VAR(bitmap_disable);
		SCAN_VAR(nmi_enable);

		SCAN_VAR(dc_lastin);
		SCAN_VAR(dc_lastout);
	}

	return 0;
}


// Jolly Jogger

static struct BurnRomInfo jollyjgrRomDesc[] = {
	{ "kd14.8a",	0x1000, 0x404cfa2b, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "kd15.8b",	0x1000, 0x4cdc4c8b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "kd16.8c",	0x1000, 0xa2fa3500, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "kd17.8d",	0x1000, 0xa5ab8c89, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "kd18.8e",	0x1000, 0x8547c9a7, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "kd19.8f",	0x1000, 0x2d0ed544, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "kd20.8h",	0x1000, 0x017a0e5a, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "kd21.8j",	0x1000, 0xe4faed01, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "kd09.1c",	0x0800, 0xecafd535, 2 | BRF_GRA },           //  8 Background Tiles
	{ "kd10.2c",	0x0800, 0xe40fc594, 2 | BRF_GRA },           //  9

	{ "kd11.5h",	0x0800, 0xd686245c, 3 | BRF_GRA },           // 10 Sprites
	{ "kd12.7h",	0x0800, 0xd69cbb4e, 3 | BRF_GRA },           // 11

	{ "kd13.1f",	0x1000, 0x4f4e4e13, 4 | BRF_GRA },           // 12 Color data
};

STD_ROM_PICK(jollyjgr)
STD_ROM_FN(jollyjgr)

static INT32 JollyjgrInit()
{
	jollyjgrmode = 1;

	return DrvInit(0);
}

struct BurnDriver BurnDrvJollyjgr = {
	"jollyjgr", NULL, NULL, NULL, "1982",
	"Jolly Jogger\0", NULL, "Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_PREFIX_TAITO, GBF_MAZE, 0,
	NULL, jollyjgrRomInfo, jollyjgrRomName, NULL, NULL, NULL, NULL, JollyjgrInputInfo, JollyjgrDIPInfo,
	JollyjgrInit, DrvExit, DrvFrame, JollyjgrDraw, DrvScan, &DrvRecalc, 0x28,
	224, 256, 3, 4
};


// Frog & Spiders (bootleg?)

static struct BurnRomInfo fspiderbRomDesc[] = {
	{ "1.5l",		0x1000, 0x3679cab1, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "2.8b",		0x1000, 0x6e543acf, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.8c",		0x1000, 0xf74f83d9, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.8e",		0x1000, 0x26add629, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "5.8f",		0x1000, 0x0457de7b, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "6.8h",		0x1000, 0x329d4716, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "7.8j",		0x1000, 0xa7d8fc3c, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "8.1c",		0x1000, 0x4e39abad, 2 | BRF_GRA },           //  7 Background Tiles
	{ "9.2c",		0x1000, 0x04dd1604, 2 | BRF_GRA },           //  8

	{ "10.5h",		0x1000, 0xd4bce323, 3 | BRF_GRA },           //  9 Sprites
	{ "11.7h",		0x1000, 0x7ab56309, 3 | BRF_GRA },           // 10

	{ "82s123.1f",	0x0020, 0xcda6001a, 4 | BRF_GRA },           // 11 Color data
};

STD_ROM_PICK(fspiderb)
STD_ROM_FN(fspiderb)

static INT32 FspiderbInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvFspiderb = {
	"fspiderb", NULL, NULL, NULL, "1981",
	"Frog & Spiders (bootleg?)\0", NULL, "Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_PREFIX_TAITO, GBF_VERSHOOT, 0,
	NULL, fspiderbRomInfo, fspiderbRomName, NULL, NULL, NULL, NULL, FspiderInputInfo, FspiderDIPInfo,
	FspiderbInit, DrvExit, DrvFrame, FspiderbDraw, DrvScan, &DrvRecalc, 0x28,
	224, 256, 3, 4
};
