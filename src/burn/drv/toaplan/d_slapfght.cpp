// FB Alpha Slap Fight driver module
// Based on MAME driver by K.Wilkins and Stephane Humbert

#include "tiles_generic.h"
#include "z80_intf.h"
#include "taito_m68705.h"
#include "ay8910.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvMCUROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvVidRAM;
static UINT8 *DrvColRAM;
static UINT8 *DrvTxtRAM;
static UINT8 *DrvTxcRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprBuf;
static UINT8 *DrvShareRAM;
static UINT8 *DrvMCURAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 sound_nmi_enable;
static INT32 irq_enable;
static INT32 palette_bank;
static INT32 flipscreen;
static INT32 scrollx;
static INT32 scrolly;
static INT32 bankdata;
static INT32 protection_counter;
static INT32 protection_data;

static INT32 vblank;
static INT32 has_mcu = 0;
static INT32 has_banks = 0;
static INT32 nSndIrqFrame = 0; //tigerh 6, slapf 3, perfr 4

static INT32 cpu_clock = 6000000;
static void (*pMCUWrite)(INT32) = NULL;
static INT32 (*pMCURead)() = NULL;
static INT32 (*pMCUStatusRead)() = NULL;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static struct BurnInputInfo SlapfighInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 4,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 5,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Slapfigh)

static struct BurnInputInfo TigerhInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 4,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 5,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Tigerh)

static struct BurnInputInfo Getstarb2InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 4,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 6,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Getstarb2)

static struct BurnDIPInfo PerfrmanDIPList[]=
{
	{0x11, 0xff, 0xff, 0xef, NULL							},
	{0x12, 0xff, 0xff, 0x77, NULL							},

	{0   , 0xfe, 0   ,    7, "Coinage"						},
	{0x11, 0x01, 0x07, 0x02, "3 Coins 1 Credits"			},
	{0x11, 0x01, 0x07, 0x04, "2 Coins 1 Credits"			},
	{0x11, 0x01, 0x07, 0x07, "1 Coin  1 Credits"			},
	{0x11, 0x01, 0x07, 0x03, "2 Coins 3 Credits"			},
	{0x11, 0x01, 0x07, 0x06, "1 Coin  2 Credits"			},
	{0x11, 0x01, 0x07, 0x05, "1 Coin  3 Credits"			},
	{0x11, 0x01, 0x07, 0x00, "Free Play"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x11, 0x01, 0x08, 0x00, "Off"							},
	{0x11, 0x01, 0x08, 0x08, "On"							},

	{0   , 0xfe, 0   ,    2, "Cabinet"						},
	{0x11, 0x01, 0x10, 0x00, "Upright"						},
	{0x11, 0x01, 0x10, 0x10, "Cocktail"						},

	{0   , 0xfe, 0   ,    2, "Intermissions"				},
	{0x11, 0x01, 0x20, 0x20, "Off"							},
	{0x11, 0x01, 0x20, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Service Mode"					},
	{0x11, 0x01, 0x40, 0x00, "On"							},
	{0x11, 0x01, 0x40, 0x40, "Off"							},

	{0   , 0xfe, 0   ,    4, "Lives"						},
	{0x12, 0x01, 0x03, 0x01, "1"							},
	{0x12, 0x01, 0x03, 0x00, "2"							},
	{0x12, 0x01, 0x03, 0x03, "3"							},
	{0x12, 0x01, 0x03, 0x02, "5"							},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x12, 0x01, 0x0c, 0x0c, "Easy"							},
	{0x12, 0x01, 0x0c, 0x08, "Medium"						},
	{0x12, 0x01, 0x0c, 0x04, "Hard"							},
	{0x12, 0x01, 0x0c, 0x00, "Hardest"						},

	{0   , 0xfe, 0   ,    16, "Bonus Life"					},
	{0x12, 0x01, 0xf0, 0xb0, "20k, 120k, then every 100k"	},
	{0x12, 0x01, 0xf0, 0xa0, "40k, 120k, then every 100k"	},
	{0x12, 0x01, 0xf0, 0x90, "60k, 160k, then every 100k"	},
	{0x12, 0x01, 0xf0, 0x80, "Every 100k"					},
	{0x12, 0x01, 0xf0, 0x70, "20k, 220k, then every 200k"	},
	{0x12, 0x01, 0xf0, 0x60, "40k, 240k, then every 200k"	},
	{0x12, 0x01, 0xf0, 0x50, "60k, 260k, then every 200k"	},
	{0x12, 0x01, 0xf0, 0x40, "Every 200k"					},
	{0x12, 0x01, 0xf0, 0x30, "20k, 320k, then every 300k"	},
	{0x12, 0x01, 0xf0, 0x20, "40k, 340k, then every 300k"	},
	{0x12, 0x01, 0xf0, 0x10, "60k, 360k, then every 300k"	},
	{0x12, 0x01, 0xf0, 0x00, "Every 300k"					},
	{0x12, 0x01, 0xf0, 0xf0, "20k only"						},
	{0x12, 0x01, 0xf0, 0xe0, "40k only"						},
	{0x12, 0x01, 0xf0, 0xd0, "60k only"						},
	{0x12, 0x01, 0xf0, 0xc0, "None"							},
};

STDDIPINFO(Perfrman)

static struct BurnDIPInfo TigerhDIPList[]=
{
	{0x11, 0xff, 0xff, 0x6f, NULL							},
	{0x12, 0xff, 0xff, 0xeb, NULL							},

	{0   , 0xfe, 0   ,    7, "Coinage"						},
	{0x11, 0x01, 0x07, 0x02, "3 Coins 1 Credits"			},
	{0x11, 0x01, 0x07, 0x04, "2 Coins 1 Credits"			},
	{0x11, 0x01, 0x07, 0x07, "1 Coin  1 Credits"			},
	{0x11, 0x01, 0x07, 0x03, "2 Coins 3 Credits"			},
	{0x11, 0x01, 0x07, 0x06, "1 Coin  2 Credits"			},
	{0x11, 0x01, 0x07, 0x05, "1 Coin  3 Credits"			},
	{0x11, 0x01, 0x07, 0x00, "Free Play"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x11, 0x01, 0x08, 0x00, "Off"							},
	{0x11, 0x01, 0x08, 0x08, "On"							},

	{0   , 0xfe, 0   ,    2, "Cabinet"						},
	{0x11, 0x01, 0x10, 0x00, "Upright"						},
	{0x11, 0x01, 0x10, 0x10, "Cocktail"						},

	{0   , 0xfe, 0   ,    2, "Flip Screen"					},
	{0x11, 0x01, 0x20, 0x20, "Off"							},
	{0x11, 0x01, 0x20, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Service Mode"					},
	{0x11, 0x01, 0x40, 0x00, "On"							},
	{0x11, 0x01, 0x40, 0x40, "Off"							},

	{0   , 0xfe, 0   ,    2, "Player Speed"					},
	{0x11, 0x01, 0x80, 0x80, "Normal"						},
	{0x11, 0x01, 0x80, 0x00, "Fast"							},

	{0   , 0xfe, 0   ,    4, "Lives"						},
	{0x12, 0x01, 0x03, 0x01, "1"							},
	{0x12, 0x01, 0x03, 0x00, "2"							},
	{0x12, 0x01, 0x03, 0x03, "3"							},
	{0x12, 0x01, 0x03, 0x02, "5"							},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x12, 0x01, 0x0c, 0x0c, "Easy"							},
	{0x12, 0x01, 0x0c, 0x08, "Medium"						},
	{0x12, 0x01, 0x0c, 0x04, "Hard"							},
	{0x12, 0x01, 0x0c, 0x00, "Hardest"						},

	{0   , 0xfe, 0   ,    2, "Bonus Life"					},
	{0x12, 0x01, 0x10, 0x10, "20k and every 80k"			},
	{0x12, 0x01, 0x10, 0x00, "50k and every 120k"			},
};

STDDIPINFO(Tigerh)

static struct BurnDIPInfo SlapfighDIPList[]=
{
	{0x11, 0xff, 0xff, 0x7f, NULL							},
	{0x12, 0xff, 0xff, 0xff, NULL							},

	{0   , 0xfe, 0   ,    4, "Coin B"						},
	{0x11, 0x01, 0x03, 0x02, "2 Coins 1 Credits"			},
	{0x11, 0x01, 0x03, 0x03, "1 Coin  1 Credits"			},
	{0x11, 0x01, 0x03, 0x00, "2 Coins 3 Credits"			},
	{0x11, 0x01, 0x03, 0x01, "1 Coin  2 Credits"			},

	{0   , 0xfe, 0   ,    4, "Coin A"						},
	{0x11, 0x01, 0x0c, 0x08, "2 Coins 1 Credits"			},
	{0x11, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"			},
	{0x11, 0x01, 0x0c, 0x00, "2 Coins 3 Credits"			},
	{0x11, 0x01, 0x0c, 0x04, "1 Coin  2 Credits"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x11, 0x01, 0x10, 0x00, "Off"							},
	{0x11, 0x01, 0x10, 0x10, "On"							},

	{0   , 0xfe, 0   ,    2, "Screen Test"					},
	{0x11, 0x01, 0x20, 0x20, "Off"							},
	{0x11, 0x01, 0x20, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Flip Screen"					},
	{0x11, 0x01, 0x40, 0x40, "Off"							},
	{0x11, 0x01, 0x40, 0x00, "On"							},
			
	{0   , 0xfe, 0   ,    2, "Cabinet"						},
	{0x11, 0x01, 0x80, 0x00, "Upright"						},
	{0x11, 0x01, 0x80, 0x80, "Cocktail"						},

	{0   , 0xfe, 0   ,    2, "Service Mode"					},
	{0x12, 0x01, 0x02, 0x02, "Off"							},
	{0x12, 0x01, 0x02, 0x00, "On"							},

	{0   , 0xfe, 0   ,    4, "Lives"						},
	{0x12, 0x01, 0x0c, 0x08, "1"							},
	{0x12, 0x01, 0x0c, 0x00, "2"							},
	{0x12, 0x01, 0x0c, 0x0c, "3"							},
	{0x12, 0x01, 0x0c, 0x04, "5"							},

	{0   , 0xfe, 0   ,    4, "Bonus Life"					},
	{0x12, 0x01, 0x30, 0x30, "30000 100000"					},
	{0x12, 0x01, 0x30, 0x10, "50000 200000"					},
	{0x12, 0x01, 0x30, 0x20, "50000"						},
	{0x12, 0x01, 0x30, 0x00, "100000"						},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x12, 0x01, 0xc0, 0x40, "Easy"							},
	{0x12, 0x01, 0xc0, 0xc0, "Medium"						},
	{0x12, 0x01, 0xc0, 0x80, "Hard"							},
	{0x12, 0x01, 0xc0, 0x00, "Hardest"						},
};

STDDIPINFO(Slapfigh)

static struct BurnDIPInfo GetstarDIPList[]=
{
	{0x11, 0xff, 0xff, 0xef, NULL							},
	{0x12, 0xff, 0xff, 0xf7, NULL							},

