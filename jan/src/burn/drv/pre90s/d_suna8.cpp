// FB Alpha SunA 8-bit driver module
// Based on MAME driver by Luca Elia

#include "tiles_generic.h"
#include "bitswap.h"
#include "z80_intf.h"
#include "burn_ym2203.h"
#include "burn_ym3812.h"
#include "dac.h"
#include "driver.h"
extern "C" {
#include "ay8910.h"
}

/*
	The samples do really sound that bad. Compare to MAME (sparkman has most obvious samples)

	Needs cleaned. Badly.
	Missing Brick Zone
	Bugs:
	  Star Fighter (v1) - the first boss has some broken tiles
*/

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80Decrypted;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvZ80ROM2;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvSampleROM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvSprRAM;
static UINT8 *DrvPalRAM;

static UINT8 *soundlatch;
static UINT8 *soundlatch2;
static UINT8 *flipscreen;
static UINT8 *nmi_enable;
static UINT8 *mainbank;

static INT16 *DrvSamplesExp;
static INT16 *pAY8910Buffer[3];

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 vblank;
static INT32 watchdog_enable = 0;
static INT32 watchdog;

static INT32 sample_start = -1;
static INT32 sample_offset = 0;
static UINT8 sample_number = 0;

static UINT8 m_gfxbank = 0;
static UINT8 m_palettebank = 0;
static UINT8 m_spritebank = 0;
static UINT8 m_spritebank_latch = 0;
static UINT8 m_rombank_latch = 0;
static UINT8 m_rambank = 0;
static UINT8 disable_mainram_write = 0;
static UINT8 protection_val = 0;
static UINT8 hardhead_ip = 0;
static UINT8 Sparkman = 0, Hardhead2 = 0;

static UINT8  DrvJoy1[8];
static UINT8  DrvJoy2[8];
static UINT8  DrvJoy3[8];
static UINT8  DrvDips[3];
static UINT8  DrvInputs[3];
static UINT8  DrvReset;

static struct BurnInputInfo DrvInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"},
};

STDINPUTINFO(Drv)

static struct BurnInputInfo SparkmanInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 fire 3"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 fire 3"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"},
};

STDINPUTINFO(Sparkman)

static struct BurnDIPInfo HardheadDIPList[]=
{
	{0x11, 0xff, 0xff, 0xfc, NULL		},
	{0x12, 0xff, 0xff, 0x77, NULL		},
	{0x13, 0xff, 0xff, 0xff, NULL		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x11, 0x01, 0x01, 0x01, "Off"		},
	{0x11, 0x01, 0x01, 0x00, "On"		},

	{0   , 0xfe, 0   ,    8, "Bonus Life"		},
	{0x11, 0x01, 0x0e, 0x0e, "No Bonus"		},
	{0x11, 0x01, 0x0e, 0x0c, "10K"		},
	{0x11, 0x01, 0x0e, 0x0a, "20K"		},
	{0x11, 0x01, 0x0e, 0x08, "50K"		},
	{0x11, 0x01, 0x0e, 0x06, "50K, Every 50K"		},
	{0x11, 0x01, 0x0e, 0x04, "100K, Every 50K"		},
	{0x11, 0x01, 0x0e, 0x02, "100K, Every 100K"		},
	{0x11, 0x01, 0x0e, 0x00, "200K, Every 100K"		},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x11, 0x01, 0x70, 0x00, "5 Coins 1 Credits"		},
	{0x11, 0x01, 0x70, 0x10, "4 Coins 1 Credits"		},
	{0x11, 0x01, 0x70, 0x20, "3 Coins 1 Credits"		},
	{0x11, 0x01, 0x70, 0x30, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x70, 0x70, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x70, 0x60, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0x70, 0x50, "1 Coin  3 Credits"		},
	{0x11, 0x01, 0x70, 0x40, "1 Coin  4 Credits"		},

	{0   , 0xfe, 0   ,    2, "Invulnerability (Cheat)"		},
	{0x11, 0x01, 0x80, 0x80, "Off"		},
	{0x11, 0x01, 0x80, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x12, 0x01, 0x01, 0x01, "Off"		},
	{0x12, 0x01, 0x01, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x12, 0x01, 0x02, 0x02, "Upright"		},
	{0x12, 0x01, 0x02, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Play Together"		},
	{0x12, 0x01, 0x04, 0x00, "No"		},
	{0x12, 0x01, 0x04, 0x04, "Yes"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x12, 0x01, 0x18, 0x18, "2"		},
	{0x12, 0x01, 0x18, 0x10, "3"		},
	{0x12, 0x01, 0x18, 0x08, "4"		},
	{0x12, 0x01, 0x18, 0x00, "5"		},

	{0   , 0xfe, 0   ,    8, "Difficulty"		},
	{0x12, 0x01, 0xe0, 0xe0, "Easiest"		},
	{0x12, 0x01, 0xe0, 0xc0, "Very Easy"		},
	{0x12, 0x01, 0xe0, 0xa0, "Easy"		},
	{0x12, 0x01, 0xe0, 0x80, "Moderate"		},
	{0x12, 0x01, 0xe0, 0x60, "Normal"		},
	{0x12, 0x01, 0xe0, 0x40, "Harder"		},
	{0x12, 0x01, 0xe0, 0x20, "Very Hard"		},
	{0x12, 0x01, 0xe0, 0x00, "Hardest"		},
};

STDDIPINFO(Hardhead)

static struct BurnDIPInfo RrangerDIPList[]=
{
	{0x11, 0xff, 0xff, 0xe7, NULL		},
	{0x12, 0xff, 0xff, 0xaf, NULL		},
	{0x13, 0xff, 0xff, 0xff, NULL		},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x11, 0x01, 0x07, 0x00, "5 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x01, "4 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x02, "3 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x03, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x11, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},

