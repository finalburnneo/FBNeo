/*
   Games supported:
        * Zaxxon				ok
        * Super Zaxxon		ok
        * Future Spy			no
        * Razmatazz			no sound+bad controls sega_universal_sound_board_rom ?
        * Ixion					no
        * Congo Bongo		ok
*/
#include "tiles_generic.h"
#include "z80_intf.h"
#include "8255ppi.h"
#include "sn76496.h"
#include "samples.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvZ80DecROM;
static UINT8 *DrvZ80ROM2;
static UINT8 *DrvSndROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvZ80RAM2;
static UINT8 *DrvSprRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvColRAM;
static UINT32 *DrvPalette;
static UINT32 *Palette;

static UINT8 DrvRecalc;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[8];
static UINT8 DrvReset;

static UINT8 *interrupt_enable;

static UINT8 *zaxxon_fg_color;
static UINT8 *zaxxon_bg_color;
static UINT8 *zaxxon_bg_enable;
static UINT32 *zaxxon_bg_scroll;
static UINT8 *zaxxon_flipscreen;
static UINT8 *zaxxon_coin_enable;

static UINT8 *congo_color_bank;
static UINT8 *congo_fg_bank;
static UINT8 *congo_custom;

static UINT8 *zaxxon_bg_pixmap;

static UINT8 *soundlatch;

static INT32 futspy_sprite = 0;
static INT32 hardware_type = 0;
static UINT8 no_flip = 0;

static struct BurnInputInfo ZaxxonInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"},
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
	{"Service",		BIT_DIGITAL,	DrvJoy4 + 2,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
};

STDINPUTINFO(Zaxxon)

static struct BurnInputInfo CongoBongoInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 6,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 7,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"},
};

STDINPUTINFO(CongoBongo)



static struct BurnDIPInfo CongoBongoDIPList[]=
{
	{0x03, 0xff, 0xff, 0xc0, NULL		},
	{0x04, 0xff, 0xff, 0xc0, NULL		},
	{0x05, 0xff, 0xff, 0x04, NULL		},
	{0x10, 0xff, 0xff, 0x7f, NULL		},
	{0x11, 0xff, 0xff, 0x33, NULL		},

	{0   , 0xfe, 0   ,    2, "Test Back and Target"		},
	{0x03, 0x01, 0x20, 0x20, "No"		},
	{0x03, 0x01, 0x20, 0x00, "Yes"		},

	{0   , 0xfe, 0   ,    0, "Test I/O and Dip SW"		},
	{0x03, 0x01, 0x20, 0x20, "No"		},
	{0x03, 0x01, 0x20, 0x00, "Yes"		},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x04, 0x01, 0x0c, 0x0c, "Easy"		},
	{0x04, 0x01, 0x0c, 0x04, "Medium"		},
	{0x04, 0x01, 0x0c, 0x08, "Hard"		},
	{0x04, 0x01, 0x0c, 0x00, "Hardest"		},

	{0x10, 0xff, 0xff, 0x7f, NULL		},
	{0x11, 0xff, 0xff, 0x33, NULL		},

	{0   , 0xfe, 0   ,    1, "Service Mode (No Toggle)"		},
	{0x10, 0x01, 0x01, 0x00, "Off"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x10, 0x01, 0x03, 0x03, "10000"		},
	{0x10, 0x01, 0x03, 0x01, "20000"		},
	{0x10, 0x01, 0x03, 0x02, "30000"		},
	{0x10, 0x01, 0x03, 0x00, "40000"		},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x10, 0x01, 0x04, 0x00, "Off"		},
	{0x10, 0x01, 0x04, 0x04, "On"		},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x10, 0x01, 0x08, 0x00, "Off"		},
	{0x10, 0x01, 0x08, 0x08, "On"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x10, 0x01, 0x30, 0x30, "3"		},
	{0x10, 0x01, 0x30, 0x10, "4"		},
	{0x10, 0x01, 0x30, 0x20, "5"		},
	{0x10, 0x01, 0x30, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Sound"		},
	{0x10, 0x01, 0x40, 0x00, "Off"		},
	{0x10, 0x01, 0x40, 0x40, "On"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x10, 0x01, 0x80, 0x00, "Upright"		},
	{0x10, 0x01, 0x80, 0x80, "Cocktail"		},

	{0   , 0xfe, 0   ,    16, "SW2:!1,!2,!3,!4"		},
	{0x11, 0x01, 0x0f, 0x0f, "SW2:!1,!2,!3,!4"		},
	{0x11, 0x01, 0x0f, 0x07, "SW2:!1,!2,!3,!4"		},
	{0x11, 0x01, 0x0f, 0x0b, "SW2:!1,!2,!3,!4"		},
	{0x11, 0x01, 0x0f, 0x06, "2C/1C 5C/3C 6C/4C"		},
	{0x11, 0x01, 0x0f, 0x0a, "2C/1C 3C/2C 4C/3C"		},
	{0x11, 0x01, 0x0f, 0x03, "SW2:!1,!2,!3,!4"		},
	{0x11, 0x01, 0x0f, 0x02, "1C/1C 5C/6C"		},
	{0x11, 0x01, 0x0f, 0x0c, "1C/1C 4C/5C"		},
	{0x11, 0x01, 0x0f, 0x04, "1C/1C 2C/3C"		},
	{0x11, 0x01, 0x0f, 0x0d, "SW2:!1,!2,!3,!4"		},
	{0x11, 0x01, 0x0f, 0x08, "1C/2C 5C/11C"		},
	{0x11, 0x01, 0x0f, 0x00, "1C/2C 4C/9C"		},
	{0x11, 0x01, 0x0f, 0x05, "SW2:!1,!2,!3,!4"		},
	{0x11, 0x01, 0x0f, 0x09, "SW2:!1,!2,!3,!4"		},
	{0x11, 0x01, 0x0f, 0x01, "SW2:!1,!2,!3,!4"		},
	{0x11, 0x01, 0x0f, 0x0e, "SW2:!1,!2,!3,!4"		},

	{0   , 0xfe, 0   ,    16, "SW2:!5,!6,!7,!8"		},
	{0x11, 0x01, 0xf0, 0xf0, "SW2:!5,!6,!7,!8"		},
	{0x11, 0x01, 0xf0, 0x70, "SW2:!5,!6,!7,!8"		},
	{0x11, 0x01, 0xf0, 0xb0, "SW2:!5,!6,!7,!8"		},
	{0x11, 0x01, 0xf0, 0x60, "2C/1C 5C/3C 6C/4C"		},
	{0x11, 0x01, 0xf0, 0xa0, "2C/1C 3C/2C 4C/3C"		},
	{0x11, 0x01, 0xf0, 0x30, "SW2:!5,!6,!7,!8"		},
	{0x11, 0x01, 0xf0, 0x20, "1C/1C 5C/6C"		},
	{0x11, 0x01, 0xf0, 0xc0, "1C/1C 4C/5C"		},
	{0x11, 0x01, 0xf0, 0x40, "1C/1C 2C/3C"		},
	{0x11, 0x01, 0xf0, 0xd0, "SW2:!5,!6,!7,!8"		},
	{0x11, 0x01, 0xf0, 0x80, "1C/2C 5C/11C"		},
	{0x11, 0x01, 0xf0, 0x00, "1C/2C 4C/9C"		},
	{0x11, 0x01, 0xf0, 0x50, "SW2:!5,!6,!7,!8"		},
	{0x11, 0x01, 0xf0, 0x90, "SW2:!5,!6,!7,!8"		},
	{0x11, 0x01, 0xf0, 0x10, "SW2:!5,!6,!7,!8"		},
	{0x11, 0x01, 0xf0, 0xe0, "SW2:!5,!6,!7,!8"		},
};

STDDIPINFO(CongoBongo)



static struct BurnDIPInfo ZaxxonDIPList[]=
{
	{0x10, 0xff, 0xff, 0x7f, NULL		},
	{0x11, 0xff, 0xff, 0x33, NULL		},

	{0   , 0xfe, 0   ,    1, "Service Mode (No Toggle)"		},
	{0x10, 0x01, 0x01, 0x00, "Off"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x10, 0x01, 0x03, 0x03, "10000"		},
	{0x10, 0x01, 0x03, 0x01, "20000"		},
	{0x10, 0x01, 0x03, 0x02, "30000"		},
	{0x10, 0x01, 0x03, 0x00, "40000"		},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x10, 0x01, 0x04, 0x00, "Off"		},
	{0x10, 0x01, 0x04, 0x04, "On"		},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x10, 0x01, 0x08, 0x00, "Off"		},
	{0x10, 0x01, 0x08, 0x08, "On"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x10, 0x01, 0x30, 0x30, "3"		},
	{0x10, 0x01, 0x30, 0x10, "4"		},
	{0x10, 0x01, 0x30, 0x20, "5"		},
	{0x10, 0x01, 0x30, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Sound"		},
	{0x10, 0x01, 0x40, 0x00, "Off"		},
	{0x10, 0x01, 0x40, 0x40, "On"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x10, 0x01, 0x80, 0x00, "Upright"		},
	{0x10, 0x01, 0x80, 0x80, "Cocktail"		},

	{0   , 0xfe, 0   ,    16, "SW2:!1,!2,!3,!4"		},
	{0x11, 0x01, 0x0f, 0x0f, "SW2:!1,!2,!3,!4"		},
	{0x11, 0x01, 0x0f, 0x07, "SW2:!1,!2,!3,!4"		},
	{0x11, 0x01, 0x0f, 0x0b, "SW2:!1,!2,!3,!4"		},
	{0x11, 0x01, 0x0f, 0x06, "2C/1C 5C/3C 6C/4C"		},
	{0x11, 0x01, 0x0f, 0x0a, "2C/1C 3C/2C 4C/3C"		},
	{0x11, 0x01, 0x0f, 0x03, "SW2:!1,!2,!3,!4"		},
	{0x11, 0x01, 0x0f, 0x02, "1C/1C 5C/6C"		},
	{0x11, 0x01, 0x0f, 0x0c, "1C/1C 4C/5C"		},
	{0x11, 0x01, 0x0f, 0x04, "1C/1C 2C/3C"		},
	{0x11, 0x01, 0x0f, 0x0d, "SW2:!1,!2,!3,!4"		},
	{0x11, 0x01, 0x0f, 0x08, "1C/2C 5C/11C"		},
	{0x11, 0x01, 0x0f, 0x00, "1C/2C 4C/9C"		},
	{0x11, 0x01, 0x0f, 0x05, "SW2:!1,!2,!3,!4"		},
	{0x11, 0x01, 0x0f, 0x09, "SW2:!1,!2,!3,!4"		},
	{0x11, 0x01, 0x0f, 0x01, "SW2:!1,!2,!3,!4"		},
	{0x11, 0x01, 0x0f, 0x0e, "SW2:!1,!2,!3,!4"		},

	{0   , 0xfe, 0   ,    16, "SW2:!5,!6,!7,!8"		},
	{0x11, 0x01, 0xf0, 0xf0, "SW2:!5,!6,!7,!8"		},
	{0x11, 0x01, 0xf0, 0x70, "SW2:!5,!6,!7,!8"		},
	{0x11, 0x01, 0xf0, 0xb0, "SW2:!5,!6,!7,!8"		},
	{0x11, 0x01, 0xf0, 0x60, "2C/1C 5C/3C 6C/4C"		},
	{0x11, 0x01, 0xf0, 0xa0, "2C/1C 3C/2C 4C/3C"		},
	{0x11, 0x01, 0xf0, 0x30, "SW2:!5,!6,!7,!8"		},
	{0x11, 0x01, 0xf0, 0x20, "1C/1C 5C/6C"		},
	{0x11, 0x01, 0xf0, 0xc0, "1C/1C 4C/5C"		},
	{0x11, 0x01, 0xf0, 0x40, "1C/1C 2C/3C"		},
	{0x11, 0x01, 0xf0, 0xd0, "SW2:!5,!6,!7,!8"		},
	{0x11, 0x01, 0xf0, 0x80, "1C/2C 5C/11C"		},
	{0x11, 0x01, 0xf0, 0x00, "1C/2C 4C/9C"		},
	{0x11, 0x01, 0xf0, 0x50, "SW2:!5,!6,!7,!8"		},
	{0x11, 0x01, 0xf0, 0x90, "SW2:!5,!6,!7,!8"		},
	{0x11, 0x01, 0xf0, 0x10, "SW2:!5,!6,!7,!8"		},
	{0x11, 0x01, 0xf0, 0xe0, "SW2:!5,!6,!7,!8"		},
};

STDDIPINFO(Zaxxon)


