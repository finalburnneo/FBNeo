// FB Alpha SNK 6502-based hardware driver module
// Based on MAME driver by Nicola Salmoria and Dan Boris

#include "tiles_generic.h"
#include "m6502_intf.h"
#include "snk6502_sound.h"
#include "sn76477.h"
#include "samples.h"
#include "lowpass2.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6502ROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvGfxExp;
static UINT8 *DrvColPROM;
static UINT8 *DrvSndROM0;
static UINT8 *DrvSndROM1;
static UINT8 *DrvM6502RAM;
static UINT8 *DrvVidRAM2;
static UINT8 *DrvVidRAM;
static UINT8 *DrvColRAM;
static UINT8 *DrvCharRAM;
static INT16 *FilterBUF;
static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 backcolor;
static UINT8 charbank;
static UINT8 flipscreen;
static UINT8 irqmask;
static UINT8 scrollx;
static UINT8 scrolly;

static INT32 sasuke_counter;
static INT32 game_type;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static INT32 nExtraCycles;

static INT32 numSN = 0; // number of sn76477
static INT32 bHasSamples = 0;

static class LowPass2 *LP1 = NULL, *LP2 = NULL;

static struct BurnInputInfo SasukeInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 1"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 start"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Sasuke)

static struct BurnInputInfo SatansatInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 start"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 2"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Satansat)

static struct BurnInputInfo VanguardInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 7,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 4"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 4"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Vanguard)

static struct BurnInputInfo FantasyInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 7,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 right"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 right"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Fantasy)

static struct BurnInputInfo NibblerInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 7,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 right"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 right"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Nibbler)

static struct BurnInputInfo PballoonInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 7,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Pballoon)

static struct BurnDIPInfo SasukeDIPList[]=
{
	{0x0a, 0xff, 0xff, 0x15, NULL			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0a, 0x01, 0x01, 0x01, "Upright"		},
	{0x0a, 0x01, 0x01, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Coinage"		},
	{0x0a, 0x01, 0x02, 0x00, "1 Coin  1 Credits"	},
	{0x0a, 0x01, 0x02, 0x02, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x0a, 0x01, 0x04, 0x04, "5000"			},
	{0x0a, 0x01, 0x04, 0x00, "10000"		},

	{0   , 0xfe, 0   ,    3, "Lives"		},
	{0x0a, 0x01, 0x30, 0x00, "3"			},
	{0x0a, 0x01, 0x30, 0x10, "4"			},
	{0x0a, 0x01, 0x30, 0x20, "5"			},

	{0   , 0xfe, 0   ,    2, "RAM Test"		},
	{0x0a, 0x01, 0x80, 0x00, "Off"			},
	{0x0a, 0x01, 0x80, 0x80, "On"			},
};

STDDIPINFO(Sasuke)

static struct BurnDIPInfo SasukeaDIPList[]=
{
	{0x0a, 0xff, 0xff, 0x95, NULL			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0a, 0x01, 0x01, 0x01, "Upright"		},
	{0x0a, 0x01, 0x01, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Coinage"		},
	{0x0a, 0x01, 0x02, 0x00, "1 Coin  1 Credits"	},
	{0x0a, 0x01, 0x02, 0x02, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x0a, 0x01, 0x04, 0x04, "5000"			},
	{0x0a, 0x01, 0x04, 0x00, "10000"		},

	{0   , 0xfe, 0   ,    3, "Lives"		},
	{0x0a, 0x01, 0x30, 0x00, "3"			},
	{0x0a, 0x01, 0x30, 0x10, "4"			},
	{0x0a, 0x01, 0x30, 0x20, "5"			},
};

STDDIPINFO(Sasukea)

static struct BurnDIPInfo SatansatDIPList[]=
{
	{0x0c, 0xff, 0xff, 0x15, NULL			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0c, 0x01, 0x01, 0x01, "Upright"		},
	{0x0c, 0x01, 0x01, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    3, "Coinage"		},
	{0x0c, 0x01, 0x0a, 0x08, "2 Coins 1 Credits"	},
	{0x0c, 0x01, 0x0a, 0x00, "1 Coin  1 Credits"	},
	{0x0c, 0x01, 0x0a, 0x02, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x0c, 0x01, 0x04, 0x04, "5000"			},
	{0x0c, 0x01, 0x04, 0x00, "10000"		},

	{0   , 0xfe, 0   ,    3, "Lives"		},
	{0x0c, 0x01, 0x30, 0x00, "3"			},
	{0x0c, 0x01, 0x30, 0x10, "4"			},
	{0x0c, 0x01, 0x30, 0x20, "5"			},

	{0   , 0xfe, 0   ,    2, "RAM Test"		},
	{0x0c, 0x01, 0x80, 0x00, "Off"			},
	{0x0c, 0x01, 0x80, 0x80, "On"			},
};

STDDIPINFO(Satansat)

static struct BurnDIPInfo VanguardDIPList[]=
{
	{0x15, 0xff, 0xff, 0x03, NULL			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x15, 0x01, 0x01, 0x01, "Upright"		},
	{0x15, 0x01, 0x01, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    5, "Coinage"		},
	{0x15, 0x01, 0x0e, 0x02, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0x0e, 0x00, "1 Coin  1 Credits"	},
	{0x15, 0x01, 0x0e, 0x08, "1 Coin  2 Credits"	},
	{0x15, 0x01, 0x0e, 0x04, "1 Coin  3 Credits"	},
	{0x15, 0x01, 0x0e, 0x0c, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    3, "Lives"		},
	{0x15, 0x01, 0x30, 0x00, "3"			},
	{0x15, 0x01, 0x30, 0x10, "4"			},
	{0x15, 0x01, 0x30, 0x20, "5"			},

	{0   , 0xfe, 0   ,    2, "Coinage Bonus"	},
	{0x15, 0x01, 0x40, 0x40, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0x40, 0x00, "1 Coin  1 Credits"	},
};

STDDIPINFO(Vanguard)

static struct BurnDIPInfo FantasyDIPList[]=
{
	{0x0d, 0xff, 0xff, 0x03, NULL			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0d, 0x01, 0x01, 0x01, "Upright"		},
	{0x0d, 0x01, 0x01, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    5, "Coinage"		},
	{0x0d, 0x01, 0x0e, 0x02, "2 Coins 1 Credits"	},
	{0x0d, 0x01, 0x0e, 0x00, "1 Coin  1 Credits"	},
	{0x0d, 0x01, 0x0e, 0x08, "1 Coin  2 Credits"	},
	{0x0d, 0x01, 0x0e, 0x04, "1 Coin  3 Credits"	},
	{0x0d, 0x01, 0x0e, 0x0c, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    3, "Lives"		},
	{0x0d, 0x01, 0x30, 0x00, "3"			},
	{0x0d, 0x01, 0x30, 0x10, "4"			},
	{0x0d, 0x01, 0x30, 0x20, "5"			},

	{0   , 0xfe, 0   ,    2, "Coinage Bonus"	},
	{0x0d, 0x01, 0x40, 0x40, "2 Coins 1 Credits"	},
	{0x0d, 0x01, 0x40, 0x00, "1 Coin  1 Credits"	},
};

STDDIPINFO(Fantasy)

static struct BurnDIPInfo FantasyuDIPList[]=
{
	{0x0d, 0xff, 0xff, 0x03, NULL			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0d, 0x01, 0x01, 0x01, "Upright"		},
	{0x0d, 0x01, 0x01, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    5, "Coinage"		},
	{0x0d, 0x01, 0x0e, 0x02, "2 Coins 1 Credits"	},
	{0x0d, 0x01, 0x0e, 0x00, "1 Coin  1 Credits"	},
	{0x0d, 0x01, 0x0e, 0x08, "1 Coin  2 Credits"	},
	{0x0d, 0x01, 0x0e, 0x04, "1 Coin  3 Credits"	},
	{0x0d, 0x01, 0x0e, 0x0c, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    3, "Lives"		},
	{0x0d, 0x01, 0x30, 0x00, "3"			},
	{0x0d, 0x01, 0x30, 0x10, "4"			},
	{0x0d, 0x01, 0x30, 0x20, "5"			},

	{0   , 0xfe, 0   ,    2, "Coinage Bonus"	},
	{0x0d, 0x01, 0x40, 0x40, "2 Coins 1 Credits"	},
	{0x0d, 0x01, 0x40, 0x00, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    2, "Allow continue"	},
	{0x0d, 0x01, 0x80, 0x80, "No"			},
	{0x0d, 0x01, 0x80, 0x00, "Yes"			},
};

STDDIPINFO(Fantasyu)

static struct BurnDIPInfo NibblerDIPList[]=
{
	{0x0d, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0d, 0x01, 0x03, 0x00, "3"			},
	{0x0d, 0x01, 0x03, 0x01, "4"			},
	{0x0d, 0x01, 0x03, 0x02, "5"			},
	{0x0d, 0x01, 0x03, 0x03, "6"			},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x0d, 0x01, 0x04, 0x00, "Easy"			},
	{0x0d, 0x01, 0x04, 0x04, "Hard"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0d, 0x01, 0x08, 0x00, "Upright"		},
	{0x0d, 0x01, 0x08, 0x08, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x0d, 0x01, 0x10, 0x00, "Off"			},
	{0x0d, 0x01, 0x10, 0x10, "On"			},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x0d, 0x01, 0x20, 0x00, "Off"			},
	{0x0d, 0x01, 0x20, 0x20, "On"			},

	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{0x0d, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"	},
	{0x0d, 0x01, 0xc0, 0xc0, "2 Coins/1 Credit 4/3"	},
	{0x0d, 0x01, 0xc0, 0x00, "1 Coin  1 Credits"	},
	{0x0d, 0x01, 0xc0, 0x80, "1 Coin/1 Credit 2/3"	},
};

STDDIPINFO(Nibbler)

static struct BurnDIPInfo Nibbler8DIPList[]=
{
	{0x0d, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0d, 0x01, 0x03, 0x00, "2"			},
	{0x0d, 0x01, 0x03, 0x01, "3"			},
	{0x0d, 0x01, 0x03, 0x02, "4"			},
	{0x0d, 0x01, 0x03, 0x03, "5"			},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x0d, 0x01, 0x04, 0x00, "Easy"			},
	{0x0d, 0x01, 0x04, 0x04, "Hard"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0d, 0x01, 0x08, 0x00, "Upright"		},
	{0x0d, 0x01, 0x08, 0x08, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x0d, 0x01, 0x10, 0x00, "Off"			},
	{0x0d, 0x01, 0x10, 0x10, "On"			},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x0d, 0x01, 0x20, 0x00, "Off"			},
	{0x0d, 0x01, 0x20, 0x20, "On"			},

	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{0x0d, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"	},
	{0x0d, 0x01, 0xc0, 0xc0, "2 Coins/1 Credit 4/3"	},
	{0x0d, 0x01, 0xc0, 0x00, "1 Coin  1 Credits"	},
	{0x0d, 0x01, 0xc0, 0x80, "1 Coin/1 Credit 2/3"	},
};

STDDIPINFO(Nibbler8)

static struct BurnDIPInfo Nibbler6DIPList[]=
{
	{0x0d, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0d, 0x01, 0x03, 0x00, "2"			},
	{0x0d, 0x01, 0x03, 0x01, "3"			},
	{0x0d, 0x01, 0x03, 0x02, "4"			},
	{0x0d, 0x01, 0x03, 0x03, "5"			},

	{0   , 0xfe, 0   ,    2, "Pause at corners"	},
	{0x0d, 0x01, 0x04, 0x00, "On"			},
	{0x0d, 0x01, 0x04, 0x04, "Off"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0d, 0x01, 0x08, 0x00, "Upright"		},
	{0x0d, 0x01, 0x08, 0x08, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x0d, 0x01, 0x10, 0x00, "Off"			},
	{0x0d, 0x01, 0x10, 0x10, "On"			},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x0d, 0x01, 0x20, 0x00, "Off"			},
	{0x0d, 0x01, 0x20, 0x20, "On"			},

	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{0x0d, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"	},
	{0x0d, 0x01, 0xc0, 0xc0, "2 Coins/1 Credit 4/3"	},
	{0x0d, 0x01, 0xc0, 0x00, "1 Coin  1 Credits"	},
	{0x0d, 0x01, 0xc0, 0x80, "1 Coin/1 Credit 2/3"	},
};

