// FB Alpha Raiden II driver module
// Based on MAME driver Olivier Galibert, Angelo Salese, David Haywood, Tomasz Slanina

#include "tiles_generic.h"
#include "nec_intf.h"
#include "z80_intf.h"
#include "seibusnd.h"
#include "eeprom.h"
#include "bitswap.h"

#if defined _MSC_VER
 #define _USE_MATH_DEFINES
 #include <cmath>
#endif

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvMainROM;
static UINT8 *DrvCopxROM;
static UINT8 *DrvEeprom;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvSndROM0;
static UINT8 *DrvSndROM1;
static UINT8 *DrvMainRAM;
static UINT8 *DrvTxRAM;
static UINT8 *DrvBgRAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvMgRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvTransTab;

static UINT32 *bitmap32;
static UINT8 *DrvAlphaTable;

static UINT8 *scroll;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT16 layer_enable;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvJoy4[4];
static UINT8 DrvDips[4];
static UINT16 DrvInputs[3];
static UINT8 DrvReset;
static INT32 hold_coin[4];

static UINT16 prg_bank = 0;
static UINT8 mg_bank = 0;
static UINT8 bg_bank = 0;
static UINT8 fg_bank = 0;
static UINT8 tx_bank = 0;

static INT32 game_select = 0; // 0 raiden2, 1 raidendx, 2 zeroteam, 3 xsedae, 4 = r2dx, 5 nzeroteam, 6 zerotm2k

static struct BurnInputInfo Raiden2InputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 3,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Raiden2)

static struct BurnInputInfo RaidendxInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 3,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Raidendx)

static struct BurnInputInfo ZeroteamInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 3"	},

	{"P3 Coin",		BIT_DIGITAL,	DrvJoy4 + 2,	"p3 coin"	},
	{"P3 Start",		BIT_DIGITAL,	DrvJoy3 + 4,	"p3 start"	},
	{"P3 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p3 up"		},
	{"P3 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p3 down"	},
	{"P3 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p3 fire 2"	},
	{"P3 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p3 fire 3"	},

	{"P4 Coin",		BIT_DIGITAL,	DrvJoy4 + 3,	"p4 coin"	},
	{"P4 Start",		BIT_DIGITAL,	DrvJoy3 + 5,	"p4 start"	},
	{"P4 Up",		BIT_DIGITAL,	DrvJoy2 + 8,	"p4 up"		},
	{"P4 Down",		BIT_DIGITAL,	DrvJoy2 + 9,	"p4 down"	},
	{"P4 Left",		BIT_DIGITAL,	DrvJoy2 + 10,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy2 + 11,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy2 + 12,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy2 + 13,	"p4 fire 2"	},
	{"P4 Button 3",		BIT_DIGITAL,	DrvJoy2 + 14,	"p4 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 3,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Zeroteam)

static struct BurnInputInfo Rdx_v33InputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 15,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy2 + 2,	"service"	},
	{"Service 1",	BIT_DIGITAL,	DrvJoy2 + 3,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Rdx_v33)

static struct BurnInputInfo NzeroteaInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy2 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Nzerotea)

static struct BurnDIPInfo Raiden2DIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x12, 0x01, 0x07, 0x01, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x02, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x04, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x07, 0x03, "1 Coin  4 Credits"	},
	{0x12, 0x01, 0x07, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x12, 0x01, 0x38, 0x08, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0x38, 0x10, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x38, 0x20, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x38, 0x30, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x38, 0x28, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x38, 0x18, "1 Coin  4 Credits"	},
	{0x12, 0x01, 0x38, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Starting Coin"	},
	{0x12, 0x01, 0x40, 0x40, "Normal"		},
	{0x12, 0x01, 0x40, 0x00, "X 2"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x12, 0x01, 0x80, 0x80, "Off"			},
	{0x12, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x03, 0x03, "Normal"		},
	{0x13, 0x01, 0x03, 0x02, "Easy"			},
	{0x13, 0x01, 0x03, 0x01, "Hard"			},
	{0x13, 0x01, 0x03, 0x00, "Very Hard"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0x0c, 0x00, "1"			},
	{0x13, 0x01, 0x0c, 0x04, "4"			},
	{0x13, 0x01, 0x0c, 0x08, "2"			},
	{0x13, 0x01, 0x0c, 0x0c, "3"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x13, 0x01, 0x30, 0x30, "200000 500000"	},
	{0x13, 0x01, 0x30, 0x20, "400000 1000000"	},
	{0x13, 0x01, 0x30, 0x10, "1000000 3000000"	},
	{0x13, 0x01, 0x30, 0x00, "No Extend"		},

	{0   , 0xfe, 0   ,    2, "Demo Sound"		},
	{0x13, 0x01, 0x40, 0x00, "Off"			},
	{0x13, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x13, 0x01, 0x80, 0x80, "Off"			},
	{0x13, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Raiden2)

static struct BurnDIPInfo RaidendxDIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL			},
	{0x15, 0xff, 0xff, 0xff, NULL			},
	{0x16, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x14, 0x01, 0x07, 0x01, "4 Coins 1 Credits"	},
	{0x14, 0x01, 0x07, 0x02, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0x07, 0x04, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x14, 0x01, 0x07, 0x03, "1 Coin  4 Credits"	},
	{0x14, 0x01, 0x07, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x14, 0x01, 0x38, 0x08, "4 Coins 1 Credits"	},
	{0x14, 0x01, 0x38, 0x10, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0x38, 0x20, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x38, 0x30, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0x38, 0x28, "1 Coin  3 Credits"	},
	{0x14, 0x01, 0x38, 0x18, "1 Coin  4 Credits"	},
	{0x14, 0x01, 0x38, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Starting Coin"	},
	{0x14, 0x01, 0x40, 0x40, "Normal"		},
	{0x14, 0x01, 0x40, 0x00, "X 2"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x15, 0x01, 0x03, 0x03, "Normal"		},
	{0x15, 0x01, 0x03, 0x02, "Easy"			},
	{0x15, 0x01, 0x03, 0x01, "Hard"			},
	{0x15, 0x01, 0x03, 0x00, "Very Hard"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x15, 0x01, 0x0c, 0x00, "1"			},
	{0x15, 0x01, 0x0c, 0x04, "4"			},
	{0x15, 0x01, 0x0c, 0x08, "2"			},
	{0x15, 0x01, 0x0c, 0x0c, "3"			},

	{0   , 0xfe, 0   ,    2, "Demo Sound"		},
	{0x15, 0x01, 0x40, 0x00, "Off"			},
	{0x15, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x15, 0x01, 0x80, 0x80, "Off"			},
	{0x15, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Raidendx)

static struct BurnDIPInfo XsedaeDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},
};

STDDIPINFO(Xsedae)

static struct BurnDIPInfo ZeroteamDIPList[]=
{
	{0x26, 0xff, 0xff, 0xff, NULL			},
	{0x27, 0xff, 0xff, 0xff, NULL			},
	{0x28, 0xff, 0xff, 0xfd, NULL			},

	{0x26, 0xff, 0xff, 0xff, NULL			},
	{0x27, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x26, 0x01, 0x07, 0x00, "4 Coins 1 Credits"	},
	{0x26, 0x01, 0x07, 0x01, "3 Coins 1 Credits"	},
	{0x26, 0x01, 0x07, 0x02, "2 Coins 1 Credits"	},
	{0x26, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x26, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x26, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x26, 0x01, 0x07, 0x03, "1 Coin  4 Credits"	},
	{0x26, 0x01, 0x07, 0x04, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x26, 0x01, 0x38, 0x00, "4 Coins 1 Credits"	},
	{0x26, 0x01, 0x38, 0x08, "3 Coins 1 Credits"	},
	{0x26, 0x01, 0x38, 0x10, "2 Coins 1 Credits"	},
	{0x26, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},
	{0x26, 0x01, 0x38, 0x30, "1 Coin  2 Credits"	},
	{0x26, 0x01, 0x38, 0x28, "1 Coin  3 Credits"	},
	{0x26, 0x01, 0x38, 0x18, "1 Coin  4 Credits"	},
	{0x26, 0x01, 0x38, 0x20, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Starting Coin"	},
	{0x26, 0x01, 0x40, 0x40, "Normal"		},
	{0x26, 0x01, 0x40, 0x00, "X 2"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x26, 0x01, 0x80, 0x80, "Off"			},
	{0x26, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x27, 0x01, 0x03, 0x03, "Normal"		},
	{0x27, 0x01, 0x03, 0x02, "Hard"			},
	{0x27, 0x01, 0x03, 0x01, "Easy"			},
	{0x27, 0x01, 0x03, 0x00, "Very Hard"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x27, 0x01, 0x0c, 0x0c, "2"			},
	{0x27, 0x01, 0x0c, 0x08, "4"			},
	{0x27, 0x01, 0x0c, 0x04, "3"			},
	{0x27, 0x01, 0x0c, 0x00, "1"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x27, 0x01, 0x30, 0x30, "1000000"		},
	{0x27, 0x01, 0x30, 0x20, "2000000"		},
	{0x27, 0x01, 0x30, 0x10, "Every 1000000"	},
	{0x27, 0x01, 0x30, 0x00, "No Extend"		},

	{0   , 0xfe, 0   ,    2, "Demo Sound"		},
	{0x27, 0x01, 0x40, 0x00, "Off"			},
	{0x27, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x27, 0x01, 0x80, 0x80, "Off"			},
	{0x27, 0x01, 0x80, 0x00, "On"			},


	{0   , 0xfe, 0   ,    8, "Cabinet Setting"	},
	{0x28, 0x01, 0x07, 0x07, "2 Players"		},
	{0x28, 0x01, 0x07, 0x06, "3 Players, 3 Slots"	},
	{0x28, 0x01, 0x07, 0x05, "4 Players, 4 Slots"	},
	{0x28, 0x01, 0x07, 0x04, "3 Players, 2 Slots"	},
	{0x28, 0x01, 0x07, 0x03, "2 Players x2"		},
	{0x28, 0x01, 0x07, 0x02, "4 Players, 2 Slots"	},
	{0x28, 0x01, 0x07, 0x01, "2 Players, Freeplay"	},
	{0x28, 0x01, 0x07, 0x00, "4 Players, Freeplay"	},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x28, 0x01, 0x08, 0x08, "Off"			},
	{0x28, 0x01, 0x08, 0x00, "On"			},
};

STDDIPINFO(Zeroteam)

static struct BurnDIPInfo Rdx_v33DIPList[]=
{
	{0x15, 0xff, 0xff, 0xc0, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x15, 0x01, 0x40, 0x40, "Off"			},
	{0x15, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x15, 0x01, 0x80, 0x80, "Off"			},
	{0x15, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Rdx_v33)

static struct BurnDIPInfo NzeroteaDIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL			},
	{0x15, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x14, 0x01, 0x07, 0x00, "4 Coins 1 Credits"	},
	{0x14, 0x01, 0x07, 0x01, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0x07, 0x02, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x14, 0x01, 0x07, 0x03, "1 Coin  4 Credits"	},
	{0x14, 0x01, 0x07, 0x04, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x14, 0x01, 0x38, 0x00, "4 Coins 1 Credits"	},
	{0x14, 0x01, 0x38, 0x08, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0x38, 0x10, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x38, 0x30, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0x38, 0x28, "1 Coin  3 Credits"	},
	{0x14, 0x01, 0x38, 0x18, "1 Coin  4 Credits"	},
	{0x14, 0x01, 0x38, 0x20, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Starting Coin"	},
	{0x14, 0x01, 0x40, 0x40, "Normal"		},
	{0x14, 0x01, 0x40, 0x00, "X 2"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x15, 0x01, 0x03, 0x03, "Normal"		},
	{0x15, 0x01, 0x03, 0x02, "Hard"			},
	{0x15, 0x01, 0x03, 0x01, "Easy"			},
	{0x15, 0x01, 0x03, 0x00, "Very Hard"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x15, 0x01, 0x0c, 0x0c, "2"			},
	{0x15, 0x01, 0x0c, 0x08, "4"			},
	{0x15, 0x01, 0x0c, 0x04, "3"			},
	{0x15, 0x01, 0x0c, 0x00, "1"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x15, 0x01, 0x30, 0x30, "1000000"		},
	{0x15, 0x01, 0x30, 0x20, "2000000"		},
	{0x15, 0x01, 0x30, 0x10, "Every 1000000"	},
	{0x15, 0x01, 0x30, 0x00, "No Extend"		},

	{0   , 0xfe, 0   ,    2, "Demo Sound"		},
	{0x15, 0x01, 0x40, 0x00, "Off"			},
	{0x15, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x15, 0x01, 0x80, 0x80, "Off"			},
	{0x15, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Nzerotea)

static UINT32 cop_regs[8], cop_itoa;
static UINT16 cop_status, cop_scale, cop_itoa_digit_count, cop_angle, cop_dist;
static UINT8 cop_itoa_digits[10];
static UINT16 cop_dma_mode, cop_dma_src[0x200], cop_dma_dst[0x200], cop_dma_size[0x200], cop_dma_v1, cop_dma_v2, cop_dma_adr_rel;
static UINT16 sprites_cur_start;
static UINT16 pal_brightness_val;

static UINT16 cop_angle_target;
static UINT16 cop_angle_step;

static UINT16 cop_bank;
static UINT16 sprite_prot_x,sprite_prot_y,dst1,cop_spr_maxx,cop_spr_off;
static UINT16 sprite_prot_src_addr[2];

static struct {
	INT16 pos[3];
	INT8 dx[3];
	UINT8 size[3];
	bool allow_swap;
	UINT16 flags_swap;
	UINT32 spradr;
	INT16 min[3], max[3];
} cop_collision_info[2];

static UINT16 cop_hit_status, cop_hit_baseadr;
INT16 cop_hit_val[3];
UINT16 cop_hit_val_stat;
static UINT32 cop_sort_ram_addr, cop_sort_lookup;
static UINT16 cop_sort_param;

static UINT16 r2dx_i_dx;
static UINT16 r2dx_i_dy;
static UINT16 r2dx_i_angle;
static UINT32 r2dx_i_sdist;
static INT32  r2dx_gameselect;
static INT32  r2dx_okibank;

static void SeibuCopReset()
{
	memset (cop_regs, 0, 8 * sizeof(UINT32));
	cop_itoa = 0;
	cop_status = 0;
	cop_scale = 0;
	cop_itoa_digit_count = 4;
	cop_angle_target = 0;
	cop_angle_step = 0;
	cop_angle = 0;
	cop_dist = 0;
	memset (cop_itoa_digits, 0, 10);

	cop_dma_mode = 0;
	memset (cop_dma_src, 0, 0x200 * sizeof(INT16));
	memset (cop_dma_dst, 0, 0x200 * sizeof(INT16));
	memset (cop_dma_size, 0, 0x200 * sizeof(INT16));
	cop_dma_v1 = 0;
	cop_dma_v2 = 0;
	cop_dma_adr_rel = 0;

	sprites_cur_start = 0;
	pal_brightness_val = 0;

	cop_bank = 0;

	sprite_prot_x = 0;
	sprite_prot_y = 0;
	dst1 = 0;
	cop_spr_maxx = 0;
	cop_spr_off = 0;
	memset (sprite_prot_src_addr, 0, 2 * sizeof(INT16));

	memset (&cop_collision_info, 0, 2 * sizeof(cop_collision_info[0]));

	cop_hit_status = 0;
	cop_hit_baseadr = 0;
	memset(&cop_hit_val, 0, sizeof(cop_hit_val));
	cop_hit_val_stat = 0;
	cop_sort_ram_addr = 0;
	cop_sort_lookup = 0;
	cop_sort_param = 0;

	r2dx_i_dx = 0;
	r2dx_i_dy = 0;
	r2dx_i_angle = 0;
	r2dx_i_sdist = 0;
	r2dx_gameselect = 0;
}

static void SeibuCopScan(INT32 nAction)
{
	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(cop_regs);
		SCAN_VAR(cop_itoa);
		SCAN_VAR(cop_status);
		SCAN_VAR(cop_scale);
		SCAN_VAR(cop_itoa_digit_count);
		SCAN_VAR(cop_angle_target);
		SCAN_VAR(cop_angle_step);
		SCAN_VAR(cop_angle);
		SCAN_VAR(cop_dist);
		SCAN_VAR(cop_itoa_digits);
		SCAN_VAR(cop_dma_mode);
		SCAN_VAR(cop_dma_src);
		SCAN_VAR(cop_dma_dst);
		SCAN_VAR(cop_dma_size);
		SCAN_VAR(cop_dma_v1);
		SCAN_VAR(cop_dma_v2);
		SCAN_VAR(cop_dma_adr_rel);
		SCAN_VAR(sprites_cur_start);
		SCAN_VAR(pal_brightness_val);
		SCAN_VAR(cop_bank);
		SCAN_VAR(sprite_prot_x);
		SCAN_VAR(sprite_prot_y);
		SCAN_VAR(dst1);
		SCAN_VAR(cop_spr_maxx);
		SCAN_VAR(cop_spr_off);
		SCAN_VAR(sprite_prot_src_addr);
		SCAN_VAR(cop_collision_info);
		SCAN_VAR(cop_hit_status);
		SCAN_VAR(cop_hit_baseadr);
		SCAN_VAR(cop_hit_val);
		SCAN_VAR(cop_hit_val_stat);
		SCAN_VAR(cop_sort_ram_addr);
		SCAN_VAR(cop_sort_lookup);
		SCAN_VAR(cop_sort_param);

		SCAN_VAR(r2dx_i_dx);
		SCAN_VAR(r2dx_i_dy);
		SCAN_VAR(r2dx_i_angle);
		SCAN_VAR(r2dx_i_sdist);
	}
}

static void itoa_compute()
{
	INT32 digits = 1 << cop_itoa_digit_count*2;
	UINT32 val = cop_itoa;

	if(digits > 9)
		digits = 9;
	for(INT32 i=0; i<digits; i++)
		if(!val && i)
			cop_itoa_digits[i] = 0x20;
		else {
			cop_itoa_digits[i] = 0x30 | (val % 10);
			val = val / 10;
		}
	cop_itoa_digits[9] = 0;
}

static void sprite_prot_src_write(UINT16 data)
{
	sprite_prot_src_addr[1] = data;
	UINT32 src = (sprite_prot_src_addr[0]<<4)+sprite_prot_src_addr[1];

	INT32 x = INT16((VezReadLong(src+0x08) >> 16) - (sprite_prot_x)) & 0xffff;
	INT32 y = INT16((VezReadLong(src+0x04) >> 16) - (sprite_prot_y)) & 0xffff;

	UINT16 head1 = VezReadWord(src+cop_spr_off);
	UINT16 head2 = VezReadWord(src+cop_spr_off+2);

	INT32 w = (((head1 >> 8 ) & 7) + 1) << 4;
	INT32 h = (((head1 >> 12) & 7) + 1) << 4;

	UINT16 flag = x-w/2 > -w && x-w/2 < cop_spr_maxx+w && y-h/2 > -h && y-h/2 < 256+h ? 1 : 0;
	
	flag = (VezReadWord(src) & 0xfffe) | flag;
	VezWriteWord(src, flag);

	if(flag & 1)
	{
		VezWriteWord(dst1,   head1);
		VezWriteWord(dst1+2, head2);
		VezWriteWord(dst1+4, x-w/2);
		VezWriteWord(dst1+6, y-h/2);

		dst1 += 8;
	}
}

static void cop_collision_read_pos(int slot, UINT32 spradr, bool allow_swap)
{
	cop_collision_info[slot].allow_swap = (allow_swap);
	cop_collision_info[slot].flags_swap = VezReadWord(spradr+2);
	cop_collision_info[slot].spradr = (spradr);
	
	for(int i=0; i<3; i++)
		cop_collision_info[slot].pos[i] = VezReadWord(spradr+6+4*i);
}

static void cop_collision_update_hitbox(INT16 slot, UINT32 hitadr, UINT16 data)
{
	UINT32 hitadr2 = VezReadWord(hitadr) | (cop_hit_baseadr << 16);
	INT32 num_axis = 2;
	INT32 extraxor = 0;
	if (/*m_cpu_is_68k*/1) extraxor = 1;

	// guess, heatbrl doesn't have this set and clearly only wants 2 axis to be checked (otherwise it reads bad params into the 3rd)
	// everything else has it set, and legionna clearly wants 3 axis for jumping attacks to work
	if (data & 0x0100) num_axis = 3;

	INT16 i;

	for(i = 0; i<3; i++) {
		cop_collision_info[slot].dx[i] = 0;
		cop_collision_info[slot].size[i] = 0;
	}
	
	for(i = 0; i<num_axis; i++) {
		cop_collision_info[slot].dx[i] = VezReadByte(hitadr2++);
		cop_collision_info[slot].size[i] = VezReadByte(hitadr2++);
	}

	cop_hit_status = 7;
	INT16 dx[3],size[3];

	for (i = 0; i < num_axis; i++)
	{
		size[i] = UINT8(cop_collision_info[slot].size[i]);
		dx[i] = INT8(cop_collision_info[slot].dx[i]);
	}

	INT16 j = slot;

	UINT8 res;

	if (num_axis==3) res = 7;
	else res = 3;

	for (i = 0; i < num_axis;i++)
	{
		if (cop_collision_info[j].allow_swap && (cop_collision_info[j].flags_swap & (1 << i)))
		{
			cop_collision_info[j].max[i] = (cop_collision_info[j].pos[i]) - dx[i];
			cop_collision_info[j].min[i] = cop_collision_info[j].max[i] - size[i];
		}
		else
		{
			cop_collision_info[j].min[i] = (cop_collision_info[j].pos[i]) + dx[i];
			cop_collision_info[j].max[i] = cop_collision_info[j].min[i] + size[i];
		}

		if(cop_collision_info[0].max[i] > cop_collision_info[1].min[i] && cop_collision_info[0].min[i] < cop_collision_info[1].max[i])
			res &= ~(1 << i);

		if(cop_collision_info[1].max[i] > cop_collision_info[0].min[i] && cop_collision_info[1].min[i] < cop_collision_info[0].max[i])
			res &= ~(1 << i);

		cop_hit_val[i] = (cop_collision_info[0].pos[i] - cop_collision_info[1].pos[i]);
	}

	cop_hit_val_stat = res; // TODO: there's also bit 2 and 3 triggered in the tests, no known meaning
	cop_hit_status = res;

}

static void cop_cmd_write(INT32 offset, UINT16 data)
{
	offset/=2;
	cop_status &= 0x7fff;

	switch(data) {
	case 0x0205: {  // 0205 0006 ffeb 0000 - 0188 0282 0082 0b8e 098e 0000 0000 0000
		INT32 ppos = VezReadLong(cop_regs[0] + 4 + offset*4);
		INT32 npos = ppos + VezReadLong(cop_regs[0] + 0x10 + offset*4);
		INT32 delta = (npos >> 16) - (ppos >> 16);
		VezWriteLong(cop_regs[0] + 4 + offset*4, npos);
		VezWriteWord(cop_regs[0] + 0x1e + offset*4, VezReadWord(cop_regs[0] + 0x1e + offset*4) + delta);
		break;
	}

	case 0x0904: { /* X Se Dae and Zero Team uses this variant */
		VezWriteLong(cop_regs[0] + 16 + offset*4, VezReadLong(cop_regs[0] + 16 + offset*4) - VezReadLong(cop_regs[0] + 0x28 + offset*4));
		break;
	}
	case 0x0905: //  0905 0006 fbfb 0008 - 0194 0288 0088 0000 0000 0000 0000 0000
		VezWriteLong(cop_regs[0] + 16 + offset*4, VezReadLong(cop_regs[0] + 16 + offset*4) + VezReadLong(cop_regs[0] + 0x28 + offset*4));
		break;

	case 0x130e:   // 130e 0005 bf7f 0010 - 0984 0aa4 0d82 0aa2 039b 0b9a 0b9a 0a9a
	case 0x138e:
	case 0x338e: { // 338e 0005 bf7f 0030 - 0984 0aa4 0d82 0aa2 039c 0b9c 0b9c 0a9a
		INT32 dx = VezReadLong(cop_regs[1]+4) - VezReadLong(cop_regs[0]+4);
		INT32 dy = VezReadLong(cop_regs[1]+8) - VezReadLong(cop_regs[0]+8);

		if(!dy) {
			cop_status |= 0x8000;
			cop_angle = 0;
		} else {
			cop_angle = (UINT16)(atan(double(dx)/double(dy)) * 128 / M_PI);
			if(dy<0)
				cop_angle += 0x80;
		}

		if(data & 0x0080) {
			VezWriteByte(cop_regs[0]+0x34, cop_angle);
		}
		break;
	}

	case 0x2208:
	case 0x2288: { // 2208 0005 f5df 0020 - 0f8a 0b8a 0388 0b9a 0b9a 0a9a 0000 0000
		INT32 dx = VezReadWord(cop_regs[0]+0x12);
		INT32 dy = VezReadWord(cop_regs[0]+0x16);

		if(!dy) {
			cop_status |= 0x8000;
			cop_angle = 0;
		} else {
			cop_angle = (UINT16)(atan(double(dx)/double(dy)) * 128 / M_PI);
			if(dy<0)
				cop_angle += 0x80;
		}

		if(data & 0x0080) {
			VezWriteByte(cop_regs[0]+0x34, cop_angle);
		}
		break;
	}

	case 0x2a05: { // 2a05 0006 ebeb 0028 - 09af 0a82 0082 0a8f 018e 0000 0000 0000
		INT32 delta = VezReadWord(cop_regs[1] + 0x1e + offset*4);
		VezWriteLong(cop_regs[0] + 4+2  + offset*4, VezReadWord(cop_regs[0] + 4+2  + offset*4) + delta);
		VezWriteLong(cop_regs[0] + 0x1e + offset*4, VezReadWord(cop_regs[0] + 0x1e + offset*4) + delta);
		break;
	}

	case 0x39b0:
	case 0x3b30:
	case 0x3bb0: { // 3bb0 0004 007f 0038 - 0f9c 0b9c 0b9c 0b9c 0b9c 0b9c 0b9c 099c
		/* TODO: these are actually internally loaded via 0x130e command */
		INT32 dx,dy;

		dx = VezReadLong(cop_regs[1]+4) - VezReadLong(cop_regs[0]+4);
		dy = VezReadLong(cop_regs[1]+8) - VezReadLong(cop_regs[0]+8);
		
		dx = dx >> 16;
		dy = dy >> 16;
		cop_dist = (UINT16)sqrt((double)(dx*dx+dy*dy));
		
		if(data & 0x0080)
			VezWriteWord(cop_regs[0]+(data & 0x200 ? 0x3a : 0x38), cop_dist);
		break;
	}

	case 0x42c2: { // 42c2 0005 fcdd 0040 - 0f9a 0b9a 0b9c 0b9c 0b9c 029c 0000 0000
		INT32 div = VezReadWord(cop_regs[0]+(0x36));
		if(!div)
			div = 1;

		/* TODO: bits 5-6-15 */
		cop_status = 7;

		VezWriteWord(cop_regs[0]+(0x38), (cop_dist << (5 - cop_scale)) / div);
		break;
	}

	case 0x4aa0: { // 4aa0 0005 fcdd 0048 - 0f9a 0b9a 0b9c 0b9c 0b9c 099b 0000 0000
		INT32 div = VezReadWord(cop_regs[0]+(0x38));
		if(!div)
			div = 1;

		/* TODO: bits 5-6-15 */
		cop_status = 7;

		VezWriteWord(cop_regs[0]+(0x36), (cop_dist << (5 - cop_scale)) / div);
		break;
	}

	case 0x6200: {
		UINT8 angle = VezReadByte(cop_regs[0]+0x34);
		UINT16 flags = VezReadWord(cop_regs[0]);
		cop_angle_target &= 0xff;
		cop_angle_step &= 0xff;
		flags &= ~0x0004;
		INT32 delta = angle - cop_angle_target;
		if(delta >= 128)
			delta -= 256;
		else if(delta < -128)
			delta += 256;
		if(delta < 0) {
			if(delta >= -cop_angle_step) {
				angle = cop_angle_target;
				flags |= 0x0004;
			} else
				angle += cop_angle_step;
		} else {
			if(delta <= cop_angle_step) {
				angle = cop_angle_target;
				flags |= 0x0004;
			} else
				angle -= cop_angle_step;
		}
		VezWriteWord(cop_regs[0], flags);
		VezWriteByte(cop_regs[0]+0x34, angle);
		break;
	}

	case 0x8100: { // 8100 0007 fdfb 0080 - 0b9a 0b88 0888 0000 0000 0000 0000 0000
		INT32 raw_angle = (VezReadWord(cop_regs[0]+(0x34)) & 0xff);
		double angle = raw_angle * M_PI / 128;
		double amp = (65536 >> 5)*(VezReadWord(cop_regs[0]+(0x36)) & 0xff);
		INT32 res;
		/* TODO: up direction, why? (check machine/seicop.c) */
		if(raw_angle == 0xc0)
			amp*=2;
		res = INT32(amp*sin(angle)) << cop_scale;
		VezWriteLong(cop_regs[0] + 16, res);
		break;
	}

	case 0x8900: { // 8900 0007 fdfb 0088 - 0b9a 0b8a 088a 0000 0000 0000 0000 0000
		INT32 raw_angle = (VezReadWord(cop_regs[0]+(0x34)) & 0xff);
		double angle = raw_angle * M_PI / 128;
		double amp = (65536 >> 5)*(VezReadWord(cop_regs[0]+(0x36)) & 0xff);
		INT32 res;
		/* TODO: left direction, why? (check machine/seicop.c) */
		if(raw_angle == 0x80)
			amp*=2;
		res = INT32(amp*cos(angle)) << cop_scale;
		VezWriteLong(cop_regs[0] + 20, res);
		break;
	}

	case 0x5205:   // 5205 0006 fff7 0050 - 0180 02e0 03a0 00a0 03a0 0000 0000 0000
	//      fprintf(stderr, "sprcpt 5205 %04x %04x %04x %08x %08x\n", cop_regs[0], cop_regs[1], cop_regs[3], VezReadLong(cop_regs[0]), VezReadLong(cop_regs[3]));
		VezWriteLong(cop_regs[1], VezReadLong(cop_regs[0]));
		break;

	case 0x5a05:   // 5a05 0006 fff7 0058 - 0180 02e0 03a0 00a0 03a0 0000 0000 0000
	//      fprintf(stderr, "sprcpt 5a05 %04x %04x %04x %08x %08x\n", cop_regs[0], cop_regs[1], cop_regs[3], VezReadLong(cop_regs[0]), VezReadLong(cop_regs[3]));
		VezWriteLong(cop_regs[1], VezReadLong(cop_regs[0]));
		break;

	case 0xf205:   // f205 0006 fff7 00f0 - 0182 02e0 03c0 00c0 03c0 0000 0000 0000
	//      fprintf(stderr, "sprcpt f205 %04x %04x %04x %08x %08x\n", cop_regs[0]+4, cop_regs[1], cop_regs[3], VezReadLong(cop_regs[0]+4), VezReadLong(cop_regs[3]));
		VezWriteLong(cop_regs[2], VezReadLong(cop_regs[0]+4));
		break;

		// raidendx only
	case 0x7e05:
		VezWriteByte(0x470, VezReadByte(cop_regs[4]));
		break;

	case 0xa100:
	case 0xa180:
		cop_collision_read_pos(0, cop_regs[0], data & 0x0080);
		break;

	case 0xa900:
	case 0xa980:
		cop_collision_read_pos(1, cop_regs[1], data & 0x0080);
		break;

	case 0xb100:
		cop_collision_update_hitbox(0, cop_regs[2], data);
		break;

	case 0xb900:
		cop_collision_update_hitbox(1, cop_regs[3], data);
		break;

	//default:
	//	logerror("pcall %04x (%04x:%04x) [%x %x %x %x]\n", data, rps(), rpc(), cop_regs[0], cop_regs[1], cop_regs[2], cop_regs[3]);
	}
}

