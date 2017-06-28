// FB Alpha Side Arms driver module
// Based on MAME driver by Paul Leaman

#include "tiles_generic.h"
#include "z80_intf.h"
#include "burn_ym2203.h"
#include "burn_ym2151.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvStarMap;
static UINT8 *DrvTileMap;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprBuf;
static UINT8 *DrvSprRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 *bgscrollx;
static UINT8 *bgscrolly;

static UINT8 flipscreen;
static UINT8 soundlatch;
static UINT8 starfield_enable;
static UINT8 character_enable;
static UINT8 sprite_enable;
static UINT8 bglayer_enable;
static UINT8 bank_data;

static UINT16 starscrollx;
static UINT16 starscrolly;

static INT32 hflop_74a;

static INT32 enable_watchdog;
static INT32 watchdog;
static INT32 vblank;
static INT32 is_whizz = 0; // uses ym2151 instead of ym2203
static INT32 is_turtshipk = 0;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvJoy5[8];
static UINT8 DrvDips[4];
static UINT8 DrvInputs[5];
static UINT8 DrvReset;

static struct BurnInputInfo SidearmsInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Sidearms)

static struct BurnInputInfo TurtshipInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 5,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Turtship)

static struct BurnInputInfo DygerInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
};

STDINPUTINFO(Dyger)

static struct BurnInputInfo WhizzInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy4 + 1,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy5 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy5 + 1,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy2 + 4,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",		BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};

STDINPUTINFO(Whizz)

static struct BurnDIPInfo SidearmsDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0xfc, NULL			},
	{0x15, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x13, 0x01, 0x08, 0x08, "Off"			},
	{0x13, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    8, "Difficulty"		},
	{0x14, 0x01, 0x07, 0x07, "0 (Easiest)"		},
	{0x14, 0x01, 0x07, 0x06, "1"			},
	{0x14, 0x01, 0x07, 0x05, "2"			},
	{0x14, 0x01, 0x07, 0x04, "3 (Normal)"		},
	{0x14, 0x01, 0x07, 0x03, "4"			},
	{0x14, 0x01, 0x07, 0x02, "5"			},
	{0x14, 0x01, 0x07, 0x01, "6"			},
	{0x14, 0x01, 0x07, 0x00, "7 (Hardest)"		},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x14, 0x01, 0x08, 0x08, "3"			},
	{0x14, 0x01, 0x08, 0x00, "5"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x14, 0x01, 0x30, 0x30, "100000"		},
	{0x14, 0x01, 0x30, 0x20, "100000 100000"	},
	{0x14, 0x01, 0x30, 0x10, "150000 150000"	},
	{0x14, 0x01, 0x30, 0x00, "200000 200000"	},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x14, 0x01, 0x40, 0x40, "Off"			},
	{0x14, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x15, 0x01, 0x07, 0x00, "4 Coins 1 Credits"	},
	{0x15, 0x01, 0x07, 0x01, "3 Coins 1 Credits"	},
	{0x15, 0x01, 0x07, 0x02, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x15, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x15, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x15, 0x01, 0x07, 0x04, "1 Coin  4 Credits"	},
	{0x15, 0x01, 0x07, 0x03, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x15, 0x01, 0x38, 0x00, "4 Coins 1 Credits"	},
	{0x15, 0x01, 0x38, 0x08, "3 Coins 1 Credits"	},
	{0x15, 0x01, 0x38, 0x10, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},
	{0x15, 0x01, 0x38, 0x30, "1 Coin  2 Credits"	},
	{0x15, 0x01, 0x38, 0x28, "1 Coin  3 Credits"	},
	{0x15, 0x01, 0x38, 0x20, "1 Coin  4 Credits"	},
	{0x15, 0x01, 0x38, 0x18, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x15, 0x01, 0x40, 0x00, "No"			},
	{0x15, 0x01, 0x40, 0x40, "Yes"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x15, 0x01, 0x80, 0x00, "Off"			},
	{0x15, 0x01, 0x80, 0x80, "On"			},
};

STDDIPINFO(Sidearms)

static struct BurnDIPInfo TurtshipDIPList[]=
{
	{0x14, 0xff, 0xff, 0xbd, NULL			},
	{0x15, 0xff, 0xff, 0xeb, NULL			},

	{0   , 0xfe, 0   ,    2, "Invulnerability"	},
	{0x14, 0x01, 0x01, 0x01, "Off"			},
	{0x14, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x14, 0x01, 0x02, 0x02, "No"			},
	{0x14, 0x01, 0x02, 0x00, "Yes"			},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x14, 0x01, 0x04, 0x04, "Off"			},
	{0x14, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x14, 0x01, 0x08, 0x08, "Off"			},
	{0x14, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x14, 0x01, 0x10, 0x10, "Normal"		},
	{0x14, 0x01, 0x10, 0x00, "Hard"			},

	{0   , 0xfe, 0   ,    8, "Lives"		},
	{0x14, 0x01, 0xe0, 0xe0, "1"			},
	{0x14, 0x01, 0xe0, 0x60, "2"			},
	{0x14, 0x01, 0xe0, 0xa0, "3"			},
	{0x14, 0x01, 0xe0, 0x20, "4"			},
	{0x14, 0x01, 0xe0, 0xc0, "5"			},
	{0x14, 0x01, 0xe0, 0x40, "6"			},
	{0x14, 0x01, 0xe0, 0x80, "7"			},
	{0x14, 0x01, 0xe0, 0x00, "8"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x15, 0x01, 0x01, 0x01, "Off"			},
	{0x15, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x15, 0x01, 0x0c, 0x08, "Every 150000"		},
	{0x15, 0x01, 0x0c, 0x00, "Every 200000"		},
	{0x15, 0x01, 0x0c, 0x0c, "150000 only"		},
	{0x15, 0x01, 0x0c, 0x04, "200000 only"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x15, 0x01, 0x10, 0x10, "Off"			},
	{0x15, 0x01, 0x10, 0x00, "On"			},

	{0   , 0xfe, 0   ,    7, "Coinage"		},
	{0x15, 0x01, 0xe0, 0x00, "4 Coins 1 Credits"	},
	{0x15, 0x01, 0xe0, 0x80, "3 Coins 1 Credits"	},
	{0x15, 0x01, 0xe0, 0x40, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"	},
	{0x15, 0x01, 0xe0, 0x60, "1 Coin  2 Credits"	},
	{0x15, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"	},
	{0x15, 0x01, 0xe0, 0x20, "1 Coin  4 Credits"	},
};

STDDIPINFO(Turtship)

static struct BurnDIPInfo DygerDIPList[]=
{
	{0x0a, 0xff, 0xff, 0xbd, NULL			},
	{0x0b, 0xff, 0xff, 0xf7, NULL			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x0a, 0x01, 0x02, 0x02, "No"			},
	{0x0a, 0x01, 0x02, 0x00, "Yes"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x0a, 0x01, 0x08, 0x08, "Off"			},
	{0x0a, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x0a, 0x01, 0x10, 0x10, "Easy"			},
	{0x0a, 0x01, 0x10, 0x00, "Hard"			},

	{0   , 0xfe, 0   ,    8, "Lives"		},
	{0x0a, 0x01, 0xe0, 0xe0, "1"			},
	{0x0a, 0x01, 0xe0, 0x60, "2"			},
	{0x0a, 0x01, 0xe0, 0xa0, "3"			},
	{0x0a, 0x01, 0xe0, 0x20, "4"			},
	{0x0a, 0x01, 0xe0, 0xc0, "5"			},
	{0x0a, 0x01, 0xe0, 0x40, "6"			},
	{0x0a, 0x01, 0xe0, 0x80, "7"			},
	{0x0a, 0x01, 0xe0, 0x00, "8"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x0b, 0x01, 0x01, 0x01, "Off"			},
	{0x0b, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x0b, 0x01, 0x0c, 0x04, "Every 150000"		},
	{0x0b, 0x01, 0x0c, 0x00, "Every 200000"		},
	{0x0b, 0x01, 0x0c, 0x0c, "150000 only"		},
	{0x0b, 0x01, 0x0c, 0x08, "200000 only"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x0b, 0x01, 0x10, 0x00, "Off"			},
	{0x0b, 0x01, 0x10, 0x10, "On"			},

	{0   , 0xfe, 0   ,    7, "Coinage"		},
	{0x0b, 0x01, 0xe0, 0x00, "4 Coins 1 Credits"	},
	{0x0b, 0x01, 0xe0, 0x80, "3 Coins 1 Credits"	},
	{0x0b, 0x01, 0xe0, 0x40, "2 Coins 1 Credits"	},
	{0x0b, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"	},
	{0x0b, 0x01, 0xe0, 0x60, "1 Coin  2 Credits"	},
	{0x0b, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"	},
	{0x0b, 0x01, 0xe0, 0x20, "1 Coin  4 Credits"	},
};

STDDIPINFO(Dyger)

static struct BurnDIPInfo WhizzDIPList[]=
{
	{0x12, 0xff, 0xff, 0x10, NULL			},
	{0x13, 0xff, 0xff, 0xfc, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},
	{0x15, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x12, 0x01, 0x10, 0x00, "No"			},
	{0x12, 0x01, 0x10, 0x10, "Yes"			},

	{0   , 0xfe, 0   ,    8, "Difficulty"		},
	{0x13, 0x01, 0x07, 0x07, "0 (Easiest)"		},
	{0x13, 0x01, 0x07, 0x06, "1"			},
	{0x13, 0x01, 0x07, 0x05, "2"			},
	{0x13, 0x01, 0x07, 0x04, "3 (Normal)"		},
	{0x13, 0x01, 0x07, 0x03, "4"			},
	{0x13, 0x01, 0x07, 0x02, "5"			},
	{0x13, 0x01, 0x07, 0x01, "6"			},
	{0x13, 0x01, 0x07, 0x00, "7 (Hardest)"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x10, 0x10, "Off"			},
	{0x13, 0x01, 0x10, 0x00, "On"			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x14, 0x01, 0x07, 0x00, "4 Coins 1 Credits"	},
	{0x14, 0x01, 0x07, 0x01, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0x07, 0x02, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x14, 0x01, 0x07, 0x04, "1 Coin  4 Credits"	},
	{0x14, 0x01, 0x07, 0x03, "1 Coin  5 Credits"	},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x14, 0x01, 0x18, 0x18, "100000 Only"		},
	{0x14, 0x01, 0x18, 0x10, "Every 100000"		},
	{0x14, 0x01, 0x18, 0x08, "Every 150000"		},
	{0x14, 0x01, 0x18, 0x00, "Every 200000"		},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x15, 0x01, 0x07, 0x00, "4 Coins 1 Credits"	},
	{0x15, 0x01, 0x07, 0x01, "3 Coins 1 Credits"	},
	{0x15, 0x01, 0x07, 0x02, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x15, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x15, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x15, 0x01, 0x07, 0x04, "1 Coin  4 Credits"	},
	{0x15, 0x01, 0x07, 0x03, "1 Coin  5 Credits"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x15, 0x01, 0x08, 0x00, "Off"			},
	{0x15, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x15, 0x01, 0x10, 0x10, "3"			},
	{0x15, 0x01, 0x10, 0x00, "5"			},
};

STDDIPINFO(Whizz)

static void palette_write(INT32 offset)
{
	offset &= 0x3ff;

	UINT16 data = (DrvPalRAM[offset + 0x400] * 256) + DrvPalRAM[offset];

	UINT8 r = (data >> 4) & 0x0f;
	UINT8 g = (data >> 0) & 0x0f;
	UINT8 b = (data >> 8) & 0x0f;

	r |= r << 4;
	g |= g << 4;
	b |= b << 4;

	DrvPalette[offset] = BurnHighCol(r,g,b,0);
}

static void bankswitch(INT32 data)
{
	bank_data = data & 0x0f;

	ZetMapMemory(DrvZ80ROM0 + 0x8000 + (bank_data * 0x4000), 0x8000, 0xbfff, MAP_ROM);
}

static void __fastcall sidearms_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf800) == 0xc000) {
		DrvPalRAM[address & 0x7ff] = data;
		palette_write(address);
		return;
	}

	switch (address)
	{
		case 0xc800:
			soundlatch = data;
		return;

		case 0xc801:
			bankswitch(data);
		return;

		case 0xc802:
			enable_watchdog = 1;
			watchdog = 0;
		return;

		case 0xc804:
		{
			// coin counters data & 0x01, data & 0x02
			// coin lockout data & 0x04, data & 0x08

			if (data & 0x10) {
				ZetClose();
				ZetOpen(1);
				ZetReset();
				ZetClose();
				ZetOpen(0);
			}

			if (starfield_enable != (data & 0x20)) {
				starfield_enable = data & 0x20;
				hflop_74a = 1;
				starscrollx = starscrolly = 0;
			}

			character_enable = data & 0x40;
			flipscreen = data & 0x80;
		}
		return;

		case 0xc805:
		{
		//	INT32 last = starscrollx;

			starscrollx++;
			starscrollx &= 0x1ff;

			// According to MAME this is correct, but for some reason in
			// FBA it is seizure-inducing. Just disable it for now.
		//	if (starscrollx && (~last & 0x100)) hflop_74a ^= 1;
		}
		return;

		case 0xc806:
			starscrolly++;
			starscrolly &= 0xff;
		return;

		case 0xc808:
		case 0xc809:
			bgscrollx[address & 1] = data;
		return;

		case 0xc80a:
		case 0xc80b:
			bgscrolly[address & 1] = data;
		return;

		case 0xc80c:
			sprite_enable = data & 0x01;
			bglayer_enable = data & 0x02;
		return;
	}
}

