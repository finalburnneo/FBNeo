// FB Alpha Twin Cobra driver module
// Based on MAME driver by Quench

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "tms32010.h"
#include "z80_intf.h"
#include "burn_ym3812.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvMCUROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvSprRAM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvMCURAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvShareRAM;
static UINT8 *DrvBgRAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvTxRAM;
static UINT8 *DrvSprBuf;

static UINT16 *pTempDraw;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvDips[3];
static UINT8 DrvReset;
static UINT8 DrvInputs[4];

static UINT16 scrollx[4];
static UINT16 scrolly[4];
static UINT16 vidramoffs[4];
static INT32 vblank;

static UINT16 golfwar = 0;

static INT32 m68k_halt = 0;
static INT32 irq_enable;
static INT32 flipscreen;
static INT32 bgrambank;
static INT32 fgrombank;
static INT32 displayenable;
static INT32 fsharkbt_8741;

static UINT32 main_ram_seg;
static UINT16 dsp_addr_w;
static INT32 dsp_execute;
static INT32 dsp_BIO;
static INT32 dsp_on;

static struct BurnInputInfo TwincobrInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 0,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy3 + 1,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Twincobr)

static struct BurnInputInfo FsharkInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy4 + 3,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy4 + 5,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy4 + 4,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy4 + 6,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy4 + 0,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy4 + 1,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Fshark)

static struct BurnDIPInfo TwincobrDIPList[]=
{
	{0x13, 0xff, 0xff, 0x00, NULL			},
	{0x14, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x02, 0x00, "Off"			},
	{0x13, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x13, 0x01, 0x04, 0x00, "Off"			},
	{0x13, 0x01, 0x04, 0x04, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x08, 0x08, "Off"			},
	{0x13, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x13, 0x01, 0x30, 0x30, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x20, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x00, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x13, 0x01, 0xc0, 0x00, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xc0, 0x40, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0xc0, 0x80, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0xc0, 0xc0, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x03, 0x01, "Easy"			},
	{0x14, 0x01, 0x03, 0x00, "Normal"		},
	{0x14, 0x01, 0x03, 0x02, "Hard"			},
	{0x14, 0x01, 0x03, 0x03, "Very Hard"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x14, 0x01, 0x0c, 0x00, "50k 200k 150k+"	},
	{0x14, 0x01, 0x0c, 0x04, "70k 270k 200k+"	},
	{0x14, 0x01, 0x0c, 0x08, "50k Only"		},
	{0x14, 0x01, 0x0c, 0x0c, "100k Only"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0x30, 0x30, "2"			},
	{0x14, 0x01, 0x30, 0x00, "3"			},
	{0x14, 0x01, 0x30, 0x20, "4"			},
	{0x14, 0x01, 0x30, 0x10, "5"			},

	{0   , 0xfe, 0   ,    2, "Dip Switch Display"	},
	{0x14, 0x01, 0x40, 0x00, "Off"			},
	{0x14, 0x01, 0x40, 0x40, "On"			},
};

STDDIPINFO(Twincobr)

static struct BurnDIPInfo TwincobruDIPList[]=
{
	{0x13, 0xff, 0xff, 0x00, NULL			},
	{0x14, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x02, 0x00, "Off"			},
	{0x13, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x13, 0x01, 0x04, 0x00, "Off"			},
	{0x13, 0x01, 0x04, 0x04, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x08, 0x08, "Off"			},
	{0x13, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x13, 0x01, 0x30, 0x20, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x00, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x30, 0x30, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0x30, 0x10, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x13, 0x01, 0xc0, 0x80, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0xc0, 0x00, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xc0, 0xc0, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0xc0, 0x40, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x03, 0x01, "Easy"			},
	{0x14, 0x01, 0x03, 0x00, "Normal"		},
	{0x14, 0x01, 0x03, 0x02, "Hard"			},
	{0x14, 0x01, 0x03, 0x03, "Very Hard"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x14, 0x01, 0x0c, 0x00, "50k 200k 150k+"	},
	{0x14, 0x01, 0x0c, 0x04, "70k 270k 200k+"	},
	{0x14, 0x01, 0x0c, 0x08, "50k Only"		},
	{0x14, 0x01, 0x0c, 0x0c, "100k Only"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0x30, 0x30, "2"			},
	{0x14, 0x01, 0x30, 0x00, "3"			},
	{0x14, 0x01, 0x30, 0x20, "4"			},
	{0x14, 0x01, 0x30, 0x10, "5"			},

	{0   , 0xfe, 0   ,    2, "Dip Switch Display"	},
	{0x14, 0x01, 0x40, 0x00, "Off"			},
	{0x14, 0x01, 0x40, 0x40, "On"			},
};

STDDIPINFO(Twincobru)


static struct BurnDIPInfo KtigerDIPList[]=
{
	{0x13, 0xff, 0xff, 0x01, NULL			},
	{0x14, 0xff, 0xff, 0x80, NULL			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x13, 0x01, 0x01, 0x01, "Upright"		},
	{0x13, 0x01, 0x01, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x02, 0x00, "Off"			},
	{0x13, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x13, 0x01, 0x04, 0x00, "Off"			},
	{0x13, 0x01, 0x04, 0x04, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x08, 0x08, "Off"			},
	{0x13, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x13, 0x01, 0x30, 0x20, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x00, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x30, 0x30, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0x30, 0x10, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x13, 0x01, 0xc0, 0x80, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0xc0, 0x00, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xc0, 0xc0, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0xc0, 0x40, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x03, 0x01, "Easy"			},
	{0x14, 0x01, 0x03, 0x00, "Normal"		},
	{0x14, 0x01, 0x03, 0x02, "Hard"			},
	{0x14, 0x01, 0x03, 0x03, "Very Hard"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x14, 0x01, 0x0c, 0x04, "50k 200k 150k+"	},
	{0x14, 0x01, 0x0c, 0x00, "70k 270k 200k+"	},
	{0x14, 0x01, 0x0c, 0x08, "100k Only"		},
	{0x14, 0x01, 0x0c, 0x0c, "None"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0x30, 0x30, "2"			},
	{0x14, 0x01, 0x30, 0x00, "3"			},
	{0x14, 0x01, 0x30, 0x20, "4"			},
	{0x14, 0x01, 0x30, 0x10, "5"			},

	{0   , 0xfe, 0   ,    2, "Dip Switch Display"	},
	{0x14, 0x01, 0x40, 0x00, "Off"			},
	{0x14, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x14, 0x01, 0x80, 0x00, "No"			},
	{0x14, 0x01, 0x80, 0x80, "Yes"			},
};

STDDIPINFO(Ktiger)


static struct BurnDIPInfo FsharkDIPList[]=
{
	{0x13, 0xff, 0xff, 0x01, NULL			},
	{0x14, 0xff, 0xff, 0x80, NULL			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x13, 0x01, 0x01, 0x01, "Upright"		},
	{0x13, 0x01, 0x01, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x02, 0x00, "Off"			},
	{0x13, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x13, 0x01, 0x04, 0x00, "Off"			},
	{0x13, 0x01, 0x04, 0x04, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x08, 0x08, "Off"			},
	{0x13, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x13, 0x01, 0x30, 0x30, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x20, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x00, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x13, 0x01, 0xc0, 0x00, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xc0, 0x40, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0xc0, 0x80, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0xc0, 0xc0, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x03, 0x01, "Easy"			},
	{0x14, 0x01, 0x03, 0x00, "Normal"		},
	{0x14, 0x01, 0x03, 0x02, "Hard"			},
	{0x14, 0x01, 0x03, 0x03, "Very Hard"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x14, 0x01, 0x0c, 0x00, "50k 200k 150k+"	},
	{0x14, 0x01, 0x0c, 0x04, "70k 270k 200k+"	},
	{0x14, 0x01, 0x0c, 0x08, "50k Only"		},
	{0x14, 0x01, 0x0c, 0x0c, "100k Only"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0x30, 0x20, "1"			},
	{0x14, 0x01, 0x30, 0x30, "2"			},
	{0x14, 0x01, 0x30, 0x00, "3"			},
	{0x14, 0x01, 0x30, 0x10, "5"			},

	{0   , 0xfe, 0   ,    2, "Dip Switch Display"	},
	{0x14, 0x01, 0x40, 0x00, "Off"			},
	{0x14, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x14, 0x01, 0x80, 0x00, "No"			},
	{0x14, 0x01, 0x80, 0x80, "Yes"			},
};

STDDIPINFO(Fshark)

static struct BurnDIPInfo SkysharkDIPList[]=
{
	{0x13, 0xff, 0xff, 0x01, NULL			},
	{0x14, 0xff, 0xff, 0x80, NULL			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x13, 0x01, 0x01, 0x01, "Upright"		},
	{0x13, 0x01, 0x01, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x02, 0x00, "Off"			},
	{0x13, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x13, 0x01, 0x04, 0x00, "Off"			},
	{0x13, 0x01, 0x04, 0x04, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x08, 0x08, "Off"			},
	{0x13, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    3, "Coin A"		},
	{0x13, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x00, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    3, "Coin B"		},
	{0x13, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0xc0, 0x00, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x03, 0x01, "Easy"			},
	{0x14, 0x01, 0x03, 0x00, "Normal"		},
	{0x14, 0x01, 0x03, 0x02, "Hard"			},
	{0x14, 0x01, 0x03, 0x03, "Very Hard"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x14, 0x01, 0x0c, 0x00, "50k 200k 150k+"	},
	{0x14, 0x01, 0x0c, 0x04, "70k 270k 200k+"	},
	{0x14, 0x01, 0x0c, 0x08, "50k Only"		},
	{0x14, 0x01, 0x0c, 0x0c, "100k Only"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0x30, 0x20, "1"			},
	{0x14, 0x01, 0x30, 0x30, "2"			},
	{0x14, 0x01, 0x30, 0x00, "3"			},
	{0x14, 0x01, 0x30, 0x10, "5"			},

