// FB Alpha Phoenix driver module
// Based on MAME driver by Richard Davies

#include "tiles_generic.h"
#define USE_Z80

#ifdef USE_Z80

#include "z80_intf.h"

#define i8085Init		ZetInit
#define i8080Reset		ZetReset
#define i8080Open		ZetOpen
#define i8080Close		ZetClose
#define i8080Exit		ZetExit
#define i8080SetWriteHandler	ZetSetWriteHandler
#define i8080SetReadHandler	ZetSetReadHandler
#define i8080MapMemory		ZetMapMemory
#define i8080Run		ZetRun
#define i8080Scan		ZetScan

#else

#include "i8080.h"

#endif

#include "phoenixsound.h"
#include "pleiadssound.h"

static UINT8 *DrvI8085ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *DrvI8085RAM;
static UINT8 *RamEnd;
static UINT8 *MemEnd;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 ram_bank;
static UINT8 scrollx;
static UINT8 palette_bank;
static UINT8 pleiads_protection_question;
static INT32 cocktail_mode = 0;

static INT32 vblank;

static INT32 pleiads = 0;
static INT32 condor = 0;
static INT32 phoenixmode = 0;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvInputs[3];
static UINT8 DrvDips[3];
static UINT8 DrvReset;

static struct BurnInputInfo PhoenixInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 start"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 fire 2"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 2,	"p2 start"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Phoenix)

static struct BurnInputInfo Phoenix3InputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 start"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 2,	"p2 start"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Phoenix3)

static struct BurnInputInfo CondorInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 start"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 2,	"p2 start"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Condor)

static struct BurnInputInfo PleiadsInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 start"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 1"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 2,	"p2 start"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 right"	},
	{"P2 Button",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Pleiads)

static struct BurnDIPInfo PleiadsDIPList[]=
{
	{0x0a, 0xff, 0xff, 0x60, NULL			},
	{0x0b, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0a, 0x01, 0x03, 0x00, "3"			},
	{0x0a, 0x01, 0x03, 0x01, "4"			},
	{0x0a, 0x01, 0x03, 0x02, "5"			},
	{0x0a, 0x01, 0x03, 0x03, "6"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x0a, 0x01, 0x0c, 0x00, "3K 30K"		},
	{0x0a, 0x01, 0x0c, 0x04, "4K 40K"		},
	{0x0a, 0x01, 0x0c, 0x08, "5K 50K"		},
	{0x0a, 0x01, 0x0c, 0x0c, "6K 60K"		},

	{0   , 0xfe, 0   ,    2, "Coinage"		},
	{0x0a, 0x01, 0x10, 0x10, "2 Coins 1 Credits"	},
	{0x0a, 0x01, 0x10, 0x00, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x0a, 0x01, 0x40, 0x00, "Off"			},
	{0x0a, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0b, 0x01, 0x01, 0x00, "Upright"		},
	{0x0b, 0x01, 0x01, 0x01, "Cocktail"		},
};

STDDIPINFO(Pleiads)

static struct BurnDIPInfo PleiadblDIPList[]=
{
	{0x0c, 0xff, 0xff, 0x60, NULL			},
	{0x0e, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0c, 0x01, 0x03, 0x00, "3"			},
	{0x0c, 0x01, 0x03, 0x01, "4"			},
	{0x0c, 0x01, 0x03, 0x02, "5"			},
	{0x0c, 0x01, 0x03, 0x03, "6"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x0c, 0x01, 0x0c, 0x00, "3K 30K"		},
	{0x0c, 0x01, 0x0c, 0x04, "4K 40K"		},
	{0x0c, 0x01, 0x0c, 0x08, "5K 50K"		},
	{0x0c, 0x01, 0x0c, 0x0c, "6K 60K"		},

	{0   , 0xfe, 0   ,    2, "Coinage"		},
	{0x0c, 0x01, 0x10, 0x10, "2 Coins 1 Credits"	},
	{0x0c, 0x01, 0x10, 0x00, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x0c, 0x01, 0x40, 0x00, "Off"			},
	{0x0c, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0e, 0x01, 0x01, 0x00, "Upright"		},
	{0x0e, 0x01, 0x01, 0x01, "Cocktail"		},
};

STDDIPINFO(Pleiadbl)

static struct BurnDIPInfo PleiadceDIPList[]=
{
	{0x0c, 0xff, 0xff, 0x60, NULL			},
	{0x0e, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0c, 0x01, 0x03, 0x00, "3"			},
	{0x0c, 0x01, 0x03, 0x01, "4"			},
	{0x0c, 0x01, 0x03, 0x02, "5"			},
	{0x0c, 0x01, 0x03, 0x03, "6"			},

	{0   , 0xfe, 0   ,    3, "Bonus Life"		},
	{0x0c, 0x01, 0x0c, 0x00, "7K 70K"		},
	{0x0c, 0x01, 0x0c, 0x04, "8K 80K"		},
	{0x0c, 0x01, 0x0c, 0x08, "9K 90K"		},

	{0   , 0xfe, 0   ,    2, "Coinage"		},
	{0x0c, 0x01, 0x10, 0x10, "2 Coins 1 Credits"	},
	{0x0c, 0x01, 0x10, 0x00, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x0c, 0x01, 0x40, 0x00, "Off"			},
	{0x0c, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0e, 0x01, 0x01, 0x00, "Upright"		},
	{0x0e, 0x01, 0x01, 0x01, "Cocktail"		},
};

STDDIPINFO(Pleiadce)

static struct BurnDIPInfo CondorDIPList[]=
{
	{0x0d, 0xff, 0xff, 0x30, NULL			},
	{0x0e, 0xff, 0xff, 0x00, NULL			},
	{0x0f, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0d, 0x01, 0x03, 0x00, "2"			},
	{0x0d, 0x01, 0x03, 0x01, "3"			},
	{0x0d, 0x01, 0x03, 0x02, "4"			},
	{0x0d, 0x01, 0x03, 0x03, "5"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x0d, 0x01, 0x0c, 0x00, "Every 6000"		},
	{0x0d, 0x01, 0x0c, 0x08, "Every 10000"		},
	{0x0d, 0x01, 0x0c, 0x04, "Every 14000"		},
	{0x0d, 0x01, 0x0c, 0x0c, "Every 18000"		},

	{0   , 0xfe, 0   ,    8, "Fuel Consumption"	},
	{0x0d, 0x01, 0x70, 0x00, "Slowest"		},
	{0x0d, 0x01, 0x70, 0x10, "Slower"		},
	{0x0d, 0x01, 0x70, 0x20, "Slow"			},
	{0x0d, 0x01, 0x70, 0x30, "Bit Slow"		},
	{0x0d, 0x01, 0x70, 0x40, "Bit Fast"		},
	{0x0d, 0x01, 0x70, 0x50, "Fast"			},
	{0x0d, 0x01, 0x70, 0x60, "Faster"		},
	{0x0d, 0x01, 0x70, 0x70, "Fastest"		},

	{0   , 0xfe, 0   ,    16, "Coin B"		},
	{0x0e, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"	},
	{0x0e, 0x01, 0x0f, 0x09, "2 Coins 2 Credits"	},
	{0x0e, 0x01, 0x0f, 0x00, "1 Coin  1 Credits"	},
	{0x0e, 0x01, 0x0f, 0x0a, "2 Coins 3 Credits"	},
	{0x0e, 0x01, 0x0f, 0x0b, "2 Coins 4 Credits"	},
	{0x0e, 0x01, 0x0f, 0x01, "1 Coin  2 Credits"	},
	{0x0e, 0x01, 0x0f, 0x0c, "2 Coins 5 Credits"	},
	{0x0e, 0x01, 0x0f, 0x0d, "2 Coins 6 Credits"	},
	{0x0e, 0x01, 0x0f, 0x02, "1 Coin  3 Credits"	},
	{0x0e, 0x01, 0x0f, 0x0e, "2 Coins 7 Credits"	},
	{0x0e, 0x01, 0x0f, 0x0f, "2 Coins 8 Credits"	},
	{0x0e, 0x01, 0x0f, 0x03, "1 Coin  4 Credits"	},
	{0x0e, 0x01, 0x0f, 0x04, "1 Coin  5 Credits"	},
	{0x0e, 0x01, 0x0f, 0x05, "1 Coin  6 Credits"	},
	{0x0e, 0x01, 0x0f, 0x06, "1 Coin  7 Credits"	},
	{0x0e, 0x01, 0x0f, 0x07, "1 Coin  8 Credits"	},

	{0   , 0xfe, 0   ,    16, "Coin A"		},
	{0x0e, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"	},
	{0x0e, 0x01, 0xf0, 0x90, "2 Coins 2 Credits"	},
	{0x0e, 0x01, 0xf0, 0x00, "1 Coin  1 Credits"	},
	{0x0e, 0x01, 0xf0, 0xa0, "2 Coins 3 Credits"	},
	{0x0e, 0x01, 0xf0, 0xb0, "2 Coins 4 Credits"	},
	{0x0e, 0x01, 0xf0, 0x10, "1 Coin  2 Credits"	},
	{0x0e, 0x01, 0xf0, 0xc0, "2 Coins 5 Credits"	},
	{0x0e, 0x01, 0xf0, 0xd0, "2 Coins 6 Credits"	},
	{0x0e, 0x01, 0xf0, 0x20, "1 Coin  3 Credits"	},
	{0x0e, 0x01, 0xf0, 0xe0, "2 Coins 7 Credits"	},
	{0x0e, 0x01, 0xf0, 0xf0, "2 Coins 8 Credits"	},
	{0x0e, 0x01, 0xf0, 0x30, "1 Coin  4 Credits"	},
	{0x0e, 0x01, 0xf0, 0x40, "1 Coin  5 Credits"	},
	{0x0e, 0x01, 0xf0, 0x50, "1 Coin  6 Credits"	},
	{0x0e, 0x01, 0xf0, 0x60, "1 Coin  7 Credits"	},
	{0x0e, 0x01, 0xf0, 0x70, "1 Coin  8 Credits"	},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0f, 0x01, 0x01, 0x00, "Upright"		},
	{0x0f, 0x01, 0x01, 0x01, "Cocktail"		},
};

STDDIPINFO(Condor)

static struct BurnDIPInfo PhoenixDIPList[]=
{
	{0x0c, 0xff, 0xff, 0x60, NULL			},
	{0x0e, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0c, 0x01, 0x03, 0x00, "3"			},
	{0x0c, 0x01, 0x03, 0x01, "4"			},
	{0x0c, 0x01, 0x03, 0x02, "5"			},
	{0x0c, 0x01, 0x03, 0x03, "6"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x0c, 0x01, 0x0c, 0x00, "3K 30K"		},
	{0x0c, 0x01, 0x0c, 0x04, "4K 40K"		},
	{0x0c, 0x01, 0x0c, 0x08, "5K 50K"		},
	{0x0c, 0x01, 0x0c, 0x0c, "6K 60K"		},

	{0   , 0xfe, 0   ,    2, "Coinage"		},
	{0x0c, 0x01, 0x10, 0x10, "2 Coins 1 Credits"	},
	{0x0c, 0x01, 0x10, 0x00, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0e, 0x01, 0x01, 0x00, "Upright"		},
	{0x0e, 0x01, 0x01, 0x01, "Cocktail"		},
};

STDDIPINFO(Phoenix)

static struct BurnDIPInfo PhoenixaDIPList[]=
{
	{0x0c, 0xff, 0xff, 0x60, NULL			},
	{0x0e, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0c, 0x01, 0x03, 0x00, "3"			},
	{0x0c, 0x01, 0x03, 0x01, "4"			},
	{0x0c, 0x01, 0x03, 0x02, "5"			},
	{0x0c, 0x01, 0x03, 0x03, "6"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x0c, 0x01, 0x0c, 0x00, "3K 30K"		},
	{0x0c, 0x01, 0x0c, 0x04, "4K 40K"		},
	{0x0c, 0x01, 0x0c, 0x08, "5K 50K"		},
	{0x0c, 0x01, 0x0c, 0x0c, "6K 60K"		},

