// Crazy Climber FBA Driver
// Based on MAME driver by Nicola Salmoria

// Todo:
// 1: hook up samples
// 2: fix Crazy Kong pt. II offsets and bigsprite flipping issues

#include "tiles_generic.h"
#include "driver.h"
#include "z80_intf.h"
extern "C" {
#include "ay8910.h"
}

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvZ80OPS;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvColPROM;
static UINT8 *DrvSndROM;
static UINT8 *DrvUser1;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvZ80RAM2;
static UINT8 *DrvZ80RAM1_0;
static UINT8 *DrvBGSprRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvColRAM;
static UINT32 *DrvPalette;
static UINT8 DrvRecalc;
static INT16 *pAY8910Buffer[6];

static INT32 DrvGfxROM0Len;
static INT32 DrvGfxROM1Len;

static INT32 flipscreen[2];
static INT32 interrupt_enable;
static UINT8 yamato_p0;
static UINT8 yamato_p1;
static UINT8 swimmer_background_color;
static UINT8 swimmer_sidebg;
static UINT8 swimmer_palettebank;
static UINT8 soundlatch;

static UINT8 DrvReset;
static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvInputs[4];
static UINT8 DrvDips[2];

// per-game constants
static INT32 game_select;
static INT32 silvland = 0;
static INT32 gfx0_cont800 = 0;
static INT32 uses_sub;
static UINT8 bigsprite_index;

static struct BurnInputInfo CclimberInputList[] = {
	{"P1 Coin"           , BIT_DIGITAL  , DrvJoy3 + 0, "p1 coin"  },
	{"P1 Start"          , BIT_DIGITAL  , DrvJoy3 + 2, "p1 start" },

	{"P1 Up (left)"      , BIT_DIGITAL  , DrvJoy1 + 0, "p1 up"    },
	{"P1 Down (left)"    , BIT_DIGITAL  , DrvJoy1 + 1, "p1 down"  },
	{"P1 Left (left)"    , BIT_DIGITAL  , DrvJoy1 + 2, "p1 left"  },
	{"P1 Right (left)"   , BIT_DIGITAL  , DrvJoy1 + 3, "p1 right" },
	{"P1 Up (right)"     , BIT_DIGITAL  , DrvJoy1 + 4, "p3 up"    },
	{"P1 Down (right)"   , BIT_DIGITAL  , DrvJoy1 + 5, "p3 down"  },
	{"P1 Left (right)"   , BIT_DIGITAL  , DrvJoy1 + 6, "p3 left"  },
	{"P1 Right (right)"  , BIT_DIGITAL  , DrvJoy1 + 7, "p3 right" },

	{"P2 Coin"           , BIT_DIGITAL  , DrvJoy3 + 1, "p2 coin"  },
	{"P2 Start"          , BIT_DIGITAL  , DrvJoy3 + 3, "p2 start" },

	{"P2 Up (left)"      , BIT_DIGITAL  , DrvJoy2 + 0, "p2 up"    },
	{"P2 Down (left)"    , BIT_DIGITAL  , DrvJoy2 + 1, "p2 down"  },
	{"P2 Left (left)"    , BIT_DIGITAL  , DrvJoy2 + 2, "p2 left"  },
	{"P2 Right (left)"   , BIT_DIGITAL  , DrvJoy2 + 3, "p2 right" },
	{"P2 Up (right)"     , BIT_DIGITAL  , DrvJoy2 + 4, "p4 up"    },
	{"P2 Down (right)"   , BIT_DIGITAL  , DrvJoy2 + 5, "p4 down"  },
	{"P2 Left (right)"   , BIT_DIGITAL  , DrvJoy2 + 6, "p4 left"  },
	{"P2 Right (right)"  , BIT_DIGITAL  , DrvJoy2 + 7, "p4 right" },

	{"Reset"             , BIT_DIGITAL  , &DrvReset    , "reset"    },
	{"Dip"               , BIT_DIPSWITCH, DrvDips + 0  , "dip"      },
};

STDINPUTINFO(Cclimber)

static struct BurnDIPInfo CclimberDIPList[]=
{
	{0x15, 0xff, 0xff, 0x00, NULL		},
	{0x16, 0xff, 0xff, 0xf0, NULL		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x15, 0x01, 0x03, 0x00, "3"		},
	{0x15, 0x01, 0x03, 0x01, "4"		},
	{0x15, 0x01, 0x03, 0x02, "5"		},
	{0x15, 0x01, 0x03, 0x03, "6"		},

	{0   , 0xfe, 0   ,    2, "Rack Test (Cheat)"		},
	{0x15, 0x01, 0x08, 0x00, "Off"		},
	{0x15, 0x01, 0x08, 0x08, "On"		},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x15, 0x01, 0x30, 0x30, "4 Coins 1 Credits"		},
	{0x15, 0x01, 0x30, 0x20, "3 Coins 1 Credits"		},
	{0x15, 0x01, 0x30, 0x10, "2 Coins 1 Credits"		},
	{0x15, 0x01, 0x30, 0x00, "1 Coin  1 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x15, 0x01, 0xc0, 0x00, "1 Coin  1 Credits"		},
	{0x15, 0x01, 0xc0, 0x40, "1 Coin  2 Credits"		},
	{0x15, 0x01, 0xc0, 0x80, "1 Coin  3 Credits"		},
	{0x15, 0x01, 0xc0, 0xc0, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x16, 0x01, 0x10, 0x10, "Upright"		},
	{0x16, 0x01, 0x10, 0x00, "Cocktail"		},
};

STDDIPINFO(Cclimber)

static struct BurnInputInfo RpatrolInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 1"},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p2 fire 1"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
};

STDINPUTINFO(Rpatrol)

static struct BurnDIPInfo RpatrolDIPList[]=
{
	{0x0a, 0xff, 0xff, 0x90, NULL		},

	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{0x0a, 0x01, 0x03, 0x02, "2 Coins 1 Credits"		},
	{0x0a, 0x01, 0x03, 0x00, "1 Coin  1 Credits"		},
	{0x0a, 0x01, 0x03, 0x01, "1 Coin  2 Credits"		},
	{0x0a, 0x01, 0x03, 0x03, "Free Play"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0a, 0x01, 0x0c, 0x00, "3"		},
	{0x0a, 0x01, 0x0c, 0x04, "4"		},
	{0x0a, 0x01, 0x0c, 0x08, "5"		},
	{0x0a, 0x01, 0x0c, 0x0c, "6"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0a, 0x01, 0x10, 0x10, "Upright"		},
	{0x0a, 0x01, 0x10, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Unknown 1"		},
	{0x0a, 0x01, 0x20, 0x00, "Off"		},
	{0x0a, 0x01, 0x20, 0x20, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown 2"		},
	{0x0a, 0x01, 0x40, 0x00, "Off"		},
	{0x0a, 0x01, 0x40, 0x40, "On"		},

	{0   , 0xfe, 0   ,    2, "Memory Test"		},
	{0x0a, 0x01, 0x80, 0x00, "Retry on Error"		},
	{0x0a, 0x01, 0x80, 0x80, "Stop on Error"		},
};

STDDIPINFO(Rpatrol)

static struct BurnInputInfo CkongInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 1"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 1"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
};

STDINPUTINFO(Ckong)


static struct BurnDIPInfo CkongDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x80, NULL		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0f, 0x01, 0x03, 0x00, "3"		},
	{0x0f, 0x01, 0x03, 0x01, "4"		},
	{0x0f, 0x01, 0x03, 0x02, "5"		},
	{0x0f, 0x01, 0x03, 0x03, "6"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x0f, 0x01, 0x0c, 0x00, "7000"		},
	{0x0f, 0x01, 0x0c, 0x04, "10000"		},
	{0x0f, 0x01, 0x0c, 0x08, "15000"		},
	{0x0f, 0x01, 0x0c, 0x0c, "20000"		},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x0f, 0x01, 0x70, 0x70, "5 Coins 1 Credits"		},
	{0x0f, 0x01, 0x70, 0x50, "4 Coins 1 Credits"		},
	{0x0f, 0x01, 0x70, 0x30, "3 Coins 1 Credits"		},
	{0x0f, 0x01, 0x70, 0x10, "2 Coins 1 Credits"		},
	{0x0f, 0x01, 0x70, 0x00, "1 Coin  1 Credits"		},
	{0x0f, 0x01, 0x70, 0x20, "1 Coin  2 Credits"		},
	{0x0f, 0x01, 0x70, 0x40, "1 Coin  3 Credits"		},
	{0x0f, 0x01, 0x70, 0x60, "1 Coin  4 Credits"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0f, 0x01, 0x80, 0x80, "Upright"		},
	{0x0f, 0x01, 0x80, 0x00, "Cocktail"		},
};

STDDIPINFO(Ckong)

