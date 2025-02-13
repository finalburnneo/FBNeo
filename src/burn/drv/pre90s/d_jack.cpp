// FB Alpha Jack the Giant Killer driver module
// Based on MAME driver by Brad Oliver

#include "tiles_generic.h"
#include "z80_intf.h"
#include "bitswap.h"
#include "ay8910.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80Dec;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM;
static UINT8 *DrvUsrROM;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvVidRAM;
static UINT8 *DrvColRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvScrRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvScroll;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 soundlatch;
static UINT8 flipscreen;
static UINT8 palette_bank;
static UINT8 nmi_enable;
static INT32 joinem_timer;
static UINT8 remap_address[0x10];
static UINT8 question_rom;
static UINT32 question_address;

static INT32 timer_rate;
static INT32 graphics_size;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvInputs[4];
static UINT8 DrvDips[2];
static UINT8 DrvReset;

static struct BurnInputInfo JackInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy4 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Jack)

static struct BurnInputInfo ZzyzzyxxInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 fire 1"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 down"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy4 + 0,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Zzyzzyxx)

static struct BurnInputInfo FreezeInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Freeze)

static struct BurnInputInfo SucasinoInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 fire 1"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy4 + 0,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Sucasino)

static struct BurnInputInfo TripoolInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 4"	},
	{"P1 Button 5",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 5"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy4 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Tripool)

static struct BurnInputInfo StrivInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 coin"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 4"	},

	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 4"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Striv)

static struct BurnInputInfo JoinemInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 right"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Joinem)

static struct BurnInputInfo UnclepooInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Unclepoo)

static struct BurnInputInfo LoverboyInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Loverboy)

static struct BurnDIPInfo JackDIPList[]=
{
	{0x11, 0xff, 0xff, 0x00, NULL			},
	{0x12, 0xff, 0xff, 0x01, NULL			},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x11, 0x01, 0x03, 0x01, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x03, 0x03, "4 Coins 3 Credits"	},
	{0x11, 0x01, 0x03, 0x00, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x03, 0x02, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x11, 0x01, 0x0c, 0x08, "3 Coins 1 Credits"	},
	{0x11, 0x01, 0x0c, 0x04, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x0c, 0x0c, "4 Coins 3 Credits"	},
	{0x11, 0x01, 0x0c, 0x00, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x11, 0x01, 0x10, 0x00, "3"			},
	{0x11, 0x01, 0x10, 0x10, "5"			},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x11, 0x01, 0x20, 0x00, "Every 10000"		},
	{0x11, 0x01, 0x20, 0x20, "10000 Only"		},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x11, 0x01, 0x40, 0x00, "Start on Level 1"	},
	{0x11, 0x01, 0x40, 0x40, "Start on Level 13"	},

	{0   , 0xfe, 0   ,    2, "Per Bean/Bullets"	},
	{0x11, 0x01, 0x80, 0x00, "1"			},
	{0x11, 0x01, 0x80, 0x80, "2"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x12, 0x01, 0x01, 0x01, "Upright"		},
	{0x12, 0x01, 0x01, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x12, 0x01, 0x20, 0x00, "Off"			},
	{0x12, 0x01, 0x20, 0x20, "On"			},

	{0   , 0xfe, 0   ,    2, "Invulnerability"	},
	{0x12, 0x01, 0x40, 0x00, "Off"			},
	{0x12, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "255 Lives"		},
	{0x12, 0x01, 0x80, 0x00, "Off"			},
	{0x12, 0x01, 0x80, 0x80, "On"			},
};

STDDIPINFO(Jack)

static struct BurnDIPInfo Jack2DIPList[]=
{
	{0x11, 0xff, 0xff, 0x00, NULL			},
	{0x12, 0xff, 0xff, 0x01, NULL			},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x11, 0x01, 0x03, 0x03, "4 Coins 3 Credits"	},
	{0x11, 0x01, 0x03, 0x00, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x03, 0x01, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0x03, 0x02, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x11, 0x01, 0x0c, 0x08, "3 Coins 1 Credits"	},
	{0x11, 0x01, 0x0c, 0x04, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x0c, 0x0c, "4 Coins 3 Credits"	},
	{0x11, 0x01, 0x0c, 0x00, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x11, 0x01, 0x10, 0x00, "3"			},
	{0x11, 0x01, 0x10, 0x10, "5"			},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x11, 0x01, 0x20, 0x00, "Every 10000"		},
	{0x11, 0x01, 0x20, 0x20, "10000 Only"		},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x11, 0x01, 0x40, 0x00, "Start on Level 1"	},
	{0x11, 0x01, 0x40, 0x40, "Start on Level 13"	},

	{0   , 0xfe, 0   ,    2, "Per Bean/Bullets"	},
	{0x11, 0x01, 0x80, 0x00, "1"			},
	{0x11, 0x01, 0x80, 0x80, "2"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x12, 0x01, 0x01, 0x01, "Upright"		},
	{0x12, 0x01, 0x01, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x12, 0x01, 0x20, 0x00, "Off"			},
	{0x12, 0x01, 0x20, 0x20, "On"			},

	{0   , 0xfe, 0   ,    2, "Invulnerability"	},
	{0x12, 0x01, 0x40, 0x00, "Off"			},
	{0x12, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "255 Lives"		},
	{0x12, 0x01, 0x80, 0x00, "Off"			},
	{0x12, 0x01, 0x80, 0x80, "On"			},
};

STDDIPINFO(Jack2)

static struct BurnDIPInfo Jack3DIPList[]=
{
	{0x11, 0xff, 0xff, 0x00, NULL			},
	{0x12, 0xff, 0xff, 0x01, NULL			},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x11, 0x01, 0x03, 0x00, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x03, 0x01, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0x03, 0x02, "1 Coin  3 Credits"	},
	{0x11, 0x01, 0x03, 0x03, "1 Coin  5 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x11, 0x01, 0x0c, 0x0c, "4 Coins 1 Credits"	},
	{0x11, 0x01, 0x0c, 0x08, "3 Coins 1 Credits"	},
	{0x11, 0x01, 0x0c, 0x04, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x0c, 0x00, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x11, 0x01, 0x10, 0x00, "3"			},
	{0x11, 0x01, 0x10, 0x10, "5"			},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x11, 0x01, 0x20, 0x00, "Every 10000"		},
	{0x11, 0x01, 0x20, 0x20, "10000 Only"		},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x11, 0x01, 0x40, 0x00, "Start on Level 1"	},
	{0x11, 0x01, 0x40, 0x40, "Start on Level 13"	},

	{0   , 0xfe, 0   ,    2, "Per Bean/Bullets"	},
	{0x11, 0x01, 0x80, 0x00, "1"			},
	{0x11, 0x01, 0x80, 0x80, "2"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x12, 0x01, 0x01, 0x01, "Upright"		},
	{0x12, 0x01, 0x01, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x12, 0x01, 0x20, 0x00, "Off"			},
	{0x12, 0x01, 0x20, 0x20, "On"			},

	{0   , 0xfe, 0   ,    2, "Invulnerability"	},
	{0x12, 0x01, 0x40, 0x00, "Off"			},
	{0x12, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "255 Lives"		},
	{0x12, 0x01, 0x80, 0x00, "Off"			},
	{0x12, 0x01, 0x80, 0x80, "On"			},
};

STDDIPINFO(Jack3)

static struct BurnDIPInfo TreahuntDIPList[]=
{
	{0x11, 0xff, 0xff, 0x00, NULL			},
	{0x12, 0xff, 0xff, 0x01, NULL			},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x11, 0x01, 0x03, 0x01, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x03, 0x03, "4 Coins 3 Credits"	},
	{0x11, 0x01, 0x03, 0x00, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x03, 0x02, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x11, 0x01, 0x0c, 0x08, "3 Coins 1 Credits"	},
	{0x11, 0x01, 0x0c, 0x04, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x0c, 0x0c, "4 Coins 3 Credits"	},
	{0x11, 0x01, 0x0c, 0x00, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x11, 0x01, 0x10, 0x00, "3"			},
	{0x11, 0x01, 0x10, 0x10, "5"			},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x11, 0x01, 0x20, 0x00, "Every 10000"		},
	{0x11, 0x01, 0x20, 0x20, "10000 Only"		},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x11, 0x01, 0x40, 0x00, "Start on Level 1"	},
	{0x11, 0x01, 0x40, 0x40, "Start on Level 13"	},

	{0   , 0xfe, 0   ,    2, "Per Bean/Bullets"	},
	{0x11, 0x01, 0x80, 0x00, "1"			},
	{0x11, 0x01, 0x80, 0x80, "2"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x12, 0x01, 0x01, 0x01, "Upright"		},
	{0x12, 0x01, 0x01, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x12, 0x01, 0x20, 0x00, "Off"			},
	{0x12, 0x01, 0x20, 0x20, "On"			},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x12, 0x01, 0x40, 0x00, "Start on Level 1"	},
	{0x12, 0x01, 0x40, 0x40, "Start on Level 6"	},

	{0   , 0xfe, 0   ,    2, "Per Bean/Bullets"	},
	{0x12, 0x01, 0x80, 0x00, "5"			},
	{0x12, 0x01, 0x80, 0x80, "20"			},
};