	{0   , 0xfe, 0   ,    2, "Coinage"		},
	{0x0c, 0x01, 0x10, 0x00, "2 Coins 1 Credits"	},
	{0x0c, 0x01, 0x10, 0x10, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0e, 0x01, 0x01, 0x00, "Upright"		},
	{0x0e, 0x01, 0x01, 0x01, "Cocktail"		},
};

STDDIPINFO(Phoenixa)

static struct BurnDIPInfo PhoenixtDIPList[]=
{
	{0x0c, 0xff, 0xff, 0x60, NULL			},
	{0x0e, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0c, 0x01, 0x03, 0x00, "3"			},
	{0x0c, 0x01, 0x03, 0x01, "4"			},
	{0x0c, 0x01, 0x03, 0x02, "5"			},
	{0x0c, 0x01, 0x03, 0x03, "6"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x0c, 0x01, 0x0c, 0x00, "3K 30K"		},
	{0x0c, 0x01, 0x0c, 0x04, "4K 40K"		},
	{0x0c, 0x01, 0x0c, 0x08, "5K 50K"		},
	{0x0c, 0x01, 0x0c, 0x0c, "6K 60K"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0e, 0x01, 0x01, 0x00, "Upright"		},
	{0x0e, 0x01, 0x01, 0x01, "Cocktail"		},
};

STDDIPINFO(Phoenixt)

static struct BurnDIPInfo Phoenix3DIPList[]=
{
	{0x0d, 0xff, 0xff, 0x60, NULL			},
	{0x0f, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0d, 0x01, 0x03, 0x00, "3"			},
	{0x0d, 0x01, 0x03, 0x01, "4"			},
	{0x0d, 0x01, 0x03, 0x02, "5"			},
	{0x0d, 0x01, 0x03, 0x03, "6"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x0d, 0x01, 0x0c, 0x00, "3K 30K"		},
	{0x0d, 0x01, 0x0c, 0x04, "4K 40K"		},
	{0x0d, 0x01, 0x0c, 0x08, "5K 50K"		},
	{0x0d, 0x01, 0x0c, 0x0c, "6K 60K"		},

	{0   , 0xfe, 0   ,    2, "Coinage"		},
	{0x0d, 0x01, 0x40, 0x40, "A- 2c/1c B-1c/3c"	},
	{0x0d, 0x01, 0x40, 0x00, "A- 1c/1c B-1c/6c"	},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0f, 0x01, 0x01, 0x00, "Upright"		},
	{0x0f, 0x01, 0x01, 0x01, "Cocktail"		},
};

STDDIPINFO(Phoenix3)

static struct BurnDIPInfo FalconzDIPList[]=
{
	{0x0c, 0xff, 0xff, 0x60, NULL			},
	{0x0e, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0c, 0x01, 0x03, 0x00, "2"			},
	{0x0c, 0x01, 0x03, 0x01, "3"			},
	{0x0c, 0x01, 0x03, 0x02, "4"			},
	{0x0c, 0x01, 0x03, 0x03, "5"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x0c, 0x01, 0x0c, 0x00, "3K 30K"		},
	{0x0c, 0x01, 0x0c, 0x04, "4K 40K"		},
	{0x0c, 0x01, 0x0c, 0x08, "5K 50K"		},
	{0x0c, 0x01, 0x0c, 0x0c, "6K 60K"		},

	{0   , 0xfe, 0   ,    2, "Coinage"		},
	{0x0c, 0x01, 0x40, 0x00, "2 Coins 1 Credits"	},
	{0x0c, 0x01, 0x40, 0x40, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0e, 0x01, 0x01, 0x00, "Upright"		},
	{0x0e, 0x01, 0x01, 0x01, "Cocktail"		},
};

STDDIPINFO(Falconz)

static struct BurnDIPInfo NextfaseDIPList[]=
{
	{0x0c, 0xff, 0xff, 0x60, NULL			},
	{0x0e, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0c, 0x01, 0x03, 0x00, "1"			},
	{0x0c, 0x01, 0x03, 0x01, "2"			},
	{0x0c, 0x01, 0x03, 0x02, "3"			},
	{0x0c, 0x01, 0x03, 0x03, "4"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x0c, 0x01, 0x0c, 0x00, "3K 30K"		},
	{0x0c, 0x01, 0x0c, 0x04, "4K 40K"		},
	{0x0c, 0x01, 0x0c, 0x08, "5K 50K"		},
	{0x0c, 0x01, 0x0c, 0x0c, "6K 60K"		},

	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{0x0c, 0x01, 0x60, 0x00, "A - 1c/1c B - 1c/2c"	},
	{0x0c, 0x01, 0x60, 0x20, "A - 2c/3c B - 1c/3c"	},
	{0x0c, 0x01, 0x60, 0x40, "A - 1c/2c B - 1c/4c"	},
	{0x0c, 0x01, 0x60, 0x60, "A - 2c/5c B - 1c/5c"	},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0e, 0x01, 0x01, 0x00, "Upright"		},
	{0x0e, 0x01, 0x01, 0x01, "Cocktail"		},
};

STDDIPINFO(Nextfase)

static void bankswitch(INT32 data)
{
	ram_bank = data & 0x01;

	i8080MapMemory(DrvI8085RAM + ram_bank * 0x1000,	0x4000, 0x4fff, MAP_RAM);
}

#ifdef USE_Z80
static void __fastcall phoenix_main_write(UINT16 address, UINT8 data)
#else
static void phoenix_main_write(UINT16 address, UINT8 data)
#endif
{
	switch (address & ~0x03ff)
	{
		case 0x5000:
		{
			cocktail_mode = (data & 1) && (DrvDips[2] & 1);
			bankswitch(data);
			palette_bank = (data & 0x02) >> 1;
			if (pleiads) {
				palette_bank |= (data & 0x04) >> 1;
				pleiads_protection_question = data & 0xfc;
				pleiads_sound_control_c_w(address - 0x5000, data);
			}
		}
		return;

		case 0x5800:
			scrollx = data;
		return;

		case 0x6000: // control a
			if (phoenixmode) phoenix_sound_control_a_w(address, data);
			if (pleiads) pleiads_sound_control_a_w(address, data);
			return;

		case 0x6800: // control b
			if (phoenixmode) phoenix_sound_control_b_w(address, data);
			if (pleiads) pleiads_sound_control_b_w(address, data);
			return;
	}
}

static UINT8 pleiads_protection_read()
{
	if (pleiads_protection_question == 0x0c || pleiads_protection_question == 0x30) {
		return 0x08;
	}

	return 0;
}

#ifdef USE_Z80
static UINT8 __fastcall phoenix_main_read(UINT16 address)
#else
static UINT8 phoenix_main_read(UINT16 address)
#endif
{
	switch (address & ~0x03ff)
	{
		case 0x5000:
			return DrvDips[1];

		case 0x7000: {
			UINT8 res = (DrvInputs[0] & 0xf) | (DrvInputs[1+cocktail_mode] << 4);

			if (pleiads) {
				res = (res & 0xf7) | pleiads_protection_read();
			}

			return res;
		}

		case 0x7800:
			return (DrvDips[0] & 0x7f) | (vblank ? 0x80 : 0);
	}

	return 0;
}

static INT32 DrvDoReset()
{
	memset(AllRam, 0, RamEnd - AllRam);

	i8080Open(0);
	i8080Reset();
	bankswitch(0);
	i8080Close();

	if (phoenixmode)
		phoenix_sound_reset();

	if (pleiads)
		pleiads_sound_reset();

	pleiads_protection_question = 0;
	scrollx = 0;
	palette_bank = 0;
	cocktail_mode = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvI8085ROM		= Next; Next += 0x004000;

	DrvGfxROM0		= Next; Next += 0x004000;
	DrvGfxROM1		= Next; Next += 0x004000;

	DrvColPROM		= Next; Next += 0x000200;

	DrvPalette		= (UINT32*)Next; Next += 0x0100 * sizeof(UINT32);

	AllRam			= Next;

	DrvI8085RAM		= Next; Next += 0x002000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[2] = { 0x4000, 0 };
	INT32 XOffs[8] = { STEP8(7,-1) };
	INT32 YOffs[8] = { STEP8(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x1000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x1000);

	GfxDecode(0x0100, 2, 8, 8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x1000);

	GfxDecode(0x0100, 2, 8, 8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM1);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit(INT32 single_prom)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvI8085ROM + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvI8085ROM + 0x0800,  1, 1)) return 1;
		if (BurnLoadRom(DrvI8085ROM + 0x1000,  2, 1)) return 1;
		if (BurnLoadRom(DrvI8085ROM + 0x1800,  3, 1)) return 1;
		if (BurnLoadRom(DrvI8085ROM + 0x2000,  4, 1)) return 1;
		if (BurnLoadRom(DrvI8085ROM + 0x2800,  5, 1)) return 1;
		if (BurnLoadRom(DrvI8085ROM + 0x3000,  6, 1)) return 1;
		if (BurnLoadRom(DrvI8085ROM + 0x3800,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0  + 0x0000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x0800,  9, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1  + 0x0000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x0800, 11, 1)) return 1;

		if (BurnLoadRom(DrvColPROM  + 0x0000, 12, 1)) return 1;

		if (single_prom == 0) {
			if (BurnLoadRom(DrvColPROM  + 0x0100, 13, 1)) return 1;
		} else {
			memcpy (DrvColPROM + 0x100, DrvColPROM, 0x100);
			
			for (INT32 i = 0; i < 0x100; i++) {
				DrvColPROM[i] >>= 4;
				DrvColPROM[i+0x100] &= 0x0f;
			}
		}

		DrvGfxDecode();
	}

	i8085Init(0);
	i8080Open(0);
	i8080MapMemory(DrvI8085ROM,	0x0000, 0x3fff, MAP_ROM);
	i8080SetWriteHandler(phoenix_main_write);
	i8080SetReadHandler(phoenix_main_read);
	i8080Close();

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 PhoenixInit()
{
	phoenixmode = 1;
	phoenix_sound_init();

	return DrvInit(0);
}

static INT32 CondorInit()
{
	phoenixmode = 1;
	phoenix_sound_init();

	condor = 1;
	return DrvInit(0);
}

static INT32 SinglePromInit()
{
	phoenixmode = 1;
	phoenix_sound_init();

	return DrvInit(1);
}

static INT32 PleiadsInit()
{
	pleiads = 1;
	pleiads_sound_init(0);

	return DrvInit(0);
}

static INT32 DrvExit()
{
	GenericTilesExit();

	i8080Exit();

	if (phoenixmode)
		phoenix_sound_deinit();

	if (pleiads)
		pleiads_sound_deinit();

	condor = 0;
	pleiads = 0;
	phoenixmode = 0;

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	UINT32 pal[0x100];

	for (INT32 i = 0; i < 0x100; i++)
	{
		INT32 b0, b1, r, g, b;

		b0 = (DrvColPROM[0x000 + i] >> 0) & 1;
		b1 = (DrvColPROM[0x100 + i] >> 0) & 1;
		r = (b0 * 0x55) + (b1 * 0xaa);

		b0 = (DrvColPROM[0x000 + i] >> 2) & 1;
		b1 = (DrvColPROM[0x100 + i] >> 2) & 1;
		g = (b0 * 0x55) + (b1 * 0xaa);

		b0 = (DrvColPROM[0x000 + i] >> 1) & 1;
		b1 = (DrvColPROM[0x100 + i] >> 1) & 1;
		b = (b0 * 0x55) + (b1 * 0xaa);

		pal[i] = BurnHighCol(r,g,b,0);
	}

	INT32 mask = BurnDrvGetPaletteEntries() - 1;

	for (INT32 i = 0; i < 256; i++)
	{
		DrvPalette[i] = pal[(((i << 3 ) & 0x18) | ((i>>2) & 0x07) | (i & 0xe0)) & mask];
	}
}

