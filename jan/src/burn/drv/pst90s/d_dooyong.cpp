// FB Alpha Dooyong driver module
// Based on MAME driver by Nicola Salmoria

/*
   Notes:
    A seriously ugly hack has been used to get sound+music in Pollux and Gulf Storm
	After 2 days of hacking, I couldn't find a better way. - Jan 9, 2015 Dink
*/

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "burn_ym2203.h"
#include "burn_ym2151.h"
#include "msm6295.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvGfxROM4;
static UINT8 *DrvGfxROM5;
static UINT8 *DrvTMapROM0;
static UINT8 *DrvTMapROM1;
static UINT8 *DrvTMapROM2;
static UINT8 *DrvTMapROM3;
static UINT8 *DrvTMapROM4;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 *Drv68KRAM0;
static UINT8 *Drv68KRAM1;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvPalRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprBuf;
static UINT8 *DrvTxtRAM;

static UINT8 *scrollregs[4];
static UINT8 *sound_irq_line;
static UINT8 *z80_bank_select;
static UINT8 sprite_enable;
static UINT8 soundlatch;
static UINT8 priority_select;
static UINT8 text_layer_enable;
static UINT8 gulf_storm = 0;
static UINT8 vblank = 0;
static UINT8 pollux_gulfstrm_irq_kicker_hack = 0;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvDips[3];
static UINT8 DrvReset;
static UINT16 DrvInputs[3];

static INT32 global_y = 8;
static INT32 main_cpu_clock = 8000000;

static INT32 gfxmask[6] = { 0, 0, 0, 0, 0, 0 };
static UINT8 *DrvTransTab[6] = { NULL, NULL, NULL, NULL, NULL, NULL };


static struct BurnInputInfo LastdayInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 2,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 5,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy1 + 3,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Lastday)

static struct BurnInputInfo GulfstrmInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Gulfstrm)

static struct BurnInputInfo PolluxInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Pollux)

static struct BurnInputInfo BluehawkInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 2,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 4,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Bluehawk)

static struct BurnInputInfo RsharkInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 4"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy1 + 10,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy1 + 9,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 8,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy1 + 15,	"p2 fire 4"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy2 + 4,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Rshark)

static struct BurnDIPInfo LastdayDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x13, 0x01, 0x01, 0x01, "Off"			},
	{0x13, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Coin Type"		},
	{0x13, 0x01, 0x02, 0x02, "A"			},
	{0x13, 0x01, 0x02, 0x00, "B"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x04, 0x00, "Off"			},
	{0x13, 0x01, 0x04, 0x04, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x08, 0x08, "Off"			},
	{0x13, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x13, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x30, 0x00, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x30, 0x00, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x10, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x20, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x13, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xc0, 0xc0, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xc0, 0x80, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0xc0, 0x40, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0xc0, 0x00, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0x03, 0x00, "1"			},
	{0x14, 0x01, 0x03, 0x02, "2"			},
	{0x14, 0x01, 0x03, 0x03, "3"			},
	{0x14, 0x01, 0x03, 0x01, "4"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x0c, 0x08, "Easy"			},
	{0x14, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x14, 0x01, 0x0c, 0x04, "Hard"			},
	{0x14, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x14, 0x01, 0x30, 0x30, "Every 200000"		},
	{0x14, 0x01, 0x30, 0x20, "Every 240000"		},
	{0x14, 0x01, 0x30, 0x10, "280000"		},
	{0x14, 0x01, 0x30, 0x00, "None"			},

	{0   , 0xfe, 0   ,    2, "Speed"		},
	{0x14, 0x01, 0x40, 0x00, "Low"			},
	{0x14, 0x01, 0x40, 0x40, "High"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x14, 0x01, 0x80, 0x00, "No"			},
	{0x14, 0x01, 0x80, 0x80, "Yes"			},
};

STDDIPINFO(Lastday)

static struct BurnDIPInfo GulfstrmDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x12, 0x01, 0x01, 0x01, "Off"			},
	{0x12, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Coin Type"		},
	{0x12, 0x01, 0x02, 0x02, "A"			},
	{0x12, 0x01, 0x02, 0x00, "B"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x04, 0x00, "Off"			},
	{0x12, 0x01, 0x04, 0x04, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x12, 0x01, 0x08, 0x08, "Off"			},
	{0x12, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x12, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x30, 0x00, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x30, 0x00, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0x30, 0x10, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x30, 0x20, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x12, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0xc0, 0xc0, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0xc0, 0x80, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0xc0, 0x40, "1 Coin  4 Credits"	},
	{0x12, 0x01, 0xc0, 0x00, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0x03, 0x00, "1"			},
	{0x13, 0x01, 0x03, 0x02, "2"			},
	{0x13, 0x01, 0x03, 0x03, "3"			},
	{0x13, 0x01, 0x03, 0x01, "4"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x0c, 0x08, "Easy"			},
	{0x13, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x13, 0x01, 0x0c, 0x04, "Hard"			},
	{0x13, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x13, 0x01, 0x30, 0x30, "Every 300,000"	},
	{0x13, 0x01, 0x30, 0x20, "Every 400,000"	},
	{0x13, 0x01, 0x30, 0x10, "Every 500,000"	},
	{0x13, 0x01, 0x30, 0x00, "None"			},

	{0   , 0xfe, 0   ,    2, "Power Rise(?)"	},
	{0x13, 0x01, 0x40, 0x40, "1"			},
	{0x13, 0x01, 0x40, 0x00, "2"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x13, 0x01, 0x80, 0x00, "No"			},
	{0x13, 0x01, 0x80, 0x80, "Yes"			},
};

STDDIPINFO(Gulfstrm)

static struct BurnDIPInfo PolluxDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x12, 0x01, 0x01, 0x01, "Off"			},
	{0x12, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Coin Type"		},
	{0x12, 0x01, 0x02, 0x02, "A"			},
	{0x12, 0x01, 0x02, 0x00, "B"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x04, 0x00, "Off"			},
	{0x12, 0x01, 0x04, 0x04, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x12, 0x01, 0x08, 0x08, "Off"			},
	{0x12, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x12, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x30, 0x00, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x30, 0x00, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0x30, 0x10, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x30, 0x20, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x12, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0xc0, 0xc0, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0xc0, 0x80, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0xc0, 0x40, "1 Coin  4 Credits"	},
	{0x12, 0x01, 0xc0, 0x00, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0x03, 0x00, "1"			},
	{0x13, 0x01, 0x03, 0x02, "2"			},
	{0x13, 0x01, 0x03, 0x03, "3"			},
	{0x13, 0x01, 0x03, 0x01, "4"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x0c, 0x08, "Easy"			},
	{0x13, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x13, 0x01, 0x0c, 0x04, "Hard"			},
	{0x13, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    0, "Allow Continue"	},
	{0x13, 0x01, 0x80, 0x00, "No"			},
	{0x13, 0x01, 0x80, 0x80, "Yes"			},
};

STDDIPINFO(Pollux)

static struct BurnDIPInfo FlytigerDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0xcf, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x12, 0x01, 0x01, 0x01, "Off"			},
	{0x12, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Coin Type"		},
	{0x12, 0x01, 0x02, 0x02, "A"			},
	{0x12, 0x01, 0x02, 0x00, "B"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x04, 0x00, "Off"			},
	{0x12, 0x01, 0x04, 0x04, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x12, 0x01, 0x08, 0x08, "Off"			},
	{0x12, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x12, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x30, 0x00, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x30, 0x00, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0x30, 0x10, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x30, 0x20, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x12, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0xc0, 0xc0, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0xc0, 0x80, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0xc0, 0x40, "1 Coin  4 Credits"	},
	{0x12, 0x01, 0xc0, 0x00, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0x03, 0x00, "1"			},
	{0x13, 0x01, 0x03, 0x02, "2"			},
	{0x13, 0x01, 0x03, 0x03, "3"			},
	{0x13, 0x01, 0x03, 0x01, "4"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x0c, 0x08, "Easy"			},
	{0x13, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x13, 0x01, 0x0c, 0x04, "Hard"			},
	{0x13, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Auto Fire"		},
	{0x13, 0x01, 0x40, 0x00, "Off"			},
	{0x13, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x13, 0x01, 0x80, 0x00, "No"			},
	{0x13, 0x01, 0x80, 0x80, "Yes"			},
};

STDDIPINFO(Flytiger)

static struct BurnDIPInfo BluehawkDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x12, 0x01, 0x01, 0x01, "Off"			},
	{0x12, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Coin Type"		},
	{0x12, 0x01, 0x02, 0x02, "A"			},
	{0x12, 0x01, 0x02, 0x00, "B"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x04, 0x00, "Off"			},
	{0x12, 0x01, 0x04, 0x04, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x12, 0x01, 0x08, 0x08, "Off"			},
	{0x12, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x12, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x30, 0x00, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x30, 0x00, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0x30, 0x10, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x30, 0x20, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x12, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0xc0, 0xc0, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0xc0, 0x80, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0xc0, 0x40, "1 Coin  4 Credits"	},
	{0x12, 0x01, 0xc0, 0x00, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0x03, 0x00, "1"			},
	{0x13, 0x01, 0x03, 0x02, "2"			},
	{0x13, 0x01, 0x03, 0x03, "3"			},
	{0x13, 0x01, 0x03, 0x01, "4"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x0c, 0x08, "Easy"			},
	{0x13, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x13, 0x01, 0x0c, 0x04, "Hard"			},
	{0x13, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    0, "Allow Continue"	},
	{0x13, 0x01, 0x80, 0x00, "No"			},
	{0x13, 0x01, 0x80, 0x80, "Yes"			},
};

STDDIPINFO(Bluehawk)

static struct BurnDIPInfo PrimellaDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x9d, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x12, 0x01, 0x01, 0x01, "Off"			},
	{0x12, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Coin Type"		},
	{0x12, 0x01, 0x02, 0x02, "A"			},
	{0x12, 0x01, 0x02, 0x00, "B"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x04, 0x00, "Off"			},
	{0x12, 0x01, 0x04, 0x04, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x12, 0x01, 0x08, 0x08, "Off"			},
	{0x12, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x12, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x30, 0x00, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x30, 0x00, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0x30, 0x10, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x30, 0x20, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x12, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0xc0, 0xc0, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0xc0, 0x80, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0xc0, 0x40, "1 Coin  4 Credits"	},
	{0x12, 0x01, 0xc0, 0x00, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Show Girl"		},
	{0x13, 0x01, 0x03, 0x00, "Skip Skip Skip"	},
	{0x13, 0x01, 0x03, 0x03, "Dress Dress Dress"	},
	{0x13, 0x01, 0x03, 0x02, "Dress Half Half"	},
	{0x13, 0x01, 0x03, 0x01, "Dress Half Naked"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x0c, 0x08, "Easy"			},
	{0x13, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x13, 0x01, 0x0c, 0x04, "Hard"			},
	{0x13, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x13, 0x01, 0x10, 0x10, "Upright"		},
	{0x13, 0x01, 0x10, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x13, 0x01, 0x80, 0x00, "No"			},
	{0x13, 0x01, 0x80, 0x80, "Yes"			},
};

STDDIPINFO(Primella)

static struct BurnDIPInfo RsharkDIPList[]=
{
	{0x16, 0xff, 0xff, 0xff, NULL			},
	{0x17, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x16, 0x01, 0x01, 0x01, "Off"			},
	{0x16, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Coin Type"		},
	{0x16, 0x01, 0x02, 0x02, "A"			},
	{0x16, 0x01, 0x02, 0x00, "B"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x16, 0x01, 0x04, 0x00, "Off"			},
	{0x16, 0x01, 0x04, 0x04, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x16, 0x01, 0x08, 0x08, "Off"			},
	{0x16, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x16, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x16, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x16, 0x01, 0x30, 0x00, "2 Coins 3 Credits"	},
	{0x16, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},
	{0x16, 0x01, 0x30, 0x00, "4 Coins 1 Credits"	},
	{0x16, 0x01, 0x30, 0x10, "3 Coins 1 Credits"	},
	{0x16, 0x01, 0x30, 0x20, "2 Coins 1 Credits"	},
	{0x16, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x16, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"	},
	{0x16, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x16, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"	},
	{0x16, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"	},
	{0x16, 0x01, 0xc0, 0xc0, "1 Coin  2 Credits"	},
	{0x16, 0x01, 0xc0, 0x80, "1 Coin  3 Credits"	},
	{0x16, 0x01, 0xc0, 0x40, "1 Coin  4 Credits"	},
	{0x16, 0x01, 0xc0, 0x00, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x17, 0x01, 0x03, 0x00, "1"			},
	{0x17, 0x01, 0x03, 0x02, "2"			},
	{0x17, 0x01, 0x03, 0x03, "3"			},
	{0x17, 0x01, 0x03, 0x01, "4"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x17, 0x01, 0x0c, 0x08, "Easy"			},
	{0x17, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x17, 0x01, 0x0c, 0x04, "Hard"			},
	{0x17, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    0, "Allow Continue"	},
	{0x17, 0x01, 0x80, 0x00, "No"			},
	{0x17, 0x01, 0x80, 0x80, "Yes"			},
};

STDDIPINFO(Rshark)

static struct BurnDIPInfo SuperxDIPList[]=
{
	{0x16, 0xff, 0xff, 0xfe, NULL			},
	{0x17, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Coin Type"		},
	{0x16, 0x01, 0x02, 0x02, "A"			},
	{0x16, 0x01, 0x02, 0x00, "B"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x16, 0x01, 0x04, 0x00, "Off"			},
	{0x16, 0x01, 0x04, 0x04, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x16, 0x01, 0x08, 0x08, "Off"			},
	{0x16, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x16, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x16, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x16, 0x01, 0x30, 0x00, "2 Coins 3 Credits"	},
	{0x16, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},
	{0x16, 0x01, 0x30, 0x00, "4 Coins 1 Credits"	},
	{0x16, 0x01, 0x30, 0x10, "3 Coins 1 Credits"	},
	{0x16, 0x01, 0x30, 0x20, "2 Coins 1 Credits"	},
	{0x16, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x16, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"	},
	{0x16, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x16, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"	},
	{0x16, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"	},
	{0x16, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x16, 0x01, 0xc0, 0x80, "1 Coin  3 Credits"	},
	{0x16, 0x01, 0xc0, 0x40, "1 Coin  4 Credits"	},
	{0x16, 0x01, 0xc0, 0x00, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x17, 0x01, 0x03, 0x00, "1"			},
	{0x17, 0x01, 0x03, 0x02, "2"			},
	{0x17, 0x01, 0x03, 0x03, "3"			},
	{0x17, 0x01, 0x03, 0x01, "4"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x17, 0x01, 0x0c, 0x08, "Easy"			},
	{0x17, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x17, 0x01, 0x0c, 0x04, "Hard"			},
	{0x17, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    0, "Allow Continue"	},
	{0x17, 0x01, 0x80, 0x00, "No"			},
	{0x17, 0x01, 0x80, 0x80, "Yes"			},
};

STDDIPINFO(Superx)

static struct BurnDIPInfo PopbingoDIPList[]=
{
	{0x16, 0xff, 0xff, 0xfb, NULL			},
	{0x17, 0xff, 0xff, 0x4d, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x16, 0x01, 0x01, 0x01, "Off"			},
	{0x16, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Coin Type"		},
	{0x16, 0x01, 0x02, 0x02, "A"			},
	{0x16, 0x01, 0x02, 0x00, "B"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x16, 0x01, 0x04, 0x04, "Off"			},
	{0x16, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x16, 0x01, 0x08, 0x08, "Off"			},
	{0x16, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x16, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x16, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x16, 0x01, 0x30, 0x00, "2 Coins 3 Credits"	},
	{0x16, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},
	{0x16, 0x01, 0x30, 0x00, "4 Coins 1 Credits"	},
	{0x16, 0x01, 0x30, 0x10, "3 Coins 1 Credits"	},
	{0x16, 0x01, 0x30, 0x20, "2 Coins 1 Credits"	},
	{0x16, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x16, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"	},
	{0x16, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x16, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"	},
	{0x16, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"	},
	{0x16, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x16, 0x01, 0xc0, 0x80, "1 Coin  3 Credits"	},
	{0x16, 0x01, 0xc0, 0x40, "1 Coin  4 Credits"	},
	{0x16, 0x01, 0xc0, 0x00, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "VS Max Round"		},
	{0x17, 0x01, 0x01, 0x01, "3"			},
	{0x17, 0x01, 0x01, 0x00, "1"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x17, 0x01, 0x0c, 0x08, "Easy"			},
	{0x17, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x17, 0x01, 0x0c, 0x04, "Hard"			},
	{0x17, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Blocks Don't Drop"	},
	{0x17, 0x01, 0x40, 0x40, "Off"			},
	{0x17, 0x01, 0x40, 0x00, "On"			},
};

STDDIPINFO(Popbingo)

static inline void palette_write_4bgr(INT32 offset)
{
	UINT16 p = *((UINT16*)(DrvPalRAM + offset));

	INT32 r = p & 0x0f;
	INT32 g = (p >> 4) & 0x0f;
	INT32 b = (p >> 8) & 0x0f;

	DrvPalette[offset/2] = BurnHighCol(r+(r*16),g+(g*16),b+(b*16),0);
}

static inline void palette_write_5rgb(INT32 offset)
{
	UINT16 p = *((UINT16*)(DrvPalRAM + offset));

	INT32 b = (p >> 0) & 0x1f;
	INT32 g = (p >> 5) & 0x1f;
	INT32 r = (p >> 10) & 0x1f;

	r = (r << 3) | (r >> 2);
	g = (g << 3) | (g >> 2);
	b = (b << 3) | (b >> 2);

	DrvPalette[offset/2] = BurnHighCol(r,g,b,0);
}

static void bankswitch(INT32 data)
{
	z80_bank_select[0] = data;

	ZetMapMemory(DrvZ80ROM0 + ((data & 0x07) * 0x4000), 0x8000, 0xbfff, MAP_ROM);
}

static void __fastcall lastday_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf800) == 0xc800) {
		DrvPalRAM[address & 0x7ff] = data;
		palette_write_4bgr(address & 0x7fe);
		return;
	}

	if ((address & 0xfff8) == 0xc000) {
		scrollregs[0][address & 7] = data;
		return;
	}

	if ((address & 0xfff8) == 0xc008) {
		scrollregs[1][address & 7] = data;
		return;
	}

	switch (address)
	{
		case 0xc010:
		//	coin counters = data & 0x03;
			sprite_enable = data & 0x10;
		//	flipscreen = data & 0x40;
		return;

		case 0xc011:
			bankswitch(data);
		return;

		case 0xc012:
			soundlatch = data;
		return;
	}
}

static UINT8 __fastcall lastday_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xc010:
			return DrvInputs[0] ^ 0x08;

		case 0xc011:
			return DrvInputs[1];

		case 0xc012:
			return DrvInputs[2];

		case 0xc013:
			return DrvDips[0];

		case 0xc014:
			return DrvDips[1];
	}

	return 0;
}

static void __fastcall gulfstrm_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf800) == 0xf800) {
		DrvPalRAM[address & 0x7ff] = data;
		palette_write_5rgb(address & 0x7fe);
		return;
	}

	if ((address & 0xfff8) == 0xf018) {
		scrollregs[0][address & 7] = data;
		return;
	}

	if ((address & 0xfff8) == 0xf020) {
		scrollregs[1][address & 7] = data;
		return;
	}

	switch (address)
	{
		case 0xf000:
			bankswitch(data);
		return;

		case 0xf008:
		//	flipscreen = data & 0x01;
		return;

		case 0xf010:
			soundlatch = data;
		return;
	}
}

static UINT8 __fastcall gulfstrm_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xf000:
			return DrvDips[0];

		case 0xf001:
			return DrvDips[1];

		case 0xf002:
			return DrvInputs[2];

		case 0xf003:
			return DrvInputs[1];

		case 0xf004:
			return (DrvInputs[0] & ~0x10) | (vblank ? 0 : 0x10); // vblank
	}

	return 0;
}

static UINT8 __fastcall pollux_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xf000:
			return DrvDips[0];

		case 0xf001:
			return DrvDips[1];

		case 0xf002:
			return DrvInputs[1];

		case 0xf003:
			return DrvInputs[2];

		case 0xf004:
			return DrvInputs[0];
	}

	return 0;
}

static void __fastcall flytiger_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf800) == 0xe800) {
		if (z80_bank_select[1]) {
			DrvPalRAM[address & 0x7ff] = data;
			palette_write_5rgb(address & 0x7fe);
		}
		return;
	}

	if ((address & 0xfff8) == 0xe030) {
		scrollregs[0][address & 7] = data;
		return;
	}

	if ((address & 0xfff8) == 0xe040) {
		scrollregs[1][address & 7] = data;
		return;
	}

	switch (address)
	{
		case 0xe000:
			bankswitch(data);
		return;

		case 0xe010:
			priority_select = data & 0x10;
			z80_bank_select[1] = data & 0x08;
		//	flipscreen = data & 0x01;
		return;

		case 0xe020:
			soundlatch = data;
		return;
	}
}

