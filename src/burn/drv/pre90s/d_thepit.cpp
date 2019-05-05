// FB Alpha The Pit driver module
// Based on MAME driver by Zsolt Vasvari

#include "tiles_generic.h"
#include "z80_intf.h"
#include "ay8910.h"
#include "watchdog.h"
#include "bitswap.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *MemEnd;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvUsrROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvVidRAM;
static UINT8 *DrvColRAM;
static UINT8 *DrvAttRAM;
static UINT8 *DrvSprRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 nmi_mask;
static UINT8 sound_enable;
static UINT8 flipscreen[2];
static UINT8 soundlatch;
static INT32 graphics_bank;
static INT32 question_rom;
static INT32 question_address;
static INT32 remap_address[0x10];

static INT32 graphics_size;
static UINT8 global_color_depth;
static UINT8 sprite_bank = 0; // desertdn, suprmous
static INT32 color_prom_size; // suprmous
static INT32 intrepid = 0;
static INT32 rtriv = 0;
static INT32 desertdn = 0;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[1];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo DrvInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Drv)

static struct BurnInputInfo IntrepidInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Intrepid)

static struct BurnInputInfo DockmanInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Dockman)


static struct BurnInputInfo RtrivInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 4"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 start"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 fire 4"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Rtriv)

static struct BurnDIPInfo ThepitDIPList[]=
{
	{0x0e, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{0x0e, 0x01, 0x03, 0x01, "2 Coins 3 Credits"	},
	{0x0e, 0x01, 0x03, 0x02, "2 Coins 4 Credits"	},
	{0x0e, 0x01, 0x03, 0x00, "1 Coin  2 Credits"	},
	{0x0e, 0x01, 0x03, 0x03, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Game Speed"		},
	{0x0e, 0x01, 0x04, 0x04, "Slow"			},
	{0x0e, 0x01, 0x04, 0x00, "Fast"			},

	{0   , 0xfe, 0   ,    2, "Time Limit"		},
	{0x0e, 0x01, 0x08, 0x00, "Long"			},
	{0x0e, 0x01, 0x08, 0x08, "Short"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x0e, 0x01, 0x10, 0x00, "Off"			},
	{0x0e, 0x01, 0x10, 0x10, "On"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0e, 0x01, 0x20, 0x00, "Upright"		},
	{0x0e, 0x01, 0x20, 0x20, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x0e, 0x01, 0x40, 0x00, "3"			},
	{0x0e, 0x01, 0x40, 0x40, "4"			},

	{0   , 0xfe, 0   ,    2, "Diagnostic Tests"	},
	{0x0e, 0x01, 0x80, 0x00, "Off"			},
	{0x0e, 0x01, 0x80, 0x80, "Loop Tests"		},
};

STDDIPINFO(Thepit)

static struct BurnDIPInfo DesertdnDIPList[]=
{
	{0x0e, 0xff, 0xff, 0x05, NULL			},

	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{0x0e, 0x01, 0x03, 0x02, "2 Coins 1 Credits"	},
	{0x0e, 0x01, 0x03, 0x01, "1 Coin  1 Credits"	},
	{0x0e, 0x01, 0x03, 0x00, "1 Coin  2 Credits"	},
	{0x0e, 0x01, 0x03, 0x03, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0e, 0x01, 0x0c, 0x00, "3"			},
	{0x0e, 0x01, 0x0c, 0x04, "4"			},
	{0x0e, 0x01, 0x0c, 0x08, "5"			},
	{0x0e, 0x01, 0x0c, 0x0c, "6"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0e, 0x01, 0x10, 0x10, "Cocktail"		},
	{0x0e, 0x01, 0x10, 0x00, "Upright"		},

	{0   , 0xfe, 0   ,    2, "Timer Speed"		},
	{0x0e, 0x01, 0x40, 0x00, "Slow"			},
	{0x0e, 0x01, 0x40, 0x40, "Fast"			},

	{0   , 0xfe, 0   ,    2, "Invulnerability"	},
	{0x0e, 0x01, 0x80, 0x00, "Off"			},
	{0x0e, 0x01, 0x80, 0x80, "On"			},
};

STDDIPINFO(Desertdn)

static struct BurnDIPInfo RoundupDIPList[]=
{
	{0x0e, 0xff, 0xff, 0x55, NULL			},

	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{0x0e, 0x01, 0x03, 0x00, "2 Coins 1 Credits"	},
	{0x0e, 0x01, 0x03, 0x01, "1 Coin  1 Credits"	},
	{0x0e, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},
	{0x0e, 0x01, 0x03, 0x03, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0e, 0x01, 0x0c, 0x00, "2"			},
	{0x0e, 0x01, 0x0c, 0x04, "3"			},
	{0x0e, 0x01, 0x0c, 0x08, "4"			},
	{0x0e, 0x01, 0x0c, 0x0c, "5"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0e, 0x01, 0x10, 0x10, "Upright"		},
	{0x0e, 0x01, 0x10, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x0e, 0x01, 0x20, 0x00, "10000"		},
	{0x0e, 0x01, 0x20, 0x20, "30000"		},

	{0   , 0xfe, 0   ,    2, "Gly Boys Wake Up"	},
	{0x0e, 0x01, 0x40, 0x40, "Slow"			},
	{0x0e, 0x01, 0x40, 0x00, "Fast"			},

	{0   , 0xfe, 0   ,    2, "Push Switch Check"	},
	{0x0e, 0x01, 0x80, 0x00, "Off"			},
	{0x0e, 0x01, 0x80, 0x80, "On"			},
};	

STDDIPINFO(Roundup)

static struct BurnDIPInfo FitterDIPList[]=
{
	{0x0e, 0xff, 0xff, 0x55, NULL			},

	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{0x0e, 0x01, 0x03, 0x00, "2 Coins 1 Credits"	},
	{0x0e, 0x01, 0x03, 0x01, "1 Coin  1 Credits"	},
	{0x0e, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},
	{0x0e, 0x01, 0x03, 0x03, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0e, 0x01, 0x0c, 0x00, "2"			},
	{0x0e, 0x01, 0x0c, 0x04, "3"			},
	{0x0e, 0x01, 0x0c, 0x08, "4"			},
	{0x0e, 0x01, 0x0c, 0x0c, "5"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0e, 0x01, 0x10, 0x10, "Upright"		},
	{0x0e, 0x01, 0x10, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x0e, 0x01, 0x20, 0x00, "10000"		},
	{0x0e, 0x01, 0x20, 0x20, "30000"		},

	{0   , 0xfe, 0   ,    2, "Gly Boys Wake Up"	},
	{0x0e, 0x01, 0x40, 0x40, "Slow"			},
	{0x0e, 0x01, 0x40, 0x00, "Fast"			},

	{0   , 0xfe, 0   ,    2, "Invulnerability"	},
	{0x0e, 0x01, 0x80, 0x00, "Off"			},
	{0x0e, 0x01, 0x80, 0x80, "On"			},
};

STDDIPINFO(Fitter)

static struct BurnDIPInfo IntrepidDIPList[]=
{
	{0x0e, 0xff, 0xff, 0x4f, NULL			},

	{0   , 0xfe, 0   ,    2, "Invulnerability"	},
	{0x0e, 0x01, 0x01, 0x01, "Off"			},
	{0x0e, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x0e, 0x01, 0x02, 0x00, "Off"			},
	{0x0e, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0e, 0x01, 0x04, 0x04, "Upright"		},
	{0x0e, 0x01, 0x04, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x0e, 0x01, 0x08, 0x08, "Easy"			},
	{0x0e, 0x01, 0x08, 0x00, "Hard"			},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x0e, 0x01, 0x10, 0x00, "10000"		},
	{0x0e, 0x01, 0x10, 0x10, "30000"		},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x0e, 0x01, 0x20, 0x20, "3"			},
	{0x0e, 0x01, 0x20, 0x00, "5"			},

	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{0x0e, 0x01, 0xc0, 0x80, "2 Coins 1 Credits"	},
	{0x0e, 0x01, 0xc0, 0x40, "1 Coin  1 Credits"	},
	{0x0e, 0x01, 0xc0, 0xc0, "1 Coin  2 Credits"	},
	{0x0e, 0x01, 0xc0, 0x00, "Free Play"		},
};

