// FinalBurn Neo Kaneko EXPRO-02 & clone hardware driver module
// Based on MAME driver by Nicola Salmoria

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "msm6295.h"
#include "kaneko_tmap.h"
#include "kaneko_hit.h"
#include "watchdog.h"
#include "bitswap.h"
#include "burn_pal.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvGfxROM[2];
static UINT8 *Drv68KRAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvBgRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprRegs;
static UINT8 *DrvView2RAM;
static UINT8 *DrvView2Regs;

static UINT16 tilebank[2];
static INT32 nOKIBank;

struct tempsprite_t {
	UINT32 code,color;
	INT32 x, y;
	INT32 xoffs, yoffs;
	bool flipx, flipy;
	INT32 priority;
};

static tempsprite_t sprites[1024];

static INT32 scanline;

static INT32 nCyclesExtra;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvDips[2];
static UINT16 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo DrvInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 10,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 8,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 8,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 9,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 10,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 11,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 9,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 8,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 9,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 13,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 14,	"service"	},
	{"Tilt",			BIT_DIGITAL,	DrvJoy3 + 13,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Drv)

static struct BurnInputInfo GalhustlInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 10,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 8,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 8,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 9,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 10,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 14,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 11,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 9,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 8,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 9,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 13,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 14,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 14,	"service"	},
	{"Tilt",			BIT_DIGITAL,	DrvJoy3 + 13,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Galhustl)