	{0   , 0xfe, 0   ,    2, "Dip Switch Display"	},
	{0x14, 0x01, 0x40, 0x00, "Off"			},
	{0x14, 0x01, 0x40, 0x40, "On"			},
	
	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x14, 0x01, 0x80, 0x00, "No"			},
	{0x14, 0x01, 0x80, 0x80, "Yes"			},
};

STDDIPINFO(Skyshark)


static struct BurnDIPInfo HishouzaDIPList[]=
{
	{0x13, 0xff, 0xff, 0x01, NULL			},
	{0x14, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x13, 0x01, 0x01, 0x01, "Upright"		},
	{0x13, 0x01, 0x01, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x02, 0x00, "Off"			},
	{0x13, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x13, 0x01, 0x04, 0x00, "Off"			},
	{0x13, 0x01, 0x04, 0x04, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x08, 0x08, "Off"			},
	{0x13, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x13, 0x01, 0x30, 0x20, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x00, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x30, 0x30, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0x30, 0x10, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x13, 0x01, 0xc0, 0x80, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0xc0, 0x00, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xc0, 0xc0, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0xc0, 0x40, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x03, 0x01, "Easy"			},
	{0x14, 0x01, 0x03, 0x00, "Normal"		},
	{0x14, 0x01, 0x03, 0x02, "Hard"			},
	{0x14, 0x01, 0x03, 0x03, "Very Hard"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x14, 0x01, 0x0c, 0x00, "50k 200k 150k+"	},
	{0x14, 0x01, 0x0c, 0x04, "70k 270k 200k+"	},
	{0x14, 0x01, 0x0c, 0x08, "50k Only"		},
	{0x14, 0x01, 0x0c, 0x0c, "100k Only"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0x30, 0x20, "1"			},
	{0x14, 0x01, 0x30, 0x30, "2"			},
	{0x14, 0x01, 0x30, 0x00, "3"			},
	{0x14, 0x01, 0x30, 0x10, "5"			},

	{0   , 0xfe, 0   ,    2, "Dip Switch Display"	},
	{0x14, 0x01, 0x40, 0x00, "Off"			},
	{0x14, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x14, 0x01, 0x80, 0x00, "No"			},
	{0x14, 0x01, 0x80, 0x80, "Yes"			},
};

STDDIPINFO(Hishouza)

static void palette_write(INT32 offset)
{
	offset &= 0xffe;

	INT32 p = *((UINT16*)(DrvPalRAM + offset));

	INT32 r = (p >>  0) & 0x1f;
	INT32 g = (p >>  5) & 0x1f;
	INT32 b = (p >> 10) & 0x1f;

	r = (r << 3) | (r >> 2);
	g = (g << 3) | (g >> 2);
	b = (b << 3) | (b >> 2);

	DrvPalette[offset/2] = BurnHighCol(r,g,b,0);
}

static void twincobr_dsp(INT32 enable)
{
	enable ^= 1;
	dsp_on = enable;

	if (enable)
	{
		tms32010_set_irq_line(0, CPU_IRQSTATUS_ACK); /* TMS32010 INT */
		m68k_halt = 1;
		SekRunEnd();
	}
	else
	{
		tms32010_set_irq_line(0, CPU_IRQSTATUS_NONE); /* TMS32010 INT */
	}
}

static void control_write(UINT16 data)
{
	switch (data)
	{
		case 0x0004:
		case 0x0005:
			irq_enable = data & 1;
		break;

		case 0x0006:
		case 0x0007:
			flipscreen = data & 1;
		break;

		case 0x0008:
		case 0x0009:
			bgrambank = ((data & 1) << 13);
		break;

		case 0x000a:
		case 0x000b:
			fgrombank = ((data & 1) << 12);
		break;

		case 0x000c:
		case 0x000d:
			twincobr_dsp(data & 1);
		break;

		case 0x000e:
		case 0x000f:
			displayenable = data & 1;
		break;
	}
}

static void __fastcall twincobr_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfff000) == 0x07a000) {
		DrvShareRAM[(address / 2) & 0x7ff] = data & 0xff;
		return;
	}

	if ((address & 0xfff000) == 0x050000) {
		*((UINT16*)(DrvPalRAM + (address & 0xffe))) = data;
		palette_write(address);
		return;
	}

	switch (address)
	{
		case 0x060000:
			// mc6845 address_w
		return;

		case 0x060002:
			// mc6845 register_w
		return;

		case 0x070000:
			scrollx[0] = data;
		return;

		case 0x070002:
			scrolly[0] = data;
		return;

		case 0x070004:
			vidramoffs[0] = data<<1;
		return;

		case 0x072000:
			scrollx[1] = data;
		return;

		case 0x072002:
			scrolly[1] = data;
		return;

		case 0x072004:
			vidramoffs[1] = data<<1;
		return;

		case 0x074000:
			scrollx[2] = data;
		return;

		case 0x074002:
			scrolly[2] = data;
		return;

		case 0x074004:
			vidramoffs[2] = data<<1;
		return;

		case 0x076000:
		case 0x076002:	// exscroll (not used)
		return;

		case 0x07800a:
			if (data < 2) twincobr_dsp(data);
		return;

		case 0x07800c:
			control_write(data & 0xff);
		return;

		case 0x07e000:
			*((UINT16*)(DrvTxRAM + (vidramoffs[0] & 0x0ffe))) = data;
		return;

		case 0x07e002:
			*((UINT16*)(DrvBgRAM + (vidramoffs[1] & 0x1ffe) + bgrambank)) = data;
		return;

		case 0x07e004:
			*((UINT16*)(DrvFgRAM + (vidramoffs[2] & 0x1ffe))) = data;
		return;
	}
}

static void __fastcall twincobr_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xfff000) == 0x050000) {
		DrvPalRAM[(address & 0xfff)^1] = data;
		palette_write(address);
		return;
	}

	bprintf (0, _T("MWB %5.5x, %2.2x\n"), address, data);
}

static UINT16 __fastcall twincobr_main_read_word(UINT32 address)
{
	if ((address & 0xfff000) == 0x07a000) {
		return DrvShareRAM[(address / 2) & 0x7ff] & 0xff;
	}

	switch (address)
	{
		case 0x07e000:
			return *((UINT16*)(DrvTxRAM + (vidramoffs[0] & 0x0ffe)));

		case 0x07e002:
			return *((UINT16*)(DrvBgRAM + (vidramoffs[1] & 0x1ffe) + bgrambank));

		case 0x07e004:
			return *((UINT16*)(DrvFgRAM + (vidramoffs[2] & 0x1ffe)));

		case 0x078000:
			return DrvDips[0];

		case 0x078002:
			return DrvDips[1];

		case 0x078004:
			return DrvInputs[0];

		case 0x078006:
			return DrvInputs[1];

		case 0x078008:
			return ((DrvInputs[3] & 0x7f) | (vblank ? 0x80 : 0)) ^ golfwar;
	}

	return 0;
}

static UINT8 __fastcall twincobr_main_read_byte(UINT32 address)
{
	if ((address & 0xfff000) == 0x07a000) {
		return DrvShareRAM[(address / 2) & 0x7ff] & 0xff;
	}

	switch (address)
	{
		case 0x078001:
			return DrvDips[0];

		case 0x078003:
			return DrvDips[1];

		case 0x078009:
			return ((vblank ? 0x80 : 0) | (DrvInputs[3] & 0x7f)) ^ golfwar;
	}

	bprintf (0, _T("MRB %5.5x\n"), address);

	return 0;
}

static void __fastcall twincobr_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
			BurnYM3812Write(0, port & 1, data);
		return;

		case 0x20:
			if (data < 2) twincobr_dsp(data);
		return;
	}
}

static UINT8 __fastcall twincobr_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
			return BurnYM3812Read(0, port & 1);

		case 0x10:
			return DrvInputs[2];

		case 0x40:
			return DrvDips[0];

		case 0x50:
			return DrvDips[1];
	}

	return 0;
}

static void twincobr_dsp_addrsel_w(UINT16 data)
{
	main_ram_seg = ((data & 0xe000) << 3);
	dsp_addr_w   = ((data & 0x1fff) << 1);
}

static UINT16 twincobr_dsp_r()
{
	switch (main_ram_seg)
	{
		case 0x30000:
		case 0x40000:
		case 0x50000:
			return SekReadWord(main_ram_seg + dsp_addr_w);
	}

	return 0;
}

static void twincobr_dsp_w(UINT16 data)
{
	dsp_execute = 0;

	switch (main_ram_seg)
	{
		case 0x30000: if ((dsp_addr_w < 3) && (data == 0)) dsp_execute = 1;
		case 0x40000:
		case 0x50000:
			SekWriteWord(main_ram_seg + dsp_addr_w, data);
		break;
	}
}

static void twincobr_dsp_bio_w(UINT16 data)
{
	if (data & 0x8000) {
		dsp_BIO = 0;
	}

	if (data == 0) {
		if (dsp_execute) {
			m68k_halt = 0;
			dsp_execute = 0;
			tms32010RunEnd();
		}

		dsp_BIO = 1;
	}
}