STDDIPINFO(Intrepid)

static struct BurnDIPInfo IntrepidbDIPList[]=
{
	{0x0e, 0xff, 0xff, 0x4f, NULL			},

	{0   , 0xfe, 0   ,    2, "Invulnerability"	},
	{0x0e, 0x01, 0x01, 0x01, "Off"			},
	{0x0e, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x0e, 0x01, 0x02, 0x00, "Off"			},
	{0x0e, 0x01, 0x02, 0x02, "On"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0e, 0x01, 0x04, 0x04, "Upright"		},
	{0x0e, 0x01, 0x04, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x0e, 0x01, 0x08, 0x08, "Easy"			},
	{0x0e, 0x01, 0x08, 0x00, "Hard"			},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x0e, 0x01, 0x10, 0x00, "10000"		},
	{0x0e, 0x01, 0x10, 0x10, "30000"		},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x0e, 0x01, 0x20, 0x20, "2"			},
	{0x0e, 0x01, 0x20, 0x00, "3"			},

	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{0x0e, 0x01, 0xc0, 0x80, "2 Coins 1 Credits"	},
	{0x0e, 0x01, 0xc0, 0x40, "1 Coin  1 Credits"	},
	{0x0e, 0x01, 0xc0, 0xc0, "1 Coin  2 Credits"	},
	{0x0e, 0x01, 0xc0, 0x00, "Free Play"		},
};

STDDIPINFO(Intrepidb)


static struct BurnDIPInfo DockmanDIPList[]=
{
	{0x08, 0xff, 0xff, 0x05, NULL			},

	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{0x08, 0x01, 0x03, 0x00, "2 Coins 1 Credits"	},
	{0x08, 0x01, 0x03, 0x01, "1 Coin  1 Credits"	},
	{0x08, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},
	{0x08, 0x01, 0x03, 0x03, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x08, 0x01, 0x0c, 0x00, "2"			},
	{0x08, 0x01, 0x0c, 0x04, "3"			},
	{0x08, 0x01, 0x0c, 0x08, "4"			},
	{0x08, 0x01, 0x0c, 0x0c, "5"			},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x08, 0x01, 0x10, 0x00, "10000"		},
	{0x08, 0x01, 0x10, 0x10, "30000"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x08, 0x01, 0x20, 0x00, "Upright"		},
	{0x08, 0x01, 0x20, 0x20, "Cocktail"		},
};

STDDIPINFO(Dockman)

static struct BurnDIPInfo SuprmousDIPList[]=
{
	{0x0e, 0xff, 0xff, 0x01, NULL			},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x0e, 0x01, 0x07, 0x00, "2 Coins 1 Credits"	},
	{0x0e, 0x01, 0x07, 0x01, "1 Coin  1 Credits"	},
	{0x0e, 0x01, 0x07, 0x02, "1 Coin  2 Credits"	},
	{0x0e, 0x01, 0x07, 0x03, "1 Coin  3 Credits"	},
	{0x0e, 0x01, 0x07, 0x04, "1 Coin  4 Credits"	},
	{0x0e, 0x01, 0x07, 0x05, "1 Coin  5 Credits"	},
	{0x0e, 0x01, 0x07, 0x06, "1 Coin  6 Credits"	},
	{0x0e, 0x01, 0x07, 0x07, "1 Coin  7 Credits"	},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x0e, 0x01, 0x18, 0x00, "3"			},
	{0x0e, 0x01, 0x18, 0x08, "5"			},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x0e, 0x01, 0x20, 0x20, "5000"			},
	{0x0e, 0x01, 0x20, 0x00, "10000"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0e, 0x01, 0x40, 0x00, "Upright"		},
	{0x0e, 0x01, 0x40, 0x40, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Invulnerability"	},
	{0x0e, 0x01, 0x80, 0x00, "Off"			},
	{0x0e, 0x01, 0x80, 0x80, "On"			},
};

STDDIPINFO(Suprmous)

static struct BurnDIPInfo RtrivDIPList[]=
{
	{0x0c, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Coinage"		},
	{0x0c, 0x01, 0x02, 0x00, "2 Coins 1 Credits"	},
	{0x0c, 0x01, 0x02, 0x02, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    2, "Show Correct Answer"	},
	{0x0c, 0x01, 0x04, 0x00, "No"			},
	{0x0c, 0x01, 0x04, 0x04, "Yes"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0c, 0x01, 0x08, 0x08, "Upright"		},
	{0x0c, 0x01, 0x08, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Monitor"		},
	{0x0c, 0x01, 0x10, 0x10, "Vertical"		},
	{0x0c, 0x01, 0x10, 0x00, "Horizontal"		},

	{0   , 0xfe, 0   ,    2, "Gaming Option"	},
	{0x0c, 0x01, 0x20, 0x20, "Number of Wrong Answer"		},
	{0x0c, 0x01, 0x20, 0x00, "Number of Good Answer for Bonus Question"		},

	{0   , 0xfe, 0   ,    4, "Gaming Option Number"	},
	{0x0c, 0x01, 0xc0, 0x00, "2 / 4"			},
	{0x0c, 0x01, 0xc0, 0x40, "3 / 5"			},
	{0x0c, 0x01, 0xc0, 0x80, "4 / 6"			},
	{0x0c, 0x01, 0xc0, 0xc0, "5 / 7"			},
};

STDDIPINFO(Rtriv)

static void __fastcall thepit_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xa000:
		return;	// nop

		case 0xb000:
			nmi_mask = data & 1;
			if (nmi_mask == 0)
				ZetSetIRQLine(0x20 /*nmi*/, CPU_IRQSTATUS_NONE);
		return;

		case 0xb002: // fitter
			// coin lockout
		return;

		case 0xb003:
			sound_enable = data & 1;
		return;

		case 0xb005: // intrepid
			graphics_bank = data & 1;
		return;

		case 0xb006:
		case 0xb007:
			flipscreen[address & 1] = data & 1;
		return;

		case 0xb800:
			soundlatch = data;
		return;
	}
}

static UINT8 rtriv_question_read(UINT16 offset)
{
	switch (offset & 0xc00)
	{
		case 0x400:
			question_rom = (offset & 0x70) >> 4;
			question_address = ((offset & 0x80) << 3) | ((offset & 0x0f) << 11);
		break;

		case 0x800:
			remap_address[offset & 0xf] = ((offset & 0xf0) >> 4) ^ 0xf;
		break;

		case 0xc00:
		{
			UINT32 address = (question_rom * 0x8000) | question_address | (offset & 0x3f0) | remap_address[offset & 0xf];
			return DrvUsrROM[address];
		}
	}

	return 0;
}

static UINT8 __fastcall thepit_main_read(UINT16 address)
{
	if ((address & 0xf000) == 0x4000) {
		bprintf(0, _T("read %X. "), address);
		return rtriv_question_read(address & 0xfff);
	}

	switch (address)
	{
		case 0xa000:
			return DrvInputs[(flipscreen[0] ? 2 : 0)];

		case 0xa800:
			return DrvInputs[1];

		case 0xb000:
			return DrvDips[0];

		case 0xb800:
			return BurnWatchdogRead();
	}

	return 0;
}

static void __fastcall thepit_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			soundlatch = 0;
		return;

		case 0x8c:
		case 0x8d:
			AY8910Write(1, port & 1, data);
		return;

		case 0x8e:
		case 0x8f:
			AY8910Write(0, port & 1, data);
		return;
	}
}

