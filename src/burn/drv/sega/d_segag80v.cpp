// FB Alpha Sega G80 Vector driver module
// Based on MAME driver by Aaron Giles

// to do:
//  distortion/crackles on some USB stuff in tac/scan
//  fix iq_132's sega_decrypt70, remove mame's sega_decrypt70
//	fix games
//	verify samples playing correctly

#include "tiles_generic.h"
#include "z80_intf.h"
#include "i8039.h" // i8035
#include "vector.h"
#include "samples.h"
#include "ay8910.h"
#include "sp0250.h"
#include "usb_snd.h"
#include "bitswap.h" // BITSWAP08
#include "math.h" // ABS()

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvI8035ROM;
static UINT8 *DrvSndROM;
static UINT8 *DrvSineTable;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvUSBRAM;
static UINT8 *DrvVectorRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 spinner_select;
static UINT8 spinner_sign;
static UINT8 spinner_count;
static UINT8 mult_data;
static UINT16 mult_result;

static UINT8 i8035_p2;
static UINT8 i8035_t0;
static UINT8 i8035_drq;
static UINT8 i8035_latch;

static UINT8 (*sega_decrypt)(UINT16 offset, UINT8 data);
static void (*write_port_cb)(UINT8 offset, UINT8 data) = NULL;
static UINT8 (*read_port_cb)(UINT8 offset) = NULL;
static INT32 global_flipx = 0;
static INT32 global_flipy = 0;
static INT32 has_speech = 0;
static INT32 has_usb = 0; // tac/scan & startrek uses USB
static INT32 min_x;
static INT32 min_y;

static INT32 dialmode = 0; // tac/scan & startrek 1, others 0

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8]; // spinner left/right
static UINT8 DrvJoy5[8]; // fake coin inputs
static UINT8 DrvJoy6[1];
static UINT8 DrvDips[8];
static UINT8 DrvInputs[8];
static UINT8 DrvReset;

static struct BurnInputInfo Elim2InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy5 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 start"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy5 + 4,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 start"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 0,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Elim2)

static struct BurnInputInfo Elim2cInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy5 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 start"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy5 + 4,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 start"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 0,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Elim2c)

static struct BurnInputInfo Elim4InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy5 + 0,	"p1 coin"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy5 + 1,	"p2 coin"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 2"	},

	{"P3 Coin",			BIT_DIGITAL,	DrvJoy5 + 2,	"p3 coin"	},
	{"P3 Left",			BIT_DIGITAL,	DrvJoy3 + 7,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy3 + 6,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p3 fire 2"	},

	{"P4 Coin",			BIT_DIGITAL,	DrvJoy5 + 3,	"p4 coin"	},
	{"P4 Left",			BIT_DIGITAL,	DrvJoy3 + 3,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy3 + 2,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy3 + 0,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy3 + 1,	"p4 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 0,	"service"	},
	{"Service Mode",    BIT_DIGITAL,	DrvJoy6 + 0,	"diag"	    },
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Elim4)

static struct BurnInputInfo SpacfuryInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy5 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 start"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy5 + 4,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 start"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 6,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 7,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 0,	"service"	},
	{"Service Mode",    BIT_DIGITAL,	DrvJoy6 + 0,	"diag"	    },
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Spacfury)

static struct BurnInputInfo ZektorInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy5 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy4 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy4 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 0,	"service"	},
	{"Service Mode",    BIT_DIGITAL,	DrvJoy6 + 0,	"diag"	    },
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Zektor)

static struct BurnInputInfo TacscanInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy5 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy4 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy4 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,	    "reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 0,	"service"	},
	{"Service Mode",    BIT_DIGITAL,	DrvJoy6 + 0,	"diag"	    },
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Tacscan)

static struct BurnInputInfo StartrekInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy5 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy4 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy4 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 fire 4"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,	    "reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 0,	"service"	},
	{"Service Mode",    BIT_DIGITAL,	DrvJoy6 + 0,	"diag"	    },
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Startrek)

static struct BurnDIPInfo Elim2DIPList[]=
{
	{0x0e, 0xff, 0xff, 0x45, NULL						},
	{0x0f, 0xff, 0xff, 0x33, NULL						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x0e, 0x01, 0x01, 0x01, "Upright"					},
	{0x0e, 0x01, 0x01, 0x00, "Cocktail"					},

	{0   , 0xfe, 0   ,    3, "Lives"					},
	{0x0e, 0x01, 0x0c, 0x04, "3"						},
	{0x0e, 0x01, 0x0c, 0x08, "4"						},
	{0x0e, 0x01, 0x0c, 0x0c, "5"						},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x0e, 0x01, 0x30, 0x00, "Easy"						},
	{0x0e, 0x01, 0x30, 0x10, "Medium"					},
	{0x0e, 0x01, 0x30, 0x20, "Hard"						},
	{0x0e, 0x01, 0x30, 0x30, "Hardest"					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"				},
	{0x0e, 0x01, 0xc0, 0xc0, "None"						},
	{0x0e, 0x01, 0xc0, 0x80, "10000"					},
	{0x0e, 0x01, 0xc0, 0x40, "20000"					},
	{0x0e, 0x01, 0xc0, 0x00, "30000"					},

	{0   , 0xfe, 0   ,    16, "Coin A"					},
	{0x0f, 0x01, 0x0f, 0x00, "4 Coins 1 Credits"		},
	{0x0f, 0x01, 0x0f, 0x01, "3 Coins 1 Credits"		},
	{0x0f, 0x01, 0x0f, 0x09, "2 Coins/1 Credit 5/3 6/4"	},
	{0x0f, 0x01, 0x0f, 0x0a, "2 Coins/1 Credit 4/3"		},
	{0x0f, 0x01, 0x0f, 0x02, "2 Coins 1 Credits"		},
	{0x0f, 0x01, 0x0f, 0x03, "1 Coin  1 Credits"		},
	{0x0f, 0x01, 0x0f, 0x0b, "1 Coin/1 Credit 5/6"		},
	{0x0f, 0x01, 0x0f, 0x0c, "1 Coin/1 Credit 4/5"		},
	{0x0f, 0x01, 0x0f, 0x0d, "1 Coin/1 Credit 2/3"		},
	{0x0f, 0x01, 0x0f, 0x04, "1 Coin  2 Credits"		},
	{0x0f, 0x01, 0x0f, 0x0f, "1 Coin/2 Credits 4/9"		},
	{0x0f, 0x01, 0x0f, 0x0e, "1 Coin/2 Credits 5/11"	},
	{0x0f, 0x01, 0x0f, 0x05, "1 Coin  3 Credits"		},
	{0x0f, 0x01, 0x0f, 0x06, "1 Coin  4 Credits"		},
	{0x0f, 0x01, 0x0f, 0x07, "1 Coin  5 Credits"		},
	{0x0f, 0x01, 0x0f, 0x08, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    16, "Coin B"			},
	{0x0f, 0x01, 0xf0, 0x00, "4 Coins 1 Credits"		},
	{0x0f, 0x01, 0xf0, 0x10, "3 Coins 1 Credits"		},
	{0x0f, 0x01, 0xf0, 0x90, "2 Coins/1 Credit 5/3 6/4"	},
	{0x0f, 0x01, 0xf0, 0xa0, "2 Coins/1 Credit 4/3"		},
	{0x0f, 0x01, 0xf0, 0x20, "2 Coins 1 Credits"		},
	{0x0f, 0x01, 0xf0, 0x30, "1 Coin  1 Credits"		},
	{0x0f, 0x01, 0xf0, 0xb0, "1 Coin/1 Credit 5/6"		},
	{0x0f, 0x01, 0xf0, 0xc0, "1 Coin/1 Credit 4/5"		},
	{0x0f, 0x01, 0xf0, 0xd0, "1 Coin/1 Credit 2/3"		},
	{0x0f, 0x01, 0xf0, 0x40, "1 Coin  2 Credits"		},
	{0x0f, 0x01, 0xf0, 0xf0, "1 Coin/2 Credits 4/9"		},
	{0x0f, 0x01, 0xf0, 0xe0, "1 Coin/2 Credits 5/11"	},
	{0x0f, 0x01, 0xf0, 0x50, "1 Coin  3 Credits"		},
	{0x0f, 0x01, 0xf0, 0x60, "1 Coin  4 Credits"		},
	{0x0f, 0x01, 0xf0, 0x70, "1 Coin  5 Credits"		},
	{0x0f, 0x01, 0xf0, 0x80, "1 Coin  6 Credits"		},
};

STDDIPINFO(Elim2)

static struct BurnDIPInfo Elim4DIPList[]=
{
	{0x17, 0xff, 0xff, 0x45, NULL						},
	{0x18, 0xff, 0xff, 0x00, NULL						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x17, 0x01, 0x01, 0x01, "Upright"					},
	{0x17, 0x01, 0x01, 0x00, "Cocktail"					},

	{0   , 0xfe, 0   ,    3, "Lives"					},
	{0x17, 0x01, 0x0c, 0x04, "3"						},
	{0x17, 0x01, 0x0c, 0x08, "4"						},
	{0x17, 0x01, 0x0c, 0x0c, "5"						},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x17, 0x01, 0x30, 0x00, "Easy"						},
	{0x17, 0x01, 0x30, 0x10, "Medium"					},
	{0x17, 0x01, 0x30, 0x20, "Hard"						},
	{0x17, 0x01, 0x30, 0x30, "Hardest"					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"				},
	{0x18, 0x01, 0xc0, 0xc0, "None"						},
	{0x18, 0x01, 0xc0, 0x80, "10000"					},
	{0x18, 0x01, 0xc0, 0x40, "20000"					},
	{0x18, 0x01, 0xc0, 0x00, "30000"					},

	{0   , 0xfe, 0   ,    8, "Coin A"					},
	{0x18, 0x01, 0xe0, 0xe0, "8 Coins 1 Credits"		},
	{0x18, 0x01, 0xe0, 0xc0, "7 Coins 1 Credits"		},
	{0x18, 0x01, 0xe0, 0xa0, "6 Coins 1 Credits"		},
	{0x18, 0x01, 0xe0, 0x80, "5 Coins 1 Credits"		},
	{0x18, 0x01, 0xe0, 0x60, "4 Coins 1 Credits"		},
	{0x18, 0x01, 0xe0, 0x40, "3 Coins 1 Credits"		},
	{0x18, 0x01, 0xe0, 0x20, "2 Coins 1 Credits"		},
	{0x18, 0x01, 0xe0, 0x00, "1 Coin  1 Credits"		},
};

STDDIPINFO(Elim4)

