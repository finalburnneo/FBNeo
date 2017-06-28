// FB Alpha Track & Field driver module
// Based on MAME driver by Chris Hardy.

// To fix / Oddities:
//   Watchdog timer doesn't get reset by Mastkin
//   Wizz Quiz needs some work.

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "m6800_intf.h"
#include "z80_intf.h"
#include "bitswap.h"
#include "vlm5030.h"
#include "sn76496.h"
#include "dac.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6809ROM;
static UINT8 *DrvM6809ROMDec;
static UINT8 *DrvQuizROM;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvSndROM;
static UINT8 *DrvNVRAM;
static UINT8 *DrvSprRAM0;
static UINT8 *DrvSprRAM1;
static UINT8 *DrvColRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvM6800RAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 bg_bank;
static UINT8 soundlatch;
static UINT8 flipscreen;
static UINT8 irq_mask;
static UINT8 nmi_mask;
static UINT16 last_addr;
static UINT8 last_sound_irq;
static UINT8 SN76496_latch;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static INT32 watchdog;
static INT32 nowatchdog = 0;
static INT32 game_select;
static INT32 nSpriteMask, nCharMask;

static struct BurnInputInfo TrackfldInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 2"},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 3"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 3"},

	{"P3 Start",		BIT_DIGITAL,	DrvJoy2 + 3,	"p3 start"},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy3 + 2,	"p3 fire 1"},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy3 + 1,	"p3 fire 2"},
	{"P3 Button 3",		BIT_DIGITAL,	DrvJoy3 + 0,	"p3 fire 3"},

	{"P4 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p4 start"},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy3 + 6,	"p4 fire 1"},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p4 fire 2"},
	{"P4 Button 3",		BIT_DIGITAL,	DrvJoy3 + 4,	"p4 fire 3"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
};

STDINPUTINFO(Trackfld)

static struct BurnInputInfo YieartfInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"},
};

STDINPUTINFO(Yieartf)

static struct BurnInputInfo ReaktorInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
};

STDINPUTINFO(Reaktor)

static struct BurnInputInfo WizzquizInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 3"},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 fire 4"},
	{"P1 Button 5",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 fire 5"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 2"},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 3"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
};

STDINPUTINFO(Wizzquiz)

static struct BurnInputInfo MastkinInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
};

STDINPUTINFO(Mastkin)