static UINT8 __fastcall flytiger_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xe004:
			return DrvInputs[0];

		case 0xe000:
			return DrvInputs[1];

		case 0xe002:
			return DrvInputs[2];

		case 0xe006:
			return DrvDips[0];

		case 0xe008:
			return DrvDips[1];
	}

	return 0;
}

static void __fastcall bluehawk_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf800) == 0xc800) {
		DrvPalRAM[address & 0x7ff] = data;
		palette_write_5rgb(address & 0x7fe);
		return;
	}

	if ((address & 0xfff8) == 0xc018) {
		scrollregs[2][address & 7] = data;
		return;
	}

	if ((address & 0xfff8) == 0xc040) {
		scrollregs[0][address & 7] = data;
		return;
	}

	if ((address & 0xfff8) == 0xc048) {
		scrollregs[1][address & 7] = data;
		return;
	}

	switch (address)
	{
		case 0xc000:
		//	flipscreen = data & 0x01;
		return;

		case 0xc008:
			bankswitch(data);
		return;

		case 0xc010:
			soundlatch = data;
		return;
	}
}

static UINT8 __fastcall bluehawk_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xc000:
			return DrvDips[0];

		case 0xc001:
			return DrvDips[1];

		case 0xc002:
			return DrvInputs[1];

		case 0xc003:
			return DrvInputs[2];

		case 0xc004:
			return DrvInputs[0];
	}

	return 0;
}

static void __fastcall primella_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf800) == 0xf000) {
		DrvPalRAM[address & 0x7ff] = data;
		palette_write_5rgb(address & 0x7fe);
		return;
	}

	if ((address & 0xfff8) == 0xfc00) {
		scrollregs[0][address & 7] = data;
		return;
	}

	if ((address & 0xfff8) == 0xfc08) {
		scrollregs[1][address & 7] = data;
		return;
	}

	switch (address)
	{
		case 0xf800:
			bankswitch(data);
			text_layer_enable = ~data & 0x08;
		//	flipscreen = data & 0x10;
		return;

		case 0xf810:
			soundlatch = data;
		return;
	}
}

static UINT8 __fastcall primella_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xf800:
			return DrvDips[0];

		case 0xf810:
			return DrvDips[1];

		case 0xf820:
			return DrvInputs[1];

		case 0xf830:
			return DrvInputs[2];

		case 0xf840:
			return DrvInputs[0];
	}

	return 0;
}

static void __fastcall superx_main_write_word(UINT32 address, UINT16 data)
{
	if (address & 0xff00000) { // Mirror top address bits.
		SekWriteWord(address & 0xfffff, data);
		return;
	}

	if ((address & 0x0f0000) == 0x0c0000) address = (address & 0xffff) | 0x80000; // fix address for rshark

	if ((address & 0x0ff000) == 0x088000) {
		*((UINT16*)(DrvPalRAM + (address & 0xffe))) = data;
		palette_write_5rgb(address & 0xffe);
		return;
	}

	if ((address & 0x0ffff0) == 0x084000) {
		scrollregs[0][(address & 0x0e) / 2] = data;
		return;
	}

	if ((address & 0x0ffff0) == 0x084010) {
		scrollregs[2][(address & 0x0e) / 2] = data;
		return;
	}
	if ((address & 0x0ffff0) == 0x08c000) {
		scrollregs[1][(address & 0x0e) / 2] = data;
		return;
	}

	if ((address & 0x0ffff0) == 0x08c010) {
		scrollregs[3][(address & 0x0e) / 2] = data;
		return;
	}

	switch (address)
	{
		case 0x080012:
		case 0x080013:
			soundlatch = data;
		return;

		case 0x080014:
		case 0x080015:
		//	flipscreen = data & 0x01;
			priority_select = data & 0x10;
		return;
	}
}

static void __fastcall superx_main_write_byte(UINT32 address, UINT8 data)
{
	if (address & 0xff00000) { // Mirror top address bits.
		SekWriteByte(address & 0xfffff, data);
		return;
	}

	if ((address & 0x0f0000) == 0x0c0000) address = (address & 0xffff) | 0x80000; // fix address for rshark

	if ((address & 0x0ff000) == 0x088000) {
		DrvPalRAM[(address & 0xfff)^1] = data;
		palette_write_5rgb(address & 0xffe);
		return;
	}

	if ((address & 0x0ffff1) == 0x084001) {
		scrollregs[0][(address & 0x0e) / 2] = data;
		return;
	}

	if ((address & 0x0ffff1) == 0x084011) {
		scrollregs[2][(address & 0x0e) / 2] = data;
		return;
	}
	if ((address & 0x0ffff1) == 0x08c001) {
		scrollregs[1][(address & 0x0e) / 2] = data;
		return;
	}

	if ((address & 0x0ffff1) == 0x08c011) {
		scrollregs[3][(address & 0x0e) / 2] = data;
		return;
	}

	switch (address)
	{
		case 0x080012:
		case 0x080013:
			soundlatch = data;
		return;

		case 0x080014:
		case 0x080015:
		//	flipscreen = data & 0x01;
			priority_select = data & 0x10;
		return;
	}
}
	
static UINT16 __fastcall superx_main_read_word(UINT32 address)
{
	if (address & 0xff00000) { // Mirror top address bits.
		return SekReadWord(address & 0xfffff);
	}

	if ((address & 0x0f0000) == 0x0c0000) address = (address & 0xffff) | 0x80000; // fix address for rshark

	switch (address)
	{
		case 0x080002:
		case 0x080003:
			return (DrvDips[1] << 8) | DrvDips[0];

		case 0x080004:
		case 0x080005:
			return DrvInputs[0];

		case 0x080006:
		case 0x080007:
			return DrvInputs[1];
	}

	return 0;
}
	
static UINT8 __fastcall superx_main_read_byte(UINT32 address)
{
	if (address & 0xff00000) { // Mirror top address bits.
		return SekReadByte(address & 0xfffff);
	}

	if ((address & 0x0f0000) == 0x0c0000) address = (address & 0xffff) | 0x80000; // fix address for rshark

	switch (address)
	{
		case 0x080002:
			return DrvDips[1];

		case 0x080003:
			return DrvDips[0];

		case 0x080004:
			return (DrvInputs[0] >> 8) & 0xff;

		case 0x080005:
			return DrvInputs[0] & 0xff;

		case 0x080006:
			return (DrvInputs[1] >> 8) & 0xff;

		case 0x080007:
			return DrvInputs[1] & 0xff;
	}

	return 0;
}

static void __fastcall sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xf000:
		case 0xf001:
		case 0xf002:
		case 0xf003:
			BurnYM2203Write((address & 2) / 2, address & 1, data);
		return;

	// pollux
		case 0xf802:
		case 0xf803:
		case 0xf804:
		case 0xf805:
			BurnYM2203Write((address & 4) / 4, address & 1, data);
		return;

	// flytiger
		case 0xf808:
			BurnYM2151SelectRegister(data);
		return;

		case 0xf809:
			BurnYM2151WriteRegister(data);
		return;

		case 0xf80a:
			MSM6295Command(0, data);
		return;
	}
}

static UINT8 __fastcall sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xc800:
		case 0xf800:
			return soundlatch;

		case 0xf000:
		case 0xf001:
		case 0xf002:
		case 0xf003:
			return BurnYM2203Read((address & 2) / 2, address & 1);

	// pollux
		case 0xf802:
		case 0xf803:
		case 0xf804:
		case 0xf805:
			return BurnYM2203Read((address & 4) / 4, address & 1);

	// flytiger
		case 0xf808:
		case 0xf809:
			return BurnYM2151ReadStatus();

		case 0xf80a:
			return MSM6295ReadStatus(0);
	}

	return 0;
}

inline static void DrvYM2203IRQHandler(INT32 n, INT32 nStatus)
{
	sound_irq_line[n] = nStatus;

	ZetSetIRQLine(0, ((sound_irq_line[0] | sound_irq_line[1]) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE));
}

static void DrvYM2151IrqHandler(INT32 nStatus)
{
	ZetSetIRQLine(0, (nStatus ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE));
}

inline static INT32 DrvSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)ZetTotalCycles() * nSoundRate / 4000000;
}

inline static INT32 DrvSynchroniseStream8Mhz(INT32 nSoundRate)
{
	return (INT64)ZetTotalCycles() * nSoundRate / 8000000;
}

inline static double DrvGetTime()
{
	return (double)ZetTotalCycles() / 4000000.0;
}

inline static double DrvGetTime8Mhz()
{
	return (double)ZetTotalCycles() / 8000000.0;
}

static INT32 Z80YM2203DoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	BurnYM2203Reset();
	ZetClose();

	sound_irq_line[0] = sound_irq_line[1] = 0;
	sprite_enable = 0;
	soundlatch = 0;
	priority_select = 0;
	text_layer_enable = 0;

	return 0;
}

static INT32 Z80YM2151DoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	ZetClose();

	BurnYM2151Reset();
	MSM6295Reset(0);

	sprite_enable = 0;
	soundlatch = 0;
	priority_select = 0;
	text_layer_enable = 0;

	return 0;
}

static INT32 Drv68KDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	ZetOpen(0);
	ZetReset();
	ZetClose();

	MSM6295Reset(0);
	BurnYM2151Reset();

	sprite_enable = 0;
	soundlatch = 0;
	priority_select = 0;
	text_layer_enable = 0;

	return 0;
}

static INT32 DrvZ80MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0	= Next; Next += 0x020000;
	DrvZ80ROM1	= Next; Next += 0x010000;

	DrvGfxROM0	= Next; Next += 0x040000;
	DrvGfxROM1	= Next; Next += 0x100000;
	DrvGfxROM2	= Next; Next += 0x100000;
	DrvGfxROM3	= Next; Next += 0x100000;
	DrvGfxROM4	= Next; Next += 0x100000;

	DrvTMapROM0	= Next; Next += 0x020000;
	DrvTMapROM1	= Next; Next += 0x020000;
	DrvTMapROM2	= Next; Next += 0x020000;

	MSM6295ROM	= Next; Next += 0x040000;

	DrvPalette	= (UINT32*)Next; Next += 0x00401 * sizeof(UINT32);

	AllRam		= Next;

	DrvZ80RAM0	= Next; Next += 0x01400;
	DrvZ80RAM1	= Next; Next += 0x00800;

	DrvSprRAM	= Next; Next += 0x01000;
	DrvSprBuf	= Next; Next += 0x01000;
	DrvTxtRAM	= Next; Next += 0x01000;
	DrvPalRAM	= Next; Next += 0x00800;

	scrollregs[0]	= Next; Next += 0x00008;
	scrollregs[1]	= Next; Next += 0x00008;
	scrollregs[2]	= Next; Next += 0x00008;
	scrollregs[3]	= Next; Next += 0x00008;
	sound_irq_line	= Next; Next += 0x00002;
	z80_bank_select	= Next; Next += 0x00002;

	RamEnd		= Next;
	MemEnd		= Next;

	return 0;
}

static INT32 Drv68KMemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM	= Next; Next += 0x040000;
	DrvZ80ROM1	= Next; Next += 0x010000;

	DrvGfxROM1	= Next; Next += 0x400000;
	DrvGfxROM2	= Next; Next += 0x200000;
	DrvGfxROM3	= Next; Next += 0x200000;
	DrvGfxROM4	= Next; Next += 0x200000;
	DrvGfxROM5	= Next; Next += 0x200000;

	DrvTMapROM0	= Next; Next += 0x080000;
	DrvTMapROM1	= Next; Next += 0x080000;
	DrvTMapROM2	= Next; Next += 0x080000;
	DrvTMapROM3	= Next; Next += 0x080000;
	DrvTMapROM4	= Next; Next += 0x080000;

	MSM6295ROM	= Next; Next += 0x040000;

	DrvPalette	= (UINT32*)Next; Next += 0x00801 * sizeof(UINT32);

	AllRam		= Next;

	Drv68KRAM0	= Next; Next += 0x0d000;
	Drv68KRAM1	= Next; Next += 0x02000;
	DrvZ80RAM1	= Next; Next += 0x00800;

	DrvSprRAM	= Next; Next += 0x01000;
	DrvSprBuf	= Next; Next += 0x01000;
	DrvPalRAM	= Next; Next += 0x01000;

	scrollregs[0]	= Next; Next += 0x00008;
	scrollregs[1]	= Next; Next += 0x00008;
	scrollregs[2]	= Next; Next += 0x00008;
	scrollregs[3]	= Next; Next += 0x00008;
	sound_irq_line	= Next; Next += 0x00002;
	z80_bank_select	= Next; Next += 0x00002;

	RamEnd		= Next;
	MemEnd		= Next;

	return 0;
}