static UINT8 __fastcall sidearms_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xc800:
		case 0xc801:
		case 0xc802:
			return DrvInputs[address & 3];

		case 0xc803:
		case 0xc804:
			return DrvDips[address - 0xc803];

		case 0xc805:
			return ((vblank) ? 0 : 0x80);
	}

	return 0;
}

static void __fastcall turtship_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf800) == 0xe000) {
		DrvPalRAM[address & 0x7ff] = data;
		palette_write(address);
		return;
	}

	switch (address)
	{
		case 0xe800:
			soundlatch = data;
		return;

		case 0xe801:
			bankswitch(data);
		return;

		case 0xe802:
			enable_watchdog = 1;
			watchdog = 0;
		return;

		case 0xe804:
		{
			// coin counters data & 0x01, data & 0x02
			// coin lockout data & 0x04, data & 0x08

			if (data & 0x10) {
				ZetClose();
				ZetOpen(1);
				ZetReset();
				ZetClose();
				ZetOpen(0);
			}

			character_enable = data & 0x40;
			flipscreen = data & 0x80;
		}
		return;

		case 0xe808:
		case 0xe809:
			bgscrollx[address & 1] = data;
		return;

		case 0xe80a:
		case 0xe80b:
			bgscrolly[address & 1] = data;
		return;

		case 0xe80c:
			sprite_enable = data & 0x01;
			bglayer_enable = data & 0x02;
		return;
	}
}

static UINT8 turtship_input_read(INT32 offset)
{
	UINT8 ret = 0;
	UINT8 ports[5] = { DrvInputs[0], DrvInputs[1], DrvInputs[2], DrvDips[0], DrvDips[1] };

	for (INT32 i = 0; i < 5; i++) {
		ret |= ((ports[i] >> (offset & 7)) & 1) << i;
	}

	return ret;
}

static UINT8 __fastcall turtship_main_read(UINT16 address)
{
	if ((address & 0xfff8) == 0xe800) {
		return turtship_input_read(address);
	}

	return 0;
}

static void __fastcall whizz_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xc801:
			bankswitch(((data >> 5) & 2) | (data >> 7));
		return;

		case 0xc803:
		case 0xc805:
		return;		// nop
	}

	sidearms_main_write(address, data);
}

static UINT8 __fastcall whizz_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xc800:
			return DrvDips[1];

		case 0xc801:
			return DrvDips[2];

		case 0xc802:
			return DrvDips[3];

		case 0xc803:
			return (DrvInputs[0] & ~0x10) | (DrvDips[0] & 0x10);

		case 0xc804:
			return DrvInputs[1];

		case 0xc805:
			return DrvInputs[2];

		case 0xc806:
			return (DrvInputs[3] & 0xef) | ((vblank) ? 0x10 : 0);

		case 0xc807:
			return DrvInputs[4];
	}

	return 0;
}

static void __fastcall sidearms_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xf000:
		case 0xf001:
		case 0xf002:
		case 0xf003:
			BurnYM2203Write((address/2)&1, address & 1, data);
		return;
	}
}

static UINT8 __fastcall sidearms_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xd000:
			return soundlatch;

		case 0xf000:
		case 0xf001:
		case 0xf002:
		case 0xf003:
			return BurnYM2203Read((address/2)&1, address & 1);
	}

	return 0;
}

static void __fastcall whizz_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			BurnYM2151SelectRegister(data);
		return;

		case 0x01:
			BurnYM2151WriteRegister(data);
		return;

		case 0x40:
		return;		// nop
	}
}

static UINT8 __fastcall whizz_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
			return BurnYM2151ReadStatus();

		case 0xc0:
			return soundlatch;
	}

	return 0;
}

