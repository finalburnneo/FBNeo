// Zodiack emu-layer for FB Alpha by dink, based on the MAME driver by Zsolt Vasvari.

#include "tiles_generic.h"
#include "driver.h"
#include "z80_intf.h"
#include "bitswap.h"

extern "C" {
	#include "ay8910.h"
}
static INT16 *pAY8910Buffer[6];

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvVidRAM;
static UINT8 *DrvVid2RAM;
static UINT8 *DrvAttrRAM;
static UINT8 *DrvBulletRAM;
static UINT8 *DrvExtraRAM;
static UINT8 *DrvSpriteRAM;
static UINT8 *DrvColorPROM;
static UINT8 *DrvCharGFX;
static UINT8 *DrvChar2GFX;
static UINT8 *DrvSpriteGFX;

static INT32 DrvGfxROM0Len;

static UINT8 *nmi_mask;
static UINT8 *sub_nmi_mask;
static UINT8 *soundlatch;

static UINT8 moguchan = 0;
static UINT8 percuss_hardware = 0;
static UINT8 flipscreen = 0;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2] = { 0, 0 };
static UINT8 DrvInput[5];
static UINT8 DrvReset;

static struct BurnInputInfo ZodiackInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 1"},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 1"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
};

STDINPUTINFO(Zodiack)


static struct BurnDIPInfo ZodiackDIPList[]=
{
	{0x0e, 0xff, 0xff, 0x40, NULL		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0e, 0x01, 0x03, 0x00, "3"		},
	{0x0e, 0x01, 0x03, 0x01, "4"		},
	{0x0e, 0x01, 0x03, 0x02, "5"		},
	{0x0e, 0x01, 0x03, 0x03, "6"		},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x0e, 0x01, 0x1c, 0x14, "2 Coins 1 Credits"		},
	{0x0e, 0x01, 0x1c, 0x18, "2 Coins/1 Credit  3 Coins/2 Credits"		},
	{0x0e, 0x01, 0x1c, 0x00, "1 Coin  1 Credits"		},
	{0x0e, 0x01, 0x1c, 0x04, "1 Coin  2 Credits"		},
	{0x0e, 0x01, 0x1c, 0x08, "1 Coin  3 Credits"		},
	{0x0e, 0x01, 0x1c, 0x0c, "1 Coin  4 Credits"		},
	{0x0e, 0x01, 0x1c, 0x10, "1 Coin  6 Credits"		},
	{0x0e, 0x01, 0x1c, 0x1c, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x0e, 0x01, 0x20, 0x00, "20000 50000"		},
	{0x0e, 0x01, 0x20, 0x20, "40000 70000"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0e, 0x01, 0x40, 0x40, "Upright"		},
	{0x0e, 0x01, 0x40, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x0e, 0x01, 0x80, 0x00, "Off"		},
	{0x0e, 0x01, 0x80, 0x80, "On"		},
};

STDDIPINFO(Zodiack)

static struct BurnInputInfo DogfightInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 1"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 1"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
};

STDINPUTINFO(Dogfight)


