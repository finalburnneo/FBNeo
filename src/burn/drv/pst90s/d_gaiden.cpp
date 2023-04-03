// FB Neo Ninja Gaiden driver module
// Based on MAME driver by Alex Pasadyn, Phil Stroffolino, Nicola Salmoria, and various others

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "msm6295.h"
#include "burn_ym2151.h"
#include "burn_ym2203.h"
#include "bitswap.h"

static INT32 game = 0; // 0 gaiden & wildfang, 2 raiga, 1,3 drgnbowl

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvZ80ROM;

static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;

static UINT8 *DrvOkiROM;

static UINT16 *bitmap[3];
static INT32 sprite_sizey = 0;

static UINT8 *Drv68KRAM;
static UINT8 *DrvVidRAM0;
static UINT8 *DrvVidRAM1;
static UINT8 *DrvVidRAM2;
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprRAM1;
static UINT8 *DrvSprRAM2;
static UINT8 *DrvPalRAM;
static UINT8 *DrvZ80RAM;

static UINT32 *DrvPalette;
static UINT32 *Palette;
static UINT8 DrvRecalc;

static INT32 nExtraCycles;

static INT32 flipscreen;
static UINT8 soundlatch;

static INT32 prot;
static INT32 jumpcode;
static INT32 jumppointer;

static INT32 tx_scroll_x;
static INT32 tx_scroll_y;
static INT32 fg_scroll_x;
static INT32 fg_scroll_y;
static INT32 bg_scroll_x;
static INT32 bg_scroll_y;
static INT32 tx_offset_y;
static INT32 fg_offset_y;
static INT32 bg_offset_y;
static INT32 sproffsety;

static struct BurnInputInfo DrvInputList[] = {
	{"P1 Coin"      , BIT_DIGITAL  , DrvJoy1 + 6,	"p1 coin"  },
	{"P1 Start"     , BIT_DIGITAL  , DrvJoy1 + 0,	"p1 start" },
	{"P1 Left"      , BIT_DIGITAL  , DrvJoy2 + 0,	"p1 left"  },
	{"P1 Right"     , BIT_DIGITAL  , DrvJoy2 + 1,	"p1 right" },
	{"P1 Down"      , BIT_DIGITAL  , DrvJoy2 + 2,	"p1 down"  },
	{"P1 Up"        , BIT_DIGITAL  , DrvJoy2 + 3,	"p1 up"    },
	{"P1 Button 1"  , BIT_DIGITAL  , DrvJoy2 + 5,	"p1 fire 1"},
	{"P1 Button 2"  , BIT_DIGITAL  , DrvJoy2 + 6,	"p1 fire 2"},
	{"P1 Button 3"  , BIT_DIGITAL  , DrvJoy2 + 4,	"p1 fire 3"},

	{"P2 Coin"      , BIT_DIGITAL  , DrvJoy1 + 7,	"p2 coin"  },
	{"P2 Start"     , BIT_DIGITAL  , DrvJoy1 + 1,	"p2 start" },
	{"P2 Left"      , BIT_DIGITAL  , DrvJoy3 + 0,	"p2 left"  },
	{"P2 Right"     , BIT_DIGITAL  , DrvJoy3 + 1,	"p2 right" },
	{"P2 Down"      , BIT_DIGITAL  , DrvJoy3 + 2,	"p2 down"  },
	{"P2 Up"        , BIT_DIGITAL  , DrvJoy3 + 3,	"p2 up"    },
	{"P2 Button 1"  , BIT_DIGITAL  , DrvJoy3 + 5,	"p2 fire 1"},
	{"P2 Button 2"  , BIT_DIGITAL  , DrvJoy3 + 6,	"p2 fire 2"},
	{"P2 Button 3"  , BIT_DIGITAL  , DrvJoy3 + 4,	"p2 fire 3"},

	{"Reset"        , BIT_DIGITAL  , &DrvReset  ,	"reset"    },
	{"Dip 1"        , BIT_DIPSWITCH, DrvDips + 0,	"dip"	   },
	{"Dip 2"        , BIT_DIPSWITCH, DrvDips + 1,	"dip"	   },
};

STDINPUTINFO(Drv)

static struct BurnInputInfo RaigaInputList[] = {
	{"P1 Coin"      , BIT_DIGITAL  , DrvJoy1 + 6,	"p1 coin"  },
	{"P1 Start"     , BIT_DIGITAL  , DrvJoy1 + 0,	"p1 start" },
	{"P1 Left"      , BIT_DIGITAL  , DrvJoy2 + 0,	"p1 left"  },
	{"P1 Right"     , BIT_DIGITAL  , DrvJoy2 + 1,	"p1 right" },
	{"P1 Down"      , BIT_DIGITAL  , DrvJoy2 + 2,	"p1 down"  },
	{"P1 Up"        , BIT_DIGITAL  , DrvJoy2 + 3,	"p1 up"    },
	{"P1 Button 1"  , BIT_DIGITAL  , DrvJoy2 + 4,	"p1 fire 1"},
	{"P1 Button 2"  , BIT_DIGITAL  , DrvJoy2 + 5,	"p1 fire 2"},

	{"P2 Coin"      , BIT_DIGITAL  , DrvJoy1 + 7,	"p2 coin"  },
	{"P2 Start"     , BIT_DIGITAL  , DrvJoy1 + 1,	"p2 start" },
	{"P2 Left"      , BIT_DIGITAL  , DrvJoy3 + 0,	"p2 left"  },
	{"P2 Right"     , BIT_DIGITAL  , DrvJoy3 + 1,	"p2 right" },
	{"P2 Down"      , BIT_DIGITAL  , DrvJoy3 + 2,	"p2 down"  },
	{"P2 Up"        , BIT_DIGITAL  , DrvJoy3 + 3,	"p2 up"    },
	{"P2 Button 1"  , BIT_DIGITAL  , DrvJoy3 + 4,	"p2 fire 1"},
	{"P2 Button 2"  , BIT_DIGITAL  , DrvJoy3 + 5,	"p2 fire 2"},

	{"Reset"        , BIT_DIGITAL  , &DrvReset  ,	"reset"    },
	{"Dip 1"        , BIT_DIPSWITCH, DrvDips + 0,	"dip"	   },
	{"Dip 2"        , BIT_DIPSWITCH, DrvDips + 1,	"dip"	   },
};

STDINPUTINFO(Raiga)

static struct BurnDIPInfo GaidenDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL	},
	{0x14, 0xff, 0xff, 0xff, NULL },

	{0   , 0xfe, 0   , 2   , "Demo Sounds" },
	{0x13, 0x01, 0x01, 0x00, "Off" },
	{0x13, 0x01, 0x01, 0x01, "On"  },

	{0   , 0xfe, 0   , 2   , "Flip Screen" },
	{0x13, 0x01, 0x02, 0x02, "Off" },
	{0x13, 0x01, 0x02, 0x00, "On"  },

	{0   , 0xfe, 0   , 8   , "Coin A" },
	{0x13, 0x01, 0xe0, 0x00, "5 Coins 1 Credit"  },
	{0x13, 0x01, 0xe0, 0x80, "4 Coins 3 Credits" },
	{0x13, 0x01, 0xe0, 0x40, "3 Coins 1 Credit"  },
	{0x13, 0x01, 0xe0, 0x20, "2 Coins 1 Credit"  },
	{0x13, 0x01, 0xe0, 0xe0, "1 Coin  1 Credit"  },
	{0x13, 0x01, 0xe0, 0x60, "1 Coin  2 Credits" },
	{0x13, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits" },
	{0x13, 0x01, 0xe0, 0xc0, "1 Coin  4 Credits" },

	{0   , 0xfe, 0   , 8   , "Coin B" },
	{0x13, 0x01, 0x1c, 0x00, "5 Coins 1 Credit"  },
	{0x13, 0x01, 0x1c, 0x10, "4 Coins 3 Credits" },
	{0x13, 0x01, 0x1c, 0x08, "3 Coins 1 Credit"  },
	{0x13, 0x01, 0x1c, 0x04, "2 Coins 1 Credit"  },
	{0x13, 0x01, 0x1c, 0x1c, "1 Coin  1 Credit"  },
	{0x13, 0x01, 0x1c, 0x0c, "1 Coin  2 Credits" },
	{0x13, 0x01, 0x1c, 0x14, "1 Coin  3 Credits" },
	{0x13, 0x01, 0x1c, 0x18, "1 Coin  4 Credits" },

	{0   , 0xfe, 0   , 4   , "Energy" },
	{0x14, 0x01, 0x30, 0x00, "2" },
	{0x14, 0x01, 0x30, 0x30, "3" },
	{0x14, 0x01, 0x30, 0x10, "4" },
	{0x14, 0x01, 0x30, 0x20, "5" },

	{0   , 0xfe, 0   , 4   , "Lives" },
	{0x14, 0x01, 0xc0, 0x00, "1" },
	{0x14, 0x01, 0xc0, 0xc0, "2" },
	{0x14, 0x01, 0xc0, 0x40, "3" },
	{0x14, 0x01, 0xc0, 0x80, "4" },
};

STDDIPINFO(Gaiden)