inline static void DrvYM2203IRQHandler(INT32, INT32 nStatus)
{
	ZetSetIRQLine(0, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static void DrvYM2151IrqHandler(INT32 nStatus)
{
	ZetSetIRQLine(0, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

inline static INT32 DrvSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)(ZetTotalCycles() * nSoundRate / 4000000);
}

inline static double DrvGetTime()
{
	return (double)ZetTotalCycles() / 4000000;
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	ZetOpen(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();

	if (is_whizz) {
		BurnYM2151Reset();
	} else {
		BurnYM2203Reset();
	}
	ZetClose();

	enable_watchdog = 0;
	watchdog = 0;

	flipscreen = 0;
	soundlatch = 0;
	character_enable = 0;
	sprite_enable = 0;
	bglayer_enable = 0;
	bank_data = 0;

	starfield_enable = 0;
	starscrollx = 0;
	starscrolly = 0;
	hflop_74a = 1;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x018000;
	DrvZ80ROM1		= Next; Next += 0x008000;

	DrvGfxROM0		= Next; Next += 0x010000;
	DrvGfxROM1		= Next; Next += 0x100000;
	DrvGfxROM2		= Next; Next += 0x080000;

	DrvStarMap		= Next; Next += 0x008000;
	DrvTileMap		= Next; Next += 0x008000;

	DrvPalette		= (UINT32*)Next; Next += 0x0400 * sizeof(UINT32);

	AllRam			= Next;

	DrvVidRAM		= Next; Next += 0x001000;
	DrvSprBuf		= Next; Next += 0x001000;
	DrvSprRAM		= Next; Next += 0x001000;
	DrvPalRAM		= Next; Next += 0x000800;
	DrvZ80RAM0		= Next; Next += 0x002000;
	DrvZ80RAM1		= Next; Next += 0x000800;

	bgscrollx		= Next; Next += 0x000002;
	bgscrolly		= Next; Next += 0x000002;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void DrvGfxDecode()
{
	INT32 Plane0[2]  = { 4, 0 };
	INT32 Plane1[4]  = { 0x200000 + 4, 0x200000 + 0, 4, 0 };
	INT32 Plane2[4]  = { 0x100000 + 4, 0x100000 + 0, 4, 0 };
	INT32 XOffs0[16] = { STEP4(0,1), STEP4(8,1), STEP4(256,1), STEP4(256+8,1) };
	INT32 XOffs1[32] = { STEP4(0,1), STEP4(8,1), STEP4(512,1), STEP4(512+8,1), STEP4(1024,1), STEP4(1024+8,1), STEP4(1536,1), STEP4(1536+8,1) };
	INT32 YOffs[32]  = { STEP32(0,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x80000);
	if (tmp == NULL) {
		return;
	}

	memcpy (tmp, DrvGfxROM0, 0x04000);

	GfxDecode(0x0400, 2,  8,  8, Plane0, XOffs0, YOffs, 0x080, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x80000);

	GfxDecode(0x0400, 4, 32, 32, Plane1, XOffs1, YOffs, 0x800, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x40000);

	GfxDecode(0x0800, 4, 16, 16, Plane2, XOffs0, YOffs, 0x200, tmp, DrvGfxROM2);

	BurnFree (tmp);
}

static INT32 SidearmsInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x08000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x10000,  2, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  3, 1)) return 1;

		if (BurnLoadRom(DrvStarMap + 0x00000,  4, 1)) return 1;
	
		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x08000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x10000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x18000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x40000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x48000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x50000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x58000, 13, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000, 14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x08000, 15, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x10000, 16, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x18000, 17, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x20000, 18, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x28000, 19, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x30000, 20, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x38000, 21, 1)) return 1;

		if (BurnLoadRom(DrvTileMap + 0x00000, 22, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvPalRAM,		0xc000, 0xc7ff, MAP_ROM);
	ZetMapMemory(DrvVidRAM,		0xd000, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM0,	0xe000, 0xefff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xf000, 0xffff, MAP_RAM);
	ZetSetWriteHandler(sidearms_main_write);
	ZetSetReadHandler(sidearms_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,	0xc000, 0xc7ff, MAP_RAM);
	ZetSetWriteHandler(sidearms_sound_write);
	ZetSetReadHandler(sidearms_sound_read);
	ZetClose();

	BurnYM2203Init(2,  4000000, &DrvYM2203IRQHandler, DrvSynchroniseStream, DrvGetTime, 0);
	BurnTimerAttachZet(4000000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE,   0.25, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_YM2203_ROUTE,   0.25, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_1, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_2, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_3, 0.15, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static INT32 TurtshipInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x08000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x10000,  2, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  3, 1)) return 1;
	
		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  4, 1)) return 1;
		if (is_turtshipk == 0) memcpy (DrvGfxROM0, DrvGfxROM0 + 0x4000, 0x4000);

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x10000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x20000,  7, 1)) return 1;
		memcpy (DrvGfxROM1 + 0x30000, DrvGfxROM1 + 0x10000, 0x10000);
		if (BurnLoadRom(DrvGfxROM1 + 0x40000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x50000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x60000, 10, 1)) return 1;
		memcpy (DrvGfxROM1 + 0x70000, DrvGfxROM1 + 0x50000, 0x10000);

		if (BurnLoadRom(DrvGfxROM2 + 0x00000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x10000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x20000, 13, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x30000, 14, 1)) return 1;

		if (BurnLoadRom(DrvTileMap + 0x00000, 15, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,	0xc000, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xd000, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvPalRAM,		0xe000, 0xe7ff, MAP_ROM);
	ZetMapMemory(DrvVidRAM,		0xf000, 0xffff, MAP_RAM);
	ZetSetWriteHandler(turtship_main_write);
	ZetSetReadHandler(turtship_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,	0xc000, 0xc7ff, MAP_RAM);
	ZetSetWriteHandler(sidearms_sound_write);
	ZetSetReadHandler(sidearms_sound_read);
	ZetClose();

	BurnYM2203Init(2,  4000000, &DrvYM2203IRQHandler, DrvSynchroniseStream, DrvGetTime, 0);
	BurnTimerAttachZet(4000000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE,   0.25, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_YM2203_ROUTE,   0.25, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_1, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_2, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_3, 0.15, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static INT32 TurtshipkInit()
{
	is_turtshipk = 1;
	return TurtshipInit();
}

static INT32 WhizzInit()
{
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
		memcpy (DrvGfxROM0, DrvGfxROM0 + 0x4000, 0x4000);

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x10000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x20000,  6, 1)) return 1;
		memcpy (DrvGfxROM1 + 0x30000, DrvGfxROM1 + 0x10000, 0x10000);
		if (BurnLoadRom(DrvGfxROM1 + 0x40000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x50000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x60000,  9, 1)) return 1;
		memcpy (DrvGfxROM1 + 0x70000, DrvGfxROM1 + 0x50000, 0x10000);

		if (BurnLoadRom(DrvGfxROM2 + 0x00000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x10000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x20000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x30000, 13, 1)) return 1;

		if (BurnLoadRom(DrvTileMap + 0x00000, 14, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvPalRAM,		0xc000, 0xc7ff, MAP_ROM);
	ZetMapMemory(DrvVidRAM,		0xd000, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM0,	0xe000, 0xefff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xf000, 0xffff, MAP_RAM);
	ZetSetWriteHandler(whizz_main_write);
	ZetSetReadHandler(whizz_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,	0xf800, 0xffff, MAP_RAM);
	ZetSetOutHandler(whizz_sound_write_port);
	ZetSetInHandler(whizz_sound_read_port);
	ZetClose();

	BurnYM2151Init(4000000);
	BurnYM2151SetIrqHandler(&DrvYM2151IrqHandler);
	BurnYM2151SetAllRoutes(1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	is_whizz = 1;

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	if (is_whizz) {
		BurnYM2151Exit();
	} else {
		BurnYM2203Exit();
	}

	BurnFree (AllMem);

	is_whizz = 0;
	is_turtshipk = 0;

	return 0;
}

static void sidearms_draw_starfield()
{
	UINT16 *lineptr = pTransDraw;
	UINT32 _hcount_191 = starscrollx & 0xff;

	for (INT32 y = 16; y < (16+nScreenHeight); y++)
	{
		UINT32 hadd_283 = _hcount_191 & ~0x1f;
		UINT32 vadd_283 = starscrolly + y;

		INT32 i = (vadd_283<<4) & 0xff0;
		i |= (hflop_74a^(hadd_283>>8)) << 3;
		i |= (hadd_283>>5) & 7;
		UINT32 latch_374 = DrvStarMap[i + 0x3000];

		hadd_283 = _hcount_191 - 1;

		for (INT32 x = 0; x < nScreenWidth; lineptr++, x++)
		{
			i = hadd_283;
			hadd_283 = _hcount_191 + (x & 0xff);
			vadd_283 = starscrolly + y;

			if (!((vadd_283 ^ (x>>3)) & 4)) continue;
			if ((vadd_283 | (hadd_283>>1)) & 2) continue;

			if ((i & 0x1f)==0x1f)
			{
				i  = (vadd_283<<4) & 0xff0;
				i |= (hflop_74a^(hadd_283>>8)) << 3;
				i |= (hadd_283>>5) & 7;
				latch_374 = DrvStarMap[i + 0x3000];
			}

			if ((~((latch_374^hadd_283)^1) & 0x1f)) continue;

			*lineptr = (UINT16)((latch_374>>5) | 0x378);
		}
	}
}

static void draw_bg_layer(INT32 type)
{
	INT32 scrollx = ((((bgscrollx[1] << 8) + bgscrollx[0]) & 0xfff) + 64) & 0xfff;
	INT32 scrolly = ((((bgscrolly[1] << 8) + bgscrolly[0]) & 0xfff) + 16) & 0xfff; 

	for (INT32 y = 0; y < 256; y += 32) {

		INT32 sy = y - (scrolly & 0x1f);
		if (sy >= nScreenHeight) continue;

		INT32 starty = (((scrolly + y) & 0xfff) / 0x20) * 0x80;
	
		for (INT32 x = 0; x < 416; x += 32) {

			INT32 sx = x - (scrollx & 0x1f);
			if (sx >= nScreenWidth) continue;

			INT32 offs = (((scrollx + x) & 0xfff) / 0x20) + starty;
			INT32 ofst = ((offs & 0x3c00) << 1) | ((offs & 0x0380) >> 6) | ((offs & 0x7f) << 4);

			INT32 attr  = DrvTileMap[ofst + 1];
			INT32 code  = DrvTileMap[ofst + 0] + ((attr & 0x01) * 256);
			INT32 color = attr >> 3;
			INT32 flipx = attr & 0x02;
			INT32 flipy = attr & 0x04;

			if (type)
			{
				if (flipy) {
					if (flipx) {
						Render32x32Tile_FlipXY_Clip(pTransDraw, code | ((attr & 0x80) << 2), sx, sy, color & 0x0f, 4, 0, DrvGfxROM1);
					} else {
						Render32x32Tile_FlipY_Clip(pTransDraw, code | ((attr & 0x80) << 2), sx, sy, color & 0x0f, 4, 0, DrvGfxROM1);
					}
				} else {
					if (flipx) {
						Render32x32Tile_FlipX_Clip(pTransDraw, code | ((attr & 0x80) << 2), sx, sy, color & 0x0f, 4, 0, DrvGfxROM1);
					} else {
						Render32x32Tile_Clip(pTransDraw, code | ((attr & 0x80) << 2), sx, sy, color & 0x0f, 4, 0, DrvGfxROM1);
					}
				}
			} else {
				if (flipy) {
					if (flipx) {
						Render32x32Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 4, 0x0f, 0, DrvGfxROM1);
					} else {
						Render32x32Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 4, 0x0f, 0, DrvGfxROM1);
					}
				} else {
					if (flipx) {
						Render32x32Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 4, 0x0f, 0, DrvGfxROM1);
					} else {
						Render32x32Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 4, 0x0f, 0, DrvGfxROM1);
					}
				}
			}
		}
	}
}

