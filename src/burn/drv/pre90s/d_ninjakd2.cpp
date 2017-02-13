// FB Alpha UPL (Ninja Kid 2) driver module
// Based on MAME driver by Roberto Ventura, Leandro Dardini, Yochizo, Nicola Salmoria

#include "tiles_generic.h"
#include "z80_intf.h"
#include "mc8123.h"
#include "burn_ym2203.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvGfxROM4;
static UINT8 *DrvZ80Key;
static UINT8 *DrvSndROM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvSprRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvBgRAM;
static UINT8 *DrvBgRAM0;
static UINT8 *DrvBgRAM1;
static UINT8 *DrvBgRAM2;

static UINT16 *pSpriteDraw;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 *soundlatch;
static UINT8 *flipscreen;

static UINT16 scrollx[3];
static UINT16 scrolly[3];
static UINT8 tilemap_enable[3];
static UINT8 overdraw_enable;
static UINT8 nZ80RomBank;
static UINT8 nZ80RamBank[3];

static UINT8 m_omegaf_io_protection[3];
static UINT8 m_omegaf_io_protection_input;
static INT32 m_omegaf_io_protection_tic;

static INT32 ninjakd2_sample_offset;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static INT32 previous_coin[2];

static struct BurnInputInfo DrvInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Drv)

static struct BurnInputInfo Drv2InputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Drv2)

static struct BurnInputInfo OmegafInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 5,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Omegaf)

static struct BurnDIPInfo MnightDIPList[]=
{
	{0x11, 0xff, 0xff, 0xcf, NULL				},
	{0x12, 0xff, 0xff, 0xff, NULL				},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
//	{0x11, 0x01, 0x01, 0x01, "Off"				},
//	{0x11, 0x01, 0x01, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Bonus Life"			},
	{0x11, 0x01, 0x02, 0x02, "30k and every 50k"		},
	{0x11, 0x01, 0x02, 0x00, "50k and every 80k"		},

	{0   , 0xfe, 0   ,    2, "Difficulty"			},
	{0x11, 0x01, 0x04, 0x04, "Normal"			},
	{0x11, 0x01, 0x04, 0x00, "Difficult"			},

	{0   , 0xfe, 0   ,    2, "Infinite Lives"		},
	{0x11, 0x01, 0x08, 0x08, "Off"				},
	{0x11, 0x01, 0x08, 0x00, "On"				},

//	{0   , 0xfe, 0   ,    2, "Cabinet"			},
//	{0x11, 0x01, 0x10, 0x00, "Upright"			},
//	{0x11, 0x01, 0x10, 0x10, "Cocktail"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x11, 0x01, 0x20, 0x20, "Off"				},
	{0x11, 0x01, 0x20, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x11, 0x01, 0xc0, 0x80, "2"				},
	{0x11, 0x01, 0xc0, 0xc0, "3"				},
	{0x11, 0x01, 0xc0, 0x40, "4"				},
	{0x11, 0x01, 0xc0, 0x00, "5"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x12, 0x01, 0x01, 0x01, "Off"				},
	{0x12, 0x01, 0x01, 0x00, "On"				},

	{0   , 0xfe, 0   ,    8, "Coinage"			},
	{0x12, 0x01, 0xe0, 0x00, "5 Coins 1 Credits"		},
	{0x12, 0x01, 0xe0, 0x20, "4 Coins 1 Credits"		},
	{0x12, 0x01, 0xe0, 0x40, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0xe0, 0x60, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0xe0, 0xc0, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0xe0, 0x80, "1 Coin  4 Credits"		},
};

STDDIPINFO(Mnight)

static struct BurnDIPInfo Ninjakd2DIPList[]=
{
	{0x11, 0xff, 0xff, 0x6f, NULL				},
	{0x12, 0xff, 0xff, 0xfd, NULL				},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
//	{0x11, 0x01, 0x01, 0x01, "Off"				},
//	{0x11, 0x01, 0x01, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x11, 0x01, 0x06, 0x04, "20000 and every 50000"	},
	{0x11, 0x01, 0x06, 0x06, "30000 and every 50000"	},
	{0x11, 0x01, 0x06, 0x02, "50000 and every 50000"	},
	{0x11, 0x01, 0x06, 0x00, "None"				},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x11, 0x01, 0x08, 0x00, "No"				},
	{0x11, 0x01, 0x08, 0x08, "Yes"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x11, 0x01, 0x10, 0x10, "Off"				},
	{0x11, 0x01, 0x10, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Difficulty"			},
	{0x11, 0x01, 0x20, 0x20, "Normal"			},
	{0x11, 0x01, 0x20, 0x00, "Hard"				},

	{0   , 0xfe, 0   ,    2, "Lives"			},
	{0x11, 0x01, 0x40, 0x40, "3"				},
	{0x11, 0x01, 0x40, 0x00, "4"				},

	{0   , 0xfe, 0   ,    2, "Language"			},
	{0x11, 0x01, 0x80, 0x00, "English"			},
	{0x11, 0x01, 0x80, 0x80, "Japanese"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x12, 0x01, 0x01, 0x01, "Off"				},
	{0x12, 0x01, 0x01, 0x00, "On"				},
		
//	{0   , 0xfe, 0   ,    2, "Cabinet"			},
//	{0x12, 0x01, 0x02, 0x00, "Upright"			},
//	{0x12, 0x01, 0x02, 0x02, "Cocktail"			},

	{0   , 0xfe, 0   ,    2, "Credit Service"		},
	{0x12, 0x01, 0x04, 0x00, "Off"				},
	{0x12, 0x01, 0x04, 0x04, "On"				},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x12, 0x01, 0x18, 0x00, "2 Coins/1 Credit, 6/4"	},
	{0x12, 0x01, 0x18, 0x18, "1 Coin/1 Credit, 3/4"		},
	{0x12, 0x01, 0x18, 0x10, "1 Coin/2 Credits, 2/6, 3/10"	},
	{0x12, 0x01, 0x18, 0x08, "1 Coin/3 Credits, 3/12"	},
	{0x12, 0x01, 0x18, 0x00, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0x18, 0x18, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0x18, 0x10, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0x18, 0x08, "1 Coin  3 Credits"		},

	{0   , 0xfe, 0   ,   16, "Coin A"			},
	{0x12, 0x01, 0xe0, 0x00, "5 Coins/1 Credit, 15/4"	},
	{0x12, 0x01, 0xe0, 0x20, "4 Coins/1 Credit, 12/4"	},
	{0x12, 0x01, 0xe0, 0x40, "3 Coins/1 Credit, 9/4"	},
	{0x12, 0x01, 0xe0, 0x60, "2 Coins/1 Credit, 6/4"	},
	{0x12, 0x01, 0xe0, 0xe0, "1 Coin/1 Credit, 3/4"		},
	{0x12, 0x01, 0xe0, 0xc0, "1 Coin/2 Credits, 2/6, 3/10"	},
	{0x12, 0x01, 0xe0, 0xa0, "1 Coin/3 Credits, 3/12"	},
	{0x12, 0x01, 0xe0, 0x80, "1 Coin  4 Credits"		},
	{0x12, 0x01, 0xe0, 0x00, "5 Coins 1 Credits"		},
	{0x12, 0x01, 0xe0, 0x20, "4 Coins 1 Credits"		},
	{0x12, 0x01, 0xe0, 0x40, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0xe0, 0x60, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0xe0, 0xc0, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0xe0, 0x80, "1 Coin  4 Credits"		},
};

STDDIPINFO(Ninjakd2)

static struct BurnDIPInfo RdactionDIPList[]=
{
	{0x11, 0xff, 0xff, 0x6f, NULL				},
	{0x12, 0xff, 0xff, 0xfd, NULL				},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
//	{0x11, 0x01, 0x01, 0x01, "Off"				},
//	{0x11, 0x01, 0x01, 0x00, "On"				},

	{0   , 0xfe, 0   ,    3, "Bonus Life"			},
	{0x11, 0x01, 0x06, 0x04, "20000 and every 50000"	},
	{0x11, 0x01, 0x06, 0x06, "30000 and every 50000"	},
	{0x11, 0x01, 0x06, 0x02, "50000 and every 100000"	},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x11, 0x01, 0x08, 0x00, "No"				},
	{0x11, 0x01, 0x08, 0x08, "Yes"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x11, 0x01, 0x10, 0x10, "Off"				},
	{0x11, 0x01, 0x10, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Difficulty"			},
	{0x11, 0x01, 0x20, 0x20, "Normal"			},
	{0x11, 0x01, 0x20, 0x00, "Hard"				},

	{0   , 0xfe, 0   ,    2, "Lives"			},
	{0x11, 0x01, 0x40, 0x40, "3"				},
	{0x11, 0x01, 0x40, 0x00, "4"				},

	{0   , 0xfe, 0   ,    2, "Language"			},
	{0x11, 0x01, 0x80, 0x00, "English"			},
	{0x11, 0x01, 0x80, 0x80, "Japanese"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x12, 0x01, 0x01, 0x01, "Off"				},
	{0x12, 0x01, 0x01, 0x00, "On"				},

//	{0   , 0xfe, 0   ,    2, "Cabinet"			},
//	{0x12, 0x01, 0x02, 0x00, "Upright"			},
//	{0x12, 0x01, 0x02, 0x02, "Cocktail"			},

	{0   , 0xfe, 0   ,    2, "Credit Service"		},
	{0x12, 0x01, 0x04, 0x00, "Off"				},
	{0x12, 0x01, 0x04, 0x04, "On"				},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x12, 0x01, 0x18, 0x00, "2 Coins/1 Credit, 6/4"	},
	{0x12, 0x01, 0x18, 0x18, "1 Coin/1 Credit, 3/4"		},
	{0x12, 0x01, 0x18, 0x10, "1 Coin/2 Credits, 2/6, 3/10"	},
	{0x12, 0x01, 0x18, 0x08, "1 Coin/3 Credits, 3/12"	},
	{0x12, 0x01, 0x18, 0x00, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0x18, 0x18, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0x18, 0x10, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0x18, 0x08, "1 Coin  3 Credits"		},

	{0   , 0xfe, 0   ,   16, "Coin A"			},
	{0x12, 0x01, 0xe0, 0x00, "5 Coins/1 Credit, 15/4"	},
	{0x12, 0x01, 0xe0, 0x20, "4 Coins/1 Credit, 12/4"	},
	{0x12, 0x01, 0xe0, 0x40, "3 Coins/1 Credit, 9/4"	},
	{0x12, 0x01, 0xe0, 0x60, "2 Coins/1 Credit, 6/4"	},
	{0x12, 0x01, 0xe0, 0xe0, "1 Coin/1 Credit, 3/4"		},
	{0x12, 0x01, 0xe0, 0xc0, "1 Coin/2 Credits, 2/6, 3/10"	},
	{0x12, 0x01, 0xe0, 0xa0, "1 Coin/3 Credits, 3/12"	},
	{0x12, 0x01, 0xe0, 0x80, "1 Coin  4 Credits"		},
	{0x12, 0x01, 0xe0, 0x00, "5 Coins 1 Credits"		},
	{0x12, 0x01, 0xe0, 0x20, "4 Coins 1 Credits"		},
	{0x12, 0x01, 0xe0, 0x40, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0xe0, 0x60, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0xe0, 0xc0, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0xe0, 0x80, "1 Coin  4 Credits"		},
};

STDDIPINFO(Rdaction)

