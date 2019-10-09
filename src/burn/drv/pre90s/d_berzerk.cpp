// FB Alpha Berzerk driver module
// Based on MAME driver by Zsolt Vasvari, Aaron Giles, R. Belmont, Jonathan Gevaryahu

#include "tiles_generic.h"
#include "z80_intf.h"
#include "bitswap.h"
#include "s14001a.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvSndROM;
static UINT8 *DrvNVRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvMagicRAM;
static UINT8 *DrvColRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 magicram_control = 0xff;
static UINT8 magicram_latch = 0xff;
static UINT8 collision = 0;

static INT32 nmi_enable;
static INT32 irq_enable;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy1f[8]; // fake inputs, for moonwarp left/right "dial"
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvInputs[10];
static UINT8 DrvDips[8];
static UINT8 DrvReset;

static INT32 vblank;

static INT32 moonwarp_mode = 0;

static struct BurnInputInfo BerzerkInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 6,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy4 + 0,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy4 + 7,	"diag"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",			BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
	{"Dip E",			BIT_DIPSWITCH,	DrvDips + 4,	"dip"		},
	{"Dip F",			BIT_DIPSWITCH,	DrvDips + 5,	"dip"		},
	{"Dip G",			BIT_DIPSWITCH,	DrvDips + 6,	"dip"		},
};

STDINPUTINFO(Berzerk)

static struct BurnInputInfo MoonwarpInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1f+ 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1f+ 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 fire 4"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy4 + 0,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy4 + 7,	"diag"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
	{"Dip D",			BIT_DIPSWITCH,	DrvDips + 4,	"dip"		},
	{"Dip E",			BIT_DIPSWITCH,	DrvDips + 5,	"dip"		},
	{"Dip F",			BIT_DIPSWITCH,	DrvDips + 6,	"dip"		},
};

STDINPUTINFO(Moonwarp)

static struct BurnDIPInfo BerzerkDIPList[]=
{
	{0x11, 0xff, 0xff, 0x80, NULL					},
	{0x12, 0xff, 0xff, 0xfc, NULL					},
	{0x13, 0xff, 0xff, 0x3c, NULL					},
	{0x14, 0xff, 0xff, 0xf0, NULL					},
	{0x15, 0xff, 0xff, 0xf0, NULL					},
	{0x16, 0xff, 0xff, 0xf0, NULL					},
	{0x17, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x11, 0x01, 0x80, 0x80, "Upright"				},
	{0x11, 0x01, 0x80, 0x00, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Color Test"			},
	{0x12, 0x01, 0x03, 0x00, "Off"					},
	{0x12, 0x01, 0x03, 0x03, "On"					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x12, 0x01, 0xc0, 0xc0, "5000 and 10000"		},
	{0x12, 0x01, 0xc0, 0x40, "5000"					},
	{0x12, 0x01, 0xc0, 0x80, "10000"				},
	{0x12, 0x01, 0xc0, 0x00, "None"					},

	{0   , 0xfe, 0   ,    2, "Input Test Mode"		},
	{0x13, 0x01, 0x01, 0x00, "Off"					},
	{0x13, 0x01, 0x01, 0x01, "On"					},

	{0   , 0xfe, 0   ,    2, "Crosshair Pattern"	},
	{0x13, 0x01, 0x02, 0x00, "Off"					},
	{0x13, 0x01, 0x02, 0x02, "On"					},

	{0   , 0xfe, 0   ,    4, "Language"				},
	{0x13, 0x01, 0xc0, 0x00, "English"				},
	{0x13, 0x01, 0xc0, 0x40, "German"				},
	{0x13, 0x01, 0xc0, 0x80, "French"				},
	{0x13, 0x01, 0xc0, 0xc0, "Spanish"				},

	{0   , 0xfe, 0   ,    16, "Coin 1"				},
	{0x14, 0x01, 0x0f, 0x09, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x0f, 0x0d, "4 Coins 3 Credits"	},
	{0x14, 0x01, 0x0f, 0x00, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x0f, 0x0e, "4 Coins 5 Credits"	},
	{0x14, 0x01, 0x0f, 0x0a, "2 Coins 3 Credits"	},
	{0x14, 0x01, 0x0f, 0x0f, "4 Coins 7 Credits"	},
	{0x14, 0x01, 0x0f, 0x01, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0x0f, 0x0b, "2 Coins 5 Credits"	},
	{0x14, 0x01, 0x0f, 0x02, "1 Coin  3 Credits"	},
	{0x14, 0x01, 0x0f, 0x0c, "2 Coins 7 Credits"	},
	{0x14, 0x01, 0x0f, 0x03, "1 Coin  4 Credits"	},
	{0x14, 0x01, 0x0f, 0x04, "1 Coin  5 Credits"	},
	{0x14, 0x01, 0x0f, 0x05, "1 Coin  6 Credits"	},
	{0x14, 0x01, 0x0f, 0x06, "1 Coin  7 Credits"	},
	{0x14, 0x01, 0x0f, 0x07, "1 Coin/10 Credits"	},
	{0x14, 0x01, 0x0f, 0x08, "1 Coin/14 Credits"	},

	{0   , 0xfe, 0   ,    16, "Coin 2"				},
	{0x15, 0x01, 0x0f, 0x09, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0x0f, 0x0d, "4 Coins 3 Credits"	},
	{0x15, 0x01, 0x0f, 0x00, "1 Coin  1 Credits"	},
	{0x15, 0x01, 0x0f, 0x0e, "4 Coins 5 Credits"	},
	{0x15, 0x01, 0x0f, 0x0a, "2 Coins 3 Credits"	},
	{0x15, 0x01, 0x0f, 0x0f, "4 Coins 7 Credits"	},
	{0x15, 0x01, 0x0f, 0x01, "1 Coin  2 Credits"	},
	{0x15, 0x01, 0x0f, 0x0b, "2 Coins 5 Credits"	},
	{0x15, 0x01, 0x0f, 0x02, "1 Coin  3 Credits"	},
	{0x15, 0x01, 0x0f, 0x0c, "2 Coins 7 Credits"	},
	{0x15, 0x01, 0x0f, 0x03, "1 Coin  4 Credits"	},
	{0x15, 0x01, 0x0f, 0x04, "1 Coin  5 Credits"	},
	{0x15, 0x01, 0x0f, 0x05, "1 Coin  6 Credits"	},
	{0x15, 0x01, 0x0f, 0x06, "1 Coin  7 Credits"	},
	{0x15, 0x01, 0x0f, 0x07, "1 Coin/10 Credits"	},
	{0x15, 0x01, 0x0f, 0x08, "1 Coin/14 Credits"	},

	{0   , 0xfe, 0   ,    16, "Coin 3"				},
	{0x16, 0x01, 0x0f, 0x09, "2 Coins 1 Credits"	},
	{0x16, 0x01, 0x0f, 0x0d, "4 Coins 3 Credits"	},
	{0x16, 0x01, 0x0f, 0x00, "1 Coin  1 Credits"	},
	{0x16, 0x01, 0x0f, 0x0e, "4 Coins 5 Credits"	},
	{0x16, 0x01, 0x0f, 0x0a, "2 Coins 3 Credits"	},
	{0x16, 0x01, 0x0f, 0x0f, "4 Coins 7 Credits"	},
	{0x16, 0x01, 0x0f, 0x01, "1 Coin  2 Credits"	},
	{0x16, 0x01, 0x0f, 0x0b, "2 Coins 5 Credits"	},
	{0x16, 0x01, 0x0f, 0x02, "1 Coin  3 Credits"	},
	{0x16, 0x01, 0x0f, 0x0c, "2 Coins 7 Credits"	},
	{0x16, 0x01, 0x0f, 0x03, "1 Coin  4 Credits"	},
	{0x16, 0x01, 0x0f, 0x04, "1 Coin  5 Credits"	},
	{0x16, 0x01, 0x0f, 0x05, "1 Coin  6 Credits"	},
	{0x16, 0x01, 0x0f, 0x06, "1 Coin  7 Credits"	},
	{0x16, 0x01, 0x0f, 0x07, "1 Coin/10 Credits"	},
	{0x16, 0x01, 0x0f, 0x08, "1 Coin/14 Credits"	},

	{0   , 0xfe, 0   ,    2, "Monitor Type"			},
	{0x17, 0x01, 0x01, 0x00, "Wells-Gardner"		},
	{0x17, 0x01, 0x01, 0x02, "Electrohome"			},
};

STDDIPINFO(Berzerk)

static struct BurnDIPInfo BerzerkfDIPList[] = {
	{0x13,	0xFF, 0xFF,	0xbc, NULL					},
};

STDDIPINFOEXT(Berzerkf,	Berzerk,	Berzerkf )

static struct BurnDIPInfo BerzerkgDIPList[] = {
	{0x13,	0xFF, 0xFF,	0x7c, NULL					},
};

STDDIPINFOEXT(Berzerkg,	Berzerk,	Berzerkg )

static struct BurnDIPInfo BerzerksDIPList[] = {
	{0x13,	0xFF, 0xFF,	0xfc, NULL					},
};

STDDIPINFOEXT(Berzerks,	Berzerk,	Berzerks )

static struct BurnDIPInfo FrenzyDIPList[]=
{
	{0x11, 0xff, 0xff, 0x80, NULL					},
	{0x12, 0xff, 0xff, 0x00, NULL					},
	{0x13, 0xff, 0xff, 0x03, NULL					},
	{0x14, 0xff, 0xff, 0x01, NULL					},
	{0x15, 0xff, 0xff, 0x01, NULL					},
	{0x16, 0xff, 0xff, 0x01, NULL					},
	{0x17, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x11, 0x01, 0x80, 0x80, "Upright"				},
	{0x11, 0x01, 0x80, 0x00, "Cocktail"				},