static UINT8 fade_table(INT32 v)
{
	INT32 low  = v & 0x001f;
	INT32 high = v & 0x03e0;

	return (low * (high | (high >> 5)) + 0x210) >> 10;
}

static void cop_dma_trigger_write(UINT16)
{
	switch(cop_dma_mode) {
	case 0x14: {
		/* TODO: this transfers the whole VRAM, not only spriteram!
		   For whatever reason, this stopped working as soon as I've implemented DMA slot concept.
		   Raiden 2 uses this DMA with cop_dma_dst == 0xfffe, effectively changing the order of the uploaded VRAMs.
		   Also the size is used for doing a sprite limit trickery.
		*/
		static INT32 rsize = ((0x80 - cop_dma_size[cop_dma_mode]) & 0x7f) +1;

		sprites_cur_start = 0x1000 - (rsize << 5);
		#if 0
		INT32 rsize = 32*(0x7f-cop_dma_size);
		INT32 radr = 64*cop_dma_adr - rsize;
		for(INT32 i=0; i<rsize; i+=2)
			sprites[i/2] = VezReadWord(radr+i);
		sprites_cur_start = rsize;
		#endif
		break;
	}
	case 0x82: {
		UINT32 src,dst,size;
		UINT32 i;

		src = (cop_dma_src[cop_dma_mode] << 6);
		dst = (cop_dma_dst[cop_dma_mode] << 6);
		size = ((cop_dma_size[cop_dma_mode] << 5) - (cop_dma_dst[cop_dma_mode] << 6) + 0x20)/2;

	//	printf("%08x %08x %08x\n",src,dst,size);

		for(i = 0;i < size;i++)
		{
			UINT16 pal_val;
			INT32 r,g,b;
			INT32 rt,gt,bt;

			bt = (VezReadWord(src + (cop_dma_adr_rel * 0x400)) & 0x7c00) >> 5;
			bt = fade_table(bt|(pal_brightness_val ^ 0));
			b = ((VezReadWord(src)) & 0x7c00) >> 5;
			b = fade_table(b|(pal_brightness_val ^ 0x1f));
			pal_val = ((b + bt) & 0x1f) << 10;
			gt = (VezReadWord(src + (cop_dma_adr_rel * 0x400)) & 0x03e0);
			gt = fade_table(gt|(pal_brightness_val ^ 0));
			g = ((VezReadWord(src)) & 0x03e0);
			g = fade_table(g|(pal_brightness_val ^ 0x1f));
			pal_val |= ((g + gt) & 0x1f) << 5;
			rt = (VezReadWord(src + (cop_dma_adr_rel * 0x400)) & 0x001f) << 5;
			rt = fade_table(rt|(pal_brightness_val ^ 0));
			r = ((VezReadWord(src)) & 0x001f) << 5;
			r = fade_table(r|(pal_brightness_val ^ 0x1f));
			pal_val |= ((r + rt) & 0x1f);

			VezWriteWord(dst, pal_val);
			src+=2;
			dst+=2;
		}

		break;
	}
	case 0x09: {
		UINT32 src,dst,size;
		UINT32 i;

		src = (cop_dma_src[cop_dma_mode] << 6);
		dst = (cop_dma_dst[cop_dma_mode] << 6);
		size = ((cop_dma_size[cop_dma_mode] << 5) - (cop_dma_dst[cop_dma_mode] << 6) + 0x20)/2;

	//	printf("%08x %08x %08x\n",src,dst,size);

		for(i = 0;i < size;i++)
		{
			VezWriteWord(dst, VezReadWord(src));
			src+=2;
			dst+=2;
		}

		break;
	}
	case 0x118:
	case 0x11f: {
		UINT32 length, address;
		UINT32 i;
		if(cop_dma_dst[cop_dma_mode] != 0x0000) // Invalid?
			return;

		address = (cop_dma_src[cop_dma_mode] << 6);
		length = (cop_dma_size[cop_dma_mode]+1) << 5;

	//	printf("%08x %08x\n",address,length);

		for (i=address;i<address+length;i+=4)
		{
			VezWriteLong(i, (cop_dma_v1) | (cop_dma_v2 << 16));
		}
	}
	}
}

static void cop_sort_dma_trig_write(UINT16 data)
{
	UINT16 sort_size;

	sort_size = data;

	{
		INT32 i,j;
		UINT8 xchg_flag;
		UINT32 addri,addrj;
		UINT16 vali,valj;

		/* TODO: use a better algorithm than bubble sort! */
		for(i=2;i<sort_size;i+=2)
		{
			for(j=i-2;j<sort_size;j+=2)
			{
				addri = cop_sort_ram_addr+VezReadWord(cop_sort_lookup+i);
				addrj = cop_sort_ram_addr+VezReadWord(cop_sort_lookup+j);

				vali = VezReadWord(addri);
				valj = VezReadWord(addrj);

				switch(cop_sort_param)
				{
					case 2: xchg_flag = (vali > valj); break;
					case 1: xchg_flag = (vali < valj); break;
					default: xchg_flag = 0; break;
				}

				if(xchg_flag)
				{
					UINT16 xch_val;

					xch_val = VezReadWord(cop_sort_lookup+i);
					VezWriteWord(cop_sort_lookup+i,VezReadWord(cop_sort_lookup+j));
					VezWriteWord(cop_sort_lookup+j,xch_val);
				}
			}
		}
	}
}

static void raiden2_crtc_write(INT32 offset, UINT8 data)
{
	if ((offset & 0x7e) == 0x1c) {
		layer_enable = (layer_enable & (0xff << ((~offset & 1) * 8))) | (data << ((offset & 1) * 8));
		return;
	}

	if ((offset & 0x7f) >= 0x20 && (offset & 0x7f) <= 0x2b) {
		scroll[offset & 0x0f] = data;
		return;
	}
}

static void raiden2_bankswitch(INT32 bank)
{
	prg_bank = bank;
	bank = (~(bank >> 15) & 1) * 0x20000;

	VezMapArea(0x20000, 0x3ffff, 0, DrvMainROM + bank);
	VezMapArea(0x20000, 0x3ffff, 2, DrvMainROM + bank);
}

static void rd2_cop_write(UINT16 offset, UINT8 data)
{
	if (offset >= 0x600 && offset <= 0x64f) {
		raiden2_crtc_write(offset, data);
	}

	if ((offset & 0xfffe0) == 0x00700) {
		seibu_main_word_write((offset & 0x1f)/2, data);
		return; // sound comms
	}

	if ((offset & 1) == 0) return; // necessary??

	UINT16 *copram = (UINT16*)DrvMainRAM;
	UINT16 dataword = copram[(offset & 0x7fe)/2];

	switch (offset & 0x7fe)
	{
		case 0x41c:
			cop_angle_target = dataword;
		return;

		case 0x41e:
			cop_angle_step = dataword;
		return;

		case 0x420:
			cop_itoa = (cop_itoa & 0xffff0000) | dataword;
			itoa_compute();
		return;

		case 0x422:
			cop_itoa = (cop_itoa & 0xffff) | (dataword << 16);
		return;

		case 0x424:
			cop_itoa_digit_count = dataword;
		return;

		case 0x428:
			cop_dma_v1 = dataword;
		return;

		case 0x42a:
			cop_dma_v2 = dataword;
		return;
		
		case 0x436:
			cop_hit_baseadr = dataword;
		return;

		case 0x444:
			cop_scale = dataword & 3;
		return;

		case 0x450:
			cop_sort_ram_addr = (cop_sort_ram_addr & 0xffff) | (dataword<<16);
		return;

		case 0x452:
			cop_sort_ram_addr = (cop_sort_ram_addr & ~0xffff) | dataword;
		return;

		case 0x454:
			cop_sort_lookup = (cop_sort_lookup & 0xffff) | (dataword<<16);
		return;

		case 0x456:
			cop_sort_lookup = (cop_sort_lookup & ~0xffff) | dataword;
		return;

		case 0x458:
			cop_sort_param = dataword;
		return;

		case 0x45a:
			pal_brightness_val = dataword;
		return;

		case 0x45c: // palette dma brightness mode
		return;

		case 0x470:
			cop_bank = dataword;
			fg_bank = (cop_bank >> 14)|4;
		return;

		case 0x476:
			cop_dma_adr_rel = dataword;
		return;

		case 0x478:
			cop_dma_src[cop_dma_mode] = dataword;
		return;

		case 0x47a:
			cop_dma_size[cop_dma_mode] = dataword;
		return;

		case 0x47c:
			cop_dma_dst[cop_dma_mode] = dataword;
		return;

		case 0x47e:
			cop_dma_mode = dataword & 0x1ff; // ??
			if (dataword & 0xfe00) bprintf (0, _T("dma mode overflow: %4.4x\n"), dataword);
		return;

		case 0x4a0:
		case 0x4a2:
		case 0x4a4:
		case 0x4a6:
		case 0x4a8:
			cop_regs[(offset&0xf)/2] = (cop_regs[(offset&0xf)/2] & 0xffff) | (dataword<<16);
		return;

		case 0x4c0:
		case 0x4c2:
		case 0x4c4:
		case 0x4c6:
		case 0x4c8:
			cop_regs[(offset&0xf)/2] = (cop_regs[(offset&0xf)/2] & 0xffff0000) | dataword;
		return;

		case 0x500:
		case 0x502:
		case 0x504:
			cop_cmd_write(offset&0x7,dataword);
		return;

		case 0x6c0:
			cop_spr_off = dataword;
		return;

		case 0x6c2:
			sprite_prot_src_addr[0] = dataword;
		return;

		case 0x6c6:
			dst1 = dataword;
			copram[0x762/2] = dst1;
		return;

		case 0x6ca:
			raiden2_bankswitch(dataword);
		return;

		case 0x6cc:
			bg_bank = (dataword & 1) << 1;
			mg_bank = (dataword & 2) | 1;
		return;

		case 0x6d8:
			sprite_prot_x = dataword;
		return;

		case 0x6da:
			sprite_prot_y = dataword;
		return;

		case 0x6dc:
			cop_spr_maxx = dataword;
		return;

		case 0x6de:
			sprite_prot_src_write(dataword);
		return;

		case 0x6fc:
			cop_dma_trigger_write(dataword);
		return;

		case 0x6fe:
			cop_sort_dma_trig_write(dataword);
		return;
	}
}

static UINT8 rd2_cop_read(UINT16 offset)
{
	UINT16 *copram = (UINT16*)DrvMainRAM;
	UINT16 ret = copram[offset/2];

	if ((offset & 0xfffe0) == 0x00700) {
		return seibu_main_word_read((offset & 0x1f)/2);
	}

	switch (offset & 0x7fe)
	{
		case 0x580:
			ret = cop_hit_status;
		break;

/*		case 0x582:
			ret = cop_hit_val_y;
		break;

		case 0x584:
			ret = cop_hit_val_x;
		break;*/
		case 0x582:
		case 0x584:
		case 0x586: //bprintf(0, _T("offset[%X]"), offset);
			ret = cop_hit_val[(offset-0x582)/2];
		break;

		case 0x588:
			ret = cop_hit_val_stat;
		break;

		case 0x590:
		case 0x592:
		case 0x594:
		case 0x596:
		case 0x598:
			ret = cop_itoa_digits[(offset&0xe)] | (cop_itoa_digits[(offset&0x0e)+1] << 8);
		break;

		case 0x5b0:
			ret = cop_status;
		break;

		case 0x5b2:
			ret = cop_dist;
		break;

		case 0x5b4:
			ret = cop_angle;
		break;
		
		case 0x6c0:
			ret = cop_spr_off;
		break;
		
		case 0x6c2:
			ret = sprite_prot_src_addr[0];
		break;

		case 0x6dc:
			ret = cop_spr_maxx;
		break;
		
		case 0x762:
			ret = dst1;
		break;
	}

	return ret >> ((offset & 1) * 8);
}

static inline void palette_update_entry(INT32 entry)
{
	UINT16 p = *((UINT16*)(DrvPalRAM + entry));

	UINT8 r = (p >> 0) & 0x1f;
	UINT8 g = (p >> 5) & 0x1f;
	UINT8 b = (p >> 10) & 0x1f;

	r = (r << 3) | (r >> 2);
	g = (g << 3) | (g >> 2);
	b = (b << 3) | (b >> 2);

	DrvPalette[entry/2] = (r*0x10000)+(g*0x100)+b;
}

static void __fastcall raiden2_main_write(UINT32 address, UINT8 data)
{
	if ((address & 0xff000) == 0x1f000) {
		DrvPalRAM[address & 0xfff] = data;
		palette_update_entry(address & 0xffe);
		return;
	}

	if ((address & 0xffc00) == 0x00000) {
		DrvMainRAM[address] = data;
		return;
	}

	switch (address)
	{
		case 0x068e:
		case 0x068f:
		return;		// nop
	}

	if ((address & 0xffc00) == 0x00400) {
		DrvMainRAM[address] = data;
		rd2_cop_write(address, data);
	}
}

static void raidendx_bankswitch(INT32 bank)
{
	prg_bank = bank;

	bank = ((bank >> 12) + 16) * 0x10000;

	VezMapArea(0x20000, 0x2ffff, 0, DrvMainROM + bank);
	VezMapArea(0x20000, 0x2ffff, 2, DrvMainROM + bank);
}

static void __fastcall raidendx_main_write(UINT32 address, UINT8 data)
{
	if ((address & 0xff000) == 0x1f000) {
		DrvPalRAM[address & 0xfff] = data;
		palette_update_entry(address & 0xffe);
		return;
	}

	if ((address & 0xffc00) == 0x00000) {
		DrvMainRAM[address] = data;
		return;
	}
	
	if (address >= 0x00600 && address <= 0x0064f) {
		raiden2_crtc_write(BITSWAP08(address & 0x7f, 7,6,4,5,3,2,1,0), data);
	}

	switch (address)
	{
		case 0x0470:
		case 0x0471: {
			DrvMainRAM[address] = data;
			cop_bank = *((UINT16*)(DrvMainRAM + 0x470));
			if (address & 1) {
				raidendx_bankswitch(cop_bank);
			}
			fg_bank = ((cop_bank >> 4) & 3) | 4;
		}
		return; // absolutely must be return!
		
		case 0x068e:
		case 0x068f:
		return;		// nop
	}

	if ((address & 0xffc00) == 0x00400) {
		DrvMainRAM[address] = data;
		rd2_cop_write(address, data);
	}
}

static void __fastcall zeroteam_main_write(UINT32 address, UINT8 data)
{
	if ((address & 0xff000) == 0x0e000) {
		DrvPalRAM[address & 0xfff] = data;
		palette_update_entry(address & 0xffe);
		return;
	}

	if ((address & 0xffc00) == 0x00000) {
		DrvMainRAM[address] = data;
		return;
	}

	switch (address)
	{
		case 0x0470:
		case 0x0471:
		return;		// disable fg banking

		case 0x06cc:
		case 0x06cd: 	// disable bg & mg banking
		return;

		case 0x068e:
		case 0x068f:
		return;		// nop
	}

	if ((address & 0xffc00) == 0x00400) {
		DrvMainRAM[address] = data;
		rd2_cop_write(address, data);
	}
}

static UINT8 __fastcall raiden2_main_read(UINT32 address)
{
	if ((address & 0xffc00) == 0x00000) {
		return DrvMainRAM[address];
	}

	switch (address)
	{
		case 0x0740:
			return DrvDips[0];
			
		case 0x0741:
			return DrvDips[1];

		case 0x0744:
			return DrvInputs[0];
			
		case 0x0745:
			return DrvInputs[0] >> 8;

		case 0x0748:
			return DrvInputs[1];
			
		case 0x0749:
			return DrvInputs[1] >> 8;

		case 0x074c:
			return DrvInputs[2];
			
		case 0x074d:
			return DrvInputs[2] >> 8;
	}

	if ((address & 0xffc00) == 0x00400) {
		return rd2_cop_read(address);
	}

	return 0;
}

static void tilemap_dma()
{
	memcpy (DrvBgRAM, DrvMainRAM + 0xd000, 0x0800);
	memcpy (DrvFgRAM, DrvMainRAM + 0xd800, 0x0800);
	memcpy (DrvMgRAM, DrvMainRAM + 0xe000, 0x0800);
	memcpy (DrvTxRAM, DrvMainRAM + 0xe800, 0x1000);
}

static void palettedma()
{
	for (INT32 i = 0; i < 0x1000 / 2; i++)
	{
		UINT16 palval = *((UINT16*)(DrvMainRAM + 0x1f000 + i * 2));

		UINT8 r = palval & 0x1f;
		UINT8 g = (palval >> 5) & 0x1f;
		UINT8 b = (palval >> 10) & 0x1f;

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);

		DrvPalette[i] = (r*0x10000)+(g*0x100)+b;
	}
}

static void __fastcall nzeroteam_main_write(UINT32 address, UINT8 data)
{
	if ((address & 0xff800) == 0x00000) {
		DrvMainRAM[(address & 0x7ff)] = data;
		if (address < 0x400) return;
	}

	UINT16 *copram = (UINT16*)DrvMainRAM;
	UINT16 dataword = copram[(address & 0x7fe)/2];

	if (address >= 0x600 && address <= 0x64f) {
		raiden2_crtc_write(address & 0xff, data);
		return;
	}

	if ((address & 0xfffe0) == 0x00780) {
		seibu_main_word_write((address & 0x1f)/2, dataword);
		return;
	}

	switch (address)
	{
		case 0x400:
			tilemap_dma();
		return;

		case 0x402:
			palettedma();
		return;

		case 0x420:
			r2dx_i_dx = data;
		return;

		case 0x422:
			r2dx_i_dy = data;
		return;

		case 0x424:
		case 0x425:
			r2dx_i_sdist = (r2dx_i_sdist & 0xffff0000) | dataword;
		return;

		case 0x426:
		case 0x427:
			r2dx_i_sdist = (r2dx_i_sdist & 0x0000ffff) | (dataword << 16);
		return;

		case 0x428:
			r2dx_i_angle = data * 4;
		return;
	}
}

static UINT8 __fastcall nzeroteam_main_read(UINT32 address)
{
	if ((address & 0xffc00) == 0x00000) {
		return DrvMainRAM[address];
	}

	if ((address & 0xfffe0) == 0x00780) {
		return seibu_main_word_read((address & 0x1f)/2) >> ((address & 1) * 8);
	}

	switch (address)
	{
		case 0x430: 
			return DrvCopxROM[(r2dx_i_dy << 8) | r2dx_i_dx];

		case 0x432:
			return (UINT32)sqrt((double)r2dx_i_sdist) >> 0;

		case 0x433:
			return (UINT32)sqrt((double)r2dx_i_sdist) >> 8;

		case 0x434:
			return DrvCopxROM[0x10000 + r2dx_i_angle + 0];

		case 0x435:
			return DrvCopxROM[0x10000 + r2dx_i_angle + 1];

		case 0x436:
			return DrvCopxROM[0x10000 + r2dx_i_angle + 2];

		case 0x437:
			return DrvCopxROM[0x10000 + r2dx_i_angle + 3];

		case 0x740:
			return DrvDips[0];

		case 0x741:
			return DrvDips[1];

		case 0x744:
			return DrvInputs[0];

		case 0x745:
			return DrvInputs[0] >> 8;

		case 0x74c:
			return DrvInputs[1];

		case 0x74d:
			return DrvInputs[1] >> 8; 
	}

	return 0;
}

static void __fastcall zerotm2k_main_write(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x748:
			EEPROMWrite((data & 0x02), (data & 0x01), (data >> 2) & 1);
		return;

		case 0x749:
		return;
	}

	nzeroteam_main_write(address, data);
}

static UINT8 __fastcall zerotm2k_main_read(UINT32 address)
{
	switch (address)
	{
		case 0x740:
			return DrvInputs[1];

		case 0x741:
			return DrvInputs[1] >> 8;

		case 0x744:
			return DrvInputs[0];

		case 0x745:
			return ((DrvInputs[0] >> 8) & 0x7f) | (EEPROMRead() ? 0x80 : 0);

		case 0x74c:
			return DrvInputs[2];

		case 0x74d:
			return DrvInputs[2] >> 8;
	}

	return nzeroteam_main_read(address);
}

static void r2dx_bankswitch(INT32 set_bank, INT32 set_main)
{
	INT32 bank0 = ((set_main & 0x1) * 0x200000);
	INT32 bank1 = ((set_bank & 0xf) * 0x010000);

	VezMapArea(0x20000, 0x2ffff, 0, DrvMainROM + 0x100000 + bank0 + bank1);
	VezMapArea(0x20000, 0x2ffff, 2, DrvMainROM + 0x100000 + bank0 + bank1);

	VezMapArea(0x30000, 0xfffff, 0, DrvMainROM + 0x030000 + bank0);
	VezMapArea(0x30000, 0xfffff, 2, DrvMainROM + 0x030000 + bank0);
}

static void r2dx_okibankswitch()
{
	memcpy (DrvSndROM0, DrvSndROM1 + r2dx_okibank * 0x40000, 0x40000);
}

static void __fastcall r2dx_main_write(UINT32 address, UINT8 data)
{
	if ((address & 0xff800) == 0x00000) {
		DrvMainRAM[(address & 0x7ff)] = data;
		if (address < 0x400) return;
	}

	if (address >= 0x600 && address <= 0x64f) {
		raiden2_crtc_write(address & 0xff, data);
		return;
	}

	UINT16 *copram = (UINT16*)DrvMainRAM;
	UINT16 dataword = copram[(address & 0x7fe)/2];

	if ((address & 1) == 0 && address < 0x700) return; // necessary

	switch (address & 0x7fe)
	{
		case 0x400:
			tilemap_dma();
		return;

		case 0x402:
			palettedma();
		return;

		case 0x404:
			prg_bank = dataword & 0x0f;
			r2dx_bankswitch(prg_bank, r2dx_gameselect);
		return;

		case 0x406:
			bg_bank = ((dataword >> 4) & 1) + 0;
			mg_bank = ((dataword >> 5) & 1) + 2;
			fg_bank = ((dataword >> 0) & 3) + 4;
		return;

		case 0x420:
			r2dx_i_dx = dataword & 0xff;
		return;

		case 0x422:
			r2dx_i_dy = dataword & 0xff;
		return;

		case 0x424:
			r2dx_i_sdist = (r2dx_i_sdist & 0xffff0000) | dataword;
		return;

		case 0x426:
			r2dx_i_sdist = (r2dx_i_sdist & 0x0000ffff) | (dataword << 16);
		return;

		case 0x428:
			r2dx_i_angle = (dataword & 0xff) * 4;
		return;

		case 0x6c0:
			cop_spr_off = dataword;
		return;

		case 0x6c2:
			sprite_prot_src_addr[0] = dataword;
		return;

		case 0x6c6:
			dst1 = dataword;
			copram[0x762/2] = dst1;
		return;

		case 0x6d8:
			sprite_prot_x = dataword;
		return;

		case 0x6da:
			sprite_prot_y = dataword;
		return;

		case 0x6dc:
			cop_spr_maxx = dataword;
		return;

		case 0x6de:
			sprite_prot_src_write(dataword);
		return;

		case 0x700:
		{
			EEPROMWrite((dataword & 0x10), (dataword & 0x08), ((dataword & 0x20) >> 5));

			r2dx_gameselect = tx_bank = (dataword & 4) >> 2;

			r2dx_bankswitch(prg_bank, r2dx_gameselect);

			r2dx_okibank = dataword & 3;
			r2dx_okibankswitch();
		}
		return;

		case 0x780: // oki write
			MSM6295Command(0, dataword & 0xff);
		return;
	}
}