static void DrvGfxDecode(INT32 gfx, UINT8 *src, INT32 nLen, INT32 nType)
{
	INT32 Plane0[4]  = { 0, 4, (nLen/2)*8, (nLen/2)*8+4 };
	INT32 XOffs0[8]  = { STEP4(0,1), STEP4(8,1) };
	INT32 YOffs0[8]  = { STEP8(0,16) };

	INT32 Plane1[4]  = { STEP4(0,4) };
	INT32 XOffs1[32] = { STEP4(0,1), STEP4(16,1), STEP4(1024,1), STEP4(1040,1), STEP4(2048,1), STEP4(2064,1), STEP4(3072,1), STEP4(3088,1) };
	INT32 YOffs1[32] = { STEP32(0,32) };

	INT32 Plane2[4]  = { STEP4(0,4) };
	INT32 XOffs2[16] = { STEP4(0,1), STEP4(16,1), STEP4(512,1), STEP4(528,1) };
	INT32 YOffs2[16] = { STEP16(0,32) };

	INT32 Plane3[4]  = { STEP4(0,1) };
	INT32 XOffs3[8]  = { STEP8(0,4) };
	INT32 YOffs3[8]  = { STEP8(0,32) };
	
	INT32 Plane4[4]  = { STEP4(0,1) };
	INT32 XOffs4[16] = { STEP8(0,4), STEP8(512,4) };
	INT32 YOffs4[16] = { STEP16(0,32) };

	INT32 Plane5[8] = { STEP4(0,4), STEP4((nLen/2)*8, 4) };

	INT32 TileSizes[6] = { 8, 32, 16, 8, 16, 32 };
	INT32 TileDepths[6] = { 4, 4, 4, 4, 4, 8 };

	INT32 tilesize = TileSizes[nType];
	INT32 tilebpp = TileDepths[nType];

	UINT8 *tmp = (UINT8*)BurnMalloc(nLen);
	if (tmp == NULL) {
		return;
	}

	memcpy (tmp, src, nLen);

	gfxmask[gfx] = (((nLen * 8) / tilebpp) / (tilesize * tilesize)) - 1;

	switch (nType)
	{
		case 0: // lastday characters
			GfxDecode(gfxmask[gfx]+1, tilebpp, tilesize, tilesize, Plane0, XOffs0, YOffs0, 0x0080, tmp, src);
		break;

		case 1: // tiles
			GfxDecode(gfxmask[gfx]+1, tilebpp, tilesize, tilesize, Plane1, XOffs1, YOffs1, 0x1000, tmp, src);
		break;

		case 2: // sprites
			GfxDecode(gfxmask[gfx]+1, tilebpp, tilesize, tilesize, Plane2, XOffs2, YOffs2, 0x0400, tmp, src);
		break;

		case 3: // bluehawk characters
			GfxDecode(gfxmask[gfx]+1, tilebpp, tilesize, tilesize, Plane3, XOffs3, YOffs3, 0x0100, tmp, src);
		break;

		case 4: // rshark sprites
			GfxDecode(gfxmask[gfx]+1, tilebpp, tilesize, tilesize, Plane4, XOffs4, YOffs4, 0x0400, tmp, src);
		break;

		case 5: // popbingo tiles
			GfxDecode(gfxmask[gfx]+1, tilebpp, tilesize, tilesize, Plane5, XOffs1, YOffs1, 0x1000, tmp, src);
		break;
	}

	// Calculate transparency tables
	{
		INT32 count = 0;

		DrvTransTab[gfx] = (UINT8*)BurnMalloc(gfxmask[gfx]+1);

		memset (DrvTransTab[gfx], 1, gfxmask[gfx]+1);

		for (INT32 i = 0; i < (gfxmask[gfx]+1)*tilesize*tilesize; i+=tilesize*tilesize) {

			int advcnt = 1;

			for (INT32 j = 0; j < tilesize*tilesize; j++) {
				if (src[i+j] != ((1<<tilebpp)-1)) {
					DrvTransTab[gfx][i/(tilesize*tilesize)] = 0;
					advcnt=0;
					break;
				}
			}

			count+=advcnt;
		}

#ifdef FBA_DEBUG
		bprintf (0, _T("%d, mask: %x, transpcnt: %x\n"), gfx, gfxmask[gfx], count);
#endif
	}

	BurnFree (tmp);
}

static void DrvSoundCPUInit(INT32 cpuno, INT32 type)
{
	ZetInit(cpuno);
	ZetOpen(cpuno);
	ZetMapMemory(DrvZ80ROM1,	((type) ? 0x0000 : 0x0000), ((type) ? 0xefff : 0x7fff), MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,	((type) ? 0xf000 : 0xc000), ((type) ? 0xf7ff : 0xc7ff), MAP_RAM);
	ZetSetWriteHandler(sound_write);
	ZetSetReadHandler(sound_read);
	ZetClose();
}


static INT32 LastdayInit()
{
	AllMem = NULL;
	DrvZ80MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	DrvZ80MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x10000,  1, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  2, 1)) return 1;
		memcpy (DrvZ80ROM1, DrvZ80ROM1 + 0x8000, 0x8000);

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  3, 1)) return 1;
		memcpy (DrvGfxROM0, DrvGfxROM0 + 0x8000, 0x8000);

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  4, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x00001,  5, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000,  6, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x00001,  7, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x40000,  8, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x40001,  9, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x00000, 10, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x00001, 11, 2)) return 1;

		if (BurnLoadRom(DrvTMapROM0 + 0x00000, 12, 2)) return 1;
		if (BurnLoadRom(DrvTMapROM0 + 0x00001, 13, 2)) return 1;

		if (BurnLoadRom(DrvTMapROM1 + 0x00000, 14, 2)) return 1;
		if (BurnLoadRom(DrvTMapROM1 + 0x00001, 15, 2)) return 1;

		DrvGfxDecode(0, DrvGfxROM0, 0x08000, 0);
		DrvGfxDecode(1, DrvGfxROM1, 0x40000, 2);
		DrvGfxDecode(2, DrvGfxROM2, 0x80000, 1);
		DrvGfxDecode(3, DrvGfxROM3, 0x40000, 1);
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvPalRAM,		0xc800, 0xcfff, MAP_ROM);
	ZetMapMemory(DrvTxtRAM,		0xd000, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM0,	0xe000, 0xefff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xf000, 0xffff, MAP_RAM);
	ZetSetWriteHandler(lastday_main_write);
	ZetSetReadHandler(lastday_main_read);
	ZetClose();

	DrvSoundCPUInit(1,0);

	BurnYM2203Init(2, 4000000, &DrvYM2203IRQHandler, DrvSynchroniseStream8Mhz, DrvGetTime8Mhz, 0);
	BurnTimerAttachZet(8000000);
	BurnYM2203SetAllRoutes(0, 0.40, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetAllRoutes(1, 0.40, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	Z80YM2203DoReset();

	return 0;
}

static INT32 GulfstrmInit()
{
	AllMem = NULL;
	DrvZ80MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	DrvZ80MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  1, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  2, 1)) return 1;
		memcpy (DrvGfxROM0, DrvGfxROM0 + 0x8000, 0x8000);

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  3, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x00001,  4, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x40000,  5, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x40001,  6, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000,  7, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x00001,  8, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x40000,  9, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x40001, 10, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x00000, 11, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x00001, 12, 2)) return 1;

		if (BurnLoadRom(DrvTMapROM0 + 0x00000, 13, 2)) return 1;
		if (BurnLoadRom(DrvTMapROM0 + 0x00001, 14, 2)) return 1;

		if (BurnLoadRom(DrvTMapROM1 + 0x00000, 15, 2)) return 1;
		if (BurnLoadRom(DrvTMapROM1 + 0x00001, 16, 2)) return 1;

		DrvGfxDecode(0, DrvGfxROM0, 0x08000, 0);
		DrvGfxDecode(1, DrvGfxROM1, 0x80000, 2);
		DrvGfxDecode(2, DrvGfxROM2, 0x80000, 1);
		DrvGfxDecode(3, DrvGfxROM3, 0x40000, 1);
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,	0xc000, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xd000, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,		0xe000, 0xefff, MAP_RAM);
	ZetMapMemory(DrvPalRAM,		0xf800, 0xffff, MAP_ROM);
	ZetSetWriteHandler(gulfstrm_main_write);
	ZetSetReadHandler(gulfstrm_main_read);
	ZetClose();

	DrvSoundCPUInit(1,0);

	BurnYM2203Init(2, 1500000, &DrvYM2203IRQHandler, DrvSynchroniseStream8Mhz, DrvGetTime8Mhz, 0);
	BurnTimerAttachZet(8000000);
	BurnYM2203SetAllRoutes(0, 0.40, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetPSGVolume(0, 0.20);
	BurnYM2203SetAllRoutes(1, 0.40, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetPSGVolume(1, 0.20);

	pollux_gulfstrm_irq_kicker_hack = 10;

	GenericTilesInit();

	Z80YM2203DoReset();

	gulf_storm = 1;

	return 0;
}

static INT32 PolluxInit()
{
	AllMem = NULL;
	DrvZ80MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	DrvZ80MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  1, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x08000,  2, 1)) return 1;
		memcpy (DrvGfxROM0, DrvGfxROM0 + 0x10000, 0x8000);

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  3, 1)) return 1;
		BurnByteswap(DrvGfxROM1, 0x80000);

		if (BurnLoadRom(DrvGfxROM2 + 0x00000,  4, 1)) return 1;
		BurnByteswap(DrvGfxROM2, 0x80000);

		if (BurnLoadRom(DrvGfxROM3 + 0x00000,  5, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x00001,  6, 2)) return 1;
		memset (DrvGfxROM3 + 0x40000, 0xff, 0x40000);

		if (BurnLoadRom(DrvTMapROM0 + 0x00000,  7, 2)) return 1;
		if (BurnLoadRom(DrvTMapROM0 + 0x00001,  8, 2)) return 1;

		if (BurnLoadRom(DrvTMapROM1 + 0x00000,  9, 2)) return 1;
		if (BurnLoadRom(DrvTMapROM1 + 0x00001, 10, 2)) return 1;

		DrvGfxDecode(0, DrvGfxROM0, 0x10000, 0);
		DrvGfxDecode(1, DrvGfxROM1, 0x80000, 2);
		DrvGfxDecode(2, DrvGfxROM2, 0x80000, 1);
		DrvGfxDecode(3, DrvGfxROM3, 0x80000, 1);
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,	0xc000, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xd000, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,		0xe000, 0xefff, MAP_RAM);
	ZetMapMemory(DrvPalRAM,		0xf800, 0xffff, MAP_ROM);
	ZetSetWriteHandler(gulfstrm_main_write);
	ZetSetReadHandler(pollux_main_read);
	ZetClose();

	DrvSoundCPUInit(1,1);

	BurnYM2203Init(2, 1500000, &DrvYM2203IRQHandler, DrvSynchroniseStream8Mhz, DrvGetTime8Mhz, 0);
	BurnTimerAttachZet(8000000);
	BurnYM2203SetAllRoutes(0, 0.40, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetAllRoutes(1, 0.40, BURN_SND_ROUTE_BOTH);

	pollux_gulfstrm_irq_kicker_hack = 13;
	main_cpu_clock = 12000000; // +4Mhz hack so the video scrolls better.

	GenericTilesInit();

	Z80YM2203DoReset();

	return 0;
}

static INT32 FlytigerCommonInit(INT32 game_select)
{
	AllMem = NULL;
	DrvZ80MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	DrvZ80MemIndex();

	if (game_select == 0)
	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  1, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x08000,  2, 1)) return 1;
		memcpy (DrvGfxROM0, DrvGfxROM0 + 0x10000, 0x8000);

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  3, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x00001,  4, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x40000,  5, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x40001,  6, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000,  7, 1)) return 1;
		BurnByteswap(DrvGfxROM2, 0x80000);

		if (BurnLoadRom(DrvGfxROM3 + 0x00000,  8, 1)) return 1;
		BurnByteswap(DrvGfxROM3, 0x80000);

		memcpy (DrvTMapROM0, DrvGfxROM2 + 0x78000, 0x08000);
		memcpy (DrvTMapROM1, DrvGfxROM3 + 0x78000, 0x08000);

		if (BurnLoadRom(MSM6295ROM + 0x00000,  9, 1)) return 1;
	}
	else
	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  1, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x08000,  2, 1)) return 1;
		memcpy (DrvGfxROM0, DrvGfxROM0 + 0x10000, 0x8000);

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  3, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x00001,  4, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x40000,  5, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x40001,  6, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00001,  7, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x00000,  8, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x40001,  9, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x40000, 10, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x00001, 11, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x00000, 12, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x40001, 13, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x40000, 14, 2)) return 1;

		memcpy (DrvTMapROM0, DrvGfxROM2 + 0x78000, 0x08000);
		memcpy (DrvTMapROM1, DrvGfxROM3 + 0x78000, 0x08000);

		if (BurnLoadRom(MSM6295ROM + 0x00000, 15, 1)) return 1;
	}

	{
		DrvGfxDecode(0, DrvGfxROM0, 0x10000, 0);
		DrvGfxDecode(1, DrvGfxROM1, 0x80000, 2);
		DrvGfxDecode(2, DrvGfxROM2, 0x80000, 1);
		DrvGfxDecode(3, DrvGfxROM3, 0x80000, 1);
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvSprRAM,		0xc000, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM0,	0xd000, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvPalRAM,		0xe800, 0xefff, MAP_ROM);
	ZetMapMemory(DrvTxtRAM,		0xf000, 0xffff, MAP_RAM);
	ZetSetWriteHandler(flytiger_main_write);
	ZetSetReadHandler(flytiger_main_read);
	ZetClose();

	DrvSoundCPUInit(1,1);

	BurnYM2151Init(3579545);
	BurnYM2151SetIrqHandler(&DrvYM2151IrqHandler);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.50, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.50, BURN_SND_ROUTE_RIGHT);

	MSM6295Init(0, 1000000 / 132, 1);
	MSM6295SetRoute(0, 0.60, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	Z80YM2151DoReset();

	return 0;
}

static INT32 BluehawkInit()
{
	AllMem = NULL;
	DrvZ80MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	DrvZ80MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  1, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  3, 1)) return 1;
		BurnByteswap(DrvGfxROM1, 0x80000);

		if (BurnLoadRom(DrvGfxROM2 + 0x00000,  4, 1)) return 1;
		BurnByteswap(DrvGfxROM2, 0x80000);

		if (BurnLoadRom(DrvGfxROM3 + 0x00000,  5, 1)) return 1;
		BurnByteswap(DrvGfxROM3, 0x80000);

		if (BurnLoadRom(DrvGfxROM4 + 0x00000,  6, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM4 + 0x00001,  7, 2)) return 1;

		memcpy (DrvTMapROM0, DrvGfxROM2 + 0x78000, 0x08000);
		memcpy (DrvTMapROM1, DrvGfxROM3 + 0x78000, 0x08000);
		memcpy (DrvTMapROM2, DrvGfxROM4 + 0x38000, 0x08000);

		if (BurnLoadRom(MSM6295ROM + 0x00000,  8, 1)) return 1;

		DrvGfxDecode(0, DrvGfxROM0, 0x10000, 3);
		DrvGfxDecode(1, DrvGfxROM1, 0x80000, 2);
		DrvGfxDecode(2, DrvGfxROM2, 0x80000, 1);
		DrvGfxDecode(3, DrvGfxROM3, 0x80000, 1);
		DrvGfxDecode(4, DrvGfxROM4, 0x40000, 1);
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvPalRAM,		0xc800, 0xcfff, MAP_ROM);
	ZetMapMemory(DrvTxtRAM,		0xd000, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xe000, 0xefff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM0,	0xf000, 0xffff, MAP_RAM);
	ZetSetWriteHandler(bluehawk_main_write);
	ZetSetReadHandler(bluehawk_main_read);
	ZetClose();

	DrvSoundCPUInit(1,1);

	BurnYM2151Init(3579545);
	BurnYM2151SetIrqHandler(&DrvYM2151IrqHandler);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.50, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.50, BURN_SND_ROUTE_RIGHT);

	MSM6295Init(0, 1000000 / 132, 1);
	MSM6295SetRoute(0, 0.60, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	Z80YM2151DoReset();

	return 0;
}