static struct BurnDIPInfo SpacfuryDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x8d, NULL						},
	{0x10, 0xff, 0xff, 0x33, NULL						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x0f, 0x01, 0x01, 0x01, "Upright"					},
	{0x0f, 0x01, 0x01, 0x00, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x0f, 0x01, 0x02, 0x02, "Off"						},
	{0x0f, 0x01, 0x02, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x0f, 0x01, 0x0c, 0x00, "2"						},
	{0x0f, 0x01, 0x0c, 0x04, "3"						},
	{0x0f, 0x01, 0x0c, 0x08, "4"						},
	{0x0f, 0x01, 0x0c, 0x0c, "5"						},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x0f, 0x01, 0x30, 0x00, "Easy"						},
	{0x0f, 0x01, 0x30, 0x10, "Medium"					},
	{0x0f, 0x01, 0x30, 0x20, "Hard"						},
	{0x0f, 0x01, 0x30, 0x30, "Hardest"					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"				},
	{0x0f, 0x01, 0xc0, 0x00, "10000"					},
	{0x0f, 0x01, 0xc0, 0x40, "20000"					},
	{0x0f, 0x01, 0xc0, 0x80, "30000"					},
	{0x0f, 0x01, 0xc0, 0xc0, "40000"					},

	{0   , 0xfe, 0   ,    16, "Coin A"					},
	{0x10, 0x01, 0x0f, 0x00, "4 Coins 1 Credits"		},
	{0x10, 0x01, 0x0f, 0x01, "3 Coins 1 Credits"		},
	{0x10, 0x01, 0x0f, 0x09, "2 Coins/1 Credit 5/3 6/4"	},
	{0x10, 0x01, 0x0f, 0x0a, "2 Coins/1 Credit 4/3"		},
	{0x10, 0x01, 0x0f, 0x02, "2 Coins 1 Credits"		},
	{0x10, 0x01, 0x0f, 0x03, "1 Coin  1 Credits"		},
	{0x10, 0x01, 0x0f, 0x0b, "1 Coin/1 Credit 5/6"		},
	{0x10, 0x01, 0x0f, 0x0c, "1 Coin/1 Credit 4/5"		},
	{0x10, 0x01, 0x0f, 0x0d, "1 Coin/1 Credit 2/3"		},
	{0x10, 0x01, 0x0f, 0x04, "1 Coin  2 Credits"		},
	{0x10, 0x01, 0x0f, 0x0f, "1 Coin/2 Credits 4/9"		},
	{0x10, 0x01, 0x0f, 0x0e, "1 Coin/2 Credits 5/11"	},
	{0x10, 0x01, 0x0f, 0x05, "1 Coin  3 Credits"		},
	{0x10, 0x01, 0x0f, 0x06, "1 Coin  4 Credits"		},
	{0x10, 0x01, 0x0f, 0x07, "1 Coin  5 Credits"		},
	{0x10, 0x01, 0x0f, 0x08, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    16, "Coin B"			},
	{0x10, 0x01, 0xf0, 0x00, "4 Coins 1 Credits"		},
	{0x10, 0x01, 0xf0, 0x10, "3 Coins 1 Credits"		},
	{0x10, 0x01, 0xf0, 0x90, "2 Coins/1 Credit 5/3 6/4"	},
	{0x10, 0x01, 0xf0, 0xa0, "2 Coins/1 Credit 4/3"		},
	{0x10, 0x01, 0xf0, 0x20, "2 Coins 1 Credits"		},
	{0x10, 0x01, 0xf0, 0x30, "1 Coin  1 Credits"		},
	{0x10, 0x01, 0xf0, 0xb0, "1 Coin/1 Credit 5/6"		},
	{0x10, 0x01, 0xf0, 0xc0, "1 Coin/1 Credit 4/5"		},
	{0x10, 0x01, 0xf0, 0xd0, "1 Coin/1 Credit 2/3"		},
	{0x10, 0x01, 0xf0, 0x40, "1 Coin  2 Credits"		},
	{0x10, 0x01, 0xf0, 0xf0, "1 Coin/2 Credits 4/9"		},
	{0x10, 0x01, 0xf0, 0xe0, "1 Coin/2 Credits 5/11"	},
	{0x10, 0x01, 0xf0, 0x50, "1 Coin  3 Credits"		},
	{0x10, 0x01, 0xf0, 0x60, "1 Coin  4 Credits"		},
	{0x10, 0x01, 0xf0, 0x70, "1 Coin  5 Credits"		},
	{0x10, 0x01, 0xf0, 0x80, "1 Coin  6 Credits"		},
};

STDDIPINFO(Spacfury)

static struct BurnDIPInfo ZektorDIPList[]=
{
	{0x09, 0xff, 0xff, 0x8d, NULL						},
	{0x0a, 0xff, 0xff, 0x33, NULL						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x09, 0x01, 0x01, 0x01, "Upright"					},
	{0x09, 0x01, 0x01, 0x00, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x09, 0x01, 0x02, 0x02, "Off"						},
	{0x09, 0x01, 0x02, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x09, 0x01, 0x0c, 0x00, "2"						},
	{0x09, 0x01, 0x0c, 0x04, "3"						},
	{0x09, 0x01, 0x0c, 0x08, "4"						},
	{0x09, 0x01, 0x0c, 0x0c, "5"						},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x09, 0x01, 0x30, 0x00, "Easy"						},
	{0x09, 0x01, 0x30, 0x10, "Medium"					},
	{0x09, 0x01, 0x30, 0x20, "Hard"						},
	{0x09, 0x01, 0x30, 0x30, "Hardest"					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"				},
	{0x09, 0x01, 0xc0, 0x00, "None"						},
	{0x09, 0x01, 0xc0, 0xc0, "10000"					},
	{0x09, 0x01, 0xc0, 0x80, "20000"					},
	{0x09, 0x01, 0xc0, 0x40, "30000"					},

	{0   , 0xfe, 0   ,    16, "Coin A"					},
	{0x0a, 0x01, 0x0f, 0x00, "4 Coins 1 Credits"		},
	{0x0a, 0x01, 0x0f, 0x01, "3 Coins 1 Credits"		},
	{0x0a, 0x01, 0x0f, 0x09, "2 Coins/1 Credit 5/3 6/4"	},
	{0x0a, 0x01, 0x0f, 0x0a, "2 Coins/1 Credit 4/3"		},
	{0x0a, 0x01, 0x0f, 0x02, "2 Coins 1 Credits"		},
	{0x0a, 0x01, 0x0f, 0x03, "1 Coin  1 Credits"		},
	{0x0a, 0x01, 0x0f, 0x0b, "1 Coin/1 Credit 5/6"		},
	{0x0a, 0x01, 0x0f, 0x0c, "1 Coin/1 Credit 4/5"		},
	{0x0a, 0x01, 0x0f, 0x0d, "1 Coin/1 Credit 2/3"		},
	{0x0a, 0x01, 0x0f, 0x04, "1 Coin  2 Credits"		},
	{0x0a, 0x01, 0x0f, 0x0f, "1 Coin/2 Credits 4/9"		},
	{0x0a, 0x01, 0x0f, 0x0e, "1 Coin/2 Credits 5/11"	},
	{0x0a, 0x01, 0x0f, 0x05, "1 Coin  3 Credits"		},
	{0x0a, 0x01, 0x0f, 0x06, "1 Coin  4 Credits"		},
	{0x0a, 0x01, 0x0f, 0x07, "1 Coin  5 Credits"		},
	{0x0a, 0x01, 0x0f, 0x08, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    16, "Coin B"			},
	{0x0a, 0x01, 0xf0, 0x00, "4 Coins 1 Credits"		},
	{0x0a, 0x01, 0xf0, 0x10, "3 Coins 1 Credits"		},
	{0x0a, 0x01, 0xf0, 0x90, "2 Coins/1 Credit 5/3 6/4"	},
	{0x0a, 0x01, 0xf0, 0xa0, "2 Coins/1 Credit 4/3"		},
	{0x0a, 0x01, 0xf0, 0x20, "2 Coins 1 Credits"		},
	{0x0a, 0x01, 0xf0, 0x30, "1 Coin  1 Credits"		},
	{0x0a, 0x01, 0xf0, 0xb0, "1 Coin/1 Credit 5/6"		},
	{0x0a, 0x01, 0xf0, 0xc0, "1 Coin/1 Credit 4/5"		},
	{0x0a, 0x01, 0xf0, 0xd0, "1 Coin/1 Credit 2/3"		},
	{0x0a, 0x01, 0xf0, 0x40, "1 Coin  2 Credits"		},
	{0x0a, 0x01, 0xf0, 0xf0, "1 Coin/2 Credits 4/9"		},
	{0x0a, 0x01, 0xf0, 0xe0, "1 Coin/2 Credits 5/11"	},
	{0x0a, 0x01, 0xf0, 0x50, "1 Coin  3 Credits"		},
	{0x0a, 0x01, 0xf0, 0x60, "1 Coin  4 Credits"		},
	{0x0a, 0x01, 0xf0, 0x70, "1 Coin  5 Credits"		},
	{0x0a, 0x01, 0xf0, 0x80, "1 Coin  6 Credits"		},
};

STDDIPINFO(Zektor)

static struct BurnDIPInfo TacscanDIPList[]=
{
	{0x09, 0xff, 0xff, 0x8d, NULL						},
	{0x0a, 0xff, 0xff, 0x33, NULL						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x09, 0x01, 0x01, 0x01, "Upright"					},
	{0x09, 0x01, 0x01, 0x00, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x09, 0x01, 0x02, 0x02, "Off"						},
	{0x09, 0x01, 0x02, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Number of Ships"			},
	{0x09, 0x01, 0x0c, 0x00, "2"						},
	{0x09, 0x01, 0x0c, 0x04, "4"						},
	{0x09, 0x01, 0x0c, 0x08, "6"						},
	{0x09, 0x01, 0x0c, 0x0c, "8"						},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x09, 0x01, 0x30, 0x00, "Easy"						},
	{0x09, 0x01, 0x30, 0x10, "Normal"					},
	{0x09, 0x01, 0x30, 0x20, "Hard"						},
	{0x09, 0x01, 0x30, 0x30, "Very Hard"				},

	{0   , 0xfe, 0   ,    4, "Bonus Life"				},
	{0x09, 0x01, 0xc0, 0x00, "None"						},
	{0x09, 0x01, 0xc0, 0xc0, "10000"					},
	{0x09, 0x01, 0xc0, 0x80, "20000"					},
	{0x09, 0x01, 0xc0, 0x40, "30000"					},

