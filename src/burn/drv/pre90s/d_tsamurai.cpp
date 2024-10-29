// FB Neo Samurai Nihon-Ichi driver module
// Based on MAME driver by Phil Stroffolino

// vsgongf - player is black (normal!)
// sound volumes seem a bit off (music should be quieter?)

#include "tiles_generic.h"
#include "z80_intf.h"
#include "dac.h"
#include "ay8910.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvZ80ROM2;
static UINT8 *DrvZ80ROM3;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvZ80RAM2;
static UINT8 *DrvZ80RAM3;
static UINT8 *DrvFgVidRAM;
static UINT8 *DrvFgColRAM;
static UINT8 *DrvBgVidRAM;
static UINT8 *DrvSprRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 flipscreen;
static UINT8 scrollx;
static UINT8 scrolly;
static UINT8 nmi_enable;
static UINT8 nmi_enable2;
static UINT8 soundlatch0;
static UINT8 soundlatch1;
static UINT8 soundlatch2;
static UINT8 back_color;
static UINT8 textbank0;
static UINT8 textbank1;

static INT32 game_select = 0;
static INT32 the26thz = 0;
static UINT16 vsgongf_protval = 0;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvInputs[3];
static UINT8 DrvDips[2];
static UINT8 DrvReset;

static struct BurnInputInfo TsamuraiInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Tsamurai)

static struct BurnInputInfo YamagchiInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Yamagchi)

static struct BurnDIPInfo TsamuraiDIPList[]=
{
	{0x12, 0xff, 0xff, 0x00, NULL			},
	{0x13, 0xff, 0xff, 0xfc, NULL			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x12, 0x01, 0x07, 0x07, "6 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x06, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x04, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x00, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x07, 0x05, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0x07, 0x01, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x07, 0x02, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x07, 0x03, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x12, 0x01, 0x38, 0x38, "6 Coins 1 Credits"	},
	{0x12, 0x01, 0x38, 0x30, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x38, 0x20, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x38, 0x00, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x38, 0x28, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0x38, 0x08, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x38, 0x10, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x38, 0x18, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x12, 0x01, 0x40, 0x00, "Off"			},
	{0x12, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x12, 0x01, 0x80, 0x00, "Upright"		},
	{0x12, 0x01, 0x80, 0x80, "Cocktail"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0x03, 0x00, "3"			},
	{0x13, 0x01, 0x03, 0x01, "5"			},
	{0x13, 0x01, 0x03, 0x02, "7"			},
	{0x13, 0x01, 0x03, 0x03, "254 (Cheat)"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x40, 0x00, "Off"			},
	{0x13, 0x01, 0x40, 0x40, "On"			},
};

STDDIPINFO(Tsamurai)

static struct BurnDIPInfo LadymstrDIPList[]=
{
	{0x12, 0xff, 0xff, 0x00, NULL			},
	{0x13, 0xff, 0xff, 0xfc, NULL			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x12, 0x01, 0x07, 0x07, "6 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x06, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x04, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x00, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x07, 0x05, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0x07, 0x01, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x07, 0x02, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x07, 0x03, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x12, 0x01, 0x38, 0x38, "6 Coins 1 Credits"	},
	{0x12, 0x01, 0x38, 0x30, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x38, 0x20, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x38, 0x00, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x38, 0x28, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0x38, 0x08, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x38, 0x10, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x38, 0x18, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x12, 0x01, 0x40, 0x00, "Off"			},
	{0x12, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x12, 0x01, 0x80, 0x00, "Upright"		},
	{0x12, 0x01, 0x80, 0x80, "Cocktail"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0x03, 0x00, "3"			},
	{0x13, 0x01, 0x03, 0x01, "5"			},
	{0x13, 0x01, 0x03, 0x02, "7"			},
	{0x13, 0x01, 0x03, 0x03, "254 (Cheat)"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x40, 0x00, "Off"			},
	{0x13, 0x01, 0x40, 0x40, "On"			},
};

STDDIPINFO(Ladymstr)

static struct BurnDIPInfo NunchakuDIPList[]=
{
	{0x12, 0xff, 0xff, 0x00, NULL			},
	{0x13, 0xff, 0xff, 0xfc, NULL			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x12, 0x01, 0x07, 0x07, "6 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x06, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x04, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x00, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x07, 0x05, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0x07, 0x01, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x07, 0x02, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x07, 0x03, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x12, 0x01, 0x38, 0x38, "6 Coins 1 Credits"	},
	{0x12, 0x01, 0x38, 0x30, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x38, 0x20, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x38, 0x00, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x38, 0x28, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0x38, 0x08, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x38, 0x10, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x38, 0x18, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x12, 0x01, 0x40, 0x00, "Off"			},
	{0x12, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x12, 0x01, 0x80, 0x00, "Upright"		},
	{0x12, 0x01, 0x80, 0x80, "Cocktail"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0x03, 0x00, "3"			},
	{0x13, 0x01, 0x03, 0x01, "5"			},
	{0x13, 0x01, 0x03, 0x02, "7"			},
	{0x13, 0x01, 0x03, 0x03, "255 (Cheat)"		},
};

STDDIPINFO(Nunchaku)

static struct BurnDIPInfo YamagchiDIPList[]=
{
	{0x12, 0xff, 0xff, 0x00, NULL			},
	{0x13, 0xff, 0xff, 0xfc, NULL			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x12, 0x01, 0x07, 0x07, "6 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x06, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x04, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x00, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x07, 0x05, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0x07, 0x01, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x07, 0x02, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x07, 0x03, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x12, 0x01, 0x38, 0x38, "6 Coins 1 Credits"	},
	{0x12, 0x01, 0x38, 0x30, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x38, 0x20, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x38, 0x00, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x38, 0x28, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0x38, 0x08, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x38, 0x10, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x38, 0x18, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x12, 0x01, 0x40, 0x00, "Off"			},
	{0x12, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x12, 0x01, 0x80, 0x00, "Upright"		},
	{0x12, 0x01, 0x80, 0x80, "Cocktail"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0x03, 0x00, "3"			},
	{0x13, 0x01, 0x03, 0x01, "5"			},
	{0x13, 0x01, 0x03, 0x02, "7"			},
	{0x13, 0x01, 0x03, 0x03, "255 (Cheat)"		},

	{0   , 0xfe, 0   ,    2, "Language"		},
	{0x13, 0x01, 0x10, 0x10, "English"		},
	{0x13, 0x01, 0x10, 0x00, "Japanese"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x40, 0x00, "Off"			},
	{0x13, 0x01, 0x40, 0x40, "On"			},
};

STDDIPINFO(Yamagchi)

static struct BurnDIPInfo M660DIPList[]=
{
	{0x12, 0xff, 0xff, 0x00, NULL			},
	{0x13, 0xff, 0xff, 0xbc, NULL			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x12, 0x01, 0x07, 0x04, "5 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x03, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x02, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x01, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x00, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x07, 0x07, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0x07, 0x05, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x07, 0x06, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x12, 0x01, 0x38, 0x20, "5 Coins 1 Credits"	},
	{0x12, 0x01, 0x38, 0x18, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0x38, 0x10, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x38, 0x08, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x38, 0x00, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x38, 0x38, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0x38, 0x28, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x38, 0x30, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x12, 0x01, 0x40, 0x00, "Off"			},
	{0x12, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Continues"		},
	{0x12, 0x01, 0x80, 0x00, "Off"			},
	{0x12, 0x01, 0x80, 0x80, "On"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0x03, 0x00, "3"			},
	{0x13, 0x01, 0x03, 0x01, "4"			},
	{0x13, 0x01, 0x03, 0x02, "5"			},
	{0x13, 0x01, 0x03, 0x03, "6"			},

	{0   , 0xfe, 0   ,    4, "Bonus"		},
	{0x13, 0x01, 0x0c, 0x00, "10,30,50"		},
	{0x13, 0x01, 0x0c, 0x04, "20,50,80"		},
	{0x13, 0x01, 0x0c, 0x08, "30,70,110"		},
	{0x13, 0x01, 0x0c, 0x0c, "50,100,150"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x30, 0x00, "Easy"			},
	{0x13, 0x01, 0x30, 0x10, "Normal"		},
	{0x13, 0x01, 0x30, 0x20, "Hard"			},
	{0x13, 0x01, 0x30, 0x30, "Very Hard"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x13, 0x01, 0x40, 0x00, "Upright"		},
	{0x13, 0x01, 0x40, 0x40, "Cocktail"		},
};

STDDIPINFO(M660)

static struct BurnDIPInfo VsgongfDIPList[]=
{
	{0x12, 0xff, 0xff, 0x00, NULL			},
	{0x13, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x12, 0x01, 0x07, 0x07, "6 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x06, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x04, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x00, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x07, 0x05, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0x07, 0x01, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x07, 0x02, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x07, 0x03, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x12, 0x01, 0x38, 0x38, "6 Coins 1 Credits"	},
	{0x12, 0x01, 0x38, 0x30, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x38, 0x20, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x38, 0x00, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x38, 0x28, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0x38, 0x08, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x38, 0x10, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x38, 0x18, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x12, 0x01, 0x40, 0x00, "Off"			},
	{0x12, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x12, 0x01, 0x80, 0x00, "Upright"		},
	{0x12, 0x01, 0x80, 0x80, "Cocktail"		},
};

STDDIPINFO(Vsgongf)

static void __fastcall tsamurai_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xf400:	// nop
		return;

		case 0xf401:
			soundlatch0 = data;
			ZetSetIRQLine(1, 0, CPU_IRQSTATUS_ACK);
		return;

		case 0xf402:
			soundlatch1 = data;
			ZetSetIRQLine(2, 0, CPU_IRQSTATUS_ACK);
		return;

		case 0xf801:
			back_color = data;
		return;

		case 0xf802:
			scrolly = data;
		return;

		case 0xf803:
			scrollx = data;
		return;

		case 0xfc00:
			flipscreen = data ? 1 : 0;
		return;

		case 0xfc01:
			nmi_enable = data ? 1 : 0;
		return;

		case 0xfc02:
			textbank0 = data;
		return;

		case 0xfc03:
		case 0xfc04:
			// coin_counter
		return;
	}
}

static UINT8 __fastcall tsamurai_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xa003:
			return 0;