static UINT8 twincobr_BIO_r()
{
	return dsp_BIO;
}

static void dsp_write(INT32 port, UINT16 data)
{
	switch (port)
	{
		case 0x00: twincobr_dsp_addrsel_w(data); return;
		case 0x01: twincobr_dsp_w(data); return;
		case 0x02: return; // bootleg
		case 0x03: twincobr_dsp_bio_w(data); return;
	}
}

static UINT16 dsp_read(INT32 port)
{
	switch (port)
	{
		case 0x01: return twincobr_dsp_r();
		case 0x02: fsharkbt_8741 += 1; return (fsharkbt_8741 & 1);
		case 0x10: return twincobr_BIO_r();
	}

	return 0;
}

static void DrvFMIRQHandler(INT32, INT32 nStatus)
{
	ZetSetIRQLine(0, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 DrvSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)(double)ZetTotalCycles() * nSoundRate / 3500000;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	ZetOpen(0);
	ZetReset();
	BurnYM3812Reset();
	ZetClose();

	tms32010_reset();

	irq_enable = 0;
	flipscreen = 0;
	bgrambank = 0;
	fgrombank = 0;
	displayenable = 0;

	main_ram_seg = 0;
	dsp_addr_w = 0;
	dsp_execute = 0;
	dsp_BIO = 0;
	dsp_on = 0;

	fsharkbt_8741 = -1;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x040000;
	DrvMCUROM		= Next; Next += 0x004000;
	DrvZ80ROM		= Next; Next += 0x008000;

	DrvGfxROM0		= Next; Next += 0x020000;
	DrvGfxROM1		= Next; Next += 0x080000;
	DrvGfxROM2		= Next; Next += 0x040000;
	DrvGfxROM3		= Next; Next += 0x080000;

	DrvPalette		= (UINT32*)Next; Next += 0x0700 * sizeof(UINT32);

	pTempDraw		= (UINT16*)Next; Next += nScreenWidth * nScreenHeight * sizeof(UINT16);

	AllRam			= Next;

	DrvSprBuf		= Next; Next += 0x001000;
	DrvSprRAM		= Next; Next += 0x001000;
	Drv68KRAM		= Next; Next += 0x010000;
	DrvMCURAM		= Next; Next += 0x010000;
	DrvPalRAM		= Next; Next += 0x000e00;
	DrvShareRAM		= Next; Next += 0x000800;

	DrvBgRAM		= Next; Next += 0x004000;
	DrvFgRAM		= Next; Next += 0x002000;
	DrvTxRAM		= Next; Next += 0x001000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[3] = { (0x4000*8*0), (0x4000*8*1), (0x4000*8*2) };
	INT32 Plane1[4] = { (0x8000*8*0), (0x8000*8*1), (0x8000*8*2), (0x8000*8*3) };
	INT32 Plane2[4] = { (0x10000*8*0), (0x10000*8*1), (0x10000*8*2), (0x10000*8*3) };
	INT32 XOffs[16] = { STEP16(0,1) };
	INT32 YOffs[8] = { STEP8(0,8) };
	INT32 YOffs1[16] = { STEP16(0,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x40000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0xc000);

	GfxDecode(0x0800, 3, 8, 8, Plane0, XOffs, YOffs, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x40000);

	GfxDecode(0x2000, 4, 8, 8, Plane2, XOffs, YOffs, 0x040, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x20000);

	GfxDecode(0x1000, 4, 8, 8, Plane1, XOffs, YOffs, 0x040, tmp, DrvGfxROM2);

	memcpy (tmp, DrvGfxROM3, 0x40000);

	GfxDecode(0x0800, 4, 16, 16, Plane2, XOffs, YOffs1, 0x100, tmp, DrvGfxROM3);

	BurnFree(tmp);

	return 0;
}

static INT32 LoadNibbles(UINT8 *dst, INT32 idx, INT32 len)
{
	UINT8 *tmp = (UINT8*)BurnMalloc(len*2);

	if (BurnLoadRom(dst + 0, idx + 1, 2)) return 1;
	if (BurnLoadRom(dst + 1, idx + 3, 2)) return 1;
	if (BurnLoadRom(tmp + 0, idx + 0, 2)) return 1;
	if (BurnLoadRom(tmp + 1, idx + 2, 2)) return 1;

	for (INT32 i = 0; i < len * 2; i++) {
		dst[i] = (dst[i] & 0xf) | (tmp[i] << 4);
	}

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit(INT32 game_select)
{
	GenericTilesInit();

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (game_select == 0) // Twin Cobra
	{
		if (BurnLoadRom(Drv68KROM  + 0x00001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x00000,  1, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x20001,  2, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x20000,  3, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x00000,  4, 1)) return 1;

		if (BurnLoadRom(DrvMCUROM  + 0x00001,  5, 2)) return 1;
		if (BurnLoadRom(DrvMCUROM  + 0x00000,  6, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x04000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x08000,  9, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x10000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x20000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x30000, 13, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000, 14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x08000, 15, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x10000, 16, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x18000, 17, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x00000, 18, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x10000, 19, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x20000, 20, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x30000, 21, 1)) return 1;
	}
	else if (game_select == 1) // Flying Shark
	{
		if (BurnLoadRom(Drv68KROM  + 0x00001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x00000,  1, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x00000,  2, 1)) return 1;

		if (LoadNibbles(DrvMCUROM  + 0x00000,  3, 0x0400)) return 1;
		if (LoadNibbles(DrvMCUROM  + 0x00800,  7, 0x0400)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x04000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x08000, 13, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000, 14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x10000, 15, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x20000, 16, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x30000, 17, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000, 18, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x08000, 19, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x10000, 20, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x18000, 21, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x00000, 22, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x10000, 23, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x20000, 24, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x30000, 25, 1)) return 1;
	}
	else if (game_select == 2 || game_select == 3) // gulfwar2
	{
		if (BurnLoadRom(Drv68KROM  + 0x00001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x00000,  1, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x00000,  2, 1)) return 1;

		if (BurnLoadRom(DrvMCUROM  + 0x00001,  3, 2)) return 1;
		if (BurnLoadRom(DrvMCUROM  + 0x00000,  4, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x04000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x08000,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x10000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x20000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x30000, 11, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x08000, 13, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x10000, 14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x18000, 15, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x00000, 16, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x10000, 17, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x20000, 18, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x30000, 19, 1)) return 1;

		golfwar = (game_select == 2) ? 0xff : 0;
	}

	DrvGfxDecode();

	BurnSetRefreshRate(54.877858);

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x02ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,		0x030000, 0x033fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,		0x040000, 0x040fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		0x050000, 0x050dff, MAP_ROM);
//	SekMapMemory(DrvShareRAM,	0x07a000, 0x07afff, MAP_RAM);
	SekSetWriteWordHandler(0,	twincobr_main_write_word);
	SekSetWriteByteHandler(0,	twincobr_main_write_byte);
	SekSetReadWordHandler(0,	twincobr_main_read_word);
	SekSetReadByteHandler(0,	twincobr_main_read_byte);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvShareRAM,	0x8000, 0x87ff, MAP_RAM);
	ZetSetOutHandler(twincobr_sound_write_port);
	ZetSetInHandler(twincobr_sound_read_port);
	ZetClose();

	tms32010_init();
	tms32010_set_write_port_handler(dsp_write);
	tms32010_set_read_port_handler(dsp_read);
	tms32010_ram = (UINT16*)DrvMCURAM;
	tms32010_rom = (UINT16*)DrvMCUROM;

	BurnYM3812Init(1, 3500000, &DrvFMIRQHandler, &DrvSynchroniseStream, 0);
	BurnTimerAttachZetYM3812(3500000);
	BurnYM3812SetRoute(0, BURN_SND_YM3812_ROUTE, 1.00, BURN_SND_ROUTE_BOTH);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	BurnYM3812Exit();

	ZetExit();
	SekExit();
	tms32010_exit();

	BurnFree (AllMem);

	golfwar = 0;

	return 0;
}

static void DrvPaletteUpdate()
{
	for (INT32 i = 0; i < 0xe00; i+=2) {
		INT32 p = *((UINT16*)(DrvPalRAM + i));

		INT32 r = (p >>  0) & 0x1f;
		INT32 g = (p >>  5) & 0x1f;
		INT32 b = (p >> 10) & 0x1f;

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);

		DrvPalette[i/2] = BurnHighCol(r,g,b,0);
	}
}

static void draw_layer(INT32 layer, INT32 rambank, INT32 rombank)
{
	UINT16 *ram[3] = { (UINT16*)DrvTxRAM, (UINT16*)DrvBgRAM, (UINT16*)DrvFgRAM };
	UINT8 *gfx[3] = { DrvGfxROM0, DrvGfxROM2, DrvGfxROM1 };
	INT32 colbank[3] = { 0x600, 0x400, 0x500 };
	INT32 colshift[3] = { 11, 12, 12 };
	INT32 depth = colshift[layer] - 8;

	INT32 transp = (layer & 1) ? 0xff : 0;

	INT32 height = (layer ? 64 : 32) * 8;

	INT32 xscroll = (scrollx[layer] + 55 ) & 0x1ff;
	INT32 yscroll = (scrolly[layer] + 30) & (height - 1);

	for (INT32 offs = 0; offs < 64 * (height/8); offs++)
	{
		INT32 sx = (offs & 0x3f) * 8;
		INT32 sy = (offs / 0x40) * 8;

		sx -= xscroll;
		if (sx < -7) sx += 512;
		sy -= yscroll;
		if (sy < -7) sy += height;

		INT32 attr  = ram[layer][offs + (rambank/2)];
		INT32 color = attr >> colshift[layer];
		INT32 code  = (attr & ((1 << colshift[layer])-1)) + rombank;

		Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy, color, depth, transp, colbank[layer], gfx[layer]);
	}
}