	{0   , 0xfe, 0   ,    8, "Bonus Life"		},
	{0x11, 0x01, 0x38, 0x30, "10K"		},
	{0x11, 0x01, 0x38, 0x28, "30K"		},
	{0x11, 0x01, 0x38, 0x20, "50K"		},
	{0x11, 0x01, 0x38, 0x18, "50K, Every 50K"		},
	{0x11, 0x01, 0x38, 0x10, "100K, Every 50K"		},
	{0x11, 0x01, 0x38, 0x08, "100K, Every 100K"		},
	{0x11, 0x01, 0x38, 0x00, "100K, Every 200K"		},
	{0x11, 0x01, 0x38, 0x38, "None"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x11, 0x01, 0xc0, 0xc0, "Normal"		},
	{0x11, 0x01, 0xc0, 0x80, "Hard"		},
	{0x11, 0x01, 0xc0, 0x40, "Harder"		},
	{0x11, 0x01, 0xc0, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x12, 0x01, 0x01, 0x01, "Off"		},
	{0x12, 0x01, 0x01, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x12, 0x01, 0x02, 0x02, "Upright"		},
	{0x12, 0x01, 0x02, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Play Together"		},
	{0x12, 0x01, 0x04, 0x00, "No"		},
	{0x12, 0x01, 0x04, 0x04, "Yes"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x12, 0x01, 0x08, 0x00, "No"		},
	{0x12, 0x01, 0x08, 0x08, "Yes"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x12, 0x01, 0x30, 0x30, "2"		},
	{0x12, 0x01, 0x30, 0x20, "3"		},
	{0x12, 0x01, 0x30, 0x10, "4"		},
	{0x12, 0x01, 0x30, 0x00, "5"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x40, 0x40, "Off"		},
	{0x12, 0x01, 0x40, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Invulnerability (Cheat)"		},
	{0x12, 0x01, 0x80, 0x80, "Off"		},
	{0x12, 0x01, 0x80, 0x00, "On"		},
};

STDDIPINFO(Rranger)

static struct BurnDIPInfo Hardhea2DIPList[]=
{
	{0x11, 0xff, 0xff, 0x5f, NULL		},
	{0x12, 0xff, 0xff, 0xf7, NULL		},
	{0x13, 0xff, 0xff, 0xff, NULL		},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x11, 0x01, 0x07, 0x00, "5 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x01, "4 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x02, "3 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x03, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x11, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},

	{0   , 0xfe, 0   ,    8, "Difficulty"		},
	{0x11, 0x01, 0x38, 0x38, "Easiest"		},
	{0x11, 0x01, 0x38, 0x30, "Very Easy"		},
	{0x11, 0x01, 0x38, 0x28, "Easy"		},
	{0x11, 0x01, 0x38, 0x20, "Moderate"		},
	{0x11, 0x01, 0x38, 0x18, "Normal"		},
	{0x11, 0x01, 0x38, 0x10, "Harder"		},
	{0x11, 0x01, 0x38, 0x08, "Very Hard"		},
	{0x11, 0x01, 0x38, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x11, 0x01, 0x40, 0x40, "Off"		},
	{0x11, 0x01, 0x40, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x11, 0x01, 0x80, 0x80, "Off"		},
	{0x11, 0x01, 0x80, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x12, 0x01, 0x01, 0x01, "Off"		},
	{0x12, 0x01, 0x01, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x12, 0x01, 0x02, 0x02, "Upright"		},
	{0x12, 0x01, 0x02, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Play Together"		},
	{0x12, 0x01, 0x04, 0x00, "No"		},
	{0x12, 0x01, 0x04, 0x04, "Yes"		},

	{0   , 0xfe, 0   ,    8, "Bonus Life"		},
	{0x12, 0x01, 0x38, 0x30, "10K"		},
	{0x12, 0x01, 0x38, 0x28, "30K"		},
	{0x12, 0x01, 0x38, 0x18, "50K, Every 50K"		},
	{0x12, 0x01, 0x38, 0x20, "50K"		},
	{0x12, 0x01, 0x38, 0x10, "100K, Every 50K"		},
	{0x12, 0x01, 0x38, 0x08, "100K, Every 100K"		},
	{0x12, 0x01, 0x38, 0x00, "200K, Every 100K"		},
	{0x12, 0x01, 0x38, 0x38, "None"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x12, 0x01, 0xc0, 0x80, "2"		},
	{0x12, 0x01, 0xc0, 0xc0, "3"		},
	{0x12, 0x01, 0xc0, 0x40, "4"		},
	{0x12, 0x01, 0xc0, 0x00, "5"		},
};

STDDIPINFO(Hardhea2)

static struct BurnDIPInfo SparkmanDIPList[]=
{
	{0x13, 0xff, 0xff, 0x5f, NULL		},
	{0x14, 0xff, 0xff, 0xff, NULL		},
	{0x15, 0xff, 0xff, 0xff, NULL		},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x13, 0x01, 0x07, 0x00, "5 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x01, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x02, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x03, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},

	{0   , 0xfe, 0   ,    8, "Difficulty"		},
	{0x13, 0x01, 0x38, 0x20, "Easiest"		},
	{0x13, 0x01, 0x38, 0x30, "Very Easy"		},
	{0x13, 0x01, 0x38, 0x28, "Easy"		},
	{0x13, 0x01, 0x38, 0x38, "Moderate"		},
	{0x13, 0x01, 0x38, 0x18, "Normal"		},
	{0x13, 0x01, 0x38, 0x10, "Harder"		},
	{0x13, 0x01, 0x38, 0x08, "Very Hard"		},
	{0x13, 0x01, 0x38, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x13, 0x01, 0x40, 0x40, "Off"		},
	{0x13, 0x01, 0x40, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x14, 0x01, 0x80, 0x80, "Off"		},
	{0x14, 0x01, 0x80, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x14, 0x01, 0x01, 0x01, "Off"		},
	{0x14, 0x01, 0x01, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x14, 0x01, 0x02, 0x02, "Upright"		},
	{0x14, 0x01, 0x02, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Play Together"		},
	{0x14, 0x01, 0x04, 0x00, "No"		},
	{0x14, 0x01, 0x04, 0x04, "Yes"		},

	{0   , 0xfe, 0   ,    8, "Bonus Life"		},
	{0x14, 0x01, 0x38, 0x38, "10K"		},
	{0x14, 0x01, 0x38, 0x28, "30K"		},
	{0x14, 0x01, 0x38, 0x18, "50K, Every 50K"		},
	{0x14, 0x01, 0x38, 0x20, "50K"		},
	{0x14, 0x01, 0x38, 0x10, "100K, Every 50K"		},
	{0x14, 0x01, 0x38, 0x08, "100K, Every 100K"		},
	{0x14, 0x01, 0x38, 0x00, "200K, Every 100K"		},
	{0x14, 0x01, 0x38, 0x30, "None"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0xc0, 0x80, "2"		},
	{0x14, 0x01, 0xc0, 0xc0, "3"		},
	{0x14, 0x01, 0xc0, 0x40, "4"		},
	{0x14, 0x01, 0xc0, 0x00, "5"		},
};

STDDIPINFO(Sparkman)

static struct BurnDIPInfo StarfighDIPList[]=
{
	{0x11, 0xff, 0xff, 0x5f, NULL		},
	{0x12, 0xff, 0xff, 0xf7, NULL		},
	{0x13, 0xff, 0xff, 0xff, NULL		},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x11, 0x01, 0x07, 0x00, "5 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x01, "4 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x02, "3 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x03, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x11, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},

	{0   , 0xfe, 0   ,    8, "Difficulty"		},
	{0x11, 0x01, 0x38, 0x38, "Easiest"		},
	{0x11, 0x01, 0x38, 0x30, "Very Easy"		},
	{0x11, 0x01, 0x38, 0x28, "Easy"		},
	{0x11, 0x01, 0x38, 0x20, "Moderate"		},
	{0x11, 0x01, 0x38, 0x18, "Normal"		},
	{0x11, 0x01, 0x38, 0x10, "Harder"		},
	{0x11, 0x01, 0x38, 0x08, "Very Hard"		},
	{0x11, 0x01, 0x38, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x11, 0x01, 0x40, 0x40, "Off"		},
	{0x11, 0x01, 0x40, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x11, 0x01, 0x80, 0x80, "Off"		},
	{0x11, 0x01, 0x80, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x12, 0x01, 0x01, 0x01, "Off"		},
	{0x12, 0x01, 0x01, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x12, 0x01, 0x02, 0x02, "Upright"		},
	{0x12, 0x01, 0x02, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Play Together"		},
	{0x12, 0x01, 0x04, 0x00, "No"		},
	{0x12, 0x01, 0x04, 0x04, "Yes"		},

	{0   , 0xfe, 0   ,    8, "Bonus Life"		},
	{0x12, 0x01, 0x38, 0x30, "10K"		},
	{0x12, 0x01, 0x38, 0x28, "30K"		},
	{0x12, 0x01, 0x38, 0x18, "50K, Every 50K"		},
	{0x12, 0x01, 0x38, 0x20, "50K"		},
	{0x12, 0x01, 0x38, 0x10, "100K, Every 50K"		},
	{0x12, 0x01, 0x38, 0x08, "100K, Every 100K"		},
	{0x12, 0x01, 0x38, 0x00, "200K, Every 100K"		},
	{0x12, 0x01, 0x38, 0x38, "None"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x12, 0x01, 0xc0, 0x80, "2"		},
	{0x12, 0x01, 0xc0, 0xc0, "3"		},
	{0x12, 0x01, 0xc0, 0x40, "4"		},
	{0x12, 0x01, 0xc0, 0x00, "5"		},

	{0   , 0xfe, 0   ,    2, "3: Copyright Screen Color + ?"		},
	{0x13, 0x01, 0x08, 0x08, "Green"		},
	{0x13, 0x01, 0x08, 0x00, "Blue"		},
};

STDDIPINFO(Starfigh)

static void play_sample(INT32 sample)
{
	//bprintf (0, _T("Play sample: %d\n"), sample);
	sample_start = sample * 0x1000;
	sample_offset = 0;
}

static void sample_render(INT16 *buffer, INT32 nLen)
{
	if (sample_start < 0) return; // stopped

	if (sample_start + (sample_offset >> 16) >= 0x20000 ) {
		bprintf(0, _T("Bad sample start!\n"));
		sample_start = -1; // stop
		sample_offset = 0;
		return;
	}

	INT32 step = (8000 << 16) / nBurnSoundRate; // 8khz
	INT32 pos = 0;
	INT16 *rom = DrvSamplesExp + sample_start;

	while (pos < nLen)
	{
		INT32 sample = (INT32)(rom[(sample_offset >> 16)] * 0.2);

		buffer[0] = BURN_SND_CLIP((INT32)(buffer[0] + sample));
		buffer[1] = BURN_SND_CLIP((INT32)(buffer[1] + sample));

		sample_offset += step;

		buffer+=2;
		pos++;

		if (sample_offset >= 0xfff0000) {
			sample_start = -1; // stop
			sample_offset = 0;
			break;
		}
	}
}

static void bankswitch(INT32 bank)
{
	bank &= 0x0f;
	*mainbank = bank;

	ZetMapMemory(DrvZ80ROM0 + 0x10000 + (bank * 0x4000), 0x8000, 0xbfff, MAP_ROM); // bank
}

static void palette_update(UINT16 offset)
{
	UINT16 p = DrvPalRAM[(offset & ~1) + 1] + (DrvPalRAM[(offset & ~1) + 0] * 256);

	UINT8 r = (p >> 12) & 0xf;
	UINT8 g = (p >>  8) & 0xf;
	UINT8 b = (p >>  4) & 0xf;

	DrvPalette[offset/2] = BurnHighCol((r*16)+r, (g*16)+g, (b*16)+b, 0);
}

static UINT8 hardhead_protection_read(UINT8 offset)
{
	if (protection_val & 0x80)
		return (~offset & 0x20) | ((protection_val & 0x04) << 5) | ((protection_val & 0x01) << 2);
	else
		return (~offset & 0x20) | (((offset ^ protection_val) & 0x01) ? 0x84 : 0);
}

static void __fastcall hardhead_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfe00) == 0xd800) {
		DrvPalRAM[address & 0x1ff] = data;
		palette_update(address & 0x1ff);
		return;
	}

	if ((address & 0xff80) == 0xdd80) {
		protection_val = (data & 0x80) ? data : (address & 1);
		return;
	}

	switch (address)
	{
		case 0xda00:
			hardhead_ip = data;
		return;

		case 0xda80:
			bankswitch(data);
		return;

		case 0xdb00:
			*soundlatch = data;
		return;

		case 0xdb80:
			*flipscreen = data & 0x04;
		return;
	}
}

static UINT8 __fastcall hardhead_read(UINT16 address)
{
	if ((address & 0xff80) == 0xdd80) {
		return hardhead_protection_read(address);
	}

	switch (address)
	{
		case 0xda00:
			switch (hardhead_ip) {
				case 0: return DrvInputs[0];
				case 1: return DrvInputs[1];
				case 2: return DrvDips[0];
				case 3: return DrvDips[1];
			}
			return 0xff;

		case 0xda80:
			return *soundlatch2;
	}

	return 0;
}


static void __fastcall rranger_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfe00) == 0xc600) {
		DrvPalRAM[address & 0x1ff] = data;
		palette_update(address & 0x1ff);
		return;
	}

	switch (address)
	{
		case 0xc000:
			*soundlatch = data;
		return;

		case 0xc002:
			*flipscreen = data & 0x20;
			bankswitch((data & 0x07) + (((data & 0x14) == 0x04) ? 4 : 0)); // weird
		return;

		case 0xc200:
			ZetWriteByte(0xcd99, 0xff); // really?
		return;

		case 0xc280: // nop
		return;
	}
}