static UINT8 __fastcall thepit_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x8d:
			return AY8910Read(1);

		case 0x8f:
			return AY8910Read(0);
	}

	return 0;
}

static tilemap_callback( layer0 )
{
	UINT8 back_color = (DrvColRAM[offs] & 0x70) >> 4;
	INT32 priority = ((back_color != 0) && ((DrvColRAM[offs] & 0x80) == 0)) ? 1 : 0;

	TILE_SET_INFO(0, 0, back_color, TILE_GROUP(priority));
}

static tilemap_callback( layer1 )
{
	TILE_SET_INFO(1, DrvVidRAM[offs] + (graphics_bank * 0x100), DrvColRAM[offs], 0);
}

static UINT8 AY8910_0_portA(UINT32)
{
	return soundlatch;
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	ZetOpen(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	ZetClose();

	BurnWatchdogReset();

	AY8910Reset(0);
	AY8910Reset(1);

	nmi_mask = 0;
	sound_enable = 0;
	flipscreen[0] = 0;
	flipscreen[1] = 0;
	soundlatch = 0;
	graphics_bank = 0;
	question_rom = 0;
	question_address = 0;
	memset (remap_address, 0, sizeof(remap_address));

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x008000;
	DrvZ80ROM1		= Next; Next += 0x002000;

	DrvUsrROM		= Next; Next += 0x040000;

	DrvGfxROM		= Next; Next += 0x008040; // + empty tile

	DrvColPROM		= Next; Next += 0x000040;

	DrvPalette		= (UINT32*)Next; Next += 0x0028 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM0		= Next; Next += 0x000800;
	DrvZ80RAM1		= Next; Next += 0x000400;
	DrvVidRAM		= Next; Next += 0x000400;
	DrvColRAM		= Next; Next += 0x000400;
	DrvAttRAM		= Next; Next += 0x000100;
	DrvSprRAM		= DrvAttRAM + 0x40; // 0x20 in size

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[3] = { 0x1000*8*2, 0x1000*8*1, 0x1000*8*0 };
	INT32 XOffs[8] = { STEP8(0,1) };
	INT32 YOffs[8] = { STEP8(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x3000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM, 0x3000);

	GfxDecode(0x0200, 3, 8, 8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM);

	BurnFree(tmp);

	return 0;
}

static INT32 LoadRoms()
{
	char* pRomName;
	struct BurnRomInfo ri;

	UINT8 *pLoad = DrvZ80ROM0;
	UINT8 *sLoad = DrvZ80ROM1;
	UINT8 *gLoad = DrvGfxROM;
	UINT8 *uLoad = DrvUsrROM;
	UINT8 *cLoad = DrvColPROM;
	graphics_size = 0;

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {

		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & 7) == 1) {
			if (BurnLoadRom(pLoad, i, 1)) return 1;
			pLoad += ri.nLen;
			continue;
		}

		if ((ri.nType & 7) == 2) {
			if (BurnLoadRom(sLoad, i, 1)) return 1;
			sLoad += ri.nLen;
			continue;
		}
		
		if ((ri.nType & 7) == 3) {
			if (BurnLoadRom(gLoad, i, 1)) return 1;
			gLoad += 0x1000; // for decoding
			graphics_size += ri.nLen;
			continue;
		}

		if ((ri.nType & 7) == 4) {
			if (BurnLoadRom(cLoad, i, 1)) return 1;
			cLoad += ri.nLen;
			continue;
		}

		if ((ri.nType & 7) == 5) {
			if (BurnLoadRom(uLoad, i, 1)) return 1;
			uLoad += ri.nLen;
			rtriv = 1;
			continue;
		}
	}

	global_color_depth = (gLoad - DrvGfxROM) >> 12;
	graphics_size = (graphics_size / global_color_depth) * 8;
	color_prom_size = cLoad - DrvColPROM;

	bprintf (0, _T("depth: %d cprom: %x\n"), global_color_depth, color_prom_size);

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
		if (LoadRoms()) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,	0x8000, 0x87ff, MAP_RAM);
	ZetMapMemory(DrvColRAM,		0x8800, 0x8bff, MAP_RAM);
	ZetMapMemory(DrvColRAM,		0x8c00, 0x8fff, MAP_RAM); // mirror
	ZetMapMemory(DrvVidRAM,		0x9000, 0x93ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,		0x9400, 0x97ff, MAP_RAM); // mirror
	for (INT32 i = 0; i < 0x800; i+=0x100) {
		ZetMapMemory(DrvAttRAM,	0x9800 + i, 0x98ff + i, MAP_RAM);
	}
	// attr = DrvAttrRAM, Spr = attr + 0x40, RAM = attr + 0x60
	ZetSetWriteHandler(thepit_main_write);
	ZetSetReadHandler(thepit_main_read);

	if (intrepid)
	{
		ZetMapMemory(DrvColRAM,		0x9400, 0x97ff, MAP_RAM);
	}

	if (rtriv)
	{
		ZetUnmapMemory(0x4000, 0x4fff, MAP_RAM);
	}

	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,	0x3800, 0x3bff, MAP_RAM);
	ZetSetInHandler(thepit_sound_read_port);
	ZetSetOutHandler(thepit_sound_write_port);
	ZetClose();

	BurnWatchdogInit(DrvDoReset, 180);

	AY8910Init(0, 1536000, 0);
	AY8910Init(1, 1536000, 1);
	AY8910SetPorts(0, &AY8910_0_portA, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.25, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.25, BURN_SND_ROUTE_BOTH);
	AY8910SetBuffered(ZetTotalCycles, 2500000);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, layer0_map_callback, 8, 8, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, layer1_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM + 0x8000, 0, 8, 8, 0x0040,  0x20, 7);
	GenericTilemapSetGfx(1, DrvGfxROM + 0x0000, global_color_depth, 8, 8, graphics_size, 0, 7 >> (global_color_depth - 2));
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);
	GenericTilemapSetScrollCols(0, 32);
	GenericTilemapSetScrollCols(1, 32);
	GenericTilemapSetTransparent(1, 0);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	ZetExit();
	AY8910Exit(0);

	BurnFree(AllMem);

	sprite_bank = 0;
	intrepid = 0;
	rtriv = 0;
	desertdn = 0;

	return 0;
}

static void DrvPaletteInit()
{
	if (color_prom_size == 0x20)
	{
		for (INT32 i = 0; i < 32; i++)
		{
			INT32 bit0 = (DrvColPROM[i] >> 0) & 0x01;
			INT32 bit1 = (DrvColPROM[i] >> 1) & 0x01;
			INT32 bit2 = (DrvColPROM[i] >> 2) & 0x01;
			INT32 r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

			bit0 = (DrvColPROM[i] >> 3) & 0x01;
			bit1 = (DrvColPROM[i] >> 4) & 0x01;
			bit2 = (DrvColPROM[i] >> 5) & 0x01;
			INT32 g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

			bit0 = 0;
			bit1 = (DrvColPROM[i] >> 6) & 0x01;
			bit2 = (DrvColPROM[i] >> 7) & 0x01;
			INT32 b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

			DrvPalette[i] = BurnHighCol(r,g,b,0);
		}
	}
	else
	{
		for (INT32 i = 0; i < 32; i++)
		{
			UINT8 b = BITSWAP08(DrvColPROM[i + 0x00], 0, 1, 2, 3, 4, 5, 6, 7);
			UINT8 g = BITSWAP08(DrvColPROM[i + 0x20], 0, 1, 2, 3, 4, 5, 6, 7);
			UINT8 r = ((b >> 3) & 0x1c) | (g >> 6);

			r = (r << 3) | (r >> 2);
			g = (g << 3) | (g >> 2);
			b = (b << 4) | (b & 0xf);

			DrvPalette[i] = BurnHighCol(r,g,b,0);
		}
	}

	for (INT32 i = 0; i < 8; i++)
	{
		DrvPalette[i+0x20] = BurnHighCol((i&4)?0xff:0, (i&2)?0xff:0, (i&1)?0xff:0, 0);
	}
}