static void draw_fg_layer()
{
	for (INT32 offs = 0; offs < 64 * 32; offs++)
	{
		INT32 sx = ((offs & 0x3f) * 8) - 64;
		INT32 sy = ((offs / 0x40) * 8) - 16;

		if (sx >= nScreenWidth || sx < 0 || sy >= nScreenHeight || sy < 0) continue;

		INT32 attr = DrvVidRAM[0x800 + offs];
		INT32 code = DrvVidRAM[0x000 + offs] | ((attr & 0xc0) << 2);
		INT32 color = attr & 0x3f;

		Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 2, 3, 0x300, DrvGfxROM0);
	}
}

static void draw_sprites_region(INT32 start, INT32 end)
{
	for (INT32 offs = end - 32; offs >= start; offs -= 32)
	{
		INT32 sy = DrvSprBuf[offs + 2];
		if (!sy || DrvSprBuf[offs + 5] == 0xc3) continue;

		INT32 attr  = DrvSprBuf[offs + 1];
		INT32 color = attr & 0xf;
		INT32 code  = DrvSprBuf[offs] + ((attr << 3) & 0x700);
		INT32 sx    = DrvSprBuf[offs + 3] + ((attr << 4) & 0x100);

		Render16x16Tile_Mask_Clip(pTransDraw, code, sx - 64, sy - 16, color, 4, 0x0f, 0x200, DrvGfxROM2);
	}
}

static INT32 SidearmsDraw()
{
	if (DrvRecalc) {
		for (INT32 offs = 0; offs < 0x400; offs++) {
			palette_write(offs);
		}

		DrvRecalc = 0;
	}

	BurnTransferClear();

	if (starfield_enable) {
		sidearms_draw_starfield();
	}

	if (bglayer_enable) {
		draw_bg_layer(0);
	}

	if (sprite_enable) {
		draw_sprites_region(0x0700, 0x0800);
		draw_sprites_region(0x0e00, 0x1000);
		draw_sprites_region(0x0800, 0x0f00);
		draw_sprites_region(0x0000, 0x0700);
	}

	if (character_enable) {
		draw_fg_layer();
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 TurtshipDraw()
{
	if (DrvRecalc) {
		for (INT32 offs = 0; offs < 0x400; offs++) {
			palette_write(offs);
		}

		DrvRecalc = 0;
	}

	if (bglayer_enable) {
		draw_bg_layer(1);
	} else {
		BurnTransferClear();
	}

	if (sprite_enable) {
		draw_sprites_region(0x0700, 0x0800);
		draw_sprites_region(0x0e00, 0x1000);
		draw_sprites_region(0x0800, 0x0f00);
		draw_sprites_region(0x0000, 0x0700);
	}

	if (character_enable) {
		draw_fg_layer();
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DygerDraw()
{
	if (DrvRecalc) {
		for (INT32 offs = 0; offs < 0x400; offs++) {
			palette_write(offs);
		}

		DrvRecalc = 0;
	}

	if (bglayer_enable) {
		draw_bg_layer(1);
	} else {
		BurnTransferClear();
	}

	if (sprite_enable) {
		draw_sprites_region(0x0000, 0x1000);
	}

	if (character_enable) {
		draw_fg_layer();
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	watchdog++;
	if (watchdog > 180 && enable_watchdog) {
		DrvDoReset(0);
	}

	if (DrvReset) {
		DrvDoReset(1);
	}

	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 5);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
			DrvInputs[4] ^= (DrvJoy5[i] & 1) << i;
		}
	}

	INT32 nSoundBufferPos = 0;
	INT32 nInterleave = 278;
	INT32 nCyclesTotal[2] =  { 4000000 / 60, 4000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++) {

		INT32 nSegment = nCyclesTotal[0] / nInterleave;

		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nSegment);
		if (i == 274) {ZetSetIRQLine(0, CPU_IRQSTATUS_ACK); vblank = 1; }
		if (i == 276) ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
		nSegment = ZetTotalCycles();
		ZetClose();

		ZetOpen(1);
		if (is_whizz) {
			nCyclesDone[1] += ZetRun(nSegment - ZetTotalCycles());
			if (i == 274) ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
			if (i == 276) ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);

			if (pBurnSoundOut) {
				INT32 nSegmentLength = nBurnSoundLen / nInterleave;
				INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
				BurnYM2151Render(pSoundBuf, nSegmentLength);
				nSoundBufferPos += nSegmentLength;
			}
		} else {
			BurnTimerUpdate(nSegment);
		}
		ZetClose();
	}

	ZetOpen(1);

	if (is_whizz == 0) {
		BurnTimerEndFrame(nCyclesTotal[1]);
	}

	if (pBurnSoundOut) {
		if (is_whizz) {
			INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
	
			if (nSegmentLength) {
				BurnYM2151Render(pSoundBuf, nSegmentLength);
			}
		} else {
			BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
		}
	}

	ZetClose();
	
	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	memcpy (DrvSprBuf, DrvSprRAM, 0x1000);

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029709;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		ZetScan(nAction);

		if (is_whizz) {
			BurnYM2151Scan(nAction);
		} else {
			BurnYM2203Scan(nAction, pnMin);
		}

		SCAN_VAR(flipscreen);
		SCAN_VAR(soundlatch);
		SCAN_VAR(starfield_enable);
		SCAN_VAR(character_enable);
		SCAN_VAR(sprite_enable);
		SCAN_VAR(bglayer_enable);
		SCAN_VAR(bank_data);
		SCAN_VAR(bgscrollx[2]);
		SCAN_VAR(bgscrolly[2]);

		SCAN_VAR(starscrollx);
		SCAN_VAR(starscrolly);
		SCAN_VAR(hflop_74a);
		SCAN_VAR(enable_watchdog);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		bankswitch(bank_data);
		ZetClose();
	}

	return 0;
}


// Side Arms - Hyper Dyne (World, 861129)

