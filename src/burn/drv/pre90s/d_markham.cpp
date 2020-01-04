// FB Alpha Markham / Strength & Skill / Ikki driver module
// Based on MAME driver by UKI

#include "tiles_generic.h"
#include "z80_intf.h"
#include "sn76496.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *MemEnd;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvVidPROM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvShareRAM;
static UINT8 *DrvTransTab;
static UINT8 *DrvProtROM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 *scroll;
static INT32 irq_source;
static UINT8 scroll_control;
static UINT8 flipscreen;

static UINT8 packet_buffer[2];
static UINT8 packet_reset;
static UINT8 packet_write_pos;

static INT32 irq_scanline[4];

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo MarkhamInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 7,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,  DrvDips + 2,    "dip"		},
};

STDINPUTINFO(Markham)

static struct BurnInputInfo StrnskilInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 7,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,  DrvDips + 2,    "dip"		},
};

STDINPUTINFO(Strnskil)

static struct BurnInputInfo BanbamInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 7,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Banbam)

static struct BurnInputInfo IkkiInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 7,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Ikki)

static struct BurnDIPInfo MarkhamDIPList[]=
{
	{0x12, 0xff, 0xff, 0x02, NULL					},
	{0x13, 0xff, 0xff, 0x7e, NULL					},
	{0x14, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    2, "Lives"				},
	{0x12, 0x01, 0x01, 0x00, "3"					},
	{0x12, 0x01, 0x01, 0x01, "5"					},

	{0   , 0xfe, 0   ,    1, "Cabinet"				},
	{0x12, 0x01, 0x02, 0x02, "Upright"				},
//	{0x12, 0x01, 0x02, 0x00, "Cocktail"				},

	{0   , 0xfe, 0   ,    1, "Flip Screen"			},
	{0x12, 0x01, 0x04, 0x00, "Off"					},
//	{0x12, 0x01, 0x04, 0x04, "On"					},

	{0   , 0xfe, 0   ,    2, "Coin Chutes"			},
	{0x12, 0x01, 0x08, 0x00, "Individual"			},
	{0x12, 0x01, 0x08, 0x08, "Common"				},

	{0   , 0xfe, 0   ,   16, "Coin1 / Coin2"		},
	{0x12, 0x01, 0xf0, 0x00, "1C 1C / 1C 1C"		},
	{0x12, 0x01, 0xf0, 0x10, "2C 1C / 2C 1C"		},
	{0x12, 0x01, 0xf0, 0x20, "2C 1C / 1C 3C"		},
	{0x12, 0x01, 0xf0, 0x30, "1C 1C / 1C 2C"		},
	{0x12, 0x01, 0xf0, 0x40, "1C 1C / 1C 3C"		},
	{0x12, 0x01, 0xf0, 0x50, "1C 1C / 1C 4C"		},
	{0x12, 0x01, 0xf0, 0x60, "1C 1C / 1C 5C"		},
	{0x12, 0x01, 0xf0, 0x70, "1C 1C / 1C 6C"		},
	{0x12, 0x01, 0xf0, 0x80, "1C 2C / 1C 2C"		},
	{0x12, 0x01, 0xf0, 0x90, "1C 2C / 1C 4C"		},
	{0x12, 0x01, 0xf0, 0xa0, "1C 2C / 1C 5C"		},
	{0x12, 0x01, 0xf0, 0xb0, "1C 2C / 1C 10C"		},
	{0x12, 0x01, 0xf0, 0xc0, "1C 2C / 1C 11C"		},
	{0x12, 0x01, 0xf0, 0xd0, "1C 2C / 1C 12C"		},
	{0x12, 0x01, 0xf0, 0xe0, "1C 2C / 1C 6C"		},
	{0x12, 0x01, 0xf0, 0xf0, "Free Play"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x13, 0x01, 0x03, 0x00, "None"					},
	{0x13, 0x01, 0x03, 0x01, "20000"				},
	{0x13, 0x01, 0x03, 0x02, "20000, Every 50000"	},
	{0x13, 0x01, 0x03, 0x03, "20000, Every 80000"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x13, 0x01, 0x04, 0x00, "Off"					},
	{0x13, 0x01, 0x04, 0x04, "On"					},
	
	{0   , 0xfe, 0   ,    2, "Freeze"				},
	{0x13, 0x01, 0x80, 0x00, "Off"					},
	{0x13, 0x01, 0x80, 0x80, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x14, 0x01, 0x10, 0x00, "Off"					},
	{0x14, 0x01, 0x10, 0x10, "On"					},
};

STDDIPINFO(Markham)

static struct BurnDIPInfo StrnskilDIPList[]=
{
	{0x12, 0xff, 0xff, 0x00, NULL					},
	{0x13, 0xff, 0xff, 0x00, NULL					},
	{0x14, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x12, 0x01, 0x01, 0x01, "Off"					},
	{0x12, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    16, "Coin1 / Coin2"		},
	{0x12, 0x01, 0xf0, 0x00, "1C 1C / 1C 1C"		},
	{0x12, 0x01, 0xf0, 0x10, "2C 1C / 2C 1C"		},
	{0x12, 0x01, 0xf0, 0x20, "2C 1C / 1C 3C"		},
	{0x12, 0x01, 0xf0, 0x30, "1C 1C / 1C 2C"		},
	{0x12, 0x01, 0xf0, 0x40, "1C 1C / 1C 3C"		},
	{0x12, 0x01, 0xf0, 0x50, "1C 1C / 1C 4C"		},
	{0x12, 0x01, 0xf0, 0x60, "1C 1C / 1C 5C"		},
	{0x12, 0x01, 0xf0, 0x70, "1C 1C / 1C 6C"		},
	{0x12, 0x01, 0xf0, 0x80, "1C 2C / 1C 2C"		},
	{0x12, 0x01, 0xf0, 0x90, "1C 2C / 1C 4C"		},
	{0x12, 0x01, 0xf0, 0xa0, "1C 2C / 1C 5C"		},
	{0x12, 0x01, 0xf0, 0xb0, "1C 2C / 1C 10C"		},
	{0x12, 0x01, 0xf0, 0xc0, "1C 2C / 1C 11C"		},
	{0x12, 0x01, 0xf0, 0xd0, "1C 2C / 1C 12C"		},
	{0x12, 0x01, 0xf0, 0xe0, "1C 2C / 1C 6C"		},
	{0x12, 0x01, 0xf0, 0xf0, "Free Play"			},