static UINT8 __fastcall rranger_read(UINT16 address)
{
	switch (address)
	{
		case 0xc000:
			watchdog_enable = 1;
			watchdog = 0;
			return 0;

		case 0xc002:
		case 0xc003:
			return DrvInputs[address & 1];

		case 0xc004:
		//	return *soundlatch2;
			return 0x02; // hacky

		case 0xc280:
			return DrvDips[0];

		case 0xc2c0:
			return DrvDips[1];
	}

	return 0;
}


static void __fastcall hardhea2_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfe00) == 0xc600) {
		DrvPalRAM[address & 0x1ff] = data;
		palette_update(address & 0x1ff);
		return;
	}

	switch (address)
	{
		case 0xc200:
			m_spritebank = ((data & 0x02) ? 1 : 0);
			ZetMapMemory(DrvSprRAM + (m_spritebank * 0x2000), 0xe000, 0xffff, MAP_RAM);
		return;

		case 0xc280:
		case 0xc28c:
			bankswitch(data);
		return;

		case 0xc300:
			*flipscreen = data & 0x01;
		return;

		case 0xc380:
			*nmi_enable = data & 0x01;
		return;

		case 0xc400:
			// leds, coin counters
		return;

		case 0xc480:
		// ?
		return;

		case 0xc500:
			*soundlatch = data;
		return;

		case 0xc508:
			m_spritebank = 0;
			ZetMapMemory(DrvSprRAM + 0x0000, 0xe000, 0xffff, MAP_RAM);
		return;

		case 0xc50f:
			m_spritebank = 1;
			ZetMapMemory(DrvSprRAM + 0x2000, 0xe000, 0xffff, MAP_RAM);
		return;

		case 0xc507:
		case 0xc556:
		case 0xc560:
			m_rambank = 1;
			ZetMapMemory(DrvZ80RAM0 + 0x1800, 0xc800, 0xdfff, MAP_RAM);
		return;

		case 0xc522:
		case 0xc528:
		case 0xc533:
			m_rambank = 0;
			ZetMapMemory(DrvZ80RAM0 + 0x0000, 0xc800, 0xdfff, MAP_RAM);
		return;	
	}
}

static UINT8 __fastcall hardhea2_read(UINT16 address)
{
	switch (address)
	{
		case 0xc000:
		case 0xc001:
			return DrvInputs[address & 1];

		case 0xc002:
		case 0xc003:
			return DrvDips[address & 1];

		case 0xc080:
			return (DrvInputs[2] & ~0x40) | (vblank ? 0x40 : 0);
	}

	return 0;
}



static void __fastcall sparkman_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfe00) == 0xc600) {
		DrvPalRAM[address & 0x1ff] = data;
		palette_update(address & 0x1ff);
		return;
	}

	if (address >= 0xc200) address &= ~0x007f; // mirror misc. writes

	switch (address)
	{
		case 0xc200: {
			m_spritebank = ((data >> 1) & 0x01) | ((data << 1) & 0x02);
			if ((m_spritebank_latch >> 1) & 0x01)
				m_spritebank ^= 0x03;

			INT32 bank = m_spritebank * 0x2000;

			ZetMapMemory(DrvSprRAM + bank, 0xe000, 0xffff, MAP_RAM);
		}
		return;

		case 0xc280:
			m_rombank_latch = data;
		return;

		case 0xc300:
			*flipscreen = data & 0x01;
			m_spritebank_latch = (data >> 4) & 0x03;
		return;

		case 0xc380:
			disable_mainram_write = (data & 0x01);
			*nmi_enable = data & 0x20;
			if (disable_mainram_write) {
				ZetUnmapMemory(0xc800,0xdfff, MAP_WRITE);
			} else {
				ZetMapMemory(DrvZ80RAM0,0xc800,0xdfff, MAP_WRITE);
			}
		return;

		case 0xc400:
			// leds, 0, 1, 0x01, 0x02
			bankswitch(m_rombank_latch & 0x0f);
		return;

		case 0xc480:
			// coin counters
		return;

		case 0xc500:
			if (~m_rombank_latch & 0x20)
				*soundlatch = data;
		return;
	}
}

static UINT8 __fastcall sparkman_read(UINT16 address)
{
	switch (address)
	{
		case 0xc000:
		case 0xc001:
			return DrvInputs[address & 1];

		case 0xc002:
		case 0xc003:
			return DrvDips[address & 1];

		case 0xc080:
			return (DrvInputs[2] & 0x3) | (vblank ? 0x40 : 0);

		case 0xc0a3:
			return (nCurrentFrame & 1) ? 0x80 : 0;
	}

	return 0;
}


static void __fastcall starfigh_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfe00) == 0xc600) {
		DrvPalRAM[address & 0x1ff] = data;
		palette_update(address & 0x1ff);
		return;
	}

	if (address >= 0xc200) address &= ~0x007f; // mirror misc. writes

	switch (address)
	{
		case 0xc200:
			m_spritebank = m_spritebank_latch;
			ZetMapMemory(DrvSprRAM + m_spritebank * 0x2000, 0xe000, 0xffff, MAP_RAM);
		return;

		case 0xc280:
			m_rombank_latch = data;
		return;

		case 0xc300:
			*flipscreen = data & 0x01;
		return;

		case 0xc380:
			m_spritebank_latch =(data & 0x04) >> 2;
			*nmi_enable        = data & 0x20;
		return;

		case 0xc400:
			// leds -> 0,1, 0x01, 0x02
			// coin counter 0x04
			m_gfxbank = (data & 0x08) ? 4 : 0;
			bankswitch(m_rombank_latch & 0x0f);
		return;

		case 0xc500:
			if (~m_rombank_latch & 0x20)
				*soundlatch = data;
		return;
	}
}



static void __fastcall hardhead_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xa000:
		case 0xa001:
			BurnYM3812Write(0, address & 1, data);
		return;

		case 0xa002:
		case 0xa003:
			AY8910Write(0, address & 1, data);
		return;

		case 0xd000:
			*soundlatch2 = data;
		return;
	}
}

static UINT8 __fastcall hardhead_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xa000:
		case 0xa001:
			return BurnYM3812Read(0, address & 1);

		case 0xa002:
		case 0xa003:
			return AY8910Read(0);

		case 0xc800:
			return BurnYM3812Read(0, address & 1); // 3812 status_port ???????????????????????????

		case 0xd800:
			return *soundlatch;
	}

	return 0;
}

static void __fastcall rranger_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xa000:
		case 0xa001:
			BurnYM2203Write(0, address & 1, data);
		return;

		case 0xa002:
		case 0xa003:
			BurnYM2203Write(1, address & 1, data);
		return;

		case 0xd000:
			*soundlatch2 = data;
		return;
	}
}

static UINT8 __fastcall rranger_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xd800:
			return *soundlatch;
	}

	return 0;
}

static void __fastcall hardhea2_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xc000:
		case 0xc001:
			BurnYM3812Write(0, address & 1, data);
		return;

		case 0xc002:
		case 0xc003:
			AY8910Write(0, address & 1, data);
		return;

		case 0xf000:
			*soundlatch2 = data;
		return;
	}
}

static UINT8 __fastcall hardhea2_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xc000:
		case 0xc001:
			return BurnYM3812Read(0, address & 1);

		case 0xc002:
		case 0xc003:
			return AY8910Read(0);

		case 0xf800:
			return *soundlatch;
	}

	return 0;
}

static void __fastcall hardhea2_pcm_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
			DACSignedWrite(port & 3, data);
		return;
	}
}

static UINT8 __fastcall hardhea2_pcm_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
			return *soundlatch2;
	}

	return 0;
}

static void hardhead_ay8910_write_A(UINT32, UINT32 data)
{
	if (data)
	{
		if ( ~data & 0x10 )
		{
			play_sample(sample_number);
		}
		else if ( ~data & 0x08 )
		{
			play_sample((sample_number& 3) + 7);
		}
		else if ( ~data & 0x40 )    // sparkman, second sample rom
		{
			play_sample(sample_number + 0x10);
		}
	}
}

static void hardhead_ay8910_write_B(UINT32, UINT32 data)
{
	sample_number = data & 0x0f;
}

static void rranger_ay8910_write_A(UINT32, UINT32 data)
{
	if (data)
	{
		if ((sample_number != 0) && (~data & 0x30))
		{
			play_sample(sample_number);
		}
	}
}

