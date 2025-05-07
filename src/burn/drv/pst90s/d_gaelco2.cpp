// FB Alpha Gaelco system 2 driver module
// Based on MAME driver by Manuel Abadia

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "eeprom.h"
#include "gaelco.h"
#include "burn_gun.h"
#include "mcs51.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvMCUROM;
static UINT8 *DrvMCUiRAM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprBuf;
static UINT8 *DrvPalRAM;
static UINT8 *Drv68KRAM;
static UINT8 *Drv68KRAM2;
static UINT8 *DrvShareRAM;
static UINT8 *DrvMCURAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT16 *DrvVidRegs;

static INT32 gun_interrupt;
static UINT32 snowboar_latch;

static INT32 game_select = 0;
static void (*pIRQCallback)(INT32 line);

static INT32 bDualMonitor = 0;
static INT32 wrally2_single = 0;
static UINT32 gfxmask = ~0;
static INT32 nCPUClockSpeed = 0;
static INT32 global_y_offset = -16;

static INT32 has_mcu = 0;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvJoy4[16];
static UINT8 DrvDips[3];
static UINT8 DrvReset;
static UINT16 DrvInputs[4];

// Lightgun stuff
static INT16 LethalGun0 = 0;
static INT16 LethalGun1 = 0;
static INT16 LethalGun2 = 0;
static INT16 LethalGun3 = 0;

// Wrally2 Wheel
static INT16 Analog[2];
static INT16 Analog_Latch[2];
static ButtonToggle GearButton[2];

static struct BurnInputInfo AlighuntInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"	},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"service"	},

	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Alighunt)

static struct BurnInputInfo SnowboarInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy3 + 2,	"diag"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 3,	"service"	},
};

STDINPUTINFO(Snowboar)

static struct BurnInputInfo ManiacsqInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"service"	},

	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Maniacsq)