static struct BurnDIPInfo WildfangDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL },
	{0x14, 0xff, 0xff, 0xff, NULL },

	{0   , 0xfe, 0   , 2   , "Demo Sounds" },
	{0x13, 0x01, 0x01, 0x00, "Off" },
	{0x13, 0x01, 0x01, 0x01, "On"  },

	{0   , 0xfe, 0   , 2   , "Flip Screen" },
	{0x13, 0x01, 0x02, 0x02, "Off" },
	{0x13, 0x01, 0x02, 0x00, "On"  },

	{0   , 0xfe, 0   , 8   , "Coin A" },
	{0x13, 0x01, 0xe0, 0x00, "5 Coins 1 Credit"  },
	{0x13, 0x01, 0xe0, 0x80, "4 Coins 3 Credits" },
	{0x13, 0x01, 0xe0, 0x40, "3 Coins 1 Credit"  },
	{0x13, 0x01, 0xe0, 0x20, "2 Coins 1 Credit"  },
	{0x13, 0x01, 0xe0, 0xe0, "1 Coin  1 Credit"  },
	{0x13, 0x01, 0xe0, 0x60, "1 Coin  2 Credits" },
	{0x13, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits" },
	{0x13, 0x01, 0xe0, 0xc0, "1 Coin  4 Credits" },

	{0   , 0xfe, 0   , 8   , "Coin B" },
	{0x13, 0x01, 0x1c, 0x00, "5 Coins 1 Credit"  },
	{0x13, 0x01, 0x1c, 0x10, "4 Coins 3 Credits" },
	{0x13, 0x01, 0x1c, 0x08, "3 Coins 1 Credit"  },
	{0x13, 0x01, 0x1c, 0x04, "2 Coins 1 Credit"  },
	{0x13, 0x01, 0x1c, 0x1c, "1 Coin  1 Credit"  },
	{0x13, 0x01, 0x1c, 0x0c, "1 Coin  2 Credits" },
	{0x13, 0x01, 0x1c, 0x14, "1 Coin  3 Credits" },
	{0x13, 0x01, 0x1c, 0x18, "1 Coin  4 Credits" },

	{0   , 0xfe, 0   , 2   , "Title" },
	{0x14, 0x01, 0x01, 0x01, "Wild Fang"  	},
	{0x14, 0x01, 0x01, 0x00, "Tecmo Knight" },

	{0   , 0xfe, 0   , 4   , "Difficulty" }, // Wild Fang
	{0x14, 0x02, 0x0c, 0x0c, "Easy"    },
	{0x14, 0x00, 0x01, 0x01, NULL },
	{0x14, 0x02, 0x0c, 0x04, "Normal"  },
	{0x14, 0x00, 0x01, 0x01, NULL },
	{0x14, 0x02, 0x0c, 0x08, "Hard"    },
	{0x14, 0x00, 0x01, 0x01, NULL },
	{0x14, 0x02, 0x0c, 0x00, "Hardest" },
	{0x14, 0x00, 0x01, 0x01, NULL },

	{0   , 0xfe, 0   , 4   , "Difficulty"	}, // Tecmo Knight
	{0x14, 0x02, 0x30, 0x30, "Easy"    },
	{0x14, 0x00, 0x01, 0x00, NULL },
	{0x14, 0x02, 0x30, 0x10, "Normal"  },
	{0x14, 0x00, 0x01, 0x00, NULL },
	{0x14, 0x02, 0x30, 0x20, "Hard"    },
	{0x14, 0x00, 0x01, 0x00, NULL },
	{0x14, 0x02, 0x30, 0x00, "Hardest" },
	{0x14, 0x00, 0x01, 0x00, NULL },

	{0   , 0xfe, 0   , 3   , "Lives" },
	{0x14, 0x01, 0xc0, 0x80, "1" },
	{0x14, 0x01, 0xc0, 0xc0, "2" },
	{0x14, 0x01, 0xc0, 0x40, "3" },
	//	{0x14, 0x01, 0xc0, 0x00, "2" },
};

STDDIPINFO(Wildfang)

static struct BurnDIPInfo TknightDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL },
	{0x14, 0xff, 0xff, 0xff, NULL },

	{0   , 0xfe, 0   , 2   , "Demo Sounds" },
	{0x13, 0x01, 0x01, 0x00, "Off" },
	{0x13, 0x01, 0x01, 0x01, "On"  },

	{0   , 0xfe, 0   , 2   , "Flip Screen" },
	{0x13, 0x01, 0x02, 0x02, "Off" },
	{0x13, 0x01, 0x02, 0x00, "On"  },

	{0   , 0xfe, 0   , 8   , "Coin A" },
	{0x13, 0x01, 0xe0, 0x00, "5 Coins 1 Credit"  },
	{0x13, 0x01, 0xe0, 0x80, "4 Coins 3 Credits" },
	{0x13, 0x01, 0xe0, 0x40, "3 Coins 1 Credit"  },
	{0x13, 0x01, 0xe0, 0x20, "2 Coins 1 Credit"  },
	{0x13, 0x01, 0xe0, 0xe0, "1 Coin  1 Credit"  },
	{0x13, 0x01, 0xe0, 0x60, "1 Coin  2 Credits" },
	{0x13, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits" },
	{0x13, 0x01, 0xe0, 0xc0, "1 Coin  4 Credits" },

	{0   , 0xfe, 0   , 8   , "Coin B" },
	{0x13, 0x01, 0x1c, 0x00, "5 Coins 1 Credit"  },
	{0x13, 0x01, 0x1c, 0x10, "4 Coins 3 Credits" },
	{0x13, 0x01, 0x1c, 0x08, "3 Coins 1 Credit"  },
	{0x13, 0x01, 0x1c, 0x04, "2 Coins 1 Credit"  },
	{0x13, 0x01, 0x1c, 0x1c, "1 Coin  1 Credit"  },
	{0x13, 0x01, 0x1c, 0x0c, "1 Coin  2 Credits" },
	{0x13, 0x01, 0x1c, 0x14, "1 Coin  3 Credits" },
	{0x13, 0x01, 0x1c, 0x18, "1 Coin  4 Credits" },

	{0   , 0xfe, 0   , 4   , "Difficulty" },
	{0x14, 0x01, 0x30, 0x30, "Easy"    },
	{0x14, 0x01, 0x30, 0x10, "Normal"  },
	{0x14, 0x01, 0x30, 0x20, "Hard"    },
	{0x14, 0x01, 0x30, 0x00, "Hardest" },

	{0   , 0xfe, 0   , 3   , "Lives" },
	{0x14, 0x01, 0xc0, 0x80, "1" },
	{0x14, 0x01, 0xc0, 0xc0, "2" },
	{0x14, 0x01, 0xc0, 0x40, "3" },
	//	{0x14, 0x01, 0xc0, 0x00, "2" },
};

STDDIPINFO(Tknight)

static struct BurnDIPInfo DrgnbowlDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL },
	{0x14, 0xff, 0xff, 0xff, NULL },

	{0   , 0xfe, 0   , 2   , "Demo Sounds" },
	{0x13, 0x01, 0x01, 0x00, "Off" },
	{0x13, 0x01, 0x01, 0x01, "On"  },

	{0   , 0xfe, 0   , 8   , "Coin A" },
	{0x13, 0x01, 0xe0, 0x00, "5 Coins 1 Credit"  },
	{0x13, 0x01, 0xe0, 0x80, "4 Coins 3 Credits" },
	{0x13, 0x01, 0xe0, 0x40, "3 Coins 1 Credit"  },
	{0x13, 0x01, 0xe0, 0x20, "2 Coins 1 Credit"  },
	{0x13, 0x01, 0xe0, 0xe0, "1 Coin  1 Credit"  },
	{0x13, 0x01, 0xe0, 0x60, "1 Coin  2 Credits" },
	{0x13, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits" },
	{0x13, 0x01, 0xe0, 0xc0, "1 Coin  4 Credits" },

	{0   , 0xfe, 0   , 8   , "Coin B" },
	{0x13, 0x01, 0x1c, 0x00, "5 Coins 1 Credit"  },
	{0x13, 0x01, 0x1c, 0x10, "4 Coins 3 Credits" },
	{0x13, 0x01, 0x1c, 0x08, "3 Coins 1 Credit"  },
	{0x13, 0x01, 0x1c, 0x04, "2 Coins 1 Credit"  },
	{0x13, 0x01, 0x1c, 0x1c, "1 Coin  1 Credit"  },
	{0x13, 0x01, 0x1c, 0x0c, "1 Coin  2 Credits" },
	{0x13, 0x01, 0x1c, 0x14, "1 Coin  3 Credits" },
	{0x13, 0x01, 0x1c, 0x18, "1 Coin  4 Credits" },

	{0   , 0xfe, 0   , 4   , "Energy" },
	{0x14, 0x01, 0x30, 0x00, "2" },
	{0x14, 0x01, 0x30, 0x30, "3" },
	{0x14, 0x01, 0x30, 0x10, "4" },
	{0x14, 0x01, 0x30, 0x20, "5" },

	{0   , 0xfe, 0   , 4   , "Lives" },
	{0x14, 0x01, 0xc0, 0x00, "1" },
	{0x14, 0x01, 0xc0, 0xc0, "2" },
	{0x14, 0x01, 0xc0, 0x40, "3" },
	{0x14, 0x01, 0xc0, 0x80, "4" },
};

STDDIPINFO(Drgnbowl)

static struct BurnDIPInfo RaigaDIPList[]=
{
	{0x11, 0xff, 0xff, 0xff, NULL },
	{0x12, 0xff, 0xff, 0x5f, NULL },

	{0   , 0xfe, 0   , 16  , "Coin A" },
	{0x11, 0x01, 0xf0, 0x00, "5 Coins 1 Credit"  },
	{0x11, 0x01, 0xf0, 0x40, "4 Coins 1 Credit"  },
	{0x11, 0x01, 0xf0, 0xa0, "3 Coins 1 Credit"  },
	{0x11, 0x01, 0xf0, 0x10, "2 Coins 1 Credit"  },
	{0x11, 0x01, 0xf0, 0x20, "3 Coins 2 Credits" },
	{0x11, 0x01, 0xf0, 0x80, "4 Coins 3 Credits" },
	{0x11, 0x01, 0xf0, 0xf0, "1 Coin  1 Credit"  },
	{0x11, 0x01, 0xf0, 0xc0, "3 Coins 4 Credits" },
	{0x11, 0x01, 0xf0, 0xe0, "2 Coins 3 Credits" },
	{0x11, 0x01, 0xf0, 0x70, "1 Coin  2 Credits" },
	{0x11, 0x01, 0xf0, 0x60, "2 Coins 5 Credits" },
	{0x11, 0x01, 0xf0, 0xb0, "1 Coin  3 Credits" },
	{0x11, 0x01, 0xf0, 0x30, "1 Coin  4 Credits" },
	{0x11, 0x01, 0xf0, 0xd0, "1 Coin  5 Credits" },
	{0x11, 0x01, 0xf0, 0x50, "1 Coin  6 Credits" },
	{0x11, 0x01, 0xf0, 0x90, "1 Coin  7 Credits" },

	{0   , 0xfe, 0   , 16  , "Coin B" },
	{0x11, 0x01, 0x0f, 0x00, "5 Coins 1 Credit"  },
	{0x11, 0x01, 0x0f, 0x04, "4 Coins 1 Credit"  },
	{0x11, 0x01, 0x0f, 0x0a, "3 Coins 1 Credit"  },
	{0x11, 0x01, 0x0f, 0x01, "2 Coins 1 Credit"  },
	{0x11, 0x01, 0x0f, 0x02, "3 Coins 2 Credits" },
	{0x11, 0x01, 0x0f, 0x08, "4 Coins 3 Credits" },
	{0x11, 0x01, 0x0f, 0x0f, "1 Coin  1 Credit"  },
	{0x11, 0x01, 0x0f, 0x0c, "3 Coins 4 Credits" },
	{0x11, 0x01, 0x0f, 0x0e, "2 Coins 3 Credits" },
	{0x11, 0x01, 0x0f, 0x07, "1 Coin  2 Credits" },
	{0x11, 0x01, 0x0f, 0x06, "2 Coins 5 Credits" },
	{0x11, 0x01, 0x0f, 0x0b, "1 Coin  3 Credits" },
	{0x11, 0x01, 0x0f, 0x03, "1 Coin  4 Credits" },
	{0x11, 0x01, 0x0f, 0x0d, "1 Coin  5 Credits" },
	{0x11, 0x01, 0x0f, 0x05, "1 Coin  6 Credits" },
	{0x11, 0x01, 0x0f, 0x09, "1 Coin  7 Credits" },

	{0   , 0xfe, 0   , 4   , "Bonus Life" },
	{0x12, 0x01, 0x03, 0x03, "50k 200k"  },
	{0x12, 0x01, 0x03, 0x01, "100k 300k" },
	{0x12, 0x01, 0x03, 0x02, "50k only"  },
	{0x12, 0x01, 0x03, 0x00, "None"    	 },