	{0   , 0xfe, 0   ,    4, "Hardware Tests"		},
	{0x12, 0x01, 0x03, 0x00, "Off"					},
	{0x12, 0x01, 0x03, 0x01, "Color test"			},
	{0x12, 0x01, 0x03, 0x02, "Unknown"				},
	{0x12, 0x01, 0x03, 0x03, "Signature Analysis"	},

	{0   , 0xfe, 0   ,    2, "Input Test Mode"		},
	{0x12, 0x01, 0x04, 0x00, "Off"					},
	{0x12, 0x01, 0x04, 0x04, "On"					},

	{0   , 0xfe, 0   ,    2, "Crosshair Pattern"	},
	{0x12, 0x01, 0x08, 0x00, "Off"					},
	{0x12, 0x01, 0x08, 0x08, "On"					},

	{0   , 0xfe, 0   ,    16, "Bonus Life"			},
	{0x13, 0x01, 0x0f, 0x01, "1000"					},
	{0x13, 0x01, 0x0f, 0x02, "2000"					},
	{0x13, 0x01, 0x0f, 0x03, "3000"					},
	{0x13, 0x01, 0x0f, 0x04, "4000"					},
	{0x13, 0x01, 0x0f, 0x05, "5000"					},
	{0x13, 0x01, 0x0f, 0x06, "6000"					},
	{0x13, 0x01, 0x0f, 0x07, "7000"					},
	{0x12, 0x01, 0x0f, 0x08, "8000"					},
	{0x13, 0x01, 0x0f, 0x09, "9000"					},
	{0x13, 0x01, 0x0f, 0x0a, "10000"				},
	{0x13, 0x01, 0x0f, 0x0b, "11000"				},
	{0x13, 0x01, 0x0f, 0x0c, "12000"				},
	{0x13, 0x01, 0x0f, 0x0d, "13000"				},
	{0x13, 0x01, 0x0f, 0x0e, "14000"				},
	{0x13, 0x01, 0x0f, 0x0f, "15000"				},
	{0x13, 0x01, 0x0f, 0x00, "None"					},

	{0   , 0xfe, 0   ,    4, "Language"				},
	{0x13, 0x01, 0xc0, 0x00, "English"				},
	{0x13, 0x01, 0xc0, 0x40, "German"				},
	{0x13, 0x01, 0xc0, 0x80, "French"				},
	{0x13, 0x01, 0xc0, 0xc0, "Spanish"				},

	{0   , 0xfe, 0   ,    17, "Coin Multiplier"		},
	{0x14, 0x01, 0xff, 0x00, "Free Play"			},
	{0x14, 0x01, 0xff, 0x01, "1"					},
	{0x14, 0x01, 0xff, 0x02, "2"					},
	{0x14, 0x01, 0xff, 0x03, "3"					},
	{0x14, 0x01, 0xff, 0x04, "4"					},
	{0x14, 0x01, 0xff, 0x05, "5"					},
	{0x14, 0x01, 0xff, 0x06, "6"					},
	{0x14, 0x01, 0xff, 0x07, "7"					},
	{0x14, 0x01, 0xff, 0x08, "8"					},
	{0x14, 0x01, 0xff, 0x09, "9"					},
	{0x14, 0x01, 0xff, 0x0a, "10"					},
	{0x14, 0x01, 0xff, 0x0b, "11"					},
	{0x14, 0x01, 0xff, 0x0c, "12"					},
	{0x14, 0x01, 0xff, 0x0d, "13"					},
	{0x14, 0x01, 0xff, 0x0e, "14"					},
	{0x14, 0x01, 0xff, 0x0f, "15"					},
	{0x14, 0x01, 0xff, 0xff, "255"					},

	{0   , 0xfe, 0   ,    17, "Coins/Credit A"		},
	{0x15, 0x01, 0xff, 0x00, "0 (invalid)"			},
	{0x15, 0x01, 0xff, 0x01, "1"					},
	{0x15, 0x01, 0xff, 0x02, "2"					},
	{0x15, 0x01, 0xff, 0x03, "3"					},
	{0x15, 0x01, 0xff, 0x04, "4"					},
	{0x15, 0x01, 0xff, 0x05, "5"					},
	{0x15, 0x01, 0xff, 0x06, "6"					},
	{0x15, 0x01, 0xff, 0x07, "7"					},
	{0x15, 0x01, 0xff, 0x08, "8"					},
	{0x15, 0x01, 0xff, 0x09, "9"					},
	{0x15, 0x01, 0xff, 0x0a, "10"					},
	{0x15, 0x01, 0xff, 0x0b, "11"					},
	{0x15, 0x01, 0xff, 0x0c, "12"					},
	{0x15, 0x01, 0xff, 0x0d, "13"					},
	{0x15, 0x01, 0xff, 0x0e, "14"					},
	{0x15, 0x01, 0xff, 0x0f, "15"					},
	{0x15, 0x01, 0xff, 0xff, "255"					},

