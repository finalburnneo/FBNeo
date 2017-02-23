// Gigas / Freekick / Counter Run / Perfect Billiard for FBA, ported by vbt

#include "tiles_generic.h"
#include "z80_intf.h"
#include "driver.h"
#include "bitswap.h"
#include "sn76496.h"
#include "mc8123.h"
#include "8255ppi.h"

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];

static UINT8 DrvInputs[2];
static UINT8 DrvDip[3];
static INT16 DrvDial1;
static INT16 DrvDial2;
static UINT8 DrvReset;

static UINT8 DrvRecalc;

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvRAM;
static UINT8 *DrvMainROM;
static UINT8 *DrvMainROMdec;
static UINT8 *DrvSndROM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvColRAM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *MC8123Key;
static UINT32 *DrvPalette;
static UINT8 DrvZ80Bank0;
typedef void (*RenderSprite)(INT32);
static RenderSprite DrawSprite;

static UINT8 nmi_enable;
static UINT8 flipscreen;
static UINT8 coin;
static UINT8 spinner;
static UINT8 ff_data;
static UINT16 romaddr;

static UINT8 use_encrypted = 0;
static UINT8 countrunbmode = 0;
static UINT8 pbillrdmode = 0;

static void DrvPaletteInit();

static struct BurnInputInfo PbillrdInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 2"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDip + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDip + 1,	"dip"},
};

STDINPUTINFO(Pbillrd)


static struct BurnDIPInfo PbillrdDIPList[]=
{
	{0x11, 0xff, 0xff, 0x1f, NULL		},
	{0x12, 0xff, 0xff, 0xff, NULL		},

	{0   , 0xfe, 0   ,    2, "Balls"		},
	{0x11, 0x01, 0x01, 0x01, "3"		},
	{0x11, 0x01, 0x01, 0x00, "5"		},

	{0   , 0xfe, 0   ,    4, "Bonus Ball"		},
	{0x11, 0x01, 0x06, 0x06, "10000, 30000 & 50000 Points"		},
	{0x11, 0x01, 0x06, 0x02, "20000 & 60000 Points"		},
	{0x11, 0x01, 0x06, 0x04, "30000 & 80000 Points"		},
	{0x11, 0x01, 0x06, 0x00, "Only 20000 Points"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x11, 0x01, 0x10, 0x00, "No"		},
	{0x11, 0x01, 0x10, 0x10, "Yes"		},

	{0   , 0xfe, 0   ,    2, "Shot"		},
	{0x11, 0x01, 0x20, 0x00, "2"		},
	{0x11, 0x01, 0x20, 0x20, "3"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x11, 0x01, 0x40, 0x00, "Upright"		},
	{0x11, 0x01, 0x40, 0x40, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x11, 0x01, 0x80, 0x00, "Off"		},
	{0x11, 0x01, 0x80, 0x80, "On"		},

	{0   , 0xfe, 0   ,   16, "Coin A"		},
	{0x12, 0x01, 0x0f, 0x00, "5 Coins 1 Credits"		},
	{0x12, 0x01, 0x0f, 0x0c, "4 Coins 1 Credits"		},
	{0x12, 0x01, 0x0f, 0x0e, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0x0f, 0x05, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0x0f, 0x06, "3 Coins 2 Credits"		},
	{0x12, 0x01, 0x0f, 0x04, "4 Coins 3 Credits"		},
	{0x12, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0x0f, 0x08, "4 Coins 5 Credits"		},
	{0x12, 0x01, 0x0f, 0x0a, "3 Coins 4 Credits"		},
	{0x12, 0x01, 0x0f, 0x09, "2 Coins 3 Credits"		},
	{0x12, 0x01, 0x0f, 0x02, "3 Coins/5 Credits"		},
	{0x12, 0x01, 0x0f, 0x07, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0x0f, 0x01, "2 Coins 5 Credits"		},
	{0x12, 0x01, 0x0f, 0x0b, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0x0f, 0x03, "1 Coin  4 Credits"		},
	{0x12, 0x01, 0x0f, 0x0d, "1 Coin  5 Credits"		},

	{0   , 0xfe, 0   ,    16, "Coin B"		},
	{0x12, 0x01, 0xf0, 0x00, "5 Coins 1 Credits"		},
	{0x12, 0x01, 0xf0, 0xc0, "4 Coins 1 Credits"		},
	{0x12, 0x01, 0xf0, 0xe0, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0xf0, 0x50, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0xf0, 0x60, "3 Coins 2 Credits"		},
	{0x12, 0x01, 0xf0, 0x40, "4 Coins 3 Credits"		},
	{0x12, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0xf0, 0x80, "4 Coins 5 Credits"		},
	{0x12, 0x01, 0xf0, 0xa0, "3 Coins 4 Credits"		},
	{0x12, 0x01, 0xf0, 0x90, "2 Coins 3 Credits"		},
	{0x12, 0x01, 0xf0, 0x20, "3 Coins/5 Credits"		},
	{0x12, 0x01, 0xf0, 0x70, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0xf0, 0x10, "2 Coins 5 Credits"		},
	{0x12, 0x01, 0xf0, 0xb0, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0xf0, 0x30, "1 Coin  4 Credits"		},
	{0x12, 0x01, 0xf0, 0xd0, "1 Coin  5 Credits"		},
};

STDDIPINFO(Pbillrd)

static struct BurnInputInfo GigasInputList[] = {

	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 start"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 2"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDip + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDip + 1,	"dip"},
};

STDINPUTINFO(Gigas)

static struct BurnDIPInfo GigasDIPList[]=
{
	{0x0d, 0xff, 0xff, 0x3f, NULL		},
	{0x0e, 0xff, 0xff, 0xff, NULL		},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x0d, 0x01, 0x01, 0x01, "3"		},
	{0x0d, 0x01, 0x01, 0x00, "5"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x0d, 0x01, 0x06, 0x06, "20000 & 60000, Every 60000 Points"		},
	{0x0d, 0x01, 0x06, 0x02, "20000 & 60000 Points"		},
	{0x0d, 0x01, 0x06, 0x04, "30000 & 80000, Every 80000 Points"		},
	{0x0d, 0x01, 0x06, 0x00, "Only 20000 Points"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x0d, 0x01, 0x18, 0x18, "Easy"		},
	{0x0d, 0x01, 0x18, 0x10, "Normal"		},
	{0x0d, 0x01, 0x18, 0x08, "Hard"		},
	{0x0d, 0x01, 0x18, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x0d, 0x01, 0x20, 0x00, "No"		},
	{0x0d, 0x01, 0x20, 0x20, "Yes"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0d, 0x01, 0x40, 0x00, "Upright"		},
	{0x0d, 0x01, 0x40, 0x40, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x0d, 0x01, 0x80, 0x00, "Off"		},
	{0x0d, 0x01, 0x80, 0x80, "On"		},

	{0   , 0xfe, 0   ,   16, "Coin A"		},
	{0x0e, 0x01, 0x0f, 0x00, "5 Coins 1 Credits"		},
	{0x0e, 0x01, 0x0f, 0x0c, "4 Coins 1 Credits"		},
	{0x0e, 0x01, 0x0f, 0x0e, "3 Coins 1 Credits"		},
	{0x0e, 0x01, 0x0f, 0x05, "2 Coins 1 Credits"		},
	{0x0e, 0x01, 0x0f, 0x06, "3 Coins 2 Credits"		},
	{0x0e, 0x01, 0x0f, 0x04, "4 Coins 3 Credits"		},
	{0x0e, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"		},
	{0x0e, 0x01, 0x0f, 0x08, "4 Coins 5 Credits"		},
	{0x0e, 0x01, 0x0f, 0x0a, "3 Coins 4 Credits"		},
	{0x0e, 0x01, 0x0f, 0x09, "2 Coins 3 Credits"		},
	{0x0e, 0x01, 0x0f, 0x02, "3 Coins/5 Credits"		},
	{0x0e, 0x01, 0x0f, 0x07, "1 Coin  2 Credits"		},
	{0x0e, 0x01, 0x0f, 0x01, "2 Coins 5 Credits"		},
	{0x0e, 0x01, 0x0f, 0x0b, "1 Coin  3 Credits"		},
	{0x0e, 0x01, 0x0f, 0x03, "1 Coin  4 Credits"		},
	{0x0e, 0x01, 0x0f, 0x0d, "1 Coin  5 Credits"		},

	{0   , 0xfe, 0   ,    16, "Coin B"		},
	{0x0e, 0x01, 0xf0, 0x00, "5 Coins 1 Credits"		},
	{0x0e, 0x01, 0xf0, 0xc0, "4 Coins 1 Credits"		},
	{0x0e, 0x01, 0xf0, 0xe0, "3 Coins 1 Credits"		},
	{0x0e, 0x01, 0xf0, 0x50, "2 Coins 1 Credits"		},
	{0x0e, 0x01, 0xf0, 0x60, "3 Coins 2 Credits"		},
	{0x0e, 0x01, 0xf0, 0x40, "4 Coins 3 Credits"		},
	{0x0e, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x0e, 0x01, 0xf0, 0x80, "4 Coins 5 Credits"		},
	{0x0e, 0x01, 0xf0, 0xa0, "3 Coins 4 Credits"		},
	{0x0e, 0x01, 0xf0, 0x90, "2 Coins 3 Credits"		},
	{0x0e, 0x01, 0xf0, 0x20, "3 Coins/5 Credits"		},
	{0x0e, 0x01, 0xf0, 0x70, "1 Coin  2 Credits"		},
	{0x0e, 0x01, 0xf0, 0x10, "2 Coins 5 Credits"		},
	{0x0e, 0x01, 0xf0, 0xb0, "1 Coin  3 Credits"		},
	{0x0e, 0x01, 0xf0, 0x30, "1 Coin  4 Credits"		},
	{0x0e, 0x01, 0xf0, 0xd0, "1 Coin  5 Credits"		},
};

