// FB Alpha NMK Argus driver module
// Based on MAME driver by Yochizo

#include "tiles_generic.h"
#include "burn_bitmap.h"
#include "z80_intf.h"
#include "burn_ym2203.h"
#include "burn_pal.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvGfxROM4;
static UINT8 *DrvGfxROM5;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvPalRAM;
static UINT8 *DrvTxtRAM;
static UINT8 *DrvBgRAM0;
static UINT8 *DrvBgRAM1;
static UINT8 *DrvSprRAM;
static UINT8 *DrvBlendTable;
static UINT32 *DrvTransBuffer;

static UINT32 *DrvPalette;
static UINT32 *DrvPalette32;
static UINT8 DrvRecalc;

static INT32 nExtraCycles;

static UINT16 palette_intensity;
static UINT8 bg_status;
static UINT8 bg1_status;
static UINT8 flipscreen;
static UINT8 soundlatch;
static UINT16 scrollx0;
static UINT16 scrolly0;
static UINT16 scrollx1;
static UINT16 scrolly1;
static UINT8 mosaic_data;
static INT32 auto_mosaic;
static UINT8 bankdata;
static INT32 rambank = -1;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static INT32 is_argus = 0;

static struct BurnInputInfo ArgusInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 6,	"p2 coin"	},
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

STDINPUTINFO(Argus)

static struct BurnInputInfo ValtricInputList[] = {
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
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 5,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Valtric)

static struct BurnInputInfo ButasanInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 6,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 5,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Butasan)

static struct BurnDIPInfo ArgusDIPList[]=
{
	{0x11, 0xff, 0xff, 0xcf, NULL						},
	{0x12, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    2, "Flip Screen"				},
	{0x11, 0x01, 0x01, 0x01, "Off"						},
	{0x11, 0x01, 0x01, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x11, 0x01, 0x06, 0x04, "Easy"						},
	{0x11, 0x01, 0x06, 0x06, "Normal"					},
	{0x11, 0x01, 0x06, 0x02, "Medium Difficult"			},
	{0x11, 0x01, 0x06, 0x00, "Difficult"				},

	{0   , 0xfe, 0   ,    2, "Free Play"				},
	{0x11, 0x01, 0x08, 0x08, "Off"						},
	{0x11, 0x01, 0x08, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x11, 0x01, 0x10, 0x00, "Upright"					},
	{0x11, 0x01, 0x10, 0x10, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x11, 0x01, 0x20, 0x20, "Off"						},
	{0x11, 0x01, 0x20, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x11, 0x01, 0xc0, 0x80, "2"						},
	{0x11, 0x01, 0xc0, 0xc0, "3"						},
	{0x11, 0x01, 0xc0, 0x40, "4"						},
	{0x11, 0x01, 0xc0, 0x00, "5"						},

	{0   , 0xfe, 0   ,    8, "Coin B"					},
	{0x12, 0x01, 0x1c, 0x00, "5 Coins 1 Credits"		},
	{0x12, 0x01, 0x1c, 0x04, "4 Coins 1 Credits"		},
	{0x12, 0x01, 0x1c, 0x08, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0x1c, 0x0c, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0x1c, 0x1c, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0x1c, 0x18, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0x1c, 0x14, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0x1c, 0x10, "1 Coin  4 Credits"		},

	{0   , 0xfe, 0   ,    8, "Coin A"					},
	{0x12, 0x01, 0xe0, 0x00, "5 Coins 1 Credits"		},
	{0x12, 0x01, 0xe0, 0x20, "4 Coins 1 Credits"		},
	{0x12, 0x01, 0xe0, 0x40, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0xe0, 0x60, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0xe0, 0xc0, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0xe0, 0x80, "1 Coin  4 Credits"		},
};

STDDIPINFO(Argus)

static struct BurnDIPInfo ValtricDIPList[]=
{
	{0x12, 0xff, 0xff, 0xcf, NULL						},
	{0x13, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    2, "Flip Screen"				},
	{0x12, 0x01, 0x01, 0x01, "Off"						},
	{0x12, 0x01, 0x01, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x12, 0x01, 0x06, 0x06, "Normal"					},
	{0x12, 0x01, 0x06, 0x04, "Medium Difficult"			},
	{0x12, 0x01, 0x06, 0x02, "Difficult"				},
	{0x12, 0x01, 0x06, 0x00, "Very Difficult"			},

	{0   , 0xfe, 0   ,    2, "Free Play"				},
	{0x12, 0x01, 0x08, 0x08, "Off"						},
	{0x12, 0x01, 0x08, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x12, 0x01, 0x10, 0x00, "Upright"					},
	{0x12, 0x01, 0x10, 0x10, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x12, 0x01, 0x20, 0x20, "Off"						},
	{0x12, 0x01, 0x20, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x12, 0x01, 0xc0, 0x00, "2"						},
	{0x12, 0x01, 0xc0, 0xc0, "3"						},
	{0x12, 0x01, 0xc0, 0x80, "4"						},
	{0x12, 0x01, 0xc0, 0x40, "5"						},

	{0   , 0xfe, 0   ,    2, "Invulnerability"			},
	{0x13, 0x01, 0x01, 0x01, "Off"						},
	{0x13, 0x01, 0x01, 0x00, "On"						},

	{0   , 0xfe, 0   ,    8, "Coin B"					},
	{0x13, 0x01, 0x1c, 0x00, "5 Coins 1 Credits"		},
	{0x13, 0x01, 0x1c, 0x04, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x1c, 0x08, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x1c, 0x0c, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x1c, 0x1c, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x1c, 0x18, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x1c, 0x14, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0x1c, 0x10, "1 Coin  4 Credits"		},

	{0   , 0xfe, 0   ,    8, "Coin A"					},
	{0x13, 0x01, 0xe0, 0x00, "5 Coins 1 Credits"		},
	{0x13, 0x01, 0xe0, 0x20, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0xe0, 0x40, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0xe0, 0x60, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0xe0, 0xc0, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0xe0, 0x80, "1 Coin  4 Credits"		},
};

STDDIPINFO(Valtric)