STDDIPINFO(Nibbler6)

static struct BurnDIPInfo PballoonDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x83, NULL			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0f, 0x01, 0x01, 0x01, "Upright"		},
	{0x0f, 0x01, 0x01, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    5, "Coinage"		},
	{0x0f, 0x01, 0x0e, 0x02, "2 Coins 1 Credits"	},
	{0x0f, 0x01, 0x0e, 0x00, "1 Coin  1 Credits"	},
	{0x0f, 0x01, 0x0e, 0x08, "1 Coin  2 Credits"	},
	{0x0f, 0x01, 0x0e, 0x04, "1 Coin  3 Credits"	},
	{0x0f, 0x01, 0x0e, 0x0c, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    3, "Lives"		},
	{0x0f, 0x01, 0x30, 0x00, "3"			},
	{0x0f, 0x01, 0x30, 0x10, "4"			},
	{0x0f, 0x01, 0x30, 0x20, "5"			},

	{0   , 0xfe, 0   ,    2, "Coinage Bonus"	},
	{0x0f, 0x01, 0x40, 0x40, "2 Coins 1 Credits"	},
	{0x0f, 0x01, 0x40, 0x00, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    2, "Language"		},
	{0x0f, 0x01, 0x80, 0x80, "English"		},
	{0x0f, 0x01, 0x80, 0x00, "Japanese"		},
};

STDDIPINFO(Pballoon)

static struct BurnSampleInfo sasukeSampleDesc[] = {
	{"hit", SAMPLE_NOLOOP },
	{"boss_start", SAMPLE_NOLOOP },
	{"shot", SAMPLE_NOLOOP },
	{"boss_attack", SAMPLE_NOLOOP },
	{"", 0 }
};

STD_SAMPLE_PICK(sasuke)
STD_SAMPLE_FN(sasuke)

static struct BurnSampleInfo vanguardSampleDesc[] = {
	{"fire", SAMPLE_NOLOOP },
	{"explsion", SAMPLE_NOLOOP },
	{"vg_voi-0", SAMPLE_NOLOOP },
	{"vg_voi-1", SAMPLE_NOLOOP },
	{"vg_voi-2", SAMPLE_NOLOOP },
	{"vg_voi-3", SAMPLE_NOLOOP },
	{"vg_voi-4", SAMPLE_NOLOOP },
	{"vg_voi-5", SAMPLE_NOLOOP },
	{"vg_voi-6", SAMPLE_NOLOOP },
	{"vg_voi-7", SAMPLE_NOLOOP },
	{"vg_voi-8", SAMPLE_NOLOOP },
	{"vg_voi-9", SAMPLE_NOLOOP },
	{"vg_voi-a", SAMPLE_NOLOOP },
	{"vg_voi-b", SAMPLE_NOLOOP },
	{"vg_voi-c", SAMPLE_NOLOOP },
	{"vg_voi-d", SAMPLE_NOLOOP },
	{"vg_voi-e", SAMPLE_NOLOOP },
	{"vg_voi-f", SAMPLE_NOLOOP },
	{"", 0 }
};

STD_SAMPLE_PICK(vanguard)
STD_SAMPLE_FN(vanguard)


static struct BurnSampleInfo fantasySampleDesc[] = {
	{"ft_voi-0", SAMPLE_NOLOOP },
	{"ft_voi-1", SAMPLE_NOLOOP },
	{"ft_voi-2", SAMPLE_NOLOOP },
	{"ft_voi-3", SAMPLE_NOLOOP },
	{"ft_voi-4", SAMPLE_NOLOOP },
	{"ft_voi-5", SAMPLE_NOLOOP },
	{"ft_voi-6", SAMPLE_NOLOOP },
	{"ft_voi-7", SAMPLE_NOLOOP },
	{"ft_voi-8", SAMPLE_NOLOOP },
	{"ft_voi-9", SAMPLE_NOLOOP },
	{"ft_voi-a", SAMPLE_NOLOOP },
	{"ft_voi-b", SAMPLE_NOLOOP },
	{"", 0 }
};

STD_SAMPLE_PICK(fantasy)
STD_SAMPLE_FN(fantasy)

static inline void character_write(INT32 offset)
{
	offset &= 0x7ff;

	UINT8 a = DrvCharRAM[offset];
	UINT8 b = DrvCharRAM[offset + 0x800];
	UINT8 *d = DrvGfxExp + (offset * 8);

	for (INT32 i = 0; i < 8; i++)
	{
		d[i ^ 7] = (((a >> i) & 1) << 1) | ((b >> i) & 1);
	}
}	

static void sasuke_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf000) == 0x1000) {
		DrvCharRAM[(address & 0xfff)^0x800] = data;
		character_write(address);
	}

	switch (address)
	{
		case 0x3000:
			// mc6845 address_w
		return;

		case 0x3001:
			// mc6845 register_w
		return;

		case 0xb000:
		case 0xb001:
			sasuke_sound_w(address&3, data);
		return;

		case 0xb002:
			flipscreen = data & 0x01;
			irqmask = data & 0x02;
		return;

		case 0xb003:
			backcolor = data & 0x03;
			DrvRecalc = 1;
		return;
	}
}	

static void satansat_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf000) == 0x1000) {
		DrvCharRAM[address & 0xfff] = data;
		character_write(address);
	}

	switch (address)
	{
		case 0x3000:
			// mc6845 address_w
		return;

		case 0x3001:
			// mc6845 register_w
		return;

		case 0xb000:
		case 0xb001:
			satansat_sound_w(address&3, data);
		return;

		case 0xb002:
			flipscreen = data & 0x01;
			irqmask = data & 0x02;
		return;

		case 0xb003:
			backcolor = data & 0x03;
			DrvRecalc = 1;
		return;
	}
}


static UINT8 sasuke_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xb004:
			return DrvInputs[0];

		case 0xb005:
			return (DrvInputs[1] & 0x7f) | (snk6502_music0_playing() ? 0x80 : 0);

		case 0xb006:
			return DrvDips[0];

		case 0xb007:
			return (DrvInputs[2] & 0x0f) | (sasuke_counter & 0xf0);
	}

	return 0;
}

static void vanguard_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf000) == 0x1000) {
		DrvCharRAM[address & 0xfff] = data;
		character_write(address);
	}

	switch (address)
	{
		case 0x3000:
			// mc6845 address_w
		return;

		case 0x3001:
			// mc6845 register_w
		return;

		case 0x3100:
		case 0x3101:
		case 0x3102:
			vanguard_sound_w(address&3, data);
		return;

		case 0x3103:
			flipscreen = data & 0x80;
			backcolor = data & 7;
			charbank = (~data & 0x08) >> 3;
			DrvRecalc = 1;
		return;

		case 0x3200:
			scrollx = data;
		return;

		case 0x3300:
			scrolly = data;
		return;

		case 0x3400:
			vanguard_speech_w(data);
		return;
	}
}

static UINT8 vanguard_main_read(UINT16 address)
{
	switch (address)
	{
		case 0x3104:
			return DrvInputs[0];

		case 0x3105:
			return DrvInputs[1];

		case 0x3106:
			return DrvDips[0];

		case 0x3107:
			return (DrvInputs[2] & 0xef) | (snk6502_music0_playing() ? 0x10 : 0);
	}

	return 0;
}

static void fantasyu_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf000) == 0x1000) {
		DrvCharRAM[address & 0xfff] = data;
		character_write(address);
	}

	switch (address)
	{
		case 0x2000:
			// mc6845 address_w
		return;

		case 0x2001:
			// mc6845 register_w
		return;

		case 0x2100:
		case 0x2101:
		case 0x2102:
			fantasy_sound_w(address&3, data);
		return;

		case 0x2103:
			flipscreen = data & 0x80;
			backcolor = data & 7;
			charbank = (~data & 0x08) >> 3;
			DrvRecalc = 1;
			fantasy_sound_w(address&3, data);
		//	some sound hw here too!!!!!!!!!!!!!!!!
		return;

		case 0x2200:
			scrollx = data;
		return;

		case 0x2300:
			scrolly = data;
		return;

		case 0x2400:
			fantasy_speech_w(data);
		return;
	}
}

static UINT8 fantasyu_main_read(UINT16 address)
{
	switch (address)
	{
		case 0x2104:
			return DrvInputs[0];

		case 0x2105:
			return DrvInputs[1];

		case 0x2106:
			return DrvDips[0];

		case 0x2107:
			return DrvInputs[2];
	}

	return 0;
}

static void pballoon_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf000) == 0x1000) {
		DrvCharRAM[address & 0xfff] = data;
		character_write(address);
	}

	switch (address)
	{
		case 0xb000:
			// mc6845 address_w
		return;

		case 0xb001:
			// mc6845 register_w
		return;

		case 0xb100:
		case 0xb101:
		case 0xb102:
			fantasy_sound_w(address&3, data);
		return;

		case 0xb103:
			flipscreen = data & 0x80;
			backcolor = data & 7;
			charbank = (~data & 0x08) >> 3;
			DrvRecalc = 1;
			fantasy_sound_w(address&3, data);
		//	some sound hw here too!!!!!!!!!!!!!!!!
		return;

		case 0xb200:
			scrollx = data;
		return;

		case 0xb300:
			scrolly = data;
		return;
	}
}

static UINT8 pballoon_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xb104:
			return DrvInputs[0];

		case 0xb105:
			return DrvInputs[1];

		case 0xb106:
			return DrvDips[0];

		case 0xb107:
			return DrvInputs[2];
	}

	return 0;
}

static tilemap_callback( ssbackground )
{
	TILE_SET_INFO(1, DrvVidRAM[offs], (DrvColRAM[offs] >> 2), 0);
}

static tilemap_callback( background )
{
	TILE_SET_INFO(1, DrvVidRAM[offs] + (256 * charbank), ((DrvColRAM[offs]&0x38) >> 3), 0);
}