	{0   , 0xfe, 0   ,    2, "Difficulty"			},
	{0x13, 0x01, 0x01, 0x00, "Normal"				},
	{0x13, 0x01, 0x01, 0x01, "Hard"					},

	{0   , 0xfe, 0   ,    2, "Freeze"				},
	{0x13, 0x01, 0x80, 0x00, "Off"					},
	{0x13, 0x01, 0x80, 0x80, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x14, 0x01, 0x20, 0x00, "Off"					},
	{0x14, 0x01, 0x20, 0x20, "On"					},
};

STDDIPINFO(Strnskil)

static struct BurnDIPInfo BanbamDIPList[]=
{
	{0x10, 0xff, 0xff, 0x02, NULL					},
	{0x11, 0xff, 0xff, 0x70, NULL					},

	{0   , 0xfe, 0   ,    2, "Lives"				},
	{0x10, 0x01, 0x01, 0x00, "3"					},
	{0x10, 0x01, 0x01, 0x01, "5"					},

	{0   , 0xfe, 0   ,    1, "Cabinet"				},
	{0x10, 0x01, 0x02, 0x02, "Upright"				},
//	{0x10, 0x01, 0x02, 0x00, "Cocktail"				},

	{0   , 0xfe, 0   ,    1, "Flip Screen"			},
	{0x10, 0x01, 0x04, 0x00, "Off"					},
//	{0x10, 0x01, 0x04, 0x04, "On"					},

	{0   , 0xfe, 0   ,    16, "Coin1 / Coin2"		},
	{0x10, 0x01, 0xf0, 0x00, "1C 1C / 1C 1C"		},
	{0x10, 0x01, 0xf0, 0x10, "2C 1C / 2C 1C"		},
	{0x10, 0x01, 0xf0, 0x20, "2C 1C / 1C 3C"		},
	{0x10, 0x01, 0xf0, 0x30, "1C 1C / 1C 2C"		},
	{0x10, 0x01, 0xf0, 0x40, "1C 1C / 1C 3C"		},
	{0x10, 0x01, 0xf0, 0x50, "1C 1C / 1C 4C"		},
	{0x10, 0x01, 0xf0, 0x60, "1C 1C / 1C 5C"		},
	{0x10, 0x01, 0xf0, 0x70, "1C 1C / 1C 6C"		},
	{0x10, 0x01, 0xf0, 0x80, "1C 2C / 1C 2C"		},
	{0x10, 0x01, 0xf0, 0x90, "1C 2C / 1C 4C"		},
	{0x10, 0x01, 0xf0, 0xa0, "1C 2C / 1C 5C"		},
	{0x10, 0x01, 0xf0, 0xb0, "1C 2C / 1C 10C"		},
	{0x10, 0x01, 0xf0, 0xc0, "1C 2C / 1C 11C"		},
	{0x10, 0x01, 0xf0, 0xd0, "1C 2C / 1C 12C"		},
	{0x10, 0x01, 0xf0, 0xe0, "1C 2C / 1C 6C"		},
	{0x10, 0x01, 0xf0, 0xf0, "Free Play"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x11, 0x01, 0x01, 0x01, "Off"					},
	{0x11, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x11, 0x01, 0x06, 0x00, "20000 50000"			},
	{0x11, 0x01, 0x06, 0x02, "20000 80000"			},
	{0x11, 0x01, 0x06, 0x04, "20000"				},
	{0x11, 0x01, 0x06, 0x06, "None"					},

	{0   , 0xfe, 0   ,    2, "Second Practice"		},
	{0x11, 0x01, 0x08, 0x08, "Off"					},
	{0x11, 0x01, 0x08, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Freeze"				},
	{0x11, 0x01, 0x80, 0x00, "Off"					},
	{0x11, 0x01, 0x80, 0x80, "On"					},
};

STDDIPINFO(Banbam)

static struct BurnDIPInfo IkkiDIPList[]=
{
	{0x10, 0xff, 0xff, 0x00, NULL					},
	{0x11, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    2, "Lives"				},
	{0x10, 0x01, 0x01, 0x00, "3"					},
	{0x10, 0x01, 0x01, 0x01, "5"					},

	{0   , 0xfe, 0   ,    2, "2 Player Game"		},
	{0x10, 0x01, 0x02, 0x00, "2 Credits"			},
	{0x10, 0x01, 0x02, 0x02, "1 Credit"				},

	{0   , 0xfe, 0   ,    1, "Flip Screen"			},
	{0x10, 0x01, 0x04, 0x00, "Off"					},
//	{0x10, 0x01, 0x04, 0x04, "On"					},

	{0   , 0xfe, 0   ,    16, "Coin1 / Coin2"		},
	{0x10, 0x01, 0xf0, 0x00, "1 Coin  1 Credit  / 1 Coin  1  Credit "	},
	{0x10, 0x01, 0xf0, 0x10, "2 Coins 1 Credit  / 2 Coins 1  Credit "	},
	{0x10, 0x01, 0xf0, 0x20, "2 Coins 1 Credit  / 1 Coin  3  Credits"	},
	{0x10, 0x01, 0xf0, 0x30, "1 Coin  1 Credit  / 1 Coin  2  Credits"	},
	{0x10, 0x01, 0xf0, 0x40, "1 Coin  1 Credit  / 1 Coin  3  Credits"	},
	{0x10, 0x01, 0xf0, 0x50, "1 Coin  1 Credit  / 1 Coin  4  Credits"	},
	{0x10, 0x01, 0xf0, 0x60, "1 Coin  1 Credit  / 1 Coin  5  Credits"	},
	{0x10, 0x01, 0xf0, 0x70, "1 Coin  1 Credit  / 1 Coin  6  Credits"	},
	{0x10, 0x01, 0xf0, 0x80, "1 Coin  2 Credits / 1 Coin  2  Credits"	},
	{0x10, 0x01, 0xf0, 0x90, "1 Coin  2 Credits / 1 Coin  4  Credits"	},
	{0x10, 0x01, 0xf0, 0xa0, "1 Coin  2 Credits / 1 Coin  5  Credits"	},
	{0x10, 0x01, 0xf0, 0xb0, "1 Coin  2 Credits / 1 Coin  10 Credits"	},
	{0x10, 0x01, 0xf0, 0xc0, "1 Coin  2 Credits / 1 Coin  11 Credits"	},
	{0x10, 0x01, 0xf0, 0xd0, "1 Coin  2 Credits / 1 Coin  12 Credits"	},
	{0x10, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits / 1 Coin  6  Credits"	},
	{0x10, 0x01, 0xf0, 0xf0, "Free_Play"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x11, 0x01, 0x01, 0x01, "Off"					},
	{0x11, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x11, 0x01, 0x06, 0x00, "1 (Normal)"			},
	{0x11, 0x01, 0x06, 0x02, "2"					},
	{0x11, 0x01, 0x06, 0x04, "3"					},
	{0x11, 0x01, 0x06, 0x06, "4 (Difficult)"		},

	{0   , 0xfe, 0   ,    2, "Freeze"				},
	{0x11, 0x01, 0x80, 0x00, "Off"					},
	{0x11, 0x01, 0x80, 0x80, "On"					},
};

STDDIPINFO(Ikki)

static void __fastcall markham_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xe00c:
			scroll[0] = data;
		return;