static struct BurnDIPInfo ButasanDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL						},
	{0x13, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    2, "Free Play"				},
	{0x12, 0x01, 0x01, 0x01, "Off"						},
	{0x12, 0x01, 0x01, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Invulnerability (Cheat)"	},
	{0x12, 0x01, 0x02, 0x02, "Off"						},
	{0x12, 0x01, 0x02, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x12, 0x01, 0x0c, 0x0c, "3"						},
	{0x12, 0x01, 0x0c, 0x08, "4"						},
	{0x12, 0x01, 0x0c, 0x04, "5"						},
	{0x12, 0x01, 0x0c, 0x00, "6"						},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x12, 0x01, 0x30, 0x30, "Normal"					},
	{0x12, 0x01, 0x30, 0x20, "Medium Difficult"			},
	{0x12, 0x01, 0x30, 0x10, "Difficult"				},
	{0x12, 0x01, 0x30, 0x00, "Very Difficult"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"				},
	{0x13, 0x01, 0x01, 0x01, "Off"						},
	{0x13, 0x01, 0x01, 0x00, "On"						},

	{0   , 0xfe, 0   ,    8, "Coin B"					},
	{0x13, 0x01, 0x1c, 0x00, "5 Coins 1 Credits"		},
	{0x13, 0x01, 0x1c, 0x04, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x1c, 0x08, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x1c, 0x0c, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x1c, 0x1c, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x1c, 0x18, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x1c, 0x14, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0x1c, 0x10, "1 Coin  4 Credits"		},

	{0   , 0xfe, 0   ,    8, "Coin A"					},
	{0x13, 0x01, 0xe0, 0x00, "5 Coins 1 Credits"		},
	{0x13, 0x01, 0xe0, 0x20, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0xe0, 0x40, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0xe0, 0x60, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0xe0, 0xc0, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0xe0, 0x80, "1 Coin  4 Credits"		},
};

STDDIPINFO(Butasan)

static void bankswitch(UINT8 data)
{
	INT32 bank = (data & 0x07) * 0x4000;

	bankdata = data;

	ZetMapMemory(DrvZ80ROM0 + 0x10000 + bank, 0x8000, 0xbfff, MAP_ROM);
}

static UINT32 blend_color(UINT32 dest, UINT32 addme, UINT8 alpha)
{
	INT32 r = dest / 0x10000;
	INT32 g = (dest / 0x100) & 0xff;
	INT32 b = dest & 0xff;

	INT32 ir = addme / 0x10000;
	INT32 ig = (addme / 0x100) & 0xff;
	INT32 ib = addme & 0xff;

	if (alpha & 4)
		{ r -= ir; if (r < 0) r = 0; }
	else
		{ r += ir; if (r > 255) r = 255; }

	if (alpha & 2)
		{ g -= ig; if (g < 0) g = 0; }
	else
		{ g += ig; if (g > 255) g = 255; }

	if (alpha & 1)
		{ b -= ib; if (b < 0) b = 0; }
	else
		{ b += ib; if (b > 255) b = 255; }

	return r * 0x10000 + g * 0x100 + b;
}

static void change_palette(INT32 color, INT32 lo_offs, INT32 hi_offs)
{
	UINT8 lo = DrvPalRAM[lo_offs];
	UINT8 hi = DrvPalRAM[hi_offs];
	
	DrvBlendTable[color] = hi & 0xf;

	UINT8 r = pal4bit(lo >> 4);
	UINT8 g = pal4bit(lo);
	UINT8 b = pal4bit(hi >> 4);

	DrvPalette[color] = BurnHighCol(r,g,b,0);
	DrvPalette32[color] = r * 0x10000 + g * 0x100 + b;
}

static void change_bg_palette(INT32 color, INT32 lo_offs, INT32 hi_offs)
{
	UINT8 ir = pal4bit(palette_intensity >> 12);
	UINT8 ig = pal4bit(palette_intensity >>  8);
	UINT8 ib = pal4bit(palette_intensity >>  4);
	UINT8 ix = palette_intensity & 0x0f;

	UINT32 irgb = ir * 0x10000 + ig * 0x100 + ib;

	UINT8 lo = DrvPalRAM[lo_offs];
	UINT8 hi = DrvPalRAM[hi_offs];

	UINT8 r = pal4bit(lo >> 4);
	UINT8 g = pal4bit(lo);
	UINT8 b = pal4bit(hi >> 4);

	UINT32 rgb = 0;

	if (bg_status & 2)
	{
		UINT8 val = (r + g + b) / 3;
		rgb = val * 0x10000 + val * 0x100 + val;
	}
	else
	{
		rgb = r * 0x10000 + g * 0x100 + b;
	}

	rgb = blend_color(rgb,irgb,ix);

	r = rgb / 0x10000;
	g = rgb / 0x100;
	b = rgb;

	DrvPalette[color] = BurnHighCol(r,g,b,0);
	DrvPalette32[color] = rgb;
}

static void argus_palette_write(UINT16 offset, UINT8 data)
{
	DrvPalRAM[offset] = data;

	if (offset <= 0x0ff)
	{
		offset &= 0x07f;

		change_palette(offset, offset, offset + 0x080);

		if (offset == 0x07f || offset == 0x0ff)
		{
			palette_intensity = DrvPalRAM[0x0ff] | (DrvPalRAM[0x07f] << 8);

			for (INT32 offs = 0; offs < 0x100; offs++)
				change_bg_palette(offs + 0x100, offs + 0x400, offs + 0x800);
		}
	}
	else if ((offset >= 0x400 && offset <= 0x4ff) || (offset >= 0x800 && offset <= 0x8ff))
	{
		INT32 offs = offset & 0xff;
		offset = offs | 0x400;

		change_bg_palette(offs + 0x100, offset, offset + 0x400);
	}
	else if ((offset >= 0x500 && offset <= 0x5ff) || (offset >= 0x900 && offset <= 0x9ff))
	{
		INT32 offs = offset & 0xff;
		offset = offs | 0x500;

		change_palette(offs + 0x200, offset, offset + 0x400);
	}
	else if ((offset >= 0x700 && offset <= 0x7ff) || (offset >= 0xb00 && offset <= 0xbff))
	{
		INT32 offs = offset & 0xff;
		offset = offs | 0x700;

		change_palette(offs + 0x300, offset, offset + 0x400);
	}
}

static void argus_bg_status_write(UINT8 data)
{
	if (bg_status != data)
	{
		bg_status = data;

		if (bg_status & 2)
		{
			for (INT32 offs = 0; offs < 0x100; offs++)
			{
				change_bg_palette(offs + 0x100, offs + 0x400, offs + 0x800);
			}
		}
	}
}

static void __fastcall argus_main_write(UINT16 address, UINT8 data)
{
	if (address >= 0xc400 && address <= 0xcfff) {
		argus_palette_write(address - 0xc400, data);
		return;
	}

	switch (address)
	{
		case 0xc200:
			soundlatch = data;
		return;

		case 0xc201:
			flipscreen = data & 0x80;
		return;

		case 0xc202:
			bankswitch(data);
		return;

		case 0xc300:
		case 0xc301:
			scrollx0 = (scrollx0 & (0xff00 >> ((address & 1) * 8))) | (data << ((address & 1) * 8));
		return;

		case 0xc302:
		case 0xc303:
			scrolly0 = (scrolly0 & (0xff00 >> ((address & 1) * 8))) | (data << ((address & 1) * 8));
		return;

		case 0xc308:
		case 0xc309:
			scrollx1 = (scrollx1 & (0xff00 >> ((address & 1) * 8))) | (data << ((address & 1) * 8));
		return;

		case 0xc30a:
		case 0xc30b:
			scrolly1 = (scrolly1 & (0xff00 >> ((address & 1) * 8))) | (data << ((address & 1) * 8));
		return;

		case 0xc30c:
			argus_bg_status_write(data);
		return;
	}
}

