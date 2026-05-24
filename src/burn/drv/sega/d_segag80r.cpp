// FB Neo Sega G80 Raster driver module
// Based on MAME driver by Aaron Giles

// not bugs:
// 	pignewta no title - normal

// to do:
//  hook up sega speech to segag80v

#include "tiles_generic.h"
#include "z80_intf.h"
#include "i8039.h" 			// i8035
#include "samples.h"
#include "8255ppi.h"
#include "bitswap.h"
#include "resnet.h"
#include "mcs48.h" 			// monsterb - upd7751 (i8048)
#include "tms36xx.h"		// monsterb
#include "dac.h"			// monsterb
#include "sn76496.h"		// sinbadm
#include "usb_snd.h"		// pignewt
#include "sega_speech.h"	// astrob
#include "stream.h"			// 005 snd
#include "dtimer.h"         // 005 snd

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvZ80OPS;
static UINT8 *DrvSndROM;
static UINT8 *DrvGfxExp;
static UINT8 *DrvGfxROM[2];
static UINT8 *DrvSampleROM;
static UINT8 *DrvSndPROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvSndRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT16 video_control;
static UINT8 vblank_latch;
static UINT8 video_flip;
static UINT8 soundlatch;
static UINT8 sound_state[2];
static UINT8 sound_rate;
// 005
static UINT16 sound_addr;
static UINT8 sound_data;
static UINT8 square_state;
static UINT8 square_count;
static INT32 snd_timer;
static INT32 snd_timer_period;

static UINT8 pignewt_bg_color_offset;
static UINT8 spaceod_bg_control;
static UINT16 bg_scrollx;
static UINT16 bg_scrolly;
static UINT8 spaceod_bg_detect;
static UINT8 spaceod_fixed_color;
static UINT8 bg_enable;
static UINT8 bg_char_bank;

enum { SOUND_ASTROB, SOUND_USB, SOUND_Z80, SOUND_MONSTERB, SOUND_005, SOUND_SPACEOD };
static INT32 sound_type = 0;

static UINT16 monsterb_sound_addr;
static UINT8 upd7751_command;
static UINT8 upd7751_busy;

static Stream stream_005;

static UINT8 (*sega_decrypt)(UINT16 offset, UINT8 data);
static void (*write_port_cb)(UINT8 offset, UINT8 data) = NULL;
static UINT8 (*read_port_cb)(UINT8 offset) = NULL;
static INT32 ext_palette_bit = 8;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoyS[1]; // special service
static UINT8 DrvDips[3];
static UINT8 DrvInputs[5];
static UINT8 DrvReset;

static INT32 nCyclesExtra;
static HoldCoin<2> hold_coin;
static HoldCoin<2> hold_start; // 005, spaceod, monsterb2 check start-button every 48 frames!
static INT32 has_startlatch = 0;

static struct BurnInputInfo AstrobInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 start"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 4,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 start"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 0,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoyS + 0,	"diag"	    },
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Astrob)

static struct BurnInputInfo Sega005InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 4,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 4,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 0,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoyS + 0,	"diag"	    },
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Sega005)

static struct BurnInputInfo SpaceodInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy3 + 3,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy3 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 4,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 6,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 0,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoyS + 0,	"diag"	    },
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Spaceod)

static struct BurnInputInfo MonsterbInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 4,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 4,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 0,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoyS + 0,	"diag"	    },
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Monsterb)

static struct BurnInputInfo PignewtaInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy3 + 7,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy3 + 5,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy3 + 6,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 4,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 start"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 0,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoyS + 0,	"diag"	    },
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Pignewta)

static struct BurnInputInfo SindbadmInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 4,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 4,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 0,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoyS + 0,	"diag"	    },
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Sindbadm)

#define default_coinage_dips									\
	{0   , 0xfe, 0   ,   16, "Coin A"					},		\
	{0x01, 0x01, 0x0f, 0x00, "4 Coins 1 Credits"		},		\
	{0x01, 0x01, 0x0f, 0x01, "3 Coins 1 Credits"		},		\
	{0x01, 0x01, 0x0f, 0x02, "2 Coins 1 Credits"		},		\
	{0x01, 0x01, 0x0f, 0x09, "2 Coins/1 Credit 5/3 6/4"	},		\
	{0x01, 0x01, 0x0f, 0x0a, "2 Coins/1 Credit 4/3"		},		\
	{0x01, 0x01, 0x0f, 0x03, "1 Coin  1 Credits"		},		\
	{0x01, 0x01, 0x0f, 0x0b, "1 Coin/1 Credit 5/6"		},		\
	{0x01, 0x01, 0x0f, 0x0c, "1 Coin/1 Credit 4/5"		},		\
	{0x01, 0x01, 0x0f, 0x0d, "1 Coin/1 Credit 2/3"		},		\
	{0x01, 0x01, 0x0f, 0x04, "1 Coin  2 Credits"		},		\
	{0x01, 0x01, 0x0f, 0x0e, "1 Coin/2 Credits 5/11"	},		\
	{0x01, 0x01, 0x0f, 0x0f, "1 Coin/2 Credits 4/9"		},		\
	{0x01, 0x01, 0x0f, 0x05, "1 Coin  3 Credits"		},		\
	{0x01, 0x01, 0x0f, 0x06, "1 Coin  4 Credits"		},		\
	{0x01, 0x01, 0x0f, 0x07, "1 Coin  5 Credits"		},		\
	{0x01, 0x01, 0x0f, 0x08, "1 Coin  6 Credits"		},		\
																\
	{0   , 0xfe, 0   ,   16, "Coin B"					},		\
	{0x01, 0x01, 0xf0, 0x00, "4 Coins 1 Credits"		},		\
	{0x01, 0x01, 0xf0, 0x10, "3 Coins 1 Credits"		},		\
	{0x01, 0x01, 0xf0, 0x20, "2 Coins 1 Credits"		},		\
	{0x01, 0x01, 0xf0, 0x90, "2 Coins/1 Credit 5/3 6/4"	},		\
	{0x01, 0x01, 0xf0, 0xa0, "2 Coins/1 Credit 4/3"		},		\
	{0x01, 0x01, 0xf0, 0x30, "1 Coin  1 Credits"		},		\
	{0x01, 0x01, 0xf0, 0xb0, "1 Coin/1 Credit 5/6"		},		\
	{0x01, 0x01, 0xf0, 0xc0, "1 Coin/1 Credit 4/5"		},		\
	{0x01, 0x01, 0xf0, 0xd0, "1 Coin/1 Credit 2/3"		},		\
	{0x01, 0x01, 0xf0, 0x40, "1 Coin  2 Credits"		},		\
	{0x01, 0x01, 0xf0, 0xe0, "1 Coin/2 Credits 5/11"	},		\
	{0x01, 0x01, 0xf0, 0xf0, "1 Coin/2 Credits 4/9"		},		\
	{0x01, 0x01, 0xf0, 0x50, "1 Coin  3 Credits"		},		\
	{0x01, 0x01, 0xf0, 0x60, "1 Coin  4 Credits"		},		\
	{0x01, 0x01, 0xf0, 0x70, "1 Coin  5 Credits"		},		\
	{0x01, 0x01, 0xf0, 0x80, "1 Coin  6 Credits"		},		\

static struct BurnDIPInfo AstrobDIPList[]=
{
	DIP_OFFSET(0x0f)
	{0x00, 0xff, 0xff, 0xf5, NULL						},
	{0x01, 0xff, 0xff, 0x33, NULL						},
	{0x02, 0xff, 0xff, 0x00, NULL						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x00, 0x01, 0x03, 0x00, "2"						},
	{0x00, 0x01, 0x03, 0x01, "3"						},
	{0x00, 0x01, 0x03, 0x02, "4"						},
	{0x00, 0x01, 0x03, 0x03, "5"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x00, 0x01, 0x04, 0x04, "Upright"					},
	{0x00, 0x01, 0x04, 0x00, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Demo Speech"				},
	{0x00, 0x01, 0x08, 0x08, "Off"						},
	{0x00, 0x01, 0x08, 0x00, "On"						},

	default_coinage_dips
};

STDDIPINFO(Astrob)

// German & French versions are missing "Demo Speech" dip
static struct BurnDIPInfo AstrobfgDIPList[]=
{
	DIP_OFFSET(0x0f)
	{0x00, 0xff, 0xff, 0xf5, NULL						},
	{0x01, 0xff, 0xff, 0x33, NULL						},
	{0x02, 0xff, 0xff, 0x00, NULL						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x00, 0x01, 0x03, 0x00, "2"						},
	{0x00, 0x01, 0x03, 0x01, "3"						},
	{0x00, 0x01, 0x03, 0x02, "4"						},
	{0x00, 0x01, 0x03, 0x03, "5"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x00, 0x01, 0x04, 0x04, "Upright"					},
	{0x00, 0x01, 0x04, 0x00, "Cocktail"					},

	default_coinage_dips
};

STDDIPINFO(Astrobfg)

static struct BurnDIPInfo Astrob2DIPList[]=
{
	DIP_OFFSET(0x0f)
	{0x00, 0xff, 0xff, 0xf5, NULL						},
	{0x01, 0xff, 0xff, 0x33, NULL						},
	{0x02, 0xff, 0xff, 0x00, NULL						},

	{0   , 0xfe, 0   ,    2, "Lives"					},
	{0x00, 0x01, 0x03, 0x00, "2"						},
	{0x00, 0x01, 0x03, 0x01, "3"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x00, 0x01, 0x04, 0x04, "Upright"					},
	{0x00, 0x01, 0x04, 0x00, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Demo Speech"				},
	{0x00, 0x01, 0x08, 0x08, "Off"						},
	{0x00, 0x01, 0x08, 0x00, "On"						},

	default_coinage_dips
};

STDDIPINFO(Astrob2)

static struct BurnDIPInfo Sega005DIPList[]=
{
	DIP_OFFSET(0x11)
	{0x00, 0xff, 0xff, 0xfc, NULL						},
	{0x01, 0xff, 0xff, 0x33, NULL						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x00, 0x01, 0x03, 0x00, "3"						},
	{0x00, 0x01, 0x03, 0x02, "4"						},
	{0x00, 0x01, 0x03, 0x01, "5"						},
	{0x00, 0x01, 0x03, 0x03, "6"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x00, 0x01, 0x04, 0x04, "Upright"					},
	{0x00, 0x01, 0x04, 0x00, "Cocktail"					},

	default_coinage_dips
};

STDDIPINFO(Sega005)

static struct BurnDIPInfo SpaceodDIPList[]=
{
	DIP_OFFSET(0x13)
	{0x00, 0xff, 0xff, 0xe4, NULL						},
	{0x01, 0xff, 0xff, 0x33, NULL						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x00, 0x01, 0x03, 0x00, "3"						},
	{0x00, 0x01, 0x03, 0x02, "4"						},
	{0x00, 0x01, 0x03, 0x01, "5"						},
	{0x00, 0x01, 0x03, 0x03, "6"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x00, 0x01, 0x04, 0x04, "Upright"					},
	{0x00, 0x01, 0x04, 0x00, "Cocktail"					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"				},
	{0x00, 0x01, 0x18, 0x00, "20000"					},
	{0x00, 0x01, 0x18, 0x10, "40000"					},
	{0x00, 0x01, 0x18, 0x08, "60000"					},
	{0x00, 0x01, 0x18, 0x18, "80000"					},

	{0   , 0xfe, 0   ,    2, "Infinite Lives (Cheat)"	},
	{0x00, 0x01, 0x80, 0x80, "Off"						},
	{0x00, 0x01, 0x80, 0x00, "On"						},

	default_coinage_dips
};

STDDIPINFO(Spaceod)

static struct BurnDIPInfo MonsterbDIPList[]=
{
	DIP_OFFSET(0x11)
	{0x00, 0xff, 0xff, 0xd4, NULL						},
	{0x01, 0xff, 0xff, 0x33, NULL						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x00, 0x01, 0x03, 0x00, "3"						},
	{0x00, 0x01, 0x03, 0x02, "4"						},
	{0x00, 0x01, 0x03, 0x01, "5"						},
	{0x00, 0x01, 0x03, 0x03, "6"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x00, 0x01, 0x04, 0x04, "Upright"					},
	{0x00, 0x01, 0x04, 0x00, "Cocktail"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x00, 0x01, 0x18, 0x00, "Easy"						},
	{0x00, 0x01, 0x18, 0x10, "Medium"					},
	{0x00, 0x01, 0x18, 0x08, "Hard"						},
	{0x00, 0x01, 0x18, 0x18, "Hardest"					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"				},
	{0x00, 0x01, 0x60, 0x20, "10000"					},
	{0x00, 0x01, 0x60, 0x40, "20000"					},
	{0x00, 0x01, 0x60, 0x60, "40000"					},
	{0x00, 0x01, 0x60, 0x00, "None"						},

	{0   , 0xfe, 0   ,    2, "Infinite Lives (Cheat)"	},
	{0x00, 0x01, 0x80, 0x80, "Off"						},
	{0x00, 0x01, 0x80, 0x00, "On"						},

	default_coinage_dips
};

STDDIPINFO(Monsterb)

static struct BurnDIPInfo PignewtDIPList[]=
{
	DIP_OFFSET(0x11)
	{0x00, 0xff, 0xff, 0x02, NULL						},
	{0x01, 0xff, 0xff, 0x33, NULL						},
	{0x02, 0xff, 0xff, 0x00, NULL						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x00, 0x01, 0x01, 0x00, "Upright"					},
	{0x00, 0x01, 0x01, 0x01, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x00, 0x01, 0x02, 0x00, "Off"						},
	{0x00, 0x01, 0x02, 0x02, "On"						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x00, 0x01, 0x0c, 0x00, "3"						},
	{0x00, 0x01, 0x0c, 0x08, "4"						},
	{0x00, 0x01, 0x0c, 0x04, "5"						},
	{0x00, 0x01, 0x0c, 0x0c, "6"						},

	default_coinage_dips
};

STDDIPINFO(Pignewt)

static struct BurnDIPInfo PignewtaDIPList[]=
{
	DIP_OFFSET(0x0c)
	{0x00, 0xff, 0xff, 0x02, NULL						},
	{0x01, 0xff, 0xff, 0x33, NULL						},
	{0x02, 0xff, 0xff, 0x00, NULL						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x00, 0x01, 0x01, 0x00, "Upright"					},
	{0x00, 0x01, 0x01, 0x01, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x00, 0x01, 0x02, 0x00, "Off"						},
	{0x00, 0x01, 0x02, 0x02, "On"						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x00, 0x01, 0x0c, 0x00, "3"						},
	{0x00, 0x01, 0x0c, 0x08, "4"						},
	{0x00, 0x01, 0x0c, 0x04, "5"						},
	{0x00, 0x01, 0x0c, 0x0c, "6"						},