	{0   , 0xfe, 0   ,    16, "Coin A"					},
	{0x0a, 0x01, 0x0f, 0x00, "4 Coins 1 Credits"		},
	{0x0a, 0x01, 0x0f, 0x01, "3 Coins 1 Credits"		},
	{0x0a, 0x01, 0x0f, 0x09, "2 Coins/1 Credit 5/3 6/4"	},
	{0x0a, 0x01, 0x0f, 0x0a, "2 Coins/1 Credit 4/3"		},
	{0x0a, 0x01, 0x0f, 0x02, "2 Coins 1 Credits"		},
	{0x0a, 0x01, 0x0f, 0x03, "1 Coin  1 Credits"		},
	{0x0a, 0x01, 0x0f, 0x0b, "1 Coin/1 Credit 5/6"		},
	{0x0a, 0x01, 0x0f, 0x0c, "1 Coin/1 Credit 4/5"		},
	{0x0a, 0x01, 0x0f, 0x0d, "1 Coin/1 Credit 2/3"		},
	{0x0a, 0x01, 0x0f, 0x04, "1 Coin  2 Credits"		},
	{0x0a, 0x01, 0x0f, 0x0f, "1 Coin/2 Credits 4/9"		},
	{0x0a, 0x01, 0x0f, 0x0e, "1 Coin/2 Credits 5/11"	},
	{0x0a, 0x01, 0x0f, 0x05, "1 Coin  3 Credits"		},
	{0x0a, 0x01, 0x0f, 0x06, "1 Coin  4 Credits"		},
	{0x0a, 0x01, 0x0f, 0x07, "1 Coin  5 Credits"		},
	{0x0a, 0x01, 0x0f, 0x08, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    16, "Coin B"					},
	{0x0a, 0x01, 0xf0, 0x00, "4 Coins 1 Credits"		},
	{0x0a, 0x01, 0xf0, 0x10, "3 Coins 1 Credits"		},
	{0x0a, 0x01, 0xf0, 0x90, "2 Coins/1 Credit 5/3 6/4"	},
	{0x0a, 0x01, 0xf0, 0xa0, "2 Coins/1 Credit 4/3"		},
	{0x0a, 0x01, 0xf0, 0x20, "2 Coins 1 Credits"		},
	{0x0a, 0x01, 0xf0, 0x30, "1 Coin  1 Credits"		},
	{0x0a, 0x01, 0xf0, 0xb0, "1 Coin/1 Credit 5/6"		},
	{0x0a, 0x01, 0xf0, 0xc0, "1 Coin/1 Credit 4/5"		},
	{0x0a, 0x01, 0xf0, 0xd0, "1 Coin/1 Credit 2/3"		},
	{0x0a, 0x01, 0xf0, 0x40, "1 Coin  2 Credits"		},
	{0x0a, 0x01, 0xf0, 0xf0, "1 Coin/2 Credits 4/9"		},
	{0x0a, 0x01, 0xf0, 0xe0, "1 Coin/2 Credits 5/11"	},
	{0x0a, 0x01, 0xf0, 0x50, "1 Coin  3 Credits"		},
	{0x0a, 0x01, 0xf0, 0x60, "1 Coin  4 Credits"		},
	{0x0a, 0x01, 0xf0, 0x70, "1 Coin  5 Credits"		},
	{0x0a, 0x01, 0xf0, 0x80, "1 Coin  6 Credits"		},
};

STDDIPINFO(Tacscan)

static struct BurnDIPInfo StartrekDIPList[]=
{
	{0x0b, 0xff, 0xff, 0x8d, NULL						},
	{0x0c, 0xff, 0xff, 0x33, NULL						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x0b, 0x01, 0x01, 0x01, "Upright"					},
	{0x0b, 0x01, 0x01, 0x00, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x0b, 0x01, 0x02, 0x02, "Off"						},
	{0x0b, 0x01, 0x02, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Photon Torpedoes"			},
	{0x0b, 0x01, 0x0c, 0x00, "1"						},
	{0x0b, 0x01, 0x0c, 0x04, "2"						},
	{0x0b, 0x01, 0x0c, 0x08, "3"						},
	{0x0b, 0x01, 0x0c, 0x0c, "4"						},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x0b, 0x01, 0x30, 0x00, "Easy"						},
	{0x0b, 0x01, 0x30, 0x10, "Medium"					},
	{0x0b, 0x01, 0x30, 0x20, "Hard"						},
	{0x0b, 0x01, 0x30, 0x30, "Tournament"				},

	{0   , 0xfe, 0   ,    4, "Bonus Life"				},
	{0x0b, 0x01, 0xc0, 0x00, "10000"					},
	{0x0b, 0x01, 0xc0, 0x40, "20000"					},
	{0x0b, 0x01, 0xc0, 0x80, "30000"					},
	{0x0b, 0x01, 0xc0, 0xc0, "40000"					},

	{0   , 0xfe, 0   ,    16, "Coin A"					},
	{0x0c, 0x01, 0x0f, 0x00, "4 Coins 1 Credits"		},
	{0x0c, 0x01, 0x0f, 0x01, "3 Coins 1 Credits"		},
	{0x0c, 0x01, 0x0f, 0x09, "2 Coins/1 Credit 5/3 6/4"	},
	{0x0c, 0x01, 0x0f, 0x0a, "2 Coins/1 Credit 4/3"		},
	{0x0c, 0x01, 0x0f, 0x02, "2 Coins 1 Credits"		},
	{0x0c, 0x01, 0x0f, 0x03, "1 Coin  1 Credits"		},
	{0x0c, 0x01, 0x0f, 0x0b, "1 Coin/1 Credit 5/6"		},
	{0x0c, 0x01, 0x0f, 0x0c, "1 Coin/1 Credit 4/5"		},
	{0x0c, 0x01, 0x0f, 0x0d, "1 Coin/1 Credit 2/3"		},
	{0x0c, 0x01, 0x0f, 0x04, "1 Coin  2 Credits"		},
	{0x0c, 0x01, 0x0f, 0x0f, "1 Coin/2 Credits 4/9"		},
	{0x0c, 0x01, 0x0f, 0x0e, "1 Coin/2 Credits 5/11"	},
	{0x0c, 0x01, 0x0f, 0x05, "1 Coin  3 Credits"		},
	{0x0c, 0x01, 0x0f, 0x06, "1 Coin  4 Credits"		},
	{0x0c, 0x01, 0x0f, 0x07, "1 Coin  5 Credits"		},
	{0x0c, 0x01, 0x0f, 0x08, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    16, "Coin B"					},
	{0x0c, 0x01, 0xf0, 0x00, "4 Coins 1 Credits"		},
	{0x0c, 0x01, 0xf0, 0x10, "3 Coins 1 Credits"		},
	{0x0c, 0x01, 0xf0, 0x90, "2 Coins/1 Credit 5/3 6/4"	},
	{0x0c, 0x01, 0xf0, 0xa0, "2 Coins/1 Credit 4/3"		},
	{0x0c, 0x01, 0xf0, 0x20, "2 Coins 1 Credits"		},
	{0x0c, 0x01, 0xf0, 0x30, "1 Coin  1 Credits"		},
	{0x0c, 0x01, 0xf0, 0xb0, "1 Coin/1 Credit 5/6"		},
	{0x0c, 0x01, 0xf0, 0xc0, "1 Coin/1 Credit 4/5"		},
	{0x0c, 0x01, 0xf0, 0xd0, "1 Coin/1 Credit 2/3"		},
	{0x0c, 0x01, 0xf0, 0x40, "1 Coin  2 Credits"		},
	{0x0c, 0x01, 0xf0, 0xf0, "1 Coin/2 Credits 4/9"		},
	{0x0c, 0x01, 0xf0, 0xe0, "1 Coin/2 Credits 5/11"	},
	{0x0c, 0x01, 0xf0, 0x50, "1 Coin  3 Credits"		},
	{0x0c, 0x01, 0xf0, 0x60, "1 Coin  4 Credits"		},
	{0x0c, 0x01, 0xf0, 0x70, "1 Coin  5 Credits"		},
	{0x0c, 0x01, 0xf0, 0x80, "1 Coin  6 Credits"		},
};

STDDIPINFO(Startrek)

static UINT8 spinner_input_read()
{
	INT8 delta = 0;

	if (spinner_select & 1)
		return DrvInputs[4];

	if (DrvJoy4[1]) delta = (dialmode) ? 0x10 : 3;
	if (DrvJoy4[0]) delta = (dialmode) ? -0x10 : -3;

	if (delta != 0)
	{
		spinner_sign = (delta >> 7) & 1;
		spinner_count += abs(delta);
	}

	return ~((spinner_count << 1) | spinner_sign);
}

static inline UINT8 demangle(UINT8 d7d6, UINT8 d5d4, UINT8 d3d2, UINT8 d1d0)
{
	return ((d7d6 << 7) & 0x80) | ((d7d6 << 2) & 0x40) |
		   ((d5d4 << 5) & 0x20) | ((d5d4 << 0) & 0x10) |
		   ((d3d2 << 3) & 0x08) | ((d3d2 >> 2) & 0x04) |
		   ((d1d0 << 1) & 0x02) | ((d1d0 >> 4) & 0x01);
}

static UINT8 mangled_ports_read(UINT8 offset)
{
	UINT8 d7d6 = DrvInputs[0];
	UINT8 d5d4 = DrvInputs[1];
	UINT8 d3d2 = DrvInputs[2];
	UINT8 d1d0 = DrvInputs[3];
	INT32 shift = offset & 3;
	return demangle(d7d6 >> shift, d5d4 >> shift, d3d2 >> shift, d1d0 >> shift);
}

static UINT16 decrypt_offset(UINT16 offset)
{
	UINT16 pc = ZetGetPrevPC(-1);

	if ((UINT16)pc == 0xffff || ZetReadByte(pc) != 0x32)
		return offset;

	return (offset & 0xff00) | (*sega_decrypt)(pc, ZetReadByte(pc + 1));
}

static void __fastcall segag80v_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf800) == 0xc800) {
		DrvZ80RAM[decrypt_offset(address & 0x7ff)] = data;
		return;
	}

	if ((address & 0xf000) == 0xd000 && has_usb) {
		usb_sound_prgram_write(decrypt_offset(address & 0xfff), data);
		return;
	}

	if ((address & 0xf000) == 0xe000) {
		DrvVectorRAM[decrypt_offset(address & 0xfff)] = data;
		return;
	}
}

static UINT8 __fastcall segag80v_read(UINT16 address)
{
	if ((address & 0xf000) == 0xd000 && has_usb) {
		return usb_sound_prgram_read(address);
	}
	
	return 0;
}

static void sync_sound()
{
	I8039Open(1);
	INT32 cyc = (ZetTotalCycles() / 10) - I8039TotalCycles();
	if (cyc > 0) {
		I8039Run(cyc);
	}
	I8039Close();
}

static void __fastcall segag80v_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0xbd:
			mult_data = data;
		return;

		case 0xbe:
			mult_result = mult_data * data;
		return;

		case 0xbf:
			// unknown_w (not used)
		return;

		case 0xf9:
		case 0xfd:
			// coin_count_w (not used in fba)
		return;

		case 0xf8:
			spinner_select = data;
		return;
	}

	if (write_port_cb) {
		if (has_usb) sync_sound();
		write_port_cb(port,data);
	}
}