static void draw_layer(UINT8 *ram, UINT8 *gfx, INT32 color_offset, INT32 transparent, INT32 xscroll)
{
	for (INT32 offs = 0; offs < 32 * 26; offs++)
	{
		INT32 sx = (offs % 32) * 8;
		INT32 sy = (offs / 32) * 8;

		sx -= xscroll;

		if (sx < 0) sx += 256; // no shit. weird, eh? - dink (fixes background scrolling @ the bottom of the screen)

		if (cocktail_mode) {
			sx = 208 - sx;
			sy = 248 - sx;
		}

		INT32 code = ram[offs];
		INT32 color = ((code & 0xe0) >> 5) + color_offset + (palette_bank << 4);

		if (sx > nScreenWidth || sy > nScreenHeight) continue;

		if (transparent) {
			Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, gfx);
		} else {
			Render8x8Tile_Clip(pTransDraw, code, sx, sy, color, 2, 0, gfx);
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

	if (nBurnLayer & 1) draw_layer(DrvI8085RAM + 0x800 + (ram_bank*0x1000), DrvGfxROM0, 0, 0, scrollx);
	if (nBurnLayer & 2) draw_layer(DrvI8085RAM + 0x000 + (ram_bank*0x1000), DrvGfxROM1, 8, 1, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, (condor) ? 0 : 0x0f, 3);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	i8080Open(0);
	vblank = 1;
	i8080Run(((2750000 / 60)*250)/256);
	vblank = 0;
	i8080Run(((2750000 / 60)*  6)/256);
	i8080Close();

	if (pBurnSoundOut) {
		if (phoenixmode)
			phoenix_sound_update(pBurnSoundOut, nBurnSoundLen);

		if (pleiads)
			pleiads_sound_update(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}


static INT32 DrvScan(INT32 nAction,INT32 *pnMin)
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

		i8080Scan(nAction);

		SCAN_VAR(scrollx);
		SCAN_VAR(ram_bank);
		SCAN_VAR(palette_bank);
		SCAN_VAR(pleiads_protection_question);
		SCAN_VAR(cocktail_mode);

		i8080Open(0);
		bankswitch(ram_bank);
		i8080Close();
	}

	return 0;
}


// Phoenix (Amstar, set 1)

static struct BurnRomInfo phoenixRomDesc[] = {
	{ "ic45",			0x0800, 0x9f68086b, 1 | BRF_PRG | BRF_ESS }, //  0 i8085 Code
	{ "ic46",			0x0800, 0x273a4a82, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ic47",			0x0800, 0x3d4284b9, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ic48",			0x0800, 0xcb5d9915, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "h5-ic49.5a",		0x0800, 0xa105e4e7, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "h6-ic50.6a",		0x0800, 0xac5e9ec1, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "h7-ic51.7a",		0x0800, 0x2eab35b4, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "h8-ic52.8a",		0x0800, 0xaff8e9c5, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "ic23.3d",		0x0800, 0x3c7e623f, 2 | BRF_GRA },           //  8 Background Tiles
	{ "ic24.4d",		0x0800, 0x59916d3b, 2 | BRF_GRA },           //  9

	{ "b1-ic39.3b",		0x0800, 0x53413e8f, 3 | BRF_GRA },           // 10 Foreground Tiles
	{ "b2-ic40.4b",		0x0800, 0x0be2ba91, 3 | BRF_GRA },           // 11

	{ "mmi6301.ic40",	0x0100, 0x79350b25, 4 | BRF_GRA },           // 12 Color PROMs
	{ "mmi6301.ic41",	0x0100, 0xe176b768, 4 | BRF_GRA },           // 13
};

STD_ROM_PICK(phoenix)
STD_ROM_FN(phoenix)

struct BurnDriver BurnDrvPhoenix = {
	"phoenix", NULL, NULL, NULL, "1980",
	"Phoenix (Amstar)\0", NULL, "Amstar", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, phoenixRomInfo, phoenixRomName, NULL, NULL, PhoenixInputInfo, PhoenixDIPInfo,
	PhoenixInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	208, 256, 3, 4
};

// Phoenix (Amstar, set 2)

static struct BurnRomInfo phoenix2RomDesc[] = {
	{ "ic45-pg1.1a",	0x0800, 0x5b8c55a8, 1 | BRF_PRG | BRF_ESS }, //  0 i8085 Code
	{ "ic46-pg2.2a",	0x0800, 0x273a4a82, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ic47-pg3.3a",	0x0800, 0xcbbb8839, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ic48-pg4.4a",	0x0800, 0xcb5d9915, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ic49-pg5.5a",	0x0800, 0x73bcd2e1, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "ic50-pg6.6a",	0x0800, 0xac5e9ec1, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "ic51-pg7.7a",	0x0800, 0x2eab35b4, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "ic52-pg8.8a",	0x0800, 0xaff8e9c5, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "ic23-cg1.3d",	0x0800, 0x3c7e623f, 2 | BRF_GRA },           //  8 Background Tiles
	{ "ic24-cg2.4d",	0x0800, 0x59916d3b, 2 | BRF_GRA },           //  9

	{ "ic39-cg3.3b",	0x0800, 0x53413e8f, 3 | BRF_GRA },           // 10 Foreground Tiles
	{ "ic40-cg4.4b",	0x0800, 0x0be2ba91, 3 | BRF_GRA },           // 11

	{ "mmi6301.ic40",	0x0100, 0x79350b25, 4 | BRF_GRA },           // 12 Color PROMs
	{ "mmi6301.ic41",	0x0100, 0xe176b768, 4 | BRF_GRA },           // 13
};

STD_ROM_PICK(phoenix2)
STD_ROM_FN(phoenix2)

struct BurnDriver BurnDrvPhoenix2 = {
	"phoenix2", "phoenix", NULL, NULL, "1980",
	"Phoenix (Amstar, set 2)\0", NULL, "Amstar", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, phoenix2RomInfo, phoenix2RomName, NULL, NULL, PhoenixInputInfo, PhoenixDIPInfo,
	PhoenixInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	208, 256, 3, 4
};

// Phoenix (Centuri, set 1)

static struct BurnRomInfo phoenixaRomDesc[] = {
	{ "1-ic45.1a",		0x0800, 0xc7a9b499, 1 | BRF_PRG | BRF_ESS }, //  0 i8085 Code
	{ "2-ic46.2a",		0x0800, 0xd0e6ae1b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3-ic47.3a",		0x0800, 0x64bf463a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4-ic48.4a",		0x0800, 0x1b20fe62, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "h5-ic49.5a",		0x0800, 0xa105e4e7, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "h6-ic50.6a",		0x0800, 0xac5e9ec1, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "h7-ic51.7a",		0x0800, 0x2eab35b4, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "h8-ic52.8a",		0x0800, 0xaff8e9c5, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "ic23.3d",		0x0800, 0x3c7e623f, 2 | BRF_GRA },           //  8 Background Tiles
	{ "ic24.4d",		0x0800, 0x59916d3b, 2 | BRF_GRA },           //  9

	{ "b1-ic39.3b",		0x0800, 0x53413e8f, 3 | BRF_GRA },           // 10 Foreground Tiles
	{ "b2-ic40.4b",		0x0800, 0x0be2ba91, 3 | BRF_GRA },           // 11

	{ "mmi6301.ic40",	0x0100, 0x79350b25, 4 | BRF_GRA },           // 12 Color Proms
	{ "mmi6301.ic41",	0x0100, 0xe176b768, 4 | BRF_GRA },           // 13
};

STD_ROM_PICK(phoenixa)
STD_ROM_FN(phoenixa)

struct BurnDriver BurnDrvPhoenixa = {
	"phoenixa", "phoenix", NULL, NULL, "1980",
	"Phoenix (Centuri, set 1)\0", NULL, "Amstar (Centuri license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, phoenixaRomInfo, phoenixaRomName, NULL, NULL, PhoenixInputInfo, PhoenixaDIPInfo,
	PhoenixInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	208, 256, 3, 4
};


// Phoenix (Centuri, set 2)

static struct BurnRomInfo phoenixbRomDesc[] = {
	{ "1-ic45.1a",		0x0800, 0xc7a9b499, 1 | BRF_PRG | BRF_ESS }, //  0 i8085 Code
	{ "2-ic46.2a",		0x0800, 0xd0e6ae1b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3-ic47.3a",		0x0800, 0x64bf463a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4-ic48.4a",		0x0800, 0x1b20fe62, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "phoenixc.49",	0x0800, 0x1a1ce0d0, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "h6-ic50.6a",		0x0800, 0xac5e9ec1, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "h7-ic51.7a",		0x0800, 0x2eab35b4, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "h8-ic52.8a",		0x0800, 0xaff8e9c5, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "ic23.3d",		0x0800, 0x3c7e623f, 2 | BRF_GRA },           //  8 Background Tiles
	{ "ic24.4d",		0x0800, 0x59916d3b, 2 | BRF_GRA },           //  9

	{ "phoenixc.39",	0x0800, 0xbb0525ed, 3 | BRF_GRA },           // 10 Foreground Tiles
	{ "phoenixc.40",	0x0800, 0x4178aa4f, 3 | BRF_GRA },           // 11

	{ "mmi6301.ic40",	0x0100, 0x79350b25, 4 | BRF_GRA },           // 12 Color Proms
	{ "mmi6301.ic41",	0x0100, 0xe176b768, 4 | BRF_GRA },           // 13
};

STD_ROM_PICK(phoenixb)
STD_ROM_FN(phoenixb)

struct BurnDriver BurnDrvPhoenixb = {
	"phoenixb", "phoenix", NULL, NULL, "1980",
	"Phoenix (Centuri, set 2)\0", NULL, "Amstar (Centuri license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, phoenixbRomInfo, phoenixbRomName, NULL, NULL, PhoenixInputInfo, PhoenixaDIPInfo,
	PhoenixInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	208, 256, 3, 4
};


// Phoenix (D&L bootleg)

static struct BurnRomInfo phoenixdalRomDesc[] = {
	{ "dal.a1",			0x0800, 0x5b8c55a8, 1 | BRF_PRG | BRF_ESS }, //  0 i8085 Code
	{ "dal.a2",			0x0800, 0xdbc942fa, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "dal.a3",			0x0800, 0xcbbb8839, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "dal.a4",			0x0800, 0x228b76ad, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "d2716,dal.a5",	0x0800, 0x1a1ce0d0, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "h6-ic50.6a",		0x0800, 0xac5e9ec1, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "h7-ic51.7a",		0x0800, 0x2eab35b4, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "dal.a8",			0x0800, 0x0a0f92c0, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "ic23.3d",		0x0800, 0x3c7e623f, 2 | BRF_GRA },           //  8 Background Tiles
	{ "ic24.4d",		0x0800, 0x59916d3b, 2 | BRF_GRA },           //  9

	{ "dal.b3",			0x0800, 0xbb0525ed, 3 | BRF_GRA },           // 10 Foreground Tiles
	{ "dal.b4",			0x0800, 0x4178aa4f, 3 | BRF_GRA },           // 11

	{ "mmi6301.ic40",	0x0100, 0x79350b25, 4 | BRF_GRA },           // 12 Color Proms
	{ "mmi6301.ic41",	0x0100, 0xe176b768, 4 | BRF_GRA },           // 13
};

STD_ROM_PICK(phoenixdal)
STD_ROM_FN(phoenixdal)

struct BurnDriver BurnDrvPhoenixdal = {
	"phoenixdal", "phoenix", NULL, NULL, "1980",
	"Phoenix (D&L bootleg)\0", NULL, "bootleg (D&L)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, phoenixdalRomInfo, phoenixdalRomName, NULL, NULL, PhoenixInputInfo, PhoenixtDIPInfo,
	PhoenixInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	208, 256, 3, 4
};


// Phoenix (Taito)

static struct BurnRomInfo phoenixtRomDesc[] = {
	{ "phoenix.45",		0x0800, 0x5b8c55a8, 1 | BRF_PRG | BRF_ESS }, //  0 i8085 Code
	{ "phoenix.46",		0x0800, 0xdbc942fa, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "phoenix.47",		0x0800, 0xcbbb8839, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "phoenix.48",		0x0800, 0xcb65eff8, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "phoenix.49",		0x0800, 0xc8a5d6d6, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "h6-ic50.6a",		0x0800, 0xac5e9ec1, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "h7-ic51.7a",		0x0800, 0x2eab35b4, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "phoenix.52",		0x0800, 0xb9915263, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "ic23.3d",		0x0800, 0x3c7e623f, 2 | BRF_GRA },           //  8 Background Tiles
	{ "ic24.4d",		0x0800, 0x59916d3b, 2 | BRF_GRA },           //  9

	{ "b1-ic39.3b",		0x0800, 0x53413e8f, 3 | BRF_GRA },           // 10 Foreground Tiles
	{ "b2-ic40.4b",		0x0800, 0x0be2ba91, 3 | BRF_GRA },           // 11

	{ "mmi6301.ic40",	0x0100, 0x79350b25, 4 | BRF_GRA },           // 12 Color Proms
	{ "mmi6301.ic41",	0x0100, 0xe176b768, 4 | BRF_GRA },           // 13
};

STD_ROM_PICK(phoenixt)
STD_ROM_FN(phoenixt)

struct BurnDriver BurnDrvPhoenixt = {
	"phoenixt", "phoenix", NULL, NULL, "1980",
	"Phoenix (Taito)\0", NULL, "Amstar (Taito license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, phoenixtRomInfo, phoenixtRomName, NULL, NULL, PhoenixInputInfo, PhoenixtDIPInfo,
	PhoenixInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	208, 256, 3, 4
};


// Phoenix (Taito Japan)

static struct BurnRomInfo phoenixjRomDesc[] = {
	{ "pn01.45",		0x0800, 0x5b8c55a8, 1 | BRF_PRG | BRF_ESS }, //  0 i8085 Code
	{ "pn02.46",		0x0800, 0xdbc942fa, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "pn03.47",		0x0800, 0xcbbb8839, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "pn04.48",		0x0800, 0xdd41f22b, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "pn05.49",		0x0800, 0x1a1ce0d0, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "pn06.50",		0x0800, 0xac5e9ec1, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "pn07.51",		0x0800, 0x2eab35b4, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "pn08.52",		0x0800, 0xb9915263, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "pn11.23",		0x0800, 0x3c7e623f, 2 | BRF_GRA },           //  8 Background Tiles
	{ "pn12.24",		0x0800, 0x59916d3b, 2 | BRF_GRA },           //  9

	{ "pn09.39",		0x0800, 0x53413e8f, 3 | BRF_GRA },           // 10 Foreground Tiles
	{ "pn10.40",		0x0800, 0x0be2ba91, 3 | BRF_GRA },           // 11

	{ "pn14.40",		0x0100, 0x79350b25, 4 | BRF_GRA },           // 12 Color Proms
	{ "pn13.41",		0x0100, 0xe176b768, 4 | BRF_GRA },           // 13
};

STD_ROM_PICK(phoenixj)
STD_ROM_FN(phoenixj)

struct BurnDriver BurnDrvPhoenixj = {
	"phoenixj", "phoenix", NULL, NULL, "1980",
	"Phoenix (Taito Japan)\0", NULL, "Amstar (Taito Japan license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, phoenixjRomInfo, phoenixjRomName, NULL, NULL, PhoenixInputInfo, PhoenixtDIPInfo,
	PhoenixInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	208, 256, 3, 4
};


// Phoenix (T.P.N. bootleg)

static struct BurnRomInfo phoenix3RomDesc[] = {
	{ "phoenix3.45",	0x0800, 0xa362cda0, 1 | BRF_PRG | BRF_ESS }, //  0 i8085 Code
	{ "phoenix3.46",	0x0800, 0x5748f486, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "phoenix.47",		0x0800, 0xcbbb8839, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "phoenix3.48",	0x0800, 0xb5d97a4d, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "h5-ic49.5a",		0x0800, 0xa105e4e7, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "h6-ic50.6a",		0x0800, 0xac5e9ec1, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "h7-ic51.7a",		0x0800, 0x2eab35b4, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "phoenix3.52",	0x0800, 0xd2c5c984, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "ic23.3d",		0x0800, 0x3c7e623f, 2 | BRF_GRA },           //  8 Background Tiles
	{ "ic24.4d",		0x0800, 0x59916d3b, 2 | BRF_GRA },           //  9

	{ "b1-ic39.3b",		0x0800, 0x53413e8f, 3 | BRF_GRA },           // 10 Foreground Tiles
	{ "b2-ic40.4b",		0x0800, 0x0be2ba91, 3 | BRF_GRA },           // 11

	{ "mmi6301.ic40",	0x0100, 0x79350b25, 4 | BRF_GRA },           // 12 Color Proms
	{ "mmi6301.ic41",	0x0100, 0xe176b768, 4 | BRF_GRA },           // 13
};

STD_ROM_PICK(phoenix3)
STD_ROM_FN(phoenix3)

struct BurnDriver BurnDrvPhoenix3 = {
	"phoenix3", "phoenix", NULL, NULL, "1980",
	"Phoenix (T.P.N. bootleg)\0", NULL, "bootleg (T.P.N.)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, phoenix3RomInfo, phoenix3RomName, NULL, NULL, Phoenix3InputInfo, Phoenix3DIPInfo,
	PhoenixInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	208, 256, 3, 4
};


// Phoenix (Irecsa / G.G.I Corp, set 1)

static struct BurnRomInfo phoenixcRomDesc[] = {
	{ "phoenix.45",		0x0800, 0x5b8c55a8, 1 | BRF_PRG | BRF_ESS }, //  0 i8085 Code
	{ "phoenix.46",		0x0800, 0xdbc942fa, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "phoenix.47",		0x0800, 0xcbbb8839, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "phoenixc.48",	0x0800, 0x5ae0b215, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "phoenixc.49",	0x0800, 0x1a1ce0d0, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "h6-ic50.6a",		0x0800, 0xac5e9ec1, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "h7-ic51.7a",		0x0800, 0x2eab35b4, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "phoenixc.52",	0x0800, 0x8424d7c4, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "ic23.3d",		0x0800, 0x3c7e623f, 2 | BRF_GRA },           //  8 Background Tiles
	{ "ic24.4d",		0x0800, 0x59916d3b, 2 | BRF_GRA },           //  9

	{ "phoenixc.39",	0x0800, 0xbb0525ed, 3 | BRF_GRA },           // 10 Foreground Tiles
	{ "phoenixc.40",	0x0800, 0x4178aa4f, 3 | BRF_GRA },           // 11

	{ "mmi6301.ic40",	0x0100, 0x79350b25, 4 | BRF_GRA },           // 12 Color Proms
	{ "mmi6301.ic41",	0x0100, 0xe176b768, 4 | BRF_GRA },           // 13
};

STD_ROM_PICK(phoenixc)
STD_ROM_FN(phoenixc)

struct BurnDriver BurnDrvPhoenixc = {
	"phoenixc", "phoenix", NULL, NULL, "1981",
	"Phoenix (Irecsa / G.G.I Corp, set 1)\0", NULL, "bootleg? (Irecsa / G.G.I Corp)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, phoenixcRomInfo, phoenixcRomName, NULL, NULL, PhoenixInputInfo, PhoenixtDIPInfo,
	PhoenixInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	208, 256, 3, 4
};


// Phoenix (Irecsa / G.G.I Corp, set 2)
// verified main and ROMs PCBs and 2 PROMs

static struct BurnRomInfo phoenixc2RomDesc[] = {
	{ "phoenix.45",		0x0800, 0x5b8c55a8, 1 | BRF_PRG | BRF_ESS }, //  0 i8085 Code
	{ "phoenix.46",		0x0800, 0xdbc942fa, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "phoenix.47",		0x0800, 0xcbbb8839, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "01.ic48",		0x0800, 0xf28e16d8, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "phoenixc.49",	0x0800, 0x1a1ce0d0, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "h6-ic50.6a",		0x0800, 0xac5e9ec1, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "h7-ic51.7a",		0x0800, 0x2eab35b4, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "phoenixc.52",	0x0800, 0x8424d7c4, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "ic23.3d",		0x0800, 0x3c7e623f, 2 | BRF_GRA },           //  8 Background Tiles
	{ "ic24.4d",		0x0800, 0x59916d3b, 2 | BRF_GRA },           //  9

	{ "phoenixc.39",	0x0800, 0xbb0525ed, 3 | BRF_GRA },           // 10 Foreground Tiles
	{ "phoenixc.40",	0x0800, 0x4178aa4f, 3 | BRF_GRA },           // 11

	{ "mmi6301.ic40",	0x0100, 0x79350b25, 4 | BRF_GRA },           // 12 Color Proms
	{ "mmi6301.ic41",	0x0100, 0xe176b768, 4 | BRF_GRA },           // 13

	{ "tbp24sa10n.183.bin",	0x0100, 0x47f5e887, 5 | BRF_OPT },           // 14 PLDs
	{ "tbp24sa10n.184.bin",	0x0100, 0x931f3292, 5 | BRF_OPT },           // 15
	{ "tbp24sa10n.185.bin",	0x0100, 0x0a06bd1b, 5 | BRF_OPT },           // 16
};

STD_ROM_PICK(phoenixc2)
STD_ROM_FN(phoenixc2)

struct BurnDriver BurnDrvPhoenixc2 = {
	"phoenixc2", "phoenix", NULL, NULL, "1981",
	"Phoenix (Irecsa / G.G.I Corp, set 2)\0", NULL, "bootleg? (Irecsa / G.G.I Corp)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, phoenixc2RomInfo, phoenixc2RomName, NULL, NULL, PhoenixInputInfo, PhoenixtDIPInfo,
	PhoenixInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	208, 256, 3, 4
};


// Phoenix (Irecsa / G.G.I Corp, set 3)
// verified main and ROMs PCBs and 2 PROMs

static struct BurnRomInfo phoenixc3RomDesc[] = {
	{ "phoenix.45",		0x0800, 0x5b8c55a8, 1 | BRF_PRG | BRF_ESS }, //  0 i8085 Code
	{ "phoenix.46",		0x0800, 0xdbc942fa, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "phoenix.47",		0x0800, 0xcbbb8839, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.a4",			0x0800, 0x61514bed, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "phoenixc.49",	0x0800, 0x1a1ce0d0, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "h6-ic50.6a",		0x0800, 0xac5e9ec1, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "h7-ic51.7a",		0x0800, 0x2eab35b4, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "phoenixc.52",	0x0800, 0x8424d7c4, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "ic23.3d",		0x0800, 0x3c7e623f, 2 | BRF_GRA },           //  8 Background Tiles
	{ "ic24.4d",		0x0800, 0x59916d3b, 2 | BRF_GRA },           //  9

	{ "phoenixc.39",	0x0800, 0xbb0525ed, 3 | BRF_GRA },           // 10 Foreground Tiles
	{ "phoenixc.40",	0x0800, 0x4178aa4f, 3 | BRF_GRA },           // 11

	{ "mmi6301.ic40",	0x0100, 0x79350b25, 4 | BRF_GRA },           // 12 Color Proms
	{ "mmi6301.ic41",	0x0100, 0xe176b768, 4 | BRF_GRA },           // 13
};

STD_ROM_PICK(phoenixc3)
STD_ROM_FN(phoenixc3)

struct BurnDriver BurnDrvPhoenixc3 = {
	"phoenixc3", "phoenix", NULL, NULL, "1981",
	"Phoenix (Irecsa / G.G.I Corp, set 3)\0", NULL, "bootleg? (Irecsa / G.G.I Corp)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, phoenixc3RomInfo, phoenixc3RomName, NULL, NULL, PhoenixInputInfo, PhoenixtDIPInfo,
	PhoenixInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	208, 256, 3, 4
};


// Phoenix (Irecsa / G.G.I Corp, set 4)
// verified main and ROMs PCBs and 2 PROMs

static struct BurnRomInfo phoenixc4RomDesc[] = {
	{ "phoenix.45",		0x0800, 0x5b8c55a8, 1 | BRF_PRG | BRF_ESS }, //  0 i8085 Code
	{ "phoenix.46",		0x0800, 0xdbc942fa, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "phoenix.47",		0x0800, 0xcbbb8839, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "phoenixd.48",	0x0800, 0x6e51f009, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "phoenixc.49",	0x0800, 0x1a1ce0d0, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "cond06c.bin",	0x0800, 0x8c83bff7, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "vautor07.1m",	0x0800, 0x079ac364, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "phoenixc.52",	0x0800, 0x8424d7c4, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "ic23.3d",		0x0800, 0x3c7e623f, 2 | BRF_GRA },           //  8 Background Tiles
	{ "ic24.4d",		0x0800, 0x59916d3b, 2 | BRF_GRA },           //  9

	{ "phoenixd.3b",	0x0800, 0x31c06c22, 3 | BRF_GRA },           // 10 Foreground Tiles
	{ "phoenixc.40",	0x0800, 0x4178aa4f, 3 | BRF_GRA },           // 11

	{ "mmi6301.ic41",	0x0100, 0xe176b768, 4 | BRF_GRA },           // 12 Color Proms
};

STD_ROM_PICK(phoenixc4)
STD_ROM_FN(phoenixc4)

struct BurnDriver BurnDrvPhoenixc4 = {
	"phoenixc4", "phoenix", NULL, NULL, "1981",
	"Phoenix (Irecsa / G.G.I Corp, set 4)\0", NULL, "bootleg? (Irecsa / G.G.I Corp)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, phoenixc4RomInfo, phoenixc4RomName, NULL, NULL, PhoenixInputInfo, PhoenixtDIPInfo,
	SinglePromInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	208, 256, 3, 4
};


// Phoenix (IDI bootleg)
// verified single PCB, single PROM
// Needs correct color PROM decode

static struct BurnRomInfo phoenixiRomDesc[] = {
	{ "0201.bin",		0x0800, 0xc0f73929, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "0202.bin",		0x0800, 0x440d56e8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "0203.bin",		0x0800, 0x750b059b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "0204.bin",		0x0800, 0xe2d3271f, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "0205.bin",		0x0800, 0x1ff3a982, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "0206.bin",		0x0800, 0x8c83bff7, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "0207.bin",		0x0800, 0x805ec2e8, 1 | BRF_PRG | BRF_ESS }, //  6
	// 0208.bin wasn't readable, but very probably matches the one from condor
	{ "cond08c.bin",	0x0800, 0x1edebb45, 1 | BRF_PRG | BRF_ESS }, //  7
	
	{ "0209.bin",		0x0800, 0x3c7e623f, 2 | BRF_GRA },           //  8 Background Tiles
	{ "0210.bin",		0x0800, 0x59916d3b, 2 | BRF_GRA },           //  9

	{ "0211.bin",		0x0800, 0x53c52eb0, 3 | BRF_GRA },           // 10 Foreground Tiles
	{ "0212.bin",		0x0800, 0xeba42f0f, 3 | BRF_GRA },           // 11

	{ "sn74s471n.bin",	0x0100, 0xc68a49bc, 4 | BRF_GRA },           // 12 Color Proms
};

STD_ROM_PICK(phoenixi)
STD_ROM_FN(phoenixi)

struct BurnDriver BurnDrvPhoenixi = {
	"phoenixi", "phoenix", NULL, NULL, "1981",
	"Phoenix (IDI bootleg)\0", NULL, "bootleg (IDI)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, phoenixiRomInfo, phoenixiRomName, NULL, NULL, CondorInputInfo, CondorDIPInfo,
	SinglePromInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	208, 256, 3, 4
};


// Condor (Sidam bootleg of Phoenix)

static struct BurnRomInfo condorRomDesc[] = {
	{ "cond01c.bin",	0x0800, 0xc0f73929, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "cond02c.bin",	0x0800, 0x440d56e8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "cond03c.bin",	0x0800, 0x750b059b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "cond04c.bin",	0x0800, 0xca55e1dd, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "cond05c.bin",	0x0800, 0x1ff3a982, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "cond06c.bin",	0x0800, 0x8c83bff7, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "cond07c.bin",	0x0800, 0x805ec2e8, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "cond08c.bin",	0x0800, 0x1edebb45, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "cond09c.bin",	0x0800, 0x3c7e623f, 2 | BRF_GRA },           //  8 Background Tiles
	{ "cond10c.bin",	0x0800, 0x59916d3b, 2 | BRF_GRA },           //  9

	{ "cond11c.bin",	0x0800, 0x53c52eb0, 3 | BRF_GRA },           // 10 Foreground Tiles
	{ "cond12c.bin",	0x0800, 0xeba42f0f, 3 | BRF_GRA },           // 11

	{ "mmi6301.ic40",	0x0100, 0x79350b25, 4 | BRF_GRA },           // 12 Color Proms
	{ "mmi6301.ic41",	0x0100, 0xe176b768, 4 | BRF_GRA },           // 13
};

STD_ROM_PICK(condor)
STD_ROM_FN(condor)

struct BurnDriver BurnDrvCondor = {
	"condor", "phoenix", NULL, NULL, "1981",
	"Condor (Sidam bootleg of Phoenix)\0", NULL, "bootleg (Sidam)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, condorRomInfo, condorRomName, NULL, NULL, CondorInputInfo, CondorDIPInfo,
	CondorInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	208, 256, 3, 4
};


// Condor (S C Novar bootleg of Phoenix)

static struct BurnRomInfo condornRomDesc[] = {
	{ "1.bin",			0x0800, 0xc0f73929, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "2.bin",			0x0800, 0x440d56e8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.bin",			0x0800, 0x750b059b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.bin",			0x0800, 0x78416372, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "5.bin",			0x0800, 0x1ff3a982, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "6.bin",			0x0800, 0x8c83bff7, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "7.bin",			0x0800, 0x805ec2e8, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "8.bin",			0x0800, 0x1edebb45, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "c.bin",			0x0800, 0x3c7e623f, 2 | BRF_GRA },           //  8 Background Tiles
	{ "d.bin",			0x0800, 0x59916d3b, 2 | BRF_GRA },           //  9

	{ "a.bin",			0x0800, 0xcdd5ef12, 3 | BRF_GRA },           // 10 Foreground Tiles
	{ "b.bin",			0x0800, 0xeba42f0f, 3 | BRF_GRA },           // 11

	{ "sn74s471n.bin",	0x0100, 0xc68a49bc, 4 | BRF_GRA },           // 12 Color Proms
};

STD_ROM_PICK(condorn)
STD_ROM_FN(condorn)

struct BurnDriver BurnDrvCondorn = {
	"condorn", "phoenix", NULL, NULL, "1981",
	"Condor (S C Novar bootleg of Phoenix)\0", NULL, "bootleg (S C Novar)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, condornRomInfo, condornRomName, NULL, NULL, CondorInputInfo, CondorDIPInfo,
	SinglePromInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	208, 256, 3, 4
};


// Falcon (bootleg of Phoenix) (8085A CPU)

static struct BurnRomInfo falconRomDesc[] = {
	{ "falcon.45",		0x0800, 0x80382b6c, 1 | BRF_PRG | BRF_ESS }, //  0 i8085 Code
	{ "falcon.46",		0x0800, 0x6a13193b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "phoenix.47",		0x0800, 0xcbbb8839, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "falcon.48",		0x0800, 0x084e9766, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "phoenixc.49",	0x0800, 0x1a1ce0d0, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "h6-ic50.6a",		0x0800, 0xac5e9ec1, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "falcon.51",		0x0800, 0x6e82e400, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "h8-ic52.8a",		0x0800, 0xaff8e9c5, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "ic23.3d",		0x0800, 0x3c7e623f, 2 | BRF_GRA },           //  8 Background Tiles
	{ "ic24.4d",		0x0800, 0x59916d3b, 2 | BRF_GRA },           //  9

	{ "b1-ic39.3b",		0x0800, 0x53413e8f, 3 | BRF_GRA },           // 10 Foreground Tiles
	{ "b2-ic40.4b",		0x0800, 0x0be2ba91, 3 | BRF_GRA },           // 11

	{ "mmi6301.ic40",	0x0100, 0x79350b25, 4 | BRF_GRA },           // 12 Color Proms
	{ "mmi6301.ic41",	0x0100, 0xe176b768, 4 | BRF_GRA },           // 13
};

STD_ROM_PICK(falcon)
STD_ROM_FN(falcon)

struct BurnDriver BurnDrvFalcon = {
	"falcon", "phoenix", NULL, NULL, "1980",
	"Falcon (bootleg of Phoenix) (8085A CPU)\0", NULL, "bootleg (BGV Ltd.)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, falconRomInfo, falconRomName, NULL, NULL, PhoenixInputInfo, PhoenixtDIPInfo,
	PhoenixInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	208, 256, 3, 4
};


// Vautour (bootleg of Phoenix) (8085A CPU)

static struct BurnRomInfo vautourRomDesc[] = {
	{ "vautor01.1e",	0x0800, 0xcd2807ee, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "phoenix.46",		0x0800, 0xdbc942fa, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "phoenix.47",		0x0800, 0xcbbb8839, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "vautor04.1j",	0x0800, 0x106262eb, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "phoenixc.49",	0x0800, 0x1a1ce0d0, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "vautor06.1h",	0x0800, 0xc90e3287, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "vautor07.1m",	0x0800, 0x079ac364, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "vautor08.1n",	0x0800, 0x1dbd937a, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "ic23.3d",		0x0800, 0x3c7e623f, 2 | BRF_GRA },           //  8 Background Tiles
	{ "ic24.4d",		0x0800, 0x59916d3b, 2 | BRF_GRA },           //  9

	{ "vautor12.2h",	0x0800, 0x8eff75c9, 3 | BRF_GRA },           // 10 Foreground Tiles
	{ "vautor11.2j",	0x0800, 0x369e7476, 3 | BRF_GRA },           // 11

	{ "mmi6301.ic40",	0x0100, 0x79350b25, 4 | BRF_GRA },           // 12 Color Proms
	{ "mmi6301.ic41",	0x0100, 0xe176b768, 4 | BRF_GRA },           // 13
};

STD_ROM_PICK(vautour)
STD_ROM_FN(vautour)

struct BurnDriver BurnDrvVautour = {
	"vautour", "phoenix", NULL, NULL, "1980",
	"Vautour (bootleg of Phoenix) (8085A CPU)\0", NULL, "bootleg (Jeutel)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, vautourRomInfo, vautourRomName, NULL, NULL, PhoenixInputInfo, PhoenixtDIPInfo,
	PhoenixInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	208, 256, 3, 4
};


// Vautour (bootleg of Phoenix) (Z80 CPU, single PROM)

static struct BurnRomInfo vautourzaRomDesc[] = {
	{ "1.e1",			0x0800, 0xcd2807ee, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "2.f1",			0x0800, 0x3699b11a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.h1",			0x0800, 0xcbbb8839, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.j1",			0x0800, 0x106262eb, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "5.k1",			0x0800, 0x1a1ce0d0, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "6.h1",			0x0800, 0x1fcac707, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "7.m1",			0x0800, 0x805ec2e8, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "8.n1",			0x0800, 0x1edebb45, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "10.h2",			0x0800, 0x3c7e623f, 2 | BRF_GRA },           //  8 Background Tiles
	{ "9.j2",			0x0800, 0x59916d3b, 2 | BRF_GRA },           //  9

	{ "12.h4",			0x0800, 0x8eff75c9, 3 | BRF_GRA },           // 10 Foreground Tiles
	{ "11.j4",			0x0800, 0x369e7476, 3 | BRF_GRA },           // 11

	{ "82s135.m9",		0x0100, 0xc68a49bc, 4 | BRF_GRA },           // 12 Color Proms
};

STD_ROM_PICK(vautourza)
STD_ROM_FN(vautourza)

struct BurnDriver BurnDrvVautourza = {
	"vautourza", "phoenix", NULL, NULL, "1980",
	"Vautour (bootleg of Phoenix) (Z80 CPU, single PROM)\0", NULL, "bootleg (Jeutel)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, vautourzaRomInfo, vautourzaRomName, NULL, NULL, PhoenixInputInfo, PhoenixtDIPInfo,
	PhoenixInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	208, 256, 3, 4
};


// Falcon (bootleg of Phoenix) (Z80 CPU)

static struct BurnRomInfo falconzRomDesc[] = {
	{ "f45.bin",		0x0800, 0x9158b43b, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "f46.bin",		0x0800, 0x22ddb600, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "f47.bin",		0x0800, 0xcb2838d9, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "f48.bin",		0x0800, 0x552cf57a, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "f49.bin",		0x0800, 0x1ff3a982, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "f50.bin",		0x0800, 0x8c83bff7, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "f51.bin",		0x0800, 0x805ec2e8, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "f52.bin",		0x0800, 0x33f3af63, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "ic23.3d",		0x0800, 0x3c7e623f, 2 | BRF_GRA },           //  8 Background Tiles
	{ "ic24.4d",		0x0800, 0x59916d3b, 2 | BRF_GRA },           //  9

	{ "f39.bin",		0x0800, 0x53c52eb0, 3 | BRF_GRA },           // 10 Foreground Tiles
	{ "f40.bin",		0x0800, 0xeba42f0f, 3 | BRF_GRA },           // 11

	{ "mmi6301.ic40",	0x0100, 0x79350b25, 4 | BRF_GRA },           // 12 Color Proms
	{ "mmi6301.ic41",	0x0100, 0xe176b768, 4 | BRF_GRA },           // 13
};

STD_ROM_PICK(falconz)
STD_ROM_FN(falconz)

struct BurnDriver BurnDrvFalconz = {
	"falconz", "phoenix", NULL, NULL, "1980",
	"Falcon (bootleg of Phoenix) (Z80 CPU)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, falconzRomInfo, falconzRomName, NULL, NULL, PhoenixInputInfo, FalconzDIPInfo,
	PhoenixInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	208, 256, 3, 4
};


// Vautour (bootleg of Phoenix) (Z80 CPU)

static struct BurnRomInfo vautourzRomDesc[] = {
	{ "vautour1.bin",	0x0800, 0xa600f6a4, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "vautour2.bin",	0x0800, 0x3699b11a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "vautour3.bin",	0x0800, 0x750b059b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "vautour4.bin",	0x0800, 0x01a4bfde, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "vautour5.bin",	0x0800, 0x1ff3a982, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "vautour6.bin",	0x0800, 0x8c83bff7, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "vautour7.bin",	0x0800, 0x805ec2e8, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "vautour8.bin",	0x0800, 0x1edebb45, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "ic23.3d",		0x0800, 0x3c7e623f, 2 | BRF_GRA },           //  8 Background Tiles
	{ "ic24.4d",		0x0800, 0x59916d3b, 2 | BRF_GRA },           //  9

	{ "vautor12.2h",	0x0800, 0x8eff75c9, 3 | BRF_GRA },           // 10 Foreground Tiles
	{ "vautor11.2j",	0x0800, 0x369e7476, 3 | BRF_GRA },           // 11

	{ "mmi6301.ic40",	0x0100, 0x79350b25, 4 | BRF_GRA },           // 12 Color Proms
	{ "mmi6301.ic41",	0x0100, 0xe176b768, 4 | BRF_GRA },           // 13
};

STD_ROM_PICK(vautourz)
STD_ROM_FN(vautourz)

struct BurnDriver BurnDrvVautourz = {
	"vautourz", "phoenix", NULL, NULL, "1980",
	"Vautour (bootleg of Phoenix) (Z80 CPU)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, vautourzRomInfo, vautourzRomName, NULL, NULL, CondorInputInfo, CondorDIPInfo,
	CondorInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	208, 256, 3, 4
};


// Fenix (bootleg of Phoenix)
// verified single PCB, single PROM

static struct BurnRomInfo fenixRomDesc[] = {
	{ "0.1e",			0x0800, 0x00000000, 1 | BRF_NODUMP | BRF_PRG | BRF_ESS }, //  0 i8085 Code
	{ "1.1f",			0x0800, 0x3699b11a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "2.1h",			0x0800, 0x750b059b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "3.1j",			0x0800, 0x61b8a41b, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "4.1k",			0x0800, 0x1ff3a982, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "5.1l",			0x0800, 0xa210fe51, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "6.1m",			0x0800, 0x805ec2e8, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "7.1n",			0x0800, 0x1edebb45, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "9.2h",			0x0800, 0x3c7e623f, 2 | BRF_GRA },           //  8 Background Tiles
	{ "8.2j",			0x0800, 0x59916d3b, 2 | BRF_GRA },           //  9

	{ "11.3h",			0x0800, 0x8eff75c9, 3 | BRF_GRA },           // 10 Foreground Tiles
	{ "10.3j",			0x0800, 0x369e7476, 3 | BRF_GRA },           // 11

	{ "mmi6301.ic40",	0x0100, 0x79350b25, 4 | BRF_GRA },           // 12 Color Proms
	{ "mmi6301.ic41",	0x0100, 0xe176b768, 4 | BRF_GRA },           // 13
};

STD_ROM_PICK(fenix)
STD_ROM_FN(fenix)

struct BurnDriver BurnDrvFenix = {
	"fenix", "phoenix", NULL, NULL, "1980",
	"Fenix (bootleg of Phoenix)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, fenixRomInfo, fenixRomName, NULL, NULL, CondorInputInfo, CondorDIPInfo,
	CondorInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	208, 256, 3, 4
};


// Ave Fenix (Electrogame, Spanish bootleg of Phoenix)

static struct BurnRomInfo avefenixRomDesc[] = {
	{ "4101-8516.rom",	0x0800, 0x5bc2e2fe, 1 | BRF_PRG | BRF_ESS }, //  0 i8085 Code
	{ "4102-2716.rom",	0x0800, 0xdcf2cc3e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "4103-8516.rom",	0x0800, 0xcbbb8839, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4104-8516.rom",	0x0800, 0x8380a581, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "4105-8516.rom",	0x0800, 0xcfa8cb51, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "4106-8516.rom",	0x0800, 0xac5e9ec1, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "4107-8516.rom",	0x0800, 0x2eab35b4, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "4108-8516.rom",	0x0800, 0xf15c439d, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "41011-8516.rom",	0x0800, 0x3c7e623f, 2 | BRF_GRA },           //  8 Background Tiles
	{ "41012-8516.rom",	0x0800, 0x59916d3b, 2 | BRF_GRA },           //  9

	{ "4109-8516.rom",	0x0800, 0x53413e8f, 3 | BRF_GRA },           // 10 Foreground Tiles
	{ "41010-8516.rom",	0x0800, 0x0be2ba91, 3 | BRF_GRA },           // 11

	{ "mmi6301.ic40",	0x0100, 0x79350b25, 4 | BRF_GRA },           // 12 Color Proms
	{ "mmi6301.ic41",	0x0100, 0xe176b768, 4 | BRF_GRA },           // 13
};

STD_ROM_PICK(avefenix)
STD_ROM_FN(avefenix)

struct BurnDriver BurnDrvAvefenix = {
	"avefenix", "phoenix", NULL, NULL, "1980",
	"Ave Fenix (Electrogame, Spanish bootleg of Phoenix)\0", NULL, "bootleg (Video Game)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, avefenixRomInfo, avefenixRomName, NULL, NULL, PhoenixInputInfo, PhoenixDIPInfo,
	PhoenixInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	208, 256, 3, 4
};


// Ave Fenix (Recreativos Franco, Spanish bootleg of Phoenix)

static struct BurnRomInfo avefenixrfRomDesc[] = {
	{ "601-ic45.a1",	0x0800, 0xb04260e9, 1 | BRF_PRG | BRF_ESS }, //  0 i8085 Code
	{ "6002-ic46.a2",	0x0800, 0x25a2e4bd, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "0003-ic47.a3",	0x0800, 0xcbbb8839, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "6004-ic48.a4",	0x0800, 0x4b7701b4, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "6005-ic49.a5",	0x0800, 0x1ab92ef9, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "0006-ic50.a6",	0x0800, 0xac5e9ec1, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "6007-ic51.a7",	0x0800, 0x2eab35b4, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "f008-ic52.a8",	0x0800, 0x3719fc84, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "0011-ic23.d3",	0x0800, 0x3c7e623f, 2 | BRF_GRA },           //  8 Background Tiles
	{ "0012-ic24.d4",	0x0800, 0x59916d3b, 2 | BRF_GRA },           //  9

	{ "0009-ic39.b3",	0x0800, 0xbb0525ed, 3 | BRF_GRA },           // 10 Foreground Tiles
	{ "0010-ic40.b4",	0x0800, 0x4178aa4f, 3 | BRF_GRA },           // 11

	{ "mmi6301.ic40",	0x0100, 0x79350b25, 4 | BRF_GRA },           // 12 Color Proms
	{ "mmi6301.ic41",	0x0100, 0xe176b768, 4 | BRF_GRA },           // 13
};

STD_ROM_PICK(avefenixrf)
STD_ROM_FN(avefenixrf)

struct BurnDriver BurnDrvAvefenixrf = {
	"avefenixrf", "phoenix", NULL, NULL, "1981",
	"Ave Fenix (Recreativos Franco, Spanish bootleg of Phoenix)\0", NULL, "bootleg (Recreativos Franco S.A.)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, avefenixrfRomInfo, avefenixrfRomName, NULL, NULL, PhoenixInputInfo, PhoenixDIPInfo,
	PhoenixInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	208, 256, 3, 4
};


// Ave Fenix (Laguna, Spanish bootleg of Phoenix)

static struct BurnRomInfo avefenixlRomDesc[] = {
	{ "01_ic45.a1",		0x0800, 0x2c53998c, 1 | BRF_PRG | BRF_ESS }, //  0 i8085 Code
	{ "02_ic46.a2",		0x0800, 0xfea2435c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "03_ic47.a3",		0x0800, 0xcbbb8839, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "04_ic48.a4",		0x0800, 0x90a02a45, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "05_ic49.a5",		0x0800, 0x74b1cf66, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "06_ic50.a6",		0x0800, 0xac5e9ec1, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "07_ic51.a7",		0x0800, 0x2eab35b4, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "08_ic52.a8",		0x0800, 0xf15c439d, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "11_ic23.d3",		0x0800, 0x3c7e623f, 2 | BRF_GRA },           //  8 Background Tiles
	{ "12_ic24.d4",		0x0800, 0x59916d3b, 2 | BRF_GRA },           //  9

	{ "09_ic39.b3",		0x0800, 0xbb0525ed, 3 | BRF_GRA },           // 10 Foreground Tiles
	{ "10_ic40.b4",		0x0800, 0x4178aa4f, 3 | BRF_GRA },           // 11

	{ "mmi6301.ic40",	0x0100, 0x79350b25, 4 | BRF_GRA },           // 12 Color Proms
	{ "mmi6301.ic41",	0x0100, 0xe176b768, 4 | BRF_GRA },           // 13
};

STD_ROM_PICK(avefenixl)
STD_ROM_FN(avefenixl)

struct BurnDriver BurnDrvAvefenixl = {
	"avefenixl", "phoenix", NULL, NULL, "1980",
	"Ave Fenix (Laguna, Spanish bootleg of Phoenix)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, avefenixlRomInfo, avefenixlRomName, NULL, NULL, PhoenixInputInfo, PhoenixDIPInfo,
	PhoenixInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	208, 256, 3, 4
};


// Griffon (bootleg of Phoenix)
// verified single PCB, single PROM

static struct BurnRomInfo griffonRomDesc[] = {
	{ "griffon0.a5",	0x0800, 0xc0f73929, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "griffon1.a6",	0x0800, 0x3cc33e4a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "griffon2.a7",	0x0800, 0x750b059b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "griffon3.a8",	0x0800, 0x5e49f5b5, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "griffon4.a9",	0x0800, 0x87a45ceb, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "griffon5.a10",	0x0800, 0x8c83bff7, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "griffon6.a11",	0x0800, 0x805ec2e8, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "griffon7.a12",	0x0800, 0x55e68cb1, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "ic23.3d",		0x0800, 0x3c7e623f, 2 | BRF_GRA },           //  8 Background Tiles
	{ "ic24.4d",		0x0800, 0x59916d3b, 2 | BRF_GRA },           //  9

	{ "griffon.d7",		0x0800, 0x53c52eb0, 3 | BRF_GRA },           // 10 Foreground Tiles
	{ "griffon.d8",		0x0800, 0xeba42f0f, 3 | BRF_GRA },           // 11

	{ "sn74s471n.bin",	0x0100, 0xc68a49bc, 4 | BRF_GRA },           // 12 Color Proms
};

STD_ROM_PICK(griffon)
STD_ROM_FN(griffon)

struct BurnDriver BurnDrvGriffon = {
	"griffon", "phoenix", NULL, NULL, "1980",
	"Griffon (bootleg of Phoenix)\0", NULL, "bootleg (Videotron)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, griffonRomInfo, griffonRomName, NULL, NULL, CondorInputInfo, CondorDIPInfo,
	SinglePromInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	208, 256, 3, 4
};


// Next Fase (bootleg of Phoenix)

static struct BurnRomInfo nextfaseRomDesc[] = {
	{ "nf01.bin",		0x0800, 0xb31ce820, 1 | BRF_PRG | BRF_ESS }, //  0 i8085 Code
	{ "nf02.bin",		0x0800, 0x891d21e1, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "nf03.bin",		0x0800, 0x2ab7389d, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "nf04.bin",		0x0800, 0x590d3c36, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "nf05.bin",		0x0800, 0x3527f247, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "nf06.bin",		0x0800, 0xac5e9ec1, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "nf07.bin",		0x0800, 0x2eab35b4, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "nf08.bin",		0x0800, 0x04c2323f, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "nf11.bin",		0x0800, 0x3c7e623f, 2 | BRF_GRA },           //  8 Background Tiles
	{ "nf12.bin",		0x0800, 0x59916d3b, 2 | BRF_GRA },           //  9

	{ "nf09.bin",		0x0800, 0xbacbfa88, 3 | BRF_GRA },           // 10 Foreground Tiles
	{ "nf10.bin",		0x0800, 0x3143a9ee, 3 | BRF_GRA },           // 11

	{ "mmi6301.ic40",	0x0100, 0x79350b25, 4 | BRF_GRA },           // 12 Color Proms
	{ "mmi6301.ic41",	0x0100, 0xe176b768, 4 | BRF_GRA },           // 13
};

STD_ROM_PICK(nextfase)
STD_ROM_FN(nextfase)

struct BurnDriver BurnDrvNextfase = {
	"nextfase", "phoenix", NULL, NULL, "1980",
	"Next Fase (bootleg of Phoenix)\0", NULL, "bootleg (Petaco S.A.)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, nextfaseRomInfo, nextfaseRomName, NULL, NULL, PhoenixInputInfo, NextfaseDIPInfo,
	PhoenixInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	208, 256, 3, 4
};



// Phoenix (Sonic, Spanish bootleg)

static struct BurnRomInfo phoenixsRomDesc[] = {
	{ "ic45.1_a1",		0x0800, 0x5b8c55a8, 1 | BRF_PRG | BRF_ESS }, //  0 i8085 Code
	{ "ic46.2_a2",		0x0800, 0xdbc942fa, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ic47.3_a3",		0x0800, 0xcbbb8839, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ic48.4_a4",		0x0800, 0x25c8b83f, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ic49.5_a5",		0x0800, 0x1a1ce0d0, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "ic50.6_a6",		0x0800, 0xac5e9ec1, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "ic51.7_a7",		0x0800, 0x2eab35b4, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "ic52.8_a8",		0x0800, 0x3657f69b, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "ic23.11_d3",		0x0800, 0x3c7e623f, 2 | BRF_GRA },           //  8 Background Tiles
	{ "ic24.12_d4",		0x0800, 0x59916d3b, 2 | BRF_GRA },           //  9

	{ "ic39.9_b3",		0x0800, 0x14ccdf63, 3 | BRF_GRA },           // 10 Foreground Tiles
	{ "ic40.10_b4",		0x0800, 0xeba42f0f, 3 | BRF_GRA },           // 11

	{ "mmi6301.ic40",	0x0100, 0x79350b25, 4 | BRF_GRA },           // 12 Color Proms
	{ "mmi6301.ic41",	0x0100, 0xe176b768, 4 | BRF_GRA },           // 13
};

STD_ROM_PICK(phoenixs)
STD_ROM_FN(phoenixs)

struct BurnDriver BurnDrvPhoenixs = {
	"phoenixs", "phoenix", NULL, NULL, "1981",
	"Phoenix (Sonic, Spanish bootleg)\0", NULL, "bootleg (Sonic)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, phoenixsRomInfo, phoenixsRomName, NULL, NULL, PhoenixInputInfo, PhoenixDIPInfo,
	PhoenixInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	208, 256, 3, 4
};


// Phoenix (Assa, Spanish bootleg)

static struct BurnRomInfo phoenixassRomDesc[] = {
	{ "ic45.bin",		0x0800, 0x5b8c55a8, 1 | BRF_PRG | BRF_ESS }, //  0 i8085 Code
	{ "ic46.bin",		0x0800, 0xdbc942fa, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ic47.bin",		0x0800, 0xcbbb8839, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ic48.bin",		0x0800, 0x1e2e2fc7, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ic49.bin",		0x0800, 0x1a1ce0d0, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "ic50.bin",		0x0800, 0xac5e9ec1, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "ic51.bin",		0x0800, 0x2eab35b4, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "ic52.bin",		0x0800, 0x15a02d87, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "ic23.bin",		0x0800, 0x3c7e623f, 2 | BRF_GRA },           //  8 Background Tiles
	{ "ic24.bin",		0x0800, 0x59916d3b, 2 | BRF_GRA },           //  9

	{ "ic39.bin",		0x0800, 0xbb0525ed, 3 | BRF_GRA },           // 10 Foreground Tiles
	{ "ic40.bin",		0x0800, 0x4178aa4f, 3 | BRF_GRA },           // 11

	{ "prom.41",		0x0100, 0x7c9f2e00, 4 | BRF_GRA },           // 12 Color Proms
	{ "prom.40",		0x0100, 0xe176b768, 4 | BRF_GRA },           // 13
};

STD_ROM_PICK(phoenixass)
STD_ROM_FN(phoenixass)

struct BurnDriver BurnDrvPhoenixass = {
	"phoenixass", "phoenix", NULL, NULL, "1981",
	"Phoenix (Assa, Spanish bootleg)\0", NULL, "bootleg (Assa)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, phoenixassRomInfo, phoenixassRomName, NULL, NULL, PhoenixInputInfo, PhoenixDIPInfo,
	PhoenixInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	208, 256, 3, 4
};


// Pleiads (Tehkan)

static struct BurnRomInfo pleiadsRomDesc[] = {
	{ "ic47.r1",		0x0800, 0x960212c8, 1 | BRF_PRG | BRF_ESS }, //  0 i8085 Code
	{ "ic48.r2",		0x0800, 0xb254217c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ic47.bin",		0x0800, 0x87e700bb, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ic48.bin",		0x0800, 0x2d5198d0, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ic51.r5",		0x0800, 0x49c629bc, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "ic50.bin",		0x0800, 0xf1a8a00d, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "ic53.r7",		0x0800, 0xb5f07fbc, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "ic52.bin",		0x0800, 0xb1b5a8a6, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "ic23.bin",		0x0800, 0x4e30f9e7, 2 | BRF_GRA },           //  8 Background Tiles
	{ "ic24.bin",		0x0800, 0x5188fc29, 2 | BRF_GRA },           //  9

	{ "ic39.bin",		0x0800, 0x85866607, 3 | BRF_GRA },           // 10 Foreground Tiles
	{ "ic40.bin",		0x0800, 0xa841d511, 3 | BRF_GRA },           // 11

	{ "7611-5.33",		0x0100, 0xe38eeb83, 4 | BRF_GRA },           // 12 Color Proms
	{ "7611-5.26",		0x0100, 0x7a1bcb1e, 4 | BRF_GRA },           // 13
};

STD_ROM_PICK(pleiads)
STD_ROM_FN(pleiads)

struct BurnDriver BurnDrvPleiads = {
	"pleiads", NULL, NULL, NULL, "1981",
	"Pleiads (Tehkan)\0", NULL, "Tehkan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, pleiadsRomInfo, pleiadsRomName, NULL, NULL, PleiadsInputInfo, PleiadsDIPInfo,
	PleiadsInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	208, 256, 3, 4
};


// Pleiads (bootleg set 2)

static struct BurnRomInfo pleiadsb2RomDesc[] = {
	{ "ic47.r1",		0x0800, 0xfa98cb73, 1 | BRF_PRG | BRF_ESS }, //  0 i8085 Code
	{ "ic48.r2",		0x0800, 0xb254217c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ic47.bin",		0x0800, 0x0951829e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ic48.bin",		0x0800, 0x4972f5ce, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ic51.r5",		0x0800, 0x49c629bc, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "ic50.bin",		0x0800, 0xf1a8a00d, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "ic53.r7",		0x0800, 0x037b319c, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "ic52.bin",		0x0800, 0xb3db08c2, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "ic23.bin",		0x0800, 0x4e30f9e7, 2 | BRF_GRA },           //  8 Background Tiles
	{ "ic24.bin",		0x0800, 0x5188fc29, 2 | BRF_GRA },           //  9

	{ "ic39.bin",		0x0800, 0x85866607, 3 | BRF_GRA },           // 10 Foreground Tiles
	{ "ic40.bin",		0x0800, 0xa841d511, 3 | BRF_GRA },           // 11

	{ "7611-5.26",		0x0100, 0x7a1bcb1e, 4 | BRF_GRA },           // 12 Color Proms
	{ "7611-5.33",		0x0100, 0xe38eeb83, 4 | BRF_GRA },           // 13
};

STD_ROM_PICK(pleiadsb2)
STD_ROM_FN(pleiadsb2)

struct BurnDriver BurnDrvPleiadsb2 = {
	"pleiadsb2", "pleiads", NULL, NULL, "1981",
	"Pleiads (bootleg set 2)\0", NULL, "bootleg (ESG)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, pleiadsb2RomInfo, pleiadsb2RomName, NULL, NULL, PleiadsInputInfo, PleiadsDIPInfo,
	PleiadsInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	208, 256, 3, 4
};


// Pleiads (bootleg set 1)

static struct BurnRomInfo pleiadblRomDesc[] = {
	{ "ic45.bin",		0x0800, 0x93fc2958, 1 | BRF_PRG | BRF_ESS }, //  0 i8085 Code
	{ "ic46.bin",		0x0800, 0xe2b5b8cd, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ic47.bin",		0x0800, 0x87e700bb, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ic48.bin",		0x0800, 0x2d5198d0, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ic49.bin",		0x0800, 0x9dc73e63, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "ic50.bin",		0x0800, 0xf1a8a00d, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "ic51.bin",		0x0800, 0x6f56f317, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "ic52.bin",		0x0800, 0xb1b5a8a6, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "ic23.bin",		0x0800, 0x4e30f9e7, 2 | BRF_GRA },           //  8 Background Tiles
	{ "ic24.bin",		0x0800, 0x5188fc29, 2 | BRF_GRA },           //  9

	{ "ic39.bin",		0x0800, 0x85866607, 3 | BRF_GRA },           // 10 Foreground Tiles
	{ "ic40.bin",		0x0800, 0xa841d511, 3 | BRF_GRA },           // 11

	{ "7611-5.33",		0x0100, 0xe38eeb83, 4 | BRF_GRA },           // 12 Color Proms
	{ "7611-5.26",		0x0100, 0x7a1bcb1e, 4 | BRF_GRA },           // 13
};

STD_ROM_PICK(pleiadbl)
STD_ROM_FN(pleiadbl)

struct BurnDriver BurnDrvPleiadbl = {
	"pleiadbl", "pleiads", NULL, NULL, "1981",
	"Pleiads (bootleg set 1)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, pleiadblRomInfo, pleiadblRomName, NULL, NULL, PhoenixInputInfo, PleiadblDIPInfo,
	PleiadsInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	208, 256, 3, 4
};


// Pleiads (Centuri)

static struct BurnRomInfo pleiadceRomDesc[] = {
	{ "pleiades.47",	0x0800, 0x711e2ba0, 1 | BRF_PRG | BRF_ESS }, //  0 i8085 Code
	{ "pleiades.48",	0x0800, 0x93a36943, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ic47.bin",		0x0800, 0x87e700bb, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "pleiades.50",	0x0800, 0x5a9beba0, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "pleiades.51",	0x0800, 0x1d828719, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "ic50.bin",		0x0800, 0xf1a8a00d, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "pleiades.53",	0x0800, 0x037b319c, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "pleiades.54",	0x0800, 0xca264c7c, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "pleiades.45",	0x0800, 0x8dbd3785, 2 | BRF_GRA },           //  8 Background Tiles
	{ "pleiades.44",	0x0800, 0x0db3e436, 2 | BRF_GRA },           //  9

	{ "ic39.bin",		0x0800, 0x85866607, 3 | BRF_GRA },           // 10 Foreground Tiles
	{ "ic40.bin",		0x0800, 0xa841d511, 3 | BRF_GRA },           // 11

	{ "7611-5.33",		0x0100, 0xe38eeb83, 4 | BRF_GRA },           // 12 Color Proms
	{ "7611-5.26",		0x0100, 0x7a1bcb1e, 4 | BRF_GRA },           // 13
};

STD_ROM_PICK(pleiadce)
STD_ROM_FN(pleiadce)

struct BurnDriver BurnDrvPleiadce = {
	"pleiadce", "pleiads", NULL, NULL, "1981",
	"Pleiads (Centuri)\0", NULL, "Tehkan (Centuri license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, pleiadceRomInfo, pleiadceRomName, NULL, NULL, PhoenixInputInfo, PleiadceDIPInfo,
	PleiadsInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	208, 256, 3, 4
};


// Pleiads (Irecsa)

static struct BurnRomInfo pleiadsiRomDesc[] = {
	{ "1 2716.bin",		0x0800, 0x9bbef607, 1 | BRF_PRG | BRF_ESS }, //  0 i8085 Code
	{ "2 2716.bin",		0x0800, 0xe2b5b8cd, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3 2716.bin",		0x0800, 0x87e700bb, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4 2716.bin",		0x0800, 0xca14fe4a, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "5 2716.bin",		0x0800, 0x9dc73e63, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "6 2716.bin",		0x0800, 0xf1a8a00d, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "7 2716.bin",		0x0800, 0x6f56f317, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "8 2716.bin",		0x0800, 0xca264c7c, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "11 2716.bin",	0x0800, 0x8dbd3785, 2 | BRF_GRA },           //  8 Background Tiles
	{ "12 2716.bin",	0x0800, 0x0db3e436, 2 | BRF_GRA },           //  9

	{ "9 2716.bin",		0x0800, 0x85866607, 3 | BRF_GRA },           // 10 Foreground Tiles
	{ "10 2716.bin",	0x0800, 0xa841d511, 3 | BRF_GRA },           // 11

	{ "7611-5.26",		0x0100, 0x7a1bcb1e, 4 | BRF_GRA },           // 12 Color Proms
	{ "7611-5.33",		0x0100, 0xe38eeb83, 4 | BRF_GRA },           // 13
};

STD_ROM_PICK(pleiadsi)
STD_ROM_FN(pleiadsi)

struct BurnDriver BurnDrvPleiadsi = {
	"pleiadsi", "pleiads", NULL, NULL, "1981",
	"Pleiads (Irecsa)\0", NULL, "bootleg? (Irecsa)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, pleiadsiRomInfo, pleiadsiRomName, NULL, NULL, PhoenixInputInfo, PleiadceDIPInfo,
	PleiadsInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	208, 256, 3, 4
};


// Pleiads (Niemer S.A.)

static struct BurnRomInfo pleiadsnRomDesc[] = {
	{ "1.bin",			0x0800, 0xc013515f, 1 | BRF_PRG | BRF_ESS }, //  0 i8085 Code
	{ "2.bin",			0x0800, 0xb254217c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.bin",			0x0800, 0x3b29aec5, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.bin",			0x0800, 0x1fbde4d7, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "5.bin",			0x0800, 0x9dc73e63, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "6.bin",			0x0800, 0xf1a8a00d, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "7.bin",			0x0800, 0xb5f07fbc, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "8.bin",			0x0800, 0xb3db08c2, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "11.bin",			0x0800, 0x4e30f9e7, 2 | BRF_GRA },           //  8 Background Tiles
	{ "12.bin",			0x0800, 0x72d511fc, 2 | BRF_GRA },           //  9

	{ "9.bin",			0x0800, 0x85866607, 3 | BRF_GRA },           // 10 Foreground Tiles
	{ "10.bin",			0x0800, 0xa841d511, 3 | BRF_GRA },           // 11

	{ "hm3-7611.bin",	0x0100, 0xe38eeb83, 4 | BRF_GRA },           // 12 Color Proms
	{ "mb7052.ic41",	0x0100, 0x7a1bcb1e, 4 | BRF_GRA },           // 13
};

STD_ROM_PICK(pleiadsn)
STD_ROM_FN(pleiadsn)

struct BurnDriver BurnDrvPleiadsn = {
	"pleiadsn", "pleiads", NULL, NULL, "1981",
	"Pleiads (Niemer S.A.)\0", NULL, "Niemer S.A.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, pleiadsnRomInfo, pleiadsnRomName, NULL, NULL, PhoenixInputInfo, PleiadceDIPInfo,
	PleiadsInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	208, 256, 3, 4
};


// Pleiads (Spanish bootleg)

static struct BurnRomInfo pleiadssRomDesc[] = {
	{ "pl45.bin",		0x0800, 0xe2528599, 1 | BRF_PRG | BRF_ESS }, //  0 i8085 Code
	{ "pl46.bin",		0x0800, 0xb254217c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "pl47.bin",		0x0800, 0x3b29aec5, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "pl48.bin",		0x0800, 0xe74ccdeb, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "pl49.bin",		0x0800, 0x24f9c3e8, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "pl50.bin",		0x0800, 0xf1a8a00d, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "pl51.bin",		0x0800, 0xb5f07fbc, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "pl52.bin",		0x0800, 0xb3db08c2, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "pl24.bin",		0x0800, 0x5188fc29, 2 | BRF_GRA },           //  8 Background Tiles
	{ "pl23.bin",		0x0800, 0x4e30f9e7, 2 | BRF_GRA },           //  9

	{ "pl39.bin",		0x0800, 0x71635678, 3 | BRF_GRA },           // 10 Foreground Tiles
	{ "pl40.bin",		0x0800, 0xa841d511, 3 | BRF_GRA },           // 11

	{ "ic41.prm",		0x0100, 0xe176b768, 4 | BRF_GRA },           // 12 Color Proms
	{ "ic40.prm",		0x0100, 0x79350b25, 4 | BRF_GRA },           // 13
};

STD_ROM_PICK(pleiadss)
STD_ROM_FN(pleiadss)

struct BurnDriver BurnDrvPleiadss = {
	"pleiadss", "pleiads", NULL, NULL, "1981",
	"Pleiads (Spanish bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, pleiadssRomInfo, pleiadssRomName, NULL, NULL, PhoenixInputInfo, PleiadceDIPInfo,
	PleiadsInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	208, 256, 3, 4
};


// Capitol

static struct BurnRomInfo capitolRomDesc[] = {
	{ "cp1.45",			0x0800, 0x0922905b, 1 | BRF_PRG | BRF_ESS }, //  0 i8085 Code
	{ "cp2.46",			0x0800, 0x4f168f45, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "cp3.47",			0x0800, 0x3975e0b0, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "cp4.48",			0x0800, 0xda49caa8, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "cp5.49",			0x0800, 0x38e4362b, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "cp6.50",			0x0800, 0xaaf798eb, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "cp7.51",			0x0800, 0xeaadf14c, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "cp8.52",			0x0800, 0xd3fe2af4, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "cp11.23",		0x0800, 0x9b0bbb8d, 2 | BRF_GRA },           //  8 Background Tiles
	{ "cp12.24",		0x0800, 0x39949e66, 2 | BRF_GRA },           //  9

	{ "cp9.39",			0x0800, 0x04f7d19a, 3 | BRF_GRA },           // 10 Foreground Tiles
	{ "cp10.40",		0x0800, 0x4807408f, 3 | BRF_GRA },           // 11

	{ "ic41.prm",		0x0100, 0xe176b768, 4 | BRF_GRA },           // 12 Color Proms
	{ "ic40.prm",		0x0100, 0x79350b25, 4 | BRF_GRA },           // 13
};

STD_ROM_PICK(capitol)
STD_ROM_FN(capitol)

struct BurnDriver BurnDrvCapitol = {
	"capitol", "pleiads", NULL, NULL, "1981",
	"Capitol\0", NULL, "bootleg? (Universal Video Spiel)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, capitolRomInfo, capitolRomName, NULL, NULL, PleiadsInputInfo, PleiadsDIPInfo,
	PleiadsInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	208, 256, 3, 4
};
