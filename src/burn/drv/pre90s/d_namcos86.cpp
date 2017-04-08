// FB Alpha Namco System 86 driver module
// Based on MAME driver by Nicola Salmoria

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "m6800_intf.h"
#include "burn_ym2151.h"
#include "namco_snd.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvMainROM;
static UINT8 *DrvSubROM;
static UINT8 *DrvMCUROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvSndROM;
static UINT8 *DrvColPROM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvVidRAM0;
static UINT8 *DrvVidRAM1;
static UINT8 *DrvMCURAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 buffer_sprites;
static INT32 watchdog;
static INT32 watchdog1;
static INT32 backcolor;
static INT32 tilebank;
static INT32 flipscreen;
static UINT8 scroll[4][3];
static UINT8 nBankData[2];

static INT32 gfxlen[3];
static INT32 has_pcm = 0;
static INT32 enable_bankswitch2 = 0;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

struct voice_63701x
{
	INT32 select;
	INT32 playing;
	INT32 base_addr;
	INT32 position;
	INT32 volume;
	INT32 silence_counter;
};

struct voice_63701x m_voices[2];

static struct BurnInputInfo CommonInputList[] = {
	{"P1 coin",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy2 + 4,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Common)

static struct BurnInputInfo HopmappyInputList[] = {
	{"P1 coin",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy2 + 4,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Hopmappy)

static struct BurnInputInfo RoishtarInputList[] = {
	{"Coin A",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 coin"	},
	{"Coin B",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 coin"	},
	{"Start 1",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"	},
	{"Start 2",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 start"	},

	{"P1 Left Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Left Down",	BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left Left",	BIT_DIGITAL,	DrvJoy3 + 4,	"p1 left"	},
	{"P1 Left Right",	BIT_DIGITAL,	DrvJoy3 + 5,	"p1 right"	},
	{"P1 Right Up",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 up"		},
	{"P1 Right Down",	BIT_DIGITAL,	DrvJoy1 + 3,	"p2 down"	},
	{"P1 Right Left",	BIT_DIGITAL,	DrvJoy3 + 7,	"p2 left"	},
	{"P1 Right Right",	BIT_DIGITAL,	DrvJoy1 + 4,	"p2 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy1 + 7,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy2 + 4,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Roishtar)

static struct BurnDIPInfo SkykiddxDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x12, 0x01, 0x03, 0x00, "3 Coins / 1 Credit"		},
	{0x12, 0x01, 0x03, 0x01, "2 Coins / 1 Credit"		},
	{0x12, 0x01, 0x03, 0x03, "1 Coin / 1 Credit"		},
	{0x12, 0x01, 0x03, 0x02, "1 Coin / 2 Credits"		},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x12, 0x01, 0x04, 0x04, "Off"			},
	{0x12, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Level Select (Cheat)"	},
	{0x12, 0x01, 0x08, 0x08, "Off"			},
	{0x12, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x10, 0x00, "Off"			},
	{0x12, 0x01, 0x10, 0x10, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x12, 0x01, 0x60, 0x00, "3 Coins / 1 Credit"		},
	{0x12, 0x01, 0x60, 0x20, "2 Coins / 1 Credit"		},
	{0x12, 0x01, 0x60, 0x60, "1 Coin / 1 Credit"		},
	{0x12, 0x01, 0x60, 0x40, "1 Coin / 2 Credits"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x12, 0x01, 0x80, 0x80, "Off"			},
	{0x12, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x01, 0x01, "Off"			},
	{0x13, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x13, 0x01, 0x30, 0x00, "20000 Every 80000"	},
	{0x13, 0x01, 0x30, 0x10, "20000 80000"		},
	{0x13, 0x01, 0x30, 0x20, "30000 Every 90000"	},
	{0x13, 0x01, 0x30, 0x30, "30000 90000"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0xc0, 0x80, "1"			},
	{0x13, 0x01, 0xc0, 0x40, "2"			},
	{0x13, 0x01, 0xc0, 0xc0, "3"			},
	{0x13, 0x01, 0xc0, 0x00, "5"			},
};

STDDIPINFO(Skykiddx)

static struct BurnDIPInfo HopmappyDIPList[]=
{
	{0x10, 0xff, 0xff, 0xff, NULL			},
	{0x11, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x10, 0x01, 0x03, 0x00, "3 Coins / 1 Credit"		},
	{0x10, 0x01, 0x03, 0x01, "2 Coins / 1 Credit"		},
	{0x10, 0x01, 0x03, 0x03, "1 Coin / 1 Credit"		},
	{0x10, 0x01, 0x03, 0x02, "1 Coin / 2 Credits"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x10, 0x01, 0x04, 0x00, "No"			},
	{0x10, 0x01, 0x04, 0x04, "Yes"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x10, 0x01, 0x18, 0x10, "1"			},
	{0x10, 0x01, 0x18, 0x08, "2"			},
	{0x10, 0x01, 0x18, 0x18, "3"			},
	{0x10, 0x01, 0x18, 0x00, "5"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x10, 0x01, 0x60, 0x00, "3C 1C"		},
	{0x10, 0x01, 0x60, 0x20, "2C 1C"		},
	{0x10, 0x01, 0x60, 0x60, "1C 1C"		},
	{0x10, 0x01, 0x60, 0x40, "1C 2C"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x10, 0x01, 0x80, 0x80, "Off"			},
	{0x10, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x11, 0x01, 0x01, 0x01, "Upright"		},
	{0x11, 0x01, 0x01, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Level Select (Cheat)"	},
	{0x11, 0x01, 0x10, 0x10, "Off"			},
	{0x11, 0x01, 0x10, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x11, 0x01, 0x20, 0x20, "Off"			},
	{0x11, 0x01, 0x20, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x11, 0x01, 0x40, 0x00, "Off"			},
	{0x11, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x11, 0x01, 0x80, 0x80, "Easy"			},
	{0x11, 0x01, 0x80, 0x00, "Hard"			},
};

STDDIPINFO(Hopmappy)

static struct BurnDIPInfo GenpeitdDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0xfc, NULL			},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x12, 0x01, 0x03, 0x00, "3 Coins / 1 Credit"		},
	{0x12, 0x01, 0x03, 0x01, "2 Coins / 1 Credit"		},
	{0x12, 0x01, 0x03, 0x03, "1 Coin / 1 Credit"		},
	{0x12, 0x01, 0x03, 0x02, "1 Coin / 2 Credits"		},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x12, 0x01, 0x04, 0x04, "Off"			},
	{0x12, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x12, 0x01, 0x08, 0x00, "No"			},
	{0x12, 0x01, 0x08, 0x08, "Yes"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x10, 0x00, "Off"			},
	{0x12, 0x01, 0x10, 0x10, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x12, 0x01, 0x60, 0x00, "3 Coins / 1 Credit"		},
	{0x12, 0x01, 0x60, 0x20, "2 Coins / 1 Credit"		},
	{0x12, 0x01, 0x60, 0x60, "1 Coin / 1 Credit"		},
	{0x12, 0x01, 0x60, 0x40, "1 Coin / 2 Credits"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x12, 0x01, 0x80, 0x80, "Off"			},
	{0x12, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x01, 0x00, "Off"			},
	{0x13, 0x01, 0x01, 0x01, "On"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x13, 0x01, 0x02, 0x00, "Upright"		},
	{0x13, 0x01, 0x02, 0x02, "Cocktail"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x30, 0x20, "Easy"			},
	{0x13, 0x01, 0x30, 0x30, "Normal"		},
	{0x13, 0x01, 0x30, 0x10, "Hard"			},
	{0x13, 0x01, 0x30, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Candle"		},
	{0x13, 0x01, 0xc0, 0x80, "40"			},
	{0x13, 0x01, 0xc0, 0xc0, "50"			},
	{0x13, 0x01, 0xc0, 0x40, "60"			},
	{0x13, 0x01, 0xc0, 0x00, "70"			},
};

STDDIPINFO(Genpeitd)

static struct BurnDIPInfo RthunderDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0xd5, NULL			},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x12, 0x01, 0x03, 0x00, "3 Coins / 1 Credit"		},
	{0x12, 0x01, 0x03, 0x01, "2 Coins / 1 Credit"		},
	{0x12, 0x01, 0x03, 0x03, "1 Coin / 1 Credit"		},
	{0x12, 0x01, 0x03, 0x02, "1 Coin / 2 Credits"		},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x12, 0x01, 0x04, 0x04, "Off"			},
	{0x12, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Invulnerability"	},
	{0x12, 0x01, 0x08, 0x08, "Off"			},
	{0x12, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x10, 0x00, "Off"			},
	{0x12, 0x01, 0x10, 0x10, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x12, 0x01, 0x60, 0x00, "3 Coins / 1 Credit"		},
	{0x12, 0x01, 0x60, 0x20, "2 Coins / 1 Credit"		},
	{0x12, 0x01, 0x60, 0x60, "1 Coin / 1 Credit"		},
	{0x12, 0x01, 0x60, 0x40, "1 Coin / 2 Credits"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x12, 0x01, 0x80, 0x80, "Off"			},
	{0x12, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Continues"		},
	{0x13, 0x01, 0x01, 0x00, "3"			},
	{0x13, 0x01, 0x01, 0x01, "6"			},

	{0   , 0xfe, 0   ,    4, "Cabinet"		},
	{0x13, 0x01, 0x06, 0x06, "Upright 1 Player"	},
	{0x13, 0x01, 0x06, 0x02, "Upright 1 Player"	},
	{0x13, 0x01, 0x06, 0x04, "Upright 2 Players"	},
	{0x13, 0x01, 0x06, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Level Select (Cheat)"	},
	{0x13, 0x01, 0x08, 0x08, "Off"			},
	{0x13, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x13, 0x01, 0x10, 0x10, "Normal"		},
	{0x13, 0x01, 0x10, 0x00, "Easy"			},

	{0   , 0xfe, 0   ,    2, "Timer value"		},
	{0x13, 0x01, 0x20, 0x20, "120 secs"		},
	{0x13, 0x01, 0x20, 0x00, "150 secs"		},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x13, 0x01, 0x40, 0x40, "70k, 200k"		},
	{0x13, 0x01, 0x40, 0x00, "100k, 300k"		},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x13, 0x01, 0x80, 0x80, "3"			},
	{0x13, 0x01, 0x80, 0x00, "5"			},
};

STDDIPINFO(Rthunder)

static struct BurnDIPInfo Rthunder1DIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0xfc, NULL			},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x12, 0x01, 0x03, 0x00, "3 Coins / 1 Credit"		},
	{0x12, 0x01, 0x03, 0x01, "2 Coins / 1 Credit"		},
	{0x12, 0x01, 0x03, 0x03, "1 Coin / 1 Credit"		},
	{0x12, 0x01, 0x03, 0x02, "1 Coin / 2 Credits"		},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x12, 0x01, 0x04, 0x04, "Off"			},
	{0x12, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Invulnerability"	},
	{0x12, 0x01, 0x08, 0x08, "Off"			},
	{0x12, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x10, 0x00, "Off"			},
	{0x12, 0x01, 0x10, 0x10, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x12, 0x01, 0x60, 0x00, "3 Coins / 1 Credit"		},
	{0x12, 0x01, 0x60, 0x20, "2 Coins / 1 Credit"		},
	{0x12, 0x01, 0x60, 0x60, "1 Coin / 1 Credit"		},
	{0x12, 0x01, 0x60, 0x40, "1 Coin / 2 Credits"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x12, 0x01, 0x80, 0x80, "Off"			},
	{0x12, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x01, 0x00, "Off"			},
	{0x13, 0x01, 0x01, 0x01, "On"			},

	{0   , 0xfe, 0   ,    4, "Cabinet"		},
	{0x13, 0x01, 0x06, 0x00, "Upright 1 Player"	},
	{0x13, 0x01, 0x06, 0x04, "Upright 2 Players"	},
	{0x13, 0x01, 0x06, 0x06, "Cocktail"		},
	{0x13, 0x01, 0x06, 0x02, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Level Select (Cheat)"	},
	{0x13, 0x01, 0x08, 0x08, "Off"			},
	{0x13, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0xc0, 0x80, "1"			},
	{0x13, 0x01, 0xc0, 0x40, "2"			},
	{0x13, 0x01, 0xc0, 0xc0, "3"			},
	{0x13, 0x01, 0xc0, 0x00, "5"			},
};

STDDIPINFO(Rthunder1)

static struct BurnDIPInfo WndrmomoDIPList[]=
{
	{0x12, 0xff, 0xff, 0xfd, NULL			},
	{0x13, 0xff, 0xff, 0xfd, NULL			},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x12, 0x01, 0x03, 0x00, "3 Coins / 1 Credit"		},
	{0x12, 0x01, 0x03, 0x01, "2 Coins / 1 Credit"		},
	{0x12, 0x01, 0x03, 0x03, "1 Coin / 1 Credit"		},
	{0x12, 0x01, 0x03, 0x02, "1 Coin / 2 Credits"		},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x12, 0x01, 0x04, 0x04, "Off"			},
	{0x12, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Level Select"		},
	{0x12, 0x01, 0x08, 0x08, "Off"			},
	{0x12, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x10, 0x00, "Off"			},
	{0x12, 0x01, 0x10, 0x10, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x12, 0x01, 0x60, 0x00, "3 Coins / 1 Credit"		},
	{0x12, 0x01, 0x60, 0x20, "2 Coins / 1 Credit"		},
	{0x12, 0x01, 0x60, 0x60, "1 Coin / 1 Credit"		},
	{0x12, 0x01, 0x60, 0x40, "1 Coin / 2 Credits"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x12, 0x01, 0x80, 0x80, "Off"			},
	{0x12, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x01, 0x01, "Off"			},
	{0x13, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Cabinet"		},
	{0x13, 0x01, 0x06, 0x02, "Upright 1 Player"	},
	{0x13, 0x01, 0x06, 0x04, "Upright 2 Players"	},
	{0x13, 0x01, 0x06, 0x06, "Cocktail"		},
	{0x13, 0x01, 0x06, 0x00, "Cocktail"		},
};

STDDIPINFO(Wndrmomo)

static struct BurnDIPInfo RoishtarDIPList[]=
{
	{0x11, 0xff, 0xff, 0xff, NULL			},
	{0x12, 0xff, 0xff, 0xbf, NULL			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x11, 0x01, 0x07, 0x00, "3 Coins / 1 Credit"		},
	{0x11, 0x01, 0x07, 0x02, "2 Coins / 1 Credit"		},
	{0x11, 0x01, 0x07, 0x07, "1 Coin / 1 Credit"		},
	{0x11, 0x01, 0x07, 0x01, "2 Coins / 3 Credits"		},
	{0x11, 0x01, 0x07, 0x06, "1 Coin / 2 Credits"		},
	{0x11, 0x01, 0x07, 0x05, "1 Coin / 3 Credits"		},
	{0x11, 0x01, 0x07, 0x04, "1 Coin / 5 Credits"		},
	{0x11, 0x01, 0x07, 0x03, "1 Coin / 6 Credits"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x11, 0x01, 0x40, 0x00, "Off"			},
	{0x11, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x11, 0x01, 0x80, 0x80, "Off"			},
	{0x11, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x12, 0x01, 0x07, 0x00, "3 Coins / 1 Credit"		},
	{0x12, 0x01, 0x07, 0x02, "2 Coins / 1 Credit"		},
	{0x12, 0x01, 0x07, 0x07, "1 Coin / 1 Credit"		},
	{0x12, 0x01, 0x07, 0x01, "2 Coins / 3 Credits"		},
	{0x12, 0x01, 0x07, 0x06, "1 Coin / 2 Credits"		},
	{0x12, 0x01, 0x07, 0x05, "1 Coin / 3 Credits"		},
	{0x12, 0x01, 0x07, 0x04, "1 Coin / 5 Credits"		},
	{0x12, 0x01, 0x07, 0x03, "1 Coin / 6 Credits"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x12, 0x01, 0x40, 0x00, "Off"			},
	{0x12, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x12, 0x01, 0x80, 0x80, "Off"			},
	{0x12, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Roishtar)

static INT32 tile_xoffset[4] = { 4, 2, 5, 3 };

static void set_tile_offsets(INT32 a, INT32 b, INT32 c, INT32 d)
{
	tile_xoffset[0] = a; tile_xoffset[1] = b;
	tile_xoffset[2] = c; tile_xoffset[3] = d;
}

static void namco_63701x_write(INT32 offset, UINT8 data)
{
	offset &= 3;

	int ch = offset / 2;

	if (offset & 1)
		m_voices[ch].select = data;
	else
	{
		if (m_voices[ch].select & 0x1f)
		{
			m_voices[ch].playing = 1;
			m_voices[ch].base_addr = 0x10000 * ((m_voices[ch].select & 0xe0) >> 5);
			INT32 rom_offs = m_voices[ch].base_addr + 2 * ((m_voices[ch].select & 0x1f) - 1);
			m_voices[ch].position = (DrvSndROM[rom_offs] << 8) + DrvSndROM[rom_offs+1];
			m_voices[ch].volume = data >> 6;
			m_voices[ch].silence_counter = 0;
		}
	}
}

static void namco_63701x_update(INT16 *outputs, INT32  samples)
{
	static const INT32  vol_table[4] = { 26, 84, 200, 258 };

	INT16 t_samples[100];

	memset (t_samples, 0, sizeof(t_samples));

	for (INT32 ch = 0; ch < 2; ch++)
	{
		INT16 *buf = t_samples;
		voice_63701x *v = &m_voices[ch];

		if (v->playing)
		{
			UINT8 *base = DrvSndROM + v->base_addr;
			INT32  pos = v->position;
			INT32  vol = vol_table[v->volume];

			for (INT32 p = 0; p < 100; p++)
			{
				if (v->silence_counter)
				{
					v->silence_counter--;
					buf++;
				}
				else
				{
					INT32  data = base[(pos++) & 0xffff];

					if (data == 0xff)   // end of sample
					{
						v->playing = 0;
						break;
					}
					else if (data == 0x00)  // silence compression
					{
						data = base[(pos++) & 0xffff];
						v->silence_counter = data;
						buf++;
					}
					else
					{
						*(buf++) += (vol * (data - 0x80));
					}
				}
			}

			v->position = pos;
		}
	}

	INT16 *buffer = outputs;

	for (INT32 i = 0; i < samples; i++) {
		INT32 offset = (i * 100) / nBurnSoundLen;
		if (offset > 99) offset = 99; // safe
		buffer[0] += t_samples[offset];
		buffer[1] += t_samples[offset];
		buffer += 2;
	}

}

static UINT8 namcos86_cpu0_read(UINT16 address)
{
	if ((address & 0xfc00) == 0x4000) {
		return namcos1_custom30_read(address & 0x3ff);
	}

	return 0;
}

static void bankswitch1(INT32 data)
{
	if (!has_pcm) data &= 0x03;

	nBankData[0] = data & 0x1f;

	M6809MapMemory(DrvMainROM + 0x10000 + nBankData[0] * 0x2000, 0x6000, 0x7fff, MAP_ROM);
}

static void namcos86_cpu0_write(UINT16 address, UINT8 data)
{
	if ((address & 0xe000) == 0x4000) {
		if (address < 0x4400) {
			namcos1_custom30_write(address & 0x3ff, data);
		} else {
			DrvSprRAM[address & 0x1fff] = data;
		}

		if (address == 0x5ff2) {
			buffer_sprites = 1;
		}

		return;
	}

	if ((address & 0xf800) == 0x8800) {
		tilebank = (address >> 10) & 1;
		return;
	}

	if ((address & 0xe000) == 0x6000 && has_pcm)
	{
		switch ((address & 0x1e00) >> 9)
		{
			case 0x00:
			case 0x01:
			case 0x02:
			case 0x03:
				namco_63701x_write((address & 0x1e00) >> 9, data);
			return;

			case 0x04:
				bankswitch1(data);
			return;
		}
	}

	switch (address)
	{
		case 0x8000:
			watchdog1 |= 1;
			if (watchdog1 == 3) {
				watchdog1 = 0;
				watchdog = 0;
			}
		return;

		case 0x8400:
			M6809SetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;

		case 0x9000:
		case 0x9001:
		case 0x9002:
			scroll[0][address & 3] = data;
		return;

		case 0x9003:
			if (has_pcm == 0) bankswitch1(data);
		return;

		case 0x9004:
		case 0x9005:
		case 0x9006:
			scroll[1][address & 3] = data;
		return;

		case 0x9400:
		case 0x9401:
		case 0x9402:
			scroll[2][address & 3] = data;
		return;

		case 0x9404:
		case 0x9405:
		case 0x9406:
			scroll[3][address & 3] = data;
		return;

		case 0xa000:
			backcolor = data;
		return;
	}
}

static void hopmappy_cpu1_write(UINT16 address, UINT8 /*data*/)
{
	switch (address)
	{
		case 0x9000:
			watchdog1 |= 2;
			if (watchdog1 == 3) {
				watchdog1 = 0;
				watchdog = 0;
			}
		return;

		case 0x9400:
			M6809SetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;
	}
}

static void roishtar_cpu1_write(UINT16 address, UINT8 data)
{
	if ((address & 0xe000) == 0x0000) {
		DrvSprRAM[address & 0x1fff] = data;

		if (address == 0x1ff2) {
			buffer_sprites = 1;
		}

		return;
	}

	switch (address)
	{
		case 0xa000:
			watchdog1 |= 2;
			if (watchdog1 == 3) {
				watchdog1 = 0;
				watchdog = 0;
			}
		return;

		case 0xb000:
			M6809SetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;
	}
}

static void bankswitch2(INT32 data)
{
	nBankData[1] = data & 0x03;
	M6809MapMemory(DrvSubROM + nBankData[1] * 0x2000, 0x6000, 0x7fff, MAP_ROM);
}

static void rthunder_cpu1_write(UINT16 address, UINT8 data)
{
	if ((address & 0xe000) == 0x0000) {
		DrvSprRAM[address & 0x1fff] = data;

		if (address == 0x1ff2) {
			buffer_sprites = 1;
		}

		return;
	}

	switch (address)
	{
		case 0x8000:
			watchdog1 |= 2;
			if (watchdog1 == 3) {
				watchdog1 = 0;
				watchdog = 0;
			}
		return;

		case 0x8800:
			M6809SetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;

		case 0xd803:
			bankswitch2(data);
		return;
	}
}

static void genpeitd_cpu1_write(UINT16 address, UINT8 data)
{
	if ((address & 0xe000) == 0x4000) {
		DrvSprRAM[address & 0x1fff] = data;

		if (address == 0x5ff2) {
			buffer_sprites = 1;
		}

		return;
	}

	switch (address)
	{
		case 0xb000:
			watchdog1 |= 2;
			if (watchdog1 == 3) {
				watchdog1 = 0;
				watchdog = 0;
			}
		return;

		case 0x8800:
			M6809SetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;
	}
}

static void wndrmomo_cpu1_write(UINT16 address, UINT8 data)
{
	if ((address & 0xe000) == 0x2000) {
		DrvSprRAM[address & 0x1fff] = data;

		if (address == 0x3ff2) {
			buffer_sprites = 1;
		}

		return;
	}

	switch (address)
	{
		case 0xc000:
			watchdog1 |= 2;
			if (watchdog1 == 3) {
				watchdog1 = 0;
				watchdog = 0;
			}
		return;

		case 0xc800:
			M6809SetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;
	}
}

static inline UINT8 dip0_read()
{
	int rhi, rlo;

	rhi  = ( DrvDips[0] & 0x01 ) << 4;
	rhi |= ( DrvDips[0] & 0x04 ) << 3;
	rhi |= ( DrvDips[0] & 0x10 ) << 2;
	rhi |= ( DrvDips[0] & 0x40 ) << 1;

	rlo  = ( DrvDips[1] & 0x01 ) >> 0;
	rlo |= ( DrvDips[1] & 0x04 ) >> 1;
	rlo |= ( DrvDips[1] & 0x10 ) >> 2;
	rlo |= ( DrvDips[1] & 0x40 ) >> 3;

	return rhi | rlo;
}

static inline UINT8 dip1_read()
{
	int rhi, rlo;

	rhi  = ( DrvDips[0] & 0x02 ) << 3;
	rhi |= ( DrvDips[0] & 0x08 ) << 2;
	rhi |= ( DrvDips[0] & 0x20 ) << 1;
	rhi |= ( DrvDips[0] & 0x80 ) << 0;

	rlo  = ( DrvDips[1] & 0x02 ) >> 1;
	rlo |= ( DrvDips[1] & 0x08 ) >> 2;
	rlo |= ( DrvDips[1] & 0x20 ) >> 3;
	rlo |= ( DrvDips[1] & 0x80 ) >> 4;

	return rhi | rlo;
}

static void namcos86_mcu_write(UINT16 address, UINT8 data)
{
	if ((address & 0xffe0) == 0x0000) {
		m6803_internal_registers_w(address & 0x001f, data);
		return;
	}

	if ((address & 0xff80) == 0x0080) {
		DrvMCURAM[0x2000 + (address & 0x7f)] = data;
		return;
	}

	if ((address & 0xfc00) == 0x1000) {
		namcos1_custom30_write(address & 0x3ff, data);
		return;
	}

	switch (address)
	{
		case 0x2000:
		case 0x2800:
		case 0x3800:
		case 0x6000:
			BurnYM2151SelectRegister(data);
		return;

		case 0x2001:
		case 0x2801:
		case 0x3801:
		case 0x6001:
			BurnYM2151WriteRegister(data);
		return;

		case 0x8000:
		case 0xa000:
		case 0xb000:
		case 0xc000:
		case 0x8800:
		case 0x9800:
		case 0xa800:
		case 0xb800:
		case 0xc800:
		return;
	}
}

static UINT8 namcos86_mcu_read(UINT16 address)
{
	if ((address & 0xffe0) == 0x0000) {
		return m6803_internal_registers_r(address & 0x001f);
	}

	if ((address & 0xff80) == 0x0080) {
		return DrvMCURAM[0x2000 + (address & 0x7f)];
	}

	if ((address & 0xfc00) == 0x1000) {
		return namcos1_custom30_read(address & 0x3ff);
	}

	switch (address)
	{
		case 0x2000:
		case 0x2800:
		case 0x3800:
		case 0x6000:
		case 0x2001:
		case 0x2801:
		case 0x3801:
		case 0x6001:
			return BurnYM2151ReadStatus();

		case 0x2020:
		case 0x2820:
		case 0x3820:
		case 0x6020:
			return DrvInputs[0];

		case 0x2021:
		case 0x2821:
		case 0x3821:
		case 0x6021:
			return DrvInputs[1];

		case 0x2030:
		case 0x2830:
		case 0x3830:
		case 0x6030:
			return dip0_read();

		case 0x2031:
		case 0x2831:
		case 0x3831:
		case 0x6031:
			return dip1_read();
	}

	return 0;
}

static void namcos86_mcu_write_port(UINT16 port, UINT8 /*data*/)
{
	switch (port & 0x1ff)
	{
		case HD63701_PORT1:
			// coin
		return;

		case HD63701_PORT2:
			// led
		return;
	}
}

static UINT8 namcos86_mcu_read_port(UINT16 port)
{
	switch (port & 0x1ff)
	{
		case HD63701_PORT1:
			return DrvInputs[2];

		case HD63701_PORT2:
			return 0xff;
	}

	return 0;
}

static tilemap_callback( layer0 )
{
	INT32 attr = DrvVidRAM0[offs * 2 + 1];
	INT32 bank = ((DrvColPROM[0x1400 + ((attr & 0x03) << 2)] & 0x0e) >> 1) * 0x100 + tilebank * 0x800;
	INT32 code = DrvVidRAM0[offs * 2 + 0] + bank;

	TILE_SET_INFO(0, code, attr, 0);
}

static tilemap_callback( layer1 )
{
	INT32 attr = DrvVidRAM0[0x1000 + offs * 2 + 1];
	INT32 bank = ((DrvColPROM[0x1410 + ((attr & 0x03) << 2)] & 0x0e) >> 1) * 0x100 + tilebank * 0x800;
	INT32 code = DrvVidRAM0[0x1000 + offs * 2 + 0] + bank;

	TILE_SET_INFO(0, code, attr, 0);
}

static tilemap_callback( layer2 )
{
	INT32 attr = DrvVidRAM1[offs * 2 + 1];
	INT32 bank = ((DrvColPROM[0x1400 + ((attr & 0x03) << 0)] & 0xe0) >> 5) * 0x100;
	INT32 code = DrvVidRAM1[offs * 2 + 0] + bank;

	TILE_SET_INFO(1, code, attr, 0);
}

static tilemap_callback( layer3 )
{
	INT32 attr = DrvVidRAM1[0x1000 + offs * 2 + 1];
	INT32 bank = ((DrvColPROM[0x1410 + ((attr & 0x03) << 0)] & 0xe0) >> 5) * 0x100;
	INT32 code = DrvVidRAM1[0x1000 + offs * 2 + 0] + bank;

	TILE_SET_INFO(1, code, attr, 0);
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	M6809Open(0);
	M6809Reset();
	M6809Close();

	M6809Open(1);
	M6809Reset();
	M6809Close();

//	HD63701Open(0);
	HD63701Reset();
//	HD63701Close();

	BurnYM2151Reset();

	buffer_sprites = 0;
	watchdog = 0;
	watchdog1 = 0;
	backcolor = 0;
	tilebank = 0;
	memset (scroll, 0, sizeof(scroll));
	memset (nBankData, 0, sizeof(nBankData));

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvMainROM		= Next; Next += 0x050000;
	DrvSubROM		= Next; Next += 0x010000;
	DrvMCUROM		= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += 0x040000;
	DrvGfxROM1		= Next; Next += 0x020000;
	DrvGfxROM2		= Next; Next += 0x200000;

	DrvSndROM		= Next; Next += 0x080000;

	DrvColPROM		= Next; Next += 0x001420;

	DrvPalette		= (UINT32*)Next; Next += 0x1000 * sizeof(UINT32);

	AllRam			= Next;

	DrvSprRAM		= Next; Next += 0x002000;
	DrvVidRAM0		= Next; Next += 0x002000;
	DrvVidRAM1		= Next; Next += 0x002000;
	DrvMCURAM		= Next; Next += 0x002080;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void DrvGfxShuffle(UINT8 *gfx, INT32 romsize)
{
	INT32 size = romsize * 2 / 3;

	UINT8 *dest1 = gfx;
	UINT8 *dest2 = gfx + (size / 2);
	UINT8 *mono = gfx + size;
	UINT8 *buf = (UINT8*)BurnMalloc(romsize);

	memcpy (buf, gfx, size);

	for (INT32  i = 0; i < size; i += 2)
	{
		UINT8 data1 = buf[i];
		UINT8 data2 = buf[i+1];
		*dest1++ = (data1 << 4) | (data2 & 0xf);
		*dest2++ = (data1 & 0xf0) | (data2 >> 4);

		*mono ^= 0xff; mono++;
	}

	BurnFree(buf);
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[3]  = { (gfxlen[0] / 3) * 8 * 2, (gfxlen[0] / 3) * 8 * 1, (gfxlen[0] / 3) * 8 * 0 };
	INT32 Plane1[3]  = { (gfxlen[1] / 3) * 8 * 2, (gfxlen[1] / 3) * 8 * 1, (gfxlen[1] / 3) * 8 * 0 };
	INT32 XOffs0[8]  = { STEP8(0,1) };
	INT32 YOffs0[8]  = { STEP8(0,8) };

	INT32 Plane2[4]  = { STEP4(0,1) };
	INT32 XOffs1[32] = { STEP16(0,4), STEP16(1024, 4) };
	INT32 YOffs1[32] = { STEP16(0,64), STEP16(2048, 64) };

	UINT8 *tmp = (UINT8*)BurnMalloc(gfxlen[2]);
	if (tmp == NULL) {
		return 1;
	}

	DrvGfxShuffle(DrvGfxROM0, gfxlen[0]);
	DrvGfxShuffle(DrvGfxROM1, gfxlen[1]);

	memcpy (tmp, DrvGfxROM0, gfxlen[0]);

	GfxDecode(((gfxlen[0] * 8) / 3) / 0x040, 3,  8,  8, Plane0, XOffs0, YOffs0, 0x0040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, gfxlen[1]);

	GfxDecode(((gfxlen[1] * 8) / 3) / 0x040, 3,  8,  8, Plane1, XOffs0, YOffs0, 0x0040, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, gfxlen[2]);

	GfxDecode(((gfxlen[2] * 8) / 4) / 0x400, 4, 32, 32, Plane2, XOffs1, YOffs1, 0x1000, tmp, DrvGfxROM2);

	BurnFree(tmp);

	return 0;
}

static void DrvPaletteInit()
{
	UINT32 pens[512];

	for (INT32 i = 0; i < 512; i++)
	{
		INT32 bit0 = (DrvColPROM[i+0] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i+0] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i+0] >> 2) & 0x01;
		INT32 bit3 = (DrvColPROM[i+0] >> 3) & 0x01;
		INT32 r = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[i+0] >> 4) & 0x01;
		bit1 = (DrvColPROM[i+0] >> 5) & 0x01;
		bit2 = (DrvColPROM[i+0] >> 6) & 0x01;
		bit3 = (DrvColPROM[i+0] >> 7) & 0x01;
		INT32 g = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[i+512] >> 0) & 0x01;
		bit1 = (DrvColPROM[i+512] >> 1) & 0x01;
		bit2 = (DrvColPROM[i+512] >> 2) & 0x01;
		bit3 = (DrvColPROM[i+512] >> 3) & 0x01;
		INT32 b = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		pens[i] = BurnHighCol(r,g,b,0);
	}

	for (INT32 i = 0; i < 2048; i++) {
		DrvPalette[0x000 + i] = pens[DrvColPROM[0x400 + i] | 0x000];
		DrvPalette[0x800 + i] = pens[DrvColPROM[0xc00 + i] | 0x100];
	}
}