static struct BurnInputInfo TouchgoInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 8,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 9,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"	},

	{"P3 Coin",			BIT_DIGITAL,	DrvJoy3 + 10,	"p3 coin"	},
	{"P3 Start",		BIT_DIGITAL,	DrvJoy3 + 7,	"p3 start"	},
	{"P3 Up",			BIT_DIGITAL,	DrvJoy3 + 0,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	DrvJoy3 + 1,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	DrvJoy3 + 3,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy3 + 2,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p3 fire 2"	},
	{"P3 Button 3",		BIT_DIGITAL,	DrvJoy3 + 6,	"p3 fire 3"	},

	{"P4 Coin",			BIT_DIGITAL,	DrvJoy3 + 11,	"p4 coin"	},
	{"P4 Start",		BIT_DIGITAL,	DrvJoy4 + 7,	"p4 start"	},
	{"P4 Up",			BIT_DIGITAL,	DrvJoy4 + 0,	"p4 up"		},
	{"P4 Down",			BIT_DIGITAL,	DrvJoy4 + 1,	"p4 down"	},
	{"P4 Left",			BIT_DIGITAL,	DrvJoy4 + 3,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy4 + 2,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy4 + 4,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy4 + 5,	"p4 fire 2"	},
	{"P4 Button 3",		BIT_DIGITAL,	DrvJoy4 + 6,	"p4 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy4 + 8,	"service"	},
	{"Service 1",		BIT_DIGITAL,	DrvJoy4 + 10,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy4 + 9,	"diag"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Touchgo)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo BangInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 start"	},
	{"P1 Button",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 1"	},
	A("P1 Gun X",    	BIT_ANALOG_REL, &LethalGun0,    "mouse x-axis"	),
	A("P1 Gun Y",    	BIT_ANALOG_REL, &LethalGun1,    "mouse y-axis"	),

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 start"	},
	{"P2 Button",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 1"	},
	A("P2 Gun X",    	BIT_ANALOG_REL, &LethalGun2,    "p2 x-axis"	),
	A("P2 Gun Y",    	BIT_ANALOG_REL, &LethalGun3,    "p2 y-axis"	),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy3 + 2,	"diag"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Bang)

static struct BurnInputInfo Wrally2InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 8,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"	},
	{"P1 Accelerator",	BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Shift",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	A("P1 Wheel",       BIT_ANALOG_REL, &Analog[0],		"p1 x-axis"),

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 10,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 7,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 right"	},
	{"P2 Accelerator",	BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Shift",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},
	A("P2 Wheel",       BIT_ANALOG_REL, &Analog[1],		"p2 x-axis"),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy4 + 8,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy4 + 10,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy4 + 9,	"diag"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};
#undef A
STDINPUTINFO(Wrally2)

static struct BurnDIPInfo TouchgoDIPList[]=
{
	{0x28, 0xff, 0xff, 0xff, NULL				},
	{0x29, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x28, 0x01, 0x03, 0x02, "Easy"				},
	{0x28, 0x01, 0x03, 0x03, "Normal"			},
	{0x28, 0x01, 0x03, 0x01, "Hard"				},
	{0x28, 0x01, 0x03, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    2, "Credit configuration"		},
	{0x28, 0x01, 0x04, 0x04, "1 Credit Start/1 Credit Continue"		},
	{0x28, 0x01, 0x04, 0x00, "2 Credits Start/1 Credit Continue"		},

	{0   , 0xfe, 0   ,    2, "Coin Slot"			},
	{0x28, 0x01, 0x08, 0x08, "Independent"			},
	{0x28, 0x01, 0x08, 0x00, "Common"			},
	
	{0   , 0xfe, 0   ,    3, "Monitor Type"			},
	{0x28, 0x01, 0x30, 0x00, "Double monitor, 4 players"	},
	{0x28, 0x01, 0x30, 0x20, "Single monitor, 4 players"	},
	{0x28, 0x01, 0x30, 0x30, "Single monitor, 2 players"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x28, 0x01, 0x40, 0x00, "Off"				},
	{0x28, 0x01, 0x40, 0x40, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x28, 0x01, 0x80, 0x80, "Off"				},
	{0x28, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,   16, "Coin A"			},
	{0x29, 0x01, 0x0f, 0x00, "Disabled/Free Play (if Coin B)"},
	{0x29, 0x01, 0x0f, 0x03, "4 Coins 1 Credits"		},
	{0x29, 0x01, 0x0f, 0x06, "3 Coins 1 Credits"		},
	{0x29, 0x01, 0x0f, 0x09, "2 Coins 1 Credits"		},
	{0x29, 0x01, 0x0f, 0x05, "3 Coins 2 Credits"		},
	{0x29, 0x01, 0x0f, 0x02, "4 Coins 3 Credits"		},
	{0x29, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"		},
	{0x29, 0x01, 0x0f, 0x01, "4 Coins 5 Credits"		},
	{0x29, 0x01, 0x0f, 0x04, "3 Coins 4 Credits"		},
	{0x29, 0x01, 0x0f, 0x08, "2 Coins 3 Credits"		},
	{0x29, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"		},
	{0x29, 0x01, 0x0f, 0x07, "2 Coins 5 Credits"		},
	{0x29, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"		},
	{0x29, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"		},
	{0x29, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"		},
	{0x29, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,   16, "Coin B"			},
	{0x29, 0x01, 0xf0, 0x00, "Disabled/Free Play (if Coin A)"},
	{0x29, 0x01, 0xf0, 0x30, "4 Coins 1 Credits"		},
	{0x29, 0x01, 0xf0, 0x60, "3 Coins 1 Credits"		},
	{0x29, 0x01, 0xf0, 0x90, "2 Coins 1 Credits"		},
	{0x29, 0x01, 0xf0, 0x50, "3 Coins 2 Credits"		},
	{0x29, 0x01, 0xf0, 0x20, "4 Coins 3 Credits"		},
	{0x29, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x29, 0x01, 0xf0, 0x10, "4 Coins 5 Credits"		},
	{0x29, 0x01, 0xf0, 0x40, "3 Coins 4 Credits"		},
	{0x29, 0x01, 0xf0, 0x80, "2 Coins 3 Credits"		},
	{0x29, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"		},
	{0x29, 0x01, 0xf0, 0x70, "2 Coins 5 Credits"		},
	{0x29, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"		},
	{0x29, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"		},
	{0x29, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"		},
	{0x29, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"		},
};

STDDIPINFO(Touchgo)

static struct BurnDIPInfo AlighuntDIPList[]=
{
	{0x15, 0xff, 0xff, 0xff, NULL				},
	{0x16, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,   12, "Coinage"			},
	{0x15, 0x01, 0x0f, 0x07, "4 Coins 1 Credit"		},
	{0x15, 0x01, 0x0f, 0x08, "3 Coins 1 Credit"		},
	{0x15, 0x01, 0x0f, 0x09, "2 Coins 1 Credit"		},
	{0x15, 0x01, 0x0f, 0x05, "3 Coins 2 Credit"		},
	{0x15, 0x01, 0x0f, 0x0f, "1 Coin  1 Credit"		},
	{0x15, 0x01, 0x0f, 0x06, "2 Coin  3 Credits"		},
	{0x15, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"		},
	{0x15, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"		},
	{0x15, 0x01, 0x0f, 0x0c, "1 Coins 4 Credits"		},
	{0x15, 0x01, 0x0f, 0x0b, "1 Coins 5 Credits"		},
	{0x15, 0x01, 0x0f, 0x0a, "1 Coins 6 Credits"		},
	{0x15, 0x01, 0x0f, 0x00, "Free Plan if Coin B too"	},

	{0   , 0xfe, 0   ,   12, "Coinage"			},
	{0x15, 0x01, 0xf0, 0x70, "4 Coins 1 Credit"		},
	{0x15, 0x01, 0xf0, 0x80, "3 Coins 1 Credit"		},
	{0x15, 0x01, 0xf0, 0x90, "2 Coins 1 Credit"		},
	{0x15, 0x01, 0xf0, 0x50, "3 Coins 2 Credit"		},
	{0x15, 0x01, 0xf0, 0xf0, "1 Coin  1 Credit"		},
	{0x15, 0x01, 0xf0, 0x60, "2 Coin  3 Credits"		},
	{0x15, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"		},
	{0x15, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"		},
	{0x15, 0x01, 0xf0, 0xc0, "1 Coins 4 Credits"		},
	{0x15, 0x01, 0xf0, 0xb0, "1 Coins 5 Credits"		},
	{0x15, 0x01, 0xf0, 0xa0, "1 Coins 6 Credits"		},
	{0x15, 0x01, 0xf0, 0x00, "Free Plan if Coin A too"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x16, 0x01, 0x03, 0x02, "Easy"				},
	{0x16, 0x01, 0x03, 0x03, "Normal"			},
	{0x16, 0x01, 0x03, 0x01, "Hard"				},
	{0x16, 0x01, 0x03, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x16, 0x01, 0x0c, 0x08, "1"				},
	{0x16, 0x01, 0x0c, 0x0c, "2"				},
	{0x16, 0x01, 0x0c, 0x04, "3"				},
	{0x16, 0x01, 0x0c, 0x00, "4"				},

	{0   , 0xfe, 0   ,    2, "Sound Type",			},
	{0x16, 0x01, 0x10, 0x00, "Mono"				},
	{0x16, 0x01, 0x10, 0x10, "Stereo"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x16, 0x01, 0x20, 0x00, "Off"				},
	{0x16, 0x01, 0x20, 0x20, "On"				},

	{0   , 0xfe, 0   ,    2, "Joystick"			},
	{0x16, 0x01, 0x40, 0x00, "Analog (Unemulated)"		},
	{0x16, 0x01, 0x40, 0x40, "Standard"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x16, 0x01, 0x80, 0x80, "Off"				},
	{0x16, 0x01, 0x80, 0x00, "On"				},
};

STDDIPINFO(Alighunt)

static struct BurnDIPInfo ManiacsqDIPList[]=
{
	{0x11, 0xff, 0xff, 0xff, NULL				},
	{0x12, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,   12, "Coinage"			},
	{0x11, 0x01, 0x0f, 0x07, "4 Coins 1 Credit"		},
	{0x11, 0x01, 0x0f, 0x08, "3 Coins 1 Credit"		},
	{0x11, 0x01, 0x0f, 0x09, "2 Coins 1 Credit"		},
	{0x11, 0x01, 0x0f, 0x05, "3 Coins 2 Credit"		},
	{0x11, 0x01, 0x0f, 0x0f, "1 Coin  1 Credit"		},
	{0x11, 0x01, 0x0f, 0x06, "2 Coin  3 Credits"		},
	{0x11, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"		},
	{0x11, 0x01, 0x0f, 0x0c, "1 Coins 4 Credits"		},
	{0x11, 0x01, 0x0f, 0x0b, "1 Coins 5 Credits"		},
	{0x11, 0x01, 0x0f, 0x0a, "1 Coins 6 Credits"		},
	{0x11, 0x01, 0x0f, 0x00, "Free Plan if Coin B too"	},

	{0   , 0xfe, 0   ,   12, "Coinage"			},
	{0x11, 0x01, 0xf0, 0x70, "4 Coins 1 Credit"		},
	{0x11, 0x01, 0xf0, 0x80, "3 Coins 1 Credit"		},
	{0x11, 0x01, 0xf0, 0x90, "2 Coins 1 Credit"		},
	{0x11, 0x01, 0xf0, 0x50, "3 Coins 2 Credit"		},
	{0x11, 0x01, 0xf0, 0xf0, "1 Coin  1 Credit"		},
	{0x11, 0x01, 0xf0, 0x60, "2 Coin  3 Credits"		},
	{0x11, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"		},
	{0x11, 0x01, 0xf0, 0xc0, "1 Coins 4 Credits"		},
	{0x11, 0x01, 0xf0, 0xb0, "1 Coins 5 Credits"		},
	{0x11, 0x01, 0xf0, 0xa0, "1 Coins 6 Credits"		},
	{0x11, 0x01, 0xf0, 0x00, "Free Plan if Coin A too"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x12, 0x01, 0x03, 0x02, "Easy"				},
	{0x12, 0x01, 0x03, 0x03, "Normal"			},
	{0x12, 0x01, 0x03, 0x01, "Hard"				},
	{0x12, 0x01, 0x03, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x12, 0x01, 0x20, 0x00, "Off"				},
	{0x12, 0x01, 0x20, 0x20, "On"				},

	{0   , 0xfe, 0   ,    2, "1P Continue"			},
	{0x12, 0x01, 0x40, 0x00, "Off"				},
	{0x12, 0x01, 0x40, 0x40, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x12, 0x01, 0x80, 0x80, "Off"				},
	{0x12, 0x01, 0x80, 0x00, "On"				},
};

STDDIPINFO(Maniacsq)

static struct BurnDIPInfo Wrally2DIPList[]=
{
	{0x16, 0xff, 0xff, 0xfd, NULL				},
	{0x17, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x16, 0x01, 0x01, 0x01, "Off"				},
	{0x16, 0x01, 0x01, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Coin mechanism"		},
	{0x16, 0x01, 0x02, 0x00, "Common"			},
	{0x16, 0x01, 0x02, 0x02, "Independent"			},
	
	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x16, 0x01, 0x04, 0x00, "Off"				},
	{0x16, 0x01, 0x04, 0x04, "On"				},

	{0   , 0xfe, 0   ,    2, "Cabinet 1 Controls"		},
	{0x16, 0x01, 0x08, 0x00, "Analog Wheel / Manual Trans"			},
	{0x16, 0x01, 0x08, 0x08, "Joystick / Automatic Trans"			},

	{0   , 0xfe, 0   ,    2, "Cabinet 2 Controls"		},
	{0x16, 0x01, 0x10, 0x00, "Analog Wheel / Manual Trans"			},
	{0x16, 0x01, 0x10, 0x10, "Joystick / Automatic Trans"			},

	{0   , 0xfe, 0   ,    2, "Monitors (change requires Restart!)"			},
	{0x16, 0x01, 0x20, 0x00, "One"				},
	{0x16, 0x01, 0x20, 0x20, "Two"				},
	
	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x16, 0x01, 0xc0, 0x40, "Easy"				},
	{0x16, 0x01, 0xc0, 0xc0, "Normal"			},
	{0x16, 0x01, 0xc0, 0x80, "Hard"				},
	{0x16, 0x01, 0xc0, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    2, "Free Play"			},
	{0x17, 0x01, 0x01, 0x01, "Off"				},
	{0x17, 0x01, 0x01, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Credit configuration"		},
	{0x17, 0x01, 0x02, 0x00, "Start 2C/Continue 1C"		},
	{0x17, 0x01, 0x02, 0x02, "Start 1C/Continue 1C"		},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x17, 0x01, 0x1c, 0x18, "4 Coins 1 Credits"		},
	{0x17, 0x01, 0x1c, 0x10, "3 Coins 1 Credits"		},
	{0x17, 0x01, 0x1c, 0x08, "2 Coins 1 Credits"		},
	{0x17, 0x01, 0x1c, 0x1c, "1 Coin  1 Credits"		},
	{0x17, 0x01, 0x1c, 0x00, "2 Coins 3 Credits"		},
	{0x17, 0x01, 0x1c, 0x0c, "1 Coin  2 Credits"		},
	{0x17, 0x01, 0x1c, 0x14, "1 Coin  3 Credits"		},
	{0x17, 0x01, 0x1c, 0x04, "1 Coin  4 Credits"		},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x17, 0x01, 0xe0, 0xc0, "4 Coins 1 Credits"		},
	{0x17, 0x01, 0xe0, 0x80, "3 Coins 1 Credits"		},
	{0x17, 0x01, 0xe0, 0x40, "2 Coins 1 Credits"		},
	{0x17, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"		},
	{0x17, 0x01, 0xe0, 0x00, "2 Coins 3 Credits"		},
	{0x17, 0x01, 0xe0, 0x60, "1 Coin  2 Credits"		},
	{0x17, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"		},
	{0x17, 0x01, 0xe0, 0x20, "1 Coin  4 Credits"		},
};

STDDIPINFO(Wrally2)

// Snowboard protection sim. engineered by Samuel Neves & Peter Wilhelmsen

static UINT16 get_lo(UINT32 x)
{
	return ((x & 0x00000010) <<  1) |
			((x & 0x00000800) <<  3) |
			((x & 0x40000000) >> 27) |
			((x & 0x00000005) <<  6) |
			((x & 0x00000008) <<  8) |
			((x & 0x00000040) <<  9) |
			((x & 0x04000000) >> 16) |
			((x & 0x00008000) >> 14) |
			((x & 0x00002000) >> 11) |
			((x & 0x00020000) >> 10) |
			((x & 0x00100000) >>  8) |
			((x & 0x00044000) >>  5) |
			((x & 0x00800000) >> 23) |
			((x & 0x00000020) >>  1);
}

static UINT16 get_hi(UINT32 x)
{
	return ((x & 0x00001400) >>  0) |
			((x & 0x10000000) >> 26) |
			((x & 0x02000000) >> 24) |
			((x & 0x08000000) >> 21) |
			((x & 0x00000002) << 12) |
			((x & 0x01000000) >> 19) |
			((x & 0x20000000) >> 18) |
			((x & 0x80000000) >> 16) |
			((x & 0x00200000) >> 13) |
			((x & 0x00010000) >> 12) |
			((x & 0x00080000) >> 10) |
			((x & 0x00000200) >>  9) |
			((x & 0x00400000) >>  8) |
			((x & 0x00000080) >>  4) |
			((x & 0x00000100) >>  1);
}

static UINT16 get_out(UINT16 x)
{
	return ((x & 0xc840) <<  0) |
			((x & 0x0080) <<  2) |
			((x & 0x0004) <<  3) |
			((x & 0x0008) <<  5) |
			((x & 0x0010) <<  8) |
			((x & 0x0002) <<  9) |
			((x & 0x0001) << 13) |
			((x & 0x0200) >>  9) |
			((x & 0x1400) >>  8) |
			((x & 0x0100) >>  7) |
			((x & 0x2000) >>  6) |
			((x & 0x0020) >>  2);
}

static void __fastcall gaelco2_main_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x218004:
		case 0x218005:
		case 0x218006:
		case 0x218007:
		case 0x218008:
		case 0x218009: {
			UINT8 *r = (UINT8*)DrvVidRegs;
			r[(address - 0x218004) ^ 1] = data;
		}
		return;

		case 0x300009:
			EEPROMWriteBit(data & 1);
		return;

		case 0x30000b:
			EEPROMSetClockLine(data & 0x01);
		return;

		case 0x30000d:
			EEPROMSetCSLine(~data & 0x01);
		return;

		case 0x300001:
		case 0x300003:
		case 0x500000:
		case 0x500001:
			// coin counter
		return;

		case 0x30004a:
		case 0x30004b:
		return; // NOP - maniacsq

		case 0x310000:
		case 0x310001:
			gun_interrupt = 1;
		return;

		case 0x500006:
		case 0x500007:
		return; // NOP - alligator
	}
}

static void __fastcall gaelco2_main_write_word(UINT32 address, UINT16 data)
{
	if ((game_select == 2 || game_select == 3) && (address & 0xff0000) == 0x310000) {
		snowboar_latch = (snowboar_latch << 16) | data;
		return;
	}

	switch (address)
	{
		case 0x218004:
		case 0x218006:
		case 0x218008:
			DrvVidRegs[(address - 0x218004) / 2] = data;
		return;

		case 0x500000:
			// coin counter
		return;

		case 0x300008:
			EEPROMWriteBit(data & 1);
		return;

		case 0x30000a:
			EEPROMSetClockLine(data & 0x01);
		return;

		case 0x30000c:
			EEPROMSetCSLine(~data & 0x01);
		return;

		case 0x310000:
			gun_interrupt = 1;
		return;

		case 0x30004a:
		return; // NOP - maniacsq

		case 0x500006:
		return; // NOP - alligator
	}
}

static UINT16 bang_analog_read(INT32 port)
{
	port = (port / 2) & 3;

	INT32 analog_value = 0x80;

	const INT32 adjust[4] = { 320, 240, 1, 0 };

	switch (port & 3)
	{
		case 0: analog_value = BurnGunReturnX(0);
		        break;
		case 1: analog_value = BurnGunReturnX(1);
		        break;
		case 2: analog_value = BurnGunReturnY(0);
		        break;
		case 3: analog_value = BurnGunReturnY(1);
		        break;
	}

	return ((analog_value * adjust[port>>1]) / 256) + adjust[(port>>1) + 2];
}

static UINT8 __fastcall gaelco2_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x218004:
		case 0x218005:
		case 0x218006:
		case 0x218007:
		case 0x218008:
		case 0x218009: {
			UINT8 *r = (UINT8*)DrvVidRegs;
			return r[(address - 0x218004) ^ 1];
		}

		case 0x300000:
		case 0x300001:
			return DrvInputs[0] >> ((~address & 1) * 8);

		case 0x300002:
		case 0x300003:
		case 0x300010:
		case 0x300011:
			return DrvInputs[1] >> ((~address & 1) * 8);

		case 0x300004:
		case 0x300005:
			return DrvInputs[2] >> ((~address & 1) * 8);

		case 0x300006:
		case 0x300007:
			return DrvInputs[3] >> ((~address & 1) * 8);

		case 0x300020:
		case 0x300021:
		case 0x320000:
		case 0x320001:
			return ((DrvInputs[2] & ~0x40) | (EEPROMRead() ? 0x40 : 0)) >> ((~address & 1) * 8);

		case 0x310000:
		case 0x310001:
		case 0x310002:
		case 0x310003:
		case 0x310004:
		case 0x310005:
		case 0x310006:
		case 0x310007:
			return bang_analog_read(address) >> ((~address & 1) * 8);
	}

	return 0;
}

static UINT16 __fastcall gaelco2_main_read_word(UINT32 address)
{
	if ((game_select == 2 || game_select == 3) && (address & 0xff0000) == 0x310000) {
		UINT16 ret = get_out(((get_lo(snowboar_latch) ^ 0x0010) - (get_hi(snowboar_latch) ^ 0x0024)) ^ 0x5496);
		return (ret >> 8) | (ret << 8);
	}

	switch (address)
	{
		case 0x218004:
		case 0x218006:
		case 0x218008:
			return DrvVidRegs[(address - 0x218004) / 2];

		case 0x300000:
			return DrvInputs[0];

		case 0x300002:
		case 0x300010:
			return DrvInputs[1];

		case 0x300004:
			return DrvInputs[0];

		case 0x300006:
			return DrvInputs[1];

		case 0x320000:
		case 0x300020:
			return (DrvInputs[2] & ~0x40) | (EEPROMRead() ? 0x40 : 0);

		case 0x310000:
		case 0x310002:
		case 0x310004:
		case 0x310006:
			return bang_analog_read(address);
	}

	return 0;
}

static void  __fastcall wrally2_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x218004:
		case 0x218006:
		case 0x218008:
			DrvVidRegs[(address - 0x218004) / 2] = data;
		return;

		case 0x400000:
		case 0x400002:
		case 0x400004:
		case 0x400006:
		case 0x400008:
		case 0x40000a:
		case 0x40000c:
		case 0x40000e:
		case 0x400010:
			// coin counter (ignore for now)
		return;

		case 0x400028:
			if (data == 0) {
				Analog_Latch[0] <<= 1;
				Analog_Latch[1] <<= 1;
			}
		return;

		case 0x400030:
			if (data == 0) {
				Analog_Latch[0] = ProcessAnalog(Analog[0], 1, INPUT_DEADZONE, 0x40, 0xbf) + 0x0a;
				Analog_Latch[1] = ProcessAnalog(Analog[1], 1, INPUT_DEADZONE, 0x40, 0xbf) + 0x0a;
			}
		return;
	}

	bprintf (0, _T("WW: %5.5x, %2.2x\n"), address, data);
}

static void  __fastcall wrally2_main_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x400029:
		case 0x400031:
			wrally2_main_write_word(address & ~1, data);
		return;
	}

	bprintf (0, _T("WB: %5.5x, %2.2x\n"), address, data);
}

static UINT16 __fastcall wrally2_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x300000: // in0
			return (DrvInputs[0] & ~0x40) | ((Analog_Latch[0] >> 1) & 0x40);
		case 0x300004: // in2
			return (DrvInputs[2] & ~0x40) | ((Analog_Latch[1] >> 1) & 0x40);

		case 0x300002: // in1
		case 0x300006: // in3
			return DrvInputs[(address/2) & 3];
	}

	return 0;
}

static UINT8 __fastcall wrally2_main_read_byte(UINT32 address)
{
	return wrally2_main_read_word(address & ~1) >> ((~address & 1) * 8);
}

static void __fastcall gaelco2_sound_write_byte(UINT32 address, UINT8 data)
{
	DrvSprRAM[(address & 0xffff) ^ 1] = data;

	if (address >= 0x202890 && address <= 0x2028ff) {
		// not used.
		return;
	}
}

static void __fastcall gaelco2_sound_write_word(UINT32 address, UINT16 data)
{
	*((UINT16*)(DrvSprRAM + (address & 0xfffe))) = BURN_ENDIAN_SWAP_INT16(data);

	if (address >= 0x202890 && address <= 0x2028ff) {
		gaelcosnd_w((address - 0x202890) >> 1, data);
		return;
	}
}

static UINT8 __fastcall gaelco2_sound_read_byte(UINT32 address)
{
	if (address >= 0x202890 && address <= 0x2028ff) {
		// not used.
	}
	return DrvSprRAM[(address & 0xffff) ^ 1];
}

static UINT16 __fastcall gaelco2_sound_read_word(UINT32 address)
{
	if (address >= 0x202890 && address <= 0x2028ff) {
		return gaelcosnd_r((address - 0x202890) >> 1);
	}
	return BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvSprRAM + (address & 0xfffe))));
}

static void dallas_sharedram_write(INT32 address, UINT8 data)
{
	if (address >= MCS51_PORT_P0) return;

	if (address >= 0x8000 && address <= 0xffff)
		DrvShareRAM[(address & 0x7fff) ^ 1] = data;

	if (address >= 0x10000 && address <= 0x17fff)
		DrvMCURAM[address & 0x7fff] = data;
}

static UINT8 dallas_sharedram_read(INT32 address)
{
	if (address >= MCS51_PORT_P0) return 0;

	if (address >= 0x8000 && address <= 0xffff)
		return DrvShareRAM[(address & 0x7fff) ^ 1];

	if (address >= 0x10000 && address <= 0x17fff)
		return DrvMCURAM[address & 0x7fff];

	return 0;
}

static void palette_update(INT32 offset)
{
	static const int pen_color_adjust[16] = {
		0, -8, -16, -24, -32, -40, -48, -56, 64, 56, 48, 40, 32, 24, 16, 8
	};

	offset = (offset & 0x1ffe);

	UINT16 color = BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvPalRAM + offset)));

	INT32 r = (color >> 10) & 0x1f;
	INT32 g = (color >>  5) & 0x1f;
	INT32 b = (color >>  0) & 0x1f;

	r = (r << 3) | (r >> 2);
	g = (g << 3) | (g >> 2);
	b = (b << 3) | (b >> 2);

	DrvPalette[offset/2] = BurnHighCol(r,g,b,0);

#define ADJUST_COLOR(c) ((c < 0) ? 0 : ((c > 255) ? 255 : c))

	for (INT32 i = 1; i < 16; i++) {
		INT32 auxr = ADJUST_COLOR(r + pen_color_adjust[i]);
		INT32 auxg = ADJUST_COLOR(g + pen_color_adjust[i]);
		INT32 auxb = ADJUST_COLOR(b + pen_color_adjust[i]);

		DrvPalette[0x1000 * i + (offset/2)] = BurnHighCol(auxr,auxg,auxb,0);
	}
}

static void __fastcall gaelco2_palette_write_byte(UINT32 address, UINT8 data)
{
	DrvPalRAM[(address & 0x1fff) ^ 1] = data;
	palette_update(address);
}

static void __fastcall gaelco2_palette_write_word(UINT32 address, UINT16 data)
{
	*((UINT16*)(DrvPalRAM + (address & 0x1ffe))) = BURN_ENDIAN_SWAP_INT16(data);
	palette_update(address);
}

static void pIRQLine6Callback(INT32 line)
{
	if (line == 255) // ??
		SekSetIRQLine(6, CPU_IRQSTATUS_AUTO);
}

static void pBangIRQLineCallback(INT32 line)
{
	line++;

	if (line == 256) {
		SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);
		gun_interrupt = 0;
	}

	if ((line % 64) == 0 && gun_interrupt) {
		SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
	}
}


static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	mcs51_reset();
	if (DrvMCUiRAM[0]) {
		//bprintf(0, _T("confucius say: ds5002fp scratch like chicken in summertime\n"));
		ds5002fp_iram_fill(DrvMCUiRAM, 0x80);
	}

	EEPROMReset();

	HiscoreReset();

	gaelcosnd_reset();

	gun_interrupt = 0;
	snowboar_latch = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x0100000;

	DrvMCURAM		= Next;
	DrvMCUROM		= Next; Next += 0x0008000;
	DrvMCUiRAM      = Next; Next += 0x00000ff;

	DrvGfxROM0		= Next; Next += 0x1400000;
	DrvGfxROM		= Next; Next += 0x2000000;

	DrvPalette		= (UINT32*)Next; Next += 0x10000 * sizeof(UINT32);

	AllRam			= Next;

	DrvSprRAM		= Next; Next += 0x0010000;
	DrvSprBuf		= Next; Next += 0x0010000;
	DrvPalRAM		= Next; Next += 0x0002000;

	DrvShareRAM 	= Next; Next += 0x0008000;
	Drv68KRAM		= Next; Next += 0x0020000;
	Drv68KRAM2		= Next; Next += 0x0002000;

	DrvVidRegs		= (UINT16*)Next; Next += 0x00004 * sizeof(UINT16);

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void DrvGfxDecode(INT32 size)
{
	INT32 Planes[5] = { (size/5)*8*4, (size/5)*8*3, (size/5)*8*2, (size/5)*8*1, (size/5)*8*0 };
	INT32 XOffs[16] = { STEP8(0,1), STEP8(128,1) };
	INT32 YOffs[16] = { STEP16(0,8) };

	GfxDecode(((size*8)/5)/(16*16), 5, 16, 16, Planes, XOffs, YOffs, 0x100, DrvGfxROM0, DrvGfxROM);

	gfxmask = (((size * 8) / 5) / (16 * 16)) - 1;
}

static void gaelco2_split_gfx(UINT8 *src, UINT8 *dst, INT32 start, INT32 length, INT32 dest1, INT32 dest2)
{
	for (INT32 i = 0; i < length/2; i++){
		dst[dest1 + i] = src[start + i*2 + 0];
		dst[dest2 + i] = src[start + i*2 + 1];
	}
}

static eeprom_interface eeprom_interface_93C66_16bit =
{
	8,				// address bits 9 (8 data bits) / 8 (16 data bits)
	16,				// data bits    8 / 16 (93C66)
	"*110",			// read         110 aaaaaaaaa
	"*101",			// write        101 aaaaaaaaa dddddddd
	"*111",			// erase        111 aaaaaaaaa
	"*10000xxxxxx",	// lock         100 00xxxxxxx
	"*10011xxxxxx",	// unlock       100 11xxxxxxx
	0,0
};

static INT32 DrvInit(INT32 game_selector)
{
	BurnAllocMemIndex();

	game_select = game_selector;
	pIRQCallback = pIRQLine6Callback;

	if (BurnLoadRom(Drv68KROM  + 0x000001, 0, 2)) return 1;
	if (BurnLoadRom(Drv68KROM  + 0x000000, 1, 2)) return 1;

	switch (game_select)
	{
		case 0:	// aligatorun
		{
			if (BurnLoadRom(DrvGfxROM + 0x000000, 2, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x400000, 3, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x800000, 4, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0xc00000, 5, 1)) return 1;

			BurnLoadRom(DrvMCUROM, 6, 1);

			gaelco2_split_gfx(DrvGfxROM, DrvGfxROM0, 0x0000000, 0x0400000, 0x0000000, 0x0400000);
			gaelco2_split_gfx(DrvGfxROM, DrvGfxROM0, 0x0400000, 0x0400000, 0x0200000, 0x0600000);
			gaelco2_split_gfx(DrvGfxROM, DrvGfxROM0, 0x0800000, 0x0400000, 0x0800000, 0x0c00000);
			gaelco2_split_gfx(DrvGfxROM, DrvGfxROM0, 0x0c00000, 0x0400000, 0x0a00000, 0x0e00000);

			DrvGfxDecode(0x1400000);

			nCPUClockSpeed = 12000000;

			gaelcosnd_start(DrvGfxROM0, 0 * 0x0400000, 1 * 0x0400000, 2 * 0x0400000, 3 * 0x0400000);
		}
		break;

		case 1:	// maniacsq
		{
			if (BurnLoadRom(DrvGfxROM0 + 0x000000, 2, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x080000, 3, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x100000, 4, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x180000, 5, 1)) return 1;

			BurnLoadRom(DrvMCUROM, 6, 1);

			DrvGfxDecode(0x280000);

			nCPUClockSpeed = 13000000;

			gaelcosnd_start(DrvGfxROM0, 0 * 0x0080000, 1 * 0x0080000, 0, 0);
		}
		break;

		case 2:	// snowboara
		{
			if (BurnLoadRom(DrvGfxROM + 0x000000, 3, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x400000, 4, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x800000, 5, 1)) return 1;

			gaelco2_split_gfx(DrvGfxROM, DrvGfxROM0, 0x0000000, 0x0400000, 0x0000000, 0x0400000);
			gaelco2_split_gfx(DrvGfxROM, DrvGfxROM0, 0x0400000, 0x0400000, 0x0200000, 0x0600000);
			gaelco2_split_gfx(DrvGfxROM, DrvGfxROM0, 0x0800000, 0x0400000, 0x0800000, 0x0c00000);

			if (BurnLoadRom(DrvGfxROM0 + 0x1000000, 2, 1)) return 1;

			DrvGfxDecode(0x1400000);

			nCPUClockSpeed = 15000000;

			gaelcosnd_start(DrvGfxROM0, 0 * 0x0400000, 1 * 0x0400000, 0, 0);
		}
		break;

		case 3:	// snowboar
		{
			if (BurnLoadRom(DrvGfxROM0 + 0x0000000, 2, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0080000, 3, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0100000, 4, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0180000, 5, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0200000, 6, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0280000, 7, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0400000, 8, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0480000, 9, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0500000, 10, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0580000, 11, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0600000, 12, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0680000, 13, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0800000, 14, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0880000, 15, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0900000, 16, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0980000, 17, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0c00000, 18, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0c80000, 19, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0d00000, 20, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0d80000, 21, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x1000000, 22, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x1080000, 23, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x1100000, 24, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x1180000, 25, 1)) return 1;

			DrvGfxDecode(0x1400000);

			nCPUClockSpeed = 15000000;

			gaelcosnd_start(DrvGfxROM0, 0 * 0x0400000, 1 * 0x0400000, 0, 0);
		}
		break;

		case 4: // touchgo
		{
			if (BurnLoadRom(DrvGfxROM0 + 0x1000000, 2, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM  + 0x0000000, 3, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM  + 0x0400000, 4, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM  + 0x0800000, 5, 1)) return 1;

			BurnLoadRom(DrvMCUROM, 6, 1);
			BurnLoadRom(DrvMCUiRAM, 7, 1);

			gaelco2_split_gfx(DrvGfxROM, DrvGfxROM0, 0x0000000, 0x0400000, 0x0000000, 0x0400000);
			gaelco2_split_gfx(DrvGfxROM, DrvGfxROM0, 0x0400000, 0x0200000, 0x0200000, 0x0600000);
			gaelco2_split_gfx(DrvGfxROM, DrvGfxROM0, 0x0800000, 0x0400000, 0x0800000, 0x0c00000);

			DrvGfxDecode(0x1400000);

			nCPUClockSpeed = 16000000;

			gaelcosnd_start(DrvGfxROM0, 0 * 0x0400000, 1 * 0x0400000, 0, 0);
		}
		break;

		case 6: // bang
		{
			if (BurnLoadRom(DrvGfxROM0 + 0x0000000,  2, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0080000,  3, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0100000,  4, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0200000,  5, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0280000,  6, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0300000,  7, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0400000,  8, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0480000,  9, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0500000, 10, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0600000, 11, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0680000, 12, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0700000, 13, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0800000, 14, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0880000, 15, 1)) return 1;

			DrvGfxDecode(0x0a00000);

			nCPUClockSpeed = 15000000;
			pIRQCallback = pBangIRQLineCallback;

			gaelcosnd_start(DrvGfxROM0, 0 * 0x0200000, 1 * 0x0200000, 2 * 0x0200000, 3 * 0x0200000);
		}
		break;

		case 7: // wrally2a
		{
			if (BurnLoadRom(DrvMCUROM  + 0x000000,  2, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM0 + 0x000000,  3, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x080000,  4, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x100000,  5, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x180000,  6, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x200000,  7, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x280000,  8, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x300000,  9, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x380000, 10, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x400000, 11, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x480000, 12, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x600000, 13, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x680000, 14, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x800000, 15, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x880000, 16, 1)) return 1;

			DrvGfxDecode(0x0a00000);

			nCPUClockSpeed = 13000000;
			bDualMonitor = 1;

			gaelcosnd_start(DrvGfxROM0, 0 * 0x0200000, 1 * 0x0200000, 0, 0);
			gaelcosnd_swaplr(); // channels swapped in wrally2
		}
		break;
		
		case 8: // wrally2
		{
			if (BurnLoadRom(DrvMCUROM  + 0x000000,  2, 1)) return 1;
			
			if (BurnLoadRom(DrvGfxROM0 + 0x0800000, 3, 1)) return 1;
			
			if (BurnLoadRom(DrvGfxROM  + 0x0000000, 4, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM  + 0x0400000, 5, 1)) return 1;

			gaelco2_split_gfx(DrvGfxROM, DrvGfxROM0, 0x0000000, 0x0400000, 0x0000000, 0x0200000);
			gaelco2_split_gfx(DrvGfxROM, DrvGfxROM0, 0x0400000, 0x0200000, 0x0400000, 0x0600000);

			DrvGfxDecode(0x0a00000);

			nCPUClockSpeed = 13000000;
			bDualMonitor = 1;

			gaelcosnd_start(DrvGfxROM0, 0 * 0x0200000, 1 * 0x0200000, 0, 0);
			gaelcosnd_swaplr(); // channels swapped in wrally2
		}
		break;
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(DrvSprRAM,		0x200000, 0x20ffff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		0x210000, 0x211fff, MAP_RAM);
	SekMapMemory(Drv68KRAM2, 	0x212000, 0x213fff, MAP_RAM); // snowboard champ. extra ram
	SekMapMemory(Drv68KRAM,		0xfe0000, 0xfe7fff, MAP_RAM);
	SekMapMemory(DrvShareRAM,	0xfe8000, 0xfeffff, MAP_RAM);

	if (game_select != 7 && game_select != 8) {
		SekSetWriteWordHandler(0,	gaelco2_main_write_word);
		SekSetWriteByteHandler(0,	gaelco2_main_write_byte);
		SekSetReadWordHandler(0,	gaelco2_main_read_word);
		SekSetReadByteHandler(0,	gaelco2_main_read_byte);
	} else {
		SekSetWriteWordHandler(0,	wrally2_main_write_word);
		SekSetWriteByteHandler(0,	wrally2_main_write_byte);
		SekSetReadWordHandler(0,	wrally2_main_read_word);
		SekSetReadByteHandler(0,	wrally2_main_read_byte);
	}

	SekMapHandler(1,		0x202800, 0x202bff, MAP_WRITE | MAP_READ);
	SekSetWriteWordHandler(1,	gaelco2_sound_write_word);
	SekSetWriteByteHandler(1,	gaelco2_sound_write_byte);
	SekSetReadWordHandler(1,	gaelco2_sound_read_word);
	SekSetReadByteHandler(1,	gaelco2_sound_read_byte);

	SekMapHandler(2,		0x210000, 0x211fff, MAP_WRITE);
	SekSetWriteWordHandler(2,	gaelco2_palette_write_word);
	SekSetWriteByteHandler(2,	gaelco2_palette_write_byte);
	SekClose();

	has_mcu = (DrvMCUROM[0] == 0x02) ? 1 : 0;

	ds5002fp_init(((game_select == 7 || game_select == 8) ? 0x69 : 0x19), 0, 0x80); // defaults
	mcs51_set_program_data(DrvMCUROM);
	mcs51_set_write_handler(dallas_sharedram_write);
	mcs51_set_read_handler(dallas_sharedram_read);

	EEPROMInit(&eeprom_interface_93C66_16bit);

	GenericTilesInit();

	if (game_select == 7 || game_select == 8) {
		if ((DrvDips[0] & 0x20) == 0x00) { // Single Screen.
			bprintf(0, _T("wrally2: single screen mode (hack).\n"));
			wrally2_single = 1;
			BurnDrvSetVisibleSize(384-16, 240);
			BurnDrvSetAspect(4, 3);
			Reinitialise();     // change window size w/ new size/settings
			GenericTilesExit();
			GenericTilesInit(); // recreate pTransDraw w/ new size

			gaelcosnd_monoize(); // sound only comes out 1 speaker in single screen mode.
		} else {
			// Q: Why is this needed when the BurnDriver struct is set to 384*2, 240?
			// A: BurnDrvSetVis* actually changes the value in the BurnDriver Struct.
			bprintf(0, _T("wrally2: double screen mode.\n"));
			BurnDrvSetVisibleSize(384*2, 240);
			BurnDrvSetAspect(8, 3);
			Reinitialise();
			GenericTilesExit();
			GenericTilesInit();
		}
	}

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	SekExit();

	mcs51_exit();

	EEPROMExit();

	if (game_select == 6)
		BurnGunExit();

	gaelcosnd_exit();

	BurnFree (AllMem);

	game_select = 0;
	bDualMonitor = 0;
	wrally2_single = 0;
	has_mcu = 0;

	return 0;
}

static INT32 get_rowscrollmode_yscroll(INT32 first_screen)
{
	UINT16 base = first_screen ? 0x2000 / 2 : 0x2400 / 2;

	UINT8 checkoffsets[32] = {
		0x02, 0x0e, 0x0a, 0x1b, 0x15, 0x13, 0x04, 0x19,
		0x0c, 0x1f, 0x08, 0x1d, 0x11, 0x06, 0x17, 0x10,
		0x01, 0x0d, 0x16, 0x09, 0x1a, 0x05, 0x1e, 0x00,
		0x12, 0x0b, 0x14, 0x03, 0x1c, 0x18, 0x07, 0x0f };

	UINT16 *ram = (UINT16*)DrvSprRAM;

	INT32 usescroll = 0;
	for (INT32 i = 31; i >= 0; i--)
	{
		INT32 checkoffset = (0x80 / 2) + ((checkoffsets[i] * 3) + 1);

		if (ram[(base)+checkoffset] & 0x1000)
		{
			usescroll = 31 - i;
		}
	}

	return usescroll;
}

static void draw_layer(INT32 layer)
{
	INT32 offset = ((DrvVidRegs[layer] >> 9) & 0x07) * 0x1000;

	UINT16 *ram = (UINT16*)DrvSprRAM;

	INT32 scrolly = (BURN_ENDIAN_SWAP_INT16(ram[(0x2800 + (layer * 4))/2]) + 0x01 - global_y_offset) & 0x1ff;

	INT32 xoffset = 0;

	if (layer && bDualMonitor) xoffset = 384;

	if ((DrvVidRegs[layer] & 0x8000) == 0)
	{
		INT32 scrollx = (BURN_ENDIAN_SWAP_INT16(ram[(0x2802 + (layer * 4))/2]) + 0x10 + (layer ? 0 : 4)) & 0x3ff;

		for (INT32 offs = 0; offs < 64 * 32; offs++)
		{
			INT32 sx = (offs & 0x3f) * 16;
			INT32 sy = (offs / 0x40) * 16;

			sx -= scrollx;
			if (sx < -15) sx += 1024;
			sy -= scrolly;
			if (sy < -15) sy += 512;

			sx += xoffset;

			if (sx >= nScreenWidth || sy >= nScreenHeight) continue;

			INT32 attr0 = BURN_ENDIAN_SWAP_INT16(ram[offset + (offs * 2) + 0]);
			INT32 attr1 = BURN_ENDIAN_SWAP_INT16(ram[offset + (offs * 2) + 1]);

			INT32 code  = (attr1 + ((attr0 & 0x07) << 16)) & gfxmask;

			INT32 color = ((attr0 & 0xfe00) >> 9);
			if (bDualMonitor) color = (color & 0x3f) | (layer ? 0x40 : 0);
			INT32 flipx = (attr0 & 0x0080) ? 0xf : 0;
			INT32 flipy = (attr0 & 0x0040) ? 0xf : 0;

			Draw16x16MaskTile(pTransDraw, code, sx, sy, flipx, flipy, color, 5, 0, 0, DrvGfxROM);
		}
	}
	else
	{
		if (game_select == 4) { // touch & go
			scrolly += get_rowscrollmode_yscroll(layer ^ 1);
		}

		for (INT32 sy = 0; sy < nScreenHeight; sy++)
		{
			INT32 yy = (sy + scrolly) & 0x1ff;
			INT32 yy2 = (game_select == 4) ? yy : sy;

			INT32 scrollx = (BURN_ENDIAN_SWAP_INT16(ram[(0x2802 + (layer * 4))/2]) + 0x10 + (layer ? 0 : 4)) & 0x3ff;
			if (DrvVidRegs[layer] & 0x8000) {
				if (layer) {
					scrollx = (BURN_ENDIAN_SWAP_INT16(ram[(0x2400/2) + yy2]) + 0x10) & 0x3ff;
				} else {
					scrollx = (BURN_ENDIAN_SWAP_INT16(ram[(0x2000/2) + yy2]) + 0x14) & 0x3ff;
				}
			}

			scrollx -= xoffset;

			UINT16 *dst = pTransDraw + sy * nScreenWidth;

			for (INT32 sx = 0; sx < nScreenWidth + 16; sx += 16)
			{
				INT32 sxx = (sx + scrollx) & 0x3ff;
				INT32 xx = sx - (scrollx & 0xf);

				INT32 index = ((sxx / 16) + ((yy / 16) * 64)) * 2;

				INT32 attr0 = ram[offset + index + 0];
				INT32 attr1 = ram[offset + index + 1];

				INT32 code  = (attr1 + ((attr0 & 0x07) << 16)) & gfxmask;

				INT32 color = ((attr0 >> 9) & 0x7f);
				if (bDualMonitor) color = (color & 0x3f) | (layer ? 0x40 : 0);
				color *= 0x20;
				INT32 flipx = (attr0 & 0x80) ? 0xf : 0;
				INT32 flipy = (attr0 & 0x40) ? 0xf : 0;

				UINT8 *gfx = DrvGfxROM + (code * 256) + (((yy & 0x0f) ^ flipy) * 16);

				for (INT32 x = 0; x < 16; x++, xx++)
				{
					if (xx >= 0 && xx < nScreenWidth) {
						if (gfx[x^flipx]) {
							dst[xx] = gfx[x^flipx] + color;
						}
					}
				}
			}
		}
	}
}

static void draw_sprites(INT32 xoffs)
{
	UINT16 *buffered_spriteram16 = (UINT16*)DrvSprBuf;
	int j, x, y, ex, ey, px, py;

	INT32 start_offset = (DrvVidRegs[1] & 0x10)*0x100;
	INT32 end_offset = start_offset + 0x1000;

	INT32 width = (nScreenWidth > 512) ? (nScreenWidth/2) : nScreenWidth;

	if (wrally2_single) width = 384;

	INT32 spr_x_adjust = ((width-1) - 320 + 1) - (511 - 320 - 1) - ((DrvVidRegs[0] >> 4) & 0x01) + xoffs;

	for (j = start_offset; j < end_offset; j += 8)
	{
		int data = BURN_ENDIAN_SWAP_INT16(buffered_spriteram16[(j/2) + 0]);
		int data2 = BURN_ENDIAN_SWAP_INT16(buffered_spriteram16[(j/2) + 1]);
		int data3 = BURN_ENDIAN_SWAP_INT16(buffered_spriteram16[(j/2) + 2]);
		int data4 = BURN_ENDIAN_SWAP_INT16(buffered_spriteram16[(j/2) + 3]);

		int sx = data3 & 0x3ff;
		int sy = data2 & 0x1ff;

		int xflip = data2 & 0x800;
		int yflip = data2 & 0x400;

		int xsize = ((data3 >> 12) & 0x0f) + 1;
		int ysize = ((data2 >> 12) & 0x0f) + 1;

		INT32 screen = data >> 15;
		if (screen && bDualMonitor) {
			if (wrally2_single) continue;
			sx += 384;
		}

		if ((data2 & 0x0200) != 0)
		{
			for (y = 0; y < ysize; y++)
			{
				for (x = 0; x < xsize; x++)
				{
					int data5 = BURN_ENDIAN_SWAP_INT16(buffered_spriteram16[((data4/2) + (y*xsize + x)) & 0x7fff]);
					int number = (((data & 0x1ff) << 10) + (data5 & 0x0fff)) & gfxmask;
					int color = ((data >> 9) & 0x7f) + ((data5 >> 12) & 0x0f);
					int color_effect = bDualMonitor ? ((color & 0x3f) == 0x3f) : (color == 0x7f);

					ex = xflip ? (xsize - 1 - x) : x;
					ey = yflip ? (ysize - 1 - y) : y;

					if (color_effect == 0) // iq_132
					{
					// clip
						INT32 xx = ((sx + ex*16) & 0x3ff) + spr_x_adjust;

						if (bDualMonitor && !wrally2_single) {
							if (screen) {
								if (xx < ((nScreenWidth/2)-15) || xx >= nScreenWidth) continue;
								GenericTilesSetClip(nScreenWidth/2, nScreenWidth, 0, nScreenHeight);
							} else {
								if (xx >= (nScreenWidth/2) || xx < -15) continue;
								GenericTilesSetClip(0, nScreenWidth/2, 0, nScreenHeight);
							}
						}

						Draw16x16MaskTile(pTransDraw, number, xx, ((sy + ey*16) & 0x1ff) + global_y_offset, xflip, yflip, color, 5, 0, 0, DrvGfxROM);

						if (bDualMonitor) GenericTilesClearClip();

					} else {
						const UINT8 *gfx_src = DrvGfxROM + number * 0x100;

						for (py = 0; py < 16; py++)
						{
							int ypos = ((sy + ey*16 + py) & 0x1ff) + global_y_offset;
							if ((ypos < 0) || (ypos >= nScreenHeight)) continue;

							UINT16 *srcy = pTransDraw  + ypos * nScreenWidth;

							int gfx_py = yflip ? (16 - 1 - py) : py;

							for (px = 0; px < 16; px++)
							{
								int xpos = (((sx + ex*16 + px) & 0x3ff) + spr_x_adjust) & 0x3ff;
								if (bDualMonitor && !wrally2_single) {
									if (screen) {
										if (xpos < (nScreenWidth/2) || xpos >= nScreenWidth) continue;
									} else {
										if (xpos >= (nScreenWidth/2) || xpos < -15) continue;
									}
								}
								UINT16 *pixel = srcy + xpos;
								int src_color = *pixel;

								int gfx_px = xflip ? (16 - 1 - px) : px;

								int gfx_pen = gfx_src[16*gfx_py + gfx_px];

								if ((gfx_pen == 0) || (gfx_pen >= 16)) continue;

								if ((xpos < 0) || (xpos >= nScreenWidth)) continue;

								*pixel = src_color + 4096*gfx_pen;
							}
						}
					}
				}
			}
		}
	}
}

static INT32 DualDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x2000; i+=2) {
			palette_update(i);
		}
		DrvRecalc = 0;
	}

	BurnTransferClear();

	if (wrally2_single) {
		if (nBurnLayer & 1) draw_layer(0);
	} else {
		if (nBurnLayer & 1) draw_layer(1);

		GenericTilesSetClip(0, nScreenWidth/2, 0, nScreenHeight);

		if (nBurnLayer & 2) draw_layer(0);

		GenericTilesClearClip();
	}

	if (nBurnLayer & 4) draw_sprites(0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x2000; i+=2) {
			palette_update(i);
		}
		DrvRecalc = 0;
	}

	BurnTransferClear();

	if (nBurnLayer & 1) draw_layer(1);
	if (nBurnLayer & 2) draw_layer(0);
	if (nBurnLayer & 4) draw_sprites(0);

	BurnTransferCopy(DrvPalette);

	if (game_select == 6) {
		BurnGunDrawTargets();
	}

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	SekNewFrame();

	{
		if (game_select == 8) { // wrally2 shifters
			GearButton[0].Toggle(DrvJoy1[5]);
			GearButton[1].Toggle(DrvJoy3[5]);
		}

		memset (DrvInputs, 0xff, 4 * sizeof(UINT16));

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}

		DrvInputs[0] = (DrvInputs[0] & 0xff) | (DrvDips[0] << 8);
		DrvInputs[1] = (DrvInputs[1] & 0xff) | (DrvDips[1] << 8);

		if (game_select == 6) {
			BurnGunMakeInputs(0, (INT16)LethalGun0, (INT16)LethalGun1);
			BurnGunMakeInputs(1, (INT16)LethalGun2, (INT16)LethalGun3);
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { (nCPUClockSpeed * 10) / 591, (1000000 * 10) / 591 }; // ?? MHZ @ 59.1 HZ
	INT32 nCyclesDone[2] = { 0, 0 };

	SekOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);

		pIRQCallback(i);

		if (has_mcu) {
			CPU_RUN(1, mcs51);
		}
	}

	if (pBurnSoundOut) {
		gaelcosnd_update(pBurnSoundOut, nBurnSoundLen);
	}

	SekClose();

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	memcpy (DrvSprBuf, DrvSprRAM, 0x10000);

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

		EEPROMScan(nAction, pnMin);

		if (game_select == 6) {
			BurnGunScan();
		}

		mcs51_scan(nAction);

		SCAN_VAR(snowboar_latch);
		SCAN_VAR(gun_interrupt);

		if (game_select == 8) { // wrally2
			SCAN_VAR(Analog_Latch);
			GearButton[0].Scan();
			GearButton[1].Scan();
		}

		gaelcosnd_scan(nAction, pnMin);
	}

	return 0;
}