	{0   , 0xfe, 0   , 4   , "Lives" },
	{0x12, 0x01, 0x0c, 0x00, "2" },
	{0x12, 0x01, 0x0c, 0x0c, "3" },
	{0x12, 0x01, 0x0c, 0x04, "4" },
	{0x12, 0x01, 0x0c, 0x08, "5" },

	{0   , 0xfe, 0   , 4   , "Difficulty" },
	{0x12, 0x01, 0x30, 0x30, "Easy"    },
	{0x12, 0x01, 0x30, 0x10, "Normal"  },
	{0x12, 0x01, 0x30, 0x20, "Hard"    },
	{0x12, 0x01, 0x30, 0x00, "Hardest" },

	{0   , 0xfe, 0   , 2   , "Demo Sounds" },
	{0x12, 0x01, 0x80, 0x80, "Off" },
	{0x12, 0x01, 0x80, 0x00, "On"  },
};

STDDIPINFO(Raiga)

static const INT32 raiga_jumppoints_boot[0x100] =
{
	0x6669,	   -1,    -1,    -1,    -1,    -1,    -1,    -1,
		-1,    -1,    -1,    -1,    -1,    -1,0x4a46,    -1,
		-1,0x6704,    -2,    -1,    -1,    -1,    -1,    -1,
		-1,    -2,    -1,    -1,    -1,    -1,    -1,    -1,
		-1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
		-2,    -1,    -1,    -1,    -1,0x4e75,    -1,    -1,
		-1,    -2,    -1,0x4e71,0x60fc,    -1,0x7288,    -1,
		-1,    -1,    -1,    -1,    -1,    -1,    -1,    -1
};

static const INT32 raiga_jumppoints_ingame[0x100] =
{
	0x5457,0x494e,0x5f4b,0x4149,0x5345,0x525f,0x4d49,0x5941,
	0x5241,0x5349,0x4d4f,0x4a49,    -1,    -1,    -1,    -1,
		-1,    -1,    -2,0x594f,    -1,0x4e75,    -1,    -1,
		-1,    -2,    -1,    -1,0x4e75,    -1,0x5349,    -1,
		-1,    -1,    -1,0x4e75,    -1,0x4849,    -1,    -1,
		-2,    -1,    -1,0x524f,    -1,    -1,    -1,    -1,
		-1,    -2,    -1,    -1,    -1,    -1,    -1,    -1,
		-1,    -1,    -1,    -1,    -1,    -1,    -1,    -1
};

static const INT32 wildfang_jumppoints[0x100] =
{
	0x0c0c,0x0cac,0x0d42,0x0da2,0x0eea,0x112e,0x1300,0x13fa,
	0x159a,0x1630,0x109a,0x1700,0x1750,0x1806,0x18d6,0x1a44,
	0x1b52,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
		-1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
		-1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
		-1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
		-1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
		-1,    -1,    -1,    -1,    -1,    -1,    -1,    -1
};

static const INT32 *jumppoints;

static void protection_w(UINT8 data)
{
	switch (data & 0xf0)
	{
		case 0x00:	// init
			prot = 0x00;
			break;

		case 0x10:	// high 4 bits of jump code
			jumpcode = (data & 0x0f) << 4;
			prot = 0x10;
			break;

		case 0x20:	// low 4 bits of jump code
			jumpcode |= data & 0x0f;
			if (jumppoints[jumpcode] == -2) { // only raiga has -2
				jumppoints = raiga_jumppoints_ingame;
				jumppointer = 1;
			}

			if (jumpcode > 0x3f || jumppoints[jumpcode] == -1) {
				jumpcode = 0;
			}
			prot = 0x20;
			break;

		case 0x30:	// bits 12-15 of function address
			prot = 0x40 | ((jumppoints[jumpcode] >> 12) & 0x0f);
			break;

		case 0x40:	// bits 8-11 of function address
			prot = 0x50 | ((jumppoints[jumpcode] >>  8) & 0x0f);
			break;

		case 0x50:	// bits 4-7 of function address
			prot = 0x60 | ((jumppoints[jumpcode] >>  4) & 0x0f);
			break;

		case 0x60:	// bits 0-3 of function address
			prot = 0x70 | ((jumppoints[jumpcode] >>  0) & 0x0f);
			break;
	}
}

static void palette_write(INT32 offset, UINT16 pal)
{
	UINT8 b = (pal >> 8) & 0x0f;
	UINT8 g = (pal >> 4) & 0x0f;
	UINT8 r = (pal >> 0) & 0x0f;

	Palette[offset] = (r<<8) | (g<<4) | b;
	DrvRecalc = 1;
}

static UINT8 __fastcall gaiden_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x07a001:
			return DrvInputs[0];

		case 0x07a002:
			return DrvInputs[2];

		case 0x07a003:
			return DrvInputs[1];

		case 0x07a004:
			return DrvDips[1];

		case 0x07a005:
			return DrvDips[0];

		case 0x07a007: // Raiga, Wild Fang
			return prot;
	}

	return 0;
}

static UINT16 __fastcall gaiden_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x07a000:
			return DrvInputs[0];

		case 0x07a002:
			return (DrvInputs[2] << 8) | DrvInputs[1];

		case 0x07a004:
			return (DrvDips[1] << 8) | DrvDips[0];
	}

	return 0;
}

static void __fastcall gaiden_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xffffe000) == 0x78000) {
		address &= 0x1fff;

		DrvPalRAM[address ^ 1] = data;

		palette_write(address>>1, *((UINT16*)(DrvPalRAM + (address & ~1))));

		return;
	}

	switch (address)
	{
		case 0x7a002:
		case 0x7a003:
			sproffsety = data;
			return;

		case 0x7a00e: // Dragon Bowl
			soundlatch = data;
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
			return;

		case 0x7a802: // Tecmo Knight
		case 0x7a803: // Ninja Gaiden
			soundlatch = data;
			ZetNmi();
			return;

		case 0x7a804: // Raiga, Wild Fang
			protection_w(data);
			return;

		case 0x7e000: // drgnbowl irq acknowledge
			SekSetIRQLine(5, CPU_IRQSTATUS_NONE);
			return;
	}
}

static void __fastcall gaiden_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xffffe000) == 0x78000) {
		address &= 0x1ffe;

		*((UINT16*)(DrvPalRAM + address)) = data;

		palette_write(address>>1, *((UINT16*)(DrvPalRAM + address)));

		return;
	}

	switch (address & 0xfffffffe)
	{
		case 0x7a002:
			sproffsety = data;
			return;

		case 0x7a104:
			tx_scroll_y = data & 0x1ff;
			return;

		case 0x7a108:
			tx_offset_y = data & 0x1ff;
			return;

		case 0x7a10c:
			tx_scroll_x = data & 0x3ff;
			return;

		case 0x7a204:
			fg_scroll_y = data & 0x1ff;
			return;

		case 0x7a208:
			fg_offset_y = data & 0x1ff;
			return;

		case 0x7a20c:
			fg_scroll_x = data & 0x3ff;
			return;

		case 0x7a304:
			bg_scroll_y = data & 0x1ff;
			return;

		case 0x7a308:
			bg_offset_y = data & 0x1ff;
			return;

		case 0x7a30c:
			bg_scroll_x = data & 0x3ff;
			return;

		case 0x7a806: // gaiden, wildfang/tknight, raiga irq acknowledge
			SekSetIRQLine(5, CPU_IRQSTATUS_NONE);
			return;

		case 0x7a808:
			flipscreen = data & 1;
			return;

			// Dragon Bowl
		case 0x7f000:
			bg_scroll_y = data & 0x1ff;
			return;

		case 0x7f002:
			bg_scroll_x = (data + 248) & 0x3ff;
			return;

		case 0x7f004:
			fg_scroll_y = data & 0x1ff;
			return;

		case 0x7f006:
			fg_scroll_x = (data + 252) & 0x3ff;
			return;
	}
}

static void __fastcall gaiden_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xf800:
			MSM6295Write(0, data);
			return;

		case 0xf810:
		case 0xf811:
			BurnYM2203Write(0, address & 1, data);
			return;

		case 0xf820:
		case 0xf821:
			BurnYM2203Write(1, address & 1, data);
			return;
	}
}

static UINT8 __fastcall gaiden_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xf800:
			return MSM6295Read(0);

		case 0xfc00:
			return 0;

		case 0xfc20:
			return soundlatch;
	}

	return 0;
}

static void __fastcall drgnbowl_sound_write(UINT16 address, UINT8 data)
{
	switch (address & 0xff)
	{
		case 0x00:
		case 0x01:
			BurnYM2151Write(address & 1, data);
			return;

		case 0x80:
			MSM6295Write(0, data);
			return;
	}
}

static UINT8 __fastcall drgnbowl_sound_read(UINT16 address)
{
	switch (address & 0xff)
	{
		case 0x01:
			return BurnYM2151Read();

		case 0x80:
			return MSM6295Read(0);

		case 0xc0:
			return soundlatch;
	}

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM	= Next; Next += 0x0040000;
	DrvZ80ROM	= Next; Next += 0x0010000;

	DrvGfxROM0	= Next; Next += 0x0020000;
	DrvGfxROM1	= Next; Next += 0x0100000;
	DrvGfxROM2	= Next; Next += 0x0100000;
	DrvGfxROM3	= Next; Next += 0x0200000;

	bitmap[0]	= (UINT16*)Next; Next += 256 * 256 * sizeof(UINT16);
	bitmap[1]	= (UINT16*)Next; Next += 256 * 256 * sizeof(UINT16);
	bitmap[2]	= (UINT16*)Next; Next += 256 * 256 * sizeof(UINT16);

	DrvOkiROM	= Next; Next += 0x0040000;

	DrvPalette	= (UINT32*)Next; Next += 0x1000 * sizeof(UINT32);

	AllRam		= Next;

	Drv68KRAM	= Next; Next += 0x0004000;
	DrvVidRAM0	= Next; Next += 0x0001000;
	DrvVidRAM1	= Next; Next += 0x0002000;
	DrvVidRAM2	= Next; Next += 0x0002000;
	DrvSprRAM	= Next; Next += 0x0002000;
	DrvSprRAM1	= Next; Next += 0x0002000;
	DrvSprRAM2	= Next; Next += 0x0002000;
	DrvPalRAM	= Next; Next += 0x0002000;

	DrvZ80RAM	= Next; Next += 0x0000800;

	Palette		= (UINT32*)Next; Next += 0x01000 * sizeof(UINT32);

	RamEnd		= Next;
	MemEnd		= Next;

	return 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	soundlatch = 0;
	tx_scroll_x = 0;
	tx_scroll_y = 0;
	fg_scroll_x = 0;
	fg_scroll_y = 0;
	bg_scroll_x = 0;
	bg_scroll_y = 0;
	tx_offset_y = 0;
	fg_offset_y = 0;
	bg_offset_y = 0;
	sproffsety = 0;

	flipscreen = 0;

	prot = 0;
	jumpcode = 0;
	jumppointer = 0;

	if (game == 2) {
		jumppoints = raiga_jumppoints_boot;
	} else {
		jumppoints = wildfang_jumppoints;
	}

	prot = 0;
	jumpcode = 0;

	SekOpen(0);
	SekReset();
	SekClose();

	ZetOpen(0);
	ZetReset();
	if (game == 1) {
		BurnYM2151Reset();
	} else {
		BurnYM2203Reset();
	}
	MSM6295Reset();
	ZetClose();

	HiscoreReset();

	nExtraCycles = 0;

	return 0;
}