static UINT8 __fastcall segag80v_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0xbc:
			return 0; // nop

		case 0xbe:
			{
				UINT8 result = mult_result;
				mult_result >>= 8;
				return result;
			}
		case 0xf8:
		case 0xf9:
		case 0xfa:
		case 0xfb:
			return mangled_ports_read(port);

		case 0xfc: if (dialmode != -1) return spinner_input_read();
			break; // continue to read_port_cb()!
	}

	if (read_port_cb) {
		if (has_usb) sync_sound();
		return read_port_cb(port);
	}

	return 0;
}

static void sega_speech_drq_write(INT32 state)
{
	i8035_drq = (state == CPU_IRQSTATUS_ACK) ? 1 : 0;
}

static void sega_speech_data_write(UINT8 data)
{
	UINT8 old = i8035_latch;
	i8035_latch = data;

	I8039Open(0);
	I8039SetIrqState((data & 0x80) ? CPU_IRQSTATUS_NONE : CPU_IRQSTATUS_ACK);
	I8039Close();
	
	if ((old & 0x80) == 0 && (data & 0x80))
		i8035_t0 = 1;
}

static UINT8 __fastcall sega_speech_read(UINT32 address)
{
	return DrvI8035ROM[address & 0x7ff];
}

static void __fastcall sega_speech_write_port(UINT32 port, UINT8 data)
{
	if (port < 0x100) {
		if (has_speech) sp0250_write(data);
		return;
	}

	switch (port & 0x1ff)
	{
		case I8039_p1:
			if ((data & 0x80) == 0) i8035_t0 = 0;
		return;

		case I8039_p2:
			i8035_p2 = data;
		return;
	}
}

static UINT8 __fastcall sega_speech_read_port(UINT32 port)
{
	if (port < 0x100) {
		return DrvSndROM[(port & 0xff) + ((i8035_p2 & 0x3f) * 0x100)];
	}

	switch (port & 0x1ff)
	{
		case I8039_p1:
			return i8035_latch;

		case I8039_t0:
			return i8035_t0;

		case I8039_t1:
			return i8035_drq;
	}

	return 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	I8039Open(0);
	I8039Reset();
	I8039Close();

	if (has_usb) usb_sound_reset();

	vector_reset();

	BurnSampleReset();
	AY8910Reset(0);
	if (has_speech) sp0250_reset();

	mult_data = 0;
	mult_result = 0;

	spinner_select = 1; // default to input4
	spinner_sign = 0;
	spinner_count = 0;

	i8035_p2 = 0;
	i8035_t0 = 0;
	i8035_drq = 0;
	i8035_latch = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x00c000;
	DrvI8035ROM		= Next; Next += 0x000800;

	DrvSndROM		= Next; Next += 0x004000;

	DrvSineTable	= Next; Next += 0x000400;

	DrvPalette		= (UINT32*)Next; Next += 0x4000 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x000800;
	DrvUSBRAM		= Next; Next += 0x001000;
	DrvVectorRAM	= Next; Next += 0x001000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvLoad()
{
	char* pRomName;
	struct BurnRomInfo ri;

	UINT8 *zLoad = DrvZ80ROM;
	UINT8 *sLoad = DrvSndROM;

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {

		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & 7) == 1) {
			if (BurnLoadRom(zLoad, i, 1)) return 1;
			zLoad += ri.nLen;
			continue;
		}

		if ((ri.nType & 7) == 2) {
			if (BurnLoadRom(DrvI8035ROM, i, 1)) return 1;
			has_speech = 1;
			continue;
		}

		if ((ri.nType & 7) == 3) {
			if (BurnLoadRom(sLoad, i, 1)) return 1;
			sLoad += ri.nLen;
			continue;
		}

		if ((ri.nType & 7) == 4) {
			if (BurnLoadRom(DrvSineTable, i, 1)) return 1;
			continue;
		}
	}

	return 0;
}

static INT32 DrvInit()
{
	BurnSetRefreshRate(40.00);

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (DrvLoad()) return 1;
	}
	
	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,			0xc800, 0xcfff, MAP_ROM);
//	ZetMapMemory(DrvUSBRAM,			0xd000, 0xdfff, MAP_ROM); // optional
	ZetMapMemory(DrvVectorRAM,		0xe000, 0xefff, MAP_ROM);
	ZetSetReadHandler(segag80v_read);
	ZetSetWriteHandler(segag80v_write);
	ZetSetOutHandler(segag80v_write_port);
	ZetSetInHandler(segag80v_read_port);
	ZetClose();

	// speech board games
	I8035Init(0);
	I8039Open(0);
	I8039SetProgramReadHandler(sega_speech_read);
	I8039SetCPUOpReadHandler(sega_speech_read);
	I8039SetCPUOpReadArgHandler(sega_speech_read);
	I8039SetIOReadHandler(sega_speech_read_port);
	I8039SetIOWriteHandler(sega_speech_write_port);
	I8039Close();
	
	if (has_usb) usb_sound_init(I8039TotalCycles, 400000);

	// zector
	AY8910Init(0, 1933560, 0);
	AY8910SetAllRoutes(0, 0.33, BURN_SND_ROUTE_BOTH);

	BurnSampleInit(1);
	BurnSampleSetAllRoutesAllSamples(0.50, BURN_SND_ROUTE_BOTH);

	if (has_speech) sp0250_init(3120000, sega_speech_drq_write, I8039TotalCycles, 208000);

	vector_init();
	vector_set_scale(1024, 832);

	min_x = 512;
	min_y = 608;

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	vector_exit();

	ZetExit();
	I8039Exit();

	if (has_speech) sp0250_exit();
	if (has_usb) usb_sound_exit();
	BurnSampleExit();
	AY8910Exit(0);

	write_port_cb = NULL;
	read_port_cb = NULL;
	global_flipx = 0;
	global_flipy = 0;
	has_speech = 0;
	has_usb = 0;
	dialmode = 0;

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x40; i++) // color
	{
		for (INT32 j = 0; j < 256; j++) // intensity
		{
			INT32 r = (i >> 4) & 3;
			r = (r << 6) | (r << 4) | (r << 2) | (r << 0);

			INT32 g = (i >> 2) & 3;
			g = (g << 6) | (g << 4) | (g << 2) | (g << 0);

			INT32 b = (i >> 0) & 3;
			b = (b << 6) | (b << 4) | (b << 2) | (b << 0);

			r = (r * j) / 255;
			g = (g * j) / 255;
			b = (b * j) / 255;

			DrvPalette[i * 256 + j] = (r << 16) | (g << 8) | b; // must be 32bit palette! -dink (see vector.cpp)
		}
	}
}

inline int adjust_xy(int rawx, int rawy, int *outx, int *outy)
{
	int clipped = 0;

	/* first apply the XOR at 0x200 */
	*outx = (rawx & 0x7ff) ^ 0x200;
	*outy = (rawy & 0x7ff) ^ 0x200;

	/* apply clipping logic to X */
	if ((*outx & 0x600) == 0x200)
		*outx = 0x000, clipped = 1;
	else if ((*outx & 0x600) == 0x400)
		*outx = 0x3ff, clipped = 1;
	else
		*outx &= 0x3ff;

	/* apply clipping logic to Y */
	if ((*outy & 0x600) == 0x200)
		*outy = 0x000, clipped = 1;
	else if ((*outy & 0x600) == 0x400)
		*outy = 0x3ff, clipped = 1;
	else
		*outy &= 0x3ff;

	/* convert into .16 values */
	*outx = (*outx - (min_x - 512)) << 16;
	*outy = (*outy - (min_y - 512)) << 16;

	if (global_flipx) *outx = ((1024 - 1) - (*outx >> 16)) << 16;
	if (global_flipy) *outy = ((832 - 1) - (*outy >> 16)) << 16;

	return clipped;
}