static tilemap_callback( foreground )
{
	TILE_SET_INFO(0, DrvVidRAM2[offs], DrvColRAM[offs], 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	M6502Open(0);
	M6502Reset();
	M6502Close();

	// SOUND
	if (bHasSamples) BurnSampleReset();
	snk6502_sound_reset();
	for (INT32 i = 0; i < numSN; i++)
		SN76477_set_enable(i, 1); // active low enable

	DrvInputs[2] = 0;
	backcolor = 0;
	charbank = 0;
	flipscreen = 0;
	irqmask = 1; // some games don't use this so always enable irqs
	scrollx = 0;
	scrolly = 0;
	sasuke_counter = 0;

	nExtraCycles = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6502ROM		= Next; Next += 0x010000;

	DrvGfxROM		= Next; Next += 0x008000;
	DrvGfxExp		= Next; Next += 0x004000;

	DrvColPROM		= Next; Next += 0x000040;

	DrvSndROM0		= Next; Next += 0x001800;
	DrvSndROM1		= Next; Next += 0x008000;

	DrvPalette		= (UINT32*)Next; Next += 0x0040 * sizeof(UINT32);

	AllRam			= Next;

	DrvM6502RAM		= Next; Next += 0x000400;
	DrvVidRAM2		= Next; Next += 0x000400;
	DrvVidRAM		= Next; Next += 0x000400;
	DrvColRAM		= Next; Next += 0x000400;
	DrvCharRAM		= Next; Next += 0x001000;

	RamEnd			= Next;

	FilterBUF       = (INT16*)Next; Next += 0x002000;

	MemEnd			= Next;

	return 0;
}

static void DrvGfxDecode(UINT8 *src, UINT8 *dst, INT32 len)
{
	for (INT32 i = 0; i < (len / 2) * 8; i++)
	{
		INT32 a = (src[(len / 2) * 0 + (i / 8)] >> (~i & 7)) & 1;
		INT32 b = (src[(len / 2) * 1 + (i / 8)] >> (~i & 7)) & 1;

		dst[i] = (a << 1) | b; // right?
	}
}

static void DrvSoundInit(INT32 type)
{
	game_type = type;
	snk6502_sound_init(DrvSndROM0);

	if (type == 0) { // 0 vanguard
		numSN = 2;
		SN76477_init(0);
		SN76477_set_mastervol(0, 3.00);
		SN76477_set_noise_params(0, RES_K(470), RES_M(1.5), CAP_P(220));
		SN76477_set_decay_res(0, 0);
		SN76477_set_attack_params(0, 0, 0);
		SN76477_set_amplitude_res(0, RES_K(47));
		SN76477_set_feedback_res(0, RES_K(4.7));
		SN76477_set_vco_params(0, 0, 0, 0);
		SN76477_set_pitch_voltage(0, 0);
		SN76477_set_slf_params(0, 0, 0);
		SN76477_set_oneshot_params(0, 0, 0);
		SN76477_set_vco_mode(0, 0);
		SN76477_set_mixer_params(0, 0, 1, 0);
		SN76477_set_envelope_params(0, 1, 1);
		SN76477_set_enable(0, 1);

		SN76477_init(1);
		SN76477_set_mastervol(1, 3.00);
		SN76477_set_noise_params(1, RES_K(10), RES_M(30), 0);
		SN76477_set_decay_res(1, 0);
		SN76477_set_attack_params(1, 0, 0);
		SN76477_set_amplitude_res(1, RES_K(47));
		SN76477_set_feedback_res(1, RES_K(4.7));
		SN76477_set_vco_params(1, 0, 0, 0);
		SN76477_set_pitch_voltage(1, 0);
		SN76477_set_slf_params(1, 0, 0);
		SN76477_set_oneshot_params(1, 0, 0);
		SN76477_set_vco_mode(1, 0);
		SN76477_set_mixer_params(1, 0, 1, 0);
		SN76477_set_envelope_params(1, 0, 1);
		SN76477_set_enable(1, 1);

		snk6502_set_music_clock(1 / 41.6);
	}

	if (type & (1|2)) { // fantasy, nibbler, pballoon
		numSN = 1;
		SN76477_init(0);
		// bomb noise with an envelope for nibbler/pballoon/fantasy -dink
		SN76477_set_mastervol(0, 5.10); // first, parameters below are calculated from this
		SN76477_set_noise_params(0, RES_K(47), RES_K(150), CAP_U(0.001));
		SN76477_set_decay_res(0, RES_M(3.3));
		SN76477_set_attack_params(0, CAP_U(1), RES_K(4.7));
		SN76477_set_amplitude_res(0, RES_K(200));
		SN76477_set_feedback_res(0, RES_K(55));
		SN76477_set_vco_params(0, 0, 0, 0);
		SN76477_set_pitch_voltage(0, 0);
		SN76477_set_slf_params(0, 0, 0);
		SN76477_set_oneshot_params(0, CAP_U(2.2), RES_K(4.7));
		SN76477_set_vco_mode(0, 0);
		SN76477_set_mixer_params(0, 0, 1, 0);
		SN76477_set_envelope_params(0, 0, 0);
		SN76477_set_enable(0, 1);
		SN76477_envelope_w(0, 1);

		if (type & 2) // pballoon
			snk6502_set_music_clock(1 / 40.3);
	}

	if (type & (4|8)) { // sasuke, satansat
		numSN = 3;
		SN76477_init(0);
		SN76477_set_mastervol(0, 1.00);
		SN76477_set_noise_params(0, RES_K(470), RES_K(150), CAP_P(4700));
		SN76477_set_decay_res(0, RES_K(22));
		SN76477_set_attack_params(0, CAP_U(10), RES_K(10));
		SN76477_set_amplitude_res(0, RES_K(100));
		SN76477_set_feedback_res(0, RES_K(47));
		SN76477_set_vco_params(0, 0, 0, 0);
		SN76477_set_pitch_voltage(0, 0);
		SN76477_set_slf_params(0, 0, RES_K(10));
		SN76477_set_oneshot_params(0, CAP_U(2.2), RES_K(100));
		SN76477_set_vco_mode(0, 0);
		SN76477_set_mixer_params(0, 0, 1, 0);
		SN76477_set_envelope_params(0, 1, 0);
		SN76477_set_enable(0, 1);

		SN76477_init(1);
		SN76477_set_mastervol(1, 1.00);
		SN76477_set_noise_params(1, RES_K(340), RES_K(47), CAP_P(100));
		SN76477_set_decay_res(1, RES_K(470));
		SN76477_set_attack_params(1, CAP_U(4.7), RES_K(10));
		SN76477_set_amplitude_res(1, RES_K(100));
		SN76477_set_feedback_res(1, RES_K(47));
		SN76477_set_vco_params(1, 0, CAP_P(220), RES_K(1000));
		SN76477_set_pitch_voltage(1, 0);
		SN76477_set_slf_params(1, 0, RES_K(220));
		SN76477_set_oneshot_params(1, CAP_U(22), RES_K(47));
		SN76477_set_vco_mode(1, 1);
		SN76477_set_mixer_params(1, 0, 1, 0);
		SN76477_set_envelope_params(1, 1, 1);
		SN76477_set_enable(1, 1);

		SN76477_init(2);
		SN76477_set_mastervol(2, 1.00);
		SN76477_set_noise_params(2, RES_K(330), RES_K(47), CAP_P(100));
		SN76477_set_decay_res(2, RES_K(1));
		SN76477_set_attack_params(2, 0, RES_K(1));
		SN76477_set_amplitude_res(2, RES_K(100));
		SN76477_set_feedback_res(2, RES_K(47));
		SN76477_set_vco_params(2, 0, CAP_P(1000), RES_K(1000));
		SN76477_set_pitch_voltage(2, 0);
		SN76477_set_slf_params(2, CAP_U(1), RES_K(10));
		SN76477_set_oneshot_params(2, CAP_U(2.2), RES_K(150));
		SN76477_set_vco_mode(2, 0);
		SN76477_set_mixer_params(2, 1, 1, 0);
		SN76477_set_envelope_params(2, 1, 0);
		SN76477_set_enable(2, 1);

		if (type == 4) {
			snk6502_set_music_clock(M_LN2 * (RES_K(18) + RES_K(1)) * CAP_U(1));
			snk6502_set_music_freq(35300);
		}
	}

	if (type == 8) { // satansat
		// continued from above...
		SN76477_set_mastervol(0, 1.00);
		SN76477_set_noise_params(0, RES_K(470), RES_M(1.5), CAP_P(220));
		SN76477_set_decay_res(0, 0);
		SN76477_set_attack_params(0, 0, 0);
		SN76477_set_amplitude_res(0, RES_K(47));
		SN76477_set_feedback_res(0, RES_K(47));
		SN76477_set_vco_params(0, 0, 0, 0);
		SN76477_set_pitch_voltage(0, 0);
		SN76477_set_slf_params(0, 0, 0);
		SN76477_set_oneshot_params(0, 0, 0);
		SN76477_set_vco_mode(0, 0);
		SN76477_set_mixer_params(0, 0, 1, 0);
		SN76477_set_envelope_params(0, 1, 1);
		SN76477_set_enable(0, 1);

		snk6502_set_music_freq(38000);
	}

	{ // filter (for nibbler)
#define SampleFreq 44100.0
#define CutFreq 1000.0
#define Q 0.4
#define Gain 1.0
#define CutFreq2 1000.0
#define Q2 0.3
#define Gain2 1.475
		LP1 = new LowPass2(CutFreq, SampleFreq, Q, Gain,
						   CutFreq2, Q2, Gain2);
		LP2 = new LowPass2(CutFreq, SampleFreq, Q, Gain,
						   CutFreq2, Q2, Gain2);
	}

	BurnSampleInit(1);
	bHasSamples = BurnSampleGetStatus(0) != -1;
	if (bHasSamples) {
		BurnSampleSetAllRoutesAllSamples(0.30, BURN_SND_ROUTE_BOTH);
		bprintf(0, _T("Loaded samples..\n"));
	}
}

static void DrvSoundExit()
{
	SN76477_exit();

	BurnSampleExit();

	delete LP1;
	delete LP2;
	LP1 = NULL;
	LP2 = NULL;
}

static INT32 SatansatInit()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvM6502ROM + 0x4000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x4800,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x5000,  2, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x5800,  3, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x6000,  4, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x6800,  5, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x7000,  6, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x7800,  7, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x8000,  8, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x8800,  9, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x9000, 10, 1)) return 1;

		if (BurnLoadRom(DrvGfxExp   + 0x0000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxExp   + 0x0800, 12, 1)) return 1;

		if (BurnLoadRom(DrvColPROM  + 0x0000, 13, 1)) return 1;

		if (BurnLoadRom(DrvSndROM0  + 0x0000, 14, 1)) return 1;
		if (BurnLoadRom(DrvSndROM0  + 0x0800, 15, 1)) return 1;

		DrvGfxDecode(DrvGfxExp, DrvGfxROM, 0x1000);
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvM6502RAM,		0x0000, 0x03ff, MAP_RAM);
	M6502MapMemory(DrvVidRAM2,		0x0400, 0x07ff, MAP_RAM);
	M6502MapMemory(DrvVidRAM,		0x0800, 0x0bff, MAP_RAM);
	M6502MapMemory(DrvColRAM,		0x0c00, 0x0fff, MAP_RAM);
	M6502MapMemory(DrvCharRAM,		0x1000, 0x1fff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM + 0x4000,	0x4000, 0x9fff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM + 0x7800,	0xf800, 0xffff, MAP_ROM);
	M6502SetWriteHandler(satansat_main_write);
	M6502SetReadHandler(sasuke_main_read);
	M6502Close();

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, ssbackground_map_callback, 8, 8, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, foreground_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxExp, 2, 8, 8, 0x04000, 0x00, 3);
	GenericTilemapSetGfx(1, DrvGfxROM, 2, 8, 8, 0x04000, 0x10, 3);
	GenericTilemapSetTransparent(1, 0);

	DrvSoundInit(8);

	DrvDoReset();

	return 0;
}

static INT32 SatansatindInit()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvM6502ROM + 0x4000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x4800,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x5000,  2, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x5800,  3, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x6000,  4, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x6800,  5, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x7000,  6, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x7800,  7, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x8000,  8, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x8800,  9, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x9000, 10, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x9800, 11, 1)) return 1;

		if (BurnLoadRom(DrvGfxExp   + 0x0000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxExp   + 0x0800, 13, 1)) return 1;

		if (BurnLoadRom(DrvColPROM  + 0x0000, 14, 1)) return 1;

		if (BurnLoadRom(DrvSndROM0  + 0x0000, 15, 1)) return 1;
		if (BurnLoadRom(DrvSndROM0  + 0x0800, 16, 1)) return 1;

		DrvGfxDecode(DrvGfxExp, DrvGfxROM, 0x1000);
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvM6502RAM,		0x0000, 0x03ff, MAP_RAM);
	M6502MapMemory(DrvVidRAM2,		0x0400, 0x07ff, MAP_RAM);
	M6502MapMemory(DrvVidRAM,		0x0800, 0x0bff, MAP_RAM);
	M6502MapMemory(DrvColRAM,		0x0c00, 0x0fff, MAP_RAM);
	M6502MapMemory(DrvCharRAM,		0x1000, 0x1fff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM + 0x4000,	0x4000, 0x9fff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM + 0x7800,	0xf800, 0xffff, MAP_ROM);
	M6502SetWriteHandler(satansat_main_write);
	M6502SetReadHandler(sasuke_main_read);
	M6502Close();

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, ssbackground_map_callback, 8, 8, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, foreground_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxExp, 2, 8, 8, 0x04000, 0x00, 3);
	GenericTilemapSetGfx(1, DrvGfxROM, 2, 8, 8, 0x04000, 0x10, 3);
	GenericTilemapSetTransparent(1, 0);

	DrvSoundInit(8);

	DrvDoReset();

	return 0;
}