static struct BurnDIPInfo DogfightDIPList[]=
{
	{0x0b, 0xff, 0xff, 0x00, NULL		},
	{0x0c, 0xff, 0xff, 0x40, NULL		},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x0b, 0x01, 0x07, 0x05, "2 Coins 1 Credits"		},
	{0x0b, 0x01, 0x07, 0x06, "3 Coins 2 Credits"		},
	{0x0b, 0x01, 0x07, 0x00, "1 Coin  1 Credits"		},
	{0x0b, 0x01, 0x07, 0x01, "1 Coin  2 Credits"		},
	{0x0b, 0x01, 0x07, 0x02, "1 Coin  3 Credits"		},
	{0x0b, 0x01, 0x07, 0x03, "1 Coin  4 Credits"		},
	{0x0b, 0x01, 0x07, 0x04, "1 Coin  6 Credits"		},
	{0x0b, 0x01, 0x07, 0x07, "Free Play"		},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x0b, 0x01, 0x38, 0x38, "5 Coins 1 Credits"		},
	{0x0b, 0x01, 0x38, 0x30, "4 Coins 1 Credits"		},
	{0x0b, 0x01, 0x38, 0x28, "3 Coins 1 Credits"		},
	{0x0b, 0x01, 0x38, 0x20, "2 Coins 1 Credits"		},
	{0x0b, 0x01, 0x38, 0x00, "1 Coin  1 Credits"		},
	{0x0b, 0x01, 0x38, 0x18, "3 Coins 4 Credits"		},
	{0x0b, 0x01, 0x38, 0x08, "1 Coin  2 Credits"		},
	{0x0b, 0x01, 0x38, 0x10, "1 Coin  3 Credits"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x0b, 0x01, 0x40, 0x00, "Off"		},
	{0x0b, 0x01, 0x40, 0x40, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x0b, 0x01, 0x80, 0x00, "Off"		},
	{0x0b, 0x01, 0x80, 0x80, "On"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0c, 0x01, 0x03, 0x00, "3"		},
	{0x0c, 0x01, 0x03, 0x01, "4"		},
	{0x0c, 0x01, 0x03, 0x02, "5"		},
	{0x0c, 0x01, 0x03, 0x03, "6"		},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x0c, 0x01, 0x04, 0x00, "Off"		},
	{0x0c, 0x01, 0x04, 0x04, "On"		},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x0c, 0x01, 0x08, 0x00, "Off"		},
	{0x0c, 0x01, 0x08, 0x08, "On"		},

	{0   , 0xfe, 0   ,    2, "Invulnerability (Cheat)"		},
	{0x0c, 0x01, 0x10, 0x00, "Off"		},
	{0x0c, 0x01, 0x10, 0x10, "On"		},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x0c, 0x01, 0x20, 0x00, "20000 50000"		},
	{0x0c, 0x01, 0x20, 0x20, "40000 70000"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0c, 0x01, 0x40, 0x40, "Upright"		},
	{0x0c, 0x01, 0x40, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x0c, 0x01, 0x80, 0x00, "Off"		},
	{0x0c, 0x01, 0x80, 0x80, "On"		},
};

STDDIPINFO(Dogfight)

static struct BurnInputInfo MoguchanInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 1"},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 1"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
};

STDINPUTINFO(Moguchan)


static struct BurnDIPInfo MoguchanDIPList[]=
{
	{0x0e, 0xff, 0xff, 0x40, NULL		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0e, 0x01, 0x03, 0x00, "3"		},
	{0x0e, 0x01, 0x03, 0x01, "4"		},
	{0x0e, 0x01, 0x03, 0x02, "5"		},
	{0x0e, 0x01, 0x03, 0x03, "6"		},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x0e, 0x01, 0x1c, 0x14, "2 Coins 1 Credits"		},
	{0x0e, 0x01, 0x1c, 0x18, "2 Coins/1 Credit  3 Coins/2 Credits"		},
	{0x0e, 0x01, 0x1c, 0x00, "1 Coin  1 Credits"		},
	{0x0e, 0x01, 0x1c, 0x04, "1 Coin  2 Credits"		},
	{0x0e, 0x01, 0x1c, 0x08, "1 Coin  3 Credits"		},
	{0x0e, 0x01, 0x1c, 0x0c, "1 Coin  4 Credits"		},
	{0x0e, 0x01, 0x1c, 0x10, "1 Coin  6 Credits"		},
	{0x0e, 0x01, 0x1c, 0x1c, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x0e, 0x01, 0x20, 0x00, "20000 50000"		},
	{0x0e, 0x01, 0x20, 0x20, "40000 70000"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0e, 0x01, 0x40, 0x40, "Upright"		},
	{0x0e, 0x01, 0x40, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x0e, 0x01, 0x80, 0x00, "Off"		},
	{0x0e, 0x01, 0x80, 0x80, "On"		},
};

STDDIPINFO(Moguchan)

static struct BurnInputInfo PercussInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy3 + 6,	"p1 fire 2"},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 fire 2"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
};

STDINPUTINFO(Percuss)


