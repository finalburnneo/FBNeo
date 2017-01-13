/*
   Games supported:
        * Zaxxon		    yes
        * Super Zaxxon		yes
        * Future Spy		yes
        * Razmatazz		    no sound+bad controls sega_universal_sound_board_rom ?
        * Ixion			    no
        * Congo Bongo		yes

   To do:
	Need to add Sega USB support.
	Analog inputs for Raxmatazz and Ixion
	Clean up video code
*/
#include "tiles_generic.h"
#include "z80_intf.h"
#include "8255ppi.h"
#include "sn76496.h"
#include "samples.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvZ80DecROM;
static UINT8 *DrvZ80ROM2;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvColPROM;
static UINT8 *DrvCharColPROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvZ80RAM2;
static UINT8 *DrvSprRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvColRAM;
static UINT32 *DrvPalette;

static UINT8 DrvRecalc;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[8];
static UINT8 DrvReset;

static UINT8 *interrupt_enable;

static UINT8 *zaxxon_fg_color;
static UINT8 *zaxxon_bg_color;
static UINT8 *zaxxon_bg_enable;
static UINT32 *zaxxon_bg_scroll;
static UINT8 *zaxxon_flipscreen;
static UINT8 *zaxxon_coin_enable;
static UINT8 *zaxxon_coin_status;
static UINT8 *zaxxon_coin_last;

static UINT8 *congo_color_bank;
static UINT8 *congo_fg_bank;
static UINT8 *congo_custom;

static UINT8 *zaxxon_bg_pixmap;

static UINT8 *soundlatch;

static UINT8 *sound_state;

static INT32 futspy_sprite = 0;
static INT32 hardware_type = 0;
static UINT8 no_flip = 0;

static struct BurnInputInfo ZaxxonInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"}	,
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy4 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Zaxxon)

static struct BurnInputInfo CongoBongoInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy4 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(CongoBongo)

static struct BurnInputInfo FutspyInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy4 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Futspy)

static struct BurnDIPInfo CongoBongoDIPList[]=
{
	{0x10, 0xff, 0xff, 0x7f, NULL		},
	{0x11, 0xff, 0xff, 0x33, NULL		},

	{0   , 0xfe, 0   ,    1, "Service Mode (No Toggle)"		},
	{0x10, 0x01, 0x01, 0x00, "Off"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x10, 0x01, 0x03, 0x03, "10000"		},
	{0x10, 0x01, 0x03, 0x01, "20000"		},
	{0x10, 0x01, 0x03, 0x02, "30000"		},
	{0x10, 0x01, 0x03, 0x00, "40000"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x10, 0x01, 0x30, 0x30, "3"		},
	{0x10, 0x01, 0x30, 0x10, "4"		},
	{0x10, 0x01, 0x30, 0x20, "5"		},
	{0x10, 0x01, 0x30, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Sound"		},
	{0x10, 0x01, 0x40, 0x00, "Off"		},
	{0x10, 0x01, 0x40, 0x40, "On"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x10, 0x01, 0x80, 0x00, "Upright"		},
	{0x10, 0x01, 0x80, 0x80, "Cocktail"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x10, 0x01, 0x0c, 0x0c, "Easy"		},
	{0x10, 0x01, 0x0c, 0x04, "Medium"		},
	{0x10, 0x01, 0x0c, 0x08, "Hard"		},
	{0x10, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    16, "Coin B"		},
	{0x11, 0x01, 0x0f, 0x0f, "4 Coins / 1 Credit"		},
	{0x11, 0x01, 0x0f, 0x07, "3 Coins / 1 Credit"		},
	{0x11, 0x01, 0x0f, 0x0b, "2 Coins / 1 Credit"		},
	{0x11, 0x01, 0x0f, 0x06, "2 Coins / 1 Credit + Bonus each 5"		},
	{0x11, 0x01, 0x0f, 0x0a, "2 Coins / 1 Credit + Bonus each 2"		},
	{0x11, 0x01, 0x0f, 0x03, "1 Coin / 1 Credit"		},
	{0x11, 0x01, 0x0f, 0x02, "1 Coin / 1 Credit + Bonus each 5"		},
	{0x11, 0x01, 0x0f, 0x0c, "1 Coin / 1 Credit + Bonus each 4"		},
	{0x11, 0x01, 0x0f, 0x04, "1 Coin / 1 Credit + Bonus each 2"		},
	{0x11, 0x01, 0x0f, 0x0d, "1 Coin / 2 Credits"		},
	{0x11, 0x01, 0x0f, 0x08, "1 Coin / 2 Credits + Bonus each 5"		},
	{0x11, 0x01, 0x0f, 0x00, "1 Coin / 2 Credits + Bonus each 4"		},
	{0x11, 0x01, 0x0f, 0x05, "1 Coin / 3 Credits"		},
	{0x11, 0x01, 0x0f, 0x09, "1 Coin / 4 Credits"		},
	{0x11, 0x01, 0x0f, 0x01, "1 Coin / 5 Credits"		},
	{0x11, 0x01, 0x0f, 0x0e, "1 Coin / 6 Credits"		},

	{0   , 0xfe, 0   ,    16, "Coin A"		},
	{0x11, 0x01, 0xf0, 0xf0, "4 Coins / 1 Credit"		},
	{0x11, 0x01, 0xf0, 0x70, "3 Coins / 1 Credit"		},
	{0x11, 0x01, 0xf0, 0xb0, "2 Coins / 1 Credit"		},
	{0x11, 0x01, 0xf0, 0x60, "2 Coins / 1 Credit + Bonus each 5"		},
	{0x11, 0x01, 0xf0, 0xa0, "2 Coins / 1 Credit + Bonus each 2"		},
	{0x11, 0x01, 0xf0, 0x30, "1 Coin / 1 Credit"		},
	{0x11, 0x01, 0xf0, 0x20, "1 Coin / 1 Credit + Bonus each 5"		},
	{0x11, 0x01, 0xf0, 0xc0, "1 Coin / 1 Credit + Bonus each 4"		},
	{0x11, 0x01, 0xf0, 0x40, "1 Coin / 1 Credit + Bonus each 2"		},
	{0x11, 0x01, 0xf0, 0xd0, "1 Coin / 2 Credits"		},
	{0x11, 0x01, 0xf0, 0x80, "1 Coin / 2 Credits + Bonus each 5"		},
	{0x11, 0x01, 0xf0, 0x00, "1 Coin / 2 Credits + Bonus each 4"		},
	{0x11, 0x01, 0xf0, 0x50, "1 Coin / 3 Credits"		},
	{0x11, 0x01, 0xf0, 0x90, "1 Coin / 4 Credits"		},
	{0x11, 0x01, 0xf0, 0x10, "1 Coin / 5 Credits"		},
	{0x11, 0x01, 0xf0, 0xe0, "1 Coin / 6 Credits"		},
};

STDDIPINFO(CongoBongo)

static struct BurnDIPInfo ZaxxonDIPList[]=
{
	{0x10, 0xff, 0xff, 0x7f, NULL		},
	{0x11, 0xff, 0xff, 0x33, NULL		},

	{0   , 0xfe, 0   ,    1, "Service Mode (No Toggle)"		},
	{0x10, 0x01, 0x01, 0x00, "Off"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x10, 0x01, 0x03, 0x03, "10000"		},
	{0x10, 0x01, 0x03, 0x01, "20000"		},
	{0x10, 0x01, 0x03, 0x02, "30000"		},
	{0x10, 0x01, 0x03, 0x00, "40000"		},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x10, 0x01, 0x04, 0x00, "Off"		},
	{0x10, 0x01, 0x04, 0x04, "On"		},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x10, 0x01, 0x08, 0x00, "Off"		},
	{0x10, 0x01, 0x08, 0x08, "On"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x10, 0x01, 0x30, 0x30, "3"		},
	{0x10, 0x01, 0x30, 0x10, "4"		},
	{0x10, 0x01, 0x30, 0x20, "5"		},
	{0x10, 0x01, 0x30, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Sound"		},
	{0x10, 0x01, 0x40, 0x00, "Off"		},
	{0x10, 0x01, 0x40, 0x40, "On"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x10, 0x01, 0x80, 0x00, "Upright"		},
	{0x10, 0x01, 0x80, 0x80, "Cocktail"		},

	{0   , 0xfe, 0   ,    16, "Coin B"		},
	{0x11, 0x01, 0x0f, 0x0f, "4 Coins / 1 Credit"		},
	{0x11, 0x01, 0x0f, 0x07, "3 Coins / 1 Credit"		},
	{0x11, 0x01, 0x0f, 0x0b, "2 Coins / 1 Credit"		},
	{0x11, 0x01, 0x0f, 0x06, "2 Coins / 1 Credit + Bonus each 5"		},
	{0x11, 0x01, 0x0f, 0x0a, "2 Coins / 1 Credit + Bonus each 2"		},
	{0x11, 0x01, 0x0f, 0x03, "1 Coin / 1 Credit"		},
	{0x11, 0x01, 0x0f, 0x02, "1 Coin / 1 Credit + Bonus each 5"		},
	{0x11, 0x01, 0x0f, 0x0c, "1 Coin / 1 Credit + Bonus each 4"		},
	{0x11, 0x01, 0x0f, 0x04, "1 Coin / 1 Credit + Bonus each 2"		},
	{0x11, 0x01, 0x0f, 0x0d, "1 Coin / 2 Credits"		},
	{0x11, 0x01, 0x0f, 0x08, "1 Coin / 2 Credits + Bonus each 5"		},
	{0x11, 0x01, 0x0f, 0x00, "1 Coin / 2 Credits + Bonus each 4"		},
	{0x11, 0x01, 0x0f, 0x05, "1 Coin / 3 Credits"		},
	{0x11, 0x01, 0x0f, 0x09, "1 Coin / 4 Credits"		},
	{0x11, 0x01, 0x0f, 0x01, "1 Coin / 5 Credits"		},
	{0x11, 0x01, 0x0f, 0x0e, "1 Coin / 6 Credits"		},

	{0   , 0xfe, 0   ,    16, "Coin A"		},
	{0x11, 0x01, 0xf0, 0xf0, "4 Coins / 1 Credit"		},
	{0x11, 0x01, 0xf0, 0x70, "3 Coins / 1 Credit"		},
	{0x11, 0x01, 0xf0, 0xb0, "2 Coins / 1 Credit"		},
	{0x11, 0x01, 0xf0, 0x60, "2 Coins / 1 Credit + Bonus each 5"		},
	{0x11, 0x01, 0xf0, 0xa0, "2 Coins / 1 Credit + Bonus each 2"		},
	{0x11, 0x01, 0xf0, 0x30, "1 Coin / 1 Credit"		},
	{0x11, 0x01, 0xf0, 0x20, "1 Coin / 1 Credit + Bonus each 5"		},
	{0x11, 0x01, 0xf0, 0xc0, "1 Coin / 1 Credit + Bonus each 4"		},
	{0x11, 0x01, 0xf0, 0x40, "1 Coin / 1 Credit + Bonus each 2"		},
	{0x11, 0x01, 0xf0, 0xd0, "1 Coin / 2 Credits"		},
	{0x11, 0x01, 0xf0, 0x80, "1 Coin / 2 Credits + Bonus each 5"		},
	{0x11, 0x01, 0xf0, 0x00, "1 Coin / 2 Credits + Bonus each 4"		},
	{0x11, 0x01, 0xf0, 0x50, "1 Coin / 3 Credits"		},
	{0x11, 0x01, 0xf0, 0x90, "1 Coin / 4 Credits"		},
	{0x11, 0x01, 0xf0, 0x10, "1 Coin / 5 Credits"		},
	{0x11, 0x01, 0xf0, 0xe0, "1 Coin / 6 Credits"		},
};

STDDIPINFO(Zaxxon)

static struct BurnDIPInfo SzaxxonDIPList[]=
{
	{0x10, 0xff, 0xff, 0x7f, NULL		},
	{0x11, 0xff, 0xff, 0x33, NULL		},