static void predraw_sprites()
{
	UINT16 *ram = (UINT16*)DrvSprBuf;

	INT32 xoffs = 31;
	INT32 xoffs_flipped = 14;

	memset (pTempDraw, 0, nScreenWidth * nScreenHeight * sizeof(short));

	for (INT32 offs = 0; offs < 0x1000/2; offs += 4)
	{
		INT32 attr = ram[offs + 1];
		INT32 prio = (attr >> 10) & 3;
		if (prio == 0) continue;

		INT32 sy = ram[offs + 3] >> 7;

		if (sy != 0x0100)
		{
			INT32 code  = ram[offs] & 0x7ff;
			INT32 color = (attr & 0x3f) | ((attr >> 4) & 0xc0);

			INT32 sx    = ram[offs + 2] >> 7;
			INT32 flipx = attr & 0x100;
			if (flipx) sx -= xoffs_flipped;

			INT32 flipy = attr & 0x200;

			sx -= xoffs;
			sy -= 16;

			if (flipx) {
				if (flipy) {
					Render16x16Tile_Mask_FlipXY_Clip(pTempDraw, code, sx, sy, color, 4, 0, 0, DrvGfxROM3);
				} else {
					Render16x16Tile_Mask_FlipX_Clip(pTempDraw, code, sx, sy, color, 4, 0, 0, DrvGfxROM3);
				}
			} else {
				if (flipy) {
					Render16x16Tile_Mask_FlipY_Clip(pTempDraw, code, sx, sy, color, 4, 0, 0, DrvGfxROM3);
				} else {
					Render16x16Tile_Mask_Clip(pTempDraw, code, sx, sy, color, 4, 0, 0, DrvGfxROM3);
				}
			}
		}
	}
}