static struct BurnDIPInfo PercussDIPList[]=
{
	{0x10, 0xff, 0xff, 0x10, NULL		},
	{0x11, 0xff, 0xff, 0x00, NULL		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x10, 0x01, 0x01, 0x00, "Off"		},
	{0x10, 0x01, 0x01, 0x01, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x10, 0x01, 0x02, 0x00, "Off"		},
	{0x10, 0x01, 0x02, 0x02, "On"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x10, 0x01, 0x0c, 0x00, "20000 100000"		},
	{0x10, 0x01, 0x0c, 0x04, "20000 200000"		},
	{0x10, 0x01, 0x0c, 0x08, "40000 100000"		},
	{0x10, 0x01, 0x0c, 0x0c, "40000 200000"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x10, 0x01, 0x10, 0x10, "Upright"		},
	{0x10, 0x01, 0x10, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x10, 0x01, 0x20, 0x00, "Off"		},
	{0x10, 0x01, 0x20, 0x20, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x10, 0x01, 0x80, 0x00, "Off"		},
	{0x10, 0x01, 0x80, 0x80, "On"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x11, 0x01, 0x03, 0x00, "4"		},
	{0x11, 0x01, 0x03, 0x01, "5"		},
	{0x11, 0x01, 0x03, 0x02, "6"		},
	{0x11, 0x01, 0x03, 0x03, "7"		},

	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{0x11, 0x01, 0x0c, 0x08, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x0c, 0x00, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x0c, 0x04, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0x0c, 0x0c, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x11, 0x01, 0x10, 0x00, "Off"		},
	{0x11, 0x01, 0x10, 0x10, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x11, 0x01, 0x20, 0x00, "Off"		},
	{0x11, 0x01, 0x20, 0x20, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x11, 0x01, 0x40, 0x00, "Off"		},
	{0x11, 0x01, 0x40, 0x40, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x11, 0x01, 0x80, 0x00, "Off"		},
	{0x11, 0x01, 0x80, 0x80, "On"		},
};

STDDIPINFO(Percuss)

static struct BurnInputInfo BountyInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy3 + 6,	"p1 fire 2"},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 fire 2"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
};

STDINPUTINFO(Bounty)


static struct BurnDIPInfo BountyDIPList[]=
{
	{0x10, 0xff, 0xff, 0x00, NULL		},
	{0x11, 0xff, 0xff, 0x40, NULL		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x10, 0x01, 0x01, 0x00, "Off"		},
	{0x10, 0x01, 0x01, 0x01, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x10, 0x01, 0x02, 0x00, "Off"		},
	{0x10, 0x01, 0x02, 0x02, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x10, 0x01, 0x04, 0x00, "Off"		},
	{0x10, 0x01, 0x04, 0x04, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x10, 0x01, 0x08, 0x00, "Off"		},
	{0x10, 0x01, 0x08, 0x08, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x10, 0x01, 0x10, 0x00, "Off"		},
	{0x10, 0x01, 0x10, 0x10, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x10, 0x01, 0x20, 0x00, "Off"		},
	{0x10, 0x01, 0x20, 0x20, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x10, 0x01, 0x80, 0x00, "Off"		},
	{0x10, 0x01, 0x80, 0x80, "On"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x11, 0x01, 0x03, 0x00, "3"		},
	{0x11, 0x01, 0x03, 0x01, "4"		},
	{0x11, 0x01, 0x03, 0x02, "5"		},
	{0x11, 0x01, 0x03, 0x03, "6"		},

	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{0x11, 0x01, 0x0c, 0x08, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x0c, 0x00, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x0c, 0x04, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0x0c, 0x0c, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x11, 0x01, 0x10, 0x00, "Off"		},
	{0x11, 0x01, 0x10, 0x10, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x11, 0x01, 0x20, 0x00, "Off"		},
	{0x11, 0x01, 0x20, 0x20, "On"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x11, 0x01, 0x40, 0x40, "Upright"		},
	{0x11, 0x01, 0x40, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x11, 0x01, 0x80, 0x00, "Off"		},
	{0x11, 0x01, 0x80, 0x80, "On"		},
};

STDDIPINFO(Bounty)

