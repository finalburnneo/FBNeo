// FB Alpha Blood Bros. driver module
// Based on MAME driver by Carlos A. Lozano Baides and Richard Bush

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "seibusnd.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *Drv68KRAM;
static UINT8 *Drv68KRAM1;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvSndROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvPalRAM;
static UINT8 *DrvBgRAM;
static UINT8 *DrvTxRAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvScrollRAM;
static UINT32 *DrvPalette;

static UINT8 DrvRecalc;

static UINT8 DrvJoy1[2];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvDips[2];
static UINT8 DrvReset;
static UINT16 DrvInputs[3];

static INT32 nGameSelect = 0;

static struct BurnInputInfo BloodbroInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 8,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 9,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 13,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 14,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 12,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Bloodbro)

static struct BurnInputInfo WeststryInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 8,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 9,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 13,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 14,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Weststry)

static struct BurnInputInfo SkysmashInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 8,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 9,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 13,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 12,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Skysmash)

static struct BurnDIPInfo BloodbroDIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL			},
	{0x15, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Coin Mode"		},
	{0x14, 0x01, 0x01, 0x01, "1"			},
	{0x14, 0x01, 0x01, 0x00, "2"			},

	// Coinage condition: Coin Mode 1
	{0   , 0xfe, 0   ,   16, "Coinage"		},
	{0x14, 0x02, 0x1e, 0x14, "6 Coins 1 Credit"	},
	{0x14, 0x00, 0x01, 0x01, NULL},
	{0x14, 0x02, 0x1e, 0x16, "5 Coins 1 Credit"	},
	{0x14, 0x00, 0x01, 0x01, NULL},
	{0x14, 0x02, 0x1e, 0x18, "4 Coins 1 Credit"	},
	{0x14, 0x00, 0x01, 0x01, NULL},
	{0x14, 0x02, 0x1e, 0x1a, "3 Coins 1 Credit"	},
	{0x14, 0x00, 0x01, 0x01, NULL},
	{0x14, 0x02, 0x1e, 0x02, "8 Coins 3 Credits"	},
	{0x14, 0x00, 0x01, 0x01, NULL},
	{0x14, 0x02, 0x1e, 0x1c, "2 Coins 1 Credit"	},
	{0x14, 0x00, 0x01, 0x01, NULL},
	{0x14, 0x02, 0x1e, 0x04, "5 Coins 3 Credits"	},
	{0x14, 0x00, 0x01, 0x01, NULL},
	{0x14, 0x02, 0x1e, 0x06, "3 Coins 2 Credits"	},
	{0x14, 0x00, 0x01, 0x01, NULL},
	{0x14, 0x02, 0x1e, 0x1e, "1 Coin  1 Credit"	},
	{0x14, 0x00, 0x01, 0x01, NULL},
	{0x14, 0x02, 0x1e, 0x08, "2 Coins 3 Credits"	},
	{0x14, 0x00, 0x01, 0x01, NULL},
	{0x14, 0x02, 0x1e, 0x12, "1 Coin  2 Credits"	},
	{0x14, 0x00, 0x01, 0x01, NULL},
	{0x14, 0x02, 0x1e, 0x10, "1 Coin  3 Credits"	},
	{0x14, 0x00, 0x01, 0x01, NULL},
	{0x14, 0x02, 0x1e, 0x0e, "1 Coin  4 Credits"	},
	{0x14, 0x00, 0x01, 0x01, NULL},
	{0x14, 0x02, 0x1e, 0x0c, "1 Coin  5 Credits"	},
	{0x14, 0x00, 0x01, 0x01, NULL},
	{0x14, 0x02, 0x1e, 0x0a, "1 Coin  6 Credits"	},
	{0x14, 0x00, 0x01, 0x01, NULL},
	{0x14, 0x02, 0x1e, 0x00, "Free Play"		},
	{0x14, 0x00, 0x01, 0x01, NULL},

	// Coin A condition: Coin Mode 2
	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x14, 0x02, 0x06, 0x00, "5 Coins 1 Credit"	},
	{0x14, 0x00, 0x01, 0x00, NULL},
	{0x14, 0x02, 0x06, 0x02, "3 Coins 1 Credit"	},
	{0x14, 0x00, 0x01, 0x00, NULL},
	{0x14, 0x02, 0x06, 0x04, "2 Coins 1 Credit"	},
	{0x14, 0x00, 0x01, 0x00, NULL},
	{0x14, 0x02, 0x06, 0x06, "1 Coin  1 Credit"	},
	{0x14, 0x00, 0x01, 0x00, NULL},

	// Coin B condition: Coin Mode 2
	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x14, 0x02, 0x18, 0x18, "1 Coin 2 Credits"	},
	{0x14, 0x00, 0x01, 0x00, NULL},
	{0x14, 0x02, 0x18, 0x10, "1 Coin 3 Credits"	},
	{0x14, 0x00, 0x01, 0x00, NULL},
	{0x14, 0x02, 0x18, 0x08, "1 Coin 5 Credits"	},
	{0x14, 0x00, 0x01, 0x00, NULL},
	{0x14, 0x02, 0x18, 0x00, "1 Coin 6 Credits"	},
	{0x14, 0x00, 0x01, 0x00, NULL},

	{0   , 0xfe, 0   ,    2, "Starting Coin"	},
	{0x14, 0x01, 0x20, 0x20, "Normal"		},
	{0x14, 0x01, 0x20, 0x00, "x2"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x15, 0x01, 0x03, 0x00, "1"			},
	{0x15, 0x01, 0x03, 0x02, "2"			},
	{0x15, 0x01, 0x03, 0x03, "3"			},
	{0x15, 0x01, 0x03, 0x01, "5"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x15, 0x01, 0x0c, 0x0c, "300K, 500K Every"	},
	{0x15, 0x01, 0x0c, 0x08, "500K, 500K Every"	},
	{0x15, 0x01, 0x0c, 0x04, "500K Only"		},
	{0x15, 0x01, 0x0c, 0x00, "None"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x15, 0x01, 0x30, 0x20, "Easy"			},
	{0x15, 0x01, 0x30, 0x30, "Normal"		},
	{0x15, 0x01, 0x30, 0x10, "Hard"			},
	{0x15, 0x01, 0x30, 0x00, "Very Hard"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x15, 0x01, 0x40, 0x00, "No"			},
	{0x15, 0x01, 0x40, 0x40, "Yes"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x15, 0x01, 0x80, 0x00, "Off"			},
	{0x15, 0x01, 0x80, 0x80, "On"			},
};