	default_coinage_dips
};

STDDIPINFO(Pignewta)

static struct BurnDIPInfo SindbadmDIPList[]=
{
	DIP_OFFSET(0x11)
	{0x00, 0xff, 0xff, 0xfc, NULL						},
	{0x01, 0xff, 0xff, 0xff, NULL						},
	{0x02, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x00, 0x01, 0x03, 0x00, "3"						},
	{0x00, 0x01, 0x03, 0x02, "4"						},
	{0x00, 0x01, 0x03, 0x01, "5"						},
	{0x00, 0x01, 0x03, 0x03, "6"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x00, 0x01, 0x04, 0x04, "Upright"					},
	{0x00, 0x01, 0x04, 0x00, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Infinite Lives (Cheat)"	},
	{0x00, 0x01, 0x80, 0x80, "Off"						},
	{0x00, 0x01, 0x80, 0x00, "On"						},

	{0   , 0xfe, 0   ,   16, "Coin A"					},
	{0x01, 0x01, 0x0f, 0x07, "4 Coins 1 Credits"		},
	{0x01, 0x01, 0x0f, 0x08, "3 Coins 1 Credits"		},
	{0x01, 0x01, 0x0f, 0x09, "2 Coins 1 Credits"		},
	{0x01, 0x01, 0x0f, 0x05, "2 Coins/1 Credit 5/3"		},
	{0x01, 0x01, 0x0f, 0x04, "2 Coins/1 Credit 4/3"		},
	{0x01, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"		},
	{0x01, 0x01, 0x0f, 0x03, "1 Coin/1 Credit 5/6"		},
	{0x01, 0x01, 0x0f, 0x02, "1 Coin/1 Credit 4/5"		},
	{0x01, 0x01, 0x0f, 0x01, "1 Coin/1 Credit 2/3"		},
	{0x01, 0x01, 0x0f, 0x06, "2 Coins 3 Credits"		},
	{0x01, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"		},
	{0x01, 0x01, 0x0f, 0x00, "1 Coin/2 Credits 5/11"	},
	{0x01, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"		},
	{0x01, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"		},
	{0x01, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"		},
	{0x01, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,   16, "Coin B"					},
	{0x01, 0x01, 0xf0, 0x70, "4 Coins 1 Credits"		},
	{0x01, 0x01, 0xf0, 0x80, "3 Coins 1 Credits"		},
	{0x01, 0x01, 0xf0, 0x90, "2 Coins 1 Credits"		},
	{0x01, 0x01, 0xf0, 0x50, "2 Coins/1 Credit 5/3"		},
	{0x01, 0x01, 0xf0, 0x40, "2 Coins/1 Credit 4/3"		},
	{0x01, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x01, 0x01, 0xf0, 0x30, "1 Coin/1 Credit 5/6"		},
	{0x01, 0x01, 0xf0, 0x20, "1 Coin/1 Credit 4/5"		},
	{0x01, 0x01, 0xf0, 0x10, "1 Coin/1 Credit 2/3"		},
	{0x01, 0x01, 0xf0, 0x60, "2 Coins 3 Credits"		},
	{0x01, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"		},
	{0x01, 0x01, 0xf0, 0x00, "1 Coin/2 Credits 5/11"	},
	{0x01, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"		},
	{0x01, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"		},
	{0x01, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"		},
	{0x01, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"		},
};

STDDIPINFO(Sindbadm)

static void sync_sound()
{
	switch (sound_type) {
		case SOUND_ASTROB:
			break;
		case SOUND_USB: {
			I8039Open(1);
			INT32 cyc = ((ZetTotalCycles() * 400) / 3867) - I8039TotalCycles();
			if (cyc > 0) {
				I8039Run(cyc);
			}
			I8039Close();
			break;
		}
		case SOUND_Z80:
			break;
		case SOUND_MONSTERB: {
			mcs48Open(0);
			INT32 cyc = (((INT64)ZetTotalCycles() * (6000000 / 15)) / 3867000) - mcs48TotalCycles();
			if (cyc > 0) {
				mcs48Run(cyc);
			}
			mcs48Close();
			break;
		}
	}
}

static inline UINT16 decrypt_offset(UINT16 offset)
{
	UINT16 pc = ZetGetPrevPC(-1);

	if (pc == 0xffff || ZetReadByte(pc) != 0x32) return offset;

	return (offset & 0xff00) | (*sega_decrypt)(pc, ZetReadByte(pc + 1));
}

static inline void decode_plane(INT32 offset, UINT8 data, INT32 plane)
{
	UINT8 *dst = DrvGfxExp + ((offset & 0x7ff) * 8);

	for (INT32 i = 0; i < 8; i++)
	{
		dst[7-i] = (dst[7-i] & (2 >> plane)) | (((data >> i) & 1) << plane);
	}
}

static void __fastcall segag80r_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf800) == 0xc800) {
		DrvZ80RAM[decrypt_offset(address & 0x7ff)] = data;
		return;
	}

	if ((address & 0xf000) == 0xe000) {
		DrvVidRAM[decrypt_offset(address & 0xfff)] = data;
		if (address & 0x800) decode_plane(address, data, 0);
		return;
	}

	if ((address & 0xf000) == 0xd000 && (sound_type == SOUND_USB)) {
		usb_sound_prgram_write(address & 0xfff, data);
		return;
	}

	if (address >= 0xf000) {
		UINT16 ori_address = address;
		address = decrypt_offset(address & 0x1fff);

		if (((address & 0xfc0) == 0x000) && (video_control & 0x02) && ext_palette_bit == 9) { // sinbad
			DrvPalRAM[0x40 + (address & 0x3f)] = data;
			return;
		}

		if (((address & 0xfc0) == 0x040) && (video_control & (1 << ext_palette_bit))) {
			DrvPalRAM[0x40 + (address & 0x3f)] = data;
			if (ext_palette_bit != 6) return; // monsterb falls through
		}

		if (video_control & 0x02) {
			DrvPalRAM[0x00 + (address & 0x3f)] = data;
			return;
		}

		DrvVidRAM[address & 0x1fff] = data;
		if (ori_address & 0x800) decode_plane(ori_address, data, 1);
		return;
	}
}

static UINT8 __fastcall segag80r_read(UINT16 address)
{
	if ((address & 0xf000) == 0xd000 && (sound_type == SOUND_USB)) {
		return usb_sound_prgram_read(address & 0xfff);
	}

	return 0;
}

static void __fastcall segag80r_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
	//	case 0x42: // sindbad
	//	case 0xbe:
	//	break; // nop

		case 0x43: // sindbad
		case 0xbf:
			video_control = data;
		break;

	//	case 0xf9:
	//	case 0xfd: // mirror
	//		// coin counter (ignore)
	//	break;
	}

	if (write_port_cb) {
		write_port_cb(port & 0xff, data);
		return;
	}
}

static UINT8 mangled_ports_read(INT32 offset)
{
	INT32 shift = offset & 3;
	UINT8 d7d6 = DrvInputs[0] >> shift;
	UINT8 d5d4 = DrvInputs[1] >> shift;
	UINT8 d3d2 = DrvInputs[2] >> shift;
	UINT8 d1d0 = DrvInputs[3] >> shift;

	return ((d7d6 << 7) & 0x80) | ((d7d6 << 2) & 0x40) | ((d5d4 << 5) & 0x20) | ((d5d4 << 0) & 0x10) |
		   ((d3d2 << 3) & 0x08) | ((d3d2 >> 2) & 0x04) | ((d1d0 << 1) & 0x02) | ((d1d0 >> 4) & 0x01);
}

static UINT8 __fastcall segag80r_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x42: // sindbad
		case 0xbe:
			return 0xff;

		case 0x43: // sindbad
		case 0xbf:
			return (vblank_latch << 0) | (video_flip << 1) | (video_control & 0x04) | 0xf8;
	}

	if (read_port_cb) {
		return read_port_cb(port & 0xff);
	}

	return 0;
}

static UINT8 standard_read_port(UINT8 port)
{
	switch (port)
	{
		case 0xf8:
		case 0xf9:
		case 0xfa:
		case 0xfb:
			return mangled_ports_read(port);

		case 0xfc:
			return DrvInputs[4];
	}

	return 0;
}

// i8243 port expander, an addon chip for mcs48 series mcu's
static UINT8 i8243_p2 = 0xf;
static UINT8 i8243_prog = 1;
static UINT8 i8243_opcode = 0;
static UINT8 i8243_output[4] = { 0, 0, 0, 0 };
static void (*i8243_write)(INT32 offset, UINT8 data) = NULL;

static void i8243_reset()
{
	i8243_p2 = 0xf;
	i8243_prog = 1;
	i8243_opcode = 0;
	memset(i8243_output, 0, sizeof(i8243_output));
}

static void i8243_scan()
{
	SCAN_VAR(i8243_p2);
	SCAN_VAR(i8243_prog);
	SCAN_VAR(i8243_opcode);
	SCAN_VAR(i8243_output);
}

static void i8243_prog_write(INT32 state)
{
	if (i8243_prog && !state)
	{
		i8243_opcode = i8243_p2;

		if ((i8243_opcode >> 2) == 0 /* OP_READ */)	{
			bprintf(0, _T("i8423: read opcode unimpl.\n"));
		} else {
			// ?
		}
	}
	else if (!i8243_prog && state)
	{
		switch (i8243_opcode >> 2)
		{
			case 0: // OP_READ
				break;
			case 1: //EXPANDER_OP_WRITE:
				i8243_output[i8243_opcode & 3] = i8243_p2 & 0x0f;
				if (i8243_write) i8243_write(i8243_opcode & 3, i8243_output[i8243_opcode & 3]);
				break;

			case 2: //EXPANDER_OP_OR:
				i8243_output[i8243_opcode & 3] |= i8243_p2 & 0x0f;
				if (i8243_write) i8243_write(i8243_opcode & 3, i8243_output[i8243_opcode & 3]);
				break;

			case 3: //EXPANDER_OP_AND:
				i8243_output[i8243_opcode & 3] &= i8243_p2 & 0x0f;
				if (i8243_write) i8243_write(i8243_opcode & 3, i8243_output[i8243_opcode & 3]);
				break;
		}
	}

	i8243_prog = state;
}

static void monsterb_sound_write(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case MCS48_P1:
			DACSignedWrite(0, data);
		return;

		case MCS48_P2:
			i8243_p2 = data & 0x0f;
			upd7751_busy = data >> 7;
		return;

		case MCS48_PROG:
			i8243_prog_write(data);
		return;
	}
}

static UINT8 monsterb_sound_read(UINT32 address)
{
	switch (address)
	{
		case MCS48_T1: return 0; // test?
		case MCS48_P2: return 0x80 | ((upd7751_command & 0x07) << 4);
		case MCS48_BUS: return DrvSampleROM[monsterb_sound_addr & 0x1fff];
	}

	return 0;
}

static void __fastcall sinbadm_sound_write(UINT16 address, UINT8 data)
{
	switch (address & ~0x1fff)
	{
		case 0xa000: // sn 0
		case 0xc000: // sn 1
			SN76496Write((address & 0x4000) >> 14, BITSWAP08(data,0,1,2,3,4,5,6,7));
		return;
	}
}

static UINT8 __fastcall sinbadm_sound_read(UINT16 address)
{
	switch (address & ~0x1fff)
	{
		case 0xe000:
			return soundlatch; // ppi acka_r
	}

	return 0;
}

static tilemap_callback( bg )
{
	if (video_flip) offs ^= 0x3ff;

	UINT8 code = DrvVidRAM[offs];

	TILE_SET_INFO(0, code, code >> 4, TILE_FLIPXY(video_flip ? 3 : 0));
}

static tilemap_callback( spaceod )
{
	INT32 code = DrvGfxROM[1][offs + 0x1000 * (spaceod_bg_control >> 6)];
	TILE_SET_INFO(1, code + ((spaceod_bg_control & 0x04) * 0x40), 0, 0);
}

static tilemap_scan( spaceod )
{
	return (row & 31) * 32 + (col & 31) + ((row >> 5) * 32*32) + ((col >> 5) * 32*32);
}

static tilemap_callback( monsterbg )
{
	INT32 code = DrvGfxROM[1][offs];

	TILE_SET_INFO(1, code + (bg_char_bank * 0x100), code >> 4, 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetReset(0);

	BurnSampleReset();

	switch (sound_type)
	{
		case SOUND_005:
		break;

		case SOUND_ASTROB:
			sega_speech_reset();
		break;

		case SOUND_USB:
			usb_sound_reset();
		break;

		case SOUND_Z80:
			ZetReset(1);
			SN76496Reset();
		break;

		case SOUND_MONSTERB:
			mcs48Open(0);
			mcs48Reset();
			DACReset();
			tms36xx_reset();
			i8243_reset();
			mcs48Close();
		break;
	}

	ppi8255_reset();

	vblank_latch = 0;
	video_control = 0;
	video_flip = 0;
	soundlatch = 0;
	memset (sound_state, 0, sizeof(sound_state));
	sound_rate = 0;

	sound_addr = 0;
	sound_data = 0;
	square_state = 0;
	square_count = 0;
	snd_timer = -1; // disabled @ reset
	snd_timer_period = 0; // set when enabled

	spaceod_bg_control = 0;
	spaceod_bg_detect = 0;
	spaceod_fixed_color = 0;
	pignewt_bg_color_offset = 0;
	bg_scrolly = 0;
	bg_scrollx = 0;
	bg_char_bank = 0;
	bg_enable = 1;

	monsterb_sound_addr = 0;
	upd7751_command = 0;
	upd7751_busy = 0;

	nCyclesExtra = 0;

	hold_coin.reset();
	hold_start.reset();

	HiscoreReset(HI_NOCOMPLIMENTWB | HI_NOCONFIRM);

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x010000;
	DrvZ80OPS		= Next; Next += 0x010000;
	DrvSndROM		= Next; Next += 0x002000;

	DrvGfxROM[0]	= Next; Next += 0x040000;
	DrvGfxROM[1]	= Next; Next += 0x010000;

	DrvSampleROM	= Next; Next += 0x004000;
	DrvSndPROM		= Next; Next += 0x000020;

	DrvPalette		= (UINT32*)Next; Next += BurnDrvGetPaletteEntries() * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x000800;
	DrvVidRAM		= Next; Next += 0x002000;
	DrvPalRAM		= Next; Next += 0x000080;
	DrvSndRAM		= Next; Next += 0x000800;
	DrvGfxExp		= Next; Next += 0x004000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvLoad()
{
	char* pRomName;
	struct BurnRomInfo ri;

	UINT8 *pLoad[8] = { 0, DrvZ80ROM, DrvSndROM, DrvSampleROM, DrvSndPROM, DrvGfxROM[0], DrvGfxROM[1], 0 };

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++)
	{
		BurnDrvGetRomInfo(&ri, i);

		if (ri.nType & 7)
		{
			//bprintf(0, _T("loading %x\n"), ri.nType);
			if (BurnLoadRom(pLoad[ri.nType & 7], i, 1)) return 1;
			pLoad[ri.nType & 7] += ri.nLen;
		}
	}

	memcpy (DrvZ80OPS, DrvZ80ROM, 0x10000);

	return 0;
}

static void DrvGfxDecode(INT32 length, INT32 bpp)
{
	INT32 Planes0[6] = { (length / 6) * 5 * 8, (length / 6) * 4 * 8, (length / 6) * 3 * 8, (length / 6) * 2 * 8, (length / 6) * 1 * 8, (length / 6) * 0 * 8 };
	INT32 Planes1[2] = { (length / 2) * 8, 0 };
	INT32 XOffs[8] = { STEP8(0,1) };
	INT32 YOffs[8] = { STEP8(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(length);
	if (tmp == NULL) {
		return;
	}

	memcpy (tmp, DrvGfxROM[0], length);

	GfxDecode(((length / bpp) * 8) / (8 * 8), bpp, 8, 8, (bpp == 6) ? Planes0 : Planes1, XOffs, YOffs, 0x040, tmp, DrvGfxROM[0]);

	BurnFree(tmp);
}

static void sega_decode(const UINT8 convtable[32][4])
{
	for (INT32 A = 0; A < 0x8000; A++)
	{
		INT32 xorval = 0;
		UINT8 src = DrvZ80ROM[A];
		INT32 row = (A & 1) + (((A >> 4) & 1) << 1) + (((A >> 8) & 1) << 2) + (((A >> 12) & 1) << 3);
		INT32 col = ((src >> 3) & 1) + (((src >> 5) & 1) << 1);

		if (src & 0x80)
		{
			col = 3 - col;
			xorval = 0xa8;
		}

		DrvZ80OPS[A] = (src & ~0xa8) | (convtable[2*row+0][col] ^ xorval);
		DrvZ80ROM[A] = (src & ~0xa8) | (convtable[2*row+1][col] ^ xorval);
	}
}

static INT32 DrvInit(UINT8 (*decrypt)(UINT16, UINT8), void (*pLoadCallback)(), UINT8 (*readport)(UINT8), void (*writeport)(UINT8,UINT8))
{
	BurnAllocMemIndex();

	{
		if (DrvLoad()) return 1;
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80OPS,		0x0000, 0xbfff, MAP_FETCHOP);
	ZetMapMemory(DrvZ80RAM,		0xc800, 0xcfff, MAP_ROM);
	ZetMapMemory(DrvVidRAM,		0xe000, 0xffff, MAP_ROM);
	ZetSetWriteHandler(segag80r_write);
	ZetSetReadHandler(segag80r_read);
	ZetSetOutHandler(segag80r_write_port);
	ZetSetInHandler(segag80r_read_port);
	ZetClose();

	ppi8255_init(1);

	switch (sound_type)
	{
		case SOUND_ASTROB:
		{
			sega_speech_init(DrvSndROM, DrvSampleROM);
		}
		break;

		case SOUND_USB:
		{
			usb_sound_init(I8039TotalCycles, 400000);
		}
		break;

		case SOUND_Z80:
		{
			ZetInit(1);
			ZetOpen(1);
			ZetMapMemory(DrvSndROM,		0x0000, 0x1fff, MAP_ROM);
			ZetMapMemory(DrvSndRAM,		0x8000, 0x87ff, MAP_RAM);
			ZetMapMemory(DrvSndRAM,		0x8800, 0x8fff, MAP_RAM);
			ZetMapMemory(DrvSndRAM,		0x9000, 0x97ff, MAP_RAM);
			ZetMapMemory(DrvSndRAM,		0x9800, 0x9fff, MAP_RAM);
			ZetSetWriteHandler(sinbadm_sound_write);
			ZetSetReadHandler(sinbadm_sound_read);
			ZetClose();

			SN76489Init(0, 4000000, 0);
			SN76489Init(1, 2000000, 1);
			SN76496SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);
			SN76496SetRoute(1, 1.00, BURN_SND_ROUTE_BOTH);
		}
		break;

		case SOUND_MONSTERB:
		{
			mcs48Init(0, 8048, DrvSndROM);
			mcs48Open(0);
			mcs48_set_read_port(monsterb_sound_read);
			mcs48_set_write_port(monsterb_sound_write);
			mcs48Close();

			DACInit(0, 0, 1, mcs48TotalCycles, 6000000/15);
			DACSetRoute(0, 0.25, BURN_SND_ROUTE_BOTH);

			double decays[6] = {0.5, 0.5, 0.5, 0.5, 0.5, 0.5};
			tms36xx_init(247, TMS3617, &decays[0], 0.5);
		}
		break;
	}

	BurnSampleInit(1);
	BurnSampleSetAllRoutesAllSamples(0.50, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxExp,    2, 8, 8, 0x04000, 0x00, 0xf);

	sega_decrypt = decrypt;
	read_port_cb = readport;
	write_port_cb = writeport;

	if (pLoadCallback) {
		pLoadCallback();
	}

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	ZetExit();
	ppi8255_exit();

	BurnSampleExit();

	switch (sound_type)
	{
		case SOUND_005:
			stream_005.exit();
		break;

		case SOUND_ASTROB:
			sega_speech_exit();
		break;

		case SOUND_USB:
			I8039Exit();
			usb_sound_exit();
		break;

		case SOUND_Z80:
			SN76496Exit();
		break;

		case SOUND_MONSTERB:
			mcs48Exit();
			tms36xx_exit();
			DACExit();
		break;
	}

	BurnFreeMemIndex();

	write_port_cb = NULL;
	read_port_cb = NULL;
	sound_type = 0;
	ext_palette_bit = 8;
	has_startlatch = 0;

	return 0;
}

static void DrvPaletteUpdate()
{
	static const INT32 rg_resistances[3] = { 4700, 2400, 1200 };
	static const INT32 b_resistances[2] = { 2000, 1000 };
	double rweights[3], gweights[3], bweights[2];

	compute_resistor_weights(0, 255, -1.0,
			3,  rg_resistances, rweights, 220, 0,
			3,  rg_resistances, gweights, 220, 0,
			2,  b_resistances,  bweights, 220, 0);

	for (INT32 i = 0; i < BurnDrvGetPaletteEntries(); i++)
	{
		UINT8 data = DrvPalRAM[i];

		INT32 r = (data & 0x07) >> 0;
		INT32 g = (data & 0x38) >> 3;
		INT32 b = (data & 0xc0) >> 6;

		INT32 bit0 = (r >> 0) & 0x01;
		INT32 bit1 = (r >> 1) & 0x01;
		INT32 bit2 = (r >> 2) & 0x01;
		r = combine_3_weights(rweights, bit0, bit1, bit2);

		bit0 = (g >> 0) & 0x01;
		bit1 = (g >> 1) & 0x01;
		bit2 = (g >> 2) & 0x01;
		g = combine_3_weights(gweights, bit0, bit1, bit2);

		bit0 = (b >> 0) & 0x01;
		bit1 = (b >> 1) & 0x01;
		b = combine_2_weights(bweights, bit0, bit1);

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	}

	GenericTilemapDraw(0, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void spaceod_bg_init_palette()
{
	static const INT32 resistances[2] = { 1800, 1200 };
	double trweights[2], tgweights[2], tbweights[2];

	compute_resistor_weights(0, 255, -1.0,
			2,  resistances, trweights, 220, 0,
			2,  resistances, tgweights, 220, 0,
			2,  resistances, tbweights, 220, 0);

	for (INT32 i = 0; i < 64; i++)
	{
		INT32 r = (i & 0x30) >> 4;
		INT32 g = (i & 0x0c) >> 2;
		INT32 b = (i & 0x03) >> 0;

		INT32 bit0 = (r >> 0) & 0x01;
		INT32 bit1 = (r >> 1) & 0x01;
		r = combine_2_weights(trweights, bit0, bit1);

		/* green component */
		bit0 = (g >> 0) & 0x01;
		bit1 = (g >> 1) & 0x01;
		g = combine_2_weights(tgweights, bit0, bit1);

		/* blue component */
		bit0 = (b >> 0) & 0x01;
		bit1 = (b >> 1) & 0x01;
		b = combine_2_weights(tbweights, bit0, bit1);

		DrvPalette[i + 0x40] = BurnHighCol(r,g,b,0);
	}
}

static void draw_background_spaceod()
{
	INT32 select = (spaceod_bg_control & 0x02) ? 2 : 1;
	INT32 flipmask = 0;//(spaceod_bg_control & 0x01) ? 0xff : 0x00;
	INT32 xoffset = (spaceod_bg_control & 0x02) ? 0x10 : 0x00;

	GenericTilemapSetScrollX(select, bg_scrollx + xoffset);
	GenericTilemapSetScrollY(select, (bg_scrolly + 22)^flipmask);
	GenericTilemapSetFlip(select, video_flip ? TMAP_FLIPXY : 0);
	GenericTilemapDraw(select, 1, 0);

	for (INT32 y = 0; y < nScreenHeight; y++)
	{
		UINT16 *src = BurnBitmapGetBitmap(1) + y * nScreenWidth;
		UINT16 *dst = pTransDraw + y * nScreenWidth;

		for (INT32 x = 0; x < nScreenWidth; x++)
		{
			UINT8 fgpix = DrvPalRAM[dst[x]];
			UINT8 bgpix = src[x] & 0x3f;

			if (bgpix != 0 && fgpix != 0 && (dst[x] >> 2) == 1)
				spaceod_bg_detect = 1;

			if (fgpix == 0 && bg_enable == 0)
				dst[x] = bgpix | spaceod_fixed_color | 0x40;
		}
	}
}

static INT32 SpaceodDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		spaceod_bg_init_palette();
		DrvRecalc = 1;
	}

	if (~nBurnLayer & 1) BurnTransferClear();
	if ( nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);
	if ( nBurnLayer & 2) draw_background_spaceod();

	BurnTransferCopy(DrvPalette);

//	if (~nSpriteEnable & 1) GenericTilemapDumpToBitmap();

	return 0;
}

static void draw_background_pignewt()
{
	INT32 flipmask = (video_control & 0x08) ? 0x3ff : 0;

	if (!bg_enable) return;
	BurnBitmapFill(1, 0);
	GenericTilemapDraw(1, 1, 0);

	for (INT32 y = 0; y < nScreenHeight; y++)
	{
		UINT16 *src = BurnBitmapGetPosition(1, 0, (y + bg_scrolly) ^ flipmask);
		UINT16 *dst = pTransDraw + y * nScreenWidth;

		for (INT32 x = 0; x < nScreenWidth; x++)
		{
			int xx = (x + bg_scrollx) ^ flipmask;
			dst[x] = src[xx & 0x3ff];
		}
	}
}

static INT32 PignewtDraw() // also monsterb2
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	}

	BurnTransferClear();
	if ( nBurnLayer & 1) draw_background_pignewt();
	if ( nBurnLayer & 2) GenericTilemapDraw(0, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

//	if (~nSpriteEnable & 1) GenericTilemapDumpToBitmap();

	return 0;
}

static INT32 MonsterbDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	}

	GenericTilemapSetEnable(1, bg_enable);
	GenericTilemapSetScrollX(1, bg_scrollx);
	GenericTilemapSetScrollY(1, bg_scrolly);
	GenericTilemapSetFlip(1, video_flip ? TMAP_FLIPXY : 0);

	BurnTransferClear();
	if ( nBurnLayer & 1) GenericTilemapDraw(1, pTransDraw, 0);
	if ( nBurnLayer & 2) GenericTilemapDraw(0, pTransDraw, 0);

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
		static UINT8 lastS = 0;
		DrvInputs[0] = 0xff;
		DrvInputs[1] = 0xff;
		DrvInputs[2] = DrvDips[0];
		DrvInputs[3] = DrvDips[1];
		DrvInputs[4] = DrvDips[2];

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[4] ^= (DrvJoy3[i] & 1) << i; // 'fc'
		}

		hold_coin.checklow(0, DrvInputs[0], 1 << 0, 3);
		hold_coin.checklow(1, DrvInputs[0], 1 << 2, 3);
		if (has_startlatch) {
			// 005, spaceod, monsterb2 listens for start every 48 frames....
			hold_start.checklow(0, DrvInputs[1], 1 << 1, 50);
			hold_start.checklow(1, DrvInputs[1], 1 << 5, 50);
		}

		if (lastS == 0 && DrvJoyS[0]) {
			ZetNmi(0);
		}
		lastS = DrvJoyS[0];
	}