	{0   , 0xfe, 0   ,    17, "Coins/Credit B"		},
	{0x16, 0x01, 0xff, 0x00, "0 (invalid)"			},
	{0x16, 0x01, 0xff, 0x01, "1"					},
	{0x16, 0x01, 0xff, 0x02, "2"					},
	{0x16, 0x01, 0xff, 0x03, "3"					},
	{0x16, 0x01, 0xff, 0x04, "4"					},
	{0x16, 0x01, 0xff, 0x05, "5"					},
	{0x16, 0x01, 0xff, 0x06, "6"					},
	{0x16, 0x01, 0xff, 0x07, "7"					},
	{0x16, 0x01, 0xff, 0x08, "8"					},
	{0x16, 0x01, 0xff, 0x09, "9"					},
	{0x16, 0x01, 0xff, 0x0a, "10"					},
	{0x16, 0x01, 0xff, 0x0b, "11"					},
	{0x16, 0x01, 0xff, 0x0c, "12"					},
	{0x16, 0x01, 0xff, 0x0d, "13"					},
	{0x16, 0x01, 0xff, 0x0e, "14"					},
	{0x16, 0x01, 0xff, 0x0f, "15"					},
	{0x16, 0x01, 0xff, 0xff, "255"					},

	{0   , 0xfe, 0   ,    2, "Monitor Type"			},
	{0x17, 0x01, 0x01, 0x00, "Wells-Gardner"		},
	{0x17, 0x01, 0x01, 0x02, "Electrohome"			},

};

STDDIPINFO(Frenzy)

static struct BurnDIPInfo MoonwarpDIPList[]=
{
	{0x0b, 0xff, 0xff, 0x00, NULL					},
	{0x0c, 0xff, 0xff, 0x00, NULL					},
	{0x0d, 0xff, 0xff, 0x00, NULL					},
	{0x0e, 0xff, 0xff, 0x00, NULL					},
	{0x0f, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    4, "Hardware Tests"		},
	{0x0b, 0x01, 0x03, 0x00, "Off"					},
	{0x0b, 0x01, 0x03, 0x01, "Color test"			},
	{0x0b, 0x01, 0x03, 0x02, "Unknown"				},
	{0x0b, 0x01, 0x03, 0x03, "Signature Analysis"	},

	{0   , 0xfe, 0   ,    6, "Bonus Life"			},
	{0x0b, 0x01, 0xf8, 0x00, "10000"				},
	{0x0b, 0x01, 0xf8, 0x08, "15000"				},
	{0x0b, 0x01, 0xf8, 0x10, "20000"				},
	{0x0b, 0x01, 0xf8, 0x20, "30000"				},
	{0x0b, 0x01, 0xf8, 0x40, "40000"				},
	{0x0b, 0x01, 0xf8, 0x80, "50000"				},

	{0   , 0xfe, 0   ,    2, "Input Test Mode"		},
	{0x0c, 0x01, 0x01, 0x00, "Off"					},
	{0x0c, 0x01, 0x01, 0x01, "On"					},

	{0   , 0xfe, 0   ,    2, "Crosshair Pattern"	},
	{0x0c, 0x01, 0x02, 0x00, "Off"					},
	{0x0c, 0x01, 0x02, 0x02, "On"					},

	{0   , 0xfe, 0   ,    2, "Spinner Orientation"	},
	{0x0c, 0x01, 0x08, 0x08, "Reverse"				},
	{0x0c, 0x01, 0x08, 0x00, "Standard"				},

	{0   , 0xfe, 0   ,    4, "Language"				},
	{0x0c, 0x01, 0xc0, 0x00, "English"				},
	{0x0c, 0x01, 0xc0, 0x40, "German"				},
	{0x0c, 0x01, 0xc0, 0x80, "French"				},
	{0x0c, 0x01, 0xc0, 0xc0, "Spanish"				},

	{0   , 0xfe, 0   ,    16, "Coin 1"				},
	{0x0d, 0x01, 0x0f, 0x09, "2 Coins 1 Credits"	},
	{0x0d, 0x01, 0x0f, 0x0d, "4 Coins 3 Credits"	},
	{0x0d, 0x01, 0x0f, 0x00, "1 Coin  1 Credits"	},
	{0x0d, 0x01, 0x0f, 0x0e, "4 Coins 5 Credits"	},
	{0x0d, 0x01, 0x0f, 0x0a, "2 Coins 3 Credits"	},
	{0x0d, 0x01, 0x0f, 0x0f, "4 Coins 7 Credits"	},
	{0x0d, 0x01, 0x0f, 0x01, "1 Coin  2 Credits"	},
	{0x0d, 0x01, 0x0f, 0x0b, "2 Coins 5 Credits"	},
	{0x0d, 0x01, 0x0f, 0x02, "1 Coin  3 Credits"	},
	{0x0d, 0x01, 0x0f, 0x0c, "2 Coins 7 Credits"	},
	{0x0d, 0x01, 0x0f, 0x03, "1 Coin  4 Credits"	},
	{0x0d, 0x01, 0x0f, 0x04, "1 Coin  5 Credits"	},
	{0x0d, 0x01, 0x0f, 0x05, "1 Coin  6 Credits"	},
	{0x0d, 0x01, 0x0f, 0x06, "1 Coin  7 Credits"	},
	{0x0d, 0x01, 0x0f, 0x07, "1 Coin/10 Credits"	},
	{0x0d, 0x01, 0x0f, 0x08, "1 Coin/14 Credits"	},

	{0   , 0xfe, 0   ,    16, "Coin 2"				},
	{0x0e, 0x01, 0x0f, 0x09, "2 Coins 1 Credits"	},
	{0x0e, 0x01, 0x0f, 0x0d, "4 Coins 3 Credits"	},
	{0x0e, 0x01, 0x0f, 0x00, "1 Coin  1 Credits"	},
	{0x0e, 0x01, 0x0f, 0x0e, "4 Coins 5 Credits"	},
	{0x0e, 0x01, 0x0f, 0x0a, "2 Coins 3 Credits"	},
	{0x0e, 0x01, 0x0f, 0x0f, "4 Coins 7 Credits"	},
	{0x0e, 0x01, 0x0f, 0x01, "1 Coin  2 Credits"	},
	{0x0e, 0x01, 0x0f, 0x0b, "2 Coins 5 Credits"	},
	{0x0e, 0x01, 0x0f, 0x02, "1 Coin  3 Credits"	},
	{0x0e, 0x01, 0x0f, 0x0c, "2 Coins 7 Credits"	},
	{0x0e, 0x01, 0x0f, 0x03, "1 Coin  4 Credits"	},
	{0x0e, 0x01, 0x0f, 0x04, "1 Coin  5 Credits"	},
	{0x0e, 0x01, 0x0f, 0x05, "1 Coin  6 Credits"	},
	{0x0e, 0x01, 0x0f, 0x06, "1 Coin  7 Credits"	},
	{0x0e, 0x01, 0x0f, 0x07, "1 Coin/10 Credits"	},
	{0x0e, 0x01, 0x0f, 0x08, "1 Coin/14 Credits"	},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x0e, 0x01, 0x80, 0x00, "Upright"				},
	{0x0e, 0x01, 0x80, 0x80, "Cocktail"				},