static UINT8 __fastcall argus_main_read(UINT16 address)
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
	}

	return 0;
}

static void valtric_palette_write(UINT16 offset, UINT8 data)
{
	DrvPalRAM[offset] = data;

	if (offset <= 0x1ff)
	{
		change_palette(offset >> 1, offset & ~1, offset | 1);

		if (offset == 0x1fe || offset == 0x1ff)
		{
			palette_intensity = DrvPalRAM[0x1ff] | (DrvPalRAM[0x1fe] << 8);

			for (INT32 offs = 0x400; offs < 0x600; offs += 2)
				change_bg_palette(((offs & 0x1ff) >> 1) + 0x100, offs & ~1, offs | 1);
		}
	}
	else if (offset >= 0x400 && offset <= 0x5ff)
	{
		change_bg_palette(((offset & 0x1ff) >> 1) + 0x100, offset & ~1, offset | 1);
	}
	else if (offset >= 0x600 && offset <= 0x7ff)
	{
		change_palette(((offset & 0x1ff) >> 1) + 0x200, offset & ~1, offset | 1);
	}
}

static void valtric_bg_status_write(UINT8 data)
{
	if (bg_status != data)
	{
		bg_status = data;

		if (bg_status & 2)
		{
			for (INT32 offs = 0x400; offs < 0x600; offs += 2)
			{
				change_bg_palette(((offs - 0x400) >> 1) + 0x100, offs & ~1, offs | 1);
			}
		}
	}
}

static void __fastcall valtric_main_write(UINT16 address, UINT8 data)
{
	if (address >= 0xc400 && address <= 0xcfff) {
		valtric_palette_write(address - 0xc400, data);
		return;
	}

	switch (address)
	{
		case 0xc30c:
			valtric_bg_status_write(data);
		return;

		case 0xc30d:
			mosaic_data = data;
		return;
	}

	argus_main_write(address,data);
}

static void butasan_palette_write(UINT16 offset, UINT8 data)
{
	DrvPalRAM[offset] = data;

	if (offset <= 0x1ff)                            /* BG0 color */
	{
		change_palette((offset >> 1) + 0x100, offset & ~1, offset | 1);
	}
	else if (offset <= 0x23f)                       /* BG1 color */
	{
		change_palette(((offset & 0x3f) >> 1) + 0x0c0, offset & ~1, offset | 1);
	}
	else if (offset >= 0x400 && offset <= 0x47f)    /* Sprite color */
	{                                               /* 16 colors */
		change_palette((offset & 0x7f) >> 1, offset & ~1, offset | 1);
	}
	else if (offset >= 0x480 && offset <= 0x4ff)    /* Sprite color */
	{                                               /* 8  colors */
		INT32 offs = (offset & 0x070) | ((offset & 0x00f) >> 1);

		change_palette(offs + 0x040, offset & ~1, offset | 1);
		change_palette(offs + 0x048, offset & ~1, offset | 1);
	}
	else if (offset >= 0x600 && offset <= 0x7ff)    /* Text color */
	{
		change_palette(((offset & 0x1ff) >> 1) + 0x200, offset & ~1, offset | 1);
	}
	else if (offset >= 0x240 && offset <= 0x25f)    // dummy
		change_palette(((offset & 0x1f) >> 1) + 0xe0, offset & ~1, offset | 1);
	else if (offset >= 0x500 && offset <= 0x51f)    // dummy
		change_palette(((offset & 0x1f) >> 1) + 0xf0, offset & ~1, offset | 1);
}

static void rambankswitch(INT32 data)
{
	rambank = data & 1;

	if (rambank == 0) {
		ZetMapMemory(DrvBgRAM0,			0xd000, 0xd7ff, MAP_RAM);
		ZetMapMemory(DrvBgRAM0 + 0x800,	0xd800, 0xdfff, MAP_RAM);
	} else {
		ZetMapMemory(DrvTxtRAM,			0xd000, 0xd7ff, MAP_RAM);
		ZetMapMemory(DrvTxtRAM + 0x800,	0xd800, 0xdfff, MAP_WRITE);
		ZetMapMemory(DrvBgRAM0 + 0x800, 0xd800, 0xdfff, MAP_ROM);
	}
}

static void __fastcall butasan_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf800) == 0xc800) {
		butasan_palette_write(address & 0x7ff, data);
		return;
	}

	switch (address)
	{
		case 0xc100:
		return; // unknown

		case 0xc200:
			soundlatch = data;
		return;

		case 0xc201:
			flipscreen = data & 0x80;
		return;

		case 0xc202:
			bankswitch(data);
		return;

		case 0xc203:
			rambankswitch(data);
		return;

		case 0xc300:
		case 0xc301:
			scrollx0 = (scrollx0 & (0xff00 >> ((address & 1) * 8))) | (data << ((address & 1) * 8));
		return;

		case 0xc302:
		case 0xc303:
			scrolly0 = (scrolly0 & (0xff00 >> ((address & 1) * 8))) | (data << ((address & 1) * 8));
		return;

		case 0xc308:
		case 0xc309:
			scrollx1 = (scrollx1 & (0xff00 >> ((address & 1) * 8))) | (data << ((address & 1) * 8));
		return;

		case 0xc30a:
		case 0xc30b:
			scrolly1 = (scrolly1 & (0xff00 >> ((address & 1) * 8))) | (data << ((address & 1) * 8));
		return;

		case 0xc304:
			bg_status = data;
		return;

		case 0xc30c:
			bg1_status = data;
		return;
	}
}

static UINT8 __fastcall argus_sound_read(UINT16 /*address*/)
{
	return soundlatch;
}

static void __fastcall argus_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
			BurnYM2203Write(0, port & 1, data);
		return;

		case 0x80:
		case 0x81:
			BurnYM2203Write(1, port & 1, data);
		return;
	}
}

static UINT8 __fastcall argus_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
			return BurnYM2203Read(0, port & 1);

		case 0x80:
		case 0x81:
			return BurnYM2203Read(1, port & 1);
	}

	return 0;
}

static tilemap_callback( bg0 )
{
	offs *= 2;
	INT32 vrom_offset = (offs >> 3);
	offs = (DrvGfxROM4[vrom_offset & ~1] << 4) | ((DrvGfxROM4[vrom_offset | 1] & 0x7) << 12) | (offs & 0xf);

	INT32 attr = DrvGfxROM5[offs + 1];
	INT32 code = DrvGfxROM5[offs + 0] | ((attr & 0xc0) << 2);

	TILE_SET_INFO(1, code, attr, TILE_FLIPYX(attr >> 4));
}