	INT32 nInterleave = 262/2;
	INT32 nCyclesTotal[3] = { 3867000 / 60, 0, 0 };
	INT32 nCyclesDone[3] = { nCyclesExtra, 0, 0 };

	switch (sound_type)
	{
		case SOUND_ASTROB:
			nCyclesTotal[1] = 208000 / 60;
			sega_speech_new_frame();
		break;

		case SOUND_USB:
			nCyclesTotal[1] = 400000 / 60;
			I8039NewFrame();
		break;

		case SOUND_Z80:
			nCyclesTotal[1] = 4000000 / 60;
		break;

		case SOUND_MONSTERB:
			nCyclesTotal[1] = 6000000 / 15 / 60; // internal divider 15 (mcs48)
			mcs48NewFrame();
		break;
	}

	vblank_latch = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		CPU_RUN(0, Zet);

		if ((i == (224/2)) && (video_control & 4)){
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
			vblank_latch = 1;
			video_flip = video_control & 1;
		    GenericTilemapSetOffsets(0, 0, video_flip ? -32 : 0);

			if (pBurnDraw) {
				BurnDrvRedraw();
			}
		}
		ZetClose();

		switch (sound_type)
		{
			case SOUND_005:
				ZetOpen(0);
				stream_005.update();
				ZetClose();
			break;

			case SOUND_ASTROB:
				sega_speech_run(i, nInterleave, nCyclesTotal, nCyclesDone, 1);
			break;

			case SOUND_USB:
				CPU_RUN(1, usbSound);
				if ((i % 6) != 4) usb_timer_t1_clock();
			break;

			case SOUND_Z80:
				ZetOpen(1);
				CPU_RUN(1, Zet);
				if ((i % 32) == 31) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
				ZetClose();
			break;

			case SOUND_MONSTERB:
				mcs48Open(0);
				CPU_RUN_SYNCINT(1, mcs48);
				mcs48Close();
			break;
		}
	}

	nCyclesExtra = nCyclesDone[0] - nCyclesTotal[0];

	if (pBurnSoundOut) {
		BurnSoundClear();
		BurnSampleRender(pBurnSoundOut, nBurnSoundLen);

		switch (sound_type)
		{
			case SOUND_005:
				stream_005.render(pBurnSoundOut, nBurnSoundLen);
				BurnSoundDCFilter();
			break;

			case SOUND_ASTROB:
				sega_speech_render(pBurnSoundOut, nBurnSoundLen);
			break;

			case SOUND_USB:
				segausb_update(pBurnSoundOut, nBurnSoundLen);
			break;

			case SOUND_Z80:
				SN76496Update(0, pBurnSoundOut, nBurnSoundLen);
				SN76496Update(1, pBurnSoundOut, nBurnSoundLen);
			break;

			case SOUND_MONSTERB:
				DACUpdate(pBurnSoundOut, nBurnSoundLen);
				tms36xx_sound_update(pBurnSoundOut, nBurnSoundLen);
			break;
		}
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	if (pnMin) {
		*pnMin = 0x029702;
	}

	if (nAction & ACB_VOLATILE) {
		ScanVar(AllRam, RamEnd - AllRam, "All Ram");

		ZetScan(nAction);
		ppi8255_scan();
		BurnSampleScan(nAction, pnMin);

		switch (sound_type)
		{
			case SOUND_005:
				SCAN_VAR(sound_addr);
				SCAN_VAR(sound_data);
				SCAN_VAR(square_state);
				SCAN_VAR(square_count);
				SCAN_VAR(snd_timer);
				SCAN_VAR(snd_timer_period);
				break;

			case SOUND_ASTROB:
				sega_speech_scan(nAction, pnMin);
			break;

			case SOUND_USB:
				I8039Scan(nAction, pnMin);
				usb_sound_scan(nAction, pnMin);
			break;

			case SOUND_Z80:
				SN76496Scan(nAction, pnMin);
			break;

			case SOUND_MONSTERB:
				mcs48Scan(nAction);
				DACScan(nAction, pnMin);
				tms36xx_scan(nAction, pnMin);
				i8243_scan();
				SCAN_VAR(monsterb_sound_addr);
				SCAN_VAR(upd7751_command);
				SCAN_VAR(upd7751_busy);
			break;
		}

		SCAN_VAR(video_control);
		SCAN_VAR(vblank_latch);
		SCAN_VAR(video_flip);
		SCAN_VAR(soundlatch);
		SCAN_VAR(sound_state);
		SCAN_VAR(sound_rate);
		SCAN_VAR(pignewt_bg_color_offset);
		SCAN_VAR(spaceod_bg_control);
		SCAN_VAR(spaceod_bg_detect);
		SCAN_VAR(spaceod_fixed_color);
		SCAN_VAR(bg_enable);
		SCAN_VAR(bg_char_bank);
		SCAN_VAR(bg_scrolly);
		SCAN_VAR(bg_scrollx);

		SCAN_VAR(nCyclesExtra);

		hold_coin.scan();
		hold_start.scan();
	}

	return 0;
}


// Astro Blaster (version 3)

static struct BurnRomInfo astrobRomDesc[] = {
	{ "829b.cpu-u25",			0x0800, 0x14ae953c, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "907a.prom-u1",			0x0800, 0xa9aaaf38, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "908a.prom-u2",			0x0800, 0x897f2b87, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "909a.prom-u3",			0x0800, 0x55a339e6, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "910a.prom-u4",			0x0800, 0x7972b60a, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "911a.prom-u5",			0x0800, 0xaf87520f, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "912a.prom-u6",			0x0800, 0xb656f929, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "913a.prom-u7",			0x0800, 0x321074b3, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "914a.prom-u8",			0x0800, 0x90d2493e, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "915a.prom-u9",			0x0800, 0xaaf828d1, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "916a.prom-u10",			0x0800, 0x56d92ab9, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "917a.prom-u11",			0x0800, 0x9dcdaf2d, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "918a.prom-u12",			0x0800, 0xc9d09655, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "919a.prom-u13",			0x0800, 0x448bd318, 1 | BRF_PRG | BRF_ESS }, // 13
	{ "920a.prom-u14",			0x0800, 0x3524a383, 1 | BRF_PRG | BRF_ESS }, // 14
	{ "921a.prom-u15",			0x0800, 0x98c14834, 1 | BRF_PRG | BRF_ESS }, // 15
	{ "922a.prom-u16",			0x0800, 0x4311513c, 1 | BRF_PRG | BRF_ESS }, // 16
	{ "923a.prom-u17",			0x0800, 0x50f0462c, 1 | BRF_PRG | BRF_ESS }, // 17
	{ "924a.prom-u18",			0x0800, 0x120a39c7, 1 | BRF_PRG | BRF_ESS }, // 18
	{ "925a.prom-u19",			0x0800, 0x790a7f4e, 1 | BRF_PRG | BRF_ESS }, // 19

	{ "808b.speech-u7",			0x0800, 0x5988c767, 2 | BRF_PRG | BRF_ESS }, // 20 I8035 Code

	{ "809a.speech-u6",			0x0800, 0x893f228d, 3 | BRF_SND },           // 21 Speech Samples
	{ "810.speech-u5",			0x0800, 0xff0163c5, 3 | BRF_SND },           // 22
	{ "811.speech-u4",			0x0800, 0x219f3978, 3 | BRF_SND },           // 23
	{ "812a.speech-u3",			0x0800, 0x410ad0d2, 3 | BRF_SND },           // 24

	{ "pr84.speech-u30",		0x0020, 0xadcb81d0, 4 | BRF_SND },           // 25 Sound PROM