static struct BurnDIPInfo SzaxxonDIPList[]=
{
	{0x01, 0xff, 0xff, 0x0c, NULL		},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x01, 0x01, 0x04, 0x04, "Normal"		},
	{0x01, 0x01, 0x04, 0x00, "Hard"		},

	{0x10, 0xff, 0xff, 0x7f, NULL		},
	{0x11, 0xff, 0xff, 0x33, NULL		},

	{0   , 0xfe, 0   ,    1, "Service Mode (No Toggle)"		},
	{0x10, 0x01, 0x01, 0x00, "Off"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x10, 0x01, 0x03, 0x03, "10000"		},
	{0x10, 0x01, 0x03, 0x01, "20000"		},
	{0x10, 0x01, 0x03, 0x02, "30000"		},
	{0x10, 0x01, 0x03, 0x00, "40000"		},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x10, 0x01, 0x04, 0x00, "Off"		},
	{0x10, 0x01, 0x04, 0x04, "On"		},

	{0   , 0xfe, 0   ,    2, "Unused"		},
	{0x10, 0x01, 0x08, 0x00, "Off"		},
	{0x10, 0x01, 0x08, 0x08, "On"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x10, 0x01, 0x30, 0x30, "3"		},
	{0x10, 0x01, 0x30, 0x10, "4"		},
	{0x10, 0x01, 0x30, 0x20, "5"		},
	{0x10, 0x01, 0x30, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Sound"		},
	{0x10, 0x01, 0x40, 0x00, "Off"		},
	{0x10, 0x01, 0x40, 0x40, "On"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x10, 0x01, 0x80, 0x00, "Upright"		},
	{0x10, 0x01, 0x80, 0x80, "Cocktail"		},

	{0   , 0xfe, 0   ,    16, "SW2:!1,!2,!3,!4"		},
	{0x11, 0x01, 0x0f, 0x0f, "SW2:!1,!2,!3,!4"		},
	{0x11, 0x01, 0x0f, 0x07, "SW2:!1,!2,!3,!4"		},
	{0x11, 0x01, 0x0f, 0x0b, "SW2:!1,!2,!3,!4"		},
	{0x11, 0x01, 0x0f, 0x06, "2C/1C 5C/3C 6C/4C"		},
	{0x11, 0x01, 0x0f, 0x0a, "2C/1C 3C/2C 4C/3C"		},
	{0x11, 0x01, 0x0f, 0x03, "SW2:!1,!2,!3,!4"		},
	{0x11, 0x01, 0x0f, 0x02, "1C/1C 5C/6C"		},
	{0x11, 0x01, 0x0f, 0x0c, "1C/1C 4C/5C"		},
	{0x11, 0x01, 0x0f, 0x04, "1C/1C 2C/3C"		},
	{0x11, 0x01, 0x0f, 0x0d, "SW2:!1,!2,!3,!4"		},
	{0x11, 0x01, 0x0f, 0x08, "1C/2C 5C/11C"		},
	{0x11, 0x01, 0x0f, 0x00, "1C/2C 4C/9C"		},
	{0x11, 0x01, 0x0f, 0x05, "SW2:!1,!2,!3,!4"		},
	{0x11, 0x01, 0x0f, 0x09, "SW2:!1,!2,!3,!4"		},
	{0x11, 0x01, 0x0f, 0x01, "SW2:!1,!2,!3,!4"		},
	{0x11, 0x01, 0x0f, 0x0e, "SW2:!1,!2,!3,!4"		},

	{0   , 0xfe, 0   ,    16, "SW2:!5,!6,!7,!8"		},
	{0x11, 0x01, 0xf0, 0xf0, "SW2:!5,!6,!7,!8"		},
	{0x11, 0x01, 0xf0, 0x70, "SW2:!5,!6,!7,!8"		},
	{0x11, 0x01, 0xf0, 0xb0, "SW2:!5,!6,!7,!8"		},
	{0x11, 0x01, 0xf0, 0x60, "2C/1C 5C/3C 6C/4C"		},
	{0x11, 0x01, 0xf0, 0xa0, "2C/1C 3C/2C 4C/3C"		},
	{0x11, 0x01, 0xf0, 0x30, "SW2:!5,!6,!7,!8"		},
	{0x11, 0x01, 0xf0, 0x20, "1C/1C 5C/6C"		},
	{0x11, 0x01, 0xf0, 0xc0, "1C/1C 4C/5C"		},
	{0x11, 0x01, 0xf0, 0x40, "1C/1C 2C/3C"		},
	{0x11, 0x01, 0xf0, 0xd0, "SW2:!5,!6,!7,!8"		},
	{0x11, 0x01, 0xf0, 0x80, "1C/2C 5C/11C"		},
	{0x11, 0x01, 0xf0, 0x00, "1C/2C 4C/9C"		},
	{0x11, 0x01, 0xf0, 0x50, "SW2:!5,!6,!7,!8"		},
	{0x11, 0x01, 0xf0, 0x90, "SW2:!5,!6,!7,!8"		},
	{0x11, 0x01, 0xf0, 0x10, "SW2:!5,!6,!7,!8"		},
	{0x11, 0x01, 0xf0, 0xe0, "SW2:!5,!6,!7,!8"		},
};

STDDIPINFO(Szaxxon)


static struct BurnInputInfo FutspyInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"},
//	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"},
//	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy4 + 2,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
};

STDINPUTINFO(Futspy)


static struct BurnDIPInfo FutspyDIPList[]=
{
	{0x05, 0xff, 0xff, 0x00, NULL		},
	{0x06, 0xff, 0xff, 0x43, NULL		},

	{0   , 0xfe, 0   ,    16, "SW2:!5,!6,!7,!8"		},
	{0x05, 0x01, 0x0f, 0x08, "SW2:!5,!6,!7,!8"		},
	{0x05, 0x01, 0x0f, 0x07, "SW2:!5,!6,!7,!8"		},
	{0x05, 0x01, 0x0f, 0x06, "SW2:!5,!6,!7,!8"		},
	{0x05, 0x01, 0x0f, 0x0a, "2C/1C 5C/3C 6C/4C"		},
	{0x05, 0x01, 0x0f, 0x0b, "2C/1C 4C/3C"		},
	{0x05, 0x01, 0x0f, 0x00, "SW2:!5,!6,!7,!8"		},
	{0x05, 0x01, 0x0f, 0x0e, "1C/1C 2C/3C"		},
	{0x05, 0x01, 0x0f, 0x0d, "1C/1C 4C/5C"		},
	{0x05, 0x01, 0x0f, 0x0c, "1C/1C 5C/6C"		},
	{0x05, 0x01, 0x0f, 0x09, "SW2:!5,!6,!7,!8"		},
	{0x05, 0x01, 0x0f, 0x01, "SW2:!5,!6,!7,!8"		},
	{0x05, 0x01, 0x0f, 0x0f, "1C/2C 5C/11C"		},
	{0x05, 0x01, 0x0f, 0x02, "SW2:!5,!6,!7,!8"		},
	{0x05, 0x01, 0x0f, 0x03, "SW2:!5,!6,!7,!8"		},
	{0x05, 0x01, 0x0f, 0x04, "SW2:!5,!6,!7,!8"		},
	{0x05, 0x01, 0x0f, 0x05, "SW2:!5,!6,!7,!8"		},

	{0   , 0xfe, 0   ,    16, "SW2:!5,!6,!7,!8"		},
	{0x05, 0x01, 0xf0, 0x80, "SW2:!5,!6,!7,!8"		},
	{0x05, 0x01, 0xf0, 0x70, "SW2:!5,!6,!7,!8"		},
	{0x05, 0x01, 0xf0, 0x60, "SW2:!5,!6,!7,!8"		},
	{0x05, 0x01, 0xf0, 0xa0, "2C/1C 5C/3C 6C/4C"		},
	{0x05, 0x01, 0xf0, 0xb0, "2C/1C 4C/3C"		},
	{0x05, 0x01, 0xf0, 0x00, "SW2:!5,!6,!7,!8"		},
	{0x05, 0x01, 0xf0, 0xe0, "1C/1C 2C/3C"		},
	{0x05, 0x01, 0xf0, 0xd0, "1C/1C 4C/5C"		},
	{0x05, 0x01, 0xf0, 0xc0, "1C/1C 5C/6C"		},
	{0x05, 0x01, 0xf0, 0x90, "SW2:!5,!6,!7,!8"		},
	{0x05, 0x01, 0xf0, 0x10, "SW2:!5,!6,!7,!8"		},
	{0x05, 0x01, 0xf0, 0xf0, "1C/2C 5C/11C"		},
	{0x05, 0x01, 0xf0, 0x20, "SW2:!5,!6,!7,!8"		},
	{0x05, 0x01, 0xf0, 0x30, "SW2:!5,!6,!7,!8"		},
	{0x05, 0x01, 0xf0, 0x40, "SW2:!5,!6,!7,!8"		},
	{0x05, 0x01, 0xf0, 0x50, "SW2:!5,!6,!7,!8"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x06, 0x01, 0x01, 0x01, "Upright"		},
	{0x06, 0x01, 0x01, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x06, 0x01, 0x02, 0x00, "Off"		},
	{0x06, 0x01, 0x02, 0x02, "On"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x06, 0x01, 0x0c, 0x00, "3"		},
	{0x06, 0x01, 0x0c, 0x04, "4"		},
	{0x06, 0x01, 0x0c, 0x08, "5"		},
	{0x06, 0x01, 0x0c, 0x0c, "Free Play"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x06, 0x01, 0x30, 0x00, "20K 40K 60K"		},
	{0x06, 0x01, 0x30, 0x10, "30K 60K 90K"		},
	{0x06, 0x01, 0x30, 0x20, "40K 70K 100K"		},
	{0x06, 0x01, 0x30, 0x30, "40K 80K 120K"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x06, 0x01, 0xc0, 0x00, "Easy"		},
	{0x06, 0x01, 0xc0, 0x40, "Medium"		},
	{0x06, 0x01, 0xc0, 0x80, "Hard"		},
	{0x06, 0x01, 0xc0, 0xc0, "Hardest"		},
};

STDDIPINFO(Futspy)

UINT8 __fastcall zaxxon_read(UINT16 address)
{
//	bprintf (PRINT_NORMAL, _T("%4.4x Inp[0] %02d Inp[1] %02d Inp[2] %02d Joy4[0] %02d Joy3[2] %02d Joy4[1] %02d Joy3[3] %02d \n"), 
//		address,DrvInputs[0],DrvInputs[1],DrvInputs[2],DrvJoy4[0],DrvJoy3[2],DrvJoy4[1],DrvJoy3[3]);
	// set up mirroring
/*
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"},
	{"P2 Coin",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"},
*/
	// video ram
	if (address >= 0x8000 && address <= 0x9fff) {
		return DrvVidRAM[address & 0x3ff];
	}

	if (address >= 0xa000 && address <= 0xafff) {
		return DrvSprRAM[address & 0xff];
	}
/*
	if ((address & 0xe100) == 0xc000)
		address &= 0xe703;

	if ((address & 0xe100) == 0xc100)
		address &= 0xe100;
*/
	if (address >= (0xc000+0x18fc) && address <= (0xc003+0x18fc)) 
	{
/*		AM_RANGE(0xc000, 0xc000) AM_MIRROR(0x18fc) AM_READ_PORT("SW00")
	AM_RANGE(0xc001, 0xc001) AM_MIRROR(0x18fc) AM_READ_PORT("SW01")
	AM_RANGE(0xc002, 0xc002) AM_MIRROR(0x18fc) AM_READ_PORT("DSW02")
	AM_RANGE(0xc003, 0xc003) AM_MIRROR(0x18fc) AM_READ_PORT("DSW03")		*/
		address-= 0x18fc;

	}

	if (address >= (0xc100+0x18fc) && address <= (0xc100+0x18ff)) 
	{
/* 	AM_RANGE(0xc100, 0xc100) AM_MIRROR(0x18ff) AM_READ_PORT("SW100") */
		address-= 0x18ff;
	}

	if (address >= (0xe03c+0x1f00) && address <= (0xe03f+0x1f00)) 
	{
/*  AM_RANGE(0xe03c, 0xe03f) AM_MIRROR(0x1f00) AM_DEVREADWRITE(PPI8255, "ppi8255", ppi8255_r, ppi8255_w) */
		address-= 0x1f00;
	}
	
	if (address >= 0xe03c && address <= 0xe03f) 
	{
/*  AM_RANGE(0xe03c, 0xe03f) AM_MIRROR(0x1f00) AM_DEVREADWRITE(PPI8255, "ppi8255", ppi8255_r, ppi8255_w) */
	//bprintf (PRINT_NORMAL, _T("ppi8255_r %02x\n"),address);
		return ppi8255_r(0, address  & 0x03);
	}

	switch (address)
	{
		case 0xc000:
			// sw00
		return DrvInputs[0];

		case 0xc001:
			// sw01 
		return DrvInputs[1];

		case 0xc002:
			// dsw02
		return DrvDips[0];

		case 0xc003:
			// dsw03
		return DrvDips[1];

		case 0xc100:
		{

//			UINT8 ret = 0;




/*
 * IN2 (bits NOT inverted)
 * bit 7 : CREDIT
 * bit 6 : COIN 2
 * bit 5 : COIN 1
 * bit 4 : ?
 * bit 3 : START 2
 * bit 2 : START 1

*/

/*
//			ret |= zaxxon_coin_enable[0] << 5;
//			ret |= zaxxon_coin_enable[1] << 6;
//			ret |= zaxxon_coin_enable[2] << 7;

#define IN2_COIN (1<<7)

			static int coin;
			UINT8 res = zaxxon_coin_enable[2];
	//bprintf (PRINT_NORMAL, _T("zaxxon_coin_enable %02d\n"), res);


			if (res & IN2_COIN)
			{
				if (coin) res &= ~IN2_COIN;
				else coin = 1;
			}
			else coin = 0;
*/
//DrvInputs[2];
			return DrvInputs[2];// sw100
		}
	}

	return 0;
}