// Maniac Square (protected, version 1.0, checksum DEEE)

static struct BurnRomInfo maniacsqRomDesc[] = {
	{ "tms27c010a.msu45",   0x020000, 0xfa44c907, 0 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "tms27c010a.msu44",   0x020000, 0x42e20121, 0 | BRF_PRG | BRF_ESS }, //  1

	{ "ms1",				0x080000, 0xd8551b2f, 1 | BRF_GRA },           //  2 Graphics & Samples
	{ "ms2",				0x080000, 0xb269c427, 1 | BRF_GRA },           //  3
	{ "ms3",				0x020000, 0xaf4ea5e7, 1 | BRF_GRA },           //  4
	{ "ms4",				0x020000, 0x578c3588, 1 | BRF_GRA },           //  5

	{ "maniacsq_ds5002fp_sram.bin",	0x8000, 0xafe9703d, 2 | BRF_PRG | BRF_ESS }, // 6 Dallas MCU
};

STD_ROM_PICK(maniacsq)
STD_ROM_FN(maniacsq)

static INT32 maniacsqInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvManiacsq = {
	"maniacsq", NULL, NULL, NULL, "1996",
	"Maniac Square (protected, version 1.0, checksum DEEE)\0", NULL, "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, maniacsqRomInfo, maniacsqRomName, NULL, NULL, NULL, NULL, ManiacsqInputInfo, ManiacsqDIPInfo,
	maniacsqInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	320, 240, 4, 3
};