	{0   , 0xfe, 0   ,    1, "Service Mode (No Toggle)"		},
	{0x10, 0x01, 0x01, 0x00, "Off"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x10, 0x01, 0x03, 0x03, "10000"		},
	{0x10, 0x01, 0x03, 0x01, "20000"		},
	{0x10, 0x01, 0x03, 0x02, "30000"		},
	{0x10, 0x01, 0x03, 0x00, "40000"		},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x10, 0x01, 0x04, 0x00, "Off"		},
	{0x10, 0x01, 0x04, 0x04, "On"		},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x10, 0x01, 0x08, 0x00, "Off"		},
	{0x10, 0x01, 0x08, 0x08, "On"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x10, 0x01, 0x30, 0x30, "3"		},
	{0x10, 0x01, 0x30, 0x10, "4"		},
	{0x10, 0x01, 0x30, 0x20, "5"		},
	{0x10, 0x01, 0x30, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Sound"		},
	{0x10, 0x01, 0x40, 0x00, "Off"		},
	{0x10, 0x01, 0x40, 0x40, "On"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x10, 0x01, 0x80, 0x00, "Upright"		},
	{0x10, 0x01, 0x80, 0x80, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x10, 0x01, 0x04, 0x04, "Normal"		},
	{0x10, 0x01, 0x04, 0x00, "Hard"		},

	{0   , 0xfe, 0   ,    16, "Coin B"		},
	{0x11, 0x01, 0x0f, 0x0f, "4 Coins / 1 Credit"		},
	{0x11, 0x01, 0x0f, 0x07, "3 Coins / 1 Credit"		},
	{0x11, 0x01, 0x0f, 0x0b, "2 Coins / 1 Credit"		},
	{0x11, 0x01, 0x0f, 0x06, "2 Coins / 1 Credit + Bonus each 5"		},
	{0x11, 0x01, 0x0f, 0x0a, "2 Coins / 1 Credit + Bonus each 2"		},
	{0x11, 0x01, 0x0f, 0x03, "1 Coin / 1 Credit"		},
	{0x11, 0x01, 0x0f, 0x02, "1 Coin / 1 Credit + Bonus each 5"		},
	{0x11, 0x01, 0x0f, 0x0c, "1 Coin / 1 Credit + Bonus each 4"		},
	{0x11, 0x01, 0x0f, 0x04, "1 Coin / 1 Credit + Bonus each 2"		},
	{0x11, 0x01, 0x0f, 0x0d, "1 Coin / 2 Credits"		},
	{0x11, 0x01, 0x0f, 0x08, "1 Coin / 2 Credits + Bonus each 5"		},
	{0x11, 0x01, 0x0f, 0x00, "1 Coin / 2 Credits + Bonus each 4"		},
	{0x11, 0x01, 0x0f, 0x05, "1 Coin / 3 Credits"		},
	{0x11, 0x01, 0x0f, 0x09, "1 Coin / 4 Credits"		},
	{0x11, 0x01, 0x0f, 0x01, "1 Coin / 5 Credits"		},
	{0x11, 0x01, 0x0f, 0x0e, "1 Coin / 6 Credits"		},

	{0   , 0xfe, 0   ,    16, "Coin A"		},
	{0x11, 0x01, 0xf0, 0xf0, "4 Coins / 1 Credit"		},
	{0x11, 0x01, 0xf0, 0x70, "3 Coins / 1 Credit"		},
	{0x11, 0x01, 0xf0, 0xb0, "2 Coins / 1 Credit"		},
	{0x11, 0x01, 0xf0, 0x60, "2 Coins / 1 Credit + Bonus each 5"		},
	{0x11, 0x01, 0xf0, 0xa0, "2 Coins / 1 Credit + Bonus each 2"		},
	{0x11, 0x01, 0xf0, 0x30, "1 Coin / 1 Credit"		},
	{0x11, 0x01, 0xf0, 0x20, "1 Coin / 1 Credit + Bonus each 5"		},
	{0x11, 0x01, 0xf0, 0xc0, "1 Coin / 1 Credit + Bonus each 4"		},
	{0x11, 0x01, 0xf0, 0x40, "1 Coin / 1 Credit + Bonus each 2"		},
	{0x11, 0x01, 0xf0, 0xd0, "1 Coin / 2 Credits"		},
	{0x11, 0x01, 0xf0, 0x80, "1 Coin / 2 Credits + Bonus each 5"		},
	{0x11, 0x01, 0xf0, 0x00, "1 Coin / 2 Credits + Bonus each 4"		},
	{0x11, 0x01, 0xf0, 0x50, "1 Coin / 3 Credits"		},
	{0x11, 0x01, 0xf0, 0x90, "1 Coin / 4 Credits"		},
	{0x11, 0x01, 0xf0, 0x10, "1 Coin / 5 Credits"		},
	{0x11, 0x01, 0xf0, 0xe0, "1 Coin / 6 Credits"		},
};

STDDIPINFO(Szaxxon)

static struct BurnDIPInfo FutspyDIPList[]=
{
	{0x12, 0xff, 0xff, 0x00, NULL		},
	{0x13, 0xff, 0xff, 0x43, NULL		},

	{0   , 0xfe, 0   ,    2, "Coin B"		},
	{0x12, 0x01, 0x0f, 0x00, "1 Coin 1 Credit"		},
	{0x12, 0x01, 0x0f, 0x01, "1 Coin 2 Credits"		},

	{0   , 0xfe, 0   ,    2, "Coin A"		},
	{0x12, 0x01, 0xf0, 0x00, "1 Coin 1 Credit"		},
	{0x12, 0x01, 0xf0, 0x10, "1 Coin 2 Credits"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x13, 0x01, 0x01, 0x01, "Upright"		},
	{0x13, 0x01, 0x01, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x02, 0x00, "Off"		},
	{0x13, 0x01, 0x02, 0x02, "On"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0x0c, 0x00, "3"		},
	{0x13, 0x01, 0x0c, 0x04, "4"		},
	{0x13, 0x01, 0x0c, 0x08, "5"		},
	{0x13, 0x01, 0x0c, 0x0c, "Free Play"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x13, 0x01, 0x30, 0x00, "20K 40K 60K"		},
	{0x13, 0x01, 0x30, 0x10, "30K 60K 90K"		},
	{0x13, 0x01, 0x30, 0x20, "40K 70K 100K"		},
	{0x13, 0x01, 0x30, 0x30, "40K 80K 120K"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0xc0, 0x00, "Easy"		},
	{0x13, 0x01, 0xc0, 0x40, "Medium"		},
	{0x13, 0x01, 0xc0, 0x80, "Hard"		},
	{0x13, 0x01, 0xc0, 0xc0, "Hardest"		},
};

STDDIPINFO(Futspy)

static void ZaxxonPPIWriteA(UINT8 data)
{
	UINT8 diff = data ^ sound_state[0];
	sound_state[0] = data;

	BurnSampleSetRoute(10, BURN_SND_SAMPLE_ROUTE_1, 0.01 + 0.01 * (data & 0x03), BURN_SND_ROUTE_BOTH);
	BurnSampleSetRoute(10, BURN_SND_SAMPLE_ROUTE_2, 0.01 + 0.01 * (data & 0x03), BURN_SND_ROUTE_BOTH);
	BurnSampleSetRoute(11, BURN_SND_SAMPLE_ROUTE_1, 0.01 + 0.01 * (data & 0x03), BURN_SND_ROUTE_BOTH);
	BurnSampleSetRoute(11, BURN_SND_SAMPLE_ROUTE_2, 0.01 + 0.01 * (data & 0x03), BURN_SND_ROUTE_BOTH);

	/* PLAYER SHIP C: channel 10 */
	if ((diff & 0x04) && !(data & 0x04)) { BurnSampleStop(11); BurnSamplePlay(10); }
	if ((diff & 0x04) &&  (data & 0x04)) BurnSampleStop(10);

	/* PLAYER SHIP D: channel 11 */
	if ((diff & 0x08) && !(data & 0x08)) { BurnSampleStop(10); BurnSamplePlay(11); }
	if ((diff & 0x08) &&  (data & 0x08)) BurnSampleStop(11);

	/* HOMING MISSILE: channel 0 */
	if ((diff & 0x10) && !(data & 0x10)) BurnSamplePlay(0);
	if ((diff & 0x10) &&  (data & 0x10)) BurnSampleStop(0);

	/* BASE MISSILE: channel 1 */
	if ((diff & 0x20) && !(data & 0x20)) BurnSamplePlay(1);

	/* LASER: channel 2 */
	if ((diff & 0x40) && !(data & 0x40)) BurnSamplePlay(2);
	if ((diff & 0x40) &&  (data & 0x40)) BurnSampleStop(2);

	/* BATTLESHIP: channel 3 */
	if ((diff & 0x80) && !(data & 0x80)) BurnSamplePlay(3);
	if ((diff & 0x80) &&  (data & 0x80)) BurnSampleStop(3);
}

static void ZaxxonPPIWriteB(UINT8 data)
{
	UINT8 diff = data ^ sound_state[1];
	sound_state[1] = data;

	/* S-EXP: channel 4 */
	if ((diff & 0x10) && !(data & 0x10)) BurnSamplePlay(4);

	/* M-EXP: channel 5 */
	if ((diff & 0x20) && !(data & 0x20) && !BurnSampleGetStatus(5)) BurnSamplePlay(5);

	/* CANNON: channel 6 */
	if ((diff & 0x80) && !(data & 0x80)) BurnSamplePlay(6);
}

static void ZaxxonPPIWriteC(UINT8 data)
{
	UINT8 diff = data ^ sound_state[2];
	sound_state[2] = data;

	/* SHOT: channel 7 */
	if ((diff & 0x01) && !(data & 0x01)) BurnSamplePlay(7);

	/* ALARM2: channel 8 */
	if ((diff & 0x04) && !(data & 0x04)) BurnSamplePlay(8);

	/* ALARM3: channel 9 */
	if ((diff & 0x08) && !(data & 0x08) && !BurnSampleGetStatus(9)) BurnSamplePlay(9);
}

static UINT8 CongoPPIReadA()
{
	return *soundlatch;
}

static void CongoPPIWriteB(UINT8 data)
{
	UINT8 diff = data ^ sound_state[1];
	sound_state[1] = data;

	if ((diff & 0x02) && !(data & 0x02) && !BurnSampleGetStatus(0)) BurnSamplePlay(0);
}

static void CongoPPIWriteC(UINT8 data)
{
	UINT8 diff = data ^ sound_state[2];
	sound_state[2] = data;

	/* BASS DRUM: channel 1 */
	if ((diff & 0x01) && !(data & 0x01)) BurnSamplePlay(1);
	if ((diff & 0x01) &&  (data & 0x01)) BurnSampleStop(1);

	/* CONGA (LOW): channel 2 */
	if ((diff & 0x02) && !(data & 0x02)) BurnSamplePlay(2);
	if ((diff & 0x02) &&  (data & 0x02)) BurnSampleStop(2);
	
	/* CONGA (HIGH): channel 3 */
	if ((diff & 0x04) && !(data & 0x04)) BurnSamplePlay(3);
	if ((diff & 0x04) &&  (data & 0x04)) BurnSampleStop(3);
	
	/* RIM: channel 4 */
	if ((diff & 0x08) && !(data & 0x08)) BurnSamplePlay(4);
	if ((diff & 0x08) &&  (data & 0x08)) BurnSampleStop(4);
}

static UINT8 __fastcall zaxxon_read(UINT16 address)
{
	// address mirroring
	if ((address & 0xe700) == 0xc000) address &= ~0x18f8;
	if ((address & 0xe700) == 0xc100) address &= ~0x18ff;
	if ((address & 0xe000) == 0xe000) address &= ~0x1f00;

	switch (address)
	{
		case 0xc000:
			return DrvInputs[0];

		case 0xc001:
			return DrvInputs[1];

		case 0xc002:
			return DrvDips[0];

		case 0xc003:
			return DrvDips[1];

		case 0xc100:
			return DrvInputs[2];

		case 0xe03c:
		case 0xe03d:
		case 0xe03e:
		case 0xe03f:
			return ppi8255_r(0, address & 0x03);
	}

	return 0;
}

static void zaxxon_coin_inserted(UINT8 param)
{
	if (zaxxon_coin_last[param] != DrvJoy4[param])
	{
		zaxxon_coin_status[param] = zaxxon_coin_enable[param];
	}
}