static UINT8 sound_state[3];

void ZaxxonPPIWriteA(UINT8 data)
{
	UINT8 diff = data ^ sound_state[0];
	sound_state[0] = data;

	BurnSampleSetRoute(10, BURN_SND_SAMPLE_ROUTE_1, 0.01 + 0.01 * (data & 0x03), BURN_SND_ROUTE_BOTH);
	BurnSampleSetRoute(10, BURN_SND_SAMPLE_ROUTE_2, 0.01 + 0.01 * (data & 0x03), BURN_SND_ROUTE_BOTH);
	BurnSampleSetRoute(11, BURN_SND_SAMPLE_ROUTE_1, 0.01 + 0.01 * (data & 0x03), BURN_SND_ROUTE_BOTH);
	BurnSampleSetRoute(11, BURN_SND_SAMPLE_ROUTE_2, 0.01 + 0.01 * (data & 0x03), BURN_SND_ROUTE_BOTH);

	/* PLAYER SHIP C: channel 10 */
	if ((diff & 0x04) && !(data & 0x04)) { BurnSampleStop(11); BurnSamplePlay(10); }
	if ((diff & 0x04) &&  (data & 0x04)) BurnSampleStop(10);

	/* PLAYER SHIP D: channel 11 */
	if ((diff & 0x08) && !(data & 0x08)) { BurnSampleStop(10); BurnSamplePlay(11); }
	if ((diff & 0x08) &&  (data & 0x08)) BurnSampleStop(11);

	/* HOMING MISSILE: channel 0 */
	if ((diff & 0x10) && !(data & 0x10)) BurnSamplePlay(0);
	if ((diff & 0x10) &&  (data & 0x10)) BurnSampleStop(0);

	/* BASE MISSILE: channel 1 */
	if ((diff & 0x20) && !(data & 0x20)) BurnSamplePlay(1);

	/* LASER: channel 2 */
	if ((diff & 0x40) && !(data & 0x40)) BurnSamplePlay(2);
	if ((diff & 0x40) &&  (data & 0x40)) BurnSampleStop(2);

	/* BATTLESHIP: channel 3 */
	if ((diff & 0x80) && !(data & 0x80)) BurnSamplePlay(3);
	if ((diff & 0x80) &&  (data & 0x80)) BurnSampleStop(3);
}

void ZaxxonPPIWriteB(UINT8 data)
{
	UINT8 diff = data ^ sound_state[1];
	sound_state[1] = data;

	/* S-EXP: channel 4 */
	if ((diff & 0x10) && !(data & 0x10)) BurnSamplePlay(4);

	/* M-EXP: channel 5 */
	if ((diff & 0x20) && !(data & 0x20) && !BurnSampleGetStatus(5)) BurnSamplePlay(5);

	/* CANNON: channel 6 */
	if ((diff & 0x80) && !(data & 0x80)) BurnSamplePlay(6);
}

void ZaxxonPPIWriteC(UINT8 data)
{
	UINT8 diff = data ^ sound_state[2];
	sound_state[2] = data;

	/* SHOT: channel 7 */
	if ((diff & 0x01) && !(data & 0x01)) BurnSamplePlay(7);

	/* ALARM2: channel 8 */
	if ((diff & 0x04) && !(data & 0x04)) BurnSamplePlay(8);

	/* ALARM3: channel 9 */
	if ((diff & 0x08) && !(data & 0x08) && !BurnSampleGetStatus(9)) BurnSamplePlay(9);
}

static UINT8 CongoPPIReadA()
{
	return *soundlatch;
}

void CongoPPIWriteB(UINT8 data)
{
	//bprintf (PRINT_NORMAL, _T("CongoPPIWriteB %02x most be 2: %d\n"),data,(data & 0x02));
	UINT8 diff = data ^ sound_state[1];
	sound_state[1] = data;

	if(1)
	{
		/* GORILLA: channel 0 */
		if ((diff & 0x02) && !(data & 0x02) && !BurnSampleGetStatus(0)) BurnSamplePlay(0);
	}
}

void CongoPPIWriteC(UINT8 data)
{
	//bprintf (PRINT_NORMAL, _T("CongoPPIWriteC %02x\n"),data);
	UINT8 diff = data ^ sound_state[2];
	sound_state[2] = data;

	if(1)
	{
		/* BASS DRUM: channel 1 */
		if ((diff & 0x01) && !(data & 0x01)) BurnSamplePlay(1);
		if ((diff & 0x01) &&  (data & 0x01)) BurnSampleStop(1);

		/* CONGA (LOW): channel 2 */
		if ((diff & 0x02) && !(data & 0x02)) BurnSamplePlay(2);
		if ((diff & 0x02) &&  (data & 0x02)) BurnSampleStop(2);
	
		/* CONGA (HIGH): channel 3 */
		if ((diff & 0x04) && !(data & 0x04)) BurnSamplePlay(3);
		if ((diff & 0x04) &&  (data & 0x04)) BurnSampleStop(3);
	
		/* RIM: channel 4 */
		if ((diff & 0x08) && !(data & 0x08)) BurnSamplePlay(4);
		if ((diff & 0x08) &&  (data & 0x08)) BurnSampleStop(4);
	}
}

void __fastcall zaxxon_write(UINT16 address, UINT8 data)
{
//	//bprintf (PRINT_NORMAL, _T("write %4.4x %2.2x\n"), address, data);

	// set up mirroring

	// video ram
	if (address >= 0x8000 && address <= 0x9fff) {
		DrvVidRAM[address & 0x3ff] = data;
		return;
	}

	if (address >= 0xa000 && address <= 0xafff) {
		DrvSprRAM[address & 0xff] = data;
		return;
	}
/*
	if (address >= 0xff3c && address <= 0xff3e) {
		//bprintf (PRINT_NORMAL, _T("1 zaxxon_sound_write %4.4x %2.2x\n"), address, data);
//		zaxxon_sound_write(address & 0x0c);
		ppi8255_w(0, address  & 0x3, data);
		return;
	}
*/
	if (address >= 0xff3c && address <= 0xff3f) 
	{
/*	AM_RANGE(0xe03c, 0xe03f) AM_MIRROR(0x1f00) AM_DEVREADWRITE(PPI8255, "ppi8255", ppi8255_r, ppi8255_w)		*/
		address-= 0x1f00;
	}

	if ((address >= 0xc000+0x18f8) && address <= (0xc006+0x18f8)) 
	{
/*	AM_RANGE(0xc000, 0xc002) AM_MIRROR(0x18f8) AM_WRITE(zaxxon_coin_enable_w)
	AM_RANGE(0xc003, 0xc004) AM_MIRROR(0x18f8) AM_WRITE(zaxxon_coin_counter_w)
	AM_RANGE(0xc006, 0xc006) AM_MIRROR(0x18f8) AM_WRITE(zaxxon_flipscreen_w)*/
		address-= 0x18f8;
	}

	if ((address >= 0xe0f0+0x1f00) && address <= (0xe0fb+0x1f00)) 
	{
/*	AM_RANGE(0xe0f0, 0xe0f0) AM_MIRROR(0x1f00) AM_WRITE(int_enable_w)
	AM_RANGE(0xe0f1, 0xe0f1) AM_MIRROR(0x1f00) AM_WRITE(zaxxon_fg_color_w)
	AM_RANGE(0xe0f8, 0xe0f9) AM_MIRROR(0x1f00) AM_WRITE(zaxxon_bg_position_w)
	AM_RANGE(0xe0fa, 0xe0fa) AM_MIRROR(0x1f00) AM_WRITE(zaxxon_bg_color_w)
	AM_RANGE(0xe0fb, 0xe0fb) AM_MIRROR(0x1f00) AM_WRITE(zaxxon_bg_enable_w)	*/
		address-= 0x1f00;
	}

	if (address >= 0xe03c && address <= 0xe03f) {
		//bprintf (PRINT_NORMAL, _T("2 zaxxon_sound_write %4.4x %2.2x\n"), address, data);
//		zaxxon_sound_write(address & 0x0c);
		ppi8255_w(0, address  & 0x03, data);
//bprintf (PRINT_NORMAL, _T("2 ppi8255_w done\n"));
		return;
	}
/*
	if ((address & 0xe000) == 0xc000)
		address &= 0xe707;

	if ((address & 0xe000) == 0xe000)
		address &= 0xe0ff;
*/
//	//bprintf (PRINT_NORMAL, _T("%4.4x %2.2x mirrored\n"), address, data);


	switch (address)
	{
		case 0xc000:
		case 0xc001:
		case 0xc002:
			//bprintf (PRINT_NORMAL, _T("write coins %02d\n"), data & 1);
			zaxxon_coin_enable[address & 3] = data & 1;
		return;

		case 0xc003:
		case 0xc004:
			// coin counter_w
		return;

		case 0xc006:
			*zaxxon_flipscreen = ~data & 1;
		return;


		case 0xe0f0:
			*interrupt_enable = data & 1;
			if (~data & 1) ZetLowerIrq();
		return;

		case 0xe0f1:
			*zaxxon_fg_color = data << 7;
		return;

		case 0xe0f8:
			*zaxxon_bg_scroll &= 0xf00;
			*zaxxon_bg_scroll |= data;
		return;

		case 0xe0f9:
			*zaxxon_bg_scroll &= 0x0ff;
			*zaxxon_bg_scroll |= (data & 0x07) << 8;
		return; 
			
		case 0xe0fa:
			*zaxxon_bg_color = data << 7;
		return;

		case 0xe0fb:
			*zaxxon_bg_enable = data & 1;
		return;
	}
}

UINT8 __fastcall congo_read(UINT16 address)
{
	//bprintf (PRINT_NORMAL, _T("%4.4x DrvInputs[0] %02d DrvInputs[1] %02d DrvInputs[2] %02d\n"), address,DrvInputs[0],DrvInputs[1],DrvInputs[2]);

	// set up mirrors
	if ((address & 0xe000) == 0xa000) {
		if (address & 0x400) {
			return DrvVidRAM[address & 0x3ff];
		} else {
			return DrvColRAM[address & 0x3ff];
		}
	}

//	if ((address & 0xe000) == 0xc000) {
//		address &= 0xe03b;
//	}

	switch (address)
	{
		case 0xc000: return DrvInputs[0];        // in0
		case 0xc001: return DrvInputs[1];        // in1
		case 0xc002: // dsw02                    // dsw1
		case 0xc003: // dsw03                    // dsw2
		case 0xc008: return DrvInputs[2];        // in2
			return 0;
	}

	return 0;
}

// zet.cpp
void ZetRunAdjust(int nCycles);

static void congo_sprite_custom_w(int offset, UINT8 data)
{
	congo_custom[offset] = data;

	/* seems to trigger on a write of 1 to the 4th byte */
	if (offset == 3 && data == 0x01)
	{
		UINT16 saddr = congo_custom[0] | (congo_custom[1] << 8);
		int count = congo_custom[2];

		/* count cycles (just a guess) */
		ZetRunAdjust(-(count * 5));

		/* this is just a guess; the chip is hardwired to the spriteram */
		while (count-- >= 0)
		{
			UINT8 daddr = DrvZ80RAM[saddr & 0xfff] * 4;
			DrvSprRAM[(daddr + 0) & 0xff] = DrvZ80RAM[(saddr + 1) & 0xfff];
			DrvSprRAM[(daddr + 1) & 0xff] = DrvZ80RAM[(saddr + 2) & 0xfff];
			DrvSprRAM[(daddr + 2) & 0xff] = DrvZ80RAM[(saddr + 3) & 0xfff];
			DrvSprRAM[(daddr + 3) & 0xff] = DrvZ80RAM[(saddr + 4) & 0xfff];
			saddr += 0x20;
		}
	}
}