static UINT8 __fastcall r2dx_main_read(UINT32 address)
{
	if ((address & 0xffc00) == 0x00000) {
		return DrvMainRAM[address];
	}

	switch (address)
	{
		case 0x430: 
			return DrvCopxROM[(r2dx_i_dy << 8) | r2dx_i_dx];

		case 0x432:
			return (UINT32)sqrt((double)r2dx_i_sdist) >> 0;

		case 0x433:
			return (UINT32)sqrt((double)r2dx_i_sdist) >> 8;

		case 0x434:
			return DrvCopxROM[0x10000 + r2dx_i_angle + 0];

		case 0x435:
			return DrvCopxROM[0x10000 + r2dx_i_angle + 1];

		case 0x436:
			return DrvCopxROM[0x10000 + r2dx_i_angle + 2];

		case 0x437:
			return DrvCopxROM[0x10000 + r2dx_i_angle + 3];

		case 0x6c0:
			return cop_spr_off >> 0;

		case 0x6c1:
			return cop_spr_off >> 8;

		case 0x6c2:
			return sprite_prot_src_addr[0] >> 0;

		case 0x6c3:
			return sprite_prot_src_addr[0] >> 8;

		case 0x6dc:
			return cop_spr_maxx >> 0;

		case 0x6dd:
			return cop_spr_maxx >> 8;

		case 0x740:
		case 0x741:
			return 0xff; // debug_r

		case 0x744:
			return DrvInputs[0];

		case 0x745:
			return DrvInputs[0] >> 8;

		case 0x74c:
			return (DrvDips[0] & 0xc0) | (DrvInputs[1] & 0x2f) | (EEPROMRead() ? 0x10 : 0);

		case 0x74d:
			return DrvInputs[1] >> 8;

		case 0x762:
			return dst1 >> 0;

		case 0x763:
			return dst1 >> 8;

		case 0x780:
			return MSM6295ReadStatus(0);
	}

	return DrvMainRAM[address & 0x7ff];
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	VezOpen(0);
	VezReset();
	VezClose();

	if (game_select != 4) {
		seibu_sound_reset();
	}

	if (game_select == 4)
	{
		MSM6295Reset(0);

		EEPROMReset();

		if (EEPROMAvailable() == 0) {
			EEPROMFill(DrvEeprom, 0, 0x80);
		}
	}

	if (game_select == 6)
	{
		EEPROMReset();
	}

	prg_bank = 0;
	layer_enable = 0;
	bg_bank = 0;
	fg_bank = (game_select >= 2) ? 2 : 6;
	mg_bank = 1;
	tx_bank = 0;
	r2dx_okibank = 0;

	SeibuCopReset();

	if (game_select >= 4) sprites_cur_start = 0x1000 - 8; // or no sprites in newzeroteam

	memset (hold_coin, 0, 4 * sizeof(INT32));
	
	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvMainROM		= Next; Next += 0x400000;

	SeibuZ80ROM		= Next; Next += 0x020000;

	DrvEeprom		= Next; Next += 0x000080;

	DrvCopxROM		= Next; Next += 0x020000;

	DrvGfxROM0		= Next; Next += 0x080000;
	DrvGfxROM1		= Next; Next += 0x800000;
	DrvGfxROM2		= Next; Next += 0x1000000;

	DrvTransTab		= Next; Next += 0x800000 / 0x100;

	MSM6295ROM		= Next;
	DrvSndROM0		= Next; Next += 0x100000;
	DrvSndROM1		= Next; Next += 0x100000;

	DrvPalette		= (UINT32*)Next; Next += 0x0800 * sizeof(UINT32);

	bitmap32		= (UINT32*)Next; Next += 320 * 256 * sizeof(UINT32);
	DrvAlphaTable		= Next; Next += 0x000800;

	AllRam			= Next;

	DrvMainRAM		= Next; Next += 0x020000;

	DrvTxRAM		= Next; Next += 0x001000;
	DrvBgRAM		= Next; Next += 0x000800;
	DrvFgRAM		= Next; Next += 0x000800;
	DrvMgRAM		= Next; Next += 0x000800;

	DrvSprRAM		= Next; Next += 0x001000;

	DrvPalRAM		= Next; Next += 0x001000;

	SeibuZ80RAM		= Next; Next += 0x000800;

	scroll			= Next; Next += 0x00000c;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void DrvCreateAlphaTable(INT32 raiden2_alpha)
{
	memset (DrvAlphaTable, 0, 0x800); 

	if (raiden2_alpha) { // raiden2/dx
		DrvAlphaTable[0x380] = 1;
		DrvAlphaTable[0x5de] = 1;
		DrvAlphaTable[0x75c] = 1;

		memset (DrvAlphaTable + 0x3c0, 1, 0x30);
		memset (DrvAlphaTable + 0x4f8, 1, 0x08);
		memset (DrvAlphaTable + 0x5c8, 1, 0x08);
		memset (DrvAlphaTable + 0x5e8, 1, 0x08);
		memset (DrvAlphaTable + 0x5f8, 1, 0x08);

		memset (DrvAlphaTable + 0x6c8, 1, 0x08);
		memset (DrvAlphaTable + 0x6d8, 1, 0x08);
		memset (DrvAlphaTable + 0x6e8, 1, 0x08);
		memset (DrvAlphaTable + 0x6f8, 1, 0x08);

		memset (DrvAlphaTable + 0x70d, 1, 0x02);
		memset (DrvAlphaTable + 0x71c, 1, 0x03);
		memset (DrvAlphaTable + 0x72d, 1, 0x02);
		memset (DrvAlphaTable + 0x73d, 1, 0x02);
		memset (DrvAlphaTable + 0x74d, 1, 0x02);
		memset (DrvAlphaTable + 0x76c, 1, 0x03);
		memset (DrvAlphaTable + 0x77d, 1, 0x02);
		memset (DrvAlphaTable + 0x7c8, 1, 0x08);
	}
	else // zero team
	{
		DrvAlphaTable[0x37e] = 1;
		DrvAlphaTable[0x38e] = 1;
		DrvAlphaTable[0x52e] = 1;
		DrvAlphaTable[0x5de] = 1;
	}
}

static void DrvCreateTransTab()
{
	memset (DrvTransTab, 1, 0x800000/0x100);

	for (INT32 i = 0; i < 0x800000; i+=0x100) {
		for (INT32 j = 0; j < 0x100; j++) {
			if (DrvGfxROM1[i+j] != 0x0f) {
				DrvTransTab[i/0x100] = 0;
				break;
			}
		}
	}
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[4]  = { 8,12,0,4};
	INT32 Plane1[4]  = { STEP4(0,1) };
	INT32 XOffs0[16] = { 3,2,1,0,19,18,17,16,3+64*8, 2+64*8, 1+64*8, 0+64*8,19+64*8,18+64*8,17+64*8,16+64*8 };
	INT32 XOffs1[16] = { 4, 0, 12, 8, 20, 16, 28, 24, 36, 32, 44, 40, 52, 48, 60, 56 };
	INT32 YOffs0[16] = { STEP16(0,32) };
	INT32 YOffs1[16] = { STEP16(0,64) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x800000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x040000);

	GfxDecode(0x02000, 4,  8,  8, Plane0, XOffs0, YOffs0, 0x100, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x400000);

	GfxDecode(0x08000, 4, 16, 16, Plane0, XOffs0, YOffs0, 0x400, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x800000);

	GfxDecode(0x10000, 4, 16, 16, Plane1, XOffs1, YOffs1, 0x400, tmp, DrvGfxROM2);

	BurnFree (tmp);

	return 0;
}

static const UINT8 rotate_r2[512] = {
	0x11, 0x17, 0x0d, 0x03, 0x17, 0x1f, 0x08, 0x1a, 0x0f, 0x04, 0x1e, 0x13, 0x19, 0x0e, 0x0e, 0x05,
	0x06, 0x07, 0x08, 0x08, 0x0d, 0x18, 0x11, 0x1a, 0x0b, 0x06, 0x12, 0x0c, 0x1f, 0x0b, 0x1c, 0x19,
	0x00, 0x1b, 0x0c, 0x09, 0x1d, 0x18, 0x1a, 0x16, 0x1a, 0x08, 0x03, 0x04, 0x0f, 0x1d, 0x16, 0x07,
	0x1a, 0x12, 0x01, 0x0b, 0x00, 0x0f, 0x1e, 0x10, 0x09, 0x0f, 0x10, 0x09, 0x0a, 0x1c, 0x0d, 0x08,
	0x06, 0x1a, 0x06, 0x02, 0x11, 0x1e, 0x0c, 0x1c, 0x11, 0x0f, 0x19, 0x0a, 0x16, 0x14, 0x18, 0x11,
	0x0b, 0x0d, 0x1c, 0x1f, 0x0d, 0x1f, 0x0d, 0x19, 0x0d, 0x04, 0x19, 0x0f, 0x06, 0x13, 0x0c, 0x1b,
	0x1f, 0x12, 0x15, 0x1a, 0x04, 0x02, 0x06, 0x03, 0x0a, 0x0d, 0x12, 0x09, 0x17, 0x1d, 0x12, 0x10,
	0x05, 0x07, 0x03, 0x00, 0x14, 0x07, 0x14, 0x1a, 0x1c, 0x0a, 0x10, 0x0f, 0x0b, 0x0c, 0x08, 0x0f,
	0x07, 0x00, 0x13, 0x1c, 0x04, 0x15, 0x0e, 0x02, 0x17, 0x17, 0x00, 0x03, 0x18, 0x00, 0x02, 0x13,
	0x14, 0x0c, 0x01, 0x0a, 0x15, 0x0b, 0x0a, 0x1c, 0x1b, 0x06, 0x17, 0x1d, 0x11, 0x1f, 0x10, 0x04,
	0x1a, 0x01, 0x1b, 0x13, 0x03, 0x09, 0x09, 0x0f, 0x0d, 0x03, 0x15, 0x1c, 0x04, 0x06, 0x06, 0x0b,
	0x04, 0x0a, 0x1f, 0x16, 0x11, 0x0a, 0x05, 0x05, 0x0c, 0x1c, 0x10, 0x0c, 0x11, 0x04, 0x10, 0x1a,
	0x06, 0x10, 0x19, 0x06, 0x15, 0x0f, 0x11, 0x01, 0x10, 0x0c, 0x1d, 0x05, 0x1f, 0x05, 0x12, 0x16,
	0x02, 0x12, 0x14, 0x0d, 0x14, 0x0f, 0x04, 0x07, 0x13, 0x01, 0x11, 0x1c, 0x1c, 0x1d, 0x0e, 0x06,
	0x1d, 0x13, 0x10, 0x06, 0x0f, 0x02, 0x12, 0x10, 0x1e, 0x0c, 0x17, 0x15, 0x0b, 0x1f, 0x01, 0x19,
	0x02, 0x01, 0x07, 0x1d, 0x13, 0x19, 0x0f, 0x0f, 0x10, 0x03, 0x1e, 0x03, 0x0d, 0x0a, 0x0c, 0x0d,

	0x16, 0x1f, 0x16, 0x1a, 0x1c, 0x16, 0x01, 0x03, 0x01, 0x08, 0x14, 0x19, 0x03, 0x1e, 0x08, 0x02,
	0x02, 0x1d, 0x15, 0x00, 0x09, 0x1d, 0x03, 0x11, 0x11, 0x0b, 0x1b, 0x14, 0x01, 0x1e, 0x11, 0x12,
	0x1d, 0x06, 0x0b, 0x13, 0x1e, 0x16, 0x0d, 0x10, 0x11, 0x1f, 0x1c, 0x15, 0x0d, 0x1a, 0x13, 0x1f,
	0x0e, 0x05, 0x10, 0x06, 0x0d, 0x1c, 0x07, 0x19, 0x06, 0x1d, 0x11, 0x00, 0x1c, 0x05, 0x0b, 0x1d,
	0x1c, 0x06, 0x05, 0x1d, 0x00, 0x13, 0x00, 0x12, 0x1b, 0x17, 0x1a, 0x1b, 0x17, 0x1c, 0x16, 0x0a,
	0x11, 0x15, 0x0f, 0x0b, 0x0f, 0x07, 0x0e, 0x04, 0x13, 0x00, 0x1c, 0x05, 0x16, 0x00, 0x1a, 0x04,
	0x17, 0x04, 0x08, 0x1b, 0x05, 0x12, 0x1d, 0x0d, 0x02, 0x16, 0x12, 0x0e, 0x06, 0x08, 0x14, 0x07,
	0x0e, 0x0f, 0x15, 0x13, 0x12, 0x00, 0x1d, 0x16, 0x1b, 0x18, 0x1f, 0x05, 0x12, 0x13, 0x01, 0x0c,
	0x12, 0x04, 0x19, 0x13, 0x12, 0x15, 0x07, 0x06, 0x0a, 0x00, 0x09, 0x14, 0x1e, 0x03, 0x10, 0x1b,
	0x08, 0x1a, 0x07, 0x02, 0x1b, 0x0d, 0x18, 0x13, 0x02, 0x07, 0x1e, 0x05, 0x15, 0x02, 0x06, 0x18,
	0x12, 0x09, 0x1c, 0x07, 0x0b, 0x02, 0x03, 0x00, 0x18, 0x18, 0x03, 0x0f, 0x02, 0x0f, 0x10, 0x09,
	0x05, 0x18, 0x08, 0x1b, 0x0d, 0x10, 0x03, 0x00, 0x0c, 0x14, 0x1d, 0x08, 0x02, 0x10, 0x0b, 0x0c,
	0x00, 0x0d, 0x0d, 0x0a, 0x06, 0x1c, 0x09, 0x19, 0x1b, 0x14, 0x18, 0x0f, 0x02, 0x07, 0x05, 0x04,
	0x1c, 0x15, 0x18, 0x00, 0x0b, 0x10, 0x19, 0x1c, 0x1b, 0x08, 0x1d, 0x12, 0x17, 0x1d, 0x0c, 0x01,
	0x03, 0x0d, 0x03, 0x0d, 0x15, 0x0e, 0x16, 0x08, 0x05, 0x11, 0x1f, 0x03, 0x16, 0x03, 0x0f, 0x10,
	0x08, 0x19, 0x18, 0x15, 0x1f, 0x05, 0x00, 0x09, 0x0e, 0x05, 0x16, 0x1b, 0x01, 0x08, 0x08, 0x1f,
};

static const UINT8 x5_r2[256] = {
	0x08, 0x09, 0x1f, 0x0f, 0x09, 0x09, 0x0b, 0x1d, 0x06, 0x13, 0x02, 0x15, 0x02, 0x0c, 0x0d, 0x19,
	0x03, 0x13, 0x0c, 0x1f, 0x1a, 0x18, 0x17, 0x10, 0x0a, 0x19, 0x15, 0x04, 0x1f, 0x11, 0x1c, 0x02,
	0x0e, 0x08, 0x06, 0x0a, 0x07, 0x1c, 0x10, 0x04, 0x11, 0x0c, 0x0a, 0x19, 0x0a, 0x04, 0x17, 0x07,
	0x16, 0x1b, 0x1d, 0x15, 0x1d, 0x13, 0x0e, 0x03, 0x1a, 0x11, 0x14, 0x14, 0x03, 0x18, 0x07, 0x03,
	0x08, 0x1a, 0x02, 0x0f, 0x0b, 0x11, 0x1c, 0x05, 0x19, 0x1d, 0x05, 0x01, 0x1f, 0x1c, 0x1d, 0x07,
	0x07, 0x0c, 0x02, 0x16, 0x0e, 0x06, 0x0b, 0x07, 0x01, 0x1a, 0x09, 0x0e, 0x0e, 0x07, 0x0e, 0x15,
	0x01, 0x16, 0x13, 0x15, 0x14, 0x07, 0x0c, 0x1f, 0x1f, 0x19, 0x17, 0x12, 0x19, 0x17, 0x0a, 0x1f,
	0x0c, 0x16, 0x15, 0x1e, 0x05, 0x14, 0x05, 0x1c, 0x0b, 0x0d, 0x0c, 0x0a, 0x05, 0x09, 0x14, 0x02,
	0x10, 0x02, 0x13, 0x05, 0x12, 0x17, 0x03, 0x0b, 0x1b, 0x06, 0x15, 0x0b, 0x01, 0x0b, 0x1b, 0x09,
	0x10, 0x0a, 0x1e, 0x09, 0x08, 0x0a, 0x04, 0x13, 0x04, 0x12, 0x04, 0x0f, 0x0b, 0x0c, 0x06, 0x07,
	0x03, 0x18, 0x00, 0x1e, 0x17, 0x00, 0x16, 0x08, 0x0d, 0x1c, 0x09, 0x07, 0x17, 0x18, 0x0b, 0x0d,
	0x11, 0x0f, 0x14, 0x1e, 0x1a, 0x1b, 0x09, 0x15, 0x03, 0x07, 0x12, 0x16, 0x15, 0x11, 0x16, 0x1e,
	0x14, 0x15, 0x00, 0x05, 0x15, 0x18, 0x18, 0x12, 0x18, 0x1e, 0x06, 0x06, 0x0c, 0x1a, 0x04, 0x0b,
	0x05, 0x08, 0x04, 0x1f, 0x0c, 0x08, 0x0a, 0x1f, 0x1a, 0x16, 0x0e, 0x1e, 0x16, 0x18, 0x18, 0x05,
	0x00, 0x1a, 0x05, 0x15, 0x19, 0x10, 0x03, 0x0e, 0x10, 0x1c, 0x0a, 0x18, 0x00, 0x16, 0x0b, 0x05,
	0x05, 0x15, 0x11, 0x0a, 0x1c, 0x00, 0x1e, 0x1f, 0x17, 0x12, 0x0a, 0x1c, 0x07, 0x04, 0x1f, 0x1a,
};

static const UINT16 x11_r2[256] = {
	0x347, 0x0f2, 0x182, 0x58f, 0x1f4, 0x42c, 0x407, 0x5f0, 0x6df, 0x2db, 0x585, 0x5fe, 0x394, 0x542, 0x3e8, 0x574,
	0x4ea, 0x6d3, 0x6b7, 0x65b, 0x324, 0x143, 0x22a, 0x11d, 0x124, 0x365, 0x7ca, 0x3d6, 0x1d2, 0x7cd, 0x6b1, 0x4f1,
	0x1de, 0x674, 0x685, 0x779, 0x264, 0x6d8, 0x379, 0x7ce, 0x201, 0x73b, 0x5c9, 0x025, 0x338, 0x4b2, 0x697, 0x567,
	0x312, 0x04e, 0x78d, 0x492, 0x044, 0x203, 0x437, 0x04b, 0x729, 0x197, 0x6e2, 0x552, 0x517, 0x3c9, 0x09c, 0x3de,
	0x2f8, 0x259, 0x1f0, 0x6ce, 0x6d6, 0x55d, 0x223, 0x65e, 0x7ca, 0x330, 0x3f7, 0x348, 0x640, 0x26d, 0x340, 0x2df,
	0x752, 0x792, 0x5b0, 0x2fb, 0x398, 0x75c, 0x0a2, 0x524, 0x538, 0x74c, 0x1c5, 0x5a2, 0x522, 0x7c3, 0x6b3, 0x4f0,
	0x5ac, 0x40b, 0x3e0, 0x1c8, 0x6ff, 0x291, 0x7c4, 0x47c, 0x6d9, 0x248, 0x623, 0x78d, 0x2cd, 0x356, 0x12a, 0x0bc,
	0x582, 0x1d8, 0x1c6, 0x6eb, 0x7c2, 0x7f9, 0x650, 0x57d, 0x701, 0x7e5, 0x118, 0x1b4, 0x4ad, 0x2b8, 0x2bb, 0x765,
	0x2d9, 0x46a, 0x020, 0x2da, 0x5e4, 0x115, 0x53c, 0x2b4, 0x16d, 0x0f7, 0x633, 0x1a6, 0x0a0, 0x3e6, 0x29d, 0x77b,
	0x558, 0x185, 0x7b9, 0x0b1, 0x36e, 0x4d3, 0x7e2, 0x5f9, 0x3d2, 0x21e, 0x0e1, 0x2ac, 0x0fc, 0x0fc, 0x66d, 0x7b5,
	0x4af, 0x627, 0x0f4, 0x621, 0x58f, 0x3d7, 0x1bc, 0x10a, 0x458, 0x259, 0x451, 0x770, 0x107, 0x134, 0x162, 0x32f,
	0x5cf, 0x6c9, 0x670, 0x2d4, 0x0da, 0x739, 0x30c, 0x62f, 0x4af, 0x0e2, 0x3e3, 0x65c, 0x214, 0x066, 0x47d, 0x2f2,
	0x729, 0x566, 0x450, 0x3f2, 0x35d, 0x593, 0x593, 0x532, 0x008, 0x270, 0x479, 0x358, 0x6f3, 0x7ed, 0x240, 0x587,
	0x581, 0x00f, 0x750, 0x4d8, 0x1ab, 0x100, 0x47f, 0x34f, 0x497, 0x240, 0x769, 0x76f, 0x705, 0x375, 0x684, 0x273,
	0x01f, 0x268, 0x2cc, 0x2d7, 0x5d4, 0x284, 0x40c, 0x5e8, 0x7c1, 0x281, 0x518, 0x4b0, 0x136, 0x73b, 0x3ea, 0x023,
	0x1c1, 0x7de, 0x106, 0x275, 0x1e1, 0x503, 0x30a, 0x271, 0x4f8, 0x52b, 0x266, 0x375, 0x024, 0x399, 0x672, 0x6f8,
};

static const UINT8 rotate_zt[256] = {
	0x0a, 0x0a, 0x0b, 0x0e, 0x02, 0x12, 0x0d, 0x16, 0x01, 0x1b, 0x08, 0x15, 0x03, 0x14, 0x0f, 0x1f,
	0x10, 0x1d, 0x03, 0x0b, 0x16, 0x07, 0x10, 0x19, 0x1f, 0x1c, 0x03, 0x0b, 0x15, 0x0d, 0x00, 0x14,
	0x15, 0x0d, 0x0c, 0x1b, 0x18, 0x18, 0x0b, 0x1b, 0x16, 0x17, 0x01, 0x0e, 0x14, 0x0c, 0x1e, 0x13,
	0x13, 0x12, 0x19, 0x0b, 0x1b, 0x1a, 0x0d, 0x04, 0x00, 0x00, 0x12, 0x06, 0x0f, 0x1c, 0x19, 0x1b,
	0x01, 0x04, 0x15, 0x1c, 0x02, 0x13, 0x0f, 0x09, 0x1c, 0x05, 0x09, 0x05, 0x0d, 0x17, 0x13, 0x1f,
	0x17, 0x19, 0x0f, 0x04, 0x05, 0x03, 0x0c, 0x03, 0x13, 0x02, 0x09, 0x0d, 0x1d, 0x1a, 0x02, 0x0d,
	0x0d, 0x0f, 0x13, 0x1c, 0x05, 0x03, 0x09, 0x08, 0x03, 0x1d, 0x05, 0x1f, 0x0b, 0x15, 0x06, 0x10,
	0x0c, 0x15, 0x18, 0x0c, 0x18, 0x1f, 0x1d, 0x1d, 0x1d, 0x0e, 0x1f, 0x0c, 0x14, 0x0d, 0x0a, 0x08,
	0x0f, 0x12, 0x08, 0x06, 0x0d, 0x1d, 0x19, 0x1e, 0x1c, 0x08, 0x09, 0x1e, 0x1a, 0x14, 0x08, 0x17,
	0x11, 0x0e, 0x1a, 0x18, 0x01, 0x08, 0x05, 0x01, 0x06, 0x1e, 0x15, 0x1d, 0x0c, 0x09, 0x1b, 0x07,
	0x12, 0x00, 0x0c, 0x05, 0x03, 0x08, 0x19, 0x09, 0x09, 0x06, 0x13, 0x12, 0x0b, 0x1a, 0x09, 0x09,
	0x03, 0x09, 0x00, 0x07, 0x1c, 0x10, 0x14, 0x01, 0x17, 0x03, 0x05, 0x11, 0x1e, 0x12, 0x14, 0x19,
	0x10, 0x10, 0x08, 0x08, 0x07, 0x0c, 0x12, 0x1f, 0x02, 0x08, 0x1f, 0x14, 0x1e, 0x14, 0x02, 0x13,
	0x04, 0x10, 0x1e, 0x08, 0x0b, 0x14, 0x19, 0x1a, 0x07, 0x1c, 0x13, 0x07, 0x10, 0x14, 0x15, 0x1e,
	0x18, 0x0a, 0x0c, 0x1f, 0x0e, 0x05, 0x11, 0x08, 0x04, 0x06, 0x12, 0x0c, 0x00, 0x04, 0x0b, 0x07,
	0x14, 0x0a, 0x0c, 0x16, 0x02, 0x0a, 0x0f, 0x0d, 0x0c, 0x1d, 0x1e, 0x13, 0x0a, 0x08, 0x12, 0x0b,
};

static const UINT8 x5_zt[256] = {
	0x0d, 0x14, 0x13, 0x1b, 0x1a, 0x05, 0x1a, 0x05, 0x1b, 0x17, 0x0d, 0x0f, 0x00, 0x17, 0x09, 0x04,
	0x13, 0x0f, 0x14, 0x17, 0x14, 0x0f, 0x1b, 0x0a, 0x0a, 0x15, 0x07, 0x0b, 0x0e, 0x05, 0x18, 0x12,
	0x1c, 0x01, 0x17, 0x02, 0x04, 0x00, 0x06, 0x12, 0x00, 0x0a, 0x1c, 0x19, 0x10, 0x1b, 0x16, 0x03,
	0x1c, 0x13, 0x16, 0x13, 0x15, 0x1d, 0x04, 0x14, 0x17, 0x1d, 0x01, 0x03, 0x1f, 0x1f, 0x1a, 0x10,
	0x08, 0x0c, 0x09, 0x16, 0x04, 0x19, 0x03, 0x14, 0x0c, 0x1d, 0x09, 0x08, 0x00, 0x1c, 0x0b, 0x18,
	0x1b, 0x00, 0x16, 0x0f, 0x0b, 0x1c, 0x0d, 0x16, 0x09, 0x15, 0x0c, 0x01, 0x18, 0x04, 0x04, 0x03,
	0x11, 0x0f, 0x03, 0x17, 0x04, 0x0a, 0x05, 0x1b, 0x08, 0x07, 0x0e, 0x08, 0x06, 0x16, 0x0f, 0x0d,
	0x0a, 0x14, 0x0a, 0x11, 0x14, 0x02, 0x08, 0x1f, 0x0e, 0x13, 0x0e, 0x02, 0x08, 0x13, 0x04, 0x19,
	0x0e, 0x00, 0x01, 0x1d, 0x02, 0x19, 0x08, 0x1c, 0x13, 0x09, 0x11, 0x14, 0x1e, 0x04, 0x03, 0x18,
	0x1f, 0x05, 0x0f, 0x03, 0x09, 0x0d, 0x12, 0x0f, 0x0d, 0x0e, 0x15, 0x1a, 0x17, 0x1c, 0x1d, 0x10,
	0x15, 0x00, 0x18, 0x18, 0x00, 0x08, 0x08, 0x01, 0x0d, 0x10, 0x14, 0x12, 0x03, 0x1c, 0x05, 0x0a,
	0x0f, 0x0d, 0x15, 0x00, 0x0b, 0x1f, 0x00, 0x1d, 0x1d, 0x1a, 0x13, 0x16, 0x08, 0x1b, 0x03, 0x12,
	0x02, 0x10, 0x0e, 0x10, 0x04, 0x05, 0x09, 0x07, 0x1d, 0x04, 0x05, 0x06, 0x17, 0x05, 0x1a, 0x04,
	0x1f, 0x0e, 0x09, 0x13, 0x15, 0x12, 0x1c, 0x13, 0x03, 0x09, 0x12, 0x08, 0x19, 0x17, 0x00, 0x19,
	0x1f, 0x06, 0x1c, 0x05, 0x18, 0x0a, 0x1f, 0x1e, 0x0d, 0x05, 0x1e, 0x1a, 0x11, 0x13, 0x15, 0x0c,
	0x05, 0x16, 0x11, 0x06, 0x12, 0x0c, 0x0c, 0x0f, 0x1c, 0x18, 0x08, 0x1e, 0x1c, 0x16, 0x15, 0x03,
};

static const UINT16 x11_zt[512] = {
	0x7e9, 0x625, 0x625, 0x006, 0x006, 0x14a, 0x14a, 0x7e9, 0x657, 0x417, 0x417, 0x1a7, 0x1a7, 0x22f, 0x22f, 0x657,
	0x37e, 0x447, 0x447, 0x117, 0x117, 0x735, 0x735, 0x37e, 0x3be, 0x487, 0x487, 0x724, 0x724, 0x6e3, 0x6e3, 0x3be,
	0x646, 0x035, 0x035, 0x57e, 0x57e, 0x134, 0x134, 0x646, 0x09a, 0x4d1, 0x4d1, 0x511, 0x511, 0x5d5, 0x5d5, 0x09a,
	0x3e2, 0x5c6, 0x5c6, 0x3b6, 0x3b6, 0x354, 0x354, 0x3e2, 0x6e3, 0x7d6, 0x7d6, 0x317, 0x317, 0x1f0, 0x1f0, 0x6e3,
	0x115, 0x462, 0x462, 0x620, 0x620, 0x0ce, 0x0ce, 0x115, 0x715, 0x7ec, 0x7ec, 0x1e6, 0x1e6, 0x610, 0x610, 0x715,
	0x34a, 0x718, 0x718, 0x7ba, 0x7ba, 0x288, 0x288, 0x34a, 0x6e7, 0x193, 0x193, 0x3d2, 0x3d2, 0x78f, 0x78f, 0x6e7,
	0x57c, 0x014, 0x014, 0x6b3, 0x6b3, 0x204, 0x204, 0x57c, 0x3f4, 0x782, 0x782, 0x5b4, 0x5b4, 0x375, 0x375, 0x3f4,
	0x17d, 0x660, 0x660, 0x48d, 0x48d, 0x7ff, 0x7ff, 0x17d, 0x5b1, 0x39d, 0x39d, 0x48c, 0x48c, 0x5ab, 0x5ab, 0x5b1,
	0x3d3, 0x4f6, 0x4f6, 0x7b9, 0x7b9, 0x5df, 0x5df, 0x3d3, 0x349, 0x4c5, 0x4c5, 0x403, 0x403, 0x0ab, 0x0ab, 0x349,
	0x46b, 0x790, 0x790, 0x59f, 0x59f, 0x2b1, 0x2b1, 0x46b, 0x6c5, 0x4e7, 0x4e7, 0x023, 0x023, 0x0b0, 0x0b0, 0x6c5,
	0x2d6, 0x3e2, 0x3e2, 0x7f3, 0x7f3, 0x376, 0x376, 0x2d6, 0x2d7, 0x0bf, 0x0bf, 0x14b, 0x14b, 0x65b, 0x65b, 0x2d7,
	0x68d, 0x360, 0x360, 0x434, 0x434, 0x5fa, 0x5fa, 0x68d, 0x654, 0x386, 0x386, 0x28b, 0x28b, 0x2e1, 0x2e1, 0x654,
	0x1ee, 0x4e8, 0x4e8, 0x482, 0x482, 0x471, 0x471, 0x1ee, 0x221, 0x3de, 0x3de, 0x467, 0x467, 0x21b, 0x21b, 0x221,
	0x7a6, 0x6b6, 0x6b6, 0x157, 0x157, 0x658, 0x658, 0x7a6, 0x1e9, 0x6e3, 0x6e3, 0x340, 0x340, 0x2cd, 0x2cd, 0x1e9,
	0x045, 0x566, 0x566, 0x1f0, 0x1f0, 0x5a1, 0x5a1, 0x045, 0x559, 0x0d4, 0x0d4, 0x67c, 0x67c, 0x21a, 0x21a, 0x559,
	0x187, 0x13b, 0x13b, 0x68e, 0x68e, 0x0f6, 0x0f6, 0x187, 0x2b5, 0x4c2, 0x4c2, 0x2b6, 0x2b6, 0x6a4, 0x6a4, 0x2b5,
	0x56b, 0x1b2, 0x1b2, 0x15d, 0x15d, 0x3aa, 0x3aa, 0x56b, 0x506, 0x67a, 0x67a, 0x5db, 0x5db, 0x5ef, 0x5ef, 0x506,
	0x5ab, 0x3ed, 0x3ed, 0x2b3, 0x2b3, 0x504, 0x504, 0x5ab, 0x137, 0x761, 0x761, 0x735, 0x735, 0x6e9, 0x6e9, 0x137,
	0x35a, 0x7bb, 0x7bb, 0x190, 0x190, 0x0a8, 0x0a8, 0x35a, 0x797, 0x164, 0x164, 0x1b8, 0x1b8, 0x761, 0x761, 0x797,
	0x138, 0x076, 0x076, 0x613, 0x613, 0x055, 0x055, 0x138, 0x2e2, 0x493, 0x493, 0x0c2, 0x0c2, 0x785, 0x785, 0x2e2,
	0x60b, 0x41a, 0x41a, 0x22a, 0x22a, 0x2a1, 0x2a1, 0x60b, 0x5cb, 0x092, 0x092, 0x656, 0x656, 0x5db, 0x5db, 0x5cb,
	0x00a, 0x490, 0x490, 0x39d, 0x39d, 0x572, 0x572, 0x00a, 0x600, 0x4cd, 0x4cd, 0x33f, 0x33f, 0x0bc, 0x0bc, 0x600,
	0x047, 0x685, 0x685, 0x4b0, 0x4b0, 0x330, 0x330, 0x047, 0x7b4, 0x590, 0x590, 0x1db, 0x1db, 0x349, 0x349, 0x7b4,
	0x6ae, 0x7a3, 0x7a3, 0x6fa, 0x6fa, 0x2d4, 0x2d4, 0x6ae, 0x117, 0x2fe, 0x2fe, 0x756, 0x756, 0x627, 0x627, 0x117,
	0x337, 0x55e, 0x55e, 0x2ef, 0x2ef, 0x2a9, 0x2a9, 0x337, 0x382, 0x02a, 0x02a, 0x4af, 0x4af, 0x7fa, 0x7fa, 0x382,
	0x73f, 0x6d8, 0x6d8, 0x7d2, 0x7d2, 0x52d, 0x52d, 0x73f, 0x385, 0x303, 0x303, 0x3eb, 0x3eb, 0x689, 0x689, 0x385,
	0x6cd, 0x4c3, 0x4c3, 0x2df, 0x2df, 0x1d5, 0x1d5, 0x6cd, 0x7da, 0x44c, 0x44c, 0x764, 0x764, 0x7f6, 0x7f6, 0x7da,
	0x7c1, 0x276, 0x276, 0x27f, 0x27f, 0x1c9, 0x1c9, 0x7c1, 0x58c, 0x001, 0x001, 0x07f, 0x07f, 0x62b, 0x62b, 0x58c,
	0x374, 0x746, 0x746, 0x5b2, 0x5b2, 0x35e, 0x35e, 0x374, 0x64d, 0x1fd, 0x1fd, 0x2dc, 0x2dc, 0x550, 0x550, 0x64d,
	0x33d, 0x40c, 0x40c, 0x3a4, 0x3a4, 0x293, 0x293, 0x33d, 0x0c0, 0x462, 0x462, 0x6fb, 0x6fb, 0x4b0, 0x4b0, 0x0c0,
	0x2aa, 0x175, 0x175, 0x01e, 0x01e, 0x762, 0x762, 0x2aa, 0x216, 0x76b, 0x76b, 0x728, 0x728, 0x15f, 0x15f, 0x216,
	0x30e, 0x0d8, 0x0d8, 0x55c, 0x55c, 0x31c, 0x31c, 0x30e, 0x351, 0x3dc, 0x3dc, 0x6c1, 0x6c1, 0x651, 0x651, 0x351,
};

static UINT32 partial_carry_sum(UINT32 add1,UINT32 add2,UINT32 carry_mask,INT32 bits)
{
	INT32 i,res,carry;

	res = 0;
	carry = 0;
	for (i = 0;i < bits;i++)
	{
		INT32 bit = BIT(add1,i) + BIT(add2,i) + carry;

		res += (bit & 1) << i;

		// generate carry only if the corresponding bit in carry_mask is 1
		if (BIT(carry_mask,i))
			carry = bit >> 1;
		else
			carry = 0;
	}

	// wrap around carry from top bit to bit 0
	if (carry)
		res ^=1;

	return res;
}

static UINT32 partial_carry_sum32(UINT32 add1,UINT32 add2,UINT32 carry_mask)
{
	return partial_carry_sum(add1,add2,carry_mask,32);
}

static UINT32 yrot(UINT32 v, INT32 r)
{
	return (v << r) | (v >> (32-r));
}

static UINT16 gm(INT32 i4)
{
	UINT16 x=0;

	for (INT32 i=0; i<4; ++i)
	{
		if (BIT(i4,i))
			x ^= 0xf << (i<<2);
	}

	return x;
}

static UINT32 core_decrypt(UINT32 ciphertext, INT32 i1, INT32 i2, INT32 i3, INT32 i4,
			const UINT8 *rotate, const UINT8 *x5, const UINT16 *x11, UINT32 preXor, UINT32 carryMask, UINT32 postXor)
{
	UINT32 v1 = BITSWAP32(yrot(ciphertext, rotate[i1]), 25,28,15,19, 6,0,3,24, 11,1,2,30, 16,7,22,17, 31,14,23,9, 27,18,4,10, 13,20,5,12, 8,29,26,21);

	UINT16 x1Low = (x5[i2]<<11) ^ x11[i3] ^ gm(i4);
	UINT32 x1 = x1Low | (BITSWAP16(x1Low, 0,8,1,9, 2,10,3,11, 4,12,5,13, 6,14,7,15)<<16);

	return partial_carry_sum32(v1, x1^preXor, carryMask) ^ postXor;
}

static void raiden2_decrypt_sprites()
{
	UINT32 *data = (UINT32 *)DrvGfxROM2;

	for(INT32 i=0; i<0x800000/4; i++)
	{
		data[i] = core_decrypt(data[i],	(i&0xff) ^ BIT(i,15) ^ (BIT(i,20)<<8), (i&0xff) ^ BIT(i,15),
			(i>>8) & 0xff, (i>>16) & 0xf, rotate_r2, x5_r2, x11_r2, 0x60860000, 0x176c91a8, 0x0f488000);
	}
}

static void zeroteam_decrypt_sprites()
{
	UINT32 *data = (UINT32 *)DrvGfxROM2;

	for(INT32 i=0; i<0x400000/4; i++)
	{
		data[i] = core_decrypt(data[i], i & 0xff, i & 0xff, (i>>7) & 0x1ff, (i>>16) & 0xf,
			rotate_zt, x5_zt, x11_zt, 0xa5800000, 0x7b67b7b9, 0xf1412ea8
		);
	}
}

static void raiden2_common_map()
{
	VezInit(0, V30_TYPE);
	VezOpen(0);
//	VezMapArea(0x00000, 0x007ff, 0, DrvMainRAM);
//	VezMapArea(0x00000, 0x007ff, 1, DrvMainRAM); // handler
	VezMapArea(0x00000, 0x007ff, 2, DrvMainRAM); // fetch (map shift is 11 bits (800))
	VezMapArea(0x00800, 0x0bfff, 0, DrvMainRAM + 0x00800);
	VezMapArea(0x00800, 0x0bfff, 1, DrvMainRAM + 0x00800);
	VezMapArea(0x00800, 0x0bfff, 2, DrvMainRAM + 0x00800);
	VezMapArea(0x0c000, 0x0cfff, 0, DrvSprRAM);
	VezMapArea(0x0c000, 0x0cfff, 1, DrvSprRAM);
	VezMapArea(0x0c000, 0x0cfff, 2, DrvSprRAM);
	VezMapArea(0x0d000, 0x0d7ff, 0, DrvBgRAM);
	VezMapArea(0x0d000, 0x0d7ff, 1, DrvBgRAM);
	VezMapArea(0x0d000, 0x0d7ff, 2, DrvBgRAM);
	VezMapArea(0x0d800, 0x0dfff, 0, DrvFgRAM);
	VezMapArea(0x0d800, 0x0dfff, 1, DrvFgRAM);
	VezMapArea(0x0d800, 0x0dfff, 2, DrvFgRAM);
	VezMapArea(0x0e000, 0x0e7ff, 0, DrvMgRAM);
	VezMapArea(0x0e000, 0x0e7ff, 1, DrvMgRAM);
	VezMapArea(0x0e000, 0x0e7ff, 2, DrvMgRAM);
	VezMapArea(0x0e800, 0x0f7ff, 0, DrvTxRAM);
	VezMapArea(0x0e800, 0x0f7ff, 1, DrvTxRAM);
	VezMapArea(0x0e800, 0x0f7ff, 2, DrvTxRAM);
	VezMapArea(0x0f800, 0x1efff, 0, DrvMainRAM + 0x0f800);
	VezMapArea(0x0f800, 0x1efff, 1, DrvMainRAM + 0x0f800);
	VezMapArea(0x0f800, 0x1efff, 2, DrvMainRAM + 0x0f800);
	VezMapArea(0x1f000, 0x1ffff, 0, DrvPalRAM);
//	VezMapArea(0x1f000, 0x1ffff, 1, DrvPalRAM); // handler
	VezMapArea(0x1f000, 0x1ffff, 2, DrvPalRAM);
	VezMapArea(0x20000, 0xfffff, 0, DrvMainROM + 0x20000);
	VezMapArea(0x20000, 0xfffff, 2, DrvMainROM + 0x20000);
	VezSetWriteHandler(raiden2_main_write);
	VezSetReadHandler(raiden2_main_read);
	VezClose();
}

static void zeroteam_common_map()
{
	VezInit(0, V30_TYPE);
	VezOpen(0);
//	VezMapArea(0x00000, 0x007ff, 0, DrvMainRAM);
//	VezMapArea(0x00000, 0x007ff, 1, DrvMainRAM); // handler
	VezMapArea(0x00000, 0x007ff, 2, DrvMainRAM); // fetch (map shift is 11 bits (800))
	VezMapArea(0x00800, 0x0b7ff, 0, DrvMainRAM + 0x00800);
	VezMapArea(0x00800, 0x0b7ff, 1, DrvMainRAM + 0x00800);
	VezMapArea(0x00800, 0x0b7ff, 2, DrvMainRAM + 0x00800);
	VezMapArea(0x0b800, 0x0bfff, 0, DrvBgRAM);
	VezMapArea(0x0b800, 0x0bfff, 1, DrvBgRAM);
	VezMapArea(0x0b800, 0x0bfff, 2, DrvBgRAM);
	VezMapArea(0x0c000, 0x0c7ff, 0, DrvFgRAM);
	VezMapArea(0x0c000, 0x0c7ff, 1, DrvFgRAM);
	VezMapArea(0x0c000, 0x0c7ff, 2, DrvFgRAM);
	VezMapArea(0x0c800, 0x0cfff, 0, DrvMgRAM);
	VezMapArea(0x0c800, 0x0cfff, 1, DrvMgRAM);
	VezMapArea(0x0c800, 0x0cfff, 2, DrvMgRAM);
	VezMapArea(0x0d000, 0x0dfff, 0, DrvTxRAM);
	VezMapArea(0x0d000, 0x0dfff, 1, DrvTxRAM);
	VezMapArea(0x0d000, 0x0dfff, 2, DrvTxRAM);
	VezMapArea(0x0e000, 0x0efff, 0, DrvPalRAM);
//	VezMapArea(0x0e000, 0x0efff, 1, DrvPalRAM); // handler
	VezMapArea(0x0e000, 0x0efff, 2, DrvPalRAM);
	VezMapArea(0x0f000, 0x0ffff, 0, DrvSprRAM);
	VezMapArea(0x0f000, 0x0ffff, 1, DrvSprRAM);
	VezMapArea(0x0f000, 0x0ffff, 2, DrvSprRAM);
	VezMapArea(0x10000, 0x1ffff, 0, DrvMainRAM + 0x10000);
	VezMapArea(0x10000, 0x1ffff, 1, DrvMainRAM + 0x10000);
	VezMapArea(0x10000, 0x1ffff, 2, DrvMainRAM + 0x10000);
	VezMapArea(0x20000, 0xfffff, 0, DrvMainROM + 0x020000);
	VezMapArea(0x20000, 0xfffff, 2, DrvMainROM + 0x020000);
	VezSetWriteHandler(zeroteam_main_write);
	VezSetReadHandler(raiden2_main_read);
	VezClose();
}

static INT32 Raiden2Init()
{
	game_select = 0;

	BurnSetRefreshRate(55.47);

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvMainROM + 0x000000,  0, 2)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x000001,  1, 2)) return 1;

	//	if (BurnLoadRom(DrvCopxROM + 0x000000,  2, 1)) return 1;

		if (BurnLoadRom(SeibuZ80ROM + 0x00000,  3, 1)) return 1;
		memcpy (SeibuZ80ROM + 0x10000, SeibuZ80ROM + 0x08000, 0x08000);
		memcpy (SeibuZ80ROM + 0x18000, SeibuZ80ROM + 0x00000, 0x08000);
		memset (SeibuZ80ROM + 0x08000, 0xff, 0x08000);

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x200000,  6, 1)) return 1;

		memset (DrvGfxROM2, 0xff, 0x800000);
		if (BurnLoadRom(DrvGfxROM2 + 0x000000,  7, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x000001,  8, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x400000,  9, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x400001, 10, 2)) return 1;

		for (INT32 i = 0; i < 0x800000; i+=4) {
			BurnByteswap(DrvGfxROM2 + i + 1, 2);
		}

		if (BurnLoadRom(DrvSndROM0 + 0x000000, 11, 1)) return 1;

		if (BurnLoadRom(DrvSndROM1 + 0x000000, 12, 1)) return 1;

		raiden2_decrypt_sprites();
		DrvGfxDecode();
		DrvCreateTransTab();
		DrvCreateAlphaTable(1);
	}

	raiden2_common_map();

	seibu_sound_init(1|4, 0, 3579545, 3579545, 1022727 / 132);
	BurnYM2151SetAllRoutes(1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 Raiden2aInit() // alternate rom layout
{
	game_select = 0;

	BurnSetRefreshRate(55.47);

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvMainROM + 0x000000,  0, 4)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x000001,  1, 4)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x000002,  2, 4)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x000003,  3, 4)) return 1;
		
	//	if (BurnLoadRom(DrvCopxROM + 0x000000,  4, 1)) return 1;

		if (BurnLoadRom(SeibuZ80ROM + 0x00000,  5, 1)) return 1;
		memcpy (SeibuZ80ROM + 0x10000, SeibuZ80ROM + 0x08000, 0x08000);
		memcpy (SeibuZ80ROM + 0x18000, SeibuZ80ROM + 0x00000, 0x08000);
		memset (SeibuZ80ROM + 0x08000, 0xff, 0x08000);

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  6, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x200000,  8, 1)) return 1;

		memset (DrvGfxROM2, 0xff, 0x800000);
		if (BurnLoadRom(DrvGfxROM2 + 0x000000,  9, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x000001, 10, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x400000, 11, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x400001, 12, 2)) return 1;

		for (INT32 i = 0; i < 0x800000; i+=4) {
			BurnByteswap(DrvGfxROM2 + i + 1, 2);
		}

		if (BurnLoadRom(DrvSndROM0 + 0x000000, 13, 1)) return 1;

		if (BurnLoadRom(DrvSndROM1 + 0x000000, 14, 1)) return 1;

		raiden2_decrypt_sprites();
		DrvGfxDecode();
		DrvCreateTransTab();
		DrvCreateAlphaTable(1);
	}

	raiden2_common_map();

	seibu_sound_init(1|4, 0, 3579545, 3579545, 1022727 / 132);
	BurnYM2151SetAllRoutes(1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 RaidendxInit()
{
	game_select = 1;

	BurnSetRefreshRate(55.47);

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvMainROM + 0x000000,  0, 4)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x000001,  1, 4)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x000002,  2, 4)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x000003,  3, 4)) return 1;
		
	//	if (BurnLoadRom(DrvCopxROM + 0x000000,  4, 1)) return 1; // skip for now

		if (BurnLoadRom(SeibuZ80ROM + 0x00000,  5, 1)) return 1;
		memcpy (SeibuZ80ROM + 0x10000, SeibuZ80ROM + 0x08000, 0x08000);
		memcpy (SeibuZ80ROM + 0x18000, SeibuZ80ROM + 0x00000, 0x08000);
		memset (SeibuZ80ROM + 0x08000, 0xff, 0x08000);

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  6, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x200000,  8, 1)) return 1;

		memset (DrvGfxROM2, 0xff, 0x800000);
		if (BurnLoadRom(DrvGfxROM2 + 0x000000,  9, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x000001, 10, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x400000, 11, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x400001, 12, 2)) return 1;

		for (INT32 i = 0; i < 0x800000; i+=4) {
			BurnByteswap(DrvGfxROM2 + i + 1, 2);
		}

		if (BurnLoadRom(DrvSndROM0 + 0x000000, 13, 1)) return 1;

		if (BurnLoadRom(DrvSndROM1 + 0x000000, 14, 1)) return 1;

		raiden2_decrypt_sprites();
		DrvGfxDecode();
		DrvCreateTransTab();
		DrvCreateAlphaTable(1);
	}

	VezInit(0, V30_TYPE);
	VezOpen(0);
