// FB Alpha Toaplan driver module
// Based on MAME driver by Darren Olafson, Quench, and Stephane Humbert

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "burn_ym3812.h"
#include "tms32010.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvMCUROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvPalRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvShareRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprSizeRAM;
static UINT8 *DrvSprBuf;
static UINT8 *DrvSprSizeBuf;
static UINT8 *Drv68KRAM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvMCURAM;
static UINT8 *DrvTransTable;

static UINT16 *scroll;

static UINT16 *pTempDraw;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT16 sprite_flipscreen;
static UINT8 flipscreen;
static UINT8 interrupt_enable;
static UINT16 tiles_offsets_x;
static UINT16 tiles_offsets_y;
static UINT16 tileram_offs;
static UINT16 spriteram_offset;
static UINT8 soundlatch;
static UINT8 mcu_command;

static INT32 m68k_halt;
static UINT32 main_ram_seg;
static UINT16 dsp_addr_w;
static INT32 dsp_execute;
static INT32 dsp_BIO;
static INT32 dsp_on;

static INT32 tile_mask;
static INT32 sprite_mask;
static INT32 mainrom_size;
static INT32 sprite_y_adjust;
static INT32 vertical_lines;
static INT32 samesame = 0;
static INT32 vblank;
static INT32 has_dsp = 0;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[4];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo Drv2bInputList[] = {
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
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 0,	"service"	},
	{"Test SW",		BIT_DIGITAL,	DrvJoy3 + 2,	"diag"		},
	{"Tilt",		BIT_DIGITAL,	DrvJoy3 + 1,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Drv2b)

static struct BurnInputInfo Drv3bInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 0,	"service"	},
	{"Test SW",		BIT_DIGITAL,	DrvJoy3 + 2,	"diag"		},
	{"Tilt",		BIT_DIGITAL,	DrvJoy3 + 1,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Drv3b)

static struct BurnInputInfo FiresharkInputList[] = {
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
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Fireshark)

static struct BurnDIPInfo RallybikDIPList[]=
{
	{0x14, 0xff, 0xff, 0x01, NULL			},
	{0x15, 0xff, 0xff, 0x20, NULL			},
	{0x16, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x14, 0x01, 0x01, 0x01, "Upright"		},
	{0x14, 0x01, 0x01, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x14, 0x01, 0x02, 0x00, "Off"			},
	{0x14, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x04, 0x00, "Off"			},
	{0x14, 0x01, 0x04, 0x04, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x14, 0x01, 0x08, 0x08, "Off"			},
	{0x14, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"			},
	{0x14, 0x01, 0x30, 0x00, "1 Coin  1 Credit"		},  // a/b ok.
	{0x14, 0x01, 0x30, 0x10, "2 Coins 1 Credit"		},
	{0x14, 0x01, 0x30, 0x20, "3 Coins 1 Credit"		},
	{0x14, 0x01, 0x30, 0x30, "4 Coins 1 Credit"		},

	{0   , 0xfe, 0   ,    4, "Coin B"			},
	{0x14, 0x01, 0xc0, 0x00, "1 Coin  2 Credits"		},
	{0x14, 0x01, 0xc0, 0x40, "1 Coin  3 Credits"		},
	{0x14, 0x01, 0xc0, 0x80, "1 Coin  4 Credits"		},
	{0x14, 0x01, 0xc0, 0xc0, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x15, 0x01, 0x03, 0x01, "Easy"			},
	{0x15, 0x01, 0x03, 0x00, "Normal"		},
	{0x15, 0x01, 0x03, 0x02, "Hard"			},
	{0x15, 0x01, 0x03, 0x03, "Very Hard"		},

	{0   , 0xfe, 0   ,    4, "Territory/Copyright"		},
	{0x15, 0x01, 0x30, 0x20, "World/Taito Corp Japan"	},
	{0x15, 0x01, 0x30, 0x10, "USA/Taito America"		},
	{0x15, 0x01, 0x30, 0x00, "Japan/Taito Corp"		},
	{0x15, 0x01, 0x30, 0x30, "USA/Taito America (Romstar)"	},

	{0   , 0xfe, 0   ,    2, "Dip Switch Display"	},
	{0x15, 0x01, 0x40, 0x00, "Off"			},
	{0x15, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x15, 0x01, 0x80, 0x80, "No"			},
	{0x15, 0x01, 0x80, 0x00, "Yes"			},
};

STDDIPINFO(Rallybik)

static struct BurnDIPInfo TruxtonDIPList[]=
{
	{0x14, 0xff, 0xff, 0x01, NULL			},
	{0x15, 0xff, 0xff, 0x00, NULL			},
	{0x16, 0xff, 0xff, 0xf2, NULL			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x14, 0x01, 0x01, 0x01, "Upright"		},
	{0x14, 0x01, 0x01, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x14, 0x01, 0x02, 0x00, "Off"			},
	{0x14, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x04, 0x00, "Off"			},
	{0x14, 0x01, 0x04, 0x04, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x14, 0x01, 0x08, 0x08, "Off"			},
	{0x14, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x14, 0x01, 0x30, 0x00, "1 Coin  1 Credit"	},  // ok a/b
	{0x14, 0x01, 0x30, 0x10, "2 Coins 1 Credit"	},
	{0x14, 0x01, 0x30, 0x20, "3 Coins 1 Credit"	},
	{0x14, 0x01, 0x30, 0x30, "4 Coins 1 Credit"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x14, 0x01, 0xc0, 0x00, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0xc0, 0x40, "1 Coin  3 Credits"	},
	{0x14, 0x01, 0xc0, 0x80, "1 Coin  4 Credits"	},
	{0x14, 0x01, 0xc0, 0xc0, "1 Coins 6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x15, 0x01, 0x03, 0x01, "Easy"			},
	{0x15, 0x01, 0x03, 0x00, "Normal"		},
	{0x15, 0x01, 0x03, 0x02, "Hard"			},
	{0x15, 0x01, 0x03, 0x03, "Very Hard"		},

	{0   , 0xfe, 0   ,	  4, "Extend bonus"},
	{0x15, 0x01, 0x03, 0x04, "50k 200k 150k+"	},
	{0x15, 0x01, 0x03, 0x00, "70k 270k 200k+"	},
	{0x15, 0x01, 0x03, 0x08, "100k Only"		},
	{0x15, 0x01, 0x03, 0x0c, "None"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x15, 0x01, 0x30, 0x30, "2"			},
	{0x15, 0x01, 0x30, 0x00, "3"			},
	{0x15, 0x01, 0x30, 0x20, "4"			},
	{0x15, 0x01, 0x30, 0x10, "5"			},

	{0   , 0xfe, 0   ,    2, "Dip Switch Display"	},
	{0x15, 0x01, 0x40, 0x00, "Off"			},
	{0x15, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x15, 0x01, 0x80, 0x80, "No"			},
	{0x15, 0x01, 0x80, 0x00, "Yes"			},

	{0   , 0xfe, 0   ,    5, "Region"		},
    {0x16, 0x01, 0x07, 0x00, "Japan/Taito Corp"},
    {0x16, 0x01, 0x07, 0x01, "US/Romstar"},
    {0x16, 0x01, 0x07, 0x02, "World/Taito Corp"},
    {0x16, 0x01, 0x07, 0x04, "US/Taito America"},
	{0x16, 0x01, 0x07, 0x06, "World/Taito America"},
};

STDDIPINFO(Truxton)

static struct BurnDIPInfo HellfireDIPList[]=
{
	{0x14, 0xff, 0xff, 0x00, NULL			},
	{0x15, 0xff, 0xff, 0x00, NULL			},
	{0x16, 0xff, 0xff, 0xf2, NULL			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x14, 0x01, 0x02, 0x00, "Off"			},
	{0x14, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x04, 0x00, "Off"			},
	{0x14, 0x01, 0x04, 0x04, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x14, 0x01, 0x08, 0x08, "Off"			},
	{0x14, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x14, 0x01, 0x30, 0x00, "1 Coin  1 Credit"	}, // ok a,b
	{0x14, 0x01, 0x30, 0x10, "2 Coins 1 Credit"	},
	{0x14, 0x01, 0x30, 0x20, "3 Coins 1 Credit"	},
	{0x14, 0x01, 0x30, 0x30, "4 Coins 1 Credit"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x14, 0x01, 0xc0, 0x00, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0xc0, 0x40, "1 Coin  3 Credits"	},
	{0x14, 0x01, 0xc0, 0x80, "1 Coin  4 Credits"	},
	{0x14, 0x01, 0xc0, 0xc0, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x15, 0x01, 0x03, 0x01, "Easy"			},
	{0x15, 0x01, 0x03, 0x00, "Normal"		},
	{0x15, 0x01, 0x03, 0x02, "Hard"			},
	{0x15, 0x01, 0x03, 0x03, "Very Hard"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x15, 0x01, 0x03, 0x00, "70k 270k 200k+"	},
	{0x15, 0x01, 0x03, 0x04, "100k 350k 250k+"	},
	{0x15, 0x01, 0x03, 0x08, "100k Only"		},
	{0x15, 0x01, 0x03, 0x0c, "200k Only"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x15, 0x01, 0x30, 0x30, "2"			},
	{0x15, 0x01, 0x30, 0x00, "3"			},
	{0x15, 0x01, 0x30, 0x20, "4"			},
	{0x15, 0x01, 0x30, 0x10, "5"			},

	{0   , 0xfe, 0   ,    2, "Invulnerability"	},
	{0x15, 0x01, 0x40, 0x00, "Off"			},
	{0x15, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    3, "Region"		},
	{0x16, 0x01, 0x03, 0x02, "Europe"		},
	{0x16, 0x01, 0x03, 0x01, "USA"			},
	{0x16, 0x01, 0x03, 0x00, "Japan"		},
};

STDDIPINFO(Hellfire)

static struct BurnDIPInfo Hellfire1DIPList[] = {
	{0x14, 0xff, 0xff, 0x01, NULL			},
	{0x15, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x14, 0x01, 0x01, 0x01, "Upright"		},
	{0x14, 0x01, 0x01, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x15, 0x01, 0x40, 0x00, "Off"			},
	{0x15, 0x01, 0x40, 0x40, "On"			},
};

STDDIPINFOEXT(Hellfire1, Hellfire,  Hellfire1)

static struct BurnDIPInfo Hellfire2aDIPList[] = {
	{0x14, 0xff, 0xff, 0x01, NULL			},
	{0x15, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    2, "Dip switch display"	},
	{0x15, 0x01, 0x40, 0x00, "Off"			},
	{0x15, 0x01, 0x40, 0x40, "On"			},
};

STDDIPINFOEXT(Hellfire2a, Hellfire,  Hellfire2a)

static struct BurnDIPInfo Hellfire1aDIPList[] = {
	{0x14, 0xff, 0xff, 0x01, NULL			},
	{0x15, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    2, "Dip switch display"	},
	{0x15, 0x01, 0x40, 0x00, "Off"			},
	{0x15, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x15, 0x01, 0x80, 0x00, "Off"			},
	{0x15, 0x01, 0x80, 0x80, "On"			},
};

STDDIPINFOEXT(Hellfire1a, Hellfire,  Hellfire1a)

static struct BurnDIPInfo ZerowingDIPList[]=
{
	{0x14, 0xff, 0xff, 0x01, NULL			},
	{0x15, 0xff, 0xff, 0x00, NULL			},
	{0x16, 0xff, 0xff, 0xf3, NULL			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x14, 0x01, 0x01, 0x01, "Upright"		},
	{0x14, 0x01, 0x01, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x14, 0x01, 0x02, 0x00, "Off"			},
	{0x14, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x04, 0x00, "Off"			},
	{0x14, 0x01, 0x04, 0x04, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x14, 0x01, 0x08, 0x08, "Off"			},
	{0x14, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x14, 0x01, 0x30, 0x00, "1 Coin  1 Credit"	},  // ok a,b
	{0x14, 0x01, 0x30, 0x10, "1 Coins 2 Credits"	},
	{0x14, 0x01, 0x30, 0x20, "2 Coins 1 Credit"	},
	{0x14, 0x01, 0x30, 0x30, "2 Coins 3 Credits"	},

	{0   , 0xfe, 0   ,    3, "Coin B"		},
	{0x14, 0x01, 0xc0, 0x00, "1 Coin  1 Credit"	},
	{0x14, 0x01, 0xc0, 0x40, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0xc0, 0x80, "2 Coins 1 Credit"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x15, 0x01, 0x03, 0x01, "Easy"			},
	{0x15, 0x01, 0x03, 0x00, "Normal"		},
	{0x15, 0x01, 0x03, 0x02, "Hard"			},
	{0x15, 0x01, 0x03, 0x03, "Very Hard"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x15, 0x01, 0x03, 0x00, "200k 700k 500k+"	},
	{0x15, 0x01, 0x03, 0x04, "500k 1500k 1000k+"	},
	{0x15, 0x01, 0x03, 0x08, "500k Only"		},
	{0x15, 0x01, 0x03, 0x0c, "None"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x15, 0x01, 0x30, 0x30, "2"			},
	{0x15, 0x01, 0x30, 0x00, "3"			},
	{0x15, 0x01, 0x30, 0x20, "4"			},
	{0x15, 0x01, 0x30, 0x10, "5"			},

	{0   , 0xfe, 0   ,    2, "Invulnerability"	},
	{0x15, 0x01, 0x40, 0x00, "Off"			},
	{0x15, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x15, 0x01, 0x80, 0x80, "No"			},
	{0x15, 0x01, 0x80, 0x00, "Yes"			},

	{0   , 0xfe, 0   ,    3, "Region"		},
	{0x16, 0x01, 0x03, 0x03, "Europe"		},
	{0x16, 0x01, 0x03, 0x01, "USA"			},
	{0x16, 0x01, 0x03, 0x00, "Japan"		},
};

STDDIPINFO(Zerowing)

static struct BurnDIPInfo Zerowing2DIPList[] = {
	{0x14, 0xff, 0xff, 0x00, NULL			},
	{0x15, 0xff, 0xff, 0x00, NULL			},
	{0x16, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x14, 0x01, 0x01, 0x01, "On"			},
	{0x14, 0x01, 0x01, 0x00, "Off"			},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x15, 0x01, 0x80, 0x00, "Off"			},
	{0x15, 0x01, 0x80, 0x80, "On"			},

	{0   , 0xfe, 0   ,    2, "Region"		},
	{0x16, 0x01, 0x03, 0x02, "Europe"		},
	{0x16, 0x01, 0x03, 0x00, "USA"			},
};

STDDIPINFOEXT(Zerowing2, Zerowing,  Zerowing2)

static struct BurnDIPInfo VimanaDIPList[]=
{
	{0x14, 0xff, 0xff, 0x01, NULL			},
	{0x15, 0xff, 0xff, 0x00, NULL			},
	{0x16, 0xff, 0xff, 0x02, NULL			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x14, 0x01, 0x01, 0x01, "Upright"		},
	{0x14, 0x01, 0x01, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x14, 0x01, 0x02, 0x00, "Off"			},
	{0x14, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x04, 0x00, "Off"			},
	{0x14, 0x01, 0x04, 0x04, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x14, 0x01, 0x08, 0x08, "Off"			},
	{0x14, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x14, 0x01, 0x30, 0x00, "1 Coin  1 Credit"	},
	{0x14, 0x01, 0x30, 0x10, "2 Coins 1 Credit"	},
	{0x14, 0x01, 0x30, 0x20, "3 Coins 1 Credit"	},
	{0x14, 0x01, 0x30, 0x30, "4 Coins 1 Credit"	},  // a/b good.

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x14, 0x01, 0xc0, 0x00, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0xc0, 0x40, "1 Coin  3 Credits"	},
	{0x14, 0x01, 0xc0, 0x80, "1 Coin  4 Credits"	},
	{0x14, 0x01, 0xc0, 0xc0, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x15, 0x01, 0x03, 0x01, "Easy"			},
	{0x15, 0x01, 0x03, 0x00, "Normal"		},
	{0x15, 0x01, 0x03, 0x02, "Hard"			},
	{0x15, 0x01, 0x03, 0x03, "Very Hard"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x15, 0x01, 0x03, 0x00, "70k 270k 200k+"	},
	{0x15, 0x01, 0x03, 0x04, "100k 350k 250k+"	},
	{0x15, 0x01, 0x03, 0x08, "100k Only"		},
	{0x15, 0x01, 0x03, 0x0c, "200k Only"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x15, 0x01, 0x30, 0x30, "2"			},
	{0x15, 0x01, 0x30, 0x00, "3"			},
	{0x15, 0x01, 0x30, 0x20, "4"			},
	{0x15, 0x01, 0x30, 0x10, "5"			},

	{0   , 0xfe, 0   ,    2, "Invulnerability"	},
	{0x15, 0x01, 0x40, 0x00, "Off"			},
	{0x15, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x15, 0x01, 0x80, 0x80, "No"			},
	{0x15, 0x01, 0x80, 0x00, "Yes"			},

	{0   , 0xfe, 0   ,    8, "Region"		},
	{0x16, 0x01, 0x0f, 0x02, "Europe"		},
	{0x16, 0x01, 0x0f, 0x01, "USA"			},
	{0x16, 0x01, 0x0f, 0x07, "USA (Romstar license)"},
	{0x16, 0x01, 0x0f, 0x04, "Korea"		},
	{0x16, 0x01, 0x0f, 0x03, "Hong Kong"		},
	{0x16, 0x01, 0x0f, 0x08, "Hong Kong (Honest Trading license)"	},
	{0x16, 0x01, 0x0f, 0x05, "Taiwan"		},
	{0x16, 0x01, 0x0f, 0x06, "Taiwan (Spacy license)"},
};