static struct BurnInputInfo GuzzlerInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"},
	{"P1 Start",	BIT_DIGITAL,	DrvJoy4 + 2,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"},
	{"P2 Start",	BIT_DIGITAL,	DrvJoy4 + 3,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"},
	{"P2 Right",	BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"},
	{"P2 Button 1",	BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
};

STDINPUTINFO(Guzzler)


static struct BurnDIPInfo GuzzlerDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x00, NULL		},
	{0x10, 0xff, 0xff, 0x10, NULL		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0f, 0x01, 0x03, 0x00, "3"		},
	{0x0f, 0x01, 0x03, 0x01, "4"		},
	{0x0f, 0x01, 0x03, 0x02, "5"		},
	{0x0f, 0x01, 0x03, 0x03, "Infinite (Cheat)"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x0f, 0x01, 0x0c, 0x04, "20K, every 50K"		},
	{0x0f, 0x01, 0x0c, 0x00, "30K, every 100K"		},
	{0x0f, 0x01, 0x0c, 0x08, "30K only"		},
	{0x0f, 0x01, 0x0c, 0x0c, "None"		},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x0f, 0x01, 0x30, 0x10, "2 Coins 1 Credits"		},
	{0x0f, 0x01, 0x30, 0x00, "1 Coin  1 Credits"		},
	{0x0f, 0x01, 0x30, 0x20, "1 Coin  2 Credits"		},
	{0x0f, 0x01, 0x30, 0x30, "1 Coin  3 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x0f, 0x01, 0xc0, 0x00, "1 Coin  1 Credits"		},
	{0x0f, 0x01, 0xc0, 0x40, "1 Coin  2 Credits"		},
	{0x0f, 0x01, 0xc0, 0x80, "1 Coin  3 Credits"		},
	{0x0f, 0x01, 0xc0, 0xc0, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x10, 0x01, 0x10, 0x10, "Upright"		},
	{0x10, 0x01, 0x10, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "High Score Names"		},
	{0x10, 0x01, 0x20, 0x20, "3 Letters"		},
	{0x10, 0x01, 0x20, 0x00, "10 Letters"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x10, 0x01, 0xc0, 0x00, "Easy"		},
	{0x10, 0x01, 0xc0, 0x40, "Medium"		},
	{0x10, 0x01, 0xc0, 0x80, "Hard"		},
	{0x10, 0x01, 0xc0, 0xc0, "Hardest"		},
};

STDDIPINFO(Guzzler)


static struct BurnInputInfo YamatoInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy4 + 2,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 2"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy4 + 3,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 2"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
};

STDINPUTINFO(Yamato)


static struct BurnDIPInfo YamatoDIPList[]=
{
	{0x11, 0xff, 0xff, 0x80, NULL		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x11, 0x01, 0x03, 0x00, "3"		},
	{0x11, 0x01, 0x03, 0x01, "4"		},
	{0x11, 0x01, 0x03, 0x02, "5"		},
	{0x11, 0x01, 0x03, 0x03, "6"		},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x11, 0x01, 0x1c, 0x0c, "4 Coins 1 Credits"		},
	{0x11, 0x01, 0x1c, 0x08, "3 Coins 1 Credits"		},
	{0x11, 0x01, 0x1c, 0x04, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x1c, 0x00, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x1c, 0x18, "2 Coins 3 Credits"		},
	{0x11, 0x01, 0x1c, 0x10, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0x1c, 0x14, "1 Coin  3 Credits"		},
	{0x11, 0x01, 0x1c, 0x1c, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x11, 0x01, 0x20, 0x00, "Every 30000"		},
	{0x11, 0x01, 0x20, 0x20, "Every 50000"		},

	{0   , 0xfe, 0   ,    2, "Speed"		},
	{0x11, 0x01, 0x40, 0x00, "Slow"		},
	{0x11, 0x01, 0x40, 0x40, "Fast"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x11, 0x01, 0x80, 0x80, "Upright"		},
	{0x11, 0x01, 0x80, 0x00, "Cocktail"		},
};

STDDIPINFO(Yamato)

static struct BurnInputInfo SwimmerInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
};

STDINPUTINFO(Swimmer)


static struct BurnDIPInfo SwimmerDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x00, NULL		},
	{0x10, 0xff, 0xff, 0x30, NULL		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0f, 0x01, 0x03, 0x00, "3"		},
	{0x0f, 0x01, 0x03, 0x01, "4"		},
	{0x0f, 0x01, 0x03, 0x02, "5"		},
	{0x0f, 0x01, 0x03, 0x03, "Infinite (Cheat)"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x0f, 0x01, 0x0c, 0x00, "10000"		},
	{0x0f, 0x01, 0x0c, 0x04, "20000"		},
	{0x0f, 0x01, 0x0c, 0x08, "30000"		},
	{0x0f, 0x01, 0x0c, 0x0c, "None"		},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x0f, 0x01, 0x30, 0x10, "2 Coins 1 Credits"		},
	{0x0f, 0x01, 0x30, 0x00, "1 Coin  1 Credits"		},
	{0x0f, 0x01, 0x30, 0x20, "1 Coin  2 Credits"		},
	{0x0f, 0x01, 0x30, 0x30, "1 Coin  3 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x0f, 0x01, 0xc0, 0x00, "1 Coin  1 Credits"		},
	{0x0f, 0x01, 0xc0, 0x40, "1 Coin  2 Credits"		},
	{0x0f, 0x01, 0xc0, 0x80, "1 Coin  3 Credits"		},
	{0x0f, 0x01, 0xc0, 0xc0, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x10, 0x01, 0x10, 0x10, "Upright"		},
	{0x10, 0x01, 0x10, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x10, 0x01, 0x20, 0x00, "Off"		},
	{0x10, 0x01, 0x20, 0x20, "On"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x10, 0x01, 0xc0, 0x00, "Easy"		},
	{0x10, 0x01, 0xc0, 0x40, "Hard"		},
	{0x10, 0x01, 0xc0, 0x80, "Harder"		},
	{0x10, 0x01, 0xc0, 0xc0, "Hardest"		},
};

STDDIPINFO(Swimmer)

static void __fastcall cclimber_write(UINT16 address, UINT8 data)
{
	if (address >= 0x9c00 && address <= 0x9fff) {
		INT32 offset = (address - 0x9c00) & 0x0fdf;
		DrvColRAM[offset] = data;
		DrvColRAM[offset + 0x20] = data;
		return;
	}

	switch (address)
	{
		case 0xa000:
			interrupt_enable = data;
		return;

		case 0xa001:
		case 0xa002:
			flipscreen[address & 1] = data & 1;
		return;

		case 0xa003:
			// cclimber_sample_trigger_w
			if (game_select == 6) {
				swimmer_sidebg = data;
			}
		return;

		case 0xa004:
			if (game_select == 6) {
				swimmer_palettebank = data;
			}
		return;

		case 0xa800:
			// cclimber_sample_rate_W
			if (game_select == 6) {
				soundlatch = data;
				ZetClose();
				ZetOpen(1);
				ZetSetVector(0xff);
				ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
				ZetClose();
				ZetOpen(0);
			}
		return;

		case 0xb000:
			// cclimber_sample_volume_w
		return;

		case 0xb800:
			if (game_select == 6) {
				swimmer_background_color = data;
			}
		return;
	}

	return;
}

static UINT8 __fastcall cclimber_read(UINT16 address)
{
	if (game_select == 6) { // swimmer hack for lazy.
		switch (address)
		{
			case 0xa000:
			    return DrvInputs[1];
			case 0xa800:
				return DrvInputs[0];
			case 0xb000:
				return DrvDips[0];
			case 0xb800:
				return DrvDips[1] | DrvInputs[2];
			case 0xb880:
				return DrvInputs[3];
		}

		return 0;
	}

	switch (address)
	{
		case 0xa000:
			return DrvInputs[0];
		case 0xa800:
			return DrvInputs[1];
		case 0xb000:
			return DrvDips[0];
		case 0xb800:
			return DrvInputs[2];
		case 0xba00:
			return DrvInputs[3];

	}

	return 0;
}

static UINT8 __fastcall swimmer_sub_read(UINT16 address)
{
	switch (address)
	{
		case 0x3000:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			return soundlatch;
	}

	return 0;
}

static void __fastcall cclimber_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x08:
		case 0x09:
			if (game_select != 5) AY8910Write(0, port & 1, data);
		return;
		case 0x00:
			yamato_p0 = data;
		return;
		case 0x01:
			yamato_p1 = data;
		return;
	}

	return;
}

static UINT8 __fastcall cclimber_in(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x0c:
			return AY8910Read(0);
	}

	return 0;
}

static void __fastcall sub_out(UINT16 port, UINT8 data)
{
	port &= 0xff;

	if (game_select == 6) { // swimmer / guzzler
		switch (port)
		{
			case 0x00:
		    case 0x01:
		    case 0x80:
		    case 0x81:
				AY8910Write((port)>>7, ~port & 1, data);
			return;
		}

		return;
	}

	switch (port)
	{
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
			AY8910Write((port&2)>>1, port & 1, data);
		return;
	}

	return;
}

static UINT8 __fastcall sub_in(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x04:
			return yamato_p0;
		case 0x08:
			return yamato_p1;
	}

	return 0;
}


static INT32 DrvDoReset()
{
	DrvReset = 0;

	memset(AllRam, 0, RamEnd - AllRam);

	flipscreen[0] = flipscreen[1] = 0;
	interrupt_enable = 0;

	bigsprite_index = (game_select == 6) ? 0xfc : 0xdc;

	yamato_p0 = yamato_p1 = 0;
	swimmer_background_color = swimmer_sidebg = swimmer_palettebank = soundlatch = 0;

	ZetOpen(0);
	ZetReset();
	ZetClose();

	if (uses_sub) {
		ZetOpen(1);
		ZetReset();
		ZetClose();
	}

	AY8910Reset(0);
	AY8910Reset(1);

	return 0;
}