void __fastcall congo_write(UINT16 address, UINT8 data)
{
	// set up mirroring
	if ((address & 0xe000) == 0xa000) {
		if (address & 0x400) {
			DrvVidRAM[address & 0x3ff] = data;
		} else {
			DrvColRAM[address & 0x3ff] = data;
		}
		return;
	}

	if ((address >= 0xc038+0x1fc0) && address <= (0xc03f+0x1fc0))
	{
		// AM_RANGE(0xc038, 0xc03f) AM_MIRROR(0x1fc0) AM_WRITE(soundlatch_byte_w)
		address-= 0x1fc0;
	}


	if ((address & 0xe000) == 0xc000) {
		address &= 0xe03f;
	}

	switch (address)
	{
		case 0xc018:
		case 0xc019:
		case 0xc01a:
			// zaxxon_coin_enable_w
		return;

		case 0xc01b:
		case 0xc01c:
			// zaxxon_coin_counter_w
		return;

		case 0xc01d:
			*zaxxon_bg_enable = data & 1;
		return;

		case 0xc01e:
			*zaxxon_flipscreen = ~data & 1;
		return;

		case 0xc01f:
			*interrupt_enable = data & 1;
			if (~data & 1) ZetLowerIrq();
		return;

		case 0xc021:
			*zaxxon_fg_color = data << 7;
		return;

		case 0xc023:
			*zaxxon_bg_color = data << 7;
		return;

		case 0xc026:
			*congo_fg_bank = data & 1;
		return;

		case 0xc027:
			*congo_color_bank = data & 1;
		return;

		case 0xc028:
		//case 0xc02a:
			*zaxxon_bg_scroll &= 0xf00;
			*zaxxon_bg_scroll |= data;
		return;

		case 0xc029:
		//case 0xc02b:
			*zaxxon_bg_scroll &= 0x0ff;
			*zaxxon_bg_scroll |= (data & 0x07) << 8;
		return; 
			
		case 0xc030:
		case 0xc031:
		case 0xc032:
		case 0xc033:
			congo_sprite_custom_w(address & 3, data);
		return;

		case 0xc038:
		case 0xc039:
		case 0xc03a:
		case 0xc03b:
		case 0xc03c:
		case 0xc03d:
		case 0xc03e:
		case 0xc03f:
			*soundlatch = data;
		return;
	}
}

void __fastcall congo_sound_write(UINT16 address, UINT8 data)
{
		//bprintf (PRINT_NORMAL, _T("???? congo_sound_write %4.4x %2.2x\n"), address, data);
	
	if ((address >= 0x6000+0x1fff) && address <= (0x6000+0x1fff)) 
	{
//	AM_RANGE(0x6000, 0x6000) AM_MIRROR(0x1fff) AM_DEVWRITE("sn1", sn76496_device, write)
		address-= 0x1fff;
	}

	if ((address >= 0x8000+0x1ffc) && address <= (0x8003+0x1ffc)) 
	{
//AM_RANGE(0x8000, 0x8003) AM_MIRROR(0x1ffc) AM_DEVREADWRITE("ppi8255", i8255_device, read, write)
		address-= 0x1ffc;
	}

	if ((address >= 0xa000+0x1ffc) && address <= (0xa000+0x1ffc)) 
	{
//AM_RANGE(0xa000, 0xa000) AM_MIRROR(0x1fff) AM_DEVWRITE("sn2", sn76496_device, write)
		address-= 0x1ffc;
	}
//	address &= 0xe003;

	switch (address)
	{
		case 0x6000:
			SN76496Write(0, data);
		return;

		case 0x8000:
		case 0x8001:
		case 0x8002:
		case 0x8003:
			ppi8255_w(0, address  & 0x03, data);
		return;

		case 0xa000:
			SN76496Write(1, data);
		return;
	}

	// set up mirroring
        if ((address & 0xc000) == 0x4000) {
		DrvZ80RAM2[address & 0x7ff] = data;
		return;
	}
}

UINT8 __fastcall congo_sound_read(UINT16 address)
{
	// set up mirroring
	if ((address & 0xc000) == 0x4000) {
		return DrvZ80RAM2[address & 0x7ff];
	}

	/*if ((address >= 0x8000+0x1ffc) && address <= (0x8003+0x1ffc))
	{ //  AM_RANGE(0x8000, 0x8003) AM_MIRROR(0x1ffc) AM_DEVREADWRITE("ppi8255", i8255_device, read, write)
		address-= 0x1ffc;
	}*/

	address &= 0xe003; // this takes care of AM_MIRROR(0x1ffc)

	switch (address)
	{
		case 0x8000:
		case 0x8001:
		case 0x8002:
		case 0x8003:
			return ppi8255_r(0, address & 0x03);
	}

	return 0;
}

static int DrvGfxDecode()
{
	int CharPlane[2] = { 0x4000 * 1, 0x4000 * 0 };
	int TilePlane[3] = { 0x10000 * 2, 0x10000 * 1, 0x10000 * 0 };
	int SpritePlane[3] = { 0x20000 * 2, 0x20000 * 1, 0x20000 * 0 };
	int SpriteXOffs[32] = { 0, 1, 2, 3, 4, 5, 6, 7,
			8*8+0, 8*8+1, 8*8+2, 8*8+3, 8*8+4, 8*8+5, 8*8+6, 8*8+7,
			16*8+0, 16*8+1, 16*8+2, 16*8+3, 16*8+4, 16*8+5, 16*8+6, 16*8+7,
			24*8+0, 24*8+1, 24*8+2, 24*8+3, 24*8+4, 24*8+5, 24*8+6, 24*8+7 };
	int SpriteYOffs[32] = { 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			32*8, 33*8, 34*8, 35*8, 36*8, 37*8, 38*8, 39*8,
			64*8, 65*8, 66*8, 67*8, 68*8, 69*8, 70*8, 71*8,
			96*8, 97*8, 98*8, 99*8, 100*8, 101*8, 102*8, 103*8 };

	UINT8 *tmp = (UINT8*)malloc(0xc000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x1000);

	GfxDecode(0x0100, 2,  8,  8, CharPlane,   SpriteXOffs, SpriteYOffs, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x6000);

	GfxDecode(0x0400, 3,  8,  8, TilePlane,   SpriteXOffs, SpriteYOffs, 0x040, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0xc000);

	GfxDecode(0x0080, 3, 32, 32, SpritePlane, SpriteXOffs, SpriteYOffs, 0x400, tmp, DrvGfxROM2);

	free (tmp);

	return 0;
}

static void DrvPaletteInit(int len)
{
	for (int i = 0; i < len; i++)
	{
		int bit0, bit1, bit2, r, g, b;

		bit0 = (DrvColPROM[i] >> 0) & 0x01;
		bit1 = (DrvColPROM[i] >> 1) & 0x01;
		bit2 = (DrvColPROM[i] >> 2) & 0x01;
		r = bit0 * 33 + bit1 * 70 + bit2 * 151;

		bit0 = (DrvColPROM[i] >> 3) & 0x01;
		bit1 = (DrvColPROM[i] >> 4) & 0x01;
		bit2 = (DrvColPROM[i] >> 5) & 0x01;
		g = bit0 * 33 + bit1 * 70 + bit2 * 151;

		bit0 = (DrvColPROM[i] >> 6) & 0x01;
		bit1 = (DrvColPROM[i] >> 7) & 0x01;
		b = bit0 * 78 + bit1 * 168;

		Palette[i] = (r << 16) | (g << 8) | b;
		int rgb = Palette[i];
//		DrvPalette[i] = BurnHighCol(rgb >> 16, rgb >> 8, rgb, 0);
		DrvPalette[i] = rgb;//BurnHighCol(r, g, b, 0);
		//bprintf (PRINT_NORMAL, _T("DrvPalette %04x %04x\n"),DrvPalette[i],rgb);
//		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}

	DrvColPROM += 0x100;
}

static void bg_layer_init()
{
#ifdef dump_bitmap
	FILE *fz = fopen("bglayer.bmp", "wb");

	UINT8 data[54] = {
		0x42, 0x4D, 0x36, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x28, 0x00, 
		0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x01, 0x00, 0x18, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};

	fwrite (data, 54, 1, fz);
#endif

	int len = (hardware_type == 2) ? 0x2000 : 0x4000;
	int mask = len-1;

	for (int offs = 0; offs < 32 * 512; offs++)
	{
		int sx = (offs & 0x1f) << 3;
		int sy = (offs >> 5) << 3;

		int moffs = offs & mask;

		int code = (DrvGfxROM3[moffs]) | ((DrvGfxROM3[moffs | len] & 3) << 8);
		int color = (DrvGfxROM3[moffs | len] & 0xf0) >> 1;

		UINT8 *src = DrvGfxROM1 + (code << 6);

		for (int y = 0; y < 8; y++, sy++) {
			for (int x = 0; x < 8; x++) {
				zaxxon_bg_pixmap[sy * 256 + sx + x] = src[(y << 3) | x] | color;
			}
		}
	}
#ifdef dump_bitmap
	UINT8 tmp[3];

	for (int i = 0; i < 1048576; i++) {
		int t =	Palette[zaxxon_bg_pixmap[i]];
		UINT8 r, g, b;
		r = (t >> 24) & 0xff;
		g = (t >> 16) & 0xff;
		b = (t >>  8) & 0xff;

		tmp[0] = r;
		tmp[1] = g;
		tmp[2] = b;

		fwrite (tmp, 3, 1, fz);
	}

	fclose (fz);
#endif
}

static int DrvDoReset()
{
	DrvReset = 0;

	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	if (hardware_type == 2) {
		ZetOpen(1);
		ZetReset();
		ZetClose();
	}

	BurnSampleReset();

	return 0;
}

static int MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x010000;
	DrvZ80DecROM		= Next; Next += 0x010000;
	DrvZ80ROM2		= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += 0x004000;
	DrvGfxROM1		= Next; Next += 0x010000;
	DrvGfxROM2		= Next; Next += 0x020000;
	DrvGfxROM3		= Next; Next += 0x010000;

	DrvColPROM		= Next; Next += 0x000200;

	Palette			= (UINT32*)Next; Next += 0x0200 * sizeof(int);
	DrvPalette		= (UINT32*)Next; Next += 0x0200 * sizeof(int);

	zaxxon_bg_pixmap	= Next; Next += 0x100000;

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x001000;
	DrvZ80RAM2		= Next; Next += 0x001000;
	DrvSprRAM		= Next; Next += 0x000100;
	DrvVidRAM		= Next; Next += 0x000400;
	DrvColRAM		= Next; Next += 0x000400;

	interrupt_enable	= Next; Next += 0x000001;

	zaxxon_fg_color		= Next; Next += 0x000001;
	zaxxon_bg_color		= Next; Next += 0x000001;
	zaxxon_bg_enable	= Next; Next += 0x000001;
	congo_color_bank	= Next; Next += 0x000001;
	congo_fg_bank		= Next; Next += 0x000001;
	congo_custom		= Next; Next += 0x000004;
	zaxxon_flipscreen	= Next; Next += 0x000001;
	zaxxon_coin_enable	= Next; Next += 0x000004;

	zaxxon_bg_scroll	= (UINT32*)Next; Next += 0x000001 * sizeof(int);

	soundlatch		= Next; Next += 0x000001;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static int DrvInit()
{
	AllMem = NULL;
	MemIndex();
	int nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)malloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		for (int i = 0; i < 3; i++) {
			if (BurnLoadRom(DrvZ80ROM  + i * 0x2000,  0 + i, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + i * 0x2000,  5 + i, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + i * 0x4000,  8 + i, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM3 + i * 0x2000, 11 + i, 1)) return 1;
		}

		for (int i = 0; i < 2; i++) {
			if (BurnLoadRom(DrvGfxROM0 + i * 0x0800,  3 + i, 1)) return 1;
			if (BurnLoadRom(DrvColPROM + i * 0x0100, 15 + i, 1)) return 1;
		}

		if (BurnLoadRom(DrvGfxROM3 + 0x6000,  14, 1)) return 1;

		DrvGfxDecode();
		DrvPaletteInit(0x100);
		bg_layer_init();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapArea(0x0000, 0x5fff, 0, DrvZ80ROM);
	ZetMapArea(0x0000, 0x5fff, 2, DrvZ80ROM);
	ZetMapArea(0x6000, 0x6fff, 0, DrvZ80RAM);
	ZetMapArea(0x6000, 0x6fff, 1, DrvZ80RAM);
	ZetMapArea(0x6000, 0x6fff, 2, DrvZ80RAM);
	ZetMapArea(0x8000, 0x83ff, 0, DrvVidRAM);
	ZetMapArea(0x8000, 0x83ff, 1, DrvVidRAM);
	ZetMapArea(0xa000, 0xa0ff, 0, DrvSprRAM);
	ZetMapArea(0xa000, 0xa0ff, 1, DrvSprRAM);
	ZetSetWriteHandler(zaxxon_write);
	ZetSetReadHandler(zaxxon_read);
	ZetClose();

	ppi8255_init(1);
	PPI0PortWriteA = ZaxxonPPIWriteA;
	PPI0PortWriteB = ZaxxonPPIWriteB;
	PPI0PortWriteC = ZaxxonPPIWriteC;

	GenericTilesInit();

	BurnSampleInit(0);

	BurnSampleSetAllRoutesAllSamples(0.50, BURN_SND_ROUTE_BOTH);

	// for Zaxxon:
	// Homing Missile
			BurnSampleSetRoute(0, BURN_SND_SAMPLE_ROUTE_1, 0.61, BURN_SND_ROUTE_BOTH);
			BurnSampleSetRoute(0, BURN_SND_SAMPLE_ROUTE_2, 0.61, BURN_SND_ROUTE_BOTH);
	// Ground Missile take-off
			BurnSampleSetRoute(1, BURN_SND_SAMPLE_ROUTE_1, 0.30, BURN_SND_ROUTE_BOTH);
			BurnSampleSetRoute(1, BURN_SND_SAMPLE_ROUTE_2, 0.30, BURN_SND_ROUTE_BOTH);
	// Pew! Pew!
			BurnSampleSetRoute(6, BURN_SND_SAMPLE_ROUTE_1, 0.50, BURN_SND_ROUTE_BOTH);
			BurnSampleSetRoute(6, BURN_SND_SAMPLE_ROUTE_2, 0.50, BURN_SND_ROUTE_BOTH);

			BurnSampleSetRoute(10, BURN_SND_SAMPLE_ROUTE_1, 0.01 * 3, BURN_SND_ROUTE_BOTH);
			BurnSampleSetRoute(10, BURN_SND_SAMPLE_ROUTE_2, 0.01 * 3, BURN_SND_ROUTE_BOTH);
			BurnSampleSetRoute(11, BURN_SND_SAMPLE_ROUTE_1, 0.01 * 3, BURN_SND_ROUTE_BOTH);
			BurnSampleSetRoute(11, BURN_SND_SAMPLE_ROUTE_2, 0.01 * 3, BURN_SND_ROUTE_BOTH);

	DrvDoReset();

	return 0;
}