		case 0xa006:
			return vsgongf_protval & 0xff;

		case 0xa100:
			return vsgongf_protval >> 8;

		case 0xd803:
			return 0x6b;

		case 0xd806:
			return 0x40;

		case 0xd900:
			return 0x6a;

		case 0xd938:
			return 0xfb;

		case 0xf800:
		case 0xf801:
		case 0xf802:
			return DrvInputs[address & 3];

		case 0xf804:
		case 0xf805:
			return DrvDips[address & 1];
	}

	return 0;
}

static void __fastcall m660_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xf401:
			soundlatch2 = data;
			ZetSetIRQLine(3, 0, CPU_IRQSTATUS_ACK);
		return;

		case 0xf402:
			soundlatch1 = data;
			ZetSetIRQLine(2, 0, CPU_IRQSTATUS_ACK);
		return;

		case 0xf403:
			soundlatch0 = data;
			ZetSetIRQLine(1, 0, CPU_IRQSTATUS_ACK);
		return;

		case 0xfc07:
			textbank1 = data;
		return;
	}

	tsamurai_main_write(address, data);
}

static UINT8 __fastcall m660_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xd803:
			return 0x53;
	}

	return tsamurai_main_read(address);
}

static void __fastcall vsgongf_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xe800:
			soundlatch0 = data;
			ZetNmi(1);
		return;

		case 0xec00: // nop
		case 0xec01:
		case 0xec02:
		case 0xec03:
		case 0xec04:
		case 0xec05:
		case 0xec06:
		return;

		case 0xf000:
			back_color = data;
		return;

		case 0xf400: // nop
		case 0xf800:
		case 0xf801:
		case 0xf803:
		case 0xfc00:
		return;

		case 0xfc01:
			nmi_enable = data ? 1 : 0;
		return;

		case 0xfc02: // coin counter
		case 0xfc03:
		return;

		case 0xfc04:
			textbank0 = data;
		return;
	}
}

static void __fastcall tsamurai_main_out_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
			AY8910Write(0, port & 1, data);
		return;
	}
}


static void __fastcall m660_main_out_port(UINT16 port, UINT8 /*data*/)
{
	switch (port & 0xff)
	{
		case 0x00: // ?
		case 0x01: // ?
		case 0x02: // ?
		return;
	}
}

static void __fastcall tsamurai_sound0_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x6001: // nop
		case 0xc001:
		return;

		case 0x6002:
		case 0xc002:
			DACWrite(0, data);
		return;
	}
}

static UINT8 __fastcall tsamurai_sound0_read(UINT16 address)
{
	switch (address)
	{
		case 0x6000:
		case 0xc000:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			return soundlatch0;
	}

	return 0;
}

static void __fastcall tsamurai_sound1_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x6001: // nop
		case 0xc001:
		return;

		case 0x6002:
		case 0xc002:
			DACWrite(1, data);
		return;
	}
}

static UINT8 __fastcall tsamurai_sound1_read(UINT16 address)
{
	switch (address)
	{
		case 0x6000:
		case 0xc000:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			return soundlatch1;
	}

	return 0;
}

static UINT8 __fastcall m660_sound2_read(UINT16 address)
{
	switch (address)
	{
		case 0xc000:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			return soundlatch2;
	}

	return 0;
}

static void __fastcall vsgongf_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x8000:
			nmi_enable2 = data ? 1 : 0;
		return;

		case 0xa000:
			DACWrite(0, data);
		return;
	}
}

static UINT8 __fastcall vsgongf_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x8000:
			return soundlatch0;
	}

	return 0;
}

