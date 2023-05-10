// FB Alpha Gaelco hardware driver module
// Based on MAME driver by Manuel Abadia with various bits by Nicola Salmoria and Andreas Naive

// thanks to research by Haze & peterferrie, thoop stg. 4 crash is fixed!
// first Haze came up with a ram patch, next day peterferrie did some deeper
// debugging and found all's needed is refresh rate of 57.3 - 57.7.  congrats guys!

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "m6809_intf.h"
#include "msm6295.h"
#include "burn_ym3812.h"
#include "gaelco_crypt.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *Drv6809ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvSndROM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvVidRegs;
static UINT8 *Drv6809RAM;

static UINT8 *soundlatch;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 nExtraCycles[1];

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvDips[3];
static UINT8 DrvReset;
static UINT16 DrvInputs[3];

static INT32 nOkiBank;

static INT32 gaelco_encryption_param1;
static INT32 has_sound_cpu = 0;
static INT32 sprite_highpri_color;

static struct BurnInputInfo DrvInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Drv)

static struct BurnInputInfo BigkarnkInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 0,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Bigkarnk)

static struct BurnInputInfo LastkmInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 coin"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Lastkm)

static struct BurnDIPInfo BigkarnkDIPList[]=
{
	DIP_OFFSET(0x12)
	{0x00, 0xff, 0xff, 0xce, NULL				}, // dip 0/1 swapped compared to other games
	{0x01, 0xff, 0xff, 0xff, NULL				},
	{0x02, 0xff, 0xff, 0xfe, NULL				},