static struct BurnRomInfo sidearmsRomDesc[] = {
	{ "sa03.bin",		0x8000, 0xe10fe6a0, 1 | BRF_PRG | BRF_ESS }, //  0 Main CPU
	{ "a_14e.rom",		0x8000, 0x4925ed03, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a_12e.rom",		0x8000, 0x81d0ece7, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "a_04k.rom",		0x8000, 0x34efe2d2, 2 | BRF_PRG | BRF_ESS }, //  3 Sound CPU

	{ "b_11j.rom",		0x8000, 0x134dc35b, 7 | BRF_GRA },           //  4 Starfield Data

	{ "a_10j.rom",		0x4000, 0x651fef75, 3 | BRF_GRA },           //  5 Characters

	{ "b_13d.rom",		0x8000, 0x3c59afe1, 4 | BRF_GRA },           //  6 Tiles
	{ "b_13e.rom",		0x8000, 0x64bc3b77, 4 | BRF_GRA },           //  7
	{ "b_13f.rom",		0x8000, 0xe6bcea6f, 4 | BRF_GRA },           //  8
	{ "b_13g.rom",		0x8000, 0xc71a3053, 4 | BRF_GRA },           //  9
	{ "b_14d.rom",		0x8000, 0x826e8a97, 4 | BRF_GRA },           // 10
	{ "b_14e.rom",		0x8000, 0x6cfc02a4, 4 | BRF_GRA },           // 11
	{ "b_14f.rom",		0x8000, 0x9b9f6730, 4 | BRF_GRA },           // 12
	{ "b_14g.rom",		0x8000, 0xef6af630, 4 | BRF_GRA },           // 13

	{ "b_11b.rom",		0x8000, 0xeb6f278c, 5 | BRF_GRA },           // 14 Sprites
	{ "b_13b.rom",		0x8000, 0xe91b4014, 5 | BRF_GRA },           // 15
	{ "b_11a.rom",		0x8000, 0x2822c522, 5 | BRF_GRA },           // 16
	{ "b_13a.rom",		0x8000, 0x3e8a9f75, 5 | BRF_GRA },           // 17
	{ "b_12b.rom",		0x8000, 0x86e43eda, 5 | BRF_GRA },           // 18
	{ "b_14b.rom",		0x8000, 0x076e92d1, 5 | BRF_GRA },           // 19
	{ "b_12a.rom",		0x8000, 0xce107f3c, 5 | BRF_GRA },           // 20
	{ "b_14a.rom",		0x8000, 0xdba06076, 5 | BRF_GRA },           // 21

	{ "b_03d.rom",		0x8000, 0x6f348008, 6 | BRF_GRA },           // 22 Tilemap

	{ "63s141.16h",		0x0100, 0x75af3553, 8 | BRF_OPT },           // 23 Proms
	{ "63s141.11h",		0x0100, 0xa6e4d68f, 8 | BRF_OPT },           // 24
	{ "63s141.15h",		0x0100, 0xc47c182a, 8 | BRF_OPT },           // 25
	{ "63s081.3j",		0x0020, 0xc5817816, 8 | BRF_OPT },           // 26
};

STD_ROM_PICK(sidearms)
STD_ROM_FN(sidearms)

struct BurnDriver BurnDrvSidearms = {
	"sidearms", NULL, NULL, NULL, "1986",
	"Side Arms - Hyper Dyne (World, 861129)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, sidearmsRomInfo, sidearmsRomName, NULL, NULL, SidearmsInputInfo, SidearmsDIPInfo,
	SidearmsInit, DrvExit, DrvFrame, SidearmsDraw, DrvScan, &DrvRecalc, 0x800,
	384, 224, 4, 3
};


// Side Arms - Hyper Dyne (US, 861202)

static struct BurnRomInfo sidearmsuRomDesc[] = {
	{ "SAA_03.15E",		0x8000, 0x32ef2739, 1 | BRF_PRG | BRF_ESS }, //  0 Main CPU
	{ "a_14e.rom",		0x8000, 0x4925ed03, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a_12e.rom",		0x8000, 0x81d0ece7, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "a_04k.rom",		0x8000, 0x34efe2d2, 2 | BRF_PRG | BRF_ESS }, //  3 Sound CPU

	{ "b_11j.rom",		0x8000, 0x134dc35b, 7 | BRF_GRA },           //  4 Starfield Data

	{ "a_10j.rom",		0x4000, 0x651fef75, 3 | BRF_GRA },           //  5 Characters

	{ "b_13d.rom",		0x8000, 0x3c59afe1, 4 | BRF_GRA },           //  6 Tiles
	{ "b_13e.rom",		0x8000, 0x64bc3b77, 4 | BRF_GRA },           //  7
	{ "b_13f.rom",		0x8000, 0xe6bcea6f, 4 | BRF_GRA },           //  8
	{ "b_13g.rom",		0x8000, 0xc71a3053, 4 | BRF_GRA },           //  9
	{ "b_14d.rom",		0x8000, 0x826e8a97, 4 | BRF_GRA },           // 10
	{ "b_14e.rom",		0x8000, 0x6cfc02a4, 4 | BRF_GRA },           // 11
	{ "b_14f.rom",		0x8000, 0x9b9f6730, 4 | BRF_GRA },           // 12
	{ "b_14g.rom",		0x8000, 0xef6af630, 4 | BRF_GRA },           // 13

	{ "b_11b.rom",		0x8000, 0xeb6f278c, 5 | BRF_GRA },           // 14 Sprites
	{ "b_13b.rom",		0x8000, 0xe91b4014, 5 | BRF_GRA },           // 15
	{ "b_11a.rom",		0x8000, 0x2822c522, 5 | BRF_GRA },           // 16
	{ "b_13a.rom",		0x8000, 0x3e8a9f75, 5 | BRF_GRA },           // 17
	{ "b_12b.rom",		0x8000, 0x86e43eda, 5 | BRF_GRA },           // 18
	{ "b_14b.rom",		0x8000, 0x076e92d1, 5 | BRF_GRA },           // 19
	{ "b_12a.rom",		0x8000, 0xce107f3c, 5 | BRF_GRA },           // 20
	{ "b_14a.rom",		0x8000, 0xdba06076, 5 | BRF_GRA },           // 21

	{ "b_03d.rom",		0x8000, 0x6f348008, 6 | BRF_GRA },           // 22 Tilemap

	{ "63s141.16h",		0x0100, 0x75af3553, 8 | BRF_OPT },           // 23 Proms
	{ "63s141.11h",		0x0100, 0xa6e4d68f, 8 | BRF_OPT },           // 24
	{ "63s141.15h",		0x0100, 0xc47c182a, 8 | BRF_OPT },           // 25
	{ "63s081.3j",		0x0020, 0xc5817816, 8 | BRF_OPT },           // 26
};

STD_ROM_PICK(sidearmsu)
STD_ROM_FN(sidearmsu)

struct BurnDriver BurnDrvSidearmsu = {
	"sidearmsu", "sidearms", NULL, NULL, "1986",
	"Side Arms - Hyper Dyne (US, 861202)\0", NULL, "Capcom (Romstar license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, sidearmsuRomInfo, sidearmsuRomName, NULL, NULL, SidearmsInputInfo, SidearmsDIPInfo,
	SidearmsInit, DrvExit, DrvFrame, SidearmsDraw, DrvScan, &DrvRecalc, 0x800,
	384, 224, 4, 3
};


// Side Arms - Hyper Dyne (US, 861128)

static struct BurnRomInfo sidearmsur1RomDesc[] = {
	{ "03",				0x8000, 0x9a799c45, 1 | BRF_PRG | BRF_ESS }, //  0 Main CPU
	{ "a_14e.rom",		0x8000, 0x4925ed03, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a_12e.rom",		0x8000, 0x81d0ece7, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "a_04k.rom",		0x8000, 0x34efe2d2, 2 | BRF_PRG | BRF_ESS }, //  3 Sound CPU

	{ "b_11j.rom",		0x8000, 0x134dc35b, 7 | BRF_GRA },           //  4 Starfield Data

	{ "a_10j.rom",		0x4000, 0x651fef75, 3 | BRF_GRA },           //  5 Characters

	{ "b_13d.rom",		0x8000, 0x3c59afe1, 4 | BRF_GRA },           //  6 Tiles
	{ "b_13e.rom",		0x8000, 0x64bc3b77, 4 | BRF_GRA },           //  7
	{ "b_13f.rom",		0x8000, 0xe6bcea6f, 4 | BRF_GRA },           //  8
	{ "b_13g.rom",		0x8000, 0xc71a3053, 4 | BRF_GRA },           //  9
	{ "b_14d.rom",		0x8000, 0x826e8a97, 4 | BRF_GRA },           // 10
	{ "b_14e.rom",		0x8000, 0x6cfc02a4, 4 | BRF_GRA },           // 11
	{ "b_14f.rom",		0x8000, 0x9b9f6730, 4 | BRF_GRA },           // 12
	{ "b_14g.rom",		0x8000, 0xef6af630, 4 | BRF_GRA },           // 13

	{ "b_11b.rom",		0x8000, 0xeb6f278c, 5 | BRF_GRA },           // 14 Sprites
	{ "b_13b.rom",		0x8000, 0xe91b4014, 5 | BRF_GRA },           // 15
	{ "b_11a.rom",		0x8000, 0x2822c522, 5 | BRF_GRA },           // 16
	{ "b_13a.rom",		0x8000, 0x3e8a9f75, 5 | BRF_GRA },           // 17
	{ "b_12b.rom",		0x8000, 0x86e43eda, 5 | BRF_GRA },           // 18
	{ "b_14b.rom",		0x8000, 0x076e92d1, 5 | BRF_GRA },           // 19
	{ "b_12a.rom",		0x8000, 0xce107f3c, 5 | BRF_GRA },           // 20
	{ "b_14a.rom",		0x8000, 0xdba06076, 5 | BRF_GRA },           // 21

	{ "b_03d.rom",		0x8000, 0x6f348008, 6 | BRF_GRA },           // 22 Tilemap

	{ "63s141.16h",		0x0100, 0x75af3553, 8 | BRF_OPT },           // 23 Proms
	{ "63s141.11h",		0x0100, 0xa6e4d68f, 8 | BRF_OPT },           // 24
	{ "63s141.15h",		0x0100, 0xc47c182a, 8 | BRF_OPT },           // 25
	{ "63s081.3j",		0x0020, 0xc5817816, 8 | BRF_OPT },           // 26
};

STD_ROM_PICK(sidearmsur1)
STD_ROM_FN(sidearmsur1)

struct BurnDriver BurnDrvSidearmsur1 = {
	"sidearmsur1", "sidearms", NULL, NULL, "1986",
	"Side Arms - Hyper Dyne (US, 861128)\0", NULL, "Capcom (Romstar license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, sidearmsur1RomInfo, sidearmsur1RomName, NULL, NULL, SidearmsInputInfo, SidearmsDIPInfo,
	SidearmsInit, DrvExit, DrvFrame, SidearmsDraw, DrvScan, &DrvRecalc, 0x800,
	384, 224, 4, 3
};


// Side Arms - Hyper Dyne (Japan, 861128)

static struct BurnRomInfo sidearmsjRomDesc[] = {
	{ "a_15e.rom",		0x8000, 0x61ceb0cc, 1 | BRF_PRG | BRF_ESS }, //  0 Main CPU
	{ "a_14e.rom",		0x8000, 0x4925ed03, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a_12e.rom",		0x8000, 0x81d0ece7, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "a_04k.rom",		0x8000, 0x34efe2d2, 2 | BRF_PRG | BRF_ESS }, //  3 Sound CPU

	{ "b_11j.rom",		0x8000, 0x134dc35b, 7 | BRF_GRA },           //  4 Starfield Data

	{ "a_10j.rom",		0x4000, 0x651fef75, 3 | BRF_GRA },           //  5 Characters

	{ "b_13d.rom",		0x8000, 0x3c59afe1, 4 | BRF_GRA },           //  6 Tiles
	{ "b_13e.rom",		0x8000, 0x64bc3b77, 4 | BRF_GRA },           //  7
	{ "b_13f.rom",		0x8000, 0xe6bcea6f, 4 | BRF_GRA },           //  8
	{ "b_13g.rom",		0x8000, 0xc71a3053, 4 | BRF_GRA },           //  9
	{ "b_14d.rom",		0x8000, 0x826e8a97, 4 | BRF_GRA },           // 10
	{ "b_14e.rom",		0x8000, 0x6cfc02a4, 4 | BRF_GRA },           // 11
	{ "b_14f.rom",		0x8000, 0x9b9f6730, 4 | BRF_GRA },           // 12
	{ "b_14g.rom",		0x8000, 0xef6af630, 4 | BRF_GRA },           // 13

	{ "b_11b.rom",		0x8000, 0xeb6f278c, 5 | BRF_GRA },           // 14 Sprites
	{ "b_13b.rom",		0x8000, 0xe91b4014, 5 | BRF_GRA },           // 15
	{ "b_11a.rom",		0x8000, 0x2822c522, 5 | BRF_GRA },           // 16
	{ "b_13a.rom",		0x8000, 0x3e8a9f75, 5 | BRF_GRA },           // 17
	{ "b_12b.rom",		0x8000, 0x86e43eda, 5 | BRF_GRA },           // 18
	{ "b_14b.rom",		0x8000, 0x076e92d1, 5 | BRF_GRA },           // 19
	{ "b_12a.rom",		0x8000, 0xce107f3c, 5 | BRF_GRA },           // 20
	{ "b_14a.rom",		0x8000, 0xdba06076, 5 | BRF_GRA },           // 21

	{ "b_03d.rom",		0x8000, 0x6f348008, 6 | BRF_GRA },           // 22 Tilemap

	{ "63s141.16h",		0x0100, 0x75af3553, 8 | BRF_OPT },           // 23 Proms
	{ "63s141.11h",		0x0100, 0xa6e4d68f, 8 | BRF_OPT },           // 24
	{ "63s141.15h",		0x0100, 0xc47c182a, 8 | BRF_OPT },           // 25
	{ "63s081.3j",		0x0020, 0xc5817816, 8 | BRF_OPT },           // 26
};

STD_ROM_PICK(sidearmsj)
STD_ROM_FN(sidearmsj)

struct BurnDriver BurnDrvSidearmsj = {
	"sidearmsj", "sidearms", NULL, NULL, "1986",
	"Side Arms - Hyper Dyne (Japan, 861128)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, sidearmsjRomInfo, sidearmsjRomName, NULL, NULL, SidearmsInputInfo, SidearmsDIPInfo,
	SidearmsInit, DrvExit, DrvFrame, SidearmsDraw, DrvScan, &DrvRecalc, 0x800,
	384, 224, 4, 3
};


// Turtle Ship (North America)

static struct BurnRomInfo turtshipRomDesc[] = {
	{ "t-3.bin",		0x08000, 0xb73ed7f2, 1 | BRF_PRG | BRF_ESS }, //  0 Main CPU
	{ "t-2.3g",			0x08000, 0x2327b35a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "t-1.bin",		0x08000, 0xa258ffec, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "t-4.8a",			0x08000, 0x1cbe48e8, 2 | BRF_PRG | BRF_ESS }, //  3 Sound CPU