static INT32 DrvGfxDecode()
{
	static INT32 Planes[4]         = { 0x000, 0x001, 0x002, 0x003 };
	static INT32 TileXOffsets[16]  = { 0x000, 0x004, 0x008, 0x00c, 0x010, 0x014, 0x018, 0x01c,
		0x100, 0x104, 0x108, 0x10c, 0x110, 0x114, 0x118, 0x11c };
	static INT32 TileYOffsets[16]  = { 0x000, 0x020, 0x040, 0x060, 0x080, 0x0a0, 0x0c0, 0x0e0,
		0x200, 0x220, 0x240, 0x260, 0x280, 0x2a0, 0x2c0, 0x2e0 };

	static INT32 SpriteXOffsets[8] = { 0x000000, 0x000004, 0x400000, 0x400004,
		0x000008, 0x00000c, 0x400008, 0x40000c };
	static INT32 SpriteYOffsets[8] = { 0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70 };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x100000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x010000);

	GfxDecode(0x00800, 4,  8,  8, Planes, TileXOffsets, TileYOffsets, 0x100, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x080000);

	GfxDecode(0x01000, 4, 16, 16, Planes, TileXOffsets, TileYOffsets, 0x400, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x080000);

	GfxDecode(0x01000, 4, 16, 16, Planes, TileXOffsets, TileYOffsets, 0x400, tmp, DrvGfxROM2);

	memcpy (tmp, DrvGfxROM3, 0x100000);

	GfxDecode(0x08000, 4,  8,  8, Planes, SpriteXOffsets, SpriteYOffsets, 0x080, tmp, DrvGfxROM3);

	BurnFree (tmp);

	return 0;
}

static INT32 drgnbowlDecode(INT32 decode_cpu)
{
	UINT8 *buf = (UINT8*)BurnMalloc(0x100000);
	if (buf == NULL) {
		return 1;
	}

	if (decode_cpu) {
		memcpy (buf, Drv68KROM, 0x40000);

		for (INT32 i = 0; i < 0x40000; i++) {
			Drv68KROM[i] = buf[BITSWAP24(i,23,22,21,20,19,18,17,15,16,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0)];
		}
	}

	for (INT32 i = 0; i < 0x100000; i++) {
		buf[i] = DrvGfxROM1[BITSWAP24(i,23,22,21,20,19,18,16,17,15,14,13,4,3,12,11,10,9,8,7,6,5,2,1,0)];
	}

	static INT32 CharPlanes[4]    = { 0, 1, 2, 3 };
	static INT32 CharXOffsets[8]  = { 0, 4, 8, 12, 16, 20, 24, 28 };
	static INT32 CharYOffsets[8]  = { 0x000, 0x020, 0x040, 0x060, 0x080, 0x0a0, 0x0c0, 0x0e0 };

	static INT32 Planes[4] = { 0x600000, 0x400000, 0x200000, 0x000000 };
	static INT32 XOffsets[16]  = { 0, 1, 2, 3, 4, 5, 6, 7, 128, 129, 130, 131, 132, 133, 134, 135 };
	static INT32 YOffsets[16]  = { 0, 8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 88, 96, 104, 112, 120 };

	GfxDecode(0x02000, 4, 16, 16, Planes, XOffsets, YOffsets, 0x100, buf, DrvGfxROM1);

	memcpy (buf, DrvGfxROM3, 0x100000);

	for (INT32 i = 0; i < 0x100000; i++) buf[i] ^= 0xff;

	GfxDecode(0x02000, 4, 16, 16, Planes, XOffsets, YOffsets, 0x100, buf, DrvGfxROM3);

	memcpy (buf, DrvGfxROM0, 0x010000);

	GfxDecode(0x00800, 4,  8,  8, CharPlanes, CharXOffsets, CharYOffsets, 0x100, buf, DrvGfxROM0);

	BurnFree (buf);

	return 0;
}

static INT32 DrvGetRoms()
{
	char* pRomName;
	struct BurnRomInfo ri;
	UINT8 *Load68K = Drv68KROM;
	UINT8 *LoadZ80 = DrvZ80ROM;
	UINT8 *LoadG0  = DrvGfxROM0;
	UINT8 *LoadG1  = DrvGfxROM1;
	UINT8 *LoadG2  = DrvGfxROM2;
	UINT8 *LoadG3  = DrvGfxROM3;
	UINT8 *LoadSND = DrvOkiROM;

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {
		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & 7) == 1) {
			if (BurnLoadRom(Load68K + 1, i + 0, 2)) return 1;
			if (BurnLoadRom(Load68K + 0, i + 1, 2)) return 1;
			Load68K += ri.nLen * 2;
			i++;
			continue;
		}

		if ((ri.nType & 7) == 2) {
			if (BurnLoadRom(LoadZ80, i, 1)) return 1;
			LoadZ80 += ri.nLen;
			continue;
		}

		if ((ri.nType & 7) == 3) {
			if (BurnLoadRom(LoadG0, i, 1)) return 1;
			LoadG0 += ri.nLen;
			continue;
		}

		if ((ri.nType & 7) == 4) {
			if (BurnLoadRom(LoadG1, i, 1)) return 1;
			LoadG1 += ri.nLen;
			continue;
		}

		if ((ri.nType & 7) == 5) {
			if (BurnLoadRom(LoadG2, i, 1)) return 1;
			LoadG2 += ri.nLen;
			continue;
		}

		if ((ri.nType & 7) == 6) {
			if (BurnLoadRom(LoadG3, i, 1)) return 1;
			LoadG3 += ri.nLen;
			continue;
		}

		if ((ri.nType & 7) == 7) {
			if (BurnLoadRom(LoadSND, i, 1)) return 1;
			LoadSND += ri.nLen;
			continue;
		}
	}

	return 0;
}

static void DrvYM2203IRQHandler(INT32, INT32 nStatus)
{
	ZetSetIRQLine(0, (nStatus & 1) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	if (DrvGetRoms()) return 1;

	if (game == 1 || game == 3) {
		if (drgnbowlDecode((game == 1) ? 1 : 0)) return 1;

		game = 1;
	} else {
		if (DrvGfxDecode()) return 1;
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,		0x060000, 0x063fff, MAP_RAM);
	SekMapMemory(DrvVidRAM0,	0x070000, 0x070fff, MAP_RAM);
	SekMapMemory(DrvVidRAM1,	0x072000, 0x073fff, MAP_RAM);
	SekMapMemory(DrvVidRAM2,	0x074000, 0x075fff, MAP_RAM);
	SekMapMemory(DrvSprRAM2,	0x076000, 0x077fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		0x078000, 0x079fff, MAP_ROM);
	SekSetWriteByteHandler(0,	gaiden_write_byte);
	SekSetWriteWordHandler(0,	gaiden_write_word);
	SekSetReadByteHandler(0,	gaiden_read_byte);
	SekSetReadWordHandler(0,	gaiden_read_word);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	if (game == 1) {
		ZetMapMemory(DrvZ80ROM, 0x0000, 0xf7ff, MAP_ROM);
		ZetMapMemory(DrvZ80RAM, 0xf800, 0xffff, MAP_RAM);
		ZetSetOutHandler(drgnbowl_sound_write);
		ZetSetInHandler(drgnbowl_sound_read);
	} else {
		ZetMapMemory(DrvZ80ROM, 0x0000, 0xefff, MAP_ROM);
		ZetMapMemory(DrvZ80RAM, 0xf000, 0xf7ff, MAP_RAM);
		ZetSetWriteHandler(gaiden_sound_write);
		ZetSetReadHandler(gaiden_sound_read);
	}
	ZetClose();

	if (game == 1) {
		BurnYM2151InitBuffered(4000000, 1, NULL, 0);
		BurnYM2151SetAllRoutes(0.40, BURN_SND_ROUTE_BOTH);
		BurnTimerAttachZet(4000000);
	} else {
		BurnYM2203Init(2, 4000000, &DrvYM2203IRQHandler, 0);
		BurnTimerAttachZet(4000000);
		BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE, 0.75, BURN_SND_ROUTE_BOTH);
		BurnYM2203SetPSGVolume(0, 0.10);
		BurnYM2203SetRoute(1, BURN_SND_YM2203_YM2203_ROUTE, 0.75, BURN_SND_ROUTE_BOTH);
		BurnYM2203SetPSGVolume(1, 0.10);
	}

	MSM6295Init(0, 1000000 / 132, 1);
	MSM6295SetRoute(0, 0.28, BURN_SND_ROUTE_BOTH);
	MSM6295SetBank(0, DrvOkiROM, 0x00000, 0x3ffff);

	DrvDoReset();

	GenericTilesInit();

	return 0;
}

static INT32 DrvExit()
{
	SekExit();
	ZetExit();

	MSM6295Exit();
	if (game == 1) {
		BurnYM2151Exit();
	} else {
		BurnYM2203Exit();
	}

	GenericTilesExit();

	BurnFreeMemIndex();

	game = 0;
	sprite_sizey = 0;

	return 0;
}

static void drgnbowl_draw_sprites(INT32 priority)
{
	UINT16 *spriteram16 = (UINT16*)DrvSprRAM2; // not buffered!

	for (INT32 i = 0x400 - 4; i >= 0; i -= 4)
	{
		if ((BURN_ENDIAN_SWAP_INT16(spriteram16[i + 3]) & 0x20) != priority)
			continue;

		INT32 code = (BURN_ENDIAN_SWAP_INT16(spriteram16[i]) & 0xff) | ((BURN_ENDIAN_SWAP_INT16(spriteram16[i + 3]) & 0x1f) << 8);
		INT32 y = 256 - (BURN_ENDIAN_SWAP_INT16(spriteram16[i + 1]) & 0xff) - 12;
		INT32 x = BURN_ENDIAN_SWAP_INT16(spriteram16[i + 2]) & 0xff;
		INT32 color = BURN_ENDIAN_SWAP_INT16(spriteram16[0x400 + i]) & 0x0f;
		INT32 flipx = BURN_ENDIAN_SWAP_INT16(spriteram16[i + 3]) & 0x40;
		INT32 flipy = BURN_ENDIAN_SWAP_INT16(spriteram16[i + 3]) & 0x80;

		if (BURN_ENDIAN_SWAP_INT16(spriteram16[0x400 + i]) & 0x80)
			x -= 256;

		x += 256;

		y -= 16;

		Draw16x16MaskTile(pTransDraw, code, x, y, flipx, flipy, color, 4, 0xf, 0x100, DrvGfxROM3);
		if (x >= 497) Draw16x16MaskTile(pTransDraw, code, x - 512, y, flipx, flipy, color, 4, 0xf, 0x100, DrvGfxROM3);
	}
}