	{0   , 0xfe, 0   ,    8, "Difficulty"			},
	{0x00, 0x01, 0x07, 0x07, "0"				},
	{0x00, 0x01, 0x07, 0x06, "1"				},
	{0x00, 0x01, 0x07, 0x05, "2"				},
	{0x00, 0x01, 0x07, 0x04, "3"				},
	{0x00, 0x01, 0x07, 0x03, "4"				},
	{0x00, 0x01, 0x07, 0x02, "5"				},
	{0x00, 0x01, 0x07, 0x01, "6"				},
	{0x00, 0x01, 0x07, 0x00, "7"				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x00, 0x01, 0x18, 0x18, "1"				},
	{0x00, 0x01, 0x18, 0x10, "2"				},
	{0x00, 0x01, 0x18, 0x08, "3"				},
	{0x00, 0x01, 0x18, 0x00, "4"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x00, 0x01, 0x20, 0x20, "Off"				},
	{0x00, 0x01, 0x20, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Impact"			},
	{0x00, 0x01, 0x40, 0x00, "Off"				},
	{0x00, 0x01, 0x40, 0x40, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x00, 0x01, 0x80, 0x80, "Off"				},
	{0x00, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    11, "Coin A"			},
	{0x01, 0x01, 0x0f, 0x07, "4 Coins 1 Credits"		},
	{0x01, 0x01, 0x0f, 0x08, "3 Coins 1 Credits"		},
	{0x01, 0x01, 0x0f, 0x09, "2 Coins 1 Credits"		},
	{0x01, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"		},
	{0x01, 0x01, 0x0f, 0x06, "2 Coins 3 Credits"		},
	{0x01, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"		},
	{0x01, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"		},
	{0x01, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"		},
	{0x01, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"		},
	{0x01, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"		},
	{0x01, 0x01, 0x0f, 0x00, "Free Play (if Coin B too)"	},

	{0   , 0xfe, 0   ,    11, "Coin B"			},
	{0x01, 0x01, 0xf0, 0x70, "4 Coins 1 Credits"		},
	{0x01, 0x01, 0xf0, 0x80, "3 Coins 1 Credits"		},
	{0x01, 0x01, 0xf0, 0x90, "2 Coins 1 Credits"		},
	{0x01, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x01, 0x01, 0xf0, 0x60, "2 Coins 3 Credits"		},
	{0x01, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"		},
	{0x01, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"		},
	{0x01, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"		},
	{0x01, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"		},
	{0x01, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"		},
	{0x01, 0x01, 0xf0, 0x00, "Free Play (if Coin A too)"	},

	{0   , 0xfe, 0   ,    2, "Go to test mode now"		},
	{0x02, 0x01, 0x02, 0x02, "Off"				},
	{0x02, 0x01, 0x02, 0x00, "On"				},
};

STDDIPINFO(Bigkarnk)

static struct BurnDIPInfo ManiacsqDIPList[]=
{
	{0x11, 0xff, 0xff, 0xff, NULL				},
	{0x12, 0xff, 0xff, 0xf5, NULL				},

	{0   , 0xfe, 0   ,    11, "Coin A"			},
	{0x11, 0x01, 0x0f, 0x07, "4 Coins 1 Credits"		},
	{0x11, 0x01, 0x0f, 0x08, "3 Coins 1 Credits"		},
	{0x11, 0x01, 0x0f, 0x09, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x0f, 0x06, "2 Coins 3 Credits"		},
	{0x11, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"		},
	{0x11, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"		},
	{0x11, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"		},
	{0x11, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"		},
	{0x11, 0x01, 0x0f, 0x00, "Free Play (if Coin B too)"	},

	{0   , 0xfe, 0   ,    11, "Coin B"			},
	{0x11, 0x01, 0xf0, 0x70, "4 Coins 1 Credits"		},
	{0x11, 0x01, 0xf0, 0x80, "3 Coins 1 Credits"		},
	{0x11, 0x01, 0xf0, 0x90, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0xf0, 0x60, "2 Coins 3 Credits"		},
	{0x11, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"		},
	{0x11, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"		},
	{0x11, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"		},
	{0x11, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"		},
	{0x11, 0x01, 0xf0, 0x00, "Free Play (if Coin A too)"	},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x12, 0x01, 0x01, 0x01, "Off"				},
	{0x12, 0x01, 0x01, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x12, 0x01, 0x04, 0x00, "Off"				},
	{0x12, 0x01, 0x04, 0x04, "On"				},

	{0   , 0xfe, 0   ,    2, "Sound Type"			},
	{0x12, 0x01, 0x08, 0x00, "Stereo"			},
	{0x12, 0x01, 0x08, 0x08, "Mono"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x12, 0x01, 0xc0, 0x40, "Easy"				},
	{0x12, 0x01, 0xc0, 0xc0, "Normal"			},
	{0x12, 0x01, 0xc0, 0x80, "Hard"				},
	{0x12, 0x01, 0xc0, 0x00, "Hardest"			},
};

STDDIPINFO(Maniacsq)

static struct BurnDIPInfo BiomtoyDIPList[]=
{
	{0x11, 0xff, 0xff, 0xff, NULL				},
	{0x12, 0xff, 0xff, 0xfb, NULL				},

	{0   , 0xfe, 0   ,    11, "Coin A"			},
	{0x11, 0x01, 0x0f, 0x07, "4 Coins 1 Credits"		},
	{0x11, 0x01, 0x0f, 0x08, "3 Coins 1 Credits"		},
	{0x11, 0x01, 0x0f, 0x09, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x0f, 0x06, "2 Coins 3 Credits"		},
	{0x11, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"		},
	{0x11, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"		},
	{0x11, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"		},
	{0x11, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"		},
	{0x11, 0x01, 0x0f, 0x00, "Free Play (if Coin B too)"	},

	{0   , 0xfe, 0   ,    11, "Coin B"			},
	{0x11, 0x01, 0xf0, 0x70, "4 Coins 1 Credits"		},
	{0x11, 0x01, 0xf0, 0x80, "3 Coins 1 Credits"		},
	{0x11, 0x01, 0xf0, 0x90, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0xf0, 0x60, "2 Coins 3 Credits"		},
	{0x11, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"		},
	{0x11, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"		},
	{0x11, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"		},
	{0x11, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"		},
	{0x11, 0x01, 0xf0, 0x00, "Free Play (if Coin A too)"	},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x12, 0x01, 0x01, 0x01, "Off"				},
	{0x12, 0x01, 0x01, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x12, 0x01, 0x08, 0x00, "Off"				},
	{0x12, 0x01, 0x08, 0x08, "On"				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x12, 0x01, 0x30, 0x20, "0"				},
	{0x12, 0x01, 0x30, 0x10, "1"				},
	{0x12, 0x01, 0x30, 0x30, "2"				},
	{0x12, 0x01, 0x30, 0x00, "3"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x12, 0x01, 0xc0, 0x40, "Easy"				},
	{0x12, 0x01, 0xc0, 0xc0, "Normal"			},
	{0x12, 0x01, 0xc0, 0x80, "Hard"				},
	{0x12, 0x01, 0xc0, 0x00, "Hardest"			},
};

STDDIPINFO(Biomtoy)

static struct BurnDIPInfo BiomtoycDIPList[]=
{
	{0x11, 0xff, 0xff, 0xff, NULL				},
	{0x12, 0xff, 0xff, 0xfb, NULL				},

	{0   , 0xfe, 0   ,    11, "Coin B"			},
	{0x11, 0x01, 0x0f, 0x0e, "4 Coins 1 Credits"		},
	{0x11, 0x01, 0x0f, 0x0a, "3 Coins 2 Credits"		},
	{0x11, 0x01, 0x0f, 0x09, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x0f, 0x07, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0x0f, 0x06, "2 Coins 3 Credits"		},
	{0x11, 0x01, 0x0f, 0x0b, "1 Coin  3 Credits"		},
	{0x11, 0x01, 0x0f, 0x0d, "1 Coin  5 Credits"		},
	{0x11, 0x01, 0x0f, 0x08, "Free Play (if Coin A too)"	},
	{0x11, 0x01, 0x0f, 0x0c, "Free Play (if Coin A too)"	},
	{0x11, 0x01, 0x0f, 0x00, "Free Play (if Coin A too)"	},

	{0   , 0xfe, 0   ,    11, "Coin A"			},
	{0x11, 0x01, 0xf0, 0xe0, "4 Coins 1 Credits"		},
	{0x11, 0x01, 0xf0, 0xa0, "3 Coins 2 Credits"		},
	{0x11, 0x01, 0xf0, 0x90, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0xf0, 0x70, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0xf0, 0x60, "2 Coins 3 Credits"		},
	{0x11, 0x01, 0xf0, 0xb0, "1 Coin  3 Credits"		},
	{0x11, 0x01, 0xf0, 0xd0, "1 Coin  5 Credits"		},
	{0x11, 0x01, 0xf0, 0x80, "Free Play (if Coin B too)"	},
	{0x11, 0x01, 0xf0, 0xc0, "Free Play (if Coin B too)"	},
	{0x11, 0x01, 0xf0, 0x00, "Free Play (if Coin B too)"	},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x12, 0x01, 0x01, 0x01, "Off"				},
	{0x12, 0x01, 0x01, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x12, 0x01, 0x08, 0x00, "Off"				},
	{0x12, 0x01, 0x08, 0x08, "On"				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x12, 0x01, 0x30, 0x20, "0"				},
	{0x12, 0x01, 0x30, 0x10, "1"				},
	{0x12, 0x01, 0x30, 0x30, "2"				},
	{0x12, 0x01, 0x30, 0x00, "3"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x12, 0x01, 0xc0, 0x40, "Easy"				},
	{0x12, 0x01, 0xc0, 0xc0, "Normal"			},
	{0x12, 0x01, 0xc0, 0x80, "Hard"				},
	{0x12, 0x01, 0xc0, 0x00, "Hardest"			},
};

STDDIPINFO(Biomtoyc)

static struct BurnDIPInfo BioplaycDIPList[]=
{
	{0x11, 0xff, 0xff, 0xff, NULL				},
	{0x12, 0xff, 0xff, 0xf3, NULL				},

	{0   , 0xfe, 0   ,    11, "Coin B"			},
	{0x11, 0x01, 0x0f, 0x0e, "4 Coins 1 Credits"		},
	{0x11, 0x01, 0x0f, 0x0a, "3 Coins 2 Credits"		},
	{0x11, 0x01, 0x0f, 0x09, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x0f, 0x07, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0x0f, 0x06, "2 Coins 3 Credits"		},
	{0x11, 0x01, 0x0f, 0x0b, "1 Coin  3 Credits"		},
	{0x11, 0x01, 0x0f, 0x0d, "1 Coin  5 Credits"		},
	{0x11, 0x01, 0x0f, 0x08, "Free Play (if Coin A too)"	},
	{0x11, 0x01, 0x0f, 0x0c, "Free Play (if Coin A too)"	},
	{0x11, 0x01, 0x0f, 0x00, "Free Play (if Coin A too)"	},

	{0   , 0xfe, 0   ,    11, "Coin A"			},
	{0x11, 0x01, 0xf0, 0xe0, "4 Coins 1 Credits"		},
	{0x11, 0x01, 0xf0, 0xa0, "3 Coins 2 Credits"		},
	{0x11, 0x01, 0xf0, 0x90, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0xf0, 0x70, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0xf0, 0x60, "2 Coins 3 Credits"		},
	{0x11, 0x01, 0xf0, 0xb0, "1 Coin  3 Credits"		},
	{0x11, 0x01, 0xf0, 0xd0, "1 Coin  5 Credits"		},
	{0x11, 0x01, 0xf0, 0x80, "Free Play (if Coin B too)"	},
	{0x11, 0x01, 0xf0, 0xc0, "Free Play (if Coin B too)"	},
	{0x11, 0x01, 0xf0, 0x00, "Free Play (if Coin B too)"	},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x12, 0x01, 0x01, 0x00, "Off"				},
	{0x12, 0x01, 0x01, 0x01, "On"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x12, 0x01, 0x04, 0x00, "Off"				},
	{0x12, 0x01, 0x04, 0x04, "On"				},

	{0   , 0xfe, 0   ,    2, "Unknown"			},
	{0x12, 0x01, 0x08, 0x08, "Off"				},
	{0x12, 0x01, 0x08, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x12, 0x01, 0x30, 0x10, "1"				},
	{0x12, 0x01, 0x30, 0x30, "2"				},
	{0x12, 0x01, 0x30, 0x20, "3"				},
	{0x12, 0x01, 0x30, 0x00, "4"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x12, 0x01, 0xc0, 0x40, "Easy"				},
	{0x12, 0x01, 0xc0, 0xc0, "Normal"			},
	{0x12, 0x01, 0xc0, 0x80, "Hard"				},
	{0x12, 0x01, 0xc0, 0x00, "Hardest"			},
};

STDDIPINFO(Bioplayc)


static struct BurnDIPInfo SquashDIPList[]=
{
	{0x11, 0xff, 0xff, 0xff, NULL				},
	{0x12, 0xff, 0xff, 0xdf, NULL				},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x11, 0x01, 0x07, 0x02, "6 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x03, "5 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x04, "4 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x05, "3 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x06, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x01, "3 Coins 2 Credits"		},
	{0x11, 0x01, 0x07, 0x00, "4 Coins 3 Credits"		},
	{0x11, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x11, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x38, 0x00, "3 Coins 4 Credits"		},
	{0x11, 0x01, 0x38, 0x08, "2 Coins 3 Credits"		},
	{0x11, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x11, 0x01, 0x38, 0x20, "1 Coin  4 Credits"		},
	{0x11, 0x01, 0x38, 0x18, "1 Coin  5 Credits"		},
	{0x11, 0x01, 0x38, 0x10, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    2, "2 Player Continue"		},
	{0x11, 0x01, 0x40, 0x40, "2 Credits / 5 Games"		},
	{0x11, 0x01, 0x40, 0x00, "1 Credit / 3 Games"		},

	{0   , 0xfe, 0   ,    2, "Free Play"			},
	{0x11, 0x01, 0x80, 0x80, "Off"				},
	{0x11, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x12, 0x01, 0x03, 0x02, "Easy"				},
	{0x12, 0x01, 0x03, 0x03, "Normal"			},
	{0x12, 0x01, 0x03, 0x01, "Hard"				},
	{0x12, 0x01, 0x03, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    4, "Number of Faults"		},
	{0x12, 0x01, 0x0c, 0x08, "4"				},
	{0x12, 0x01, 0x0c, 0x0c, "5"				},
	{0x12, 0x01, 0x0c, 0x04, "6"				},
	{0x12, 0x01, 0x0c, 0x00, "7"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x12, 0x01, 0x20, 0x20, "Off"				},
	{0x12, 0x01, 0x20, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x12, 0x01, 0x80, 0x80, "Off"				},
	{0x12, 0x01, 0x80, 0x00, "On"				},
};

STDDIPINFO(Squash)

static struct BurnDIPInfo ThoopDIPList[]=
{
	{0x11, 0xff, 0xff, 0xff, NULL				},
	{0x12, 0xff, 0xff, 0xcf, NULL				},

	{0   , 0xfe, 0   ,    11, "Coin A"			},
	{0x11, 0x01, 0x0f, 0x07, "4 Coins 1 Credits"		},
	{0x11, 0x01, 0x0f, 0x08, "3 Coins 1 Credits"		},
	{0x11, 0x01, 0x0f, 0x09, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x0f, 0x06, "2 Coins 3 Credits"		},
	{0x11, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"		},
	{0x11, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"		},
	{0x11, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"		},
	{0x11, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"		},
	{0x11, 0x01, 0x0f, 0x00, "Free Play (if Coin B too)"	},

	{0   , 0xfe, 0   ,    2, "2 Cr. Start, 1 Continue"	},
	{0x11, 0x01, 0x40, 0x40, "Off"				},
	{0x11, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x12, 0x01, 0x03, 0x03, "Easy"				},
	{0x12, 0x01, 0x03, 0x02, "Normal"			},
	{0x12, 0x01, 0x03, 0x01, "Hard"				},
	{0x12, 0x01, 0x03, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    2, "Player Controls"		},
	{0x12, 0x01, 0x04, 0x04, "2 Joysticks"			},
	{0x12, 0x01, 0x04, 0x00, "1 Joystick"			},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x12, 0x01, 0x18, 0x00, "4"				},
	{0x12, 0x01, 0x18, 0x08, "3"				},
	{0x12, 0x01, 0x18, 0x10, "2"				},
	{0x12, 0x01, 0x18, 0x18, "1"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x12, 0x01, 0x20, 0x20, "Off"				},
	{0x12, 0x01, 0x20, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x12, 0x01, 0x40, 0x40, "Upright"			},
	{0x12, 0x01, 0x40, 0x00, "Cocktail"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x12, 0x01, 0x80, 0x80, "Off"				},
	{0x12, 0x01, 0x80, 0x00, "On"				},
};

STDDIPINFO(Thoop)

static struct BurnDIPInfo LastkmDIPList[]=
{
	{0x05, 0xff, 0xff, 0xff, NULL				},
	{0x06, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    11, "Coin A"			},
	{0x05, 0x01, 0xf0, 0x70, "1 Coin  2 Credits"		},
	{0x05, 0x01, 0xf0, 0x80, "Free Play"			},
	{0x05, 0x01, 0xf0, 0xd0, "Free Play (duplicate 1)"	},
	{0x05, 0x01, 0xf0, 0x00, "Free Play (duplicate 2)"	},
	{0x05, 0x01, 0xf0, 0xe0, "4 Coins 1 Credits"		},
	{0x05, 0x01, 0xf0, 0xb0, "3 Coins 2 Credits"		},
	{0x05, 0x01, 0xf0, 0x90, "2 Coins 1 Credits"		},
	{0x05, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x05, 0x01, 0xf0, 0x60, "2 Coins 3 Credits"		},
	{0x05, 0x01, 0xf0, 0xc0, "1 Coin  3 Credits"		},
	{0x05, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x06, 0x01, 0x01, 0x01, "Off"				},
	{0x06, 0x01, 0x01, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Master"			},
	{0x06, 0x01, 0x80, 0x80, "No"				},
	{0x06, 0x01, 0x80, 0x00, "Yes"				},
};

STDDIPINFO(Lastkm)

static void oki_bankswitch(INT32 data)
{
	nOkiBank = data & 0x0f;

	MSM6295SetBank(0, DrvSndROM, 0x00000, 0x2ffff);
	MSM6295SetBank(0, DrvSndROM + ((data & 0x0f) * 0x10000), 0x30000, 0x3ffff);
}

static void palette_write(INT32 offset)
{
	offset = offset & 0x7fe;

	UINT16 p = BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvPalRAM + offset)));

	INT32 r = (p >>  0) & 0x1f;
	INT32 g = (p >>  5) & 0x1f;
	INT32 b = (p >> 10) & 0x1f;

	r = (r << 3) | (r >> 2);
	g = (g << 3) | (g >> 2);
	b = (b << 3) | (b >> 2);

	DrvPalette[offset/2] = BurnHighCol(r, g, b, 0);
}

static void __fastcall main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xffc000) == 0x100000) {
		*((UINT16*)(DrvVidRAM + (address & 0x3ffe))) = gaelco_decrypt((address & 0x3ffe)/2, data, gaelco_encryption_param1, 0x4228);
		return;
	}

	switch (address)
	{
		case 0x108000:
		case 0x108001:
		case 0x108002:
		case 0x108003:
		case 0x108004:
		case 0x108005:
		case 0x108006:
		case 0x108007:
			*((UINT16*)(DrvVidRegs + (address & 0x06))) = BURN_ENDIAN_SWAP_INT16(data);
		return;

		case 0x10800c:
		case 0x10800d:
			// watchdog
		return;

		case 0x70000c:
		case 0x70000d:
			oki_bankswitch(data);
		return;

		case 0x70000e:
		case 0x70000f:
			MSM6295Write(0, data);
		return;
	}
}

static void __fastcall main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xffc000) == 0x100000) {
		return;	// encrypted ram write
	}

	switch (address)
	{
		case 0x108000:
		case 0x108001:
		case 0x108002:
		case 0x108003:
		case 0x108004:
		case 0x108005:
		case 0x108006:
		case 0x108007:
			//DrvVidRegs[(address & 0x07)^1] = data;
		return;

		case 0x10800c:
		case 0x10800d:
			// watchdog
		return;

		case 0x70000c:
		case 0x70000d:
			oki_bankswitch(data);
		return;

		case 0x70000e:
		case 0x70000f:
			if (has_sound_cpu) {
				*soundlatch = data;
				M6809SetIRQLine(1, CPU_IRQSTATUS_AUTO);
			} else {
				MSM6295Write(0, data);
			}
		return;
	}
}

static UINT16 __fastcall main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x700000:
			return DrvDips[1];

		case 0x700002:
			return DrvDips[0];

		case 0x700004:
			return DrvInputs[0];

		case 0x700006:
			return DrvInputs[1];

		case 0x700008:
			return DrvInputs[2];

		case 0x70000e:
			return MSM6295Read(0);
	}

	return 0;
}

static UINT8 __fastcall main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x700001:
			return DrvDips[1];

		case 0x700003:
			return DrvDips[0];

		case 0x700005:
			return DrvInputs[0];

		case 0x700007:
			return DrvInputs[1];

		case 0x700009:
			return DrvInputs[2];

		case 0x70000f:
			return MSM6295Read(0);
	}

	return 0;
}

static void __fastcall palette_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfff800) == 0x200000) {
		*((UINT16 *)(DrvPalRAM + (address & 0x7fe))) = BURN_ENDIAN_SWAP_INT16(data);
		palette_write(address);
		return;
	}
}

static void __fastcall palette_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xfff800) == 0x200000) {
		DrvPalRAM[(address & 0x7ff) ^ 1] = data;
		palette_write(address);
		return;
	}
}

static void sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x0800:
		case 0x0801:
			MSM6295Write(0, data);
		return;

		case 0x0a00:
		case 0x0a01:
			BurnYM3812Write(0, address & 1, data);
		return;
	}
}

static UINT8 sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x0800:
		case 0x0801:
			return MSM6295Read(0);

		case 0x0a00:
		case 0x0a01:
			return BurnYM3812Read(0, address & 1);

		case 0x0b00:
			return *soundlatch;
	}

	return 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	M6809Open(0);
	M6809Reset();
	BurnYM3812Reset();
	M6809Close();

	MSM6295Reset(0);

	oki_bankswitch(3);

	nExtraCycles[0] = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM	= Next; Next += 0x100000;
	Drv6809ROM	= Next; Next += 0x010000;

	DrvGfxROM0	= Next; Next += 0x400000;
	DrvGfxROM1	= Next; Next += 0x400000;

	DrvSndROM	= Next; Next += 0x140000;

	AllRam		= Next;

	DrvPalRAM	= Next; Next += 0x000800;
	Drv68KRAM	= Next; Next += 0x010000;
	DrvVidRAM	= Next; Next += 0x004000;
	DrvSprRAM	= Next; Next += 0x001000;

	DrvVidRegs	= Next; Next += 0x000008;

	Drv6809RAM	= Next; Next += 0x000800;

	soundlatch	= Next; Next += 0x000001;

	RamEnd		= Next;

	DrvPalette	= (UINT32*)Next; Next += 0x0400 * sizeof(UINT32);

	MemEnd		= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[4] = { 0x0000000, 0x0800000, 0x1000000, 0x1800000 };
	INT32 Plane1[4] = { 0x0400000, 0x0c00000, 0x1400000, 0x1c00000 };
	INT32 XOffs[16] = { 0,1,2,3,4,5,6,7, 16*8+0,16*8+1,16*8+2,16*8+3,16*8+4,16*8+5,16*8+6,16*8+7 };
	INT32 YOffs[16] = { 0*8,1*8,2*8,3*8,4*8,5*8,6*8,7*8, 8*8,9*8,10*8,11*8,12*8,13*8,14*8,15*8 };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x400000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x400000);

	GfxDecode(0x10000, 4,  8,  8, Plane0, XOffs, YOffs, 0x040, tmp, DrvGfxROM0);
	GfxDecode(0x04000, 4, 16, 16, Plane1, XOffs, YOffs, 0x100, tmp, DrvGfxROM1);

	BurnFree (tmp);

	return 0;
}

static tilemap_callback( screen0 )
{
	UINT16 *ram = (UINT16*)(DrvVidRAM + 0x0000);

	INT32 attr0 = BURN_ENDIAN_SWAP_INT16(ram[offs * 2 + 0]);
	INT32 attr1 = BURN_ENDIAN_SWAP_INT16(ram[offs * 2 + 1]);

	INT32 flags = TILE_FLIPYX(attr0 & 0x0003) | TILE_GROUP((attr1 >> 6) & 3);

	TILE_SET_INFO(1, (attr0 & 0xfffc) >> 2, (attr1 & 0x003f), flags);
}

static tilemap_callback( screen1 )
{
	UINT16 *ram = (UINT16*)(DrvVidRAM + 0x1000);

	INT32 attr0 = BURN_ENDIAN_SWAP_INT16(ram[offs * 2 + 0]);
	INT32 attr1 = BURN_ENDIAN_SWAP_INT16(ram[offs * 2 + 1]);

	INT32 flags = TILE_FLIPYX(attr0 & 0x0003) | TILE_GROUP((attr1 >> 6) & 3);

	TILE_SET_INFO(1, (attr0 & 0xfffc) >> 2, (attr1 & 0x003f), flags);
}

static INT32 DrvInit(INT32 (*pRomLoadCallback)(), INT32 encrypted_ram, INT32 sound_cpu)
{
	BurnAllocMemIndex();

	BurnSetRefreshRate(57.42); // Thoop wants this

	if (pRomLoadCallback) {
		if (pRomLoadCallback()) return 1;

		DrvGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(DrvVidRAM,		0x100000, 0x103fff, encrypted_ram ? MAP_ROM : MAP_RAM);
	SekMapMemory(DrvPalRAM,		0x200000, 0x2007ff, MAP_ROM);
	SekMapMemory(DrvSprRAM,		0x440000, 0x440fff, MAP_RAM);
	SekMapMemory(Drv68KRAM,		0xff0000, 0xffffff, MAP_RAM);
	SekSetWriteWordHandler(0,	main_write_word);
	SekSetWriteByteHandler(0,	main_write_byte);
	SekSetReadWordHandler(0,	main_read_word);
	SekSetReadByteHandler(0,	main_read_byte);

	SekMapHandler(1,		0x200000, 0x2007FF, MAP_WRITE);
	SekSetWriteWordHandler(1,	palette_write_word);
	SekSetWriteByteHandler(1,	palette_write_byte);
	SekClose();

	has_sound_cpu = sound_cpu ? 1 : 0;

	// big karnak
	{
		M6809Init(0);
		M6809Open(0);
		M6809MapMemory(Drv6809RAM,		0x0000, 0x07ff, MAP_RAM);
		M6809MapMemory(Drv6809ROM + 0x0c00,	0x0c00, 0xffff, MAP_ROM);
		M6809SetReadHandler(sound_read);
		M6809SetWriteHandler(sound_write);
		M6809Close();

		BurnYM3812Init(1, 4000000, NULL, 0);
		BurnTimerAttachYM3812(&M6809Config, 2216750);
		BurnYM3812SetRoute(0, BURN_SND_YM3812_ROUTE, 1.00, BURN_SND_ROUTE_BOTH);
	}

	MSM6295Init(0, 1000000 / MSM6295_PIN7_HIGH, has_sound_cpu ? 1 : 0);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	gaelco_encryption_param1 = encrypted_ram;

	sprite_highpri_color = 0x38; // default

	GenericTilesInit();

	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, screen0_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, screen1_map_callback, 16, 16, 32, 32);
	GenericTilemapSetGfx(1, DrvGfxROM1, 4, 16, 16, 0x400000, 0, 0x3f);

	DrvDoReset();

	return 0;
}

static void DrvGfxReorder()
{
	for (INT32 i = 0; i < 0x400000; i++) {
		DrvGfxROM0[(i & 0xf3ffff) | ((i & 0x80000) >> 1) | ((i & 0x40000) << 1)] = DrvGfxROM1[i];
	}
}

static INT32 ThoopRomLoad()
{
	if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;

	if (BurnLoadRom(DrvGfxROM1 + 0x300000,  2, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x200000,  3, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x100000,  4, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x000000,  5, 1)) return 1;

	DrvGfxReorder();

	if (BurnLoadRom(DrvSndROM  + 0x000000,  6, 1)) return 1;

	return 0;
}

static INT32 SquashRomLoad()
{
	if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x000000,  2, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x080000,  2, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x100000,  3, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x180000,  3, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x200000,  4, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x280000,  4, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x300000,  5, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x380000,  5, 1)) return 1;

	if (BurnLoadRom(DrvSndROM  + 0x000000,  6, 1)) return 1;
	if (BurnLoadRom(DrvSndROM  + 0x080000,  6, 1)) return 1;

	return 0;
}

static INT32 BiomtoyRomLoad()
{
	if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;

	if (BurnLoadRom(DrvGfxROM1 + 0x000000,  3, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x080000,  2, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x100000,  5, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x180000,  4, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x200000,  7, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x280000,  6, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x300000,  9, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x380000,  8, 1)) return 1;

	DrvGfxReorder();

	if (BurnLoadRom(DrvSndROM  + 0x000000, 10, 1)) return 1;
	if (BurnLoadRom(DrvSndROM  + 0x080000, 11, 1)) return 1;

	return 0;
}

static INT32 BigkarnkRomLoad()
{
	if (BurnLoadRom(Drv6809ROM + 0x000000,  7, 1)) return 1;

	return SquashRomLoad();
}

static INT32 ThoopInit()
{
	return DrvInit(ThoopRomLoad,	0x0e, 0);
}

static INT32 SquashInit()
{
	INT32 rc = DrvInit(SquashRomLoad,	0x0f, 0);
	if (!rc) {
		sprite_highpri_color = 0x3c;
	}
	return rc;
}

static INT32 BiomtoyInit()
{
	return DrvInit(BiomtoyRomLoad,	0 /* Ram is not encrypted! */, 0);
}

static INT32 ManiacspInit()
{
	return DrvInit(SquashRomLoad,	0 /* Ram is not encrypted! */, 0);
}

static INT32 BigkarnkInit()
{
	return DrvInit(BigkarnkRomLoad,	0 /* Ram is not encrypted! */, 1 /* M6809 Sound CPU */);
}

static INT32 DrvExit()
{
	GenericTilesExit();

	BurnYM3812Exit();
	MSM6295Exit(0);

	SekExit();
	M6809Exit();

	BurnFreeMemIndex();

	return 0;
}

static void draw_sprites()
{
	UINT16 *spriteram = (UINT16*)DrvSprRAM;

	for (INT32 i = 0x800 - 4 - 1; i >= 3; i -= 4)
	{
		INT32 sx = BURN_ENDIAN_SWAP_INT16(spriteram[i + 2]) & 0x01ff;
		INT32 sy = (240 - (BURN_ENDIAN_SWAP_INT16(spriteram[i]) & 0x00ff)) & 0x00ff;
		INT32 number = BURN_ENDIAN_SWAP_INT16(spriteram[i + 3]);
		INT32 color = (BURN_ENDIAN_SWAP_INT16(spriteram[i + 2]) & 0x7e00) >> 9;
		INT32 attr = (BURN_ENDIAN_SWAP_INT16(spriteram[i]) & 0xfe00) >> 9;
		INT32 priority = (BURN_ENDIAN_SWAP_INT16(spriteram[i]) & 0x3000) >> 12;

		INT32 xflip = attr & 0x20;
		INT32 yflip = attr & 0x40;
		INT32 spr_size, pri_mask;

		if (color >= sprite_highpri_color) priority = 4;

		switch (priority)
		{
			case 0: pri_mask = 0xff00; break;
			case 1: pri_mask = 0xfff0; break;
			case 2: pri_mask = 0xfffc; break;
			case 3: pri_mask = 0xfffe; break;
			default:
			case 4: pri_mask = 0; break;
		}

		if (attr & 0x04)
			spr_size = 1;
		else
		{
			spr_size = 2;
			number &= ~3;
		}

		for (INT32 y = 0; y < spr_size; y++)
		{
			for (INT32 x = 0; x < spr_size; x++)
			{
				INT32 ex = xflip ? (spr_size - 1 - x) : x;
				INT32 ey = yflip ? (spr_size - 1 - y) : y;

				RenderPrioSprite(pTransDraw, DrvGfxROM0, number + (ex * 2) + ey, (color * 16), 0, sx-0x0f+x*8, sy+y*8-16, xflip, yflip, 8, 8, pri_mask);
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x800; i+=2) {
			palette_write(i);
		}
		DrvRecalc = 0;
	}

	BurnTransferClear();

	UINT16 *reg = (UINT16*)DrvVidRegs;

	GenericTilemapSetScrollY(0, (BURN_ENDIAN_SWAP_INT16(reg[0]) + 16));
	GenericTilemapSetScrollX(0, (BURN_ENDIAN_SWAP_INT16(reg[1]) + 4));
	GenericTilemapSetScrollY(1, (BURN_ENDIAN_SWAP_INT16(reg[2]) + 16));
	GenericTilemapSetScrollX(1, (BURN_ENDIAN_SWAP_INT16(reg[3]) + 0));

	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetTransparent(1, 0);

	GenericTilemapDraw(1, pTransDraw, TMAP_SET_GROUP(3) | 0);
	GenericTilemapDraw(0, pTransDraw, TMAP_SET_GROUP(3) | 0);

	GenericTilemapDraw(1, pTransDraw, TMAP_SET_GROUP(2) | 1);
	GenericTilemapDraw(0, pTransDraw, TMAP_SET_GROUP(2) | 1);

	GenericTilemapDraw(1, pTransDraw, TMAP_SET_GROUP(1) | 2);
	GenericTilemapDraw(0, pTransDraw, TMAP_SET_GROUP(1) | 2);

	GenericTilemapDraw(1, pTransDraw, TMAP_SET_GROUP(0) | 4);
	GenericTilemapDraw(0, pTransDraw, TMAP_SET_GROUP(0) | 4);

	draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void draw_layer(INT32 layer, INT32 mask, INT32 category, INT32 priority)
{
	GenericTilemapSetTransMask(layer, 0, mask);

	GenericTilemapDraw(layer, pTransDraw, priority | TMAP_SET_GROUP(category));
}

static INT32 BigkarnkDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x800; i+=2) {
			palette_write(i);
		}
		DrvRecalc = 0;
	}