static UINT8 zaxxon_coin_r(UINT8 param)
{
	return zaxxon_coin_status[param];
}

static void zaxxon_coin_enable_w(UINT8 offset, UINT8 data)
{
	zaxxon_coin_enable[offset] = data;
	if (!zaxxon_coin_enable[offset])
		zaxxon_coin_status[offset] = 0;
}

static void __fastcall zaxxon_write(UINT16 address, UINT8 data)
{
	// address mirroring
	if ((address & 0xe700) == 0xc000) address &= ~0x18f8;
	if ((address & 0xe000) == 0xe000) address &= ~0x1f00;

	switch (address)
	{
		case 0xc000:
		case 0xc001:
		case 0xc002:
			zaxxon_coin_enable_w(address & 3, data & 1);
		return;

		case 0xc003:
		case 0xc004:
			// coin counter_w
		return;

		case 0xc006:
			*zaxxon_flipscreen = ~data & 1;
		return;

		case 0xe03c:
		case 0xe03d:
		case 0xe03e:
		case 0xe03f:
			ppi8255_w(0, address  & 0x03, data);
		return;

		case 0xe0f0:
			*interrupt_enable = data & 1;
			if (~data & 1) ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;

		case 0xe0f1:
			*zaxxon_fg_color = (data & 1) << 7;
		return;

		case 0xe0f8:
			*zaxxon_bg_scroll &= 0xf00;
			*zaxxon_bg_scroll |= data;
		return;

		case 0xe0f9:
			*zaxxon_bg_scroll &= 0x0ff;
			*zaxxon_bg_scroll |= (data & 0x07) << 8;
		return; 
			
		case 0xe0fa:
			*zaxxon_bg_color = data << 7;
		return;

		case 0xe0fb:
			*zaxxon_bg_enable = data & 1;
		return;
	}
}

static UINT8 __fastcall congo_read(UINT16 address)
{
	// address mirroring
	if ((address & 0xe008) == 0xc000) address &= ~0x1fc4;
	if ((address & 0xe008) == 0xc008) address &= ~0x1fc7;

	switch (address)
	{
		case 0xc000:
			return DrvInputs[0];        // in0
		case 0xc001:
			return DrvInputs[1];        // in1
		case 0xc002:
			return DrvDips[0]; // dsw02                    // dsw1
		case 0xc003:
			return DrvDips[1]; // dsw03                    // dsw2
		case 0xc008:
			return DrvInputs[2];
	}

	return 0;
}

static void congo_sprite_custom_w(INT32 offset, UINT8 data)
{
	congo_custom[offset] = data;

	/* seems to trigger on a write of 1 to the 4th byte */
	if (offset == 3 && data == 0x01)
	{
		UINT16 saddr = congo_custom[0] | (congo_custom[1] << 8);
		INT32 count = congo_custom[2];

		/* count cycles (just a guess) */
		ZetIdle(-count * 5);

		/* this is just a guess; the chip is hardwired to the spriteram */
		while (count-- >= 0)
		{
			UINT8 daddr = DrvZ80RAM[saddr & 0xfff] * 4;
			DrvSprRAM[(daddr + 0) & 0xff] = DrvZ80RAM[(saddr + 1) & 0xfff];
			DrvSprRAM[(daddr + 1) & 0xff] = DrvZ80RAM[(saddr + 2) & 0xfff];
			DrvSprRAM[(daddr + 2) & 0xff] = DrvZ80RAM[(saddr + 3) & 0xfff];
			DrvSprRAM[(daddr + 3) & 0xff] = DrvZ80RAM[(saddr + 4) & 0xfff];
			saddr += 0x20;
		}
	}
}

static void __fastcall congo_write(UINT16 address, UINT8 data)
{
	// address mirroring
	if ((address & 0xe000) == 0xc000) address &= ~0x1fc0;

	switch (address)
	{
		case 0xc018:
		case 0xc019:
		case 0xc01a:
			zaxxon_coin_enable_w(address & 3, data & 1);
		return;

		case 0xc01b:
		case 0xc01c:
			// zaxxon_coin_counter_w
		return;

		case 0xc01d:
			*zaxxon_bg_enable = data & 1;
		return;

		case 0xc01e:
			*zaxxon_flipscreen = ~data & 1;
		return;

		case 0xc01f:
			*interrupt_enable = data & 1;
			if (~data & 1) ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;

		case 0xc021:
			*zaxxon_fg_color = data << 7;
		return;

		case 0xc023:
			*zaxxon_bg_color = data << 7;
		return;

		case 0xc026:
			*congo_fg_bank = data & 1;
		return;

		case 0xc027:
			*congo_color_bank = data & 1;
		return;

		case 0xc028:
		//case 0xc02a:
			*zaxxon_bg_scroll &= 0xf00;
			*zaxxon_bg_scroll |= data;
		return;

		case 0xc029:
		//case 0xc02b:
			*zaxxon_bg_scroll &= 0x0ff;
			*zaxxon_bg_scroll |= (data & 0x07) << 8;
		return; 
			
		case 0xc030:
		case 0xc031:
		case 0xc032:
		case 0xc033:
			congo_sprite_custom_w(address & 3, data);
		return;

		case 0xc038:
		case 0xc039:
		case 0xc03a:
		case 0xc03b:
		case 0xc03c:
		case 0xc03d:
		case 0xc03e:
		case 0xc03f:
			*soundlatch = data;
		return;
	}
}

static void __fastcall congo_sound_write(UINT16 address, UINT8 data)
{
	// address mirroring
	if ((address & 0xe000) == 0x6000) address &= ~0x1fff;
	if ((address & 0xe000) == 0x8000) address &= ~0x1ffc;
	if ((address & 0xe000) == 0xa000) address &= ~0x1fff;

	switch (address)
	{
		case 0x6000:
			SN76496Write(0, data);
		return;

		case 0x8000:
		case 0x8001:
		case 0x8002:
		case 0x8003:
			ppi8255_w(0, address  & 0x03, data);
		return;

		case 0xa000:
			SN76496Write(1, data);
		return;
	}
}

static UINT8 __fastcall congo_sound_read(UINT16 address)
{
	// address mirroring
	if ((address & 0xe000) == 0x8000) address &= ~0x1ffc;

	switch (address)
	{
		case 0x8000:
		case 0x8001:
		case 0x8002:
		case 0x8003:
			return ppi8255_r(0, address & 0x03);
	}

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 CharPlane[2] = { 0x4000 * 1, 0x4000 * 0 };
	INT32 TilePlane[3] = { 0x10000 * 2, 0x10000 * 1, 0x10000 * 0 };
	INT32 SpritePlane[3] = { 0x20000 * 2, 0x20000 * 1, 0x20000 * 0 };
	INT32 SpriteXOffs[32] = { STEP8(0, 1), STEP8(64, 1), STEP8(128, 1), STEP8(192, 1) };
	INT32 SpriteYOffs[32] = { STEP8(0, 8), STEP8(256, 8), STEP8(512, 8), STEP8(768, 8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0xc000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x1000);

	GfxDecode(0x0100, 2,  8,  8, CharPlane,   SpriteXOffs, SpriteYOffs, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x6000);

	GfxDecode(0x0400, 3,  8,  8, TilePlane,   SpriteXOffs, SpriteYOffs, 0x040, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0xc000);

	GfxDecode(0x0080, 3, 32, 32, SpritePlane, SpriteXOffs, SpriteYOffs, 0x400, tmp, DrvGfxROM2);

	BurnFree (tmp);

	return 0;
}

static void DrvPaletteInit(INT32 len)
{
	for (INT32 i = 0; i < len; i++)
	{
		INT32 bit0, bit1, bit2, r, g, b;

		bit0 = (DrvColPROM[i] >> 0) & 0x01;
		bit1 = (DrvColPROM[i] >> 1) & 0x01;
		bit2 = (DrvColPROM[i] >> 2) & 0x01;
		r = bit0 * 33 + bit1 * 70 + bit2 * 151;

		bit0 = (DrvColPROM[i] >> 3) & 0x01;
		bit1 = (DrvColPROM[i] >> 4) & 0x01;
		bit2 = (DrvColPROM[i] >> 5) & 0x01;
		g = bit0 * 33 + bit1 * 70 + bit2 * 151;

		bit0 = (DrvColPROM[i] >> 6) & 0x01;
		bit1 = (DrvColPROM[i] >> 7) & 0x01;
		b = bit0 * 78 + bit1 * 168;

		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}

	DrvCharColPROM =  DrvColPROM;
	DrvCharColPROM += 0x100; // Character color prom starts at DrvColPROM + 0x100
}

static void bg_layer_init()
{
	INT32 len = (hardware_type == 2) ? 0x2000 : 0x4000;
	INT32 mask = len-1;

	for (INT32 offs = 0; offs < 32 * 512; offs++)
	{
		INT32 sx = (offs & 0x1f) << 3;
		INT32 sy = (offs >> 5) << 3;

		INT32 moffs = offs & mask;

		INT32 code = (DrvGfxROM3[moffs]) | ((DrvGfxROM3[moffs | len] & 3) << 8);
		INT32 color = (DrvGfxROM3[moffs | len] & 0xf0) >> 1;

		UINT8 *src = DrvGfxROM1 + (code << 6);

		for (INT32 y = 0; y < 8; y++, sy++) {
			for (INT32 x = 0; x < 8; x++) {
				zaxxon_bg_pixmap[sy * 256 + sx + x] = src[(y << 3) | x] | color;
			}
		}
	}
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	if (hardware_type == 2) {
		ZetOpen(1);
		ZetReset();
		ZetClose();
	}

	BurnSampleReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x010000;
	DrvZ80DecROM		= Next; Next += 0x010000;
	DrvZ80ROM2		= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += 0x004000;
	DrvGfxROM1		= Next; Next += 0x010000;
	DrvGfxROM2		= Next; Next += 0x020000;
	DrvGfxROM3		= Next; Next += 0x010000;

	DrvColPROM		= Next; Next += 0x000200;

	DrvPalette		= (UINT32*)Next; Next += 0x0200 * sizeof(INT32);

	zaxxon_bg_pixmap	= Next; Next += 0x100000;

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x001000;
	DrvZ80RAM2		= Next; Next += 0x001000;
	DrvSprRAM		= Next; Next += 0x000100;
	DrvVidRAM		= Next; Next += 0x000400;
	DrvColRAM		= Next; Next += 0x000400;

	interrupt_enable	= Next; Next += 0x000001;

	zaxxon_fg_color		= Next; Next += 0x000001;
	zaxxon_bg_color		= Next; Next += 0x000001;
	zaxxon_bg_enable	= Next; Next += 0x000001;
	congo_color_bank	= Next; Next += 0x000001;
	congo_fg_bank		= Next; Next += 0x000001;
	congo_custom		= Next; Next += 0x000004;
	zaxxon_flipscreen	= Next; Next += 0x000001;
	zaxxon_coin_enable	= Next; Next += 0x000004;
	zaxxon_coin_status	= Next; Next += 0x000004;
	zaxxon_coin_last	= Next; Next += 0x000004;

	zaxxon_bg_scroll	= (UINT32*)Next; Next += 0x000001 * sizeof(INT32);

	soundlatch		= Next; Next += 0x000001;

	sound_state		= Next; Next += 0x000003;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static INT32 DrvInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM  + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x2000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x4000,  2, 1)) return 1;
	
		if (BurnLoadRom(DrvGfxROM0 + 0x0000,  3, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0800,  4, 1)) return 1;
	
		if (BurnLoadRom(DrvGfxROM1 + 0x0000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x2000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x4000,  7, 1)) return 1;
	
		if (BurnLoadRom(DrvGfxROM2 + 0x0000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x4000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x8000, 10, 1)) return 1;
	
		if (BurnLoadRom(DrvGfxROM3 + 0x0000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x2000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x4000, 13, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x6000, 14, 1)) return 1;
	
		if (BurnLoadRom(DrvColPROM + 0x0000, 15, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0100, 16, 1)) return 1;

		DrvGfxDecode();
		DrvPaletteInit(0x200);
		bg_layer_init();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM, 0x0000, 0x5fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM, 0x6000, 0x6fff, MAP_RAM);

	// address mirroring
	for (INT32 i = 0; i < 0x2000; i+= 0x400) {
		ZetMapMemory(DrvVidRAM, 0x8000 + i, 0x83ff + i, MAP_RAM);
	}

	for (INT32 i = 0; i < 0x1000; i+= 0x100) {
		ZetMapMemory(DrvSprRAM, 0xa000 + i, 0xa0ff + i, MAP_RAM);
	}

	ZetSetWriteHandler(zaxxon_write);
	ZetSetReadHandler(zaxxon_read);
	ZetClose();

	ppi8255_init(1);
	PPI0PortWriteA = ZaxxonPPIWriteA;
	PPI0PortWriteB = ZaxxonPPIWriteB;
	PPI0PortWriteC = ZaxxonPPIWriteC;

	BurnSampleInit(0);

	BurnSampleSetAllRoutesAllSamples(0.50, BURN_SND_ROUTE_BOTH);

	// for Zaxxon:
	// Homing Missile
	BurnSampleSetRoute(0, BURN_SND_SAMPLE_ROUTE_1, 0.61, BURN_SND_ROUTE_BOTH);
	BurnSampleSetRoute(0, BURN_SND_SAMPLE_ROUTE_2, 0.61, BURN_SND_ROUTE_BOTH);
	// Ground Missile take-off
	BurnSampleSetRoute(1, BURN_SND_SAMPLE_ROUTE_1, 0.30, BURN_SND_ROUTE_BOTH);
	BurnSampleSetRoute(1, BURN_SND_SAMPLE_ROUTE_2, 0.30, BURN_SND_ROUTE_BOTH);
	// Pew! Pew!
	BurnSampleSetRoute(6, BURN_SND_SAMPLE_ROUTE_1, 0.50, BURN_SND_ROUTE_BOTH);
	BurnSampleSetRoute(6, BURN_SND_SAMPLE_ROUTE_2, 0.50, BURN_SND_ROUTE_BOTH);

	BurnSampleSetRoute(10, BURN_SND_SAMPLE_ROUTE_1, 0.01 * 3, BURN_SND_ROUTE_BOTH);
	BurnSampleSetRoute(10, BURN_SND_SAMPLE_ROUTE_2, 0.01 * 3, BURN_SND_ROUTE_BOTH);
	BurnSampleSetRoute(11, BURN_SND_SAMPLE_ROUTE_1, 0.01 * 3, BURN_SND_ROUTE_BOTH);
	BurnSampleSetRoute(11, BURN_SND_SAMPLE_ROUTE_2, 0.01 * 3, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 CongoInit()
{
	hardware_type = 2;
	futspy_sprite = 1;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM  + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x2000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x4000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x6000,  3, 1)) return 1;
	
		if (BurnLoadRom(DrvGfxROM0 + 0x0000,  4, 1)) return 1;
	
		if (BurnLoadRom(DrvGfxROM1 + 0x0000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x2000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x4000,  7, 1)) return 1;
	
		if (BurnLoadRom(DrvGfxROM2 + 0x0000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x2000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x4000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x6000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x8000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0xa000, 13, 1)) return 1;
	
		if (BurnLoadRom(DrvGfxROM3 + 0x0000, 14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x2000, 15, 1)) return 1;
	
		if (BurnLoadRom(DrvColPROM + 0x0000, 16, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0100, 16, 1)) return 1; // reload

		if (BurnLoadRom(DrvZ80ROM2 + 0x0000, 17, 1)) return 1;

		DrvGfxDecode();
		DrvPaletteInit(0x200);
		bg_layer_init();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM, 0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM, 0x8000, 0x8fff, MAP_RAM);

	// address mirroring
	for (INT32 i = 0; i < 0x2000; i+= 0x800) {
		ZetMapMemory(DrvVidRAM, 0xa000, 0xa3ff, MAP_RAM);
		ZetMapMemory(DrvColRAM, 0xa400, 0xa7ff, MAP_RAM);
	}

	ZetSetWriteHandler(congo_write);
	ZetSetReadHandler(congo_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM2, 0x0000, 0x1fff, MAP_ROM);

	for (INT32 i = 0; i < 0x2000; i+= 0x800) {
		ZetMapMemory(DrvZ80RAM2, 0x4000 + i, 0x47ff + i, MAP_RAM);
	}

	ZetSetWriteHandler(congo_sound_write);
	ZetSetReadHandler(congo_sound_read);
	ZetClose();

	ppi8255_init(1);
	PPI0PortReadA = CongoPPIReadA;
	PPI0PortWriteA = NULL;
	PPI0PortWriteB = CongoPPIWriteB;
	PPI0PortWriteC = CongoPPIWriteC;

	BurnSampleInit(1);
	BurnSampleSetAllRoutesAllSamples(0.10, BURN_SND_ROUTE_BOTH);

	SN76489AInit(0, 4000000,     0);
	SN76489AInit(1, 4000000 / 4, 1);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	ZetExit();
	BurnSampleExit();
	ppi8255_exit();
	if (hardware_type == 2) {
		SN76496Exit();
	}
	GenericTilesExit();
	BurnFree (AllMem);
	AllMem = NULL;

	futspy_sprite = 0;
	hardware_type = 0;
	no_flip = 0;

	return 0;
}

