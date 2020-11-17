// FB Alpha Legionnaire driver module
// Based on MAME driver by David Graves, Angelo Salese, David Haywood, Tomasz Slanina

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "seibucop.h"
#include "seibusnd.h"

static UINT8 *AllMem;
static UINT8 *Drv68KROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvGfxROM4;
static UINT8 *DrvSndROM;
static UINT8 *AllRam;
static UINT8 *DrvZ80RAM;
static UINT8 *Drv1KRAM;
static UINT8 *DrvAllRAM;
static UINT8 *DrvBgBuf;
static UINT8 *DrvFgBuf;
static UINT8 *DrvMgBuf;
static UINT8 *DrvTxBuf;
static UINT16 *DrvPalBuf16;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvTransTable[5];

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 background_bank;
static INT32 foreground_bank;
static INT32 midground_bank;
static UINT16 layer_disable;
static INT32 flipscreen;
static INT32 scroll[7];
static INT32 sample_bank;
static INT32 coin_inserted_counter[2];

static INT32 coin_hold_length = 4;
static UINT32 sprite_size;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvJoy4[16];
static UINT16 DrvInputs[4];
static UINT8 DrvDips[2];
static UINT8 DrvReset;

static INT32 denjinmk_hack = 0;

static struct BurnInputInfo LegionnaInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Legionna)

static struct BurnInputInfo HeatbrlInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},

	{"P3 Coin",			BIT_DIGITAL,	DrvJoy4 + 3,	"p3 coin"	},
	{"P3 Start",		BIT_DIGITAL,	DrvJoy4 + 4,	"p3 start"	},
	{"P3 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p3 fire 2"	},

	{"P4 Coin",			BIT_DIGITAL,	DrvJoy4 + 7,	"p4 coin"	},
	{"P4 Start",		BIT_DIGITAL,	DrvJoy4 + 5,	"p4 start"	},
	{"P4 Up",			BIT_DIGITAL,	DrvJoy2 + 8,	"p4 up"		},
	{"P4 Down",			BIT_DIGITAL,	DrvJoy2 + 9,	"p4 down"	},
	{"P4 Left",			BIT_DIGITAL,	DrvJoy2 + 10,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy2 + 11,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy2 + 12,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy2 + 13,	"p4 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Heatbrl)

static struct BurnInputInfo GodzillaInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 3,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Godzilla)