static void sound_type1_fm_irq_handler(INT32, INT32 nStatus)
{
	ZetSetIRQLine(0, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

inline static INT32 hardhead_fm_syncronize(INT32 nSoundRate)
{
	return (INT64)ZetTotalCycles() * nSoundRate / 3000000;
}

static INT32 DrvSyncDAC()
{
	return (INT32)(float)(nBurnSoundLen * (ZetTotalCycles() / (6000000.000 / (nBurnFPS / 100.000))));
}

inline static INT32 rranger_fm_syncronize(INT32 nSoundRate)
{
	return (INT64)ZetTotalCycles() * nSoundRate / 6000000;
}

inline static double rranger_get_time()
{
	return (double)ZetTotalCycles() / 6000000.0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x050000;
	DrvZ80Decrypted		= Next; Next += 0x050000;
	DrvZ80ROM1		= Next; Next += 0x010000;
	DrvZ80ROM2		= Next; Next += 0x010000;

	DrvSampleROM		= Next; Next += 0x010000;
	DrvSamplesExp		= (INT16*)Next; Next += 0x20000 * sizeof(INT16);

	DrvGfxROM0		= Next; Next += 0x200000;
	DrvGfxROM1		= Next; Next += 0x200000;

	DrvPalette		= (UINT32*)Next; Next += 0x0200 * sizeof(UINT32);

	AllRam			= Next;

	DrvSprRAM		= Next; Next += 0x008000;
	DrvZ80RAM0		= Next; Next += 0x004800;
	DrvZ80RAM1		= Next; Next += 0x000800;
	DrvPalRAM		= Next; Next += 0x000200;

	soundlatch		= Next; Next += 0x000001;
	soundlatch2		= Next; Next += 0x000001;
	flipscreen		= Next; Next += 0x000001;
	nmi_enable		= Next; Next += 0x000001;
	mainbank		= Next; Next += 0x000001;

	RamEnd			= Next;

	pAY8910Buffer[0] 	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[1] 	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[2] 	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);

	MemEnd			= Next;

	return 0;
}

static void DrvExpandSamples(INT32 len)
{
	for (INT32 i = 0; i < len; i++) {
		DrvSamplesExp[i] = (INT8)(((DrvSampleROM[i/2] << ((i & 1) ? 0 : 4)) & 0xf0)  ^ 0x80) * 0x100;
	}
}

static INT32 DrvGfxDecode(UINT8 *src, INT32 len)
{
	INT32 Plane[4] = { (len/2)*8 + 0, (len/2)*8 + 4, 0, 4 };
	INT32 XOffs[8] = { STEP4(3,-1), STEP4(11,-1) };
	INT32 YOffs[8] = { STEP8(0,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(len);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, src, len);

	GfxDecode((len*2)/0x40, 4, 8, 8, Plane, XOffs, YOffs, 0x080, tmp, src);

	BurnFree(tmp);

	return 0;
}

static void HardheadDecrypt()
{
	for (INT32 i = 0; i < 0x8000; i++)
	{
		static const UINT8 swaptable[8] =
		{
			1,1,0,1,1,1,1,0
		};
		int table = ((i & 0x0c00) >> 10) | ((i & 0x4000) >> 12);

		if (swaptable[table]) {
			DrvZ80ROM0[i] = BITSWAP08(DrvZ80ROM0[i], 7,6,5,3,4,2,1,0) ^ 0x58;
		}
	}
}

static void hardhea2_decrypt()
{
	UINT8   *RAM    =   DrvZ80ROM0;
	UINT8   *decrypt =  DrvZ80Decrypted;

	memcpy (decrypt, RAM, 0x50000);

	for (INT32 i = 0x00000; i < 0x50000; i++)
	{
		static const UINT8 swaptable[0x50] =
		{
			1,1,1,1,0,0,1,1,0,0,0,0,0,0,0,0,
			1,1,0,0,0,0,0,0,0,0,0,0,1,1,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			1,1,0,0,0,0,0,0,1,1,0,0,1,1,0,0
		};
		int addr = i;

		if (swaptable[(i & 0xff000) >> 12])
			addr = (addr & 0xf0000) | BITSWAP16(addr, 15,14,13,12,11,10,9,8,6,7,5,4,3,2,1,0);

		RAM[i] = decrypt[addr];
	}

	for (INT32 i = 0; i < 0x8000; i++)
	{
		static const UINT8 swaptable[32] =
		{
			1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,
			1,1,0,1,1,1,1,1,1,1,1,1,0,1,0,0
		};
		static const UINT8 xortable[32] =
		{
			0x04,0x04,0x00,0x04,0x00,0x04,0x00,0x00,0x04,0x45,0x00,0x04,0x00,0x04,0x00,0x00,
			0x04,0x45,0x00,0x04,0x00,0x04,0x00,0x00,0x04,0x04,0x00,0x04,0x00,0x04,0x00,0x00
		};
		int table = (i & 1) | ((i & 0x400) >> 9) | ((i & 0x7000) >> 10);

		UINT8 x = RAM[i];

		x = BITSWAP08(x, 7,6,5,3,4,2,1,0) ^ 0x41 ^ xortable[table];
		if (swaptable[table])
			x = BITSWAP08(x, 5,6,7,4,3,2,1,0);

		decrypt[i] = x;
	}

	for (INT32 i = 0; i < 0x8000; i++)
	{
		static const UINT8 swaptable[8] = { 1,1,0,1,0,1,1,0 };

		if (swaptable[(i & 0x7000) >> 12])
			RAM[i] = BITSWAP08(RAM[i], 5,6,7,4,3,2,1,0) ^ 0x41;
	}
}

static void sparkman_decrypt()
{
	UINT8   *RAM    =   DrvZ80ROM0;
	size_t  size    =   0x50000;
	UINT8   *decrypt =  DrvZ80Decrypted;
	UINT8 x;
	int i;

	memcpy(decrypt, RAM, size);
	for (i = 0; i < 0x50000; i++)
	{
		static const UINT8 swaptable[0x50] =
		{
			1,1,1,1,    0,0,1,1,    0,0,0,0,    0,0,0,0,    // 8000-ffff not used
			0,0,0,0,    0,0,0,0,    0,0,0,0,    0,0,0,0,
			0,0,0,0,    0,0,0,0,    0,0,0,0,    0,0,0,0,
			0,0,0,0,    0,0,0,0,    0,0,0,0,    0,0,0,0,
			0,0,0,0,    0,0,0,0,    1,1,0,0,    0,0,0,0     // bank $0e, $8xxx, $9xxx (hand in title screen)
		};
		int addr = i;

		if (swaptable[(i & 0xff000) >> 12])
			addr = BITSWAP24(addr, 23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,7,8,6,5,4,3,2,1,0);

		RAM[i] = decrypt[addr];
	}

	/* Opcodes */
	for (i = 0; i < 0x8000; i++)
	{
		static const UINT8 swaptable[32] =
		{
			0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,1,
			0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0
		};
		static const UINT8 xortable[32] =
		{
			0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
			0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x00,0x00
		};
		int table = (i & 0x7c00) >> 10;

		x = RAM[i];

		x = BITSWAP08(x, 5,6,7,3,4,2,1,0) ^ 0x44 ^ xortable[table];
		if (swaptable[table])
			x = BITSWAP08(x, 5,6,7,4,3,2,1,0) ^ 0x04;

		decrypt[i] = x;
	}

	/* Data */
	for (i = 0; i < 0x8000; i++)
	{
		static const UINT8 swaptable[8] = { 1,1,1,0,1,1,0,1 };

		if (swaptable[(i & 0x7000) >> 12])
			RAM[i] = BITSWAP08(RAM[i], 5,6,7,4,3,2,1,0) ^ 0x44;
	}

	// !!!!!! PATCHES !!!!!!

	// c083 bit 7 protection
	decrypt[0x0ee0] = 0x00;
	decrypt[0x0ee1] = 0x00;
	decrypt[0x0ee2] = 0x00;

	// c083 bit 7 protection
	decrypt[0x1ac3] = 0x00;
	decrypt[0x1ac4] = 0x00;
	decrypt[0x1ac5] = 0x00;
}


static void CommonDoReset(INT32 clear_ram)
{
	if (clear_ram) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	ZetOpen(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	ZetClose();

	watchdog_enable = 0;
	watchdog = 0;

	sample_start = -1;
	sample_offset = 0;
	sample_number = 0;

	m_gfxbank = 0;
	m_palettebank = 0;
	m_spritebank = 0;
	m_spritebank_latch = 0;
	m_rombank_latch = 0;
	m_rambank = 0;
	disable_mainram_write = 0;
	protection_val = 0;
	hardhead_ip = 0;

	HiscoreReset();
}

static INT32 HardheadDoReset()
{
	CommonDoReset(1);

	AY8910Reset(0);
	BurnYM3812Reset();

	return 0;
}

static INT32 HardheadInit()
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
		if (BurnLoadRom(DrvZ80ROM0 + 0x30000,  5, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x38000,  6, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x40000,  7, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  8, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x10000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x20000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x30000, 12, 1)) return 1;

		for (INT32 i = 0x3ffff; i >= 0; i--) {	// invert & mirror
			DrvGfxROM0[i] = (DrvGfxROM0[i & ~0x8000]) ^ 0xff;
		}

		if (BurnLoadRom(DrvSampleROM + 0x000, 13, 1)) return 1;

		DrvGfxDecode(DrvGfxROM0, 0x40000);
		HardheadDecrypt();
		DrvExpandSamples(0x10000);
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0x7fff, MAP_ROM);
	bankswitch(0); // ?
	ZetMapMemory(DrvZ80RAM0,		0xc000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvPalRAM,			0xd800, 0xd9ff, MAP_ROM);
	ZetMapMemory(DrvSprRAM,			0xe000, 0xffff, MAP_RAM);
	ZetSetWriteHandler(hardhead_write);
	ZetSetReadHandler(hardhead_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,		0xc000, 0xc7ff, MAP_RAM);
	ZetSetWriteHandler(hardhead_sound_write);
	ZetSetReadHandler(hardhead_sound_read);
	ZetClose();

	BurnYM3812Init(1, 3000000, (0 ? &sound_type1_fm_irq_handler : NULL), hardhead_fm_syncronize, 0);
	BurnTimerAttachZetYM3812(3000000);
	BurnYM3812SetRoute(0, BURN_SND_YM3812_ROUTE, 1.00, BURN_SND_ROUTE_BOTH);

	AY8910Init(0, 1500000, nBurnSoundRate, NULL, NULL, &hardhead_ay8910_write_A, &hardhead_ay8910_write_B);
	AY8910SetAllRoutes(0, 0.30, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	HardheadDoReset();

	return 0;
}

static INT32 SparkmanInit()
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
		if (BurnLoadRom(DrvZ80ROM0 + 0x20000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x30000,  3, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x40000,  4, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  5, 1)) return 1;

		memset (DrvGfxROM0,0xff,0x100000);
		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x10000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x40000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x80000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x90000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0xc0000, 11, 1)) return 1;

		for (INT32 i = 0xfffff; i >= 0; i--) {	// invert
			DrvGfxROM0[i] ^= 0xff;
		}

		memset (DrvGfxROM1,0xff,0x100000);

		if (BurnLoadRom(DrvGfxROM1 + 0x00000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x80000, 13, 1)) return 1;

		for (INT32 i = 0xfffff; i >= 0; i--) {	// invert
			DrvGfxROM1[i] = DrvGfxROM1[i & ~0x60000] ^ 0xff;
		}

		if (BurnLoadRom(DrvSampleROM + 0x0000, 14, 1)) return 1;
		if (BurnLoadRom(DrvSampleROM + 0x8000, 15, 1)) return 1;

		DrvGfxDecode(DrvGfxROM0, 0x100000);
		DrvGfxDecode(DrvGfxROM1, 0x100000);
		sparkman_decrypt();
		DrvExpandSamples(0x20000);
	}

	ZetInit(0);
	ZetOpen(0);
	//ZetMapMemory(DrvZ80ROM0,		0x0000, 0x7fff, MAP_ROM);
	ZetMapArea(0x0000, 0x7fff, 0, DrvZ80ROM0);
	ZetMapArea(0x0000, 0x7fff, 2, DrvZ80Decrypted, DrvZ80ROM0);
	//bankswitch(0); // ?
	ZetMapMemory(DrvPalRAM,			0xc600, 0xc7ff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,		0xc800, 0xdfff, MAP_ROM);
	ZetMapMemory(DrvSprRAM,			0xe000, 0xffff, MAP_RAM); // banked
	ZetSetWriteHandler(sparkman_write);
	ZetSetReadHandler(sparkman_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,		0xc000, 0xc7ff, MAP_RAM);
	ZetSetWriteHandler(hardhead_sound_write);
	ZetSetReadHandler(hardhead_sound_read);
	ZetClose();

	BurnYM3812Init(1, 4000000, NULL, rranger_fm_syncronize, 0);
	BurnTimerAttachZetYM3812(6000000);
	BurnYM3812SetRoute(0, BURN_SND_YM3812_ROUTE, 1.00, BURN_SND_ROUTE_BOTH);

	AY8910Init(0, 1500000, nBurnSoundRate, NULL, NULL, &hardhead_ay8910_write_A, &hardhead_ay8910_write_B);
	AY8910SetAllRoutes(0, 0.30, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	HardheadDoReset();

	Sparkman = 1;

	return 0;
}