static void cclimber_sample_select_w(UINT32, UINT32)
{
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x0010000;
   	DrvZ80OPS		= Next; Next += 0x0010000;

	DrvGfxROM0		= Next; Next += 0x0060000;
	DrvGfxROM1		= Next; Next += 0x0060000;
	DrvGfxROM2		= Next; Next += 0x0060000;

	DrvColPROM		= Next; Next += 0x0000300;

	DrvSndROM		= Next; Next += 0x0012000;
	DrvUser1		= Next; Next += 0x0010000;

	DrvPalette		= (UINT32*)Next; Next += 0x0200 * sizeof(UINT32);

	pAY8910Buffer[0]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[1]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[2]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[3]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[4]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[5]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);

	AllRam			= Next;

	DrvZ80RAM0		= Next; Next += 0x0000c00;
	DrvZ80RAM1		= Next; Next += 0x0000800;
	DrvZ80RAM2		= Next; Next += 0x0000800;
	DrvZ80RAM1_0    = Next; Next += 0x0001000;
	DrvBGSprRAM		= Next; Next += 0x0000100;
	DrvSprRAM		= Next; Next += 0x0000400;
	DrvColRAM		= Next; Next += 0x0000400;
	DrvVidRAM		= Next; Next += 0x0000400;

	RamEnd			= Next;
	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode(UINT8 *gfx_base, UINT8 *gfx_dest, INT32 len, INT32 size)
{
	INT32 Plane[2] = { 0, (len / 2) * 8 };
	INT32 PlaneSwimmer[3] = { 0, RGN_FRAC(len, 1, 3), RGN_FRAC(len, 2, 3) };
	INT32 XOffs[16] = {  0, 1, 2, 3, 4, 5, 6, 7,
			8*8+0, 8*8+1, 8*8+2, 8*8+3, 8*8+4, 8*8+5, 8*8+6, 8*8+7 };
	INT32 YOffs[16] = {  0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			16*8, 17*8, 18*8, 19*8, 20*8, 21*8, 22*8, 23*8 };

	UINT8 *tmp = (UINT8*)malloc(len);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, gfx_base, len);
	if (game_select == 6) { // swimmer, guzzler
		GfxDecode(((len * 8) / 3) / (size * size), 3, size, size, PlaneSwimmer, XOffs, YOffs, (size * size), tmp, gfx_dest);
	} else {
		GfxDecode(((len * 8) / 2) / (size * size), 2, size, size, Plane, XOffs, YOffs, (size * size), tmp, gfx_dest);
	}

	free (tmp);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0;i < 96; i++)
	{
		INT32 bit0, bit1, bit2;
		INT32 r, g, b;

		bit0 = (DrvColPROM[i] >> 0) & 0x01;
		bit1 = (DrvColPROM[i] >> 1) & 0x01;
		bit2 = (DrvColPROM[i] >> 2) & 0x01;
		r = bit0 * 33 + bit1 * 71 + bit2 * 151;
		r = (int)(r + 0.5);

		bit0 = (DrvColPROM[i] >> 3) & 0x01;
		bit1 = (DrvColPROM[i] >> 4) & 0x01;
		bit2 = (DrvColPROM[i] >> 5) & 0x01;
		g = bit0 * 33 + bit1 * 71 + bit2 * 151;
		g = (int)(g + 0.5);

		bit0 = (DrvColPROM[i] >> 6) & 0x01;
		bit1 = (DrvColPROM[i] >> 7) & 0x01;
		b = bit0 * 71 + bit1 * 151;
		b = (int)(b + 0.5);

		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}
	if (silvland) {
		bprintf(0, _T("silvlandpalette"));
		DrvPalette[0x42] = BurnHighCol(0xff, 0xce, 0xce, 0);
	}
}

static void YamatoPaletteInit()
{
	const UINT8 *color_prom = DrvColPROM;
	int i;

	/* chars - 12 bits RGB */
	for (i = 0; i < 0x40; i++)
	{
		int bit0, bit1, bit2, bit3;
		int r, g, b;

		/* red component */
		bit0 = (color_prom[i + 0x00] >> 0) & 0x01;
		bit1 = (color_prom[i + 0x00] >> 1) & 0x01;
		bit2 = (color_prom[i + 0x00] >> 2) & 0x01;
		bit3 = (color_prom[i + 0x00] >> 3) & 0x01;
		r = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		/* green component */
		bit0 = (color_prom[i + 0x00] >> 4) & 0x01;
		bit1 = (color_prom[i + 0x00] >> 5) & 0x01;
		bit2 = (color_prom[i + 0x00] >> 6) & 0x01;
		bit3 = (color_prom[i + 0x00] >> 7) & 0x01;
		g = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		/* blue component */
		bit0 = (color_prom[i + 0x40] >> 0) & 0x01;
		bit1 = (color_prom[i + 0x40] >> 1) & 0x01;
		bit2 = (color_prom[i + 0x40] >> 2) & 0x01;
		bit3 = (color_prom[i + 0x40] >> 3) & 0x01;
		b = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}

	/* big sprite - 8 bits RGB */
	for (i = 0; i < 0x20; i++)
	{
		int bit0, bit1, bit2;
		int r, g, b;

		/* red component */
		bit0 = (color_prom[i + 0x80] >> 0) & 0x01;
		bit1 = (color_prom[i + 0x80] >> 1) & 0x01;
		bit2 = (color_prom[i + 0x80] >> 2) & 0x01;
		r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		/* green component */
		bit0 = (color_prom[i + 0x80] >> 3) & 0x01;
		bit1 = (color_prom[i + 0x80] >> 4) & 0x01;
		bit2 = (color_prom[i + 0x80] >> 5) & 0x01;
		g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		/* blue component */
		bit0 = 0;
		bit1 = (color_prom[i + 0x80] >> 6) & 0x01;
		bit2 = (color_prom[i + 0x80] >> 7) & 0x01;
		b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		DrvPalette[i + 0x40] = BurnHighCol(r, g, b, 0);
	}

	/* fake colors for bg gradient */
	for (i = 0; i < 0x100; i++)
		DrvPalette[i + 0x60] = BurnHighCol(0, 0, i, 0);
}

void swimmer_set_background_pen()
{
	int bit0, bit1, bit2;
	int r, g, b;

	/* red component */
	bit0 = 0;
	bit1 = (swimmer_background_color >> 6) & 0x01;
	bit2 = (swimmer_background_color >> 7) & 0x01;
	r = 0x20 * bit0 + 0x40 * bit1 + 0x80 * bit2;

	/* green component */
	bit0 = (swimmer_background_color >> 3) & 0x01;
	bit1 = (swimmer_background_color >> 4) & 0x01;
	bit2 = (swimmer_background_color >> 5) & 0x01;
	g = 0x20 * bit0 + 0x40 * bit1 + 0x80 * bit2;

	/* blue component */
	bit0 = (swimmer_background_color >> 0) & 0x01;
	bit1 = (swimmer_background_color >> 1) & 0x01;
	bit2 = (swimmer_background_color >> 2) & 0x01;
	b = 0x20 * bit0 + 0x40 * bit1 + 0x80 * bit2;

	DrvPalette[0x00] = BurnHighCol(r, g, b, 0);
}

static void SwimmerPaletteInit()
{
	const UINT8 *color_prom = DrvColPROM;
	int i;

	for (i = 0; i < 0x100; i++)
	{
		int bit0, bit1, bit2;
		int r, g, b;

		/* red component */
		bit0 = (color_prom[i + 0x000] >> 0) & 0x01;
		bit1 = (color_prom[i + 0x000] >> 1) & 0x01;
		bit2 = (color_prom[i + 0x000] >> 2) & 0x01;
		r = 0x20 * bit0 + 0x40 * bit1 + 0x80 * bit2;

		/* green component */
		bit0 = (color_prom[i + 0x000] >> 3) & 0x01;
		bit1 = (color_prom[i + 0x100] >> 0) & 0x01;
		bit2 = (color_prom[i + 0x100] >> 1) & 0x01;
		g = 0x20 * bit0 + 0x40 * bit1 + 0x80 * bit2;

		/* blue component */
		bit0 = 0;
		bit1 = (color_prom[i + 0x100] >> 2) & 0x01;
		bit2 = (color_prom[i + 0x100] >> 3) & 0x01;
		b = 0x20 * bit0 + 0x40 * bit1 + 0x80 * bit2;

		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}

	color_prom += 0x200;

	/* big sprite */
	for (i = 0; i < 0x20; i++)
	{
		int bit0, bit1, bit2;
		int r, g, b;

		/* red component */
		bit0 = (color_prom[i] >> 0) & 0x01;
		bit1 = (color_prom[i] >> 1) & 0x01;
		bit2 = (color_prom[i] >> 2) & 0x01;
		r = 0x20 * bit0 + 0x40 * bit1 + 0x80 * bit2;

		/* green component */
		bit0 = (color_prom[i] >> 3) & 0x01;
		bit1 = (color_prom[i] >> 4) & 0x01;
		bit2 = (color_prom[i] >> 5) & 0x01;
		g = 0x20 * bit0 + 0x40 * bit1 + 0x80 * bit2;

		/* blue component */
		bit0 = 0;
		bit1 = (color_prom[i] >> 6) & 0x01;
		bit2 = (color_prom[i] >> 7) & 0x01;
		b = 0x20 * bit0 + 0x40 * bit1 + 0x80 * bit2;

		DrvPalette[i + 0x100] = BurnHighCol(r, g, b, 0);
	}

	/* side panel backgrond pen */
	DrvPalette[0x120] = BurnHighCol(0x20, 0x98, 0x79, 0);

	swimmer_set_background_pen();
}