static INT32 PrimellaCommonInit(INT32 game_select)
{
	AllMem = NULL;
	DrvZ80MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	DrvZ80MemIndex();

	if (game_select == 0)
	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  1, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000,  3, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x00001,  4, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x40000,  5, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x40001,  6, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x00000,  7, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x00001,  8, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x40000,  9, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x40001, 10, 2)) return 1;

		memcpy (DrvTMapROM0, DrvGfxROM2 + 0x78000, 0x08000);
		memcpy (DrvTMapROM1, DrvGfxROM3 + 0x78000, 0x08000);

		if (BurnLoadRom(MSM6295ROM + 0x00000, 11, 1)) return 1;

		DrvGfxDecode(0, DrvGfxROM0, 0x20000, 3);
		DrvGfxDecode(2, DrvGfxROM2, 0x80000, 1);
		DrvGfxDecode(3, DrvGfxROM3, 0x80000, 1);
	}
	else
	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  1, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000,  3, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x00001,  4, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x00000,  5, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x00001,  6, 2)) return 1;

		memcpy (DrvTMapROM0, DrvGfxROM2 + 0x38000, 0x08000);
		memcpy (DrvTMapROM1, DrvGfxROM3 + 0x38000, 0x08000);

		if (BurnLoadRom(MSM6295ROM + 0x00000,  7, 1)) return 1;

		DrvGfxDecode(0, DrvGfxROM0, 0x20000, 3);
		DrvGfxDecode(2, DrvGfxROM2, 0x40000, 1);
		DrvGfxDecode(3, DrvGfxROM3, 0x40000, 1);
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,	0xc000, 0xd3ff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,		0xe000, 0xefff, MAP_RAM);
	ZetMapMemory(DrvPalRAM,		0xf000, 0xf7ff, MAP_ROM);
	ZetSetWriteHandler(primella_main_write);
	ZetSetReadHandler(primella_main_read);
	ZetClose();

	DrvSoundCPUInit(1,1);

	BurnYM2151Init(4000000);
	BurnYM2151SetIrqHandler(&DrvYM2151IrqHandler);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.50, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.50, BURN_SND_ROUTE_RIGHT);

	MSM6295Init(0, 1000000 / 132, 1);

	MSM6295SetRoute(0, 0.60, BURN_SND_ROUTE_BOTH);

	global_y = 0;

	GenericTilesInit();

	Z80YM2151DoReset();

	return 0;
}
	
static INT32 RsharkCommonInit(INT32 game_select)
{
	AllMem = NULL;
	Drv68KMemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	Drv68KMemIndex();

	if (game_select == 0) // superx
	{
		if (BurnLoadRom(Drv68KROM  + 0x00001,   0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x00000,   1, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,   2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,   3, 1)) return 1;
		BurnByteswap(DrvGfxROM1, 0x200000);

		if (BurnLoadRom(DrvGfxROM2 + 0x00000,   4, 1)) return 1;
		BurnByteswap(DrvGfxROM2, 0x100000);

		if (BurnLoadRom(DrvGfxROM3 + 0x00000,   5, 1)) return 1;
		BurnByteswap(DrvGfxROM3, 0x100000);

		if (BurnLoadRom(DrvGfxROM4 + 0x00000,   6, 1)) return 1;
		BurnByteswap(DrvGfxROM4, 0x100000);

		if (BurnLoadRom(DrvGfxROM5 + 0x00000,   7, 1)) return 1;
		BurnByteswap(DrvGfxROM5, 0x100000);

		if (BurnLoadRom(DrvTMapROM4 + 0x00000,  8, 1)) return 1;
		if (BurnLoadRom(DrvTMapROM4 + 0x20000,  9, 1)) return 1;
		if (BurnLoadRom(DrvTMapROM4 + 0x40000, 10, 1)) return 1;
		if (BurnLoadRom(DrvTMapROM4 + 0x60000, 11, 1)) return 1;

		memcpy (DrvTMapROM0, DrvGfxROM2 + 0x00000, 0x080000);
		memcpy (DrvTMapROM1, DrvGfxROM3 + 0x00000, 0x080000);
		memcpy (DrvTMapROM2, DrvGfxROM4 + 0x00000, 0x080000);
		memcpy (DrvTMapROM3, DrvGfxROM5 + 0x00000, 0x080000);

		if (BurnLoadRom(MSM6295ROM + 0x00000, 12, 1)) return 1;
		if (BurnLoadRom(MSM6295ROM + 0x20000, 13, 1)) return 1;

		DrvGfxDecode(1, DrvGfxROM1, 0x200000, 4);
		DrvGfxDecode(2, DrvGfxROM2, 0x100000, 2);
		DrvGfxDecode(3, DrvGfxROM3, 0x100000, 2);
		DrvGfxDecode(4, DrvGfxROM4, 0x100000, 2);
		DrvGfxDecode(5, DrvGfxROM5, 0x100000, 2);
	}
	else if (game_select == 1) // rshark
	{
		if (BurnLoadRom(Drv68KROM  + 0x00001,   0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x00000,   1, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,   2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  3, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x000001,  4, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x100000,  5, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x100001,  6, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000,   7, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x00001,   8, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x00000,   9, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x00001,  10, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM4 + 0x00000,  11, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM4 + 0x00001,  12, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM5 + 0x00000,  13, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM5 + 0x00001,  14, 2)) return 1;

		if (BurnLoadRom(DrvTMapROM4 + 0x00000, 15, 1)) return 1;
		if (BurnLoadRom(DrvTMapROM4 + 0x20000, 16, 1)) return 1;
		if (BurnLoadRom(DrvTMapROM4 + 0x40000, 17, 1)) return 1;
		if (BurnLoadRom(DrvTMapROM4 + 0x60000, 18, 1)) return 1;

		memcpy (DrvTMapROM0, DrvGfxROM2 + 0x00000, 0x080000);
		memcpy (DrvTMapROM1, DrvGfxROM3 + 0x00000, 0x080000);
		memcpy (DrvTMapROM2, DrvGfxROM4 + 0x00000, 0x080000);
		memcpy (DrvTMapROM3, DrvGfxROM5 + 0x00000, 0x080000);

		if (BurnLoadRom(MSM6295ROM + 0x00000, 19, 1)) return 1;
		if (BurnLoadRom(MSM6295ROM + 0x20000, 20, 1)) return 1;

		DrvGfxDecode(1, DrvGfxROM1, 0x200000, 4);
		DrvGfxDecode(2, DrvGfxROM2, 0x100000, 2);
		DrvGfxDecode(3, DrvGfxROM3, 0x100000, 2);
		DrvGfxDecode(4, DrvGfxROM4, 0x100000, 2);
		DrvGfxDecode(5, DrvGfxROM5, 0x100000, 2);
	}
	else if (game_select == 2) // popbingo
	{
		if (BurnLoadRom(Drv68KROM  + 0x00001,   0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x00000,   1, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,   2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  3, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x000001,  4, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x000000,  5, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x000001,  6, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x100000,  7, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x100001,  8, 2)) return 1;

		memcpy (DrvTMapROM0, DrvGfxROM2 + 0x00000, 0x080000);

		if (BurnLoadRom(MSM6295ROM + 0x00000,   9, 1)) return 1;

		DrvGfxDecode(1, DrvGfxROM1, 0x100000, 4);
		DrvGfxDecode(2, DrvGfxROM2, 0x200000, 5);

		main_cpu_clock = 10000000; // 10mhz
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x03ffff, MAP_ROM);

	if (game_select > 0) {
		SekMapMemory(Drv68KRAM0,	0x040000, 0x04cfff, MAP_RAM);
		SekMapMemory(DrvSprRAM,		0x04d000, 0x04dfff, MAP_RAM);
		SekMapMemory(Drv68KRAM1,	0x04e000, 0x04ffff, MAP_RAM);
		SekMapMemory(DrvPalRAM,		0x0c8000, 0x0c8fff, MAP_ROM);
	} else {
		SekMapMemory(DrvPalRAM,		0x088000, 0x088fff, MAP_ROM);
		SekMapMemory(Drv68KRAM0,	0x0d0000, 0x0dcfff, MAP_RAM);
		SekMapMemory(DrvSprRAM,		0x0dd000, 0x0ddfff, MAP_RAM);
		SekMapMemory(Drv68KRAM1,	0x0de000, 0x0dffff, MAP_RAM);
	}

	SekSetWriteWordHandler(0,	superx_main_write_word);
	SekSetWriteByteHandler(0,	superx_main_write_byte);
	SekSetReadWordHandler(0,	superx_main_read_word);
	SekSetReadByteHandler(0,	superx_main_read_byte);
	SekClose();

	DrvSoundCPUInit(0,1);

	BurnYM2151Init(4000000);
	BurnYM2151SetIrqHandler(&DrvYM2151IrqHandler);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.50, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.50, BURN_SND_ROUTE_RIGHT);

	MSM6295Init(0, 1000000 / 132, 1);
	MSM6295SetRoute(0, 0.60, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	Drv68KDoReset();

	return 0;
}

static void DrvFreeTransTab()
{
	for (INT32 i = 0; i < 6; i++) {
		if (DrvTransTab[i]) {
			BurnFree(DrvTransTab[i]);
		}
	}
}

static INT32 Z80YM2203Exit()
{
	GenericTilesExit();

	ZetExit();

	BurnYM2203Exit();

	BurnFree (AllMem);

	DrvFreeTransTab();

	memset (gfxmask, 0, 6 * sizeof(INT32));

	global_y = 8;
	main_cpu_clock = 8000000;
	vblank = 0;
	gulf_storm = 0;
	pollux_gulfstrm_irq_kicker_hack = 0;

	return 0;
}

static INT32 Z80YM2151Exit()
{
	GenericTilesExit();

	ZetExit();

	BurnYM2151Exit();
	MSM6295Exit(0);
	MSM6295ROM = NULL;

	BurnFree (AllMem);

	DrvFreeTransTab();

	memset (gfxmask, 0, 6 * sizeof(INT32));

	global_y = 8;
	main_cpu_clock = 8000000;

	return 0;
}

static INT32 Drv68KExit()
{
	GenericTilesExit();

	SekExit();
	ZetExit();

	BurnYM2151Exit();
	MSM6295Exit(0);
	MSM6295ROM = NULL;

	memset (gfxmask, 0, 6 * sizeof(INT32));

	BurnFree (AllMem);

	DrvFreeTransTab();

	global_y = 8;
	main_cpu_clock = 8000000;

	return 0;
}

static void DrawLayer(UINT8 *rom, UINT8 *scroll, UINT8 *gfxbase, INT32 gfxlenmask, INT32 colorbase, UINT8 *transtab, INT32 transp, INT32 depth, INT32 /*pri*/)
{
	if (scroll[6] & 0x10) return;

	INT32 scrollx = scroll[0] + 64;
	INT32 scrolly = scroll[3] + global_y;

	INT32 format = scroll[6] & 0x20;

	for (INT32 offs = 0; offs < 32 * 8; offs++)
	{
		INT32 sy = (offs & 0x07) * 32;
		INT32 sx = (offs / 0x08) * 32;

		sy -= scrolly;
		if (sy < -31) sy += 256;
		sx -= scrollx;
		if (sx >= nScreenWidth || sy >= nScreenHeight) continue;

		INT32 ofst = (offs + ((INT32)scroll[1] * 0x40)) * 2;

		INT32 attr = rom[ofst];

		INT32 code, color, flipx, flipy;

		if (format)
		{
			code  = rom[ofst + 1] | ((attr & 0x01) * 256) | ((attr & 0x80) * 4);
			color = (attr & 0x78) / 8;
			flipy = attr & 0x04;
			flipx = attr & 0x02;
		}
		else
		{
			INT32 codemask = 0x03;
			INT32 colormask = 0x3c;

			if (depth == 8) {
				codemask = 0x07;
				colormask = 0;
			}

			code  = rom[ofst + 1] | ((attr & codemask) * 256);
			color = (attr & colormask) / 4;
			flipy = attr & 0x80;
			flipx = attr & 0x40;
		}

		code &= gfxlenmask;
		if (transtab[code] && transp != -1) continue; // let's skip transparent tiles

		if (flipy) {
			if (flipx) {
				Render32x32Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, depth, transp, colorbase, gfxbase);
			} else {
				Render32x32Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, depth, transp, colorbase, gfxbase);
			}
		} else {
			if (flipx) {
				Render32x32Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, depth, transp, colorbase, gfxbase);
			} else {
				Render32x32Tile_Mask_Clip(pTransDraw, code, sx, sy, color, depth, transp, colorbase, gfxbase);
			}
		}

		if (sy >= 0) continue;

		if (flipy) {
			if (flipx) {
				Render32x32Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy + 256, color, depth, transp, colorbase, gfxbase);
			} else {
				Render32x32Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy + 256, color, depth, transp, colorbase, gfxbase);
			}
		} else {
			if (flipx) {
				Render32x32Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy + 256, color, depth, transp, colorbase, gfxbase);
			} else {
				Render32x32Tile_Mask_Clip(pTransDraw, code, sx, sy + 256, color, depth, transp, colorbase, gfxbase);
			}
		}
	}
}

static void draw_layer_rshark(UINT8 *tmaprom0, UINT8 *tmaprom1, UINT8 *scroll, UINT8 *gfxbase, INT32 gfxlenmask, INT32 colorbase, UINT8 *transtab, INT32 transp, INT32 /*pri*/)
{
	INT32 scrollx = scroll[0] + 64;
	INT32 scrolly = (scroll[3] + (scroll[4] * 256) + global_y) & 0x1ff;

	for (INT32 offs = 0; offs < 64 * 32; offs++)
	{
		INT32 sy = (offs & 0x1f) * 16;
		INT32 sx = (offs / 0x20) * 16;

		sy -= scrolly;
		if (sy < -15) sy += 512;
		sx -= scrollx;
		if (sx >= nScreenWidth || sy >= nScreenHeight) continue;

		INT32 ofst  = (offs + ((INT32)scroll[1] * 0x200)) * 2;
		INT32 attr  = tmaprom0[ofst + 0];
		INT32 code  =(tmaprom0[ofst + 1] + ((attr & 0x1f) * 256)) & gfxlenmask;
		INT32 color = tmaprom1[ofst/2] & 0x0f;
		INT32 flipy = attr & 0x80;
		INT32 flipx = attr & 0x40;

		code &= gfxlenmask;
		if (transtab[code] && transp != -1) continue; // let's skip transparent tiles

		if (flipy) {
			if (flipx) {
				Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 4, transp, colorbase, gfxbase);
			} else {
				Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 4, transp, colorbase, gfxbase);
			}
		} else {
			if (flipx) {
				Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 4, transp, colorbase, gfxbase);
			} else {
				Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 4, transp, colorbase, gfxbase);
			}
		}

		if (sy >= 0) continue;

		if (flipy) {
			if (flipx) {
				Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy + 512, color, 4, transp, colorbase, gfxbase);
			} else {
				Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy + 512, color, 4, transp, colorbase, gfxbase);
			}
		} else {
			if (flipx) {
				Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy + 512, color, 4, transp, colorbase, gfxbase);
			} else {
				Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy + 512, color, 4, transp, colorbase, gfxbase);
			}
		}
	}
}

static void DrawTextLayer(INT32 mode, INT32 /*pri*/, INT32 scroll_adj)
{
	for (INT32 offs = 0; offs < 64 * 32; offs++)
	{
		INT32 sy = ((((offs & 0x1f) * 8) - global_y) - scroll_adj) & 0xff;
		INT32 sx = ((offs / 0x20) * 8) - 64;

		if (sx >= nScreenWidth || sy >= nScreenHeight) continue;

		INT32 code;

		if (mode)
		{
			code = DrvTxtRAM[offs * 2] + (DrvTxtRAM[offs * 2 + 1] << 8);
		}
		else
		{
			code = DrvTxtRAM[offs] + (DrvTxtRAM[offs | 0x800] << 8);
		}

		if (DrvTransTab[0][code & gfxmask[0]]) continue;

		Render8x8Tile_Mask_Clip(pTransDraw, code & gfxmask[0], sx, sy, (code >> 12) & 0xf, 4, 0x0f, 0, DrvGfxROM0);
	}
}

static void draw_sprites(INT32 priority, UINT8 extensions)
{
	UINT8 *ram = DrvSprBuf;

	for (INT32 offs = 0x1000-32; offs >= 0; offs -= 32)
	{
		INT32 color = ram[offs+1] & 0x0f;

		INT32 pri = (((color == 0x00) || (color == 0x0f)) ? 0: 1);
		if (pri != priority) continue;

		INT32 sx    = ram[offs+3] | ((ram[offs+1] & 0x10) << 4);
		INT32 sy    = ram[offs+2];
		INT32 code  = ram[offs] | ((ram[offs+1] & 0xe0) << 3);

		INT32 flipy = 0;
		INT32 flipx = 0;
		INT32 height = 0;

		if (extensions)
		{
			UINT8 ext = ram[offs+0x1c];

			if (extensions & 0x01) // 12 bit sprites
				code |= ((ext & 0x01) << 11);

			if (extensions & 0x02) // height
			{
				height = (ext & 0x70) >> 4;
				code &= ~height;

				flipx = ext & 0x08;
				flipy = ext & 0x04;
			}

			if (extensions & 0x04) // bluehawk y shift
				sy += 6 - ((~ext & 0x02) << 7);

			if (extensions & 0x08) // flytiger y shift
				sy -=(ext & 0x02) << 7;
		}

		sy -= global_y;

		for (INT32 y = 0; y <= height; y++)
		{
			if (DrvTransTab[1][(code + y)&gfxmask[1]]) continue; // skip transparent tiles

			if (flipy) {
				if (flipx) {
					Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, (code + y)&gfxmask[1], sx - 64, sy + (16 * (flipy ? (height - y) : y)), color, 4, 0xf, 0x100, DrvGfxROM1);
				} else {
					Render16x16Tile_Mask_FlipY_Clip(pTransDraw, (code + y)&gfxmask[1], sx - 64, sy + (16 * (flipy ? (height - y) : y)), color, 4, 0xf, 0x100, DrvGfxROM1);
				}
			} else {
				if (flipx) {
					Render16x16Tile_Mask_FlipX_Clip(pTransDraw, (code + y)&gfxmask[1], sx - 64, sy + (16 * (flipy ? (height - y) : y)), color, 4, 0xf, 0x100, DrvGfxROM1);
				} else {
					Render16x16Tile_Mask_Clip(pTransDraw, (code + y)&gfxmask[1], sx - 64, sy + (16 * (flipy ? (height - y) : y)), color, 4, 0xf, 0x100, DrvGfxROM1);
				}
			}
		}
	}
}