STDDIPINFO(Bloodbro)

static struct BurnDIPInfo WeststryDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Coin Mode"		},
	{0x13, 0x01, 0x01, 0x01, "1"			},
	{0x13, 0x01, 0x01, 0x00, "2"			},

	// Coinage condition: Coin Mode 1
	{0   , 0xfe, 0   ,   16, "Coinage"		},
	{0x13, 0x02, 0x1e, 0x14, "6 Coins 1 Credit"	},
	{0x13, 0x00, 0x01, 0x01, NULL},
	{0x13, 0x02, 0x1e, 0x16, "5 Coins 1 Credit"	},
	{0x13, 0x00, 0x01, 0x01, NULL},
	{0x13, 0x02, 0x1e, 0x18, "4 Coins 1 Credit"	},
	{0x13, 0x00, 0x01, 0x01, NULL},
	{0x13, 0x02, 0x1e, 0x1a, "3 Coins 1 Credit"	},
	{0x13, 0x00, 0x01, 0x01, NULL},
	{0x13, 0x02, 0x1e, 0x02, "8 Coins 3 Credits"	},
	{0x13, 0x00, 0x01, 0x01, NULL},
	{0x13, 0x02, 0x1e, 0x1c, "2 Coins 1 Credit"	},
	{0x13, 0x00, 0x01, 0x01, NULL},
	{0x13, 0x02, 0x1e, 0x04, "5 Coins 3 Credits"	},
	{0x13, 0x00, 0x01, 0x01, NULL},
	{0x13, 0x02, 0x1e, 0x06, "3 Coins 2 Credits"	},
	{0x13, 0x00, 0x01, 0x01, NULL},
	{0x13, 0x02, 0x1e, 0x1e, "1 Coin  1 Credit"	},
	{0x13, 0x00, 0x01, 0x01, NULL},
	{0x13, 0x02, 0x1e, 0x08, "2 Coins 3 Credits"	},
	{0x13, 0x00, 0x01, 0x01, NULL},
	{0x13, 0x02, 0x1e, 0x12, "1 Coin  2 Credits"	},
	{0x13, 0x00, 0x01, 0x01, NULL},
	{0x13, 0x02, 0x1e, 0x10, "1 Coin  3 Credits"	},
	{0x13, 0x00, 0x01, 0x01, NULL},
	{0x13, 0x02, 0x1e, 0x0e, "1 Coin  4 Credits"	},
	{0x13, 0x00, 0x01, 0x01, NULL},
	{0x13, 0x02, 0x1e, 0x0c, "1 Coin  5 Credits"	},
	{0x13, 0x00, 0x01, 0x01, NULL},
	{0x13, 0x02, 0x1e, 0x0a, "1 Coin  6 Credits"	},
	{0x13, 0x00, 0x01, 0x01, NULL},
	{0x13, 0x02, 0x1e, 0x00, "Free Play"		},
	{0x13, 0x00, 0x01, 0x01, NULL			},

	// Coin A condition: Coin Mode 2
	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x13, 0x02, 0x06, 0x00, "5 Coins 1 Credit"	},
	{0x13, 0x00, 0x01, 0x00, NULL},
	{0x13, 0x02, 0x06, 0x02, "3 Coins 1 Credit"	},
	{0x13, 0x00, 0x01, 0x00, NULL},
	{0x13, 0x02, 0x06, 0x04, "2 Coins 1 Credit"	},
	{0x13, 0x00, 0x01, 0x00, NULL},
	{0x13, 0x02, 0x06, 0x06, "1 Coin  1 Credit"	},
	{0x13, 0x00, 0x01, 0x00, NULL},

	// Coin B condition: Coin Mode 2
	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x13, 0x02, 0x18, 0x18, "1 Coin 2 Credits"	},
	{0x13, 0x00, 0x01, 0x00, NULL			},
	{0x13, 0x02, 0x18, 0x10, "1 Coin 3 Credits"	},
	{0x13, 0x00, 0x01, 0x00, NULL			},
	{0x13, 0x02, 0x18, 0x08, "1 Coin 5 Credits"	},
	{0x13, 0x00, 0x01, 0x00, NULL			},
	{0x13, 0x02, 0x18, 0x00, "1 Coin 6 Credits"	},
	{0x13, 0x00, 0x01, 0x00, NULL			},

	{0   , 0xfe, 0   ,    2, "Starting Coin"	},
	{0x13, 0x01, 0x20, 0x20, "Normal"		},
	{0x13, 0x01, 0x20, 0x00, "x2"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0x03, 0x00, "1"			},
	{0x14, 0x01, 0x03, 0x02, "2"			},
	{0x14, 0x01, 0x03, 0x03, "3"			},
	{0x14, 0x01, 0x03, 0x01, "5"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x14, 0x01, 0x0c, 0x0c, "300K, 500K Every"	},
	{0x14, 0x01, 0x0c, 0x08, "500K, 500K Every"	},
	{0x14, 0x01, 0x0c, 0x04, "500K Only"		},
	{0x14, 0x01, 0x0c, 0x00, "None"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x30, 0x20, "Easy"			},
	{0x14, 0x01, 0x30, 0x30, "Normal"		},
	{0x14, 0x01, 0x30, 0x10, "Hard"			},
	{0x14, 0x01, 0x30, 0x00, "Very Hard"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x14, 0x01, 0x40, 0x00, "No"			},
	{0x14, 0x01, 0x40, 0x40, "Yes"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x14, 0x01, 0x80, 0x00, "Off"			},
	{0x14, 0x01, 0x80, 0x80, "On"			},
};

STDDIPINFO(Weststry)