static int DrvExit()
{
	MemEnd = AllRam = RamEnd = DrvZ80ROM = DrvZ80DecROM = DrvZ80ROM2 = NULL;
	DrvSndROM = DrvGfxROM0 = DrvGfxROM1 = DrvGfxROM2 = DrvGfxROM3 = NULL;
	DrvColPROM = DrvZ80RAM = DrvZ80RAM2 = DrvSprRAM = DrvVidRAM = DrvColRAM = NULL;
	DrvPalette = Palette = NULL;

	BurnSampleExit();
	ppi8255_exit();
	if (hardware_type == 2) {
		SN76496Exit();
	}
	ZetExit();
	GenericTilesExit();
	free (AllMem);
	AllMem = NULL;

	futspy_sprite = 0;
	hardware_type = 0;
	no_flip = 0;

	return 0;
}

static void draw_fg_layer(int type)
{
	for (int offs = 0x40; offs < 0x3c0; offs++)
	{
		int color;

		int sx = (offs & 0x1f);
		int sy = (offs >> 5);

		int code = DrvVidRAM[offs] + (*congo_fg_bank << 8);
		if (type != 2) {
			int colpromoffs = type ? code : (sx | ((sy >> 2) << 5));
			color = DrvColPROM[colpromoffs] & 0x0f;
		} else {
			color = (DrvColRAM[offs] & 0x1f) + (*congo_color_bank << 8);
		}
		sx <<= 3;
		sy <<= 3;

		/*if (*zaxxon_flipscreen) {
			sx ^= 0xf8;
			sy ^= 0xf8;
		}*/

		if(no_flip)
		{
			Render8x8Tile_Mask_Clip(pTransDraw, color, sx, sy-16, color, 3 /*actually 2*/, 0, 0, DrvGfxROM0);
		}
		else
		{
			Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, code, 248-sx, 232-sy, color, 3 /*actually 2*/, 0, 0, DrvGfxROM0);
		}
	}
}

static void draw_background(int skew)
{
	/* only draw if enabled */
	if (*zaxxon_bg_enable)
	{
		UINT8 *pixmap = zaxxon_bg_pixmap;
		int colorbase = *zaxxon_bg_color + (*congo_color_bank << 8);
		int xmask = 255;
		int ymask = 4095;
		int flipmask = *zaxxon_flipscreen ? 0xff : 0x00;
		int flipoffs = *zaxxon_flipscreen ? 0x38 : 0x40;
		int x, y;

		/* the starting X value is offset by 1 pixel (normal) or 7 pixels */
		/* (flipped) due to a delay in the loading */
		if (*zaxxon_flipscreen)
			flipoffs += 7;
		else
			flipoffs -= 1;

		/* loop over visible rows */
		for (y = 16; y < 240; y++)
		{
			UINT16 *dst = pTransDraw + (240-y) * 0x100;
			int srcx, srcy, vf;
			UINT8 *src;

			/* VF = flipped V signals */
			vf = y ^ flipmask;

			/* base of the source row comes from VF plus the scroll value */
			/* this is done by the 3 4-bit adders at U56, U74, U75 */
			srcy = vf + ((*zaxxon_bg_scroll << 1) ^ 0xfff) + 1;
			src = pixmap + (srcy & ymask) * 0x100;

			/* loop over visible colums */
			for (x = 0; x < 256; x++)
			{
				/* start with HF = flipped H signals */
				srcx = x ^ flipmask;
				if (skew)
				{
					/* position within source row is a two-stage addition */
					/* first stage is HF plus half the VF, done by the 2 4-bit */
					/* adders at U53, U54 */
					srcx += ((vf >> 1) ^ 0xff) + 1;

					/* second stage is first stage plus a constant based on the flip */
					/* value is 0x40 for non-flipped, or 0x38 for flipped */
					srcx += flipoffs;
				}

				/* store the pixel, offset by the color offset */
				dst[256-x] = src[srcx & xmask] + colorbase;
			}
		}
	}

	/* if not enabled, fill the background with black */
	else
		memset (pTransDraw, 0, nScreenWidth * nScreenHeight * 2);
}



int find_minimum_y(UINT8 value)
{
	int flipmask = *zaxxon_flipscreen ? 0xff : 0x00;
	int flipconst = *zaxxon_flipscreen ? 0xef : 0xf1;
	int y;

	/* the sum of the Y position plus a constant based on the flip state */
	/* is added to the current flipped VF; if the top 3 bits are 1, we hit */

	/* first find a 16-pixel bucket where we hit */
	for (y = 0; y < 256; y += 16)
	{
		int sum = (value + flipconst + 1) + (y ^ flipmask);
		if ((sum & 0xe0) == 0xe0)
			break;
	}

	/* then scan backwards until we no longer match */
	while (1)
	{
		int sum = (value + flipconst + 1) + ((y - 1) ^ flipmask);
		if ((sum & 0xe0) != 0xe0)
			break;
		y--;
	}

	/* add one line since we draw sprites on the previous line */
	return (y + 1) & 0xff;
}


inline int find_minimum_x(UINT8 value)
{
	int flipmask = *zaxxon_flipscreen ? 0xff : 0x00;
	int x;

	/* the sum of the X position plus a constant specifies the address within */
	/* the line bufer; if we're flipped, we will write backwards */
	x = (value + 0xef + 1) ^ flipmask;
	if (flipmask)
		x -= 31;
	return x & 0xff;
}

static void draw_sprites(UINT16 flipxmask, UINT16 flipymask)
{
	int flipmask = *zaxxon_flipscreen ? 0xff : 0x00;
	int offs;

	/* only the lower half of sprite RAM is read during rendering */
	for (offs = 0x7c; offs >= 0; offs -= 4)
	{
		int sy = find_minimum_y(DrvSprRAM[offs]);
		int flipy = (DrvSprRAM[offs + (flipymask >> 8)] ^ flipmask) & flipymask;
		int flipx = (DrvSprRAM[offs + (flipxmask >> 8)] ^ flipmask) & flipxmask;
		int code = DrvSprRAM[offs + 1] & (0x3f | (futspy_sprite * 0x40));
		int color = (DrvSprRAM[offs + 2] & 0x1f) + (*congo_color_bank << 5);
		int sx = find_minimum_x(DrvSprRAM[offs + 3]);

		//bprintf (PRINT_NORMAL, _T("flipy %d\n"),flipy);
		//bprintf (PRINT_NORMAL, _T("flipx %d\n"),flipx);

		sx = (240 - sx) - 16;
		if (sx < -15) sx += 256;
		sy = (240 - sy) - 30;
		if (sy < -15) sy += 256;

		if (flipy) {
			if (flipx) {
				// future spy
				//bprintf (PRINT_NORMAL, _T("flipxy\n"));
				Render32x32Tile_Mask_Clip(pTransDraw, code, sx,       sy,       color, 3, 0, 0, DrvGfxROM2);
				Render32x32Tile_Mask_Clip(pTransDraw, code, sx - 256, sy,       color, 3, 0, 0, DrvGfxROM2);
				Render32x32Tile_Mask_Clip(pTransDraw, code, sx,       sy - 256, color, 3, 0, 0, DrvGfxROM2);
				Render32x32Tile_Mask_Clip(pTransDraw, code, sx - 256, sy - 256, color, 3, 0, 0, DrvGfxROM2);

			} else {
				//bprintf (PRINT_NORMAL, _T("flipy\n"));
				Render32x32Tile_Mask_FlipX_Clip(pTransDraw, code, sx,       sy,       color, 3, 0, 0, DrvGfxROM2);
				Render32x32Tile_Mask_FlipX_Clip(pTransDraw, code, sx - 256, sy,       color, 3, 0, 0, DrvGfxROM2);
				Render32x32Tile_Mask_FlipX_Clip(pTransDraw, code, sx,       sy - 256, color, 3, 0, 0, DrvGfxROM2);
				Render32x32Tile_Mask_FlipX_Clip(pTransDraw, code, sx - 256, sy - 256, color, 3, 0, 0, DrvGfxROM2);
			}
		} else {
			if (flipx) {
				//bprintf (PRINT_NORMAL, _T("flipx\n"));
				Render32x32Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, DrvGfxROM2);
			} else {
				//bprintf (PRINT_NORMAL, _T("noflip\n"));
				// zaxxon & szaxxon
				if (sx > 31 && sx < (nScreenWidth - 31) && sy > 31 && sy < (nScreenHeight-31)) {
					Render32x32Tile_Mask_FlipXY(pTransDraw, code, sx, sy, color, 3, 0, 0, DrvGfxROM2);
				} else {
					Render32x32Tile_Mask_FlipXY_Clip(pTransDraw, code, sx,       sy,       color, 3, 0, 0, DrvGfxROM2);
					Render32x32Tile_Mask_FlipXY_Clip(pTransDraw, code, sx - 256, sy,       color, 3, 0, 0, DrvGfxROM2);
					Render32x32Tile_Mask_FlipXY_Clip(pTransDraw, code, sx,       sy - 256, color, 3, 0, 0, DrvGfxROM2);
					Render32x32Tile_Mask_FlipXY_Clip(pTransDraw, code, sx - 256, sy - 256, color, 3, 0, 0, DrvGfxROM2);
				}
			}	
		}
	}
}


static int DrvDraw()
{
	/*if (DrvRecalc) {
		for (int i = 0; i < 0x200; i++) {
			int rgb = Palette[i];
			DrvPalette[i] = BurnHighCol(rgb >> 16, rgb >> 8, rgb, 0);
		}
	}*/

	if (hardware_type == 1) {
		draw_background(0);
	} else {
		draw_background(1);
	}

	int flipx_mask = 0x140;
	if (futspy_sprite) flipx_mask += 0x040;
	if (hardware_type == 2) flipx_mask += 0x100;

	draw_sprites(flipx_mask, 0x180);

	draw_fg_layer(hardware_type);

	BurnTransferCopy(DrvPalette);
	return 0;
}

static int DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		DrvInputs[0] = 0x00;
		DrvInputs[1] = 0x00;
		DrvInputs[2] = 0x00;
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			// DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
		DrvInputs[2] ^= (DrvJoy3[2] & 1) << 2;   //  start 1
		DrvInputs[2] ^= (DrvJoy3[3] & 1) << 3;   //  start 2
		DrvInputs[2] ^= (DrvJoy4[0] & 1) << 5;   //  coins 1
		DrvInputs[2] ^= (DrvJoy4[1] & 1) << 6;	 //  coins 2
		// DrvInputs[2] ^= (DrvJoy4[0] & 1) << 7;   //  credits
	}

	INT32 nInterleave = 1;
	INT32 nCyclesDone, nCyclesTotal;

	nCyclesDone = 0;
	nCyclesTotal = 3041250 / 60;

	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)	{
		INT32 nCyclesSegment = (nCyclesTotal - nCyclesDone) / (nInterleave - i);

		nCyclesDone = ZetRun(nCyclesSegment);
		//	nCyclesDone = ZetRun(nCyclesTotal);
		if (*interrupt_enable) ZetRaiseIrq(0);
	}

	ZetClose();

	if (hardware_type == 2) {
		ZetOpen(1);
		ZetRun(4000000 / 60);
		if (*interrupt_enable) ZetRaiseIrq(0);
		ZetClose();
	}
//	SN76496Update(0, pBurnSoundOut, nBurnSoundLen);
//	SN76496Update(1, pBurnSoundOut, nBurnSoundLen);


	if (pBurnDraw) {
		DrvDraw();
	}

	if (pBurnSoundOut) {
		BurnSampleRender(pBurnSoundOut, nBurnSoundLen);
	}

	return 0;
}