static void draw_fg_layer(INT32 type)
{
	for (INT32 offs = 0x40; offs < 0x3c0; offs++)
	{
		INT32 color;

		INT32 sx = offs & 0x1f;
		INT32 sy = offs >> 5;

		INT32 code = DrvVidRAM[offs] + (*congo_fg_bank << 8);

		switch (type)
		{
			case 0:
				color = DrvCharColPROM[(sx | ((sy >> 2) << 5))] & 0x0f;
			break;

			case 2:
				color = (DrvColRAM[offs] & 0x1f) + (*congo_color_bank << 8);
			break;

			default:
				color = DrvCharColPROM[offs] & 0x0f;
			break;
		}

		if (no_flip) {
			Render8x8Tile_Mask_Clip(pTransDraw, code, sx * 8, (sy * 8) - 16, color, 3, 0, 0, DrvGfxROM0);
		} else {
			Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, code, 248 - (sx * 8), 232 - (sy * 8), color, 3, 0, 0, DrvGfxROM0);
		}
	}
}

static void draw_background(INT32 skew)
{
	if (*zaxxon_bg_enable)
	{
		UINT8 *pixmap = zaxxon_bg_pixmap;
		INT32 colorbase = *zaxxon_bg_color + (*congo_color_bank << 8);
		INT32 xmask = 255;
		INT32 ymask = 4095;
		INT32 flipmask = *zaxxon_flipscreen ? 0xff : 0x00;
		INT32 flipoffs = *zaxxon_flipscreen ? 0x38 : 0x40;
		INT32 x, y;

		if (*zaxxon_flipscreen)
			flipoffs += 7;
		else
			flipoffs -= 1;

		for (y = 0; y < 224; y++)
		{
			UINT16 *dst = pTransDraw + (223-y) * 0x100;
			INT32 srcx, srcy, vf;
			UINT8 *src;

			vf = (y + 16) ^ flipmask;
			srcy = vf + ((*zaxxon_bg_scroll << 1) ^ 0xfff) + 1;
			src = pixmap + (srcy & ymask) * 0x100;

			for (x = 0; x < 256; x++)
			{
				srcx = x ^ flipmask;
				if (skew)
				{
					srcx += ((vf >> 1) ^ 0xff) + 1;
					srcx += flipoffs;
				}

				dst[255-x] = src[srcx & xmask] + colorbase;
			}
		}
	}
	else
		memset (pTransDraw, 0, nScreenWidth * nScreenHeight * 2);
}

static INT32 find_minimum_y(UINT8 value)
{
	INT32 flipmask = *zaxxon_flipscreen ? 0xff : 0x00;
	INT32 flipconst = *zaxxon_flipscreen ? 0xef : 0xf1;
	INT32 y;

	for (y = 0; y < 256; y += 16)
	{
		INT32 sum = (value + flipconst + 1) + (y ^ flipmask);
		if ((sum & 0xe0) == 0xe0)
			break;
	}

	while (1)
	{
		INT32 sum = (value + flipconst + 1) + ((y - 1) ^ flipmask);
		if ((sum & 0xe0) != 0xe0)
			break;
		y--;
	}

	return (y + 1) & 0xff;
}


static inline INT32 find_minimum_x(UINT8 value)
{
	INT32 flipmask = *zaxxon_flipscreen ? 0xff : 0x00;
	INT32 x;

	x = (value + 0xef + 1) ^ flipmask;
	if (flipmask)
		x -= 31;
	return x & 0xff;
}

static void draw_sprites(UINT16 flipxmask, UINT16 flipymask)
{
	INT32 flipmask = *zaxxon_flipscreen ? 0xff : 0x00;

	for (INT32 offs = 0x7c; offs >= 0; offs -= 4)
	{
		INT32 sy = find_minimum_y(DrvSprRAM[offs]);
		INT32 flipy = (DrvSprRAM[offs + (flipymask >> 8)] ^ flipmask) & flipymask;
		INT32 flipx = (DrvSprRAM[offs + (flipxmask >> 8)] ^ flipmask) & flipxmask;

		INT32 code = DrvSprRAM[offs + 1] & (0x3f | (futspy_sprite * 0x40));

		INT32 color = (DrvSprRAM[offs + 2] & 0x1f) + (*congo_color_bank << 5);
		INT32 sx = find_minimum_x(DrvSprRAM[offs + 3]);

		sx = (240 - sx) - 16;
		if (sx < -15) sx += 256;
		sy = (240 - sy) - 32;
		if (sy < -15) sy += 256;

		if (flipy) {
			if (flipx) {
				Render32x32Tile_Mask_Clip(pTransDraw, code, sx,       sy,       color, 3, 0, 0, DrvGfxROM2);
				Render32x32Tile_Mask_Clip(pTransDraw, code, sx - 256, sy,       color, 3, 0, 0, DrvGfxROM2);
				Render32x32Tile_Mask_Clip(pTransDraw, code, sx,       sy - 256, color, 3, 0, 0, DrvGfxROM2);
				Render32x32Tile_Mask_Clip(pTransDraw, code, sx - 256, sy - 256, color, 3, 0, 0, DrvGfxROM2);

			} else {
				Render32x32Tile_Mask_FlipX_Clip(pTransDraw, code, sx,       sy,       color, 3, 0, 0, DrvGfxROM2);
				Render32x32Tile_Mask_FlipX_Clip(pTransDraw, code, sx - 256, sy,       color, 3, 0, 0, DrvGfxROM2);
				Render32x32Tile_Mask_FlipX_Clip(pTransDraw, code, sx,       sy - 256, color, 3, 0, 0, DrvGfxROM2);
				Render32x32Tile_Mask_FlipX_Clip(pTransDraw, code, sx - 256, sy - 256, color, 3, 0, 0, DrvGfxROM2);
			}
		} else {
			if (flipx) {
				Render32x32Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, DrvGfxROM2);
			} else {
				if (sx > 31 && sx < (nScreenWidth - 31) && sy > 31 && sy < (nScreenHeight-31)) {
					Render32x32Tile_Mask_FlipXY(pTransDraw, code, sx, sy, color, 3, 0, 0, DrvGfxROM2);
				} else {
					Render32x32Tile_Mask_FlipXY_Clip(pTransDraw, code, sx,       sy,       color, 3, 0, 0, DrvGfxROM2);
					Render32x32Tile_Mask_FlipXY_Clip(pTransDraw, code, sx - 256, sy,       color, 3, 0, 0, DrvGfxROM2);
					Render32x32Tile_Mask_FlipXY_Clip(pTransDraw, code, sx,       sy - 256, color, 3, 0, 0, DrvGfxROM2);
					Render32x32Tile_Mask_FlipXY_Clip(pTransDraw, code, sx - 256, sy - 256, color, 3, 0, 0, DrvGfxROM2);
				}
			}	
		}
	}
}


