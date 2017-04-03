// FB Alpha Wiz driver module
// Based on MAME driver by Zsolt Vasvari

// Wiz Todo:
// scion: static in audio is normal (no kidding), use scionc!
//

#include "tiles_generic.h"
#include "bitswap.h"
#include "z80_intf.h"
#include "driver.h"
extern "C" {
#include "ay8910.h"
}
#include "samples.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80Dec;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvVidRAM0;
static UINT8 *DrvVidRAM1;
static UINT8 *DrvColRAM0;
static UINT8 *DrvColRAM1;
static UINT8 *DrvSprRAM0;
static UINT8 *DrvSprRAM1;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 *soundlatch;
static UINT8 *sprite_bank;
static UINT8 *interrupt_enable;
static UINT8 *palette_bank;
static UINT8 *char_bank_select;
static UINT8 *screen_flip;
static UINT8 *background_color;

static INT16 *pAY8910Buffer[9];

static UINT8 DrvInputs[2];
static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[2];
static UINT8 DrvReset;

static UINT8 bHasSamples = 0;

static UINT8 Wizmode = 0;
static UINT8 Scionmodeoffset = 0;

static struct BurnInputInfo WizInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Wiz)

static struct BurnInputInfo ScionInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 fire 2"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
};

STDINPUTINFO(Scion)

static struct BurnInputInfo StingerInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 fire 2"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
};

STDINPUTINFO(Stinger)

static struct BurnInputInfo KungfutInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 3"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 start"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 fire 2"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
};

STDINPUTINFO(Kungfut)

static struct BurnDIPInfo StingerDIPList[]=
{
	{0x11, 0xff, 0xff, 0xef, NULL		},
	{0x12, 0xff, 0xff, 0xae, NULL		},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x11, 0x01, 0x07, 0x01, "4 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x02, "3 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x04, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x00, "3 Coins 2 Credits"		},
	{0x11, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x07, 0x03, "2 Coins 3 Credits"		},
	{0x11, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x11, 0x01, 0x18, 0x00, "2"		},
	{0x11, 0x01, 0x18, 0x08, "3"		},
	{0x11, 0x01, 0x18, 0x10, "4"		},
	{0x11, 0x01, 0x18, 0x18, "5"		},

	{0   , 0xfe, 0   ,    8, "Bonus Life"		},
	{0x11, 0x01, 0xe0, 0xe0, "20000 50000"		},
	{0x11, 0x01, 0xe0, 0xc0, "20000 60000"		},
	{0x11, 0x01, 0xe0, 0xa0, "20000 70000"		},
	{0x11, 0x01, 0xe0, 0x80, "20000 80000"		},
	{0x11, 0x01, 0xe0, 0x60, "20000 90000"		},
	{0x11, 0x01, 0xe0, 0x40, "30000 80000"		},
	{0x11, 0x01, 0xe0, 0x20, "30000 90000"		},
	{0x11, 0x01, 0xe0, 0x00, "None"		},

	{0   , 0xfe, 0   ,    2, "Debug Mode"		},
	{0x12, 0x01, 0x01, 0x00, "Off"		},
	{0x12, 0x01, 0x01, 0x01, "On"		},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x12, 0x01, 0x0e, 0x00, "5 Coins 1 Credits"		},
	{0x12, 0x01, 0x0e, 0x08, "4 Coins 1 Credits"		},
	{0x12, 0x01, 0x0e, 0x0c, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0x0e, 0x0a, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0x0e, 0x04, "3 Coins 2 Credits"		},
	{0x12, 0x01, 0x0e, 0x0e, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0x0e, 0x02, "2 Coins 3 Credits"		},
	{0x12, 0x01, 0x0e, 0x06, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    4, "Bongo Time"		},
	{0x12, 0x01, 0x30, 0x30, "Long"		},
	{0x12, 0x01, 0x30, 0x20, "Medium"		},
	{0x12, 0x01, 0x30, 0x10, "Short"		},
	{0x12, 0x01, 0x30, 0x00, "Shortest"		},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x12, 0x01, 0x40, 0x00, "Normal"		},
	{0x12, 0x01, 0x40, 0x40, "Hard"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x12, 0x01, 0x80, 0x80, "Upright"		},
	{0x12, 0x01, 0x80, 0x00, "Cocktail"		},
};

STDDIPINFO(Stinger)

static struct BurnDIPInfo Stinger2DIPList[]=
{
	{0x11, 0xff, 0xff, 0xef, NULL		},
	{0x12, 0xff, 0xff, 0xa0, NULL		},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x11, 0x01, 0x07, 0x01, "4 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x02, "3 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x04, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x00, "3 Coins 2 Credits"		},
	{0x11, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x07, 0x03, "2 Coins 3 Credits"		},
	{0x11, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x11, 0x01, 0x18, 0x00, "2"		},
	{0x11, 0x01, 0x18, 0x08, "3"		},
	{0x11, 0x01, 0x18, 0x10, "4"		},
	{0x11, 0x01, 0x18, 0x18, "5"		},

	{0   , 0xfe, 0   ,    8, "Bonus Life"		},
	{0x11, 0x01, 0xe0, 0xe0, "20000 50000"		},
	{0x11, 0x01, 0xe0, 0xc0, "20000 60000"		},
	{0x11, 0x01, 0xe0, 0xa0, "20000 70000"		},
	{0x11, 0x01, 0xe0, 0x80, "20000 80000"		},
	{0x11, 0x01, 0xe0, 0x60, "20000 90000"		},
	{0x11, 0x01, 0xe0, 0x40, "30000 80000"		},
	{0x11, 0x01, 0xe0, 0x20, "30000 90000"		},
	{0x11, 0x01, 0xe0, 0x00, "None"		},

	{0   , 0xfe, 0   ,    2, "Debug Mode"		},
	{0x12, 0x01, 0x01, 0x00, "Off"		},
	{0x12, 0x01, 0x01, 0x01, "On"		},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x12, 0x01, 0x02, 0x00, "Off"		},
	{0x12, 0x01, 0x02, 0x02, "On"		},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x12, 0x01, 0x04, 0x00, "Off"		},
	{0x12, 0x01, 0x04, 0x04, "On"		},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x12, 0x01, 0x08, 0x00, "Off"		},
	{0x12, 0x01, 0x08, 0x08, "On"		},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x12, 0x01, 0x70, 0x70, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0x70, 0x60, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0x70, 0x50, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0x70, 0x40, "1 Coin  4 Credits"		},
	{0x12, 0x01, 0x70, 0x30, "1 Coin  5 Credits"		},
	{0x12, 0x01, 0x70, 0x20, "1 Coin  6 Credits"		},
	{0x12, 0x01, 0x70, 0x10, "1 Coin  7 Credits"		},
	{0x12, 0x01, 0x70, 0x00, "1 Coin  8 Credits"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x12, 0x01, 0x80, 0x80, "Upright"		},
	{0x12, 0x01, 0x80, 0x00, "Cocktail"		},
};