static struct BurnInputInfo DenjinmkInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 3"	},

	{"P3 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p3 fire 2"	},

	{"P4 Up",			BIT_DIGITAL,	DrvJoy2 + 8,	"p4 up"		},
	{"P4 Down",			BIT_DIGITAL,	DrvJoy2 + 9,	"p4 down"	},
	{"P4 Left",			BIT_DIGITAL,	DrvJoy2 + 10,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy2 + 11,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy2 + 12,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy2 + 13,	"p4 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Denjinmk)

static struct BurnInputInfo GrainbowInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 3"	},

	{"P3 Start",		BIT_DIGITAL,	DrvJoy1 + 8,	"p3 start"	},
	{"P3 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p3 fire 2"	},
	{"P3 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p3 fire 3"	},

	{"P4 Start",		BIT_DIGITAL,	DrvJoy1 + 9,	"p4 start"	},
	{"P4 Up",			BIT_DIGITAL,	DrvJoy2 + 8,	"p4 up"		},
	{"P4 Down",			BIT_DIGITAL,	DrvJoy2 + 9,	"p4 down"	},
	{"P4 Left",			BIT_DIGITAL,	DrvJoy2 + 10,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy2 + 11,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy2 + 12,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy2 + 13,	"p4 fire 2"	},
	{"P4 Button 3",		BIT_DIGITAL,	DrvJoy2 + 14,	"p4 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 3,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Grainbow)

static struct BurnDIPInfo LegionnaDIPList[]=
{
	{0x11, 0xff, 0xff, 0xff, NULL						},
	{0x12, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    20, "Coinage"					},
	{0x11, 0x01, 0x1f, 0x15, "6 Coins 1 Credits"		},
	{0x11, 0x01, 0x1f, 0x17, "5 Coins 1 Credits"		},
	{0x11, 0x01, 0x1f, 0x19, "4 Coins 1 Credits"		},
	{0x11, 0x01, 0x1f, 0x1b, "3 Coins 1 Credits"		},
	{0x11, 0x01, 0x1f, 0x03, "8 Coins 3 Credits"		},
	{0x11, 0x01, 0x1f, 0x1d, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x1f, 0x05, "5 Coins 3 Credits"		},
	{0x11, 0x01, 0x1f, 0x07, "3 Coins 2 Credits"		},
	{0x11, 0x01, 0x1f, 0x1f, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x1f, 0x09, "2 Coins 3 Credits"		},
	{0x11, 0x01, 0x1f, 0x13, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0x1f, 0x11, "1 Coin  3 Credits"		},
	{0x11, 0x01, 0x1f, 0x0f, "1 Coin  4 Credits"		},
	{0x11, 0x01, 0x1f, 0x0d, "1 Coin  5 Credits"		},
	{0x11, 0x01, 0x1f, 0x0b, "1 Coin  6 Credits"		},
	{0x11, 0x01, 0x1f, 0x1e, "A 1/1 B 1/2"				},
	{0x11, 0x01, 0x1f, 0x14, "A 2/1 B 1/3"				},
	{0x11, 0x01, 0x1f, 0x0a, "A 3/1 B 1/5"				},
	{0x11, 0x01, 0x1f, 0x00, "A 5/1 B 1/6"				},
	{0x11, 0x01, 0x1f, 0x01, "Free Play"				},

	{0   , 0xfe, 0   ,    2, "Freeze"					},
	{0x11, 0x01, 0x40, 0x40, "Off"						},
	{0x11, 0x01, 0x40, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Flip Screen"				},
	{0x11, 0x01, 0x80, 0x80, "Off"						},
	{0x11, 0x01, 0x80, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x12, 0x01, 0x03, 0x02, "1"						},
	{0x12, 0x01, 0x03, 0x03, "2"						},
	{0x12, 0x01, 0x03, 0x01, "3"						},
	{0x12, 0x01, 0x03, 0x00, "4"						},

	{0   , 0xfe, 0   ,    2, "Extend"					},
	{0x12, 0x01, 0x04, 0x00, "Off"						},
	{0x12, 0x01, 0x04, 0x04, "On"						},

	{0   , 0xfe, 0   ,    2, "Unknown"					},
	{0x12, 0x01, 0x08, 0x08, "Off"						},
	{0x12, 0x01, 0x08, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x12, 0x01, 0x30, 0x20, "Easy"						},
	{0x12, 0x01, 0x30, 0x30, "Medium"					},
	{0x12, 0x01, 0x30, 0x10, "Hard"						},
	{0x12, 0x01, 0x30, 0x00, "Hardest"					},

	{0   , 0xfe, 0   ,    2, "Allow Continue"			},
	{0x12, 0x01, 0x40, 0x00, "No"						},
	{0x12, 0x01, 0x40, 0x40, "Yes"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x12, 0x01, 0x80, 0x00, "Off"						},
	{0x12, 0x01, 0x80, 0x80, "On"						},
};

STDDIPINFO(Legionna)


static struct BurnDIPInfo HeatbrlDIPList[]=
{
	{0x21, 0xff, 0xff, 0xff, NULL						},
	{0x22, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    20, "Coinage"					},
	{0x21, 0x01, 0x1f, 0x15, "6 Coins 1 Credits"		},
	{0x21, 0x01, 0x1f, 0x17, "5 Coins 1 Credits"		},
	{0x21, 0x01, 0x1f, 0x19, "4 Coins 1 Credits"		},
	{0x21, 0x01, 0x1f, 0x1b, "3 Coins 1 Credits"		},
	{0x21, 0x01, 0x1f, 0x03, "8 Coins 3 Credits"		},
	{0x21, 0x01, 0x1f, 0x1d, "2 Coins 1 Credits"		},
	{0x21, 0x01, 0x1f, 0x05, "5 Coins 3 Credits"		},
	{0x21, 0x01, 0x1f, 0x07, "3 Coins 2 Credits"		},
	{0x21, 0x01, 0x1f, 0x1f, "1 Coin  1 Credits"		},
	{0x21, 0x01, 0x1f, 0x09, "2 Coins 3 Credits"		},
	{0x21, 0x01, 0x1f, 0x13, "1 Coin  2 Credits"		},
	{0x21, 0x01, 0x1f, 0x11, "1 Coin  3 Credits"		},
	{0x21, 0x01, 0x1f, 0x0f, "1 Coin  4 Credits"		},
	{0x21, 0x01, 0x1f, 0x0d, "1 Coin  5 Credits"		},
	{0x21, 0x01, 0x1f, 0x0b, "1 Coin  6 Credits"		},
	{0x21, 0x01, 0x1f, 0x1e, "A 1/1 B 1/2"				},
	{0x21, 0x01, 0x1f, 0x14, "A 2/1 B 1/3"				},
	{0x21, 0x01, 0x1f, 0x0a, "A 3/1 B 1/5"				},
	{0x21, 0x01, 0x1f, 0x00, "A 5/1 B 1/6"				},
	{0x21, 0x01, 0x1f, 0x01, "Free Play"				},

	{0   , 0xfe, 0   ,    2, "Players"					},
	{0x21, 0x01, 0x20, 0x20, "2"						},
	{0x21, 0x01, 0x20, 0x00, "4"						},

	{0   , 0xfe, 0   ,    2, "Freeze"					},
	{0x21, 0x01, 0x40, 0x40, "Off"						},
	{0x21, 0x01, 0x40, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Flip Screen"				},
	{0x21, 0x01, 0x80, 0x80, "Off"						},
	{0x21, 0x01, 0x80, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x22, 0x01, 0x03, 0x02, "1"						},
	{0x22, 0x01, 0x03, 0x01, "2"						},
	{0x22, 0x01, 0x03, 0x03, "3"						},
	{0x22, 0x01, 0x03, 0x00, "5"						},

	{0   , 0xfe, 0   ,    2, "Extend"					},
	{0x22, 0x01, 0x04, 0x00, "Off"						},
	{0x22, 0x01, 0x04, 0x04, "On"						},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x22, 0x01, 0x30, 0x20, "Easy"						},
	{0x22, 0x01, 0x30, 0x30, "Normal"					},
	{0x22, 0x01, 0x30, 0x10, "Hard"						},
	{0x22, 0x01, 0x30, 0x00, "Very Hard"				},

	{0   , 0xfe, 0   ,    2, "Allow Continue"			},
	{0x22, 0x01, 0x40, 0x00, "No"						},
	{0x22, 0x01, 0x40, 0x40, "Yes"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x22, 0x01, 0x80, 0x00, "Off"						},
	{0x22, 0x01, 0x80, 0x80, "On"						},
};

STDDIPINFO(Heatbrl)

static struct BurnDIPInfo GodzillaDIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL						},
	{0x15, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x14, 0x01, 0x01, 0x00, "Off"						},
	{0x14, 0x01, 0x01, 0x01, "On"						},

	{0   , 0xfe, 0   ,    2, "Flip Screen"				},
	{0x14, 0x01, 0x10, 0x10, "Off"						},
	{0x14, 0x01, 0x10, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x14, 0x01, 0x80, 0x80, "Off"						},
	{0x14, 0x01, 0x80, 0x00, "On"						},

	{0   , 0xfe, 0   ,    8, "Coin A"					},
	{0x15, 0x01, 0x07, 0x00, "4 Coins 1 Credits"		},
	{0x15, 0x01, 0x07, 0x01, "3 Coins 1 Credits"		},
	{0x15, 0x01, 0x07, 0x02, "2 Coins 1 Credits"		},
	{0x15, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x15, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x15, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x15, 0x01, 0x07, 0x03, "1 Coin  4 Credits"		},
	{0x15, 0x01, 0x07, 0x04, "1 Coin  5 Credits"		},

	{0   , 0xfe, 0   ,    8, "Coin B"					},
	{0x15, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x15, 0x01, 0x38, 0x00, "3 Coins/5 Credits"		},
	{0x15, 0x01, 0x38, 0x10, "2 Coins 3 Credits"		},
	{0x15, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x15, 0x01, 0x38, 0x08, "2 Coins 5 Credits"		},
	{0x15, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x15, 0x01, 0x38, 0x18, "1 Coin  4 Credits"		},
	{0x15, 0x01, 0x38, 0x20, "1 Coin  5 Credits"		},

	{0   , 0xfe, 0   ,    2, "Free Play"				},
	{0x15, 0x01, 0x80, 0x80, "Off"						},
	{0x15, 0x01, 0x80, 0x00, "On"						},
};

STDDIPINFO(Godzilla)

static struct BurnDIPInfo DenjinmkDIPList[]=
{
	{0x1f, 0xff, 0xff, 0xff, NULL						},
	{0x20, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    2, "Flip Screen"				},
	{0x1f, 0x01, 0x10, 0x10, "Off"						},
	{0x1f, 0x01, 0x10, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x1f, 0x01, 0x20, 0x00, "Off"						},
	{0x1f, 0x01, 0x20, 0x20, "On"						},

	{0,    0xfe, 0,       2, "Service Mode"				},
	{0x1f, 0x01, 0x40, 0x40, "Off"                      },
	{0x1f, 0x01, 0x40, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Language"					},
	{0x1f, 0x01, 0x80, 0x80, "Japanese"					},
	{0x1f, 0x01, 0x80, 0x00, "English"					},

	{0   , 0xfe, 0   ,    16, "Coin A"					},
	{0x20, 0x01, 0x0f, 0x02, "4 Coins 1 Credits"		},
	{0x20, 0x01, 0x0f, 0x05, "3 Coins 1 Credits"		},
	{0x20, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"		},
	{0x20, 0x01, 0x0f, 0x06, "3 Coins / 5 Credits"		},
	{0x20, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"		},
	{0x20, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"		},
	{0x20, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"		},
	{0x20, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"		},
	{0x20, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"		},
	{0x20, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"		},
	{0x20, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"		},
	{0x20, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"		},
	{0x20, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"		},
	{0x20, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"		},
	{0x20, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"		},
	{0x20, 0x01, 0x0f, 0x00, "Free Play"				},

	{0   , 0xfe, 0   ,    16, "Coin B"					},
	{0x20, 0x01, 0xf0, 0x20, "4 Coins 1 Credits"		},
	{0x20, 0x01, 0xf0, 0x50, "3 Coins 1 Credits"		},
	{0x20, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"		},
	{0x20, 0x01, 0xf0, 0x60, "3 Coins / 5 Credits"		},
	{0x20, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"		},
	{0x20, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"		},
	{0x20, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x20, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"		},
	{0x20, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"		},
	{0x20, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"		},
	{0x20, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"		},
	{0x20, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"		},
	{0x20, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"		},
	{0x20, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"		},
	{0x20, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"		},
	{0x20, 0x01, 0xf0, 0x00, "Free Play"				},
};

STDDIPINFO(Denjinmk)

static struct BurnDIPInfo GrainbowDIPList[]=
{
	{0x24, 0xff, 0xff, 0xff, NULL						},
	{0x25, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x24, 0x01, 0x03, 0x02, "2"						},
	{0x24, 0x01, 0x03, 0x03, "3"						},
	{0x24, 0x01, 0x03, 0x00, "5"						},
	{0x24, 0x01, 0x03, 0x01, "4"						},

	{0   , 0xfe, 0   ,    3, "Difficulty"				},
	{0x24, 0x01, 0x0c, 0x0c, "2"						},
	{0x24, 0x01, 0x0c, 0x08, "0"						},
	{0x24, 0x01, 0x0c, 0x04, "4"						},

	{0   , 0xfe, 0   ,    16, "Coin A"					},
	{0x24, 0x01, 0xf0, 0x00, "Free Play"				},
	{0x24, 0x01, 0xf0, 0xa0, "6 Coins 1 Credits"		},
	{0x24, 0x01, 0xf0, 0xb0, "5 Coins 1 Credits"		},
	{0x24, 0x01, 0xf0, 0xc0, "4 Coins 1 Credits"		},
	{0x24, 0x01, 0xf0, 0xd0, "3 Coins 1 Credits"		},
	{0x24, 0x01, 0xf0, 0x10, "8 Coins 3 Credits"		},
	{0x24, 0x01, 0xf0, 0xe0, "2 Coins 1 Credits"		},
	{0x24, 0x01, 0xf0, 0x20, "5 Coins 3 Credits"		},
	{0x24, 0x01, 0xf0, 0x30, "3 Coins 2 Credits"		},
	{0x24, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x24, 0x01, 0xf0, 0x40, "2 Coins 3 Credits"		},
	{0x24, 0x01, 0xf0, 0x90, "1 Coin  2 Credits"		},
	{0x24, 0x01, 0xf0, 0x80, "1 Coin  3 Credits"		},
	{0x24, 0x01, 0xf0, 0x70, "1 Coin  4 Credits"		},
	{0x24, 0x01, 0xf0, 0x60, "1 Coin  5 Credits"		},
	{0x24, 0x01, 0xf0, 0x50, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"				},
	{0x25, 0x01, 0x01, 0x01, "Off"						},
	{0x25, 0x01, 0x01, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x25, 0x01, 0x80, 0x80, "Off"						},
	{0x25, 0x01, 0x80, 0x00, "On"						},
};

STDDIPINFO(Grainbow)

static inline void legionna_ctc_write(INT32 offset, UINT16 data)
{
	offset &= 0x7e;

	if (offset == 0x1a) flipscreen = data & 1;
	if (offset == 0x1c) layer_disable = data;

	if (offset >= 0x20 && offset <= 0x2b) {
		scroll[(offset - 0x20) / 2] = data;
		return;
	}
	if (offset >= 0x2c && offset <= 0x3b) {
		offset = (offset - 0x2c) / 2;
		if (offset == 7) scroll[6] = data; // godzilla
		return;
	}
}

static inline void legionna_common_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x100470: // tile bank?
			background_bank = (data >> 2) & 0x1000;
			foreground_bank = (data >> 1) & 0x1000;
			midground_bank  = (data >> 3) & 0x1000;
		break;

		case 0x100680:	// irq ack?
		break;
	}

	if (address >= 0x100400 && address <= 0x1006ff) {
		seibu_cop_write(address & 0x3ff, data);
		return;
	}
	bprintf(0, _T("ww: %X  %x   PC:%X\n"), address, data, SekGetPC(-1));
}

static inline void legionna_common_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x100470:
		case 0x100471:
	//		bprintf (0, _T("WB: tile bank: %5.5x, %2.2x\n"), address, data);
		break;

		case 0x100680:	// irq ack?
		case 0x100681:
		break;
	}

	bprintf(0, _T("wB: %X  %x   PC:%X\n"), address, data, SekGetPC(-1));
}

static inline UINT16 legionna_common_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x100740:
			return (DrvDips[1] * 256) + DrvDips[0];

		case 0x100744:
			return DrvInputs[0];

		case 0x100748:
			return DrvInputs[1];

		case 0x10074c:
			return DrvInputs[2];

		case 0x10075c:
			return 0xffff; // dip bank 2 (unused)
	}

	if (address >= 0x100400 && address <= 0x1006ff) {
		return seibu_cop_read(address & 0x3fe);
	}

	return 0;
}

static void __fastcall legionna_main_write_word(UINT32 address, UINT16 data)
{
	if (address >= 0x100600 && address <= 0x10064f) {
		legionna_ctc_write(address - 0x100600, data);
		return;
	}

	if (address >= 0x100700 && address <= 0x10071f) {
		seibu_main_word_write((address-0x100700)/2, data);
		return;
	}

	legionna_common_write_word(address, data);
}

static void __fastcall legionna_main_write_byte(UINT32 address, UINT8 data)
{
	if (address >= 0x100600 && address <= 0x10064f) {
	//	bprintf (0, _T("CTC WB: %5.5x, %2.2x\n"), address, data);
		return;
	}

	if (address >= 0x100700 && address <= 0x10071f) {
		seibu_main_word_write((address-0x100700)/2, data);
		return;
	}

	legionna_common_write_byte(address, data);
}

static UINT16 __fastcall legionna_main_read_word(UINT32 address)
{
	if (address >= 0x100600 && address <= 0x10064f) {
	//	bprintf (0, _T("CTC RW: %5.x\n"));
		return 0;
	}
	if (address >= 0x100640 && address <= 0x10068f) {
	//	bprintf (0, _T("CTC RW: %5.x\n"));
		return 0;
	}

	if (address >= 0x100700 && address <= 0x10071f) {
		if (denjinmk_hack) {
			if (address == 0x100714) return 1;
			return seibu_main_word_read(((address & 0x1f)/2) & 7);
		}
		return seibu_main_word_read((address & 0x1f)/2);
	}

	return legionna_common_read_word(address);
}

static UINT8 __fastcall legionna_main_read_byte(UINT32 address)
{
	return SekReadWord(address) >> ((~address & 1) * 8);
}

static void __fastcall heatbrl_main_write_word(UINT32 address, UINT16 data)
{
	if (address >= 0x100640 && address <= 0x10068f) {
		legionna_ctc_write(address- 0x100640, data);
		return;
	}

	if (address >= 0x1007c0 && address <= 0x1007df) {
		seibu_main_word_write((address-0x1007c0)/2, data);
		return;
	}

	legionna_common_write_word(address, data);
}

static void __fastcall heatbrl_main_write_byte(UINT32 address, UINT8 data)
{
	if (address >= 0x100640 && address <= 0x10068f) {
	//	bprintf (0, _T("CTC WB: %5.5x, %2.2x\n"), address, data);
		return;
	}

	if (address >= 0x1007c0 && address <= 0x1007df) {
		seibu_main_word_write((address-0x1007c0)/2, data);
		bprintf(0, _T("sound wb? %X\n"), address);
		return;
	}
	bprintf(0, _T("wb: %X  %X  PC:%X\n"), address, data, SekGetPC(-1));
	legionna_common_write_byte(address, data);
}

static UINT16 __fastcall heatbrl_main_read_word(UINT32 address)
{
	if (address >= 0x100640 && address <= 0x10068f) {
	//	bprintf (0, _T("CTC RW: %5.x\n"));
		return 0;
	}

	if (address >= 0x1007c0 && address <= 0x1007df) {
		return seibu_main_word_read((address & 0x1f)/2);
	}

	return legionna_common_read_word(address);
}

static void __fastcall denjinmk_palette_write_word(UINT32 address, UINT16 data)
{
	*((UINT16*)(DrvAllRAM + 0x3800 + ((address & 0xffe) ^ 2))) = data;
}

static void __fastcall denjinmk_palette_write_byte(UINT32 address, UINT8 data)
{
	DrvAllRAM[0x3800 + ((address & 0xfff) ^ 3)] = data;
}

static void sample_bankswitch(INT32 data)
{
	if (sample_bank != (data & 1))
	{
		sample_bank = data & 1;

		MSM6295SetBank(0, DrvSndROM + sample_bank * 0x40000, 0, 0x3ffff);
	}
}

static void __fastcall godzilla_sound_write_port(UINT16 address, UINT8 data)
{
	switch (address & 0xff)
	{
		case 0x00:
			sample_bankswitch(data);
		return;
	}
}

static void videowrite_cb_w(INT32 offset, UINT16 data, UINT16 /*mask*/)
{
	offset *= 2;

	if (offset < 0x800)
	{
		*((UINT16*)(DrvBgBuf + offset - 0x0000)) = data;
	}
	else if (offset < 0x1000)
	{
		*((UINT16*)(DrvFgBuf + offset - 0x0800)) = data;
	}
	else if (offset < 0x1800)
	{
		*((UINT16*)(DrvMgBuf + offset - 0x1000)) = data;
	}
	else if (offset < 0x2800)
	{
		*((UINT16*)(DrvTxBuf + offset - 0x1800)) = data;
	}
}

static void palette_write_xbgr555(INT32 entry, UINT16 data)
{
	UINT8 b = (data >> 10) & 0x1f;
	UINT8 g = (data >>  5) & 0x1f;
	UINT8 r = (data >>  0) & 0x1f;

	b = (b << 3) | (b >> 2);
	g = (g << 3) | (g >> 2);
	r = (r << 3) | (r >> 2);

	DrvPalBuf16[entry] = data; // buffer palette
	DrvPalette[entry] = BurnHighCol(r, g, b, 0);
}

static tilemap_callback( bg )
{
	GenericTilesGfx *gfx = &GenericGfxData[3];

	UINT16 attr = *((UINT16*)(DrvBgBuf + offs * 2));
	INT32 code = ((attr & 0xfff) | background_bank) % gfx->code_mask;
	INT32 flags = DrvTransTable[3][code] ? TILE_SKIP : 0;

	TILE_SET_INFO(3, code, attr >> 12, flags);
}

static tilemap_callback( mg )
{
	GenericTilesGfx *gfx = &GenericGfxData[4];
	UINT16 attr = *((UINT16*)(DrvMgBuf + offs * 2));
	INT32 code = ((attr & 0xfff) | 0x1000) % gfx->code_mask;
	INT32 flags = DrvTransTable[4][code] ? TILE_SKIP : 0;

	TILE_SET_INFO(4, code, attr >> 12, flags);
}

static tilemap_callback( mgh )
{
	GenericTilesGfx *gfx = &GenericGfxData[4];
	UINT16 attr = *((UINT16*)(DrvMgBuf + offs * 2));
	INT32 code = ((attr & 0xfff) | midground_bank) % gfx->code_mask;
	INT32 flags = DrvTransTable[4][code] ? TILE_SKIP : 0;

	TILE_SET_INFO(4, code, attr >> 12, flags);
}

static tilemap_callback( fg )
{
	GenericTilesGfx *gfx = &GenericGfxData[1];
	UINT16 attr = *((UINT16*)(DrvFgBuf + offs * 2));
	INT32 code = ((attr & 0xfff) | foreground_bank) % gfx->code_mask;
	INT32 flags = DrvTransTable[1][code] ? TILE_SKIP : 0;

	TILE_SET_INFO(1, code, attr >> 12, flags);
}

static tilemap_callback( tx )
{
	GenericTilesGfx *gfx = &GenericGfxData[0];
	UINT16 attr = *((UINT16*)(DrvTxBuf + offs * 2));
	INT32 code = (attr & 0xfff) % gfx->code_mask;
	INT32 flags = DrvTransTable[0][code] ? TILE_SKIP : 0;

	TILE_SET_INFO(0, code, attr >> 12, flags);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	sample_bank = -1;
	sample_bankswitch(0);
	seibu_cop_reset();

	seibu_sound_reset();

	background_bank = 0;
	foreground_bank = 0;
	midground_bank = 0;
	layer_disable = 0;
	flipscreen = 0;
	memset (scroll, 0, sizeof(scroll));
	memset (coin_inserted_counter, 0, sizeof(coin_inserted_counter));

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM			= Next; Next += 0x100000;
	SeibuZ80ROM			= Next;
	DrvZ80ROM			= Next; Next += 0x020000;

	DrvGfxROM0			= Next; Next += 0x040000;
	DrvGfxROM1			= Next; Next += 0x200000;
	DrvGfxROM2			= Next; Next += sprite_size * 2;
	DrvGfxROM3			= Next; Next += 0x200000;
	DrvGfxROM4			= Next; Next += 0x200000;

	DrvTransTable[0]	= Next; Next += 0x040000 / (8 * 8);
	DrvTransTable[1]	= Next; Next += 0x200000 / (16 * 16);
	DrvTransTable[3]	= Next; Next += 0x200000 / (16 * 16);
	DrvTransTable[4]	= Next; Next += 0x200000 / (16 * 16);

	MSM6295ROM			= Next;
	DrvSndROM			= Next; Next += 0x080000;

	DrvPalette			= (UINT32*)Next; Next += 0x1000 * sizeof(UINT32);

	AllRam				= Next;

	SeibuZ80RAM			= Next;
	DrvZ80RAM			= Next; Next += 0x000800;

	Drv1KRAM			= Next; Next += 0x000400;
	DrvAllRAM			= Next; Next += 0x020000;

	DrvBgBuf			= Next; Next += 0x000800;
	DrvFgBuf			= Next; Next += 0x000800;
	DrvMgBuf			= Next; Next += 0x000800;
	DrvTxBuf			= Next; Next += 0x001000;
	DrvPalBuf16			= (UINT16*)Next; Next += 0x002000;

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static void DrvCalculateTransTable(UINT8 *tab, UINT8 *gfx, INT32 len, INT32 tilesize, INT32 transvalue)
{
	memset (tab, 0xff, len/tilesize);

	for (INT32 i = 0; i < len; i ++)
	{
		if (gfx[i] != transvalue) {
			tab[i/tilesize] = 0;
			i |= tilesize-1;
		}
	}
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[4]  = { STEP4(0, 4) };
	INT32 XOffs[16] = { STEP4(3, -1), STEP4(4*4+3, -1), STEP4(4*8*16+3, -1), STEP4(4*8*16+4*4+3, -1) };
	INT32 YOffs[16] = { STEP16(0, 4*8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x100000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x020000);

	GfxDecode(0x1000, 4,  8,  8, Plane, XOffs, YOffs, 0x100, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x100000);

	GfxDecode(0x2000, 4, 16, 16, Plane, XOffs, YOffs, 0x400, tmp, DrvGfxROM1);

	for (INT32 i = sprite_size - 0x100000; i >= 0; i -= 0x100000)
	{
		memcpy (tmp, DrvGfxROM2 + i, 0x100000);

		GfxDecode(0x2000, 4, 16, 16, Plane, XOffs, YOffs, 0x400, tmp, DrvGfxROM2 + i * 2);
	}

	memcpy (tmp, DrvGfxROM3, 0x100000);

	GfxDecode(0x2000, 4, 16, 16, Plane, XOffs, YOffs, 0x400, tmp, DrvGfxROM3);

	memcpy (tmp, DrvGfxROM4, 0x100000);

	GfxDecode(0x2000, 4, 16, 16, Plane, XOffs, YOffs, 0x400, tmp, DrvGfxROM4);

	BurnFree(tmp);

	DrvCalculateTransTable(DrvTransTable[0], DrvGfxROM0, 0x040000, 0x040, 0x0f);
	DrvCalculateTransTable(DrvTransTable[1], DrvGfxROM1, 0x200000, 0x100, 0x0f);
	DrvCalculateTransTable(DrvTransTable[3], DrvGfxROM3, 0x200000, 0x100, 0x0f);
	DrvCalculateTransTable(DrvTransTable[4], DrvGfxROM4, 0x200000, 0x100, 0x0f);

	return 0;
}

static void descramble_legionnaire_gfx()
{
	for (INT32 i = 0; i < 0x10000; i++)
	{
		DrvGfxROM1[i] = DrvGfxROM1[0x10000 | (i & 0x1f) | ((i & 0x60) << 9) | ((i & 0xff80) >> 2)];
	}
}

static INT32 LegionnaInit()
{
	sprite_size = 0x200000;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM  + 0x0000001,  k++, 4)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x0000000,  k++, 4)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x0000003,  k++, 4)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x0000002,  k++, 4)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x0000000,  k++, 1)) return 1;
		memcpy (DrvZ80ROM + 0x10000, DrvZ80ROM + 0x08000, 0x08000);
		memcpy (DrvZ80ROM + 0x18000, DrvZ80ROM + 0x00000, 0x08000);

		if (BurnLoadRom(DrvGfxROM1 + 0x0010000,  k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x0010001,  k++, 2)) return 1;

		memcpy (DrvGfxROM0, DrvGfxROM1 + 0x20000, 0x10000); // top half

		if (BurnLoadRom(DrvGfxROM2 + 0x0000000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x0100000,  k++, 1)) return 1;
		BurnByteswap(DrvGfxROM2, 0x200000);

		if (BurnLoadRom(DrvGfxROM3 + 0x0000000,  k++, 1)) return 1;
		BurnByteswap(DrvGfxROM3, 0x100000);

		memcpy (DrvGfxROM4, DrvGfxROM3, 0x100000);

		if (BurnLoadRom(DrvSndROM  + 0x0000000,  k++, 1)) return 1;

		descramble_legionnaire_gfx();
		DrvGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(Drv1KRAM,		0x100000, 0x1003ff, MAP_RAM);
	SekMapMemory(DrvAllRAM,		0x101000, 0x11ffff, MAP_RAM);
	SekSetWriteWordHandler(0,	legionna_main_write_word);
	SekSetWriteByteHandler(0,	legionna_main_write_byte);
	SekSetReadWordHandler(0,	legionna_main_read_word);
	SekSetReadByteHandler(0,	legionna_main_read_byte);
	SekClose();

	seibu_cop_config(1, videowrite_cb_w, palette_write_xbgr555);

	seibu_sound_init(0, 0x20000, 3579545, 3579545, 1000000 / 132);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, mg_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, fg_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(3, TILEMAP_SCAN_ROWS, tx_map_callback,  8,  8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 4,  8,  8, 0x020000, 0x300, 0xf);
	GenericTilemapSetGfx(1, DrvGfxROM1, 4, 16, 16, 0x020000, 0x200, 0xf);
	GenericTilemapSetGfx(2, DrvGfxROM2, 4, 16, 16, sprite_size * 2, 0x400, 0xf);
	GenericTilemapSetGfx(3, DrvGfxROM3, 4, 16, 16, 0x200000, 0x000, 0xf);
	GenericTilemapSetGfx(4, DrvGfxROM4, 4, 16, 16, 0x200000, 0x100, 0xf);
	GenericTilemapSetTransparent(0, 0xf);
	GenericTilemapSetTransparent(1, 0xf);
	GenericTilemapSetTransparent(2, 0xf);
	GenericTilemapSetTransparent(3, 0xf);

	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);

	DrvDoReset();

	return 0;
}

static INT32 HeatbrlInit()
{
	sprite_size = 0x200000;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM  + 0x0000001,  k++, 4)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x0000000,  k++, 4)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x0000003,  k++, 4)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x0000002,  k++, 4)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x0000000,  k++, 1)) return 1;
		memcpy (DrvZ80ROM + 0x10000, DrvZ80ROM + 0x08000, 0x08000);
		memcpy (DrvZ80ROM + 0x18000, DrvZ80ROM + 0x00000, 0x08000);

		if (BurnLoadRom(DrvGfxROM0 + 0x0000000,  k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0000001,  k++, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x0000000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x0100000,  k++, 1)) return 1;
		BurnByteswap(DrvGfxROM2, 0x200000);

		if (BurnLoadRom(DrvGfxROM3 + 0x0000000,  k++, 1)) return 1;
		BurnByteswap(DrvGfxROM3, 0x100000);

		if (BurnLoadRom(DrvGfxROM4 + 0x0000000,  k++, 1)) return 1;
		BurnByteswap(DrvGfxROM4, 0x080000);

		if (BurnLoadRom(DrvGfxROM1 + 0x0000000,  k++, 1)) return 1;
		BurnByteswap(DrvGfxROM1, 0x080000);

		if (BurnLoadRom(DrvSndROM  + 0x0000000,  k++, 1)) return 1;

		DrvGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(Drv1KRAM,		0x100000, 0x1003ff, MAP_RAM);
	SekMapMemory(DrvAllRAM,		0x100800, 0x11ffff, MAP_RAM);
	SekSetWriteWordHandler(0,	heatbrl_main_write_word);
	SekSetWriteByteHandler(0,	heatbrl_main_write_byte);
	SekSetReadWordHandler(0,	heatbrl_main_read_word);
	SekSetReadByteHandler(0,	legionna_main_read_byte);
	SekClose();

	seibu_cop_config(1, videowrite_cb_w, palette_write_xbgr555);

	seibu_sound_init(0, 0x20000, 3579545, 3579545, 1000000 / 132);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, mgh_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, fg_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(3, TILEMAP_SCAN_ROWS, tx_map_callback,  8,  8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 4,  8,  8, 0x040000, 0x300, 0xf);
	GenericTilemapSetGfx(1, DrvGfxROM1, 4, 16, 16, 0x100000, 0x200, 0xf);
	GenericTilemapSetGfx(2, DrvGfxROM2, 4, 16, 16, sprite_size * 2, 0x400, 0xf);
	GenericTilemapSetGfx(3, DrvGfxROM3, 4, 16, 16, 0x200000, 0x000, 0xf);
	GenericTilemapSetGfx(4, DrvGfxROM4, 4, 16, 16, 0x100000, 0x100, 0xf); // mg
	GenericTilemapSetTransparent(0, 0xf);
	GenericTilemapSetTransparent(1, 0xf);
	GenericTilemapSetTransparent(2, 0xf);
	GenericTilemapSetTransparent(3, 0xf);

	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, 0);

	DrvDoReset();

	return 0;
}

static INT32 GodzillaInit()
{
	sprite_size = 0x600000;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM  + 0x0000001,  k++, 4)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x0000000,  k++, 4)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x0000003,  k++, 4)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x0000002,  k++, 4)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x0000000,  k++, 1)) return 1;
		memcpy (DrvZ80ROM + 0x10000, DrvZ80ROM + 0x08000, 0x08000);
		memcpy (DrvZ80ROM + 0x18000, DrvZ80ROM + 0x00000, 0x08000);

		if (BurnLoadRom(DrvGfxROM0 + 0x0000000,  k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0000001,  k++, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x0000000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x0200000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x0400000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x0500000,  k++, 1)) return 1;
		BurnByteswap(DrvGfxROM2, 0x600000);

		if (BurnLoadRom(DrvGfxROM3 + 0x0000000,  k++, 1)) return 1;
		BurnByteswap(DrvGfxROM3, 0x100000);

		memcpy (DrvGfxROM4, DrvGfxROM3, 0x100000);

		if (BurnLoadRom(DrvGfxROM1 + 0x0000000,  k++, 1)) return 1;
		BurnByteswap(DrvGfxROM1, 0x100000);

		if (BurnLoadRom(DrvSndROM  + 0x0000000,  k++, 1)) return 1;

		DrvGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(Drv1KRAM,		0x100000, 0x1003ff, MAP_RAM);
	SekMapMemory(DrvAllRAM,		0x100800, 0x11ffff, MAP_RAM);
	SekSetWriteWordHandler(0,	legionna_main_write_word);
	SekSetWriteByteHandler(0,	legionna_main_write_byte);
	SekSetReadWordHandler(0,	legionna_main_read_word);
	SekSetReadByteHandler(0,	legionna_main_read_byte);
	SekClose();

	seibu_cop_config(1, videowrite_cb_w, palette_write_xbgr555);

	seibu_sound_init(1, 0x20000, 3579545, 3579545, 1000000 / 132);

	ZetOpen(0);
	ZetSetOutHandler(godzilla_sound_write_port);
	ZetClose();

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, mg_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, fg_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(3, TILEMAP_SCAN_ROWS, tx_map_callback,  8,  8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 4,  8,  8, 0x040000, 0x300, 0xf);
	GenericTilemapSetGfx(1, DrvGfxROM1, 4, 16, 16, 0x200000, 0x200, 0xf);
	GenericTilemapSetGfx(2, DrvGfxROM2, 4, 16, 16, sprite_size * 2, 0x400, 0xf);
	GenericTilemapSetGfx(3, DrvGfxROM3, 4, 16, 16, 0x200000, 0x000, 0xf);
	GenericTilemapSetGfx(4, DrvGfxROM4, 4, 16, 16, 0x200000, 0x100, 0xf); // mg
	GenericTilemapSetTransparent(0, 0xf);
	GenericTilemapSetTransparent(1, 0xf);
	GenericTilemapSetTransparent(2, 0xf);
	GenericTilemapSetTransparent(3, 0xf);

	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, 0);
	GenericTilemapSetOffsets(3, 4, 4); // text layer offset

	*((UINT16*)(Drv68KROM + 0xbe0e + 0x0a)) = 0xb000; // fix collisions (hack)
	*((UINT16*)(Drv68KROM + 0xbe0e + 0x1a)) = 0xb800;
	*((UINT16*)(Drv68KROM + 0xbb0a + 0x0a)) = 0xb000;
	*((UINT16*)(Drv68KROM + 0xbb0a + 0x1a)) = 0xb800;
	*((UINT16*)(Drv68KROM + 0x3fffe)) = 0x61ba; // checksum

	DrvDoReset();

	return 0;
}

static INT32 DenjinmkInit()
{
	sprite_size = 0x500000;

	BurnSetRefreshRate(56.00);

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM  + 0x0000001,  k++, 4)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x0000000,  k++, 4)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x0000003,  k++, 4)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x0000002,  k++, 4)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x0000000,  k++, 1)) return 1;
		memcpy (DrvZ80ROM + 0x10000, DrvZ80ROM + 0x08000, 0x08000);
		memcpy (DrvZ80ROM + 0x18000, DrvZ80ROM + 0x00000, 0x08000);

		if (BurnLoadRom(DrvGfxROM0 + 0x0000000,  k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0000001,  k++, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x0000000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x0200000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x0300000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x0400000,  k++, 1)) return 1;
		BurnByteswap(DrvGfxROM2, 0x500000);

		if (BurnLoadRom(DrvGfxROM3 + 0x0000000,  k++, 1)) return 1;
		BurnByteswap(DrvGfxROM3, 0x100000);

		if (BurnLoadRom(DrvGfxROM4 + 0x0000000,  k++, 1)) return 1;
		BurnByteswap(DrvGfxROM4, 0x100000);

		if (BurnLoadRom(DrvGfxROM1 + 0x0000000,  k++, 1)) return 1;
		BurnByteswap(DrvGfxROM1, 0x100000);

		if (BurnLoadRom(DrvSndROM  + 0x0000000,  k++, 1)) return 1;

		DrvGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(Drv1KRAM,		0x100000, 0x1003ff, MAP_RAM);
	SekMapMemory(DrvAllRAM,		0x100800, 0x11ffff, MAP_RAM);
	SekSetWriteWordHandler(0,	legionna_main_write_word);
	SekSetWriteByteHandler(0,	legionna_main_write_byte);
	SekSetReadWordHandler(0,	legionna_main_read_word);
	SekSetReadByteHandler(0,	legionna_main_read_byte);

	SekMapHandler(1,			0x104000, 0x104fff, MAP_WRITE);
	SekSetWriteByteHandler(1,	denjinmk_palette_write_byte);
	SekSetWriteWordHandler(1,	denjinmk_palette_write_word);
	SekClose();

	seibu_cop_config(1, videowrite_cb_w, palette_write_xbgr555);

	seibu_sound_init(1, 0x20000, 3579545, 3579545, 1000000 / 132);
	coin_hold_length = 2; // this game only likes coin held for 2 frames
	denjinmk_hack = 1; // special handling for seibusnd reads

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, mgh_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, fg_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(3, TILEMAP_SCAN_ROWS, tx_map_callback,  8,  8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 4,  8,  8, 0x040000, 0x300, 0xf);
	GenericTilemapSetGfx(1, DrvGfxROM1, 4, 16, 16, 0x200000, 0x200, 0xf);
	GenericTilemapSetGfx(2, DrvGfxROM2, 4, 16, 16, sprite_size * 2, 0x400, 0xf);
	GenericTilemapSetGfx(3, DrvGfxROM3, 4, 16, 16, 0x200000, 0x000, 0xf);
	GenericTilemapSetGfx(4, DrvGfxROM4, 4, 16, 16, 0x200000, 0x100, 0xf); // mg
	GenericTilemapSetTransparent(0, 0xf);
	GenericTilemapSetTransparent(1, 0xf);
	GenericTilemapSetTransparent(2, 0xf);
	GenericTilemapSetTransparent(3, 0x7);

	DrvCalculateTransTable(DrvTransTable[0], DrvGfxROM0, 0x040000, 0x040, 0x07);

	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, 0);

	DrvDoReset();

	return 0;
}