		case 0xe00d:
			scroll[1] = data;
		return;

		case 0xe00e:
			flipscreen = data & 1;
		return;
	}
}

static UINT8 __fastcall markham_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xe000:
			return DrvDips[1];

		case 0xe001:
			return DrvDips[0];

		case 0xe002:
			return DrvInputs[0];

		case 0xe003:
			return DrvInputs[1];

		case 0xe005:
			return (DrvInputs[2] & ~0x10) | (DrvDips[2] & 0x10);
	}

	return 0;
}

static UINT8 banbam_protection_read()
{
	UINT8 init = packet_buffer[0] & 0x0f;
	UINT8 arg = packet_buffer[1] & 0x0f;

	if (packet_reset)
	{
		return 0xa5;
	}
	else if (init == 0x08 || init == 0x05)
	{
		UINT8 comm = packet_buffer[1] & 0xf0;

		switch (comm)
		{
			case 0x30:
				return (DrvProtROM[0x799 + (arg * 4)] & 0xf) | comm;
			
			case 0x40:
				return (DrvProtROM[0x7C5 + (arg * 4)] & 0xf) | comm;

			case 0x60:
				return (BurnRandom() & 0xf) | comm;
				
			case 0x70:
				return ((arg + 1) & 0xf) | comm;
			
			case 0xb0:
				return ((arg + 3) & 0xf) | comm;
		}
		
		return comm;
	}

	return arg | 0xf0;
}

static void banbam_protection_write(UINT8 data)
{
	packet_buffer[packet_write_pos & 1] = data;
	packet_write_pos ^= 1;
	packet_reset = packet_write_pos;
}

static void __fastcall strnskil_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xd808:
			scroll_control = data >> 5;
			flipscreen = data & 8;
		return;

		case 0xd809:
		return; // nop

		case 0xd80a:
		case 0xd80b:
			scroll[address & 1] = data;
		return;

		case 0xd80d:
			banbam_protection_write(data);
		return;
	}
}

static UINT8 __fastcall strnskil_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xd800:
			return irq_source;

		case 0xd801:
			return DrvDips[0];

		case 0xd802:
			return DrvDips[1];

		case 0xd803:
			return (DrvInputs[2] & ~0x20) | (DrvDips[2] & 0x20);

		case 0xd804:
			return DrvInputs[0];

		case 0xd805:
			return DrvInputs[1];

		case 0xd806:
			return banbam_protection_read();
	}

	return 0;
}

static void __fastcall ikki_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xe008:
			flipscreen = data & 4;
		return;

		case 0xe009:
		return; // coin counter

		case 0xe00a:
		case 0xe00b:
			scroll[address & 1] = data;
		return;
	}
}

static UINT8 __fastcall ikki_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xe000:
			return irq_source << 1;

		case 0xe001:
			return DrvDips[0];

		case 0xe002:
			return DrvDips[1];

		case 0xe003:
			return DrvInputs[2];

		case 0xe004:
			return DrvInputs[0];

		case 0xe005:
			return DrvInputs[1];
	}

	return 0;
}

static void __fastcall markham_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xc000:
			SN76496Write(0, data);
		return;

		case 0xc001:
			SN76496Write(1, data);
		return;
	}
}

static void __fastcall strnskil_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xd801:
			SN76496Write(0, data);
		return;

		case 0xd802:
			SN76496Write(1, data);
		return;
	}
}

