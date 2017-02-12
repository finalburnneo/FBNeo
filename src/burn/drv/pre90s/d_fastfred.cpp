// Fast Freddy / Jump Coaster HW emu-layer for FB Alpha, Based on the MAME driver by Zsolt Vasvari
#include "tiles_generic.h"
#include "z80_intf.h"
#include "driver.h"
#include "flt_rc.h"

extern "C" {
    #include "ay8910.h"
}

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;

static UINT8 *DrvMainRAM;
static UINT8 *DrvSubRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvFGVidRAM;
static UINT8 *DrvAttrRAM;

static UINT8 *Rom0, *Rom1;
static UINT8 *Gfx0, *Gfx1, *Gfx2, *Gfx3, *GfxImagoSprites, *Prom;

static UINT8 DrvJoy1[8], DrvJoy2[8], DrvDips[2], DrvInput[2], DrvReset;

static INT16 *pAY8910Buffer[6];

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 flyboymode = 0;
static UINT8 boggy84mode = 0;
static UINT8 boggy84bmode = 0;
static UINT8 imagomode = 0;

static INT32 fastfred_hardware_type = 0;

static INT32 fastfred_background_color = 0;
static INT32 fastfred_cpu0_interrupt_enable = 0;
static INT32 fastfred_cpu1_interrupt_enable = 0;
static INT32 fastfred_colorbank = 0;
static INT32 fastfred_charbank = 0;
static INT32 fastfred_flipscreenx = 0;
static INT32 fastfred_flipscreeny = 0;
static INT32 fastfred_soundlatch = 0;
static INT32 fastfred_scroll[32];
static INT32 fastfred_color_select[32];

static UINT8 imago_sprites[0x800*3];
static UINT16 imago_sprites_address;
static UINT8 imago_sprites_bank;

static struct BurnInputInfo CommonInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"},
	{"P1 Start",	BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 1"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"},
	{"P2 Start",	BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"},
	{"P2 Right",	BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"},
	{"P2 Button 1",	BIT_DIGITAL,	DrvJoy1 + 7,	"p2 fire 1"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	    "reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
};

STDINPUTINFO(Common)

static struct BurnInputInfo ImagoInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 coin"},
	{"P1 Start",	BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 1"},

	{"P2 Start",	BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	    "reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
};

STDINPUTINFO(Imago)

static struct BurnInputInfo TwoBtnInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"},
	{"P1 Start",	BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 1"},
	{"P1 Button 2",	BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"},
	{"P2 Start",	BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	    "reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
};

STDINPUTINFO(TwoBtn)

static struct BurnDIPInfo FastfredDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x20, NULL		},

	{0   , 0xfe, 0   ,    16, "Coinage"		},
	{0x0f, 0x01, 0x0f, 0x01, "A 2/1 B 2/1"		},
	{0x0f, 0x01, 0x0f, 0x02, "A 2/1 B 1/3"		},
	{0x0f, 0x01, 0x0f, 0x00, "A 1/1 B 1/1"		},
	{0x0f, 0x01, 0x0f, 0x03, "A 1/1 B 1/2"		},
	{0x0f, 0x01, 0x0f, 0x04, "A 1/1 B 1/3"		},
	{0x0f, 0x01, 0x0f, 0x05, "A 1/1 B 1/4"		},
	{0x0f, 0x01, 0x0f, 0x06, "A 1/1 B 1/5"		},
	{0x0f, 0x01, 0x0f, 0x07, "A 1/1 B 1/6"		},
	{0x0f, 0x01, 0x0f, 0x08, "A 1/2 B 1/2"		},
	{0x0f, 0x01, 0x0f, 0x09, "A 1/2 B 1/4"		},
	{0x0f, 0x01, 0x0f, 0x0a, "A 1/2 B 1/5"		},
	{0x0f, 0x01, 0x0f, 0x0e, "A 1/2 B 1/6"		},
	{0x0f, 0x01, 0x0f, 0x0b, "A 1/2 B 1/10"		},
	{0x0f, 0x01, 0x0f, 0x0c, "A 1/2 B 1/11"		},
	{0x0f, 0x01, 0x0f, 0x0d, "A 1/2 B 1/12"		},
	{0x0f, 0x01, 0x0f, 0x0f, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x0f, 0x01, 0x10, 0x00, "3"		},
	{0x0f, 0x01, 0x10, 0x10, "5"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x0f, 0x01, 0x60, 0x20, "30000"		},
	{0x0f, 0x01, 0x60, 0x40, "50000"		},
	{0x0f, 0x01, 0x60, 0x60, "100000"		},
	{0x0f, 0x01, 0x60, 0x00, "None"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0f, 0x01, 0x80, 0x00, "Upright"		},
	{0x0f, 0x01, 0x80, 0x80, "Cocktail"		},
};

STDDIPINFO(Fastfred)

static struct BurnDIPInfo FlyboyDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x00, NULL		},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x0f, 0x01, 0x03, 0x03, "6 Coins 1 Credits"		},
	{0x0f, 0x01, 0x03, 0x02, "2 Coins 1 Credits"		},
	{0x0f, 0x01, 0x03, 0x00, "1 Coin  1 Credits"		},
	{0x0f, 0x01, 0x03, 0x01, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x0f, 0x01, 0x0c, 0x0c, "6 Coins 1 Credits"		},
	{0x0f, 0x01, 0x0c, 0x08, "2 Coins 1 Credits"		},
	{0x0f, 0x01, 0x0c, 0x00, "1 Coin  1 Credits"		},
	{0x0f, 0x01, 0x0c, 0x04, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0f, 0x01, 0x30, 0x00, "3"		},
	{0x0f, 0x01, 0x30, 0x10, "5"		},
	{0x0f, 0x01, 0x30, 0x20, "7"		},
	{0x0f, 0x01, 0x30, 0x30, "255 (Cheat)"		},

	{0   , 0xfe, 0   ,    2, "Invulnerability (Cheat)"		},
	{0x0f, 0x01, 0x40, 0x00, "Off"		},
	{0x0f, 0x01, 0x40, 0x40, "On"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0f, 0x01, 0x80, 0x00, "Upright"		},
	{0x0f, 0x01, 0x80, 0x80, "Cocktail"		},
};

STDDIPINFO(Flyboy)

static struct BurnDIPInfo JumpcoasDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x00, NULL		},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x0f, 0x01, 0x03, 0x03, "6 Coins 1 Credits"		},
	{0x0f, 0x01, 0x03, 0x02, "2 Coins 1 Credits"		},
	{0x0f, 0x01, 0x03, 0x00, "1 Coin  1 Credits"		},
	{0x0f, 0x01, 0x03, 0x01, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x0f, 0x01, 0x0c, 0x0c, "6 Coins 1 Credits"		},
	{0x0f, 0x01, 0x0c, 0x08, "2 Coins 1 Credits"		},
	{0x0f, 0x01, 0x0c, 0x00, "1 Coin  1 Credits"		},
	{0x0f, 0x01, 0x0c, 0x04, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0f, 0x01, 0x30, 0x00, "3"		},
	{0x0f, 0x01, 0x30, 0x10, "5"		},
	{0x0f, 0x01, 0x30, 0x20, "7"		},
	{0x0f, 0x01, 0x30, 0x30, "255 (Cheat)"		},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x0f, 0x01, 0x40, 0x00, "Off"		},
	{0x0f, 0x01, 0x40, 0x40, "On"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0f, 0x01, 0x80, 0x00, "Upright"		},
	{0x0f, 0x01, 0x80, 0x80, "Cocktail"		},
};

STDDIPINFO(Jumpcoas)