	{ "t-5.8k",			0x08000, 0x35c3dbc5, 3 | BRF_GRA },           //  4 Characters

	{ "t-8.1d",			0x10000, 0x30a857f0, 4 | BRF_GRA },           //  5 Tiles
	{ "t-10.3c",		0x10000, 0x76bb73bb, 4 | BRF_GRA },           //  6
	{ "t-11.3d",		0x10000, 0x53da6cb1, 4 | BRF_GRA },           //  7
	{ "t-6.1a",			0x10000, 0x45ce41ad, 4 | BRF_GRA },           //  8
	{ "t-7.1c",			0x10000, 0x3ccf11b9, 4 | BRF_GRA },           //  9
	{ "t-9.3a",			0x10000, 0x44762916, 4 | BRF_GRA },           // 10

	{ "t-13.1i",		0x10000, 0x599f5246, 5 | BRF_GRA },           // 11 Sprites
	{ "t-15.bin",		0x10000, 0x6489b7b4, 5 | BRF_GRA },           // 12
	{ "t-12.1g",		0x10000, 0xfb54cd33, 5 | BRF_GRA },           // 13
	{ "t-14.bin",		0x10000, 0x1b67b674, 5 | BRF_GRA },           // 14

	{ "t-16.9f",		0x08000, 0x1a5a45d7, 6 | BRF_GRA },           // 15 Tilemap
};

STD_ROM_PICK(turtship)
STD_ROM_FN(turtship)

struct BurnDriver BurnDrvTurtship = {
	"turtship", NULL, NULL, NULL, "1988",
	"Turtle Ship (North America)\0", NULL, "Philko (Sharp Image license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, turtshipRomInfo, turtshipRomName, NULL, NULL, TurtshipInputInfo, TurtshipDIPInfo,
	TurtshipInit, DrvExit, DrvFrame, TurtshipDraw, DrvScan, &DrvRecalc, 0x800,
	384, 224, 4, 3
};


// Turtle Ship (Japan)

static struct BurnRomInfo turtshipjRomDesc[] = {
	{ "t-3.5g",			0x08000, 0x0863fc1c, 1 | BRF_PRG | BRF_ESS }, //  0 Main CPU
	{ "t-2.3g",			0x08000, 0x2327b35a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "t-1.3e",			0x08000, 0x845a9ab0, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "t-4.8a",			0x08000, 0x1cbe48e8, 2 | BRF_PRG | BRF_ESS }, //  3 Sound CPU

	{ "t-5.8k",			0x08000, 0x35c3dbc5, 3 | BRF_GRA },           //  4 Characters

	{ "t-8.1d",			0x10000, 0x30a857f0, 4 | BRF_GRA },           //  5 Tiles
	{ "t-10.3c",		0x10000, 0x76bb73bb, 4 | BRF_GRA },           //  6
	{ "t-11.3d",		0x10000, 0x53da6cb1, 4 | BRF_GRA },           //  7
	{ "t-6.1a",			0x10000, 0x45ce41ad, 4 | BRF_GRA },           //  8
	{ "t-7.1c",			0x10000, 0x3ccf11b9, 4 | BRF_GRA },           //  9
	{ "t-9.3a",			0x10000, 0x44762916, 4 | BRF_GRA },           // 10