	{0   , 0xfe, 0   ,    7, "Coinage"						},
	{0x11, 0x01, 0x07, 0x02, "3 Coins 1 Credits"			},
	{0x11, 0x01, 0x07, 0x04, "2 Coins 1 Credits"			},
	{0x11, 0x01, 0x07, 0x07, "1 Coin  1 Credits"			},
	{0x11, 0x01, 0x07, 0x03, "2 Coins 3 Credits"			},
	{0x11, 0x01, 0x07, 0x06, "1 Coin  2 Credits"			},
	{0x11, 0x01, 0x07, 0x05, "1 Coin  3 Credits"			},
	{0x11, 0x01, 0x07, 0x00, "Free Play"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x11, 0x01, 0x08, 0x00, "Off"							},
	{0x11, 0x01, 0x08, 0x08, "On"							},

	{0   , 0xfe, 0   ,    2, "Cabinet"						},
	{0x11, 0x01, 0x10, 0x00, "Upright"						},
	{0x11, 0x01, 0x10, 0x10, "Cocktail"						},

	{0   , 0xfe, 0   ,    2, "Flip Screen"					},
	{0x11, 0x01, 0x20, 0x20, "Off"							},
	{0x11, 0x01, 0x20, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Service Mode"					},
	{0x11, 0x01, 0x40, 0x40, "Off"							},
	{0x11, 0x01, 0x40, 0x00, "On"							},

	{0   , 0xfe, 0   ,    4, "Lives"						},
	{0x12, 0x01, 0x03, 0x01, "1"							},
	{0x12, 0x01, 0x03, 0x00, "2"							},
	{0x12, 0x01, 0x03, 0x03, "3"							},
	{0x12, 0x01, 0x03, 0x02, "5"							},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x12, 0x01, 0x0c, 0x0c, "Easy"							},
	{0x12, 0x01, 0x0c, 0x08, "Medium"						},
	{0x12, 0x01, 0x0c, 0x04, "Hard"							},
	{0x12, 0x01, 0x0c, 0x00, "Hardest"						},

	{0   , 0xfe, 0   ,    4, "Bonus Life"					},
	{0x12, 0x01, 0x30, 0x30, "50k only"						},
	{0x12, 0x01, 0x30, 0x20, "100k only"					},
	{0x12, 0x01, 0x30, 0x10, "150k only"					},
	{0x12, 0x01, 0x30, 0x00, "200k only"					},
};

STDDIPINFO(Getstar)

static struct BurnDIPInfo GetstarjDIPList[]=
{
	{0x11, 0xff, 0xff, 0xef, NULL							},
	{0x12, 0xff, 0xff, 0xff, NULL							},

	{0   , 0xfe, 0   ,    7, "Coinage"						},
	{0x11, 0x01, 0x07, 0x02, "3 Coins 1 Credits"			},
	{0x11, 0x01, 0x07, 0x04, "2 Coins 1 Credits"			},
	{0x11, 0x01, 0x07, 0x07, "1 Coin  1 Credits"			},
	{0x11, 0x01, 0x07, 0x03, "2 Coins 3 Credits"			},
	{0x11, 0x01, 0x07, 0x06, "1 Coin  2 Credits"			},
	{0x11, 0x01, 0x07, 0x05, "1 Coin  3 Credits"			},
	{0x11, 0x01, 0x07, 0x00, "Free Play"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x11, 0x01, 0x08, 0x00, "Off"							},
	{0x11, 0x01, 0x08, 0x08, "On"							},

	{0   , 0xfe, 0   ,    2, "Cabinet"						},
	{0x11, 0x01, 0x10, 0x00, "Upright"						},
	{0x11, 0x01, 0x10, 0x10, "Cocktail"						},

	{0   , 0xfe, 0   ,    2, "Flip Screen"					},
	{0x11, 0x01, 0x20, 0x20, "Off"							},
	{0x11, 0x01, 0x20, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Service Mode"					},
	{0x11, 0x01, 0x40, 0x40, "Off"							},
	{0x11, 0x01, 0x40, 0x00, "On"							},

	{0   , 0xfe, 0   ,    4, "Lives"						},
	{0x12, 0x01, 0x03, 0x01, "1"							},
	{0x12, 0x01, 0x03, 0x00, "2"							},
	{0x12, 0x01, 0x03, 0x03, "3"							},
	{0x12, 0x01, 0x03, 0x02, "5"							},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x12, 0x01, 0x0c, 0x0c, "Easy"							},
	{0x12, 0x01, 0x0c, 0x08, "Medium"						},
	{0x12, 0x01, 0x0c, 0x04, "Hard"							},
	{0x12, 0x01, 0x0c, 0x00, "Hardest"						},

	{0   , 0xfe, 0   ,    2, "Bonus Life"					},
	{0x12, 0x01, 0x10, 0x10, "30k, 130k, then every 100k"	},
	{0x12, 0x01, 0x10, 0x00, "50k, 200k, then every 150k"	},
};

STDDIPINFO(Getstarj)

static struct BurnDIPInfo Getstarb2DIPList[]=
{
	{0x11, 0xff, 0xff, 0xef, NULL							},
	{0x12, 0xff, 0xff, 0xf6, NULL							},

	{0   , 0xfe, 0   ,    7, "Coinage"						},
	{0x11, 0x01, 0x07, 0x02, "3 Coins 1 Credits"			},
	{0x11, 0x01, 0x07, 0x04, "2 Coins 1 Credits"			},
	{0x11, 0x01, 0x07, 0x07, "1 Coin  1 Credits"			},
	{0x11, 0x01, 0x07, 0x03, "2 Coins 3 Credits"			},
	{0x11, 0x01, 0x07, 0x06, "1 Coin  2 Credits"			},
	{0x11, 0x01, 0x07, 0x05, "1 Coin  3 Credits"			},
	{0x11, 0x01, 0x07, 0x00, "Free Play"					},
	
	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x11, 0x01, 0x08, 0x00, "Off"							},
	{0x11, 0x01, 0x08, 0x08, "On"							},

	{0   , 0xfe, 0   ,    2, "Cabinet"						},
	{0x11, 0x01, 0x10, 0x00, "Upright"						},
	{0x11, 0x01, 0x10, 0x10, "Cocktail"						},

	{0   , 0xfe, 0   ,    2, "Flip Screen"					},
	{0x11, 0x01, 0x20, 0x20, "Off"							},
	{0x11, 0x01, 0x20, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Service Mode"					},
	{0x11, 0x01, 0x40, 0x40, "Off"							},
	{0x11, 0x01, 0x40, 0x00, "On"							},

	{0   , 0xfe, 0   ,    4, "Lives"						},
	{0x12, 0x01, 0x03, 0x02, "3"							},
	{0x12, 0x01, 0x03, 0x01, "4"							},
	{0x12, 0x01, 0x03, 0x00, "5"							},
	{0x12, 0x01, 0x03, 0x03, "240 (Cheat)"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x12, 0x01, 0x0c, 0x0c, "Easy"							},
	{0x12, 0x01, 0x0c, 0x08, "Medium"						},
	{0x12, 0x01, 0x0c, 0x04, "Hard"							},
	{0x12, 0x01, 0x0c, 0x00, "Hardest"						},

	{0   , 0xfe, 0   ,    4, "Bonus Life"					},
	{0x12, 0x01, 0x30, 0x30, "50k only"						},
	{0x12, 0x01, 0x30, 0x20, "100k only"					},
	{0x12, 0x01, 0x30, 0x10, "150k only"					},
	{0x12, 0x01, 0x30, 0x00, "200k only"					},
};

STDDIPINFO(Getstarb2)

static inline void sync_mcu() // slap fight hangs without syncing the mcu on writes
{
	INT32 cycles = (ZetTotalCycles() * (3000000 / 1000000)) / (cpu_clock / 1000000);

	cycles -= m6805TotalCycles();

	if (cycles > 0)	m6805Run(cycles);
}

static void __fastcall slapfigh_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xe800:
			scrollx = (scrollx & 0xff00) | data;
		return;

		case 0xe801:
			scrollx = (scrollx & 0x00ff) | (data << 8);
		return;

		case 0xe802:
			scrolly = data;
		return;

		case 0xe803:
			if (pMCUWrite) {
				if (has_mcu) sync_mcu();
				pMCUWrite(data);
				return;
			}
		return;
	}
}

static void __fastcall slapfighb2_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xe800:
			scrollx = (scrollx & 0x00ff) | (data << 8);
		return;

		case 0xe802:
			scrolly = data;
		return;

		case 0xe803:
			scrollx = (scrollx & 0xff00) | data;
		return;
	}
}

static UINT8 __fastcall slapfigh_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xe803:
			if (pMCURead) {
				return pMCURead();
			}
			return 0;
	}

	return 0;
}

static void bankswitch(INT32 bank)
{
	if (has_banks == 0) return;

	bankdata = bank;

	ZetMapMemory(DrvZ80ROM0 + 0x8000 + (bank * 0x4000), 0x8000, 0xbfff, MAP_ROM);
}

static void __fastcall perfrman_write_port(UINT16 address, UINT8 data)
{
	switch (address & 0xff)
	{
		case 0x00:
		{
			sound_nmi_enable = 0;

			ZetSetRESETLine(1, 1);
		}
		return;

		case 0x01:
			ZetSetRESETLine(1, 0);
		return;

		case 0x02:
		case 0x03:
			flipscreen = ~address & 1;
		return;

		case 0x05:
		return; // nop?

		case 0x06:
			irq_enable = 0;
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;

		case 0x07:
			irq_enable = 1;
		return;

		case 0x08:
		case 0x09:
			if (has_banks) bankswitch(address & 1);
		return;

		case 0x0c:
		case 0x0d:
			if (has_banks == 0) palette_bank = address & 1;
		return;

		case 0x0e:
		case 0x0f:
		return; // nop
	}
}

static UINT8 __fastcall perfrman_read_port(UINT16 address)
{
	switch (address & 0xff)
	{
		case 0x00: {
			if (pMCUStatusRead) {
				return pMCUStatusRead();
			}
			return vblank ? 1 : 0;
		}
	}

	return 0;
}

static void __fastcall perfrman_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xa080:
		case 0xa082:
		case 0xa090:
		case 0xa092:
			AY8910Write((address >> 4) & 1, (address >> 1) & 1, data);
		return;

		case 0xa0e0:
			sound_nmi_enable = 1;
		return;

		case 0xa0f0:
			sound_nmi_enable = 0;
		return;
	}
}