static void draw_sprites_rshark(INT32 priority)
{
	UINT16 *ram = (UINT16*)DrvSprBuf;

	for (INT32 offs = 0; offs < 0x1000 / 2; offs += 8)
	{
		if (ram[offs] & 0x0001)
		{
			INT32 color  = ram[offs+7] & 0x000f;

			INT32 pri (((color == 0x00) || (color == 0x0f)) ? 0 : 1);
			if (priority != pri) continue;

			INT32 code   = ram[offs+3];
			INT32 width  = ram[offs+1] & 0x000f;
			INT32 height = (ram[offs+1] & 0x00f0) >> 4;
			INT32 sx     = ram[offs+4] & 0x01ff;
			INT32 sy     = (INT16)ram[offs+6] & 0x01ff;
			if (sy & 0x0100) sy |= ~(int)0x01ff;

			for (INT32 y = 0; y <= height; y++)
			{
				INT32 _y = sy + (16 * y);

				for (INT32 x = 0; x <= width; x++)
				{
					INT32 _x = sx + (16 * x);

					if (DrvTransTab[1][code & gfxmask[1]] == 0) // skip transparent tiles
						Render16x16Tile_Mask_Clip(pTransDraw, code & gfxmask[1], _x - 64, _y - global_y, color, 4, 0xf, 0, DrvGfxROM1);

					code++;
				}
			}
		}
	}
}
static inline void DrvPaletteRecalc4BGR()
{
	for (INT32 i = 0; i < BurnDrvGetPaletteEntries()*2; i+=2) {
		palette_write_4bgr(i);
	}

	DrvPalette[BurnDrvGetPaletteEntries()] = 0; // black
}

static inline void DrvPaletteRecalc5RGB()
{
	for (INT32 i = 0; i < BurnDrvGetPaletteEntries()*2; i+=2) {
		palette_write_5rgb(i);
	}

	DrvPalette[BurnDrvGetPaletteEntries()] = 0; // black
}

static INT32 LastdayDraw()
{
	if (DrvRecalc) {
		DrvPaletteRecalc4BGR();
		DrvRecalc = 0;
	}

	UINT32 black = BurnDrvGetPaletteEntries();

	for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
		pTransDraw[i] = black;
	}

	if (nBurnLayer & 1) DrawLayer(DrvTMapROM0, scrollregs[0], DrvGfxROM2, gfxmask[2], 0x300, DrvTransTab[2], -1,   4, 1);
	if (!sprite_enable) draw_sprites(0,0);
	if (nBurnLayer & 2) DrawLayer(DrvTMapROM1, scrollregs[1], DrvGfxROM3, gfxmask[3], 0x200, DrvTransTab[3], 0x0f, 4, 2);
	if (!sprite_enable) draw_sprites(1,0);
	if (nBurnLayer & 4) DrawTextLayer(0, 4, 8);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 GulfstrmDraw()
{
	if (DrvRecalc) {
		DrvPaletteRecalc5RGB();
		DrvRecalc = 0;
	}

	UINT32 black = BurnDrvGetPaletteEntries();

	for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
		pTransDraw[i] = black;
	}

	if (nBurnLayer & 1) DrawLayer(DrvTMapROM0, scrollregs[0], DrvGfxROM2, gfxmask[2], 0x300, DrvTransTab[2], -1,   4, 1);
	draw_sprites(0,1);
	if (nBurnLayer & 2) DrawLayer(DrvTMapROM1, scrollregs[1], DrvGfxROM3, gfxmask[3], 0x200, DrvTransTab[3], 0x0f, 4, 2);
	draw_sprites(1,1);
	if (nBurnLayer & 4) DrawTextLayer(0, 4, 8);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 PolluxDraw()
{
	if (DrvRecalc) {
		DrvPaletteRecalc5RGB();
		DrvRecalc = 0;
	}

	UINT32 black = BurnDrvGetPaletteEntries();

	for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
		pTransDraw[i] = black;
	}

	if (nBurnLayer & 1) DrawLayer(DrvTMapROM0, scrollregs[0], DrvGfxROM2, gfxmask[2], 0x300, DrvTransTab[2], -1,   4, 1);
	draw_sprites(0,1|2);
	if (nBurnLayer & 2) DrawLayer(DrvTMapROM1, scrollregs[1], DrvGfxROM3, gfxmask[3], 0x200, DrvTransTab[3], 0x0f, 4, 2);
	draw_sprites(1,1|2);
	if (nBurnLayer & 4) DrawTextLayer(0, 4, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 FlytigerDraw()
{
	if (DrvRecalc) {
		DrvPaletteRecalc5RGB();
		DrvRecalc = 0;
	}

	UINT32 black = BurnDrvGetPaletteEntries();

	for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
		pTransDraw[i] = black;
	}

	if (priority_select)
	{
		if (nBurnLayer & 2) DrawLayer(DrvTMapROM1, scrollregs[1], DrvGfxROM3, gfxmask[3], 0x200, DrvTransTab[3], 0x0f, 4, 1);
		draw_sprites(0,1|2|8);
		if (nBurnLayer & 1) DrawLayer(DrvTMapROM0, scrollregs[0], DrvGfxROM2, gfxmask[2], 0x300, DrvTransTab[2], 0x0f, 4, 2);
	}
	else
	{
		if (nBurnLayer & 1) DrawLayer(DrvTMapROM0, scrollregs[0], DrvGfxROM2, gfxmask[2], 0x300, DrvTransTab[2], 0x0f, 4, 1);
		draw_sprites(0,1|2|8);
		if (nBurnLayer & 2) DrawLayer(DrvTMapROM1, scrollregs[1], DrvGfxROM3, gfxmask[3], 0x200, DrvTransTab[3], 0x0f, 4, 2);
	}

	draw_sprites(1,1|2|8);

	if (nBurnLayer & 4) DrawTextLayer(0, 4, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 BluehawkDraw()
{
	if (DrvRecalc) {
		DrvPaletteRecalc5RGB();
		DrvRecalc = 0;
	}

	UINT32 black = BurnDrvGetPaletteEntries();

	for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
		pTransDraw[i] = black;
	}

	if (nBurnLayer & 1) DrawLayer(DrvTMapROM0, scrollregs[0], DrvGfxROM2, gfxmask[2], 0x300, DrvTransTab[2], -1,   4, 1);
	draw_sprites(0,1|2|4);
	if (nBurnLayer & 2) DrawLayer(DrvTMapROM1, scrollregs[1], DrvGfxROM3, gfxmask[3], 0x200, DrvTransTab[3], 0x0f, 4, 2);
	draw_sprites(1,1|2|4);
	if (nBurnLayer & 2) DrawLayer(DrvTMapROM2, scrollregs[2], DrvGfxROM4, gfxmask[4], 0x000, DrvTransTab[4], 0x0f, 4, 4);
	if (nBurnLayer & 4) DrawTextLayer(1, 4, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 PrimellaDraw()
{
	if (DrvRecalc) {
		DrvPaletteRecalc5RGB();
		DrvRecalc = 0;
	}

	UINT32 black = BurnDrvGetPaletteEntries();

	for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
		pTransDraw[i] = black;
	}

	if (nBurnLayer & 1) DrawLayer(DrvTMapROM0, scrollregs[0], DrvGfxROM2, gfxmask[2], 0x300, DrvTransTab[2], -1,   4, 0);
 	if (nBurnLayer & 4) if (!text_layer_enable) DrawTextLayer(1, 0, 0);
	if (nBurnLayer & 2) DrawLayer(DrvTMapROM1, scrollregs[1], DrvGfxROM3, gfxmask[3], 0x200, DrvTransTab[3], 0x0f, 4, 0);
	if (nBurnLayer & 4) if ( text_layer_enable) DrawTextLayer(1, 0, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 RsharkDraw()
{
	if (DrvRecalc) {
		DrvPaletteRecalc5RGB();
		DrvRecalc = 0;
	}

	UINT32 black = BurnDrvGetPaletteEntries();

	for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
		pTransDraw[i] = black;
	}

	if (nBurnLayer & 1) draw_layer_rshark(DrvTMapROM3, DrvTMapROM4 + 0x60000, scrollregs[0], DrvGfxROM5, gfxmask[5], 0x400, DrvTransTab[5], -1, 1);
	if ((nSpriteEnable & 1) && (priority_select == 0)) draw_sprites_rshark(0);
	if (nBurnLayer & 2) draw_layer_rshark(DrvTMapROM2, DrvTMapROM4 + 0x40000, scrollregs[2], DrvGfxROM4, gfxmask[4], 0x300, DrvTransTab[4], 0x0f, (priority_select) ? 2 : 1);
	if ((nSpriteEnable & 1) && (priority_select != 0)) draw_sprites_rshark(0);
	if (nBurnLayer & 4) draw_layer_rshark(DrvTMapROM1, DrvTMapROM4 + 0x20000, scrollregs[1], DrvGfxROM3, gfxmask[3], 0x200, DrvTransTab[3], 0x0f, 2);
	if (nBurnLayer & 8) draw_layer_rshark(DrvTMapROM0, DrvTMapROM4 + 0x00000, scrollregs[3], DrvGfxROM2, gfxmask[2], 0x100, DrvTransTab[2], 0x0f, 2);
	if (nSpriteEnable & 2) draw_sprites_rshark(1);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 PopbingoDraw()
{
	if (DrvRecalc) {
		DrvPaletteRecalc5RGB();
		DrvRecalc = 0;
	}

	UINT32 black = BurnDrvGetPaletteEntries();

	for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
		pTransDraw[i] = black;
	}

	if (nBurnLayer & 1) DrawLayer(DrvTMapROM0, scrollregs[0], DrvGfxROM2, gfxmask[2], 0x100, DrvTransTab[2], -1, 8, 1);
	if (nSpriteEnable & 1) draw_sprites_rshark(0);
	if (nSpriteEnable & 2) draw_sprites_rshark(1);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 LastdayFrame()
{
	if (DrvReset) {
		Z80YM2203DoReset();
	}

	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 3*sizeof(INT16));
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 100;
	INT32 nCyclesTotal[2] = { main_cpu_clock / 60, 8000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);
		if (gulf_storm) {
			if (i == 92) { ZetSetIRQLine(0, CPU_IRQSTATUS_ACK); vblank = 1; }
			if (i == 93) { ZetSetIRQLine(0, CPU_IRQSTATUS_NONE); vblank = 0; }
		} else {
			if (i == (nInterleave - 2)) ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
			if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
		}
		ZetClose();

		ZetOpen(1);
		BurnTimerUpdate((i + 1) * nCyclesTotal[1] / nInterleave);
		if (pollux_gulfstrm_irq_kicker_hack && (i % pollux_gulfstrm_irq_kicker_hack) == 0) { // ugly hack for pollux musix+sfx
			if (!(sound_irq_line[0] | sound_irq_line[1])) {
				ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
				ZetRun(60);
				ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			}
		}
		ZetClose();
	}

	ZetOpen(1);

	BurnTimerEndFrame(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	memcpy (DrvSprBuf, DrvSprRAM, 0x1000);

	return 0;
}

static INT32 FlytigerFrame()
{
	if (DrvReset) {
		Z80YM2151DoReset();
	}

	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 3*sizeof(INT16));
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nSegment = 0;
	INT32 nInterleave = 100;
	INT32 nSoundBufferPos = 0;
	INT32 nCyclesTotal[2] = { 8000000 / 60, 4000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);
		if (i == (nInterleave - 2)) ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
		ZetClose();

		ZetOpen(1);

		nCyclesDone[1] += ZetRun(nCyclesTotal[1] / nInterleave);

		if (pBurnSoundOut) {
			nSegment = nBurnSoundLen / nInterleave;

			BurnYM2151Render(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
			MSM6295Render(0, pBurnSoundOut + (nSoundBufferPos << 1), nSegment);

			nSoundBufferPos += nSegment;
		}

		ZetClose();
	}

	ZetOpen(1);

	if (pBurnSoundOut) {
		nSegment = nBurnSoundLen - nSoundBufferPos;
		if (nSegment > 0) {
			BurnYM2151Render(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
			MSM6295Render(0, pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
		}
	}

	ZetClose();

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	memcpy (DrvSprBuf, DrvSprRAM, 0x1000);

	return 0;
}

static inline void JoyClearOpposites(UINT8* nJoystickInputs)
{ // works on active-high
	if ((*nJoystickInputs & 0x03) == 0x03) {
		*nJoystickInputs &= ~0x03;
	}
	if ((*nJoystickInputs & 0x0c) == 0x0c) {
		*nJoystickInputs &= ~0x0c;
	}
}

static INT32 RsharkFrame()
{
	if (DrvReset) {
		Drv68KDoReset();
	}

	SekNewFrame();
	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 3*sizeof(INT16));
		DrvInputs[0] = 0;
		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}

		UINT8 *p1p2 = (UINT8 *)&DrvInputs[0];
		JoyClearOpposites(p1p2 + 0);
		JoyClearOpposites(p1p2 + 1);

		DrvInputs[0] ^= 0xffff; // convert to active-low
	}

	INT32 nSegment = 0;
	INT32 nInterleave = 256;
	INT32 nSoundBufferPos = 0;
	INT32 nCyclesTotal[2] = { main_cpu_clock / 60, 4000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone[0] += SekRun(nCyclesTotal[0] / nInterleave);
		if (i == 250) SekSetIRQLine(5, CPU_IRQSTATUS_AUTO);
		if (i == 120) SekSetIRQLine(6, CPU_IRQSTATUS_AUTO);

		nCyclesDone[1] += ZetRun(nCyclesTotal[1] / nInterleave);

		if (pBurnSoundOut) {
			nSegment = nBurnSoundLen / nInterleave;

			BurnYM2151Render(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
			MSM6295Render(0, pBurnSoundOut + (nSoundBufferPos << 1), nSegment);

			nSoundBufferPos += nSegment;
		}
	}

	if (pBurnSoundOut) {
		nSegment = nBurnSoundLen - nSoundBufferPos;
		if (nSegment > 0) {
			BurnYM2151Render(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
			MSM6295Render(0, pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
		}
	}

	SekClose();
	ZetClose();

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	memcpy (DrvSprBuf, DrvSprRAM, 0x1000);

	return 0;
}

static INT32 Z80YM2203Scan(INT32 nAction,INT32 *pnMin)
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

		BurnYM2203Scan(nAction, pnMin);

		SCAN_VAR(sprite_enable);
		SCAN_VAR(soundlatch);
		SCAN_VAR(priority_select);
		SCAN_VAR(text_layer_enable);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		bankswitch(z80_bank_select[0]);
		ZetClose();
	}

	return 0;
}

static INT32 Z80YM2151Scan(INT32 nAction,INT32 *pnMin)
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

		BurnYM2151Scan(nAction);
		MSM6295Scan(0, nAction);

		SCAN_VAR(sprite_enable);
		SCAN_VAR(soundlatch);
		SCAN_VAR(priority_select);
		SCAN_VAR(text_layer_enable);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		bankswitch(z80_bank_select[0]);
		ZetClose();
	}

	return 0;
}

static INT32 Drv68KScan(INT32 nAction,INT32 *pnMin)
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

		SekScan(nAction);
		ZetScan(nAction);

		BurnYM2151Scan(nAction);
		MSM6295Scan(0, nAction);

		SCAN_VAR(sprite_enable);
		SCAN_VAR(soundlatch);
		SCAN_VAR(priority_select);
		SCAN_VAR(text_layer_enable);
	}

	return 0;
}


// The Last Day (set 1)

static struct BurnRomInfo lastdayRomDesc[] = {
	{ "lday3.s5",	0x10000, 0xa06dfb1e, 1 | BRF_PRG | BRF_ESS }, //  0 Main CPU Code
	{ "4.u5",	0x10000, 0x70961ea6, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "1.d3",	0x10000, 0xdd4316fd, 2 | BRF_PRG | BRF_ESS }, //  2 Audio CPU Code

	{ "2.j4",	0x10000, 0x83eb572c, 3 | BRF_GRA },           //  3 Characters

	{ "16.d14",	0x20000, 0xdf503504, 4 | BRF_GRA },           //  4 Sprites
	{ "15.a14",	0x20000, 0xcd990442, 4 | BRF_GRA },           //  5

	{ "6.s9",	0x20000, 0x1054361d, 5 | BRF_GRA },           //  6 Tiles
	{ "9.s11",	0x20000, 0x6952ef4d, 5 | BRF_GRA },           //  7
	{ "7.u9",	0x20000, 0x6e57a888, 5 | BRF_GRA },           //  8
	{ "10.u11",	0x20000, 0xa5548dca, 5 | BRF_GRA },           //  9