static INT32 DrvSyncDAC()
{
	return (INT32)(float)(nBurnSoundLen * (ZetTotalCycles() / (3000000.000 / 60)));
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetReset(0);
	ZetReset(1);

	if (game_select == 1 || game_select == 2) {
		ZetReset(2);
	}

	if (game_select == 2) {
		ZetReset(3);
	}

	AY8910Reset(0);
	DACReset();

	flipscreen = 0;
	scrollx = 0;
	scrolly = 0;
	nmi_enable = 0;
	nmi_enable2 = 0;
	soundlatch0 = 0;
	soundlatch1 = 0;
	soundlatch2 = 0;
	back_color = 0;
	textbank0 = 0;
	textbank1 = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x00c000;
	DrvZ80ROM1		= Next; Next += 0x004000;
	DrvZ80ROM2		= Next; Next += 0x004000;
	DrvZ80ROM3		= Next; Next += 0x004000;

	DrvGfxROM0		= Next; Next += 0x020000;
	DrvGfxROM1		= Next; Next += 0x010000;
	DrvGfxROM2		= Next; Next += 0x020000;

	DrvColPROM		= Next; Next += 0x000300;

	DrvPalette		= (UINT32*)Next; Next += 0x0100 * sizeof(INT32);

	AllRam			= Next;

	DrvZ80RAM0		= Next; Next += 0x001000;
	DrvZ80RAM1		= Next; Next += 0x000800;
	DrvZ80RAM2		= Next; Next += 0x000800;
	DrvZ80RAM3		= Next; Next += 0x000800;
	DrvFgVidRAM		= Next; Next += 0x000400;
	DrvFgColRAM		= Next; Next += 0x000400;
	DrvBgVidRAM		= Next; Next += 0x000800;
	DrvSprRAM		= Next; Next += 0x000400;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x100; i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 0x01;
		INT32 bit3 = (DrvColPROM[i] >> 3) & 0x01;
		INT32 r = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[i + 0x100] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + 0x100] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + 0x100] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + 0x100] >> 3) & 0x01;
		INT32 g = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[i + 2*0x100] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + 2*0x100] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + 2*0x100] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + 2*0x100] >> 3) & 0x01;
		INT32 b = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static INT32 DrvGfxDecode(INT32 nLen0, INT32 nLen1, INT32 nLen2)
{
	INT32 Plane0[3] = { (nLen0/3)*8*2, (nLen0/3)*8*1, (nLen0/3)*8*0 };
	INT32 Plane1[3] = { (nLen1/3)*8*2, (nLen1/3)*8*1, (nLen1/3)*8*0 };
	INT32 Plane2[3] = { (nLen2/3)*8*2, (nLen2/3)*8*1, (nLen2/3)*8*0 };
	INT32 XOffs[32] = { STEP8(0,1), STEP8(64, 1), STEP8(128,1), STEP8(192,1) };
	INT32 YOffs[32] = { STEP8(0,8), STEP8(256,8), STEP8(512,8), STEP8(768,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0xc000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, nLen0);

	GfxDecode(((nLen0/3)*8)/0x040, 3,  8,  8, Plane0, XOffs, YOffs, 0x040, tmp, DrvGfxROM0);

	if (nLen0 == 0x6000) memcpy (DrvGfxROM0 + 0x10000, DrvGfxROM0, 0x10000); // mirror

	memcpy (tmp, DrvGfxROM1, nLen1);

	GfxDecode(((nLen1/3)*8)/0x040, 3,  8,  8, Plane1, XOffs, YOffs, 0x040, tmp, DrvGfxROM1);

	if (nLen1 == 0x3000) memcpy (DrvGfxROM1 + 0x08000, DrvGfxROM1, 0x08000); // mirror

	memcpy (tmp, DrvGfxROM2, nLen2);

	GfxDecode(((nLen2/3)*8)/0x400, 3, 32, 32, Plane2, XOffs, YOffs, 0x400, tmp, DrvGfxROM2);

	BurnFree (tmp);

	return 0;
}

static INT32 DrvInit(INT32 game)
{
	game_select = 1;

	BurnAllocMemIndex();

	{
		if (game == 0) {
			if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0 + 0x04000,  1, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0 + 0x08000,  2, 1)) return 1;

			if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  3, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM1 + 0x02000,  4, 1)) return 1;

			if (BurnLoadRom(DrvZ80ROM2 + 0x00000,  5, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM0 + 0x00000,  6, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x02000,  7, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x04000,  8, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM1 + 0x00000,  9, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x01000, 10, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x02000, 11, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM2 + 0x00000, 12, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x04000, 13, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x08000, 14, 1)) return 1;

			if (BurnLoadRom(DrvColPROM + 0x00000, 15, 1)) return 1;
			if (BurnLoadRom(DrvColPROM + 0x00100, 16, 1)) return 1;
			if (BurnLoadRom(DrvColPROM + 0x00200, 17, 1)) return 1;
		}
		else if (game == 1)
		{
			if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0 + 0x04000,  1, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0 + 0x08000,  2, 1)) return 1;

			if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  3, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM1 + 0x02000,  4, 1)) return 1;

			if (BurnLoadRom(DrvZ80ROM2 + 0x00000,  5, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM2 + 0x02000,  6, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM0 + 0x00000,  7, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x02000,  8, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x04000,  9, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM1 + 0x00000, 10, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x01000, 11, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x02000, 12, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM2 + 0x00000, 13, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x04000, 14, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x08000, 15, 1)) return 1;

			if (BurnLoadRom(DrvColPROM + 0x00000, 16, 1)) return 1;
			if (BurnLoadRom(DrvColPROM + 0x00100, 17, 1)) return 1;
			if (BurnLoadRom(DrvColPROM + 0x00200, 18, 1)) return 1;
		}
		else if (game == 2)
		{
			if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0 + 0x04000,  1, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0 + 0x08000,  2, 1)) return 1;

			if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  3, 1)) return 1;

			if (BurnLoadRom(DrvZ80ROM2 + 0x00000,  4, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM0 + 0x00000,  5, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x02000,  6, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x04000,  7, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM1 + 0x00000,  8, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x01000,  9, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x02000, 10, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM2 + 0x00000, 11, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x04000, 12, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x08000, 13, 1)) return 1;

			if (BurnLoadRom(DrvColPROM + 0x00000, 14, 1)) return 1;
			if (BurnLoadRom(DrvColPROM + 0x00100, 15, 1)) return 1;
			if (BurnLoadRom(DrvColPROM + 0x00200, 16, 1)) return 1;
		}

		DrvGfxDecode(0x6000, 0x3000, 0xc000);
		DrvPaletteInit();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,	0xc000, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvFgVidRAM,	0xe000, 0xe3ff, MAP_RAM);
	ZetMapMemory(DrvFgColRAM,	0xe400, 0xe7ff, MAP_RAM);
	ZetMapMemory(DrvBgVidRAM,	0xe800, 0xefff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xf000, 0xf3ff, MAP_RAM);
	ZetSetWriteHandler(tsamurai_main_write);
	ZetSetReadHandler(tsamurai_main_read);
	ZetSetOutHandler(tsamurai_main_out_port);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,	0x7f00, 0x7fff, MAP_RAM);
	ZetSetWriteHandler(tsamurai_sound0_write);
	ZetSetReadHandler(tsamurai_sound0_read);
	ZetClose();

	ZetInit(2);
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM2,	0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM2,	0x7f00, 0x7fff, MAP_RAM);
	ZetSetWriteHandler(tsamurai_sound1_write);
	ZetSetReadHandler(tsamurai_sound1_read);
	ZetClose();

	AY8910Init(0, 3000000, 1);
	AY8910SetAllRoutes(0, 0.10, BURN_SND_ROUTE_BOTH);

	DACInit(0, 0, 0, DrvSyncDAC);
	DACInit(1, 1, 0, DrvSyncDAC);
	DACSetRoute(0, 0.20, BURN_SND_ROUTE_BOTH);
	DACSetRoute(1, 0.20, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 m660CommonInit(INT32 game)
{
	game_select = 2;

	BurnAllocMemIndex();

	{
		if (game == 0)
		{
			if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0 + 0x04000,  1, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0 + 0x08000,  2, 1)) return 1;

			if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  3, 1)) return 1;

			if (BurnLoadRom(DrvZ80ROM2 + 0x00000,  4, 1)) return 1;

			if (BurnLoadRom(DrvZ80ROM3 + 0x00000,  5, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM0 + 0x00000,  6, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x04000,  7, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x08000,  8, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM1 + 0x00000,  9, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x02000, 10, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x04000, 11, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM2 + 0x00000, 12, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x04000, 13, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x08000, 14, 1)) return 1;

			if (BurnLoadRom(DrvColPROM + 0x00000, 15, 1)) return 1;
			if (BurnLoadRom(DrvColPROM + 0x00100, 16, 1)) return 1;
			if (BurnLoadRom(DrvColPROM + 0x00200, 17, 1)) return 1;
	} else { // m660j
			if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0 + 0x04000,  1, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0 + 0x08000,  2, 1)) return 1;

			if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  3, 1)) return 1;

			if (BurnLoadRom(DrvZ80ROM2 + 0x00000,  4, 1)) return 1;

			if (BurnLoadRom(DrvZ80ROM3 + 0x00000,  5, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM3 + 0x04000,  6, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM0 + 0x00000,  7, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x04000,  8, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x08000,  9, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM1 + 0x00000, 10, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x02000, 11, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x04000, 12, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM2 + 0x00000, 13, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x04000, 14, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x08000, 15, 1)) return 1;

			if (BurnLoadRom(DrvColPROM + 0x00000, 16, 1)) return 1;
			if (BurnLoadRom(DrvColPROM + 0x00100, 17, 1)) return 1;
			if (BurnLoadRom(DrvColPROM + 0x00200, 18, 1)) return 1;
		}

	DrvGfxDecode(0xc000, 0x6000, 0xc000);
	DrvPaletteInit();
	}
	
	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,	0xc000, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvFgVidRAM,	0xe000, 0xe3ff, MAP_RAM);
	ZetMapMemory(DrvFgColRAM,	0xe400, 0xe7ff, MAP_RAM);
	ZetMapMemory(DrvBgVidRAM,	0xe800, 0xefff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xf000, 0xf3ff, MAP_RAM);
	ZetSetWriteHandler(m660_main_write);
	ZetSetReadHandler(the26thz ? tsamurai_main_read : m660_main_read);
	ZetSetOutHandler(m660_main_out_port);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,	0x8000, 0x87ff, MAP_RAM);
	ZetSetWriteHandler(tsamurai_sound0_write);
	ZetSetReadHandler(tsamurai_sound0_read);
	ZetClose();

	ZetInit(2);
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM2,	0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM2,	0x8000, 0x87ff, MAP_RAM);
	ZetSetWriteHandler(tsamurai_sound1_write);
	ZetSetReadHandler(tsamurai_sound1_read);
	ZetClose();

	ZetInit(3);
	ZetOpen(3);
	ZetMapMemory(DrvZ80ROM3,	0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM3,	0x8000, 0x87ff, MAP_RAM);