STDDIPINFO(Vimana)

static struct BurnDIPInfo VimananDIPList[] = {
	{0x16, 0xff, 0xff, 0x02, NULL			},

	{0   , 0xfe, 0   ,    8, "Region"		},
	{0x16, 0x01, 0x0f, 0x02, "Europe (Nova Apparate license)" },
	{0x16, 0x01, 0x0f, 0x01, "USA"			},
	{0x16, 0x01, 0x0f, 0x07, "USA (Romstar license)"},
	{0x16, 0x01, 0x0f, 0x04, "Korea"		},
	{0x16, 0x01, 0x0f, 0x03, "Hong Kong"		},
	{0x16, 0x01, 0x0f, 0x08, "Hong Kong (Honest Trading license)" },
	{0x16, 0x01, 0x0f, 0x05, "Taiwan"		},
	{0x16, 0x01, 0x0f, 0x06, "Taiwan (Spacy license)"},
};

STDDIPINFOEXT(Vimanan, Vimana,  Vimanan)

static struct BurnDIPInfo VimanajDIPList[] = {
	{0x16, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    8, "Region"		},
	{0x16, 0x01, 0x0f, 0x00, "Japan (distributed by Tecmo)" },
};

STDDIPINFOEXT(Vimanaj, Vimana,  Vimanaj)

static struct BurnDIPInfo DemonwldDIPList[]=
{
	{0x16, 0xff, 0xff, 0x00, NULL			},
	{0x17, 0xff, 0xff, 0x00, NULL			},
	{0x18, 0xff, 0xff, 0x01, NULL			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x16, 0x01, 0x02, 0x00, "Off"			},
	{0x16, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x16, 0x01, 0x04, 0x00, "Off"			},
	{0x16, 0x01, 0x04, 0x04, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x16, 0x01, 0x08, 0x08, "Off"			},
	{0x16, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x16, 0x01, 0x30, 0x00, "1 Coin  1 Credit"	},     // ok a,b
	{0x16, 0x01, 0x30, 0x10, "1 Coin  2 Credits"	},
	{0x16, 0x01, 0x30, 0x20, "2 Coins 1 Credit"	},
	{0x16, 0x01, 0x30, 0x30, "2 Coins 3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x16, 0x01, 0xc0, 0x00, "1 Coin  1 Credit"	},
	{0x16, 0x01, 0xc0, 0x40, "1 Coin  2 Credits"	},
	{0x16, 0x01, 0xc0, 0x80, "2 Coins 1 Credit"	},
	{0x16, 0x01, 0xc0, 0xc0, "2 Coins 3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x17, 0x01, 0x03, 0x01, "Easy"			},
	{0x17, 0x01, 0x03, 0x00, "Medium"		},
	{0x17, 0x01, 0x03, 0x02, "Hard"			},
	{0x17, 0x01, 0x03, 0x03, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x17, 0x01, 0x0c, 0x00, "30K, every 100K"	},
	{0x17, 0x01, 0x0c, 0x04, "50K and 100K"		},
	{0x17, 0x01, 0x0c, 0x08, "100K only"		},
	{0x17, 0x01, 0x0c, 0x0c, "None"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x17, 0x01, 0x30, 0x30, "1"			},
	{0x17, 0x01, 0x30, 0x20, "2"			},
	{0x17, 0x01, 0x30, 0x00, "3"			},
	{0x17, 0x01, 0x30, 0x10, "5"			},

	{0   , 0xfe, 0   ,    2, "Invulnerability"	},
	{0x17, 0x01, 0x40, 0x00, "Off"			},
	{0x17, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Territory/Copyright"	},
	{0x18, 0x01, 0x01, 0x01, "Toaplan"		},
	{0x18, 0x01, 0x01, 0x00, "Japan/Taito Corp"	},
};

STDDIPINFO(Demonwld)

static struct BurnDIPInfo Demonwld1DIPList[] = {
	{0x18, 0xff, 0xff, 0x02, NULL			},

	{0   , 0xfe, 0   ,    4, "Territory/Copyright"	},
	{0x18, 0x01, 0x03, 0x02, "World/Taito Japan"	},
	{0x18, 0x01, 0x03, 0x03, "US/Toaplan"		},
	{0x18, 0x01, 0x03, 0x01, "US/Taito America"	},
	{0x18, 0x01, 0x03, 0x00, "Japan/Taito Corp"	},
};

STDDIPINFOEXT(Demonwld1, Demonwld, Demonwld1)

static struct BurnDIPInfo OutzoneDIPList[]=
{
	{0x16, 0xff, 0xff, 0x00, NULL			},
	{0x17, 0xff, 0xff, 0x00, NULL			},
	{0x18, 0xff, 0xff, 0x02, NULL			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x16, 0x01, 0x02, 0x00, "Off"			},
	{0x16, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x16, 0x01, 0x04, 0x00, "Off"			},
	{0x16, 0x01, 0x04, 0x04, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x16, 0x01, 0x08, 0x08, "Off"			},
	{0x16, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x16, 0x01, 0x30, 0x00, "1 Coin  1 Credit"	},
	{0x16, 0x01, 0x30, 0x10, "2 Coins 1 Credit"	},   // ok a,b
	{0x16, 0x01, 0x30, 0x20, "3 Coins 1 Credit"	},
	{0x16, 0x01, 0x30, 0x30, "4 Coins 1 Credit"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x16, 0x01, 0xc0, 0x00, "1 Coin  2 Credits"	},
	{0x16, 0x01, 0xc0, 0x40, "1 Coin  3 Credits"	},
	{0x16, 0x01, 0xc0, 0x80, "1 Coin  4 Credits"	},
	{0x16, 0x01, 0xc0, 0xc0, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x17, 0x01, 0x03, 0x01, "Easy"			},
	{0x17, 0x01, 0x03, 0x00, "Normal"		},
	{0x17, 0x01, 0x03, 0x02, "Hard"			},
	{0x17, 0x01, 0x03, 0x03, "Very Hard"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x17, 0x01, 0x0c, 0x00, "Every 300k"		},
	{0x17, 0x01, 0x0c, 0x04, "200k and 500k"	},
	{0x17, 0x01, 0x0c, 0x08, "300k Only"		},
	{0x17, 0x01, 0x0c, 0x0c, "None"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x17, 0x01, 0x30, 0x30, "1"			},
	{0x17, 0x01, 0x30, 0x20, "2"			},
	{0x17, 0x01, 0x30, 0x00, "3"			},
	{0x17, 0x01, 0x30, 0x10, "5"			},

	{0   , 0xfe, 0   ,    2, "Invulnerability"	},
	{0x17, 0x01, 0x40, 0x00, "Off"			},
	{0x17, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    0, "Region"		},
	{0x18, 0x01, 0x0f, 0x00, "Japan"		},
	{0x18, 0x01, 0x0f, 0x01, "USA"			},
	{0x18, 0x01, 0x0f, 0x02, "Europe"		},
	{0x18, 0x01, 0x0f, 0x03, "Hong Kong"		},
	{0x18, 0x01, 0x0f, 0x04, "Korea"		},
	{0x18, 0x01, 0x0f, 0x05, "Taiwan"		},
	{0x18, 0x01, 0x0f, 0x06, "Taiwan (Spacy Co., Ltd.)"	},
	{0x18, 0x01, 0x0f, 0x07, "USA (Romstar, Inc.)"		},
	{0x18, 0x01, 0x0f, 0x08, "Hong Kong & China (Honest Trading Co.)"	},
};

STDDIPINFO(Outzone)

static struct BurnDIPInfo OutzoneaDIPList[] = {
	{0x18, 0xff, 0xff, 0x02, NULL			},

	{0   , 0xfe, 0   ,    4, "Territory/Copyright"	},
	{0x18, 0x01, 0x03, 0x02, "World/Taito Japan"	},
	{0x18, 0x01, 0x03, 0x03, "US/Toaplan"		},
	{0x18, 0x01, 0x03, 0x01, "US/Taito America"	},
	{0x18, 0x01, 0x03, 0x00, "Japan/Taito Corp"	},
};

STDDIPINFOEXT(Outzonea, Outzone, Outzonea)

static struct BurnDIPInfo OutzonecDIPList[] = {
	{0x18, 0xff, 0xff, 0x02, NULL			},

	{0   , 0xfe, 0   ,    0, "Region"		},
	{0x18, 0x01, 0x07, 0x00, "Japan"		},
	{0x18, 0x01, 0x07, 0x01, "USA"			},
	{0x18, 0x01, 0x07, 0x02, "Europe"		},
	{0x18, 0x01, 0x07, 0x03, "Hong Kong"		},
	{0x18, 0x01, 0x07, 0x04, "Korea"		},
	{0x18, 0x01, 0x07, 0x05, "Taiwan"		},
	{0x18, 0x01, 0x07, 0x06, "World"		},
};

STDDIPINFOEXT(Outzonec, Outzone, Outzonec)

static struct BurnDIPInfo FireshrkDIPList[]=
{
	{0x13, 0xff, 0xff, 0x01, NULL			},
	{0x14, 0xff, 0xff, 0x00, NULL			},
	{0x15, 0xff, 0xff, 0x02, NULL			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x13, 0x01, 0x01, 0x01, "Upright"		},
	{0x13, 0x01, 0x01, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x02, 0x00, "Off"			},
	{0x13, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x08, 0x08, "Off"			},
	{0x13, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x13, 0x01, 0x30, 0x00, "1 Coin  1 Credits"	},  // good a,b
	{0x13, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x20, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x30, "4 Coins 1 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x13, 0x01, 0xc0, 0x00, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xc0, 0x40, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0xc0, 0x80, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0xc0, 0xc0, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x03, 0x01, "Easy"			},
	{0x14, 0x01, 0x03, 0x00, "Medium"		},
	{0x14, 0x01, 0x03, 0x02, "Hard"			},
	{0x14, 0x01, 0x03, 0x03, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x14, 0x01, 0x0c, 0x04, "50K, every 150K"	},
	{0x14, 0x01, 0x0c, 0x00, "70K, every 200K"	},
	{0x14, 0x01, 0x0c, 0x08, "100K"			},
	{0x14, 0x01, 0x0c, 0x0c, "None"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0x30, 0x30, "2"			},
	{0x14, 0x01, 0x30, 0x00, "3"			},
	{0x14, 0x01, 0x30, 0x20, "4"			},
	{0x14, 0x01, 0x30, 0x10, "5"			},

	{0   , 0xfe, 0   ,    2, "Invulnerability"	},
	{0x14, 0x01, 0x40, 0x00, "Off"			},
	{0x14, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x14, 0x01, 0x80, 0x80, "No"			},
	{0x14, 0x01, 0x80, 0x00, "Yes"			},

	{0   , 0xfe, 0   ,    3, "Region"		},
	{0x15, 0x01, 0x07, 0x02, "Europe"		},
	{0x15, 0x01, 0x07, 0x04, "USA"			},
	{0x15, 0x01, 0x07, 0x00, "USA (Romstar)"	},
};

STDDIPINFO(Fireshrk)

static struct BurnDIPInfo FireshrkaDIPList[] = {
	{0x15, 0xff, 0xff, 0x03, NULL			},

	{0   , 0xfe, 0   ,    2, "Region"		},
	{0x15, 0x01, 0x07, 0x03, "Europe"		},
	{0x15, 0x01, 0x07, 0x00, "USA"			},
};

STDDIPINFOEXT(Fireshrka, Fireshrk, Fireshrka)

static struct BurnDIPInfo SamesameDIPList[] = {
	{0x13, 0xff, 0xff, 0x01, NULL			},
	{0x14, 0xff, 0xff, 0x00, NULL			},
	{0x15, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x13, 0x01, 0x01, 0x01, "Upright"		},
	{0x13, 0x01, 0x01, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x02, 0x00, "Off"			},
	{0x13, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x08, 0x08, "Off"			},
	{0x13, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x03, 0x01, "Easy"			},
	{0x14, 0x01, 0x03, 0x00, "Medium"		},
	{0x14, 0x01, 0x03, 0x02, "Hard"			},
	{0x14, 0x01, 0x03, 0x03, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x14, 0x01, 0x0c, 0x04, "50K, every 150K"	},
	{0x14, 0x01, 0x0c, 0x00, "70K, every 200K"	},
	{0x14, 0x01, 0x0c, 0x08, "100K"			},
	{0x14, 0x01, 0x0c, 0x0c, "None"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0x30, 0x30, "2"			},
	{0x14, 0x01, 0x30, 0x00, "3"			},
	{0x14, 0x01, 0x30, 0x20, "4"			},
	{0x14, 0x01, 0x30, 0x10, "5"			},

	{0   , 0xfe, 0   ,    2, "Invulnerability"	},
	{0x14, 0x01, 0x40, 0x00, "Off"			},
	{0x14, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x14, 0x01, 0x80, 0x80, "No"			},
	{0x14, 0x01, 0x80, 0x00, "Yes"			},
};

STDDIPINFO(Samesame)

static struct BurnDIPInfo Samesame2DIPList[]=
{
	{0x13, 0xff, 0xff, 0x00, NULL			},
	{0x14, 0xff, 0xff, 0x00, NULL			},
	{0x15, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x02, 0x00, "Off"			},
	{0x13, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x08, 0x08, "Off"			},
	{0x13, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x03, 0x01, "Easy"			},
	{0x14, 0x01, 0x03, 0x00, "Medium"		},
	{0x14, 0x01, 0x03, 0x02, "Hard"			},
	{0x14, 0x01, 0x03, 0x03, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x14, 0x01, 0x0c, 0x04, "50K, every 150K"	},
	{0x14, 0x01, 0x0c, 0x00, "70K, every 200K"	},
	{0x14, 0x01, 0x0c, 0x08, "100K"			},
	{0x14, 0x01, 0x0c, 0x0c, "None"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0x30, 0x30, "2"			},
	{0x14, 0x01, 0x30, 0x00, "3"			},
	{0x14, 0x01, 0x30, 0x20, "4"			},
	{0x14, 0x01, 0x30, 0x10, "5"			},

	{0   , 0xfe, 0   ,    2, "Invulnerability"	},
	{0x14, 0x01, 0x40, 0x00, "Off"			},
	{0x14, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x14, 0x01, 0x80, 0x80, "No"			},
	{0x14, 0x01, 0x80, 0x00, "Yes"			},

	{0   , 0xfe, 0   ,    2, "Show Territory Notice"},
	{0x15, 0x01, 0x01, 0x01, "No"			},
	{0x15, 0x01, 0x01, 0x00, "Yes"			},
};

STDDIPINFO(Samesame2)

static void __fastcall rallybik_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x1c8000:
			if (data == 0) {
				ZetReset();
				BurnYM3812Reset();
			}
		return;
	}

	bprintf (0, _T("MWW: %5.5x, %4.4x\n"), address, data);
}

static void __fastcall truxton_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x1d0000:
			if (data == 0) {
				ZetReset();
				BurnYM3812Reset();
			}
		return;
	}

	bprintf (0, _T("MWW: %5.5x, %4.4x\n"), address, data);
}

static void __fastcall hellfire_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x180000:
			tiles_offsets_x = data;
		return;

		case 0x180002:
			tiles_offsets_y = data;
		return;

		case 0x180006:
			sprite_flipscreen = data & 0x8000;
		return;

		case 0x180008:
			if (data == 0) {
				ZetReset();
				BurnYM3812Reset();
			}
		return;
	}

	bprintf (0, _T("MWW: %5.5x, %4.4x\n"), address, data);
}

static void demonwld_dsp(INT32 enable)
{
	enable ^= 1;
	dsp_on = enable;

	if (enable)
	{
		tms32010_set_irq_line(0, CPU_IRQSTATUS_ACK);
		m68k_halt = 1;
		SekRunEnd();
	}
	else
	{
		tms32010_set_irq_line(0, CPU_IRQSTATUS_NONE);
		tms32010RunEnd();
	}
}

static void __fastcall demonwld_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0xe00000:
			tiles_offsets_x = data;
		return;

		case 0xe00002:
			tiles_offsets_y = data;
		return;

		case 0xe00006:
			sprite_flipscreen = data & 0x8000;
		return;

		case 0xe00008:
			if (data == 0) {
				ZetReset();
				BurnYM3812Reset();
			}
		return;

		case 0xe0000a:
			if (data < 2) demonwld_dsp(data & 1);
		return;
	}
}

static UINT16 __fastcall samesame_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x140000:
			return DrvInputs[0];

		case 0x140002:
			return DrvInputs[1];

		case 0x140004:
			return DrvDips[0];

		case 0x140006:
			return DrvDips[1];

		case 0x140008:
			return (DrvInputs[2] & 0x7f) | (vblank ? 0x80 : 0);

		case 0x14000a:
			return DrvDips[2] | 0x80;

		case 0x14000e:
			return 0; // mcu read?
	}

	bprintf (0, _T("MRW: %5.5x\n"), address);

	return 0;
}

static UINT8 __fastcall samesame_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x140005:
			return DrvDips[0];

		case 0x140007:
			return DrvDips[1];

		case 0x140009:
			return (DrvInputs[2] & 0x7f) | (vblank ? 0x80 : 0);

		case 0x14000b:
			return DrvDips[2] | 0x80;
	}

	bprintf (0, _T("RB: %5.5x\n"), address);

	return 0;
}

static void __fastcall samesame_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x14000c:
			// coin counter / lockout
		return;

		case 0x14000e:
			soundlatch = data;
			mcu_command = 1;
		return;
	}

	bprintf (0, _T("MWW: %5.5x, %4.4x\n"), address,data);
}