static struct BurnDIPInfo Boggy84DIPList[]=
{
	{0x0b, 0xff, 0xff, 0x00, NULL		},
	{0x0c, 0xff, 0xff, 0xff, NULL		},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x0b, 0x01, 0x03, 0x03, "6 Coins 1 Credits"		},
	{0x0b, 0x01, 0x03, 0x02, "2 Coins 1 Credits"		},
	{0x0b, 0x01, 0x03, 0x00, "1 Coin  1 Credits"		},
	{0x0b, 0x01, 0x03, 0x01, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x0b, 0x01, 0x0c, 0x0c, "6 Coins 1 Credits"		},
	{0x0b, 0x01, 0x0c, 0x08, "2 Coins 1 Credits"		},
	{0x0b, 0x01, 0x0c, 0x00, "1 Coin  1 Credits"		},
	{0x0b, 0x01, 0x0c, 0x04, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0b, 0x01, 0x30, 0x00, "3"		},
	{0x0b, 0x01, 0x30, 0x10, "5"		},
	{0x0b, 0x01, 0x30, 0x20, "7"		},
	{0x0b, 0x01, 0x30, 0x30, "255 (Cheat)"		},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x0b, 0x01, 0x40, 0x00, "Off"		},
	{0x0b, 0x01, 0x40, 0x40, "On"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0b, 0x01, 0x80, 0x00, "Upright"		},
	{0x0b, 0x01, 0x80, 0x80, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x0c, 0x01, 0x01, 0x01, "Off"		},
	{0x0c, 0x01, 0x01, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x0c, 0x01, 0x02, 0x02, "Off"		},
	{0x0c, 0x01, 0x02, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x0c, 0x01, 0x04, 0x04, "Off"		},
	{0x0c, 0x01, 0x04, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x0c, 0x01, 0x08, 0x08, "Off"		},
	{0x0c, 0x01, 0x08, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x0c, 0x01, 0x10, 0x10, "Off"		},
	{0x0c, 0x01, 0x10, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x0c, 0x01, 0x20, 0x20, "Off"		},
	{0x0c, 0x01, 0x20, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x0c, 0x01, 0x40, 0x40, "Off"		},
	{0x0c, 0x01, 0x40, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x0c, 0x01, 0x80, 0x80, "Off"		},
	{0x0c, 0x01, 0x80, 0x00, "On"		},
};

STDDIPINFO(Boggy84)

static struct BurnDIPInfo RedrobinDIPList[]=
{
	{0x0b, 0xff, 0xff, 0x10, NULL		},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x0b, 0x01, 0x03, 0x03, "4 Coins 1 Credits"		},
	{0x0b, 0x01, 0x03, 0x02, "3 Coins 1 Credits"		},
	{0x0b, 0x01, 0x03, 0x01, "2 Coins 1 Credits"		},
	{0x0b, 0x01, 0x03, 0x00, "1 Coin  1 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x0b, 0x01, 0x0c, 0x0c, "4 Coins 1 Credits"		},
	{0x0b, 0x01, 0x0c, 0x08, "3 Coins 1 Credits"		},
	{0x0b, 0x01, 0x0c, 0x04, "2 Coins 1 Credits"		},
	{0x0b, 0x01, 0x0c, 0x00, "1 Coin  1 Credits"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0b, 0x01, 0x30, 0x00, "2"		},
	{0x0b, 0x01, 0x30, 0x10, "3"		},
	{0x0b, 0x01, 0x30, 0x20, "4"		},
	{0x0b, 0x01, 0x30, 0x30, "5"		},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x0b, 0x01, 0x40, 0x00, "30000"		},
	{0x0b, 0x01, 0x40, 0x40, "50000"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x0b, 0x01, 0x80, 0x00, "Off"		},
	{0x0b, 0x01, 0x80, 0x80, "On"		},
};

STDDIPINFO(Redrobin)

static struct BurnDIPInfo ImagoDIPList[]=
{
	{0x09, 0xff, 0xff, 0x01, NULL		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x09, 0x01, 0x03, 0x00, "2"		},
	{0x09, 0x01, 0x03, 0x01, "3"		},
	{0x09, 0x01, 0x03, 0x02, "4"		},
	{0x09, 0x01, 0x03, 0x03, "5"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x09, 0x01, 0x04, 0x04, "Off"		},
	{0x09, 0x01, 0x04, 0x00, "On"		},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x09, 0x01, 0x38, 0x38, "5 Coins 1 Credits"		},
	{0x09, 0x01, 0x38, 0x18, "4 Coins 1 Credits"		},
	{0x09, 0x01, 0x38, 0x28, "3 Coins 1 Credits"		},
	{0x09, 0x01, 0x38, 0x08, "2 Coins 1 Credits"		},
	{0x09, 0x01, 0x38, 0x00, "1 Coin  1 Credits"		},
	{0x09, 0x01, 0x38, 0x20, "1 Coin  2 Credits"		},
	{0x09, 0x01, 0x38, 0x10, "1 Coin  3 Credits"		},
	{0x09, 0x01, 0x38, 0x30, "1 Coin  4 Credits"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x09, 0x01, 0x40, 0x40, "Off"		},
	{0x09, 0x01, 0x40, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x09, 0x01, 0x80, 0x80, "Off"		},
	{0x09, 0x01, 0x80, 0x00, "On"		},
};

STDDIPINFO(Imago)

static UINT8 flyboy_custom1_io_r()
{
	switch (ZetGetPC(-1))
	{
		case 0x049d: return 0xad;	// compare
		case 0x04b9:			// compare with 0x9e ??? When ???
		case 0x0563: return 0x03;	// $c085 compare - starts game
		case 0x069b: return 0x69;	// $c086 compare
		case 0x076b: return 0xbb;	// $c087 compare
		case 0x0852: return 0xd9;	// $c096 compare
		case 0x09d5: return 0xa4;	// $c099 compare
		case 0x0a83: return 0xa4;	// $c099 compare
		case 0x1028:			// $c08a  bit 0  compare
		case 0x1051:			// $c08a  bit 3  compare
		case 0x107d:			// $c08c  bit 5  compare
		case 0x10a7:			// $c08e  bit 1  compare
		case 0x10d0:			// $c08d  bit 2  compare
		case 0x10f6:			// $c090  bit 0  compare
		case 0x3fb6:			// lddr

		return 0x00;
	}

	return 0x00;
}

static UINT8 flyboy_custom2_io_r()
{
	switch (ZetGetPC(-1))
	{
		case 0x0395: return 0xf7;	// $C900 compare
		case 0x03f5:			// $c8fd
		case 0x043d:			// $c8fd
		case 0x0471:			// $c900
		case 0x1031: return 0x01;	// $c8fe  bit 0  compare
		case 0x1068: return 0x04;	// $c8fe  bit 2  compare
		case 0x1093: return 0x20;	// $c8fe  bit 5  compare
		case 0x10bd: return 0x80;	// $c8fb  bit 7  compare
		case 0x103f:			// $c8fe
		case 0x10e4:			// $c900
		case 0x110a:			// $c900
		case 0x3fc8:			// ld a with c8fc-c900

		return 0x00;
	}

	return 0x00;
}

static UINT8 fastfred_custom_io_r(INT32 offset)
{
	if (~fastfred_hardware_type & 1) {
		if (offset == 0x100) {
			if (boggy84bmode) return 0x63;
			return (boggy84mode) ? 0x6a : 0x63;
		} else {
			return 0;
		}
	}

	switch (ZetGetPC(-1))
	{
		case 0x03c0: return 0x9d;
		case 0x03e6: return 0x9f;
		case 0x0407: return 0x00;
		case 0x0446: return 0x94;
		case 0x049f: return 0x01;
		case 0x04b1: return 0x00;
		case 0x0dd2: return 0x00;
		case 0x0de4: return 0x20;
		case 0x122b: return 0x10;
		case 0x123d: return 0x00;
		case 0x1a83: return 0x10;
		case 0x1a93: return 0x00;
		case 0x1b26: return 0x00;
		case 0x1b37: return 0x80;
		case 0x2491: return 0x10;
		case 0x24a2: return 0x00;
		case 0x46ce: return 0x20;
		case 0x46df: return 0x00;
		case 0x7b18: return 0x01;
		case 0x7b29: return 0x00;
		case 0x7b47: return 0x00;
		case 0x7b58: return 0x20;
	}

	return 0x00;
}

static INT32 ImagoSpritesDecode() // for sprite dma in 0_w
{
	INT32 spriteplanes[3] = { 0x800*8*2, 0x800*8*1, 0x800*8*0 };
	INT32 spritexoffs[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 8*8+0, 8*8+1, 8*8+2, 8*8+3, 8*8+4, 8*8+5, 8*8+6, 8*8+7 };
	INT32 spriteyoffs[16] = { 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8, 16*8, 17*8, 18*8, 19*8, 20*8, 21*8, 22*8, 23*8 };

	GfxDecode(0x40, 3,  16,  16, spriteplanes, spritexoffs, spriteyoffs, 0x100, &imago_sprites[0], GfxImagoSprites);

	return 0;
}