//	VezMapArea(0x00000, 0x007ff, 0, DrvMainRAM);
//	VezMapArea(0x00000, 0x007ff, 1, DrvMainRAM); // handler
	VezMapArea(0x00000, 0x007ff, 2, DrvMainRAM); // fetch (map shift is 11 bits (800))
	VezMapArea(0x00800, 0x0bfff, 0, DrvMainRAM + 0x00800);
	VezMapArea(0x00800, 0x0bfff, 1, DrvMainRAM + 0x00800);
	VezMapArea(0x00800, 0x0bfff, 2, DrvMainRAM + 0x00800);
	VezMapArea(0x0c000, 0x0cfff, 0, DrvSprRAM);
	VezMapArea(0x0c000, 0x0cfff, 1, DrvSprRAM);
	VezMapArea(0x0c000, 0x0cfff, 2, DrvSprRAM);
	VezMapArea(0x0d000, 0x0d7ff, 0, DrvBgRAM);
	VezMapArea(0x0d000, 0x0d7ff, 1, DrvBgRAM);
	VezMapArea(0x0d000, 0x0d7ff, 2, DrvBgRAM);
	VezMapArea(0x0d800, 0x0dfff, 0, DrvFgRAM);
	VezMapArea(0x0d800, 0x0dfff, 1, DrvFgRAM);
	VezMapArea(0x0d800, 0x0dfff, 2, DrvFgRAM);
	VezMapArea(0x0e000, 0x0e7ff, 0, DrvMgRAM);
	VezMapArea(0x0e000, 0x0e7ff, 1, DrvMgRAM);
	VezMapArea(0x0e000, 0x0e7ff, 2, DrvMgRAM);
	VezMapArea(0x0e800, 0x0f7ff, 0, DrvTxRAM);
	VezMapArea(0x0e800, 0x0f7ff, 1, DrvTxRAM);
	VezMapArea(0x0e800, 0x0f7ff, 2, DrvTxRAM);
	VezMapArea(0x0f800, 0x1efff, 0, DrvMainRAM + 0x0f800);
	VezMapArea(0x0f800, 0x1efff, 1, DrvMainRAM + 0x0f800);
	VezMapArea(0x0f800, 0x1efff, 2, DrvMainRAM + 0x0f800);
	VezMapArea(0x1f000, 0x1ffff, 0, DrvPalRAM);
//	VezMapArea(0x1f000, 0x1ffff, 1, DrvPalRAM); // handler
	VezMapArea(0x1f000, 0x1ffff, 2, DrvPalRAM);
	VezMapArea(0x20000, 0x2ffff, 0, DrvMainROM + 0x100000);
	VezMapArea(0x20000, 0x2ffff, 2, DrvMainROM + 0x100000);
	VezMapArea(0x30000, 0xfffff, 0, DrvMainROM + 0x030000);
	VezMapArea(0x30000, 0xfffff, 2, DrvMainROM + 0x030000);
	VezSetWriteHandler(raidendx_main_write);
	VezSetReadHandler(raiden2_main_read);
	VezClose();

	seibu_sound_init(1|4, 0, 3579545, 3579545, 1022727 / 132);
	BurnYM2151SetAllRoutes(1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 ZeroteamInit()
{
	game_select = 2;

	BurnSetRefreshRate(55.47);

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvMainROM + 0x000000,  0, 4)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x000002,  1, 4)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x000001,  2, 4)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x000003,  3, 4)) return 1;

	//	if (BurnLoadRom(DrvCopxROM + 0x000000,  4, 1)) return 1;

		if (BurnLoadRom(SeibuZ80ROM + 0x00000,  5, 1)) return 1;
		memcpy (SeibuZ80ROM + 0x10000, SeibuZ80ROM + 0x08000, 0x08000);
		memcpy (SeibuZ80ROM + 0x18000, SeibuZ80ROM + 0x00000, 0x08000);
		memset (SeibuZ80ROM + 0x08000, 0xff, 0x08000);

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  6, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000001,  7, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x100000,  9, 1)) return 1;

		memset (DrvGfxROM2, 0xff, 0x800000);
		if (BurnLoadRom(DrvGfxROM2 + 0x000000, 10, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x000001, 11, 2)) return 1;

		for (INT32 i = 0; i < 0x400000; i+=4) {
			BurnByteswap(DrvGfxROM2 + i + 1, 2);
		}

		if (BurnLoadRom(DrvSndROM0 + 0x000000, 12, 1)) return 1;

		zeroteam_decrypt_sprites();
		DrvGfxDecode();
		DrvCreateTransTab();
		DrvCreateAlphaTable(0);
	}

	zeroteam_common_map();

	seibu_sound_init(0, 0, 3579545, 3579545, 1320000 / 132);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 XsedaeInit()
{
	game_select = 3;

	BurnSetRefreshRate(55.47);

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvMainROM + 0x000000,  0, 4)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x000001,  1, 4)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x000002,  2, 4)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x000003,  3, 4)) return 1;
		
	//	no cop mcu

		if (BurnLoadRom(SeibuZ80ROM + 0x00000,  4, 1)) return 1;
		memcpy (SeibuZ80ROM + 0x10000, SeibuZ80ROM + 0x08000, 0x08000);
		memcpy (SeibuZ80ROM + 0x18000, SeibuZ80ROM + 0x00000, 0x08000);
		memset (SeibuZ80ROM + 0x08000, 0xff, 0x08000);

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  5, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000001,  6, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x100000,  8, 1)) return 1;

		memset (DrvGfxROM2, 0xff, 0x800000);
		if (BurnLoadRom(DrvGfxROM2 + 0x000000,  9, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x000001, 10, 2)) return 1;

		for (INT32 i = 0; i < 0x400000; i+=4) {
			BurnByteswap(DrvGfxROM2 + i + 1, 2);
		}

		if (BurnLoadRom(DrvSndROM0 + 0x000000, 11, 1)) return 1;

		memset (DrvSndROM1, 0xff, 0x40000);

		DrvGfxDecode();
		DrvCreateTransTab();
		DrvCreateAlphaTable(0);
	}

	zeroteam_common_map();

	seibu_sound_init(1|4, 0, 3579545, 3579545, 1022727 / 132);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 R2dxInit()
{
	game_select = 4;

	BurnSetRefreshRate(55.47);

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvMainROM + 0x000000,  0, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  1, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  2, 1)) return 1;

		if (BurnLoadRomExt(DrvGfxROM2 + 0x000,  3, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM2 + 0x002,  4, 4, LD_GROUP(2))) return 1;

		if (BurnLoadRom(DrvSndROM1 + 0x000000,  5, 1)) return 1;

		if (BurnLoadRom(DrvCopxROM + 0x000000,  6, 1)) return 1;

		if (BurnLoadRom(DrvEeprom  + 0x000000,  7, 1)) return 1;

		raiden2_decrypt_sprites();
		DrvGfxDecode();
		DrvCreateTransTab();
		DrvCreateAlphaTable(1);
	}

	VezInit(0, V33_TYPE);
	VezOpen(0);