static struct BurnDIPInfo SkysmashDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Coin Mode"		},
	{0x12, 0x01, 0x01, 0x01, "1"			},
	{0x12, 0x01, 0x01, 0x00, "2"			},

	// Coinage condition: Coin Mode 1
	{0   , 0xfe, 0   ,   16, "Coinage"		},
	{0x12, 0x02, 0x1e, 0x14, "6 Coins 1 Credit"	},
	{0x12, 0x00, 0x01, 0x01, NULL},
	{0x12, 0x02, 0x1e, 0x16, "5 Coins 1 Credit"	},
	{0x12, 0x00, 0x01, 0x01, NULL},
	{0x12, 0x02, 0x1e, 0x18, "4 Coins 1 Credit"	},
	{0x12, 0x00, 0x01, 0x01, NULL},
	{0x12, 0x02, 0x1e, 0x1a, "3 Coins 1 Credit"	},
	{0x12, 0x00, 0x01, 0x01, NULL},
	{0x12, 0x02, 0x1e, 0x02, "8 Coins 3 Credits"	},
	{0x12, 0x00, 0x01, 0x01, NULL},
	{0x12, 0x02, 0x1e, 0x1c, "2 Coins 1 Credit"	},
	{0x12, 0x00, 0x01, 0x01, NULL},
	{0x12, 0x02, 0x1e, 0x04, "5 Coins 3 Credits"	},
	{0x12, 0x00, 0x01, 0x01, NULL},
	{0x12, 0x02, 0x1e, 0x06, "3 Coins 2 Credits"	},
	{0x12, 0x00, 0x01, 0x01, NULL},
	{0x12, 0x02, 0x1e, 0x1e, "1 Coin  1 Credit"	},
	{0x12, 0x00, 0x01, 0x01, NULL},
	{0x12, 0x02, 0x1e, 0x08, "2 Coins 3 Credits"	},
	{0x12, 0x00, 0x01, 0x01, NULL},
	{0x12, 0x02, 0x1e, 0x12, "1 Coin  2 Credits"	},
	{0x12, 0x00, 0x01, 0x01, NULL},
	{0x12, 0x02, 0x1e, 0x10, "1 Coin  3 Credits"	},
	{0x12, 0x00, 0x01, 0x01, NULL},
	{0x12, 0x02, 0x1e, 0x0e, "1 Coin  4 Credits"	},
	{0x12, 0x00, 0x01, 0x01, NULL},
	{0x12, 0x02, 0x1e, 0x0c, "1 Coin  5 Credits"	},
	{0x12, 0x00, 0x01, 0x01, NULL},
	{0x12, 0x02, 0x1e, 0x0a, "1 Coin  6 Credits"	},
	{0x12, 0x00, 0x01, 0x01, NULL},
	{0x12, 0x02, 0x1e, 0x00, "Free Play"		},
	{0x12, 0x00, 0x01, 0x01, NULL},

	// Coin A condition: Coin Mode 2
	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x12, 0x02, 0x06, 0x00, "5 Coins 1 Credit"	},
	{0x12, 0x00, 0x01, 0x00, NULL		},
	{0x12, 0x02, 0x06, 0x02, "3 Coins 1 Credit"	},
	{0x12, 0x00, 0x01, 0x00, NULL		},
	{0x12, 0x02, 0x06, 0x04, "2 Coins 1 Credit"	},
	{0x12, 0x00, 0x01, 0x00, NULL},
	{0x12, 0x02, 0x06, 0x06, "1 Coin  1 Credit"	},
	{0x12, 0x00, 0x01, 0x00, NULL},

	// Coin B condition: Coin Mode 2
	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x12, 0x02, 0x18, 0x18, "1 Coin 2 Credits"	},
	{0x12, 0x00, 0x01, 0x00, NULL},
	{0x12, 0x02, 0x18, 0x10, "1 Coin 3 Credits"	},
	{0x12, 0x00, 0x01, 0x00, NULL},
	{0x12, 0x02, 0x18, 0x08, "1 Coin 5 Credits"	},
	{0x12, 0x00, 0x01, 0x00, NULL},
	{0x12, 0x02, 0x18, 0x00, "1 Coin 6 Credits"	},
	{0x12, 0x00, 0x01, 0x00, NULL},

	{0   , 0xfe, 0   ,    2, "Starting Coin"	},
	{0x12, 0x01, 0x20, 0x20, "Normal"		},
	{0x12, 0x01, 0x20, 0x00, "x2"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0x03, 0x02, "2"			},
	{0x13, 0x01, 0x03, 0x03, "3"			},
	{0x13, 0x01, 0x03, 0x01, "5"			},
	{0x13, 0x01, 0x03, 0x00, "Infinite"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x13, 0x01, 0x0c, 0x0c, "120K, 200K Every"	},
	{0x13, 0x01, 0x0c, 0x08, "200K, 200K Every"	},
	{0x13, 0x01, 0x0c, 0x04, "250K, 250K Every"	},
	{0x13, 0x01, 0x0c, 0x00, "200K Only"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x30, 0x00, "Easy"			},
	{0x13, 0x01, 0x30, 0x30, "Normal"		},
	{0x13, 0x01, 0x30, 0x20, "Hard"			},
	{0x13, 0x01, 0x30, 0x10, "Very Hard"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x13, 0x01, 0x40, 0x00, "No"			},
	{0x13, 0x01, 0x40, 0x40, "Yes"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x80, 0x00, "Off"			},
	{0x13, 0x01, 0x80, 0x80, "On"			},
};

STDDIPINFO(Skysmash)

static void __fastcall bloodbro_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xffffff0) == 0xa0000) {
		seibu_main_word_write(address, data);
		return;
	}
}

static void __fastcall bloodbro_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xffffff0) == 0xa0000) {
		seibu_main_word_write(address, data);
		return;
	}

	if ((address & 0xfffff80) == 0xc0000) {
		*((UINT16*)(DrvScrollRAM + (address & 0x7e))) = BURN_ENDIAN_SWAP_INT16(data);
		return;
	}
}

static UINT8 __fastcall bloodbro_read_byte(UINT32 address)
{
	if ((address & 0xffffff0) == 0xa0000) {
		return seibu_main_word_read(address);
	}

	return 0;
}

static UINT16 __fastcall bloodbro_read_word(UINT32 address)
{
	if ((address & 0xffffff0) == 0xa0000) {
		return seibu_main_word_read(address);
	}

	switch (address)
	{
		case 0xe0000:
			return ((DrvDips[1] << 8) | DrvDips[0]);

		case 0xe0002:
			return DrvInputs[1];

		case 0xe0004:
			return DrvInputs[2];
	}

	return 0;
}