static struct BurnDIPInfo Expro02DIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL						},
	{0x14, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x13, 0x01, 0x03, 0x02, "Easy"						},
	{0x13, 0x01, 0x03, 0x03, "Normal"					},
	{0x13, 0x01, 0x03, 0x01, "Hard"						},
	{0x13, 0x01, 0x03, 0x00, "Hardest"					},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x13, 0x01, 0x0c, 0x04, "2"						},
	{0x13, 0x01, 0x0c, 0x0c, "3"						},
	{0x13, 0x01, 0x0c, 0x08, "4"						},
	{0x13, 0x01, 0x0c, 0x00, "5"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x13, 0x01, 0x10, 0x00, "Off"						},
	{0x13, 0x01, 0x10, 0x10, "On"						},

	{0   , 0xfe, 0   ,    2, "Use Button"				},
	{0x13, 0x01, 0x20, 0x00, "No"						},
	{0x13, 0x01, 0x20, 0x20, "Yes"						},

	{0   , 0xfe, 0   ,    2, "Censored Girls"			},
	{0x13, 0x01, 0x40, 0x40, "Off"						},
	{0x13, 0x01, 0x40, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Show Ending Picture"		},
	{0x13, 0x01, 0x80, 0x00, "Off"						},
	{0x13, 0x01, 0x80, 0x80, "On"						},

	{0   , 0xfe, 0   ,    2, "Flip Screen"				},
	{0x14, 0x01, 0x02, 0x02, "Off"						},
	{0x14, 0x01, 0x02, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x14, 0x01, 0x04, 0x04, "Off"						},
	{0x14, 0x01, 0x04, 0x00, "On"						},

	{0   , 0xfe, 0   ,    6, "Coin A"					},
	{0x14, 0x01, 0x38, 0x28, "2 Coins 1 Credits"		},
	{0x14, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x14, 0x01, 0x38, 0x18, "1 Coin  2 Credits"		},
	{0x14, 0x01, 0x38, 0x10, "1 Coin  3 Credits"		},
	{0x14, 0x01, 0x38, 0x00, "1 Coin  4 Credits"		},
	{0x14, 0x01, 0x38, 0x08, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x14, 0x01, 0xc0, 0x80, "2 Coins 1 Credits"		},
	{0x14, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"		},
	{0x14, 0x01, 0xc0, 0x40, "1 Coin  2 Credits"		},
	{0x14, 0x01, 0xc0, 0x00, "1 Coin  6 Credits"		},
};

STDDIPINFO(Expro02)

static struct BurnDIPInfo GalsnewDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL						},
	{0x14, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x13, 0x01, 0x03, 0x02, "Easy"						},
	{0x13, 0x01, 0x03, 0x03, "Normal"					},
	{0x13, 0x01, 0x03, 0x01, "Hard"						},
	{0x13, 0x01, 0x03, 0x00, "Hardest"					},

	{0   , 0xfe, 0   ,    2, "Unknown"					},
	{0x13, 0x01, 0x04, 0x04, "Off"						},
	{0x13, 0x01, 0x04, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Unknown"					},
	{0x13, 0x01, 0x08, 0x08, "Off"						},
	{0x13, 0x01, 0x08, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x13, 0x01, 0x30, 0x10, "2"						},
	{0x13, 0x01, 0x30, 0x30, "3"						},
	{0x13, 0x01, 0x30, 0x20, "4"						},
	{0x13, 0x01, 0x30, 0x00, "5"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x13, 0x01, 0x40, 0x40, "Off"						},
	{0x13, 0x01, 0x40, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Show Ending Picture"		},
	{0x13, 0x01, 0x80, 0x00, "Off"						},
	{0x13, 0x01, 0x80, 0x80, "On"						},

	{0   , 0xfe, 0   ,    2, "Flip Screen"				},
	{0x14, 0x01, 0x02, 0x02, "Off"						},
	{0x14, 0x01, 0x02, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x14, 0x01, 0x04, 0x04, "Off"						},
	{0x14, 0x01, 0x04, 0x00, "On"						},

	{0   , 0xfe, 0   ,    6, "Coin A"					},
	{0x14, 0x01, 0x38, 0x28, "2 Coins 1 Credits"		},
	{0x14, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x14, 0x01, 0x38, 0x18, "1 Coin  2 Credits"		},
	{0x14, 0x01, 0x38, 0x10, "1 Coin  3 Credits"		},
	{0x14, 0x01, 0x38, 0x00, "1 Coin  4 Credits"		},
	{0x14, 0x01, 0x38, 0x08, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x14, 0x01, 0xc0, 0x80, "2 Coins 1 Credits"		},
	{0x14, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"		},
	{0x14, 0x01, 0xc0, 0x40, "1 Coin  2 Credits"		},
	{0x14, 0x01, 0xc0, 0x00, "1 Coin  6 Credits"		},
};

STDDIPINFO(Galsnew)

static struct BurnDIPInfo GalsnewjDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL						},
	{0x14, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x13, 0x01, 0x03, 0x02, "Easy"						},
	{0x13, 0x01, 0x03, 0x03, "Normal"					},
	{0x13, 0x01, 0x03, 0x01, "Hard"						},
	{0x13, 0x01, 0x03, 0x00, "Hardest"					},

	{0   , 0xfe, 0   ,    2, "Unknown"					},
	{0x13, 0x01, 0x04, 0x04, "Off"						},
	{0x13, 0x01, 0x04, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Unknown"					},
	{0x13, 0x01, 0x08, 0x08, "Off"						},
	{0x13, 0x01, 0x08, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x13, 0x01, 0x30, 0x10, "2"						},
	{0x13, 0x01, 0x30, 0x30, "3"						},
	{0x13, 0x01, 0x30, 0x20, "4"						},
	{0x13, 0x01, 0x30, 0x00, "5"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x13, 0x01, 0x40, 0x00, "Off"						},
	{0x13, 0x01, 0x40, 0x40, "On"						},

	{0   , 0xfe, 0   ,    2, "Unknown"					},
	{0x13, 0x01, 0x80, 0x00, "Off"						},
	{0x13, 0x01, 0x80, 0x80, "On"						},

	{0   , 0xfe, 0   ,    2, "Flip Screen"				},
	{0x14, 0x01, 0x02, 0x02, "Off"						},
	{0x14, 0x01, 0x02, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x14, 0x01, 0x04, 0x04, "Off"						},
	{0x14, 0x01, 0x04, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Unknown"					},
	{0x14, 0x01, 0x08, 0x00, "Off"						},
	{0x14, 0x01, 0x08, 0x08, "On"						},

	{0   , 0xfe, 0   ,    4, "Coin A"					},
	{0x14, 0x01, 0x30, 0x10, "2 Coins 1 Credit"			},
	{0x14, 0x01, 0x30, 0x30, "1 Coin  1 Credit"			},
	{0x14, 0x01, 0x30, 0x00, "2 Coins 3 Credits"		},
	{0x14, 0x01, 0x30, 0x20, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"					},
	{0x14, 0x01, 0xc0, 0x40, "2 Coins 1 Credit"			},
	{0x14, 0x01, 0xc0, 0xc0, "1 Coin  1 Credit"			},
	{0x14, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"		},
	{0x14, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"		},
};

STDDIPINFO(Galsnewj)

static struct BurnDIPInfo FantasiaDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL						},
	{0x14, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x13, 0x01, 0x03, 0x02, "Easy"						},
	{0x13, 0x01, 0x03, 0x03, "Normal"					},
	{0x13, 0x01, 0x03, 0x01, "Hard"						},
	{0x13, 0x01, 0x03, 0x00, "Hardest"					},

	{0   , 0xfe, 0   ,    2, "Unknown"					},
	{0x13, 0x01, 0x04, 0x04, "Off"						},
	{0x13, 0x01, 0x04, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Unknown"					},
	{0x13, 0x01, 0x08, 0x08, "Off"						},
	{0x13, 0x01, 0x08, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x13, 0x01, 0x30, 0x10, "2"						},
	{0x13, 0x01, 0x30, 0x30, "3"						},
	{0x13, 0x01, 0x30, 0x20, "4"						},
	{0x13, 0x01, 0x30, 0x00, "5"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x13, 0x01, 0x40, 0x40, "Off"						},
	{0x13, 0x01, 0x40, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Unknown"					},
	{0x13, 0x01, 0x80, 0x00, "Off"						},
	{0x13, 0x01, 0x80, 0x80, "On"						},

	{0   , 0xfe, 0   ,    2, "Flip Screen"				},
	{0x14, 0x01, 0x02, 0x02, "Off"						},
	{0x14, 0x01, 0x02, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x14, 0x01, 0x04, 0x04, "Off"						},
	{0x14, 0x01, 0x04, 0x00, "On"						},

	{0   , 0xfe, 0   ,    6, "Coin A"					},
	{0x14, 0x01, 0x38, 0x28, "2 Coins 1 Credits"		},
	{0x14, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x14, 0x01, 0x38, 0x18, "1 Coin  2 Credits"		},
	{0x14, 0x01, 0x38, 0x10, "1 Coin  3 Credits"		},
	{0x14, 0x01, 0x38, 0x00, "1 Coin  4 Credits"		},
	{0x14, 0x01, 0x38, 0x08, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x14, 0x01, 0xc0, 0x80, "2 Coins 1 Credits"		},
	{0x14, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"		},
	{0x14, 0x01, 0xc0, 0x40, "1 Coin  2 Credits"		},
	{0x14, 0x01, 0xc0, 0x00, "1 Coin  6 Credits"		},
};

STDDIPINFO(Fantasia)

static struct BurnDIPInfo FantasiaaDIPList[]=
{
	{0x13, 0xff, 0xff, 0xf7, NULL						},
	{0x14, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x13, 0x01, 0x03, 0x02, "Easy"						},
	{0x13, 0x01, 0x03, 0x03, "Normal"					},
	{0x13, 0x01, 0x03, 0x01, "Hard"						},
	{0x13, 0x01, 0x03, 0x00, "Hardest"					},

	{0   , 0xfe, 0   ,    0, "Lives"					},
	{0x13, 0x01, 0x30, 0x10, "2"						},
	{0x13, 0x01, 0x30, 0x30, "3"						},
	{0x13, 0x01, 0x30, 0x20, "4"						},
	{0x13, 0x01, 0x30, 0x00, "5"						},

	{0   , 0xfe, 0   ,    4, "Demo Sounds"				},
	{0x13, 0x01, 0x40, 0x00, "Off"						},
	{0x13, 0x01, 0x40, 0x40, "On"						},

	{0   , 0xfe, 0   ,    2, "Unknown"					},
	{0x14, 0x01, 0x02, 0x02, "Off"						},
	{0x14, 0x01, 0x02, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x14, 0x01, 0x04, 0x04, "Off"						},
	{0x14, 0x01, 0x04, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Coin Mode"				},
	{0x14, 0x01, 0x08, 0x08, "Mode 1"					},
	{0x14, 0x01, 0x08, 0x00, "Mode 2"					},

	{0   , 0xfe, 0   ,    6, "Coin A"					},
	{0x14, 0x01, 0x30, 0x20, "2 Coins 1 Credits"		},
	{0x14, 0x01, 0x30, 0x30, "1 Coin  1 Credits"		},
	{0x14, 0x01, 0x30, 0x10, "1 Coin  2 Credits"		},
	{0x14, 0x01, 0x30, 0x10, "1 Coin  3 Credits"		},
	{0x14, 0x01, 0x30, 0x00, "1 Coin  4 Credits"		},
	{0x14, 0x01, 0x30, 0x00, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    6, "Coin B"					},
	{0x14, 0x01, 0xc0, 0x80, "2 Coins 1 Credits"		},
	{0x14, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"		},
	{0x14, 0x01, 0xc0, 0x40, "1 Coin  2 Credits"		},
	{0x14, 0x01, 0xc0, 0x40, "1 Coin  3 Credits"		},
	{0x14, 0x01, 0xc0, 0x00, "1 Coin  4 Credits"		},
	{0x14, 0x01, 0xc0, 0x00, "1 Coin  6 Credits"		},
};

STDDIPINFO(Fantasiaa)

static struct BurnDIPInfo Missw96DIPList[]=
{
	{0x13, 0xff, 0xff, 0xf7, NULL						},
	{0x14, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x13, 0x01, 0x03, 0x02, "Easy"						},
	{0x13, 0x01, 0x03, 0x03, "Normal"					},
	{0x13, 0x01, 0x03, 0x01, "Hard"						},
	{0x13, 0x01, 0x03, 0x00, "Hardest"					},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x13, 0x01, 0x30, 0x10, "2"						},
	{0x13, 0x01, 0x30, 0x30, "3"						},
	{0x13, 0x01, 0x30, 0x20, "4"						},
	{0x13, 0x01, 0x30, 0x00, "5"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x13, 0x01, 0x40, 0x00, "Off"						},
	{0x13, 0x01, 0x40, 0x40, "On"						},

	{0   , 0xfe, 0   ,    2, "Unknown"					},
	{0x14, 0x01, 0x02, 0x02, "Off"						},
	{0x14, 0x01, 0x02, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Coin Mode"				},
	{0x14, 0x01, 0x08, 0x08, "Mode 1"					},
	{0x14, 0x01, 0x08, 0x00, "Mode 2"					},

	{0   , 0xfe, 0   ,    6, "Coin A"					},
	{0x14, 0x01, 0x30, 0x20, "2 Coins 1 Credits"		},
	{0x14, 0x01, 0x30, 0x30, "1 Coin  1 Credits"		},
	{0x14, 0x01, 0x30, 0x10, "1 Coin  2 Credits"		},
	{0x14, 0x01, 0x30, 0x10, "1 Coin  3 Credits"		},
	{0x14, 0x01, 0x30, 0x00, "1 Coin  4 Credits"		},
	{0x14, 0x01, 0x30, 0x00, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    6, "Coin B"					},
	{0x14, 0x01, 0xc0, 0x80, "2 Coins 1 Credits"		},
	{0x14, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"		},
	{0x14, 0x01, 0xc0, 0x40, "1 Coin  2 Credits"		},
	{0x14, 0x01, 0xc0, 0x40, "1 Coin  3 Credits"		},
	{0x14, 0x01, 0xc0, 0x00, "1 Coin  4 Credits"		},
	{0x14, 0x01, 0xc0, 0x00, "1 Coin  6 Credits"		},
};

STDDIPINFO(Missw96)

static struct BurnDIPInfo GalhustlDIPList[]=
{
	{0x15, 0xff, 0xff, 0xff, NULL						},
	{0x16, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x15, 0x01, 0x03, 0x00, "6"						},
	{0x15, 0x01, 0x03, 0x01, "7"						},
	{0x15, 0x01, 0x03, 0x03, "8"						},
	{0x15, 0x01, 0x03, 0x02, "10"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x15, 0x01, 0x40, 0x00, "Off"						},
	{0x15, 0x01, 0x40, 0x40, "On"						},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x15, 0x01, 0x80, 0x80, "Off"						},
	{0x15, 0x01, 0x80, 0x00, "On"						},

	{0   , 0xfe, 0   ,    8, "Coinage"					},
	{0x16, 0x01, 0x07, 0x00, "4 Coins 1 Credits"		},
	{0x16, 0x01, 0x07, 0x01, "3 Coins 1 Credits"		},
	{0x16, 0x01, 0x07, 0x02, "2 Coins 1 Credits"		},
	{0x16, 0x01, 0x07, 0x04, "3 Coins 2 Credits"		},
	{0x16, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x16, 0x01, 0x07, 0x03, "2 Coins 3 Credits"		},
	{0x16, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x16, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x16, 0x01, 0x18, 0x10, "Easy"						},
	{0x16, 0x01, 0x18, 0x18, "Normal"					},
	{0x16, 0x01, 0x18, 0x08, "Hard"						},
	{0x16, 0x01, 0x18, 0x00, "Hardest"					},

	{0   , 0xfe, 0   ,    4, "Play Time"				},
	{0x16, 0x01, 0x60, 0x40, "120 Sec"					},
	{0x16, 0x01, 0x60, 0x60, "100 Sec"					},
	{0x16, 0x01, 0x60, 0x20, "80 Sec"					},
	{0x16, 0x01, 0x60, 0x00, "70 Sec"					},
};

STDDIPINFO(Galhustl)

static struct BurnDIPInfo ZipzapDIPList[]=
{
	{0x15, 0xff, 0xff, 0xff, NULL						},
	{0x16, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    2, "Unknown"					},
	{0x15, 0x01, 0x01, 0x01, "Off"						},
	{0x15, 0x01, 0x01, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Additional Obstacles"		},
	{0x15, 0x01, 0x02, 0x02, "Off"						},
	{0x15, 0x01, 0x02, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x15, 0x01, 0x0c, 0x00, "1"						},
	{0x15, 0x01, 0x0c, 0x08, "2"						},
	{0x15, 0x01, 0x0c, 0x0c, "3"						},
	{0x15, 0x01, 0x0c, 0x04, "4"						},

	{0   , 0xfe, 0   ,    2, "Unknown"					},
	{0x15, 0x01, 0x10, 0x10, "Off"						},
	{0x15, 0x01, 0x10, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Unknown"					},
	{0x15, 0x01, 0x20, 0x20, "Off"						},
	{0x15, 0x01, 0x20, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Unknown"					},
	{0x15, 0x01, 0x40, 0x40, "Off"						},
	{0x15, 0x01, 0x40, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Unknown"					},
	{0x15, 0x01, 0x80, 0x80, "Off"						},
	{0x15, 0x01, 0x80, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Coinage"					},
	{0x16, 0x01, 0x03, 0x00, "3 Coins 1 Credits"		},
	{0x16, 0x01, 0x03, 0x01, "2 Coins 1 Credits"		},
	{0x16, 0x01, 0x03, 0x03, "1 Coin  1 Credits"		},
	{0x16, 0x01, 0x03, 0x02, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    2, "Select Player Mode"		},
	{0x16, 0x01, 0x04, 0x00, "Off"						},
	{0x16, 0x01, 0x04, 0x04, "On"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x16, 0x01, 0x08, 0x00, "Off"						},
	{0x16, 0x01, 0x08, 0x08, "On"						},

	{0   , 0xfe, 0   ,    2, "Unknown"					},
	{0x16, 0x01, 0x10, 0x10, "Off"						},
	{0x16, 0x01, 0x10, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Unknown"					},
	{0x16, 0x01, 0x20, 0x20, "Off"						},
	{0x16, 0x01, 0x20, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Level Skip with Button 2"	},
	{0x16, 0x01, 0x40, 0x40, "Off"						},
	{0x16, 0x01, 0x40, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Unknown"					},
	{0x16, 0x01, 0x80, 0x80, "Off"						},
	{0x16, 0x01, 0x80, 0x00, "On"						},
};

STDDIPINFO(Zipzap)

static void set_oki_bank(INT32 data)
{
	nOKIBank = data & 0xf;
	MSM6295SetBank(0, MSM6295ROM + nOKIBank * 0x10000, 0x30000, 0x3ffff);
}

static void __fastcall expro02_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x900000:
			set_oki_bank(data);
		return;

		case 0xa00000:
		return;
	}
}

static void __fastcall expro02_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x900000:
			set_oki_bank(data);
		return;

		case 0xa00000:
		case 0xa00001:
		return;

		case 0xd80001:
		case 0xe80001:
			tilebank[(address >> 20) & 1] = data << 8;
		return;
	}
}

static UINT16 __fastcall expro02_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x800000:
			return (DrvInputs[0] & 0xff00) | (DrvDips[0]);

		case 0x800002:
			return (DrvInputs[1] & 0xff00) | (DrvDips[1]);

		case 0x800004:
			return DrvInputs[2];

		case 0x800006: // comad timer
		case 0x800008:
		case 0x80000a:
		case 0x80000c:
		case 0x80000e:
			return (scanline & 7) << 8;
	}

	return 0;
}

static UINT8 __fastcall expro02_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x800000:
			return DrvInputs[0] >> 8;

		case 0x800001:
			return DrvDips[0];

		case 0x800002:
			return DrvInputs[1] >> 8;

		case 0x800003:
			return DrvDips[1];

		case 0x800004:
			return DrvInputs[2] >> 8;

		case 0x800005:
			return DrvInputs[2] >> 0;

		case 0x800006: // comad timer
		case 0x800007:
		case 0x800008:
		case 0x800009:
		case 0x80000a:
		case 0x80000b:
		case 0x80000c:
		case 0x80000d:
		case 0x80000e:
		case 0x80000f:
			return scanline & 7;
	}

	return 0;
}

static void __fastcall msm6295_write_word(UINT32 /*address*/, UINT16 data)
{
	MSM6295Write(0, data & 0xff);
}

static void __fastcall msm6295_write_byte(UINT32 /*address*/, UINT8 data)
{
	MSM6295Write(0, data);
}

static UINT16 __fastcall msm6295_read_word(UINT32 /*address*/)
{
	return MSM6295Read(0) + (MSM6295Read(0) << 8);
}

static UINT8 __fastcall msm6295_read_byte(UINT32 /*address*/)
{
	return MSM6295Read(0);
}

static UINT16 __fastcall comad_msm6295_read_word(UINT32 /*address*/)
{
	return BurnRandom();
}

static UINT8 __fastcall comad_msm6295_read_byte(UINT32 /*address*/)
{
	return BurnRandom();
}

static tilemap_callback( view2_layer0 )
{
	UINT16 *ram = (UINT16*)(DrvView2RAM + 0x1000);
	INT32 code = ram[offs * 2 + 1];
	INT32 attr = ram[offs * 2 + 0];

	code += tilebank[0];

	TILE_SET_INFO(1, code, (attr >> 2) & 0x3f, TILE_FLIPXY(attr & 3) | TILE_GROUP((attr >> 8) & 7));
}

static tilemap_callback( view2_layer1 )
{
	UINT16 *ram = (UINT16*)(DrvView2RAM + 0x0000);
	INT32 code = ram[offs * 2 + 1];
	INT32 attr = ram[offs * 2 + 0];

	code += tilebank[1];

	TILE_SET_INFO(1, code, (attr >> 2) & 0x3f, TILE_FLIPXY(attr & 3) | TILE_GROUP((attr >> 8) & 7));
}

static INT32 DrvDoReset(INT32 full_reset)
{
	if (full_reset) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	SekOpen(0);
	SekReset();
	SekClose();

	MSM6295Reset(0);
	set_oki_bank(3);

	kaneko_hit_calc_reset();
	BurnWatchdogResetEnable();

	nCyclesExtra = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x500400;

	DrvGfxROM[0]	= Next; Next += 0x400000;
	DrvGfxROM[1]	= Next; Next += 0x400000;

	MSM6295ROM		= Next; Next += 0x100000;

	BurnPalette		= (UINT32*)Next; Next += 0x8800 * sizeof(UINT32);

	AllRam			= Next;

	Drv68KRAM		= Next; Next += 0x030000;
	DrvFgRAM		= Next; Next += 0x020000;
	DrvBgRAM		= Next; Next += 0x020000;
	DrvSprRAM		= Next; Next += 0x001000;
	DrvSprRegs		= Next; Next += 0x000400;
	BurnPalRAM		= Next; Next += 0x001000;

	DrvView2RAM		= Next; Next += 0x004000;
	DrvView2Regs	= Next; Next += 0x000400;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGetRoms()
{
	char* pRomName;
	struct BurnRomInfo ri;
	UINT8 *pLoad[7] = { Drv68KROM, Drv68KROM + 0x100000, Drv68KROM + 0x080000, 0, DrvGfxROM[0], DrvGfxROM[1], MSM6295ROM };
//	UINT8 *pLoads[7] = { Drv68KROM, Drv68KROM + 0x100000, Drv68KROM + 0x080000, 0, DrvGfxROM[0], DrvGfxROM[1], MSM6295ROM };

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++)
	{
		BurnDrvGetRomInfo(&ri, i);

		int type = ((ri.nType >> 2) & 3) + ((ri.nType & BRF_PRG) ? 0 : ((ri.nType & BRF_GRA) ? 4 : 6));

//		bprintf (0, _T("%d, Type: %d, size: %5.5x, offset: %5.5x\n"), i, type, ri.nLen, pLoad[type] - pLoads[type]);

		if ((ri.nType & 3) == 1)
		{
			if (BurnLoadRom(pLoad[type] + 1, i + 0, 2)) return 1;
			if (BurnLoadRom(pLoad[type] + 0, i + 1, 2)) return 1;
			pLoad[type] += ri.nLen * 2;
			i++;
			continue;
		}

		if ((ri.nType & 3) == 2)
		{
			if (BurnLoadRom(pLoad[type], i, 1)) return 1;
			if (ri.nType & BRF_SND) {
				if (ri.nLen == 0x20000) {
					for (INT32 i2 = 0; i2 < 0x80000; i2++) {
						pLoad[type][i2] = pLoad[type][i2 & 0x1ffff];
					}
					pLoad[type] += ri.nLen * 3;
				}
			}
			pLoad[type] += ri.nLen;
			continue;
		}
	}

	return 0;
}

static void DrvGfxDecrypt()
{
	UINT32 *src = (UINT32*)BurnMalloc(0x200000);
	UINT32 *dst = (UINT32*)DrvGfxROM[1];

	memcpy (src, dst, 0x200000);

	for (INT32 x = 0, offset; x < 0x80000; x++)
	{
		offset = BITSWAP24(x, 23, 22, 21, 20, 19, 18, 15, 9, 10, 8, 7, 12, 13, 16, 17, 6, 5, 4, 3, 14, 11, 2, 1, 0) ^ 0x0528f;
		offset = (offset & ~0x1ffff) | ((offset + 0x00043) & 0x001ff) | ((offset - 0x09600) & 0x1fe00);
		offset = BITSWAP24(offset, 23, 22, 21, 20, 19, 18, 9, 10, 17, 4, 11, 12, 3, 15, 16, 14, 13, 8, 7, 6, 5, 2, 1, 0);

		dst[x] = src[offset];
	}

	BurnFree (src);
}

static INT32 DrvGfxDecode()
{
	static INT32 Plane[4]   = { STEP4(0,1) };
	static INT32 XOffs0[16] = { STEP4(12,-4), STEP4(28,-4), STEP4(268,-4),STEP4(284,-4) };
	static INT32 XOffs1[16] = { STEP8(0,4), STEP8(256,4) };
	static INT32 YOffs[16]  = { STEP8(0,32), STEP8(512,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x200000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM[0], 0x200000);

	GfxDecode(0x4000, 4, 16, 16, Plane, XOffs1, YOffs, 0x400, tmp, DrvGfxROM[0]);

	BurnByteswap(DrvGfxROM[1], 0x200000);
	memcpy (tmp, DrvGfxROM[1], 0x200000);

	GfxDecode(0x4000, 4, 16, 16, Plane, XOffs0, YOffs, 0x400, tmp, DrvGfxROM[1]);

	BurnFree (tmp);

	return 0;
}

static void common_sound_map(INT32 nMSMAddress, INT32 nIsComad)
{
	SekMapHandler(1,				nMSMAddress, nMSMAddress + 0x3ff, MAP_READ | MAP_WRITE);
	SekSetWriteWordHandler(1,		msm6295_write_word);
	SekSetWriteByteHandler(1,		msm6295_write_byte);
	SekSetReadWordHandler(1,		nIsComad ? comad_msm6295_read_word : msm6295_read_word);
	SekSetReadByteHandler(1,		nIsComad ? comad_msm6295_read_byte : msm6295_read_byte);
}

static void common_map(INT32 n68KROMLen, INT32 n68KRAMOffset, INT32 n68KRAMMirror, INT32 nMSMAddress, INT32 nIsComad)
{
	SekMapMemory(Drv68KROM,			0x000000, n68KROMLen-1, MAP_ROM);
	SekMapMemory(DrvFgRAM,			0x500000, 0x51ffff, MAP_RAM);
	SekMapMemory(DrvBgRAM,			0x520000, 0x53ffff, MAP_RAM);
	SekMapMemory(DrvView2RAM,		0x580000, 0x583fff, MAP_RAM);
	SekMapMemory(BurnPalRAM,		0x600000, 0x600fff, MAP_RAM);
	SekMapMemory(DrvView2Regs,		0x680000, 0x6803ff, MAP_RAM); // 0-1f
	SekMapMemory(DrvSprRAM,			0x700000, 0x700fff, MAP_RAM);
	SekMapMemory(DrvSprRegs,		0x780000, 0x7803ff, MAP_RAM); // 0-1f

	SekMapMemory(Drv68KRAM,			n68KRAMOffset, n68KRAMOffset + 0xffff, MAP_RAM);
	if (n68KRAMMirror)
		SekMapMemory(Drv68KRAM,		n68KRAMOffset - 	0x80000, n68KRAMOffset + 0xffff - 	0x80000, MAP_RAM);

	common_sound_map(nMSMAddress, nIsComad);

	kaneko_hit_calc_init(2, 0xe00000);
}

static INT32 GalsnewInit()
{
	BurnAllocMemIndex();

	if (DrvGetRoms()) return 1;

	DrvGfxDecrypt();
	DrvGfxDecode();

	SekInit(0, 0x68000);
	SekOpen(0);
	common_map(0x400000, 0xc80000, 0, 0x400000, 0);
	SekSetWriteWordHandler(0, 		expro02_write_word);
	SekSetWriteByteHandler(0, 		expro02_write_byte);
	SekSetReadWordHandler(0, 		expro02_read_word);
	SekSetReadByteHandler(0, 		expro02_read_byte);
	SekClose();

	BurnWatchdogInit(DrvDoReset, 180);

	GenericTilesInit();
	BurnBitmapAllocate(1, nScreenWidth, nScreenHeight, true);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 4, 16, 16, 0x400000, 0x000, 0x3f);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 4, 16, 16, 0x400000, 0x400, 0x3f);
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, view2_layer0_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, view2_layer1_map_callback, 16, 16, 32, 32);
	GenericTilemapBuildSkipTable(0, 1, 0);
	GenericTilemapBuildSkipTable(1, 1, 0);
	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetScrollRows(0, 512);
	GenericTilemapSetScrollRows(1, 512);

	MSM6295Init(0, 2000000 / MSM6295_PIN7_LOW, 0);
	MSM6295SetRoute(0, 0.80, BURN_SND_ROUTE_BOTH);

	DrvDoReset(1);

	return 0;
}

static INT32 FantasiaInit()
{
	INT32 nRet = GalsnewInit();

	if (nRet == 0)
	{
		BurnWatchdogInit(DrvDoReset, -1);
	}

	return nRet;
}

static INT32 ComadCommonInit(void (*pMap68K)(), INT32 nM6295Frequency)
{
	BurnAllocMemIndex();

	if (DrvGetRoms()) return 1;

	DrvGfxDecode();

	pMap68K();

	BurnWatchdogInit(DrvDoReset, -1);

	GenericTilesInit();
	BurnBitmapAllocate(1, nScreenWidth, nScreenHeight, true);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 4, 16, 16, 0x400000, 0x100, 0x3f);

	MSM6295Init(0, nM6295Frequency, 0);
	MSM6295SetRoute(0, 0.80, BURN_SND_ROUTE_BOTH);

	DrvDoReset(1);

	return 0;
}

static void FantasianMap()
{
	SekInit(0, 0x68000);
	SekOpen(0);
	common_map(0x500000, 0xc80000, 1, 0xf00000, 0);
	common_sound_map(0xf80000, 0);
	SekSetWriteWordHandler(0, 		expro02_write_word);
	SekSetWriteByteHandler(0, 		expro02_write_byte);
	SekSetReadWordHandler(0, 		expro02_read_word);
	SekSetReadByteHandler(0, 		expro02_read_byte);
	SekClose();
}

static INT32 FantasianInit()
{
	return ComadCommonInit(FantasianMap, 2000000 / 132);
}

static void Fantsia2Map()
{
	SekInit(0, 0x68000);
	SekOpen(0);
	common_map(0x500000, 0xf80000, 0, 0xc80000, 1);
	SekSetWriteWordHandler(0, 		expro02_write_word);
	SekSetWriteByteHandler(0, 		expro02_write_byte);
	SekSetReadWordHandler(0, 		expro02_read_word);
	SekSetReadByteHandler(0, 		expro02_read_byte);
	SekClose();
}

static INT32 Fantsia2Init()
{
	return ComadCommonInit(Fantsia2Map, 2000000 / 132);
}

static void GalhustlMap()
{
	SekInit(0, 0x68000);
	SekOpen(0);
	common_map(0x300000, 0xe80000, 0, 0xd00000, 0);
	SekSetWriteWordHandler(0, 		expro02_write_word);
	SekSetWriteByteHandler(0, 		expro02_write_byte);
	SekSetReadWordHandler(0, 		expro02_read_word);
	SekSetReadByteHandler(0, 		expro02_read_byte);
	SekClose();
}

static INT32 GalhustlInit()
{
	return ComadCommonInit(GalhustlMap, 1056000 / MSM6295_PIN7_HIGH);
}

static void ZipzapMap()
{
	SekInit(0, 0x68000);
	SekOpen(0);
	common_map(0x500000, 0xc80000, 0, 0xc00000, 1);
	SekMapMemory(Drv68KRAM + 0x010000, 0x701000, 0x71ffff, MAP_RAM); // extra ram
	SekSetWriteWordHandler(0, 		expro02_write_word);
	SekSetWriteByteHandler(0, 		expro02_write_byte);
	SekSetReadWordHandler(0, 		expro02_read_word);
	SekSetReadByteHandler(0, 		expro02_read_byte);
	SekClose();
}

static INT32 ZipzapInit()
{
	return ComadCommonInit(ZipzapMap, 1056000 / MSM6295_PIN7_HIGH);
}

static void SupmodelMap()
{
	SekInit(0, 0x68000);
	SekOpen(0);
	common_map(0x500000, 0xc80000, 0, 0xf80000, 1);
	SekSetWriteWordHandler(0, 		expro02_write_word);
	SekSetWriteByteHandler(0, 		expro02_write_byte);
	SekSetReadWordHandler(0, 		expro02_read_word);
	SekSetReadByteHandler(0, 		expro02_read_byte);
	SekClose();
}

static INT32 SupmodelInit()
{
	return ComadCommonInit(SupmodelMap, 1584000 / MSM6295_PIN7_HIGH);
}

static void SmisswMap()
{
	SekInit(0, 0x68000);
	SekOpen(0);
	common_map(0x500000, 0xc00000, 0, 0xf00000, 1);
	SekSetWriteWordHandler(0, 		expro02_write_word);
	SekSetWriteByteHandler(0, 		expro02_write_byte);
	SekSetReadWordHandler(0, 		expro02_read_word);
	SekSetReadByteHandler(0, 		expro02_read_byte);
	SekClose();
}

static INT32 SmisswInit()
{
	return ComadCommonInit(SmisswMap, 2000000 / 132);
}

static INT32 DrvExit()
{
	MSM6295Exit(0);
	MSM6295ROM = NULL;

	SekExit();

	GenericTilesExit();

	BurnFreeMemIndex();

	return 0;
}

static void BuildPalette()
{
	for (INT32 i = 0; i < 0x8000; i++)
	{
		BurnPalette[0x800 + i] = BurnHighCol(pal5bit(i >> 5), pal5bit(i >> 10), pal5bit(i >> 0), 0);
	}
}

static void prepare_layers(bool invert_flip)
{
	UINT16 *regs = (UINT16*)DrvView2Regs;
	UINT16 *ram = (UINT16*)(DrvView2RAM + 0x2000);

	INT32 layer_flip = regs[4];
	if (invert_flip) layer_flip ^= 0x300;

	INT32 x_offsets[2] = { 0x5b, 0x5b };
	INT32 y_offsets[2] = { 8, 8 };

	for (INT32 i = 0; i < 2; i++)
	{
		GenericTilemapSetEnable(i, ~layer_flip & (0x1000 >> (i * 8)));

		GenericTilemapSetFlip(i, ((layer_flip & 0x100) ? TMAP_FLIPY : 0) | ((layer_flip & 0x200) ? TMAP_FLIPX : 0));

		GenericTilemapSetScrollY(i, (regs[(i ? 0 : 2) + 1] >> 6) + y_offsets[i]);

		for (INT32 j = 0; j < 512; j++)
		{
			UINT16 scroll = (layer_flip & (0x800 >> (i * 8))) ? ram[(i ? 0 : 0x0800) + j] : 0;
			GenericTilemapSetScrollRow(i, j, ((regs[i ? 0 : 2] + scroll) >> 6) + x_offsets[i]);
		}
	}
}

static void draw_bitmaps()
{
	UINT16 *bgram = (UINT16*)DrvBgRAM;
	UINT16 *fgram = (UINT16*)DrvFgRAM;
	UINT16 *palram = (UINT16*)BurnPalRAM;
	UINT16 *dst = pTransDraw;

	for (INT32 i = 0; i < 224 * 256; i++)
	{
		INT32 p = fgram[i] & 0x7ff;

		dst[i] = (palram[p] & 1) ? ((bgram[i] >> 1) + 0x800) : p;
	}
}

static void draw_sprites_custom(INT32 gfxno, UINT32 code,UINT32 color,bool flipx,bool flipy,int sx,int sy,int priority)
{
	GenericTilesGfx *gfx = &GenericGfxData[gfxno];

	const UINT32 pen_base = (1<<gfx->depth) * (color & gfx->color_mask);
	UINT8 *source_base = gfx->gfxbase + ((code % gfx->code_mask) * gfx->width * gfx->height);

	int ex = sx+gfx->width;
	int ey = sy+gfx->height;
	int dx = 1, dy = 1;
	int x_index_base = 0;
	int y_index = 0;

	if (flipx)
	{
		x_index_base = gfx->width - 1;
		dx = -1;
	}

	if (flipy)
	{
		y_index = gfx->height - 1;
		dy = -1;
	}

	if (sx < 0)
	{
		int pixels = 0/*clip.min_x*/ - sx;
		sx += pixels;
		x_index_base += pixels * dx;
	}
	if (sy < 0)
	{
		int pixels = 0/*clip.min_y*/ - sy;
		sy += pixels;
		y_index += pixels * dy;
	}

	if (ex > nScreenWidth)
	{
		int pixels = ex-(nScreenWidth - 1)/*clip.max_x*/ - 1;
		ex -= pixels;
	}

	if (ey > nScreenHeight)
	{
		int pixels = ey-(nScreenHeight - 1)/*clip.max_y*/ - 1;
		ey -= pixels;
	}

	if (ex > sx)
	{
		for (int y = sy; y < ey; y++)
		{
			UINT8 *source = source_base + y_index * gfx->width;
			UINT16 *dest = BurnBitmapGetBitmap(1) + y * nScreenWidth;
			UINT8 *pri = BurnBitmapGetPriomap(1) + y * nScreenWidth;

			int x_index = x_index_base;
			for (int x = sx; x < ex; x++)
			{
				const UINT8 c = source[x_index];
				if (c != 0)
				{
					if (pri[x] == 0)
						dest[x] = ((pen_base + c) & 0x3fff) + ((priority & 3) << 14);

					pri[x] = 0xff; // mark it "already drawn"
				}
				x_index += dx;
			}
			y_index += dy;
		}
	}
}

static void draw_sprites()
{
#define USE_LATCHED_XY      1
#define USE_LATCHED_CODE    2
#define USE_LATCHED_COLOR   4

	INT32 m_sprite_xoffs = 0;
	INT32 m_sprite_yoffs = -1;

	UINT16 *regs = (UINT16*)DrvSprRegs;
	UINT16* spriteram16 = (UINT16*)DrvSprRAM;
	INT32 spriteram16_bytes = 0x1000;
	UINT16 new_data = regs[0];
	static INT32 sprite_flipx = new_data & 2;
	static INT32 sprite_flipy = new_data & 1;

	const int max =   0x100 << 6;

	INT32 i = 0;
	struct tempsprite_t *s = &sprites[0];

	int x           =   0;
	int y           =   0;
	int code        =   0;
	int color       =   0;
	int priority    =   0;
	int xoffs       =   0;
	int yoffs       =   0;
	int flipx       =   0;
	int flipy       =   0;

	while (1)
	{
		const int offs = i * 8 / 2;

		if (offs >= (spriteram16_bytes / 2))
			break;

		const UINT16 attr = spriteram16[offs + 0];
		s->code		= spriteram16[offs + 1];
		s->x		= spriteram16[offs + 2];
		s->y		= spriteram16[offs + 3];
		s->flipy	= (attr & 0x0001);
		s->flipx	= (attr & 0x0002);
		s->color	= (attr & 0x00fc) >> 2;
		s->priority	= (attr & 0x0300) >> 8;

		const UINT16 xoffs1 = (attr & 0x1800) >> 11;
		s->yoffs	= regs[0x10 / 2 + xoffs1 * 2 + 1];
		s->xoffs	= regs[0x10 / 2 + xoffs1 * 2 + 0];

		s->yoffs -= regs[0x2/2];

		int flags = ((attr & 0x2000) ? USE_LATCHED_XY : 0)  | ((attr & 0x4000) ? USE_LATCHED_COLOR : 0) | ((attr & 0x8000) ? USE_LATCHED_CODE  : 0) ;

		if (flags == -1)    // End of Sprites
			break;

		if (flags & USE_LATCHED_CODE)
			s->code = ++code;   // Use the latched code + 1 ..
		else
			code = s->code;     // .. or latch this value

		if (flags & USE_LATCHED_COLOR)
		{
			s->color	= color;
			s->priority	= priority;
			s->xoffs	= xoffs;
			s->yoffs	= yoffs;
			s->flipx 	= flipx;
			s->flipy 	= flipy;
		}
		else
		{
			color		= s->color;
			priority	= s->priority;
			xoffs		= s->xoffs;
			yoffs		= s->yoffs;
			flipx		= s->flipx;
			flipy		= s->flipy;
		}

		if (flags & USE_LATCHED_XY)
		{
			s->x += x;
			s->y += y;
		}

		x = s->x;
		y = s->y;

		s->x  = s->xoffs + s->x;
		s->y  = s->yoffs + s->y;

		s->x += m_sprite_xoffs;
		s->y += m_sprite_yoffs;

		if (sprite_flipx) { s->x = max - s->x - (16 << 6); s->flipx = !s->flipx; }
		if (sprite_flipy) { s->y = max - s->y - (16 << 6); s->flipy = !s->flipy; }

		s->x  = ((s->x & 0x7fc0) - (s->x & 0x8000)) / 0x40;
		s->y  = ((s->y & 0x7fc0) - (s->y & 0x8000)) / 0x40;

		i++;
		s++;
	}

	for (s--; s >= sprites; s--)
	{
		draw_sprites_custom(0,s->code,s->color,s->flipx, s->flipy,s->x, s->y,s->priority);
	}
}

static void render_sprites()
{
	UINT16 *regs = (UINT16*)DrvSprRegs;

	BurnBitmapPrimapClear(1);

	if (!(~regs[0] & 0x04))
		BurnBitmapFill(1, 0);

	draw_sprites();
}

static void copybitmap(INT32 *priorities, INT32 colbase)
{
	UINT16 *dstbitmap = pTransDraw;
	UINT8 *dstprimap = BurnBitmapGetPriomap(0);
	UINT16 *srcbitmap = BurnBitmapGetBitmap(1);

	for (int i = 0; i < nScreenHeight * nScreenWidth; i++)
	{
		UINT16 pri = srcbitmap[i] >> 14;

		if (priorities[pri] > dstprimap[i])
		{
			UINT16 pix = srcbitmap[i] & 0x3fff;

			if (pix & 0x3fff)
			{
				dstbitmap[i] = colbase + pix;
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (BurnRecalc) {
		BuildPalette();
		BurnRecalc = 0;
	}

	BurnPaletteUpdate_GGGGGRRRRRBBBBBx();

	BurnTransferClear();

	prepare_layers(false);

	if (nBurnLayer & 1) draw_bitmaps();

	for (INT32 i = 0; i < 8; i++) {
		if (nBurnLayer & 2) GenericTilemapDraw(0, 0, TMAP_SET_GROUP(i));
		if (nBurnLayer & 4) GenericTilemapDraw(1, 0, TMAP_SET_GROUP(i));
	}

	render_sprites();

	INT32 priorities[4] = { 8, 8, 8, 8 };
	copybitmap(priorities, 0x100);

	BurnTransferCopy(BurnPalette);

	return 0;
}

static INT32 ComadDraw()
{
	if (BurnRecalc) {
		BuildPalette();
		BurnRecalc = 0;
	}

	BurnPaletteUpdate_GGGGGRRRRRBBBBBx();

	BurnTransferClear();

	if (nBurnLayer & 1) draw_bitmaps();

	render_sprites();

	INT32 priorities[4] = { 8, 8, 8, 8 };
	copybitmap(priorities, 0x100);

	BurnTransferCopy(BurnPalette);

	return 0;
}

static void bootleg_draw_sprites()
{
	UINT16 *ram = (UINT16*)DrvSprRAM;
	INT32 sx = 0, sy = 0;

	for (INT32 offs = 0; offs < 0x1000 / 2; offs += 4)
	{
		INT32 code  =  ram[offs + 1];
		INT32 color = (ram[offs] & 0x003c) >> 2;
		INT32 flipx =  ram[offs] & 0x0002;
		INT32 flipy =  ram[offs] & 0x0001;

		if ((ram[offs] & 0x6000) == 0x6000)
		{
			sx += ram[offs + 2] >> 6;
			sy += ram[offs + 3] >> 6;
		}
		else
		{
			sx = ram[offs + 2] >> 6;
			sy = ram[offs + 3] >> 6;
		}

		sx = (sx & 0x1ff) - (sx & 0x200);
		sy = (sy & 0x1ff) - (sy & 0x200);

		DrawGfxMaskTile(0, 0, code, sx, sy, flipx, flipy, color, 0);
	}
}

static INT32 ZipzapDraw()
{
	if (BurnRecalc) {
		BuildPalette();
		BurnRecalc = 0;
	}

	BurnPaletteUpdate_GGGGGRRRRRBBBBBx();

	BurnTransferClear();

	if (nBurnLayer & 1) draw_bitmaps();

	bootleg_draw_sprites();

	BurnTransferCopy(BurnPalette);

	return 0;
}

static void clear_oppo(UINT16 &inp)
{
	if ((inp & 0x0300) == 0x0000) {
		inp |= 0x0300;
	}
	if ((inp & 0x0c00) == 0x0000) {
		inp |= 0x0c00;
	}
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	{
		memset (DrvInputs, 0xff, 3 * sizeof(UINT16));

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		clear_oppo(DrvInputs[0]);
		clear_oppo(DrvInputs[1]);
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[1] =  { 12000000 / 60 };
	INT32 nCyclesDone[1] = { nCyclesExtra };

	SekOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		scanline = i;

		CPU_RUN(0, Sek);

		if (scanline ==  32) SekSetIRQLine(5, CPU_IRQSTATUS_AUTO);
		if (scanline == 144) SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
		if (scanline == 256-1) SekSetIRQLine(3, CPU_IRQSTATUS_AUTO);
	}

	SekClose();

	nCyclesExtra = nCyclesDone[0] - nCyclesTotal[0];

	if (pBurnSoundOut) {
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
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
		*pnMin =  0x029702;
	}

	if (nAction & ACB_MEMORY_ROM)
	{
		ba.Data		= Drv68KROM;
		ba.nLen		= 0x500000;
		ba.nAddress	= 0;
		ba.szName	= "68K ROM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_MEMORY_RAM)
	{
		ba.Data		= Drv68KRAM;
		ba.nLen		= 0x0030000;
		ba.nAddress	= 0xc80000;
		ba.szName	= "68k RAM";
		BurnAcb(&ba);

		ba.Data		= DrvFgRAM;
		ba.nLen		= 0x0020000;
		ba.nAddress	= 0x500000;
		ba.szName	= "Fg RAM";
		BurnAcb(&ba);

		ba.Data		= DrvBgRAM;
		ba.nLen		= 0x0020000;
		ba.nAddress	= 0x520000;
		ba.szName	= "Bg RAM";
		BurnAcb(&ba);

		ba.Data		= BurnPalRAM;
		ba.nLen		= 0x1000;
		ba.nAddress	= 0x600000;
		ba.szName	= "Palette RAM";
		BurnAcb(&ba);

		ba.Data		= DrvSprRAM;
		ba.nLen		= 0x001000;
		ba.nAddress	= 0x700000;
		ba.szName	= "Sprites";
		BurnAcb(&ba);

		ba.Data		= DrvSprRegs;
		ba.nLen		= 0x000020;
		ba.nAddress	= 0x780000;
		ba.szName	= "Sprite Regs";
		BurnAcb(&ba);

		ba.Data		= DrvView2Regs;
		ba.nLen		= 0x000020;
		ba.nAddress	= 0x680000;
		ba.szName	= "View2 Regs";
		BurnAcb(&ba);

		ba.Data		= DrvView2RAM;
		ba.nLen		= 0x004000;
		ba.nAddress	= 0x580000;
		ba.szName	= "View2 RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA)
	{
		SekScan(nAction);

		MSM6295Scan(nAction, pnMin);
		BurnWatchdogScan(nAction);
		kaneko_hit_calc_scan(nAction);

		SCAN_VAR(tilebank);
		SCAN_VAR(nOKIBank);
		SCAN_VAR(nCyclesExtra);
	}

	if (nAction & ACB_WRITE)
	{
		set_oki_bank(nOKIBank);
	}

	return 0;
}


// Gals Panic (Export, EXPRO-02 PCB)

static struct BurnRomInfo galsnewRomDesc[] = {
	{ "pm110e_u87-01.u87",			0x020000, 0x34e1ee0d, 0x1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "pm109e_u88-01.u88",			0x020000, 0xc694255a, 0x1 | BRF_PRG | BRF_ESS }, //  1
	{ "pm005e.u85",					0x080000, 0xd7ec650c, 0x5 | BRF_PRG | BRF_ESS }, //  2
	{ "pm004e.u86",					0x080000, 0xd3af52bc, 0x5 | BRF_PRG | BRF_ESS }, //  3
	{ "pm001e.u73",					0x080000, 0x90433eb1, 0x5 | BRF_PRG | BRF_ESS }, //  4
	{ "pm000e.u74",					0x080000, 0x5d220f3f, 0x5 | BRF_PRG | BRF_ESS }, //  5
	{ "pm003e.u75",					0x080000, 0x6bb060fd, 0x5 | BRF_PRG | BRF_ESS }, //  6
	{ "pm002e.u76",					0x080000, 0x713ee898, 0x5 | BRF_PRG | BRF_ESS }, //  7
	{ "pm017e.u84",					0x080000, 0xbc41b6ca, 0xa | BRF_PRG | BRF_ESS }, //  8

	{ "pm006e.u83",					0x080000, 0xa7555d9a, 0x2 | BRF_GRA },           //  9 Sprites
	{ "pm206e.u82",					0x080000, 0xcc978baa, 0x2 | BRF_GRA },           // 10
	{ "pm018e.u94",					0x080000, 0xf542d708, 0x2 | BRF_GRA },           // 11

	{ "pm013e.u89",					0x080000, 0x10f27b05, 0x6 | BRF_GRA },           // 12 View2 Tiles
	{ "pm014e.u90",					0x080000, 0x2f367106, 0x6 | BRF_GRA },           // 13
	{ "pm015e.u91",					0x080000, 0xa563f8ef, 0x6 | BRF_GRA },           // 14
	{ "pm016e.u92",					0x080000, 0xc0b9494c, 0x6 | BRF_GRA },           // 15

	{ "pm008e.u46",					0x080000, 0xd9379ba8, 0x2 | BRF_SND },           // 16 Samples
	{ "pm007e.u47",					0x080000, 0xc7ed7950, 0x2 | BRF_SND },           // 17
};

STD_ROM_PICK(galsnew)
STD_ROM_FN(galsnew)

struct BurnDriver BurnDrvGalsnew = {
	"galsnew", NULL, NULL, NULL, "1990",
	"Gals Panic (Export, EXPRO-02 PCB)\0", NULL, "Kaneko", "EXPRO-02",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, galsnewRomInfo, galsnewRomName, NULL, NULL, NULL, NULL, DrvInputInfo, GalsnewDIPInfo,
	GalsnewInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x800,
	224, 256, 3, 4
};


// Gals Panic (US, EXPRO-02 PCB)

static struct BurnRomInfo galsnewuRomDesc[] = {
	{ "pm110u_u87-01.u87",			0x020000, 0xb793a57d, 0x1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "pm109u_u88-01.u88",			0x020000, 0x35b936f8, 0x1 | BRF_PRG | BRF_ESS }, //  1
	{ "pm005e.u85",					0x080000, 0xd7ec650c, 0x5 | BRF_PRG | BRF_ESS }, //  2
	{ "pm004e.u86",					0x080000, 0xd3af52bc, 0x5 | BRF_PRG | BRF_ESS }, //  3
	{ "pm001e.u73",					0x080000, 0x90433eb1, 0x5 | BRF_PRG | BRF_ESS }, //  4
	{ "pm000e.u74",					0x080000, 0x5d220f3f, 0x5 | BRF_PRG | BRF_ESS }, //  5
	{ "pm003e.u75",					0x080000, 0x6bb060fd, 0x5 | BRF_PRG | BRF_ESS }, //  6
	{ "pm002e.u76",					0x080000, 0x713ee898, 0x5 | BRF_PRG | BRF_ESS }, //  7
	{ "pm017e.u84",					0x080000, 0xbc41b6ca, 0xa | BRF_PRG | BRF_ESS }, //  8

	{ "pm006e.u83",					0x080000, 0xa7555d9a, 0x2 | BRF_GRA },           //  9 Sprites
	{ "pm206e.u82",					0x080000, 0xcc978baa, 0x2 | BRF_GRA },           // 10
	{ "pm018e.u94",					0x080000, 0xf542d708, 0x2 | BRF_GRA },           // 11
	{ "pm019u_u93-01.u93",			0x010000, 0x3cb79005, 0x2 | BRF_GRA },           // 12

	{ "pm013e.u89",					0x080000, 0x10f27b05, 0x6 | BRF_GRA },           // 13 View2 Tiles
	{ "pm014e.u90",					0x080000, 0x2f367106, 0x6 | BRF_GRA },           // 14
	{ "pm015e.u91",					0x080000, 0xa563f8ef, 0x6 | BRF_GRA },           // 15
	{ "pm016e.u92",					0x080000, 0xc0b9494c, 0x6 | BRF_GRA },           // 16

	{ "pm008e.u46",					0x080000, 0xd9379ba8, 0x2 | BRF_SND },           // 17 Samples
	{ "pm007e.u47",					0x080000, 0xc7ed7950, 0x2 | BRF_SND },           // 18
};

STD_ROM_PICK(galsnewu)
STD_ROM_FN(galsnewu)

struct BurnDriver BurnDrvGalsnewu = {
	"galsnewu", "galsnew", NULL, NULL, "1990",
	"Gals Panic (US, EXPRO-02 PCB)\0", NULL, "Kaneko", "EXPRO-02",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, galsnewuRomInfo, galsnewuRomName, NULL, NULL, NULL, NULL, DrvInputInfo, Expro02DIPInfo,
	GalsnewInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x800,
	224, 256, 3, 4
};


// Gals Panic (Japan, EXPRO-02 PCB)

static struct BurnRomInfo galsnewjRomDesc[] = {
	{ "pm110j.u87",					0x020000, 0x220b6df5, 0x1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "pm109j.u88",					0x020000, 0x17721444, 0x1 | BRF_PRG | BRF_ESS }, //  1
	{ "pm005e.u85",					0x080000, 0xd7ec650c, 0x5 | BRF_PRG | BRF_ESS }, //  2
	{ "pm004e.u86",					0x080000, 0xd3af52bc, 0x5 | BRF_PRG | BRF_ESS }, //  3
	{ "pm001e.u73",					0x080000, 0x90433eb1, 0x5 | BRF_PRG | BRF_ESS }, //  4
	{ "pm000e.u74",					0x080000, 0x5d220f3f, 0x5 | BRF_PRG | BRF_ESS }, //  5
	{ "pm003e.u75",					0x080000, 0x6bb060fd, 0x5 | BRF_PRG | BRF_ESS }, //  6
	{ "pm002e.u76",					0x080000, 0x713ee898, 0x5 | BRF_PRG | BRF_ESS }, //  7

	{ "pm006e.u83",					0x080000, 0xa7555d9a, 0xa | BRF_PRG | BRF_ESS }, //  8 Sprites
	{ "pm206e.u82",					0x080000, 0xcc978baa, 0xa | BRF_PRG | BRF_ESS }, //  9
	{ "pm018e.u94",					0x080000, 0xf542d708, 0xa | BRF_PRG | BRF_ESS }, // 10

	{ "pm013e.u89",					0x080000, 0x10f27b05, 0x2 | BRF_GRA },           // 11 View2 Tiles
	{ "pm014e.u90",					0x080000, 0x2f367106, 0x2 | BRF_GRA },           // 12
	{ "pm015e.u91",					0x080000, 0xa563f8ef, 0x2 | BRF_GRA },           // 13
	{ "pm016e.u92",					0x080000, 0xc0b9494c, 0x2 | BRF_GRA },           // 14

	{ "pm008j.u46",					0x080000, 0xf394670e, 0x2 | BRF_SND },           // 15 Samples
	{ "pm007j.u47",					0x080000, 0x06780287, 0x2 | BRF_SND },           // 16
};

STD_ROM_PICK(galsnewj)
STD_ROM_FN(galsnewj)

struct BurnDriver BurnDrvGalsnewj = {
	"galsnewj", "galsnew", NULL, NULL, "1990",
	"Gals Panic (Japan, EXPRO-02 PCB)\0", NULL, "Kaneko (Taito license)", "EXPRO-02",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, galsnewjRomInfo, galsnewjRomName, NULL, NULL, NULL, NULL, DrvInputInfo, GalsnewjDIPInfo,
	GalsnewInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x800,
	224, 256, 3, 4
};


// Gals Panic (Korea, EXPRO-02 PCB)

static struct BurnRomInfo galsnewkRomDesc[] = {
	{ "pm110k.u87",					0x020000, 0xbabe6a71, 0x1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "pm109k.u88",					0x020000, 0xe486d98f, 0x1 | BRF_PRG | BRF_ESS }, //  1
	{ "pm005k.u85",					0x080000, 0x33b5d0e3, 0x5 | BRF_PRG | BRF_ESS }, //  2
	{ "pm004k.u86",					0x080000, 0x9a14c8a3, 0x5 | BRF_PRG | BRF_ESS }, //  3
	{ "pm001e.u73",					0x080000, 0x90433eb1, 0x5 | BRF_PRG | BRF_ESS }, //  4
	{ "pm000e.u74",					0x080000, 0x5d220f3f, 0x5 | BRF_PRG | BRF_ESS }, //  5
	{ "pm003e.u75",					0x080000, 0x6bb060fd, 0x5 | BRF_PRG | BRF_ESS }, //  6
	{ "pm002e.u76",					0x080000, 0x713ee898, 0x5 | BRF_PRG | BRF_ESS }, //  7
	{ "pm017k.u84",					0x080000, 0x0c656fb5, 0xa | BRF_PRG | BRF_ESS }, //  8

	{ "pm006e.u83",					0x080000, 0xa7555d9a, 0x2 | BRF_GRA },           //  9 Sprites
	{ "pm206e.u82",					0x080000, 0xcc978baa, 0x2 | BRF_GRA },           // 10
	{ "pm018e.u94",					0x080000, 0xf542d708, 0x2 | BRF_GRA },           // 11
	{ "pm19k.u93",					0x010000, 0xc17d2989, 0x2 | BRF_GRA },           // 12

	{ "pm013e.u89",					0x080000, 0x10f27b05, 0x5 | BRF_GRA },           // 13 View2 Tiles
	{ "pm014e.u90",					0x080000, 0x2f367106, 0x5 | BRF_GRA },           // 14
	{ "pm015e.u91",					0x080000, 0xa563f8ef, 0x5 | BRF_GRA },           // 15
	{ "pm016e.u92",					0x080000, 0xc0b9494c, 0x5 | BRF_GRA },           // 16

	{ "pm008k.u46",					0x080000, 0x7498483f, 0x2 | BRF_SND },           // 17 Samples
	{ "pm007k.u47",					0x080000, 0xa8dc1fd5, 0x2 | BRF_SND },           // 18
};

STD_ROM_PICK(galsnewk)
STD_ROM_FN(galsnewk)

struct BurnDriver BurnDrvGalsnewk = {
	"galsnewk", "galsnew", NULL, NULL, "1990",
	"Gals Panic (Korea, EXPRO-02 PCB)\0", NULL, "Kaneko (Inter license)", "EXPRO-02",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, galsnewkRomInfo, galsnewkRomName, NULL, NULL, NULL, NULL, DrvInputInfo, GalsnewjDIPInfo,
	GalsnewInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x800,
	224, 256, 3, 4
};


// Gals Panic (Taiwan, EXPRO-02 PCB)

static struct BurnRomInfo galsnewtRomDesc[] = {
	{ "pm110t_u87-01.u87",			0x020000, 0x356c65de, 0x1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "pm109t_u88-01.u88",			0x020000, 0x6d7b4131, 0x1 | BRF_PRG | BRF_ESS }, //  1
	{ "pm005e.u85",					0x080000, 0xd7ec650c, 0x5 | BRF_PRG | BRF_ESS }, //  2
	{ "pm004e.u86",					0x080000, 0xd3af52bc, 0x5 | BRF_PRG | BRF_ESS }, //  3
	{ "pm001e.u73",					0x080000, 0x90433eb1, 0x5 | BRF_PRG | BRF_ESS }, //  4
	{ "pm000e.u74",					0x080000, 0x5d220f3f, 0x5 | BRF_PRG | BRF_ESS }, //  5
	{ "pm003e.u75",					0x080000, 0x6bb060fd, 0x5 | BRF_PRG | BRF_ESS }, //  6
	{ "pm002e.u76",					0x080000, 0x713ee898, 0x5 | BRF_PRG | BRF_ESS }, //  7
	{ "pm017e.u84",					0x080000, 0xbc41b6ca, 0xa | BRF_PRG | BRF_ESS }, //  8

	{ "pm006e.u83",					0x080000, 0xa7555d9a, 0x2 | BRF_GRA },           //  9 Sprites
	{ "pm206e.u82",					0x080000, 0xcc978baa, 0x2 | BRF_GRA },           // 10
	{ "pm018e.u94",					0x080000, 0xf542d708, 0x2 | BRF_GRA },           // 11

	{ "pm013e.u89",					0x080000, 0x10f27b05, 0x5 | BRF_GRA },           // 12 View2 Tiles
	{ "pm014e.u90",					0x080000, 0x2f367106, 0x5 | BRF_GRA },           // 13
	{ "pm015e.u91",					0x080000, 0xa563f8ef, 0x5 | BRF_GRA },           // 14
	{ "pm016e.u92",					0x080000, 0xc0b9494c, 0x5 | BRF_GRA },           // 15

	{ "pm008j.u46",					0x080000, 0xf394670e, 0x2 | BRF_SND },           // 16 Samples
	{ "pm007j.u47",					0x080000, 0x06780287, 0x2 | BRF_SND },           // 17
};

STD_ROM_PICK(galsnewt)
STD_ROM_FN(galsnewt)

struct BurnDriver BurnDrvGalsnewt = {
	"galsnewt", "galsnew", NULL, NULL, "1990",
	"Gals Panic (Taiwan, EXPRO-02 PCB)\0", NULL, "Kaneko", "EXPRO-02",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, galsnewtRomInfo, galsnewtRomName, NULL, NULL, NULL, NULL, DrvInputInfo, GalsnewjDIPInfo,
	GalsnewInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x800,
	224, 256, 3, 4
};


// Fantasia (940429 PCB, set 1)

static struct BurnRomInfo fantasiaRomDesc[] = {
	{ "16.pro2",					0x080000, 0xe27c6c57, 0x1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "13.pro1",					0x080000, 0x68d27413, 0x1 | BRF_PRG | BRF_ESS }, //  1
	{ "9.fg_ind87",					0x080000, 0x2a588393, 0x1 | BRF_PRG | BRF_ESS }, //  2
	{ "5.fg_ind83",					0x080000, 0x6160e0f0, 0x1 | BRF_PRG | BRF_ESS }, //  3
	{ "8.fg_ind86",					0x080000, 0xf776b743, 0x1 | BRF_PRG | BRF_ESS }, //  4
	{ "4.fg_ind82",					0x080000, 0x5df0dff2, 0x1 | BRF_PRG | BRF_ESS }, //  5
	{ "7.fg_ind85",					0x080000, 0x5707d861, 0x1 | BRF_PRG | BRF_ESS }, //  6
	{ "3.fg_ind81",					0x080000, 0x36cb811a, 0x1 | BRF_PRG | BRF_ESS }, //  7
	{ "10.imag2",					0x080000, 0x1f14a395, 0x1 | BRF_PRG | BRF_ESS }, //  8
	{ "6.imag1",					0x080000, 0xfaf870e4, 0x1 | BRF_PRG | BRF_ESS }, //  9

	{ "17.scr3",					0x080000, 0xaadb6eb7, 0x2 | BRF_GRA },           // 10 Sprites

	{ "15.obj3",					0x080000, 0x46666768, 0x5 | BRF_GRA },           // 11 View2 Tiles
	{ "12.obj1",					0x080000, 0x4bd25be6, 0x5 | BRF_GRA },           // 12
	{ "14.obj4",					0x080000, 0x4e7e6ed4, 0x5 | BRF_GRA },           // 13
	{ "11.obj2",					0x080000, 0x6d00a4c5, 0x5 | BRF_GRA },           // 14

	{ "2.music1",					0x080000, 0x22955efb, 0x2 | BRF_SND },           // 15 Samples
	{ "1.music2",					0x080000, 0x4cd4d6c3, 0x2 | BRF_SND },           // 16
};

STD_ROM_PICK(fantasia)
STD_ROM_FN(fantasia)

struct BurnDriver BurnDrvFantasia = {
	"fantasia", NULL, NULL, NULL, "1994",
	"Fantasia (940429 PCB, set 1)\0", NULL, "Comad & New Japan System", "EXPRO-02",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, fantasiaRomInfo, fantasiaRomName, NULL, NULL, NULL, NULL, DrvInputInfo, FantasiaDIPInfo,
	FantasiaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x800,
	224, 256, 3, 4
};


// Fantasia (940307 PCB)

static struct BurnRomInfo fantasiaaRomDesc[] = {
	{ "prog2_16.ue17",				0x080000, 0x0b41ad10, 0x1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "prog1_13.ud17",				0x080000, 0xa3748726, 0x1 | BRF_PRG | BRF_ESS }, //  1
	{ "i-scr6_9.ue16b",				0x080000, 0x2a588393, 0x1 | BRF_PRG | BRF_ESS }, //  2
	{ "i-scr5_5.ue16a",				0x080000, 0x6160e0f0, 0x1 | BRF_PRG | BRF_ESS }, //  3
	{ "i-scr4_8.ue15b",				0x080000, 0xf776b743, 0x1 | BRF_PRG | BRF_ESS }, //  4
	{ "i-scr3_4.ue15a",				0x080000, 0x5df0dff2, 0x1 | BRF_PRG | BRF_ESS }, //  5
	{ "i-scr2_7.ue14b",				0x080000, 0x5707d861, 0x1 | BRF_PRG | BRF_ESS }, //  6
	{ "i-scr1_3.ue14a",				0x080000, 0x36cb811a, 0x1 | BRF_PRG | BRF_ESS }, //  7
	{ "imag2_10.ue20b",				0x080000, 0x1f14a395, 0x1 | BRF_PRG | BRF_ESS }, //  8
	{ "imag1_6.ue20a",				0x080000, 0xfaf870e4, 0x1 | BRF_PRG | BRF_ESS }, //  9

	{ "obj1_17.u5",					0x080000, 0xaadb6eb7, 0x2 | BRF_GRA },           // 10 Sprites

	{ "g-scr2_15.ul16b",			0x080000, 0x46666768, 0x5 | BRF_GRA },           // 11 View2 Tiles
	{ "g-scr1_12.ul16a",			0x080000, 0x4bd25be6, 0x5 | BRF_GRA },           // 12
	{ "g-scr4_14.ul19b",			0x080000, 0x4e7e6ed4, 0x5 | BRF_GRA },           // 13
	{ "g-scr3_11.ul19a",			0x080000, 0x6d00a4c5, 0x5 | BRF_GRA },           // 14

	{ "music1_1.ub6",				0x080000, 0xaf0be817, 0x2 | BRF_SND },           // 15 Samples
	{ "music2_2.uc6",				0x080000, 0x4cd4d6c3, 0x2 | BRF_SND },           // 16
};

STD_ROM_PICK(fantasiaa)
STD_ROM_FN(fantasiaa)

struct BurnDriver BurnDrvFantasiaa = {
	"fantasiaa", "fantasia", NULL, NULL, "1994",
	"Fantasia (940307 PCB)\0", NULL, "Comad & New Japan System", "EXPRO-02",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, fantasiaaRomInfo, fantasiaaRomName, NULL, NULL, NULL, NULL, DrvInputInfo, FantasiaDIPInfo,
	FantasiaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x800,
	224, 256, 3, 4
};


// Fantasia (940429 PCB, set 2)

static struct BurnRomInfo fantasiabRomDesc[] = {
	{ "fantasia_16",				0x080000, 0xc5d93077, 0x1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "fantasia_13",				0x080000, 0xd88529bd, 0x1 | BRF_PRG | BRF_ESS }, //  1
	{ "9.fg_ind87",					0x080000, 0x2a588393, 0x1 | BRF_PRG | BRF_ESS }, //  2
	{ "5.fg_ind83",					0x080000, 0x6160e0f0, 0x1 | BRF_PRG | BRF_ESS }, //  3
	{ "8.fg_ind86",					0x080000, 0xf776b743, 0x1 | BRF_PRG | BRF_ESS }, //  4
	{ "4.fg_ind82",					0x080000, 0x5df0dff2, 0x1 | BRF_PRG | BRF_ESS }, //  5
	{ "7.fg_ind85",					0x080000, 0x5707d861, 0x1 | BRF_PRG | BRF_ESS }, //  6
	{ "3.fg_ind81",					0x080000, 0x36cb811a, 0x1 | BRF_PRG | BRF_ESS }, //  7
	{ "10.imag2",					0x080000, 0x1f14a395, 0x1 | BRF_PRG | BRF_ESS }, //  8
	{ "6.imag1",					0x080000, 0xfaf870e4, 0x1 | BRF_PRG | BRF_ESS }, //  9

	{ "17.scr3",					0x080000, 0xaadb6eb7, 0x2 | BRF_GRA },           // 10 Sprites

	{ "15.obj3",					0x080000, 0x46666768, 0x5 | BRF_GRA },           // 11 View2 Tiles
	{ "12.obj1",					0x080000, 0x4bd25be6, 0x5 | BRF_GRA },           // 12
	{ "14.obj4",					0x080000, 0x4e7e6ed4, 0x5 | BRF_GRA },           // 13
	{ "11.obj2",					0x080000, 0x6d00a4c5, 0x5 | BRF_GRA },           // 14

	{ "2.music1",					0x080000, 0x22955efb, 0x2 | BRF_SND },           // 15 Samples
	{ "1.music2",					0x080000, 0x4cd4d6c3, 0x2 | BRF_SND },           // 16
};

STD_ROM_PICK(fantasiab)
STD_ROM_FN(fantasiab)

struct BurnDriver BurnDrvFantasiab = {
	"fantasiab", "fantasia", NULL, NULL, "1994",
	"Fantasia (940429 PCB, set 2)\0", NULL, "Comad & New Japan System", "EXPRO-02",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, fantasiabRomInfo, fantasiabRomName, NULL, NULL, NULL, NULL, DrvInputInfo, FantasiaDIPInfo,
	FantasiaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x800,
	224, 256, 3, 4
};


// Fantasia (940803 PCB)

static struct BurnRomInfo fantasianRomDesc[] = {
	{ "prog2_12.ue17",				0x080000, 0x8bb70be1, 0x1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "prog1_7.ud17",				0x080000, 0xd1616a3e, 0x1 | BRF_PRG | BRF_ESS }, //  1
	{ "i-scr2_10.ue16b",			0x080000, 0x2a588393, 0x1 | BRF_PRG | BRF_ESS }, //  2
	{ "i-scr1_5.ue16a",				0x080000, 0x6160e0f0, 0x1 | BRF_PRG | BRF_ESS }, //  3
	{ "i-scr4_9.ue15b",				0x080000, 0xf776b743, 0x1 | BRF_PRG | BRF_ESS }, //  4
	{ "i-scr3_4.ue15a",				0x080000, 0x5df0dff2, 0x1 | BRF_PRG | BRF_ESS }, //  5
	{ "i-scr6_8.ue14b",				0x080000, 0x5707d861, 0x1 | BRF_PRG | BRF_ESS }, //  6
	{ "i-scr5_3.ue14a",				0x080000, 0x36cb811a, 0x1 | BRF_PRG | BRF_ESS }, //  7
	{ "i-scr8_11.ue20b",			0x080000, 0x1f14a395, 0x1 | BRF_PRG | BRF_ESS }, //  8
	{ "i-scr7_6.ue20a",				0x080000, 0xfaf870e4, 0x1 | BRF_PRG | BRF_ESS }, //  9

	{ "obj1_13.u5",					0x080000, 0xf99751f5, 0x2 | BRF_GRA },           // 10 Sprites

	{ "music1_1.ub6",				0x080000, 0x22955efb, 0x2 | BRF_SND },           // 11 Samples
	{ "music2_2.uc6",				0x080000, 0x4cd4d6c3, 0x2 | BRF_SND },           // 12
};

STD_ROM_PICK(fantasian)
STD_ROM_FN(fantasian)

struct BurnDriver BurnDrvFantasian = {
	"fantasian", "fantasia", NULL, NULL, "1994",
	"Fantasia (940803 PCB)\0", NULL, "Comad & New Japan System", "EXPRO-02",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, fantasianRomInfo, fantasianRomName, NULL, NULL, NULL, NULL, DrvInputInfo, FantasiaaDIPInfo,
	FantasianInit, DrvExit, DrvFrame, ComadDraw, DrvScan, &BurnRecalc, 0x800,
	224, 256, 3, 4
};


// New Fantasia (1995 copyright)

static struct BurnRomInfo newfantRomDesc[] = {
	{ "prog2.12",					0x080000, 0xde43a457, 0x1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "prog1.07",					0x080000, 0x370b45be, 0x1 | BRF_PRG | BRF_ESS }, //  1
	{ "iscr2.10",					0x080000, 0x4f2da2eb, 0x1 | BRF_PRG | BRF_ESS }, //  2
	{ "iscr1.05",					0x080000, 0x63c6894f, 0x1 | BRF_PRG | BRF_ESS }, //  3
	{ "iscr4.09",					0x080000, 0x725741ec, 0x1 | BRF_PRG | BRF_ESS }, //  4
	{ "iscr3.04",					0x080000, 0x51d6b362, 0x1 | BRF_PRG | BRF_ESS }, //  5
	{ "iscr6.08",					0x080000, 0x178b2ef3, 0x1 | BRF_PRG | BRF_ESS }, //  6
	{ "iscr5.03",					0x080000, 0xd2b5c5fa, 0x1 | BRF_PRG | BRF_ESS }, //  7
	{ "iscr8.11",					0x080000, 0xf4148528, 0x1 | BRF_PRG | BRF_ESS }, //  8
	{ "iscr7.06",					0x080000, 0x2dee0c31, 0x1 | BRF_PRG | BRF_ESS }, //  9

	{ "nf95obj1.13",				0x080000, 0xe6d1bc71, 0x2 | BRF_GRA },           // 10 Sprites

	{ "musc1.01",					0x080000, 0x10347fce, 0x2 | BRF_SND },           // 11 Samples
	{ "musc2.02",					0x080000, 0xb9646a8c, 0x2 | BRF_SND },           // 12
};

STD_ROM_PICK(newfant)
STD_ROM_FN(newfant)

struct BurnDriver BurnDrvNewfant = {
	"newfant", NULL, NULL, NULL, "1995",
	"New Fantasia (1995 copyright)\0", NULL, "Comad & New Japan System", "EXPRO-02",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, newfantRomInfo, newfantRomName, NULL, NULL, NULL, NULL, DrvInputInfo, FantasiaaDIPInfo,
	FantasianInit, DrvExit, DrvFrame, ComadDraw, DrvScan, &BurnRecalc, 0x800,
	224, 256, 3, 4
};


// New Fantasia (1994 copyright)

static struct BurnRomInfo newfantaRomDesc[] = {
	{ "12.ue17",					0x080000, 0xde43a457, 0x1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "7.ud17",						0x080000, 0x370b45be, 0x1 | BRF_PRG | BRF_ESS }, //  1
	{ "10.ue16b",					0x080000, 0x4f2da2eb, 0x1 | BRF_PRG | BRF_ESS }, //  2
	{ "5.ue16a",					0x080000, 0x63c6894f, 0x1 | BRF_PRG | BRF_ESS }, //  3
	{ "9.ue15b",					0x080000, 0x725741ec, 0x1 | BRF_PRG | BRF_ESS }, //  4
	{ "4.ue15a",					0x080000, 0x51d6b362, 0x1 | BRF_PRG | BRF_ESS }, //  5
	{ "8.ue14b",					0x080000, 0x178b2ef3, 0x1 | BRF_PRG | BRF_ESS }, //  6
	{ "3.ue14a",					0x080000, 0xd2b5c5fa, 0x1 | BRF_PRG | BRF_ESS }, //  7
	{ "11.ue20b",					0x080000, 0xf4148528, 0x1 | BRF_PRG | BRF_ESS }, //  8
	{ "6.ue20a",					0x080000, 0x2dee0c31, 0x1 | BRF_PRG | BRF_ESS }, //  9

	{ "13.u5",						0x080000, 0x832cd451, 0x2 | BRF_GRA },           // 10 Sprites

	{ "1.ub6",						0x080000, 0x10347fce, 0x2 | BRF_SND },           // 11 Samples
	{ "2.uc6",						0x080000, 0xb9646a8c, 0x2 | BRF_SND },           // 12
};

STD_ROM_PICK(newfanta)
STD_ROM_FN(newfanta)

struct BurnDriver BurnDrvNewfanta = {
	"newfanta", "newfant", NULL, NULL, "1994",
	"New Fantasia (1994 copyright)\0", NULL, "Comad & New Japan System", "EXPRO-02",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, newfantaRomInfo, newfantaRomName, NULL, NULL, NULL, NULL, DrvInputInfo, FantasiaaDIPInfo,
	FantasianInit, DrvExit, DrvFrame, ComadDraw, DrvScan, &BurnRecalc, 0x800,
	224, 256, 3, 4
};


// Hot Night

static struct BurnRomInfo hotnightRomDesc[] = {
	{ "12_prog2_4m_12.ue17",		0x080000, 0x30094b5c, 0x1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "7_prog1_4m_7.ud17",			0x080000, 0x64503285, 0x1 | BRF_PRG | BRF_ESS }, //  1
	{ "10_i-scr2_4m_10.ue16b",		0x080000, 0xab8756ff, 0x1 | BRF_PRG | BRF_ESS }, //  2
	{ "5_i-scr1_4m_5.ue16a",		0x080000, 0xd8e2ef77, 0x1 | BRF_PRG | BRF_ESS }, //  3
	{ "9_i-scr4_4m_9.ue15b",		0x080000, 0x4e52eb23, 0x1 | BRF_PRG | BRF_ESS }, //  4
	{ "4_i-scr3_4m_4.ue15a",		0x080000, 0x797731f8, 0x1 | BRF_PRG | BRF_ESS }, //  5
	{ "8_i-scr6_4m_8.ue14b",		0x080000, 0x6f8e5239, 0x1 | BRF_PRG | BRF_ESS }, //  6
	{ "3_i-scr5_4m_3.ue14a",		0x080000, 0x85420e3f, 0x1 | BRF_PRG | BRF_ESS }, //  7
	{ "11_i-scr8_4m_11.ue20b",		0x080000, 0x67006b4e, 0x1 | BRF_PRG | BRF_ESS }, //  8
	{ "6_i-scr7_4m_6.ue20a",		0x080000, 0x8979ffaf, 0x1 | BRF_PRG | BRF_ESS }, //  9

	{ "13_obj1_4m_13.u5",			0x080000, 0x832cd451, 0x2 | BRF_GRA },           // 10 Sprites

	{ "1_music1_4m_1.ub6",			0x080000, 0x3117e2ef, 0x2 | BRF_SND },           // 11 Samples
	{ "2_music2_4m_2.uc6",			0x080000, 0x0c1109f9, 0x2 | BRF_SND },           // 12
};

STD_ROM_PICK(hotnight)
STD_ROM_FN(hotnight)

struct BurnDriver BurnDrvHotnight = {
	"hotnight", "newfant", NULL, NULL, "1994",
	"Hot Night\0", NULL, "Bulldog Amusements Inc.", "EXPRO-02",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, hotnightRomInfo, hotnightRomName, NULL, NULL, NULL, NULL, DrvInputInfo, FantasiaaDIPInfo,
	FantasianInit, DrvExit, DrvFrame, ComadDraw, DrvScan, &BurnRecalc, 0x800,
	224, 256, 3, 4
};


// Fantasy '95

static struct BurnRomInfo fantsy95RomDesc[] = {
	{ "prog2.12",					0x080000, 0x1e684da7, 0x1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "prog1.7",					0x080000, 0xdc4e4f6b, 0x1 | BRF_PRG | BRF_ESS }, //  1
	{ "i-scr2.10",					0x080000, 0xab8756ff, 0x1 | BRF_PRG | BRF_ESS }, //  2
	{ "i-scr1.5",					0x080000, 0xd8e2ef77, 0x1 | BRF_PRG | BRF_ESS }, //  3
	{ "i-scr4.9",					0x080000, 0x4e52eb23, 0x1 | BRF_PRG | BRF_ESS }, //  4
	{ "i-scr3.4",					0x080000, 0x797731f8, 0x1 | BRF_PRG | BRF_ESS }, //  5
	{ "i-scr6.8",					0x080000, 0x6f8e5239, 0x1 | BRF_PRG | BRF_ESS }, //  6
	{ "i-scr5.3",					0x080000, 0x85420e3f, 0x1 | BRF_PRG | BRF_ESS }, //  7
	{ "i-scr8.11",					0x080000, 0x33db8177, 0x1 | BRF_PRG | BRF_ESS }, //  8
	{ "i-scr7.6",					0x080000, 0x8662dd01, 0x1 | BRF_PRG | BRF_ESS }, //  9

	{ "obj1.13",					0x080000, 0x832cd451, 0x2 | BRF_GRA },           // 10 Sprites

	{ "music1.1",					0x080000, 0x3117e2ef, 0x2 | BRF_SND },           // 11 Samples
	{ "music2.2",					0x080000, 0x0c1109f9, 0x2 | BRF_SND },           // 12
};

STD_ROM_PICK(fantsy95)
STD_ROM_FN(fantsy95)

struct BurnDriver BurnDrvFantsy95 = {
	"fantsy95", "newfant", NULL, NULL, "1995",
	"Fantasy '95\0", NULL, "Hi-max Technology Inc.", "EXPRO-02",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, fantsy95RomInfo, fantsy95RomName, NULL, NULL, NULL, NULL, DrvInputInfo, FantasiaaDIPInfo,
	FantasianInit, DrvExit, DrvFrame, ComadDraw, DrvScan, &BurnRecalc, 0x800,
	224, 256, 3, 4
};



// Miss World '96 (Nude) (C-3000A PCB, set 1)

static struct BurnRomInfo missw96RomDesc[] = {
	{ "mw96_10.bin",				0x080000, 0xb1309bb1, 0x1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "mw96_06.bin",				0x080000, 0xa5892bb3, 0x1 | BRF_PRG | BRF_ESS }, //  1
	{ "mw96_09.bin",				0x080000, 0x7032dfdf, 0x1 | BRF_PRG | BRF_ESS }, //  2
	{ "mw96_05.bin",				0x080000, 0x91de5ab5, 0x1 | BRF_PRG | BRF_ESS }, //  3
	{ "mw96_08.bin",				0x080000, 0xb8e66fb5, 0x1 | BRF_PRG | BRF_ESS }, //  4
	{ "mw96_04.bin",				0x080000, 0xe77a04f8, 0x1 | BRF_PRG | BRF_ESS }, //  5
	{ "mw96_07.bin",				0x080000, 0x26112ed3, 0x1 | BRF_PRG | BRF_ESS }, //  6
	{ "mw96_03.bin",				0x080000, 0xe9374a46, 0x1 | BRF_PRG | BRF_ESS }, //  7

	{ "mw96_11.bin",				0x080000, 0x3983152f, 0x2 | BRF_GRA },           //  8 Sprites

	{ "mw96_01.bin",				0x080000, 0xe78a659e, 0x2 | BRF_SND },           //  9 Samples
	{ "mw96_02.bin",				0x080000, 0x60fa0c00, 0x2 | BRF_SND },           // 10
};

STD_ROM_PICK(missw96)
STD_ROM_FN(missw96)

struct BurnDriver BurnDrvMissw96 = {
	"missw96", NULL, NULL, NULL, "1996",
	"Miss World '96 (Nude) (C-3000A PCB, set 1)\0", NULL, "Comad", "EXPRO-02",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, missw96RomInfo, missw96RomName, NULL, NULL, NULL, NULL, DrvInputInfo, Missw96DIPInfo,
	FantasianInit, DrvExit, DrvFrame, ComadDraw, DrvScan, &BurnRecalc, 0x800,
	256, 224, 4, 3
};


// Miss World '96 (Nude) (C-3000A PCB, set 2)

static struct BurnRomInfo missw96aRomDesc[] = {
	{ "mw96n2_10.prog2",			0x080000, 0x563ce811, 0x1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "mw96n2_6.prog1",				0x080000, 0x98e91a3b, 0x1 | BRF_PRG | BRF_ESS }, //  1
	{ "mw96_09.bin",				0x080000, 0x7032dfdf, 0x1 | BRF_PRG | BRF_ESS }, //  2
	{ "mw96_05.bin",				0x080000, 0x91de5ab5, 0x1 | BRF_PRG | BRF_ESS }, //  3
	{ "mw96_08.bin",				0x080000, 0xb8e66fb5, 0x1 | BRF_PRG | BRF_ESS }, //  4
	{ "mw96_04.bin",				0x080000, 0xe77a04f8, 0x1 | BRF_PRG | BRF_ESS }, //  5
	{ "mw96_07.bin",				0x080000, 0x26112ed3, 0x1 | BRF_PRG | BRF_ESS }, //  6
	{ "mw96_03.bin",				0x080000, 0xe9374a46, 0x1 | BRF_PRG | BRF_ESS }, //  7

	{ "mw96_11.bin",				0x080000, 0x3983152f, 0x2 | BRF_GRA },           //  8 Sprites

	{ "mw96_01.bin",				0x080000, 0xe78a659e, 0x2 | BRF_SND },           //  9 Samples
	{ "mw96_02.bin",				0x080000, 0x60fa0c00, 0x2 | BRF_SND },           // 10
};

STD_ROM_PICK(missw96a)
STD_ROM_FN(missw96a)

struct BurnDriver BurnDrvMissw96a = {
	"missw96a", "missw96", NULL, NULL, "1996",
	"Miss World '96 (Nude) (C-3000A PCB, set 2)\0", NULL, "Comad", "EXPRO-02",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, missw96aRomInfo, missw96aRomName, NULL, NULL, NULL, NULL, DrvInputInfo, Missw96DIPInfo,
	FantasianInit, DrvExit, DrvFrame, ComadDraw, DrvScan, &BurnRecalc, 0x800,
	256, 224, 4, 3
};


// Miss World '96 (Nude) (C-3000A PCB, set 3)

static struct BurnRomInfo missw96bRomDesc[] = {
	{ "mw96n3_10.prog2",			0x080000, 0x67bde86b, 0x1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "mw96n3_6.prog1",				0x080000, 0xde99cc48, 0x1 | BRF_PRG | BRF_ESS }, //  1
	{ "mw96_09.bin",				0x080000, 0x7032dfdf, 0x1 | BRF_PRG | BRF_ESS }, //  2
	{ "mw96_05.bin",				0x080000, 0x91de5ab5, 0x1 | BRF_PRG | BRF_ESS }, //  3
	{ "mw96_08.bin",				0x080000, 0xb8e66fb5, 0x1 | BRF_PRG | BRF_ESS }, //  4
	{ "mw96_04.bin",				0x080000, 0xe77a04f8, 0x1 | BRF_PRG | BRF_ESS }, //  5
	{ "mw96_07.bin",				0x080000, 0x26112ed3, 0x1 | BRF_PRG | BRF_ESS }, //  6
	{ "mw96_03.bin",				0x080000, 0xe9374a46, 0x1 | BRF_PRG | BRF_ESS }, //  7

	{ "mw96_11.bin",				0x080000, 0x3983152f, 0x2 | BRF_GRA },           //  8 Sprites

	{ "mw96_01.bin",				0x080000, 0xe78a659e, 0x2 | BRF_SND },           //  9 Samples
	{ "mw96_02.bin",				0x080000, 0x60fa0c00, 0x2 | BRF_SND },           // 10
};

STD_ROM_PICK(missw96b)
STD_ROM_FN(missw96b)

struct BurnDriver BurnDrvMissw96b = {
	"missw96b", "missw96", NULL, NULL, "1996",
	"Miss World '96 (Nude) (C-3000A PCB, set 3)\0", NULL, "Comad", "EXPRO-02",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, missw96bRomInfo, missw96bRomName, NULL, NULL, NULL, NULL, DrvInputInfo, Missw96DIPInfo,
	FantasianInit, DrvExit, DrvFrame, ComadDraw, DrvScan, &BurnRecalc, 0x800,
	256, 224, 4, 3
};


// Miss World '96 (Nude) (C-3000B PCB)

static struct BurnRomInfo missw96cRomDesc[] = {
	{ "10_prog2.ue17",				0x080000, 0x36a7beb6, 0x1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "6_prog1.ud17",				0x080000, 0xe70b562f, 0x1 | BRF_PRG | BRF_ESS }, //  1
	{ "9_im1-b.ue16b",				0x080000, 0xeedc24f8, 0x1 | BRF_PRG | BRF_ESS }, //  2
	{ "5_im1-a.ue16a",				0x080000, 0xbb0eb7d7, 0x1 | BRF_PRG | BRF_ESS }, //  3
	{ "8_im2-b.ue15b",				0x080000, 0x68dd67b2, 0x1 | BRF_PRG | BRF_ESS }, //  4
	{ "4_im2-a.ue15a",				0x080000, 0x2b39ec56, 0x1 | BRF_PRG | BRF_ESS }, //  5
	{ "7_im3_b.ue14b",				0x080000, 0x7fd5ca2c, 0x1 | BRF_PRG | BRF_ESS }, //  6
	{ "3_im3-a.ue14a",				0x080000, 0x4ba5dab7, 0x1 | BRF_PRG | BRF_ESS }, //  7

	{ "20_obj1.u5",					0x080000, 0x3983152f, 0x2 | BRF_GRA },           //  8 Sprites

	{ "1_music1.ub6",				0x080000, 0xe78a659e, 0x2 | BRF_SND },           //  9 Samples
	{ "2_music2.uc6",				0x080000, 0x60fa0c00, 0x2 | BRF_SND },           // 10
};

STD_ROM_PICK(missw96c)
STD_ROM_FN(missw96c)

struct BurnDriver BurnDrvMissw96c = {
	"missw96c", "missw96", NULL, NULL, "1996",
	"Miss World '96 (Nude) (C-3000B PCB)\0", NULL, "Comad", "EXPRO-02",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, missw96cRomInfo, missw96cRomName, NULL, NULL, NULL, NULL, DrvInputInfo, Missw96DIPInfo,
	FantasianInit, DrvExit, DrvFrame, ComadDraw, DrvScan, &BurnRecalc, 0x800,
	256, 224, 4, 3
};


// Miss Mister World '96 (Nude)

static struct BurnRomInfo missmw96RomDesc[] = {
	{ "mmw96_10.bin",				0x080000, 0x45ed1cd9, 0x1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "mmw96_06.bin",				0x080000, 0x52ec9e5d, 0x1 | BRF_PRG | BRF_ESS }, //  1
	{ "mmw96_09.bin",				0x080000, 0x6c458b05, 0x1 | BRF_PRG | BRF_ESS }, //  2
	{ "mmw96_05.bin",				0x080000, 0x48159555, 0x1 | BRF_PRG | BRF_ESS }, //  3
	{ "mmw96_08.bin",				0x080000, 0x1dc72b07, 0x1 | BRF_PRG | BRF_ESS }, //  4
	{ "mmw96_04.bin",				0x080000, 0xfc3e18fa, 0x1 | BRF_PRG | BRF_ESS }, //  5
	{ "mmw96_07.bin",				0x080000, 0x001572bf, 0x1 | BRF_PRG | BRF_ESS }, //  6
	{ "mmw96_03.bin",				0x080000, 0x22204025, 0x1 | BRF_PRG | BRF_ESS }, //  7

	{ "mmw96_11.bin",				0x080000, 0x7d491f8c, 0x2 | BRF_GRA },           //  8 Sprites

	{ "mw96_01.bin",				0x080000, 0xe78a659e, 0x2 | BRF_SND },           //  9 Samples
	{ "mw96_02.bin",				0x080000, 0x60fa0c00, 0x2 | BRF_SND },           // 10
};

STD_ROM_PICK(missmw96)
STD_ROM_FN(missmw96)

struct BurnDriver BurnDrvMissmw96 = {
	"missmw96", "missw96", NULL, NULL, "1996",
	"Miss Mister World '96 (Nude)\0", NULL, "Comad", "EXPRO-02",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, missmw96RomInfo, missmw96RomName, NULL, NULL, NULL, NULL, DrvInputInfo, Missw96DIPInfo,
	FantasianInit, DrvExit, DrvFrame, ComadDraw, DrvScan, &BurnRecalc, 0x800,
	256, 224, 4, 3
};


// Super Model

static struct BurnRomInfo supmodelRomDesc[] = {
	{ "prog2.12",					0x080000, 0x714b7e74, 0x1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "prog1.7",					0x080000, 0x0bb858de, 0x1 | BRF_PRG | BRF_ESS }, //  1
	{ "i-scr2.10",					0x080000, 0xd07ec0ce, 0x1 | BRF_PRG | BRF_ESS }, //  2
	{ "i-scr1.5",					0x080000, 0xa96a8bde, 0x1 | BRF_PRG | BRF_ESS }, //  3
	{ "i-scr4.9",					0x080000, 0xe959cab5, 0x1 | BRF_PRG | BRF_ESS }, //  4
	{ "i-scr3.4",					0x080000, 0x4bf5e082, 0x1 | BRF_PRG | BRF_ESS }, //  5
	{ "i-scr6.8",					0x080000, 0xe71337c2, 0x1 | BRF_PRG | BRF_ESS }, //  6
	{ "i-scr5.3",					0x080000, 0x641ccdfb, 0x1 | BRF_PRG | BRF_ESS }, //  7
	{ "i-scr8.11",					0x080000, 0x7c1813c8, 0x1 | BRF_PRG | BRF_ESS }, //  8
	{ "i-scr7.6",					0x080000, 0x19c73268, 0x1 | BRF_PRG | BRF_ESS }, //  9

	{ "obj1.13",					0x080000, 0x832cd451, 0x2 | BRF_GRA },           // 10 Sprites

	{ "music1.1",					0x080000, 0x2b1f6655, 0x2 | BRF_SND },           // 11 Samples
	{ "music2.2",					0x080000, 0xcccae65a, 0x2 | BRF_SND },           // 12
};

STD_ROM_PICK(supmodel)
STD_ROM_FN(supmodel)

struct BurnDriver BurnDrvSupmodel = {
	"supmodel", NULL, NULL, NULL, "1994",
	"Super Model\0", NULL, "Comad & New Japan System", "EXPRO-02",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, supmodelRomInfo, supmodelRomName, NULL, NULL, NULL, NULL, DrvInputInfo, FantasiaaDIPInfo,
	SupmodelInit, DrvExit, DrvFrame, ComadDraw, DrvScan, &BurnRecalc, 0x800,
	256, 224, 4, 3
};


// Fantasia II (Explicit)

static struct BurnRomInfo fantsia2RomDesc[] = {
	{ "prog2.g17",					0x080000, 0x57c59972, 0x1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "prog1.f17",					0x080000, 0xbf2d9a26, 0x1 | BRF_PRG | BRF_ESS }, //  1
	{ "scr2.g16",					0x080000, 0x887b1bc5, 0x1 | BRF_PRG | BRF_ESS }, //  2
	{ "scr1.f16",					0x080000, 0xcbba3182, 0x1 | BRF_PRG | BRF_ESS }, //  3
	{ "scr4.g15",					0x080000, 0xce97e411, 0x1 | BRF_PRG | BRF_ESS }, //  4
	{ "scr3.f15",					0x080000, 0x480cc2e8, 0x1 | BRF_PRG | BRF_ESS }, //  5
	{ "scr6.g14",					0x080000, 0xb29d49de, 0x1 | BRF_PRG | BRF_ESS }, //  6
	{ "scr5.f14",					0x080000, 0xd5f88b83, 0x1 | BRF_PRG | BRF_ESS }, //  7
	{ "scr8.g20",					0x080000, 0x694ae2b3, 0x1 | BRF_PRG | BRF_ESS }, //  8
	{ "scr7.f20",					0x080000, 0x6068712c, 0x1 | BRF_PRG | BRF_ESS }, //  9

	{ "obj1.1i",					0x080000, 0x52e6872a, 0x2 | BRF_GRA },           // 10 Sprites
	{ "obj2.2i",					0x080000, 0xea6e3861, 0x2 | BRF_GRA },           // 11

	{ "music2.1b",					0x080000, 0x23cc4f9c, 0x2 | BRF_SND },           // 12 Samples
	{ "music1.1a",					0x080000, 0x864167c2, 0x2 | BRF_SND },           // 13
};

STD_ROM_PICK(fantsia2)
STD_ROM_FN(fantsia2)

struct BurnDriver BurnDrvFantsia2 = {
	"fantsia2", NULL, NULL, NULL, "1997",
	"Fantasia II (Explicit)\0", NULL, "Comad", "EXPRO-02",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, fantsia2RomInfo, fantsia2RomName, NULL, NULL, NULL, NULL, DrvInputInfo, Missw96DIPInfo,
	Fantsia2Init, DrvExit, DrvFrame, ComadDraw, DrvScan, &BurnRecalc, 0x800,
	256, 224, 4, 3
};


// Fantasia II (Less Explicit)

static struct BurnRomInfo fantsia2aRomDesc[] = {
	{ "fnt2-22.bin",				0x080000, 0xa3a92c4b, 0x1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "fnt2-17.bin",				0x080000, 0xd0ce4493, 0x1 | BRF_PRG | BRF_ESS }, //  1
	{ "fnt2-21.bin",				0x080000, 0xe989c2e7, 0x1 | BRF_PRG | BRF_ESS }, //  2
	{ "fnt2-16.bin",				0x080000, 0x8c06d372, 0x1 | BRF_PRG | BRF_ESS }, //  3
	{ "fnt2-20.bin",				0x080000, 0x6e9f1e65, 0x1 | BRF_PRG | BRF_ESS }, //  4
	{ "fnt2-15.bin",				0x080000, 0x85cbeb2b, 0x1 | BRF_PRG | BRF_ESS }, //  5
	{ "fnt2-19.bin",				0x080000, 0x7953226a, 0x1 | BRF_PRG | BRF_ESS }, //  6
	{ "fnt2-14.bin",				0x080000, 0x10d8ccff, 0x1 | BRF_PRG | BRF_ESS }, //  7
	{ "fnt2-18.bin",				0x080000, 0x4cdaeda3, 0x1 | BRF_PRG | BRF_ESS }, //  8
	{ "fnt2-13.bin",				0x080000, 0x68c7f042, 0x1 | BRF_PRG | BRF_ESS }, //  9

	{ "obj1.1i",					0x080000, 0x52e6872a, 0x2 | BRF_GRA },           // 10 Sprites
	{ "obj2.2i",					0x080000, 0xea6e3861, 0x2 | BRF_GRA },           // 11

	{ "music2.1b",					0x080000, 0x23cc4f9c, 0x2 | BRF_SND },           // 12 Samples
	{ "music1.1a",					0x080000, 0x864167c2, 0x2 | BRF_SND },           // 13
};

STD_ROM_PICK(fantsia2a)
STD_ROM_FN(fantsia2a)

struct BurnDriver BurnDrvFantsia2a = {
	"fantsia2a", "fantsia2", NULL, NULL, "1997",
	"Fantasia II (Less Explicit)\0", NULL, "Comad", "EXPRO-02",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, fantsia2aRomInfo, fantsia2aRomName, NULL, NULL, NULL, NULL, DrvInputInfo, Missw96DIPInfo,
	Fantsia2Init, DrvExit, DrvFrame, ComadDraw, DrvScan, &BurnRecalc, 0x800,
	256, 224, 4, 3
};


// Fantasia II (1998)

static struct BurnRomInfo fantsia2nRomDesc[] = {
	{ "prog2.g17",					0x080000, 0x57c59972, 0x1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "prog1.f17",					0x080000, 0xbf2d9a26, 0x1 | BRF_PRG | BRF_ESS }, //  1
	{ "scr2.g16",					0x080000, 0x887b1bc5, 0x1 | BRF_PRG | BRF_ESS }, //  2
	{ "scr1.f16",					0x080000, 0xcbba3182, 0x1 | BRF_PRG | BRF_ESS }, //  3
	{ "scr4.g15",					0x080000, 0xce97e411, 0x1 | BRF_PRG | BRF_ESS }, //  4
	{ "scr3.f15",					0x080000, 0x480cc2e8, 0x1 | BRF_PRG | BRF_ESS }, //  5
	{ "scr6.g14",					0x080000, 0xb29d49de, 0x1 | BRF_PRG | BRF_ESS }, //  6
	{ "scr5.f14",					0x080000, 0xd5f88b83, 0x1 | BRF_PRG | BRF_ESS }, //  7
	{ "scr8.g20",					0x080000, 0x694ae2b3, 0x1 | BRF_PRG | BRF_ESS }, //  8
	{ "scr7.f20",					0x080000, 0x6068712c, 0x1 | BRF_PRG | BRF_ESS }, //  9

	{ "23_obj1.u5",					0x080000, 0xb45c9234, 0x2 | BRF_GRA },           // 10 Sprites
	{ "obj2.2i",					0x080000, 0xea6e3861, 0x2 | BRF_GRA },           // 11

	{ "music2.1b",					0x080000, 0x23cc4f9c, 0x2 | BRF_SND },           // 12 Samples
	{ "music1.1a",					0x080000, 0x864167c2, 0x2 | BRF_SND },           // 13
};

STD_ROM_PICK(fantsia2n)
STD_ROM_FN(fantsia2n)

struct BurnDriver BurnDrvFantsia2n = {
	"fantsia2n", "fantsia2", NULL, NULL, "1998",
	"Fantasia II (1998)\0", NULL, "Comad", "EXPRO-02",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, fantsia2nRomInfo, fantsia2nRomName, NULL, NULL, NULL, NULL, DrvInputInfo, Missw96DIPInfo,
	Fantsia2Init, DrvExit, DrvFrame, ComadDraw, DrvScan, &BurnRecalc, 0x800,
	256, 224, 4, 3
};


// Super Model II

static struct BurnRomInfo supmodl2RomDesc[] = {
	{ "12_prog2.ue17",				0x080000, 0x9107f65d, 0x1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "7_prog1.ud17",				0x080000, 0x0d9253a7, 0x1 | BRF_PRG | BRF_ESS }, //  1
	{ "11_i-scr2.ue16b",			0x080000, 0xb836c1f3, 0x1 | BRF_PRG | BRF_ESS }, //  2
	{ "6_i-scr1.ue16a",				0x080000, 0xd56cac96, 0x1 | BRF_PRG | BRF_ESS }, //  3
	{ "10_i-scr4.ue15b",			0x080000, 0xaa85a247, 0x1 | BRF_PRG | BRF_ESS }, //  4
	{ "5_i-scr3.ue15a",				0x080000, 0x3e9bd17a, 0x1 | BRF_PRG | BRF_ESS }, //  5
	{ "9_i-scr6.ue14b",				0x080000, 0xd21355dc, 0x1 | BRF_PRG | BRF_ESS }, //  6
	{ "4_i-scr5.ue14a",				0x080000, 0xd1c9155c, 0x1 | BRF_PRG | BRF_ESS }, //  7
	{ "8_i-scr8.ue20b",				0x080000, 0x7ffe761e, 0x1 | BRF_PRG | BRF_ESS }, //  8
	{ "3_i-scr7.ue20a",				0x080000, 0xd2bb19ef, 0x1 | BRF_PRG | BRF_ESS }, //  9

	{ "13_obj1.u5",					0x080000, 0x52e6872a, 0x2 | BRF_GRA },           // 10 Sprites
	{ "14_obj2.u4",					0x080000, 0xea6e3861, 0x2 | BRF_GRA },           // 11

	{ "1_music1.ub6",				0x080000, 0x23cc4f9c, 0x2 | BRF_SND },           // 12 Samples
	{ "2_music2.uc6",				0x080000, 0x864167c2, 0x2 | BRF_SND },           // 13
};

STD_ROM_PICK(supmodl2)
STD_ROM_FN(supmodl2)

struct BurnDriver BurnDrvSupmodl2 = {
	"supmodl2", NULL, NULL, NULL, "1997",
	"Super Model II\0", NULL, "Comad", "EXPRO-02",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, supmodl2RomInfo, supmodl2RomName, NULL, NULL, NULL, NULL, DrvInputInfo, Missw96DIPInfo,
	Fantsia2Init, DrvExit, DrvFrame, ComadDraw, DrvScan, &BurnRecalc, 0x800,
	256, 224, 4, 3
};


// WOW New Fantasia (Explicit)

static struct BurnRomInfo wownfantRomDesc[] = {
	{ "ep-4001 42750001 u81.u81",	0x080000, 0x9942d200, 0x1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "ep-4001 42750001 u80.u80",	0x080000, 0x17359eeb, 0x1 | BRF_PRG | BRF_ESS }, //  1
	{ "ep-061 43750002 - 1.bin",	0x200000, 0xc318e841, 0x2 | BRF_PRG | BRF_ESS }, //  2
	{ "ep-061 43750002 - 2.bin",	0x200000, 0x8871dc3a, 0x2 | BRF_PRG | BRF_ESS }, //  3

	{ "ep-4001 42750001 u113.u113",	0x080000, 0x3e77ca1f, 0x2 | BRF_GRA },           //  4 Sprites
	{ "ep-4001 42750001 u112.u112",	0x080000, 0x51f4b604, 0x2 | BRF_GRA },           //  5

	{ "ep-4001 42750001 u4.u4",		0x080000, 0x06dc889e, 0x2 | BRF_SND },           //  6 Samples
	{ "ep-4001 42750001 u1.u1",		0x080000, 0x864167c2, 0x2 | BRF_SND },           //  7
};

STD_ROM_PICK(wownfant)
STD_ROM_FN(wownfant)

struct BurnDriver BurnDrvWownfant = {
	"wownfant", NULL, NULL, NULL, "2002",
	"WOW New Fantasia (Explicit)\0", NULL, "Comad", "EXPRO-02",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, wownfantRomInfo, wownfantRomName, NULL, NULL, NULL, NULL, DrvInputInfo, Missw96DIPInfo,
	Fantsia2Init, DrvExit, DrvFrame, ComadDraw, DrvScan, &BurnRecalc, 0x800,
	256, 224, 4, 3
};


// WOW New Fantasia

static struct BurnRomInfo wownfantaRomDesc[] = {
	{ "2.u81",						0x080000, 0x159178f8, 0x1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "1.u80",						0x080000, 0x509bc2d2, 0x1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.bin",						0x200000, 0x4d082ec1, 0x2 | BRF_PRG | BRF_ESS }, //  2
	{ "4.bin",						0x200000, 0xaee91094, 0x2 | BRF_PRG | BRF_ESS }, //  3

	{ "5.u113",						0x080000, 0x3e77ca1f, 0x2 | BRF_GRA },           //  4 Sprites
	{ "6.u112",						0x080000, 0x0013473e, 0x2 | BRF_GRA },           //  5

	{ "8.u4",						0x080000, 0x06dc889e, 0x2 | BRF_SND },           //  6 Samples
	{ "7.u1",						0x080000, 0x864167c2, 0x2 | BRF_SND },           //  7
};

STD_ROM_PICK(wownfanta)
STD_ROM_FN(wownfanta)

struct BurnDriver BurnDrvWownfanta = {
	"wownfanta", "wownfant", NULL, NULL, "2002",
	"WOW New Fantasia\0", NULL, "Comad", "EXPRO-02",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, wownfantaRomInfo, wownfantaRomName, NULL, NULL, NULL, NULL, DrvInputInfo, Missw96DIPInfo,
	Fantsia2Init, DrvExit, DrvFrame, ComadDraw, DrvScan, &BurnRecalc, 0x800,
	256, 224, 4, 3
};


// Miss World 2002

static struct BurnRomInfo missw02RomDesc[] = {
	{ "u81",						0x080000, 0x86ca4d5f, 0x1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "u80",						0x080000, 0x96d40592, 0x1 | BRF_PRG | BRF_ESS }, //  1
	{ "gfx1",						0x200000, 0xfdfe36ba, 0x2 | BRF_PRG | BRF_ESS }, //  2
	{ "gfx2",						0x200000, 0xaa769a81, 0x2 | BRF_PRG | BRF_ESS }, //  3

	{ "u113",						0x080000, 0x3e77ca1f, 0x2 | BRF_GRA },           //  4 Sprites
	{ "u112",						0x080000, 0x51f4b604, 0x2 | BRF_GRA },           //  5

	{ "u4",							0x080000, 0x06dc889e, 0x2 | BRF_SND },           //  6 Samples
	{ "u1",							0x080000, 0x864167c2, 0x2 | BRF_SND },           //  7
};

STD_ROM_PICK(missw02)
STD_ROM_FN(missw02)

struct BurnDriver BurnDrvMissw02 = {
	"missw02", NULL, NULL, NULL, "2002",
	"Miss World 2002\0", NULL, "Comad", "EXPRO-02",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, missw02RomInfo, missw02RomName, NULL, NULL, NULL, NULL, DrvInputInfo, Missw96DIPInfo,
	Fantsia2Init, DrvExit, DrvFrame, ComadDraw, DrvScan, &BurnRecalc, 0x800,
	256, 224, 4, 3
};


// Miss World 2002 (Daigom license)

static struct BurnRomInfo missw02dRomDesc[] = {
	{ "8.u81",						0x080000, 0x316666d0, 0x1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "7.u80",						0x080000, 0xd61f4d18, 0x1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.bin",						0x200000, 0xfdfe36ba, 0x2 | BRF_PRG | BRF_ESS }, //  2
	{ "4.bin",						0x200000, 0xaa769a81, 0x2 | BRF_PRG | BRF_ESS }, //  3

	{ "6.u113",						0x080000, 0x3e77ca1f, 0x2 | BRF_GRA },           //  4 Sprites
	{ "5.u112",						0x080000, 0xead3411d, 0x2 | BRF_GRA },           //  5

	{ "2.u4",						0x080000, 0x06dc889e, 0x2 | BRF_SND },           //  6 Samples
	{ "1.u1",						0x080000, 0x864167c2, 0x2 | BRF_SND },           //  7
};

STD_ROM_PICK(missw02d)
STD_ROM_FN(missw02d)

struct BurnDriver BurnDrvMissw02d = {
	"missw02d", "missw02", NULL, NULL, "2002",
	"Miss World 2002 (Daigom license)\0", NULL, "Daigom", "EXPRO-02",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, missw02dRomInfo, missw02dRomName, NULL, NULL, NULL, NULL, DrvInputInfo, Missw96DIPInfo,
	Fantsia2Init, DrvExit, DrvFrame, ComadDraw, DrvScan, &BurnRecalc, 0x800,
	256, 224, 4, 3
};


// Super Miss World

static struct BurnRomInfo smisswRomDesc[] = {
	{ "10_prog2.ue17",				0x080000, 0xe99e520f, 0x1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "6_prog1.ud17",				0x080000, 0x22831657, 0x1 | BRF_PRG | BRF_ESS }, //  1
	{ "9_im1-b.ue16b",				0x080000, 0xfff1eee4, 0x1 | BRF_PRG | BRF_ESS }, //  2
	{ "5_im1-a.ue16a",				0x080000, 0x2134a72d, 0x1 | BRF_PRG | BRF_ESS }, //  3
	{ "8_im2-b.ue15b",				0x080000, 0xcf44b638, 0x1 | BRF_PRG | BRF_ESS }, //  4
	{ "4_im2-a.ue15a",				0x080000, 0xd22b270f, 0x1 | BRF_PRG | BRF_ESS }, //  5
	{ "7_im3-b.ue14b",				0x080000, 0x12a9441d, 0x1 | BRF_PRG | BRF_ESS }, //  6
	{ "3_im3-a.ue14a",				0x080000, 0x8c656fc9, 0x1 | BRF_PRG | BRF_ESS }, //  7

	{ "15_obj11.u5",				0x080000, 0x3983152f, 0x2 | BRF_GRA },           //  8 Sprites

	{ "1_music1.ub6",				0x080000, 0xe78a659e, 0x2 | BRF_SND },           //  9 Samples
	{ "2_music2.uc6",				0x080000, 0x60fa0c00, 0x2 | BRF_SND },           // 10
};

STD_ROM_PICK(smissw)
STD_ROM_FN(smissw)

struct BurnDriver BurnDrvSmissw = {
	"smissw", NULL, NULL, NULL, "1996",
	"Super Miss World\0", NULL, "Comad", "EXPRO-02",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, smisswRomInfo, smisswRomName, NULL, NULL, NULL, NULL, DrvInputInfo, Missw96DIPInfo,
	SmisswInit, DrvExit, DrvFrame, ComadDraw, DrvScan, &BurnRecalc, 0x800,
	256, 224, 4, 3
};


// Gals Hustler

static struct BurnRomInfo galhustlRomDesc[] = {
	{ "ue17.3",						0x080000, 0xb2583dbb, 0x1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "ud17.4",						0x080000, 0x470a3668, 0x1 | BRF_PRG | BRF_ESS }, //  1

	{ "galhstl5.u5",				0x080000, 0x44a18f15, 0x2 | BRF_GRA },           //  2 Sprites

	{ "galhstl1.ub6",				0x080000, 0x23848790, 0x2 | BRF_SND },           //  3 Samples
	{ "galhstl2.uc6",				0x080000, 0x2168e54a, 0x2 | BRF_SND },           //  4
};

STD_ROM_PICK(galhustl)
STD_ROM_FN(galhustl)

struct BurnDriver BurnDrvGalhustl = {
	"galhustl", "pgalvip", NULL, NULL, "1997",
	"Gals Hustler\0", NULL, "ACE International", "EXPRO-02",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, galhustlRomInfo, galhustlRomName, NULL, NULL, NULL, NULL, DrvInputInfo, GalhustlDIPInfo,
	GalhustlInit, DrvExit, DrvFrame, ZipzapDraw, DrvScan, &BurnRecalc, 0x800,
	256, 224, 4, 3
};


// Pocket Gals V.I.P (set 1)

static struct BurnRomInfo pgalvipRomDesc[] = {
	{ "afega_15.ue17",				0x020000, 0x050060ca, 0x1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "afega_16.ud17",				0x020000, 0xd32e4052, 0x1 | BRF_PRG | BRF_ESS }, //  1
	{ "afega_13.rob1",				0x080000, 0xac51ef72, 0x1 | BRF_PRG | BRF_ESS }, //  2
	{ "afega_14.roa1",				0x080000, 0x0877c00f, 0x1 | BRF_PRG | BRF_ESS }, //  3

	{ "afega_17.u5",				0x080000, 0xa8a50745, 0x2 | BRF_GRA },           //  4 Sprites

	{ "afega_12.ub6",				0x020000, 0xd32a6c0c, 0x2 | BRF_SND },           //  5 Samples
	{ "afega_11.uc6",				0x080000, 0x2168e54a, 0x2 | BRF_SND },           //  6
};

STD_ROM_PICK(pgalvip)
STD_ROM_FN(pgalvip)

static INT32 PgalvipInit()
{
	INT32 nRet = GalhustlInit();

	if (nRet == 0) {
		memmove (Drv68KROM + 0x200000, Drv68KROM + 0x040000, 0x100000);
	}

	return nRet;
}

struct BurnDriver BurnDrvPgalvip = {
	"pgalvip", NULL, NULL, NULL, "1996",
	"Pocket Gals V.I.P (set 1)\0", NULL, "ACE International / Afega", "EXPRO-02",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, pgalvipRomInfo, pgalvipRomName, NULL, NULL, NULL, NULL, GalhustlInputInfo, GalhustlDIPInfo,
	PgalvipInit, DrvExit, DrvFrame, ZipzapDraw, DrvScan, &BurnRecalc, 0x800,
	256, 224, 4, 3
};


// Pocket Gals V.I.P (set 2)

static struct BurnRomInfo pgalvipaRomDesc[] = {
	{ "pgalvip_3.ue17",				0x080000, 0xa48e8255, 0x1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "pgalvip_4.ud17",				0x080000, 0x829a2085, 0x1 | BRF_PRG | BRF_ESS }, //  1

	{ "pgalvip_5.u5",				0x080000, 0x2d6e5a90, 0x2 | BRF_GRA },           //  2 Sprites

	{ "pgalvip_1.ub6",				0x020000, 0xd32a6c0c, 0x2 | BRF_SND },           //  3 Samples
	{ "pgalvip_2.uc6",				0x080000, 0x2168e54a, 0x2 | BRF_SND },           //  4
};

STD_ROM_PICK(pgalvipa)
STD_ROM_FN(pgalvipa)

struct BurnDriver BurnDrvPgalvipa = {
	"pgalvipa", "pgalvip", NULL, NULL, "1997",
	"Pocket Gals V.I.P (set 2)\0", NULL, "<unknown>", "EXPRO-02",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, pgalvipaRomInfo, pgalvipaRomName, NULL, NULL, NULL, NULL, GalhustlInputInfo, GalhustlDIPInfo,
	GalhustlInit, DrvExit, DrvFrame, ZipzapDraw, DrvScan, &BurnRecalc, 0x800,
	256, 224, 4, 3
};


// Zip & Zap (Explicit)

static struct BurnRomInfo zipzapRomDesc[] = {
	{ "ue17.bin",					0x040000, 0xda6c3fc8, 0x1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "ud17.bin",					0x040000, 0x2901fae1, 0x1 | BRF_PRG | BRF_ESS }, //  1
	{ "937.bin",					0x080000, 0x61dd653f, 0x5 | BRF_PRG | BRF_ESS }, //  2
	{ "941.bin",					0x080000, 0x320321ed, 0x5 | BRF_PRG | BRF_ESS }, //  3
	{ "936.bin",					0x080000, 0x596543cc, 0x5 | BRF_PRG | BRF_ESS }, //  4
	{ "940.bin",					0x080000, 0x0c9dfb53, 0x5 | BRF_PRG | BRF_ESS }, //  5
	{ "934.bin",					0x080000, 0x1e65988a, 0x5 | BRF_PRG | BRF_ESS }, //  6
	{ "939.bin",					0x080000, 0x8790a6a3, 0x5 | BRF_PRG | BRF_ESS }, //  7
	{ "938.bin",					0x080000, 0x61c06b60, 0x5 | BRF_PRG | BRF_ESS }, //  8
	{ "942.bin",					0x080000, 0x282413b8, 0x5 | BRF_PRG | BRF_ESS }, //  9

	{ "u5.bin",						0x080000, 0xc274d8b5, 0x2 | BRF_GRA },           // 10 Sprites

	{ "snd.bin",					0x080000, 0xbc20423e, 0x2 | BRF_SND },           // 11 Samples
};

STD_ROM_PICK(zipzap)
STD_ROM_FN(zipzap)

struct BurnDriver BurnDrvZipzap = {
	"zipzap", NULL, NULL, NULL, "1995",
	"Zip & Zap (Explicit)\0", NULL, "Barko Corp", "EXPRO-02",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, zipzapRomInfo, zipzapRomName, NULL, NULL, NULL, NULL, GalhustlInputInfo, ZipzapDIPInfo,
	ZipzapInit, DrvExit, DrvFrame, ZipzapDraw, DrvScan, &BurnRecalc, 0x800,
	224, 256, 3, 4
};


// Zip & Zap (Less Explicit)

static struct BurnRomInfo zipzapaRomDesc[] = {
	{ "31.ue17",					0x020000, 0xe098ef98, 0x1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "30.ud17",					0x020000, 0x769ec252, 0x1 | BRF_PRG | BRF_ESS }, //  1
	{ "37.rd2b",					0x080000, 0x0b59d718, 0x5 | BRF_PRG | BRF_ESS }, //  2
	{ "33.rd2a",					0x080000, 0xdf35c8f5, 0x5 | BRF_PRG | BRF_ESS }, //  3
	{ "38.rd3b",					0x080000, 0x575dfc8c, 0x5 | BRF_PRG | BRF_ESS }, //  4
	{ "34.rd3a",					0x080000, 0xf8bd156b, 0x5 | BRF_PRG | BRF_ESS }, //  5
	{ "39",							0x080000, 0x302375c0, 0x5 | BRF_PRG | BRF_ESS }, //  6
	{ "35",							0x080000, 0x9b6409a6, 0x5 | BRF_PRG | BRF_ESS }, //  7
	{ "36.rd1b",					0x080000, 0xf3256bbb, 0x5 | BRF_PRG | BRF_ESS }, //  8
	{ "32.rd1a",					0x080000, 0x1dd511a9, 0x5 | BRF_PRG | BRF_ESS }, //  9

	{ "40.u5",						0x080000, 0xc274d8b5, 0x2 | BRF_GRA },           // 10 Sprites

	{ "29.ub6",						0x020000, 0x2cf3b8ea, 0x2 | BRF_SND },           // 11 Samples
	{ "28.uc6",						0x080000, 0xe5d026c7, 0x2 | BRF_SND },           // 12
};

STD_ROM_PICK(zipzapa)
STD_ROM_FN(zipzapa)

struct BurnDriver BurnDrvZipzapa = {
	"zipzapa", "zipzap", NULL, NULL, "1995",
	"Zip & Zap (Less Explicit)\0", NULL, "Barko Corp", "EXPRO-02",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, zipzapaRomInfo, zipzapaRomName, NULL, NULL, NULL, NULL, GalhustlInputInfo, ZipzapDIPInfo,
	ZipzapInit, DrvExit, DrvFrame, ZipzapDraw, DrvScan, &BurnRecalc, 0x800,
	224, 256, 3, 4
};