	{0   , 0xfe, 0   ,    16, "Coin 3"				},
	{0x0f, 0x01, 0x0f, 0x09, "2 Coins 1 Credits"	},
	{0x0f, 0x01, 0x0f, 0x0d, "4 Coins 3 Credits"	},
	{0x0f, 0x01, 0x0f, 0x00, "1 Coin  1 Credits"	},
	{0x0f, 0x01, 0x0f, 0x0e, "4 Coins 5 Credits"	},
	{0x0f, 0x01, 0x0f, 0x0a, "2 Coins 3 Credits"	},
	{0x0f, 0x01, 0x0f, 0x0f, "4 Coins 7 Credits"	},
	{0x0f, 0x01, 0x0f, 0x01, "1 Coin  2 Credits"	},
	{0x0f, 0x01, 0x0f, 0x0b, "2 Coins 5 Credits"	},
	{0x0f, 0x01, 0x0f, 0x02, "1 Coin  3 Credits"	},
	{0x0f, 0x01, 0x0f, 0x0c, "2 Coins 7 Credits"	},
	{0x0f, 0x01, 0x0f, 0x03, "1 Coin  4 Credits"	},
	{0x0f, 0x01, 0x0f, 0x04, "1 Coin  5 Credits"	},
	{0x0f, 0x01, 0x0f, 0x05, "1 Coin  6 Credits"	},
	{0x0f, 0x01, 0x0f, 0x06, "1 Coin  7 Credits"	},
	{0x0f, 0x01, 0x0f, 0x07, "1 Coin/10 Credits"	},
	{0x0f, 0x01, 0x0f, 0x08, "1 Coin/14 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x0f, 0x01, 0xc0, 0x00, "Very Easy"			},
	{0x0f, 0x01, 0xc0, 0x40, "Easy"					},
	{0x0f, 0x01, 0xc0, 0x80, "Normal"				},
	{0x0f, 0x01, 0xc0, 0xc0, "Hard"					},
};

STDDIPINFO(Moonwarp)

static void magicram_write(UINT16 offset, UINT8 data)
{
	INT32 shift_amount = magicram_control & 0x06;

	UINT16 data2 = ((data >> shift_amount) | (magicram_latch << (8 - shift_amount))) & 0x1ff;
	data2 >>= (magicram_control & 0x01);

	UINT8 data3 = data2;

	if (magicram_control & 0x08)
	{
		data3 = BITSWAP08(data3,0,1,2,3,4,5,6,7);
	}

	magicram_latch = data;

	collision |= ((data3 & DrvVidRAM[offset]) ? 0x80 : 0);

	switch (magicram_control & 0xf0)
	{
		case 0x00: 											break;	/* No change */
		case 0x10: data3 |=  DrvVidRAM[offset]; 			break;
		case 0x20: data3 |= ~DrvVidRAM[offset]; 			break;
		case 0x30: data3  = 0xff;  							break;
		case 0x40: data3 &=  DrvVidRAM[offset]; 			break;
		case 0x50: data3  =  DrvVidRAM[offset]; 			break;
		case 0x60: data3  = ~(data3 ^ DrvVidRAM[offset]);	break;
		case 0x70: data3  = ~data3 | DrvVidRAM[offset]; 	break;
		case 0x80: data3 &= ~DrvVidRAM[offset];			 	break;
		case 0x90: data3 ^=  DrvVidRAM[offset];			 	break;
		case 0xa0: data3  = ~DrvVidRAM[offset];				break;
		case 0xb0: data3  = ~(data3 & DrvVidRAM[offset]);	break;
		case 0xc0: data3  = 0x00; 						 	break;
		case 0xd0: data3  = ~data3 & DrvVidRAM[offset]; 	break;
		case 0xe0: data3  = ~(data3 | DrvVidRAM[offset]); 	break;
		case 0xf0: data3  = ~data3; 					 	break;
	}

	DrvMagicRAM[offset] = data3;
	DrvVidRAM[offset] = data3;
}

static void __fastcall berzerk_write(UINT16 address, UINT8 data)
{	
	if ((address & 0xe000) == 0x6000) {
		magicram_write(address & 0x1fff, data);
		return;
	}
}

// --- berzerk/exidy sound custom, original code by Aaron Giles ---
#define CRYSTAL_OSC				(3579545)
#define SH8253_CLOCK			(CRYSTAL_OSC / 2)
#define SH6840_CLOCK			(CRYSTAL_OSC / 4)
#define BASE_VOLUME				(32767 / 6)
#define S14001_CLOCK			(10000000 / 4)

/* 6840 variables */
struct sh6840_timer_channel
{
	UINT8	cr;
	UINT8	state;
	UINT8	leftovers;
	UINT16	timer;
	UINT32	clocks;
	union
	{
#ifdef LSB_FIRST
		struct { UINT8 l, h; } b;
#else
		struct { UINT8 h, l; } b;
#endif
		UINT16 w;
	} counter;
};
static struct sh6840_timer_channel sh6840_timer[3];
static INT16 sh6840_volume[3];
static UINT8 sh6840_MSB;
static UINT8 sh6840_LFSR_oldxor = 0; /* should be saved in savestate */
static UINT32 sh6840_LFSR_0;/* ditto */
static UINT32 sh6840_LFSR_1;/* ditto */
static UINT32 sh6840_LFSR_2;/* ditto */
static UINT32 sh6840_LFSR_3;/* ditto */
static UINT32 sh6840_clocks_per_sample;
static UINT32 sh6840_clock_count;

static UINT8 exidy_sfxctrl;

static UINT32 nSampleSize;
static INT32 samples_from; // samples per frame
static INT16 *mixer_buffer;
static INT32 nCurrentPosition;
static INT32 nFractionalPosition;

static void exidy_sound_init()
{
	samples_from = (INT32)((double)((SH8253_CLOCK * 100) / nBurnFPS) + 0.5);
	nSampleSize = (UINT64)SH8253_CLOCK * (1 << 16) / nBurnSoundRate;
	mixer_buffer = (INT16*)BurnMalloc(2 * sizeof(INT16) * SH8253_CLOCK);
	nCurrentPosition = 0;
	nFractionalPosition = 0;

	sh6840_clocks_per_sample = (int)((double)SH6840_CLOCK / (double)SH8253_CLOCK * (double)(1 << 24));
	s14001a_init(DrvSndROM, ZetTotalCycles, 2500000);
}

static void exidy_sound_exit()
{
	BurnFree(mixer_buffer);
	s14001a_exit();
}

static void exidy_sound_scan()
{
	SCAN_VAR(sh6840_MSB);
	SCAN_VAR(sh6840_volume);
	SCAN_VAR(exidy_sfxctrl);
	SCAN_VAR(sh6840_LFSR_oldxor);
	SCAN_VAR(sh6840_LFSR_0);
	SCAN_VAR(sh6840_LFSR_1);
	SCAN_VAR(sh6840_LFSR_2);
	SCAN_VAR(sh6840_LFSR_3);
}

static void berzerk_sound_write(UINT8 offset, UINT8 data); //forward
static void exidy_sync_stream(); // forward

static void exidy_sound_reset()
{
	berzerk_sound_write(4, 0x40);

	/* 6840 */
	memset(sh6840_timer, 0, sizeof(sh6840_timer));
	sh6840_MSB = 0;
	sh6840_volume[0] = 0;
	sh6840_volume[1] = 0;
	sh6840_volume[2] = 0;
	exidy_sfxctrl = 0;

	/* LFSR */
	sh6840_LFSR_oldxor = 0;
	sh6840_LFSR_0 = 0xffffffff;
	sh6840_LFSR_1 = 0xffffffff;
	sh6840_LFSR_2 = 0xffffffff;
	sh6840_LFSR_3 = 0xffffffff;
}

static void exidy_sound_write(UINT8 offset, UINT8 data)
{
	exidy_sync_stream();

	switch (offset)
	{
		/* offset 0 writes to either channel 0 control or channel 2 control */
		case 0:
			if (sh6840_timer[1].cr & 0x01)
				sh6840_timer[0].cr = data;
			else
				sh6840_timer[2].cr = data;
			break;

		/* offset 1 writes to channel 1 control */
		case 1:
			sh6840_timer[1].cr = data;
			break;

		/* offsets 2/4/6 write to the common MSB latch */
		case 2:
		case 4:
		case 6:
			sh6840_MSB = data;
			break;

		/* offsets 3/5/7 write to the LSB controls */
		case 3:
		case 5:
		case 7:
		{
			/* latch the timer value */
			INT32 ch = (offset - 3) / 2;
			sh6840_timer[ch].timer = (sh6840_MSB << 8) | (data & 0xff);

			/* if CR4 is clear, the value is loaded immediately */
			if (!(sh6840_timer[ch].cr & 0x10))
				sh6840_timer[ch].counter.w = sh6840_timer[ch].timer;
			break;
		}
	}
}

static void exidy_sfx_write(UINT8 offset, UINT8 data)
{
	exidy_sync_stream();

	switch (offset)
	{
		case 0:
			exidy_sfxctrl = data;
			break;

		case 1:
		case 2:
		case 3:
			sh6840_volume[offset - 1] = ((data & 7) * BASE_VOLUME) / 7;
			break;
	}
}

static void sh6840_apply_clock(struct sh6840_timer_channel *t, INT32 clocks)
{
	/* dual 8-bit case */
	if (t->cr & 0x04)
	{
		/* handle full decrements */
		while (clocks > t->counter.b.l)
		{
			clocks -= t->counter.b.l + 1;
			t->counter.b.l = t->timer;

			/* decrement MSB */
			if (!t->counter.b.h--)
			{
				t->state = 0;
				t->counter.w = t->timer;
			}

			/* state goes high when MSB is 0 */
			else if (!t->counter.b.h)
			{
				t->state = 1;
				t->clocks++;
			}
		}

		/* subtract off the remainder */
		t->counter.b.l -= clocks;
	}

	/* 16-bit case */
	else
	{
		/* handle full decrements */
		while (clocks > t->counter.w)
		{
			clocks -= t->counter.w + 1;
			t->state ^= 1;
			t->clocks += t->state;
			t->counter.w = t->timer;
		}

		/* subtract off the remainder */
		t->counter.w -= clocks;
	}
}

/*************************************
 *
 *  Noise generation helper
 *
 *************************************/

static INT32 sh6840_update_noise(INT32 clocks)
{
	UINT32 newxor;
	INT32 noise_clocks = 0;

	/* loop over clocks */
	for (INT32 i = 0; i < clocks; i++)
	{
		/* shift the LFSR. its a LOOOONG LFSR, so we need
        * four longs to hold it all!
        * first we grab new sample, then shift the high bits,
        * then the low ones; finally or in the result and see if we've
        * had a 0->1 transition */
		newxor = (sh6840_LFSR_3 ^ sh6840_LFSR_2) >> 31; /* high bits of 3 and 2 xored is new xor */
		sh6840_LFSR_3 <<= 1;
		sh6840_LFSR_3 |= sh6840_LFSR_2 >> 31;
		sh6840_LFSR_2 <<= 1;
		sh6840_LFSR_2 |= sh6840_LFSR_1 >> 31;
		sh6840_LFSR_1 <<= 1;
		sh6840_LFSR_1 |= sh6840_LFSR_0 >> 31;
		sh6840_LFSR_0 <<= 1;
		sh6840_LFSR_0 |= newxor ^ sh6840_LFSR_oldxor;
		sh6840_LFSR_oldxor = newxor;
		/*printf("LFSR: %4x, %4x, %4x, %4x\n", sh6840_LFSR_3, sh6840_LFSR_2, sh6840_LFSR_1, sh6840_LFSR_0);*/
		/* if we clocked 0->1, that will serve as an external clock */
		if ((sh6840_LFSR_2 & 0x03) == 0x01) /* tap is at 96th bit */
		{
			noise_clocks++;
		}
	}
	return noise_clocks;
}



/*************************************
 *
 *  Core sound generation
 *
 *************************************/

static void exidy_update(INT16 *buffer, INT32 length)
{
	INT32 noisy = ((sh6840_timer[0].cr & sh6840_timer[1].cr & sh6840_timer[2].cr & 0x02) == 0);

	/* loop over samples */
	while (length--)
	{
		struct sh6840_timer_channel *t;
		//struct sh8253_timer_channel *c;
		INT32 clocks_this_sample;
		INT16 sample = 0;

		/* determine how many 6840 clocks this sample */
		sh6840_clock_count += sh6840_clocks_per_sample;
		clocks_this_sample = sh6840_clock_count >> 24;
		sh6840_clock_count &= (1 << 24) - 1;

		/* skip if nothing enabled */
		if ((sh6840_timer[0].cr & 0x01) == 0)
		{
			INT32 noise_clocks_this_sample = 0;
			UINT32 chan0_clocks;

			/* generate E-clocked noise if configured to do so */
			if (noisy && !(exidy_sfxctrl & 0x01))
				noise_clocks_this_sample = sh6840_update_noise(clocks_this_sample);

			/* handle timer 0 if enabled */
			t = &sh6840_timer[0];
			chan0_clocks = t->clocks;
			if (t->cr & 0x80)
			{
				INT32 clocks = (t->cr & 0x02) ? clocks_this_sample : noise_clocks_this_sample;
				sh6840_apply_clock(t, clocks);
				if (t->state && !(exidy_sfxctrl & 0x02))
					sample += sh6840_volume[0];
			}

			/* generate channel 0-clocked noise if configured to do so */
			if (noisy && (exidy_sfxctrl & 0x01))
				noise_clocks_this_sample = sh6840_update_noise(t->clocks - chan0_clocks);

			/* handle timer 1 if enabled */
			t = &sh6840_timer[1];
			if (t->cr & 0x80)
			{
				INT32 clocks = (t->cr & 0x02) ? clocks_this_sample : noise_clocks_this_sample;
				sh6840_apply_clock(t, clocks);
				if (t->state)
					sample += sh6840_volume[1];
			}

			/* handle timer 2 if enabled */
			t = &sh6840_timer[2];
			if (t->cr & 0x80)
			{
				INT32 clocks = (t->cr & 0x02) ? clocks_this_sample : noise_clocks_this_sample;

				/* prescale */
				if (t->cr & 0x01)
				{
					clocks += t->leftovers;
					t->leftovers = clocks % 8;
					clocks /= 8;
				}
				sh6840_apply_clock(t, clocks);
				if (t->state)
					sample += sh6840_volume[2];
			}
		}

		/* stash */
		*buffer++ = sample;
	}
}

// Streambuffer handling
static INT32 SyncInternal()
{
	return (INT32)(float)(samples_from * (ZetTotalCycles() / (2500000 / (nBurnFPS / 100.0000))));
}

static void UpdateStream(INT32 length)
{
	length -= nCurrentPosition;
	if (length <= 0) return;

	INT16 *mix = mixer_buffer + 5 + nCurrentPosition;
	memset(mix, 0, length * sizeof(INT16));
	exidy_update(mix, length);
	nCurrentPosition += length;
}

static void exidy_sync_stream()
{
	UpdateStream(SyncInternal());
}

void exidy_render(INT16 *buffer, INT32 length)
{
	if (mixer_buffer == NULL || samples_from == 0) return;

	if (length != nBurnSoundLen) {
		bprintf(0, _T("exidy_render(): once per frame, please!\n"));
		return;
	}

	UpdateStream(samples_from);

	INT16 *pBufL = mixer_buffer + 5;

	for (INT32 i = (nFractionalPosition & 0xFFFF0000) >> 15; i < (length << 1); i += 2, nFractionalPosition += nSampleSize) {
		INT32 nLeftSample[4] = {0, 0, 0, 0};
		INT32 nTotalLeftSample; // it's mono!

		nLeftSample[0] += (INT32)(pBufL[(nFractionalPosition >> 16) - 3]);
		nLeftSample[1] += (INT32)(pBufL[(nFractionalPosition >> 16) - 2]);
		nLeftSample[2] += (INT32)(pBufL[(nFractionalPosition >> 16) - 1]);
		nLeftSample[3] += (INT32)(pBufL[(nFractionalPosition >> 16) - 0]);

		nTotalLeftSample  = INTERPOLATE4PS_16BIT((nFractionalPosition >> 4) & 0x0fff, nLeftSample[0], nLeftSample[1], nLeftSample[2], nLeftSample[3]);
		nTotalLeftSample  = BURN_SND_CLIP(nTotalLeftSample * 0.45);

		buffer[i + 0] = nTotalLeftSample;
		buffer[i + 1] = nTotalLeftSample;
	}
	nCurrentPosition = 0;

	if (length >= nBurnSoundLen) {
		INT32 nExtraSamples = samples_from - (nFractionalPosition >> 16);

		for (INT32 i = -4; i < nExtraSamples; i++) {
			pBufL[i] = pBufL[(nFractionalPosition >> 16) + i];
		}

		nFractionalPosition &= 0xFFFF;

		nCurrentPosition = nExtraSamples;
	}
}

static void berzerk_sound_write(UINT8 offset, UINT8 data)
{
	INT32 clock_divisor;

	switch (offset)
	{
		case 4:
			switch (data >> 6)
			{
				/* write data to the S14001 */
				case 0:
					/* only if not busy */
					if (!s14001a_bsy_read())
					{
						s14001a_reg_write(data & 0x3f);

						/* clock the chip -- via a 555 timer */
						s14001a_rst_write(1);
						s14001a_rst_write(0);
					}

					break;

				case 1:
					/* volume */
					s14001a_set_volume(((data & 0x38) >> 3) + 1);

					/* clock control - the first LS161 divides the clock by 9 to 16, the 2nd by 8,
					 giving a final clock from 19.5kHz to 34.7kHz */
					clock_divisor = 16 - (data & 0x07);

					s14001a_set_clock(S14001_CLOCK / clock_divisor / 8);

					break;

				default: break; /* 2 and 3 are not connected */
			}

			break;

			/* offset 6 writes to the sfxcontrol latch */
		case 6:
			exidy_sfx_write(data >> 6, data);
			break;

			/* everything else writes to the 6840 */
		default:
			exidy_sound_write(offset, data);
			break;

	}
}

static void __fastcall berzerk_write_port(UINT16 address, UINT8 data)
{
	switch (address & 0xff)
	{
		case 0x40:
		case 0x41:
		case 0x42:
		case 0x43:
		case 0x44:
		case 0x45:
		case 0x46:
		case 0x47:
			berzerk_sound_write(address&7, data);
		return;

		case 0x48:
		case 0x49:
		case 0x4a:
		return; // nop

		case 0x4b:
			magicram_control = data;
			magicram_latch = 0;
			collision = 0;
		return;

		case 0x4c:
			nmi_enable = 1;
		return;

		case 0x4d:
			nmi_enable = 0;
		return;

		case 0x4f:
			irq_enable = data & 1;
		return;

		case 0x50:
		case 0x51:
		case 0x52:
		case 0x53:
		case 0x54:
		case 0x55:
		case 0x56:
		case 0x57:
			// sound board #2 (not used)
		return;
	}
}

static UINT8 __fastcall berzerk_read_port(UINT16 address)
{
	if ((address & 0xe0) == 0x60) address &= ~0x18;

	switch (address & 0xff)
	{
		case 0x44:
			return (!s14001a_bsy_read()) ? 0x40 : 0x00;

		case 0x48: // p1
			return DrvInputs[0];

		case 0x49: // system
			return DrvInputs[2];

		case 0x4a: // p2
			return DrvInputs[1];

		case 0x4c:
			nmi_enable = 1;
			return 0;

		case 0x4d:
			nmi_enable = 0;
			return 0;

		case 0x4e:
			return (collision & 0x80) | 0x7e | (vblank & 1);

		case 0x60: // f3 = input test
			return DrvInputs[5];

		case 0x61: // f2 = color test
			return DrvInputs[4];

		case 0x62: // coin chute 3
			return DrvInputs[8];

		case 0x63: // coin chute 2
			return DrvInputs[7];

		case 0x64: // coin chute 1
			return DrvInputs[6];

		case 0x65: // service
			return DrvInputs[3];

		case 0x66:
		case 0x67:
			// led off, led on [67]
			return 0;
	}

	return 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	// sound
	exidy_sound_reset();
	s14001a_reset();

	magicram_control = 0xff;
	magicram_latch = 0xff;
	collision = 0;
	nmi_enable = 0;
	irq_enable = 0;
	vblank = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x010000;
	DrvSndROM		= Next; Next += 0x001000;

	DrvPalette		= (UINT32*)Next; Next += 0x0010 * sizeof(UINT32);

	DrvNVRAM		= Next; Next += 0x000400;

	AllRam			= Next;

	DrvVidRAM		= Next; Next += 0x002000;
	DrvMagicRAM		= Next; Next += 0x002000;
	DrvColRAM		= Next; Next += 0x000800;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvInit(INT32 berzerk)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	moonwarp_mode = !strcmp(BurnDrvGetTextA(DRV_NAME), "moonwarp");

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvZ80ROM + 0x0000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x1000,  k++, 1)) return 1;
		
		if (berzerk)
		{
			if (BurnLoadRom(DrvZ80ROM + 0x1800,  k++, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM + 0x2000,  k++, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM + 0x2800,  k++, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM + 0x3000,  k++, 1)) return 1;
			memset (DrvZ80ROM + 0x3800, 0xff, 0x800);
		}
		else
		{
			if (BurnLoadRom(DrvZ80ROM + 0x2000,  k++, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM + 0x3000,  k++, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM + 0xc000,  k++, 1)) return 1;
		
		}

		if (BurnLoadRom(DrvSndROM + 0x0000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM + 0x0800,  k++, 1)) return 1;
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,				0x0000, 0x3fff, MAP_ROM);
	if (berzerk)
	{
		ZetMapMemory(DrvNVRAM,			0x0800, 0x0bff, MAP_RAM); // over rom!
		ZetMapMemory(DrvNVRAM,			0x0c00, 0x0fff, MAP_RAM);
	}
	ZetMapMemory(DrvVidRAM,				0x4000, 0x5fff, MAP_RAM);
	ZetMapMemory(DrvMagicRAM,			0x6000, 0x7fff, MAP_ROM); // handler
	for (INT32 i = 0; i < 0x4000; i+= 0x800)
	{
		ZetMapMemory(DrvColRAM,			0x8000 + i, 0x87ff + i, MAP_RAM);
	}
	if (berzerk == 0)
	{
		ZetMapMemory(DrvZ80ROM + 0xc000,	0xc000, 0xcfff, MAP_ROM);
		ZetMapMemory(DrvNVRAM,				0xf800, 0xfbff, MAP_RAM);
		ZetMapMemory(DrvNVRAM,				0xfc00, 0xffff, MAP_RAM);
	}
	ZetSetWriteHandler(berzerk_write);
	ZetSetOutHandler(berzerk_write_port);
	ZetSetInHandler(berzerk_read_port);
	ZetClose();

	//	SOUND
	exidy_sound_init();

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	exidy_sound_exit();

	GenericTilesExit();
	ZetExit();

	BurnFree(AllMem);

	moonwarp_mode = 0;

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 16; i++)
	{
		INT32 n = (i & 8) ? 0x40 : 0x00;
		INT32 r = (i & 1) ? 0xff : n;
		INT32 g = (i & 2) ? 0xff : n;
		INT32 b = (i & 4) ? 0xff : n;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_layer()
{
	for (INT32 y = 32; y < 256; y++)
	{
		UINT16 *dst = pTransDraw + (y - 32) * nScreenWidth;
		UINT8 *vsrc = DrvVidRAM + (y * 32);
		UINT8 *csrc = DrvColRAM + ((y / 4) * 32);
		
		for (INT32 x = 0; x < 256; x += 8)
		{
			INT32 data = vsrc[x/8];
			INT32 color = csrc[x/8];

			for (INT32 sx = 0; sx < 8; sx++, dst++, data <<= 1)
			{
				if (sx & 4) {
					*dst = (data & 0x80) ? (color & 0xf) : 0;
				} else {
					*dst = (data & 0x80) ? (color >> 4) : 0;
				}
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

	draw_layer();

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
		DrvInputs[0] = 0xff; // p1
		DrvInputs[1] = (moonwarp_mode) ? 0xff : (0x7f | (DrvDips[0] & 0x80)); // p2 / upright/cocktail
		DrvInputs[2] = (moonwarp_mode) ? 0xfb : 0xff; // sys
		DrvInputs[3] = 0x7e; // service
		DrvInputs[4] = DrvDips[1]; // color test
		DrvInputs[5] = DrvDips[2]; // input test
		DrvInputs[6] = DrvDips[3]; // coin chute 1
		DrvInputs[7] = DrvDips[4]; // coin chute 2
		DrvInputs[8] = DrvDips[5]; // coin chute 3

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}

		if (moonwarp_mode) {
			static UINT8 p1_dir = 0;
			static UINT8 p1_cntr = 0;

			DrvInputs[0] &= ~0x1f; // cancel out dial bits (active high)
			DrvInputs[1] &= ~0x1f; // p2 (coctail) dial bits, not hooked up in this driver

			if (DrvJoy1f[0] || DrvJoy1f[1]) p1_cntr+=3;
			if (DrvJoy1f[0]) p1_dir = 0;
			if (DrvJoy1f[1]) p1_dir = 0x10;
			DrvInputs[0] |= (p1_cntr & 0xf) | p1_dir;
		}
	}

	INT32 nInterleave = 262;
	INT32 nCyclesTotal[1] = { 2500000 / 60 };
	INT32 nCyclesDone[1] = { 0 };

	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		if (i == 32) vblank = 0;
		if (i == 256) {
			if (pBurnDraw) {
				BurnDrvRedraw();
			}
			vblank = 1;
		}

		if (i == 128 || i == 256) {
			if (irq_enable) {
				ZetSetVector(0xfc);
				ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
			}
		}
		else if ((i % 32) == 0 && nmi_enable) {
			ZetNmi();
		}

		CPU_RUN(0, Zet);
	}

	ZetClose();

	if (pBurnSoundOut) {
		exidy_render(pBurnSoundOut, nBurnSoundLen);
		s14001a_render(pBurnSoundOut, nBurnSoundLen);
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

		exidy_sound_scan();
		s14001a_scan(nAction, pnMin);

		SCAN_VAR(magicram_control);
		SCAN_VAR(magicram_latch);
		SCAN_VAR(collision);
		SCAN_VAR(nmi_enable);
		SCAN_VAR(irq_enable);
	}

	if (nAction & ACB_NVRAM) {
		ba.Data		= DrvNVRAM;
		ba.nLen		= 0x00400;
		ba.nAddress	= 0;
		ba.szName	= "NV RAM";
		BurnAcb(&ba);
	}

	return 0;
}


// Berzerk (set 1)

static struct BurnRomInfo berzerkRomDesc[] = {
	{ "berzerk_rc31_1c.rom0.1c",	0x0800, 0xca566dbc, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "berzerk_rc31_1d.rom1.1d",	0x0800, 0x7ba69fde, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "berzerk_rc31_3d.rom2.3d",	0x0800, 0xa1d5248b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "berzerk_rc31_5d.rom3.5d",	0x0800, 0xfcaefa95, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "berzerk_rc31_6d.rom4.6d",	0x0800, 0x1e35b9a0, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "berzerk_rc31_5c.rom5.5c",	0x0800, 0xc8c665e5, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "berzerk_r_vo_1c.1c",			0x0800, 0x2cfe825d, 2 | BRF_SND },           //  6 Speech Data
	{ "berzerk_r_vo_2c.2c",			0x0800, 0xd2b6324e, 2 | BRF_SND },           //  7
};

STD_ROM_PICK(berzerk)
STD_ROM_FN(berzerk)

static INT32 BerzerkInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvBerzerk = {
	"berzerk", NULL, NULL, NULL, "1980",
	"Berzerk (set 1)\0", NULL, "Stern Electronics", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, berzerkRomInfo, berzerkRomName, NULL, NULL, NULL, NULL, BerzerkInputInfo, BerzerkDIPInfo,
	BerzerkInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	256, 224, 4, 3
};


// Berzerk (set 2)

static struct BurnRomInfo berzerk1RomDesc[] = {
	{ "berzerk_rc28_1c.rom0.1c",	0x0800, 0x5b7eb77d, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "berzerk_rc28_1d.rom1.1d",	0x0800, 0xe58c8678, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "berzerk_rc28_3d.rom2.3d",	0x0800, 0x705bb339, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "berzerk_rc28_5d.rom3.5d",	0x0800, 0x6a1936b4, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "berzerk_rc28_6d.rom4.6d",	0x0800, 0xfa5dce40, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "berzerk_rc28_5c.rom5.5c",	0x0800, 0x2579b9f4, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "berzerk_r_vo_1c.1c",			0x0800, 0x2cfe825d, 2 | BRF_SND },           //  6 Speech Data
	{ "berzerk_r_vo_2c.2c",			0x0800, 0xd2b6324e, 2 | BRF_SND },           //  7
};

STD_ROM_PICK(berzerk1)
STD_ROM_FN(berzerk1)

struct BurnDriver BurnDrvBerzerk1 = {
	"berzerk1", "berzerk", NULL, NULL, "1980",
	"Berzerk (set 2)\0", NULL, "Stern Electronics", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, berzerk1RomInfo, berzerk1RomName, NULL, NULL, NULL, NULL, BerzerkInputInfo, BerzerkDIPInfo,
	BerzerkInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	256, 224, 4, 3
};


// Berzerk (French Speech)

static struct BurnRomInfo berzerkfRomDesc[] = {
	{ "berzerk_rc31f_1c.rom0.1c",	0x0800, 0x3ba6e56e, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "berzerk_rc31f_1d.rom1.1d",	0x0800, 0xa1de2a3e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "berzerk_rc31f_3d.rom2.3d",	0x0800, 0xbc31c478, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "berzerk_rc31f_5d.rom3.5d",	0x0800, 0x316192b5, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "berzerk_rc31f_6d.rom4.6d",	0x0800, 0xcd51238c, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "berzerk_rc31f_5c.rom5.5c",	0x0800, 0x563b13b6, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "berzerk_rvof_1c.1c",			0x0800, 0xd7bfaca2, 2 | BRF_SND },           //  6 Speech Data
	{ "berzerk_rvof_2c.2c",			0x0800, 0x7bdc3573, 2 | BRF_SND },           //  7
};

STD_ROM_PICK(berzerkf)
STD_ROM_FN(berzerkf)

struct BurnDriver BurnDrvBerzerkf = {
	"berzerkf", "berzerk", NULL, NULL, "1980",
	"Berzerk (French Speech)\0", NULL, "Stern Electronics", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, berzerkfRomInfo, berzerkfRomName, NULL, NULL, NULL, NULL, BerzerkInputInfo, BerzerkfDIPInfo,
	BerzerkInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	256, 224, 4, 3
};


// Berzerk (German Speech)

static struct BurnRomInfo berzerkgRomDesc[] = {
	{ "cpu rom 00.1c",				0x0800, 0x77923a9e, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "cpu rom 01.1d",				0x0800, 0x19bb3aac, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "cpu rom 02.3d",				0x0800, 0xb0888ff7, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "cpu rom 03.5d",				0x0800, 0xe23239a9, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "cpu rom 04.6d",				0x0800, 0x651b31b7, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "cpu rom 05.5c",				0x0800, 0x8a403bba, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "berzerk_rvog_1c.1c",			0x0800, 0xfc1da15f, 2 | BRF_SND },           //  6 Speech Data
	{ "berzerk_rvog_2c.2c",			0x0800, 0x7f6808fb, 2 | BRF_SND },           //  7
};

STD_ROM_PICK(berzerkg)
STD_ROM_FN(berzerkg)

struct BurnDriver BurnDrvBerzerkg = {
	"berzerkg", "berzerk", NULL, NULL, "1980",
	"Berzerk (German Speech)\0", NULL, "Stern Electronics", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, berzerkgRomInfo, berzerkgRomName, NULL, NULL, NULL, NULL, BerzerkInputInfo, BerzerkgDIPInfo,
	BerzerkInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	256, 224, 4, 3
};


// Berzerk (Spanish Speech)

static struct BurnRomInfo berzerksRomDesc[] = {
	{ "berzerk_rc32_1c.rom0.1c",	0x0800, 0x77923a9e, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "berzerk_rc32_1d.rom1.1d",	0x0800, 0x19bb3aac, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "berzerk_rc32_3d.rom2.3d",	0x0800, 0x5423ea87, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "berzerk_rc32_5d.rom3.5d",	0x0800, 0xe23239a9, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "berzerk_rc32_6d.rom4.6d",	0x0800, 0x959efd86, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "berzerk_rc32s_5c.rom5.5c",	0x0800, 0x9ad80e4e, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "berzerk_rvos_1c.1c",			0x0800, 0x0b51409c, 2 | BRF_SND },           //  6 Speech Data
};

STD_ROM_PICK(berzerks)
STD_ROM_FN(berzerks)

struct BurnDriver BurnDrvBerzerks = {
	"berzerks", "berzerk", NULL, NULL, "1980",
	"Berzerk (Spanish Speech)\0", NULL, "Stern Electronics (Sonic License)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, berzerksRomInfo, berzerksRomName, NULL, NULL, NULL, NULL, BerzerkInputInfo, BerzerksDIPInfo,
	BerzerkInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	256, 224, 4, 3
};


// Frenzy

static struct BurnRomInfo frenzyRomDesc[] = {
	{ "frenzy_ral_rom1.1d",		0x1000, 0xabdd25b8, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "frenzy_ral_rom2.3d",		0x1000, 0x536e4ae8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "frenzy_ral_rom3.5d",		0x1000, 0x3eb9bc9b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "frenzy_ral_rom4.6d",		0x1000, 0xe1d3133c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "frenzy_ral_rom5.5c",		0x1000, 0x5581a7b1, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "e169-1cvo.1c",			0x0800, 0x2cfe825d, 2 | BRF_SND },           //  5 Speech Data
	{ "e169-2cvo.2c",			0x0800, 0xd2b6324e, 2 | BRF_SND },           //  6

	{ "frenzy_decoder_6ea1.6e",	0x0020, 0x4471ca5d, 3 | BRF_GRA },           //  7 Address PROM
};

STD_ROM_PICK(frenzy)
STD_ROM_FN(frenzy)

static INT32 FrenzyInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvFrenzy = {
	"frenzy", NULL, NULL, NULL, "1981",
	"Frenzy\0", NULL, "Stern Electronics", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, frenzyRomInfo, frenzyRomName, NULL, NULL, NULL, NULL, BerzerkInputInfo, FrenzyDIPInfo,
	FrenzyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	256, 224, 4, 3
};


// Moon War (prototype on Frenzy hardware)

static struct BurnRomInfo moonwarpRomDesc[] = {
	{ "1d.bin",					0x1000, 0x75470634, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "3d.bin",					0x1000, 0xa9d046dc, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "5d.bin",					0x1000, 0xbf671737, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "6d.bin",					0x1000, 0xcef2d697, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "5c.bin",					0x1000, 0xa3d551ab, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "moon_war_rv0_1c.1c",		0x0800, 0x9e9a653f, 2 | BRF_SND },           //  5 Speech Data
	{ "moon_war_rv0_2c.2c",		0x0800, 0x73fd988d, 2 | BRF_SND },           //  6

	{ "n82s123.6e",				0x0020, 0x4471ca5d, 3 | BRF_GRA },           //  7 Address PROMs
	{ "prom.6e",				0x0020, 0x56bffba3, 3 | BRF_GRA },           //  8
};

STD_ROM_PICK(moonwarp)
STD_ROM_FN(moonwarp)

static INT32 MoonwarpInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvMoonwarp = {
	"moonwarp", NULL, NULL, NULL, "1981",
	"Moon War (prototype on Frenzy hardware)\0", NULL, "Stern Electronics", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, moonwarpRomInfo, moonwarpRomName, NULL, NULL, NULL, NULL, MoonwarpInputInfo, MoonwarpDIPInfo,
	MoonwarpInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	256, 224, 4, 3
};