static INT32 GrainbowInit()
{
	sprite_size = 0x200000;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM  + 0x0000001,  k++, 4)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x0000000,  k++, 4)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x0000003,  k++, 4)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x0000002,  k++, 4)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x0000000,  k++, 1)) return 1;
		memcpy (DrvZ80ROM + 0x10000, DrvZ80ROM + 0x08000, 0x08000);
		memcpy (DrvZ80ROM + 0x18000, DrvZ80ROM + 0x00000, 0x08000);

		if (BurnLoadRom(DrvGfxROM0 + 0x0000000,  k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0000001,  k++, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x0000000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x0100000,  k++, 1)) return 1;
		BurnByteswap(DrvGfxROM2, 0x200000);

		if (BurnLoadRom(DrvGfxROM3 + 0x0000000,  k++, 1)) return 1;
		BurnByteswap(DrvGfxROM3, 0x100000);

		memcpy (DrvGfxROM4, DrvGfxROM3, 0x100000); // mg

		if (BurnLoadRom(DrvGfxROM1 + 0x0000000,  k++, 1)) return 1;
		BurnByteswap(DrvGfxROM1, 0x100000);

		if (BurnLoadRom(DrvSndROM  + 0x0000000,  k++, 1)) return 1;

		DrvGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(Drv1KRAM,		0x100000, 0x1003ff, MAP_RAM);
	SekMapMemory(DrvAllRAM,		0x100800, 0x11ffff, MAP_RAM);
	SekSetWriteWordHandler(0,	legionna_main_write_word);
	SekSetWriteByteHandler(0,	legionna_main_write_byte);
	SekSetReadWordHandler(0,	legionna_main_read_word);
	SekSetReadByteHandler(0,	legionna_main_read_byte);
	SekClose();

	seibu_cop_config(1, videowrite_cb_w, palette_write_xbgr555);

	seibu_sound_init(1, 0x20000, 3579545, 3579545, 1000000 / 132);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, mg_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, fg_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(3, TILEMAP_SCAN_ROWS, tx_map_callback,  8,  8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 4,  8,  8, 0x040000, 0x300, 0xf);
	GenericTilemapSetGfx(1, DrvGfxROM1, 4, 16, 16, 0x200000, 0x200, 0xf);
	GenericTilemapSetGfx(2, DrvGfxROM2, 4, 16, 16, sprite_size * 2, 0x400, 0xf);
	GenericTilemapSetGfx(3, DrvGfxROM3, 4, 16, 16, 0x200000, 0x000, 0xf);
	GenericTilemapSetGfx(4, DrvGfxROM4, 4, 16, 16, 0x200000, 0x100, 0xf); // mg
	GenericTilemapSetTransparent(0, 0xf);
	GenericTilemapSetTransparent(1, 0xf);
	GenericTilemapSetTransparent(2, 0xf);
	GenericTilemapSetTransparent(3, 0xf);

	GenericTilemapSetOffsets(TMAP_GLOBAL, -16, -16);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	SekExit();
	seibu_sound_exit();

	coin_hold_length = 4;

	denjinmk_hack = 0;

	BurnFree(AllMem);

	return 0;
}