//	VezMapArea(0x00000, 0x007ff, 0, DrvMainRAM);
//	VezMapArea(0x00000, 0x007ff, 1, DrvMainRAM); // handler
	VezMapArea(0x00000, 0x1ffff, 2, DrvMainRAM + 0x00000);
	VezMapArea(0x00800, 0x1ffff, 0, DrvMainRAM + 0x00800);
	VezMapArea(0x00800, 0x1ffff, 1, DrvMainRAM + 0x00800);
	VezMapArea(0x0c000, 0x0cfff, 0, DrvSprRAM);
	VezMapArea(0x0c000, 0x0cfff, 1, DrvSprRAM);
	VezMapArea(0x0c000, 0x0cfff, 2, DrvSprRAM);
	VezMapArea(0x20000, 0x2ffff, 0, DrvMainROM + 0x100000);
	VezMapArea(0x20000, 0x2ffff, 2, DrvMainROM + 0x100000);
	VezMapArea(0x30000, 0xfffff, 0, DrvMainROM + 0x030000);
	VezMapArea(0x30000, 0xfffff, 2, DrvMainROM + 0x030000);
	VezSetWriteHandler(r2dx_main_write);
	VezSetReadHandler(r2dx_main_read);
	VezClose();

	MSM6295Init(0, 1022727 / 132, 0);
	MSM6295SetRoute(0, 0.50, BURN_SND_ROUTE_BOTH);

	EEPROMInit(&eeprom_interface_93C46);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 NzeroteamInit()
{
	game_select = 5;

	BurnSetRefreshRate(55.47);

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvMainROM + 0x000000,  0, 2)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x000001,  1, 2)) return 1;

		if (BurnLoadRom(DrvCopxROM + 0x000000,  2, 1)) return 1;

		if (BurnLoadRom(SeibuZ80ROM + 0x00000,  3, 1)) return 1;
		memcpy (SeibuZ80ROM + 0x10000, SeibuZ80ROM + 0x08000, 0x08000);
		memcpy (SeibuZ80ROM + 0x18000, SeibuZ80ROM + 0x00000, 0x08000);
		memset (SeibuZ80ROM + 0x08000, 0xff, 0x08000);

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  4, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000001,  5, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x100000,  7, 1)) return 1;

		if (BurnLoadRomExt(DrvGfxROM2 + 0x000,  8, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM2 + 0x002,  9, 4, LD_GROUP(2))) return 1;

		if (BurnLoadRom(DrvSndROM0 + 0x000000, 10, 1)) return 1;

		zeroteam_decrypt_sprites();
		DrvGfxDecode();
		DrvCreateTransTab();
		DrvCreateAlphaTable(0);
	}

	VezInit(0, V33_TYPE);
	VezOpen(0);
//	VezMapArea(0x00000, 0x007ff, 0, DrvMainRAM);
//	VezMapArea(0x00000, 0x007ff, 1, DrvMainRAM); // handler
	VezMapArea(0x00000, 0x1ffff, 2, DrvMainRAM + 0x00000);
	VezMapArea(0x00800, 0x1ffff, 0, DrvMainRAM + 0x00800);
	VezMapArea(0x00800, 0x1ffff, 1, DrvMainRAM + 0x00800);
	VezMapArea(0x0c000, 0x0cfff, 0, DrvSprRAM);
	VezMapArea(0x0c000, 0x0cfff, 1, DrvSprRAM);
	VezMapArea(0x0c000, 0x0cfff, 2, DrvSprRAM);
	VezMapArea(0x20000, 0xfffff, 0, DrvMainROM + 0x020000);
	VezMapArea(0x20000, 0xfffff, 2, DrvMainROM + 0x020000);
	VezSetWriteHandler(nzeroteam_main_write);
	VezSetReadHandler(nzeroteam_main_read);
	VezClose();

	seibu_sound_init(0, 0, 3579545, 3579545, 1320000 / 132);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static void zerotm2kGfxDescramble()
{
	UINT8 tmp[0x80];

	for (INT32 i = 0x100000; i < 0x180000; i+=0x80)
	{
		for (INT32 j = 0; j < 0x80; j++)
		{
			INT32 k = (j & 0x1f) | ((j&0x20)<<1) | ((j&0x40)>>1);

			tmp[j] = DrvGfxROM1[i+k];
		}

		memcpy (DrvGfxROM1 + i, tmp, 0x80);
	}
}

static INT32 Zerotm2kInit()
{
	game_select = 6;

	BurnSetRefreshRate(55.47);

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvMainROM + 0x000000,  0, 1)) return 1;

		if (BurnLoadRom(DrvCopxROM + 0x000000,  1, 1)) return 1;

		if (BurnLoadRom(SeibuZ80ROM + 0x00000,  2, 1)) return 1;
		memcpy (SeibuZ80ROM + 0x10000, SeibuZ80ROM + 0x08000, 0x08000);
		memcpy (SeibuZ80ROM + 0x18000, SeibuZ80ROM + 0x00000, 0x08000);
		memset (SeibuZ80ROM + 0x08000, 0xff, 0x08000);

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  3, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000001,  4, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x100000,  6, 1)) return 1;

		if (BurnLoadRomExt(DrvGfxROM2 + 0x000,  7, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM2 + 0x002,  8, 4, LD_GROUP(2))) return 1;

		if (BurnLoadRom(DrvSndROM0 + 0x000000,  9, 1)) return 1;

		zerotm2kGfxDescramble();
		//zeroteam_decrypt_sprites();
		DrvGfxDecode();
		DrvCreateTransTab();
		DrvCreateAlphaTable(0);
	}

	VezInit(0, V33_TYPE);
	VezOpen(0);
//	VezMapArea(0x00000, 0x007ff, 0, DrvMainRAM);
//	VezMapArea(0x00000, 0x007ff, 1, DrvMainRAM); // handler
	VezMapArea(0x00000, 0x1ffff, 2, DrvMainRAM + 0x00000);
	VezMapArea(0x00800, 0x1ffff, 0, DrvMainRAM + 0x00800);
	VezMapArea(0x00800, 0x1ffff, 1, DrvMainRAM + 0x00800);
	VezMapArea(0x0c000, 0x0cfff, 0, DrvSprRAM);
	VezMapArea(0x0c000, 0x0cfff, 1, DrvSprRAM);
	VezMapArea(0x0c000, 0x0cfff, 2, DrvSprRAM);
	VezMapArea(0x20000, 0xfffff, 0, DrvMainROM + 0x020000);
	VezMapArea(0x20000, 0xfffff, 2, DrvMainROM + 0x020000);
	VezSetWriteHandler(zerotm2k_main_write);
	VezSetReadHandler(zerotm2k_main_read);
	VezClose();

	seibu_sound_init(0, 0, 3579545, 3579545, 1320000 / 132);

	EEPROMInit(&eeprom_interface_93C46);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	VezExit();

	if (game_select != 4) {
		seibu_sound_exit();
	} else {
		MSM6295Exit(0);
	}

	if (game_select == 4 || game_select == 6) {
		EEPROMExit();
	}

	BurnFree (AllMem);

	game_select = 0;

	return 0;
}

static inline UINT32 alpha_blend(UINT32 d, UINT32 s)
{
	return (((((s & 0xff00ff) * 0x7f) + ((d & 0xff00ff) * 0x81)) & 0xff00ff00) +
		((((s & 0x00ff00) * 0x7f) + ((d & 0x00ff00) * 0x81)) & 0x00ff0000)) / 0x100;
}

static void draw_layer(UINT8 *ram, INT32 scr, UINT32 color_base, INT32 bank)
{
	UINT16 *vram = (UINT16*)ram;

	INT32 scrollx = (scroll[scr * 4 + 0] + (scroll[scr * 4 + 1] * 256)) & 0x1ff;
	INT32 scrolly = (scroll[scr * 4 + 2] + (scroll[scr * 4 + 3] * 256)) & 0x1ff;

	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sx = (offs & 0x1f) * 16;
		INT32 sy = (offs / 0x20) * 16;

		sx -= scrollx;
		if (sx < -15) sx += 512;
		sy -= scrolly;
		if (sy < -15) sy += 512;

		if (sy >= nScreenHeight || sx >= nScreenWidth) continue;

		INT32 attr = vram[offs];
		INT32 color = attr >> 12;
		INT32 code = (attr & 0xfff) + (bank << 12);

		if (DrvTransTab[code]) continue;

		{
			color *= 16;
			color += color_base;
			UINT32 *pal = DrvPalette + color;
			UINT8 *gfx = DrvGfxROM1 + (code * 0x100);
			UINT8 *alpha = DrvAlphaTable + color;
			UINT32 *dst = bitmap32 + sy * nScreenWidth;

			for (INT32 y = 0; y < 16; y++, sy++) {
				if (sy >= 0 && sy < nScreenHeight) {
					for (INT32 x = 0; x < 16; x++, sx++) {
						if (sx < 0 || sx >= nScreenWidth) continue;

						INT32 pxl = gfx[y*16+x];

						if (pxl != 0x0f)
						{
							if (alpha[pxl]) {
								dst[sx] = alpha_blend(dst[sx], pal[pxl]);
							} else {
								dst[sx] = pal[pxl];
							}
						}	
					}
					sx -= 16;
				}
				dst += nScreenWidth;
			}
		}
	}
}

static void draw_txt_layer()
{
	UINT16 *vram = (UINT16*)DrvTxRAM;

	for (INT32 offs = 0; offs < 64 * 32; offs++)
	{
		INT32 sx = (offs & 0x3f) * 8;
		INT32 sy = (offs / 0x40) * 8;

		if (sy >= nScreenHeight || sx >= nScreenWidth) continue;

		INT32 attr = vram[offs];
		INT32 color = attr >> 12;
		INT32 code = (attr & 0xfff) + (tx_bank * 0x1000);

		if ((code & 0xfff) <= 0x0020) continue;

		{
			color *= 16;
			color += 0x700;
			UINT32 *pal = DrvPalette + color;
			UINT8 *gfx = DrvGfxROM0 + (code * 0x40);
			UINT8 *alpha = DrvAlphaTable + color;
			UINT32 *dst = bitmap32 + sy * nScreenWidth;

			for (INT32 y = 0; y < 8; y++, sy++) {
				if (sy >= 0 && sy < nScreenHeight) {
					for (INT32 x = 0; x < 8; x++, sx++) {
						if (sx < 0 || sx >= nScreenWidth) continue;

						INT32 pxl = gfx[y*8+x];
						if (pxl != 0x0f)
						{
							if (alpha[pxl]) {
								dst[sx] = alpha_blend(dst[sx], pal[pxl]);
							} else {
								dst[sx] = pal[pxl];
							}
						}	
					}
					sx -= 8;
				}
				dst += nScreenWidth;
			}
		}
	}
}

static void draw_single_sprite(INT32 code, INT32 color, INT32 sx, INT32 sy, INT32 flipy, INT32 flipx)
{
	if (sx < -15 || sy < -15 || sy >= nScreenHeight || sx >= nScreenWidth) return;

	{
		INT32 flip = (flipy ? 0xf0 : 0) + (flipx ? 0x0f : 0);
		color *= 16;
		UINT32 *pal = DrvPalette + color;
		UINT8 *gfx = DrvGfxROM2 + (code * 0x100);
		UINT8 *alpha = DrvAlphaTable + color;
		UINT32 *dst = bitmap32 + sy * nScreenWidth;

		for (INT32 y = 0; y < 16; y++, sy++) {
			if (sy >= 0 && sy < nScreenHeight) {
				for (INT32 x = 0; x < 16; x++, sx++) {
					if (sx < 0 || sx >= nScreenWidth) continue;

					INT32 pxl = gfx[(y*16+x)^flip];
					if (pxl != 0x0f)
					{
						if (alpha[pxl]) {
							dst[sx] = alpha_blend(dst[sx], pal[pxl]);
						} else {
							dst[sx] = pal[pxl];
						}
					}	
				}
				sx -= 16;
			}
			dst += nScreenWidth;
		}
	}
}

static void draw_sprites(INT32 priority)
{
	if (layer_enable & 0x10) return;

	UINT16 *sprites = (UINT16*)DrvSprRAM;
	UINT16 *source = sprites + sprites_cur_start/2;

	while( source >= sprites ){
		INT32 tile_number = source[1];
		INT32 sx = source[2];
		INT32 sy = source[3];
		INT32 colr;
		INT32 xtiles, ytiles;
		INT32 ytlim, xtlim;
		INT32 xflip, yflip;
		INT32 xstep, ystep;
		INT32 pri;

		ytlim = (source[0] >> 12) & 0x7;
		xtlim = (source[0] >> 8 ) & 0x7;

		xflip = (source[0] >> 15) & 0x1;
		yflip = (source[0] >> 11) & 0x1;

		colr = source[0] & 0x3f;

		pri = (source[0] >> 6) & 3;

		if (pri != priority) {
			source -= 4;
			continue;
		}
		
		ytlim += 1;
		xtlim += 1;

		xstep = 16;
		ystep = 16;

		if (xflip)
		{
			ystep = -16;
			sy += ytlim*16-16;
		}

		if (yflip)
		{
			xstep = -16;
			sx += xtlim*16-16;
		}

		for (xtiles = 0; xtiles < xtlim; xtiles++)
		{
			for (ytiles = 0; ytiles < ytlim; ytiles++)
			{
				draw_single_sprite(tile_number, colr, (sx+xstep*xtiles)&0x1ff,(sy+ystep*ytiles)&0x1ff, xflip, yflip);
				draw_single_sprite(tile_number, colr, ((sx+xstep*xtiles)&0x1ff)-0x200,(sy+ystep*ytiles)&0x1ff, xflip, yflip);
				draw_single_sprite(tile_number, colr, (sx+xstep*xtiles)&0x1ff,((sy+ystep*ytiles)&0x1ff)-0x200, xflip, yflip);
				draw_single_sprite(tile_number, colr, ((sx+xstep*xtiles)&0x1ff)-0x200,((sy+ystep*ytiles)&0x1ff)-0x200, xflip, yflip);

				tile_number++;
			}
		}

		source -= 4;
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x1000; i+=2) {
			palette_update_entry(i);
		}
		DrvRecalc = 0;
	}

//	BurnTransferClear();
	for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
		pTransDraw[i] = 0x0800;
		bitmap32[i] = 0; // black
	}

	if (nBurnLayer & 1) draw_sprites(0);
	if (nSpriteEnable & 1) if (~layer_enable & 1) draw_layer(DrvBgRAM, 0, 0x400, bg_bank);
	if (nBurnLayer & 2) draw_sprites(1);
	if (nSpriteEnable & 2) if (~layer_enable & 2) draw_layer(DrvMgRAM, 1, 0x600, mg_bank);
	if (nBurnLayer & 4) draw_sprites(2);
	if (nSpriteEnable & 4) if (~layer_enable & 4) draw_layer(DrvFgRAM, 2, 0x500, fg_bank);
	if (nBurnLayer & 8) draw_sprites(3);
	if (nSpriteEnable & 8) if (~layer_enable & 8) draw_txt_layer();

	for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
		PutPix(pBurnDraw + (i * nBurnBpp), BurnHighCol(bitmap32[i]>>16, (bitmap32[i]>>8)&0xff, bitmap32[i]&0xff, 0));
	}
	return 0;
}

static INT32 ZeroteamDraw() // sprite priorities different
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x1000; i+=2) {
			palette_update_entry(i);
		}
		DrvRecalc = 0;
	}

//	BurnTransferClear();
	for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
		pTransDraw[i] = 0x0800;
		bitmap32[i] = 0; // black
	}

	if (nSpriteEnable & 1) if (~layer_enable & 1) draw_layer(DrvBgRAM, 0, 0x400, bg_bank);
	if (nBurnLayer & 1) draw_sprites(0);
	if (nSpriteEnable & 2) if (~layer_enable & 2) draw_layer(DrvMgRAM, 1, 0x600, mg_bank);
	if (nBurnLayer & 2) draw_sprites(1);
	if (nSpriteEnable & 4) if (~layer_enable & 4) draw_layer(DrvFgRAM, 2, 0x500, fg_bank);
	if (nBurnLayer & 4) draw_sprites(2);
	if (nSpriteEnable & 8) if (~layer_enable & 8) draw_txt_layer();
	if (nBurnLayer & 8) draw_sprites(3);

	for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
		PutPix(pBurnDraw + (i * nBurnBpp), BurnHighCol(bitmap32[i]>>16, (bitmap32[i]>>8)&0xff, bitmap32[i]&0xff, 0));
	}

	return 0;
}
static UINT32 framecntr = 0;
static UINT32 seibu_start = 0;

static void compile_inputs()
{
	memset (DrvInputs, 0xff, 3*sizeof(short));
	for (INT32 i = 0; i < 16; i++) {
		DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
		DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
	}

	DrvInputs[2] = (DrvInputs[2] & 0x00ff) | (DrvDips[2] << 8);

	// hold coin down for a few frames so that it registers
	static INT32 previous_coin = seibu_coin_input;
	seibu_coin_input = 0xff;

	for (INT32 i = 0; i < 4; i++) {
		if ((previous_coin & (1 << i)) == 0 && DrvJoy4[i]) {
			hold_coin[i] = 4;
			framecntr = 0;
		}

		if (hold_coin[i]) { // only hold for 4 frames, with no extra from input holddown.
			hold_coin[i]--;
			if (framecntr & 1)
				seibu_start = 1;
			if (seibu_start)
				seibu_coin_input ^= (1 << i);
			if (!hold_coin[i])
				seibu_start = 0;
		}
	}
	//bprintf(0, _T("%X"), (seibu_coin_input == 0xff) ? 0 : seibu_coin_input);
	framecntr++;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	VezNewFrame();
	ZetNewFrame();

	compile_inputs();

	INT32 nInterleave = 128;
	INT32 nSoundBufferPos = 0;
	INT32 nCyclesTotal[2] = { (16000000 * 100) / 5547, (3579545 * 100) / 5547 };
	INT32 nCyclesDone[2] = { 0, 0 };

	ZetOpen(0);
	VezOpen(0);
	
	for (INT32 i = 0; i < nInterleave; i++)
	{
		INT32 nSegment = nCyclesTotal[0] / nInterleave;

		nCyclesDone[0] += VezRun(nSegment);
		if (i == (nInterleave-2)) VezSetIRQLineAndVector(0, 0xc0/4, CPU_IRQSTATUS_AUTO);

		nSegment = (nCyclesTotal[1] / nInterleave) * (i+1);
		nCyclesDone[1] += ZetRun(nSegment - ZetTotalCycles());

		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			seibu_sound_update(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			seibu_sound_update(pSoundBuf, nSegmentLength);
		}
	}
	
	VezClose();
	ZetClose();

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 ZeroteamFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	VezNewFrame();
	ZetNewFrame();

	compile_inputs();

	INT32 nInterleave = 128;
	INT32 nCyclesTotal[2] = { (16000000 * 100) / 5547, (3579545 * 100) / 5547 };
	INT32 nCyclesDone[2] = { 0, 0 };

	ZetOpen(0);
	VezOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		INT32 nSegment = nCyclesTotal[0] / nInterleave;

		nCyclesDone[0] += VezRun(nSegment);
		if (i == (nInterleave-2)) VezSetIRQLineAndVector(0, 0xc0/4, CPU_IRQSTATUS_AUTO);

		BurnTimerUpdateYM3812(i * (nCyclesTotal[1] / nInterleave));
	}

	BurnTimerEndFrameYM3812(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		seibu_sound_update(pBurnSoundOut, nBurnSoundLen);
	}

	VezClose();
	ZetClose();

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 R2dxFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, 3*sizeof(short));
		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

//	INT32 nCyclesTotal[1] = { (16000000 * 100) / 5547 };

	VezOpen(0);
	VezRun(288444 - 500);
	VezSetIRQLineAndVector(0, 0xc0/4, CPU_IRQSTATUS_AUTO);
	VezRun(500);
	VezClose();

	if (pBurnSoundOut) {
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction,INT32 *pnMin)
{
	if ( pnMin ) *pnMin =  0x029671;

	struct BurnArea ba;

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data   = AllRam;
		ba.nLen   = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		VezScan(nAction);

		if (game_select != 4) {
			seibu_sound_scan(pnMin, nAction);
		} else {
			MSM6295Scan(0, nAction);
		}

		SCAN_VAR(layer_enable);

		SCAN_VAR(prg_bank);
		SCAN_VAR(mg_bank);
		SCAN_VAR(bg_bank);
		SCAN_VAR(fg_bank);
		SCAN_VAR(r2dx_gameselect);
		SCAN_VAR(r2dx_okibank);

		SeibuCopScan(nAction);
	}

	if (nAction & ACB_WRITE) {
		VezOpen(0);
		if (game_select == 0) raiden2_bankswitch(prg_bank);
		if (game_select == 1) raidendx_bankswitch(prg_bank);
		if (game_select == 4) {
			r2dx_bankswitch(prg_bank, r2dx_gameselect);
			r2dx_okibankswitch();
		}
		VezClose();

		DrvRecalc = 1;
	}

	if (nAction & ACB_NVRAM) {
		if (game_select == 4 || game_select == 6) {
			EEPROMScan(nAction, pnMin);
		}
	}

	return 0;
}


// Raiden II (US, set 1)

static struct BurnRomInfo raiden2RomDesc[] = {
	{ "prg0.u0211",					0x080000, 0x09475ec4, 1 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "prg1.u0212",					0x080000, 0x4609b5f2, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "copx-d2.u0313",				0x040000, 0xa6732ff9, 2 | BRF_PRG | BRF_OPT }, //  2 COPX MCU data

	{ "snd.u1110",  				0x010000, 0xf51a28f9, 3 | BRF_PRG | BRF_ESS }, //  3 Z80 Code

	{ "seibu7.u0724",				0x020000, 0xc9ec9469, 4 | BRF_GRA },           //  4 Characters

	{ "raiden_2_seibu_bg-1.u0714",	0x200000, 0xe61ad38e, 5 | BRF_GRA },           //  5 Tiles
	{ "raiden_2_seibu_bg-2.u075",	0x200000, 0xa694a4bb, 5 | BRF_GRA },           //  6

	{ "raiden_2_seibu_obj-1.u0811", 0x200000, 0xff08ef0b, 6 | BRF_GRA },           //  7 Sprites (Encrypted)
	{ "raiden_2_seibu_obj-2.u082", 	0x200000, 0x638eb771, 6 | BRF_GRA },           //  8
	{ "raiden_2_seibu_obj-3.u0837", 0x200000, 0x897a0322, 6 | BRF_GRA },           //  9
	{ "raiden_2_seibu_obj-4.u0836", 0x200000, 0xb676e188, 6 | BRF_GRA },           // 10

	{ "seibu6.u1017",				0x040000, 0xfb0fca23, 7 | BRF_SND },           // 11 OKI #0 Samples

	{ "raiden_2_pcm.u1018",			0x040000, 0x8cf0d17e, 8 | BRF_SND },           // 12 OKI #1 Samples

	{ "jj4b02__ami18cv8-15.u0342.jed", 		0x000288, 0x00000000, 0 | BRF_NODUMP },	       // 13 Pals
	{ "jj4b01__mmipal16l8bcn.u0341.jed",	0x000335, 0xd1a039af, 0 | BRF_OPT },	       // 14
};

STD_ROM_PICK(raiden2)
STD_ROM_FN(raiden2)

struct BurnDriver BurnDrvRaiden2 = {
	"raiden2", NULL, NULL, NULL, "1993",
	"Raiden II (US, set 1)\0", NULL, "Seibu Kaihatsu (Fabtek license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, raiden2RomInfo, raiden2RomName, NULL, NULL, Raiden2InputInfo, Raiden2DIPInfo,
	Raiden2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Raiden II (US, set 2)

static struct BurnRomInfo raiden2uRomDesc[] = {
	{ "1.u0211",					0x080000, 0xb16df955, 1 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "2.u0212",					0x080000, 0x2a14b112, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "copx-d2.u0313",				0x040000, 0xa6732ff9, 2 | BRF_PRG | BRF_OPT }, //  2 COPX MCU data

	{ "seibu5.u1110",				0x010000, 0x6d362472, 3 | BRF_PRG | BRF_ESS }, //  3 Z80 Code

	{ "seibu7.u0724",				0x020000, 0xc7aa4d00, 4 | BRF_GRA },           //  4 Characters

	{ "raiden_2_seibu_bg-1.u0714",	0x200000, 0xe61ad38e, 5 | BRF_GRA },           //  5 Tiles
	{ "raiden_2_seibu_bg-2.u075",	0x200000, 0xa694a4bb, 5 | BRF_GRA },           //  6

	{ "raiden_2_seibu_obj-1.u0811",	0x200000, 0xff08ef0b, 6 | BRF_GRA },           //  7 Sprites (Encrypted)
	{ "raiden_2_seibu_obj-2.u082",	0x200000, 0x638eb771, 6 | BRF_GRA },           //  8
	{ "raiden_2_seibu_obj-3.u0837",	0x200000, 0x897a0322, 6 | BRF_GRA },           //  9
	{ "raiden_2_seibu_obj-4.u0836",	0x200000, 0xb676e188, 6 | BRF_GRA },           // 10

	{ "seibu6.u1017",				0x040000, 0xfab9f8e4, 7 | BRF_SND },           // 11 OKI #0 Samples

	{ "raiden_2_pcm.u1018",			0x040000, 0x8cf0d17e, 8 | BRF_SND },           // 12 OKI #1 Samples

	{ "jj4b02__ami18cv8-15.u0342.jed",		0x000288, 0x00000000, 9 | BRF_NODUMP },        // 13 pals
	{ "jj4b01__mmipal16l8bcn.u0341.jed",	0x000335, 0xd1a039af, 0 | BRF_OPT },	       // 14
};

STD_ROM_PICK(raiden2u)
STD_ROM_FN(raiden2u)

struct BurnDriver BurnDrvRaiden2u = {
	"raiden2u", "raiden2", NULL, NULL, "1993",
	"Raiden II (US, set 2)\0", NULL, "Seibu Kaihatsu (Fabtek license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE  | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, raiden2uRomInfo, raiden2uRomName, NULL, NULL, Raiden2InputInfo, Raiden2DIPInfo,
	Raiden2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Raiden II (Hong Kong)

static struct BurnRomInfo raiden2hkRomDesc[] = {
	{ "prg0.u0211",					0x080000, 0x09475ec4, 1 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "rom2e.u0212",				0x080000, 0x458d619c, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "copx-d2.u0313",				0x040000, 0xa6732ff9, 2 | BRF_PRG | BRF_OPT }, //  2 COPX MCU data

	{ "seibu5.u1110",  				0x010000, 0x8f130589, 3 | BRF_PRG | BRF_ESS }, //  3 Z80 Code

	{ "seibu7.u0724",				0x020000, 0xc9ec9469, 4 | BRF_GRA },           //  4 Characters

	{ "raiden_2_seibu_bg-1.u0714",	0x200000, 0xe61ad38e, 5 | BRF_GRA },           //  5 Tiles
	{ "raiden_2_seibu_bg-2.u075",	0x200000, 0xa694a4bb, 5 | BRF_GRA },           //  6