static INT32 DrvROMload()
{
	char* pRomName;
	struct BurnRomInfo ri;
	INT32 pSize = 0;
	INT32 genpeitd = 0;

	UINT8 *pLoad[4] = { DrvMainROM + 0x8000, DrvSubROM, DrvMCUROM + 0x8000, DrvMainROM + 0x10000 };
	UINT8 *gLoad[4] = { DrvGfxROM0, DrvGfxROM1, DrvGfxROM2, DrvColPROM };
	UINT8 *sLoad = DrvSndROM;

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {
		BurnDrvGetRomInfo(&ri, i);
		INT32 nType = ri.nType & 0xf;

		if (nType == 1) {
			if (BurnLoadRom(pLoad[0], i, 1)) return 1;
			pLoad[0] += ri.nLen;
			continue;
		}

		if (nType == 2) {
			if (ri.nLen == 0x4000) pLoad[1] = DrvSubROM + 0xc000;
			if (BurnLoadRom(pLoad[1], i, 1)) return 1;
			if ((pLoad[1] - DrvSubROM) == 0) memcpy (DrvSubROM + 0x8000, DrvSubROM, 0x8000); // rthunder
			pLoad[1] += ri.nLen;
			continue;
		}

		if (nType == 7) {
			if (ri.nLen == 0x1000) pLoad[2] = DrvMCUROM + 0xf000; // internal rom
			if (ri.nLen == 0x8000) pLoad[2] = DrvMCUROM + 0x4000;
			if (BurnLoadRom(pLoad[2], i, 1)) return 1;
			pLoad[2] += ri.nLen;
			continue;
		}

		if (nType == 8) {
			if (ri.nLen == 0x2000) pLoad[3] += 0x4000; // roishtar
			if (BurnLoadRom(pLoad[3], i, 1)) return 1;
			pLoad[3] += ri.nLen;
			continue;
		}

		if (nType > 2 && nType <= 6) {
			if (BurnLoadRom(gLoad[nType - 3], i, 1)) return 1;

			if (nType == 5 && ((pSize == 0x20000 && ri.nLen == 0x10000) || genpeitd)) {
				gLoad[nType - 3] += 0x20000; // genpeitd
				genpeitd = 1;
			} else {
				gLoad[nType - 3] += ri.nLen;
			}
		
			pSize = ri.nLen;

			continue;
		}
		
		if (nType == 9) {
			if (BurnLoadRom(sLoad, i, 1)) return 1;
			sLoad += 0x20000;

			continue;
		}
	}

	memcpy (DrvMCUROM, DrvMCUROM + 0x4000, 0x4000); // mirror

	gfxlen[0] = gLoad[0] - DrvGfxROM0;
	gfxlen[1] = gLoad[1] - DrvGfxROM1;
	gfxlen[2] = gLoad[2] - DrvGfxROM2;

	if (gfxlen[0] < 0x06000) gfxlen[0] = 0x06000;
	if (gfxlen[1] < 0x06000) gfxlen[1] = 0x06000;
	if (gfxlen[2] < 0x40000) gfxlen[2] = 0x40000;

	return 0;
}