static void sega_generate_vector_list()
{
#define VECTOR_CLOCK		15468480			/* master clock */
#define U34_CLOCK			(VECTOR_CLOCK/3)	/* clock for interrupt chain */
#define VCL_CLOCK			(U34_CLOCK/2)		/* clock for vector generator */
#define U51_CLOCK			(VCL_CLOCK/16)		/* clock for phase generator */
#define IRQ_CLOCK			(U34_CLOCK/0x1f788)	/* 40Hz interrupt */
#define TIME_IN_HZ(hz)        (1.0 / (double)(hz))

	UINT8 *sintable = DrvSineTable;
	double total_time = TIME_IN_HZ(IRQ_CLOCK);
	UINT16 symaddr = 0;

	vector_reset();

	while (total_time > 0)
	{
		UINT16 curx, cury, xaccum, yaccum;
		UINT16 vecaddr, symangle;
		UINT8 scale, draw;

		draw = DrvVectorRAM[symaddr++ & 0xfff];

		curx  = DrvVectorRAM[symaddr++ & 0xfff];
		curx |= (DrvVectorRAM[symaddr++ & 0xfff] & 7) << 8;
		curx |= (curx << 1) & 0x800;

		cury  = DrvVectorRAM[symaddr++ & 0xfff];
		cury |= (DrvVectorRAM[symaddr++ & 0xfff] & 7) << 8;
		cury |= (cury << 1) & 0x800;

		vecaddr  = DrvVectorRAM[symaddr++ & 0xfff];
		vecaddr |= (DrvVectorRAM[symaddr++ & 0xfff] & 0xf) << 8;

		symangle  = DrvVectorRAM[symaddr++ & 0xfff];
		symangle |= (DrvVectorRAM[symaddr++ & 0xfff] & 3) << 8;

		scale = DrvVectorRAM[symaddr++ & 0xfff];

		total_time -= 10 * TIME_IN_HZ(U51_CLOCK);

		if (draw & 1)
		{
			int adjx, adjy, clipped;

			clipped = adjust_xy(curx, cury, &adjx, &adjy);
			if (!clipped)
				vector_add_point(adjx, adjy, 0, 0);

			while (total_time > 0)
			{
				UINT16 vecangle, length, deltax, deltay;
				UINT8 attrib, intensity;
				UINT32 color;

				attrib = DrvVectorRAM[vecaddr++ & 0xfff];
				length = (DrvVectorRAM[vecaddr++ & 0xfff] * scale) >> 7;

				vecangle  = DrvVectorRAM[vecaddr++ & 0xfff];
				vecangle |= (DrvVectorRAM[vecaddr++ & 0xfff] & 3) << 8;

				deltax = sintable[((vecangle + symangle) & 0x1ff) << 1];
				deltay = sintable[((vecangle + symangle + 0x100) & 0x1ff) << 1];

				total_time -= 4 * TIME_IN_HZ(U51_CLOCK);

				color = ((attrib >> 1) & 0x3f);
				if ((attrib & 1) && color)
					intensity = 0xff;
				else
					intensity = 0;

				clipped = adjust_xy(curx, cury, &adjx, &adjy);
				xaccum = yaccum = 0;
				while (length-- != 0 && total_time > 0)
				{
					int newclip;

					xaccum += deltax + (deltax >> 7);

					if (((vecangle + symangle) & 0x200) == 0)
						curx += xaccum >> 8;
					else
						curx -= xaccum >> 8;
					xaccum &= 0xff;

					yaccum += deltay + (deltay >> 7);

					if (((vecangle + symangle + 0x100) & 0x200) == 0)
						cury += yaccum >> 8;
					else
						cury -= yaccum >> 8;
					yaccum &= 0xff;

					newclip = adjust_xy(curx, cury, &adjx, &adjy);
					if (newclip != clipped)
					{
						if (!newclip)
							vector_add_point(adjx, adjy, 0, 0);
						else
							vector_add_point(adjx, adjy, color, intensity);
					}
					clipped = newclip;

					total_time -= TIME_IN_HZ(VCL_CLOCK);
				}

				if (!clipped)
					vector_add_point(adjx, adjy, color, intensity);

				if (attrib & 0x80)
					break;
			}
		}

		if (draw & 0x80)
			break;
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	sega_generate_vector_list();

	draw_vector(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	I8039NewFrame();
	ZetNewFrame();

	{
		DrvInputs[0] = 0xff;
		DrvInputs[1] = 0xff;
		DrvInputs[2] = DrvDips[0];
		DrvInputs[3] = DrvDips[1];
		DrvInputs[4] = 0;
		DrvInputs[5] = 0;  // elim4 fake coins input

		if (dialmode == -1) { // eliminator 4, hold coin for 1 frame
			static UINT8 last = 0;
			UINT8 coins = (DrvJoy5[0] | DrvJoy5[1] | DrvJoy5[2] | DrvJoy5[3]);
			DrvJoy1[0] = (last == 0 && coins);
			last = coins;
		} else { // others, hold for 1 frame
			static UINT8 last[2] = { 0, 0 };
			UINT8 coin[2] = { DrvJoy5[0], DrvJoy5[4] };
			DrvJoy1[0] = (last[0] == 0 && coin[0]);
			DrvJoy1[4] = (last[1] == 0 && coin[1]);
			last[0] = coin[0];
			last[1] = coin[1];
		}

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		//	DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		//	DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
			DrvInputs[4] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[5] ^= (DrvJoy5[i] & 1) << i;
		}

		{ // Service Mode (Diagnostics) NMI
			static UINT8 OldDiag = 0;
			if (DrvJoy6[0] == 0 && OldDiag == 1) {
				ZetOpen(0);
				ZetNmi();
				ZetClose();
			}
			OldDiag = DrvJoy6[0];
		}
	}

	INT32 nInterleave = 232; // for speech chip
	INT32 nCyclesTotal[3] = { 4000000 / 40, 208000 / 40, 400000 / 40 };
	INT32 nCyclesDone[3] = { 0, 0, 0 };
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);
		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);

		if (has_speech) {
			I8039Open(0);
			nCyclesDone[1] += I8039Run(((i + 1) * nCyclesTotal[1] / nInterleave) - nCyclesDone[1]);
			sp0250_tick();
			I8039Close();
		}

		if (has_usb) {
			nCyclesDone[2] += usb_sound_run(((i + 1) * nCyclesTotal[2] / nInterleave) - nCyclesDone[2]);
			if ((i % 6) != 4) usb_timer_t1_clock();
		}
	}

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
		BurnSampleRender(pBurnSoundOut, nBurnSoundLen);
		if (has_speech) {
			I8039Open(0);
			sp0250_update(pBurnSoundOut, nBurnSoundLen);
			I8039Close();
		}
		if (has_usb) {
			segausb_update(pBurnSoundOut, nBurnSoundLen);
		}

	}

	ZetClose();
	
	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
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

		ZetScan(nAction);
		I8039Scan(nAction, pnMin);
		if (has_speech) sp0250_scan(nAction, pnMin);
		if (has_usb) usb_sound_scan(nAction, pnMin);
		BurnSampleScan(nAction, pnMin);
		AY8910Scan(nAction, pnMin);
		vector_scan(nAction);

		SCAN_VAR(spinner_select);
		SCAN_VAR(spinner_sign);
		SCAN_VAR(spinner_count);
		SCAN_VAR(mult_data);
		SCAN_VAR(mult_result);
		SCAN_VAR(i8035_p2);
		SCAN_VAR(i8035_t0);
		SCAN_VAR(i8035_drq);
		SCAN_VAR(i8035_latch);
	}

	return 0;
}


// Eliminator (2 Players, set 1)

static struct BurnRomInfo elim2RomDesc[] = {
	{ "969.cpu-u25",		0x0800, 0x411207f2, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "1333.prom-u1",		0x0800, 0xfd2a2916, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "1334.prom-u2",		0x0800, 0x79eb5548, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "1335.prom-u3",		0x0800, 0x3944972e, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "1336.prom-u4",		0x0800, 0x852f7b4d, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "1337.prom-u5",		0x0800, 0xcf932b08, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "1338.prom-u6",		0x0800, 0x99a3f3c9, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "1339.prom-u7",		0x0800, 0xd35f0fa3, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "1340.prom-u8",		0x0800, 0x8fd4da21, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "1341.prom-u9",		0x0800, 0x629c9a28, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "1342.prom-u10",		0x0800, 0x643df651, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "1343.prom-u11",		0x0800, 0xd29d70d2, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "1344.prom-u12",		0x0800, 0xc5e153a3, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "1345.prom-u13",		0x0800, 0x40597a92, 1 | BRF_PRG | BRF_ESS }, // 13

	{ "s-c.xyt-u39",		0x0400, 0x56484d19, 4 | BRF_GRA },           // 14 Sine Tables

	{ "pr-82.cpu-u15",		0x0020, 0xc609b79e, 0 | BRF_OPT },           // 15 Addressing PROM
};

STD_ROM_PICK(elim2)
STD_ROM_FN(elim2)

static struct BurnSampleInfo elim2SampleDesc[] = {
	{"elim1",		SAMPLE_NOLOOP },
	{"elim2",		SAMPLE_NOLOOP },
	{"elim3",		SAMPLE_NOLOOP },
	{"elim4",		SAMPLE_NOLOOP },
	{"elim5",		SAMPLE_NOLOOP },
	{"elim6",		SAMPLE_NOLOOP },
	{"elim7",		SAMPLE_NOLOOP },
	{"elim8",		SAMPLE_NOLOOP },
	{"elim9",		SAMPLE_NOLOOP },
	{"elim10",		SAMPLE_NOLOOP },
	{"elim11",		SAMPLE_NOLOOP },
	{"elim12",		SAMPLE_NOLOOP },
	{"",            0             }
};

STD_SAMPLE_PICK(elim2)
STD_SAMPLE_FN(elim2)

static void elim2_sh1_write(UINT8 data)
{
	if (data & 0x02) BurnSamplePlay(0);
	if (data & 0x04) BurnSamplePlay(10);
	if (data & 0x08) BurnSamplePlay(9);
	if (data & 0x10) BurnSamplePlay(8);

	if (data & 0x20)
	{
		if (BurnSampleGetStatus(1))
			BurnSampleStop(1);
		BurnSamplePlay(1);
	}

	if (data & 0xc0)
	{
		if (BurnSampleGetStatus(5))
			BurnSampleStop(5);
		BurnSamplePlay(5);
	}
}

static void elim2_sh2_write(UINT8 data)
{
	if (data & 0x0f)
		BurnSamplePlay(6);
	else
		BurnSampleStop(6);

	if (data & 0x10) BurnSamplePlay(2);
	if (data & 0x20) BurnSamplePlay(3);
	if (data & 0x40) BurnSamplePlay(7);
	if (data & 0x80) BurnSamplePlay(4);
}

static void elim2_port_write(UINT8 port, UINT8 data)
{
	switch (port)
	{
		case 0x3e:
			elim2_sh1_write(data ^ 0xff);
		return;

		case 0x3f:
			elim2_sh2_write(data ^ 0xff);
		return;
	}
}

static UINT8 sega_decrypt70(UINT16 pc, UINT8 lo)
{
	switch (pc & 0x09)
	{
		case 0x00:
			return BITSWAP08(lo, 2, 7, 3, 4, 6, 5, 1, 0) ^ 0x10;

		case 0x01:
			return lo;

		case 0x08:
			return BITSWAP08(lo, 2, 4, 5, 3, 7, 6, 1, 0) ^ 0x80;

		case 0x09:
			return BITSWAP08(lo, 2, 3, 6, 5, 7, 4, 1, 0) ^ 0x20;
	}

	return lo;
}

static INT32 Elim2Init()
{
	global_flipy = 1;
	sega_decrypt = sega_decrypt70;
	write_port_cb = elim2_port_write;

	return DrvInit();
}

struct BurnDriver BurnDrvElim2 = {
	"elim2", NULL, NULL, "elim2", "1981",
	"Eliminator (2 Players, set 1)\0", NULL, "Gremlin", "G80 Vector",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_SHOOT, 0,
	NULL, elim2RomInfo, elim2RomName, NULL, NULL, elim2SampleInfo, elim2SampleName, Elim2InputInfo, Elim2DIPInfo,
	Elim2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	800, 600, 4, 3
};


// Eliminator (2 Players, set 2)

static struct BurnRomInfo elim2aRomDesc[] = {
	{ "969.cpu-u25",		0x0800, 0x411207f2, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "1158.prom-u1",		0x0800, 0xa40ac3a5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "1159.prom-u2",		0x0800, 0xff100604, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "1160a.prom-u3",		0x0800, 0xebfe33bd, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "1161a.prom-u4",		0x0800, 0x03d41db3, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "1162a.prom-u5",		0x0800, 0xf2c7ece3, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "1163a.prom-u6",		0x0800, 0x1fc58b00, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "1164a.prom-u7",		0x0800, 0xf37480d1, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "1165a.prom-u8",		0x0800, 0x328819f8, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "1166a.prom-u9",		0x0800, 0x1b8e8380, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "1167a.prom-u10",		0x0800, 0x16aa3156, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "1168a.prom-u11",		0x0800, 0x3c7c893a, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "1169a.prom-u12",		0x0800, 0x5cee23b1, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "1170a.prom-u13",		0x0800, 0x8cdacd35, 1 | BRF_PRG | BRF_ESS }, // 13

	{ "s-c.xyt-u39",		0x0400, 0x56484d19, 4 | BRF_GRA },           // 14 Sine Tables

	{ "pr-82.cpu-u15",		0x0020, 0xc609b79e, 0 | BRF_OPT },           // 15 Addressing PROM
};

STD_ROM_PICK(elim2a)
STD_ROM_FN(elim2a)

