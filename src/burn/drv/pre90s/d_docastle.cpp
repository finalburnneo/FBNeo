// Mr. Do's Castle emu-layer for FB Neo by dink, based on the MAME driver by Brad Oliver.

#include "tiles_generic.h"
#include "z80_intf.h"
#include "sn76496.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvRom0;
static UINT8 *DrvRom1;
static UINT8 *DrvRom2;
static UINT8 *DrvGfx0;
static UINT8 *DrvGfx1;
static UINT8 *DrvProm;

static UINT8 *DrvZ80RAM;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvZ80RAM2;
static UINT8 *DrvSharedRAM0;
static UINT8 *DrvSharedRAM1;

static UINT8 *DrvVidRAM;
static UINT8 *DrvSpriteRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 nExtraCycles[2];

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDip[3] = { 0, 0, 0 };
static UINT8 DrvInput[3];
static UINT8 DrvReset;

static HoldCoin<2> hold_coin;

static INT32 flipscreen = 0;

static UINT8 dorunrunmode = 0;

static struct BurnInputInfo DocastleInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 coin"	},
	{"P1 Start",	BIT_DIGITAL,	DrvJoy2 + 3,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 1"	},
	{"P1 Button 2",	BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 coin"	},
	{"P2 Start",	BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 left"	},
	{"P2 Right",	BIT_DIGITAL,	DrvJoy1 + 4,	"p2 right"	},
	{"P2 Button 1",	BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",	BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	    "reset"		},
	{"Service Mode",BIT_DIGITAL,	DrvJoy3 + 1,    "diag"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,    "service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy3 + 0,    "tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDip + 0,	    "dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDip + 1,	    "dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDip + 2,	    "dip"		},
};

STDINPUTINFO(Docastle)


static struct BurnDIPInfo DocastleDIPList[]=
{
	DIP_OFFSET(0x14)
	{0x00, 0xff, 0xff, 0x08, NULL						},
	{0x01, 0xff, 0xff, 0xff, NULL						},
	{0x02, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    2, "Freeze"					},
	{0x00, 0x01, 0x08, 0x08, "Off"						},
	{0x00, 0x01, 0x08, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x01, 0x01, 0x03, 0x03, "1 (Beginner)"				},
	{0x01, 0x01, 0x03, 0x02, "2"						},
	{0x01, 0x01, 0x03, 0x01, "3"						},
	{0x01, 0x01, 0x03, 0x00, "4 (Advanced)"				},

	{0   , 0xfe, 0   ,    2, "Rack Test"				},
	{0x01, 0x01, 0x04, 0x04, "Off"						},
	{0x01, 0x01, 0x04, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Advance Level on Getting Diamond"	},
	{0x01, 0x01, 0x08, 0x08, "On"						},
	{0x01, 0x01, 0x08, 0x00, "Off"						},

	{0   , 0xfe, 0   ,    2, "Difficulty of EXTRA"		},
	{0x01, 0x01, 0x10, 0x10, "Easy"						},
	{0x01, 0x01, 0x10, 0x00, "Difficult"				},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x01, 0x01, 0x20, 0x00, "Upright"					},
	{0x01, 0x01, 0x20, 0x20, "Cocktail"					},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x01, 0x01, 0xc0, 0x00, "2"						},
	{0x01, 0x01, 0xc0, 0xc0, "3"						},
	{0x01, 0x01, 0xc0, 0x80, "4"						},
	{0x01, 0x01, 0xc0, 0x40, "5"						},

	{0   , 0xfe, 0   ,    11, "Coin B"					},
	{0x02, 0x01, 0x0f, 0x06, "4 Coins 1 Credits"		},
	{0x02, 0x01, 0x0f, 0x08, "3 Coins 1 Credits"		},
	{0x02, 0x01, 0x0f, 0x0a, "2 Coins 1 Credits"		},
	{0x02, 0x01, 0x0f, 0x07, "3 Coins 2 Credits"		},
	{0x02, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"		},
	{0x02, 0x01, 0x0f, 0x09, "2 Coins 3 Credits"		},
	{0x02, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"		},
	{0x02, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"		},
	{0x02, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"		},
	{0x02, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"		},
	{0x02, 0x01, 0x0f, 0x00, "Free Play"				},

	{0   , 0xfe, 0   ,    11, "Coin A"					},
	{0x02, 0x01, 0xf0, 0x60, "4 Coins 1 Credits"		},
	{0x02, 0x01, 0xf0, 0x80, "3 Coins 1 Credits"		},
	{0x02, 0x01, 0xf0, 0xa0, "2 Coins 1 Credits"		},
	{0x02, 0x01, 0xf0, 0x70, "3 Coins 2 Credits"		},
	{0x02, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x02, 0x01, 0xf0, 0x90, "2 Coins 3 Credits"		},
	{0x02, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"		},
	{0x02, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"		},
	{0x02, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"		},
	{0x02, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"		},
	{0x02, 0x01, 0xf0, 0x00, "Free Play"				},
};

STDDIPINFO(Docastle)

static struct BurnDIPInfo DorunrunDIPList[]=
{
	DIP_OFFSET(0x14)
	{0x00, 0xff, 0xff, 0x08, NULL						},
	{0x01, 0xff, 0xff, 0xff, NULL						},
	{0x02, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    2, "Freeze"					},
	{0x00, 0x01, 0x08, 0x08, "Off"						},
	{0x00, 0x01, 0x08, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x01, 0x01, 0x03, 0x03, "1 (Beginner)"				},
	{0x01, 0x01, 0x03, 0x02, "2"						},
	{0x01, 0x01, 0x03, 0x01, "3"						},
	{0x01, 0x01, 0x03, 0x00, "4 (Advanced)"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x01, 0x01, 0x04, 0x00, "Off"						},
	{0x01, 0x01, 0x04, 0x04, "On"						},
#if 0
	{0   , 0xfe, 0   ,    2, "Flip Screen"				},
	{0x01, 0x01, 0x08, 0x08, "Off"						},
	{0x01, 0x01, 0x08, 0x00, "On"						},
#endif
	{0   , 0xfe, 0   ,    2, "Difficulty of EXTRA"		},
	{0x01, 0x01, 0x10, 0x10, "Easy"						},
	{0x01, 0x01, 0x10, 0x00, "Difficult"				},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x01, 0x01, 0x20, 0x00, "Upright"					},
	{0x01, 0x01, 0x20, 0x20, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Special"					},
	{0x01, 0x01, 0x40, 0x40, "Given"					},
	{0x01, 0x01, 0x40, 0x00, "Not Given"				},

	{0   , 0xfe, 0   ,    2, "Lives"					},
	{0x01, 0x01, 0x80, 0x80, "3"						},
	{0x01, 0x01, 0x80, 0x00, "5"						},

	{0   , 0xfe, 0   ,    11, "Coin B"					},
	{0x02, 0x01, 0x0f, 0x06, "4 Coins 1 Credits"		},
	{0x02, 0x01, 0x0f, 0x08, "3 Coins 1 Credits"		},
	{0x02, 0x01, 0x0f, 0x0a, "2 Coins 1 Credits"		},
	{0x02, 0x01, 0x0f, 0x07, "3 Coins 2 Credits"		},
	{0x02, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"		},
	{0x02, 0x01, 0x0f, 0x09, "2 Coins 3 Credits"		},
	{0x02, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"		},
	{0x02, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"		},
	{0x02, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"		},
	{0x02, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"		},
	{0x02, 0x01, 0x0f, 0x00, "Free Play"				},

	{0   , 0xfe, 0   ,    11, "Coin A"					},
	{0x02, 0x01, 0xf0, 0x60, "4 Coins 1 Credits"		},
	{0x02, 0x01, 0xf0, 0x80, "3 Coins 1 Credits"		},
	{0x02, 0x01, 0xf0, 0xa0, "2 Coins 1 Credits"		},
	{0x02, 0x01, 0xf0, 0x70, "3 Coins 2 Credits"		},
	{0x02, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x02, 0x01, 0xf0, 0x90, "2 Coins 3 Credits"		},
	{0x02, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"		},
	{0x02, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"		},
	{0x02, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"		},
	{0x02, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"		},
	{0x02, 0x01, 0xf0, 0x00, "Free Play"				},
};

STDDIPINFO(Dorunrun)

static struct BurnDIPInfo RunrunDIPList[]=
{
	DIP_OFFSET(0x14)
	{0x00, 0xff, 0xff, 0x08, NULL						},
	{0x01, 0xff, 0xff, 0xff, NULL						},
	{0x02, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    2, "Freeze"					},
	{0x00, 0x01, 0x08, 0x08, "Off"						},
	{0x00, 0x01, 0x08, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x01, 0x01, 0x03, 0x03, "1 (Beginner)"				},
	{0x01, 0x01, 0x03, 0x02, "2"						},
	{0x01, 0x01, 0x03, 0x01, "3"						},
	{0x01, 0x01, 0x03, 0x00, "4 (Advanced)"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x01, 0x01, 0x04, 0x00, "Off"						},
	{0x01, 0x01, 0x04, 0x04, "On"						},
#if 0
	{0   , 0xfe, 0   ,    2, "Flip Screen"				},
	{0x01, 0x01, 0x08, 0x08, "Off"						},
	{0x01, 0x01, 0x08, 0x00, "On"						},
#endif
	{0   , 0xfe, 0   ,    2, "Difficulty of EXTRA"		},
	{0x01, 0x01, 0x10, 0x10, "Easy"						},
	{0x01, 0x01, 0x10, 0x00, "Difficult"				},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x01, 0x01, 0x20, 0x00, "Upright"					},
	{0x01, 0x01, 0x20, 0x20, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Special"					},
	{0x01, 0x01, 0x40, 0x40, "Given"					},
	{0x01, 0x01, 0x40, 0x00, "Not Given"				},

	{0   , 0xfe, 0   ,    2, "Lives"					},
	{0x01, 0x01, 0x80, 0x80, "1"						},
	{0x01, 0x01, 0x80, 0x00, "2"						},

	{0   , 0xfe, 0   ,    11, "Coin B"					},
	{0x02, 0x01, 0x0f, 0x06, "4 Coins 1 Credits"		},
	{0x02, 0x01, 0x0f, 0x08, "3 Coins 1 Credits"		},
	{0x02, 0x01, 0x0f, 0x0a, "2 Coins 1 Credits"		},
	{0x02, 0x01, 0x0f, 0x07, "3 Coins 2 Credits"		},
	{0x02, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"		},
	{0x02, 0x01, 0x0f, 0x09, "2 Coins 3 Credits"		},
	{0x02, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"		},
	{0x02, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"		},
	{0x02, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"		},
	{0x02, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"		},
	{0x02, 0x01, 0x0f, 0x00, "Free Play"				},

	{0   , 0xfe, 0   ,    11, "Coin A"					},
	{0x02, 0x01, 0xf0, 0x60, "4 Coins 1 Credits"		},
	{0x02, 0x01, 0xf0, 0x80, "3 Coins 1 Credits"		},
	{0x02, 0x01, 0xf0, 0xa0, "2 Coins 1 Credits"		},
	{0x02, 0x01, 0xf0, 0x70, "3 Coins 2 Credits"		},
	{0x02, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x02, 0x01, 0xf0, 0x90, "2 Coins 3 Credits"		},
	{0x02, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"		},
	{0x02, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"		},
	{0x02, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"		},
	{0x02, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"		},
	{0x02, 0x01, 0xf0, 0x00, "Free Play"				},
};

STDDIPINFO(Runrun)

static struct BurnDIPInfo DowildDIPList[]=
{
	DIP_OFFSET(0x14)
	{0x00, 0xff, 0xff, 0x08, NULL						},
	{0x01, 0xff, 0xff, 0xff, NULL						},
	{0x02, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    2, "Freeze"					},
	{0x00, 0x01, 0x08, 0x08, "Off"						},
	{0x00, 0x01, 0x08, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x01, 0x01, 0x03, 0x03, "1 (Beginner)"				},
	{0x01, 0x01, 0x03, 0x02, "2"						},
	{0x01, 0x01, 0x03, 0x01, "3"						},
	{0x01, 0x01, 0x03, 0x00, "4 (Advanced)"				},

	{0   , 0xfe, 0   ,    2, "Rack Test"				},
	{0x01, 0x01, 0x04, 0x04, "Off"						},
	{0x01, 0x01, 0x04, 0x00, "On"						},
#if 0
	{0   , 0xfe, 0   ,    2, "Flip Screen"				},
	{0x01, 0x01, 0x08, 0x08, "Off"						},
	{0x01, 0x01, 0x08, 0x00, "On"						},
#endif
	{0   , 0xfe, 0   ,    2, "Difficulty of EXTRA"		},
	{0x01, 0x01, 0x10, 0x10, "Easy"						},
	{0x01, 0x01, 0x10, 0x00, "Difficult"				},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x01, 0x01, 0x20, 0x00, "Upright"					},
	{0x01, 0x01, 0x20, 0x20, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Special"					},
	{0x01, 0x01, 0x40, 0x40, "Given"					},
	{0x01, 0x01, 0x40, 0x00, "Not Given"				},

	{0   , 0xfe, 0   ,    2, "Lives"					},
	{0x01, 0x01, 0x80, 0x80, "3"						},
	{0x01, 0x01, 0x80, 0x00, "5"						},

	{0   , 0xfe, 0   ,    11, "Coin B"					},
	{0x02, 0x01, 0x0f, 0x06, "4 Coins 1 Credits"		},
	{0x02, 0x01, 0x0f, 0x08, "3 Coins 1 Credits"		},
	{0x02, 0x01, 0x0f, 0x0a, "2 Coins 1 Credits"		},
	{0x02, 0x01, 0x0f, 0x07, "3 Coins 2 Credits"		},
	{0x02, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"		},
	{0x02, 0x01, 0x0f, 0x09, "2 Coins 3 Credits"		},
	{0x02, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"		},
	{0x02, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"		},
	{0x02, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"		},
	{0x02, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"		},
	{0x02, 0x01, 0x0f, 0x00, "Free Play"				},

	{0   , 0xfe, 0   ,    11, "Coin A"					},
	{0x02, 0x01, 0xf0, 0x60, "4 Coins 1 Credits"		},
	{0x02, 0x01, 0xf0, 0x80, "3 Coins 1 Credits"		},
	{0x02, 0x01, 0xf0, 0xa0, "2 Coins 1 Credits"		},
	{0x02, 0x01, 0xf0, 0x70, "3 Coins 2 Credits"		},
	{0x02, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x02, 0x01, 0xf0, 0x90, "2 Coins 3 Credits"		},
	{0x02, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"		},
	{0x02, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"		},
	{0x02, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"		},
	{0x02, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"		},
	{0x02, 0x01, 0xf0, 0x00, "Free Play"				},
};

STDDIPINFO(Dowild)

static struct BurnDIPInfo JjackDIPList[]=
{
	DIP_OFFSET(0x14)
	{0x00, 0xff, 0xff, 0x08, NULL						},
	{0x01, 0xff, 0xff, 0xff, NULL						},
	{0x02, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    2, "Freeze"					},
	{0x00, 0x01, 0x08, 0x08, "Off"						},
	{0x00, 0x01, 0x08, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Difficulty?"				},
	{0x01, 0x01, 0x03, 0x03, "Easy"						},
	{0x01, 0x01, 0x03, 0x02, "Medium"					},
	{0x01, 0x01, 0x03, 0x01, "Hard"						},
	{0x01, 0x01, 0x03, 0x00, "Hardest"					},

	{0   , 0xfe, 0   ,    2, "Rack Test"				},
	{0x01, 0x01, 0x04, 0x04, "Off"						},
	{0x01, 0x01, 0x04, 0x00, "On"						},
#if 0
	{0   , 0xfe, 0   ,    2, "Flip Screen"				},
	{0x01, 0x01, 0x08, 0x08, "Off"						},
	{0x01, 0x01, 0x08, 0x00, "On"						},
#endif
	{0   , 0xfe, 0   ,    2, "Extra?"					},
	{0x01, 0x01, 0x10, 0x10, "Easy"						},
	{0x01, 0x01, 0x10, 0x00, "Hard"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x01, 0x01, 0x20, 0x00, "Upright"					},
	{0x01, 0x01, 0x20, 0x20, "Cocktail"					},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x01, 0x01, 0xc0, 0x00, "2"						},
	{0x01, 0x01, 0xc0, 0xc0, "3"						},
	{0x01, 0x01, 0xc0, 0x80, "4"						},
	{0x01, 0x01, 0xc0, 0x40, "5"						},

	{0   , 0xfe, 0   ,    11, "Coin B"					},
	{0x02, 0x01, 0x0f, 0x06, "4 Coins 1 Credits"		},
	{0x02, 0x01, 0x0f, 0x08, "3 Coins 1 Credits"		},
	{0x02, 0x01, 0x0f, 0x0a, "2 Coins 1 Credits"		},
	{0x02, 0x01, 0x0f, 0x07, "3 Coins 2 Credits"		},
	{0x02, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"		},
	{0x02, 0x01, 0x0f, 0x09, "2 Coins 3 Credits"		},
	{0x02, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"		},
	{0x02, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"		},
	{0x02, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"		},
	{0x02, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"		},
	{0x02, 0x01, 0x0f, 0x00, "Free Play"				},

	{0   , 0xfe, 0   ,    11, "Coin A"					},
	{0x02, 0x01, 0xf0, 0x60, "4 Coins 1 Credits"		},
	{0x02, 0x01, 0xf0, 0x80, "3 Coins 1 Credits"		},
	{0x02, 0x01, 0xf0, 0xa0, "2 Coins 1 Credits"		},
	{0x02, 0x01, 0xf0, 0x70, "3 Coins 2 Credits"		},
	{0x02, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x02, 0x01, 0xf0, 0x90, "2 Coins 3 Credits"		},
	{0x02, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"		},
	{0x02, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"		},
	{0x02, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"		},
	{0x02, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"		},
	{0x02, 0x01, 0xf0, 0x00, "Free Play"				},
};

STDDIPINFO(Jjack)

static struct BurnDIPInfo KickridrDIPList[]=
{
	DIP_OFFSET(0x14)
	{0x00, 0xff, 0xff, 0x08, NULL						},
	{0x01, 0xff, 0xff, 0xff, NULL						},
	{0x02, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    2, "Freeze"					},
	{0x00, 0x01, 0x08, 0x08, "Off"						},
	{0x00, 0x01, 0x08, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Difficulty?"				},
	{0x01, 0x01, 0x03, 0x03, "Easy"						},
	{0x01, 0x01, 0x03, 0x02, "Medium"					},
	{0x01, 0x01, 0x03, 0x01, "Hard"						},
	{0x01, 0x01, 0x03, 0x00, "Hardest"					},

	{0   , 0xfe, 0   ,    2, "Rack Test"				},
	{0x01, 0x01, 0x04, 0x04, "Off"						},
	{0x01, 0x01, 0x04, 0x00, "On"						},
#if 0
	{0   , 0xfe, 0   ,    2, "Flip Screen"				},
	{0x01, 0x01, 0x08, 0x08, "Off"						},
	{0x01, 0x01, 0x08, 0x00, "On"						},
#endif
	{0   , 0xfe, 0   ,    2, "DSW4"						},
	{0x01, 0x01, 0x10, 0x10, "Off"						},
	{0x01, 0x01, 0x10, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x01, 0x01, 0x20, 0x00, "Upright"					},
	{0x01, 0x01, 0x20, 0x20, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "DSW2"						},
	{0x01, 0x01, 0x40, 0x40, "Off"						},
	{0x01, 0x01, 0x40, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "DSW1"						},
	{0x01, 0x01, 0x80, 0x00, "Off"						},
	{0x01, 0x01, 0x80, 0x80, "On"						},

	{0   , 0xfe, 0   ,    11, "Coin B"					},
	{0x02, 0x01, 0x0f, 0x06, "4 Coins 1 Credits"		},
	{0x02, 0x01, 0x0f, 0x08, "3 Coins 1 Credits"		},
	{0x02, 0x01, 0x0f, 0x0a, "2 Coins 1 Credits"		},
	{0x02, 0x01, 0x0f, 0x07, "3 Coins 2 Credits"		},
	{0x02, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"		},
	{0x02, 0x01, 0x0f, 0x09, "2 Coins 3 Credits"		},
	{0x02, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"		},
	{0x02, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"		},
	{0x02, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"		},
	{0x02, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"		},
	{0x02, 0x01, 0x0f, 0x00, "Free Play"				},

	{0   , 0xfe, 0   ,    11, "Coin A"					},
	{0x02, 0x01, 0xf0, 0x60, "4 Coins 1 Credits"		},
	{0x02, 0x01, 0xf0, 0x80, "3 Coins 1 Credits"		},
	{0x02, 0x01, 0xf0, 0xa0, "2 Coins 1 Credits"		},
	{0x02, 0x01, 0xf0, 0x70, "3 Coins 2 Credits"		},
	{0x02, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x02, 0x01, 0xf0, 0x90, "2 Coins 3 Credits"		},
	{0x02, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"		},
	{0x02, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"		},
	{0x02, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"		},
	{0x02, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"		},
	{0x02, 0x01, 0xf0, 0x00, "Free Play"				},
};

STDDIPINFO(Kickridr)

static UINT8 shared0r(UINT8 offs)
{
	return DrvSharedRAM0[offs];
}

static UINT8 shared1r(UINT8 offs)
{
	return DrvSharedRAM1[offs];
}

static void shared0w(UINT8 offs, UINT8 data)
{
	if (offs == 8) {
		ZetSetHALT(0, 0);
	}
	DrvSharedRAM0[offs] = data;
}

static void shared1w(UINT8 offs, UINT8 data)
{
	if (offs == 8) {
		ZetSetHALT(0, 1);
	}
	DrvSharedRAM1[offs] = data;
}

static UINT8 __fastcall docastle_cpu0_read(UINT16 address)
{
	if (address >= 0xa000 && address <= 0xa008)
		return shared0r(address & 0xf);

	return 0;
}

static void __fastcall docastle_cpu0_write(UINT16 address, UINT8 data)
{
	if (address >= 0xa000 && address <= 0xa008) {
		shared1w(address & 0xf, data);
		return;
	}

	switch (address)
	{
		case 0xa800:
			// watchdog
		return;

		case 0xb800:
		case 0xe000:
			{
				INT32 cyc = ZetTotalCycles(0) - ZetTotalCycles(1);
				if (cyc > 0) {
					ZetRun(1, cyc); // synchronize subcpu before sending nmi.
				}
				ZetNmi(1);
			}
		return;
	}
}

static void __fastcall dorunrun_cpu1_write(UINT16 address, UINT8 data)
{
	if (address >= 0xe000 && address <= 0xe008) {
		shared0w(address & 0xf, data);
		return;
	}

	switch (address)
	{
		case 0xc004:
		case 0xc084:
			flipscreen = (address >> 7) & 1;
		return;

		case 0xa000:
		case 0xa400:
		case 0xa800:
		case 0xac00:
			SN76496Write((address >> 10) & 3, data);
		return;
	}
}

static void __fastcall docastle_cpu1_write(UINT16 address, UINT8 data)
{
	if (address >= 0xa000 && address <= 0xa008) {
		shared0w(address & 0xf, data);
		return;
	}

	switch (address)
	{
		case 0xc004:
		case 0xc084:
			flipscreen = (address >> 7) & 1;
		return;

		case 0xe000:
		case 0xe400:
		case 0xe800:
		case 0xec00:
			SN76496Write((address >> 10) & 3, data);
		return;
	}
}


static UINT8 __fastcall docastle_cpu1_read(UINT16 address)
{
	if (address >= 0xa000 && address <= 0xa008)
		return shared1r(address & 0xf);

	if (address >= 0xe000 && address <= 0xe008) // dorunrun+
		return shared1r(address & 0xf);

	switch (address & 0xff7f)
	{
		case 0xc001: return DrvDip[2];
		case 0xc002: return DrvDip[1];

		case 0xc003: return DrvInput[0];
		case 0xc005: return DrvInput[1];
		case 0xc007: return (DrvInput[2] & ~0x08) | (DrvDip[0] & 0x08);

		case 0xc004:
			flipscreen = (address & 0x80) >> 7;
			return flipscreen;
	}

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvRom0         = Next; Next += 0x10000;
	DrvRom1         = Next; Next += 0x10000;
	DrvRom2         = Next; Next += 0x10000;

	DrvGfx0         = Next; Next += 0x08000;
	DrvGfx1         = Next; Next += 0x10000;

	DrvProm         = Next; Next += 0x00200;

	DrvPalette      = (UINT32 *)Next; Next += 0x00800 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM       = Next; Next += 0x01800;
	DrvZ80RAM1      = Next; Next += 0x00800;
	DrvZ80RAM2      = Next; Next += 0x00800;
	DrvVidRAM       = Next; Next += 0x00800;
	DrvSpriteRAM    = Next; Next += 0x00200;
	DrvSharedRAM0   = Next; Next += 0x00010;
	DrvSharedRAM1   = Next; Next += 0x00010;

	RamEnd			= Next;

	MemEnd          = Next;

	return 0;
}


static INT32 DrvDoReset()
{
	DrvReset = 0;

	memset (AllRam, 0, RamEnd - AllRam);

	ZetReset(0);
	ZetReset(1);
	ZetReset(2);

	SN76496Reset();
	HiscoreReset();

	flipscreen = 0;

	nExtraCycles[0] = nExtraCycles[1] = 0;

	hold_coin.reset();

	return 0;
}

static void DrvPaletteInit()
{
	UINT8 *color_prom = DrvProm;

	for (INT32 i = 0; i < 0x100; i++)
	{
		INT32 bit0 = (*color_prom >> 5) & 0x01;
		INT32 bit1 = (*color_prom >> 6) & 0x01;
		INT32 bit2 = (*color_prom >> 7) & 0x01;
		INT32 r = 0x23 * bit0 + 0x4b * bit1 + 0x91 * bit2;

		bit0 = (*color_prom >> 2) & 0x01;
		bit1 = (*color_prom >> 3) & 0x01;
		bit2 = (*color_prom >> 4) & 0x01;
		INT32 g = 0x23 * bit0 + 0x4b * bit1 + 0x91 * bit2;

		bit0 = 0;
		bit1 = (*color_prom >> 0) & 0x01;
		bit2 = (*color_prom >> 1) & 0x01;
		INT32 b = 0x23 * bit0 + 0x4b * bit1 + 0x91 * bit2;

		DrvPalette[((i & ~0x07) * 2) + (i & ~0xf8) + (0 << 3)] = BurnHighCol(r, g, b, 0);
		DrvPalette[((i & ~0x07) * 2) + (i & ~0xf8) + (1 << 3)] = BurnHighCol(r, g, b, 0);
		color_prom++;
	}
}

static INT32 GraphicsDecode()
{
	static INT32 Planes[4]		= { STEP4(0, 1) };
	static INT32 TileYOffs[8]	= { STEP8(0, 32) };
	static INT32 XOffs[16]		= { STEP16(0, 4) };
	static INT32 SpriYOffs[16]	= { STEP16(0, 64) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x8000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfx0, 0x04000);

	GfxDecode(0x200, 4,  8,  8, Planes, XOffs, TileYOffs, 0x100, tmp, DrvGfx0);

	memcpy (tmp, DrvGfx1, 0x08000);

	GfxDecode(0x100, 4, 16, 16, Planes, XOffs, SpriYOffs, 0x400, tmp, DrvGfx1);

	BurnFree (tmp);

	return 0;
}

static tilemap_callback( fg )
{
	INT32 Code = DrvVidRAM[offs] + 0x08 * (DrvVidRAM[0x0400 + offs] & 0x20);
	INT32 Color = DrvVidRAM[0x0400 + offs] & 0x1f;

	TILE_SET_INFO(1, Code, Color, 0);
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	if (dorunrunmode) {
		if (BurnLoadRom(DrvRom0 + 0 * 0x2000, 0, 1)) return 1;
		if (BurnLoadRom(DrvRom0 + 2 * 0x2000, 1, 1)) return 1;
		if (BurnLoadRom(DrvRom0 + 3 * 0x2000, 2, 1)) return 1;
		if (BurnLoadRom(DrvRom0 + 4 * 0x2000, 3, 1)) return 1;

		if (BurnLoadRom(DrvGfx1 + 0 * 0x2000, 7, 1)) return 1;
		if (BurnLoadRom(DrvGfx1 + 1 * 0x2000, 8, 1)) return 1;
		if (BurnLoadRom(DrvGfx1 + 2 * 0x2000, 9, 1)) return 1;
		if (BurnLoadRom(DrvGfx1 + 3 * 0x2000,10, 1)) return 1;

		if (BurnLoadRom(DrvRom1,  4, 1)) return 1;
		if (BurnLoadRom(DrvRom2,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfx0,  6, 1)) return 1;
		if (BurnLoadRom(DrvProm, 11, 1)) return 1;
	} else 	{
		if (BurnLoadRom(DrvRom0 + 0 * 0x2000, 0, 1)) return 1;
		if (BurnLoadRom(DrvRom0 + 1 * 0x2000, 1, 1)) return 1;
		if (BurnLoadRom(DrvRom0 + 2 * 0x2000, 2, 1)) return 1;
		if (BurnLoadRom(DrvRom0 + 3 * 0x2000, 3, 1)) return 1;

		if (BurnLoadRom(DrvGfx1 + 0 * 0x2000, 7, 1)) return 1;
		if (BurnLoadRom(DrvGfx1 + 1 * 0x2000, 8, 1)) return 1;
		if (BurnLoadRom(DrvGfx1 + 2 * 0x2000, 9, 1)) return 1;
		if (BurnLoadRom(DrvGfx1 + 3 * 0x2000,10, 1)) return 1;

		if (BurnLoadRom(DrvRom1,  4, 1)) return 1;
		if (BurnLoadRom(DrvRom2,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfx0,  6, 1)) return 1;
		if (BurnLoadRom(DrvProm, 11, 1)) return 1;
	}

	if (GraphicsDecode()) return 1;
	DrvPaletteInit();

	ZetInit(0);
	ZetOpen(0);
	if (dorunrunmode) {
		ZetMapMemory(DrvRom0,           0x0000, 0x1fff, MAP_ROM);
		ZetMapMemory(DrvRom0 + 0x4000,  0x4000, 0x9fff, MAP_ROM);
		ZetMapMemory(DrvZ80RAM,         0x2000, 0x37ff, MAP_RAM);
		ZetMapMemory(DrvSpriteRAM,      0x3800, 0x39ff, MAP_RAM);
		ZetMapMemory(DrvVidRAM,         0xb000, 0xb7ff, MAP_RAM);
	} else {  // mr. do's castle
		ZetMapMemory(DrvRom0,           0x0000, 0x7fff, MAP_ROM);
		ZetMapMemory(DrvZ80RAM,         0x8000, 0x97ff, MAP_RAM);
		ZetMapMemory(DrvSpriteRAM,      0x9800, 0x99ff, MAP_RAM);
		ZetMapMemory(DrvVidRAM,         0xb000, 0xb7ff, MAP_RAM);
		ZetMapMemory(DrvVidRAM,         0xb800, 0xbfff, MAP_RAM); // mirror
	}
	ZetSetWriteHandler(docastle_cpu0_write);
	ZetSetReadHandler(docastle_cpu0_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvRom1,               0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,            0x8000, 0x87ff, MAP_RAM);
	ZetSetWriteHandler(docastle_cpu1_write);
	ZetSetReadHandler(docastle_cpu1_read);
	if (dorunrunmode) {
		ZetSetWriteHandler(dorunrun_cpu1_write);
	}

	ZetClose();

	ZetInit(2);
	ZetOpen(2);
	ZetMapMemory(DrvRom2,               0x0000, 0x00ff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM2,            0x4000, 0x47ff, MAP_RAM);
	ZetClose();

	SN76489AInit(0, 4000000, 0);
	SN76496SetRoute(0, 0.40, BURN_SND_ROUTE_BOTH);
	SN76489AInit(1, 4000000, 1);
	SN76496SetRoute(1, 0.40, BURN_SND_ROUTE_BOTH);
	SN76489AInit(2, 4000000, 1);
	SN76496SetRoute(2, 0.40, BURN_SND_ROUTE_BOTH);
	SN76489AInit(3, 4000000, 1);
	SN76496SetRoute(3, 0.40, BURN_SND_ROUTE_BOTH);
	SN76496SetBuffered(ZetTotalCycles, 4000000);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, fg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(1, DrvGfx0, 4,  8,  8, 0x200 * 8 * 8, 0x00, 0x3f);
	GenericTilemapSetTransSplit(0, 0, (dorunrunmode) ? 0xff00 : 0x00ff, 0x0000);
	GenericTilemapSetOffsets(TMAP_GLOBAL, -8, -32);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	SN76496Exit();
	ZetExit();
	GenericTilesExit();

	BurnFreeMemIndex();

	dorunrunmode = 0;

	return 0;
}

static void draw_sprites()
{
	for (INT32 offs = 0x200 - 4; offs >= 0; offs -= 4)
	{
		INT32 code = DrvSpriteRAM[offs + 3] & 0x1ff;
		INT32 color = DrvSpriteRAM[offs + 2] & 0x3f;
		INT32 sx = ((DrvSpriteRAM[offs + 1] + 8) & 0xff) - 8;
		INT32 sy = DrvSpriteRAM[offs];
		INT32 flipx = DrvSpriteRAM[offs + 2] & 0x40;
		INT32 flipy = DrvSpriteRAM[offs + 2] & 0x80;

		sy -= 32;
		sx -= 8;

		RenderPrioTransmaskSprite(pTransDraw, DrvGfx1, code, (color << 4), 0x80ff, sx, sy, flipx, flipy, 16, 16, 0);
		RenderPrioTransmaskSprite(pTransDraw, DrvGfx1, code, (color << 4), 0x7fff, sx, sy, flipx, flipy, 16, 16, 2);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	BurnTransferClear();

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, TMAP_DRAWLAYER1);

	memset(pPrioDraw, 1, nScreenWidth * nScreenHeight);
	if (nSpriteEnable & 1) draw_sprites();

	if (nBurnLayer & 2) GenericTilemapDraw(0, pTransDraw, TMAP_DRAWLAYER0);

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
		UINT8 *DrvJoy[3] = { DrvJoy1, DrvJoy2, DrvJoy3 };
		UINT32 DrvJoyInit[3] = { 0, 0xff, 0xff };

		CompileInput(DrvJoy, (void*)DrvInput, 3, 8, DrvJoyInit);

		hold_coin.checklow(0, DrvInput[2], 1 << 5, 2);
		hold_coin.checklow(1, DrvInput[2], 1 << 4, 2);

		// Both DrvInput[0], not a typo!
		ProcessJoystick(&DrvInput[0], 0, 1,3,2,0, INPUT_4WAY);
		ProcessJoystick(&DrvInput[0], 1, 5,7,6,4, INPUT_4WAY | INPUT_MAKEACTIVELOW);
	}

	INT32 nInterleave = 264;
	INT32 nCyclesTotal[3] = { 4000000 / 60, 4000000 / 60, 4000000 / 60 };
	INT32 nCyclesDone[3] = { nExtraCycles[0], nExtraCycles[1], 0 };

	ZetIdle(0, nExtraCycles[0]); // With CPU_RUN_SYNCINT() we must pre-load the cpu core's cycle counter with the roll-over cycles.
	ZetIdle(1, nExtraCycles[1]); //

	for (INT32 i = 0; i < nInterleave; i++) {
		ZetOpen(0);
		CPU_RUN_SYNCINT(0, Zet);
		if (i == 192) {
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
		ZetClose();

		ZetOpen(1);
		CPU_RUN_SYNCINT(1, Zet);
		if ((i % (nInterleave / 8)) == ((nInterleave / 8) - 1)) {
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
		ZetClose();

#if 0
		ZetOpen(2); // this z80 drives the crt controller, useless for emulation
		CPU_RUN(2, Zet);
		if (i == (nInterleave - 1)) ZetNmi();
		ZetClose();
#endif
	}

	nExtraCycles[0] = ZetTotalCycles(0) - nCyclesTotal[0];
	nExtraCycles[1] = ZetTotalCycles(1) - nCyclesTotal[1];

	if (pBurnSoundOut) {
		SN76496Update(pBurnSoundOut, nBurnSoundLen);
		BurnSoundDCFilter();
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
		*pnMin = 0x029735;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);
		SN76496Scan(nAction, pnMin);

		SCAN_VAR(flipscreen);
		SCAN_VAR(nExtraCycles);

		hold_coin.scan();
	}

	return 0;
}


// Mr. Do's Castle (set 1)

static struct BurnRomInfo docastleRomDesc[] = {
	{ "01p_a1.bin",     0x2000, 0x17c6fc24, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "01n_a2.bin",     0x2000, 0x1d2fc7f4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "01l_a3.bin",     0x2000, 0x71a70ba9, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "01k_a4.bin",     0x2000, 0x479a745e, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "07n_a0.bin",     0x4000, 0xf23b5cdb, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code

	{ "01d.bin",        0x0200, 0x2747ca77, 3 | BRF_PRG | BRF_ESS }, //  5 Z80 #2 Code

	{ "03a_a5.bin",     0x4000, 0x0636b8f4, 4 | BRF_GRA },           //  6 Tiles

	{ "04m_a6.bin",     0x2000, 0x3bbc9b26, 5 | BRF_GRA },           //  7 Sprites
	{ "04l_a7.bin",     0x2000, 0x3dfaa9d1, 5 | BRF_GRA },           //  8
	{ "04j_a8.bin",     0x2000, 0x9afb16e9, 5 | BRF_GRA },           //  9
	{ "04h_a9.bin",     0x2000, 0xaf24bce0, 5 | BRF_GRA },           // 10

	{ "09c.bin",        0x0200, 0x066f52bc, 6 | BRF_GRA },           // 11 Color DrvProm
};

STD_ROM_PICK(docastle)
STD_ROM_FN(docastle)

struct BurnDriver BurnDrvdocastle = {
	"docastle", NULL, NULL, NULL, "1983",
	"Mr. Do\'s Castle (set 1)\0", NULL, "Universal", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, docastleRomInfo, docastleRomName, NULL, NULL, NULL, NULL, DocastleInputInfo, DocastleDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	192, 240, 3, 4
};

// Mr. Do's Castle (set 2)

static struct BurnRomInfo docastle2RomDesc[] = {
	{ "a1",				0x2000, 0x0d81fafc, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "a2",				0x2000, 0xa13dc4ac, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a3",				0x2000, 0xa1f04ffb, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "a4",				0x2000, 0x1fb14aa6, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "a10",			0x4000, 0x45f7f69b, 2 | BRF_GRA },           //  4 slave

	{ "01d.bin",		0x0200, 0x2747ca77, 3 | BRF_PRG | BRF_ESS }, //  5 cpu3

	{ "03a_a5.bin",		0x4000, 0x0636b8f4, 4 | BRF_GRA },           //  6 gfx1

	{ "04m_a6.bin",		0x2000, 0x3bbc9b26, 5 | BRF_GRA },           //  7 gfx2
	{ "04l_a7.bin",		0x2000, 0x3dfaa9d1, 5 | BRF_GRA },           //  8
	{ "04j_a8.bin",		0x2000, 0x9afb16e9, 5 | BRF_GRA },           //  9
	{ "04h_a9.bin",		0x2000, 0xaf24bce0, 5 | BRF_GRA },           // 10

	{ "09c.bin",		0x0200, 0x066f52bc, 6 | BRF_GRA },           // 11 proms
};

STD_ROM_PICK(docastle2)
STD_ROM_FN(docastle2)

struct BurnDriver BurnDrvDocastle2 = {
	"docastle2", "docastle", NULL, NULL, "1983",
	"Mr. Do's Castle (set 2)\0", NULL, "Universal", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, docastle2RomInfo, docastle2RomName, NULL, NULL, NULL, NULL, DocastleInputInfo, DocastleDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	192, 240, 3, 4
};


// Mr. Do's Castle (older)

static struct BurnRomInfo docastleoRomDesc[] = {
	{ "c1.bin",			0x2000, 0xc9ce96ab, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "c2.bin",			0x2000, 0x42b28369, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "c3.bin",			0x2000, 0xc8c13124, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "c4.bin",			0x2000, 0x7ca78471, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "dorev10.bin",	0x4000, 0x4b1925e3, 2 | BRF_GRA },           //  4 slave

	{ "01d.bin",		0x0200, 0x2747ca77, 3 | BRF_PRG | BRF_ESS }, //  5 cpu3

	{ "03a_a5.bin",		0x4000, 0x0636b8f4, 4 | BRF_GRA },           //  6 gfx1

	{ "dorev6.bin",		0x2000, 0x9e335bf8, 5 | BRF_GRA },           //  7 gfx2
	{ "dorev7.bin",		0x2000, 0xf5d5701d, 5 | BRF_GRA },           //  8
	{ "dorev8.bin",		0x2000, 0x7143ca68, 5 | BRF_GRA },           //  9
	{ "dorev9.bin",		0x2000, 0x893fc004, 5 | BRF_GRA },           // 10

	{ "dorevc9.bin",	0x0200, 0x96624ebe, 6 | BRF_GRA },           // 11 proms
};

STD_ROM_PICK(docastleo)
STD_ROM_FN(docastleo)

struct BurnDriver BurnDrvDocastleo = {
	"docastleo", "docastle", NULL, NULL, "1983",
	"Mr. Do's Castle (older)\0", NULL, "Universal", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, docastleoRomInfo, docastleoRomName, NULL, NULL, NULL, NULL, DocastleInputInfo, DocastleDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	192, 240, 3, 4
};

// Mr. Do! vs. Unicorns (Japan)

static struct BurnRomInfo douniRomDesc[] = {
	{ "dorev1.bin",		0x2000, 0x1e2cbb3c, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "dorev2.bin",		0x2000, 0x18418f83, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "dorev3.bin",		0x2000, 0x7b9e2061, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "dorev4.bin",		0x2000, 0xe013954d, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "dorev10.bin",	0x4000, 0x4b1925e3, 2 | BRF_GRA },           //  4 slave

	{ "01d.bin",		0x0200, 0x2747ca77, 3 | BRF_PRG | BRF_ESS }, //  5 cpu3

	{ "03a_a5.bin",		0x4000, 0x0636b8f4, 4 | BRF_GRA },           //  6 gfx1

	{ "dorev6.bin",		0x2000, 0x9e335bf8, 5 | BRF_GRA },           //  7 gfx2
	{ "dorev7.bin",		0x2000, 0xf5d5701d, 5 | BRF_GRA },           //  8
	{ "dorev8.bin",		0x2000, 0x7143ca68, 5 | BRF_GRA },           //  9
	{ "dorev9.bin",		0x2000, 0x893fc004, 5 | BRF_GRA },           // 10

	{ "dorevc9.bin",	0x0200, 0x96624ebe, 6 | BRF_GRA },           // 11 proms
};

STD_ROM_PICK(douni)
STD_ROM_FN(douni)

struct BurnDriver BurnDrvDouni = {
	"douni", "docastle", NULL, NULL, "1983",
	"Mr. Do! vs. Unicorns (Japan)\0", NULL, "Universal", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, douniRomInfo, douniRomName, NULL, NULL, NULL, NULL, DocastleInputInfo, DocastleDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	192, 240, 3, 4
};

static INT32 DorunrunDrvInit()
{
	dorunrunmode = 1;

	return DrvInit();
}

// Do! Run Run (set 1)

static struct BurnRomInfo dorunrunRomDesc[] = {
	{ "2764.p1",		0x2000, 0x95c86f8e, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "2764.l1",		0x2000, 0xe9a65ba7, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "2764.k1",		0x2000, 0xb1195d3d, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "2764.n1",		0x2000, 0x6a8160d1, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "27128.p7",		0x4000, 0x8b06d461, 2 | BRF_PRG | BRF_ESS }, //  4 slave

	{ "bprom2.bin",		0x0200, 0x2747ca77, 3 | BRF_PRG | BRF_ESS }, //  5 cpu3

	{ "27128.a3",		0x4000, 0x4be96dcf, 4 | BRF_GRA }, 			 //  6 gfx1

	{ "2764.m4",		0x2000, 0x4bb231a0, 5 | BRF_GRA }, 			 //  7 gfx2
	{ "2764.l4",		0x2000, 0x0c08508a, 5 | BRF_GRA },			 //  8
	{ "2764.j4",		0x2000, 0x79287039, 5 | BRF_GRA },			 //  9
	{ "2764.h4",		0x2000, 0x523aa999, 5 | BRF_GRA },			 // 10

	{ "dorunrun.clr",	0x0100, 0xd5bab5d5, 6 | BRF_GRA },			 // 11 proms
};

STD_ROM_PICK(dorunrun)
STD_ROM_FN(dorunrun)

struct BurnDriver BurnDrvDorunrun = {
	"dorunrun", NULL, NULL, NULL, "1984",
	"Do! Run Run (set 1)\0", NULL, "Universal", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, dorunrunRomInfo, dorunrunRomName, NULL, NULL, NULL, NULL, DocastleInputInfo, DorunrunDIPInfo,
	DorunrunDrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 192, 4, 3
};

// Do! Run Run (set 2)

static struct BurnRomInfo dorunrun2RomDesc[] = {
	{ "p1",				0x2000, 0x12a99365, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "l1",				0x2000, 0x38609287, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "k1",				0x2000, 0x099aaf54, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "n1",				0x2000, 0x4f8fcbae, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "27128.p7",		0x4000, 0x8b06d461, 2 | BRF_GRA },           //  4 slave

	{ "bprom2.bin",		0x0200, 0x2747ca77, 3 | BRF_PRG | BRF_ESS }, //  5 cpu3

	{ "27128.a3",		0x4000, 0x4be96dcf, 4 | BRF_GRA },           //  6 gfx1

	{ "2764.m4",		0x2000, 0x4bb231a0, 5 | BRF_GRA },           //  7 gfx2
	{ "2764.l4",		0x2000, 0x0c08508a, 5 | BRF_GRA },           //  8
	{ "2764.j4",		0x2000, 0x79287039, 5 | BRF_GRA },           //  9
	{ "2764.h4",		0x2000, 0x523aa999, 5 | BRF_GRA },           // 10

	{ "dorunrun.clr",	0x0100, 0xd5bab5d5, 6 | BRF_GRA },           // 11 proms
};

STD_ROM_PICK(dorunrun2)
STD_ROM_FN(dorunrun2)

struct BurnDriver BurnDrvDorunrun2 = {
	"dorunrun2", "dorunrun", NULL, NULL, "1984",
	"Do! Run Run (set 2)\0", NULL, "Universal", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, dorunrun2RomInfo, dorunrun2RomName, NULL, NULL, NULL, NULL, DocastleInputInfo, DorunrunDIPInfo,
	DorunrunDrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 192, 4, 3
};

// Do! Run Run (Do's Castle hardware, set 1)

static struct BurnRomInfo dorunruncRomDesc[] = {
	{ "rev-0-1.p1",		0x2000, 0x49906ebd, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "rev-0-2.n1",		0x2000, 0xdbe3e7db, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rev-0-3.l1",		0x2000, 0xe9b8181a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rev-0-4.k1",		0x2000, 0xa63d0b89, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "rev-0-2.n7",		0x4000, 0x6dac2fa3, 2 | BRF_GRA },           //  4 slave

	{ "bprom2.bin",		0x0200, 0x2747ca77, 3 | BRF_PRG | BRF_ESS }, //  5 cpu3

	{ "rev-0-5.a3",		0x4000, 0xe20795b7, 4 | BRF_GRA },           //  6 gfx1

	{ "2764.m4",		0x2000, 0x4bb231a0, 5 | BRF_GRA },           //  7 gfx2
	{ "2764.l4",		0x2000, 0x0c08508a, 5 | BRF_GRA },           //  8
	{ "2764.j4",		0x2000, 0x79287039, 5 | BRF_GRA },           //  9
	{ "2764.h4",		0x2000, 0x523aa999, 5 | BRF_GRA },           // 10

	{ "dorunrun.clr",	0x0100, 0xd5bab5d5, 6 | BRF_GRA },           // 11 proms
};

STD_ROM_PICK(dorunrunc)
STD_ROM_FN(dorunrunc)

struct BurnDriver BurnDrvDorunrunc = {
	"dorunrunc", "dorunrun", NULL, NULL, "1984",
	"Do! Run Run (Do's Castle hardware, set 1)\0", NULL, "Universal", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, dorunruncRomInfo, dorunruncRomName, NULL, NULL, NULL, NULL, DocastleInputInfo, DorunrunDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 192, 4, 3
};


// Do! Run Run (Do's Castle hardware, set 2)

static struct BurnRomInfo dorunruncaRomDesc[] = {
	{ "runrunc_p1",		0x2000, 0xa8d789c6, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "runrunc_n1",		0x2000, 0xf8c8a9f4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "runrunc_l1",		0x2000, 0x223927ca, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "runrunc_k1",		0x2000, 0x5edd6752, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "rev-0-2.n7",		0x4000, 0x6dac2fa3, 2 | BRF_GRA },           //  4 slave

	{ "bprom2.bin",		0x0200, 0x2747ca77, 3 | BRF_PRG | BRF_ESS }, //  5 cpu3

	{ "rev-0-5.a3",		0x4000, 0xe20795b7, 4 | BRF_GRA },           //  6 gfx1

	{ "2764.m4",		0x2000, 0x4bb231a0, 5 | BRF_GRA },           //  7 gfx2
	{ "2764.l4",		0x2000, 0x0c08508a, 5 | BRF_GRA },           //  8
	{ "2764.j4",		0x2000, 0x79287039, 5 | BRF_GRA },           //  9
	{ "2764.h4",		0x2000, 0x523aa999, 5 | BRF_GRA },           // 10

	{ "dorunrun.clr",	0x0100, 0xd5bab5d5, 6 | BRF_GRA },           // 11 proms
};

STD_ROM_PICK(dorunrunca)
STD_ROM_FN(dorunrunca)

struct BurnDriver BurnDrvDorunrunca = {
	"dorunrunca", "dorunrun", NULL, NULL, "1984",
	"Do! Run Run (Do's Castle hardware, set 2)\0", NULL, "Universal", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, dorunruncaRomInfo, dorunruncaRomName, NULL, NULL, NULL, NULL, DocastleInputInfo, DorunrunDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 192, 4, 3
};


// Run Run (Do! Run Run bootleg)

static struct BurnRomInfo runrunRomDesc[] = {
	{ "electric_1.bin",		0x2000, 0x9f23896b, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "electric_2.bin",		0x2000, 0xdbe3e7db, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "electric_3.bin",		0x2000, 0xe9b8181a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "electric_4.bin",		0x2000, 0xa63d0b89, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "electric_10.bin",	0x4000, 0x6dac2fa3, 2 | BRF_GRA },           //  4 slave

	{ "bprom2.bin",			0x0200, 0x2747ca77, 3 | BRF_PRG | BRF_ESS }, //  5 cpu3

	{ "electric_5.bin",		0x4000, 0xe20795b7, 4 | BRF_GRA },           //  6 gfx1

	{ "electric_6.bin",		0x2000, 0x4bb231a0, 5 | BRF_GRA },           //  7 gfx2
	{ "electric_7.bin",		0x2000, 0x0c08508a, 5 | BRF_GRA },           //  8
	{ "electric_8.bin",		0x2000, 0x79287039, 5 | BRF_GRA },           //  9
	{ "electric_9.bin",		0x2000, 0x523aa999, 5 | BRF_GRA },           // 10

	{ "dorunrun.clr",		0x0100, 0xd5bab5d5, 6 | BRF_GRA },           // 11 proms
};

STD_ROM_PICK(runrun)
STD_ROM_FN(runrun)

struct BurnDriver BurnDrvRunrun = {
	"runrun", "dorunrun", NULL, NULL, "1984",
	"Run Run (Do! Run Run bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, runrunRomInfo, runrunRomName, NULL, NULL, NULL, NULL, DocastleInputInfo, RunrunDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 192, 4, 3
};

// Super Pierrot (Japan)

static struct BurnRomInfo spieroRomDesc[] = {
	{ "sp1.bin",		0x2000, 0x08d23e38, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "sp3.bin",		0x2000, 0xfaa0c18c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sp4.bin",		0x2000, 0x639b4e5d, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sp2.bin",		0x2000, 0x3a29ccb0, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "27128.p7",		0x4000, 0x8b06d461, 2 | BRF_GRA },           //  4 slave

	{ "bprom2.bin",		0x0200, 0x2747ca77, 3 | BRF_PRG | BRF_ESS }, //  5 cpu3

	{ "sp5.bin",		0x4000, 0x1b704bb0, 4 | BRF_GRA },           //  6 gfx1

	{ "sp6.bin",		0x2000, 0x00f893a7, 5 | BRF_GRA },           //  7 gfx2
	{ "sp7.bin",		0x2000, 0x173e5c6a, 5 | BRF_GRA },           //  8
	{ "sp8.bin",		0x2000, 0x2e66525a, 5 | BRF_GRA },           //  9
	{ "sp9.bin",		0x2000, 0x9c571525, 5 | BRF_GRA },           // 10

	{ "bprom1.bin",		0x0200, 0xfc1b66ff, 6 | BRF_GRA },           // 11 proms
};

STD_ROM_PICK(spiero)
STD_ROM_FN(spiero)

struct BurnDriver BurnDrvSpiero = {
	"spiero", "dorunrun", NULL, NULL, "1987",
	"Super Pierrot (Japan)\0", NULL, "Universal", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, spieroRomInfo, spieroRomName, NULL, NULL, NULL, NULL, DocastleInputInfo, DorunrunDIPInfo,
	DorunrunDrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 192, 4, 3
};

// Mr. Do's Wild Ride

static struct BurnRomInfo dowildRomDesc[] = {
	{ "w1",		        0x2000, 0x097de78b, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "w3",		        0x2000, 0xfc6a1cbb, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "w4",		        0x2000, 0x8aac1d30, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "w2",		        0x2000, 0x0914ab69, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "w10",	        0x4000, 0xd1f37fba, 2 | BRF_PRG | BRF_ESS }, //  4 slave

	{ "8300b-2",        0x0200, 0x2747ca77, 3 | BRF_PRG | BRF_ESS }, //  5 cpu3

	{ "w5",		        0x4000, 0xb294b151, 4 | BRF_GRA },			 //  6 gfx1

	{ "w6",		        0x2000, 0x57e0208b, 5 | BRF_GRA },			 //  7 gfx2
	{ "w7",		        0x2000, 0x5001a6f7, 5 | BRF_GRA },			 //  8
	{ "w8",		        0x2000, 0xec503251, 5 | BRF_GRA },			 //  9
	{ "w9",		        0x2000, 0xaf7bd7eb, 5 | BRF_GRA },			 // 10

	{ "dowild.clr",	    0x0100, 0xa703dea5, 6 | BRF_GRA },			 // 11 proms
};

STD_ROM_PICK(dowild)
STD_ROM_FN(dowild)

struct BurnDriver BurnDrvDowild = {
	"dowild", NULL, NULL, NULL, "1984",
	"Mr. Do's Wild Ride\0", NULL, "Universal", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, dowildRomInfo, dowildRomName, NULL, NULL, NULL, NULL, DocastleInputInfo, DowildDIPInfo,
	DorunrunDrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 192, 4, 3
};

// Jumping Jack

static struct BurnRomInfo jjackRomDesc[] = {
	{ "j1.bin",	        0x2000, 0x87f29bd2, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "j3.bin",	        0x2000, 0x35b0517e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "j4.bin",	        0x2000, 0x35bb316a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "j2.bin",	        0x2000, 0xdec52e80, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "j0.bin",	        0x4000, 0xab042f04, 2 | BRF_PRG | BRF_ESS }, //  4 slave

	{ "bprom2.bin",	    0x0200, 0x2747ca77, 3 | BRF_GRA },			 //  5 cpu3

	{ "j5.bin",	        0x4000, 0x75038ff9, 4 | BRF_GRA },			 //  6 gfx1

	{ "j6.bin",	        0x2000, 0x5937bd7b, 5 | BRF_GRA },			 //  7 gfx2
	{ "j7.bin",	        0x2000, 0xcf8ae8e7, 5 | BRF_GRA },			 //  8
	{ "j8.bin",	        0x2000, 0x84f6fc8c, 5 | BRF_GRA },			 //  9
	{ "j9.bin",	        0x2000, 0x3f9bb09f, 5 | BRF_GRA },			 // 10

	{ "bprom1.bin",	    0x0200, 0x2f0955f2, 6 | BRF_GRA },			 // 11 proms
};

STD_ROM_PICK(jjack)
STD_ROM_FN(jjack)

struct BurnDriver BurnDrvJjack = {
	"jjack", NULL, NULL, NULL, "1984",
	"Jumping Jack\0", NULL, "Universal", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, jjackRomInfo, jjackRomName, NULL, NULL, NULL, NULL, DocastleInputInfo, JjackDIPInfo,
	DorunrunDrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	192, 240, 3, 4
};


// Kick Rider

static struct BurnRomInfo kickridrRomDesc[] = {
	{ "k1",			    0x2000, 0xdfdd1ab4, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "k3",			    0x2000, 0x412244da, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "k4",			    0x2000, 0xa67dd2ec, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "k2",			    0x2000, 0xe193fb5c, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "k10",		    0x4000, 0x6843dbc0, 2 | BRF_PRG | BRF_ESS }, //  4 slave

	{ "8300b-2",	    0x0200, 0x2747ca77, 3 | BRF_PRG | BRF_ESS }, //  5 cpu3

	{ "k5",			    0x4000, 0x3f7d7e49, 4 | BRF_GRA },			 //  6 gfx1

	{ "k6",			    0x2000, 0x94252ed3, 5 | BRF_GRA },			 //  7 gfx2
	{ "k7",			    0x2000, 0x7ef2420e, 5 | BRF_GRA },			 //  8
	{ "k8",			    0x2000, 0x29bed201, 5 | BRF_GRA },			 //  9
	{ "k9",			    0x2000, 0x847584d3, 5 | BRF_GRA },			 // 10

	{ "kickridr.clr",	0x0100, 0x73ec281c, 6 | BRF_GRA },			 // 11 proms
};

STD_ROM_PICK(kickridr)
STD_ROM_FN(kickridr)

struct BurnDriver BurnDrvKickridr = {
	"kickridr", NULL, NULL, NULL, "1984",
	"Kick Rider\0", NULL, "Universal", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, kickridrRomInfo, kickridrRomName, NULL, NULL, NULL, NULL, DocastleInputInfo, KickridrDIPInfo,
	DorunrunDrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 192, 4, 3
};