	BurnTransferClear();

	UINT16 *reg = (UINT16*)DrvVidRegs;

	GenericTilemapSetScrollY(0, (BURN_ENDIAN_SWAP_INT16(reg[0]) + 16));
	GenericTilemapSetScrollX(0, (BURN_ENDIAN_SWAP_INT16(reg[1]) + 4));
	GenericTilemapSetScrollY(1, (BURN_ENDIAN_SWAP_INT16(reg[2]) + 16));
	GenericTilemapSetScrollX(1, (BURN_ENDIAN_SWAP_INT16(reg[3]) + 0));

	draw_layer(1, 0x00ff, 3, 0);
	draw_layer(0, 0x00ff, 3, 0);
	draw_layer(1, 0xff01, 3, 1);
	draw_layer(0, 0xff01, 3, 1);
	draw_layer(1, 0x00ff, 2, 1);
	draw_layer(0, 0x00ff, 2, 1);
	draw_layer(1, 0xff01, 2, 2);
	draw_layer(0, 0xff01, 2, 2);
	draw_layer(1, 0x00ff, 1, 2);
	draw_layer(0, 0x00ff, 1, 2);
	draw_layer(1, 0xff01, 1, 4);
	draw_layer(0, 0xff01, 1, 4);
	draw_layer(1, 0x00ff, 0, 4);
	draw_layer(0, 0x00ff, 0, 4);
	draw_layer(1, 0xff01, 0, 8);
	draw_layer(0, 0xff01, 0, 8);

	draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, 3 * sizeof(UINT16));
		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 512;
	INT32 nCyclesTotal[1] =  { (INT32)(12000000 / 57.42) };
	INT32 nCyclesDone[1] = { nExtraCycles[0] };

	SekOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		if (i == 256) {
			SekSetIRQLine(6, CPU_IRQSTATUS_AUTO);
		}

		CPU_RUN(0, Sek);
	}

	SekClose();

	nExtraCycles[0] = nCyclesDone[0] - nCyclesTotal[0];

	if (pBurnSoundOut) {
		MSM6295Render(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 BigkarnkFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	M6809NewFrame();

	{
		memset (DrvInputs, 0xff, 3 * sizeof(UINT16));
		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		DrvInputs[2] = (DrvInputs[2] & ~0x02) | (DrvDips[2] & 0x02);
	}

	INT32 nInterleave = 512;
	INT32 nCyclesTotal[2] =  { (INT32)(10000000 / 57.42), (INT32)(2216750 / 57.42) };
	INT32 nCyclesDone[2] = { nExtraCycles[0], 0 };

	SekOpen(0);
	M6809Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		if (i == 256) {
			SekSetIRQLine(6, CPU_IRQSTATUS_AUTO);
		}

		CPU_RUN(0, Sek);
		CPU_RUN_TIMER_YM3812(1);
	}

	SekClose();
	M6809Close();

	nExtraCycles[0] = nCyclesDone[0] - nCyclesTotal[0];

	if (pBurnSoundOut) {
		BurnYM3812Update(pBurnSoundOut, nBurnSoundLen);
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
		BurnSoundDCFilter(); // big karnak distortion w/lp filter
	}

	if (pBurnDraw) {
		BigkarnkDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin != NULL) {
		*pnMin = 0x029698;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);
		M6809Scan(nAction);

		BurnYM3812Scan(nAction, pnMin);
		MSM6295Scan(nAction, pnMin);

		SCAN_VAR(nOkiBank);

		SCAN_VAR(nExtraCycles);
	}

	if (nAction & ACB_WRITE) {
		oki_bankswitch(nOkiBank);
	}

	return 0;
}