static void draw_layer(UINT16 *dest, UINT16 *vidram, UINT8 *gfxbase, INT32 palette_offset, UINT16 scrollx, UINT16 scrolly, INT32 transp)
{
	for (INT32 offs = 0; offs < 0x800; offs++)
	{
		INT32 sx = (offs & 0x3f) << 4;
		INT32 sy = (offs >> 6) << 4;
		sx -= scrollx;
		sy -= scrolly;

		if (sx < -15) sx += 1024;
		if (sy < -15) sy +=  512;
		if (sy >= 0x100 || sx >= 0x100) continue;

		if (flipscreen) {
			sy = 0x1ff - sy;
			sx = 0x3ff - sx;
			sy -= 0x110;
			sx -= 0x310;
		}

		sy -= 32;

		INT32 code = BURN_ENDIAN_SWAP_INT16(vidram[0x0800 + offs]) & 0x0fff;
		INT32 color = (BURN_ENDIAN_SWAP_INT16(vidram[offs]) >> 4) & 0x0f;

		if (game == 2 && gfxbase == DrvGfxROM2) { //raiga
			color |= (BURN_ENDIAN_SWAP_INT16(vidram[offs]) & 0x08) << 1;
		}

		if (transp == -1) {
			Draw16x16Tile(dest, code, sx, sy, flipscreen, flipscreen, color, 4, palette_offset, gfxbase);
		} else {
			Draw16x16MaskTile(dest, code, sx, sy, flipscreen, flipscreen, color, 4, transp, palette_offset, gfxbase);
		}
	}
}

static void draw_text(INT32 paloffset, INT32 transp)
{
	UINT16 *vidram = (UINT16*)DrvVidRAM0;

	for (INT32 offs = 0; offs < 0x400; offs++)
	{
		INT32 sx = (offs & 0x1f) << 3;
		INT32 sy = (offs >> 2) & 0xf8;

		if (flipscreen) {
			sy = 0xf8 - sy;
			sx = 0xf8 - sx;
		}

		sx -= tx_scroll_x;
		sy = (sy - (tx_scroll_y - tx_offset_y)) & 0xff; // wraps @ 256px, 8,8,32,32 tmap. -dink aug2021
		sy -= ((game == 1) ? 16 : 32);

		INT32 code  = BURN_ENDIAN_SWAP_INT16(vidram[0x400 + offs]) & 0x07ff;
		INT32 color = (BURN_ENDIAN_SWAP_INT16(vidram[0x000 + offs]) >> 4) & 0x0f;

		if (transp == -1) {
			Draw8x8Tile(pTransDraw, code, sx, sy, flipscreen, flipscreen, color, 4, paloffset, DrvGfxROM0);
		} else {
			Draw8x8MaskTile(pTransDraw, code, sx, sy, flipscreen, flipscreen, color, 4, transp, paloffset, DrvGfxROM0);
		}
	}
}

static void gaiden_draw_sprites(INT32 spr_sizey, INT32 spr_offset_y)
{
	static const UINT8 layout[8][8] = {
		{  0,  1,  4,  5, 16, 17, 20, 21 },
		{  2,  3,  6,  7, 18, 19, 22, 23 },
		{  8,  9, 12, 13, 24, 25, 28, 29 },
		{ 10, 11, 14, 15, 26, 27, 30, 31 },
		{ 32, 33, 36, 37, 48, 49, 52, 53 },
		{ 34, 35, 38, 39, 50, 51, 54, 55 },
		{ 40, 41, 44, 45, 56, 57, 60, 61 },
		{ 42, 43, 46, 47, 58, 59, 62, 63 }
	};

	INT32 count = 256;
	UINT16 *source = (UINT16*)DrvSprRAM;

	while (count--)
	{
		UINT32 attributes = source[0];

		if (attributes & 0x04)
		{
			UINT32 flipx = (attributes & 1);
			UINT32 flipy = (attributes & 2);

			UINT32 color = source[2];
			UINT32 sizex = 1 << ((color >> 0) & 3);
			UINT32 sizey = 1 << ((color >> spr_sizey) & 3);

			UINT32 number = (source[1]);
			if (sizex >= 2) number &= ~0x01;
			if (sizey >= 2) number &= ~0x02;
			if (sizex >= 4) number &= ~0x04;
			if (sizey >= 4) number &= ~0x08;
			if (sizex >= 8) number &= ~0x10;
			if (sizey >= 8) number &= ~0x20;

			int ypos = (source[3] + spr_offset_y) & 0x1ff;
			int xpos = source[4] & 0x1ff;

			color = (color >> 4) & 0x0f;

			if (xpos >= 256) xpos -= 512;
			if (ypos >= 256) ypos -= 512;

			if (flipscreen)
			{
				flipx = !flipx;
				flipy = !flipy;

				xpos = 256 - (8 * sizex) - xpos;
				ypos = 256 - (8 * sizey) - ypos;

				if (xpos <= -256) xpos += 512;
				if (ypos <= -256) ypos += 512;
			}

			color |= attributes & 0x03f0;

			for (INT32 row = 0; row < (INT32)sizey; row++)
			{
				for (INT32 col = 0; col < (INT32)sizex; col++)
				{
					INT32 sx = xpos + 8 * (flipx ? (sizex - 1 - col) : col);
					INT32 sy =(ypos + 8 * (flipy ? (sizey - 1 - row) : row)) - 32;

					Draw8x8MaskTile(bitmap[2], number + layout[row][col], sx, sy, flipx, flipy, color, 4, 0, 0, DrvGfxROM3);
				}
			}
		}

		source += 8;
	}
}

static void mix_bitmaps(UINT32 *paldata, UINT16* bitmap_bg, UINT16* bitmap_fg, UINT16* bitmap_tx, UINT16* bitmap_sp)
{
	for (INT32 y = 0; y < nScreenHeight; y++)
	{
		UINT16 *sd2 = bitmap_sp + y * 256;
		UINT16 *fg =  bitmap_fg + y * 256;
		UINT16 *bg =  bitmap_bg + y * 256;
		UINT16 *tx =  bitmap_tx + y * 256;

		UINT8 *pdst = pBurnDraw + (y * nScreenWidth * nBurnBpp);

		for (INT32 x = 0; x < nScreenWidth; x++)
		{
			UINT16 sprpixel = (sd2[x]);

			UINT16 m_sprpri = (sprpixel >> 10) & 0x3;
			UINT16 m_sprbln = (sprpixel >> 9) & 0x1;
			UINT16 m_sprcol = (sprpixel >> 4) & 0xf;

			UINT32 dest = paldata[0x200];

			sprpixel = (sprpixel & 0xf) | (m_sprcol << 4);

			UINT16 fgpixel = (fg[x]);
			UINT16 fgbln = (fgpixel & 0x0100) >> 8;
			fgpixel &= 0xff;

			UINT16 bgpixel = (bg[x]);
			bgpixel &= 0xff;

			if (sprpixel&0xf)
			{
				if (m_sprpri == 3)
				{
					if (fgpixel & 0xf)
					{
						if (fgbln)
						{
							dest = rand();
						}
						else
						{
							dest = paldata[fgpixel + 0x200];
						}
					}
					else if (bgpixel & 0x0f)
					{
						dest = paldata[bgpixel + 0x300];
					}
					else
					{
						if (m_sprbln)
						{
							dest = rand();
						}
						else
						{
							dest = paldata[sprpixel];
						}

					}
				}
				else  if (m_sprpri == 2)
				{
					if (fgpixel & 0xf)
					{
						if (fgbln)
						{
							if (m_sprbln)
							{
								dest = paldata[bgpixel + 0x700] + paldata[sprpixel + 0x800];
							}
							else
							{
								dest = paldata[fgpixel + 0xa00] + paldata[sprpixel + 0x400];
							}
						}
						else
						{
							dest = paldata[fgpixel + 0x200];
						}

					}
					else
					{
						if (m_sprbln)
						{
							dest = paldata[bgpixel + 0x700] + paldata[sprpixel + 0x800];
						}
						else
						{
							dest = paldata[sprpixel];
						}
					}


				}
				else if (m_sprpri == 1)
				{
					if (m_sprbln)
					{
						if (fgpixel & 0xf)
						{
							if (fgbln)
							{
								dest =  rand();
							}
							else
							{
								dest = paldata[fgpixel + 0x600] + paldata[sprpixel + 0x800];
							}
						}
						else
						{
							dest = paldata[bgpixel + 0x700] + paldata[sprpixel + 0x800];
						}
					}
					else
					{
						dest = paldata[sprpixel];
					}
				}

				else if (m_sprpri == 0)
				{
					if (m_sprbln)
					{
						dest = rand();
					}
					else
					{
						dest = paldata[sprpixel];
					}

				}
			}
			else
			{
				if (fgpixel & 0x0f)
				{
					if (fgbln)
					{
						dest = paldata[fgpixel + 0xa00] + paldata[bgpixel + 0x700];

					}
					else
					{
						dest = paldata[fgpixel + 0x200];
					}

				}
				else if (bgpixel & 0x0f)
				{
					dest = paldata[bgpixel + 0x300];
				}
			}

			if (tx[x] & 0xf) {
				dest = paldata[tx[x]];
			}

			// draw screen
			PutPix(pdst + x * nBurnBpp, DrvPalette[dest & 0xfff]);
		}
	}
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x1000; i++)
	{
		INT32 r = (i >> 8) & 0xf;
		INT32 g = (i >> 4) & 0xf;
		INT32 b = (i >> 0) & 0xf;

		r += r * 16;
		g += g * 16;
		b += b * 16;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	pBurnDrvPalette = DrvPalette;

	memset (bitmap[2], 0, 256 * 256 * sizeof(UINT16)); // clear sprite bitmap

	if (nBurnLayer & 1) draw_layer(bitmap[0], (UINT16*)DrvVidRAM2, DrvGfxROM1, 0x000, bg_scroll_x, (bg_scroll_y - bg_offset_y) & 0x1ff, -1);
	if (nBurnLayer & 2) draw_layer(bitmap[1], (UINT16*)DrvVidRAM1, DrvGfxROM2, 0x000, fg_scroll_x, (fg_scroll_y - fg_offset_y) & 0x1ff, -1);
	if (nBurnLayer & 4) gaiden_draw_sprites(sprite_sizey, sproffsety);
	if (nBurnLayer & 8) draw_text(0x100,-1);

	mix_bitmaps(Palette, bitmap[0], bitmap[1], pTransDraw, bitmap[2]);

	return 0;
}