STDDIPINFO(Stinger2)

static struct BurnDIPInfo ScionDIPList[]=
{
	{0x11, 0xff, 0xff, 0x05, NULL		},
	{0x12, 0xff, 0xff, 0x00, NULL		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x11, 0x01, 0x01, 0x01, "Upright"		},
	{0x11, 0x01, 0x01, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x11, 0x01, 0x02, 0x00, "Easy"		},
	{0x11, 0x01, 0x02, 0x02, "Hard"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x11, 0x01, 0x0c, 0x00, "2"		},
	{0x11, 0x01, 0x0c, 0x04, "3"		},
	{0x11, 0x01, 0x0c, 0x08, "4"		},
	{0x11, 0x01, 0x0c, 0x0c, "5"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x11, 0x01, 0x30, 0x00, "20000 40000"		},
	{0x11, 0x01, 0x30, 0x20, "20000 60000"		},
	{0x11, 0x01, 0x30, 0x10, "20000 80000"		},
	{0x11, 0x01, 0x30, 0x30, "30000 90000"		},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x11, 0x01, 0x40, 0x40, "Off"		},
	{0x11, 0x01, 0x40, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x11, 0x01, 0x80, 0x80, "Off"		},
	{0x11, 0x01, 0x80, 0x00, "On"		},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x12, 0x01, 0x07, 0x07, "5 Coins 1 Credits"		},
	{0x12, 0x01, 0x07, 0x03, "4 Coins 1 Credits"		},
	{0x12, 0x01, 0x07, 0x05, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0x07, 0x01, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0x07, 0x00, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0x07, 0x04, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0x07, 0x02, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0x07, 0x06, "1 Coin  5 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x12, 0x01, 0x18, 0x18, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0x18, 0x08, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0x18, 0x00, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0x18, 0x10, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    1, "Unused"		},
	{0x12, 0x01, 0x20, 0x00, "Off"		},

	{0   , 0xfe, 0   ,    1, "Unused"		},
	{0x12, 0x01, 0x40, 0x00, "Off"		},

	{0   , 0xfe, 0   ,    1, "Unused"		},
	{0x12, 0x01, 0x80, 0x00, "Off"		},
};

STDDIPINFO(Scion)

static struct BurnDIPInfo KungfutDIPList[]=
{
	{0x0e, 0xff, 0xff, 0x20, NULL		},
	{0x0f, 0xff, 0xff, 0x0c, NULL		},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x0e, 0x01, 0x07, 0x07, "5 Coins 1 Credits"		},
	{0x0e, 0x01, 0x07, 0x03, "4 Coins 1 Credits"		},
	{0x0e, 0x01, 0x07, 0x05, "3 Coins 1 Credits"		},
	{0x0e, 0x01, 0x07, 0x01, "2 Coins 1 Credits"		},
	{0x0e, 0x01, 0x07, 0x00, "1 Coin  1 Credits"		},
	{0x0e, 0x01, 0x07, 0x04, "1 Coin  2 Credits"		},
	{0x0e, 0x01, 0x07, 0x02, "1 Coin  3 Credits"		},
	{0x0e, 0x01, 0x07, 0x06, "1 Coin  5 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x0e, 0x01, 0x18, 0x18, "3 Coins 1 Credits"		},
	{0x0e, 0x01, 0x18, 0x08, "2 Coins 1 Credits"		},
	{0x0e, 0x01, 0x18, 0x00, "1 Coin  1 Credits"		},
	{0x0e, 0x01, 0x18, 0x10, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    2, "2 Players Game"		},
	{0x0e, 0x01, 0x20, 0x00, "1 Credit"		},
	{0x0e, 0x01, 0x20, 0x20, "2 Credits"		},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x0e, 0x01, 0x40, 0x00, "Off"		},
	{0x0e, 0x01, 0x40, 0x40, "On"		},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x0e, 0x01, 0x80, 0x00, "Off"		},
	{0x0e, 0x01, 0x80, 0x80, "On"		},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x0f, 0x01, 0x01, 0x00, "Easy"		},
	{0x0f, 0x01, 0x01, 0x01, "Hard"		},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x0f, 0x01, 0x02, 0x00, "Off"		},
	{0x0f, 0x01, 0x02, 0x02, "On"		},

	{0   , 0xfe, 0   ,    2, "Microphone"		},
	{0x0f, 0x01, 0x04, 0x04, "Off"		},
	{0x0f, 0x01, 0x04, 0x00, "On"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0f, 0x01, 0x18, 0x00, "2"		},
	{0x0f, 0x01, 0x18, 0x08, "3"		},
	{0x0f, 0x01, 0x18, 0x10, "4"		},
	{0x0f, 0x01, 0x18, 0x18, "5"		},

	{0   , 0xfe, 0   ,    3, "Bonus Life"		},
	{0x0f, 0x01, 0x60, 0x00, "20000 40000"		},
	{0x0f, 0x01, 0x60, 0x10, "20000 80000"		},
	{0x0f, 0x01, 0x60, 0x30, "30000 90000"		},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x0f, 0x01, 0x80, 0x00, "Off"		},
	{0x0f, 0x01, 0x80, 0x80, "On"		},
};

STDDIPINFO(Kungfut)