// Big Karnak (ver. 1.0, checksum 1e38c94)
// PCB silkscreened REF.901112 

static struct BurnRomInfo bigkarnkRomDesc[] = {
	{ "d16",		0x40000, 0x44fb9c73, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "d19",		0x40000, 0xff79dfdd, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "h5",			0x80000, 0x20e239ff, 2 | BRF_GRA },           //  2 Tiles and Sprites
	{ "h10",		0x80000, 0xab442855, 2 | BRF_GRA },           //  3
	{ "h8",			0x80000, 0x83dce5a3, 2 | BRF_GRA },           //  4
	{ "h6",			0x80000, 0x24e84b24, 2 | BRF_GRA },           //  5

	{ "d1",			0x40000, 0x26444ad1, 3 | BRF_SND },           //  6 M6295 Samples

	{ "d5",			0x10000, 0x3b73b9c5, 4 | BRF_PRG | BRF_ESS }, //  7 M6809 Code
	
	{ "bigkarnak_gal16v8.d6", 0x00117, 0x587fe895, 5 | BRF_OPT }, //  8 plds
	{ "bigkarnak_gal20v8.d6", 0x00157, 0x0dcb286e, 5 | BRF_OPT }, //  9
};

STD_ROM_PICK(bigkarnk)
STD_ROM_FN(bigkarnk)