static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit(0x200);
		DrvRecalc = 0;
	}

	if (~nBurnLayer & 1) BurnTransferClear();

	if (hardware_type == 1) {
		if (nBurnLayer & 1) draw_background(0);
	} else {
		if (nBurnLayer & 1) draw_background(1);
	}

	INT32 flipx_mask = 0x140;
	if (futspy_sprite) flipx_mask += 0x040;
	if (hardware_type == 2) flipx_mask += 0x100;

	if (nBurnLayer & 2) draw_sprites(flipx_mask, 0x180);

	if (nBurnLayer & 4) draw_fg_layer(hardware_type);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void zaxxon_coin_lockout()
{
	// soft-coin lockout - prevents 30 coins per 1 insert-coin keypress.
	if (DrvJoy4[0]) // a coin inserted
		zaxxon_coin_inserted(0);
	if (DrvJoy4[1]) // b coin inserted
		zaxxon_coin_inserted(1);
	if (DrvJoy4[2]) // service pressed
		zaxxon_coin_inserted(2);
	DrvInputs[2] += (zaxxon_coin_r(0)) ? 0x20 : 0;
	DrvInputs[2] += (zaxxon_coin_r(1)) ? 0x40 : 0;
	DrvInputs[2] += (zaxxon_coin_r(2)) ? 0x80 : 0;
	zaxxon_coin_last[0] = DrvJoy4[0];
	zaxxon_coin_last[1] = DrvJoy4[1];
	zaxxon_coin_last[2] = DrvJoy4[2];
	// end soft-coin lockout
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		DrvInputs[0] = 0x00;
		DrvInputs[1] = 0x00;
		DrvInputs[2] = 0x00;
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			// DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
		DrvInputs[2] ^= (DrvJoy3[2] & 1) << 2;   //  start 1
		DrvInputs[2] ^= (DrvJoy3[3] & 1) << 3;   //  start 2
		zaxxon_coin_lockout();
	}

	ZetOpen(0);
	ZetRun(3041250 / 60);
	if (*interrupt_enable) ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
	ZetClose();

	if (pBurnDraw) {
		DrvDraw();
	}

	if (pBurnSoundOut) {
		BurnSampleRender(pBurnSoundOut, nBurnSoundLen);
	}

	return 0;
}

static INT32 CongoFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		DrvInputs[0] = 0x00;
		DrvInputs[1] = 0x00;
		DrvInputs[2] = 0x00;
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
		zaxxon_coin_lockout();
	}

	INT32 nCyclesSegment;
	INT32 nInterleave = 32;
	INT32 nCyclesTotal[2] = { 3041250 / 60, 4000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };
	INT32 nSoundBufferPos = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesSegment = nCyclesTotal[0] / nInterleave;
		nCyclesDone[0] += ZetRun(nCyclesSegment);
		if (i == nInterleave-1 && *interrupt_enable) ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		ZetClose();

		ZetOpen(1);
		nCyclesSegment = nCyclesTotal[1] / nInterleave;
		nCyclesDone[1] += ZetRun(nCyclesSegment);
		if ((i%7)==0) ZetSetIRQLine(0, CPU_IRQSTATUS_AUTO);
		ZetClose();

		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			SN76496Update(0, pSoundBuf, nSegmentLength);
			SN76496Update(1, pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			SN76496Update(0, pSoundBuf, nSegmentLength);
			SN76496Update(1, pSoundBuf, nSegmentLength);
		}
		BurnSampleRender(pBurnSoundOut, nBurnSoundLen);
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
		ZetScan(nAction);
		BurnSampleScan(nAction, pnMin);

		if (hardware_type == 2) {
			SN76496Scan(nAction, pnMin);
		}
	}

	return 0;
}


static struct BurnSampleInfo zaxxonSampleDesc[] = {
	{"03",  	 SAMPLE_AUTOLOOP },	/* 0 - Homing Missile */
	{"02",  	 SAMPLE_NOLOOP },	/* 1 - Base Missile */
	{"01",  	 SAMPLE_AUTOLOOP },	/* 2 - Laser (force field) */
	{"00",  	 SAMPLE_AUTOLOOP },	/* 3 - Battleship (end of level boss) */
	{"11",   	 SAMPLE_NOLOOP },	/* 4 - S-Exp (enemy explosion) */
	{"10",   	 SAMPLE_NOLOOP },	/* 5 - M-Exp (ship explosion) */
	{"08",   	 SAMPLE_NOLOOP },	/* 6 - Cannon (ship fire) */
	{"23",   	 SAMPLE_NOLOOP },	/* 7 - Shot (enemy fire) */
	{"21",   	 SAMPLE_NOLOOP },	/* 8 - Alarm 2 (target lock) */
	{"20",   	 SAMPLE_NOLOOP },	/* 9 - Alarm 3 (low fuel) */
	{"05",   	 SAMPLE_AUTOLOOP },	/* 10 - initial background noise */
	{"04",   	 SAMPLE_AUTOLOOP },	/* 11 - looped asteroid noise */
	{ "",            0             }
};

STD_SAMPLE_PICK(zaxxon)
STD_SAMPLE_FN(zaxxon)


static struct BurnSampleInfo congoSampleDesc[] = {
	{"gorilla",		SAMPLE_NOLOOP },  /* 0 */
	{"bass",		SAMPLE_NOLOOP },     /* 1 */
	{"congal",		SAMPLE_NOLOOP },   /* 2 */
	{"congah",		SAMPLE_NOLOOP },   /* 3 */
	{"rim",			SAMPLE_NOLOOP },      /* 4 */
	{ "",            0             }
};

STD_SAMPLE_PICK(congo)
STD_SAMPLE_FN(congo)


// Zaxxon (set 1, rev D)

static struct BurnRomInfo zaxxonRomDesc[] = {
	{ "zaxxon_rom3d.u27",	0x2000, 0x6e2b4a30, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "zaxxon_rom2d.u28",	0x2000, 0x1c9ea398, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "zaxxon_rom1d.u29",	0x1000, 0x1c123ef9, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "zaxxon_rom14.u68",	0x0800, 0x07bf8c52, 2 | BRF_GRA },           //  3 Characters
	{ "zaxxon_rom15.u69",	0x0800, 0xc215edcb, 2 | BRF_GRA },           //  4

	{ "zaxxon_rom6.u113",	0x2000, 0x6e07bb68, 3 | BRF_GRA },           //  5 Background Tiles
	{ "zaxxon_rom5.u112",	0x2000, 0x0a5bce6a, 3 | BRF_GRA },           //  6
	{ "zaxxon_rom4.u111",	0x2000, 0xa5bf1465, 3 | BRF_GRA },           //  7

	{ "zaxxon_rom11.u77",	0x2000, 0xeaf0dd4b, 4 | BRF_GRA },           //  8 Sprites
	{ "zaxxon_rom12.u78",	0x2000, 0x1c5369c7, 4 | BRF_GRA },           //  9
	{ "zaxxon_rom13.u79",	0x2000, 0xab4e8a9a, 4 | BRF_GRA },           // 10

	{ "zaxxon_rom8.u91",	0x2000, 0x28d65063, 5 | BRF_GRA },           // 11 Tilemaps
	{ "zaxxon_rom7.u90",	0x2000, 0x6284c200, 5 | BRF_GRA },           // 12
	{ "zaxxon_rom10.u93",	0x2000, 0xa95e61fd, 5 | BRF_GRA },           // 13
	{ "zaxxon_rom9.u92",	0x2000, 0x7e42691f, 5 | BRF_GRA },           // 14

	{ "mro16.u76",			0x0100, 0x6cc6695b, 6 | BRF_GRA },           // 15 Color Proms
	{ "zaxxon.u72",			0x0100, 0xdeaa21f7, 6 | BRF_GRA },           // 16
};

STD_ROM_PICK(zaxxon)
STD_ROM_FN(zaxxon)

struct BurnDriver BurnDrvZaxxon = {
	"zaxxon", NULL, NULL, "zaxxon", "1982",
	"Zaxxon (set 1, rev D)\0", NULL, "Sega", "Zaxxon",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_SEGA_MISC, GBF_VERSHOOT, 0,
	NULL, zaxxonRomInfo, zaxxonRomName, zaxxonSampleInfo, zaxxonSampleName, ZaxxonInputInfo, ZaxxonDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Zaxxon (set 2, unknown rev)

static struct BurnRomInfo zaxxon2RomDesc[] = {
	{ "zaxxon_rom3a.u27",	0x2000, 0xb18e428a, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "zaxxon_rom2a.u28",	0x2000, 0x1c9ea398, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "zaxxon_rom1a.u29",	0x1000, 0x1977d933, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "zaxxon_rom14.u68",	0x0800, 0x07bf8c52, 2 | BRF_GRA },           //  3 Characters
	{ "zaxxon_rom15.u69",	0x0800, 0xc215edcb, 2 | BRF_GRA },           //  4

	{ "zaxxon_rom6.u113",	0x2000, 0x6e07bb68, 3 | BRF_GRA },           //  5 Background Tiles
	{ "zaxxon_rom5.u112",	0x2000, 0x0a5bce6a, 3 | BRF_GRA },           //  6
	{ "zaxxon_rom4.u111",	0x2000, 0xa5bf1465, 3 | BRF_GRA },           //  7

	{ "zaxxon_rom11.u77",	0x2000, 0xeaf0dd4b, 4 | BRF_GRA },           //  8 Sprites
	{ "zaxxon_rom12.u78",	0x2000, 0x1c5369c7, 4 | BRF_GRA },           //  9
	{ "zaxxon_rom13.u79",	0x2000, 0xab4e8a9a, 4 | BRF_GRA },           // 10

	{ "zaxxon_rom8.u91",	0x2000, 0x28d65063, 5 | BRF_GRA },           // 11 Tilemaps
	{ "zaxxon_rom7.u90",	0x2000, 0x6284c200, 5 | BRF_GRA },           // 12
	{ "zaxxon_rom10.u93",	0x2000, 0xa95e61fd, 5 | BRF_GRA },           // 13
	{ "zaxxon_rom9.u92",	0x2000, 0x7e42691f, 5 | BRF_GRA },           // 14

	{ "mro16.u76",			0x0100, 0x6cc6695b, 6 | BRF_GRA },           // 15 Color Proms
	{ "mro17.u41",			0x0100, 0xa9e1fb43, 6 | BRF_GRA },           // 16
};

STD_ROM_PICK(zaxxon2)
STD_ROM_FN(zaxxon2)

struct BurnDriver BurnDrvZaxxon2 = {
	"zaxxon2", "zaxxon", NULL, "zaxxon", "1982",
	"Zaxxon (set 2, unknown rev)\0", NULL, "Sega", "Zaxxon",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_SEGA_MISC, GBF_VERSHOOT, 0,
	NULL, zaxxon2RomInfo, zaxxon2RomName, zaxxonSampleInfo, zaxxonSampleName, ZaxxonInputInfo, ZaxxonDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Zaxxon (set 3, unknown rev)

static struct BurnRomInfo zaxxon3RomDesc[] = {
	{ "zaxxon3_alt.u27",	0x2000, 0x2f2f2b7c, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "zaxxon2_alt.u28",	0x2000, 0xae7e1c38, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "zaxxon1_alt.u29",	0x1000, 0xcc67c097, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "zaxxon_rom14.u68",	0x0800, 0x07bf8c52, 2 | BRF_GRA },           //  3 Characters
	{ "zaxxon_rom15.u69",	0x0800, 0xc215edcb, 2 | BRF_GRA },           //  4

	{ "zaxxon_rom6.u113",	0x2000, 0x6e07bb68, 3 | BRF_GRA },           //  5 Background Tiles
	{ "zaxxon_rom5.u112",	0x2000, 0x0a5bce6a, 3 | BRF_GRA },           //  6
	{ "zaxxon_rom4.u111",	0x2000, 0xa5bf1465, 3 | BRF_GRA },           //  7

	{ "zaxxon_rom11.u77",	0x2000, 0xeaf0dd4b, 4 | BRF_GRA },           //  8 Sprites
	{ "zaxxon_rom12.u78",	0x2000, 0x1c5369c7, 4 | BRF_GRA },           //  9
	{ "zaxxon_rom13.u79",	0x2000, 0xab4e8a9a, 4 | BRF_GRA },           // 10

	{ "zaxxon_rom8.u91",	0x2000, 0x28d65063, 5 | BRF_GRA },           // 11 Tilemaps
	{ "zaxxon_rom7.u90",	0x2000, 0x6284c200, 5 | BRF_GRA },           // 12
	{ "zaxxon_rom10.u93",	0x2000, 0xa95e61fd, 5 | BRF_GRA },           // 13
	{ "zaxxon_rom9.u92",	0x2000, 0x7e42691f, 5 | BRF_GRA },           // 14