static void starfigh_decrypt()
{
	UINT8   *RAM    =   DrvZ80ROM0;
	INT32	size	=   0x50000;
	UINT8   *decrypt =  DrvZ80Decrypted;
	UINT8 x;
	int i;

	/* Address lines scrambling */
	memcpy(decrypt, RAM, size);
	for (i = 0; i < 0x50000; i++)
	{
		static const UINT8 swaptable[0x50] =
		{
			1,1,1,1,    1,1,0,0,    0,0,0,0,    0,0,0,0,    // 8000-ffff not used
			0,0,0,0,    0,0,0,0,    0,0,0,0,    0,0,0,0,
			0,0,0,0,    0,0,0,0,    0,0,0,0,    0,0,0,0,
			0,0,0,0,    0,0,0,0,    0,0,0,0,    0,0,0,0,
			0,0,0,0,    0,0,0,0,    1,1,0,0,    0,0,0,0     // bank $0e, 9c80 (boss 1) and 8350 (first wave)
		};
		int addr = i;

		if (swaptable[(i & 0xff000) >> 12])
			addr = BITSWAP24(addr, 23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,6,7,5,4,3,2,1,0);

		RAM[i] = decrypt[addr];
	}

	/* Opcodes */
	for (i = 0; i < 0x8000; i++)
	{
		static const UINT8 swaptable[32] =
		{
			0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,
			0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0
		};
		static const UINT8 xortable[32] =
		{
			0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x01,0x41,0x01,0x00,0x00,0x00,0x00,
			0x01,0x01,0x41,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
		};
		int table = (i & 0x7c00) >> 10;

		x = RAM[i];

		x = BITSWAP08(x, 5,6,7,3,4,2,1,0) ^ 0x45 ^ xortable[table];
		if (swaptable[table])
			x = BITSWAP08(x, 5,6,7,4,3,2,1,0) ^ 0x04;

		decrypt[i] = x;
	}

	/* Data */
	for (i = 0; i < 0x8000; i++)
	{
		static const UINT8 swaptable[8] = { 1,1,0,1,0,1,1,0 };

		if (swaptable[(i & 0x7000) >> 12])
			RAM[i] = BITSWAP08(RAM[i], 5,6,7,4,3,2,1,0) ^ 0x45;
	}


	// !!!!!! PATCHES !!!!!!

	decrypt[0x07c0] = 0xc9; // c080 bit 7 protection check

//  decrypt[0x083e] = 0x00; // sound latch disabling
//  decrypt[0x083f] = 0x00; // ""
//  decrypt[0x0840] = 0x00; // ""

//  decrypt[0x0cef] = 0xc9; // rombank latch check, corrupt d12d

	decrypt[0x2696] = 0xc9; // work ram writes disable, corrupt next routine
	decrypt[0x4e9a] = 0x00; // work ram writes disable, flip background sprite
}


static INT32 StarfighInit()
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
		if (BurnLoadRom(DrvZ80ROM0 + 0x20000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x30000,  3, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x40000,  4, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x10000,  7, 1)) return 1;
		memcpy (DrvGfxROM0 + 0x20000, DrvGfxROM0 + 0x00000, 0x20000);
		if (BurnLoadRom(DrvGfxROM0 + 0x40000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x50000,  9, 1)) return 1;
		memcpy (DrvGfxROM0 + 0x60000, DrvGfxROM0 + 0x40000, 0x20000);
		if (BurnLoadRom(DrvGfxROM0 + 0x80000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x90000, 11, 1)) return 1;
		memcpy (DrvGfxROM0 + 0xa0000, DrvGfxROM0 + 0x80000, 0x20000);
		if (BurnLoadRom(DrvGfxROM0 + 0xc0000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0xd0000, 13, 1)) return 1;
		memcpy (DrvGfxROM0 + 0xe0000, DrvGfxROM0 + 0xc0000, 0x20000);

		for (INT32 i = 0xfffff; i >= 0; i--) {	// invert
			DrvGfxROM0[i] ^= 0xff;
		}

		if (BurnLoadRom(DrvSampleROM + 0x0000, 14, 1)) return 1;

		DrvGfxDecode(DrvGfxROM0, 0x100000);
		starfigh_decrypt();
		DrvExpandSamples(0x10000);
	}

	ZetInit(0);
	ZetOpen(0);
	//ZetMapMemory(DrvZ80ROM0,		0x0000, 0x7fff, MAP_ROM);
	ZetMapArea(0x0000, 0x7fff, 0, DrvZ80ROM0);
	ZetMapArea(0x0000, 0x7fff, 2, DrvZ80Decrypted, DrvZ80ROM0);
	//bankswitch(0); // ?
	ZetMapMemory(DrvPalRAM,			0xc600, 0xc7ff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,		0xc800, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0xe000, 0xffff, MAP_RAM); // banked
	ZetSetWriteHandler(starfigh_write);
	ZetSetReadHandler(sparkman_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,		0xc000, 0xc7ff, MAP_RAM);
	ZetSetWriteHandler(hardhead_sound_write);
	ZetSetReadHandler(hardhead_sound_read);
	ZetClose();

	BurnYM3812Init(1, 4000000, NULL, rranger_fm_syncronize, 0);
	BurnTimerAttachZetYM3812(6000000);
	BurnYM3812SetRoute(0, BURN_SND_YM3812_ROUTE, 1.00, BURN_SND_ROUTE_BOTH);

	AY8910Init(0, 1500000, nBurnSoundRate, NULL, NULL, &hardhead_ay8910_write_A, &hardhead_ay8910_write_B);
	AY8910SetAllRoutes(0, 0.30, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	HardheadDoReset();

	return 0;
}

static INT32 HardheadExit()
{
	GenericTilesExit();
	ZetExit();
	BurnYM3812Exit();
	AY8910Exit(0);

	BurnFree(AllMem);

	Sparkman = 0;

	return 0;
}

static INT32 RrangerDoReset(INT32 clear_ram)
{
	CommonDoReset(clear_ram);

	BurnYM2203Reset();

	return 0;
}