	{ "316-0806.video1-u52",	0x0020, 0x358128b6, 0 | BRF_OPT },           // 26 Unused PROMs
	{ "316-0764.cpu-u15",    	0x0020, 0xc609b79e, 0 | BRF_OPT },           // 27
	
};

STD_ROM_PICK(astrob)
STD_ROM_FN(astrob)


static void astrob_sound(UINT32 offset, UINT8 data)
{
	static const float attack_resistor[10] =
	{
		120.0f, 82.0f, 62.0f, 56.0f, 47.0f, 39.0f, 33.0f, 27.0f, 24.0f, 22.0f
	};
	float freq_factor;

	UINT8 diff = data ^ sound_state[offset];
	sound_state[offset] = data;

	switch (offset)
	{
		case 0:
			/* INVADER-1: channel 0 */
			if ((diff & 0x01) && !(data & 0x01)) splayexch(0, (data & 0x80) ? 0 : 1, 0.25, 100, false, true);
			if ((data & 0x01) && splayingch(0)) sstopch(0);

			/* INVADER-2: channel 1 */
			if ((diff & 0x02) && !(data & 0x02)) splayexch(1, (data & 0x80) ? 2 : 3, 0.25, 100, false, true);
			if ((data & 0x02) && splayingch(1)) sstopch(1);

			/* INVADER-3: channel 2 */
			if ((diff & 0x04) && !(data & 0x04)) splayexch(2, (data & 0x80) ? 4 : 5, 0.25, 100, false, true);
			if ((data & 0x04) && splayingch(2)) sstopch(2);

			/* INVADER-4: channel 3 */
			if ((diff & 0x08) && !(data & 0x08)) splayexch(3, (data & 0x80) ? 6 : 7, 0.25, 100, false, true);
			if ((data & 0x08) && splayingch(3)) sstopch(3);

			/* ASTROIDS: channel 4 */
			if ((diff & 0x10) && !(data & 0x10)) splayexch(4, 8, 0.25, 100, false, true);
			if ((data & 0x10) && splayingch(4)) sstopch(4);

			/* MUTE */
			//machine().sound().system_mute(data & 0x20);

			/* REFILL: channel 5 */
			if (!(data & 0x40) && !splayingch(5)) splayexch(5, 9, 0.25, 100, false, false);
			if ( (data & 0x40) && splayingch(5))  sstopch(5);

			/* WARP: changes which sample is played for the INVADER samples above */
			if (diff & 0x80)
			{
				if (splayingch(0)) splayexch(0, (data & 0x80) ? 0 : 1, 0.25, 100, false, true);
				if (splayingch(1)) splayexch(1, (data & 0x80) ? 2 : 3, 0.25, 100, false, true);
				if (splayingch(2)) splayexch(2, (data & 0x80) ? 4 : 5, 0.25, 100, false, true);
				if (splayingch(3)) splayexch(3, (data & 0x80) ? 6 : 7, 0.25, 100, false, true);
			}
			break;

		case 1:
			/* LASER #1: channel 6 */
			if ((diff & 0x01) && !(data & 0x01)) splayexch(6, 10, 0.25, 100, false, false);

			/* LASER #2: channel 7 */
			if ((diff & 0x02) && !(data & 0x02)) splayexch(7, 11, 0.25, 100, false, false);

			/* SHORT EXPL: channel 8 */
			if ((diff & 0x04) && !(data & 0x04)) splayexch(8, 12, 0.50, 100, false, false);

			/* LONG EXPL: channel 8 */
			if ((diff & 0x08) && !(data & 0x08)) splayexch(8, 13, 0.55, 100, false, false);

			/* ATTACK RATE */
			if ((diff & 0x10) && !(data & 0x10)) sound_rate = (sound_rate + 1) % 10;

			/* RATE RESET */
			if (!(data & 0x20)) sound_rate = 0;

			/* BONUS: channel 9 */
			if ((diff & 0x40) && !(data & 0x40)) splayexch(9, 14, 0.25, 100, false, false);

			/* SONAR: channel 10 */
			if ((diff & 0x80) && !(data & 0x80)) splayexch(10, 15, 0.25, 100, true, false);
			break;
	}

	/* the samples were recorded with sound_rate = 0, so we need to scale */
	/* the frequency as a fraction of that; these equations come from */
	/* Derrick's analysis above; we compute the inverted scale factor to */
	/* account for the fact that frequency goes up as CV goes down */
	/* WARP is already taken into account by the differing samples above */
	freq_factor  = (11.5f - 8.163f) * (-22.0f / attack_resistor[0]) + 8.163f;
	freq_factor /= (11.5f - 8.163f) * (-22.0f / attack_resistor[sound_rate]) + 8.163f;

	/* adjust the sample rate of invader sounds based the sound_rate */
	/* this is an approximation */

	if (splayingch(0)) BurnSampleChannelSetPlaybackRate(0, freq_factor * 100);
	if (splayingch(1)) BurnSampleChannelSetPlaybackRate(1, freq_factor * 100);
	if (splayingch(2)) BurnSampleChannelSetPlaybackRate(2, freq_factor * 100);
	if (splayingch(3)) BurnSampleChannelSetPlaybackRate(3, freq_factor * 100);

}

static void astrob_write_ports(UINT8 port, UINT8 data)
{
	switch (port)
	{
		case 0x38:
			sega_speech_data_write(data);
		return;

		case 0x3b:
			// speech_control_w (not necessary)
		return;

		case 0x3e:
		case 0x3f:
			astrob_sound(port & 1, data);
		return;
	}
}

static UINT8 sega_decrypt62(UINT16 pc, UINT8 lo)
{
	switch (pc & 0x03)
	{
		case 0x00: return BITSWAP08(lo, 2, 4, 5, 3, 7, 6, 1, 0) ^ 0x80;
		case 0x01: return BITSWAP08(lo, 2, 3, 6, 5, 7, 4, 1, 0) ^ 0x20;
		case 0x02: return BITSWAP08(lo, 2, 7, 3, 4, 6, 5, 1, 0) ^ 0x10;
		case 0x03: return lo;
	}

	return lo;
}

static INT32 AstrobInit()
{
	sound_type = SOUND_ASTROB;

	return DrvInit(sega_decrypt62, NULL, standard_read_port, astrob_write_ports);
}

static struct BurnSampleInfo AstrobSampleDesc[] = {
	{ "invadr1", SAMPLE_NOLOOP },
	{ "winvadr1", SAMPLE_NOLOOP },
	{ "invadr2", SAMPLE_NOLOOP },
	{ "winvadr2", SAMPLE_NOLOOP },
	{ "invadr3", SAMPLE_NOLOOP },
	{ "winvadr3", SAMPLE_NOLOOP },
	{ "invadr4", SAMPLE_NOLOOP },
	{ "winvadr4", SAMPLE_NOLOOP },
	{ "asteroid", SAMPLE_NOLOOP },
	{ "refuel", SAMPLE_NOLOOP },
	{ "pbullet", SAMPLE_NOLOOP },
	{ "ebullet", SAMPLE_NOLOOP },
	{ "eexplode", SAMPLE_NOLOOP },
	{ "pexplode", SAMPLE_NOLOOP },
	{ "deedle", SAMPLE_NOLOOP },
	{ "sonar", SAMPLE_NOLOOP },
	{ "", 0 }
};

STD_SAMPLE_PICK(Astrob)
STD_SAMPLE_FN(Astrob)

struct BurnDriver BurnDrvAstrob = {
	"astrob", NULL, NULL, "astrob", "1981",
	"Astro Blaster (version 3)\0", NULL, "Sega", "G80 Raster",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_VERSHOOT, 0,
	NULL, astrobRomInfo, astrobRomName, NULL, NULL, AstrobSampleInfo, AstrobSampleName, AstrobInputInfo, AstrobDIPInfo,
	AstrobInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Astro Blaster (version 2)

static struct BurnRomInfo astrob2RomDesc[] = {
	{ "829b.cpu-u25",			0x0800, 0x14ae953c, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "888a.prom-u1",			0x0800, 0x42601744, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "889.prom-u2",			0x0800, 0xdd9ab173, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "890.prom-u3",			0x0800, 0x26f5b4cf, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "891.prom-u4",			0x0800, 0x6437c95f, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "892.prom-u5",			0x0800, 0x2d3c949b, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "893.prom-u6",			0x0800, 0xccdb1a76, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "894.prom-u7",			0x0800, 0x66ae5ced, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "895.prom-u8",			0x0800, 0x202cf3a3, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "896.prom-u9",			0x0800, 0xb603fe23, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "897.prom-u10",			0x0800, 0x989198c6, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "898.prom-u11",			0x0800, 0xef2bab04, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "899.prom-u12",			0x0800, 0xe0d189ee, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "900.prom-u13",			0x0800, 0x682d4604, 1 | BRF_PRG | BRF_ESS }, // 13
	{ "901.prom-u14",			0x0800, 0x9ed11c61, 1 | BRF_PRG | BRF_ESS }, // 14
	{ "902.prom-u15",			0x0800, 0xb4d6c330, 1 | BRF_PRG | BRF_ESS }, // 15
	{ "903a.prom-u16",			0x0800, 0x84acc38c, 1 | BRF_PRG | BRF_ESS }, // 16
	{ "904.prom-u17",			0x0800, 0x5eba3097, 1 | BRF_PRG | BRF_ESS }, // 17
	{ "905.prom-u18",			0x0800, 0x4f08f9f4, 1 | BRF_PRG | BRF_ESS }, // 18
	{ "906.prom-u19",			0x0800, 0x58149df1, 1 | BRF_PRG | BRF_ESS }, // 19

	{ "808b.speech-u7",			0x0800, 0x5988c767, 2 | BRF_PRG | BRF_ESS }, // 20 I8035 Code

	{ "809a.speech-u6",			0x0800, 0x893f228d, 3 | BRF_SND },           // 21 Speech Samples
	{ "810.speech-u5",			0x0800, 0xff0163c5, 3 | BRF_SND },           // 22
	{ "811.speech-u4",			0x0800, 0x219f3978, 3 | BRF_SND },           // 23
	{ "812a.speech-u3",			0x0800, 0x410ad0d2, 3 | BRF_SND },           // 24

	{ "pr84.speech-u30",		0x0020, 0xadcb81d0, 4 | BRF_SND },           // 25 Sound PROM

	{ "316-0806.video1-u52",	0x0020, 0x358128b6, 0 | BRF_OPT },           // 26 Unused PROMs
	{ "316-0764.cpu-u15",		0x0020, 0xc609b79e, 0 | BRF_OPT },           // 27
};

STD_ROM_PICK(astrob2)
STD_ROM_FN(astrob2)

struct BurnDriver BurnDrvAstrob2 = {
	"astrob2", "astrob", NULL, "astrob", "1981",
	"Astro Blaster (version 2)\0", NULL, "Sega", "G80 Raster",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_VERSHOOT, 0,
	NULL, astrob2RomInfo, astrob2RomName, NULL, NULL, AstrobSampleInfo, AstrobSampleName, AstrobInputInfo, Astrob2DIPInfo,
	AstrobInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};

// Astro Blaster (version 2 hack) by Clay Cowgill

static struct BurnRomInfo astrob2hRomDesc[] = {
	{ "829b.cpu-u25",			0x0800, 0x14ae953c, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "888a.prom-u1",			0x0800, 0x42601744, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "889.prom-u2",			0x0800, 0x9fcdc62f, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "890.prom-u3",			0x0800, 0x26f5b4cf, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "891.prom-u4",			0x0800, 0x74f906dc, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "892.prom-u5",			0x0800, 0x2d3c949b, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "893.prom-u6",			0x0800, 0xccdb1a76, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "894.prom-u7",			0x0800, 0x66ae5ced, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "895.prom-u8",			0x0800, 0x202cf3a3, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "896.prom-u9",			0x0800, 0xb603fe23, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "897.prom-u10",			0x0800, 0x989198c6, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "898.prom-u11",			0x0800, 0xef2bab04, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "899.prom-u12",			0x0800, 0xe0d189ee, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "900.prom-u13",			0x0800, 0x682d4604, 1 | BRF_PRG | BRF_ESS }, // 13
	{ "901.prom-u14",			0x0800, 0x9ed11c61, 1 | BRF_PRG | BRF_ESS }, // 14
	{ "902.prom-u15",			0x0800, 0xb4d6c330, 1 | BRF_PRG | BRF_ESS }, // 15
	{ "903a.prom-u16",			0x0800, 0x84acc38c, 1 | BRF_PRG | BRF_ESS }, // 16
	{ "904.prom-u17",			0x0800, 0x5eba3097, 1 | BRF_PRG | BRF_ESS }, // 17
	{ "905.prom-u18",			0x0800, 0x4f08f9f4, 1 | BRF_PRG | BRF_ESS }, // 18
	{ "906.prom-u19",			0x0800, 0x58149df1, 1 | BRF_PRG | BRF_ESS }, // 19

	{ "808b.speech-u7",			0x0800, 0x5988c767, 2 | BRF_PRG | BRF_ESS }, // 20 I8035 Code

	{ "809a.speech-u6",			0x0800, 0x893f228d, 3 | BRF_SND },           // 21 Speech Samples
	{ "810.speech-u5",			0x0800, 0xff0163c5, 3 | BRF_SND },           // 22
	{ "811.speech-u4",			0x0800, 0x219f3978, 3 | BRF_SND },           // 23
	{ "812a.speech-u3",			0x0800, 0x410ad0d2, 3 | BRF_SND },           // 24

	{ "pr84.speech-u30",		0x0020, 0xadcb81d0, 4 | BRF_SND },           // 25 Sound PROM

	{ "316-0806.video1-u52",	0x0020, 0x358128b6, 0 | BRF_OPT },           // 26 Unused PROMs
	{ "316-0764.cpu-u15",		0x0020, 0xc609b79e, 0 | BRF_OPT },           // 27
};

STD_ROM_PICK(astrob2h)
STD_ROM_FN(astrob2h)

struct BurnDriver BurnDrvAstrob2h = {
	"astrob2h", "astrob", NULL, "astrob", "1981",
	"Astro Blaster (version 2 Hack)\0", "Less Overheat hack by Clay Cowgill", "Sega", "G80 Raster",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_VERSHOOT, 0,
	NULL, astrob2hRomInfo, astrob2hRomName, NULL, NULL, AstrobSampleInfo, AstrobSampleName, AstrobInputInfo, Astrob2DIPInfo,
	AstrobInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Astro Blaster (version 2a)

static struct BurnRomInfo astrob2aRomDesc[] = {
	{ "829b.cpu-u25",			0x0800, 0x14ae953c, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "888c.prom-u1",			0x0800, 0x15fa9c3e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "889.prom-u2",			0x0800, 0xdd9ab173, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "890.prom-u3",			0x0800, 0x26f5b4cf, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "891.prom-u4",			0x0800, 0x6437c95f, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "892.prom-u5",			0x0800, 0x2d3c949b, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "893.prom-u6",			0x0800, 0xccdb1a76, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "894.prom-u7",			0x0800, 0x66ae5ced, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "895.prom-u8",			0x0800, 0x202cf3a3, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "896.prom-u9",			0x0800, 0xb603fe23, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "897b.prom-u10",			0x0800, 0xa4f6008a, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "898a.prom-u11",			0x0800, 0xa92c7826, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "899.prom-u12",			0x0800, 0xe0d189ee, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "900.prom-u13",			0x0800, 0x682d4604, 1 | BRF_PRG | BRF_ESS }, // 13
	{ "901.prom-u14",			0x0800, 0x9ed11c61, 1 | BRF_PRG | BRF_ESS }, // 14
	{ "902.prom-u15",			0x0800, 0xb4d6c330, 1 | BRF_PRG | BRF_ESS }, // 15
	{ "903c.prom-u16",			0x0800, 0xec8c8d98, 1 | BRF_PRG | BRF_ESS }, // 16
	{ "904a.prom-u17",			0x0800, 0xd1df0f3e, 1 | BRF_PRG | BRF_ESS }, // 17
	{ "905.prom-u18",			0x0800, 0x4f08f9f4, 1 | BRF_PRG | BRF_ESS }, // 18
	{ "906.prom-u19",			0x0800, 0x58149df1, 1 | BRF_PRG | BRF_ESS }, // 19

	{ "808b.speech-u7",			0x0800, 0x5988c767, 2 | BRF_PRG | BRF_ESS }, // 20 I8035 Code

	{ "809a.speech-u6",			0x0800, 0x893f228d, 3 | BRF_SND },           // 21 Speech Samples
	{ "810.speech-u5",			0x0800, 0xff0163c5, 3 | BRF_SND },           // 22
	{ "811.speech-u4",			0x0800, 0x219f3978, 3 | BRF_SND },           // 23
	{ "812a.speech-u3",			0x0800, 0x410ad0d2, 3 | BRF_SND },           // 24

	{ "pr84.speech-u30",		0x0020, 0xadcb81d0, 4 | BRF_SND },           // 25 Sound PROM

	{ "316-0806.video1-u52",	0x0020, 0x358128b6, 0 | BRF_OPT },           // 26 Unused PROMs
	{ "316-0764.cpu-u15",		0x0020, 0xc609b79e, 0 | BRF_OPT },           // 27
};

STD_ROM_PICK(astrob2a)
STD_ROM_FN(astrob2a)

struct BurnDriver BurnDrvAstrob2a = {
	"astrob2a", "astrob", NULL, "astrob", "1981",
	"Astro Blaster (version 2a)\0", NULL, "Sega", "G80 Raster",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_VERSHOOT, 0,
	NULL, astrob2aRomInfo, astrob2aRomName, NULL, NULL, AstrobSampleInfo, AstrobSampleName, AstrobInputInfo, Astrob2DIPInfo,
	AstrobInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Astro Blaster (version 2b)

static struct BurnRomInfo astrob2bRomDesc[] = {
	{ "829d.cpu-u25",			0x0800, 0x987a195c, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "888c.prom-u1",			0x0800, 0x15fa9c3e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "889.prom-u2",			0x0800, 0xdd9ab173, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "890.prom-u3",			0x0800, 0x26f5b4cf, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "891.prom-u4",			0x0800, 0x6437c95f, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "892.prom-u5",			0x0800, 0x2d3c949b, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "893.prom-u6",			0x0800, 0xccdb1a76, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "894.prom-u7",			0x0800, 0x66ae5ced, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "895.prom-u8",			0x0800, 0x202cf3a3, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "896.prom-u9",			0x0800, 0xb603fe23, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "897b.prom-u10",			0x0800, 0xa4f6008a, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "898a.prom-u11",			0x0800, 0xa92c7826, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "899.prom-u12",			0x0800, 0xe0d189ee, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "900.prom-u13",			0x0800, 0x682d4604, 1 | BRF_PRG | BRF_ESS }, // 13
	{ "901.prom-u14",			0x0800, 0x9ed11c61, 1 | BRF_PRG | BRF_ESS }, // 14
	{ "902.prom-u15",			0x0800, 0xb4d6c330, 1 | BRF_PRG | BRF_ESS }, // 15
	{ "903c.prom-u16",			0x0800, 0xec8c8d98, 1 | BRF_PRG | BRF_ESS }, // 16
	{ "904a.prom-u17",			0x0800, 0xd1df0f3e, 1 | BRF_PRG | BRF_ESS }, // 17
	{ "905.prom-u18",			0x0800, 0x4f08f9f4, 1 | BRF_PRG | BRF_ESS }, // 18
	{ "906.prom-u19",			0x0800, 0x58149df1, 1 | BRF_PRG | BRF_ESS }, // 19