static void draw_single_sprite(INT32 code, INT32 color, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy)
{
	INT32 depth = global_color_depth;

	if (flipy) {
		if (flipx) {
			Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, code + 3, sx + 0, sy + 0, color, depth, 0, 0, DrvGfxROM);
			Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, code + 2, sx + 8, sy + 0, color, depth, 0, 0, DrvGfxROM);
			Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, code + 1, sx + 0, sy + 8, color, depth, 0, 0, DrvGfxROM);
			Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, code + 0, sx + 8, sy + 8, color, depth, 0, 0, DrvGfxROM);
		} else {
			Render8x8Tile_Mask_FlipY_Clip(pTransDraw, code + 2, sx + 0, sy + 0, color, depth, 0, 0, DrvGfxROM);
			Render8x8Tile_Mask_FlipY_Clip(pTransDraw, code + 3, sx + 8, sy + 0, color, depth, 0, 0, DrvGfxROM);
			Render8x8Tile_Mask_FlipY_Clip(pTransDraw, code + 0, sx + 0, sy + 8, color, depth, 0, 0, DrvGfxROM);
			Render8x8Tile_Mask_FlipY_Clip(pTransDraw, code + 1, sx + 8, sy + 8, color, depth, 0, 0, DrvGfxROM);
		}
	} else {
		if (flipx) {
			Render8x8Tile_Mask_FlipX_Clip(pTransDraw, code + 1, sx + 0, sy + 0, color, depth, 0, 0, DrvGfxROM);
			Render8x8Tile_Mask_FlipX_Clip(pTransDraw, code + 0, sx + 8, sy + 0, color, depth, 0, 0, DrvGfxROM);
			Render8x8Tile_Mask_FlipX_Clip(pTransDraw, code + 3, sx + 0, sy + 8, color, depth, 0, 0, DrvGfxROM);
			Render8x8Tile_Mask_FlipX_Clip(pTransDraw, code + 2, sx + 8, sy + 8, color, depth, 0, 0, DrvGfxROM);
		} else {
			Render8x8Tile_Mask_Clip(pTransDraw, code + 0, sx + 0, sy + 0, color, depth, 0, 0, DrvGfxROM);
			Render8x8Tile_Mask_Clip(pTransDraw, code + 1, sx + 8, sy + 0, color, depth, 0, 0, DrvGfxROM);
			Render8x8Tile_Mask_Clip(pTransDraw, code + 2, sx + 0, sy + 8, color, depth, 0, 0, DrvGfxROM);
			Render8x8Tile_Mask_Clip(pTransDraw, code + 3, sx + 8, sy + 8, color, depth, 0, 0, DrvGfxROM);
		}
	}
}