static void cclimber_decode(const INT8 convtable[8][16])
{
	UINT8 *rom = DrvZ80ROM;
	UINT8 *decrypt = DrvZ80OPS;
	INT32 A;

	ZetOpen(0);
	ZetMapArea(0x0000, 0x5fff, 2, DrvZ80OPS, DrvZ80ROM);
	ZetClose();

	for (A = 0x0000;A < 0x10000;A++)
	{
		INT32 i,j;
		UINT8 src;

		src = rom[A];

		/* pick the translation table from bit 0 of the address */
		/* and from bits 1 7 of the source data */
		i = (A & 1) | (src & 0x02) | ((src & 0x80) >> 5);

		/* pick the offset in the table from bits 0 2 4 6 of the source data */
		j = (src & 0x01) | ((src & 0x04) >> 1) | ((src & 0x10) >> 2) | ((src & 0x40) >> 3);

		/* decode the opcodes */
		decrypt[A] = (src & 0xaa) | convtable[i][j];
	}
}


static INT32 GetRoms()
{
	char* pRomName;
	struct BurnRomInfo ri;
	UINT8 *Load0 = DrvZ80ROM;
	UINT8 *Loadg0 = DrvGfxROM0;
	UINT8 *Loadg1 = DrvGfxROM2;
	UINT8 *Loadc = DrvColPROM;
	UINT8 *Loads = DrvSndROM;
	UINT8 *LoadU = DrvUser1;
	DrvGfxROM0Len = 0;
	DrvGfxROM1Len = 0;

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {

		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & 7) == 1) {
			if (BurnLoadRom(Load0, i, 1)) return 1;
			Load0 += ri.nLen;

			continue;
		}

		if ((ri.nType & 7) == 2) {
			if (BurnLoadRom(Loadg0, i, 1)) return 1;
			if (gfx0_cont800) { // loads 0x800 bytes at 0x0000 and 0x1000
				UINT8 *tmp = (UINT8*)BurnMalloc(0x4000);
				memmove(tmp, Loadg0, 0x1000);
				memset(Loadg0, 0, 0x1000);
				memmove(Loadg0 + 0x00000, tmp + 0x00000, 0x800);
				memmove(Loadg0 + 0x01000, tmp + 0x00800, 0x800);
				BurnFree(tmp);
				Loadg0 += 0x2000;
				DrvGfxROM0Len += 0x2000;
			} else {
				Loadg0 += (game_select == 1) ? 0x1000 : ri.nLen;
				DrvGfxROM0Len += (game_select == 1) ? 0x1000 : ri.nLen;
			}

			continue;
		}

		if ((ri.nType & 7) == 3) {
			if (BurnLoadRom(Loadg1, i, 1)) return 1;
			Loadg1 += ri.nLen;
			DrvGfxROM1Len += ri.nLen;

			continue;
		}

		if ((ri.nType & 7) == 4) {
			if (BurnLoadRom(LoadU, i, 1)) return 1;
			LoadU += ri.nLen;

			continue;
		}

		if ((ri.nType & 7) == 6) {
			if (BurnLoadRom(Loadc, i, 1)) return 1;
			Loadc += ri.nLen;

			continue;
		}

		if ((ri.nType & 7) == 7) {
			if (BurnLoadRom(Loads, i, 1)) return 1;
			Loads += ri.nLen;

			continue;
		}
	}

	return 0;
}

static INT32 DrvInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)malloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (GetRoms()) return 1;

		DrvGfxDecode(DrvGfxROM0, DrvGfxROM1, DrvGfxROM0Len, 16);
		DrvGfxDecode(DrvGfxROM0, DrvGfxROM0, DrvGfxROM0Len,  8);
		DrvGfxDecode(DrvGfxROM2, DrvGfxROM2, DrvGfxROM1Len,  8);

		DrvPaletteInit();
	}

	ZetInit(0);
	ZetOpen(0);

	if (game_select == 6) { // swimmer, guzzler
		ZetMapMemory(DrvZ80ROM,		    0x0000, 0x7fff, MAP_ROM);
		ZetMapMemory(DrvZ80ROM + 0x8000,0xe000, 0xffff, MAP_ROM);
		ZetMapMemory(DrvZ80RAM0,		0x8000, 0x87ff, MAP_RAM);
		ZetMapMemory(DrvZ80RAM1,		0xc000, 0xc7ff, MAP_RAM);
		ZetMapMemory(DrvBGSprRAM,		0x8800, 0x88ff, MAP_RAM);
		ZetMapMemory(DrvBGSprRAM,		0x8900, 0x89ff, MAP_RAM); // mirror
	} else {
		ZetMapMemory(DrvZ80ROM,		    0x0000, 0x5fff, MAP_ROM);
		ZetMapMemory(DrvZ80RAM0,		0x6000, 0x6bff, MAP_RAM);
		ZetMapMemory(DrvZ80RAM1,		0x8000, 0x83ff, MAP_RAM);
		ZetMapMemory(DrvBGSprRAM,		0x8800, 0x88ff, MAP_RAM);
		ZetMapMemory(DrvZ80RAM2,		0x8900, 0x8bff, MAP_RAM);
	}
	ZetMapMemory(DrvVidRAM,		    0x9000, 0x93ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,		    0x9400, 0x97ff, MAP_RAM); // mirror
	ZetMapMemory(DrvSprRAM,		    0x9800, 0x9bff, MAP_RAM);
	ZetMapMemory(DrvColRAM,		    0x9c00, 0x9fff, MAP_READ); // special write in handler

	ZetSetWriteHandler(cclimber_write);
	ZetSetReadHandler(cclimber_read);
	ZetSetOutHandler(cclimber_out);
	ZetSetInHandler(cclimber_in);
	ZetClose();

	if (uses_sub) {
		ZetInit(1);
		ZetOpen(1);
		if (game_select == 5) { // yamato
			ZetMapMemory(DrvSndROM,		    0x0000, 0x07ff, MAP_ROM);
			ZetMapMemory(DrvZ80RAM1_0,		0x5000, 0x53ff, MAP_RAM);
			ZetSetOutHandler(sub_out);
			ZetSetInHandler(sub_in);
		}
		if (game_select == 6) { // swimmer/guzzler
			ZetMapMemory(DrvSndROM,		    0x0000, 0x0fff, MAP_ROM);
			ZetMapMemory(DrvZ80RAM1_0,		0x2000, 0x23ff, MAP_RAM);
			ZetMapMemory(DrvSndROM + 0x1000,0x4000, 0xffff, MAP_RAM);
			ZetSetReadHandler(swimmer_sub_read);
			ZetSetOutHandler(sub_out);
		}
		ZetClose();
	}

	AY8910Init(0, (game_select == 6) ? 2000000 : 1536000, nBurnSoundRate, NULL, NULL, &cclimber_sample_select_w, NULL);
	AY8910SetAllRoutes(0, 0.15, BURN_SND_ROUTE_BOTH);
	AY8910Init(1, (game_select == 6) ? 2000000 : 1536000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(1, 0.15, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	AY8910Exit(0);
	AY8910Exit(1);

	free (AllMem);
	AllMem = NULL;

	game_select = 0;
	uses_sub = 0;
	silvland = 0;
	gfx0_cont800 = 0;

	return 0;
}

static void cclimber_draw_bigsprite()
{
	UINT8 x = 136 - DrvSprRAM[bigsprite_index + 3];
	UINT8 y = 128 - DrvSprRAM[bigsprite_index + 2];
	INT32 flipx = (DrvSprRAM[bigsprite_index + 1] & 0x10) >> 4;
	INT32 flipy = (DrvSprRAM[bigsprite_index + 1] & 0x20) >> 5;
	INT32 bits = (game_select == 6) ? 3 : 2;
	INT32 palindex = (game_select == 6) ? 0x100 : 0x40;

	if (flipscreen[0]) { // flipx
		flipx = !flipx;
	}

	if (flipscreen[1]) { // flipy
		y = 128 - y;
		flipy = !flipy;
	}

	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		if ((offs & 0x210) != 0x210) continue; // everything else is transparent, apparently

		INT32 ofst = ((offs & 0x1e0) >> 1) | (offs & 0x0f);

		INT32 sx = ofst % 0x10;
		INT32 sy = ofst / 0x10;
		if (flipx) sx = 15 - sx;
		if (flipy) sy = 15 - sy;

		sx = (x+8*sx) & 0xff;
		sy = (y+8*sy) & 0xff;

		sy -= 16; //offsets

		INT32 code =  ((DrvSprRAM[bigsprite_index + 1] & 0x08) << 5) | DrvBGSprRAM[ofst];
		code &= 0x3ff;
		INT32 color = DrvSprRAM[bigsprite_index + 1] & 0x07;
		color &= 0x1ff;

		if (flipy) {
			if (flipx) {
				Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, bits, 0, palindex, DrvGfxROM2);
				Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, code, sx-256, sy, color, bits, 0, palindex, DrvGfxROM2);
			} else {
				Render8x8Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, bits, 0, palindex, DrvGfxROM2);
				Render8x8Tile_Mask_FlipY_Clip(pTransDraw, code, sx-256, sy, color, bits, 0, palindex, DrvGfxROM2);
			}
		} else {
			if (flipx) {
				Render8x8Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, bits, 0, palindex, DrvGfxROM2);
				Render8x8Tile_Mask_FlipX_Clip(pTransDraw, code, sx-256, sy, color, bits, 0, palindex, DrvGfxROM2);
			} else {
				Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy, color, bits, 0, palindex, DrvGfxROM2);
				Render8x8Tile_Mask_Clip(pTransDraw, code, sx-256, sy, color, bits, 0, palindex, DrvGfxROM2);
			}
		}

	}
}