	{ "808b.speech-u7",			0x0800, 0x5988c767, 2 | BRF_PRG | BRF_ESS }, // 20 I8035 Code

	{ "809a.speech-u6",			0x0800, 0x893f228d, 3 | BRF_SND },           // 21 Speech Samples
	{ "810.speech-u5",			0x0800, 0xff0163c5, 3 | BRF_SND },           // 22
	{ "811.speech-u4",			0x0800, 0x219f3978, 3 | BRF_SND },           // 23
	{ "812a.speech-u3",			0x0800, 0x410ad0d2, 3 | BRF_SND },           // 24

	{ "pr84.speech-u30",		0x0020, 0xadcb81d0, 4 | BRF_SND },           // 25 Sound PROM

	{ "316-0806.video1-u52",	0x0020, 0x358128b6, 0 | BRF_OPT },           // 26 Unused PROMs
	{ "316-0764.cpu-u15",		0x0020, 0xc609b79e, 0 | BRF_OPT },           // 27
};

STD_ROM_PICK(astrob2b)
STD_ROM_FN(astrob2b)

struct BurnDriver BurnDrvAstrob2b = {
	"astrob2b", "astrob", NULL, "astrob", "1981",
	"Astro Blaster (version 2b)\0", NULL, "Sega", "G80 Raster",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_VERSHOOT, 0,
	NULL, astrob2bRomInfo, astrob2bRomName, NULL, NULL, AstrobSampleInfo, AstrobSampleName, AstrobInputInfo, Astrob2DIPInfo,
	AstrobInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Astro Blaster (version 1)

static struct BurnRomInfo astrob1RomDesc[] = {
	{ "829.cpu-u25",			0x0800, 0x5f66046e, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "837.prom-u1",			0x0800, 0xce9c3763, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "838.prom-u2",			0x0800, 0x3557289e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "839.prom-u3",			0x0800, 0xc88bda24, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "840.prom-u4",			0x0800, 0x24c9fe23, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "841.prom-u5",			0x0800, 0xf153c683, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "842.prom-u6",			0x0800, 0x4c5452b2, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "843.prom-u7",			0x0800, 0x673161a6, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "844.prom-u8",			0x0800, 0x6bfc59fd, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "845.prom-u9",			0x0800, 0x018623f3, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "846.prom-u10",			0x0800, 0x4d7c5fb3, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "847.prom-u11",			0x0800, 0x24d1d50a, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "848.prom-u12",			0x0800, 0x1c145541, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "849.prom-u13",			0x0800, 0xd378c169, 1 | BRF_PRG | BRF_ESS }, // 13
	{ "850.prom-u14",			0x0800, 0x9da673ae, 1 | BRF_PRG | BRF_ESS }, // 14
	{ "851.prom-u15",			0x0800, 0x3d4cf9f0, 1 | BRF_PRG | BRF_ESS }, // 15
	{ "852.prom-u16",			0x0800, 0xaf88a97e, 1 | BRF_PRG | BRF_ESS }, // 16

	{ "808b.speech-u7",			0x0800, 0x5988c767, 2 | BRF_PRG | BRF_ESS }, // 17 I8035 Code

	{ "809a.speech-u6",			0x0800, 0x893f228d, 3 | BRF_SND },           // 18 Speech Samples
	{ "810.speech-u5",			0x0800, 0xff0163c5, 3 | BRF_SND },           // 19
	{ "811.speech-u4",			0x0800, 0x219f3978, 3 | BRF_SND },           // 20
	{ "812a.speech-u3",			0x0800, 0x410ad0d2, 3 | BRF_SND },           // 21

	{ "pr84.speech-u30",		0x0020, 0xadcb81d0, 4 | BRF_SND },           // 22 Sound PROM

	{ "316-0806.video1-u52",	0x0020, 0x358128b6, 0 | BRF_OPT },           // 23 Unused PROMs
	{ "316-0764.cpu-u15",		0x0020, 0xc609b79e, 0 | BRF_OPT },           // 24
};

STD_ROM_PICK(astrob1)
STD_ROM_FN(astrob1)

struct BurnDriverD BurnDrvAstrob1 = {
	"astrob1", "astrob", NULL, "astrob", "1981",
	"Astro Blaster (version 1)\0", NULL, "Sega", "G80 Raster",
	NULL, NULL, NULL, NULL,
	BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_VERSHOOT, 0,
	NULL, astrob1RomInfo, astrob1RomName, NULL, NULL, AstrobSampleInfo, AstrobSampleName, AstrobInputInfo, AstrobfgDIPInfo,
	AstrobInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Astro Blaster (French)

static struct BurnRomInfo astrobfRomDesc[] = {
	{ "829b.u25",				0x0800, 0x14ae953c, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "834-0012_epr-375.u1",	0x0800, 0x82630950, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "834-0012_epr-376.u2",	0x0800, 0xd70d7d5e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "834-0012_epr-377.u3",	0x0800, 0x0dbad477, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "834-0012_epr-377.u4",	0x0800, 0x8fa809ab, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "834-0012_epr-379.u5",	0x0800, 0xc7a3c014, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "834-0012_epr-380.u6",	0x0800, 0xf13e804c, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "834-0012_epr-381.u7",	0x0800, 0x2194a624, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "834-0012_epr-382.u8",	0x0800, 0x95d84829, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "834-0012_epr-383.u9",	0x0800, 0x1495059f, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "834-0012_epr-384.u10",	0x0800, 0xd036a5c5, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "834-0012_epr-385.u11",	0x0800, 0x788380dd, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "834-0012_epr-386.u12",	0x0800, 0xa43686c4, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "834-0012_epr-387.u13",	0x0800, 0x43c8b973, 1 | BRF_PRG | BRF_ESS }, // 13
	{ "834-0012_epr-388.u14",	0x0800, 0x9f26d132, 1 | BRF_PRG | BRF_ESS }, // 14
	{ "834-0012_epr-436.u15",	0x0800, 0x3897de4a, 1 | BRF_PRG | BRF_ESS }, // 15
	{ "834-0012_epr-437.u16",	0x0800, 0x7f8f7d95, 1 | BRF_PRG | BRF_ESS }, // 16

	{ "834-0013_epr-311x.u7",	0x0800, 0x5988c767, 2 | BRF_PRG | BRF_ESS }, // 17 I8035 Code

	{ "834-0013_epr-438.u6",	0x0800, 0x06ec6186, 3 | BRF_SND },           // 18 Speech Samples
	{ "834-0013_epr-434.u5",	0x0800, 0xa8be23d5, 3 | BRF_SND },           // 19
	{ "834-0013_epr-440.u4",	0x0800, 0x7e07891d, 3 | BRF_SND },           // 20
	{ "834-0013_epr-441.u3",	0x0800, 0x205ef84d, 3 | BRF_SND },           // 21

	{ "pr84.speech-u30",		0x0020, 0xadcb81d0, 4 | BRF_SND },           // 22 Sound PROM

	{ "316-0806.video1-u52",	0x0020, 0x358128b6, 0 | BRF_OPT },           // 23 Unused PROMs
	{ "316-0764.cpu-u15",		0x0020, 0xc609b79e, 0 | BRF_OPT },           // 24
};

STD_ROM_PICK(astrobf)
STD_ROM_FN(astrobf)

struct BurnDriver BurnDrvAstrobf = {
	"astrobf", "astrob", NULL, "astrob", "1981",
	"Astro Blaster (French)\0", NULL, "Sega", "G80 Raster",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_VERSHOOT, 0,
	NULL, astrobfRomInfo, astrobfRomName, NULL, NULL, AstrobSampleInfo, AstrobSampleName, AstrobInputInfo, AstrobfgDIPInfo,
	AstrobInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Astro Blaster (German)

static struct BurnRomInfo astrobgRomDesc[] = {
	{ "829b.u25",				0x0800, 0x14ae953c, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "834b.u01",				0x0800, 0x82630950, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "814a.u02",				0x0800, 0xd70d7d5e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "815a.u03",				0x0800, 0x0dbad477, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "816a.u04",				0x0800, 0x8fa809ab, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "817a.u05",				0x0800, 0xc7a3c014, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "818a.u06",				0x0800, 0xf13e804c, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "819a.u07",				0x0800, 0x2194a624, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "820a.u08",				0x0800, 0x95d84829, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "821a.u09",				0x0800, 0x1495059f, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "822a.u10",				0x0800, 0xd036a5c5, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "823a.u11",				0x0800, 0x788380dd, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "824a.u12",				0x0800, 0xa43686c4, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "825a.u13",				0x0800, 0x43c8b973, 1 | BRF_PRG | BRF_ESS }, // 13
	{ "826a.u14",				0x0800, 0x9f26d132, 1 | BRF_PRG | BRF_ESS }, // 14
	{ "835.u15",				0x0800, 0x6eeeb409, 1 | BRF_PRG | BRF_ESS }, // 15
	{ "836.u16",				0x0800, 0x07ffe6dc, 1 | BRF_PRG | BRF_ESS }, // 16

	{ "808b_speech_de.u07",		0x0800, 0x5988c767, 2 | BRF_PRG | BRF_ESS }, // 17 I8035 Code

	{ "830_speech_de.u06",		0x0800, 0x2d840552, 3 | BRF_SND },           // 18 Speech Samples
	{ "831_speech_de.u05",		0x0800, 0x46b30ee4, 3 | BRF_SND },           // 19
	{ "832_speech_de.u04",		0x0800, 0xd05280b8, 3 | BRF_SND },           // 20
	{ "833_speech_de.u03",		0x0800, 0x08f11459, 3 | BRF_SND },           // 21

	{ "pr84.speech-u30",		0x0020, 0xadcb81d0, 4 | BRF_SND },           // 22 Sound PROM

	{ "316-0806.video1-u52",	0x0020, 0x358128b6, 0 | BRF_OPT },           // 23 Unused PROMs
	{ "316-0764.cpu-u15",		0x0020, 0xc609b79e, 0 | BRF_OPT },           // 24
};

STD_ROM_PICK(astrobg)
STD_ROM_FN(astrobg)

struct BurnDriver BurnDrvAstrobg = {
	"astrobg", "astrob", NULL, "astrob", "1981",
	"Astro Blaster (German)\0", NULL, "Sega", "G80 Raster",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_VERSHOOT, 0,
	NULL, astrobgRomInfo, astrobgRomName, NULL, NULL, AstrobSampleInfo, AstrobSampleName, AstrobInputInfo, AstrobfgDIPInfo,
	AstrobInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// 005

static struct BurnRomInfo Sega005RomDesc[] = {
	{ "1346b.cpu-u25",			0x0800, 0x8e68533e, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "5092.prom-u1",			0x0800, 0x29e10a81, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "5093.prom-u2",			0x0800, 0xe1edc3df, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "5094.prom-u3",			0x0800, 0x995773bb, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "5095.prom-u4",			0x0800, 0xf887f575, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "5096.prom-u5",			0x0800, 0x5545241e, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "5097.prom-u6",			0x0800, 0x428edb54, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "5098.prom-u7",			0x0800, 0x5bcb9d63, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "5099.prom-u8",			0x0800, 0x0ea24ba3, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "5100.prom-u9",			0x0800, 0xa79af131, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "5101.prom-u10",			0x0800, 0x8a1cdae0, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "5102.prom-u11",			0x0800, 0x70826a15, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "5103.prom-u12",			0x0800, 0x7f80c5b0, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "5104.prom-u13",			0x0800, 0x0140930e, 1 | BRF_PRG | BRF_ESS }, // 13
	{ "5105.prom-u14",			0x0800, 0x17807a05, 1 | BRF_PRG | BRF_ESS }, // 14
	{ "5106.prom-u15",			0x0800, 0xc7cdfa9d, 1 | BRF_PRG | BRF_ESS }, // 15
	{ "5107.prom-u16",			0x0800, 0x95f8a2e6, 1 | BRF_PRG | BRF_ESS }, // 16
	{ "5108.prom-u17",			0x0800, 0xd371cacd, 1 | BRF_PRG | BRF_ESS }, // 17
	{ "5109.prom-u18",			0x0800, 0x48a20617, 1 | BRF_PRG | BRF_ESS }, // 18
	{ "5110.prom-u19",			0x0800, 0x7d26111a, 1 | BRF_PRG | BRF_ESS }, // 19
	{ "5111.prom-u20",			0x0800, 0xa888e175, 1 | BRF_PRG | BRF_ESS }, // 20

	{ "epr-1286.sound-16",		0x0800, 0xfbe0d501, 2 | BRF_SND },           // 21 Sound ROM

