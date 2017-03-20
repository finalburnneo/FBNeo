// FB Alpha Gaelco system 2 driver module
// Based on MAME driver by Manuel Abadia

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "eeprom.h"
#include "gaelco.h"
#include "burn_gun.h"

// Todo/tofix:
//	EEPROM save doesn't seem to work in Snowboard

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprBuf;
static UINT8 *DrvPalRAM;
static UINT8 *Drv68KRAM;
static UINT8 *Drv68KRAM2;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT16 *DrvVidRegs;

static INT32 gun_interrupt;
static UINT32 snowboar_latch;

static INT32 game_select = 0;
static void (*pIRQCallback)(INT32 line);
static UINT8 m_dual_monitor = 0;
static UINT32 gfxmask = ~0;
static INT32 nCPUClockSpeed = 0;
static INT32 global_y_offset = -16;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvJoy4[16];
static UINT8 DrvDips[3];
static UINT8 DrvReset;
static UINT16 DrvInputs[4];

// Lightgun stuff
static INT32 LethalGun0 = 0;
static INT32 LethalGun1 = 0;
static INT32 LethalGun2 = 0;
static INT32 LethalGun3 = 0;

static struct BurnInputInfo AlighuntInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},

	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},

	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"	},
	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"service"	},

	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Alighunt)

static struct BurnInputInfo SnowboarInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy3 + 2,	"service"},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 3,	"service"},
//	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
};

STDINPUTINFO(Snowboar)

static struct BurnInputInfo ManiacsqInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},

	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},

	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"service"	},

	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Maniacsq)