static void draw_playfield()
{
	INT32 bits = (game_select == 6) ? 3 : 2;

	for (INT32 offs = 0; offs < 0x400; offs++)
	{
		INT32 sx = (offs & 0x1f) << 3;
		INT32 sy = (offs >> 5) << 3;

		sy -= DrvSprRAM[sx >> 3]; // col scroll
		sy -= 16; //offsets
		if (sy < -7) sy += 256;
		if (sx < -7) sx += 256;

		INT32 flipy = (DrvColRAM[offs] & 0x80) >> 7;
		INT32 flipx = (DrvColRAM[offs] & 0x40) >> 6;

		INT32 tile_offs = offs ^ (flipy << 5);
		
		if (flipscreen[0]) { // flipx
			sx = 248 - sx;
			flipx ^= 1;
		}

		if (flipscreen[1]) { // flipy
			sy = 248 - sy;
			flipy ^= 1;
		}

		INT32 code = ((DrvColRAM[tile_offs] & 0x10) << 5) + ((DrvColRAM[tile_offs] & 0x20) << 3) + DrvVidRAM[tile_offs];
		INT32 color = DrvColRAM[tile_offs] & 0x0f;

		if (game_select == 6) {
			code = ((DrvColRAM[tile_offs] & 0x10) << 4) | DrvVidRAM[tile_offs];
			color = ((swimmer_palettebank & 0x01) << 4) | (DrvColRAM[tile_offs] & 0x0f);
		}

		if (sx > nScreenWidth || sy > nScreenHeight) continue;

		if (flipy) {
			if (flipx) {
				Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, bits, 0, 0, DrvGfxROM0);
			} else {
				Render8x8Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, bits, 0, 0, DrvGfxROM0);
			}
		} else {
			if (flipx) {
				Render8x8Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, bits, 0, 0, DrvGfxROM0);
			} else {
				Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy, color, bits, 0, 0, DrvGfxROM0);
			}
		}
	}
}

#if 0
extern int counter;
static void draw_debug()
{
	INT32 code = 0;
	INT32 color = counter;
	for (INT32 sy = 0; sy<31; sy++)
		for (INT32 sx = 0; sx<31; sx++)	{
			Render8x8Tile_Mask_Clip(pTransDraw, code, sx*8, sy*8, color, 2, 0, 0, DrvGfxROM0);
			code++;
		}
}
#endif

static void draw_sprites()
{
	INT32 bits = (game_select == 6) ? 3 : 2;

	for (INT32 offs = 0x9c; offs >= 0x80; offs -= 4)
	{
		INT32 x = DrvSprRAM[offs + 3];
		INT32 y = 240 - DrvSprRAM[offs + 2];
		y -= 16; //offsets

		INT32 code = ((DrvSprRAM[offs + 1] & 0x10) << 3) |
				   ((DrvSprRAM[offs + 1] & 0x20) << 1) |
				   (DrvSprRAM[offs + 0] & 0x3f);

		INT32 color = DrvSprRAM[offs + 1] & 0x0f;

		if (game_select == 6) {
			code = ((DrvSprRAM[offs + 1] & 0x10) << 2) |
					(DrvSprRAM[offs + 0] & 0x3f);

			color = ((swimmer_palettebank & 0x01) << 4) |
					(DrvSprRAM[offs + 1] & 0x0f);

		}

		INT32 flipx = DrvSprRAM[offs + 0] & 0x40;
		INT32 flipy = DrvSprRAM[offs + 0] & 0x80;

		if (flipscreen[0] & 1)
		{
			x = 240 - x;
			flipx = !flipx;
		}

		if (flipscreen[1] & 1)
		{
			y = 240 - y;
			flipy = !flipy;
		}

		if (flipy) {
			if (flipx) {
				Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, x, y, color, bits, 0, 0, DrvGfxROM1);
			} else {
				Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, x, y, color, bits, 0, 0, DrvGfxROM1);
			}
		} else {
			if (flipx) {
				Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, x, y, color, bits, 0, 0, DrvGfxROM1);
			} else {
				Render16x16Tile_Mask_Clip(pTransDraw, code, x, y, color, bits, 0, 0, DrvGfxROM1);
			}
		}
	}
}

#define SWIMMER_SIDE_BG_PEN (0x120)
#define SWIMMER_BG_PEN      (0x00)
#define SWIMMER_BG_SPLIT    (0x18 * 8)

void swimmer_draw_backdrop() // background effects for swimmer/guzzler
{
	swimmer_set_background_pen();

	if (swimmer_sidebg & 1) {
		for (INT32 y = 0; y < nScreenHeight; y++)
			for (INT32 x = 0; x < nScreenWidth; x++) {
				if (flipscreen[0] & 1) {
					if (x <= 0xff - SWIMMER_BG_SPLIT) {
						pTransDraw[(y*nScreenWidth) + x] = SWIMMER_SIDE_BG_PEN;
					} else {
						pTransDraw[(y*nScreenWidth) + x] = SWIMMER_BG_PEN;
					}
				} else {
					if (x <= SWIMMER_BG_SPLIT - 1) {
						pTransDraw[(y*nScreenWidth) + x] = SWIMMER_BG_PEN;
					} else {
						pTransDraw[(y*nScreenWidth) + x] = SWIMMER_SIDE_BG_PEN;
					}
				}
			}
	} else { // just fill bg
		for (INT32 y = 0; y < nScreenHeight; y++)
			for (INT32 x = 0; x < nScreenWidth; x++) {
				pTransDraw[(y*nScreenWidth) + x] = SWIMMER_BG_PEN;
			}
	}
}

void yamato_draw_backdrop() // synth yamato backdrop
{
	UINT8 *sky_rom = DrvUser1 + 0x1200;

	for (INT32 i = 0; i < 0x100; i++) {
		INT32 pen = 0x60 + sky_rom[(flipscreen[0] ? 0x80 : 0) + (i >> 1)];

		for (INT32 j = 0; j < 0x100; j++) {
			INT32 coord = (j * nScreenWidth) + ((i - 8) & 0xff);

			if (coord < (nScreenHeight * nScreenWidth))
				pTransDraw[coord] = pen;
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		if (game_select == 6) {
			SwimmerPaletteInit();
		} else
		if (game_select == 5) {
			YamatoPaletteInit();
		} else {
			DrvPaletteInit();
		}
		DrvRecalc = 0;
	}

	BurnTransferClear();

	if (game_select == 6) {
		swimmer_draw_backdrop();
	}

	if (game_select == 5) {
		yamato_draw_backdrop();
	}

	if (nBurnLayer & 1) draw_playfield();

	if (DrvSprRAM[bigsprite_index] & 1) {
		if (nBurnLayer & 2) cclimber_draw_bigsprite();
		if (nBurnLayer & 4) draw_sprites();
	} else {
		if (nBurnLayer & 4) draw_sprites();
		if (nBurnLayer & 2) cclimber_draw_bigsprite();
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
		UINT32 JoyInit[4] = { 0x00, 0x00, 0x00, 0x00 };
		UINT8 *DrvJoys[4] = { DrvJoy1, DrvJoy2, DrvJoy3, DrvJoy4 };

		CompileInput(DrvJoys, (void*)DrvInputs, 4, 8, JoyInit);

		if (game_select == 2)
			DrvInputs[2] = 0xff - DrvInputs[2];
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 3072000 / 60, ((game_select == 6) ? 2000000 : 3072000) / 60 };

	for (INT32 i = 0; i < nInterleave; i++) {
		ZetOpen(0);
		ZetRun(nCyclesTotal[0] / nInterleave);
		if (i == nInterleave - 1 && interrupt_enable)
			ZetNmi();
		ZetClose();

		if (uses_sub) {
			ZetOpen(1);
			ZetRun(nCyclesTotal[1] / nInterleave);
			if (game_select == 6 && (i%63==0)) // 4x per frame
				ZetNmi();
			ZetClose();
		}
	}

	if (pBurnSoundOut) {
		AY8910Render(&pAY8910Buffer[0], pBurnSoundOut, nBurnSoundLen, 0);
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
		*pnMin = 0x029735;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);

		AY8910Scan(nAction, pnMin);

		SCAN_VAR(flipscreen);
		SCAN_VAR(interrupt_enable);
		SCAN_VAR(yamato_p0);
		SCAN_VAR(yamato_p1);
		SCAN_VAR(swimmer_background_color);
		SCAN_VAR(swimmer_sidebg);
		SCAN_VAR(swimmer_palettebank);
		SCAN_VAR(soundlatch);
	}

	return 0;
}