	{ "6331.sound-u8",			0x0020, 0x1d298cb0, 4 | BRF_SND },           // 22 Sound PROM
};

STD_ROM_PICK(Sega005)
STD_ROM_FN(Sega005)

static struct BurnSampleInfo Sega005SampleDesc[] = {
	{ "lexplode",	SAMPLE_NOLOOP },	// 0
	{ "sexplode",	SAMPLE_NOLOOP },	// 1
	{ "dropbomb",	SAMPLE_NOLOOP },	// 2
	{ "shoot",		SAMPLE_NOLOOP },	// 3
	{ "missile",	SAMPLE_NOLOOP },	// 4
	{ "helicopt",	SAMPLE_NOLOOP },	// 5
	{ "whistle",	SAMPLE_NOLOOP },	// 6
	{ "", 0 }
};

STD_SAMPLE_PICK(Sega005)
STD_SAMPLE_FN(Sega005)

static void sega005_sound_a_write(UINT8 data)
{
	UINT8 diff = data ^ sound_state[0];
	sound_state[0] = data;

	if ((diff & 0x01) && !(data & 0x01)) splay(0, 0.20, false, false);// LARGE EXPL: channel 0
	if ((diff & 0x02) && !(data & 0x02)) splay(1, 0.20, false, false);// SMALL EXPL: channel 1
	if ((diff & 0x04) && !(data & 0x04)) splay(2, 0.15, false, false);// DROP BOMB: channel 2
	if ((diff & 0x08) && !(data & 0x08)) splay(3, 0.15, false, false);// SHOOT PISTOL: channel 3
	if ((diff & 0x10) && !(data & 0x10)) splay(4, 0.08, false, false);// MISSILE: channel 4
	// note: doesn't actually need "channel" feature of samples, but using it to test samples core.
	if ((diff & 0x20) && !(data & 0x20)) splayexch(5, 5, 0.15, 100, true,  true);// HELICOPTER: channel 5
	if ((diff & 0x20) &&  (data & 0x20)) sstopch(5);
	if ((diff & 0x40) && !(data & 0x40)) splay(6, 0.15, true,  true); // WHISTLE: channel 6
	if ((diff & 0x40) &&  (data & 0x40)) sstop(6);
}

static void sega005_update_sound_data()
{
	UINT8 newval = DrvSndROM[sound_addr];
	UINT8 diff = newval ^ sound_data;
	sound_data = newval;

	if ((diff & 0x20) && !(newval & 0x20)) {
		// m_005snd->m_sega005_sound_timer->adjust(attotime::never);
		snd_timer = -1; // timer disabled
	}

	if ((diff & 0x20) && (newval & 0x20)) {
		snd_timer = 0; // timer enabled
		// m_005snd->m_sega005_sound_timer->adjust(attotime::from_hz(SEGA005_555_TIMER_FREQ), 0, attotime::from_hz(SEGA005_555_TIMER_FREQ));
		snd_timer_period = hz_to_cycles((1.44 / ((15000 + 2 * 4700) * 1.5e-6)), 100000);
	}
}

static void sega005_auto_timer()
{
	if ((sound_state[1] & 0x20) && !(sound_state[1] & 0x10)) {
		sound_addr = (sound_addr & 0x780) | ((sound_addr + 1) & 0x07f);
		sega005_update_sound_data();
	}
}

static void sound_005_render(INT16 **streams, INT32 len)
{
	INT16 *dest = streams[0];

	for (INT32 i = 0; i < len; i++)
	{
		if (!(sound_state[1] & 0x10) && (++square_count & 0xff) == 0)
		{
			square_count = DrvSndPROM[sound_data & 0x1f];

			// hack - the RC should filter this out
			if (square_count != 0xff)
				square_state += 2;
		}

		dest[i] = (square_state & 2) ? 0x3fff : -0x3fff;

		if (snd_timer != -1) { // counter enabled
			snd_timer++;
			if (snd_timer >= snd_timer_period) {
				snd_timer -= snd_timer_period;
				sega005_auto_timer();
			}
		}
	}
}

static void sega005_sound_b_write(UINT8 data)
{
	UINT8 diff = data ^ sound_state[1];
	sound_state[1] = data;

	stream_005.update();

	sound_addr = ((data & 0x0f) << 7) | (sound_addr & 0x7f);

	if (data & 0x10) {
		sound_addr &= 0x780;
		square_state = 0;
	}

	if ((diff & 0x40) && (data & 0x40) && !(data & 0x20) && !(data & 0x10))
		sound_addr = (sound_addr & 0x780) | ((sound_addr + 1) & 0x07f);

	sega005_update_sound_data();
}

static void sega005_write_port(UINT8 port, UINT8 data)
{
	switch (port)
	{
		case 0x0c:
		case 0x0d:
		case 0x0e:
		case 0x0f:
			ppi8255_w(0, port & 3, data);
		return;
	}
}

static UINT8 sega005_read_port(UINT8 port)
{
	switch (port)
	{
		case 0x0c:
		case 0x0d:
		case 0x0e:
		case 0x0f:
			return ppi8255_r(0, port & 3);

		case 0xf8:
		case 0xf9:
		case 0xfa:
		case 0xfb:
			return mangled_ports_read(port);

		case 0xfc:
			return DrvInputs[4];
	}

	return 0;
}

static void Sega005Callback()
{
	ppi8255_set_write_port(0, 0, sega005_sound_a_write);
	ppi8255_set_write_port(0, 1, sega005_sound_b_write);

	stream_005.init(100000, nBurnSoundRate, 1, 1, sound_005_render);
    stream_005.set_volume(0.35);
	stream_005.set_buffered(ZetTotalCycles, 3867000);
	sega005_update_sound_data();
	sound_data = 0;
}

static UINT8 sega_decrypt70(UINT16 pc, UINT8 lo)
{
	switch (pc & 0x09)
	{
		case 0x00: return BITSWAP08(lo, 2, 7, 3, 4, 6, 5, 1, 0) ^ 0x10;
		case 0x01: return lo;
		case 0x08: return BITSWAP08(lo, 2, 4, 5, 3, 7, 6, 1, 0) ^ 0x80;
		case 0x09: return BITSWAP08(lo, 2, 3, 6, 5, 7, 4, 1, 0) ^ 0x20;
	}

	return lo;
}

static INT32 Sega005Init()
{
	sound_type = SOUND_005;
	has_startlatch = 1;

	return DrvInit(sega_decrypt70, Sega005Callback, sega005_read_port, sega005_write_port);
}

struct BurnDriver BurnDrvSega005 = {
	"005", NULL, NULL, "005", "1981",
	"005\0", NULL, "Sega", "G80 Raster",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_MAZE, 0,
	NULL, Sega005RomInfo, Sega005RomName, NULL, NULL, Sega005SampleInfo, Sega005SampleName, Sega005InputInfo, Sega005DIPInfo,
	Sega005Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// 005 (earlier version?)

static struct BurnRomInfo Sega005aRomDesc[] = {
	// first rom is bad dump
	{ "1346b.cpu-u25",			0x0800, 0x8e68533e, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "5092.prom-u1.bin",		0x0800, 0x85e3f7b0, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "5093.prom-u2.bin",		0x0800, 0x494b1a75, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "5094.prom-u3.bin",		0x0800, 0x1dc90882, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "5095.prom-u4.bin",		0x0800, 0x69c4e639, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "5096.prom-u5.bin",		0x0800, 0x635247ab, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "5097.prom-u6.bin",		0x0800, 0x02dc5126, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "5098.prom-u7.bin",		0x0800, 0xdd07a7be, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "5099.prom-u8.bin",		0x0800, 0x8ce68fef, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "5100.prom-u9.bin",		0x0800, 0xca52d905, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "5101.prom-u10.bin",		0x0800, 0xad03fc04, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "5102.prom-u11.bin",		0x0800, 0x00b4c810, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "5103.prom-u12.bin",		0x0800, 0x8f613070, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "5104.prom-u13.bin",		0x0800, 0xcf2764a2, 1 | BRF_PRG | BRF_ESS }, // 13
	{ "5105.prom-u14.bin",		0x0800, 0x2fccfbe2, 1 | BRF_PRG | BRF_ESS }, // 14
	{ "5106.prom-u15.bin",		0x0800, 0xc6b0aca1, 1 | BRF_PRG | BRF_ESS }, // 15
	{ "5107.prom-u16.bin",		0x0800, 0xd5b4e12d, 1 | BRF_PRG | BRF_ESS }, // 16
	{ "5108.prom-u17.bin",		0x0800, 0x02d6f0e6, 1 | BRF_PRG | BRF_ESS }, // 17
	{ "5109.prom-u18.bin",		0x0800, 0x37104272, 1 | BRF_PRG | BRF_ESS }, // 18
	{ "5110.prom-u19.bin",		0x0800, 0xea0a2104, 1 | BRF_PRG | BRF_ESS }, // 19
	{ "5111.prom-u20.bin",		0x0800, 0x126a9280, 1 | BRF_PRG | BRF_ESS }, // 20

	{ "epr-1286.sound-16",		0x0800, 0xfbe0d501, 2 | BRF_PRG | BRF_ESS }, // 21 Sound ROM

	{ "6331.sound-u8",			0x0020, 0x1d298cb0, 3 | BRF_SND },           // 22 Sound PROM
};

STD_ROM_PICK(Sega005a)
STD_ROM_FN(Sega005a)

struct BurnDriverD BurnDrvSega005a = {
	"005a", "005", NULL, "005", "1981",
	"005 (earlier version?)\0", NULL, "Sega", "G80 Raster",
	NULL, NULL, NULL, NULL,
	BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_MAZE, 0,
	NULL, Sega005aRomInfo, Sega005aRomName, NULL, NULL, Sega005SampleInfo, Sega005SampleName, Sega005InputInfo, Sega005DIPInfo,
	Sega005Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Space Odyssey (version 2)

static struct BurnRomInfo spaceodRomDesc[] = {
	{ "so-959.cpu-25",			0x0800, 0xbbae3cd1, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "spod941b.prom-u1",		0x0800, 0xb396c097, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "spod942a.prom-u2",		0x0800, 0xe4451554, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "spod943a.prom-u3",		0x0800, 0x0d87a8f3, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "spod944a.prom-u4",		0x0800, 0x9c79f505, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "spod945a.prom-u5",		0x0800, 0x375681e4, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "spod946a.prom-u6",		0x0800, 0x4a27ff64, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "spod947a.prom-u7",		0x0800, 0x6170de1c, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "spod948a.prom-u8",		0x0800, 0x44fbbc87, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "spod949a.prom-u9",		0x0800, 0xb2705596, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "so-950.prom-u10",		0x0800, 0x70a7a3b4, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "spod951a.prom-u11",		0x0800, 0x2bb7b5a3, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "so-952.prom-u12",		0x0800, 0x5bf19a12, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "so-953.prom-u13",		0x0800, 0x8066ac83, 1 | BRF_PRG | BRF_ESS }, // 13
	{ "so-954.prom-u14",		0x0800, 0x44ed6a0d, 1 | BRF_PRG | BRF_ESS }, // 14
	{ "spod955a.prom-u15",		0x0800, 0x254ca0fa, 1 | BRF_PRG | BRF_ESS }, // 15
	{ "so-956.prom-u16",		0x0800, 0x97de45a9, 1 | BRF_PRG | BRF_ESS }, // 16
	{ "spod957a.prom-u17",		0x0800, 0x1e31d118, 1 | BRF_PRG | BRF_ESS }, // 17
	{ "spod958a.prom-u18",		0x0800, 0xaa07e8e7, 1 | BRF_PRG | BRF_ESS }, // 18

	{ "epr-18.bg-u10",			0x1000, 0x24a81c04, 5 | BRF_GRA },           // 19 Tiles
	{ "epr-17.bg-u9",			0x1000, 0x6c7490c0, 5 | BRF_GRA },           // 20
	{ "epr-16.bg-u8",			0x1000, 0xacdf203e, 5 | BRF_GRA },           // 21
	{ "epr-15.bg-u7",			0x1000, 0xae0e5d71, 5 | BRF_GRA },           // 22
	{ "epr-14.bg-u6",			0x1000, 0xd2ebd915, 5 | BRF_GRA },           // 23
	{ "epr-13.bg-u5",			0x1000, 0x74bd7f9a, 5 | BRF_GRA },           // 24

	{ "epr-09.bg-u1",			0x1000, 0xa87bfc0a, 6 | BRF_GRA },           // 25 Tile Map
	{ "epr-10.bg-u2",			0x1000, 0x8ce88100, 6 | BRF_GRA },           // 26
	{ "epr-11.bg-u3",			0x1000, 0x1bdbdab5, 6 | BRF_GRA },           // 27
	{ "epr-12.bg-u4",			0x1000, 0x629a4a1f, 6 | BRF_GRA },           // 28
};

STD_ROM_PICK(spaceod)
STD_ROM_FN(spaceod)

static struct BurnSampleInfo SpaceodSampleDesc[] = {
	{ "fire", 		SAMPLE_NOLOOP },	// 0
	{ "bomb", 		SAMPLE_NOLOOP },	// 1
	{ "eexplode",	SAMPLE_NOLOOP },	// 2
	{ "pexplode",	SAMPLE_NOLOOP },	// 3
	{ "warp", 		SAMPLE_NOLOOP },	// 4
	{ "birth", 		SAMPLE_NOLOOP },	// 5
	{ "scoreup", 	SAMPLE_NOLOOP },	// 6
	{ "ssound", 	SAMPLE_NOLOOP },	// 7
	{ "accel", 		SAMPLE_NOLOOP },	// 8
	{ "damaged", 	SAMPLE_NOLOOP },	// 9
	{ "erocket", 	SAMPLE_NOLOOP },	// 10
	{ "", 0 }
};

STD_SAMPLE_PICK(Spaceod)
STD_SAMPLE_FN(Spaceod)

static void spaceod_sound_write(UINT8 offset, UINT8 data)
{
	UINT8 diff = data ^ sound_state[offset];
	sound_state[offset] = data;

	switch (offset)
	{
		case 0:
			if ((diff & 0x01) && !(data & 0x01)) splaych(0, 7, 0.25, true, true);			// BACK G: channel 0
			if ((diff & 0x01) &&  (data & 0x01)) sstopch(0);
			if ((diff & 0x04) && !(data & 0x04)) splaych(1, 2, 0.25);			// SHORT EXP: channel 1
			if ((diff & 0x10) && !(data & 0x10)) splaych(2, 8, 0.25);			// ACCELERATE: channel 2
			if ((diff & 0x20) && !(data & 0x20)) splaych(3, 10, 0.25);			// BATTLE STAR: channel 3
			if ((diff & 0x40) && !(data & 0x40)) splaych(4, 1, 0.25);			// D BOMB: channel 4
			if ((diff & 0x80) && !(data & 0x80)) splaych(5, 3, 0.25);			// LONG EXP: channel 5
		break;

		case 1:
			if ((diff & 0x01) && !(data & 0x01)) splaych(6, 0, 0.25);			// SHOT: channel 6
			if ((diff & 0x02) && !(data & 0x02)) splaych(7, 6, 0.25);			// BONUS UP: channel 7
			if ((diff & 0x08) && !(data & 0x08)) splaych(8, 4, 0.25);			// WARP: channel 8
			if (diff & 0x10) bprintf(0, _T("diff & 0x10\n"));
			if (diff & 0x20) bprintf(0, _T("diff & 0x20\n"));
			if ((diff & 0x40) && !(data & 0x40)) splaych(9, 5, 0.25);			// APPEARANCE UFO: channel 9
			if ((diff & 0x80) && !(data & 0x80)) splaych(10, 9, 0.25);			// BLACK HOLE: channel 10
		break;
	}
}

static void spaceod_port_write(UINT8 port, UINT8 data)
{
	switch (port)
	{
		case 0x08: spaceod_bg_control = data; return;
		case 0x09: bg_scrollx = bg_scrolly = 0; return;
		case 0x0a: ((spaceod_bg_control & 0x02) ? bg_scrolly : bg_scrollx) += ((spaceod_bg_control & 0x01) ? -1 : 1); return;
		case 0x0b: spaceod_bg_detect = 0; return;
		case 0x0c: bg_enable = data & 1; return;
		case 0x0d: spaceod_fixed_color = data & 0x3f; return;

		case 0x0e:
		case 0x0f:
			spaceod_sound_write(port & 1, data);
		return;
	}
}

static UINT8 spaceod_mangled_ports_read(UINT8 offset)
{
	UINT8 d7d6 = DrvInputs[0];
	UINT8 d5d4 = DrvInputs[1];
	UINT8 d3d2 = DrvInputs[2];
	UINT8 d1d0 = DrvInputs[3];
	INT32 shift = offset & 3;

	UINT8 upright = d3d2 & 0x04;
	if (upright)
	{
		UINT8 fc = DrvInputs[4];
		d7d6 |= 0x60;
		d5d4 = (d5d4 & ~0x1c) | ((~fc & 0x20) >> 3) | ((~fc & 0x10) >> 1) | ((~fc & 0x08) << 1) | 0xc0;
	}

	d7d6 >>= shift;
	d5d4 >>= shift;
	d3d2 >>= shift;
	d1d0 >>= shift;

	return ((d7d6 << 7) & 0x80) | ((d7d6 << 2) & 0x40) | ((d5d4 << 5) & 0x20) | ((d5d4 << 0) & 0x10) |
			((d3d2 << 3) & 0x08) | ((d3d2 >> 2) & 0x04) | ((d1d0 << 1) & 0x02) | ((d1d0 >> 4) & 0x01);
}

static UINT8 spaceod_port_fc_read()
{
	UINT8 fc = DrvInputs[4];

	if (DrvDips[0] & 0x04) // upright?
	{
		return (fc & 0x04) | ((fc & 0x02) >> 1) | ((fc & 0x01) << 1);
	}

	return fc;
}

static UINT8 spaceod_port_read(UINT8 port)
{
	switch (port)
	{
		case 0x08:
		case 0x09:
		case 0x0a:
		case 0x0b:
		case 0x0c:
		case 0x0d:
		case 0x0e:
		case 0x0f:
			return 0xfe | spaceod_bg_detect;

		case 0xf8:
		case 0xf9:
		case 0xfa:
		case 0xfb:
			return spaceod_mangled_ports_read(port);

		case 0xfc:
			return spaceod_port_fc_read();
	}

	return 0;
}

static unsigned char sega_decrypt63(UINT16 pc, UINT8 lo)
{
	switch (pc & 0x09)
	{
		case 0x00: return BITSWAP08(lo, 2, 4, 5, 3, 7, 6, 1, 0) ^ 0x80;
		case 0x01: return BITSWAP08(lo, 2, 3, 6, 5, 7, 4, 1, 0) ^ 0x20;
		case 0x08: return BITSWAP08(lo, 2, 7, 3, 4, 6, 5, 1, 0) ^ 0x10;
		case 0x09: return lo;
	}

	return lo;
}

static void SpaceodCallback()
{
	DrvGfxDecode(0x6000, 6);
	GenericTilemapSetGfx(1, DrvGfxROM[0], 6, 8, 8, 0x08000, 0x40, 0);
	GenericTilemapInit(1, spaceod_map_scan, spaceod_map_callback, 8, 8, 128,  32);
	GenericTilemapInit(2, spaceod_map_scan, spaceod_map_callback, 8, 8,  32, 128);
	BurnBitmapAllocate(1, 256, 256, true);
}

static INT32 SpaceodInit()
{
	sound_type = SOUND_SPACEOD;
	has_startlatch = 1;

	return DrvInit(sega_decrypt63, SpaceodCallback, spaceod_port_read, spaceod_port_write);
}

struct BurnDriver BurnDrvSpaceod = {
	"spaceod", NULL, NULL, "spaceod", "1981",
	"Space Odyssey (version 2)\0", NULL, "Sega", "G80 Raster",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_HORSHOOT, 0,
	NULL, spaceodRomInfo, spaceodRomName, NULL, NULL, SpaceodSampleInfo, SpaceodSampleName, SpaceodInputInfo, SpaceodDIPInfo,
	SpaceodInit, DrvExit, DrvFrame, SpaceodDraw, DrvScan, &DrvRecalc, 0x80,
	224, 256, 3, 4
};


// Space Odyssey (version 1)

static struct BurnRomInfo spaceod2RomDesc[] = {
	{ "so-959.cpu-25",			0x0800, 0xbbae3cd1, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "so-941.prom-u1",			0x0800, 0x8b63585a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "so-942.prom-u2",			0x0800, 0x93e7d900, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "so-943.prom-u3",			0x0800, 0xe2f5dc10, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "so-944.prom-u4",			0x0800, 0xb5ab01e9, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "so-945.prom-u5",			0x0800, 0x6c5fa1b1, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "so-946.prom-u6",			0x0800, 0x4cef25d6, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "so-947.prom-u7",			0x0800, 0x7220fc42, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "so-948.prom-u8",			0x0800, 0x94bcd726, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "so-949.prom-u9",			0x0800, 0xe11e7034, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "so-950.prom-u10",		0x0800, 0x70a7a3b4, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "so-951.prom-u11",		0x0800, 0xf5f0d3f9, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "so-952.prom-u12",		0x0800, 0x5bf19a12, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "so-953.prom-u13",		0x0800, 0x8066ac83, 1 | BRF_PRG | BRF_ESS }, // 13
	{ "so-954.prom-u14",		0x0800, 0x44ed6a0d, 1 | BRF_PRG | BRF_ESS }, // 14
	{ "so-955.prom-u15",		0x0800, 0xb5e2748d, 1 | BRF_PRG | BRF_ESS }, // 15
	{ "so-956.prom-u16",		0x0800, 0x97de45a9, 1 | BRF_PRG | BRF_ESS }, // 16
	{ "so-957.prom-u17",		0x0800, 0xc14b98c4, 1 | BRF_PRG | BRF_ESS }, // 17
	{ "so-958.prom-u18",		0x0800, 0x4c0a7242, 1 | BRF_PRG | BRF_ESS }, // 18