static struct BurnDIPInfo WizDIPList[]=
{
	{0x11, 0xff, 0xff, 0x00, NULL			},
	{0x12, 0xff, 0xff, 0x10, NULL			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x11, 0x01, 0x07, 0x07, "5 Coins 1 Credits"	},
	{0x11, 0x01, 0x07, 0x03, "4 Coins 1 Credits"	},
	{0x11, 0x01, 0x07, 0x05, "3 Coins 1 Credits"	},
	{0x11, 0x01, 0x07, 0x01, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x07, 0x00, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x07, 0x04, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0x07, 0x02, "1 Coin  3 Credits"	},
	{0x11, 0x01, 0x07, 0x06, "1 Coin  5 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x11, 0x01, 0x18, 0x08, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x18, 0x00, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x18, 0x18, "2 Coins 3 Credits"	},
	{0x11, 0x01, 0x18, 0x10, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x11, 0x01, 0x80, 0x00, "Off"			},
	{0x11, 0x01, 0x80, 0x80, "On"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x12, 0x01, 0x01, 0x00, "Upright"		},
	{0x12, 0x01, 0x01, 0x01, "Cocktail"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x12, 0x01, 0x06, 0x00, "Easy"			},
	{0x12, 0x01, 0x06, 0x02, "Normal"		},
	{0x12, 0x01, 0x06, 0x04, "Hard"			},
	{0x12, 0x01, 0x06, 0x06, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x12, 0x01, 0x18, 0x08, "1"			},
	{0x12, 0x01, 0x18, 0x10, "3"			},
	{0x12, 0x01, 0x18, 0x18, "5"			},
	{0x12, 0x01, 0x18, 0x00, "255 (Cheat)"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x12, 0x01, 0x60, 0x00, "10000 30000"		},
	{0x12, 0x01, 0x60, 0x20, "20000 40000"		},
	{0x12, 0x01, 0x60, 0x40, "30000 60000"		},
	{0x12, 0x01, 0x60, 0x60, "40000 80000"		},
};

STDDIPINFO(Wiz)

void __fastcall wiz_main_write(UINT16 address, UINT8 data)
{
	static INT32 lastboom = 0;

	switch (address)
	{
		case 0xc800:
		case 0xc801: // coin counter
		return;

		case 0xf000:
			*sprite_bank = data;
		return;

		case 0xf001:
			interrupt_enable[0] = data;
		return;

		case 0xf002:
		case 0xf003:
			palette_bank[address & 1] = data & 1;
		return;

		case 0xf004:
		case 0xf005:
			char_bank_select[address & 1] = data & 1;
		return;

		case 0xf006: // x
		case 0xf007: // y
			screen_flip[address & 1] = data;
			//bprintf(PRINT_NORMAL, _T("address %04d screen_flip %04d\n"),address,data );

		return;

		case 0xf008:
		case 0xf009:
		case 0xf00a:
		case 0xf00b:
		case 0xf00c:
		case 0xf00d:
		case 0xf00e:
		case 0xf00f: // nop
		return;

		case 0xf800:
			*soundlatch = data;
		return;

		case 0xf808: { // Explosions!
			switch (ZetGetPC(-1)) {
				case 0x3394: if (!BurnSampleGetStatus(2)) BurnSamplePlay(2); break; // plr death

    			default: { // enemy death (scion/scionc)
					if (lastboom + 1 == nCurrentFrame || lastboom == nCurrentFrame) {
						lastboom = nCurrentFrame;
					} else {
						BurnSamplePlay(1);
						lastboom = nCurrentFrame;
					}
					break;
				}
			}
		}
		return;

		case 0xf80a: {
			// Pew! Pew!
			BurnSamplePlay(0);
			lastboom = 0;
		}
		return;

		case 0xf818:
			*background_color = data;
		return;
	}
}

UINT8 __fastcall wiz_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xf000:
			return DrvDips[0];

		case 0xf008:
			return DrvDips[1];

		case 0xf010:
			return DrvInputs[0];

		case 0xf018:
			return DrvInputs[1];

		case 0xf800:
			return 0; // watchdog
	}

	// Wiz protection
	if ((address & 0xfc00) == 0xd400)
	{
		if ((address & 0xff) == 0)
		{
			switch (DrvColRAM1[0])
			{
				case 0x35:
					return 0x25;

				case 0x8f:
					return 0x1f;

				case 0xa0:
					return 0x00;
			}
		}

		return DrvColRAM1[address & 0x3ff];
	}

	return 0;
}

void __fastcall wiz_sound_write(UINT16 address, UINT8 data)
{
	address &= 0x7fff;

	switch (address)
	{
		case 0x3000:
		case 0x7000:
			interrupt_enable[1] = data;
		return;

		case 0x4000:
		case 0x4001: if (Wizmode)
			AY8910Write(2, address & 1, data);
		return;

		case 0x5000:
		case 0x5001:
			AY8910Write(0, address & 1, data);
		return;

		case 0x6000:
		case 0x6001:
			AY8910Write(1, address & 1, data);
		return;
	}
}

UINT8 __fastcall wiz_sound_read(UINT16 address)
{
	address &= 0x7fff;

	switch (address)
	{
		case 0x3000:
		case 0x7000:
			return *soundlatch;
	}

	return 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	ZetClose();

	AY8910Reset(0);
	AY8910Reset(1);
	AY8910Reset(2);

	BurnSampleReset();

	return 0;
}

static void DrvPaletteInit()
{
	for (UINT32 i = 0; i < 0x100; i++)
	{
		UINT32 bit0 = (DrvColPROM[0x000 + i] >> 0) & 0x01;
		UINT32 bit1 = (DrvColPROM[0x000 + i] >> 1) & 0x01;
		UINT32 bit2 = (DrvColPROM[0x000 + i] >> 2) & 0x01;
		UINT32 bit3 = (DrvColPROM[0x000 + i] >> 3) & 0x01;
		UINT32 r = 0x0e * bit0 + 0x1f * bit1 + 0x42 * bit2 + 0x90 * bit3;

		bit0 = (DrvColPROM[0x100 + i] >> 0) & 0x01;
		bit1 = (DrvColPROM[0x100 + i] >> 1) & 0x01;
		bit2 = (DrvColPROM[0x100 + i] >> 2) & 0x01;
		bit3 = (DrvColPROM[0x100 + i] >> 3) & 0x01;
		UINT32 g = 0x0e * bit0 + 0x1f * bit1 + 0x42 * bit2 + 0x90 * bit3;

		bit0 = (DrvColPROM[0x200 + i] >> 0) & 0x01;
		bit1 = (DrvColPROM[0x200 + i] >> 1) & 0x01;
		bit2 = (DrvColPROM[0x200 + i] >> 2) & 0x01;
		bit3 = (DrvColPROM[0x200 + i] >> 3) & 0x01;
		UINT32 b = 0x0e * bit0 + 0x1f * bit1 + 0x42 * bit2 + 0x90 * bit3;

		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}
}


static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x010000;
	DrvZ80Dec		= Next; Next += 0x010000;
	DrvZ80ROM1		= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += 6 *  8 *  8 * 256; // 6 banks,   8x8 tiles, 256 tiles (characters)
	DrvGfxROM1		= Next; Next += 3 * 16 * 16 * 256; // 3 banks, 16x16 tiles, 256 tiles (sprites)

	DrvColPROM		= Next; Next += 0x000300;

	DrvPalette		= (unsigned int*)Next; Next += 0x0100 * sizeof(INT32);

	AllRam			= Next;

	DrvZ80RAM0		= Next; Next += 0x000800;
	DrvZ80RAM1		= Next; Next += 0x000400;

	DrvVidRAM0		= Next; Next += 0x000400;
	DrvVidRAM1		= Next; Next += 0x000400;

	DrvColRAM0		= Next; Next += 0x000400;
	DrvColRAM1		= Next; Next += 0x000400;

	DrvSprRAM0		= Next; Next += 0x000100;
	DrvSprRAM1		= Next; Next += 0x000100;

	soundlatch		= Next; Next += 0x000001;

	sprite_bank		= Next; Next += 0x000001;
	interrupt_enable= Next; Next += 0x000002;
	palette_bank	= Next; Next += 0x000002;
	char_bank_select= Next; Next += 0x000002;
	screen_flip		= Next; Next += 0x000002;
	background_color= Next; Next += 0x000001;

	RamEnd			= Next;

	{
		for (INT32 i = 0; i < 9; i++) {
			pAY8910Buffer[i] = (INT16*)Next; Next += nBurnSoundLen * sizeof(UINT16);
		}
	}

	MemEnd			= Next;

	return 0;
}