// Maniac Square (protected, version 1.0, checksum CF2D)

static struct BurnRomInfo maniacsqaRomDesc[] = {
	{ "ms_u_45.u45",	0x020000, 0x98f4fdc0, 0 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "ms_u_44.u44",	0x020000, 0x1785dd41, 0 | BRF_PRG | BRF_ESS }, //  1

	{ "ms1",			0x080000, 0xd8551b2f, 1 | BRF_GRA },           //  2 Graphics & Samples
	{ "ms2",			0x080000, 0xb269c427, 1 | BRF_GRA },           //  3
	{ "ms3",			0x020000, 0xaf4ea5e7, 1 | BRF_GRA },           //  4
	{ "ms4",			0x020000, 0x578c3588, 1 | BRF_GRA },           //  5

	{ "maniacsq_ds5002fp_sram.bin",	0x8000, 0xafe9703d, 2 | BRF_PRG | BRF_ESS }, // 6 Dallas MCU
};

STD_ROM_PICK(maniacsqa)
STD_ROM_FN(maniacsqa)

struct BurnDriver BurnDrvManiacsqa = {
	"maniacsqa", "maniacsq", NULL, NULL, "1996",
	"Maniac Square (protected, version 1.0, checksum CF2D)\0", NULL, "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, maniacsqaRomInfo, maniacsqaRomName, NULL, NULL, NULL, NULL, ManiacsqInputInfo, ManiacsqDIPInfo,
	maniacsqInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	320, 240, 4, 3
};