STDDIPINFO(Gigas)

static struct BurnDIPInfo Gigasm2DIPList[]=
{
	{0x0d, 0xff, 0xff, 0x3f, NULL		},
	{0x0e, 0xff, 0xff, 0xff, NULL		},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x0d, 0x01, 0x01, 0x01, "3"		},
	{0x0d, 0x01, 0x01, 0x00, "5"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x0d, 0x01, 0x06, 0x06, "20000 & 60000, Every 60000 Points"		},
	{0x0d, 0x01, 0x06, 0x02, "20000 & 60000 Points"		},
	{0x0d, 0x01, 0x06, 0x04, "30000 & 80000, Every 80000 Points"		},
	{0x0d, 0x01, 0x06, 0x00, "Only 20000 Points"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x0d, 0x01, 0x18, 0x18, "Easy"		},
	{0x0d, 0x01, 0x18, 0x10, "Normal"		},
	{0x0d, 0x01, 0x18, 0x08, "Hard"		},
	{0x0d, 0x01, 0x18, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x0d, 0x01, 0x20, 0x00, "No"		},
	{0x0d, 0x01, 0x20, 0x20, "Yes"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0d, 0x01, 0x40, 0x00, "Upright"		},
	{0x0d, 0x01, 0x40, 0x40, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x0d, 0x01, 0x80, 0x00, "Off"		},
	{0x0d, 0x01, 0x80, 0x80, "On"		},

	{0   , 0xfe, 0   ,   16, "Coin A"		},
	{0x0e, 0x01, 0x0f, 0x00, "5 Coins 1 Credits"		},
	{0x0e, 0x01, 0x0f, 0x0c, "4 Coins 1 Credits"		},
	{0x0e, 0x01, 0x0f, 0x0e, "3 Coins 1 Credits"		},
	{0x0e, 0x01, 0x0f, 0x05, "2 Coins 1 Credits"		},
	{0x0e, 0x01, 0x0f, 0x06, "3 Coins 2 Credits"		},
	{0x0e, 0x01, 0x0f, 0x04, "4 Coins 3 Credits"		},
	{0x0e, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"		},
	{0x0e, 0x01, 0x0f, 0x08, "4 Coins 5 Credits"		},
	{0x0e, 0x01, 0x0f, 0x0a, "3 Coins 4 Credits"		},
	{0x0e, 0x01, 0x0f, 0x09, "2 Coins 3 Credits"		},
	{0x0e, 0x01, 0x0f, 0x02, "3 Coins/5 Credits"		},
	{0x0e, 0x01, 0x0f, 0x07, "1 Coin  2 Credits"		},
	{0x0e, 0x01, 0x0f, 0x01, "2 Coins 5 Credits"		},
	{0x0e, 0x01, 0x0f, 0x0b, "1 Coin  3 Credits"		},
	{0x0e, 0x01, 0x0f, 0x03, "1 Coin  4 Credits"		},
	{0x0e, 0x01, 0x0f, 0x0d, "1 Coin  5 Credits"		},

	{0   , 0xfe, 0   ,    16, "Coin B"		},
	{0x0e, 0x01, 0xf0, 0x00, "5 Coins 1 Credits"		},
	{0x0e, 0x01, 0xf0, 0xc0, "4 Coins 1 Credits"		},
	{0x0e, 0x01, 0xf0, 0xe0, "3 Coins 1 Credits"		},
	{0x0e, 0x01, 0xf0, 0x50, "2 Coins 1 Credits"		},
	{0x0e, 0x01, 0xf0, 0x60, "3 Coins 2 Credits"		},
	{0x0e, 0x01, 0xf0, 0x40, "4 Coins 3 Credits"		},
	{0x0e, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x0e, 0x01, 0xf0, 0x80, "4 Coins 5 Credits"		},
	{0x0e, 0x01, 0xf0, 0xa0, "3 Coins 4 Credits"		},
	{0x0e, 0x01, 0xf0, 0x90, "2 Coins 3 Credits"		},
	{0x0e, 0x01, 0xf0, 0x20, "3 Coins/5 Credits"		},
	{0x0e, 0x01, 0xf0, 0x70, "1 Coin  2 Credits"		},
	{0x0e, 0x01, 0xf0, 0x10, "2 Coins 5 Credits"		},
	{0x0e, 0x01, 0xf0, 0xb0, "1 Coin  3 Credits"		},
	{0x0e, 0x01, 0xf0, 0x30, "1 Coin  4 Credits"		},
	{0x0e, 0x01, 0xf0, 0xd0, "1 Coin  5 Credits"		},
};

STDDIPINFO(Gigasm2)

static struct BurnInputInfo FreekckInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 2"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 2"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDip + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDip + 1,	"dip"},
	{"Dip C",		BIT_DIPSWITCH,	DrvDip + 2,	"dip"},
};

STDINPUTINFO(Freekck)


static struct BurnDIPInfo FreekckDIPList[]=
{
	{0x11, 0xff, 0xff, 0xbf, NULL		},
	{0x12, 0xff, 0xff, 0xff, NULL		},
	{0x13, 0xff, 0xff, 0xfe, NULL		},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x11, 0x01, 0x01, 0x01, "3"		},
	{0x11, 0x01, 0x01, 0x00, "5"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x11, 0x01, 0x06, 0x06, "2-3-4-5-60000 Points"		},
	{0x11, 0x01, 0x06, 0x02, "3-4-5-6-7-80000 Points"		},
	{0x11, 0x01, 0x06, 0x04, "20000 & 60000 Points"		},
	{0x11, 0x01, 0x06, 0x00, "ONLY 20000 Points"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x11, 0x01, 0x18, 0x18, "Easy"		},
	{0x11, 0x01, 0x18, 0x10, "Normal"		},
	{0x11, 0x01, 0x18, 0x08, "Hard"		},
	{0x11, 0x01, 0x18, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x11, 0x01, 0x20, 0x00, "No"		},
	{0x11, 0x01, 0x20, 0x20, "Yes"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x11, 0x01, 0x40, 0x00, "Upright"		},
	{0x11, 0x01, 0x40, 0x40, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x11, 0x01, 0x80, 0x80, "Off"		},
	{0x11, 0x01, 0x80, 0x00, "On"		},

	{0   , 0xfe, 0   ,    16, "Coin A"		},
	{0x12, 0x01, 0x0f, 0x00, "5 Coins 1 Credits"		},
	{0x12, 0x01, 0x0f, 0x0c, "4 Coins 1 Credits"		},
	{0x12, 0x01, 0x0f, 0x0e, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0x0f, 0x05, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0x0f, 0x06, "3 Coins 2 Credits"		},
	{0x12, 0x01, 0x0f, 0x04, "4 Coins 3 Credits"		},
	{0x12, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0x0f, 0x08, "4 Coins 5 Credits"		},
	{0x12, 0x01, 0x0f, 0x0a, "3 Coins 4 Credits"		},
	{0x12, 0x01, 0x0f, 0x09, "2 Coins 3 Credits"		},
	{0x12, 0x01, 0x0f, 0x02, "3 Coins/5 Credits"		},
	{0x12, 0x01, 0x0f, 0x07, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0x0f, 0x01, "2 Coins 5 Credits"		},
	{0x12, 0x01, 0x0f, 0x0b, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0x0f, 0x03, "1 Coin  4 Credits"		},
	{0x12, 0x01, 0x0f, 0x0d, "1 Coin  5 Credits"		},

	{0   , 0xfe, 0   ,    16, "Coin B"		},
	{0x12, 0x01, 0xf0, 0x00, "5 Coins 1 Credits"		},
	{0x12, 0x01, 0xf0, 0xe0, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0xf0, 0x50, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0xf0, 0x60, "3 Coins 2 Credits"		},
	{0x12, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0xf0, 0xa0, "3 Coins 4 Credits"		},
	{0x12, 0x01, 0xf0, 0x90, "2 Coins 3 Credits"		},
	{0x12, 0x01, 0xf0, 0x20, "3 Coins/5 Credits"		},
	{0x12, 0x01, 0xf0, 0x70, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0xf0, 0x10, "2 Coins 5 Credits"		},
	{0x12, 0x01, 0xf0, 0xb0, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0xf0, 0x30, "1 Coin  4 Credits"		},
	{0x12, 0x01, 0xf0, 0xd0, "1 Coin  5 Credits"		},
	{0x12, 0x01, 0xf0, 0xc0, "1 Coin/10 Credits"		},
	{0x12, 0x01, 0xf0, 0x40, "1 Coin/25 Credits"		},
	{0x12, 0x01, 0xf0, 0x80, "1 Coin/50 Credits"		},

	{0   , 0xfe, 0   ,    2, "Manufacturer"		},
	{0x13, 0x01, 0x01, 0x00, "Nihon System"		},
	{0x13, 0x01, 0x01, 0x01, "Sega/Nihon System"		},

	{0   , 0xfe, 0   ,    0, "Coin Slots"		},
	{0x13, 0x01, 0x80, 0x00, "1"		},
	{0x13, 0x01, 0x80, 0x80, "2"		},
};