static INT32 DrvDoReset()
{
	DrvReset = 0;

	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	seibu_sound_reset();

	HiscoreReset();

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[4]  = { 0x00000, 0x00004, 0x80000, 0x80004 };
	INT32 XOffs0[8]  = { 0x003, 0x002, 0x001, 0x000, 0x00b, 0x00a, 0x009, 0x008 };
	INT32 YOffs0[8]  = { 0x000, 0x010, 0x020, 0x030, 0x040, 0x050, 0x060, 0x070 };

	INT32 Plane1[4]  = { 0x008, 0x00c, 0x000, 0x004 };
	INT32 XOffs1[16] = { 0x003, 0x002, 0x001, 0x000, 0x013, 0x012, 0x011, 0x010,
			   0x203, 0x202, 0x201, 0x200, 0x213, 0x212, 0x211, 0x210 };
	INT32 YOffs1[16] = { 0x000, 0x020, 0x040, 0x060, 0x080, 0x0a0, 0x0c0, 0x0e0,
			   0x100, 0x120, 0x140, 0x160, 0x180, 0x1a0, 0x1c0, 0x1e0 };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x100000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x020000);

	GfxDecode(0x1000, 4,  8,  8, Plane0, XOffs0, YOffs0, 0x080, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x100000);

	GfxDecode(0x2000, 4, 16, 16, Plane1, XOffs1, YOffs1, 0x400, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x100000);

	GfxDecode(0x2000, 4, 16, 16, Plane1, XOffs1, YOffs1, 0x400, tmp, DrvGfxROM2);

	BurnFree (tmp);

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM	= Next; Next += 0x080000;
	SeibuZ80ROM	= Next;
	DrvZ80ROM	= Next; Next += 0x020000;

	DrvGfxROM2	= Next; Next += 0x200000;
	DrvGfxROM0	= Next; Next += 0x040000;
	DrvGfxROM1	= Next; Next += 0x200000;

	MSM6295ROM	= Next;
	DrvSndROM	= Next; Next += 0x040000;

	DrvPalette	= (UINT32*)Next; Next += 0x0800 * sizeof(UINT32);

	AllRam		= Next;

	Drv68KRAM	= Next; Next += 0x00b800;
	Drv68KRAM1	= Next; Next += 0x009000;

	DrvPalRAM	= Next; Next += 0x001000;
	DrvSprRAM	= Next; Next += 0x001000;
	DrvBgRAM	= Next; Next += 0x001000;
	DrvTxRAM	= Next; Next += 0x002800;
	DrvFgRAM	= Next; Next += 0x001000;

	DrvScrollRAM	= Next; Next += 0x000080;

	SeibuZ80RAM	= Next;
	DrvZ80RAM	= Next; Next += 0x000800;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static tilemap_callback( background )
{
	UINT16 *ram = (UINT16*)DrvBgRAM;

	TILE_SET_INFO(1, BURN_ENDIAN_SWAP_INT16(ram[offs]) & 0xfff, BURN_ENDIAN_SWAP_INT16(ram[offs]) >> 12, 0);
}

static tilemap_callback( foreground )
{
	UINT16 *ram = (UINT16*)DrvFgRAM;

	TILE_SET_INFO(2, (BURN_ENDIAN_SWAP_INT16(ram[offs]) & 0xfff) + 0x1000, BURN_ENDIAN_SWAP_INT16(ram[offs]) >> 12, 0);
}

static tilemap_callback( text )
{
	UINT16 *ram = (UINT16*)DrvTxRAM;

	TILE_SET_INFO(0, BURN_ENDIAN_SWAP_INT16(ram[offs]) & 0xfff, BURN_ENDIAN_SWAP_INT16(ram[offs]) >> 12, 0);
}