// Maniac Square (unprotected, version 1.0, checksum BB73)

static struct BurnRomInfo maniacsquRomDesc[] = {
	{ "d8-d15.1m",		0x020000, 0x9121d1b6, 0 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "d0-d7.1m",		0x020000, 0xa95cfd2a, 0 | BRF_PRG | BRF_ESS }, //  1

	{ "d0-d7.4m",		0x080000, 0xd8551b2f, 1 | BRF_GRA },           //  2 Graphics & Samples
	{ "d8-d15.4m",		0x080000, 0xb269c427, 1 | BRF_GRA },           //  3
	{ "d16-d23.1m",		0x020000, 0xaf4ea5e7, 1 | BRF_GRA },           //  4
	{ "d24-d31.1m",		0x020000, 0x578c3588, 1 | BRF_GRA },           //  5
};

STD_ROM_PICK(maniacsqu)
STD_ROM_FN(maniacsqu)

struct BurnDriver BurnDrvManiacsqu = {
	"maniacsqu", "maniacsq", NULL, NULL, "1996",
	"Maniac Square (unprotected, version 1.0, checksum BB73)\0", NULL, "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, maniacsquRomInfo, maniacsquRomName, NULL, NULL, NULL, NULL, ManiacsqInputInfo, ManiacsqDIPInfo,
	maniacsqInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	320, 240, 4, 3
};