static UINT8 __fastcall perfrman_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xa081:
		case 0xa091:
			return AY8910Read((address >> 4) & 1);
	}

	return 0;
}

static INT32 slapfigh_mcu_status_read()
{
	UINT8 ret = vblank ? 1 : 0;
	if (!main_sent) ret |= 2;
	if (!mcu_sent) ret |= 4;
	return ret;
}

static void slapfigh_m68705_portB_out(UINT8 *data)
{
	standard_m68705_portB_out(data);

	if ((ddrB & 0x08) && (~*data & 0x08) && (portB_out & 0x08))
	{
		scrollx = (scrollx & 0xff00) | portA_out;
	}
	if ((ddrB & 0x10) && (~*data & 0x10) && (portB_out & 0x10))
	{
		scrollx = (scrollx & 0x00ff) | (portA_out << 8);
	}
}

static m68705_interface slapfigh_m68705_interface = {
	NULL, slapfigh_m68705_portB_out, NULL,
	NULL, NULL, NULL,
	NULL, NULL, standard_m68705_portC_in
};

static void tigerh_m68705_portC_in()
{
	portC_in = 0;
	if (!main_sent) portC_in |= 0x01; // inverted from normal
	if (  mcu_sent) portC_in |= 0x02;
}

static m68705_interface tigerh_m68705_interface = {
	NULL, standard_m68705_portB_out, NULL,
	NULL, NULL, NULL,
	NULL, NULL, tigerh_m68705_portC_in
};

static UINT8 read_input0(UINT32)
{
	return DrvInputs[0];
}

static UINT8 read_input1(UINT32)
{
	return DrvInputs[1];
}

static UINT8 read_dip0(UINT32)
{
	return DrvDips[0];
}

static UINT8 read_dip1(UINT32)
{
	return DrvDips[1];
}

static tilemap_callback( perfrman )
{
	INT32 attr  = DrvColRAM[offs];
	INT32 code  = DrvVidRAM[offs] + (attr << 8);
	INT32 color = attr >> 3;

	TILE_SET_INFO(0, code, color, 0);
}

static tilemap_callback( bg )
{
	INT32 attr  = DrvColRAM[offs];
	INT32 code  = DrvVidRAM[offs] + (attr << 8);
	INT32 color = attr >> 4;

	TILE_SET_INFO(1, code, color, 0);
}

static tilemap_callback( tx )
{
	INT32 attr  = DrvTxcRAM[offs];
	INT32 code  = DrvTxtRAM[offs] + (attr << 8);
	INT32 color = attr >> 2;

	TILE_SET_INFO(0, code, color, 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	bankswitch(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	ZetClose();

	if (has_mcu) m67805_taito_reset();

	AY8910Reset(0);
	AY8910Reset(1);

	sound_nmi_enable = 0;
	irq_enable = 0;
	palette_bank = 0;
	flipscreen = 0;
	scrollx = 0;
	scrolly = 0;
	protection_counter = 0;
	protection_data = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x012000;
	DrvZ80ROM1		= Next; Next += 0x002000;

	DrvMCUROM		= Next; Next += 0x000800;

	DrvGfxROM0		= Next; Next += 0x010000;
	DrvGfxROM1		= Next; Next += 0x040000;
	DrvGfxROM2		= Next; Next += 0x040000;

	DrvColPROM		= Next; Next += 0x000300;

	DrvPalette		= (UINT32*)Next; Next += 0x100 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM0		= Next; Next += 0x000800;
	DrvZ80RAM1		= Next; Next += 0x003000;
	DrvVidRAM		= Next; Next += 0x000800;
	DrvColRAM		= Next; Next += 0x000800;
	DrvSprRAM		= Next; Next += 0x000800;
	DrvShareRAM		= Next; Next += 0x000800;
	DrvTxtRAM		= Next; Next += 0x000800;
	DrvTxcRAM		= Next; Next += 0x000800;
	DrvMCURAM		= Next; Next += 0x000080;
	DrvSprBuf		= Next; Next += 0x000800;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode(INT32 gfx1size)
{
	INT32 Plane0[3]  = { 0x2000*8*0, 0x2000*8*1, 0x2000*8*2 };
	INT32 Plane1[4]  = { STEP4(0, (gfx1size/4)*8) };
	INT32 XOffs0[16] = { STEP16(0,1) };
	INT32 YOffs0[8]  = { STEP8(0,8) };
	INT32 YOffs1[16] = { STEP16(0,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(gfx1size);
	if (tmp == NULL) {
		return 1;
	}

	if (gfx1size > 0x6000) // 4bpp
	{
		memcpy (tmp, DrvGfxROM0, 0x4000);

		GfxDecode(0x0400, 2,  8,  8, Plane0, XOffs0, YOffs0, 0x040, tmp, DrvGfxROM0);

		memcpy (tmp, DrvGfxROM1, gfx1size);

		GfxDecode((gfx1size*2)/64, 4,  8,  8, Plane1, XOffs0, YOffs0, 0x040, tmp, DrvGfxROM1);

		memcpy (tmp, DrvGfxROM2, gfx1size);

		GfxDecode((gfx1size*2)/256, 4, 16, 16, Plane1, XOffs0, YOffs1, 0x100, tmp, DrvGfxROM2);

		if (gfx1size == 0x10000) { // mirror data
			memcpy (DrvGfxROM1 + 0x20000, DrvGfxROM1, 0x20000);
			memcpy (DrvGfxROM2 + 0x20000, DrvGfxROM2, 0x20000);
		}
	}
	else
	{
		memcpy (tmp, DrvGfxROM0, 0x6000);

		GfxDecode(0x0400, 3,  8,  8, Plane0, XOffs0, YOffs0, 0x040, tmp, DrvGfxROM0);

		memcpy (tmp, DrvGfxROM1, 0x6000);

		GfxDecode(0x0100, 3, 16, 16, Plane0, XOffs0, YOffs1, 0x100, tmp, DrvGfxROM1);
	}

	BurnFree(tmp);

	return 0;
}

static INT32 DrvLoadRoms()
{
	char* pRomName;
	struct BurnRomInfo ri;

	UINT8 *pLoad[3] = { DrvZ80ROM0, DrvZ80ROM1, DrvMCUROM };
	UINT8 *gLoad[3] = { DrvGfxROM0, DrvGfxROM1, DrvGfxROM2 };
	UINT8 *cLoad = DrvColPROM;

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++)
	{
		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & 7) == 1 || (ri.nType & 7) == 2 || (ri.nType & 7) == 3) {
			INT32 type = (ri.nType & 3) - 1;
			if (BurnLoadRom(pLoad[type], i, 1)) return 1;
			pLoad[type] += ri.nLen;
			continue;
		}

		if ((ri.nType & 7) == 4 || (ri.nType & 7) == 5 || (ri.nType & 7) == 6) {
			INT32 type = (ri.nType & 3) - 0;
			if (BurnLoadRom(gLoad[type], i, 1)) return 1;
			gLoad[type] += ri.nLen;
			continue;
		}

		if ((ri.nType & 7) == 7) {
			if (BurnLoadRom(cLoad, i, 1)) return 1;
			cLoad += ri.nLen;
			continue;
		}	
	}

	if ((pLoad[2] - DrvMCUROM) != 0) has_mcu = 1;
	if ((pLoad[0] - DrvZ80ROM0) > 0xc000) has_banks = 1;

	DrvGfxDecode(gLoad[1] - DrvGfxROM1);

	return 0;
}

static void sound_init(INT32 sound_clock)
{
	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,			0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvShareRAM,			0x8800, 0x8fff, MAP_RAM); // perfrman
	ZetMapMemory(DrvShareRAM,			0xc800, 0xcfff, MAP_RAM); 
	ZetMapMemory(DrvZ80RAM1,			0xd000, 0xffff, MAP_RAM);
	ZetSetWriteHandler(perfrman_sound_write);
	ZetSetReadHandler(perfrman_sound_read);
	ZetClose();

	AY8910Init(0, sound_clock, 0);
	AY8910Init(1, sound_clock, 0);
	AY8910SetPorts(0, &read_input0, &read_input1, NULL, NULL);
	AY8910SetPorts(1, &read_dip0, &read_dip1, NULL, NULL);
	AY8910SetAllRoutes(0, 0.25, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.25, BURN_SND_ROUTE_BOTH);
}

static INT32 PerfrmanInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (DrvLoadRoms()) return 1;

	cpu_clock = 4000000;

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,			0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,			0x8000, 0x87ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,			0x8800, 0x8fff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,				0x9000, 0x97ff, MAP_RAM);
	ZetMapMemory(DrvColRAM,				0x9800, 0x9fff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,				0xa000, 0xa7ff, MAP_RAM);
	ZetSetOutHandler(perfrman_write_port);
	ZetSetInHandler(perfrman_read_port);
	ZetClose();

	sound_init(2000000);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, perfrman_map_callback, 8, 8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 3,  8,  8, 0x10000, 0x00, 0xf);
	GenericTilemapSetTransparent(0, 0);

	nSndIrqFrame = 4;

	DrvDoReset();

	return 0;
}