static void DrvPaletteInit()
{
	const UINT8 *color_prom = DrvColorPROM;

	INT32 pal[0x100];

	for (INT32 i = 0; i < 0x30; i++)
	{
		INT32 bit0, bit1, bit2, r, g, b;

		/* red component */
		bit0 = (color_prom[i] >> 0) & 0x01;
		bit1 = (color_prom[i] >> 1) & 0x01;
		bit2 = (color_prom[i] >> 2) & 0x01;
		r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		/* green component */
		bit0 = (color_prom[i] >> 3) & 0x01;
		bit1 = (color_prom[i] >> 4) & 0x01;
		bit2 = (color_prom[i] >> 5) & 0x01;
		g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		/* blue component */
		bit0 = 0;
		bit1 = (color_prom[i] >> 6) & 0x01;
		bit2 = (color_prom[i] >> 7) & 0x01;
		b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		pal[i] = BurnHighCol(r, g, b, 0);
	}

	pal[0x30] = BurnHighCol(0xff, 0xff, 0xff, 0); // bullet

	for (INT32 i = 0; i < 0x20; i++)
		DrvPalette[i] = pal[(i & 3) ? i : 0];

	for (INT32 i = 0; i < 0x10; i += 2)
	{
		DrvPalette[0x20 + i] = pal[32 + (i / 2)];
		DrvPalette[0x21 + i] = pal[40 + (i / 2)];
	}

	/* bullet */
	DrvPalette[0x30] = pal[0];
	DrvPalette[0x31] = pal[0x30];
}

#define WB(a1,a2,dest) if (address >= a1 && address <= a2) { \
		dest[address - a1] = data; \
		return; \
	}
#define RB(a1,a2,dest) if (address >= a1 && address <= a2) \
		return dest[address - a1];

static void __fastcall main_write(UINT16 address, UINT8 data)
{
	WB(0x9000, 0x903f, DrvAttrRAM);
	WB(0x9040, 0x905f, DrvSpriteRAM);
	WB(0x9060, 0x907f, DrvBulletRAM);
	WB(0x9080, 0x93ff, DrvExtraRAM);

	switch (address)
	{
		case 0x7100: *nmi_mask = (data & 1) ^ 1; return;
		case 0x7200: flipscreen = data; return;
		case 0x6090: {
			*soundlatch = data & 0xff;
			ZetClose();
			ZetOpen(1);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
			ZetClose();
			ZetOpen(0);
			return;
		}
	}
}

static UINT8 __fastcall main_read(UINT16 address)
{
	RB(0x9000, 0x903f, DrvAttrRAM);
	RB(0x9040, 0x905f, DrvSpriteRAM);
	RB(0x9060, 0x907f, DrvBulletRAM);
	RB(0x9080, 0x93ff, DrvExtraRAM);

	switch (address)
	{
		case 0x6081: return DrvDips[0] | (DrvInput[2] & 0x40);
		case 0x6082: return DrvDips[1];
		case 0x6083: return DrvInput[0];
		case 0x6084: return DrvInput[1];
		case 0x6090: return *soundlatch;
	}

	return 0;
}

static void __fastcall audio_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x4000: {
			*sub_nmi_mask = data & 1;
			return;
		}
		case 0x6000: {
			*soundlatch = data;
			return;
		}
	}
}

static UINT8 __fastcall audio_read(UINT16 address)
{
	switch (address)
	{
		case 0x6000: return *soundlatch;
	}

	return 0;
}

static void __fastcall audio_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
			AY8910Write(0, port & 1, data);
		return;
	}

	return;
}


static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	AY8910Reset(0);
	AY8910Reset(1);

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x10000;
	DrvZ80ROM1		= Next; Next += 0x10000;

	DrvPalette		= (UINT32*)Next; Next += 0x0100 * sizeof(UINT32);
	DrvCharGFX      = Next; Next += 0x40000;
	DrvChar2GFX     = Next; Next += 0x40000;
	DrvSpriteGFX    = Next; Next += 0x40000;
	DrvColorPROM    = Next; Next += 0x00400;

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x01000;
	DrvZ80RAM1		= Next; Next += 0x01000;
	DrvAttrRAM	    = Next; Next += 0x00400;
	DrvVidRAM		= Next; Next += 0x00400;
	DrvVid2RAM		= Next; Next += 0x00400;
	DrvBulletRAM	= Next; Next += 0x00400;
	DrvSpriteRAM	= Next; Next += 0x00400;
	DrvExtraRAM	    = Next; Next += 0x00400;

	nmi_mask        = Next; Next += 0x00001;
	sub_nmi_mask    = Next; Next += 0x00001;
	soundlatch      = Next; Next += 0x00001;

	RamEnd			= Next;

	pAY8910Buffer[0]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[1]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[2]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[3]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[4]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[5]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);

	MemEnd			= Next;

	return 0;
}