static INT32 CommonInit(INT32 nSubCPUConfig, INT32 pcmdata)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (DrvROMload()) return 1;

		DrvGfxDecode();
		DrvPaletteInit();
	}

	M6809Init(2);
	M6809Open(0);
	M6809MapMemory(DrvVidRAM0, 		0x0000, 0x1fff, MAP_RAM);
	M6809MapMemory(DrvVidRAM1, 		0x2000, 0x3fff, MAP_RAM);
//	M6809MapMemory(DrvSprRAM + 0x0000,	0x4000, 0x43ff, MAP_RAM);
	M6809MapMemory(DrvSprRAM + 0x0400,	0x4400, 0x5fff, MAP_ROM);
//	M6809MapMemory(DrvMainROM + 0x0000,	0x6000, 0x7fff, MAP_ROM); // bank1
	M6809MapMemory(DrvMainROM + 0x8000,	0x8000, 0xffff, MAP_ROM);
	M6809SetReadHandler(namcos86_cpu0_read);
	M6809SetWriteHandler(namcos86_cpu0_write);
	M6809Close();

	HD63701Init(1);
//	HD63701Open(0);
//	HD63701MapMemory(DrvMCURAM + 0x0000,	0x1000, 0x13ff, MAP_RAM);
	HD63701MapMemory(DrvMCURAM + 0x0400,	0x1400, 0x1fff, MAP_RAM);
	HD63701MapMemory(DrvMCUROM + 0x8000,	0x8000, 0xffff, MAP_ROM);