static INT32 RrangerInit()
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
		if (BurnLoadRom(DrvZ80ROM0 + 0x30000,  3, 1)) return 1;
		memcpy (DrvZ80ROM0 + 0x20000, DrvZ80ROM0 + 0x38000, 0x08000);
		if (BurnLoadRom(DrvZ80ROM0 + 0x38000,  4, 1)) return 1;
		memcpy (DrvZ80ROM0 + 0x28000, DrvZ80ROM0 + 0x40000, 0x08000);

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x08000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x10000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x18000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x20000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x28000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x30000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x38000, 13, 1)) return 1;

		for (INT32 i = 0x3ffff; i >= 0; i--) {	// invert
			DrvGfxROM0[i] = DrvGfxROM0[i] ^ 0xff;
		}

		if (BurnLoadRom(DrvSampleROM + 0x000, 14, 1)) return 1;

		DrvGfxDecode(DrvGfxROM0, 0x40000);
		DrvExpandSamples(0x10000);
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0x7fff, MAP_ROM);
	bankswitch(0); // ?
	ZetMapMemory(DrvPalRAM,			0xc600, 0xc7ff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,		0xc800, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0xe000, 0xffff, MAP_RAM);
	ZetSetWriteHandler(rranger_write);
	ZetSetReadHandler(rranger_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,		0xc000, 0xc7ff, MAP_RAM);
	ZetSetWriteHandler(rranger_sound_write);
	ZetSetReadHandler(rranger_sound_read);
	ZetClose();

	BurnYM2203Init(2, 4000000, NULL, rranger_fm_syncronize, rranger_get_time, 0);
	BurnYM2203SetPorts(0, NULL, NULL, &rranger_ay8910_write_A, &hardhead_ay8910_write_B);
	BurnTimerAttachZet(6000000);
	BurnYM2203SetAllRoutes(0, 0.90, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetAllRoutes(1, 0.90, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	RrangerDoReset(1);

	return 0;
}


static INT32 RrangerExit()
{
	GenericTilesExit();
	ZetExit();
	BurnYM2203Exit();

	BurnFree(AllMem);

	return 0;
}

static INT32 Hardhea2DoReset()
{
	CommonDoReset(1);

	ZetOpen(2);
	ZetReset();
	ZetClose();

	AY8910Reset(0);
	BurnYM3812Reset();
	DACReset();

	return 0;
}

static INT32 Hardhea2Init()
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
		if (BurnLoadRom(DrvZ80ROM0 + 0x20000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x30000,  3, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x40000,  4, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  5, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM2 + 0x00000,  6, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x10000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x20000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x30000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x40000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x50000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x60000, 13, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x70000, 14, 1)) return 1;

		for (INT32 i = 0x7ffff; i >= 0; i--) {	// invert
			DrvGfxROM0[i] ^= 0xff;
		}

		DrvGfxDecode(DrvGfxROM0, 0x80000);
		hardhea2_decrypt();
	}

	ZetInit(0);
	ZetOpen(0);
	//ZetMapMemory(DrvZ80ROM0,		0x0000, 0x7fff, MAP_ROM);
	ZetMapArea(0x0000, 0x7fff, 0, DrvZ80ROM0);
	ZetMapArea(0x0000, 0x7fff, 2, DrvZ80Decrypted, DrvZ80ROM0);
	bankswitch(0); // ?
	m_rambank = 0;
	ZetMapMemory(DrvPalRAM,			0xc600, 0xc7ff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,		0xc800, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0xe000, 0xffff, MAP_RAM);
	ZetSetWriteHandler(hardhea2_write);
	ZetSetReadHandler(hardhea2_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,		0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,		0xe000, 0xe7ff, MAP_RAM);
	ZetSetWriteHandler(hardhea2_sound_write);
	ZetSetReadHandler(hardhea2_sound_read);
	ZetClose();

	ZetInit(2);
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM2,		0x0000, 0xffff, MAP_ROM);
	ZetSetOutHandler(hardhea2_pcm_write_port);
	ZetSetInHandler(hardhea2_pcm_read_port);
	ZetClose();

	BurnYM3812Init(1, 3000000, sound_type1_fm_irq_handler, rranger_fm_syncronize, 0);
	BurnTimerAttachZetYM3812(6000000);
	BurnYM3812SetRoute(0, BURN_SND_YM3812_ROUTE, 1.00, BURN_SND_ROUTE_BOTH);

	AY8910Init(0, 1500000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.33, BURN_SND_ROUTE_BOTH);

	DACInit(0, 0, 1, DrvSyncDAC);
	DACSetRoute(0, 0.17, BURN_SND_ROUTE_LEFT);
	DACInit(1, 0, 1, DrvSyncDAC);
	DACSetRoute(1, 0.17, BURN_SND_ROUTE_RIGHT);
	DACInit(2, 0, 1, DrvSyncDAC);
	DACSetRoute(2, 0.17, BURN_SND_ROUTE_LEFT);
	DACInit(3, 0, 1, DrvSyncDAC);
	DACSetRoute(3, 0.17, BURN_SND_ROUTE_RIGHT);

	GenericTilesInit();

	Hardhea2DoReset();

	Hardhead2 = 1;

	return 0;
}

static INT32 Hardhea2Exit()
{
	GenericTilesExit();
	ZetExit();
	BurnYM3812Exit();
	AY8910Exit(0);
	DACExit();

	BurnFree(AllMem);

	Hardhead2 = 0;

	return 0;
}

static void draw_single_sprite(INT32 code, INT32 color, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy)
{
	if (sy == 0 || sy >= 240) return;

	sy -= 16;

	UINT8 *rom = (code & 0x8000) ? DrvGfxROM1 : DrvGfxROM0;
	code &= 0x7fff;

	if (flipy) {
		if (flipx) {
			Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 4, 0xf, 0, rom);
		} else {
			Render8x8Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 4, 0xf, 0, rom);			
		}
	} else {
		if (flipx) {
			Render8x8Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 4, 0xf, 0, rom);
		} else {
			Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 4, 0xf, 0, rom);			
		}
	}
}

static void draw_normal_sprites(INT32 which, INT32 new_style, INT32 gfxbank_type)
{
	UINT8 *spriteram = DrvSprRAM + which * 0x2000 * 2;

	int i;
	int mx = 0; // multisprite x counter

	int max_x = nScreenWidth - 8;
	int max_y = nScreenHeight - 8;

	for (i = 0x1d00; i < 0x2000; i += 4)
	{
		int srcpg, srcx,srcy, dimx,dimy, tx, ty;
		int gfxbank, colorbank = 0, flipx,flipy, multisprite;

		int y       =   spriteram[i + 0];
		int code    =   spriteram[i + 1];
		int x       =   spriteram[i + 2];
		int bank    =   spriteram[i + 3];

		if (!new_style)
		{
			flipx = 0;
			flipy = 0;
			gfxbank = bank & 0x3f;
			switch( code & 0x80 )
			{
			case 0x80:
				dimx = 2;                   dimy =  32;
				srcx  = (code & 0xf) * 2;   srcy = 0;
				srcpg = (code >> 4) & 3;
				break;
			case 0x00:
			default:
				dimx = 2;                   dimy =  2;
				srcx  = (code & 0xf) * 2;   srcy = ((code >> 5) & 0x3) * 8 + 6;
				srcpg = (code >> 4) & 1;
				break;
			}
			multisprite = ((code & 0x80) && (code & 0x40));
		}
		else
		{
			switch( code & 0xc0 )
			{
			case 0xc0:
				dimx = 4;                   dimy = 32;
				srcx  = (code & 0xe) * 2;   srcy = 0;
				flipx = (code & 0x1);
				flipy = 0;
				gfxbank = bank & 0x1f;
				srcpg = (code >> 4) & 3;
				break;
			case 0x80:
				dimx = 2;                   dimy = 32;
				srcx  = (code & 0xf) * 2;   srcy = 0;
				flipx = 0;
				flipy = 0;
				gfxbank = bank & 0x1f;
				srcpg = (code >> 4) & 3;
				break;
			case 0x40:
				dimx = 4;                   dimy = 4;
				srcx  = (code & 0xe) * 2;
				flipx = code & 0x01;
				flipy = bank & 0x10;
				srcy  = (((bank & 0x80)>>4) + (bank & 0x04) + ((~bank >> 4)&2)) * 2;
				srcpg = ((code >> 4) & 3) + 4;
				gfxbank = (bank & 0x3);
				switch (gfxbank_type)
				{
					case 0: //suna8_state::GFXBANK_TYPE_SPARKMAN:
						break;

					case 1: //suna8_state::GFXBANK_TYPE_BRICKZN:
						gfxbank += 4;
						break;

					case 2: //suna8_state::GFXBANK_TYPE_STARFIGH:
						if (gfxbank == 3) gfxbank += m_gfxbank;
						break;
				}
				colorbank = (bank & 8) >> 3;
				break;
			case 0x00:
			default:
				dimx = 2;                   dimy = 2;
				srcx  = (code & 0xf) * 2;
				flipx = 0;
				flipy = 0;
				srcy  = (((bank & 0x80)>>4) + (bank & 0x04) + ((~bank >> 4)&3)) * 2;
				srcpg = (code >> 4) & 3;
				gfxbank = bank & 0x03;
				switch (gfxbank_type)
				{
					case 2: // suna8_state::GFXBANK_TYPE_STARFIGH:
						// starfigh: boss 2 tail, p2 g7:
						//      61 20 1b 27
						if (gfxbank == 3)
							gfxbank += m_gfxbank;
					break;

					default:
					break;
				}
				break;
			}
			multisprite = ((code & 0x80) && (bank & 0x80));
		}

		x = x - ((bank & 0x40) ? 0x100 : 0);
		y = (0x100 - y - dimy*8 ) & 0xff;

		/* Multi Sprite */
		if ( multisprite )  {   mx += dimx*8;   x = mx; }
		else                    mx = x;

		gfxbank *= 0x400;

		for (ty = 0; ty < dimy; ty ++)
		{
			for (tx = 0; tx < dimx; tx ++)
			{
				int addr    =   (srcpg * 0x20 * 0x20) +
								((srcx + (flipx?dimx-tx-1:tx)) & 0x1f) * 0x20 +
								((srcy + (flipy?dimy-ty-1:ty)) & 0x1f);

				int tile    =   spriteram[addr*2 + 0];
				int attr    =   spriteram[addr*2 + 1];

				int tile_flipx  =   attr & 0x40;
				int tile_flipy  =   attr & 0x80;

				int sx      =    x + tx * 8;
				int sy      =   (y + ty * 8) & 0xff;

				if (flipx)  tile_flipx = !tile_flipx;
				if (flipy)  tile_flipy = !tile_flipy;

				if (*flipscreen)
				{
					sx = max_x - sx;    tile_flipx = !tile_flipx;
					sy = max_y - sy;    tile_flipy = !tile_flipy;
				}

				draw_single_sprite(tile + (attr & 0x3)*0x100 + gfxbank + (which*0x8000), (((attr >> 2) & 0xf) ^ colorbank) + 0x10 * m_palettebank, sx, sy, tile_flipx, tile_flipy);
			}
		}

	}
}

