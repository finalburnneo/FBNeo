// FB Neo Taito SJ system driver module
// Based on MAME driver by Nicola Salmoria

#include "tiles_generic.h"
#include "z80_intf.h"
#include "m6805_intf.h"
#include "watchdog.h"
#include "ay8910.h"
#include "dac.h"
#include "math.h" // abs()

static UINT8 *AllMem;
static UINT8 *RamEnd;
static UINT8 *AllRam;
static UINT8 *MemEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvMCUROM;
static UINT8 *DrvGfxExp;
static UINT8 *DrvSprExp;
static UINT8 *DrvGfxROM;
static UINT8 *DrvZ80RAMA;
static UINT8 *DrvZ80RAMB;
static UINT8 *DrvZ80RAMC;
static UINT8 *DrvVidRAM1;
static UINT8 *DrvVidRAM2;
static UINT8 *DrvVidRAM3;
static UINT8 *DrvColScroll;
static UINT8 *DrvSprRAM;
static UINT8 *DrvCharRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvMCURAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT16 *bitmap[9];

static UINT8 video_priority;
static UINT8 scroll[8];
static UINT8 color_bank[2];
static UINT8 gfxpointer[2];
static UINT8 soundlatch;
static UINT8 video_mode;
static UINT8 collision_reg[4];
static UINT8 rom_bank;
static UINT8 sound_nmi_disable;
static UINT8 input_port_data;
static UINT8 protection_value;
static UINT8 dac_volume;
static INT8 dac_out_data;

static INT32 sound_irq_timer;

static UINT8 toz80;
static UINT8 fromz80;
static UINT16 mcu_address;
static UINT8 portA_in;
static UINT8 portA_out;
static UINT8 zready;
static UINT8 zaccept;
static UINT8 busreq;

static INT32 spriteram_bank;
static INT32 global_flipx;
static INT32 global_flipy;
static INT32 draw_order[32][4];
static INT32 coin_state = 0;
static UINT8 charram_xor = 0; // junglhbr
static INT32 input2_xor = 0;
static INT32 has_mcu = 0;
static INT32 is_alpine = 0;
static INT32 is_kikstart = 0;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvJoy5[8];
static UINT8 DrvJoyF0[8];
static UINT8 DrvJoyF1[8];
static UINT8 DrvDips[4];
static UINT8 DrvReset;
static UINT8 DrvInputs[8];
static UINT8 kikstart_gears[2];

static INT32 nCyclesExtra[3];