struct BurnDriver BurnDrvElim2a = {
	"elim2a", "elim2", NULL, "elim2", "1981",
	"Eliminator (2 Players, set 2)\0", NULL, "Gremlin", "G80 Vector",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MISC, GBF_SHOOT, 0,
	NULL, elim2aRomInfo, elim2aRomName, NULL, NULL, elim2SampleInfo, elim2SampleName, Elim2InputInfo, Elim2DIPInfo,
	Elim2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	800, 600, 4, 3
};


// Eliminator (2 Players, cocktail)

static struct BurnRomInfo elim2cRomDesc[] = {
	{ "969t.cpu-u25",		0x0800, 0x896a615c, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "1200.prom-u1",		0x0800, 0x590beb6a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "1201.prom-u2",		0x0800, 0xfed32b30, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "1202.prom-u3",		0x0800, 0x0a2068d0, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "1203.prom-u4",		0x0800, 0x1f593aa2, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "1204.prom-u5",		0x0800, 0x046f1030, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "1205.prom-u6",		0x0800, 0x8d10b870, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "1206.prom-u7",		0x0800, 0x7f6c5afa, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "1207.prom-u8",		0x0800, 0x6cc74d62, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "1208.prom-u9",		0x0800, 0xcc37a631, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "1209.prom-u10",		0x0800, 0x844922f8, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "1210.prom-u11",		0x0800, 0x7b289783, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "1211.prom-u12",		0x0800, 0x17349db7, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "1212.prom-u13",		0x0800, 0x152cf376, 1 | BRF_PRG | BRF_ESS }, // 13

	{ "s-c.xyt-u39",		0x0400, 0x56484d19, 4 | BRF_GRA },           // 14 Sine Tables

	{ "pr-82.cpu-u15",		0x0020, 0xc609b79e, 0 | BRF_OPT },           // 15 Addressing PROM
};

STD_ROM_PICK(elim2c)
STD_ROM_FN(elim2c)

struct BurnDriver BurnDrvElim2c = {
	"elim2c", "elim2", NULL, "elim2", "1981",
	"Eliminator (2 Players, cocktail)\0", NULL, "Gremlin", "G80 Vector",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MISC, GBF_SHOOT, 0,
	NULL, elim2cRomInfo, elim2cRomName, NULL, NULL, elim2SampleInfo, elim2SampleName, Elim2cInputInfo, Elim2DIPInfo,
	Elim2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	800, 600, 4, 3
};


// Eliminator (4 Players)

static struct BurnRomInfo elim4RomDesc[] = {
	{ "1390.cpu-u25",		0x0800, 0x97010c3e, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "1347.prom-u1",		0x0800, 0x657d7320, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "1348.prom-u2",		0x0800, 0xb15fe578, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "1349.prom-u3",		0x0800, 0x0702b586, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "1350.prom-u4",		0x0800, 0x4168dd3b, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "1351.prom-u5",		0x0800, 0xc950f24c, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "1352.prom-u6",		0x0800, 0xdc8c91cc, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "1353.prom-u7",		0x0800, 0x11eda631, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "1354.prom-u8",		0x0800, 0xb9dd6e7a, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "1355.prom-u9",		0x0800, 0xc92c7237, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "1356.prom-u10",		0x0800, 0x889b98e3, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "1357.prom-u11",		0x0800, 0xd79248a5, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "1358.prom-u12",		0x0800, 0xc5dabc77, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "1359.prom-u13",		0x0800, 0x24c8e5d8, 1 | BRF_PRG | BRF_ESS }, // 13
	{ "1360.prom-u14",		0x0800, 0x96d48238, 1 | BRF_PRG | BRF_ESS }, // 14

	{ "s-c.xyt-u39",		0x0400, 0x56484d19, 4 | BRF_GRA },           // 15 Sine Tables

	{ "pr-82.cpu-u15",		0x0020, 0xc609b79e, 0 | BRF_OPT },           // 16 Addressing PROM
};

STD_ROM_PICK(elim4)
STD_ROM_FN(elim4)

static UINT8 sega_decrypt76(UINT16 pc, UINT8 lo)
{
	switch (pc & 0x09)
	{
		case 0x00:
			return lo;

		case 0x01:
			return BITSWAP08(lo, 2, 7, 3, 4, 6, 5, 1, 0) ^ 0x10;

		case 0x08:
			return BITSWAP08(lo, 2, 3, 6, 5, 7, 4, 1, 0) ^ 0x20;

		case 0x09:
			return BITSWAP08(lo, 2, 4, 5, 3, 7, 6, 1, 0) ^ 0x80;
	}

	return lo;
}

static UINT8 elim4_port_read(UINT8 port)
{
	switch (port)
	{
		case 0xfc: {
			if (spinner_select & 0x08) {
				switch (spinner_select & 0x07) {
					case 6: return DrvInputs[4]; // p3,p4 inputs
					case 7: return DrvInputs[5]; // coins
				}
			}
			return 0;
		}
	}

	return 0;
}

static INT32 Elim4Init()
{
	dialmode = -1; // spinner disabled - bypass 0xfc for elim4_port_read()
	global_flipy = 1;
	sega_decrypt = sega_decrypt76;
	write_port_cb = elim2_port_write;
	read_port_cb = elim4_port_read;

	return DrvInit();
}

struct BurnDriver BurnDrvElim4 = {
	"elim4", "elim2", NULL, "elim2", "1981",
	"Eliminator (4 Players)\0", NULL, "Gremlin", "G80 Vector",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MISC, GBF_SHOOT, 0,
	NULL, elim4RomInfo, elim4RomName, NULL, NULL, elim2SampleInfo, elim2SampleName, Elim4InputInfo, Elim4DIPInfo,
	Elim4Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	800, 600, 4, 3
};


// Eliminator (4 Players, prototype)

static struct BurnRomInfo elim4pRomDesc[] = {
	{ "1390.cpu-u25",		0x0800, 0x97010c3e, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "sw1.prom-u1",		0x0800, 0x5350b8eb, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sw2.prom-u2",		0x0800, 0x44f45465, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sw3.prom-u3",		0x0800, 0x5b692c3c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "sw4.prom-u4",		0x0800, 0x0b78dd00, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "sw5.prom-u5",		0x0800, 0x8b3795f1, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "sw6.prom-u6",		0x0800, 0x4304b503, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "sw7.prom-u7",		0x0800, 0x3cb4a604, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "sw8.prom-u8",		0x0800, 0xbdc55223, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "sw9.prom-u9",		0x0800, 0xf6ca1bf1, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "swa.prom-u10",		0x0800, 0x12373f7f, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "swb.prom-u11",		0x0800, 0xd1effc6b, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "swc.prom-u12",		0x0800, 0xbf361ab3, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "swd.prom-u13",		0x0800, 0xae2c88e5, 1 | BRF_PRG | BRF_ESS }, // 13
	{ "swe.prom-u14",		0x0800, 0xec4cc343, 1 | BRF_PRG | BRF_ESS }, // 14

	{ "s-c.xyt-u39",		0x0400, 0x56484d19, 4 | BRF_GRA },           // 15 Sine Table

	{ "pr-82.cpu-u15",		0x0020, 0xc609b79e, 0 | BRF_OPT },           // 15 Addressing PROM
};

STD_ROM_PICK(elim4p)
STD_ROM_FN(elim4p)

struct BurnDriver BurnDrvElim4p = {
	"elim4p", "elim2", NULL, "elim2", "1981",
	"Eliminator (4 Players, prototype)\0", NULL, "Gremlin", "G80 Vector",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MISC, GBF_SHOOT, 0,
	NULL, elim4pRomInfo, elim4pRomName, NULL, NULL, elim2SampleInfo, elim2SampleName, Elim4InputInfo, Elim4DIPInfo,
	Elim4Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	800, 600, 4, 3
};


// Space Fury (revision C)

static struct BurnRomInfo spacfuryRomDesc[] = {
	{ "969c.cpu-u25",		0x0800, 0x411207f2, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "960c.prom-u1",		0x0800, 0xd071ab7e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "961c.prom-u2",		0x0800, 0xaebc7b97, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "962c.prom-u3",		0x0800, 0xdbbba35e, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "963c.prom-u4",		0x0800, 0xd9e9eadc, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "964c.prom-u5",		0x0800, 0x7ed947b6, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "965c.prom-u6",		0x0800, 0xd2443a22, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "966c.prom-u7",		0x0800, 0x1985ccfc, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "967c.prom-u8",		0x0800, 0x330f0751, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "968c.prom-u9",		0x0800, 0x8366eadb, 1 | BRF_PRG | BRF_ESS }, //  9

	{ "808c.speech-u7",		0x0800, 0xb779884b, 2 | BRF_PRG | BRF_ESS }, // 10 I8035 Code

	{ "970c.speech-u6",		0x1000, 0x979d8535, 3 | BRF_SND },           // 11 Speech Data
	{ "971c.speech-u5",		0x1000, 0x022dbd32, 3 | BRF_SND },           // 12
	{ "972c.speech-u4",		0x1000, 0xfad9346d, 3 | BRF_SND },           // 13

	{ "s-c.xyt-u39",		0x0400, 0x56484d19, 4 | BRF_GRA },           // 14 Sine Tables

	{ "pr-82.cpu-u15",		0x0020, 0xc609b79e, 0 | BRF_OPT },           // 15 Addressing PROMs
	{ "6331.speech-u30",	0x0020, 0xadcb81d0, 0 | BRF_OPT },           // 16
};

STD_ROM_PICK(spacfury)
STD_ROM_FN(spacfury)

static struct BurnSampleInfo spacfurySampleDesc[] = {
	{"sfury1",		SAMPLE_NOLOOP },
	{"sfury2",		SAMPLE_NOLOOP },
	{"sfury3",		SAMPLE_NOLOOP },
	{"sfury4",		SAMPLE_NOLOOP },
	{"sfury5",		SAMPLE_NOLOOP },
	{"sfury6",		SAMPLE_NOLOOP },
	{"sfury7",		SAMPLE_NOLOOP },
	{"sfury8",		SAMPLE_NOLOOP },
	{"sfury9",		SAMPLE_NOLOOP },
	{"sfury10",		SAMPLE_NOLOOP },
	{"",            0             }
};

STD_SAMPLE_PICK(spacfury)
STD_SAMPLE_FN(spacfury)

static void spacfury_sh1_write(UINT8 data)
{
	if (data & 0x02)
	{
		if (!BurnSampleGetStatus(1))
			BurnSamplePlay(1);
	}
	else
		BurnSampleStop(1);

	if (data & 0x04)
	{
		if (!BurnSampleGetStatus(4))
			BurnSamplePlay(4);
	}
	else
		BurnSampleStop(4);

	if (data & 0x01) BurnSamplePlay(0);
	if (data & 0x40) BurnSamplePlay(8);
	if (data & 0x80) BurnSamplePlay(9);
}

