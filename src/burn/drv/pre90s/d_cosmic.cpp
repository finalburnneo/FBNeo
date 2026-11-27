// FB Neo Universal "Cosmic" hardware driver module
// Based on MAME driver by Nicola Salmoria, Lee Taylor

#include "tiles_generic.h"
#include "z80_intf.h"
#include "samples.h"
#include "dac.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM;		// raw sprite roms
static UINT8 *DrvGfx16;			// decoded 16x16
static UINT8 *DrvGfx32;			// decoded 32x32
static UINT8 *DrvColPROM;
static UINT8 *DrvColMap;		// color map
static UINT8 *DrvUser2;			// starfield / grid horizontal / tree+river
static UINT8 *DrvUser3;			// grid vertical
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT16 *DrvBitmap;		// 256x256 bitmap

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

enum { HW_PANIC = 0, HW_COSMICA, HW_MAGSPOT, HW_DEVZONE, HW_NOMNLND };
static INT32 hardware_type;
static INT32 has_samples;
static INT32 gfx_len;
static INT32 cpu_clock;
static INT32 cycles_per_frame;
static INT32 nExtraCycles;

static UINT8 color_registers[3];
static UINT8 flipscreen;
static UINT8 background_enable;
static UINT8 sound_enabled;
static UINT8 dive_bomb_b_select;
static UINT32 frame_number;
static INT32 scanline;
static UINT8 coin_last;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvJoy5[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[5];
static UINT8 DrvReset;

static struct BurnInputInfo PanicInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Panic)

static struct BurnInputInfo CosmicaInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy5 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 7,	"p1 start"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 start"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Cosmica)

static struct BurnInputInfo MagspotInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy5 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 7,	"p1 start"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy4 + 7,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy5 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 start"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy4 + 6,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Magspot)

static struct BurnInputInfo NomnlndInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy5 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 7,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy4 + 7,	"p1 fire 1"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy4 + 6,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Nomnlnd)

static struct BurnDIPInfo PanicDIPList[]=
{
	DIP_OFFSET(0x11)
	{0x00, 0xff, 0xff, 0x40, NULL					},

	{0   , 0xfe, 0   ,    6, "Coin A"				},
	{0x00, 0x01, 0x07, 0x05, "2 Coins 3 Credits"	},
	{0x00, 0x01, 0x07, 0x00, "1 Coin  1 Credit"		},
	{0x00, 0x01, 0x07, 0x01, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0x07, 0x02, "1 Coin  3 Credits"	},
	{0x00, 0x01, 0x07, 0x03, "1 Coin  4 Credits"	},
	{0x00, 0x01, 0x07, 0x04, "1 Coin  5 Credits"	},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x00, 0x01, 0x08, 0x00, "Upright"				},
	{0x00, 0x01, 0x08, 0x08, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Bonus Life"			},
	{0x00, 0x01, 0x10, 0x00, "3000"					},
	{0x00, 0x01, 0x10, 0x10, "5000"					},

	{0   , 0xfe, 0   ,    2, "Lives"				},
	{0x00, 0x01, 0x20, 0x00, "3"					},
	{0x00, 0x01, 0x20, 0x20, "4"					},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x00, 0x01, 0xc0, 0x00, "2 Coins 1 Credit"		},
	{0x00, 0x01, 0xc0, 0x40, "1 Coin  1 Credit"		},
	{0x00, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0xc0, 0xc0, "1 Coin  3 Credits"	},
};

STDDIPINFO(Panic)