static INT32 DrgnbowlDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x1000; i++) {
			INT32 rgb = Palette[i];

			UINT8 r = (rgb >> 8) & 0xf;
			UINT8 g = (rgb >> 4) & 0xf;
			UINT8 b = (rgb >> 0) & 0xf;

			r = (r << 4) | r;
			g = (g << 4) | g;
			b = (b << 4) | b;

			DrvPalette[i] = BurnHighCol(r, g, b, 0);
		}

		DrvRecalc = 0;
	}

	draw_layer(pTransDraw, (UINT16*)DrvVidRAM2, DrvGfxROM1, 0x300, bg_scroll_x, (bg_scroll_y - 16) & 0x1ff,  -1); // no transp
	drgnbowl_draw_sprites(0x20);
	draw_layer(pTransDraw, (UINT16*)DrvVidRAM1, DrvGfxROM2, 0x200, fg_scroll_x, (fg_scroll_y - 16) & 0x1ff, 0xf);
	drgnbowl_draw_sprites(0x00);
	draw_text(0, 0x0f);

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
		DrvInputs[0] = DrvInputs[1] = DrvInputs[2] = 0xff;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { (game == 1 ? 10000000 : 9216000) / 60, 4000000 / 60 };
	INT32 nCyclesDone[2] = { nExtraCycles, 0 };

	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++) {
		CPU_RUN(0, Sek);

		if (i == 240) {
			SekSetIRQLine(5, CPU_IRQSTATUS_ACK);

			if (pBurnDraw) {
				BurnDrvRedraw();
			}
			memcpy(DrvSprRAM,  DrvSprRAM1, 0x2000);
			memcpy(DrvSprRAM1, DrvSprRAM2, 0x2000);
		}

		CPU_RUN_TIMER(1);
	}

	if (pBurnSoundOut) {
		if (game == 1) {
			BurnYM2151Render(pBurnSoundOut, nBurnSoundLen);
		} else {
			BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
			BurnSoundDCFilter(); // psg (ay8910) imposes DC offset in raiga
		}
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	SekClose();

	nExtraCycles = nCyclesDone[0] - nCyclesTotal[0];

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029523;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);
		ZetScan(nAction);

		MSM6295Scan(nAction, pnMin);

		if (game == 1) {
			BurnYM2151Scan(nAction, pnMin);
		} else {
			BurnYM2203Scan(nAction, pnMin);
		}

		SCAN_VAR(prot);
		SCAN_VAR(jumpcode);
		SCAN_VAR(jumppointer);

		SCAN_VAR(tx_scroll_x);
		SCAN_VAR(tx_scroll_y);
		SCAN_VAR(fg_scroll_x);
		SCAN_VAR(fg_scroll_y);
		SCAN_VAR(bg_scroll_x);
		SCAN_VAR(bg_scroll_y);

		SCAN_VAR(tx_offset_y);
		SCAN_VAR(fg_offset_y);
		SCAN_VAR(bg_offset_y);
		SCAN_VAR(sproffsety);

		SCAN_VAR(soundlatch);
		SCAN_VAR(flipscreen);

		SCAN_VAR(nExtraCycles);

		if (jumppointer && game == 2) {
			jumppoints = raiga_jumppoints_ingame;
		}
	}

	return 0;
}


// Shadow Warriors (World, set 1)

static struct BurnRomInfo shadowwRomDesc[] = {
	{ "shadowa_1.3s",	0x20000, 0x8290d567, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "shadowa_2.4s",	0x20000, 0xf3f08921, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "gaiden_3.4b",    0x10000, 0x75fd3e6a, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "gaiden_5.7a",	0x10000, 0x8d4035f7, 3 | BRF_GRA },           //  3 Characters

	{ "14.3a",       	0x20000, 0x1ecfddaa, 4 | BRF_GRA },           //  4 Foreground Tiles
	{ "15.3b",       	0x20000, 0x1291a696, 4 | BRF_GRA },           //  5
	{ "16.1a",       	0x20000, 0x140b47ca, 4 | BRF_GRA },           //  6
	{ "17.1b",       	0x20000, 0x7638cccb, 4 | BRF_GRA },           //  7

	{ "18.6a",       	0x20000, 0x3fadafd6, 5 | BRF_GRA },           //  8 Background Tiles
	{ "19.6b",       	0x20000, 0xddae9d5b, 5 | BRF_GRA },           //  9
	{ "20.4b",       	0x20000, 0x08cf7a93, 5 | BRF_GRA },           // 10
	{ "21.4b",       	0x20000, 0x1ac892f5, 5 | BRF_GRA },           // 11

	// sprite roms also seen on daughterboard "4M512" with 16 0x10000-sized roms
	{ "6.3m",         	0x20000, 0xe7ccdf9f, 6 | BRF_GRA },           // 12 Sprites
	{ "8.3n",         	0x20000, 0x7ef7f880, 6 | BRF_GRA },           // 13
	{ "10.3r",        	0x20000, 0xa6451dec, 6 | BRF_GRA },           // 14
	{ "12.3s", 			0x20000, 0x94a836d8, 6 | BRF_GRA },           // 15
	{ "7.1m",         	0x20000, 0x016bec95, 6 | BRF_GRA },           // 16
	{ "9.1n",         	0x20000, 0x6e9b7fd3, 6 | BRF_GRA },           // 17
	{ "11.1r",        	0x20000, 0x7fbfdf5e, 6 | BRF_GRA },           // 18
	{ "13.1s", 			0x20000, 0xe9caea3b, 6 | BRF_GRA },           // 19

	{ "4.4a",     		0x20000, 0xb0e0faf9, 7 | BRF_SND },           // 20 MSM6295 Samples
};

STD_ROM_PICK(shadoww)
STD_ROM_FN(shadoww)

struct BurnDriver BurnDrvShadoww = {
	"shadoww", NULL, NULL, NULL, "1988",
	"Shadow Warriors (World, set 1)\0", NULL, "Tecmo", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, shadowwRomInfo, shadowwRomName, NULL, NULL, NULL, NULL, DrvInputInfo, GaidenDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	256, 224, 4, 3
};


// Shadow Warriors (World, set 2)

static struct BurnRomInfo shadowwaRomDesc[] = {
	{ "shadoww_1.3s",	0x20000, 0xfefba387, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "shadoww_2.4s",	0x20000, 0x9b9d6b18, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "gaiden_3.4b",   	0x10000, 0x75fd3e6a, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "gaiden_5.7a",	0x10000, 0x8d4035f7, 3 | BRF_GRA },           //  3 Characters

	{ "14.3a",       	0x20000, 0x1ecfddaa, 4 | BRF_GRA },           //  4 Foreground Tiles
	{ "15.3b",       	0x20000, 0x1291a696, 4 | BRF_GRA },           //  5
	{ "16.1a",       	0x20000, 0x140b47ca, 4 | BRF_GRA },           //  6
	{ "17.1b",       	0x20000, 0x7638cccb, 4 | BRF_GRA },           //  7

	{ "18.6a",       	0x20000, 0x3fadafd6, 5 | BRF_GRA },           //  8 Background Tiles
	{ "19.6b",       	0x20000, 0xddae9d5b, 5 | BRF_GRA },           //  9
	{ "20.4b",       	0x20000, 0x08cf7a93, 5 | BRF_GRA },           // 10
	{ "21.4b",       	0x20000, 0x1ac892f5, 5 | BRF_GRA },           // 11

	// sprite roms also seen on daughterboard "4M512" with 16 0x10000-sized roms
	{ "6.3m",         	0x20000, 0xe7ccdf9f, 6 | BRF_GRA },           // 12 Sprites
	{ "8.3n",         	0x20000, 0x7ef7f880, 6 | BRF_GRA },           // 13
	{ "10.3r",        	0x20000, 0xa6451dec, 6 | BRF_GRA },           // 14
	{ "12.3s", 			0x20000, 0x94a836d8, 6 | BRF_GRA },           // 15
	{ "7.1m",         	0x20000, 0x016bec95, 6 | BRF_GRA },           // 16
	{ "9.1n",         	0x20000, 0x6e9b7fd3, 6 | BRF_GRA },           // 17
	{ "11.1r",        	0x20000, 0x7fbfdf5e, 6 | BRF_GRA },           // 18
	{ "13.1s", 			0x20000, 0xe9caea3b, 6 | BRF_GRA },           // 19

	{ "4.4a",     		0x20000, 0xb0e0faf9, 7 | BRF_SND },           // 20 MSM6295 Samples
};

STD_ROM_PICK(shadowwa)
STD_ROM_FN(shadowwa)

struct BurnDriver BurnDrvShadowwa = {
	"shadowwa", "shadoww", NULL, NULL, "1988",
	"Shadow Warriors (World, set 2)\0", NULL, "Tecmo", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, shadowwaRomInfo, shadowwaRomName, NULL, NULL, NULL, NULL, DrvInputInfo, GaidenDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	256, 224, 4, 3
};


// Ninja Gaiden (US)

static struct BurnRomInfo gaidenRomDesc[] = {
	{ "gaiden_1.3s",    0x20000, 0xe037ff7c, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "gaiden_2.4s",    0x20000, 0x454f7314, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "gaiden_3.4b",    0x10000, 0x75fd3e6a, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "gaiden_5.7a",	0x10000, 0x8d4035f7, 3 | BRF_GRA },           //  3 Characters

	{ "14.3a",       	0x20000, 0x1ecfddaa, 4 | BRF_GRA },           //  4 Foreground Tiles
	{ "15.3b",       	0x20000, 0x1291a696, 4 | BRF_GRA },           //  5
	{ "16.1a",       	0x20000, 0x140b47ca, 4 | BRF_GRA },           //  6
	{ "17.1b",       	0x20000, 0x7638cccb, 4 | BRF_GRA },           //  7

	{ "18.6a",       	0x20000, 0x3fadafd6, 5 | BRF_GRA },           //  8 Background Tiles
	{ "19.6b",       	0x20000, 0xddae9d5b, 5 | BRF_GRA },           //  9
	{ "20.4b",       	0x20000, 0x08cf7a93, 5 | BRF_GRA },           // 10
	{ "21.4b",       	0x20000, 0x1ac892f5, 5 | BRF_GRA },           // 11

	// sprite roms also seen on daughterboard "4M512" with 16 0x10000-sized roms
	{ "6.3m",         	0x20000, 0xe7ccdf9f, 6 | BRF_GRA },           // 12 Sprites
	{ "8.3n",         	0x20000, 0x7ef7f880, 6 | BRF_GRA },           // 13
	{ "10.3r",        	0x20000, 0xa6451dec, 6 | BRF_GRA },           // 14
	{ "12.3s", 		    0x20000, 0x90f1e13a, 6 | BRF_GRA },           // 15
	{ "7.1m",         	0x20000, 0x016bec95, 6 | BRF_GRA },           // 16
	{ "9.1n",         	0x20000, 0x6e9b7fd3, 6 | BRF_GRA },           // 17
	{ "11.1r",        	0x20000, 0x7fbfdf5e, 6 | BRF_GRA },           // 18
	{ "13.1s", 			0x20000, 0x7d9f5c5e, 6 | BRF_GRA },           // 19

	{ "4.4a",     		0x20000, 0xb0e0faf9, 7 | BRF_SND },           // 20 MSM6295 Samples
};