static int CongoFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		DrvInputs[0] = 0x00;
		DrvInputs[1] = 0x00;
		DrvInputs[2] = 0x00;
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nCyclesSegment;
	INT32 nInterleave = 32;
	INT32 nCyclesTotal[2] = { 3041250 / 60, 4000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };
	INT32 nSoundBufferPos = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesSegment = nCyclesTotal[0] / nInterleave;
		nCyclesDone[0] += ZetRun(nCyclesSegment);
		if (i == nInterleave-1 && *interrupt_enable) ZetRaiseIrq(0);
		ZetClose();

		ZetOpen(1);
		nCyclesSegment = nCyclesTotal[1] / nInterleave;
		nCyclesDone[1] += ZetRun(nCyclesSegment);
		if (i%7==0) ZetSetIRQLine(0, ZET_IRQSTATUS_AUTO);
		ZetClose();

		// Render Sound Segment
		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			SN76496Update(0, pSoundBuf, nSegmentLength);
			SN76496Update(1, pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}
	// Make sure the buffer is entirely filled.
	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			SN76496Update(0, pSoundBuf, nSegmentLength);
			SN76496Update(1, pSoundBuf, nSegmentLength);
		}
		BurnSampleRender(pBurnSoundOut, nBurnSoundLen);
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
		BurnSampleScan(nAction, pnMin);

		if (hardware_type == 2) {
			SN76496Scan(nAction, pnMin);
		}
	}

	if (nAction & ACB_WRITE) {
	}

	return 0;
}


static struct BurnSampleInfo zaxxonSampleDesc[] = {
#if !defined (ROM_VERIFY)
	{"03.wav",   SAMPLE_AUTOLOOP },	/* 0 - Homing Missile */
	{"02.wav",   SAMPLE_NOLOOP },	/* 1 - Base Missile */
	{"01.wav",   SAMPLE_AUTOLOOP },	/* 2 - Laser (force field) */
	{"00.wav",   SAMPLE_AUTOLOOP },	/* 3 - Battleship (end of level boss) */
	{"11.wav",   SAMPLE_NOLOOP },	/* 4 - S-Exp (enemy explosion) */
	{"10.wav",   SAMPLE_NOLOOP },	/* 5 - M-Exp (ship explosion) */
	{"08.wav",   SAMPLE_NOLOOP },	/* 6 - Cannon (ship fire) */
	{"23.wav",   SAMPLE_NOLOOP },	/* 7 - Shot (enemy fire) */
	{"21.wav",   SAMPLE_NOLOOP },	/* 8 - Alarm 2 (target lock) */
	{"20.wav",   SAMPLE_NOLOOP },	/* 9 - Alarm 3 (low fuel) */
	{"05.wav",   SAMPLE_AUTOLOOP },	/* 10 - initial background noise */
	{"04.wav",   SAMPLE_AUTOLOOP },	/* 11 - looped asteroid noise */
#endif
	{ "",            0             }
};

STD_SAMPLE_PICK(zaxxon)
STD_SAMPLE_FN(zaxxon)


static struct BurnSampleInfo congoSampleDesc[] = {
#if !defined (ROM_VERIFY)
		{"gorilla.wav",   SAMPLE_NOLOOP },  /* 0 */
		{"bass.wav",   SAMPLE_NOLOOP },     /* 1 */
		{"congal.wav",   SAMPLE_NOLOOP },   /* 2 */
		{"congah.wav",   SAMPLE_NOLOOP },   /* 3 */
		{"rim.wav",   SAMPLE_NOLOOP },      /* 4 */
#endif
		{ "",            0             }
};

STD_SAMPLE_PICK(congo)
STD_SAMPLE_FN(congo)

// Zaxxon (set 1)

static struct BurnRomInfo zaxxonRomDesc[] = {
	{ "zaxxon3.u27",	0x2000, 0x6e2b4a30, 1 }, //  0 main
	{ "zaxxon2.u28",	0x2000, 0x1c9ea398, 1 }, //  1
	{ "zaxxon1.u29",	0x1000, 0x1c123ef9, 1 }, //  2

	{ "zaxxon14.u68",	0x0800, 0x07bf8c52, 2 }, //  3 gfx1
	{ "zaxxon15.u69",	0x0800, 0xc215edcb, 2 }, //  4

	{ "zaxxon6.u113",	0x2000, 0x6e07bb68, 3 }, //  5 gfx2
	{ "zaxxon5.u112",	0x2000, 0x0a5bce6a, 3 }, //  6
	{ "zaxxon4.u111",	0x2000, 0xa5bf1465, 3 }, //  7

	{ "zaxxon11.u77",	0x2000, 0xeaf0dd4b, 4 }, //  8 gfx3
	{ "zaxxon12.u78",	0x2000, 0x1c5369c7, 4 }, //  9
	{ "zaxxon13.u79",	0x2000, 0xab4e8a9a, 4 }, // 10

	{ "zaxxon8.u91",	0x2000, 0x28d65063, 5 }, // 11 gfx4
	{ "zaxxon7.u90",	0x2000, 0x6284c200, 5 }, // 12
	{ "zaxxon10.u93",	0x2000, 0xa95e61fd, 5 }, // 13
	{ "zaxxon9.u92",	0x2000, 0x7e42691f, 5 }, // 14

	{ "zaxxon.u98",		0x0100, 0x6cc6695b, 6 }, // 15 proms
	{ "zaxxon.u72",		0x0100, 0xdeaa21f7, 6 }, // 16
};

STD_ROM_PICK(zaxxon)
STD_ROM_FN(zaxxon)

struct BurnDriver BurnDrvZaxxon = {
	"zaxxon", "zaxxon", NULL, "zaxxon", "1982",
	"Zaxxon (set 1)\0", NULL, "Sega", "hardware",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, zaxxonRomInfo, zaxxonRomName, zaxxonSampleInfo, zaxxonSampleName, ZaxxonInputInfo, ZaxxonDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	224, 256, 3, 4
};


// Zaxxon (set 2)

static struct BurnRomInfo zaxxon2RomDesc[] = {
	{ "zaxxon3a.u27",	0x2000, 0xb18e428a, 1 }, //  0 main
	{ "zaxxon2.u28",	0x2000, 0x1c9ea398, 1 }, //  1
	{ "zaxxon1a.u29",	0x1000, 0x1977d933, 1 }, //  2

	{ "zaxxon14.u68",	0x0800, 0x07bf8c52, 2 }, //  3 gfx1
	{ "zaxxon15.u69",	0x0800, 0xc215edcb, 2 }, //  4

	{ "zaxxon6.u113",	0x2000, 0x6e07bb68, 3 }, //  5 gfx2
	{ "zaxxon5.u112",	0x2000, 0x0a5bce6a, 3 }, //  6
	{ "zaxxon4.u111",	0x2000, 0xa5bf1465, 3 }, //  7

	{ "zaxxon11.u77",	0x2000, 0xeaf0dd4b, 4 }, //  8 gfx3
	{ "zaxxon12.u78",	0x2000, 0x1c5369c7, 4 }, //  9
	{ "zaxxon13.u79",	0x2000, 0xab4e8a9a, 4 }, // 10

	{ "zaxxon8.u91",	0x2000, 0x28d65063, 5 }, // 11 gfx4
	{ "zaxxon7.u90",	0x2000, 0x6284c200, 5 }, // 12
	{ "zaxxon10.u93",	0x2000, 0xa95e61fd, 5 }, // 13
	{ "zaxxon9.u92",	0x2000, 0x7e42691f, 5 }, // 14

	{ "zaxxon.u98",		0x0100, 0x6cc6695b, 6 }, // 15 proms
	{ "j214a2.72",		0x0100, 0xa9e1fb43, 6 }, // 16
};

STD_ROM_PICK(zaxxon2)
STD_ROM_FN(zaxxon2)

struct BurnDriver BurnDrvZaxxon2 = {
	"zaxxon2", "zaxxon", NULL, "zaxxon", "1982",
	"Zaxxon (set 2)\0", NULL, "Sega", "hardware",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, zaxxon2RomInfo, zaxxon2RomName, zaxxonSampleInfo, zaxxonSampleName, ZaxxonInputInfo, ZaxxonDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	224, 256, 3, 4
};

// Jackson

static struct BurnRomInfo zaxxonbRomDesc[] = {
	{ "zaxxonb3.u27",	0x2000, 0x125bca1c, 1 }, //  0 main
	{ "zaxxonb2.u28",	0x2000, 0xc088df92, 1 }, //  1
	{ "zaxxonb1.u29",	0x1000, 0xe7bdc417, 1 }, //  2

	{ "zaxxon14.u68",	0x0800, 0x07bf8c52, 2 }, //  3 gfx1
	{ "zaxxon15.u69",	0x0800, 0xc215edcb, 2 }, //  4

	{ "zaxxon6.u113",	0x2000, 0x6e07bb68, 3 }, //  5 gfx2
	{ "zaxxon5.u112",	0x2000, 0x0a5bce6a, 3 }, //  6
	{ "zaxxon4.u111",	0x2000, 0xa5bf1465, 3 }, //  7

	{ "zaxxon11.u77",	0x2000, 0xeaf0dd4b, 4 }, //  8 gfx3
	{ "zaxxon12.u78",	0x2000, 0x1c5369c7, 4 }, //  9
	{ "zaxxon13.u79",	0x2000, 0xab4e8a9a, 4 }, // 10

	{ "zaxxon8.u91",	0x2000, 0x28d65063, 5 }, // 11 gfx4
	{ "zaxxon7.u90",	0x2000, 0x6284c200, 5 }, // 12
	{ "zaxxon10.u93",	0x2000, 0xa95e61fd, 5 }, // 13
	{ "zaxxon9.u92",	0x2000, 0x7e42691f, 5 }, // 14

	{ "zaxxon.u98",		0x0100, 0x6cc6695b, 6 }, // 15 proms
	{ "zaxxon.u72",		0x0100, 0xdeaa21f7, 6 }, // 16
};

STD_ROM_PICK(zaxxonb)
STD_ROM_FN(zaxxonb)

static void zaxxonb_decode()
{
	static const UINT8 data_xortable[2][8] =
	{
		{ 0x0a,0x0a,0x22,0x22,0xaa,0xaa,0x82,0x82 },	/* ...............0 */
		{ 0xa0,0xaa,0x28,0x22,0xa0,0xaa,0x28,0x22 },	/* ...............1 */
	};

	static const UINT8 opcode_xortable[8][8] =
	{
		{ 0x8a,0x8a,0x02,0x02,0x8a,0x8a,0x02,0x02 },	/* .......0...0...0 */
		{ 0x80,0x80,0x08,0x08,0xa8,0xa8,0x20,0x20 },	/* .......0...0...1 */
		{ 0x8a,0x8a,0x02,0x02,0x8a,0x8a,0x02,0x02 },	/* .......0...1...0 */
		{ 0x02,0x08,0x2a,0x20,0x20,0x2a,0x08,0x02 },	/* .......0...1...1 */
		{ 0x88,0x0a,0x88,0x0a,0xaa,0x28,0xaa,0x28 },	/* .......1...0...0 */
		{ 0x80,0x80,0x08,0x08,0xa8,0xa8,0x20,0x20 },	/* .......1...0...1 */
		{ 0x88,0x0a,0x88,0x0a,0xaa,0x28,0xaa,0x28 },	/* .......1...1...0 */
		{ 0x02,0x08,0x2a,0x20,0x20,0x2a,0x08,0x02 } 	/* .......1...1...1 */
	};

	int A;
	UINT8 *rom = DrvZ80ROM;
	int size = 0x6000;
	UINT8 *decrypt = DrvZ80DecROM;

	ZetOpen(0);
	ZetMapArea(0x0000, 0x5fff, 2, DrvZ80DecROM, DrvZ80ROM );
	ZetClose();

	for (A = 0x0000; A < size; A++)
	{
		int i,j;
		UINT8 src;

		src = rom[A];

		/* pick the translation table from bit 0 of the address */
		i = A & 1;

		/* pick the offset in the table from bits 1, 3 and 5 of the source data */
		j = ((src >> 1) & 1) + (((src >> 3) & 1) << 1) + (((src >> 5) & 1) << 2);
		/* the bottom half of the translation table is the mirror image of the top */
		if (src & 0x80) j = 7 - j;

		/* decode the ROM data */
		rom[A] = src ^ data_xortable[i][j];

		/* now decode the opcodes */
		/* pick the translation table from bits 0, 4, and 8 of the address */
		i = ((A >> 0) & 1) + (((A >> 4) & 1) << 1) + (((A >> 8) & 1) << 2);
		decrypt[A] = src ^ opcode_xortable[i][j];
	}
}

static int ZaxxonbInit()
{
	int nRet;

	nRet = DrvInit();

	if (nRet == 0) {
		zaxxonb_decode();
	}

	return nRet;
}