	{ "raiden_2_seibu_obj-1.u0811",	0x200000, 0xff08ef0b, 6 | BRF_GRA },           //  7 Sprites (Encrypted)
	{ "raiden_2_seibu_obj-2.u082",  0x200000, 0x638eb771, 6 | BRF_GRA },           //  8
	{ "raiden_2_seibu_obj-3.u0837", 0x200000, 0x897a0322, 6 | BRF_GRA },           //  9
	{ "raiden_2_seibu_obj-4.u0836", 0x200000, 0xb676e188, 6 | BRF_GRA },           // 10

	{ "seibu6.u1017",				0x040000, 0xfb0fca23, 7 | BRF_SND },           // 11 OKI #0 Samples

	{ "raiden_2_pcm.u1018",			0x040000, 0x8cf0d17e, 8 | BRF_SND },           // 12 OKI #1 Samples

	{ "jj4b02__ami18cv8-15.u0342.jed", 		0x000288, 0x00000000, 0 | BRF_OPT },	       // 13 Pals
	{ "jj4b01__mmipal16l8bcn.u0341.jed",	0x000335, 0xd1a039af, 0 | BRF_OPT },	       // 14
};

STD_ROM_PICK(raiden2hk)
STD_ROM_FN(raiden2hk)

struct BurnDriver BurnDrvRaiden2hk = {
	"raiden2hk", "raiden2", NULL, NULL, "1993",
	"Raiden II (Hong Kong)\0", NULL, "Seibu Kaihatsu (Metrotainment license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, raiden2hkRomInfo, raiden2hkRomName, NULL, NULL, Raiden2InputInfo, Raiden2DIPInfo,
	Raiden2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Raiden II (Japan)

static struct BurnRomInfo raiden2jRomDesc[] = {
	{ "prg0.u0211",					0x080000, 0x09475ec4, 1 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "rom2j.u0212",				0x080000, 0xe4e4fb4c, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "copx-d2.u0313",				0x040000, 0xa6732ff9, 2 | BRF_PRG | BRF_ESS }, //  2 COPX MCU data

	{ "seibu5.u1110",				0x010000, 0x8f130589, 3 | BRF_PRG | BRF_ESS }, //  3 Z80 Code

	{ "seibu7.u0724",				0x020000, 0xc9ec9469, 4 | BRF_GRA },           //  4 Characters

	{ "raiden_2_seibu_bg-1.u0714",	0x200000, 0xe61ad38e, 5 | BRF_GRA },           //  5 Tiles
	{ "raiden_2_seibu_bg-2.u075",	0x200000, 0xa694a4bb, 5 | BRF_GRA },           //  6

	{ "raiden_2_seibu_obj-1.u0811",	0x200000, 0xff08ef0b, 6 | BRF_GRA },           //  7 Sprites (Encrypted)
	{ "raiden_2_seibu_obj-2.u082",	0x200000, 0x638eb771, 6 | BRF_GRA },           //  8
	{ "raiden_2_seibu_obj-3.u0837",	0x200000, 0x897a0322, 6 | BRF_GRA },           //  9
	{ "raiden_2_seibu_obj-4.u0836",	0x200000, 0xb676e188, 6 | BRF_GRA },           // 10

	{ "seibu6.u1017",				0x040000, 0xfb0fca23, 7 | BRF_SND },           // 11 OKI #0 Samples

	{ "raiden_2_pcm.u1018",			0x040000, 0x8cf0d17e, 8 | BRF_SND },           // 12 OKI #1 Samples

	{ "jj4b02__ami18cv8-15.u0342.jed",		0x000288, 0x00000000, 9 | BRF_NODUMP },        // 13 pals
	{ "jj4b01__mmipal16l8bcn.u0341.jed",	0x000335, 0xd1a039af, 0 | BRF_OPT },	       // 14
};

STD_ROM_PICK(raiden2j)
STD_ROM_FN(raiden2j)

struct BurnDriver BurnDrvRaiden2j = {
	"raiden2j", "raiden2", NULL, NULL, "1993",
	"Raiden II (Japan)\0", NULL, "Seibu Kaihatsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE  | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, raiden2jRomInfo, raiden2jRomName, NULL, NULL, Raiden2InputInfo, Raiden2DIPInfo,
	Raiden2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Raiden II (Italy)

static struct BurnRomInfo raiden2iRomDesc[] = {
	{ "seibu1.u0211",				0x080000, 0xc1fc70f5, 1 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "seibu2.u0212",				0x080000, 0x28d5365f, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "copx-d2.u0313",				0x040000, 0xa6732ff9, 2 | BRF_PRG | BRF_ESS }, //  2 COPX MCU data

	{ "seibu5.c.u1110",				0x010000, 0x5db9f922, 3 | BRF_PRG | BRF_ESS }, //  3 Z80 Code

	{ "seibu7.u0724",				0x020000, 0xc9ec9469, 4 | BRF_GRA },           //  4 Characters

	{ "raiden_2_seibu_bg-1.u0714",	0x200000, 0xe61ad38e, 5 | BRF_GRA },           //  5 Tiles
	{ "raiden_2_seibu_bg-2.u075",	0x200000, 0xa694a4bb, 5 | BRF_GRA },           //  6

	{ "raiden_2_seibu_obj-1.u0811",	0x200000, 0xff08ef0b, 6 | BRF_GRA },           //  7 Sprites (Encrypted)
	{ "raiden_2_seibu_obj-2.u082",	0x200000, 0x638eb771, 6 | BRF_GRA },           //  8
	{ "raiden_2_seibu_obj-3.u0837",	0x200000, 0x897a0322, 6 | BRF_GRA },           //  9
	{ "raiden_2_seibu_obj-4.u0836",	0x200000, 0xb676e188, 6 | BRF_GRA },           // 10

	{ "seibu6.u1017",				0x040000, 0xfb0fca23, 7 | BRF_SND },           // 11 OKI #0 Samples

	{ "raiden_2_pcm.u1018",			0x040000, 0x8cf0d17e, 8 | BRF_SND },           // 12 OKI #1 Samples

	{ "jj4b02__ami18cv8-15.u0342.jed",		0x000288, 0x00000000, 9 | BRF_NODUMP },        // 13 pals
	{ "jj4b01__mmipal16l8bcn.u0341.jed",	0x000335, 0xd1a039af, 0 | BRF_OPT },	       // 14
};

STD_ROM_PICK(raiden2i)
STD_ROM_FN(raiden2i)

struct BurnDriver BurnDrvRaiden2i = {
	"raiden2i", "raiden2", NULL, NULL, "1993",
	"Raiden II (Italy)\0", NULL, "Seibu Kaihatsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE  | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, raiden2iRomInfo, raiden2iRomName, NULL, NULL, Raiden2InputInfo, Raiden2DIPInfo,
	Raiden2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Raiden II (Holland)

static struct BurnRomInfo raiden2nlRomDesc[] = {
	{ "1_u0211.bin",				0x080000, 0x53be3dd0, 1 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "2_u0212.bin",				0x080000, 0x88829c08, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "copx-d2.u0313",				0x040000, 0xa6732ff9, 2 | BRF_PRG | BRF_ESS }, //  2 COPX MCU data

	{ "5_u1110.bin",				0x010000, 0x8f130589, 3 | BRF_PRG | BRF_ESS }, //  3 Z80 Code

	{ "7_u0724.bin",				0x020000, 0xc9ec9469, 4 | BRF_GRA },           //  4 Characters

	{ "raiden_2_seibu_bg-1.u0714",	0x200000, 0xe61ad38e, 5 | BRF_GRA },           //  5 Tiles
	{ "raiden_2_seibu_bg-2.u075",	0x200000, 0xa694a4bb, 5 | BRF_GRA },           //  6

	{ "raiden_2_seibu_obj-1.u0811",	0x200000, 0xff08ef0b, 6 | BRF_GRA },           //  7 Sprites (Encrypted)
	{ "raiden_2_seibu_obj-2.u082",	0x200000, 0x638eb771, 6 | BRF_GRA },           //  8
	{ "raiden_2_seibu_obj-3.u0837",	0x200000, 0x897a0322, 6 | BRF_GRA },           //  9
	{ "raiden_2_seibu_obj-4.u0836",	0x200000, 0xb676e188, 6 | BRF_GRA },           // 10

	{ "6_u1017.bin",				0x040000, 0xfb0fca23, 7 | BRF_SND },           // 11 OKI #0 Samples

	{ "raiden_2_pcm.u1018",			0x040000, 0x8cf0d17e, 8 | BRF_SND },           // 12 OKI #1 Samples

	{ "jj4b02__ami18cv8-15.u0342.jed",		0x000288, 0x00000000, 9 | BRF_NODUMP },        // 13 pals
	{ "jj4b01__mmipal16l8bcn.u0341.jed",	0x000335, 0xd1a039af, 0 | BRF_OPT },	       // 14
};

STD_ROM_PICK(raiden2nl)
STD_ROM_FN(raiden2nl)

struct BurnDriver BurnDrvRaiden2nl = {
	"raiden2nl", "raiden2", NULL, NULL, "1993",
	"Raiden II (Holland)\0", NULL, "Seibu Kaihatsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE  | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, raiden2nlRomInfo, raiden2nlRomName, NULL, NULL, Raiden2InputInfo, Raiden2DIPInfo,
	Raiden2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Raiden II (Switzerland)

static struct BurnRomInfo raiden2swRomDesc[] = {
	{ "seibu_1.u0211",				0x080000, 0x09475ec4, 1 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "seibu_2.u0212",				0x080000, 0x59abc2ec, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "copx-d2.u0313",				0x040000, 0xa6732ff9, 2 | BRF_PRG | BRF_OPT }, //  2 COPX MCU data

	{ "seibu_5.u1110",				0x010000, 0xc2028ba2, 3 | BRF_PRG | BRF_ESS }, //  3 Z80 Code

	{ "seibu_7.u0724",				0x020000, 0xc9ec9469, 4 | BRF_GRA },           //  4 Characters

	{ "raiden_2_seibu_bg-1.u0714",	0x200000, 0xe61ad38e, 5 | BRF_GRA },           //  5 Tiles
	{ "raiden_2_seibu_bg-2.u075",	0x200000, 0xa694a4bb, 5 | BRF_GRA },           //  6

	{ "raiden_2_seibu_obj-1.u0811",	0x200000, 0xff08ef0b, 6 | BRF_GRA },           //  7 Sprites (Encrypted)
	{ "raiden_2_seibu_obj-2.u082",	0x200000, 0x638eb771, 6 | BRF_GRA },           //  8
	{ "raiden_2_seibu_obj-3.u0837",	0x200000, 0x897a0322, 6 | BRF_GRA },           //  9
	{ "raiden_2_seibu_obj-4.u0836",	0x200000, 0xb676e188, 6 | BRF_GRA },           // 10

	{ "seibu_6.u1017",				0x040000, 0xfb0fca23, 7 | BRF_SND },           // 11 OKI #0 Samples

	{ "raiden_2_pcm.u1018",			0x040000, 0x8cf0d17e, 8 | BRF_SND },           // 12 OKI #1 Samples

	{ "jj4b02__ami18cv8-15.u0342.jed",		0x000288, 0x00000000, 9 | BRF_NODUMP },        // 13 pals
	{ "jj4b01__mmipal16l8bcn.u0341.jed",	0x000335, 0xd1a039af, 0 | BRF_OPT },	       // 14
};

STD_ROM_PICK(raiden2sw)
STD_ROM_FN(raiden2sw)

struct BurnDriver BurnDrvRaiden2sw = {
	"raiden2sw", "raiden2", NULL, NULL, "1993",
	"Raiden II (Switzerland)\0", NULL, "Seibu Kaihatsu (Fabtek license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE  | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, raiden2swRomInfo, raiden2swRomName, NULL, NULL, Raiden2InputInfo, Raiden2DIPInfo,
	Raiden2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Raiden II (France)

static struct BurnRomInfo raiden2fRomDesc[] = {
	{ "1_u0211.bin",				0x080000, 0x53be3dd0, 1 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "seibu2_u0212.bin",			0x080000, 0x8dcd8a8d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "copx-d2.u0313",				0x040000, 0xa6732ff9, 2 | BRF_PRG | BRF_ESS }, //  2 COPX MCU data

	{ "seibu5_u1110.bin",			0x010000, 0xf51a28f9, 3 | BRF_PRG | BRF_ESS }, //  3 Z80 Code

	{ "7_u0724.bin",				0x020000, 0xc9ec9469, 4 | BRF_GRA },           //  4 Characters

	{ "raiden_2_seibu_bg-1.u0714",	0x200000, 0xe61ad38e, 5 | BRF_GRA },           //  5 Tiles
	{ "raiden_2_seibu_bg-2.u075",	0x200000, 0xa694a4bb, 5 | BRF_GRA },           //  6

	{ "raiden_2_seibu_obj-1.u0811",	0x200000, 0xff08ef0b, 6 | BRF_GRA },           //  7 Sprites (Encrypted)
	{ "raiden_2_seibu_obj-2.u082",	0x200000, 0x638eb771, 6 | BRF_GRA },           //  8
	{ "raiden_2_seibu_obj-3.u0837",	0x200000, 0x897a0322, 6 | BRF_GRA },           //  9
	{ "raiden_2_seibu_obj-4.u0836",	0x200000, 0xb676e188, 6 | BRF_GRA },           // 10

	{ "6_u1017.bin",				0x040000, 0xfb0fca23, 7 | BRF_SND },           // 11 OKI #0 Samples

	{ "raiden_2_pcm.u1018",			0x040000, 0x8cf0d17e, 8 | BRF_SND },           // 12 OKI #1 Samples

	{ "jj4b02__ami18cv8-15.u0342.jed",		0x000288, 0x00000000, 9 | BRF_NODUMP },        // 13 pals
	{ "jj4b01__mmipal16l8bcn.u0341.jed",	0x000335, 0xd1a039af, 0 | BRF_OPT },	       // 14
};

STD_ROM_PICK(raiden2f)
STD_ROM_FN(raiden2f)

struct BurnDriver BurnDrvRaiden2f = {
	"raiden2f", "raiden2", NULL, NULL, "1993",
	"Raiden II (France)\0", NULL, "Seibu Kaihatsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE  | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, raiden2fRomInfo, raiden2fRomName, NULL, NULL, Raiden2InputInfo, Raiden2DIPInfo,
	Raiden2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Raiden II (Germany)

static struct BurnRomInfo raiden2gRomDesc[] = {
	{ "prg0.u0211",					0x080000, 0x09475ec4, 1 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "prg1g.u0212",				0x080000, 0x41001d2e, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "copx-d2.u0313",				0x040000, 0xa6732ff9, 2 | BRF_PRG | BRF_OPT }, //  2 COPX MCU data

	{ "snd.u1110",  				0x010000, 0xf51a28f9, 3 | BRF_PRG | BRF_ESS }, //  3 Z80 Code

	{ "seibu7.u0724",				0x020000, 0xc9ec9469, 4 | BRF_GRA },           //  4 Characters

	{ "raiden_2_seibu_bg-1.u0714",	0x200000, 0xe61ad38e, 5 | BRF_GRA },           //  5 Tiles
	{ "raiden_2_seibu_bg-2.u075",	0x200000, 0xa694a4bb, 5 | BRF_GRA },           //  6

	{ "raiden_2_seibu_obj-1.u0811", 0x200000, 0xff08ef0b, 6 | BRF_GRA },           //  7 Sprites (Encrypted)
	{ "raiden_2_seibu_obj-2.u082", 	0x200000, 0x638eb771, 6 | BRF_GRA },           //  8
	{ "raiden_2_seibu_obj-3.u0837", 0x200000, 0x897a0322, 6 | BRF_GRA },           //  9
	{ "raiden_2_seibu_obj-4.u0836", 0x200000, 0xb676e188, 6 | BRF_GRA },           // 10

	{ "seibu6.u1017",				0x040000, 0xfb0fca23, 7 | BRF_SND },           // 11 OKI #0 Samples

	{ "raiden_2_pcm.u1018",			0x040000, 0x8cf0d17e, 8 | BRF_SND },           // 12 OKI #1 Samples

	{ "jj4b02__ami18cv8-15.u0342.jed", 		0x000288, 0x00000000, 0 | BRF_NODUMP },	       // 13 Pals
	{ "jj4b01__mmipal16l8bcn.u0341.jed",	0x000335, 0xd1a039af, 0 | BRF_OPT },	       // 14
};

STD_ROM_PICK(raiden2g)
STD_ROM_FN(raiden2g)

struct BurnDriver BurnDrvRaiden2g = {
	"raiden2g", "raiden2", NULL, NULL, "1993",
	"Raiden II (Germany)\0", NULL, "Seibu Kaihatsu (Tuning license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, raiden2gRomInfo, raiden2gRomName, NULL, NULL, Raiden2InputInfo, Raiden2DIPInfo,
	Raiden2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Raiden II (Easy Version, Korea?)

static struct BurnRomInfo raiden2eRomDesc[] = {
	{ "r2_prg_0.u0211",				0x080000, 0x2abc848c, 1 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "r2_prg_1.u0212",				0x080000, 0x509ade43, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "copx-d2.u0313",				0x040000, 0xa6732ff9, 2 | BRF_PRG | BRF_ESS }, //  2 COPX MCU data

	{ "r2_snd.u1110",				0x010000, 0x6bad0a3e, 3 | BRF_PRG | BRF_ESS }, //  3 Z80 Code

	{ "r2_fx0.u0724",				0x020000, 0xc709bdf6, 4 | BRF_GRA },           //  4 Characters

	{ "raiden_2_seibu_bg-1.u0714",	0x200000, 0xe61ad38e, 5 | BRF_GRA },           //  5 Tiles
	{ "raiden_2_seibu_bg-2.u075",	0x200000, 0xa694a4bb, 5 | BRF_GRA },           //  6

	{ "raiden_2_seibu_obj-1.u0811",	0x200000, 0xff08ef0b, 6 | BRF_GRA },           //  7 Sprites (Encrypted)
	{ "raiden_2_seibu_obj-2.u082",	0x200000, 0x638eb771, 6 | BRF_GRA },           //  8
	{ "raiden_2_seibu_obj-3.u0837",	0x200000, 0x897a0322, 6 | BRF_GRA },           //  9
	{ "raiden_2_seibu_obj-4.u0836",	0x200000, 0xb676e188, 6 | BRF_GRA },           // 10

	{ "r2_voi1.u1017",				0x040000, 0x488d050f, 7 | BRF_SND },           // 11 OKI #0 Samples

	{ "raiden_2_pcm.u1018",			0x040000, 0x8cf0d17e, 8 | BRF_SND },           // 12 OKI #1 Samples

	{ "jj4b02__ami18cv8-15.u0342.jed",		0x000288, 0x00000000, 9 | BRF_NODUMP },        // 13 pals
	{ "jj4b01__mmipal16l8bcn.u0341.jed",	0x000335, 0xd1a039af, 0 | BRF_OPT },	       // 14
};

STD_ROM_PICK(raiden2e)
STD_ROM_FN(raiden2e)

struct BurnDriver BurnDrvRaiden2e = {
	"raiden2e", "raiden2", NULL, NULL, "1993",
	"Raiden II (Easy Version, Korea?)\0", NULL, "Seibu Kaihatsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE  | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, raiden2eRomInfo, raiden2eRomName, NULL, NULL, Raiden2InputInfo, Raiden2DIPInfo,
	Raiden2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Raiden II (Easy Version, Japan?)

static struct BurnRomInfo raiden2eaRomDesc[] = {
	{ "r2.1.u0211",					0x080000, 0xd7041be4, 1 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "r2.2.u0212",					0x080000, 0xbf7577ec, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "copx-d2.u0313",				0x040000, 0xa6732ff9, 2 | BRF_PRG | BRF_ESS }, //  2 COPX MCU data

	{ "r2.5.u1110",					0x010000, 0xf5f835af, 3 | BRF_PRG | BRF_ESS }, //  3 Z80 Code

	{ "r2.7.u0724",					0x020000, 0xc7aa4d00, 4 | BRF_GRA },           //  4 Characters

	{ "raiden_2_seibu_bg-1.u0714",	0x200000, 0xe61ad38e, 5 | BRF_GRA },           //  5 Tiles
	{ "raiden_2_seibu_bg-2.u075",	0x200000, 0xa694a4bb, 5 | BRF_GRA },           //  6

	{ "raiden_2_seibu_obj-1.u0811",	0x200000, 0xff08ef0b, 6 | BRF_GRA },           //  7 Sprites (Encrypted)
	{ "raiden_2_seibu_obj-2.u082",	0x200000, 0x638eb771, 6 | BRF_GRA },           //  8
	{ "raiden_2_seibu_obj-3.u0837",	0x200000, 0x897a0322, 6 | BRF_GRA },           //  9
	{ "raiden_2_seibu_obj-4.u0836",	0x200000, 0xb676e188, 6 | BRF_GRA },           // 10

	{ "r2.6.u1017",					0x040000, 0xfab9f8e4, 7 | BRF_SND },           // 11 OKI #0 Samples

	{ "raiden_2_pcm.u1018",			0x040000, 0x8cf0d17e, 8 | BRF_SND },           // 12 OKI #1 Samples

	{ "jj4b02__ami18cv8-15.u0342.jed",		0x000288, 0x00000000, 9 | BRF_NODUMP },        // 13 pals
	{ "jj4b01__mmipal16l8bcn.u0341.jed",	0x000335, 0xd1a039af, 0 | BRF_OPT },	       // 14
};

STD_ROM_PICK(raiden2ea)
STD_ROM_FN(raiden2ea)

struct BurnDriver BurnDrvRaiden2ea = {
	"raiden2ea", "raiden2", NULL, NULL, "1993",
	"Raiden II (Easy Version, Japan?)\0", NULL, "Seibu Kaihatsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE  | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, raiden2eaRomInfo, raiden2eaRomName, NULL, NULL, Raiden2InputInfo, Raiden2DIPInfo,
	Raiden2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Raiden II (Easy Version, US set 2)

static struct BurnRomInfo raiden2euRomDesc[] = {
	{ "seibu_1.u0211",				0x080000, 0xd7041be4, 1 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "seibu_2.u0212",				0x080000, 0xbeb71ddb, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "copx-d2.u0313",				0x040000, 0xa6732ff9, 2 | BRF_PRG | BRF_ESS }, //  2 COPX MCU data

	{ "r2.5.u1110",					0x010000, 0xf5f835af, 3 | BRF_PRG | BRF_ESS }, //  3 Z80 Code

	{ "r2.7.u0724",					0x020000, 0xc7aa4d00, 4 | BRF_GRA },           //  4 Characters

	{ "raiden_2_seibu_bg-1.u0714",	0x200000, 0xe61ad38e, 5 | BRF_GRA },           //  5 Tiles
	{ "raiden_2_seibu_bg-2.u075",	0x200000, 0xa694a4bb, 5 | BRF_GRA },           //  6

	{ "raiden_2_seibu_obj-1.u0811",	0x200000, 0xff08ef0b, 6 | BRF_GRA },           //  7 Sprites (Encrypted)
	{ "raiden_2_seibu_obj-2.u082",	0x200000, 0x638eb771, 6 | BRF_GRA },           //  8
	{ "raiden_2_seibu_obj-3.u0837",	0x200000, 0x897a0322, 6 | BRF_GRA },           //  9
	{ "raiden_2_seibu_obj-4.u0836",	0x200000, 0xb676e188, 6 | BRF_GRA },           // 10

	{ "r2.6.u1017",					0x040000, 0xfab9f8e4, 7 | BRF_SND },           // 11 OKI #0 Samples

	{ "raiden_2_pcm.u1018",			0x040000, 0x8cf0d17e, 8 | BRF_SND },           // 12 OKI #1 Samples

	{ "jj4b02__ami18cv8-15.u0342.jed",		0x000288, 0x00000000, 9 | BRF_NODUMP },        // 13 pals
	{ "jj4b01__mmipal16l8bcn.u0341.jed",	0x000335, 0xd1a039af, 0 | BRF_OPT },	       // 14
};

STD_ROM_PICK(raiden2eu)
STD_ROM_FN(raiden2eu)

struct BurnDriver BurnDrvRaiden2eu = {
	"raiden2eu", "raiden2", NULL, NULL, "1993",
	"Raiden II (Easy Version, US set 2)\0", NULL, "Seibu Kaihatsu (Fabtek license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE  | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, raiden2euRomInfo, raiden2euRomName, NULL, NULL, Raiden2InputInfo, Raiden2DIPInfo,
	Raiden2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Raiden II (Easy Version, US set 1)

static struct BurnRomInfo raiden2euaRomDesc[] = {
	{ "seibu__1.27c020j.u1210",			0x040000, 0xed1514e3, 1 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "seibu__2.27c2001.u1211",			0x040000, 0xbb6ecf2a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "seibu__3.27c2001.u129",			0x040000, 0x6a01d52c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "seibu__4.27c2001.u1212",			0x040000, 0xe54bfa37, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "copx-d2.u0313",					0x040000, 0xa6732ff9, 2 | BRF_PRG | BRF_ESS }, //  4 COPX MCU data

	{ "seibu__5.27c512.u1110",			0x010000, 0x6d362472, 3 | BRF_PRG | BRF_ESS }, //  5 Z80 Code

	{ "seibu__7.fx0.27c210.u0724",		0x020000, 0xc7aa4d00, 4 | BRF_GRA },           //  6 Characters

	{ "raiden_2_seibu_bg-1.u0714",		0x200000, 0xe61ad38e, 5 | BRF_GRA },           //  7 Tiles
	{ "raiden_2_seibu_bg-2.u075",		0x200000, 0xa694a4bb, 5 | BRF_GRA },           //  8

	{ "raiden_2_seibu_obj-1.u0811",		0x200000, 0xff08ef0b, 6 | BRF_GRA },           //  9 Sprites (Encrypted)
	{ "raiden_2_seibu_obj-2.u082",		0x200000, 0x638eb771, 6 | BRF_GRA },           // 10
	{ "raiden_2_seibu_obj-3.u0837",		0x200000, 0x897a0322, 6 | BRF_GRA },           // 11
	{ "raiden_2_seibu_obj-4.u0836",		0x200000, 0xb676e188, 6 | BRF_GRA },           // 12

	{ "seibu__6.voice1.23c020.u1017",	0x040000, 0xfab9f8e4, 7 | BRF_SND },           // 13 OKI #0 Samples

	{ "raiden_2_pcm.u1018",				0x040000, 0x8cf0d17e, 8 | BRF_SND },           // 14 OKI #1 Samples

	{ "jj4b02__ami18cv8-15.u0342.jed",		0x000288, 0x00000000, 9 | BRF_NODUMP },        // 15 pals
	{ "jj4b01__mmipal16l8bcn.u0341.jed",	0x000335, 0xd1a039af, 0 | BRF_OPT },	       // 14
};

STD_ROM_PICK(raiden2eua)
STD_ROM_FN(raiden2eua)

struct BurnDriver BurnDrvRaiden2eua = {
	"raiden2eua", "raiden2", NULL, NULL, "1993",
	"Raiden II (Easy Version, US set 1)\0", NULL, "Seibu Kaihatsu (Fabtek license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE  | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, raiden2euaRomInfo, raiden2euaRomName, NULL, NULL, Raiden2InputInfo, Raiden2DIPInfo,
	Raiden2aInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Raiden II (Easy Version, Germany)
// this is the same code revision as raiden2eua but a german region

static struct BurnRomInfo raiden2egRomDesc[] = {
	{ "raiden_2_1.bin",				0x040000, 0xed1514e3, 1 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "raiden_2_2.bin",				0x040000, 0xbb6ecf2a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "raiden_2_3.bin",				0x040000, 0x6a01d52c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "raiden_2_4.bin",				0x040000, 0x81273f33, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "copx-d2.u0313",				0x040000, 0xa6732ff9, 2 | BRF_PRG | BRF_ESS }, //  4 COPX MCU data