static tilemap_callback( bg1 )
{
	INT32 attr = DrvBgRAM0[offs * 2 + 1];
	INT32 code = DrvBgRAM0[offs * 2 + 0] | ((attr & 0xc0) << 2);

	TILE_SET_INFO(2, code, attr, TILE_FLIPYX(attr >> 4));
}

static tilemap_callback( vbg )
{
	INT32 attr = DrvBgRAM0[offs * 2 + 1];
	INT32 code = DrvBgRAM0[offs * 2 + 0] | ((attr & 0xc0) << 2) | ((attr & 0x20) << 5);

	TILE_SET_INFO(1, code, attr, 0);
}

static tilemap_callback( txt )
{
	INT32 attr = DrvTxtRAM[offs * 2 + 1];
	INT32 code = DrvTxtRAM[offs * 2 + 0] | ((attr & 0xc0) << 2);

	TILE_SET_INFO(0, code, attr, TILE_FLIPYX(attr >> 4));
}

static tilemap_callback( bbg0 )
{
	INT32 attr = DrvBgRAM0[offs * 2 + 1];
	INT32 code = DrvBgRAM0[offs * 2 + 0] | ((attr & 0xc0) << 2);

	TILE_SET_INFO(1, code, attr, TILE_FLIPYX(attr >> 4));
}

static tilemap_callback( bbg1 )
{
	INT32 code = DrvBgRAM1[offs] + ((bg1_status & 2) << 7);

	TILE_SET_INFO(2, code, code >> 7, 0);
}

static tilemap_scan( bg )
{
	return (col & 0x0f) | ((row ^ 0x0f) << 4) | ((col & 0x10) << 5);
}

static tilemap_scan( tx )
{
	return (col & 0x1f) | ((row ^ 0x1f) << 5);
}

inline static void DrvYM2203IRQHandler(INT32, INT32 nStatus)
{
	ZetSetIRQLine(0, nStatus ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	bankswitch(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	BurnYM2203Reset();
	ZetClose();

	palette_intensity = 0;
	bg_status = 1;
	bg1_status = 0;
	flipscreen = 0;
	soundlatch = 0;
	scrollx0 = 0;
	scrollx1 = 0;
	scrolly0 = 0;
	scrolly1 = 0;
	mosaic_data = 0;
	auto_mosaic = 0;

	nExtraCycles = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x030000;
	DrvZ80ROM1		= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += 0x100000;
	DrvGfxROM1		= Next; Next += 0x080000;
	DrvGfxROM2		= Next; Next += 0x020000;
	DrvGfxROM3		= Next; Next += 0x020000;
	DrvGfxROM4		= Next; Next += 0x008000;
	DrvGfxROM5		= Next; Next += 0x008000;

	DrvPalette		= (UINT32*)Next; Next += 0x0400 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM0		= Next; Next += 0x002000;
	DrvZ80RAM1		= Next; Next += 0x000800;
	DrvPalRAM		= Next; Next += 0x000c00;
	DrvTxtRAM		= Next; Next += 0x001000;
	DrvBgRAM0		= Next; Next += 0x001000;
	DrvBgRAM1		= Next; Next += 0x000800;
	DrvSprRAM		= Next; Next += 0x000700;

	DrvBlendTable	= Next; Next += 0x000400;

	DrvPalette32	= (UINT32*)Next; Next += 0x0400 * sizeof(UINT32);

	DrvTransBuffer  = (UINT32*)Next; Next += 512 * 512 * sizeof(UINT32);

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[4]  = { STEP4(0,1) };
	INT32 XOffs[16] = { STEP8(0,4), STEP8(4*8*16, 4) };
	INT32 YOffs[16] = { STEP16(0,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x80000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x80000);

	GfxDecode(0x1000, 4, 16, 16, Plane, XOffs, YOffs, 0x400, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x40000);

	GfxDecode(0x0800, 4, 16, 16, Plane, XOffs, YOffs, 0x400, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x20000);

	GfxDecode(0x0200, 4, 16, 16, Plane, XOffs, YOffs, 0x400, tmp, DrvGfxROM2);

	memcpy (tmp, DrvGfxROM3, 0x08000);

	GfxDecode(0x0400, 4,  8,  8, Plane, XOffs, YOffs, 0x100, tmp, DrvGfxROM3);

	BurnFree(tmp);

	return 0;
}

static INT32 ArgusInit()
{
	BurnSetRefreshRate(54.00);

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x10000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x18000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x20000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x08000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x10000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x18000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x08000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x10000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x18000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x00000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM4 + 0x00000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM5 + 0x00000,  k++, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvPalRAM,			0xc400, 0xcfff, MAP_ROM); // handler
	ZetMapMemory(DrvTxtRAM,			0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvBgRAM0,			0xd800, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM0,		0xe000, 0xffff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0xf200, 0xf7ff, MAP_RAM);
	ZetSetWriteHandler(argus_main_write);
	ZetSetReadHandler(argus_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,		0x8000, 0x87ff, MAP_RAM);
	ZetSetReadHandler(argus_sound_read);
	ZetSetOutHandler(argus_sound_write_port);
	ZetSetInHandler(argus_sound_read_port);
	ZetClose();

	BurnYM2203Init(2,  1500000, &DrvYM2203IRQHandler, 0);
	BurnTimerAttachZet(5000000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE,   0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.15, BURN_SND_ROUTE_BOTH);
	// not used on this set
	BurnYM2203SetRoute(1, BURN_SND_YM2203_YM2203_ROUTE,   0.00, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_1, 0.00, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_2, 0.00, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_3, 0.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_COLS, txt_map_callback,  8,  8, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_COLS, bg0_map_callback, 16, 16, 4096, 32);
	GenericTilemapInit(2, TILEMAP_SCAN_COLS, bg1_map_callback, 16, 16, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM3, 4,  8,  8, 0x10000, 0x300, 0xf);
	GenericTilemapSetGfx(1, DrvGfxROM1, 4, 16, 16, 0x40000, 0x100, 0xf);
	GenericTilemapSetGfx(2, DrvGfxROM2, 4, 16, 16, 0x10000, 0x200, 0xf);
	GenericTilemapSetTransparent(2, 0xf);
	GenericTilemapSetTransparent(0, 0xf);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -((256 - nScreenHeight) / 2));

	is_argus = 1;

	DrvDoReset();

	return 0;
}

static INT32 ValtricInit()
{
	BurnSetRefreshRate(54.00);

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x10000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x20000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x10000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x10000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x20000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x30000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x00000,  k++, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvPalRAM,			0xc400, 0xcfff, MAP_ROM); // handler
	ZetMapMemory(DrvTxtRAM,			0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvBgRAM0,			0xd800, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM0,		0xe000, 0xffff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0xf200, 0xf7ff, MAP_RAM);
	ZetSetWriteHandler(valtric_main_write);
	ZetSetReadHandler(argus_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,		0x8000, 0x87ff, MAP_RAM);
	ZetSetReadHandler(argus_sound_read);
	ZetSetOutHandler(argus_sound_write_port);
	ZetSetInHandler(argus_sound_read_port);
	ZetClose();

	BurnYM2203Init(2,  1500000, &DrvYM2203IRQHandler, 0);
	BurnTimerAttachZet(5000000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE,   0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_YM2203_ROUTE,   0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_1, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_2, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_3, 0.15, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_COLS, txt_map_callback,  8,  8, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_COLS, vbg_map_callback, 16, 16, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM3, 4,  8,  8, 0x10000, 0x200, 0xf);
	GenericTilemapSetGfx(1, DrvGfxROM1, 4, 16, 16, 0x80000, 0x100, 0xf);
	GenericTilemapSetTransparent(0, 0xf);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -((256 - nScreenHeight) / 2));

	DrvDoReset();

	return 0;
}