struct BurnDriver BurnDrvZaxxonb = {
	"zaxxonb", "zaxxon", NULL, "zaxxon", "1982",
	"Jackson\0", NULL, "bootleg", "hardware",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, zaxxonbRomInfo, zaxxonbRomName, zaxxonSampleInfo, zaxxonSampleName, ZaxxonInputInfo, ZaxxonDIPInfo,
	ZaxxonbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	224, 256, 3, 4
};

// Super Zaxxon

static struct BurnRomInfo szaxxonRomDesc[] = {
	{ "suzaxxon.3",	0x2000, 0xaf7221da, 1 }, //  0 main
	{ "suzaxxon.2",	0x2000, 0x1b90fb2a, 1 }, //  1
	{ "suzaxxon.1",	0x1000, 0x07258b4a, 1 }, //  2

	{ "suzaxxon.14",	0x0800, 0xbccf560c, 2 }, //  3 gfx1
	{ "suzaxxon.15",	0x0800, 0xd28c628b, 2 }, //  4

	{ "suzaxxon.6",	0x2000, 0xf51af375, 3 }, //  5 gfx2
	{ "suzaxxon.5",	0x2000, 0xa7de021d, 3 }, //  6
	{ "suzaxxon.4",	0x2000, 0x5bfb3b04, 3 }, //  7

	{ "suzaxxon.11",	0x2000, 0x1503ae41, 4 }, //  8 gfx3
	{ "suzaxxon.12",	0x2000, 0x3b53d83f, 4 }, //  9
	{ "suzaxxon.13",	0x2000, 0x581e8793, 4 }, // 10

	{ "suzaxxon.8",	0x2000, 0xdd1b52df, 5 }, // 11 gfx4
	{ "suzaxxon.7",	0x2000, 0xb5bc07f0, 5 }, // 12
	{ "suzaxxon.10",	0x2000, 0x68e84174, 5 }, // 13
	{ "suzaxxon.9",	0x2000, 0xa509994b, 5 }, // 14

	{ "suzaxxon.u98",	0x0100, 0x15727a9f, 6 }, // 15 proms
	{ "suzaxxon.u72",	0x0100, 0xdeaa21f7, 6 }, // 16
};

STD_ROM_PICK(szaxxon)
STD_ROM_FN(szaxxon)