static void draw_sprites(INT32 ram_offset, UINT16 *pri_masks, INT32 ext_bank, INT32 sprite_xoffs, INT32 sprite_yoffs)
{
	GenericTilesGfx *gfx = &GenericGfxData[2];
	INT32 screen_mask = (nScreenWidth < 320) ? 0x200 : 0x1000;
	UINT16 *spriteram = (UINT16*)(DrvAllRAM + ram_offset);

	for (INT32 offs = 0; offs < 0x400; offs += 4)
	{
		UINT16 data = BURN_ENDIAN_SWAP_INT16(spriteram[offs]);
		if (!(data & 0x8000)) continue;

		INT32 pri_mask = 0;

		if (pri_masks == NULL)
		{
			UINT8 cur_pri = (BURN_ENDIAN_SWAP_INT16(spriteram[offs+1]) >> 14) | ((data & 0x40) >> 4);

			switch (cur_pri)
			{
				case 0: pri_mask = 0xff00; break; // gumdam swamp monster l2
				case 1: pri_mask = 0xff00; break; // cupsoc
				case 2: pri_mask = 0xfffc; break; // masking effect for gundam l2 monster
				case 3: pri_mask = 0xfffc; break; // cupsoc (not sure what..)
				case 4: pri_mask = 0xffe0; break; // gundam level 2/3 player
				case 6: pri_mask = 0; break; // insert coin in gundam
			}
		}
		else
		{
			pri_mask = pri_masks[BURN_ENDIAN_SWAP_INT16(spriteram[offs+1]) >> 14];
		}

		INT32 sprite = (BURN_ENDIAN_SWAP_INT16(spriteram[offs+1]) & 0x3fff);
		if (ext_bank) sprite |= ((data & 0x0040) << 8) | (BURN_ENDIAN_SWAP_INT16(spriteram[offs+3]) & 0x8000);

		INT32 y = BURN_ENDIAN_SWAP_INT16(spriteram[offs+3]) & (screen_mask - 1);
		INT32 x = BURN_ENDIAN_SWAP_INT16(spriteram[offs+2]) & (screen_mask - 1);
		if (x & (screen_mask / 2)) x -= screen_mask;
		if (y & (screen_mask / 2)) y -= screen_mask;

		INT32 color	= ((data & 0x003f) << gfx->depth) + gfx->color_offset;
		INT32 flipx	=  data & 0x4000;
		INT32 flipy	=  data & 0x2000;
		INT32 dy	= ((data & 0x0380) >> 7)  + 1;
		INT32 dx	= ((data & 0x1c00) >> 10) + 1;

		for (INT32 ax = 0; ax < dx; ax++)
		{
			for (INT32 ay = 0; ay < dy; ay++)
			{
				INT32 xpos = (flipx ? (x+(dx-ax-1)*16) : (x+ax*16)) + sprite_xoffs;
				INT32 ypos = (flipy ? (y+(dy-ay-1)*16) : (y+ay*16)) + sprite_yoffs;

				RenderPrioSprite(pTransDraw, gfx->gfxbase, sprite++ % gfx->code_mask, color, 0xf, xpos, ypos, flipx, flipy, 16, 16, pri_mask);
			}
		}
	}
}