static struct BurnDIPInfo ArkareaDIPList[]=
{
	{0x11, 0xff, 0xff, 0xef, NULL				},
	{0x12, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    4, "Coinage"			},
	{0x11, 0x01, 0x03, 0x00, "3 Coins 1 Credits"		},
	{0x11, 0x01, 0x03, 0x01, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x03, 0x03, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x03, 0x02, "1 Coin  2 Credits"		},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
//	{0x11, 0x01, 0x04, 0x04, "Off"				},
//	{0x11, 0x01, 0x04, 0x00, "On"				},

	{0   , 0xfe, 0   ,    0, "Demo Sounds"			},
	{0x11, 0x01, 0x10, 0x10, "Off"				},
	{0x11, 0x01, 0x10, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Difficulty"			},
	{0x11, 0x01, 0x20, 0x20, "Normal"			},
	{0x11, 0x01, 0x20, 0x00, "Hard"				},

	{0   , 0xfe, 0   ,    2, "Bonus Life"			},
	{0x11, 0x01, 0x40, 0x40, "50000 and every 50000"	},
	{0x11, 0x01, 0x40, 0x00, "100000 and every 100000"	},

	{0   , 0xfe, 0   ,    2, "Lives"			},
	{0x11, 0x01, 0x80, 0x80, "3"				},
	{0x11, 0x01, 0x80, 0x00, "4"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x12, 0x01, 0x01, 0x01, "Off"				},
	{0x12, 0x01, 0x01, 0x00, "On"				},
};

STDDIPINFO(Arkarea)

static struct BurnDIPInfo RobokidDIPList[]=
{
	{0x11, 0xff, 0xff, 0xcf, NULL				},
	{0x12, 0xff, 0xff, 0xff, NULL				},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
//	{0x11, 0x01, 0x01, 0x01, "Off"				},
//	{0x11, 0x01, 0x01, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Bonus Life"			},
	{0x11, 0x01, 0x02, 0x02, "50000 and every 100000"		},
	{0x11, 0x01, 0x02, 0x00, "None"				},

	{0   , 0xfe, 0   ,    2, "Difficulty"			},
	{0x11, 0x01, 0x04, 0x04, "Normal"			},
	{0x11, 0x01, 0x04, 0x00, "Hard"				},

	{0   , 0xfe, 0   ,    2, "Free Play"			},
	{0x11, 0x01, 0x08, 0x08, "Off"				},
	{0x11, 0x01, 0x08, 0x00, "On"				},

//	{0   , 0xfe, 0   ,    2, "Cabinet"			},
//	{0x11, 0x01, 0x10, 0x00, "Upright"			},
//	{0x11, 0x01, 0x10, 0x10, "Cocktail"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x11, 0x01, 0x20, 0x20, "Off"				},
	{0x11, 0x01, 0x20, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x11, 0x01, 0xc0, 0x80, "2"				},
	{0x11, 0x01, 0xc0, 0xc0, "3"				},
	{0x11, 0x01, 0xc0, 0x40, "4"				},
	{0x11, 0x01, 0xc0, 0x00, "5"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x12, 0x01, 0x01, 0x01, "Off"				},
	{0x12, 0x01, 0x01, 0x00, "On"				},

	{0   , 0xfe, 0   ,    8, "Coinage"			},
	{0x12, 0x01, 0xe0, 0x00, "5 Coins 1 Credits"		},
	{0x12, 0x01, 0xe0, 0x20, "4 Coins 1 Credits"		},
	{0x12, 0x01, 0xe0, 0x40, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0xe0, 0x60, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0xe0, 0xc0, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0xe0, 0x80, "1 Coin  4 Credits"		},
};

STDDIPINFO(Robokid)

static struct BurnDIPInfo RobokidjDIPList[]=
{
	{0x11, 0xff, 0xff, 0xcf, NULL				},
	{0x12, 0xff, 0xff, 0xff, NULL				},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
//	{0x11, 0x01, 0x01, 0x01, "Off"				},
//	{0x11, 0x01, 0x01, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Bonus Life"			},
	{0x11, 0x01, 0x02, 0x02, "30000 and every 50000"	},
	{0x11, 0x01, 0x02, 0x00, "50000 and every 80000"	},

	{0   , 0xfe, 0   ,    2, "Difficulty"			},
	{0x11, 0x01, 0x04, 0x04, "Normal"			},
	{0x11, 0x01, 0x04, 0x00, "Hard"				},

	{0   , 0xfe, 0   ,    2, "Free Play"			},
	{0x11, 0x01, 0x08, 0x08, "Off"				},
	{0x11, 0x01, 0x08, 0x00, "On"				},

//	{0   , 0xfe, 0   ,    2, "Cabinet"			},
//	{0x11, 0x01, 0x10, 0x00, "Upright"			},
//	{0x11, 0x01, 0x10, 0x10, "Cocktail"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x11, 0x01, 0x20, 0x20, "Off"				},
	{0x11, 0x01, 0x20, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x11, 0x01, 0xc0, 0x80, "2"				},
	{0x11, 0x01, 0xc0, 0xc0, "3"				},
	{0x11, 0x01, 0xc0, 0x40, "4"				},
	{0x11, 0x01, 0xc0, 0x00, "5"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x12, 0x01, 0x01, 0x01, "Off"				},
	{0x12, 0x01, 0x01, 0x00, "On"				},

	{0   , 0xfe, 0   ,    8, "Coinage"			},
	{0x12, 0x01, 0xe0, 0x00, "5 Coins 1 Credits"		},
	{0x12, 0x01, 0xe0, 0x20, "4 Coins 1 Credits"		},
	{0x12, 0x01, 0xe0, 0x40, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0xe0, 0x60, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0xe0, 0xc0, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0xe0, 0x80, "1 Coin  4 Credits"		},
};

STDDIPINFOEXT(Robokidj, Robokid, Robokidj)

static struct BurnDIPInfo OmegafDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL				},
	{0x13, 0xff, 0xff, 0xff, NULL				},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
//	{0x12, 0x01, 0x01, 0x01, "Off"				},
//	{0x12, 0x01, 0x01, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x12, 0x01, 0x06, 0x00, "Easy"				},
	{0x12, 0x01, 0x06, 0x06, "Normal"			},
	{0x12, 0x01, 0x06, 0x02, "Hard"				},
	{0x12, 0x01, 0x06, 0x04, "Hardest"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x12, 0x01, 0x08, 0x08, "Off"				},
	{0x12, 0x01, 0x08, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x12, 0x01, 0x20, 0x00, "Off"				},
	{0x12, 0x01, 0x20, 0x20, "On"				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x12, 0x01, 0xc0, 0x00, "2"				},
	{0x12, 0x01, 0xc0, 0xc0, "3"				},
	{0x12, 0x01, 0xc0, 0x40, "4"				},
	{0x12, 0x01, 0xc0, 0x80, "5"				},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x13, 0x01, 0x03, 0x00, "200000"			},
	{0x13, 0x01, 0x03, 0x03, "300000"			},
	{0x13, 0x01, 0x03, 0x01, "500000"			},
	{0x13, 0x01, 0x03, 0x02, "1000000"			},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x13, 0x01, 0x1c, 0x00, "5 Coins 1 Credits"		},
	{0x13, 0x01, 0x1c, 0x10, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x1c, 0x08, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x1c, 0x18, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x1c, 0x1c, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x1c, 0x0c, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x1c, 0x14, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0x1c, 0x04, "1 Coin  4 Credits"		},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x13, 0x01, 0xe0, 0x00, "5 Coins 1 Credits"		},
	{0x13, 0x01, 0xe0, 0x80, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0xe0, 0x40, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0xe0, 0xc0, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0xe0, 0x60, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0xe0, 0x20, "1 Coin  4 Credits"		},
};

STDDIPINFO(Omegaf)

static void DrvPaletteUpdate(INT32 offset)
{
	offset &= 0x7fe;

	INT32 p = (DrvPalRAM[offset+0] * 256) + DrvPalRAM[offset+1];

	INT32 r = p >> 12;
	INT32 g = (p >> 8) & 0xf;
	INT32 b = (p >> 4) & 0xf;

	r |= r << 4;
	g |= g << 4;
	b |= b << 4;

	DrvPalette[offset/2] = BurnHighCol(r,g,b,0);
}

static void ninjakd2_bankswitch(INT32 data)
{
	INT32 nBank = 0x10000 + (data * 0x4000);

	nZ80RomBank = data;

	ZetMapMemory(DrvZ80ROM0 + nBank, 	0x8000, 0xbfff, MAP_ROM);
}

static void ninjakd2_bgconfig(INT32 sel, INT32 offset, UINT8 data)
{
	switch (offset & 0x07)
	{
		case 0:
			scrollx[sel] = (scrollx[sel] & 0x700) + data;
		return;

		case 1:
			scrollx[sel] = (scrollx[sel] & 0x0ff) + ((data & 0x07) * 256);
		return;

		case 2:
			scrolly[sel] = (scrolly[sel] & 0x100) + data;
		return;

		case 3:
			scrolly[sel] = (scrolly[sel] & 0x0ff) + ((data & 0x01) * 256);
		return;

		case 4:
			tilemap_enable[sel] = data & 0x01;
		return;
	}
}

static UINT8 __fastcall ninjakd2_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xc000:
		case 0xc001:
		case 0xc002:
		case 0xdc00:
		case 0xdc01:
		case 0xdc02:
		case 0xf800:
		case 0xf801:
		case 0xf802:
			return DrvInputs[address & 3];

		case 0xc003:
		case 0xc004:
		case 0xdc03:
		case 0xdc04:
		case 0xf803:
		case 0xf804:
			return DrvDips[(address & 7) - 3];
	}

	return 0;
}

static void __fastcall ninjakd2_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf800) == 0xc800) {
		DrvPalRAM[address & 0x7ff] = data;
		DrvPaletteUpdate(address);
		return;
	}

	switch (address)
	{
		case 0xc200:
			*soundlatch = data;
		return;

		case 0xc201:
		{
			if (data & 0x10)
			{
				ZetClose();
				ZetOpen(1);
				ZetReset();
				ZetClose();
				ZetOpen(0);
			}

			*flipscreen = data & 0x80;
		}
		return;

		case 0xc202:
			ninjakd2_bankswitch(data & 0x07);
		return;

		case 0xc203:
			overdraw_enable = data & 0x01;
		return;

		case 0xc208:
		case 0xc209:
		case 0xc20a:
		case 0xc20b:
		case 0xc20c:
			ninjakd2_bgconfig(0, address, data);
		return;
	}
}

static void __fastcall mnight_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf800) == 0xf000) {
		DrvPalRAM[address & 0x7ff] = data;
		DrvPaletteUpdate(address);
		return;
	}

	switch (address)
	{
		case 0xfa00:
			*soundlatch = data;
		return;

		case 0xfa01:
		{
			if (data & 0x10)
			{
				ZetClose();
				ZetOpen(1);
				ZetReset();
				ZetClose();
				ZetOpen(0);
			}

			*flipscreen = data & 0x80;
		}
		return;

		case 0xfa02:
			ninjakd2_bankswitch(data & 0x07);
		return;

		case 0xfa03:
			overdraw_enable = data & 0x01;
		return;

		case 0xfa08:
		case 0xfa09:
		case 0xfa0a:
		case 0xfa0b:
		case 0xfa0c:
			ninjakd2_bgconfig(0, address, data);
		return;
	}
}

static void robokid_rambank(INT32 sel, UINT8 data)
{
	UINT8 *ram[3] = { DrvBgRAM0, DrvBgRAM1, DrvBgRAM2 };
	INT32 off[2][3]  = { { 0xd800, 0xd400, 0xd000 }, { 0xc400, 0xc800, 0xcc00 } };

	INT32 nBank = 0x400 * data;

	nZ80RamBank[sel&3] = data;

	ZetMapMemory(ram[sel&3] + nBank, off[sel>>2][sel&3], off[sel>>2][sel&3] | 0x3ff, MAP_RAM);
}