static struct BurnInputInfo TouchgoInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 8,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 9,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"	},

	{"P3 Coin",		BIT_DIGITAL,	DrvJoy3 + 10,	"p3 coin"	},
	{"P3 Start",		BIT_DIGITAL,	DrvJoy3 + 7,	"p3 start"	},
	{"P3 Up",		BIT_DIGITAL,	DrvJoy3 + 0,	"p3 up"		},
	{"P3 Down",		BIT_DIGITAL,	DrvJoy3 + 1,	"p3 down"	},
	{"P3 Left",		BIT_DIGITAL,	DrvJoy3 + 3,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy3 + 2,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p3 fire 2"	},
	{"P3 Button 3",		BIT_DIGITAL,	DrvJoy3 + 6,	"p3 fire 3"	},

	{"P4 Coin",		BIT_DIGITAL,	DrvJoy3 + 11,	"p4 coin"	},
	{"P4 Start",		BIT_DIGITAL,	DrvJoy4 + 7,	"p4 start"	},
	{"P4 Up",		BIT_DIGITAL,	DrvJoy4 + 0,	"p4 up"		},
	{"P4 Down",		BIT_DIGITAL,	DrvJoy4 + 1,	"p4 down"	},
	{"P4 Left",		BIT_DIGITAL,	DrvJoy4 + 3,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy4 + 2,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy4 + 4,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy4 + 5,	"p4 fire 2"	},
	{"P4 Button 3",		BIT_DIGITAL,	DrvJoy4 + 6,	"p4 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy4 + 8,	"service"	},
	{"Service1",		BIT_DIGITAL,	DrvJoy4 + 10,	"service"	},
	{"Service2",		BIT_DIGITAL,	DrvJoy4 + 9,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Touchgo)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo BangInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 start"	},
	{"P1 Button",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 1"	},
	A("P1 Gun X",    	BIT_ANALOG_REL, &LethalGun0,    "mouse x-axis"	),
	A("P1 Gun Y",    	BIT_ANALOG_REL, &LethalGun1,    "mouse y-axis"	),

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 start"	},
	{"P2 Button",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 1"	},
	A("P2 Gun X",    	BIT_ANALOG_REL, &LethalGun2,    "p2 x-axis"	),
	A("P2 Gun Y",    	BIT_ANALOG_REL, &LethalGun3,    "p2 y-axis"	),

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Bang)
#undef A

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

		case 0x300008:
		case 0x300009:
			EEPROMWriteBit(data & 1);
		return;

		case 0x30000a:
		case 0x30000b:
			EEPROMSetClockLine(data & 0x01);
		return;

		case 0x30000c:
		case 0x30000d:
			EEPROMSetCSLine(data & 0x01);
		return;

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
			EEPROMSetCSLine(data & 0x01);
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

	INT32 analog_value = 0x80; // placeholder until analog inputs hooked up!

	const INT32 adjust[6] = { 320, 240, 1, -4, 0, 0};

	switch (port & 3)
	{
		case 0: analog_value = BurnGunReturnX(0);// input 0, x
		        break;
		case 1: analog_value = BurnGunReturnX(1);// input 1, x
		        break;
		case 2: analog_value = BurnGunReturnY(0);// input 0, y
		        break;
		case 3: analog_value = BurnGunReturnY(1);// input 1, y
		        break;
	}

	return ((analog_value * adjust[port]) / 256) + adjust[(port)+2];
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
	*((UINT16*)(DrvSprRAM + (address & 0xfffe))) = data;

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
	return *((UINT16*)(DrvSprRAM + (address & 0xfffe)));
}

static void palette_update(INT32 offset)
{
	static const int pen_color_adjust[16] = {
		0, -8, -16, -24, -32, -40, -48, -56, 64, 56, 48, 40, 32, 24, 16, 8
	};

	offset = (offset & 0x1ffe);

	UINT16 color = *((UINT16*)(DrvPalRAM + offset));

	INT32 r = (color >> 10) & 0x1f;
	INT32 g = (color >>  5) & 0x1f;
	INT32 b = (color >>  0) & 0x1f;

	r = (r << 3) | (r >> 2);
	g = (g << 3) | (g >> 2);
	b = (b << 3) | (b >> 2);

	DrvPalette[offset/2] = BurnHighCol(r,g,b,0);

	if (offset >= 0x211fe0) return;

#define ADJUST_COLOR(c) ((c < 0) ? 0 : ((c > 255) ? 255 : c))

	for (INT32 i = 1; i < 16; i++) {
		INT32 auxr = ADJUST_COLOR(r + pen_color_adjust[i]);
		INT32 auxg = ADJUST_COLOR(g + pen_color_adjust[i]);
		INT32 auxb = ADJUST_COLOR(b + pen_color_adjust[i]);

		DrvPalette[0x1000 + (offset/2) * i] = BurnHighCol(auxr,auxg,auxb,0);
	}
}

static void __fastcall gaelco2_palette_write_byte(UINT32 address, UINT8 data)
{
	DrvPalRAM[(address & 0x1fff) ^ 1] = data;
	palette_update(address);
}

static void __fastcall gaelco2_palette_write_word(UINT32 address, UINT16 data)
{
	*((UINT16*)(DrvPalRAM + (address & 0x1ffe))) = data;
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

	Drv68KROM	= Next; Next += 0x0100000;

	DrvGfxROM0	= Next; Next += 0x1400000;
	DrvGfxROM	= Next; Next += 0x2000000;

	DrvPalette	= (UINT32*)Next; Next += 0x10000 * sizeof(UINT32);

	AllRam		= Next;

	DrvSprRAM	= Next; Next += 0x0010000;
	DrvSprBuf	= Next; Next += 0x0010000;
	DrvPalRAM	= Next; Next += 0x0002000;
	Drv68KRAM	= Next; Next += 0x0020000;
	Drv68KRAM2	= Next; Next += 0x0002000;

	DrvVidRegs	= (UINT16*)Next; Next += 0x00003 * sizeof(UINT16);

	RamEnd		= Next;

	MemEnd		= Next;

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


static const eeprom_interface gaelco2_eeprom_interface =
{
	8,				/* address bits */
	16,				/* data bits */
	"*110",			/* read command */
	"*101",			/* write command */
	"*111",			/* erase command */
	"*10000xxxxxx",	/* lock command */
	"*10011xxxxxx", /* unlock command */
	0,//  "*10001xxxxxx", /* write all */
	0//  "*10010xxxxxx", /* erase all */
};

static INT32 DrvInit(INT32 game_selector)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	game_select = game_selector;

	switch (game_select)
	{
		case 0:	// aligatorun
		{
			if (BurnLoadRom(Drv68KROM  + 0x000001, 0, 2)) return 1;
			if (BurnLoadRom(Drv68KROM  + 0x000000, 1, 2)) return 1;

			if (BurnLoadRom(DrvGfxROM + 0x000000, 2, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x400000, 3, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x800000, 4, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0xc00000, 5, 1)) return 1;

			memset (DrvGfxROM0, 0, 0x1400000);

			gaelco2_split_gfx(DrvGfxROM, DrvGfxROM0, 0x0000000, 0x0400000, 0x0000000, 0x0400000);
			gaelco2_split_gfx(DrvGfxROM, DrvGfxROM0, 0x0400000, 0x0400000, 0x0200000, 0x0600000);
			gaelco2_split_gfx(DrvGfxROM, DrvGfxROM0, 0x0800000, 0x0400000, 0x0800000, 0x0c00000);
			gaelco2_split_gfx(DrvGfxROM, DrvGfxROM0, 0x0c00000, 0x0400000, 0x0a00000, 0x0e00000);

			memset (DrvGfxROM, 0, 0x2000000);

			DrvGfxDecode(0x1400000);

			nCPUClockSpeed = 13000000;
			pIRQCallback = pIRQLine6Callback;

			gaelcosnd_start(DrvGfxROM0, 0 * 0x0400000, 1 * 0x0400000, 2 * 0x0400000, 3 * 0x0400000);
		}
		break;

		case 1:	// maniacsq
		{
			if (BurnLoadRom(Drv68KROM  + 0x000001, 0, 2)) return 1;
			if (BurnLoadRom(Drv68KROM  + 0x000000, 1, 2)) return 1;

			if (BurnLoadRom(DrvGfxROM0 + 0x000000, 2, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x080000, 3, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x100000, 4, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x180000, 5, 1)) return 1;

			DrvGfxDecode(0x280000);

			nCPUClockSpeed = 13000000;
			pIRQCallback = pIRQLine6Callback;

			gaelcosnd_start(DrvGfxROM0, 0 * 0x0080000, 1 * 0x0080000, 0, 0);
		}
		break;

		case 2:	// snowboara
		{
			if (BurnLoadRom(Drv68KROM  + 0x000001, 0, 2)) return 1;
			if (BurnLoadRom(Drv68KROM  + 0x000000, 1, 2)) return 1;

			if (BurnLoadRom(DrvGfxROM + 0x000000, 3, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x400000, 4, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x800000, 5, 1)) return 1;

			gaelco2_split_gfx(DrvGfxROM, DrvGfxROM0, 0x0000000, 0x0400000, 0x0000000, 0x0400000);
			gaelco2_split_gfx(DrvGfxROM, DrvGfxROM0, 0x0400000, 0x0400000, 0x0200000, 0x0600000);
			gaelco2_split_gfx(DrvGfxROM, DrvGfxROM0, 0x0800000, 0x0400000, 0x0800000, 0x0c00000);

			if (BurnLoadRom(DrvGfxROM0 + 0x1000000, 2, 1)) return 1;

			DrvGfxDecode(0x1400000);

			nCPUClockSpeed = 15000000;
			pIRQCallback = pIRQLine6Callback;

			gaelcosnd_start(DrvGfxROM0, 0 * 0x0400000, 1 * 0x0400000, 0, 0);
		}
		break;

		case 3:	// snowboar
		{
			if (BurnLoadRom(Drv68KROM  + 0x000001, 0, 2)) return 1;
			if (BurnLoadRom(Drv68KROM  + 0x000000, 1, 2)) return 1;

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

			// no split needed for this set.

			DrvGfxDecode(0x1400000);

			nCPUClockSpeed = 15000000;
			pIRQCallback = pIRQLine6Callback;

			gaelcosnd_start(DrvGfxROM0, 0 * 0x0400000, 1 * 0x0400000, 0, 0);
		}
		break;

		case 4: // touchgo
		{
			if (BurnLoadRom(Drv68KROM  + 0x0000001, 0, 2)) return 1;
			if (BurnLoadRom(Drv68KROM  + 0x0000000, 1, 2)) return 1;

			if (BurnLoadRom(DrvGfxROM0 + 0x1000000, 2, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM  + 0x0000000, 3, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM  + 0x0400000, 4, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM  + 0x0800000, 5, 1)) return 1;

			gaelco2_split_gfx(DrvGfxROM, DrvGfxROM0, 0x0000000, 0x0400000, 0x0000000, 0x0400000);
			gaelco2_split_gfx(DrvGfxROM, DrvGfxROM0, 0x0400000, 0x0200000, 0x0200000, 0x0600000);
			gaelco2_split_gfx(DrvGfxROM, DrvGfxROM0, 0x0800000, 0x0400000, 0x0800000, 0x0c00000);

			DrvGfxDecode(0x1400000);

			nCPUClockSpeed = 16000000;
			pIRQCallback = pIRQLine6Callback;

			gaelcosnd_start(DrvGfxROM0, 0 * 0x0400000, 1 * 0x0400000, 0, 0);
		}
		break;

		case 6: // bang
		{
			if (BurnLoadRom(Drv68KROM  + 0x0000001,  0, 2)) return 1;
			if (BurnLoadRom(Drv68KROM  + 0x0000000,  1, 2)) return 1;

			if (BurnLoadRom(DrvGfxROM0 + 0x0000000,  2, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0080000,  3, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0100000,  4, 1)) return 1;
			memset (DrvGfxROM0 + 0x180000, 0, 0x80000);
			if (BurnLoadRom(DrvGfxROM0 + 0x0200000,  5, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0280000,  6, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0300000,  7, 1)) return 1;
			memset (DrvGfxROM0 + 0x380000, 0, 0x80000);
			if (BurnLoadRom(DrvGfxROM0 + 0x0400000,  8, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0480000,  9, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0500000, 10, 1)) return 1;
			memset (DrvGfxROM0 + 0x580000, 0, 0x80000);
			if (BurnLoadRom(DrvGfxROM0 + 0x0600000, 11, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0680000, 12, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0700000, 13, 1)) return 1;
			memset (DrvGfxROM0 + 0x780000, 0, 0x80000);
			if (BurnLoadRom(DrvGfxROM0 + 0x0800000, 14, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x0880000, 15, 1)) return 1;
			memset (DrvGfxROM0 + 0x900000, 0, 0x100000);

			DrvGfxDecode(0x0a00000);

			nCPUClockSpeed = 15000000;
			pIRQCallback = pBangIRQLineCallback;

			gaelcosnd_start(DrvGfxROM0, 0 * 0x0200000, 1 * 0x0200000, 2 * 0x0200000, 3 * 0x0200000);
		}
		break;
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(DrvSprRAM,		0x200000, 0x20ffff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		0x210000, 0x211fff, MAP_RAM);
	SekMapMemory(Drv68KRAM,		0xfe0000, 0xffffff, MAP_RAM);
	if (game_select == 2 || game_select == 3) // snowboard champ. extra ram
		SekMapMemory(Drv68KRAM2, 0x212000, 0x213fff, MAP_RAM);

	SekSetWriteWordHandler(0,	gaelco2_main_write_word);
	SekSetWriteByteHandler(0,	gaelco2_main_write_byte);
	SekSetReadWordHandler(0,	gaelco2_main_read_word);
	SekSetReadByteHandler(0,	gaelco2_main_read_byte);

	SekMapHandler(1,		0x202800, 0x202bff, MAP_WRITE | MAP_READ);
	SekSetWriteWordHandler(1,	gaelco2_sound_write_word);
	SekSetWriteByteHandler(1,	gaelco2_sound_write_byte);
	SekSetReadWordHandler(1,	gaelco2_sound_read_word);
	SekSetReadByteHandler(1,	gaelco2_sound_read_byte);

	SekMapHandler(2,		0x210000, 0x211fff, MAP_WRITE);
	SekSetWriteWordHandler(2,	gaelco2_palette_write_word);
	SekSetWriteByteHandler(2,	gaelco2_palette_write_byte);
	SekClose();

	EEPROMInit(&gaelco2_eeprom_interface);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	SekExit();

	EEPROMExit();

	if (game_select == 6)
		BurnGunExit();

	BurnFree (AllMem);

	gaelcosnd_exit();
	game_select = 0;

	return 0;
}

static void draw_layer(INT32 layer)
{
	INT32 offset = ((DrvVidRegs[layer] >> 9) & 0x07) * 0x1000;

	UINT16 *ram = (UINT16*)DrvSprRAM;

	INT32 scrolly = (ram[(0x2800 + (layer * 4))/2] + 0x01 - global_y_offset) & 0x1ff;

	if ((DrvVidRegs[layer] & 0x8000) == 0)
	{
		INT32 scrollx = (ram[(0x2802 + (layer * 4))/2] + 0x10 + (layer ? 0 : 4)) & 0x3ff;

		for (INT32 offs = 0; offs < 64 * 32; offs++)
		{
			INT32 sx = (offs & 0x3f) * 16;
			INT32 sy = (offs / 0x40) * 16;

			sx -= scrollx;
			if (sx < -15) sx += 1024;
			sy -= scrolly;
			if (sy < -15) sy += 512;

			if (sx >= nScreenWidth || sy >= nScreenHeight) continue;

			INT32 attr0 = ram[offset + (offs * 2) + 0];
			INT32 attr1 = ram[offset + (offs * 2) + 1];

			INT32 code  = (attr1 + ((attr0 & 0x07) << 16)) & gfxmask;

			INT32 color = (attr0 & 0xfe00) >> 9;
			INT32 flipx = (attr0 & 0x0080) ? 0xf : 0;
			INT32 flipy = (attr0 & 0x0040) ? 0xf : 0;

			if (flipy) {
				if (flipx) {
					Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 5, 0, 0, DrvGfxROM);
				} else {
					Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 5, 0, 0, DrvGfxROM);
				}
			} else {
				if (flipx) {
					Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 5, 0, 0, DrvGfxROM);
				} else {
					Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 5, 0, 0, DrvGfxROM);
				}
			}
		}
	}
	else
	{
		if (game_select == 4)
			scrolly += 32; // for touch n go alignment

		for (INT32 sy = 0; sy < nScreenHeight; sy++)
		{
			INT32 yy = (sy + scrolly) & 0x1ff;
			INT32 yy2 = (game_select == 4) ? yy : sy;

			INT32 scrollx = (ram[(0x2802 + (layer * 4))/2] + 0x10 + (layer ? 0 : 4)) & 0x3ff;
			if (DrvVidRegs[layer] & 0x8000) {
				if (layer) {
					scrollx = (ram[(0x2400/2) + yy2] + 0x10) & 0x3ff;
				} else {
					scrollx = (ram[(0x2000/2) + yy2] + 0x14) & 0x3ff;
				}
			}

			UINT16 *dst = pTransDraw + sy * nScreenWidth;

			for (INT32 sx = 0; sx < nScreenWidth + 16; sx += 16)
			{
				INT32 sxx = (sx + scrollx) & 0x3ff;
				INT32 xx = sx - (scrollx & 0xf);

				INT32 index = ((sxx / 16) + ((yy / 16) * 64)) * 2;

				INT32 attr0 = ram[offset + index + 0];
				INT32 attr1 = ram[offset + index + 1];

				INT32 code  = (attr1 + ((attr0 & 0x07) << 16)) & gfxmask;

				INT32 color = ((attr0 >> 9) & 0x7f) * 0x20;
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

static void draw_sprites(INT32 mask, INT32 xoffs)
{
	UINT16 *buffered_spriteram16 = (UINT16*)DrvSprBuf;
	int j, x, y, ex, ey, px, py;

	INT32 start_offset = (DrvVidRegs[1] & 0x10)*0x100;
	INT32 end_offset = start_offset + 0x1000;

	INT32 spr_x_adjust = ((nScreenWidth-1) - 320 + 1) - (511 - 320 - 1) - ((DrvVidRegs[0] >> 4) & 0x01) + xoffs;

	for (j = start_offset; j < end_offset; j += 8)
	{
		int data = buffered_spriteram16[(j/2) + 0];
		int data2 = buffered_spriteram16[(j/2) + 1];
		int data3 = buffered_spriteram16[(j/2) + 2];
		int data4 = buffered_spriteram16[(j/2) + 3];

		int sx = data3 & 0x3ff;
		int sy = data2 & 0x1ff;

		int xflip = data2 & 0x800;
		int yflip = data2 & 0x400;

		int xsize = ((data3 >> 12) & 0x0f) + 1;
		int ysize = ((data2 >> 12) & 0x0f) + 1;

		if (m_dual_monitor && ((data & 0x8000) != mask)) continue;

		if ((data2 & 0x0200) != 0)
		{
			for (y = 0; y < ysize; y++)
			{
				for (x = 0; x < xsize; x++)
				{
					int data5 = buffered_spriteram16[((data4/2) + (y*xsize + x)) & 0x7fff];
					int number = (((data & 0x1ff) << 10) + (data5 & 0x0fff)) & gfxmask;
					int color = ((data >> 9) & 0x7f) + ((data5 >> 12) & 0x0f);
					int color_effect = m_dual_monitor ? ((color & 0x3f) == 0x3f) : (color == 0x7f);

					ex = xflip ? (xsize - 1 - x) : x;
					ey = yflip ? (ysize - 1 - y) : y;

					if (color_effect == 0) // iq_132
					{
						if (yflip) {
							if (xflip) {
								Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, number, ((sx + ex*16) & 0x3ff) + spr_x_adjust, ((sy + ey*16) & 0x1ff) + global_y_offset, color, 5, 0, 0, DrvGfxROM);
							} else {
								Render16x16Tile_Mask_FlipY_Clip(pTransDraw, number, ((sx + ex*16) & 0x3ff) + spr_x_adjust, ((sy + ey*16) & 0x1ff) + global_y_offset, color, 5, 0, 0, DrvGfxROM);
							}
						} else {
							if (xflip) {
								Render16x16Tile_Mask_FlipX_Clip(pTransDraw, number, ((sx + ex*16) & 0x3ff) + spr_x_adjust, ((sy + ey*16) & 0x1ff) + global_y_offset, color, 5, 0, 0, DrvGfxROM);
							} else {
								Render16x16Tile_Mask_Clip(pTransDraw, number, ((sx + ex*16) & 0x3ff) + spr_x_adjust, ((sy + ey*16) & 0x1ff) + global_y_offset, color, 5, 0, 0, DrvGfxROM);
							}
						}
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
	if (nBurnLayer & 4) draw_sprites(0,0);

	BurnTransferCopy(DrvPalette);

	if (game_select == 6) {
		for (INT32 i = 0; i < nBurnGunNumPlayers; i++) {
			BurnGunDrawTarget(i, BurnGunX[i] >> 8, BurnGunY[i] >> 8);
		}
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
	INT32 nCyclesTotal[1] = { (nCPUClockSpeed * 10) / 591 }; // ?? MHZ @ 59.1 HZ
	INT32 nCyclesDone[1] = { 0 };
	INT32 nSegment = 0;

	SekOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nSegment = (nCyclesTotal[0] - nCyclesDone[0]) / (nInterleave - i);
		nCyclesDone[0] += SekRun(nSegment);

		pIRQCallback(i);

	}

	if (pBurnSoundOut) {
		gaelcosnd_update(pBurnSoundOut, nBurnSoundLen);
	}

	SekClose();

	if (pBurnDraw) {
		DrvDraw();
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

		if (game_select == 6)
			BurnGunScan();

		SCAN_VAR(snowboar_latch);
		SCAN_VAR(gun_interrupt);
		gaelcosnd_scan();
	}

	return 0;
}


// Maniac Square (unprotected)

static struct BurnRomInfo maniacsqRomDesc[] = {
	{ "d8-d15.1m",		0x020000, 0x9121d1b6, 0 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "d0-d7.1m",		0x020000, 0xa95cfd2a, 0 | BRF_PRG | BRF_ESS }, //  1

	{ "d0-d7.4m",		0x080000, 0xd8551b2f, 1 | BRF_GRA },           //  2 Graphics & Samples
	{ "d8-d15.4m",		0x080000, 0xb269c427, 1 | BRF_GRA },           //  3
	{ "d16-d23.1m",		0x020000, 0xaf4ea5e7, 1 | BRF_GRA },           //  4
	{ "d24-d31.1m",		0x020000, 0x578c3588, 1 | BRF_GRA },           //  5
};

STD_ROM_PICK(maniacsq)
STD_ROM_FN(maniacsq)

static INT32 maniacsqInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvManiacsq = {
	"maniacsq", NULL, NULL, NULL, "1996",
	"Maniac Square (unprotected)\0", NULL, "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, maniacsqRomInfo, maniacsqRomName, NULL, NULL, ManiacsqInputInfo, ManiacsqDIPInfo,
	maniacsqInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	320, 240, 4, 3
};


// Alligator Hunt

static struct BurnRomInfo aligatorRomDesc[] = {
	{ "u45",  		0x080000, 0x61c47c56, 0 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "u44",  		0x080000, 0xf0be007a, 0 | BRF_PRG | BRF_ESS }, //  1

	{ "u48",		0x400000, 0x19e03bf1, 1 | BRF_GRA },           //  2 Graphics & Samples
	{ "u47",		0x400000, 0x74a5a29f, 1 | BRF_GRA },           //  3
	{ "u50",		0x400000, 0x85daecf9, 1 | BRF_GRA },           //  4
	{ "u49",		0x400000, 0x70a4ee0b, 1 | BRF_GRA },           //  5
	
	{ "aligator_ds5002fp.bin", 0x080000, 0x00000000, BRF_OPT | BRF_NODUMP },
};

STD_ROM_PICK(aligator)
STD_ROM_FN(aligator)

static INT32 aligatorInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvAligator = {
	"aligator", NULL, NULL, NULL, "1994",
	"Alligator Hunt\0", "Play the unprotected clone, instead!", "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	0, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, aligatorRomInfo, aligatorRomName, NULL, NULL, AlighuntInputInfo, AlighuntDIPInfo,
	aligatorInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	320, 240, 4, 3
};


// Alligator Hunt (unprotected)

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
	"Alligator Hunt (unprotected)\0", NULL, "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, aligatorunRomInfo, aligatorunRomName, NULL, NULL, AlighuntInputInfo, AlighuntDIPInfo,
	aligatorInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	320, 240, 4, 3
};



// Snow Board Championship (Version 2.0)

static struct BurnRomInfo snowboaraRomDesc[] = {
	{ "sb53",		0x080000, 0xe4eaefd4, 0 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "sb55",		0x080000, 0xe2476994, 0 | BRF_PRG | BRF_ESS }, //  1

	{ "sb43",		0x200000, 0xafce54ed, 1 | BRF_GRA },           //  2 Graphics & Samples
	{ "sb44",		0x400000, 0x1bbe88bc, 1 | BRF_GRA },           //  3
	{ "sb45",		0x400000, 0x373983d9, 1 | BRF_GRA },           //  4
	{ "sb46",		0x400000, 0x22e7c648, 1 | BRF_GRA },           //  5
};

STD_ROM_PICK(snowboara)
STD_ROM_FN(snowboara)

static INT32 snowboaraInit()
{
	return DrvInit(2);
}

struct BurnDriver BurnDrvSnowboara = {
	"snowboara", "snowboar", NULL, NULL, "1994",
	"Snow Board Championship (Version 2.0)\0", NULL, "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_RACING, 0,
	NULL, snowboaraRomInfo, snowboaraRomName, NULL, NULL, SnowboarInputInfo, NULL,
	snowboaraInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	384, 240, 4, 3
};


// Snow Board Championship (Version 2.1)

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
	"snowboar", NULL, NULL, NULL, "1994",
	"Snow Board Championship (Version 2.1)\0", NULL, "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_RACING, 0,
	NULL, snowboarRomInfo, snowboarRomName, NULL, NULL, SnowboarInputInfo, NULL,
	snowboarInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	384, 240, 4, 3
};



// Touch & Go (World)

static struct BurnRomInfo touchgoRomDesc[] = {
	{ "tg_56",		0x080000, 0x8ab065f3, 1 | BRF_PRG | BRF_ESS }, //  0  68k Code
	{ "tg_57",		0x080000, 0x0dfd3f65, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ic69",		0x200000, 0x18bb12d4, 2 | BRF_GRA },           //  2 Graphics & Samples
	{ "ic65",		0x400000, 0x91b89c7c, 2 | BRF_GRA },           //  3
	{ "ic66",		0x200000, 0x52682953, 2 | BRF_GRA },           //  4
	{ "ic67",		0x400000, 0xc0a2ce5b, 2 | BRF_GRA },           //  5

	{ "touchgo_ds5002fp.bin", 0x8000, 0x00000000, 4 | BRF_NODUMP },        //  6 Dallas MCU
};

STD_ROM_PICK(touchgo)
STD_ROM_FN(touchgo)

static INT32 touchgoInit()
{
	return DrvInit(4);
}

struct BurnDriver BurnDrvTouchgo = {
	"touchgo", NULL, NULL, NULL, "1995",
	"Touch & Go (World)\0", NULL, "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	0, 4, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, touchgoRomInfo, touchgoRomName, NULL, NULL, TouchgoInputInfo, TouchgoDIPInfo,
	touchgoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	480, 240, 4, 3
};


// Touch & Go (Non North America)

static struct BurnRomInfo touchgonRomDesc[] = {
	{ "tg56.bin",		0x080000, 0xfd3b4642, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "tg57.bin",		0x080000, 0xee891835, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ic69",		0x200000, 0x18bb12d4, 2 | BRF_GRA },           //  2 Graphics & Samples
	{ "ic65",		0x400000, 0x91b89c7c, 2 | BRF_GRA },           //  3
	{ "ic66",		0x200000, 0x52682953, 2 | BRF_GRA },           //  4
	{ "ic67",		0x400000, 0xc0a2ce5b, 2 | BRF_GRA },           //  5

	{ "touchgo_ds5002fp.bin", 0x8000, 0x00000000, 4 | BRF_NODUMP },        //  6 Dallas MCU
};

STD_ROM_PICK(touchgon)
STD_ROM_FN(touchgon)

struct BurnDriver BurnDrvTouchgon = {
	"touchgon", "touchgo", NULL, NULL, "1995",
	"Touch & Go (Non North America)\0", NULL, "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_CLONE, 4, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, touchgonRomInfo, touchgonRomName, NULL, NULL, TouchgoInputInfo, TouchgoDIPInfo,
	touchgoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	480, 240, 4, 3
};


// Touch & Go (earlier revision)

static struct BurnRomInfo touchgoeRomDesc[] = {
	{ "tg56",		0x080000, 0x6d0f5c65, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "tg57",		0x080000, 0x845787b5, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ic69",		0x200000, 0x18bb12d4, 2 | BRF_GRA },           //  2 Graphics & Samples
	{ "ic65",		0x400000, 0x91b89c7c, 2 | BRF_GRA },           //  3
	{ "ic66",		0x200000, 0x52682953, 2 | BRF_GRA },           //  4
	{ "ic67",		0x400000, 0xc0a2ce5b, 2 | BRF_GRA },           //  5

	{ "touchgo_ds5002fp.bin", 0x8000, 0x00000000, 4 | BRF_NODUMP },        //  6 Dallas MCU
};

STD_ROM_PICK(touchgoe)
STD_ROM_FN(touchgoe)

struct BurnDriver BurnDrvTouchgoe = {
	"touchgoe", "touchgo", NULL, NULL, "1995",
	"Touch & Go (earlier revision)\0", NULL, "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_CLONE, 4, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, touchgoeRomInfo, touchgoeRomName, NULL, NULL, TouchgoInputInfo, TouchgoDIPInfo,
	touchgoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	480, 240, 4, 3
};


// Touch & Go (Korea, unprotected)

static struct BurnRomInfo touchgokRomDesc[] = {
	{ "56.IC56",		0x080000, 0xcbb87505, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "57.IC57",		0x080000, 0x36bcc7e7, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ic69",		0x200000, 0x18bb12d4, 2 | BRF_GRA },           //  2 Graphics & Samples
	{ "ic65",		0x400000, 0x91b89c7c, 2 | BRF_GRA },           //  3
	{ "ic66",		0x200000, 0x52682953, 2 | BRF_GRA },           //  4
	{ "ic67",		0x400000, 0xc0a2ce5b, 2 | BRF_GRA },           //  5
};

STD_ROM_PICK(touchgok)
STD_ROM_FN(touchgok)

struct BurnDriver BurnDrvTouchgok = {
	"touchgok", "touchgo", NULL, NULL, "1995",
	"Touch & Go (Korea, unprotected)\0", NULL, "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, touchgokRomInfo, touchgokRomName, NULL, NULL, TouchgoInputInfo, TouchgoDIPInfo,
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
	"Bang!\0", NULL, "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, bangRomInfo, bangRomName, NULL, NULL, BangInputInfo, NULL,
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
};

STD_ROM_PICK(bangj)
STD_ROM_FN(bangj)

struct BurnDriver BurnDrvBangj = {
	"bangj", "bang", NULL, NULL, "1998",
	"Gun Gabacho (Japan)\0", NULL, "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, bangjRomInfo, bangjRomName, NULL, NULL, BangInputInfo, NULL,
	bangInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	320, 240, 4, 3
};