static INT32 ButasanInit()
{
	BurnSetRefreshRate(54.00);

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x10000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x20000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x10000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x20000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x30000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x40000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x50000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x60000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x70000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x10000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x00000,  k++, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvBgRAM1,			0xc400, 0xc7ff, MAP_RAM);
	ZetMapMemory(DrvPalRAM,			0xc800, 0xcfff, MAP_ROM); // handler
	ZetMapMemory(DrvZ80RAM0,		0xe000, 0xffff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0xf000, 0xf6ff, MAP_RAM);
	ZetSetWriteHandler(butasan_main_write);
	ZetSetReadHandler(argus_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,		0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,		0xc000, 0xc7ff, MAP_RAM);
	ZetSetReadHandler(argus_sound_read);
	ZetSetOutHandler(argus_sound_write_port);
	ZetSetInHandler(argus_sound_read_port);
	ZetClose();

	BurnYM2203Init(2,  1500000, &DrvYM2203IRQHandler, 0);
	BurnTimerAttachZet(5000000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE,   0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_YM2203_ROUTE,   0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_1, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_2, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_3, 0.15, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, tx_map_scan, txt_map_callback,   8,  8, 32, 32);
	GenericTilemapInit(1, bg_map_scan, bbg0_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(2, bg_map_scan, bbg1_map_callback, 16, 16, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM3, 4,  8,  8, 0x10000, 0x200, 0xf);
	GenericTilemapSetGfx(1, DrvGfxROM1, 4, 16, 16, 0x40000, 0x100, 0xf);
	GenericTilemapSetGfx(2, DrvGfxROM2, 4, 16, 16, 0x20000, 0x0c0, 0x1);
	GenericTilemapSetTransparent(0, 0xf);
	GenericTilemapSetTransparent(2, 0xf);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -((256 - nScreenHeight) / 2));

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	BurnYM2203Exit();

	BurnFree(AllMem);

	rambank = -1;

	is_argus = 0;

	return 0;
}