static struct BurnDIPInfo TrackfldDIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL		},
	{0x15, 0xff, 0xff, 0x59, NULL		},

	{0   , 0xfe, 0   ,    16, "Coin A"		},
	{0x14, 0x01, 0x0f, 0x02, "4 Coins 1 Credits"		},
	{0x14, 0x01, 0x0f, 0x05, "3 Coins 1 Credits"		},
	{0x14, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"		},
	{0x14, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"		},
	{0x14, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"		},
	{0x14, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"		},
	{0x14, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"		},
	{0x14, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"		},
	{0x14, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"		},
	{0x14, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"		},
	{0x14, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"		},
	{0x14, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"		},
	{0x14, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"		},
	{0x14, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"		},
	{0x14, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"		},
	{0x14, 0x01, 0x0f, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    16, "Coin B"		},
	{0x14, 0x01, 0xf0, 0x20, "4 Coins 1 Credits"		},
	{0x14, 0x01, 0xf0, 0x50, "3 Coins 1 Credits"		},
	{0x14, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"		},
	{0x14, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"		},
	{0x14, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"		},
	{0x14, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x14, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"		},
	{0x14, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"		},
	{0x14, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"		},
	{0x14, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"		},
	{0x14, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"		},
	{0x14, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"		},
	{0x14, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"		},
	{0x14, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"		},
	{0x14, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"		},
	{0x14, 0x01, 0xf0, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x15, 0x01, 0x01, 0x01, "1"		},
	{0x15, 0x01, 0x01, 0x00, "2"		},

	{0   , 0xfe, 0   ,    2, "After Last Event"		},
	{0x15, 0x01, 0x02, 0x02, "Game Over"		},
	{0x15, 0x01, 0x02, 0x00, "Game Continues"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x15, 0x01, 0x04, 0x00, "Upright"		},
	{0x15, 0x01, 0x04, 0x04, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x15, 0x01, 0x08, 0x08, "None"		},
	{0x15, 0x01, 0x08, 0x00, "100000"		},

	{0   , 0xfe, 0   ,    2, "World Records"		},
	{0x15, 0x01, 0x10, 0x10, "Don't Erase"		},
	{0x15, 0x01, 0x10, 0x00, "Erase on Reset"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x15, 0x01, 0x60, 0x60, "Easy"		},
	{0x15, 0x01, 0x60, 0x40, "Normal"		},
	{0x15, 0x01, 0x60, 0x20, "Hard"		},
	{0x15, 0x01, 0x60, 0x00, "Difficult"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x15, 0x01, 0x80, 0x80, "Off"		},
	{0x15, 0x01, 0x80, 0x00, "On"		},
};

STDDIPINFO(Trackfld)

static struct BurnDIPInfo YieartfDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL		},
	{0x13, 0xff, 0xff, 0x5b, NULL		},
	{0x14, 0xff, 0xff, 0xff, NULL		},

	{0   , 0xfe, 0   ,    16, "Coin A"		},
	{0x12, 0x01, 0x0f, 0x02, "4 Coins 1 Credits"		},
	{0x12, 0x01, 0x0f, 0x05, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"		},
	{0x12, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"		},
	{0x12, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"		},
	{0x12, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"		},
	{0x12, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"		},
	{0x12, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"		},
	{0x12, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"		},
	{0x12, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"		},
	{0x12, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"		},
	{0x12, 0x01, 0x0f, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    15, "Coin B"		},
	{0x12, 0x01, 0xf0, 0x20, "4 Coins 1 Credits"		},
	{0x12, 0x01, 0xf0, 0x50, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"		},
	{0x12, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"		},
	{0x12, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"		},
	{0x12, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"		},
	{0x12, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"		},
	{0x12, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"		},
	{0x12, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"		},
	{0x12, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"		},
	{0x12, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0x03, 0x03, "1"		},
	{0x13, 0x01, 0x03, 0x02, "2"		},
	{0x13, 0x01, 0x03, 0x01, "3"		},
	{0x13, 0x01, 0x03, 0x00, "5"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x13, 0x01, 0x04, 0x00, "Upright"		},
	{0x13, 0x01, 0x04, 0x04, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x13, 0x01, 0x08, 0x08, "30000 80000"		},
	{0x13, 0x01, 0x08, 0x00, "40000 90000"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x30, 0x30, "Easy"		},
	{0x13, 0x01, 0x30, 0x10, "Normal"		},
	{0x13, 0x01, 0x30, 0x20, "Difficult"		},
	{0x13, 0x01, 0x30, 0x00, "Very Difficult"		},

	{0   , 0xfe, 0   ,    0, "Demo Sounds"		},
	{0x13, 0x01, 0x80, 0x80, "Off"		},
	{0x13, 0x01, 0x80, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x14, 0x01, 0x01, 0x01, "Off"		},
	{0x14, 0x01, 0x01, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Upright Controls"		},
	{0x14, 0x01, 0x02, 0x02, "Single"		},
	{0x14, 0x01, 0x02, 0x00, "Dual"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x04, 0x04, "Off"		},
	{0x14, 0x01, 0x04, 0x00, "On"		},
};

STDDIPINFO(Yieartf)

static struct BurnDIPInfo ReaktorDIPList[]=
{
	{0x09, 0xff, 0xff, 0xbf, NULL		},
	{0x0a, 0xff, 0xff, 0xef, NULL		},

	{0   , 0xfe, 0   ,    2, "Pricing"		},
	{0x09, 0x01, 0x01, 0x01, "10p / 25c per play"		},
	{0x09, 0x01, 0x01, 0x00, "20p / 50c per play"		},

	{0   , 0xfe, 0   ,    2, "Coinage Type"		},
	{0x09, 0x01, 0x10, 0x10, "English (10p / 20p)"		},
	{0x09, 0x01, 0x10, 0x00, "American (25c / 50c)"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x09, 0x01, 0x60, 0x60, "2"		},
	{0x09, 0x01, 0x60, 0x40, "3"		},
	{0x09, 0x01, 0x60, 0x20, "4"		},
	{0x09, 0x01, 0x60, 0x00, "5"		},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x09, 0x01, 0x80, 0x80, "20000"		},
	{0x09, 0x01, 0x80, 0x00, "30000"		},

	{0   , 0xfe, 0   ,    2, "Game Orientation"		},
	{0x0a, 0x01, 0x01, 0x01, "For Vertical Monitor"		},
	{0x0a, 0x01, 0x01, 0x00, "For Horizontal Monitor"		},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x0a, 0x01, 0x08, 0x08, "Off"		},
	{0x0a, 0x01, 0x08, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Wipe Highscores"		},
	{0x0a, 0x01, 0x10, 0x00, "Off"		},
	{0x0a, 0x01, 0x10, 0x10, "On"		},
};

STDDIPINFO(Reaktor)

static struct BurnDIPInfo WizzquizDIPList[]=
{
	{0x0e, 0xff, 0xff, 0xff, NULL		},
	{0x0f, 0xff, 0xff, 0xff, NULL		},

	{0   , 0xfe, 0   ,    17, "Coin A"		},
	{0x0e, 0x01, 0x0f, 0x02, "4 Coins 1 Credits"		},
	{0x0e, 0x01, 0x0f, 0x05, "3 Coins 1 Credits"		},
	{0x0e, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"		},
	{0x0e, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"		},
	{0x0e, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"		},
	{0x0e, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"		},
	{0x0e, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"		},
	{0x0e, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"		},
	{0x0e, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"		},
	{0x0e, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"		},
	{0x0e, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"		},
	{0x0e, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"		},
	{0x0e, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"		},
	{0x0e, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"		},
	{0x0e, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"		},
	{0x0e, 0x01, 0x0f, 0x00, "Free Play"		},
	{0x0e, 0x01, 0x0f, 0x00, "No Coin A"		},

	{0   , 0xfe, 0   ,    17, "Coin B"		},
	{0x0e, 0x01, 0xf0, 0x20, "4 Coins 1 Credits"		},
	{0x0e, 0x01, 0xf0, 0x50, "3 Coins 1 Credits"		},
	{0x0e, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"		},
	{0x0e, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"		},
	{0x0e, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"		},
	{0x0e, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x0e, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"		},
	{0x0e, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"		},
	{0x0e, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"		},
	{0x0e, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"		},
	{0x0e, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"		},
	{0x0e, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"		},
	{0x0e, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"		},
	{0x0e, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"		},
	{0x0e, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"		},
	{0x0e, 0x01, 0xf0, 0x00, "Free Play"		},
	{0x0e, 0x01, 0xf0, 0x00, "No Coin B"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0f, 0x01, 0x03, 0x03, "3"		},
	{0x0f, 0x01, 0x03, 0x02, "4"		},
	{0x0f, 0x01, 0x03, 0x01, "5"		},
	{0x0f, 0x01, 0x03, 0x00, "6"		},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x0f, 0x01, 0x04, 0x00, "8,000 Points"		},
	{0x0f, 0x01, 0x04, 0x04, "10,000 Points"		},

	{0   , 0xfe, 0   ,    0, "Show Correct Answer"		},
	{0x0f, 0x01, 0x40, 0x40, "No"		},
	{0x0f, 0x01, 0x40, 0x00, "Yes"		},

	{0   , 0xfe, 0   ,    0, "Credit Limit"		},
	{0x0f, 0x01, 0x80, 0x80, "99 Credits"		},
	{0x0f, 0x01, 0x80, 0x00, "9 Credits"		},
};

STDDIPINFO(Wizzquiz)

static struct BurnDIPInfo MastkinDIPList[]=
{
	{0x09, 0xff, 0xff, 0xD6, NULL		},
	{0x0a, 0xff, 0xff, 0x00, NULL		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x09, 0x01, 0x01, 0x01, "Off"		},
	{0x09, 0x01, 0x01, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Timer Speed"		},
	{0x09, 0x01, 0x02, 0x02, "Normal"		},
	{0x09, 0x01, 0x02, 0x00, "Fast"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x09, 0x01, 0x0c, 0x0c, "Easy"		},
	{0x09, 0x01, 0x0c, 0x04, "Normal"		},
	{0x09, 0x01, 0x0c, 0x08, "Hard"		},
	{0x09, 0x01, 0x0c, 0x00, "Very Hard"		},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x09, 0x01, 0x10, 0x10, "4"		},
	{0x09, 0x01, 0x10, 0x00, "5"		},

	{0   , 0xfe, 0   ,    2, "Internal speed"		},
	{0x09, 0x01, 0x20, 0x20, "Slow"		},
	{0x09, 0x01, 0x20, 0x00, "Fast"		},

	{0   , 0xfe, 0   ,    0, "Coin B"		},
	{0x0a, 0x01, 0x0f, 0x0a, "4 Coins 1 Credits"		},
	{0x0a, 0x01, 0x0f, 0x09, "3 Coins 1 Credits"		},
	{0x0a, 0x01, 0x0f, 0x05, "2 Coins 1 Credits"		},
	{0x0a, 0x01, 0x0f, 0x06, "2 Coins 2 Credits"		},
	{0x0a, 0x01, 0x0f, 0x00, "1 Coin  1 Credits"		},
	{0x0a, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"		},
	{0x0a, 0x01, 0x0f, 0x08, "2 Coins 4 Credits"		},
	{0x0a, 0x01, 0x0f, 0x01, "1 Coin  2 Credits"		},
	{0x0a, 0x01, 0x0f, 0x02, "1 Coin  3 Credits"		},
	{0x0a, 0x01, 0x0f, 0x03, "1 Coin  4 Credits"		},
	{0x0a, 0x01, 0x0f, 0x04, "1 Coin  5 Credits"		},

	{0   , 0xfe, 0   ,    11, "Coin A"		},
	{0x0a, 0x01, 0xf0, 0xa0, "4 Coins 1 Credits"		},
	{0x0a, 0x01, 0xf0, 0x90, "3 Coins 1 Credits"		},
	{0x0a, 0x01, 0xf0, 0x50, "2 Coins 1 Credits"		},
	{0x0a, 0x01, 0xf0, 0x60, "2 Coins 2 Credits"		},
	{0x0a, 0x01, 0xf0, 0x00, "1 Coin  1 Credits"		},
	{0x0a, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"		},
	{0x0a, 0x01, 0xf0, 0x80, "2 Coins 4 Credits"		},
	{0x0a, 0x01, 0xf0, 0x10, "1 Coin  2 Credits"		},
	{0x0a, 0x01, 0xf0, 0x20, "1 Coin  3 Credits"		},
	{0x0a, 0x01, 0xf0, 0x30, "1 Coin  4 Credits"		},
	{0x0a, 0x01, 0xf0, 0x40, "1 Coin  5 Credits"		},
};

STDDIPINFO(Mastkin)

static void trackfld_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfc80) == 0x1000) address &= ~0x7;
	if ((address & 0xff00) == 0x1200) address &= ~0x4;

	switch (address & ~0x78)
	{
		case 0x1000:
			watchdog = 0;
		return;

		case 0x1080:
		case 0x10b0:
			flipscreen = data; //?
		return;

		case 0x1081:
		case 0x10b1:
		{
			if (last_sound_irq == 0 && data)
			{
				ZetSetVector(0xff);
				ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
			}

			last_sound_irq = data;
		}
		return;

		case 0x1082:
			nmi_mask = data & 0x01;
		return;

		case 0x1083:
		case 0x1084:
		case 0x10b3:
		case 0x10b4:
			// coin counter
		return;

		case 0x1085:
		case 0x1086:
		return;		// nop

		case 0x1087:
		case 0x10b7:
			irq_mask = data & 0x01;
		return;

		case 0x1100:
			soundlatch = data;
		return;
	}
}

static UINT8 trackfld_main_read(UINT16 address)
{
	//if ((address & 0xfc80) == 0x1080) address &= ~0x7f;
	//if ((address & 0xffff) == 0x1200) address &= ~0x7c;

	switch (address)
	{
		case 0x0000: // yieartf
			return (vlm5030_bsy(0) ? 1 : 0);

		case 0x1080: // flipscreen_w;
			return 0;

		case 0x1200:
			return DrvDips[1];

		case 0x1280:
			return DrvInputs[0];

		case 0x1281:
			return DrvInputs[1];

		case 0x1282:
			return DrvInputs[2];

		case 0x1283:
			return DrvDips[0];

		case 0x1300: // yieartf
			return DrvDips[2];
	}
	//bprintf(0, _T("mr %X. "), address);
	return 0;
}

static void yieartf_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x0000:
			SN76496_latch = data;
		return;

		case 0x0001:
			SN76496Write(0, SN76496_latch);
		return;

		case 0x0002:
		{
			vlm5030_st(0, (data >> 1) & 1);
			vlm5030_rst(0, (data >> 2) & 1);
		}
		return;

		case 0x0003:
			vlm5030_data_write(0, data);
		return;

		case 0x1100:
		return; // no soundlatch
	}

	trackfld_main_write(address, data);
}

static void __fastcall reaktor_main_write(UINT16 address, UINT8 data)
{
	if (address == 0x9081)
	{
		if (last_sound_irq == 0 && data)
		{
			ZetClose();
			ZetOpen(1);
			ZetSetVector(0xff);
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
			ZetRun(100);
			ZetSetIRQLine(0,CPU_IRQSTATUS_NONE);
			ZetClose();
			ZetOpen(0);
		}
		last_sound_irq = data;
		return;
	}

	if (address == 0x9100) {
		soundlatch = data;
		return;
	}

	if ((address & 0xfc00) == 0x9000) {
		trackfld_main_write(address - 0x8000, data);
		return;
	}

	//bprintf (0, _T("wb %4.4x, %2.2x\n"), address, data);
}

static UINT8 __fastcall reaktor_main_read(UINT16 address)
{
	if ((address & 0xfc00) == 0x9000) {
		return trackfld_main_read(address - 0x8000);
	}

//	bprintf (0, _T("rb %4.4x\n"), address);
	return 0;
}

static void wizzquiz_bank(UINT8 data)
{
	for (INT32 i = 0; i < 8; i++)
	{
		if ((data & (1 << i)) == 0)
		{
			M6800MapMemory(DrvQuizROM + (i * 0x8000), 0x6000, 0xdfff, MAP_ROM);
			return;
		}
	}
}

static void wizzquiz_main_write(UINT16 address, UINT8 data)
{
//	if (address != 0x1000)
//		bprintf (0, _T("%4.4x, %2.2x\n"), address, data);

//	if (address < 0x80) {
//		DrvM6800RAM[address] = data;
//		return;
//	}

	switch (address)
	{
		case 0xc000:
			wizzquiz_bank(data);
		return;
	}

	trackfld_main_write(address, data);
}

static UINT8 wizzquiz_main_read(UINT16 address)
{
//	if (address != 0x1000) bprintf (0, _T("rb %4.4x\n"), address);

//	if (address < 0x80) {
//		return DrvM6800RAM[address];
//	}

	switch (address)
	{
		case 0x1000:
			watchdog = 0;
			return 0;
	}

	return trackfld_main_read(address);
}


static void __fastcall trackfld_sound_write(UINT16 address, UINT8 data)
{
	UINT16 unmasked = address;
	if (address < 0xe000) address &= ~0x1fff;
	if (address > 0xe000) address &= ~0x1ff8;

	switch (address)
	{
		case 0xa000:
			SN76496_latch = data;
		return;

		case 0xc000:
			SN76496Write(0, SN76496_latch);
		return;

		case 0xe000:
			DACWrite(0, data);
		return;

		case 0xe003:
		{
			INT32 addr = (unmasked & 0x380) ^ last_addr;

			if (addr & 0x100) vlm5030_st(0, (unmasked & 0x100) >> 8);
			if (addr & 0x200) vlm5030_rst(0, (unmasked & 0x200) >> 9);
			last_addr = unmasked & 0x380;
		}
		return;

		case 0xe004:
			vlm5030_data_write(0, data);
		return;
	}
}

static UINT8 __fastcall trackfld_sound_read(UINT16 address)
{
	if (address < 0xe000) address &= ~0x1fff;
	if (address > 0xe000) address &= ~0x1ff8;

	switch (address)
	{
		case 0x6000:
			return soundlatch;

		case 0x8000:
			return (ZetTotalCycles() / 1024) & 0xf;

		case 0xc000:
			SN76496Write(0, SN76496_latch);
			return 0xff;

		case 0xe001:
			return 0; // nop

		case 0xe002:
			return (vlm5030_bsy(0) ? 0x10 : 0);
	}

	return 0;
}

static INT32 DrvSyncDAC()
{
	return (INT32)(float)(nBurnSoundLen * (ZetTotalCycles() / (3579545.000 / (nBurnFPS / 100.000))));
}

static UINT32 DrvVLM5030Sync(INT32 samples_rate)
{
	return (samples_rate * ZetTotalCycles()) / (3579545 / 60);
}

static UINT32 DrvVLM5030Sync2(INT32 samples_rate)
{
	return (samples_rate * M6809TotalCycles()) / (1536000 / 60);
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	if (game_select == 4) {
	//	M6800Open(0);
		M6800Reset();
		wizzquiz_bank(0);
	//	M6800Close();
	}

	if (game_select == 3) {
		ZetOpen(0);
		ZetReset();
		ZetClose();
	}

	if (game_select == 1 || game_select == 2) {
		M6809Open(0);
		M6809Reset();
		M6809Close();
	}

	if (game_select == 1 || game_select == 3 || game_select == 4) {
		ZetOpen(1);
		ZetReset();
		vlm5030Reset(0);
		SN76496Reset();
		DACReset();
		ZetClose();
	}

	if (game_select == 2) {
		vlm5030Reset(0);
		SN76496Reset();
	}

	bg_bank = 0;
	soundlatch = 0;
	flipscreen = 0;
	irq_mask = 0;
	nmi_mask = 0;
	last_addr = 0;
	last_sound_irq = 0;
	SN76496_latch = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next;
	DrvM6809ROM		= Next; Next += 0x010000;
	DrvM6809ROMDec	= Next; Next += 0x010000;

	DrvQuizROM		= Next; Next += 0x040000;

	DrvZ80ROM1		= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += 0x020000;
	DrvGfxROM1		= Next; Next += 0x010000;

	DrvColPROM		= Next; Next += 0x000220;

	DrvSndROM		= Next; Next += 0x002000;

	DrvPalette		= (UINT32*)Next; Next += 0x0200 * sizeof(UINT32);

	AllRam			= Next;

	DrvNVRAM		= Next; Next += 0x000800;
	DrvM6800RAM		= Next; Next += 0x000100;
	DrvSprRAM0		= Next; Next += 0x000400;
	DrvSprRAM1		= Next; Next += 0x000400;
	DrvColRAM		= Next; Next += 0x000800;
	DrvVidRAM		= Next; Next += 0x000800;
	DrvZ80RAM0		= Next; Next += 0x000c00;
	DrvZ80RAM1		= Next; Next += 0x000400;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode() // 0, 100
{
	INT32 Plane1[4] = { STEP4(0,1) };
	INT32 XOffs1[8] = { STEP8(0,4) };
	INT32 YOffs1[8] = { STEP8(0,32) };

	INT32 Plane0[4] = { (0x8000*8)+4, (0x8000*8), 4, 0 };
	INT32 XOffs0[16] = { STEP4(0,1), STEP4(64,1), STEP4(128,1), STEP4(192,1) };
	INT32 YOffs0[16] = { STEP8(0,8), STEP8(256,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x10000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x10000);

	GfxDecode(0x0200, 4, 16, 16, Plane0, XOffs0, YOffs0, 0x200, tmp, DrvGfxROM0);

	memset (tmp, 0, 0x10000);
	memcpy (tmp, DrvGfxROM1, 0x8000);

	GfxDecode(0x0400, 4,  8,  8, Plane1, XOffs1, YOffs1, 0x100, tmp, DrvGfxROM1);

	BurnFree(tmp);

	return 0;
}

static void M6809Decode()
{
	for (INT32 i = 0x6000; i < 0x10000; i++) {
		DrvM6809ROMDec[i] = DrvM6809ROM[i] ^ (((i&2)?0x80:0x20)|((i&8)?0x08:0x02));
	}
}

static void CommonSoundInit()
{
	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,		0x0000, 0x3fff, MAP_ROM);
	for (INT32 i = 0; i < 0x2000; i+=0x400) {
		ZetMapMemory(DrvZ80RAM1,		0x4000+i, 0x43ff+i, MAP_RAM);
	}
	ZetSetWriteHandler(trackfld_sound_write);
	ZetSetReadHandler(trackfld_sound_read);
	ZetClose();

	vlm5030Init(0, 3579545, DrvVLM5030Sync, DrvSndROM, 0x2000, 0);
	vlm5030SetAllRoutes(0, 1.00, BURN_SND_ROUTE_BOTH);

	SN76496Init(0, 1789772, 1);
	SN76496SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	DACInit(0, 0, 1, DrvSyncDAC);
	DACSetRoute(0, 0.80, BURN_SND_ROUTE_BOTH);
}

static INT32 DrvInit()
{
	game_select = 1;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvM6809ROM + 0x6000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x8000,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0xa000,  2, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0xc000,  3, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0xe000,  4, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1  + 0x0000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0  + 0x0000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x2000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x8000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0xa000,  9, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1  + 0x0000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x2000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x4000, 12, 1)) return 1;

		if (BurnLoadRom(DrvColPROM  + 0x0000, 13, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x0020, 14, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x0120, 15, 1)) return 1;

		if (BurnLoadRom(DrvSndROM   + 0x0000, 16, 1)) return 1;

		DrvGfxDecode();
		M6809Decode();
	}

	M6809Init(1);
	M6809Open(0);
	M6809MapMemory(DrvSprRAM1,		0x1800, 0x1bff, MAP_RAM);
	// spr2 00-3f, scroll 40-5f
	M6809MapMemory(DrvSprRAM0,		0x1c00, 0x1fff, MAP_RAM);
	// spr  00-3f, scroll 40-5f
	M6809MapMemory(DrvNVRAM,		0x2800, 0x2fff, MAP_RAM);
	M6809MapMemory(DrvVidRAM,		0x3000, 0x37ff, MAP_RAM);
	M6809MapMemory(DrvColRAM,		0x3800, 0x3fff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM + 0x6000,	0x6000, 0xffff, MAP_READ);
	M6809MapMemory(DrvM6809ROMDec + 0x6000,	0x6000, 0xffff, MAP_FETCH);
	M6809SetWriteHandler(trackfld_main_write);
	M6809SetReadHandler(trackfld_main_read);
	M6809Close();

	ZetInit(0); // reaktor
	CommonSoundInit();

	nSpriteMask = 0xff;
	nCharMask = 0x3ff;

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static void TrackfldnzDecode()
{
	for (INT32 i = 0x6000; i < 0x10000; i++)
		DrvM6809ROM[i] = BITSWAP08(DrvM6809ROM[i], 6, 7, 5, 4, 3, 2, 1, 0);
}

static INT32 TrackfldnzInit()
{
	game_select = 1;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvM6809ROM + 0x6000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x8000,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0xa000,  2, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0xc000,  3, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0xe000,  4, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1  + 0x0000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0  + 0x0000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x2000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x8000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0xa000,  9, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1  + 0x0000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x2000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x4000, 12, 1)) return 1;

		if (BurnLoadRom(DrvColPROM  + 0x0000, 13, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x0020, 14, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x0120, 15, 1)) return 1;

		if (BurnLoadRom(DrvSndROM   + 0x0000, 16, 1)) return 1;

		DrvGfxDecode();
		TrackfldnzDecode(); // before M6809Decode
		M6809Decode();
	}

	M6809Init(1);
	M6809Open(0);
	M6809MapMemory(DrvSprRAM1,		0x1800, 0x1bff, MAP_RAM);
	// spr2 00-3f, scroll 40-5f
	M6809MapMemory(DrvSprRAM0,		0x1c00, 0x1fff, MAP_RAM);
	// spr  00-3f, scroll 40-5f
	M6809MapMemory(DrvNVRAM,		0x2800, 0x2fff, MAP_RAM);
	M6809MapMemory(DrvVidRAM,		0x3000, 0x37ff, MAP_RAM);
	M6809MapMemory(DrvColRAM,		0x3800, 0x3fff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM + 0x6000,	0x6000, 0xffff, MAP_READ);
	M6809MapMemory(DrvM6809ROMDec + 0x6000,	0x6000, 0xffff, MAP_FETCH);
	M6809SetWriteHandler(trackfld_main_write);
	M6809SetReadHandler(trackfld_main_read);
	M6809Close();

	ZetInit(0); // reaktor
	CommonSoundInit();

	nSpriteMask = 0xff;
	nCharMask = 0x3ff;

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static INT32 YieartfInit()
{
	game_select = 2;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvM6809ROM + 0x8000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0xa000,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0xc000,  2, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0xe000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0  + 0x0000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x4000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x8000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0xc000,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1  + 0x0000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x2000,  9, 1)) return 1;

		if (BurnLoadRom(DrvColPROM  + 0x0000, 10, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x0020, 11, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x0120, 12, 1)) return 1;

		if (BurnLoadRom(DrvSndROM   + 0x0000, 13, 1)) return 1;

		DrvGfxDecode();
	}

	M6809Init(1);
	M6809Open(0);
	M6809MapMemory(DrvSprRAM1,		0x1800, 0x1bff, MAP_RAM);
	// spr2 00-3f, scroll 40-5f
	M6809MapMemory(DrvSprRAM0,		0x1c00, 0x1fff, MAP_RAM);
	// spr  00-3f, scroll 40-5f
	M6809MapMemory(DrvNVRAM,		0x2800, 0x2fff, MAP_RAM);
	M6809MapMemory(DrvVidRAM,		0x3000, 0x37ff, MAP_RAM);
	M6809MapMemory(DrvColRAM,		0x3800, 0x3fff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM + 0x6000,	0x6000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(yieartf_main_write);
	M6809SetReadHandler(trackfld_main_read);
	M6809Close();

	vlm5030Init(0, 3579545, DrvVLM5030Sync2, DrvSndROM, 0x2000, 1);
	vlm5030SetAllRoutes(0, 1.00, BURN_SND_ROUTE_BOTH);

	SN76496Init(0, 1536000, 0);
	SN76496SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	nSpriteMask = 0x1ff;
	nCharMask = 0x1ff;

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static INT32 ReaktorInit()
{
	game_select = 3;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0  + 0x0000,  0, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1  + 0x0000,  1, 1)) return 1;
		memmove(DrvZ80ROM1, DrvZ80ROM1 + 0x2000, 0x2000);
		memset(DrvZ80ROM1 + 0x2000, 0, 0x2000);

		UINT8 *tmp = (UINT8*)BurnMalloc(0x4000);

		if (BurnLoadRom(tmp  + 0x0000,  2, 1)) return 1;
		memcpy (DrvGfxROM0 + 0x0000, tmp + 0x2000, 0x2000);
		if (BurnLoadRom(tmp  + 0x0000,  3, 1)) return 1;
		memcpy (DrvGfxROM0 + 0x8000, tmp + 0x2000, 0x2000);
		if (BurnLoadRom(DrvGfxROM0 + 0x2000,  4, 1)) return 1;
		if (BurnLoadRom(tmp  + 0x0000,  5, 1)) return 1;
		memcpy (DrvGfxROM0 + 0xa000, tmp + 0x2000, 0x2000);

		BurnFree(tmp);

		if (BurnLoadRom(DrvGfxROM1  + 0x0000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x2000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x4000,  8, 1)) return 1;

		if (BurnLoadRom(DrvColPROM  + 0x0000,  9, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x0020, 10, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x0120, 11, 1)) return 1;

		if (BurnLoadRom(DrvSndROM   + 0x0000, 12, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvSprRAM1,	0x9800, 0x9bff, MAP_RAM);
	ZetMapMemory(DrvSprRAM0,	0x9c00, 0x9fff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM0,	0xa800, 0xabff, MAP_RAM);
	ZetMapMemory(DrvNVRAM,		0xac00, 0xafff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,		0xb000, 0xb7ff, MAP_RAM);
	ZetMapMemory(DrvColRAM,		0xb800, 0xbfff, MAP_RAM);
	ZetSetWriteHandler(reaktor_main_write);
	ZetSetReadHandler(reaktor_main_read);
	ZetClose();

	CommonSoundInit();

	nSpriteMask = 0xff;
	nCharMask = 0x3ff;

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static void wizzquizSwap(UINT8 *rom)
{
	UINT8 *tmp = (UINT8*)BurnMalloc(0x8000);

	memcpy (tmp + 0x2000, rom + 0x0000, 0x6000);
	memcpy (tmp + 0x0000, rom + 0x6000, 0x2000);

	BurnFree(tmp);
}

static void wizzquizDecode(UINT8 *rom, INT32 len)
{
	for (INT32 i = 0; i < len; i++)
		rom[i] = BITSWAP08(rom[i],0,1,2,3,4,5,6,7);
}

static INT32 WizzquizInit()
{
	game_select = 4;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvM6809ROM + 0x0000,  0, 1)) return 1;

		if (BurnLoadRom(DrvQuizROM + 0x00000,  1, 1)) return 1;
		if (BurnLoadRom(DrvQuizROM + 0x08000,  2, 1)) return 1;
		if (BurnLoadRom(DrvQuizROM + 0x10000,  3, 1)) return 1;
		if (BurnLoadRom(DrvQuizROM + 0x18000,  4, 1)) return 1;
		if (BurnLoadRom(DrvQuizROM + 0x20000,  5, 1)) return 1;
		if (BurnLoadRom(DrvQuizROM + 0x28000,  6, 1)) return 1;
		if (BurnLoadRom(DrvQuizROM + 0x30000,  7, 1)) return 1;
		if (BurnLoadRom(DrvQuizROM + 0x38000,  8, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1  + 0x0000,  9, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0  + 0x0000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x8000, 11, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1  + 0x0000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x2000, 13, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x4000, 14, 1)) return 1;

		if (BurnLoadRom(DrvColPROM  + 0x0000, 15, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x0020, 16, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x0120, 17, 1)) return 1;

		wizzquizDecode(DrvM6809ROM, 0x02000);
		for (INT32 i = 0; i < 8; i++) {
			wizzquizDecode(DrvQuizROM + i * 0x8000, 0x8000);
			wizzquizSwap(DrvQuizROM + i * 0x8000);
		}

		DrvGfxDecode();
	}

	M6800Init(1);
//	M6800Open(0);
	M6800MapMemory(DrvM6800RAM,		0x0000, 0x00ff, MAP_RAM);
	M6800MapMemory(DrvSprRAM1,		0x1800, 0x1bff, MAP_RAM);
	M6800MapMemory(DrvSprRAM0,		0x1c00, 0x1fff, MAP_RAM);
	M6800MapMemory(DrvZ80ROM0,		0x2800, 0x2bff, MAP_RAM);
	M6800MapMemory(DrvNVRAM,		0x2c00, 0x2fff, MAP_RAM);
	M6800MapMemory(DrvVidRAM,		0x3000, 0x37ff, MAP_RAM);
	M6800MapMemory(DrvColRAM,		0x3800, 0x3fff, MAP_RAM);
	M6800MapMemory(DrvM6809ROM,		0xe000, 0xffff, MAP_ROM);
	M6800SetWriteHandler(wizzquiz_main_write);
	M6800SetReadHandler(wizzquiz_main_read);
//	M6800Close();

	ZetInit(0); // reaktor

	CommonSoundInit();

	nSpriteMask = 0x7f;
	nCharMask = 0x3ff;

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static void MastkinPaletteCreate()
{
	for (INT32 i = 0; i < 0x20; i++)
	{
		DrvColPROM[i] = i * 4;
	}

	for (INT32 i = 0; i < 0x200; i++)
	{
		DrvColPROM[i + 0x20] = (i & 0x0f) ? ((i + i / 16) & 0x0f) : 0;
	}
}

static INT32 MastkinInit()
{
	game_select = 1;
	nowatchdog = 1;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvM6809ROM + 0x8000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0xa000,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0xe000,  2, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1  + 0x0000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0  + 0x0000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x2000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x8000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0xa000,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1  + 0x0000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x2000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x4000, 10, 1)) return 1;

#if 0
		if (BurnLoadRom(DrvColPROM  + 0x0000, 11, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x0020, 12, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x0120, 13, 1)) return 1;
#endif

		if (BurnLoadRom(DrvSndROM   + 0x0000, 14, 1)) return 1;

		MastkinPaletteCreate();
		DrvGfxDecode();
	}

	M6809Init(1);
	M6809Open(0);
	M6809MapMemory(DrvSprRAM1,		0x1800, 0x1bff, MAP_RAM);
	// spr2 00-3f, scroll 40-5f
	M6809MapMemory(DrvSprRAM0,		0x1c00, 0x1fff, MAP_RAM);
	// spr  00-3f, scroll 40-5f
	M6809MapMemory(DrvZ80RAM0,		0x2000, 0x2bff, MAP_RAM);
	M6809MapMemory(DrvNVRAM,		0x2c00, 0x2fff, MAP_RAM);
	M6809MapMemory(DrvVidRAM,		0x3000, 0x37ff, MAP_RAM);
	M6809MapMemory(DrvColRAM,		0x3800, 0x3fff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM + 0x6000,	0x6000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(trackfld_main_write);
	M6809SetReadHandler(trackfld_main_read);
	M6809Close();

	ZetInit(0); // reaktor
	CommonSoundInit();

	nSpriteMask = 0xff;
	nCharMask = 0x3ff;

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}


static INT32 DrvExit()
{
	GenericTilesExit();

	if (game_select == 4) M6800Exit();
	if (game_select == 1 || game_select == 2) M6809Exit();
	if (game_select == 1 || game_select == 3) ZetExit();

	vlm5030Exit();
	if (game_select == 1 || game_select == 3) DACExit();
	SN76496Exit();

	nowatchdog = 0;

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	UINT32 tab[0x20];

	for (INT32 i = 0; i < 0x20; i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 0x01;
		INT32 r = bit0 * 33 + bit1 * 71 + bit2 * 151;

		bit0 = (DrvColPROM[i] >> 3) & 0x01;
		bit1 = (DrvColPROM[i] >> 4) & 0x01;
		bit2 = (DrvColPROM[i] >> 5) & 0x01;
		INT32 g = bit0 * 33 + bit1 * 71 + bit2 * 151;

		bit0 = (DrvColPROM[i] >> 6) & 0x01;
		bit1 = (DrvColPROM[i] >> 7) & 0x01;
		INT32 b = bit0 * 81 + bit1 * 174;

		tab[i] = BurnHighCol(r,g,b,0);
	}

	for (INT32 i = 0; i < 0x100; i++)
	{
		DrvPalette[0x000 + i] = tab[(DrvColPROM[i + 0x020] & 0x0f) | 0x00];
		DrvPalette[0x100 + i] = tab[(DrvColPROM[i + 0x120] & 0x0f) | 0x10];
	}
}

static void draw_layers()
{
	for (INT32 offs = 0; offs < 64 * 32; offs++)
	{
		INT32 sx = (offs & 0x3f) * 8;
		INT32 sy = (offs / 0x40) * 8;

		sy -= 16;

		INT32 attr  = DrvColRAM[offs];
		INT32 code  = DrvVidRAM[offs] + (((attr & 0xc0) << 2) | (bg_bank ? 0x400 : 0));
		INT32 color = attr & 0x0f;
		INT32 flipx = attr & 0x10;
		INT32 flipy = attr & 0x20;

		code &= nCharMask;

		sx -= DrvSprRAM1[0x40+(sy/8)+2] + ((DrvSprRAM0[0x40+(sy/8)+2] & 1) * 256);
		if (sx < -7) sx += 512;

		if (flipx) {
			if (flipy) {
				Render8x8Tile_FlipXY_Clip(pTransDraw, code, sx, sy, color, 4, 0x100, DrvGfxROM1);
			} else {
				Render8x8Tile_FlipX_Clip(pTransDraw, code, sx, sy, color, 4, 0x100, DrvGfxROM1);
			}
		} else {
			if (flipy) {
				Render8x8Tile_FlipY_Clip(pTransDraw, code, sx, sy, color, 4, 0x100, DrvGfxROM1);
			} else {
				Render8x8Tile_Clip(pTransDraw, code, sx, sy, color, 4, 0x100, DrvGfxROM1);
			}
		}
	}
}

static void draw_single_sprite(INT32 code, INT32 color, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy)
{
	if (flipy) {
		if (flipx) {
			Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0, DrvGfxROM0);
		} else {
			Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0, DrvGfxROM0);
		}
	} else {
		if (flipx) {
			Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0, DrvGfxROM0);
		} else {
			Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0, DrvGfxROM0);
		}
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0x40 - 2; offs >= 0; offs -= 2)
	{
		INT32 attr = DrvSprRAM1[offs];
		INT32 code = DrvSprRAM0[offs + 1];
		INT32 color = attr & 0x0f;
		if (game_select == 2 && (attr & 1)) code |= 0x100; // yiear
	//	code += m_sprite_bank1 + m_sprite_bank2;		// atlantol
		code &= nSpriteMask;
		INT32 flipx = ~attr & 0x40;
		INT32 flipy = attr & 0x80;
		INT32 sx = DrvSprRAM0[offs] - 1;
		INT32 sy = 240 - DrvSprRAM1[offs + 1];

		sy -= 16;

		if (flipscreen)
		{
			sy = 240 - sy;
			flipy = !flipy;
		}

		draw_single_sprite(code, color, sx      , sy + 1, flipx, flipy);
		draw_single_sprite(code, color, sx - 256, sy + 1, flipx, flipy);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 1;
	}

	BurnTransferClear();

	if (nBurnLayer & 1) draw_layers();
	if (nBurnLayer & 2) draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	watchdog++;
	if (watchdog >= 120 && !nowatchdog) {
		bprintf(0, _T("Watchdog tripped.\n"));
		DrvDoReset(0);
	}

	if (DrvReset) {
		DrvDoReset(1);
	}

	M6809NewFrame();
	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 3);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 1536000 / 60, 3579545 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	M6809Open(0);
	ZetOpen(1);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone[0] += M6809Run(nCyclesTotal[0] / nInterleave);

		if (i == (nInterleave-1) && irq_mask) M6809SetIRQLine(0, CPU_IRQSTATUS_AUTO);

		nCyclesDone[1] += ZetRun(nCyclesTotal[1] / nInterleave);
	}

	if (pBurnSoundOut) {
		vlm5030Update(0, pBurnSoundOut, nBurnSoundLen);
		SN76496Update(0, pBurnSoundOut, nBurnSoundLen);
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	M6809Close();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 YieartfFrame()
{
	watchdog++;
	if (watchdog >= 120) {
		DrvDoReset(0);
	}

	if (DrvReset) {
		DrvDoReset(1);
	}

	M6809NewFrame();

	{
		memset (DrvInputs, 0xff, 3);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 1536000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };
	INT32 nSoundBufferPos = 0;

	M6809Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone[0] += M6809Run(nCyclesTotal[0] / nInterleave);

		if ((i & 0xff) == 0xff && irq_mask) M6809SetIRQLine(0x00, CPU_IRQSTATUS_AUTO);
		if ((i & 0x1f) == 0x1f && nmi_mask) M6809SetIRQLine(0x20, CPU_IRQSTATUS_AUTO);

		// Render Sound Segment
		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			SN76496Update(0, pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	// Make sure the buffer is entirely filled.
	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			SN76496Update(0, pSoundBuf, nSegmentLength);
		}
		vlm5030Update(0, pBurnSoundOut, nBurnSoundLen);
	}

	M6809Close();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 ReaktorFrame()
{
	watchdog++;
	if (watchdog >= 120) {
		DrvDoReset(0);
	}

	if (DrvReset) {
		DrvDoReset(1);
	}

	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 3);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 3072000 / 60, 3579545 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);
		if (i == (nInterleave-1) && irq_mask) ZetSetIRQLine(0, CPU_IRQSTATUS_AUTO);
		ZetClose();

		ZetOpen(1);
		nCyclesDone[1] += ZetRun(nCyclesTotal[1] / nInterleave);
		ZetClose();

	}

	ZetOpen(1);

	if (pBurnSoundOut) {
		vlm5030Update(0, pBurnSoundOut, nBurnSoundLen);
		SN76496Update(0, pBurnSoundOut, nBurnSoundLen);
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();


	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 WizzquizFrame()
{
	watchdog++;
	if (watchdog >= 120) {
		DrvDoReset(0);
	}

	if (DrvReset) {
		DrvDoReset(1);
	}

	M6800NewFrame();
	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 3);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 2048000 / 60, 3579545 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

//	M6800Open(0);
	ZetOpen(1);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone[0] += M6800Run(nCyclesTotal[0] / nInterleave);

		if (i == 239 && irq_mask) M6800SetIRQLine(M6800_INPUT_LINE_NMI, CPU_IRQSTATUS_AUTO);

		nCyclesDone[1] += ZetRun(nCyclesTotal[1] / nInterleave);
	}

	if (pBurnSoundOut) {
		vlm5030Update(0, pBurnSoundOut, nBurnSoundLen);
		SN76496Update(0, pBurnSoundOut, nBurnSoundLen);
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
//	M6800Close();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029705;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		if (game_select == 4) M6800Scan(nAction);
		if (game_select == 1 || game_select == 2) M6809Scan(nAction);
		if (game_select == 1 || game_select == 3) ZetScan(nAction);
		if (game_select == 1 || game_select == 3) DACScan(nAction, pnMin);

		SN76496Scan(nAction, pnMin);
		vlm5030Scan(nAction);

		SCAN_VAR(watchdog);
		SCAN_VAR(bg_bank);
		SCAN_VAR(soundlatch);
		SCAN_VAR(flipscreen);
		SCAN_VAR(irq_mask);
		SCAN_VAR(nmi_mask);
		SCAN_VAR(last_addr);
		SCAN_VAR(last_sound_irq);
		SCAN_VAR(SN76496_latch);
	}

	return 0;
}


// Track & Field

static struct BurnRomInfo trackfldRomDesc[] = {
	{ "a01_e01.bin",	0x2000, 0x2882f6d4, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "a02_e02.bin",	0x2000, 0x1743b5ee, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a03_k03.bin",	0x2000, 0x6c0d1ee9, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "a04_e04.bin",	0x2000, 0x21d6c448, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "a05_e05.bin",	0x2000, 0xf08c7b7e, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "c2_d13.bin",		0x2000, 0x95bf79b6, 2 | BRF_PRG | BRF_ESS }, //  5 audiocpu

	{ "c11_d06.bin",	0x2000, 0x82e2185a, 3 | BRF_GRA },           //  6 gfx1
	{ "c12_d07.bin",	0x2000, 0x800ff1f1, 3 | BRF_GRA },           //  7
	{ "c13_d08.bin",	0x2000, 0xd9faf183, 3 | BRF_GRA },           //  8
	{ "c14_d09.bin",	0x2000, 0x5886c802, 3 | BRF_GRA },           //  9

	{ "h16_e12.bin",	0x2000, 0x50075768, 4 | BRF_GRA },           // 10 gfx2
	{ "h15_e11.bin",	0x2000, 0xdda9e29f, 4 | BRF_GRA },           // 11
	{ "h14_e10.bin",	0x2000, 0xc2166a5c, 4 | BRF_GRA },           // 12

	{ "361b16.f1",		0x0020, 0xd55f30b5, 5 | BRF_GRA },           // 13 proms
	{ "361b17.b16",		0x0100, 0xd2ba4d32, 5 | BRF_GRA },           // 14
	{ "361b18.e15",		0x0100, 0x053e5861, 5 | BRF_GRA },           // 15

	{ "c9_d15.bin",		0x2000, 0xf546a56b, 6 | BRF_GRA },           // 16 vlm
};

STD_ROM_PICK(trackfld)
STD_ROM_FN(trackfld)

struct BurnDriver BurnDrvTrackfld = {
	"trackfld", NULL, NULL, NULL, "1983",
	"Track & Field\0", NULL, "Konami", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PREFIX_KONAMI, GBF_SPORTSMISC, 0,
	NULL, trackfldRomInfo, trackfldRomName, NULL, NULL, TrackfldInputInfo, TrackfldDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// Track & Field (Centuri)

static struct BurnRomInfo trackfldcRomDesc[] = {
	{ "f01.1a",		0x2000, 0x4e32b360, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "f02.2a",		0x2000, 0x4e7ebf07, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "l03.3a",		0x2000, 0xfef4c0ea, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "f04.4a",		0x2000, 0x73940f2d, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "f05.5a",		0x2000, 0x363fd761, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "c2_d13.bin",		0x2000, 0x95bf79b6, 2 | BRF_PRG | BRF_ESS }, //  5 audiocpu

	{ "c11_d06.bin",	0x2000, 0x82e2185a, 3 | BRF_GRA },           //  6 gfx1
	{ "c12_d07.bin",	0x2000, 0x800ff1f1, 3 | BRF_GRA },           //  7
	{ "c13_d08.bin",	0x2000, 0xd9faf183, 3 | BRF_GRA },           //  8
	{ "c14_d09.bin",	0x2000, 0x5886c802, 3 | BRF_GRA },           //  9

	{ "h16_e12.bin",	0x2000, 0x50075768, 4 | BRF_GRA },           // 10 gfx2
	{ "h15_e11.bin",	0x2000, 0xdda9e29f, 4 | BRF_GRA },           // 11
	{ "h14_e10.bin",	0x2000, 0xc2166a5c, 4 | BRF_GRA },           // 12

	{ "361b16.f1",		0x0020, 0xd55f30b5, 5 | BRF_GRA },           // 13 proms
	{ "361b17.b16",		0x0100, 0xd2ba4d32, 5 | BRF_GRA },           // 14
	{ "361b18.e15",		0x0100, 0x053e5861, 5 | BRF_GRA },           // 15

	{ "c9_d15.bin",		0x2000, 0xf546a56b, 6 | BRF_GRA },           // 16 vlm
};

STD_ROM_PICK(trackfldc)
STD_ROM_FN(trackfldc)

struct BurnDriver BurnDrvTrackfldc = {
	"trackfldc", "trackfld", NULL, NULL, "1983",
	"Track & Field (Centuri)\0", NULL, "Konami (Centuri license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, trackfldcRomInfo, trackfldcRomName, NULL, NULL, TrackfldInputInfo, TrackfldDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// Hyper Olympic

static struct BurnRomInfo hyprolymRomDesc[] = {
	{ "361-d01.a01",	0x2000, 0x82257fb7, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "361-d02.a02",	0x2000, 0x15b83099, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "361-d03.a03",	0x2000, 0xe54cc960, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "361-d04.a04",	0x2000, 0xd099b1e8, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "361-d05.a05",	0x2000, 0x974ff815, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "c2_d13.bin",		0x2000, 0x95bf79b6, 2 | BRF_PRG | BRF_ESS }, //  5 audiocpu

	{ "c11_d06.bin",	0x2000, 0x82e2185a, 3 | BRF_GRA },           //  6 gfx1
	{ "c12_d07.bin",	0x2000, 0x800ff1f1, 3 | BRF_GRA },           //  7
	{ "c13_d08.bin",	0x2000, 0xd9faf183, 3 | BRF_GRA },           //  8
	{ "c14_d09.bin",	0x2000, 0x5886c802, 3 | BRF_GRA },           //  9

	{ "361-d12.h16",	0x2000, 0x768bb63d, 4 | BRF_GRA },           // 10 gfx2
	{ "361-d11.h15",	0x2000, 0x3af0e2a8, 4 | BRF_GRA },           // 11
	{ "h14_e10.bin",	0x2000, 0xc2166a5c, 4 | BRF_GRA },           // 12

	{ "361b16.f1",		0x0020, 0xd55f30b5, 5 | BRF_GRA },           // 13 proms
	{ "361b17.b16",		0x0100, 0xd2ba4d32, 5 | BRF_GRA },           // 14
	{ "361b18.e15",		0x0100, 0x053e5861, 5 | BRF_GRA },           // 15

	{ "c9_d15.bin",		0x2000, 0xf546a56b, 6 | BRF_GRA },           // 16 vlm
};

STD_ROM_PICK(hyprolym)
STD_ROM_FN(hyprolym)

struct BurnDriver BurnDrvHyprolym = {
	"hyprolym", "trackfld", NULL, NULL, "1983",
	"Hyper Olympic\0", NULL, "Konami", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, hyprolymRomInfo, hyprolymRomName, NULL, NULL, TrackfldInputInfo, TrackfldDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// Track & Field (NZ bootleg?)

static struct BurnRomInfo trackfldnzRomDesc[] = {
	{ "gold.7a",	0x2000, 0x77ea4509, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "gold.6a",	0x2000, 0xa13f3131, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gold.5a",	0x2000, 0xb0abe171, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gold.4a",	0x2000, 0xfee9b922, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "gold.2a",	0x2000, 0xad6dc048, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "gold.2d",	0x2000, 0x95bf79b6, 2 | BRF_PRG | BRF_ESS }, //  5 audiocpu

	{ "gold.20a",	0x2000, 0x82e2185a, 3 | BRF_GRA },           //  6 gfx1
	{ "gold.21a",	0x2000, 0x800ff1f1, 3 | BRF_GRA },           //  7
	{ "gold.17a",	0x2000, 0xd9faf183, 3 | BRF_GRA },           //  8
	{ "gold.19a",	0x2000, 0x5886c802, 3 | BRF_GRA },           //  9

	{ "gold.2k",	0x2000, 0x50075768, 4 | BRF_GRA },           // 10 gfx2
	{ "gold.4k",	0x2000, 0xdda9e29f, 4 | BRF_GRA },           // 11
	{ "gold.5k",	0x2000, 0xc2166a5c, 4 | BRF_GRA },           // 12

	{ "gold.2g",	0x0020, 0xd55f30b5, 5 | BRF_GRA },           // 13 proms
	{ "gold.18d",	0x0100, 0xd2ba4d32, 5 | BRF_GRA },           // 14
	{ "gold.4j",	0x0100, 0x053e5861, 5 | BRF_GRA },           // 15

	{ "gold.d9",	0x2000, 0xf546a56b, 6 | BRF_GRA },           // 16 vlm
};

STD_ROM_PICK(trackfldnz)
STD_ROM_FN(trackfldnz)

struct BurnDriver BurnDrvTrackfldnz = {
	"trackfldnz", "trackfld", NULL, NULL, "1982",
	"Track & Field (NZ bootleg?)\0", NULL, "bootleg? (Goldberg Enterprizes Inc.)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, trackfldnzRomInfo, trackfldnzRomName, NULL, NULL, TrackfldInputInfo, TrackfldDIPInfo,
	TrackfldnzInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// Hyper Olympic (bootleg, set 1)

static struct BurnRomInfo hyprolymbRomDesc[] = {
	{ "blue.a1",	0x2000, 0x82257fb7, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "2.a2",	0x2000, 0x15b83099, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.a4",	0x2000, 0x2d6fc308, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.a5",	0x2000, 0xd099b1e8, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "5.a7",	0x2000, 0x974ff815, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "c2_d13.bin",	0x2000, 0x95bf79b6, 2 | BRF_PRG | BRF_ESS }, //  5 audiocpu

	{ "2764.1",	0x2000, 0xa4cddeb8, 3 | BRF_SND },           //  6 adpcm
	{ "2764.2",	0x2000, 0xe9919365, 3 | BRF_SND },           //  7
	{ "2764.3",	0x2000, 0xc3ec42e1, 3 | BRF_SND },           //  8
	{ "2764.4",	0x2000, 0x76998389, 3 | BRF_SND },           //  9

	{ "6.a18",	0x2000, 0x82e2185a, 4 | BRF_GRA },           // 10 gfx1
	{ "7.a19",	0x2000, 0x800ff1f1, 4 | BRF_GRA },           // 11
	{ "8.a21",	0x2000, 0xd9faf183, 4 | BRF_GRA },           // 12
	{ "9.a22",	0x2000, 0x5886c802, 4 | BRF_GRA },           // 13

	{ "12.h22",	0x2000, 0x768bb63d, 5 | BRF_GRA },           // 14 gfx2
	{ "11.h21",	0x2000, 0x3af0e2a8, 5 | BRF_GRA },           // 15
	{ "10.h19",	0x2000, 0xc2166a5c, 5 | BRF_GRA },           // 16

	{ "361b16.e1",	0x0020, 0xd55f30b5, 6 | BRF_GRA },           // 17 proms
	{ "361b17.b15",	0x0100, 0xd2ba4d32, 6 | BRF_GRA },           // 18
	{ "361b18.f22",	0x0100, 0x053e5861, 6 | BRF_GRA },           // 19

	{ "pal16l8.e4",	0x0104, 0x641efc84, 7 | BRF_OPT },           // 20 plds
	{ "pal16l8.e6",	0x0104, 0x122f23e6, 7 | BRF_OPT },           // 21
};

STD_ROM_PICK(hyprolymb)
STD_ROM_FN(hyprolymb)

static INT32 bootInit()
{
	return 1;
}

struct BurnDriver BurnDrvHyprolymb = {
	"hyprolymb", "trackfld", NULL, NULL, "1983",
	"Hyper Olympic (bootleg, set 1)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	0 | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, hyprolymbRomInfo, hyprolymbRomName, NULL, NULL, TrackfldInputInfo, TrackfldDIPInfo,
	bootInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// Hyper Olympic (bootleg, set 2)

static struct BurnRomInfo hyprolymbaRomDesc[] = {
	{ "1.a1",	0x2000, 0x9aee2d5a, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "2.a2",	0x2000, 0x15b83099, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.a4",	0x2000, 0x2d6fc308, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.a5",	0x2000, 0xd099b1e8, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "5.a7",	0x2000, 0x974ff815, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "c2_d13.bin",	0x2000, 0x95bf79b6, 2 | BRF_PRG | BRF_ESS }, //  5 audiocpu

	{ "2764.1",	0x2000, 0xa4cddeb8, 3 | BRF_SND },           //  6 adpcm
	{ "2764.2",	0x2000, 0xe9919365, 3 | BRF_SND },           //  7
	{ "2764.3",	0x2000, 0xc3ec42e1, 3 | BRF_SND },           //  8
	{ "2764.4",	0x2000, 0x76998389, 3 | BRF_SND },           //  9

	{ "6.a18",	0x2000, 0x82e2185a, 4 | BRF_GRA },           // 10 gfx1
	{ "7.a19",	0x2000, 0x800ff1f1, 4 | BRF_GRA },           // 11
	{ "8.a21",	0x2000, 0xd9faf183, 4 | BRF_GRA },           // 12
	{ "9.a22",	0x2000, 0x5886c802, 4 | BRF_GRA },           // 13

	{ "12.h22",	0x2000, 0x768bb63d, 5 | BRF_GRA },           // 14 gfx2
	{ "11.h21",	0x2000, 0x3af0e2a8, 5 | BRF_GRA },           // 15
	{ "10.h19",	0x2000, 0xc2166a5c, 5 | BRF_GRA },           // 16

	{ "361b16.e1",	0x0020, 0xd55f30b5, 6 | BRF_GRA },           // 17 proms
	{ "361b17.b15",	0x0100, 0xd2ba4d32, 6 | BRF_GRA },           // 18
	{ "361b18.f22",	0x0100, 0x053e5861, 6 | BRF_GRA },           // 19

	{ "pal16l8.e4",	0x0104, 0x641efc84, 7 | BRF_OPT },           // 20 plds
	{ "pal16l8.e6",	0x0104, 0x122f23e6, 7 | BRF_OPT },           // 21
};

STD_ROM_PICK(hyprolymba)
STD_ROM_FN(hyprolymba)

struct BurnDriver BurnDrvHyprolymba = {
	"hyprolymba", "trackfld", NULL, NULL, "1983",
	"Hyper Olympic (bootleg, set 2)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	0 | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, hyprolymbaRomInfo, hyprolymbaRomName, NULL, NULL, TrackfldInputInfo, TrackfldDIPInfo,
	bootInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// Hipoly (bootleg of Hyper Olympic)

static struct BurnRomInfo hipolyRomDesc[] = {
	{ "2.1a",	0x2000, 0x82257fb7, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "2.2a",	0x2000, 0x15b83099, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "2.4a",	0x2000, 0x93a32a97, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "2.5a",	0x2000, 0xd099b1e8, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "2.7a",	0x2000, 0x974ff815, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "1.2c",	0x2000, 0x95bf79b6, 2 | BRF_PRG | BRF_ESS }, //  5 audiocpu

	{ "1.11d",	0x2000, 0x102d3a78, 3 | BRF_SND },           //  6 adpcm
	{ "1.10d",	0x2000, 0xe9919365, 3 | BRF_SND },           //  7
	{ "1.11c",	0x2000, 0xc3ec42e1, 3 | BRF_SND },           //  8
	{ "1.10c",	0x2000, 0x76998389, 3 | BRF_SND },           //  9

	{ "2.18a",	0x2000, 0x8d28864f, 4 | BRF_GRA },           // 10 gfx1
	{ "2.19a",	0x2000, 0x800ff1f1, 4 | BRF_GRA },           // 11
	{ "2.21a",	0x2000, 0xd9faf183, 4 | BRF_GRA },           // 12
	{ "2.22a",	0x2000, 0x5886c802, 4 | BRF_GRA },           // 13

	{ "2.22h",	0x2000, 0x6c107a9c, 5 | BRF_GRA },           // 14 gfx2
	{ "2.21h",	0x2000, 0x21847e56, 5 | BRF_GRA },           // 15
	{ "2.19h",	0x2000, 0xc2166a5c, 5 | BRF_GRA },           // 16

	{ "361b16.e1",	0x0020, 0xd55f30b5, 6 | BRF_GRA },           // 17 proms
	{ "361b17.b15",	0x0100, 0xd2ba4d32, 6 | BRF_GRA },           // 18
	{ "361b18.f22",	0x0100, 0x053e5861, 6 | BRF_GRA },           // 19

	{ "pal16l8.e4",	0x0104, 0x00000000, 7 | BRF_NODUMP | BRF_OPT },           // 20 plds
	{ "pal16l8.e6",	0x0104, 0x00000000, 7 | BRF_NODUMP | BRF_OPT },           // 21
};

STD_ROM_PICK(hipoly)
STD_ROM_FN(hipoly)

struct BurnDriver BurnDrvHipoly = {
	"hipoly", "trackfld", NULL, NULL, "1983",
	"Hipoly (bootleg of Hyper Olympic)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, hipolyRomInfo, hipolyRomName, NULL, NULL, TrackfldInputInfo, TrackfldDIPInfo,
	bootInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// Atlant Olimpic

static struct BurnRomInfo atlantolRomDesc[] = {
	{ "atl37",	0x20000, 0xaca8da51, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu

	{ "atl35",	0x20000, 0x03331597, 2 | BRF_PRG | BRF_ESS }, //  1 audiocpu

	{ "atl36",	0x20000, 0x0bae8489, 3 | BRF_SND },           //  2 adpcm

	{ "atl38",	0x20000, 0xdbbcbcda, 4 | BRF_GRA },           //  3 gfx1
	{ "atl39",	0x20000, 0xd08f067f, 4 | BRF_GRA },           //  4

	{ "atl40",	0x20000, 0xc915f53a, 5 | BRF_GRA },           //  5 gfx2

	{ "361b16.f1",	0x00020, 0xd55f30b5, 6 | BRF_GRA },           //  6 proms
	{ "361b17.b16",	0x00100, 0xd2ba4d32, 6 | BRF_GRA },           //  7
	{ "361b18.e15",	0x00100, 0x053e5861, 6 | BRF_GRA },           //  8
};

STD_ROM_PICK(atlantol)
STD_ROM_FN(atlantol)

struct BurnDriver BurnDrvAtlantol = {
	"atlantol", "trackfld", NULL, NULL, "1996",
	"Atlant Olimpic\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	0 | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, atlantolRomInfo, atlantolRomName, NULL, NULL, TrackfldInputInfo, TrackfldDIPInfo, //AtlantolInputInfo, AtlantolDIPInfo,
	bootInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	256, 224, 4, 3
};


// Yie Ar Kung-Fu (GX361 conversion)

static struct BurnRomInfo yieartfRomDesc[] = {
	{ "2.2a",	0x2000, 0x349430e9, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "3.3a",	0x2000, 0x17d8337b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "4.4a",	0x2000, 0xa89a2166, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "5.5a",	0x2000, 0xff1599eb, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "a.15c",	0x4000, 0x45109b29, 2 | BRF_GRA },           //  4 gfx1
	{ "b.16c",	0x4000, 0x1d650790, 2 | BRF_GRA },           //  5
	{ "c.17c",	0x4000, 0xe6aa945b, 2 | BRF_GRA },           //  6
	{ "d.18c",	0x4000, 0xcc187c22, 2 | BRF_GRA },           //  7

	{ "6.16h",	0x2000, 0x05a23af3, 3 | BRF_GRA },           //  8 gfx2
	{ "7.15h",	0x2000, 0x988154fa, 3 | BRF_GRA },           //  9

	{ "yiear.clr",	0x0020, 0xc283d71f, 4 | BRF_GRA },           // 10 proms
	{ "prom1.b16",	0x0100, 0x93dc32a0, 4 | BRF_GRA },           // 11
	{ "prom2.e15",	0x0100, 0xe7e0f9e5, 4 | BRF_GRA },           // 12

	{ "01.snd",	0x2000, 0xf75a1539, 5 | BRF_GRA },           // 13 vlm
};

STD_ROM_PICK(yieartf)
STD_ROM_FN(yieartf)

struct BurnDriver BurnDrvYieartf = {
	"yieartf", "yiear", NULL, NULL, "1985",
	"Yie Ar Kung-Fu (GX361 conversion)\0", NULL, "Konami", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_VSFIGHT, 0,
	NULL, yieartfRomInfo, yieartfRomName, NULL, NULL, YieartfInputInfo, YieartfDIPInfo,
	YieartfInit, DrvExit, YieartfFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// Reaktor (Track & Field conversion)

static struct BurnRomInfo reaktorRomDesc[] = {
	{ "prog3.bin",	0x8000, 0x8ba956fa, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu

	{ "2c.bin",	0x4000, 0x105a8beb, 2 | BRF_PRG | BRF_ESS }, //  1 audiocpu

	{ "11c.bin",	0x4000, 0xd24553fa, 3 | BRF_GRA },           //  2 gfx1
	{ "14c.bin",	0x4000, 0x4d0ab831, 3 | BRF_GRA },           //  3
	{ "12c.bin",	0x2000, 0xd0d39e66, 3 | BRF_GRA },           //  4
	{ "15c.bin",	0x4000, 0xbf1e608d, 3 | BRF_GRA },           //  5

	{ "16h.bin",	0x4000, 0xcb062c3b, 4 | BRF_GRA },           //  6 gfx2
	{ "15h.bin",	0x4000, 0xdf83e659, 4 | BRF_GRA },           //  7
	{ "14h.bin",	0x4000, 0x5ca53215, 4 | BRF_GRA },           //  8

	{ "361b16.f1",	0x0020, 0xd55f30b5, 5 | BRF_GRA },           //  9 proms
	{ "361b17.b16",	0x0100, 0xd2ba4d32, 5 | BRF_GRA },           // 10
	{ "361b18.e15",	0x0100, 0x053e5861, 5 | BRF_GRA },           // 11

	{ "c9_d15.bin",	0x2000, 0xf546a56b, 6 | BRF_GRA },           // 12 vlm
};

STD_ROM_PICK(reaktor)
STD_ROM_FN(reaktor)

struct BurnDriver BurnDrvReaktor = {
	"reaktor", NULL, NULL, NULL, "1987",
	"Reaktor (Track & Field conversion)\0", NULL, "Zilec", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_BREAKOUT, 0,
	NULL, reaktorRomInfo, reaktorRomName, NULL, NULL, ReaktorInputInfo, ReaktorDIPInfo,
	ReaktorInit, DrvExit, ReaktorFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Wizz Quiz (Konami version)

static struct BurnRomInfo wizzquizRomDesc[] = {
	{ "pros.rom",	0x2000, 0x4c858841, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu

	{ "sn1.rom",	0x8000, 0x0ae28676, 2 | BRF_GRA },           //  1 user1
	{ "sn2.rom",	0x8000, 0xf2b7374a, 2 | BRF_GRA },           //  2
	{ "tvmov1.rom",	0x8000, 0x921f551d, 2 | BRF_GRA },           //  3
	{ "tvmov2.rom",	0x8000, 0x1ed44df6, 2 | BRF_GRA },           //  4
	{ "sport1.rom",	0x8000, 0x3b7f2ce4, 2 | BRF_GRA },           //  5
	{ "sport2.rom",	0x8000, 0x14dbfa23, 2 | BRF_GRA },           //  6
	{ "pop1.rom",	0x8000, 0x61f60def, 2 | BRF_GRA },           //  7
	{ "pop2.rom",	0x8000, 0x5a5b41cd, 2 | BRF_GRA },           //  8

	{ "zandz.2c",	0x2000, 0x3daca93a, 3 | BRF_PRG | BRF_ESS }, //  9 audiocpu

	{ "rom.11c",	0x2000, 0x87d060d4, 4 | BRF_GRA },           // 10 gfx1
	{ "rom.14c",	0x2000, 0x5bff1607, 4 | BRF_GRA },           // 11

	{ "rom.16h",	0x2000, 0xe6728bda, 5 | BRF_GRA },           // 12 gfx2
	{ "rom.15h",	0x2000, 0x9c067ef4, 5 | BRF_GRA },           // 13
	{ "rom.14h",	0x2000, 0x3bbad920, 5 | BRF_GRA },           // 14

	{ "361b16.f1",	0x0020, 0xd55f30b5, 6 | BRF_GRA },           // 15 proms
	{ "361b17.b16",	0x0100, 0xd2ba4d32, 6 | BRF_GRA },           // 16
	{ "361b18.e15",	0x0100, 0x053e5861, 6 | BRF_GRA },           // 17
};

STD_ROM_PICK(wizzquiz)
STD_ROM_FN(wizzquiz)

struct BurnDriver BurnDrvWizzquiz = {
	"wizzquiz", NULL, NULL, NULL, "1985",
	"Wizz Quiz (Konami version)\0", NULL, "Zilec-Zenitone (Konami license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	0, 2, HARDWARE_MISC_PRE90S, GBF_QUIZ, 0,
	NULL, wizzquizRomInfo, wizzquizRomName, NULL, NULL, WizzquizInputInfo, WizzquizDIPInfo,
	WizzquizInit, DrvExit, WizzquizFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 256, 4, 3
};


// Wizz Quiz (version 4)

static struct BurnRomInfo wizzquizaRomDesc[] = {
	{ "ic9_a1.bin",		0x2000, 0x608e1ff3, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu

	{ "ic1_q06.bin",	0x8000, 0xc62f25b1, 2 | BRF_GRA },           //  1 user1
	{ "ic2_q28.bin",	0x8000, 0x2bd00476, 2 | BRF_GRA },           //  2
	{ "ic3_q27.bin",	0x8000, 0x46d28aaf, 2 | BRF_GRA },           //  3
	{ "ic4_q23.bin",	0x8000, 0x3f46f702, 2 | BRF_GRA },           //  4
	{ "ic5_q26.bin",	0x8000, 0x9d130515, 2 | BRF_GRA },           //  5
	{ "ic6_q09.bin",	0x8000, 0x636f89b4, 2 | BRF_GRA },           //  6
	{ "ic7_q15.bin",	0x8000, 0xb35332b1, 2 | BRF_GRA },           //  7
	{ "ic8_q19.bin",	0x8000, 0x8d152da0, 2 | BRF_GRA },           //  8

	{ "02c.bin",		0x2000, 0x3daca93a, 3 | BRF_PRG | BRF_ESS }, //  9 audiocpu

	{ "11c.bin",		0x2000, 0x87d060d4, 4 | BRF_GRA },           // 10 gfx1
	{ "14c.bin",		0x2000, 0x5bff1607, 4 | BRF_GRA },           // 11

	{ "16h.bin",		0x2000, 0xe6728bda, 5 | BRF_GRA },           // 12 gfx2
	{ "15h.bin",		0x2000, 0x9c067ef4, 5 | BRF_GRA },           // 13
	{ "14h.bin",		0x2000, 0x3bbad920, 5 | BRF_GRA },           // 14

	{ "361b16.f1",		0x0020, 0xd55f30b5, 6 | BRF_GRA },           // 15 proms
	{ "361b17.b16",		0x0100, 0xd2ba4d32, 6 | BRF_GRA },           // 16
	{ "361b18.e15",		0x0100, 0x053e5861, 6 | BRF_GRA },           // 17
};

STD_ROM_PICK(wizzquiza)
STD_ROM_FN(wizzquiza)

struct BurnDriver BurnDrvWizzquiza = {
	"wizzquiza", "wizzquiz", NULL, NULL, "1985",
	"Wizz Quiz (version 4)\0", NULL, "Zilec-Zenitone", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	0 | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_QUIZ, 0,
	NULL, wizzquizaRomInfo, wizzquizaRomName, NULL, NULL, WizzquizInputInfo, WizzquizDIPInfo,
	WizzquizInit, DrvExit, WizzquizFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 256, 4, 3
};


// The Masters of Kin

static struct BurnRomInfo mastkinRomDesc[] = {
	{ "mk3",	0x2000, 0x9f80d6ae, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "mk4",	0x2000, 0x99f361e7, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mk5",	0x2000, 0x143d76ce, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "mk1",	0x2000, 0x95bf79b6, 2 | BRF_PRG | BRF_ESS }, //  3 audiocpu

	{ "mk6",	0x2000, 0x18fbe047, 3 | BRF_GRA },           //  4 gfx1
	{ "mk7",	0x2000, 0x47dee791, 3 | BRF_GRA },           //  5
	{ "mk8",	0x2000, 0x9c091ead, 3 | BRF_GRA },           //  6
	{ "mk9",	0x2000, 0x5c8ed3fe, 3 | BRF_GRA },           //  7

	{ "mk12",	0x2000, 0x8b1a19cf, 4 | BRF_GRA },           //  8 gfx2
	{ "mk11",	0x2000, 0x1a56d24d, 4 | BRF_GRA },           //  9
	{ "mk10",	0x2000, 0xe7d05634, 4 | BRF_GRA },           // 10

	{ "prom.1",	0x0020, 0x00000000, 5 | BRF_NODUMP | BRF_GRA },           // 11 proms
	{ "prom.3",	0x0100, 0x00000000, 5 | BRF_NODUMP | BRF_GRA },           // 12
	{ "prom.2",	0x0100, 0x00000000, 5 | BRF_NODUMP | BRF_GRA },           // 13

	{ "mk2",	0x2000, 0xf546a56b, 6 | BRF_GRA },           // 14 vlm
};

STD_ROM_PICK(mastkin)
STD_ROM_FN(mastkin)

struct BurnDriver BurnDrvMastkin = {
	"mastkin", NULL, NULL, NULL, "1988",
	"The Masters of Kin\0", "Colors are wrong", "Du Tech", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, mastkinRomInfo, mastkinRomName, NULL, NULL, MastkinInputInfo, MastkinDIPInfo,
	MastkinInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};