static void __fastcall robokid_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf800) == 0xc000) {
		DrvPalRAM[address & 0x7ff] = data;
		DrvPaletteUpdate(address);
		return;
	}

	switch (address)
	{
		case 0xdc00:
			*soundlatch = data;
		return;

		case 0xdc01:
		{
			if (data & 0x10) {
				ZetClose();
				ZetOpen(1);
				ZetReset();
				ZetClose();
				ZetOpen(0);
			}

			*flipscreen = data & 0x80;
		}
		return;

		case 0xdc02:
			ninjakd2_bankswitch(data & 0x0f);
		return;

		case 0xdc03:
			overdraw_enable = data & 0x01;
		return;

		case 0xdd00:
		case 0xdd01:
		case 0xdd02:
		case 0xdd03:
		case 0xdd04:
			ninjakd2_bgconfig(0, address, data);
		return;

		case 0xdd05:
			robokid_rambank(0, data & 1);
		return;

		case 0xde00:
		case 0xde01:
		case 0xde02:
		case 0xde03:
		case 0xde04:
			ninjakd2_bgconfig(1, address, data);
		return;

		case 0xde05:
			robokid_rambank(1, data & 1);
		return;

		case 0xdf00:
		case 0xdf01:
		case 0xdf02:
		case 0xdf03:
		case 0xdf04:
			ninjakd2_bgconfig(2, address, data);
		return;

		case 0xdf05:
			robokid_rambank(2, data & 1);
		return;
	}
}

// Copied directly from MAME
static UINT8 omegaf_protection_read(INT32 offset)
{
	UINT8 result = 0xff;

	switch (m_omegaf_io_protection[1] & 3)
	{
		case 0:
			switch (offset)
			{
				case 1:
					switch (m_omegaf_io_protection[0] & 0xe0)
					{
						case 0x00:
							if (++m_omegaf_io_protection_tic & 1)
							{
								result = 0x00;
							}
							else
							{
								switch (m_omegaf_io_protection_input)
								{
									// first interrogation
									// this happens just after setting mode 0.
									// input is not explicitly loaded so could be anything
									case 0x00:
										result = 0x80 | 0x02;
										break;

									// second interrogation
									case 0x8c:
										result = 0x80 | 0x1f;
										break;

									// third interrogation
									case 0x89:
										result = 0x80 | 0x0b;
										break;
								}
							}
							break;

						case 0x20:
							result = 0xc7;
							break;

						case 0x60:
							result = 0x00;
							break;

						case 0x80:
							result = 0x20 | (m_omegaf_io_protection_input & 0x1f);
							break;

						case 0xc0:
							result = 0x60 | (m_omegaf_io_protection_input & 0x1f);
							break;
					}
					break;
			}
			break;

		case 1: // dip switches
			switch (offset)
			{
				case 0: result = DrvDips[0]; break;
				case 1: result = DrvDips[1]; break;
				case 2: result = 0x02; break;
			}
			break;

		case 2: // player inputs
			switch (offset)
			{
				case 0: result = DrvInputs[1]; break;
				case 1: result = DrvInputs[2]; break;
				case 2: result = 0x01; break;
			}
			break;
	}

	return result;
}

static UINT8 __fastcall omegaf_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xc000:
			return DrvInputs[0];

		case 0xc001:
		case 0xc002:
		case 0xc003:
			return omegaf_protection_read(address - 0xc001);
	}

	return 0;
}

static void __fastcall omegaf_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf800) == 0xd800) {
		DrvPalRAM[address & 0x7ff] = data;
		DrvPaletteUpdate(address);
		return;
	}

	switch (address)
	{
		case 0xc000:
			*soundlatch = data;
		return;

		case 0xc001:
		{
			if (data & 0x10) {
				ZetClose();
				ZetOpen(1);
				ZetReset();
				ZetClose();
				ZetOpen(0);
			}

			*flipscreen = data & 0x80;
		}
		return;

		case 0xc002:
			ninjakd2_bankswitch(data & 0x0f);
		return;

		case 0xc003:
			overdraw_enable = data & 0x01;
		return;

		case 0xc004:
		case 0xc005:
		case 0xc006:
		{
			if (address == 0xc006 && (data & 1) && !(m_omegaf_io_protection[2] & 1))
			{
				m_omegaf_io_protection_input = m_omegaf_io_protection[0];
			}
		
			m_omegaf_io_protection[address - 0xc004] = data;
		}
		return;

		case 0xc100:
		case 0xc101:
		case 0xc102:
		case 0xc103:
		case 0xc104:
			ninjakd2_bgconfig(0, address, data);
		return;

		case 0xc105:
			robokid_rambank(4|0, data & 7);
		return;

		case 0xc200:
		case 0xc201:
		case 0xc202:
		case 0xc203:
		case 0xc204:
			ninjakd2_bgconfig(1, address, data);
		return;

		case 0xc205:
			robokid_rambank(4|1, data & 7);
		return;

		case 0xc300:
		case 0xc301:
		case 0xc302:
		case 0xc303:
		case 0xc304:
			ninjakd2_bgconfig(2, address, data);
		return;

		case 0xc305:
			robokid_rambank(4|2, data & 7);
		return;
	}
}

static void ninjakd2_sample_player(INT16 *dest, INT32 len)
{
	if (ninjakd2_sample_offset == -1) return;

	for (INT32 i = 0; i < len; i++)
	{
		UINT16 ofst =  ninjakd2_sample_offset + ((i * 271) / len);

		if (DrvSndROM[ofst] == 0) {
			ninjakd2_sample_offset = -1;
			break;
		}

		INT32 sample = BURN_SND_CLIP(((DrvSndROM[ofst]<<7) * 45) / 100);

		dest[i*2+0] = BURN_SND_CLIP(dest[i*2+0]+sample);
		dest[i*2+1] = BURN_SND_CLIP(dest[i*2+1]+sample);
	}

	if (ninjakd2_sample_offset != -1)
		ninjakd2_sample_offset += 271;
}

static void __fastcall ninjakd2_sound_write(UINT16 address, UINT8 data)
{
	data = data;

	switch (address)
	{
		case 0xf000:
			ninjakd2_sample_offset = data << 8;
		return;
	}
}

static UINT8 __fastcall ninjakd2_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xe000:
			return *soundlatch;
	}

	return 0;
}

static void __fastcall ninjakd2_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
		case 0x80:
		case 0x81:
			BurnYM2203Write((port >> 7) & 1, port & 1, data);
		return;
	}
}

inline static void DrvYM2203IRQHandler(INT32, INT32 nStatus)
{
	ZetSetIRQLine(0, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

inline static INT32 DrvSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)ZetTotalCycles() * nSoundRate / 5000000;
}

inline static double DrvGetTime()
{
	return (double)ZetTotalCycles() / 5000000.0;
}

static void ninjakd2_sound_init()
{
	ZetInit(1);
	ZetOpen(1);

//	ZetMapMemory(DrvZ80ROM1, 0x0000, 0xbfff, MAP_ROM);

	ZetMapArea(0x0000, 0xbfff, 0, DrvZ80ROM1);
	ZetMapArea(0x0000, 0xbfff, 2, DrvZ80ROM1 + 0x10000, DrvZ80ROM1);

	ZetMapMemory(DrvZ80RAM1,		0xc000, 0xc7ff, MAP_RAM);
	ZetSetOutHandler(ninjakd2_sound_write_port);
	ZetSetWriteHandler(ninjakd2_sound_write);
	ZetSetReadHandler(ninjakd2_sound_read);
	ZetClose();

	BurnYM2203Init(2,  1500000, &DrvYM2203IRQHandler, DrvSynchroniseStream, DrvGetTime, 0);
	BurnTimerAttachZet(5000000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE,   0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.10, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.10, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.10, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_YM2203_ROUTE,   0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_1, 0.10, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_2, 0.10, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_3, 0.10, BURN_SND_ROUTE_BOTH);
}

static INT32 DrvDoReset()
{
	memset(AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	BurnYM2203Reset();
	ZetClose();

	memset (scrollx, 0, 3 * sizeof(UINT16));
	memset (scrolly, 0, 3 * sizeof(UINT16));

	nZ80RomBank = 0;
	memset (nZ80RamBank, 0, 3);

	overdraw_enable = 0;
	memset (tilemap_enable, 0, 3);

	memset (m_omegaf_io_protection, 0, 3);
	m_omegaf_io_protection_input = 0;
	m_omegaf_io_protection_tic = 0;

	ninjakd2_sample_offset = -1;

	previous_coin[0] = previous_coin[1] = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0	= Next; Next += 0x050000;
	DrvZ80ROM1	= Next; Next += 0x020000;

	DrvGfxROM0	= Next; Next += 0x010000;
	DrvGfxROM1	= Next; Next += 0x080000;
	DrvGfxROM2	= Next; Next += 0x100000;
	DrvGfxROM3	= Next; Next += 0x100000;
	DrvGfxROM4	= Next; Next += 0x100000;

	DrvZ80Key	= Next; Next += 0x002000;

	DrvSndROM	= Next; Next += 0x010000;

	DrvPalette	= (UINT32*)Next; Next += 0x0400 * sizeof(UINT32);

	AllRam		= Next;

	DrvZ80RAM0	= Next; Next += 0x001a00;
	DrvZ80RAM1	= Next; Next += 0x000800;
	DrvSprRAM	= Next; Next += 0x000600;
	DrvPalRAM	= Next; Next += 0x000800;
	DrvFgRAM	= Next; Next += 0x000800;
	DrvBgRAM0	= Next;
	DrvBgRAM	= Next; Next += 0x002000;
	DrvBgRAM1	= Next; Next += 0x002000;
	DrvBgRAM2	= Next; Next += 0x002000;

	soundlatch	= Next; Next += 0x000001;
	flipscreen	= Next; Next += 0x000001;

	pSpriteDraw	= (UINT16*)Next; Next += 256 * 256 * sizeof(UINT16);

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static INT32 DrvGfxDecode(UINT8 *rom, INT32 len, INT32 type)
{
	INT32 Plane[4]   = { STEP4(0,1) };
	INT32 XOffs0[16] = { STEP8(0,4), STEP8(32*8,4) };
	INT32 XOffs1[16] = { STEP8(0,4), STEP8(64*8,4) };
	INT32 YOffs0[16] = { STEP8(0,32), STEP8(64*8,32) };
	INT32 YOffs1[16] = { STEP16(0,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(len);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, rom, len);

	switch (type)
	{
		case 0:
			GfxDecode((len * 2) / ( 8 *  8), 4,  8,  8, Plane, XOffs0, YOffs0, 0x100, tmp, rom);
		break;

		case 1:
			GfxDecode((len * 2) / (16 * 16), 4, 16, 16, Plane, XOffs0, YOffs0, 0x400, tmp, rom);
		break;

		case 2:
			GfxDecode((len * 2) / (16 * 16), 4, 16, 16, Plane, XOffs1, YOffs1, 0x400, tmp, rom);
		break;
	}

	BurnFree (tmp);

	return 0;
}

static void lineswap_gfx_roms(UINT8 *rom, INT32 len, const INT32 bit)
{
	UINT8 *tmp = (UINT8*)BurnMalloc(len);

	const INT32 mask = (1 << (bit + 1)) - 1;

	for (INT32 sa = 0; sa < len; sa++)
	{
		const INT32 da = (sa & ~mask) | ((sa << 1) & mask) | ((sa >> bit) & 1);
		tmp[da] = rom[sa];
	}

	memcpy (rom, tmp, len);

	BurnFree (tmp);
}

static void gfx_unscramble(INT32 gfxlen)
{
	lineswap_gfx_roms(DrvGfxROM0, 0x08000, 13);
	lineswap_gfx_roms(DrvGfxROM1, gfxlen, 14);
	lineswap_gfx_roms(DrvGfxROM2, gfxlen, 14);
}

static INT32 Ninjakd2CommonInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x10000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x18000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x20000,  3, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x28000,  4, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  6, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x10000,  8, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x10000, 10, 1)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x00000, 11, 1)) return 1;

		gfx_unscramble(0x20000);
		DrvGfxDecode(DrvGfxROM0, 0x08000, 0);
		DrvGfxDecode(DrvGfxROM1, 0x20000, 1);
		DrvGfxDecode(DrvGfxROM2, 0x20000, 1);
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80ROM0 + 0x10000, 	0x8000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvPalRAM,			0xc800, 0xcdff, MAP_ROM);
	ZetMapMemory(DrvFgRAM,			0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvBgRAM,			0xd800, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM0,		0xe000, 0xf9ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0xfa00, 0xffff, MAP_RAM);
	ZetSetWriteHandler(ninjakd2_main_write);
	ZetSetReadHandler(ninjakd2_main_read);
	ZetClose();

	ninjakd2_sound_init();

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 Ninjakd2Init()
{
	INT32 nRet = Ninjakd2CommonInit();

	if (nRet == 0)
	{
		if (BurnLoadRom(DrvZ80Key  + 0x00000, 12, 1)) return 1;

		mc8123_decrypt_rom(0, 0, DrvZ80ROM1, DrvZ80ROM1 + 0x10000, DrvZ80Key);
	}

	return nRet;
}