	{ "raiden_2_5.bin",				0x010000, 0x6d362472, 3 | BRF_PRG | BRF_ESS }, //  5 Z80 Code

	{ "raiden_2_7.bin",				0x020000, 0xc7aa4d00, 4 | BRF_GRA },           //  6 Characters

	{ "raiden_2_seibu_bg-1.u0714",	0x200000, 0xe61ad38e, 5 | BRF_GRA },           //  7 Tiles
	{ "raiden_2_seibu_bg-2.u075",	0x200000, 0xa694a4bb, 5 | BRF_GRA },           //  8

	{ "raiden_2_seibu_obj-1.u0811",	0x200000, 0xff08ef0b, 6 | BRF_GRA },           //  9 Sprites (Encrypted)
	{ "raiden_2_seibu_obj-2.u082",	0x200000, 0x638eb771, 6 | BRF_GRA },           // 10
	{ "raiden_2_seibu_obj-3.u0837",	0x200000, 0x897a0322, 6 | BRF_GRA },           // 11
	{ "raiden_2_seibu_obj-4.u0836",	0x200000, 0xb676e188, 6 | BRF_GRA },           // 12

	{ "raiden_2_6.bin",				0x040000, 0xfab9f8e4, 7 | BRF_SND },           // 13 OKI #0 Samples

	{ "raiden_2_pcm.u1018",			0x040000, 0x8cf0d17e, 8 | BRF_SND },           // 14 OKI #1 Samples

	{ "jj4b02__ami18cv8-15.u0342.jed",		0x000288, 0x00000000, 9 | BRF_NODUMP },        // 15 pals
	{ "jj4b01__mmipal16l8bcn.u0341.jed",	0x000335, 0xd1a039af, 0 | BRF_OPT },	       // 16
};

STD_ROM_PICK(raiden2eg)
STD_ROM_FN(raiden2eg)

struct BurnDriver BurnDrvRaiden2eg = {
	"raiden2eg", "raiden2", NULL, NULL, "1993",
	"Raiden II (Easy Version, Germany)\0", NULL, "Seibu Kaihatsu (Tuning license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE  | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, raiden2egRomInfo, raiden2egRomName, NULL, NULL, Raiden2InputInfo, Raiden2DIPInfo,
	Raiden2aInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Raiden II (harder, Raiden DX Hardware)

static struct BurnRomInfo raiden2dxRomDesc[] = {
	{ "u1210.bin",				0x080000, 0x413241e0, 1 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "prg1_u1211.bin",			0x080000, 0x93491f56, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "u129.bin",				0x080000, 0xe0932b6c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "u1212.bin",				0x080000, 0x505423f4, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "copx-d2.6s",				0x040000, 0xa6732ff9, 2 | BRF_PRG | BRF_ESS }, //  4 COPX MCU data

	{ "u1110.bin",				0x010000, 0xb8ad8fe7, 3 | BRF_PRG | BRF_ESS }, //  5 Z80 Code

//	{ "fx0_u0724.bin",			0x020000, 0xded3c718, 4 | BRF_GRA },           //  6 Characters
	{ "7_u0724.bin",			0x020000, 0xc9ec9469, 4 | BRF_GRA },           //  6 Characters

	{ "dx_back1.1s",			0x200000, 0x90970355, 5 | BRF_GRA },           //  7 Tiles
	{ "dx_back2.2s",			0x200000, 0x5799af3e, 5 | BRF_GRA },           //  8

	{ "obj1",					0x200000, 0xff08ef0b, 6 | BRF_GRA },           //  9 Sprites (Encrypted)
	{ "obj2",					0x200000, 0x638eb771, 6 | BRF_GRA },           // 10
	{ "dx_obj3.4k",				0x200000, 0xba381227, 6 | BRF_GRA },           // 11
	{ "dx_obj4.6k",				0x200000, 0x65e50d19, 6 | BRF_GRA },           // 12

	{ "dx_6.3b",				0x040000, 0x9a9196da, 7 | BRF_SND },           // 13 OKI #0 Samples

	{ "dx_pcm.3a",				0x040000, 0x8cf0d17e, 8 | BRF_SND },           // 14 OKI #1 Samples
};

STD_ROM_PICK(raiden2dx)
STD_ROM_FN(raiden2dx)

struct BurnDriver BurnDrvRaiden2dx = {
	"raiden2dx", "raiden2", NULL, NULL, "1993",
	"Raiden II (harder, Raiden DX Hardware)\0", NULL, "Seibu Kaihatsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE  | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, raiden2dxRomInfo, raiden2dxRomName, NULL, NULL, Raiden2InputInfo, Raiden2DIPInfo,
	RaidendxInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Raiden DX (UK)

static struct BurnRomInfo raidendxRomDesc[] = {
	{ "1d.4n",				0x080000, 0x14d725fc, 1 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "2d.4p",				0x080000, 0x5e7e45cb, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3d.6n",				0x080000, 0xf0a47e67, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4d.6p",				0x080000, 0x2a2003e8, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "copx-d2.6s",			0x040000, 0xa6732ff9, 2 | BRF_PRG | BRF_OPT }, //  4 COPX MCU data

	{ "dx_5.5b",  			0x010000, 0x8c46857a, 3 | BRF_PRG | BRF_ESS }, //  5 Z80 Code

	{ "dx_7.4s",			0x020000, 0xc73986d4, 4 | BRF_GRA },           //  6 Characters

	{ "dx_back1.1s",		0x200000, 0x90970355, 5 | BRF_GRA },           //  7 Tiles
	{ "dx_back2.2s",		0x200000, 0x5799af3e, 5 | BRF_GRA },           //  8 Tiles

	{ "obj1", 				0x200000, 0xff08ef0b, 6 | BRF_GRA },           //  9 Sprites (Encrypted)
	{ "obj2",				0x200000, 0x638eb771, 6 | BRF_GRA },           // 10
	{ "dx_obj3.4k",  		0x200000, 0xba381227, 6 | BRF_GRA },           // 11
	{ "dx_obj4.6k",			0x200000, 0x65e50d19, 6 | BRF_GRA },           // 12

	{ "dx_6.3b",			0x040000, 0x9a9196da, 7 | BRF_SND },           // 13 OKI #0 Samples

	{ "dx_pcm.3a",			0x040000, 0x8cf0d17e, 8 | BRF_SND },           // 14 OKI #1 Samples
};

STD_ROM_PICK(raidendx)
STD_ROM_FN(raidendx)

struct BurnDriver BurnDrvRaidendx = {
	"raidendx", NULL, NULL, NULL, "1994",
	"Raiden DX (UK)\0", NULL, "Seibu Kaihatsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, raidendxRomInfo, raidendxRomName, NULL, NULL, RaidendxInputInfo, RaidendxDIPInfo,
	RaidendxInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Raiden DX (Hong Kong, set 1)

static struct BurnRomInfo raidendxa1RomDesc[] = {
	{ "dx_1h.4n",			0x080000, 0x7624c36b, 1 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "dx_2h.4p",			0x080000, 0x4940fdf3, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "dx_3h.6n",			0x080000, 0x6c495bcf, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "dx_4h.6k",			0x080000, 0x9ed6335f, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "copx-d2.6s",			0x040000, 0xa6732ff9, 2 | BRF_PRG | BRF_OPT }, //  4 COPX MCU data

	{ "dx_5.5b",  			0x010000, 0x8c46857a, 3 | BRF_PRG | BRF_ESS }, //  5 Z80 Code

	{ "dx_7.4s",			0x020000, 0xc73986d4, 4 | BRF_GRA },           //  6 Characters

	{ "dx_back1.1s",		0x200000, 0x90970355, 5 | BRF_GRA },           //  7 Tiles
	{ "dx_back2.2s",		0x200000, 0x5799af3e, 5 | BRF_GRA },           //  8 Tiles

	{ "obj1", 				0x200000, 0xff08ef0b, 6 | BRF_GRA },           //  9 Sprites (Encrypted)
	{ "obj2",				0x200000, 0x638eb771, 6 | BRF_GRA },           // 10
	{ "dx_obj3.4k",  		0x200000, 0xba381227, 6 | BRF_GRA },           // 11
	{ "dx_obj4.6k",			0x200000, 0x65e50d19, 6 | BRF_GRA },           // 12

	{ "dx_6.3b",			0x040000, 0x9a9196da, 7 | BRF_SND },           // 13 OKI #0 Samples

	{ "dx_pcm.3a",			0x040000, 0x8cf0d17e, 8 | BRF_SND },           // 14 OKI #1 Samples
};

STD_ROM_PICK(raidendxa1)
STD_ROM_FN(raidendxa1)

struct BurnDriver BurnDrvRaidendxa1 = {
	"raidendxa1", "raidendx", NULL, NULL, "1994",
	"Raiden DX (Hong Kong, set 1)\0", NULL, "Seibu Kaihatsu (Metrotainment license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE  | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, raidendxa1RomInfo, raidendxa1RomName, NULL, NULL, RaidendxInputInfo, RaidendxDIPInfo,
	RaidendxInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Raiden DX (Hong Kong, set 2)

static struct BurnRomInfo raidendxa2RomDesc[] = {
	{ "1d.bin",				0x080000, 0x22b155ae, 1 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "2d.bin",				0x080000, 0x2be98ca8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3d.bin",				0x080000, 0xb4785576, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4d.bin",				0x080000, 0x5a77f7b4, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "copx-d2.6s",			0x040000, 0xa6732ff9, 2 | BRF_PRG | BRF_OPT }, //  4 COPX MCU data

	{ "dx_5.5b",  			0x010000, 0x8c46857a, 3 | BRF_PRG | BRF_ESS }, //  5 Z80 Code

	{ "dx_7.4s",			0x020000, 0xc73986d4, 4 | BRF_GRA },           //  6 Characters

	{ "dx_back1.1s",		0x200000, 0x90970355, 5 | BRF_GRA },           //  7 Tiles
	{ "dx_back2.2s",		0x200000, 0x5799af3e, 5 | BRF_GRA },           //  8 Tiles

	{ "obj1", 				0x200000, 0xff08ef0b, 6 | BRF_GRA },           //  9 Sprites (Encrypted)
	{ "obj2",				0x200000, 0x638eb771, 6 | BRF_GRA },           // 10
	{ "dx_obj3.4k",  		0x200000, 0xba381227, 6 | BRF_GRA },           // 11
	{ "dx_obj4.6k",			0x200000, 0x65e50d19, 6 | BRF_GRA },           // 12

	{ "dx_6.3b",			0x040000, 0x9a9196da, 7 | BRF_SND },           // 13 OKI #0 Samples

	{ "dx_pcm.3a",			0x040000, 0x8cf0d17e, 8 | BRF_SND },           // 14 OKI #1 Samples
};

STD_ROM_PICK(raidendxa2)
STD_ROM_FN(raidendxa2)

struct BurnDriver BurnDrvRaidendxa2 = {
	"raidendxa2", "raidendx", NULL, NULL, "1994",
	"Raiden DX (Hong Kong, set 2)\0", NULL, "Seibu Kaihatsu (Metrotainment license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, raidendxa2RomInfo, raidendxa2RomName, NULL, NULL, RaidendxInputInfo, RaidendxDIPInfo,
	RaidendxInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Raiden DX (Korea)

static struct BurnRomInfo raidendxkRomDesc[] = {
	{ "rdxj_1.bin",			0x080000, 0xb5b32885, 1 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "rdxj_2.bin",			0x080000, 0x7efd581d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rdxj_3.bin",			0x080000, 0x55ec0e1d, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rdxj_4.bin",			0x080000, 0xf8fb31b4, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "copx-d2.6s",			0x040000, 0xa6732ff9, 2 | BRF_PRG | BRF_OPT }, //  4 COPX MCU data

	{ "dx_5.5b",  			0x010000, 0x8c46857a, 3 | BRF_PRG | BRF_ESS }, //  5 Z80 Code

	{ "dx_7.4s",			0x020000, 0xc73986d4, 4 | BRF_GRA },           //  6 Characters

	{ "dx_back1.1s",		0x200000, 0x90970355, 5 | BRF_GRA },           //  7 Tiles
	{ "dx_back2.2s",		0x200000, 0x5799af3e, 5 | BRF_GRA },           //  8 Tiles

	{ "obj1", 				0x200000, 0xff08ef0b, 6 | BRF_GRA },           //  9 Sprites (Encrypted)
	{ "obj2",				0x200000, 0x638eb771, 6 | BRF_GRA },           // 10
	{ "dx_obj3.4k",  		0x200000, 0xba381227, 6 | BRF_GRA },           // 11
	{ "dx_obj4.6k",			0x200000, 0x65e50d19, 6 | BRF_GRA },           // 12

	{ "dx_6.3b",			0x040000, 0x9a9196da, 7 | BRF_SND },           // 13 OKI #0 Samples

	{ "dx_pcm.3a",			0x040000, 0x8cf0d17e, 8 | BRF_SND },           // 14 OKI #1 Samples
};

STD_ROM_PICK(raidendxk)
STD_ROM_FN(raidendxk)

struct BurnDriver BurnDrvRaidendxk = {
	"raidendxk", "raidendx", NULL, NULL, "1994",
	"Raiden DX (Korea)\0", NULL, "Seibu Kaihatsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, raidendxkRomInfo, raidendxkRomName, NULL, NULL, RaidendxInputInfo, RaidendxDIPInfo,
	RaidendxInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Raiden DX (US)

static struct BurnRomInfo raidendxuRomDesc[] = {
	{ "1a.u1210",			0x080000, 0x53e63194, 1 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "2a.u1211",			0x080000, 0xec8d1647, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3a.u129",			0x080000, 0x7dbfd73d, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4a.u1212",			0x080000, 0xcb41a459, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "copx-d2.6s",			0x040000, 0xa6732ff9, 2 | BRF_PRG | BRF_OPT }, //  4 COPX MCU data

	{ "dx_5.5b",  			0x010000, 0x8c46857a, 3 | BRF_PRG | BRF_ESS }, //  5 Z80 Code

	{ "dx_7.4s",			0x020000, 0xc73986d4, 4 | BRF_GRA },           //  6 Characters

	{ "dx_back1.1s",		0x200000, 0x90970355, 5 | BRF_GRA },           //  7 Tiles
	{ "dx_back2.2s",		0x200000, 0x5799af3e, 5 | BRF_GRA },           //  8 Tiles

	{ "obj1", 				0x200000, 0xff08ef0b, 6 | BRF_GRA },           //  9 Sprites (Encrypted)
	{ "obj2",				0x200000, 0x638eb771, 6 | BRF_GRA },           // 10
	{ "dx_obj3.4k",  		0x200000, 0xba381227, 6 | BRF_GRA },           // 11
	{ "dx_obj4.6k",			0x200000, 0x65e50d19, 6 | BRF_GRA },           // 12

	{ "dx_6.3b",			0x040000, 0x9a9196da, 7 | BRF_SND },           // 13 OKI #0 Samples

	{ "dx_pcm.3a",			0x040000, 0x8cf0d17e, 8 | BRF_SND },           // 14 OKI #1 Samples
};

STD_ROM_PICK(raidendxu)
STD_ROM_FN(raidendxu)

struct BurnDriver BurnDrvRaidendxu = {
	"raidendxu", "raidendx", NULL, NULL, "1994",
	"Raiden DX (US)\0", NULL, "Seibu Kaihatsu (Fabtek license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, raidendxuRomInfo, raidendxuRomName, NULL, NULL, RaidendxInputInfo, RaidendxDIPInfo,
	RaidendxInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Raiden DX (Germany)

static struct BurnRomInfo raidendxgRomDesc[] = {
	{ "1d.u1210",				0x080000, 0x14d725fc, 1 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "2d.u1211",				0x080000, 0x5e7e45cb, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3d.u129",				0x080000, 0xf0a47e67, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4d.u1212",				0x080000, 0x6bde6edc, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "copx-d2.u0313",			0x040000, 0xa6732ff9, 2 | BRF_PRG | BRF_OPT }, //  4 COPX MCU data

	{ "5.u1110",				0x010000, 0x8c46857a, 3 | BRF_PRG | BRF_ESS }, //  5 Z80 Code

	{ "7.u0724",				0x020000, 0xc73986d4, 4 | BRF_GRA },           //  6 Characters

	{ "dx_back-1.u075",			0x200000, 0x90970355, 5 | BRF_GRA },           //  7 Tiles
	{ "dx_back-2.u0714",		0x200000, 0x5799af3e, 5 | BRF_GRA },           //  8

	{ "raiden_2_seibu_obj-1.u0811",		0x200000, 0xff08ef0b, 6 | BRF_GRA },           //  9 Sprites (Encrypted)
	{ "raiden_2_seibu_obj-2.u082",		0x200000, 0x638eb771, 6 | BRF_GRA },           // 10
	{ "dx_obj-3.u0837",			0x200000, 0xba381227, 6 | BRF_GRA },           // 11
	{ "dx_obj-4.u0836",			0x200000, 0x65e50d19, 6 | BRF_GRA },           // 12

	{ "6.u1017",				0x040000, 0x9a9196da, 7 | BRF_SND },           // 13 OKI #0 Samples

	{ "pcm.u1018",				0x040000, 0x8cf0d17e, 8 | BRF_SND },           // 14 OKI #1 Samples
};

STD_ROM_PICK(raidendxg)
STD_ROM_FN(raidendxg)

struct BurnDriver BurnDrvRaidendxg = {
	"raidendxg", "raidendx", NULL, NULL, "1994",
	"Raiden DX (Germany)\0", NULL, "Seibu Kaihatsu (Tuning license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, raidendxgRomInfo, raidendxgRomName, NULL, NULL, RaidendxInputInfo, RaidendxDIPInfo,
	RaidendxInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Raiden DX (Holland)

static struct BurnRomInfo raidendxnlRomDesc[] = {
	{ "u1210_4n.bin",			0x080000, 0xc589019a, 1 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "u1211_4p.bin",			0x080000, 0xb2222254, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "u129_6n.bin",			0x080000, 0x60f04634, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "u1212_6p.bin",			0x080000, 0x21ec37cc, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "copx-d2.u0313",			0x040000, 0xa6732ff9, 2 | BRF_PRG | BRF_OPT }, //  4 COPX MCU data

	{ "u1110_5b.bin",			0x010000, 0x8c46857a, 3 | BRF_PRG | BRF_ESS }, //  5 Z80 Code

	{ "seibu_7b_u724.bin",		0x020000, 0xc73986d4, 4 | BRF_GRA },           //  6 Characters

	{ "dx_back-1.u075",			0x200000, 0x90970355, 5 | BRF_GRA },           //  7 Tiles
	{ "dx_back-2.u0714",		0x200000, 0x5799af3e, 5 | BRF_GRA },           //  8

	{ "raiden_2_seibu_obj-1.u0811",		0x200000, 0xff08ef0b, 6 | BRF_GRA },           //  9 Sprites (Encrypted)
	{ "raiden_2_seibu_obj-2.u082",		0x200000, 0x638eb771, 6 | BRF_GRA },           // 10
	{ "dx_obj-3.u0837",			0x200000, 0xba381227, 6 | BRF_GRA },           // 11
	{ "dx_obj-4.u0836",			0x200000, 0x65e50d19, 6 | BRF_GRA },           // 12

	{ "seibu_6_u1017.bin",		0x040000, 0x9a9196da, 7 | BRF_SND },           // 13 OKI #0 Samples

	{ "pcm.u1018",				0x040000, 0x8cf0d17e, 8 | BRF_SND },           // 14 OKI #1 Samples
};

STD_ROM_PICK(raidendxnl)
STD_ROM_FN(raidendxnl)

struct BurnDriver BurnDrvRaidendxnl = {
	"raidendxnl", "raidendx", NULL, NULL, "1994",
	"Raiden DX (Holland)\0", NULL, "Seibu Kaihatsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, raidendxnlRomInfo, raidendxnlRomName, NULL, NULL, RaidendxInputInfo, RaidendxDIPInfo,
	RaidendxInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Raiden DX (Japan)

static struct BurnRomInfo raidendxjRomDesc[] = {
	{ "rdxj_1.u1211",			0x080000, 0x5af382e1, 1 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "rdxj_2.u0212",			0x080000, 0x899966fc, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rdxj_3.u129",			0x080000, 0xe7f08013, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rdxj_4.u1212",			0x080000, 0x78037e1f, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "copx-d2.u0313",			0x040000, 0xa6732ff9, 2 | BRF_PRG | BRF_OPT }, //  4 COPX MCU data

	{ "rdxj_5.u1110",			0x010000, 0x8c46857a, 3 | BRF_PRG | BRF_ESS }, //  5 Z80 Code

	{ "rdxj_7.u0724",			0x020000, 0xec31fa10, 4 | BRF_GRA },           //  6 Characters

	{ "dx_back-1.u075",			0x200000, 0x90970355, 5 | BRF_GRA },           //  7 Tiles
	{ "dx_back-2.u0714",		0x200000, 0x5799af3e, 5 | BRF_GRA },           //  8

	{ "raiden_2_seibu_obj-1.u0811",		0x200000, 0xff08ef0b, 6 | BRF_GRA },           //  9 Sprites (Encrypted)
	{ "raiden_2_seibu_obj-2.u082",		0x200000, 0x638eb771, 6 | BRF_GRA },           // 10
	{ "dx_obj-3.u0837",			0x200000, 0xba381227, 6 | BRF_GRA },           // 11
	{ "dx_obj-4.u0836",			0x200000, 0x65e50d19, 6 | BRF_GRA },           // 12

	{ "rdxj_6.u1017",			0x040000, 0x9a9196da, 7 | BRF_SND },           // 13 OKI #0 Samples

	{ "pcm.u1018",				0x040000, 0x8cf0d17e, 8 | BRF_SND },           // 14 OKI #1 Samples
};

STD_ROM_PICK(raidendxj)
STD_ROM_FN(raidendxj)

struct BurnDriver BurnDrvRaidendxj = {
	"raidendxj", "raidendx", NULL, NULL, "1994",
	"Raiden DX (Japan)\0", NULL, "Seibu Kaihatsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, raidendxjRomInfo, raidendxjRomName, NULL, NULL, RaidendxInputInfo, RaidendxDIPInfo,
	RaidendxInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Raiden DX (China)

static struct BurnRomInfo raidendxchRomDesc[] = {
	{ "rdxc_1.u1210",		0x080000, 0x2154c6ae, 1 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "rdxc_2.u1211",		0x080000, 0x73bb74b7, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rdxc_3.u129",		0x080000, 0x50f0a6aa, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rdxc_4.u1212",		0x080000, 0x00071e70, 1 | BRF_PRG | BRF_ESS }, //  3

	// no other roms present with this set, so the ones below could be wrong
	{ "copx-d2.6s",			0x040000, 0xa6732ff9, 2 | BRF_PRG | BRF_OPT }, //  4 COPX MCU data

	{ "dx_5.5b",			0x010000, 0x8c46857a, 3 | BRF_PRG | BRF_ESS }, //  5 Z80 Code

	{ "dx_7.4s",			0x020000, 0xc73986d4, 4 | BRF_GRA },           //  6 Characters

	{ "dx_back1.1s",		0x200000, 0x90970355, 5 | BRF_GRA },           //  7 Tiles
	{ "dx_back2.2s",		0x200000, 0x5799af3e, 5 | BRF_GRA },           //  8

	{ "obj1",				0x200000, 0xff08ef0b, 6 | BRF_GRA },   	       //  9 Sprites (Encrypted)
	{ "obj2",				0x200000, 0x638eb771, 6 | BRF_GRA },   	       // 10
	{ "dx_obj3.4k",			0x200000, 0xba381227, 6 | BRF_GRA },           // 11
	{ "dx_obj4.6k",			0x200000, 0x65e50d19, 6 | BRF_GRA },           // 12

	{ "dx_6.3b",			0x040000, 0x9a9196da, 7 | BRF_SND },           // 13 OKI #0 Samples

	{ "dx_pcm.3a",			0x040000, 0x8cf0d17e, 8 | BRF_SND },           // 14 OKI #1 Samples
};

STD_ROM_PICK(raidendxch)
STD_ROM_FN(raidendxch)

struct BurnDriver BurnDrvRaidendxch = {
	"raidendxch", "raidendx", NULL, NULL, "1994",
	"Raiden DX (China)\0", NULL, "Seibu Kaihatsu (Ideal International Development Corp license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, raidendxchRomInfo, raidendxchRomName, NULL, NULL, RaidendxInputInfo, RaidendxDIPInfo,
	RaidendxInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Zero Team USA (set 1, US, Fabtek license)

static struct BurnRomInfo zeroteamRomDesc[] = {
	{ "seibu__1.u024.5k",		0x040000, 0x25aa5ba4, 1 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "seibu__3.u023.6k",		0x040000, 0xec79a12b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "seibu__2.u025.6l",		0x040000, 0x54f3d359, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "seibu__4.u026.5l",		0x040000, 0xa017b8d0, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "copx-d2.u0313.6n",		0x040000, 0xa6732ff9, 2 | BRF_PRG | BRF_OPT }, //  4 COPX MCU data

	{ "seibu__5.u1110.5b", 		0x010000, 0x7ec1fbc3, 3 | BRF_PRG | BRF_ESS }, //  5 Z80 Code

	{ "seibu__7.u072.5s",		0x010000, 0x9f6aa0f0, 4 | BRF_GRA },           //  6 Characters
	{ "seibu__8.u077.5r",		0x010000, 0x68f7dddc, 4 | BRF_GRA },           //  7

	{ "musha_back-1.u075.4s",  	0x100000, 0x8b7f9219, 5 | BRF_GRA },           //  8 Tiles
	{ "musha_back-2.u0714.2s",  0x080000, 0xce61c952, 5 | BRF_GRA },           //  9

	{ "musha_obj-1.u0811.6f",	0x200000, 0x45be8029, 6 | BRF_GRA },           // 10 Sprites (Encrypted)
	{ "musha_obj-2.u082.5f",	0x200000, 0xcb61c19d, 6 | BRF_GRA },           // 11

	{ "seibu__6.u105.4a",		0x040000, 0x48be32b1, 7 | BRF_SND },           // 12 OKI Samples

	{ "v3c001.pal.u0310.jed", 	     		0x000288, 0x00000000, 0 | BRF_OPT },	       // 13 Pals
	{ "v3c002.tibpal16l8-25.u0322.jed",  	0x000288, 0x00000000, 0 | BRF_OPT },	       // 14
	{ "v3c003.ami18cv8p-15.u0619.jed",  	0x000288, 0x00000000, 0 | BRF_OPT },	       // 15
	{ "v3c004x.ami18cv8pc-25.u0310.jed", 	0x000288, 0x00000000, 0 | BRF_OPT },	       // 16
};

STD_ROM_PICK(zeroteam)
STD_ROM_FN(zeroteam)

struct BurnDriver BurnDrvZeroteam = {
	"zeroteam", NULL, NULL, NULL, "1993",
	"Zero Team USA (set 1, US, Fabtek license)\0", "Unemulated protection", "Seibu Kaihatsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, zeroteamRomInfo, zeroteamRomName, NULL, NULL, ZeroteamInputInfo, ZeroteamDIPInfo,
	ZeroteamInit, DrvExit, ZeroteamFrame, ZeroteamDraw, DrvScan, &DrvRecalc, 0x800,
	320, 256, 4, 3
};


// Zero Team (set 2, Japan? (earlier?))

static struct BurnRomInfo zeroteamaRomDesc[] = {
	{ "1.u024.5k",				0x040000, 0xbd7b3f3a, 1 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "3.u023.6k",				0x040000, 0x19e02822, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "2.u025.6l",				0x040000, 0x0580b7e8, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.u026.5l",				0x040000, 0xcc666385, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "copx-d2.u0313.6n",		0x040000, 0xa6732ff9, 2 | BRF_PRG | BRF_OPT }, //  4 COPX MCU data