STD_ROM_PICK(gaiden)
STD_ROM_FN(gaiden)

struct BurnDriver BurnDrvGaiden = {
	"gaiden", "shadoww", NULL, NULL, "1988",
	"Ninja Gaiden (US)\0", NULL, "Tecmo", "Miscellaneous",
	L"Ninja \u5916\u4F1D Gaiden (US)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, gaidenRomInfo, gaidenRomName, NULL, NULL, NULL, NULL, DrvInputInfo, GaidenDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	256, 224, 4, 3
};


// Ninja Ryukenden (Japan, set 1)

static struct BurnRomInfo ryukendnRomDesc[] = {
	{ "ryukendn_1.3s",  0x20000, 0x6203a5e2, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "ryukendn_2.4s",  0x20000, 0x9e99f522, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "3.4b",   		0x10000, 0x6b686b69, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "hn27512p.7a",   	0x10000, 0x765e7baa, 3 | BRF_GRA },           //  3 Characters

	{ "14.3a",       	0x20000, 0x1ecfddaa, 4 | BRF_GRA },           //  4 Foreground Tiles
	{ "15.3b",       	0x20000, 0x1291a696, 4 | BRF_GRA },           //  5
	{ "16.1a",       	0x20000, 0x140b47ca, 4 | BRF_GRA },           //  6
	{ "17.1b",       	0x20000, 0x7638cccb, 4 | BRF_GRA },           //  7

	{ "18.6a",       	0x20000, 0x3fadafd6, 5 | BRF_GRA },           //  8 Background Tiles
	{ "19.6b",       	0x20000, 0xddae9d5b, 5 | BRF_GRA },           //  9
	{ "20.4b",       	0x20000, 0x08cf7a93, 5 | BRF_GRA },           // 10
	{ "21.4b",       	0x20000, 0x1ac892f5, 5 | BRF_GRA },           // 11

	{ "6.3m",         	0x20000, 0xe7ccdf9f, 6 | BRF_GRA },           // 12 Sprites
	{ "8.3n",         	0x20000, 0x7ef7f880, 6 | BRF_GRA },           // 13
	{ "10.3r",        	0x20000, 0xa6451dec, 6 | BRF_GRA },           // 14
	{ "12.3s", 			0x20000, 0x277204f0, 6 | BRF_GRA },           // 15
	{ "7.1m",         	0x20000, 0x016bec95, 6 | BRF_GRA },           // 16
	{ "9.1n",         	0x20000, 0x6e9b7fd3, 6 | BRF_GRA },           // 17
	{ "11.1r",        	0x20000, 0x7fbfdf5e, 6 | BRF_GRA },           // 18
	{ "13.1s", 			0x20000, 0x4e56a508, 6 | BRF_GRA },           // 19

	{ "4.4a",     		0x20000, 0xb0e0faf9, 7 | BRF_SND },           // 20 MSM6295 Samples
};

STD_ROM_PICK(ryukendn)
STD_ROM_FN(ryukendn)

struct BurnDriver BurnDrvRyukendn = {
	"ryukendn", "shadoww", NULL, NULL, "1989",
	"Ninja Ryukenden (Japan, set 1)\0", NULL, "Tecmo", "Miscellaneous",
	L"\u5FCD\u8005 \u9F8D\u5263\u4F1D (Japan, set 1)\0Ninja Ryukenden\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, ryukendnRomInfo, ryukendnRomName, NULL, NULL, NULL, NULL, DrvInputInfo, GaidenDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	256, 224, 4, 3
};


// Ninja Ryukenden (Japan, set 2)
// Dumped from an original Tecmo board. Board No. 6215-A. Serial A-59488.

static struct BurnRomInfo ryukendnaRomDesc[] = {
	{ "1.3s",  		    0x20000, 0x5532e302, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	//	{ "1.3s",  		    0x20000, 0x5532e302, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	//	 2 bytes different ( 022a : 50 instead of 51, 12f9 : 6b instead of 6a) - possible bad rom
	{ "2.4s",  		    0x20000, 0xa93a8256, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "3.4b",   		0x10000, 0x6b686b69, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "hn27512p.7a",   	0x10000, 0x765e7baa, 3 | BRF_GRA },           //  3 Characters

	{ "14.3a",       	0x20000, 0x1ecfddaa, 4 | BRF_GRA },           //  4 Foreground Tiles
	{ "15.3b",       	0x20000, 0x1291a696, 4 | BRF_GRA },           //  5
	{ "16.1a",       	0x20000, 0x140b47ca, 4 | BRF_GRA },           //  6
	{ "17.1b",       	0x20000, 0x7638cccb, 4 | BRF_GRA },           //  7

	{ "18.6a",          0x20000, 0x3fadafd6, 5 | BRF_GRA },           //  8 Background Tiles
	{ "19.6b",       	0x20000, 0xddae9d5b, 5 | BRF_GRA },           //  9
	{ "20.4b",       	0x20000, 0x08cf7a93, 5 | BRF_GRA },           // 10
	{ "21.4b",       	0x20000, 0x1ac892f5, 5 | BRF_GRA },           // 11

	{ "6.3m",         	0x20000, 0xe7ccdf9f, 6 | BRF_GRA },           // 12 Sprites
	{ "8.3n",         	0x20000, 0x7ef7f880, 6 | BRF_GRA },           // 13
	{ "10.3r",        	0x20000, 0xa6451dec, 6 | BRF_GRA },           // 14
	{ "12.3s", 			0x20000, 0x277204f0, 6 | BRF_GRA },           // 15
	{ "7.1m",         	0x20000, 0x016bec95, 6 | BRF_GRA },           // 16
	{ "9.1n",         	0x20000, 0x6e9b7fd3, 6 | BRF_GRA },           // 17
	{ "11.1r",        	0x20000, 0x7fbfdf5e, 6 | BRF_GRA },           // 18
	{ "13.1s", 			0x20000, 0x4e56a508, 6 | BRF_GRA },           // 19

	{ "4.4a",     		0x20000, 0xb0e0faf9, 7 | BRF_SND },           // 20 MSM6295 Samples
};

STD_ROM_PICK(ryukendna)
STD_ROM_FN(ryukendna)

struct BurnDriver BurnDrvRyukendna = {
	"ryukendna", "shadoww", NULL, NULL, "1989",
	"Ninja Ryukenden (Japan, set 2)\0", NULL, "Tecmo", "Miscellaneous",
	L"\u5FCD\u8005 \u9F8D\u5263\u4F1D (Japan, set 2)\0Ninja Ryukenden\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, ryukendnaRomInfo, ryukendnaRomName, NULL, NULL, NULL, NULL, DrvInputInfo, GaidenDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	256, 224, 4, 3
};


// Wild Fang / Tecmo Knight

static struct BurnRomInfo wildfangRomDesc[] = {
	{ "1.3st",		    0x20000, 0xab876c9b, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "2.5st",		    0x20000, 0x1dc74b3b, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "tkni3.bin",		0x10000, 0x15623ec7, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "tkni5.bin",		0x10000, 0x5ed15896, 3 | BRF_GRA },           //  3 Characters

	{ "14.3a",		    0x20000, 0x0d20c10c, 4 | BRF_GRA },           //  4 Foreground Tiles
	{ "15.3b",		    0x20000, 0x3f40a6b4, 4 | BRF_GRA },           //  5
	{ "16.1a",		    0x20000, 0x0f31639e, 4 | BRF_GRA },           //  6
	{ "17.1b",		    0x20000, 0xf32c158e, 4 | BRF_GRA },           //  7

	{ "tkni6.bin",		0x80000, 0xf68fafb1, 5 | BRF_GRA },           //  8

	{ "tkni9.bin",		0x80000, 0xd22f4239, 6 | BRF_GRA },           //  9 Sprites
	{ "tkni8.bin",		0x80000, 0x4931b184, 6 | BRF_GRA },           // 10

	{ "tkni4.bin",		0x20000, 0xa7a1dbcf, 7 | BRF_SND },           // 11 MSM6295 Samples

	{ "a-6v.mcu",       0x01000, 0x00000000, 0 | BRF_OPT | BRF_NODUMP },
};

STD_ROM_PICK(wildfang)
STD_ROM_FN(wildfang)

struct BurnDriver BurnDrvWildfang = {
	"wildfang", NULL, NULL, NULL, "1989",
	"Wild Fang / Tecmo Knight\0", NULL, "Tecmo", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, wildfangRomInfo, wildfangRomName, NULL, NULL, NULL, NULL, DrvInputInfo, WildfangDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	256, 224, 4, 3
};


// Wild Fang

static struct BurnRomInfo wildfangsRomDesc[] = {
	{ "1.3s",		    0x20000, 0x3421f691, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "2.5s",		    0x20000, 0xd3547708, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "tkni3.bin",		0x10000, 0x15623ec7, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "tkni5.bin",		0x10000, 0x5ed15896, 3 | BRF_GRA },           //  3 Characters

	{ "14.3a",		    0x20000, 0x0d20c10c, 4 | BRF_GRA },           //  4 Foreground Tiles
	{ "15.3b",		    0x20000, 0x3f40a6b4, 4 | BRF_GRA },           //  5
	{ "16.1a",		    0x20000, 0x0f31639e, 4 | BRF_GRA },           //  6
	{ "17.1b",		    0x20000, 0xf32c158e, 4 | BRF_GRA },           //  7

	{ "tkni6.bin",		0x80000, 0xf68fafb1, 5 | BRF_GRA },           //  8

	{ "tkni9.bin",		0x80000, 0xd22f4239, 6 | BRF_GRA },           //  9 Sprites
	{ "tkni8.bin",		0x80000, 0x4931b184, 6 | BRF_GRA },           // 10

	{ "tkni4.bin",		0x20000, 0xa7a1dbcf, 7 | BRF_SND },           // 11 MSM6295 Samples

	{ "a-6v.mcu",       0x01000, 0x00000000, 0 | BRF_OPT | BRF_NODUMP },
};

STD_ROM_PICK(wildfangs)
STD_ROM_FN(wildfangs)

struct BurnDriver BurnDrvWildfangs = {
	"wildfangs", "wildfang", NULL, NULL, "1989",
	"Wild Fang\0", NULL, "Tecmo", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, wildfangsRomInfo, wildfangsRomName, NULL, NULL, NULL, NULL, DrvInputInfo, TknightDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	256, 224, 4, 3
};


// Tecmo Knight

static struct BurnRomInfo tknightRomDesc[] = {
	{ "tkni1.bin",		0x20000, 0x9121daa8, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "tkni2.bin",		0x20000, 0x6669cd87, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "tkni3.bin",		0x10000, 0x15623ec7, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "tkni5.bin",		0x10000, 0x5ed15896, 3 | BRF_GRA },           //  3 Characters

	{ "tkni7.bin",		0x80000, 0x4b4d4286, 4 | BRF_GRA },           //  4 Foreground Tiles

	{ "tkni6.bin",		0x80000, 0xf68fafb1, 5 | BRF_GRA },           //  5