static INT32 Ninjakd2DecryptedInit()
{
	INT32 nRet = Ninjakd2CommonInit();

	if (nRet == 0)
	{
		memcpy (DrvZ80ROM1 + 0x10000, DrvZ80ROM1, 0x10000);
		memcpy (DrvZ80ROM1, DrvZ80ROM1 + 0x08000, 0x08000);
	}

	return nRet;
}

static INT32 MnightInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x10000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x18000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x20000,  3, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x28000,  4, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  5, 1)) return 1;
		memcpy (DrvZ80ROM1 + 0x10000, DrvZ80ROM1, 0x10000);

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  6, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x10000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x20000,  9, 1)) return 1;
		memcpy (DrvGfxROM1 + 0x30000, DrvGfxROM1 + 0x20000, 0x10000);

		if (BurnLoadRom(DrvGfxROM2 + 0x00000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x10000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x20000, 12, 1)) return 1;
		memcpy (DrvGfxROM1 + 0x30000, DrvGfxROM1 + 0x20000, 0x10000);

		gfx_unscramble(0x40000);
		DrvGfxDecode(DrvGfxROM0, 0x08000, 0);
		DrvGfxDecode(DrvGfxROM1, 0x40000, 1);
		DrvGfxDecode(DrvGfxROM2, 0x40000, 1);
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80ROM0 + 0x10000, 	0x8000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,		0xc000, 0xd9ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0xda00, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvBgRAM,			0xe000, 0xe7ff, MAP_RAM);
	ZetMapMemory(DrvFgRAM,			0xe800, 0xefff, MAP_RAM);
	ZetMapMemory(DrvPalRAM,			0xf000, 0xf5ff, MAP_ROM);
	ZetSetWriteHandler(mnight_main_write);
	ZetSetReadHandler(ninjakd2_main_read);
	ZetClose();

	ninjakd2_sound_init();
	BurnYM2203SetPSGVolume(0, 0.05);
	BurnYM2203SetPSGVolume(1, 0.05);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 RobokidInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x10000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x20000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x30000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x40000,  3, 1)) return 1;
		memcpy (DrvZ80ROM0, DrvZ80ROM0 + 0x10000, 0x10000);

		if (BurnLoadRom(DrvZ80ROM1 + 0x10000,  4, 1)) return 1;
		memcpy (DrvZ80ROM1, DrvZ80ROM1 + 0x10000, 0x10000);

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x10000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x20000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x30000,  9, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x10000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x20000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x30000, 13, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x40000, 14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x50000, 15, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x60000, 16, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x00000, 17, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x10000, 18, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x20000, 19, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x30000, 20, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x40000, 21, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x50000, 22, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x60000, 23, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x70000, 24, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM4 + 0x00000, 25, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM4 + 0x10000, 26, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM4 + 0x20000, 27, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM4 + 0x30000, 28, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM4 + 0x40000, 29, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM4 + 0x50000, 30, 1)) return 1;

		DrvGfxDecode(DrvGfxROM0, 0x08000, 0);
		DrvGfxDecode(DrvGfxROM1, 0x40000, 2);
		DrvGfxDecode(DrvGfxROM2, 0x80000, 2);
		DrvGfxDecode(DrvGfxROM3, 0x80000, 2);
		DrvGfxDecode(DrvGfxROM4, 0x80000, 2);
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80ROM0 + 0x10000, 	0x8000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvPalRAM,			0xc000, 0xc7ff, MAP_ROM);
	ZetMapMemory(DrvFgRAM,			0xc800, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvBgRAM2,			0xd000, 0xd3ff, MAP_RAM);
	ZetMapMemory(DrvBgRAM1,			0xd400, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvBgRAM0,			0xd800, 0xdbff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM0,		0xe000, 0xf9ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0xfa00, 0xffff, MAP_RAM);
	ZetSetWriteHandler(robokid_main_write);
	ZetSetReadHandler(ninjakd2_main_read);
	ZetClose();

	ninjakd2_sound_init();
	BurnYM2203SetPSGVolume(0, 0.03);
	BurnYM2203SetPSGVolume(1, 0.03);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 OmegafInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x10000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x30000,  1, 1)) return 1;
		memcpy (DrvZ80ROM0, DrvZ80ROM0 + 0x10000, 0x10000);

		if (BurnLoadRom(DrvZ80ROM1 + 0x10000,  2, 1)) return 1;
		memcpy (DrvZ80ROM1, DrvZ80ROM1 + 0x10000, 0x10000);

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x00000,  6, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM4 + 0x00000,  7, 1)) return 1;

		DrvGfxDecode(DrvGfxROM0, 0x08000, 0);
		DrvGfxDecode(DrvGfxROM1, 0x20000, 2);
		DrvGfxDecode(DrvGfxROM2, 0x80000, 2);
		DrvGfxDecode(DrvGfxROM3, 0x80000, 2);
		DrvGfxDecode(DrvGfxROM4, 0x80000, 2);
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80ROM0 + 0x10000, 	0x8000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvBgRAM0,			0xc400, 0xc7ff, MAP_RAM);
	ZetMapMemory(DrvBgRAM1,			0xc800, 0xcbff, MAP_RAM);
	ZetMapMemory(DrvBgRAM2,			0xcc00, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvFgRAM,			0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvPalRAM,			0xd800, 0xdfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,		0xe000, 0xf9ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0xfa00, 0xffff, MAP_RAM);
	ZetSetWriteHandler(omegaf_main_write);
	ZetSetReadHandler(omegaf_main_read);
	ZetClose();

	ninjakd2_sound_init();
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE,   0.80, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_YM2203_ROUTE,   0.80, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	BurnYM2203Exit();

	GenericTilesExit();

	ZetExit();

	BurnFree (AllMem);

	return 0;
}

static void DrvCalculatePalette()
{
	for (INT32 i = 0; i < 0x800; i+=2)
	{
		DrvPaletteUpdate(i);
	}
}

static void draw_bg_layer()
{
	INT32 xscroll = (scrollx[0] + 0) & 0x1ff;
	INT32 yscroll = (scrolly[0] + 32) & 0x1ff;

	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sx = (offs & 0x1f) * 16;
		INT32 sy = (offs / 0x20) * 16;

		sx -= xscroll;
		if (sx < -15) sx += 512;
		sy -= yscroll;
		if (sy < -15) sy += 512;

		if (sx >= nScreenWidth || sy >= nScreenHeight) continue;

		INT32 attr  = DrvBgRAM[offs*2+1];
		INT32 code  = DrvBgRAM[offs*2+0] + ((attr & 0xc0) << 2);
		INT32 flipx = attr & 0x10;
		INT32 flipy = attr & 0x20;
		INT32 color = attr & 0x0f;

		if (flipy) {
			if (flipx) {
				Render16x16Tile_FlipXY_Clip(pTransDraw, code, sx, sy, color, 4, 0, DrvGfxROM2);
			} else {
				Render16x16Tile_FlipY_Clip(pTransDraw, code, sx, sy, color, 4, 0, DrvGfxROM2);
			}
		} else {
			if (flipx) {
				Render16x16Tile_FlipX_Clip(pTransDraw, code, sx, sy, color, 4, 0, DrvGfxROM2);
			} else {
				Render16x16Tile_Clip(pTransDraw, code, sx, sy, color, 4, 0, DrvGfxROM2);
			}
		}
	}
}

static void draw_mnight_bg_layer()
{
	INT32 xscroll = (scrollx[0] + 0) & 0x1ff;
	INT32 yscroll = (scrolly[0] + 32) & 0x1ff;

	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sx = (offs & 0x1f) * 16;
		INT32 sy = (offs / 0x20) * 16;

		sx -= xscroll;
		if (sx < -15) sx += 512;
		sy -= yscroll;
		if (sy < -15) sy += 512;

		if (sx >= nScreenWidth || sy >= nScreenHeight) continue;

		INT32 attr  = DrvBgRAM[offs*2+1];
		INT32 code  = DrvBgRAM[offs*2+0] + ((attr & 0xc0) << 2) + ((attr & 0x10) << 6);
		INT32 flipx = 0;
		INT32 flipy = attr & 0x20;
		INT32 color = attr & 0x0f;

		if (flipy) {
			if (flipx) {
				Render16x16Tile_FlipXY_Clip(pTransDraw, code, sx, sy, color, 4, 0, DrvGfxROM2);
			} else {
				Render16x16Tile_FlipY_Clip(pTransDraw, code, sx, sy, color, 4, 0, DrvGfxROM2);
			}
		} else {
			if (flipx) {
				Render16x16Tile_FlipX_Clip(pTransDraw, code, sx, sy, color, 4, 0, DrvGfxROM2);
			} else {
				Render16x16Tile_Clip(pTransDraw, code, sx, sy, color, 4, 0, DrvGfxROM2);
			}
		}
	}
}

static void draw_robokid_bg_layer(INT32 sel, UINT8 *ram, UINT8 *rom, INT32 width, INT32 transp)
{
	if (tilemap_enable[sel] == 0) return;

	INT32 wide = (width) ? 128 : 32;
	INT32 xscroll = scrollx[sel] & ((wide * 16) - 1);
	INT32 yscroll = (scrolly[sel] + 32) & 0x1ff;

	for (INT32 offs = 0; offs < wide * 32; offs++)
	{
		INT32 sx = (offs % wide);
		INT32 sy = (offs / wide);

		INT32 ofst = (sx & 0x0f) + (sy * 16) + ((sx & 0x70) * 0x20);

		sx = (sx * 16) - xscroll;
		if (sx < -15) sx += wide * 16;
		sy = (sy * 16) - yscroll;
		if (sy < -15) sy += 32 * 16;

		if (sx >= nScreenWidth || sy >= nScreenHeight) continue;

		INT32 attr  = ram[ofst * 2 + 1];
		INT32 code  = ram[ofst * 2 + 0] + ((attr & 0x10) << 7) + ((attr & 0x20) << 5) + ((attr & 0xc0) << 2);
		INT32 color = attr & 0x0f;

		if (transp) {
			Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 4, 0xf, 0, rom);
		} else {
			Render16x16Tile_Clip(pTransDraw, code, sx, sy, color, 4, 0, rom);
		}
	}
}