STDDIPINFO(Treahunt)

static struct BurnDIPInfo ZzyzzyxxDIPList[]=
{
	{0x0a, 0xff, 0xff, 0x20, NULL			},
	{0x0b, 0xff, 0xff, 0x44, NULL			},

	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{0x0a, 0x01, 0x03, 0x01, "2 Coins 1 Credits"	},
	{0x0a, 0x01, 0x03, 0x03, "4 Coins 3 Credits"	},
	{0x0a, 0x01, 0x03, 0x00, "1 Coin  1 Credits"	},
	{0x0a, 0x01, 0x03, 0x02, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x0a, 0x01, 0x04, 0x04, "2"			},
	{0x0a, 0x01, 0x04, 0x00, "3"			},

	{0   , 0xfe, 0   ,    2, "2 Credits on Reset"	},
	{0x0a, 0x01, 0x08, 0x00, "Off"			},
	{0x0a, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x0a, 0x01, 0x10, 0x10, "Off"			},
	{0x0a, 0x01, 0x10, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0a, 0x01, 0x20, 0x20, "Upright"		},
	{0x0a, 0x01, 0x20, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x0a, 0x01, 0x40, 0x00, "Off"			},
	{0x0a, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x0a, 0x01, 0x80, 0x00, "Off"			},
	{0x0a, 0x01, 0x80, 0x80, "On"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x0b, 0x01, 0x03, 0x02, "None"			},
	{0x0b, 0x01, 0x03, 0x00, "10000 50000"		},
	{0x0b, 0x01, 0x03, 0x01, "25000 100000"		},
	{0x0b, 0x01, 0x03, 0x03, "100000 300000"	},

	{0   , 0xfe, 0   ,    2, "2nd Bonus Given"	},
	{0x0b, 0x01, 0x04, 0x00, "No"			},
	{0x0b, 0x01, 0x04, 0x04, "Yes"			},

	{0   , 0xfe, 0   ,    2, "Starting Laps"	},
	{0x0b, 0x01, 0x08, 0x00, "2"			},
	{0x0b, 0x01, 0x08, 0x08, "3"			},

	{0   , 0xfe, 0   ,    2, "Difficulty of Lola"	},
	{0x0b, 0x01, 0x10, 0x00, "Easy"			},
	{0x0b, 0x01, 0x10, 0x10, "Hard"			},

	{0   , 0xfe, 0   ,    2, "Show Intermissions"	},
	{0x0b, 0x01, 0x20, 0x00, "No"			},
	{0x0b, 0x01, 0x20, 0x20, "Yes"			},

	{0   , 0xfe, 0   ,    3, "Extra Lives"		},
	{0x0b, 0x01, 0xc0, 0x00, "3 under 4000 pts"	},
	{0x0b, 0x01, 0xc0, 0x80, "5 under 4000 pts"	},
	{0x0b, 0x01, 0xc0, 0x40, "None"			},
};

STDDIPINFO(Zzyzzyxx)

static struct BurnDIPInfo FreezeDIPList[]=
{
	{0x07, 0xff, 0xff, 0x00, NULL			},
	{0x08, 0xff, 0xff, 0x00, NULL			},
	
	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x07, 0x01, 0x01, 0x00, "Off"			},
	{0x07, 0x01, 0x01, 0x01, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x07, 0x01, 0x02, 0x00, "Off"			},
	{0x07, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x07, 0x01, 0x04, 0x00, "Easy"			},
	{0x07, 0x01, 0x04, 0x04, "Hard"			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x07, 0x01, 0x08, 0x00, "3"			},
	{0x07, 0x01, 0x08, 0x08, "5"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x07, 0x01, 0x30, 0x00, "10000"		},
	{0x07, 0x01, 0x30, 0x10, "10000 & Every 40000"	},
	{0x07, 0x01, 0x30, 0x20, "10000 & Every 60000"	},
	{0x07, 0x01, 0x30, 0x30, "20000 & Every 100000"	},

	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{0x07, 0x01, 0xc0, 0x80, "2 Coins 1 Credits"	},
	{0x07, 0x01, 0xc0, 0x00, "1 Coin  1 Credits"	},
	{0x07, 0x01, 0xc0, 0x40, "1 Coin  2 Credits"	},
	{0x07, 0x01, 0xc0, 0xc0, "Free Play"		},
};

STDDIPINFO(Freeze)

static struct BurnDIPInfo SucasinoDIPList[]=
{
	{0x0a, 0xff, 0xff, 0x00, NULL			},
	{0x0b, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{0x0a, 0x01, 0x03, 0x00, "1 Coin  1 Credits"	},
	{0x0a, 0x01, 0x03, 0x01, "1 Coin  2 Credits"	},
	{0x0a, 0x01, 0x03, 0x02, "1 Coin  3 Credits"	},
	{0x0a, 0x01, 0x03, 0x03, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0a, 0x01, 0x04, 0x00, "Upright"		},
	{0x0a, 0x01, 0x04, 0x04, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x0a, 0x01, 0x08, 0x00, "Off"			},
	{0x0a, 0x01, 0x08, 0x08, "On"			},
};

STDDIPINFO(Sucasino)

static struct BurnDIPInfo TripoolDIPList[]=
{
	{0x14, 0xff, 0xff, 0x00, NULL			},
	{0x15, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x14, 0x01, 0x03, 0x00, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x03, 0x01, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0x03, 0x02, "1 Coin  3 Credits"	},
	{0x14, 0x01, 0x03, 0x03, "1 Coin  5 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x14, 0x01, 0x0c, 0x0c, "4 Coins 1 Credits"	},
	{0x14, 0x01, 0x0c, 0x08, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0x0c, 0x04, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x0c, 0x00, "1 Coin  1 Credits"	},
};

STDDIPINFO(Tripool)

static struct BurnDIPInfo StrivDIPList[]=
{
	{0x0a, 0xff, 0xff, 0xfd, NULL			},
	{0x0b, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    2, "Monitor"		},
	{0x0a, 0x01, 0x02, 0x02, "Horizontal"		},
	{0x0a, 0x01, 0x02, 0x00, "Vertical"		},

	{0   , 0xfe, 0   ,    8, "Gaming Option #"	},
	{0x0a, 0x01, 0x05, 0x01, "2"			},
	{0x0a, 0x01, 0x05, 0x05, "3"			},
	{0x0a, 0x01, 0x05, 0x00, "4"			},
	{0x0a, 0x01, 0x05, 0x04, "5"			},
	{0x0a, 0x01, 0x05, 0x01, "4"			},
	{0x0a, 0x01, 0x05, 0x05, "5"			},
	{0x0a, 0x01, 0x05, 0x00, "6"			},
	{0x0a, 0x01, 0x05, 0x04, "7"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0a, 0x01, 0x08, 0x08, "Upright"		},
	{0x0a, 0x01, 0x08, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Coinage"		},
	{0x0a, 0x01, 0x10, 0x00, "2 Coins 1 Credits"	},
	{0x0a, 0x01, 0x10, 0x10, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    2, "Gaming Option"	},
	{0x0a, 0x01, 0x20, 0x20, "# of Wrong Answer"	},
	{0x0a, 0x01, 0x20, 0x00, "# of Questions"	},

	{0   , 0xfe, 0   ,    2, "Show Correct Answer"	},
	{0x0a, 0x01, 0x40, 0x00, "No"			},
	{0x0a, 0x01, 0x40, 0x40, "Yes"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x0a, 0x01, 0x80, 0x00, "Off"			},
	{0x0a, 0x01, 0x80, 0x80, "On"			},
};

STDDIPINFO(Striv)

static struct BurnDIPInfo JoinemDIPList[]=
{
	{0x0d, 0xff, 0xff, 0x00, NULL			},
	{0x0e, 0xff, 0xff, 0x01, NULL			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x0d, 0x01, 0x03, 0x01, "2 Coins 1 Credits"	},
	{0x0d, 0x01, 0x03, 0x03, "4 Coins 3 Credits"	},
	{0x0d, 0x01, 0x03, 0x00, "1 Coin  1 Credits"	},
	{0x0d, 0x01, 0x03, 0x02, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x0d, 0x01, 0x0c, 0x08, "3 Coins 1 Credits"	},
	{0x0d, 0x01, 0x0c, 0x04, "2 Coins 1 Credits"	},
	{0x0d, 0x01, 0x0c, 0x0c, "4 Coins 3 Credits"	},
	{0x0d, 0x01, 0x0c, 0x00, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x0d, 0x01, 0x10, 0x00, "2"			},
	{0x0d, 0x01, 0x10, 0x10, "5"			},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x0d, 0x01, 0x20, 0x00, "Every 30000"		},
	{0x0d, 0x01, 0x20, 0x20, "30000 Only"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0e, 0x01, 0x01, 0x01, "Upright"		},
	{0x0e, 0x01, 0x01, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x0e, 0x01, 0x20, 0x00, "Off"			},
	{0x0e, 0x01, 0x20, 0x20, "On"			},