static void DrvPaletteRecalc()
{
	for (INT32 i = 0; i < 0x400; i++)
	{
		UINT8 r = (DrvPalette32[i] / 0x10000) & 0xff;
		UINT8 g = (DrvPalette32[i] / 0x100) & 0xff;
		UINT8 b = DrvPalette32[i] & 0xff;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_sprite_blend(INT32 bgmask, INT32 code, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy, INT32 color, INT32 depth, INT32 mask, INT32 color_offset, UINT8 *gfx)
{
	gfx += (code * 16 * 16);

	color = (color << depth) + color_offset;

	INT32 flip = (flipx ? 0x0f : 0) | (flipy ? 0xf0 : 0);

	for (INT32 y = 0; y < 16; y++, sy++)
	{
		if (sy < 0 || sy >= nScreenHeight) continue;

		for (INT32 x = 0; x < 16; x++)
		{
			if ((sx + x) < 0 || (sx + x) >= nScreenWidth) continue;

			INT32 c = gfx[((y * 16) + x) ^ flip];

			if (c != mask)
			{
				c += color;
				INT32 dst = (sy * nScreenWidth) + sx + x;

				if (pTransDraw[dst] >= bgmask) continue; // Don't draw over text layer

				// Don't allow the title "mask" to get blitted over the background
				if (is_argus && ((code >= 0x3f0 && code <= 0x3f8) || (code >= 0x3b6 && code <= 0x3bf) ||
					  (code >= 0x3ac && code <= 0x3af)) && (~pTransDraw[dst] & 0x200))
					continue;

				UINT8 a = DrvBlendTable[c];

				if (a & 8)
				{
					UINT32 d = blend_color(DrvTransBuffer[dst], DrvPalette32[c], a);
					DrvTransBuffer[dst] = d;
					PutPix(pBurnDraw + (dst * nBurnBpp), BurnHighCol((d/0x10000)&0xff, (d/0x100)&0xff, d&0xff, 0));
				}
				else
				{
					DrvTransBuffer[dst] = DrvPalette32[c];
					PutPix(pBurnDraw + (dst * nBurnBpp), DrvPalette[c]);
				}
			}
		}
	}
}

static void argus_draw_sprites(INT32 prio_mask, INT32 priority, INT32 color_mask)
{
	for (INT32 offs = 0; offs < 0x600; offs += 16)
	{
		if (!(DrvSprRAM[offs + 15] == 0 && DrvSprRAM[offs + 11] == 0xf0))
		{
			INT32 sy    = DrvSprRAM[offs + 11];
			INT32 sx    = DrvSprRAM[offs + 12];
			INT32 attr  = DrvSprRAM[offs + 13];
			INT32 code  = DrvSprRAM[offs + 14] | ((attr & 0xc0) << 2);
			INT32 color = DrvSprRAM[offs + 15] & color_mask;
			INT32 pri   = DrvSprRAM[offs + 15] & prio_mask;
			INT32 flipx = attr & 0x10;
			INT32 flipy = attr & 0x20;
			if ( attr & 0x01) sx -= 256;
			if (~attr & 0x02) sy -= 256;

			if ((priority != pri) && prio_mask) continue;

			if (priority == 8) pri = 0x200;
			if (priority == 0) pri = 0x300;

			if (flipscreen)
			{
				sx = 240 - sx;
				sy = 240 - sy;
				flipx = !flipx;
				flipy = !flipy;
			}

			draw_sprite_blend(pri, code, sx, sy - 16, flipx, flipy, color, 4, 0xf, 0, DrvGfxROM0);
		}
	}
}

static INT32 ArgusDraw()
{
	if (DrvRecalc) {
		DrvPaletteRecalc();
		DrvRecalc = 0;
	}

	GenericTilemapSetFlip(TMAP_GLOBAL, flipscreen ? (TMAP_FLIPX|TMAP_FLIPY) : 0);
	GenericTilemapSetScrollX(1, scrollx0);
	GenericTilemapSetScrollY(1, scrolly0);
	GenericTilemapSetScrollX(2, scrollx1);
	GenericTilemapSetScrollY(2, scrolly1);

	if (nBurnLayer & 1) {
		UINT32 p = DrvPalette32[0x100];
		DrvPalette[0x100] = BurnHighCol(p/0x10000, (p/0x100)&0xff, p&0xff, 0);
		GenericTilemapDraw(1, pTransDraw, 0);
	} else {
		DrvPalette[0x100] = 0;
		BurnTransferClear(0x100);
	}

	if ((nBurnLayer & 2) && (bg_status & 1)) {
		GenericTilemapDraw(2, pTransDraw, 0);
	}

	if (nBurnLayer & 4) {
		GenericTilemapDraw(0, pTransDraw, 0);
	}

	BurnTransferCopy(DrvPalette);

	// copy screen to 32bit alpha-trans buffer. yay! :)
	for (INT32 sy = 0; sy < nScreenHeight; sy++)
		for (INT32 sx = 0; sx < nScreenWidth; sx++) {
			DrvTransBuffer[(sy * nScreenWidth) + sx] = DrvPalette32[pTransDraw[(sy * nScreenWidth) + sx]];
		}

	if (nSpriteEnable & 2) argus_draw_sprites(8, 8, 0x07);
	if (nSpriteEnable & 1) argus_draw_sprites(8, 0, 0x07);

	return 0;
}

static void draw_mosaic()
{
	if (mosaic_data != 0x80)
	{
		auto_mosaic = 0x0f - (mosaic_data & 0x0f);
		if (auto_mosaic) auto_mosaic++;
		if (mosaic_data & 0x80) auto_mosaic *= -1;
	}

	GenericTilemapSetScrollX(1, scrollx1);
	GenericTilemapSetScrollY(1, scrolly1);

	if (auto_mosaic == 0)
	{
		GenericTilemapDraw(1, pTransDraw, 0);
	}
	else
	{
		UINT16 *pTempDraw = (UINT16*)DrvTransBuffer;
		GenericTilemapDraw(1, pTempDraw, 0);

		{
			INT32 step = auto_mosaic;

			if (auto_mosaic < 0) step*=-1;

			for (INT32 y=0;y<nScreenWidth+step;y+=step)
			{
				for (INT32 x=0;x<nScreenHeight+32+step;x+=step)
				{
					INT32 c = 0;
					if (y < nScreenHeight && x < nScreenWidth)
						c = pTempDraw[(y * nScreenWidth) + x];

					if (auto_mosaic<0)
						if (y+step-1 < nScreenHeight && x+step-1 < nScreenWidth)
							c = pTempDraw[((y+step-1) * nScreenWidth) + (x+step-1)];

					for (INT32 yy=0;yy<step;yy++)
					{
						for (INT32 xx=0;xx<step;xx++)
						{
							if ((xx+x) >= 0 && (xx+x) < nScreenWidth && (yy+y) >= 0 && (yy+y) < nScreenHeight)
							{
								pTransDraw[((y+yy) * nScreenWidth) + (x+xx)] = c;
							}
						}
					}
				}
			}
		}
	}
}

static INT32 ValtricDraw()
{
	if (DrvRecalc) {
		DrvPaletteRecalc();
		DrvRecalc = 0;
	}

	GenericTilemapSetFlip(TMAP_GLOBAL, flipscreen ? (TMAP_FLIPX|TMAP_FLIPY) : 0);

	if ((nBurnLayer & 1) && (bg_status & 1)) {
		DrvPalette[0x100] = 0;
		draw_mosaic();
	} else {
		UINT32 p = DrvPalette32[0x100];
		DrvPalette[0x100] = BurnHighCol(p/0x10000, (p/0x100)&0xff, p&0xff, 0);
		BurnTransferClear(0x100);
	}

	if (nBurnLayer & 4) {
		GenericTilemapDraw(0, pTransDraw, 0);
	}

	BurnTransferCopy(DrvPalette);

	// copy screen to 32bit alpha-trans buffer. yay! :)
	for (INT32 sy = 0; sy < nScreenHeight; sy++)
		for (INT32 sx = 0; sx < nScreenWidth; sx++) {
			DrvTransBuffer[(sy * nScreenWidth) + sx] = DrvPalette32[pTransDraw[(sy * nScreenWidth) + sx]];
		}

	if (nSpriteEnable & 1) argus_draw_sprites(0,8,0xf);

	return 0;
}

static void butasan_draw_sprites()
{
	for (INT32 offs = 0; offs < 0x680; offs += 16)
	{
		INT32 flipx = DrvSprRAM[offs +  8] & 0x01;
		INT32 flipy = DrvSprRAM[offs +  8] & 0x04;
		INT32 color = DrvSprRAM[offs +  9] & 0x0f;
		INT32 sx    = DrvSprRAM[offs + 10];
		INT32 sy    = DrvSprRAM[offs + 12];
		INT32 code  = DrvSprRAM[offs + 14] | ((DrvSprRAM[offs + 15] & 0x0f) << 8);

		if (DrvSprRAM[offs + 11] & 0x01) sx -= 256;
		if (DrvSprRAM[offs + 13] & 0x01) sy -= 256;

		sy = 240 - sy;

		if (flipscreen)
		{
			sx = 240 - sx;
			sy = 240 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		if ((offs >= 0x100 && offs <= 0x2ff) || (offs >= 0x400 && offs <= 0x57f))
		{
			draw_sprite_blend(0x200, code, sx, sy, flipx, flipy, color, 4, 7, 0, DrvGfxROM0);
		}
		else if ((offs <= 0xff) || (offs >= 0x300 && offs <= 0x3ff))
		{
			for (INT32 i = 0; i <= 1; i++)
			{
				INT32 td = (flipx) ? (1 - i) : i;

				draw_sprite_blend(0x200, code + td, sx + i * 16, sy, flipx, flipy, color, 4, 7, 0, DrvGfxROM0);
			}
		}
		else if (offs >= 0x580 && offs <= 0x61f)
		{
			for (INT32 i = 0; i <= 1; i++)
			{
				for (INT32 j = 0; j <= 1; j++)
				{
					INT32 td;
					if (flipy)
						td = (flipx) ? ((1 - i) * 2) + 1 - j : (1 - i) * 2 + j;
					else
						td = (flipx) ? (i * 2) + 1 - j : i * 2 + j;

					draw_sprite_blend(0x200, code + td, sx + j * 16, sy - i * 16, flipx, flipy, color, 4, 7, 0, DrvGfxROM0);
				}
			}
		}
		else if (offs >= 0x620)
		{
			for (INT32 i = 0; i <= 3; i++)
			{
				for (INT32 j = 0; j <= 3; j++)
				{
					INT32 td;
					if (flipy)
						td = (flipx) ? ((3 - i) * 4) + 3 - j : (3 - i) * 4 + j;
					else
						td = (flipx) ? (i * 4) + 3 - j : i * 4 + j;

					draw_sprite_blend(0x200, code + td, sx + j * 16, sy - i * 16, flipx, flipy, color, 4, 7, 0, DrvGfxROM0);
				}
			}
		}
	}
}

static INT32 ButasanDraw()
{
	if (DrvRecalc) {
		DrvPaletteRecalc();
		DrvRecalc = 0;
	}

	GenericTilemapSetFlip(TMAP_GLOBAL, flipscreen ? (TMAP_FLIPX|TMAP_FLIPY) : 0);
	GenericTilemapSetScrollX(1, scrollx0);
	GenericTilemapSetScrollY(1, scrolly0);
	GenericTilemapSetScrollX(2, scrollx1);
	GenericTilemapSetScrollY(2, scrolly1);

	if ((nBurnLayer & 1) && (bg_status & 1)) {
		UINT32 p = DrvPalette32[0x100];
		DrvPalette[0x100] = BurnHighCol(p/0x10000, (p/0x100)&0xff, p&0xff, 0);
		GenericTilemapDraw(1, pTransDraw, 0);
	} else {
		DrvPalette[0x100] = 0;
		BurnTransferClear();
	}

	if ((nBurnLayer & 1) && (bg1_status & 1)) {
		GenericTilemapDraw(1, pTransDraw, 0);
	}

	if (nBurnLayer & 4) {
		GenericTilemapDraw(0, pTransDraw, 0);
	}

	BurnTransferCopy(DrvPalette);

	// copy screen to 32bit alpha-trans buffer. yay! :)
	for (INT32 sy = 0; sy < nScreenHeight; sy++)
		for (INT32 sx = 0; sx < nScreenWidth; sx++) {
			DrvTransBuffer[(sy * nScreenWidth) + sx] = DrvPalette32[pTransDraw[(sy * nScreenWidth) + sx]];
		}

	if (nSpriteEnable & 1) butasan_draw_sprites();

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();

	{
		DrvInputs[0] = 0xff;
		DrvInputs[1] = 0xff;
		DrvInputs[2] = 0xff;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 5000000 / 54, 5000000 / 54 };
	INT32 nCyclesDone[2] = { nExtraCycles, 0 };

	INT32 irq1_line = ((256 - nScreenHeight) / 2);
	INT32 irq2_line = (256 - ((256 - nScreenHeight) / 2));

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(((i + 1) * nCyclesTotal[0] / nInterleave) - nCyclesDone[0]);
		if (i == irq1_line) {
			ZetSetVector(0xcf);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
		if (i == irq2_line) {
			ZetSetVector(0xd7);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);

			if (pBurnDraw) {
				BurnDrvRedraw();
			}
		}
		ZetClose();

		ZetOpen(1);
		BurnTimerUpdate((nCyclesTotal[0] / nInterleave) * (i + 1));
		ZetClose();
	}

	ZetOpen(1);
	BurnTimerEndFrame(nCyclesTotal[1]);
	
	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();

	nExtraCycles = nCyclesDone[0] - nCyclesTotal[0];

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
		BurnYM2203Scan(nAction, pnMin);

		SCAN_VAR(palette_intensity);
		SCAN_VAR(bg_status);
		SCAN_VAR(bg1_status);
		SCAN_VAR(flipscreen);
		SCAN_VAR(soundlatch);
		SCAN_VAR(scrollx0);
		SCAN_VAR(scrollx1);
		SCAN_VAR(scrolly0);
		SCAN_VAR(scrolly1);
		SCAN_VAR(mosaic_data);
		SCAN_VAR(auto_mosaic);
		SCAN_VAR(bankdata);
		SCAN_VAR(rambank);

		SCAN_VAR(nExtraCycles);
	}

	if (nAction & ACB_WRITE)
	{
		ZetOpen(0);
		bankswitch(bankdata);
		if (rambank >= 0) rambankswitch(rambank); // butasan
		ZetClose();
	}

	return 0;
}


// Argus

static struct BurnRomInfo argusRomDesc[] = {
	{ "ag_02.bin",		0x8000, 0x278a3f3d, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "ag_03.bin",		0x8000, 0x3a7f3bfa, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ag_04.bin",		0x8000, 0x76adc9f6, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ag_05.bin",		0x8000, 0xf76692d6, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "ag_01.bin",		0x4000, 0x769e3f57, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code

	{ "ag_09.bin",		0x8000, 0x6dbc1c58, 3 | BRF_GRA },           //  5 Sprite Tiles
	{ "ag_08.bin",		0x8000, 0xce6e987e, 3 | BRF_GRA },           //  6
	{ "ag_07.bin",		0x8000, 0xbbb9638d, 3 | BRF_GRA },           //  7
	{ "ag_06.bin",		0x8000, 0x655b48f8, 3 | BRF_GRA },           //  8

	{ "ag_13.bin",		0x8000, 0x20274268, 4 | BRF_GRA },           //  9 Background Tiles
	{ "ag_14.bin",		0x8000, 0xceb8860b, 4 | BRF_GRA },           // 10
	{ "ag_11.bin",		0x8000, 0x99ce8556, 4 | BRF_GRA },           // 11
	{ "ag_12.bin",		0x8000, 0xe0e5377c, 4 | BRF_GRA },           // 12

	{ "ag_17.bin",		0x8000, 0x0f12d09b, 5 | BRF_GRA },           // 13 Midground Tiles

	{ "ag_10.bin",		0x4000, 0x2de696c4, 6 | BRF_GRA },           // 14 Character Tiles

	{ "ag_15.bin",		0x8000, 0x99834c1b, 7 | BRF_GRA },           // 15 Background Tilemap Look-up Table

	{ "ag_16.bin",		0x8000, 0x39a51714, 8 | BRF_GRA },           // 16 Background Tilemap
};

STD_ROM_PICK(argus)
STD_ROM_FN(argus)

struct BurnDriver BurnDrvArgus = {
	"argus", NULL, NULL, NULL, "1986",
	"Argus\0", NULL, "NMK (Jaleco license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, argusRomInfo, argusRomName, NULL, NULL, NULL, NULL, ArgusInputInfo, ArgusDIPInfo,
	ArgusInit, DrvExit, DrvFrame, ArgusDraw, DrvScan, &DrvRecalc, 0x380,
	224, 256, 3, 4
};


// Valtric

static struct BurnRomInfo valtricRomDesc[] = {
	{ "vt_04.bin",		0x08000, 0x709c705f, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "vt_06.bin",		0x10000, 0xc9cbb4e4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "vt_05.bin",		0x10000, 0x7ab2684b, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "vt_01.bin",		0x08000, 0x4616484f, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "vt_02.bin",		0x10000, 0x66401977, 3 | BRF_GRA },           //  4 Sprite Tiles
	{ "vt_03.bin",		0x10000, 0x9203bbce, 3 | BRF_GRA },           //  5

	{ "vt_08.bin",		0x10000, 0x661dd338, 4 | BRF_GRA },           //  6 Background Tiles
	{ "vt_09.bin",		0x10000, 0x085a35b1, 4 | BRF_GRA },           //  7
	{ "vt_10.bin",		0x10000, 0x09c47323, 4 | BRF_GRA },           //  8
	{ "vt_11.bin",		0x10000, 0x4cf800b5, 4 | BRF_GRA },           //  9

	{ "vt_07.bin",		0x04000, 0x4b9771ae, 5 | BRF_GRA },           // 10 Character Tiles

	{ "82s129.1s",		0x00100, 0xbc42f9b5, 0 | BRF_OPT },           // 11 Unused PROMs
};

STD_ROM_PICK(valtric)
STD_ROM_FN(valtric)

struct BurnDriver BurnDrvValtric = {
	"valtric", NULL, NULL, NULL, "1986",
	"Valtric\0", NULL, "NMK (Jaleco license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, valtricRomInfo, valtricRomName, NULL, NULL, NULL, NULL, ValtricInputInfo, ValtricDIPInfo,
	ValtricInit, DrvExit, DrvFrame, ValtricDraw, DrvScan, &DrvRecalc, 0x300,
	224, 256, 3, 4
};


// Butasan - Pig's & Bomber's (Japan, English)

static struct BurnRomInfo butasanRomDesc[] = {
	{ "4.t2",			0x08000, 0x1ba1d8e4, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "3.s2",			0x10000, 0xa6b3ccc2, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "2.p2",			0x10000, 0x96517fa9, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "1.b2",			0x10000, 0xc9d23e2d, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "16.k16",			0x10000, 0xe0ce51b6, 3 | BRF_GRA },           //  4 Sprite Tiles
	{ "15.k15",			0x10000, 0x2105f6e1, 3 | BRF_GRA },           //  5
	{ "14.k14",			0x10000, 0x8ec891c1, 3 | BRF_GRA },           //  6
	{ "13.k13",			0x10000, 0x5023e74d, 3 | BRF_GRA },           //  7
	{ "12.k12",			0x10000, 0x44f59905, 3 | BRF_GRA },           //  8
	{ "11.k11",			0x10000, 0xb8929f1d, 3 | BRF_GRA },           //  9
	{ "10.k10",			0x10000, 0xfd4d3baf, 3 | BRF_GRA },           // 10
	{ "9.k9",			0x10000, 0x7da4c0fd, 3 | BRF_GRA },           // 11

	{ "5.l7",			0x10000, 0xb8e026b0, 4 | BRF_GRA },           // 12 Background Tiles
	{ "6.n7",			0x10000, 0x8bbacb81, 4 | BRF_GRA },           // 13

	{ "7.a8",			0x10000, 0x3a48d531, 5 | BRF_GRA },           // 14 Midground Tiles

	{ "8.a7",			0x08000, 0x85153252, 6 | BRF_GRA },           // 15 Character Tiles

	{ "buta-01.prm",	0x00100, 0x45baedd0, 0 | BRF_OPT },           // 16 Unused PROMs
	{ "buta-02.prm",	0x00100, 0x0dcb18fc, 0 | BRF_OPT },           // 17
};

STD_ROM_PICK(butasan)
STD_ROM_FN(butasan)

struct BurnDriver BurnDrvButasan = {
	"butasan", NULL, NULL, NULL, "1987",
	"Butasan - Pig's & Bomber's (Japan, English)\0", NULL, "NMK (Jaleco license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, butasanRomInfo, butasanRomName, NULL, NULL, NULL, NULL, ButasanInputInfo, ButasanDIPInfo,
	ButasanInit, DrvExit, DrvFrame, ButasanDraw, DrvScan, &DrvRecalc, 0x300,
	256, 240, 4, 3
};


// Butasan (Japan, Japanese)

static struct BurnRomInfo butasanjRomDesc[] = {
	{ "buta-04.bin",	0x08000, 0x47ff4ca9, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "buta-03.bin",	0x10000, 0x69fd88c7, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "buta-02.bin",	0x10000, 0x519dc412, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "1.b2",			0x10000, 0xc9d23e2d, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "16.k16",			0x10000, 0xe0ce51b6, 3 | BRF_GRA },           //  4 Sprite Tiles
	{ "buta-15.bin",	0x10000, 0x3ed19daa, 3 | BRF_GRA },           //  5
	{ "14.k14",			0x10000, 0x8ec891c1, 3 | BRF_GRA },           //  6
	{ "13.k13",			0x10000, 0x5023e74d, 3 | BRF_GRA },           //  7
	{ "12.k12",			0x10000, 0x44f59905, 3 | BRF_GRA },           //  8
	{ "11.k11",			0x10000, 0xb8929f1d, 3 | BRF_GRA },           //  9
	{ "10.k10",			0x10000, 0xfd4d3baf, 3 | BRF_GRA },           // 10
	{ "9.k9",			0x10000, 0x7da4c0fd, 3 | BRF_GRA },           // 11

	{ "5.l7",			0x10000, 0xb8e026b0, 4 | BRF_GRA },           // 12 Background Tiles
	{ "6.n7",			0x10000, 0x8bbacb81, 4 | BRF_GRA },           // 13

	{ "7.a8",			0x10000, 0x3a48d531, 5 | BRF_GRA },           // 14 Midground Tiles

	{ "buta-08.bin",	0x08000, 0x5d45ce9c, 6 | BRF_GRA },           // 15 Character Tiles

	{ "buta-01.prm",	0x00100, 0x45baedd0, 0 | BRF_OPT },           // 16 Unused PROMs
	{ "buta-02.prm",	0x00100, 0x0dcb18fc, 0 | BRF_OPT },           // 17
};

STD_ROM_PICK(butasanj)
STD_ROM_FN(butasanj)

struct BurnDriver BurnDrvButasanj = {
	"butasanj", "butasan", NULL, NULL, "1987",
	"Butasan (Japan, Japanese)\0", NULL, "NMK (Jaleco license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, butasanjRomInfo, butasanjRomName, NULL, NULL, NULL, NULL, ButasanInputInfo, ButasanDIPInfo,
	ButasanInit, DrvExit, DrvFrame, ButasanDraw, DrvScan, &DrvRecalc, 0x300,
	256, 240, 4, 3
};