static void draw_fg_layer(INT32 color_offset)
{
	for (INT32 offs = (32 * 4); offs < (32 * 32) - (32 * 4); offs++)
	{
		INT32 sx = (offs & 0x1f) * 8;
		INT32 sy = (offs / 0x20) * 8;

		INT32 attr  = DrvFgRAM[offs*2+1];
		INT32 code  = DrvFgRAM[offs*2+0] + ((attr & 0xc0) << 2);
		INT32 flipx = attr & 0x10;
		INT32 flipy = attr & 0x20;
		INT32 color = attr & 0x0f;

		if (flipy) {
			if (flipx) {
				Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy - 32, color, 4, 0xf, color_offset, DrvGfxROM0);
			} else {
				Render8x8Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy - 32, color, 4, 0xf, color_offset, DrvGfxROM0);
			}
		} else {
			if (flipx) {
				Render8x8Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy - 32, color, 4, 0xf, color_offset, DrvGfxROM0);
			} else {
				Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy - 32, color, 4, 0xf, color_offset, DrvGfxROM0);
			}
		}
	}
}

static void draw_sprites(INT32 color_offset, INT32 robokid)
{
	int const big_xshift = robokid ? 1 : 0;
	int const big_yshift = robokid ? 0 : 1;

	UINT8* sprptr = DrvSprRAM + 11;
	int sprites_drawn = 0;

	while (1)
	{
		if (sprptr[2] & 0x02)
		{
			int sx = sprptr[1] - ((sprptr[2] & 0x01) << 8);
			int sy = sprptr[0];

			int code = sprptr[3] + ((sprptr[2] & 0xc0) << 2) + ((sprptr[2] & 0x08) << 7);
			int flipx = (sprptr[2] & 0x10) >> 4;
			int flipy = (sprptr[2] & 0x20) >> 5;
			int const color = sprptr[4] & 0x0f;

			int const big = (sprptr[2] & 0x04) >> 2;

			if (*flipscreen)
			{
				sx = 240 - 16*big - sx;
				sy = 240 - 16*big - sy;
				flipx ^= 1;
				flipy ^= 1;
			}

			if (big)
			{
				code &= ~3;
				code ^= flipx << big_xshift;
				code ^= flipy << big_yshift;
			}

			for (int y = 0; y <= big; ++y)
			{
				for (int x = 0; x <= big; ++x)
				{
					int const tile = code ^ (x << big_xshift) ^ (y << big_yshift);

					if (flipy) {
						if (flipx) {
							Render16x16Tile_Mask_FlipXY_Clip(pSpriteDraw, tile, sx + 16*x, (sy + 16*y) - 32, color, 4, 0xf, color_offset, DrvGfxROM1);
						} else {
							Render16x16Tile_Mask_FlipY_Clip(pSpriteDraw, tile, sx + 16*x, (sy + 16*y) - 32, color, 4, 0xf, color_offset, DrvGfxROM1);
						}
					} else {
						if (flipx) {
							Render16x16Tile_Mask_FlipX_Clip(pSpriteDraw, tile, sx + 16*x, (sy + 16*y) - 32, color, 4, 0xf, color_offset, DrvGfxROM1);
						} else {
							Render16x16Tile_Mask_Clip(pSpriteDraw, tile, sx + 16*x, (sy + 16*y) - 32, color, 4, 0xf, color_offset, DrvGfxROM1);
						}
					}

					++sprites_drawn;

					if (sprites_drawn >= 96)
						break;
				}
			}
		}
		else
		{
			++sprites_drawn;

			if (sprites_drawn >= 96)
				break;
		}

		sprptr += 16;
	}
}

static void draw_copy_sprites()
{
	for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
		if (pSpriteDraw[i] != 0x000f) pTransDraw[i] = pSpriteDraw[i];
	}
}

static INT32 Ninjakd2Draw()
{
	if (DrvRecalc) {
		DrvCalculatePalette();
		DrvRecalc = 0;
	}

	if (overdraw_enable) {
		for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
			if ((pSpriteDraw[i] & 0x00f0) == 0x00f0) pSpriteDraw[i] = 0x000f;
		}
	} else {
		for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
			pSpriteDraw[i] = 0x000f;
		}
	}

	draw_sprites(0x100, 0);

	if (tilemap_enable[0] == 0)
		BurnTransferClear();

	if (tilemap_enable[0])
		draw_bg_layer();

	draw_copy_sprites();

	draw_fg_layer(0x200);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 MnightDraw()
{
	if (DrvRecalc) {
		DrvCalculatePalette();
		DrvRecalc = 0;
	}

	if (overdraw_enable) {
		for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
			if ((pSpriteDraw[i] & 0x00f0) == 0x00f0) pSpriteDraw[i] = 0x000f;
		}
	} else {
		for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
			pSpriteDraw[i] = 0x000f;
		}
	}

	draw_sprites(0x100, 0);

	if (tilemap_enable[0] == 0)
		BurnTransferClear();

	if (tilemap_enable[0])
		draw_mnight_bg_layer();

	draw_copy_sprites();

	draw_fg_layer(0x200);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 RobokidDraw()
{
	if (DrvRecalc) {
		DrvCalculatePalette();
		DrvRecalc = 0;
	}

	if (overdraw_enable) {
		for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
			if ((pSpriteDraw[i] & 0x00f0) < 0x00e0) pSpriteDraw[i] = 0x000f;
		}
	} else {
		for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
			pSpriteDraw[i] = 0x000f;
		}
	}

	draw_sprites(0x200, 1);

	if (tilemap_enable[0] == 0)
		BurnTransferClear();

	draw_robokid_bg_layer(0, DrvBgRAM0, DrvGfxROM2, 0, 0);

	draw_robokid_bg_layer(1, DrvBgRAM1, DrvGfxROM3, 0, 1);

	draw_copy_sprites();

	draw_robokid_bg_layer(2, DrvBgRAM2, DrvGfxROM4, 0, 1);

	draw_fg_layer(0x300);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 OmegafDraw()
{
	if (DrvRecalc) {
		DrvCalculatePalette();
		DrvRecalc = 0;
	}

	if (overdraw_enable) {
		for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
			pSpriteDraw[i] = 0x000f; // no enable??
		}
	} else {
		for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
			pSpriteDraw[i] = 0x000f;
		}
	}

	draw_sprites(0x200, 1);

	BurnTransferClear();

	draw_robokid_bg_layer(0, DrvBgRAM0, DrvGfxROM2, 1, 1);
	draw_robokid_bg_layer(1, DrvBgRAM1, DrvGfxROM3, 1, 1);
	draw_robokid_bg_layer(2, DrvBgRAM2, DrvGfxROM4, 1, 1);

	draw_copy_sprites();

	draw_fg_layer(0x300);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		previous_coin[0] = (DrvInputs[0] & 0x40) ? 0 : (previous_coin[0] + 1);
		previous_coin[1] = (DrvInputs[0] & 0x80) ? 0 : (previous_coin[1] + 1);
		if (previous_coin[0] >= 4) DrvInputs[0] |= 0x40;
		if (previous_coin[1] >= 4) DrvInputs[0] |= 0x80;
	}

	ZetNewFrame();

	INT32 nCycleSegment;
	INT32 nInterleave = 10;
	INT32 nCyclesTotal[2] = { 6000000 / 60, 5000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCycleSegment = nCyclesTotal[0] / nInterleave;

		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nCycleSegment);

		if (i == (nInterleave-1))
		{
			ZetSetVector(0xd7);
			ZetSetIRQLine(0, CPU_IRQSTATUS_AUTO);
		}

		ZetClose();

		nCycleSegment = nCyclesTotal[1] / nInterleave;

		ZetOpen(1);
	//	nCyclesDone[1] += ZetRun(nCycleSegment);
		BurnTimerUpdate((i + 1) * nCycleSegment);
		ZetClose();
	}

	ZetOpen(1);

	BurnTimerEndFrame(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
		ninjakd2_sample_player(pBurnSoundOut, nBurnSoundLen);
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

		BurnYM2203Scan(nAction, pnMin);

		for (INT32 i = 0; i < 3; i++) {
			SCAN_VAR(scrollx[i]);
			SCAN_VAR(scrolly[i]);
			SCAN_VAR(tilemap_enable[i]);
			SCAN_VAR(m_omegaf_io_protection[i]);
			SCAN_VAR(nZ80RamBank[i]);
		}

		SCAN_VAR(nZ80RomBank);

		SCAN_VAR(overdraw_enable);
		SCAN_VAR(m_omegaf_io_protection_input);
		SCAN_VAR(m_omegaf_io_protection_tic);

		SCAN_VAR(ninjakd2_sample_offset);
	}

	if (nAction & ACB_WRITE) {
		DrvRecalc = 1;

		ZetOpen(0);
		ninjakd2_bankswitch(nZ80RomBank);
		ZetClose();

	}

	return 0;
}

static INT32 RobokidScan(INT32 nAction, INT32 *pnMin)
{
	INT32 nRet = DrvScan(nAction, pnMin);

	if (nRet == 0)
	{
		ZetOpen(0);
		robokid_rambank(0, nZ80RamBank[0]);
		robokid_rambank(1, nZ80RamBank[1]);
		robokid_rambank(2, nZ80RamBank[2]);
		ZetClose();
	}

	return nRet;
}

static INT32 OmegafScan(INT32 nAction, INT32 *pnMin)
{
	INT32 nRet = DrvScan(nAction, pnMin);

	if (nRet == 0)
	{
		ZetOpen(0);
		robokid_rambank(4|0, nZ80RamBank[0]);
		robokid_rambank(4|1, nZ80RamBank[1]);
		robokid_rambank(4|2, nZ80RamBank[2]);
		ZetClose();
	}

	return nRet;
}



// Ninja-Kid II / NinjaKun Ashura no Shou (set 1)

static struct BurnRomInfo ninjakd2RomDesc[] = {
	{ "nk2_01.rom",		0x08000, 0x3cdbb906, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "nk2_02.rom",		0x08000, 0xb5ce9a1a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "nk2_03.rom",		0x08000, 0xad275654, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "nk2_04.rom",		0x08000, 0xe7692a77, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "nk2_05.rom",		0x08000, 0x5dac9426, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "nk2_06.rom",		0x10000, 0xd3a18a79, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 #1 Code (mc8123 encrypted)

	{ "nk2_12.rom",		0x08000, 0xdb5657a9, 3 | BRF_GRA },           //  6 Foreground Tiles

	{ "nk2_08.rom",		0x10000, 0x1b79c50a, 4 | BRF_GRA },           //  7 Sprite Tiles
	{ "nk2_07.rom",		0x10000, 0x0be5cd13, 4 | BRF_GRA },           //  8

	{ "nk2_11.rom",		0x10000, 0x41a714b3, 5 | BRF_GRA },           //  9 Background Tiles
	{ "nk2_10.rom",		0x10000, 0xc913c4ab, 5 | BRF_GRA },           // 10

	{ "nk2_09.rom",		0x10000, 0xc1d2d170, 6 | BRF_GRA },           // 11 Samples (8 bit unsigned)