static tilemap_callback( markham )
{
	INT32 attr  = DrvVidRAM[offs * 2 + 0];

	TILE_SET_INFO(0, DrvVidRAM[offs * 2 + 1] | ((attr & 0x60) << 3), (attr & 0x1f) | ((attr & 0x80) >> 2), 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetReset(0);
	ZetReset(1);

	BurnRandomSetSeed(0x0132013201320132ull);

	irq_source = 0;
	flipscreen = 0;
	scroll_control = 0;

	memset (packet_buffer, 0, 2);
	packet_reset = 0;
	packet_write_pos = 0;
	
	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x00a000;
	DrvZ80ROM1		= Next; Next += 0x006000;

	DrvGfxROM0		= Next; Next += 0x020000;
	DrvGfxROM1		= Next; Next += 0x020000;

	DrvColPROM		= Next; Next += 0x000700;
	DrvVidPROM		= Next; Next += 0x000100;
	
	DrvProtROM		= Next; Next += 0x002000;

	DrvTransTab		= Next; Next += 0x000400;

	DrvPalette		= (UINT32*)Next; Next += 0x0400 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM0		= Next; Next += 0x000800;
	DrvVidRAM		= Next; Next += 0x000800;
	DrvSprRAM		= Next; Next += 0x000800;
	DrvShareRAM		= Next; Next += 0x000800;

	scroll			= Next; Next += 0x000002;

	RamEnd			= Next;
	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode(INT32 type)
{
	INT32 Plane[3]  = { 0x00000, 0x20000, 0x40000 };
	INT32 XOffs[16] = { STEP8(7,-1), STEP8(128+7,-1) };
	INT32 YOffs[32] = { STEP16(0,8), STEP16(256, 8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0xc000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0xc000);

	if (type == 0) {
		GfxDecode(0x0200, 3, 16, 16, Plane, XOffs, YOffs, 0x100, tmp, DrvGfxROM0);
	} else {
		GfxDecode(0x0100, 3, 16, 32, Plane, XOffs, YOffs, 0x200, tmp, DrvGfxROM0);
	}

	memcpy (tmp, DrvGfxROM1, 0xc000);

	GfxDecode(0x0800, 3,  8,  8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM1);

	BurnFree (tmp);

	return 0;
}

static INT32 CommonInit(INT32 game)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (game == 0) // strnskil / guiness
		{
			if (BurnLoadRom(DrvZ80ROM0 + 0x0000,  0, 1)) return 1;
			memcpy (DrvZ80ROM0 + 0x8000, DrvZ80ROM0 + 0x2000, 0x2000);
			if (BurnLoadRom(DrvZ80ROM0 + 0x2000,  1, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0 + 0x4000,  2, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0 + 0x6000,  3, 1)) return 1;

			if (BurnLoadRom(DrvZ80ROM1 + 0x0000,  4, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM1 + 0x2000,  5, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM0 + 0x8000,  6, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x4000,  7, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0000,  8, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM1 + 0x0000,  9, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x4000, 10, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x8000, 11, 1)) return 1;

			if (BurnLoadRom(DrvColPROM + 0x0000, 12, 1)) return 1;
			if (BurnLoadRom(DrvColPROM + 0x0100, 13, 1)) return 1;
			if (BurnLoadRom(DrvColPROM + 0x0200, 14, 1)) return 1;
			if (BurnLoadRom(DrvColPROM + 0x0300, 15, 1)) return 1;
			if (BurnLoadRom(DrvColPROM + 0x0500, 16, 1)) return 1;
			
			if (BurnLoadRom(DrvVidPROM + 0x0000, 17, 1)) return 1;
		}
		else if (game == 1) // pettanp / banbam
		{
			if (BurnLoadRom(DrvZ80ROM0 + 0x0000,  0, 1)) return 1;
			memcpy (DrvZ80ROM0 + 0x8000, DrvZ80ROM0 + 0x2000, 0x2000);
			if (BurnLoadRom(DrvZ80ROM0 + 0x2000,  1, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0 + 0x4000,  2, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0 + 0x6000,  3, 1)) return 1;

			if (BurnLoadRom(DrvZ80ROM1 + 0x0000,  4, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM0 + 0x8000,  5, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x4000,  6, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0000,  7, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM1 + 0x0000,  8, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x4000,  9, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x8000, 10, 1)) return 1;

			if (BurnLoadRom(DrvColPROM + 0x0000, 11, 1)) return 1;
			if (BurnLoadRom(DrvColPROM + 0x0100, 12, 1)) return 1;
			if (BurnLoadRom(DrvColPROM + 0x0200, 13, 1)) return 1;
			if (BurnLoadRom(DrvColPROM + 0x0300, 14, 1)) return 1;
			if (BurnLoadRom(DrvColPROM + 0x0500, 15, 1)) return 1;
			
			if (BurnLoadRom(DrvVidPROM + 0x0000, 16, 1)) return 1;
			
			if (BurnLoadRom(DrvProtROM + 0x0000, 17, 1)) return 1;
		}

		DrvGfxDecode(0);
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0x9fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,		0xc000, 0xc7ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,		0xc800, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,			0xd000, 0xd7ff, MAP_RAM);
	ZetSetWriteHandler(strnskil_main_write);
	ZetSetReadHandler(strnskil_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,		0x0000, 0x5fff, MAP_ROM);
	ZetMapMemory(DrvSprRAM,			0xc000, 0xc7ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,		0xc800, 0xcfff, MAP_RAM);
	ZetSetWriteHandler(strnskil_sound_write);
	ZetClose();
	
	SN76489Init(0, 15468000 / 6, 0);
	SN76489Init(1, 15468000 / 6, 1);
	SN76496SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);
	SN76496SetRoute(1, 1.00, BURN_SND_ROUTE_BOTH);
	SN76496SetBuffered(ZetTotalCycles, 4000000);

	GenericTilesInit();
	GenericTilemapInit(0, scan_cols_map_scan, markham_map_callback, 8, 8, 32, 32);
	GenericTilemapSetOffsets(0, -8, -16);
	GenericTilemapSetScrollRows(0, 32);
	GenericTilemapSetGfx(0, DrvGfxROM1, 3, 8, 8, 0x10000, 0x200, 0x3f);

	irq_scanline[0] = 96;
	irq_scanline[1] = 240;
	irq_scanline[2] = 120;
	irq_scanline[3] = 240;

	DrvDoReset();

	return 0;
}

static INT32 IkkiInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x0000,  0, 1)) return 1;
		memcpy (DrvZ80ROM0 + 0x8000, DrvZ80ROM0 + 0x2000, 0x2000);
		if (BurnLoadRom(DrvZ80ROM0 + 0x2000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x4000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x6000,  3, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x0000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x8000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x4000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0000,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x8000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x4000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x0000, 10, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x0000, 11, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0100, 12, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0200, 13, 1)) return 1;
		if (BurnLoadRomExt(DrvColPROM + 0x0300, 14, 1, LD_INVERT)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0500, 15, 1)) return 1;

		if (BurnLoadRom(DrvVidPROM + 0x0000, 16, 1)) return 1;

		DrvGfxDecode(1);
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0x9fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,		0xc000, 0xc7ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,		0xc800, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,			0xd000, 0xd7ff, MAP_RAM);
	ZetSetWriteHandler(ikki_main_write);
	ZetSetReadHandler(ikki_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,		0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvSprRAM,			0xc000, 0xc7ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,		0xc800, 0xcfff, MAP_RAM);
	ZetSetWriteHandler(strnskil_sound_write);
	ZetClose();
	
	SN76496Init(0, 8000000 / 4, 0);
	SN76496Init(1, 8000000 / 2, 1);
	SN76496SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);
	SN76496SetRoute(1, 1.00, BURN_SND_ROUTE_BOTH);
	SN76496SetBuffered(ZetTotalCycles, 4000000);

	GenericTilesInit();

	irq_scanline[0] = 120;
	irq_scanline[1] = 240;
	irq_scanline[2] = 12;
	irq_scanline[3] = 120;

	DrvDoReset();

	return 0;
}