	{ "mro16.u76",			0x0100, 0x6cc6695b, 6 | BRF_GRA },           // 15 Color Proms
	{ "mro17.u41",			0x0100, 0xa9e1fb43, 6 | BRF_GRA },           // 16
};

STD_ROM_PICK(zaxxon3)
STD_ROM_FN(zaxxon3)

struct BurnDriver BurnDrvZaxxon3 = {
	"zaxxon3", "zaxxon", NULL, "zaxxon", "1982",
	"Zaxxon (set 3, unknown rev)\0", NULL, "Sega", "Zaxxon",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_SEGA_MISC, GBF_VERSHOOT, 0,
	NULL, zaxxon3RomInfo, zaxxon3RomName, zaxxonSampleInfo, zaxxonSampleName, ZaxxonInputInfo, ZaxxonDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Zaxxon (Japan)

static struct BurnRomInfo zaxxonjRomDesc[] = {
	{ "zaxxon_rom3.u13",	0x2000, 0x925168c7, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "zaxxon_rom2.u12",	0x2000, 0xc088df92, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "zaxxon_rom1.u11",	0x1000, 0xf832dd79, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "zaxxon_rom14.u54",	0x0800, 0x07bf8c52, 2 | BRF_GRA },           //  3 Characters
	{ "zaxxon_rom15.u55",	0x0800, 0xc215edcb, 2 | BRF_GRA },           //  4

	{ "zaxxon_rom6.u70",	0x2000, 0x6e07bb68, 3 | BRF_GRA },           //  5 Background Tiles
	{ "zaxxon_rom5.u69",	0x2000, 0x0a5bce6a, 3 | BRF_GRA },           //  6
	{ "zaxxon_rom4.u68",	0x2000, 0xa5bf1465, 3 | BRF_GRA },           //  7

	{ "zaxxon_rom11.u59",	0x2000, 0xeaf0dd4b, 4 | BRF_GRA },           //  8 Sprites
	{ "zaxxon_rom12.u60",	0x2000, 0x1c5369c7, 4 | BRF_GRA },           //  9
	{ "zaxxon_rom13.u61",	0x2000, 0xab4e8a9a, 4 | BRF_GRA },           // 10

	{ "zaxxon_rom8.u58",	0x2000, 0x28d65063, 5 | BRF_GRA },           // 11 Tilemaps
	{ "zaxxon_rom7.u57",	0x2000, 0x6284c200, 5 | BRF_GRA },           // 12
	{ "zaxxon_rom10.u60",	0x2000, 0xa95e61fd, 5 | BRF_GRA },           // 13
	{ "zaxxon_rom9.u59",	0x2000, 0x7e42691f, 5 | BRF_GRA },           // 14

	{ "mro16.u76",			0x0100, 0x6cc6695b, 6 | BRF_GRA },           // 15 Color Proms
	{ "mro17.u41",			0x0100, 0xa9e1fb43, 6 | BRF_GRA },           // 16
};

STD_ROM_PICK(zaxxonj)
STD_ROM_FN(zaxxonj)

static void zaxxonj_decode()
{
	static const UINT8 data_xortable[2][8] =
	{
		{ 0x0a,0x0a,0x22,0x22,0xaa,0xaa,0x82,0x82 },	/* ...............0 */
		{ 0xa0,0xaa,0x28,0x22,0xa0,0xaa,0x28,0x22 },	/* ...............1 */
	};

	static const UINT8 opcode_xortable[8][8] =
	{
		{ 0x8a,0x8a,0x02,0x02,0x8a,0x8a,0x02,0x02 },	/* .......0...0...0 */
		{ 0x80,0x80,0x08,0x08,0xa8,0xa8,0x20,0x20 },	/* .......0...0...1 */
		{ 0x8a,0x8a,0x02,0x02,0x8a,0x8a,0x02,0x02 },	/* .......0...1...0 */
		{ 0x02,0x08,0x2a,0x20,0x20,0x2a,0x08,0x02 },	/* .......0...1...1 */
		{ 0x88,0x0a,0x88,0x0a,0xaa,0x28,0xaa,0x28 },	/* .......1...0...0 */
		{ 0x80,0x80,0x08,0x08,0xa8,0xa8,0x20,0x20 },	/* .......1...0...1 */
		{ 0x88,0x0a,0x88,0x0a,0xaa,0x28,0xaa,0x28 },	/* .......1...1...0 */
		{ 0x02,0x08,0x2a,0x20,0x20,0x2a,0x08,0x02 } 	/* .......1...1...1 */
	};

	INT32 A;
	UINT8 *rom = DrvZ80ROM;
	INT32 size = 0x6000;
	UINT8 *decrypt = DrvZ80DecROM;

	ZetOpen(0);
	ZetMapArea(0x0000, 0x5fff, 2, DrvZ80DecROM, DrvZ80ROM );
	ZetClose();

	for (A = 0x0000; A < size; A++)
	{
		INT32 i,j;
		UINT8 src;

		src = rom[A];

		/* pick the translation table from bit 0 of the address */
		i = A & 1;

		/* pick the offset in the table from bits 1, 3 and 5 of the source data */
		j = ((src >> 1) & 1) + (((src >> 3) & 1) << 1) + (((src >> 5) & 1) << 2);
		/* the bottom half of the translation table is the mirror image of the top */
		if (src & 0x80) j = 7 - j;

		/* decode the ROM data */
		rom[A] = src ^ data_xortable[i][j];

		/* now decode the opcodes */
		/* pick the translation table from bits 0, 4, and 8 of the address */
		i = ((A >> 0) & 1) + (((A >> 4) & 1) << 1) + (((A >> 8) & 1) << 2);
		decrypt[A] = src ^ opcode_xortable[i][j];
	}
}

static INT32 ZaxxonjInit()
{
	INT32 nRet = DrvInit();

	if (nRet == 0) {
		zaxxonj_decode();
	}

	return nRet;
}

struct BurnDriver BurnDrvZaxxonj = {
	"zaxxonj", "zaxxon", NULL, "zaxxon", "1982",
	"Zaxxon (Japan)\0", NULL, "Sega", "Zaxxon",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_SEGA_MISC, GBF_VERSHOOT, 0,
	NULL, zaxxonjRomInfo, zaxxonjRomName, zaxxonSampleInfo, zaxxonSampleName, ZaxxonInputInfo, ZaxxonDIPInfo,
	ZaxxonjInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Jackson

static struct BurnRomInfo zaxxonbRomDesc[] = {
	{ "jackson_rom3.u27",	0x2000, 0x125bca1c, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "jackson_rom2.u28",	0x2000, 0xc088df92, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "jackson_rom1.u29",	0x1000, 0xe7bdc417, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "zaxxon_rom14.u54",	0x0800, 0x07bf8c52, 2 | BRF_GRA },           //  3 Characters
	{ "zaxxon_rom15.u55",	0x0800, 0xc215edcb, 2 | BRF_GRA },           //  4

	{ "zaxxon_rom6.u70",	0x2000, 0x6e07bb68, 3 | BRF_GRA },           //  5 Background Tiles
	{ "zaxxon_rom5.u69",	0x2000, 0x0a5bce6a, 3 | BRF_GRA },           //  6
	{ "zaxxon_rom4.u68",	0x2000, 0xa5bf1465, 3 | BRF_GRA },           //  7

	{ "zaxxon_rom11.u59",	0x2000, 0xeaf0dd4b, 4 | BRF_GRA },           //  8 Sprites
	{ "zaxxon_rom12.u60",	0x2000, 0x1c5369c7, 4 | BRF_GRA },           //  9
	{ "zaxxon_rom13.u61",	0x2000, 0xab4e8a9a, 4 | BRF_GRA },           // 10

	{ "zaxxon_rom8.u58",	0x2000, 0x28d65063, 5 | BRF_GRA },           // 11 Tilemaps
	{ "zaxxon_rom7.u57",	0x2000, 0x6284c200, 5 | BRF_GRA },           // 12
	{ "zaxxon_rom10.u60",	0x2000, 0xa95e61fd, 5 | BRF_GRA },           // 13
	{ "zaxxon_rom9.u59",	0x2000, 0x7e42691f, 5 | BRF_GRA },           // 14

	{ "mro16.u76",			0x0100, 0x6cc6695b, 6 | BRF_GRA },           // 15 Color Proms
	{ "zaxxon.u72",			0x0100, 0xdeaa21f7, 6 | BRF_GRA },           // 16
};

STD_ROM_PICK(zaxxonb)
STD_ROM_FN(zaxxonb)

struct BurnDriver BurnDrvZaxxonb = {
	"zaxxonb", "zaxxon", NULL, "zaxxon", "1982",
	"Jackson\0", NULL, "bootleg", "Zaxxon",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_SEGA_MISC, GBF_VERSHOOT, 0,
	NULL, zaxxonbRomInfo, zaxxonbRomName, zaxxonSampleInfo, zaxxonSampleName, ZaxxonInputInfo, ZaxxonDIPInfo,
	ZaxxonjInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Super Zaxxon

static struct BurnRomInfo szaxxonRomDesc[] = {
	{ "1804e.u27",		0x2000, 0xaf7221da, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "1803e.u28",		0x2000, 0x1b90fb2a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "1802e.u29",		0x1000, 0x07258b4a, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "1815b.u68",		0x0800, 0xbccf560c, 2 | BRF_GRA },           //  3 Characters
	{ "1816b.u69",		0x0800, 0xd28c628b, 2 | BRF_GRA },           //  4

	{ "1807b.u113",		0x2000, 0xf51af375, 3 | BRF_GRA },           //  5 Background Tiles
	{ "1806b.u112",		0x2000, 0xa7de021d, 3 | BRF_GRA },           //  6
	{ "1805b.u111",		0x2000, 0x5bfb3b04, 3 | BRF_GRA },           //  7

	{ "1812e.u77",		0x2000, 0x1503ae41, 4 | BRF_GRA },           //  8 Sprites
	{ "1813e.u78",		0x2000, 0x3b53d83f, 4 | BRF_GRA },           //  9
	{ "1814e.u79",		0x2000, 0x581e8793, 4 | BRF_GRA },           // 10

	{ "1809b.u91",		0x2000, 0xdd1b52df, 5 | BRF_GRA },           // 11 Tilemaps
	{ "1808b.u90",		0x2000, 0xb5bc07f0, 5 | BRF_GRA },           // 12
	{ "1811b.u93",		0x2000, 0x68e84174, 5 | BRF_GRA },           // 13
	{ "1810b.u92",		0x2000, 0xa509994b, 5 | BRF_GRA },           // 14