static INT32 GetRoms()
{
	char* pRomName;
	struct BurnRomInfo ri;
	UINT8 *Load0 = DrvZ80ROM; //    1 main
	UINT8 *Load1 = DrvZ80ROM1; //   2 audio
	UINT8 *Loadg0 = DrvCharGFX; //  3 gfx
	UINT8 *Loadc = DrvColorPROM; //   4 prom

	DrvGfxROM0Len = 0;

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {

		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & 7) == 1) {
			if (BurnLoadRom(Load0, i, 1)) return 1;
			Load0 += ri.nLen;

			continue;
		}

		if ((ri.nType & 7) == 2) {
			if (BurnLoadRom(Load1, i, 1)) return 1;
			Load1 += ri.nLen;

			continue;
		}

		if ((ri.nType & 7) == 3) {
			if (BurnLoadRom(Loadg0, i, 1)) return 1;
			Loadg0 += ri.nLen;
			DrvGfxROM0Len += ri.nLen;

			continue;
		}

		if ((ri.nType & 7) == 4) {
			if (BurnLoadRom(Loadc, i, 1)) return 1;
			Loadc += ri.nLen;

			continue;
		}
	}

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 charPlane[2] = { 0, 512*8*8 };
	INT32 spritePlane[2] = { 0, 128*32*8 };

	INT32 XOffs[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 8*8+0, 8*8+1, 8*8+2, 8*8+3, 8*8+4, 8*8+5, 8*8+6, 8*8+7 };
	INT32 YOffs[16] = { 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8, 16*8, 17*8, 18*8, 19*8, 20*8, 21*8, 22*8, 23*8 };

	UINT8 *tmp = (UINT8*)malloc(0x20000);
	if (tmp == NULL) {
		return 1;
	}
	memset(tmp, 0, 0x20000);
	memcpy(tmp, DrvCharGFX, 0x2800);

	GfxDecode(0x100, 1, 8, 8, charPlane, XOffs, YOffs, 0x40, tmp + 0x0000, DrvCharGFX);
	GfxDecode(0x40, 2, 16, 16, spritePlane, XOffs, YOffs, 0x100, tmp + 0x0800, DrvSpriteGFX);
	GfxDecode(0x100, 2, 8, 8, charPlane, XOffs, YOffs, 0x40, tmp + 0x1000, DrvChar2GFX);

	free (tmp);

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
		if (GetRoms()) return 1;

		DrvGfxDecode();

		DrvPaletteInit();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		    0x0000, 0x4fff, MAP_ROM);
	ZetMapMemory(DrvZ80ROM + 0x5000,0xc000, 0xcfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,		    0x5800, 0x5fff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,		    0xa000, 0xa3ff, MAP_RAM);
	ZetMapMemory(DrvVid2RAM,		0xb000, 0xb3ff, MAP_RAM);
	ZetSetWriteHandler(main_write);
	ZetSetReadHandler(main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,	0x2000, 0x23ff, MAP_RAM);
	ZetSetWriteHandler(audio_write);
	ZetSetReadHandler(audio_read);
	ZetSetOutHandler(audio_out);
	ZetClose();

	AY8910Init(0, 1536000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.50, BURN_SND_ROUTE_BOTH);
	AY8910Init(1, 1536000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(1, 0.50, BURN_SND_ROUTE_BOTH);

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

	BurnFree(AllMem);
	percuss_hardware = 0;
	moguchan = 0;

	return 0;
}