static INT32 LegionnaDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x800; i++) {
			palette_write_xbgr555(i, DrvPalBuf16[i]);
		}
		DrvPalette[0x800] = 0;
		DrvRecalc = 0;
	}

	static UINT16 pri_masks[4] = { 0, 0xfff0, 0xfffc, 0xfffe };

	GenericTilemapSetScrollX(0, scroll[0]);
	GenericTilemapSetScrollY(0, scroll[1]);
	GenericTilemapSetScrollX(1, scroll[2]);
	GenericTilemapSetScrollY(1, scroll[3]);
	GenericTilemapSetScrollX(2, scroll[4]);
	GenericTilemapSetScrollY(2, scroll[5]);

	BurnTransferClear(0x800);

	if ((layer_disable & 0x0001) == 0 && (nBurnLayer & 1)) GenericTilemapDraw(1, pTransDraw, 0);
	if ((layer_disable & 0x0002) == 0 && (nBurnLayer & 2)) GenericTilemapDraw(0, pTransDraw, 1);
	if ((layer_disable & 0x0004) == 0 && (nBurnLayer & 4)) GenericTilemapDraw(2, pTransDraw, 2);
	if ((layer_disable & 0x0008) == 0 && (nBurnLayer & 8)) GenericTilemapDraw(3, pTransDraw, 4);

	if ((layer_disable & 0x0010) == 0 && (nSpriteEnable & 1)) draw_sprites(0x5000 - 0x1000, pri_masks, 0, 0, -16);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 HeatbrlDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x800; i++) {
			palette_write_xbgr555(i, DrvPalBuf16[i]);
		}
		DrvPalette[0x800] = 0;
		DrvRecalc = 0;
	}

	static UINT16 pri_masks[4] = { 0xfff0, 0xfffc, 0xfffe, 0xffff };

	GenericTilemapSetScrollX(0, scroll[0]);
	GenericTilemapSetScrollY(0, scroll[1]);
	GenericTilemapSetScrollX(1, scroll[2]);
	GenericTilemapSetScrollY(1, scroll[3]);
	GenericTilemapSetScrollX(2, scroll[4]);
	GenericTilemapSetScrollY(2, scroll[5]);

	BurnTransferClear(0x800);

	if ((layer_disable & 0x0004) == 0 && (nBurnLayer & 1)) GenericTilemapDraw(2, pTransDraw, 0);
	if ((layer_disable & 0x0002) == 0 && (nBurnLayer & 2)) GenericTilemapDraw(1, pTransDraw, 1);
	if ((layer_disable & 0x0001) == 0 && (nBurnLayer & 4)) GenericTilemapDraw(0, pTransDraw, 2);
	if ((layer_disable & 0x0008) == 0 && (nBurnLayer & 8)) GenericTilemapDraw(3, pTransDraw, 4);

	if ((layer_disable & 0x0010) == 0 && (nSpriteEnable & 1)) draw_sprites(0x3000-0x800, pri_masks, 0, 0, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 GodzillaDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x800; i++) {
			palette_write_xbgr555(i, DrvPalBuf16[i]);
		}
		DrvPalette[0x800] = 0;
		DrvRecalc = 0;
	}

	static UINT16 pri_masks[4] = { 0xfff0, 0xfffc, 0xfffe, 0xffff };

	GenericTilemapSetScrollX(0, scroll[0]);
	GenericTilemapSetScrollY(0, scroll[1]);
	GenericTilemapSetScrollX(1, scroll[2]);
	GenericTilemapSetScrollY(1, scroll[3]);
	GenericTilemapSetScrollX(2, scroll[4]);
	GenericTilemapSetScrollY(2, scroll[5]);
	GenericTilemapSetScrollX(3, (0x1ef) - scroll[6]);

	BurnTransferClear(0xff);

	if ((layer_disable & 0x0001) == 0 && (nBurnLayer & 1)) GenericTilemapDraw(0, pTransDraw, 0);
	if ((layer_disable & 0x0002) == 0 && (nBurnLayer & 2)) GenericTilemapDraw(1, pTransDraw, 1);
	if ((layer_disable & 0x0004) == 0 && (nBurnLayer & 4)) GenericTilemapDraw(2, pTransDraw, 2);
	if ((layer_disable & 0x0008) == 0 && (nBurnLayer & 8)) GenericTilemapDraw(3, pTransDraw, 4);

	if ((layer_disable & 0x0010) == 0 && (nSpriteEnable & 1)) draw_sprites(0x5000-0x800, pri_masks, 1, 0, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DenjinmkDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x800; i++) {
			palette_write_xbgr555(i, DrvPalBuf16[i]);
		}
		DrvPalette[0x800] = 0;
		DrvRecalc = 0;
	}

	static UINT16 pri_masks[4] = { 0xfff0, 0xfffc, 0xfffe, 0x0000 };

	GenericTilemapSetScrollX(0, scroll[0]);
	GenericTilemapSetScrollY(0, scroll[1]);
	GenericTilemapSetScrollX(1, scroll[2]);
	GenericTilemapSetScrollY(1, scroll[3]);
	GenericTilemapSetScrollX(2, scroll[4]);
	GenericTilemapSetScrollY(2, scroll[5]);

	BurnTransferClear(0xff);

	if ((layer_disable & 0x0001) == 0 && (nBurnLayer & 1)) GenericTilemapDraw(0, pTransDraw, 0);
	if ((layer_disable & 0x0002) == 0 && (nBurnLayer & 2)) GenericTilemapDraw(1, pTransDraw, 1);
	if ((layer_disable & 0x0004) == 0 && (nBurnLayer & 4)) GenericTilemapDraw(2, pTransDraw, 2);
	if ((layer_disable & 0x0008) == 0 && (nBurnLayer & 8)) GenericTilemapDraw(3, pTransDraw, 4);

	if ((layer_disable & 0x0010) == 0 && (nSpriteEnable & 1)) draw_sprites(0x5000-0x800, pri_masks, 1, 0, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 GrainbowDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x800; i++) {
			palette_write_xbgr555(i, DrvPalBuf16[i]);
		}
		DrvPalette[0x800] = 0;
		DrvRecalc = 0;
	}

	GenericTilemapSetScrollX(0, scroll[0]);
	GenericTilemapSetScrollY(0, scroll[1]);
	GenericTilemapSetScrollX(1, scroll[2]);
	GenericTilemapSetScrollY(1, scroll[3]);
	GenericTilemapSetScrollX(2, scroll[4]);
	GenericTilemapSetScrollY(2, scroll[5]);

	BurnTransferClear(0x800);

	if ((layer_disable & 0x0001) == 0 && (nBurnLayer & 1)) GenericTilemapDraw(0, pTransDraw, 1, 0xff);
	if ((layer_disable & 0x0002) == 0 && (nBurnLayer & 2)) GenericTilemapDraw(1, pTransDraw, 2, 0xff);
	if ((layer_disable & 0x0004) == 0 && (nBurnLayer & 4)) GenericTilemapDraw(2, pTransDraw, 4, 0xff);
	if ((layer_disable & 0x0008) == 0 && (nBurnLayer & 8)) GenericTilemapDraw(3, pTransDraw, 8, 0xff);

	if ((layer_disable & 0x0010) == 0 && (nSpriteEnable & 1)) draw_sprites(0x7000-0x800, NULL, 0, 0, 0);

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
		memset (DrvInputs, 0xff, sizeof(DrvInputs));
		seibu_coin_input = 0;

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			seibu_coin_input ^= (DrvJoy4[i] & 1) << i;
		}

		for (INT32 i = 0; i < 2; i++) {
			if (seibu_coin_input & (1 << i)) {
				coin_inserted_counter[i]++;
				if (coin_inserted_counter[i] >= coin_hold_length) seibu_coin_input &= ~(1 << i);
			} else {
				coin_inserted_counter[i] = 0;
			}
		}
	}

	INT32 nInterleave = 288;
	INT32 nCyclesTotal[2] = { 10000000 / 60, 3579545 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);
		if (i == (nInterleave - 1)) SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);

		BurnTimerUpdateYM3812((i + 1) * (nCyclesTotal[1] / nInterleave));
	}

	BurnTimerEndFrameYM3812(nCyclesTotal[1]);

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	if (pBurnSoundOut) {
		seibu_sound_update(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	SekClose();

	return 0;
}

static INT32 DrvYM2151Frame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));
		seibu_coin_input = 0;

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			seibu_coin_input ^= (DrvJoy4[i] & 1) << i;
		}

		for (INT32 i = 0; i < 2; i++) {
			if (seibu_coin_input & (1 << i)) {
				coin_inserted_counter[i]++;
				if (coin_inserted_counter[i] >= coin_hold_length) seibu_coin_input &= ~(1 << i);
			} else {
				coin_inserted_counter[i] = 0;
			}
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { (10000000 * 100) / nBurnFPS, (3579545 * 100) / nBurnFPS };
	INT32 nCyclesDone[2] = { 0, 0 };
	INT32 nSoundBufferPos = 0;

	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);
		if (i == (nInterleave - 1)) SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);

		CPU_RUN(1, Zet);

		if (pBurnSoundOut && ((i%4)==3)) {
			INT32 nSegmentLength = nBurnSoundLen / (nInterleave / 4);
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			seibu_sound_update(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength > 0) {
			seibu_sound_update(pSoundBuf, nSegmentLength);
		}
	}

	ZetClose();
	SekClose();

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

		seibu_cop_scan(nAction, pnMin);
		seibu_sound_scan(nAction, pnMin);

		SCAN_VAR(background_bank);
		SCAN_VAR(foreground_bank);
		SCAN_VAR(midground_bank);
		SCAN_VAR(layer_disable);
		SCAN_VAR(flipscreen);
		SCAN_VAR(scroll);
		SCAN_VAR(sample_bank);
		SCAN_VAR(coin_inserted_counter);
	}

	return 0;
}