static struct BurnInputInfo TwoButtonInputList[] = {
	{"P1 Coin",				BIT_DIGITAL,	DrvJoy3 + 5,	"p1 coin"	},
	{"P1 Start",			BIT_DIGITAL,	DrvJoy3 + 6,	"p1 start"	},
	{"P1 Up",				BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",				BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",				BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",				BIT_DIGITAL,	DrvJoy3 + 4,	"p2 coin"	},
	{"P2 Start",			BIT_DIGITAL,	DrvJoy3 + 7,	"p2 start"	},
	{"P2 Up",				BIT_DIGITAL,	DrvJoy2 + 3,	"p2 up"		},
	{"P2 Down",				BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"	},
	{"P2 Left",				BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Tilt",				BIT_DIGITAL,	DrvJoy4 + 5,	"tilt"		},
	{"Dip A",				BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",				BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",				BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(TwoButton)

static struct BurnInputInfo TwoButtonLRInputList[] = {
	{"P1 Coin",				BIT_DIGITAL,	DrvJoy3 + 5,	"p1 coin"	},
	{"P1 Start",			BIT_DIGITAL,	DrvJoy3 + 6,	"p1 start"	},
	{"P1 Left",				BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",				BIT_DIGITAL,	DrvJoy3 + 4,	"p2 coin"	},
	{"P2 Start",			BIT_DIGITAL,	DrvJoy3 + 7,	"p2 start"	},
	{"P2 Left",				BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Tilt",				BIT_DIGITAL,	DrvJoy4 + 5,	"tilt"		},
	{"Dip A",				BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",				BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",				BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(TwoButtonLR)

static struct BurnInputInfo OneButtonInputList[] = {
	{"P1 Coin",				BIT_DIGITAL,	DrvJoy3 + 5,	"p1 coin"	},
	{"P1 Start",			BIT_DIGITAL,	DrvJoy3 + 6,	"p1 start"	},
	{"P1 Up",				BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",				BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",				BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"P2 Coin",				BIT_DIGITAL,	DrvJoy3 + 4,	"p2 coin"	},
	{"P2 Start",			BIT_DIGITAL,	DrvJoy3 + 7,	"p2 start"	},
	{"P2 Up",				BIT_DIGITAL,	DrvJoy2 + 3,	"p2 up"		},
	{"P2 Down",				BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"	},
	{"P2 Left",				BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Tilt",				BIT_DIGITAL,	DrvJoy4 + 5,	"tilt"		},
	{"Dip A",				BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",				BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",				BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(OneButton)

static struct BurnInputInfo AlpineInputList[] = {
	{"P1 Coin",				BIT_DIGITAL,	DrvJoy3 + 5,	"p1 coin"	},
	{"P1 Start",			BIT_DIGITAL,	DrvJoy3 + 6,	"p1 start"	},
	{"P1 Left",				BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",			BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},

	{"P2 Coin",				BIT_DIGITAL,	DrvJoy3 + 0,	"p2 coin"	},
	{"P2 Start",			BIT_DIGITAL,	DrvJoy3 + 7,	"p2 start"	},
	{"P2 Left",				BIT_DIGITAL,	DrvJoy1 + 0,	"p2 left"	},
	{"P2 Right",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 right"	},
	{"P2 Button 1",			BIT_DIGITAL,	DrvJoy1 + 4,	"p2 fire 1"	},

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Tilt",				BIT_DIGITAL,	DrvJoy4 + 5,	"tilt"		},
	{"Dip A",				BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",				BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",				BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Alpine)

static struct BurnInputInfo TimetunlInputList[] = {
	{"P1 Coin",				BIT_DIGITAL,	DrvJoy3 + 5,	"p1 coin"	},
	{"P1 Start",			BIT_DIGITAL,	DrvJoy3 + 6,	"p1 start"	},
	{"P1 Up",				BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",				BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Button 1",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"P2 Coin",				BIT_DIGITAL,	DrvJoy3 + 4,	"p2 coin"	},
	{"P2 Start",			BIT_DIGITAL,	DrvJoy3 + 7,	"p2 start"	},
	{"P2 Up",				BIT_DIGITAL,	DrvJoy2 + 3,	"p2 up"		},
	{"P2 Down",				BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"	},
	{"P2 Button 1",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Tilt",				BIT_DIGITAL,	DrvJoy4 + 5,	"tilt"		},
	{"Dip A",				BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",				BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",				BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Timetunl)

static struct BurnInputInfo DualStickInputList[] = {
	{"P1 Coin",				BIT_DIGITAL,	DrvJoy3 + 5,	"p1 coin"	},
	{"P1 Start",			BIT_DIGITAL,	DrvJoy3 + 6,	"p1 start"	},
	{"P1 Leftstick Up",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Leftstick Down",	BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Leftstick Left",	BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Leftstick Right",	BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Rightstick Up",	BIT_DIGITAL,	DrvJoy4 + 3,	"p1 up 2"	},
	{"P1 Rightstick Down",	BIT_DIGITAL,	DrvJoy4 + 2,	"p1 down 2"	},
	{"P1 Rightstick Left",	BIT_DIGITAL,	DrvJoy4 + 0,	"p1 left 2"	},
	{"P1 Rightstick Right",	BIT_DIGITAL,	DrvJoy4 + 1,	"p1 right 2"},
	{"P1 Button 1",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",				BIT_DIGITAL,	DrvJoy3 + 0,	"p2 coin"	},
	{"P2 Start",			BIT_DIGITAL,	DrvJoy3 + 7,	"p2 start"	},
	{"P2 Leftstick Up",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 up"		},
	{"P2 Leftstick Down",	BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"	},
	{"P2 Leftstick Left",	BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Leftstick Right",	BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Rightstick Up",	BIT_DIGITAL,	DrvJoy5 + 3,	"p2 up 2"	},
	{"P2 Rightstick Down",	BIT_DIGITAL,	DrvJoy5 + 2,	"p2 down 2"	},
	{"P2 Rightstick Left",	BIT_DIGITAL,	DrvJoy5 + 0,	"p2 left 2"	},
	{"P2 Rightstick Right",	BIT_DIGITAL,	DrvJoy5 + 1,	"p2 right 2"},
	{"P2 Button 1",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"P3 Coin",				BIT_DIGITAL,	DrvJoy4 + 4,	"p3 coin"	},

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Tilt",				BIT_DIGITAL,	DrvJoy4 + 5,	"tilt"		},
	{"Dip A",				BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",				BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",				BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(DualStick)

static struct BurnInputInfo KikstartInputList[] = {
	{"P1 Coin",		        BIT_DIGITAL,	DrvJoy3 + 5,	"p1 coin"	},
	{"P1 Start",		    BIT_DIGITAL,	DrvJoy3 + 6,	"p1 start"	},
	{"P1 Left",		        BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		    BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		    BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2 (Gear Down)",BIT_DIGITAL,	DrvJoyF0 + 0,	"p1 fire 2"	},
	{"P1 Button 3 (Gear Up)",BIT_DIGITAL,	DrvJoyF0 + 1,	"p1 fire 3"	},

	{"P2 Coin",		        BIT_DIGITAL,	DrvJoy3 + 4,	"p2 coin"	},
	{"P2 Start",		    BIT_DIGITAL,	DrvJoy3 + 7,	"p2 start"	},
	{"P2 Left",		        BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		    BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		    BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2 (Gear Down)",BIT_DIGITAL,	DrvJoyF1 + 0,	"p2 fire 2"	},
	{"P2 Button 3 (Gear Up)",BIT_DIGITAL,	DrvJoyF1 + 1,	"p2 fire 3"	},

	{"Reset",		        BIT_DIGITAL,	&DrvReset,	    "reset"		},
	{"Service",		        BIT_DIGITAL,	DrvJoy4 + 4,	"service"	},
	{"Tilt",		        BIT_DIGITAL,	DrvJoy4 + 5,	"tilt"		},
	{"Dip A",		        BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		        BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",				BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Kikstart)
	
static struct BurnInputInfo SpacecrInputList[] = {
	{"P1 Coin",				BIT_DIGITAL,	DrvJoy3 + 5,	"p1 coin"	},
	{"P1 Start",			BIT_DIGITAL,	DrvJoy3 + 6,	"p1 start"	},
	{"P1 Left",				BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Continue",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2" },

	{"P2 Coin",				BIT_DIGITAL,	DrvJoy3 + 4,	"p2 coin"	},
	{"P2 Start",			BIT_DIGITAL,	DrvJoy3 + 7,	"p2 start"	},
	{"P2 Left",				BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Continue",         BIT_DIGITAL,    DrvJoy2 + 5,    "p2 fire 2" },

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Tilt",				BIT_DIGITAL,	DrvJoy4 + 5,	"tilt"		},
	{"Dip A",				BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",				BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",				BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Spacecr)

#define COMMON_COIN_DIPS(offs)							\
	{0   , 0xfe, 0   ,   16, "Coin A"				},	\
	{offs, 0x01, 0x0f, 0x0f, "9 Coins 1 Credits"	},	\
	{offs, 0x01, 0x0f, 0x0e, "8 Coins 1 Credits"	},	\
	{offs, 0x01, 0x0f, 0x0d, "7 Coins 1 Credits"	},	\
	{offs, 0x01, 0x0f, 0x0c, "6 Coins 1 Credits"	},	\
	{offs, 0x01, 0x0f, 0x0b, "5 Coins 1 Credits"	},	\
	{offs, 0x01, 0x0f, 0x0a, "4 Coins 1 Credits"	},	\
	{offs, 0x01, 0x0f, 0x09, "3 Coins 1 Credits"	},	\
	{offs, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"	},	\
	{offs, 0x01, 0x0f, 0x00, "1 Coin  1 Credits"	},	\
	{offs, 0x01, 0x0f, 0x01, "1 Coin  2 Credits"	},	\
	{offs, 0x01, 0x0f, 0x02, "1 Coin  3 Credits"	},	\
	{offs, 0x01, 0x0f, 0x03, "1 Coin  4 Credits"	},	\
	{offs, 0x01, 0x0f, 0x04, "1 Coin  5 Credits"	},	\
	{offs, 0x01, 0x0f, 0x05, "1 Coin  6 Credits"	},	\
	{offs, 0x01, 0x0f, 0x06, "1 Coin  7 Credits"	},	\
	{offs, 0x01, 0x0f, 0x07, "1 Coin  8 Credits"	},	\
	{0   , 0xfe, 0   ,   16, "Coin B"				},	\
	{offs, 0x01, 0xf0, 0xf0, "9 Coins 1 Credits"	},	\
	{offs, 0x01, 0xf0, 0xe0, "8 Coins 1 Credits"	},	\
	{offs, 0x01, 0xf0, 0xd0, "7 Coins 1 Credits"	},	\
	{offs, 0x01, 0xf0, 0xc0, "6 Coins 1 Credits"	},	\
	{offs, 0x01, 0xf0, 0xb0, "5 Coins 1 Credits"	},	\
	{offs, 0x01, 0xf0, 0xa0, "4 Coins 1 Credits"	},	\
	{offs, 0x01, 0xf0, 0x90, "3 Coins 1 Credits"	},	\
	{offs, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"	},	\
	{offs, 0x01, 0xf0, 0x00, "1 Coin  1 Credits"	},	\
	{offs, 0x01, 0xf0, 0x10, "1 Coin  2 Credits"	},	\
	{offs, 0x01, 0xf0, 0x20, "1 Coin  3 Credits"	},	\
	{offs, 0x01, 0xf0, 0x30, "1 Coin  4 Credits"	},	\
	{offs, 0x01, 0xf0, 0x40, "1 Coin  5 Credits"	},	\
	{offs, 0x01, 0xf0, 0x50, "1 Coin  6 Credits"	},	\
	{offs, 0x01, 0xf0, 0x60, "1 Coin  7 Credits"	},	\
	{offs, 0x01, 0xf0, 0x70, "1 Coin  8 Credits"	},

static struct BurnDIPInfo SpaceskrDIPList[]=
{
	{0x12, 0xff, 0xff, 0xe7, NULL					},
	{0x13, 0xff, 0xff, 0x00, NULL					},
	{0x14, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x12, 0x01, 0x18, 0x00, "3"					},
	{0x12, 0x01, 0x18, 0x08, "4"					},
	{0x12, 0x01, 0x18, 0x10, "5"					},
	{0x12, 0x01, 0x18, 0x18, "6"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x12, 0x01, 0x20, 0x20, "Off"					},
	{0x12, 0x01, 0x20, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x12, 0x01, 0x80, 0x80, "Upright"				},
	{0x12, 0x01, 0x80, 0x00, "Cocktail"				},

	COMMON_COIN_DIPS(0x13)

	{0   , 0xfe, 0   ,    2, "Year Display"			},
	{0x14, 0x01, 0x20, 0x00, "No"					},
	{0x14, 0x01, 0x20, 0x20, "Yes"					},

	{0   , 0xfe, 0   ,    2, "Invulnerability"		},
	{0x14, 0x01, 0x40, 0x40, "Off"					},
	{0x14, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Coinage"				},
	{0x14, 0x01, 0x80, 0x80, "A and B"				},
	{0x14, 0x01, 0x80, 0x00, "A only"				},
};

STDDIPINFO(Spaceskr)

static struct BurnDIPInfo SpacecrDIPList[]=
{
	{0x0e, 0xff, 0xff, 0x3f, NULL					},
	{0x0f, 0xff, 0xff, 0x00, NULL					},
	{0x10, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x0e, 0x01, 0x18, 0x00, "6"					},
	{0x0e, 0x01, 0x18, 0x08, "5"					},
	{0x0e, 0x01, 0x18, 0x10, "4"					},
	{0x0e, 0x01, 0x18, 0x18, "3"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x0e, 0x01, 0x40, 0x00, "Upright"				},
	{0x0e, 0x01, 0x40, 0x40, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x0e, 0x01, 0x80, 0x00, "Off"					},
	{0x0e, 0x01, 0x80, 0x80, "On"					},

	COMMON_COIN_DIPS(0x0f)

	{0   , 0xfe, 0   ,    2, "Year Display"			},
	{0x10, 0x01, 0x20, 0x00, "No"					},
	{0x10, 0x01, 0x20, 0x20, "Yes"					},

	{0   , 0xfe, 0   ,    2, "Infinite Lives"		},
	{0x10, 0x01, 0x40, 0x40, "Off"					},
	{0x10, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Coin Slots"			},
	{0x10, 0x01, 0x80, 0x80, "A and B"				},
	{0x10, 0x01, 0x80, 0x00, "A only"				},
};

STDDIPINFO(Spacecr)

static struct BurnDIPInfo KikstartDIPList[]=
{
	{0x11, 0xff, 0xff, 0x75, NULL				    },
	{0x12, 0xff, 0xff, 0x00, NULL					},
	{0x13, 0xff, 0xff, 0x78, NULL				    },

	{0   , 0xfe, 0   ,    4, "Gate Bonus"			},
	{0x11, 0x01, 0x03, 0x00, "5k Points"			},
	{0x11, 0x01, 0x03, 0x01, "10k Points"			},
	{0x11, 0x01, 0x03, 0x02, "15k Points"			},
	{0x11, 0x01, 0x03, 0x03, "20k Points"			},

	{0   , 0xfe, 0   ,    2, "Free Play"			},
	{0x11, 0x01, 0x04, 0x04, "Off"				    },
	{0x11, 0x01, 0x04, 0x00, "On"				    },

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x11, 0x01, 0x18, 0x18, "Easy"				    },
	{0x11, 0x01, 0x18, 0x10, "Normal"			    },
	{0x11, 0x01, 0x18, 0x08, "Difficult"			},
	{0x11, 0x01, 0x18, 0x00, "Very Difficult"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x11, 0x01, 0x20, 0x20, "Off"					},
	{0x11, 0x01, 0x20, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x11, 0x01, 0x40, 0x40, "Off"				    },
	{0x11, 0x01, 0x40, 0x00, "On"				    },

	{0   , 0xfe, 0   ,    2, "Cabinet"			    },
	{0x11, 0x01, 0x80, 0x00, "Upright"			    },
	{0x11, 0x01, 0x80, 0x80, "Cocktail"			    },

	COMMON_COIN_DIPS(0x12)

	{0   , 0xfe, 0   ,    2, "Control Type"			},
	{0x13, 0x01, 0x08, 0x08, "Revolve"			    },
	{0x13, 0x01, 0x08, 0x00, "Buttons"			    },

	{0   , 0xfe, 0   ,    2, "Coinage Display"		},
	{0x13, 0x01, 0x10, 0x00, "Off"				    },
	{0x13, 0x01, 0x10, 0x10, "On"				    },

	{0   , 0xfe, 0   ,    2, "Year Display"			},
	{0x13, 0x01, 0x20, 0x00, "No"				    },
	{0x13, 0x01, 0x20, 0x20, "Yes"				    },

	{0   , 0xfe, 0   ,    2, "No Hit (Cheat)"		},
	{0x13, 0x01, 0x40, 0x40, "No"				    },
	{0x13, 0x01, 0x40, 0x00, "Yes"				    },

	{0   , 0xfe, 0   ,    2, "Coinage"			    },
	{0x13, 0x01, 0x80, 0x80, "A and B"			    },
	{0x13, 0x01, 0x80, 0x00, "A only"			    },
};

STDDIPINFO(Kikstart)

static struct BurnDIPInfo JunglekDIPList[]=
{
	{0x10, 0xff, 0xff, 0xff, NULL					},
	{0x11, 0xff, 0xff, 0x00, NULL					},
	{0x12, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    4, "Finish Bonus"			},
	{0x10, 0x01, 0x03, 0x03, "None"					},
	{0x10, 0x01, 0x03, 0x02, "Timer x1"				},
	{0x10, 0x01, 0x03, 0x01, "Timer x2"				},
	{0x10, 0x01, 0x03, 0x00, "Timer x3"				},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x10, 0x01, 0x18, 0x18, "3"					},
	{0x10, 0x01, 0x18, 0x10, "4"					},
	{0x10, 0x01, 0x18, 0x08, "5"					},
	{0x10, 0x01, 0x18, 0x00, "6"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x10, 0x01, 0x40, 0x00, "Off"					},
	{0x10, 0x01, 0x40, 0x40, "On"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x10, 0x01, 0x80, 0x00, "Upright"				},
	{0x10, 0x01, 0x80, 0x80, "Cocktail"				},

	COMMON_COIN_DIPS(0x11)

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x12, 0x01, 0x03, 0x02, "10000"				},
	{0x12, 0x01, 0x03, 0x01, "20000"				},
	{0x12, 0x01, 0x03, 0x00, "30000"				},
	{0x12, 0x01, 0x03, 0x03, "None"					},

	{0   , 0xfe, 0   ,    2, "Year Display"			},
	{0x12, 0x01, 0x20, 0x00, "No"					},
	{0x12, 0x01, 0x20, 0x20, "Yes"					},

	{0   , 0xfe, 0   ,    2, "Hit Detection"		},
	{0x12, 0x01, 0x40, 0x40, "Normal Game"			},
	{0x12, 0x01, 0x40, 0x00, "No Hit"				},

	{0   , 0xfe, 0   ,    2, "Coin Slots"			},
	{0x12, 0x01, 0x80, 0x80, "A and B"				},
	{0x12, 0x01, 0x80, 0x00, "A only"				},
};

STDDIPINFO(Junglek)

static struct BurnDIPInfo PiratpetDIPList[]=
{
	{0x10, 0xff, 0xff, 0xff, NULL					},
	{0x11, 0xff, 0xff, 0x00, NULL					},
	{0x12, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    4, "Finish Bonus"			},
	{0x10, 0x01, 0x03, 0x03, "None"					},
	{0x10, 0x01, 0x03, 0x02, "Timer x1"				},
	{0x10, 0x01, 0x03, 0x01, "Timer x2"				},
	{0x10, 0x01, 0x03, 0x00, "Timer x3"				},

	{0   , 0xfe, 0   ,    2, "Debug Mode"			},
	{0x10, 0x01, 0x04, 0x04, "Off"					},
	{0x10, 0x01, 0x04, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x10, 0x01, 0x18, 0x18, "3"					},
	{0x10, 0x01, 0x18, 0x10, "4"					},
	{0x10, 0x01, 0x18, 0x08, "5"					},
	{0x10, 0x01, 0x18, 0x00, "6"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x10, 0x01, 0x40, 0x00, "Off"					},
	{0x10, 0x01, 0x40, 0x40, "On"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x10, 0x01, 0x80, 0x00, "Upright"				},
	{0x10, 0x01, 0x80, 0x80, "Cocktail"				},

	COMMON_COIN_DIPS(0x11)

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x12, 0x01, 0x03, 0x02, "10000"				},
	{0x12, 0x01, 0x03, 0x01, "20000"				},
	{0x12, 0x01, 0x03, 0x00, "50000"				},
	{0x12, 0x01, 0x03, 0x03, "None"					},

	{0   , 0xfe, 0   ,    7, "Difficulty"			},
	{0x12, 0x01, 0x1c, 0x1c, "Easiest"				},
	{0x12, 0x01, 0x1c, 0x18, "Easier"				},
	{0x12, 0x01, 0x1c, 0x14, "Easy"					},
	{0x12, 0x01, 0x1c, 0x10, "Normal"				},
	{0x12, 0x01, 0x1c, 0x0c, "Medium"				},
	{0x12, 0x01, 0x1c, 0x08, "Hard"					},
	{0x12, 0x01, 0x1c, 0x04, "Harder"				},

	{0   , 0xfe, 0   ,    2, "Year Display"			},
	{0x12, 0x01, 0x20, 0x00, "No"					},
	{0x12, 0x01, 0x20, 0x20, "Yes"					},

	{0   , 0xfe, 0   ,    2, "Infinite Lives"		},
	{0x12, 0x01, 0x40, 0x40, "No"					},
	{0x12, 0x01, 0x40, 0x00, "Yes"					},

	{0   , 0xfe, 0   ,    2, "Coin Slots"			},
	{0x12, 0x01, 0x80, 0x80, "A and B"				},
	{0x12, 0x01, 0x80, 0x00, "A only"				},
};

STDDIPINFO(Piratpet)

static struct BurnDIPInfo AlpineDIPList[]=
{
	{0x0c, 0xff, 0xff, 0xdf, NULL					},
	{0x0d, 0xff, 0xff, 0x00, NULL					},
	{0x0e, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    4, "Jump Bonus"			},
	{0x0c, 0x01, 0x03, 0x00, "500-1500"				},
	{0x0c, 0x01, 0x03, 0x01, "800-2000"				},
	{0x0c, 0x01, 0x03, 0x02, "1000-2500"			},
	{0x0c, 0x01, 0x03, 0x03, "2000-4000"			},

	{0   , 0xfe, 0   ,    4, "Game Time"			},
	{0x0c, 0x01, 0x18, 0x00, "1:00"					},
	{0x0c, 0x01, 0x18, 0x08, "1:30"					},
	{0x0c, 0x01, 0x18, 0x10, "2:00"					},
	{0x0c, 0x01, 0x18, 0x18, "2:30"					},

	{0   , 0xfe, 0   ,    2, "End of Race Time Bonus"	},
	{0x0c, 0x01, 0x20, 0x20, "0:10"					},
	{0x0c, 0x01, 0x20, 0x00, "0:20"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x0c, 0x01, 0x40, 0x40, "Off"					},
	{0x0c, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x0c, 0x01, 0x80, 0x80, "Upright"				},
	{0x0c, 0x01, 0x80, 0x00, "Cocktail"				},

	COMMON_COIN_DIPS(0x0d)

	{0   , 0xfe, 0   ,    4, "1st Extended Time"	},
	{0x0e, 0x01, 0x03, 0x00, "10000"				},
	{0x0e, 0x01, 0x03, 0x01, "15000"				},
	{0x0e, 0x01, 0x03, 0x02, "20000"				},
	{0x0e, 0x01, 0x03, 0x03, "25000"				},

	{0   , 0xfe, 0   ,    8, "Extended Time Every"	},
	{0x0e, 0x01, 0x1c, 0x00, "5000"					},
	{0x0e, 0x01, 0x1c, 0x04, "6000"					},
	{0x0e, 0x01, 0x1c, 0x08, "7000"					},
	{0x0e, 0x01, 0x1c, 0x0c, "8000"					},
	{0x0e, 0x01, 0x1c, 0x10, "9000"					},
	{0x0e, 0x01, 0x1c, 0x14, "10000"				},
	{0x0e, 0x01, 0x1c, 0x18, "11000"				},
	{0x0e, 0x01, 0x1c, 0x1c, "12000"				},

	{0   , 0xfe, 0   ,    2, "Year Display"			},
	{0x0e, 0x01, 0x20, 0x00, "No"					},
	{0x0e, 0x01, 0x20, 0x20, "Yes"					},

	{0   , 0xfe, 0   ,    2, "Hit Detection"		},
	{0x0e, 0x01, 0x40, 0x40, "Normal Game"			},
	{0x0e, 0x01, 0x40, 0x00, "No Hit"				},

	{0   , 0xfe, 0   ,    2, "Coin Slots"			},
	{0x0e, 0x01, 0x80, 0x80, "A and B"				},
	{0x0e, 0x01, 0x80, 0x00, "A only"				},
};

STDDIPINFO(Alpine)

static struct BurnDIPInfo AlpineaDIPList[]=
{
	{0x0c, 0xff, 0xff, 0xff, NULL					},
	{0x0d, 0xff, 0xff, 0x00, NULL					},
	{0x0e, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    4, "Jump Bonus"			},
	{0x0c, 0x01, 0x03, 0x00, "500-1500"				},
	{0x0c, 0x01, 0x03, 0x01, "800-2000"				},
	{0x0c, 0x01, 0x03, 0x02, "1000-2500"			},
	{0x0c, 0x01, 0x03, 0x03, "2000-4000"			},

	{0   , 0xfe, 0   ,    4, "Game Time"			},
	{0x0c, 0x01, 0x18, 0x00, "1:00"					},
	{0x0c, 0x01, 0x18, 0x08, "1:30"					},
	{0x0c, 0x01, 0x18, 0x10, "2:00"					},
	{0x0c, 0x01, 0x18, 0x18, "2:30"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x0c, 0x01, 0x20, 0x20, "Off"					},
	{0x0c, 0x01, 0x20, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x0c, 0x01, 0x40, 0x40, "Off"					},
	{0x0c, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x0c, 0x01, 0x80, 0x80, "Upright"				},
	{0x0c, 0x01, 0x80, 0x00, "Cocktail"				},

	COMMON_COIN_DIPS(0x0d)

	{0   , 0xfe, 0   ,    4, "1st Extended Time"	},
	{0x0e, 0x01, 0x03, 0x00, "10000"				},
	{0x0e, 0x01, 0x03, 0x01, "15000"				},
	{0x0e, 0x01, 0x03, 0x02, "20000"				},
	{0x0e, 0x01, 0x03, 0x03, "25000"				},

	{0   , 0xfe, 0   ,    8, "Extended Time Every"	},
	{0x0e, 0x01, 0x1c, 0x00, "5000"					},
	{0x0e, 0x01, 0x1c, 0x04, "6000"					},
	{0x0e, 0x01, 0x1c, 0x08, "7000"					},
	{0x0e, 0x01, 0x1c, 0x0c, "8000"					},
	{0x0e, 0x01, 0x1c, 0x10, "9000"					},
	{0x0e, 0x01, 0x1c, 0x14, "10000"				},
	{0x0e, 0x01, 0x1c, 0x18, "11000"				},
	{0x0e, 0x01, 0x1c, 0x1c, "12000"				},

	{0   , 0xfe, 0   ,    2, "Year Display"			},
	{0x0e, 0x01, 0x20, 0x00, "No"					},
	{0x0e, 0x01, 0x20, 0x20, "Yes"					},

	{0   , 0xfe, 0   ,    2, "Hit Detection"		},
	{0x0e, 0x01, 0x40, 0x40, "Normal Game"			},
	{0x0e, 0x01, 0x40, 0x00, "No Hit"				},

	{0   , 0xfe, 0   ,    2, "Coin Slots"			},
	{0x0e, 0x01, 0x80, 0x80, "A and B"				},
	{0x0e, 0x01, 0x80, 0x00, "A only"				},
};

STDDIPINFO(Alpinea)

static struct BurnDIPInfo TimetunlDIPList[]=
{
	{0x0c, 0xff, 0xff, 0x1c, NULL					},
	{0x0d, 0xff, 0xff, 0x00, NULL					},
	{0x0e, 0xff, 0xff, 0xf0, NULL					},

	{0   , 0xfe, 0   ,    2, "Free Play"			},
	{0x0c, 0x01, 0x04, 0x04, "Off"					},
	{0x0c, 0x01, 0x04, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x0c, 0x01, 0x18, 0x18, "3"					},
	{0x0c, 0x01, 0x18, 0x10, "4"					},
	{0x0c, 0x01, 0x18, 0x08, "5"					},
	{0x0c, 0x01, 0x18, 0x00, "6"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x0c, 0x01, 0x40, 0x00, "Off"					},
	{0x0c, 0x01, 0x40, 0x40, "On"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x0c, 0x01, 0x80, 0x00, "Upright"				},
	{0x0c, 0x01, 0x80, 0x80, "Cocktail"				},

	COMMON_COIN_DIPS(0x0d)

	{0   , 0xfe, 0   ,    2, "Coinage Display"		},
	{0x0e, 0x01, 0x10, 0x10, "Coins/Credits"		},
	{0x0e, 0x01, 0x10, 0x00, "Insert Coin"			},

	{0   , 0xfe, 0   ,    2, "Year Display"			},
	{0x0e, 0x01, 0x20, 0x00, "No"					},
	{0x0e, 0x01, 0x20, 0x20, "Yes"					},

	{0   , 0xfe, 0   ,    2, "Hit Detection"		},
	{0x0e, 0x01, 0x40, 0x40, "Normal Game"			},
	{0x0e, 0x01, 0x40, 0x00, "No Hit"				},

	{0   , 0xfe, 0   ,    2, "Coin Slots"			},
	{0x0e, 0x01, 0x80, 0x80, "A and B"				},
	{0x0e, 0x01, 0x80, 0x00, "A only"				},
};

STDDIPINFO(Timetunl)

static struct BurnDIPInfo ElevatorDIPList[]=
{
	{0x12, 0xff, 0xff, 0x7f, NULL					},
	{0x13, 0xff, 0xff, 0x00, NULL					},
	{0x14, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x12, 0x01, 0x03, 0x03, "10000"				},
	{0x12, 0x01, 0x03, 0x02, "15000"				},
	{0x12, 0x01, 0x03, 0x01, "20000"				},
	{0x12, 0x01, 0x03, 0x00, "25000"				},

	{0   , 0xfe, 0   ,    2, "Free Play"			},
	{0x12, 0x01, 0x04, 0x04, "Off"					},
	{0x12, 0x01, 0x04, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x12, 0x01, 0x18, 0x18, "3"					},
	{0x12, 0x01, 0x18, 0x10, "4"					},
	{0x12, 0x01, 0x18, 0x08, "5"					},
	{0x12, 0x01, 0x18, 0x00, "6"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x12, 0x01, 0x40, 0x40, "Off"					},
	{0x12, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x12, 0x01, 0x80, 0x00, "Upright"				},
	{0x12, 0x01, 0x80, 0x80, "Cocktail"				},

	COMMON_COIN_DIPS(0x13)

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x14, 0x01, 0x03, 0x03, "Easiest"				},
	{0x14, 0x01, 0x03, 0x02, "Easy"					},
	{0x14, 0x01, 0x03, 0x01, "Normal"				},
	{0x14, 0x01, 0x03, 0x00, "Hard"					},

	{0   , 0xfe, 0   ,    2, "Coinage Display"		},
	{0x14, 0x01, 0x10, 0x10, "Coins/Credits"		},
	{0x14, 0x01, 0x10, 0x00, "Insert Coin"			},

	{0   , 0xfe, 0   ,    2, "Year Display"			},
	{0x14, 0x01, 0x20, 0x00, "No"					},
	{0x14, 0x01, 0x20, 0x20, "Yes"					},

	{0   , 0xfe, 0   ,    2, "Hit Detection"		},
	{0x14, 0x01, 0x40, 0x40, "Normal Game"			},
	{0x14, 0x01, 0x40, 0x00, "No Hit"				},

	{0   , 0xfe, 0   ,    2, "Coin Slots"			},
	{0x14, 0x01, 0x80, 0x80, "A and B"				},
	{0x14, 0x01, 0x80, 0x00, "A only"				},
};

STDDIPINFO(Elevator)

static struct BurnDIPInfo WaterskiDIPList[]=
{
	{0x0e, 0xff, 0xff, 0x3f, NULL					},
	{0x0f, 0xff, 0xff, 0x00, NULL					},
	{0x10, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    2, "Free Play"			},
	{0x0e, 0x01, 0x04, 0x04, "Off"					},
	{0x0e, 0x01, 0x04, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Game Time"			},
	{0x0e, 0x01, 0x18, 0x00, "2:00"					},
	{0x0e, 0x01, 0x18, 0x08, "2:10"					},
	{0x0e, 0x01, 0x18, 0x10, "2:20"					},
	{0x0e, 0x01, 0x18, 0x18, "2:30"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x0e, 0x01, 0x40, 0x00, "Off"					},
	{0x0e, 0x01, 0x40, 0x40, "On"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x0e, 0x01, 0x80, 0x00, "Upright"				},
	{0x0e, 0x01, 0x80, 0x80, "Cocktail"				},

	COMMON_COIN_DIPS(0x0f)

	{0   , 0xfe, 0   ,    2, "Coinage Display"		},
	{0x10, 0x01, 0x10, 0x10, "Coins/Credits"		},
	{0x10, 0x01, 0x10, 0x00, "Insert Coin"			},

	{0   , 0xfe, 0   ,    2, "Year Display"			},
	{0x10, 0x01, 0x20, 0x00, "No"					},
	{0x10, 0x01, 0x20, 0x20, "Yes"					},

	{0   , 0xfe, 0   ,    2, "Hit Detection"		},
	{0x10, 0x01, 0x40, 0x40, "Normal Game"			},
	{0x10, 0x01, 0x40, 0x00, "No Hit"				},

	{0   , 0xfe, 0   ,    2, "Coin Slots"			},
	{0x10, 0x01, 0x80, 0x80, "A and B"				},
	{0x10, 0x01, 0x80, 0x00, "A only"				},
};

STDDIPINFO(Waterski)

static struct BurnDIPInfo BioatackDIPList[]=
{
	{0x10, 0xff, 0xff, 0x3f, NULL					},
	{0x11, 0xff, 0xff, 0x00, NULL					},
	{0x12, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x10, 0x01, 0x03, 0x03, "5000"					},
	{0x10, 0x01, 0x03, 0x02, "10000"				},
	{0x10, 0x01, 0x03, 0x01, "15000"				},
	{0x10, 0x01, 0x03, 0x00, "20000"				},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x10, 0x01, 0x18, 0x18, "3"					},
	{0x10, 0x01, 0x18, 0x10, "4"					},
	{0x10, 0x01, 0x18, 0x08, "5"					},
	{0x10, 0x01, 0x18, 0x00, "6"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x10, 0x01, 0x40, 0x00, "Off"					},
	{0x10, 0x01, 0x40, 0x40, "On"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x10, 0x01, 0x80, 0x00, "Upright"				},
	{0x10, 0x01, 0x80, 0x80, "Cocktail"				},

	COMMON_COIN_DIPS(0x11)

	{0   , 0xfe, 0   ,    2, "Year Display"			},
	{0x12, 0x01, 0x20, 0x00, "No"					},
	{0x12, 0x01, 0x20, 0x20, "Yes"					},

	{0   , 0xfe, 0   ,    2, "Hit Detection"		},
	{0x12, 0x01, 0x40, 0x40, "No Hit"				},
	{0x12, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Coin Slots"			},
	{0x12, 0x01, 0x80, 0x80, "A and B"				},
	{0x12, 0x01, 0x80, 0x00, "A only"				},
};

STDDIPINFO(Bioatack)

static struct BurnDIPInfo FrontlinDIPList[]=
{
	{0x1b, 0xff, 0xff, 0x7f, NULL					},
	{0x1c, 0xff, 0xff, 0x00, NULL					},
	{0x1d, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x1b, 0x01, 0x03, 0x03, "10000"				},
	{0x1b, 0x01, 0x03, 0x02, "20000"				},
	{0x1b, 0x01, 0x03, 0x01, "30000"				},
	{0x1b, 0x01, 0x03, 0x00, "50000"				},

	{0   , 0xfe, 0   ,    2, "Free Play"			},
	{0x1b, 0x01, 0x04, 0x04, "Off"					},
	{0x1b, 0x01, 0x04, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x1b, 0x01, 0x18, 0x18, "3"					},
	{0x1b, 0x01, 0x18, 0x10, "4"					},
	{0x1b, 0x01, 0x18, 0x08, "5"					},
	{0x1b, 0x01, 0x18, 0x00, "6"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x1b, 0x01, 0x20, 0x20, "Off"					},
	{0x1b, 0x01, 0x20, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x1b, 0x01, 0x40, 0x40, "Off"					},
	{0x1b, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x1b, 0x01, 0x80, 0x00, "Upright"				},
	{0x1b, 0x01, 0x80, 0x80, "Cocktail"				},

	COMMON_COIN_DIPS(0x1c)

	{0   , 0xfe, 0   ,    2, "Coinage Display"		},
	{0x1d, 0x01, 0x10, 0x10, "Coins/Credits"		},
	{0x1d, 0x01, 0x10, 0x00, "Insert Coin"			},

	{0   , 0xfe, 0   ,    2, "Year Display"			},
	{0x1d, 0x01, 0x20, 0x00, "No"					},
	{0x1d, 0x01, 0x20, 0x20, "Yes"					},

	{0   , 0xfe, 0   ,    2, "Hit Detection"		},
	{0x1d, 0x01, 0x40, 0x40, "Normal Game"			},
	{0x1d, 0x01, 0x40, 0x00, "No Hit"				},

	{0   , 0xfe, 0   ,    2, "Coin Slots"			},
	{0x1d, 0x01, 0x80, 0x80, "A and B"				},
	{0x1d, 0x01, 0x80, 0x00, "A only"				},
};

STDDIPINFO(Frontlin)

static struct BurnDIPInfo WwesternDIPList[]=
{
	{0x1b, 0xff, 0xff, 0x7b, NULL					},
	{0x1c, 0xff, 0xff, 0x00, NULL					},
	{0x1d, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x1b, 0x01, 0x03, 0x03, "10000"				},
	{0x1b, 0x01, 0x03, 0x02, "30000"				},
	{0x1b, 0x01, 0x03, 0x01, "50000"				},
	{0x1b, 0x01, 0x03, 0x00, "70000"				},

	{0   , 0xfe, 0   ,    2, "High Score Table"		},
	{0x1b, 0x01, 0x04, 0x04, "No"					},
	{0x1b, 0x01, 0x04, 0x00, "Yes"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x1b, 0x01, 0x18, 0x18, "3"					},
	{0x1b, 0x01, 0x18, 0x10, "4"					},
	{0x1b, 0x01, 0x18, 0x08, "5"					},
	{0x1b, 0x01, 0x18, 0x00, "6"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x1b, 0x01, 0x20, 0x20, "Off"					},
	{0x1b, 0x01, 0x20, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x1b, 0x01, 0x40, 0x40, "Off"					},
	{0x1b, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x1b, 0x01, 0x80, 0x00, "Upright"				},
	{0x1b, 0x01, 0x80, 0x80, "Cocktail"				},

	COMMON_COIN_DIPS(0x1c)

	{0   , 0xfe, 0   ,    2, "Hit Detection"		},
	{0x1d, 0x01, 0x40, 0x40, "Normal Game"			},
	{0x1d, 0x01, 0x40, 0x00, "No Hit"				},

	{0   , 0xfe, 0   ,    2, "Coin Slots"			},
	{0x1d, 0x01, 0x80, 0x80, "A and B"				},
	{0x1d, 0x01, 0x80, 0x00, "A only"				},
};

STDDIPINFO(Wwestern)

static struct BurnDIPInfo TinstarDIPList[]=
{
	{0x1b, 0xff, 0xff, 0x37, NULL					},
	{0x1c, 0xff, 0xff, 0x00, NULL					},
	{0x1d, 0xff, 0xff, 0xfd, NULL					},

	{0   , 0xfe, 0   ,    4, "Bonus Life?"			},
	{0x1b, 0x01, 0x03, 0x03, "10000?"				},
	{0x1b, 0x01, 0x03, 0x02, "20000?"				},
	{0x1b, 0x01, 0x03, 0x01, "30000?"				},
	{0x1b, 0x01, 0x03, 0x00, "50000?"				},

	{0   , 0xfe, 0   ,    2, "Free Play"			},
	{0x1b, 0x01, 0x04, 0x04, "Off"					},
	{0x1b, 0x01, 0x04, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x1b, 0x01, 0x18, 0x18, "2"					},
	{0x1b, 0x01, 0x18, 0x10, "3"					},
	{0x1b, 0x01, 0x18, 0x08, "4"					},
	{0x1b, 0x01, 0x18, 0x00, "5"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x1b, 0x01, 0x20, 0x20, "Off"					},
	{0x1b, 0x01, 0x20, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x1b, 0x01, 0x40, 0x00, "Off"					},
	{0x1b, 0x01, 0x40, 0x40, "On"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x1b, 0x01, 0x80, 0x00, "Upright"				},
	{0x1b, 0x01, 0x80, 0x80, "Cocktail"				},

	COMMON_COIN_DIPS(0x1c)

	{0   , 0xfe, 0   ,    2, "Coinage Display"		},
	{0x1d, 0x01, 0x10, 0x10, "Coins/Credits"		},
	{0x1d, 0x01, 0x10, 0x00, "Insert Coin"			},

	{0   , 0xfe, 0   ,    2, "Year Display"			},
	{0x1d, 0x01, 0x20, 0x00, "No"					},
	{0x1d, 0x01, 0x20, 0x20, "Yes"					},

	{0   , 0xfe, 0   ,    2, "Hit Detection"		},
	{0x1d, 0x01, 0x40, 0x40, "Normal Game"			},
	{0x1d, 0x01, 0x40, 0x00, "No Hit"				},

	{0   , 0xfe, 0   ,    2, "Coin Slots"			},
	{0x1d, 0x01, 0x80, 0x80, "A and B"				},
	{0x1d, 0x01, 0x80, 0x00, "A only"				},
};

STDDIPINFO(Tinstar)

static struct BurnDIPInfo SfposeidDIPList[]=
{
	{0x12, 0xff, 0xff, 0x6f, NULL					},
	{0x13, 0xff, 0xff, 0x00, NULL					},
	{0x14, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x12, 0x01, 0x18, 0x00, "2"					},
	{0x12, 0x01, 0x18, 0x08, "3"					},
	{0x12, 0x01, 0x18, 0x10, "4"					},
	{0x12, 0x01, 0x18, 0x18, "5"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x12, 0x01, 0x40, 0x40, "Off"					},
	{0x12, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x12, 0x01, 0x80, 0x00, "Upright"				},
	{0x12, 0x01, 0x80, 0x80, "Cocktail"				},

	COMMON_COIN_DIPS(0x13)

	{0   , 0xfe, 0   ,    2, "Coinage Display"		},
	{0x14, 0x01, 0x10, 0x10, "Coins/Credits"		},
	{0x14, 0x01, 0x10, 0x00, "Insert Coin"			},

	{0   , 0xfe, 0   ,    2, "Year Display"			},
	{0x14, 0x01, 0x20, 0x00, "No"					},
	{0x14, 0x01, 0x20, 0x20, "Yes"					},

	{0   , 0xfe, 0   ,    2, "Hit Detection"		},
	{0x14, 0x01, 0x40, 0x40, "Normal Game"			},
	{0x14, 0x01, 0x40, 0x00, "No Hit"				},

	{0   , 0xfe, 0   ,    2, "Coin Slots"			},
	{0x14, 0x01, 0x80, 0x80, "A and B"				},
	{0x14, 0x01, 0x80, 0x00, "A only"				},
};

STDDIPINFO(Sfposeid)

static struct BurnDIPInfo HwraceDIPList[]=
{
	{0x12, 0xff, 0xff, 0x7f, NULL					},
	{0x13, 0xff, 0xff, 0x00, NULL					},
	{0x14, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    2, "Free Play"			},
	{0x12, 0x01, 0x04, 0x04, "Off"					},
	{0x12, 0x01, 0x04, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x12, 0x01, 0x20, 0x20, "Off"					},
	{0x12, 0x01, 0x20, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x12, 0x01, 0x40, 0x40, "Off"					},
	{0x12, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x12, 0x01, 0x80, 0x00, "Upright"				},
	{0x12, 0x01, 0x80, 0x80, "Cocktail"				},

	COMMON_COIN_DIPS(0x13)

	{0   , 0xfe, 0   ,    2, "Coinage Display"		},
	{0x14, 0x01, 0x10, 0x00, "Off"					},
	{0x14, 0x01, 0x10, 0x10, "On"					},

	{0   , 0xfe, 0   ,    2, "Year Display"			},
	{0x14, 0x01, 0x20, 0x00, "No"					},
	{0x14, 0x01, 0x20, 0x20, "Yes"					},

	{0   , 0xfe, 0   ,    2, "Hit Detection"		},
	{0x14, 0x01, 0x40, 0x40, "Normal Game"			},
	{0x14, 0x01, 0x40, 0x00, "No Hit"				},

	{0   , 0xfe, 0   ,    2, "Coin Slots"			},
	{0x14, 0x01, 0x80, 0x80, "A and B"				},
	{0x14, 0x01, 0x80, 0x00, "A only"				},
};

STDDIPINFO(Hwrace)

static void alpine_protection_write(INT32 data)
{
	switch (data)
	{
		case 0x05:
			protection_value = 0x18;
		break;

		case 0x07:
		case 0x0c:
		case 0x0f:
			protection_value = 0x00;      // not used?
		break;

		case 0x16:
			protection_value = 0x08;
		break;

		case 0x1d:
			protection_value = 0x18;
		break;

		default:
			protection_value = data;      // not used?
		break;
	}
}

static void ram_decode(INT32 offset)
{
	INT32 destbase = 0;
	INT32 source = 0;

	if (offset >= 0x1800) {
		destbase = 0x4000;
		source = 0x1800;
	}

	offset &= 0x7ff;
	UINT8 a = DrvCharRAM[0x0000 + offset + source];
	UINT8 b = DrvCharRAM[0x0800 + offset + source];
	UINT8 c = DrvCharRAM[0x1000 + offset + source];
	UINT8 *tdest = DrvGfxExp + destbase + (offset * 8);
	UINT8 *sdest = DrvSprExp + destbase + (((offset & 7) << 4) + (offset & 8) + ((offset & 0x7f0) << 3));

	for (INT32 i = 0; i < 8; i++)
	{
		tdest[i] = sdest[i] = ((a >> i) & 1) | (((b >> i) & 1) << 1) | (((c >> i) & 1) << 2);
	}
}

static void sync_mcu()
{
	if (has_mcu) {
		INT32 cyc = (ZetTotalCycles(0) * (3000000 / 4) / 4000000) - m6805TotalCycles();
		if (cyc > 0) {
			m6805Run(cyc);
		}
	}
}

static void bankswitch(INT32 data)
{
	rom_bank = data;

	INT32 bank = 0x6000 + ((data >> 7) * 0x2000);

	ZetMapMemory(DrvZ80ROM0 + bank, 	0x6000, 0x7fff, MAP_ROM);

	if (is_alpine == 2) protection_value = data >> 2; // alpinea
}

static void __fastcall taitosj_main_write(UINT16 address, UINT8 data)
{
	if (address >= 0xd700) return; // nop

	if (address >= 0x9000 && address <= 0xbfff) {
		address -= 0x9000;
		DrvCharRAM[address] = data ^ charram_xor;
		ram_decode(address);
		return;
	}

	if ((address & 0xff00) == 0xd200) { // mirror 2x
		DrvPalRAM[address & 0x7f] = data ^ 0xff;
		return;
	}

	if ((address & 0xf000) == 0xd000) address &= ~0x00f0;
	if ((address & 0xf800) == 0x8800 && is_kikstart == 0) address &= ~0x07fe; // kikstart has no mirror

	if (address >= 0xd400 && address <= 0xd40d) return; // nop

	switch (address)
	{
		case 0x8800:
		{
			if (has_mcu == 0) return;
			sync_mcu();
			zready = 1;
			m68705SetIrqLine(0, 1);
			fromz80 = data;
		}
		return;

		case 0x8801:
		return;	// nop?

		case 0x8802:
		return;	// kikstart nop

		case 0xd300:
			video_priority = data;
		return;

		case 0xd40e:
		case 0xd40f:
			AY8910Write(0, address & 1, data);
		return;

		case 0xd500:
		case 0xd501:
		case 0xd502:
		case 0xd503:
		case 0xd504:
		case 0xd505:
			scroll[address & 7] = data;
		return;

		case 0xd506:
		case 0xd507:
			color_bank[address & 1] = data;
		return;

		case 0xd508:
			memset (collision_reg, 0, 4);
		return;

		case 0xd509:
		case 0xd50a:
			gfxpointer[address - 0xd509] = data;
		return;

		case 0xd50b:
			soundlatch = data;
			if (sound_nmi_disable == 0) {
				ZetNmi(1);
			}
		return;

		case 0xd50c:
			// semaphore2_w
		return;

		case 0xd50d:
			BurnWatchdogWrite();
		return;

		case 0xd50e:
			bankswitch(data);
		return;

		case 0xd50f:
			if (is_alpine == 1) alpine_protection_write(data);
			// nop
		return;

		case 0xd600:
			video_mode = data;
		return;
	}

	bprintf (0, _T("MW: %4.4x, %2.2x bad!\n"), address, data);
}

static UINT8 __fastcall taitosj_main_read(UINT16 address)
{
	if (address >= 0xd700) return 0; // nop

	if ((address & 0xff00) == 0xd200) { // mirror 2x
		return DrvPalRAM[address & 0x7f] ^ 0xff;
	}

	if ((address & 0xf000) == 0xd000) address &= ~0x00f0;
	if ((address & 0xf800) == 0x8800 && is_kikstart == 0) address &= ~0x07fe; // kikstart has no mirror

	switch (address)
	{
		case 0x8800:
			if (has_mcu) {
				sync_mcu();
				zaccept = 1;
				return toz80;
			}
			return 0x00;

		case 0x8801:
			if (has_mcu) {
				sync_mcu();
				return ~((zready << 0) | (zaccept << 1));
			}
			return 0xff;

		case 0x8802:
			return 0;	// kikstart nop

		case 0xd400:
		case 0xd401:
		case 0xd402:
		case 0xd403:
			return collision_reg[address & 3];

		case 0xd404:
		{
			UINT16 addr = gfxpointer[0] + (gfxpointer[1] << 8);

			UINT8 ret = 0;
			if (addr < 0x8000) ret = DrvGfxROM[addr];

			addr++;
			gfxpointer[0] = addr & 0xff;
			gfxpointer[1] = addr >> 8;

			return ret;
		}

		case 0xd408:
			return DrvInputs[0]; // in0

		case 0xd409:
			return DrvInputs[1]; // in1

		case 0xd40a:
			return DrvDips[0]; // dsw1

		case 0xd40b:
			if (is_alpine == 1) return ((DrvInputs[2] & ~0x1e) | protection_value);
			if (is_alpine == 2) return ((DrvInputs[2] & ~0x0f) | protection_value);
			return DrvInputs[2]; // in2

		case 0xd40c:
			return DrvInputs[3]; // in3

		case 0xd40d:
			return DrvInputs[4] | (input_port_data & 0xf0); // in4

		case 0xd40f:
			return AY8910Read(0);

		case 0xd48b:
			protection_value ^= 0xff;
			return protection_value;
	}

	bprintf (0, _T("MR: %4.4x bad!\n"), address);

	return 0;
}

static void __fastcall kikstart_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xd000:
		case 0xd001:
			color_bank[address & 1] = data;
		return;

		case 0xd002:
		case 0xd003:
		case 0xd004:
		case 0xd005:
		case 0xd006:
		case 0xd007:
			scroll[address - 0xd002] = data;
		return;
	}

	taitosj_main_write(address, data);
}

static void __fastcall taitosj_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x4800:
		case 0x4801:
			AY8910Write(1, address & 1, data);
		return;

		case 0x4802:
		case 0x4803:
			AY8910Write(2, address & 1, data);
		return;

		case 0x4804:
		case 0x4805:
			AY8910Write(3, address & 1, data);
		return;
	}
}

static UINT8 __fastcall taitosj_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x4800:
		case 0x4801:
			return AY8910Read(1);

		case 0x4802:
		case 0x4803:
			return AY8910Read(2);

		case 0x4804:
		case 0x4805:
			return AY8910Read(3);

		case 0x5000:
			return soundlatch;
	}

	return 0;
}

static void taitosj_68705_portB_w(UINT8 data)
{
	if (~data & 0x02)
	{
		zready = 0;
		m68705SetIrqLine(0, 0);
		portA_in = fromz80;
	}

	busreq = (data & 0x08) >> 3;

	if (~data & 0x04)
	{
		toz80 = portA_out;
		zaccept = 0;
	}

	if (~data & 0x10)
	{
		ZetWriteByte(mcu_address, portA_out);
		mcu_address = (mcu_address & 0xff00) | ((mcu_address + 1) & 0xff);
	}

	if (~data & 0x20)
	{
		portA_in = ZetReadByte(mcu_address);
	}

	if (~data & 0x40)
	{
		mcu_address = (mcu_address & 0xff00) | portA_out;
	}

	if (~data & 0x80)
	{
		mcu_address = (mcu_address & 0x00ff) | (portA_out << 8);
	}
}

static void m67805_mcu_write(UINT16 address, UINT8 data)
{
	switch (address & 0x7ff)
	{
		case 0x0000:
			portA_out = data;
		return;

		case 0x0001:
			taitosj_68705_portB_w(data);
		return;
	}

	if (address < 0x80) DrvMCURAM[address] = data;
}

static UINT8 m67805_mcu_read(UINT16 address)
{
	switch (address & 0x7ff)
	{
		case 0x0000:
			return portA_in;

		case 0x0001:
			return 0xff;

		case 0x0002:
			return (zready << 0) | (zaccept << 1) | (busreq << 2);
	}

	if (address < 0x80) return DrvMCURAM[address];

	return 0;
}

static UINT8 ay8910_0_port_A_r(UINT32)
{
	return DrvDips[1];
}

static UINT8 ay8910_0_port_B_r(UINT32)
{
	return DrvDips[2];
}

static void ay8910_1_port_A_w(UINT32, UINT32 data)
{
	dac_out_data = data - 0x80;
	DACWrite16(0, dac_out_data * dac_volume);
}

static void ay8910_1_port_B_w(UINT32, UINT32 data)
{
	dac_volume = (((data ^ 255) * 157) / 255) + 98;
	DACWrite16(0, dac_out_data * dac_volume);
}

static void ay8910_2_port_A_w(UINT32, UINT32 data)
{
	input_port_data = data;
}

static void ay8910_3_port_B_w(UINT32, UINT32 data)
{
	sound_nmi_disable = data & 1;
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);

		input_port_data = 0;
		video_priority = 0;
		soundlatch = 0xff;
		sound_nmi_disable = 1;
		video_mode = 0;
		memset (scroll, 0, 6);
		memset (color_bank, 0, 2);
		memset (gfxpointer, 0, 2);
		memset (collision_reg, 0, 4);
		sound_irq_timer = 0;
		dac_volume = 0;
		dac_out_data = 0;
		protection_value = 0;

		toz80 = 0;
		fromz80 = 0;
		mcu_address = 0;
		portA_in = 0;
		portA_out = 0;
		zready = 0;
		zaccept = 1;
		busreq = 0;

		kikstart_gears[0] = 0;
		kikstart_gears[1] = 0;
	}

	ZetOpen(0);
	if (clear_mem) bankswitch(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	DACReset();
	AY8910Reset(0);
	AY8910Reset(1);
	AY8910Reset(2);
	AY8910Reset(3);
	ZetClose();

	m6805Open(0);
	m68705Reset();
	m68705SetIrqLine(0, CPU_IRQSTATUS_NONE);
	m6805Close();

	BurnWatchdogReset();

	sound_irq_timer = 0;

	nCyclesExtra[0] = nCyclesExtra[1] = nCyclesExtra[2] = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x0100000;
	DrvZ80ROM1		= Next; Next += 0x0100000;

	DrvMCUROM		= Next; Next += 0x0008000;

	DrvGfxExp		= Next; Next += 0x0080000;
	DrvSprExp		= Next; Next += 0x0080000;

	DrvGfxROM		= Next; Next += 0x0100000;

	DrvPalette		= (UINT32*)Next; Next += 0x400 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAMA		= Next; Next += 0x0008000;
	DrvZ80RAMB		= Next; Next += 0x0004000;
	DrvZ80RAMC		= Next; Next += 0x0008000;
	DrvVidRAM1		= Next; Next += 0x0004000;
	DrvVidRAM2		= Next; Next += 0x0004000;
	DrvVidRAM3		= Next; Next += 0x0004000;

	DrvZ80RAM1		= Next; Next += 0x0004000;

	DrvPalRAM		= Next; Next += 0x0000800;

	DrvMCURAM		= Next; Next += 0x0000800;

	DrvColScroll	= Next; Next += 0x0001000;
	DrvSprRAM		= Next; Next += 0x0001000;

	DrvCharRAM		= Next; Next += 0x0030000;

	RamEnd			= Next;

	bitmap[0]		= (UINT16*)Next; Next += 256 * 256 * sizeof(INT16);
	bitmap[1]		= (UINT16*)Next; Next += 256 * 256 * sizeof(INT16);
	bitmap[2]		= (UINT16*)Next; Next += 256 * 256 * sizeof(INT16);
	bitmap[3]		= NULL; // not used
	bitmap[4]		= (UINT16*)Next; Next += 32 * 32 * sizeof(INT16);
	bitmap[5]		= (UINT16*)Next; Next += 32 * 32 * sizeof(INT16);

	bitmap[6]		= (UINT16*)Next; Next += 256 * 256 * sizeof(INT16);
	bitmap[7]		= (UINT16*)Next; Next += 256 * 256 * sizeof(INT16);
	bitmap[8]		= (UINT16*)Next; Next += 256 * 256 * sizeof(INT16);

	MemEnd			= Next;

	return 0;
}

static void DrvVideoOrder(UINT8 *prom)
{
	for (INT32 i = 0; i < 32; i++)
	{
		UINT8 mask = 0;

		for (INT32 j = 3; j >= 0; j--)
		{
			UINT8 data = (prom[0x10 * (i & 0x0f) + mask] >> ((i & 0x10) ? 2 : 0)) & 3;

			mask |= (1 << data);

			draw_order[i][j] = data;
		}
	}
}

static INT32 DrvLoadRoms()
{
	char* pRomName;
	struct BurnRomInfo ri;

	UINT8 *mLoad = DrvZ80ROM0;
	UINT8 *sLoad = DrvZ80ROM1;
	UINT8 *gLoad = DrvGfxROM;

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {

		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & 7) == 1) {
			if ((mLoad - DrvZ80ROM0) >= 0xa000) mLoad = DrvZ80ROM0 + 0xe000; // frontline
			if (BurnLoadRom(mLoad, i, 1)) return 1;
			mLoad += ri.nLen;
			if (ri.nType & 8) mLoad += 0x1000;
			continue;
		}

		if ((ri.nType & 7) == 2) {
			if (BurnLoadRom(sLoad, i, 1)) return 1;
			sLoad += ri.nLen;
			continue;
		}

		if ((ri.nType & 7) == 3) {
			if (BurnLoadRom(gLoad, i, 1)) return 1;
			gLoad += ri.nLen;
			continue;
		}

		if ((ri.nType & 7) == 4) {
			UINT8 *tmp = (UINT8*)BurnMalloc(ri.nLen);
			if (BurnLoadRom(tmp, i, 1)) return 1;
			DrvVideoOrder(tmp);
			BurnFree(tmp);
			continue;
		}

		if ((ri.nType & 7) == 5) {
			if (BurnLoadRom(DrvMCUROM, i, 1)) return 1;
			has_mcu = 1;
			continue;
		}
	}

	return 0;
}

static INT32 CommonInit(INT32 coinstate, INT32 charramxor, INT32 kikstart)
{
	BurnAllocMemIndex();

	if (DrvLoadRoms()) return 1;

	if (kikstart)
	{
		ZetInit(0);
		ZetOpen(0);
		ZetMapMemory(DrvZ80ROM0,		0x0000, 0x5fff, MAP_ROM);
		ZetMapMemory(DrvZ80RAMA,		0x8000, 0x87ff, MAP_RAM);
		ZetMapMemory(DrvColScroll,		0x8a00, 0x8aff, MAP_RAM); // 0-5f
		ZetMapMemory(DrvCharRAM,		0x9000, 0xbfff, MAP_ROM); // Handler
		ZetMapMemory(DrvZ80RAMB,		0xc000, 0xc3ff, MAP_RAM);
		ZetMapMemory(DrvVidRAM1,		0xc400, 0xc7ff, MAP_RAM);
		ZetMapMemory(DrvVidRAM2,		0xc800, 0xcbff, MAP_RAM);
		ZetMapMemory(DrvVidRAM3,		0xcc00, 0xcfff, MAP_RAM);
		ZetMapMemory(DrvSprRAM,			0xd100, 0xd1ff, MAP_RAM);
		ZetMapMemory(DrvZ80RAMC,		0xd800, 0xdfff, MAP_RAM);
		ZetMapMemory(DrvZ80ROM0 + 0xe000,	0xe000, 0xffff, MAP_ROM);
		ZetSetWriteHandler(kikstart_main_write);
		ZetSetReadHandler(taitosj_main_read);
		ZetClose();
	} else {
		ZetInit(0);
		ZetOpen(0);
		ZetMapMemory(DrvZ80ROM0,		0x0000, 0x5fff, MAP_ROM);
		ZetMapMemory(DrvZ80RAMA,		0x8000, 0x87ff, MAP_RAM);
		ZetMapMemory(DrvCharRAM,		0x9000, 0xbfff, MAP_ROM); // Handler
		ZetMapMemory(DrvZ80RAMB,		0xc000, 0xc3ff, MAP_RAM);
		ZetMapMemory(DrvVidRAM1,		0xc400, 0xc7ff, MAP_RAM);
		ZetMapMemory(DrvVidRAM2,		0xc800, 0xcbff, MAP_RAM);
		ZetMapMemory(DrvVidRAM3,		0xcc00, 0xcfff, MAP_RAM);
		ZetMapMemory(DrvColScroll,		0xd000, 0xd0ff, MAP_RAM); // 0-5f
		ZetMapMemory(DrvSprRAM,			0xd100, 0xd1ff, MAP_RAM);
		ZetMapMemory(DrvZ80ROM0 + 0xe000,	0xe000, 0xffff, MAP_ROM);
		ZetSetWriteHandler(taitosj_main_write);
		ZetSetReadHandler(taitosj_main_read);
		ZetClose();
	}

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,		0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,		0x4000, 0x43ff, MAP_RAM);
	ZetMapMemory(DrvZ80ROM1 + 0xe000,	0xe000, 0xefff, MAP_ROM);
	ZetSetWriteHandler(taitosj_sound_write);
	ZetSetReadHandler(taitosj_sound_read);
	ZetClose();

	m6805Init(1, 0x800 /*max memory range - page size is max range / 0x100*/);
	m6805Open(0);
	m6805MapMemory(DrvMCURAM + 0x08, 	0x0008, 0x007f, MAP_RAM);
	m6805MapMemory(DrvMCUROM + 0x80,	0x0080, 0x07ff, MAP_ROM);
	m6805SetWriteHandler(m67805_mcu_write);
	m6805SetReadHandler(m67805_mcu_read);
	m6805Close();

	BurnWatchdogInit(DrvDoReset, 180);

	DACInit(0, 0, 1, ZetTotalCycles, 3000000);
	DACSetRoute(0, 0.15, BURN_SND_ROUTE_BOTH);

	AY8910Init(0, 1500000, 0);
	AY8910Init(1, 1500000, 1);
	AY8910Init(2, 1500000, 1);
	AY8910Init(3, 1500000, 1);
	AY8910SetPorts(0, &ay8910_0_port_A_r, &ay8910_0_port_B_r, NULL, NULL);
	AY8910SetPorts(1, NULL, NULL, &ay8910_1_port_A_w, &ay8910_1_port_B_w);
	AY8910SetPorts(2, NULL, NULL, &ay8910_2_port_A_w, NULL);
	AY8910SetPorts(3, NULL, NULL, NULL, &ay8910_3_port_B_w);
	AY8910SetAllRoutes(0, 0.12, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.12, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(2, 0.12, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(3, 0.18, BURN_SND_ROUTE_BOTH);
    AY8910SetBuffered(ZetTotalCycles, 3000000);

	coin_state = (coinstate) ? 0 : 0x10;
	charram_xor = charramxor;

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	m6805Exit();

	AY8910Exit(0);
	AY8910Exit(1);
	AY8910Exit(2);
	AY8910Exit(3);
	DACExit();

	BurnFreeMemIndex();

	input2_xor = 0;
	has_mcu = 0;
	is_alpine = 0;
	is_kikstart = 0;

	return 0;
}

static void DrvPaletteUpdate()
{
	for (INT32 i = 0; i < 0x40; i++)
	{
		INT32 bit0 = (DrvPalRAM[(i * 2) + 1] >> 6) & 0x01;
		INT32 bit1 = (DrvPalRAM[(i * 2) + 1] >> 7) & 0x01;
		INT32 bit2 = (DrvPalRAM[(i * 2) + 0] >> 0) & 0x01;
		INT32 r = (bit2 * 1000) + (bit1 * 470) + (bit0 * 270);

		bit0 = (DrvPalRAM[(i * 2) + 1] >> 3) & 0x01;
		bit1 = (DrvPalRAM[(i * 2) + 1] >> 4) & 0x01;
		bit2 = (DrvPalRAM[(i * 2) + 1] >> 5) & 0x01;
		INT32 g = (bit2 * 1000) + (bit1 * 470) + (bit0 * 270);

		bit0 = (DrvPalRAM[(i * 2) + 1] >> 0) & 0x01;
		bit1 = (DrvPalRAM[(i * 2) + 1] >> 1) & 0x01;
		bit2 = (DrvPalRAM[(i * 2) + 1] >> 2) & 0x01;
		INT32 b = (bit2 * 1000) + (bit1 * 470) + (bit0 * 270);

		DrvPalette[i] = BurnHighCol((r * 255) / 1740, (g * 255) / 1740, (b * 255) / 1740,0);
	}
}

static void draw_layers()
{
	GenericTilesSetClipRaw(0, 256, 0, 256);

	memset (bitmap[0], 0, 256*256*2);
	memset (bitmap[1], 0, 256*256*2);
	memset (bitmap[2], 0, 256*256*2);

	INT32 colbank0 = color_bank[0] & 7;
	INT32 colbank1 =(color_bank[0] >> 4) & 7;
	INT32 colbank2 = color_bank[1] & 7;

	INT32 gfxbank0 = (color_bank[0] & 0x08) << 5;
	INT32 gfxbank1 = (color_bank[0] & 0x80) << 1;
	INT32 gfxbank2 = (color_bank[1] & 0x08) << 5;

	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sx = (offs & 0x1f) * 8;
		INT32 sy = (offs / 32) * 8;

		if (global_flipx) sx = 248 - sx;
		if (global_flipy) sy = 248 - sy;

		if (global_flipy) {
			if (global_flipx) {
				Render8x8Tile_Mask_FlipXY(bitmap[0], DrvVidRAM1[offs] + gfxbank0, sx, sy, colbank0, 3, 0, 0, DrvGfxExp);
				Render8x8Tile_Mask_FlipXY(bitmap[1], DrvVidRAM2[offs] + gfxbank1, sx, sy, colbank1, 3, 0, 0, DrvGfxExp);
				Render8x8Tile_Mask_FlipXY(bitmap[2], DrvVidRAM3[offs] + gfxbank2, sx, sy, colbank2, 3, 0, 0, DrvGfxExp);
			} else {
				Render8x8Tile_Mask_FlipY(bitmap[0], DrvVidRAM1[offs] + gfxbank0, sx, sy, colbank0, 3, 0, 0, DrvGfxExp);
				Render8x8Tile_Mask_FlipY(bitmap[1], DrvVidRAM2[offs] + gfxbank1, sx, sy, colbank1, 3, 0, 0, DrvGfxExp);
				Render8x8Tile_Mask_FlipY(bitmap[2], DrvVidRAM3[offs] + gfxbank2, sx, sy, colbank2, 3, 0, 0, DrvGfxExp);
			}
		} else {
			if (global_flipx) {
				Render8x8Tile_Mask_FlipX(bitmap[0], DrvVidRAM1[offs] + gfxbank0, sx, sy, colbank0, 3, 0, 0, DrvGfxExp);
				Render8x8Tile_Mask_FlipX(bitmap[1], DrvVidRAM2[offs] + gfxbank1, sx, sy, colbank1, 3, 0, 0, DrvGfxExp);
				Render8x8Tile_Mask_FlipX(bitmap[2], DrvVidRAM3[offs] + gfxbank2, sx, sy, colbank2, 3, 0, 0, DrvGfxExp);
			} else {
				Render8x8Tile_Mask(bitmap[0], DrvVidRAM1[offs] + gfxbank0, sx, sy, colbank0, 3, 0, 0, DrvGfxExp);
				Render8x8Tile_Mask(bitmap[1], DrvVidRAM2[offs] + gfxbank1, sx, sy, colbank1, 3, 0, 0, DrvGfxExp);
				Render8x8Tile_Mask(bitmap[2], DrvVidRAM3[offs] + gfxbank2, sx, sy, colbank2, 3, 0, 0, DrvGfxExp);
			}
		}
	}

	GenericTilesClearClipRaw();
}

static void draw_single_sprite(UINT16 *dest, INT32 code, INT32 color, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy)
{
	if (flipy) {
		if (flipx) {
			Render8x8Tile_Mask_FlipXY_Clip(dest, code, sx, sy, color, 3, 0, 0, DrvGfxExp);
		} else {
			Render8x8Tile_Mask_FlipY_Clip(dest, code, sx, sy, color, 3, 0, 0, DrvGfxExp);
		}
	} else {
		if (flipx) {
			Render8x8Tile_Mask_FlipX_Clip(dest, code, sx, sy, color, 3, 0, 0, DrvGfxExp);
		} else {
			Render8x8Tile_Mask_Clip(dest, code, sx, sy, color, 3, 0, 0, DrvGfxExp);
		}
	}
}

static void draw_one_sprite(UINT16 *dest, INT32 code, INT32 color, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy)
{
	if (flipy) {
		if (flipx) {
			draw_single_sprite(dest, code + 3, color, sx + 0, sy + 0, flipx, flipy);
			draw_single_sprite(dest, code + 2, color, sx + 8, sy + 0, flipx, flipy);
			draw_single_sprite(dest, code + 1, color, sx + 0, sy + 8, flipx, flipy);
			draw_single_sprite(dest, code + 0, color, sx + 8, sy + 8, flipx, flipy);
		} else {
			draw_single_sprite(dest, code + 2, color, sx + 0, sy + 0, flipx, flipy);
			draw_single_sprite(dest, code + 3, color, sx + 8, sy + 0, flipx, flipy);
			draw_single_sprite(dest, code + 0, color, sx + 0, sy + 8, flipx, flipy);
			draw_single_sprite(dest, code + 1, color, sx + 8, sy + 8, flipx, flipy);
		}
	} else {
		if (flipx) {
			draw_single_sprite(dest, code + 1, color, sx + 0, sy + 0, flipx, flipy);
			draw_single_sprite(dest, code + 0, color, sx + 8, sy + 0, flipx, flipy);
			draw_single_sprite(dest, code + 3, color, sx + 0, sy + 8, flipx, flipy);
			draw_single_sprite(dest, code + 2, color, sx + 8, sy + 8, flipx, flipy);
		} else {
			draw_single_sprite(dest, code + 0, color, sx + 0, sy + 0, flipx, flipy);
			draw_single_sprite(dest, code + 1, color, sx + 8, sy + 0, flipx, flipy);
			draw_single_sprite(dest, code + 2, color, sx + 0, sy + 8, flipx, flipy);
			draw_single_sprite(dest, code + 3, color, sx + 8, sy + 8, flipx, flipy);
		}
	}
}

static inline INT32 get_sprite_xy(INT32 which, UINT8 * sx, UINT8* sy)
{
	INT32 offs = which * 4;

	*sx =       DrvSprRAM[spriteram_bank + offs + 0] - 1;
	*sy = 240 - DrvSprRAM[spriteram_bank + offs + 1];

	return (*sy < 240);
}

static INT32 check_sprite_sprite_bitpattern(INT32 sx1, INT32 sy1, INT32 which1, INT32 sx2, INT32 sy2, INT32 which2)
{
	INT32 minx, miny, maxx = 16, maxy = 16;

	INT32 offs1 = which1 * 4;
	INT32 offs2 = which2 * 4;

	if (sx1 < sx2)
	{
		sx2 -= sx1;
		sx1 = 0;
		minx = sx2;
	}
	else
	{
		sx1 -= sx2;
		sx2 = 0;
		minx = sx1;
	}

	if (sy1 < sy2)
	{
		sy2 -= sy1;
		sy1 = 0;
		miny = sy2;
	}
	else
	{
		sy1 -= sy2;
		sy2 = 0;
		miny = sy1;
	}

	memset (bitmap[4], 0, 32 * 32 * 2);
	memset (bitmap[5], 0, 32 * 32 * 2);

	GenericTilesSetClipRaw(0, 32, 0, 32);

	INT32 code  = DrvSprRAM[spriteram_bank + offs1 + 3] & 0x7f;
	INT32 flipx = DrvSprRAM[spriteram_bank + offs1 + 2] & 0x01;
	INT32 flipy = DrvSprRAM[spriteram_bank + offs1 + 2] & 0x02;

	draw_one_sprite(bitmap[4], code * 4, 0, sx1, sy1, flipx, flipy);

	code  = DrvSprRAM[spriteram_bank + offs2 + 3] & 0x7f;
	flipx = DrvSprRAM[spriteram_bank + offs2 + 2] & 0x01;
	flipy = DrvSprRAM[spriteram_bank + offs2 + 2] & 0x02;

	draw_one_sprite(bitmap[5], code * 4, 0, sx2, sy2, flipx, flipy);

	GenericTilesClearClipRaw();

	for (INT32 y = miny; y < maxy; y++)
		for (INT32 x = minx; x < maxx; x++)
			if ((bitmap[4][y*32+x] != 0) && (bitmap[5][y*32+x] != 0))
				return 1;

	return 0;
}

static void check_sprite_sprite_collision()
{
	if (video_mode & 0x80)
	{
		for (INT32 which1 = 0; which1 < 0x20; which1++)
		{
			UINT8 sx1, sy1;

			if ((which1 >= 0x10) && (which1 <= 0x17)) continue; // no sprites here

			if (!get_sprite_xy(which1, &sx1, &sy1)) continue;

			for (INT32 which2 = which1 + 1; which2 < 0x20; which2++)
			{
				UINT8 sx2, sy2;

				if ((which2 >= 0x10) && (which2 <= 0x17)) continue;   // no sprites here

				if (!get_sprite_xy(which2, &sx2, &sy2)) continue;

				if ((abs((INT8)sx1 - (INT8)sx2) < 16) && (abs((INT8)sy1 - (INT8)sy2) < 16))
				{
					if (!check_sprite_sprite_bitpattern(sx1, sy1, which1, sx2, sy2, which2))  continue;

					if (which2 == 0x1f)
					{
						INT32 reg = which1 >> 3;
						if (reg == 3)  reg = 2;

						collision_reg[reg] |= (1 << (which1 & 0x07));
					}
					else
					{
						INT32 reg = which2 >> 3;
						if (reg == 3)  reg = 2;

						collision_reg[reg] |= (1 << (which2 & 0x07));
					}
				}
			}
		}
	}
}

static void draw_sprites()
{
	if (video_mode & 0x80)
	{
		for (INT32 sprite = 0x1f; sprite >= 0; sprite--)
		{
			UINT8 sx, sy;
			INT32 which = (sprite - 1) & 0x1f;
			INT32 offs = which * 4;

			if ((which >= 0x10) && (which <= 0x17)) continue;   // no sprites here */

			if (get_sprite_xy(which, &sx, &sy))
			{
				INT32 code = DrvSprRAM[spriteram_bank + offs + 3] & 0x7f;
				INT32 color = 2 * ((color_bank[1] >> 4) & 0x03) + ((DrvSprRAM[spriteram_bank + offs + 2] >> 2) & 0x01);
				INT32 flipx = DrvSprRAM[spriteram_bank + offs + 2] & 0x01;
				INT32 flipy = DrvSprRAM[spriteram_bank + offs + 2] & 0x02;

				if (global_flipx)
				{
					sx = 238 - sx;
					flipx = !flipx;
				}

				if (global_flipy)
				{
					sy = 242 - sy;
					flipy = !flipy;
				}

				code *= 4;

				// note -16 for removing rows
				draw_one_sprite(pTransDraw, code, color, sx, sy - 16, flipx, flipy);
				draw_one_sprite(pTransDraw, code, color, sx - 0x100, sy - 16, flipx, flipy); // wrap
			}
		}
	}
}

static void copyscrollbitmap(UINT16 *dest, UINT16 *src, INT32 rows, INT32 *scrollx, INT32 cols, INT32 *scrolly, INT32 *area, INT32 transparent)
{
	INT32 minx = area[0];
	INT32 maxx = area[1];
	INT32 miny = area[2];
	INT32 maxy = area[3];

	INT32 shifty = 0;

	if (transparent == 0) // hacky - remove top and bottom rows
	{
		if (miny < 16) miny = 16;
		if (maxy > 240) maxy = 240;

		shifty = 16;
	}

	for (INT32 y = miny; y < maxy; y++)
	{
		for (INT32 x = minx; x < maxx; x++)
		{
			UINT8 sx = x - scrollx[y/(256/rows)];
			UINT8 sy = y - scrolly[sx/(256/cols)];

			UINT16 pxl = src[sy * 256 + sx];

			if (pxl != transparent)
				dest[(y - shifty) * 256 + x] = pxl;
		}
	}
}

static void taitosj_copy_layer(INT32 which, INT32 *sprites_on, INT32 *sprite_areas)
{
	static const INT32 fudge1[3] = { 3,  1, -1 };
	static const INT32 fudge2[3] = { 8, 10, 12 };

	INT32 area[4] = { 0, 256, 0, 256 };

	if (video_mode & (1 << (which+4)))
	{
		INT32 scrolly[32];
		INT32 scrollx = scroll[2 * which];

		if (video_mode & 0x01)
			scrollx =  (scrollx & 0xf8) + ((scrollx + fudge1[which]) & 7) + fudge2[which];
		else
			scrollx = -(scrollx & 0xf8) + ((scrollx + fudge1[which]) & 7) + fudge2[which];

		if (video_mode & 0x02)
			for (INT32 i = 0;i < 32;i++)
				scrolly[31 - i] =  DrvColScroll[32 * which + i] + scroll[2 * which + 1];
		else
			for (INT32 i = 0;i < 32;i++)
				scrolly[i]      = -DrvColScroll[32 * which + i] - scroll[2 * which + 1];

		copyscrollbitmap(pTransDraw, bitmap[which], 1, &scrollx, 32, scrolly, area, 0);

		for (INT32 i = 0; i < 0x20; i++)
		{
			if ((i >= 0x10) && (i <= 0x17)) continue; /* no sprites here */

			if (sprites_on[i])
				copyscrollbitmap(bitmap[6+which], bitmap[which], 1, &scrollx, 32, scrolly, sprite_areas + i * 4, -1);
		}
	}
}

static void kikstart_copy_layer(int which, int *sprites_on, INT32 *sprite_areas)
{
	INT32 area[4] = { 0, 256, 0, 256 };

	if (video_mode & (1 << (which+4)))
	{
		INT32 scrollx[32 * 8];

		for (INT32 i = 1; i < 32*8; i++) {
			if (video_mode & 2) {
				switch (which)
				{
					case 0: scrollx[32 * 8 - i] = 0 ;break;
					case 1: scrollx[32 * 8 - i] = DrvZ80RAMC[i] + ((scroll[2 * which] + 0x0a) & 0xff);break;
					case 2: scrollx[32 * 8 - i] = DrvZ80RAMC[0x100 + i] + ((scroll[2 * which] + 0xc) & 0xff);break;
				}
			} else {
				switch (which)
				{
					case 0: scrollx[i] = 0 ;break;
					case 1: scrollx[i] = 0xff - DrvZ80RAMC[i - 1] - ((scroll[2 * which] - 0x10) & 0xff);break;
					case 2: scrollx[i] = 0xff - DrvZ80RAMC[0x100 + i - 1] - ((scroll[2 * which] - 0x12) & 0xff);break;
				}
			}
		}

		INT32 scrolly = scroll[2 * which + 1];

		copyscrollbitmap(pTransDraw, bitmap[which], 256, scrollx, 1, &scrolly, area, 0);

		for (INT32 i = 0; i < 0x20; i++)
		{
			if ((i >= 0x10) && (i <= 0x17)) continue; /* no sprites here */

			if (sprites_on[i])
				copyscrollbitmap(bitmap[6+which], bitmap[which], 256, scrollx, 1, &scrolly, sprite_areas + i * 4, -1);
		}
	}
}

static void copy_layers(void (*copy_func)(INT32 which, INT32 *sprites_on, INT32 *sprite_areas), INT32 *sprites_on, INT32 *sprite_areas)
{
	BurnTransferClear(8 * (color_bank[1] & 7));

	for (INT32 i = 0; i < 4; i++)
	{
		INT32 which = draw_order[video_priority & 0x1f][i];

		if (which == 0)
		{
			draw_sprites();
		}
		else
			copy_func(which - 1, sprites_on, sprite_areas);
	}
}

static INT32 check_sprite_layer_bitpattern(int which, INT32 *sprite_areas)
{
	INT32 offs = which * 4;
	INT32 result = 0;  // no collisions

	INT32 check_layer_1 = video_mode & 0x10;
	INT32 check_layer_2 = video_mode & 0x20;
	INT32 check_layer_3 = video_mode & 0x40;

	INT32 minx = sprite_areas[which*4+0];
	INT32 miny = sprite_areas[which*4+2];
	INT32 maxx = sprite_areas[which*4+1] + 1;
	INT32 maxy = sprite_areas[which*4+3] + 1;

	INT32 code = DrvSprRAM[spriteram_bank + offs + 3] & 0x7f;
	INT32 flip_x = (DrvSprRAM[spriteram_bank + offs + 2] & 0x01) ^ global_flipx;
	INT32 flip_y = (DrvSprRAM[spriteram_bank + offs + 2] & 0x02) ^ global_flipy;

	GenericTilesSetClipRaw(0, 32, 0, 32);
	memset (bitmap[4], 0, 32*32*2);
	draw_one_sprite(bitmap[4], code*4, 0, 0, 0, flip_x, flip_y);
	GenericTilesClearClipRaw();

	for (INT32 y = miny; y < maxy; y++) {
		for (INT32 x = minx; x < maxx; x++) {
			if (bitmap[4][(y - miny) * 32 + (x - minx)] != 0) // is there anything to check for ?
			{
				if (check_layer_1 && bitmap[6][y*256+x] != 0) result |= 0x01;  // collided with layer 1
				if (check_layer_2 && bitmap[7][y*256+x] != 0) result |= 0x02;  // collided with layer 2
				if (check_layer_3 && bitmap[8][y*256+x] != 0) result |= 0x04;  // collided with layer 3
			}
		}
	}

	return result;
}

static void check_sprite_layer_collision(int *sprites_on, INT32 *sprite_areas)
{
	if (video_mode & 0x80)
	{
		for (INT32 which = 0; which < 0x20; which++)
		{
			if ((which >= 0x10) && (which <= 0x17)) continue;

			if (sprites_on[which])
				collision_reg[3] |= check_sprite_layer_bitpattern(which, sprite_areas);
		}
	}
}

static void calculate_sprite_areas(int *sprites_on, INT32 *sprite_areas)
{
	INT32 width = 256;
	INT32 height = 256;

	for (INT32 which = 0; which < 0x20; which++)
	{
		UINT8 sx, sy;

		if ((which >= 0x10) && (which <= 0x17)) continue;

		if (get_sprite_xy(which, &sx, &sy))
		{
			int minx, miny, maxx, maxy;

			if (video_mode & 0x01)
				sx = 238 - sx;

			if (video_mode & 0x02)
				sy = 242 - sy;

			minx = sx;
			miny = sy;

			maxx = minx + 15;
			maxy = miny + 15;

			if (minx < 0) minx = 0;
			if (miny < 0) miny = 0;
			if (maxx >= width - 1)
				maxx = width - 1;
			if (maxy >= height - 1)
				maxy = height - 1;

			sprite_areas[which*4+0] = minx;
			sprite_areas[which*4+1] = maxx;
			sprite_areas[which*4+2] = miny;
			sprite_areas[which*4+3] = maxy;

			sprites_on[which] = 1;
		}
		else
			sprites_on[which] = 0;
	}
}

static INT32 DrvDraw()
{
	//if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	//}

	INT32 sprites_on[0x20];
	INT32 sprite_areas[0x20*4];  // minx, maxx, miny, maxy

	global_flipx = video_mode & 0x01;
	global_flipy = video_mode & 0x02;
	spriteram_bank = (video_mode & 0x04) ? 0x80 : 0x00;

	draw_layers();

	calculate_sprite_areas(sprites_on, sprite_areas);

	copy_layers(taitosj_copy_layer, sprites_on, sprite_areas);

	check_sprite_sprite_collision();

	check_sprite_layer_collision(sprites_on, sprite_areas);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 KikstartDraw()
{
	//if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	//}

	INT32 sprites_on[0x20];
	INT32 sprite_areas[0x20*4];  // minx, maxx, miny, maxy

	global_flipx = video_mode & 0x01;
	global_flipy = video_mode & 0x02;
	spriteram_bank = (video_mode & 0x04) ? 0x80 : 0x00;

	draw_layers();

	calculate_sprite_areas(sprites_on, sprite_areas);

	copy_layers(kikstart_copy_layer, sprites_on, sprite_areas);

	check_sprite_sprite_collision();

	check_sprite_layer_collision(sprites_on, sprite_areas);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	ZetNewFrame();
	m6805NewFrame();

	{
		UINT8 Last5 = DrvInputs[5]; // kikstart gear shifter
		UINT8 Last6 = DrvInputs[6]; // kikstart gear shifter

		memset (DrvInputs, 0xff, 5);

		DrvInputs[2] ^= input2_xor; // protection
		DrvInputs[3] &= ~coin_state;
		DrvInputs[4] &= 0x0f;

		DrvInputs[5] = 0; // gear shifter
		DrvInputs[6] = 0; // gear shifter

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
			DrvInputs[4] ^= (DrvJoy5[i] & 1) << i;
			DrvInputs[5] ^= (DrvJoyF0[i] & 1) << i; // Fake Inputs for kikstart
			DrvInputs[6] ^= (DrvJoyF1[i] & 1) << i;
		}

		if (is_kikstart) {
			if (DrvInputs[5]&1 && ~Last5&1) if (kikstart_gears[0] > 0) kikstart_gears[0]--;
			if (DrvInputs[5]&2 && ~Last5&2) if (kikstart_gears[0] < 2) kikstart_gears[0]++;

			if (DrvInputs[6]&1 && ~Last6&1) if (kikstart_gears[1] > 0) kikstart_gears[1]--;
			if (DrvInputs[6]&2 && ~Last6&2) if (kikstart_gears[1] < 2) kikstart_gears[1]++;

			DrvInputs[3] = (DrvInputs[3] & ~(3|8)) | (kikstart_gears[0] ^ 2) | (kikstart_gears[0] >> 1); // 0,1,2 -> 2,3,1
			DrvInputs[4] = (DrvInputs[4] & ~(3|8)) | (kikstart_gears[1] ^ 2) | (kikstart_gears[1] >> 1);
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[3] = { 4000000 / 60, 3000000 / 60, 3000000 / 4 / 60 };
	INT32 nCyclesDone[3] = { nCyclesExtra[0], nCyclesExtra[1], nCyclesExtra[2] };

	m6805Open(0);
	m6805Idle(nCyclesExtra[2]); // _SYNCINT & outside-frame syncing used

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		CPU_RUN(0, Zet);
		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(1);
		CPU_RUN(1, Zet);
		if (sound_irq_timer == 419) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD); // 36HZ
		ZetClose();

		if (has_mcu) {
			ZetOpen(0);
			CPU_RUN_SYNCINT(2, m6805);
			ZetClose();
		}

		sound_irq_timer++;
		if (sound_irq_timer >= 420)
			sound_irq_timer = 0;
	}

	m6805Close();

	nCyclesExtra[0] = nCyclesDone[0] - nCyclesTotal[0];
	nCyclesExtra[1] = nCyclesDone[1] - nCyclesTotal[1];
	nCyclesExtra[2] = m6805TotalCycles() - nCyclesTotal[2];

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
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
		*pnMin = 0x029702;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);
		m6805Scan(nAction);

		AY8910Scan(nAction, pnMin);
		DACScan(nAction, pnMin);
		BurnWatchdogScan(nAction);

		SCAN_VAR(video_priority);
		SCAN_VAR(scroll);
		SCAN_VAR(color_bank);
		SCAN_VAR(gfxpointer);
		SCAN_VAR(soundlatch);
		SCAN_VAR(video_mode);
		SCAN_VAR(collision_reg);
		SCAN_VAR(rom_bank);
		SCAN_VAR(sound_nmi_disable);
		SCAN_VAR(input_port_data);
		SCAN_VAR(protection_value);
		SCAN_VAR(dac_volume);
		SCAN_VAR(dac_out_data);
		SCAN_VAR(sound_irq_timer);
		SCAN_VAR(toz80);
		SCAN_VAR(fromz80);
		SCAN_VAR(mcu_address);
		SCAN_VAR(portA_in);
		SCAN_VAR(portA_out);
		SCAN_VAR(zready);
		SCAN_VAR(zaccept);
		SCAN_VAR(busreq);
		SCAN_VAR(kikstart_gears);

		SCAN_VAR(nCyclesExtra);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		bankswitch(rom_bank);
		ZetClose();

		for (INT32 i = 0; i < 0x3000; i++) {
			ram_decode(i);
		}
	}

	return 0;
}


// Space Seeker

static struct BurnRomInfo spaceskrRomDesc[] = {
	{ "eb01",				0x1000, 0x92345b05, 1 | BRF_PRG | BRF_ESS }, //  0 Main Z80 Code
	{ "eb02",				0x1000, 0xa3e21420, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "eb03",				0x1000, 0xa077c52f, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "eb04",				0x1000, 0x440030cf, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "eb05",				0x1000, 0xb0d396ab, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "eb06",				0x1000, 0x371d2f7a, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "eb07",				0x1000, 0x13e667c4, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "eb08",				0x1000, 0xf2e84015, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "eb13",				0x1000, 0x192f6536, 2 | BRF_PRG | BRF_ESS }, //  8 Sound Z80 Code
	{ "eb14",				0x1000, 0xd04d0a21, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "eb15",				0x1000, 0x88194305, 2 | BRF_PRG | BRF_ESS }, // 10

	{ "eb09",				0x1000, 0x77af540e, 3 | BRF_GRA },           // 11 Graphics data
	{ "eb10",				0x1000, 0xb10073de, 3 | BRF_GRA },           // 12
	{ "eb11",				0x1000, 0xc7954bd1, 3 | BRF_GRA },           // 13
	{ "eb12",				0x1000, 0xcd6c087b, 3 | BRF_GRA },           // 14

	{ "eb16.22",			0x0100, 0xb833b5ea, 4 | BRF_GRA },           // 15 Layer Priority
};

STD_ROM_PICK(spaceskr)
STD_ROM_FN(spaceskr)

static INT32 spaceskrInit()
{
	return CommonInit(0, 0, 0);
}

struct BurnDriver BurnDrvSpaceskr = {
	"spaceskr", NULL, NULL, NULL, "1981",
	"Space Seeker\0", NULL, "Taito Corporation", "Taito SJ System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_RUNAHEAD_DRAWSYNC | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_SHOOT, 0,
	NULL, spaceskrRomInfo, spaceskrRomName, NULL, NULL, NULL, NULL, TwoButtonInputInfo, SpaceskrDIPInfo,
	spaceskrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	256, 224, 4, 3
};


// Space Cruiser

static struct BurnRomInfo spacecrRomDesc[] = {
	{ "cg01.69",			0x1000, 0x2fe28b71, 1 | BRF_PRG | BRF_ESS }, //  0 Main Z80 Code
	{ "cg02.68",			0x1000, 0x88f4f856, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "cg03.67",			0x1000, 0x2223319c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "cg04.66",			0x1000, 0x4daeb8b5, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "cg05.65",			0x1000, 0xcdc40ca0, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "cg06.64",			0x1000, 0x2cc6b4c0, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "cg07.55",			0x1000, 0xe4c8780a, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "cg08.54",			0x1000, 0x2c23ff4d, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "cg09.53",			0x1000, 0x3c8bb95e, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "cg10.52",			0x1000, 0x0ff17fce, 1 | BRF_PRG | BRF_ESS }, //  9

	{ "cg17.70",			0x1000, 0x53486204, 2 | BRF_PRG | BRF_ESS }, // 10 Sound Z80 Code
	{ "cg18.71",			0x1000, 0xd1acf96c, 2 | BRF_PRG | BRF_ESS }, // 11
	{ "cg19.72",			0x1000, 0xffd27215, 2 | BRF_PRG | BRF_ESS }, // 12

	{ "cg11.1",				0x1000, 0x1e4ae527, 3 | BRF_GRA },           // 13 Graphics data
	{ "cg12.2",				0x1000, 0xaa57b616, 3 | BRF_GRA },           // 14
	{ "cg13.3",				0x1000, 0x945a1b69, 3 | BRF_GRA },           // 15
	{ "cg14.4",				0x1000, 0x1a29d06b, 3 | BRF_GRA },           // 16
	{ "cg15.5",				0x1000, 0x656f9713, 3 | BRF_GRA },           // 17
	{ "cg16.6",				0x1000, 0xe2c0d585, 3 | BRF_GRA },           // 18

	{ "eb16.22",			0x0100, 0xb833b5ea, 4 | BRF_GRA },           // 19 Layer Priority
};

STD_ROM_PICK(spacecr)
STD_ROM_FN(spacecr)

static INT32 spacecrInit()
{
	return CommonInit(1, 0, 0);
}

struct BurnDriver BurnDrvSpacecr = {
	"spacecr", NULL, NULL, NULL, "1981",
	"Space Cruiser\0", NULL, "Taito Corporation", "Taito SJ System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_RUNAHEAD_DRAWSYNC | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_SHOOT, 0,
	NULL, spacecrRomInfo, spacecrRomName, NULL, NULL, NULL, NULL, SpacecrInputInfo, SpacecrDIPInfo,
	spacecrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Jungle King (Japan)

static struct BurnRomInfo junglekRomDesc[] = {
	{ "kn21-1.bin",			0x1000, 0x45f55d30, 1 | BRF_PRG | BRF_ESS }, //  0 Main Z80 Code
	{ "kn22-1.bin",			0x1000, 0x07cc9a21, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "kn43.bin",			0x1000, 0xa20e5a48, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "kn24.bin",			0x1000, 0x19ea7f83, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "kn25.bin",			0x1000, 0x844365ea, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "kn46.bin",			0x1000, 0x27a95fd5, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "kn47.bin",			0x1000, 0x5c3199e0, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "kn28.bin",			0x1000, 0x194a2d09, 9 | BRF_PRG | BRF_ESS }, //  7
	{ "kn60.bin",			0x1000, 0x1a9c0a26, 1 | BRF_PRG | BRF_ESS }, //  8

	{ "kn37.bin",			0x1000, 0xdee7f5d4, 2 | BRF_PRG | BRF_ESS }, //  9 Sound Z80 Code
	{ "kn38.bin",			0x1000, 0xbffd3d21, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "kn59-1.bin",			0x1000, 0xcee485fc, 2 | BRF_PRG | BRF_ESS }, // 11

	{ "kn29.bin",			0x1000, 0x8f83c290, 3 | BRF_GRA },           // 12 Graphics data
	{ "kn30.bin",			0x1000, 0x89fd19f1, 3 | BRF_GRA },           // 13
	{ "kn51.bin",			0x1000, 0x70e8fc12, 3 | BRF_GRA },           // 14
	{ "kn52.bin",			0x1000, 0xbcbac1a3, 3 | BRF_GRA },           // 15
	{ "kn53.bin",			0x1000, 0xb946c87d, 3 | BRF_GRA },           // 16
	{ "kn34.bin",			0x1000, 0x320db2e1, 3 | BRF_GRA },           // 17
	{ "kn55.bin",			0x1000, 0x70aef58f, 3 | BRF_GRA },           // 18
	{ "kn56.bin",			0x1000, 0x932eb667, 3 | BRF_GRA },           // 19

	{ "eb16.22",			0x0100, 0xb833b5ea, 4 | BRF_GRA },           // 20 Layer Priority
};

STD_ROM_PICK(junglek)
STD_ROM_FN(junglek)

static INT32 junglekInit()
{
	return CommonInit(0, 0, 0);
}

struct BurnDriver BurnDrvJunglek = {
	"junglek", NULL, NULL, NULL, "1982",
	"Jungle King (Japan)\0", NULL, "Taito Corporation", "Taito SJ System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_RUNAHEAD_DRAWSYNC | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, junglekRomInfo, junglekRomName, NULL, NULL, NULL, NULL, OneButtonInputInfo, JunglekDIPInfo,
	junglekInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	256, 224, 4, 3
};


// Jungle King (alternate sound)

static struct BurnRomInfo junglekasRomDesc[] = {
	{ "kn21-1.bin",			0x1000, 0x45f55d30, 1 | BRF_PRG | BRF_ESS }, //  0 Main Z80 Code
	{ "kn22-1.bin",			0x1000, 0x07cc9a21, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "kn43.bin",			0x1000, 0xa20e5a48, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "kn24.bin",			0x1000, 0x19ea7f83, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "kn25.bin",			0x1000, 0x844365ea, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "kn46.bin",			0x1000, 0x27a95fd5, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "kn47.bin",			0x1000, 0x5c3199e0, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "kn28.bin",			0x1000, 0x194a2d09, 9 | BRF_PRG | BRF_ESS }, //  7
	{ "kn60.bin",			0x1000, 0x1a9c0a26, 1 | BRF_PRG | BRF_ESS }, //  8

	{ "kn-a17.bin",			0x1000, 0x62f6763a, 2 | BRF_PRG | BRF_ESS }, //  9 Sound Z80 Code
	{ "kn-a18.bin",			0x1000, 0x8a813a7c, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "kn-a19.bin",			0x1000, 0xabbe4ae5, 2 | BRF_PRG | BRF_ESS }, // 11

	{ "kn29.bin",			0x1000, 0x8f83c290, 3 | BRF_GRA },           // 12 Graphics data
	{ "kn30.bin",			0x1000, 0x89fd19f1, 3 | BRF_GRA },           // 13
	{ "kn51.bin",			0x1000, 0x70e8fc12, 3 | BRF_GRA },           // 14
	{ "kn52.bin",			0x1000, 0xbcbac1a3, 3 | BRF_GRA },           // 15
	{ "kn53.bin",			0x1000, 0xb946c87d, 3 | BRF_GRA },           // 16
	{ "kn34.bin",			0x1000, 0x320db2e1, 3 | BRF_GRA },           // 17
	{ "kn55.bin",			0x1000, 0x70aef58f, 3 | BRF_GRA },           // 18
	{ "kn56.bin",			0x1000, 0x932eb667, 3 | BRF_GRA },           // 19

	{ "eb16.22",			0x0100, 0xb833b5ea, 4 | BRF_GRA },           // 20 Layer Priority
};

STD_ROM_PICK(junglekas)
STD_ROM_FN(junglekas)

struct BurnDriver BurnDrvJunglekas = {
	"junglekas", "junglek", NULL, NULL, "1982",
	"Jungle King (alternate sound)\0", NULL, "Taito Corporation", "Taito SJ System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_RUNAHEAD_DRAWSYNC | BDF_CLONE | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, junglekasRomInfo, junglekasRomName, NULL, NULL, NULL, NULL, OneButtonInputInfo, JunglekDIPInfo,
	junglekInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	256, 224, 4, 3
};


// Jungle Boy (bootleg)

static struct BurnRomInfo junglebyRomDesc[] = {
	{ "j1.bin",				0x1000, 0x6f2ac11f, 1 | BRF_PRG | BRF_ESS }, //  0 Main Z80 Code
	{ "j2.bin",				0x1000, 0x07cc9a21, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "j3.bin",				0x1000, 0xa20e5a48, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "j4.bin",				0x1000, 0x19ea7f83, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "j5.bin",				0x1000, 0x844365ea, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "j6.bin",				0x1000, 0x27a95fd5, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "j7.bin",				0x1000, 0x5c3199e0, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "j8.bin",				0x1000, 0x895e5708, 9 | BRF_PRG | BRF_ESS }, //  7
	{ "j9.bin",				0x1000, 0x1a9c0a26, 1 | BRF_PRG | BRF_ESS }, //  8

	{ "j10.bin",			0x1000, 0xdee7f5d4, 2 | BRF_PRG | BRF_ESS }, //  9 Sound Z80 Code
	{ "j11.bin",			0x1000, 0xbffd3d21, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "j12.bin",			0x1000, 0xcee485fc, 2 | BRF_PRG | BRF_ESS }, // 11

	{ "j13.bin",			0x1000, 0x8f83c290, 3 | BRF_GRA },           // 12 Graphics data
	{ "j14.bin",			0x1000, 0x89fd19f1, 3 | BRF_GRA },           // 13
	{ "j15.bin",			0x1000, 0x70e8fc12, 3 | BRF_GRA },           // 14
	{ "j16.bin",			0x1000, 0xbcbac1a3, 3 | BRF_GRA },           // 15
	{ "j17.bin",			0x1000, 0xb946c87d, 3 | BRF_GRA },           // 16
	{ "j18.bin",			0x1000, 0x320db2e1, 3 | BRF_GRA },           // 17
	{ "j19.bin",			0x1000, 0x8438eb41, 3 | BRF_GRA },           // 18
	{ "j20.bin",			0x1000, 0x932eb667, 3 | BRF_GRA },           // 19

	{ "eb16.22",			0x0100, 0xb833b5ea, 4 | BRF_GRA },           // 20 Layer Priority
};

STD_ROM_PICK(jungleby)
STD_ROM_FN(jungleby)

struct BurnDriver BurnDrvJungleby = {
	"jungleby", "junglek", NULL, NULL, "1982",
	"Jungle Boy (bootleg)\0", NULL, "bootleg", "Taito SJ System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_RUNAHEAD_DRAWSYNC | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, junglebyRomInfo, junglebyRomName, NULL, NULL, NULL, NULL, OneButtonInputInfo, JunglekDIPInfo,
	junglekInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	256, 224, 4, 3
};


// Jungle King (Japan, earlier)

static struct BurnRomInfo junglekj2RomDesc[] = {
	{ "kn41.bin",			0x1000, 0x7e4cd631, 1 | BRF_PRG | BRF_ESS }, //  0 Main Z80 Code
	{ "kn42.bin",			0x1000, 0xbade53af, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "kn43.bin",			0x1000, 0xa20e5a48, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "kn44.bin",			0x1000, 0x44c770d3, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "kn45.bin",			0x1000, 0xf60a3d06, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "kn46.bin",			0x1000, 0x27a95fd5, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "kn47.bin",			0x1000, 0x5c3199e0, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "kn48.bin",			0x1000, 0xe690b36e, 9 | BRF_PRG | BRF_ESS }, //  7
	{ "kn60.bin",			0x1000, 0x1a9c0a26, 1 | BRF_PRG | BRF_ESS }, //  8

	{ "kn57-1.bin",			0x1000, 0x62f6763a, 2 | BRF_PRG | BRF_ESS }, //  9 Sound Z80 Code
	{ "kn58-1.bin",			0x1000, 0x9ef46c7f, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "kn59-1.bin",			0x1000, 0xcee485fc, 2 | BRF_PRG | BRF_ESS }, // 11

	{ "kn49.bin",			0x1000, 0xfe275213, 3 | BRF_GRA },           // 12 Graphics data
	{ "kn50.bin",			0x1000, 0xd9f93c55, 3 | BRF_GRA },           // 13
	{ "kn51.bin",			0x1000, 0x70e8fc12, 3 | BRF_GRA },           // 14
	{ "kn52.bin",			0x1000, 0xbcbac1a3, 3 | BRF_GRA },           // 15
	{ "kn53.bin",			0x1000, 0xb946c87d, 3 | BRF_GRA },           // 16
	{ "kn54.bin",			0x1000, 0xf757d8f0, 3 | BRF_GRA },           // 17
	{ "kn55.bin",			0x1000, 0x70aef58f, 3 | BRF_GRA },           // 18
	{ "kn56.bin",			0x1000, 0x932eb667, 3 | BRF_GRA },           // 19

	{ "eb16.22",			0x0100, 0xb833b5ea, 4 | BRF_GRA },           // 20 Layer Priority
};

STD_ROM_PICK(junglekj2)
STD_ROM_FN(junglekj2)

struct BurnDriver BurnDrvJunglekj2 = {
	"junglekj2", "junglek", NULL, NULL, "1982",
	"Jungle King (Japan, earlier)\0", NULL, "Taito Corporation", "Taito SJ System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_RUNAHEAD_DRAWSYNC | BDF_CLONE | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, junglekj2RomInfo, junglekj2RomName, NULL, NULL, NULL, NULL, OneButtonInputInfo, JunglekDIPInfo,
	junglekInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	256, 224, 4, 3
};


// Jungle King (Japan, earlier, alt)

static struct BurnRomInfo junglekj2aRomDesc[] = {
	{ "kn41.bin",			0x1000, 0x7e4cd631, 1 | BRF_PRG | BRF_ESS }, //  0 Main Z80 Code
	{ "kn42.bin",			0x1000, 0xbade53af, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "kn43.bin",			0x1000, 0xa20e5a48, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "kn44.bin",			0x1000, 0x44c770d3, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "kn45.bin",			0x1000, 0xf60a3d06, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "kn26.bin",			0x1000, 0x4b5adca2, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "kn27.bin",			0x1000, 0x5c3199e0, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "kn48.bin",			0x1000, 0xe690b36e, 9 | BRF_PRG | BRF_ESS }, //  7
	{ "kn60.bin",			0x1000, 0x1a9c0a26, 1 | BRF_PRG | BRF_ESS }, //  8

	{ "kn37.bin",			0x1000, 0x60d13095, 2 | BRF_PRG | BRF_ESS }, //  9 Sound Z80 Code
	{ "kn38.bin",			0x1000, 0x6950413d, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "kn59.bin",			0x1000, 0xcee485fc, 2 | BRF_PRG | BRF_ESS }, // 11

	{ "kn49.bin",			0x1000, 0xfe275213, 3 | BRF_GRA },           // 12 Graphics data
	{ "kn50.bin",			0x1000, 0xd9f93c55, 3 | BRF_GRA },           // 13
	{ "kn51.bin",			0x1000, 0x70e8fc12, 3 | BRF_GRA },           // 14
	{ "kn52.bin",			0x1000, 0xbcbac1a3, 3 | BRF_GRA },           // 15
	{ "kn53.bin",			0x1000, 0xb946c87d, 3 | BRF_GRA },           // 16
	{ "kn54.bin",			0x1000, 0xf757d8f0, 3 | BRF_GRA },           // 17
	{ "kn55.bin",			0x1000, 0x70aef58f, 3 | BRF_GRA },           // 18
	{ "kn56.bin",			0x1000, 0x932eb667, 3 | BRF_GRA },           // 19

	{ "eb16.22",			0x0100, 0xb833b5ea, 4 | BRF_GRA },           // 20 Layer Priority
};

STD_ROM_PICK(junglekj2a)
STD_ROM_FN(junglekj2a)

struct BurnDriver BurnDrvJunglekj2a = {
	"junglekj2a", "junglek", NULL, NULL, "1982",
	"Jungle King (Japan, earlier, alt)\0", NULL, "Taito Corporation", "Taito SJ System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_RUNAHEAD_DRAWSYNC | BDF_CLONE | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, junglekj2aRomInfo, junglekj2aRomName, NULL, NULL, NULL, NULL, OneButtonInputInfo, JunglekDIPInfo,
	junglekInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	256, 224, 4, 3
};


// Jungle Hunt (US)

static struct BurnRomInfo junglehRomDesc[] = {
	{ "kn41a",				0x1000, 0x6bf118d8, 1 | BRF_PRG | BRF_ESS }, //  0 Main Z80 Code
	{ "kn42.bin",			0x1000, 0xbade53af, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "kn43.bin",			0x1000, 0xa20e5a48, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "kn44.bin",			0x1000, 0x44c770d3, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "kn45.bin",			0x1000, 0xf60a3d06, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "kn46a",				0x1000, 0xac89c155, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "kn47.bin",			0x1000, 0x5c3199e0, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "kn48a",				0x1000, 0xef80e931, 9 | BRF_PRG | BRF_ESS }, //  7
	{ "kn60.bin",			0x1000, 0x1a9c0a26, 1 | BRF_PRG | BRF_ESS }, //  8

	{ "kn57-1.bin",			0x1000, 0x62f6763a, 2 | BRF_PRG | BRF_ESS }, //  9 Sound Z80 Code
	{ "kn58-1.bin",			0x1000, 0x9ef46c7f, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "kn59-1.bin",			0x1000, 0xcee485fc, 2 | BRF_PRG | BRF_ESS }, // 11

	{ "kn49a",				0x1000, 0xb139e792, 3 | BRF_GRA },           // 12 Graphics data
	{ "kn50a",				0x1000, 0x1046019f, 3 | BRF_GRA },           // 13
	{ "kn51a",				0x1000, 0xda50c8a4, 3 | BRF_GRA },           // 14
	{ "kn52a",				0x1000, 0x0444f06c, 3 | BRF_GRA },           // 15
	{ "kn53a",				0x1000, 0x6a17803e, 3 | BRF_GRA },           // 16
	{ "kn54a",				0x1000, 0xd41428c7, 3 | BRF_GRA },           // 17
	{ "kn55.bin",			0x1000, 0x70aef58f, 3 | BRF_GRA },           // 18
	{ "kn56a",				0x1000, 0x679c1101, 3 | BRF_GRA },           // 19

	{ "eb16.22",			0x0100, 0xb833b5ea, 4 | BRF_GRA },           // 20 Layer Priority
};

STD_ROM_PICK(jungleh)
STD_ROM_FN(jungleh)

struct BurnDriver BurnDrvJungleh = {
	"jungleh", "junglek", NULL, NULL, "1982",
	"Jungle Hunt (US)\0", NULL, "Taito America Corporation", "Taito SJ System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_RUNAHEAD_DRAWSYNC | BDF_CLONE | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, junglehRomInfo, junglehRomName, NULL, NULL, NULL, NULL, OneButtonInputInfo, JunglekDIPInfo,
	junglekInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	256, 224, 4, 3
};


// Jungle Hunt (Brazil)

static struct BurnRomInfo junglehbrRomDesc[] = {
	{ "ic1.bin",			0x2000, 0x3255a10e, 1 | BRF_PRG | BRF_ESS }, //  0 Main Z80 Code
	{ "ic2.bin",			0x2000, 0x8482bc63, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ic3.bin",			0x2000, 0x1abc661d, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "kn47.bin",			0x1000, 0x5c3199e0, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "kn48a",				0x1000, 0xef80e931, 9 | BRF_PRG | BRF_ESS }, //  4
	{ "kn60.bin",			0x1000, 0x1a9c0a26, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "kn37.bin",			0x1000, 0xdee7f5d4, 2 | BRF_PRG | BRF_ESS }, //  6 Sound Z80 Code
	{ "kn38.bin",			0x1000, 0xbffd3d21, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "kn59-1.bin",			0x1000, 0xcee485fc, 2 | BRF_PRG | BRF_ESS }, //  8

	{ "kn29.bin",			0x1000, 0x8f83c290, 3 | BRF_GRA },           //  9 Graphics data
	{ "kn30.bin",			0x1000, 0x89fd19f1, 3 | BRF_GRA },           // 10
	{ "kn51.bin",			0x1000, 0x70e8fc12, 3 | BRF_GRA },           // 11
	{ "kn52.bin",			0x1000, 0xbcbac1a3, 3 | BRF_GRA },           // 12
	{ "kn53.bin",			0x1000, 0xb946c87d, 3 | BRF_GRA },           // 13
	{ "kn34.bin",			0x1000, 0x320db2e1, 3 | BRF_GRA },           // 14
	{ "kn55.bin",			0x1000, 0x70aef58f, 3 | BRF_GRA },           // 15
	{ "kn56.bin",			0x1000, 0x932eb667, 3 | BRF_GRA },           // 16

	{ "eb16.22",			0x0100, 0xb833b5ea, 4 | BRF_GRA },           // 17 Layer Priority
};

STD_ROM_PICK(junglehbr)
STD_ROM_FN(junglehbr)

static INT32 junglehbrInit()
{
	return CommonInit(0, 0xfc, 0);
}

struct BurnDriver BurnDrvJunglehbr = {
	"junglehbr", "junglek", NULL, NULL, "1983",
	"Jungle Hunt (Brazil)\0", NULL, "Taito do Brasil", "Taito SJ System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_RUNAHEAD_DRAWSYNC | BDF_CLONE | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, junglehbrRomInfo, junglehbrRomName, NULL, NULL, NULL, NULL, OneButtonInputInfo, JunglekDIPInfo,
	junglehbrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	256, 224, 4, 3
};


// Jungle Hunt (US, alternate set)
// This is a mix of ROMs from 41a to 50a in "jungleh" and from 51 to 60 in "junglekj2"
// f205v id 1550

static struct BurnRomInfo junglehaRomDesc[] = {
	{ "kn41a",				0x1000, 0x6bf118d8, 1 | BRF_PRG | BRF_ESS }, //  0 Main Z80 Code
	{ "kn42.bin",			0x1000, 0xbade53af, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "kn43.bin",			0x1000, 0xa20e5a48, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "kn44.bin",			0x1000, 0x44c770d3, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "kn45.bin",			0x1000, 0xf60a3d06, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "kn46a",				0x1000, 0xac89c155, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "kn47.bin",			0x1000, 0x5c3199e0, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "kn48a",				0x1000, 0xef80e931, 9 | BRF_PRG | BRF_ESS }, //  7
	{ "kn60.bin",			0x1000, 0x1a9c0a26, 1 | BRF_PRG | BRF_ESS }, //  8

	{ "kn57-1.bin",			0x1000, 0x62f6763a, 2 | BRF_PRG | BRF_ESS }, //  9 Sound Z80 Code
	{ "kn58-1.bin",			0x1000, 0x9ef46c7f, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "kn59-1.bin",			0x1000, 0xcee485fc, 2 | BRF_PRG | BRF_ESS }, // 11

	{ "kn49a",				0x1000, 0xb139e792, 3 | BRF_GRA },           // 12 Graphics data
	{ "kn50a",				0x1000, 0x1046019f, 3 | BRF_GRA },           // 13
	{ "kn51.bin",			0x1000, 0x70e8fc12, 3 | BRF_GRA },           // 14
	{ "kn52.bin",			0x1000, 0xbcbac1a3, 3 | BRF_GRA },           // 15
	{ "kn53.bin",			0x1000, 0xb946c87d, 3 | BRF_GRA },           // 16
	{ "kn54.bin",			0x1000, 0xf757d8f0, 3 | BRF_GRA },           // 17
	{ "kn55.bin",			0x1000, 0x70aef58f, 3 | BRF_GRA },           // 18
	{ "kn56.bin",			0x1000, 0x932eb667, 3 | BRF_GRA },           // 19

	{ "eb16.22",			0x0100, 0xb833b5ea, 4 | BRF_GRA },           // 20 Layer Priority
};

STD_ROM_PICK(jungleha)
STD_ROM_FN(jungleha)

struct BurnDriver BurnDrvJungleha = {
	"jungleha", "junglek", NULL, NULL, "1982",
	"Jungle Hunt (US, alternate set)\0", NULL, "Taito America Corp", "Taito SJ System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_RUNAHEAD_DRAWSYNC | BDF_CLONE | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, junglehaRomInfo, junglehaRomName, NULL, NULL, NULL, NULL, OneButtonInputInfo, JunglekDIPInfo,
	junglekInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	256, 224, 4, 3
};


// Pirate Pete

static struct BurnRomInfo piratpetRomDesc[] = {
	{ "pp0p_ic.69",			0x1000, 0x8287dbc2, 1 | BRF_PRG | BRF_ESS }, //  0 Main Z80 Code
	{ "pp1p_ic.68",			0x1000, 0x27a90850, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "pp2p_ic.67",			0x1000, 0xd224fa85, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "pp3p_ic.66",			0x1000, 0x2c900874, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "pp4p_ic.65",			0x1000, 0x1aed98d9, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "pp5p_ic.64",			0x1000, 0x09c3aacd, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "pp6p_ic.55",			0x1000, 0xbdeed702, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "pp7p_ic.54",			0x1000, 0x5f36d082, 9 | BRF_PRG | BRF_ESS }, //  7
	{ "pp7b_ic.52",			0x1000, 0xbbc38b03, 1 | BRF_PRG | BRF_ESS }, //  8

	{ "pp05_ic.70",			0x1000, 0xdcb5eb9d, 2 | BRF_PRG | BRF_ESS }, //  9 Sound Z80 Code
	{ "pp15_ic.71",			0x1000, 0x3123dbe1, 2 | BRF_PRG | BRF_ESS }, // 10

	{ "pp0e_ic.1",			0x1000, 0xaceaf79b, 3 | BRF_GRA },           // 11 Graphics data
	{ "pp1e_ic.2",			0x1000, 0xac148214, 3 | BRF_GRA },           // 12
	{ "pp2e_ic.3",			0x1000, 0x108194d2, 3 | BRF_GRA },           // 13
	{ "pp3e_ic.4",			0x1000, 0x621b0da1, 3 | BRF_GRA },           // 14
	{ "pp4e_ic.5",			0x1000, 0xe9826d90, 3 | BRF_GRA },           // 15
	{ "pp5e_ic.6",			0x1000, 0xfe0d38c6, 3 | BRF_GRA },           // 16
	{ "pp6e_ic.7",			0x1000, 0x2cfd127b, 3 | BRF_GRA },           // 17
	{ "pp7e_ic.8",			0x1000, 0x9857533f, 3 | BRF_GRA },           // 18

	{ "eb16.22",			0x0100, 0xb833b5ea, 4 | BRF_GRA },           // 19 Layer Priority
};

STD_ROM_PICK(piratpet)
STD_ROM_FN(piratpet)

struct BurnDriver BurnDrvPiratpet = {
	"piratpet", "junglek", NULL, NULL, "1982",
	"Pirate Pete\0", NULL, "Taito America Corporation", "Taito SJ System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_RUNAHEAD_DRAWSYNC | BDF_CLONE | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, piratpetRomInfo, piratpetRomName, NULL, NULL, NULL, NULL, OneButtonInputInfo, PiratpetDIPInfo,
	junglekInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	256, 224, 4, 3
};


// Alpine Ski (set 1)

static struct BurnRomInfo alpineRomDesc[] = {
	{ "rh16.069",			0x1000, 0x6b2a69b7, 1 | BRF_PRG | BRF_ESS }, //  0 Main Z80 Code
	{ "rh17.068",			0x1000, 0xe344b0b7, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rh18.067",			0x1000, 0x753bdd87, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rh19.066",			0x1000, 0x3efb3fcd, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "rh20.065",			0x1000, 0xc2cd4e79, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "rh21.064",			0x1000, 0x74109145, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "rh22.055",			0x1000, 0xefa82a57, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "rh23.054",			0x1000, 0x77c25acf, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "rh13.070",			0x1000, 0xdcad1794, 2 | BRF_PRG | BRF_ESS }, //  8 Sound Z80 Code

	{ "rh24.001",			0x1000, 0x4b1d9455, 3 | BRF_GRA },           //  9 Graphics data
	{ "rh25.002",			0x1000, 0xbf71e278, 3 | BRF_GRA },           // 10
	{ "rh26.003",			0x1000, 0x13da2a9b, 3 | BRF_GRA },           // 11
	{ "rh27.004",			0x1000, 0x425b52b0, 3 | BRF_GRA },           // 12

	{ "eb16.22",			0x0100, 0xb833b5ea, 4 | BRF_GRA },           // 13 Layer Priority
};

STD_ROM_PICK(alpine)
STD_ROM_FN(alpine)

static INT32 alpineInit()
{
	is_alpine = 1;
	return CommonInit(1, 0, 0);
}

struct BurnDriver BurnDrvAlpine = {
	"alpine", NULL, NULL, NULL, "1982",
	"Alpine Ski (set 1)\0", NULL, "Taito Corporation", "Taito SJ System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_RUNAHEAD_DRAWSYNC | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_SPORTSMISC, 0,
	NULL, alpineRomInfo, alpineRomName, NULL, NULL, NULL, NULL, AlpineInputInfo, AlpineDIPInfo,
	alpineInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Alpine Ski (set 2)

static struct BurnRomInfo alpineaRomDesc[] = {
	{ "rh01-1.69",			0x1000, 0x7fbcb635, 1 | BRF_PRG | BRF_ESS }, //  0 Main Z80 Code
	{ "rh02.68",			0x1000, 0xc83f95af, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rh03.67",			0x1000, 0x211102bc, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rh04-1.66",			0x1000, 0x494a91b0, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "rh05.65",			0x1000, 0xd85588be, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "rh06.64",			0x1000, 0x521fddb9, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "rh07.55",			0x1000, 0x51f369a4, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "rh08.54",			0x1000, 0xe0af9cb2, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "rh13.070",			0x1000, 0xdcad1794, 2 | BRF_PRG | BRF_ESS }, //  8 Sound Z80 Code

	{ "rh24.001",			0x1000, 0x4b1d9455, 3 | BRF_GRA },           //  9 Graphics data
	{ "rh25.002",			0x1000, 0xbf71e278, 3 | BRF_GRA },           // 10
	{ "rh26.003",			0x1000, 0x13da2a9b, 3 | BRF_GRA },           // 11
	{ "rh12.4",				0x1000, 0x0ff0d1fe, 3 | BRF_GRA },           // 12

	{ "eb16.22",			0x0100, 0xb833b5ea, 4 | BRF_GRA },           // 13 Layer Priority
};

STD_ROM_PICK(alpinea)
STD_ROM_FN(alpinea)

static INT32 alpineaInit()
{
	is_alpine = 2;
	return CommonInit(1, 0, 0);
}

struct BurnDriver BurnDrvAlpinea = {
	"alpinea", "alpine", NULL, NULL, "1982",
	"Alpine Ski (set 2)\0", NULL, "Taito Corporation", "Taito SJ System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_RUNAHEAD_DRAWSYNC | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_SPORTSMISC, 0,
	NULL, alpineaRomInfo, alpineaRomName, NULL, NULL, NULL, NULL, AlpineInputInfo, AlpineaDIPInfo,
	alpineaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Time Tunnel

static struct BurnRomInfo timetunlRomDesc[] = {
	{ "un01.69",			0x1000, 0x2e56d946, 1 | BRF_PRG | BRF_ESS }, //  0 Main Z80 Code
	{ "un02.68",			0x1000, 0xf611d852, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "un03.67",			0x1000, 0x144b5e7f, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "un04.66",			0x1000, 0xb6767eba, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "un05.65",			0x1000, 0x91e3c558, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "un06.64",			0x1000, 0xaf5a7d2a, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "un07.55",			0x1000, 0x4ee50999, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "un08.54",			0x1000, 0x97259b57, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "un09.53",			0x1000, 0x771d0fb0, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "un10.52",			0x1000, 0x8b6afad2, 1 | BRF_PRG | BRF_ESS }, //  9

	{ "un19.70",			0x1000, 0xdbf726c6, 2 | BRF_PRG | BRF_ESS }, // 10 Sound Z80 Code

	{ "un11.1",				0x1000, 0x3be4fed6, 3 | BRF_GRA },           // 11 Graphics data
	{ "un12.2",				0x1000, 0x2dee1cf3, 3 | BRF_GRA },           // 12
	{ "un13.3",				0x1000, 0x72b491a8, 3 | BRF_GRA },           // 13
	{ "un14.4",				0x1000, 0x5f695369, 3 | BRF_GRA },           // 14
	{ "un15.5",				0x1000, 0x001df94b, 3 | BRF_GRA },           // 15
	{ "un16.6",				0x1000, 0xe33b9019, 3 | BRF_GRA },           // 16
	{ "un17.7",				0x1000, 0xd66025b8, 3 | BRF_GRA },           // 17
	{ "un18.8",				0x1000, 0xe67ff377, 3 | BRF_GRA },           // 18

	{ "eb16.22",			0x0100, 0xb833b5ea, 4 | BRF_GRA },           // 19 Layer Priority
};

STD_ROM_PICK(timetunl)
STD_ROM_FN(timetunl)

static INT32 timetunlInit()
{
	return CommonInit(1, 0, 0);
}

struct BurnDriver BurnDrvTimetunl = {
	"timetunl", NULL, NULL, NULL, "1982",
	"Time Tunnel\0", NULL, "Taito Corporation", "Taito SJ System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_RUNAHEAD_DRAWSYNC | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_MAZE, 0,
	NULL, timetunlRomInfo, timetunlRomName, NULL, NULL, NULL, NULL, TimetunlInputInfo, TimetunlDIPInfo,
	timetunlInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	256, 224, 4, 3
};


// Wild Western (set 1)

static struct BurnRomInfo wwesternRomDesc[] = {
	{ "ww01.bin",			0x1000, 0xbfe10753, 1 | BRF_PRG | BRF_ESS }, //  0 Main Z80 Code
	{ "ww02d.bin",			0x1000, 0x20579e90, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ww03d.bin",			0x1000, 0x0e65be37, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ww04d.bin",			0x1000, 0xb3565a31, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ww05d.bin",			0x1000, 0x089f3d89, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "ww06d.bin",			0x1000, 0xc81c9736, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "ww07.bin",			0x1000, 0x1937cc17, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "ww14.bin",			0x1000, 0x23776870, 2 | BRF_PRG | BRF_ESS }, //  7 Sound Z80 Code

	{ "ww08.bin",			0x1000, 0x041a5a1c, 3 | BRF_GRA },           //  8 Graphics data
	{ "ww09.bin",			0x1000, 0x07982ac5, 3 | BRF_GRA },           //  9
	{ "ww10.bin",			0x1000, 0xf32ae203, 3 | BRF_GRA },           // 10
	{ "ww11.bin",			0x1000, 0x7ff1431f, 3 | BRF_GRA },           // 11
	{ "ww12.bin",			0x1000, 0xbe1b563a, 3 | BRF_GRA },           // 12
	{ "ww13.bin",			0x1000, 0x092cd9e5, 3 | BRF_GRA },           // 13

	{ "ww17",				0x0100, 0x93447d2b, 4 | BRF_GRA },           // 14 Layer Priority
};

STD_ROM_PICK(wwestern)
STD_ROM_FN(wwestern)

static INT32 wwesternInit()
{
	input2_xor = 0x04;
	return CommonInit(0, 0, 0);
}

struct BurnDriver BurnDrvWwestern = {
	"wwestern", NULL, NULL, NULL, "1982",
	"Wild Western (set 1)\0", NULL, "Taito Corporation", "Taito SJ System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_RUNAHEAD_DRAWSYNC | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_SHOOT, 0,
	NULL, wwesternRomInfo, wwesternRomName, NULL, NULL, NULL, NULL, DualStickInputInfo, WwesternDIPInfo,
	wwesternInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Wild Western (set 2)

static struct BurnRomInfo wwestern1RomDesc[] = {
	{ "ww01.bin",			0x1000, 0xbfe10753, 1 | BRF_PRG | BRF_ESS }, //  0 Main Z80 Code
	{ "ww02",				0x1000, 0xf011103a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ww03d.bin",			0x1000, 0x0e65be37, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ww04a",				0x1000, 0x68b31a6e, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ww05",				0x1000, 0x78293f81, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "ww06",				0x1000, 0xd015e435, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "ww07.bin",			0x1000, 0x1937cc17, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "ww14.bin",			0x1000, 0x23776870, 2 | BRF_PRG | BRF_ESS }, //  7 Sound Z80 Code

	{ "ww08.bin",			0x1000, 0x041a5a1c, 3 | BRF_GRA },           //  8 Graphics data
	{ "ww09.bin",			0x1000, 0x07982ac5, 3 | BRF_GRA },           //  9
	{ "ww10.bin",			0x1000, 0xf32ae203, 3 | BRF_GRA },           // 10
	{ "ww11.bin",			0x1000, 0x7ff1431f, 3 | BRF_GRA },           // 11
	{ "ww12.bin",			0x1000, 0xbe1b563a, 3 | BRF_GRA },           // 12
	{ "ww13.bin",			0x1000, 0x092cd9e5, 3 | BRF_GRA },           // 13

	{ "ww17",				0x0100, 0x93447d2b, 4 | BRF_GRA },           // 14 Layer Priority
};

STD_ROM_PICK(wwestern1)
STD_ROM_FN(wwestern1)

struct BurnDriver BurnDrvWwestern1 = {
	"wwestern1", "wwestern", NULL, NULL, "1982",
	"Wild Western (set 2)\0", NULL, "Taito Corporation", "Taito SJ System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_RUNAHEAD_DRAWSYNC | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_SHOOT, 0,
	NULL, wwestern1RomInfo, wwestern1RomName, NULL, NULL, NULL, NULL, DualStickInputInfo, WwesternDIPInfo,
	wwesternInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Front Line (AA1, 4 PCB version)

static struct BurnRomInfo frontlinRomDesc[] = {
	{ "aa1_05.ic1",			0x2000, 0x4b7c0d81, 1 | BRF_PRG | BRF_ESS }, //  0 Main Z80 Code
	{ "aa1_06.ic2",			0x2000, 0xcaacdc02, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "aa1_07.ic3",			0x2000, 0xdf2b2691, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "aa1_08.ic6",			0x2000, 0xf9bc3374, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "aa1_09.ic7",			0x2000, 0xe24d1f05, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "aa1_10.ic8",			0x1000, 0x2704aa4c, 1 | BRF_PRG | BRF_ESS }, //  5
	
	{ "aa1_11.ic70",		0x1000, 0x15f4ed8c, 2 | BRF_PRG | BRF_ESS }, // 11 Sound Z80 Code
	{ "aa1_12.ic71",		0x1000, 0xc3eb38e7, 2 | BRF_PRG | BRF_ESS }, // 12

	{ "aa1_01.ic4",			0x2000, 0x724fd755, 3 | BRF_GRA },           // 13 Graphics data
	{ "aa1_02.ic5",			0x2000, 0xb2d35070, 3 | BRF_GRA },           // 14
	{ "aa1_03.ic9",			0x2000, 0xd0723026, 3 | BRF_GRA },           // 15
	{ "aa1_04.ic10",		0x2000, 0xbdc0a4f0, 3 | BRF_GRA },           // 16
	
	{ "eb16.ic22",			0x0100, 0xb833b5ea, 4 | BRF_GRA },           // 21 Layer Priority

	{ "aa1_13.ic24",		0x0800, 0x7e78bdd3, 5 | BRF_PRG | BRF_ESS }, // 22 M68705 MCU Code
};

STD_ROM_PICK(frontlin)
STD_ROM_FN(frontlin)

static INT32 frontlinInit()
{
	input2_xor = 0x02;
	return CommonInit(0, 0, 0);
}

struct BurnDriver BurnDrvFrontlin = {
	"frontlin", NULL, NULL, NULL, "1982",
	"Front Line (AA1, 4 PCB version)\0", NULL, "Taito Corporation", "Taito SJ System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_RUNAHEAD_DRAWSYNC | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_RUNGUN, 0,
	NULL, frontlinRomInfo, frontlinRomName, NULL, NULL, NULL, NULL, DualStickInputInfo, FrontlinDIPInfo,
	frontlinInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Front Line (FL, 5 PCB version)

static struct BurnRomInfo frontlinaRomDesc[] = {
	{ "fl69.u69",			0x1000, 0x93b64599, 1 | BRF_PRG | BRF_ESS }, //  0 Main Z80 Code
	{ "fl68.u68",			0x1000, 0x82dccdfb, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "fl67.u67",			0x1000, 0x3fa1ba12, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "fl66.u66",			0x1000, 0x4a3db285, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "fl65.u65",			0x1000, 0xda00ec70, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "fl64.u64",			0x1000, 0x9fc90a20, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "fl55.u55",			0x1000, 0x359242c2, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "fl54.u54",			0x1000, 0xd234c60f, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "fl53.u53",			0x1000, 0x67429975, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "fl52.u52",			0x1000, 0xcb223d34, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "aa1_10.ic8",			0x1000, 0x2704aa4c, 1 | BRF_PRG | BRF_ESS }, // 10

	{ "fl70.u70",			0x1000, 0x15f4ed8c, 2 | BRF_PRG | BRF_ESS }, // 11 Sound Z80 Code
	{ "fl71.u71",			0x1000, 0xc3eb38e7, 2 | BRF_PRG | BRF_ESS }, // 12

	{ "fl1.u1",				0x1000, 0xe82c9f46, 3 | BRF_GRA },           // 13 Graphics data
	{ "fl2.u2",				0x1000, 0x123055d3, 3 | BRF_GRA },           // 14
	{ "fl3.u3",				0x1000, 0x7ea46347, 3 | BRF_GRA },           // 15
	{ "fl4.u4",				0x1000, 0x9e2cff10, 3 | BRF_GRA },           // 16
	{ "fl5.u5",				0x1000, 0x630b4be1, 3 | BRF_GRA },           // 17
	{ "fl6.u6",				0x1000, 0x9e092d58, 3 | BRF_GRA },           // 18
	{ "fl7.u7",				0x1000, 0x613682a3, 3 | BRF_GRA },           // 19
	{ "fl8.u8",				0x1000, 0xf73b0d5e, 3 | BRF_GRA },           // 20

	{ "eb16.ic22",			0x0100, 0xb833b5ea, 4 | BRF_GRA },           // 21 Layer Priority

	{ "aa1_13.ic24",		0x0800, 0x7e78bdd3, 5 | BRF_PRG | BRF_ESS }, // 22 M68705 MCU Code
};

STD_ROM_PICK(frontlina)
STD_ROM_FN(frontlina)

struct BurnDriver BurnDrvFrontlina = {
	"frontlina", "frontlin", NULL, NULL, "1982",
	"Front Line (FL, 5 PCB version)\0", NULL, "Taito Corporation", "Taito SJ System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_RUNAHEAD_DRAWSYNC | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_RUNGUN, 0,
	NULL, frontlinaRomInfo, frontlinaRomName, NULL, NULL, NULL, NULL, DualStickInputInfo, FrontlinDIPInfo,
	frontlinInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Elevator Action (BA3, 4 PCB version, 1.1)
// later 4 board set, with rom data on 2764s, split between gfx and cpu data.

static struct BurnRomInfo elevatorRomDesc[] = {
	{ "ba3__01.2764.ic1",		0x2000, 0xda775a24, 1 | BRF_PRG | BRF_ESS }, //  0 Main Z80 Code
	{ "ba3__02.2764.ic2",		0x2000, 0xfbfd8b3a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ba3__03-1.2764.ic3",		0x2000, 0xa2e69833, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ba3__04-1.2764.ic6",		0x2000, 0x2b78c462, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "ba3__09.2732.ic70",		0x1000, 0x6d5f57cb, 2 | BRF_PRG | BRF_ESS }, //  4 Sound Z80 Code
	{ "ba3__10.2732.ic71",		0x1000, 0xf0a769a1, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "ba3__05.2764.ic4",		0x2000, 0x6c4ee58f, 3 | BRF_GRA },           //  6 Graphics data
	{ "ba3__06.2764.ic5",		0x2000, 0x41ab0afc, 3 | BRF_GRA },           //  7
	{ "ba3__07.2764.ic9",		0x2000, 0xefe43731, 3 | BRF_GRA },           //  8
	{ "ba3__08.2764.ic10",		0x2000, 0x3ca20696, 3 | BRF_GRA },           //  9

	{ "eb16.ic22",				0x0100, 0xb833b5ea, 4 | BRF_GRA },           // 10 Layer Priority

	{ "ba3__11.mc68705p3.ic24",	0x0800, 0x9ce75afc, 5 | BRF_PRG | BRF_ESS }, // 11 M68705 MCU Code

	{ "ww15.pal16l8.ic24.jed.bin",	0x0117, 0xc3ec20d6, 0 | BRF_OPT },   // 12 pal
};

STD_ROM_PICK(elevator)
STD_ROM_FN(elevator)

static INT32 elevatorInit()
{
	return CommonInit(0, 0, 0);
}

struct BurnDriver BurnDrvElevator = {
	"elevator", NULL, NULL, NULL, "1983",
	"Elevator Action (BA3, 4 PCB version, 1.1)\0", NULL, "Taito Corporation", "Taito SJ System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_RUNAHEAD_DRAWSYNC | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, elevatorRomInfo, elevatorRomName, NULL, NULL, NULL, NULL, TwoButtonInputInfo, ElevatorDIPInfo,
	elevatorInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	256, 224, 4, 3
};


// Elevator Action (EA, 5 PCB version, 1.1)
// 5 board set, using 2732s on both mainboard and square rom board, and 68705 on daughterboard at bottom of stack, upside down

static struct BurnRomInfo elevatoraRomDesc[] = {
	{ "ea_12.2732.ic69",	   0x1000, 0x24e277ef, 1 | BRF_PRG | BRF_ESS }, //  0 Main Z80 Code
	{ "ea_13.2732.ic68",	   0x1000, 0x13702e39, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ea_14.2732.ic67",	   0x1000, 0x46f52646, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ea_15.2732.ic66",	   0x1000, 0xe22fe57e, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ea_16.2732.ic65",	   0x1000, 0xc10691d7, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "ea_17.2732.ic64",	   0x1000, 0x8913b293, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "ea_18.2732.ic55",	   0x1000, 0x1cabda08, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "ea_19.2732.ic54",	   0x1000, 0xf4647b4f, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "ea_9.2732.ic70",		   0x1000, 0x6d5f57cb, 2 | BRF_PRG | BRF_ESS }, //  8 Sound Z80 Code
	{ "ea_10.2732.ic71",	   0x1000, 0xf0a769a1, 2 | BRF_PRG | BRF_ESS }, //  9

	{ "ea_20.2732.ic1",		   0x1000, 0xbbbb3fba, 3 | BRF_GRA },           // 10 Graphics data
	{ "ea_21.2732.ic2",		   0x1000, 0x639cc2fd, 3 | BRF_GRA },           // 11
	{ "ea_22.2732.ic3",		   0x1000, 0x61317eea, 3 | BRF_GRA },           // 12
	{ "ea_23.2732.ic4",		   0x1000, 0x55446482, 3 | BRF_GRA },           // 13
	{ "ea_24.2732.ic5",		   0x1000, 0x77895c0f, 3 | BRF_GRA },           // 14
	{ "ea_25.2732.ic6",		   0x1000, 0x9a1b6901, 3 | BRF_GRA },           // 15
	{ "ea_26.2732.ic7",		   0x1000, 0x839112ec, 3 | BRF_GRA },           // 16
	{ "ea_27.2732.ic8",		   0x1000, 0xdb7ff692, 3 | BRF_GRA },           // 17

	{ "eb16.ic22",			   0x0100, 0xb833b5ea, 4 | BRF_GRA },           // 18 Layer Priority

	{ "ba3__11.mc68705p3.ic4", 0x0800, 0x9ce75afc, 5 | BRF_PRG | BRF_ESS }, // 19 M68705 MCU Code

	{ "ww15.pal16l8.ic24.jed.bin",	0x0117, 0xc3ec20d6, 0 | BRF_OPT },   // 20 pal
};

STD_ROM_PICK(elevatora)
STD_ROM_FN(elevatora)

struct BurnDriver BurnDrvElevatora = {
	"elevatora", "elevator", NULL, NULL, "1983",
	"Elevator Action (EA, 5 PCB version, 1.1)\0", NULL, "Taito Corporation", "Taito SJ System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_RUNAHEAD_DRAWSYNC | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, elevatoraRomInfo, elevatoraRomName, NULL, NULL, NULL, NULL, TwoButtonInputInfo, ElevatorDIPInfo,
	elevatorInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	256, 224, 4, 3
};


// Elevator Action (bootleg)

static struct BurnRomInfo elevatorbRomDesc[] = {
	{ "eabl_12.2732.ic69",	0x1000, 0x66baa214, 1 | BRF_PRG | BRF_ESS }, //  0 Main Z80 Code
	{ "ea_13.2732.ic68",	0x1000, 0x13702e39, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ea_14.2732.ic67",	0x1000, 0x46f52646, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "eabl_15.2732.ic66",	0x1000, 0xb88f3383, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ea_16.2732.ic65",	0x1000, 0xc10691d7, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "ea_17.2732.ic64",	0x1000, 0x8913b293, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "eabl_18.2732.ic55",	0x1000, 0xd546923e, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "eabl_19.2732.ic54",	0x1000, 0x963ec5a5, 9 | BRF_PRG | BRF_ESS }, //  7
	{ "eabl.2732.ic52",		0x1000, 0x44b1314a, 1 | BRF_PRG | BRF_ESS }, //  8

	{ "ea_9.2732.ic70",		0x1000, 0x6d5f57cb, 2 | BRF_PRG | BRF_ESS }, //  9 Sound Z80 Code
	{ "ea_10.2732.ic71",	0x1000, 0xf0a769a1, 2 | BRF_PRG | BRF_ESS }, // 10

	{ "ea_20.2732.ic1",		0x1000, 0xbbbb3fba, 3 | BRF_GRA },           // 11 Graphics data
	{ "ea_21.2732.ic2",		0x1000, 0x639cc2fd, 3 | BRF_GRA },           // 12
	{ "ea_22.2732.ic3",		0x1000, 0x61317eea, 3 | BRF_GRA },           // 13
	{ "ea_23.2732.ic4",		0x1000, 0x55446482, 3 | BRF_GRA },           // 14
	{ "ea_24.2732.ic5",		0x1000, 0x77895c0f, 3 | BRF_GRA },           // 15
	{ "ea_25.2732.ic6",		0x1000, 0x9a1b6901, 3 | BRF_GRA },           // 16
	{ "ea_26.2732.ic7",		0x1000, 0x839112ec, 3 | BRF_GRA },           // 17
	{ "eabl_27.2732.ic8",	0x1000, 0x67ebf7c1, 3 | BRF_GRA },           // 18

	{ "eb16.ic22",			0x0100, 0xb833b5ea, 4 | BRF_GRA },           // 19 Layer Priority

	{ "ww15.pal16l8.ic24.jed.bin",	0x0117, 0xc3ec20d6, 0 | BRF_OPT },   // 20 pal
};

STD_ROM_PICK(elevatorb)
STD_ROM_FN(elevatorb)

static INT32 elevatorbInit()
{
	return CommonInit(0, 0, 0);
}

struct BurnDriver BurnDrvElevatorb = {
	"elevatorb", "elevator", NULL, NULL, "1983",
	"Elevator Action (bootleg)\0", NULL, "bootleg", "Taito SJ System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_RUNAHEAD_DRAWSYNC | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, elevatorbRomInfo, elevatorbRomName, NULL, NULL, NULL, NULL, TwoButtonInputInfo, ElevatorDIPInfo,
	elevatorbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	256, 224, 4, 3
};


// The Tin Star (A10, 4 PCB version)
// later 4 PCB stack

static struct BurnRomInfo tinstarRomDesc[] = {
	{ "a10-01.bin",			0x2000, 0x19faf0b3, 1 | BRF_PRG | BRF_ESS }, //  0 Main Z80 Code
	{ "a10-02.bin",			0x2000, 0x99bb26ff, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a10-03.bin",			0x2000, 0x3169e175, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "a10-04.bin",			0x2000, 0x6641233c, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "a10-29.bin",			0x2000, 0x771f1a6a, 2 | BRF_PRG | BRF_ESS }, //  4 Sound Z80 Code
	{ "a10-10.bin",			0x1000, 0xbeeed8f3, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "a10-05.bin",			0x2000, 0x6bb1bba9, 3 | BRF_GRA },           //  6 Graphics data
	{ "a10-06.bin",			0x2000, 0x0abff1a1, 3 | BRF_GRA },           //  7
	{ "a10-07.bin",			0x2000, 0xd1bec7a8, 3 | BRF_GRA },           //  8
	{ "a10-08.bin",			0x2000, 0x15c6eb41, 3 | BRF_GRA },           //  9

	{ "eb16.22",			0x0100, 0xb833b5ea, 4 | BRF_GRA },           // 10 Layer Priority

	{ "a10-12",				0x0800, 0x889eefc9, 5 | BRF_PRG | BRF_ESS }, // 11 M68705 MCU Code
};

STD_ROM_PICK(tinstar)
STD_ROM_FN(tinstar)

static INT32 TinstarInit()
{
	input2_xor = 0x02;
	return CommonInit(1, 0, 0);
}

struct BurnDriver BurnDrvTinstar = {
	"tinstar", NULL, NULL, NULL, "1983",
	"The Tin Star (A10, 4 PCB version)\0", NULL, "Taito Corporation", "Taito SJ System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_RUNAHEAD_DRAWSYNC | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_SHOOT, 0,
	NULL, tinstarRomInfo, tinstarRomName, NULL, NULL, NULL, NULL, DualStickInputInfo, TinstarDIPInfo,
	TinstarInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	256, 224, 4, 3
};


// The Tin Star (TS, 5 PCB version)
// 5 PCB stack - same data as above in 2732 EPROM format

static struct BurnRomInfo tinstaraRomDesc[] = {
	{ "ts.69",				0x1000, 0xa930af60, 1 | BRF_PRG | BRF_ESS }, //  0 Main Z80 Code
	{ "ts.68",				0x1000, 0x7f2714ca, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ts.67",				0x1000, 0x49170786, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ts.66",				0x1000, 0x3766f130, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ts.65",				0x1000, 0x41251246, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "ts.64",				0x1000, 0x812285d5, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "ts.55",				0x1000, 0x6b80ac51, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "ts.54",				0x1000, 0xb352360f, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "ts.70",				0x1000, 0x4771838d, 2 | BRF_PRG | BRF_ESS }, //  8 Sound Z80 Code
	{ "ts.71",				0x1000, 0x03c91332, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "ts.72",				0x1000, 0xbeeed8f3, 2 | BRF_PRG | BRF_ESS }, // 10

	{ "ts.1",				0x1000, 0xf1160718, 3 | BRF_GRA },           // 11 Graphics data
	{ "ts.2",				0x1000, 0x39dc6dbb, 3 | BRF_GRA },           // 12
	{ "ts.3",				0x1000, 0x079df429, 3 | BRF_GRA },           // 13
	{ "ts.4",				0x1000, 0xe61105d4, 3 | BRF_GRA },           // 14
	{ "ts.5",				0x1000, 0xffab5d15, 3 | BRF_GRA },           // 15
	{ "ts.6",				0x1000, 0xf1d8ca36, 3 | BRF_GRA },           // 16
	{ "ts.7",				0x1000, 0x894f6332, 3 | BRF_GRA },           // 17
	{ "ts.8",				0x1000, 0x519aed19, 3 | BRF_GRA },           // 18

	{ "eb16.22",			0x0100, 0xb833b5ea, 4 | BRF_GRA },           // 19 Layer Priority

	{ "a10-12",				0x0800, 0x889eefc9, 5 | BRF_PRG | BRF_ESS }, // 20 M68705 MCU Code
};

STD_ROM_PICK(tinstara)
STD_ROM_FN(tinstara)

struct BurnDriver BurnDrvTinstara = {
	"tinstara", "tinstar", NULL, NULL, "1983",
	"The Tin Star (TS, 5 PCB version)\0", NULL, "Taito Corporation", "Taito SJ System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_RUNAHEAD_DRAWSYNC | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_SHOOT, 0,
	NULL, tinstaraRomInfo, tinstaraRomName, NULL, NULL, NULL, NULL, DualStickInputInfo, TinstarDIPInfo,
	TinstarInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	256, 224, 4, 3
};


// Water Ski

static struct BurnRomInfo waterskiRomDesc[] = {
	{ "a03-01",				0x1000, 0x322c4c2c, 1 | BRF_PRG | BRF_ESS }, //  0 Main Z80 Code
	{ "a03-02",				0x1000, 0x8df176d1, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a03-03",				0x1000, 0x420bd04f, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "a03-04",				0x1000, 0x5c081a94, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "a03-05",				0x1000, 0x1fae90d2, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "a03-06",				0x1000, 0x55b7c151, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "a03-07",				0x1000, 0x8abc7522, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "a03-13",				0x1000, 0x78c7d37f, 2 | BRF_PRG | BRF_ESS }, //  7 Sound Z80 Code
	{ "a03-14",				0x1000, 0x31f991ca, 2 | BRF_PRG | BRF_ESS }, //  8

	{ "a03-08",				0x1000, 0xc206d870, 3 | BRF_GRA },           //  9 Graphics
	{ "a03-09",				0x1000, 0x48ac912a, 3 | BRF_GRA },           // 10
	{ "a03-10",				0x1000, 0xa056defb, 3 | BRF_GRA },           // 11
	{ "a03-11",				0x1000, 0xf06cddd6, 3 | BRF_GRA },           // 12
	{ "a03-12",				0x1000, 0x27dfd8c2, 3 | BRF_GRA },           // 13

	{ "eb16.22",			0x0100, 0xb833b5ea, 4 | BRF_GRA },           // 14 Layer Priority
};

STD_ROM_PICK(waterski)
STD_ROM_FN(waterski)

static INT32 waterskiInit()
{
	return CommonInit(0, 0, 0);
}

struct BurnDriver BurnDrvWaterski = {
	"waterski", NULL, NULL, NULL, "1983",
	"Water Ski\0", NULL, "Taito Corporation", "Taito SJ System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_RUNAHEAD_DRAWSYNC | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_RACING, 0,
	NULL, waterskiRomInfo, waterskiRomName, NULL, NULL, NULL, NULL, TwoButtonLRInputInfo, WaterskiDIPInfo,
	waterskiInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Bio Attack

static struct BurnRomInfo bioatackRomDesc[] = {
	{ "aa8-01.69",			0x1000, 0xe5abc211, 1 | BRF_PRG | BRF_ESS }, //  0 Main Z80 Code
	{ "aa8-02.68",			0x1000, 0xb5bfde00, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "aa8-03.67",			0x1000, 0xe4e46e69, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "aa8-04.66",			0x1000, 0x86e0af8c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "aa8-05.65",			0x1000, 0xc6248608, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "aa8-06.64",			0x1000, 0x685a0383, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "aa8-07.55",			0x1000, 0x9d58e2b7, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "aa8-08.54",			0x1000, 0xdec5271f, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "aa8-17.70",			0x1000, 0x36eb95b5, 2 | BRF_PRG | BRF_ESS }, //  8 Sound Z80 Code

	{ "aa8-09.1",			0x1000, 0x1fee5fd6, 3 | BRF_GRA },           //  9 Graphics
	{ "aa8-10.2",			0x1000, 0xe0133423, 3 | BRF_GRA },           // 10
	{ "aa8-11.3",			0x1000, 0x0f5715c6, 3 | BRF_GRA },           // 11
	{ "aa8-12.4",			0x1000, 0x71126dd0, 3 | BRF_GRA },           // 12
	{ "aa8-13.5",			0x1000, 0xadcdd2f0, 3 | BRF_GRA },           // 13
	{ "aa8-14.6",			0x1000, 0x2fe18680, 3 | BRF_GRA },           // 14
	{ "aa8-15.7",			0x1000, 0xff5aad4b, 3 | BRF_GRA },           // 15
	{ "aa8-16.8",			0x1000, 0xceba4036, 3 | BRF_GRA },           // 16

	{ "eb16.22",			0x0100, 0xb833b5ea, 4 | BRF_GRA },           // 17 Layer Priority
};

STD_ROM_PICK(bioatack)
STD_ROM_FN(bioatack)

static INT32 bioatackInit()
{
	input2_xor = 0x30; // coins active high
	return CommonInit(1, 0, 0);
}

struct BurnDriver BurnDrvBioatack = {
	"bioatack", NULL, NULL, NULL, "1983",
	"Bio Attack\0", NULL, "Taito Corporation", "Taito SJ System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_RUNAHEAD_DRAWSYNC | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, bioatackRomInfo, bioatackRomName, NULL, NULL, NULL, NULL, OneButtonInputInfo, BioatackDIPInfo,
	bioatackInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Sea Fighter Poseidon

static struct BurnRomInfo sfposeidRomDesc[] = {
	{ "a14-01.1",			0x2000, 0xaa779fbb, 1 | BRF_PRG | BRF_ESS }, //  0 Main Z80 Code
	{ "a14-02.2",			0x2000, 0xecec9dc3, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a14-03.3",			0x2000, 0x469498c1, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "a14-04.6",			0x2000, 0x1db4bc02, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "a14-05.7",			0x2000, 0x95e2f903, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "a14-10.70",			0x1000, 0xf1365f35, 2 | BRF_PRG | BRF_ESS }, //  5 Sound Z80 Code
	{ "a14-11.71",			0x1000, 0x74a12fe2, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "a14-06.4",			0x2000, 0x9740493b, 3 | BRF_GRA },           //  7 Graphics
	{ "a14-07.5",			0x2000, 0x1c93de97, 3 | BRF_GRA },           //  8
	{ "a14-08.9",			0x2000, 0x4367e65a, 3 | BRF_GRA },           //  9
	{ "a14-09.10",			0x2000, 0x677cffd5, 3 | BRF_GRA },           // 10

	{ "eb16.22",			0x0100, 0xb833b5ea, 4 | BRF_GRA },           // 11 Layer Priority

	{ "a14-12",				0x0800, 0x091beed8, 5 | BRF_PRG | BRF_ESS }, // 12 M68705 MCU Code
};

STD_ROM_PICK(sfposeid)
STD_ROM_FN(sfposeid)

static INT32 sfposeidInit()
{
	return CommonInit(0, 0, 0);
}

struct BurnDriver BurnDrvSfposeid = {
	"sfposeid", NULL, NULL, NULL, "1984",
	"Sea Fighter Poseidon\0", NULL, "Taito Corporation", "Taito SJ System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_RUNAHEAD_DRAWSYNC | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_HORSHOOT, 0,
	NULL, sfposeidRomInfo, sfposeidRomName, NULL, NULL, NULL, NULL, TwoButtonInputInfo, SfposeidDIPInfo,
	sfposeidInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	256, 224, 4, 3
};


// High Way Race

static struct BurnRomInfo hwraceRomDesc[] = {
	{ "hw_race.01",			0x1000, 0x8beec11f, 1 | BRF_PRG | BRF_ESS }, //  0 Main Z80 Code
	{ "hw_race.02",			0x1000, 0x72ad099d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "hw_race.03",			0x1000, 0xd0c221d7, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "hw_race.04",			0x1000, 0xeb97015b, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "hw_race.05",			0x1000, 0x777c8007, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "hw_race.06",			0x1000, 0x165f46a3, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "hw_race.07",			0x1000, 0x53d7e323, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "hw_race.08",			0x1000, 0xbdbc1208, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "hw_race.17",			0x1000, 0xafe24f3e, 2 | BRF_PRG | BRF_ESS }, //  8 Sound Z80 Code
	{ "hw_race.18",			0x1000, 0xdbec897d, 2 | BRF_PRG | BRF_ESS }, //  9

	{ "hw_race.09",			0x1000, 0x345b9b88, 3 | BRF_GRA },           // 10 Graphics
	{ "hw_race.10",			0x1000, 0x598a3c3e, 3 | BRF_GRA },           // 11
	{ "hw_race.11",			0x1000, 0x3f436a7d, 3 | BRF_GRA },           // 12
	{ "hw_race.12",			0x1000, 0x8694b2c6, 3 | BRF_GRA },           // 13
	{ "hw_race.13",			0x1000, 0xa0af7711, 3 | BRF_GRA },           // 14
	{ "hw_race.14",			0x1000, 0x9be0f556, 3 | BRF_GRA },           // 15
	{ "hw_race.15",			0x1000, 0xe1057eb7, 3 | BRF_GRA },           // 16
	{ "hw_race.16",			0x1000, 0xf7104668, 3 | BRF_GRA },           // 17

	{ "eb16.22",			0x0100, 0xb833b5ea, 4 | BRF_GRA },           // 18 Layer Priority
};

STD_ROM_PICK(hwrace)
STD_ROM_FN(hwrace)

static INT32 hwraceInit()
{
	return CommonInit(0, 0, 0);
}

struct BurnDriver BurnDrvHwrace = {
	"hwrace", NULL, NULL, NULL, "1983",
	"High Way Race\0", NULL, "Taito Corporation", "Taito SJ System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_RUNAHEAD_DRAWSYNC | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_RACING, 0,
	NULL, hwraceRomInfo, hwraceRomName, NULL, NULL, NULL, NULL, TwoButtonInputInfo, HwraceDIPInfo,
	hwraceInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Kick Start: Wheelie King

static struct BurnRomInfo kikstartRomDesc[] = {
	{ "a20-01",				0x2000, 0x5810be97, 1 | BRF_PRG | BRF_ESS }, //  0 Main Z80 Code
	{ "a20-02",				0x2000, 0x13e9565d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a20-03",				0x2000, 0x93d7a9e1, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "a20-04",				0x2000, 0x1f23c5d6, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "a20-05",				0x2000, 0x66e100aa, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "a20-10",				0x1000, 0xde4352a4, 2 | BRF_PRG | BRF_ESS }, //  5 Sound Z80 Code
	{ "a20-11",				0x1000, 0x8db12dd9, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "a20-12",				0x1000, 0xe7eeb933, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "a20-06",				0x2000, 0x6582fc89, 3 | BRF_GRA },           //  8 Graphics
	{ "a20-07",				0x2000, 0x8c0b76d2, 3 | BRF_GRA },           //  9
	{ "a20-08",				0x2000, 0x0cca7a9d, 3 | BRF_GRA },           // 10
	{ "a20-09",				0x2000, 0xda625ccf, 3 | BRF_GRA },           // 11

	{ "eb16.22",			0x0100, 0xb833b5ea, 4 | BRF_GRA },           // 12 Layer Priority

	{ "a20-13.ic91",		0x0800, 0x3fb6c4fb, 5 | BRF_PRG | BRF_ESS }, // 13 M68705 MCU Code

	{ "pal16l8.28",			0x0104, 0       ,0 | BRF_NODUMP | BRF_OPT }, // 14 PLDs
};

STD_ROM_PICK(kikstart)
STD_ROM_FN(kikstart)

static INT32 kikstartInit()
{
	input2_xor = 0x30;
	is_kikstart = 1;

	return CommonInit(0, 0, 1);
}

struct BurnDriver BurnDrvKikstart = {
	"kikstart", NULL, NULL, NULL, "1984",
	"Kick Start: Wheelie King\0", NULL, "Taito Corporation", "Taito SJ System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_RUNAHEAD_DRAWSYNC | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_RACING, 0,
	NULL, kikstartRomInfo, kikstartRomName, NULL, NULL, NULL, NULL, KikstartInputInfo, KikstartDIPInfo,
	kikstartInit, DrvExit, DrvFrame, KikstartDraw, DrvScan, &DrvRecalc, 0x40,
	256, 224, 4, 3
};