static void DrvGfxDecode(UINT32 type)
{
	INT32 Plane[3]  = { 0x4000*8, 0x2000*8, 0 };
	INT32 XOffs[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 8*8+0, 8*8+1, 8*8+2, 8*8+3, 8*8+4, 8*8+5, 8*8+6, 8*8+7 };
	INT32 YOffs[16] = { 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8, 16*8, 17*8, 18*8, 19*8, 20*8, 21*8, 22*8, 23*8 };

	UINT8 *tmp0 = (UINT8*)malloc(0x6000);
	UINT8 *tmp1 = (UINT8*)malloc(0xc000);

	memcpy (tmp0, DrvGfxROM0, 0x6000);
	memcpy (tmp1, DrvGfxROM1, 0xc000);
	memset (DrvGfxROM0, 0, 0x6000);
	memset (DrvGfxROM1, 0, 0xc000);

	GfxDecode(256, 3,  8,  8, Plane, XOffs, YOffs, 0x040, tmp0 + 0x0000, DrvGfxROM0 + 0 *  8 *  8 * 256);
	GfxDecode(256, 3,  8,  8, Plane, XOffs, YOffs, 0x040, tmp0 + 0x0800, DrvGfxROM0 + 1 *  8 *  8 * 256);
	if (type == 0)
	{
		GfxDecode(256, 3,  8,  8, Plane, XOffs, YOffs, 0x040, tmp1 + 0x6000, DrvGfxROM0 + 2 *  8 *  8 * 256);
		GfxDecode(256, 3,  8,  8, Plane, XOffs, YOffs, 0x040, tmp1 + 0x0000, DrvGfxROM0 + 3 *  8 *  8 * 256);
		GfxDecode(256, 3,  8,  8, Plane, XOffs, YOffs, 0x040, tmp1 + 0x0800, DrvGfxROM0 + 4 *  8 *  8 * 256);
		GfxDecode(256, 3,  8,  8, Plane, XOffs, YOffs, 0x040, tmp1 + 0x6800, DrvGfxROM0 + 5 *  8 *  8 * 256);
	} else {
		GfxDecode(256, 3,  8,  8, Plane, XOffs, YOffs, 0x040, tmp1 + 0x0000, DrvGfxROM0 + 2 *  8 *  8 * 256);
		GfxDecode(256, 3,  8,  8, Plane, XOffs, YOffs, 0x040, tmp1 + 0x0800, DrvGfxROM0 + 3 *  8 *  8 * 256);
	}

	GfxDecode(256, 3, 16, 16, Plane, XOffs, YOffs, 0x100, tmp0 + 0x0000, DrvGfxROM1 + 0 * 16 * 16 * 256);
	GfxDecode(256, 3, 16, 16, Plane, XOffs, YOffs, 0x100, tmp1 + 0x0000, DrvGfxROM1 + 1 * 16 * 16 * 256);
	GfxDecode(256, 3, 16, 16, Plane, XOffs, YOffs, 0x100, tmp1 + 0x6000, DrvGfxROM1 + 2 * 16 * 16 * 256);

	free (tmp0);
	free (tmp1);
}

static INT32 WizLoadRoms()
{
	if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM0 + 0x04000,  1, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM0 + 0x08000,  2, 1)) return 1;

	if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  3, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x00000,  7, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x04000,  8, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x08000,  9, 1)) return 1;

	for (UINT32 i = 0; i < 0xc000; i++) {
		DrvGfxROM1[((i & 0x2000) * 3) + ((i & 0xc000) >> 1) + (i & 0x1fff)] = DrvGfxROM0[i];
	}

	if (BurnLoadRom(DrvGfxROM0 + 0x00000,  4, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x02000,  5, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x04000,  6, 1)) return 1;

	if (BurnLoadRom(DrvColPROM + 0x00000, 10, 1)) return 1;
	if (BurnLoadRom(DrvColPROM + 0x00100, 11, 1)) return 1;
	if (BurnLoadRom(DrvColPROM + 0x00200, 12, 1)) return 1;

	DrvGfxDecode(0);

	return 0;
}

static INT32 KungfutLoadRoms()
{
	if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM0 + 0x04000,  1, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM0 + 0x08000,  2, 1)) return 1;

	if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  3, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x00000,  4, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x02000,  5, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x04000,  6, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM1 + 0x00000,  7, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x02000,  8, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x04000,  9, 1)) return 1;

	if (BurnLoadRom(DrvColPROM + 0x00000, 10, 1)) return 1;
	if (BurnLoadRom(DrvColPROM + 0x00100, 11, 1)) return 1;
	if (BurnLoadRom(DrvColPROM + 0x00200, 12, 1)) return 1;

	DrvGfxDecode(1);

	return 0;
}

static INT32 StingerLoadRoms()
{
	if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM0 + 0x02000,  1, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM0 + 0x04000,  2, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM0 + 0x06000,  3, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM0 + 0x08000,  4, 1)) return 1;

	if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  5, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM0 + 0x00000,  6, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x02000,  7, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM0 + 0x04000,  8, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM1 + 0x00000,  9, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x02000, 10, 1)) return 1;
	if (BurnLoadRom(DrvGfxROM1 + 0x04000, 11, 1)) return 1;

	if (BurnLoadRom(DrvColPROM + 0x00000, 12, 1)) return 1;
	if (BurnLoadRom(DrvColPROM + 0x00100, 13, 1)) return 1;
	if (BurnLoadRom(DrvColPROM + 0x00200, 14, 1)) return 1;

	DrvGfxDecode(1);

	return 0;
}