	{ "ninjakd2.key",	0x02000, 0xec25318f, 7 | BRF_PRG | BRF_ESS }, // 12 mc8123 key
};

STD_ROM_PICK(ninjakd2)
STD_ROM_FN(ninjakd2)

struct BurnDriver BurnDrvNinjakd2 = {
	"ninjakd2", NULL, NULL, NULL, "1987",
	"Ninja-Kid II / NinjaKun Ashura no Shou (set 1)\0", NULL, "UPL", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, ninjakd2RomInfo, ninjakd2RomName, NULL, NULL, DrvInputInfo, Ninjakd2DIPInfo,
	Ninjakd2Init, DrvExit, DrvFrame, Ninjakd2Draw, DrvScan, &DrvRecalc, 0x300,
	256, 192, 4, 3
};


// Ninja-Kid II / NinjaKun Ashura no Shou (set 2, bootleg?)

static struct BurnRomInfo ninjakd2aRomDesc[] = {
	{ "nk2_01.bin",		0x08000, 0xe6adca65, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "nk2_02.bin",		0x08000, 0xd9284bd1, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "nk2_03.rom",		0x08000, 0xad275654, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "nk2_04.rom",		0x08000, 0xe7692a77, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "nk2_05.bin",		0x08000, 0x960725fb, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "nk2_06.bin",		0x10000, 0x7bfe6c9e, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 #1 Code

	{ "nk2_12.rom",		0x08000, 0xdb5657a9, 3 | BRF_GRA },           //  6 Foreground Tiles

	{ "nk2_08.rom",		0x10000, 0x1b79c50a, 4 | BRF_GRA },           //  7 Sprite Tiles
	{ "nk2_07.rom",		0x10000, 0x0be5cd13, 4 | BRF_GRA },           //  8

	{ "nk2_11.rom",		0x10000, 0x41a714b3, 5 | BRF_GRA },           //  9 Background Tiles
	{ "nk2_10.rom",		0x10000, 0xc913c4ab, 5 | BRF_GRA },           // 10

	{ "nk2_09.rom",		0x10000, 0xc1d2d170, 6 | BRF_GRA },           // 11 Samples (8 bit unsigned)
};

STD_ROM_PICK(ninjakd2a)
STD_ROM_FN(ninjakd2a)

struct BurnDriver BurnDrvNinjakd2a = {
	"ninjakd2a", "ninjakd2", NULL, NULL, "1987",
	"Ninja-Kid II / NinjaKun Ashura no Shou (set 2, bootleg?)\0", NULL, "UPL", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, ninjakd2aRomInfo, ninjakd2aRomName, NULL, NULL, DrvInputInfo, Ninjakd2DIPInfo,
	Ninjakd2DecryptedInit, DrvExit, DrvFrame, Ninjakd2Draw, DrvScan, &DrvRecalc, 0x300,
	256, 192, 4, 3
};


// Ninja-Kid II / NinjaKun Ashura no Shou (set 3, bootleg?)

static struct BurnRomInfo ninjakd2bRomDesc[] = {
	{ "1.3s",			0x08000, 0xcb4f4624, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "2.3q",			0x08000, 0x0ad0c100, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "nk2_03.rom",		0x08000, 0xad275654, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "nk2_04.rom",		0x08000, 0xe7692a77, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "nk2_05.rom",		0x08000, 0x5dac9426, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "nk2_06.bin",		0x10000, 0x7bfe6c9e, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 #1 Code

	{ "nk2_12.rom",		0x08000, 0xdb5657a9, 3 | BRF_GRA },           //  6 Foreground Tiles

	{ "nk2_08.rom",		0x10000, 0x1b79c50a, 4 | BRF_GRA },           //  7 Sprite Tiles
	{ "nk2_07.rom",		0x10000, 0x0be5cd13, 4 | BRF_GRA },           //  8

	{ "nk2_11.rom",		0x10000, 0x41a714b3, 5 | BRF_GRA },           //  9 Background Tiles
	{ "nk2_10.rom",		0x10000, 0xc913c4ab, 5 | BRF_GRA },           // 10

	{ "nk2_09.rom",		0x10000, 0xc1d2d170, 6 | BRF_GRA },           // 11 Samples (8 bit unsigned)
};

STD_ROM_PICK(ninjakd2b)
STD_ROM_FN(ninjakd2b)

struct BurnDriver BurnDrvNinjakd2b = {
	"ninjakd2b", "ninjakd2", NULL, NULL, "1987",
	"Ninja-Kid II / NinjaKun Ashura no Shou (set 3, bootleg?)\0", NULL, "UPL", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, ninjakd2bRomInfo, ninjakd2bRomName, NULL, NULL, DrvInputInfo, RdactionDIPInfo,
	Ninjakd2DecryptedInit, DrvExit, DrvFrame, Ninjakd2Draw, DrvScan, &DrvRecalc, 0x300,
	256, 192, 4, 3
};


// Ninja-Kid II / NinjaKun Ashura no Shou (set 4)
// close to set 3

static struct BurnRomInfo ninjakd2cRomDesc[] = {
	{ "1.3U",			0x08000, 0x06096412, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "2.3T",			0x08000, 0x9ed9a994, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "nk2_03.rom",		0x08000, 0xad275654, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "nk2_04.rom",		0x08000, 0xe7692a77, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "5.3M",			0x08000, 0x800d4951, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "nk2_06.rom",		0x10000, 0xd3a18a79, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 #1 Code (mc8123 encrypted)

	{ "nk2_12.rom",		0x08000, 0xdb5657a9, 3 | BRF_GRA },           //  6 Foreground Tiles

	{ "nk2_08.rom",		0x10000, 0x1b79c50a, 4 | BRF_GRA },           //  7 Sprite Tiles
	{ "nk2_07.rom",		0x10000, 0x0be5cd13, 4 | BRF_GRA },           //  8

	{ "nk2_11.rom",		0x10000, 0x41a714b3, 5 | BRF_GRA },           //  9 Background Tiles
	{ "nk2_10.rom",		0x10000, 0xc913c4ab, 5 | BRF_GRA },           // 10

	{ "nk2_09.rom",		0x10000, 0xc1d2d170, 6 | BRF_GRA },           // 11 Samples (8 bit unsigned)
	
	{ "ninjakd2.key",	0x02000, 0xec25318f, 7 | BRF_PRG | BRF_ESS }, // 12 mc8123 key
};

STD_ROM_PICK(ninjakd2c)
STD_ROM_FN(ninjakd2c)

struct BurnDriver BurnDrvNinjakd2c = {
	"ninjakd2c", "ninjakd2", NULL, NULL, "1987",
	"Ninja-Kid II / NinjaKun Ashura no Shou (set 4)\0", NULL, "UPL", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, ninjakd2cRomInfo, ninjakd2cRomName, NULL, NULL, DrvInputInfo, RdactionDIPInfo,
	Ninjakd2Init, DrvExit, DrvFrame, Ninjakd2Draw, DrvScan, &DrvRecalc, 0x300,
	256, 192, 4, 3
};


// Rad Action / NinjaKun Ashura no Shou

static struct BurnRomInfo rdactionRomDesc[] = {
	{ "1.3u",			0x08000, 0x5c475611, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "2.3s",			0x08000, 0xa1e23bd2, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "nk2_03.rom",		0x08000, 0xad275654, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "nk2_04.rom",		0x08000, 0xe7692a77, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "nk2_05.bin",		0x08000, 0x960725fb, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "nk2_06.rom",		0x10000, 0xd3a18a79, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 #1 Code (mc8123 encrypted)

	{ "12.5n",			0x08000, 0x0936b365, 3 | BRF_GRA },           //  6 Foreground Tiles

	{ "nk2_08.rom",		0x10000, 0x1b79c50a, 4 | BRF_GRA },           //  7 Sprite Tiles
	{ "nk2_07.rom",		0x10000, 0x0be5cd13, 4 | BRF_GRA },           //  8

	{ "nk2_11.rom",		0x10000, 0x41a714b3, 5 | BRF_GRA },           //  9 Background Tiles
	{ "nk2_10.rom",		0x10000, 0xc913c4ab, 5 | BRF_GRA },           // 10

	{ "nk2_09.rom",		0x10000, 0xc1d2d170, 6 | BRF_GRA },           // 11 Samples (8 bit unsigned)

	{ "ninjakd2.key",	0x02000, 0xec25318f, 7 | BRF_PRG | BRF_ESS }, // 12 mc8123 key
};

STD_ROM_PICK(rdaction)
STD_ROM_FN(rdaction)

struct BurnDriver BurnDrvRdaction = {
	"rdaction", "ninjakd2", NULL, NULL, "1987",
	"Rad Action / NinjaKun Ashura no Shou\0", NULL, "UPL (World Games license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, rdactionRomInfo, rdactionRomName, NULL, NULL, DrvInputInfo, RdactionDIPInfo,
	Ninjakd2Init, DrvExit, DrvFrame, Ninjakd2Draw, DrvScan, &DrvRecalc, 0x300,
	256, 192, 4, 3
};


// JT-104 (title screen modification of Rad Action)
// identical to rdaction set with different gfx rom and decrypted sound rom

static struct BurnRomInfo jt104RomDesc[] = {
	{ "1.3u",			0x08000, 0x5c475611, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "2.3s",			0x08000, 0xa1e23bd2, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "nk2_03.rom",		0x08000, 0xad275654, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "nk2_04.rom",		0x08000, 0xe7692a77, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "nk2_05.bin",		0x08000, 0x960725fb, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "nk2_06.bin",		0x10000, 0x7bfe6c9e, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 #1 Code

	{ "jt_104_12.bin",	0x08000, 0xc038fadb, 3 | BRF_GRA },           //  6 Foreground Tiles

	{ "nk2_08.rom",		0x10000, 0x1b79c50a, 4 | BRF_GRA },           //  7 Sprite Tiles
	{ "nk2_07.rom",		0x10000, 0x0be5cd13, 4 | BRF_GRA },           //  8

	{ "nk2_11.rom",		0x10000, 0x41a714b3, 5 | BRF_GRA },           //  9 Background Tiles
	{ "nk2_10.rom",		0x10000, 0xc913c4ab, 5 | BRF_GRA },           // 10

	{ "nk2_09.rom",		0x10000, 0xc1d2d170, 6 | BRF_GRA },           // 11 Samples (8 bit unsigned)

	{ "ninjakd2.key",	0x02000, 0xec25318f, 7 | BRF_PRG | BRF_ESS }, // 12 mc8123 key
};

STD_ROM_PICK(jt104)
STD_ROM_FN(jt104)

struct BurnDriver BurnDrvJt104 = {
	"jt104", "ninjakd2", NULL, NULL, "1987",
	"JT-104 (title screen modification of Rad Action)\0", NULL, "UPL (United Amusements license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, jt104RomInfo, jt104RomName, NULL, NULL, DrvInputInfo, RdactionDIPInfo,
	Ninjakd2DecryptedInit, DrvExit, DrvFrame, Ninjakd2Draw, DrvScan, &DrvRecalc, 0x300,
	256, 192, 4, 3
};


// Mutant Night 

static struct BurnRomInfo mnightRomDesc[] = {
	{ "1.j19",			0x08000, 0x56678d14, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "2.j17",			0x08000, 0x2a73f88e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.j16",			0x08000, 0xc5e42bb4, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.j14",			0x08000, 0xdf6a4f7a, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "5.j12",			0x08000, 0x9c391d1b, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "6.j7",			0x10000, 0xa0782a31, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 #1 Code

	{ "13.b10",			0x08000, 0x8c177a19, 3 | BRF_GRA },           //  6 Foreground Tiles

	{ "9.e11",			0x10000, 0x4883059c, 4 | BRF_GRA },           //  7 Sprite Tiles
	{ "8.e12",			0x10000, 0x02b91445, 4 | BRF_GRA },           //  8
	{ "7.e14",			0x10000, 0x9f08d160, 4 | BRF_GRA },           //  9