	{ "pr-5168.u98",	0x0100, 0x15727a9f, 6 | BRF_GRA },           // 15 Color Proms
	{ "pr-5167.u72",	0x0100, 0xdeaa21f7, 6 | BRF_GRA },           // 16
};

STD_ROM_PICK(szaxxon)
STD_ROM_FN(szaxxon)

static void sega_decode(const UINT8 convtable[32][4])
{
	INT32 A;
	INT32 length = 0x6000;
	INT32 cryptlen = length;
	UINT8 *rom = DrvZ80ROM;
	UINT8 *decrypted = DrvZ80DecROM;

	memcpy (DrvZ80DecROM, DrvZ80ROM, 0x6000);

	ZetOpen(0);
	ZetMapArea(0x0000, 0x5fff, 2, DrvZ80DecROM, DrvZ80ROM);
	ZetClose();

	for (A = 0x0000;A < cryptlen;A++)
	{
		INT32 xorval = 0;
		UINT8 src = rom[A];
		/* pick the translation table from bits 0, 4, 8 and 12 of the address */
		INT32 row = (A & 1) + (((A >> 4) & 1) << 1) + (((A >> 8) & 1) << 2) + (((A >> 12) & 1) << 3);
		/* pick the offset in the table from bits 3 and 5 of the source data */
		INT32 col = ((src >> 3) & 1) + (((src >> 5) & 1) << 1);
		/* the bottom half of the translation table is the mirror image of the top */
		if (src & 0x80)
		{
			col = 3 - col;
			xorval = 0xa8;
		}

		/* decode the opcodes */
		decrypted[A] = (src & ~0xa8) | (convtable[2*row][col] ^ xorval);

		/* decode the data */
		rom[A] = (src & ~0xa8) | (convtable[2*row+1][col] ^ xorval);

		if (convtable[2*row][col] == 0xff)	/* table incomplete! (for development) */
			decrypted[A] = 0xee;
		if (convtable[2*row+1][col] == 0xff)	/* table incomplete! (for development) */
			rom[A] = 0xee;
	}
}

static void szaxxon_decode()
{
	static const UINT8 convtable[32][4] =
	{
		/*       opcode                   data                     address      */
		/*  A    B    C    D         A    B    C    D                           */
		{ 0x88,0xa8,0x80,0xa0 }, { 0x28,0x20,0xa8,0xa0 },	/* ...0...0...0...0 */
		{ 0x08,0x28,0x88,0xa8 }, { 0x88,0x80,0x08,0x00 },	/* ...0...0...0...1 */
		{ 0xa8,0x28,0xa0,0x20 }, { 0x20,0xa0,0x00,0x80 },	/* ...0...0...1...0 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0x28,0x20,0xa8,0xa0 },	/* ...0...0...1...1 */
		{ 0x08,0x28,0x88,0xa8 }, { 0x88,0x80,0x08,0x00 },	/* ...0...1...0...0 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0x28,0x20,0xa8,0xa0 },	/* ...0...1...0...1 */
		{ 0xa8,0x28,0xa0,0x20 }, { 0x20,0xa0,0x00,0x80 },	/* ...0...1...1...0 */
		{ 0x08,0x28,0x88,0xa8 }, { 0x88,0x80,0x08,0x00 },	/* ...0...1...1...1 */
		{ 0x08,0x28,0x88,0xa8 }, { 0x88,0x80,0x08,0x00 },	/* ...1...0...0...0 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0x28,0x20,0xa8,0xa0 },	/* ...1...0...0...1 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0x28,0x20,0xa8,0xa0 },	/* ...1...0...1...0 */
		{ 0xa8,0x28,0xa0,0x20 }, { 0x20,0xa0,0x00,0x80 },	/* ...1...0...1...1 */
		{ 0xa8,0x28,0xa0,0x20 }, { 0x20,0xa0,0x00,0x80 },	/* ...1...1...0...0 */
		{ 0xa8,0x28,0xa0,0x20 }, { 0x20,0xa0,0x00,0x80 },	/* ...1...1...0...1 */
		{ 0x08,0x28,0x88,0xa8 }, { 0x88,0x80,0x08,0x00 },	/* ...1...1...1...0 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0x28,0x20,0xa8,0xa0 }	/* ...1...1...1...1 */
	};

	sega_decode(convtable);
}

static INT32 sZaxxonInit()
{
	INT32 nRet = DrvInit();

	if (nRet == 0) {
		szaxxon_decode();
	}

	return nRet;
}

struct BurnDriver BurnDrvSzaxxon = {
	"szaxxon", NULL, NULL, "zaxxon", "1982",
	"Super Zaxxon\0", NULL, "Sega", "Zaxxon",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_SEGA_MISC, GBF_VERSHOOT, 0,
	NULL, szaxxonRomInfo, szaxxonRomName, zaxxonSampleInfo, zaxxonSampleName, ZaxxonInputInfo, SzaxxonDIPInfo,
	sZaxxonInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Future Spy

static struct BurnRomInfo futspyRomDesc[] = {
	{ "fs_snd.u27",		0x2000, 0x7578fe7f, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "fs_snd.u28",		0x2000, 0x8ade203c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "fs_snd.u29",		0x1000, 0x734299c3, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "fs_snd.u68",		0x0800, 0x305fae2d, 2 | BRF_GRA },           //  3 Characters
	{ "fs_snd.u69",		0x0800, 0x3c5658c0, 2 | BRF_GRA },           //  4

	{ "fs_vid.u113",	0x2000, 0x36d2bdf6, 3 | BRF_GRA },           //  5 Background Tiles
	{ "fs_vid.u112",	0x2000, 0x3740946a, 3 | BRF_GRA },           //  6
	{ "fs_vid.u111",	0x2000, 0x4cd4df98, 3 | BRF_GRA },           //  7

	{ "fs_vid.u77",		0x4000, 0x1b93c9ec, 4 | BRF_GRA },           //  8 Sprites
	{ "fs_vid.u78",		0x4000, 0x50e55262, 4 | BRF_GRA },           //  9
	{ "fs_vid.u79",		0x4000, 0xbfb02e3e, 4 | BRF_GRA },           // 10

	{ "fs_vid.u91",		0x2000, 0x86da01f4, 5 | BRF_GRA },           // 11 Tilemaps
	{ "fs_vid.u90",		0x2000, 0x2bd41d2d, 5 | BRF_GRA },           // 12
	{ "fs_vid.u93",		0x2000, 0xb82b4997, 5 | BRF_GRA },           // 13
	{ "fs_vid.u92",		0x2000, 0xaf4015af, 5 | BRF_GRA },           // 14

	{ "futrprom.u98",	0x0100, 0x9ba2acaa, 6 | BRF_GRA },           // 15 Color Proms
	{ "futrprom.u72",	0x0100, 0xf9e26790, 6 | BRF_GRA },           // 16
};

STD_ROM_PICK(futspy)
STD_ROM_FN(futspy)

static void futspy_decode()
{
	static const UINT8 convtable[32][4] =
	{
		/*       opcode                   data                     address      */
		/*  A    B    C    D         A    B    C    D                           */
		{ 0x28,0x08,0x20,0x00 }, { 0x28,0x08,0x20,0x00 },	/* ...0...0...0...0 */
		{ 0x80,0x00,0xa0,0x20 }, { 0x08,0x88,0x00,0x80 },	/* ...0...0...0...1 */
		{ 0x80,0x00,0xa0,0x20 }, { 0x08,0x88,0x00,0x80 },	/* ...0...0...1...0 */
		{ 0xa0,0x80,0x20,0x00 }, { 0x20,0x28,0xa0,0xa8 },	/* ...0...0...1...1 */
		{ 0x28,0x08,0x20,0x00 }, { 0x88,0x80,0xa8,0xa0 },	/* ...0...1...0...0 */
		{ 0x80,0x00,0xa0,0x20 }, { 0x08,0x88,0x00,0x80 },	/* ...0...1...0...1 */
		{ 0x80,0x00,0xa0,0x20 }, { 0x20,0x28,0xa0,0xa8 },	/* ...0...1...1...0 */
		{ 0x20,0x28,0xa0,0xa8 }, { 0x08,0x88,0x00,0x80 },	/* ...0...1...1...1 */
		{ 0x88,0x80,0xa8,0xa0 }, { 0x28,0x08,0x20,0x00 },	/* ...1...0...0...0 */
		{ 0x80,0x00,0xa0,0x20 }, { 0xa0,0x80,0x20,0x00 },	/* ...1...0...0...1 */
		{ 0x20,0x28,0xa0,0xa8 }, { 0x08,0x88,0x00,0x80 },	/* ...1...0...1...0 */
		{ 0x80,0x00,0xa0,0x20 }, { 0x20,0x28,0xa0,0xa8 },	/* ...1...0...1...1 */
		{ 0x88,0x80,0xa8,0xa0 }, { 0x88,0x80,0xa8,0xa0 },	/* ...1...1...0...0 */
		{ 0x80,0x00,0xa0,0x20 }, { 0x08,0x88,0x00,0x80 },	/* ...1...1...0...1 */
		{ 0x80,0x00,0xa0,0x20 }, { 0x28,0x08,0x20,0x00 },	/* ...1...1...1...0 */
		{ 0x20,0x28,0xa0,0xa8 }, { 0xa0,0x80,0x20,0x00 }	/* ...1...1...1...1 */
	};

	sega_decode(convtable);
}

static INT32 futspyInit()
{
	futspy_sprite = 1;
	no_flip = 1;

	INT32 nRet = DrvInit();

	if (nRet == 0) {
		futspy_decode();
	}

	return nRet;
}

struct BurnDriver BurnDrvFutspy = {
	"futspy", NULL, NULL, "zaxxon", "1984",
	"Future Spy\0", NULL, "Sega", "Zaxxon",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_SEGA_MISC, GBF_VERSHOOT, 0,
	NULL, futspyRomInfo, futspyRomName, zaxxonSampleInfo, zaxxonSampleName, FutspyInputInfo, FutspyDIPInfo,
	futspyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Razzmatazz

static struct BurnRomInfo razmatazRomDesc[] = {
	{ "u27",		0x2000, 0x254f350f, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "u28",		0x2000, 0x3a1eaa99, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "u29",		0x2000, 0x0ee67e78, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "1921.u68",		0x0800, 0x77f8ff5a, 2 | BRF_GRA },           //  3 Characters
	{ "1922.u69",		0x0800, 0xcf63621e, 2 | BRF_GRA },           //  4

	{ "1934.u113",		0x2000, 0x39bb679c, 3 | BRF_GRA },           //  5 Background Tiles
	{ "1933.u112",		0x2000, 0x1022185e, 3 | BRF_GRA },           //  6
	{ "1932.u111",		0x2000, 0xc7a715eb, 3 | BRF_GRA },           //  7

	{ "1925.u77",		0x2000, 0xa7965437, 4 | BRF_GRA },           //  8 Sprites
	{ "1926.u78",		0x2000, 0x9a3af434, 4 | BRF_GRA },           //  9
	{ "1927.u79",		0x2000, 0x0323de2b, 4 | BRF_GRA },           // 10

	{ "1929.u91",		0x2000, 0x55c7c757, 6 | BRF_GRA },           // 11 Tilemaps
	{ "1928.u90",		0x2000, 0xe58b155b, 6 | BRF_GRA },           // 12
	{ "1931.u93",		0x2000, 0x55fe0f82, 6 | BRF_GRA },           // 13
	{ "1930.u92",		0x2000, 0xf355f105, 6 | BRF_GRA },           // 14

	{ "clr.u98",		0x0100, 0x0fd671af, 5 | BRF_GRA },           // 15 Color Proms
	{ "clr.u72",		0x0100, 0x03233bc5, 5 | BRF_GRA },           // 16

	{ "1924.u51",		0x0800, 0xa75e0011, 7 | BRF_PRG | BRF_ESS }, // 17 Universal Sound Board Code
	{ "1923.u50",		0x0800, 0x59994a51, 7 | BRF_PRG | BRF_ESS }, // 18
};

STD_ROM_PICK(razmataz)
STD_ROM_FN(razmataz)

static void nprinces_decode()
{
	static const UINT8 convtable[32][4] =
	{
		/*       opcode                   data                     address      */
		/*  A    B    C    D         A    B    C    D                           */
		{ 0x08,0x88,0x00,0x80 }, { 0xa0,0x20,0x80,0x00 },	/* ...0...0...0...0 */
		{ 0xa8,0xa0,0x28,0x20 }, { 0x88,0xa8,0x80,0xa0 },	/* ...0...0...0...1 */
		{ 0x88,0x80,0x08,0x00 }, { 0x28,0x08,0xa8,0x88 },	/* ...0...0...1...0 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0x28,0x08,0xa8,0x88 },	/* ...0...0...1...1 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0xa0,0x20,0x80,0x00 },	/* ...0...1...0...0 */
		{ 0xa8,0xa0,0x28,0x20 }, { 0xa8,0xa0,0x28,0x20 },	/* ...0...1...0...1 */
		{ 0x88,0x80,0x08,0x00 }, { 0x88,0xa8,0x80,0xa0 },	/* ...0...1...1...0 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0x88,0xa8,0x80,0xa0 },	/* ...0...1...1...1 */
		{ 0xa0,0x20,0x80,0x00 }, { 0xa0,0x20,0x80,0x00 },	/* ...1...0...0...0 */
		{ 0x08,0x88,0x00,0x80 }, { 0x28,0x08,0xa8,0x88 },	/* ...1...0...0...1 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0x88,0x80,0x08,0x00 },	/* ...1...0...1...0 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0x28,0x08,0xa8,0x88 },	/* ...1...0...1...1 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0x88,0xa8,0x80,0xa0 },	/* ...1...1...0...0 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0x88,0xa8,0x80,0xa0 },	/* ...1...1...0...1 */
		{ 0x88,0x80,0x08,0x00 }, { 0x88,0x80,0x08,0x00 },	/* ...1...1...1...0 */
		{ 0x08,0x88,0x00,0x80 }, { 0x28,0x08,0xa8,0x88 }	/* ...1...1...1...1 */
	};


	sega_decode(convtable);
}

static INT32 razmatazInit()
{
	hardware_type = 1;
	no_flip = 1;

	INT32 nRet = DrvInit();

	if (nRet == 0) {
		nprinces_decode();
	}

	return nRet;
}

struct BurnDriver BurnDrvRazmataz = {
	"razmataz", NULL, NULL, NULL, "1983",
	"Razzmatazz\0", NULL, "Sega", "Zaxxon",
	NULL, NULL, NULL, NULL,
	0 | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, razmatazRomInfo, razmatazRomName, NULL, NULL, ZaxxonInputInfo, ZaxxonDIPInfo, //RazmatazInputInfo, RazmatazDIPInfo,
	razmatazInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Ixion (prototype)

static struct BurnRomInfo ixionRomDesc[] = {
	{ "1937d.u27",		0x2000, 0xf447aac5, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "1938b.u28",		0x2000, 0x17f48640, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "1955b.u29",		0x1000, 0x78636ec6, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "1939a.u68",		0x0800, 0xc717ddc7, 1 | BRF_GRA },           //  3 Characters
	{ "1940a.u69",		0x0800, 0xec4bb3ad, 1 | BRF_GRA },           //  4