static INT32 SlapfighInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (DrvLoadRoms()) return 1;

	cpu_clock = 6000000;

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,			0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,			0xc000, 0xc7ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,			0xc800, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,				0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvColRAM,				0xd800, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,				0xe000, 0xe7ff, MAP_RAM);
	ZetMapMemory(DrvZ80ROM0 + 0x10c00,	0xec00, 0xefff, MAP_ROM); // slapfigh bootlegs
	ZetMapMemory(DrvTxtRAM,				0xf000, 0xf7ff, MAP_RAM);
	ZetMapMemory(DrvTxcRAM,				0xf800, 0xffff, MAP_RAM);
	ZetSetWriteHandler(slapfigh_main_write);
	ZetSetReadHandler(slapfigh_main_read);
	ZetSetOutHandler(perfrman_write_port);
	ZetSetInHandler(perfrman_read_port);
	ZetClose();

	if (has_mcu)
	{
		m67805_taito_init(DrvMCUROM, DrvMCURAM, has_banks ? &slapfigh_m68705_interface : &tigerh_m68705_interface);

		pMCUWrite = standard_taito_mcu_write;
		pMCURead = standard_taito_mcu_read;
		pMCUStatusRead = slapfigh_mcu_status_read;
	}

	sound_init(1500000);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 64, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, tx_map_callback, 8, 8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 2,  8,  8, 0x10000, 0, 0x3f);
	GenericTilemapSetGfx(1, DrvGfxROM1, 4,  8,  8, 0x40000, 0, 0x0f);
	GenericTilemapSetTransparent(1, 0);

	nSndIrqFrame = 3;

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	ZetExit();
	if (has_mcu) m67805_taito_exit();
	AY8910Exit(0);
	AY8910Exit(1);

	BurnFree(AllMem);

	pMCUWrite = NULL;
	pMCURead = NULL;
	pMCUStatusRead = NULL;

	has_mcu = 0;
	has_banks = 0;

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x100; i++)
	{
		INT32 bit0 = (DrvColPROM[i + 0x000] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i + 0x000] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i + 0x000] >> 2) & 0x01;
		INT32 bit3 = (DrvColPROM[i + 0x000] >> 3) & 0x01;
		INT32 r = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[i + 0x100] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + 0x100] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + 0x100] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + 0x100] >> 3) & 0x01;
		INT32 g = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[i + 0x200] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + 0x200] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + 0x200] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + 0x200] >> 3) & 0x01;
		INT32 b = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_perfrman_sprites(INT32 layer)
{
	for (INT32 offs = 0; offs < 0x800; offs += 4)
	{
		INT32 code  = DrvSprRAM[offs + 0];
		INT32 sx    = DrvSprRAM[offs + 1] - 13;
		INT32 attr  = DrvSprRAM[offs + 2];
		INT32 sy    = DrvSprRAM[offs + 3] - 1;
		INT32 pri   = attr >> 6;
		INT32 color = ((attr >> 1) & 3) | ((attr << 2) & 4) | (palette_bank << 3);
		INT32 flip  = 0;

		if (flipscreen)
		{
			sy = 256 - sy;
			sx = 256 - sx;
			flip = 1;
		}
		else
			sy -= 16;

		if (layer == pri)
		{
			Draw16x16MaskTile(pTransDraw, code, sx, sy, flip, flip, color, 3, 0, 0x80, DrvGfxROM1);
		}
	}
}

static INT32 PerfrmanDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	GenericTilemapSetFlip(TMAP_GLOBAL, flipscreen ? TMAP_FLIPXY : 0);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, flipscreen ? 0 : -16);

	GenericTilemapDraw(0, pTransDraw, TMAP_FORCEOPAQUE);

	draw_perfrman_sprites(0);
	draw_perfrman_sprites(1);

	GenericTilemapDraw(0, pTransDraw, 0);

	draw_perfrman_sprites(2);
	draw_perfrman_sprites(3);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void draw_slapfigh_sprites()
{
	for (INT32 offs = 0; offs < 0x800; offs += 4)
	{
		INT32 attr  = DrvSprBuf[offs + 2];
		INT32 code  = DrvSprBuf[offs + 0] | ((attr & 0xc0) << 2);
		INT32 sx    = (DrvSprBuf[offs + 1] + ((attr & 1) << 8)) - 13;
		INT32 sy    = DrvSprBuf[offs + 3];
		INT32 color = (attr >> 1) & 0xf;
		INT32 flip  = 0;

		if (flipscreen)
		{
			sy = 238 - sy;
			sx = 272 - sx;
			flip = 1;
		}
		else
		{
			sx -= 8;
		}

		Draw16x16MaskTile(pTransDraw, code, sx, sy - 16, flip, flip, color, 4, 0, 0, DrvGfxROM2);
	}
}

static INT32 SlapfighDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	GenericTilemapSetFlip(TMAP_GLOBAL, flipscreen ? TMAP_FLIPXY : 0);
	GenericTilemapSetOffsets(0, flipscreen ? 0 : -8, flipscreen ? 0 : -15);
	GenericTilemapSetOffsets(1, flipscreen ? 0 : -8, flipscreen ? 0 : -16);

	GenericTilemapSetScrollX(0, scrollx);
	GenericTilemapSetScrollY(0, scrolly);

	BurnTransferClear();

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);

	draw_slapfigh_sprites();

	if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();
	if (has_mcu) m6805NewFrame();

	{
		DrvInputs[0] = 0xff;
		DrvInputs[1] = 0xff;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[3] = { cpu_clock / 60, cpu_clock / 2 / 60, 3000000 / 60 };
	INT32 nCyclesDone[3] = { 0, 0, 0 };
	INT32 nSoundBufferPos = 0;

	vblank = 1;

	if (has_mcu) m6805Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(((i + 1) * nCyclesTotal[0] / nInterleave) - nCyclesDone[0]);

		if (i == 15) {
			vblank = 0;
		}

		if (i == 255) {
			if (irq_enable) ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
			vblank = 1;
			memcpy (DrvSprBuf, DrvSprRAM, 0x800);
		}
		ZetClose();

		ZetOpen(1);
		nCyclesDone[1] += ZetRun(((i + 1) * nCyclesTotal[1] / nInterleave) - nCyclesDone[1]);
		if (((i % (nInterleave / nSndIrqFrame)) == ((nInterleave / nSndIrqFrame) - 1)) && sound_nmi_enable) {
			ZetNmi();
		}
		ZetClose();

		if (has_mcu) {
			m6805Run((((i + 1) * nCyclesTotal[2]) / nInterleave) - m6805TotalCycles());
		}

		// Render sound segment
		if (pBurnSoundOut && i&1) {
			INT32 nSegmentLength = nBurnSoundLen / (nInterleave / 2);
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			AY8910Render(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	if (has_mcu) m6805Close();

	// Make sure the buffer is entirely filled.
	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			AY8910Render(pSoundBuf, nSegmentLength);
		}
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
		AY8910Scan(nAction, pnMin);
		if (has_mcu) {
			m68705_taito_scan(nAction);
		}

		SCAN_VAR(sound_nmi_enable);
		SCAN_VAR(irq_enable);
		SCAN_VAR(palette_bank);
		SCAN_VAR(flipscreen);
		SCAN_VAR(scrollx);
		SCAN_VAR(scrolly);
		SCAN_VAR(bankdata);
		SCAN_VAR(protection_counter);
		SCAN_VAR(protection_data);
	}

	if (nAction & ACB_WRITE)
	{
		ZetOpen(0);
		bankswitch(bankdata);
		ZetClose();
	}

	return 0;
}


// Performan (Japan)

static struct BurnRomInfo perfrmanRomDesc[] = {
	{ "ci07.0",				0x4000, 0x7ad32eea, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "ci08.1",				0x4000, 0x90a02d5f, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ci06.4",				0x2000, 0xdf891ad0, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "ci02.7",				0x2000, 0x8efa960a, 4 | BRF_GRA },           //  3 Background Tiles
	{ "ci01.6",				0x2000, 0x2e8e69df, 4 | BRF_GRA },           //  4
	{ "ci00.5",				0x2000, 0x79e191f8, 4 | BRF_GRA },           //  5

	{ "ci05.10",			0x2000, 0x809a4ccc, 5 | BRF_GRA },           //  6 Sprites
	{ "ci04.9",				0x2000, 0x026f27b3, 5 | BRF_GRA },           //  7
	{ "ci03.8",				0x2000, 0x6410d9eb, 5 | BRF_GRA },           //  8

	{ "ci14.16",			0x0100, 0x515f8a3b, 7 | BRF_GRA },           //  9 Color Data
	{ "ci13.15",			0x0100, 0xa9a397eb, 7 | BRF_GRA },           // 10
	{ "ci12.14",			0x0100, 0x67f86e3d, 7 | BRF_GRA },           // 11

	{ "ci11.11",			0x0100, 0xd492e6c2, 0 | BRF_OPT },           // 12 Unused PROMs
	{ "ci10.12",			0x0100, 0x59490887, 0 | BRF_OPT },           // 13
	{ "ci09.13",			0x0020, 0xaa0ca5a5, 0 | BRF_OPT },           // 14
};

STD_ROM_PICK(perfrman)
STD_ROM_FN(perfrman)

struct BurnDriver BurnDrvPerfrman = {
	"perfrman", NULL, NULL, NULL, "1985",
	"Performan (Japan)\0", NULL, "Toaplan / Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TOAPLAN_MISC, GBF_SHOOT, 0,
	NULL, perfrmanRomInfo, perfrmanRomName, NULL, NULL, NULL, NULL, SlapfighInputInfo, PerfrmanDIPInfo,
	PerfrmanInit, DrvExit, DrvFrame, PerfrmanDraw, DrvScan, &DrvRecalc, 0x100,
	240, 256, 3, 4
};


// Performan (US)

static struct BurnRomInfo perfrmanuRomDesc[] = {
	{ "ci07.0",				0x4000, 0x7ad32eea, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "ci108r5.1",			0x4000, 0x9d373efa, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ci06.4",				0x2000, 0xdf891ad0, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "ci02.7",				0x2000, 0x8efa960a, 4 | BRF_GRA },           //  3 Background Tiles
	{ "ci01.6",				0x2000, 0x2e8e69df, 4 | BRF_GRA },           //  4
	{ "ci00.5",				0x2000, 0x79e191f8, 4 | BRF_GRA },           //  5

	{ "ci05.10",			0x2000, 0x809a4ccc, 5 | BRF_GRA },           //  6 Sprites
	{ "ci04.9",				0x2000, 0x026f27b3, 5 | BRF_GRA },           //  7
	{ "ci03.8",				0x2000, 0x6410d9eb, 5 | BRF_GRA },           //  8

	{ "ci14.16",			0x0100, 0x515f8a3b, 7 | BRF_GRA },           //  9 Color Data
	{ "ci13.15",			0x0100, 0xa9a397eb, 7 | BRF_GRA },           // 10
	{ "ci12.14",			0x0100, 0x67f86e3d, 7 | BRF_GRA },           // 11

	{ "ci11.11",			0x0100, 0xd492e6c2, 0 | BRF_OPT },           // 12 Unused PROMs
	{ "ci10.12",			0x0100, 0x59490887, 0 | BRF_OPT },           // 13
	{ "ci09r1.13",			0x0020, 0xd9e92f6f, 0 | BRF_OPT },           // 14
};

STD_ROM_PICK(perfrmanu)
STD_ROM_FN(perfrmanu)

struct BurnDriver BurnDrvPerfrmanu = {
	"perfrmanu", "perfrman", NULL, NULL, "1985",
	"Performan (US)\0", NULL, "Toaplan / Data East USA", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TOAPLAN_MISC, GBF_SHOOT, 0,
	NULL, perfrmanuRomInfo, perfrmanuRomName, NULL, NULL, NULL, NULL, SlapfighInputInfo, PerfrmanDIPInfo,
	PerfrmanInit, DrvExit, DrvFrame, PerfrmanDraw, DrvScan, &DrvRecalc, 0x100,
	240, 256, 3, 4
};


// Tiger Heli (US)

static struct BurnRomInfo tigerhRomDesc[] = {
	{ "0.4",				0x4000, 0x4be73246, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "1.4",				0x4000, 0xaad04867, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "2.4",				0x4000, 0x4843f15c, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "a47_03.12d",			0x2000, 0xd105260f, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "a47_14.6a",			0x0800, 0x4042489f, 3 | BRF_PRG | BRF_ESS }, //  4 M68705 Code