	{ "tkni9.bin",		0x80000, 0xd22f4239, 6 | BRF_GRA },           //  6 Sprites
	{ "tkni8.bin",		0x80000, 0x4931b184, 6 | BRF_GRA },           //  7

	{ "tkni4.bin",		0x20000, 0xa7a1dbcf, 7 | BRF_SND },           //  8 MSM6295 Samples

	{ "a-6v.mcu",       0x01000, 0x00000000, 0 | BRF_OPT | BRF_NODUMP },
};

STD_ROM_PICK(tknight)
STD_ROM_FN(tknight)

struct BurnDriver BurnDrvTknight = {
	"tknight", "wildfang", NULL, NULL, "1989",
	"Tecmo Knight\0", NULL, "Tecmo", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, tknightRomInfo, tknightRomName, NULL, NULL, NULL, NULL, DrvInputInfo, TknightDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	256, 224, 4, 3
};


// Raiga - Strato Fighter (US)

static struct BurnRomInfo stratofRomDesc[] = {
	{ "1.3s",		    0x20000, 0x060822a4, 1 | BRF_PRG | BRF_ESS }, // 0 68k Code
	{ "2.4s",		    0x20000, 0x339358fa, 1 | BRF_PRG | BRF_ESS }, // 1

	{ "a-4b.3",		  	0x10000, 0x18655c95, 2 | BRF_PRG | BRF_ESS }, // 2 Z80 Code

	{ "b-7a.5",		  	0x10000, 0x6d2e4bf1, 3 | BRF_GRA },           // 3 Characters

	{ "b-1b",		    0x80000, 0x781d1bd2, 4 | BRF_GRA },           // 4 Foreground Tiles

	{ "b-4b",		    0x80000, 0x89468b84, 5 | BRF_GRA },           // 5

	{ "b-2m",		    0x80000, 0x5794ec32, 6 | BRF_GRA },           // 6 Sprites
	{ "b-1m",		    0x80000, 0xb0de0ded, 6 | BRF_GRA },           // 7

	{ "a-4a.4",		  	0x20000, 0xef9acdcf, 7 | BRF_SND },           // 8 MSM6295 Samples

	{ "a-6v.mcu",		0x01000, 0x00000000, 0 | BRF_NODUMP },	      // 9 MCU
};

STD_ROM_PICK(stratof)
STD_ROM_FN(stratof)

static INT32 stratofInit()
{
	game = 2;
	sprite_sizey = 2;

	return DrvInit();
}

struct BurnDriver BurnDrvStratof = {
	"stratof", NULL, NULL, NULL, "1991",
	"Raiga - Strato Fighter (US)\0", NULL, "Tecmo", "Miscellaneous",
	L"\u96F7\u7259 Strato Fighter (US)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_HORSHOOT, 0,
	NULL, stratofRomInfo, stratofRomName, NULL, NULL, NULL, NULL, RaigaInputInfo, RaigaDIPInfo,
	stratofInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	256, 224, 4, 3
};


// Raiga - Strato Fighter (Japan)

static struct BurnRomInfo raigaRomDesc[] = {
	{ "a-3s.1",		  	0x20000, 0x303c2a6c, 1 | BRF_PRG | BRF_ESS }, // 0 68k Code
	{ "a-4s.2",		  	0x20000, 0x5f31fecb, 1 | BRF_PRG | BRF_ESS }, // 1

	{ "a-4b.3",		  	0x10000, 0x18655c95, 2 | BRF_PRG | BRF_ESS }, // 2 Z80 Code

	{ "b-7a.5",		  	0x10000, 0x6d2e4bf1, 3 | BRF_GRA },           // 3 Characters

	{ "b-1b",		    0x80000, 0x781d1bd2, 4 | BRF_GRA },           // 4 Foreground Tiles

	{ "b-4b",		    0x80000, 0x89468b84, 5 | BRF_GRA },           // 5

	{ "b-2m",		    0x80000, 0x5794ec32, 6 | BRF_GRA },           // 6 Sprites
	{ "b-1m",		    0x80000, 0xb0de0ded, 6 | BRF_GRA },           // 7

	{ "a-4a.4",		  	0x20000, 0xef9acdcf, 7 | BRF_SND },           // 8 MSM6295 Samples

	{ "a-6v.mcu",		0x01000, 0x00000000, 0 | BRF_NODUMP },	      // 9 MCU
};

STD_ROM_PICK(raiga)
STD_ROM_FN(raiga)

struct BurnDriver BurnDrvRaiga = {
	"raiga", "stratof", NULL, NULL, "1991",
	"Raiga - Strato Fighter (Japan)\0", NULL, "Tecmo", "Miscellaneous",
	L"\u96F7\u7259 Strato Fighter (Japan)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_HORSHOOT, 0,
	NULL, raigaRomInfo, raigaRomName, NULL, NULL, NULL, NULL, RaigaInputInfo, RaigaDIPInfo,
	stratofInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	256, 224, 4, 3
};


// Dragon Bowl

static struct BurnRomInfo drgnbowlRomDesc[] = {
	{ "4.3h",		  	0x20000, 0x90730008, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "5.4h",		  	0x20000, 0x193cc915, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "1.2r",		  	0x10000, 0xd9cbf84a, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "22.6m",			0x10000, 0x86e41198, 3 | BRF_GRA },           //  3 Characters

	{ "6.5a",		  	0x20000, 0xb15759f7, 4 | BRF_GRA },           //  4 Foreground & Background Tiles
	{ "7.5b",		  	0x20000, 0x2541d445, 4 | BRF_GRA },           //  5
	{ "8.6a",		  	0x20000, 0x51a2f5c4, 4 | BRF_GRA },           //  6
	{ "9.6b",		  	0x20000, 0xf4c8850f, 4 | BRF_GRA },           //  7
	{ "10.7a",			0x20000, 0x9e4b3c61, 4 | BRF_GRA },           //  8
	{ "11.7b",			0x20000, 0x0d33d083, 4 | BRF_GRA },           //  9
	{ "12.8a",			0x20000, 0x6c497ad3, 4 | BRF_GRA },           // 10
	{ "13.8b",			0x20000, 0x7a84adff, 4 | BRF_GRA },           // 11

	{ "21.8r",			0x20000, 0x0cee8711, 6 | BRF_GRA },           // 18 Sprites
	{ "20.8q",			0x20000, 0x9647e02a, 6 | BRF_GRA },           // 19
	{ "19.7r",			0x20000, 0x5082ceff, 6 | BRF_GRA },           // 16
	{ "18.7q",			0x20000, 0xd18a7ffb, 6 | BRF_GRA },           // 17
	{ "17.6r",			0x20000, 0x9088af09, 6 | BRF_GRA },           // 14
	{ "16.6q",			0x20000, 0x8ade4e01, 6 | BRF_GRA },           // 15
	{ "15.5r",			0x20000, 0x7429371c, 6 | BRF_GRA },           // 12
	{ "14.5q",			0x20000, 0x4301b97f, 6 | BRF_GRA },           // 13

	{ "3.3q",		  	0x20000, 0x489c6d0e, 7 | BRF_SND },           // 20 MSM6295 Samples
	{ "2.3r",		  	0x20000, 0x7710ce39, 7 | BRF_SND },           // 21
};

STD_ROM_PICK(drgnbowl)
STD_ROM_FN(drgnbowl)

static INT32 drgnbowlInit()
{
	game = 1;

	INT32 nRet = DrvInit();

	MSM6295SetRoute(0, 0.50, BURN_SND_ROUTE_BOTH);

	return nRet;
}

struct BurnDriver BurnDrvDrgnbowl = {
	"drgnbowl", NULL, NULL, NULL, "1992",
	"Dragon Bowl\0", NULL, "Nics", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, drgnbowlRomInfo, drgnbowlRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrgnbowlDIPInfo,
	drgnbowlInit, DrvExit, DrvFrame, DrgnbowlDraw, DrvScan, &DrvRecalc, 0x1000,
	256, 224, 4, 3
};


// Dragon Bowl (set 2, unencrypted program)

static struct BurnRomInfo drgnbowlaRomDesc[] = {
	{ "dbowl_4.u4",		0x20000, 0x58d69235, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "dbowl_5.u3",	  	0x20000, 0xe3176ebb, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "1.2r",		    0x10000, 0xd9cbf84a, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "22.6m",			0x10000, 0x86e41198, 3 | BRF_GRA },           //  3 Characters

	{ "6.5a",		  	0x20000, 0xb15759f7, 4 | BRF_GRA },           //  4 Foreground & Background Tiles
	{ "7.5b",		  	0x20000, 0x2541d445, 4 | BRF_GRA },           //  5
	{ "8.6a",		  	0x20000, 0x51a2f5c4, 4 | BRF_GRA },           //  6
	{ "9.6b",		  	0x20000, 0xf4c8850f, 4 | BRF_GRA },           //  7
	{ "10.7a",			0x20000, 0x9e4b3c61, 4 | BRF_GRA },           //  8
	{ "11.7b",			0x20000, 0x0d33d083, 4 | BRF_GRA },           //  9
	{ "12.8a",			0x20000, 0x6c497ad3, 4 | BRF_GRA },           // 10
	{ "13.8b",			0x20000, 0x7a84adff, 4 | BRF_GRA },           // 11

	{ "21.8r",			0x20000, 0x0cee8711, 6 | BRF_GRA },           // 18 Sprites
	{ "20.8q",			0x20000, 0x9647e02a, 6 | BRF_GRA },           // 19
	{ "19.7r",			0x20000, 0x5082ceff, 6 | BRF_GRA },           // 16
	{ "18.7q",			0x20000, 0xd18a7ffb, 6 | BRF_GRA },           // 17
	{ "17.6r",			0x20000, 0x9088af09, 6 | BRF_GRA },           // 14
	{ "16.6q",			0x20000, 0x8ade4e01, 6 | BRF_GRA },           // 15
	{ "15.5r",			0x20000, 0x7429371c, 6 | BRF_GRA },           // 12
	{ "14.5q",			0x20000, 0x4301b97f, 6 | BRF_GRA },           // 13

	{ "3.3q",		  	0x20000, 0x489c6d0e, 7 | BRF_SND },           // 20 MSM6295 Samples
	{ "2.3r",		  	0x20000, 0x7710ce39, 7 | BRF_SND },           // 21
};

STD_ROM_PICK(drgnbowla)
STD_ROM_FN(drgnbowla)

static INT32 drgnbowlaInit()
{
	game = 3;

	INT32 nRet = DrvInit();

	MSM6295SetRoute(0, 0.50, BURN_SND_ROUTE_BOTH);

	return nRet;
}

struct BurnDriver BurnDrvDrgnbowla = {
	"drgnbowla", "drgnbowl", NULL, NULL, "1992",
	"Dragon Bowl (set 2, unencrypted program)\0", NULL, "Nics", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, drgnbowlaRomInfo, drgnbowlaRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrgnbowlDIPInfo,
	drgnbowlaInit, DrvExit, DrvFrame, DrgnbowlDraw, DrvScan, &DrvRecalc, 0x1000,
	256, 224, 4, 3
};