static INT32 MarkhamInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x02000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x04000,  2, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  3, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM1 + 0x04000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x04000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x08000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x04000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x08000, 10, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000, 11, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00100, 12, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00200, 13, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00300, 14, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00500, 15, 1)) return 1;

		DrvGfxDecode(0);
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0x9fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,		0xc000, 0xc7ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0xc800, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,			0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,		0xd800, 0xdfff, MAP_RAM);
	ZetSetWriteHandler(markham_main_write);
	ZetSetReadHandler(markham_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,		0x0000, 0x5fff, MAP_ROM);
	ZetMapMemory(DrvShareRAM,		0x8000, 0x87ff, MAP_RAM);
	ZetSetWriteHandler(markham_sound_write);
	ZetClose();

	SN76496Init(0, 4000000, 0);
	SN76496Init(1, 4000000, 1);
	SN76496SetRoute(0, 0.75, BURN_SND_ROUTE_BOTH);
	SN76496SetRoute(1, 0.75, BURN_SND_ROUTE_BOTH);
	SN76496SetBuffered(ZetTotalCycles, 4000000);

	GenericTilesInit();
	GenericTilemapInit(0, scan_cols_map_scan, markham_map_callback, 8, 8, 32, 32);
	GenericTilemapSetOffsets(0, -8, -16);
	GenericTilemapSetScrollRows(0, 32);
	GenericTilemapSetGfx(0, DrvGfxROM1, 3, 8, 8, 0x10000, 0x200, 0x3f);

	irq_scanline[0] = 240;
	irq_scanline[1] = -1;
	irq_scanline[2] = 240;
	irq_scanline[3] = -1;
	
	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	ZetExit();
	SN76496Exit();

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x400; i++)
	{
		INT32 j = DrvColPROM[i + 0x300];
		INT32 r = DrvColPROM[j + 0x000] & 0x0f;
		INT32 g = DrvColPROM[j + 0x100] & 0x0f;
		INT32 b = DrvColPROM[j + 0x200] & 0x0f;

		DrvPalette[i] = BurnHighCol(r*16+r, g*16+g, b*16+b, 0);

		DrvTransTab[i] = (j == 0) ? 0 : 1;
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0x60; offs < 0x100; offs += 4)
	{
		INT32 code  = DrvSprRAM[offs + 1];
		INT32 color = DrvSprRAM[offs + 2];

		INT32 sx = DrvSprRAM[offs + 3];
		INT32 sy = DrvSprRAM[offs + 0];
		INT32 px, py;

		if (flipscreen == 0)
		{
			px = sx - 2;
			py = 240 - sy;
		}
		else
		{
			px = 240 - sx;
			py = sy;
		}

		px = px & 0xff;

		if (px > 248)
			px = px - 256;

		RenderTileTranstab(pTransDraw, DrvGfxROM0, code, (color & 0x3f) << 3, 0, px - 8, py - 16, flipscreen, flipscreen, 16, 16, DrvTransTab);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	for (INT32 i =  32/8; i < 128/8; i++) GenericTilemapSetScrollRow(0, i, scroll[0]);
	for (INT32 i = 128/8; i < 256/8; i++) GenericTilemapSetScrollRow(0, i, scroll[1]);

//	GenericTilemapSetFlip(0, (flipscreen) ? TMAP_FLIPXY : 0);
	GenericTilemapDraw(0, pTransDraw, -1);

	draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 StrnskilDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	for (INT32 row = 0; row < 32; row++)
	{
		if (scroll_control != 0x07)
		{
			switch (DrvVidPROM[(scroll_control & 7) * 32 + row])
			{
				case 2:
					GenericTilemapSetScrollRow(0, row, -~scroll[1]);
				break;
				case 4:
					GenericTilemapSetScrollRow(0, row, -~scroll[0]);
				break;
			}
		}
	}

	BurnTransferClear();

//	GenericTilemapSetFlip(0, (flipscreen) ? TMAP_FLIPXY : 0);
	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, -1);

	if (nSpriteEnable & 1) draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void ikki_draw_sprites()
{
	for (INT32 offs = 0; offs < 0x800; offs += 4)
	{
		INT32 sy    = DrvSprRAM[offs + 0];
		INT32 color = DrvSprRAM[offs + 2] & 0x3f;
		INT32 code  =(DrvSprRAM[offs + 1] >> 1) | (DrvSprRAM[offs + 2] & 0x80);
		INT32 sx    = DrvSprRAM[offs + 3];

		sy = 224 - sy;
		sx &= 0xff;
		sy &= 0xff;
		if (sx > 248) sx -= 256;
		if (sy > 240) sy -= 256;

		RenderTileTranstab(pTransDraw, DrvGfxROM0, code, color << 3, 0, sx - 8, sy - 16, flipscreen, flipscreen, 16, 32, DrvTransTab);
	}
}

static void ikki_draw_bg_layer(INT32 prio)
{
	for (INT32 offs = 0; offs < 0x800 / 2; offs++)
	{
		INT32 x = (offs >> 5) << 3;
		INT32 y = (offs & 0x1f) << 3;
		
		INT32 d = DrvVidPROM[x >> 3];

		if (d != 0 && d != 0x0d) {
			if (prio) continue;
		}

		INT32 color = DrvVidRAM[offs * 2 + 0];
		INT32 code  = DrvVidRAM[offs * 2 + 1] + ((color & 0xe0) << 3);
		color = (color & 0x1f) | ((color & 0x80) >> 2);

		if (d == 0x02 && prio == 0) {
			x -= scroll[1];
			if (x < 0) x += 176;

			y = (y + ~scroll[0]) & 0xff;
		}

		Render8x8Tile_Clip(pTransDraw, code, x-8, y-16, color, 3, 0x200, DrvGfxROM1);
	}
}