//	HD63701SetReadOpHandler(namcos86_mcu_read);
//	HD63701SetReadOpArgHandler(namcos86_mcu_read);
	HD63701SetReadHandler(namcos86_mcu_read);
	HD63701SetWriteHandler(namcos86_mcu_write);
	HD63701SetWritePortHandler(namcos86_mcu_write_port);
	HD63701SetReadPortHandler(namcos86_mcu_read_port);
//	HD63701Close();

	set_tile_offsets(4, 2, 5, 3); // rthunder

	switch (nSubCPUConfig)
	{
		case 0: // hopmappy / skykid
		{
			M6809Open(1);
			M6809MapMemory(DrvSubROM + 0x0000,	0x0000, 0xffff, MAP_ROM);
			M6809SetWriteHandler(hopmappy_cpu1_write);
			M6809Close();

			if (strstr(BurnDrvGetTextA(DRV_NAME), "skykid"))
				set_tile_offsets(-3, -2, 5, 3); // skykid
		}
		break;

		case 1: // roishtar
		{
			M6809Open(1);
			M6809MapMemory(DrvSprRAM + 0x0000,	0x0000, 0x1eff, MAP_RAM);
			M6809MapMemory(DrvSprRAM + 0x1f00,	0x1f00, 0x1fff, MAP_ROM);
			M6809MapMemory(DrvVidRAM1, 		0x4000, 0x5fff, MAP_RAM);
			M6809MapMemory(DrvVidRAM0, 		0x6000, 0x7fff, MAP_RAM);
			M6809MapMemory(DrvSubROM + 0x8000,	0x8000, 0xffff, MAP_ROM);
			M6809SetWriteHandler(roishtar_cpu1_write);
			M6809Close();

		//	HD6370Open(0);
			HD63701MapMemory(DrvMCUROM + 0x2000,	0x2000, 0x3fff, MAP_ROM);
		//	HD63701Close();
		}
		break;

		case 2: // genpeitd
		{
			M6809Open(1);
			M6809MapMemory(DrvVidRAM0, 		0x0000, 0x1fff, MAP_RAM);
			M6809MapMemory(DrvVidRAM1, 		0x2000, 0x3fff, MAP_RAM);
			M6809MapMemory(DrvSprRAM + 0x0000,	0x4000, 0x5eff, MAP_RAM);
			M6809MapMemory(DrvSprRAM + 0x1f00,	0x5f00, 0x5fff, MAP_ROM);
			M6809MapMemory(DrvSubROM + 0x8000,	0x8000, 0xffff, MAP_ROM);
			M6809SetWriteHandler(genpeitd_cpu1_write);
			M6809Close();

		//	HD6370Open(0);
			HD63701MapMemory(DrvMCUROM + 0x4000,	0x4000, 0x7fff, MAP_ROM);
		//	HD63701Close();
		}
		break;

		case 3: // rthunder
		{
			M6809Open(1);
			M6809MapMemory(DrvSprRAM + 0x0000,	0x0000, 0x1eff, MAP_RAM);
			M6809MapMemory(DrvSprRAM + 0x1f00,	0x1f00, 0x1fff, MAP_ROM);
			M6809MapMemory(DrvVidRAM0, 		0x2000, 0x3fff, MAP_RAM);
			M6809MapMemory(DrvVidRAM1, 		0x4000, 0x5fff, MAP_RAM);
			M6809MapMemory(DrvSubROM + 0x8000,	0x8000, 0xffff, MAP_ROM);
			M6809SetWriteHandler(rthunder_cpu1_write);
			M6809Close();

		//	HD6370Open(0);
			HD63701MapMemory(DrvMCUROM + 0x4000,	0x4000, 0x7fff, MAP_ROM);
		//	HD63701Close();
		}
		break;

		case 4: // wndrmomo
		{
			M6809Open(1);
			M6809MapMemory(DrvSprRAM + 0x0000,	0x2000, 0x3eff, MAP_RAM);
			M6809MapMemory(DrvSprRAM + 0x1f00,	0x3f00, 0x3fff, MAP_ROM);
			M6809MapMemory(DrvVidRAM0, 		0x4000, 0x5fff, MAP_RAM);
			M6809MapMemory(DrvVidRAM1, 		0x6000, 0x7fff, MAP_RAM);
			M6809MapMemory(DrvSubROM + 0x8000,	0x8000, 0xffff, MAP_ROM);
			M6809SetWriteHandler(wndrmomo_cpu1_write);
			M6809Close();
		
		//	HD6370Open(0);
			HD63701MapMemory(DrvMCUROM + 0x4000,	0x4000, 0x7fff, MAP_ROM);
		//	HD63701Close();
		}
		break;
	}

	BurnYM2151Init(3579580);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.60, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.60, BURN_SND_ROUTE_RIGHT);

	NamcoSoundInit(24000, 8, 1);
	NacmoSoundSetAllRoutes(0.50 * 10.0 / 16.0, BURN_SND_ROUTE_BOTH);

	has_pcm = pcmdata;

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, layer0_map_callback, 8, 8, 64, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, layer1_map_callback, 8, 8, 64, 32);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, layer2_map_callback, 8, 8, 64, 32);
	GenericTilemapInit(3, TILEMAP_SCAN_ROWS, layer3_map_callback, 8, 8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 3, 8, 8, (gfxlen[0]*8)/3, 0, 0xff);
	GenericTilemapSetGfx(1, DrvGfxROM1, 3, 8, 8, (gfxlen[1]*8)/3, 0, 0xff);
	GenericTilemapSetOffsets(TMAP_GLOBAL, -16, -25);
	GenericTilemapSetTransparent(0, 0x07);
	GenericTilemapSetTransparent(1, 0x07);
	GenericTilemapSetTransparent(2, 0x07);
	GenericTilemapSetTransparent(3, 0x07);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6809Exit();
	HD63701Exit();

	NamcoSoundExit();
	NamcoSoundProm = NULL;

	BurnYM2151Exit();

	BurnFree(AllMem);

	has_pcm = 0;
	enable_bankswitch2 = 0;

	return 0;
}