static void draw_text_sprites(INT32 text_dim)
{
	UINT8 *spriteram = DrvSprRAM;
	int i;

	int max_x = nScreenWidth - 8;
	int max_y = nScreenHeight - 8;

	for (i = 0x1900; i < 0x19ff; i += 4)
	{
		int srcpg, srcx,srcy, dimx,dimy, tx, ty;

		int y       =   spriteram[i + 0];
		int code    =   spriteram[i + 1];
		int x       =   spriteram[i + 2];
		int bank    =   spriteram[i + 3];

		if (~code & 0x80)   continue;

		dimx = 2;                   dimy = text_dim;
		srcx  = (code & 0xf) * 2;   srcy = (y & 0xf0) / 8;
		srcpg = (code >> 4) & 3;

		x = x - ((bank & 0x40) ? 0x100 : 0);
		y = 0;

		bank    =   (bank & 0x3f) * 0x400;

		for (ty = 0; ty < dimy; ty ++)
		{
			for (tx = 0; tx < dimx; tx ++)
			{
				int real_ty =   (ty < (dimy/2)) ? ty : (ty + 0x20 - dimy);

				int addr    =   (srcpg * 0x20 * 0x20) +
								((srcx + tx) & 0x1f) * 0x20 +
								((srcy + real_ty) & 0x1f);

				int tile    =   spriteram[addr*2 + 0];
				int attr    =   spriteram[addr*2 + 1];

				int flipx   =   attr & 0x40;
				int flipy   =   attr & 0x80;

				int sx      =    x + tx * 8;
				int sy      =   (y + real_ty * 8) & 0xff;

				if (*flipscreen)
				{
					sx = max_x - sx;    flipx = !flipx;
					sy = max_y - sy;    flipy = !flipy;
				}

				draw_single_sprite(tile + (attr & 0x3)*0x100 + bank, (attr >> 2) & 0xf, sx, sy, flipx, flipy);
			}
		}
	}
}

static INT32 DrvDraw(INT32 sprite2x, INT32 sprite_new, INT32 sprite_type, INT32 text_mode)
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x200; i+=2) {
			palette_update(i);
		}
		DrvRecalc = 0;
	}

	for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
		pTransDraw[i] = 0xff;
	}

	draw_normal_sprites(0,sprite_new,sprite_type);
	if (sprite2x) draw_normal_sprites(1,sprite_new,sprite_type);
	draw_text_sprites(text_mode);

	BurnTransferCopy(DrvPalette);
	
	return 0;
}

static INT32 HardheadDraw()
{
	return DrvDraw(0,0,0,12);
}

static INT32 RrangerDraw()
{
	return DrvDraw(0,0,0,8);
}

static INT32 Hardhea2Draw()
{
	return DrvDraw(0,1,1,8);
}

static INT32 SparkmanDraw()
{
	return DrvDraw(1,1,0,0);
}

static INT32 StarfighDraw()
{
	return DrvDraw(0,1,2,0);
}