	{ "a47_05.6f",			0x2000, 0xc5325b49, 4 | BRF_GRA },           //  5 Characters
	{ "a47_04.6g",			0x2000, 0xcd59628e, 4 | BRF_GRA },           //  6

	{ "a47_09.4m",			0x4000, 0x31fae8a8, 5 | BRF_GRA },           //  7 Background Tiles
	{ "a47_08.6m",			0x4000, 0xe539af2b, 5 | BRF_GRA },           //  8
	{ "a47_07.6n",			0x4000, 0x02fdd429, 5 | BRF_GRA },           //  9
	{ "a47_06.6p",			0x4000, 0x11fbcc8c, 5 | BRF_GRA },           // 10

	{ "a47_13.8j",			0x4000, 0x739a7e7e, 6 | BRF_GRA },           // 11 Sprites
	{ "a47_12.6j",			0x4000, 0xc064ecdb, 6 | BRF_GRA },           // 12
	{ "a47_11.8h",			0x4000, 0x744fae9b, 6 | BRF_GRA },           // 13
	{ "a47_10.6h",			0x4000, 0xe1cf844e, 6 | BRF_GRA },           // 14

	{ "82s129.12q",			0x0100, 0x2c69350d, 7 | BRF_GRA },           // 15 Color Data
	{ "82s129.12m",			0x0100, 0x7142e972, 7 | BRF_GRA },           // 16
	{ "82s129.12n",			0x0100, 0x25f273f2, 7 | BRF_GRA },           // 17

	{ "pal16r4a.2e",		0x0104, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 18 plds
};

STD_ROM_PICK(tigerh)
STD_ROM_FN(tigerh)

static INT32 TigerhInit()
{
	INT32 nRet = SlapfighInit();

	nSndIrqFrame = 6;

	return nRet;
}

struct BurnDriver BurnDrvTigerh = {
	"tigerh", NULL, NULL, NULL, "1985",
	"Tiger Heli (US)\0", NULL, "Toaplan / Taito America Corp.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TOAPLAN_MISC, GBF_VERSHOOT, 0,
	NULL, tigerhRomInfo, tigerhRomName, NULL, NULL, NULL, NULL, TigerhInputInfo, TigerhDIPInfo,
	TigerhInit, DrvExit, DrvFrame, SlapfighDraw, DrvScan, &DrvRecalc, 0x100,
	240, 280, 3, 4
};


// Tiger Heli (Japan)

static struct BurnRomInfo tigerhjRomDesc[] = {
	{ "a47_00.8p",			0x4000, 0xcbdbe3cc, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "a47_01.8n",			0x4000, 0x65df2152, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a47_02.8k",			0x4000, 0x633d324b, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "a47_03.12d",			0x2000, 0xd105260f, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "a47_14.6a",			0x0800, 0x4042489f, 3 | BRF_PRG | BRF_ESS }, //  4 M68705 Code

	{ "a47_05.6f",			0x2000, 0xc5325b49, 4 | BRF_GRA },           //  5 Characters
	{ "a47_04.6g",			0x2000, 0xcd59628e, 4 | BRF_GRA },           //  6

	{ "a47_09.4m",			0x4000, 0x31fae8a8, 5 | BRF_GRA },           //  7 Background Tiles
	{ "a47_08.6m",			0x4000, 0xe539af2b, 5 | BRF_GRA },           //  8
	{ "a47_07.6n",			0x4000, 0x02fdd429, 5 | BRF_GRA },           //  9
	{ "a47_06.6p",			0x4000, 0x11fbcc8c, 5 | BRF_GRA },           // 10

	{ "a47_13.8j",			0x4000, 0x739a7e7e, 6 | BRF_GRA },           // 11 Sprites
	{ "a47_12.6j",			0x4000, 0xc064ecdb, 6 | BRF_GRA },           // 12
	{ "a47_11.8h",			0x4000, 0x744fae9b, 6 | BRF_GRA },           // 13
	{ "a47_10.6h",			0x4000, 0xe1cf844e, 6 | BRF_GRA },           // 14

	{ "82s129.12q",			0x0100, 0x2c69350d, 7 | BRF_GRA },           // 15 Color Data
	{ "82s129.12m",			0x0100, 0x7142e972, 7 | BRF_GRA },           // 16
	{ "82s129.12n",			0x0100, 0x25f273f2, 7 | BRF_GRA },           // 17
};

STD_ROM_PICK(tigerhj)
STD_ROM_FN(tigerhj)

struct BurnDriver BurnDrvTigerhj = {
	"tigerhj", "tigerh", NULL, NULL, "1985",
	"Tiger Heli (Japan)\0", NULL, "Toaplan / Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TOAPLAN_MISC, GBF_VERSHOOT, 0,
	NULL, tigerhjRomInfo, tigerhjRomName, NULL, NULL, NULL, NULL, TigerhInputInfo, TigerhDIPInfo,
	TigerhInit, DrvExit, DrvFrame, SlapfighDraw, DrvScan, &DrvRecalc, 0x100,
	240, 280, 3, 4
};


// Tiger Heli (bootleg set 1)

static struct BurnRomInfo tigerhb1RomDesc[] = {
	{ "b0.5",				0x4000, 0x6ae7e13c, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "a47_01.8n",			0x4000, 0x65df2152, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a47_02.8k",			0x4000, 0x633d324b, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "a47_03.12d",			0x2000, 0xd105260f, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "a47_05.6f",			0x2000, 0xc5325b49, 4 | BRF_GRA },           //  4 Characters
	{ "a47_04.6g",			0x2000, 0xcd59628e, 4 | BRF_GRA },           //  5

	{ "a47_09.4m",			0x4000, 0x31fae8a8, 5 | BRF_GRA },           //  6 Background Tiles
	{ "a47_08.6m",			0x4000, 0xe539af2b, 5 | BRF_GRA },           //  7
	{ "a47_07.6n",			0x4000, 0x02fdd429, 5 | BRF_GRA },           //  8
	{ "a47_06.6p",			0x4000, 0x11fbcc8c, 5 | BRF_GRA },           //  9

	{ "a47_13.8j",			0x4000, 0x739a7e7e, 6 | BRF_GRA },           // 10 Sprites
	{ "a47_12.6j",			0x4000, 0xc064ecdb, 6 | BRF_GRA },           // 11
	{ "a47_11.8h",			0x4000, 0x744fae9b, 6 | BRF_GRA },           // 12
	{ "a47_10.6h",			0x4000, 0xe1cf844e, 6 | BRF_GRA },           // 13

	{ "82s129.12q",			0x0100, 0x2c69350d, 7 | BRF_GRA },           // 14 Color Data
	{ "82s129.12m",			0x0100, 0x7142e972, 7 | BRF_GRA },           // 15
	{ "82s129.12n",			0x0100, 0x25f273f2, 7 | BRF_GRA },           // 16
};

STD_ROM_PICK(tigerhb1)
STD_ROM_FN(tigerhb1)

static INT32 tigerhb1_prot_read()
{
	return protection_counter;
}

static void tigerhb1_prot_write(INT32 data)
{
	protection_counter = (data == 0x73) ? 0x83 : 0;
}

static INT32 Tigerhb1Init()
{
	INT32 nRet = TigerhInit();

	if (nRet == 0)
	{
		pMCUWrite = tigerhb1_prot_write;
		pMCURead = tigerhb1_prot_read;
	}

	return 0;
}

struct BurnDriver BurnDrvTigerhb1 = {
	"tigerhb1", "tigerh", NULL, NULL, "1985",
	"Tiger Heli (bootleg set 1)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TOAPLAN_MISC, GBF_VERSHOOT, 0,
	NULL, tigerhb1RomInfo, tigerhb1RomName, NULL, NULL, NULL, NULL, TigerhInputInfo, TigerhDIPInfo,
	Tigerhb1Init, DrvExit, DrvFrame, SlapfighDraw, DrvScan, &DrvRecalc, 0x100,
	240, 280, 3, 4
};


// Tiger Heli (bootleg set 2)

static struct BurnRomInfo tigerhb2RomDesc[] = {
	{ "rom00_09.bin",		0x4000, 0xef738c68, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "a47_01.8n",			0x4000, 0x65df2152, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rom02_07.bin",		0x4000, 0x36e250b9, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "a47_03.12d",			0x2000, 0xd105260f, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "a47_05.6f",			0x2000, 0xc5325b49, 4 | BRF_GRA },           //  4 Characters
	{ "a47_04.6g",			0x2000, 0xcd59628e, 4 | BRF_GRA },           //  5

	{ "a47_09.4m",			0x4000, 0x31fae8a8, 5 | BRF_GRA },           //  6 Background Tiles
	{ "a47_08.6m",			0x4000, 0xe539af2b, 5 | BRF_GRA },           //  7
	{ "a47_07.6n",			0x4000, 0x02fdd429, 5 | BRF_GRA },           //  8
	{ "a47_06.6p",			0x4000, 0x11fbcc8c, 5 | BRF_GRA },           //  9

	{ "a47_13.8j",			0x4000, 0x739a7e7e, 6 | BRF_GRA },           // 10 Sprites
	{ "a47_12.6j",			0x4000, 0xc064ecdb, 6 | BRF_GRA },           // 11
	{ "a47_11.8h",			0x4000, 0x744fae9b, 6 | BRF_GRA },           // 12
	{ "a47_10.6h",			0x4000, 0xe1cf844e, 6 | BRF_GRA },           // 13

	{ "82s129.12q",			0x0100, 0x2c69350d, 7 | BRF_GRA },           // 14 Color Data
	{ "82s129.12m",			0x0100, 0x7142e972, 7 | BRF_GRA },           // 15
	{ "82s129.12n",			0x0100, 0x25f273f2, 7 | BRF_GRA },           // 16
};

STD_ROM_PICK(tigerhb2)
STD_ROM_FN(tigerhb2)

struct BurnDriver BurnDrvTigerhb2 = {
	"tigerhb2", "tigerh", NULL, NULL, "1985",
	"Tiger Heli (bootleg set 2)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TOAPLAN_MISC, GBF_VERSHOOT, 0,
	NULL, tigerhb2RomInfo, tigerhb2RomName, NULL, NULL, NULL, NULL, TigerhInputInfo, TigerhDIPInfo,
	TigerhInit, DrvExit, DrvFrame, SlapfighDraw, DrvScan, &DrvRecalc, 0x100,
	240, 280, 3, 4
};


// Tiger Heli (bootleg set 3)

static struct BurnRomInfo tigerhb3RomDesc[] = {
	{ "14",					0x4000, 0xca59dd73, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "13",					0x4000, 0x38bd54db, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a47_02.8k",			0x4000, 0x633d324b, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "a47_03.12d",			0x2000, 0xd105260f, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "a47_05.6f",			0x2000, 0xc5325b49, 4 | BRF_GRA },           //  4 Characters
	{ "a47_04.6g",			0x2000, 0xcd59628e, 4 | BRF_GRA },           //  5