static UINT16 __fastcall toaplan1_main_read_word(UINT32 address)
{
	bprintf (0, _T("MRW: %5.5x\n"), address);

	return 0;
}

static UINT8 __fastcall toaplan1_main_read_byte(UINT32 address)
{
	bprintf (0, _T("MRB: %5.5x\n"), address);

	return 0;
}

static void __fastcall toaplan1_main_write_word(UINT32 address, UINT16 data)
{
	bprintf (0, _T("MWW: %5.5x, %4.4x\n"), address, data);
}

static void __fastcall toaplan1_main_write_byte(UINT32 address, UINT8 data)
{
	bprintf (0, _T("MWB: %5.5x, %2.2x\n"), address, data);
}

static void __fastcall toaplan1_miscctrl2_write_word(UINT32 address, UINT16 data)
{
	switch (address & 0x0006)
	{
		case 0x0000:
			tiles_offsets_x = data;
		return;

		case 0x0002:
			tiles_offsets_y = data;
		return;

		case 0x0006:
			sprite_flipscreen = data & 0x8000;
		return;
	}
}

static void __fastcall toaplan1_miscctrl2_write_byte(UINT32 address, UINT8 data)
{
	bprintf (0, _T("MWB: %5.5x, %2.2x\n"), address, data);
}

static UINT16 __fastcall toaplan1_miscctrl_read_word(UINT32 address)
{
	switch (address & 0x03fe)
	{
		case 0x0000:
			return (vblank ? 0x01 : 0);
	}

	return 0;
}

static UINT8 __fastcall toaplan1_miscctrl_read_byte(UINT32 address)
{
	switch (address & 0x03ff)
	{
		case 0x0001:
			return (vblank ? 0x01 : 0);
	}

	return 0;
}

static void __fastcall toaplan1_miscctrl_write_word(UINT32 address, UINT16 data)
{
	switch (address & 0x000e)
	{
		case 0x0000:
			// video frame related?
		return;

		case 0x0002:
			interrupt_enable = data & 0xff;
		return;

		case 0x0008: // controls refresh rate, screen dimensions
		case 0x000a:
		case 0x000c:
		case 0x000e:
		//	bprintf (0, _T("BCU CONTROL: %5.5x, %4.4x\n"), address, data);
		return;
	}

	bprintf (0, _T("MWW: %5.5x, %4.4x\n"), address, data);
}

static void __fastcall toaplan1_miscctrl_write_byte(UINT32 address, UINT8 data)
{
	switch (address & 0x000f)
	{
		case 0x0001:
			// video frame related?
		return;
	}

	bprintf (0, _T("MWB: %5.5x, %2.2x\n"), address, data);
}

static UINT16 __fastcall toaplan1_tilemap_read_word(UINT32 address)
{
	switch (address & 0x001e)
	{
		case 0x0002:
			return tileram_offs;

		case 0x0004:
		{
			UINT16 ret = *((UINT16*)(DrvVidRAM + ((tileram_offs & 0x3fff) * 4)));

			// rallybik (doesn't seem to affect other games)
			ret |= ((ret & 0xf000) >> 4);
			ret |= ((ret & 0x0030) << 2);

			return ret;
		}

		case 0x0006:
			return *((UINT16*)(DrvVidRAM + ((tileram_offs & 0x3fff) * 4) + (address & 2)));

		case 0x0010:
		case 0x0012:
		case 0x0014:
		case 0x0016:
		case 0x0018:
		case 0x001a:
		case 0x001c:
		case 0x001e:
			return scroll[(address / 2) & 7];
	}

	return 0;
}

static UINT8 __fastcall toaplan1_tilemap_read_byte(UINT32 address)
{
	bprintf (0, _T("MRB: %5.5x\n"), address);
	return 0;
}

static void __fastcall toaplan1_tilemap_write_word(UINT32 address, UINT16 data)
{
	switch (address & 0x001e)
	{
		case 0x0000:
			flipscreen = data & 0x01;
		return;

		case 0x0002:
			tileram_offs = data;
		return;

		case 0x0004:
		case 0x0006:
			*((UINT16*)(DrvVidRAM + ((tileram_offs & 0x3fff) * 4) + (address & 2))) = data;
		return;

		case 0x0010:
		case 0x0012:
		case 0x0014:
		case 0x0016:
		case 0x0018:
		case 0x001a:
		case 0x001c:
		case 0x001e:
			scroll[(address / 2) & 7] = data;
		return;
	}
}

static void __fastcall toaplan1_tilemap_write_byte(UINT32 address, UINT8 data)
{
	bprintf (0, _T("MWB: %5.5x, %2.2x\n"), address, data);
}

static UINT16 __fastcall toaplan1_spriteram_read_word(UINT32 address)
{
	switch (address & 0x0006)
	{
		case 0x0002:
			return spriteram_offset;

		case 0x0004:
		{
			UINT16 *spriteram = (UINT16*)DrvSprRAM;
			UINT16 ret = spriteram[spriteram_offset & 0x7ff];
			spriteram_offset++;
			return ret;
		}

		case 0x0006:
		{
			UINT16 *spritesizeram = (UINT16*)DrvSprSizeRAM;
			UINT16 ret = spritesizeram[spriteram_offset & 0x3f];
			spriteram_offset++;
			return ret;
		}
	}

	return 0;
}

static UINT8 __fastcall toaplan1_spriteram_read_byte(UINT32 address)
{
	switch (address & 0x0007)
	{
		case 0x0001:
			return (vblank ? 0x01 : 0);
	}

	bprintf (0, _T("MRB: %5.5x\n"), address);

	return 0;
}

static void __fastcall toaplan1_spriteram_write_word(UINT32 address, UINT16 data)
{
	switch (address & 0x0006)
	{
		case 0x0002:
			spriteram_offset = data;
		return;

		case 0x0004:
		{
			UINT16 *spriteram = (UINT16*)DrvSprRAM;
			spriteram[spriteram_offset & 0x7ff] = data;
			spriteram_offset++;
		}
		return;

		case 0x0006:
		{
			UINT16 *spritesizeram = (UINT16*)DrvSprSizeRAM;
			spritesizeram[spriteram_offset & 0x3f] = data;
			spriteram_offset++;
		}
		return;
	}
}

static void __fastcall toaplan1_spriteram_write_byte(UINT32 address, UINT8 data)
{
	bprintf (0, _T("MWB: %5.5x, %2.2x\n"), address, data);
}

static inline void update_palette(INT32 offset)
{
	UINT16 p = *((UINT16*)(DrvPalRAM + offset * 2));

	UINT8 r = (p >>  0) & 0x1f;
	UINT8 g = (p >>  5) & 0x1f;
	UINT8 b = (p >> 10) & 0x1f;

	r = (r << 3) | (r >> 2);
	g = (g << 3) | (g >> 2);
	b = (b << 3) | (b >> 2);

	DrvPalette[offset] = BurnHighCol(r,g,b,0);
}

static void __fastcall toaplan1_palette_write_word(UINT32 address, UINT16 data)
{
	UINT16 *p = (UINT16*)DrvPalRAM;
	INT32 offset = ((address & 0x7ff) / 2) + ((address & 0x2000) >> 3);
	p[offset] = data;
	update_palette(offset);
}

static void __fastcall toaplan1_palette_write_byte(UINT32 address, UINT8 data)
{
	INT32 offset = (address & 0x7ff) + ((address & 0x2000) >> 2);
	DrvPalRAM[offset] = data;
	update_palette(offset/2);
}

static void __fastcall toaplan1_shareram_write_word(UINT32 address, UINT16 data)
{
	DrvShareRAM[(address / 2) & 0x7ff] = data;
}

static void __fastcall toaplan1_shareram_write_byte(UINT32 address, UINT8 data)
{
	DrvShareRAM[(address / 2) & 0x7ff] = data;
}

static UINT16 __fastcall toaplan1_shareram_read_word(UINT32 address)
{
	return DrvShareRAM[(address / 2) & 0x7ff];
}

static UINT8 __fastcall toaplan1_shareram_read_byte(UINT32 address)
{
	return DrvShareRAM[(address / 2) & 0x7ff];
}

static void demonwld_dsp_w(UINT16 data)
{
	dsp_execute = 0;

	if (main_ram_seg == 0xc00000)
	{
		if ((dsp_addr_w < 3) && (data == 0)) dsp_execute = 1;

		SekWriteWord(main_ram_seg + dsp_addr_w, data);
	}
}

static void demonwld_dsp_bio_w(UINT16 data)
{
	if (data & 0x8000)
	{
		dsp_BIO = 0;
	}

	if (data == 0)
	{
		if (dsp_execute)
		{
			m68k_halt = 0;
			dsp_execute = 0;
		}

		dsp_BIO = 1;
	}
}

static void dsp_write(INT32 port, UINT16 data)
{
	switch (port)
	{
		case 0x00:
			main_ram_seg = ((data & 0xe000) << 9);
			dsp_addr_w   = ((data & 0x1fff) << 1);
		return;

		case 0x01: demonwld_dsp_w(data); return;
		case 0x03: demonwld_dsp_bio_w(data); return;
	}
}

static UINT16 dsp_read(INT32 port)
{
	switch (port)
	{
		case 0x01: return SekReadWord(main_ram_seg + dsp_addr_w);
		case 0x10: return dsp_BIO;
	}

	return 0;
}

static void __fastcall rallybik_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x30:
			// coin counter / lockout
		return;

		case 0x60:
		case 0x61:
			BurnYM3812Write(0, port & 1, data);
		return;
	}
}

static UINT8 __fastcall rallybik_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x10:
		case 0x20:
			return DrvInputs[(port >> 4) & 3];

		case 0x40:
		case 0x50:
			return DrvDips[(port >> 4) & 1];

		case 0x60:
			return BurnYM3812Read(0, 0);

		case 0x70: // truxton
			return DrvDips[2];
	}

	return 0;
}

static void __fastcall hellfire_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x30:
			// coin counter / lockout
		return;

		case 0x70:
		case 0x71:
			BurnYM3812Write(0, port & 1, data);
		return;
	}
}

static UINT8 __fastcall hellfire_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x10:
			return DrvDips[(port >> 4) & 1];

		case 0x20:
			return DrvDips[2];

		case 0x40:
		case 0x50:
		case 0x60:
			return DrvInputs[(port >> 4) & 3];

		case 0x70:
			return BurnYM3812Read(0, 0);
	}

	return 0;
}

static void __fastcall zerowing_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0xa0:
			// coin counter / lockout
		return;

		case 0xa8:
		case 0xa9:
			BurnYM3812Write(0, port & 1, data);
		return;
	}
}

static UINT8 __fastcall zerowing_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x08:
			return DrvInputs[(port >> 3) & 1];

		case 0x20:
		case 0x28:
			return DrvDips[(port >> 3) & 1];

		case 0x80:
			return DrvInputs[2];

		case 0x88:
			return DrvDips[2];

		case 0xa8:
			return BurnYM3812Read(0, 0);
	}

	return 0;
}

static void __fastcall outzone_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
			BurnYM3812Write(0, port & 1, data);
		return;

		case 0x04:
			// coin counter / lockout
		return;
	}
}

static UINT8 __fastcall outzone_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
			return BurnYM3812Read(0, 0);

		case 0x08:
		case 0x0c:
			return DrvDips[(port >> 2) & 1];

		case 0x10:
			return DrvInputs[2];

		case 0x14:
		case 0x18:
			return DrvInputs[(port >> 3) & 1];

		case 0x1c:
			return DrvDips[2];
	}

	return 0;
}

static void __fastcall vimana_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x84:
			// coin counter / lockout
		return;

		case 0x87:
		case 0x8f:
			BurnYM3812Write(0, (port >> 3) & 1, data);
		return;
	}
}

static UINT8 __fastcall vimana_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x87:
			return BurnYM3812Read(0, 0);

		case 0x60:
			return DrvDips[1] ^ 0xff;

		case 0x66:
			return DrvDips[2] ^ 0xff;

		case 0x82:
			return DrvDips[0];

		case 0x83:
			return DrvInputs[2];

		case 0x80:
		case 0x81:
			return DrvInputs[port & 1];
	}

	return 0;
}

static void __fastcall samesame_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x80:
		case 0x81:
			BurnYM3812Write(0, port & 1, data);
		return;

		case 0xb0:
			soundlatch = data;
			mcu_command = 0;
		return;
	}
}

static UINT8 __fastcall samesame_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x63:
			return mcu_command ? 0xff : 0;

		case 0x80:
		case 0x81:
			return BurnYM3812Read(0, port & 1);

		case 0xa0:
			return soundlatch;
	}

	return 0;
}

static void __fastcall demonwld_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
			BurnYM3812Write(0, port & 1, data);
		return;

		case 0x40:
			// coin counter / lockout
		return;
	}
}

static UINT8 __fastcall demonwld_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
			return BurnYM3812Read(0, port & 1);

		case 0x20:
			return DrvDips[2];

		case 0x60:
			return DrvInputs[2];

		case 0x80:
			return DrvInputs[0];

		case 0xa0:
			return DrvDips[1];

		case 0xc0:
			return DrvInputs[1];

		case 0xe0:
			return DrvDips[0];
	}

	return 0;
}

#define tilemap_cb(layer)					\
	UINT16 *ram = (UINT16*)(DrvVidRAM + (layer*0x4000));	\
	UINT16 attr = ram[offs * 2 + 0];			\
	UINT16 code = ram[offs * 2 + 1];			\
	UINT8 color = attr & 0x3f;				\
	INT32 flags = (code & 0x8000) ? TILE_SKIP : 0;		\
	if (DrvTransTable[code]) flags |= TILE_SKIP;		\
	flags |= TILE_GROUP(attr >> 12);			\
	TILE_SET_INFO(0, code, color, flags)

static tilemap_callback( layer0 )
{
	tilemap_cb(0);
}

static tilemap_callback( layer1 )
{
	tilemap_cb(1);
}

static tilemap_callback( layer2 )
{
	tilemap_cb(2);
}

static tilemap_callback( layer3 )
{
	tilemap_cb(3);
}

inline static INT32 toaplan1SynchroniseStream(INT32 nSoundRate)
{
	return (INT64)(ZetTotalCycles() * nSoundRate / 3500000);
}