static INT32 SasukeInit()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvM6502ROM + 0x4000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x4800,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x5000,  2, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x5800,  3, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x6000,  4, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x6800,  5, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x7000,  6, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x7800,  7, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x8000,  8, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x8800,  9, 1)) return 1;

		if (BurnLoadRom(DrvGfxExp   + 0x0800, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxExp   + 0x0000, 11, 1)) return 1;

		if (BurnLoadRom(DrvColPROM  + 0x0000, 12, 1)) return 1;

		if (BurnLoadRom(DrvSndROM0  + 0x0000, 13, 1)) return 1;

		DrvGfxDecode(DrvGfxExp, DrvGfxROM, 0x1000);
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvM6502RAM,		0x0000, 0x03ff, MAP_RAM);
	M6502MapMemory(DrvVidRAM2,		0x0400, 0x07ff, MAP_RAM);
	M6502MapMemory(DrvVidRAM,		0x0800, 0x0bff, MAP_RAM);
	M6502MapMemory(DrvColRAM,		0x0c00, 0x0fff, MAP_RAM);
	M6502MapMemory(DrvCharRAM + 0x800,	0x1000, 0x17ff, MAP_ROM);
	M6502MapMemory(DrvCharRAM,		0x1800, 0x1fff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM + 0x4000,	0x4000, 0x9fff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM + 0x7800,	0xf800, 0xffff, MAP_ROM);
	M6502SetWriteHandler(sasuke_main_write);
	M6502SetReadHandler(sasuke_main_read);
	M6502Close();

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, ssbackground_map_callback, 8, 8, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, foreground_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxExp, 2, 8, 8, 0x04000, 0x00, 3);
	GenericTilemapSetGfx(1, DrvGfxROM, 2, 8, 8, 0x04000, 0x10, 3);
	GenericTilemapSetTransparent(1, 0);

	DrvSoundInit(4);

	DrvDoReset();

	return 0;
}

static INT32 VanguardInit()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvM6502ROM + 0x4000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x5000,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x6000,  2, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x7000,  3, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x8000,  4, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x9000,  5, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0xa000,  6, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0xb000,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxExp   + 0x0000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxExp   + 0x0800,  9, 1)) return 1;

		if (BurnLoadRom(DrvColPROM  + 0x0000, 10, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x0020, 11, 1)) return 1;

		if (BurnLoadRom(DrvSndROM0  + 0x0000, 12, 1)) return 1;
		if (BurnLoadRom(DrvSndROM0  + 0x0800, 13, 1)) return 1;

		if (BurnLoadRom(DrvSndROM1  + 0x4000, 14, 1)) return 1;
		if (BurnLoadRom(DrvSndROM1  + 0x4800, 15, 1)) return 1;
		if (BurnLoadRom(DrvSndROM1  + 0x5000, 16, 1)) return 1;

		DrvGfxDecode(DrvGfxExp, DrvGfxROM, 0x1000);
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvM6502RAM,		0x0000, 0x03ff, MAP_RAM);
	M6502MapMemory(DrvVidRAM2,		0x0400, 0x07ff, MAP_RAM);
	M6502MapMemory(DrvVidRAM,		0x0800, 0x0bff, MAP_RAM);
	M6502MapMemory(DrvColRAM,		0x0c00, 0x0fff, MAP_RAM);
	M6502MapMemory(DrvCharRAM,		0x1000, 0x1fff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM + 0x4000,	0x4000, 0xbfff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM + 0x8000,	0xf000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(vanguard_main_write);
	M6502SetReadHandler(vanguard_main_read);
	M6502Close();

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, background_map_callback, 8, 8, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, foreground_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxExp, 2, 8, 8, 0x04000, 0x00, 7);
	GenericTilemapSetGfx(1, DrvGfxROM, 2, 8, 8, 0x04000, 0x20, 7);
	GenericTilemapSetTransparent(1, 0);

	DrvSoundInit(0);

	DrvDoReset();

	return 0;
}

static INT32 FantasyuInit()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvM6502ROM + 0x3000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x4000,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x5000,  2, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x6000,  3, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x7000,  4, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x8000,  5, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x9000,  6, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0xa000,  7, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0xb000,  8, 1)) return 1;

		if (BurnLoadRom(DrvGfxExp   + 0x0000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxExp   + 0x1000, 10, 1)) return 1;

		if (BurnLoadRom(DrvColPROM  + 0x0000, 11, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x0020, 12, 1)) return 1;

		if (BurnLoadRom(DrvSndROM0  + 0x0000, 13, 1)) return 1;
		if (BurnLoadRom(DrvSndROM0  + 0x0800, 14, 1)) return 1;
		if (BurnLoadRom(DrvSndROM0  + 0x1000, 15, 1)) return 1;

		if (BurnLoadRom(DrvSndROM1  + 0x4000, 16, 1)) return 1;
		if (BurnLoadRom(DrvSndROM1  + 0x4800, 17, 1)) return 1;
		if (BurnLoadRom(DrvSndROM1  + 0x5000, 18, 1)) return 1;

		DrvGfxDecode(DrvGfxExp, DrvGfxROM, 0x2000);
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvM6502RAM,		0x0000, 0x03ff, MAP_RAM);
	M6502MapMemory(DrvVidRAM2,		0x0400, 0x07ff, MAP_RAM);
	M6502MapMemory(DrvVidRAM,		0x0800, 0x0bff, MAP_RAM);
	M6502MapMemory(DrvColRAM,		0x0c00, 0x0fff, MAP_RAM);
	M6502MapMemory(DrvCharRAM,		0x1000, 0x1fff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM + 0x3000,	0x3000, 0xbfff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM + 0x8000,	0xf000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(fantasyu_main_write);
	M6502SetReadHandler(fantasyu_main_read);
	M6502Close();

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, background_map_callback, 8, 8, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, foreground_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxExp, 2, 8, 8, 0x04000, 0x00, 7);
	GenericTilemapSetGfx(1, DrvGfxROM, 2, 8, 8, 0x08000, 0x20, 7);
	GenericTilemapSetTransparent(1, 0);

	DrvSoundInit(1);

	DrvDoReset();

	return 0;
}

static INT32 NibblerInit()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvM6502ROM + 0x3000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x4000,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x5000,  2, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x6000,  3, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x7000,  4, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x8000,  5, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x9000,  6, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0xa000,  7, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0xb000,  8, 1)) return 1;

		if (BurnLoadRom(DrvGfxExp   + 0x0000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxExp   + 0x1000, 10, 1)) return 1;

		if (BurnLoadRom(DrvColPROM  + 0x0000, 11, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x0020, 12, 1)) return 1;

		memset(DrvSndROM0, 0xff, 0x1800);
		if (BurnLoadRom(DrvSndROM0  + 0x0800, 13, 1)) return 1;
		if (BurnLoadRom(DrvSndROM0  + 0x1000, 14, 1)) return 1;

		DrvGfxDecode(DrvGfxExp, DrvGfxROM, 0x2000);
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvM6502RAM,		0x0000, 0x03ff, MAP_RAM);
	M6502MapMemory(DrvVidRAM2,		0x0400, 0x07ff, MAP_RAM);
	M6502MapMemory(DrvVidRAM,		0x0800, 0x0bff, MAP_RAM);
	M6502MapMemory(DrvColRAM,		0x0c00, 0x0fff, MAP_RAM);
	M6502MapMemory(DrvCharRAM,		0x1000, 0x1fff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM + 0x3000,	0x3000, 0xbfff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM + 0x8000,	0xf000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(fantasyu_main_write);
	M6502SetReadHandler(fantasyu_main_read);
	M6502Close();

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, background_map_callback, 8, 8, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, foreground_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxExp, 2, 8, 8, 0x04000, 0x00, 7);
	GenericTilemapSetGfx(1, DrvGfxROM, 2, 8, 8, 0x08000, 0x20, 7);
	GenericTilemapSetTransparent(1, 0);

	DrvSoundInit(1);

	DrvDoReset();

	return 0;
}

static INT32 NibblerpInit()
{
	INT32 rc = NibblerInit();

	if (!rc) {
		BurnLoadRom(DrvSndROM0  + 0x0000, 13, 1);
		BurnLoadRom(DrvSndROM0  + 0x0800, 14, 1);
		BurnLoadRom(DrvSndROM0  + 0x1000, 15, 1);
	}

	return rc;
}

static INT32 PballoonInit()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvM6502ROM + 0x3000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x4000,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x5000,  2, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x6000,  3, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x7000,  4, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x8000,  5, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x9000,  6, 1)) return 1;

		if (BurnLoadRom(DrvGfxExp   + 0x0000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxExp   + 0x1000,  8, 1)) return 1;

		if (BurnLoadRom(DrvColPROM  + 0x0000,  9, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x0020, 10, 1)) return 1;

		if (BurnLoadRom(DrvSndROM0  + 0x0000, 11, 1)) return 1;
		if (BurnLoadRom(DrvSndROM0  + 0x0800, 12, 1)) return 1;
		if (BurnLoadRom(DrvSndROM0  + 0x1000, 13, 1)) return 1;

		DrvGfxDecode(DrvGfxExp, DrvGfxROM, 0x2000);
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvM6502RAM,		0x0000, 0x03ff, MAP_RAM);
	M6502MapMemory(DrvVidRAM2,		0x0400, 0x07ff, MAP_RAM);
	M6502MapMemory(DrvVidRAM,		0x0800, 0x0bff, MAP_RAM);
	M6502MapMemory(DrvColRAM,		0x0c00, 0x0fff, MAP_RAM);
	M6502MapMemory(DrvCharRAM,		0x1000, 0x1fff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM + 0x3000,	0x3000, 0x9fff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM + 0x8000,	0xf000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(pballoon_main_write);
	M6502SetReadHandler(pballoon_main_read);
	M6502Close();

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, background_map_callback, 8, 8, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, foreground_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxExp, 2, 8, 8, 0x04000, 0x00, 7);
	GenericTilemapSetGfx(1, DrvGfxROM, 2, 8, 8, 0x08000, 0x20, 7);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);
	GenericTilemapSetTransparent(1, 0);

	DrvSoundInit(2);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6502Exit();

	DrvSoundExit();

	bHasSamples = 0;

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x40; i++)
	{
		int bit0, bit1, bit2, r, g, b;

		bit0 = (DrvColPROM[i] >> 0) & 0x01;
		bit1 = (DrvColPROM[i] >> 1) & 0x01;
		bit2 = (DrvColPROM[i] >> 2) & 0x01;
		r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = (DrvColPROM[i] >> 3) & 0x01;
		bit1 = (DrvColPROM[i] >> 4) & 0x01;
		bit2 = (DrvColPROM[i] >> 5) & 0x01;
		g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = 0;
		bit1 = (DrvColPROM[i] >> 6) & 0x01;
		bit2 = (DrvColPROM[i] >> 7) & 0x01;

		b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}

	// set background color
	UINT32 c = DrvPalette[0x20 + backcolor * 4];

	for (INT32 i = 0 ;i < 0x20; i += 4) {
		DrvPalette[i + 0x20] = c;
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 1;
	}

	BurnTransferClear();

	GenericTilemapSetScrollX(0, scrollx);
	GenericTilemapSetScrollY(0, scrolly);

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);
	if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void SatansatPaletteInit()
{
	UINT32 pal[0x20];

	for (INT32 i = 0; i < 0x20; i++)
	{
		int bit0, bit1, bit2, r, g, b;

		bit0 = (DrvColPROM[i] >> 0) & 0x01;
		bit1 = (DrvColPROM[i] >> 1) & 0x01;
		bit2 = (DrvColPROM[i] >> 2) & 0x01;
		r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = (DrvColPROM[i] >> 3) & 0x01;
		bit1 = (DrvColPROM[i] >> 4) & 0x01;
		bit2 = (DrvColPROM[i] >> 5) & 0x01;
		g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = 0;
		bit1 = (DrvColPROM[i] >> 6) & 0x01;
		bit2 = (DrvColPROM[i] >> 7) & 0x01;

		b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		pal[i] = BurnHighCol(r,g,b,0);
	}

	for (INT32 i = 0; i < 0x10; i++) {
		DrvPalette[i] = pal[(4 * (i & 3)) + (i / 4)];
		DrvPalette[i + 0x10] = pal[(4 * (i & 3)) + (i / 4) + 0x10];
	}

	// set background color
	UINT32 c = DrvPalette[0x10 + backcolor];

	for (INT32 i = 0; i < 0x10; i += 4) {
		DrvPalette[i + 0x10] = c;
	}
}

static INT32 SatansatDraw()
{
	if (DrvRecalc) {
		SatansatPaletteInit();
		DrvRecalc = 1;
	}

	BurnTransferClear();

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);
	if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		INT32 prevcoin = DrvInputs[2] & 3;

		memset (DrvInputs, 0, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		if (prevcoin != (DrvInputs[2] & 3)) {
			M6502Open(0);
 			if (!(DrvInputs[2] & 3)) M6502SetIRQLine(0x20, CPU_IRQSTATUS_AUTO); // nmi on key up for satansat & sasuke
			M6502Close();
		}

		if (nCurrentFrame & 1) sasuke_counter += 0x10;
	}

	INT32 nInterleave = 262;
	INT32 nCyclesTotal[1] = { 705562 / 60 };
	INT32 nCyclesDone[1] = { nExtraCycles };

	M6502Open(0);
	for (INT32 i = 0; i < nInterleave; i++) {
		CPU_RUN(0, M6502);
		if (i == (nInterleave-1) && irqmask) M6502SetIRQLine(0, CPU_IRQSTATUS_HOLD);
	}
	M6502Close();

	nExtraCycles = nCyclesDone[0] - nCyclesTotal[0];