	{ "12.b20",			0x10000, 0x4d37e0f4, 5 | BRF_GRA },           // 10 Background Tiles
	{ "11.b22",			0x10000, 0xb22cbbd3, 5 | BRF_GRA },           // 11
	{ "10.b23",			0x10000, 0x65714070, 5 | BRF_GRA },           // 12
};

STD_ROM_PICK(mnight)
STD_ROM_FN(mnight)

struct BurnDriver BurnDrvMnight = {
	"mnight", NULL, NULL, NULL, "1987",
	"Mutant Night\0", NULL, "UPL", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, mnightRomInfo, mnightRomName, NULL, NULL, DrvInputInfo, MnightDIPInfo,
	MnightInit, DrvExit, DrvFrame, MnightDraw, DrvScan, &DrvRecalc, 0x300,
	256, 192, 4, 3
};


// Mutant Night (Japan)

static struct BurnRomInfo mnightjRomDesc[] = {
	{ "1.j19",			0x08000, 0x56678d14, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "2.j17",			0x08000, 0x2a73f88e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.j16",			0x08000, 0xc5e42bb4, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.j14",			0x08000, 0xdf6a4f7a, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "5.j12",			0x08000, 0x9c391d1b, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "6.j7",			0x10000, 0xa0782a31, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 #1 Code

	{ "13.b10",			0x08000, 0x37b8221f, 3 | BRF_GRA },           //  6 Foreground Tiles

	{ "9.e11",			0x10000, 0x4883059c, 4 | BRF_GRA },           //  7 Sprite Tiles
	{ "8.e12",			0x10000, 0x02b91445, 4 | BRF_GRA },           //  8
	{ "7.e14",			0x10000, 0x9f08d160, 4 | BRF_GRA },           //  9

	{ "12.b20",			0x10000, 0x4d37e0f4, 5 | BRF_GRA },           // 10 Background Tiles
	{ "11.b22",			0x10000, 0xb22cbbd3, 5 | BRF_GRA },           // 11
	{ "10.b23",			0x10000, 0x65714070, 5 | BRF_GRA },           // 12
};

STD_ROM_PICK(mnightj)
STD_ROM_FN(mnightj)

struct BurnDriver BurnDrvMnightj = {
	"mnightj", "mnight", NULL, NULL, "1987",
	"Mutant Night (Japan)\0", NULL, "UPL (Kawakus license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, mnightjRomInfo, mnightjRomName, NULL, NULL, DrvInputInfo, MnightDIPInfo,
	MnightInit, DrvExit, DrvFrame, MnightDraw, DrvScan, &DrvRecalc, 0x300,
	256, 192, 4, 3
};


// Ark Area

static struct BurnRomInfo arkareaRomDesc[] = {
	{ "arkarea.008",	0x08000, 0x1ce1b5b9, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "arkarea.009",	0x08000, 0xdb1c81d1, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "arkarea.010",	0x08000, 0x5a460dae, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "arkarea.011",	0x08000, 0x63f022c9, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "arkarea.012",	0x08000, 0x3c4c65d5, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "arkarea.013",	0x08000, 0x2d409d58, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 #1 Code

	{ "arkarea.004",	0x08000, 0x69e36af2, 3 | BRF_GRA },           //  6 Foreground Tiles

	{ "arkarea.007",	0x10000, 0xd5684a27, 4 | BRF_GRA },           //  7 Sprite Tiles
	{ "arkarea.006",	0x10000, 0x2c0567d6, 4 | BRF_GRA },           //  8
	{ "arkarea.005",	0x10000, 0x9886004d, 4 | BRF_GRA },           //  9

	{ "arkarea.003",	0x10000, 0x6f45a308, 5 | BRF_GRA },           // 10 Background Tiles
	{ "arkarea.002",	0x10000, 0x051d3482, 5 | BRF_GRA },           // 11
	{ "arkarea.001",	0x10000, 0x09d11ab7, 5 | BRF_GRA },           // 12
};

STD_ROM_PICK(arkarea)
STD_ROM_FN(arkarea)

struct BurnDriver BurnDrvArkarea = {
	"arkarea", NULL, NULL, NULL, "1988",
	"Ark Area\0", NULL, "UPL", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, arkareaRomInfo, arkareaRomName, NULL, NULL, Drv2InputInfo, ArkareaDIPInfo,
	MnightInit, DrvExit, DrvFrame, MnightDraw, DrvScan, &DrvRecalc, 0x300,
	256, 192, 4, 3
};


// Atomic Robo-kid (World, Type-2)

static struct BurnRomInfo robokidRomDesc[] = {
	{ "robokid1.18j",	0x10000, 0x378c21fc, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "robokid2.18k",	0x10000, 0xddef8c5a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "robokid3.15k",	0x10000, 0x05295ec3, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "robokid4.12k",	0x10000, 0x3bc3977f, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "robokid.k7",		0x10000, 0xf490a2e9, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code

	{ "robokid.b9",		0x08000, 0xfac59c3f, 3 | BRF_GRA },           //  5 Foreground Tiles

	{ "robokid.15f",	0x10000, 0xba61f5ab, 4 | BRF_GRA },           //  6 Sprite Tiles
	{ "robokid.16f",	0x10000, 0xd9b399ce, 4 | BRF_GRA },           //  7
	{ "robokid.17f",	0x10000, 0xafe432b9, 4 | BRF_GRA },           //  8
	{ "robokid.18f",	0x10000, 0xa0aa2a84, 4 | BRF_GRA },           //  9

	{ "robokid.19c",	0x10000, 0x02220421, 5 | BRF_GRA },           // 10 Background Layer 0 Tiles
	{ "robokid.20c",	0x10000, 0x02d59bc2, 5 | BRF_GRA },           // 11
	{ "robokid.17d",	0x10000, 0x2fa29b99, 5 | BRF_GRA },           // 12
	{ "robokid.18d",	0x10000, 0xae15ce02, 5 | BRF_GRA },           // 13
	{ "robokid.19d",	0x10000, 0x784b089e, 5 | BRF_GRA },           // 14
	{ "robokid.20d",	0x10000, 0xb0b395ed, 5 | BRF_GRA },           // 15
	{ "robokid.19f",	0x10000, 0x0f9071c6, 5 | BRF_GRA },           // 16

	{ "robokid.12c",	0x10000, 0x0ab45f94, 6 | BRF_GRA },           // 17 Background Layer 1 Tiles
	{ "robokid.14c",	0x10000, 0x029bbd4a, 6 | BRF_GRA },           // 18
	{ "robokid.15c",	0x10000, 0x7de67ebb, 6 | BRF_GRA },           // 19
	{ "robokid.16c",	0x10000, 0x53c0e582, 6 | BRF_GRA },           // 20
	{ "robokid.17c",	0x10000, 0x0cae5a1e, 6 | BRF_GRA },           // 21
	{ "robokid.18c",	0x10000, 0x56ac7c8a, 6 | BRF_GRA },           // 22
	{ "robokid.15d",	0x10000, 0xcd632a4d, 6 | BRF_GRA },           // 23
	{ "robokid.16d",	0x10000, 0x18d92b2b, 6 | BRF_GRA },           // 24

	{ "robokid.12a",	0x10000, 0xe64d1c10, 7 | BRF_GRA },           // 25 Background Layer 2 Tiles
	{ "robokid.14a",	0x10000, 0x8f9371e4, 7 | BRF_GRA },           // 26
	{ "robokid.15a",	0x10000, 0x469204e7, 7 | BRF_GRA },           // 27
	{ "robokid.16a",	0x10000, 0x4e340815, 7 | BRF_GRA },           // 28
	{ "robokid.17a",	0x10000, 0xf0863106, 7 | BRF_GRA },           // 29
	{ "robokid.18a",	0x10000, 0xfdff7441, 7 | BRF_GRA },           // 30
	
	{ "prom82s129.cpu",	0x00100, 0x4dd96f67, 0 | BRF_OPT },
};

STD_ROM_PICK(robokid)
STD_ROM_FN(robokid)

struct BurnDriver BurnDrvRobokid = {
	"robokid", NULL, NULL, NULL, "1988",
	"Atomic Robo-kid (World, Type-2)\0", NULL, "UPL", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, robokidRomInfo, robokidRomName, NULL, NULL, DrvInputInfo, RobokidDIPInfo,
	RobokidInit, DrvExit, DrvFrame, RobokidDraw, RobokidScan, &DrvRecalc, 0x400,
	256, 192, 4, 3
};


// Atomic Robo-kid (Japan, Type-2, set 1)

static struct BurnRomInfo robokidjRomDesc[] = {
	{ "1.29",			0x10000, 0x59a1e2ec, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "2.30",			0x10000, 0xe3f73476, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "robokid3.15k",	0x10000, 0x05295ec3, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "robokid4.12k",	0x10000, 0x3bc3977f, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "robokid.k7",		0x10000, 0xf490a2e9, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code

	{ "robokid.b9",		0x08000, 0xfac59c3f, 3 | BRF_GRA },           //  5 Foreground Tiles

	{ "robokid.15f",	0x10000, 0xba61f5ab, 4 | BRF_GRA },           //  6 Sprite Tiles
	{ "robokid.16f",	0x10000, 0xd9b399ce, 4 | BRF_GRA },           //  7
	{ "robokid.17f",	0x10000, 0xafe432b9, 4 | BRF_GRA },           //  8
	{ "robokid.18f",	0x10000, 0xa0aa2a84, 4 | BRF_GRA },           //  9

	{ "robokid.19c",	0x10000, 0x02220421, 5 | BRF_GRA },           // 10 Background Layer 0 Tiles
	{ "robokid.20c",	0x10000, 0x02d59bc2, 5 | BRF_GRA },           // 11
	{ "robokid.17d",	0x10000, 0x2fa29b99, 5 | BRF_GRA },           // 12
	{ "robokid.18d",	0x10000, 0xae15ce02, 5 | BRF_GRA },           // 13
	{ "robokid.19d",	0x10000, 0x784b089e, 5 | BRF_GRA },           // 14
	{ "robokid.20d",	0x10000, 0xb0b395ed, 5 | BRF_GRA },           // 15
	{ "robokid.19f",	0x10000, 0x0f9071c6, 5 | BRF_GRA },           // 16

	{ "robokid.12c",	0x10000, 0x0ab45f94, 6 | BRF_GRA },           // 17 Background Layer 1 Tiles
	{ "robokid.14c",	0x10000, 0x029bbd4a, 6 | BRF_GRA },           // 18
	{ "robokid.15c",	0x10000, 0x7de67ebb, 6 | BRF_GRA },           // 19
	{ "robokid.16c",	0x10000, 0x53c0e582, 6 | BRF_GRA },           // 20
	{ "robokid.17c",	0x10000, 0x0cae5a1e, 6 | BRF_GRA },           // 21
	{ "robokid.18c",	0x10000, 0x56ac7c8a, 6 | BRF_GRA },           // 22
	{ "robokid.15d",	0x10000, 0xcd632a4d, 6 | BRF_GRA },           // 23
	{ "robokid.16d",	0x10000, 0x18d92b2b, 6 | BRF_GRA },           // 24

	{ "robokid.12a",	0x10000, 0xe64d1c10, 7 | BRF_GRA },           // 25 Background Layer 2 Tiles
	{ "robokid.14a",	0x10000, 0x8f9371e4, 7 | BRF_GRA },           // 26
	{ "robokid.15a",	0x10000, 0x469204e7, 7 | BRF_GRA },           // 27
	{ "robokid.16a",	0x10000, 0x4e340815, 7 | BRF_GRA },           // 28
	{ "robokid.17a",	0x10000, 0xf0863106, 7 | BRF_GRA },           // 29
	{ "robokid.18a",	0x10000, 0xfdff7441, 7 | BRF_GRA },           // 30
	