static void sega_decode(const UINT8 convtable[32][4])
{
	int A;
	int length = 0x6000;
	int cryptlen = length;
	UINT8 *rom = DrvZ80ROM;
	UINT8 *decrypted = DrvZ80DecROM;

	ZetOpen(0);
	ZetMapArea(0x0000, 0x5fff, 2, DrvZ80DecROM, DrvZ80ROM);
	ZetClose();

	for (A = 0x0000;A < cryptlen;A++)
	{
		int xorval = 0;
		UINT8 src = rom[A];
		/* pick the translation table from bits 0, 4, 8 and 12 of the address */
		int row = (A & 1) + (((A >> 4) & 1) << 1) + (((A >> 8) & 1) << 2) + (((A >> 12) & 1) << 3);
		/* pick the offset in the table from bits 3 and 5 of the source data */
		int col = ((src >> 3) & 1) + (((src >> 5) & 1) << 1);
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
#if 0
	/* this is a kludge to catch anyone who has code that crosses the encrypted/ */
	/* decrypted boundary. ssanchan does it */
	if (length > 0x8000)
	{
		int bytes = MIN(length - 0x8000, 0x4000);
		memcpy(&decrypted[0x8000], &rom[0x8000], bytes);
	}
#endif
}

void szaxxon_decode()
{
	static const UINT8 convtable[32][4] =
	{
		/*       opcode                   data                     address      */
		/*  A    B    C    D         A    B    C    D                           */
		{ 0x88,0xa8,0x80,0xa0 }, { 0x28,0x20,0xa8,0xa0 },	/* ...0...0...0...0 */
		{ 0x08,0x28,0x88,0xa8 }, { 0x88,0x80,0x08,0x00 },	/* ...0...0...0...1 */
		{ 0xa8,0x28,0xa0,0x20 }, { 0x20,0xa0,0x00,0x80 },	/* ...0...0...1...0 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0x28,0x20,0xa8,0xa0 },	/* ...0...0...1...1 */
		{ 0x08,0x28,0x88,0xa8 }, { 0x88,0x80,0x08,0x00 },	/* ...0...1...0...0 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0x28,0x20,0xa8,0xa0 },	/* ...0...1...0...1 */
		{ 0xa8,0x28,0xa0,0x20 }, { 0x20,0xa0,0x00,0x80 },	/* ...0...1...1...0 */
		{ 0x08,0x28,0x88,0xa8 }, { 0x88,0x80,0x08,0x00 },	/* ...0...1...1...1 */
		{ 0x08,0x28,0x88,0xa8 }, { 0x88,0x80,0x08,0x00 },	/* ...1...0...0...0 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0x28,0x20,0xa8,0xa0 },	/* ...1...0...0...1 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0x28,0x20,0xa8,0xa0 },	/* ...1...0...1...0 */
		{ 0xa8,0x28,0xa0,0x20 }, { 0x20,0xa0,0x00,0x80 },	/* ...1...0...1...1 */
		{ 0xa8,0x28,0xa0,0x20 }, { 0x20,0xa0,0x00,0x80 },	/* ...1...1...0...0 */
		{ 0xa8,0x28,0xa0,0x20 }, { 0x20,0xa0,0x00,0x80 },	/* ...1...1...0...1 */
		{ 0x08,0x28,0x88,0xa8 }, { 0x88,0x80,0x08,0x00 },	/* ...1...1...1...0 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0x28,0x20,0xa8,0xa0 }	/* ...1...1...1...1 */
	};

	sega_decode(convtable);
}

static int sZaxxonInit()
{
	int nRet;
	nRet = DrvInit();

	if (nRet == 0) {
		szaxxon_decode();
	}

	return nRet;
}

struct BurnDriver BurnDrvSzaxxon = {
	"szaxxon", NULL, NULL, "zaxxon", "1982",
	"Super Zaxxon\0", NULL, "Sega", "hardware",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, szaxxonRomInfo, szaxxonRomName, zaxxonSampleInfo, zaxxonSampleName, ZaxxonInputInfo, SzaxxonDIPInfo,
	sZaxxonInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	224, 256, 3, 4
};

// Future Spy

static struct BurnRomInfo futspyRomDesc[] = {
	{ "fs_snd.u27",		0x2000, 0x7578fe7f, 1 }, //  0 main
	{ "fs_snd.u28",		0x2000, 0x8ade203c, 1 }, //  1
	{ "fs_snd.u29",		0x1000, 0x734299c3, 1 }, //  2

	{ "fs_snd.u68",		0x0800, 0x305fae2d, 2 }, //  3 gfx1
	{ "fs_snd.u69",		0x0800, 0x3c5658c0, 2 }, //  4

	{ "fs_vid.u113",	0x2000, 0x36d2bdf6, 3 }, //  5 gfx2
	{ "fs_vid.u112",	0x2000, 0x3740946a, 3 }, //  6
	{ "fs_vid.u111",	0x2000, 0x4cd4df98, 3 }, //  7

	{ "fs_vid.u77",		0x4000, 0x1b93c9ec, 4 }, //  8 gfx3
	{ "fs_vid.u78",		0x4000, 0x50e55262, 4 }, //  9
	{ "fs_vid.u79",		0x4000, 0xbfb02e3e, 4 }, // 10

	{ "fs_vid.u91",		0x2000, 0x86da01f4, 5 }, // 11 gfx4
	{ "fs_vid.u90",		0x2000, 0x2bd41d2d, 5 }, // 12
	{ "fs_vid.u93",		0x2000, 0xb82b4997, 5 }, // 13
	{ "fs_vid.u92",		0x2000, 0xaf4015af, 5 }, // 14

	{ "futrprom.u98",	0x0100, 0x9ba2acaa, 6 }, // 15 proms
	{ "futrprom.u72",	0x0100, 0xf9e26790, 6 }, // 16
};

STD_ROM_PICK(futspy)
STD_ROM_FN(futspy)

void futspy_decode()
{
	static const UINT8 convtable[32][4] =
	{
		/*       opcode                   data                     address      */
		/*  A    B    C    D         A    B    C    D                           */
		{ 0x28,0x08,0x20,0x00 }, { 0x28,0x08,0x20,0x00 },	/* ...0...0...0...0 */
		{ 0x80,0x00,0xa0,0x20 }, { 0x08,0x88,0x00,0x80 },	/* ...0...0...0...1 */
		{ 0x80,0x00,0xa0,0x20 }, { 0x08,0x88,0x00,0x80 },	/* ...0...0...1...0 */
		{ 0xa0,0x80,0x20,0x00 }, { 0x20,0x28,0xa0,0xa8 },	/* ...0...0...1...1 */
		{ 0x28,0x08,0x20,0x00 }, { 0x88,0x80,0xa8,0xa0 },	/* ...0...1...0...0 */
		{ 0x80,0x00,0xa0,0x20 }, { 0x08,0x88,0x00,0x80 },	/* ...0...1...0...1 */
		{ 0x80,0x00,0xa0,0x20 }, { 0x20,0x28,0xa0,0xa8 },	/* ...0...1...1...0 */
		{ 0x20,0x28,0xa0,0xa8 }, { 0x08,0x88,0x00,0x80 },	/* ...0...1...1...1 */
		{ 0x88,0x80,0xa8,0xa0 }, { 0x28,0x08,0x20,0x00 },	/* ...1...0...0...0 */
		{ 0x80,0x00,0xa0,0x20 }, { 0xa0,0x80,0x20,0x00 },	/* ...1...0...0...1 */
		{ 0x20,0x28,0xa0,0xa8 }, { 0x08,0x88,0x00,0x80 },	/* ...1...0...1...0 */
		{ 0x80,0x00,0xa0,0x20 }, { 0x20,0x28,0xa0,0xa8 },	/* ...1...0...1...1 */
		{ 0x88,0x80,0xa8,0xa0 }, { 0x88,0x80,0xa8,0xa0 },	/* ...1...1...0...0 */
		{ 0x80,0x00,0xa0,0x20 }, { 0x08,0x88,0x00,0x80 },	/* ...1...1...0...1 */
		{ 0x80,0x00,0xa0,0x20 }, { 0x28,0x08,0x20,0x00 },	/* ...1...1...1...0 */
		{ 0x20,0x28,0xa0,0xa8 }, { 0xa0,0x80,0x20,0x00 }	/* ...1...1...1...1 */
	};


	sega_decode(convtable);
}


static int futspyInit()
{
	int nRet;

	futspy_sprite = 1;
	no_flip = 1;
	nRet = DrvInit();

	if (nRet == 0) {
		futspy_decode();
	}

	return nRet;
}

struct BurnDriver BurnDrvFutspy = {
	"futspy", NULL, NULL, "zaxxon", "1984",
	"Future Spy\0", NULL, "Sega", "hardware",
	NULL, NULL, NULL, NULL,
	0 | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, futspyRomInfo, futspyRomName, zaxxonSampleInfo, zaxxonSampleName, FutspyInputInfo, FutspyDIPInfo, //FutspyInputInfo, FutspyDIPInfo,
	futspyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	224, 256, 3, 4
};

// Razzmatazz

static struct BurnRomInfo razmatazRomDesc[] = {
	{ "u27",	0x2000, 0x254f350f, 1 }, //  0 main
	{ "u28",	0x2000, 0x3a1eaa99, 1 }, //  1
	{ "u29",	0x2000, 0x0ee67e78, 1 }, //  2

	{ "1921.u68",	0x0800, 0x77f8ff5a, 3 }, //  3 gfx1
	{ "1922.u69",	0x0800, 0xcf63621e, 3 }, //  4

	{ "1934.u113",	0x2000, 0x39bb679c, 4 }, //  5 gfx2
	{ "1933.u112",	0x2000, 0x1022185e, 4 }, //  6
	{ "1932.u111",	0x2000, 0xc7a715eb, 4 }, //  7

	{ "1925.u77",	0x2000, 0xa7965437, 5 }, //  8 gfx3
	{ "1926.u78",	0x2000, 0x9a3af434, 5 }, //  9
	{ "1927.u79",	0x2000, 0x0323de2b, 5 }, // 10

	{ "1929.u91",	0x2000, 0x55c7c757, 6 }, // 11 gfx4
	{ "1928.u90",	0x2000, 0xe58b155b, 6 }, // 12
	{ "1931.u93",	0x2000, 0x55fe0f82, 6 }, // 13
	{ "1930.u92",	0x2000, 0xf355f105, 6 }, // 14

	{ "clr.u98",	0x0100, 0x0fd671af, 7 }, // 15 proms
	{ "clr.u72",	0x0100, 0x03233bc5, 7 }, // 16

	{ "1924.u51",	0x0800, 0xa75e0011, 2 }, // 17 usb
	{ "1923.u50",	0x0800, 0x59994a51, 2 }, // 18
};

STD_ROM_PICK(razmataz)
STD_ROM_FN(razmataz)

void nprinces_decode()
{
	static const UINT8 convtable[32][4] =
	{
		/*       opcode                   data                     address      */
		/*  A    B    C    D         A    B    C    D                           */
		{ 0x08,0x88,0x00,0x80 }, { 0xa0,0x20,0x80,0x00 },	/* ...0...0...0...0 */
		{ 0xa8,0xa0,0x28,0x20 }, { 0x88,0xa8,0x80,0xa0 },	/* ...0...0...0...1 */
		{ 0x88,0x80,0x08,0x00 }, { 0x28,0x08,0xa8,0x88 },	/* ...0...0...1...0 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0x28,0x08,0xa8,0x88 },	/* ...0...0...1...1 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0xa0,0x20,0x80,0x00 },	/* ...0...1...0...0 */
		{ 0xa8,0xa0,0x28,0x20 }, { 0xa8,0xa0,0x28,0x20 },	/* ...0...1...0...1 */
		{ 0x88,0x80,0x08,0x00 }, { 0x88,0xa8,0x80,0xa0 },	/* ...0...1...1...0 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0x88,0xa8,0x80,0xa0 },	/* ...0...1...1...1 */
		{ 0xa0,0x20,0x80,0x00 }, { 0xa0,0x20,0x80,0x00 },	/* ...1...0...0...0 */
		{ 0x08,0x88,0x00,0x80 }, { 0x28,0x08,0xa8,0x88 },	/* ...1...0...0...1 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0x88,0x80,0x08,0x00 },	/* ...1...0...1...0 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0x28,0x08,0xa8,0x88 },	/* ...1...0...1...1 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0x88,0xa8,0x80,0xa0 },	/* ...1...1...0...0 */
		{ 0x88,0xa8,0x80,0xa0 }, { 0x88,0xa8,0x80,0xa0 },	/* ...1...1...0...1 */
		{ 0x88,0x80,0x08,0x00 }, { 0x88,0x80,0x08,0x00 },	/* ...1...1...1...0 */
		{ 0x08,0x88,0x00,0x80 }, { 0x28,0x08,0xa8,0x88 }	/* ...1...1...1...1 */
	};


	sega_decode(convtable);
}

static int razmatazInit()
{
	int nRet;

	hardware_type = 1;
	no_flip = 1;

	nRet = DrvInit();

	if (nRet == 0) {
		nprinces_decode();
	}

	return nRet;
}

struct BurnDriver BurnDrvRazmataz = {
	"razmataz", NULL, NULL, NULL, "1983",
	"Razzmatazz\0", NULL, "Sega", "hardware",
	NULL, NULL, NULL, NULL,
	0 | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, razmatazRomInfo, razmatazRomName, NULL, NULL, ZaxxonInputInfo, ZaxxonDIPInfo, //RazmatazInputInfo, RazmatazDIPInfo,
	razmatazInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	224, 256, 3, 4
};


// Ixion (prototype)

static struct BurnRomInfo ixionRomDesc[] = {
	{ "1937d.u27",	0x2000, 0xf447aac5, 1 }, //  0 main
	{ "1938b.u28",	0x2000, 0x17f48640, 1 }, //  1
	{ "1955b.u29",	0x1000, 0x78636ec6, 1 }, //  2

	{ "1939a.u68",	0x0800, 0xc717ddc7, 3 }, //  5 gfx1
	{ "1940a.u69",	0x0800, 0xec4bb3ad, 3 }, //  6

	{ "1952a.u113",	0x2000, 0xffb9b03d, 4 }, //  7 gfx2
	{ "1951a.u112",	0x2000, 0xdb743f1b, 4 }, //  8
	{ "1950a.u111",	0x2000, 0xc2de178a, 4 }, //  9

	{ "1945a.u77",	0x2000, 0x3a3fbfe7, 5 }, // 10 gfx3
	{ "1946a.u78",	0x2000, 0xf2cb1b53, 5 }, // 11
	{ "1947a.u79",	0x2000, 0xd2421e92, 5 }, // 12

	{ "1948a.u91",	0x2000, 0x7a7fcbbe, 6 }, // 13 gfx4
	{ "1953a.u90",	0x2000, 0x6b626ea7, 6 }, // 14
	{ "1949a.u93",	0x2000, 0xe7722d09, 6 }, // 15
	{ "1954a.u92",	0x2000, 0xa970f5ff, 6 }, // 16

	{ "1942a.u98",	0x0100, 0x3a8e6f74, 7 }, // 17 proms
	{ "1941a.u72",	0x0100, 0xa5d0d97e, 7 }, // 18

	{ "1944a.u51",	0x0800, 0x88215098, 2 }, //  3 usb
	{ "1943a.u50",	0x0800, 0x77e5a1f0, 2 }, //  4
};

STD_ROM_PICK(ixion)
STD_ROM_FN(ixion)

static int ixionInit()
{
	int nRet;

	hardware_type = 1;

	nRet = DrvInit();

	if (nRet == 0) {
		szaxxon_decode();
	}

	return DrvInit();
}

struct BurnDriver BurnDrvIxion = {
	"ixion", NULL, NULL, NULL, "1983",
	"Ixion (prototype)\0", NULL, "Sega", "hardware",
	NULL, NULL, NULL, NULL,
	0 | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, ixionRomInfo, ixionRomName, NULL, NULL, ZaxxonInputInfo, ZaxxonDIPInfo, //RazmatazInputInfo, RazmatazDIPInfo,
	ixionInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	224, 256, 3, 4
};

static int CongoInit()
{
	hardware_type = 2;
	futspy_sprite = 1;

	AllMem = NULL;
	MemIndex();
	int nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)malloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		for (int i = 0; i < 4; i++) {
			if (BurnLoadRom(DrvZ80ROM  + i * 0x2000,  0 + i, 1)) return 1;
		}

		for (int i = 0; i < 3; i++) {
			if (BurnLoadRom(DrvGfxROM1 + i * 0x2000,  6 + i, 1)) return 1;
		}

		for (int i = 0; i < 6; i++) {
			if (BurnLoadRom(DrvGfxROM2 + i * 0x2000,  9 + i, 1)) return 1;
		}

		for (int i = 0; i < 2; i++) {
			if (BurnLoadRom(DrvGfxROM3 + i * 0x2000, 15 + i, 1)) return 1;
		}

		if (BurnLoadRom(DrvZ80ROM2,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0,  5, 1)) return 1;
		if (BurnLoadRom(DrvColPROM, 17, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x100, 17, 1)) return 1;

		DrvGfxDecode();
		DrvPaletteInit(0x200);
		bg_layer_init();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapArea(0x0000, 0x7fff, 0, DrvZ80ROM);
	ZetMapArea(0x0000, 0x7fff, 2, DrvZ80ROM);
	ZetMapArea(0x8000, 0x8fff, 0, DrvZ80RAM);
	ZetMapArea(0x8000, 0x8fff, 1, DrvZ80RAM);
	ZetMapArea(0x8000, 0x8fff, 2, DrvZ80RAM);
	ZetMapArea(0xa000, 0xa3ff, 0, DrvVidRAM);
	ZetMapArea(0xa000, 0xa3ff, 1, DrvVidRAM);
	ZetMapArea(0xa400, 0xa7ff, 0, DrvColRAM);
	ZetMapArea(0xa400, 0xa7ff, 1, DrvColRAM);
	ZetSetWriteHandler(congo_write);
	ZetSetReadHandler(congo_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapArea(0x0000, 0x1fff, 0, DrvZ80ROM2);
	ZetMapArea(0x0000, 0x1fff, 2, DrvZ80ROM2);
	ZetMapArea(0x4000, 0x47ff, 0, DrvZ80RAM2);
	ZetMapArea(0x4000, 0x47ff, 1, DrvZ80RAM2);
	ZetMapArea(0x4000, 0x47ff, 2, DrvZ80RAM2);
	ZetSetWriteHandler(congo_sound_write);
	ZetSetReadHandler(congo_sound_read);
	ZetClose();
	GenericTilesInit();

	ppi8255_init(1);
	PPI0PortReadA = CongoPPIReadA;
	PPI0PortWriteA = NULL;
	PPI0PortWriteB = CongoPPIWriteB;
	PPI0PortWriteC = CongoPPIWriteC;

	BurnSampleInit(1);
	BurnSampleSetAllRoutesAllSamples(0.10, BURN_SND_ROUTE_BOTH);

    SN76496Init(0, 4000000, 0);
	SN76496Init(1, 4000000 / 4, 1);

	DrvDoReset();

	return 0;
}


// Congo Bongo

static struct BurnRomInfo congoRomDesc[] = {
	{ "congo1.u35",		0x2000, 0x09355b5b, 1 }, //  0 main
	{ "congo2.u34",		0x2000, 0x1c5e30ae, 1 }, //  1
	{ "congo3.u33",		0x2000, 0x5ee1132c, 1 }, //  2
	{ "congo4.u32",		0x2000, 0x5332b9bf, 1 }, //  3

	{ "congo17.u11",	0x2000, 0x5024e673, 2 }, //  4 audio

	{ "congo5.u76",		0x1000, 0x7bf6ba2b, 3 }, //  5 gfx1

	{ "congo8.u93",		0x2000, 0xdb99a619, 4 }, //  6 gfx2
	{ "congo9.u94",		0x2000, 0x93e2309e, 4 }, //  7
	{ "congo10.u95",	0x2000, 0xf27a9407, 4 }, //  8

	{ "congo12.u78",	0x2000, 0x15e3377a, 5 }, //  9 gfx3
	{ "congo13.u79",	0x2000, 0x1d1321c8, 5 }, // 10
	{ "congo11.u77",	0x2000, 0x73e2709f, 5 }, // 11
	{ "congo14.u104",	0x2000, 0xbf9169fe, 5 }, // 12
	{ "congo16.u106",	0x2000, 0xcb6d5775, 5 }, // 13
	{ "congo15.u105",	0x2000, 0x7b15a7a4, 5 }, // 14

	{ "congo6.u57",		0x2000, 0xd637f02b, 6 }, // 15 gfx4
	{ "congo7.u58",		0x2000, 0x80927943, 6 }, // 16

	{ "congo.u68",		0x0100, 0xb788d8ae, 7 }, // 17 proms
};

STD_ROM_PICK(congo)
STD_ROM_FN(congo)

struct BurnDriver BurnDrvCongo = {
	"congo", NULL, NULL, "congo", "1983",
	"Congo Bongo\0", NULL, "Sega", "hardware",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, congoRomInfo, congoRomName, congoSampleInfo, congoSampleName, CongoBongoInputInfo, CongoBongoDIPInfo, //CongoInputInfo, CongoDIPInfo,
	CongoInit, DrvExit, CongoFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	224, 256, 3, 4
};



// Tip Top

static struct BurnRomInfo tiptopRomDesc[] = {
	{ "tiptop1.u35",	0x2000, 0xe19dc77b, 1 }, //  0 main
	{ "tiptop2.u34",	0x2000, 0x3fcd3b6e, 1 }, //  1
	{ "tiptop3.u33",	0x2000, 0x1c94250b, 1 }, //  2
	{ "tiptop4.u32",	0x2000, 0x577b501b, 1 }, //  3

	{ "congo17.u11",	0x2000, 0x5024e673, 2 }, //  4 audio

	{ "congo5.u76",		0x1000, 0x7bf6ba2b, 3 }, //  5 gfx1

	{ "congo8.u93",		0x2000, 0xdb99a619, 4 }, //  6 gfx2
	{ "congo9.u94",		0x2000, 0x93e2309e, 4 }, //  7
	{ "congo10.u95",	0x2000, 0xf27a9407, 4 }, //  8

	{ "congo12.u78",	0x2000, 0x15e3377a, 5 }, //  9 gfx3
	{ "congo13.u79",	0x2000, 0x1d1321c8, 5 }, // 10
	{ "congo11.u77",	0x2000, 0x73e2709f, 5 }, // 11
	{ "congo14.u104",	0x2000, 0xbf9169fe, 5 }, // 12
	{ "congo16.u106",	0x2000, 0xcb6d5775, 5 }, // 13
	{ "congo15.u105",	0x2000, 0x7b15a7a4, 5 }, // 14

	{ "congo6.u57",		0x2000, 0xd637f02b, 6 }, // 15 gfx4
	{ "congo7.u58",		0x2000, 0x80927943, 6 }, // 16

	{ "congo.u68",		0x0100, 0xb788d8ae, 7 }, // 17 proms
};

STD_ROM_PICK(tiptop)
STD_ROM_FN(tiptop)

struct BurnDriver BurnDrvTiptop = {
	"tiptop", "congo", NULL, "congo", "1983",
	"Tip Top\0", NULL, "Sega", "hardware",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, tiptopRomInfo, tiptopRomName, congoSampleInfo, congoSampleName, ZaxxonInputInfo, ZaxxonDIPInfo, //CongoInputInfo, CongoDIPInfo,
	CongoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	224, 256, 3, 4
};