	{ "a47_09.4m",			0x4000, 0x31fae8a8, 5 | BRF_GRA },           //  6 Background Tiles
	{ "a47_08.6m",			0x4000, 0xe539af2b, 5 | BRF_GRA },           //  7
	{ "a47_07.6n",			0x4000, 0x02fdd429, 5 | BRF_GRA },           //  8
	{ "a47_06.6p",			0x4000, 0x11fbcc8c, 5 | BRF_GRA },           //  9

	{ "a47_13.8j",			0x4000, 0x739a7e7e, 6 | BRF_GRA },           // 10 Sprites
	{ "a47_12.6j",			0x4000, 0xc064ecdb, 6 | BRF_GRA },           // 11
	{ "a47_11.8h",			0x4000, 0x744fae9b, 6 | BRF_GRA },           // 12
	{ "a47_10.6h",			0x4000, 0xe1cf844e, 6 | BRF_GRA },           // 13

	{ "82s129.12q",			0x0100, 0x2c69350d, 7 | BRF_GRA },           // 14 Color Data
	{ "82s129.12m",			0x0100, 0x7142e972, 7 | BRF_GRA },           // 15
	{ "82s129.12n",			0x0100, 0x25f273f2, 7 | BRF_GRA },           // 16
};

STD_ROM_PICK(tigerhb3)
STD_ROM_FN(tigerhb3)

struct BurnDriver BurnDrvTigerhb3 = {
	"tigerhb3", "tigerh", NULL, NULL, "1985",
	"Tiger Heli (bootleg set 3)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TOAPLAN_MISC, GBF_VERSHOOT, 0,
	NULL, tigerhb3RomInfo, tigerhb3RomName, NULL, NULL, NULL, NULL, TigerhInputInfo, TigerhDIPInfo,
	TigerhInit, DrvExit, DrvFrame, SlapfighDraw, DrvScan, &DrvRecalc, 0x100,
	240, 280, 3, 4
};


// Alcon (US)

static struct BurnRomInfo alconRomDesc[] = {
	{ "a77_00-1.8p",		0x8000, 0x2ba82d60, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "a77_01-1.8n",		0x8000, 0x18bb2f12, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "a77_02.12d",			0x2000, 0x87f4705a, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "a77_13.6a",			0x0800, 0xa70c81d9, 3 | BRF_PRG | BRF_ESS }, //  3 M68705 Code

	{ "a77_04-1.6f",		0x2000, 0x31003483, 4 | BRF_GRA },           //  4 Characters
	{ "a77_03-1.6g",		0x2000, 0x404152c0, 4 | BRF_GRA },           //  5

	{ "a77_08.6k",			0x8000, 0xb6358305, 5 | BRF_GRA },           //  6 Background Tiles
	{ "a77_07.6m",			0x8000, 0xe92d9d60, 5 | BRF_GRA },           //  7
	{ "a77_06.6n",			0x8000, 0x5faeeea3, 5 | BRF_GRA },           //  8
	{ "a77_05.6p",			0x8000, 0x974e2ea9, 5 | BRF_GRA },           //  9

	{ "a77_12.8j",			0x8000, 0x8545d397, 6 | BRF_GRA },           // 10 Sprites
	{ "a77_11.7j",			0x8000, 0xb1b7b925, 6 | BRF_GRA },           // 11
	{ "a77_10.8h",			0x8000, 0x422d946b, 6 | BRF_GRA },           // 12
	{ "a77_09.7h",			0x8000, 0x587113ae, 6 | BRF_GRA },           // 13

	{ "21_82s129.12q",		0x0100, 0xa0efaf99, 7 | BRF_GRA },           // 14 Color Data
	{ "20_82s129.12m",		0x0100, 0xa56d57e5, 7 | BRF_GRA },           // 15
	{ "19_82s129.12n",		0x0100, 0x5cbf9fbf, 7 | BRF_GRA },           // 16
};

STD_ROM_PICK(alcon)
STD_ROM_FN(alcon)

struct BurnDriver BurnDrvAlcon = {
	"alcon", NULL, NULL, NULL, "1986",
	"Alcon (US)\0", NULL, "Toaplan / Taito America Corp.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TOAPLAN_MISC, GBF_VERSHOOT, 0,
	NULL, alconRomInfo, alconRomName, NULL, NULL, NULL, NULL, SlapfighInputInfo, SlapfighDIPInfo,
	SlapfighInit, DrvExit, DrvFrame, SlapfighDraw, DrvScan, &DrvRecalc, 0x100,
	240, 280, 3, 4
};


// Slap Fight (A77 set, 8606M PCB)

static struct BurnRomInfo slapfighRomDesc[] = {
	{ "a77_00.8p",			0x8000, 0x674c0e0f, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "a77_01.8n",			0x8000, 0x3c42e4a7, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "a77_02.12d",			0x2000, 0x87f4705a, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "a77_13.6a",			0x0800, 0xa70c81d9, 3 | BRF_PRG | BRF_ESS }, //  3 M68705 Code

	{ "a77_04.6f",			0x2000, 0x2ac7b943, 4 | BRF_GRA },           //  4 Characters
	{ "a77_03.6g",			0x2000, 0x33cadc93, 4 | BRF_GRA },           //  5

	{ "a77_08.6k",			0x8000, 0xb6358305, 5 | BRF_GRA },           //  6 Background Tiles
	{ "a77_07.6m",			0x8000, 0xe92d9d60, 5 | BRF_GRA },           //  7
	{ "a77_06.6n",			0x8000, 0x5faeeea3, 5 | BRF_GRA },           //  8
	{ "a77_05.6p",			0x8000, 0x974e2ea9, 5 | BRF_GRA },           //  9

	{ "a77_12.8j",			0x8000, 0x8545d397, 6 | BRF_GRA },           // 10 Sprites
	{ "a77_11.7j",			0x8000, 0xb1b7b925, 6 | BRF_GRA },           // 11
	{ "a77_10.8h",			0x8000, 0x422d946b, 6 | BRF_GRA },           // 12
	{ "a77_09.7h",			0x8000, 0x587113ae, 6 | BRF_GRA },           // 13

	{ "21_82s129.12q",		0x0100, 0xa0efaf99, 7 | BRF_GRA },           // 14 Color Data
	{ "20_82s129.12m",		0x0100, 0xa56d57e5, 7 | BRF_GRA },           // 15
	{ "19_82s129.12n",		0x0100, 0x5cbf9fbf, 7 | BRF_GRA },           // 16
};

STD_ROM_PICK(slapfigh)
STD_ROM_FN(slapfigh)

struct BurnDriver BurnDrvSlapfigh = {
	"slapfigh", "alcon", NULL, NULL, "1986",
	"Slap Fight (A77 set, 8606M PCB)\0", NULL, "Toaplan / Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TOAPLAN_MISC, GBF_VERSHOOT, 0,
	NULL, slapfighRomInfo, slapfighRomName, NULL, NULL, NULL, NULL, SlapfighInputInfo, SlapfighDIPInfo,
	SlapfighInit, DrvExit, DrvFrame, SlapfighDraw, DrvScan, &DrvRecalc, 0x100,
	240, 280, 3, 4
};


// Slap Fight (A76 set, GX-006-A PCB)

static struct BurnRomInfo slapfighaRomDesc[] = {
	{ "a76_00.8p",			0x4000, 0xac22bb86, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "a76_01.8n",			0x4000, 0xd6b4f02e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a76_02.8k",			0x8000, 0x9dd0971f, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "a76_03.12d",			0x2000, 0x87f4705a, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "a76_14.6a",			0x0800, 0x3f4980bb, 3 | BRF_PRG | BRF_ESS }, //  4 M68705 Code

	{ "a76_05.6f",			0x2000, 0xbe9a1bc5, 4 | BRF_GRA },           //  5 Characters
	{ "a76_04.6g",			0x2000, 0x3519daa4, 4 | BRF_GRA },           //  6

	{ "a76_09.4m",			0x8000, 0xb6358305, 5 | BRF_GRA },           //  7 Background Tiles
	{ "a76_08.6m",			0x8000, 0xe92d9d60, 5 | BRF_GRA },           //  8
	{ "a76_07.6n",			0x8000, 0x5faeeea3, 5 | BRF_GRA },           //  9
	{ "a76_06.6p",			0x8000, 0x974e2ea9, 5 | BRF_GRA },           // 10

	{ "a76_13.8j",			0x8000, 0x8545d397, 6 | BRF_GRA },           // 11 Sprites
	{ "a76_12.6j",			0x8000, 0xb1b7b925, 6 | BRF_GRA },           // 12
	{ "a76_11.8h",			0x8000, 0x422d946b, 6 | BRF_GRA },           // 13
	{ "a76_10.6h",			0x8000, 0x587113ae, 6 | BRF_GRA },           // 14

	{ "a76-17.12q",			0x0100, 0xa0efaf99, 7 | BRF_GRA },           // 15 Color Data
	{ "a76-15.12m",			0x0100, 0xa56d57e5, 7 | BRF_GRA },           // 16
	{ "a76-16.12n",			0x0100, 0x5cbf9fbf, 7 | BRF_GRA },           // 17
};

STD_ROM_PICK(slapfigha)
STD_ROM_FN(slapfigha)

struct BurnDriver BurnDrvSlapfigha = {
	"slapfigha", "alcon", NULL, NULL, "1986",
	"Slap Fight (A76 set, GX-006-A PCB)\0", NULL, "Toaplan / Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TOAPLAN_MISC, GBF_VERSHOOT, 0,
	NULL, slapfighaRomInfo, slapfighaRomName, NULL, NULL, NULL, NULL, SlapfighInputInfo, SlapfighDIPInfo,
	SlapfighInit, DrvExit, DrvFrame, SlapfighDraw, DrvScan, &DrvRecalc, 0x100,
	240, 280, 3, 4
};


// Slap Fight (bootleg set 1)

static struct BurnRomInfo slapfighb1RomDesc[] = {
	{ "sf_r19jb.bin",		0x8000, 0x9a7ac8b3, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "sf_rh.bin",			0x8000, 0x3c42e4a7, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "sf_r05.bin",			0x2000, 0x87f4705a, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "sf_r11.bin",			0x2000, 0x2ac7b943, 4 | BRF_GRA },           //  3 Characters
	{ "sf_r10.bin",			0x2000, 0x33cadc93, 4 | BRF_GRA },           //  4