// Legionnaire (World)

static struct BurnRomInfo legionnaRomDesc[] = {
	{ "1.u025",						0x020000, 0x9e2d3ec8, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "2.u024",						0x020000, 0x35c8a28f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.u026",						0x020000, 0x553fc7c0, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4a.u023",					0x020000, 0x2cc36c98, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "6.u1110",					0x010000, 0xfe7b8d06, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "7.u077",						0x010000, 0x88e26809, 3 | BRF_GRA },           //  5 Characters and Foreground Tiles
	{ "8.u072",						0x010000, 0x06e35407, 3 | BRF_GRA },           //  6

	{ "legionnire_obj1.u0815",		0x100000, 0xd35602f5, 4 | BRF_GRA },           //  7 Sprites
	{ "legionnire_obj2.u0814",		0x100000, 0x351d3917, 4 | BRF_GRA },           //  8

	{ "legionnire_back.u075",		0x100000, 0x58280989, 5 | BRF_GRA },           //  9 Background and Midground Tiles

	{ "5.u106",						0x020000, 0x21d09bde, 6 | BRF_SND },           // 10 Samples

	{ "copx-d1.u0330",				0x080000, 0x029bc402, 7 | BRF_PRG | BRF_ESS }, // 11 COP ROM

	{ "leg007.u0910",				0x000200, 0xcc6da568, 0 | BRF_GRA },           // 12 Priority PROM?
};

STD_ROM_PICK(legionna)
STD_ROM_FN(legionna)

struct BurnDriver BurnDrvLegionna = {
	"legionna", NULL, NULL, NULL, "1992",
	"Legionnaire (World)\0", NULL, "TAD Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, legionnaRomInfo, legionnaRomName, NULL, NULL, NULL, NULL, LegionnaInputInfo, LegionnaDIPInfo,
	LegionnaInit, DrvExit, DrvFrame, LegionnaDraw, DrvScan, &DrvRecalc, 0x801,
	256, 224, 4, 3
};


// Legionnaire (Japan)

static struct BurnRomInfo legionnajRomDesc[] = {
	{ "1.u025",						0x020000, 0x9e2d3ec8, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "2.u024",						0x020000, 0x35c8a28f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.u026",						0x020000, 0x553fc7c0, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.u023",						0x020000, 0x4c385dc7, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "6.u1110",					0x010000, 0xfe7b8d06, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "7.u077",						0x010000, 0x88e26809, 3 | BRF_GRA },           //  5 Characters and Foreground Tiles
	{ "8.u072",						0x010000, 0x06e35407, 3 | BRF_GRA },           //  6

	{ "legionnire_obj1.u0815",		0x100000, 0xd35602f5, 4 | BRF_GRA },           //  7 Sprites
	{ "legionnire_obj2.u0814",		0x100000, 0x351d3917, 4 | BRF_GRA },           //  8

	{ "legionnire_back.u075",		0x100000, 0x58280989, 5 | BRF_GRA },           //  9 Background and Midground Tiles

	{ "5.u106",						0x020000, 0x21d09bde, 6 | BRF_SND },           // 10 Samples

	{ "copx-d1.u0330",				0x080000, 0x029bc402, 7 | BRF_GRA },           // 11 COP ROM

	{ "leg007.u0910",				0x000200, 0xcc6da568, 8 | BRF_GRA },           // 12 Priority PROM?
};

STD_ROM_PICK(legionnaj)
STD_ROM_FN(legionnaj)

struct BurnDriver BurnDrvLegionnaj = {
	"legionnaj", "legionna", NULL, NULL, "1992",
	"Legionnaire (Japan)\0", NULL, "TAD Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, legionnajRomInfo, legionnajRomName, NULL, NULL, NULL, NULL, LegionnaInputInfo, LegionnaDIPInfo,
	LegionnaInit, DrvExit, DrvFrame, LegionnaDraw, DrvScan, &DrvRecalc, 0x801,
	256, 224, 4, 3
};


// Legionnaire (US)

static struct BurnRomInfo legionnauRomDesc[] = {
	{ "1.u025",						0x020000, 0x9e2d3ec8, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "2.u024",						0x020000, 0x35c8a28f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.u026",						0x020000, 0x553fc7c0, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.u023",						0x020000, 0x91fd4648, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "6.u1110",					0x010000, 0xfe7b8d06, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "7.u077",						0x010000, 0x88e26809, 3 | BRF_GRA },           //  5 Characters and Foreground Tiles
	{ "8.u072",						0x010000, 0x06e35407, 3 | BRF_GRA },           //  6

	{ "legionnire_obj1.u0815",		0x100000, 0xd35602f5, 4 | BRF_GRA },           //  7 Sprites
	{ "legionnire_obj2.u0814",		0x100000, 0x351d3917, 4 | BRF_GRA },           //  8

	{ "legionnire_back.u075",		0x100000, 0x58280989, 5 | BRF_GRA },           //  9 Background and Midground Tiles

	{ "5.u106",						0x020000, 0x21d09bde, 6 | BRF_SND },           // 10 Samples

	{ "copx-d1.u0330",				0x080000, 0x029bc402, 7 | BRF_GRA },           // 11 COP ROM

	{ "leg007.u0910",				0x000200, 0xcc6da568, 8 | BRF_GRA },           // 12 Priority PROM?
};

STD_ROM_PICK(legionnau)
STD_ROM_FN(legionnau)

struct BurnDriver BurnDrvLegionnau = {
	"legionnau", "legionna", NULL, NULL, "1992",
	"Legionnaire (US)\0", NULL, "TAD Corporation (Fabtek license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, legionnauRomInfo, legionnauRomName, NULL, NULL, NULL, NULL, LegionnaInputInfo, LegionnaDIPInfo,
	LegionnaInit, DrvExit, DrvFrame, LegionnaDraw, DrvScan, &DrvRecalc, 0x801,
	256, 224, 4, 3
};


// Heated Barrel (World version 3)

static struct BurnRomInfo heatbrlRomDesc[] = {
	{ "1e_ver3.9k",					0x020000, 0x6b41fbac, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "2e_ver3.9m",					0x020000, 0xdd21969b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3e_ver3.9f",					0x020000, 0x09544a91, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4e_ver3.9h",					0x020000, 0xebd34559, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "barrel_7.u1110",				0x010000, 0x0784dbd8, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "barrel_6.u077",				0x010000, 0xbea3c581, 3 | BRF_GRA },           //  5 Characters
	{ "barrel_5.u072",				0x010000, 0x5604d155, 3 | BRF_GRA },           //  6

	{ "heated-barrel_obj1.u085",	0x100000, 0xf7a7c31c, 4 | BRF_GRA },           //  7 Sprites
	{ "heated-barrel_obj2.u0814",	0x100000, 0x24236116, 4 | BRF_GRA },           //  8

	{ "heated-barrel_bg-1.u075",	0x100000, 0x2f5d8baa, 5 | BRF_GRA },           //  9 Background Tiles

	{ "heated-barrel_bg-2.u074",	0x080000, 0x77ee4c6f, 6 | BRF_GRA },           // 10 Midground Tiles

	{ "heated-barrel_bg-3.u076",	0x080000, 0x83850e2d, 7 | BRF_GRA },           // 11 Foreground Tiles

	{ "barrel_8.u106",				0x020000, 0x489e5b1d, 8 | BRF_SND },           // 12 Samples

	{ "copx-d2.u0339",				0x080000, 0x7c52581b, 9 | BRF_PRG | BRF_ESS }, // 13 COP ROM

	{ "heat07.u0910",				0x000200, 0x265eccc8, 0 | BRF_GRA },           // 14 Priority PROM?
};

STD_ROM_PICK(heatbrl)
STD_ROM_FN(heatbrl)

struct BurnDriver BurnDrvHeatbrl = {
	"heatbrl", NULL, NULL, NULL, "1992",
	"Heated Barrel (World version 3)\0", NULL, "TAD Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_MISC_POST90S, GBF_RUNGUN, 0,
	NULL, heatbrlRomInfo, heatbrlRomName, NULL, NULL, NULL, NULL, HeatbrlInputInfo, HeatbrlDIPInfo,
	HeatbrlInit, DrvExit, DrvFrame, HeatbrlDraw, DrvScan, &DrvRecalc, 0x801,
	256, 256, 4, 3
};


// Heated Barrel (World version 2)

static struct BurnRomInfo heatbrl2RomDesc[] = {
	{ "1e_ver2.9k",					0x020000, 0xb30bd632, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "2e_ver2.9m",					0x020000, 0xf3a23056, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3e_ver2.9f",					0x020000, 0xa2c41715, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4e_ver2.9h",					0x020000, 0xa50f4f08, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "barrel_7.u1110",				0x010000, 0x0784dbd8, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "barrel_6.u077",				0x010000, 0xbea3c581, 3 | BRF_GRA },           //  5 Characters
	{ "barrel_5.u072",				0x010000, 0x5604d155, 3 | BRF_GRA },           //  6