// Crazy Climber (US)

static struct BurnRomInfo cclimberRomDesc[] = {
	{ "cc11",		0x1000, 0x217ec4ff, 1 | BRF_PRG | BRF_ESS }, //  0 - Z80 Code
	{ "cc10",		0x1000, 0xb3c26cef, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "cc09",		0x1000, 0x6db0879c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "cc08",		0x1000, 0xf48c5fe3, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "cc07",		0x1000, 0x3e873baf, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "cc06",		0x0800, 0x481b64cc, 2 | BRF_GRA },	     //  5 - Sprites & Tiles
	{ "cc05",		0x0800, 0x2c33b760, 2 | BRF_GRA },	     //  6
	{ "cc04",		0x0800, 0x332347cb, 2 | BRF_GRA },	     //  7
	{ "cc03",		0x0800, 0x4e4b3658, 2 | BRF_GRA },	     //  8

	{ "cc02",		0x0800, 0x14f3ecc9, 3 | BRF_GRA },	     //  9 - Big Sprites
	{ "cc01",		0x0800, 0x21c0f9fb, 3 | BRF_GRA },	     // 10

	{ "cclimber.pr1", 	0x0020, 0x751c3325, 6 | BRF_GRA },	     // 11 - Color Proms
	{ "cclimber.pr2", 	0x0020, 0xab1940fa, 6 | BRF_GRA },	     // 12
	{ "cclimber.pr3", 	0x0020, 0x71317756, 6 | BRF_GRA },	     // 13 

	{ "cc13",		0x1000, 0xe0042f75, 7 | BRF_SND },	     // 14 - Samples
	{ "cc12",		0x1000, 0x5da13aaa, 7 | BRF_SND },	     // 15
};

STD_ROM_PICK(cclimber)
STD_ROM_FN(cclimber)

static void cclimber_decrypt()
{
	static const INT8 convtable[8][16] =
	{
		/* -1 marks spots which are unused and therefore unknown */
		{ 0x44,0x14,0x54,0x10,0x11,0x41,0x05,0x50,0x51,0x00,0x40,0x55,0x45,0x04,0x01,0x15 },
		{ 0x44,0x10,0x15,0x55,0x00,0x41,0x40,0x51,0x14,0x45,0x11,0x50,0x01,0x54,0x04,0x05 },
		{ 0x45,0x10,0x11,0x44,0x05,0x50,0x51,0x04,0x41,0x14,0x15,0x40,0x01,0x54,0x55,0x00 },
		{ 0x04,0x51,0x45,0x00,0x44,0x10,  -1,0x55,0x11,0x54,0x50,0x40,0x05,  -1,0x14,0x01 },
		{ 0x54,0x51,0x15,0x45,0x44,0x01,0x11,0x41,0x04,0x55,0x50,  -1,0x00,0x10,0x40,  -1 },
		{   -1,0x54,0x14,0x50,0x51,0x01,  -1,0x40,0x41,0x10,0x00,0x55,0x05,0x44,0x11,0x45 },
		{ 0x51,0x04,0x10,  -1,0x50,0x40,0x00,  -1,0x41,0x01,0x05,0x15,0x11,0x14,0x44,0x54 },
		{   -1,  -1,0x54,0x01,0x15,0x40,0x45,0x41,0x51,0x04,0x50,0x05,0x11,0x44,0x10,0x14 }
	};

	cclimber_decode(convtable);
}

static INT32 cclimberInit()
{
	INT32 nRet;

	game_select = 1;

	nRet = DrvInit();

	if (nRet == 0) {
		cclimber_decrypt();
	}

	return nRet;
}

struct BurnDriver BurnDrvCclimber = {
	"cclimber", NULL, NULL, NULL, "1980",
	"Crazy Climber (US)\0", NULL, "Nichibutsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, cclimberRomInfo, cclimberRomName, NULL, NULL, CclimberInputInfo, CclimberDIPInfo,
	cclimberInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x60,
	256, 224, 4, 3
};

static INT32 ckongInit()
{
	game_select = 2;
	uses_sub = 0;

	INT32 rc = DrvInit();

	if (rc == 0) {
		//save for later maybe
	}

	return rc;
}

// Crazy Kong Part II (set 1)

static struct BurnRomInfo ckongpt2RomDesc[] = {
	{ "7.5d",		0x1000, 0xb27df032, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "8.5e",		0x1000, 0x5dc1aaba, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "9.5h",		0x1000, 0xc9054c94, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "10.5k",		0x1000, 0x069c4797, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "11.5l",		0x1000, 0xae159192, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "12.5n",		0x1000, 0x966bc9ab, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "6.11n",		0x1000, 0x2dcedd12, 2 | BRF_GRA }, //  6 gfx1
	{ "5.11l",		0x1000, 0xfa7cbd91, 2 | BRF_GRA }, //  7
	{ "4.11k",		0x1000, 0x3375b3bd, 2 | BRF_GRA }, //  8
	{ "3.11h",		0x1000, 0x5655cc11, 2 | BRF_GRA }, //  9

	{ "2.11c",		0x0800, 0xd1352c31, 3 | BRF_GRA }, // 10 gfx2
	{ "1.11a",		0x0800, 0xa7a2fdbd, 3 | BRF_GRA }, // 11

	{ "prom.v6",	0x0020, 0xb3fc1505, 6 | BRF_GRA }, // 12 proms
	{ "prom.u6",	0x0020, 0x26aada9e, 6 | BRF_GRA }, // 13
	{ "prom.t6",	0x0020, 0x676b3166, 6 | BRF_GRA }, // 14

	{ "14.5s",		0x1000, 0x5f0bcdfb, 7 | BRF_GRA }, // 15 samples
	{ "13.5p",		0x1000, 0x9003ffbd, 7 | BRF_GRA }, // 16
};

STD_ROM_PICK(ckongpt2)
STD_ROM_FN(ckongpt2)

struct BurnDriverD BurnDrvCkongpt2 = {
	"ckongpt2", NULL, NULL, NULL, "1981",
	"Crazy Kong Part II (set 1)\0", NULL, "Falcon", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	/*BDF_GAME_WORKING*/ 0 | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, ckongpt2RomInfo, ckongpt2RomName, NULL, NULL, CkongInputInfo, CkongDIPInfo,
	ckongInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x60,
	224, 256, 3, 4
};


// Crazy Kong (not working but needed as parent)

static struct BurnRomInfo ckongRomDesc[] = {
	{ "falcon7",	0x1000, 0x2171cac3, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "falcon8",	0x1000, 0x88b83ff7, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "falcon9",	0x1000, 0xcff2af47, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "falcon10",	0x1000, 0x6b2ecf23, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "falcon11",	0x1000, 0x327dcadf, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "falcon6",	0x1000, 0xa8916dc8, 2 | BRF_GRA }, //  6 gfx1
	{ "falcon5",	0x1000, 0xcd3b5dde, 2 | BRF_GRA }, //  7
	{ "falcon4",	0x1000, 0xb62a0367, 2 | BRF_GRA }, //  8
	{ "falcon3",	0x1000, 0x61122c5e, 2 | BRF_GRA }, //  9

	{ "falcon2",	0x0800, 0xf67c80f1, 3 | BRF_GRA }, // 10 gfx2
	{ "falcon1",	0x0800, 0x80eb517d, 3 | BRF_GRA }, // 11

	{ "ck6v.bin",	0x0020, 0x751c3325, 6 | BRF_GRA }, // 12 proms
	{ "ck6u.bin",	0x0020, 0xab1940fa, 6 | BRF_GRA }, // 13
	{ "ck6t.bin",	0x0020, 0xb4e827a5, 6 | BRF_GRA }, // 14

	{ "falcon13",	0x1000, 0x5f0bcdfb, 7 | BRF_GRA }, // 15 samples
	{ "falcon12",	0x1000, 0x9003ffbd, 7 | BRF_GRA }, // 16
};

STD_ROM_PICK(ckong)
STD_ROM_FN(ckong)

struct BurnDriver BurnDrvCkong = {
	"ckong", NULL, NULL, NULL, "1981",
	"Crazy Kong\0", NULL, "Kyoei / Falcon", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	/*BDF_GAME_WORKING*/ 0 | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, ckongRomInfo, ckongRomName, NULL, NULL, CkongInputInfo, CkongDIPInfo,
	ckongInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x60,
	224, 256, 3, 4
};



void cclimbrj_decode()
{
	static const INT8 convtable[8][16] =
	{
		{ 0x41,0x54,0x51,0x14,0x05,0x10,0x01,0x55,0x44,0x11,0x00,0x50,0x15,0x40,0x04,0x45 },
		{ 0x50,0x11,0x40,0x55,0x51,0x14,0x45,0x04,0x54,0x15,0x10,0x05,0x44,0x01,0x00,0x41 },
		{ 0x44,0x11,0x00,0x50,0x41,0x54,0x04,0x14,0x15,0x40,0x51,0x55,0x05,0x10,0x01,0x45 },
		{ 0x10,0x50,0x54,0x55,0x01,0x44,0x40,0x04,0x14,0x11,0x00,0x41,0x45,0x15,0x51,0x05 },
		{ 0x14,0x41,0x01,0x44,0x04,0x50,0x51,0x45,0x11,0x40,0x54,0x15,0x10,0x00,0x55,0x05 },
		{ 0x01,0x05,0x41,0x45,0x54,0x50,0x55,0x10,0x11,0x15,0x51,0x14,0x44,0x40,0x04,0x00 },
		{ 0x05,0x55,0x00,0x50,0x11,0x40,0x54,0x14,0x45,0x51,0x10,0x04,0x44,0x01,0x41,0x15 },
		{ 0x55,0x50,0x15,0x10,0x01,0x04,0x41,0x44,0x45,0x40,0x05,0x00,0x11,0x14,0x51,0x54 },
	};

	cclimber_decode(convtable);
}