static void draw_bg()
{
	UINT8 bits = 1; // fg 2bit, bg 1 bit

	for (INT32 offs = 0; offs < 0x400; offs++)
	{
		INT32 sx = (offs & 0x1f) << 3;
		INT32 sy = (offs >> 5) << 3;


		sy -= 16; //offsets
		if (sy < -7) sy += 256;
		if (sx < -7) sx += 256;

		INT32 flipy = 0;
		INT32 flipx = 0;

		INT32 code = DrvVid2RAM[offs];
		INT32 color = (DrvAttrRAM[(offs & 0x1f) << 1 | 1] >> 4) & 0x07;

		if (sx > nScreenWidth || sy > nScreenHeight) continue;

		if (flipy) {
			if (flipx) {
				Render8x8Tile_FlipXY_Clip(pTransDraw, code, sx, sy, color, bits, 8*4, DrvCharGFX);
			} else {
				Render8x8Tile_FlipY_Clip(pTransDraw, code, sx, sy, color, bits, 8*4, DrvCharGFX);
			}
		} else {
			if (flipx) {
				Render8x8Tile_FlipX_Clip(pTransDraw, code, sx, sy, color, bits, 8*4, DrvCharGFX);
			} else {
				Render8x8Tile_Clip(pTransDraw, code, sx, sy, color, bits, 8*4, DrvCharGFX);
			}
		}
	}
}

static void draw_fg()
{
	UINT8 bits = 2; // fg 2bit, bg 1 bit

	for (INT32 offs = 0; offs < 0x400; offs++)
	{
		INT32 sx = (offs & 0x1f) << 3;
		INT32 sy = (offs >> 5) << 3;

		sy -= DrvAttrRAM[(sx >> 3) * 2]; // col scroll
		sy -= 16; //offsets
		if (sy < -7) sy += 256;
		if (sx < -7) sx += 256;

		INT32 flipy = 0;
		INT32 flipx = 0;

		INT32 code = DrvVidRAM[offs];
		INT32 color = (DrvAttrRAM[(offs & 0x1f) << 1 | 1] >> 0) & 0x07;

		if (sx > nScreenWidth || sy > nScreenHeight) continue;

		if (flipy) {
			if (flipx) {
				Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, bits, 0, 0, DrvChar2GFX);
			} else {
				Render8x8Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, bits, 0, 0, DrvChar2GFX);
			}
		} else {
			if (flipx) {
				Render8x8Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, bits, 0, 0, DrvChar2GFX);
			} else {
				Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy, color, bits, 0, 0, DrvChar2GFX);
			}
		}
	}
}

static void draw_bullets()
{
	for (INT32 offs = 0; offs < 0x20; offs += 4)
	{
		INT32 sx = DrvBulletRAM[offs + 3];
		INT32 sy = DrvBulletRAM[offs + 1];
		if (!sx && !sy) continue;

		if (moguchan) {
			sy -= 16;
		} else {
			sy += 16; // everyone else
		}
		sx += 7;

		if (!(flipscreen && percuss_hardware))
			sy = 255 - sy;
		if (!flipscreen && percuss_hardware) sx = 255 - sx;

		if (sx >= nScreenWidth || sx < 0 || sy >= nScreenHeight || sy < 0) continue;

		pTransDraw[(sy * nScreenWidth) + sx] = 0x31;
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0x20-4; offs >= 0; offs -= 4)
	{
		INT32 sx = 240 - DrvSpriteRAM[offs + 3];
		INT32 sy = 240 - DrvSpriteRAM[offs];
		sy -= 16; //offsets

		INT32 code = DrvSpriteRAM[offs + 1] & 0x3f;
		INT32 color = DrvSpriteRAM[offs + 2] & 0x07;
		INT32 flipx = !(DrvSpriteRAM[offs + 1] & 0x40);
		INT32 flipy = DrvSpriteRAM[offs + 1] & 0x80;

		if (!flipscreen && percuss_hardware)
		{
			sx = DrvSpriteRAM[offs + 3];
			flipx = !flipx;
		}

		if (flipy) {
			if (flipx) {
				Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvSpriteGFX);
			} else {
				Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvSpriteGFX);
			}
		} else {
			if (flipx) {
				Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvSpriteGFX);
			} else {
				Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvSpriteGFX);
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	BurnTransferClear();

	if (nBurnLayer & 1) draw_bg();
	if (nBurnLayer & 2) draw_fg();
	if (nBurnLayer & 4) draw_bullets();
	if (nBurnLayer & 8) draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void DrvMakeInputs()
{
	// Reset Inputs (all active HIGH)
	DrvInput[0] = 0;
	DrvInput[1] = 0;
	DrvInput[2] = 0;

	// Compile Digital Inputs
	for (INT32 i = 0; i < 8; i++) {
		DrvInput[0] |= (DrvJoy1[i] & 1) << i;
		DrvInput[1] |= (DrvJoy2[i] & 1) << i;
		DrvInput[2] |= (DrvJoy3[i] & 1) << i;
	}
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	DrvMakeInputs();

	INT32 nInterleave = 256;
	INT32 nCyclesTotal = 3072000 / 60;

	for (INT32 i = 0; i < nInterleave; i++) {
		ZetOpen(0);
		ZetRun(nCyclesTotal / nInterleave);

		if (i == 235 && *nmi_mask)
			ZetNmi();

		if (i == 0)
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);

		ZetClose();

		ZetOpen(1);
		ZetRun(nCyclesTotal / nInterleave);

		if ((i % (nInterleave / 8) == (nInterleave / 8) - 1) && *sub_nmi_mask)
			ZetNmi();

		ZetClose();
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
	}

	return 0;
}