static INT32 DrvInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (!strcmp(BurnDrvGetTextA(DRV_NAME), "weststry") || !strcmp(BurnDrvGetTextA(DRV_NAME), "weststrya"))
	{
		{
			if (BurnLoadRom(Drv68KROM + 0x000000, 0, 2)) return 1;
			if (BurnLoadRom(Drv68KROM + 0x000001, 1, 2)) return 1;
			if (BurnLoadRom(Drv68KROM + 0x040000, 2, 2)) return 1;
			if (BurnLoadRom(Drv68KROM + 0x040001, 3, 2)) return 1;

			if (BurnLoadRom(DrvZ80ROM + 0x000000, 4, 1)) return 1;
			memcpy (DrvZ80ROM + 0x10000, DrvZ80ROM + 0x8000, 0x8000);
			memcpy (DrvZ80ROM + 0x18000, DrvZ80ROM + 0x8000, 0x8000);

			UINT8 *pTemp = (UINT8*)BurnMalloc(0x10000);
			if (BurnLoadRom(pTemp      + 0x00000, 5, 1)) return 1;
			memcpy(DrvGfxROM0 + 0x00000, pTemp + 0x8000, 0x8000);
			if (BurnLoadRom(pTemp      + 0x00000, 6, 1)) return 1;
			memcpy(DrvGfxROM0 + 0x08000, pTemp + 0x8000, 0x8000);
			if (BurnLoadRom(pTemp      + 0x00000, 7, 1)) return 1;
			memcpy(DrvGfxROM0 + 0x10000, pTemp + 0x8000, 0x8000);
			if (BurnLoadRom(pTemp      + 0x00000, 8, 1)) return 1;
			memcpy(DrvGfxROM0 + 0x18000, pTemp + 0x8000, 0x8000);
			BurnFree(pTemp);

			if (BurnLoadRom(DrvGfxROM1 + 0x20000, 9, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x60000,10, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0xa0000,11, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0xe0000,12, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x00000,13, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x40000,14, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x80000,15, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0xc0000,16, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM2 + 0x00000,17, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x20000,18, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x40000,19, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x60000,20, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x80000,21, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0xa0000,22, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0xc0000,23, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0xe0000,24, 1)) return 1;

			if (BurnLoadRom(DrvSndROM + 0x000000,25, 1)) return 1;

			DrvGfxDecode();
		}

		SekInit(0, 0x68000);
		SekOpen(0);
		SekMapMemory(Drv68KROM,			0x000000, 0x07ffff, MAP_ROM);
		SekMapMemory(Drv68KRAM,			0x080000, 0x08afff, MAP_RAM);
		SekMapMemory(DrvSprRAM,			0x08b000, 0x08bfff, MAP_RAM);
		SekMapMemory(DrvBgRAM,			0x08c000, 0x08cfff, MAP_RAM);
		SekMapMemory(DrvFgRAM,			0x08d000, 0x08d7ff, MAP_RAM);
		SekMapMemory(DrvTxRAM,			0x08d800, 0x08ffff, MAP_RAM);
		SekMapMemory(Drv68KRAM  + 0xb000,       0x0c1000, 0x0c17ff, MAP_RAM);
		SekMapMemory(Drv68KRAM1 + 0x0000,	0x120000, 0x127fff, MAP_RAM);
		SekMapMemory(DrvPalRAM,			0x128000, 0x1287ff, MAP_RAM);
		SekMapMemory(Drv68KRAM1 + 0x8800,	0x128800, 0x128fff, MAP_RAM);
		SekClose();

		nGameSelect = 2;
	}
	else
	{
		{
			if (BurnLoadRom(Drv68KROM + 0x000000, 0, 2)) return 1;
			if (BurnLoadRom(Drv68KROM + 0x000001, 1, 2)) return 1;
			if (BurnLoadRom(Drv68KROM + 0x040000, 2, 2)) return 1;
			if (BurnLoadRom(Drv68KROM + 0x040001, 3, 2)) return 1;

			if (BurnLoadRom(DrvZ80ROM + 0x000000, 4, 1)) return 1;
			memcpy (DrvZ80ROM + 0x10000, DrvZ80ROM + 0x8000, 0x8000);
			memcpy (DrvZ80ROM + 0x18000, DrvZ80ROM + 0x8000, 0x8000);

			if (BurnLoadRom(DrvGfxROM0 + 0x00000, 5, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x10000, 6, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM1 + 0x00000, 7, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM2 + 0x00000, 8, 1)) return 1;

			if (BurnLoadRom(DrvSndROM + 0x000000, 9, 1)) return 1;

			DrvGfxDecode();
		}

		nGameSelect = 0;
		if (!strcmp(BurnDrvGetTextA(DRV_NAME), "skysmash")) nGameSelect = 1;

		SekInit(0, 0x68000);
		SekOpen(0);
		SekMapMemory(Drv68KROM,			0x000000, 0x07ffff, MAP_ROM);
		SekMapMemory(Drv68KRAM,			0x080000, 0x08afff, MAP_RAM);
		SekMapMemory(DrvSprRAM,			0x08b000, 0x08bfff, MAP_RAM);
		SekMapMemory(DrvBgRAM,			0x08c000, 0x08cfff, MAP_RAM);
		SekMapMemory(DrvFgRAM,			0x08d000, 0x08d7ff, MAP_RAM);
		SekMapMemory(DrvTxRAM,			0x08d800, 0x08e7ff, MAP_RAM);
		SekMapMemory(DrvPalRAM,			0x08e800, 0x08f7ff, MAP_RAM);
		SekMapMemory(Drv68KRAM + 0xb000,        0x08f800, 0x08ffff, MAP_RAM);
		SekSetWriteByteHandler(0,		bloodbro_write_byte);
		SekSetWriteWordHandler(0,		bloodbro_write_word);
		SekSetReadByteHandler(0,		bloodbro_read_byte);
		SekSetReadWordHandler(0,		bloodbro_read_word);
		SekClose();
	}

	seibu_sound_init(0, 0, 3579545, 3579545, 1000000/132);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, background_map_callback, 16, 16, 32, 16);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, foreground_map_callback, 16, 16, 32, 16);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, text_map_callback,        8,  8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0,            4,  8,  8,  8 *  8 * 0x1000, 0x700, 0xf);
	GenericTilemapSetGfx(1, DrvGfxROM1,            4, 16, 16, 16 * 16 * 0x1000, 0x400, 0xf);
	GenericTilemapSetGfx(2, DrvGfxROM1 + 0x100000, 4, 16, 16, 16 * 16 * 0x1000, 0x500, 0xf);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);
	GenericTilemapSetTransparent(1, 0xf);
	GenericTilemapSetTransparent(2, 0xf);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	SekExit();

	seibu_sound_exit();

	BurnFree (AllMem);

	nGameSelect = 0;

	return 0;
}

static inline void DrvRecalcPalette()
{
	UINT8 r,g,b;
	UINT16 *p = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x1000/2; i++)
	{
		INT32 data = BURN_ENDIAN_SWAP_INT16(p[i]);

		r = (data >> 0) & 0x0f;
		g = (data >> 4) & 0x0f;
		b = (data >> 8) & 0x0f;

		r |= r << 4;
		g |= g << 4;
		b |= b << 4;

		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}
}