	{0   , 0xfe, 0   ,    2, "255 Lives (Cheat)"	},
	{0x0e, 0x01, 0x80, 0x00, "Off"			},
	{0x0e, 0x01, 0x80, 0x80, "On"			},
};

STDDIPINFO(Joinem)

static struct BurnDIPInfo UnclepooDIPList[]=
{
	{0x11, 0xff, 0xff, 0x00, NULL			},
	{0x12, 0xff, 0xff, 0x01, NULL			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x11, 0x01, 0x03, 0x01, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x03, 0x03, "4 Coins 3 Credits"	},
	{0x11, 0x01, 0x03, 0x00, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x03, 0x02, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x11, 0x01, 0x0c, 0x08, "3 Coins 1 Credits"	},
	{0x11, 0x01, 0x0c, 0x04, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x0c, 0x0c, "4 Coins 3 Credits"	},
	{0x11, 0x01, 0x0c, 0x00, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x11, 0x01, 0x10, 0x00, "2"			},
	{0x11, 0x01, 0x10, 0x10, "5"			},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x11, 0x01, 0x20, 0x00, "Every 30000"		},
	{0x11, 0x01, 0x20, 0x20, "30000 Only"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x12, 0x01, 0x01, 0x01, "Upright"		},
	{0x12, 0x01, 0x01, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x12, 0x01, 0x20, 0x00, "Off"			},
	{0x12, 0x01, 0x20, 0x20, "On"			},

	{0   , 0xfe, 0   ,    2, "255 Lives (Cheat)"	},
	{0x12, 0x01, 0x80, 0x00, "Off"			},
	{0x12, 0x01, 0x80, 0x80, "On"			},
};

STDDIPINFO(Unclepoo)

static struct BurnDIPInfo LoverboyDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x80, NULL			},
	{0x10, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    16, "Coinage"		},
	{0x0f, 0x01, 0x0f, 0x0c, "4 Coins 1 Credits"	},
	{0x0f, 0x01, 0x0f, 0x08, "3 Coins 1 Credits"	},
	{0x0f, 0x01, 0x0f, 0x0d, "4 Coins 2 Credits"	},
	{0x0f, 0x01, 0x0f, 0x04, "2 Coins 1 Credits"	},
	{0x0f, 0x01, 0x0f, 0x09, "3 Coins 2 Credits"	},
	{0x0f, 0x01, 0x0f, 0x0e, "4 Coins 3 Credits"	},
	{0x0f, 0x01, 0x0f, 0x0f, "4 Coins 4 Credits"	},
	{0x0f, 0x01, 0x0f, 0x0a, "3 Coins 3 Credits"	},
	{0x0f, 0x01, 0x0f, 0x05, "2 Coins 2 Credits"	},
	{0x0f, 0x01, 0x0f, 0x00, "1 Coin  1 Credits"	},
	{0x0f, 0x01, 0x0f, 0x0b, "3 Coins 4 Credits"	},
	{0x0f, 0x01, 0x0f, 0x06, "2 Coins 3 Credits"	},
	{0x0f, 0x01, 0x0f, 0x07, "2 Coins 4 Credits"	},
	{0x0f, 0x01, 0x0f, 0x01, "1 Coin  2 Credits"	},
	{0x0f, 0x01, 0x0f, 0x02, "1 Coin  3 Credits"	},
	{0x0f, 0x01, 0x0f, 0x03, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x0f, 0x01, 0x20, 0x00, "20000"		},
	{0x0f, 0x01, 0x20, 0x20, "30000"		},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x0f, 0x01, 0x40, 0x00, "3"			},
	{0x0f, 0x01, 0x40, 0x40, "5"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0f, 0x01, 0x80, 0x80, "Upright"		},
	{0x0f, 0x01, 0x80, 0x00, "Cocktail"		},
};

STDDIPINFO(Loverboy)

static UINT8 __fastcall striv_question_read(UINT16 offset)
{
	if ((offset & 0xc00) == 0x800)
	{
		remap_address[offset & 0x0f] = (offset & 0xf0) >> 4;
	}
	else if ((offset & 0xc00) == 0xc00)
	{
		question_rom = offset & 7;
		question_address = (offset & 0xf8) << 7;
	}
	else
	{
		UINT32 real_address = question_address | (offset & 0x3f0) | remap_address[offset & 0x0f];

		if(offset & 0x400)
			real_address |= 0x8000 * (question_rom + 8);
		else
			real_address |= 0x8000 * question_rom;

		return DrvUsrROM[real_address];
	}

	return 0;
}

static void __fastcall jack_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xff00) == 0xb000) {
		DrvSprRAM[address & 0xff] = data;
		if ((address & 0x83) == 0x80) DrvScroll[(address / 4) & 0x1f] = data;
		return;
	}

	switch (address)
	{
		case 0xb400:
			soundlatch = data;
			ZetSetIRQLine(1, 0, CPU_IRQSTATUS_HOLD);
		return;

		case 0xb506:
		case 0xb507:
			flipscreen = address & 1;
		return;

		case 0xb700:
			palette_bank = (data & ((BurnDrvGetPaletteEntries() - 1) >> 3)) & 0x18;
			nmi_enable = data & 0x20;
			flipscreen = data & 0x80;
		return;
	}
}

static UINT8 __fastcall jack_main_read(UINT16 address)
{
	if ((address & 0xf000) == 0xc000) {
		return striv_question_read(address & 0xfff);
	}

	switch (address)
	{
		case 0xb500:
		case 0xb501:
			return DrvDips[address & 1];

		case 0xb502:
		case 0xb503:
		case 0xb504:
		case 0xb505:
			return DrvInputs[address - 0xb502];

		case 0xb506:
		case 0xb507:
			flipscreen = address & 1;
			return 0;
	}

	return 0;
}

static void __fastcall jack_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x40:
			AY8910Write(0, 1, data);
		return;

		case 0x80:
			AY8910Write(0, 0, data);
		return;
	}
}

static UINT8 __fastcall jack_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x40:
			return AY8910Read(0);
	}

	return 0;
}

static tilemap_callback( background )
{
	INT32 attr = DrvColRAM[offs];
	INT32 code = DrvVidRAM[offs] + ((attr & 0x18) * 0x20);

	TILE_SET_INFO(0, code, attr & 0x07, 0);
}

static tilemap_callback( joinem )
{
	INT32 attr = DrvColRAM[offs];
	INT32 code = DrvVidRAM[offs] + ((attr & 0x03) * 0x100);
	INT32 color = ((attr & 0x38) >> 3) + palette_bank;

	TILE_SET_INFO(0, code, color << 1, 0); // color<<1 for 3bpp gfx
}

static tilemap_scan( background )
{
	return (col * 32) + (31 - row);
}

static UINT8 AY8910_portA(UINT32)
{
	return soundlatch;
}