static void sega_decode(const UINT8 convtable[32][4])
{
	INT32 A;

	INT32 length = 0x10000;
	INT32 cryptlen = 0x8000;
	UINT8 *rom = DrvZ80ROM;
	UINT8 *decrypted = DrvZ80OPS;
	
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

	/* this is a kludge to catch anyone who has code that crosses the encrypted/ */
	/* decrypted boundary. ssanchan does it */
	if (length > 0x8000)
	{
		INT32 bytes = 0x4000;
		memcpy(&decrypted[0x8000], &rom[0x8000], bytes);
	}
}


static INT32 yamatoInit()
{
	static const UINT8 convtable[32][4] =
	{
		/*       opcode                   data                     address      */
		/*  A    B    C    D         A    B    C    D                           */
		{ 0x88,0xa8,0x08,0x28 }, { 0x88,0xa8,0x80,0xa0 },   /* ...0...0...0...0 */
		{ 0x20,0xa0,0x28,0xa8 }, { 0x88,0xa8,0x80,0xa0 },   /* ...0...0...0...1 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0x88,0xa8,0x80,0xa0 },   /* ...0...0...1...0 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0x20,0xa0,0x28,0xa8 },   /* ...0...0...1...1 */
		{ 0x88,0xa8,0x08,0x28 }, { 0x88,0xa8,0x08,0x28 },   /* ...0...1...0...0 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0x88,0xa8,0x80,0xa0 },   /* ...0...1...0...1 */
		{ 0x20,0xa0,0x28,0xa8 }, { 0x20,0xa0,0x28,0xa8 },   /* ...0...1...1...0 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0x88,0xa8,0x80,0xa0 },   /* ...0...1...1...1 */
		{ 0x20,0xa0,0x28,0xa8 }, { 0x88,0xa8,0x08,0x28 },   /* ...1...0...0...0 */
		{ 0x20,0xa0,0x28,0xa8 }, { 0x28,0x20,0xa8,0xa0 },   /* ...1...0...0...1 */
		{ 0xa0,0x20,0x80,0x00 }, { 0x20,0xa0,0x28,0xa8 },   /* ...1...0...1...0 */
		{ 0x28,0x20,0xa8,0xa0 }, { 0x20,0xa0,0x28,0xa8 },   /* ...1...0...1...1 */
		{ 0x20,0xa0,0x28,0xa8 }, { 0x88,0xa8,0x08,0x28 },   /* ...1...1...0...0 */
		{ 0x88,0xa8,0x08,0x28 }, { 0x88,0xa8,0x08,0x28 },   /* ...1...1...0...1 */
		{ 0xa0,0x20,0x80,0x00 }, { 0x88,0x08,0x80,0x00 },   /* ...1...1...1...0 */
		{ 0x20,0xa0,0x28,0xa8 }, { 0x00,0x08,0x20,0x28 }    /* ...1...1...1...1 */
	};

	game_select = 5;
	uses_sub = 1;

	INT32 rc = DrvInit();
	if (rc == 0) {
		memmove(DrvZ80ROM+0x7000, DrvZ80ROM+0x6000, 0x1000);
		memset(DrvZ80ROM+0x6000, 0, 0x1000);
		sega_decode(convtable);

		ZetOpen(0);
		ZetMapArea(0x0000, 0x5fff, 2, DrvZ80OPS, DrvZ80ROM);
		ZetMapMemory(DrvZ80ROM+0x7000, 0x7000, 0x7fff, MAP_ROM);
		ZetMapArea(0x7000, 0x7fff, 2, DrvZ80OPS+0x7000, DrvZ80ROM+0x7000);

		ZetClose();
	}

	return rc;
}

// Yamato (US)

static struct BurnRomInfo yamatoRomDesc[] = {
	{ "2.5de",	0x2000, 0x20895096, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "3.5f",	0x2000, 0x57a696f9, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "4.5jh",	0x2000, 0x59a468e8, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "11.5a",	0x1000, 0x35987485, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "1.5v",	0x0800, 0x3aad9e3c, 7 | BRF_PRG | BRF_ESS }, //  4 audiocpu

	{ "10.11k",	0x2000, 0x161121f5, 2 | BRF_GRA }, //  5 gfx1
	{ "9.11h",	0x2000, 0x56e84cc4, 2 | BRF_GRA }, //  6

	{ "8.11c",	0x1000, 0x28024d9a, 3 | BRF_GRA }, //  7 gfx2
	{ "7.11a",	0x1000, 0x4a179790, 3 | BRF_GRA }, //  8

	{ "5.5lm",	0x1000, 0x7761ad24, 4 | BRF_GRA }, //  9 user1
	{ "6.5n",	0x1000, 0xda48444c, 4 | BRF_GRA }, // 10

	{ "1.bpr",	0x0020, 0xef2053ab, 6 | BRF_GRA }, // 11 proms
	{ "2.bpr",	0x0020, 0x2281d39f, 6 | BRF_GRA }, // 12
	{ "3.bpr",	0x0020, 0x9e6341e3, 6 | BRF_GRA }, // 13
	{ "4.bpr",	0x0020, 0x1c97dc0b, 6 | BRF_GRA }, // 14
	{ "5.bpr",	0x0020, 0xedd6c05f, 6 | BRF_GRA }, // 15
};

STD_ROM_PICK(yamato)
STD_ROM_FN(yamato)

struct BurnDriver BurnDrvYamato = {
	"yamato", NULL, NULL, NULL, "1983",
	"Yamato (US)\0", NULL, "Sega", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, yamatoRomInfo, yamatoRomName, NULL, NULL, YamatoInputInfo, YamatoDIPInfo,
	yamatoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Yamato (World?)

static struct BurnRomInfo yamato2RomDesc[] = {
	{ "2-2.5de",0x2000, 0x93da1d52, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "3-2.5f",	0x2000, 0x31e73821, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "4-2.5jh",0x2000, 0xfd7bcfc3, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "1.5v",	0x0800, 0x3aad9e3c, 7 | BRF_PRG | BRF_ESS }, //  3 audiocpu

	{ "10.11k",	0x2000, 0x161121f5, 2 | BRF_GRA }, //  4 gfx1
	{ "9.11h",	0x2000, 0x56e84cc4, 2 | BRF_GRA }, //  5

	{ "8.11c",	0x1000, 0x28024d9a, 3 | BRF_GRA }, //  6 gfx2
	{ "7.11a",	0x1000, 0x4a179790, 3 | BRF_GRA }, //  7

	{ "5.5lm",	0x1000, 0x7761ad24, 4 | BRF_GRA }, //  8 user1
	{ "6.5n",	0x1000, 0xda48444c, 4 | BRF_GRA }, //  9

	{ "1.bpr",	0x0020, 0xef2053ab, 6 | BRF_GRA }, // 10 proms
	{ "2.bpr",	0x0020, 0x2281d39f, 6 | BRF_GRA }, // 11
	{ "3.bpr",	0x0020, 0x9e6341e3, 6 | BRF_GRA }, // 12
	{ "4.bpr",	0x0020, 0x1c97dc0b, 6 | BRF_GRA }, // 13
	{ "5.bpr",	0x0020, 0xedd6c05f, 6 | BRF_GRA }, // 14
};

STD_ROM_PICK(yamato2)
STD_ROM_FN(yamato2)

struct BurnDriver BurnDrvYamato2 = {
	"yamato2", "yamato", NULL, NULL, "1983",
	"Yamato (World?)\0", NULL, "Sega", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, yamato2RomInfo, yamato2RomName, NULL, NULL, YamatoInputInfo, YamatoDIPInfo,
	yamatoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};

static INT32 guzzlerInit()
{
	game_select = 6;
	uses_sub = 1;

	INT32 rc = DrvInit();

	if (rc == 0) {
		//save for later maybe
	}

	return rc;
}

// Guzzler

static struct BurnRomInfo guzzlerRomDesc[] = {
	{ "guzz-01.bin",	0x2000, 0x58aaa1e9, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "guzz-02.bin",	0x2000, 0xf80ceb17, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "guzz-03.bin",	0x2000, 0xe63c65a2, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "guzz-04.bin",	0x2000, 0x45be42f5, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "guzz-16.bin",	0x2000, 0x61ee00b7, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "guzz-12.bin",	0x1000, 0xf3754d9e, 7 | BRF_PRG | BRF_ESS }, //  5 audiocpu

	{ "guzz-13.bin",	0x1000, 0xafc464e2, 2 | BRF_GRA },           //  6 gfx1
	{ "guzz-14.bin",	0x1000, 0xacbdfe1f, 2 | BRF_GRA },           //  7
	{ "guzz-15.bin",	0x1000, 0x66978c05, 2 | BRF_GRA },           //  8