STDDIPINFO(Freekck)

static struct BurnInputInfo CountrunInputList[] = {

	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 2"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 2"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDip + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDip + 1,	"dip"},
	{"Dip C",		BIT_DIPSWITCH,	DrvDip + 2,	"dip"},
};

STDINPUTINFO(Countrun)


static struct BurnDIPInfo CountrunDIPList[]=
{
	{0x11, 0xff, 0xff, 0xbf, NULL		},
	{0x12, 0xff, 0xff, 0xff, NULL		},
	{0x13, 0xff, 0xff, 0xfe, NULL		},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x11, 0x01, 0x01, 0x01, "3"		},
	{0x11, 0x01, 0x01, 0x00, "2"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x11, 0x01, 0x06, 0x06, "20000, 60000 and every 60000 Points"		},
	{0x11, 0x01, 0x06, 0x02, "30000, 80000 and every 80000 Points"		},
	{0x11, 0x01, 0x06, 0x04, "20000 & 60000 Points"		},
	{0x11, 0x01, 0x06, 0x00, "ONLY 20000 Points"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x11, 0x01, 0x18, 0x18, "Easy"		},
	{0x11, 0x01, 0x18, 0x10, "Normal"		},
	{0x11, 0x01, 0x18, 0x08, "Hard"		},
	{0x11, 0x01, 0x18, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x11, 0x01, 0x20, 0x00, "No"		},
	{0x11, 0x01, 0x20, 0x20, "Yes"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x11, 0x01, 0x40, 0x00, "Upright"		},
	{0x11, 0x01, 0x40, 0x40, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x11, 0x01, 0x80, 0x80, "Off"		},
	{0x11, 0x01, 0x80, 0x00, "On"		},

	{0   , 0xfe, 0   ,    16, "Coin A"		},
	{0x12, 0x01, 0x0f, 0x00, "5 Coins 1 Credits"		},
	{0x12, 0x01, 0x0f, 0x0c, "4 Coins 1 Credits"		},
	{0x12, 0x01, 0x0f, 0x0e, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0x0f, 0x05, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0x0f, 0x06, "3 Coins 2 Credits"		},
	{0x12, 0x01, 0x0f, 0x04, "4 Coins 3 Credits"		},
	{0x12, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0x0f, 0x08, "4 Coins 5 Credits"		},
	{0x12, 0x01, 0x0f, 0x0a, "3 Coins 4 Credits"		},
	{0x12, 0x01, 0x0f, 0x09, "2 Coins 3 Credits"		},
	{0x12, 0x01, 0x0f, 0x02, "3 Coins/5 Credits"		},
	{0x12, 0x01, 0x0f, 0x07, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0x0f, 0x01, "2 Coins 5 Credits"		},
	{0x12, 0x01, 0x0f, 0x0b, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0x0f, 0x03, "1 Coin  4 Credits"		},
	{0x12, 0x01, 0x0f, 0x0d, "1 Coin  5 Credits"		},

	{0   , 0xfe, 0   ,    16, "Coin B"		},
	{0x12, 0x01, 0xf0, 0x00, "5 Coins 1 Credits"		},
	{0x12, 0x01, 0xf0, 0xe0, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0xf0, 0x50, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0xf0, 0x60, "3 Coins 2 Credits"		},
	{0x12, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0xf0, 0xa0, "3 Coins 4 Credits"		},
	{0x12, 0x01, 0xf0, 0x90, "2 Coins 3 Credits"		},
	{0x12, 0x01, 0xf0, 0x20, "3 Coins/5 Credits"		},
	{0x12, 0x01, 0xf0, 0x70, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0xf0, 0x10, "2 Coins 5 Credits"		},
	{0x12, 0x01, 0xf0, 0xb0, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0xf0, 0x30, "1 Coin  4 Credits"		},
	{0x12, 0x01, 0xf0, 0xd0, "1 Coin  5 Credits"		},
	{0x12, 0x01, 0xf0, 0xc0, "1 Coin/10 Credits"		},
	{0x12, 0x01, 0xf0, 0x40, "1 Coin/25 Credits"		},
	{0x12, 0x01, 0xf0, 0x80, "1 Coin/50 Credits"		},

	{0   , 0xfe, 0   ,    2, "Manufacturer"		},
	{0x13, 0x01, 0x01, 0x00, "Nihon System"		},
	{0x13, 0x01, 0x01, 0x01, "Sega/Nihon System"		},

	{0   , 0xfe, 0   ,    0, "Coin Slots"		},
	{0x13, 0x01, 0x80, 0x00, "1"		},
	{0x13, 0x01, 0x80, 0x80, "2"		},

};

STDDIPINFO(Countrun)

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);
	coin = 0;
	nmi_enable = 0;
	ff_data = 0;
	romaddr = 0;
	flipscreen = 0;

	spinner = 0;
	DrvDial1 = 0;
	DrvDial2 = 0;

	ZetOpen(0);
	ZetReset();
	ZetClose();

	return 0;
}

void freekick_draw_sprite(INT32 offs)
{
	INT32 sx = DrvSprRAM[offs + 3];
	INT32 sy = 232 - DrvSprRAM[offs + 0];
	INT32 code = DrvSprRAM[offs + 1] + ((DrvSprRAM[offs + 2] & 0x20) << 3);

	INT32 flipx  = DrvSprRAM[offs + 2] & 0x80;    //?? unused ?
	INT32 flipy  = !(DrvSprRAM[offs + 2] & 0x40);
	INT32 color = DrvSprRAM[offs + 2] & 0x1f;

	if (flipy)
	{
		if (flipx)
		{
			Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 3, 0, 256, DrvGfxROM1);
		}
		else
		{
			Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 3, 0, 256, DrvGfxROM1);
		}
	}
	else
	{
		if (flipx)
		{
			Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 3, 0, 256, DrvGfxROM1);
		}
		else
		{
			Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 3, 0, 256, DrvGfxROM1);
		}
	}
}

static void gigas_draw_sprite(INT32 offs)
{
	INT32 sx = DrvSprRAM[offs + 3];
	INT32 sy = DrvSprRAM[offs + 2];
	INT32 code = DrvSprRAM[offs + 0] | ((DrvSprRAM[offs + 1] & 0x20) << 3);

	INT32 flipx = 0;
	INT32 flipy = 0;
	INT32 color = DrvSprRAM[offs + 1] & 0x1f;

	if (pbillrdmode) {
		code = DrvSprRAM[offs + 0];
		color = DrvSprRAM[offs + 1] & 0x0f;
	}

	if (0)
	{
		sx = 240 - sx;
		flipx = !flipx;
	}
	if (1)
	{
		sy = 224 - sy;
		flipy = !flipy;
	}

	if (flipy) 
	{
		if (flipx) 
		{
			Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 3, 0, 256, DrvGfxROM1);
		}
		else
		{
			Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 3, 0, 256, DrvGfxROM1);
		}
	}
	else
	{
		if (flipx)
		{
			Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 3, 0, 256, DrvGfxROM1);
		} 
		else 
		{
			Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 3, 0, 256, DrvGfxROM1);
		}
	}
}