static UINT8 AY8910_portB(UINT32)
{
	return ZetTotalCycles() / timer_rate;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetReset(0);
	ZetReset(1);

	AY8910Reset(0);

	HiscoreReset();

	soundlatch = 0;
	flipscreen = 0;
	nmi_enable = 0;
	palette_bank = 0;
	joinem_timer = 0;

	memset(remap_address, 0, 0x10);
	question_rom = 0;
	question_address = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x008000;
	DrvZ80Dec		= Next; Next += 0x008000;
	DrvZ80ROM1		= Next; Next += 0x002000;

	DrvGfxROM		= Next; Next += 0x010000;

	DrvUsrROM		= Next; Next += 0x080000;

	DrvColPROM		= Next; Next += 0x000400;

	DrvPalette		= (UINT32*)Next; Next += BurnDrvGetPaletteEntries() * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM0		= Next; Next += 0x002000;
	DrvZ80RAM1		= Next; Next += 0x000400;
	DrvVidRAM		= Next; Next += 0x000400;
	DrvColRAM		= Next; Next += 0x000400;
	DrvSprRAM		= Next; Next += 0x000100;
	DrvScrRAM		= DrvSprRAM + 0x80;
	DrvPalRAM		= Next; Next += 0x000100;
	DrvScroll		= Next; Next += 0x000020;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void DrvGfxDecode(INT32 size, INT32 depth)
{
	INT32 Planes[3] = { (size / depth) * 8 * 0, (size / depth) * 8 * 1, (size / depth) * 8 * 2 };
	INT32 XOffs[8]  = { STEP8(0,1) };
	INT32 YOffs[8]  = { STEP8(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(size);
	if (tmp == NULL) {
		return;
	}

	memcpy (tmp, DrvGfxROM, size);

	GfxDecode(((size / depth) * 8) / (8*8), depth, 8, 8, Planes, XOffs, YOffs, 0x040, tmp, DrvGfxROM);

	BurnFree (tmp);
}

static INT32 GetRoms()
{
	char* pRomName;
	struct BurnRomInfo ri;
	UINT8 *pLoad0 = DrvZ80ROM0;
	UINT8 *pLoad1 = DrvZ80ROM1;
	UINT8 *gLoad = DrvGfxROM;
	UINT8 *uLoad = DrvUsrROM;
	INT32 gfx_count = 0;

	memset (uLoad, 0xff, 0x80000);

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {

		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & 7) == 1) {
			if (ri.nType & 8) pLoad0 += 0x1000; // skip gap
			if (BurnLoadRom(pLoad0, i, 1)) return 1;
			pLoad0 += ri.nLen;
			continue;
		}

		if ((ri.nType & 7) == 2) {
			if (BurnLoadRom(pLoad1, i, 1)) return 1;
			pLoad1 += ri.nLen;
			continue;
		}

		if ((ri.nType & 7) == 3) {
			if (BurnLoadRom(gLoad, i, 1)) return 1;
			if (ri.nType & 8) gLoad += 0x1000; // skip gap
			gLoad += ri.nLen;
			gfx_count++;
			continue;
		}

		if ((ri.nType & 7) == 4) {
			if (BurnLoadRom(uLoad, i, 1)) return 1;
			uLoad += ri.nLen;
			continue;
		}

		if ((ri.nType & 7) == 5) {
			if (BurnLoadRom(DrvColPROM + 0x000, i + 0, 1)) return 1;
			if (BurnLoadRom(DrvColPROM + 0x200, i + 1, 1)) return 1;
			i++;

			// Roms are nibbles, (1/2 byte), #0 is low, #1 is high
			for (INT32 j = 0; j < 0x200; j++) {
				DrvColPROM[j] = DrvColPROM[j] | (DrvColPROM[j + 0x200] << 4);
			}

			continue;
		}		
	}

	INT32 depth = (gfx_count == 3) ? 3 : 2;
	graphics_size = gLoad - DrvGfxROM;

	DrvGfxDecode(graphics_size, depth);

	graphics_size = (graphics_size / depth) * 8;

	return 0;
}

static INT32 DrvInit(INT32 map_type, INT32 timerrate, INT32 alt_volume)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (GetRoms()) return 1;
	}

	if (map_type == 0) // jack
	{
		ZetInit(0);
		ZetOpen(0);
		ZetMapMemory(DrvZ80ROM0,		0x0000, 0x3fff, MAP_ROM);
		ZetMapMemory(DrvZ80RAM0,		0x4000, 0x5fff, MAP_RAM);
		ZetMapMemory(DrvSprRAM,			0xb000, 0xb0ff, MAP_ROM); // 0-7f
		ZetMapMemory(DrvPalRAM,			0xb600, 0xb6ff, MAP_RAM); // 0-1f
		ZetMapMemory(DrvVidRAM,			0xb800, 0xbbff, MAP_RAM);
		ZetMapMemory(DrvColRAM,			0xbc00, 0xbfff, MAP_RAM);
		ZetMapMemory(DrvZ80ROM0 + 0x4000,	0xc000, 0xffff, MAP_ROM);
		ZetSetWriteHandler(jack_main_write);
		ZetSetReadHandler(jack_main_read);
		ZetClose();
	}
	else // joinem
	{
		ZetInit(0);
		ZetOpen(0);
		ZetMapMemory(DrvZ80ROM0,		0x0000, 0x7fff, MAP_ROM);
		ZetMapMemory(DrvZ80RAM0,		0x8000, 0x97ff, MAP_RAM);
		ZetMapMemory(DrvSprRAM,			0xb000, 0xb0ff, MAP_ROM); // 0-7f, scroll 80-ff
		ZetMapMemory(DrvVidRAM,			0xb800, 0xbbff, MAP_RAM);
		ZetMapMemory(DrvColRAM,			0xbc00, 0xbfff, MAP_RAM);
		ZetSetWriteHandler(jack_main_write);
		ZetSetReadHandler(jack_main_read);
		ZetClose();
	}

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,		0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,		0x4000, 0x43ff, MAP_RAM);
	ZetSetOutHandler(jack_sound_write_port);
	ZetSetInHandler(jack_sound_read_port);
	ZetClose();

	AY8910Init(0, 1500000, 0);
	AY8910SetPorts(0, &AY8910_portA, &AY8910_portB, NULL, NULL);
	AY8910SetAllRoutes(0, (alt_volume) ? 0.20 : 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, background_map_scan, (map_type == 0) ? background_map_callback : joinem_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM, 2,  8,  8, graphics_size, 0, 0x3f);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, 0 - ((256 - nScreenHeight) / 2));
	GenericTilemapSetScrollCols(0, 32); // joinem

	timer_rate = timerrate;

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	AY8910Exit(0);

	ZetExit();

	GenericTilesExit();

	BurnFree(AllMem);

	return 0;
}

static void joinemPaletteInit()
{
	for (INT32 i = 0; i < BurnDrvGetPaletteEntries(); i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 0x01;
		INT32 r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = (DrvColPROM[i] >> 3) & 0x01;
		bit1 = (DrvColPROM[i] >> 4) & 0x01;
		bit2 = (DrvColPROM[i] >> 5) & 0x01;
		INT32 g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = 0;
		bit1 = (DrvColPROM[i] >> 6) & 0x01;
		bit2 = (DrvColPROM[i] >> 7) & 0x01;
		INT32 b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void jackPaletteUpdate()
{
	for (INT32 i = 0; i < BurnDrvGetPaletteEntries(); i++)
	{
		UINT8 p = DrvPalRAM[i] ^ 0xff;

		UINT8 r = p & 7;
		UINT8 g = (p >> 3) & 7;
		UINT8 b = p >> 6;

		r = (r << 5) | (r << 3) | (r >> 1);
		g = (g << 5) | (g << 3) | (g >> 1);
		b = (b << 6) | (b << 4) | (b << 2) | b;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_sprites(INT32 sprite_type)
{
	INT32 y_adjust = (256 - nScreenHeight) / 2;

	for (INT32 offs = 0x80 - 4; offs >= 0; offs -= 4)
	{
		INT32 color;
		INT32 attr  = DrvSprRAM[offs + 3];
		INT32 code  = DrvSprRAM[offs + 2];
		INT32 sx    = DrvSprRAM[offs + 1];
		INT32 sy    = DrvSprRAM[offs + 0];
		INT32 flipx = attr & 0x80;
		INT32 flipy = attr & 0x40;

		if (sprite_type) {
			code |= (attr & 0x03) << 8;
			color = (((attr >> 3) & 0x07) | palette_bank) << 1;
		} else {
			code |= (attr & 0x08) << 5;
			color = attr & 0x07;
		}

		if (flipscreen)
		{
			sx = 248 - sx;
			sy = 248 - sy;
			flipx ^= 0x80;
			flipy ^= 0x40;
		}

		sy -= y_adjust;

		if (flipy) {
			if (flipx) {
				Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM);
			} else {
				Render8x8Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM);
			}
		} else {
			if (flipx) {
				Render8x8Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM);
			} else {
				Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM);
			}
		}
	}
}