static INT32 IkkiDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	ikki_draw_bg_layer(0);
	ikki_draw_sprites();
	ikki_draw_bg_layer(1);

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
		memset (DrvInputs, 0, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		if (DrvJoy1[2] && DrvJoy1[3]) DrvInputs[0] &= ~0x0c;
		if (DrvJoy1[1] && DrvJoy1[0]) DrvInputs[0] &= ~0x03;
		if (DrvJoy2[3] && DrvJoy2[2]) DrvInputs[1] &= ~0x0c;
		if (DrvJoy2[1] && DrvJoy2[0]) DrvInputs[1] &= ~0x03;
	}

	INT32 nInterleave = 262*16;
	INT32 nCyclesTotal[2] = { 4000000 / 60, 4000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		CPU_RUN(0, Zet);
		if (i == irq_scanline[0]*16) { ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD); irq_source = 1; };
		if (i == irq_scanline[1]*16) { ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD); irq_source = 0; };
		ZetClose();

		ZetOpen(1);
		CPU_RUN(1, Zet);
		if (i == irq_scanline[2]*16) { ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD); };
		if (i == irq_scanline[3]*16) { ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD); };
		ZetClose();
	}

	if (pBurnSoundOut) {
		SN76496Update(pBurnSoundOut, nBurnSoundLen);
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
		*pnMin = 0x029707;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);
		SN76496Scan(nAction, pnMin);
		BurnRandomScan(nAction);

		SCAN_VAR(flipscreen);
		SCAN_VAR(irq_source);
		SCAN_VAR(scroll_control);
		SCAN_VAR(packet_buffer);
		SCAN_VAR(packet_reset);
		SCAN_VAR(packet_write_pos);
	}

	return 0;
}


// Markham

static struct BurnRomInfo markhamRomDesc[] = {
	{ "tv3.9",				0x2000, 0x59391637, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "tvg4.10",			0x2000, 0x1837bcce, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tvg5.11",			0x2000, 0x651da602, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "tvg1.5",				0x2000, 0xc5299766, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code
	{ "tvg2.6",				0x2000, 0xb216300a, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "tvg6.84",			0x2000, 0xab933ae5, 3 | BRF_GRA },           //  5 Sprites
	{ "tvg7.85",			0x2000, 0xce8edda7, 3 | BRF_GRA },           //  6
	{ "tvg8.86",			0x2000, 0x74d1536a, 3 | BRF_GRA },           //  7

	{ "tvg9.87",			0x2000, 0x42168675, 4 | BRF_GRA },           //  8 Tiles
	{ "tvg10.88",			0x2000, 0xfa9feb67, 4 | BRF_GRA },           //  9
	{ "tvg11.89",			0x2000, 0x71f3dd49, 4 | BRF_GRA },           // 10

	{ "14-3.99",			0x0100, 0x89d09126, 5 | BRF_GRA },           // 11 Color PROMs
	{ "14-4.100",			0x0100, 0xe1cafe6c, 5 | BRF_GRA },           // 12
	{ "14-5.101",			0x0100, 0x2d444fa6, 5 | BRF_GRA },           // 13
	{ "14-1.61",			0x0200, 0x3ad8306d, 5 | BRF_GRA },           // 14
	{ "14-2.115",			0x0200, 0x12a4f1ff, 5 | BRF_GRA },           // 15
};

STD_ROM_PICK(markham)
STD_ROM_FN(markham)

struct BurnDriver BurnDrvMarkham = {
	"markham", NULL, NULL, NULL, "1983",
	"Markham\0", NULL, "Sun Electronics", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, markhamRomInfo, markhamRomName, NULL, NULL, NULL, NULL, MarkhamInputInfo, MarkhamDIPInfo,
	MarkhamInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	240, 224, 4, 3
};


// Strength & Skill

static struct BurnRomInfo strnskilRomDesc[] = {
	{ "tvg3.7",				0x4000, 0x31fd793a, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "tvg4.8",				0x2000, 0xc58315b5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tvg5.9",				0x2000, 0x29e7ded5, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "tvg6.10",			0x2000, 0x8b126a4b, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "tvg1.2",				0x2000, 0xb586b753, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code
	{ "tvg2.3",				0x2000, 0x8bd71bb6, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "tvg7.90",			0x2000, 0xee3bd593, 3 | BRF_GRA },           //  6 Sprites
	{ "tvg8.92",			0x2000, 0x1b265360, 3 | BRF_GRA },           //  7
	{ "tvg9.94",			0x2000, 0x776c7ca6, 3 | BRF_GRA },           //  8

	{ "tvg12.102",			0x2000, 0x68b9d888, 4 | BRF_GRA },           //  9 Tiles
	{ "tvg11.101",			0x2000, 0x7f2179ff, 4 | BRF_GRA },           // 10
	{ "tvg10.100",			0x2000, 0x321ad963, 4 | BRF_GRA },           // 11

	{ "15-3.prm",			0x0100, 0xdbcd3bec, 5 | BRF_GRA },           // 12 Color PROMs
	{ "15-4.prm",			0x0100, 0x9eb7b6cf, 5 | BRF_GRA },           // 13
	{ "15-5.prm",			0x0100, 0x9b30a7f3, 5 | BRF_GRA },           // 14
	{ "15-1.prm",			0x0200, 0xd4f5b3d7, 5 | BRF_GRA },           // 15
	{ "15-2.prm",			0x0200, 0xcdffede9, 5 | BRF_GRA },           // 16

	{ "15-6.prm",			0x0100, 0xec4faf5b, 0 | BRF_OPT },           // 17 Scroll Control PROM
};

STD_ROM_PICK(strnskil)
STD_ROM_FN(strnskil)

static INT32 StrnskilInit()
{
	return CommonInit(0);
}

struct BurnDriver BurnDrvStrnskil = {
	"strnskil", NULL, NULL, NULL, "1984",
	"Strength & Skill\0", NULL, "Sun Electronics", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, strnskilRomInfo, strnskilRomName, NULL, NULL, NULL, NULL, StrnskilInputInfo, StrnskilDIPInfo,
	StrnskilInit, DrvExit, DrvFrame, StrnskilDraw, DrvScan, &DrvRecalc, 0x400,
	240, 224, 4, 3
};


// The Guiness (Japan)

static struct BurnRomInfo guinessRomDesc[] = {
	{ "tvg3.15",			0x4000, 0x3a605ad8, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "tvg4.8",				0x2000, 0xc58315b5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tvg5.9",				0x2000, 0x29e7ded5, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "tvg6.10",			0x2000, 0x8b126a4b, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "tvg1.2",				0x2000, 0xb586b753, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code
	{ "tvg2.3",				0x2000, 0x8bd71bb6, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "tvg7.90",			0x2000, 0xee3bd593, 3 | BRF_GRA },           //  6 Sprites
	{ "tvg8.92",			0x2000, 0x1b265360, 3 | BRF_GRA },           //  7
	{ "tvg9.94",			0x2000, 0x776c7ca6, 3 | BRF_GRA },           //  8