static INT32 DrvDraw()
{

	if (DrvRecalc)
	{
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	// Draw tiles
	for (INT32 offs = 0x3ff; offs >= 0; offs--)
	{
		INT32 sx = (offs % 32) * 8;
		INT32 sy = (offs / 32) * 8;

		INT32 code  = DrvVidRAM[offs] + ((DrvVidRAM[offs + 0x400] & 0xe0) << 3);
		INT32 color = DrvVidRAM[offs + 0x400] & 0x1f;
		sy -= 16;

		if(sy >= 0)
			Render8x8Tile_Clip(pTransDraw, code, sx, sy, color, 3, 0, DrvGfxROM0);
	}

	for (INT32 offs = 0; offs < 0x100; offs += 4)
	{
		DrawSprite(offs);
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void __fastcall freekick_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
//	AM_RANGE(0xec00, 0xec03) AM_DEVREADWRITE("ppi8255_0", i8255_device, read, write)
		case 0xec00:
		case 0xec01:
		case 0xec02:
		case 0xec03:
			ppi8255_w(0, address & 0x03, data);
			return;

//	AM_RANGE(0xf000, 0xf003) AM_DEVREADWRITE("ppi8255_1", i8255_device, read, write)
		case 0xf000:
		case 0xf001:
		case 0xf002:
		case 0xf003:
			ppi8255_w(1, address & 0x03, data);
			return;

//	AM_RANGE(0xf800, 0xf800) AM_READ_PORT("IN0") AM_WRITE(flipscreen_w)
		case 0xf800:
		case 0xf801:
			return;

		case 0xf802:
		case 0xf803:
			coin = ~data & 1;
			return;

		case 0xf804: 
//			bprintf(0, _T("nmi enable %X\n"), data);
			nmi_enable = data & 1;
			return;

// AM_RANGE(0xf806, 0xf806) AM_WRITE(spinner_select_w)
		case 0xf806:
			spinner = data & 1;
		return;

		case 0xfc00: 
			SN76496Write(0, data);
		return;
		
		case 0xfc01:
			SN76496Write(1, data);
		return;
		
		case 0xfc02:
			SN76496Write(2, data);
		return;

		case 0xfc03:
			SN76496Write(3, data);
		return;
	}
}

UINT8 __fastcall freekick_read(UINT16 address)
{
	switch (address)
	{
//	AM_RANGE(0xec00, 0xec03) AM_DEVREADWRITE("ppi8255_0", i8255_device, read, write)
		case 0xec00:
		case 0xec01:
		case 0xec02:
		case 0xec03:
			return ppi8255_r(0, address & 0x03);

//	AM_RANGE(0xf000, 0xf003) AM_DEVREADWRITE("ppi8255_1", i8255_device, read, write)
		case 0xf000:
		case 0xf001:
		case 0xf002:
		case 0xf003:
			return ppi8255_r(1, address & 0x03);

		case 0xf800: {
			return DrvInputs[0];
		}

		case 0xf801: {
			return DrvInputs[1];
		}

//	AM_RANGE(0xf802, 0xf802) AM_READNOP //MUST return bit 0 = 0, otherwise game resets
		case 0xf802:
			return 0;

//	AM_RANGE(0xf803, 0xf803) AM_READ(spinner_r)
		case 0xf803:  
		{
			if(spinner)
				return DrvDial2;
			else
				return DrvDial1;
		}
	}
	return 0;
}

static void pbillrd_setbank(UINT8 banknum)
{
	DrvZ80Bank0 = banknum; // for savestates

	UINT32 bankloc = 0x10000 + banknum * 0x4000;

	if (use_encrypted) {
		ZetMapArea(0x8000, 0xbfff, 0, DrvMainROM + bankloc); // read
		ZetMapArea(0x8000, 0xbfff, 2, DrvMainROMdec + bankloc, DrvMainROM + bankloc); // fetch ops(encrypted), opargs(unencrypted)
	} else {
		ZetMapArea(0x8000, 0xbfff, 0, DrvMainROM + bankloc); // read
		ZetMapArea(0x8000, 0xbfff, 2, DrvMainROM + bankloc); // fetch
	}
}

static void __fastcall gigas_write(UINT16 address, UINT8 data)
{
//	AM_RANGE(0xe000, 0xe000) AM_WRITENOP
//	AM_RANGE(0xe002, 0xe003) AM_WRITE(coin_w)
//	AM_RANGE(0xe004, 0xe004) AM_WRITE(nmi_enable_w)
//	AM_RANGE(0xe005, 0xe005) AM_WRITENOP
	switch (address)
	{
		case 0xe000:
//		case 0xe001:
//			flipscreen = data;
			return;

		case 0xe002:
		case 0xe003:
			coin = ~data & 1;
			return;

		case 0xe004: //bprintf(0, _T("nmi enable %X\n"), data);
			nmi_enable = data & 1;
			return;
		case 0xe005:
			return;
// 	AM_RANGE(0xf000, 0xf000) AM_WRITE(SMH_NOP) //bankswitch ?

		case 0xf000: {
			if (pbillrdmode) {
				pbillrd_setbank(data & 1);
			}
			return;
		}

//	AM_RANGE(0xfc00, 0xfc00) AM_WRITE(sn76496_0_w)
//	AM_RANGE(0xfc01, 0xfc01) AM_WRITE(sn76496_1_w)
//	AM_RANGE(0xfc02, 0xfc02) AM_WRITE(sn76496_2_w)
//	AM_RANGE(0xfc03, 0xfc03) AM_WRITE(sn76496_3_w)

		case 0xfc00: 
			SN76496Write(0, data);
		return;
		
		case 0xfc01:
			SN76496Write(1, data);
		return;
		
		case 0xfc02:
			SN76496Write(2, data);
		return;

		case 0xfc03:
			SN76496Write(3, data);
		return;
	}
}

UINT8 __fastcall gigas_read(UINT16 address)
{
	switch (address)
	{
		case 0xe000:
		{
			return DrvInputs[0];
			//	AM_RANGE(0xe000, 0xe000) AM_READ_PORT("IN0") AM_WRITENOP // probably not flipscreen
		}

		case 0xe800:
		{
			return DrvInputs[1];
			//	AM_RANGE(0xe800, 0xe800) AM_READ_PORT("IN1")
		}

		case 0xf000: {
			return DrvDip[0];//	AM_RANGE(0xf000, 0xf000) AM_READ_PORT("DSW1") AM_WRITENOP //bankswitch ?
		}

		case 0xf800: {
			return DrvDip[1];//	AM_RANGE(0xf800, 0xf800) AM_READ_PORT("DSW2")
		}
	}
	return 0;
}

UINT8 __fastcall freekick_in(UINT16 address)
{
	switch (address & 0xff)
	{
		case 0xff:
            // 	AM_RANGE(0xff, 0xff) AM_READWRITE(freekick_ff_r, freekick_ff_w)
			return ff_data;
		break;
	}

	return 0;
}

void __fastcall freekick_out(UINT16 address, UINT8 data)
{
	switch (address & 0xff)
	{
		case 0xff:
            // 	AM_RANGE(0xff, 0xff) AM_READWRITE(freekick_ff_r, freekick_ff_w)
			ff_data = data;
		break;
	}
}

static UINT8 freekick_ppiread_1_c()
{
	return DrvSndROM[romaddr & 0x7fff];
}

static void freekick_ppi_write_1_a(UINT8 data)
{
	romaddr = (romaddr & 0xff00) | data;
}

static void freekick_ppi_write_1_b(UINT8 data)
{
	romaddr = (romaddr & 0x00ff) | (data << 8);
}

static UINT8 freekick_ppiread_2_a()
{
	return DrvDip[0];
}

static UINT8 freekick_ppiread_2_b()
{
	return DrvDip[1];
}

static UINT8 freekick_ppiread_2_c()
{
	return DrvDip[2];
}

UINT8 __fastcall gigas_in(UINT16 address)
{
	switch (address & 0xff)
	{
		case 0x00: 
			if(spinner)
				return DrvDial2;
			else
				return DrvDial1;
		break;
		
		case 0x01:
			return 0;
		break;
	}

	return 0;
}

void __fastcall gigas_out(UINT16 address, UINT8 data)
{
	switch (address & 0xff)
	{
		case 0x00:
			spinner = data & 1;
		break;
	}
}


static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvMainROM	 	= Next; Next += 0x40000;
	DrvMainROMdec   = Next; Next += 0x20000;
	DrvSndROM		= Next; Next += 0x10000;
	DrvGfxROM0		= Next; Next += 0x20000; // 0x800 * 8 * 8
	DrvGfxROM1		= Next; Next += 0x20000; // 0x200 * 16 * 16
	MC8123Key		= Next; Next += 0x02000;
	DrvColPROM		= Next; Next += 0x00600;
	DrvPalette		= (UINT32*)Next; Next += 0x0400 * sizeof(UINT32); // à faire

	AllRam			= Next;

	DrvRAM			= Next; Next += 0x02000; // 0x0e000 - 0x0c000
	DrvVidRAM		= Next; Next += 0x00800;
	DrvSprRAM		= Next; Next += 0x00100;
	DrvColRAM		= Next; Next += 0x00600;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void DrvGfxDecode()
{
	INT32 Planes0[3] = { RGN_FRAC(0xc000, 2,3), RGN_FRAC(0xc000, 1,3), RGN_FRAC(0xc000, 0,3) };
	INT32 XOffs0[8]  = {0, 1, 2, 3, 4, 5, 6, 7};
	INT32 YOffs0[8]  = {STEP8(0, 8)};

	INT32 Planes1[3] = { RGN_FRAC(0xc000, 0,3),RGN_FRAC(0xc000, 2,3),RGN_FRAC(0xc000, 1,3) };
	INT32 XOffs1[16] = {0, 1, 2, 3, 4, 5, 6, 7,128+0,128+1,128+2,128+3,128+4,128+5,128+6,128+7};

//	INT32 YOffs1[16] = {0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8, 8*8, 9*8, 10*8, 11*8,12*8,13*8,14*8,15*8};
	INT32 YOffs1[16] = {15*8, 14*8, 13*8, 12*8, 11*8, 10*8, 9*8, 8*8, 7*8, 6*8, 5*8, 4*8,3*8,2*8,1*8,0*8};

	UINT8 *tmp = (UINT8*)BurnMalloc(0xc000);
	if (tmp == NULL) {
		return;
	}

	memcpy (tmp, DrvGfxROM0, 0xc000);
	GfxDecode(0x0800, 3,  8,  8, Planes0, XOffs0, YOffs0, 0x40, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0xc000);
	GfxDecode(0x0200, 3, 16, 16, Planes1, XOffs1, YOffs1, 0x100, tmp, DrvGfxROM1);

	BurnFree (tmp);
}

static void DrvPaletteInit()
{
	INT32 len = 0x200;

	for (INT32 i = 0; i < len; i++)
	{
		INT32 bit0,bit1,bit2,bit3,r,g,b;

		bit0 = (DrvColPROM[i + len * 0] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + len * 0] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + len * 0] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + len * 0] >> 3) & 0x01;
		r = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[i + len * 1] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + len * 1] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + len * 1] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + len * 1] >> 3) & 0x01;
		g = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[i + len * 2] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + len * 2] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + len * 2] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + len * 2] >> 3) & 0x01;
		b = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}
}