static INT32 DrvDraw()
{
	//if (DrvRecalc) {
		jackPaletteUpdate();
	//	DrvRecalc = 0;
	//}

	if ((nBurnLayer & 1) == 0) BurnTransferClear();
	if ((nBurnLayer & 1) != 0) GenericTilemapDraw(0, pTransDraw, 0);

	draw_sprites(0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 JoinemDraw()
{
	if (DrvRecalc) {
		joinemPaletteInit();
		DrvRecalc = 0;
	}

	for (INT32 i = 0; i < 0x20; i++) {
		GenericTilemapSetScrollCol(0, i, -DrvScroll[i]);
	}

	if ((nBurnLayer & 1) == 0) BurnTransferClear();
	if ((nBurnLayer & 1) != 0) GenericTilemapDraw(0, pTransDraw, 0);

	draw_sprites(1);

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
		memset (DrvInputs, 0, 4);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nSoundBufferPos = 0;
	INT32 nCyclesTotal[2] = { 3000000 / 60, 3000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };
	INT32 nSegment;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nSegment = ((i + 1) * nCyclesTotal[0] / nInterleave) - nCyclesDone[0];
		nCyclesDone[0] += ZetRun(nSegment);
		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(1);
		nSegment = ((i + 1) * nCyclesTotal[1] / nInterleave) - nCyclesDone[1];
		nCyclesDone[1] += ZetRun(nSegment);
		ZetClose();

		if (pBurnSoundOut && (i%8) == 7) {
			INT32 nSegmentLength = nBurnSoundLen / (nInterleave / 8);
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			AY8910Render(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			AY8910Render(pSoundBuf, nSegmentLength);
		}
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 JoinemFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0, 4);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}

		DrvInputs[2] ^= 0x40;
	}

	INT32 nInterleave = 256;
	INT32 nSoundBufferPos = 0;
	INT32 nCyclesTotal[2] = { 3000000 / 60, 3000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };
	INT32 nSegment;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nSegment = ((i + 1) * nCyclesTotal[0] / nInterleave) - nCyclesDone[0];
		nCyclesDone[0] += ZetRun(nSegment);
		if (i == (nInterleave - 1) && nmi_enable) ZetNmi();
		if (joinem_timer == 61) {
			ZetSetIRQLine(0, CPU_IRQSTATUS_AUTO);
			joinem_timer = 0;
		}
		joinem_timer++;
		ZetClose();

		ZetOpen(1);
		nSegment = ((i + 1) * nCyclesTotal[1] / nInterleave) - nCyclesDone[1];
		nCyclesDone[1] += ZetRun(nSegment);
		ZetClose();

		if (pBurnSoundOut && (i%8) == 7) {
			INT32 nSegmentLength = nBurnSoundLen / (nInterleave / 8);
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			AY8910Render(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			AY8910Render(pSoundBuf, nSegmentLength);
		}
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
		*pnMin = 0x029521;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);
		AY8910Scan(nAction, pnMin);

		SCAN_VAR(soundlatch);
		SCAN_VAR(flipscreen);
		SCAN_VAR(nmi_enable);
		SCAN_VAR(palette_bank);
		SCAN_VAR(joinem_timer);

		SCAN_VAR(remap_address);
		SCAN_VAR(question_rom);
		SCAN_VAR(question_address);
	}

	return 0;
}


// Jack the Giantkiller (set 1)

static struct BurnRomInfo jackRomDesc[] = {
	{ "j8",			0x1000, 0xc8e73998, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "jgk.j6",		0x1000, 0x36d7810e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "jgk.j7",		0x1000, 0xb15ff3ee, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "jgk.j5",		0x1000, 0x4a63d242, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "jgk.j3",		0x1000, 0x605514a8, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "jgk.j4",		0x1000, 0xbce489b7, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "jgk.j2",		0x1000, 0xdb21bd55, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "jgk.j1",		0x1000, 0x49fffe31, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "jgk.j9",		0x1000, 0xc2dc1e00, 2 | BRF_PRG | BRF_ESS }, //  8 Z80 #1 Code

	{ "jgk.j12",		0x1000, 0xce726df0, 3 | BRF_GRA },           //  9 Graphics
	{ "jgk.j13",		0x1000, 0x6aec2c8d, 3 | BRF_GRA },           // 10
	{ "jgk.j11",		0x1000, 0xfd14c525, 3 | BRF_GRA },           // 11
	{ "jgk.j10",		0x1000, 0xeab890b2, 3 | BRF_GRA },           // 12
};

STD_ROM_PICK(jack)
STD_ROM_FN(jack)

static INT32 JackInit()
{
	return DrvInit(0, 256, 0);
}

struct BurnDriver BurnDrvJack = {
	"jack", NULL, NULL, NULL, "1982",
	"Jack the Giantkiller (set 1)\0", NULL, "Hara Industries (Cinematronics license)", "Jack the Giantkiller",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, jackRomInfo, jackRomName, NULL, NULL, NULL, NULL, JackInputInfo, JackDIPInfo,
	JackInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x20,
	224, 256, 3, 4
};


// Jack the Giantkiller (set 2)

static struct BurnRomInfo jack2RomDesc[] = {
	{ "jgk.j8",		0x1000, 0xfe229e20, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "jgk.j6",		0x1000, 0x36d7810e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "jgk.j7",		0x1000, 0xb15ff3ee, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "jgk.j5",		0x1000, 0x4a63d242, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "jgk.j3",		0x1000, 0x605514a8, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "jgk.j4",		0x1000, 0xbce489b7, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "jgk.j2",		0x1000, 0xdb21bd55, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "jgk.j1",		0x1000, 0x49fffe31, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "jgk.j9",		0x1000, 0xc2dc1e00, 2 | BRF_PRG | BRF_ESS }, //  8 Z80 #1 Code

	{ "jgk.j12",		0x1000, 0xce726df0, 3 | BRF_GRA },           //  9 Graphics
	{ "jgk.j13",		0x1000, 0x6aec2c8d, 3 | BRF_GRA },           // 10
	{ "jgk.j11",		0x1000, 0xfd14c525, 3 | BRF_GRA },           // 11
	{ "jgk.j10",		0x1000, 0xeab890b2, 3 | BRF_GRA },           // 12
};

STD_ROM_PICK(jack2)
STD_ROM_FN(jack2)

struct BurnDriver BurnDrvJack2 = {
	"jack2", "jack", NULL, NULL, "1982",
	"Jack the Giantkiller (set 2)\0", NULL, "Hara Industries (Cinematronics license)", "Jack the Giantkiller",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, jack2RomInfo, jack2RomName, NULL, NULL, NULL, NULL, JackInputInfo, Jack2DIPInfo,
	JackInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x20,
	224, 256, 3, 4
};


// Jack the Giantkiller (set 3)

static struct BurnRomInfo jack3RomDesc[] = {
	{ "jack8",		0x1000, 0x632151d2, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "jack6",		0x1000, 0xf94f80d9, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "jack7",		0x1000, 0xc830ff1e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "jack5",		0x1000, 0x8dea17e7, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "jgk.j3",		0x1000, 0x605514a8, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "jgk.j4",		0x1000, 0xbce489b7, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "jgk.j2",		0x1000, 0xdb21bd55, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "jack1",		0x1000, 0x7e75ea3d, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "jgk.j9",		0x1000, 0xc2dc1e00, 2 | BRF_PRG | BRF_ESS }, //  8 Z80 #1 Code

	{ "jack12",		0x1000, 0x80320647, 3 | BRF_GRA },           //  9 Graphics
	{ "jgk.j13",		0x1000, 0x6aec2c8d, 3 | BRF_GRA },           // 10
	{ "jgk.j11",		0x1000, 0xfd14c525, 3 | BRF_GRA },           // 11
	{ "jgk.j10",		0x1000, 0xeab890b2, 3 | BRF_GRA },           // 12
};

STD_ROM_PICK(jack3)
STD_ROM_FN(jack3)

struct BurnDriver BurnDrvJack3 = {
	"jack3", "jack", NULL, NULL, "1982",
	"Jack the Giantkiller (set 3)\0", NULL, "Hara Industries (Cinematronics license)", "Jack the Giantkiller",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, jack3RomInfo, jack3RomName, NULL, NULL, NULL, NULL, JackInputInfo, Jack3DIPInfo,
	JackInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x20,
	224, 256, 3, 4
};


// Treasure Hunt

static struct BurnRomInfo treahuntRomDesc[] = {
	{ "thunt-1.f2",		0x1000, 0x0b35858c, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "thunt-2.f3",		0x1000, 0x67305a51, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "thunt-3.4f",		0x1000, 0xd7a969c3, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "thunt-4.6f",		0x1000, 0x2483f14d, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "thunt-5.7f",		0x1000, 0xc69d5e21, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "thunt-6.7e",		0x1000, 0x11bf3d49, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "thunt-7.6e",		0x1000, 0x7c2d6279, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "thunt-8.4e",		0x1000, 0xf73b86fb, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "jgk.j9",		0x1000, 0xc2dc1e00, 2 | BRF_PRG | BRF_ESS }, //  8 Z80 #1 Code

	{ "thunt-13.a4",	0x1000, 0xe03f1f09, 3 | BRF_GRA },           //  9 Graphics
	{ "thunt-12.a3",	0x1000, 0xda4ee9eb, 3 | BRF_GRA },           // 10
	{ "thunt-10.a1",	0x1000, 0x51ec7934, 3 | BRF_GRA },           // 11
	{ "thunt-11.a2",	0x1000, 0xf9781143, 3 | BRF_GRA },           // 12
};

STD_ROM_PICK(treahunt)
STD_ROM_FN(treahunt)