static void toaplan1YM3812IrqHandler(INT32, INT32 nStatus)
{
	ZetSetIRQLine(0, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
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

	HiscoreReset();

	m68k_halt = 0;

	if (has_dsp)
	{
		tms32010_reset();

		main_ram_seg = 0;
		dsp_addr_w = 0;
		dsp_execute = 0;
		dsp_BIO = 0;
		dsp_on = 0;
	}

	flipscreen = 0;
	sprite_flipscreen = 0;
	interrupt_enable = 0;
	tiles_offsets_x = 0;
	tiles_offsets_y = 0;
	tileram_offs = 0;
	spriteram_offset = 0;
	soundlatch = 0;
	mcu_command = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM	= Next; Next += 0x080000;
	DrvZ80ROM	= Next; Next += 0x008000;

	DrvMCUROM	= Next; Next += 0x001000;

	DrvGfxROM0	= Next; Next += 0x200000;
	DrvGfxROM1	= Next; Next += 0x200000;

	DrvTransTable	= Next; Next += 0x200000 / 0x40;

	DrvPalette	= (UINT32*)Next; Next += 0x0800 * sizeof(UINT32);

	pTempDraw	= (UINT16*)Next; Next += 512 * 512 * sizeof(short);

	AllRam		= Next;

	Drv68KRAM	= Next; Next += 0x008000;
	DrvPalRAM	= Next; Next += 0x001000;
	DrvVidRAM	= Next; Next += 0x010000;
	DrvShareRAM	= Next; Next += 0x000800;
	DrvSprRAM	= Next; Next += 0x001000;
	DrvSprBuf	= Next; Next += 0x001000;
	DrvSprSizeRAM	= Next; Next += 0x000080;
	DrvSprSizeBuf	= Next; Next += 0x000080;
	DrvZ80RAM	= Next; Next += 0x000200;
	DrvMCURAM	= Next; Next += 0x000400;

	scroll		= (UINT16*)Next; Next += 8 * sizeof(INT16);

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static void DrvGfxDecode(UINT8 *src, INT32 len, INT32 type)
{
	INT32 Plane0[4] = { (len/2)*8+8, (len/2)*8+0, 8, 0 };
	INT32 Plane1[4] = { STEP4(0, ((len/4)*8)) };
	INT32 XOffs[16] = { STEP16(0,1) };
	INT32 YOffs[16] = { STEP16(0,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(len);

	memcpy (tmp, src, len);

	if (type == 0)
	{
		GfxDecode(((len*8)/4)/(8*8), 4, 8, 8, Plane0, XOffs, YOffs, 0x080, tmp, src);
	}
	else if (type == 1)
	{
		GfxDecode(((len*8)/4)/(16*16), 4, 16, 16, Plane1, XOffs, YOffs, 0x100, tmp, src);
	}

	BurnFree (tmp);
}

static INT32 LoadRoms()
{
	char* pRomName;
	struct BurnRomInfo ri;
	UINT8 *pLoad = Drv68KROM;
	UINT8 *zLoad = DrvZ80ROM;
	UINT8 *gLoad = DrvGfxROM0;
	UINT8 *sLoad = DrvGfxROM1;
	INT32 sprite_type = 0;

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++)
	{
		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & 0xf) == 1) {
			if (BurnLoadRom(pLoad + 1, i+0, 2)) return 1;
			if (BurnLoadRom(pLoad + 0, i+1, 2)) return 1;
			pLoad += 0x40000; i++;
			continue;
		}

		if ((ri.nType & 0xf) == 2) {
			if (BurnLoadRom(zLoad, i, 1)) return 1;
			zLoad += ri.nLen;
			continue;
		}

		if ((ri.nType & 0xf) == 3) {
			if (BurnLoadRom(gLoad + 0, i+0, 2)) return 1;
			if (BurnLoadRom(gLoad + 1, i+1, 2)) return 1;
			gLoad += ri.nLen * 2; i++;
			continue;
		}

		if ((ri.nType & 0xf) == 4) {
			if (BurnLoadRom(gLoad, i, 1)) return 1;
			gLoad += ri.nLen;
			continue;
		}

		if ((ri.nType & 0xf) == 5) {
			if (BurnLoadRom(sLoad + 0, i+0, 2)) return 1;
			if (BurnLoadRom(sLoad + 1, i+1, 2)) return 1;
			sLoad += ri.nLen * 2; i++;
			continue;
		}

		if ((ri.nType & 0xf) == 6 || (ri.nType & 0xf) == 8) {
			if (BurnLoadRom(sLoad, i, 1)) return 1;
			sLoad += ri.nLen;
			sprite_type = ((ri.nType & 0xf) == 6) ? 1 : 0; // rallybik / vimana
			continue;
		}

		if ((ri.nType & 0xf) == 7) {
			if (BurnLoadRom(DrvMCUROM + 0, i+0, 2)) return 1;
			if (BurnLoadRom(DrvMCUROM + 1, i+1, 2)) return 1;
			i++;
			continue;
		}
	}

	mainrom_size = (pLoad - Drv68KROM);

	tile_mask = gLoad - DrvGfxROM0;
	sprite_mask = sLoad - DrvGfxROM1;

	DrvGfxDecode(DrvGfxROM0, tile_mask, 0);
	DrvGfxDecode(DrvGfxROM1, sprite_mask, sprite_type);

	tile_mask *= 2;
	sprite_mask = ((sprite_mask * 2) / (sprite_type ? 0x100 : 0x40)) - 1;

	{
		for (INT32 i = 0; i < tile_mask; i+=0x40)
		{
			DrvTransTable[i/0x40] = 1;

			for (INT32 j = 0; j < 0x40; j++)
			{
				if (DrvGfxROM0[i+j]) {
					DrvTransTable[i/0x40] = 0;
					break;
				}
			}
		}
	}

	return 0;
}

static void configure_68k_map(UINT32 mainram, UINT32 palette, UINT32 share, UINT32 sprite, UINT32 tilemap, UINT32 misc, UINT32 misc2)
{
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, mainrom_size-1, MAP_ROM);
	SekMapMemory(Drv68KRAM,			mainram,  mainram+0x7fff, MAP_RAM); // some only have 4000
	SekSetWriteWordHandler(0,		toaplan1_main_write_word);
	SekSetWriteByteHandler(0,		toaplan1_main_write_byte);
	SekSetReadWordHandler(0,		toaplan1_main_read_word);
	SekSetReadByteHandler(0,		toaplan1_main_read_byte);

	if (palette != (UINT32)~0)
	{
		SekMapMemory(DrvPalRAM,			palette + 0x0000, palette + 0x07ff, MAP_RAM);
		SekMapMemory(DrvPalRAM + 0x800,		palette + 0x2000, palette + 0x27ff, MAP_RAM);

		SekMapHandler(1,			palette, palette + 0x27ff, MAP_WRITE);
		SekSetWriteWordHandler(1,		toaplan1_palette_write_word);
		SekSetWriteByteHandler(1,		toaplan1_palette_write_byte);
	}

	if (share != (UINT32)~0)
	{
		SekMapHandler(2,			share, share + 0xfff, MAP_RAM);
		SekSetWriteWordHandler(2,		toaplan1_shareram_write_word);
		SekSetWriteByteHandler(2,		toaplan1_shareram_write_byte);
		SekSetReadWordHandler(2,		toaplan1_shareram_read_word);
		SekSetReadByteHandler(2,		toaplan1_shareram_read_byte);
	}

	if (sprite != (UINT32)~0)
	{
		SekMapHandler(3,			sprite, sprite + 0x007, MAP_RAM);
		SekSetWriteWordHandler(3,		toaplan1_spriteram_write_word);
		SekSetWriteByteHandler(3,		toaplan1_spriteram_write_byte);
		SekSetReadWordHandler(3,		toaplan1_spriteram_read_word);
		SekSetReadByteHandler(3,		toaplan1_spriteram_read_byte);
	}

	if (tilemap != (UINT32)~0)
	{
		SekMapHandler(4,			tilemap, tilemap + 0x01f, MAP_RAM);
		SekSetWriteWordHandler(4,		toaplan1_tilemap_write_word);
		SekSetWriteByteHandler(4,		toaplan1_tilemap_write_byte);
		SekSetReadWordHandler(4,		toaplan1_tilemap_read_word);
		SekSetReadByteHandler(4,		toaplan1_tilemap_read_byte);
	}

	if (misc != (UINT32)~0)
	{
		SekMapHandler(5,			misc, misc + 0x00f, MAP_RAM);
		SekSetWriteWordHandler(5,		toaplan1_miscctrl_write_word);
		SekSetWriteByteHandler(5,		toaplan1_miscctrl_write_byte);
		SekSetReadWordHandler(5,		toaplan1_miscctrl_read_word);
		SekSetReadByteHandler(5,		toaplan1_miscctrl_read_byte);
	}

	if (misc != (UINT32)~0)
	{
		SekMapHandler(6,			misc2, misc2 + 0x007, MAP_WRITE);
		SekSetWriteWordHandler(6,		toaplan1_miscctrl2_write_word);
		SekSetWriteByteHandler(6,		toaplan1_miscctrl2_write_byte);
	}

	SekClose();
}

static void common_sound_init(void (__fastcall *write_port)(UINT16, UINT8), UINT8 (__fastcall *read_port)(UINT16))
{
	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvShareRAM,		0x8000, 0x87ff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM,			0xfe00, 0xffff, MAP_RAM); // for z180 games
	ZetSetOutHandler(write_port);
	ZetSetInHandler(read_port);
	ZetClose();

	BurnYM3812Init(1, 3500000, &toaplan1YM3812IrqHandler, toaplan1SynchroniseStream, 0);
	BurnTimerAttachYM3812(&ZetConfig, 3500000);
	BurnYM3812SetRoute(0, BURN_SND_YM3812_ROUTE, 1.00, BURN_SND_ROUTE_BOTH);
}

static void configure_graphics(INT32 xoffs, INT32 yoffs, INT32 adjust_sprite_y)
{
	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, layer0_map_callback, 8, 8, 64, 64);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, layer1_map_callback, 8, 8, 64, 64);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, layer2_map_callback, 8, 8, 64, 64);
	GenericTilemapInit(3, TILEMAP_SCAN_ROWS, layer3_map_callback, 8, 8, 64, 64);
	GenericTilemapSetGfx(0, DrvGfxROM0, 4, 8, 8, tile_mask, 0, 0x3f);
	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetTransparent(2, 0);
	GenericTilemapSetTransparent(3, 0);
	GenericTilemapSetOffsets(0, -(xoffs+6), -(256+1+yoffs));
	GenericTilemapSetOffsets(1, -(xoffs+4), -(256+1+yoffs));
	GenericTilemapSetOffsets(2, -(xoffs+2), -(256+1+yoffs));
	GenericTilemapSetOffsets(3, -(xoffs+0), -(256+1+yoffs));

	sprite_y_adjust = adjust_sprite_y;
}

static INT32 RallybikInit()
{
	vertical_lines = 282;
	BurnSetRefreshRate(((28000000.0 / 4.0) / (450.0 * vertical_lines)));

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (LoadRoms()) return 1;

	configure_68k_map(0x080000, 0x144000, 0x180000, ~0, 0x100000, 0x140000, 0x1c0000);

	SekOpen(0);
	SekMapMemory(DrvSprRAM,		0x0c0000, 0x0c0fff, MAP_RAM);
	SekSetWriteWordHandler(0,	rallybik_main_write_word);
	SekClose();

	common_sound_init(rallybik_sound_write_port, rallybik_sound_read_port);

	configure_graphics(13, 16, 0);

	DrvDoReset();

	return 0;
}

static INT32 TruxtonInit()
{
	vertical_lines = 270;
	BurnSetRefreshRate(((28000000.0 / 4.0) / (450.0 * vertical_lines)));

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (LoadRoms()) return 1;

	configure_68k_map(0x080000, 0x144000, 0x180000, 0x0c0000, 0x100000, 0x140000, 0x1c0000);

	SekOpen(0);
	SekSetWriteWordHandler(0,	truxton_main_write_word);
	SekClose();

	common_sound_init(rallybik_sound_write_port, rallybik_sound_read_port);

	configure_graphics(495, 0, 0);

	DrvDoReset();

	return 0;
}

static INT32 HellfireInit()
{
	vertical_lines = 270;
	BurnSetRefreshRate(((28000000.0 / 4.0) / (450.0 * vertical_lines)));

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (LoadRoms()) return 1;

	configure_68k_map(0x040000, 0x084000, 0x0c0000, 0x140000, 0x100000, 0x080000, ~0);

	SekOpen(0);
	SekSetWriteWordHandler(0,	hellfire_main_write_word);
	SekClose();

	common_sound_init(hellfire_sound_write_port, hellfire_sound_read_port);

	configure_graphics(495, 16, 16);

	DrvDoReset();

	return 0;
}

static INT32 ZerowingInit()
{
	vertical_lines = 270;
	BurnSetRefreshRate(((28000000.0 / 4.0) / (450.0 * vertical_lines)));

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (LoadRoms()) return 1;

	configure_68k_map(0x080000, 0x404000, 0x440000, 0x4c0000, 0x480000, 0x400000, 0x0c0000);

	common_sound_init(zerowing_sound_write_port, zerowing_sound_read_port);

	configure_graphics(495, 16, 16);

	DrvDoReset();

	return 0;
}

static INT32 OutzoneInit()
{
	vertical_lines = 282;
	BurnSetRefreshRate(((28000000.0 / 4.0) / (450.0 * vertical_lines)));

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (LoadRoms()) return 1;

	configure_68k_map(0x240000, 0x304000, 0x140000, 0x100000, 0x200000, 0x300000, 0x340000);

	common_sound_init(outzone_sound_write_port, outzone_sound_read_port);

	configure_graphics(495, 0, 0);

	DrvDoReset();

	return 0;
}

static INT32 OutzonecvInit()
{
	vertical_lines = 270;
	BurnSetRefreshRate(((28000000.0 / 4.0) / (450.0 * vertical_lines)));

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (LoadRoms()) return 1;

	configure_68k_map(0x080000, 0x404000, 0x440000, 0x4c0000, 0x480000,0x400000, 0x0c0000);

	common_sound_init(zerowing_sound_write_port, zerowing_sound_read_port);

	configure_graphics(495, 0, 0);

	DrvDoReset();

	return 0;
}

static INT32 VimanaInit()
{
	vertical_lines = 270;
	BurnSetRefreshRate(((28000000.0 / 4.0) / (450.0 * vertical_lines)));

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (LoadRoms()) return 1;

	configure_68k_map(0x480000, 0x404000, 0x440000, 0x0c0000, 0x4c0000, 0x400000, 0x080000);

	// z180 cpu!
	common_sound_init(vimana_sound_write_port, vimana_sound_read_port);

	configure_graphics(495, 0, 0);

	DrvDoReset();

	return 0;
}

static INT32 SamesameInit()
{
	samesame = 1;
	vertical_lines = 270;
	BurnSetRefreshRate(((28000000.0 / 4.0) / (450.0 * vertical_lines)));

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (LoadRoms()) return 1;

	configure_68k_map(0x0c0000, 0x104000, ~0, 0x1c0000, 0x180000, 0x100000, 0x080000);

	SekOpen(0);
	SekSetWriteWordHandler(0,		samesame_main_write_word);
	SekSetReadWordHandler(0,		samesame_main_read_word);
	SekSetReadByteHandler(0,		samesame_main_read_byte);
	SekClose();

	// z180 cpu!
	common_sound_init(samesame_sound_write_port, samesame_sound_read_port);

	configure_graphics(495, 0, 0);

	DrvDoReset();

	return 0;
}

static INT32 DemonwldInit()
{
	vertical_lines = 282;
	BurnSetRefreshRate(((28000000.0 / 4.0) / (450.0 * vertical_lines)));

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (LoadRoms()) return 1;

	configure_68k_map(0xc00000, 0x404000, 0x600000, 0xa00000, 0x800000, 0x400000, ~0);

	SekOpen(0);
	SekSetWriteWordHandler(0,		demonwld_main_write_word);
	SekClose();

	{
		has_dsp = 1;
		tms32010_init();
		tms32010_set_write_port_handler(dsp_write);
		tms32010_set_read_port_handler(dsp_read);
		tms32010_ram = (UINT16*)DrvMCURAM;
		tms32010_rom = (UINT16*)DrvMCUROM;
	}

	common_sound_init(demonwld_sound_write_port, demonwld_sound_read_port);

	configure_graphics(495, 16, 16);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	BurnYM3812Exit();

	SekExit();
	ZetExit();

	if (has_dsp) {
		tms32010_exit();
	}

	BurnFree (AllMem);

	has_dsp = 0;
	samesame = 0;

	return 0;
}

static void DrvPaletteUpdate()
{
	UINT16 *p = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < BurnDrvGetPaletteEntries(); i++)
	{
		UINT8 r = (p[i] >>  0) & 0x1f;
		UINT8 g = (p[i] >>  5) & 0x1f;
		UINT8 b = (p[i] >> 10) & 0x1f;

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void rallybik_draw_sprites()
{
	INT32 xoffs = 35;
	INT32 xoffs_flipped = 15;

	UINT16 *spriteram = (UINT16*)DrvSprBuf;

	for (INT32 offs = (0x1000/2)-4; offs >= 0; offs -= 4)
	{
		UINT16 attr = spriteram[offs + 1];
		UINT16 priority = (attr & 0x0c00)>>8;
		if (!priority) continue;

		INT32 sy = spriteram[offs + 3] >> 7;

		if (sy != 0x0100)
		{
			UINT16 code  = spriteram[offs] & 0x7ff;
			UINT16 color = ((attr & 0x3f) << 4) | 0x400;
			INT32  sx    = spriteram[offs + 2] >> 7;
			INT32 flipx  = attr & 0x100;
			INT32 flipy  = attr & 0x200;
			
			if (flipx) sx -= xoffs_flipped;

			{
				sx -= xoffs;
				sy -= 16;

				if (sy < -15 || sx < -15 || sx >= nScreenWidth || sy >= nScreenHeight) continue;

				UINT8 *gfx = DrvGfxROM1 + (code * 0x100);

				INT32 flip = (flipx ? 0x0f : 0) | (flipy ? 0xf0 : 0);

				UINT16 *dst = pTransDraw + (sy * nScreenWidth) + sx;
				UINT8  *pri = pPrioDraw + (sy * nScreenWidth) + sx;

				for (INT32 y = 0; y < 16; y++, dst += nScreenWidth, pri += nScreenWidth)
				{
					if ((sy + y) < 0 || (sy + y) >= nScreenHeight) continue;

					for (INT32 x = 0; x < 16; x++)
					{
						if ((sx + x) < 0 || (sx + x) >= nScreenWidth) continue;

						INT32 pxl = gfx[((y * 16) + x) ^ flip];

						if (pxl) {
							if (pri[x] <= priority) {
								dst[x] = pxl + color;
								pri[x] = 0xff;
							}
						}
					}
				}
			}
		}
	}
}

static INT32 RallybikDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 0;
	}

	for (INT32 i = 0; i < 4; i++) {
		GenericTilemapSetScrollX(i, (scroll[0+i*2] >> 7) - tiles_offsets_x);
		GenericTilemapSetScrollY(i, (scroll[1+i*2] >> 7) - tiles_offsets_y);
	}

	BurnTransferClear();

	if (nBurnLayer & 8) GenericTilemapDraw(0, pTransDraw, TMAP_FORCEOPAQUE);

	for (INT32 priority = 1; priority < 16; priority++)
	{
		if (nBurnLayer & 1) GenericTilemapDraw(3, pTransDraw, TMAP_SET_GROUP(priority)|priority);
		if (nBurnLayer & 2) GenericTilemapDraw(2, pTransDraw, TMAP_SET_GROUP(priority)|priority);
		if (nBurnLayer & 4) GenericTilemapDraw(1, pTransDraw, TMAP_SET_GROUP(priority)|priority);
		if (nBurnLayer & 8) GenericTilemapDraw(0, pTransDraw, TMAP_SET_GROUP(priority)|priority);
	}

	rallybik_draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void toaplan1_draw_sprite_custom(UINT32 code, UINT32 color, INT32 flipx, INT32 flipy, INT32 sx, INT32 sy, INT32 priority)
{
	sy -= sprite_y_adjust;

	INT32 pal_base = 0x400 + (color & 0x3f) * 0x10;
	UINT8 *source_base = DrvGfxROM1 + (code * 0x40);
	UINT8 *priority_bitmap = pPrioDraw;

	int sprite_screen_height = ((1<<16)*8+0x8000)>>16;
	int sprite_screen_width = ((1<<16)*8+0x8000)>>16;

	if (sprite_screen_width && sprite_screen_height)
	{
		int dx = (8<<16)/sprite_screen_width;
		int dy = (8<<16)/sprite_screen_height;

		int ex = sx+sprite_screen_width;
		int ey = sy+sprite_screen_height;

		int x_index_base;
		int y_index;

		if( flipx )
		{
			x_index_base = (sprite_screen_width-1)*dx;
			dx = -dx;
		}
		else
		{
			x_index_base = 0;
		}

		if( flipy )
		{
			y_index = (sprite_screen_height-1)*dy;
			dy = -dy;
		}
		else
		{
			y_index = 0;
		}

		if( sx < 0/*clip.min_x*/)
		{
			int pixels = 0-sx; //clip.min_x-sx;
			sx += pixels;
			x_index_base += pixels*dx;
		}

		if( sy < 0/*clip.min_y*/)
		{
			int pixels = 0-sy; //clip.min_y-sy;
			sy += pixels;
			y_index += pixels*dy;
		}

		if( ex > nScreenWidth/*clip.max_x+1*/)
		{
			int pixels = ex-nScreenWidth; //clip.max_x-1;
			ex -= pixels;
		}

		if( ey > nScreenHeight/*clip.max_y+1*/)
		{
			int pixels = ey-nScreenHeight; //clip.max_y-1;
			ey -= pixels;
		}

		if( ex>sx )
		{
			for(INT32 y=sy; y<ey; y++ )
			{
				const UINT8 *source = source_base + (y_index>>16) * 8;
				UINT16 *dest = pTransDraw + (y * nScreenWidth);
				uint8_t *pri = priority_bitmap + (y * nScreenWidth);

				int x_index = x_index_base;
				for(INT32 x=sx; x<ex; x++ )
				{
					int c = source[x_index>>16];
					if( c != 0 )
					{
						if (pri[x] < priority)
							dest[x] = pal_base+c;
						pri[x] = 0xff; // mark it "already drawn"
					}
					x_index += dx;
				}

				y_index += dy;
			}
		}
	}
}