/*static INT32 GigasDecode()
{
	mc8123_decrypt_rom(0, 0, DrvMainROM, DrvMainROM + 0x10000, MC8123Key);
	return 0;
}*/

static INT32 LoadRoms()
{
	INT32 rom_number = 0;

	countrunbmode = !strcmp(BurnDrvGetTextA(DRV_NAME), "countrunb");

	if (!strcmp(BurnDrvGetTextA(DRV_NAME), "countrunb") || 
		!strcmp(BurnDrvGetTextA(DRV_NAME), "freekick") ||
		!strcmp(BurnDrvGetTextA(DRV_NAME), "freekicka") ||
		!strcmp(BurnDrvGetTextA(DRV_NAME), "freekickb1") ||
		!strcmp(BurnDrvGetTextA(DRV_NAME), "freekickb2") ||
		!strcmp(BurnDrvGetTextA(DRV_NAME), "freekickb3") 
	) 
	{
		if (BurnLoadRom(DrvMainROM,  rom_number++, 1)) return 1;
		if (!strcmp(BurnDrvGetTextA(DRV_NAME), "freekickb3")) {
			if (BurnLoadRom(DrvMainROM + 0x08000,  rom_number++, 1)) return 1;
		}
		if (BurnLoadRom(DrvSndROM,   rom_number++, 1)) return 1;	// sound rom
	}

	if (!strcmp(BurnDrvGetTextA(DRV_NAME), "pbillrd") ||
		!strcmp(BurnDrvGetTextA(DRV_NAME), "pbillrdsa"))
	{
		if (BurnLoadRom(DrvMainROM,  rom_number++, 1)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x04000,  rom_number++, 1)) return 1;
		memmove(DrvMainROM + 0x10000, DrvMainROM + 0x08000, 0x4000);
		if (BurnLoadRom(DrvMainROM + 0x14000,  rom_number++, 1)) return 1;

		if (!strcmp(BurnDrvGetTextA(DRV_NAME), "pbillrdsa")) {
			if (BurnLoadRom(MC8123Key,  rom_number++, 1)) return 1;
			mc8123_decrypt_rom(0, 2, DrvMainROM, DrvMainROMdec, MC8123Key);
			use_encrypted = 1;
		}
	}

	if (!strcmp(BurnDrvGetTextA(DRV_NAME), "gigasb")) 
	{
		if (BurnLoadRom(DrvMainROM + 0x10000,  rom_number++, 1)) return 1;
		memmove(DrvMainROM + 0x00000, DrvMainROM + 0x14000, 0x4000);

		if (BurnLoadRom(DrvMainROM + 0x14000,  rom_number++, 1)) return 1;
		memmove(DrvMainROM + 0x04000, DrvMainROM + 0x1c000, 0x8000);
	}

	if (!strcmp(BurnDrvGetTextA(DRV_NAME), "gigasm2b")) 
	{
		if (BurnLoadRom(DrvMainROM + 0x10000,  rom_number++, 1)) return 1;
		memmove(DrvMainROM + 0x00000, DrvMainROM + 0x14000, 0x4000);

		if (BurnLoadRom(DrvMainROM + 0x14000,  rom_number++, 1)) return 1;
		memmove(DrvMainROM + 0x04000, DrvMainROM + 0x18000, 0x4000);

		if (BurnLoadRom(DrvMainROM + 0x18000,  rom_number++, 1)) return 1;
		memmove(DrvMainROM + 0x08000, DrvMainROM + 0x1c000, 0x4000);
	}

	// Gfx char
	if (BurnLoadRom(DrvGfxROM0  + 0x00000,  rom_number++, 1)) return 1; // ( "4.3k", 0x00000, 0x04000
	if (BurnLoadRom(DrvGfxROM0  + 0x04000,  rom_number++, 1)) return 1; // ( "5.3h", 0x04000, 0x04000
	if (BurnLoadRom(DrvGfxROM0  + 0x08000,  rom_number++, 1)) return 1; // ( "6.3g", 0x08000, 0x04000

	// Gfx sprite
	if (BurnLoadRom(DrvGfxROM1  + 0x00000,  rom_number++, 1)) return 1; // ( "1.3p", 0x00000, 0x04000
	if (BurnLoadRom(DrvGfxROM1  + 0x04000,  rom_number++, 1)) return 1; // ( "3.3l", 0x04000, 0x04000
	if (BurnLoadRom(DrvGfxROM1  + 0x08000,  rom_number++, 1)) return 1; // ( "2.3n", 0x08000, 0x04000
	// Opcode Decryption PROMS
	//		GigasDecode(); - not used due to incomplete "gigas" romset.

	// Palette
	if (BurnLoadRom(DrvColPROM + 0x000000,	rom_number++, 1)) return 1; // ( "3a.bin", 0x0000, 0x0100
	if (BurnLoadRom(DrvColPROM + 0x000100,  rom_number++, 1)) return 1; // ( "4d.bin", 0x0100, 0x0100
	if (BurnLoadRom(DrvColPROM + 0x000200,	rom_number++, 1)) return 1; // ( "4a.bin", 0x0200, 0x0100
	if (BurnLoadRom(DrvColPROM + 0x000300,	rom_number++, 1)) return 1; // ( "3d.bin", 0x0300, 0x0100
	if (BurnLoadRom(DrvColPROM + 0x000400,	rom_number++, 1)) return 1; // ( "3b.bin", 0x0400, 0x0100
	if (BurnLoadRom(DrvColPROM + 0x000500,	rom_number++, 1)) return 1; // ( "3c.bin", 0x0500, 0x0100

	return 0;
}