static void __fastcall fastfred_cpu0_write(UINT16 address, UINT8 data)
{
	if (fastfred_hardware_type & 1) // fastfred, flyboy,
	{
		if (address >= 0xd800 && address < 0xd840) {
			if (address & 1) {
				fastfred_color_select[(address >> 1) & 0x1f] = data & 7;
			} else {
				fastfred_scroll[(address >> 1) & 0x1f] = data;
			}
		}

		if (address >= 0xd800 && address <= 0xdbff) {
			DrvAttrRAM[address & 0x3ff] = data;
			return;
		}
	} else { // jumpcoas = 0, boggy84 = 2
		if (address >= 0xd000 && address < 0xd040) {
			if (address & 1) {
				fastfred_color_select[(address >> 1) & 0x1f] = data & 7;
			} else {
				fastfred_scroll[(address >> 1) & 0x1f] = data;
			}
		}

		if (address >= 0xd000 && address <= 0xd3ff) {
			DrvAttrRAM[address & 0x3ff] = data;
			return;
		}
	}

	if (imagomode && address >= 0xb800 && address <= 0xbfff) {
		UINT8 *rom = Gfx1;
		UINT8 sprites_data;
		UINT32 offset = address - 0xb800;

		sprites_data = rom[imago_sprites_address + 0x2000*0 + imago_sprites_bank * 0x1000];
		imago_sprites[offset + 0x800*0] = sprites_data;

		sprites_data = rom[imago_sprites_address + 0x2000*1 + imago_sprites_bank * 0x1000];
		imago_sprites[offset + 0x800*1] = sprites_data;

		sprites_data = rom[imago_sprites_address + 0x2000*2 + imago_sprites_bank * 0x1000];
		imago_sprites[offset + 0x800*2] = sprites_data;

		if ((offset & 0xf) == 0xf && (offset & 0xff) >= 0x7f)
		{ // end-of-dma always ends on a 0x007f or greater offset (if we decode every time, it causes some lag)
			ImagoSpritesDecode(); // decode the dma'd sprites.
		}
		return;
	}

	switch (address)
	{
		case 0xe000:
			fastfred_background_color = data;
		return;

		case 0xf001:
			fastfred_cpu0_interrupt_enable = data & 1;
		return;

		case 0xf002:
			fastfred_colorbank = (fastfred_colorbank & 0x10) | ((data & 1) << 3);
		return;

		case 0xf003:
			fastfred_colorbank = (fastfred_colorbank & 0x08) | ((data & 1) << 4);
		return;

		case 0xf004:
			if (imagomode) {
				ZetSetIRQLine(0, (data & 1) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
			} else {
				fastfred_charbank = (fastfred_charbank & 0x0200) | ((data & 1) << 8);
			}
		return;

		case 0xf005:
			if (imagomode) {
				fastfred_charbank = data;
			} else {
				fastfred_charbank = (fastfred_charbank & 0x0100) | ((data & 1) << 9);
			}
		return;

		case 0xf006:
		case 0xf116:
			fastfred_flipscreenx = data & 1;
		return;

		case 0xf007:
		case 0xf117:
			fastfred_flipscreeny = data & 1;
		return;

		case 0xf401:
			imago_sprites_bank = (data & 2) >> 1;
		return;

		case 0xf800:
			if (fastfred_hardware_type & 1) {
				fastfred_soundlatch = data;
			} else {
				AY8910Write(0, 0, data);
			}
		return;

		case 0xf801:
			if (~fastfred_hardware_type & 1) {
				AY8910Write(0, 1, data);
			}
		return;
	}
}

static UINT8 __fastcall fastfred_cpu0_read(UINT16 address)
{

	if (imagomode && address >= 0x1000 && address <= 0x1fff) {
		imago_sprites_address = address & 0xfff;
		return 0xff;
	}

	switch (address)
	{
		case 0xe000: // buttons
		case 0xe802:
			return DrvInput[0];

		case 0xe800: {
			return ((fastfred_hardware_type == 0) || boggy84mode) ? DrvDips[0] : DrvInput[1];
		}

		case 0xf000: // dip 1
		    return DrvDips[0];

		case 0xe801: // dip 2
		    return DrvDips[1];

		case 0xe803: // joy inputs
    		return DrvInput[1];

		case 0xf800: // watchdog
			return 0;
	}

	if (flyboymode) {
		if (address >= 0xc085 && address <= 0xc099) {
			return flyboy_custom1_io_r();
		}

		if (address >= 0xc8fb && address <= 0xc900) {
			return flyboy_custom2_io_r();
		}
	}

	if (address >= 0xc800 && address <= 0xcfff) {
		return fastfred_custom_io_r(address & 0x7ff);
	}

	return 0;
}

static void __fastcall fastfred_cpu1_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x3000:
			fastfred_cpu1_interrupt_enable = data & 1;
		return;

		case 0x4000:
			AY8910Reset(0);
			AY8910Reset(1);
		return;

		case 0x5000:
		case 0x5001:
		case 0x6000:
		case 0x6001:
			AY8910Write((address >> 13) & 1, address & 1, data);
		return;

		case 0x7000:
		return;
	}
}

static UINT8 __fastcall fastfred_cpu1_read(UINT16 address)
{
	switch (address)
	{
		case 0x3000:
			return fastfred_soundlatch;
	}

	return 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	fastfred_background_color = 0;
	fastfred_cpu0_interrupt_enable = 0;
	fastfred_cpu1_interrupt_enable = 0;
	fastfred_colorbank = 0;
	fastfred_charbank = 0;
	fastfred_flipscreenx = 0;
	fastfred_flipscreeny = 0;
	fastfred_soundlatch = 0;
	memset(fastfred_scroll, 0, sizeof(fastfred_scroll));
	memset(fastfred_color_select, 0, sizeof(fastfred_color_select));

	memset(&imago_sprites, 0, sizeof(imago_sprites));
	imago_sprites_address = 0;
	imago_sprites_bank = 0;

	DrvReset = 0;

	for (INT32 i = 0; i < 2; i++) {
		ZetOpen(i);
		ZetReset();
		ZetClose();

		AY8910Reset(i);
	}

	return 0;
}

static INT32 DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x100; i++)
	{
		INT32 bit0, bit1, bit2, bit3;
		INT32 r, g, b;

		bit0 = (Prom[i + 0x000] >> 0) & 0x01;
		bit1 = (Prom[i + 0x000] >> 1) & 0x01;
		bit2 = (Prom[i + 0x000] >> 2) & 0x01;
		bit3 = (Prom[i + 0x000] >> 3) & 0x01;
		r = bit0 * 14 + bit1 * 31 + bit2 * 66 + bit3 * 144;

		bit0 = (Prom[i + 0x100] >> 0) & 0x01;
		bit1 = (Prom[i + 0x100] >> 1) & 0x01;
		bit2 = (Prom[i + 0x100] >> 2) & 0x01;
		bit3 = (Prom[i + 0x100] >> 3) & 0x01;
		g = bit0 * 14 + bit1 * 31 + bit2 * 66 + bit3 * 144;

		bit0 = (Prom[i + 0x200] >> 0) & 0x01;
		bit1 = (Prom[i + 0x200] >> 1) & 0x01;
		bit2 = (Prom[i + 0x200] >> 2) & 0x01;
		bit3 = (Prom[i + 0x200] >> 3) & 0x01;
		b = bit0 * 14 + bit1 * 31 + bit2 * 66 + bit3 * 144;

		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}

	if (imagomode) {
		DrvPalette[0x100+0x40+0] = BurnHighCol(0x15, 0x00, 0x00, 0);
		DrvPalette[0x100+0x40+1] = BurnHighCol(0x00, 0x00, 0x00, 0);
	}

	return 0;
}