// Alligator Hunt (World, protected)

static struct BurnRomInfo aligatorRomDesc[] = {
	{ "1.u45",  	0x080000, 0x61c47c56, 0 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "2.u44",  	0x080000, 0x96bc77c2, 0 | BRF_PRG | BRF_ESS }, //  1

	{ "u48",		0x400000, 0x19e03bf1, 1 | BRF_GRA },           //  2 Graphics & Samples
	{ "u47",		0x400000, 0x74a5a29f, 1 | BRF_GRA },           //  3
	{ "u50",		0x400000, 0x85daecf9, 1 | BRF_GRA },           //  4
	{ "u49",		0x400000, 0x70a4ee0b, 1 | BRF_GRA },           //  5
	
	{ "aligator_ds5002fp_sram.bin", 0x08000, 0x6558f215, BRF_PRG | BRF_ESS }, //  6 Dallas MCU
};

STD_ROM_PICK(aligator)
STD_ROM_FN(aligator)

static INT32 aligatorInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvAligator = {
	"aligator", NULL, NULL, NULL, "1994",
	"Alligator Hunt (World, protected)\0", NULL, "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, aligatorRomInfo, aligatorRomName, NULL, NULL, NULL, NULL, AlighuntInputInfo, AlighuntDIPInfo,
	aligatorInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	320, 240, 4, 3
};


// Alligator Hunt (Spain, protected)

static struct BurnRomInfo aligatorsRomDesc[] = {
	{ "u45",  		0x080000, 0x61c47c56, 0 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "u44",  		0x080000, 0xf0be007a, 0 | BRF_PRG | BRF_ESS }, //  1

	{ "u48",		0x400000, 0x19e03bf1, 1 | BRF_GRA },           //  2 Graphics & Samples
	{ "u47",		0x400000, 0x74a5a29f, 1 | BRF_GRA },           //  3
	{ "u50",		0x400000, 0x85daecf9, 1 | BRF_GRA },           //  4
	{ "u49",		0x400000, 0x70a4ee0b, 1 | BRF_GRA },           //  5
	
	{ "aligator_ds5002fp_sram.bin", 0x08000, 0x6558f215, BRF_PRG | BRF_ESS }, //  6 Dallas MCU
};

STD_ROM_PICK(aligators)
STD_ROM_FN(aligators)

struct BurnDriver BurnDrvAligators = {
	"aligators", "aligator", NULL, NULL, "1994",
	"Alligator Hunt (Spain, protected)\0", NULL, "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, aligatorsRomInfo, aligatorsRomName, NULL, NULL, NULL, NULL, AlighuntInputInfo, AlighuntDIPInfo,
	aligatorInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	320, 240, 4, 3
};


// Alligator Hunt (unprotected, set 1)

static struct BurnRomInfo aligatorunRomDesc[] = {
	{ "ahntu45n.040",	0x080000, 0xfc02cb2d, 0 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "ahntu44n.040",	0x080000, 0x7fbea3a3, 0 | BRF_PRG | BRF_ESS }, //  1

	{ "u48",		0x400000, 0x19e03bf1, 1 | BRF_GRA },           //  2 Graphics & Samples
	{ "u47",		0x400000, 0x74a5a29f, 1 | BRF_GRA },           //  3
	{ "u50",		0x400000, 0x85daecf9, 1 | BRF_GRA },           //  4
	{ "u49",		0x400000, 0x70a4ee0b, 1 | BRF_GRA },           //  5
};

STD_ROM_PICK(aligatorun)
STD_ROM_FN(aligatorun)

struct BurnDriver BurnDrvAligatorun = {
	"aligatorun", "aligator", NULL, NULL, "1994",
	"Alligator Hunt (unprotected, set 1)\0", NULL, "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, aligatorunRomInfo, aligatorunRomName, NULL, NULL, NULL, NULL, AlighuntInputInfo, AlighuntDIPInfo,
	aligatorInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	320, 240, 4, 3
};


// Alligator Hunt (unprotected, set 2)

static struct BurnRomInfo aligatorunaRomDesc[] = {
	{ "stm27c4001.45",	0x080000, 0xa70301b8, 0 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "am27c040.44",	0x080000, 0xd45a26ed, 0 | BRF_PRG | BRF_ESS }, //  1

	{ "u48",		0x400000, 0x19e03bf1, 1 | BRF_GRA },           //  2 Graphics & Samples
	{ "u47",		0x400000, 0x74a5a29f, 1 | BRF_GRA },           //  3
	{ "u50",		0x400000, 0x85daecf9, 1 | BRF_GRA },           //  4
	{ "u49",		0x400000, 0x70a4ee0b, 1 | BRF_GRA },           //  5
};

STD_ROM_PICK(aligatoruna)
STD_ROM_FN(aligatoruna)

struct BurnDriver BurnDrvAligatoruna = {
	"aligatoruna", "aligator", NULL, NULL, "1994",
	"Alligator Hunt (unprotected, set 2)\0", NULL, "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, aligatorunaRomInfo, aligatorunaRomName, NULL, NULL, NULL, NULL, AlighuntInputInfo, AlighuntDIPInfo,
	aligatorInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	320, 240, 4, 3
};


// Snow Board Championship (version 2.0)

static struct BurnRomInfo snowboaraRomDesc[] = {
	{ "sb_53.ic53",		0x080000, 0xe4eaefd4, 0 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "sb_55.ic55",		0x080000, 0xe2476994, 0 | BRF_PRG | BRF_ESS }, //  1

	{ "sb_ic43.ic43",		0x200000, 0xafce54ed, 1 | BRF_GRA },           //  2 Graphics & Samples
	{ "sb_ic44.ic44",		0x400000, 0x1bbe88bc, 1 | BRF_GRA },           //  3
	{ "sb_ic45.ic45",		0x400000, 0x373983d9, 1 | BRF_GRA },           //  4
	{ "sb_ic46.ic46",		0x400000, 0x22e7c648, 1 | BRF_GRA },           //  5
};

STD_ROM_PICK(snowboara)
STD_ROM_FN(snowboara)

static INT32 snowboaraInit()
{
	return DrvInit(2);
}

struct BurnDriver BurnDrvSnowboara = {
	"snowboara", "snowboar", NULL, NULL, "1996",
	"Snow Board Championship (version 2.0)\0", NULL, "Gaelco / OMK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_RACING, 0,
	NULL, snowboaraRomInfo, snowboaraRomName, NULL, NULL, NULL, NULL, SnowboarInputInfo, NULL,
	snowboaraInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	384, 240, 4, 3
};


// Snow Board Championship (version 2.1)

static struct BurnRomInfo snowboarRomDesc[] = {
	{ "sb.53",		0x080000, 0x4742749e, 0 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "sb.55",		0x080000, 0x6ddc431f, 0 | BRF_PRG | BRF_ESS }, //  1

	{ "sb.a0",		0x080000, 0xaa476e44, 1 | BRF_GRA },           //  2 Graphics & Samples
	{ "sb.a1",		0x080000, 0x6bc99195, 1 | BRF_GRA },           //  3
	{ "sb.a2",		0x080000, 0xfae2ebba, 1 | BRF_GRA },           //  4
	{ "sb.a3",		0x080000, 0x17ed9cf8, 1 | BRF_GRA },           //  5
	{ "sb.a4",		0x080000, 0x2ba3a5c8, 1 | BRF_GRA },           //  6
	{ "sb.a5",		0x080000, 0xae011eb3, 1 | BRF_GRA },           //  7
	{ "sb.b0",		0x080000, 0x96c714cd, 1 | BRF_GRA },           //  8
	{ "sb.b1",		0x080000, 0x39a4c30c, 1 | BRF_GRA },           //  9
	{ "sb.b2",		0x080000, 0xb58fcdd6, 1 | BRF_GRA },           // 10
	{ "sb.b3",		0x080000, 0x96afdebf, 1 | BRF_GRA },           // 11
	{ "sb.b4",		0x080000, 0xe62cf8df, 1 | BRF_GRA },           // 12
	{ "sb.b5",		0x080000, 0xcaa90856, 1 | BRF_GRA },           // 13
	{ "sb.c0",		0x080000, 0xc9d57a71, 1 | BRF_GRA },           // 14
	{ "sb.c1",		0x080000, 0x1d14a3d4, 1 | BRF_GRA },           // 15
	{ "sb.c2",		0x080000, 0x55026352, 1 | BRF_GRA },           // 16
	{ "sb.c3",		0x080000, 0xd9b62dee, 1 | BRF_GRA },           // 17
	{ "sb.d0",		0x080000, 0x7434c1ae, 1 | BRF_GRA },           // 18
	{ "sb.d1",		0x080000, 0xf00cc6c8, 1 | BRF_GRA },           // 19
	{ "sb.d2",		0x080000, 0x019f9aec, 1 | BRF_GRA },           // 20
	{ "sb.d3",		0x080000, 0xd05bd286, 1 | BRF_GRA },           // 21
	{ "sb.e0",		0x080000, 0xe6195323, 1 | BRF_GRA },           // 22
	{ "sb.e1",		0x080000, 0x9f38910b, 1 | BRF_GRA },           // 23
	{ "sb.e2",		0x080000, 0xf5948c6c, 1 | BRF_GRA },           // 24
	{ "sb.e3",		0x080000, 0x4baa678f, 1 | BRF_GRA },           // 25
};