	{ "5.a.u1110.5b",			0x010000, 0xefc484ca, 3 | BRF_PRG | BRF_ESS }, //  5 Z80 Code
	
	{ "7.a.u072.5s",			0x010000, 0xeb10467f, 4 | BRF_GRA },           //  6 Characters
	{ "8.a.u077.5r",			0x010000, 0xa0b2a09a, 4 | BRF_GRA },           //  7

	{ "musha_back-1.u075.4s",	0x100000, 0x8b7f9219, 5 | BRF_GRA },           //  8 Tiles
	{ "musha_back-2.u0714.2s",	0x080000, 0xce61c952, 5 | BRF_GRA },           //  9

	{ "musha_obj-1.u0811.6f",	0x200000, 0x45be8029, 6 | BRF_GRA },           // 10 Sprites (Encrypted)
	{ "musha_obj-2.u082.5f",	0x200000, 0xcb61c19d, 6 | BRF_GRA },           // 11

	{ "6.u105.4a",				0x040000, 0x48be32b1, 7 | BRF_SND },           // 12 OKI Samples

	{ "v3c001.pal.u0310.jed",				0x000288, 0x00000000, 8 | BRF_NODUMP },       // 13 pals
	{ "v3c002.tibpal16l8-25.u0322.jed",		0x000288, 0x00000000, 8 | BRF_NODUMP },       // 14
	{ "v3c003.ami18cv8p-15.u0619.jed",		0x000288, 0x00000000, 8 | BRF_NODUMP },       // 15
	{ "v3c004x.ami18cv8pc-25.u0310.jed",	0x000288, 0x00000000, 8 | BRF_NODUMP },       // 16
};

STD_ROM_PICK(zeroteama)
STD_ROM_FN(zeroteama)

struct BurnDriver BurnDrvZeroteama = {
	"zeroteama", "zeroteam", NULL, NULL, "1993",
	"Zero Team (set 2, Japan? (earlier?))\0", "Unemulated protection", "Seibu Kaihatsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, zeroteamaRomInfo, zeroteamaRomName, NULL, NULL, ZeroteamInputInfo, ZeroteamDIPInfo,
	ZeroteamInit, DrvExit, ZeroteamFrame, ZeroteamDraw, DrvScan, &DrvRecalc, 0x800,
	320, 256, 4, 3
};


// Zero Team (set 3, Japan? (later batteryless))

static struct BurnRomInfo zeroteambRomDesc[] = {
	{ "1b.u024.5k",				0x040000, 0x157743d0, 1 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "3b.u023.6k",				0x040000, 0xfea7e4e8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "2b.u025.6l",				0x040000, 0x21d68f62, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4b.u026.5l",				0x040000, 0xce8fe6c2, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "copx-d2.u0313.6n",		0x040000, 0xa6732ff9, 2 | BRF_PRG | BRF_OPT }, //  4 COPX MCU data

	{ "5.u1110.5b",				0x010000, 0x7ec1fbc3, 3 | BRF_PRG | BRF_ESS }, //  5 Z80 Code

	{ "7.u072.5s",				0x010000, 0x9f6aa0f0, 4 | BRF_GRA },           //  6 Characters
	{ "8.u077.5r",				0x010000, 0x68f7dddc, 4 | BRF_GRA },           //  7

	{ "musha_back-1.u075.4s",	0x100000, 0x8b7f9219, 5 | BRF_GRA },           //  8 Tiles
	{ "musha_back-2.u0714.2s",	0x080000, 0xce61c952, 5 | BRF_GRA },           //  9

	{ "musha_obj-1.u0811.6f",	0x200000, 0x45be8029, 6 | BRF_GRA },           // 10 Sprites (Encrypted)
	{ "musha_obj-2.u082.5f",	0x200000, 0xcb61c19d, 6 | BRF_GRA },           // 11

	{ "6.u105.4a",				0x040000, 0x48be32b1, 7 | BRF_SND },           // 12 OKI Samples

	{ "v3c001.pal.u0310.jed",				0x000288, 0x00000000, 8 | BRF_NODUMP },        // 13 pals
	{ "v3c002.tibpal16l8-25.u0322.jed",		0x000288, 0x00000000, 8 | BRF_NODUMP },        // 14
	{ "v3c003.ami18cv8p-15.u0619.jed",		0x000288, 0x00000000, 8 | BRF_NODUMP },        // 15
	{ "v3c004x.ami18cv8pc-25.u0310.jed",	0x000288, 0x00000000, 8 | BRF_NODUMP },        // 16
};

STD_ROM_PICK(zeroteamb)
STD_ROM_FN(zeroteamb)

struct BurnDriver BurnDrvZeroteamb = {
	"zeroteamb", "zeroteam", NULL, NULL, "1993",
	"Zero Team (set 3, Japan? (later batteryless))\0", "Unemulated protection", "Seibu Kaihatsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, zeroteambRomInfo, zeroteambRomName, NULL, NULL, ZeroteamInputInfo, ZeroteamDIPInfo,
	ZeroteamInit, DrvExit, ZeroteamFrame, ZeroteamDraw, DrvScan, &DrvRecalc, 0x800,
	320, 256, 4, 3
};


// Zero Team (set 4, Taiwan, Liang Hwa license)

static struct BurnRomInfo zeroteamcRomDesc[] = {
	{ "b1.u024.5k",				0x040000, 0x528de3b9, 1 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "b3.u023.6k",				0x040000, 0x3688739a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "b2.u025.6l",				0x040000, 0x5176015e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "b4.u026.5l",				0x040000, 0xc79925cb, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "copx-d2.u0313.6n",		0x040000, 0xa6732ff9, 2 | BRF_PRG | BRF_OPT }, //  4 COPX MCU data

	{ "5.c.u1110.5b",			0x010000, 0xefc484ca, 3 | BRF_PRG | BRF_ESS }, //  5 Z80 Code

	{ "b7.u072.5s",				0x010000, 0x30ec0241, 4 | BRF_GRA },           //  6 Characters
	{ "b8.u077.5r",				0x010000, 0xe18b3a75, 4 | BRF_GRA },           //  7

	{ "musha_back-1.u075.4s",	0x100000, 0x8b7f9219, 5 | BRF_GRA },           //  8 Tiles
	{ "musha_back-2.u0714.2s",	0x080000, 0xce61c952, 5 | BRF_GRA },           //  9

	{ "musha_obj-1.u0811.6f",	0x200000, 0x45be8029, 6 | BRF_GRA },           // 10 Sprites (Encrypted)
	{ "musha_obj-2.u082.5f",	0x200000, 0xcb61c19d, 6 | BRF_GRA },           // 11

	{ "6.c.u105.4a",			0x040000, 0xb4a6e899, 7 | BRF_SND },           // 12 OKI Samples

	{ "v3c001.pal.u0310.jed",				0x000288, 0x00000000, 8 | BRF_NODUMP },        // 13 pals
	{ "v3c002.tibpal16l8-25.u0322.jed",		0x000288, 0x00000000, 8 | BRF_NODUMP },        // 14
	{ "v3c003.ami18cv8p-15.u0619.jed",		0x000288, 0x00000000, 8 | BRF_NODUMP },        // 15
	{ "v3c004x.ami18cv8pc-25.u0310.jed",	0x000288, 0x00000000, 8 | BRF_NODUMP },        // 16
};

STD_ROM_PICK(zeroteamc)
STD_ROM_FN(zeroteamc)

struct BurnDriver BurnDrvZeroteamc = {
	"zeroteamc", "zeroteam", NULL, NULL, "1993",
	"Zero Team (set 4, Taiwan, Liang Hwa license)\0", "Unemulated protection", "Seibu Kaihatsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, zeroteamcRomInfo, zeroteamcRomName, NULL, NULL, ZeroteamInputInfo, ZeroteamDIPInfo,
	ZeroteamInit, DrvExit, ZeroteamFrame, ZeroteamDraw, DrvScan, &DrvRecalc, 0x800,
	320, 256, 4, 3
};


// Zero Team (set 5, Korea, Dream Soft license)

static struct BurnRomInfo zeroteamdRomDesc[] = {
	{ "1.d.u024.5k",			0x040000, 0x6cc279be, 1 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "3.d.u023.6k",			0x040000, 0x0212400d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "4.d.u025.6l",			0x040000, 0x08813ebb, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "2.d.u026.5l",			0x040000, 0x9236129d, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "copx-d2.u0313.6n",		0x040000, 0xa6732ff9, 2 | BRF_PRG | BRF_OPT }, //  4 COPX MCU data

	{ "512kb.u1110.5b",			0x010000, 0xefc484ca, 3 | BRF_PRG | BRF_ESS }, //  5 Z80 Code

	{ "512kb.u072.5s",			0x010000, 0x30ec0241, 4 | BRF_GRA },           //  6 Characters
	{ "512kb.u077.5r",			0x010000, 0xe18b3a75, 4 | BRF_GRA },           //  7

	{ "musha_back-1.u075.4s",	0x100000, 0x8b7f9219, 5 | BRF_GRA },           //  8 Tiles
	{ "musha_back-2.u0714.2s",	0x080000, 0xce61c952, 5 | BRF_GRA },           //  9

	{ "musha_obj-1.u0811.6f",	0x200000, 0x45be8029, 6 | BRF_GRA },           // 10 Sprites (Encrypted)
	{ "musha_obj-2.u082.5f",	0x200000, 0xcb61c19d, 6 | BRF_GRA },           // 11

	{ "8.u105.4a",				0x040000, 0xb4a6e899, 7 | BRF_SND },           // 12 OKI Samples

	{ "v3c001.pal.u0310.jed",				0x000288, 0x00000000, 8 | BRF_NODUMP },        // 13 pals
	{ "v3c002.tibpal16l8-25.u0322.jed",		0x000288, 0x00000000, 8 | BRF_NODUMP },        // 14
	{ "v3c003.ami18cv8p-15.u0619.jed",		0x000288, 0x00000000, 8 | BRF_NODUMP },        // 15
	{ "v3c004x.ami18cv8pc-25.u0310.jed",	0x000288, 0x00000000, 8 | BRF_NODUMP },        // 16
};

STD_ROM_PICK(zeroteamd)
STD_ROM_FN(zeroteamd)

struct BurnDriver BurnDrvZeroteamd = {
	"zeroteamd", "zeroteam", NULL, NULL, "1993",
	"Zero Team (set 5, Korea, Dream Soft license)\0", "Unemulated protection", "Seibu Kaihatsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, zeroteamdRomInfo, zeroteamdRomName, NULL, NULL, ZeroteamInputInfo, ZeroteamDIPInfo,
	ZeroteamInit, DrvExit, ZeroteamFrame, ZeroteamDraw, DrvScan, &DrvRecalc, 0x800,
	320, 256, 4, 3
};


// Zero Team Selection

static struct BurnRomInfo zeroteamsRomDesc[] = {
	{ "1_sel.bin",				0x040000, 0xd99d6273, 1 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "3_sel.bin",				0x040000, 0x0a9fe0b1, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "2_sel.bin",				0x040000, 0x4e114e74, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4_sel.bin",				0x040000, 0x0df8ba94, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "copx-d2.u0313.6n",		0x040000, 0xa6732ff9, 2 | BRF_PRG | BRF_OPT }, //  4 COPX MCU data

	{ "5_sel.bin",				0x010000, 0xed91046c, 3 | BRF_PRG | BRF_ESS }, //  5 Z80 Code

	{ "7.u072.5s",				0x010000, 0x9f6aa0f0, 4 | BRF_GRA },           //  6 Characters
	{ "8.u077.5r",				0x010000, 0x68f7dddc, 4 | BRF_GRA },           //  7

	{ "musha_back-1.u075.4s",	0x100000, 0x8b7f9219, 5 | BRF_GRA },           //  8 Tiles
	{ "musha_back-2.u0714.2s",	0x080000, 0xce61c952, 5 | BRF_GRA },           //  9

	{ "musha_obj-1.u0811.6f",	0x200000, 0x45be8029, 6 | BRF_GRA },           // 10 Sprites (Encrypted)
	{ "musha_obj-2.u082.5f",	0x200000, 0xcb61c19d, 6 | BRF_GRA },           // 11

	{ "6.u105.4a",				0x040000, 0x48be32b1, 7 | BRF_SND },           // 12 oki

	{ "v3c001.pal.u0310.jed",				0x000288, 0x00000000, 8 | BRF_NODUMP },        // 13 pals
	{ "v3c002.tibpal16l8-25.u0322.jed",		0x000288, 0x00000000, 8 | BRF_NODUMP },        // 14
	{ "v3c003.ami18cv8p-15.u0619.jed",		0x000288, 0x00000000, 8 | BRF_NODUMP },        // 15
	{ "v3c004x.ami18cv8pc-25.u0310.jed",	0x000288, 0x00000000, 8 | BRF_NODUMP },        // 16
};

STD_ROM_PICK(zeroteams)
STD_ROM_FN(zeroteams)

struct BurnDriver BurnDrvZeroteams = {
	"zeroteams", "zeroteam", NULL, NULL, "1993",
	"Zero Team Selection\0", "Unemulated protection", "Seibu Kaihatsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, zeroteamsRomInfo, zeroteamsRomName, NULL, NULL, ZeroteamInputInfo, ZeroteamDIPInfo,
	ZeroteamInit, DrvExit, ZeroteamFrame, ZeroteamDraw, DrvScan, &DrvRecalc, 0x800,
	320, 256, 4, 3
};


// Zero Team Suicide Revival Kit

static struct BurnRomInfo zeroteamsrRomDesc[] = {
	{ "zteam1.u24",				0x040000, 0xc531e009, 1 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "zteam3.u23",				0x040000, 0x1f988808, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "zteam2.u25",				0x040000, 0xb7234b93, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "zteam4.u26",				0x040000, 0xc2d26708, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "copx-d2.u0313.6n",		0x040000, 0xa6732ff9, 2 | BRF_PRG | BRF_OPT }, //  4 COPX MCU data

	{ "5.5c",					0x010000, 0x7ec1fbc3, 3 | BRF_PRG | BRF_ESS }, //  5 Z80 Code

	{ "7.u072.5s",				0x010000, 0x9f6aa0f0, 4 | BRF_GRA },           //  6 Characters
	{ "8.u077.5r",				0x010000, 0x68f7dddc, 4 | BRF_GRA },           //  7

	{ "musha_back-1.u075.4s",	0x100000, 0x8b7f9219, 5 | BRF_GRA },           //  8 Tiles
	{ "musha_back-2.u0714.2s",	0x080000, 0xce61c952, 5 | BRF_GRA },           //  9

	{ "musha_obj-1.u0811.6f",	0x200000, 0x45be8029, 6 | BRF_GRA },           // 10 Sprites (Encrypted)
	{ "musha_obj-2.u082.5f",	0x200000, 0xcb61c19d, 6 | BRF_GRA },           // 11

	{ "6.u105.4a",				0x040000, 0x48be32b1, 7 | BRF_SND },           // 12 OKI Samples

	{ "v3c001.pal.u0310.jed",				0x000288, 0x00000000, 8 | BRF_NODUMP },        // 13 pals
	{ "v3c002.tibpal16l8-25.u0322.jed",		0x000288, 0x00000000, 8 | BRF_NODUMP },        // 14
	{ "v3c003.ami18cv8p-15.u0619.jed",		0x000288, 0x00000000, 8 | BRF_NODUMP },        // 15
	{ "v3c004x.ami18cv8pc-25.u0310.jed",	0x000288, 0x00000000, 8 | BRF_NODUMP },        // 16
};

STD_ROM_PICK(zeroteamsr)
STD_ROM_FN(zeroteamsr)

struct BurnDriver BurnDrvZeroteamsr = {
	"zeroteamsr", "zeroteam", NULL, NULL, "1993",
	"Zero Team Suicide Revival Kit\0", "No game code! Black screen normal!", "Seibu Kaihatsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, zeroteamsrRomInfo, zeroteamsrRomName, NULL, NULL, ZeroteamInputInfo, ZeroteamDIPInfo,
	ZeroteamInit, DrvExit, ZeroteamFrame, ZeroteamDraw, DrvScan, &DrvRecalc, 0x800,
	320, 256, 4, 3
};


// X Se Dae Quiz (Korea)

static struct BurnRomInfo xsedaeRomDesc[] = {
	{ "1.u024",				0x040000, 0x185437f9, 1 | BRF_PRG | BRF_ESS }, //  0 V30 Code
	{ "2.u025",				0x040000, 0xa2b052df, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.u023",				0x040000, 0x293fd6c1, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.u026",				0x040000, 0x5adf20bf, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "8.u1110", 			0x020000, 0x2dc2f81a, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "6.u072.5s",			0x010000, 0xa788402d, 4 | BRF_GRA },           //  5 Characters
	{ "5.u077.5r",			0x010000, 0x478deced, 4 | BRF_GRA },           //  6

	{ "bg-1.u075",			0x100000, 0xac087560, 5 | BRF_GRA },           //  7 Tiles
	{ "7.u0714",			0x080000, 0x296105dc, 5 | BRF_GRA },           //  8

	{ "obj-1.u0811",		0x200000, 0x6ae993eb, 6 | BRF_GRA },           //  9 Sprites
	{ "obj-2.u082",			0x200000, 0x26c806ee, 6 | BRF_GRA },           // 10

	{ "9.u105.4a",			0x040000, 0xa7a0c5f9, 7 | BRF_SND },           // 11 OKI Samples
};

STD_ROM_PICK(xsedae)
STD_ROM_FN(xsedae)

struct BurnDriverD BurnDrvXsedae = {
	"xsedae", NULL, NULL, NULL, "1993",
	"X Se Dae Quiz (Korea)\0", NULL, "Dream Island", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	0, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, xsedaeRomInfo, xsedaeRomName, NULL, NULL, Raiden2InputInfo, XsedaeDIPInfo,
	XsedaeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	320, 256, 4, 3
};


// Raiden II New / Raiden DX (newer V33 PCB) (Raiden DX EEPROM)

static struct BurnRomInfo r2dx_v33RomDesc[] = {
	{ "prg.223",			0x400000, 0xb3dbcf98, 1 | BRF_PRG | BRF_ESS }, //  0 V33 Code

	{ "fix.613",			0x040000, 0x3da27e39, 2 | BRF_GRA },           //  1 Characters

	{ "bg.612",				0x400000, 0x162c61e9, 3 | BRF_GRA },           //  2 Tiles

	{ "obj1.724",			0x400000, 0x7d218985, 4 | BRF_GRA },           //  3 Sprites (Encrypted)
	{ "obj2.725",			0x400000, 0x891b24d6, 4 | BRF_GRA },           //  4

	{ "pcm.099",			0x100000, 0x97ca2907, 5 | BRF_SND },           //  5 OKI Samples

	{ "copx_d3.357",		0x020000, 0xfa2cf3ad, 6 | BRF_GRA },           //  6 Copx data

	{ "raidendx_eeprom-r2dx_v33.bin", 	0x000080, 0x0b34c0ca, 7 | BRF_PRG | BRF_ESS }, //  7 EEPROM
};

STD_ROM_PICK(r2dx_v33)
STD_ROM_FN(r2dx_v33)

struct BurnDriver BurnDrvR2dx_v33 = {
	"r2dx_v33", NULL, NULL, NULL, "1996",
	"Raiden II New / Raiden DX (newer V33 PCB) (Raiden DX EEPROM)\0", "Terrible sound quality is normal for this game, use Raiden DX instead!", "Seibu Kaihatsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, r2dx_v33RomInfo, r2dx_v33RomName, NULL, NULL, Rdx_v33InputInfo, Rdx_v33DIPInfo,
	R2dxInit, DrvExit, R2dxFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Raiden II New / Raiden DX (newer V33 PCB) (Raiden II EEPROM)

static struct BurnRomInfo r2dx_v33_r2RomDesc[] = {
	{ "prg.223",			0x400000, 0xb3dbcf98, 1 | BRF_PRG | BRF_ESS }, //  0 V33 Code

	{ "fix.613",			0x040000, 0x3da27e39, 2 | BRF_GRA },           //  1 Characters

	{ "bg.612",				0x400000, 0x162c61e9, 3 | BRF_GRA },           //  2 Tiles

	{ "obj1.724",			0x400000, 0x7d218985, 4 | BRF_GRA },           //  3 Sprites (Encrypted)
	{ "obj2.725",			0x400000, 0x891b24d6, 4 | BRF_GRA },           //  4

	{ "pcm.099",			0x100000, 0x97ca2907, 5 | BRF_SND },           //  5 OKI Samples

	{ "copx_d3.357",		0x020000, 0xfa2cf3ad, 6 | BRF_GRA },           //  6 Copx data

	{ "raidenii_eeprom-r2dx_v33.bin", 	0x000080, 0xba454777, 7 | BRF_PRG | BRF_ESS }, //  7 EEPROM
};

STD_ROM_PICK(r2dx_v33_r2)
STD_ROM_FN(r2dx_v33_r2)

struct BurnDriver BurnDrvR2dx_v33_r2 = {
	"r2dx_v33_r2", "r2dx_v33", NULL, NULL, "1996",
	"Raiden II New / Raiden DX (newer V33 PCB) (Raiden II EEPROM)\0", "Terrible sound quality is normal for this game, use Raiden II instead!", "Seibu Kaihatsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, r2dx_v33_r2RomInfo, r2dx_v33_r2RomName, NULL, NULL, Rdx_v33InputInfo, Rdx_v33DIPInfo,
	R2dxInit, DrvExit, R2dxFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// New Zero Team (V33 SYSTEM TYPE_B hardware)

static struct BurnRomInfo nzeroteamRomDesc[] = {
	{ "SEIBU_1.U0224",		0x080000, 0xce1bcaf4, 1 | BRF_PRG | BRF_ESS }, //  0 V33 Code
	{ "SEIBU_2.U0226",		0x080000, 0x03f6e32d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "copx-d3.bin",		0x020000, 0xfa2cf3ad, 2 | BRF_GRA },           //  2 Copx data

	{ "SEIBU_3.U01019",		0x010000, 0x7ec1fbc3, 3 | BRF_PRG | BRF_ESS }, //  3 Z80 Code

	{ "SEIBU_5.U0616",		0x010000, 0xce68ba3c, 4 | BRF_GRA },           //  4 Characters
	{ "SEIBU_6.U0617",		0x010000, 0xcf44aea7, 4 | BRF_GRA },           //  5

	{ "back-1",				0x100000, 0x8b7f9219, 5 | BRF_GRA },           //  6 Tiles
	{ "back-2",				0x080000, 0xce61c952, 5 | BRF_GRA },           //  7

	{ "obj-1",				0x200000, 0x45be8029, 6 | BRF_GRA },           //  8 Sprites (Encrypted)
	{ "obj-2",				0x200000, 0xcb61c19d, 6 | BRF_GRA },           //  9

	{ "SEIBU_4.U099",		0x040000, 0x48be32b1, 7 | BRF_SND },           // 10 OKI Samples
	
	{ "SYSV33B-2.U0227.bin",	0x0117, 0xd9f4612f, 0 | BRF_OPT },
	{ "SYSV33B-1.U0222.bin",	0x0117, 0xf514a11f, 0 | BRF_OPT },
};

STD_ROM_PICK(nzeroteam)
STD_ROM_FN(nzeroteam)

struct BurnDriver BurnDrvNzeroteam = {
	"nzeroteam", "zeroteam", NULL, NULL, "1997",
	"New Zero Team (V33 SYSTEM TYPE_B hardware)\0", NULL, "Seibu Kaihatsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, nzeroteamRomInfo, nzeroteamRomName, NULL, NULL, NzeroteaInputInfo, NzeroteaDIPInfo,
	NzeroteamInit, DrvExit, ZeroteamFrame, ZeroteamDraw, DrvScan, &DrvRecalc, 0x800,
	320, 256, 4, 3
};


// New Zero Team (V33 SYSTEM TYPE_B hardware, China?)

static struct BurnRomInfo nzeroteamaRomDesc[] = {
	{ "prg1",				0x080000, 0x3c7d9410, 1 | BRF_PRG | BRF_ESS }, //  0 V33 Code
	{ "prg2",				0x080000, 0x6cba032d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "copx-d3.bin",		0x020000, 0xfa2cf3ad, 2 | BRF_GRA },           //  2 Copx data

	{ "sound",				0x010000, 0x7ec1fbc3, 3 | BRF_PRG | BRF_ESS }, //  3 Z80 Code

	{ "fix1",				0x010000, 0x0c4895b0, 4 | BRF_GRA },           //  4 Characters
	{ "fix2",				0x010000, 0x07d8e387, 4 | BRF_GRA },           //  5

	{ "back-1",				0x100000, 0x8b7f9219, 5 | BRF_GRA },           //  6 Tiles
	{ "back-2",				0x080000, 0xce61c952, 5 | BRF_GRA },           //  7

	{ "obj-1",				0x200000, 0x45be8029, 6 | BRF_GRA },           //  8 Sprites (Encrypted)
	{ "obj-2",				0x200000, 0xcb61c19d, 6 | BRF_GRA },           //  9

	{ "6.pcm",				0x040000, 0x48be32b1, 7 | BRF_SND },           // 10 OKI Samples
	
	{ "SYSV33B-2.U0227.bin",	0x0117, 0xd9f4612f, 0 | BRF_OPT },
	{ "SYSV33B-1.U0222.bin",	0x0117, 0xf514a11f, 0 | BRF_OPT },
};

STD_ROM_PICK(nzeroteama)
STD_ROM_FN(nzeroteama)

struct BurnDriver BurnDrvNzeroteama = {
	"nzeroteama", "zeroteam", NULL, NULL, "1997",
	"New Zero Team (V33 SYSTEM TYPE_B hardware, China?)\0", NULL, "Seibu Kaihatsu (Haoyunlai Trading Company license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, nzeroteamaRomInfo, nzeroteamaRomName, NULL, NULL, NzeroteaInputInfo, NzeroteaDIPInfo,
	NzeroteamInit, DrvExit, ZeroteamFrame, ZeroteamDraw, DrvScan, &DrvRecalc, 0x800,
	320, 256, 4, 3
};


// Zero Team 2000

static struct BurnRomInfo zerotm2kRomDesc[] = {
	{ "mt28f800b1.u0230",		0x100000, 0x6ab49d8c, 1 | BRF_PRG | BRF_ESS }, //  0 V33 Code

	{ "mx27c1000mc.u0366",		0x020000, 0xfa2cf3ad, 2 | BRF_GRA },           //  1 Copx data

	{ "syz-02.u019",			0x010000, 0x55371073, 3 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "syz-04.u0616",			0x010000, 0x3515a45f, 4 | BRF_GRA },           //  3 Characters
	{ "syz-03.u0617",			0x010000, 0x02fbf9d7, 4 | BRF_GRA },           //  4

	{ "szy-05.u0614",			0x100000, 0x8b7f9219, 5 | BRF_GRA },           //  5 Tiles
	{ "mt28f400b1.u0619",		0x080000, 0x266acee6, 5 | BRF_GRA },           //  6

	{ "musha_obj-1a.u0729",		0x200000, 0x9b2cf68c, 6 | BRF_GRA },           //  7 Sprites (Scrambled)
	{ "musha_obj-2a.u0730",		0x200000, 0xfcabee05, 6 | BRF_GRA },           //  8

	{ "szy-01.u099",			0x040000, 0x48be32b1, 7 | BRF_SND },           //  9 OKI Samples
};

STD_ROM_PICK(zerotm2k)
STD_ROM_FN(zerotm2k)

struct BurnDriver BurnDrvZerotm2k = {
	"zerotm2k", "zeroteam", NULL, NULL, "2000",
	"Zero Team 2000\0", NULL, "Seibu Kaihatsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, zerotm2kRomInfo, zerotm2kRomName, NULL, NULL, ZeroteamInputInfo, NULL,
	Zerotm2kInit, DrvExit, ZeroteamFrame, ZeroteamDraw, DrvScan, &DrvRecalc, 0x800,
	320, 256, 4, 3
};