static void draw_sprites()
{
	UINT16 *ram = (UINT16*)DrvSprRAM;

	for (INT32 offs = 0; offs < 0x800; offs += 4)
	{
		INT32 attr = BURN_ENDIAN_SWAP_INT16(ram[offs]);
		INT32 pri_mask = (attr & 0x0800) ? 2 : 0;

		if (attr & 0x8000) continue;

		INT32 width   = (attr >> 7) & 7;
		INT32 height  = (attr >> 4) & 7;
		INT32 code    = BURN_ENDIAN_SWAP_INT16(ram[offs+1]) & 0x1fff;
		INT32 sx      = BURN_ENDIAN_SWAP_INT16(ram[offs+2]) & 0x01ff;
		INT32 sy      = BURN_ENDIAN_SWAP_INT16(ram[offs+3]) & 0x01ff;
		if (sx > 255) sx -= 512;
		if (sy > 255) sy -= 512;
		sy -= 16;

		INT32 flipx = attr & 0x2000;
		INT32 flipy = attr & 0x4000;
		INT32 color = attr & 0x000f;

		for (INT32 x = 0; x <= width; x++)
		{
			for (INT32 y = 0; y <= height; y++)
			{
				INT32 syy = sy + 16 * ((flipy) ? (height - y) : y);
				INT32 sxx = sx + 16 * ((flipx) ? (width - x) : x);

				RenderPrioSprite(pTransDraw, DrvGfxROM2, (code++) & 0x1fff, (color << 4), 15, sxx, syy, flipx, flipy, 16, 16, pri_mask);
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvRecalcPalette();
	}

	UINT16 *scroll = (UINT16*)DrvScrollRAM;
	scroll += 0x10 >> (nGameSelect & 1); // skysmash

	BurnTransferClear();

	GenericTilemapSetScrollX(0, BURN_ENDIAN_SWAP_INT16(scroll[0]));
	GenericTilemapSetScrollY(0, BURN_ENDIAN_SWAP_INT16(scroll[1]));
	GenericTilemapSetScrollX(1, BURN_ENDIAN_SWAP_INT16(scroll[2]));
	GenericTilemapSetScrollY(1, BURN_ENDIAN_SWAP_INT16(scroll[3]));

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);
	if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 1);
	if (nBurnLayer & 4) draw_sprites();
	if (nBurnLayer & 8) GenericTilemapDraw(2, pTransDraw, 0);

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
		memset (DrvInputs, 0xff, 3 * sizeof(UINT16));
		for (INT32 i = 0; i < 16; i++)
		{
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		if (nGameSelect == 2) {
			UINT16 *ram = (UINT16*)(Drv68KRAM + 0xb000);
			ram[0] = (DrvDips[1] << 8) | (DrvDips[0]);
			ram[1] = DrvInputs[1];
			ram[2] = DrvInputs[2];
		}

		seibu_coin_input = (DrvJoy1[1] << 1) | DrvJoy1[0];
	}

	INT32 nInterleave = 1000;
	INT32 nCyclesTotal[2] = { 10000000 / 60, 3579545 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);

		CPU_RUN_TIMER(1);
	}

	if (nGameSelect == 0) SekSetIRQLine(4, CPU_IRQSTATUS_AUTO); // bloodbro
	if (nGameSelect == 1) SekSetIRQLine(2, CPU_IRQSTATUS_AUTO); // skysmash
	if (nGameSelect == 2) SekSetIRQLine(6, CPU_IRQSTATUS_AUTO); // weststry

	ZetClose();
	SekClose();

	if (pBurnSoundOut) {
		seibu_sound_update(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin != NULL) {
		*pnMin = 0x029706;
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

		seibu_sound_scan(nAction, pnMin);
	}

	return 0;
}


// Blood Bros. (World?)

static struct BurnRomInfo bloodbroRomDesc[] = {
	{ "2.u021.7n",		0x020000, 0x204dca6e, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "1.u022.8n",		0x020000, 0xac6719e7, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "4.u023.7l",		0x020000, 0xfd951c2c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "3.u024.8l",		0x020000, 0x18d3c460, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "bb_07.u1016.6a",	0x010000, 0x411b94e8, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "bb_05.u061.6f",	0x010000, 0x04ba6d19, 3 | BRF_GRA },           //  5 Characters
	{ "bb_06.u063.6d",	0x010000, 0x7092e35b, 3 | BRF_GRA },           //  6

	{ "blood_bros_bk__=c=1990_tad_corp.u064.4d",	0x100000, 0x1aa87ee6, 4 | BRF_GRA },           //  7 Tiles

	{ "blood_bros_obj__=c=1990_tad_corp.u078.2n",	0x100000, 0xd27c3952, 5 | BRF_GRA },           //  8 Sprites

	{ "bb_08.u095.5a",	0x020000, 0xdeb1b975, 6 | BRF_SND },           //  9 Samples
	
	{ "cb006.u083.6c",	0x000100, 0xb2b89a74, 0 | BRF_OPT },
};

STD_ROM_PICK(bloodbro)
STD_ROM_FN(bloodbro)

struct BurnDriver BurnDrvBloodbro = {
	"bloodbro", NULL, NULL, NULL, "1990",
	"Blood Bros. (World?)\0", NULL, "TAD Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, bloodbroRomInfo, bloodbroRomName, NULL, NULL, NULL, NULL, BloodbroInputInfo, BloodbroDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	256, 224, 4, 3
};


// Blood Bros. (Japan, rev A)

static struct BurnRomInfo bloodbrojRomDesc[] = {
	{ "2j_a.u021.7n",	0x020000, 0xe8ca21b8, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "1j_a.u022.8n",	0x020000, 0x6b28cfc7, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "4.u023.7l",		0x020000, 0xfd951c2c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "3.u024.8l",		0x020000, 0x18d3c460, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "bb_07.u1016.6a",	0x010000, 0x411b94e8, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "bb_05.u061.6f",	0x010000, 0x04ba6d19, 3 | BRF_GRA },           //  5 Characters
	{ "bb_06.u063.6d",	0x010000, 0x7092e35b, 3 | BRF_GRA },           //  6

	{ "blood_bros_bk__=c=1990_tad_corp.u064.4d",	0x100000, 0x1aa87ee6, 4 | BRF_GRA },           //  7 Tiles

	{ "blood_bros_obj__=c=1990_tad_corp.u078.2n",	0x100000, 0xd27c3952, 5 | BRF_GRA },           //  8 Sprites

	{ "bb_08.u095.5a",	0x020000, 0xdeb1b975, 6 | BRF_SND },           //  9 Samples
	
	{ "cb006.u083.6c",	0x000100, 0xb2b89a74, 0 | BRF_OPT },
};

STD_ROM_PICK(bloodbroj)
STD_ROM_FN(bloodbroj)

struct BurnDriver BurnDrvBloodbroj = {
	"bloodbroj", "bloodbro", NULL, NULL, "1990",
	"Blood Bros. (Japan, rev A)\0", NULL, "TAD Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, bloodbrojRomInfo, bloodbrojRomName, NULL, NULL, NULL, NULL, BloodbroInputInfo, BloodbroDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	256, 224, 4, 3
};


// Blood Bros. (Japan)

static struct BurnRomInfo bloodbrojaRomDesc[] = {
	{ "2j.u021.7n",		0x020000, 0xc0fdc3e4, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "1j.u022.8n",		0x020000, 0x2d7e0fdf, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "4.u023.7l",		0x020000, 0xfd951c2c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "3.u024.8l",		0x020000, 0x18d3c460, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "bb_07.u1016.6a",	0x010000, 0x411b94e8, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "bb_05.u061.6f",	0x010000, 0x04ba6d19, 3 | BRF_GRA },           //  5 Characters
	{ "bb_06.u063.6d",	0x010000, 0x7092e35b, 3 | BRF_GRA },           //  6