static INT32 GraphicsDecode()
{
	static INT32 TilePlanes[3] = { 0x20000, 0x10000, 0x00000 };
	static INT32 SpriPlanes[3] = { 0x10000, 0x08000, 0x00000 };
	static INT32 SharXOffs[16] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
				     0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47 };
	static INT32 SharYOffs[16] = { 0x00, 0x08, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38,
				     0x80, 0x88, 0x90, 0x98, 0xa0, 0xa8, 0xb0, 0xb8 };

	UINT8 *tmp = (UINT8*)malloc(0x6000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, Gfx0, 0x06000);
	GfxDecode(0x400, 3,  8,  8, TilePlanes, SharXOffs, SharYOffs, 0x040, tmp, Gfx0);

	memcpy (tmp, Gfx1, 0x03000);
	GfxDecode(0x080, 3, 16, 16, SpriPlanes, SharXOffs, SharYOffs, 0x100, tmp, Gfx1);

	free (tmp);

	return 0;
}

static INT32 ImagoGraphicsDecode()
{
	INT32 charplanes[3] = { RGN_FRAC(0x3000, 2,3), RGN_FRAC(0x3000, 1,3), RGN_FRAC(0x3000, 0,3) };
	INT32 charxoffs[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
	INT32 charyoffs[8] = { 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 };

	INT32 imago1bppplanes[1] = { 0 };
	INT32 imago1bppxoffs[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
	INT32 imago1bppyoffs[8] = { 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 };

	UINT8 *tmp = (UINT8*)malloc(0x6000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, Gfx0, 0x03000);
	GfxDecode(0x200, 3,  8,  8, charplanes, charxoffs, charyoffs, 0x040, tmp, Gfx0);

	memcpy (tmp, Gfx2, 0x03000);
	GfxDecode(0x200, 3,  8,  8, charplanes, charxoffs, charyoffs, 0x040, tmp, Gfx2);

	memcpy (tmp, Gfx3, 0x01000);
	GfxDecode(0x200, 1,  8,  8, imago1bppplanes, imago1bppxoffs, imago1bppyoffs, 0x040, tmp, Gfx3);

	free (tmp);

	return 0;
}

static INT32 DrvLoadRoms()
{
	char* pRomName;
	struct BurnRomInfo ri;
	INT32 gfx1_loaded = 0;
	UINT8 *Rom0Load = Rom0;
	UINT8 *Rom1Load = Rom1;
	UINT8 *Gfx0Load = Gfx0;
	UINT8 *Gfx1Load = Gfx1;
	UINT8 *Gfx2Load = Gfx2;
	UINT8 *Gfx3Load = Gfx3;
	UINT8 *PromLoad = Prom;

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {

		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & 7) == 1) {
			if (BurnLoadRom(Rom0Load, i, 1)) return 1;
			Rom0Load += ri.nLen;

			continue;
		}

		if ((ri.nType & 7) == 2) {
			if (BurnLoadRom(Rom1Load, i, 1)) return 1;
			Rom1Load += ri.nLen;

			continue;
		}

		if ((ri.nType & 7) == 3) {
			if (BurnLoadRom(Gfx0Load, i, 1)) return 1;
			Gfx0Load += ri.nLen;
			gfx1_loaded = 1;

			continue;
		}

		if ((ri.nType & 7) == 4) {
			if (BurnLoadRom(Gfx1Load, i, 1)) return 1;
			Gfx1Load += ri.nLen;

			continue;
		}

		if ((ri.nType & 7) == 5) {
			if (BurnLoadRom(PromLoad, i, 1)) return 1;
			PromLoad += ri.nLen;

			continue;
		}

		if ((ri.nType & 7) == 6) {
			if (BurnLoadRom(Gfx2Load, i, 1)) return 1;
			Gfx2Load += ri.nLen;

			continue;
		}

		if ((ri.nType & 7) == 7) {
			if (BurnLoadRom(Gfx3Load, i, 1)) return 1;
			Gfx3Load += ri.nLen;

			continue;
		}
	}

	if (!gfx1_loaded) {
		memcpy (Gfx0 + 0x0000, Gfx1 + 0x0000, 0x1000);
		memcpy (Gfx0 + 0x2000, Gfx1 + 0x1000, 0x1000);
		memcpy (Gfx0 + 0x4000, Gfx1 + 0x2000, 0x1000);
	}

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Rom0           = Next; Next += 0x10000;
	Rom1           = Next; Next += 0x10000;

	Gfx0           = Next; Next += 0x20000;
	Gfx1           = Next; Next += 0x20000;
	Gfx2           = Next; Next += 0x20000;
	Gfx3           = Next; Next += 0x20000;
	GfxImagoSprites= Next; Next += 0x20000;

	Prom           = Next; Next += 0x00300;

	DrvPalette     = (UINT32*)Next; Next += 0x00200 * sizeof(UINT32);

	AllRam			= Next;

	DrvMainRAM      = Next; Next += 0x10000;
	DrvVidRAM       = Next; Next += 0x10000;
	DrvFGVidRAM     = Next; Next += 0x10000;
	DrvAttrRAM      = Next; Next += 0x10000;
	DrvSubRAM       = Next; Next += 0x10000;

	RamEnd			= Next;

	pAY8910Buffer[0]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[1]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[2]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[3]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[4]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[5]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);

	MemEnd         = Next;

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
		if (DrvLoadRoms()) return 1;
		if (DrvPaletteInit()) return 1;
		if (fastfred_hardware_type == 3) {
			if (ImagoGraphicsDecode()) return 1;
		} else {
			if (GraphicsDecode()) return 1;
		}
	}

	ZetInit(0);
	ZetOpen(0);

	ZetMapMemory(Rom0, 0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvMainRAM, 0xc000, 0xc7ff, MAP_RAM);

	if (fastfred_hardware_type == 3) {
		ZetUnmapMemory(0x1000, 0x1fff, MAP_ROM); // sprites dma addy
		ZetMapMemory(DrvFGVidRAM, 0xc800, 0xcfff, MAP_RAM);
	}

	if (fastfred_hardware_type & 1) { // Fast Freddie, Fly Boy & Imago
		ZetMapMemory(DrvVidRAM, 0xd000, 0xd3ff, MAP_RAM); // video ram
		ZetMapMemory(DrvVidRAM, 0xd400, 0xd7ff, MAP_RAM); // mirror @ +0x400
		ZetMapMemory(DrvAttrRAM, 0xd800, 0xdbff, MAP_READ);
	} else { // jump coaster, boggy84
		ZetMapMemory(DrvVidRAM, 0xd800, 0xdbff, MAP_RAM); // video ram
		ZetMapMemory(DrvVidRAM, 0xdc00, 0xdfff, MAP_RAM); // mirror @ +0x400
		ZetMapMemory(DrvAttrRAM, 0xd000, 0xd3ff, MAP_READ);
	}

	ZetSetWriteHandler(fastfred_cpu0_write);
	ZetSetReadHandler(fastfred_cpu0_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(Rom1, 0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvSubRAM, 0x2000, 0x23ff, MAP_RAM);
	ZetSetWriteHandler(fastfred_cpu1_write);
	ZetSetReadHandler(fastfred_cpu1_read);
	ZetClose();

	AY8910Init(0, 1536000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910Init(1, 1536000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.10, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.10, BURN_SND_ROUTE_BOTH);

   	// for boggy '84, remove horrible hiss on one of the ay's channels using LPF.
	filter_rc_init(0, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_P(0), 0); // (CAP_P(0) = passthru / mix only)
	filter_rc_init(1, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_N(0x2a0), 1);
	filter_rc_init(2, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_N(0x4a0), 1);

	filter_rc_set_route(0, 0.10, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(1, 0.20, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(2, 0.20, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	ZetExit();
	GenericTilesExit();

	AY8910Exit(0);
	AY8910Exit(1);

	filter_rc_exit();

	BurnFree (AllMem);
	fastfred_hardware_type = 0;
	flyboymode = 0;
	boggy84mode = 0;
	boggy84bmode = 0;
	imagomode = 0;

	return 0;
}

static void draw_sprites()
{
	UINT8 *fastfred_spriteram = DrvAttrRAM + 0x40;

	for (INT32 offs = 32 - 4; offs >= 0; offs -= 4)
	{
		INT32 code, sx, sy;
		INT32 flipx, flipy;

		sx = fastfred_spriteram[offs + 3];
		sy = 240 - fastfred_spriteram[offs];

		if (fastfred_hardware_type == 3)
		{
			// Imago
			code  = (fastfred_spriteram[offs + 1]) & 0x3f;

			flipx = 0;
			flipy = 0;
		}
		else if (fastfred_hardware_type == 2)
		{
			// Boggy 84
			code  =  fastfred_spriteram[offs + 1] & 0x7f;
			flipx =  0;
			flipy =  fastfred_spriteram[offs + 1] & 0x80;
		}
		else if (fastfred_hardware_type == 1)
		{
			// Fly-Boy/Fast Freddie/Red Robin
			code  =  fastfred_spriteram[offs + 1] & 0x7f;
			flipx =  0;
			flipy = ~fastfred_spriteram[offs + 1] & 0x80;
		}
		else
		{
			// Jump Coaster
			code  = (fastfred_spriteram[offs + 1] & 0x3f) | 0x40;
			flipx = ~fastfred_spriteram[offs + 1] & 0x40;
			flipy =  fastfred_spriteram[offs + 1] & 0x80;
		}

		sy -= 16; // offsets

		if (sy < -15) sy += 256;

		if (fastfred_flipscreenx)
		{
			sx = 240 - sx;
			flipx = !flipx;
		}
		if (fastfred_flipscreeny)
		{
			sy = 240 - sy;
			flipy = !flipy;
		}

		INT32 color = fastfred_colorbank | (fastfred_spriteram[offs + 2] & 7);

		if (flipy) {
			if (flipx) {
				Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, (imagomode) ? GfxImagoSprites : Gfx1);
			} else {
				Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, (imagomode) ? GfxImagoSprites : Gfx1);
			}
		} else {
			if (flipx) {
				Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, (imagomode) ? GfxImagoSprites : Gfx1);
			} else {
				Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, (imagomode) ? GfxImagoSprites : Gfx1);
			}
		}
	}
}