	{ "heated-barrel_obj1.u085",	0x100000, 0xf7a7c31c, 4 | BRF_GRA },           //  7 Sprites
	{ "heated-barrel_obj2.u0814",	0x100000, 0x24236116, 4 | BRF_GRA },           //  8

	{ "heated-barrel_bg-1.u075",	0x100000, 0x2f5d8baa, 5 | BRF_GRA },           //  9 Background Tiles

	{ "heated-barrel_bg-2.u074",	0x080000, 0x77ee4c6f, 6 | BRF_GRA },           // 10 Midground Tiles

	{ "heated-barrel_bg-3.u076",	0x080000, 0x83850e2d, 7 | BRF_GRA },           // 11 Foreground Tiles

	{ "barrel_8.u106",				0x020000, 0x489e5b1d, 8 | BRF_SND },           // 12 Samples

	{ "copx-d2.u0339",				0x080000, 0x7c52581b, 9 | BRF_PRG | BRF_ESS }, // 13 COP ROM

	{ "heat07.u0910",				0x000200, 0x265eccc8, 0 | BRF_GRA },           // 14 Priority PROM?
};

STD_ROM_PICK(heatbrl2)
STD_ROM_FN(heatbrl2)

struct BurnDriver BurnDrvHeatbrl2 = {
	"heatbrl2", "heatbrl", NULL, NULL, "1992",
	"Heated Barrel (World version 2)\0", NULL, "TAD Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_RUNGUN, 0,
	NULL, heatbrl2RomInfo, heatbrl2RomName, NULL, NULL, NULL, NULL, HeatbrlInputInfo, HeatbrlDIPInfo,
	HeatbrlInit, DrvExit, DrvFrame, HeatbrlDraw, DrvScan, &DrvRecalc, 0x801,
	256, 256, 4, 3
};


// Heated Barrel (World version ?)

static struct BurnRomInfo heatbrl3RomDesc[] = {
	{ "barrel_1.u025.9k",			0x020000, 0xb360c9cd, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "barrel_2.u024.9m",			0x020000, 0x06730aac, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "barrel_3.u026.9f",			0x020000, 0x63fff651, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "barrel_4.u023.9h",			0x020000, 0x7a119fd5, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "u1110.a6",					0x010000, 0x790bdba4, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "barrel_6.u077.5t",			0x010000, 0xbea3c581, 3 | BRF_GRA },           //  5 Characters
	{ "barrel_5.u072.5v",			0x010000, 0x5604d155, 3 | BRF_GRA },           //  6

	{ "heated-barrel_obj1.u085",	0x100000, 0xf7a7c31c, 4 | BRF_GRA },           //  7 Sprites
	{ "heated-barrel_obj2.u0814",	0x100000, 0x24236116, 4 | BRF_GRA },           //  8

	{ "heated-barrel_bg-1.u075",	0x100000, 0x2f5d8baa, 5 | BRF_GRA },           //  9 Background Tiles

	{ "heated-barrel_bg-2.u074",	0x080000, 0x77ee4c6f, 6 | BRF_GRA },           // 10 Midground Tiles

	{ "heated-barrel_bg-3.u076",	0x080000, 0x83850e2d, 7 | BRF_GRA },           // 11 Foreground Tiles

	{ "barrel_8.u106",				0x020000, 0x489e5b1d, 8 | BRF_SND },           // 12 Samples

	{ "copx-d2.u0339",				0x080000, 0x7c52581b, 9 | BRF_PRG | BRF_ESS }, // 13 COP ROM

	{ "heat07.u0910",				0x000200, 0x265eccc8, 0 | BRF_GRA },           // 14 Priority PROM?
};

STD_ROM_PICK(heatbrl3)
STD_ROM_FN(heatbrl3)

struct BurnDriver BurnDrvHeatbrl3 = {
	"heatbrl3", "heatbrl", NULL, NULL, "1992",
	"Heated Barrel (World version ?)\0", NULL, "TAD Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_RUNGUN, 0,
	NULL, heatbrl3RomInfo, heatbrl3RomName, NULL, NULL, NULL, NULL, HeatbrlInputInfo, HeatbrlDIPInfo,
	HeatbrlInit, DrvExit, DrvFrame, HeatbrlDraw, DrvScan, &DrvRecalc, 0x801,
	256, 256, 4, 3
};


// Heated Barrel (World old version)

static struct BurnRomInfo heatbrloRomDesc[] = {
	{ "barrel.1h",					0x020000, 0xd5a85c36, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "barrel.2h",					0x020000, 0x5104d463, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "barrel.3h",					0x020000, 0x823373a0, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "barrel.4h",					0x020000, 0x19a8606b, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "barrel_7.u1110",				0x010000, 0x0784dbd8, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "barrel_6.u077",				0x010000, 0xbea3c581, 3 | BRF_GRA },           //  5 Characters
	{ "barrel_5.u072",				0x010000, 0x5604d155, 3 | BRF_GRA },           //  6

	{ "heated-barrel_obj1.u085",	0x100000, 0xf7a7c31c, 4 | BRF_GRA },           //  7 Sprites
	{ "heated-barrel_obj2.u0814",	0x100000, 0x24236116, 4 | BRF_GRA },           //  8

	{ "heated-barrel_bg-1.u075",	0x100000, 0x2f5d8baa, 5 | BRF_GRA },           //  9 Background Tiles

	{ "heated-barrel_bg-2.u074",	0x080000, 0x77ee4c6f, 6 | BRF_GRA },           // 10 Midground Tiles

	{ "heated-barrel_bg-3.u076",	0x080000, 0x83850e2d, 7 | BRF_GRA },           // 11 Foreground Tiles

	{ "barrel_8.u106",				0x020000, 0x489e5b1d, 8 | BRF_SND },           // 12 Samples

	{ "copx-d2.u0339",				0x080000, 0x7c52581b, 9 | BRF_PRG | BRF_ESS }, // 13 COP ROM

	{ "heat07.u0910",				0x000200, 0x265eccc8, 0 | BRF_GRA },           // 14 Priority PROM?
};

STD_ROM_PICK(heatbrlo)
STD_ROM_FN(heatbrlo)

struct BurnDriver BurnDrvHeatbrlo = {
	"heatbrlo", "heatbrl", NULL, NULL, "1992",
	"Heated Barrel (World old version)\0", NULL, "TAD Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_RUNGUN, 0,
	NULL, heatbrloRomInfo, heatbrloRomName, NULL, NULL, NULL, NULL, HeatbrlInputInfo, HeatbrlDIPInfo,
	HeatbrlInit, DrvExit, DrvFrame, HeatbrlDraw, DrvScan, &DrvRecalc, 0x801,
	256, 256, 4, 3
};


// Heated Barrel (US)

static struct BurnRomInfo heatbrluRomDesc[] = {
	{ "1e_ver2.9k",					0x020000, 0xb30bd632, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "2u",							0x020000, 0x289dd629, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3e_ver2.9f",					0x020000, 0xa2c41715, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4e_ver2.9h",					0x020000, 0xa50f4f08, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "barrel_7.u1110",				0x010000, 0x0784dbd8, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "barrel_6.u077",				0x010000, 0xbea3c581, 3 | BRF_GRA },           //  5 Characters
	{ "barrel_5.u072",				0x010000, 0x5604d155, 3 | BRF_GRA },           //  6

	{ "heated-barrel_obj1.u085",	0x100000, 0xf7a7c31c, 4 | BRF_GRA },           //  7 Sprites
	{ "heated-barrel_obj2.u0814",	0x100000, 0x24236116, 4 | BRF_GRA },           //  8

	{ "heated-barrel_bg-1.u075",	0x100000, 0x2f5d8baa, 5 | BRF_GRA },           //  9 Background Tiles

	{ "heated-barrel_bg-2.u074",	0x080000, 0x77ee4c6f, 6 | BRF_GRA },           // 10 Midground Tiles

	{ "heated-barrel_bg-3.u076",	0x080000, 0x83850e2d, 7 | BRF_GRA },           // 11 Foreground Tiles

	{ "barrel_8.u106",				0x020000, 0x489e5b1d, 8 | BRF_SND },           // 12 Samples

	{ "copx-d2.u0339",				0x080000, 0x7c52581b, 9 | BRF_PRG | BRF_ESS }, // 13 COP ROM

	{ "heat07.u0910",				0x000200, 0x265eccc8, 0 | BRF_GRA },           // 14 Priority PROM?
};

STD_ROM_PICK(heatbrlu)
STD_ROM_FN(heatbrlu)

struct BurnDriver BurnDrvHeatbrlu = {
	"heatbrlu", "heatbrl", NULL, NULL, "1992",
	"Heated Barrel (US)\0", NULL, "TAD Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_RUNGUN, 0,
	NULL, heatbrluRomInfo, heatbrluRomName, NULL, NULL, NULL, NULL, HeatbrlInputInfo, HeatbrlDIPInfo,
	HeatbrlInit, DrvExit, DrvFrame, HeatbrlDraw, DrvScan, &DrvRecalc, 0x801,
	256, 256, 4, 3
};


// Heated Barrel (Electronic Devices license)

static struct BurnRomInfo heatbrleRomDesc[] = {
	{ "2.u025",						0x040000, 0xb34dc60c, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "1.u024",						0x040000, 0x16a3754f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "4.u026",						0x040000, 0xfae85c88, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "3.u023",						0x040000, 0x3b035081, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "barrel_7.u1110",				0x010000, 0x0784dbd8, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "barrel_6.u077",				0x010000, 0xbea3c581, 3 | BRF_GRA },           //  5 Characters
	{ "barrel_5.u072",				0x010000, 0x5604d155, 3 | BRF_GRA },           //  6

	{ "heated-barrel_obj1.u085",	0x100000, 0xf7a7c31c, 4 | BRF_GRA },           //  7 Sprites
	{ "heated-barrel_obj2.u0814",	0x100000, 0x24236116, 4 | BRF_GRA },           //  8

	{ "heated-barrel_bg-1.u075",	0x100000, 0x2f5d8baa, 5 | BRF_GRA },           //  9 Background Tiles

	{ "heated-barrel_bg-2.u074",	0x080000, 0x77ee4c6f, 6 | BRF_GRA },           // 10 Midground Tiles

	{ "heated-barrel_bg-3.u076",	0x080000, 0x83850e2d, 7 | BRF_GRA },           // 11 Foreground Tiles

	{ "barrel_8.u106",				0x020000, 0x489e5b1d, 8 | BRF_SND },           // 12 Samples

	{ "copx-d2.u0339",				0x080000, 0x7c52581b, 9 | BRF_PRG | BRF_ESS }, // 13 COP ROM

	{ "heat07.u0910",				0x000200, 0x265eccc8, 0 | BRF_GRA },           // 14 Priority PROM?
};

STD_ROM_PICK(heatbrle)
STD_ROM_FN(heatbrle)

struct BurnDriver BurnDrvHeatbrle = {
	"heatbrle", "heatbrl", NULL, NULL, "1992",
	"Heated Barrel (Electronic Devices license)\0", NULL, "TAD Corporation (Electronic Devices license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_RUNGUN, 0,
	NULL, heatbrleRomInfo, heatbrleRomName, NULL, NULL, NULL, NULL, HeatbrlInputInfo, HeatbrlDIPInfo,
	HeatbrlInit, DrvExit, DrvFrame, HeatbrlDraw, DrvScan, &DrvRecalc, 0x801,
	256, 256, 4, 3
};