	{ "=tvg12.15",			0x2000, 0xa82c923d, 4 | BRF_GRA },           //  9 Tiles
	{ "tvg11.15",			0x2000, 0xd432c96f, 4 | BRF_GRA },           // 10
	{ "tvg10.15",			0x2000, 0xa53959d6, 4 | BRF_GRA },           // 11

	{ "15-3.prm",			0x0100, 0xdbcd3bec, 5 | BRF_GRA },           // 12 Color PROMs
	{ "15-4.prm",			0x0100, 0x9eb7b6cf, 5 | BRF_GRA },           // 13
	{ "15-5.prm",			0x0100, 0x9b30a7f3, 5 | BRF_GRA },           // 14
	{ "15-1.prm",			0x0200, 0xd4f5b3d7, 5 | BRF_GRA },           // 15
	{ "15-2.prm",			0x0200, 0xcdffede9, 5 | BRF_GRA },           // 16

	{ "15-6.prm",			0x0100, 0xec4faf5b, 0 | BRF_OPT },           // 17 Scroll Control PROM
};

STD_ROM_PICK(guiness)
STD_ROM_FN(guiness)

struct BurnDriver BurnDrvGuiness = {
	"guiness", "strnskil", NULL, NULL, "1984",
	"The Guiness (Japan)\0", NULL, "Sun Electronics", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, guinessRomInfo, guinessRomName, NULL, NULL, NULL, NULL, StrnskilInputInfo, StrnskilDIPInfo,
	StrnskilInit, DrvExit, DrvFrame, StrnskilDraw, DrvScan, &DrvRecalc, 0x400,
	240, 224, 4, 3
};


// BanBam

static struct BurnRomInfo banbamRomDesc[] = {
	{ "ban-rom2.ic7",		0x4000, 0xa5aeef6e, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "ban-rom3.ic8",		0x2000, 0xf91472bf, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ban-rom4.ic9",		0x2000, 0x436a09ef, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ban-rom5.ic10",		0x2000, 0x45205f86, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "ban-rom1.ic2",		0x2000, 0xe36009f6, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code

	{ "ban-rom6.ic90",		0x2000, 0x41fc44df, 3 | BRF_GRA },           //  5 Sprites
	{ "ban-rom7.ic92",		0x2000, 0x8b429c5b, 3 | BRF_GRA },           //  6
	{ "ban-rom8.ic94",		0x2000, 0x76c02d6b, 3 | BRF_GRA },           //  7

	{ "ban-rom11.ic102",	0x2000, 0xaa827c57, 4 | BRF_GRA },           //  8 Tiles
	{ "ban-rom10.ic101",	0x2000, 0x51bd1c5c, 4 | BRF_GRA },           //  9
	{ "ban-rom9.ic100",		0x2000, 0xc0a5a4c8, 4 | BRF_GRA },           // 10

	{ "16-3.66",			0x0100, 0xdbcd3bec, 5 | BRF_GRA },           // 11 Color PROMs
	{ "16-4.67",			0x0100, 0x9eb7b6cf, 5 | BRF_GRA },           // 12
	{ "16-5.68",			0x0100, 0x9b30a7f3, 5 | BRF_GRA },           // 13
	{ "16-1.148",			0x0200, 0x777e2770, 5 | BRF_GRA },           // 14
	{ "16-2.97",			0x0200, 0x7f95d4b2, 5 | BRF_GRA },           // 15

	{ "16-6.59",			0x0100, 0xec4faf5b, 6 | BRF_GRA },           // 16 Scroll Control PROM
	
	{ "ban-rom12.ic2",		0x2000, 0x044bb2f6, 7 | BRF_GRA },           // 17 MB8841 External ROM

	{ "sun-8212.ic3",		0x0800, 0x8869611e, 8 | BRF_PRG }, // 18 MB8841 Internal ROM 
};

STD_ROM_PICK(banbam)
STD_ROM_FN(banbam)

static INT32 BanbamInit()
{
	return CommonInit(1);
}

struct BurnDriver BurnDrvBanbam = {
	"banbam", NULL, NULL, NULL, "1984",
	"BanBam\0", NULL, "Sun Electronics", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, banbamRomInfo, banbamRomName, NULL, NULL, NULL, NULL, BanbamInputInfo, BanbamDIPInfo,
	BanbamInit, DrvExit, DrvFrame, StrnskilDraw, DrvScan, &DrvRecalc, 0x400,
	240, 224, 4, 3
};


// Pettan Pyuu (Japan)

static struct BurnRomInfo pettanpRomDesc[] = {
	{ "tvg2-16a.7",			0x4000, 0x4cbbbd01, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "tvg3-16a.8",			0x2000, 0xaaa0420f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tvg4-16a.9",			0x2000, 0x43306369, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "tvg5-16a.10",		0x2000, 0xda9c635f, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "tvg1-16.2",			0x2000, 0xe36009f6, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code

	{ "tvg6-16.90",			0x2000, 0x6905d9d5, 3 | BRF_GRA },           //  5 Sprites
	{ "tvg7-16.92",			0x2000, 0x40d02bfd, 3 | BRF_GRA },           //  6
	{ "tvg8-16.94",			0x2000, 0xb18a2244, 3 | BRF_GRA },           //  7

	{ "tvg11-16.102",		0x2000, 0x327b7a29, 4 | BRF_GRA },           //  8 Tile
	{ "tvg10-16.101",		0x2000, 0x624ac061, 4 | BRF_GRA },           //  9
	{ "tvg9-16.100",		0x2000, 0xc477e74c, 4 | BRF_GRA },           // 10

	{ "16-3.66",			0x0100, 0xdbcd3bec, 5 | BRF_GRA },           // 11 Color PROMs
	{ "16-4.67",			0x0100, 0x9eb7b6cf, 5 | BRF_GRA },           // 12
	{ "16-5.68",			0x0100, 0x9b30a7f3, 5 | BRF_GRA },           // 13
	{ "16-1.148",			0x0200, 0x777e2770, 5 | BRF_GRA },           // 14
	{ "16-2.97",			0x0200, 0x7f95d4b2, 5 | BRF_GRA },           // 15