static INT32 DrvInit(int (*RomLoadCallback)())
{
	AllMem = NULL;
	MemIndex();
	UINT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)malloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (RomLoadCallback()) return 1;

		DrvPaletteInit();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0, 0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0, 0xc000, 0xc7ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM1, 0xd000, 0xd3ff, MAP_RAM);
	ZetMapMemory(DrvColRAM1, 0xd400, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM1, 0xd800, 0xd8ff, MAP_RAM); // 00 - 3f attributs, 40-5f sprites, 60+ junk
	ZetMapMemory(DrvVidRAM0, 0xe000, 0xe3ff, MAP_RAM);
	ZetMapMemory(DrvColRAM0, 0xe400, 0xe7ff, MAP_RAM); //just ram?
	ZetMapMemory(DrvSprRAM0, 0xe800, 0xe8ff, MAP_RAM); // 00 - 3f attributs, 40-5f sprites, 60+ junk

	ZetSetWriteHandler(wiz_main_write);
	ZetSetReadHandler(wiz_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1, 0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1, 0x2000, 0x23ff, MAP_RAM);
	ZetSetWriteHandler(wiz_sound_write);
	ZetSetReadHandler(wiz_sound_read);
	ZetClose();

	AY8910Init(0, 1536000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910Init(1, 1536000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910Init(2, 1536000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.10, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.10, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(2, 0.10, BURN_SND_ROUTE_BOTH);

	BurnSampleInit(1);
	BurnSampleSetAllRoutesAllSamples(0.05, BURN_SND_ROUTE_BOTH);
    bHasSamples = BurnSampleGetStatus(0) != -1;

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
	AY8910Exit(2);
	BurnSampleExit();

	free (AllMem);
	AllMem = NULL;

	Wizmode = 0;
	Scionmodeoffset = 0;
	bHasSamples = 0;

	return 0;
}


static void draw_background(INT16 bank, INT16 palbank, INT16 colortype)
{
	for (INT16 offs = 0x3ff; offs >= 0; offs--)
	{
		INT16 sx    = (offs & 0x1f);
		UINT8 sy = (((offs / 32)<<3) - DrvSprRAM0[2 * sx + 0]) &0xff;
		INT16 color;

		if (colortype) 
		{
			color = (DrvSprRAM0[2 * sx | 1] & 0x07) | (palbank << 3);
		}
		else 
		{
			color = (DrvSprRAM0[2 * sx + 1] & 0x04) | (DrvVidRAM0[offs] & 3) | (palbank << 3);
		}

		INT16 code  = DrvVidRAM0[offs] | (bank << 8);

 
		if (screen_flip[1]) { // flipy
			if (screen_flip[0]) { // flipx
				Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, code, (sx << 3) ^ 0xf8, sy - 16, color, 3, 0, 0, DrvGfxROM0);
			} else {
				Render8x8Tile_Mask_FlipY_Clip(pTransDraw, code, sx << 3, sy - 16, color, 3, 0, 0, DrvGfxROM0);
			}
		} else {
			if (screen_flip[0]) { // flipx
				Render8x8Tile_Mask_FlipX_Clip(pTransDraw, code, (sx << 3) ^ 0xf8, sy - 16, color, 3, 0, 0, DrvGfxROM0);
			} else {
				Render8x8Tile_Mask_Clip(pTransDraw, code, (sx << 3)-Scionmodeoffset, sy - 16, color, 3, 0, 0, DrvGfxROM0);
			}
		}
 
	}
}

static void draw_foreground(INT16 palbank, INT16 colortype)
{
	for (INT16 offs = 0x3ff; offs >= 0; offs--)
	{
		INT16 sx    = (offs & 0x1f);
		UINT8 sy    = (((offs / 32)<<3) - DrvSprRAM1[2 * sx + 0]);
 		INT16 code  = DrvVidRAM1[offs] | (char_bank_select[1] << 8);
		INT16 color = DrvColRAM1[sx << 1 | 1] & 7;
		INT16 scroll;

		if (colortype)
		{
			color = (DrvSprRAM1[2 * sx + 1] & 0x07);
		}
		else
		{
			color = (DrvColRAM1[offs] & 0x07);
		}

		color |= (palbank << 3);
		scroll = (8*sy + 256 - DrvVidRAM1[2 * sx]) % 256;
		if (screen_flip[1])
		{
			scroll = (248 - scroll) % 256;
		}

		if (screen_flip[0]) sx = 31 - sx;

		Render8x8Tile_Mask_Clip(pTransDraw, code, (sx << 3)-Scionmodeoffset, sy-16, color, 3, 0, 0, DrvGfxROM0);
	}
}

static void draw_sprites(UINT8 *ram, INT16 palbank, INT16 bank)
{
	for (INT16 offs = 0x1c; offs >= 0; offs -= 4)
	{
		INT16 sy =    240 - ram[offs + 0];
		INT16 code  = ram[offs + 1] | (bank << 8);
		INT16 color = (ram[offs + 2] & 0x07) | (palbank << 3);
		INT16 sx =    ram[offs + 3];
		if (!sx || !sy) continue;

		if (screen_flip[1]) { // flipy
			if (screen_flip[0]) { // flipx
				Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, 240 - sx, (240 - sy) - 16, color, 3, 0, 0, DrvGfxROM1);
			} else {
				Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, (240 - sy) - 16, color, 3, 0, 0, DrvGfxROM1);
			}
		} else {
			if (screen_flip[0]) { // flipx
				Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, 240 - sx, sy - 16, color, 3, 0, 0, DrvGfxROM1);
			} else {
				Render16x16Tile_Mask_Clip(pTransDraw, code, sx-Scionmodeoffset, sy - 16, color, 3, 0, 0, DrvGfxROM1);
			}
		}
	}
}

static INT32 DrvDraw()
{
	INT16 palbank = (palette_bank[0] << 0) | (palette_bank[1] << 1);

	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
		pTransDraw[i] = *background_color;
	} 

	draw_background(2 + ((char_bank_select[0] << 1) | char_bank_select[1]), palbank, 0);
	draw_foreground(palbank, 0);

	draw_sprites(DrvSprRAM1 + 0x40, palbank, 0);
	draw_sprites(DrvSprRAM0 + 0x40, palbank, 1 + *sprite_bank);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 StingerDraw()
{
	INT16 palbank = (palette_bank[0] << 0) | (palette_bank[1] << 1);

	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
		pTransDraw[i] = *background_color;
	}

	draw_background(2 + char_bank_select[0], palbank, 1);
	draw_foreground(palbank, 1);

	draw_sprites(DrvSprRAM1 + 0x40, palbank, 0);
	draw_sprites(DrvSprRAM0 + 0x40, palbank, 1);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 KungfutDraw()
{
	INT16 palbank = (palette_bank[0] << 0) | (palette_bank[1] << 1);

	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
		pTransDraw[i] = *background_color;
	}

	draw_background(2 + char_bank_select[0], palbank, 0);
	draw_foreground(palbank, 0);

	draw_sprites(DrvSprRAM1 + 0x40, palbank, 0);
	draw_sprites(DrvSprRAM0 + 0x40, palbank, 1);

	BurnTransferCopy(DrvPalette);
	
	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0, 2);
		for (INT16 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 16;
	INT32 nCyclesTotal[2] = { 3072000 / 60, 3072000 / 60 };
	INT32 nCyclesDone[2]  = { 0, 0 };
	INT32 nSoundBufferPos = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);
		if (i == (nInterleave - 1) && interrupt_enable[0]) ZetNmi();
		ZetClose();

		ZetOpen(1);
		nCyclesDone[1] += ZetRun(nCyclesTotal[1] / nInterleave);
		if ((i % 4) == 0x03 && interrupt_enable[1]) ZetNmi();
		ZetClose();

		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			AY8910Render(&pAY8910Buffer[0], pSoundBuf, nSegmentLength, 0);
			if (bHasSamples) BurnSampleRender(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			AY8910Render(&pAY8910Buffer[0], pSoundBuf, nSegmentLength, 0);
			if (bHasSamples) BurnSampleRender(pSoundBuf, nSegmentLength);
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

		AY8910Scan(nAction, pnMin);
		BurnSampleScan(nAction, pnMin);
	}

	return 0;
}