	{ "epr-18.bg-u10",			0x1000, 0x24a81c04, 5 | BRF_GRA },           // 19 Tiles
	{ "epr-17.bg-u9",			0x1000, 0x6c7490c0, 5 | BRF_GRA },           // 20
	{ "epr-16.bg-u8",			0x1000, 0xacdf203e, 5 | BRF_GRA },           // 21
	{ "epr-15.bg-u7",			0x1000, 0xae0e5d71, 5 | BRF_GRA },           // 22
	{ "epr-14.bg-u6",			0x1000, 0xd2ebd915, 5 | BRF_GRA },           // 23
	{ "epr-13.bg-u5",			0x1000, 0x74bd7f9a, 5 | BRF_GRA },           // 24

	{ "epr-09.bg-u1",			0x1000, 0xa87bfc0a, 6 | BRF_GRA },           // 25 Tile Map
	{ "epr-10.bg-u2",			0x1000, 0x8ce88100, 6 | BRF_GRA },           // 26
	{ "epr-11.bg-u3",			0x1000, 0x1bdbdab5, 6 | BRF_GRA },           // 27
	{ "epr-12.bg-u4",			0x1000, 0x629a4a1f, 6 | BRF_GRA },           // 28
};

STD_ROM_PICK(spaceod2)
STD_ROM_FN(spaceod2)

struct BurnDriver BurnDrvSpaceod2 = {
	"spaceod2", "spaceod", NULL, "spaceod", "1981",
	"Space Odyssey (version 1)\0", NULL, "Sega", "G80 Raster",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_HORSHOOT, 0,
	NULL, spaceod2RomInfo, spaceod2RomName, NULL, NULL, SpaceodSampleInfo, SpaceodSampleName, SpaceodInputInfo, SpaceodDIPInfo,
	SpaceodInit, DrvExit, DrvFrame, SpaceodDraw, DrvScan, &DrvRecalc, 0x80,
	224, 256, 3, 4
};


// Monster Bash

static struct BurnRomInfo monsterbRomDesc[] = {
	{ "1778.cpu-u25",			0x0800, 0x19761be3, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "1779c.prom-u1",			0x0800, 0x5b67dc4c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "1780b.prom-u2",			0x0800, 0xfac5aac6, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "1781b.prom-u3",			0x0800, 0x3b104103, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "1782b.prom-u4",			0x0800, 0xc1523553, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "1783b.prom-u5",			0x0800, 0xe0ea08c5, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "1784b.prom-u6",			0x0800, 0x48976d11, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "1785b.prom-u7",			0x0800, 0x297d33ae, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "1786b.prom-u8",			0x0800, 0xef94c8f4, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "1787b.prom-u9",			0x0800, 0x1b62994e, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "1788b.prom-u10",			0x0800, 0xa2e32d91, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "1789b.prom-u11",			0x0800, 0x08a172dc, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "1790b.prom-u12",			0x0800, 0x4e320f9d, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "1791b.prom-u13",			0x0800, 0x3b4cba31, 1 | BRF_PRG | BRF_ESS }, // 13
	{ "1792b.prom-u14",			0x0800, 0x7707b9f8, 1 | BRF_PRG | BRF_ESS }, // 14
	{ "1793b.prom-u15",			0x0800, 0xa5d05155, 1 | BRF_PRG | BRF_ESS }, // 15
	{ "1794b.prom-u16",			0x0800, 0xe4813da9, 1 | BRF_PRG | BRF_ESS }, // 16
	{ "1795b.prom-u17",			0x0800, 0x4cd6ed88, 1 | BRF_PRG | BRF_ESS }, // 17
	{ "1796b.prom-u18",			0x0800, 0x9f141a42, 1 | BRF_PRG | BRF_ESS }, // 18
	{ "1797b.prom-u19",			0x0800, 0xec14ad16, 1 | BRF_PRG | BRF_ESS }, // 19
	{ "1798b.prom-u20",			0x0800, 0x86743a4f, 1 | BRF_PRG | BRF_ESS }, // 20
	{ "1799b.prom-u21",			0x0800, 0x41198a83, 1 | BRF_PRG | BRF_ESS }, // 21
	{ "1800b.prom-u22",			0x0800, 0x6a062a04, 1 | BRF_PRG | BRF_ESS }, // 22
	{ "1801b.prom-u23",			0x0800, 0xf38488fe, 1 | BRF_PRG | BRF_ESS }, // 23

	{ "7751.bin",				0x0400, 0x6a9534fc, 2 | BRF_PRG | BRF_ESS }, // 24 UPD7751 Code

	{ "1543snd.bin",			0x1000, 0xb525ce8f, 3 | BRF_SND },           // 28 soundbrd:upd7751
	{ "1544snd.bin",			0x1000, 0x56c79fb0, 3 | BRF_SND },           // 29

	{ "pr1512.u31",				0x0020, 0x414ebe9b, 4 | BRF_SND },           // 30 Sound PROM

	{ "1516.bg-u13",			0x2000, 0xe93a2281, 5 | BRF_GRA },           // 25 Tiles
	{ "1517.bg-u8",				0x2000, 0x1e589101, 5 | BRF_GRA },           // 26

	{ "1518a.bg-u22",			0x2000, 0x2d5932fe, 6 | BRF_GRA },           // 27 Tile Map
};

STD_ROM_PICK(monsterb)
STD_ROM_FN(monsterb)

static struct BurnSampleInfo MonsterbSampleDesc[] = {
	{ "zap",		SAMPLE_NOLOOP },	// 1
	{ "jumpdown",	SAMPLE_NOLOOP },	// 1
	{ "", 0 }
};

STD_SAMPLE_PICK(Monsterb)
STD_SAMPLE_FN(Monsterb)

static void monsterb_write_port(UINT8 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x0c:
		case 0x0d:
		case 0x0e:
		case 0x0f:
			sync_sound();
			ppi8255_w(0, port & 3, data);
		return;

		case 0xbc:
			bg_char_bank = data & 0x0f;
			bg_scrolly = (data << 4) & 0x700;
			bg_enable = data & 0x80;
		return;
	}
}

static UINT8 monsterb_read_port(UINT8 port)
{
	switch (port & 0xff)
	{
		case 0x0c:
		case 0x0d:
		case 0x0e:
		case 0x0f:
			sync_sound();
			return ppi8255_r(0, port & 3);
		case 0xf8:
		case 0xf9:
		case 0xfa:
		case 0xfb:
			return mangled_ports_read(port);

		case 0xfc:
			return DrvInputs[4];
	}

	return 0;
}

static UINT8 sega_decrypt82(UINT16 pc, UINT8 lo)
{
	switch (pc & 0x11)
	{
		case 0x00: return lo;
		case 0x01: return BITSWAP08(lo, 2, 7, 3, 4, 6, 5, 1, 0) ^ 0x10;
		case 0x10: return BITSWAP08(lo, 2, 3, 6, 5, 7, 4, 1, 0) ^ 0x20;
		case 0x11: return BITSWAP08(lo, 2, 4, 5, 3, 7, 6, 1, 0) ^ 0x80;
	}

	return lo;
}

static void monsterb_expand_gfx()
{
	UINT8 *tmp = (UINT8*)BurnMalloc(0x4000);

	memcpy (tmp, DrvGfxROM[0], 0x4000);

	for (INT32 i = 0; i < 16; i++)
	{
		memcpy(DrvGfxROM[0] + 0x0000 + i * 0x800, tmp + 0x0000 + (i & 3) * 0x800, 0x800);
		memcpy(DrvGfxROM[0] + 0x8000 + i * 0x800, tmp + 0x2000 + (i >> 2) * 0x800, 0x800);
	}

	BurnFree (tmp);
}

static void monsterb_sound_a_write(UINT8 data)
{
	tms36xx_note_w(0, data & 0x0f);
	tms3617_enable(DrvSndPROM[(data & 0xF0) >> 4] >> 2);
}

static void monsterb_sound_b_write(UINT8 data)
{
	UINT8 diff = data ^ sound_state[1];
	sound_state[1] = data;

	if ((diff & 0x01) && (~data & 0x01)) { splay(0, 0.25, false, false); }
	if ((diff & 0x02) && (~data & 0x02)) { splay(1, 0.25, false, false); }
}

static void monsterb_upd7751_command_write(UINT8 data)
{
	upd7751_command = data & 0x07;
	mcs48Open(0);
	mcs48SetIRQLine(0, (data & 0x08) ? CPU_IRQSTATUS_NONE : CPU_IRQSTATUS_ACK);
	mcs48Close();
}

static UINT8 monsterb_upd7751_status_read()
{
	return upd7751_busy << 4;
}

static void monsterb_i8243_write(INT32 offset, UINT8 data)
{
	offset &= 3;

	switch (offset & 3)
	{
		case 0: monsterb_sound_addr = (monsterb_sound_addr & ~(0x00f << 0)) | ((data & 0x0f) << 0); break;
		case 1: monsterb_sound_addr = (monsterb_sound_addr & ~(0x00f << 4)) | ((data & 0x0f) << 4); break;
		case 2: monsterb_sound_addr = (monsterb_sound_addr & ~(0x00f << 8)) | ((data & 0x0f) << 8); break;
		case 3: {
			monsterb_sound_addr &= 0xfff;
			if (!(data & 0x01)) monsterb_sound_addr |= 0x0000;
			if (!(data & 0x02)) monsterb_sound_addr |= 0x1000;
		}
		break;
	}
}

static void MonsterbCallback()
{
	monsterb_expand_gfx();
	DrvGfxDecode(0x10000, 2);

	GenericTilemapSetGfx(1, DrvGfxROM[0], 2, 8, 8, 0x40000, 0x40, 0xf);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, monsterbg_map_callback, 8, 8, 32, 256);
	GenericTilemapSetTransparent(0, 0);

	ppi8255_set_read_port(0, 2, monsterb_upd7751_status_read);
	ppi8255_set_write_ports(0, monsterb_sound_a_write, monsterb_sound_b_write, monsterb_upd7751_command_write);
	i8243_write = monsterb_i8243_write;
}

static INT32 MonsterbInit()
{
	sound_type = SOUND_MONSTERB;
	ext_palette_bit = 6;

	return DrvInit(sega_decrypt82, MonsterbCallback, monsterb_read_port, monsterb_write_port);
}

struct BurnDriver BurnDrvMonsterb = {
	"monsterb", NULL, NULL, "monsterb", "1982",
	"Monster Bash\0", NULL, "Sega", "G80 Raster",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_PLATFORM, 0,
	NULL, monsterbRomInfo, monsterbRomName, NULL, NULL, MonsterbSampleInfo, MonsterbSampleName, MonsterbInputInfo, MonsterbDIPInfo,
	MonsterbInit, DrvExit, DrvFrame, MonsterbDraw, DrvScan, &DrvRecalc, 0x80,
	224, 256, 3, 4
};


// Monster Bash (2 board version)

static struct BurnRomInfo monsterb2RomDesc[] = {
	{ "epr-1548.2",				0x2000, 0x239f77c1, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code (Encrypted)
	{ "epr-1549.3",				0x2000, 0x40aeb223, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-1550.13",			0x2000, 0xb42bb2b3, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-1551.14",			0x2000, 0xad7728cc, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "epr-1552.22",			0x2000, 0xe876e216, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "epr-1553.23",			0x2000, 0x4a839fb2, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "7751.34",				0x0400, 0x6a9534fc, 2 | BRF_PRG | BRF_ESS }, //  6 UPD7751 Code

	{ "epr-1543.19",			0x1000, 0xb525ce8f, 3 | BRF_SND },           //  7 soundbrd:upd7751
	{ "epr-1544.23",			0x1000, 0x56c79fb0, 3 | BRF_SND },           //  8

	{ "pr-1542.31",				0x0020, 0x414ebe9b, 4 | BRF_GRA },           //  9 Sound PROM

	{ "epr-1661.19",			0x2000, 0xe93a2281, 5 | BRF_GRA },           // 10 Tiles
	{ "epr-1662.5",				0x2000, 0x1e589101, 5 | BRF_GRA },           // 11

	{ "epr-1554.58",			0x2000, 0xa87937d0, 6 | BRF_GRA },           // 12 Tile Map

	{ "pr-1535.118",			0x0020, 0x087df496, 0 | BRF_OPT },           // 13 Unused PROMs
	{ "pr-1536.128",			0x0020, 0x57c65534, 0 | BRF_OPT },           // 14
	{ "pr-1537.156",			0x0020, 0xe4451c6c, 0 | BRF_OPT },           // 15
	{ "pr-1538.55",				0x0100, 0x025996b1, 0 | BRF_OPT },           // 16
	{ "pr-1539.135",			0x0100, 0xdd18a9ab, 0 | BRF_OPT },           // 17
	{ "pr1540.36",				0x0100, 0xe767ab01, 0 | BRF_OPT },           // 18
	{ "pr1541.145",				0x0100, 0x411aa2a5, 0 | BRF_OPT },           // 19
	{ "pr-5021.39",				0x0020, 0x5222c542, 0 | BRF_OPT },           // 20
};

STD_ROM_PICK(monsterb2)
STD_ROM_FN(monsterb2)

static void monsterb2_port_write(UINT8 port, UINT8 data)
{
	switch (port)
	{
		case 0x0c:
		case 0x0d:
		case 0x0e:
		case 0x0f:
			sync_sound();
			ppi8255_w(0, port & 3, data);
		return;

		case 0xb8: bg_scrollx = (bg_scrollx & 0x300) | data; return;
		case 0xb9: bg_scrollx = (bg_scrollx & 0x0ff) | ((data << 8) & 0x300); bg_enable = data & 0x80; return;
		case 0xba: bg_scrolly = (bg_scrolly & 0x300) | data; return;
		case 0xbb: bg_scrolly = (bg_scrolly & 0x0ff) | ((data << 8) & 0x300); return;
		case 0xbc: bg_char_bank = (data & 0x09) | ((data >> 2) & 0x02) | ((data << 2) & 0x04); return;
	}
}

static UINT8 sega_decrypt0(UINT16,UINT8 lo)
{
	return lo;
}

static void monsterb2_decode()
{
	static const UINT8 convtable[32][4] =
	{
		{ 0x88,0x08,0x80,0x00 }, { 0x00,0x08,0x20,0x28 },   // ...0...0...0...0
		{ 0x28,0xa8,0x08,0x88 }, { 0x28,0xa8,0x08,0x88 },   // ...0...0...0...1
		{ 0x28,0x20,0xa8,0xa0 }, { 0x28,0x20,0xa8,0xa0 },   // ...0...0...1...0
		{ 0x88,0x08,0x80,0x00 }, { 0x88,0x08,0x80,0x00 },   // ...0...0...1...1
		{ 0x00,0x08,0x20,0x28 }, { 0x88,0x08,0x80,0x00 },   // ...0...1...0...0
		{ 0xa0,0x80,0x20,0x00 }, { 0x80,0x88,0x00,0x08 },   // ...0...1...0...1
		{ 0x88,0x08,0x80,0x00 }, { 0xa0,0x80,0x20,0x00 },   // ...0...1...1...0
		{ 0x88,0x08,0x80,0x00 }, { 0x28,0x20,0xa8,0xa0 },   // ...0...1...1...1
		{ 0x28,0xa8,0x08,0x88 }, { 0x80,0x88,0x00,0x08 },   // ...1...0...0...0
		{ 0x80,0x88,0x00,0x08 }, { 0x00,0x08,0x20,0x28 },   // ...1...0...0...1
		{ 0x28,0x20,0xa8,0xa0 }, { 0x28,0xa8,0x08,0x88 },   // ...1...0...1...0
		{ 0x00,0x08,0x20,0x28 }, { 0x80,0xa0,0x88,0xa8 },   // ...1...0...1...1
		{ 0x80,0x88,0x00,0x08 }, { 0xa0,0x80,0x20,0x00 },   // ...1...1...0...0
		{ 0x80,0xa0,0x88,0xa8 }, { 0xa0,0x80,0x20,0x00 },   // ...1...1...0...1
		{ 0xa0,0x80,0x20,0x00 }, { 0x80,0xa0,0x88,0xa8 },   // ...1...1...1...0
		{ 0x28,0x20,0xa8,0xa0 }, { 0x00,0x08,0x20,0x28 }    // ...1...1...1...1
	};

	sega_decode(convtable);
}

static void Monsterb2Callback()
{
	monsterb2_decode();

	monsterb_expand_gfx();
	DrvGfxDecode(0x10000, 2);

	GenericTilemapSetGfx(1, DrvGfxROM[0], 2, 8, 8, 0x40000, 0x40, 0xf);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, monsterbg_map_callback, 8, 8, 128, 64);
	GenericTilemapSetTransparent(0, 0);
	BurnBitmapAllocate(1, 8*128, 8*64, true);