#if 0
	if (pBurnSoundOut) {
		snk_sound_update(pBurnSoundOut, nBurnSoundLen);
		for (INT32 i = 0; i < numSN; i++)
			SN76477_sound_update(i, pBurnSoundOut, nBurnSoundLen);
		if (bHasSamples)
			BurnSampleRender(pBurnSoundOut, nBurnSoundLen);
	}
#endif
	if (pBurnSoundOut) {
		snk_sound_update(pBurnSoundOut, nBurnSoundLen);
		memset(FilterBUF, 0, 0x2000);
		SN76477_sound_update(FilterBUF, nBurnSoundLen);
		if (LP1 && LP2) {
			LP1->Filter(FilterBUF, nBurnSoundLen);  // Left
			LP2->Filter(FilterBUF+1, nBurnSoundLen); // Right
		}

		for (INT32 i = 0; i < nBurnSoundLen; i++) {
			pBurnSoundOut[(i<<1)+0] = BURN_SND_CLIP(pBurnSoundOut[(i<<1)+0] + FilterBUF[(i<<1)+0]);
			pBurnSoundOut[(i<<1)+1] = BURN_SND_CLIP(pBurnSoundOut[(i<<1)+1] + FilterBUF[(i<<1)+1]);
		}

		if (bHasSamples)
			BurnSampleRender(pBurnSoundOut, nBurnSoundLen);
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
		*pnMin = 0x029722;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		M6502Scan(nAction);

		snk6502_sound_savestate(nAction, pnMin);
		SN76477_scan(nAction, pnMin);

		SCAN_VAR(backcolor);
		SCAN_VAR(charbank);
		SCAN_VAR(flipscreen);
		SCAN_VAR(irqmask);
		SCAN_VAR(scrollx);
		SCAN_VAR(scrolly);

		SCAN_VAR(sasuke_counter);

		SCAN_VAR(nExtraCycles);
	}

	if (nAction & ACB_WRITE) {
		for (INT32 i = 0; i < 0x800; i++) { // recalc ram chars
			character_write(i);
		}
	}

	return 0;
}


// Sasuke vs. Commander (set 1)

static struct BurnRomInfo sasukeRomDesc[] = {
	{ "sc1",		0x0800, 0x34cbbe03, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 code
	{ "sc2",		0x0800, 0x38cc14f0, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sc3",		0x0800, 0x54c41285, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sc4",		0x0800, 0x23edafcf, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "sc5",		0x0800, 0xca410e4f, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "sc6",		0x0800, 0x80406afb, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "sc7",		0x0800, 0x04d0f104, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "sc8",		0x0800, 0x0219104b, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "sc9",		0x0800, 0xd6ff889a, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "sc10",		0x0800, 0x19df6b9a, 1 | BRF_PRG | BRF_ESS }, //  9

	{ "mcs_c",		0x0800, 0xaff9743d, 2 | BRF_GRA },           // 10 Background tiles
	{ "mcs_d",		0x0800, 0x9c805120, 2 | BRF_GRA },           // 11

	{ "sasuke.clr",		0x0020, 0xb70f34c1, 3 | BRF_GRA },           // 12 Color data

	{ "sc11",		0x0800, 0x24a0e121, 4 | BRF_GRA },           // 13 Custom sound data
};

STD_ROM_PICK(sasuke)
STD_ROM_FN(sasuke)

struct BurnDriver BurnDrvSasuke = {
	"sasuke", NULL, NULL, "sasuke", "1980",
	"Sasuke vs. Commander (set 1)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, sasukeRomInfo, sasukeRomName, NULL, NULL, sasukeSampleInfo, sasukeSampleName, SasukeInputInfo, SasukeDIPInfo,
	SasukeInit, DrvExit, DrvFrame, SatansatDraw, DrvScan, &DrvRecalc, 0x20,
	224, 256, 3, 4
};


// Sasuke vs. Commander (set 2)

static struct BurnRomInfo sasukeaRomDesc[] = {
	{ "sc1",		0x0800, 0x34cbbe03, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 code
	{ "sc2",		0x0800, 0x38cc14f0, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sc3",		0x0800, 0xc7a0c668, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sc4",		0x0800, 0x23edafcf, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "sc5",		0x0800, 0xca410e4f, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "sc6",		0x0800, 0xd97e98fa, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "sc7",		0x0800, 0x04d0f104, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "sc8",		0x0800, 0x1893a1d3, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "sc9",		0x0800, 0x681dc3c5, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "sc10",		0x0800, 0x19df6b9a, 1 | BRF_PRG | BRF_ESS }, //  9

	{ "mcs_c",		0x0800, 0xaff9743d, 2 | BRF_GRA },           // 10 Background tiles
	{ "mcs_d",		0x0800, 0x9c805120, 2 | BRF_GRA },           // 11

	{ "sasuke.clr",		0x0020, 0xb70f34c1, 3 | BRF_GRA },           // 12 Color data

	{ "sc11",		0x0800, 0x24a0e121, 4 | BRF_GRA },           // 13 Custom sound data
};

STD_ROM_PICK(sasukea)
STD_ROM_FN(sasukea)

struct BurnDriver BurnDrvSasukea = {
	"sasukea", "sasuke", NULL, "sasuke", "1980",
	"Sasuke vs. Commander (set 2)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, sasukeaRomInfo, sasukeaRomName, NULL, NULL, sasukeSampleInfo, sasukeSampleName, SasukeInputInfo, SasukeaDIPInfo,
	SasukeInit, DrvExit, DrvFrame, SatansatDraw, DrvScan, &DrvRecalc, 0x20,
	224, 256, 3, 4
};


// Satan of Saturn (set 1)

static struct BurnRomInfo satansatRomDesc[] = {
	{ "ss1",		0x0800, 0x549dd13a, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 code
	{ "ss2",		0x0800, 0x04972fa8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ss3",		0x0800, 0x9caf9057, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ss4",		0x0800, 0xe1bdcfe1, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ss5",		0x0800, 0xd454de19, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "ss6",		0x0800, 0x7fbd5d30, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "zarz128.15",		0x0800, 0x93ea2df9, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "zarz129.16",		0x0800, 0xe67ec873, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "zarz130.22",		0x0800, 0x22c44650, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "ss10",		0x0800, 0x8f1b313a, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "ss11",		0x0800, 0xe74f98e0, 1 | BRF_PRG | BRF_ESS }, // 10

	{ "zarz135.73",		0x0800, 0xe837c62b, 2 | BRF_GRA },           // 11 Background tiles
	{ "zarz136.75",		0x0800, 0x83f61623, 2 | BRF_GRA },           // 12

	{ "zarz138.03",		0x0020, 0x5dd6933a, 3 | BRF_GRA },           // 13 Color data

	{ "ss12",		    0x0800, 0xdee01f24, 4 | BRF_SND },           // 14 Custom sound data
	{ "zarz134.54",		0x0800, 0x580934d2, 4 | BRF_SND },           // 15
};

STD_ROM_PICK(satansat)
STD_ROM_FN(satansat)

struct BurnDriver BurnDrvSatansat = {
	"satansat", NULL, NULL, "vanguard", "1981",
	"Satan of Saturn (set 1)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, satansatRomInfo, satansatRomName, NULL, NULL, vanguardSampleInfo, vanguardSampleName, SatansatInputInfo, SatansatDIPInfo,
	SatansatInit, DrvExit, DrvFrame, SatansatDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};



// Satan of Saturn (set 2)

static struct BurnRomInfo satansataRomDesc[] = {
	{ "ic7.bin",		0x0800, 0x549dd13a, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 code
	{ "ic8.bin",		0x0800, 0x04972fa8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ic9.bin",		0x0800, 0x9caf9057, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ic10.bin",		0x0800, 0xe1bdcfe1, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ic13.bin",		0x0800, 0xd454de19, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "ic14.bin",		0x0800, 0x7fbd5d30, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "ic15.bin",		0x0800, 0x93ea2df9, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "ic16.bin",		0x0800, 0x9ec5fe09, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "ic22.bin",		0x0800, 0x21092f1f, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "ic23.bin",		0x0800, 0x8f1b313a, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "ic24.bin",		0x0800, 0xe74f98e0, 1 | BRF_PRG | BRF_ESS }, // 10

	{ "ic73.bin",		0x0800, 0xe837c62b, 2 | BRF_GRA },           // 11 Background tiles
	{ "ic75.bin",		0x0800, 0x83f61623, 2 | BRF_GRA },           // 12

	{ "zarz138.03",		0x0020, 0x5dd6933a, 3 | BRF_GRA },           // 13 Color data

	{ "ic53.bin",		0x0800, 0x8cb95a6b, 4 | BRF_SND },           // 14 Custom sound data
	{ "ic54.bin",		0x0800, 0x580934d2, 4 | BRF_SND },           // 15
};

STD_ROM_PICK(satansata)
STD_ROM_FN(satansata)

struct BurnDriver BurnDrvSatansata = {
	"satansata", "satansat", NULL, "vanguard", "1981",
	"Satan of Saturn (set 2)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, satansataRomInfo, satansataRomName, NULL, NULL, vanguardSampleInfo, vanguardSampleName, SatansatInputInfo, SatansatDIPInfo,
	SatansatInit, DrvExit, DrvFrame, SatansatDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Zarzon

static struct BurnRomInfo zarzonRomDesc[] = {
	{ "zarz122.07",		0x0800, 0xbdfa67e2, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 code
	{ "zarz123.08",		0x0800, 0xd034e61e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "zarz124.09",		0x0800, 0x296397ea, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "zarz125.10",		0x0800, 0x26dc5e66, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "zarz126.13",		0x0800, 0xcee18d7f, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "zarz127.14",		0x0800, 0xbbd2cc0d, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "zarz128.15",		0x0800, 0x93ea2df9, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "zarz129.16",		0x0800, 0xe67ec873, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "zarz130.22",		0x0800, 0x22c44650, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "zarz131.23",		0x0800, 0x7be20678, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "zarz132.24",		0x0800, 0x72b2cb76, 1 | BRF_PRG | BRF_ESS }, // 10

	{ "zarz135.73",		0x0800, 0xe837c62b, 2 | BRF_GRA },           // 11 Background tiles
	{ "zarz136.75",		0x0800, 0x83f61623, 2 | BRF_GRA },           // 12

	{ "zarz138.03",		0x0020, 0x5dd6933a, 3 | BRF_GRA },           // 13 Color data