	{ "blood_bros_bk__=c=1990_tad_corp.u064.4d",	0x100000, 0x1aa87ee6, 4 | BRF_GRA },           //  7 Tiles

	{ "blood_bros_obj__=c=1990_tad_corp.u078.2n",	0x100000, 0xd27c3952, 5 | BRF_GRA },           //  8 Sprites

	{ "bb_08.u095.5a",	0x020000, 0xdeb1b975, 6 | BRF_SND },           //  9 Samples
	
	{ "cb006.u083.6c",	0x000100, 0xb2b89a74, 0 | BRF_OPT },
};

STD_ROM_PICK(bloodbroja)
STD_ROM_FN(bloodbroja)

struct BurnDriver BurnDrvBloodbroja = {
	"bloodbroja", "bloodbro", NULL, NULL, "1990",
	"Blood Bros. (Japan)\0", NULL, "TAD Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, bloodbrojaRomInfo, bloodbrojaRomName, NULL, NULL, NULL, NULL, BloodbroInputInfo, BloodbroDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	256, 224, 4, 3
};


// Blood Bros. (US)

static struct BurnRomInfo bloodbrouRomDesc[] = {
	{ "2u.u021.7n",		0x020000, 0x45186bd3, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "1u.u022.8n",		0x020000, 0xf2cb95c1, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "4.u023.7l",		0x020000, 0xfd951c2c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "3.u024.8l",		0x020000, 0x18d3c460, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "bb_07.u1016.6a",	0x010000, 0x411b94e8, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "bb_05.u061.6f",	0x010000, 0x04ba6d19, 3 | BRF_GRA },           //  5 Characters
	{ "bb_06.u063.6d",	0x010000, 0x7092e35b, 3 | BRF_GRA },           //  6

	{ "blood_bros_bk__=c=1990_tad_corp.u064.4d",	0x100000, 0x1aa87ee6, 4 | BRF_GRA },           //  7 Tiles

	{ "blood_bros_obj__=c=1990_tad_corp.u078.2n",	0x100000, 0xd27c3952, 5 | BRF_GRA },           //  8 Sprites

	{ "bb_08.u095.5a",	0x020000, 0xdeb1b975, 6 | BRF_SND },           //  9 Samples
	
	{ "cb006.u083.6c",	0x000100, 0xb2b89a74, 0 | BRF_OPT },
};

STD_ROM_PICK(bloodbrou)
STD_ROM_FN(bloodbrou)

struct BurnDriver BurnDrvBloodbrou = {
	"bloodbrou", "bloodbro", NULL, NULL, "1990",
	"Blood Bros. (US)\0", NULL, "TAD Corporation (Fabtek license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, bloodbrouRomInfo, bloodbrouRomName, NULL, NULL, NULL, NULL, BloodbroInputInfo, BloodbroDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	256, 224, 4, 3
};


// Blood Bros. (Korea)

static struct BurnRomInfo bloodbrokRomDesc[] = {
	{ "2k.u021.7n",		0x020000, 0xd65f5e1f, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "1k.u022.8n",		0x020000, 0x3d4b48fd, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "4.u023.7l",		0x020000, 0xfd951c2c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "3.u024.8l",		0x020000, 0x18d3c460, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "u1016.6a",		0x010000, 0x4f462b4f, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "bb_05.u061.6f",	0x010000, 0x04ba6d19, 3 | BRF_GRA },           //  5 Characters
	{ "bb_06.u063.6d",	0x010000, 0x7092e35b, 3 | BRF_GRA },           //  6

	{ "blood_bros_bk__=c=1990_tad_corp.u064.4d",	0x100000, 0x1aa87ee6, 4 | BRF_GRA },           //  7 Tiles

	{ "blood_bros_obj__=c=1990_tad_corp.u078.2n",	0x100000, 0xd27c3952, 5 | BRF_GRA },           //  8 Sprites

	{ "bb_08.u095.5a",	0x020000, 0xdeb1b975, 6 | BRF_SND },           //  9 Samples
	
	{ "cb006.u083.6c",	0x000100, 0xb2b89a74, 0 | BRF_OPT },
};

STD_ROM_PICK(bloodbrok)
STD_ROM_FN(bloodbrok)

struct BurnDriver BurnDrvBloodbrok = {
	"bloodbrok", "bloodbro", NULL, NULL, "1990",
	"Blood Bros. (Korea)\0", NULL, "TAD Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, bloodbrokRomInfo, bloodbrokRomName, NULL, NULL, NULL, NULL, BloodbroInputInfo, BloodbroDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	256, 224, 4, 3
};


// West Story (bootleg of Blood Bros., set 1)

static struct BurnRomInfo weststryRomDesc[] = {
	{ "ws13.bin",	0x20000, 0x158e302a, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "ws15.bin",	0x20000, 0x672e9027, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ws14.bin",	0x20000, 0xfd951c2c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ws16.bin",	0x20000, 0x18d3c460, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "ws17.bin",	0x10000, 0xe00a8f09, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "ws09.bin",	0x10000, 0xf05b2b3e, 3 | BRF_GRA },           //  5 Characters
	{ "ws11.bin",	0x10000, 0x2b10e3d2, 3 | BRF_GRA },           //  6
	{ "ws10.bin",	0x10000, 0xefdf7c82, 3 | BRF_GRA },           //  7
	{ "ws12.bin",	0x10000, 0xaf993578, 3 | BRF_GRA },           //  8

	{ "ws05.bin",	0x20000, 0x007c8dc0, 4 | BRF_GRA },           //  9 Tiles
	{ "ws07.bin",	0x20000, 0x0f0c8d9a, 4 | BRF_GRA },           // 10
	{ "ws06.bin",	0x20000, 0x459d075e, 4 | BRF_GRA },           // 11
	{ "ws08.bin",	0x20000, 0x4d6783b3, 4 | BRF_GRA },           // 12
	{ "ws01.bin",	0x20000, 0x32bda4bc, 4 | BRF_GRA },           // 13
	{ "ws03.bin",	0x20000, 0x046b51f8, 4 | BRF_GRA },           // 14
	{ "ws02.bin",	0x20000, 0xed9d682e, 4 | BRF_GRA },           // 15
	{ "ws04.bin",	0x20000, 0x75f082e5, 4 | BRF_GRA },           // 16