	ppi8255_set_read_port(0, 2, monsterb_upd7751_status_read);
	ppi8255_set_write_ports(0, monsterb_sound_a_write, monsterb_sound_b_write, monsterb_upd7751_command_write);
	i8243_write = monsterb_i8243_write;
}

static INT32 Monsterb2Init()
{
	sound_type = SOUND_MONSTERB;
	ext_palette_bit = 1;
	has_startlatch = 1;

	return DrvInit(sega_decrypt0, Monsterb2Callback, monsterb_read_port, monsterb2_port_write);
}

struct BurnDriver BurnDrvMonsterb2 = {
	"monsterb2", "monsterb", NULL, "monsterb", "1982",
	"Monster Bash (2 board version)\0", NULL, "Sega", "G80 Raster",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_PLATFORM, 0,
	NULL, monsterb2RomInfo, monsterb2RomName, NULL, NULL, MonsterbSampleInfo, MonsterbSampleName, MonsterbInputInfo, MonsterbDIPInfo,
	Monsterb2Init, DrvExit, DrvFrame, PignewtDraw, DrvScan, &DrvRecalc, 0x80,
	224, 256, 3, 4
};


// Pig Newton (version C)

static struct BurnRomInfo pignewtRomDesc[] = {
	{ "cpu.u25",				0x0800, 0xeccc814d, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "1888c.prom-u1",			0x0800, 0xfd18ed09, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "1889c.prom-u2",			0x0800, 0xf633f5ff, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "1890c.prom-u3",			0x0800, 0x22009d7f, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "1891c.prom-u4",			0x0800, 0x1540a7d6, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "1892c.prom-u5",			0x0800, 0x960385d0, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "1893c.prom-u6",			0x0800, 0x58c5c461, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "1894c.prom-u7",			0x0800, 0x5817a59d, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "1895c.prom-u8",			0x0800, 0x812f67d7, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "1896c.prom-u9",			0x0800, 0xcc0ecdd0, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "1897c.prom-u10",			0x0800, 0x7820e93b, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "1898c.prom-u11",			0x0800, 0xe9a10ded, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "1899c.prom-u12",			0x0800, 0xd7ddf02b, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "1900c.prom-u13",			0x0800, 0x8deff4e5, 1 | BRF_PRG | BRF_ESS }, // 13
	{ "1901c.prom-u14",			0x0800, 0x46051305, 1 | BRF_PRG | BRF_ESS }, // 14
	{ "1902c.prom-u15",			0x0800, 0xcb937e19, 1 | BRF_PRG | BRF_ESS }, // 15
	{ "1903c.prom-u16",			0x0800, 0x53239f12, 1 | BRF_PRG | BRF_ESS }, // 16
	{ "1913c.prom-u17",			0x0800, 0x4652cb0c, 1 | BRF_PRG | BRF_ESS }, // 17
	{ "1914c.prom-u18",			0x0800, 0xcb758697, 1 | BRF_PRG | BRF_ESS }, // 18
	{ "1915c.prom-u19",			0x0800, 0x9f3bad66, 1 | BRF_PRG | BRF_ESS }, // 19
	{ "1916c.prom-u20",			0x0800, 0x5bb6f61e, 1 | BRF_PRG | BRF_ESS }, // 20
	{ "1917c.prom-u21",			0x0800, 0x725e2c87, 1 | BRF_PRG | BRF_ESS }, // 21

	{ "1904c.bg",				0x2000, 0xe9de2c8b, 5 | BRF_GRA },           // 22 Tiles
	{ "1905c.bg",				0x2000, 0xaf7cfe0b, 5 | BRF_GRA },           // 23

	{ "1906c.bg",				0x1000, 0xc79d33ce, 6 | BRF_GRA },           // 24 Tile Map
	{ "1907c.bg",				0x1000, 0xbc839d3c, 6 | BRF_GRA },           // 25
	{ "1908c.bg",				0x1000, 0x92cb14da, 6 | BRF_GRA },           // 26
};

STD_ROM_PICK(pignewt)
STD_ROM_FN(pignewt)

static void pignewt_port_write(UINT8 port, UINT8 data)
{
	switch (port)
	{
		case 0x3f:
			sync_sound();
			usb_sound_data_write(data);
		return;
		case 0xb4: pignewt_bg_color_offset = data; return;
		case 0xb8: bg_scrollx = (bg_scrollx & 0x300) | data; return;
		case 0xb9: bg_scrollx = (bg_scrollx & 0x0ff) | ((data << 8) & 0x300); bg_enable = data & 0x80; return;
		case 0xba: bg_scrolly = (bg_scrolly & 0x300) | data; return;
		case 0xbb: bg_scrolly = (bg_scrolly & 0x0ff) | ((data << 8) & 0x300); return;
		case 0xbc: bg_char_bank = (data & 0x09) | ((data >> 2) & 0x02) | ((data << 2) & 0x04); return;
	}
}

static UINT8 pignewt_port_read(UINT8 port)
{
	switch (port)
	{
		case 0x3f:
			sync_sound();
			return usb_sound_status_read();

		case 0xf8:
		case 0xf9:
		case 0xfa:
		case 0xfb:
			return mangled_ports_read(port);

		case 0xfc:
			return DrvInputs[4];
	}
	bprintf(0, _T("unmapped port r %x\n"), port);
	return 0;
}

static void PignewtCallback()
{
	monsterb_expand_gfx();
	DrvGfxDecode(0x10000, 2);

	GenericTilemapSetGfx(1, DrvGfxROM[0], 2, 8, 8, 0x40000, 0x40, 0xf);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, monsterbg_map_callback, 8, 8, 128, 128);
	GenericTilemapSetTransparent(0, 0);
	BurnBitmapAllocate(1, 8*128, 8*128, true);
}

static INT32 PignewtInit()
{
	ext_palette_bit = 1;
	sound_type = SOUND_USB;

	return DrvInit(sega_decrypt63, PignewtCallback, pignewt_port_read, pignewt_port_write);
}

struct BurnDriver BurnDrvPignewt = {
	"pignewt", NULL, NULL, NULL, "1983",
	"Pig Newton (version C)\0", NULL, "Sega", "G80 Raster",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_PLATFORM, 0,
	NULL, pignewtRomInfo, pignewtRomName, NULL, NULL, NULL, NULL, MonsterbInputInfo, PignewtDIPInfo,
	PignewtInit, DrvExit, DrvFrame, PignewtDraw, DrvScan, &DrvRecalc, 0x80,
	224, 256, 3, 4
};


// Pig Newton (version A)

static struct BurnRomInfo pignewtaRomDesc[] = {
	{ "cpu.u25",				0x0800, 0xeccc814d, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "1888a.prom-u1",			0x0800, 0x491c0835, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "1889a.prom-u2",			0x0800, 0x0dcf0af2, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "1890a.prom-u3",			0x0800, 0x640b8b2e, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "1891a.prom-u4",			0x0800, 0x7b8aa07f, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "1892a.prom-u5",			0x0800, 0xafc545cb, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "1893a.prom-u6",			0x0800, 0x82448619, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "1894a.prom-u7",			0x0800, 0x4302dbfb, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "1895a.prom-u8",			0x0800, 0x137ebaaf, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "1896.prom-u9",			0x0800, 0x1604c811, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "1897.prom-u10",			0x0800, 0x3abee406, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "1898.prom-u11",			0x0800, 0xa96410dc, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "1899.prom-u12",			0x0800, 0x612568a5, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "1900.prom-u13",			0x0800, 0x5b231cea, 1 | BRF_PRG | BRF_ESS }, // 13
	{ "1901.prom-u14",			0x0800, 0x3fd74b05, 1 | BRF_PRG | BRF_ESS }, // 14
	{ "1902.prom-u15",			0x0800, 0xd568fc22, 1 | BRF_PRG | BRF_ESS }, // 15
	{ "1903.prom-u16",			0x0800, 0x7d16633b, 1 | BRF_PRG | BRF_ESS }, // 16
	{ "1913.prom-u17",			0x0800, 0xfa4be04f, 1 | BRF_PRG | BRF_ESS }, // 17
	{ "1914.prom-u18",			0x0800, 0x08253c50, 1 | BRF_PRG | BRF_ESS }, // 18
	{ "1915.prom-u19",			0x0800, 0xde786c3b, 1 | BRF_PRG | BRF_ESS }, // 19

	{ "1904a.bg",				0x2000, 0xe9de2c8b, 5 | BRF_GRA },           // 20 Tiles
	{ "1905a.bg",				0x2000, 0xaf7cfe0b, 5 | BRF_GRA },           // 21

	{ "1906a.bg",				0x1000, 0xc79d33ce, 6 | BRF_GRA },           // 22 Tile Map
	{ "1907a.bg",				0x1000, 0xbc839d3c, 6 | BRF_GRA },           // 23
	{ "1908a.bg",				0x1000, 0x92cb14da, 6 | BRF_GRA },           // 24
};

STD_ROM_PICK(pignewta)
STD_ROM_FN(pignewta)

struct BurnDriver BurnDrvPignewta = {
	"pignewta", "pignewt", NULL, NULL, "1983",
	"Pig Newton (version A)\0", NULL, "Sega", "G80 Raster",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_PLATFORM, 0,
	NULL, pignewtaRomInfo, pignewtaRomName, NULL, NULL, NULL, NULL, PignewtaInputInfo, PignewtaDIPInfo,
	PignewtInit, DrvExit, DrvFrame, PignewtDraw, DrvScan, &DrvRecalc, 0x80,
	224, 256, 3, 4
};


// Sindbad Mystery

static struct BurnRomInfo sindbadmRomDesc[] = {
	{ "epr-5393.4d",			0x2000, 0x51f2e51e, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code (Encrypted)
	{ "epr-5394.4e",			0x2000, 0xd39ce2ee, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-5395.4h",			0x2000, 0xb1d15c82, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-5396.4j",			0x2000, 0xea9d40bf, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "epr-5397.4l",			0x2000, 0x595d16dc, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "epr-5398.4m",			0x2000, 0xe57ff63c, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "epr-5400.4a",			0x2000, 0x5114f18e, 2 | BRF_PRG | BRF_ESS }, //  6 Z80 Code (Audio)

	{ "epr-5428.9m",			0x2000, 0xf6044a1e, 5 | BRF_GRA },           //  7 Tiles
	{ "epr-5429.9p",			0x2000, 0xb23eca10, 5 | BRF_GRA },           //  8

	{ "epr-5424.9e",			0x2000, 0x4bfc2e95, 6 | BRF_GRA },           //  9 Tile Map
	{ "epr-5425.9h",			0x2000, 0xb654841a, 6 | BRF_GRA },           // 10
	{ "epr-5426.9j",			0x2000, 0x9de0da28, 6 | BRF_GRA },           // 11
	{ "epr-5427.9l",			0x2000, 0xa94f4d41, 6 | BRF_GRA },           // 12
};

STD_ROM_PICK(sindbadm)
STD_ROM_FN(sindbadm)

// ancient mame does not emulate the ppi for this, it directly writes to sound and reads input4 (fc)

static void sindbadm_write_port(UINT8 port, UINT8 data)
{
	switch (port)
	{
		case 0x40:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE); // ack main cpu irq
		return;

		case 0x41:
			bg_enable = data & 0x80;
			bg_scrollx = (data << 6) & 0x300;
			bg_scrolly = (data << 4) & 0x700;
			bg_char_bank = data & 0x03;
		return;

		case 0x43: // hack
			video_control |= 4;
		return;

		case 0x80:
			soundlatch = data;
			ZetNmi(1);
		return;
		//case 0x81:
		//case 0x82:
		//case 0x83:
		//	ppi8255_w(0, port & 3, data);
		//return;
	}
}

static UINT8 sindbadm_read_port(UINT8 port)
{
	switch (port)
	{
		case 0x81:
			return DrvInputs[4];
	//	case 0x80:
	//	case 0x81:
	//	case 0x82:
	//	case 0x83:
	//		return ppi8255_r(0, port & 3);
	
		case 0xf8:
		case 0xf9:
		case 0xfa:
		case 0xfb:
			return mangled_ports_read(port);
	}

	return 0;
}

static void sindbadm_decode()
{
	static const UINT8 convtable[32][4] =
	{
		{ 0x28,0xa8,0x08,0x88 }, { 0x88,0x80,0x08,0x00 },   // ...0...0...0...0
		{ 0xa8,0xa0,0x88,0x80 }, { 0x00,0x20,0x80,0xa0 },   // ...0...0...0...1
		{ 0xa8,0xa0,0x88,0x80 }, { 0x00,0x20,0x80,0xa0 },   // ...0...0...1...0
		{ 0x28,0xa8,0x08,0x88 }, { 0x88,0x80,0x08,0x00 },   // ...0...0...1...1
		{ 0xa8,0x88,0xa0,0x80 }, { 0xa0,0x20,0xa8,0x28 },   // ...0...1...0...0
		{ 0x28,0xa8,0x08,0x88 }, { 0x88,0x80,0x08,0x00 },   // ...0...1...0...1
		{ 0xa8,0xa0,0x88,0x80 }, { 0x00,0x20,0x80,0xa0 },   // ...0...1...1...0
		{ 0xa8,0xa0,0x88,0x80 }, { 0x00,0x20,0x80,0xa0 },   // ...0...1...1...1
		{ 0x28,0xa8,0x08,0x88 }, { 0x88,0x80,0x08,0x00 },   // ...1...0...0...0
		{ 0x28,0xa8,0x08,0x88 }, { 0x88,0x80,0x08,0x00 },   // ...1...0...0...1
		{ 0xa8,0xa0,0x88,0x80 }, { 0x00,0x20,0x80,0xa0 },   // ...1...0...1...0
		{ 0xa8,0xa0,0x88,0x80 }, { 0x00,0x20,0x80,0xa0 },   // ...1...0...1...1
		{ 0x28,0xa8,0x08,0x88 }, { 0x88,0x80,0x08,0x00 },   // ...1...1...0...0
		{ 0xa8,0x88,0xa0,0x80 }, { 0xa0,0x20,0xa8,0x28 },   // ...1...1...0...1
		{ 0x28,0xa8,0x08,0x88 }, { 0x88,0x80,0x08,0x00 },   // ...1...1...1...0
		{ 0x28,0xa8,0x08,0x88 }, { 0x88,0x80,0x08,0x00 }    // ...1...1...1...1
	};

	sega_decode(convtable);
}

static void SindbadmCallback()
{
	sindbadm_decode();

	DrvGfxDecode(0x4000, 2);

	GenericTilemapSetGfx(1, DrvGfxROM[0], 2, 8, 8, 0x10000, 0x40, 0xf);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, monsterbg_map_callback, 8, 8, 128, 256);
	GenericTilemapSetTransparent(0, 0);
}

static INT32 SindbadmInit()
{
	sound_type = SOUND_Z80;
	ext_palette_bit = 9; // special palette writing

	return DrvInit(sega_decrypt0, SindbadmCallback, sindbadm_read_port, sindbadm_write_port);
}

struct BurnDriver BurnDrvSindbadm = {
	"sindbadm", NULL, NULL, NULL, "1983",
	"Sindbad Mystery\0", NULL, "Sega", "G80 Raster",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_SEGA_MISC, GBF_MAZE, 0,
	NULL, sindbadmRomInfo, sindbadmRomName, NULL, NULL, NULL, NULL, SindbadmInputInfo, SindbadmDIPInfo,
	SindbadmInit, DrvExit, DrvFrame, MonsterbDraw, DrvScan, &DrvRecalc, 0x80,
	224, 256, 3, 4
};