static void spacfury_sh2_write(UINT8 data)
{
	if (data & 0x02)
	{
		if (BurnSampleGetStatus(3))
			BurnSampleStop(3);
		BurnSamplePlay(3);
	}

	if (data & 0x01) BurnSamplePlay(2);
	if (data & 0x04) BurnSamplePlay(6);
	if (data & 0x08) BurnSamplePlay(6);
	if (data & 0x10) BurnSamplePlay(5);
	if (data & 0x20) BurnSamplePlay(7);
}

static void spacfury_port_write(UINT8 port, UINT8 data)
{
	switch (port)
	{
		case 0x38:
			sega_speech_data_write(data);
		return;

		case 0x3b:
			// speech_sound control_w (not used for emulation)
		return;

		case 0x3e:
			spacfury_sh1_write(data ^ 0xff);
		return;

		case 0x3f:
			spacfury_sh2_write(data ^ 0xff);
		return;
	}
}

static UINT8 sega_decrypt64(UINT16 pc, UINT8 lo)
{
	switch (pc & 0x03)
	{
		case 0x00:
			return lo;

		case 0x01:
			return BITSWAP08(lo, 2, 7, 3, 4, 6, 5, 1, 0) ^ 0x10;

		case 0x02:
			return BITSWAP08(lo, 2, 3, 6, 5, 7, 4, 1, 0) ^ 0x20;

		case 0x03:
			return BITSWAP08(lo, 2, 4, 5, 3, 7, 6, 1, 0) ^ 0x80;
	}

	return lo;
}

static INT32 SpacfuryInit()
{
	global_flipy = 1;
	sega_decrypt = sega_decrypt64;
	write_port_cb = spacfury_port_write;

	return DrvInit();
}

struct BurnDriver BurnDrvSpacfury = {
	"spacfury", NULL, NULL, "spacfury", "1981",
	"Space Fury (revision C)\0", NULL, "Sega", "G80 Vector",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_SHOOT, 0,
	NULL, spacfuryRomInfo, spacfuryRomName, NULL, NULL, spacfurySampleInfo, spacfurySampleName, SpacfuryInputInfo, SpacfuryDIPInfo,
	SpacfuryInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	800, 600, 4, 3
};


// Space Fury (revision A)

static struct BurnRomInfo spacfuryaRomDesc[] = {
	{ "969a.cpu-u25",		0x0800, 0x896a615c, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "960a.prom-u1",		0x0800, 0xe1ea7964, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "961a.prom-u2",		0x0800, 0xcdb04233, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "962a.prom-u3",		0x0800, 0x5f03e632, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "963a.prom-u4",		0x0800, 0x45a77b44, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "964a.prom-u5",		0x0800, 0xba008f8b, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "965a.prom-u6",		0x0800, 0x78677d31, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "966a.prom-u7",		0x0800, 0xa8a51105, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "967a.prom-u8",		0x0800, 0xd60f667d, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "968a.prom-u9",		0x0800, 0xaea85b6a, 1 | BRF_PRG | BRF_ESS }, //  9

	{ "808a.speech-u7",		0x0800, 0x5988c767, 2 | BRF_PRG | BRF_ESS }, // 10 I8035 Code

	{ "970.speech-u6",		0x1000, 0xf3b47b36, 3 | BRF_GRA },           // 11 Speech Data
	{ "971.speech-u5",		0x1000, 0xe72bbe88, 3 | BRF_GRA },           // 12
	{ "972.speech-u4",		0x1000, 0x8b3da539, 3 | BRF_GRA },           // 13

	{ "s-c.xyt-u39",		0x0400, 0x56484d19, 4 | BRF_GRA },           // 14 Sine Table

	{ "pr-82.cpu-u15",		0x0020, 0xc609b79e, 0 | BRF_OPT },           // 15 Address PROMs
	{ "6331.speech-u30",	0x0020, 0xadcb81d0, 0 | BRF_OPT },           // 16
};

STD_ROM_PICK(spacfurya)
STD_ROM_FN(spacfurya)

struct BurnDriver BurnDrvSpacfurya = {
	"spacfurya", "spacfury", NULL, "spacfury", "1981",
	"Space Fury (revision A)\0", NULL, "Sega", "G80 Vector",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MISC, GBF_SHOOT, 0,
	NULL, spacfuryaRomInfo, spacfuryaRomName, NULL, NULL, spacfurySampleInfo, spacfurySampleName, SpacfuryInputInfo, SpacfuryDIPInfo,
	SpacfuryInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	800, 600, 4, 3
};


// Space Fury (revision B)

static struct BurnRomInfo spacfurybRomDesc[] = {
	{ "969a.cpu-u25",		0x0800, 0x896a615c, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "960b.prom-u1",		0x0800, 0x8a99b63f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "961b.prom-u2",		0x0800, 0xc72c1609, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "962b.prom-u3",		0x0800, 0x7ffc338d, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "963b.prom-u4",		0x0800, 0x4fe0bd88, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "964b.prom-u5",		0x0800, 0x09b359db, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "965b.prom-u6",		0x0800, 0x7c1f9b71, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "966b.prom-u7",		0x0800, 0x8933b852, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "967b.prom-u8",		0x0800, 0x82b5768d, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "968b.prom-u9",		0x0800, 0xfea68f02, 1 | BRF_PRG | BRF_ESS }, //  9

	{ "808a.speech-u7",		0x0800, 0x5988c767, 2 | BRF_PRG | BRF_ESS }, // 10 I8035 Code

	{ "970.speech-u6",		0x1000, 0xf3b47b36, 3 | BRF_GRA },           // 11 Speech Data
	{ "971.speech-u5",		0x1000, 0xe72bbe88, 3 | BRF_GRA },           // 12
	{ "972.speech-u4",		0x1000, 0x8b3da539, 3 | BRF_GRA },           // 13

	{ "s-c.xyt-u39",		0x0400, 0x56484d19, 4 | BRF_GRA },           // 14 Sine Table

	{ "pr-82.cpu-u15",		0x0020, 0xc609b79e, 0 | BRF_OPT },           // 15 Address PROMs
	{ "6331.speech-u30",	0x0020, 0xadcb81d0, 0 | BRF_OPT },           // 16
};

STD_ROM_PICK(spacfuryb)
STD_ROM_FN(spacfuryb)

struct BurnDriver BurnDrvSpacfuryb = {
	"spacfuryb", "spacfury", NULL, "spacfury", "1981",
	"Space Fury (revision B)\0", NULL, "Sega", "G80 Vector",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MISC, GBF_SHOOT, 0,
	NULL, spacfurybRomInfo, spacfurybRomName, NULL, NULL, spacfurySampleInfo, spacfurySampleName, SpacfuryInputInfo, SpacfuryDIPInfo,
	SpacfuryInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	800, 600, 4, 3
};


// Zektor (revision B)

static struct BurnRomInfo zektorRomDesc[] = {
	{ "1611.cpu-u25",		0x0800, 0x6245aa23, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "1586.prom-u1",		0x0800, 0xefeb4fb5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "1587.prom-u2",		0x0800, 0xdaa6c25c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "1588.prom-u3",		0x0800, 0x62b67dde, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "1589.prom-u4",		0x0800, 0xc2db0ba4, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "1590.prom-u5",		0x0800, 0x4d948414, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "1591.prom-u6",		0x0800, 0xb0556a6c, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "1592.prom-u7",		0x0800, 0x750ecadf, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "1593.prom-u8",		0x0800, 0x34f8850f, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "1594.prom-u9",		0x0800, 0x52b22ab2, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "1595.prom-u10",		0x0800, 0xa704d142, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "1596.prom-u11",		0x0800, 0x6975e33d, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "1597.prom-u12",		0x0800, 0xd48ab5c2, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "1598.prom-u13",		0x0800, 0xab54a94c, 1 | BRF_PRG | BRF_ESS }, // 13
	{ "1599.prom-u14",		0x0800, 0xc9d4f3a5, 1 | BRF_PRG | BRF_ESS }, // 14
	{ "1600.prom-u15",		0x0800, 0x893b7dbc, 1 | BRF_PRG | BRF_ESS }, // 15
	{ "1601.prom-u16",		0x0800, 0x867bdf4f, 1 | BRF_PRG | BRF_ESS }, // 16
	{ "1602.prom-u17",		0x0800, 0xbd447623, 1 | BRF_PRG | BRF_ESS }, // 17
	{ "1603.prom-u18",		0x0800, 0x9f8f10e8, 1 | BRF_PRG | BRF_ESS }, // 18
	{ "1604.prom-u19",		0x0800, 0xad2f0f6c, 1 | BRF_PRG | BRF_ESS }, // 19
	{ "1605.prom-u20",		0x0800, 0xe27d7144, 1 | BRF_PRG | BRF_ESS }, // 20
	{ "1606.prom-u21",		0x0800, 0x7965f636, 1 | BRF_PRG | BRF_ESS }, // 21

	{ "1607.speech-u7",		0x0800, 0xb779884b, 2 | BRF_PRG | BRF_ESS }, // 22 I8035 Code

	{ "1608.speech-u6",		0x1000, 0x637e2b13, 3 | BRF_SND },           // 23 Speech Data
	{ "1609.speech-u5",		0x1000, 0x675ee8e5, 3 | BRF_SND },           // 24
	{ "1610.speech-u4",		0x1000, 0x2915c7bd, 3 | BRF_SND },           // 25

	{ "s-c.xyt-u39",		0x0400, 0x56484d19, 4 | BRF_GRA },           // 26 Sine Tables

	{ "pr-82.cpu-u15",		0x0020, 0xc609b79e, 0 | BRF_OPT },           // 27 Addressing PROMs
	{ "6331.speech-u30",	0x0020, 0xadcb81d0, 0 | BRF_OPT },           // 28
};

STD_ROM_PICK(zektor)
STD_ROM_FN(zektor)

static struct BurnSampleInfo zektorSampleDesc[] = {
	{"elim1",		SAMPLE_NOLOOP },  //  0 fireball
	{"elim2",		SAMPLE_NOLOOP },  //  1 bounce
	{"elim3",		SAMPLE_NOLOOP },  //  2 Skitter
	{"elim4",		SAMPLE_NOLOOP },  //  3 Eliminator
	{"elim5",		SAMPLE_NOLOOP },  //  4 Electron
	{"elim6",		SAMPLE_NOLOOP },  //  5 fire
	{"elim7",		SAMPLE_NOLOOP },  //  6 thrust
	{"elim8",		SAMPLE_NOLOOP },  //  7 Electron
	{"elim9",		SAMPLE_NOLOOP },  //  8 small explosion
	{"elim10",		SAMPLE_NOLOOP },  //  9 med explosion
	{"elim11",		SAMPLE_NOLOOP },  // 10 big explosion
	{"",            0             }
};