static void treahunt_decode()
{
	for (INT32 i = 0; i < 0x4000; i++)
	{
		if (i & 0x1000)
		{
			DrvZ80Dec[i] = BITSWAP08(DrvZ80ROM0[i], 0, 2, 5, 1, 3, 6, 4, 7);

			if (~i & 0x04) DrvZ80Dec[i] ^= 0x81;
		}
		else
		{
			DrvZ80Dec[i] = BITSWAP08(DrvZ80ROM0[i], 7, 2, 5, 1, 3, 6, 4, 0) ^ 0x81;
		}
	}

	ZetOpen(0);
	ZetMapArea(0x0000, 0x3fff, 2, DrvZ80Dec, DrvZ80ROM0);
	ZetReset(); // get vectors
	ZetClose();
}

static INT32 TreahuntInit()
{
	INT32 nRet = DrvInit(0, 256, 0);

	if (nRet == 0)
	{
		treahunt_decode();
	}

	return nRet;
}

struct BurnDriver BurnDrvTreahunt = {
	"treahunt", "jack", NULL, NULL, "1982",
	"Treasure Hunt\0", NULL, "Hara Industries", "Jack the Giantkiller",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, treahuntRomInfo, treahuntRomName, NULL, NULL, NULL, NULL, JackInputInfo, TreahuntDIPInfo,
	TreahuntInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x20,
	224, 256, 3, 4
};


// Zzyzzyxx (set 1)

static struct BurnRomInfo zzyzzyxxRomDesc[] = {
	{ "a.2f",		0x1000, 0xa9102e34, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "zzyzzyxx.b",		0x1000, 0xefa9d4c6, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "zzyzzyxx.c",		0x1000, 0xb0a365b1, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "zzyzzyxx.d",		0x1000, 0x5ed6dd9a, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "zzyzzyxx.e",		0x1000, 0x5966fdbf, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "f.7e",		0x1000, 0x12f24c68, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "g.6e",		0x1000, 0x408f2326, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "h.4e",		0x1000, 0xf8bbabe0, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "i.5a",		0x1000, 0xc7742460, 2 | BRF_PRG | BRF_ESS }, //  8 Z80 #1 Code
	{ "j.6a",		0x1000, 0x72166ccd, 2 | BRF_PRG | BRF_ESS }, //  9

	{ "n.1c",		0x1000, 0x4f64538d, 3 | BRF_GRA },           // 10 Graphics
	{ "m.1d",		0x1000, 0x217b1402, 3 | BRF_GRA },           // 11
	{ "k.1b",		0x1000, 0xb8b2b8cc, 3 | BRF_GRA },           // 12
	{ "l.1a",		0x1000, 0xab421a83, 3 | BRF_GRA },           // 13
};

STD_ROM_PICK(zzyzzyxx)
STD_ROM_FN(zzyzzyxx)

static INT32 ZzyzzyxxInit()
{
	return DrvInit(0, 32, 1);
}

struct BurnDriver BurnDrvZzyzzyxx = {
	"zzyzzyxx", NULL, NULL, NULL, "1982",
	"Zzyzzyxx (set 1)\0", NULL, "Cinematronics / Advanced Microcomputer Systems", "Jack the Giantkiller",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_ACTION, 0,
	NULL, zzyzzyxxRomInfo, zzyzzyxxRomName, NULL, NULL, NULL, NULL, ZzyzzyxxInputInfo, ZzyzzyxxDIPInfo,
	ZzyzzyxxInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x20,
	224, 256, 3, 4
};


// Zzyzzyxx (set 2)

static struct BurnRomInfo zzyzzyxx2RomDesc[] = {
	{ "a.2f",		0x1000, 0xa9102e34, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "b.3f",		0x1000, 0x4277beab, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "c.4f",		0x1000, 0x72ac99e1, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "d.6f",		0x1000, 0x7c7eec2b, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "e.7f",		0x1000, 0xcffc4a68, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "f.7e",		0x1000, 0x12f24c68, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "g.6e",		0x1000, 0x408f2326, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "h.4e",		0x1000, 0xf8bbabe0, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "i.5a",		0x1000, 0xc7742460, 2 | BRF_PRG | BRF_ESS }, //  8 Z80 #1 Code
	{ "j.6a",		0x1000, 0x72166ccd, 2 | BRF_PRG | BRF_ESS }, //  9

	{ "n.1c",		0x1000, 0x4f64538d, 3 | BRF_GRA },           // 10 Graphics
	{ "m.1d",		0x1000, 0x217b1402, 3 | BRF_GRA },           // 11
	{ "k.1b",		0x1000, 0xb8b2b8cc, 3 | BRF_GRA },           // 12
	{ "l.1a",		0x1000, 0xab421a83, 3 | BRF_GRA },           // 13
};

STD_ROM_PICK(zzyzzyxx2)
STD_ROM_FN(zzyzzyxx2)

struct BurnDriver BurnDrvZzyzzyxx2 = {
	"zzyzzyxx2", "zzyzzyxx", NULL, NULL, "1982",
	"Zzyzzyxx (set 2)\0", NULL, "Cinematronics / Advanced Microcomputer Systems", "Jack the Giantkiller",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_ACTION, 0,
	NULL, zzyzzyxx2RomInfo, zzyzzyxx2RomName, NULL, NULL, NULL, NULL, ZzyzzyxxInputInfo, ZzyzzyxxDIPInfo,
	ZzyzzyxxInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x20,
	224, 256, 3, 4
};


// Brix
// P-1244-1A PCB

static struct BurnRomInfo brixRomDesc[] = {
	{ "brix_a.2f",	0x1000, 0x050e0d70, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "brix_b.3f",	0x1000, 0x668118ae, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "brix_c.4f",	0x1000, 0xff5ed6cf, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "brix_d.6f",	0x1000, 0xc3ae45a9, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "brix_e.7f",	0x1000, 0xdef99fa9, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "brix_f.7e",	0x1000, 0xdde717ed, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "brix_g.6e",	0x1000, 0xadca02d8, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "brix_h.4e",	0x1000, 0xbc3b878c, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "brix_i.5a",	0x1000, 0xc7742460, 2 | BRF_PRG | BRF_ESS }, //  8 Z80 #1 Code
	{ "brix_j.6a",	0x1000, 0x72166ccd, 2 | BRF_PRG | BRF_ESS }, //  9

	{ "brix_n.1c",	0x1000, 0x8064910e, 3 | BRF_GRA },           // 10 Graphics
	{ "brix_m.1d",	0x1000, 0x217b1402, 3 | BRF_GRA },           // 11
	{ "brix_k.1b",	0x1000, 0xc7d7e2a0, 3 | BRF_GRA },           // 12
	{ "brix_l.1a",	0x1000, 0xab421a83, 3 | BRF_GRA },           // 13
};

STD_ROM_PICK(brix)
STD_ROM_FN(brix)

struct BurnDriver BurnDrvBrix = {
	"brix", "zzyzzyxx", NULL, NULL, "1982",
	"Brix\0", NULL, "Cinematronics / Advanced Microcomputer Systems", "Jack the Giantkiller",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_ACTION, 0,
	NULL, brixRomInfo, brixRomName, NULL, NULL, NULL, NULL, ZzyzzyxxInputInfo, ZzyzzyxxDIPInfo,
	ZzyzzyxxInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x20,
	224, 256, 3, 4
};

static INT32 FreezeInit()
{
	return DrvInit(0, 256, 1);
}

// Freeze

static struct BurnRomInfo freezeRomDesc[] = {
	{ "freeze.f2",		0x1000, 0x0a431665, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "freeze.f3",		0x1000, 0x1189b8ad, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "freeze.f4",		0x1000, 0x10c4a5ea, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "freeze.f5",		0x1000, 0x16024c53, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "freeze.f7",		0x1000, 0xea0b0765, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "freeze.e7",		0x1000, 0x1155c00b, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "freeze.e5",		0x1000, 0x95c18d75, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "freeze.e4",		0x1000, 0x7e8f5afc, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "freeze.a1",		0x1000, 0x7771f5b9, 2 | BRF_PRG | BRF_ESS }, //  8 Z80 #1 Code

	{ "freeze.5a",		0x1000, 0x6c8a98a0, 3 | BRF_GRA },           //  9 Graphics
	{ "freeze.3a",		0x1000, 0x6d2125e4, 3 | BRF_GRA },           // 10
	{ "freeze.1a",		0x1000, 0x3a7f2fa9, 3 | BRF_GRA },           // 11
	{ "freeze.2a",		0x1000, 0xdd70ddd6, 3 | BRF_GRA },           // 12
};

STD_ROM_PICK(freeze)
STD_ROM_FN(freeze)

struct BurnDriver BurnDrvFreeze = {
	"freeze", NULL, NULL, NULL, "1984",
	"Freeze\0", NULL, "Cinematronics", "Jack the Giantkiller",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, freezeRomInfo, freezeRomName, NULL, NULL, NULL, NULL, FreezeInputInfo, FreezeDIPInfo,
	FreezeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x20,
	224, 256, 3, 4
};


// Super Casino