// Zodiack

static struct BurnRomInfo zodiackRomDesc[] = {
	{ "ovg30c.2",	0x2000, 0xa2125e99, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "ovg30c.3",	0x2000, 0xaee2b77f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ovg30c.6",	0x0800, 0x1debb278, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "ovg20c.1",	0x1000, 0x2d3c3baf, 2 | BRF_PRG | BRF_ESS }, //  3 audiocpu

	{ "ovg40c.7",	0x0800, 0xed9d3be7, 3 | BRF_GRA },           //  4 gfx1
	{ "orca40c.8",	0x1000, 0x88269c94, 3 | BRF_GRA },           //  5
	{ "orca40c.9",	0x1000, 0xa3bd40c9, 3 | BRF_GRA },           //  6

	{ "ovg40c.2a",	0x0020, 0x703821b8, 4 | BRF_GRA },           //  7 proms
	{ "ovg40c.2b",	0x0020, 0x21f77ec7, 4 | BRF_GRA },           //  8
};

STD_ROM_PICK(zodiack)
STD_ROM_FN(zodiack)

struct BurnDriver BurnDrvZodiack = {
	"zodiack", NULL, NULL, NULL, "1983",
	"Zodiack\0", NULL, "Orca (Esco Trading Co., Inc. license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, zodiackRomInfo, zodiackRomName, NULL, NULL, ZodiackInputInfo, ZodiackDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// Dog Fight (Thunderbolt)

static struct BurnRomInfo dogfightRomDesc[] = {
	{ "df-2.4f",	0x2000, 0xad24b28b, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "df-3.4h",	0x2000, 0xcd172707, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "df-5.4n",	0x1000, 0x874dc6bf, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "df-4.4m",	0x1000, 0xd8aa3d6d, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "df-1.4n",	0x1000, 0xdcbb1c5b, 2 | BRF_PRG | BRF_ESS }, //  4 audiocpu

	{ "df-6.3s",	0x0800, 0x3059b515, 3 | BRF_GRA },           //  5 gfx1
	{ "df-7.7n",	0x1000, 0xffe05fee, 3 | BRF_GRA },           //  6
	{ "df-8.7r",	0x1000, 0x2cb51793, 3 | BRF_GRA },           //  7

	{ "mmi6331.2a",	0x0020, 0x69a35aa5, 4 | BRF_GRA },           //  8 proms
	{ "mmi6331.2b",	0x0020, 0x596ae457, 4 | BRF_GRA },           //  9
};

STD_ROM_PICK(dogfight)
STD_ROM_FN(dogfight)

struct BurnDriver BurnDrvDogfight = {
	"dogfight", NULL, NULL, NULL, "1983",
	"Dog Fight (Thunderbolt)\0", NULL, "Orca / Thunderbolt", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, dogfightRomInfo, dogfightRomName, NULL, NULL, DogfightInputInfo, DogfightDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};

static INT32 moguchanInit()
{
	moguchan = 1;
	percuss_hardware = 1;

	return DrvInit();
}

static INT32 percussInit()
{
	percuss_hardware = 1;

	return DrvInit();
}

// Mogu Chan (bootleg?)

static struct BurnRomInfo moguchanRomDesc[] = {
	{ "2.5r",		0x1000, 0x85d0cb7e, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "4.5m",		0x1000, 0x359ef951, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.5np",		0x1000, 0xc8776f77, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "1.7hj",		0x1000, 0x1a88d35f, 2 | BRF_PRG | BRF_ESS }, //  3 audiocpu