static struct BurnSampleInfo stingerSampleDesc[] = {
#if !defined ROM_VERIFY
	{"pewpew",      SAMPLE_NOLOOP   },
	{"boomshort",   SAMPLE_NOLOOP   },
	{"boomlong",    SAMPLE_NOLOOP   },
#endif
	{"",            0               }
};

STD_SAMPLE_PICK(stinger)
STD_SAMPLE_FN(stinger)

// Wiz

static struct BurnRomInfo wizRomDesc[] = {
	{ "ic07_01.bin",	0x4000, 0xc05f2c78, 1 }, //  0 maincpu
	{ "ic05_03.bin",	0x4000, 0x7978d879, 1 }, //  1
	{ "ic06_02.bin",	0x4000, 0x9c406ad2, 1 }, //  2

	{ "ic57_10.bin",	0x2000, 0x8a7575bd, 2 }, //  3 audiocpu

	{ "ic12_04.bin",	0x2000, 0x8969acdd, 3 }, //  4 gfx1
	{ "ic13_05.bin",	0x2000, 0x2868e6a5, 3 }, //  5
	{ "ic14_06.bin",	0x2000, 0xb398e142, 3 }, //  6

	{ "ic03_07.bin",	0x4000, 0x297c02fc, 4 }, //  7 gfx2
	{ "ic02_08.bin",	0x4000, 0xede77d37, 4 }, //  8
	{ "ic01_09.bin",	0x4000, 0x4d86b041, 4 }, //  9

	{ "ic23_3-1.bin",	0x0100, 0x2dd52fb2, 5 }, // 10 proms
	{ "ic23_3-2.bin",	0x0100, 0x8c2880c9, 5 }, // 11
	{ "ic23_3-3.bin",	0x0100, 0xa488d761, 5 }, // 12
};

STD_ROM_PICK(wiz)
STD_ROM_FN(wiz)

static INT32 WizInit()
{
	Wizmode = 1;

	return DrvInit(WizLoadRoms);
}

struct BurnDriver BurnDrvWiz = {
	"wiz", NULL, NULL, NULL, "1985",
	"Wiz\0", NULL, "Seibu Kaihatsu Inc.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, wizRomInfo, wizRomName, NULL, NULL, WizInputInfo, WizDIPInfo,
	WizInit, DrvExit, DrvFrame, DrvDraw, DrvScan, 
	&DrvRecalc, 0x100, 224, 256, 3, 4
};

// Wiz (Taito, set 1)

static struct BurnRomInfo wiztRomDesc[] = {
	{ "wiz1.bin",		0x4000, 0x5a6d3c60, 1 }, //  0 maincpu
	{ "ic05_03.bin",	0x4000, 0x7978d879, 1 }, //  1
	{ "ic06_02.bin",	0x4000, 0x9c406ad2, 1 }, //  2

	{ "ic57_10.bin",	0x2000, 0x8a7575bd, 2 }, //  3 audiocpu

	{ "wiz4.bin",		0x2000, 0xe6c636b3, 3 }, //  4 gfx1
	{ "wiz5.bin",		0x2000, 0x77986058, 3 }, //  5
	{ "wiz6.bin",		0x2000, 0xf6970b23, 3 }, //  6

	{ "wiz7.bin",		0x4000, 0x601f2f3f, 4 }, //  7 gfx2
	{ "wiz8.bin",		0x4000, 0xf5ab982d, 4 }, //  8
	{ "wiz9.bin",		0x4000, 0xf6c662e2, 4 }, //  9

	{ "ic23_3-1.bin",	0x0100, 0x2dd52fb2, 5 }, // 10 proms
	{ "ic23_3-2.bin",	0x0100, 0x8c2880c9, 5 }, // 11
	{ "ic23_3-3.bin",	0x0100, 0xa488d761, 5 }, // 12
};

STD_ROM_PICK(wizt)
STD_ROM_FN(wizt)

struct BurnDriver BurnDrvWizt = {
	"wizt", "wiz", NULL, NULL, "1985",
	"Wiz (Taito, set 1)\0", NULL, "[Seibu] (Taito license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, wiztRomInfo, wiztRomName, NULL, NULL, WizInputInfo, WizDIPInfo,
	WizInit, DrvExit, DrvFrame, DrvDraw, DrvScan, 
	&DrvRecalc, 0x100, 224, 256, 3, 4
};


// Wiz (Taito, set 2)

static struct BurnRomInfo wiztaRomDesc[] = {
	{ "ic7",		0x4000, 0xb2ec49ad, 1 }, //  0 maincpu
	{ "ic5",		0x4000, 0xf7e8f792, 1 }, //  1
	{ "ic6",		0x4000, 0x9c406ad2, 1 }, //  2

	{ "ic57",		0x2000, 0x8a7575bd, 2 }, //  3 audiocpu

	{ "ic12",		0x2000, 0xe6c636b3, 3 }, //  4 gfx1
	{ "ic13",		0x2000, 0x77986058, 3 }, //  5
	{ "ic14",		0x2000, 0xf6970b23, 3 }, //  6

	{ "ic3",		0x4000, 0x601f2f3f, 4 }, //  7 gfx2
	{ "ic2",		0x4000, 0xf5ab982d, 4 }, //  8
	{ "ic1",		0x4000, 0xf6c662e2, 4 }, //  9

	{ "ic23_3-1.bin",	0x0100, 0x2dd52fb2, 5 }, // 10 proms
	{ "ic23_3-2.bin",	0x0100, 0x8c2880c9, 5 }, // 11
	{ "ic23_3-3.bin",	0x0100, 0xa488d761, 5 }, // 12
};

STD_ROM_PICK(wizta)
STD_ROM_FN(wizta)