	{ "1952a.u113",		0x2000, 0xffb9b03d, 2 | BRF_GRA },           //  5 Background Tiles
	{ "1951a.u112",		0x2000, 0xdb743f1b, 2 | BRF_GRA },           //  6
	{ "1950a.u111",		0x2000, 0xc2de178a, 2 | BRF_GRA },           //  7

	{ "1945a.u77",		0x2000, 0x3a3fbfe7, 3 | BRF_GRA },           //  8 Sprites
	{ "1946a.u78",		0x2000, 0xf2cb1b53, 3 | BRF_GRA },           //  9
	{ "1947a.u79",		0x2000, 0xd2421e92, 3 | BRF_GRA },           // 10

	{ "1948a.u91",		0x2000, 0x7a7fcbbe, 4 | BRF_GRA },           // 11 Tilemaps
	{ "1953a.u90",		0x2000, 0x6b626ea7, 4 | BRF_GRA },           // 12
	{ "1949a.u93",		0x2000, 0xe7722d09, 4 | BRF_GRA },           // 13
	{ "1954a.u92",		0x2000, 0xa970f5ff, 4 | BRF_GRA },           // 14

	{ "1942a.u98",		0x0100, 0x3a8e6f74, 5 | BRF_GRA },           // 15 proms
	{ "1941a.u72",		0x0100, 0xa5d0d97e, 5 | BRF_GRA },           // 16

	{ "1944a.u51",		0x0800, 0x88215098, 7 | BRF_PRG | BRF_ESS }, // 17 Univesal Sound Board Code
	{ "1943a.u50",		0x0800, 0x77e5a1f0, 7 | BRF_PRG | BRF_ESS }, // 18
};

STD_ROM_PICK(ixion)
STD_ROM_FN(ixion)

static INT32 ixionInit()
{
	hardware_type = 1;

	INT32 nRet = DrvInit();

	if (nRet == 0) {
		szaxxon_decode();
	}

	return DrvInit();
}

struct BurnDriver BurnDrvIxion = {
	"ixion", NULL, NULL, NULL, "1983",
	"Ixion (prototype)\0", NULL, "Sega", "Zaxxon",
	NULL, NULL, NULL, NULL,
	0 | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, ixionRomInfo, ixionRomName, NULL, NULL, ZaxxonInputInfo, ZaxxonDIPInfo, //RazmatazInputInfo, RazmatazDIPInfo,
	ixionInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Congo Bongo

static struct BurnRomInfo congoRomDesc[] = {
	{ "congo_rev_c_rom1.u21",	0x2000, 0x09355b5b, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "congo_rev_c_rom2a.u22",	0x2000, 0x1c5e30ae, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "congo_rev_c_rom3.u23",	0x2000, 0x5ee1132c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "congo_rev_c_rom4.u24",	0x2000, 0x5332b9bf, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "tip_top_rom_5.u76",		0x1000, 0x7bf6ba2b, 1 | BRF_GRA },           //  4 Characters

	{ "tip_top_rom_8.u93",		0x2000, 0xdb99a619, 2 | BRF_GRA },           //  5 Background Tiles
	{ "tip_top_rom_9.u94",		0x2000, 0x93e2309e, 2 | BRF_GRA },           //  6
	{ "tip_top_rom_10.u95",		0x2000, 0xf27a9407, 2 | BRF_GRA },           //  7

	{ "tip_top_rom_12.u78",		0x2000, 0x15e3377a, 3 | BRF_GRA },           //  8 Sprites
	{ "tip_top_rom_13.u79",		0x2000, 0x1d1321c8, 3 | BRF_GRA },           //  9
	{ "tip_top_rom_11.u77",		0x2000, 0x73e2709f, 3 | BRF_GRA },           // 10
	{ "tip_top_rom_14.u104",	0x2000, 0xbf9169fe, 3 | BRF_GRA },           // 11
	{ "tip_top_rom_16.u106",	0x2000, 0xcb6d5775, 3 | BRF_GRA },           // 12
	{ "tip_top_rom_15.u105",	0x2000, 0x7b15a7a4, 3 | BRF_GRA },           // 13

	{ "tip_top_rom_6.u57",		0x2000, 0xd637f02b, 4 | BRF_GRA },           // 14 Tilemaps
	{ "tip_top_rom_7.u58",		0x2000, 0x80927943, 4 | BRF_GRA },           // 15

	{ "mr019.u87",				0x0100, 0xb788d8ae, 5 | BRF_GRA },           // 16 Color Proms

	{ "tip_top_rom_17.u19",		0x2000, 0x5024e673, 6 | BRF_PRG | BRF_ESS }, // 17 Sound Z80 Code
};

STD_ROM_PICK(congo)
STD_ROM_FN(congo)

struct BurnDriver BurnDrvCongo = {
	"congo", NULL, NULL, "congo", "1983",
	"Congo Bongo\0", NULL, "Sega", "Zaxxon",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_SEGA_MISC, GBF_PLATFORM, 0,
	NULL, congoRomInfo, congoRomName, congoSampleInfo, congoSampleName, CongoBongoInputInfo, CongoBongoDIPInfo,
	CongoInit, DrvExit, CongoFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Congo Bongo (Rev C, 3 board stack)

static struct BurnRomInfo congoaRomDesc[] = {
	{ "congo_rev_c_rom1.u35",	0x2000, 0x09355b5b, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "congo_rev_c_rom2a.u34",	0x2000, 0x1c5e30ae, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "congo_rev_c_rom3.u33",	0x2000, 0x5ee1132c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "congo_rev_c_rom4.u32",	0x2000, 0x5332b9bf, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "tip_top_rom_5.u76",		0x1000, 0x7bf6ba2b, 1 | BRF_GRA },           //  4 Characters

	{ "tip_top_rom_8.u93",		0x2000, 0xdb99a619, 2 | BRF_GRA },           //  5 Background Tiles
	{ "tip_top_rom_9.u94",		0x2000, 0x93e2309e, 2 | BRF_GRA },           //  6
	{ "tip_top_rom_10.u95",		0x2000, 0xf27a9407, 2 | BRF_GRA },           //  7

	{ "tip_top_rom_12.u78",		0x2000, 0x15e3377a, 3 | BRF_GRA },           //  8 Sprites
	{ "tip_top_rom_13.u79",		0x2000, 0x1d1321c8, 3 | BRF_GRA },           //  9
	{ "tip_top_rom_11.u77",		0x2000, 0x73e2709f, 3 | BRF_GRA },           // 10
	{ "tip_top_rom_14.u104",	0x2000, 0xbf9169fe, 3 | BRF_GRA },           // 11
	{ "tip_top_rom_16.u106",	0x2000, 0xcb6d5775, 3 | BRF_GRA },           // 12
	{ "tip_top_rom_15.u105",	0x2000, 0x7b15a7a4, 3 | BRF_GRA },           // 13

	{ "tip_top_rom_6.u57",		0x2000, 0xd637f02b, 4 | BRF_GRA },           // 14 Tilemaps
	{ "tip_top_rom_7.u58",		0x2000, 0x80927943, 4 | BRF_GRA },           // 15

	{ "mr018.u68",			0x0200, 0x56b9f1ba, 5 | BRF_GRA },           // 16 Color Proms

	{ "tip_top_rom_17.u11",		0x2000, 0x5024e673, 6 | BRF_PRG | BRF_ESS }, // 17 Sound Z80 Code
};

STD_ROM_PICK(congoa)
STD_ROM_FN(congoa)

struct BurnDriver BurnDrvCongoa = {
	"congoa", "congo", NULL, "congo", "1983",
	"Congo Bongo (Rev C, 3 board stack)\0", NULL, "Sega", "Zaxxon",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_SEGA_MISC, GBF_PLATFORM, 0,
	NULL, congoaRomInfo, congoaRomName, congoSampleInfo, congoSampleName, CongoBongoInputInfo, CongoBongoDIPInfo,
	CongoInit, DrvExit, CongoFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Tip Top

static struct BurnRomInfo tiptopRomDesc[] = {
	{ "tiptop1.u35",	0x2000, 0xe19dc77b, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "tiptop2.u34",	0x2000, 0x3fcd3b6e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tiptop3.u33",	0x2000, 0x1c94250b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "tiptop4.u32",	0x2000, 0x577b501b, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "tip_top_rom_5.u76",		0x1000, 0x7bf6ba2b, 1 | BRF_GRA },           //  4 Characters

	{ "tip_top_rom_8.u93",		0x2000, 0xdb99a619, 2 | BRF_GRA },           //  5 Background Tiles
	{ "tip_top_rom_9.u94",		0x2000, 0x93e2309e, 2 | BRF_GRA },           //  6
	{ "tip_top_rom_10.u95",		0x2000, 0xf27a9407, 2 | BRF_GRA },           //  7

	{ "tip_top_rom_12.u78",		0x2000, 0x15e3377a, 3 | BRF_GRA },           //  8 Sprites
	{ "tip_top_rom_13.u79",		0x2000, 0x1d1321c8, 3 | BRF_GRA },           //  9
	{ "tip_top_rom_11.u77",		0x2000, 0x73e2709f, 3 | BRF_GRA },           // 10
	{ "tip_top_rom_14.u104",	0x2000, 0xbf9169fe, 3 | BRF_GRA },           // 11
	{ "tip_top_rom_16.u106",	0x2000, 0xcb6d5775, 3 | BRF_GRA },           // 12
	{ "tip_top_rom_15.u105",	0x2000, 0x7b15a7a4, 3 | BRF_GRA },           // 13

	{ "tip_top_rom_6.u57",		0x2000, 0xd637f02b, 4 | BRF_GRA },           // 14 Tilemaps
	{ "tip_top_rom_7.u58",		0x2000, 0x80927943, 4 | BRF_GRA },           // 15

	{ "mr018.u68",			0x0200, 0x56b9f1ba, 5 | BRF_GRA },           // 16 Color Proms

	{ "tip_top_rom_17.u11",		0x2000, 0x5024e673, 6 | BRF_PRG | BRF_ESS }, // 17 Sound Z80 Code
};

STD_ROM_PICK(tiptop)
STD_ROM_FN(tiptop)

struct BurnDriver BurnDrvTiptop = {
	"tiptop", "congo", NULL, "congo", "1983",
	"Tip Top\0", NULL, "Sega", "Zaxxon",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_SEGA_MISC, GBF_PLATFORM, 0,
	NULL, tiptopRomInfo, tiptopRomName, congoSampleInfo, congoSampleName, CongoBongoInputInfo, CongoBongoDIPInfo,
	CongoInit, DrvExit, CongoFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};