	{ "prom82s129.cpu",	0x00100, 0x4dd96f67, 0 | BRF_OPT },
};

STD_ROM_PICK(robokidj)
STD_ROM_FN(robokidj)

struct BurnDriver BurnDrvRobokidj = {
	"robokidj", "robokid", NULL, NULL, "1988",
	"Atomic Robo-kid (Japan, Type-2, set 1)\0", NULL, "UPL", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, robokidjRomInfo, robokidjRomName, NULL, NULL, DrvInputInfo, RobokidjDIPInfo,
	RobokidInit, DrvExit, DrvFrame, RobokidDraw, RobokidScan, &DrvRecalc, 0x400,
	256, 192, 4, 3
};


// Atomic Robo-kid (Japan, Type-2, set 2)

static struct BurnRomInfo robokidj2RomDesc[] = {
	{ "1_rom29.18j",	0x10000, 0x969fb951, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "2_rom30.18k",	0x10000, 0xc0228b63, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "robokid3.15k",	0x10000, 0x05295ec3, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "robokid4.12k",	0x10000, 0x3bc3977f, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "robokid.k7",		0x10000, 0xf490a2e9, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code

	{ "robokid.b9",		0x08000, 0xfac59c3f, 3 | BRF_GRA },           //  5 Foreground Tiles

	{ "robokid.15f",	0x10000, 0xba61f5ab, 4 | BRF_GRA },           //  6 Sprite Tiles
	{ "robokid.16f",	0x10000, 0xd9b399ce, 4 | BRF_GRA },           //  7
	{ "robokid.17f",	0x10000, 0xafe432b9, 4 | BRF_GRA },           //  8
	{ "robokid.18f",	0x10000, 0xa0aa2a84, 4 | BRF_GRA },           //  9

	{ "robokid.19c",	0x10000, 0x02220421, 5 | BRF_GRA },           // 10 Background Layer 0 Tiles
	{ "robokid.20c",	0x10000, 0x02d59bc2, 5 | BRF_GRA },           // 11
	{ "robokid.17d",	0x10000, 0x2fa29b99, 5 | BRF_GRA },           // 12
	{ "robokid.18d",	0x10000, 0xae15ce02, 5 | BRF_GRA },           // 13
	{ "robokid.19d",	0x10000, 0x784b089e, 5 | BRF_GRA },           // 14
	{ "robokid.20d",	0x10000, 0xb0b395ed, 5 | BRF_GRA },           // 15
	{ "robokid.19f",	0x10000, 0x0f9071c6, 5 | BRF_GRA },           // 16

	{ "robokid.12c",	0x10000, 0x0ab45f94, 6 | BRF_GRA },           // 17 Background Layer 1 Tiles
	{ "robokid.14c",	0x10000, 0x029bbd4a, 6 | BRF_GRA },           // 18
	{ "robokid.15c",	0x10000, 0x7de67ebb, 6 | BRF_GRA },           // 19
	{ "robokid.16c",	0x10000, 0x53c0e582, 6 | BRF_GRA },           // 20
	{ "robokid.17c",	0x10000, 0x0cae5a1e, 6 | BRF_GRA },           // 21
	{ "robokid.18c",	0x10000, 0x56ac7c8a, 6 | BRF_GRA },           // 22
	{ "robokid.15d",	0x10000, 0xcd632a4d, 6 | BRF_GRA },           // 23
	{ "robokid.16d",	0x10000, 0x18d92b2b, 6 | BRF_GRA },           // 24

	{ "robokid.12a",	0x10000, 0xe64d1c10, 7 | BRF_GRA },           // 25 Background Layer 2 Tiles
	{ "robokid.14a",	0x10000, 0x8f9371e4, 7 | BRF_GRA },           // 26
	{ "robokid.15a",	0x10000, 0x469204e7, 7 | BRF_GRA },           // 27
	{ "robokid.16a",	0x10000, 0x4e340815, 7 | BRF_GRA },           // 28
	{ "robokid.17a",	0x10000, 0xf0863106, 7 | BRF_GRA },           // 29
	{ "robokid.18a",	0x10000, 0xfdff7441, 7 | BRF_GRA },           // 30
	
	{ "prom82s129.cpu",	0x00100, 0x4dd96f67, 0 | BRF_OPT },
};

STD_ROM_PICK(robokidj2)
STD_ROM_FN(robokidj2)

struct BurnDriver BurnDrvRobokidj2 = {
	"robokidj2", "robokid", NULL, NULL, "1988",
	"Atomic Robo-kid (Japan, Type-2, set 2)\0", NULL, "UPL", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, robokidj2RomInfo, robokidj2RomName, NULL, NULL, DrvInputInfo, RobokidjDIPInfo,
	RobokidInit, DrvExit, DrvFrame, RobokidDraw, RobokidScan, &DrvRecalc, 0x400,
	256, 192, 4, 3
};


// Atomic Robo-kid (Japan)

static struct BurnRomInfo robokidj3RomDesc[] = {
	{ "robokid1.18j",	0x10000, 0x77a9332a, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "robokid2.18k",	0x10000, 0x715ecee4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "robokid3.15k",	0x10000, 0xce12fa86, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "robokid4.12k",	0x10000, 0x97e86600, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "robokid.k7",		0x10000, 0xf490a2e9, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code

	{ "robokid.b9",		0x08000, 0xfac59c3f, 3 | BRF_GRA },           //  5 Foreground Tiles

	{ "robokid.15f",	0x10000, 0xba61f5ab, 4 | BRF_GRA },           //  6 Sprite Tiles
	{ "robokid.16f",	0x10000, 0xd9b399ce, 4 | BRF_GRA },           //  7
	{ "robokid.17f",	0x10000, 0xafe432b9, 4 | BRF_GRA },           //  8
	{ "robokid.18f",	0x10000, 0xa0aa2a84, 4 | BRF_GRA },           //  9

	{ "robokid.19c",	0x10000, 0x02220421, 5 | BRF_GRA },           // 10 Background Layer 0 Tiles
	{ "robokid.20c",	0x10000, 0x02d59bc2, 5 | BRF_GRA },           // 11
	{ "robokid.17d",	0x10000, 0x2fa29b99, 5 | BRF_GRA },           // 12
	{ "robokid.18d",	0x10000, 0xae15ce02, 5 | BRF_GRA },           // 13
	{ "robokid.19d",	0x10000, 0x784b089e, 5 | BRF_GRA },           // 14
	{ "robokid.20d",	0x10000, 0xb0b395ed, 5 | BRF_GRA },           // 15
	{ "robokid.19f",	0x10000, 0x0f9071c6, 5 | BRF_GRA },           // 16

	{ "robokid.12c",	0x10000, 0x0ab45f94, 6 | BRF_GRA },           // 17 Background Layer 1 Tiles
	{ "robokid.14c",	0x10000, 0x029bbd4a, 6 | BRF_GRA },           // 18
	{ "robokid.15c",	0x10000, 0x7de67ebb, 6 | BRF_GRA },           // 19
	{ "robokid.16c",	0x10000, 0x53c0e582, 6 | BRF_GRA },           // 20
	{ "robokid.17c",	0x10000, 0x0cae5a1e, 6 | BRF_GRA },           // 21
	{ "robokid.18c",	0x10000, 0x56ac7c8a, 6 | BRF_GRA },           // 22
	{ "robokid.15d",	0x10000, 0xcd632a4d, 6 | BRF_GRA },           // 23
	{ "robokid.16d",	0x10000, 0x18d92b2b, 6 | BRF_GRA },           // 24

	{ "robokid.12a",	0x10000, 0xe64d1c10, 7 | BRF_GRA },           // 25 Background Layer 2 Tiles
	{ "robokid.14a",	0x10000, 0x8f9371e4, 7 | BRF_GRA },           // 26
	{ "robokid.15a",	0x10000, 0x469204e7, 7 | BRF_GRA },           // 27
	{ "robokid.16a",	0x10000, 0x4e340815, 7 | BRF_GRA },           // 28
	{ "robokid.17a",	0x10000, 0xf0863106, 7 | BRF_GRA },           // 29
	{ "robokid.18a",	0x10000, 0xfdff7441, 7 | BRF_GRA },           // 30
	
	{ "prom82s129.cpu",	0x00100, 0x4dd96f67, 0 | BRF_OPT },
};

STD_ROM_PICK(robokidj3)
STD_ROM_FN(robokidj3)

struct BurnDriver BurnDrvRobokidj3 = {
	"robokidj3", "robokid", NULL, NULL, "1988",
	"Atomic Robo-kid (Japan)\0", NULL, "UPL", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, robokidj3RomInfo, robokidj3RomName, NULL, NULL, DrvInputInfo, RobokidjDIPInfo,
	RobokidInit, DrvExit, DrvFrame, RobokidDraw, RobokidScan, &DrvRecalc, 0x400,
	256, 192, 4, 3
};


// Omega Fighter

static struct BurnRomInfo omegafRomDesc[] = {
	{ "1.5",		0x20000, 0x57a7fd96, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "6.4l",		0x20000, 0x6277735c, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "7.7m",		0x10000, 0xd40fc8d5, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "4.18h",		0x08000, 0x9e2d8152, 3 | BRF_GRA },           //  3 Foreground Tiles

	{ "8.23m",		0x20000, 0x0bd2a5d1, 4 | BRF_GRA },           //  4 Sprite Tiles

	{ "2back1.27b",		0x80000, 0x21f8a32e, 5 | BRF_GRA },           //  5 Background Layer 0 Tiles

	{ "1back2.15b",		0x80000, 0x6210ddcc, 6 | BRF_GRA },           //  6 Background Layer 1 Tiles

	{ "3back3.5f",		0x80000, 0xc31cae56, 7 | BRF_GRA },           //  7 Background Layer 2 Tiles
};

STD_ROM_PICK(omegaf)
STD_ROM_FN(omegaf)

struct BurnDriver BurnDrvOmegaf = {
	"omegaf", NULL, NULL, NULL, "1989",
	"Omega Fighter\0", NULL, "UPL", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, omegafRomInfo, omegafRomName, NULL, NULL, OmegafInputInfo, OmegafDIPInfo,
	OmegafInit, DrvExit, DrvFrame, OmegafDraw, OmegafScan, &DrvRecalc, 0x400,
	192, 256, 3, 4
};


// Omega Fighter Special

static struct BurnRomInfo omegafsRomDesc[] = {
	{ "5.3l",		0x20000, 0x503a3e63, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "6.4l",		0x20000, 0x6277735c, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "7.7m",		0x10000, 0xd40fc8d5, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "4.18h",		0x08000, 0x9e2d8152, 3 | BRF_GRA },           //  3 Foreground Tiles

	{ "8.23m",		0x20000, 0x0bd2a5d1, 4 | BRF_GRA },           //  4 Sprite Tiles

	{ "2back1.27b",		0x80000, 0x21f8a32e, 5 | BRF_GRA },           //  5 Background Layer 0 Tiles

	{ "1back2.15b",		0x80000, 0x6210ddcc, 6 | BRF_GRA },           //  6 Background Layer 1 Tiles

	{ "3back3.5f",		0x80000, 0xc31cae56, 7 | BRF_GRA },           //  7 Background Layer 2 Tiles
};

STD_ROM_PICK(omegafs)
STD_ROM_FN(omegafs)

struct BurnDriver BurnDrvOmegafs = {
	"omegafs", "omegaf", NULL, NULL, "1989",
	"Omega Fighter Special\0", NULL, "UPL", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, omegafsRomInfo, omegafsRomName, NULL, NULL, OmegafInputInfo, OmegafDIPInfo,
	OmegafInit, DrvExit, DrvFrame, OmegafDraw, OmegafScan, &DrvRecalc, 0x400,
	192, 256, 3, 4
};