STD_SAMPLE_PICK(zektor)
STD_SAMPLE_FN(zektor)

static void zektor_sh1_write(UINT8 data)
{
	if (data & 0x02) BurnSamplePlay(0);
	if (data & 0x04) BurnSamplePlay(10);
	if (data & 0x08) BurnSamplePlay(9);
	if (data & 0x10) BurnSamplePlay(8);

	if (data & 0x20)
	{
		if (BurnSampleGetStatus(1))
			BurnSampleStop(1);
		BurnSamplePlay(1);
	}

	if (data & 0xc0)
	{
		if (BurnSampleGetStatus(5))
			BurnSampleStop(5);
		BurnSamplePlay(5);
	}
}

static void zektor_sh2_write(UINT8 data)
{
	if (data & 0x0f)
		BurnSamplePlay(6);
	else
		BurnSampleStop(6);

	if (data & 0x10) BurnSamplePlay(2);
	if (data & 0x20) BurnSamplePlay(3);
	//if (data & 0x40) BurnSamplePlay(40); // iq_132??
	//if (data & 0x80) BurnSamplePlay(41);
}

static void zektor_port_write(UINT8 port, UINT8 data)
{
	switch (port)
	{
		case 0x38:
			sega_speech_data_write(data);
		return;

		case 0x3b:
			// speech_sound control_w (not used for emulation)
		return;

		case 0x3c:
		case 0x3d:
			AY8910Write(0, port & 1, data);
		return;

		case 0x3e:
			zektor_sh1_write(data ^ 0xff);
		return;

		case 0x3f:
			zektor_sh2_write(data ^ 0xff);
		return;
	}
}
static UINT8 sega_decrypt82(UINT16 pc, UINT8 lo)
{
	switch (pc & 0x11)
	{
		case 0x00:
			return lo;

		case 0x01:
			return BITSWAP08(lo, 2, 7, 3, 4, 6, 5, 1, 0) ^ 0x10;

		case 0x10:
			return BITSWAP08(lo, 2, 3, 6, 5, 7, 4, 1, 0) ^ 0x20;

		case 0x11:
			return BITSWAP08(lo, 2, 4, 5, 3, 7, 6, 1, 0) ^ 0x80;
	}

	return lo;
}

static INT32 ZektorInit()
{
	global_flipy = 1;
	sega_decrypt = sega_decrypt82;
	write_port_cb = zektor_port_write;

	return DrvInit();
}

struct BurnDriver BurnDrvZektor = {
	"zektor", NULL, NULL, "zektor", "1982",
	"Zektor (revision B)\0", NULL, "Sega", "G80 Vector",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_SHOOT, 0,
	NULL, zektorRomInfo, zektorRomName, NULL, NULL, zektorSampleInfo, zektorSampleName, ZektorInputInfo, ZektorDIPInfo,
	ZektorInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	800, 600, 4, 3
};


// Tac/Scan

static struct BurnRomInfo tacscanRomDesc[] = {
	{ "1711a.cpu-u25",		0x0800, 0x0da13158, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "1670c.prom-u1",		0x0800, 0x98de6fd5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "1671a.prom-u2",		0x0800, 0xdc400074, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "1672a.prom-u3",		0x0800, 0x2caf6f7e, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "1673a.prom-u4",		0x0800, 0x1495ce3d, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "1674a.prom-u5",		0x0800, 0xab7fc5d9, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "1675a.prom-u6",		0x0800, 0xcf5e5016, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "1676a.prom-u7",		0x0800, 0xb61a3ab3, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "1677a.prom-u8",		0x0800, 0xbc0273b1, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "1678b.prom-u9",		0x0800, 0x7894da98, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "1679a.prom-u10",		0x0800, 0xdb865654, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "1680a.prom-u11",		0x0800, 0x2c2454de, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "1681a.prom-u12",		0x0800, 0x77028885, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "1682a.prom-u13",		0x0800, 0xbabe5cf1, 1 | BRF_PRG | BRF_ESS }, // 13
	{ "1683a.prom-u14",		0x0800, 0x1b98b618, 1 | BRF_PRG | BRF_ESS }, // 14
	{ "1684a.prom-u15",		0x0800, 0xcb3ded3b, 1 | BRF_PRG | BRF_ESS }, // 15
	{ "1685a.prom-u16",		0x0800, 0x43016a79, 1 | BRF_PRG | BRF_ESS }, // 16
	{ "1686a.prom-u17",		0x0800, 0xa4397772, 1 | BRF_PRG | BRF_ESS }, // 17
	{ "1687a.prom-u18",		0x0800, 0x002f3bc4, 1 | BRF_PRG | BRF_ESS }, // 18
	{ "1688a.prom-u19",		0x0800, 0x0326d87a, 1 | BRF_PRG | BRF_ESS }, // 19
	{ "1709a.prom-u20",		0x0800, 0xf35ed1ec, 1 | BRF_PRG | BRF_ESS }, // 20
	{ "1710a.prom-u21",		0x0800, 0x6203be22, 1 | BRF_PRG | BRF_ESS }, // 21

	{ "s-c.xyt-u39",		0x0400, 0x56484d19, 4 | BRF_GRA },           // 22 Sine Tables

	{ "pr-82.cpu-u15",		0x0020, 0xc609b79e, 0 | BRF_OPT },           // 23 Addressing PROM
};

STD_ROM_PICK(tacscan)
STD_ROM_FN(tacscan)

static void tacscan_port_write(UINT8 port, UINT8 data)
{
	switch (port)
	{
		case 0x3f:
			usb_sound_data_write(data);
		return;
	}
}

static UINT8 tacscan_port_read(UINT8 port)
{
	switch (port)
	{
		case 0x3f:
			return usb_sound_status_read();
	}

	return 0;
}

static INT32 TacscanInit()
{
	has_usb = 1;
	dialmode = 1;
	global_flipx = 1;
	sega_decrypt = sega_decrypt76;
	write_port_cb = tacscan_port_write;
	read_port_cb = tacscan_port_read;

	return DrvInit();
}

struct BurnDriver BurnDrvTacscan = {
	"tacscan", NULL, NULL, NULL, "1982",
	"Tac/Scan\0", NULL, "Sega", "G80 Vector",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_SEGA_MISC, GBF_VERSHOOT, 0,
	NULL, tacscanRomInfo, tacscanRomName, NULL, NULL, NULL, NULL, TacscanInputInfo, TacscanDIPInfo,
	TacscanInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	600, 800, 3, 4
};


// Star Trek

static struct BurnRomInfo startrekRomDesc[] = {
	{ "1873.cpu-u25",		0x0800, 0xbe46f5d9, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "1848.prom-u1",		0x0800, 0x65e3baf3, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "1849.prom-u2",		0x0800, 0x8169fd3d, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "1850.prom-u3",		0x0800, 0x78fd68dc, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "1851.prom-u4",		0x0800, 0x3f55ab86, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "1852.prom-u5",		0x0800, 0x2542ecfb, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "1853.prom-u6",		0x0800, 0x75c2526a, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "1854.prom-u7",		0x0800, 0x096d75d0, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "1855.prom-u8",		0x0800, 0xbc7b9a12, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "1856.prom-u9",		0x0800, 0xed9fe2fb, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "1857.prom-u10",		0x0800, 0x28699d45, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "1858.prom-u11",		0x0800, 0x3a7593cb, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "1859.prom-u12",		0x0800, 0x5b11886b, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "1860.prom-u13",		0x0800, 0x62eb96e6, 1 | BRF_PRG | BRF_ESS }, // 13
	{ "1861.prom-u14",		0x0800, 0x99852d1d, 1 | BRF_PRG | BRF_ESS }, // 14
	{ "1862.prom-u15",		0x0800, 0x76ce27b2, 1 | BRF_PRG | BRF_ESS }, // 15
	{ "1863.prom-u16",		0x0800, 0xdd92d187, 1 | BRF_PRG | BRF_ESS }, // 16
	{ "1864.prom-u17",		0x0800, 0xe37d3a1e, 1 | BRF_PRG | BRF_ESS }, // 17
	{ "1865.prom-u18",		0x0800, 0xb2ec8125, 1 | BRF_PRG | BRF_ESS }, // 18
	{ "1866.prom-u19",		0x0800, 0x6f188354, 1 | BRF_PRG | BRF_ESS }, // 19
	{ "1867.prom-u20",		0x0800, 0xb0a3eae8, 1 | BRF_PRG | BRF_ESS }, // 20
	{ "1868.prom-u21",		0x0800, 0x8b4e2e07, 1 | BRF_PRG | BRF_ESS }, // 21
	{ "1869.prom-u22",		0x0800, 0xe5663070, 1 | BRF_PRG | BRF_ESS }, // 22
	{ "1870.prom-u23",		0x0800, 0x4340616d, 1 | BRF_PRG | BRF_ESS }, // 23

	{ "1670.speech-u7",		0x0800, 0xb779884b, 2 | BRF_PRG | BRF_ESS }, // 24 I8035 Code

	{ "1871.speech-u6",		0x1000, 0x03713920, 3 | BRF_GRA },           // 25 Speech Data
	{ "1872.speech-u5",		0x1000, 0xebb5c3a9, 3 | BRF_GRA },           // 26

	{ "s-c.xyt-u39",		0x0400, 0x56484d19, 4 | BRF_GRA },           // 27 Sine Tables

	{ "pr-82.cpu-u15",		0x0020, 0xc609b79e, 0 | BRF_OPT },           // 28 Addressing PROMs
	{ "6331.speech-u30",	0x0020, 0xadcb81d0, 0 | BRF_OPT },           // 29
};

STD_ROM_PICK(startrek)
STD_ROM_FN(startrek)

static void startrek_port_write(UINT8 port, UINT8 data)
{
	switch (port)
	{
		case 0x38:
			sega_speech_data_write(data);
		return;

		case 0x3b:
			// speech_sound_device control_w (not used for emulation)
		return;

		case 0x3f:
			usb_sound_data_write(data);
		return;
	}
}

static UINT8 startrek_port_read(UINT8 port)
{
	switch (port)
	{
		case 0x3f:
			return usb_sound_status_read();
	}

	return 0;
}

static INT32 StartrekInit()
{
	has_usb = 1;
	dialmode = 1;
	global_flipy = 1;
	sega_decrypt = sega_decrypt64;
	write_port_cb = startrek_port_write;
	read_port_cb = startrek_port_read;

	return DrvInit();
}

struct BurnDriver BurnDrvStartrek = {
	"startrek", NULL, NULL, NULL, "1982",
	"Star Trek\0", NULL, "Sega", "G80 Vector",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_SHOOT, 0,
	NULL, startrekRomInfo, startrekRomName, NULL, NULL, NULL, NULL, StartrekInputInfo, StartrekDIPInfo,
	StartrekInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	800, 600, 4, 3
};