struct BurnDriver BurnDrvBigkarnk = {
	"bigkarnk", NULL, NULL, NULL, "1991",
	"Big Karnak (ver. 1.0, checksum 1e38c94)\0", NULL, "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_PLATFORM | GBF_SCRFIGHT, 0,
	NULL, bigkarnkRomInfo, bigkarnkRomName, NULL, NULL, NULL, NULL, BigkarnkInputInfo, BigkarnkDIPInfo,
	BigkarnkInit, DrvExit, BigkarnkFrame, BigkarnkDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};


// Maniac Square (ver 1.0, checksum b602, prototype)

static struct BurnRomInfo maniacspRomDesc[] = {
	{ "d18",		0x20000, 0x740ecab2, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "d16",		0x20000, 0xc6c42729, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "f3",			0x40000, 0xe7f6582b, 2 | BRF_GRA },           //  2 Tiles and Sprites
	{ "f2",			0x40000, 0xca43a5ae, 2 | BRF_GRA },           //  3
	{ "f1",			0x40000, 0xfca112e8, 2 | BRF_GRA },           //  4
	{ "f0",			0x40000, 0x6e829ee8, 2 | BRF_GRA },           //  5

	{ "c1",			0x80000, 0x2557f2d6, 3 | BRF_SND },           //  6 M6295 Samples
};

STD_ROM_PICK(maniacsp)
STD_ROM_FN(maniacsp)

struct BurnDriver BurnDrvManiacsp = {
	"maniacsp", "maniacsq", NULL, NULL, "1996",
	"Maniac Square (ver 1.0, checksum b602, prototype)\0", NULL, "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, maniacspRomInfo, maniacspRomName, NULL, NULL, NULL, NULL, DrvInputInfo, ManiacsqDIPInfo,
	ManiacspInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};


// Biomechanical Toy (ver. 1.0.1885, checksum 69f5e032)
// PCB - REF.922804/2 

static struct BurnRomInfo biomtoyRomDesc[] = {
	{ "18.d18",		0x80000, 0x4569ce64, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "16.d16",		0x80000, 0x739449bd, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "h6",			0x80000, 0x9416a729, 2 | BRF_GRA },           //  2 Tiles and Sprites
	{ "j6",			0x80000, 0xe923728b, 2 | BRF_GRA },           //  3
	{ "h7",			0x80000, 0x9c984d7b, 2 | BRF_GRA },           //  4
	{ "j7",			0x80000, 0x0e18fac2, 2 | BRF_GRA },           //  5
	{ "h9",			0x80000, 0x8c1f6718, 2 | BRF_GRA },           //  6
	{ "j9",			0x80000, 0x1c93f050, 2 | BRF_GRA },           //  7
	{ "h10",		0x80000, 0xaca1702b, 2 | BRF_GRA },           //  8
	{ "j10",		0x80000, 0x8e3e96cc, 2 | BRF_GRA },           //  9