static void draw_sprites(INT32 priority)
{
	INT32 color_mask = 7 >> (global_color_depth - 2);
	priority <<= 3;

	INT32 bank = (sprite_bank * 0x100) + (graphics_bank * 0x100);

	if (flipscreen[0]) {
		if (desertdn)
			GenericTilesSetClip(0*8+1, 24*8-1, 2*8, 30*8-1);
		else
			GenericTilesSetClip(8*8, 32*8-2, 2*8, 30*8-1);
	} else {
		if (desertdn)
			GenericTilesSetClip(0*8, 30*8-2, 0*8, 28*8-1);
		else
			GenericTilesSetClip(2*8+1, 32*8-1, 0*8, 28*8-1);
	}

	for (INT32 offs = 0x20 - 4; offs >= 0; offs -= 4)
	{
		if ((DrvSprRAM[offs + 2] & 0x08) == priority)
		{
			if ((DrvSprRAM[offs + 0] == 0) || (DrvSprRAM[offs + 3] == 0))
			{
				continue;
			}

			UINT8 sy = 240 - DrvSprRAM[offs];
			UINT8 sx = DrvSprRAM[offs + 3] + 1;

			UINT8 flipx = DrvSprRAM[offs + 1] & 0x40;
			UINT8 flipy = DrvSprRAM[offs + 1] & 0x80;

			INT32 code = (DrvSprRAM[offs + 1] & 0x3f) * 4 + bank;
			INT32 color = DrvSprRAM[offs + 2] & color_mask;

			if (flipscreen[1])
			{
				sy = 240 - sy;
				flipy = !flipy;
			}

			if (flipscreen[0])
			{
				sx = 242 - sx;
				flipx = !flipx;
			}

			if (offs < 16) sy++;	// sprites 0-3 are drawn one pixel down

			draw_single_sprite(code, color, sx,       sy - 16, flipx, flipy);
			draw_single_sprite(code, color, sx - 256, sy - 16, flipx, flipy); // wrap
		}
	}

	GenericTilesClearClip();
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	INT32 xshift = flipscreen[0] ? 128 : 0;
	INT32 yshift = flipscreen[1] ? -8 : 0;

	GenericTilemapSetFlip(TMAP_GLOBAL, (flipscreen[0] ? TMAP_FLIPX : 0) | (flipscreen[1] ? TMAP_FLIPY : 0));

	GenericTilemapSetScrollX(0, xshift);
	GenericTilemapSetScrollX(1, xshift);

	for (INT32 col = 0; col < 32; col++)
	{
		INT32 scrolly = yshift + DrvAttRAM[col * 2];

		GenericTilemapSetScrollCol(0, col, scrolly);
		GenericTilemapSetScrollCol(1, col, scrolly);
	}

	if ((nBurnLayer & 1) == 0) BurnTransferClear();

	if ((nBurnLayer & 1) == 1) GenericTilemapDraw(0, pTransDraw, 0);
	if ((nBurnLayer & 2) == 2) GenericTilemapDraw(1, pTransDraw, 0);

	if (nSpriteEnable & 1) draw_sprites(0);

	if ((nBurnLayer & 4) == 4) GenericTilemapDraw(0, pTransDraw, TMAP_SET_GROUP(1));

	if (nSpriteEnable & 2) draw_sprites(1);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	{
		memset (DrvInputs, 0, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	ZetNewFrame();

	INT32 nInterleave = 10;
	INT32 nCyclesTotal[2] = { 3072000 / 60, 2500000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		CPU_RUN(0, Zet);
		if (i == (nInterleave - 1) && nmi_mask) ZetSetIRQLine(0x20, CPU_IRQSTATUS_ACK);
		ZetClose();

		ZetOpen(1);
		CPU_RUN(1, Zet);
		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();
	}

	if (pBurnSoundOut) {
		if (sound_enable) {
			AY8910Render(pBurnSoundOut, nBurnSoundLen);
		} else {
			BurnSoundClear();
		}
	}

	if (pBurnDraw) {
		DrvDraw();
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
		BurnWatchdogScan(nAction);

		SCAN_VAR(nmi_mask);
		SCAN_VAR(sound_enable);
		SCAN_VAR(flipscreen[0]);
		SCAN_VAR(flipscreen[1]);
		SCAN_VAR(soundlatch);
		SCAN_VAR(graphics_bank);

		SCAN_VAR(question_rom);
		SCAN_VAR(question_address);
		SCAN_VAR(remap_address);
	}

	return 0;
}


// The Pit

static struct BurnRomInfo thepitRomDesc[] = {
	{ "pit1.bin",		0x1000, 0x71affecc, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "pit2.bin",		0x1000, 0x894063cd, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "pit3.bin",		0x1000, 0x1b488543, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "pit4.bin",		0x1000, 0xe941e848, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "pit5.bin",		0x1000, 0xe0643c95, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "pit6.bin",		0x0800, 0x1b79dfb6, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 #1 Code

	{ "pit8.bin",		0x0800, 0x69502afc, 3 | BRF_GRA },           //  6 Graphics
	{ "pit7.bin",		0x0800, 0xd901b353, 3 | BRF_GRA },           //  7

	{ "82s123.ic4",		0x0020, 0xa758b567, 4 | BRF_GRA },           //  8 Color data
};

STD_ROM_PICK(thepit)
STD_ROM_FN(thepit)

struct BurnDriver BurnDrvThepit = {
	"thepit", NULL, NULL, NULL, "1982",
	"The Pit\0", NULL, "Zilec Electronics", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, thepitRomInfo, thepitRomName, NULL, NULL, NULL, NULL, DrvInputInfo, ThepitDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x28,
	224, 256, 3, 4
};


// The Pit (US set 1)

static struct BurnRomInfo thepitu1RomDesc[] = {
	{ "p38b.ic38",		0x1000, 0x7315e1bc, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "p39b.ic39",		0x1000, 0xc9cc30fe, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "p40b.ic40",		0x1000, 0x986738b5, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "p41b.ic41",		0x1000, 0x31ceb0a1, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "p33b.ic33",		0x1000, 0x614ec454, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "p30.ic30",		0x0800, 0x1b79dfb6, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 #1 Code

	{ "p9.ic9",		0x0800, 0x69502afc, 3 | BRF_GRA },           //  6 Graphics
	{ "p8.ic8",		0x0800, 0x2ddd5045, 3 | BRF_GRA },           //  7

	{ "82s123.ic4",		0x0020, 0xa758b567, 4 | BRF_GRA },           //  8 Color data
};

STD_ROM_PICK(thepitu1)
STD_ROM_FN(thepitu1)

struct BurnDriver BurnDrvThepitu1 = {
	"thepitu1", "thepit", NULL, NULL, "1982",
	"The Pit (US set 1)\0", NULL, "Zilec Electronics (Centuri license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, thepitu1RomInfo, thepitu1RomName, NULL, NULL, NULL, NULL, DrvInputInfo, ThepitDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x28,
	224, 256, 3, 4
};


// The Pit (US set 2)

static struct BurnRomInfo thepitu2RomDesc[] = {
	{ "p38.ic38",		0x1000, 0x332723e2, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "p39.ic39",		0x1000, 0x4fdc11b7, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "p40.ic40",		0x1000, 0xe4327d0e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "p41.ic41",		0x1000, 0x7d5df97c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "p33.ic33",		0x1000, 0x396d9856, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "p30.ic30",		0x0800, 0x1b79dfb6, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 #1 Code

	{ "p9b.ic9",		0x0800, 0x69502afc, 3 | BRF_GRA },           //  6 Graphics
	{ "p8b.ic8",		0x0800, 0x2ddd5045, 3 | BRF_GRA },           //  7

	{ "82s123.ic4",		0x0020, 0xa758b567, 4 | BRF_GRA },           //  8 Color data
};

STD_ROM_PICK(thepitu2)
STD_ROM_FN(thepitu2)

struct BurnDriver BurnDrvThepitu2 = {
	"thepitu2", "thepit", NULL, NULL, "1982",
	"The Pit (US set 2)\0", NULL, "Zilec Electronics (Centuri license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, thepitu2RomInfo, thepitu2RomName, NULL, NULL, NULL, NULL, DrvInputInfo, ThepitDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x28,
	224, 256, 3, 4
};


// The Pit (Japan)

static struct BurnRomInfo thepitjRomDesc[] = {
	{ "pit01.ic38",		0x1000, 0x87c269bf, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "pit02.ic39",		0x1000, 0xe1dfd360, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "pit03.ic40",		0x1000, 0x3674ccac, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "pit04.ic41",		0x1000, 0xbddde829, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "pit05.ic33",		0x1000, 0x584d1546, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "pit07.ic30",		0x0800, 0x2d4881f9, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 #1 Code
	{ "pit06.ic31",		0x0800, 0xc9d8c1cc, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "pit08.ic9",		0x0800, 0x00dce65f, 3 | BRF_GRA },           //  7 Graphics
	{ "pit09.ic8",		0x0800, 0xa2e2b218, 3 | BRF_GRA },           //  8

	{ "82s123.ic4",		0x0020, 0xa758b567, 4 | BRF_GRA },           //  9 Color data
};

STD_ROM_PICK(thepitj)
STD_ROM_FN(thepitj)

struct BurnDriver BurnDrvThepitj = {
	"thepitj", "thepit", NULL, NULL, "1982",
	"The Pit (Japan)\0", NULL, "Zilec Electronics (Taito license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, thepitjRomInfo, thepitjRomName, NULL, NULL, NULL, NULL, DrvInputInfo, ThepitDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x28,
	224, 256, 3, 4
};


// Round-Up

static struct BurnRomInfo roundupRomDesc[] = {
	{ "roundup.u38",	0x1000, 0xd62c3b7a, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "roundup.u39",	0x1000, 0x37bf554b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "roundup.u40",	0x1000, 0x5109d0c5, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "roundup.u41",	0x1000, 0x1c5ed660, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "roundup.u33",	0x1000, 0x2fa711f3, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "roundup.u30",	0x0800, 0x1b18faee, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 #1 Code
	{ "roundup.u31",	0x0800, 0x76cf4394, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "roundup.u9",		0x0800, 0x394676a2, 3 | BRF_GRA },           //  7 Graphics
	{ "roundup.u10",	0x0800, 0xa38d708d, 3 | BRF_GRA },           //  8

	{ "roundup.clr",	0x0020, 0xa758b567, 4 | BRF_GRA },           //  9 Color data
};

STD_ROM_PICK(roundup)
STD_ROM_FN(roundup)

struct BurnDriver BurnDrvRoundup = {
	"roundup", NULL, NULL, NULL, "1981",
	"Round-Up\0", NULL, "Taito Corporation (Amenip/Centuri license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, roundupRomInfo, roundupRomName, NULL, NULL, NULL, NULL, DrvInputInfo, RoundupDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x28,
	224, 256, 3, 4
};


// Fitter

static struct BurnRomInfo fitterRomDesc[] = {
	{ "ic38.bin",		0x1000, 0x6bf6cca4, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "roundup.u39",	0x1000, 0x37bf554b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ic40.bin",		0x1000, 0x572e2157, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "roundup.u41",	0x1000, 0x1c5ed660, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ic33.bin",		0x1000, 0xab47c6c2, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "ic30.bin",		0x0800, 0x4055b5ca, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 #1 Code
	{ "ic31.bin",		0x0800, 0xc9d8c1cc, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "ic9.bin",		0x0800, 0xa6799a37, 3 | BRF_GRA },           //  7 Graphics
	{ "ic8.bin",		0x0800, 0xa8256dfe, 3 | BRF_GRA },           //  8

	{ "roundup.clr",	0x0020, 0xa758b567, 4 | BRF_GRA },           //  9 Color data
};

STD_ROM_PICK(fitter)
STD_ROM_FN(fitter)

struct BurnDriver BurnDrvFitter = {
	"fitter", "roundup", NULL, NULL, "1981",
	"Fitter\0", NULL, "Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, fitterRomInfo, fitterRomName, NULL, NULL, NULL, NULL, DrvInputInfo, FitterDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x28,
	224, 256, 3, 4
};


// Fitter (bootleg of Round-Up)

static struct BurnRomInfo fitterblRomDesc[] = {
	{ "ic38.bin",		0x1000, 0x805c6974, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "roundup.u39",	0x1000, 0x37bf554b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ic40.bin",		0x1000, 0xc5f7156e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ic41.bin",		0x1000, 0xa67d5bda, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ic33.bin",		0x1000, 0x1f3c78ee, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "ic30.bin",		0x0800, 0x1b18faee, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 #1 Code
	{ "ic31.bin",		0x0800, 0x76cf4394, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "ic9.bin",		0x0800, 0x394676a2, 3 | BRF_GRA },           //  7 Graphics
	{ "ic10.bin",		0x0800, 0xa38d708d, 3 | BRF_GRA },           //  8

	{ "roundup.clr",	0x0020, 0xa758b567, 4 | BRF_GRA },           //  9 Color data
};

STD_ROM_PICK(fitterbl)
STD_ROM_FN(fitterbl)

struct BurnDriver BurnDrvFitterbl = {
	"fitterbl", "roundup", NULL, NULL, "1981",
	"Fitter (bootleg of Round-Up)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, fitterblRomInfo, fitterblRomName, NULL, NULL, NULL, NULL, DrvInputInfo, FitterDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x28,
	224, 256, 3, 4
};


// T.T Fitter (Japan)

static struct BurnRomInfo ttfitterRomDesc[] = {
	{ "ttfitter.u38",	0x1000, 0x2ccd60d4, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "ttfitter.u39",	0x1000, 0x37bf554b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ttfitter.u40",	0x1000, 0x572e2157, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ttfitter.u41",	0x1000, 0x1c5ed660, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ttfitter.u33",	0x1000, 0xd6fc9d0c, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "ttfitter.u30",	0x0800, 0x4055b5ca, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 #1 Code
	{ "ttfitter.u31",	0x0800, 0xc9d8c1cc, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "ttfitter.u9",	0x0800, 0xa6799a37, 3 | BRF_GRA },           //  7 Graphics
	{ "ttfitter.u8",	0x0800, 0xa8256dfe, 3 | BRF_GRA },           //  8

	{ "ttfitter.clr",	0x0020, 0xa758b567, 4 | BRF_GRA },           //  9 Color data
};

STD_ROM_PICK(ttfitter)
STD_ROM_FN(ttfitter)

struct BurnDriver BurnDrvTtfitter = {
	"ttfitter", "roundup", NULL, NULL, "1981",
	"T.T Fitter (Japan)\0", NULL, "Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, ttfitterRomInfo, ttfitterRomName, NULL, NULL, NULL, NULL, DrvInputInfo, FitterDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x28,
	224, 256, 3, 4
};


// Intrepid (set 1)

static struct BurnRomInfo intrepidRomDesc[] = {
	{ "ic19.1",		0x1000, 0x7d927b23, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "ic18.2",		0x1000, 0xdcc22542, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ic17.3",		0x1000, 0xfd11081e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ic16.4",		0x1000, 0x74a51841, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ic15.5",		0x1000, 0x4fef643d, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "ic22.7",		0x0800, 0x1a7cc392, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 #1 Code
	{ "ic23.6",		0x0800, 0x91ca7097, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "ic9.9",		0x1000, 0x8c70d18d, 3 | BRF_GRA },           //  7 Graphics
	{ "ic8.8",		0x1000, 0x04d067d3, 3 | BRF_GRA },           //  8

	{ "ic3.prm",		0x0020, 0x927ff40a, 4 | BRF_GRA },           //  9 Color data
};

STD_ROM_PICK(intrepid)
STD_ROM_FN(intrepid)

static INT32 IntrepidInit()
{
	intrepid = 1;
	return DrvInit();
}

struct BurnDriver BurnDrvIntrepid = {
	"intrepid", NULL, NULL, NULL, "1983",
	"Intrepid (set 1)\0", NULL, "Nova Games Ltd.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, intrepidRomInfo, intrepidRomName, NULL, NULL, NULL, NULL, IntrepidInputInfo, IntrepidDIPInfo,
	IntrepidInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x28,
	224, 256, 3, 4
};


// Intrepid (set 2)

static struct BurnRomInfo intrepid2RomDesc[] = {
	{ "intrepid.001",	0x1000, 0x9505df1e, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "intrepid.002",	0x1000, 0x27e9f53f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "intrepid.003",	0x1000, 0xda082ed7, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "intrepid.004",	0x1000, 0x60acecd9, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "intrepid.005",	0x1000, 0x7c868725, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "intrepid.007",	0x0800, 0xf85ead07, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 #1 Code
	{ "intrepid.006",	0x0800, 0x9eb6c61b, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "ic9.9",		0x1000, 0x8c70d18d, 3 | BRF_GRA },           //  7 Graphics
	{ "ic8.8",		0x1000, 0x04d067d3, 3 | BRF_GRA },           //  8

	{ "ic3.prm",		0x0020, 0x927ff40a, 4 | BRF_GRA },           //  9 Color data
};

STD_ROM_PICK(intrepid2)
STD_ROM_FN(intrepid2)

struct BurnDriver BurnDrvIntrepid2 = {
	"intrepid2", "intrepid", NULL, NULL, "1983",
	"Intrepid (set 2)\0", NULL, "Nova Games Ltd.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, intrepid2RomInfo, intrepid2RomName, NULL, NULL, NULL, NULL, IntrepidInputInfo, IntrepidDIPInfo,
	IntrepidInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x28,
	224, 256, 3, 4
};


// Intrepid (Elsys bootleg, set 1)

static struct BurnRomInfo intrepidbRomDesc[] = {
	{ "ic38.bin",		0x1000, 0xb23e632a, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "ic39.bin",		0x1000, 0xfd75b90e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ic40.bin",		0x1000, 0x86a9b6de, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ic41.bin",		0x1000, 0xfb6373c2, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ic33.bin",		0x1000, 0x7c868725, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "ic22.bin",		0x0800, 0xf85ead07, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 #1 Code
	{ "ic23.bin",		0x0800, 0x9eb6c61b, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "ic9.9",		0x1000, 0x8c70d18d, 3 | BRF_GRA },           //  7 Graphics
	{ "ic8.8",		0x1000, 0x04d067d3, 3 | BRF_GRA },           //  8

	{ "82s123.ic4",		0x0020, 0xaa1f7f5e, 4 | BRF_GRA },           //  9 Color data
};

STD_ROM_PICK(intrepidb)
STD_ROM_FN(intrepidb)

struct BurnDriver BurnDrvIntrepidb = {
	"intrepidb", "intrepid", NULL, NULL, "1984",
	"Intrepid (Elsys bootleg, set 1)\0", NULL, "bootleg (Elsys)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, intrepidbRomInfo, intrepidbRomName, NULL, NULL, NULL, NULL, IntrepidInputInfo, IntrepidbDIPInfo,
	IntrepidInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x28,
	224, 256, 3, 4
};


// Intrepid (Loris bootleg)

static struct BurnRomInfo intrepidb2RomDesc[] = {
	{ "1intrepid.prg",	0x1000, 0xb23e632a, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "2intrepid.prg",	0x1000, 0xfd75b90e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3intrepid.prg",	0x1000, 0x86a9b6de, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4intrepid.prg",	0x1000, 0x28abf634, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "5intrepid.prg",	0x1000, 0x7c868725, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "7intrepid.prg",	0x0800, 0xf85ead07, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 #1 Code
	{ "6intrepid.prg",	0x0800, 0x9eb6c61b, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "9intrepid.prg",	0x1000, 0x8c70d18d, 3 | BRF_GRA },           //  7 Graphics
	{ "8intrepid.prg",	0x1000, 0x04d067d3, 3 | BRF_GRA },           //  8

	{ "82s123.ic4",		0x0020, 0xaa1f7f5e, 4 | BRF_GRA },           //  9 Color data
};

STD_ROM_PICK(intrepidb2)
STD_ROM_FN(intrepidb2)

struct BurnDriver BurnDrvIntrepidb2 = {
	"intrepidb2", "intrepid", NULL, NULL, "1984",
	"Intrepid (Loris bootleg)\0", NULL, "bootleg (Loris)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, intrepidb2RomInfo, intrepidb2RomName, NULL, NULL, NULL, NULL, IntrepidInputInfo, IntrepidbDIPInfo,
	IntrepidInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x28,
	224, 256, 3, 4
};


// Intrepid (Elsys bootleg, set 2)

static struct BurnRomInfo intrepidb3RomDesc[] = {
	{ "1intrepid.prg",	0x1000, 0xb23e632a, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "2intrepid.prg",	0x1000, 0xfd75b90e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3intrepid.prg",	0x1000, 0x86a9b6de, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4intrepidb.prg",	0x1000, 0x137d0648, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "5intrepid.prg",	0x1000, 0x7c868725, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "7intrepid.prg",	0x0800, 0xf85ead07, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 #1 Code
	{ "6intrepid.prg",	0x0800, 0x9eb6c61b, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "9intrepid.prg",	0x1000, 0x8c70d18d, 3 | BRF_GRA },           //  7 Graphics
	{ "8intrepid.prg",	0x1000, 0x04d067d3, 3 | BRF_GRA },           //  8

	{ "82s123.ic4",		0x0020, 0xaa1f7f5e, 4 | BRF_GRA },           //  9 Color data
};

STD_ROM_PICK(intrepidb3)
STD_ROM_FN(intrepidb3)

struct BurnDriver BurnDrvIntrepidb3 = {
	"intrepidb3", "intrepid", NULL, NULL, "1984",
	"Intrepid (Elsys bootleg, set 2)\0", NULL, "bootleg (Elsys)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, intrepidb3RomInfo, intrepidb3RomName, NULL, NULL, NULL, NULL, IntrepidInputInfo, IntrepidbDIPInfo,
	IntrepidInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x28,
	224, 256, 3, 4
};


// Zarya Vostoka

static struct BurnRomInfo zaryavosRomDesc[] = {
	{ "zv1.rom",		0x1000, 0xb7eec75d, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "zv2.rom",		0x1000, 0x000aa722, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "zv3.rom",		0x1000, 0x9b8b431a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "zv4.rom",		0x1000, 0x3636d5bf, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "zv5.rom",		0x1000, 0xc5d405a7, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "zv6.rom",		0x1000, 0xd07778a1, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "zv7.rom",		0x1000, 0x63d75e5e, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "zv8.rom",		0x1000, 0xb87a286a, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "ic22.7",		0x0800, 0x00000000, 2 | BRF_NODUMP | BRF_PRG | BRF_ESS }, //  8 Z80 #1 Code
	{ "ic23.6",		0x0800, 0x00000000, 2 | BRF_NODUMP | BRF_PRG | BRF_ESS }, //  9

	{ "ic9.9",		0x1000, 0x00000000, 3 | BRF_NODUMP | BRF_GRA },           // 10 Graphics
	{ "ic8.8",		0x1000, 0x00000000, 3 | BRF_NODUMP | BRF_GRA },           // 11

	{ "zvprom.rom",		0x0020, 0x364e5700, 4 | BRF_GRA },           // 12 Color data
};

STD_ROM_PICK(zaryavos)
STD_ROM_FN(zaryavos)

struct BurnDriverD BurnDrvZaryavos = {
	"zaryavos", NULL, NULL, NULL, "1984",
	"Zarya Vostoka\0", "undumped/missing roms", "Nova Games of Canada", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, zaryavosRomInfo, zaryavosRomName, NULL, NULL, NULL, NULL, IntrepidInputInfo, IntrepidDIPInfo,
	IntrepidInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x28,
	224, 256, 3, 4
};


// Dock Man

static struct BurnRomInfo dockmanRomDesc[] = {
	{ "pe1.19",		0x1000, 0xeef2ec54, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "pe2.18",		0x1000, 0xbc48d16b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "pe3.17",		0x1000, 0x1c923057, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "pe4.16",		0x1000, 0x23af1cba, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "pe5.15",		0x1000, 0x39dbe429, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "pe7.22",		0x0800, 0xd2094e4a, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 #1 Code
	{ "pe6.23",		0x0800, 0x1cf447f4, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "pe8.9",		0x1000, 0x4d8c2974, 3 | BRF_GRA },           //  7 Graphics
	{ "pe9.8",		0x1000, 0x4e4ea162, 3 | BRF_GRA },           //  8

	{ "mb7051.3",		0x0020, 0x6440dc61, 4 | BRF_GRA },           //  9 Color data
};

STD_ROM_PICK(dockman)
STD_ROM_FN(dockman)

struct BurnDriver BurnDrvDockman = {
	"dockman", NULL, NULL, NULL, "1982",
	"Dock Man\0", NULL, "Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, dockmanRomInfo, dockmanRomName, NULL, NULL, NULL, NULL, DockmanInputInfo, DockmanDIPInfo,
	IntrepidInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x28,
	224, 256, 3, 4
};


// Port Man

static struct BurnRomInfo portmanRomDesc[] = {
	{ "pe1",		0x1000, 0xa5cf6083, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "pe2",		0x1000, 0x0b53d48a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "pe3.17",		0x1000, 0x1c923057, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "pe4",		0x1000, 0x555c71ef, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "pe5",		0x1000, 0xf749e2d4, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "pe7.22",		0x0800, 0xd2094e4a, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 #1 Code
	{ "pe6.23",		0x0800, 0x1cf447f4, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "pe8.9",		0x1000, 0x4d8c2974, 3 | BRF_GRA },           //  7 Graphics
	{ "pe9.8",		0x1000, 0x4e4ea162, 3 | BRF_GRA },           //  8

	{ "mb7051.3",		0x0020, 0x6440dc61, 4 | BRF_GRA },           //  9 Color data
};

STD_ROM_PICK(portman)
STD_ROM_FN(portman)

struct BurnDriver BurnDrvPortman = {
	"portman", "dockman", NULL, NULL, "1982",
	"Port Man\0", NULL, "Taito Corporation (Nova Games Ltd. license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, portmanRomInfo, portmanRomName, NULL, NULL, NULL, NULL, DockmanInputInfo, DockmanDIPInfo,
	IntrepidInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x28,
	224, 256, 3, 4
};


// Port Man (Japan)

static struct BurnRomInfo portmanjRomDesc[] = {
	{ "pa1.ic19",		0x1000, 0xa5cf6083, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "pa2.ic18",		0x1000, 0xbc48d16b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "pa3.ic17",		0x1000, 0x1c923057, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "pa4,ic16",		0x1000, 0x555c71ef, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "pa5.ic15",		0x1000, 0x0a48ff6c, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "pa7.ic22",		0x0800, 0xd2094e4a, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 #1 Code
	{ "pa6.ic23",		0x0800, 0x1cf447f4, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "pa9.ic9",		0x1000, 0x4d8c2974, 3 | BRF_GRA },           //  7 Graphics
	{ "pa8.ic8",		0x1000, 0x4e4ea162, 3 | BRF_GRA },           //  8

	{ "mb7051.3",		0x0020, 0x6440dc61, 4 | BRF_GRA },           //  9 Color data
};

STD_ROM_PICK(portmanj)
STD_ROM_FN(portmanj)

struct BurnDriver BurnDrvPortmanj = {
	"portmanj", "dockman", NULL, NULL, "1982",
	"Port Man (Japan)\0", NULL, "Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, portmanjRomInfo, portmanjRomName, NULL, NULL, NULL, NULL, DockmanInputInfo, DockmanDIPInfo,
	IntrepidInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x28,
	224, 256, 3, 4
};


// Super Mouse

static struct BurnRomInfo suprmousRomDesc[] = {
	{ "sm.1",		0x1000, 0x9db2b786, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "sm.2",		0x1000, 0x0a3d91d3, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sm.3",		0x1000, 0x32af6285, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sm.4",		0x1000, 0x46091524, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "sm.5",		0x1000, 0xf15fd5d2, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "sm.6",		0x1000, 0xfba71785, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 #1 Code

	{ "sm.8",		0x1000, 0x2f81ab5f, 3 | BRF_GRA },           //  6 Graphics
	{ "sm.9",		0x1000, 0x8463af89, 3 | BRF_GRA },           //  7
	{ "sm.7",		0x1000, 0x1d476696, 3 | BRF_GRA },           //  8

	{ "smouse2.clr",	0x0020, 0x8c295553, 4 | BRF_GRA },           //  9 Color data
	{ "smouse1.clr",	0x0020, 0xd815504b, 4 | BRF_GRA },           // 10
};

STD_ROM_PICK(suprmous)
STD_ROM_FN(suprmous)

static INT32 SuprmousInit()
{
	intrepid = 1;
	sprite_bank = 1;

	return DrvInit();
}

struct BurnDriver BurnDrvSuprmous = {
	"suprmous", NULL, NULL, NULL, "1982",
	"Super Mouse\0", NULL, "Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, suprmousRomInfo, suprmousRomName, NULL, NULL, NULL, NULL, IntrepidInputInfo, SuprmousDIPInfo,
	SuprmousInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x28,
	224, 256, 3, 4
};


// Funny Mouse (Japan)

static struct BurnRomInfo funnymouRomDesc[] = {
	{ "fm.1",		0x1000, 0xad72b467, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "fm.2",		0x1000, 0x53f5be5e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "fm.3",		0x1000, 0xb5b8d34d, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "fm.4",		0x1000, 0x603333df, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "fm.5",		0x1000, 0x2ef9cbf1, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "fm.6",		0x1000, 0xfba71785, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 #1 Code

	{ "fm.8",		0x1000, 0xdbef9db8, 3 | BRF_GRA },           //  6 Graphics
	{ "fm.9",		0x1000, 0x700d996e, 3 | BRF_GRA },           //  7
	{ "fm.7",		0x1000, 0xe9295071, 3 | BRF_GRA },           //  8

	{ "smouse2.clr",	0x0020, 0x8c295553, 4 | BRF_GRA },           //  9 Color data
	{ "smouse1.clr",	0x0020, 0xd815504b, 4 | BRF_GRA },           // 10
};

STD_ROM_PICK(funnymou)
STD_ROM_FN(funnymou)

struct BurnDriver BurnDrvFunnymou = {
	"funnymou", "suprmous", NULL, NULL, "1982",
	"Funny Mouse (Japan)\0", NULL, "Taito Corporation (Chuo Co. Ltd license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, funnymouRomInfo, funnymouRomName, NULL, NULL, NULL, NULL, IntrepidInputInfo, SuprmousDIPInfo,
	SuprmousInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x28,
	224, 256, 3, 4
};


// Macho Mouse

static struct BurnRomInfo machomouRomDesc[] = {
	{ "mm1.2g",		0x1000, 0x91f116be, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "mm2.2h",		0x1000, 0x3aa88c9b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mm3.2i",		0x1000, 0x3b66b519, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "mm4.2j",		0x1000, 0xd4f99896, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "mm5.3f",		0x1000, 0x5bfc3874, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "mm6.e6",		0x1000, 0x20816913, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 #1 Code

	{ "mm8.3c",		0x1000, 0x062e77cb, 3 | BRF_GRA },           //  6 Graphics
	{ "mm9.3a",		0x1000, 0xa2f0cfb3, 3 | BRF_GRA },           //  7
	{ "mm7.3d",		0x1000, 0xa6f60ed2, 3 | BRF_GRA },           //  8

	{ "mm-pr1.1",		0x0020, 0xe4babc1f, 4 | BRF_GRA },           //  9 Color data
	{ "mm-pr2.2",		0x0020, 0x9a4919ed, 4 | BRF_GRA },           // 10
};

STD_ROM_PICK(machomou)
STD_ROM_FN(machomou)

struct BurnDriver BurnDrvMachomou = {
	"machomou", NULL, NULL, NULL, "1982",
	"Macho Mouse\0", NULL, "Techstar", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, machomouRomInfo, machomouRomName, NULL, NULL, NULL, NULL, IntrepidInputInfo, SuprmousDIPInfo,
	SuprmousInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x28,
	224, 256, 3, 4
};


// Romar Triv

static struct BurnRomInfo rtrivRomDesc[] = {
	{ "rtriv-e.p1",		0x1000, 0xf3c74f58, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "rtriv-e.p2",		0x1000, 0xd6ba213f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rtriv-e.p3",		0x1000, 0xb8cf20cd, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rtriv-i.fc1",	0x1000, 0xbe5dca69, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "ngames7.22",		0x0800, 0x871e5a03, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code

	{ "ngames9.9",		0x1000, 0xdb553afc, 3 | BRF_GRA },           //  5 Graphics
	{ "ngames9.9",		0x1000, 0xdb553afc, 3 | BRF_GRA },           //  5 Graphics
	{ "ngames8.8",		0x1000, 0xf7644e1d, 3 | BRF_GRA },           //  6

	{ "rtriv.ic3",		0x0020, 0x927ff40a, 4 | BRF_GRA },           //  7 Color data

	{ "rtriv-1f.d0",	0x8000, 0x84787af0, 5 | BRF_PRG | BRF_ESS }, //  8 Question ROMs
	{ "rtriv-1f.d1",	0x8000, 0xff718059, 5 | BRF_PRG | BRF_ESS }, //  9
	{ "rtriv-1f.l0",	0x8000, 0xea43fdea, 5 | BRF_PRG | BRF_ESS }, // 10
	{ "rtriv-1f.l1",	0x8000, 0x6e15f4e2, 5 | BRF_PRG | BRF_ESS }, // 11
	{ "rtriv-1f.l2",	0x8000, 0xecb9fc3c, 5 | BRF_PRG | BRF_ESS }, // 12
	{ "rtriv-1f.t0",	0x8000, 0xca82b8a6, 5 | BRF_PRG | BRF_ESS }, // 13
	{ "rtriv-1f.t1",	0x8000, 0x56c24a9c, 5 | BRF_PRG | BRF_ESS }, // 14
	{ "rtriv-1f.t2",	0x8000, 0xe62917cf, 5 | BRF_PRG | BRF_ESS }, // 15
};

STD_ROM_PICK(rtriv)
STD_ROM_FN(rtriv)

struct BurnDriverD BurnDrvRtriv = {
	"rtriv", NULL, NULL, NULL, "198?",
	"Romar Triv\0", "Incorrect colors", "Romar", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_QUIZ, 0,
	NULL, rtrivRomInfo, rtrivRomName, NULL, NULL, NULL, NULL, RtrivInputInfo, RtrivDIPInfo,
	IntrepidInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x28,
	224, 256, 3, 4
};


// Desert Dan

static struct BurnRomInfo desertdnRomDesc[] = {
	{ "rs5.bin",		0x1000, 0x3a48f53e, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "rs6.bin",		0x1000, 0x3b6125e9, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rs7.bin",		0x1000, 0x2f793ca4, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rs8.bin",		0x1000, 0x52674db3, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "rs2.bin",		0x1000, 0xd0b78243, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "rs3.bin",		0x1000, 0x54a0d133, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "rs4.bin",		0x1000, 0x72d79d62, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "rs9.bin",		0x1000, 0x6daf40ca, 2 | BRF_PRG | BRF_ESS }, //  7 Z80 #1 Code
	{ "rs10.bin",		0x1000, 0xf4fc2c53, 2 | BRF_PRG | BRF_ESS }, //  8

	{ "rs0.bin",		0x1000, 0x8eb856e8, 3 | BRF_GRA },           //  9 Graphics
	{ "rs1.bin",		0x1000, 0xc051b090, 3 | BRF_GRA },           // 10

	{ "mb7051.8j",		0x0020, 0xa14111f4, 4 | BRF_GRA },           // 11 Color data
};

STD_ROM_PICK(desertdn)
STD_ROM_FN(desertdn)

static INT32 DesertdnInit()
{
	sprite_bank = 1;
	desertdn = 1;

	return DrvInit();
}

struct BurnDriver BurnDrvDesertdn = {
	"desertdn", NULL, NULL, NULL, "1982",
	"Desert Dan\0", NULL, "Video Optics", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, desertdnRomInfo, desertdnRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DesertdnDIPInfo,
	DesertdnInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x28,
	256, 224, 4, 3
};