	{ "sf_r06.bin",			0x8000, 0xb6358305, 5 | BRF_GRA },           //  5 Background Tiles
	{ "sf_r09.bin",			0x8000, 0xe92d9d60, 5 | BRF_GRA },           //  6
	{ "sf_r08.bin",			0x8000, 0x5faeeea3, 5 | BRF_GRA },           //  7
	{ "sf_r07.bin",			0x8000, 0x974e2ea9, 5 | BRF_GRA },           //  8

	{ "sf_r03.bin",			0x8000, 0x8545d397, 6 | BRF_GRA },           //  9 Sprites
	{ "sf_r01.bin",			0x8000, 0xb1b7b925, 6 | BRF_GRA },           // 10
	{ "sf_r04.bin",			0x8000, 0x422d946b, 6 | BRF_GRA },           // 11
	{ "sf_r02.bin",			0x8000, 0x587113ae, 6 | BRF_GRA },           // 12

	{ "sf_col21.bin",		0x0100, 0xa0efaf99, 7 | BRF_GRA },           // 13 Color Data
	{ "sf_col20.bin",		0x0100, 0xa56d57e5, 7 | BRF_GRA },           // 14
	{ "sf_col19.bin",		0x0100, 0x5cbf9fbf, 7 | BRF_GRA },           // 15
};

STD_ROM_PICK(slapfighb1)
STD_ROM_FN(slapfighb1)

struct BurnDriver BurnDrvSlapfighb1 = {
	"slapfighb1", "alcon", NULL, NULL, "1986",
	"Slap Fight (bootleg set 1)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TOAPLAN_MISC, GBF_VERSHOOT, 0,
	NULL, slapfighb1RomInfo, slapfighb1RomName, NULL, NULL, NULL, NULL, SlapfighInputInfo, SlapfighDIPInfo,
	SlapfighInit, DrvExit, DrvFrame, SlapfighDraw, DrvScan, &DrvRecalc, 0x100,
	240, 280, 3, 4
};


// Slap Fight (bootleg set 2)

static struct BurnRomInfo slapfighb2RomDesc[] = {
	{ "sf_r19eb.bin",		0x4000, 0x2efe47af, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "sf_r20eb.bin",		0x4000, 0xf42c7951, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sf_rh.bin",			0x8000, 0x3c42e4a7, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "sf_r05.bin",			0x2000, 0x87f4705a, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "sf_r11.bin",			0x2000, 0x2ac7b943, 4 | BRF_GRA },           //  4 Characters
	{ "sf_r10.bin",			0x2000, 0x33cadc93, 4 | BRF_GRA },           //  5

	{ "sf_r06.bin",			0x8000, 0xb6358305, 5 | BRF_GRA },           //  6 Background Tiles
	{ "sf_r09.bin",			0x8000, 0xe92d9d60, 5 | BRF_GRA },           //  7
	{ "sf_r08.bin",			0x8000, 0x5faeeea3, 5 | BRF_GRA },           //  8
	{ "sf_r07.bin",			0x8000, 0x974e2ea9, 5 | BRF_GRA },           //  9

	{ "sf_r03.bin",			0x8000, 0x8545d397, 6 | BRF_GRA },           // 10 Sprites
	{ "sf_r01.bin",			0x8000, 0xb1b7b925, 6 | BRF_GRA },           // 11
	{ "sf_r04.bin",			0x8000, 0x422d946b, 6 | BRF_GRA },           // 12
	{ "sf_r02.bin",			0x8000, 0x587113ae, 6 | BRF_GRA },           // 13
	
	{ "sf_col21.bin",		0x0100, 0xa0efaf99, 7 | BRF_GRA },           // 14 Color Data
	{ "sf_col20.bin",		0x0100, 0xa56d57e5, 7 | BRF_GRA },           // 15
	{ "sf_col19.bin",		0x0100, 0x5cbf9fbf, 7 | BRF_GRA },           // 16
};

STD_ROM_PICK(slapfighb2)
STD_ROM_FN(slapfighb2)

static INT32 Slapfighb2Init()
{
	INT32 nRet = SlapfighInit();

	if (nRet == 0)
	{
		ZetOpen(0);
		ZetSetWriteHandler(slapfighb2_main_write);
		ZetClose();

		// this set is missing a title rom, copy the standard data out
		// and in the order the bootleg expects.
		memcpy (DrvZ80ROM0 + 0x10c00, DrvZ80ROM0 + 0x04000 + 0x2c07, 0x100);
		memcpy (DrvZ80ROM0 + 0x10d00, DrvZ80ROM0 + 0x04000 + 0x2b07, 0x100);
		memcpy (DrvZ80ROM0 + 0x10e00, DrvZ80ROM0 + 0x04000 + 0x2d07, 0x100);
	}

	return nRet;
}

struct BurnDriver BurnDrvSlapfighb2 = {
	"slapfighb2", "alcon", NULL, NULL, "1986",
	"Slap Fight (bootleg set 2)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TOAPLAN_MISC, GBF_VERSHOOT, 0,
	NULL, slapfighb2RomInfo, slapfighb2RomName, NULL, NULL, NULL, NULL, SlapfighInputInfo, SlapfighDIPInfo,
	Slapfighb2Init, DrvExit, DrvFrame, SlapfighDraw, DrvScan, &DrvRecalc, 0x100,
	240, 280, 3, 4
};


// Slap Fight (bootleg set 3)

static struct BurnRomInfo slapfighb3RomDesc[] = {
	{ "k1-10.u90",			0x4000, 0x2efe47af, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "k1-09.u89",			0x4000, 0x17c187c5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "k1-07.u87",			0x8000, 0x3c42e4a7, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "k1-08.u88",			0x2000, 0x945af97f, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "k1-11.u89",			0x2000, 0x87f4705a, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code

	{ "k1-02.u57",			0x2000, 0x2ac7b943, 4 | BRF_GRA },           //  5 Characters
	{ "k1-03.u58",			0x2000, 0x33cadc93, 4 | BRF_GRA },           //  6

	{ "k1-01.u49",			0x8000, 0xb6358305, 5 | BRF_GRA },           //  7 Background Tiles
	{ "k1-04.u62",			0x8000, 0xe92d9d60, 5 | BRF_GRA },           //  8
	{ "k1-05.u63",			0x8000, 0x5faeeea3, 5 | BRF_GRA },           //  9
	{ "k1-06.u64",			0x8000, 0x974e2ea9, 5 | BRF_GRA },           // 10

	{ "k1-15.u60",			0x8000, 0x8545d397, 6 | BRF_GRA },           // 11 Sprites
	{ "k1-13.u50",			0x8000, 0xb1b7b925, 6 | BRF_GRA },           // 12
	{ "k1-14.u59",			0x8000, 0x422d946b, 6 | BRF_GRA },           // 13
	{ "k1-12.u49",			0x8000, 0x587113ae, 6 | BRF_GRA },           // 14

	{ "sf_col21.bin",		0x0100, 0xa0efaf99, 7 | BRF_GRA },           // 15 Color Data
	{ "sf_col20.bin",		0x0100, 0xa56d57e5, 7 | BRF_GRA },           // 16
	{ "sf_col19.bin",		0x0100, 0x5cbf9fbf, 7 | BRF_GRA },           // 17
};

STD_ROM_PICK(slapfighb3)
STD_ROM_FN(slapfighb3)

static INT32 Slapfighb3Init()
{
	INT32 nRet = SlapfighInit();

	if (nRet == 0)
	{
		ZetOpen(0);
		ZetSetWriteHandler(slapfighb2_main_write);
		ZetClose();
	}

	return nRet;
}

struct BurnDriver BurnDrvSlapfighb3 = {
	"slapfighb3", "alcon", NULL, NULL, "1986",
	"Slap Fight (bootleg set 3)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TOAPLAN_MISC, GBF_VERSHOOT, 0,
	NULL, slapfighb3RomInfo, slapfighb3RomName, NULL, NULL, NULL, NULL, SlapfighInputInfo, SlapfighDIPInfo,
	Slapfighb3Init, DrvExit, DrvFrame, SlapfighDraw, DrvScan, &DrvRecalc, 0x100,
	240, 280, 3, 4
};


// Guardian (US)

static struct BurnRomInfo grdianRomDesc[] = {
	{ "a68_00-1.8p",		0x4000, 0x6a8bdc6c, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "a68_01-1.8n",		0x4000, 0xebe8db3c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a68_02-1.8k",		0x8000, 0x343e8415, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "a68-03.12d",			0x2000, 0x18daa44c, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "a68_14.6a",			0x0800, 0x87c4ca48, 3 | BRF_PRG | BRF_ESS }, //  4 M68705 Code

	{ "a68_05-1.6f",		0x2000, 0x06f60107, 4 | BRF_GRA },           //  5 Characters
	{ "a68_04-1.6g",		0x2000, 0x1fc8f277, 4 | BRF_GRA },           //  6

	{ "a68_09.4m",			0x8000, 0xa293cc2e, 5 | BRF_GRA },           //  7 Background Tiles
	{ "a68_08.6m",			0x8000, 0x37662375, 5 | BRF_GRA },           //  8
	{ "a68_07.6n",			0x8000, 0xcf1a964c, 5 | BRF_GRA },           //  9
	{ "a68_06.6p",			0x8000, 0x05f9eb9a, 5 | BRF_GRA },           // 10

	{ "a68-13.8j",			0x8000, 0x643fb282, 6 | BRF_GRA },           // 11 Sprites
	{ "a68-12.6j",			0x8000, 0x11f74e32, 6 | BRF_GRA },           // 12
	{ "a68-11.8h",			0x8000, 0xf24158cf, 6 | BRF_GRA },           // 13
	{ "a68-10.6h",			0x8000, 0x83161ed0, 6 | BRF_GRA },           // 14