	{ "ws25.bin",	0x20000, 0x8092e8e9, 5 | BRF_GRA },           // 17 Sprites
	{ "ws26.bin",	0x20000, 0xf6a1f42c, 5 | BRF_GRA },           // 18
	{ "ws23.bin",	0x20000, 0x43d58e24, 5 | BRF_GRA },           // 19
	{ "ws24.bin",	0x20000, 0x20a867ea, 5 | BRF_GRA },           // 20
	{ "ws21.bin",	0x20000, 0x5ef55779, 5 | BRF_GRA },           // 21
	{ "ws22.bin",	0x20000, 0x7150a060, 5 | BRF_GRA },           // 22
	{ "ws19.bin",	0x20000, 0xc5dd0a96, 5 | BRF_GRA },           // 23
	{ "ws20.bin",	0x20000, 0xf1245c16, 5 | BRF_GRA },           // 24

	{ "ws18.bin",	0x20000, 0xdeb1b975, 6 | BRF_SND },           // 25 Samples
};

STD_ROM_PICK(weststry)
STD_ROM_FN(weststry)

struct BurnDriverD BurnDrvWeststry = {
	"weststry", "bloodbro", NULL, NULL, "1990",
	"West Story (bootleg of Blood Bros., set 1)\0", NULL, "bootleg (Datsu)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, weststryRomInfo, weststryRomName, NULL, NULL, NULL, NULL, WeststryInputInfo, WeststryDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	256, 224, 4, 3
};


// West Story (bootleg of Blood Bros., set 2)

static struct BurnRomInfo weststryaRomDesc[] = {
	{ "13.bin",		0x20000, 0xd50e1dfd, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "15.bin",		0x20000, 0xfd419c7b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ws14.bin",	0x20000, 0xfd951c2c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ws16.bin",	0x20000, 0x18d3c460, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "ws17.bin",	0x10000, 0xe00a8f09, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "ws09.bin",	0x10000, 0xf05b2b3e, 3 | BRF_GRA },           //  5 Characters
	{ "ws11.bin",	0x10000, 0x2b10e3d2, 3 | BRF_GRA },           //  6
	{ "ws10.bin",	0x10000, 0xefdf7c82, 3 | BRF_GRA },           //  7
	{ "ws12.bin",	0x10000, 0xaf993578, 3 | BRF_GRA },           //  8

	{ "ws05.bin",	0x20000, 0x007c8dc0, 4 | BRF_GRA },           //  9 Tiles
	{ "ws07.bin",	0x20000, 0x0f0c8d9a, 4 | BRF_GRA },           // 10
	{ "ws06.bin",	0x20000, 0x459d075e, 4 | BRF_GRA },           // 11
	{ "ws08.bin",	0x20000, 0x4d6783b3, 4 | BRF_GRA },           // 12
	{ "ws01.bin",	0x20000, 0x32bda4bc, 4 | BRF_GRA },           // 13
	{ "ws03.bin",	0x20000, 0x046b51f8, 4 | BRF_GRA },           // 14
	{ "ws02.bin",	0x20000, 0xed9d682e, 4 | BRF_GRA },           // 15
	{ "ws04.bin",	0x20000, 0x75f082e5, 4 | BRF_GRA },           // 16

	{ "ws25.bin",	0x20000, 0x8092e8e9, 5 | BRF_GRA },           // 17 Sprites
	{ "ws26.bin",	0x20000, 0xf6a1f42c, 5 | BRF_GRA },           // 18
	{ "ws23.bin",	0x20000, 0x43d58e24, 5 | BRF_GRA },           // 19
	{ "ws24.bin",	0x20000, 0x20a867ea, 5 | BRF_GRA },           // 20
	{ "ws21.bin",	0x20000, 0x5ef55779, 5 | BRF_GRA },           // 21
	{ "ws22.bin",	0x20000, 0x7150a060, 5 | BRF_GRA },           // 22
	{ "ws19.bin",	0x20000, 0xc5dd0a96, 5 | BRF_GRA },           // 23
	{ "ws20.bin",	0x20000, 0xf1245c16, 5 | BRF_GRA },           // 24

	{ "ws18.bin",	0x20000, 0xdeb1b975, 6 | BRF_SND },           // 25 Samples
};

STD_ROM_PICK(weststrya)
STD_ROM_FN(weststrya)

struct BurnDriverD BurnDrvWeststrya = {
	"weststrya", "bloodbro", NULL, NULL, "1990",
	"West Story (bootleg of Blood Bros., set 2)\0", NULL, "bootleg (Datsu)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, weststryaRomInfo, weststryaRomName, NULL, NULL, NULL, NULL, WeststryInputInfo, WeststryDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	256, 224, 4, 3
};


// Sky Smasher

static struct BurnRomInfo skysmashRomDesc[] = {
	{ "rom6",	0x020000, 0xe9c1d308, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "rom5",	0x020000, 0x867f9897, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rom8",	0x020000, 0xd3646728, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rom7",	0x020000, 0xd209db4d, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "rom2",	0x010000, 0x75b194cf, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "rom3",	0x010000, 0xfbb241be, 3 | BRF_GRA },           //  5 Characters
	{ "rom4",	0x010000, 0xad3cde81, 3 | BRF_GRA },           //  6

	{ "rom9",	0x100000, 0xb0a5eecf, 4 | BRF_GRA },           //  7 Tiles

	{ "rom10",	0x080000, 0x1bbcda5d, 5 | BRF_GRA },           //  8 Sprites

	{ "rom1",	0x020000, 0xe69986f6, 6 | BRF_SND },           //  9 Samples
	
	{ "ss006.u083.4j",	0x00100, 0x8b2ae4a0, 0 | BRF_OPT },
};

STD_ROM_PICK(skysmash)
STD_ROM_FN(skysmash)

struct BurnDriver BurnDrvSkysmash = {
	"skysmash", NULL, NULL, NULL, "1990",
	"Sky Smasher\0", NULL, "Nihon System", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, skysmashRomInfo, skysmashRomName, NULL, NULL, NULL, NULL, SkysmashInputInfo, SkysmashDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	224, 256, 3, 4
};