// Godzilla (Japan)

static struct BurnRomInfo godzillaRomDesc[] = {
	{ "2.025",						0x020000, 0xbe9c6e5a, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "1.024",						0x020000, 0x0d6b663d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "4.026",						0x020000, 0xbb8c0132, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "3.023",						0x020000, 0xbb16e5d0, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "8.016",						0x010000, 0x4ab76e43, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "11.620",						0x010000, 0x58e0e41f, 3 | BRF_GRA },           //  5 Characters
	{ "10.615",						0x010000, 0x9c22bc13, 3 | BRF_GRA },           //  6

	{ "obj1.748",					0x200000, 0x0dfaf26d, 4 | BRF_GRA },           //  7 Sprites
	{ "obj2.756",					0x200000, 0x32b1516a, 4 | BRF_GRA },           //  8
	{ "obj3.743",					0x100000, 0x5af0114e, 4 | BRF_GRA },           //  9
	{ "obj4.757",					0x100000, 0x7448b054, 4 | BRF_GRA },           // 10

	{ "bg1.618",					0x100000, 0x78fbbb84, 5 | BRF_GRA },           // 11 Background / Midground Tiles

	{ "bg2.619",					0x100000, 0x8ac192a5, 6 | BRF_GRA },           // 12 Foreground Tiles

	{ "pcm.922",					0x080000, 0x59cbef10, 7 | BRF_SND },           // 13 Samples

	{ "copx-d2.313",				0x080000, 0x7c52581b, 8 | BRF_PRG | BRF_ESS }, // 15 COP ROM

	{ "s68e08.844",					0x000200, 0x96f7646e, 9 | BRF_GRA },           // 14 Priority PROM?
};

STD_ROM_PICK(godzilla)
STD_ROM_FN(godzilla)

struct BurnDriver BurnDrvGodzilla = {
	"godzilla", NULL, NULL, NULL, "1993",
	"Godzilla (Japan)\0", NULL, "Banpresto", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, godzillaRomInfo, godzillaRomName, NULL, NULL, NULL, NULL, GodzillaInputInfo, GodzillaDIPInfo,
	GodzillaInit, DrvExit, DrvYM2151Frame, GodzillaDraw, DrvScan, &DrvRecalc, 0x801,
	320, 224, 4, 3
};


// Denjin Makai (set 1)

static struct BurnRomInfo denjinmkRomDesc[] = {
	{ "denjin_1.u025",				0x040000, 0xb23a6e6f, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "denjin_2.u024",				0x040000, 0x4fde59e7, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "denjin_3.u026",				0x040000, 0x4f10292b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "denjin_4.u023",				0x040000, 0x209f1f6b, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "denjin_7.u1016",				0x010000, 0x970f36dd, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "denjin_6.u0620",				0x010000, 0xe1f759b1, 3 | BRF_GRA },           //  5 Characters
	{ "denjin_5.u0615",				0x010000, 0xcc36af0d, 3 | BRF_GRA },           //  6

	{ "obj-0-3.748",				0x200000, 0x67c26a67, 4 | BRF_GRA },           //  7 Sprites
	{ "obj-4-5.756",				0x100000, 0x01f8d4e6, 4 | BRF_GRA },           //  8
	{ "obj-6-7.743",				0x100000, 0xe5805757, 4 | BRF_GRA },           //  9
	{ "obj-8-9.757",				0x100000, 0xc8f7e1c9, 4 | BRF_GRA },           // 10

	{ "bg-1-ab.618",				0x100000, 0xeaad151a, 5 | BRF_GRA },           // 11 Background Tiles

	{ "bg-2-ab.617",				0x100000, 0x40938f74, 6 | BRF_GRA },           // 12 Midground Tiles

	{ "bg-3-ab.619",				0x100000, 0xde7366ee, 7 | BRF_GRA },           // 13 Foreground Tiles

	{ "denjin_8.u0922",				0x040000, 0xa11adb8f, 8 | BRF_SND },           // 14 Samples

	{ "copx-d2.313",				0x080000, 0x7c52581b, 9 | BRF_PRG | BRF_ESS }, // 15 COP ROM

	{ "s68e08.844",					0x000200, 0x96f7646e, 0 | BRF_GRA },           // 16 Priority PROM?
};

STD_ROM_PICK(denjinmk)
STD_ROM_FN(denjinmk)

struct BurnDriver BurnDrvDenjinmk = {
	"denjinmk", NULL, NULL, NULL, "1994",
	"Denjin Makai (set 1)\0", NULL, "Winkysoft (Banpresto license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, denjinmkRomInfo, denjinmkRomName, NULL, NULL, NULL, NULL, DenjinmkInputInfo, DenjinmkDIPInfo,
	DenjinmkInit, DrvExit, DrvYM2151Frame, DenjinmkDraw, DrvScan, &DrvRecalc, 0x801,
	320, 256, 4, 3
};


// Denjin Makai (set 2)

static struct BurnRomInfo denjinmkaRomDesc[] = {
	{ "rom1.025",					0x040000, 0x44a648e8, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "rom2.024",					0x040000, 0xe5ee8fe0, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rom3.026",					0x040000, 0x781b942e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rom4.023",					0x040000, 0x502a588b, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "rom5.016",					0x010000, 0x7fe7e352, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "rom7.620",					0x010000, 0xe1f759b1, 3 | BRF_GRA },           //  5 Characters
	{ "rom8.615",					0x010000, 0xcc36af0d, 3 | BRF_GRA },           //  6

	{ "obj-0-3.748",				0x200000, 0x67c26a67, 4 | BRF_GRA },           //  7 Sprites
	{ "obj-4-5.756",				0x100000, 0x01f8d4e6, 4 | BRF_GRA },           //  8
	{ "obj-6-7.743",				0x100000, 0xe5805757, 4 | BRF_GRA },           //  9
	{ "obj-8-9.757",				0x100000, 0xc8f7e1c9, 4 | BRF_GRA },           // 10

	{ "bg-1-ab.618",				0x100000, 0xeaad151a, 5 | BRF_GRA },           // 11 Background Tiles

	{ "bg-2-ab.617",				0x100000, 0x40938f74, 6 | BRF_GRA },           // 12 Midground Tiles

	{ "bg-3-ab.619",				0x100000, 0xde7366ee, 7 | BRF_GRA },           // 13 Foreground Tiles

	{ "rom6.922",					0x040000, 0x09e13213, 8 | BRF_SND },           // 14 Samples

	{ "copx-d2.313",				0x080000, 0x7c52581b, 9 | BRF_PRG | BRF_ESS }, // 15 COP ROM

	{ "s68e08.844",					0x000200, 0x96f7646e, 0 | BRF_GRA },           // 16 Priority PROM?
};

STD_ROM_PICK(denjinmka)
STD_ROM_FN(denjinmka)

struct BurnDriver BurnDrvDenjinmka = {
	"denjinmka", "denjinmk", NULL, NULL, "1994",
	"Denjin Makai (set 2)\0", NULL, "Winkysoft (Banpresto license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, denjinmkaRomInfo, denjinmkaRomName, NULL, NULL, NULL, NULL, DenjinmkInputInfo, DenjinmkDIPInfo,
	DenjinmkInit, DrvExit, DrvYM2151Frame, DenjinmkDraw, DrvScan, &DrvRecalc, 0x801,
	320, 256, 4, 3
};


// SD Gundam Sangokushi Rainbow Tairiku Senki (Japan)

static struct BurnRomInfo grainbowRomDesc[] = {
	{ "rb-p1.25",					0x040000, 0x0995c511, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "rb-p2.24",					0x040000, 0xc9eb756f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rb-p3.26",					0x040000, 0xfe2f08a8, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rb-p4.23",					0x040000, 0xf558962a, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "rb-s.016",					0x010000, 0x8439bf5b, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "rb-f1.620",					0x010000, 0x792c403d, 3 | BRF_GRA },           //  5 Characters
	{ "rb-f2.615",					0x010000, 0xa30e0903, 3 | BRF_GRA },           //  6

	{ "rb-spr01.748",				0x100000, 0x11a3479d, 4 | BRF_GRA },           //  7 Sprites
	{ "rb-spr23.756",				0x100000, 0xfd08a761, 4 | BRF_GRA },           //  8

	{ "rb-bg-01.618",				0x100000, 0x6a4ca7e7, 5 | BRF_GRA },           //  9 Background / Midground Tiles

	{ "rb-bg-2.619",				0x100000, 0xa9b5c85e, 6 | BRF_GRA },           // 10 Foreground Tiles

	{ "rb-ad.922",					0x020000, 0xa364cb42, 7 | BRF_SND },           // 11 Samples

	{ "copx-d2.313",				0x040000, 0xa6732ff9, 9 | BRF_PRG | BRF_ESS }, // 12 COP ROM
};

STD_ROM_PICK(grainbow)
STD_ROM_FN(grainbow)

struct BurnDriver BurnDrvGrainbow = {
	"grainbow", NULL, NULL, NULL, "1993",
	"SD Gundam Sangokushi Rainbow Tairiku Senki (Japan)\0", NULL, "Banpresto", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_RUNGUN, 0,
	NULL, grainbowRomInfo, grainbowRomName, NULL, NULL, NULL, NULL, GrainbowInputInfo, GrainbowDIPInfo,
	GrainbowInit, DrvExit, DrvYM2151Frame, GrainbowDraw, DrvScan, &DrvRecalc, 0x801,
	320, 224, 4, 3
};


// SD Gundam Sangokushi Rainbow Tairiku Senki (Korea)

static struct BurnRomInfo grainbowkRomDesc[] = {
	{ "rom1.u025",					0x040000, 0x686c25ea, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "rom2.u024",					0x040000, 0x2400662e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rom3.u026",					0x040000, 0x22857489, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rom4.u023",					0x040000, 0xad8de15b, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "rb-s.016",					0x010000, 0x8439bf5b, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "rb-f1.620",					0x010000, 0x792c403d, 3 | BRF_GRA },           //  5 Characters
	{ "rb-f2.615",					0x010000, 0xa30e0903, 3 | BRF_GRA },           //  6

	{ "rb-spr01.748",				0x100000, 0x11a3479d, 4 | BRF_GRA },           //  7 Sprites
	{ "rb-spr23.756",				0x100000, 0xfd08a761, 4 | BRF_GRA },           //  8

	{ "rb-bg-01.618",				0x100000, 0x6a4ca7e7, 5 | BRF_GRA },           //  9 Background / Midground Tiles

	{ "rb-bg-2.619",				0x100000, 0xa9b5c85e, 6 | BRF_GRA },           // 10 Foreground Tiles

	{ "rb-ad.922",					0x020000, 0xa364cb42, 7 | BRF_SND },           // 11 Samples

	{ "copx-d2.313",				0x040000, 0xa6732ff9, 8 | BRF_GRA },           // 12 COP ROM
	};

STD_ROM_PICK(grainbowk)
STD_ROM_FN(grainbowk)

struct BurnDriver BurnDrvGrainbowk = {
	"grainbowk", "grainbow", NULL, NULL, "1993",
	"SD Gundam Sangokushi Rainbow Tairiku Senki (Korea)\0", NULL, "Banpresto", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_RUNGUN, 0,
	NULL, grainbowkRomInfo, grainbowkRomName, NULL, NULL, NULL, NULL, GrainbowInputInfo, GrainbowDIPInfo,
	GrainbowInit, DrvExit, DrvYM2151Frame, GrainbowDraw, DrvScan, &DrvRecalc, 0x801,
	320, 224, 4, 3
};