	{ "zarz133.53",		0x0800, 0xb253cf78, 4 | BRF_SND },           // 14 Custom sound data
	{ "zarz134.54",		0x0800, 0x580934d2, 4 | BRF_SND },           // 15
};

STD_ROM_PICK(zarzon)
STD_ROM_FN(zarzon)

struct BurnDriver BurnDrvZarzon = {
	"zarzon", "satansat", NULL, "vanguard", "1981",
	"Zarzon\0", NULL, "SNK (Taito America license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, zarzonRomInfo, zarzonRomName, NULL, NULL, vanguardSampleInfo, vanguardSampleName, SatansatInputInfo, SatansatDIPInfo,
	SatansatInit, DrvExit, DrvFrame, SatansatDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Satan of Saturn (Inder S.A., bootleg)

static struct BurnRomInfo satansatindRomDesc[] = {
	{ "ss01.rom",		0x0800, 0x7f16f8fe, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 code
	{ "ss02.rom",		0x0800, 0x04972fa8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ss03.rom",		0x0800, 0x6e0077e8, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ss04.rom",		0x0800, 0xf9e33359, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ss05.rom",		0x0800, 0xf771e007, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "ss06.rom",		0x0800, 0xe5b02850, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "ss07.rom",		0x0800, 0x93ea2df9, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "ss08.rom",		0x0800, 0xe67ec873, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "ss09.rom",		0x0800, 0x22c44650, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "ss10.rom",		0x0800, 0x8f1b313a, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "ss11.rom",		0x0800, 0x3cbcbddb, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "ss16.rom",		0x0800, 0x20bd6ee4, 1 | BRF_PRG | BRF_ESS }, // 11

	{ "ss14.rom",		0x0800, 0x5dfcd508, 2 | BRF_GRA },           // 12 Background tiles
	{ "ss15.rom",		0x0800, 0x363d0500, 2 | BRF_GRA },           // 13

	{ "zarz138.03",		0x0020, 0x5dd6933a, 3 | BRF_GRA },           // 14 Color data

	{ "ss12.rom",		0x0800, 0xdee01f24, 4 | BRF_SND },           // 15 Custom sound data
	{ "ss13.rom",		0x0800, 0x580934d2, 4 | BRF_SND },           // 16
};

STD_ROM_PICK(satansatind)
STD_ROM_FN(satansatind)

struct BurnDriver BurnDrvSatansatind = {
	"satansatind", "satansat", NULL, "vanguard", "1981",
	"Satan of Saturn (Inder S.A., bootleg)\0", NULL, "bootleg (Inder S.A.)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, satansatindRomInfo, satansatindRomName, NULL, NULL, vanguardSampleInfo, vanguardSampleName, SatansatInputInfo, SatansatDIPInfo,
	SatansatindInit, DrvExit, DrvFrame, SatansatDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Vanguard (SNK)

static struct BurnRomInfo vanguardRomDesc[] = {
	{ "sk4_ic07.bin",	0x1000, 0x6a29e354, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 code
	{ "sk4_ic08.bin",	0x1000, 0x302bba54, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sk4_ic09.bin",	0x1000, 0x424755f6, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sk4_ic10.bin",	0x1000, 0x54603274, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "sk4_ic13.bin",	0x1000, 0xfde157d0, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "sk4_ic14.bin",	0x1000, 0x0d5b47d0, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "sk4_ic15.bin",	0x1000, 0x8549b8f8, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "sk4_ic16.bin",	0x1000, 0x062e0be2, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "sk5_ic50.bin",	0x0800, 0xe7d4315b, 2 | BRF_GRA },           //  8 Background tiles
	{ "sk5_ic51.bin",	0x0800, 0x96e87858, 2 | BRF_GRA },           //  9

	{ "sk5_ic7.bin",	0x0020, 0xad782a73, 3 | BRF_GRA },           // 10 Color data
	{ "sk5_ic6.bin",	0x0020, 0x7dc9d450, 3 | BRF_GRA },           // 11

	{ "sk4_ic51.bin",	0x0800, 0xd2a64006, 4 | BRF_SND },           // 12 Custom sound data
	{ "sk4_ic52.bin",	0x0800, 0xcc4a0b6f, 4 | BRF_SND },           // 13

	{ "sk6_ic07.bin",	0x0800, 0x2b7cbae9, 5 | BRF_SND },           // 14 Custom speech data
	{ "sk6_ic08.bin",	0x0800, 0x3b7e9d7c, 5 | BRF_SND },           // 15
	{ "sk6_ic11.bin",	0x0800, 0xc36df041, 5 | BRF_SND },           // 16
};

STD_ROM_PICK(vanguard)
STD_ROM_FN(vanguard)

struct BurnDriver BurnDrvVanguard = {
	"vanguard", NULL, NULL, "vanguard", "1981",
	"Vanguard (SNK)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, vanguardRomInfo, vanguardRomName, NULL, NULL, vanguardSampleInfo, vanguardSampleName, VanguardInputInfo, VanguardDIPInfo,
	VanguardInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Vanguard (Centuri)

static struct BurnRomInfo vanguardcRomDesc[] = {
	{ "sk4_ic07.bin",	0x1000, 0x6a29e354, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 code
	{ "sk4_ic08.bin",	0x1000, 0x302bba54, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sk4_ic09.bin",	0x1000, 0x424755f6, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4",			0x1000, 0x770f9714, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "5",			0x1000, 0x3445cba6, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "sk4_ic14.bin",	0x1000, 0x0d5b47d0, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "sk4_ic15.bin",	0x1000, 0x8549b8f8, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "8",			0x1000, 0x4b825bc8, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "sk5_ic50.bin",	0x0800, 0xe7d4315b, 2 | BRF_GRA },           //  8 Background tiles
	{ "sk5_ic51.bin",	0x0800, 0x96e87858, 2 | BRF_GRA },           //  9

	{ "sk5_ic7.bin",	0x0020, 0xad782a73, 3 | BRF_GRA },           // 10 Color data
	{ "sk5_ic6.bin",	0x0020, 0x7dc9d450, 3 | BRF_GRA },           // 11

	{ "sk4_ic51.bin",	0x0800, 0xd2a64006, 4 | BRF_SND },           // 12 Custom sound data
	{ "sk4_ic52.bin",	0x0800, 0xcc4a0b6f, 4 | BRF_SND },           // 13

	{ "sk6_ic07.bin",	0x0800, 0x2b7cbae9, 5 | BRF_SND },           // 14 Custom speech data
	{ "sk6_ic08.bin",	0x0800, 0x3b7e9d7c, 5 | BRF_SND },           // 15
	{ "sk6_ic11.bin",	0x0800, 0xc36df041, 5 | BRF_SND },           // 16
};

STD_ROM_PICK(vanguardc)
STD_ROM_FN(vanguardc)

struct BurnDriver BurnDrvVanguardc = {
	"vanguardc", "vanguard", NULL, "vanguard", "1981",
	"Vanguard (Centuri)\0", NULL, "SNK (Centuri license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, vanguardcRomInfo, vanguardcRomName, NULL, NULL, vanguardSampleInfo, vanguardSampleName, VanguardInputInfo, VanguardDIPInfo,
	VanguardInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Vanguard (Germany)

static struct BurnRomInfo vanguardgRomDesc[] = {
	{ "vg1.bin",		0x1000, 0x6a29e354, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 code
	{ "vg2.bin",		0x1000, 0x302bba54, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "vg3.bin",		0x1000, 0x424755f6, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "vg4g.bin",		0x1000, 0x4a82306a, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "vg5.bin",		0x1000, 0xfde157d0, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "vg6.bin",		0x1000, 0x0d5b47d0, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "vg7.bin",		0x1000, 0x8549b8f8, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "vg8s.bin",		0x1000, 0xabe5fa3f, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "sk5_ic50.bin",	0x0800, 0xe7d4315b, 2 | BRF_GRA },           //  8 Background tiles
	{ "sk5_ic51.bin",	0x0800, 0x96e87858, 2 | BRF_GRA },           //  9

	{ "sk5_ic7.bin",	0x0020, 0xad782a73, 3 | BRF_GRA },           // 10 Color data
	{ "sk5_ic6.bin",	0x0020, 0x7dc9d450, 3 | BRF_GRA },           // 11

	{ "sk4_ic51.bin",	0x0800, 0xd2a64006, 4 | BRF_SND },           // 12 Custom sound data
	{ "sk4_ic52.bin",	0x0800, 0xcc4a0b6f, 4 | BRF_SND },           // 13

	{ "sk6_ic07.bin",	0x0800, 0x2b7cbae9, 5 | BRF_SND },           // 14 Custom speech data
	{ "sk6_ic08.bin",	0x0800, 0x3b7e9d7c, 5 | BRF_SND },           // 15
	{ "sk6_ic11.bin",	0x0800, 0xc36df041, 5 | BRF_SND },           // 16
};

STD_ROM_PICK(vanguardg)
STD_ROM_FN(vanguardg)

struct BurnDriver BurnDrvVanguardg = {
	"vanguardg", "vanguard", NULL, "vanguard", "1981",
	"Vanguard (Germany)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, vanguardgRomInfo, vanguardgRomName, NULL, NULL, vanguardSampleInfo, vanguardSampleName, VanguardInputInfo, VanguardDIPInfo,
	VanguardInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Vanguard (Japan)

static struct BurnRomInfo vanguardjRomDesc[] = {
	{ "sk4_ic07.bin",	0x1000, 0x6a29e354, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 code
	{ "sk4_ic08.bin",	0x1000, 0x302bba54, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sk4_ic09.bin",	0x1000, 0x424755f6, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "vgj4ic10.bin",	0x1000, 0x0a91a5d1, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "vgj5ic13.bin",	0x1000, 0x06601a40, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "sk4_ic14.bin",	0x1000, 0x0d5b47d0, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "sk4_ic15.bin",	0x1000, 0x8549b8f8, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "sk4_ic16.bin",	0x1000, 0x062e0be2, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "sk5_ic50.bin",	0x0800, 0xe7d4315b, 2 | BRF_GRA },           //  8 Background tiles
	{ "sk5_ic51.bin",	0x0800, 0x96e87858, 2 | BRF_GRA },           //  9

	{ "sk5_ic7.bin",	0x0020, 0xad782a73, 3 | BRF_GRA },           // 10 Color data
	{ "sk5_ic6.bin",	0x0020, 0x7dc9d450, 3 | BRF_GRA },           // 11

	{ "sk4_ic51.bin",	0x0800, 0xd2a64006, 4 | BRF_SND },           // 12 Custom sound data
	{ "sk4_ic52.bin",	0x0800, 0xcc4a0b6f, 4 | BRF_SND },           // 13

	{ "sk6_ic07.bin",	0x0800, 0x2b7cbae9, 5 | BRF_SND },           // 14 Custom speech data
	{ "sk6_ic08.bin",	0x0800, 0x3b7e9d7c, 5 | BRF_SND },           // 15
	{ "sk6_ic11.bin",	0x0800, 0xc36df041, 5 | BRF_SND },           // 16
};

STD_ROM_PICK(vanguardj)
STD_ROM_FN(vanguardj)

struct BurnDriver BurnDrvVanguardj = {
	"vanguardj", "vanguard", NULL, "vanguard", "1981",
	"Vanguard (Japan)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, vanguardjRomInfo, vanguardjRomName, NULL, NULL, vanguardSampleInfo, vanguardSampleName, VanguardInputInfo, VanguardDIPInfo,
	VanguardInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Fantasy (US)

static struct BurnRomInfo fantasyuRomDesc[] = {
	{ "ic12.cpu",		0x1000, 0x22cb2249, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "ic07.cpu",		0x1000, 0x0e2880b6, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ic08.cpu",		0x1000, 0x4c331317, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ic09.cpu",		0x1000, 0x6ac1dbfc, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ic10.cpu",		0x1000, 0xc796a406, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "ic14.cpu",		0x1000, 0x6f1f0698, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "ic15.cpu",		0x1000, 0x5534d57e, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "ic16.cpu",		0x1000, 0x6c2aeb6e, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "ic17.cpu",		0x1000, 0xf6aa5de1, 1 | BRF_PRG | BRF_ESS }, //  8

	{ "fs10ic50.bin",	0x1000, 0x86a801c3, 2 | BRF_GRA },           //  9 Background tiles
	{ "fs11ic51.bin",	0x1000, 0x9dfff71c, 2 | BRF_GRA },           // 10

	{ "fantasy.ic7",	0x0020, 0x361a5e99, 3 | BRF_GRA },           // 11 Color data
	{ "fantasy.ic6",	0x0020, 0x33d974f7, 3 | BRF_GRA },           // 12

	{ "fs_b_51.bin",	0x0800, 0x48094ec5, 4 | BRF_SND },           // 13 Custom sound data
	{ "fs_a_52.bin",	0x0800, 0x1d0316e8, 4 | BRF_SND },           // 14
	{ "fs_c_53.bin",	0x0800, 0x49fd4ae8, 4 | BRF_SND },           // 15

	{ "fs_d_7.bin",		0x0800, 0xa7ef4cc6, 5 | BRF_SND },           // 16 Custom speech data
	{ "fs_e_8.bin",		0x0800, 0x19b8fb3e, 5 | BRF_SND },           // 17
	{ "fs_f_11.bin",	0x0800, 0x3a352e1f, 5 | BRF_SND },           // 18
};

STD_ROM_PICK(fantasyu)
STD_ROM_FN(fantasyu)

struct BurnDriver BurnDrvFantasyu = {
	"fantasyu", NULL, NULL, "fantasy", "1981",
	"Fantasy (US)\0", NULL, "SNK (Rock-Ola license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, fantasyuRomInfo, fantasyuRomName, NULL, NULL, fantasySampleInfo, fantasySampleName, FantasyInputInfo, FantasyuDIPInfo,
	FantasyuInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Fantasy (Germany, set 1)

static struct BurnRomInfo fantasygRomDesc[] = {
	{ "5.12",		0x1000, 0x0968ab50, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "1.7",		0x1000, 0xde83000e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "2.8",		0x1000, 0x90499b5a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "3.9",		0x1000, 0x6fbffeb6, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "4.10",		0x1000, 0x02e85884, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "ic14.cpu",		0x1000, 0x6f1f0698, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "ic15.cpu",		0x1000, 0x5534d57e, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "8.16",		0x1000, 0x371129fe, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "9.17",		0x1000, 0x56a7c8b8, 1 | BRF_PRG | BRF_ESS }, //  8