//	ZetSetWriteHandler(m660_sound2_write); // nothing useful written...
	ZetSetReadHandler(m660_sound2_read);
	ZetSetOutHandler(tsamurai_main_out_port); // re-use this since it's the same
	ZetClose();

	AY8910Init(0, 3000000, 1);
	AY8910SetAllRoutes(0, 0.10, BURN_SND_ROUTE_BOTH);

	DACInit(0, 0, 0, DrvSyncDAC);
	DACSetRoute(0, 0.20, BURN_SND_ROUTE_BOTH);

	DACInit(1, 1, 0, DrvSyncDAC);
	DACSetRoute(1, 0.20, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 VsgongfCommonInit(INT32 game)
{
	game_select = 3;

	BurnAllocMemIndex();

	{
		if (game == 0)
		{
			if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0 + 0x02000,  1, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0 + 0x04000,  2, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0 + 0x06000,  3, 1)) return 1;
	
			if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  4, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM1 + 0x02000,  5, 1)) return 1;
	
			if (BurnLoadRom(DrvGfxROM1 + 0x00000,  6, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x01000,  7, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x02000,  8, 1)) return 1;
	
			if (BurnLoadRom(DrvGfxROM2 + 0x00000,  9, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x02000, 10, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x04000, 11, 1)) return 1;
	
			if (BurnLoadRom(DrvColPROM + 0x00000, 12, 1)) return 1;
			if (BurnLoadRom(DrvColPROM + 0x00100, 13, 1)) return 1;
			if (BurnLoadRom(DrvColPROM + 0x00200, 14, 1)) return 1;
		} else {
			if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0 + 0x02000,  1, 1)) return 1;
	
			if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  2, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM1 + 0x02000,  3, 1)) return 1;
	
			if (BurnLoadRom(DrvGfxROM1 + 0x00000,  4, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x01000,  5, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x02000,  6, 1)) return 1;
	
			if (BurnLoadRom(DrvGfxROM2 + 0x00000,  7, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x02000,  8, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x04000,  9, 1)) return 1;
	
			if (BurnLoadRom(DrvColPROM + 0x00000, 10, 1)) return 1;
			if (BurnLoadRom(DrvColPROM + 0x00100, 11, 1)) return 1;
			if (BurnLoadRom(DrvColPROM + 0x00200, 12, 1)) return 1;
		}

		DrvGfxDecode(0x0300, 0x3000, 0x6000);
		DrvPaletteInit();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,	0xc000, 0xc7ff, MAP_RAM);
	ZetMapMemory(DrvFgVidRAM,	0xe000, 0xe3ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xe400, 0xe4ff, MAP_RAM);
	ZetSetWriteHandler(vsgongf_main_write);
	ZetSetReadHandler(tsamurai_main_read); // mostly the same
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,	0x6000, 0x63ff, MAP_RAM);
	ZetSetWriteHandler(vsgongf_sound_write);
	ZetSetReadHandler(vsgongf_sound_read);
	ZetSetOutHandler(tsamurai_main_out_port); // re-use this since it's the same
	ZetClose();

	AY8910Init(0, 3000000, 1);
	AY8910SetAllRoutes(0, 0.10, BURN_SND_ROUTE_BOTH);

	DACInit(0, 0, 0, DrvSyncDAC);
	DACSetRoute(0, 0.20, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	DACExit();
	AY8910Exit(0);

	ZetExit();

	the26thz = 0;

	BurnFreeMemIndex();

	return 0;
}

static void draw_bg_layer()
{
	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sx = (offs & 0x1f) * 8;
		INT32 sy = (offs / 0x20) * 8;

		sx -= scrollx;
		if (sx < -7) sx += 256;
		sy -= (scrolly + 16) & 0xff;
		if (sy < -7) sy += 256;

		INT32 attr = DrvBgVidRAM[offs * 2 + 1];
		INT32 code = DrvBgVidRAM[offs * 2 + 0] + ((attr & 0xc0) << 2) + ((attr & 0x20) << 5);

		Render8x8Tile_Clip(pTransDraw, code, sx, sy, attr & 0x1f, 3, 0, DrvGfxROM0);
	}
}

static void draw_bg_layer_type2()
{
	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sx = (offs & 0x1f) * 8;
		INT32 sy = (offs / 0x20) * 8;

		INT32 code = DrvFgVidRAM[offs] + (textbank0 ? 0x100 : 0);

		Render8x8Tile_Clip(pTransDraw, code, sx, sy - 16, back_color & 0x1f, 3, 0, DrvGfxROM1);
	}
}

static void draw_fg_layer()
{
	INT32 bank = ((textbank0 & 1) * 0x100) + ((textbank1 & 1) * 0x200);

	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sx = (offs & 0x1f) * 8;
		INT32 sy = (offs / 0x20) * 8;

		sy -= (DrvFgColRAM[((offs&0x1f)*2)+0] + 16) & 0xff;
		if (sy < -7) sy += 256;

		INT32 code  = DrvFgVidRAM[offs] + bank;
		INT32 color = DrvFgColRAM[((offs&0x1f)*2)+1] & 0x1f;

		Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, DrvGfxROM1);

	}
}

static void draw_sprites()
{;
	UINT8 *source = DrvSprRAM + 31*4;
	UINT8 *finish = DrvSprRAM;

	while (source >= finish)
	{
		INT32 sx    = source[3] - 16;
		INT32 sy    = 240 - source[0];
		INT32 code  = source[1];
		INT32 color = source[2] & 0x1f;

		if (sy<-16) sy += 256;

		INT32 flipy = code & 0x80;

		if (flipscreen)
		{
			flipy ^= 0x80;
			sx = 256-32-sx;
			sy = 256-32-sy;
		}

		if (flipy) {
			if (flipscreen) {
				Render32x32Tile_Mask_FlipXY_Clip(pTransDraw, code & 0x7f, sx, sy - 16, color, 3, 0, 0, DrvGfxROM2);
			} else {
				Render32x32Tile_Mask_FlipY_Clip(pTransDraw, code & 0x7f, sx, sy - 16, color, 3, 0, 0, DrvGfxROM2);
			}
		} else {
			if (flipscreen) {
				Render32x32Tile_Mask_FlipX_Clip(pTransDraw, code & 0x7f, sx, sy - 16, color, 3, 0, 0, DrvGfxROM2);
			} else {
				Render32x32Tile_Mask_Clip(pTransDraw, code & 0x7f, sx, sy - 16, color, 3, 0, 0, DrvGfxROM2);
			}
		}

		source -= 4;
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	if (game_select == 3)
	{
		draw_bg_layer_type2();
		draw_sprites();
	}
	else
	{
		for (INT32 i = 0; i < 0x100; i+=8) {
			DrvPalette[i] = DrvPalette[back_color];
		}

		draw_bg_layer();
		draw_sprites();
		draw_fg_layer();
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
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

	INT32 nInterleave = 100;
	INT32 nCyclesTotal[4] = { 3000000 / 60, 3000000 / 60, 3000000 / 60, 3000000 / 60 };
	INT32 nCyclesDone[4] = { 0, 0, 0, 0 };
	
	for (INT32 i = 0; i < nInterleave; i++)
	{
		INT32 nSegment = nCyclesTotal[0] / nInterleave;

		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nSegment);
		if (i == (nInterleave - 10) && nmi_enable) ZetNmi();
		ZetClose();

		nSegment = nCyclesTotal[1] / nInterleave;

		ZetOpen(1);
		nCyclesDone[1] += ZetRun(nSegment);
		if (game_select == 3 && nmi_enable2 && (i == 33 || i == 66 || i == 99)) ZetNmi();
		ZetClose();

		if (game_select == 1 || game_select == 2) {
			nSegment = nCyclesTotal[2] / nInterleave;

			ZetOpen(2);
			nCyclesDone[2] += ZetRun(nSegment);
			ZetClose();
		}

		if (game_select == 2) {
			nSegment = nCyclesTotal[3] / nInterleave;

			ZetOpen(3);
			nCyclesDone[3] += ZetRun(nSegment);
			if (i == (nInterleave - 10)) ZetNmi();
			ZetClose();
		}
	}

	if (pBurnSoundOut) {
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction,INT32 *pnMin)
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
		DACScan(nAction, pnMin);

		SCAN_VAR(flipscreen);
		SCAN_VAR(scrollx);
		SCAN_VAR(scrolly);
		SCAN_VAR(nmi_enable);
		SCAN_VAR(nmi_enable2);
		SCAN_VAR(soundlatch0);
		SCAN_VAR(soundlatch1);
		SCAN_VAR(soundlatch2);
		SCAN_VAR(back_color);
		SCAN_VAR(textbank0);
		SCAN_VAR(textbank1);
	}

	return 0;
}


// Samurai Nihon-Ichi (rev 1)
// there's a protection device labeled B5 at location l3 on the main board

static struct BurnRomInfo tsamuraiRomDesc[] = {
	{ "a35-01-1.3r",	0x4000, 0xd09c8609, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "a35-02-1.3t",	0x4000, 0xd0f2221c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a35.03-1.3v",	0x4000, 0xeee8b0c9, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "a35-14.4e",		0x2000, 0x220e9c04, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code
	{ "a35-15.4c",		0x2000, 0x1e0d1e33, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "a35-13.4j",		0x2000, 0x73feb0e2, 3 | BRF_PRG | BRF_ESS }, //  5 Z80 #2 Code

	{ "a35-04.10a",		0x2000, 0xb97ce9b1, 4 | BRF_GRA },           //  6 Background Tiles
	{ "a35-05.10b",		0x2000, 0x55a17b08, 4 | BRF_GRA },           //  7
	{ "a35-06.10d",		0x2000, 0xf5ee6f8f, 4 | BRF_GRA },           //  8