STD_ROM_PICK(snowboar)
STD_ROM_FN(snowboar)

static INT32 snowboarInit()
{
	return DrvInit(3);
}

struct BurnDriver BurnDrvSnowboar = {
	"snowboar", NULL, NULL, NULL, "1997",
	"Snow Board Championship (version 2.1)\0", NULL, "Gaelco / OMK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_RACING, 0,
	NULL, snowboarRomInfo, snowboarRomName, NULL, NULL, NULL, NULL, SnowboarInputInfo, NULL,
	snowboarInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	384, 240, 4, 3
};


// Touch and Go (World, 05/Feb/1996, checksum 059D0235)
// REF: 950510-1

static struct BurnRomInfo touchgoRomDesc[] = {
	{ "tg_873d_56_5-2.ic56",		0x080000, 0x6d0f5c65, 1 | BRF_PRG | BRF_ESS }, //  0  68k Code
	{ "tg_cee8_57_5-2.ic57",		0x080000, 0x845787b5, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "tg_ic69.ic69",				0x200000, 0x18bb12d4, 2 | BRF_GRA },           //  2 Graphics & Samples
	{ "tg_ic65.ic65",				0x400000, 0x91b89c7c, 2 | BRF_GRA },           //  3
	{ "tg_ic66.ic66",				0x200000, 0x52682953, 2 | BRF_GRA },           //  4
	{ "tg_ic67.ic67",				0x400000, 0xc0a2ce5b, 2 | BRF_GRA },           //  5

	{ "touchgo_ds5002fp_sram.bin",	0x8000, 0x6a238adb, 4 | BRF_PRG | BRF_ESS },   //  6 Dallas MCU
	{ "touchgo_scratch",		    0x0080, 0xf9ca54ff, 4 | BRF_PRG | BRF_ESS },   //  7 Dallas MCU internal RAM
};

STD_ROM_PICK(touchgo)
STD_ROM_FN(touchgo)

static INT32 touchgoInit()
{
	return DrvInit(4);
}

struct BurnDriver BurnDrvTouchgo = {
	"touchgo", NULL, NULL, NULL, "1995",
	"Touch and Go (World, 05/Feb/1996, checksum 059D0235)\0", NULL, "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, touchgoRomInfo, touchgoRomName, NULL, NULL, NULL, NULL, TouchgoInputInfo, TouchgoDIPInfo,
	touchgoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	480, 240, 4, 3
};


// Touch and Go (World, 11/Dec/1995, checksum 05A0C7FB)
// REF: 950906

static struct BurnRomInfo touchgoaRomDesc[] = {
	{ "tg_be51_5.6_11-12.ic56",		0x080000, 0x8ab065f3, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "tg_6701_5.7_11-12.ic57",		0x080000, 0x0dfd3f65, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "tg_ic69.ic69",				0x200000, 0x18bb12d4, 2 | BRF_GRA },           //  2 Graphics & Samples
	{ "tg_ic65.ic65",				0x400000, 0x91b89c7c, 2 | BRF_GRA },           //  3
	{ "tg_ic66.ic66",				0x200000, 0x52682953, 2 | BRF_GRA },           //  4
	{ "tg_ic67.ic67",				0x400000, 0xc0a2ce5b, 2 | BRF_GRA },           //  5

	{ "touchgo_ds5002fp_sram.bin", 	0x8000, 0x6a238adb, 4 | BRF_PRG | BRF_ESS },   //  6 Dallas MCU
	{ "touchgo_scratch",		   	0x0080, 0xf9ca54ff, 4 | BRF_PRG | BRF_ESS },   //  7 Dallas MCU internal RAM
};

STD_ROM_PICK(touchgoa)
STD_ROM_FN(touchgoa)

struct BurnDriver BurnDrvTouchgoa = {
	"touchgoa", "touchgo", NULL, NULL, "1995",
	"Touch and Go (World, 11/Dec/1995, checksum 05A0C7FB)\0", NULL, "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, touchgoaRomInfo, touchgoaRomName, NULL, NULL, NULL, NULL, TouchgoInputInfo, TouchgoDIPInfo,
	touchgoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	480, 240, 4, 3
};


// Touch and Go (North America, 14/Nov/1995, checksum 05737572)

static struct BurnRomInfo touchgonaRomDesc[] = {
	{ "v_us_56_f546_14-11.ic56",	0x080000, 0x3bfe2010, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "v_us_57_d888_14-11.ic57",	0x080000, 0xc8a9e7bd, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "tg_ic69.ic69",				0x200000, 0x18bb12d4, 2 | BRF_GRA },           //  2 Graphics & Samples
	{ "tg_ic65.ic65",				0x400000, 0x91b89c7c, 2 | BRF_GRA },           //  3
	{ "tg_ic66.ic66",				0x200000, 0x52682953, 2 | BRF_GRA },           //  4
	{ "tg_ic67.ic67",				0x400000, 0xc0a2ce5b, 2 | BRF_GRA },           //  5

	{ "touchgo_ds5002fp_sram.bin", 	0x8000, 0x6a238adb, 4 | BRF_PRG | BRF_ESS },   //  6 Dallas MCU
	{ "touchgo_scratch",		   	0x0080, 0xf9ca54ff, 4 | BRF_PRG | BRF_ESS },   //  7 Dallas MCU internal RAM
};

STD_ROM_PICK(touchgona)
STD_ROM_FN(touchgona)

static INT32 touchgonaInit() {
	INT32 rc = touchgoInit();

	if (!rc) {
		Drv68KROM[1^1] = 0xfe;
		SekReset(0); // need to latch SP in SekReset()
	}

	return rc;
}

struct BurnDriver BurnDrvTouchgona = {
	"touchgona", "touchgo", NULL, NULL, "1995",
	"Touch and Go (North America, 14/Nov/1995, checksum 05737572)\0", NULL, "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, touchgonaRomInfo, touchgonaRomName, NULL, NULL, NULL, NULL, TouchgoInputInfo, TouchgoDIPInfo,
	touchgonaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	480, 240, 4, 3
};


// Touch and Go (non North America, 16/Nov/1995, checksum 056533F0)

static struct BurnRomInfo touchgonnaRomDesc[] = {
	{ "v_e_56_ef33_16-11.ic56",		0x080000, 0xc2715874, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "v_o_57_9c10_16-11.ic57",		0x080000, 0x3acefb06, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "tg_ic69.ic69",				0x200000, 0x18bb12d4, 2 | BRF_GRA },           //  2 Graphics & Samples
	{ "tg_ic65.ic65",				0x400000, 0x91b89c7c, 2 | BRF_GRA },           //  3
	{ "tg_ic66.ic66",				0x200000, 0x52682953, 2 | BRF_GRA },           //  4
	{ "tg_ic67.ic67",				0x400000, 0xc0a2ce5b, 2 | BRF_GRA },           //  5

	{ "touchgo_ds5002fp_sram.bin", 	0x8000, 0x6a238adb, 4 | BRF_PRG | BRF_ESS },   //  6 Dallas MCU
	{ "touchgo_scratch",		   	0x0080, 0xf9ca54ff, 4 | BRF_PRG | BRF_ESS },   //  7 Dallas MCU internal RAM
};

STD_ROM_PICK(touchgonna)
STD_ROM_FN(touchgonna)

struct BurnDriver BurnDrvTouchgonna = {
	"touchgonna", "touchgo", NULL, NULL, "1995",
	"Touch and Go (non North America, 16/Nov/1995, checksum 056533F0)\0", NULL, "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, touchgonnaRomInfo, touchgonnaRomName, NULL, NULL, NULL, NULL, TouchgoInputInfo, TouchgoDIPInfo,
	touchgoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	480, 240, 4, 3
};


// Touch and Go (non North America, 15/Nov/1995, checksum 056C2336)

static struct BurnRomInfo touchgonnaaRomDesc[] = {
	{ "v_e_c79b_56_15-11.ic56",		0x080000, 0xd92bb02a, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "v_e_b34a_57_15-11.ic57",		0x080000, 0x0711c8ba, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "tg_ic69.ic69",				0x200000, 0x18bb12d4, 2 | BRF_GRA },           //  2 Graphics & Samples
	{ "tg_ic65.ic65",				0x400000, 0x91b89c7c, 2 | BRF_GRA },           //  3
	{ "tg_ic66.ic66",				0x200000, 0x52682953, 2 | BRF_GRA },           //  4
	{ "tg_ic67.ic67",				0x400000, 0xc0a2ce5b, 2 | BRF_GRA },           //  5

	{ "touchgo_ds5002fp_sram.bin", 	0x8000, 0x6a238adb, 4 | BRF_PRG | BRF_ESS },   //  6 Dallas MCU
	{ "touchgo_scratch",		   	0x0080, 0xf9ca54ff, 4 | BRF_PRG | BRF_ESS },   //  7 Dallas MCU internal RAM
};

STD_ROM_PICK(touchgonnaa)
STD_ROM_FN(touchgonnaa)

struct BurnDriver BurnDrvTouchgonnaa = {
	"touchgonnaa", "touchgo", NULL, NULL, "1995",
	"Touch and Go (non North America, 15/Nov/1995, checksum 056C2336)\0", NULL, "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, touchgonnaaRomInfo, touchgonnaaRomName, NULL, NULL, NULL, NULL, TouchgoInputInfo, TouchgoDIPInfo,
	touchgoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	480, 240, 4, 3
};


// Touch and Go (non North America, 15/Nov/1995, checksum 056C138F)

static struct BurnRomInfo touchgonnabRomDesc[] = {
	{ "v_alt_e_56_15-11.ic56",		0x080000, 0x64b83556, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "v_alt_o_57_15-11.ic57",		0x080000, 0xe9ceb77a, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "tg_ic69.ic69",				0x200000, 0x18bb12d4, 2 | BRF_GRA },           //  2 Graphics & Samples
	{ "tg_ic65.ic65",				0x400000, 0x91b89c7c, 2 | BRF_GRA },           //  3
	{ "tg_ic66.ic66",				0x200000, 0x52682953, 2 | BRF_GRA },           //  4
	{ "tg_ic67.ic67",				0x400000, 0xc0a2ce5b, 2 | BRF_GRA },           //  5

	{ "touchgo_ds5002fp_sram.bin", 	0x8000, 0x6a238adb, 4 | BRF_PRG | BRF_ESS },   //  6 Dallas MCU
	{ "touchgo_scratch",		   	0x0080, 0xf9ca54ff, 4 | BRF_PRG | BRF_ESS },   //  7 Dallas MCU internal RAM
};

STD_ROM_PICK(touchgonnab)
STD_ROM_FN(touchgonnab)

struct BurnDriver BurnDrvTouchgonnab = {
	"touchgonnab", "touchgo", NULL, NULL, "1995",
	"Touch and Go (non North America, 15/Nov/1995, checksum 056C138F)\0", NULL, "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, touchgonnabRomInfo, touchgonnabRomName, NULL, NULL, NULL, NULL, TouchgoInputInfo, TouchgoDIPInfo,
	touchgoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	480, 240, 4, 3
};


// Touch and Go (non North America, 11/Nov/2005, checksum 056AA304)
// REF 950906, no plug-in daughterboard, non North America notice, also found on REF: 950510-1 with daughterboard