	{ "fs10ic50.bin",	0x1000, 0x86a801c3, 2 | BRF_GRA },           //  9 Background tiles
	{ "fs11ic51.bin",	0x1000, 0x9dfff71c, 2 | BRF_GRA },           // 10

	{ "fantasy.ic7",	0x0020, 0x361a5e99, 3 | BRF_GRA },           // 11 Color data
	{ "fantasy.ic6",	0x0020, 0x33d974f7, 3 | BRF_GRA },           // 12

	{ "fs_b_51.bin",	0x0800, 0x48094ec5, 4 | BRF_SND },           // 13 Custom sound data
	{ "fs_a_52.bin",	0x0800, 0x1d0316e8, 4 | BRF_SND },           // 14
	{ "fs_c_53.bin",	0x0800, 0x49fd4ae8, 4 | BRF_SND },           // 15

	{ "fs_d_7.bin",		0x0800, 0xa7ef4cc6, 5 | BRF_SND },           // 16 Custom speech data
	{ "fs_e_8.bin",		0x0800, 0x19b8fb3e, 5 | BRF_SND },           // 17
	{ "fs_f_11.bin",	0x0800, 0x3a352e1f, 5 | BRF_SND },           // 18
};

STD_ROM_PICK(fantasyg)
STD_ROM_FN(fantasyg)

struct BurnDriver BurnDrvFantasyg = {
	"fantasyg", "fantasyu", NULL, "fantasy", "1981",
	"Fantasy (Germany, set 1)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, fantasygRomInfo, fantasygRomName, NULL, NULL, fantasySampleInfo, fantasySampleName, FantasyInputInfo, FantasyuDIPInfo,
	FantasyuInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Fantasy (Germany, set 2)

static struct BurnRomInfo fantasyg2RomDesc[] = {
	{ "ts5.bin",		0x1000, 0x6edca14e, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "fs1.bin",		0x1000, 0xd99656e8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ts2.bin",		0x1000, 0x2db6ce28, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ts3.bin",		0x1000, 0x1a0aa7c5, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "fs4.bin",		0x1000, 0xc02ad442, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "fs6.bin",		0x1000, 0xe5b91bc2, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "fs7.bin",		0x1000, 0xcc18428e, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "fs8.bin",		0x1000, 0x371129fe, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "fs9.bin",		0x1000, 0x49574d4a, 1 | BRF_PRG | BRF_ESS }, //  8

	{ "fs10.bin",		0x1000, 0x86a801c3, 2 | BRF_GRA },           //  9 Background tiles
	{ "fs11.bin",		0x1000, 0x9dfff71c, 2 | BRF_GRA },           // 10

	{ "fantasy.ic7",	0x0020, 0x361a5e99, 3 | BRF_GRA },           // 11 Color data
	{ "fantasy.ic6",	0x0020, 0x33d974f7, 3 | BRF_GRA },           // 12

	{ "fs_b_51.bin",	0x0800, 0x48094ec5, 4 | BRF_SND },           // 13 Custom sound data
	{ "fs_a_52.bin",	0x0800, 0x1d0316e8, 4 | BRF_SND },           // 14
	{ "fs_c_53.bin",	0x0800, 0x49fd4ae8, 4 | BRF_SND },           // 15

	{ "fs_d_7.bin",		0x0800, 0xa7ef4cc6, 5 | BRF_SND },           // 16 Custom speech data
	{ "fs_e_8.bin",		0x0800, 0x19b8fb3e, 5 | BRF_SND },           // 17
	{ "fs_f_11.bin",	0x0800, 0x3a352e1f, 5 | BRF_SND },           // 18
};

STD_ROM_PICK(fantasyg2)
STD_ROM_FN(fantasyg2)

struct BurnDriver BurnDrvFantasyg2 = {
	"fantasyg2", "fantasyu", NULL, "fantasy", "1981",
	"Fantasy (Germany, set 2)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, fantasyg2RomInfo, fantasyg2RomName, NULL, NULL, fantasySampleInfo, fantasySampleName, FantasyInputInfo, FantasyDIPInfo,
	FantasyuInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Fantasy (Japan)

static struct BurnRomInfo fantasyjRomDesc[] = {
	{ "fs5jic12.bin",	0x1000, 0xdd1eac89, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "fs1jic7.bin",	0x1000, 0x7b8115ae, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "fs2jic8.bin",	0x1000, 0x61531dd1, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "fs3jic9.bin",	0x1000, 0x36a12617, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "fs4jic10.bin",	0x1000, 0xdbf7c347, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "fs6jic14.bin",	0x1000, 0xbf59a33a, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "fs7jic15.bin",	0x1000, 0xcc18428e, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "fs8jic16.bin",	0x1000, 0xae5bf727, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "fs9jic17.bin",	0x1000, 0xfa6903e2, 1 | BRF_PRG | BRF_ESS }, //  8

	{ "fs10ic50.bin",	0x1000, 0x86a801c3, 2 | BRF_GRA },           //  9 gfx1
	{ "fs11ic51.bin",	0x1000, 0x9dfff71c, 2 | BRF_GRA },           // 10

	{ "prom-8.bpr",		0x0020, 0x1aa9285a, 3 | BRF_GRA },           // 11 proms
	{ "prom-7.bpr",		0x0020, 0x7a6f7dc3, 3 | BRF_GRA },           // 12

	{ "fs_b_51.bin",	0x0800, 0x48094ec5, 4 | BRF_SND },           // 13 snk6502
	{ "fs_a_52.bin",	0x0800, 0x1d0316e8, 4 | BRF_SND },           // 14
	{ "fs_c_53.bin",	0x0800, 0x49fd4ae8, 4 | BRF_SND },           // 15

	{ "fs_d_7.bin",		0x0800, 0xa7ef4cc6, 5 | BRF_SND },           // 16 speech
	{ "fs_e_8.bin",		0x0800, 0x19b8fb3e, 5 | BRF_SND },           // 17
	{ "fs_f_11.bin",	0x0800, 0x3a352e1f, 5 | BRF_SND },           // 18
};

STD_ROM_PICK(fantasyj)
STD_ROM_FN(fantasyj)

struct BurnDriver BurnDrvFantasyj = {
	"fantasyj", "fantasyu", NULL, "fantasy", "1981",
	"Fantasy (Japan)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, fantasyjRomInfo, fantasyjRomName, NULL, NULL, fantasySampleInfo, fantasySampleName, FantasyInputInfo, FantasyuDIPInfo,
	FantasyuInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Nibbler (rev 9)

static struct BurnRomInfo nibblerRomDesc[] = {
	{ "g-0960-52.ic12",	0x1000, 0x6dfa1be5, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "g-0960-48.ic7",	0x1000, 0x808e1a03, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "g-0960-49.ic8",	0x1000, 0x1571d4a2, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "g-0960-50.ic9",	0x1000, 0xa599df10, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "g-0960-51.ic10",	0x1000, 0xa6b5abe5, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "g-0960-53.ic14",	0x1000, 0x9f537185, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "g-0960-54.ic15",	0x1000, 0x7205fb8d, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "g-0960-55.ic16",	0x1000, 0x4bb39815, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "g-0960-56.ic17",	0x1000, 0xed680f19, 1 | BRF_PRG | BRF_ESS }, //  8

	{ "g-0960-57.ic50",	0x1000, 0x01d4d0c2, 2 | BRF_GRA },           //  9 Background Tiles
	{ "g-0960-58.ic51",	0x1000, 0xfeff7faf, 2 | BRF_GRA },           // 10

	{ "g-0708-05.ic7",	0x0020, 0xa5709ff3, 3 | BRF_GRA },           // 11 Color data
	{ "g-0708-04.ic6",	0x0020, 0xdacd592d, 3 | BRF_GRA },           // 12

	{ "g-0959-44.ic52",	0x0800, 0x87d67dee, 4 | BRF_SND },           // 13 Custom sound data
	{ "g-0959-45.ic53",	0x0800, 0x33189917, 4 | BRF_SND },           // 14
};

STD_ROM_PICK(nibbler)
STD_ROM_FN(nibbler)

struct BurnDriver BurnDrvNibbler = {
	"nibbler", NULL, NULL, NULL, "1982",
	"Nibbler (rev 9, set 1)\0", NULL, "Rock-Ola", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_ACTION, 0,
	NULL, nibblerRomInfo, nibblerRomName, NULL, NULL, NULL, NULL, NibblerInputInfo, NibblerDIPInfo,
	NibblerInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Nibbler (rev 8)

static struct BurnRomInfo nibbler8RomDesc[] = {
	{ "50-144.012",		0x1000, 0x68af8f4b, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "50-140.007",		0x1000, 0xc18b3009, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "50-141.008",		0x1000, 0xb50fd79c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "g-0960-50.ic9",	0x1000, 0xa599df10, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "g-0960-51.ic10",	0x1000, 0xa6b5abe5, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "50-145.014",		0x1000, 0x29ea246a, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "g-0960-54.ic15",	0x1000, 0x7205fb8d, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "g-0960-55.ic16",	0x1000, 0x4bb39815, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "g-0960-56.ic17",	0x1000, 0xed680f19, 1 | BRF_PRG | BRF_ESS }, //  8

	{ "g-0960-57.ic50",	0x1000, 0x01d4d0c2, 2 | BRF_GRA },           //  9 Background tiles
	{ "g-0960-58.ic51",	0x1000, 0xfeff7faf, 2 | BRF_GRA },           // 10

	{ "g-0708-05.ic7",	0x0020, 0xa5709ff3, 3 | BRF_GRA },           // 11 Color data
	{ "g-0708-04.ic6",	0x0020, 0xdacd592d, 3 | BRF_GRA },           // 12

	{ "g-0959-44.ic52",	0x0800, 0x87d67dee, 4 | BRF_SND },           // 13 Custom sound data
	{ "g-0959-45.ic53",	0x0800, 0x33189917, 4 | BRF_SND },           // 14
};

STD_ROM_PICK(nibbler8)
STD_ROM_FN(nibbler8)

struct BurnDriver BurnDrvNibbler8 = {
	"nibbler8", "nibbler", NULL, NULL, "1982",
	"Nibbler (rev 8)\0", NULL, "Rock-Ola", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_ACTION, 0,
	NULL, nibbler8RomInfo, nibbler8RomName, NULL, NULL, NULL, NULL, NibblerInputInfo, Nibbler8DIPInfo,
	NibblerInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Nibbler (rev 7)

static struct BurnRomInfo nibbler7RomDesc[] = {
	{ "ic12",		0x1000, 0x8685d060, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "ic7",		0x1000, 0xb07195c7, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ic8",		0x1000, 0x61034cca, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "g-0960-50.ic9",	0x1000, 0xa599df10, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "g-0960-51.ic10",	0x1000, 0xa6b5abe5, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "ic14",		0x1000, 0x7a87c766, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "g-0960-54.ic15",	0x1000, 0x7205fb8d, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "g-0960-55.ic16",	0x1000, 0x4bb39815, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "g-0960-56.ic17",	0x1000, 0xed680f19, 1 | BRF_PRG | BRF_ESS }, //  8

	{ "g-0960-57.ic50",	0x1000, 0x01d4d0c2, 2 | BRF_GRA },           //  9 Background tiles
	{ "g-0960-58.ic51",	0x1000, 0xfeff7faf, 2 | BRF_GRA },           // 10

	{ "g-0708-05.ic7",	0x0020, 0xa5709ff3, 3 | BRF_GRA },           // 11 Color data
	{ "g-0708-04.ic6",	0x0020, 0xdacd592d, 3 | BRF_GRA },           // 12