static struct BurnDIPInfo CosmicaDIPList[]=
{
	DIP_OFFSET(0x0a)
	{0x00, 0xff, 0xff, 0x3e, NULL					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x00, 0x01, 0x01, 0x00, "Upright"				},
	{0x00, 0x01, 0x01, 0x01, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Lives"				},
	{0x00, 0x01, 0x02, 0x02, "3"					},
	{0x00, 0x01, 0x02, 0x00, "5"					},

	{0   , 0xfe, 0   ,    3, "Coinage"				},
	{0x00, 0x01, 0x0c, 0x08, "2 Coins 1 Credit"		},
	{0x00, 0x01, 0x0c, 0x0c, "1 Coin  1 Credit"		},
	{0x00, 0x01, 0x0c, 0x04, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x00, 0x01, 0x30, 0x30, "5000"					},
	{0x00, 0x01, 0x30, 0x20, "10000"				},
	{0x00, 0x01, 0x30, 0x10, "15000"				},
	{0x00, 0x01, 0x30, 0x00, "None"					},
};

STDDIPINFO(Cosmica)

#define COSMIC_COINAGE_DIPS(offs)					\
	{0   , 0xfe, 0   ,   16, "Coin A"				},	\
	{offs, 0x01, 0x0f, 0x0c, "4 Coins 1 Credit"		},	\
	{offs, 0x01, 0x0f, 0x08, "3 Coins 1 Credit"		},	\
	{offs, 0x01, 0x0f, 0x0d, "4 Coins 2 Credits"	},	\
	{offs, 0x01, 0x0f, 0x05, "2 Coins 1 Credit"		},	\
	{offs, 0x01, 0x0f, 0x09, "3 Coins 2 Credits"	},	\
	{offs, 0x01, 0x0f, 0x0e, "4 Coins 3 Credits"	},	\
	{offs, 0x01, 0x0f, 0x0f, "4 Coins 4 Credits"	},	\
	{offs, 0x01, 0x0f, 0x0a, "3 Coins 3 Credits"	},	\
	{offs, 0x01, 0x0f, 0x06, "2 Coins 2 Credits"	},	\
	{offs, 0x01, 0x0f, 0x00, "1 Coin  1 Credit"		},	\
	{offs, 0x01, 0x0f, 0x0b, "3 Coins 4 Credits"	},	\
	{offs, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},	\
	{offs, 0x01, 0x0f, 0x01, "1 Coin  2 Credits"	},	\
	{offs, 0x01, 0x0f, 0x02, "1 Coin  3 Credits"	},	\
	{offs, 0x01, 0x0f, 0x03, "1 Coin  4 Credits"	},	\
	{offs, 0x01, 0x0f, 0x04, "1 Coin  5 Credits"	},	\
															\
	{0   , 0xfe, 0   ,   16, "Coin B"				},	\
	{offs, 0x01, 0xf0, 0xc0, "4 Coins 1 Credit"		},	\
	{offs, 0x01, 0xf0, 0x80, "3 Coins 1 Credit"		},	\
	{offs, 0x01, 0xf0, 0xd0, "4 Coins 2 Credits"	},	\
	{offs, 0x01, 0xf0, 0x50, "2 Coins 1 Credit"		},	\
	{offs, 0x01, 0xf0, 0x90, "3 Coins 2 Credits"	},	\
	{offs, 0x01, 0xf0, 0xe0, "4 Coins 3 Credits"	},	\
	{offs, 0x01, 0xf0, 0xf0, "4 Coins 4 Credits"	},	\
	{offs, 0x01, 0xf0, 0xa0, "3 Coins 3 Credits"	},	\
	{offs, 0x01, 0xf0, 0x60, "2 Coins 2 Credits"	},	\
	{offs, 0x01, 0xf0, 0x00, "1 Coin  1 Credit"		},	\
	{offs, 0x01, 0xf0, 0xb0, "3 Coins 4 Credits"	},	\
	{offs, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"	},	\
	{offs, 0x01, 0xf0, 0x10, "1 Coin  2 Credits"	},	\
	{offs, 0x01, 0xf0, 0x20, "1 Coin  3 Credits"	},	\
	{offs, 0x01, 0xf0, 0x30, "1 Coin  4 Credits"	},	\
	{offs, 0x01, 0xf0, 0x40, "1 Coin  5 Credits"	},

static struct BurnDIPInfo MagspotDIPList[]=
{
	DIP_OFFSET(0x0b)
	{0x00, 0xff, 0xff, 0x40, NULL					},
	{0x01, 0xff, 0xff, 0x09, NULL					},
	{0x02, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    4, "Bonus Game"			},
	{0x00, 0x01, 0xc0, 0x40, "5000"					},
	{0x00, 0x01, 0xc0, 0x80, "10000"				},
	{0x00, 0x01, 0xc0, 0xc0, "15000"				},
	{0x00, 0x01, 0xc0, 0x00, "None"					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x01, 0x01, 0x03, 0x01, "2000"					},
	{0x01, 0x01, 0x03, 0x02, "3000"					},
	{0x01, 0x01, 0x03, 0x03, "5000"					},
	{0x01, 0x01, 0x03, 0x00, "None"					},

	{0   , 0xfe, 0   ,    2, "Unknown (SWA:4)"		},
	{0x01, 0x01, 0x04, 0x00, "Off"					},
	{0x01, 0x01, 0x04, 0x04, "On"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x01, 0x01, 0x18, 0x00, "2"					},
	{0x01, 0x01, 0x18, 0x08, "3"					},
	{0x01, 0x01, 0x18, 0x10, "4"					},
	{0x01, 0x01, 0x18, 0x18, "5"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x01, 0x01, 0x20, 0x00, "Upright"				},
	{0x01, 0x01, 0x20, 0x20, "Cocktail"				},

	COSMIC_COINAGE_DIPS(0x02)
};

STDDIPINFO(Magspot)

static struct BurnDIPInfo DevzoneDIPList[]=
{
	DIP_OFFSET(0x0b)
	{0x00, 0xff, 0xff, 0xc0, NULL					},
	{0x01, 0xff, 0xff, 0x1d, NULL					},
	{0x02, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x01, 0x01, 0x03, 0x01, "4000"					},
	{0x01, 0x01, 0x03, 0x02, "6000"					},
	{0x01, 0x01, 0x03, 0x03, "8000"					},
	{0x01, 0x01, 0x03, 0x00, "None"					},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x01, 0x01, 0x0c, 0x0c, "Use Coin A & B"		},
	{0x01, 0x01, 0x0c, 0x04, "2 Coins 1 Credit"		},
	{0x01, 0x01, 0x0c, 0x00, "1 Coin  1 Credit"		},
	{0x01, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Lives"				},
	{0x01, 0x01, 0x10, 0x00, "2"					},
	{0x01, 0x01, 0x10, 0x10, "3"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x01, 0x01, 0x20, 0x00, "Upright"				},
	{0x01, 0x01, 0x20, 0x20, "Cocktail"				},

	COSMIC_COINAGE_DIPS(0x02)
};

STDDIPINFO(Devzone)

static struct BurnDIPInfo Devzone2DIPList[]=
{
	DIP_OFFSET(0x0b)
	{0x00, 0xff, 0xff, 0xc0, NULL					},
	{0x01, 0xff, 0xff, 0x1d, NULL					},
	{0x02, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x01, 0x01, 0x03, 0x01, "2000"					},
	{0x01, 0x01, 0x03, 0x02, "3000"					},
	{0x01, 0x01, 0x03, 0x03, "5000"					},
	{0x01, 0x01, 0x03, 0x00, "None"					},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x01, 0x01, 0x0c, 0x0c, "Use Coin A & B"		},
	{0x01, 0x01, 0x0c, 0x04, "2 Coins 1 Credit"		},
	{0x01, 0x01, 0x0c, 0x00, "1 Coin  1 Credit"		},
	{0x01, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Lives"				},
	{0x01, 0x01, 0x10, 0x00, "2"					},
	{0x01, 0x01, 0x10, 0x10, "3"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x01, 0x01, 0x20, 0x00, "Upright"				},
	{0x01, 0x01, 0x20, 0x20, "Cocktail"				},

	COSMIC_COINAGE_DIPS(0x02)
};

STDDIPINFO(Devzone2)

static struct BurnDIPInfo NomnlndDIPList[]=
{
	DIP_OFFSET(0x0e)
	{0x00, 0xff, 0xff, 0x02, NULL					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x00, 0x01, 0x03, 0x01, "2000"					},
	{0x00, 0x01, 0x03, 0x02, "3000"					},
	{0x00, 0x01, 0x03, 0x03, "5000"					},
	{0x00, 0x01, 0x03, 0x00, "None"					},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x00, 0x01, 0x0c, 0x04, "2 Coins 1 Credit"		},
	{0x00, 0x01, 0x0c, 0x0c, "4 Coins 4 Credits"	},
	{0x00, 0x01, 0x0c, 0x00, "1 Coin  1 Credit"		},
	{0x00, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Lives"				},
	{0x00, 0x01, 0x10, 0x00, "3"					},
	{0x00, 0x01, 0x10, 0x10, "5"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x00, 0x01, 0x20, 0x00, "Upright"				},
	{0x00, 0x01, 0x20, 0x20, "Cocktail"				},
};

STDDIPINFO(Nomnlnd)

static struct BurnDIPInfo NomnlndgDIPList[]=
{
	DIP_OFFSET(0x0e)
	{0x00, 0xff, 0xff, 0x12, NULL					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x00, 0x01, 0x03, 0x01, "3000"					},
	{0x00, 0x01, 0x03, 0x02, "5000"					},
	{0x00, 0x01, 0x03, 0x03, "8000"					},
	{0x00, 0x01, 0x03, 0x00, "None"					},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x00, 0x01, 0x0c, 0x04, "2 Coins 1 Credit"		},
	{0x00, 0x01, 0x0c, 0x0c, "4 Coins 4 Credits"	},
	{0x00, 0x01, 0x0c, 0x00, "1 Coin  1 Credit"		},
	{0x00, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Lives"				},
	{0x00, 0x01, 0x10, 0x00, "2"					},
	{0x00, 0x01, 0x10, 0x10, "3"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x00, 0x01, 0x20, 0x00, "Upright"				},
	{0x00, 0x01, 0x20, 0x20, "Cocktail"				},
};

STDDIPINFO(Nomnlndg)

// --- Sound ---

#define SAMPLE_VOL	0.25

static void dac_write(UINT8 data)
{
	DACWrite(0, (data & 0x80) ? 0xff : 0x00);
}

// Schematics show 12 triggers for discrete sound circuits
static void panic_sound_output_w(INT32 offset, UINT8 data)
{
	if (offset == 11)
	{
		if (data == 0)
			for (INT32 i = 0; i < 9; i++)
				sstopch(i);

		sound_enabled = data;
	}

	if (sound_enabled)
	{
		switch (offset)
		{
			case 0: if (data) splaych(0, 0, SAMPLE_VOL); break;		// Walk
			case 1: if (data) splaych(0, 5, SAMPLE_VOL); break;		// Enemy Die 1
			case 2:												// Drop 1
				if (data) {
					if (!splayingch(1)) {
						sstopch(2);
						splaych(1, 3, SAMPLE_VOL);
					}
				} else {
					sstopch(1);
				}
			break;

			case 3:												// Oxygen
				if (data && !splayingch(6))
					splaych(6, 9, SAMPLE_VOL, true, true);
			break;

			case 4: break;										// Drop 2
			case 5: if (data) splaych(0, 5, SAMPLE_VOL); break;		// Enemy Die 2 (use same sample as 1)
			case 6:												// Hang
				if (data && !splayingch(1) && !splayingch(3))
					splaych(2, 2, SAMPLE_VOL);
			break;

			case 7:												// Escape
				if (data) {
					sstopch(2);
					splaych(3, 4, SAMPLE_VOL);
				} else {
					sstopch(3);
				}
			break;

			case 8: if (data) splaych(0, 1, SAMPLE_VOL); break;		// Stairs
			case 9:												// Extend
				if (data)
					splaych(4, 8, SAMPLE_VOL);
				else
					sstopch(4);
			break;

			case 10: dac_write(data); break;					// Bonus
		}
	}
}

static void panic_sound_output2_w(INT32 offset, UINT8 data)
{
	if (sound_enabled)
	{
		switch (offset)
		{
			case 0: if (data) splaych(0, 6, SAMPLE_VOL); break;		// Player Die
			case 1: if (data) splaych(5, 7, SAMPLE_VOL); break;		// Enemy Laugh
		}
	}
}

static void cosmica_sound_output_w(INT32 offset, UINT8 data)
{
	if (offset == 11)
	{
		if (data == 0) {
			for (INT32 i = 0; i < 12; i++)
				sstopch(i);
		} else {
			splaych(0, 0, SAMPLE_VOL, true, true);	// Background Noise
		}

		sound_enabled = data;
	}

	if (sound_enabled)
	{
		switch (offset)
		{
			case 0: if (data) splaych(1, 2, SAMPLE_VOL); break;		// Dive Bombing Type A

			case 1: break;										// game writes 0 when alien shot

			case 2:												// Dive Bombing Type B (Main Control)
				if (data && dive_bomb_b_select >= 2 && dive_bomb_b_select <= 7) {
					INT32 ch = dive_bomb_b_select;
					sstopch(ch);
					splaych(ch, ch + 1, SAMPLE_VOL);
				}
			break;

			case 3: if (data) dive_bomb_b_select |= 0x04; else dive_bomb_b_select &= 0xfb; break;	// G.S.B
			case 4: if (data) dive_bomb_b_select |= 0x02; else dive_bomb_b_select &= 0xfd; break;	// M.S.B
			case 5: if (data) dive_bomb_b_select |= 0x01; else dive_bomb_b_select &= 0xfe; break;	// L.S.B

			case 6: if (data) splaych(8, 9, SAMPLE_VOL); break;		// Fire Control
			case 7: if (data) splaych(9, 10, SAMPLE_VOL); break;	// Small Explosion
			case 8: if (data) splaych(10, 11, SAMPLE_VOL); break;	// Loud Explosion

			case 9:												// Extend Sound control
				if (data)
					splaych(11, 1, SAMPLE_VOL, true, true);
				else
					sstopch(11);
			break;
		}
	}
}

static inline INT32 vblank()
{
	return (scanline < 32 || scanline >= 224);
}

static void __fastcall panic_write(UINT16 address, UINT8 data)
{
	if ((address & 0xffe0) == 0x6000) {
		DrvSprRAM[address & 0x1f] = data;
		return;
	}

	if (address >= 0x7000 && address <= 0x700b) {
		panic_sound_output_w(address & 0x0f, data);
		return;
	}

	if (address >= 0x700c && address <= 0x700e) {
		color_registers[address - 0x700c] = data ? 1 : 0;
		return;
	}

	if (address == 0x700f) {
		flipscreen = data & 0x80;
		return;
	}

	if (address == 0x7800 || address == 0x7801) {
		panic_sound_output2_w(address & 1, data);
		return;
	}
}

static UINT8 __fastcall panic_read(UINT16 address)
{
	switch (address)
	{
		case 0x6800: return DrvInputs[0];
		case 0x6801: return DrvInputs[1];
		case 0x6802: return DrvDips[0];
		case 0x6803: return DrvInputs[2];
	}

	return 0;
}

static void __fastcall cosmica_write(UINT16 address, UINT8 data)
{
	if ((address & 0xffe0) == 0x6000) {
		DrvSprRAM[address & 0x1f] = data;
		return;
	}

	if (address >= 0x7000 && address <= 0x700b) {
		cosmica_sound_output_w(address & 0x0f, data);
		return;
	}

	if (address == 0x700c || address == 0x700d) {
		color_registers[address - 0x700c] = data ? 1 : 0;
		return;
	}

	if (address == 0x700f) {
		flipscreen = data & 0x80;
		return;
	}
}

static UINT8 __fastcall cosmica_read(UINT16 address)
{
	switch (address)
	{
		case 0x6800: return DrvInputs[0];
		case 0x6801: return DrvInputs[1];
		case 0x6802: return DrvInputs[2];
		case 0x6803: return (scanline >> 2) & 0x3f;		// pixel clock (vpos)
	}

	return 0;
}

static void __fastcall magspot_write(UINT16 address, UINT8 data)
{
	if ((address & 0xffe0) == 0x4000) {
		DrvSprRAM[address & 0x1f] = data;
		return;
	}

	switch (address)
	{
		case 0x4800:
			if (hardware_type != HW_NOMNLND) dac_write(data);
		return;

		case 0x4807:
			if (hardware_type == HW_DEVZONE || hardware_type == HW_NOMNLND) background_enable = data;
		return;

		case 0x480a:
			if (hardware_type == HW_NOMNLND) dac_write(data);
		return;

		case 0x480c:
		case 0x480d:
			color_registers[address - 0x480c] = data ? 1 : 0;
		return;

		case 0x480f:
			flipscreen = data & 0x80;
		return;
	}
}

static UINT8 nomnlnd_port_0_1_r(INT32 offset)
{
	UINT8 control = DrvInputs[offset];
	UINT8 fire = DrvInputs[3];

	// If firing - stop tank
	if ((fire & 0xc0) == 0) return 0xff;

	// set bit according to 8 way direction
	if ((control & 0x82) == 0) return 0xfe;		// Up & Left
	if ((control & 0x0a) == 0) return 0xfb;		// Down & Left
	if ((control & 0x28) == 0) return 0xef;		// Down & Right
	if ((control & 0xa0) == 0) return 0xbf;		// Up & Right

	return control;
}

static UINT8 __fastcall magspot_read(UINT16 address)
{
	if (address >= 0x3800 && address <= 0x3807) {
		// coinage dips, one bit per address
		return (DrvInputs[4] & (1 << (7 - (address & 7)))) ? 0 : 1;
	}

	switch (address)
	{
		case 0x5000:
		case 0x5001:
			if (hardware_type == HW_NOMNLND) return nomnlnd_port_0_1_r(address & 1);
			return DrvInputs[address & 1];

		case 0x5002:
			return DrvInputs[2];

		case 0x5003:
		{
			UINT8 ret = DrvInputs[3] & 0xfe;
			if (hardware_type == HW_DEVZONE)
				ret |= vblank() ? 0 : 1;	// active low
			else
				ret |= vblank() ? 1 : 0;	// active high
			return ret;
		}
	}

	return 0;
}

static UINT8 map_color(UINT8 x, UINT8 y)
{
	INT32 offs;
	UINT8 pen;

	switch (hardware_type)
	{
		case HW_PANIC:
			offs = (color_registers[0] << 9) | (color_registers[2] << 10) | ((x >> 4) << 5) | (y >> 3);
			pen = DrvColMap[offs];
			if (color_registers[1]) pen >>= 4;
			return pen & 0x0f;

		case HW_COSMICA:
			offs = (color_registers[0] << 9) | ((x >> 4) << 5) | (y >> 3);
			pen = DrvColMap[offs & 0x3ff];
			if (color_registers[1]) pen >>= 4;
			return pen & 0x07;

		default: // magspot, devzone, nomnlnd
			offs = (color_registers[0] << 9) | ((x >> 3) << 4) | (y >> 4);
			pen = DrvColMap[offs & 0x3ff];
			if (color_registers[1]) pen >>= 4;
			return pen & 0x0f;
	}
}

static void DrvPaletteInit()
{
	UINT32 tab[0x10];

	switch (hardware_type)
	{
		case HW_PANIC:
		{
			// bit 3 can pull Blue via a 2k resistor to 5v (1k to ground), so blue = 2/3
			for (INT32 i = 0; i < 0x10; i++) {
				INT32 r = (i & 1) ? 0xff : 0;
				INT32 g = (i & 2) ? 0xff : 0;
				INT32 b = ((i & 0x0c) == 0x08) ? 0xaa : ((i & 4) ? 0xff : 0);
				tab[i] = BurnHighCol(r, g, b, 0);
			}

			for (INT32 i = 0; i < 0x10; i++) DrvPalette[i] = tab[i]; // pen 15 is white on hw
			for (INT32 i = 0; i < 0x20; i++) DrvPalette[0x10 + i] = tab[DrvColPROM[i] & 0x07];
		}
		break;

		case HW_COSMICA:
		{
			for (INT32 i = 0; i < 0x08; i++) {
				tab[i] = BurnHighCol((i & 1) ? 0xff : 0, (i & 2) ? 0xff : 0, (i & 4) ? 0xff : 0, 0);
			}

			for (INT32 i = 0; i < 0x08; i++) DrvPalette[i] = tab[i];
			for (INT32 i = 0; i < 0x20; i++) {
				DrvPalette[0x08 + i] = tab[(DrvColPROM[i] >> 0) & 0x07];
				DrvPalette[0x28 + i] = tab[(DrvColPROM[i] >> 4) & 0x07];
			}
		}
		break;

		default: // magspot palette (also used by devzone & nomnlnd)
		{
			for (INT32 i = 0; i < 0x10; i++) {
				INT32 r = ((i & 0x09) == 0x08) ? 0xaa : ((i & 1) ? 0xff : 0);
				INT32 g = (i & 2) ? 0xff : 0;
				INT32 b = (i & 4) ? 0xff : 0;
				tab[i] = BurnHighCol(r, g, b, 0);
			}

			for (INT32 i = 0; i < 0x10; i++) DrvPalette[i] = tab[i];
			for (INT32 i = 0; i < 0x20; i++) DrvPalette[0x10 + i] = tab[DrvColPROM[i] & 0x0f];
		}
		break;
	}
}

static inline void pset(INT32 x, INT32 y, UINT16 pen)
{
	if (flipscreen)
		DrvBitmap[(255 - y) * 256 + (255 - x)] = pen;
	else
		DrvBitmap[y * 256 + x] = pen;
}

static void draw_bitmap()
{
	for (INT32 offs = 0; offs < 0x2000; offs++)
	{
		UINT8 data = DrvVidRAM[offs];
		if (data == 0) continue;

		UINT8 x = offs << 3;
		UINT8 y = offs >> 5;

		UINT8 pen = map_color(x, y);

		for (INT32 i = 0; i < 8; i++)
		{
			if (data & 0x80) pset(x, y, pen);

			x++;
			data <<= 1;
		}
	}
}

static void draw_one_sprite(UINT8 *gfx, INT32 size, INT32 code, INT32 pen_base, INT32 flipy, INT32 sx, INT32 sy)
{
	UINT8 *src = gfx + code * size * size;

	for (INT32 y = 0; y < size; y++)
	{
		INT32 dy = sy + y;
		if (dy < 32 || dy >= 224) continue;

		UINT8 *row = src + (flipy ? (size - 1 - y) : y) * size;
		UINT16 *dst = DrvBitmap + dy * 256;

		for (INT32 x = 0; x < size; x++)
		{
			INT32 dx = sx + x;
			if (dx < 0 || dx >= 256) continue;

			if (row[x]) dst[dx] = pen_base + row[x];
		}
	}
}

static void draw_sprites(INT32 color_mask, INT32 extra_sprites, INT32 color_base)
{
	INT32 count16 = (gfx_len / 2) / 32;
	INT32 count32 = (gfx_len / 2) / 128;

	for (INT32 offs = 0x20 - 4; offs >= 0; offs -= 4)
	{
		if (DrvSprRAM[offs] == 0) continue;

		INT32 code  = ~DrvSprRAM[offs] & 0x3f;
		INT32 color = ~DrvSprRAM[offs + 3] & color_mask;

		if (extra_sprites)
			code |= (DrvSprRAM[offs + 3] & 0x08) << 3;

		INT32 flipy = ~DrvSprRAM[offs] & 0x40;
		INT32 sx = 256 - DrvSprRAM[offs + 2];
		INT32 sy = DrvSprRAM[offs + 1];
		INT32 pen_base = color_base + color * 4;

		if (DrvSprRAM[offs] & 0x80) {
			// 16x16 sprite
			draw_one_sprite(DrvGfx16, 16, code % count16, pen_base, flipy, sx, sy);
		} else {
			// 32x32 sprite
			draw_one_sprite(DrvGfx32, 32, (code >> 2) % count32, pen_base, flipy, sx, sy);
		}
	}
}

static void cosmica_draw_starfield()
{
	UINT8 y = 0;
	UINT8 map = 0;

	while (1)
	{
		INT32 va =  y       & 0x01;
		INT32 vb = (y >> 1) & 0x01;

		UINT8 x = 0;

		while (1)
		{
			UINT8 x1;

			if (flipscreen)
				x1 = x - frame_number;
			else
				x1 = x + frame_number;

			INT32 hc  = (x1 >> 2) & 0x01;
			INT32 hb_ = (x  >> 5) & 0x01;  // not a bug, this one is the real x

			if ((x1 & 0x1f) == 0)
				// flip-flop at IC11 is clocked
				map = DrvUser2[(x1 >> 5) | (y >> 1 << 3)];

			if (((!(hc & va)) & (vb ^ hb_)) &&			// right network
				(((x1 ^ map) & (hc | 0x1e)) == 0x1e))	// left network
			{
				// RGB order is reversed -- bit 7=R, 6=G, 5=B
				INT32 col = (map >> 7) | ((map >> 5) & 0x02) | ((map >> 3) & 0x04);

				DrvBitmap[y * 256 + x] = col;
			}

			x++;
			if (x == 0) break;
		}

		y++;
		if (y == 0) break;
	}
}

static void devzone_draw_grid()
{
	INT32 horz_addr = 0;

	UINT8 count = 0;
	UINT8 horz_data = 0;
	UINT8 vert_data;

	for (INT32 y = 32; y < 224; y++)
	{
		UINT8 x = 0;

		while (1)
		{
			// for the vertical lines, each bit indicates
			// if there should be a line at the x position
			vert_data = DrvUser3[x >> 3];

			// the horizontal (perspective) lines are RLE encoded.
			// When the screen is flipped, the address should be
			// decrementing.  But since it's just a mirrored image,
			// this is easier.
			if (count == 0)
				count = DrvUser2[(horz_addr++) & 0x7ff];

			count++;

			if (count == 0)
				horz_data = DrvUser2[(horz_addr++) & 0x7ff];

			for (INT32 x1 = 0; x1 < 8; x1++)
			{
				if (!(vert_data & horz_data & 0x80))	// NAND gate
				{
					pset(x, y, 4);	// blue
				}

				horz_data = (horz_data << 1) | 0x01;
				vert_data = (vert_data << 1) | 0x01;

				x++;
			}

			if (x == 0) break;
		}
	}
}

static void nomnlnd_draw_background()
{
	UINT8 y = 0;
	UINT8 water = frame_number;

	while (1)
	{
		INT32 vb_ = (y >> 5) & 0x01;
		INT32 vc_ = (y >> 6) & 0x01;
		INT32 vd_ =  y >> 7;

		UINT8 x = 0;

		while (1)
		{
			INT32 color = 0;

			INT32 hd  = (x >> 3) & 0x01;
			INT32 ha_ = (x >> 4) & 0x01;
			INT32 hb_ = (x >> 5) & 0x01;
			INT32 hc_ = (x >> 6) & 0x01;
			INT32 hd_ =  x >> 7;

			if (((!vb_) & vc_ & (!vd_)) ^ (vb_ & (!vc_) & vd_))
			{
				// tree
				if (!hd_ && hc_ && !hb_)
				{
					INT32 offs = ((x >> 3) & 0x03) | ((y & 0x1f) << 2) | (flipscreen ? 0x80 : 0);

					UINT8 plane1 = DrvUser2[offs         ] << (x & 0x07);
					UINT8 plane2 = DrvUser2[offs | 0x0400] << (x & 0x07);

					plane1 >>= 7;
					plane2 >>= 7;

					color = (plane1 & plane2)       |	// R
							(plane2         )  << 1 |	// G
							(plane1 & ~plane2) << 2;	// B
				}
			}
			else
			{
				// water
				if (hd_ && !hc_ && hb_ && !ha_)
				{
					INT32 offs = hd | (water << 1) | 0x0200;

					UINT8 plane1 = DrvUser2[offs         ] << (x & 0x07);
					UINT8 plane2 = DrvUser2[offs | 0x0400] << (x & 0x07);

					plane1 >>= 7;
					plane2 >>= 7;

					color = ( plane1 & plane2)      |	// R
							( plane1 | plane2) << 1 |	// G
							((!plane1) & hd)     << 2;	// B
				}
			}

			if (color != 0) pset(x, y, color);

			x++;
			if (x == 0) break;
		}

		water++;

		y++;
		if (y == 0) break;
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	memset(DrvBitmap, 0, 256 * 256 * sizeof(UINT16));

	switch (hardware_type)
	{
		case HW_PANIC:
			if (nBurnLayer & 1) draw_bitmap();
			if (nSpriteEnable & 1) draw_sprites(0x07, 1, 0x10);
		break;

		case HW_COSMICA:
			if (nBurnLayer & 2) cosmica_draw_starfield();
			if (nBurnLayer & 1) draw_bitmap();
			if (nSpriteEnable & 1) draw_sprites(0x0f, 0, 0x08);
		break;

		case HW_MAGSPOT:
			if (nBurnLayer & 1) draw_bitmap();
			if (nSpriteEnable & 1) draw_sprites(0x07, 0, 0x10);
		break;

		case HW_DEVZONE:
			if (background_enable && (nBurnLayer & 2)) devzone_draw_grid();
			if (nBurnLayer & 1) draw_bitmap();
			if (nSpriteEnable & 1) draw_sprites(0x07, 0, 0x10);
		break;

		case HW_NOMNLND:
			// according to the video summation logic on pg4, the trees and river have the highest priority
			if (nBurnLayer & 1) draw_bitmap();
			if (nSpriteEnable & 1) draw_sprites(0x07, 0, 0x10);
			if (background_enable && (nBurnLayer & 2)) nomnlnd_draw_background();
		break;
	}

	// visible area is lines 32 - 223
	for (INT32 y = 0; y < nScreenHeight; y++) {
		memcpy(pTransDraw + y * nScreenWidth, DrvBitmap + (y + 32) * 256, nScreenWidth * sizeof(UINT16));
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	ZetOpen(0);
	ZetReset();
	ZetSetVector(0xff);
	ZetClose();

	if (has_samples) BurnSampleReset();
	if (hardware_type != HW_COSMICA) DACReset();

	color_registers[0] = color_registers[1] = color_registers[2] = 0;
	flipscreen = 0;
	background_enable = 0;
	sound_enabled = (hardware_type == HW_PANIC || hardware_type == HW_COSMICA) ? 1 : 0;
	dive_bomb_b_select = 0;
	coin_last = 0;
	scanline = 0;

	nExtraCycles = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x010000;

	DrvGfxROM		= Next; Next += 0x002000;
	DrvGfx16		= Next; Next += 0x008000;
	DrvGfx32		= Next; Next += 0x008000;

	DrvColPROM		= Next; Next += 0x000020;
	DrvColMap		= Next; Next += 0x000800;
	DrvUser2		= Next; Next += 0x000800;
	DrvUser3		= Next; Next += 0x000020;

	DrvPalette		= (UINT32*)Next; Next += 0x0048 * sizeof(UINT32);

	DrvBitmap		= (UINT16*)Next; Next += 256 * 256 * sizeof(UINT16);

	AllRam			= Next;

	DrvVidRAM		= Next; Next += 0x002000;
	DrvSprRAM		= Next; Next += 0x000020;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvLoadRoms()
{
	char* pRomName;
	struct BurnRomInfo ri;

	UINT8 *Load[7] = { NULL, DrvZ80ROM, DrvGfxROM, DrvColPROM, DrvColMap, DrvUser2, DrvUser3 };

	gfx_len = 0;

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++)
	{
		BurnDrvGetRomInfo(&ri, i);

		INT32 type = ri.nType & 7;
		if (type < 1 || type > 6) continue;

		if (BurnLoadRom(Load[type], i, 1)) return 1;
		Load[type] += ri.nLen;

		if (type == 2) gfx_len += ri.nLen;
	}

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 half = gfx_len / 2;

	INT32 Plane[2]   = { 0, half * 8 };
	INT32 XOffs16[16] = { STEP8(0, 1), STEP8(16*8, 1) };
	INT32 YOffs16[16] = { STEP16(0, 8) };
	INT32 XOffs32[32] = { STEP8(0, 1), STEP8(32*8, 1), STEP8(64*8, 1), STEP8(96*8, 1) };
	INT32 YOffs32[32] = { STEP32(0, 8) };

	GfxDecode(half / 32,  2, 16, 16, Plane, XOffs16, YOffs16, 32*8,  DrvGfxROM, DrvGfx16);
	GfxDecode(half / 128, 2, 32, 32, Plane, XOffs32, YOffs32, 128*8, DrvGfxROM, DrvGfx32);

	return 0;
}

static INT32 DrvInit(INT32 type)
{
	hardware_type = type;
	has_samples = (type == HW_PANIC || type == HW_COSMICA);

	BurnAllocMemIndex();

	if (DrvLoadRoms()) return 1;
	DrvGfxDecode();

	cpu_clock = (type == HW_PANIC || type == HW_COSMICA) ? (10816000 / 6) : (10816000 / 4);
	cycles_per_frame = (INT32)(((double)cpu_clock * 352.0 * 262.0) / 5408000.0);

	ZetInit(0);
	ZetOpen(0);

	switch (type)
	{
		case HW_PANIC:
			ZetMapMemory(DrvZ80ROM,	0x0000, 0x3fff, MAP_ROM);
			ZetMapMemory(DrvVidRAM,	0x4000, 0x5fff, MAP_RAM);
			ZetSetWriteHandler(panic_write);
			ZetSetReadHandler(panic_read);
		break;

		case HW_COSMICA:
			ZetMapMemory(DrvZ80ROM,	0x0000, 0x3fff, MAP_ROM);
			ZetMapMemory(DrvVidRAM,	0x4000, 0x5fff, MAP_RAM);
			ZetSetWriteHandler(cosmica_write);
			ZetSetReadHandler(cosmica_read);
		break;

		default:
			ZetMapMemory(DrvZ80ROM,	0x0000, 0x2fff, MAP_ROM);
			ZetMapMemory(DrvVidRAM,	0x6000, 0x7fff, MAP_RAM);
			ZetSetWriteHandler(magspot_write);
			ZetSetReadHandler(magspot_read);
		break;
	}

	ZetClose();

	if (has_samples) {
		BurnSampleInit(0);
		BurnSampleSetAllRoutesAllSamples(SAMPLE_VOL, BURN_SND_ROUTE_BOTH);
	}

	if (type != HW_COSMICA) {
		DACInit(0, 0, has_samples ? 1 : 0, ZetTotalCycles, cpu_clock);
		DACSetRoute(0, 0.25, BURN_SND_ROUTE_BOTH);
		DACDCBlock(1);
	}

	BurnSetRefreshRate(5408000.0 / (352.0 * 262.0));	// 58.64hz

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	if (has_samples) BurnSampleExit();
	if (hardware_type != HW_COSMICA) DACExit();

	BurnFreeMemIndex();

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset(1);
	}

	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}

		switch (hardware_type)
		{
			case HW_PANIC:
				ProcessJoystick(&DrvInputs[0], 0, 4, 3, 2, 1, INPUT_4WAY | INPUT_ISACTIVELOW);
				ProcessJoystick(&DrvInputs[1], 1, 4, 3, 2, 1, INPUT_4WAY | INPUT_ISACTIVELOW);
			break;

			case HW_COSMICA:
				DrvInputs[2] = (DrvInputs[2] & 0xc0) | (DrvDips[0] & 0x3f);
			break;

			case HW_MAGSPOT:
			case HW_DEVZONE:
				DrvInputs[0] = (DrvInputs[0] & 0x3f) | ((hardware_type == HW_MAGSPOT) ? (DrvDips[0] & 0xc0) : 0xc0);
				DrvInputs[2] = (DrvInputs[2] & 0xc0) | (DrvDips[1] & 0x3f);
				DrvInputs[4] = DrvDips[2];
			break;

			case HW_NOMNLND:
				ProcessJoystick(&DrvInputs[0], 0, 7, 3, 1, 5, INPUT_CLEAROPPOSITES | INPUT_ISACTIVELOW);
				ProcessJoystick(&DrvInputs[1], 1, 7, 3, 1, 5, INPUT_CLEAROPPOSITES | INPUT_ISACTIVELOW);
				DrvInputs[2] = (DrvInputs[2] & 0xc0) | (DrvDips[0] & 0x3f);
				DrvInputs[4] = 0; // no coinage dsw, 0x3800-0x3807 read as 1
			break;
		}
	}

	ZetOpen(0);

	{
		UINT8 coin_now = 0;

		switch (hardware_type)
		{
			case HW_PANIC:
				coin_now = (DrvJoy3[6] ? 1 : 0) | (DrvJoy3[7] ? 2 : 0);
				if ((coin_now & ~coin_last) && sound_enabled) splaych(0, 10, SAMPLE_VOL);
			break;

			case HW_COSMICA:
			case HW_NOMNLND:
				coin_now = DrvJoy5[0] ? 1 : 0;
				ZetSetIRQLine(CPU_IRQLINE_NMI, coin_now ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
			break;

			case HW_MAGSPOT:
			case HW_DEVZONE:
				coin_now = (DrvJoy5[0] ? 1 : 0) | (DrvJoy5[1] ? 2 : 0);
				if (coin_now & ~coin_last & 1) {
					ZetSetVector(0xff);
					ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
				}
				ZetSetIRQLine(CPU_IRQLINE_NMI, (coin_now & 2) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
			break;
		}

		coin_last = coin_now;
	}

	INT32 nInterleave = 262;
	INT32 nCyclesTotal[1] = { cycles_per_frame };
	INT32 nCyclesDone[1] = { nExtraCycles };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		scanline = i;

		if (hardware_type == HW_PANIC && (i == 0 || i == 224)) {
			ZetSetVector((i == 224) ? 0xd7 : 0xcf);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}

		CPU_RUN(0, Zet);
	}

	nExtraCycles = nCyclesDone[0] - nCyclesTotal[0];

	if (pBurnSoundOut) {
		if (has_samples) {
			BurnSampleRender(pBurnSoundOut, nBurnSoundLen);
		}
		if (hardware_type != HW_COSMICA) {
			DACUpdate(pBurnSoundOut, nBurnSoundLen);
		}
		BurnSoundDCFilter();
	}

	ZetClose();

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	frame_number++;

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	if (pnMin) {
		*pnMin = 0x029702;
	}

	if (nAction & ACB_VOLATILE) {
		ScanVar(AllRam, RamEnd - AllRam, "All Ram");

		ZetScan(nAction);

		if (has_samples) BurnSampleScan(nAction, pnMin);
		if (hardware_type != HW_COSMICA) DACScan(nAction, pnMin);

		SCAN_VAR(color_registers);
		SCAN_VAR(flipscreen);
		SCAN_VAR(background_enable);
		SCAN_VAR(sound_enabled);
		SCAN_VAR(dive_bomb_b_select);
		SCAN_VAR(frame_number);
		SCAN_VAR(coin_last);
		SCAN_VAR(nExtraCycles);
	}

	return 0;
}

static struct BurnSampleInfo CosmicaSampleDesc[] = {
	{ "backgr",		SAMPLE_NOLOOP },	//  0
	{ "extend",		SAMPLE_NOLOOP },	//  1
	{ "divea",		SAMPLE_NOLOOP },	//  2
	{ "diveb1",		SAMPLE_NOLOOP },	//  3
	{ "diveb2",		SAMPLE_NOLOOP },	//  4
	{ "diveb3",		SAMPLE_NOLOOP },	//  5
	{ "diveb4",		SAMPLE_NOLOOP },	//  6
	{ "diveb5",		SAMPLE_NOLOOP },	//  7
	{ "diveb6",		SAMPLE_NOLOOP },	//  8
	{ "fire",		SAMPLE_NOLOOP },	//  9
	{ "loudexp",	SAMPLE_NOLOOP },	// 10
	{ "smallexp",	SAMPLE_NOLOOP },	// 11
	{ "coin",		SAMPLE_NOLOOP },	// 12
	{ "",			0 }
};

STD_SAMPLE_PICK(Cosmica)
STD_SAMPLE_FN(Cosmica)

static struct BurnSampleInfo PanicSampleDesc[] = {
	{ "walk",		SAMPLE_NOLOOP },	//  0
	{ "upordown",	SAMPLE_NOLOOP },	//  1
	{ "trapped",	SAMPLE_NOLOOP },	//  2
	{ "falling",	SAMPLE_NOLOOP },	//  3
	{ "escaping",	SAMPLE_NOLOOP },	//  4
	{ "ekilled",	SAMPLE_NOLOOP },	//  5
	{ "death",		SAMPLE_NOLOOP },	//  6
	{ "elaugh",		SAMPLE_NOLOOP },	//  7
	{ "extral",		SAMPLE_NOLOOP },	//  8
	{ "oxygen",		SAMPLE_NOLOOP },	//  9
	{ "coin",		SAMPLE_NOLOOP },	// 10
	{ "",			0 }
};

STD_SAMPLE_PICK(Panic)
STD_SAMPLE_FN(Panic)

static INT32 PanicInit()   { return DrvInit(HW_PANIC); }
static INT32 CosmicaInit() { return DrvInit(HW_COSMICA); }
static INT32 MagspotInit() { return DrvInit(HW_MAGSPOT); }
static INT32 DevzoneInit() { return DrvInit(HW_DEVZONE); }
static INT32 NomnlndInit() { return DrvInit(HW_NOMNLND); }


// Space Panic (version E)

static struct BurnRomInfo panicRomDesc[] = {
	{ "spe1",			0x0800, 0x70ac0888, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "spe2",			0x0800, 0x2b910c48, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "spe3",			0x0800, 0x03810148, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "spe4",			0x0800, 0x119bbbfd, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "spcpanic.5",		0x0800, 0x5b80f277, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "spcpanic.6",		0x0800, 0xb73babf0, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "spe7",			0x0800, 0x2894106e, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "spcpanic.11",	0x0800, 0xacea9df4, 2 | BRF_GRA },           //  7 Sprites
	{ "spcpanic.12",	0x0800, 0xe83423d0, 2 | BRF_GRA },           //  8
	{ "spcpanic.10",	0x0800, 0xc9631c2d, 2 | BRF_GRA },           //  9
	{ "spcpanic.9",		0x0800, 0xeec78b4c, 2 | BRF_GRA },           // 10

	{ "82s123.sp",		0x0020, 0x35d43d2f, 3 | BRF_GRA },           // 11 Color PROM

	{ "spcpanic.8",		0x0800, 0x7da0b321, 4 | BRF_GRA },           // 12 Color Map
};

STD_ROM_PICK(panic)
STD_ROM_FN(panic)

struct BurnDriver BurnDrvPanic = {
	"panic", NULL, NULL, "panic", "1980",
	"Space Panic (version E)\0", NULL, "Universal", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, panicRomInfo, panicRomName, NULL, NULL, PanicSampleInfo, PanicSampleName, PanicInputInfo, PanicDIPInfo,
	PanicInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x30,
	192, 256, 3, 4
};


// Space Panic (set 2)

static struct BurnRomInfo panic2RomDesc[] = {
	{ "spcpanic.1",		0x0800, 0x405ae6f9, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "spcpanic.2",		0x0800, 0xb6a286c5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "spcpanic.3",		0x0800, 0x85ae8b2e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "spcpanic.4",		0x0800, 0xb6d4f52f, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "spcpanic.5",		0x0800, 0x5b80f277, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "spcpanic.6",		0x0800, 0xb73babf0, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "spcpanic.7",		0x0800, 0xfc27f4e5, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "spcpanic.11",	0x0800, 0xacea9df4, 2 | BRF_GRA },           //  7 Sprites
	{ "spcpanic.12",	0x0800, 0xe83423d0, 2 | BRF_GRA },           //  8
	{ "spcpanic.10",	0x0800, 0xc9631c2d, 2 | BRF_GRA },           //  9
	{ "spcpanic.9",		0x0800, 0xeec78b4c, 2 | BRF_GRA },           // 10

	{ "82s123.sp",		0x0020, 0x35d43d2f, 3 | BRF_GRA },           // 11 Color PROM

	{ "spcpanic.8",		0x0800, 0x7da0b321, 4 | BRF_GRA },           // 12 Color Map
};

STD_ROM_PICK(panic2)
STD_ROM_FN(panic2)

struct BurnDriver BurnDrvPanic2 = {
	"panic2", "panic", NULL, "panic", "1980",
	"Space Panic (set 2)\0", NULL, "Universal", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, panic2RomInfo, panic2RomName, NULL, NULL, PanicSampleInfo, PanicSampleName, PanicInputInfo, PanicDIPInfo,
	PanicInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x30,
	192, 256, 3, 4
};


// Space Panic (set 3)

static struct BurnRomInfo panic3RomDesc[] = {
	{ "panica.1",		0x0800, 0x289720ce, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "spcpanic.2",		0x0800, 0xb6a286c5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "spcpanic.3",		0x0800, 0x85ae8b2e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "spcpanic.4",		0x0800, 0xb6d4f52f, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "spcpanic.5",		0x0800, 0x5b80f277, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "spcpanic.6",		0x0800, 0xb73babf0, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "panica.7",		0x0800, 0x3641cb7f, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "spcpanic.11",	0x0800, 0xacea9df4, 2 | BRF_GRA },           //  7 Sprites
	{ "spcpanic.12",	0x0800, 0xe83423d0, 2 | BRF_GRA },           //  8
	{ "spcpanic.10",	0x0800, 0xc9631c2d, 2 | BRF_GRA },           //  9
	{ "spcpanic.9",		0x0800, 0xeec78b4c, 2 | BRF_GRA },           // 10

	{ "82s123.sp",		0x0020, 0x35d43d2f, 3 | BRF_GRA },           // 11 Color PROM

	{ "spcpanic.8",		0x0800, 0x7da0b321, 4 | BRF_GRA },           // 12 Color Map
};

STD_ROM_PICK(panic3)
STD_ROM_FN(panic3)

struct BurnDriver BurnDrvPanic3 = {
	"panic3", "panic", NULL, "panic", "1980",
	"Space Panic (set 3)\0", NULL, "Universal", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, panic3RomInfo, panic3RomName, NULL, NULL, PanicSampleInfo, PanicSampleName, PanicInputInfo, PanicDIPInfo,
	PanicInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x30,
	192, 256, 3, 4
};


// Space Panic (set 4)

static struct BurnRomInfo panic4RomDesc[] = {
	{ "sp_m.1",			0x0800, 0x3208f7da, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "sp_m.2",			0x0800, 0x9161e86d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sp_m.3",			0x0800, 0x85ae8b2e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sp_m.4",			0x0800, 0xd9b38bd4, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "sp_m.5",			0x0800, 0x01f25330, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "sp_m.6",			0x0800, 0x56716d33, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "sp_m.7",			0x0800, 0xec2107cc, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "sp_m.11",		0x0800, 0xacea9df4, 2 | BRF_GRA },           //  7 Sprites
	{ "sp_m.12",		0x0800, 0xe83423d0, 2 | BRF_GRA },           //  8
	{ "sp_m.10",		0x0800, 0xc9631c2d, 2 | BRF_GRA },           //  9
	{ "sp_m.9",			0x0800, 0xeec78b4c, 2 | BRF_GRA },           // 10

	{ "74s288.bin",		0x0020, 0x35d43d2f, 3 | BRF_GRA },           // 11 Color PROM

	{ "sp_m.8",			0x0800, 0x7da0b321, 4 | BRF_GRA },           // 12 Color Map
};

STD_ROM_PICK(panic4)
STD_ROM_FN(panic4)

struct BurnDriver BurnDrvPanic4 = {
	"panic4", "panic", NULL, "panic", "1980",
	"Space Panic (set 4)\0", NULL, "Universal", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, panic4RomInfo, panic4RomName, NULL, NULL, PanicSampleInfo, PanicSampleName, PanicInputInfo, PanicDIPInfo,
	PanicInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x30,
	192, 256, 3, 4
};


// Space Panic (harder)

static struct BurnRomInfo panichRomDesc[] = {
	{ "sph1",			0x0800, 0xf6e9c6ef, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "sph2",			0x0800, 0x58dbc49b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sph3",			0x0800, 0xc4f275ad, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sph4",			0x0800, 0x6e7785de, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "sph5",			0x0800, 0x1916c9b8, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "sph6",			0x0800, 0x54b92314, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "sph7",			0x0800, 0x8600b881, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "spcpanic.11",	0x0800, 0xacea9df4, 2 | BRF_GRA },           //  7 Sprites
	{ "spcpanic.12",	0x0800, 0xe83423d0, 2 | BRF_GRA },           //  8
	{ "spcpanic.10",	0x0800, 0xc9631c2d, 2 | BRF_GRA },           //  9
	{ "spcpanic.9",		0x0800, 0xeec78b4c, 2 | BRF_GRA },           // 10

	{ "82s123.sp",		0x0020, 0x35d43d2f, 3 | BRF_GRA },           // 11 Color PROM

	{ "spcpanic.8",		0x0800, 0x7da0b321, 4 | BRF_GRA },           // 12 Color Map
};

STD_ROM_PICK(panich)
STD_ROM_FN(panich)

struct BurnDriver BurnDrvPanich = {
	"panich", "panic", NULL, "panic", "1980",
	"Space Panic (harder)\0", NULL, "Universal", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, panichRomInfo, panichRomName, NULL, NULL, PanicSampleInfo, PanicSampleName, PanicInputInfo, PanicDIPInfo,
	PanicInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x30,
	192, 256, 3, 4
};


// Space Panic (German)

static struct BurnRomInfo panicgerRomDesc[] = {
	{ "spacepan.001",	0x0800, 0xa6d9515a, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "spacepan.002",	0x0800, 0xcfc22663, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "spacepan.003",	0x0800, 0xe1f36893, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "spacepan.004",	0x0800, 0x01be297c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "spacepan.005",	0x0800, 0xe0d54805, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "spacepan.006",	0x0800, 0xaae1458e, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "spacepan.007",	0x0800, 0x14e46e70, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "spcpanic.11",	0x0800, 0xacea9df4, 2 | BRF_GRA },           //  7 Sprites
	{ "spcpanic.12",	0x0800, 0xe83423d0, 2 | BRF_GRA },           //  8
	{ "spcpanic.10",	0x0800, 0xc9631c2d, 2 | BRF_GRA },           //  9
	{ "spcpanic.9",		0x0800, 0xeec78b4c, 2 | BRF_GRA },           // 10

	{ "82s123.sp",		0x0020, 0x35d43d2f, 3 | BRF_GRA },           // 11 Color PROM

	{ "spcpanic.8",		0x0800, 0x7da0b321, 4 | BRF_GRA },           // 12 Color Map
};

STD_ROM_PICK(panicger)
STD_ROM_FN(panicger)

struct BurnDriver BurnDrvPanicger = {
	"panicger", "panic", NULL, "panic", "1980",
	"Space Panic (German)\0", NULL, "Universal (ADP Automaten license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, panicgerRomInfo, panicgerRomName, NULL, NULL, PanicSampleInfo, PanicSampleName, PanicInputInfo, PanicDIPInfo,
	PanicInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x30,
	192, 256, 3, 4
};


// Space Panic (bootleg)

static struct BurnRomInfo panicblRomDesc[] = {
	{ "1.2d",			0x0800, 0xe1efa292, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "2.3d",			0x0800, 0xa5885fb1, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.4d",			0x0800, 0xab402d38, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.5d",			0x0800, 0x27c69de1, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "5.5d",			0x0800, 0x9b409a21, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "6.6d",			0x0800, 0xadd15e92, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "7.7d",			0x0800, 0x786c8d41, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "11.7m",			0x0800, 0xacea9df4, 2 | BRF_GRA },           //  7 Sprites
	{ "12.5m",			0x0800, 0xe83423d0, 2 | BRF_GRA },           //  8
	{ "10.8m",			0x0800, 0xc9631c2d, 2 | BRF_GRA },           //  9
	{ "9.9m",			0x0800, 0xeec78b4c, 2 | BRF_GRA },           // 10

	{ "mmi6330.5l",		0x0020, 0x35d43d2f, 3 | BRF_GRA },           // 11 Color PROM (not dumped for this set)

	{ "8.2l",			0x0800, 0x7da0b321, 4 | BRF_GRA },           // 12 Color Map
};

STD_ROM_PICK(panicbl)
STD_ROM_FN(panicbl)

struct BurnDriver BurnDrvPanicbl = {
	"panicbl", "panic", NULL, "panic", "1980",
	"Space Panic (bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, panicblRomInfo, panicblRomName, NULL, NULL, PanicSampleInfo, PanicSampleName, PanicInputInfo, PanicDIPInfo,
	PanicInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x30,
	192, 256, 3, 4
};


// Cosmic Alien (version II, set 1)
// Later revision 7910-AII PCB; this set does NOT have the 1979 copyright date on the titlescreen!

static struct BurnRomInfo cosmicaRomDesc[] = {
	{ "ii-1.e3",		0x0800, 0x535ee0c5, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "ii-2.e4",		0x0800, 0xed3cf8f7, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ii-3.e5",		0x0800, 0x6a111e5e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ii-4.e6",		0x0800, 0xc9b5ca2a, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ii-5.e7",		0x0800, 0x43666d68, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "ii-7.n2",		0x0800, 0xaa6c6079, 2 | BRF_GRA },           //  5 Sprites
	{ "ii-6.n1",		0x0800, 0x431e866c, 2 | BRF_GRA },           //  6

	{ "u7910.d9",		0x0020, 0xdfb60f19, 3 | BRF_GRA },           //  7 Color PROM

	{ "9.e2",			0x0400, 0xea4ee931, 4 | BRF_GRA },           //  8 Color Map

	{ "8.k3",			0x0400, 0xacbd4e98, 5 | BRF_GRA },           //  9 Starfield Generator
};

STD_ROM_PICK(cosmica)
STD_ROM_FN(cosmica)

struct BurnDriver BurnDrvCosmica = {
	"cosmica", NULL, NULL, "cosmica", "1979",
	"Cosmic Alien (version II, set 1)\0", "Imperfect sound", "Universal", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, cosmicaRomInfo, cosmicaRomName, NULL, NULL, CosmicaSampleInfo, CosmicaSampleName, CosmicaInputInfo, CosmicaDIPInfo,
	CosmicaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x48,
	192, 256, 3, 4
};


// Cosmic Alien (version II, set 2)

static struct BurnRomInfo cosmica22RomDesc[] = {
	{ "1.e3",			0x0800, 0x535ee0c5, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "2-ii-2.e4",		0x0800, 0x6c9907e8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3-ii-3.e5",		0x0800, 0xc7205278, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4-ii-4.e6",		0x0800, 0xc7765ecd, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "5-ii-5.e7",		0x0800, 0x5f60242f, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "7.n2",			0x0800, 0xaa6c6079, 2 | BRF_GRA },           //  5 Sprites
	{ "6.n1",			0x0800, 0x431e866c, 2 | BRF_GRA },           //  6

	{ "u7910.d9",		0x0020, 0xdfb60f19, 3 | BRF_GRA },           //  7 Color PROM (not dumped for this set, probably matches the other)

	{ "9-9.e2",			0x0400, 0xea4ee931, 4 | BRF_GRA },           //  8 Color Map

	{ "8-8.ic10",		0x0400, 0xacbd4e98, 5 | BRF_GRA },           //  9 Starfield Generator
};

STD_ROM_PICK(cosmica22)
STD_ROM_FN(cosmica22)

struct BurnDriver BurnDrvCosmica22 = {
	"cosmica22", "cosmica", NULL, "cosmica", "1979",
	"Cosmic Alien (version II, set 2)\0", "Imperfect sound", "Universal", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, cosmica22RomInfo, cosmica22RomName, NULL, NULL, CosmicaSampleInfo, CosmicaSampleName, CosmicaInputInfo, CosmicaDIPInfo,
	CosmicaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x48,
	192, 256, 3, 4
};


// Cosmic Alien (version II, set 3)

static struct BurnRomInfo cosmica23RomDesc[] = {
	{ "1-al2019-2516.e3",	0x0800, 0x2303333f, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "2-al202a-2516.e4",	0x0800, 0x20710582, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3-al2019-2516.e5",	0x0800, 0xb25d24b4, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4-2516.e6",			0x0800, 0x51834cd9, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "5-al2019-2516.e7",	0x0800, 0xe2b6680f, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "7-2716.n2",			0x0800, 0xaa6c6079, 2 | BRF_GRA },           //  5 Sprites
	{ "6-2516.n1",			0x0800, 0x431e866c, 2 | BRF_GRA },           //  6

	{ "mmi-6331.d9",		0x0020, 0xdfb60f19, 3 | BRF_GRA },           //  7 Color PROM

	{ "9-4708-2708.e2",		0x0400, 0xea4ee931, 4 | BRF_GRA },           //  8 Color Map

	{ "8-2708.k3",			0x0400, 0xacbd4e98, 5 | BRF_GRA },           //  9 Starfield Generator
};

STD_ROM_PICK(cosmica23)
STD_ROM_FN(cosmica23)

struct BurnDriver BurnDrvCosmica23 = {
	"cosmica23", "cosmica", NULL, "cosmica", "1979",
	"Cosmic Alien (version II, set 3)\0", "Imperfect sound", "Universal", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, cosmica23RomInfo, cosmica23RomName, NULL, NULL, CosmicaSampleInfo, CosmicaSampleName, CosmicaInputInfo, CosmicaDIPInfo,
	CosmicaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x48,
	192, 256, 3, 4
};


// Cosmic Alien (early version II?)

static struct BurnRomInfo cosmica2aRomDesc[] = {
	{ "a-1.e2",			0x0400, 0x8a401b22, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "b-2.d3",			0x0400, 0xc8bf86b1, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "c-3.e3",			0x0400, 0x699c849e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "d-4.d4",			0x0400, 0x168e38da, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "e-5.e4",			0x0400, 0x80cc1fb8, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "f-6.d5",			0x0400, 0x0dc464f7, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "g-7.e5",			0x0400, 0xd5381c54, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "h-8.d6",			0x0400, 0x2175fe6f, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "i-9.e6",			0x0400, 0x3bb57720, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "j-0.d7",			0x0400, 0x4ff70f45, 1 | BRF_PRG | BRF_ESS }, //  9

	{ "l-8.n2",			0x0800, 0xaa6c6079, 2 | BRF_GRA },           // 10 Sprites
	{ "k-7.n1",			0x0800, 0x431e866c, 2 | BRF_GRA },           // 11

	{ "u7910.d9",		0x0020, 0xdfb60f19, 3 | BRF_GRA },           // 12 Color PROM

	{ "9-9.e2",			0x0400, 0xea4ee931, 4 | BRF_GRA },           // 13 Color Map

	{ "8-8.k3",			0x0400, 0xacbd4e98, 5 | BRF_GRA },           // 14 Starfield Generator
};

STD_ROM_PICK(cosmica2a)
STD_ROM_FN(cosmica2a)

struct BurnDriver BurnDrvCosmica2a = {
	"cosmica2a", "cosmica", NULL, "cosmica", "1979",
	"Cosmic Alien (early version II?)\0", "Imperfect sound", "Universal", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, cosmica2aRomInfo, cosmica2aRomName, NULL, NULL, CosmicaSampleInfo, CosmicaSampleName, CosmicaInputInfo, CosmicaDIPInfo,
	CosmicaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x48,
	192, 256, 3, 4
};


// Cosmic Alien (first version)

static struct BurnRomInfo cosmica1RomDesc[] = {
	{ "1.e3",			0x0800, 0x2e44f50e, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "2.e4",			0x0800, 0x9e5c5281, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.e5",			0x0800, 0x9e1309db, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.e6",			0x0800, 0xba4a9295, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "5.e7",			0x0800, 0x2106c82a, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "7.n2",			0x0800, 0xee3e86fc, 2 | BRF_GRA },           //  5 Sprites
	{ "6.n1",			0x0800, 0x81c86ca0, 2 | BRF_GRA },           //  6

	{ "u7910.d9",		0x0020, 0xdfb60f19, 3 | BRF_GRA },           //  7 Color PROM

	{ "9.e2",			0x0400, 0xea4ee931, 4 | BRF_GRA },           //  8 Color Map

	{ "8.k3",			0x0400, 0xacbd4e98, 5 | BRF_GRA },           //  9 Starfield Generator
};

STD_ROM_PICK(cosmica1)
STD_ROM_FN(cosmica1)

struct BurnDriver BurnDrvCosmica1 = {
	"cosmica1", "cosmica", NULL, "cosmica", "1979",
	"Cosmic Alien (first version)\0", "Imperfect sound", "Universal", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, cosmica1RomInfo, cosmica1RomName, NULL, NULL, CosmicaSampleInfo, CosmicaSampleName, CosmicaInputInfo, CosmicaDIPInfo,
	CosmicaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x48,
	192, 256, 3, 4
};


// Sengoku no Jieitai

static struct BurnRomInfo nomnlndRomDesc[] = {
	{ "1.bin",			0x0800, 0xba117ba6, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "2.bin",			0x0800, 0xe5ed654f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.bin",			0x0800, 0x7fc42724, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "5.bin",			0x0800, 0x9cc2f1d9, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "4.bin",			0x0800, 0x0e8cd46a, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "6.bin",			0x0800, 0xba472ba5, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "nml8.n2",		0x0800, 0x739009b4, 2 | BRF_GRA },           //  6 Sprites
	{ "nml7.n1",		0x0800, 0xd08ed22f, 2 | BRF_GRA },           //  7

	{ "nml.d9",			0x0020, 0x65e911f9, 3 | BRF_GRA },           //  8 Color PROM

	{ "nl9.e2",			0x0400, 0x9e05f14e, 4 | BRF_GRA },           //  9 Color Map

	{ "nl10.ic4",		0x0400, 0x5b13f64e, 5 | BRF_GRA },           // 10 Tree + River
	{ "nl11.ic7",		0x0400, 0xe717b241, 5 | BRF_GRA },           // 11
};

STD_ROM_PICK(nomnlnd)
STD_ROM_FN(nomnlnd)

struct BurnDriver BurnDrvNomnlnd = {
	"nomnlnd", NULL, NULL, NULL, "1980",
	"Sengoku no Jieitai\0", "no sfx", "Universal", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, nomnlndRomInfo, nomnlndRomName, NULL, NULL, NULL, NULL, NomnlndInputInfo, NomnlndDIPInfo,
	NomnlndInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x30,
	192, 256, 3, 4
};


// No Man's Land (Gottlieb)

static struct BurnRomInfo nomnlndgRomDesc[] = {
	{ "nml1.e3",		0x0800, 0xe212ed91, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "nml2.e4",		0x0800, 0xf66ef3d8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "nml3.e5",		0x0800, 0xd422fc8a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "nml5.e7",		0x0800, 0xd58952ac, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "nml4.e6",		0x0800, 0x994c9afb, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "nml6.e8",		0x0800, 0x01ed2d8c, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "nml8.n2",		0x0800, 0x739009b4, 2 | BRF_GRA },           //  6 Sprites
	{ "nml7.n1",		0x0800, 0xd08ed22f, 2 | BRF_GRA },           //  7

	{ "nml.d9",			0x0020, 0x65e911f9, 3 | BRF_GRA },           //  8 Color PROM

	{ "nl9.e2",			0x0400, 0x9e05f14e, 4 | BRF_GRA },           //  9 Color Map

	{ "nl10.ic4",		0x0400, 0x5b13f64e, 5 | BRF_GRA },           // 10 Tree + River
	{ "nl11.ic7",		0x0400, 0xe717b241, 5 | BRF_GRA },           // 11
};

STD_ROM_PICK(nomnlndg)
STD_ROM_FN(nomnlndg)

struct BurnDriver BurnDrvNomnlndg = {
	"nomnlndg", "nomnlnd", NULL, NULL, "1980",
	"No Man's Land (Gottlieb)\0", "no sfx", "Universal (Gottlieb license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, nomnlndgRomInfo, nomnlndgRomName, NULL, NULL, NULL, NULL, NomnlndInputInfo, NomnlndgDIPInfo,
	NomnlndInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x30,
	192, 256, 3, 4
};


// Magical Spot

static struct BurnRomInfo magspotRomDesc[] = {
	{ "ms1.bin",		0x0800, 0x59e9019d, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "ms2.bin",		0x0800, 0x98b913b1, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ms3.bin",		0x0800, 0xea58c124, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ms5.bin",		0x0800, 0x4302a658, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ms4.bin",		0x0800, 0x088582ab, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "ms6.bin",		0x0800, 0xe6bf492c, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "ms8.bin",		0x0800, 0x9e1d63a2, 2 | BRF_GRA },           //  6 Sprites
	{ "ms7.bin",		0x0800, 0x1ab338d3, 2 | BRF_GRA },           //  7

	{ "ms.d9",			0x0020, 0x36e2aa2a, 3 | BRF_GRA },           //  8 Color PROM

	{ "ms.e2",			0x0400, 0x89f23ebd, 4 | BRF_GRA },           //  9 Color Map
};

STD_ROM_PICK(magspot)
STD_ROM_FN(magspot)

struct BurnDriver BurnDrvMagspot = {
	"magspot", NULL, NULL, NULL, "1980",
	"Magical Spot\0", "no sfx", "Universal", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, magspotRomInfo, magspotRomName, NULL, NULL, NULL, NULL, MagspotInputInfo, MagspotDIPInfo,
	MagspotInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x30,
	192, 256, 3, 4
};


// Magical Spot II

static struct BurnRomInfo magspot2RomDesc[] = {
	{ "ms.e3",			0x0800, 0xc0085ade, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "ms.e4",			0x0800, 0xd534a68b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ms.e5",			0x0800, 0x25513b2a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ms.e7",			0x0800, 0x8836bbc4, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ms.e6",			0x0800, 0x6a08ab94, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "ms.e8",			0x0800, 0x77c6d109, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "ms.n2",			0x0800, 0x9e1d63a2, 2 | BRF_GRA },           //  6 Sprites
	{ "ms.n1",			0x0800, 0x1ab338d3, 2 | BRF_GRA },           //  7

	{ "ms.d9",			0x0020, 0x36e2aa2a, 3 | BRF_GRA },           //  8 Color PROM

	{ "ms.e2",			0x0400, 0x89f23ebd, 4 | BRF_GRA },           //  9 Color Map
};

STD_ROM_PICK(magspot2)
STD_ROM_FN(magspot2)

struct BurnDriver BurnDrvMagspot2 = {
	"magspot2", NULL, NULL, NULL, "1980",
	"Magical Spot II\0", "no sfx", "Universal", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, magspot2RomInfo, magspot2RomName, NULL, NULL, NULL, NULL, MagspotInputInfo, MagspotDIPInfo,
	MagspotInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x30,
	192, 256, 3, 4
};


// Devil Zone

static struct BurnRomInfo devzoneRomDesc[] = {
	{ "dv1.e3",			0x0800, 0xc70faf00, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "dv2.e4",			0x0800, 0xeacfed61, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "dv3.e5",			0x0800, 0x7973317e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "dv5.e7",			0x0800, 0xb71a3989, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "dv4.e6",			0x0800, 0xa58c5b8c, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "dv6.e8",			0x0800, 0x3930fb67, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "dv8.n2",			0x0800, 0xda1cbec1, 2 | BRF_GRA },           //  6 Sprites
	{ "dv7.n1",			0x0800, 0xe7562fcf, 2 | BRF_GRA },           //  7

	{ "m13.d9",			0x0020, 0x36e2aa2a, 3 | BRF_GRA },           //  8 Color PROM

	{ "db9.e2",			0x0400, 0x693855b6, 4 | BRF_GRA },           //  9 Color Map

	{ "dv9.ic12",		0x0800, 0xf61c1c45, 5 | BRF_GRA },           // 10 Grid horizontal line positions

	{ "22.ic1",			0x0020, 0xdf974878, 6 | BRF_GRA },           // 11 Grid vertical line positions
};

STD_ROM_PICK(devzone)
STD_ROM_FN(devzone)

struct BurnDriver BurnDrvDevzone = {
	"devzone", NULL, NULL, NULL, "1980",
	"Devil Zone\0", "no sfx", "Universal", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, devzoneRomInfo, devzoneRomName, NULL, NULL, NULL, NULL, MagspotInputInfo, DevzoneDIPInfo,
	DevzoneInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x30,
	192, 256, 3, 4
};


// Devil Zone (easier)

static struct BurnRomInfo devzone2RomDesc[] = {
	{ "p10_1.e3",		0x0800, 0x38bd45a4, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "my4_2.e4",		0x0800, 0xe1637800, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ms6_3.e5",		0x0800, 0xc1952e2f, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "mx6_5.e7",		0x0800, 0xc5394215, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "my1_4.e6",		0x0800, 0x5d965d93, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "mz7_6.e8",		0x0800, 0x8504e8c9, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "my8_8.n2",		0x0800, 0x18abce02, 2 | BRF_GRA },           //  6 Sprites
	{ "mx3_7.n1",		0x0800, 0xc089c9e3, 2 | BRF_GRA },           //  7

	{ "ms.d9",			0x0020, 0x36e2aa2a, 3 | BRF_GRA },           //  8 Color PROM

	{ "dz9.e2",			0x0400, 0x693855b6, 4 | BRF_GRA },           //  9 Color Map

	{ "ic12.sub",		0x0800, 0xf61c1c45, 5 | BRF_GRA },           // 10 Grid horizontal line positions

	{ "ic1.sub",		0x0020, 0xdf974878, 6 | BRF_GRA },           // 11 Grid vertical line positions
};

STD_ROM_PICK(devzone2)
STD_ROM_FN(devzone2)

struct BurnDriver BurnDrvDevzone2 = {
	"devzone2", "devzone", NULL, NULL, "1980",
	"Devil Zone (easier)\0", "no sfx", "Universal", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, devzone2RomInfo, devzone2RomName, NULL, NULL, NULL, NULL, MagspotInputInfo, Devzone2DIPInfo,
	DevzoneInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x30,
	192, 256, 3, 4
};