static INT32 DrvFreeKickInit()
{
	DrawSprite = freekick_draw_sprite;
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	LoadRoms();

	DrvPaletteInit();

	DrvGfxDecode();

	ZetInit(0);
	ZetOpen(0);
//	AM_RANGE(0x0000, 0xcfff) AM_ROM
	ZetMapArea(0x0000, 0xcfff, 0, DrvMainROM);
	ZetMapArea(0x0000, 0xcfff, 2, DrvMainROM); //+0x10000,DrvMainROM);
//	AM_RANGE(0xd000, 0xdfff) AM_RAM
	ZetMapMemory(DrvRAM,		0xd000, 0xdfff, MAP_RAM);
//	AM_RANGE(0xe000, 0xe7ff) AM_RAM_WRITE(freek_videoram_w) AM_SHARE("videoram")    // tilemap
	ZetMapMemory(DrvVidRAM,		0xe000, 0xe7ff, MAP_RAM);
//	AM_RANGE(0xe800, 0xe8ff) AM_RAM AM_SHARE("spriteram")   // sprites
	ZetMapMemory(DrvSprRAM,		0xe800, 0xe8ff, MAP_RAM);

	ppi8255_init(2);

	PPI0PortReadC  = freekick_ppiread_1_c;
	PPI0PortWriteA = freekick_ppi_write_1_a;
	PPI0PortWriteB = freekick_ppi_write_1_b;

	PPI1PortReadA = freekick_ppiread_2_a;
	PPI1PortReadB = freekick_ppiread_2_b;
	PPI1PortReadC = freekick_ppiread_2_c;

	ZetSetReadHandler(freekick_read);
	ZetSetWriteHandler(freekick_write);
	ZetSetInHandler(freekick_in);
	ZetSetOutHandler(freekick_out);

	ZetClose();

	SN76489AInit(0, 12000000/4, 0);
	SN76489AInit(1, 12000000/4, 1);
	SN76489AInit(2, 12000000/4, 1);
	SN76489AInit(3, 12000000/4, 1);

	SN76496SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);
	SN76496SetRoute(1, 1.00, BURN_SND_ROUTE_BOTH);
	SN76496SetRoute(2, 1.00, BURN_SND_ROUTE_BOTH);
	SN76496SetRoute(3, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvInit()
{
	DrawSprite = gigas_draw_sprite;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	LoadRoms();

	DrvPaletteInit();

	DrvGfxDecode();

	ZetInit(0);
	ZetOpen(0);

	if (pbillrdmode) {
		ZetMapArea(0x0000, 0x7fff, 0, DrvMainROM); // read
		ZetMapArea(0x0000, 0x7fff, 2, DrvMainROM); // fetch

		if (use_encrypted) {
			ZetMapArea(0x0000, 0x7fff, 0, DrvMainROM);
			ZetMapArea(0x0000, 0x7fff, 2, DrvMainROMdec, DrvMainROM); // fetch ops(encrypted), opargs(unencrypted)
		}
		pbillrd_setbank(0);
	} else { // gigas*
		ZetMapArea(0x0000, 0xbfff, 0, DrvMainROM);
		ZetMapArea(0x0000, 0xbfff, 2, DrvMainROM + 0x10000, DrvMainROM);
	}


//	AM_RANGE(0xc000, 0xcfff) AM_RAM
	ZetMapMemory(DrvRAM,		0xc000, 0xcfff, MAP_RAM);
//	AM_RANGE(0xd000, 0xd7ff) AM_RAM_WRITE(freek_videoram_w) AM_SHARE("videoram")
	ZetMapMemory(DrvVidRAM,		0xd000, 0xd7ff, MAP_RAM);
//	AM_RANGE(0xd800, 0xd8ff) AM_RAM AM_SHARE("spriteram")
	ZetMapMemory(DrvSprRAM,		0xd800, 0xd8ff, MAP_RAM);
//	AM_RANGE(0xd900, 0xdfff) AM_RAM
	ZetMapMemory(DrvRAM + 0x1000,		0xd900, 0xdfff, MAP_RAM);

	ppi8255_init(1);

	ZetSetReadHandler(gigas_read); // Memory
	ZetSetWriteHandler(gigas_write);

	ZetSetInHandler(gigas_in); // IO Port
	ZetSetOutHandler(gigas_out);

	ZetClose();

	SN76489AInit(0, 12000000/4, 0);
	SN76489AInit(1, 12000000/4, 1);
	SN76489AInit(2, 12000000/4, 1);
	SN76489AInit(3, 12000000/4, 1);

	SN76496SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);
	SN76496SetRoute(1, 1.00, BURN_SND_ROUTE_BOTH);
	SN76496SetRoute(2, 1.00, BURN_SND_ROUTE_BOTH);
	SN76496SetRoute(3, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	ZetExit();
	SN76496Exit();
	ppi8255_exit();
	BurnFree (AllMem);

	countrunbmode = 0;
	pbillrdmode = 0;
	use_encrypted = 0;

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	DrvInputs[0] = 0xff; // Active LOW
	DrvInputs[1] = 0xff;

	for (INT32 i = 0; i < 8; i++) {
		DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
		DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
	}

	{ // spinner calculation stuff.
		if (DrvJoy1[2]) DrvDial1 -= 0x04;
		if (DrvJoy1[3]) DrvDial1 += 0x04;
		if (DrvDial1 >= 0x100) DrvDial1 = 0;
		if (DrvDial1 < 0) DrvDial1 = 0xfc;

		if (DrvJoy2[2]) DrvDial2 -= 0x04;
		if (DrvJoy2[3]) DrvDial2 += 0x04;
		if (DrvDial2 >= 0x100) DrvDial2 = 0;
		if (DrvDial2 < 0) DrvDial2 = 0xfc;
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal = nCyclesTotal = (countrunbmode) ? 6000000 / 60 : 3072000 / 60;

	ZetOpen(0);
	for (INT32 i = 0; i < nInterleave; i++) {
		ZetRun(nCyclesTotal / nInterleave);

		if (i % 128 == 127)
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD); // audio irq


		if (i == (nInterleave - 1) && nmi_enable) { // vblank
			ZetNmi();
		}
	}
	ZetClose();

	if (pBurnSoundOut) {
		SN76496Update(0, pBurnSoundOut, nBurnSoundLen);
		SN76496Update(1, pBurnSoundOut, nBurnSoundLen);
		SN76496Update(2, pBurnSoundOut, nBurnSoundLen);
		SN76496Update(3, pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		DrvDraw();
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

		SN76496Scan(nAction, pnMin);

		SCAN_VAR(nmi_enable);
		SCAN_VAR(flipscreen);
		SCAN_VAR(coin);
		SCAN_VAR(spinner);
		SCAN_VAR(ff_data);
		SCAN_VAR(romaddr);
		SCAN_VAR(DrvDial1);
		SCAN_VAR(DrvDial2);
		SCAN_VAR(DrvZ80Bank0);
	}

	if (nAction & ACB_WRITE && pbillrdmode) {
		ZetOpen(0);
		pbillrd_setbank(DrvZ80Bank0);
		ZetClose();
	}

	return 0;
}

static INT32 pbillrdInit()
{
	pbillrdmode = 1;

	return DrvInit();
}

// Perfect Billiard

static struct BurnRomInfo pbillrdRomDesc[] = {
	{ "pb.18",	0x4000, 0x9e6275ac, 1 }, //  0 maincpu
	{ "pb.7",	0x8000, 0xdd438431, 1 }, //  1
	{ "pb.9",	0x4000, 0x089ce80a, 1 }, //  2

	{ "pb.4",	0x4000, 0x2f4d4dd3, 2 }, //  3 gfx1
	{ "pb.5",	0x4000, 0x9dfccbd3, 2 }, //  4
	{ "pb.6",	0x4000, 0xb5c3f6f6, 2 }, //  5

	{ "10619.3r",	0x2000, 0x3296b9d9, 3 }, //  6 gfx2
	{ "10621.3m",	0x2000, 0x3dca8e4b, 3 }, //  7
	{ "10620.3n",	0x2000, 0xee76b079, 3 }, //  8

	{ "82s129.3a",	0x0100, 0x44802169, 4 }, //  9 proms
	{ "82s129.4d",	0x0100, 0x69ca07cc, 4 }, // 10
	{ "82s129.4a",	0x0100, 0x145f950a, 4 }, // 11
	{ "82s129.3d",	0x0100, 0x43d24e17, 4 }, // 12
	{ "82s129.3b",	0x0100, 0x7fdc872c, 4 }, // 13
	{ "82s129.3c",	0x0100, 0xcc1657e5, 4 }, // 14
};

STD_ROM_PICK(pbillrd)
STD_ROM_FN(pbillrd)

struct BurnDriver BurnDrvPbillrd = {
	"pbillrd", NULL, NULL, NULL, "1987",
	"Perfect Billiard\0", NULL, "Nihon System", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, pbillrdRomInfo, pbillrdRomName, NULL, NULL, PbillrdInputInfo, PbillrdDIPInfo,
	pbillrdInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};

// Perfect Billiard (MC-8123, 317-5008)

static struct BurnRomInfo pbillrdsaRomDesc[] = {
	{ "20",			0x4000, 0xda020258, 1 }, //  0 maincpu
	{ "17",			0x8000, 0x9bb3d467, 1 }, //  1
	{ "19",			0x4000, 0x2335e6dd, 1 }, //  2

	{ "317-0030.key",	0x2000, 0x9223f06d, 2 }, //  3 user1

	{ "pb.4",		0x4000, 0x2f4d4dd3, 3 }, //  4 gfx1
	{ "pb.5",		0x4000, 0x9dfccbd3, 3 }, //  5
	{ "pb.6",		0x4000, 0xb5c3f6f6, 3 }, //  6

	{ "1",			0x4000, 0xc8ed651e, 4 }, //  7 gfx2
	{ "3",			0x4000, 0x5282fc86, 4 }, //  8
	{ "2",			0x4000, 0xe9f73f5b, 4 }, //  9

	{ "82s129.3a",		0x0100, 0x44802169, 5 }, // 10 proms
	{ "82s129.4d",		0x0100, 0x69ca07cc, 5 }, // 11
	{ "82s129.4a",		0x0100, 0x145f950a, 5 }, // 12
	{ "82s129.3d",		0x0100, 0x43d24e17, 5 }, // 13
	{ "82s129.3b",		0x0100, 0x7fdc872c, 5 }, // 14
	{ "82s129.3c",		0x0100, 0xcc1657e5, 5 }, // 15
};

STD_ROM_PICK(pbillrdsa)
STD_ROM_FN(pbillrdsa)

struct BurnDriver BurnDrvPbillrds = {
	"pbillrdsa", "pbillrd", NULL, NULL, "1987",
	"Perfect Billiard (MC-8123, 317-5008)\0", NULL, "Nihon System", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, pbillrdsaRomInfo, pbillrdsaRomName, NULL, NULL, PbillrdInputInfo, PbillrdDIPInfo,
	pbillrdInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// Free Kick (NS6201-A 1987.10)

static struct BurnRomInfo freekickRomDesc[] = {
	{ "ns6201-a_1987.10_free_kick.cpu",	0xd000, 0x6d172850, 1 }, //  0 maincpu

	{ "11.1e",		0x8000, 0xa6030ba9, 2 }, //  1 user1

	{ "12.1h",		0x4000, 0xfb82e486, 3 }, //  2 gfx1
	{ "13.1j",		0x4000, 0x3ad78ee2, 3 }, //  3
	{ "14.1l",		0x4000, 0x0185695f, 3 }, //  4

	{ "15.1m",		0x4000, 0x0fa7c13c, 4 }, //  5 gfx2
	{ "16.1p",		0x4000, 0x2b996e89, 4 }, //  6
	{ "17.1r",		0x4000, 0xe7894def, 4 }, //  7

	{ "24s10n.8j",		0x0100, 0x53a6bc21, 5 }, //  8 proms
	{ "24s10n.7j",		0x0100, 0x38dd97d8, 5 }, //  9
	{ "24s10n.8k",		0x0100, 0x18e66087, 5 }, // 10
	{ "24s10n.7k",		0x0100, 0xbc21797a, 5 }, // 11
	{ "24s10n.8h",		0x0100, 0x8aac5fd0, 5 }, // 12
	{ "24s10n.7h",		0x0100, 0xa507f941, 5 }, // 13
};

STD_ROM_PICK(freekick)
STD_ROM_FN(freekick)

struct BurnDriver BurnDrvFreekick = {
	"freekick", NULL, NULL, NULL, "1987",
	"Free Kick (NS6201-A 1987.10)\0", NULL, "Nihon System (Merit license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_BREAKOUT, 0,
	NULL, freekickRomInfo, freekickRomName, NULL, NULL, FreekckInputInfo, FreekckDIPInfo,
	DrvFreeKickInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};

// Free Kick (NS6201-A 1987.9)

static struct BurnRomInfo freekickaRomDesc[] = {
	{ "ns6201-a_1987.9_free_kick.cpu",	0xd000, 0xacc0a278, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu

	{ "11.1e",		0x8000, 0xa6030ba9, 2 | BRF_GRA },           //  1 user1

	{ "12.1h",		0x4000, 0xfb82e486, 3 | BRF_GRA },           //  2 gfx1
	{ "13.1j",		0x4000, 0x3ad78ee2, 3 | BRF_GRA },           //  3
	{ "14.1l",		0x4000, 0x0185695f, 3 | BRF_GRA },           //  4

	{ "15.1m",		0x4000, 0x0fa7c13c, 4 | BRF_GRA },           //  5 gfx2
	{ "16.1p",		0x4000, 0x2b996e89, 4 | BRF_GRA },           //  6
	{ "17.1r",		0x4000, 0xe7894def, 4 | BRF_GRA },           //  7

	{ "24s10n.8j",		0x0100, 0x53a6bc21, 5 | BRF_GRA },           //  8 proms
	{ "24s10n.7j",		0x0100, 0x38dd97d8, 5 | BRF_GRA },           //  9
	{ "24s10n.8k",		0x0100, 0x18e66087, 5 | BRF_GRA },           // 10
	{ "24s10n.7k",		0x0100, 0xbc21797a, 5 | BRF_GRA },           // 11
	{ "24s10n.8h",		0x0100, 0x8aac5fd0, 5 | BRF_GRA },           // 12
	{ "24s10n.7h",		0x0100, 0xa507f941, 5 | BRF_GRA },           // 13
};

STD_ROM_PICK(freekicka)
STD_ROM_FN(freekicka)

struct BurnDriver BurnDrvFreekicka = {
	"freekicka", "freekick", NULL, NULL, "1987",
	"Free Kick (NS6201-A 1987.9)\0", NULL, "Nihon System", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_BREAKOUT, 0,
	NULL, freekickaRomInfo, freekickaRomName, NULL, NULL, FreekckInputInfo, FreekckDIPInfo,
	DrvFreeKickInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};

// Free Kick (bootleg set 1)

static struct BurnRomInfo freekickb1RomDesc[] = {
	{ "freekbl8.q7",	0x10000, 0x4208cfe5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu

	{ "11.1e",		0x08000, 0xa6030ba9, 2 | BRF_GRA },           //  1 user1

	{ "12.1h",		0x04000, 0xfb82e486, 3 | BRF_GRA },           //  2 gfx1
	{ "13.1j",		0x04000, 0x3ad78ee2, 3 | BRF_GRA },           //  3
	{ "14.1l",		0x04000, 0x0185695f, 3 | BRF_GRA },           //  4

	{ "15.1m",		0x04000, 0x0fa7c13c, 4 | BRF_GRA },           //  5 gfx2
	{ "16.1p",		0x04000, 0x2b996e89, 4 | BRF_GRA },           //  6
	{ "17.1r",		0x04000, 0xe7894def, 4 | BRF_GRA },           //  7

	{ "24s10n.8j",		0x00100, 0x53a6bc21, 5 | BRF_GRA },           //  8 proms
	{ "24s10n.7j",		0x00100, 0x38dd97d8, 5 | BRF_GRA },           //  9
	{ "24s10n.8k",		0x00100, 0x18e66087, 5 | BRF_GRA },           // 10
	{ "24s10n.7k",		0x00100, 0xbc21797a, 5 | BRF_GRA },           // 11
	{ "24s10n.8h",		0x00100, 0x8aac5fd0, 5 | BRF_GRA },           // 12
	{ "24s10n.7h",		0x00100, 0xa507f941, 5 | BRF_GRA },           // 13

	{ "pal16l8.q10.bin",	0x00001, 0x00000000, 6 | BRF_NODUMP | BRF_GRA },           // 14 pals
	{ "pal16l8.r1.bin",	0x00001, 0x00000000, 6 | BRF_NODUMP | BRF_GRA },           // 15
	{ "pal16l8.s1.bin",	0x00001, 0x00000000, 6 | BRF_NODUMP | BRF_GRA },           // 16
};

STD_ROM_PICK(freekickb1)
STD_ROM_FN(freekickb1)

struct BurnDriver BurnDrvFreekickb1 = {
	"freekickb1", "freekick", NULL, NULL, "1987",
	"Free Kick (bootleg set 1)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_BREAKOUT, 0,
	NULL, freekickb1RomInfo, freekickb1RomName, NULL, NULL, FreekckInputInfo, FreekckDIPInfo,
	DrvFreeKickInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};

// Free Kick (bootleg set 3)

static struct BurnRomInfo freekickb3RomDesc[] = {
	{ "1",		0x8000, 0x214e1868, 1 }, //  0 maincpu
	{ "2",		0x8000, 0x734cdfc7, 1 }, //  1

	{ "11.1e",	0x8000, 0xa6030ba9, 2 }, //  2 user1

	{ "12.1h",	0x4000, 0xfb82e486, 3 }, //  3 gfx1
	{ "13.1j",	0x4000, 0x3ad78ee2, 3 }, //  4
	{ "14.1l",	0x4000, 0x0185695f, 3 }, //  5

	{ "15.1m",	0x4000, 0x0fa7c13c, 4 }, //  6 gfx2
	{ "16.1p",	0x4000, 0x2b996e89, 4 }, //  7
	{ "17.1r",	0x4000, 0xe7894def, 4 }, //  8

	{ "24s10n.8j",	0x0100, 0x53a6bc21, 5 }, //  9 proms
	{ "24s10n.7j",	0x0100, 0x38dd97d8, 5 }, // 10
	{ "24s10n.8k",	0x0100, 0x18e66087, 5 }, // 11
	{ "24s10n.7k",	0x0100, 0xbc21797a, 5 }, // 12
	{ "24s10n.8h",	0x0100, 0x8aac5fd0, 5 }, // 13
	{ "24s10n.7h",	0x0100, 0xa507f941, 5 }, // 14
	{ "n82s123an",	0x0020, 0x5ed93a02, 5 }, // 15
};

STD_ROM_PICK(freekickb3)
STD_ROM_FN(freekickb3)

struct BurnDriver BurnDrvFreekickb3 = {
	"freekickb3", "freekick", NULL, NULL, "1987",
	"Free Kick (bootleg set 3)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_BREAKOUT, 0,
	NULL, freekickb3RomInfo, freekickb3RomName, NULL, NULL, FreekckInputInfo, FreekckDIPInfo,
	DrvFreeKickInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Counter Run (NS6201-A 1988.3)

static struct BurnRomInfo countrunRomDesc[] = {
	{ "ns6201-a_1988.3_counter_run.cpu",	0x10000, 0x00000000, 1 | BRF_NODUMP }, //  0 maincpu

	{ "c-run.e1",		0x08000, 0x2c3b6f8f, 2 }, //  1 user1

	{ "c-run.h1",		0x04000, 0x3385b7b5, 3 }, //  2 gfx1
	{ "c-run.j1",		0x04000, 0x58dc148d, 3 }, //  3
	{ "c-run.l1",		0x04000, 0x3201f1e9, 3 }, //  4

	{ "c-run.m1",		0x04000, 0x1efab3b4, 4 }, //  5 gfx2
	{ "c-run.p1",		0x04000, 0xd0bf8d42, 4 }, //  6
	{ "c-run.r1",		0x04000, 0x4bb4a3e3, 4 }, //  7

	{ "24s10n.8j",		0x00100, 0x63c114ad, 5 }, //  8 proms
	{ "24s10n.7j",		0x00100, 0xd16f95cc, 5 }, //  9
	{ "24s10n.8k",		0x00100, 0x217db2c1, 5 }, // 10
	{ "24s10n.7k",		0x00100, 0x8d983949, 5 }, // 11
	{ "24s10n.8h",		0x00100, 0x33e87550, 5 }, // 12
	{ "24s10n.7h",		0x00100, 0xc77d0077, 5 }, // 13
};

STD_ROM_PICK(countrun)
STD_ROM_FN(countrun)

struct BurnDriver BurnDrvCountrun = {
	"countrun", NULL, NULL, NULL, "1988",
	"Counter Run (NS6201-A 1988.3)\0", "Please use countrunb instead!", "Nihon System (Sega license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	0, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, countrunRomInfo, countrunRomName, NULL, NULL, CountrunInputInfo, CountrunDIPInfo,
	DrvFreeKickInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};

// Counter Run (bootleg set 1)

static struct BurnRomInfo countrunbRomDesc[] = {
	{ "rom_cpu.bin",	0x10000, 0xf65639ae, 1 }, //  0 maincpu

	{ "c-run.e1",		0x08000, 0x2c3b6f8f, 2 }, //  1 user1

	{ "c-run.h1",		0x04000, 0x3385b7b5, 3 }, //  2 gfx1
	{ "c-run.j1",		0x04000, 0x58dc148d, 3 }, //  3
	{ "c-run.l1",		0x04000, 0x3201f1e9, 3 }, //  4

	{ "c-run.m1",		0x04000, 0x1efab3b4, 4 }, //  5 gfx2
	{ "c-run.p1",		0x04000, 0xd0bf8d42, 4 }, //  6
	{ "c-run.r1",		0x04000, 0x4bb4a3e3, 4 }, //  7

	{ "24s10n.8j",		0x00100, 0x63c114ad, 5 }, //  8 proms
	{ "24s10n.7j",		0x00100, 0xd16f95cc, 5 }, //  9
	{ "24s10n.8k",		0x00100, 0x217db2c1, 5 }, // 10
	{ "24s10n.7k",		0x00100, 0x8d983949, 5 }, // 11
	{ "24s10n.8h",		0x00100, 0x33e87550, 5 }, // 12
	{ "24s10n.7h",		0x00100, 0xc77d0077, 5 }, // 13
};

STD_ROM_PICK(countrunb)
STD_ROM_FN(countrunb)

struct BurnDriver BurnDrvCountrunb = {
	"countrunb", "countrun", NULL, NULL, "1988",
	"Counter Run (bootleg set 1)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, countrunbRomInfo, countrunbRomName, NULL, NULL, CountrunInputInfo, CountrunDIPInfo,
	DrvFreeKickInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};

// Gigas (bootleg)

static struct BurnRomInfo gigasbRomDesc[] = {
	{ "g-7",	0x08000, 0xdaf4e88d, 1 }, //  0 maincpu
	{ "g-8",	0x10000, 0x4ab4c1f1, 1 }, //  1

	{ "g-4",	0x04000, 0x8ed78981, 2 }, //  2 gfx1
	{ "g-5",	0x04000, 0x0645ec2d, 2 }, //  3
	{ "g-6",	0x04000, 0x99e9cb27, 2 }, //  4

	{ "g-1",	0x04000, 0xd78fae6e, 3 }, //  5 gfx2
	{ "g-3",	0x04000, 0x37df4a4c, 3 }, //  6
	{ "g-2",	0x04000, 0x3a46e354, 3 }, //  7

	{ "1.pr",	0x00100, 0xa784e71f, 4 }, //  8 proms
	{ "6.pr",	0x00100, 0x376df30c, 4 }, //  9
	{ "5.pr",	0x00100, 0x4edff5bd, 4 }, // 10
	{ "4.pr",	0x00100, 0xfe201a4e, 4 }, // 11
	{ "2.pr",	0x00100, 0x5796cc4a, 4 }, // 12
	{ "3.pr",	0x00100, 0x28b5ee4c, 4 }, // 13
};

STD_ROM_PICK(gigasb)
STD_ROM_FN(gigasb)

struct BurnDriver BurnDrvGigasb = {
	"gigasb", "gigas", NULL, NULL, "1986",
	"Gigas (bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_BREAKOUT, 0,
	NULL, gigasbRomInfo, gigasbRomName, NULL, NULL, GigasInputInfo, GigasDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};

// Gigas Mark II (bootleg)

static struct BurnRomInfo gigasm2RomDesc[] = {
	{ "8.rom",	0x08000, 0xc00a4a6c, 1 }, //  0 maincpu
	{ "7.rom",	0x08000, 0x92bd9045, 1 }, //  1
	{ "9.rom",	0x08000, 0xa3ef809c, 1 }, //  1

	{ "4.rom",	0x04000, 0x20b3405f, 2 }, //  2 gfx1
	{ "5.rom",	0x04000, 0xd04ecfa8, 2 }, //  3
	{ "6.rom",	0x04000, 0x33776801, 2 }, //  4

	{ "1.rom",	0x04000, 0xf64cbd1e, 3 }, //  5 gfx2
	{ "3.rom",	0x04000, 0xc228df19, 3 }, //  6
	{ "2.rom",	0x04000, 0xa6ad9ce2, 3 }, //  7

	{ "1.pr",	0x00100, 0xa784e71f, 4 }, //  8 proms
	{ "6.pr",	0x00100, 0x376df30c, 4 }, //  9
	{ "5.pr",	0x00100, 0x4edff5bd, 4 }, // 10
	{ "4.pr",	0x00100, 0xfe201a4e, 4 }, // 11
	{ "2.pr",	0x00100, 0x5796cc4a, 4 }, // 12
	{ "3.pr",	0x00100, 0x28b5ee4c, 4 }, // 13
};

STD_ROM_PICK(gigasm2)
STD_ROM_FN(gigasm2)

struct BurnDriver BurnDrvGigasm2 = {
	"gigasm2b", NULL, NULL, NULL, "1986",
	"Gigas Mark II\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_BREAKOUT, 0,
	NULL, gigasm2RomInfo, gigasm2RomName, NULL, NULL, GigasInputInfo, Gigasm2DIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};

// Gigas (MC-8123, 317-5002)

static struct BurnRomInfo gigasRomDesc[] = {
	{ "8.8n",	0x04000, 0x34ea8262, BRF_ESS | BRF_PRG }, //  0 maincpu
	{ "7.8r",	0x08000, 0x43653909, BRF_ESS | BRF_PRG }, //  1

	{ "4.3k",	0x04000, 0x8ed78981, BRF_GRA }, //  2 gfx1
	{ "5.3h",	0x04000, 0x0645ec2d, BRF_GRA }, //  3
	{ "6.3g",	0x04000, 0x99e9cb27, BRF_GRA }, //  4

	{ "1.3p",	0x04000, 0xd78fae6e, BRF_GRA }, //  5 gfx2
	{ "3.3l",	0x04000, 0x37df4a4c, BRF_GRA }, //  6
	{ "2.3n",	0x04000, 0x3a46e354, BRF_GRA }, //  7

	{ "3a.bin",	0x00100, 0xa784e71f, BRF_OPT }, //  8 proms
	{ "4d.bin",	0x00100, 0x376df30c, BRF_OPT }, //  9
	{ "4a.bin",	0x00100, 0x4edff5bd, BRF_OPT }, // 10
	{ "3d.bin",	0x00100, 0xfe201a4e, BRF_OPT }, // 11
	{ "3b.bin",	0x00100, 0x5796cc4a, BRF_OPT }, // 12
	{ "3c.bin",	0x00100, 0x28b5ee4c, BRF_OPT }, // 13

	{ "317-5002.key",	0x02000, 0x86a7e5f6, BRF_ESS | BRF_PRG }, //  14
};
STD_ROM_PICK(gigas)
STD_ROM_FN(gigas)

struct BurnDriver BurnDrvGigas = {
	"gigas", NULL, NULL, NULL, "1986",
	"Gigas (MC-8123, 317-5002)\0", "Please use gigasb instead!", "SEGA", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	0 | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_BREAKOUT, 0,
	NULL, gigasRomInfo, gigasRomName, NULL, NULL, GigasInputInfo, GigasDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};