	{ "12.s13",	0x20000, 0x992bc4af, 6 | BRF_GRA },           // 10 Tiles
	{ "14.s14",	0x20000, 0xa79abc85, 6 | BRF_GRA },           // 11

	{ "5.r9",	0x10000, 0x4789bae8, 7 | BRF_GRA },           // 12 Tiles
	{ "8.r11",	0x10000, 0x92402b9a, 7 | BRF_GRA },           // 13

	{ "11.r13",	0x10000, 0x04b961de, 8 | BRF_GRA },           // 14 Tiles
	{ "13.r14",	0x10000, 0x6bdbd887, 8 | BRF_GRA },           // 15
};

STD_ROM_PICK(lastday)
STD_ROM_FN(lastday)

struct BurnDriver BurnDrvLastday = {
	"lastday", NULL, NULL, NULL, "1990",
	"The Last Day (set 1)\0", NULL, "Dooyong", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, lastdayRomInfo, lastdayRomName, NULL, NULL, LastdayInputInfo, LastdayDIPInfo,
	LastdayInit, Z80YM2203Exit, LastdayFrame, LastdayDraw, Z80YM2203Scan, &DrvRecalc, 0x400,
	240, 384, 3, 4
};


// The Last Day (set 2)

static struct BurnRomInfo lastdayaRomDesc[] = {
	{ "lday3.s5",	0x10000, 0xa06dfb1e, 1 | BRF_PRG | BRF_ESS }, //  0 Main CPU Code
	{ "4.u5",	0x10000, 0x70961ea6, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "e1.d3",	0x10000, 0xce96e106, 2 | BRF_PRG | BRF_ESS }, //  2 Audio CPU Code

	{ "2.j4",	0x10000, 0x83eb572c, 3 | BRF_GRA },           //  3 Characters

	{ "16.d14",	0x20000, 0xdf503504, 4 | BRF_GRA },           //  4 Sprites
	{ "15.a14",	0x20000, 0xcd990442, 4 | BRF_GRA },           //  5

	{ "e6.s9",	0x20000, 0x7623c443, 5 | BRF_GRA },           //  6 Tiles
	{ "e9.s11",	0x20000, 0x717f6a0e, 5 | BRF_GRA },           //  7
	{ "7.u9",	0x20000, 0x6e57a888, 5 | BRF_GRA },           //  8
	{ "10.u11",	0x20000, 0xa5548dca, 5 | BRF_GRA },           //  9

	{ "12.s13",	0x20000, 0x992bc4af, 6 | BRF_GRA },           // 10 Tiles
	{ "14.s14",	0x20000, 0xa79abc85, 6 | BRF_GRA },           // 11

	{ "e5.r9",	0x10000, 0x5f801410, 7 | BRF_GRA },           // 12 Tiles
	{ "e8.r11",	0x10000, 0xa7b8250b, 7 | BRF_GRA },           // 13

	{ "11.r13",	0x10000, 0x04b961de, 8 | BRF_GRA },           // 14 Tiles
	{ "13.r14",	0x10000, 0x6bdbd887, 8 | BRF_GRA },           // 15
};

STD_ROM_PICK(lastdaya)
STD_ROM_FN(lastdaya)

struct BurnDriver BurnDrvLastdaya = {
	"lastdaya", "lastday", NULL, NULL, "1990",
	"The Last Day (set 2)\0", NULL, "Dooyong", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, lastdayaRomInfo, lastdayaRomName, NULL, NULL, LastdayInputInfo, LastdayDIPInfo,
	LastdayInit, Z80YM2203Exit, LastdayFrame, LastdayDraw, Z80YM2203Scan, &DrvRecalc, 0x400,
	240, 384, 3, 4
};


// Chulgyeok D-Day (Korea)

static struct BurnRomInfo ddaydooRomDesc[] = {
	{ "3.s5",	0x10000, 0x7817d4f3, 1 | BRF_PRG | BRF_ESS }, //  0 Main CPU Code
	{ "4.u5",	0x10000, 0x70961ea6, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "1.d3",	0x10000, 0xdd4316fd, 2 | BRF_PRG | BRF_ESS }, //  2 Audio CPU Code

	{ "2.j4",	0x10000, 0x83eb572c, 3 | BRF_GRA },           //  3 Characters

	{ "16.d14",	0x20000, 0xdf503504, 4 | BRF_GRA },           //  4 Sprites
	{ "15.a14",	0x20000, 0xcd990442, 4 | BRF_GRA },           //  5

	{ "6.s9",	0x20000, 0x1054361d, 5 | BRF_GRA },           //  6 Tiles
	{ "9.s11",	0x20000, 0x6952ef4d, 5 | BRF_GRA },           //  7
	{ "7.u9",	0x20000, 0x6e57a888, 5 | BRF_GRA },           //  8
	{ "10.u11",	0x20000, 0xa5548dca, 5 | BRF_GRA },           //  9

	{ "12.s13",	0x20000, 0x992bc4af, 6 | BRF_GRA },           // 10 Tiles
	{ "14.s14",	0x20000, 0xa79abc85, 6 | BRF_GRA },           // 11

	{ "5.r9",	0x10000, 0x4789bae8, 7 | BRF_GRA },           // 12 Tiles
	{ "8.r11",	0x10000, 0x92402b9a, 7 | BRF_GRA },           // 13

	{ "11.r13",	0x10000, 0x04b961de, 8 | BRF_GRA },           // 14 Tiles
	{ "13.r14",	0x10000, 0x6bdbd887, 8 | BRF_GRA },           // 15
};

STD_ROM_PICK(ddaydoo)
STD_ROM_FN(ddaydoo)

struct BurnDriver BurnDrvDdaydoo = {
	"ddaydoo", "lastday", NULL, NULL, "1990",
	"Chulgyeok D-Day (Korea)\0", NULL, "Dooyong", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, ddaydooRomInfo, ddaydooRomName, NULL, NULL, LastdayInputInfo, LastdayDIPInfo,
	LastdayInit, Z80YM2203Exit, LastdayFrame, LastdayDraw, Z80YM2203Scan, &DrvRecalc, 0x400,
	240, 384, 3, 4
};


// Gulf Storm (set 1)

static struct BurnRomInfo gulfstrmRomDesc[] = {
	{ "1.l4",	0x20000, 0x59e0478b, 1 | BRF_PRG | BRF_ESS }, //  0 Main CPU Code

	{ "3.c5",	0x10000, 0xc029b015, 2 | BRF_PRG | BRF_ESS }, //  1 Audio CPU Code

	{ "2.s4",	0x10000, 0xc2d65a25, 3 | BRF_GRA },           //  2 Characters

	{ "14.b1",	0x20000, 0x67bdf73d, 4 | BRF_GRA },           //  3 Sprites
	{ "16.c1",	0x20000, 0x7770a76f, 4 | BRF_GRA },           //  4
	{ "15.b1",	0x20000, 0x84803f7e, 4 | BRF_GRA },           //  5
	{ "17.e1",	0x20000, 0x94706500, 4 | BRF_GRA },           //  6

	{ "4.d8",	0x20000, 0x858fdbb6, 5 | BRF_GRA },           //  7 Tiles
	{ "5.b9",	0x20000, 0xc0a552e8, 5 | BRF_GRA },           //  8
	{ "6.d8",	0x20000, 0x20eedda3, 5 | BRF_GRA },           //  9
	{ "7.d9",	0x20000, 0x294f8c40, 5 | BRF_GRA },           // 10

	{ "12.r8",	0x20000, 0xec3ad3e7, 6 | BRF_GRA },           // 11 Tiles
	{ "13.r9",	0x20000, 0xc64090cb, 6 | BRF_GRA },           // 12

	{ "8.e8",	0x10000, 0x8d7f4693, 7 | BRF_GRA },           // 13 Tiles
	{ "9.e9",	0x10000, 0x34d440c4, 7 | BRF_GRA },           // 14

	{ "10.n8",	0x10000, 0xb4f15bf4, 8 | BRF_GRA },           // 15 Tiles
	{ "11.n9",	0x10000, 0x7dfe4a9c, 8 | BRF_GRA },           // 16
};

STD_ROM_PICK(gulfstrm)
STD_ROM_FN(gulfstrm)

struct BurnDriver BurnDrvGulfstrm = {
	"gulfstrm", NULL, NULL, NULL, "1991",
	"Gulf Storm (set 1)\0", NULL, "Dooyong", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, gulfstrmRomInfo, gulfstrmRomName, NULL, NULL, GulfstrmInputInfo, GulfstrmDIPInfo,
	GulfstrmInit, Z80YM2203Exit, LastdayFrame, GulfstrmDraw, Z80YM2203Scan, &DrvRecalc, 0x400,
	240, 384, 3, 4
};


// Gulf Storm (set 2)

static struct BurnRomInfo gulfstrmaRomDesc[] = {
	{ "1.bin",	0x20000, 0xd04fb06b, 1 | BRF_PRG | BRF_ESS }, //  0 Main CPU Code

	{ "3.c5",	0x10000, 0xc029b015, 2 | BRF_PRG | BRF_ESS }, //  1 Audio CPU Code

	{ "2.s4",	0x10000, 0xc2d65a25, 3 | BRF_GRA },           //  2 Characters

	{ "14.b1",	0x20000, 0x67bdf73d, 4 | BRF_GRA },           //  3 Sprites
	{ "16.c1",	0x20000, 0x7770a76f, 4 | BRF_GRA },           //  4
	{ "15.b1",	0x20000, 0x84803f7e, 4 | BRF_GRA },           //  5
	{ "17.e1",	0x20000, 0x94706500, 4 | BRF_GRA },           //  6

	{ "4.d8",	0x20000, 0x858fdbb6, 5 | BRF_GRA },           //  7 Tiles
	{ "5.b9",	0x20000, 0xc0a552e8, 5 | BRF_GRA },           //  8
	{ "6.d8",	0x20000, 0x20eedda3, 5 | BRF_GRA },           //  9
	{ "7.d9",	0x20000, 0x294f8c40, 5 | BRF_GRA },           // 10

	{ "12.bin",	0x20000, 0x3e3d3b57, 6 | BRF_GRA },           // 11 Tiles
	{ "13.bin",	0x20000, 0x66fcce80, 6 | BRF_GRA },           // 12

	{ "8.e8",	0x10000, 0x8d7f4693, 7 | BRF_GRA },           // 13 Tiles
	{ "9.e9",	0x10000, 0x34d440c4, 7 | BRF_GRA },           // 14

	{ "10.bin",	0x10000, 0x08149140, 8 | BRF_GRA },           // 15 Tiles
	{ "11.bin",	0x10000, 0x2ed7545b, 8 | BRF_GRA },           // 16
};

STD_ROM_PICK(gulfstrma)
STD_ROM_FN(gulfstrma)

struct BurnDriver BurnDrvGulfstrma = {
	"gulfstrma", "gulfstrm", NULL, NULL, "1991",
	"Gulf Storm (set 2)\0", NULL, "Dooyong", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, gulfstrmaRomInfo, gulfstrmaRomName, NULL, NULL, GulfstrmInputInfo, GulfstrmDIPInfo,
	GulfstrmInit, Z80YM2203Exit, LastdayFrame, GulfstrmDraw, Z80YM2203Scan, &DrvRecalc, 0x400,
	240, 384, 3, 4
};


// Gulf Storm (set 3)

static struct BurnRomInfo gulfstrmbRomDesc[] = {
	{ "1.l4",	0x20000, 0xaabd95a5, 1 | BRF_PRG | BRF_ESS }, //  0 Main CPU Code

	{ "3.c5",	0x10000, 0xc029b015, 2 | BRF_PRG | BRF_ESS }, //  1 Audio CPU Code

	{ "2.s4",	0x10000, 0xc2d65a25, 3 | BRF_GRA },           //  2 Characters

	{ "14.b1",	0x20000, 0x67bdf73d, 4 | BRF_GRA },           //  3 Sprites
	{ "16.c1",	0x20000, 0x7770a76f, 4 | BRF_GRA },           //  4
	{ "15.b1",	0x20000, 0x84803f7e, 4 | BRF_GRA },           //  5
	{ "17.e1",	0x20000, 0x94706500, 4 | BRF_GRA },           //  6

	{ "4.d8",	0x20000, 0x858fdbb6, 5 | BRF_GRA },           //  7 Tiles
	{ "5.b9",	0x20000, 0xc0a552e8, 5 | BRF_GRA },           //  8
	{ "6.d8",	0x20000, 0x20eedda3, 5 | BRF_GRA },           //  9
	{ "7.d9",	0x20000, 0x294f8c40, 5 | BRF_GRA },           // 10

	{ "12.bin",	0x20000, 0x3e3d3b57, 6 | BRF_GRA },           // 11 Tiles
	{ "13.bin",	0x20000, 0x66fcce80, 6 | BRF_GRA },           // 12

	{ "8.e8",	0x10000, 0x8d7f4693, 7 | BRF_GRA },           // 13 Tiles
	{ "9.e9",	0x10000, 0x34d440c4, 7 | BRF_GRA },           // 14

	{ "10.bin",	0x10000, 0x08149140, 8 | BRF_GRA },           // 15 Tiles
	{ "11.bin",	0x10000, 0x2ed7545b, 8 | BRF_GRA },           // 16
};

STD_ROM_PICK(gulfstrmb)
STD_ROM_FN(gulfstrmb)

struct BurnDriver BurnDrvGulfstrmb = {
	"gulfstrmb", "gulfstrm", NULL, NULL, "1991",
	"Gulf Storm (set 3)\0", NULL, "Dooyong", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, gulfstrmbRomInfo, gulfstrmbRomName, NULL, NULL, GulfstrmInputInfo, GulfstrmDIPInfo,
	GulfstrmInit, Z80YM2203Exit, LastdayFrame, GulfstrmDraw, Z80YM2203Scan, &DrvRecalc, 0x400,
	240, 384, 3, 4
};


// Gulf Storm (Media Shoji)

static struct BurnRomInfo gulfstrmmRomDesc[] = {
	{ "18.l4",	0x20000, 0xd38e2667, 1 | BRF_PRG | BRF_ESS }, //  0 Main CPU Code

	{ "3.c5",	0x10000, 0xc029b015, 2 | BRF_PRG | BRF_ESS }, //  1 Audio CPU Code

	{ "2.bin",	0x10000, 0xcb555d96, 3 | BRF_GRA },           //  2 Characters

	{ "14.b1",	0x20000, 0x67bdf73d, 4 | BRF_GRA },           //  3 Sprites
	{ "16.c1",	0x20000, 0x7770a76f, 4 | BRF_GRA },           //  4
	{ "15.b1",	0x20000, 0x84803f7e, 4 | BRF_GRA },           //  5
	{ "17.e1",	0x20000, 0x94706500, 4 | BRF_GRA },           //  6

	{ "4.d8",	0x20000, 0x858fdbb6, 5 | BRF_GRA },           //  7 Tiles
	{ "5.b9",	0x20000, 0xc0a552e8, 5 | BRF_GRA },           //  8
	{ "6.d8",	0x20000, 0x20eedda3, 5 | BRF_GRA },           //  9
	{ "7.d9",	0x20000, 0x294f8c40, 5 | BRF_GRA },           // 10

	{ "12.bin",	0x20000, 0x3e3d3b57, 6 | BRF_GRA },           // 11 Tiles
	{ "13.bin",	0x20000, 0x66fcce80, 6 | BRF_GRA },           // 12

	{ "8.e8",	0x10000, 0x8d7f4693, 7 | BRF_GRA },           // 13 Tiles
	{ "9.e9",	0x10000, 0x34d440c4, 7 | BRF_GRA },           // 14

	{ "10.bin",	0x10000, 0x08149140, 8 | BRF_GRA },           // 15 Tiles
	{ "11.bin",	0x10000, 0x2ed7545b, 8 | BRF_GRA },           // 16
};

STD_ROM_PICK(gulfstrmm)
STD_ROM_FN(gulfstrmm)

struct BurnDriver BurnDrvGulfstrmm = {
	"gulfstrmm", "gulfstrm", NULL, NULL, "1991",
	"Gulf Storm (Media Shoji)\0", NULL, "Dooyong (Media Shoji license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, gulfstrmmRomInfo, gulfstrmmRomName, NULL, NULL, GulfstrmInputInfo, GulfstrmDIPInfo,
	GulfstrmInit, Z80YM2203Exit, LastdayFrame, GulfstrmDraw, Z80YM2203Scan, &DrvRecalc, 0x400,
	240, 384, 3, 4
};


// Gulf Storm (Korea)

static struct BurnRomInfo gulfstrmkRomDesc[] = {
	{ "18.4L",	0x20000, 0x02bcf56d, 1 | BRF_PRG | BRF_ESS }, //  0 Main CPU Code

	{ "3.c5",	0x10000, 0xc029b015, 2 | BRF_PRG | BRF_ESS }, //  1 Audio CPU Code