	{ "c1",			0x80000, 0x0f02de7e, 3 | BRF_SND },           // 10 M6295 Samples
	{ "c3",			0x80000, 0x914e4bbc, 3 | BRF_SND },           // 11
};

STD_ROM_PICK(biomtoy)
STD_ROM_FN(biomtoy)

struct BurnDriver BurnDrvBiomtoy = {
	"biomtoy", NULL, NULL, NULL, "1995",
	"Biomechanical Toy (ver. 1.0.1885, checksum 69f5e032)\0", NULL, "Gaelco / Zeus", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_RUNGUN, 0,
	NULL, biomtoyRomInfo, biomtoyRomName, NULL, NULL, NULL, NULL, DrvInputInfo, BiomtoyDIPInfo,
	BiomtoyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};


// Biomechanical Toy (ver. 1.0.1884, checksum 3f316c70)
// PCB - REF.922804/2 

static struct BurnRomInfo biomtoyaRomDesc[] = {
	{ "18.d18",		0x80000, 0x39b6cdbd, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "16.d16",		0x80000, 0xab340671, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "h6",			0x80000, 0x9416a729, 2 | BRF_GRA },           //  2 Tiles and Sprites
	{ "j6",			0x80000, 0xe923728b, 2 | BRF_GRA },           //  3
	{ "h7",			0x80000, 0x9c984d7b, 2 | BRF_GRA },           //  4
	{ "j7",			0x80000, 0x0e18fac2, 2 | BRF_GRA },           //  5
	{ "h9",			0x80000, 0x8c1f6718, 2 | BRF_GRA },           //  6
	{ "j9",			0x80000, 0x1c93f050, 2 | BRF_GRA },           //  7
	{ "h10",		0x80000, 0xaca1702b, 2 | BRF_GRA },           //  8
	{ "j10",		0x80000, 0x8e3e96cc, 2 | BRF_GRA },           //  9

	{ "c1",			0x80000, 0xedf77532, 3 | BRF_SND },           // 10 M6295 Samples
	{ "c3",			0x80000, 0xc3aea660, 3 | BRF_SND },           // 11
};

STD_ROM_PICK(biomtoya)
STD_ROM_FN(biomtoya)

struct BurnDriver BurnDrvBiomtoya = {
	"biomtoya", "biomtoy", NULL, NULL, "1995",
	"Biomechanical Toy (ver. 1.0.1884, checksum 3f316c70)\0", NULL, "Gaelco / Zeus", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_RUNGUN, 0,
	NULL, biomtoyaRomInfo, biomtoyaRomName, NULL, NULL, NULL, NULL, DrvInputInfo, BiomtoyDIPInfo,
	BiomtoyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};


// Biomechanical Toy (ver. 1.0.1878, checksum d84b28ff)
// PCB - REF.922804/2 

static struct BurnRomInfo biomtoybRomDesc[] = {
	{ "18.d18",		0x80000, 0x2dfadee3, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "16.d16",		0x80000, 0xb35e3ca6, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "h6",			0x80000, 0x9416a729, 2 | BRF_GRA },           //  2 Tiles and Sprites
	{ "j6",			0x80000, 0xe923728b, 2 | BRF_GRA },           //  3
	{ "h7",			0x80000, 0x9c984d7b, 2 | BRF_GRA },           //  4
	{ "j7",			0x80000, 0x0e18fac2, 2 | BRF_GRA },           //  5
	{ "h9",			0x80000, 0x8c1f6718, 2 | BRF_GRA },           //  6
	{ "j9",			0x80000, 0x1c93f050, 2 | BRF_GRA },           //  7
	{ "h10",		0x80000, 0xaca1702b, 2 | BRF_GRA },           //  8
	{ "j10",		0x80000, 0x8e3e96cc, 2 | BRF_GRA },           //  9

	{ "c1",			0x80000, 0xedf77532, 3 | BRF_SND },           // 10 M6295 Samples
	{ "c3",			0x80000, 0xc3aea660, 3 | BRF_SND },           // 11
};

STD_ROM_PICK(biomtoyb)
STD_ROM_FN(biomtoyb)

struct BurnDriver BurnDrvBiomtoyb = {
	"biomtoyb", "biomtoy", NULL, NULL, "1995",
	"Biomechanical Toy (ver. 1.0.1878, checksum d84b28ff)\0", NULL, "Gaelco / Zeus", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_RUNGUN, 0,
	NULL, biomtoybRomInfo, biomtoybRomName, NULL, NULL, NULL, NULL, DrvInputInfo, BiomtoyDIPInfo,
	BiomtoyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};


// Biomechanical Toy (ver. 1.0.1870, checksum ba682195)
// PCB - REF.922804/1 or REF.922804/2

static struct BurnRomInfo biomtoycRomDesc[] = {
	{ "program18.d18",		0x80000, 0x05ad7d30, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "program16.d16",		0x80000, 0xa288e73f, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "gfx6.h6",			0x80000, 0xab19a1ce, 2 | BRF_GRA },           //  2 Tiles and Sprites
	{ "gfx10.j6",			0x80000, 0x7b2dc36c, 2 | BRF_GRA },           //  3
	{ "gfx7.h7",			0x80000, 0x4bc82598, 2 | BRF_GRA },           //  4
	{ "gfx11.j7",			0x80000, 0xaff7fd0e, 2 | BRF_GRA },           //  5
	{ "gfx8.h9",			0x80000, 0x09de4799, 2 | BRF_GRA },           //  6
	{ "gfx12.j9",			0x80000, 0x7b27b2a9, 2 | BRF_GRA },           //  7
	{ "gfx9.h10",			0x80000, 0x38bcd72d, 2 | BRF_GRA },           //  8
	{ "gfx13.j10",			0x80000, 0x52c984df, 2 | BRF_GRA },           //  9

	{ "sound1.c1",			0x80000, 0xedf77532, 3 | BRF_SND },           // 10 M6295 Samples
	{ "sound2.c3",			0x80000, 0xc3aea660, 3 | BRF_SND },           // 11
};

STD_ROM_PICK(biomtoyc)
STD_ROM_FN(biomtoyc)

struct BurnDriver BurnDrvBiomtoyc = {
	"biomtoyc", "biomtoy", NULL, NULL, "1995",
	"Biomechanical Toy (ver. 1.0.1870, checksum ba682195)\0", NULL, "Gaelco / Zeus", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_RUNGUN, 0,
	NULL, biomtoycRomInfo, biomtoycRomName, NULL, NULL, NULL, NULL, DrvInputInfo, BiomtoycDIPInfo,
	BiomtoyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};


// Bioplaything Cop (ver. 1.0.1823, checksum cd960fc9, prototype)
// PCB - REF.922804/2?? - Spanish version