	{ "a35-10.11n",		0x1000, 0x0b5a0c45, 5 | BRF_GRA },           //  9 Foreground Tiles
	{ "a35-11.11q",		0x1000, 0x93346d75, 5 | BRF_GRA },           // 10
	{ "a35-12.11r",		0x1000, 0xf4c69d8a, 5 | BRF_GRA },           // 11

	{ "a35-07.12h",		0x4000, 0x38fc349f, 6 | BRF_GRA },           // 12 Sprites
	{ "a35-08.12j",		0x4000, 0xa07d6dc3, 6 | BRF_GRA },           // 13
	{ "a35-09.12k",		0x4000, 0xc0784a0e, 6 | BRF_GRA },           // 14

	{ "a35-16.2j",		0x0100, 0x72d8b332, 7 | BRF_GRA },           // 15 Color Proms
	{ "a35-17.2l",		0x0100, 0x9bf1829e, 7 | BRF_GRA },           // 16
	{ "a35-18.2m",		0x0100, 0x918e4732, 7 | BRF_GRA },           // 17
};

STD_ROM_PICK(tsamurai)
STD_ROM_FN(tsamurai)

static INT32 tsamuraiInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvTsamurai = {
	"tsamurai", NULL, NULL, NULL, "1985",
	"Samurai Nihon-Ichi (rev 1)\0", NULL, "Kaneko / Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, tsamuraiRomInfo, tsamuraiRomName, NULL, NULL, NULL, NULL, TsamuraiInputInfo, TsamuraiDIPInfo,
	tsamuraiInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Samurai Nihon-Ichi

static struct BurnRomInfo tsamuraiaRomDesc[] = {
	{ "a35-01.3r",		0x4000, 0x282d96ad, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "a35-02.3t",		0x4000, 0xe3fa0cfa, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a35-03.3v",		0x4000, 0x2fff1e0a, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "a35-14.4e",		0x2000, 0xf10aee3b, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code
	{ "a35-15.4c",		0x2000, 0x1e0d1e33, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "a35-13.4j",		0x2000, 0x3828f4d2, 3 | BRF_PRG | BRF_ESS }, //  5 Z80 #2 Code

	{ "a35-04.10a",		0x2000, 0xb97ce9b1, 4 | BRF_GRA },           //  6 Background Tiles
	{ "a35-05.10b",		0x2000, 0x55a17b08, 4 | BRF_GRA },           //  7
	{ "a35-06.10d",		0x2000, 0xf5ee6f8f, 4 | BRF_GRA },           //  8

	{ "a35-10.11n",		0x1000, 0x0b5a0c45, 5 | BRF_GRA },           //  9 Foreground Tiles
	{ "a35-11.11q",		0x1000, 0x93346d75, 5 | BRF_GRA },           // 10
	{ "a35-12.11r",		0x1000, 0xf4c69d8a, 5 | BRF_GRA },           // 11

	{ "a35-07.12h",		0x4000, 0x38fc349f, 6 | BRF_GRA },           // 12 Sprites
	{ "a35-08.12j",		0x4000, 0xa07d6dc3, 6 | BRF_GRA },           // 13
	{ "a35-09.12k",		0x4000, 0xc0784a0e, 6 | BRF_GRA },           // 14

	{ "a35-16.2j",		0x0100, 0x72d8b332, 7 | BRF_GRA },           // 15 Color Proms
	{ "a35-17.2l",		0x0100, 0x9bf1829e, 7 | BRF_GRA },           // 16
	{ "a35-18.2m",		0x0100, 0x918e4732, 7 | BRF_GRA },           // 17
};

STD_ROM_PICK(tsamuraia)
STD_ROM_FN(tsamuraia)

struct BurnDriver BurnDrvTsamuraia = {
	"tsamuraia", "tsamurai", NULL, NULL, "1985",
	"Samurai Nihon-Ichi\0", NULL, "Kaneko / Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, tsamuraiaRomInfo, tsamuraiaRomName, NULL, NULL, NULL, NULL, TsamuraiInputInfo, TsamuraiDIPInfo,
	tsamuraiInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Samurai Nihon-Ichi (bootleg, harder)

static struct BurnRomInfo tsamuraihRomDesc[] = {
	{ "a35-01h.3r",		0x4000, 0x551e1fd1, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "a35-02.3t",		0x4000, 0xe3fa0cfa, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a35-03.3v",		0x4000, 0x2fff1e0a, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "a35-14.4e",		0x2000, 0xf10aee3b, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code
	{ "a35-15.4c",		0x2000, 0x1e0d1e33, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "a35-13.4j",		0x2000, 0x3828f4d2, 3 | BRF_PRG | BRF_ESS }, //  5 Z80 #2 Code

	{ "a35-04.10a",		0x2000, 0xb97ce9b1, 4 | BRF_GRA },           //  6 Background Tiles
	{ "a35-05.10b",		0x2000, 0x55a17b08, 4 | BRF_GRA },           //  7
	{ "a35-06.10d",		0x2000, 0xf5ee6f8f, 4 | BRF_GRA },           //  8

	{ "a35-10.11n",		0x1000, 0x0b5a0c45, 5 | BRF_GRA },           //  9 Foreground Tiles
	{ "a35-11.11q",		0x1000, 0x93346d75, 5 | BRF_GRA },           // 10
	{ "a35-12.11r",		0x1000, 0xf4c69d8a, 5 | BRF_GRA },           // 11

	{ "a35-07.12h",		0x4000, 0x38fc349f, 6 | BRF_GRA },           // 12 Sprites
	{ "a35-08.12j",		0x4000, 0xa07d6dc3, 6 | BRF_GRA },           // 13
	{ "a35-09.12k",		0x4000, 0xc0784a0e, 6 | BRF_GRA },           // 14

	{ "a35-16.2j",		0x0100, 0x72d8b332, 7 | BRF_GRA },           // 15 Color Proms
	{ "a35-17.2l",		0x0100, 0x9bf1829e, 7 | BRF_GRA },           // 16
	{ "a35-18.2m",		0x0100, 0x918e4732, 7 | BRF_GRA },           // 17
};

STD_ROM_PICK(tsamuraih)
STD_ROM_FN(tsamuraih)

struct BurnDriver BurnDrvTsamuraih = {
	"tsamuraih", "tsamurai", NULL, NULL, "1985",
	"Samurai Nihon-Ichi (bootleg, harder)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, tsamuraihRomInfo, tsamuraihRomName, NULL, NULL, NULL, NULL, TsamuraiInputInfo, TsamuraiDIPInfo,
	tsamuraiInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Lady Master of Kung Fu (rev 1)
// there's a protection device labeled 6 at location l3 on the main board

static struct BurnRomInfo ladymstrRomDesc[] = {
	{ "a49-01-1.3r",	0x4000, 0xacbd0b64, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code // believed to be newer because of the -01 suffix
	{ "a49-02.3t",		0x4000, 0xb0a9020b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a49-03.3v",		0x4000, 0x641c94ed, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "a49-14.4e",		0x2000, 0xd83a3c02, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code
	{ "a49-15.4c",		0x2000, 0xd24ee5fd, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "a49-13.4j",		0x2000, 0x7942bd7c, 3 | BRF_PRG | BRF_ESS }, //  5 Z80 #2 Code

	{ "a49-04.10a",		0x2000, 0x1fed96e6, 4 | BRF_GRA },           //  6 Background Tiles
	{ "a49-05.10c",		0x2000, 0xe0fce676, 4 | BRF_GRA },           //  7
	{ "a49-06.10d",		0x2000, 0xf895672e, 4 | BRF_GRA },           //  8

	{ "a49-10.11n",		0x1000, 0xa7a361ba, 5 | BRF_GRA },           //  9 Foreground Tiles
	{ "a49-11.11q",		0x1000, 0x801902e3, 5 | BRF_GRA },           // 10
	{ "a49-12.11r",		0x1000, 0xcef75565, 5 | BRF_GRA },           // 11

	{ "a49-07.12h",		0x4000, 0x8c749828, 6 | BRF_GRA },           // 12 Sprites
	{ "a49-08.12j",		0x4000, 0x03c10aed, 6 | BRF_GRA },           // 13
	{ "a49-09.12k",		0x4000, 0xf61316d2, 6 | BRF_GRA },           // 14

	{ "a49-16.2j",		0x0100, 0xa7b077d4, 7 | BRF_GRA },           // 15 Color Proms
	{ "a49-17.2l",		0x0100, 0x1c04c087, 7 | BRF_GRA },           // 16
	{ "a49-18.2m",		0x0100, 0xf5ce3c45, 7 | BRF_GRA },           // 17
};

STD_ROM_PICK(ladymstr)
STD_ROM_FN(ladymstr)

struct BurnDriver BurnDrvLadymstr = {
	"ladymstr", NULL, NULL, NULL, "1985",
	"Lady Master of Kung Fu (rev 1)\0", NULL, "Kaneko / Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, ladymstrRomInfo, ladymstrRomName, NULL, NULL, NULL, NULL, TsamuraiInputInfo, LadymstrDIPInfo,
	tsamuraiInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Lady Master of Kung Fu
// there's a protection device labeled 6 at location l3 on the main board

static struct BurnRomInfo ladymstraRomDesc[] = {
	{ "a49-01.3r",		0x4000, 0x8729e50e, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code // believed to be newer because of the -01 suffix
	{ "a49-02.3t",		0x4000, 0xb0a9020b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a49-03.3v",		0x4000, 0x641c94ed, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "a49-14.4e",		0x2000, 0xd83a3c02, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code
	{ "a49-15.4c",		0x2000, 0xd24ee5fd, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "a49-13.4j",		0x2000, 0x7942bd7c, 3 | BRF_PRG | BRF_ESS }, //  5 Z80 #2 Code

	{ "a49-04.10a",		0x2000, 0x1fed96e6, 4 | BRF_GRA },           //  6 Background Tiles
	{ "a49-05.10c",		0x2000, 0xe0fce676, 4 | BRF_GRA },           //  7
	{ "a49-06.10d",		0x2000, 0xf895672e, 4 | BRF_GRA },           //  8

	{ "a49-10.11n",		0x1000, 0xa7a361ba, 5 | BRF_GRA },           //  9 Foreground Tiles
	{ "a49-11.11q",		0x1000, 0x801902e3, 5 | BRF_GRA },           // 10
	{ "a49-12.11r",		0x1000, 0xcef75565, 5 | BRF_GRA },           // 11

	{ "a49-07.12h",		0x4000, 0x8c749828, 6 | BRF_GRA },           // 12 Sprites
	{ "a49-08.12j",		0x4000, 0x03c10aed, 6 | BRF_GRA },           // 13
	{ "a49-09.12k",		0x4000, 0xf61316d2, 6 | BRF_GRA },           // 14

	{ "a49-16.2j",		0x0100, 0xa7b077d4, 7 | BRF_GRA },           // 15 Color Proms
	{ "a49-17.2l",		0x0100, 0x1c04c087, 7 | BRF_GRA },           // 16
	{ "a49-18.2m",		0x0100, 0xf5ce3c45, 7 | BRF_GRA },           // 17
};

STD_ROM_PICK(ladymstra)
STD_ROM_FN(ladymstra)

struct BurnDriver BurnDrvLadymstra = {
	"ladymstra", "ladymstr", NULL, NULL, "1985",
	"Lady Master of Kung Fu\0", NULL, "Kaneko / Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, ladymstraRomInfo, ladymstraRomName, NULL, NULL, NULL, NULL, TsamuraiInputInfo, LadymstrDIPInfo,
	tsamuraiInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Nunchackun

static struct BurnRomInfo nunchakuRomDesc[] = {
	{ "nunchack.p1",	0x4000, 0x4385aca6, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "nunchack.p2",	0x4000, 0xf9beb72c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "nunchack.p3",	0x4000, 0xcde5d674, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "nunchack.m3",	0x2000, 0x9036c945, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code
	{ "nunchack.m4",	0x2000, 0xe7206724, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "nunchack.m1",	0x2000, 0xb53d73f6, 3 | BRF_PRG | BRF_ESS }, //  5 Z80 #2 Code
	{ "nunchack.m2",	0x2000, 0xf37d7c49, 3 | BRF_PRG | BRF_ESS }, //  6

	{ "nunchack.b1",	0x2000, 0x48c88fea, 4 | BRF_GRA },           //  7 Background Tiles
	{ "nunchack.b2",	0x2000, 0xeec818e4, 4 | BRF_GRA },           //  8
	{ "nunchack.b3",	0x2000, 0x5f16473f, 4 | BRF_GRA },           //  9

	{ "nunchack.v1",	0x1000, 0x358a3714, 5 | BRF_GRA },           // 10 Foreground Tiles
	{ "nunchack.v2",	0x1000, 0x54c18d8e, 5 | BRF_GRA },           // 11
	{ "nunchack.v3",	0x1000, 0xf7ac203a, 5 | BRF_GRA },           // 12

	{ "nunchack.c1",	0x4000, 0x797cbc8a, 6 | BRF_GRA },           // 13 Sprites
	{ "nunchack.c2",	0x4000, 0x701a0cc3, 6 | BRF_GRA },           // 14
	{ "nunchack.c3",	0x4000, 0xffb841fc, 6 | BRF_GRA },           // 15

	{ "nunchack.016",	0x0100, 0xa7b077d4, 7 | BRF_GRA },           // 16 Color Proms
	{ "nunchack.017",	0x0100, 0x1c04c087, 7 | BRF_GRA },           // 17
	{ "nunchack.018",	0x0100, 0xf5ce3c45, 7 | BRF_GRA },           // 18
};

STD_ROM_PICK(nunchaku)
STD_ROM_FN(nunchaku)

static INT32 nunchakuInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvNunchaku = {
	"nunchaku", "ladymstr", NULL, NULL, "1985",
	"Nunchackun\0", NULL, "Kaneko / Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, nunchakuRomInfo, nunchakuRomName, NULL, NULL, NULL, NULL, TsamuraiInputInfo, NunchakuDIPInfo,
	nunchakuInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Go Go Mr. Yamaguchi / Yuke Yuke Yamaguchi-kun

static struct BurnRomInfo yamagchiRomDesc[] = {
	{ "a38-01.3s",		0x4000, 0x1a6c8498, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "a38-02.3t",		0x4000, 0xfa66b396, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a38-03.3v",		0x4000, 0x6a4239cf, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "a38-14.4e",		0x2000, 0x5a758992, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "a38-13.4j",		0x2000, 0xa26445bb, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #2 Code

	{ "a38-04.10a",		0x2000, 0x6bc69d4d, 4 | BRF_GRA },           //  5 Background Tiles
	{ "a38-05.10b",		0x2000, 0x047fb315, 4 | BRF_GRA },           //  6
	{ "a38-06.10d",		0x2000, 0xa636afb2, 4 | BRF_GRA },           //  7

	{ "a38-10.11n",		0x1000, 0x51ab4671, 5 | BRF_GRA },           //  8 Foreground Tiles
	{ "a38-11.11p",		0x1000, 0x27890169, 5 | BRF_GRA },           //  9
	{ "a38-12.11r",		0x1000, 0xc98d5cf2, 5 | BRF_GRA },           // 10

	{ "a38-07.12h",		0x4000, 0xa3a521b6, 6 | BRF_GRA },           // 11 Sprites
	{ "a38-08.12j",		0x4000, 0x553afc66, 6 | BRF_GRA },           // 12
	{ "a38-09.12l",		0x4000, 0x574156ae, 6 | BRF_GRA },           // 13

	{ "mb7114e.2k",		0x0100, 0xe7648110, 7 | BRF_GRA },           // 14 Color Proms
	{ "mb7114e.2l",		0x0100, 0x7b874ee6, 7 | BRF_GRA },           // 15
	{ "mb7114e.2m",		0x0100, 0x938d0fce, 7 | BRF_GRA },           // 16
};

STD_ROM_PICK(yamagchi)
STD_ROM_FN(yamagchi)

static INT32 yamagchiInit()
{
	return DrvInit(2);
}

struct BurnDriver BurnDrvYamagchi = {
	"yamagchi", NULL, NULL, NULL, "1985",
	"Go Go Mr. Yamaguchi / Yuke Yuke Yamaguchi-kun\0", NULL, "Kaneko / Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, yamagchiRomInfo, yamagchiRomName, NULL, NULL, NULL, NULL, YamagchiInputInfo, YamagchiDIPInfo,
	yamagchiInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Mission 660 (US)

static struct BurnRomInfo m660RomDesc[] = {
	{ "660l.bin",	0x4000, 0x57c0d1cc, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "660m.bin",	0x4000, 0x628c6686, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "660n.bin",	0x4000, 0x1b418a97, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "14.4n",		0x4000, 0x5734db5a, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "13.4j",		0x4000, 0xfba51cf7, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #2 Code

	{ "660x.bin",	0x8000, 0xb82f0cfa, 4 | BRF_PRG | BRF_ESS }, //  5 Z80 #3 Code

	{ "4.7k",		0x4000, 0xe24e431a, 5 | BRF_GRA },           //  6 Background Tiles
	{ "5.6k",		0x4000, 0xb2c93d46, 5 | BRF_GRA },           //  7
	{ "6.5k",		0x4000, 0x763c5983, 5 | BRF_GRA },           //  8

	{ "660u.bin",	0x2000, 0x030af716, 6 | BRF_GRA },           //  9 Foreground Tiles
	{ "660v.bin",	0x2000, 0x51a6e160, 6 | BRF_GRA },           // 10
	{ "660w.bin",	0x2000, 0x8a45b469, 6 | BRF_GRA },           // 11

	{ "7.15e",		0x4000, 0x990c0cee, 7 | BRF_GRA },           // 12 Sprites
	{ "8.15d",		0x4000, 0xd9aa7834, 7 | BRF_GRA },           // 13
	{ "9.15b",		0x4000, 0x27b26905, 7 | BRF_GRA },           // 14

	{ "4r.bpr",		0x0100, 0xcd16d0f1, 8 | BRF_GRA },           // 15 Color Proms
	{ "4p.bpr",		0x0100, 0x22e8b22c, 8 | BRF_GRA },           // 16
	{ "5r.bpr",		0x0100, 0xb7d6fdb5, 8 | BRF_GRA },           // 17
};

STD_ROM_PICK(m660)
STD_ROM_FN(m660)

static INT32 m660Init()
{
	return m660CommonInit(0);
}

struct BurnDriver BurnDrvM660 = {
	"m660", NULL, NULL, NULL, "1986",
	"Mission 660 (US)\0", NULL, "Wood Place Co. Ltd. (Taito America Corporation license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, m660RomInfo, m660RomName, NULL, NULL, NULL, NULL, TsamuraiInputInfo, M660DIPInfo,
	m660Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Mission 660 (Japan)

static struct BurnRomInfo m660jRomDesc[] = {
	{ "1.3c",		0x4000, 0x4c8f96aa, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "2.3d",		0x4000, 0xe6661504, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.3f",		0x4000, 0x3a389ccd, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "14.4n",		0x4000, 0x5734db5a, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "13.4j",		0x4000, 0xfba51cf7, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #2 Code

	{ "d.4e",		0x4000, 0x93f3d852, 4 | BRF_PRG | BRF_ESS }, //  5 Z80 #3 Code
	{ "e.4d",		0x4000, 0x12f5c077, 4 | BRF_PRG | BRF_ESS }, //  6

	{ "4.7k",		0x4000, 0xe24e431a, 5 | BRF_GRA },           //  7 Background Tiles
	{ "5.6k",		0x4000, 0xb2c93d46, 5 | BRF_GRA },           //  8
	{ "6.5k",		0x4000, 0x763c5983, 5 | BRF_GRA },           //  9

	{ "a.16j",		0x2000, 0x06f44c8c, 6 | BRF_GRA },           // 10 Foreground Tiles
	{ "b.16k",		0x2000, 0x94b8b69f, 6 | BRF_GRA },           // 11
	{ "c.16m",		0x2000, 0xd6768c68, 6 | BRF_GRA },           // 12

	{ "7.15e",		0x4000, 0x990c0cee, 7 | BRF_GRA },           // 13 Sprites
	{ "8.15d",		0x4000, 0xd9aa7834, 7 | BRF_GRA },           // 14
	{ "9.15b",		0x4000, 0x27b26905, 7 | BRF_GRA },           // 15

	{ "4r.bpr",		0x0100, 0xcd16d0f1, 8 | BRF_GRA },           // 16 Color Proms
	{ "4p.bpr",		0x0100, 0x22e8b22c, 8 | BRF_GRA },           // 17
	{ "5r.bpr",		0x0100, 0xb7d6fdb5, 8 | BRF_GRA },           // 18
};

STD_ROM_PICK(m660j)
STD_ROM_FN(m660j)

static INT32 m660jInit()
{
	return m660CommonInit(1);
}

struct BurnDriver BurnDrvM660j = {
	"m660j", "m660", NULL, NULL, "1986",
	"Mission 660 (Japan)\0", NULL, "Wood Place Co. Ltd. (Taito Corporation license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, m660jRomInfo, m660jRomName, NULL, NULL, NULL, NULL, TsamuraiInputInfo, M660DIPInfo,
	m660jInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Mission 660 (bootleg)

static struct BurnRomInfo m660bRomDesc[] = {
	{ "m660-1.bin",	 0x4000, 0x18f6c4be, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "2.3d",		 0x4000, 0xe6661504, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.3f",		 0x4000, 0x3a389ccd, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "14.4n",		 0x4000, 0x5734db5a, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "13.4j",		 0x4000, 0xfba51cf7, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #2 Code

	{ "660x.bin",	 0x8000, 0xb82f0cfa, 4 | BRF_PRG | BRF_ESS }, //  5 Z80 #3 Code

	{ "4.7k",		 0x4000, 0xe24e431a, 5 | BRF_GRA },           //  6 Background Tiles
	{ "5.6k",		 0x4000, 0xb2c93d46, 5 | BRF_GRA },           //  7
	{ "6.5k",		 0x4000, 0x763c5983, 5 | BRF_GRA },           //  8

	{ "m660-10.bin", 0x2000, 0xb11405a6, 6 | BRF_GRA },           //  9 Foreground Tiles
	{ "b.16k",		 0x2000, 0x94b8b69f, 6 | BRF_GRA },           // 10
	{ "c.16m",		 0x2000, 0xd6768c68, 6 | BRF_GRA },           // 11

	{ "7.15e",		 0x4000, 0x990c0cee, 7 | BRF_GRA },           // 12 Sprites
	{ "8.15d",		 0x4000, 0xd9aa7834, 7 | BRF_GRA },           // 13
	{ "9.15b",		 0x4000, 0x27b26905, 7 | BRF_GRA },           // 14

	{ "4r.bpr",		 0x0100, 0xcd16d0f1, 8 | BRF_GRA },           // 15 Color Proms
	{ "4p.bpr",		 0x0100, 0x22e8b22c, 8 | BRF_GRA },           // 16
	{ "5r.bpr",		 0x0100, 0xb7d6fdb5, 8 | BRF_GRA },           // 17
};

STD_ROM_PICK(m660b)
STD_ROM_FN(m660b)

struct BurnDriver BurnDrvM660b = {
	"m660b", "m660", NULL, NULL, "1986",
	"Mission 660 (bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, m660bRomInfo, m660bRomName, NULL, NULL, NULL, NULL, TsamuraiInputInfo, M660DIPInfo,
	m660Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// The Alphax Z (Japan)

static struct BurnRomInfo alphaxzRomDesc[] = {
	{ "az-01.bin",		0x4000, 0x5336f842, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "az-02.bin",		0x4000, 0xa0779b6b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "az-03.bin",		0x4000, 0x2797bc7b, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "14.4n",			0x4000, 0x5734db5a, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "13.4j",			0x4000, 0xfba51cf7, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #2 Code

	{ "660x.bin",		0x8000, 0xb82f0cfa, 4 | BRF_PRG | BRF_ESS }, //  5 Z80 #3 Code

	{ "az-04.bin",		0x4000, 0x23da4e3d, 5 | BRF_GRA },           //  6 Background Tiles
	{ "az-05.bin",		0x4000, 0x8746ff69, 5 | BRF_GRA },           //  7
	{ "az-06.bin",		0x4000, 0x6e494964, 5 | BRF_GRA },           //  8

	{ "az-10.bin",		0x2000, 0x10b499bb, 6 | BRF_GRA },           //  9 Foreground Tiles
	{ "az-11.bin",		0x2000, 0xd91993f6, 6 | BRF_GRA },           // 10
	{ "az-12.bin",		0x2000, 0x8ea48ef3, 6 | BRF_GRA },           // 11

	{ "az-07.bin",		0x4000, 0x5f9cc65e, 7 | BRF_GRA },           // 12 Sprites
	{ "az-08.bin",		0x4000, 0x23e3a6ba, 7 | BRF_GRA },           // 13
	{ "az-09.bin",		0x4000, 0x7096fa71, 7 | BRF_GRA },           // 14

	{ "4r.bpr",			0x0100, 0xcd16d0f1, 8 | BRF_GRA },           // 15 Color Proms
	{ "4p.bpr",			0x0100, 0x22e8b22c, 8 | BRF_GRA },           // 16
	{ "5r.bpr",			0x0100, 0xb7d6fdb5, 8 | BRF_GRA },           // 17
};

STD_ROM_PICK(alphaxz)
STD_ROM_FN(alphaxz)

struct BurnDriver BurnDrvAlphaxz = {
	"alphaxz", NULL, NULL, NULL, "1986",
	"The Alphax Z (Japan)\0", NULL, "Ed Co., Ltd. (Wood Place Co., Ltd. license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, alphaxzRomInfo, alphaxzRomName, NULL, NULL, NULL, NULL, TsamuraiInputInfo, M660DIPInfo,
	m660Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Mission 660 (Japan)

static struct BurnRomInfo the26thzRomDesc[] = {
	{ "a72_01.3r",		0x4000, 0x2be77520, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "a72_02.3t",		0x4000, 0xef2646f2, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a72_03.3v",		0x4000, 0xd83b7758, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "a72_16.4n",		0x4000, 0x5734db5a, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 #1 Code

	{ "a72_15.4j",		0x4000, 0xfba51cf7, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #2 Code

	{ "a72_14.4e",		0x4000, 0xd078373e, 4 | BRF_PRG | BRF_ESS }, //  5 Z80 #3 Code
	{ "a72_13.4d",		0x4000, 0x11980449, 4 | BRF_PRG | BRF_ESS }, //  6

	{ "a72_04.10a",		0x4000, 0x23da4e3d, 5 | BRF_GRA },           //  7 Background Tiles
	{ "a72_05.10b",		0x4000, 0x8746ff69, 5 | BRF_GRA },           //  8
	{ "a72_06.10d",		0x4000, 0x6e494964, 5 | BRF_GRA },           //  9

	{ "a72_10.11n",		0x1000, 0xa23e4829, 6 | BRF_GRA },           // 10 Foreground Tiles
	{ "a72_11.11q",		0x1000, 0x9717229f, 6 | BRF_GRA },           // 11
	{ "a72_12.11r",		0x1000, 0x7a602979, 6 | BRF_GRA },           // 12

	{ "a72_07.12h",		0x4000, 0x5f9cc65e, 7 | BRF_GRA },           // 13 Sprites
	{ "a72_08.12j",		0x4000, 0x23e3a6ba, 7 | BRF_GRA },           // 14
	{ "a72_09.12k",		0x4000, 0x7096fa71, 7 | BRF_GRA },           // 15

	{ "a72_17.2k",		0x100, 0xcd16d0f1, 8 | BRF_GRA },           // 16 Color Proms
	{ "a72_18.2l",		0x100, 0x22e8b22c, 8 | BRF_GRA },           // 17
	{ "a72_19.2m",		0x100, 0xb7d6fdb5, 8 | BRF_GRA },           // 18
};

STD_ROM_PICK(the26thz)
STD_ROM_FN(the26thz)

static INT32 the26thzInit()
{
	the26thz = 1;
	return m660CommonInit(1);
}

struct BurnDriver BurnDrvThe26thz = {
	"the26thz", "alphaxz", NULL, NULL, "1986",
	"The 26th Z (Japan, location test)\0", NULL, "Ed Co., Ltd. (Taito license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_PROTOTYPE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, the26thzRomInfo, the26thzRomName, NULL, NULL, NULL, NULL, TsamuraiInputInfo, M660DIPInfo,
	the26thzInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// VS Gong Fight

static struct BurnRomInfo vsgongfRomDesc[] = {
	{ "1.5a",		0x2000, 0x2c056dee, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "2",			0x2000, 0x1a634daf, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.5d",		0x2000, 0x5ac16861, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.5f",		0x2000, 0x1d1baf7b, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "6.5n",		0x2000, 0x785b9000, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code
	{ "5.5l",		0x2000, 0x76dbfde9, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "7.6f",		0x1000, 0x6ec68692, 3 | BRF_GRA },           //  6 Background Tiles
	{ "8.7f",		0x1000, 0xafba16c8, 3 | BRF_GRA },           //  7
	{ "9.8f",		0x1000, 0x536bf710, 3 | BRF_GRA },           //  8

	{ "13.15j",		0x2000, 0xa2451a31, 4 | BRF_GRA },           //  9 Sprites
	{ "14.15h",		0x2000, 0xb387403e, 4 | BRF_GRA },           // 10
	{ "15.15f",		0x2000, 0x0e649334, 4 | BRF_GRA },           // 11

	{ "clr.6s",		0x0100, 0x578bfbea, 5 | BRF_GRA },           // 12 Color Proms
	{ "clr.6r",		0x0100, 0x3ec00739, 5 | BRF_GRA },           // 13
	{ "clr.6p",		0x0100, 0x0e4fd17a, 5 | BRF_GRA },           // 14
};

STD_ROM_PICK(vsgongf)
STD_ROM_FN(vsgongf)

static INT32 VsgongfInit()
{
	vsgongf_protval = 0xaa80;

	return VsgongfCommonInit(0);
}

struct BurnDriver BurnDrvVsgongf = {
	"vsgongf", NULL, NULL, NULL, "1984",
	"VS Gong Fight\0", NULL, "Kaneko", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, vsgongfRomInfo, vsgongfRomName, NULL, NULL, NULL, NULL, TsamuraiInputInfo, VsgongfDIPInfo,
	VsgongfInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Ring Fighter (rev 1)

static struct BurnRomInfo ringfgtRomDesc[] = {
	{ "rft_04-1.5a",	0x2000, 0x11030866, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "rft_03-1.5c",	0x2000, 0x357a2085, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "rft_01.5n",		0x2000, 0x785b9000, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code
	{ "rft_02.5l",		0x2000, 0x76dbfde9, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "rft_05.6f",		0x1000, 0xa7b732fd, 3 | BRF_GRA },           //  4 Background Tiles
	{ "rft_06.7f",		0x1000, 0xff2721f7, 3 | BRF_GRA },           //  5
	{ "rft_07.8f",		0x1000, 0xec1d7ba4, 3 | BRF_GRA },           //  6

	{ "rft_08.15j",		0x2000, 0x80d67d28, 4 | BRF_GRA },           //  7 Sprites
	{ "rft_09.15h",		0x2000, 0xea8f0656, 4 | BRF_GRA },           //  8
	{ "rft_10.15f",		0x2000, 0x833ca89f, 4 | BRF_GRA },           //  9

	{ "rft-11.6s",		0x0100, 0x578bfbea, 5 | BRF_GRA },           // 10 Color Proms
	{ "rft-12.6r",		0x0100, 0x3ec00739, 5 | BRF_GRA },           // 11
	{ "rft-13.6p",		0x0100, 0x0e4fd17a, 5 | BRF_GRA },           // 12
};

STD_ROM_PICK(ringfgt)
STD_ROM_FN(ringfgt)

static INT32 RingfgtInit()
{
	vsgongf_protval = 0x6380;

	return VsgongfCommonInit(1);
}

struct BurnDriver BurnDrvRingfgt = {
	"ringfgt", "vsgongf", NULL, NULL, "1984",
	"Ring Fighter (rev 1)\0", NULL, "Kaneko (Taito license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, ringfgtRomInfo, ringfgtRomName, NULL, NULL, NULL, NULL, TsamuraiInputInfo, VsgongfDIPInfo,
	RingfgtInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Ring Fighter

static struct BurnRomInfo ringfgtaRomDesc[] = {
	{ "rft_04.5a",		0x2000, 0x6b9b3f3d, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "rft_03.5c",		0x2000, 0x1821974b, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "rft_01.5n",		0x2000, 0x785b9000, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code
	{ "rft_02.5l",		0x2000, 0x76dbfde9, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "rft_05.6f",		0x1000, 0xa7b732fd, 3 | BRF_GRA },           //  4 Background Tiles
	{ "rft_06.7f",		0x1000, 0xff2721f7, 3 | BRF_GRA },           //  5
	{ "rft_07.8f",		0x1000, 0xec1d7ba4, 3 | BRF_GRA },           //  6

	{ "rft_08.15j",		0x2000, 0x80d67d28, 4 | BRF_GRA },           //  7 Sprites
	{ "rft_09.15h",		0x2000, 0xea8f0656, 4 | BRF_GRA },           //  8
	{ "rft_10.15f",		0x2000, 0x833ca89f, 4 | BRF_GRA },           //  9

	{ "rft-11.6s",		0x0100, 0x578bfbea, 5 | BRF_GRA },           // 10 Color Proms
	{ "rft-12.6r",		0x0100, 0x3ec00739, 5 | BRF_GRA },           // 11
	{ "rft-13.6p",		0x0100, 0x0e4fd17a, 5 | BRF_GRA },           // 12
};

STD_ROM_PICK(ringfgta)
STD_ROM_FN(ringfgta)

static INT32 RingfgtaInit()
{
	vsgongf_protval = 0x6ac0;

	return VsgongfCommonInit(1);
}

struct BurnDriver BurnDrvRingfgta = {
	"ringfgta", "vsgongf", NULL, NULL, "1984",
	"Ring Fighter\0", NULL, "Kaneko (Taito license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, ringfgtaRomInfo, ringfgtaRomName, NULL, NULL, NULL, NULL, TsamuraiInputInfo, VsgongfDIPInfo,
	RingfgtaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};