	{ "rom21.12q",			0x0100, 0xd6360b4d, 7 | BRF_GRA },           // 15 Color Data
	{ "rom20.12m",			0x0100, 0x4ca01887, 7 | BRF_GRA },           // 16
	{ "rom19.12p",			0x0100, 0x513224f0, 7 | BRF_GRA },           // 17
};

STD_ROM_PICK(grdian)
STD_ROM_FN(grdian)

struct BurnDriver BurnDrvGrdian = {
	"grdian", NULL, NULL, NULL, "1986",
	"Guardian (US)\0", NULL, "Toaplan / Taito America Corporation (Kitkorp license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TOAPLAN_MISC, GBF_SCRFIGHT, 0,
	NULL, grdianRomInfo, grdianRomName, NULL, NULL, NULL, NULL, SlapfighInputInfo, GetstarDIPInfo,
	SlapfighInit, DrvExit, DrvFrame, SlapfighDraw, DrvScan, &DrvRecalc, 0x100,
	280, 240, 4, 3
};


// Get Star (Japan)

static struct BurnRomInfo getstarjRomDesc[] = {
	{ "a68_00.8p",			0x4000, 0xad1a0143, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "a68_01.8n",			0x4000, 0x3426eb7c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a68_02.8k",			0x8000, 0x3567da17, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "a68-03.12d",			0x2000, 0x18daa44c, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "a68_14.6a",			0x0800, 0x87c4ca48, 3 | BRF_PRG | BRF_ESS }, //  4 M68705 Code

	{ "a68_05.6f",			0x2000, 0xe3d409e7, 4 | BRF_GRA },           //  5 Characters
	{ "a68_04.6g",			0x2000, 0x6e5ac9d4, 4 | BRF_GRA },           //  6

	{ "a68_09.4m",			0x8000, 0xa293cc2e, 5 | BRF_GRA },           //  7 Background Tiles
	{ "a68_08.6m",			0x8000, 0x37662375, 5 | BRF_GRA },           //  8
	{ "a68_07.6n",			0x8000, 0xcf1a964c, 5 | BRF_GRA },           //  9
	{ "a68_06.6p",			0x8000, 0x05f9eb9a, 5 | BRF_GRA },           // 10

	{ "a68-13.8j",			0x8000, 0x643fb282, 6 | BRF_GRA },           // 11 Sprites
	{ "a68-12.6j",			0x8000, 0x11f74e32, 6 | BRF_GRA },           // 12
	{ "a68-11.8h",			0x8000, 0xf24158cf, 6 | BRF_GRA },           // 13
	{ "a68-10.6h",			0x8000, 0x83161ed0, 6 | BRF_GRA },           // 14

	{ "rom21.12q",			0x0100, 0xd6360b4d, 7 | BRF_GRA },           // 15 Color Data
	{ "rom20.12m",			0x0100, 0x4ca01887, 7 | BRF_GRA },           // 16
	{ "rom19.12p",			0x0100, 0x513224f0, 7 | BRF_GRA },           // 17
};

STD_ROM_PICK(getstarj)
STD_ROM_FN(getstarj)

struct BurnDriver BurnDrvGetstarj = {
	"getstarj", "grdian", NULL, NULL, "1986",
	"Get Star (Japan)\0", NULL, "Toaplan / Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TOAPLAN_MISC, GBF_SCRFIGHT, 0,
	NULL, getstarjRomInfo, getstarjRomName, NULL, NULL, NULL, NULL, SlapfighInputInfo, GetstarjDIPInfo,
	SlapfighInit, DrvExit, DrvFrame, SlapfighDraw, DrvScan, &DrvRecalc, 0x100,
	280, 240, 4, 3
};


// Get Star (bootleg set 1)

static struct BurnRomInfo getstarb1RomDesc[] = {
	{ "getstar_14_b.8p",	0x4000, 0x9afad7e0, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "getstar_13_b.8n",	0x4000, 0x5feb0a60, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "getstar_12_b.8k",	0x8000, 0xe3cfb1ba, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "getstar_5_a.12e",	0x2000, 0x18daa44c, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "getstar_7_b.6f",		0x2000, 0xe3d409e7, 4 | BRF_GRA },           //  4 Characters
	{ "getstar_8_b.6g",		0x2000, 0x6e5ac9d4, 4 | BRF_GRA },           //  5

	{ "getstar_6_b.4m",		0x8000, 0xa293cc2e, 5 | BRF_GRA },           //  6 Background Tiles
	{ "getstar_9_b.6m",		0x8000, 0x37662375, 5 | BRF_GRA },           //  7
	{ "getstar_10_b.6n",	0x8000, 0xcf1a964c, 5 | BRF_GRA },           //  8
	{ "getstar_11_b.6p",	0x8000, 0x05f9eb9a, 5 | BRF_GRA },           //  9

	{ "getstar_4_a.8j",		0x8000, 0x643fb282, 6 | BRF_GRA },           // 10 Sprites
	{ "getstar_2_a.7j",		0x8000, 0x11f74e32, 6 | BRF_GRA },           // 11
	{ "getstar_3_a.8h",		0x8000, 0xf24158cf, 6 | BRF_GRA },           // 12
	{ "getstar_1_a.7h",		0x8000, 0x83161ed0, 6 | BRF_GRA },           // 13

	{ "getstar_p7_a.12q",	0x0100, 0xd6360b4d, 7 | BRF_GRA },           // 14 Color Data
	{ "getstar_p5_a.12m",	0x0100, 0x4ca01887, 7 | BRF_GRA },           // 15
	{ "getstar_p6_a.12n",	0x0100, 0x35eefbbb, 7 | BRF_GRA },           // 16

	{ "getstar_p1_a.1c",	0x0100, 0xd492e6c2, 0 | BRF_OPT },           // 17 Extra PROMs
	{ "getstar_p2_a.1e",	0x0100, 0x59490887, 0 | BRF_OPT },           // 18
	{ "getstar_p3_a.2b",	0x0020, 0xaa0ca5a5, 0 | BRF_OPT },           // 19
	{ "getstar_p8_b.2c",	0x0100, 0x4dc04453, 0 | BRF_OPT },           // 20
	{ "getstar_p9_b.8b",	0x0100, 0x48ac3db4, 0 | BRF_OPT },           // 21

	{ "getstar_p4_a.2e",	0x0104, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 22 PLDs
};

STD_ROM_PICK(getstarb1)
STD_ROM_FN(getstarb1)

static INT32 getstarb1_mcusim_read()
{
	UINT8 lookup_table[4] = { 0x03, 0x05, 0x01, 0x02 };

	if (ZetGetPC(-1) == 0x6b04) return lookup_table[protection_data];

	return 0;
}

static void getstar_mcusim_write(INT32)
{
	if (ZetGetPC(-1) == 0x6ae2)
	{
		protection_data = 0;
	}

	if (ZetGetPC(-1) == 0x6af3)
	{
		protection_data = ZetBc(-1) & 3;
	}
}

static INT32 getstarb1_prot_read()
{
	protection_counter++;
	return protection_counter << 1;
}

static INT32 Getstarb1Init()
{
	INT32 nRet = SlapfighInit();

	if (nRet == 0)
	{
		pMCUStatusRead = getstarb1_prot_read;
		pMCURead = getstarb1_mcusim_read;
		pMCUWrite = getstar_mcusim_write;

		DrvZ80ROM0[0x6d56] = 0xc3; // fix bad bit?
	}

	return nRet;
}

struct BurnDriver BurnDrvGetstarb1 = {
	"getstarb1", "grdian", NULL, NULL, "1986",
	"Get Star (bootleg set 1)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_TOAPLAN_MISC, GBF_SCRFIGHT, 0,
	NULL, getstarb1RomInfo, getstarb1RomName, NULL, NULL, NULL, NULL, SlapfighInputInfo, GetstarjDIPInfo,
	Getstarb1Init, DrvExit, DrvFrame, SlapfighDraw, DrvScan, &DrvRecalc, 0x100,
	280, 240, 4, 3
};


// Get Star (bootleg set 2)

static struct BurnRomInfo getstarb2RomDesc[] = {
	{ "gs_14.rom",			0x4000, 0x1a57a920, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "gs_13.rom",			0x4000, 0x805f8e77, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a68_02.bin",			0x8000, 0x3567da17, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "a68-03",				0x2000, 0x18daa44c, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "a68_05.bin",			0x2000, 0xe3d409e7, 4 | BRF_GRA },           //  4 Characters
	{ "a68_04.bin",			0x2000, 0x6e5ac9d4, 4 | BRF_GRA },           //  5

	{ "a68_09",				0x8000, 0xa293cc2e, 5 | BRF_GRA },           //  6 Background Tiles
	{ "a68_08",				0x8000, 0x37662375, 5 | BRF_GRA },           //  7
	{ "a68_07",				0x8000, 0xcf1a964c, 5 | BRF_GRA },           //  8
	{ "a68_06",				0x8000, 0x05f9eb9a, 5 | BRF_GRA },           //  9

	{ "a68-13",				0x8000, 0x643fb282, 6 | BRF_GRA },           // 10 Sprites
	{ "a68-12",				0x8000, 0x11f74e32, 6 | BRF_GRA },           // 11
	{ "a68-11",				0x8000, 0xf24158cf, 6 | BRF_GRA },           // 12
	{ "a68-10",				0x8000, 0x83161ed0, 6 | BRF_GRA },           // 13

	{ "rom21",				0x0100, 0xd6360b4d, 7 | BRF_GRA },           // 14 Color Data
	{ "rom20",				0x0100, 0x4ca01887, 7 | BRF_GRA },           // 15
	{ "rom19",				0x0100, 0x513224f0, 7 | BRF_GRA },           // 16
};

STD_ROM_PICK(getstarb2)
STD_ROM_FN(getstarb2)

static INT32 getstar_mcusim_status_read()
{
	UINT8 states[3] = { 0xc7, 0x55, 0x00 };

	INT32 ret = states[protection_counter];

	protection_counter++;
	if (protection_counter > 2) protection_counter = 0;

	return ret;
}

static INT32 getstarb2_mcusim_read()
{
	UINT8 lookup_table[4] = { 0x00, 0x03, 0x04, 0x05 };

	if (ZetGetPC(-1) == 0x056e) return 0;
	if (ZetGetPC(-1) == 0x0570) return 1;
	if (ZetGetPC(-1) == 0x0577) return 0x53;
	if (ZetGetPC(-1) == 0x6b04) return lookup_table[protection_data];

	return 0;
}

static INT32 Getstarb2Init()
{
	INT32 nRet = SlapfighInit();

	if (nRet == 0)
	{
		pMCUStatusRead = getstar_mcusim_status_read;
		pMCURead = getstarb2_mcusim_read;
		pMCUWrite = getstar_mcusim_write;
	}

	return nRet;
}

struct BurnDriver BurnDrvGetstarb2 = {
	"getstarb2", "grdian", NULL, NULL, "1986",
	"Get Star (bootleg set 2)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_TOAPLAN_MISC, GBF_SCRFIGHT, 0,
	NULL, getstarb2RomInfo, getstarb2RomName, NULL, NULL, NULL, NULL, Getstarb2InputInfo, Getstarb2DIPInfo,
	Getstarb2Init, DrvExit, DrvFrame, SlapfighDraw, DrvScan, &DrvRecalc, 0x100,
	280, 240, 4, 3
};