static struct BurnRomInfo sucasinoRomDesc[] = {
	{ "1",			0x1000, 0xe116e979, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "2",			0x1000, 0x2a2635f5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3",			0x1000, 0x69864d90, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4",			0x1000, 0x174c9373, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "5",			0x1000, 0x115bcb1e, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "6",			0x1000, 0x434caa17, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "7",			0x1000, 0x67c68b82, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "8",			0x1000, 0xf5b63006, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "9",			0x1000, 0x67cf8aec, 2 | BRF_PRG | BRF_ESS }, //  8 Z80 #1 Code

	{ "11",			0x1000, 0xf92c4c5b, 11 | BRF_GRA },           //  9 Graphics
	{ "10",			0x1000, 0x3b0783ce, 11 | BRF_GRA },           // 10
};

STD_ROM_PICK(sucasino)
STD_ROM_FN(sucasino)

struct BurnDriver BurnDrvSucasino = {
	"sucasino", NULL, NULL, NULL, "1984",
	"Super Casino\0", NULL, "Data Amusement", "Jack the Giantkiller",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_CASINO, 0,
	NULL, sucasinoRomInfo, sucasinoRomName, NULL, NULL, NULL, NULL, SucasinoInputInfo, SucasinoDIPInfo,
	JackInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x20,
	224, 256, 3, 4
};


// Tri-Pool: 3-In-One (Casino Tech)

static struct BurnRomInfo tripoolRomDesc[] = {
	{ "tri73a.bin",		0x1000, 0x96893aa7, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "tri62a.bin",		0x1000, 0x3299dc65, 9 | BRF_PRG | BRF_ESS }, //  1
	{ "tri52b.bin",		0x1000, 0x27ef765e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "tri33c.bin",		0x1000, 0xd7ef061d, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "tri45c.bin",		0x1000, 0x51b813b1, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "tri25d.bin",		0x1000, 0x8e64512d, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "tri13d.bin",		0x1000, 0xad268e9b, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "trisnd.bin",		0x1000, 0x945c4b8b, 2 | BRF_PRG | BRF_ESS }, //  7 Z80 #1 Code

	{ "tri105a.bin",	0x1000, 0x366a753c, 11 | BRF_GRA },           //  8 Graphics
	{ "tri93a.bin",		0x1000, 0x35213782, 11 | BRF_GRA },           //  9
};

STD_ROM_PICK(tripool)
STD_ROM_FN(tripool)

struct BurnDriver BurnDrvTripool = {
	"tripool", NULL, NULL, NULL, "1981",
	"Tri-Pool: 3-In-One (Casino Tech)\0", NULL, "Noma (Casino Tech license)", "Jack the Giantkiller",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, tripoolRomInfo, tripoolRomName, NULL, NULL, NULL, NULL, TripoolInputInfo, TripoolDIPInfo,
	JackInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x20,
	224, 256, 3, 4
};


// Tri-Pool: 3-In-One (Coastal Games)

static struct BurnRomInfo tripoolaRomDesc[] = {
	{ "tri73a.bin",		0x1000, 0x96893aa7, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "tri62a.bin",		0x1000, 0x3299dc65, 9 | BRF_PRG | BRF_ESS }, //  1
	{ "tri52b.bin",		0x1000, 0x27ef765e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "tri33c.bin",		0x1000, 0xd7ef061d, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "tri45c.bin",		0x1000, 0x51b813b1, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "tri25d.bin",		0x1000, 0x8e64512d, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "tp1ckt",			0x1000, 0x72ec43a3, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "trisnd.bin",		0x1000, 0x945c4b8b, 2 | BRF_PRG | BRF_ESS }, //  7 Z80 #1 Code

	{ "tri105a.bin",	0x1000, 0x366a753c, 11 | BRF_GRA },          //  8 Graphics
	{ "tri93a.bin",		0x1000, 0x35213782, 11 | BRF_GRA },          //  9
};

STD_ROM_PICK(tripoola)
STD_ROM_FN(tripoola)

struct BurnDriver BurnDrvTripoola = {
	"tripoola", "tripool", NULL, NULL, "1981",
	"Tri-Pool: 3-In-One (Coastal Games)\0", NULL, "Noma (Coastal Games license)", "Jack the Giantkiller",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, tripoolaRomInfo, tripoolaRomName, NULL, NULL, NULL, NULL, TripoolInputInfo, TripoolDIPInfo,
	JackInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x20,
	224, 256, 3, 4
};


// Super Triv (English questions)

static struct BurnRomInfo strivRomDesc[] = {
	{ "s.triv_p1.2f",		0x1000, 0xdcf5da6e, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "s.triv_p2.3f",		0x1000, 0x921610ba, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "pr3.5f",		    	0x1000, 0xc36f0e21, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "s.triv_cp4.6f",		0x1000, 0x0dc98a97, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "s.triv_c3.7e",		0x1000, 0x2f4b0570, 9 | BRF_PRG | BRF_ESS }, //  4
	{ "s.triv_c2.6e",		0x1000, 0xe21bd3ab, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "s.triv_c1.5e",		0x1000, 0x2c2c7282, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "s.triv_sound.5a",	0x1000, 0xb7ddf84f, 2 | BRF_PRG | BRF_ESS }, //  7 Z80 #1 Code

	{ "s.triv.5a",			0x1000, 0x8f982a9c, 3 | BRF_GRA },           //  8 Graphics
	{ "s.triv.4a",			0x1000, 0x8f982a9c, 3 | BRF_GRA },           //  9
	{ "s.triv.2a",			0x1000, 0x7ad4358e, 3 | BRF_GRA },           // 10
	{ "s.triv.1a",			0x1000, 0x8f60229b, 3 | BRF_GRA },           // 11

	{ "s.triv_ts0.u6",		0x8000, 0x796849da, 4 | BRF_GRA },           // 12 Question data
	{ "s.triv_ts1.u7",		0x8000, 0x059d4900, 4 | BRF_GRA },           // 13
	{ "s.triv_ts2.u8",		0x8000, 0x184159aa, 4 | BRF_GRA },           // 14
	{ "s.triv_ta0.u9",		0x8000, 0xc4eb7f2e, 4 | BRF_GRA },           // 15
	{ "s.triv_ta1.u10",		0x8000, 0x3d9a136f, 4 | BRF_GRA },           // 16
	{ "s.triv_ta2.u11",		0x8000, 0x8fa557b2, 4 | BRF_GRA },           // 17
	{ "s.triv_te0.u12",		0x8000, 0x3f5d1c4b, 4 | BRF_GRA },           // 18
	{ "s.triv_te1.u13",		0x8000, 0x6ae2bf3a, 4 | BRF_GRA },           // 19
	{ "s.triv_tg0.u14",		0x8000, 0x8fc9c76d, 4 | BRF_GRA },           // 20
	{ "s.triv_tg1.u15",		0x8000, 0x981a2a43, 4 | BRF_GRA },           // 21
};

STD_ROM_PICK(striv)
STD_ROM_FN(striv)

static void strivDecode()
{
	for (INT32 i = 0; i < 0x4000; i++)
	{
		if (i & 0x1000)
		{
			if (i & 4)
				DrvZ80ROM0[i] = BITSWAP08(DrvZ80ROM0[i],7,2,5,1,3,6,4,0) ^ 1;
			else
				DrvZ80ROM0[i] = BITSWAP08(DrvZ80ROM0[i],0,2,5,1,3,6,4,7) ^ 0x81;
		}
		else
		{
			if (i & 4)
				DrvZ80ROM0[i] = BITSWAP08(DrvZ80ROM0[i],7,2,5,1,3,6,4,0) ^ 1;
			else
				DrvZ80ROM0[i] = BITSWAP08(DrvZ80ROM0[i],0,2,5,1,3,6,4,7);
		}
	}

	ZetOpen(0);
	ZetUnmapMemory(0xc000, 0xcfff, MAP_RAM);
	ZetClose();

	AY8910SetAllRoutes(0, 0.00, BURN_SND_ROUTE_BOTH); // kill sound 
}

static INT32 StrivInit()
{
	INT32 nRet = DrvInit(0, 256, 0);

	if (nRet == 0)
	{
		strivDecode();
	}

	return nRet;
}

struct BurnDriverD BurnDrvStriv = {
	"striv", NULL, NULL, NULL, "1985",
	"Super Triv (English questions)\0", "bad sound", "Nova du Canada", "Jack the Giantkiller",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_QUIZ, 0,
	NULL, strivRomInfo, strivRomName, NULL, NULL, NULL, NULL, StrivInputInfo, StrivDIPInfo,
	StrivInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x20,
	224, 256, 3, 4
};


// Super Triv (French questions)