static void draw_chars()
{ // draw chars
	for (INT32 offs = 0; offs < 0x400; offs++)
	{
		INT32 sx = offs & 0x1f;
		INT32 sy = (offs >> 2) & 0xf8;

		INT32 code = fastfred_charbank | DrvVidRAM[offs];
		INT32 color = fastfred_colorbank | fastfred_color_select[sx];

		if (imagomode) {// for bg
			UINT8 x = offs & 0x1f;

			code = fastfred_charbank * 0x100 + DrvVidRAM[offs];
			color = fastfred_colorbank | (DrvAttrRAM[2 * x + 1] & 0x07);
		}

		sy -= 16;

		sy -= (fastfred_scroll[sx]+((fastfred_hardware_type == 1) ? 2 : 0));
		if (sy < -15) sy += 0x100;

		sx <<= 3;

		if (fastfred_flipscreeny) {
			if (fastfred_flipscreenx) {
				Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, Gfx0);
			} else {
				Render8x8Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, Gfx0);
			}
		} else {
			if (fastfred_flipscreenx) {
				Render8x8Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, Gfx0);
			} else {
				Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, Gfx0);
			}
		}
	}
}

static void draw_web()
{ // imago
	for (INT32 offs = 0; offs < 0x400; offs++)
	{
		INT32 sx = (offs & 0x1f) << 3;
		INT32 sy = (offs >> 5) << 3;

		INT32 code = offs & 0x1ff;
		INT32 color = 0;

		sy -= 16;

		if (sy < -7) sy += 256;
		if (sx < -7) sx += 256;

		if (fastfred_flipscreeny) {
			if (fastfred_flipscreenx) {
				Render8x8Tile_FlipXY_Clip(pTransDraw, code, sx, sy, color, 1, 0x140, Gfx3);
			} else {
				Render8x8Tile_FlipY_Clip(pTransDraw, code, sx, sy, color, 1, 0x140, Gfx3);
			}
		} else {
			if (fastfred_flipscreenx) {
				Render8x8Tile_FlipX_Clip(pTransDraw, code, sx, sy, color, 1, 0x140, Gfx3);
			} else {
				Render8x8Tile_Clip(pTransDraw, code, sx, sy, color, 1, 0x140, Gfx3);
			}
		}
	}
}

static void draw_imagofg()
{ // imago
	for (INT32 offs = 0; offs < 0x400; offs++)
	{
		INT32 sx = (offs & 0x1f) << 3;
		INT32 sy = (offs >> 5) << 3;

		INT32 code = DrvFGVidRAM[offs];
		INT32 color = 2;

		sy -= 16;

		if (sy < -7) sy += 256;
		if (sx < -7) sx += 256;

		if (fastfred_flipscreeny) {
			if (fastfred_flipscreenx) {
				Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, Gfx2);
			} else {
				Render8x8Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, Gfx2);
			}
		} else {
			if (fastfred_flipscreenx) {
				Render8x8Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, Gfx2);
			} else {
				Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, Gfx2);
			}
		}
	}
}