static void draw_sprites(INT32 priority)
{
	priority <<= 10;

	for (INT32 y = 0;y < nScreenHeight; y++)
	{
		UINT16* src = pTempDraw + y * nScreenWidth;
		UINT16* dst = pTransDraw + y * nScreenWidth;

		for (INT32 x = 0;x < nScreenWidth; x++)
		{
			UINT16 pix = src[x];

			if (pix & 0xf)
			{
				if ((pix & 0xc00) == priority)
				{
					dst[x] = pix & 0x3ff;
				}
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 0;
	}

	BurnTransferClear();

	if (displayenable)
	{
		predraw_sprites();

		if (nBurnLayer & 1) draw_layer(1, bgrambank, 0);
		if (nSpriteEnable & 1) draw_sprites(1);
		if (nBurnLayer & 2) draw_layer(2, 0, fgrombank);
		if (nSpriteEnable & 2) draw_sprites(2);
		if (nBurnLayer & 4) draw_layer(0, 0, 0);
		if (nSpriteEnable & 4) draw_sprites(3);
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	SekNewFrame();
	ZetNewFrame();

	{
		memset (DrvInputs, 0, 4);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}
	}

	INT32 nInterleave = 286;
	INT32 nCyclesTotal[3] = { (INT32)(double)(7000000 / 54.877858), (INT32)(double)(3500000 / 54.877858), (INT32)(double)(14000000 / 54.877858) };
	INT32 nCyclesDone[3] = { 0, 0, 0 };

	SekOpen(0);
	ZetOpen(0);

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		INT32 nSegment = nCyclesTotal[0] / nInterleave;

		if (m68k_halt) {
			nCyclesDone[0] += nSegment;
			SekIdle(nSegment);
		} else {
			nCyclesDone[0] += SekRun(nSegment);

			if (i == 240 && irq_enable) {
				irq_enable = 0;
				SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
			}
		}

		if (dsp_on) tms32010_execute(nCyclesTotal[2] / nInterleave);

		BurnTimerUpdateYM3812((i + 1) * (nCyclesTotal[1] / nInterleave));

		if (i == 240) {
			if (pBurnDraw) {
				DrvDraw();
			}
			vblank = 1;
		}
	}

	BurnTimerEndFrameYM3812(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		BurnYM3812Update(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	SekClose();

	memcpy (DrvSprBuf, DrvSprRAM, 0x1000);

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029719;
	}

	if (nAction & ACB_MEMORY_RAM)
	{
		memset(&ba, 0, sizeof(ba));
		ba.Data		= AllRam;
		ba.nLen		= RamEnd - AllRam;
		ba.szName	= "RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA)
	{
		SekScan(nAction);
		ZetScan(nAction);
		tms32010_scan(nAction);

		BurnYM3812Scan(nAction, pnMin);

		SCAN_VAR(m68k_halt);
		SCAN_VAR(irq_enable);
		SCAN_VAR(flipscreen);
		SCAN_VAR(bgrambank);
		SCAN_VAR(fgrombank);
		SCAN_VAR(displayenable);
		SCAN_VAR(main_ram_seg);
		SCAN_VAR(dsp_addr_w);
		SCAN_VAR(dsp_execute);
		SCAN_VAR(dsp_BIO);
		SCAN_VAR(dsp_on);
		SCAN_VAR(scrollx);
		SCAN_VAR(scrolly);
		SCAN_VAR(vidramoffs);

		SCAN_VAR(fsharkbt_8741);
	}

	return 0;
}


// Twin Cobra (World)

static struct BurnRomInfo twincobrRomDesc[] = {
	{ "b30_01.7j",		0x10000, 0x07f64d13, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "b30_03.7h",		0x10000, 0x41be6978, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "b30_26_ii.8j",	0x08000, 0x3a646618, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "b30_27_ii.8h",	0x08000, 0xd7d1e317, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "b30_05_ii.4f",	0x08000, 0xe37b3c44, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 code

	{ "dsp_22.bin",		0x00800, 0x79389a71, 3 | BRF_PRG | BRF_ESS }, //  5 TMS32010 code
	{ "dsp_21.bin",		0x00800, 0x2d135376, 3 | BRF_PRG | BRF_ESS }, //  6

	{ "b30_08.8c",		0x04000, 0x0a254133, 4 | BRF_GRA },           //  7 Text characters
	{ "b30_07.10b",		0x04000, 0xe9e2d4b1, 4 | BRF_GRA },           //  8
	{ "b30_06.8b",		0x04000, 0xa599d845, 4 | BRF_GRA },           //  9

	{ "b30_16.20b",		0x10000, 0x15b3991d, 5 | BRF_GRA },           // 10 Background tiles
	{ "b30_15.18b",		0x10000, 0xd9e2e55d, 5 | BRF_GRA },           // 11
	{ "b30_13.18c",		0x10000, 0x13daeac8, 5 | BRF_GRA },           // 12
	{ "b30_14.20c",		0x10000, 0x8cc79357, 5 | BRF_GRA },           // 13

	{ "b30_12.16c",		0x08000, 0xb5d48389, 6 | BRF_GRA },           // 14 Foreground tiles
	{ "b30_11.14c",		0x08000, 0x97f20fdc, 6 | BRF_GRA },           // 15
	{ "b30_10.12c",		0x08000, 0x170c01db, 6 | BRF_GRA },           // 16
	{ "b30_09.10c",		0x08000, 0x44f5accd, 6 | BRF_GRA },           // 17

	{ "b30_20.12d",		0x10000, 0xcb4092b8, 7 | BRF_GRA },           // 18 Sprites
	{ "b30_19.14d",		0x10000, 0x9cb8675e, 7 | BRF_GRA },           // 19
	{ "b30_18.15d",		0x10000, 0x806fb374, 7 | BRF_GRA },           // 20
	{ "b30_17.16d",		0x10000, 0x4264bff8, 7 | BRF_GRA },           // 21

	{ "82s129.d3",		0x00100, 0x24e7d62f, 0 | BRF_OPT },           // 22 Proms (not used)
	{ "82s129.d4",		0x00100, 0xa50cef09, 0 | BRF_OPT },           // 23
	{ "82s123.d2",		0x00020, 0xf72482db, 0 | BRF_OPT },           // 24
	{ "82s123.e18",		0x00020, 0xbc88cced, 0 | BRF_OPT },           // 25
	{ "82s123.b24",		0x00020, 0x4fb5df2a, 0 | BRF_OPT },           // 26
};

STD_ROM_PICK(twincobr)
STD_ROM_FN(twincobr)

static INT32 twincobrInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvTwincobr = {
	"twincobr", NULL, NULL, NULL, "1987",
	"Twin Cobra (World)\0", NULL, "Toaplan / Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TOAPLAN_MISC, GBF_VERSHOOT, 0,
	NULL, twincobrRomInfo, twincobrRomName, NULL, NULL, TwincobrInputInfo, TwincobrDIPInfo,
	twincobrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x700,
	240, 320, 3, 4
};


// Twin Cobra (US)

static struct BurnRomInfo twincobruRomDesc[] = {
	{ "b30_01.7j",		0x10000, 0x07f64d13, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "b30_03.7h",		0x10000, 0x41be6978, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "b30_26_i.8j",	0x08000, 0xbdd00ba4, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "b30_27_i.8h",	0x08000, 0xed600907, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "b30_05.4f",		0x08000, 0x1a8f1e10, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 code

	{ "dsp_22.bin",		0x00800, 0x79389a71, 3 | BRF_PRG | BRF_ESS }, //  5 TMS32010 code
	{ "dsp_21.bin",		0x00800, 0x2d135376, 3 | BRF_PRG | BRF_ESS }, //  6

	{ "b30_08.8c",		0x04000, 0x0a254133, 4 | BRF_GRA },           //  7 Text characters
	{ "b30_07.10b",		0x04000, 0xe9e2d4b1, 4 | BRF_GRA },           //  8
	{ "b30_06.8b",		0x04000, 0xa599d845, 4 | BRF_GRA },           //  9

	{ "b30_16.20b",		0x10000, 0x15b3991d, 5 | BRF_GRA },           // 10 Background tiles
	{ "b30_15.18b",		0x10000, 0xd9e2e55d, 5 | BRF_GRA },           // 11
	{ "b30_13.18c",		0x10000, 0x13daeac8, 5 | BRF_GRA },           // 12
	{ "b30_14.20c",		0x10000, 0x8cc79357, 5 | BRF_GRA },           // 13

	{ "b30_12.16c",		0x08000, 0xb5d48389, 6 | BRF_GRA },           // 14 Foreground tiles
	{ "b30_11.14c",		0x08000, 0x97f20fdc, 6 | BRF_GRA },           // 15
	{ "b30_10.12c",		0x08000, 0x170c01db, 6 | BRF_GRA },           // 16
	{ "b30_09.10c",		0x08000, 0x44f5accd, 6 | BRF_GRA },           // 17

	{ "b30_20.12d",		0x10000, 0xcb4092b8, 7 | BRF_GRA },           // 18 Sprites
	{ "b30_19.14d",		0x10000, 0x9cb8675e, 7 | BRF_GRA },           // 19
	{ "b30_18.15d",		0x10000, 0x806fb374, 7 | BRF_GRA },           // 20
	{ "b30_17.16d",		0x10000, 0x4264bff8, 7 | BRF_GRA },           // 21

	{ "82s129.d3",		0x00100, 0x24e7d62f, 0 | BRF_OPT },           // 22 Proms (not used)
	{ "82s129.d4",		0x00100, 0xa50cef09, 0 | BRF_OPT },           // 23
	{ "82s123.d2",		0x00020, 0xf72482db, 0 | BRF_OPT },           // 24
	{ "82s123.e18",		0x00020, 0xbc88cced, 0 | BRF_OPT },           // 25
	{ "82s123.b24",		0x00020, 0x4fb5df2a, 0 | BRF_OPT },           // 26
};

STD_ROM_PICK(twincobru)
STD_ROM_FN(twincobru)

struct BurnDriver BurnDrvTwincobru = {
	"twincobru", "twincobr", NULL, NULL, "1987",
	"Twin Cobra (US)\0", NULL, "Toaplan / Taito America Corporation (Romstar license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TOAPLAN_MISC, GBF_VERSHOOT, 0,
	NULL, twincobruRomInfo, twincobruRomName, NULL, NULL, TwincobrInputInfo, TwincobruDIPInfo,
	twincobrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x700,
	240, 320, 3, 4
};


// Kyukyoku Tiger (Japan)

static struct BurnRomInfo ktigerRomDesc[] = {
	{ "b30_01.7j",		0x10000, 0x07f64d13, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "b30_03.7h",		0x10000, 0x41be6978, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "b30_02.8j",		0x08000, 0x1d63e9c4, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "b30_04.8h",		0x08000, 0x03957a30, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "b30_05.4f",		0x08000, 0x1a8f1e10, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 code

	{ "dsp-22",			0x00800, 0x8a1d48d9, 3 | BRF_PRG | BRF_ESS }, //  5 TMS32010 code
	{ "dsp-21",			0x00800, 0x33d99bc2, 3 | BRF_PRG | BRF_ESS }, //  6

	{ "b30_08.8c",		0x04000, 0x0a254133, 4 | BRF_GRA },           //  7 Text characters
	{ "b30_07.10b",		0x04000, 0xe9e2d4b1, 4 | BRF_GRA },           //  8
	{ "b30_06.8b",		0x04000, 0xa599d845, 4 | BRF_GRA },           //  9

	{ "b30_16.20b",		0x10000, 0x15b3991d, 5 | BRF_GRA },           // 10 Background tiles
	{ "b30_15.18b",		0x10000, 0xd9e2e55d, 5 | BRF_GRA },           // 11
	{ "b30_13.18c",		0x10000, 0x13daeac8, 5 | BRF_GRA },           // 12
	{ "b30_14.20c",		0x10000, 0x8cc79357, 5 | BRF_GRA },           // 13

	{ "b30_12.16c",		0x08000, 0xb5d48389, 6 | BRF_GRA },           // 14 Foreground tiles
	{ "b30_11.14c",		0x08000, 0x97f20fdc, 6 | BRF_GRA },           // 15
	{ "b30_10.12c",		0x08000, 0x170c01db, 6 | BRF_GRA },           // 16
	{ "b30_09.10c",		0x08000, 0x44f5accd, 6 | BRF_GRA },           // 17

	{ "b30_20.12d",		0x10000, 0xcb4092b8, 7 | BRF_GRA },           // 18 Sprites
	{ "b30_19.14d",		0x10000, 0x9cb8675e, 7 | BRF_GRA },           // 19
	{ "b30_18.15d",		0x10000, 0x806fb374, 7 | BRF_GRA },           // 20
	{ "b30_17.16d",		0x10000, 0x4264bff8, 7 | BRF_GRA },           // 21

	{ "82s129.d3",		0x00100, 0x24e7d62f, 0 | BRF_OPT },           // 22 Proms (not used)
	{ "82s129.d4",		0x00100, 0xa50cef09, 0 | BRF_OPT },           // 23
	{ "82s123.d2",		0x00020, 0xf72482db, 0 | BRF_OPT },           // 24
	{ "82s123.e18",		0x00020, 0xbc88cced, 0 | BRF_OPT },           // 25
	{ "82s123.b24",		0x00020, 0x4fb5df2a, 0 | BRF_OPT },           // 26
};

STD_ROM_PICK(ktiger)
STD_ROM_FN(ktiger)

struct BurnDriver BurnDrvKtiger = {
	"ktiger", "twincobr", NULL, NULL, "1987",
	"Kyukyoku Tiger (Japan)\0", NULL, "Toaplan / Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TOAPLAN_MISC, GBF_VERSHOOT, 0,
	NULL, ktigerRomInfo, ktigerRomName, NULL, NULL, TwincobrInputInfo, KtigerDIPInfo,
	twincobrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x700,
	240, 320, 3, 4
};


// Flying Shark (World)

static struct BurnRomInfo fsharkRomDesc[] = {
	{ "b02_18-1.m8",	0x10000, 0x04739e02, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "b02_17-1.p8",	0x10000, 0xfd6ef7a8, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "b02_16.l5",		0x08000, 0xcdd1a153, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 code

	{ "82s137-1.mcu",	0x00400, 0xcc5b3f53, 3 | BRF_PRG | BRF_ESS }, //  3 TMS32010 code
	{ "82s137-2.mcu",	0x00400, 0x47351d55, 3 | BRF_PRG | BRF_ESS }, //  4
	{ "82s137-3.mcu",	0x00400, 0x70b537b9, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "82s137-4.mcu",	0x00400, 0x6edb2de8, 3 | BRF_PRG | BRF_ESS }, //  6
	{ "82s137-5.mcu",	0x00400, 0xf35b978a, 3 | BRF_PRG | BRF_ESS }, //  7
	{ "82s137-6.mcu",	0x00400, 0x0459e51b, 3 | BRF_PRG | BRF_ESS }, //  8
	{ "82s137-7.mcu",	0x00400, 0xcbf3184b, 3 | BRF_PRG | BRF_ESS }, //  9
	{ "82s137-8.mcu",	0x00400, 0x8246a05c, 3 | BRF_PRG | BRF_ESS }, // 10

	{ "b02_07-1.h11",	0x04000, 0xe669f80e, 4 | BRF_GRA },           // 11 Text characters
	{ "b02_06-1.h10",	0x04000, 0x5e53ae47, 4 | BRF_GRA },           // 12
	{ "b02_05-1.h8",	0x04000, 0xa8b05bd0, 4 | BRF_GRA },           // 13

	{ "b02_12.h20",		0x08000, 0x733b9997, 5 | BRF_GRA },           // 14 Background tiles
	{ "b02_15.h24",		0x08000, 0x8b70ef32, 5 | BRF_GRA },           // 15
	{ "b02_14.h23",		0x08000, 0xf711ba7d, 5 | BRF_GRA },           // 16
	{ "b02_13.h21",		0x08000, 0x62532cd3, 5 | BRF_GRA },           // 17

	{ "b02_08.h13",		0x08000, 0xef0cf49c, 6 | BRF_GRA },           // 18 Foreground tiles
	{ "b02_11.h18",		0x08000, 0xf5799422, 6 | BRF_GRA },           // 19
	{ "b02_10.h16",		0x08000, 0x4bd099ff, 6 | BRF_GRA },           // 20
	{ "b02_09.h15",		0x08000, 0x230f1582, 6 | BRF_GRA },           // 21

	{ "b02_01.d15",		0x10000, 0x2234b424, 7 | BRF_GRA },           // 22 Sprites
	{ "b02_02.d16",		0x10000, 0x30d4c9a8, 7 | BRF_GRA },           // 23
	{ "b02_03.d17",		0x10000, 0x64f3d88f, 7 | BRF_GRA },           // 24
	{ "b02_04.d20",		0x10000, 0x3b23a9fc, 7 | BRF_GRA },           // 25

	{ "b02-20.b4",		0x00100, 0x24e7d62f, 0 | BRF_OPT },           // 26 Proms (not used)
	{ "b02-21.b5",		0x00100, 0xa50cef09, 0 | BRF_OPT },           // 27
	{ "b02-19.b2",		0x00020, 0xf72482db, 0 | BRF_OPT },           // 28
	{ "b02-22.c21",		0x00020, 0xbc88cced, 0 | BRF_OPT },           // 29
	{ "b02-23.f28",		0x00020, 0x4fb5df2a, 0 | BRF_OPT },           // 30
};

STD_ROM_PICK(fshark)
STD_ROM_FN(fshark)

static INT32 fsharkInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvFshark = {
	"fshark", NULL, NULL, NULL, "1987",
	"Flying Shark (World)\0", NULL, "Toaplan / Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TOAPLAN_MISC, GBF_VERSHOOT, 0,
	NULL, fsharkRomInfo, fsharkRomName, NULL, NULL, FsharkInputInfo, FsharkDIPInfo,
	fsharkInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x700,
	240, 320, 3, 4
};


// Sky Shark (US, set 1)

static struct BurnRomInfo skysharkRomDesc[] = {
	{ "b02_18-2.m8",	0x10000, 0x888e90f3, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "b02_17-2.p8",	0x10000, 0x066d67be, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "b02_16.l5",		0x08000, 0xcdd1a153, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 code

	{ "82s137-1.mcu",	0x00400, 0xcc5b3f53, 3 | BRF_PRG | BRF_ESS }, //  3 TMS32010 code
	{ "82s137-2.mcu",	0x00400, 0x47351d55, 3 | BRF_PRG | BRF_ESS }, //  4
	{ "82s137-3.mcu",	0x00400, 0x70b537b9, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "82s137-4.mcu",	0x00400, 0x6edb2de8, 3 | BRF_PRG | BRF_ESS }, //  6
	{ "82s137-5.mcu",	0x00400, 0xf35b978a, 3 | BRF_PRG | BRF_ESS }, //  7
	{ "82s137-6.mcu",	0x00400, 0x0459e51b, 3 | BRF_PRG | BRF_ESS }, //  8
	{ "82s137-7.mcu",	0x00400, 0xcbf3184b, 3 | BRF_PRG | BRF_ESS }, //  9
	{ "82s137-8.mcu",	0x00400, 0x8246a05c, 3 | BRF_PRG | BRF_ESS }, // 10

	{ "b02_7-2.h11",	0x04000, 0xaf48c4e6, 4 | BRF_GRA },           // 11 Text characters
	{ "b02_6-2.h10",	0x04000, 0x9a29a862, 4 | BRF_GRA },           // 12
	{ "b02_5-2.h8",		0x04000, 0xfb7cad55, 4 | BRF_GRA },           // 13

	{ "b02_12.h20",		0x08000, 0x733b9997, 5 | BRF_GRA },           // 14 Background tiles
	{ "b02_15.h24",		0x08000, 0x8b70ef32, 5 | BRF_GRA },           // 15
	{ "b02_14.h23",		0x08000, 0xf711ba7d, 5 | BRF_GRA },           // 16
	{ "b02_13.h21",		0x08000, 0x62532cd3, 5 | BRF_GRA },           // 17

	{ "b02_08.h13",		0x08000, 0xef0cf49c, 6 | BRF_GRA },           // 18 Foreground tiles
	{ "b02_11.h18",		0x08000, 0xf5799422, 6 | BRF_GRA },           // 19
	{ "b02_10.h16",		0x08000, 0x4bd099ff, 6 | BRF_GRA },           // 20
	{ "b02_09.h15",		0x08000, 0x230f1582, 6 | BRF_GRA },           // 21

	{ "b02_01.d15",		0x10000, 0x2234b424, 7 | BRF_GRA },           // 22 Sprites
	{ "b02_02.d16",		0x10000, 0x30d4c9a8, 7 | BRF_GRA },           // 23
	{ "b02_03.d17",		0x10000, 0x64f3d88f, 7 | BRF_GRA },           // 24
	{ "b02_04.d20",		0x10000, 0x3b23a9fc, 7 | BRF_GRA },           // 25

	{ "b02-20.b4",		0x00100, 0x24e7d62f, 0 | BRF_OPT },           // 26 Proms (not used)
	{ "b02-21.b5",		0x00100, 0xa50cef09, 0 | BRF_OPT },           // 27
	{ "b02-19.b2",		0x00020, 0xf72482db, 0 | BRF_OPT },           // 28
	{ "b02-22.c21",		0x00020, 0xbc88cced, 0 | BRF_OPT },           // 29
	{ "b02-23.f28",		0x00020, 0x4fb5df2a, 0 | BRF_OPT },           // 30
};

STD_ROM_PICK(skyshark)
STD_ROM_FN(skyshark)

struct BurnDriver BurnDrvSkyshark = {
	"skyshark", "fshark", NULL, NULL, "1987",
	"Sky Shark (US, set 1)\0", NULL, "Toaplan / Taito America Corporation (Romstar license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TOAPLAN_MISC, GBF_VERSHOOT, 0,
	NULL, skysharkRomInfo, skysharkRomName, NULL, NULL, FsharkInputInfo, SkysharkDIPInfo,
	fsharkInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x700,
	240, 320, 3, 4
};


// Sky Shark (US, set 2)

static struct BurnRomInfo skysharkaRomDesc[] = {
	{ "b02_18-2.m8",	0x10000, 0x341deaac, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "b02_17-2.p8",	0x10000, 0xec3b5a2c, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "b02_16.l5",		0x08000, 0xcdd1a153, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 code

	{ "82s137-1.mcu",	0x00400, 0xcc5b3f53, 3 | BRF_PRG | BRF_ESS }, //  3 TMS32010 code
	{ "82s137-2.mcu",	0x00400, 0x47351d55, 3 | BRF_PRG | BRF_ESS }, //  4
	{ "82s137-3.mcu",	0x00400, 0x70b537b9, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "82s137-4.mcu",	0x00400, 0x6edb2de8, 3 | BRF_PRG | BRF_ESS }, //  6
	{ "82s137-5.mcu",	0x00400, 0xf35b978a, 3 | BRF_PRG | BRF_ESS }, //  7
	{ "82s137-6.mcu",	0x00400, 0x0459e51b, 3 | BRF_PRG | BRF_ESS }, //  8
	{ "82s137-7.mcu",	0x00400, 0xcbf3184b, 3 | BRF_PRG | BRF_ESS }, //  9
	{ "82s137-8.mcu",	0x00400, 0x8246a05c, 3 | BRF_PRG | BRF_ESS }, // 10

	{ "b02_7-2.h11",	0x04000, 0xaf48c4e6, 4 | BRF_GRA },           // 11 Text characters
	{ "b02_6-2.h10",	0x04000, 0x9a29a862, 4 | BRF_GRA },           // 12
	{ "b02_5-2.h8",		0x04000, 0xfb7cad55, 4 | BRF_GRA },           // 13

	{ "b02_12.h20",		0x08000, 0x733b9997, 5 | BRF_GRA },           // 14 Background tiles
	{ "b02_15.h24",		0x08000, 0x8b70ef32, 5 | BRF_GRA },           // 15
	{ "b02_14.h23",		0x08000, 0xf711ba7d, 5 | BRF_GRA },           // 16
	{ "b02_13.h21",		0x08000, 0x62532cd3, 5 | BRF_GRA },           // 17

	{ "b02_08.h13",		0x08000, 0xef0cf49c, 6 | BRF_GRA },           // 18 Foreground tiles
	{ "b02_11.h18",		0x08000, 0xf5799422, 6 | BRF_GRA },           // 19
	{ "b02_10.h16",		0x08000, 0x4bd099ff, 6 | BRF_GRA },           // 20
	{ "b02_09.h15",		0x08000, 0x230f1582, 6 | BRF_GRA },           // 21

	{ "b02_01.d15",		0x10000, 0x2234b424, 7 | BRF_GRA },           // 22 Sprites
	{ "b02_02.d16",		0x10000, 0x30d4c9a8, 7 | BRF_GRA },           // 23
	{ "b02_03.d17",		0x10000, 0x64f3d88f, 7 | BRF_GRA },           // 24
	{ "b02_04.d20",		0x10000, 0x3b23a9fc, 7 | BRF_GRA },           // 25

	{ "b02-20.b4",		0x00100, 0x24e7d62f, 0 | BRF_OPT },           // 26 Proms (not used)
	{ "b02-21.b5",		0x00100, 0xa50cef09, 0 | BRF_OPT },           // 27
	{ "b02-19.b2",		0x00020, 0xf72482db, 0 | BRF_OPT },           // 28
	{ "b02-22.c21",		0x00020, 0xbc88cced, 0 | BRF_OPT },           // 29
	{ "b02-23.f28",		0x00020, 0x4fb5df2a, 0 | BRF_OPT },           // 30
};

STD_ROM_PICK(skysharka)
STD_ROM_FN(skysharka)

struct BurnDriver BurnDrvSkysharka = {
	"skysharka", "fshark", NULL, NULL, "1987",
	"Sky Shark (US, set 2)\0", NULL, "Toaplan / Taito America Corporation (Romstar license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TOAPLAN_MISC, GBF_VERSHOOT, 0,
	NULL, skysharkaRomInfo, skysharkaRomName, NULL, NULL, FsharkInputInfo, SkysharkDIPInfo,
	fsharkInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x700,
	240, 320, 3, 4
};


// Hishou Zame (Japan)

static struct BurnRomInfo hishouzaRomDesc[] = {
	{ "b02_18.m8",		0x10000, 0x4444bb94, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "b02_17.p8",		0x10000, 0xcdac7228, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "b02_16.l5",		0x08000, 0xcdd1a153, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 code

	{ "dsp-a1.bpr",		0x00400, 0x45d4d1b1, 3 | BRF_PRG | BRF_ESS }, //  3 TMS32010 code
	{ "dsp-a2.bpr",		0x00400, 0xedd227fa, 3 | BRF_PRG | BRF_ESS }, //  4
	{ "dsp-a3.bpr",		0x00400, 0xdf88e79b, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "dsp-a4.bpr",		0x00400, 0xa2094a7f, 3 | BRF_PRG | BRF_ESS }, //  6
	{ "dsp-b5.bpr",		0x00400, 0x85ca5d47, 3 | BRF_PRG | BRF_ESS }, //  7
	{ "dsp-b6.bpr",		0x00400, 0x81816b2c, 3 | BRF_PRG | BRF_ESS }, //  8
	{ "dsp-b7.bpr",		0x00400, 0xe87540cd, 3 | BRF_PRG | BRF_ESS }, //  9
	{ "dsp-b8.bpr",		0x00400, 0xd3c16c5c, 3 | BRF_PRG | BRF_ESS }, // 10

	{ "b02-07.h11",		0x04000, 0xc13a775e, 4 | BRF_GRA },           // 11 Text characters
	{ "b02-06.h10",		0x04000, 0xad5f1371, 4 | BRF_GRA },           // 12
	{ "b02-05.h8",		0x04000, 0x85a7bff6, 4 | BRF_GRA },           // 13

	{ "b02_12.h20",		0x08000, 0x733b9997, 5 | BRF_GRA },           // 14 Background tiles
	{ "b02_15.h24",		0x08000, 0x8b70ef32, 5 | BRF_GRA },           // 15
	{ "b02_14.h23",		0x08000, 0xf711ba7d, 5 | BRF_GRA },           // 16
	{ "b02_13.h21",		0x08000, 0x62532cd3, 5 | BRF_GRA },           // 17

	{ "b02_08.h13",		0x08000, 0xef0cf49c, 6 | BRF_GRA },           // 18 Foreground tiles
	{ "b02_11.h18",		0x08000, 0xf5799422, 6 | BRF_GRA },           // 19
	{ "b02_10.h16",		0x08000, 0x4bd099ff, 6 | BRF_GRA },           // 20
	{ "b02_09.h15",		0x08000, 0x230f1582, 6 | BRF_GRA },           // 21

	{ "b02_01.d15",		0x10000, 0x2234b424, 7 | BRF_GRA },           // 22 Sprites
	{ "b02_02.d16",		0x10000, 0x30d4c9a8, 7 | BRF_GRA },           // 23
	{ "b02_03.d17",		0x10000, 0x64f3d88f, 7 | BRF_GRA },           // 24
	{ "b02_04.d20",		0x10000, 0x3b23a9fc, 7 | BRF_GRA },           // 25

	{ "b02-20.b4",		0x00100, 0x24e7d62f, 0 | BRF_OPT },           // 26 Proms (not used)
	{ "b02-21.b5",		0x00100, 0xa50cef09, 0 | BRF_OPT },           // 27
	{ "b02-19.b2",		0x00020, 0xf72482db, 0 | BRF_OPT },           // 28
	{ "b02-22.c21",		0x00020, 0xbc88cced, 0 | BRF_OPT },           // 29
	{ "b02-23.f28",		0x00020, 0x4fb5df2a, 0 | BRF_OPT },           // 30
};

STD_ROM_PICK(hishouza)
STD_ROM_FN(hishouza)

struct BurnDriver BurnDrvHishouza = {
	"hishouza", "fshark", NULL, NULL, "1987",
	"Hishou Zame (Japan)\0", NULL, "Toaplan / Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TOAPLAN_MISC, GBF_VERSHOOT, 0,
	NULL, hishouzaRomInfo, hishouzaRomName, NULL, NULL, FsharkInputInfo, HishouzaDIPInfo,
	fsharkInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x700,
	240, 320, 3, 4
};


// Flying Shark (bootleg with 8741)

static struct BurnRomInfo fsharkbtRomDesc[] = {
	{ "r18",		0x10000, 0xef30f563, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "r17",		0x10000, 0x0e18d25f, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "b02_16.l5",		0x08000, 0xcdd1a153, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 code

	{ "mcu-1.bpr",		0x00400, 0x45d4d1b1, 3 | BRF_PRG | BRF_ESS }, //  3 TMS32010 code
	{ "mcu-2.bpr",		0x00400, 0x651336d1, 3 | BRF_PRG | BRF_ESS }, //  4
	{ "mcu-3.bpr",		0x00400, 0xdf88e79b, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "mcu-4.bpr",		0x00400, 0xa2094a7f, 3 | BRF_PRG | BRF_ESS }, //  6
	{ "mcu-5.bpr",		0x00400, 0xf97a58da, 3 | BRF_PRG | BRF_ESS }, //  7
	{ "mcu-6.bpr",		0x00400, 0xffcc422d, 3 | BRF_PRG | BRF_ESS }, //  8
	{ "mcu-7.bpr",		0x00400, 0x0cd30d49, 3 | BRF_PRG | BRF_ESS }, //  9
	{ "mcu-8.bpr",		0x00400, 0x3379bbff, 3 | BRF_PRG | BRF_ESS }, // 10

	{ "b02_07-1.h11",	0x04000, 0xe669f80e, 4 | BRF_GRA },           // 11 Text characters
	{ "b02_06-1.h10",	0x04000, 0x5e53ae47, 4 | BRF_GRA },           // 12
	{ "b02_05-1.h8",	0x04000, 0xa8b05bd0, 4 | BRF_GRA },           // 13

	{ "b02_12.h20",		0x08000, 0x733b9997, 5 | BRF_GRA },           // 14 Background tiles
	{ "b02_15.h24",		0x08000, 0x8b70ef32, 5 | BRF_GRA },           // 15
	{ "b02_14.h23",		0x08000, 0xf711ba7d, 5 | BRF_GRA },           // 16
	{ "b02_13.h21",		0x08000, 0x62532cd3, 5 | BRF_GRA },           // 17

	{ "b02_08.h13",		0x08000, 0xef0cf49c, 6 | BRF_GRA },           // 18 Foreground tiles
	{ "b02_11.h18",		0x08000, 0xf5799422, 6 | BRF_GRA },           // 19
	{ "b02_10.h16",		0x08000, 0x4bd099ff, 6 | BRF_GRA },           // 20
	{ "b02_09.h15",		0x08000, 0x230f1582, 6 | BRF_GRA },           // 21

	{ "b02_01.d15",		0x10000, 0x2234b424, 7 | BRF_GRA },           // 22 Sprites
	{ "b02_02.d16",		0x10000, 0x30d4c9a8, 7 | BRF_GRA },           // 23
	{ "b02_03.d17",		0x10000, 0x64f3d88f, 7 | BRF_GRA },           // 24
	{ "b02_04.d20",		0x10000, 0x3b23a9fc, 7 | BRF_GRA },           // 25

	{ "clr2.bpr",		0x00100, 0x24e7d62f, 0 | BRF_OPT },           // 26 Proms (not used)
	{ "clr1.bpr",		0x00100, 0xa50cef09, 0 | BRF_OPT },           // 27
	{ "clr3.bpr",		0x00100, 0x016fe2f7, 0 | BRF_OPT },           // 28

	{ "fsb_8741.mcu",	0x00400, 0x00000000, 9 | BRF_NODUMP | BRF_OPT},// 29 MCU
};

STD_ROM_PICK(fsharkbt)
STD_ROM_FN(fsharkbt)

struct BurnDriver BurnDrvFsharkbt = {
	"fsharkbt", "fshark", NULL, NULL, "1987",
	"Flying Shark (bootleg with 8741)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TOAPLAN_MISC, GBF_VERSHOOT, 0,
	NULL, fsharkbtRomInfo, fsharkbtRomName, NULL, NULL, FsharkInputInfo, SkysharkDIPInfo,
	fsharkInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x700,
	240, 320, 3, 4
};


// Flyin' Shark (bootleg of Hishou Zame)

static struct BurnRomInfo fnsharkRomDesc[] = {
	{ "h.ic226",		0x10000, 0xea4bcb43, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "g.ic202",		0x10000, 0xd1f39ed2, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "f.ic170",		0x08000, 0xcdd1a153, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 code

	{ "82s191_r.bin",	0x00800, 0x5b96ae3f, 7 | BRF_PRG | BRF_ESS }, //  3 TMS32010 code
	{ "82s191_l.bin",	0x00800, 0xd5dfc8dd, 7 | BRF_PRG | BRF_ESS }, //  4

	{ "7.ic119",		0x04000, 0xa0f8890d, 3 | BRF_GRA },           //  5 Text characters
	{ "6.ic120",		0x04000, 0xc5bfca95, 3 | BRF_GRA },           //  6
	{ "5.ic121",		0x04000, 0xb8c370bc, 3 | BRF_GRA },           //  7

	{ "b.ic114",		0x08000, 0x733b9997, 4 | BRF_GRA },           //  8 Background tiles
	{ "e.ic111",		0x08000, 0x8b70ef32, 4 | BRF_GRA },           //  9
	{ "d.ic112",		0x08000, 0xf711ba7d, 4 | BRF_GRA },           // 10
	{ "c.ic113",		0x08000, 0x62532cd3, 4 | BRF_GRA },           // 11

	{ "8.ic118",		0x08000, 0xef0cf49c, 5 | BRF_GRA },           // 12 Foreground tiles
	{ "a.ic115",		0x08000, 0xf5799422, 5 | BRF_GRA },           // 13
	{ "10.ic116",		0x08000, 0x4bd099ff, 5 | BRF_GRA },           // 14
	{ "9.ic117",		0x08000, 0x230f1582, 5 | BRF_GRA },           // 15

	{ "1.ic54",		0x10000, 0x2234b424, 6 | BRF_GRA },           // 16 Sprites
	{ "2.ic53",		0x10000, 0x30d4c9a8, 6 | BRF_GRA },           // 17
	{ "3.ic52",		0x10000, 0x64f3d88f, 6 | BRF_GRA },           // 18
	{ "4.ic51",		0x10000, 0x3b23a9fc, 6 | BRF_GRA },           // 19

	{ "82s129.ic41",	0x00100, 0x24e7d62f, 0 | BRF_OPT },           // 20 Proms (not used)
	{ "82s129.ic40",	0x00100, 0xa50cef09, 0 | BRF_OPT },           // 21
	{ "82s123.ic42",	0x00020, 0xf72482db, 0 | BRF_OPT },           // 22
	{ "82s123.ic50",	0x00020, 0xbc88cced, 0 | BRF_OPT },           // 23
	{ "82s123.ic99",	0x00020, 0x4fb5df2a, 0 | BRF_OPT },           // 24
};

STD_ROM_PICK(fnshark)
STD_ROM_FN(fnshark)

static INT32 bootInit()
{
	return DrvInit(3);
}

struct BurnDriver BurnDrvFnshark = {
	"fnshark", "fshark", NULL, NULL, "1987",
	"Flyin' Shark (bootleg of Hishou Zame)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TOAPLAN_MISC, GBF_VERSHOOT, 0,
	NULL, fnsharkRomInfo, fnsharkRomName, NULL, NULL, FsharkInputInfo, HishouzaDIPInfo,
	bootInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x700,
	240, 320, 3, 4
};


// Gulf War II (set 1)

static struct BurnRomInfo gulfwar2RomDesc[] = {
	{ "08-u119.bin",	0x20000, 0x41ebf9c0, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "07-u92.bin",		0x20000, 0xb73e6b25, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "06-u51.bin",		0x08000, 0x75504f95, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 code

	{ "02-u1.rom",		0x02000, 0xabefe4ca, 3 | BRF_PRG | BRF_ESS }, //  3 TMS32010 code
	{ "01-u2.rom",		0x02000, 0x01399b65, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "03-u9.bin",		0x04000, 0x1b7934b3, 4 | BRF_GRA },           //  5 Text characters
	{ "04-u10.bin",		0x04000, 0x6f7bfb58, 4 | BRF_GRA },           //  6
	{ "05-u11.bin",		0x04000, 0x31814724, 4 | BRF_GRA },           //  7

	{ "16-u202.bin",	0x10000, 0xd815d175, 5 | BRF_GRA },           //  8 Background tiles
	{ "13-u199.bin",	0x10000, 0xd949b0d9, 5 | BRF_GRA },           //  9
	{ "14-u200.bin",	0x10000, 0xc109a6ac, 5 | BRF_GRA },           // 10
	{ "15-u201.bin",	0x10000, 0xad21f2ab, 5 | BRF_GRA },           // 11

	{ "09-u195.bin",	0x08000, 0xb7be3a6d, 6 | BRF_GRA },           // 12 Foreground tiles
	{ "12-u198.bin",	0x08000, 0xfd7032a6, 6 | BRF_GRA },           // 13
	{ "11-u197.bin",	0x08000, 0x7b721ed3, 6 | BRF_GRA },           // 14
	{ "10-u196.rom",	0x08000, 0x160f38ab, 6 | BRF_GRA },           // 15

	{ "20-u262.bin",	0x10000, 0x10665ca0, 7 | BRF_GRA },           // 16 Sprites
	{ "19-u261.bin",	0x10000, 0xcfa6d417, 7 | BRF_GRA },           // 17
	{ "18-u260.bin",	0x10000, 0x2e6a0c49, 7 | BRF_GRA },           // 18
	{ "17-u259.bin",	0x10000, 0x66c1b0e6, 7 | BRF_GRA },           // 19

	{ "82s129.d3",		0x00100, 0x24e7d62f, 0 | BRF_OPT },           // 20 Proms (not used)
	{ "82s129.d4",		0x00100, 0xa50cef09, 0 | BRF_OPT },           // 21
	{ "82s123.d2",		0x00020, 0xf72482db, 0 | BRF_OPT },           // 22
	{ "82s123.e18",		0x00020, 0xbc88cced, 0 | BRF_OPT },           // 23
	{ "82s123.b24",		0x00020, 0x4fb5df2a, 0 | BRF_OPT },           // 24
};

STD_ROM_PICK(gulfwar2)
STD_ROM_FN(gulfwar2)

static INT32 gulfwar2Init()
{
	return DrvInit(2);
}

struct BurnDriver BurnDrvGulfwar2 = {
	"gulfwar2", NULL, NULL, NULL, "1991",
	"Gulf War II (set 1)\0", NULL, "Comad", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TOAPLAN_MISC, GBF_VERSHOOT, 0,
	NULL, gulfwar2RomInfo, gulfwar2RomName, NULL, NULL, TwincobrInputInfo, TwincobrDIPInfo,
	gulfwar2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x700,
	240, 320, 3, 4
};


// Gulf War II (set 2)

static struct BurnRomInfo gulfwar2aRomDesc[] = {
	{ "gw2_28.u119",	0x20000, 0xb9118660, 1 | BRF_PRG | BRF_ESS }, //  0 68k code
	{ "gw2_27.u92",		0x20000, 0x3494f1aa, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "06-u51.bin",		0x08000, 0x75504f95, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 code

	{ "gw2_22.udsp1",	0x01000, 0x3a97b0db, 3 | BRF_PRG | BRF_ESS }, //  3 TMS32010 code
	{ "gw2_21.udsp2",	0x01000, 0x87a473af, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "gw2_23.u9",		0x08000, 0xa2aee4c8, 4 | BRF_GRA },           //  5 Text characters
	{ "gw2_24.u10",		0x08000, 0xfb3f71cd, 4 | BRF_GRA },           //  6
	{ "gw2_25.u11",		0x08000, 0x90eeb0a0, 4 | BRF_GRA },           //  7

	{ "16-u202.bin",	0x10000, 0xd815d175, 5 | BRF_GRA },           //  8 Background tiles
	{ "13-u199.bin",	0x10000, 0xd949b0d9, 5 | BRF_GRA },           //  9
	{ "14-u200.bin",	0x10000, 0xc109a6ac, 5 | BRF_GRA },           // 10
	{ "15-u201.bin",	0x10000, 0xad21f2ab, 5 | BRF_GRA },           // 11

	{ "09-u195.bin",	0x08000, 0xb7be3a6d, 6 | BRF_GRA },           // 12 Foreground tiles
	{ "12-u198.bin",	0x08000, 0xfd7032a6, 6 | BRF_GRA },           // 13
	{ "11-u197.bin",	0x08000, 0x7b721ed3, 6 | BRF_GRA },           // 14
	{ "10-u196.rom",	0x08000, 0x160f38ab, 6 | BRF_GRA },           // 15

	{ "20-u262.bin",	0x10000, 0x10665ca0, 7 | BRF_GRA },           // 16 Sprites
	{ "19-u261.bin",	0x10000, 0xcfa6d417, 7 | BRF_GRA },           // 17
	{ "18-u260.bin",	0x10000, 0x2e6a0c49, 7 | BRF_GRA },           // 18
	{ "17-u259.bin",	0x10000, 0x66c1b0e6, 7 | BRF_GRA },           // 19

	{ "82s129.d3",		0x00100, 0x24e7d62f, 0 | BRF_OPT },           // 20 Proms (not used)
	{ "82s129.d4",		0x00100, 0xa50cef09, 0 | BRF_OPT },           // 21
	{ "82s123.d2",		0x00020, 0xf72482db, 0 | BRF_OPT },           // 22
	{ "82s123.e18",		0x00020, 0xbc88cced, 0 | BRF_OPT },           // 23
	{ "82s123.b24",		0x00020, 0x4fb5df2a, 0 | BRF_OPT },           // 24
};

STD_ROM_PICK(gulfwar2a)
STD_ROM_FN(gulfwar2a)

struct BurnDriver BurnDrvGulfwar2a = {
	"gulfwar2a", "gulfwar2", NULL, NULL, "1991",
	"Gulf War II (set 2)\0", NULL, "Comad", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TOAPLAN_MISC, GBF_VERSHOOT, 0,
	NULL, gulfwar2aRomInfo, gulfwar2aRomName, NULL, NULL, TwincobrInputInfo, TwincobrDIPInfo,
	gulfwar2Init, DrvExit, DrvFrame, DrvDraw, NULL, &DrvRecalc, 0x700,
	240, 320, 3, 4
};