	{ "g-0959-44.ic52",	0x0800, 0x87d67dee, 4 | BRF_SND },           // 13 Custom sound data
	{ "g-0959-45.ic53",	0x0800, 0x33189917, 4 | BRF_SND },           // 14
};

STD_ROM_PICK(nibbler7)
STD_ROM_FN(nibbler7)

struct BurnDriver BurnDrvNibbler7 = {
	"nibbler7", "nibbler", NULL, NULL, "1982",
	"Nibbler (rev 7)\0", NULL, "Rock-Ola", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_ACTION, 0,
	NULL, nibbler7RomInfo, nibbler7RomName, NULL, NULL, NULL, NULL, NibblerInputInfo, Nibbler8DIPInfo,
	NibblerInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Nibbler (rev 6)

static struct BurnRomInfo nibbler6RomDesc[] = {
	{ "ic12",		0x1000, 0xac6a802b, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "ic7",		0x1000, 0x35971364, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ic8",		0x1000, 0x6b33b806, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ic9",		0x1000, 0x91a4f98d, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ic10",		0x1000, 0xa151d934, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "ic14",		0x1000, 0x063f05cc, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "g-0960-54.ic15",	0x1000, 0x7205fb8d, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "g-0960-55.ic16",	0x1000, 0x4bb39815, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "g-0960-56.ic17",	0x1000, 0xed680f19, 1 | BRF_PRG | BRF_ESS }, //  8

	{ "g-0960-57.ic50",	0x1000, 0x01d4d0c2, 2 | BRF_GRA },           //  9 Background tiles
	{ "g-0960-58.ic51",	0x1000, 0xfeff7faf, 2 | BRF_GRA },           // 10

	{ "g-0708-05.ic7",	0x0020, 0xa5709ff3, 3 | BRF_GRA },           // 11 Color data
	{ "g-0708-04.ic6",	0x0020, 0xdacd592d, 3 | BRF_GRA },           // 12

	{ "g-0959-44.ic52",	0x0800, 0x87d67dee, 4 | BRF_SND },           // 13 Custom sound data
	{ "g-0959-45.ic53",	0x0800, 0x33189917, 4 | BRF_SND },           // 14
};

STD_ROM_PICK(nibbler6)
STD_ROM_FN(nibbler6)

struct BurnDriver BurnDrvNibbler6 = {
	"nibbler6", "nibbler", NULL, NULL, "1982",
	"Nibbler (rev 6)\0", NULL, "Rock-Ola", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_ACTION, 0,
	NULL, nibbler6RomInfo, nibbler6RomName, NULL, NULL, NULL, NULL, NibblerInputInfo, Nibbler6DIPInfo,
	NibblerInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Nibbler (rev 9, alternate set)

static struct BurnRomInfo nibbleraRomDesc[] = {
	{ "2732.ic12",		0x1000, 0xe569937b, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "2732.ic07",		0x1000, 0x7f9d715c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "2732.ic08",		0x1000, 0xe46eb1c9, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "2732.ic09",		0x1000, 0xa599df10, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "2732.ic10",		0x1000, 0x746e94cd, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "2732.ic14",		0x1000, 0x48ec4af0, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "2732.ic15",		0x1000, 0x7205fb8d, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "2732.ic16",		0x1000, 0x4bb39815, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "2732.ic17",		0x1000, 0xed680f19, 1 | BRF_PRG | BRF_ESS }, //  8

	{ "2732.ic50",		0x1000, 0x01d4d0c2, 2 | BRF_GRA },           //  9 Background tiles
	{ "2732.ic51",		0x1000, 0xfeff7faf, 2 | BRF_GRA },           // 10

	{ "g-0708-05.ic7",	0x0020, 0xa5709ff3, 3 | BRF_GRA },           // 11 Color data
	{ "g-0708-04.ic6",	0x0020, 0xdacd592d, 3 | BRF_GRA },           // 12

	{ "2716.ic52",		0x0800, 0xcabe6c34, 4 | BRF_SND },           // 13 Custom sound data
	{ "2716.ic53",		0x0800, 0x33189917, 4 | BRF_SND },           // 14
};

STD_ROM_PICK(nibblera)
STD_ROM_FN(nibblera)

struct BurnDriver BurnDrvNibblera = {
	"nibblera", "nibbler", NULL, NULL, "1982",
	"Nibbler (rev 9, set 2)\0", NULL, "Rock-Ola", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_ACTION, 0,
	NULL, nibbleraRomInfo, nibbleraRomName, NULL, NULL, NULL, NULL, NibblerInputInfo, NibblerDIPInfo,
	NibblerInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Nibbler (Pioneer Balloon conversion - rev 6)

static struct BurnRomInfo nibblerpRomDesc[] = {
	{ "ic12",		0x1000, 0xac6a802b, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "ic7",		0x1000, 0x35971364, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ic8",		0x1000, 0x6b33b806, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ic9",		0x1000, 0x91a4f98d, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ic10",		0x1000, 0xa151d934, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "ic14",		0x1000, 0x063f05cc, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "g-0960-54.ic15",	0x1000, 0x7205fb8d, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "g-0960-55.ic16",	0x1000, 0x4bb39815, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "g-0960-56.ic17",	0x1000, 0xed680f19, 1 | BRF_PRG | BRF_ESS }, //  8

	{ "g-0960-57.ic50",	0x1000, 0x01d4d0c2, 2 | BRF_GRA },           //  9 Background tiles
	{ "g-0960-58.ic51",	0x1000, 0xfeff7faf, 2 | BRF_GRA },           // 10

	{ "g-0708-05.ic7",	0x0020, 0xa5709ff3, 3 | BRF_GRA },           // 11 Color data
	{ "g-0708-04.ic6",	0x0020, 0xdacd592d, 3 | BRF_GRA },           // 12

	{ "sk7_ic51.bin",	0x0800, 0x0345f8b7, 4 | BRF_SND },           // 13 Custom sound data
	{ "g-0959-44.ic52",	0x0800, 0x87d67dee, 4 | BRF_SND },           // 14
	{ "g-0959-45.ic53",	0x0800, 0x33189917, 4 | BRF_SND },           // 15
};

STD_ROM_PICK(nibblerp)
STD_ROM_FN(nibblerp)

struct BurnDriver BurnDrvNibblerp = {
	"nibblerp", "nibbler", NULL, NULL, "1982",
	"Nibbler (rev 6, Pioneer Balloon conversion)\0", NULL, "Rock-Ola", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_ACTION, 0,
	NULL, nibblerpRomInfo, nibblerpRomName, NULL, NULL, NULL, NULL, NibblerInputInfo, Nibbler6DIPInfo,
	NibblerpInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Nibbler (Olympia - rev 8)

static struct BurnRomInfo nibbleroRomDesc[] = {
	{ "50-144g.012",	0x1000, 0x1093f525, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "50-140g.007",	0x1000, 0x848651dd, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "50-141.008",		0x1000, 0xb50fd79c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "nibblero.ic9",	0x1000, 0xa599df10, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "nibblero.ic10",	0x1000, 0xa6b5abe5, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "50-145.014",		0x1000, 0x29ea246a, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "g-0960-54.ic15",	0x1000, 0x7205fb8d, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "g-0960-55.ic16",	0x1000, 0x4bb39815, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "g-0960-56.ic17",	0x1000, 0xed680f19, 1 | BRF_PRG | BRF_ESS }, //  8

	{ "g-0960-57.ic50",	0x1000, 0x01d4d0c2, 2 | BRF_GRA },           //  9 Background tiles
	{ "g-0960-58.ic51",	0x1000, 0xfeff7faf, 2 | BRF_GRA },           // 10

	{ "g-0708-05.ic7",	0x0020, 0xa5709ff3, 3 | BRF_GRA },           // 11 Color data
	{ "g-0708-04.ic6",	0x0020, 0xdacd592d, 3 | BRF_GRA },           // 12

	{ "g-0959-44.ic52",	0x0800, 0x87d67dee, 4 | BRF_SND },           // 13 Custom sound data
	{ "g-0959-45.ic53",	0x0800, 0x33189917, 4 | BRF_SND },           // 14
};

STD_ROM_PICK(nibblero)
STD_ROM_FN(nibblero)

struct BurnDriver BurnDrvNibblero = {
	"nibblero", "nibbler", NULL, NULL, "1983",
	"Nibbler (rev 8, Olympia)\0", NULL, "Rock-Ola (Olympia license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_ACTION, 0,
	NULL, nibbleroRomInfo, nibbleroRomName, NULL, NULL, NULL, NULL, NibblerInputInfo, Nibbler8DIPInfo,
	NibblerInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Pioneer Balloon

static struct BurnRomInfo pballoonRomDesc[] = {
	{ "sk7_ic12.bin",	0x1000, 0xdfe2ae05, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "sk7_ic07.bin",	0x1000, 0x736e67df, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sk7_ic08.bin",	0x1000, 0x7a2032b2, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sk7_ic09.bin",	0x1000, 0x2d63cf3a, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "sk7_ic10.bin",	0x1000, 0x7b88cbd4, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "sk7_ic14.bin",	0x1000, 0x6a8817a5, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "sk7_ic15.bin",	0x1000, 0x1f78d814, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "sk8_ic50.bin",	0x1000, 0x560df07f, 2 | BRF_GRA },           //  7 Background Tiles
	{ "sk8_ic51.bin",	0x1000, 0xd415de51, 2 | BRF_GRA },           //  8

	{ "sk8_ic7.bin",	0x0020, 0xef6c82a0, 3 | BRF_GRA },           //  9 Color data
	{ "sk8_ic6.bin",	0x0020, 0xeabc6a00, 3 | BRF_GRA },           // 10

	{ "sk7_ic51.bin",	0x0800, 0x0345f8b7, 4 | BRF_SND },           // 11 Custom sound data
	{ "sk7_ic52.bin",	0x0800, 0x5d6d68ea, 4 | BRF_SND },           // 12
	{ "sk7_ic53.bin",	0x0800, 0xa4c505cd, 4 | BRF_SND },           // 13
};

STD_ROM_PICK(pballoon)
STD_ROM_FN(pballoon)

struct BurnDriver BurnDrvPballoon = {
	"pballoon", NULL, NULL, NULL, "1982",
	"Pioneer Balloon\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, pballoonRomInfo, pballoonRomName, NULL, NULL, NULL, NULL, PballoonInputInfo, PballoonDIPInfo,
	PballoonInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Pioneer Balloon (Rock-Ola license)

static struct BurnRomInfo pballoonrRomDesc[] = {
	{ "sk7_ic12.bin",	0x1000, 0xdfe2ae05, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "rock-ola_skpb1.ic7",	0x1000, 0xdfd802e8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rock-ola_skpb1.ic8",	0x1000, 0xc433c062, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rock-ola_skpb1.ic9",	0x1000, 0xf85b9c37, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "rock-ola_skpb1.ic10",0x1000, 0x8020e52d, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "sk7_ic14.bin",	0x1000, 0x6a8817a5, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "sk7_ic15.bin",	0x1000, 0x1f78d814, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "sk8_ic50.bin",	0x1000, 0x560df07f, 2 | BRF_GRA },           //  7 Background Tiles
	{ "sk8_ic51.bin",	0x1000, 0xd415de51, 2 | BRF_GRA },           //  8

	{ "sk8_ic7.bin",	0x0020, 0xef6c82a0, 3 | BRF_GRA },           //  9 Color data
	{ "sk8_ic6.bin",	0x0020, 0xeabc6a00, 3 | BRF_GRA },           // 10

	{ "sk7_ic51.bin",	0x0800, 0x0345f8b7, 4 | BRF_SND },           // 11 Custom sound data
	{ "sk7_ic52.bin",	0x0800, 0x5d6d68ea, 4 | BRF_SND },           // 12
	{ "sk7_ic53.bin",	0x0800, 0xa4c505cd, 4 | BRF_SND },           // 13
};

STD_ROM_PICK(pballoonr)
STD_ROM_FN(pballoonr)

struct BurnDriver BurnDrvPballoonr = {
	"pballoonr", "pballoon", NULL, NULL, "1982",
	"Pioneer Balloon (Rock-Ola license)\0", NULL, "SNK (Rock-Ola license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, pballoonrRomInfo, pballoonrRomName, NULL, NULL, NULL, NULL, PballoonInputInfo, PballoonDIPInfo,
	PballoonInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};