static INT32 ImagoDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	{ // fill background
		for (INT32 offs = 0; offs < nScreenWidth * nScreenHeight; offs++) {
			pTransDraw[offs] = fastfred_background_color;
		}
	}

	if (nBurnLayer & 1) draw_web();
	// draw_stars();
	if (nBurnLayer & 2) draw_chars(); // imago's bg
	if (nBurnLayer & 4) draw_sprites();
	if (nBurnLayer & 8) draw_imagofg();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	{ // fill background
		for (INT32 offs = 0; offs < nScreenWidth * nScreenHeight; offs++) {
			pTransDraw[offs] = fastfred_background_color;
		}
	}

	draw_chars();

	draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void DrvMakeInputs()
{
	// Reset Inputs (all active HIGH)
	DrvInput[0] = 0;
	DrvInput[1] = 0;

	// Compile Digital Inputs
	for (INT32 i = 0; i < 8; i++) {
		DrvInput[0] |= (DrvJoy1[i] & 1) << i;
		DrvInput[1] |= (DrvJoy2[i] & 1) << i;
	}
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	DrvMakeInputs();

	INT32 nInterleave = 128;
	INT32 nCyclesDone[2] = { 0, 0 };
	INT32 nCyclesTotal[2] = { 3108000 / 60, 1536000 / 60 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		INT32 nCycleSegment;

		ZetOpen(0);
		nCycleSegment = (nCyclesTotal[0] - nCyclesDone[0]) / (nInterleave - i);
		nCyclesDone[0] += ZetRun(nCycleSegment);
		if (i == ( nInterleave - 1) && fastfred_cpu0_interrupt_enable) ZetNmi();
		ZetClose();

		if (~fastfred_hardware_type & 1) continue;

		ZetOpen(1);
		nCycleSegment = (nCyclesTotal[1] - nCyclesDone[1]) / (nInterleave - i);
		nCyclesDone[1] += ZetRun(nCycleSegment);
		if (fastfred_cpu1_interrupt_enable && (i % (nInterleave / 4)) == ((nInterleave / 4) - 1)) ZetNmi();
		ZetClose();
	}

	if (pBurnSoundOut) {
		AY8910Render(&pAY8910Buffer[0], pBurnSoundOut, nBurnSoundLen, 0);

		if (boggy84mode) { // hiss-removal lpf (see init)
			filter_rc_update(0, pAY8910Buffer[0], pBurnSoundOut, nBurnSoundLen);
			filter_rc_update(1, pAY8910Buffer[1], pBurnSoundOut, nBurnSoundLen);
			filter_rc_update(2, pAY8910Buffer[2], pBurnSoundOut, nBurnSoundLen);
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

		SCAN_VAR(fastfred_background_color);
		SCAN_VAR(fastfred_cpu0_interrupt_enable);
		SCAN_VAR(fastfred_cpu1_interrupt_enable);
		SCAN_VAR(fastfred_colorbank);
		SCAN_VAR(fastfred_charbank);
		SCAN_VAR(fastfred_flipscreenx);
		SCAN_VAR(fastfred_flipscreeny);
		SCAN_VAR(fastfred_soundlatch);
		SCAN_VAR(fastfred_scroll);
		SCAN_VAR(fastfred_color_select);
	}

	return 0;
}

static INT32 fastfredInit()
{
	fastfred_hardware_type = 1;

	return DrvInit();
}


// Fast Freddie

static struct BurnRomInfo fastfredRomDesc[] = {
	{ "ffr.01",	0x1000, 0x15032c13, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "ffr.02",	0x1000, 0xf9642744, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ffr.03",	0x1000, 0xf0919727, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ffr.04",	0x1000, 0xc778751e, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ffr.05",	0x1000, 0xcd6e160a, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "ffr.06",	0x1000, 0x67f7f9b3, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "ffr.07",	0x1000, 0x2935c76a, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "ffr.08",	0x1000, 0x0fb79e7b, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "ffr.09",	0x1000, 0xa1ec8d7e, 2 | BRF_PRG | BRF_ESS }, //  8 audiocpu
	{ "ffr.10",	0x1000, 0x460ca837, 2 | BRF_PRG | BRF_ESS }, //  9

	{ "ffr.14",	0x1000, 0xe8a00e81, 3 | BRF_GRA },           // 10 gfx1
	{ "ffr.17",	0x1000, 0x701e0f01, 3 | BRF_GRA },           // 11
	{ "ffr.15",	0x1000, 0xb49b053f, 3 | BRF_GRA },           // 12
	{ "ffr.18",	0x1000, 0x4b208c8b, 3 | BRF_GRA },           // 13
	{ "ffr.16",	0x1000, 0x8c686bc2, 3 | BRF_GRA },           // 14
	{ "ffr.19",	0x1000, 0x75b613f6, 3 | BRF_GRA },           // 15

	{ "ffr.11",	0x1000, 0x0e1316d4, 4 | BRF_GRA },           // 16 gfx2
	{ "ffr.12",	0x1000, 0x94c06686, 4 | BRF_GRA },           // 17
	{ "ffr.13",	0x1000, 0x3fcfaa8e, 4 | BRF_GRA },           // 18

	{ "red.9h",	0x0100, 0xb801e294, 5 | BRF_GRA },           // 19 proms
	{ "green.8h",	0x0100, 0x7da063d0, 5 | BRF_GRA },           // 20
	{ "blue.7h",	0x0100, 0x85c05c18, 5 | BRF_GRA },           // 21
};

STD_ROM_PICK(fastfred)
STD_ROM_FN(fastfred)

struct BurnDriver BurnDrvFastfred = {
	"fastfred", "flyboy", NULL, NULL, "1982",
	"Fast Freddie\0", NULL, "Kaneko (Atari license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, fastfredRomInfo, fastfredRomName, NULL, NULL, CommonInputInfo, FastfredDIPInfo,
	fastfredInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Fly-Boy

static struct BurnRomInfo flyboyRomDesc[] = {
	{ "flyboy01.cpu",	0x1000, 0xb05aa900, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "flyboy02.cpu",	0x1000, 0x474867f5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rom3.cpu",		0x1000, 0xd2f8f085, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rom4.cpu",		0x1000, 0x19e5e15c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "flyboy05.cpu",	0x1000, 0x207551f7, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "rom6.cpu",		0x1000, 0xf5464c72, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "rom7.cpu",		0x1000, 0x50a1baff, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "rom8.cpu",		0x1000, 0xfe2ae95d, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "rom9.cpu",		0x1000, 0x5d05d1a0, 2 | BRF_PRG | BRF_ESS }, //  8 audiocpu
	{ "rom10.cpu",		0x1000, 0x7a28005b, 2 | BRF_PRG | BRF_ESS }, //  9

	{ "rom14.rom",		0x1000, 0xaeb07260, 3 | BRF_GRA },           // 10 gfx1
	{ "rom17.rom",		0x1000, 0xa834325b, 3 | BRF_GRA },           // 11
	{ "rom15.rom",		0x1000, 0xc10c7ce2, 3 | BRF_GRA },           // 12
	{ "rom18.rom",		0x1000, 0x2f196c80, 3 | BRF_GRA },           // 13
	{ "rom16.rom",		0x1000, 0x719246b1, 3 | BRF_GRA },           // 14
	{ "rom19.rom",		0x1000, 0x00c1c5d2, 3 | BRF_GRA },           // 15

	{ "rom11.rom",		0x1000, 0xee7ec342, 4 | BRF_GRA },           // 16 gfx2
	{ "rom12.rom",		0x1000, 0x84d03124, 4 | BRF_GRA },           // 17
	{ "rom13.rom",		0x1000, 0xfcb33ff4, 4 | BRF_GRA },           // 18

	{ "red.9h",		    0x0100, 0xb801e294, 5 | BRF_GRA },           // 19 proms
	{ "green.8h",		0x0100, 0x7da063d0, 5 | BRF_GRA },           // 20
	{ "blue.7h",		0x0100, 0x85c05c18, 5 | BRF_GRA },           // 21
};

STD_ROM_PICK(flyboy)
STD_ROM_FN(flyboy)

static void flyboy_patch()
{
	// The simulation fails due to the way Doze works,
	// so use patches from the bootleg
	Rom0[0x0397] = 0x00;
	Rom0[0x0398] = 0x00;
	Rom0[0x0399] = 0x00;
	Rom0[0x049F] = 0x00;
	Rom0[0x04A0] = 0x00;
	Rom0[0x04A1] = 0x00;
	Rom0[0x04F6] = 0x00;
	Rom0[0x04F7] = 0x00;
	Rom0[0x04F8] = 0x00;
	Rom0[0x0567] = 0x00;
	Rom0[0x0568] = 0x00;
	Rom0[0x0569] = 0x00;
	Rom0[0x069F] = 0x00;
	Rom0[0x06A0] = 0x00;
	Rom0[0x06A1] = 0x00;
	Rom0[0x06A2] = 0x00;
	Rom0[0x076F] = 0x00;
	Rom0[0x0770] = 0x00;
	Rom0[0x0856] = 0x00;
	Rom0[0x0857] = 0x00;
	Rom0[0x09D9] = 0x00;
	Rom0[0x09DA] = 0x00;
	Rom0[0x09DB] = 0x00;
	Rom0[0x0A87] = 0x00;
	Rom0[0x0A88] = 0x00;
	Rom0[0x0A89] = 0x00;
	Rom0[0x0A8B] = 0x00;
	Rom0[0x0A8C] = 0x00;
	Rom0[0x0A8D] = 0x00;
	Rom0[0x0A8E] = 0x00;
	Rom0[0x0A8F] = 0x00;
	Rom0[0x1037] = 0x00;
	Rom0[0x1038] = 0x00;
	Rom0[0x1045] = 0xC9;
	Rom0[0x1060] = 0xC9;
	Rom0[0x106E] = 0x00;
	Rom0[0x106F] = 0x00;
	Rom0[0x108B] = 0xC9;
	Rom0[0x1099] = 0x00;
	Rom0[0x109A] = 0x00;
	Rom0[0x10B4] = 0xC9;
	Rom0[0x10C2] = 0x00;
	Rom0[0x10C3] = 0x00;
	Rom0[0x10DD] = 0x00;
	Rom0[0x10DE] = 0x00;
	Rom0[0x10E9] = 0xC9;
	Rom0[0x1103] = 0x00;
	Rom0[0x1104] = 0x00;
	Rom0[0x110F] = 0xC9;
	Rom0[0x1122] = 0x00;
	Rom0[0x1123] = 0x00;
	Rom0[0x1124] = 0x00;
	Rom0[0x1125] = 0x00;
	Rom0[0x1126] = 0x00;
	Rom0[0x1127] = 0x00;
	Rom0[0x1128] = 0x00;
	Rom0[0x1129] = 0x00;
	Rom0[0x112A] = 0x00;
	Rom0[0x117A] = 0x00;
	Rom0[0x117B] = 0x00;
	Rom0[0x117C] = 0x00;
	Rom0[0x117D] = 0x00;
	Rom0[0x117E] = 0x00;
	Rom0[0x117F] = 0x00;
	Rom0[0x1317] = 0x00;
	Rom0[0x1318] = 0x00;
	Rom0[0x1319] = 0x00;
	Rom0[0x131A] = 0x00;
	Rom0[0x131B] = 0x00;
	Rom0[0x4A55] = 0x01;
}

static INT32 flyboyInit()
{
	fastfred_hardware_type = 1;
	flyboymode = 1;

	INT32 nRet = DrvInit();
	if (!nRet) {
		flyboy_patch();
	}
	return nRet;
}

struct BurnDriver BurnDrvFlyboy = {
	"flyboy", NULL, NULL, NULL, "1982",
	"Fly-Boy\0", NULL, "Kaneko", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, flyboyRomInfo, flyboyRomName, NULL, NULL, CommonInputInfo, FlyboyDIPInfo,
	flyboyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Fly-Boy (bootleg)

static struct BurnRomInfo flyboybRomDesc[] = {
	{ "rom1.cpu",	0x1000, 0xe9e1f527, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "rom2.cpu",	0x1000, 0x07fbe78c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rom3.cpu",	0x1000, 0xd2f8f085, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rom4.cpu",	0x1000, 0x19e5e15c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "rom5.cpu",	0x1000, 0xd56872ea, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "rom6.cpu",	0x1000, 0xf5464c72, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "rom7.cpu",	0x1000, 0x50a1baff, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "rom8.cpu",	0x1000, 0xfe2ae95d, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "rom9.cpu",	0x1000, 0x5d05d1a0, 2 | BRF_PRG | BRF_ESS }, //  8 audiocpu
	{ "rom10.cpu",	0x1000, 0x7a28005b, 2 | BRF_PRG | BRF_ESS }, //  9

	{ "rom14.rom",	0x1000, 0xaeb07260, 3 | BRF_GRA },           // 10 gfx1
	{ "rom17.rom",	0x1000, 0xa834325b, 3 | BRF_GRA },           // 11
	{ "rom15.rom",	0x1000, 0xc10c7ce2, 3 | BRF_GRA },           // 12
	{ "rom18.rom",	0x1000, 0x2f196c80, 3 | BRF_GRA },           // 13
	{ "rom16.rom",	0x1000, 0x719246b1, 3 | BRF_GRA },           // 14
	{ "rom19.rom",	0x1000, 0x00c1c5d2, 3 | BRF_GRA },           // 15

	{ "rom11.rom",	0x1000, 0xee7ec342, 4 | BRF_GRA },           // 16 gfx2
	{ "rom12.rom",	0x1000, 0x84d03124, 4 | BRF_GRA },           // 17
	{ "rom13.rom",	0x1000, 0xfcb33ff4, 4 | BRF_GRA },           // 18

	{ "red.9h",	    0x0100, 0xb801e294, 5 | BRF_GRA },           // 19 proms
	{ "green.8h",	0x0100, 0x7da063d0, 5 | BRF_GRA },           // 20
	{ "blue.7h",	0x0100, 0x85c05c18, 5 | BRF_GRA },           // 21
};

STD_ROM_PICK(flyboyb)
STD_ROM_FN(flyboyb)

struct BurnDriver BurnDrvFlyboyb = {
	"flyboyb", "flyboy", NULL, NULL, "1982",
	"Fly-Boy (bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, flyboybRomInfo, flyboybRomName, NULL, NULL, CommonInputInfo, FlyboyDIPInfo,
	fastfredInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Jump Coaster

static struct BurnRomInfo jumpcoasRomDesc[] = {
	{ "jumpcoas.001",	0x2000, 0x0778c953, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "jumpcoas.002",	0x2000, 0x57f59ce1, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "jumpcoas.003",	0x2000, 0xd9fc93be, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "jumpcoas.004",	0x2000, 0xdc108fc1, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "jumpcoas.005",	0x1000, 0x2dce6b07, 4 | BRF_GRA },           //  4 gfx1
	{ "jumpcoas.006",	0x1000, 0x0d24aa1b, 4 | BRF_GRA },           //  5
	{ "jumpcoas.007",	0x1000, 0x14c21e67, 4 | BRF_GRA },           //  6

	{ "jumpcoas.red",	0x0100, 0x13714880, 5 | BRF_GRA },           //  7 proms
	{ "jumpcoas.gre",	0x0100, 0x05354848, 5 | BRF_GRA },           //  8
	{ "jumpcoas.blu",	0x0100, 0xf4662db7, 5 | BRF_GRA },           //  9
};

STD_ROM_PICK(jumpcoas)
STD_ROM_FN(jumpcoas)

static INT32 jumpcoasInit()
{
	fastfred_hardware_type = 0;

	return DrvInit();
}

struct BurnDriver BurnDrvJumpcoas = {
	"jumpcoas", NULL, NULL, NULL, "1983",
	"Jump Coaster\0", NULL, "Kaneko", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, jumpcoasRomInfo, jumpcoasRomName, NULL, NULL, CommonInputInfo, JumpcoasDIPInfo,
	jumpcoasInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Jump Coaster (Taito)

static struct BurnRomInfo jumpcoastRomDesc[] = {
	{ "1.d1",		    0x2000, 0x8ac220c5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "jumpcoas.002",	0x2000, 0x57f59ce1, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.d3",		    0x2000, 0x17e4deba, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "jumpcoas.004",	0x2000, 0xdc108fc1, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "jumpcoas.005",	0x1000, 0x2dce6b07, 4 | BRF_GRA },           //  4 gfx1
	{ "jumpcoas.006",	0x1000, 0x0d24aa1b, 4 | BRF_GRA },           //  5
	{ "jumpcoas.007",	0x1000, 0x14c21e67, 4 | BRF_GRA },           //  6

	{ "jumpcoas.red",	0x0100, 0x13714880, 5 | BRF_GRA },           //  7 proms
	{ "jumpcoas.gre",	0x0100, 0x05354848, 5 | BRF_GRA },           //  8
	{ "jumpcoas.blu",	0x0100, 0xf4662db7, 5 | BRF_GRA },           //  9
};

STD_ROM_PICK(jumpcoast)
STD_ROM_FN(jumpcoast)

struct BurnDriver BurnDrvJumpcoast = {
	"jumpcoast", "jumpcoas", NULL, NULL, "1983",
	"Jump Coaster (Taito)\0", NULL, "Kaneko (Taito license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, jumpcoastRomInfo, jumpcoastRomName, NULL, NULL, CommonInputInfo, JumpcoasDIPInfo,
	jumpcoasInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Boggy '84

static struct BurnRomInfo boggy84RomDesc[] = {
	{ "p1.d1",	0x2000, 0x722cc0ec, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "p2.d2",	0x2000, 0x6c096798, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "p3.d3",	0x2000, 0x9da59104, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "p4.d4",	0x2000, 0x73ef6807, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "g1.h10",	0x1000, 0xf4238c68, 4 | BRF_GRA },           //  4 gfx1
	{ "g2.h11",	0x1000, 0xce285bd2, 4 | BRF_GRA },           //  5
	{ "g3.h12",	0x1000, 0x02f5f4fa, 4 | BRF_GRA },           //  6

	{ "r.e10",	0x0100, 0xf3862912, 5 | BRF_GRA },           //  7 proms
	{ "g.e11",	0x0100, 0x80b87220, 5 | BRF_GRA },           //  8
	{ "b.e12",	0x0100, 0x52b7f445, 5 | BRF_GRA },           //  9
};

STD_ROM_PICK(boggy84)
STD_ROM_FN(boggy84)

static INT32 boggy84Init()
{
	fastfred_hardware_type = 2;
	boggy84mode = 1;

	return DrvInit();
}

static INT32 boggy84bInit()
{
	fastfred_hardware_type = 2;
	boggy84mode = 1;
	boggy84bmode = 1;

	return DrvInit();
}

struct BurnDriver BurnDrvBoggy84 = {
	"boggy84", NULL, NULL, NULL, "1983",
	"Boggy '84\0", NULL, "Kaneko", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, boggy84RomInfo, boggy84RomName, NULL, NULL, TwoBtnInputInfo, Boggy84DIPInfo,
	boggy84Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};

// Boggy '84 (bootleg)

static struct BurnRomInfo boggy84bRomDesc[] = {
	{ "cpurom1.bin",	0x2000, 0x665266c0, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "cpurom2.bin",	0x2000, 0x6c096798, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "cpurom3.bin",	0x2000, 0x9da59104, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "cpurom4.bin",	0x2000, 0x73ef6807, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "gfx1.bin",		0x1000, 0xf4238c68, 4 | BRF_GRA },           //  4 gfx1
	{ "gfx2.bin",		0x1000, 0xce285bd2, 4 | BRF_GRA },           //  5
	{ "gfx3.bin",		0x1000, 0x02f5f4fa, 4 | BRF_GRA },           //  6

	{ "r12e",		0x0100, 0xf3862912, 5 | BRF_GRA },           //  7 proms
	{ "g12e",		0x0100, 0x80b87220, 5 | BRF_GRA },           //  8
	{ "b12e",		0x0100, 0x52b7f445, 5 | BRF_GRA },           //  9
};

STD_ROM_PICK(boggy84b)
STD_ROM_FN(boggy84b)

struct BurnDriver BurnDrvBoggy84b = {
	"boggy84b", "boggy84", NULL, NULL, "1983",
	"Boggy '84 (bootleg)\0", NULL, "bootleg (Eddie's Games)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, boggy84bRomInfo, boggy84bRomName, NULL, NULL, TwoBtnInputInfo, Boggy84DIPInfo,
	boggy84bInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Red Robin

static struct BurnRomInfo redrobinRomDesc[] = {
	{ "redro01f.16d",	0x1000, 0x0788ce10, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "redrob02.17d",	0x1000, 0xbf9b95b4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "redrob03.14b",	0x1000, 0x9386e40b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "redrob04.16b",	0x1000, 0x5cafffc4, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "redrob05.17b",	0x1000, 0xa224d41e, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "redrob06.14a",	0x1000, 0x822e0bd7, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "redrob07.15a",	0x1000, 0x0deacf17, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "redrob08.17a",	0x1000, 0x095cf908, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "redrob20.15e",	0x4000, 0x5cce22b7, 1 | BRF_PRG | BRF_ESS }, //  8

	{ "redrob09.1f",	0x1000, 0x21af2d03, 2 | BRF_PRG | BRF_ESS }, //  9 audiocpu
	{ "redro10f.1e",	0x1000, 0xbf0e772f, 2 | BRF_PRG | BRF_ESS }, // 10

	{ "redrob14.17l",	0x1000, 0xf6c571e0, 3 | BRF_GRA },           // 11 gfx1
	{ "redrob17.17j",	0x1000, 0x86dcdf21, 3 | BRF_GRA },           // 12
	{ "redrob15.15k",	0x1000, 0x05f7df48, 3 | BRF_GRA },           // 13
	{ "redrob18.16j",	0x1000, 0x7aeb2bb9, 3 | BRF_GRA },           // 14
	{ "redrob16.14l",	0x1000, 0x21349d09, 3 | BRF_GRA },           // 15
	{ "redrob19.14j",	0x1000, 0x7184d999, 3 | BRF_GRA },           // 16

	{ "redrob11.17m",	0x1000, 0x559f7894, 4 | BRF_GRA },           // 17 gfx2
	{ "redrob12.15m",	0x1000, 0xa763b11d, 4 | BRF_GRA },           // 18
	{ "redrob13.14m",	0x1000, 0xd667f45b, 4 | BRF_GRA },           // 19

	{ "red.9h",		    0x0100, 0xb801e294, 5 | BRF_GRA },           // 20 proms
	{ "green.8h",		0x0100, 0x7da063d0, 5 | BRF_GRA },           // 21
	{ "blue.7h",		0x0100, 0x85c05c18, 5 | BRF_GRA },           // 22
};

STD_ROM_PICK(redrobin)
STD_ROM_FN(redrobin)

struct BurnDriver BurnDrvRedrobin = {
	"redrobin", NULL, NULL, NULL, "1986",
	"Red Robin\0", NULL, "Elettronolo", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, redrobinRomInfo, redrobinRomName, NULL, NULL, TwoBtnInputInfo, RedrobinDIPInfo,
	fastfredInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


static INT32 imagoInit()
{
	fastfred_hardware_type = 3;
	imagomode = 1;

	INT32 nRet = DrvInit();
	if (!nRet) {
		memmove (Rom0 + 0x2000, Rom0 + 0x1000, 0x5000);
		memset (Rom0 + 0x1000, 0, 0x1000);		

	}
	return nRet;
}


// Imago (cocktail set)

static struct BurnRomInfo imagoRomDesc[] = {
	{ "imago11.82",		0x2000, 0x3cce69b4, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "imago12.83",		0x2000, 0x8dff98c0, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "13.bin",		    0x2000, 0xae684602, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "imago08.60",		0x1000, 0x4f77c2c9, 2 | BRF_PRG | BRF_ESS }, //  3 audiocpu

	{ "1.bin",		    0x1000, 0xf80a0b69, 3 | BRF_GRA },           //  4 gfx1
	{ "imago02.40",		0x1000, 0x71354480, 3 | BRF_GRA },           //  5
	{ "3.bin",		    0x1000, 0x722fd625, 3 | BRF_GRA },           //  6

	{ "imago04.51",		0x1000, 0xed987b3e, 4 | BRF_GRA },           //  7 gfx2
	{ "imago05.52",		0x1000, 0x77ee68ce, 4 | BRF_GRA },           //  8
	{ "imago07.56",		0x1000, 0x48b35190, 4 | BRF_GRA },           //  9
	{ "imago06.55",		0x1000, 0x136990fc, 4 | BRF_GRA },           // 10
	{ "imago09.64",		0x1000, 0x9efb806d, 4 | BRF_GRA },           // 11
	{ "imago10.65",		0x1000, 0x801a18d3, 4 | BRF_GRA },           // 12

	{ "imago14.170",	0x1000, 0xeded37f6, 6 | BRF_GRA },           // 13 gfx3

	{ "imago15.191",	0x1000, 0x85fcc195, 7 | BRF_GRA },           // 14 gfx4

	{ "imago.96",		0x0100, 0x5ba81edc, 5 | BRF_GRA },           // 15 proms
	{ "imago.95",		0x0100, 0xe2b7aa09, 5 | BRF_GRA },           // 16
	{ "imago.97",		0x0100, 0xe28a7f00, 5 | BRF_GRA },           // 17
};

STD_ROM_PICK(imago)
STD_ROM_FN(imago)

struct BurnDriver BurnDrvImago = {
	"imago", NULL, NULL, NULL, "1984",
	"Imago (cocktail set)\0", NULL, "Acom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, imagoRomInfo, imagoRomName, NULL, NULL, ImagoInputInfo, ImagoDIPInfo,
	imagoInit, DrvExit, DrvFrame, ImagoDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Imago (no cocktail set)

static struct BurnRomInfo imagoaRomDesc[] = {
	{ "imago11.82",		0x2000, 0x3cce69b4, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "imago12.83",		0x2000, 0x8dff98c0, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "imago13.84",		0x2000, 0xf0f14b4d, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "imago08.60",		0x1000, 0x4f77c2c9, 2 | BRF_PRG | BRF_ESS }, //  3 audiocpu

	{ "imago01.39",		0x1000, 0xf09fe0d4, 3 | BRF_GRA },           //  4 gfx1
	{ "imago02.40",		0x1000, 0x71354480, 3 | BRF_GRA },           //  5
	{ "imago03.41",		0x1000, 0x7aba3d98, 3 | BRF_GRA },           //  6

	{ "imago04.51",		0x1000, 0xed987b3e, 4 | BRF_GRA },           //  7 gfx2
	{ "imago05.52",		0x1000, 0x77ee68ce, 4 | BRF_GRA },           //  8
	{ "imago07.56",		0x1000, 0x48b35190, 4 | BRF_GRA },           //  9
	{ "imago06.55",		0x1000, 0x136990fc, 4 | BRF_GRA },           // 10
	{ "imago09.64",		0x1000, 0x9efb806d, 4 | BRF_GRA },           // 11
	{ "imago10.65",		0x1000, 0x801a18d3, 4 | BRF_GRA },           // 12

	{ "imago.96",		0x0100, 0x5ba81edc, 5 | BRF_GRA },           // 15 proms
	{ "imago.95",		0x0100, 0xe2b7aa09, 5 | BRF_GRA },           // 16
	{ "imago.97",		0x0100, 0xe28a7f00, 5 | BRF_GRA },           // 17

	{ "imago14.170",	0x1000, 0xeded37f6, 6 | BRF_GRA },           // 13 gfx3

	{ "imago15.191",	0x1000, 0x85fcc195, 7 | BRF_GRA },           // 14 gfx4

};

STD_ROM_PICK(imagoa)
STD_ROM_FN(imagoa)

struct BurnDriver BurnDrvImagoa = {
	"imagoa", "imago", NULL, NULL, "1983",
	"Imago (no cocktail set)\0", NULL, "Acom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, imagoaRomInfo, imagoaRomName, NULL, NULL, ImagoInputInfo, ImagoDIPInfo,
	imagoInit, DrvExit, DrvFrame, ImagoDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