	{ "16-6.59",			0x0100, 0xec4faf5b, 0 | BRF_OPT },           // 16 Scroll Control PROM

	{ "tvg12-16.2",			0x1000, 0x3abc6ba8, 0 | BRF_OPT },           // 17 MB8841 External ROM

	{ "sun-8212.ic3",		0x0800,	0x00000000, 0 | BRF_NODUMP | BRF_PRG }, // 18 MB8841 Internal ROM 
};

STD_ROM_PICK(pettanp)
STD_ROM_FN(pettanp)

struct BurnDriver BurnDrvPettanp = {
	"pettanp", "banbam", NULL, NULL, "1984",
	"Pettan Pyuu (Japan)\0", NULL, "Sun Electronics", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, pettanpRomInfo, pettanpRomName, NULL, NULL, NULL, NULL, BanbamInputInfo, BanbamDIPInfo,
	BanbamInit, DrvExit, DrvFrame, StrnskilDraw, DrvScan, &DrvRecalc, 0x400,
	240, 224, 4, 3
};


// Ikki (Japan)

static struct BurnRomInfo ikkiRomDesc[] = {
	{ "tvg17_1",			0x4000, 0xcb28167c, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "tvg17_2",			0x2000, 0x756c7450, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tvg17_3",			0x2000, 0x91f0a8b6, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "tvg17_4",			0x2000, 0x696fcf7d, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "tvg17_5",			0x2000, 0x22bdb40e, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code

	{ "tvg17_6",			0x4000, 0xdc8aa269, 3 | BRF_GRA },           //  5 Sprites
	{ "tvg17_7",			0x4000, 0x0e9efeba, 3 | BRF_GRA },           //  6
	{ "tvg17_8",			0x4000, 0x45c9087a, 3 | BRF_GRA },           //  7

	{ "tvg17_9",			0x4000, 0xc594f3c5, 4 | BRF_GRA },           //  8 Tiles
	{ "tvg17_10",			0x4000, 0x2e510b4e, 4 | BRF_GRA },           //  9
	{ "tvg17_11",			0x4000, 0x35012775, 4 | BRF_GRA },           // 10

	{ "prom17_3",			0x0100, 0xdbcd3bec, 5 | BRF_GRA },           // 11 Color PROMs
	{ "prom17_4",			0x0100, 0x9eb7b6cf, 5 | BRF_GRA },           // 12
	{ "prom17_5",			0x0100, 0x9b30a7f3, 5 | BRF_GRA },           // 13
	{ "prom17_6",			0x0200, 0x962e619d, 5 | BRF_GRA },           // 14
	{ "prom17_7",			0x0200, 0xb1f5148c, 5 | BRF_GRA },           // 15

	{ "prom17_1",			0x0100, 0xca0af30c, 6 | BRF_GRA },           // 16 Video PROM

	{ "prom17_2",			0x0100, 0xf3c55174, 6 | BRF_OPT },           // 17 Unused PROMs
};

STD_ROM_PICK(ikki)
STD_ROM_FN(ikki)

struct BurnDriver BurnDrvIkki = {
	"ikki", NULL, NULL, NULL, "1985",
	"Ikki (Japan)\0", NULL, "Sun Electronics", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_SCRFIGHT, 0,
	NULL, ikkiRomInfo, ikkiRomName, NULL, NULL, NULL, NULL, IkkiInputInfo, IkkiDIPInfo,
	IkkiInit, DrvExit, DrvFrame, IkkiDraw, DrvScan, &DrvRecalc, 0x400,
	240, 224, 4, 3
};


// Farmers Rebellion

static struct BurnRomInfo farmerRomDesc[] = {
	{ "tvg-1.10",			0x4000, 0x2c0bd392, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "tvg-2.9",			0x2000, 0xb86efe02, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tvg-3.8",			0x2000, 0xfd686ff4, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "tvg-4.7",			0x2000, 0x1415355d, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "tvg-5.30",			0x2000, 0x22bdb40e, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code

	{ "tvg-6.104",			0x4000, 0xdc8aa269, 3 | BRF_GRA },           //  5 Sprites
	{ "tvg-7.103",			0x4000, 0x0e9efeba, 3 | BRF_GRA },           //  6
	{ "tvg-8.102",			0x4000, 0x45c9087a, 3 | BRF_GRA },           //  7

	{ "tvg17_9",			0x4000, 0xc594f3c5, 4 | BRF_GRA },           //  8 Tiles
	{ "tvg17_10",			0x4000, 0x2e510b4e, 4 | BRF_GRA },           //  9
	{ "tvg17_11",			0x4000, 0x35012775, 4 | BRF_GRA },           // 10

	{ "prom17_3",			0x0100, 0xdbcd3bec, 5 | BRF_GRA },           // 11 Color PROMs
	{ "prom17_4",			0x0100, 0x9eb7b6cf, 5 | BRF_GRA },           // 12
	{ "prom17_5",			0x0100, 0x9b30a7f3, 5 | BRF_GRA },           // 13
	{ "prom17_6",			0x0200, 0x962e619d, 5 | BRF_GRA },           // 14
	{ "prom17_7",			0x0200, 0xb1f5148c, 5 | BRF_GRA },           // 15

	{ "prom17_1",			0x0100, 0xca0af30c, 6 | BRF_GRA },           // 16 Video PROM

	{ "prom17_2",			0x0100, 0xf3c55174, 6 | BRF_OPT },           // 17 Unused PROMs
};

STD_ROM_PICK(farmer)
STD_ROM_FN(farmer)

struct BurnDriver BurnDrvFarmer = {
	"farmer", "ikki", NULL, NULL, "1985",
	"Farmers Rebellion\0", NULL, "Sun Electronics", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_SCRFIGHT, 0,
	NULL, farmerRomInfo, farmerRomName, NULL, NULL, NULL, NULL, IkkiInputInfo, IkkiDIPInfo,
	IkkiInit, DrvExit, DrvFrame, IkkiDraw, DrvScan, &DrvRecalc, 0x400,
	240, 224, 4, 3
};