static struct BurnRomInfo bioplaycRomDesc[] = {
	// copyright based on Ver. 1.0.1870
	{ "t.d18",				0x80000, 0xec518c6c, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "t.d16",				0x80000, 0xde4b031d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "toy-high-3.h6",		0x80000, 0xab19a1ce, 2 | BRF_GRA },           //  2 Tiles and Sprites
	{ "toy-low-3.j6",		0x80000, 0x927f5cd7, 2 | BRF_GRA },           //  3
	{ "toy-high-2.h7",		0x80000, 0xfd975d89, 2 | BRF_GRA },           //  4
	{ "toy-low-2.j7",		0x80000, 0x6cbf9937, 2 | BRF_GRA },           //  5
	{ "toy-high-1.h9",		0x80000, 0x09de4799, 2 | BRF_GRA },           //  6
	{ "toy-low-1.j9",		0x80000, 0x57922c41, 2 | BRF_GRA },           //  7
	{ "toy-high-0.h10",		0x80000, 0x5bee6df7, 2 | BRF_GRA },           //  8
	{ "toy-low-0.j10",		0x80000, 0x26c49ca2, 2 | BRF_GRA },           //  9

	{ "c1",					0x80000, 0xedf77532, 3 | BRF_SND },           // 10 M6295 Samples
	{ "c3",					0x80000, 0xc3aea660, 3 | BRF_SND },           // 11
};

STD_ROM_PICK(bioplayc)
STD_ROM_FN(bioplayc)

struct BurnDriver BurnDrvBioplayc = {
	"bioplayc", "biomtoy", NULL, NULL, "1995",
	"Bioplaything Cop (ver. 1.0.1823, checksum cd960fc9, prototype)\0", NULL, "Gaelco / Zeus", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_RUNGUN, 0,
	NULL, bioplaycRomInfo, bioplaycRomName, NULL, NULL, NULL, NULL, DrvInputInfo, BioplaycDIPInfo,
	BiomtoyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};


// Squash (ver. 1.0, checksum 015aef61)
// PCB - REF.922804/1 

static struct BurnRomInfo squashRomDesc[] = {
	{ "squash.d18",			0x20000, 0xce7aae96, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "squash.d16",			0x20000, 0x8ffaedd7, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "squash.c12",			0x80000, 0x5c440645, 2 | BRF_GRA },           //  2 Tiles and Sprites
	{ "squash.c11",			0x80000, 0x9e19694d, 2 | BRF_GRA },           //  3
	{ "squash.c10",			0x80000, 0x892a035c, 2 | BRF_GRA },           //  4
	{ "squash.c09",			0x80000, 0x0bb91c69, 2 | BRF_GRA },           //  5 

	{ "squash.d01",			0x80000, 0xa1b9651b, 3 | BRF_SND },           //  6 M6295 Samples
	
	{ "squashv1_gal16v8.f2",	0x00117, 0xd5ed5985, 4 | BRF_OPT },       //  7 plds
	{ "squashv1_gal16v8.j16",	0x00117, 0xfe78b903, 4 | BRF_OPT },       //  8
	{ "squashv1_gal20v8.d21",	0x00157, 0xa715e392, 4 | BRF_OPT },       //  9
	{ "squashv1_gal16v8.h11",	0x00157, 0x51e34bc2, 4 | BRF_OPT },       // 10
};

STD_ROM_PICK(squash)
STD_ROM_FN(squash)

struct BurnDriver BurnDrvSquash = {
	"squash", NULL, NULL, NULL, "1992",
	"Squash (ver. 1.0, checksum 015aef61)\0", NULL, "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, squashRomInfo, squashRomName, NULL, NULL, NULL, NULL, DrvInputInfo, SquashDIPInfo,
	SquashInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};

/*
There is a Thunder Hoop on a REF.922804/2 PCB, with exactly the same ROM contents,
but on a different chips layout / capacity:
   program.d16             th161eb4.020            IDENTICAL
   program.d18             th18dea1.040            IDENTICAL
   gfx.j10                 c09          [1/2]      IDENTICAL
   gfx.j6                  c12          [1/2]      IDENTICAL
   gfx.j7                  c11          [1/2]      IDENTICAL
   gfx.j9                  c10          [1/2]      IDENTICAL
   sound.c1                sound        [1/2]      IDENTICAL
   gfx.h10                 c09          [2/2]      IDENTICAL
   gfx.h6                  c12          [2/2]      IDENTICAL
   gfx.h7                  c11          [2/2]      IDENTICAL
   gfx.h9                  c10          [2/2]      IDENTICAL
   sound.c3                sound        [2/2]      IDENTICAL
*/

// Thunder Hoop (ver. 1, checksum 02a09f7d)
// PCB - REF.922804/1 

static struct BurnRomInfo thoopRomDesc[] = {
	{ "th18dea1.040",		0x080000, 0x59bad625, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "th161eb4.020",		0x040000, 0x6add61ed, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "c09",				0x100000, 0x06f0edbf, 2 | BRF_GRA },           //  2 Tiles and Sprites
	{ "c10",				0x100000, 0x2d227085, 2 | BRF_GRA },           //  3
	{ "c11",				0x100000, 0x7403ef7e, 2 | BRF_GRA },           //  4
	{ "c12",				0x100000, 0x29a5ca36, 2 | BRF_GRA },           //  5

	{ "sound",				0x100000, 0x99f80961, 3 | BRF_SND },           //  6 M6295 Samples
	
	{ "thunderhoop_gal16v8.f2",	 0x00117, 0xd5ed5985, 4 | BRF_OPT },       //  7 plds
	{ "thunderhoop_gal16v8.j16", 0x00117, 0xfe78b903, 4 | BRF_OPT },       //  8
	{ "thunderhoop_gal20v8.d21", 0x00157, 0xa715e392, 4 | BRF_OPT },       //  9
	{ "thunderhoop_gal16v8.h11", 0x00157, 0x51e34bc2, 4 | BRF_OPT },        // 10
};

STD_ROM_PICK(thoop)
STD_ROM_FN(thoop)

struct BurnDriver BurnDrvThoop = {
	"thoop", NULL, NULL, NULL, "1992",
	"Thunder Hoop (ver. 1, checksum 02a09f7d)\0", NULL, "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_RUNGUN, 0,
	NULL, thoopRomInfo, thoopRomName, NULL, NULL, NULL, NULL, DrvInputInfo, ThoopDIPInfo,
	ThoopInit, DrvExit, DrvFrame, BigkarnkDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};


// Last KM (ver 1.0.0275, checksum 13bff751, prototype)

static struct BurnRomInfo lastkmRomDesc[] = {
	{ "prog-bici-e-8.11.95.d18",	0x80000, 0x1fc5fba0, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "prog-bici-o-8.11.95.d16",	0x80000, 0xb93e57e3, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "bici-f3.h6",			0x80000, 0x0bf9f213, 2 | BRF_GRA },           //  2 Tiles and Sprites
	{ "bici-f2.h7",			0x80000, 0xc48d5376, 2 | BRF_GRA },           //  3
	{ "bici-f1.h9",			0x80000, 0xe7958070, 2 | BRF_GRA },           //  4
	{ "bici-f0.h10",		0x80000, 0x73d4b29f, 2 | BRF_GRA },           //  5

	{ "sonido-bici-0-8.11.95.c1",	0x80000, 0x7380c963, 3 | BRF_SND },           //  6 M6295 Samples
};

STD_ROM_PICK(lastkm)
STD_ROM_FN(lastkm)

struct BurnDriver BurnDrvLastkm = {
	"lastkm", NULL, NULL, NULL, "1995",
	"Last KM (ver 1.0.0275, checksum 13bff751, prototype)\0", NULL, "Gaelco / Zeus", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_RACING, 0,
	NULL, lastkmRomInfo, lastkmRomName, NULL, NULL, NULL, NULL, LastkmInputInfo, LastkmDIPInfo,
	ManiacspInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};