static void draw_sprites()
{
	UINT8 *m_spriteram = DrvSprRAM + 0x1800;
	const UINT8 *source = &m_spriteram[0x800-0x20];
	const UINT8 *finish = &m_spriteram[0];

	INT32 sprite_xoffs = m_spriteram[0x07f5] + ((m_spriteram[0x07f4] & 1) << 8);
	INT32 sprite_yoffs = m_spriteram[0x07f7];

	INT32 gfxmask = ((gfxlen[2] * 8) / 4) / (32 * 32);
	INT32 bank_sprites = gfxmask / 8;

	while (source >= finish)
	{
		static const INT32 sprite_size[4] = { 16, 8, 32, 4 };
		INT32 attr1 = source[10];
		INT32 attr2 = source[14];
		INT32 color = source[12];
		INT32 flipx = (attr1 & 0x20) >> 5;
		INT32 flipy = (attr2 & 0x01);
		INT32 sizex = sprite_size[(attr1 & 0xc0) >> 6];
		INT32 sizey = sprite_size[(attr2 & 0x06) >> 1];
		INT32 tx = (attr1 & 0x18) & (~(sizex-1));
		INT32 ty = (attr2 & 0x18) & (~(sizey-1));
		INT32 sx = source[13] + ((color & 0x01) << 8);
		INT32 sy = -source[15] - sizey;
		INT32 sprite = source[11];
		INT32 sprite_bank = attr1 & 7;
		INT32 priority = (source[14] & 0xe0) >> 5;
		UINT32 pri_mask = (0xff << (priority + 1)) & 0xff;

		pri_mask |= 1 << 31;

		sprite &= bank_sprites-1;
		sprite += sprite_bank * bank_sprites;
		sprite &= gfxmask - 1;

		color = color >> 1;

		sx += sprite_xoffs;
		sy -= sprite_yoffs;

		if (flipscreen)
		{
			sx = -sx - sizex;
			sy = -sy - sizey;
			flipx ^= 1;
			flipy ^= 1;
		}

		{
			sx &= 0x1ff;
			sy = ((sy + 16) & 0xff) - 16;

			sy -= 15;
			sx -= 67;

			UINT8 *gfxbase = DrvGfxROM2 + (sprite * 0x400);

			color = (color * 16) + 0x800;

			for (INT32 y = 0; y < sizey; y++, sy++) {
				if (sy < 0 || sy >= nScreenHeight) continue;

				for (INT32 x = 0; x < sizex; x++, sx++) {
					if (sx < 0 || sx >= nScreenWidth) continue;

					INT32 xx = x;
					INT32 yy = y;

					if (flipx) xx = (sizex - 1) - x;
					if (flipy) yy = (sizey - 1) - y;

					INT32 pxl = gfxbase[xx + tx + ((yy + ty) * 32)];

					if (pxl != 0x0f) {
						if ((pri_mask & (1 << (pPrioDraw[(sy * nScreenWidth) + sx] & 0x1f))) == 0) {
							pTransDraw[(sy * nScreenWidth) + sx] = pxl + color;
							pPrioDraw[(sy * nScreenWidth) + sx] = 0x1f; // or enemies in wonder momo show up through the audience...
						}
					}
				}

				sx -= sizex;
			}
		}

		source -= 0x10;
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	BurnTransferClear();

	{
		INT32 bgcolor = (backcolor << 3) | 7;

		for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
			pTransDraw[i] = bgcolor;
		}
	}

	flipscreen = DrvSprRAM[0x1ff6] & 1;

	GenericTilemapSetFlip(TMAP_GLOBAL, (flipscreen) ? (TMAP_FLIPX | TMAP_FLIPY) : 0);

	for (INT32 i = 0; i < 4; i++)
	{
		if (flipscreen)
		{
			GenericTilemapSetScrollX(i, -((scroll[i][0] * 256) + scroll[i][1] - 192 - tile_xoffset[i]));
			GenericTilemapSetScrollY(i, -scroll[i][2] - 1 - 16);
		}
		else
		{
			GenericTilemapSetScrollX(i, (scroll[i][0] * 256) + scroll[i][1] + tile_xoffset[i]);
			GenericTilemapSetScrollY(i, scroll[i][2]);
		}
	}

	for (INT32 layer = 0; layer < 8; layer++)
	{
		for (INT32 i = 3; i >= 0; i--)
		{
			if (((scroll[i][0] & 0x0e) >> 1) == layer) {
				if (nBurnLayer & (1 << i)) GenericTilemapDraw(i, pTransDraw, layer);
			}
		}
	}

	draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset(1);
	}

	watchdog++;
	if (watchdog >= 180) {
	//	bprintf (0, _T("Watchdog triggered!\n"));
		DrvDoReset(0);
	}

	M6809NewFrame();
	HD63701NewFrame();

	{
		memset (DrvInputs, 0xff, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nSegment;
	INT32 nInterleave = 800;
	INT32 nSoundBufferPos = 0;
	INT32 nCyclesTotal[3] = { 1536000 / 60, 1536000 / 60, 1536000 / 60 };
	INT32 nCyclesDone[3] = { 0, 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		M6809Open(0);
		nCyclesDone[0] += M6809Run(nCyclesTotal[0] / nInterleave);
		nSegment = M6809TotalCycles();
		if (i == 725) M6809SetIRQLine(0, CPU_IRQSTATUS_ACK);
		M6809Close();

		M6809Open(1);
		nCyclesDone[1] += M6809Run(nSegment - M6809TotalCycles());
		if (i == 725) M6809SetIRQLine(0, CPU_IRQSTATUS_ACK);
		M6809Close();

		nCyclesDone[2] += HD63701Run(nSegment - M6800TotalCycles());
		if (i == 725) HD63701SetIRQLine(0, CPU_IRQSTATUS_AUTO);

		if ((i % 8) == 7) {
			if (pBurnSoundOut) {
				nSegment = nBurnSoundLen / (nInterleave / 8);
				BurnYM2151Render(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
				nSoundBufferPos += nSegment;
			}
		}
	}

	if (pBurnSoundOut) {
		nSegment = nBurnSoundLen - nSoundBufferPos;
		if (nSegment > 0) {
			BurnYM2151Render(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
		}
		NamcoSoundUpdate(pBurnSoundOut, nBurnSoundLen);

		namco_63701x_update(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		DrvDraw();
	}

	if (buffer_sprites)
	{
		for (INT32 i = 0x1800 ; i < 0x2000; i+= 0x10) {
			for (INT32 j = 0x0A; j < 0x10; j++) {
				DrvSprRAM[i + j] = DrvSprRAM[i + j - 6];
			}
		}

		buffer_sprites = 0;
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

		M6809Scan(nAction);
		HD63701Scan(nAction);

		NamcoSoundScan(nAction, pnMin);
		BurnYM2151Scan(nAction);

		SCAN_VAR(m_voices);
		SCAN_VAR(buffer_sprites);
		SCAN_VAR(watchdog1);
		SCAN_VAR(backcolor);
		SCAN_VAR(tilebank);
		SCAN_VAR(scroll);
		SCAN_VAR(nBankData);
	}

	if (nAction & ACB_WRITE) {
		M6809Open(0);
		bankswitch1(nBankData[0]);
		M6809Close();

		if (enable_bankswitch2) {
			M6809Open(1);
			bankswitch2(nBankData[1]);
			M6809Close();
		}
	}

	return 0;
}



// Sky Kid Deluxe (set 1)

static struct BurnRomInfo skykiddxRomDesc[] = {
	{ "sk3_1b.9c",		0x8000, 0x767b3514, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code

	{ "sk3_3.12c",		0x8000, 0x6d1084c4, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 #1 Code

	{ "sk3_9.7r",		0x8000, 0x48675b17, 3 | BRF_GRA },           //  2 Layer 0 & 1 Tiles
	{ "sk3_10.7s",		0x4000, 0x7418465a, 3 | BRF_GRA },           //  3

	{ "sk3_7.4r",		0x8000, 0x4036b735, 4 | BRF_GRA },           //  4 Layer 2 & 3 Tiles
	{ "sk3_8.4s",		0x4000, 0x044bfd21, 4 | BRF_GRA },           //  5

	{ "sk3_5.12h",		0x8000, 0x5c7d4399, 5 | BRF_GRA },           //  6 Sprites
	{ "sk3_6.12k",		0x8000, 0xc908a3b2, 5 | BRF_GRA },           //  7

	{ "sk3-1.3r",		0x0200, 0x9e81dedd, 6 | BRF_GRA },           //  8 Color Proms
	{ "sk3-2.3s",		0x0200, 0xcbfec4dd, 6 | BRF_GRA },           //  9
	{ "sk3-3.4v",		0x0800, 0x81714109, 6 | BRF_GRA },           // 10
	{ "sk3-4.5v",		0x0800, 0x1bf25acc, 6 | BRF_GRA },           // 11
	{ "sk3-5.6u",		0x0020, 0xe4130804, 6 | BRF_GRA },           // 12

	{ "sk3_4.6b",		0x4000, 0xe6cae2d6, 7 | BRF_PRG | BRF_ESS }, // 13 HD63701 MCU Code
	{ "cus60-60a1.mcu",	0x1000, 0x076ea82a, 7 | BRF_PRG | BRF_ESS }, // 14

	{ "sk3_2.9d",		0x8000, 0x74b8f8e2, 8 | BRF_PRG | BRF_ESS }, // 15 M6809 #0 Bank Code
};

STD_ROM_PICK(skykiddx)
STD_ROM_FN(skykiddx)

static INT32 SkykiddxInit()
{
	return CommonInit(0, 0);
}

struct BurnDriver BurnDrvSkykiddx = {
	"skykiddx", NULL, NULL, NULL, "1986",
	"Sky Kid Deluxe (set 1)\0", NULL, "Namco", "System 86",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, skykiddxRomInfo, skykiddxRomName, NULL, NULL, CommonInputInfo, SkykiddxDIPInfo,
	SkykiddxInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	288, 224, 4, 3
};


// Sky Kid Deluxe (set 2)

static struct BurnRomInfo skykiddxoRomDesc[] = {
	{ "sk3_1.9c",		0x8000, 0x5722a291, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code

	{ "sk3_3.12c",		0x8000, 0x6d1084c4, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 #1 Code

	{ "sk3_9.7r",		0x8000, 0x48675b17, 3 | BRF_GRA },           //  2 Layer 0 & 1 Tiles
	{ "sk3_10.7s",		0x4000, 0x7418465a, 3 | BRF_GRA },           //  3

	{ "sk3_7.4r",		0x8000, 0x4036b735, 4 | BRF_GRA },           //  4 Layer 2 & 3 Tiles
	{ "sk3_8.4s",		0x4000, 0x044bfd21, 4 | BRF_GRA },           //  5

	{ "sk3_5.12h",		0x8000, 0x5c7d4399, 5 | BRF_GRA },           //  6 Sprites
	{ "sk3_6.12k",		0x8000, 0xc908a3b2, 5 | BRF_GRA },           //  7

	{ "sk3-1.3r",		0x0200, 0x9e81dedd, 6 | BRF_GRA },           //  8 Color Proms
	{ "sk3-2.3s",		0x0200, 0xcbfec4dd, 6 | BRF_GRA },           //  9
	{ "sk3-3.4v",		0x0800, 0x81714109, 6 | BRF_GRA },           // 10
	{ "sk3-4.5v",		0x0800, 0x1bf25acc, 6 | BRF_GRA },           // 11
	{ "sk3-5.6u",		0x0020, 0xe4130804, 6 | BRF_GRA },           // 12

	{ "sk3_4.6b",		0x4000, 0xe6cae2d6, 7 | BRF_PRG | BRF_ESS }, // 13 HD63701 MCU Code
	{ "cus60-60a1.mcu",	0x1000, 0x076ea82a, 7 | BRF_PRG | BRF_ESS }, // 14

	{ "sk3_2.9d",		0x8000, 0x74b8f8e2, 8 | BRF_PRG | BRF_ESS }, // 15 M6809 #0 Bank Code
};

STD_ROM_PICK(skykiddxo)
STD_ROM_FN(skykiddxo)

struct BurnDriver BurnDrvSkykiddxo = {
	"skykiddxo", "skykiddx", NULL, NULL, "1986",
	"Sky Kid Deluxe (set 2)\0", NULL, "Namco", "System 86",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, skykiddxoRomInfo, skykiddxoRomName, NULL, NULL, CommonInputInfo, SkykiddxDIPInfo,
	SkykiddxInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	288, 224, 4, 3
};


// Hopping Mappy

static struct BurnRomInfo hopmappyRomDesc[] = {
	{ "hm1_1.9c",		0x8000, 0x1a83914e, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code

	{ "hm1_2.12c",		0x4000, 0xc46cda65, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 #1 Code

	{ "hm1_6.7r",		0x4000, 0xfd0e8887, 3 | BRF_GRA },           //  2 Layer 0 & 1 Tiles

	{ "hm1_5.4r",		0x4000, 0x9c4f31ae, 4 | BRF_GRA },           //  3 Layer 2 & 3 Tiles

	{ "hm1_4.12h",		0x8000, 0x78719c52, 5 | BRF_GRA },           //  4 Sprites

	{ "hm1-1.3r",		0x0200, 0xcc801088, 6 | BRF_GRA },           //  5 Color Proms
	{ "hm1-2.3s",		0x0200, 0xa1cb71c5, 6 | BRF_GRA },           //  6
	{ "hm1-3.4v",		0x0800, 0xe362d613, 6 | BRF_GRA },           //  7
	{ "hm1-4.5v",		0x0800, 0x678252b4, 6 | BRF_GRA },           //  8
	{ "hm1-5.6u",		0x0020, 0x475bf500, 6 | BRF_GRA },           //  9

	{ "hm1_3.6b",		0x2000, 0x6496e1db, 7 | BRF_PRG | BRF_ESS }, // 10 HD63701 MCU Code
	{ "cus60-60a1.mcu",	0x1000, 0x076ea82a, 7 | BRF_PRG | BRF_ESS }, // 11
};

STD_ROM_PICK(hopmappy)
STD_ROM_FN(hopmappy)

static INT32 HopmappyInit()
{
	return CommonInit(0, 0);
}

struct BurnDriver BurnDrvHopmappy = {
	"hopmappy", NULL, NULL, NULL, "1986",
	"Hopping Mappy\0", NULL, "Namco", "System 86",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PUZZLE, 0,
	NULL, hopmappyRomInfo, hopmappyRomName, NULL, NULL, HopmappyInputInfo, HopmappyDIPInfo,
	HopmappyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	288, 224, 4, 3
};


// The Return of Ishtar

static struct BurnRomInfo roishtarRomDesc[] = {
	{ "ri1_1c.9c",		0x8000, 0x14acbacb, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code

	{ "ri1_3.12c",		0x8000, 0xa39829f7, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 #1 Code

	{ "ri1_14.7r",		0x4000, 0xde8154b4, 3 | BRF_GRA },           //  2 Layer 0 & 1 Tiles
	{ "ri1_15.7s",		0x2000, 0x4298822b, 3 | BRF_GRA },           //  3

	{ "ri1_12.4r",		0x4000, 0x557e54d3, 4 | BRF_GRA },           //  4 Layer 2 & 3 Tiles
	{ "ri1_13.4s",		0x2000, 0x9ebe8e32, 4 | BRF_GRA },           //  5

	{ "ri1_5.12h",		0x8000, 0x46b59239, 5 | BRF_GRA },           //  6 Sprites
	{ "ri1_6.12k",		0x8000, 0x94d9ef48, 5 | BRF_GRA },           //  7
	{ "ri1_7.12l",		0x8000, 0xda802b59, 5 | BRF_GRA },           //  8
	{ "ri1_8.12m",		0x8000, 0x16b88b74, 5 | BRF_GRA },           //  9
	{ "ri1_9.12p",		0x8000, 0xf3de3c2a, 5 | BRF_GRA },           // 10
	{ "ri1_10.12r",		0x8000, 0x6dacc70d, 5 | BRF_GRA },           // 11
	{ "ri1_11.12t",		0x8000, 0xfb6bc533, 5 | BRF_GRA },           // 12

	{ "ri1-1.3r",		0x0200, 0x29cd0400, 6 | BRF_GRA },           // 13 Color Proms
	{ "ri1-2.3s",		0x0200, 0x02fd278d, 6 | BRF_GRA },           // 14
	{ "ri1-3.4v",		0x0800, 0xcbd7e53f, 6 | BRF_GRA },           // 15
	{ "ri1-4.5v",		0x0800, 0x22921617, 6 | BRF_GRA },           // 16
	{ "ri1-5.6u",		0x0020, 0xe2188075, 6 | BRF_GRA },           // 17

	{ "ri1_4.6b",		0x8000, 0x552172b8, 7 | BRF_PRG | BRF_ESS }, // 18 HD63701 MCU Code
	{ "cus60-60a1.mcu",	0x1000, 0x076ea82a, 7 | BRF_PRG | BRF_ESS }, // 19

	{ "ri1_2.9d",		0x2000, 0xfcd58d91, 8 | BRF_PRG | BRF_ESS }, // 20 M6809 #0 Bank Code
};

STD_ROM_PICK(roishtar)
STD_ROM_FN(roishtar)

static INT32 RoishtarInit()
{
	return CommonInit(1, 0);
}

struct BurnDriver BurnDrvRoishtar = {
	"roishtar", NULL, NULL, NULL, "1986",
	"The Return of Ishtar\0", NULL, "Namco", "System 86",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, roishtarRomInfo, roishtarRomName, NULL, NULL, RoishtarInputInfo, RoishtarDIPInfo,
	RoishtarInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	288, 224, 4, 3
};


// Genpei ToumaDen

static struct BurnRomInfo genpeitdRomDesc[] = {
	{ "gt1_1b.9c",		0x08000, 0x75396194, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code

	{ "gt1_2.12c",		0x04000, 0x302f2cb6, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 #1 Code

	{ "gt1_7.7r",		0x10000, 0xea77a211, 3 | BRF_GRA },           //  2 Layer 0 & 1 Tiles
	{ "gt1_6.7s",		0x08000, 0x1b128a2e, 3 | BRF_GRA },           //  3

	{ "gt1_5.4r",		0x08000, 0x44d58b06, 4 | BRF_GRA },           //  4 Layer 2 & 3 Tiles
	{ "gt1_4.4s",		0x04000, 0xdb8d45b0, 4 | BRF_GRA },           //  5

	{ "gt1_11.12h",		0x20000, 0x3181a5fe, 5 | BRF_GRA },           //  6 Sprites
	{ "gt1_12.12k",		0x20000, 0x76b729ab, 5 | BRF_GRA },           //  7
	{ "gt1_13.12l",		0x20000, 0xe332a36e, 5 | BRF_GRA },           //  8
	{ "gt1_14.12m",		0x20000, 0xe5ffaef5, 5 | BRF_GRA },           //  9
	{ "gt1_15.12p",		0x20000, 0x198b6878, 5 | BRF_GRA },           // 10
	{ "gt1_16.12r",		0x20000, 0x801e29c7, 5 | BRF_GRA },           // 11
	{ "gt1_8.12t",		0x10000, 0xad7bc770, 5 | BRF_GRA },           // 12
	{ "gt1_9.12u",		0x10000, 0xd95a5fd7, 5 | BRF_GRA },           // 13

	{ "gt1-1.3r",		0x00200, 0x2f0ddddb, 6 | BRF_GRA },           // 14 Color Proms
	{ "gt1-2.3s",		0x00200, 0x87d27025, 6 | BRF_GRA },           // 15
	{ "gt1-3.4v",		0x00800, 0xc178de99, 6 | BRF_GRA },           // 16
	{ "gt1-4.5v",		0x00800, 0x9f48ef17, 6 | BRF_GRA },           // 17
	{ "gt1-5.6u",		0x00020, 0xe4130804, 6 | BRF_GRA },           // 18

	{ "gt1_3.6b",		0x08000, 0x315cd988, 7 | BRF_PRG | BRF_ESS }, // 19 HD63701 MCU Code
	{ "cus60-60a1.mcu",	0x01000, 0x076ea82a, 7 | BRF_PRG | BRF_ESS }, // 20

	{ "gt1_10b.f1",		0x10000, 0x5721ad0d, 8 | BRF_PRG | BRF_ESS }, // 21 M6809 #0 Bank Code

	{ "gt1_17.f3",		0x20000, 0x26181ff8, 9 | BRF_SND },           // 22 Namco 63701x Samples
	{ "gt1_18.h3",		0x20000, 0x7ef9e5ea, 9 | BRF_SND },           // 23
	{ "gt1_19.k3",		0x20000, 0x38e11f6c, 9 | BRF_SND },           // 24
};

STD_ROM_PICK(genpeitd)
STD_ROM_FN(genpeitd)

static INT32 GenpeitdInit()
{
	return CommonInit(2, 1);
}

struct BurnDriver BurnDrvGenpeitd = {
	"genpeitd", NULL, NULL, NULL, "1986",
	"Genpei ToumaDen\0", NULL, "Namco", "System 86",
	L"Genpei ToumaDen\0\u906E\u735E\u0E8A\u549B\u1D4F\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, genpeitdRomInfo, genpeitdRomName, NULL, NULL, CommonInputInfo, GenpeitdDIPInfo,
	GenpeitdInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	288, 224, 4, 3
};



// Rolling Thunder (rev 3)

static struct BurnRomInfo rthunderRomDesc[] = {
	{ "rt3_1b.9c",		0x08000, 0x7d252a1b, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code

	{ "rt3_3.12d",		0x08000, 0xa13f601c, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 #1 Code
	{ "rt3_2b.12c",		0x08000, 0xa7ea46ee, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "rt1_7.7r",		0x10000, 0xa85efa39, 3 | BRF_GRA },           //  3 Layer 0 & 1 Tiles
	{ "rt1_8.7s",		0x08000, 0xf7a95820, 3 | BRF_GRA },           //  4

	{ "rt1_5.4r",		0x08000, 0xd0fc470b, 4 | BRF_GRA },           //  5 Layer 2 & 3 Tiles
	{ "rt1_6.4s",		0x04000, 0x6b57edb2, 4 | BRF_GRA },           //  6

	{ "rt1_9.12h",		0x10000, 0x8e070561, 5 | BRF_GRA },           //  7 Sprites
	{ "rt1_10.12k",		0x10000, 0xcb8fb607, 5 | BRF_GRA },           //  8
	{ "rt1_11.12l",		0x10000, 0x2bdf5ed9, 5 | BRF_GRA },           //  9
	{ "rt1_12.12m",		0x10000, 0xe6c6c7dc, 5 | BRF_GRA },           // 10
	{ "rt1_13.12p",		0x10000, 0x489686d7, 5 | BRF_GRA },           // 11
	{ "rt1_14.12r",		0x10000, 0x689e56a8, 5 | BRF_GRA },           // 12
	{ "rt1_15.12t",		0x10000, 0x1d8bf2ca, 5 | BRF_GRA },           // 13
	{ "rt1_16.12u",		0x10000, 0x1bbcf37b, 5 | BRF_GRA },           // 14

	{ "rt1-1.3r",		0x00200, 0x8ef3bb9d, 6 | BRF_GRA },           // 15 Color Proms
	{ "rt1-2.3s",		0x00200, 0x6510a8f2, 6 | BRF_GRA },           // 16
	{ "rt1-3.4v",		0x00800, 0x95c7d944, 6 | BRF_GRA },           // 17
	{ "rt1-4.5v",		0x00800, 0x1391fec9, 6 | BRF_GRA },           // 18
	{ "rt1-5.6u",		0x00020, 0xe4130804, 6 | BRF_GRA },           // 19

	{ "rt3_4.6b",		0x08000, 0x00cf293f, 7 | BRF_PRG | BRF_ESS }, // 20 HD63701 MCU Code
	{ "cus60-60a1.mcu",	0x01000, 0x076ea82a, 7 | BRF_PRG | BRF_ESS }, // 21

	{ "rt1_17.f1",		0x10000, 0x766af455, 8 | BRF_PRG | BRF_ESS }, // 22 M6809 #0 Bank Code
	{ "rt1_18.h1",		0x10000, 0x3f9f2f5d, 8 | BRF_PRG | BRF_ESS }, // 23
	{ "rt3_19.k1",		0x10000, 0xc16675e9, 8 | BRF_PRG | BRF_ESS }, // 24
	{ "rt3_20.m1",		0x10000, 0xc470681b, 8 | BRF_PRG | BRF_ESS }, // 25

	{ "rt1_21.f3",		0x10000, 0x454968f3, 9 | BRF_SND },           // 26 Namco 63701x Samples
	{ "rt2_22.h3",		0x10000, 0xfe963e72, 9 | BRF_SND },           // 27
};

STD_ROM_PICK(rthunder)
STD_ROM_FN(rthunder)

static INT32 RthunderInit()
{
	enable_bankswitch2 = 1;

	return CommonInit(3, 1);
}

struct BurnDriver BurnDrvRthunder = {
	"rthunder", NULL, NULL, NULL, "1986",
	"Rolling Thunder (rev 3)\0", NULL, "Namco", "System 86",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, rthunderRomInfo, rthunderRomName, NULL, NULL, CommonInputInfo, RthunderDIPInfo,
	RthunderInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	288, 224, 4, 3
};

// Rolling Thunder (rev 3, alternate?)

static struct BurnRomInfo rthunderaRomDesc[] = {
	{ "1.9c",			0x08000, 0x13c92678, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code

	{ "rt3_3.12d",		0x08000, 0xa13f601c, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 #1 Code
	{ "rt3_2b.12c",		0x08000, 0xa7ea46ee, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "rt1_7.7r",		0x10000, 0xa85efa39, 3 | BRF_GRA },           //  3 Layer 0 & 1 Tiles
	{ "rt1_8.7s",		0x08000, 0xf7a95820, 3 | BRF_GRA },           //  4

	{ "rt1_5.4r",		0x08000, 0xd0fc470b, 4 | BRF_GRA },           //  5 Layer 2 & 3 Tiles
	{ "rt1_6.4s",		0x04000, 0x6b57edb2, 4 | BRF_GRA },           //  6

	{ "rt1_9.12h",		0x10000, 0x8e070561, 5 | BRF_GRA },           //  7 Sprites
	{ "rt1_10.12k",		0x10000, 0xcb8fb607, 5 | BRF_GRA },           //  8
	{ "rt1_11.12l",		0x10000, 0x2bdf5ed9, 5 | BRF_GRA },           //  9
	{ "rt1_12.12m",		0x10000, 0xe6c6c7dc, 5 | BRF_GRA },           // 10
	{ "rt1_13.12p",		0x10000, 0x489686d7, 5 | BRF_GRA },           // 11
	{ "rt1_14.12r",		0x10000, 0x689e56a8, 5 | BRF_GRA },           // 12
	{ "rt1_15.12t",		0x10000, 0x1d8bf2ca, 5 | BRF_GRA },           // 13
	{ "rt1_16.12u",		0x10000, 0x1bbcf37b, 5 | BRF_GRA },           // 14

	{ "rt1-1.3r",		0x00200, 0x8ef3bb9d, 6 | BRF_GRA },           // 15 Color Proms
	{ "rt1-2.3s",		0x00200, 0x6510a8f2, 6 | BRF_GRA },           // 16
	{ "rt1-3.4v",		0x00800, 0x95c7d944, 6 | BRF_GRA },           // 17
	{ "rt1-4.5v",		0x00800, 0x1391fec9, 6 | BRF_GRA },           // 18
	{ "rt1-5.6u",		0x00020, 0xe4130804, 6 | BRF_GRA },           // 19

	{ "rt3_4.6b",		0x08000, 0x00cf293f, 7 | BRF_PRG | BRF_ESS }, // 20 HD63701 MCU Code
	{ "cus60-60a1.mcu",	0x01000, 0x076ea82a, 7 | BRF_PRG | BRF_ESS }, // 21

	{ "rt1_17.f1",		0x10000, 0x766af455, 8 | BRF_PRG | BRF_ESS }, // 22 M6809 #0 Bank Code
	{ "rt1_18.h1",		0x10000, 0x3f9f2f5d, 8 | BRF_PRG | BRF_ESS }, // 23
	{ "rt3_19.k1",		0x10000, 0xc16675e9, 8 | BRF_PRG | BRF_ESS }, // 24
	{ "20.m1",			0x10000, 0x05d5db25, 8 | BRF_PRG | BRF_ESS }, // 25

	{ "rt1_21.f3",		0x10000, 0x454968f3, 9 | BRF_SND },           // 26 Namco 63701x Samples
	{ "rt2_22.h3",		0x10000, 0xfe963e72, 9 | BRF_SND },           // 27
};

STD_ROM_PICK(rthundera)
STD_ROM_FN(rthundera)

struct BurnDriver BurnDrvRthundera = {
	"rthundera", "rthunder", NULL, NULL, "1986",
	"Rolling Thunder (rev 3, alternate?)\0", NULL, "Namco", "System 86",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, rthunderaRomInfo, rthunderaRomName, NULL, NULL, CommonInputInfo, Rthunder1DIPInfo,
	RthunderInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	288, 224, 4, 3
};


// Rolling Thunder (rev 2)

static struct BurnRomInfo rthunder2RomDesc[] = {
	{ "rt2_1.9c",		0x08000, 0x7eaa9fdf, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code

	{ "rt2_3.12d",		0x08000, 0xf5d439d8, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 #1 Code
	{ "rt2_2.12c",		0x08000, 0x1c0e29e0, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "rt1_7.7r",		0x10000, 0xa85efa39, 3 | BRF_GRA },           //  3 Layer 0 & 1 Tiles
	{ "rt1_8.7s",		0x08000, 0xf7a95820, 3 | BRF_GRA },           //  4

	{ "rt1_5.4r",		0x08000, 0xd0fc470b, 4 | BRF_GRA },           //  5 Layer 2 & 3 Tiles
	{ "rt1_6.4s",		0x04000, 0x6b57edb2, 4 | BRF_GRA },           //  6

	{ "rt1_9.12h",		0x10000, 0x8e070561, 5 | BRF_GRA },           //  7 Sprites
	{ "rt1_10.12k",		0x10000, 0xcb8fb607, 5 | BRF_GRA },           //  8
	{ "rt1_11.12l",		0x10000, 0x2bdf5ed9, 5 | BRF_GRA },           //  9
	{ "rt1_12.12m",		0x10000, 0xe6c6c7dc, 5 | BRF_GRA },           // 10
	{ "rt1_13.12p",		0x10000, 0x489686d7, 5 | BRF_GRA },           // 11
	{ "rt1_14.12r",		0x10000, 0x689e56a8, 5 | BRF_GRA },           // 12
	{ "rt1_15.12t",		0x10000, 0x1d8bf2ca, 5 | BRF_GRA },           // 13
	{ "rt1_16.12u",		0x10000, 0x1bbcf37b, 5 | BRF_GRA },           // 14

	{ "rt1-1.3r",		0x00200, 0x8ef3bb9d, 6 | BRF_GRA },           // 15 Color Proms
	{ "rt1-2.3s",		0x00200, 0x6510a8f2, 6 | BRF_GRA },           // 16
	{ "rt1-3.4v",		0x00800, 0x95c7d944, 6 | BRF_GRA },           // 17
	{ "rt1-4.5v",		0x00800, 0x1391fec9, 6 | BRF_GRA },           // 18
	{ "rt1-5.6u",		0x00020, 0xe4130804, 6 | BRF_GRA },           // 19

	{ "rt2_4.6b",		0x08000, 0x0387464f, 7 | BRF_PRG | BRF_ESS }, // 20 HD63701 MCU Code
	{ "cus60-60a1.mcu",	0x01000, 0x076ea82a, 7 | BRF_PRG | BRF_ESS }, // 21

	{ "rt1_17.f1",		0x10000, 0x766af455, 8 | BRF_PRG | BRF_ESS }, // 22 M6809 #0 Bank Code
	{ "rt1_18.h1",		0x10000, 0x3f9f2f5d, 8 | BRF_PRG | BRF_ESS }, // 23
	{ "rt3_19.k1",		0x10000, 0xc16675e9, 8 | BRF_PRG | BRF_ESS }, // 24
	{ "rt3_20.m1",		0x10000, 0xc470681b, 8 | BRF_PRG | BRF_ESS }, // 25

	{ "rt1_21.f3",		0x10000, 0x454968f3, 9 | BRF_SND },           // 26 Namco 63701x Samples
	{ "rt2_22.h3",		0x10000, 0xfe963e72, 9 | BRF_SND },           // 27
};

STD_ROM_PICK(rthunder2)
STD_ROM_FN(rthunder2)

struct BurnDriver BurnDrvRthunder2 = {
	"rthunder2", "rthunder", NULL, NULL, "1986",
	"Rolling Thunder (rev 2)\0", NULL, "Namco", "System 86",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, rthunder2RomInfo, rthunder2RomName, NULL, NULL, CommonInputInfo, Rthunder1DIPInfo,
	RthunderInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	288, 224, 4, 3
};


// Rolling Thunder (rev 1)
// some roms (mcu + samples) and maybe r19 updated to rt2

static struct BurnRomInfo rthunder1RomDesc[] = {
	{ "rt1_1b.9c",		0x08000, 0x6f8c1252, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code

	{ "rt1_3.12d",		0x08000, 0xaaa82885, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 #1 Code
	{ "rt1_2b.12c",		0x08000, 0xf22a03d8, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "rt1_7.7r",		0x10000, 0xa85efa39, 3 | BRF_GRA },           //  3 Layer 0 & 1 Tiles
	{ "rt1_8.7s",		0x08000, 0xf7a95820, 3 | BRF_GRA },           //  4

	{ "rt1_5.4r",		0x08000, 0xd0fc470b, 4 | BRF_GRA },           //  5 Layer 2 & 3 Tiles2
	{ "rt1_6.4s",		0x04000, 0x6b57edb2, 4 | BRF_GRA },           //  6

	{ "rt1_9.12h",		0x10000, 0x8e070561, 5 | BRF_GRA },           //  7 Sprites
	{ "rt1_10.12k",		0x10000, 0xcb8fb607, 5 | BRF_GRA },           //  8
	{ "rt1_11.12l",		0x10000, 0x2bdf5ed9, 5 | BRF_GRA },           //  9
	{ "rt1_12.12m",		0x10000, 0xe6c6c7dc, 5 | BRF_GRA },           // 10
	{ "rt1_13.12p",		0x10000, 0x489686d7, 5 | BRF_GRA },           // 11
	{ "rt1_14.12r",		0x10000, 0x689e56a8, 5 | BRF_GRA },           // 12
	{ "rt1_15.12t",		0x10000, 0x1d8bf2ca, 5 | BRF_GRA },           // 13
	{ "rt1_16.12u",		0x10000, 0x1bbcf37b, 5 | BRF_GRA },           // 14

	{ "rt1-1.3r",		0x00200, 0x8ef3bb9d, 6 | BRF_GRA },           // 15 Color Proms
	{ "rt1-2.3s",		0x00200, 0x6510a8f2, 6 | BRF_GRA },           // 16
	{ "rt1-3.4v",		0x00800, 0x95c7d944, 6 | BRF_GRA },           // 17
	{ "rt1-4.5v",		0x00800, 0x1391fec9, 6 | BRF_GRA },           // 18
	{ "rt1-5.6u",		0x00020, 0xe4130804, 6 | BRF_GRA },           // 19

	{ "rt2_4.6b",		0x08000, 0x0387464f, 7 | BRF_PRG | BRF_ESS }, // 20 HD63701 MCU Code
	{ "cus60-60a1.mcu",	0x01000, 0x076ea82a, 7 | BRF_PRG | BRF_ESS }, // 21

	{ "rt1_17.f1",		0x10000, 0x766af455, 8 | BRF_PRG | BRF_ESS }, // 22 M6809 #0 Bank Code
	{ "rt1_18.h1",		0x10000, 0x3f9f2f5d, 8 | BRF_PRG | BRF_ESS }, // 23
	{ "r19",			0x10000, 0xfe9343b0, 8 | BRF_PRG | BRF_ESS }, // 24
	{ "rt1_20.m1",		0x10000, 0xf8518d4f, 8 | BRF_PRG | BRF_ESS }, // 25

	{ "rt1_21.f3",		0x10000, 0x454968f3, 9 | BRF_SND },           // 26 Namco 63701x Samples
	{ "rt2_22.h3",		0x10000, 0xfe963e72, 9 | BRF_SND },           // 27
};

STD_ROM_PICK(rthunder1)
STD_ROM_FN(rthunder1)

struct BurnDriver BurnDrvRthunder1 = {
	"rthunder1", "rthunder", NULL, NULL, "1986",
	"Rolling Thunder (rev 1)\0", NULL, "Namco", "System 86",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, rthunder1RomInfo, rthunder1RomName, NULL, NULL, CommonInputInfo, Rthunder1DIPInfo,
	RthunderInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	288, 224, 4, 3
};


// Rolling Thunder (oldest)

static struct BurnRomInfo rthunder0RomDesc[] = {
	{ "rt1_1b.9c",		0x08000, 0x6f8c1252, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code

	{ "rt1_3.12d",		0x08000, 0xaaa82885, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 #1 Code
	{ "rt1_2b.12c",		0x08000, 0xf22a03d8, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "rt1_7.7r",		0x10000, 0xa85efa39, 3 | BRF_GRA },           //  3 Layer 0 & 1 Tiles
	{ "rt1_8.7s",		0x08000, 0xf7a95820, 3 | BRF_GRA },           //  4

	{ "rt1_5.4r",		0x08000, 0xd0fc470b, 4 | BRF_GRA },           //  5 Layer 2 & 3 Tiles2
	{ "rt1_6.4s",		0x04000, 0x6b57edb2, 4 | BRF_GRA },           //  6

	{ "rt1_9.12h",		0x10000, 0x8e070561, 5 | BRF_GRA },           //  7 Sprites
	{ "rt1_10.12k",		0x10000, 0xcb8fb607, 5 | BRF_GRA },           //  8
	{ "rt1_11.12l",		0x10000, 0x2bdf5ed9, 5 | BRF_GRA },           //  9
	{ "rt1_12.12m",		0x10000, 0xe6c6c7dc, 5 | BRF_GRA },           // 10
	{ "rt1_13.12p",		0x10000, 0x489686d7, 5 | BRF_GRA },           // 11
	{ "rt1_14.12r",		0x10000, 0x689e56a8, 5 | BRF_GRA },           // 12
	{ "rt1_15.12t",		0x10000, 0x1d8bf2ca, 5 | BRF_GRA },           // 13
	{ "rt1_16.12u",		0x10000, 0x1bbcf37b, 5 | BRF_GRA },           // 14

	{ "rt1-1.3r",		0x00200, 0x8ef3bb9d, 6 | BRF_GRA },           // 15 Color Proms
	{ "rt1-2.3s",		0x00200, 0x6510a8f2, 6 | BRF_GRA },           // 16
	{ "rt1-3.4v",		0x00800, 0x95c7d944, 6 | BRF_GRA },           // 17
	{ "rt1-4.5v",		0x00800, 0x1391fec9, 6 | BRF_GRA },           // 18
	{ "rt1-5.6u",		0x00020, 0xe4130804, 6 | BRF_GRA },           // 19

	{ "rt1_4.6b",		0x08000, 0x3f795094, 7 | BRF_PRG | BRF_ESS }, // 20 HD63701 MCU Code
	{ "cus60-60a1.mcu",	0x01000, 0x076ea82a, 7 | BRF_PRG | BRF_ESS }, // 21

	{ "rt1_17.f1",		0x10000, 0x766af455, 8 | BRF_PRG | BRF_ESS }, // 22 M6809 #0 Bank Code
	{ "rt1_18.h1",		0x10000, 0x3f9f2f5d, 8 | BRF_PRG | BRF_ESS }, // 23
	{ "rt1_19.k1",		0x10000, 0x1273a048, 8 | BRF_PRG | BRF_ESS }, // 24
	{ "rt1_20.m1",		0x10000, 0xf8518d4f, 8 | BRF_PRG | BRF_ESS }, // 25

	{ "rt1_21.f3",		0x10000, 0x454968f3, 9 | BRF_SND },           // 26 Namco 63701x Samples
	{ "rt1_22.h3",		0x10000, 0x842a2fd4, 9 | BRF_SND },           // 27
};

STD_ROM_PICK(rthunder0)
STD_ROM_FN(rthunder0)

struct BurnDriver BurnDrvRthunder0 = {
	"rthunder0", "rthunder", NULL, NULL, "1986",
	"Rolling Thunder (oldest)\0", NULL, "Namco", "System 86",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, rthunder0RomInfo, rthunder0RomName, NULL, NULL, CommonInputInfo, Rthunder1DIPInfo,
	RthunderInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	288, 224, 4, 3
};


// Wonder Momo

static struct BurnRomInfo wndrmomoRomDesc[] = {
	{ "wm1_1.9c",		0x08000, 0x34b50bf0, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code

	{ "wm1_2.12c",		0x08000, 0x3181efd0, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 #1 Code

	{ "wm1_6.7r",		0x08000, 0x93955fbb, 3 | BRF_GRA },           //  2 Layer 0 & 1 Tiles
	{ "wm1_7.7s",		0x04000, 0x7d662527, 3 | BRF_GRA },           //  3

	{ "wm1_4.4r",		0x08000, 0xbbe67836, 4 | BRF_GRA },           //  4 Layer 2 & 3 Tiles
	{ "wm1_5.4s",		0x04000, 0xa81b481f, 4 | BRF_GRA },           //  5

	{ "wm1_8.12h",		0x10000, 0x14f52e72, 5 | BRF_GRA },           //  6 Sprites
	{ "wm1_9.12k",		0x10000, 0x16f8cdae, 5 | BRF_GRA },           //  7
	{ "wm1_10.12l",		0x10000, 0xbfbc1896, 5 | BRF_GRA },           //  8
	{ "wm1_11.12m",		0x10000, 0xd775ddb2, 5 | BRF_GRA },           //  9
	{ "wm1_12.12p",		0x10000, 0xde64c12f, 5 | BRF_GRA },           // 10
	{ "wm1_13.12r",		0x10000, 0xcfe589ad, 5 | BRF_GRA },           // 11
	{ "wm1_14.12t",		0x10000, 0x2ae21a53, 5 | BRF_GRA },           // 12
	{ "wm1_15.12u",		0x10000, 0xb5c98be0, 5 | BRF_GRA },           // 13

	{ "wm1-1.3r",		0x00200, 0x1af8ade8, 6 | BRF_GRA },           // 14 Color Proms
	{ "wm1-2.3s",		0x00200, 0x8694e213, 6 | BRF_GRA },           // 15
	{ "wm1-3.4v",		0x00800, 0x2ffaf9a4, 6 | BRF_GRA },           // 16
	{ "wm1-4.5v",		0x00800, 0xf4e83e0b, 6 | BRF_GRA },           // 17
	{ "wm1-5.6u",		0x00020, 0xe4130804, 6 | BRF_GRA },           // 18

	{ "wm1_3.6b",		0x08000, 0x55f01df7, 7 | BRF_PRG | BRF_ESS }, // 19 HD63701 MCU Code
	{ "cus60-60a1.mcu",	0x01000, 0x076ea82a, 7 | BRF_PRG | BRF_ESS }, // 20

	{ "wm1_16.f1",		0x10000, 0xe565f8f3, 8 | BRF_PRG | BRF_ESS }, // 21 M6809 #0 Bank Code

	{ "wm1_17.f3",		0x10000, 0xbea3c318, 9 | BRF_SND },           // 22 Namco 63701x Samples
	{ "wm1_18.h3",		0x10000, 0x6d73bcc5, 9 | BRF_SND },           // 23
	{ "wm1_19.k3",		0x10000, 0xd288e912, 9 | BRF_SND },           // 24
	{ "wm1_20.m3",		0x10000, 0x076a72cb, 9 | BRF_SND },           // 25
};

STD_ROM_PICK(wndrmomo)
STD_ROM_FN(wndrmomo)

static INT32 WndrmomoInit()
{
	return CommonInit(4, 1);
}

struct BurnDriver BurnDrvWndrmomo = {
	"wndrmomo", NULL, NULL, NULL, "1987",
	"Wonder Momo\0", NULL, "Namco", "System 86",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, wndrmomoRomInfo, wndrmomoRomName, NULL, NULL, CommonInputInfo, WndrmomoDIPInfo,
	WndrmomoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	288, 224, 4, 3
};