static void draw_sprites()
{
	UINT16 *source = (UINT16*)DrvSprBuf;
	UINT16 *size   = (UINT16*)DrvSprSizeBuf;
	INT32 fcu_flipscreen = sprite_flipscreen;

	for (INT32 offs = 0x1000/2 - 4; offs >= 0; offs -= 4)
	{
		if (!(source[offs] & 0x8000))
		{
			INT32 sx, sy;

			INT32 attrib = source[offs+1];
			INT32 priority = (attrib & 0xf000) >> 12;

			INT32 sprite = source[offs] & 0x7fff;
			INT32 color = attrib & 0x3f;

			INT32 sizeram_ptr = (attrib >> 6) & 0x3f;
			INT32 sprite_sizex = ( size[sizeram_ptr]       & 0x0f) * 8;
			INT32 sprite_sizey = ((size[sizeram_ptr] >> 4) & 0x0f) * 8;

			INT32 sx_base = (source[offs + 2] >> 7) & 0x1ff;
			INT32 sy_base = (source[offs + 3] >> 7) & 0x1ff;

			if (sx_base >= 0x180) sx_base -= 0x200;
			if (sy_base >= 0x180) sy_base -= 0x200;

			if (fcu_flipscreen)
			{
				sx_base = 320 - (sx_base + 8);
				sy_base = 240 - (sy_base + 8);
			//	sy_base += ((visarea.max_y + 1) - 240) * 2;    // Horizontal games are offset so adjust by +0x20
			}

			for (INT32 dim_y = 0; dim_y < sprite_sizey; dim_y += 8)
			{
				if (fcu_flipscreen) sy = sy_base - dim_y;
				else                sy = sy_base + dim_y;

				for (INT32 dim_x = 0; dim_x < sprite_sizex; dim_x += 8)
				{
					if (fcu_flipscreen) sx = sx_base - dim_x;
					else                sx = sx_base + dim_x;

					toaplan1_draw_sprite_custom(sprite & sprite_mask,color, fcu_flipscreen,fcu_flipscreen, sx,sy, priority);

					sprite++ ;
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

	for (INT32 i = 0; i < 4; i++) {
		GenericTilemapSetScrollX(i, (scroll[0+i*2] >> 7) - tiles_offsets_x);
		GenericTilemapSetScrollY(i, (scroll[1+i*2] >> 7) - tiles_offsets_y);
	}

	BurnTransferClear();

	if (nBurnLayer & 8) GenericTilemapDraw(0, pTransDraw, TMAP_FORCEOPAQUE);

	for (INT32 priority = 1; priority < 16; priority++)
	{
		if (nBurnLayer & 1) GenericTilemapDraw(3, pTransDraw, TMAP_SET_GROUP(priority)|priority);
		if (nBurnLayer & 2) GenericTilemapDraw(2, pTransDraw, TMAP_SET_GROUP(priority)|priority);
		if (nBurnLayer & 4) GenericTilemapDraw(1, pTransDraw, TMAP_SET_GROUP(priority)|priority);
		if (nBurnLayer & 8) GenericTilemapDraw(0, pTransDraw, TMAP_SET_GROUP(priority)|priority);
	}

	draw_sprites();

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
		DrvInputs[0] = DrvInputs[1] = DrvInputs[2] = 0;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = vertical_lines;
	INT32 nCyclesTotal[3] = { (10000000 * 100) / nBurnFPS, (3500000 * 100) / nBurnFPS, (14000000 * 100) / nBurnFPS };
	INT32 nCyclesDone[3] = { 0, 0, 0 };
	INT32 nVBlankLine = 240 + sprite_y_adjust;

	SekOpen(0);
	ZetOpen(0);

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		if (m68k_halt) {
			SekIdle(nCyclesTotal[0] / nInterleave);
			nCyclesDone[0] += nCyclesTotal[0] / nInterleave;
		} else {
			nCyclesDone[0] += SekRun(((i + 1) * nCyclesTotal[0] / nInterleave) - nCyclesDone[0]);
		}

		if (i == nVBlankLine) {
			vblank = 1;
			if (interrupt_enable) SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);

			if (pBurnDraw) {
				BurnDrvRedraw();
			}

			memcpy (DrvSprBuf , DrvSprRAM , 0x1000);
			memcpy (DrvSprSizeBuf , DrvSprSizeRAM , 0x80);
		}

		if (samesame && i == nVBlankLine + 2) {
			SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);
		}

		BurnTimerUpdateYM3812((i + 1) * (nCyclesTotal[1] / nInterleave));

		if (has_dsp && dsp_on) tms32010_execute(nCyclesTotal[2] / nInterleave);
	}

	BurnTimerEndFrameYM3812(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		BurnYM3812Update(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	SekClose();

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029698;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		SekScan(nAction);
		ZetScan(nAction);

		BurnYM3812Scan(nAction, pnMin);

		SCAN_VAR(flipscreen);
		SCAN_VAR(interrupt_enable);
		SCAN_VAR(tiles_offsets_x);
		SCAN_VAR(tiles_offsets_y);
		SCAN_VAR(tileram_offs);
		SCAN_VAR(spriteram_offset);
		SCAN_VAR(sprite_flipscreen);
		SCAN_VAR(soundlatch);
		SCAN_VAR(mcu_command);

		if (has_dsp) {
			tms32010_scan(nAction);

			SCAN_VAR(m68k_halt);
			SCAN_VAR(main_ram_seg);
			SCAN_VAR(dsp_addr_w);
			SCAN_VAR(dsp_execute);
			SCAN_VAR(dsp_BIO);
			SCAN_VAR(dsp_on);
		}
	}

	return 0;
}


// Rally Bike / Dash Yarou

static struct BurnRomInfo rallybikRomDesc[] = {
	{ "b45-02.rom",			0x08000, 0x383386d7, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "b45-01.rom",			0x08000, 0x7602f6a7, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "b45-04.rom",			0x20000, 0xe9b005b1, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "b45-03.rom",			0x20000, 0x555344ce, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "b45-05.rom",			0x04000, 0x10814601, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "b45-09.bin",			0x20000, 0x1dc7b010, 3 | BRF_GRA },           //  5 Layer Tiles
	{ "b45-08.bin",			0x20000, 0xfab661ba, 3 | BRF_GRA },           //  6
	{ "b45-07.bin",			0x20000, 0xcd3748b4, 3 | BRF_GRA },           //  7
	{ "b45-06.bin",			0x20000, 0x144b085c, 3 | BRF_GRA },           //  8

	{ "b45-11.rom",			0x10000, 0x0d56e8bb, 6 | BRF_GRA },           //  9 Sprites
	{ "b45-10.rom",			0x10000, 0xdbb7c57e, 6 | BRF_GRA },           // 10
	{ "b45-12.rom",			0x10000, 0xcf5aae4e, 6 | BRF_GRA },           // 11
	{ "b45-13.rom",			0x10000, 0x1683b07c, 6 | BRF_GRA },           // 12

	{ "b45-15.bpr",			0x00100, 0x24e7d62f, 0 | BRF_OPT },           // 13 PROMs
	{ "b45-16.bpr",			0x00100, 0xa50cef09, 0 | BRF_OPT },           // 14
	{ "b45-14.bpr",			0x00020, 0xf72482db, 0 | BRF_OPT },           // 15
	{ "b45-17.bpr",			0x00020, 0xbc88cced, 0 | BRF_OPT },           // 16
};

STD_ROM_PICK(rallybik)
STD_ROM_FN(rallybik)

struct BurnDriver BurnDrvRallybik = {
	"rallybik", NULL, NULL, NULL, "1988",
	"Rally Bike / Dash Yarou\0", NULL, "Toaplan / Taito Corporation", "Toaplan BCU-2 / FCU-2 based",
	L"Rally Bike\0\u30C0\u30C3\u30B7\u30E5\uC91E\u90CE\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TOAPLAN_RAIZING, GBF_RACING, 0,
	NULL, rallybikRomInfo, rallybikRomName, NULL, NULL, NULL, NULL, Drv2bInputInfo, RallybikDIPInfo,
	RallybikInit, DrvExit, DrvFrame, RallybikDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Truxton / Tatsujin

static struct BurnRomInfo truxtonRomDesc[] = {
	{ "b65_11.bin",			0x20000, 0x1a62379a, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "b65_10.bin",			0x20000, 0xaff5195d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "b65_09.bin",			0x04000, 0x1bdd4ddc, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "b65_08.bin",			0x20000, 0xd2315b37, 3 | BRF_GRA },           //  3 Layer Tiles
	{ "b65_07.bin",			0x20000, 0xfb83252a, 3 | BRF_GRA },           //  4
	{ "b65_06.bin",			0x20000, 0x36cedcbe, 3 | BRF_GRA },           //  5
	{ "b65_05.bin",			0x20000, 0x81cd95f1, 3 | BRF_GRA },           //  6

	{ "b65_04.bin",			0x20000, 0x8c6ff461, 5 | BRF_GRA },           //  7 Sprites
	{ "b65_03.bin",			0x20000, 0x58b1350b, 5 | BRF_GRA },           //  8
	{ "b65_02.bin",			0x20000, 0x1dd55161, 5 | BRF_GRA },           //  9
	{ "b65_01.bin",			0x20000, 0xe974937f, 5 | BRF_GRA },           // 10

	{ "b65_12.bpr",			0x00020, 0xbc88cced, 0 | BRF_OPT },           // 11 PROMs
	{ "b65_13.bpr",			0x00020, 0xa1e17492, 0 | BRF_OPT },           // 12
};

STD_ROM_PICK(truxton)
STD_ROM_FN(truxton)

struct BurnDriver BurnDrvTruxton = {
	"truxton", NULL, NULL, NULL, "1988",
	"Truxton / Tatsujin\0", NULL, "Toaplan / Taito Corporation", "Toaplan BCU-2 / FCU-2 based",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TOAPLAN_RAIZING, GBF_VERSHOOT, 0,
	NULL, truxtonRomInfo, truxtonRomName, NULL, NULL, NULL, NULL, Drv2bInputInfo, TruxtonDIPInfo,
	TruxtonInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Hellfire (2P set)

static struct BurnRomInfo hellfireRomDesc[] = {
	{ "b90_14.0",			0x20000, 0x101df9f5, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "b90_15.1",			0x20000, 0xe67fd452, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "b90_03.2",			0x08000, 0x4058fa67, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "b90_04.3",			0x20000, 0xea6150fc, 3 | BRF_GRA },           //  3 Layer Tiles
	{ "b90_05.4",			0x20000, 0xbb52c507, 3 | BRF_GRA },           //  4
	{ "b90_06.5",			0x20000, 0xcf5b0252, 3 | BRF_GRA },           //  5
	{ "b90_07.6",			0x20000, 0xb98af263, 3 | BRF_GRA },           //  6

	{ "b90_11.10",			0x20000, 0xc33e543c, 5 | BRF_GRA },           //  7 Sprites
	{ "b90_10.9",			0x20000, 0x35fd1092, 5 | BRF_GRA },           //  8
	{ "b90_09.8",			0x20000, 0xcf01009e, 5 | BRF_GRA },           //  9
	{ "b90_08.7",			0x20000, 0x3404a5e3, 5 | BRF_GRA },           // 10

	{ "13.3w",			0x00020, 0xbc88cced, 0 | BRF_OPT },           // 11 PROMs
	{ "12.6b",			0x00020, 0xa1e17492, 0 | BRF_OPT },           // 12
};

STD_ROM_PICK(hellfire)
STD_ROM_FN(hellfire)

struct BurnDriver BurnDrvHellfire = {
	"hellfire", NULL, NULL, NULL, "1989",
	"Hellfire (2P set)\0", NULL, "Toaplan (Taito license)", "Toaplan BCU-2 / FCU-2 based",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TOAPLAN_RAIZING, GBF_VERSHOOT, 0,
	NULL, hellfireRomInfo, hellfireRomName, NULL, NULL, NULL, NULL, Drv2bInputInfo, HellfireDIPInfo,
	HellfireInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};


// Hellfire (1P set)

static struct BurnRomInfo hellfire1RomDesc[] = {
	{ "b90_01.10m",			0x20000, 0x034966d3, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "b90_02.9m",			0x20000, 0x06dd24c7, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "b90_03.2",			0x08000, 0x4058fa67, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "b90_04.3",			0x20000, 0xea6150fc, 3 | BRF_GRA },           //  3 Layer Tiles
	{ "b90_05.4",			0x20000, 0xbb52c507, 3 | BRF_GRA },           //  4
	{ "b90_06.5",			0x20000, 0xcf5b0252, 3 | BRF_GRA },           //  5
	{ "b90_07.6",			0x20000, 0xb98af263, 3 | BRF_GRA },           //  6

	{ "b90_11.10",			0x20000, 0xc33e543c, 5 | BRF_GRA },           //  7 Sprites
	{ "b90_10.9",			0x20000, 0x35fd1092, 5 | BRF_GRA },           //  8
	{ "b90_09.8",			0x20000, 0xcf01009e, 5 | BRF_GRA },           //  9
	{ "b90_08.7",			0x20000, 0x3404a5e3, 5 | BRF_GRA },           // 10

	{ "13.3w",			0x00020, 0xbc88cced, 0 | BRF_OPT },           // 11 PROMs
	{ "12.6b",			0x00020, 0xa1e17492, 0 | BRF_OPT },           // 12
};

STD_ROM_PICK(hellfire1)
STD_ROM_FN(hellfire1)

struct BurnDriver BurnDrvHellfire1 = {
	"hellfire1", "hellfire", NULL, NULL, "1989",
	"Hellfire (1P set)\0", NULL, "Toaplan (Taito license)", "Toaplan BCU-2 / FCU-2 based",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TOAPLAN_RAIZING, GBF_VERSHOOT, 0,
	NULL, hellfire1RomInfo, hellfire1RomName, NULL, NULL, NULL, NULL, Drv2bInputInfo, Hellfire1DIPInfo,
	HellfireInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};


// Hellfire (2P set, older)

static struct BurnRomInfo hellfire2aRomDesc[] = {
	{ "b90_01.0",			0x20000, 0xc94acf53, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "b90_02.1",			0x20000, 0xd17f03c3, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "b90_03.2",			0x08000, 0x4058fa67, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "b90_04.3",			0x20000, 0xea6150fc, 3 | BRF_GRA },           //  3 Layer Tiles
	{ "b90_05.4",			0x20000, 0xbb52c507, 3 | BRF_GRA },           //  4
	{ "b90_06.5",			0x20000, 0xcf5b0252, 3 | BRF_GRA },           //  5
	{ "b90_07.6",			0x20000, 0xb98af263, 3 | BRF_GRA },           //  6

	{ "b90_11.10",			0x20000, 0xc33e543c, 5 | BRF_GRA },           //  7 Sprites
	{ "b90_10.9",			0x20000, 0x35fd1092, 5 | BRF_GRA },           //  8
	{ "b90_09.8",			0x20000, 0xcf01009e, 5 | BRF_GRA },           //  9
	{ "b90_08.7",			0x20000, 0x3404a5e3, 5 | BRF_GRA },           // 10

	{ "13.3w",			0x00020, 0xbc88cced, 0 | BRF_OPT },           // 11 PROMs
	{ "12.6b",			0x00020, 0xa1e17492, 0 | BRF_OPT },           // 12
};

STD_ROM_PICK(hellfire2a)
STD_ROM_FN(hellfire2a)

struct BurnDriver BurnDrvHellfire2a = {
	"hellfire2a", "hellfire", NULL, NULL, "1989",
	"Hellfire (2P set, older)\0", NULL, "Toaplan (Taito license)", "Toaplan BCU-2 / FCU-2 based",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TOAPLAN_RAIZING, GBF_VERSHOOT, 0,
	NULL, hellfire2aRomInfo, hellfire2aRomName, NULL, NULL, NULL, NULL, Drv2bInputInfo, Hellfire2aDIPInfo,
	HellfireInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};


// Hellfire (1P set, older)

static struct BurnRomInfo hellfire1aRomDesc[] = {
	{ "b90_14x.0",			0x20000, 0xa3141ea5, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "b90_15x.1",			0x20000, 0xe864daf4, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "b90_03x.2",			0x08000, 0xf58c368f, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "b90_04.3",			0x20000, 0xea6150fc, 3 | BRF_GRA },           //  3 Layer Tiles
	{ "b90_05.4",			0x20000, 0xbb52c507, 3 | BRF_GRA },           //  4
	{ "b90_06.5",			0x20000, 0xcf5b0252, 3 | BRF_GRA },           //  5
	{ "b90_07.6",			0x20000, 0xb98af263, 3 | BRF_GRA },           //  6

	{ "b90_11.10",			0x20000, 0xc33e543c, 5 | BRF_GRA },           //  7 Sprites
	{ "b90_10.9",			0x20000, 0x35fd1092, 5 | BRF_GRA },           //  8
	{ "b90_09.8",			0x20000, 0xcf01009e, 5 | BRF_GRA },           //  9
	{ "b90_08.7",			0x20000, 0x3404a5e3, 5 | BRF_GRA },           // 10

	{ "13.3w",			0x00020, 0xbc88cced, 0 | BRF_OPT },           // 11 PROMs
	{ "12.6b",			0x00020, 0xa1e17492, 0 | BRF_OPT },           // 12
};

STD_ROM_PICK(hellfire1a)
STD_ROM_FN(hellfire1a)

struct BurnDriver BurnDrvHellfire1a = {
	"hellfire1a", "hellfire", NULL, NULL, "1989",
	"Hellfire (1P set, older)\0", NULL, "Toaplan (Taito license)", "Toaplan BCU-2 / FCU-2 based",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TOAPLAN_RAIZING, GBF_VERSHOOT, 0,
	NULL, hellfire1aRomInfo, hellfire1aRomName, NULL, NULL, NULL, NULL, Drv2bInputInfo, Hellfire1aDIPInfo,
	HellfireInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};


// Zero Wing (2P set)

static struct BurnRomInfo zerowingRomDesc[] = {
	{ "o15-11ii.bin",		0x08000, 0xe697ecb9, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "o15-12ii.bin",		0x08000, 0xb29ee3ad, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "o15-09.rom",			0x20000, 0x13764e95, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "o15-10.rom",			0x20000, 0x351ba71a, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "o15-13.rom",			0x08000, 0xe7b72383, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "o15-05.rom",			0x20000, 0x4e5dd246, 3 | BRF_GRA },           //  5 Layer Tiles
	{ "o15-06.rom",			0x20000, 0xc8c6d428, 3 | BRF_GRA },           //  6
	{ "o15-07.rom",			0x20000, 0xefc40e99, 3 | BRF_GRA },           //  7
	{ "o15-08.rom",			0x20000, 0x1b019eab, 3 | BRF_GRA },           //  8

	{ "o15-03.rom",			0x20000, 0x7f245fd3, 5 | BRF_GRA },           //  9 Sprites
	{ "o15-04.rom",			0x20000, 0x0b1a1289, 5 | BRF_GRA },           // 10
	{ "o15-01.rom",			0x20000, 0x70570e43, 5 | BRF_GRA },           // 11
	{ "o15-02.rom",			0x20000, 0x724b487f, 5 | BRF_GRA },           // 12

	{ "tp015_14.bpr",		0x00020, 0xbc88cced, 0 | BRF_OPT },           // 13 PROMs
	{ "tp015_15.bpr",		0x00020, 0xa1e17492, 0 | BRF_OPT },           // 14
};

STD_ROM_PICK(zerowing)
STD_ROM_FN(zerowing)

struct BurnDriver BurnDrvZerowing = {
	"zerowing", NULL, NULL, NULL, "1989",
	"Zero Wing (2P set)\0", NULL, "Toaplan", "Toaplan BCU-2 / FCU-2 based",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TOAPLAN_RAIZING, GBF_VERSHOOT, 0,
	NULL, zerowingRomInfo, zerowingRomName, NULL, NULL, NULL, NULL, Drv2bInputInfo, Zerowing2DIPInfo,
	ZerowingInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};


// Zero Wing (1P set)

static struct BurnRomInfo zerowing1RomDesc[] = {
	{ "o15-11.rom",			0x08000, 0x6ff2b9a0, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "o15-12.rom",			0x08000, 0x9773e60b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "o15-09.rom",			0x20000, 0x13764e95, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "o15-10.rom",			0x20000, 0x351ba71a, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "o15-13.rom",			0x08000, 0xe7b72383, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "o15-05.rom",			0x20000, 0x4e5dd246, 3 | BRF_GRA },           //  5 Layer Tiles
	{ "o15-06.rom",			0x20000, 0xc8c6d428, 3 | BRF_GRA },           //  6
	{ "o15-07.rom",			0x20000, 0xefc40e99, 3 | BRF_GRA },           //  7
	{ "o15-08.rom",			0x20000, 0x1b019eab, 3 | BRF_GRA },           //  8

	{ "o15-03.rom",			0x20000, 0x7f245fd3, 5 | BRF_GRA },           //  9 Sprites
	{ "o15-04.rom",			0x20000, 0x0b1a1289, 5 | BRF_GRA },           // 10
	{ "o15-01.rom",			0x20000, 0x70570e43, 5 | BRF_GRA },           // 11
	{ "o15-02.rom",			0x20000, 0x724b487f, 5 | BRF_GRA },           // 12

	{ "tp015_14.bpr",		0x00020, 0xbc88cced, 0 | BRF_OPT },           // 13 PROMs
	{ "tp015_15.bpr",		0x00020, 0xa1e17492, 0 | BRF_OPT },           // 14
};

STD_ROM_PICK(zerowing1)
STD_ROM_FN(zerowing1)

struct BurnDriver BurnDrvZerowing1 = {
	"zerowing1", "zerowing", NULL, NULL, "1989",
	"Zero Wing (1P set)\0", NULL, "Toaplan", "Toaplan BCU-2 / FCU-2 based",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TOAPLAN_RAIZING, GBF_VERSHOOT, 0,
	NULL, zerowing1RomInfo, zerowing1RomName, NULL, NULL, NULL, NULL, Drv2bInputInfo, ZerowingDIPInfo,
	ZerowingInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};


// Zero Wing (2P set, Williams license)

static struct BurnRomInfo zerowingwRomDesc[] = {
	{ "o15-11iiw.bin",		0x08000, 0x38b0bb5b, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "o15-12iiw.bin",		0x08000, 0x74c91e6f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "o15-09.rom",			0x20000, 0x13764e95, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "o15-10.rom",			0x20000, 0x351ba71a, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "o15-13.rom",			0x08000, 0xe7b72383, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "o15-05.rom",			0x20000, 0x4e5dd246, 3 | BRF_GRA },           //  5 Layer Tiles
	{ "o15-06.rom",			0x20000, 0xc8c6d428, 3 | BRF_GRA },           //  6
	{ "o15-07.rom",			0x20000, 0xefc40e99, 3 | BRF_GRA },           //  7
	{ "o15-08.rom",			0x20000, 0x1b019eab, 3 | BRF_GRA },           //  8

	{ "o15-03.rom",			0x20000, 0x7f245fd3, 5 | BRF_GRA },           //  9 Sprites
	{ "o15-04.rom",			0x20000, 0x0b1a1289, 5 | BRF_GRA },           // 10
	{ "o15-01.rom",			0x20000, 0x70570e43, 5 | BRF_GRA },           // 11
	{ "o15-02.rom",			0x20000, 0x724b487f, 5 | BRF_GRA },           // 12

	{ "tp015_14.bpr",		0x00020, 0xbc88cced, 0 | BRF_OPT },           // 13 PROMs
	{ "tp015_15.bpr",		0x00020, 0xa1e17492, 0 | BRF_OPT },           // 14
};

STD_ROM_PICK(zerowingw)
STD_ROM_FN(zerowingw)

struct BurnDriver BurnDrvZerowingw = {
	"zerowingw", "zerowing", NULL, NULL, "1989",
	"Zero Wing (2P set, Williams license)\0", NULL, "Toaplan (Williams license)", "Toaplan BCU-2 / FCU-2 based",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TOAPLAN_RAIZING, GBF_VERSHOOT, 0,
	NULL, zerowingwRomInfo, zerowingwRomName, NULL, NULL, NULL, NULL, Drv2bInputInfo, Zerowing2DIPInfo,
	ZerowingInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};


// Demon's World / Horror Story (set 1)

static struct BurnRomInfo demonwldRomDesc[] = {
	{ "o16-10.v2",			0x20000, 0xca8194f3, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "o16-09.v2",			0x20000, 0x7baea7ba, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "rom11.v2",			0x08000, 0xdbe08c85, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "dsp_21.bin",			0x00800, 0x2d135376, 7 | BRF_PRG | BRF_ESS }, //  3 DSP Code
	{ "dsp_22.bin",			0x00800, 0x79389a71, 7 | BRF_PRG | BRF_ESS }, //  4

	{ "rom05",			0x20000, 0x6506c982, 3 | BRF_GRA },           //  5 Layer Tiles
	{ "rom07",			0x20000, 0xa3a0d993, 3 | BRF_GRA },           //  6
	{ "rom06",			0x20000, 0x4fc5e5f3, 3 | BRF_GRA },           //  7
	{ "rom08",			0x20000, 0xeb53ab09, 3 | BRF_GRA },           //  8

	{ "rom01",			0x20000, 0x1b3724e9, 5 | BRF_GRA },           //  9 Sprites
	{ "rom02",			0x20000, 0x7b20a44d, 5 | BRF_GRA },           // 10
	{ "rom03",			0x20000, 0x2cacdcd0, 5 | BRF_GRA },           // 11
	{ "rom04",			0x20000, 0x76fd3201, 5 | BRF_GRA },           // 12

	{ "prom12.bpr",			0x00020, 0xbc88cced, 0 | BRF_OPT },           // 13 PROMs
	{ "prom13.bpr",			0x00020, 0xa1e17492, 0 | BRF_OPT },           // 14
};

STD_ROM_PICK(demonwld)
STD_ROM_FN(demonwld)

struct BurnDriver BurnDrvDemonwld = {
	"demonwld", NULL, NULL, NULL, "1990",
	"Demon's World / Horror Story (set 1)\0", NULL, "Toaplan", "Toaplan BCU-2 / FCU-2 based",
	L"Demon's World\0\u30DB\u30E9\u30FC\u30B9\u30C8\u30FC\u30EA\u30FC (set 1)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TOAPLAN_RAIZING, GBF_RUNGUN, 0,
	NULL, demonwldRomInfo, demonwldRomName, NULL, NULL, NULL, NULL, Drv3bInputInfo, DemonwldDIPInfo,
	DemonwldInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};


// Demon's World / Horror Story (set 2)

static struct BurnRomInfo demonwld1RomDesc[] = {
	{ "o16n-10.bin",		0x20000, 0xfc38aeaa, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "o16n-09.bin",		0x20000, 0x74f66643, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "o16-11.bin",			0x08000, 0xdbe08c85, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "dsp_21.bin",			0x00800, 0x2d135376, 7 | BRF_PRG | BRF_ESS }, //  3 DSP Code
	{ "dsp_22.bin",			0x00800, 0x79389a71, 7 | BRF_PRG | BRF_ESS }, //  4

	{ "rom05",			0x20000, 0x6506c982, 3 | BRF_GRA },           //  5 Layer Tiles
	{ "rom07",			0x20000, 0xa3a0d993, 3 | BRF_GRA },           //  6
	{ "rom06",			0x20000, 0x4fc5e5f3, 3 | BRF_GRA },           //  7
	{ "rom08",			0x20000, 0xeb53ab09, 3 | BRF_GRA },           //  8

	{ "rom01",			0x20000, 0x1b3724e9, 5 | BRF_GRA },           //  9 Sprites
	{ "rom02",			0x20000, 0x7b20a44d, 5 | BRF_GRA },           // 10
	{ "rom03",			0x20000, 0x2cacdcd0, 5 | BRF_GRA },           // 11
	{ "rom04",			0x20000, 0x76fd3201, 5 | BRF_GRA },           // 12

	{ "prom12.bpr",			0x00020, 0xbc88cced, 0 | BRF_OPT },           // 13 PROMs
	{ "prom13.bpr",			0x00020, 0xa1e17492, 0 | BRF_OPT },           // 14
};

STD_ROM_PICK(demonwld1)
STD_ROM_FN(demonwld1)

struct BurnDriver BurnDrvDemonwld1 = {
	"demonwld1", "demonwld", NULL, NULL, "1989",
	"Demon's World / Horror Story (set 2)\0", NULL, "Toaplan", "Toaplan BCU-2 / FCU-2 based",
	L"Demon's World\0\u30DB\u30E9\u30FC\u30B9\u30C8\u30FC\u30EA\u30FC (set 2)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TOAPLAN_RAIZING, GBF_RUNGUN, 0,
	NULL, demonwld1RomInfo, demonwld1RomName, NULL, NULL, NULL, NULL, Drv3bInputInfo, DemonwldDIPInfo,
	DemonwldInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};


// Demon's World / Horror Story (set 3)

static struct BurnRomInfo demonwld2RomDesc[] = {
	{ "o16-10.rom",			0x20000, 0x036ee46c, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "o16-09.rom",			0x20000, 0xbed746e3, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "rom11",			0x08000, 0x397eca1b, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "dsp_21.bin",			0x00800, 0x2d135376, 7 | BRF_PRG | BRF_ESS }, //  3 DSP Code
	{ "dsp_22.bin",			0x00800, 0x79389a71, 7 | BRF_PRG | BRF_ESS }, //  4

	{ "rom05",			0x20000, 0x6506c982, 3 | BRF_GRA },           //  5 Layer Tiles
	{ "rom07",			0x20000, 0xa3a0d993, 3 | BRF_GRA },           //  6
	{ "rom06",			0x20000, 0x4fc5e5f3, 3 | BRF_GRA },           //  7
	{ "rom08",			0x20000, 0xeb53ab09, 3 | BRF_GRA },           //  8

	{ "rom01",			0x20000, 0x1b3724e9, 5 | BRF_GRA },           //  9 Sprites
	{ "rom02",			0x20000, 0x7b20a44d, 5 | BRF_GRA },           // 10
	{ "rom03",			0x20000, 0x2cacdcd0, 5 | BRF_GRA },           // 11
	{ "rom04",			0x20000, 0x76fd3201, 5 | BRF_GRA },           // 12

	{ "prom12.bpr",			0x00020, 0xbc88cced, 0 | BRF_OPT },           // 13 PROMs
	{ "prom13.bpr",			0x00020, 0xa1e17492, 0 | BRF_OPT },           // 14
};

STD_ROM_PICK(demonwld2)
STD_ROM_FN(demonwld2)

struct BurnDriver BurnDrvDemonwld2 = {
	"demonwld2", "demonwld", NULL, NULL, "1989",
	"Demon's World / Horror Story (set 3)\0", NULL, "Toaplan", "Toaplan BCU-2 / FCU-2 based",
	L"Demon's World\0\u30DB\u30E9\u30FC\u30B9\u30C8\u30FC\u30EA\u30FC (set 3)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TOAPLAN_RAIZING, GBF_RUNGUN, 0,
	NULL, demonwld2RomInfo, demonwld2RomName, NULL, NULL, NULL, NULL, Drv3bInputInfo, Demonwld1DIPInfo,
	DemonwldInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};


// Demon's World / Horror Story (set 4)

static struct BurnRomInfo demonwld3RomDesc[] = {
	{ "o16-10-2.bin",		0x20000, 0x84ee5218, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "o16-09-2.bin",		0x20000, 0xcf474cb2, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "rom11",			0x08000, 0x397eca1b, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "dsp_21.bin",			0x00800, 0x2d135376, 7 | BRF_PRG | BRF_ESS }, //  3 DSP Code
	{ "dsp_22.bin",			0x00800, 0x79389a71, 7 | BRF_PRG | BRF_ESS }, //  4

	{ "rom05",			0x20000, 0x6506c982, 3 | BRF_GRA },           //  5 Layer Tiles
	{ "rom07",			0x20000, 0xa3a0d993, 3 | BRF_GRA },           //  6
	{ "rom06",			0x20000, 0x4fc5e5f3, 3 | BRF_GRA },           //  7
	{ "rom08",			0x20000, 0xeb53ab09, 3 | BRF_GRA },           //  8

	{ "rom01",			0x20000, 0x1b3724e9, 5 | BRF_GRA },           //  9 Sprites
	{ "rom02",			0x20000, 0x7b20a44d, 5 | BRF_GRA },           // 10
	{ "rom03",			0x20000, 0x2cacdcd0, 5 | BRF_GRA },           // 11
	{ "rom04",			0x20000, 0x76fd3201, 5 | BRF_GRA },           // 12

	{ "prom12.bpr",			0x00020, 0xbc88cced, 0 | BRF_OPT },           // 13 PROMs
	{ "prom13.bpr",			0x00020, 0xa1e17492, 0 | BRF_OPT },           // 14
};

STD_ROM_PICK(demonwld3)
STD_ROM_FN(demonwld3)

struct BurnDriver BurnDrvDemonwld3 = {
	"demonwld3", "demonwld", NULL, NULL, "1989",
	"Demon's World / Horror Story (set 4)\0", NULL, "Toaplan", "Toaplan BCU-2 / FCU-2 based",
	L"Demon's World\0\u30DB\u30E9\u30FC\u30B9\u30C8\u30FC\u30EA\u30FC (set 4)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TOAPLAN_RAIZING, GBF_RUNGUN, 0,
	NULL, demonwld3RomInfo, demonwld3RomName, NULL, NULL, NULL, NULL, Drv3bInputInfo, Demonwld1DIPInfo,
	DemonwldInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};


// Demon's World / Horror Story (set 5)

static struct BurnRomInfo demonwld4RomDesc[] = {
	{ "o16-10.bin",			0x20000, 0x6f7468e0, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "o16-09.bin",			0x20000, 0xa572f5f7, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "rom11",			0x08000, 0x397eca1b, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "dsp_21.bin",			0x00800, 0x2d135376, 7 | BRF_PRG | BRF_ESS }, //  3 DSP Code
	{ "dsp_22.bin",			0x00800, 0x79389a71, 7 | BRF_PRG | BRF_ESS }, //  4

	{ "rom05",			0x20000, 0x6506c982, 3 | BRF_GRA },           //  5 Layer Tiles
	{ "rom07",			0x20000, 0xa3a0d993, 3 | BRF_GRA },           //  6
	{ "rom06",			0x20000, 0x4fc5e5f3, 3 | BRF_GRA },           //  7
	{ "rom08",			0x20000, 0xeb53ab09, 3 | BRF_GRA },           //  8

	{ "rom01",			0x20000, 0x1b3724e9, 5 | BRF_GRA },           //  9 Sprites
	{ "rom02",			0x20000, 0x7b20a44d, 5 | BRF_GRA },           // 10
	{ "rom03",			0x20000, 0x2cacdcd0, 5 | BRF_GRA },           // 11
	{ "rom04",			0x20000, 0x76fd3201, 5 | BRF_GRA },           // 12

	{ "prom12.bpr",			0x00020, 0xbc88cced, 0 | BRF_OPT },           // 13 PROMs
	{ "prom13.bpr",			0x00020, 0xa1e17492, 0 | BRF_OPT },           // 14
};

STD_ROM_PICK(demonwld4)
STD_ROM_FN(demonwld4)

struct BurnDriver BurnDrvDemonwld4 = {
	"demonwld4", "demonwld", NULL, NULL, "1989",
	"Demon's World / Horror Story (set 5)\0", NULL, "Toaplan", "Toaplan BCU-2 / FCU-2 based",
	L"Demon's World\0\u30DB\u30E9\u30FC\u30B9\u30C8\u30FC\u30EA\u30FC (set 5)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TOAPLAN_RAIZING, GBF_RUNGUN, 0,
	NULL, demonwld4RomInfo, demonwld4RomName, NULL, NULL, NULL, NULL, Drv3bInputInfo, Demonwld1DIPInfo,
	DemonwldInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};


// Same! Same! Same! (1P set)

static struct BurnRomInfo samesameRomDesc[] = {
	{ "o17_09.8j",			0x08000, 0x3f69e437, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "o17_10.8l",			0x08000, 0x4e723e0a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "o17_11.7j",			0x20000, 0xbe07d101, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "o17_12.7l",			0x20000, 0xef698811, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "hd647180.017",		0x08000, 0x43523032, 2 | BRF_PRG | BRF_ESS }, //  4 Z180 Code

	{ "o17_05.12j",			0x20000, 0x565315f8, 3 | BRF_GRA },           //  5 Layer Tiles
	{ "o17_06.13j",			0x20000, 0x95262d4c, 3 | BRF_GRA },           //  6
	{ "o17_07.12l",			0x20000, 0x4c4b735c, 3 | BRF_GRA },           //  7
	{ "o17_08.13l",			0x20000, 0x95c6586c, 3 | BRF_GRA },           //  8

	{ "o17_01.1d",			0x20000, 0xea12e491, 5 | BRF_GRA },           //  9 Sprites
	{ "o17_02.3d",			0x20000, 0x32a13a9f, 5 | BRF_GRA },           // 10
	{ "o17_03.5d",			0x20000, 0x68723dc9, 5 | BRF_GRA },           // 11
	{ "o17_04.7d",			0x20000, 0xfe0ecb13, 5 | BRF_GRA },           // 12

	{ "prom14.25b",			0x00020, 0xbc88cced, 0 | BRF_OPT },           // 13 PROMs
	{ "prom15.20c",			0x00020, 0xa1e17492, 0 | BRF_OPT },           // 14
};

STD_ROM_PICK(samesame)
STD_ROM_FN(samesame)

struct BurnDriver BurnDrvSamesame = {
	"samesame", "fireshrk", NULL, NULL, "1989",
	"Same! Same! Same! (1P set)\0", NULL, "Toaplan", "Toaplan BCU-2 / FCU-2 based",
	L"\u9BAB!\u9BAB!\u9BAB!\0Same! Same! Same! (1P set)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TOAPLAN_RAIZING, GBF_VERSHOOT, 0,
	NULL, samesameRomInfo, samesameRomName, NULL, NULL, NULL, NULL, FiresharkInputInfo, SamesameDIPInfo,
	SamesameInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Same! Same! Same! (2P set)

static struct BurnRomInfo samesame2RomDesc[] = {
	{ "o17_09x.8j",			0x08000, 0x3472e03e, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "o17_10x.8l",			0x08000, 0xa3ac49b5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "o17_11ii.7j",		0x20000, 0x6beac378, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "o17_12ii.7l",		0x20000, 0x6adb6eb5, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "hd647180.017",		0x08000, 0x43523032, 2 | BRF_PRG | BRF_ESS }, //  4 Z180 Code

	{ "o17_05.12j",			0x20000, 0x565315f8, 3 | BRF_GRA },           //  5 Layer Tiles
	{ "o17_06.13j",			0x20000, 0x95262d4c, 3 | BRF_GRA },           //  6
	{ "o17_07.12l",			0x20000, 0x4c4b735c, 3 | BRF_GRA },           //  7
	{ "o17_08.13l",			0x20000, 0x95c6586c, 3 | BRF_GRA },           //  8

	{ "o17_01.1d",			0x20000, 0xea12e491, 5 | BRF_GRA },           //  9 Sprites
	{ "o17_02.3d",			0x20000, 0x32a13a9f, 5 | BRF_GRA },           // 10
	{ "o17_03.5d",			0x20000, 0x68723dc9, 5 | BRF_GRA },           // 11
	{ "o17_04.7d",			0x20000, 0xfe0ecb13, 5 | BRF_GRA },           // 12

	{ "prom14.25b",			0x00020, 0xbc88cced, 0 | BRF_OPT },           // 13 PROMs
	{ "prom15.20c",			0x00020, 0xa1e17492, 0 | BRF_OPT },           // 14
};

STD_ROM_PICK(samesame2)
STD_ROM_FN(samesame2)

struct BurnDriver BurnDrvSamesame2 = {
	"samesame2", "fireshrk", NULL, NULL, "1989",
	"Same! Same! Same! (2P set)\0", NULL, "Toaplan", "Toaplan BCU-2 / FCU-2 based",
	L"\u9BAB!\u9BAB!\u9BAB!\0Same! Same! Same!\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TOAPLAN_RAIZING, GBF_VERSHOOT, 0,
	NULL, samesame2RomInfo, samesame2RomName, NULL, NULL, NULL, NULL, FiresharkInputInfo, Samesame2DIPInfo,
	SamesameInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Same! Same! Same! (1P set, NEW VER! hack)

static struct BurnRomInfo samesamenhRomDesc[] = {
	{ "o17_09_nv.8j",		0x08000, 0xf60af2f9, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "o17_10_nv.8l",		0x08000, 0x023bcb95, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "o17_11.7j",			0x20000, 0xbe07d101, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "o17_12.7l",			0x20000, 0xef698811, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "hd647180.017",		0x08000, 0x43523032, 2 | BRF_PRG | BRF_ESS }, //  4 Z180 Code

	{ "o17_05.12j",			0x20000, 0x565315f8, 3 | BRF_GRA },           //  5 Layer Tiles
	{ "o17_06.13j",			0x20000, 0x95262d4c, 3 | BRF_GRA },           //  6
	{ "o17_07.12l",			0x20000, 0x4c4b735c, 3 | BRF_GRA },           //  7
	{ "o17_08.13l",			0x20000, 0x95c6586c, 3 | BRF_GRA },           //  8

	{ "o17_01.1d",			0x20000, 0xea12e491, 5 | BRF_GRA },           //  9 Sprites
	{ "o17_02.3d",			0x20000, 0x32a13a9f, 5 | BRF_GRA },           // 10
	{ "o17_03.5d",			0x20000, 0x68723dc9, 5 | BRF_GRA },           // 11
	{ "o17_04.7d",			0x20000, 0xfe0ecb13, 5 | BRF_GRA },           // 12

	{ "prom14.25b",			0x00020, 0xbc88cced, 0 | BRF_OPT },           // 13 PROMs
	{ "prom15.20c",			0x00020, 0xa1e17492, 0 | BRF_OPT },           // 14
};

STD_ROM_PICK(samesamenh)
STD_ROM_FN(samesamenh)

struct BurnDriver BurnDrvSamesamenh = {
	"samesamenh", "fireshrk", NULL, NULL, "2015",
	"Same! Same! Same! (1P set, NEW VER! hack)\0", NULL, "hack (trap15)", "Toaplan BCU-2 / FCU-2 based",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TOAPLAN_RAIZING, GBF_VERSHOOT, 0,
	NULL, samesamenhRomInfo, samesamenhRomName, NULL, NULL, NULL, NULL, FiresharkInputInfo, SamesameDIPInfo,
	SamesameInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Fire Shark

static struct BurnRomInfo fireshrkRomDesc[] = {
	{ "09.8j",			0x08000, 0xf0c70e6f, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "10.8l",			0x08000, 0x9d253d77, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "o17_11ii.7j",		0x20000, 0x6beac378, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "o17_12ii.7l",		0x20000, 0x6adb6eb5, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "hd647180.017",		0x08000, 0x43523032, 2 | BRF_PRG | BRF_ESS }, //  4 Z180 Code

	{ "o17_05.12j",			0x20000, 0x565315f8, 3 | BRF_GRA },           //  5 Layer Tiles
	{ "o17_06.13j",			0x20000, 0x95262d4c, 3 | BRF_GRA },           //  6
	{ "o17_07.12l",			0x20000, 0x4c4b735c, 3 | BRF_GRA },           //  7
	{ "o17_08.13l",			0x20000, 0x95c6586c, 3 | BRF_GRA },           //  8

	{ "o17_01.1d",			0x20000, 0xea12e491, 5 | BRF_GRA },           //  9 Sprites
	{ "o17_02.3d",			0x20000, 0x32a13a9f, 5 | BRF_GRA },           // 10
	{ "o17_03.5d",			0x20000, 0x68723dc9, 5 | BRF_GRA },           // 11
	{ "o17_04.7d",			0x20000, 0xfe0ecb13, 5 | BRF_GRA },           // 12

	{ "prom14.25b",			0x00020, 0xbc88cced, 0 | BRF_OPT },           // 13 PROMs
	{ "prom15.20c",			0x00020, 0xa1e17492, 0 | BRF_OPT },           // 14
};

STD_ROM_PICK(fireshrk)
STD_ROM_FN(fireshrk)

struct BurnDriver BurnDrvFireshrk = {
	"fireshrk", NULL, NULL, NULL, "1990",
	"Fire Shark\0", NULL, "Toaplan", "Toaplan BCU-2 / FCU-2 based",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TOAPLAN_RAIZING, GBF_VERSHOOT, 0,
	NULL, fireshrkRomInfo, fireshrkRomName, NULL, NULL, NULL, NULL, FiresharkInputInfo, FireshrkDIPInfo,
	SamesameInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Fire Shark (earlier)

static struct BurnRomInfo fireshrkaRomDesc[] = {
	{ "o17_09ii.8j",		0x08000, 0xb60541ee, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "o17_10ii.8l",		0x08000, 0x96f5045e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "o17_11ii.7j",		0x20000, 0x6beac378, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "o17_12ii.7l",		0x20000, 0x6adb6eb5, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "hd647180.017",		0x08000, 0x43523032, 2 | BRF_PRG | BRF_ESS }, //  4 Z180 Code

	{ "o17_05.12j",			0x20000, 0x565315f8, 3 | BRF_GRA },           //  5 Layer Tiles
	{ "o17_06.13j",			0x20000, 0x95262d4c, 3 | BRF_GRA },           //  6
	{ "o17_07.12l",			0x20000, 0x4c4b735c, 3 | BRF_GRA },           //  7
	{ "o17_08.13l",			0x20000, 0x95c6586c, 3 | BRF_GRA },           //  8

	{ "o17_01.1d",			0x20000, 0xea12e491, 5 | BRF_GRA },           //  9 Sprites
	{ "o17_02.3d",			0x20000, 0x32a13a9f, 5 | BRF_GRA },           // 10
	{ "o17_03.5d",			0x20000, 0x68723dc9, 5 | BRF_GRA },           // 11
	{ "o17_04.7d",			0x20000, 0xfe0ecb13, 5 | BRF_GRA },           // 12

	{ "prom14.25b",			0x00020, 0xbc88cced, 0 | BRF_OPT },           // 13 PROMs
	{ "prom15.20c",			0x00020, 0xa1e17492, 0 | BRF_OPT },           // 14
};

STD_ROM_PICK(fireshrka)
STD_ROM_FN(fireshrka)

struct BurnDriver BurnDrvFireshrka = {
	"fireshrka", "fireshrk", NULL, NULL, "1989",
	"Fire Shark (earlier)\0", NULL, "Toaplan", "Toaplan BCU-2 / FCU-2 based",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TOAPLAN_RAIZING, GBF_VERSHOOT, 0,
	NULL, fireshrkaRomInfo, fireshrkaRomName, NULL, NULL, NULL, NULL, FiresharkInputInfo, FireshrkaDIPInfo,
	SamesameInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Fire Shark (Korea, set 1, easier)

static struct BurnRomInfo fireshrkdRomDesc[] = {
	{ "o17_09dyn.8j",		0x10000, 0xe25eee27, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "o17_10dyn.8l",		0x10000, 0xc4c58cf6, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "o17_11ii.7j",		0x20000, 0x6beac378, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "o17_12ii.7l",		0x20000, 0x6adb6eb5, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "hd647180.017",		0x08000, 0x43523032, 2 | BRF_PRG | BRF_ESS }, //  4 Z180 Code

	{ "o17_05.12j",			0x20000, 0x565315f8, 3 | BRF_GRA },           //  5 Layer Tiles
	{ "o17_06.13j",			0x20000, 0x95262d4c, 3 | BRF_GRA },           //  6
	{ "o17_07.12l",			0x20000, 0x4c4b735c, 3 | BRF_GRA },           //  7
	{ "o17_08.13l",			0x20000, 0x95c6586c, 3 | BRF_GRA },           //  8

	{ "o17_01.1d",			0x20000, 0xea12e491, 5 | BRF_GRA },           //  9 Sprites
	{ "o17_02.3d",			0x20000, 0x32a13a9f, 5 | BRF_GRA },           // 10
	{ "o17_03.5d",			0x20000, 0x68723dc9, 5 | BRF_GRA },           // 11
	{ "o17_04.7d",			0x20000, 0xfe0ecb13, 5 | BRF_GRA },           // 12

	{ "prom14.25b",			0x00020, 0xbc88cced, 0 | BRF_OPT },           // 13 PROMs
	{ "prom15.20c",			0x00020, 0xa1e17492, 0 | BRF_OPT },           // 14
};

STD_ROM_PICK(fireshrkd)
STD_ROM_FN(fireshrkd)

struct BurnDriver BurnDrvFireshrkd = {
	"fireshrkd", "fireshrk", NULL, NULL, "1990",
	"Fire Shark (Korea, set 1, easier)\0", NULL, "Toaplan (Dooyong license)", "Toaplan BCU-2 / FCU-2 based",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TOAPLAN_RAIZING, GBF_VERSHOOT, 0,
	NULL, fireshrkdRomInfo, fireshrkdRomName, NULL, NULL, NULL, NULL, FiresharkInputInfo, Samesame2DIPInfo,
	SamesameInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Fire Shark (Korea, set 2, harder)

static struct BurnRomInfo fireshrkdhRomDesc[] = {
	{ "o17_09dyh.8j",		0x10000, 0x7b4c14dd, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "o17_10dyh.8l",		0x10000, 0xa3f159f9, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "o17_11ii.7j",		0x20000, 0x6beac378, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "o17_12ii.7l",		0x20000, 0x6adb6eb5, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "hd647180.017",		0x08000, 0x43523032, 2 | BRF_PRG | BRF_ESS }, //  4 Z180 Code

	{ "o17_05.12j",			0x20000, 0x565315f8, 3 | BRF_GRA },           //  5 Layer Tiles
	{ "o17_06.13j",			0x20000, 0x95262d4c, 3 | BRF_GRA },           //  6
	{ "o17_07.12l",			0x20000, 0x4c4b735c, 3 | BRF_GRA },           //  7
	{ "o17_08.13l",			0x20000, 0x95c6586c, 3 | BRF_GRA },           //  8

	{ "o17_01.1d",			0x20000, 0xea12e491, 5 | BRF_GRA },           //  9 Sprites
	{ "o17_02.3d",			0x20000, 0x32a13a9f, 5 | BRF_GRA },           // 10
	{ "o17_03.5d",			0x20000, 0x68723dc9, 5 | BRF_GRA },           // 11
	{ "o17_04.7d",			0x20000, 0xfe0ecb13, 5 | BRF_GRA },           // 12

	{ "prom14.25b",			0x00020, 0xbc88cced, 0 | BRF_OPT },           // 13 PROMs
	{ "prom15.20c",			0x00020, 0xa1e17492, 0 | BRF_OPT },           // 14
};

STD_ROM_PICK(fireshrkdh)
STD_ROM_FN(fireshrkdh)

struct BurnDriver BurnDrvFireshrkdh = {
	"fireshrkdh", "fireshrk", NULL, NULL, "1990",
	"Fire Shark (Korea, set 2, harder)\0", NULL, "Toaplan (Dooyong license)", "Toaplan BCU-2 / FCU-2 based",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TOAPLAN_RAIZING, GBF_VERSHOOT, 0,
	NULL, fireshrkdhRomInfo, fireshrkdhRomName, NULL, NULL, NULL, NULL, FiresharkInputInfo, Samesame2DIPInfo,
	SamesameInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Out Zone

static struct BurnRomInfo outzoneRomDesc[] = {
	{ "tp_018_08.bin",		0x20000, 0x127a38d7, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "tp_018_07.bin",		0x20000, 0x9704db16, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "tp_018_09.bin",		0x08000, 0x73d8e235, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "tp-018_rom5.bin",	0x80000, 0xc64ec7b6, 4 | BRF_GRA },           //  3 Layer Tiles
	{ "tp-018_rom6.bin",	0x80000, 0x64b6c5ac, 4 | BRF_GRA },           //  4

	{ "tp-018_rom2.bin",	0x20000, 0x6bb72d16, 5 | BRF_GRA },           //  5 Sprites
	{ "tp-018_rom1.bin",	0x20000, 0x0934782d, 5 | BRF_GRA },           //  6
	{ "tp-018_rom3.bin",	0x20000, 0xec903c07, 5 | BRF_GRA },           //  7
	{ "tp-018_rom4.bin",	0x20000, 0x50cbf1a8, 5 | BRF_GRA },           //  8

	{ "tp018_10.bpr",		0x00020, 0xbc88cced, 0 | BRF_OPT },           //  9 PROMs
	{ "tp018_11.bpr",		0x00020, 0xa1e17492, 0 | BRF_OPT },           // 10
};

STD_ROM_PICK(outzone)
STD_ROM_FN(outzone)

struct BurnDriver BurnDrvOutzone = {
	"outzone", NULL, NULL, NULL, "1990",
	"Out Zone\0", NULL, "Toaplan", "Toaplan BCU-2 / FCU-2 based",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TOAPLAN_RAIZING, GBF_RUNGUN, 0,
	NULL, outzoneRomInfo, outzoneRomName, NULL, NULL, NULL, NULL, Drv3bInputInfo, OutzoneDIPInfo,
	OutzoneInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Out Zone (harder)

static struct BurnRomInfo outzonehRomDesc[] = {
	{ "tp_018_07h.bin",		0x20000, 0x0c2ac02d, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "tp_018_08h.bin",		0x20000, 0xca7e48aa, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "tp_018_09.bin",		0x08000, 0x73d8e235, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "tp-018_rom5.bin",	0x80000, 0xc64ec7b6, 4 | BRF_GRA },           //  3 Layer Tiles
	{ "tp-018_rom6.bin",	0x80000, 0x64b6c5ac, 4 | BRF_GRA },           //  4

	{ "tp-018_rom2.bin",	0x20000, 0x6bb72d16, 5 | BRF_GRA },           //  5 Sprites
	{ "tp-018_rom1.bin",	0x20000, 0x0934782d, 5 | BRF_GRA },           //  6
	{ "tp-018_rom3.bin",	0x20000, 0xec903c07, 5 | BRF_GRA },           //  7
	{ "tp-018_rom4.bin",	0x20000, 0x50cbf1a8, 5 | BRF_GRA },           //  8

	{ "tp018_10.bpr",		0x00020, 0xbc88cced, 0 | BRF_OPT },           //  9 PROMs
	{ "tp018_11.bpr",		0x00020, 0xa1e17492, 0 | BRF_OPT },           // 10
};

STD_ROM_PICK(outzoneh)
STD_ROM_FN(outzoneh)

struct BurnDriver BurnDrvOutzoneh = {
	"outzoneh", "outzone", NULL, NULL, "1990",
	"Out Zone (harder)\0", NULL, "Toaplan", "Toaplan BCU-2 / FCU-2 based",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TOAPLAN_RAIZING, GBF_RUNGUN, 0,
	NULL, outzonehRomInfo, outzonehRomName, NULL, NULL, NULL, NULL, Drv3bInputInfo, OutzoneDIPInfo,
	OutzoneInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Out Zone (old set)

static struct BurnRomInfo outzoneaRomDesc[] = {
	{ "18.bin",			0x20000, 0x31a171bb, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "19.bin",			0x20000, 0x804ecfd1, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "tp_018_09.bin",		0x08000, 0x73d8e235, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "tp-018_rom5.bin",	0x80000, 0xc64ec7b6, 4 | BRF_GRA },           //  3 Layer Tiles
	{ "tp-018_rom6.bin",	0x80000, 0x64b6c5ac, 4 | BRF_GRA },           //  4

	{ "tp-018_rom2.bin",	0x20000, 0x6bb72d16, 5 | BRF_GRA },           //  5 Sprites
	{ "tp-018_rom1.bin",	0x20000, 0x0934782d, 5 | BRF_GRA },           //  6
	{ "tp-018_rom3.bin",	0x20000, 0xec903c07, 5 | BRF_GRA },           //  7
	{ "tp-018_rom4.bin",	0x20000, 0x50cbf1a8, 5 | BRF_GRA },           //  8

	{ "tp018_10.bpr",		0x00020, 0xbc88cced, 0 | BRF_OPT },           //  9 PROMs
	{ "tp018_11.bpr",		0x00020, 0xa1e17492, 0 | BRF_OPT },           // 10
};

STD_ROM_PICK(outzonea)
STD_ROM_FN(outzonea)

struct BurnDriver BurnDrvOutzonea = {
	"outzonea", "outzone", NULL, NULL, "1990",
	"Out Zone (old set)\0", NULL, "Toaplan", "Toaplan BCU-2 / FCU-2 based",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TOAPLAN_RAIZING, GBF_RUNGUN, 0,
	NULL, outzoneaRomInfo, outzoneaRomName, NULL, NULL, NULL, NULL, Drv3bInputInfo, OutzoneaDIPInfo,
	OutzoneInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Out Zone (older set)

static struct BurnRomInfo outzonebRomDesc[] = {
	{ "tp07.bin",			0x20000, 0xa85a1d48, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "tp08.bin",			0x20000, 0xd8cc44af, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "tp09.bin",			0x08000, 0xdd56041f, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "tp-018_rom5.bin",	0x80000, 0xc64ec7b6, 4 | BRF_GRA },           //  3 Layer Tiles
	{ "tp-018_rom6.bin",	0x80000, 0x64b6c5ac, 4 | BRF_GRA },           //  4

	{ "tp-018_rom2.bin",	0x20000, 0x6bb72d16, 5 | BRF_GRA },           //  5 Sprites
	{ "tp-018_rom1.bin",	0x20000, 0x0934782d, 5 | BRF_GRA },           //  6
	{ "tp-018_rom3.bin",	0x20000, 0xec903c07, 5 | BRF_GRA },           //  7
	{ "tp-018_rom4.bin",	0x20000, 0x50cbf1a8, 5 | BRF_GRA },           //  8

	{ "tp018_10.bpr",		0x00020, 0xbc88cced, 0 | BRF_OPT },           //  9 PROMs
	{ "tp018_11.bpr",		0x00020, 0xa1e17492, 0 | BRF_OPT },           // 10
};

STD_ROM_PICK(outzoneb)
STD_ROM_FN(outzoneb)

struct BurnDriver BurnDrvOutzoneb = {
	"outzoneb", "outzone", NULL, NULL, "1990",
	"Out Zone (older set)\0", NULL, "Toaplan", "Toaplan BCU-2 / FCU-2 based",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TOAPLAN_RAIZING, GBF_RUNGUN, 0,
	NULL, outzonebRomInfo, outzonebRomName, NULL, NULL, NULL, NULL, Drv3bInputInfo, OutzoneaDIPInfo,
	OutzoneInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Out Zone (oldest set)

static struct BurnRomInfo outzonecRomDesc[] = {
	{ "rom7.bin",			0x20000, 0x936e25d8, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "rom8.bin",			0x20000, 0xd19b3ecf, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "tp_018_09.bin",		0x08000, 0x73d8e235, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "tp-018_rom5.bin",	0x80000, 0xc64ec7b6, 4 | BRF_GRA },           //  3 Layer Tiles
	{ "tp-018_rom6.bin",	0x80000, 0x64b6c5ac, 4 | BRF_GRA },           //  4

	{ "tp-018_rom2.bin",	0x20000, 0x6bb72d16, 5 | BRF_GRA },           //  5 Sprites
	{ "tp-018_rom1.bin",	0x20000, 0x0934782d, 5 | BRF_GRA },           //  6
	{ "tp-018_rom3.bin",	0x20000, 0xec903c07, 5 | BRF_GRA },           //  7
	{ "tp-018_rom4.bin",	0x20000, 0x50cbf1a8, 5 | BRF_GRA },           //  8

	{ "tp018_10.bpr",		0x00020, 0xbc88cced, 0 | BRF_OPT },           //  9 PROMs
	{ "tp018_11.bpr",		0x00020, 0xa1e17492, 0 | BRF_OPT },           // 10
};

STD_ROM_PICK(outzonec)
STD_ROM_FN(outzonec)

struct BurnDriver BurnDrvOutzonec = {
	"outzonec", "outzone", NULL, NULL, "1990",
	"Out Zone (oldest set)\0", NULL, "Toaplan", "Toaplan BCU-2 / FCU-2 based",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TOAPLAN_RAIZING, GBF_RUNGUN, 0,
	NULL, outzonecRomInfo, outzonecRomName, NULL, NULL, NULL, NULL, Drv3bInputInfo, OutzonecDIPInfo,
	OutzoneInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Out Zone (Zero Wing TP-015 PCB conversion)

static struct BurnRomInfo outzonecvRomDesc[] = {
	{ "tp_018_07+.bin",		0x20000, 0x8768d843, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "tp_018_08+.bin",		0x20000, 0xaf238f71, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "tp_018_09+.bin",		0x08000, 0xb7201606, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "tp-018_rom5.bin",	0x80000, 0xc64ec7b6, 4 | BRF_GRA },           //  3 Layer Tiles
	{ "tp-018_rom6.bin",	0x80000, 0x64b6c5ac, 4 | BRF_GRA },           //  4

	{ "tp-018_rom2.bin",	0x20000, 0x6bb72d16, 5 | BRF_GRA },           //  5 Sprites
	{ "tp-018_rom1.bin",	0x20000, 0x0934782d, 5 | BRF_GRA },           //  6
	{ "tp-018_rom3.bin",	0x20000, 0xec903c07, 5 | BRF_GRA },           //  7
	{ "tp-018_rom4.bin",	0x20000, 0x50cbf1a8, 5 | BRF_GRA },           //  8

	{ "tp018_10.bpr",		0x00020, 0xbc88cced, 0 | BRF_OPT },           //  9 PROMs
	{ "tp018_11.bpr",		0x00020, 0xa1e17492, 0 | BRF_OPT },           // 10
};

STD_ROM_PICK(outzonecv)
STD_ROM_FN(outzonecv)

struct BurnDriver BurnDrvOutzonecv = {
	"outzonecv", "outzone", NULL, NULL, "1990",
	"Out Zone (Zero Wing TP-015 PCB conversion)\0", NULL, "Toaplan", "Toaplan BCU-2 / FCU-2 based",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TOAPLAN_RAIZING, GBF_RUNGUN, 0,
	NULL, outzonecvRomInfo, outzonecvRomName, NULL, NULL, NULL, NULL, Drv3bInputInfo, OutzoneDIPInfo,
	OutzonecvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Vimana (World, set 1)

static struct BurnRomInfo vimanaRomDesc[] = {
	{ "tp019-7a.bin",		0x20000, 0x5a4bf73e, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "tp019-8a.bin",		0x20000, 0x03ba27e8, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "hd647180.019",		0x08000, 0x41a97ebe, 2 | BRF_PRG | BRF_ESS }, //  2 Z180 Code

	{ "vim6.bin",			0x20000, 0x2886878d, 3 | BRF_GRA },           //  3 Layer Tiles
	{ "vim5.bin",			0x20000, 0x61a63d7a, 3 | BRF_GRA },           //  4
	{ "vim4.bin",			0x20000, 0xb0515768, 3 | BRF_GRA },           //  5
	{ "vim3.bin",			0x20000, 0x0b539131, 3 | BRF_GRA },           //  6

	{ "vim1.bin",			0x80000, 0xcdde26cd, 8 | BRF_GRA },           //  7 Sprites
	{ "vim2.bin",			0x80000, 0x1dbfc118, 8 | BRF_GRA },           //  8

	{ "tp019-09.bpr",		0x00020, 0xbc88cced, 0 | BRF_OPT },           //  9 PROMs
	{ "tp019-10.bpr",		0x00020, 0xa1e17492, 0 | BRF_OPT },           // 10
};

STD_ROM_PICK(vimana)
STD_ROM_FN(vimana)

struct BurnDriver BurnDrvVimana = {
	"vimana", NULL, NULL, NULL, "1991",
	"Vimana (World, set 1)\0", NULL, "Toaplan", "Toaplan BCU-2 / FCU-2 based",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TOAPLAN_RAIZING, GBF_VERSHOOT, 0,
	NULL, vimanaRomInfo, vimanaRomName, NULL, NULL, NULL, NULL, Drv2bInputInfo, VimanaDIPInfo,
	VimanaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Vimana (World, set 2)

static struct BurnRomInfo vimananRomDesc[] = {
	{ "tp019-07.rom",		0x20000, 0x78888ff2, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "tp019-08.rom",		0x20000, 0x6cd2dc3c, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "hd647180.019",		0x08000, 0x41a97ebe, 2 | BRF_PRG | BRF_ESS }, //  2 Z180 Code

	{ "vim6.bin",			0x20000, 0x2886878d, 3 | BRF_GRA },           //  3 Layer Tiles
	{ "vim5.bin",			0x20000, 0x61a63d7a, 3 | BRF_GRA },           //  4
	{ "vim4.bin",			0x20000, 0xb0515768, 3 | BRF_GRA },           //  5
	{ "vim3.bin",			0x20000, 0x0b539131, 3 | BRF_GRA },           //  6

	{ "vim1.bin",			0x80000, 0xcdde26cd, 8 | BRF_GRA },           //  7 Sprites
	{ "vim2.bin",			0x80000, 0x1dbfc118, 8 | BRF_GRA },           //  8

	{ "tp019-09.bpr",		0x00020, 0xbc88cced, 0 | BRF_OPT },           //  9 PROMs
	{ "tp019-10.bpr",		0x00020, 0xa1e17492, 0 | BRF_OPT },           // 10
};

STD_ROM_PICK(vimanan)
STD_ROM_FN(vimanan)

struct BurnDriver BurnDrvVimanan = {
	"vimanan", "vimana", NULL, NULL, "1991",
	"Vimana (World, set 2)\0", NULL, "Toaplan", "Toaplan BCU-2 / FCU-2 based",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TOAPLAN_RAIZING, GBF_VERSHOOT, 0,
	NULL, vimananRomInfo, vimananRomName, NULL, NULL, NULL, NULL, Drv2bInputInfo, VimananDIPInfo,
	VimanaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Vimana (Japan)

static struct BurnRomInfo vimanajRomDesc[] = {
	{ "vim07.bin",			0x20000, 0x1efaea84, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "vim08.bin",			0x20000, 0xe45b7def, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "hd647180.019",		0x08000, 0x41a97ebe, 2 | BRF_PRG | BRF_ESS }, //  2 Z180 Code

	{ "vim6.bin",			0x20000, 0x2886878d, 3 | BRF_GRA },           //  3 Layer Tiles
	{ "vim5.bin",			0x20000, 0x61a63d7a, 3 | BRF_GRA },           //  4
	{ "vim4.bin",			0x20000, 0xb0515768, 3 | BRF_GRA },           //  5
	{ "vim3.bin",			0x20000, 0x0b539131, 3 | BRF_GRA },           //  6

	{ "vim1.bin",			0x80000, 0xcdde26cd, 8 | BRF_GRA },           //  7 Sprites
	{ "vim2.bin",			0x80000, 0x1dbfc118, 8 | BRF_GRA },           //  8

	{ "tp019-09.bpr",		0x00020, 0xbc88cced, 0 | BRF_OPT },           //  9 PROMs
	{ "tp019-10.bpr",		0x00020, 0xa1e17492, 0 | BRF_OPT },           // 10
};

STD_ROM_PICK(vimanaj)
STD_ROM_FN(vimanaj)

struct BurnDriver BurnDrvVimanaj = {
	"vimanaj", "vimana", NULL, NULL, "1991",
	"Vimana (Japan)\0", NULL, "Toaplan", "Toaplan BCU-2 / FCU-2 based",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TOAPLAN_RAIZING, GBF_VERSHOOT, 0,
	NULL, vimanajRomInfo, vimanajRomName, NULL, NULL, NULL, NULL, Drv2bInputInfo, VimanajDIPInfo,
	VimanaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};