	{ "5.4r",		0x0800, 0x1b7febd8, 3 | BRF_GRA },           //  4 gfx1
	{ "6.7p",		0x1000, 0xc8060ffe, 3 | BRF_GRA },           //  5
	{ "7.7m",		0x1000, 0xbfca00f4, 3 | BRF_GRA },           //  6

	{ "moguchan.2a",	0x0020, 0xe83daab3, 4 | BRF_GRA },           //  7 proms
	{ "moguchan.2b",	0x0020, 0x9abfdf40, 4 | BRF_GRA },           //  8
};

STD_ROM_PICK(moguchan)
STD_ROM_FN(moguchan)

struct BurnDriver BurnDrvMoguchan = {
	"moguchan", NULL, NULL, NULL, "1982",
	"Mogu Chan (bootleg?)\0", NULL, "Orca (Eastern Commerce Inc. license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, moguchanRomInfo, moguchanRomName, NULL, NULL, MoguchanInputInfo, MoguchanDIPInfo,
	moguchanInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// The Percussor

static struct BurnRomInfo percussRomDesc[] = {
	{ "percuss.1",		0x1000, 0xff0364f7, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "percuss.3",		0x1000, 0x7f646c59, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "percuss.2",		0x1000, 0x6bf72dd2, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "percuss.4",		0x1000, 0xfb1b15ba, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "percuss.5",		0x1000, 0x8e5a9692, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "percuss.8",		0x0800, 0xd63f56f3, 2 | BRF_PRG | BRF_ESS }, //  5 audiocpu
	{ "percuss.9",		0x0800, 0xe08fef2f, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "percuss.10",		0x0800, 0x797598aa, 3 | BRF_GRA },           //  7 gfx1
	{ "percuss.6",		0x1000, 0x5285a580, 3 | BRF_GRA },           //  8
	{ "percuss.7",		0x1000, 0x8fc4175d, 3 | BRF_GRA },           //  9

	{ "percus2a.prm",	0x0020, 0xe2ee9637, 4 | BRF_GRA },           // 10 proms
	{ "percus2b.prm",	0x0020, 0xe561b029, 4 | BRF_GRA },           // 11
};

STD_ROM_PICK(percuss)
STD_ROM_FN(percuss)

struct BurnDriver BurnDrvPercuss = {
	"percuss", NULL, NULL, NULL, "1981",
	"The Percussor\0", NULL, "Orca", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, percussRomInfo, percussRomName, NULL, NULL, PercussInputInfo, PercussDIPInfo,
	percussInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};


// The Bounty

static struct BurnRomInfo bountyRomDesc[] = {
	{ "1.4f",	0x1000, 0xf495b19d, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "3.4k",	0x1000, 0xfa3086c3, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "2.4h",	0x1000, 0x52ab5314, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.4m",	0x1000, 0x5c9d3f07, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "7.4n",	0x1000, 0x45e369b8, 2 | BRF_PRG | BRF_ESS }, //  4 audiocpu
	{ "8.4r",	0x1000, 0x4f52c87d, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "9.4r",	0x0800, 0x4b4acde5, 3 | BRF_GRA },           //  6 gfx1
	{ "5.7m",	0x1000, 0xa5ce2a24, 3 | BRF_GRA },           //  7
	{ "6.7p",	0x1000, 0x43183301, 3 | BRF_GRA },           //  8

	{ "mb7051.2a",	0x0020, 0x0de11a46, 4 | BRF_GRA },           //  9 proms
	{ "mb7051.2b",	0x0020, 0x465e31d4, 4 | BRF_GRA },           // 10
};

STD_ROM_PICK(bounty)
STD_ROM_FN(bounty)

struct BurnDriver BurnDrvBounty = {
	"bounty", NULL, NULL, NULL, "1982",
	"The Bounty\0", NULL, "Orca", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, bountyRomInfo, bountyRomName, NULL, NULL, BountyInputInfo, BountyDIPInfo,
	percussInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	256, 224, 4, 3
};