	{ "2.bin",	0x10000, 0xcb555d96, 3 | BRF_GRA },           //  2 Characters

	{ "14.b1",	0x20000, 0x67bdf73d, 4 | BRF_GRA },           //  3 Sprites
	{ "16.c1",	0x20000, 0x7770a76f, 4 | BRF_GRA },           //  4
	{ "15.b1",	0x20000, 0x84803f7e, 4 | BRF_GRA },           //  5
	{ "17.e1",	0x20000, 0x94706500, 4 | BRF_GRA },           //  6

	{ "4.d8",	0x20000, 0x858fdbb6, 5 | BRF_GRA },           //  7 Tiles
	{ "5.b9",	0x20000, 0xc0a552e8, 5 | BRF_GRA },           //  8
	{ "6.d8",	0x20000, 0x20eedda3, 5 | BRF_GRA },           //  9
	{ "7.d9",	0x20000, 0x294f8c40, 5 | BRF_GRA },           // 10

	{ "12.bin",	0x20000, 0x3e3d3b57, 6 | BRF_GRA },           // 11 Tiles
	{ "13.bin",	0x20000, 0x66fcce80, 6 | BRF_GRA },           // 12

	{ "8.e8",	0x10000, 0x8d7f4693, 7 | BRF_GRA },           // 13 Tiles
	{ "9.e9",	0x10000, 0x34d440c4, 7 | BRF_GRA },           // 14

	{ "10.bin",	0x10000, 0x08149140, 8 | BRF_GRA },           // 15 Tiles
	{ "11.bin",	0x10000, 0x2ed7545b, 8 | BRF_GRA },           // 16
};

STD_ROM_PICK(gulfstrmk)
STD_ROM_FN(gulfstrmk)

struct BurnDriver BurnDrvGulfstrmk = {
	"gulfstrmk", "gulfstrm", NULL, NULL, "1991",
	"Gulf Storm (Korea)\0", NULL, "Dooyong", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, gulfstrmkRomInfo, gulfstrmkRomName, NULL, NULL, GulfstrmInputInfo, GulfstrmDIPInfo,
	GulfstrmInit, Z80YM2203Exit, LastdayFrame, GulfstrmDraw, Z80YM2203Scan, &DrvRecalc, 0x400,
	240, 384, 3, 4
};


// Pollux (set 1)

static struct BurnRomInfo polluxRomDesc[] = {
	{ "pollux2.bin",	0x10000, 0x45e10d4e, 1 | BRF_PRG | BRF_ESS }, //  0 Main CPU Code

	{ "pollux3.bin",	0x10000, 0x85a9dc98, 2 | BRF_PRG | BRF_ESS }, //  1 Audio CPU Code

	{ "pollux1.bin",	0x10000, 0x7f7135da, 3 | BRF_GRA },           //  2 Characters

	{ "dy-pl-m2_be023.bin",	0x80000, 0xbdea6f7d, 4 | BRF_GRA },           //  3 Sprites

	{ "dy-pl-m1_be015.bin",	0x80000, 0x1d2dedd2, 5 | BRF_GRA },           //  4 Tiles

	{ "pollux6.bin",	0x20000, 0xb0391db5, 6 | BRF_GRA },           //  5 Tiles
	{ "pollux7.bin",	0x20000, 0x632f6e10, 6 | BRF_GRA },           //  6

	{ "pollux9.bin",	0x10000, 0x378d8914, 7 | BRF_GRA },           //  7 Tiles
	{ "pollux8.bin",	0x10000, 0x8859fa70, 7 | BRF_GRA },           //  8

	{ "pollux5.bin",	0x10000, 0xac090d34, 8 | BRF_GRA },           //  9 Tiles
	{ "pollux4.bin",	0x10000, 0x2c6bd3be, 8 | BRF_GRA },           // 10
};

STD_ROM_PICK(pollux)
STD_ROM_FN(pollux)

struct BurnDriver BurnDrvPollux = {
	"pollux", NULL, NULL, NULL, "1991",
	"Pollux (set 1)\0", NULL, "Dooyong", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, polluxRomInfo, polluxRomName, NULL, NULL, PolluxInputInfo, PolluxDIPInfo,
	PolluxInit, Z80YM2203Exit, LastdayFrame, PolluxDraw, Z80YM2203Scan, &DrvRecalc, 0x400,
	240, 384, 3, 4
};


// Pollux (set 2)

static struct BurnRomInfo polluxaRomDesc[] = {
	{ "dooyong2.bin",	0x10000, 0xe4ea8dbd, 1 | BRF_PRG | BRF_ESS }, //  0 Main CPU Code

	{ "pollux3.bin",	0x10000, 0x85a9dc98, 2 | BRF_PRG | BRF_ESS }, //  1 Audio CPU Code

	{ "dooyong1.bin",	0x10000, 0xa7d820b2, 3 | BRF_GRA },           //  2 Characters

	{ "dy-pl-m2_be023.bin",	0x80000, 0xbdea6f7d, 4 | BRF_GRA },           //  3 Sprites

	{ "dy-pl-m1_be015.bin",	0x80000, 0x1d2dedd2, 5 | BRF_GRA },           //  4 Tiles

	{ "pollux6.bin",	0x20000, 0xb0391db5, 6 | BRF_GRA },           //  5 Tiles
	{ "pollux7.bin",	0x20000, 0x632f6e10, 6 | BRF_GRA },           //  6

	{ "pollux9.bin",	0x10000, 0x378d8914, 7 | BRF_GRA },           //  7 Tiles
	{ "pollux8.bin",	0x10000, 0x8859fa70, 7 | BRF_GRA },           //  8

	{ "pollux5.bin",	0x10000, 0xac090d34, 8 | BRF_GRA },           //  9 Tiles
	{ "pollux4.bin",	0x10000, 0x2c6bd3be, 8 | BRF_GRA },           // 10
};

STD_ROM_PICK(polluxa)
STD_ROM_FN(polluxa)

struct BurnDriver BurnDrvPolluxa = {
	"polluxa", "pollux", NULL, NULL, "1991",
	"Pollux (set 2)\0", NULL, "Dooyong", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, polluxaRomInfo, polluxaRomName, NULL, NULL, PolluxInputInfo, PolluxDIPInfo,
	PolluxInit, Z80YM2203Exit, LastdayFrame, PolluxDraw, Z80YM2203Scan, &DrvRecalc, 0x400,
	240, 384, 3, 4
};


// Pollux (set 3)

static struct BurnRomInfo polluxa2RomDesc[] = {
	{ "dooyong16_tms27c512.bin",	0x10000, 0xdffe5173, 1 | BRF_PRG | BRF_ESS }, //  0 Main CPU Code

	{ "pollux3.bin",	0x10000, 0x85a9dc98, 2 | BRF_PRG | BRF_ESS }, //  1 Audio CPU Code

	{ "dooyong1.bin",	0x10000, 0xa7d820b2, 3 | BRF_GRA },           //  2 Characters

	{ "dy-pl-m2_be023.bin",	0x80000, 0xbdea6f7d, 4 | BRF_GRA },           //  3 Sprites

	{ "dy-pl-m1_be015.bin",	0x80000, 0x1d2dedd2, 5 | BRF_GRA },           //  4 Tiles

	{ "pollux6.bin",	0x20000, 0xb0391db5, 6 | BRF_GRA },           //  5 Tiles
	{ "pollux7.bin",	0x20000, 0x632f6e10, 6 | BRF_GRA },           //  6

	{ "pollux9.bin",	0x10000, 0x378d8914, 7 | BRF_GRA },           //  7 Tiles
	{ "pollux8.bin",	0x10000, 0x8859fa70, 7 | BRF_GRA },           //  8

	{ "pollux5.bin",	0x10000, 0xac090d34, 8 | BRF_GRA },           //  9 Tiles
	{ "pollux4.bin",	0x10000, 0x2c6bd3be, 8 | BRF_GRA },           // 10
};

STD_ROM_PICK(polluxa2)
STD_ROM_FN(polluxa2)

struct BurnDriver BurnDrvPolluxa2 = {
	"polluxa2", "pollux", NULL, NULL, "1991",
	"Pollux (set 3)\0", NULL, "Dooyong", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, polluxa2RomInfo, polluxa2RomName, NULL, NULL, PolluxInputInfo, PolluxDIPInfo,
	PolluxInit, Z80YM2203Exit, LastdayFrame, PolluxDraw, Z80YM2203Scan, &DrvRecalc, 0x400,
	240, 384, 3, 4
};


// Pollux (Japan, NTC license)

static struct BurnRomInfo polluxnRomDesc[] = {
	{ "polluxntc_2.3g",			0x10000, 0x96d3e3af, 1 | BRF_PRG | BRF_ESS }, //  0 Main CPU Code

	{ "polluxntc_3.6t",			0x10000, 0x85a9dc98, 2 | BRF_PRG | BRF_ESS }, //  1 Audio CPU Code

	{ "polluxntc_1.3r",			0x10000, 0x7f7135da, 3 | BRF_GRA },           //  2 Characters

	{ "polluxntc_dy-pl-m2_be023.3t",	0x80000, 0xbdea6f7d, 4 | BRF_GRA },           //  3 Sprites

	{ "polluxntc_dy-pl-m1_be002.8a",	0x80000, 0x1d2dedd2, 5 | BRF_GRA },           //  4 Tiles

	{ "polluxntc_6.8m",			0x20000, 0xb0391db5, 6 | BRF_GRA },           //  5 Tiles
	{ "polluxntc_7.8l",			0x20000, 0x632f6e10, 6 | BRF_GRA },           //  6

	{ "polluxntc_9.8b",			0x10000, 0x378d8914, 7 | BRF_GRA },           //  7 Tiles
	{ "polluxntc_8.8j",			0x10000, 0x8859fa70, 7 | BRF_GRA },           //  8

	{ "polluxntc_5.8p",			0x10000, 0xac090d34, 8 | BRF_GRA },           //  9 Tiles
	{ "polluxntc_4.8r",			0x10000, 0x0195dc4e, 8 | BRF_GRA },           // 10
};

STD_ROM_PICK(polluxn)
STD_ROM_FN(polluxn)

struct BurnDriver BurnDrvPolluxn = {
	"polluxn", "pollux", NULL, NULL, "1991",
	"Pollux (Japan, NTC license)\0", NULL, "Dooyong (NTC license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, polluxnRomInfo, polluxnRomName, NULL, NULL, PolluxInputInfo, PolluxDIPInfo,
	PolluxInit, Z80YM2203Exit, LastdayFrame, PolluxDraw, Z80YM2203Scan, &DrvRecalc, 0x400,
	240, 384, 3, 4
};


// Flying Tiger (set 1)

static struct BurnRomInfo flytigerRomDesc[] = {
	{ "1.3c",		0x20000, 0x2d634c8e, 1 | BRF_PRG | BRF_ESS }, //  0 Main CPU Code

	{ "3.6p",		0x10000, 0xd238df5e, 2 | BRF_PRG | BRF_ESS }, //  1 Audio CPU Code

	{ "2.4h",		0x10000, 0x2fb72912, 3 | BRF_GRA },           //  2 Characters

	{ "16.4h",		0x20000, 0x8a158b95, 4 | BRF_GRA },           //  3 Sprites
	{ "15.2h",		0x20000, 0x399f6043, 4 | BRF_GRA },           //  4
	{ "14.4k",		0x20000, 0xdf66b6f3, 4 | BRF_GRA },           //  5
	{ "13.2k",		0x20000, 0xf24a5099, 4 | BRF_GRA },           //  6

	{ "dy-ft-m1.11n",	0x80000, 0xf06589c2, 5 | BRF_GRA },           //  7 Tiles

	{ "dy-ft-m2.11g",	0x80000, 0x7545f9c9, 6 | BRF_GRA },           //  8 Tiles

	{ "4.9n",		0x20000, 0xcd95cf9a, 7 | BRF_SND },           //  9 Samples
};

STD_ROM_PICK(flytiger)
STD_ROM_FN(flytiger)

static INT32 FlytigerInit()
{
	return FlytigerCommonInit(0);
}

struct BurnDriver BurnDrvFlytiger = {
	"flytiger", NULL, NULL, NULL, "1992",
	"Flying Tiger (set 1)\0", NULL, "Dooyong", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, flytigerRomInfo, flytigerRomName, NULL, NULL, BluehawkInputInfo, FlytigerDIPInfo,
	FlytigerInit, Z80YM2151Exit, FlytigerFrame, FlytigerDraw, Z80YM2151Scan, &DrvRecalc, 0x400,
	240, 384, 3, 4
};


// Flying Tiger (set 2)

static struct BurnRomInfo flytigeraRomDesc[] = {
	{ "ftiger_1.3c",	0x20000, 0x02acd1ce, 1 | BRF_PRG | BRF_ESS }, //  0 Main CPU Code

	{ "ftiger_11.6p",	0x10000, 0xd238df5e, 2 | BRF_PRG | BRF_ESS }, //  1 Audio CPU Code

	{ "ftiger_2.4h",	0x10000, 0xca9d6713, 3 | BRF_GRA },           //  2 Characters

	{ "ftiger_16.4h",	0x20000, 0x8a158b95, 4 | BRF_GRA },           //  3 Sprites
	{ "ftiger_15.2h",	0x20000, 0x399f6043, 4 | BRF_GRA },           //  4
	{ "ftiger_14.4k",	0x20000, 0xdf66b6f3, 4 | BRF_GRA },           //  5
	{ "ftiger_13.2k",	0x20000, 0xf24a5099, 4 | BRF_GRA },           //  6

	{ "ftiger_3.10p",	0x20000, 0x9fc12ebd, 5 | BRF_GRA },           //  7 Tiles
	{ "ftiger_5.10l",	0x20000, 0x06c9dd2a, 5 | BRF_GRA },           //  8
	{ "ftiger_4.11p",	0x20000, 0xfb30e884, 5 | BRF_GRA },           //  9
	{ "ftiger_6.11l",	0x20000, 0xdfb85152, 5 | BRF_GRA },           // 10

	{ "ftiger_8.11h",	0x20000, 0xcbd8c22f, 6 | BRF_GRA },           // 11 Tiles
	{ "ftiger_10.11f",	0x20000, 0xe2175f3b, 6 | BRF_GRA },           // 12
	{ "ftiger_7.10h",	0x20000, 0xbe431c61, 6 | BRF_GRA },           // 13
	{ "ftiger_9.10f",	0x20000, 0x91bcd84f, 6 | BRF_GRA },           // 14

	{ "ftiger_12.9n",	0x20000, 0xcd95cf9a, 7 | BRF_SND },           // 15 Samples
};

STD_ROM_PICK(flytigera)
STD_ROM_FN(flytigera)

static INT32 FlytigeraInit()
{
	return FlytigerCommonInit(1);
}

struct BurnDriver BurnDrvFlytigera = {
	"flytigera", "flytiger", NULL, NULL, "1992",
	"Flying Tiger (set 2)\0", NULL, "Dooyong", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, flytigeraRomInfo, flytigeraRomName, NULL, NULL, BluehawkInputInfo, FlytigerDIPInfo,
	FlytigeraInit, Z80YM2151Exit, FlytigerFrame, FlytigerDraw, Z80YM2151Scan, &DrvRecalc, 0x400,
	240, 384, 3, 4
};


// Blue Hawk

static struct BurnRomInfo bluehawkRomDesc[] = {
	{ "rom19",	0x20000, 0x24149246, 1 | BRF_PRG | BRF_ESS }, //  0 Main CPU Code

	{ "rom1",	0x10000, 0xeef22920, 2 | BRF_PRG | BRF_ESS }, //  1 Audio CPU Code

	{ "rom3",	0x10000, 0xc192683f, 3 | BRF_GRA },           //  2 Characters

	{ "dy-bh-m3",	0x80000, 0x8809d157, 4 | BRF_GRA },           //  3 Sprites

	{ "dy-bh-m1",	0x80000, 0x51816b2c, 5 | BRF_GRA },           //  4 Tiles

	{ "dy-bh-m2",	0x80000, 0xf9daace6, 6 | BRF_GRA },           //  5 Tiles

	{ "rom6",	0x20000, 0xe6bd9daa, 7 | BRF_GRA },           //  6 Tiles
	{ "rom5",	0x20000, 0x5c654dc6, 7 | BRF_GRA },           //  7

	{ "rom4",	0x20000, 0xf7318919, 8 | BRF_SND },           //  8 Samples
};

STD_ROM_PICK(bluehawk)
STD_ROM_FN(bluehawk)

struct BurnDriver BurnDrvBluehawk = {
	"bluehawk", NULL, NULL, NULL, "1993",
	"Blue Hawk\0", NULL, "Dooyong", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, bluehawkRomInfo, bluehawkRomName, NULL, NULL, BluehawkInputInfo, BluehawkDIPInfo,
	BluehawkInit, Z80YM2151Exit, FlytigerFrame, BluehawkDraw, Z80YM2151Scan, &DrvRecalc, 0x400,
	240, 384, 3, 4
};


// Blue Hawk (NTC)