static INT32 HardheadFrame()
{
	if (DrvReset) {
		HardheadDoReset();
	}

	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 3);

		for (INT32 i = 0 ; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nCyclesSegment;
	INT32 nInterleave = 16;
	INT32 nCyclesTotal[2] = { 6000000 / 60, 3000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesSegment = nCyclesTotal[0] / nInterleave;
		nCyclesDone[0] += ZetRun(nCyclesSegment);
		if (i == (nInterleave-1)) ZetSetIRQLine(0, CPU_IRQSTATUS_AUTO);
		ZetClose();

		ZetOpen(1);
		nCyclesSegment = (nCyclesTotal[1] / nInterleave) * (i + 1);
		BurnTimerUpdateYM3812(nCyclesSegment);
		if ((i % (nInterleave/4)) == ((nInterleave / 4) - 1)) {
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
		ZetClose();
	}

	ZetOpen(1);
	BurnTimerEndFrameYM3812(nCyclesTotal[1]);

	if (pBurnSoundOut) {		
		BurnYM3812Update(pBurnSoundOut, nBurnSoundLen);
		AY8910Render(&pAY8910Buffer[0], pBurnSoundOut, nBurnSoundLen, 1);
		sample_render(pBurnSoundOut, nBurnSoundLen);
	}
	ZetClose();

	if (pBurnDraw) {
		HardheadDraw();
	}

	return 0;
}

static INT32 RrangerFrame()
{
	watchdog++;
	if (watchdog >= 180 && watchdog_enable) {
		RrangerDoReset(0);
	}

	if (DrvReset) {
		RrangerDoReset(1);
	}

	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 2);

		for (INT32 i = 0 ; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nCyclesSegment;
	INT32 nInterleave = 16;
	INT32 nCyclesTotal[2] = { 6000000 / 60, 3000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesSegment = nCyclesTotal[0] / nInterleave;
		nCyclesDone[0] += ZetRun(nCyclesSegment);
		if (i == (nInterleave-1)) ZetSetIRQLine(0, CPU_IRQSTATUS_AUTO);
		ZetClose();

		ZetOpen(1);
		nCyclesSegment = (nCyclesTotal[1] / nInterleave) * (i + 1);
		BurnTimerUpdate(nCyclesSegment);
		if ((i % (nInterleave/4)) == ((nInterleave / 4) - 1)) {
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
		ZetClose();
	}

	ZetOpen(1);
	BurnTimerEndFrame(nCyclesTotal[1]);

	if (pBurnSoundOut) {		
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
		sample_render(pBurnSoundOut, nBurnSoundLen);
	}
	ZetClose();

	if (pBurnDraw) {
		RrangerDraw();
	}

	return 0;
}

static INT32 Hardhea2Frame()
{
	if (DrvReset) {
		Hardhea2DoReset();
	}

	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 2);

		for (INT32 i = 0 ; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nCyclesSegment;
	INT32 nInterleave = 256;
	INT32 nCyclesTotal[3] = { 6000000 / 60, 6000000 / 60, 6000000 / 60 };
	INT32 nCyclesDone[3] = { 0, 0, 0 };

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesSegment = nCyclesTotal[0] / nInterleave;
		nCyclesDone[0] += ZetRun(nCyclesSegment);
		if (i == 112 && *nmi_enable) ZetNmi();
		if (i == 240) { vblank = 1; ZetSetIRQLine(0, CPU_IRQSTATUS_AUTO); }
		ZetClose();

		ZetOpen(1);
		nCyclesSegment = (nCyclesTotal[1] / nInterleave) * (i + 1);
		BurnTimerUpdateYM3812(nCyclesSegment);
		if ((i % (nInterleave/4)) == ((nInterleave / 4) - 1)) {
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
		ZetClose();

		ZetOpen(2);
		nCyclesSegment = nCyclesTotal[2] / nInterleave;
		nCyclesDone[2] += ZetRun(nCyclesSegment);
		ZetClose();
	}

	ZetOpen(1);
	BurnTimerEndFrameYM3812(nCyclesTotal[1]);
	ZetClose();

	if (pBurnSoundOut) {
		ZetOpen(1);	
		BurnYM3812Update(pBurnSoundOut, nBurnSoundLen);
		AY8910Render(&pAY8910Buffer[0], pBurnSoundOut, nBurnSoundLen, 1);
		ZetClose();
		ZetOpen(2);
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
		ZetClose();
	}

	if (pBurnDraw) {
		Hardhea2Draw();
	}

	return 0;
}


static INT32 SparkmanFrame() // & starfigh
{
	if (DrvReset) {
		HardheadDoReset();
	}

	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 3);

		for (INT32 i = 0 ; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
		DrvInputs[2] = (DrvInputs[2] & 0x03) | (DrvDips[2] & 0xbc);
	}

	INT32 nCyclesSegment;
	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { (Sparkman ? 6000000 : 9000000) / 60, 6000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesSegment = nCyclesTotal[0] / nInterleave;
		nCyclesDone[0] += ZetRun(nCyclesSegment);
		if (i == 112 && *nmi_enable) ZetNmi();
		if (i == (Sparkman ? 250 : 255)) { // any other value and the sprites glitch (MAME uses 240 here, hmmm)
			vblank = 1;
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
		ZetClose();

		ZetOpen(1);
		nCyclesSegment = (nCyclesTotal[1] / nInterleave) * (i + 1);
		BurnTimerUpdateYM3812(nCyclesSegment);
		if ((i % (nInterleave/4)) == ((nInterleave / 4) - 1)) {
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
		ZetClose();
	}

	ZetOpen(1);
	BurnTimerEndFrameYM3812(nCyclesTotal[1]);

	if (pBurnSoundOut) {		
		BurnYM3812Update(pBurnSoundOut, nBurnSoundLen);
		AY8910Render(&pAY8910Buffer[0], pBurnSoundOut, nBurnSoundLen, 1);
		sample_render(pBurnSoundOut, nBurnSoundLen);
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
	
	if (pnMin != NULL) {			// Return minimum compatible version
		*pnMin = 0x029672;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		ZetScan(nAction);			// Scan Z80

		if (Hardhead2) {
			DACScan(nAction, pnMin);
		}

		ZetOpen(1);
		if (strstr(BurnDrvGetTextA(DRV_NAME), "ranger")) {
			BurnYM2203Scan(nAction, pnMin);
		} else {
			AY8910Scan(nAction, pnMin);
			BurnYM3812Scan(nAction, pnMin);
		}
		ZetClose();

		// Scan critical driver variables
		SCAN_VAR(m_gfxbank);
		SCAN_VAR(m_palettebank);
		SCAN_VAR(m_spritebank);
		SCAN_VAR(m_spritebank_latch);
		SCAN_VAR(m_rombank_latch);
		SCAN_VAR(m_rambank);
		SCAN_VAR(disable_mainram_write);
		SCAN_VAR(protection_val);
		SCAN_VAR(hardhead_ip);
	}
	
	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		bankswitch(*mainbank);
		ZetMapMemory(DrvSprRAM + (m_spritebank * 0x2000), 0xe000, 0xffff, MAP_RAM);
		if (Sparkman) {
			if (disable_mainram_write) {
				ZetUnmapMemory(0xc800,0xdfff, MAP_WRITE);
			} else {
				ZetMapMemory(DrvZ80RAM0,0xc800,0xdfff, MAP_WRITE);
			}
		}
		if (Hardhead2) {
			ZetMapMemory(DrvZ80RAM0 + (m_rambank * 0x1800), 0xc800, 0xdfff, MAP_RAM);
		}
		ZetClose();
	}

	return 0;
}


// Hard Head

static struct BurnRomInfo hardheadRomDesc[] = {
	{ "p1",		0x8000, 0xc6147926, 1 }, //  0 maincpu
	{ "p2",		0x8000, 0xfaa2cf9a, 1 }, //  1
	{ "p3",		0x8000, 0x3d24755e, 1 }, //  2
	{ "p4",		0x8000, 0x0241ac79, 1 }, //  3
	{ "p7",		0x8000, 0xbeba8313, 1 }, //  4
	{ "p8",		0x8000, 0x211a9342, 1 }, //  5
	{ "p9",		0x8000, 0x2ad430c4, 1 }, //  6
	{ "p10",	0x8000, 0xb6894517, 1 }, //  7

	{ "p13",	0x8000, 0x493c0b41, 2 }, //  8 audiocpu

	{ "p5",		0x8000, 0xe9aa6fba, 3 }, //  9 gfx1
	{ "p6",		0x8000, 0x15d5f5dd, 3 }, // 10
	{ "p11",	0x8000, 0x055f4c29, 3 }, // 11
	{ "p12",	0x8000, 0x9582e6db, 3 }, // 12

	{ "p14",	0x8000, 0x41314ac1, 4 }, // 13 samples
};

STD_ROM_PICK(hardhead)
STD_ROM_FN(hardhead)

struct BurnDriver BurnDrvHardhead = {
	"hardhead", NULL, NULL, NULL, "1988",
	"Hard Head\0", NULL, "SunA", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, hardheadRomInfo, hardheadRomName, NULL, NULL, DrvInputInfo, HardheadDIPInfo,
	HardheadInit, HardheadExit, HardheadFrame, HardheadDraw, DrvScan, &DrvRecalc, 0x100,
	256, 224, 4, 3
};


// Super Ranger (v2.0)

static struct BurnRomInfo srangerRomDesc[] = {
	{ "1.e2",	0x08000, 0x4eef1ede, 1 }, //  0 maincpu
	{ "2.f2",	0x08000, 0xff65af29, 1 }, //  1
	{ "3.h2",	0x08000, 0x64e09436, 1 }, //  2
	{ "4.i2",	0x10000, 0x4346fae6, 1 }, //  3
	{ "5.j2",	0x10000, 0x6a7ca1c3, 1 }, //  4

	{ "14.j13",	0x08000, 0x11c83aa1, 2 }, //  5 audiocpu

	{ "6.p5",	0x08000, 0x4f11fef3, 3 }, //  6 gfx1
	{ "7.p6",	0x08000, 0x9f35dbfa, 3 }, //  7
	{ "8.p7",	0x08000, 0xf400db89, 3 }, //  8
	{ "9.p8",	0x08000, 0xfa2a11ea, 3 }, //  9
	{ "10.p9",	0x08000, 0x1b204d6b, 3 }, // 10
	{ "11.p10",	0x08000, 0x19037a7b, 3 }, // 11
	{ "12.p11",	0x08000, 0xc59c0ec7, 3 }, // 12
	{ "13.p12",	0x08000, 0x9809fee8, 3 }, // 13

	{ "15.e13",	0x08000, 0x28c2c87e, 4 }, // 14 samples
};

STD_ROM_PICK(sranger)
STD_ROM_FN(sranger)

struct BurnDriver BurnDrvSranger = {
	"sranger", NULL, NULL, NULL, "1988",
	"Super Ranger (v2.0)\0", NULL, "SunA", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, srangerRomInfo, srangerRomName, NULL, NULL, DrvInputInfo, RrangerDIPInfo,
	RrangerInit, RrangerExit, RrangerFrame, RrangerDraw, DrvScan, &DrvRecalc, 0x100,
	256, 224, 4, 3
};


// Hard Head 2 (v2.0)

static struct BurnRomInfo hardhea2RomDesc[] = {
	{ "hrd-hd9",	0x08000, 0x69c4c307, 1 }, //  0 maincpu
	{ "hrd-hd10",	0x10000, 0x77ec5b0a, 1 }, //  1
	{ "hrd-hd11",	0x10000, 0x12af8f8e, 1 }, //  2
	{ "hrd-hd12",	0x10000, 0x35d13212, 1 }, //  3
	{ "hrd-hd13",	0x10000, 0x3225e7d7, 1 }, //  4

	{ "hrd-hd14",	0x08000, 0x79a3be51, 2 }, //  5 audiocpu

	{ "hrd-hd15",	0x10000, 0xbcbd88c3, 3 }, //  6 pcm

	{ "hrd-hd1",	0x10000, 0x7e7b7a58, 4 }, //  7 gfx1
	{ "hrd-hd2",	0x10000, 0x303ec802, 4 }, //  8
	{ "hrd-hd3",	0x10000, 0x3353b2c7, 4 }, //  9
	{ "hrd-hd4",	0x10000, 0xdbc1f9c1, 4 }, // 10
	{ "hrd-hd5",	0x10000, 0xf738c0af, 4 }, // 11
	{ "hrd-hd6",	0x10000, 0xbf90d3ca, 4 }, // 12
	{ "hrd-hd7",	0x10000, 0x992ce8cb, 4 }, // 13
	{ "hrd-hd8",	0x10000, 0x359597a4, 4 }, // 14
};

STD_ROM_PICK(hardhea2)
STD_ROM_FN(hardhea2)

struct BurnDriver BurnDrvHardhea2 = {
	"hardhea2", NULL, NULL, NULL, "1991",
	"Hard Head 2 (v2.0)\0", NULL, "SunA", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, hardhea2RomInfo, hardhea2RomName, NULL, NULL, DrvInputInfo, Hardhea2DIPInfo,
	Hardhea2Init, Hardhea2Exit, Hardhea2Frame, Hardhea2Draw, DrvScan, &DrvRecalc, 0x100,
	256, 224, 4, 3
};


// Spark Man (v2.0, set 1)

static struct BurnRomInfo sparkmanRomDesc[] = {
	{ "sparkman.e7",	0x08000, 0xd89c5780, 1 }, //  0 maincpu
	{ "10.g7",		0x10000, 0x48b4a31e, 1 }, //  1
	{ "12.g8",		0x10000, 0xb8a4a557, 1 }, //  2
	{ "11.i7",		0x10000, 0xf5f38e1f, 1 }, //  3
	{ "13.i8",		0x10000, 0xe54eea25, 1 }, //  4

	{ "14.h11",		0x08000, 0x06822f3d, 2 }, //  5 audiocpu

	{ "p3.u1",		0x10000, 0x39dbd414, 3 }, //  6 gfx1
	{ "p2.t1",		0x10000, 0x2e474203, 3 }, //  7
	{ "p1.r1",		0x08000, 0x7115cfe7, 3 }, //  8
	{ "p6.u2",		0x10000, 0xe6551db9, 3 }, //  9
	{ "p5.t2",		0x10000, 0x0df5da2a, 3 }, // 10
	{ "p4.r2",		0x08000, 0x6904bde2, 3 }, // 11

	{ "p7.u4",		0x10000, 0x17c16ce4, 4 }, // 12 gfx2
	{ "p8.u6",		0x10000, 0x414222ea, 4 }, // 13

	{ "15.b10",		0x08000, 0x46c7d4d8, 5 }, // 14 samples
	{ "16.b11",		0x08000, 0xd6823a62, 5 }, // 15
};

STD_ROM_PICK(sparkman)
STD_ROM_FN(sparkman)

struct BurnDriver BurnDrvSparkman = {
	"sparkman", NULL, NULL, NULL, "1989",
	"Spark Man (v2.0, set 1)\0", NULL, "SunA", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, sparkmanRomInfo, sparkmanRomName, NULL, NULL, SparkmanInputInfo, SparkmanDIPInfo,
	SparkmanInit, HardheadExit, SparkmanFrame, SparkmanDraw, DrvScan, &DrvRecalc, 0x100,
	256, 224, 4, 3
};


// Star Fighter (v1)

static struct BurnRomInfo starfighRomDesc[] = {
	{ "starfgtr.l1",	0x08000, 0xf93802c6, 1 }, //  0 maincpu
	{ "starfgtr.j1",	0x10000, 0xfcfcf08a, 1 }, //  1
	{ "starfgtr.i1",	0x10000, 0x6935fcdb, 1 }, //  2
	{ "starfgtr.l3",	0x10000, 0x50c072a4, 1 }, //  3
	{ "starfgtr.j3",	0x10000, 0x3fe3c714, 1 }, //  4

	{ "starfgtr.m8",	0x08000, 0xae3b0691, 2 }, //  5 audiocpu

	{ "starfgtr.e4",	0x10000, 0x54c0ca3d, 3 }, //  6 gfx1
	{ "starfgtr.d4",	0x10000, 0x4313ba40, 3 }, //  7
	{ "starfgtr.b4",	0x10000, 0xad8d0f21, 3 }, //  8
	{ "starfgtr.a4",	0x10000, 0x6d8f74c8, 3 }, //  9
	{ "starfgtr.e6",	0x10000, 0xceff00ff, 3 }, // 10
	{ "starfgtr.d6",	0x10000, 0x7aaa358a, 3 }, // 11
	{ "starfgtr.b6",	0x10000, 0x47d6049c, 3 }, // 12
	{ "starfgtr.a6",	0x10000, 0x4a33f6f3, 3 }, // 13

	{ "starfgtr.q10",	0x08000, 0xfa510e94, 4 }, // 14 samples
};

STD_ROM_PICK(starfigh)
STD_ROM_FN(starfigh)

struct BurnDriver BurnDrvStarfigh = {
	"starfigh", NULL, NULL, NULL, "1990",
	"Star Fighter (v1)\0", NULL, "SunA", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, starfighRomInfo, starfighRomName, NULL, NULL, DrvInputInfo, StarfighDIPInfo,
	StarfighInit, HardheadExit, SparkmanFrame, StarfighDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};