static struct BurnRomInfo strivfRomDesc[] = {
	{ "pr1.f2",		0x1000, 0xdcf5da6e, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "pr2.4f",		0x1000, 0x921610ba, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "pr3.5f",		0x1000, 0xc36f0e21, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "pr4.6f",		0x1000, 0x0dc98a97, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "bc3.7e",		0x1000, 0x83f03885, 9 | BRF_PRG | BRF_ESS }, //  4
	{ "bc2.6e",		0x1000, 0x75f18361, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "bc1.5e",		0x1000, 0x0d150385, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "snd.5a",		0x1000, 0xb7ddf84f, 2 | BRF_PRG | BRF_ESS }, //  7 Z80 #1 Code

	{ "chr3.5a",	0x1000, 0x8f982a9c, 3 | BRF_GRA },           //  8 Graphics
	{ "chr2.4a",	0x1000, 0x8f982a9c, 3 | BRF_GRA },           //  9
	{ "chr1.2a",	0x1000, 0x7ad4358e, 3 | BRF_GRA },           // 10
	{ "chr0.1a",	0x1000, 0x8f60229b, 3 | BRF_GRA },           // 11

	{ "rom.u6",		0x8000, 0xa32d7a28, 4 | BRF_GRA },           // 12 Question data
	{ "rom.u7",		0x8000, 0xbc44ae18, 4 | BRF_GRA },           // 13
	{ "tbfd2.u8",	0x8000, 0x9572984a, 4 | BRF_GRA },           // 14
	{ "tbfd3.u9",	0x8000, 0xd904a2f1, 4 | BRF_GRA },           // 15
	{ "tbfl0.u10",	0x8000, 0x680264a2, 4 | BRF_GRA },           // 16
	{ "tbfl1.u11",	0x8000, 0x33e99d00, 4 | BRF_GRA },           // 17
	{ "tbfl2.u12",	0x8000, 0x2e7a941f, 4 | BRF_GRA },           // 18
	{ "tbft0.u13",	0x8000, 0x7d2e5e89, 4 | BRF_GRA },           // 19
	{ "tbft1.u14",	0x8000, 0xd36246cf, 4 | BRF_GRA },           // 20
	{ "tbfd1.u15",	0x8000, 0x745db398, 4 | BRF_GRA },           // 21

	{ "tbfd0.u21",	0x2000, 0x15b83099, 0 | BRF_OPT },           // 22 Junk leftover rom
};

STD_ROM_PICK(strivf)
STD_ROM_FN(strivf)

struct BurnDriverD BurnDrvStrivf = {
	"strivf", "striv", NULL, NULL, "1985",
	"Super Triv (French questions)\0", "bad sound", "Nova du Canada", "Jack the Giantkiller",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_QUIZ, 0,
	NULL, strivfRomInfo, strivfRomName, NULL, NULL, NULL, NULL, StrivInputInfo, StrivDIPInfo,
	StrivInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x20,
	224, 256, 3, 4
};


// Joinem

static struct BurnRomInfo joinemRomDesc[] = {
	{ "join1.r0",		0x2000, 0xb5b2e2cc, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "join2.r2",		0x2000, 0xbcf140e6, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "join3.r4",		0x2000, 0xfe04e4d4, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "join7.s0",		0x1000, 0xbb8a7814, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "join4.p3",		0x1000, 0x4964c82c, 3 | BRF_GRA },           //  4 Graphics
	{ "join5.p2",		0x1000, 0xae78fa89, 3 | BRF_GRA },           //  5
	{ "join6.p1",		0x1000, 0x2b533261, 3 | BRF_GRA },           //  6

	{ "l82s129.11n",	0x0100, 0x7b724211, 5 | BRF_GRA },           //  7 Color data
	{ "h82s129.12n",	0x0100, 0x2e81c5ff, 5 | BRF_GRA },           //  8
};

STD_ROM_PICK(joinem)
STD_ROM_FN(joinem)

static INT32 JoinemInit()
{
	return DrvInit(1, 32, 1);
}

struct BurnDriver BurnDrvJoinem = {
	"joinem", NULL, NULL, NULL, "1983",
	"Joinem\0", NULL, "Global Corporation", "Jack the Giantkiller",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, joinemRomInfo, joinemRomName, NULL, NULL, NULL, NULL, JoinemInputInfo, JoinemDIPInfo,
	JoinemInit, DrvExit, JoinemFrame, JoinemDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Uncle Poo

static struct BurnRomInfo unclepooRomDesc[] = {
	{ "01.f17",		0x2000, 0x92fb238c, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "02.f14",		0x2000, 0xb99214ef, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "03.f11",		0x2000, 0xa136af97, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "04.f09",		0x2000, 0xc4bcd414, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "08.c15",		0x1000, 0xfd84106b, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code

	{ "07.h04",		0x2000, 0xe2f73e99, 3 | BRF_GRA },           //  5 Graphics
	{ "06.j04",		0x2000, 0x94b5f676, 3 | BRF_GRA },           //  6
	{ "05.k04",		0x2000, 0x64026934, 3 | BRF_GRA },           //  7

	{ "diatec_l.bin",	0x0100, 0xb04d466a, 5 | BRF_GRA },           //  8 Color data
	{ "diatec_h.bin",	0x0100, 0x938601b1, 5 | BRF_GRA },           //  9
};

STD_ROM_PICK(unclepoo)
STD_ROM_FN(unclepoo)

struct BurnDriver BurnDrvUnclepoo = {
	"unclepoo", NULL, NULL, NULL, "1983",
	"Uncle Poo\0", NULL, "Diatec", "Jack the Giantkiller",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_ACTION, 0,
	NULL, unclepooRomInfo, unclepooRomName, NULL, NULL, NULL, NULL, UnclepooInputInfo, UnclepooDIPInfo,
	JoinemInit, DrvExit, JoinemFrame, JoinemDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Lover Boy

static struct BurnRomInfo loverboyRomDesc[] = {
	{ "lover.r0",		0x2000, 0xffec4e41, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "lover.r2",		0x2000, 0x04052262, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "lover.r4",		0x2000, 0xce5f3b49, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "lover.r6",		0x1000, 0x839d79b7, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "lover.s0",		0x1000, 0xec38111c, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code

	{ "lover.p3",		0x2000, 0x1a519c8f, 3 | BRF_GRA },           //  5 Graphics
	{ "lover.p2",		0x2000, 0xe465372f, 3 | BRF_GRA },           //  6
	{ "lover.p1",		0x2000, 0xcda0d87e, 3 | BRF_GRA },           //  7

	{ "color.n11",		0x0200, 0xcf4a16ae, 5 | BRF_GRA },           //  8 Color data
	{ "color.n12",		0x0200, 0x4b11ac21, 5 | BRF_GRA },           //  9
};

STD_ROM_PICK(loverboy)
STD_ROM_FN(loverboy)

static INT32 LoverboyInit()
{
	INT32 nRet = DrvInit(1, 32, 1);

	if (nRet == 0)
	{
		DrvZ80ROM0[0x13] = 0x01;
		DrvZ80ROM0[0x12] = 0x9d;
	}

	return nRet;
}

struct BurnDriver BurnDrvLoverboy = {
	"loverboy", NULL, NULL, NULL, "1983",
	"Lover Boy\0", NULL, "G.T Enterprise Inc", "Jack the Giantkiller",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, loverboyRomInfo, loverboyRomName, NULL, NULL, NULL, NULL, LoverboyInputInfo, LoverboyDIPInfo,
	LoverboyInit, DrvExit, JoinemFrame, JoinemDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Triki Triki (bootleg of Lover Boy)

static struct BurnRomInfo trikitriRomDesc[] = {
	{ "1.bin",		0x2000, 0x248f2f12, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "2.bin",		0x2000, 0x04052262, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.bin",		0x2000, 0x979c17c1, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.bin",		0x1000, 0x839d79b7, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "snd.bin",		0x1000, 0x1589c4a9, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code

	{ "5.bin",		0x2000, 0x8cb6ec1c, 3 | BRF_GRA },           //  5 Graphics
	{ "6.bin",		0x2000, 0xa7bed0c1, 3 | BRF_GRA },           //  6
	{ "7.bin",		0x2000, 0xb473ce14, 3 | BRF_GRA },           //  7

	{ "prom2.bin",		0x0100, 0xed5cec15, 5 | BRF_GRA },           //  8 Color data
	{ "prom1.bin",		0x0100, 0x79632c67, 5 | BRF_GRA },           //  9
};

STD_ROM_PICK(trikitri)
STD_ROM_FN(trikitri)

struct BurnDriver BurnDrvTrikitri = {
	"trikitri", "loverboy", NULL, NULL, "1993",
	"Triki Triki (bootleg of Lover Boy)\0", NULL, "bootleg (DDT Enterprise Inc)", "Jack the Giantkiller",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, trikitriRomInfo, trikitriRomName, NULL, NULL, NULL, NULL, LoverboyInputInfo, LoverboyDIPInfo,
	LoverboyInit, DrvExit, JoinemFrame, JoinemDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};