struct BurnDriver BurnDrvWizta = {
	"wizta", "wiz", NULL, NULL, "1985",
	"Wiz (Taito, set 2)\0", NULL, "[Seibu] (Taito license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, wiztaRomInfo, wiztaRomName, NULL, NULL, WizInputInfo, WizDIPInfo,
	WizInit, DrvExit, DrvFrame, DrvDraw, DrvScan, 
	&DrvRecalc, 0x100, 224, 256, 3, 4
};

// Kung-Fu Taikun

static struct BurnRomInfo kungfutRomDesc[] = {
	{ "p1.bin",	0x4000, 0xb1e56960, 1 }, //  0 maincpu
	{ "p3.bin",	0x4000, 0x6fc346f8, 1 }, //  1
	{ "p2.bin",	0x4000, 0x042cc9c5, 1 }, //  2

	{ "1.bin",	0x2000, 0x68028a5d, 2 }, //  3 audiocpu

	{ "2.bin",	0x2000, 0x5c3ef697, 3 }, //  4 gfx1
	{ "3.bin",	0x2000, 0x905e81fa, 3 }, //  5
	{ "4.bin",	0x2000, 0x965bb5d1, 3 }, //  6

	{ "5.bin",	0x2000, 0x763bb61a, 4 }, //  7 gfx2
	{ "6.bin",	0x2000, 0xc9649fce, 4 }, //  8
	{ "7.bin",	0x2000, 0x32f02c13, 4 }, //  9

	{ "82s129.0",	0x0100, 0xeb823177, 5 }, // 10 proms
	{ "82s129.1",	0x0100, 0x6eec5dd9, 5 }, // 11
	{ "82s129.2",	0x0100, 0xc31eb3e6, 5 }, // 12
};

STD_ROM_PICK(kungfut)
STD_ROM_FN(kungfut)

static INT32 KungfutInit()
{
	Wizmode = 1;
	return DrvInit(KungfutLoadRoms);
}

struct BurnDriver BurnDrvKungfut = {
	"kungfut", NULL, NULL, NULL, "1984",
	"Kung-Fu Taikun\0", NULL, "Seibu Kaihatsu Inc.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, kungfutRomInfo, kungfutRomName, NULL, NULL, KungfutInputInfo, KungfutDIPInfo,
	KungfutInit, DrvExit, DrvFrame, KungfutDraw, DrvScan,
	&DrvRecalc, 0x100, 256, 224, 4, 3
};

// Kung-Fu Taikun (alt)

static struct BurnRomInfo kungfutaRomDesc[] = {
	{ "kungfu.01",	0x4000, 0x48dada70, 1 }, //  0 maincpu
	{ "kungfu.02",	0x4000, 0xc08c5152, 1 }, //  1
	{ "kungfu.03",	0x4000, 0x09b8670c, 1 }, //  2

	{ "kungfu.04",	0x2000, 0x352bff48, 2 }, //  3 audiocpu

	{ "kungfu.08",	0x2000, 0x60b91d2f, 3 }, //  4 gfx1
	{ "kungfu.09",	0x2000, 0x121ba029, 3 }, //  5
	{ "kungfu.10",	0x2000, 0x146df9de, 3 }, //  6

	{ "kungfu.07",	0x2000, 0x1df48de5, 4 }, //  7 gfx2
	{ "kungfu.06",	0x2000, 0x1921d49b, 4 }, //  8
	{ "kungfu.05",	0x2000, 0xff9aced4, 4 }, //  9

	{ "82s129.0",	0x0100, 0xeb823177, 5 }, // 10 proms
	{ "82s129.1",	0x0100, 0x6eec5dd9, 5 }, // 11
	{ "82s129.2",	0x0100, 0xc31eb3e6, 5 }, // 12
};

STD_ROM_PICK(kungfuta)
STD_ROM_FN(kungfuta)

struct BurnDriver BurnDrvKungfuta = {
	"kungfuta", "kungfut", NULL, NULL, "1984",
	"Kung-Fu Taikun (alt)\0", NULL, "Seibu Kaihatsu Inc.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, kungfutaRomInfo, kungfutaRomName, NULL, NULL, KungfutInputInfo, KungfutDIPInfo,
	KungfutInit, DrvExit, DrvFrame, KungfutDraw, DrvScan,
	&DrvRecalc, 0x100, 256, 224, 4, 3
};

// Stinger

static struct BurnRomInfo stingerRomDesc[] = {
	{ "1-5j.bin",	0x2000, 0x1a2ca600, 1 }, //  0 maincpu
	{ "2-6j.bin",	0x2000, 0x957cd39c, 1 }, //  1
	{ "3-8j.bin",	0x2000, 0x404c932e, 1 }, //  2
	{ "4-9j.bin",	0x2000, 0x2d570f91, 1 }, //  3
	{ "5-10j.bin",	0x2000, 0xc841795c, 1 }, //  4

	{ "6-9f.bin",	0x2000, 0x79757f0c, 2 }, //  5 audiocpu

	{ "7-9e.bin",	0x2000, 0x775489be, 3 }, //  6 gfx1
	{ "8-11e.bin",	0x2000, 0x43c61b3f, 3 }, //  7
	{ "9-14e.bin",	0x2000, 0xc9ed8fc7, 3 }, //  8

	{ "10-9h.bin",	0x2000, 0x6fc3a22d, 4 }, //  9 gfx2
	{ "11-11h.bin",	0x2000, 0x3df1f57e, 4 }, // 10
	{ "12-14h.bin",	0x2000, 0x2fbe1391, 4 }, // 11

	{ "stinger.a7",	0x0100, 0x52c06fc2, 5 }, // 12 proms
	{ "stinger.b7",	0x0100, 0x9985e575, 5 }, // 13
	{ "stinger.a8",	0x0100, 0x76b57629, 5 }, // 14
};

STD_ROM_PICK(stinger)
STD_ROM_FN(stinger)

static void StingerDecode()
{
	INT32 swap_xor_table[4][4] =
	{
		{ 7,3,5, 0xa0 },
		{ 3,7,5, 0x88 },
		{ 5,3,7, 0x80 },
		{ 5,7,3, 0x28 }
	};

	for (INT32 A = 0; A < 0x10000; A++)
	{
		if (A & 0x2040)
		{
			DrvZ80Dec[A] = DrvZ80ROM0[A];
		}
		else
		{
			INT32 *tbl = swap_xor_table[((A >> 3) & 1) + (((A >> 5) & 1) << 1)];

			DrvZ80Dec[A] = BITSWAP08(DrvZ80ROM0[A],tbl[0],6,tbl[1],4,tbl[2],2,1,0) ^ tbl[3];
		}
	}

	ZetOpen(0);
	ZetMapArea(0x0000, 0xbfff, 2, DrvZ80Dec, DrvZ80ROM0);
	ZetClose();
}