	{ "t-13.1i",		0x10000, 0x599f5246, 5 | BRF_GRA },           // 11 Sprites
	{ "t-15.3i",		0x10000, 0xf30cfa90, 5 | BRF_GRA },           // 12
	{ "t-12.1g",		0x10000, 0xfb54cd33, 5 | BRF_GRA },           // 13
	{ "t-14.3g",		0x10000, 0xd636873c, 5 | BRF_GRA },           // 14

	{ "t-16.9f",		0x08000, 0x1a5a45d7, 6 | BRF_GRA },           // 15 Tilemap
};

STD_ROM_PICK(turtshipj)
STD_ROM_FN(turtshipj)

struct BurnDriver BurnDrvTurtshipj = {
	"turtshipj", "turtship", NULL, NULL, "1988",
	"Turtle Ship (Japan)\0", NULL, "Philko (Pacific Games license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, turtshipjRomInfo, turtshipjRomName, NULL, NULL, TurtshipInputInfo, TurtshipDIPInfo,
	TurtshipInit, DrvExit, DrvFrame, TurtshipDraw, DrvScan, &DrvRecalc, 0x800,
	384, 224, 4, 3
};


// Turtle Ship (Korea)

static struct BurnRomInfo turtshipkRomDesc[] = {
	{ "turtship.003",	0x08000, 0xe7a7fc2e, 1 | BRF_PRG | BRF_ESS }, //  0 Main CPU
	{ "turtship.002",	0x08000, 0xe576f482, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "turtship.001",	0x08000, 0xa9b64240, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "t-4.8a",			0x08000, 0x1cbe48e8, 2 | BRF_PRG | BRF_ESS }, //  3 Sound CPU

	{ "turtship.005",	0x04000, 0x651fef75, 3 | BRF_GRA },           //  4 Characters

	{ "turtship.008",	0x10000, 0xe0658469, 4 | BRF_GRA },           //  5 Tiles
	{ "t-10.3c",		0x10000, 0x76bb73bb, 4 | BRF_GRA },           //  6
	{ "t-11.3d",		0x10000, 0x53da6cb1, 4 | BRF_GRA },           //  7
	{ "turtship.006",	0x10000, 0xa7cce654, 4 | BRF_GRA },           //  8
	{ "t-7.1c",			0x10000, 0x3ccf11b9, 4 | BRF_GRA },           //  9
	{ "t-9.3a",			0x10000, 0x44762916, 4 | BRF_GRA },           // 10

	{ "t-13.1i",		0x10000, 0x599f5246, 5 | BRF_GRA },           // 11 Sprites
	{ "turtship.015",	0x10000, 0x69fd202f, 5 | BRF_GRA },           // 12
	{ "t-12.1g",		0x10000, 0xfb54cd33, 5 | BRF_GRA },           // 13
	{ "turtship.014",	0x10000, 0xb3ea74a3, 5 | BRF_GRA },           // 14

	{ "turtship.016",	0x08000, 0xaffd51dd, 6 | BRF_GRA },           // 15 Tilemap
};

STD_ROM_PICK(turtshipk)
STD_ROM_FN(turtshipk)

struct BurnDriver BurnDrvTurtshipk = {
	"turtshipk", "turtship", NULL, NULL, "1988",
	"Turtle Ship (Korea)\0", NULL, "Philko", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, turtshipkRomInfo, turtshipkRomName, NULL, NULL, TurtshipInputInfo, TurtshipDIPInfo,
	TurtshipkInit, DrvExit, DrvFrame, TurtshipDraw, DrvScan, &DrvRecalc, 0x800,
	384, 224, 4, 3
};


// Turtle Ship (Korea, older)

static struct BurnRomInfo turtshipkoRomDesc[] = {
	{ "T-3.G5",			0x08000, 0xcd789535, 1 | BRF_PRG | BRF_ESS }, //  0 Main CPU
	{ "T-2.G3",			0x08000, 0x253678c0, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "T-1.E3",			0x08000, 0xd6fdc376, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "T-4.A8",			0x08000, 0x1cbe48e8, 2 | BRF_PRG | BRF_ESS }, //  3 Sound CPU

	{ "T-5.K8",			0x08000, 0x35c3dbc5, 3 | BRF_GRA },           //  4 Characters

	{ "T-8.D1",			0x10000, 0x2f0b2336, 4 | BRF_GRA },           //  5 Tiles
	{ "T-10.C3",		0x10000, 0x6a0072f4, 4 | BRF_GRA },           //  6
	{ "T-11.D3",		0x10000, 0x53da6cb1, 4 | BRF_GRA },           //  7
	{ "T-6.A1",			0x10000, 0xa7cce654, 4 | BRF_GRA },           //  8
	{ "T-7.C1",			0x10000, 0x90dd8415, 4 | BRF_GRA },           //  9
	{ "T-9.A3",			0x10000, 0x44762916, 4 | BRF_GRA },           // 10

	{ "T-13.I1",		0x10000, 0x1cc87f50, 5 | BRF_GRA },           // 11 Sprites
	{ "T-15.I3",		0x10000, 0x775ee5d9, 5 | BRF_GRA },           // 12
	{ "T-12.G1",		0x10000, 0x57783312, 5 | BRF_GRA },           // 13
	{ "T-14.G3",		0x10000, 0xa30e3346, 5 | BRF_GRA },           // 14

	{ "T-16.F9",		0x08000, 0x9b377277, 6 | BRF_GRA },           // 15 Tilemap
};

STD_ROM_PICK(turtshipko)
STD_ROM_FN(turtshipko)

struct BurnDriver BurnDrvTurtshipko = {
	"turtshipko", "turtship", NULL, NULL, "1988",
	"Turtle Ship (Korea, older)\0", NULL, "Philko", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, turtshipkoRomInfo, turtshipkoRomName, NULL, NULL, TurtshipInputInfo, TurtshipDIPInfo,
	TurtshipInit, DrvExit, DrvFrame, TurtshipDraw, DrvScan, &DrvRecalc, 0x800,
	384, 224, 4, 3
};


// Turtle Ship (Korea, 88/9)

static struct BurnRomInfo turtshipknRomDesc[] = {
	{ "T-3.G5",			0x08000, 0x529b091c, 1 | BRF_PRG | BRF_ESS }, //  0 Main CPU
	{ "T-2.G3",			0x08000, 0xd2f30195, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "T-1.E3",			0x08000, 0x2d02da90, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "T-4.A8",			0x08000, 0x1cbe48e8, 2 | BRF_PRG | BRF_ESS }, //  3 Sound CPU

	{ "T-5.K8",			0x08000, 0x5c2ee02d, 3 | BRF_GRA },           //  4 Characters

	{ "T-8.D1",			0x10000, 0x2f0b2336, 4 | BRF_GRA },           //  5 Tiles
	{ "T-10.C3",		0x10000, 0x6a0072f4, 4 | BRF_GRA },           //  6
	{ "T-11.D3",		0x10000, 0x53da6cb1, 4 | BRF_GRA },           //  7
	{ "T-6.A1",			0x10000, 0xa7cce654, 4 | BRF_GRA },           //  8
	{ "T-7.C1",			0x10000, 0x90dd8415, 4 | BRF_GRA },           //  9
	{ "T-9.A3",			0x10000, 0x44762916, 4 | BRF_GRA },           // 10

	{ "T-13.I1",		0x10000, 0x1cc87f50, 5 | BRF_GRA },           // 11 Sprites
	{ "T-15.I3",		0x10000, 0x3bf91fb8, 5 | BRF_GRA },           // 12
	{ "T-12.G1",		0x10000, 0x57783312, 5 | BRF_GRA },           // 13
	{ "T-14.G3",		0x10000, 0xee162dc0, 5 | BRF_GRA },           // 14

	{ "T-16.F9",		0x08000, 0x9b377277, 6 | BRF_GRA },           // 15 Tilemap
};

STD_ROM_PICK(turtshipkn)
STD_ROM_FN(turtshipkn)

struct BurnDriver BurnDrvTurtshipkn = {
	"turtshipkn", "turtship", NULL, NULL, "1988",
	"Turtle Ship (Korea, 88/9)\0", NULL, "Philko", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, turtshipknRomInfo, turtshipknRomName, NULL, NULL, TurtshipInputInfo, TurtshipDIPInfo,
	TurtshipInit, DrvExit, DrvFrame, TurtshipDraw, DrvScan, &DrvRecalc, 0x800,
	384, 224, 4, 3
};


// Dyger (Korea set 1)

static struct BurnRomInfo dygerRomDesc[] = {
	{ "d-3.5g",			0x08000, 0xbae9882e, 1 | BRF_PRG | BRF_ESS }, //  0 Main CPU
	{ "d-2.3g",			0x08000, 0x059ac4dc, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "d-1.3e",			0x08000, 0xd8440f66, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "d-4.8a",			0x08000, 0x8a256c09, 2 | BRF_PRG | BRF_ESS }, //  3 Sound CPU

	{ "d-5.8k",			0x08000, 0xc4bc72a5, 3 | BRF_GRA },           //  4 Characters