static struct BurnRomInfo touchgonnacRomDesc[] = {
	{ "1.ic63",						0x080000, 0xfd3b4642, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "2.ic64",						0x080000, 0xee891835, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "tg_ic69.ic69",				0x200000, 0x18bb12d4, 2 | BRF_GRA },           //  2 Graphics & Samples
	{ "tg_ic65.ic65",				0x400000, 0x91b89c7c, 2 | BRF_GRA },           //  3
	{ "tg_ic66.ic66",				0x200000, 0x52682953, 2 | BRF_GRA },           //  4
	{ "tg_ic67.ic67",				0x400000, 0xc0a2ce5b, 2 | BRF_GRA },           //  5

	{ "touchgo_ds5002fp_sram.bin", 	0x8000, 0x6a238adb, 4 | BRF_PRG | BRF_ESS },   //  6 Dallas MCU
	{ "touchgo_scratch",		   	0x0080, 0xf9ca54ff, 4 | BRF_PRG | BRF_ESS },   //  7 Dallas MCU internal RAM
};

STD_ROM_PICK(touchgonnac)
STD_ROM_FN(touchgonnac)

struct BurnDriver BurnDrvTouchgonnac = {
	"touchgonnac", "touchgo", NULL, NULL, "1995",
	"Touch and Go (non North America, 11/Nov/2005, checksum 056AA304)\0", NULL, "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, touchgonnacRomInfo, touchgonnacRomName, NULL, NULL, NULL, NULL, TouchgoInputInfo, TouchgoDIPInfo,
	touchgoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	480, 240, 4, 3
};


// Touch and Go (unprotected, checksum 059CC336)
// REF: 950510-1 - ds5002fp unpopulated, game is unprotected

static struct BurnRomInfo touchgounRomDesc[] = {
	{ "tg_d_56_1e4b_no_dallas.ic56",	0x080000, 0xcbb87505, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "tg_d_57_060d_no_dallas.ic57",	0x080000, 0x36bcc7e7, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "tg_ic69.ic69",					0x200000, 0x18bb12d4, 2 | BRF_GRA },           //  2 Graphics & Samples
	{ "tg_ic65.ic65",					0x400000, 0x91b89c7c, 2 | BRF_GRA },           //  3
	{ "tg_ic66.ic66",					0x200000, 0x52682953, 2 | BRF_GRA },           //  4
	{ "tg_ic67.ic67",					0x400000, 0xc0a2ce5b, 2 | BRF_GRA },           //  5
};

STD_ROM_PICK(touchgoun)
STD_ROM_FN(touchgoun)

struct BurnDriver BurnDrvTouchgoun = {
	"touchgoun", "touchgo", NULL, NULL, "1995",
	"Touch and Go (unprotected, checksum 059CC336)\0", NULL, "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, touchgounRomInfo, touchgounRomName, NULL, NULL, NULL, NULL, TouchgoInputInfo, TouchgoDIPInfo,
	touchgoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	480, 240, 4, 3
};


// Bang!

static struct BurnRomInfo bangRomDesc[] = {
	{ "bang53.ic53",	0x80000, 0x014bb939, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "bang55.ic55",	0x80000, 0x582f8b1e, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "bang16.ic16",	0x80000, 0x6ee4b878, 2 | BRF_GRA },           //  2 Graphics & Samples
	{ "bang17.ic17",	0x80000, 0x0c35aa6f, 2 | BRF_GRA },           //  3
	{ "bang18.ic18",	0x80000, 0x2056b1ad, 2 | BRF_GRA },           //  4
	{ "bang9.ic9",		0x80000, 0x078195dc, 2 | BRF_GRA },           //  5
	{ "bang10.ic10",	0x80000, 0x06711eeb, 2 | BRF_GRA },           //  6
	{ "bang11.ic11",	0x80000, 0x2088d15c, 2 | BRF_GRA },           //  7
	{ "bang1.ic1",		0x80000, 0xe7b97b0f, 2 | BRF_GRA },           //  8
	{ "bang2.ic2",		0x80000, 0xff297a8f, 2 | BRF_GRA },           //  9
	{ "bang3.ic3",		0x80000, 0xd3da5d4f, 2 | BRF_GRA },           // 10
	{ "bang20.ic20",	0x80000, 0xa1145df8, 2 | BRF_GRA },           // 11
	{ "bang13.ic13",	0x80000, 0xfe3e8d07, 2 | BRF_GRA },           // 12
	{ "bang5.ic5",		0x80000, 0x9bee444c, 2 | BRF_GRA },           // 13
	{ "bang21.ic21",	0x80000, 0xfd93d7f2, 2 | BRF_GRA },           // 14
	{ "bang14.ic14",	0x80000, 0x858fcbf9, 2 | BRF_GRA },           // 15
	
	{ "bang_gal16v8.ic56", 0x00117, 0x226923ac, 3 | BRF_OPT },		  // 16 plds
};

STD_ROM_PICK(bang)
STD_ROM_FN(bang)

static INT32 bangInit()
{
	INT32 rc = DrvInit(6);

	if (!rc) {
		BurnGunInit(2, true);
	}

	return rc;
}

struct BurnDriver BurnDrvBang = {
	"bang", NULL, NULL, NULL, "1998",
	"Bang!\0", NULL, "Gaelco / Bit Managers", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, bangRomInfo, bangRomName, NULL, NULL, NULL, NULL, BangInputInfo, NULL,
	bangInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	320, 240, 4, 3
};


// Gun Gabacho (Japan)

static struct BurnRomInfo bangjRomDesc[] = {
	{ "bang-a.ic53",	0x80000, 0x5ee514e9, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "bang-a.ic55",	0x80000, 0xb90223ab, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "bang-a.ic16",	0x80000, 0x3b63acfc, 2 | BRF_GRA },           //  2 Graphics & Samples
	{ "bang-a.ic17",	0x80000, 0x72865b80, 2 | BRF_GRA },           //  3
	{ "bang18.ic18",	0x80000, 0x2056b1ad, 2 | BRF_GRA },           //  4
	{ "bang-a.ic9",		0x80000, 0x3cb86360, 2 | BRF_GRA },           //  5
	{ "bang-a.ic10",	0x80000, 0x03fdd777, 2 | BRF_GRA },           //  6
	{ "bang11.ic11",	0x80000, 0x2088d15c, 2 | BRF_GRA },           //  7
	{ "bang-a.ic1",		0x80000, 0x965d0ad9, 2 | BRF_GRA },           //  8
	{ "bang-a.ic2",		0x80000, 0x8ea261a7, 2 | BRF_GRA },           //  9
	{ "bang3.ic3",		0x80000, 0xd3da5d4f, 2 | BRF_GRA },           // 10
	{ "bang-a.ic20",	0x80000, 0x4b828f3c, 2 | BRF_GRA },           // 11
	{ "bang-a.ic13",	0x80000, 0xd1146b92, 2 | BRF_GRA },           // 12
	{ "bang5.ic5",		0x80000, 0x9bee444c, 2 | BRF_GRA },           // 13
	{ "bang-a.ic21",	0x80000, 0x531ce3b6, 2 | BRF_GRA },           // 14
	{ "bang-a.ic14",	0x80000, 0xf8e1cf84, 2 | BRF_GRA },           // 15
	
	{ "bang_gal16v8.ic56", 0x00117, 0x226923ac, 3 | BRF_OPT },		  // 16 plds
};

STD_ROM_PICK(bangj)
STD_ROM_FN(bangj)

struct BurnDriver BurnDrvBangj = {
	"bangj", "bang", NULL, NULL, "1999",
	"Gun Gabacho (Japan)\0", NULL, "Gaelco / Bit Managers (GM Shoji license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, bangjRomInfo, bangjRomName, NULL, NULL, NULL, NULL, BangInputInfo, NULL,
	bangInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	320, 240, 4, 3
};


// World Rally 2: Twin Racing (mask ROM version)

static struct BurnRomInfo wrally2RomDesc[] = {
	{ "wr2_64.ic64",		0x80000, 0x4cdf4e1e, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "wr2_63.ic63",		0x80000, 0x94887c9f, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "wrally2_ds5002fp_sram.bin",	0x08000, 0x4c532e9e, 2 | BRF_PRG | BRF_ESS }, //  2 DS5002FP MCU
	
	{ "wr2_ic68.ic68",  0x0100000, 0x4a75ffaa, 3 | BRF_OPT },
	{ "wr2_ic69.ic69",  0x0400000, 0xa174d196, 3 | BRF_OPT },
	{ "wr2_ic70.ic70",  0x0200000, 0x8d1e43ba, 3 | BRF_OPT },
};

STD_ROM_PICK(wrally2)
STD_ROM_FN(wrally2)

static INT32 wrally2Init()
{
	return DrvInit(8);
}

struct BurnDriver BurnDrvWrally2 = {
	"wrally2", NULL, NULL, NULL, "1995",
	"World Rally 2: Twin Racing (mask ROM version)\0", NULL, "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_RACING, 0,
	NULL, wrally2RomInfo, wrally2RomName, NULL, NULL, NULL, NULL, Wrally2InputInfo, Wrally2DIPInfo,
	wrally2Init, DrvExit, DrvFrame, DualDraw, DrvScan, &DrvRecalc, 0x10000,
	384*2, 240, 8, 3
};


// World Rally 2: Twin Racing (EPROM version)

static struct BurnRomInfo wrally2aRomDesc[] = {
	{ "wr2_64.ic64",		0x80000, 0x4cdf4e1e, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "wr2_63.ic63",		0x80000, 0x94887c9f, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "wrally2_ds5002fp_sram.bin",	0x08000, 0x4c532e9e, 2 | BRF_PRG | BRF_ESS }, //  2 DS5002FP MCU

	{ "wr2.16d",		0x80000, 0xad26086b, 3 | BRF_GRA },           //  3 Graphics & Samples
	{ "wr2.17d",		0x80000, 0xc1ec0745, 3 | BRF_GRA },           //  4
	{ "wr2.18d",		0x80000, 0xe3617814, 3 | BRF_GRA },           //  5
	{ "wr2.19d",		0x80000, 0x2dae988c, 3 | BRF_GRA },           //  6
	{ "wr2.09d",		0x80000, 0x372d70c8, 3 | BRF_GRA },           //  7
	{ "wr2.10d",		0x80000, 0x5db67eb3, 3 | BRF_GRA },           //  8
	{ "wr2.11d",		0x80000, 0xae66b97c, 3 | BRF_GRA },           //  9
	{ "wr2.12d",		0x80000, 0x6dbdaa95, 3 | BRF_GRA },           // 10
	{ "wr2.01d",		0x80000, 0x753a138d, 3 | BRF_GRA },           // 11
	{ "wr2.02d",		0x80000, 0x9c2a723c, 3 | BRF_GRA },           // 12
	{ "wr2.20d",		0x80000, 0x4f7ade84, 3 | BRF_GRA },           // 13
	{ "wr2.13d",		0x80000, 0xa4cd32f8, 3 | BRF_GRA },           // 14
	{ "wr2.21d",		0x80000, 0x899b0583, 3 | BRF_GRA },           // 15
	{ "wr2.14d",		0x80000, 0x6eb781d5, 3 | BRF_GRA },           // 16
};

STD_ROM_PICK(wrally2a)
STD_ROM_FN(wrally2a)

static INT32 wrally2aInit()
{
	return DrvInit(7);
}

struct BurnDriver BurnDrvWrally2a = {
	"wrally2a", "wrally2", NULL, NULL, "1995",
	"World Rally 2: Twin Racing (EPROM version)\0", NULL, "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_RACING, 0,
	NULL, wrally2aRomInfo, wrally2aRomName, NULL, NULL, NULL, NULL, Wrally2InputInfo, Wrally2DIPInfo,
	wrally2aInit, DrvExit, DrvFrame, DualDraw, DrvScan, &DrvRecalc, 0x10000,
	384*2, 240, 8, 3
};