	{ "guzz-11.bin",	0x1000, 0xec2e9d86, 3 | BRF_GRA },           //  9 gfx2
	{ "guzz-10.bin",	0x1000, 0xbd3f0bf7, 3 | BRF_GRA },           // 10
	{ "guzz-09.bin",	0x1000, 0x18927579, 3 | BRF_GRA },           // 11

	{ "guzzler.003",	0x0100, 0xf86930c1, 6 | BRF_GRA },           // 12 proms
	{ "guzzler.002",	0x0100, 0xb566ea9e, 6 | BRF_GRA },           // 13
	{ "guzzler.001",	0x0020, 0x69089495, 6 | BRF_GRA },           // 14
};

STD_ROM_PICK(guzzler)
STD_ROM_FN(guzzler)

struct BurnDriver BurnDrvGuzzler = {
	"guzzler", NULL, NULL, NULL, "1983",
	"Guzzler\0", NULL, "Tehkan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, guzzlerRomInfo, guzzlerRomName, NULL, NULL, GuzzlerInputInfo, GuzzlerDIPInfo,
	guzzlerInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};

// Swimmer (set 1)

static struct BurnRomInfo swimmerRomDesc[] = {
	{ "sw1",	0x1000, 0xf12481e7, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "sw2",	0x1000, 0xa0b6fdd2, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sw3",	0x1000, 0xec93d7de, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sw4",	0x1000, 0x0107927d, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "sw5",	0x1000, 0xebd8a92c, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "sw6",	0x1000, 0xf8539821, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "sw7",	0x1000, 0x37efb64e, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "sw8",	0x1000, 0x33d6001e, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "sw12.4k",	0x1000, 0x2eee9bcb, 7 | BRF_PRG | BRF_ESS }, //  8 audiocpu

	{ "sw15.18k",	0x1000, 0x4f3608cb, 2 | BRF_GRA },           //  9 gfx1
	{ "sw14.18l",	0x1000, 0x7181c8b4, 2 | BRF_GRA },           // 10
	{ "sw13.18m",	0x1000, 0x2eb1af5c, 2 | BRF_GRA },           // 11

	{ "sw23.6c",	0x0800, 0x9ca67e24, 3 | BRF_GRA },           // 12 gfx2
	{ "sw22.5c",	0x0800, 0x02c10992, 3 | BRF_GRA },           // 13
	{ "sw21.4c",	0x0800, 0x7f4993c1, 3 | BRF_GRA },           // 14

	{ "24s10.13b",	0x0100, 0x8e35b97d, 6 | BRF_GRA },           // 15 proms
	{ "24s10.13a",	0x0100, 0xc5f24909, 6 | BRF_GRA },           // 16
	{ "18s030.12c",	0x0020, 0x3b2deb3a, 6 | BRF_GRA },           // 17
};

STD_ROM_PICK(swimmer)
STD_ROM_FN(swimmer)

struct BurnDriver BurnDrvSwimmer = {
	"swimmer", NULL, NULL, NULL, "1982",
	"Swimmer (set 1)\0", NULL, "Tehkan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, swimmerRomInfo, swimmerRomName, NULL, NULL, SwimmerInputInfo, SwimmerDIPInfo,
	guzzlerInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};

static INT32 rpatrolInit()
{
	game_select = 1;
	uses_sub = 0;
	gfx0_cont800 = 1;

	INT32 rc = DrvInit();

	if (rc == 0) { // decryption
		for (INT32 i = 0x0000; i < 0x5000; i++)	{
			DrvZ80ROM[i] = DrvZ80ROM[i] ^ 0x79;
			i++;
			DrvZ80ROM[i] = DrvZ80ROM[i] ^ 0x5b;
		}
	}

	return rc;
}

static INT32 rpatrolbInit()
{
	game_select = 1;
	uses_sub = 0;

	INT32 rc = DrvInit();

	return rc;
}

static INT32 silvlandInit()
{
	game_select = 1;
	uses_sub = 0;
	silvland = 1;

	INT32 rc = DrvInit();

	return rc;
}

// River Patrol (Orca)

static struct BurnRomInfo rpatrolRomDesc[] = {
	{ "1.1h",		0x1000, 0x065197f0, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "2.1f",		0x1000, 0x3614b820, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.1d",		0x1000, 0xba428bbf, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.1c",		0x1000, 0x41497a94, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "5.1a",		0x1000, 0xe20ee7e7, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "6.6l.2732",	0x1000, 0xb38d8aca, 2 | BRF_GRA },           //  5 gfx1
	{ "7.6p.2732",	0x1000, 0xbc2bddf9, 2 | BRF_GRA },           //  6

	{ "9.2t",		0x0800, 0xd373fc48, 3 | BRF_GRA },           //  7 gfx2
	{ "8.2s",		0x0800, 0x59747c31, 3 | BRF_GRA },           //  8

	{ "bprom1.9n",	0x0020, 0xf9a2383b, 6 | BRF_GRA },           //  9 proms
	{ "bprom2.9p",	0x0020, 0x1743bd26, 6 | BRF_GRA },           // 10
	{ "bprom3.9c",	0x0020, 0xee03bc96, 6 | BRF_GRA },           // 11
};

STD_ROM_PICK(rpatrol)
STD_ROM_FN(rpatrol)

struct BurnDriver BurnDrvRpatrol = {
	"rpatrol", NULL, NULL, NULL, "1981",
	"River Patrol (Orca)\0", NULL, "Orca", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, rpatrolRomInfo, rpatrolRomName, NULL, NULL, RpatrolInputInfo, RpatrolDIPInfo,
	rpatrolInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// River Patrol (bootleg)

static struct BurnRomInfo rpatrolbRomDesc[] = {
	{ "rp1.4l",	0x1000, 0xbfd7ae7a, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "rp2.4j",	0x1000, 0x03f53340, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rp3.4f",	0x1000, 0x8fa300df, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rp4.4e",	0x1000, 0x74a8f1f4, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "rp5.4c",	0x1000, 0xd7ef6c87, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "rp6.6n",	0x0800, 0x19f18e9e, 2 | BRF_GRA },           //  5 gfx1
	{ "rp7.6l",	0x0800, 0x07f2070d, 2 | BRF_GRA },           //  6
	{ "rp8.6k",	0x0800, 0x008738c7, 2 | BRF_GRA },           //  7
	{ "rp9.6h",	0x0800, 0xea5aafca, 2 | BRF_GRA },           //  8

	{ "rp11.6c",	0x0800, 0x065651a5, 3 | BRF_GRA },           //  9 gfx2
	{ "rp10.6a",	0x0800, 0x59747c31, 3 | BRF_GRA },           // 10

	{ "bprom1.9n",	0x0020, 0xf9a2383b, 6 | BRF_GRA },           // 11 proms
	{ "bprom2.9p",	0x0020, 0x1743bd26, 6 | BRF_GRA },           // 12
	{ "bprom3.9c",	0x0020, 0xee03bc96, 6 | BRF_GRA },           // 13
};

STD_ROM_PICK(rpatrolb)
STD_ROM_FN(rpatrolb)

struct BurnDriver BurnDrvRpatrolb = {
	"rpatrolb", "rpatrol", NULL, NULL, "1981",
	"River Patrol (bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, rpatrolbRomInfo, rpatrolbRomName, NULL, NULL, RpatrolInputInfo, RpatrolDIPInfo,
	rpatrolbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// Silver Land

static struct BurnRomInfo silvlandRomDesc[] = {
	{ "7.2r",	0x1000, 0x57e6be62, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "8.1n",	0x1000, 0xbbb2b287, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rp3.4f",	0x1000, 0x8fa300df, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "10.2n",	0x1000, 0x5536a65d, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "11.1r",	0x1000, 0x6f23f66f, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "12.2k",	0x1000, 0x26f1537c, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "6.6n",	0x0800, 0xaffb804f, 2 | BRF_GRA },           //  6 gfx1
	{ "5.6l",	0x0800, 0xad4642e5, 2 | BRF_GRA },           //  7
	{ "4.6k",	0x0800, 0xe487579d, 2 | BRF_GRA },           //  8
	{ "3.6h",	0x0800, 0x59125a1a, 2 | BRF_GRA },           //  9

	{ "2.6c",	0x0800, 0xc8d32b8e, 3 | BRF_GRA },           // 10 gfx2
	{ "1.6a",	0x0800, 0xee333daf, 3 | BRF_GRA },           // 11

	{ "mb7051.1v",	0x0020, 0x1d2343b1, 6 | BRF_GRA },           // 12 proms
	{ "mb7051.1u",	0x0020, 0xc174753c, 6 | BRF_GRA },           // 13
	{ "mb7051.1t",	0x0020, 0x04a1be01, 6 | BRF_GRA },           // 14
};

STD_ROM_PICK(silvland)
STD_ROM_FN(silvland)

struct BurnDriver BurnDrvSilvland = {
	"silvland", "rpatrol", NULL, NULL, "1981",
	"Silver Land\0", NULL, "Falcon", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, silvlandRomInfo, silvlandRomName, NULL, NULL, RpatrolInputInfo, RpatrolDIPInfo,
	silvlandInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};