static INT32 StingerInit()
{
	INT32 nRet = DrvInit(StingerLoadRoms);

	if (nRet == 0) {
		StingerDecode();
	}

	return nRet;
}

struct BurnDriver BurnDrvStinger = {
	"stinger", NULL, NULL, "stinger", "1983",
	"Stinger\0", NULL, "Seibu Denshi", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, stingerRomInfo, stingerRomName, stingerSampleInfo, stingerSampleName, StingerInputInfo, StingerDIPInfo,
	StingerInit, DrvExit, DrvFrame, StingerDraw, DrvScan,
	&DrvRecalc, 0x100, 224, 256, 3, 4
};

// Stinger (prototype?)

static struct BurnRomInfo stinger2RomDesc[] = {
	{ "n1.bin",	0x2000, 0xf2d2790c, 1 }, //  0 maincpu
	{ "n2.bin",	0x2000, 0x8fd2d8d8, 1 }, //  1
	{ "n3.bin",	0x2000, 0xf1794d36, 1 }, //  2
	{ "n4.bin",	0x2000, 0x230ba682, 1 }, //  3
	{ "n5.bin",	0x2000, 0xa03a01da, 1 }, //  4

	{ "6-9f.bin",	0x2000, 0x79757f0c, 2 }, //  5 audiocpu

	{ "7-9e.bin",	0x2000, 0x775489be, 3 }, //  6 gfx1
	{ "8-11e.bin",	0x2000, 0x43c61b3f, 3 }, //  7
	{ "9-14e.bin",	0x2000, 0xc9ed8fc7, 3 }, //  8

	{ "10.bin",	0x2000, 0xf6721930, 4 }, //  9 gfx2
	{ "11.bin",	0x2000, 0xa4404e63, 4 }, // 10
	{ "12.bin",	0x2000, 0xb60fa88c, 4 }, // 11

	{ "stinger.a7",	0x0100, 0x52c06fc2, 5 }, // 12 proms
	{ "stinger.b7",	0x0100, 0x9985e575, 5 }, // 13
	{ "stinger.a8",	0x0100, 0x76b57629, 5 }, // 14
};

STD_ROM_PICK(stinger2)
STD_ROM_FN(stinger2)

struct BurnDriver BurnDrvStinger2 = {
	"stinger2", "stinger", NULL, "stinger", "1983",
	"Stinger (prototype?)\0", NULL, "Seibu Denshi", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, stinger2RomInfo, stinger2RomName, stingerSampleInfo, stingerSampleName, StingerInputInfo, Stinger2DIPInfo,
	StingerInit, DrvExit, DrvFrame, StingerDraw, DrvScan,
	&DrvRecalc, 0x100, 224, 256, 3, 4
};

// Scion

static struct BurnRomInfo scionRomDesc[] = {
	{ "sc1",	0x2000, 0x8dcad575, 1 }, //  0 maincpu
	{ "sc2",	0x2000, 0xf608e0ba, 1 }, //  1
	{ "sc3",	0x2000, 0x915289b9, 1 }, //  2
	{ "4.9j",	0x2000, 0x0f40d002, 1 }, //  3
	{ "5.10j",	0x2000, 0xdc4923b7, 1 }, //  4

	{ "sc6",	0x2000, 0x09f5f9c1, 2 }, //  5 audiocpu

	{ "7.10e",	0x2000, 0x223e0d2a, 3 }, //  6 gfx1
	{ "8.12e",	0x2000, 0xd3e39b48, 3 }, //  7
	{ "9.15e",	0x2000, 0x630861b5, 3 }, //  8

	{ "10.10h",	0x2000, 0x0d2a0d1e, 4 }, //  9 gfx2
	{ "11.12h",	0x2000, 0xdc6ef8ab, 4 }, // 10
	{ "12.15h",	0x2000, 0xc82c28bf, 4 }, // 11

	{ "82s129.7a",	0x0100, 0x2f89d9ea, 5 }, // 12 proms
	{ "82s129.7b",	0x0100, 0xba151e6a, 5 }, // 13
	{ "82s129.8a",	0x0100, 0xf681ce59, 5 }, // 14
};

STD_ROM_PICK(scion)
STD_ROM_FN(scion)

static INT32 ScionInit()
{
	Scionmodeoffset = 8*2; // 2 8x8char offset

	return DrvInit(StingerLoadRoms);
}

struct BurnDriver BurnDrvScion = {
	"scion", NULL, NULL, "stinger", "1984",
	"Scion\0", "Music horribly broken, use scionc instead!", "Seibu Denshi", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, scionRomInfo, scionRomName, stingerSampleInfo, stingerSampleName, ScionInputInfo, ScionDIPInfo,
	ScionInit, DrvExit, DrvFrame, StingerDraw, DrvScan,
	&DrvRecalc, 0x100, 240, 224, 4, 3
};

// Scion (Cinematronics)

static struct BurnRomInfo scioncRomDesc[] = {
	{ "1.5j",	0x2000, 0x5aaf571e, 1 }, //  0 maincpu
	{ "2.6j",	0x2000, 0xd5a66ac9, 1 }, //  1
	{ "3.8j",	0x2000, 0x6e616f28, 1 }, //  2
	{ "4.9j",	0x2000, 0x0f40d002, 1 }, //  3
	{ "5.10j",	0x2000, 0xdc4923b7, 1 }, //  4

	{ "6.9f",	0x2000, 0xa66a0ce6, 2 }, //  5 audiocpu

	{ "7.10e",	0x2000, 0x223e0d2a, 3 }, //  6 gfx1
	{ "8.12e",	0x2000, 0xd3e39b48, 3 }, //  7
	{ "9.15e",	0x2000, 0x630861b5, 3 }, //  8

	{ "10.10h",	0x2000, 0x0d2a0d1e, 4 }, //  9 gfx2
	{ "11.12h",	0x2000, 0xdc6ef8ab, 4 }, // 10
	{ "12.15h",	0x2000, 0xc82c28bf, 4 }, // 11

	{ "82s129.7a",	0x0100, 0x2f89d9ea, 5 }, // 12 proms
	{ "82s129.7b",	0x0100, 0xba151e6a, 5 }, // 13
	{ "82s129.8a",	0x0100, 0xf681ce59, 5 }, // 14
};

STD_ROM_PICK(scionc)
STD_ROM_FN(scionc)

struct BurnDriver BurnDrvScionc = {
	"scionc", "scion", NULL, "stinger", "1984",
	"Scion (Cinematronics)\0", NULL, "Seibu Denshi (Cinematronics license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, scioncRomInfo, scioncRomName, stingerSampleInfo, stingerSampleName, ScionInputInfo, ScionDIPInfo,
	ScionInit, DrvExit, DrvFrame, StingerDraw, DrvScan,
	&DrvRecalc, 0x100, 240, 224, 4, 3
};