static struct BurnRomInfo bluehawknRomDesc[] = {
	{ "rom19",	0x20000, 0x24149246, 1 | BRF_PRG | BRF_ESS }, //  0 Main CPU Code

	{ "rom1",	0x10000, 0xeef22920, 2 | BRF_PRG | BRF_ESS }, //  1 Audio CPU Code

	{ "rom3ntc",	0x10000, 0x31eb221a, 3 | BRF_GRA },           //  2 Characters

	{ "dy-bh-m3",	0x80000, 0x8809d157, 4 | BRF_GRA },           //  3 Sprites

	{ "dy-bh-m1",	0x80000, 0x51816b2c, 5 | BRF_GRA },           //  4 Tiles

	{ "dy-bh-m2",	0x80000, 0xf9daace6, 6 | BRF_GRA },           //  5 Tiles

	{ "rom6",	0x20000, 0xe6bd9daa, 7 | BRF_GRA },           //  6 Tiles
	{ "rom5",	0x20000, 0x5c654dc6, 7 | BRF_GRA },           //  7

	{ "rom4",	0x20000, 0xf7318919, 8 | BRF_SND },           //  8 Samples
};

STD_ROM_PICK(bluehawkn)
STD_ROM_FN(bluehawkn)

struct BurnDriver BurnDrvBluehawkn = {
	"bluehawkn", "bluehawk", NULL, NULL, "1993",
	"Blue Hawk (NTC)\0", NULL, "Dooyong (NTC license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, bluehawknRomInfo, bluehawknRomName, NULL, NULL, BluehawkInputInfo, BluehawkDIPInfo,
	BluehawkInit, Z80YM2151Exit, FlytigerFrame, BluehawkDraw, Z80YM2151Scan, &DrvRecalc, 0x400,
	240, 384, 3, 4
};


// Sadari

static struct BurnRomInfo sadariRomDesc[] = {
	{ "1.3d",	0x20000, 0xbd953217, 1 | BRF_PRG | BRF_ESS }, //  0 Main CPU Code

	{ "3.6r",	0x10000, 0x4786fca6, 2 | BRF_PRG | BRF_ESS }, //  1 Audio CPU Code

	{ "2.4c",	0x20000, 0xb2a3f1c6, 3 | BRF_GRA },           //  2 Characters

	{ "10.10l",	0x20000, 0x70269ab1, 4 | BRF_GRA },           //  3 Sprites
	{ "5.8l",	0x20000, 0xceceb4c3, 4 | BRF_GRA },           //  4
	{ "9.10n",	0x20000, 0x21bd1bda, 4 | BRF_GRA },           //  5
	{ "4.8n",	0x20000, 0xcd318ae5, 4 | BRF_GRA },           //  6

	{ "11.10j",	0x20000, 0x62a1d580, 5 | BRF_GRA },           //  7 Tiles
	{ "6.8j",	0x20000, 0xc4b13ed7, 5 | BRF_GRA },           //  8
	{ "12.10g",	0x20000, 0x547b7645, 5 | BRF_GRA },           //  9
	{ "7.8g",	0x20000, 0x14f20fa3, 5 | BRF_GRA },           // 10

	{ "8.10r",	0x20000, 0x9c29a093, 6 | BRF_SND },           // 11 Samples
};

STD_ROM_PICK(sadari)
STD_ROM_FN(sadari)

static INT32 SadariInit()
{
	return PrimellaCommonInit(0);
}

struct BurnDriver BurnDrvSadari = {
	"sadari", NULL, NULL, NULL, "1993",
	"Sadari\0", NULL, "Dooyong (NTC license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, sadariRomInfo, sadariRomName, NULL, NULL, BluehawkInputInfo, PrimellaDIPInfo,
	SadariInit, Z80YM2151Exit, FlytigerFrame, PrimellaDraw, Z80YM2151Scan, &DrvRecalc, 0x400,
	384, 256, 4, 3
};


// Gun Dealer '94

static struct BurnRomInfo gundl94RomDesc[] = {
	{ "gd94_001.d3",	0x20000, 0x3a5cc045, 1 | BRF_PRG | BRF_ESS }, //  0 Main CPU Code

	{ "gd94_003.r6",	0x10000, 0xea41c4ad, 2 | BRF_PRG | BRF_ESS }, //  1 Audio CPU Code

	{ "gd94_002.c5",	0x20000, 0x8575e64b, 3 | BRF_GRA },           //  2 Characters

	{ "gd94_009.n9",	0x20000, 0x40eabf55, 4 | BRF_GRA },           //  3 Sprites
	{ "gd94_004.n7",	0x20000, 0x0654abb9, 4 | BRF_GRA },           //  4

	{ "gd94_012.g9",	0x20000, 0x117c693c, 5 | BRF_GRA },           //  5 Tiles
	{ "gd94_007.g7",	0x20000, 0x96a72c6d, 5 | BRF_GRA },           //  6

	{ "gd94_008.r9",	0x20000, 0xf92e5803, 6 | BRF_SND },           //  7 Samples

	{ "gd94_011.j9",	0x20000, 0xd8ad0208, 7 | BRF_OPT },           //  8 cpu2

	{ "gd94_006.j7",	0x20000, 0x1d9536fe, 8 | BRF_OPT },           //  9 Tiles
	{ "gd94_010.l7",	0x20000, 0x4b74857f, 8 | BRF_OPT },           // 10
};

STD_ROM_PICK(gundl94)
STD_ROM_FN(gundl94)

static INT32 PrimellaInit()
{
	return PrimellaCommonInit(1);
}

struct BurnDriver BurnDrvGundl94 = {
	"gundl94", NULL, NULL, NULL, "1994",
	"Gun Dealer '94\0", NULL, "Dooyong", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, gundl94RomInfo, gundl94RomName, NULL, NULL, BluehawkInputInfo, PrimellaDIPInfo,
	PrimellaInit, Z80YM2151Exit, FlytigerFrame, PrimellaDraw, Z80YM2151Scan, &DrvRecalc, 0x400,
	384, 256, 4, 3
};


// Primella

static struct BurnRomInfo primellaRomDesc[] = {
	{ "1_d3.bin",		0x20000, 0x82fea4e0, 1 | BRF_PRG | BRF_ESS }, //  0 Main CPU Code

	{ "gd94_003.r6",	0x10000, 0xea41c4ad, 2 | BRF_PRG | BRF_ESS }, //  1 Audio CPU Code

	{ "gd94_002.c5",	0x20000, 0x8575e64b, 3 | BRF_GRA },           //  2 Characters

	{ "7_n9.bin",		0x20000, 0x20b6a574, 4 | BRF_GRA },           //  3 Sprites
	{ "4_n7.bin",		0x20000, 0xfe593666, 4 | BRF_GRA },           //  4

	{ "8_g9.bin",		0x20000, 0x542ecb83, 5 | BRF_GRA },           //  5 Tiles
	{ "5_g7.bin",		0x20000, 0x058ecac6, 5 | BRF_GRA },           //  6

	{ "gd94_008.r9",	0x20000, 0xf92e5803, 6 | BRF_SND },           //  7 Samples
};

STD_ROM_PICK(primella)
STD_ROM_FN(primella)

struct BurnDriver BurnDrvPrimella = {
	"primella", "gundl94", NULL, NULL, "1994",
	"Primella\0", NULL, "Dooyong (NTC license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, primellaRomInfo, primellaRomName, NULL, NULL, BluehawkInputInfo, PrimellaDIPInfo,
	PrimellaInit, Z80YM2151Exit, FlytigerFrame, PrimellaDraw, Z80YM2151Scan, &DrvRecalc, 0x400,
	384, 256, 4, 3
};


// Super-X (NTC)

static struct BurnRomInfo superxRomDesc[] = {
	{ "2.3m",		0x020000, 0xbe7aebe7, 1 | BRF_PRG | BRF_ESS }, //  0 Main CPU Code
	{ "3.3l",		0x020000, 0xdc4a25fc, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "1.5u",		0x010000, 0x6894ce05, 2 | BRF_PRG | BRF_ESS }, //  2 Audio CPU Code

	{ "spxo-m05.10m",	0x200000, 0x9120dd84, 3 | BRF_GRA },           //  3 Characters

	{ "spxb-m04.8f",	0x100000, 0x91a7ac6e, 4 | BRF_GRA },           //  4 Sprites

	{ "spxb-m03.8j",	0x100000, 0x8b42861b, 5 | BRF_GRA },           //  5 Tiles

	{ "spxb-m02.8a",	0x100000, 0x21b8db78, 6 | BRF_GRA },           //  6 Tiles

	{ "spxb-m01.8c",	0x100000, 0x60c69129, 7 | BRF_GRA },           //  7 Tiles

	{ "spxb-ms3.10f",	0x020000, 0x8bf8c77d, 8 | BRF_GRA },           //  8 Tiles
	{ "spxb-ms4.10j",	0x020000, 0xd418a900, 8 | BRF_GRA },           //  9
	{ "spxb-ms2.10a",	0x020000, 0x5ec87adf, 8 | BRF_GRA },           // 10
	{ "spxb-ms1.10c",	0x020000, 0x40b4fe6c, 8 | BRF_GRA },           // 11

	{ "4.7v",		0x020000, 0x434290b5, 9 | BRF_SND },           // 12 Samples
	{ "5.7u",		0x020000, 0xebe6abb4, 9 | BRF_SND },           // 13
};

STD_ROM_PICK(superx)
STD_ROM_FN(superx)

static INT32 SuperxInit()
{
	return RsharkCommonInit(0);
}

struct BurnDriver BurnDrvSuperx = {
	"superx", NULL, NULL, NULL, "1994",
	"Super-X (NTC)\0", NULL, "Dooyong (NTC license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, superxRomInfo, superxRomName, NULL, NULL, RsharkInputInfo, SuperxDIPInfo,
	SuperxInit, Drv68KExit, RsharkFrame, RsharkDraw, Drv68KScan, &DrvRecalc, 0x800,
	240, 384, 3, 4
};


// Super-X (Mitchell)

static struct BurnRomInfo superxmRomDesc[] = {
	{ "2_m.3m",		0x020000, 0x41c50aac, 1 | BRF_PRG | BRF_ESS }, //  0 Main CPU Code
	{ "3_m.3l",		0x020000, 0x6738b703, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "1_m.5u",		0x010000, 0x319fa632, 2 | BRF_PRG | BRF_ESS }, //  2 Audio CPU Code

	{ "spxo-m05.10m",	0x200000, 0x9120dd84, 3 | BRF_GRA },           //  3 Characters

	{ "spxb-m04.8f",	0x100000, 0x91a7ac6e, 4 | BRF_GRA },           //  4 Sprites

	{ "spxb-m03.8j",	0x100000, 0x8b42861b, 5 | BRF_GRA },           //  5 Tiles

	{ "spxb-m02.8a",	0x100000, 0x21b8db78, 6 | BRF_GRA },           //  6 Tiles

	{ "spxb-m01.8c",	0x100000, 0x60c69129, 7 | BRF_GRA },           //  7 Tiles

	{ "spxb-ms3.10f",	0x020000, 0x8bf8c77d, 8 | BRF_GRA },           //  8 Tiles
	{ "spxb-ms4.10j",	0x020000, 0xd418a900, 8 | BRF_GRA },           //  9
	{ "spxb-ms2.10a",	0x020000, 0x5ec87adf, 8 | BRF_GRA },           // 10
	{ "spxb-ms1.10c",	0x020000, 0x40b4fe6c, 8 | BRF_GRA },           // 11

	{ "4.7v",		0x020000, 0x434290b5, 9 | BRF_SND },           // 12 Samples
	{ "5.7u",		0x020000, 0xebe6abb4, 9 | BRF_SND },           // 13
};

STD_ROM_PICK(superxm)
STD_ROM_FN(superxm)

struct BurnDriver BurnDrvSuperxm = {
	"superxm", "superx", NULL, NULL, "1994",
	"Super-X (Mitchell)\0", NULL, "Dooyong (Mitchell license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, superxmRomInfo, superxmRomName, NULL, NULL, RsharkInputInfo, SuperxDIPInfo,
	SuperxInit, Drv68KExit, RsharkFrame, RsharkDraw, Drv68KScan, &DrvRecalc, 0x800,
	240, 384, 3, 4
};


// R-Shark

static struct BurnRomInfo rsharkRomDesc[] = {
	{ "rspl00.bin",	0x20000, 0x40356b9d, 1 | BRF_PRG | BRF_ESS }, //  0 Main CPU Code
	{ "rspu00.bin",	0x20000, 0x6635c668, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "rse3.bin",	0x10000, 0x03c8fd17, 2 | BRF_PRG | BRF_ESS }, //  2 Audio CPU Code

	{ "rse4.bin",	0x80000, 0xb857e411, 3 | BRF_GRA },           //  3 Characters
	{ "rse5.bin",	0x80000, 0x7822d77a, 3 | BRF_GRA },           //  4
	{ "rse6.bin",	0x80000, 0x80215c52, 3 | BRF_GRA },           //  5
	{ "rse7.bin",	0x80000, 0xbd28bbdc, 3 | BRF_GRA },           //  6

	{ "rse11.bin",	0x80000, 0x8a0c572f, 4 | BRF_GRA },           //  7 Sprites
	{ "rse10.bin",	0x80000, 0x139d5947, 4 | BRF_GRA },           //  8

	{ "rse15.bin",	0x80000, 0xd188134d, 5 | BRF_GRA },           //  9 Tiles
	{ "rse14.bin",	0x80000, 0x0ef637a7, 5 | BRF_GRA },           // 10

	{ "rse17.bin",	0x80000, 0x7ff0f3c7, 6 | BRF_GRA },           // 11 Tiles
	{ "rse16.bin",	0x80000, 0xc176c8bc, 6 | BRF_GRA },           // 12

	{ "rse21.bin",	0x80000, 0x2ea665af, 7 | BRF_GRA },           // 13 Tiles
	{ "rse20.bin",	0x80000, 0xef93e3ac, 7 | BRF_GRA },           // 14

	{ "rse12.bin",	0x20000, 0xfadbf947, 8 | BRF_GRA },           // 15 Tiles
	{ "rse13.bin",	0x20000, 0x323d4df6, 8 | BRF_GRA },           // 16
	{ "rse18.bin",	0x20000, 0xe00c9171, 8 | BRF_GRA },           // 17
	{ "rse19.bin",	0x20000, 0xd214d1d0, 8 | BRF_GRA },           // 18

	{ "rse1.bin",	0x20000, 0x0291166f, 9 | BRF_SND },           // 19 Samples
	{ "rse2.bin",	0x20000, 0x5a26ee72, 9 | BRF_SND },           // 20
};

STD_ROM_PICK(rshark)
STD_ROM_FN(rshark)

static INT32 RsharkInit()
{
	return RsharkCommonInit(1);
}

struct BurnDriver BurnDrvRshark = {
	"rshark", NULL, NULL, NULL, "1995",
	"R-Shark\0", NULL, "Dooyong", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, rsharkRomInfo, rsharkRomName, NULL, NULL, RsharkInputInfo, RsharkDIPInfo,
	RsharkInit, Drv68KExit, RsharkFrame, RsharkDraw, Drv68KScan, &DrvRecalc, 0x800,
	240, 384, 3, 4
};


// Pop Bingo

static struct BurnRomInfo popbingoRomDesc[] = {
	{ "rom2.3f",	0x20000, 0xb24513c6, 1 | BRF_PRG | BRF_ESS }, //  0 Main CPU Code
	{ "rom3.3e",	0x20000, 0x48070081, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "rom1.3p",	0x10000, 0x46e8d2c4, 2 | BRF_PRG | BRF_ESS }, //  2 Audio CPU Code

	{ "rom5.9m",	0x80000, 0xe8d73e07, 3 | BRF_GRA },           //  3 Characters
	{ "rom6.9l",	0x80000, 0xc3db3975, 3 | BRF_GRA },           //  4

	{ "rom10.9a",	0x80000, 0x135ab90a, 4 | BRF_GRA },           //  5 Sprites
	{ "rom9.9c",	0x80000, 0xc9d90007, 4 | BRF_GRA },           //  6
	{ "rom7.9h",	0x80000, 0xb2b4c13b, 4 | BRF_GRA },           //  7
	{ "rom8.9e",	0x80000, 0x66c4b00f, 4 | BRF_GRA },           //  8

	{ "rom4.4r",	0x20000, 0x0fdee034, 5 | BRF_SND },           //  9 Samples
};

STD_ROM_PICK(popbingo)
STD_ROM_FN(popbingo)

static INT32 PopbingoInit()
{
	return RsharkCommonInit(2);
}

struct BurnDriver BurnDrvPopbingo = {
	"popbingo", NULL, NULL, NULL, "1996",
	"Pop Bingo\0", NULL, "Dooyong", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, popbingoRomInfo, popbingoRomName, NULL, NULL, RsharkInputInfo, PopbingoDIPInfo,
	PopbingoInit, Drv68KExit, RsharkFrame, PopbingoDraw, Drv68KScan, &DrvRecalc, 0x800,
	384, 240, 4, 3
};