	{ "d-10.1d",		0x10000, 0x9715880d, 4 | BRF_GRA },           //  5 Tiles
	{ "d-9.3c",			0x10000, 0x628dae72, 4 | BRF_GRA },           //  6
	{ "d-11.3d",		0x10000, 0x23248db1, 4 | BRF_GRA },           //  7
	{ "d-6.1a",			0x10000, 0x4ba7a437, 4 | BRF_GRA },           //  8
	{ "d-8.1c",			0x10000, 0x6c0f0e0c, 4 | BRF_GRA },           //  9
	{ "d-7.3a",			0x10000, 0x2c50a229, 4 | BRF_GRA },           // 10

	{ "d-14.1i",		0x10000, 0x99c60b26, 5 | BRF_GRA },           // 11 Sprites
	{ "d-15.3i",		0x10000, 0xd6475ecc, 5 | BRF_GRA },           // 12
	{ "d-12.1g",		0x10000, 0xe345705f, 5 | BRF_GRA },           // 13
	{ "d-13.3g",		0x10000, 0xfaf4be3a, 5 | BRF_GRA },           // 14

	{ "d-16.9f",		0x08000, 0x0792e8f2, 6 | BRF_GRA },           // 15 Tilemap
};

STD_ROM_PICK(dyger)
STD_ROM_FN(dyger)

static INT32 DygerInit()
{
	INT32 nRet = TurtshipInit();

	BurnYM2203SetPSGVolume(0, 0.10);
	BurnYM2203SetPSGVolume(1, 0.10);

	return nRet;
}

struct BurnDriver BurnDrvDyger = {
	"dyger", NULL, NULL, NULL, "1989",
	"Dyger (Korea set 1)\0", NULL, "Philko", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, dygerRomInfo, dygerRomName, NULL, NULL, DygerInputInfo, DygerDIPInfo,
	DygerInit, DrvExit, DrvFrame, DygerDraw, DrvScan, &DrvRecalc, 0x800,
	224, 384, 3, 4
};


// Dyger (Korea set 2)

static struct BurnRomInfo dygeraRomDesc[] = {
	{ "d-3.bin",		0x08000, 0xfc63da8b, 1 | BRF_PRG | BRF_ESS }, //  0 Main CPU
	{ "d-2.3g",			0x08000, 0x059ac4dc, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "d-1.3e",			0x08000, 0xd8440f66, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "d-4.8a",			0x08000, 0x8a256c09, 2 | BRF_PRG | BRF_ESS }, //  3 Sound CPU

	{ "d-5.8k",			0x08000, 0xc4bc72a5, 3 | BRF_GRA },           //  4 Characters

	{ "d-10.1d",		0x10000, 0x9715880d, 4 | BRF_GRA },           //  5 Tiles
	{ "d-9.3c",			0x10000, 0x628dae72, 4 | BRF_GRA },           //  6
	{ "d-11.3d",		0x10000, 0x23248db1, 4 | BRF_GRA },           //  7
	{ "d-6.1a",			0x10000, 0x4ba7a437, 4 | BRF_GRA },           //  8
	{ "d-8.1c",			0x10000, 0x6c0f0e0c, 4 | BRF_GRA },           //  9
	{ "d-7.3a",			0x10000, 0x2c50a229, 4 | BRF_GRA },           // 10

	{ "d-14.1i",		0x10000, 0x99c60b26, 5 | BRF_GRA },           // 11 Sprites
	{ "d-15.3i",		0x10000, 0xd6475ecc, 5 | BRF_GRA },           // 12
	{ "d-12.1g",		0x10000, 0xe345705f, 5 | BRF_GRA },           // 13
	{ "d-13.3g",		0x10000, 0xfaf4be3a, 5 | BRF_GRA },           // 14

	{ "d-16.9f",		0x08000, 0x0792e8f2, 6 | BRF_GRA },           // 15 Tilemap
};

STD_ROM_PICK(dygera)
STD_ROM_FN(dygera)

struct BurnDriver BurnDrvDygera = {
	"dygera", "dyger", NULL, NULL, "1989",
	"Dyger (Korea set 2)\0", NULL, "Philko", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, dygeraRomInfo, dygeraRomName, NULL, NULL, DygerInputInfo, DygerDIPInfo,
	DygerInit, DrvExit, DrvFrame, DygerDraw, DrvScan, &DrvRecalc, 0x800,
	224, 384, 3, 4
};


// Twin Falcons

static struct BurnRomInfo twinfalcRomDesc[] = {
	{ "t-15.bin",		0x08000, 0xe1f20144, 1 | BRF_PRG | BRF_ESS }, //  0 Main CPU
	{ "t-14.bin",		0x10000, 0xc499ff83, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "t-1.b4",			0x08000, 0xb84bc980, 2 | BRF_PRG | BRF_ESS }, //  2 Sound CPU

	{ "t-6.r6",			0x08000, 0x8e4ca776, 3 | BRF_GRA },           //  3 Characters

	{ "t-10.y10",		0x10000, 0xb678ef5b, 4 | BRF_GRA },           //  4 Tiles
	{ "t-9.w10",		0x10000, 0xd7345fb9, 4 | BRF_GRA },           //  5
	{ "t-8.u10",		0x10000, 0x41428dac, 4 | BRF_GRA },           //  6
	{ "t-13.y11",		0x10000, 0x0eba10bd, 4 | BRF_GRA },           //  7
	{ "t-12.w11",		0x10000, 0xc65050ce, 4 | BRF_GRA },           //  8
	{ "t-11.u11",		0x10000, 0x51a2c65d, 4 | BRF_GRA },           //  9

	{ "t-2.a5",			0x10000, 0x9c106835, 5 | BRF_GRA },           // 10 Sprites
	{ "t-3.b5",			0x10000, 0x9b421ccf, 5 | BRF_GRA },           // 11
	{ "t-4.a7",			0x10000, 0x3a1db986, 5 | BRF_GRA },           // 12
	{ "t-5.b7",			0x10000, 0x9bd22190, 5 | BRF_GRA },           // 13

	{ "t-7.y8",			0x08000, 0xa8b5f750, 6 | BRF_GRA },           // 14 Tilemap
};

STD_ROM_PICK(twinfalc)
STD_ROM_FN(twinfalc)

struct BurnDriver BurnDrvTwinfalc = {
	"twinfalc", NULL, NULL, NULL, "1989",
	"Twin Falcons\0", NULL, "Philko (Poara Enterprises license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, twinfalcRomInfo, twinfalcRomName, NULL, NULL, WhizzInputInfo, WhizzDIPInfo,
	WhizzInit, DrvExit, DrvFrame, DygerDraw, DrvScan, &DrvRecalc, 0x800,
	384, 224, 4, 3
};


// Whizz

static struct BurnRomInfo whizzRomDesc[] = {
	{ "t-15.l11",		0x08000, 0x73161302, 1 | BRF_PRG | BRF_ESS }, //  0 Main CPU
	{ "t-14.k11",		0x10000, 0xbf248879, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "t-1.b4",			0x08000, 0xb84bc980, 2 | BRF_PRG | BRF_ESS }, //  2 Sound CPU

	{ "t-6.r6",			0x08000, 0x8e4ca776, 3 | BRF_GRA },           //  3 Characters

	{ "t-10.y10",		0x10000, 0xb678ef5b, 4 | BRF_GRA },           //  4 Tiles
	{ "t-9.w10",		0x10000, 0xd7345fb9, 4 | BRF_GRA },           //  5
	{ "t-8.u10",		0x10000, 0x41428dac, 4 | BRF_GRA },           //  6
	{ "t-13.y11",		0x10000, 0x0eba10bd, 4 | BRF_GRA },           //  7
	{ "t-12.w11",		0x10000, 0xc65050ce, 4 | BRF_GRA },           //  8
	{ "t-11.u11",		0x10000, 0x51a2c65d, 4 | BRF_GRA },           //  9

	{ "t-2.a5",			0x10000, 0x9c106835, 5 | BRF_GRA },           // 10 Sprites
	{ "t-3.b5",			0x10000, 0x9b421ccf, 5 | BRF_GRA },           // 11
	{ "t-4.a7",			0x10000, 0x3a1db986, 5 | BRF_GRA },           // 12
	{ "t-5.b7",			0x10000, 0x9bd22190, 5 | BRF_GRA },           // 13

	{ "t-7.y8",			0x08000, 0xa8b5f750, 6 | BRF_GRA },           // 14 Tilemap
};

STD_ROM_PICK(whizz)
STD_ROM_FN(whizz)

struct BurnDriver BurnDrvWhizz = {
	"whizz", "twinfalc", NULL, NULL, "1989",
	"Whizz\0", NULL, "Philko", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, whizzRomInfo, whizzRomName, NULL, NULL, WhizzInputInfo, WhizzDIPInfo,
	WhizzInit, DrvExit, DrvFrame, DygerDraw, DrvScan, &DrvRecalc, 0x800,
	384, 224, 4, 3
};

