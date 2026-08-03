// FB Neo Bally Astrocade Driver Module
// Based on MAME driver by Nicola Salmoria, Mike Coates, Frank Palazzolo

// to done:
// wow
// gorf
// profpac
// spacezap
// robby
// ebases
// demndrgn inputs sporadic weirdness, no service mode (normal)

#include "tiles_generic.h"
#include "z80_intf.h"
#include "samples.h" // seawolf2
#include "math.h"
#include "burn_pal.h"
#include "burn_gun.h" // trackball (ebases, demndrgn)
#include "astrocde_snd.h"
#include "votrax.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvCartROM;
static UINT8 *DrvEPROMS;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvVidRAM;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvJoyF[4];
static UINT8 DrvDips[4];
static UINT8 DrvInputs[8];
static INT16 Analog[4];
static UINT8 DrvKbd[4][6];

static UINT8 DrvReset;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 nExtraCycles;

static UINT8 interrupt_vector;
static UINT8 NextScanInt;
static UINT8 InterruptFlag;
static UINT8 VerticalBlank;

// add to reset/scan
static UINT8 funcgen_expand_color[2];
static UINT8 funcgen_control;
static UINT8 funcgen_expand_count;
static UINT8 funcgen_rotate_count;
static UINT8 funcgen_rotate_data[4];
static UINT8 funcgen_shift_prev_data;
static UINT8 funcgen_intercept;
static UINT16 pattern_source;
static UINT8 pattern_mode;
static UINT16 pattern_dest;
static UINT8 pattern_skip;
static UINT8 pattern_width;
static UINT8 pattern_height;

static INT32 scanline;
static INT32 verti;
static INT32 hori;

static UINT8 colorsplit;
static UINT8 colors[8];
static UINT8 sparkle[4];
static UINT8 *sparklestar;
static UINT8 BackgroundData;

#define RNG_PERIOD      ((1 << 17) - 1)
#define VERT_OFFSET     (22)                /* pixels from top of screen to top of game area */
#define HORZ_OFFSET     (16)                /* pixels from left of screen to left of game area */

static UINT8 video_mode;

static INT32 ram_write_enable;

static INT32 trackball_select;

static UINT8 profpac_bankdata;
static UINT8 profpac_readshift;
static UINT8 profpac_readpage;
static UINT8 profpac_vispage;
static UINT8 profpac_writepage;
static UINT8 profpac_writemode;
static UINT16 profpac_writemask;
static UINT8 profpac_intercept;
static UINT8 profpac_vw;
static UINT16 profpac_palram[16];
static UINT16 profpac_color_mapping[4];

static UINT8 sound_p0_last;
static UINT8 sound_p1_last;
static UINT8 lamps[2];

// configuration stuff
static void (*update_line_cb)(int);

static INT32 y_offset = 0;
static UINT16 *bitmap;
static INT32 has_sc01 = 0; // votrax sc01 speech synth
static INT32 has_snd = 0; // astrocde sound synth
static INT32 has_sam = 0; // samples
static INT32 is_wow = 0;
static INT32 is_seawolf2 = 0;
static INT32 is_home = 0;
static INT32 has_stars = 0;
static INT32 has_tball = 0;

enum { CART_STD = 0, CART_256K, CART_512K };

static INT32 cart_size;
static INT32 cart_bank;
static INT32 cart_type;

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo Seawolf2InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 start"	},
	A("P1 X Axis", 		BIT_ANALOG_REL, &Analog[2],		"p1 x-axis"),
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 fire 1"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 start"	},
	A("P2 X Axis", 		BIT_ANALOG_REL, &Analog[0],		"p2 x-axis"),
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",			BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};

STDINPUTINFO(Seawolf2)

static struct BurnInputInfo DemndrgnInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	A("P1 Trackball X", BIT_ANALOG_REL, &Analog[0],		"p1 x-axis"),
	A("P1 Trackball Y", BIT_ANALOG_REL, &Analog[1],		"p1 y-axis"),
	A("P1 Fire X", 		BIT_ANALOG_REL, &Analog[2],		"p1 x-axis2"),
	A("P1 Fire Y", 		BIT_ANALOG_REL, &Analog[3],		"p1 y-axis2"),

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 start"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 3,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",			BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};

STDINPUTINFO(Demndrgn)

static struct BurnInputInfo SpacezapInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy3 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy3 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 2,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",			BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};

STDINPUTINFO(Spacezap)

static struct BurnInputInfo WowInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy3 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy3 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 4,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",			BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};

STDINPUTINFO(Wow)

static struct BurnInputInfo GorfInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy3 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy3 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 3,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",			BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};

STDINPUTINFO(Gorf)

static struct BurnInputInfo RobbyInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy3 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy3 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 4,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",			BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};

STDINPUTINFO(Robby)

static struct BurnInputInfo ProfpacInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 start"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 3,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",			BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};

STDINPUTINFO(Profpac)

static struct BurnInputInfo EbasesInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	A("P1 Trackball X", BIT_ANALOG_REL, &Analog[0],		"p1 x-axis"),
	A("P1 Trackball Y", BIT_ANALOG_REL, &Analog[1],		"p1 y-axis"),
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 start"	},
	A("P2 Trackball X", BIT_ANALOG_REL, &Analog[2],		"p2 x-axis"),
	A("P2 Trackball Y", BIT_ANALOG_REL, &Analog[3],		"p2 y-axis"),
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy2 + 2,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",			BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};

STDINPUTINFO(Ebases)

static struct BurnInputInfo HomeInputList[] = {
	{"P1 Start",		BIT_DIGITAL,	DrvJoyF + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right", 		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	A("P1 Knob",		BIT_ANALOG_REL,	&Analog[0],		"p1 x-axis"),
	{"P1 Trigger",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoyF + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right", 		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	A("P2 Knob",		BIT_ANALOG_REL,	&Analog[1],		"p2 x-axis"),
	{"P2 Trigger",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},

	{"P3 Start",		BIT_DIGITAL,	DrvJoyF + 2,	"p3 start"	},
	{"P3 Up",			BIT_DIGITAL,	DrvJoy3 + 0,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	DrvJoy3 + 1,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	DrvJoy3 + 2,	"p3 left"	},
	{"P3 Right", 		BIT_DIGITAL,	DrvJoy3 + 3,	"p3 right"	},
	A("P3 Knob",		BIT_ANALOG_REL,	&Analog[2],		"p3 x-axis"),
	{"P3 Trigger",		BIT_DIGITAL,	DrvJoy3 + 4,	"p3 fire 1"	},

	{"P4 Start",		BIT_DIGITAL,	DrvJoyF + 3,	"p4 start"	},
	{"P4 Up",			BIT_DIGITAL,	DrvJoy4 + 0,	"p4 up"		},
	{"P4 Down",			BIT_DIGITAL,	DrvJoy4 + 1,	"p4 down"	},
	{"P4 Left",			BIT_DIGITAL,	DrvJoy4 + 2,	"p4 left"	},
	{"P4 Right", 		BIT_DIGITAL,	DrvJoy4 + 3,	"p4 right"	},
	A("P4 Knob",		BIT_ANALOG_REL,	&Analog[3],		"p4 x-axis"),
	{"P4 Trigger",		BIT_DIGITAL,	DrvJoy4 + 4,	"p4 fire 1"	},

	{"1",				BIT_DIGITAL,	&DrvKbd[3][4],	"keyb_1"	},
	{"2",				BIT_DIGITAL,	&DrvKbd[2][4],	"keyb_2"	},
	{"3",				BIT_DIGITAL,	&DrvKbd[1][4],	"keyb_3"	},
	{"4",				BIT_DIGITAL,	&DrvKbd[3][3],	"keyb_4"	},
	{"5",				BIT_DIGITAL,	&DrvKbd[2][3],	"keyb_5"	},
	{"6",				BIT_DIGITAL,	&DrvKbd[2][3],	"keyb_6"	},
	{"7",				BIT_DIGITAL,	&DrvKbd[2][3],	"keyb_7"	},
	{"8",				BIT_DIGITAL,	&DrvKbd[2][3],	"keyb_8"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
};

STDINPUTINFO(Home)

static struct BurnDIPInfo Seawolf2DIPList[]=
{
	DIP_OFFSET(0x08)
	{0x00, 0xff, 0xff, 0x00, NULL					},
	{0x01, 0xff, 0xff, 0x00, NULL					},
	{0x02, 0xff, 0xff, 0x00, NULL					},
	{0x03, 0xff, 0xff, 0xc9, NULL					},

	{0   , 0xfe, 0   ,    2, "Language 1"			},
	{0x00, 0x01, 0x40, 0x00, "Language 2"			},
	{0x00, 0x01, 0x40, 0x40, "French"				},

	{0   , 0xfe, 0   ,    2, "Language 2"			},
	{0x02, 0x01, 0x08, 0x00, "English"				},
	{0x02, 0x01, 0x08, 0x08, "German"				},

	{0   , 0xfe, 0   ,    2, "Coinage"				},
	{0x03, 0x01, 0x01, 0x00, "2 Coins 1 Credits"	},
	{0x03, 0x01, 0x01, 0x01, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    4, "Play Time"			},
	{0x03, 0x01, 0x06, 0x06, "1P 40s/2P 45s"		},
	{0x03, 0x01, 0x06, 0x04, "1P 50s/2P 60s"		},
	{0x03, 0x01, 0x06, 0x02, "1P 60s/2P 75s"		},
	{0x03, 0x01, 0x06, 0x00, "1P 70s/2P 90s"		},

	{0   , 0xfe, 0   ,    2, "2 Players Game"		},
	{0x03, 0x01, 0x08, 0x00, "1 Credit"				},
	{0x03, 0x01, 0x08, 0x08, "2 Credits"			},

	{0   , 0xfe, 0   ,    4, "Extended Play"		},
	{0x03, 0x01, 0x30, 0x10, "5000"					},
	{0x03, 0x01, 0x30, 0x20, "6000"					},
	{0x03, 0x01, 0x30, 0x30, "7000"					},
	{0x03, 0x01, 0x30, 0x00, "None"					},

	{0   , 0xfe, 0   ,    2, "Monitor"				},
	{0x03, 0x01, 0x40, 0x40, "Color"				},
	{0x03, 0x01, 0x40, 0x00, "B/W"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x03, 0x01, 0x80, 0x80, "Off"					},
	{0x03, 0x01, 0x80, 0x00, "On"					},
};

STDDIPINFO(Seawolf2)

static struct BurnDIPInfo DemndrgnDIPList[]=
{
	DIP_OFFSET(0x0a)
	{0x00, 0xff, 0xff, 0xff, NULL									},
	{0x01, 0xff, 0xff, 0x00, NULL									},
	{0x02, 0xff, 0xff, 0x00, NULL									},
	{0x03, 0xff, 0xff, 0x00, NULL									},

	{0   , 0xfe, 0   ,    2, "Service Mode"							},
	{0x00, 0x01, 0x04, 0x04, "Off"									},
	{0x00, 0x01, 0x04, 0x00, "On"									},
};

STDDIPINFO(Demndrgn)

static struct BurnDIPInfo EbasesDIPList[]=
{
	DIP_OFFSET(0x0c)
	{0x00, 0xff, 0xff, 0x33, NULL					},
	{0x01, 0xff, 0xff, 0x8f, NULL					},
	{0x02, 0xff, 0xff, 0x00, NULL					},
	{0x03, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    2, "Monitor"				},
	{0x01, 0x01, 0x10, 0x00, "Color"				},
	{0x01, 0x01, 0x10, 0x10, "B/W"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x01, 0x01, 0x40, 0x00, "Upright"				},
	{0x01, 0x01, 0x40, 0x40, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "2 Players Game"		},
	{0x02, 0x01, 0x01, 0x00, "1 Credit"				},
	{0x02, 0x01, 0x01, 0x01, "2 Credits"			},
};

STDDIPINFO(Ebases)

static struct BurnDIPInfo WowDIPList[]=
{
	DIP_OFFSET(0x12)
	{0x00, 0xff, 0xff, 0xff, NULL									},
	{0x01, 0xff, 0xff, 0xef, NULL									},
	{0x02, 0xff, 0xff, 0x6f, NULL									},
	{0x03, 0xff, 0xff, 0xef, NULL									},

	{0   , 0xfe, 0   ,    2, "Service Mode"							},
	{0x00, 0x01, 0x08, 0x08, "Off"									},
	{0x00, 0x01, 0x08, 0x00, "On"									},

	{0   , 0xfe, 0   ,    2, "Flip Screen"							},
	{0x00, 0x01, 0x80, 0x80, "Off"									},
	{0x00, 0x01, 0x80, 0x00, "On"									},

	{0   , 0xfe, 0   ,    2, "Coin A"								},
	{0x03, 0x01, 0x01, 0x00, "2 Coins 1 Credits"					},
	{0x03, 0x01, 0x01, 0x01, "1 Coin  1 Credits"					},

	{0   , 0xfe, 0   ,    4, "Coin B"								},
	{0x03, 0x01, 0x06, 0x04, "2 Coins 1 Credits"					},
	{0x03, 0x01, 0x06, 0x06, "1 Coin  1 Credits"					},
	{0x03, 0x01, 0x06, 0x02, "1 Coin  3 Credits"					},
	{0x03, 0x01, 0x06, 0x00, "1 Coin  5 Credits"					},

	{0   , 0xfe, 0   ,    2, "Language"								},
	{0x03, 0x01, 0x08, 0x08, "English"								},
	{0x03, 0x01, 0x08, 0x00, "Foreign (NEED ROM)"					},

	{0   , 0xfe, 0   ,    2, "Lives"								},
	{0x03, 0x01, 0x10, 0x10, "2 for 1 Credit / 5 for 2 Credits"		},
	{0x03, 0x01, 0x10, 0x00, "3 for 1 Credit / 7 for 2 Credits"		},

	{0   , 0xfe, 0   ,    2, "Bonus Life"							},
	{0x03, 0x01, 0x20, 0x20, "After 3rd Level"						},
	{0x03, 0x01, 0x20, 0x00, "After 4th Level"						},

	{0   , 0xfe, 0   ,    2, "Free Play"							},
	{0x03, 0x01, 0x40, 0x40, "Off"									},
	{0x03, 0x01, 0x40, 0x00, "On"									},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"							},
	{0x03, 0x01, 0x80, 0x00, "On only when controls are touched"	},
	{0x03, 0x01, 0x80, 0x80, "Always On"							},
};

STDDIPINFO(Wow)

static struct BurnDIPInfo WowgDIPList[]=
{
	{0x03, 0xff, 0xff, 0xe7, NULL									},

	{0   , 0xfe, 0   ,    2, "Language"								},
	{0x03, 0x01, 0x08, 0x08, "English"								},
	{0x03, 0x01, 0x08, 0x00, "German"								},
};

STDDIPINFOEXT(Wowg, Wow, Wowg)

static struct BurnDIPInfo GorfBaseDIPList[]=
{
	DIP_OFFSET(0x10)
	{0x00, 0xff, 0xff, 0xff, NULL									},
	{0x01, 0xff, 0xff, 0xff, NULL									},
	{0x02, 0xff, 0xff, 0x7f, NULL									},
	{0x03, 0xff, 0xff, 0xef, NULL									},

	{0   , 0xfe, 0   ,    2, "Service Mode"							},
	{0x00, 0x01, 0x04, 0x04, "Off"									},
	{0x00, 0x01, 0x04, 0x00, "On"									},

	{0   , 0xfe, 0   ,    2, "Cabinet"								},
	{0x00, 0x01, 0x40, 0x40, "Upright"								},
	{0x00, 0x01, 0x40, 0x00, "Cocktail"								},

	{0   , 0xfe, 0   ,    2, "Speech"								},
	{0x00, 0x01, 0x80, 0x00, "Off"									},
	{0x00, 0x01, 0x80, 0x80, "On"									},

	{0   , 0xfe, 0   ,    2, "Coin A"								},
	{0x03, 0x01, 0x01, 0x00, "2 Coins 1 Credits"					},
	{0x03, 0x01, 0x01, 0x01, "1 Coin  1 Credits"					},

	{0   , 0xfe, 0   ,    4, "Coin B"								},
	{0x03, 0x01, 0x06, 0x04, "2 Coins 1 Credits"					},
	{0x03, 0x01, 0x06, 0x06, "1 Coin  1 Credits"					},
	{0x03, 0x01, 0x06, 0x02, "1 Coin  3 Credits"					},
	{0x03, 0x01, 0x06, 0x00, "1 Coin  5 Credits"					},

	{0   , 0xfe, 0   ,    2, "Lives per Credit"						},
	{0x03, 0x01, 0x10, 0x10, "2"									},
	{0x03, 0x01, 0x10, 0x00, "3"									},

	{0   , 0xfe, 0   ,    2, "Bonus Life"							},
	{0x03, 0x01, 0x20, 0x00, "None"									},
	{0x03, 0x01, 0x20, 0x20, "Mission 5"							},

	{0   , 0xfe, 0   ,    2, "Free Play"							},
	{0x03, 0x01, 0x40, 0x40, "Off"									},
	{0x03, 0x01, 0x40, 0x00, "On"									},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"							},
	{0x03, 0x01, 0x80, 0x00, "Off"									},
	{0x03, 0x01, 0x80, 0x80, "On"									},
};

static struct BurnDIPInfo Gorfpgm1fDIPList[]=
{
	{0x03, 0xff, 0xff, 0xe7, NULL									},

	{0   , 0xfe, 0   ,    2, "Language"								},
	{0x03, 0x01, 0x08, 0x08, "English"								},
	{0x03, 0x01, 0x08, 0x00, "French"								},
};

STDDIPINFOEXT(Gorfpgm1f, GorfBase, Gorfpgm1f)

static struct BurnDIPInfo Gorfpgm1gDIPList[]=
{
	{0x03, 0xff, 0xff, 0xe7, NULL									},

	{0   , 0xfe, 0   ,    2, "Language"								},
	{0x03, 0x01, 0x08, 0x08, "English"								},
	{0x03, 0x01, 0x08, 0x00, "German"								},
};

STDDIPINFOEXT(Gorfpgm1g, GorfBase, Gorfpgm1g)

static struct BurnDIPInfo GorfirecDIPList[]=
{
	{0x03, 0xff, 0xff, 0xef, NULL									},

	{0   , 0xfe, 0   ,    2, "Language"								},
	{0x03, 0x01, 0x08, 0x08, "Spanish"								},
	{0x03, 0x01, 0x08, 0x00, "Foreign (NEED ROM)"					},
};

STDDIPINFOEXT(Gorfirec, GorfBase, Gorfirec)

static struct BurnDIPInfo GorfDIPList[]=
{
	{0x03, 0xff, 0xff, 0xef, NULL									},

	{0   , 0xfe, 0   ,    2, "Language"								},
	{0x03, 0x01, 0x08, 0x08, "English"								},
	{0x03, 0x01, 0x08, 0x00, "Foreign (NEED ROM)"					},
};

STDDIPINFOEXT(Gorf, GorfBase, Gorf)

static struct BurnDIPInfo RobbyDIPList[]=
{
	DIP_OFFSET(0x10)
	{0x00, 0xff, 0xff, 0xff, NULL									},
	{0x01, 0xff, 0xff, 0xff, NULL									},
	{0x02, 0xff, 0xff, 0xff, NULL									},
	{0x03, 0xff, 0xff, 0x7f, NULL									},

	{0   , 0xfe, 0   ,    2, "Service Mode"							},
	{0x00, 0x01, 0x08, 0x08, "Off"									},
	{0x00, 0x01, 0x08, 0x00, "On"									},

	{0   , 0xfe, 0   ,    0, "Use NVRAM"							},
	{0x01, 0x01, 0x01, 0x00, "No"									},
	{0x01, 0x01, 0x01, 0x01, "Yes"									},

	{0   , 0xfe, 0   ,    2, "Use Service Mode Settings"			},
	{0x01, 0x01, 0x02, 0x00, "Reset"								},
	{0x01, 0x01, 0x02, 0x02, "Yes"									},

	{0   , 0xfe, 0   ,    2, "Free Play"							},
	{0x01, 0x01, 0x04, 0x04, "Off"									},
	{0x01, 0x01, 0x04, 0x00, "On"									},

	{0   , 0xfe, 0   ,    2, "Cabinet"								},
	{0x01, 0x01, 0x08, 0x08, "Upright"								},
	{0x01, 0x01, 0x08, 0x00, "Cocktail"								},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"							},
	{0x01, 0x01, 0x80, 0x00, "Off"									},
	{0x01, 0x01, 0x80, 0x80, "On"									},
};

STDDIPINFO(Robby)

static struct BurnDIPInfo SpacezapDIPList[]=
{
	DIP_OFFSET(0x10)
	{0x00, 0xff, 0xff, 0xff, NULL									},
	{0x01, 0xff, 0xff, 0xff, NULL									},
	{0x02, 0xff, 0xff, 0xff, NULL									},
	{0x03, 0xff, 0xff, 0xff, NULL									},

	{0   , 0xfe, 0   ,    2, "Service Mode"							},
	{0x00, 0x01, 0x08, 0x08, "Off"									},
	{0x00, 0x01, 0x08, 0x00, "On"									},

	{0   , 0xfe, 0   ,    2, "Lives"								},
	{0x01, 0x01, 0x20, 0x20, "3"									},
	{0x01, 0x01, 0x20, 0x00, "4"									},

	{0   , 0xfe, 0   ,    2, "Cabinet"								},
	{0x02, 0x01, 0x20, 0x20, "Upright"								},
	{0x02, 0x01, 0x20, 0x00, "Cocktail"								},

	{0   , 0xfe, 0   ,    2, "Coin A"								},
	{0x03, 0x01, 0x01, 0x00, "2 Coins 1 Credits"					},
	{0x03, 0x01, 0x01, 0x01, "1 Coin  1 Credits"					},

	{0   , 0xfe, 0   ,    4, "Coin B"								},
	{0x03, 0x01, 0x06, 0x04, "2 Coins 1 Credits"					},
	{0x03, 0x01, 0x06, 0x06, "1 Coin  1 Credits"					},
	{0x03, 0x01, 0x06, 0x02, "1 Coin  3 Credits"					},
	{0x03, 0x01, 0x06, 0x00, "1 Coin  5 Credits"					},
};

STDDIPINFO(Spacezap)

static struct BurnDIPInfo ProfpacDIPList[]=
{
	DIP_OFFSET(0x0c)
	{0x00, 0xff, 0xff, 0xff, NULL									},
	{0x01, 0xff, 0xff, 0x77, NULL									},
	{0x02, 0xff, 0xff, 0x00, NULL									},
	{0x03, 0xff, 0xff, 0x03, NULL									},

	{0   , 0xfe, 0   ,    2, "Service Mode"							},
	{0x00, 0x01, 0x04, 0x04, "Off"									},
	{0x00, 0x01, 0x04, 0x00, "On"									},

	{0   , 0xfe, 0   ,    2, "Cabinet"								},
	{0x03, 0x01, 0x01, 0x01, "Upright"								},
	{0x03, 0x01, 0x01, 0x00, "Cocktail"								},

	{0   , 0xfe, 0   ,    2, "Reset on powerup"						},
	{0x03, 0x01, 0x02, 0x02, "No"									},
	{0x03, 0x01, 0x02, 0x00, "Yes"									},

	{0   , 0xfe, 0   ,    2, "Halt on error"						},
	{0x03, 0x01, 0x04, 0x04, "No"									},
	{0x03, 0x01, 0x04, 0x00, "Yes"									},

	{0   , 0xfe, 0   ,    2, "Beep"									},
	{0x03, 0x01, 0x08, 0x08, "No"									},
	{0x03, 0x01, 0x08, 0x00, "Yes"									},

	{0   , 0xfe, 0   ,    2, "ROM's Used"							},
	{0x03, 0x01, 0x10, 0x10, "8K & 16K ROM's"						},
	{0x03, 0x01, 0x10, 0x00, "32K ROM's"							},
};

STDDIPINFO(Profpac)

static void profpac_update_line(int line)
{
	if (line >= nScreenHeight) return;

	UINT16 *profpac_videoram = (UINT16*)DrvVidRAM;
	UINT16 *sline = pTransDraw + line * nScreenWidth;

	for (INT32 i = 0; i < 80; i++) {
		for (INT32 j = 0; j < 4; j++) {
			sline[i * 4 + j] = profpac_videoram[profpac_vispage * 0x4000 + i + line * 80] >> ((3 - j) * 4) & 0x0f;
		}
	}
}

static int mame_vpos_to_astrocade_vpos(int scan_line)
{
	scan_line -= VERT_OFFSET;
	if (scan_line < 0)
		scan_line += 262;
	return scan_line;
}

static void common_update_line(int line)
{
	UINT8 const *const videoram = DrvVidRAM;
	UINT32 sparklebase = 0;
	const int colormask = (/*video_config & AC_MONITOR_BW*/ 0) ? 0 : 0x1f0;
	int xystep = 2 - video_mode;

	/* compute the starting point of sparkle for the current frame */
	int width = 352;
	int height = 240;

	if (has_stars)
		sparklebase = (nCurrentFrame * (UINT64)(width * height)) % RNG_PERIOD;

	int x_count = 0;

	/* iterate over scanlines */
	int y = line;
	{
		UINT16 *dest = bitmap + line * 352;
		int effy = mame_vpos_to_astrocade_vpos(y);
		UINT16 offset = (effy / xystep) * (80 / xystep);
		UINT32 sparkleoffs = 0, staroffs = 0;

		/* compute the star and sparkle offset at the start of this line */
		if (has_stars)
		{
			staroffs = ((effy < 0) ? (effy + 262) : effy) * width;
			sparkleoffs = sparklebase + y * width;
			if (sparkleoffs >= RNG_PERIOD)
				sparkleoffs -= RNG_PERIOD;
		}

		/* iterate over groups of 4 pixels */
		for (int x = 0; x < 456/4; x += xystep)
		{
			int effx = x - HORZ_OFFSET/4;
			const UINT8 *colorbase = &colors[(effx < colorsplit) ? 4 : 0];

			/* select either video data or background data */
			UINT8 data = (effx >= 0 && effx < 80 && effy >= 0 && effy < VerticalBlank) ? videoram[offset++] : BackgroundData;

			/* iterate over the 4 pixels */
			for (int xx = 0; xx < 4; xx++)
			{
				UINT8 pixdata = (data >> 6) & 3;
				int colordata = colorbase[pixdata] << 1;
				int luma = colordata & 0x0f;

				/* handle stars/sparkle */
				if (has_stars)
				{
					/* if sparkle is enabled for this pixel index and either it is non-zero or a star */
					/* then adjust the intensity */
					if (sparkle[pixdata] == 0)
					{
						if (pixdata != 0 || (sparklestar[staroffs] & 0x10))
							luma = sparklestar[sparkleoffs] & 0x0f;
						else if (pixdata == 0)
							colordata = luma = 0;
					}

					/* update sparkle/star offsets */
					staroffs++;
					if (++sparkleoffs >= RNG_PERIOD)
						sparkleoffs = 0;
				}
				UINT16 color = (colordata & colormask) | luma;

				/* store the final color to the destination and shift */
				*dest++ = color;
				x_count++;
				if (xystep == 2) {
					*dest++ = color;
					x_count++;
				}
				data <<= 2;
			}
		}
	}

	return;
}

static void common_interrupt()
{
	const INT32 derp = mame_vpos_to_astrocade_vpos(scanline);

	if ((InterruptFlag & 0x08) && (derp == NextScanInt)) {
		//bprintf(0, _T("raster INT @ %d  adj. %d\n"), scanline, derp);
		ZetSetVector(interrupt_vector & 0xff);
		if ((InterruptFlag & 0x04) == 0) {
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		} else {
			ZetSetIRQLine(0, CPU_IRQSTATUS_AUTO);
		}
	} else {
		if ((InterruptFlag & 0x12)) {
		//bprintf(0, _T("other INT @ %d  adj. %d\n"), scanline, derp);
			ZetSetVector(interrupt_vector & 0xf0);
			if ((InterruptFlag & 0x01) == 0) {
				ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
			} else {
				ZetSetIRQLine(0, CPU_IRQSTATUS_AUTO);
			}
			verti = derp;
			hori = 8;
		}
	}
}

static void increment_source(UINT8 curwidth, UINT8 *u13ff)
{
	/* if the flip-flop at U13 is high and mode.d2 is 1 we can increment */
	/* however, if mode.d3 is set and we're on the last byte of a row, the increment is suppressed */
	if (*u13ff && (pattern_mode & 0x04) != 0 && (curwidth != 0 || (pattern_mode & 0x08) == 0))
		pattern_source++;

	/* if mode.d1 is 1, toggle the flip-flop; otherwise leave it preset */
	if ((pattern_mode & 0x02) != 0)
		*u13ff ^= 1;
}


static void increment_dest(UINT8 curwidth)
{
	/* increment is suppressed for the last byte in a row */
	if (curwidth != 0)
	{
		/* if mode.d5 is 1, we increment */
		if ((pattern_mode & 0x20) != 0)
			pattern_dest++;

		/* otherwise, we decrement */
		else
			pattern_dest--;
	}
}


static void execute_blit()
{
	/*
	    pattern_source = counter set U7/U16/U25/U34
	    pattern_dest = counter set U9/U18/U30/U39
	    pattern_mode = latch U21
	    pattern_skip = latch set U30/U39
	    pattern_width = latch set U32/U41
	    pattern_height = counter set U31/U40

	    pattern_mode bits:
	        d0 = direction (0 = read from src, write to dest, 1 = read from dest, write to src)
	        d1 = expand (0 = increment src each pixel, 1 = increment src every other pixel)
	        d2 = constant (0 = never increment src, 1 = normal src increment)
	        d3 = flush (0 = copy all data, 1 = copy a 0 in place of last byte of each row)
	        d4 = dest increment direction (0 = decrement dest on carry, 1 = increment dest on carry)
	        d5 = dest direction (0 = increment dest, 1 = decrement dest)
	*/

	UINT8 curwidth; /* = counter set U33/U42 */
	UINT8 u13ff;    /* = flip-flop at U13 */
	int cycles = 0;

/*  logerror("Blit: src=%04X mode=%02X dest=%04X skip=%02X width=%02X height=%02X\n",
            pattern_source, pattern_mode, pattern_dest, pattern_skip, pattern_width, pattern_height);*/

	/* flip-flop at U13 is cleared at the beginning */
	u13ff = 0;

	/* it is also forced preset if mode.d1 == 0 */
	if ((pattern_mode & 0x02) == 0)
		u13ff = 1;

	/* loop over height */
	do
	{
		UINT16 carry;

		/* loop over width */
		curwidth = pattern_width;
		do
		{
			UINT16 busaddr;
			UINT8 busdata;

			/* ----- read phase ----- */

			/* address is selected between source/dest based on mode.d0 */
			busaddr = ((pattern_mode & 0x01) == 0) ? pattern_source : pattern_dest;

			/* if mode.d3 is set, then the last byte fetched per row is forced to 0 */
			if (curwidth == 0 && (pattern_mode & 0x08) != 0)
				busdata = 0;
			else
				busdata = ZetReadByte(busaddr);

			/* increment the appropriate address */
			if ((pattern_mode & 0x01) == 0)
				increment_source(curwidth, &u13ff);
			else
				increment_dest(curwidth);

			/* ----- write phase ----- */

			/* address is selected between source/dest based on mode.d0 */
			busaddr = ((pattern_mode & 0x01) != 0) ? pattern_source : pattern_dest;
			ZetWriteByte(busaddr, busdata);

			/* increment the appropriate address */
			if ((pattern_mode & 0x01) == 0)
				increment_dest(curwidth);
			else
				increment_source(curwidth, &u13ff);

			/* count 4 cycles (two read, two write) */
			cycles += 4;

		} while (curwidth-- != 0);

		/* at the end of each row, the skip value is added to the dest value */
		carry = ((pattern_dest & 0xff) + pattern_skip) & 0x100;
		pattern_dest = (pattern_dest & 0xff00) | ((pattern_dest + pattern_skip) & 0xff);

		/* carry behavior into the top byte is controlled by mode.d4 */
		if ((pattern_mode & 0x10) == 0)
			pattern_dest += carry;
		else
			pattern_dest -= carry ^ 0x100;

	} while (pattern_height-- != 0);

	/* count cycles we ran the bus */
	Z80Burn(cycles);
}


static void astrocade_pattern_board_write(INT32 offset, UINT8 data)
{
	switch (offset)
	{
		case 0:     /* source offset low 8 bits */
			pattern_source = (pattern_source & 0xff00) | (data << 0);
			break;

		case 1:     /* source offset upper 8 bits */
			pattern_source = (pattern_source & 0x00ff) | (data << 8);
			break;

		case 2:     /* mode control; also clears low byte of dest */
			pattern_mode = data & 0x3f;
			pattern_dest &= 0xff00;
			break;

		case 3:     /* skip value */
			pattern_skip = data;
			break;

		case 4:     /* dest offset upper 8 bits; also adds skip to low 8 bits */
			pattern_dest = ((pattern_dest + pattern_skip) & 0xff) | (data << 8);
			break;

		case 5:     /* width of blit */
			pattern_width = data;
			break;

		case 6:     /* height of blit and initiator */
			pattern_height = data;
			execute_blit();
			break;
	}
}


static void astrocade_funcgen_write(INT32 offset, UINT8 data)
{
	UINT8 prev_data;

	/* control register:
	    bit 0 = shift amount LSB
	    bit 1 = shift amount MSB
	    bit 2 = rotate
	    bit 3 = expand
	    bit 4 = OR
	    bit 5 = XOR
	    bit 6 = flop
	*/

	/* expansion */
	if (funcgen_control & 0x08)
	{
		funcgen_expand_count ^= 1;
		data >>= 4 * funcgen_expand_count;
		data =  (funcgen_expand_color[(data >> 3) & 1] << 6) |
				(funcgen_expand_color[(data >> 2) & 1] << 4) |
				(funcgen_expand_color[(data >> 1) & 1] << 2) |
				(funcgen_expand_color[(data >> 0) & 1] << 0);
	}
	prev_data = funcgen_shift_prev_data;
	funcgen_shift_prev_data = data;

	/* rotate or shift */
	if (funcgen_control & 0x04)
	{
		/* rotate */

		/* first 4 writes accumulate data for the rotate */
		if ((funcgen_rotate_count & 4) == 0)
		{
			funcgen_rotate_data[funcgen_rotate_count++ & 3] = data;
			return;
		}

		/* second 4 writes actually write it */
		else
		{
			UINT8 shift = 2 * (~funcgen_rotate_count++ & 3);
			data =  (((funcgen_rotate_data[3] >> shift) & 3) << 6) |
					(((funcgen_rotate_data[2] >> shift) & 3) << 4) |
					(((funcgen_rotate_data[1] >> shift) & 3) << 2) |
					(((funcgen_rotate_data[0] >> shift) & 3) << 0);
		}
	}
	else
	{
		/* shift */
		UINT8 shift = 2 * (funcgen_control & 0x03);
		data = (data >> shift) | (prev_data << (8 - shift));
	}

	/* flopping */
	if (funcgen_control & 0x40)
		data = (data >> 6) | ((data >> 2) & 0x0c) | ((data << 2) & 0x30) | (data << 6);

	/* OR/XOR */
	if (funcgen_control & 0x30)
	{
		UINT8 olddata = ZetReadByte(0x4000 + offset);

		/* compute any intercepts */
		funcgen_intercept &= 0x0f;
		if ((olddata & 0xc0) && (data & 0xc0))
			funcgen_intercept |= 0x11;
		if ((olddata & 0x30) && (data & 0x30))
			funcgen_intercept |= 0x22;
		if ((olddata & 0x0c) && (data & 0x0c))
			funcgen_intercept |= 0x44;
		if ((olddata & 0x03) && (data & 0x03))
			funcgen_intercept |= 0x88;

		/* apply the operation */
		if (funcgen_control & 0x10)
			data |= olddata;
		else if (funcgen_control & 0x20)
			data ^= olddata;
	}

	/* write the result */
	ZetWriteByte(0x4000 + offset, data);
}


static void __fastcall common_write(UINT16 address, UINT8 data)
{
	if (address <= 0x3fff)
	{
		astrocade_funcgen_write(address, data);
		return;
	}

	// robby requires this?
	if (address >= 0xe000) {
		if (ram_write_enable) {
			DrvZ80RAM[address & 0x1fff] = data;
			ram_write_enable = 0;
		}
		return;
	}

	bprintf (0, _T("WM: %4.4x, %2.2x\n"), address, data);
}

static UINT8 __fastcall common_read(UINT16 address)
{
	bprintf (0, _T("RM: %4.4x\n"), address);

	return 0;
}

static void __fastcall common_port_write(UINT16 port, UINT8 data)
{
	if (port == 0xa55b) {
		ram_write_enable = 1;
		return;
	}

	if ((port & 0xf8) == 0x00) {
		colors[port & 0x7] = data;
		return;
	}

	if ((port & 0xff) >= 0x10 && (port & 0xff) <= 0x18 && has_snd) {
		astrocde_snd_write(0, port, data);
		return;
	}
	if ((port & 0xff) >= 0x50 && (port & 0xff) <= 0x58 && has_snd == 2) {
		astrocde_snd_write(1, port, data);
		return;
	}

	switch (port & 0xff)
	{
		case 0x08:
			video_mode = data & 0x01;
		return;

		case 0x09:
			colorsplit = 2 * (data & 0x3f);
			BackgroundData = ((data&0xc0) >> 6) * 0x55;
		return;

		case 0x0a:
			VerticalBlank = data;
		return;

		case 0x0b:
			colors[(port >> 8) & 7] = data;
		return;

		case 0x0c:
			funcgen_control = data;
			funcgen_expand_count = 0;     /* reset flip-flop for expand mode on write to this register */
			funcgen_rotate_count = 0;     /* reset counter for rotate mode on write to this register */
			funcgen_shift_prev_data = 0;  /* reset shift buffer on write to this register */
		return;

		case 0x0d:
			interrupt_vector = data;
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;

		case 0x0e:
			InterruptFlag = data;
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;

		case 0x0f:
			NextScanInt = data;
			//bprintf(0, _T("next scanINT @ %d   slNOW: %d\n"), data, scanline);
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;

		case 0x19:
			funcgen_expand_color[0] = data & 0x03;
			funcgen_expand_color[1] = (data >> 2) & 0x03;
		return;

		case 0x78:
		case 0x79:
		case 0x7a:
		case 0x7b:
		case 0x7c:
		case 0x7d:
		case 0x7e:
			astrocade_pattern_board_write(port & 0x7, data);
		return;
	}
	bprintf (0, _T("WP: %4.4x, %2.2x\n"), port, data);
}

static UINT8 __fastcall common_port_read(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x08:
		{
			UINT8 res = funcgen_intercept;
			funcgen_intercept = 0;
			return res;
		}

		case 0x0e:
			return verti;
		case 0x0f:
			return hori;
		case 0x10:
			return DrvInputs[0];
		case 0x11:
			return DrvInputs[1];
		case 0x12:
			return DrvInputs[2];
		case 0x13:
			return DrvInputs[3];
	}
	
	bprintf (0, _T("CRP: %4.4x\n"), port);

	return 0;
}

static void seawolf2_samples_all_vol(float vol)
{
	for (int i = 0; i <= 9; i++) {
		BurnSampleSetAllRoutesFade(i*2+0, vol, BURN_SND_ROUTE_LEFT);
		BurnSampleSetAllRoutesFade(i*2+1, vol, BURN_SND_ROUTE_RIGHT);
	}
}

static void __fastcall seawolf2_write_port(UINT16 port, UINT8 data)
{
	UINT8 rising = 0;
	switch (port & 0xff)
	{
		case 0x40:
			rising = data & ~sound_p0_last;
			sound_p0_last = data;

			if (rising & 0x01) BurnSamplePlay(2); // left torpedo
			if (rising & 0x02) BurnSamplePlay(0); // left shiphit
			if (rising & 0x04) BurnSamplePlay(8); // left minehit
			if (rising & 0x08) BurnSamplePlay(3); // right torpedo
			if (rising & 0x10) BurnSamplePlay(1); // right shiphit
			if (rising & 0x20) BurnSamplePlay(9); // right minehit
		return;

		case 0x41:
			rising = data & ~sound_p1_last;
			sound_p1_last = data;

			seawolf2_samples_all_vol((data & 0x80) ? 0.25 : 0.00);

			if (rising & 0x08) {
				BurnSamplePlay(4); // dive starts
				BurnSamplePlay(5); // dive starts
			}
			BurnSampleSetAllRoutesFade(4, (float)(~data & 7) / 7.0, BURN_SND_ROUTE_LEFT);
			BurnSampleSetAllRoutesFade(5, (float)(data & 7) / 7.0, BURN_SND_ROUTE_RIGHT);
			if (rising & 0x10) if (BurnSampleGetStatus(6) != SAMPLE_PLAYING) BurnSamplePlay(6); // left sonar
			if (rising & 0x20) if (BurnSampleGetStatus(7) != SAMPLE_PLAYING) BurnSamplePlay(7); // right sonar
			// coin counter (0x40)
		return;

		case 0x42:
		case 0x43:
			lamps[port & 1] = data;
			// lamps
		return;
	}

	if ((port & 0xff) < 0x20) {
		common_port_write(port, data);
	}
}

static UINT8 __fastcall spacezap_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x12:
		{
			// coin counter 0 = (offset & 0x100) >> 8
			// coin counter 1 = (offset & 0x200) >> 9
			return DrvInputs[2];
		}
	}

	return common_port_read(port);
}

static UINT8 __fastcall ebases_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x13:
			return BurnTrackballRead(trackball_select ^ 2);
	}

	return common_port_read(port);
}

static void __fastcall ebases_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xf8)
	{
		case 0x20:
			// coin counter = data & 1
		return;

		case 0x28:
			trackball_select = data & 3;
			BurnTrackballReadReset();
		return;
	}

	common_port_write(port, data);
}

static void speech_write(UINT8 data)
{
	sc01_set_clock((data & 0x40) ? 782000 : 756000);
	sc01_inflection_write((data & 0x80) ? 0 : 2);
	sc01_write(data & 0x3f);
}

static UINT8 __fastcall gorf_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x12:
		{
			UINT8 ret = DrvInputs[2] & 0x7f; // port 2
			if (sc01_read_request())
				ret |= 0x80;
			return ret;
		}
		
		case 0x15:
		{
			UINT8 data = (port >> 8) & 1;
			int offset = (port >> 9);
			switch (offset)
			{
				case 0: break; // coin counter 1
				case 1: break; // coin counter 2
				case 2:
				case 3:
				case 4:
				case 5: sparkle[offset - 2] = data; break;
				case 7: break; // wow - coin counter 3
			}

			return 0;
		}

		case 0x16:
			return 0; // lamps - don't  bother
		case 0x17:
			speech_write(port >> 8);
			return port >> 8;
	}

	return common_port_read(port);
}

static UINT8 __fastcall wow_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x12:
		{
			int ret = DrvInputs[2] & 0x7f; // port 2
			if (sc01_read_request())
				ret |= 0x80;
			return ret;
		}
		
		case 0x15:
		{
			UINT8 data = (port >> 8) & 1;
			int offset = (port >> 9) & 7;
			switch (offset)
			{
				case 0: break; // coin counter 1
				case 1: break; // coin counter 2
				case 2:
				case 3:
				case 4:
				case 5: sparkle[offset - 2] = data; break;
				case 7: break; // coin counter 3
			}
			return 0;
		}

		case 0x17: // reading writes the port msb to votrax
			speech_write(port >> 8);
			return port >> 8;
	}

	return common_port_read(port);
}

static UINT8 __fastcall robby_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x15:
		{
		 //   UINT8 data = ( port >> 8) & 1;
			int offset = ( port >> 9) & 7;
			switch (offset)
			{
				case 0: break; // coin counter 1
				case 1: break; // coin counter 2
				case 2: break; // coin counter 3
				case 3:
				case 4:
				case 6: break; // set led status - 0
				case 7: break; // set led status - 1
			}
			return 0;
		}
	}

	return common_port_read(port);
}

/* All this information comes from decoding the PLA at U39 on the screen ram board */
static void profpac_videoram_write(UINT16 offset, UINT8 data)
{
	UINT16 *profpac_videoram = (UINT16*)DrvVidRAM;
	UINT16 oldbits = profpac_videoram[profpac_writepage * 0x4000 + offset];
	UINT16 newbits, result = 0;

	/* apply the 2->4 bit expansion first */
	newbits = (profpac_color_mapping[(data >> 6) & 3] << 12) |
				(profpac_color_mapping[(data >> 4) & 3] << 8) |
				(profpac_color_mapping[(data >> 2) & 3] << 4) |
				(profpac_color_mapping[(data >> 0) & 3] << 0);

	/* there are 4 write modes: overwrite, xor, overlay, or underlay */
	switch (profpac_writemode)
	{
		case 0:     /* normal write */
			result = newbits;
			break;

		case 1:     /* xor write */
			result = newbits ^ oldbits;
			break;

		case 2:     /* overlay write */
			result  = ((newbits & 0xf000) == 0) ? (oldbits & 0xf000) : (newbits & 0xf000);
			result |= ((newbits & 0x0f00) == 0) ? (oldbits & 0x0f00) : (newbits & 0x0f00);
			result |= ((newbits & 0x00f0) == 0) ? (oldbits & 0x00f0) : (newbits & 0x00f0);
			result |= ((newbits & 0x000f) == 0) ? (oldbits & 0x000f) : (newbits & 0x000f);
			break;

		case 3: /* underlay write */
			result  = ((oldbits & 0xf000) != 0) ? (oldbits & 0xf000) : (newbits & 0xf000);
			result |= ((oldbits & 0x0f00) != 0) ? (oldbits & 0x0f00) : (newbits & 0x0f00);
			result |= ((oldbits & 0x00f0) != 0) ? (oldbits & 0x00f0) : (newbits & 0x00f0);
			result |= ((oldbits & 0x000f) != 0) ? (oldbits & 0x000f) : (newbits & 0x000f);
			break;
	}

	/* apply the write mask and store */
	result = (result & profpac_writemask) | (oldbits & ~profpac_writemask);
	profpac_videoram[profpac_writepage * 0x4000 + offset] = result;

	/* Intercept (collision) stuff */

	/* There are 3 bits on the register, which are set by various combinations of writes */
	if (((oldbits & 0xf000) == 0x2000 && (newbits & 0x8000) == 0x8000) ||
		((oldbits & 0xf000) == 0x3000 && (newbits & 0xc000) == 0x4000) ||
		((oldbits & 0x0f00) == 0x0200 && (newbits & 0x0800) == 0x0800) ||
		((oldbits & 0x0f00) == 0x0300 && (newbits & 0x0c00) == 0x0400) ||
		((oldbits & 0x00f0) == 0x0020 && (newbits & 0x0080) == 0x0080) ||
		((oldbits & 0x00f0) == 0x0030 && (newbits & 0x00c0) == 0x0040) ||
		((oldbits & 0x000f) == 0x0002 && (newbits & 0x0008) == 0x0008) ||
		((oldbits & 0x000f) == 0x0003 && (newbits & 0x000c) == 0x0004))
		profpac_intercept |= 0x01;

	if (((newbits & 0xf000) != 0x0000 && (oldbits & 0xc000) == 0x4000) ||
		((newbits & 0x0f00) != 0x0000 && (oldbits & 0x0c00) == 0x0400) ||
		((newbits & 0x00f0) != 0x0000 && (oldbits & 0x00c0) == 0x0040) ||
		((newbits & 0x000f) != 0x0000 && (oldbits & 0x000c) == 0x0004))
		profpac_intercept |= 0x02;

	if (((newbits & 0xf000) != 0x0000 && (oldbits & 0x8000) == 0x8000) ||
		((newbits & 0x0f00) != 0x0000 && (oldbits & 0x0800) == 0x0800) ||
		((newbits & 0x00f0) != 0x0000 && (oldbits & 0x0080) == 0x0080) ||
		((newbits & 0x000f) != 0x0000 && (oldbits & 0x0008) == 0x0008))
		profpac_intercept |= 0x04;
}

static UINT8 profpac_videoram_read(UINT16 offset)
{
	const UINT16 *profpac_videoram = (UINT16*)DrvVidRAM;
	UINT16 temp = profpac_videoram[profpac_readpage * 0x4000 + offset] >> profpac_readshift;
	return ((temp >> 6) & 0xc0) | ((temp >> 4) & 0x30) | ((temp >> 2) & 0x0c) | ((temp >> 0) & 0x03);
}

static void profpac_screenram_ctrl_write(UINT8 offset, UINT8 data)
{
	int select = (data >> 4) & 0x0f;

	switch (offset)
	{
		case 0:     /* port 0xC0 - red component */
			profpac_palram[select] = (profpac_palram[select] & 0x0ff) | ((data & 0x0f) << 8);
			DrvPalette[select] = BurnHighCol(pal4bit((profpac_palram[select] >> 8) & 0xf), pal4bit((profpac_palram[select] >> 4) & 0xf), pal4bit((profpac_palram[select] >> 0) & 0xf), 0);
			break;

		case 1:     /* port 0xC1 - green component */
			profpac_palram[select] = (profpac_palram[select] & 0xf0f) | ((data & 0x0f) << 4);
			DrvPalette[select] = BurnHighCol(pal4bit((profpac_palram[select] >> 8) & 0xf), pal4bit((profpac_palram[select] >> 4) & 0xf), pal4bit((profpac_palram[select] >> 0) & 0xf), 0);
			break;

		case 2:     /* port 0xC2 - blue component */
			profpac_palram[select] = (profpac_palram[select] & 0xff0) | ((data & 0x0f) << 0);
			DrvPalette[select] = BurnHighCol(pal4bit((profpac_palram[select] >> 8) & 0xf), pal4bit((profpac_palram[select] >> 4) & 0xf), pal4bit((profpac_palram[select] >> 0) & 0xf), 0);
			break;

		case 3:     /* port 0xC3 - set 2bpp to 4bpp mapping and clear intercepts */
			profpac_color_mapping[(data >> 4) & 3] = data & 0x0f;
			profpac_intercept = 0x00;
			break;

		case 4:     /* port 0xC4 - which half to read on a memory access */
			profpac_vw = data & 0x0f; /* refresh write enable lines TBD */
			profpac_readshift = 2 * ((data >> 4) & 1);
			break;

		case 5:     /* port 0xC5 - write enable and write mode */
			profpac_writemask = ((data & 0x0f) << 12) | ((data & 0x0f) << 8) | ((data & 0x0f) << 4) | ((data & 0x0f) << 0);
			profpac_writemode = (data >> 4) & 0x03;
			break;
	}
}

static void __fastcall profpac_write(UINT16 address, UINT8 data)
{
	if (address <= 0x3fff) {
		astrocade_funcgen_write(address, data);
		return;
	}

	if (address >= 0x4000 && address <= 0x7fff) {
		profpac_videoram_write(address & 0x3fff, data);
	}
}

static UINT8 __fastcall profpac_read(UINT16 address)
{
	if (address >= 0x4000 && address <= 0x7fff) {
		return profpac_videoram_read(address & 0x3fff);
	}

	return 0;
}

static void demndrgn_bank(UINT8 data)
{
	profpac_bankdata = data;

	data = (data >> 5) & 3;
	ZetMapMemory(DrvZ80ROM + 0x14000 + (data * 0x8000), 0x8000, 0xbfff, MAP_ROM);
	if (data == 0) {
		// when 0, we read from video ram via standard handler
		ZetUnmapMemory(0x4000, 0x7fff, 0xff);
	} else {
		ZetMapMemory(DrvZ80ROM + 0x10000 + data * 0x8000, 0x4000, 0x7fff, MAP_ROM);
	}
}

static void profpac_bank(UINT8 data)
{
	demndrgn_bank(data);

	profpac_bankdata = data;

	if (data & 0x80) {
		data &= 0x7f;

		ZetMapMemory(DrvEPROMS + ((data < 0x28) ? data : 0x28) * 0x4000, 0x4000, 0x7fff, MAP_ROM);
	}
}

static void __fastcall profpac_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0xbf:
			profpac_readpage = data & 3;
			profpac_writepage = (data >> 2) & 3;
			profpac_vispage = (data >> 4) & 3;
		return;

		case 0xc0:
		case 0xc1:
		case 0xc2:
		case 0xc3:
		case 0xc4:
		case 0xc5:
			profpac_screenram_ctrl_write(port & 7, data);
		return;

		case 0xf3:
			profpac_bank(data);

		return;
	}

	common_port_write(port, data);
}

static UINT8 __fastcall profpac_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x14:
		//	coin counters & leds
			return 0;

		case 0x15:
		// leds
			return 0;

		case 0xc3:
			return profpac_intercept;
	}

	return common_port_read(port);
}

static void __fastcall demndrgn_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x97:
			// no sound board?
		return;

		case 0xbf:
			profpac_readpage = data & 3;
			profpac_writepage = (data >> 2) & 3;
			profpac_vispage = (data >> 4) & 3;
		return;

		case 0xc0:
		case 0xc1:
		case 0xc2:
		case 0xc3:
		case 0xc4:
		case 0xc5:
			profpac_screenram_ctrl_write(port & 7, data);
		return;

		case 0xf3:
			demndrgn_bank(data);
		return;
	}

	common_port_write(port, data);
}

static UINT8 __fastcall demndrgn_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x11:
			return BurnTrackballRead(trackball_select);

		case 0x14:
			// coin counter & leds as well
			trackball_select = (port >> 12) & 1;
			BurnTrackballReadReset();
			return 0;

		case 0x1c:
			return ProcessAnalog(Analog[2], 1, 1, 0x00, 0xff);
		case 0x1d:
			return ProcessAnalog(Analog[3], 1, 1, 0x00, 0xff);

		case 0xc3:
			return profpac_intercept;
	}

	return common_port_read(port);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	if (has_snd) astrocde_snd_reset();
	if (has_sc01) sc01_reset();
	if (has_sam) BurnSampleReset();

	interrupt_vector = 0;
	NextScanInt = 0;
	InterruptFlag = 0;
	VerticalBlank = 0;
	BackgroundData = 0;

	ram_write_enable = 0;

	trackball_select = 0;

	profpac_bankdata = 0;
	profpac_readshift = 0;
	profpac_readpage = 0;
	profpac_vispage = 0;
	profpac_writepage = 0;
	profpac_writemode = 0;
	profpac_intercept = 0;
	profpac_vw = 0;

	cart_bank = 0;

	memset(profpac_palram, 0, sizeof(profpac_palram));;

	memset(profpac_color_mapping, 0, sizeof(profpac_color_mapping));

	memset(sparkle, 0, sizeof(sparkle));

	sound_p0_last = 0;
	sound_p1_last = 0;
	lamps[0] = lamps[1] = 0;

	nExtraCycles = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x030000;
	if (is_home) { DrvCartROM = Next; Next += 0x080000; }
	DrvEPROMS		= Next; Next += 0x0a4000; // roms are smaller, but can bank up to a0000

	DrvPalette		= (UINT32*)Next; Next += 0x0300 * sizeof(UINT32);

	AllRam			= Next;

	DrvVidRAM		= Next; Next += 0x004000 * 2 * 4; // big for profpac
	DrvZ80RAM		= Next; Next += 0x002000;

	RamEnd			= Next;

	sparklestar     = Next; Next += RNG_PERIOD;

	bitmap          = (UINT16*)Next; Next += 352*262*2;

	MemEnd			= Next;

	return 0;
}

static void init_star_field()
{
	UINT32 shiftreg;
	int i;

	/* reset global sparkle state */
	sparkle[0] = sparkle[1] = sparkle[2] = sparkle[3] = 0;

	/* generate the data for the sparkle/star array */
	for (shiftreg = i = 0; i < RNG_PERIOD; i++)
	{
		UINT8 newbit;

		/* clock the shift register */
		newbit = ((shiftreg >> 12) ^ ~shiftreg) & 1;
		shiftreg = (shiftreg >> 1) | (newbit << 16);

		/* extract the sparkle/star intensity here */
		/* this is controlled by the shift register at U17/U19/U18 */
		sparklestar[i] =    (((shiftreg >> 4) & 1) << 3) |
							(((shiftreg >> 12) & 1) << 2) |
							(((shiftreg >> 16) & 1) << 1) |
							(((shiftreg >> 8) & 1) << 0);

		/* determine the star enable here */
		/* this is controlled by the shift register at U17/U12/U11 */
		if ((shiftreg & 0xff) == 0xfe)
			sparklestar[i] |= 0x10;
	}
}

static INT32 DrvLoadROMs()
{
	char* pRomName;
	struct BurnRomInfo ri;
	UINT8 *pLoad = DrvZ80ROM;
	UINT8 *eLoad = DrvEPROMS;

	memset (eLoad, 0xff, 0xa4000);

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {

		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & BRF_PRG) && (ri.nType & 7) == 1) {
			//bprintf(0, _T("loading rom %d @ %x\n"), i, (int)(pLoad - DrvZ80ROM));
			if (BurnLoadRom(pLoad, i, 1)) return 1;
			pLoad += ri.nLen;
			if ((pLoad - DrvZ80ROM) == 0x4000) pLoad += 0x4000;
			if ((pLoad - DrvZ80ROM) == 0xe000) pLoad += 0x6000;
			if (is_wow && (pLoad - DrvZ80ROM) == 0xb000) pLoad += 0x1000; // German rom @ 0xc000
			continue;
		}

		if ((ri.nType & BRF_PRG) && (ri.nType & 7) == 2) {
			if (BurnLoadRom(eLoad, i, 1)) return 1;
			eLoad += ri.nLen;
			continue;
		}

		if ((ri.nType & BRF_PRG) && (ri.nType & 7) == 3) {
			if (BurnLoadRom(DrvCartROM, i, 1)) return 1;
			cart_size = ri.nLen;
			cart_type = (cart_size > 0x40000) ? CART_512K
						: (cart_size > 0x8000)  ? CART_256K
						: CART_STD;
			continue;
		}
	}

	return 0;
}

static INT32 Seawolf2Init()
{
	BurnAllocMemIndex();

	if (DrvLoadROMs()) return 1;

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,				0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvVidRAM,				0x4000, 0x7fff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM,				0xc000, 0xc3ff, MAP_RAM);
	ZetSetWriteHandler(common_write);
	ZetSetReadHandler(common_read);
	ZetSetOutHandler(seawolf2_write_port);
	ZetSetInHandler(common_port_read);
	ZetClose();

	update_line_cb = common_update_line;

	BurnSampleInit(0);
	seawolf2_samples_all_vol(0.00); // start muted
	BurnSampleSetBuffered(ZetTotalCycles, 3579545);

	has_sam = 1;
	is_seawolf2 = 1;

	y_offset = 24;

	GenericTilesInit();

	BurnGunInit(2, 1);
	BurnGunSetHideTime(20 * 60);
	BurnGunSetCoords(0, nScreenWidth/2, 44);
	BurnGunSetCoords(1, nScreenWidth/2, 44);

	DrvDoReset();

	return 0;
}

static INT32 EbasesInit()
{
	BurnAllocMemIndex();

	if (DrvLoadROMs()) return 1;

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,				0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvVidRAM,				0x4000, 0x7fff, MAP_RAM);
	ZetMapMemory(DrvZ80ROM + 0x8000,	0x8000, 0xcfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,				0xd000, 0xdfff, MAP_RAM);
	ZetSetWriteHandler(common_write);
	ZetSetReadHandler(common_read);
	ZetSetOutHandler(ebases_write_port);
	ZetSetInHandler(ebases_read_port);
	ZetClose();

	update_line_cb = common_update_line;
	has_tball = 1;
	BurnTrackballInit(2);

	astrocde_snd_init(0, BURN_SND_ROUTE_BOTH);
	astrocde_snd_set_buffered(ZetTotalCycles, 1789772);
	has_snd = 1;

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 SpacezapInit()
{
	BurnAllocMemIndex();

	if (DrvLoadROMs()) return 1;

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,				0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvVidRAM,				0x4000, 0x7fff, MAP_RAM);
	ZetMapMemory(DrvZ80ROM + 0x8000,	0x8000, 0xcfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,				0xd000, 0xdfff, MAP_RAM);
	ZetSetWriteHandler(common_write);
	ZetSetReadHandler(common_read);
	ZetSetOutHandler(common_port_write);
	ZetSetInHandler(spacezap_read_port);
	ZetClose();

	update_line_cb = common_update_line;
	y_offset = 16;

	astrocde_snd_init(0, BURN_SND_ROUTE_BOTH);
	astrocde_snd_set_buffered(ZetTotalCycles, 1789772);
	has_snd = 1;

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 GorfInit()
{
	BurnAllocMemIndex();

	if (DrvLoadROMs()) return 1;

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,				0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvVidRAM,				0x4000, 0x7fff, MAP_RAM);
	ZetMapMemory(DrvZ80ROM + 0x8000,	0x8000, 0xcfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,				0xd000, 0xdfff, MAP_RAM);
	ZetSetWriteHandler(common_write);
	ZetSetReadHandler(common_read);
	ZetSetOutHandler(common_port_write);
	ZetSetInHandler(gorf_read_port);
	ZetClose();

	update_line_cb = common_update_line;
	init_star_field();

	astrocde_snd_init(0, BURN_SND_ROUTE_BOTH);
	astrocde_snd_init(1, BURN_SND_ROUTE_BOTH);
	astrocde_snd_set_buffered(ZetTotalCycles, 1789772);
	astrocde_snd_set_volume(0.65);
	sc01_init(756000, NULL, 0);
	sc01_set_buffered(ZetTotalCycles, 1789772);
	sc01_set_volume(1.30);
	has_sc01 = 1;
	has_snd = 2;
	has_stars = 1;

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 WowInit()
{
	BurnAllocMemIndex();

	is_wow = 1;

	if (DrvLoadROMs()) return 1;

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,				0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvVidRAM,				0x4000, 0x7fff, MAP_RAM);
	ZetMapMemory(DrvZ80ROM + 0x8000,	0x8000, 0xcfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,				0xd000, 0xdfff, MAP_RAM);
	ZetSetWriteHandler(common_write);
	ZetSetReadHandler(common_read);
	ZetSetOutHandler(common_port_write);
	ZetSetInHandler(wow_read_port);
	ZetClose();

	update_line_cb = common_update_line;
	init_star_field();

	astrocde_snd_init(0, BURN_SND_ROUTE_PANLEFT);
	astrocde_snd_init(1, BURN_SND_ROUTE_PANRIGHT);
	astrocde_snd_set_buffered(ZetTotalCycles, 1789772);
	sc01_init(756000, NULL, 0);
	sc01_set_buffered(ZetTotalCycles, 1789772);
	has_sc01 = 1;
	has_snd = 2;
	has_stars = 1;

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 RobbyInit()
{
	BurnAllocMemIndex();

	if (DrvLoadROMs()) return 1;

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,				0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvVidRAM,				0x4000, 0x7fff, MAP_RAM);
	ZetMapMemory(DrvZ80ROM + 0x8000,	0x8000, 0xdfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,				0xe000, 0xe3ff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM + 0x400,		0xe400, 0xffff, MAP_RAM);
	ZetSetWriteHandler(common_write);
	ZetSetReadHandler(common_read);
	ZetSetOutHandler(common_port_write);
	ZetSetInHandler(robby_read_port);
	ZetClose();

	update_line_cb = common_update_line;

	astrocde_snd_init(0, BURN_SND_ROUTE_PANLEFT);
	astrocde_snd_init(1, BURN_SND_ROUTE_PANRIGHT);
	astrocde_snd_set_buffered(ZetTotalCycles, 1789772);
	has_snd = 2;

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 ProfpacInit()
{
	BurnAllocMemIndex();

	{
		int k = 0;
		if (BurnLoadRom(DrvZ80ROM + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x02000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x0c000, k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM + 0x14000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x16000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x18000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x1a000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x1c000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x1e000, k++, 1)) return 1;

		memset(DrvEPROMS, 0xff, 0xa0000); // a0000 - a3fff == 0x00

		if (BurnLoadRom(DrvEPROMS + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvEPROMS + 0x04000, k++, 1)) return 1;
		if (BurnLoadRom(DrvEPROMS + 0x08000, k++, 1)) return 1;
		if (BurnLoadRom(DrvEPROMS + 0x0c000, k++, 1)) return 1;
		if (BurnLoadRom(DrvEPROMS + 0x10000, k++, 1)) return 1;
		if (BurnLoadRom(DrvEPROMS + 0x14000, k++, 1)) return 1;
		if (BurnLoadRom(DrvEPROMS + 0x18000, k++, 1)) return 1;
		if (BurnLoadRom(DrvEPROMS + 0x1c000, k++, 1)) return 1;
		if (BurnLoadRom(DrvEPROMS + 0x20000, k++, 1)) return 1;
		if (BurnLoadRom(DrvEPROMS + 0x24000, k++, 1)) return 1;
		if (BurnLoadRom(DrvEPROMS + 0x28000, k++, 1)) return 1;
		if (BurnLoadRom(DrvEPROMS + 0x2c000, k++, 1)) return 1;
		if (BurnLoadRom(DrvEPROMS + 0x30000, k++, 1)) return 1;
		if (BurnLoadRom(DrvEPROMS + 0x34000, k++, 1)) return 1;
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,				0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80ROM + 0xc000,	0xc000, 0xdfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,				0xe000, 0xffff, MAP_RAM);
	ZetSetWriteHandler(profpac_write);
	ZetSetReadHandler(profpac_read);
	ZetSetOutHandler(profpac_write_port);
	ZetSetInHandler(profpac_read_port);
	ZetClose();

	update_line_cb = profpac_update_line;

	astrocde_snd_init(0, BURN_SND_ROUTE_PANLEFT);
	astrocde_snd_init(1, BURN_SND_ROUTE_PANRIGHT);
	astrocde_snd_set_buffered(ZetTotalCycles, 1789772);
	has_snd = 2;

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DemndrgnInit()
{
	BurnAllocMemIndex();

	{
		int k = 0;
		if (BurnLoadRom(DrvZ80ROM + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x02000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x0c000, k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM + 0x18000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x1a000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x1c000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x20000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x22000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x24000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x26000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x28000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x2a000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x2c000, k++, 1)) return 1;
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,				0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80ROM + 0xc000,	0xc000, 0xdfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,				0xe000, 0xffff, MAP_RAM);
	ZetSetWriteHandler(profpac_write);
	ZetSetReadHandler(profpac_read);
	ZetSetOutHandler(demndrgn_write_port);
	ZetSetInHandler(demndrgn_read_port);
	ZetClose();

	update_line_cb = profpac_update_line;

	has_tball = 1;
	BurnTrackballInit(2);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	ZetExit();

	if (has_snd) astrocde_snd_exit();
	if (has_sc01) sc01_exit();
	if (has_tball || is_home) BurnTrackballExit();
	if (has_sam) BurnSampleExit();
	if (is_seawolf2) BurnGunExit();

	has_sc01 = 0;
	has_snd = 0;
	has_sam = 0;
	is_wow = 0;
	is_seawolf2 = 0;
	is_home = 0;
	has_stars = 0;
	has_tball = 0;

	y_offset = 0;

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteInit()
{
	#define MIN(a,b) (((a)<(b))?(a):(b))
	#define MAX(a,b) (((a)>(b))?(a):(b))
	#define CLAMP(x, lower, upper) (MIN(upper, MAX(x, lower)))

	// loop over color values
	for (int color = 0; color < 32; color++)
	{
		// color 0 maps to ry = by = 0
		double const angle = (color / 32.0) * (2.0 * M_PI);
		float const ry = color ? (0.75 * sin(angle)) : 0;
		float const by = color ? (1.15 * cos(angle)) : 0;

		// iterate over luminence values
		for (int luma = 0; luma < 16; luma++)
		{
			float const y = luma / 15.0;

			// transform to RGB
			int r = (ry + y) * 255;
			int g = ((y - 0.299f * (ry + y) - 0.114f * (by + y)) / 0.587f) * 255;
			int b = (by + y) * 255;

			// clamp and store
			r = CLAMP(r, 0, 255);
			g = CLAMP(g, 0, 255);
			b = CLAMP(b, 0, 255);

			DrvPalette[color * 16 + luma] = BurnHighCol(r, g, b, 0);
		}
	}
}

static INT32 ProfpacDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 16; i++) {
			int c = profpac_palram[i];
			DrvPalette[i] = BurnHighCol(pal4bit(c >> 8), pal4bit((c >> 4) & 0xf), pal4bit(c & 0xf), 0);
		}
		DrvRecalc = 0;
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void seawolf2_draw_icons()
{
	#define o 0,
	#define x 1,
	static const UINT8 icon_torpedo[8*8] = {
		o o o x o o o o
		o o x x x o o o
		o o x x x o o o
		o o x x x o o o
		o o x x x o o o
		o o o x o o o o
		o o x x x o o o
		o o o x o o o o
	};
	#undef o
	#undef x
	//bprintf(0, _T("lamps: %02x   %02x\n"), lamps[0], lamps[1]);
	// Draw torpedo stock

	for (INT32 i = 0; i < 4; i++) {
		if (lamps[1] & (1<<i)) {
			Draw8x8MaskTile(pTransDraw, 0, 256+(3-i)*16, 202, 0, 0, 0, 1, 0, 0, (UINT8*)&icon_torpedo);
		}
		if (lamps[0] & (1<<i)) {
			Draw8x8MaskTile(pTransDraw, 0,   8+(3-i)*16, 202, 0, 0, 0, 1, 0, 0, (UINT8*)&icon_torpedo);
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	int x_offset = (352 - nScreenWidth) / 2;

	for (INT32 y = 0; y < nScreenHeight; y++) {
		UINT16 *out = pTransDraw + y * nScreenWidth;
		UINT16 *in = bitmap + (y + y_offset) * 352;
		for (INT32 x = 0; x < nScreenWidth; x++) {
			out[x] = in[x+x_offset];
		}
	}

	if (is_seawolf2) {
		seawolf2_draw_icons();
	}

	BurnTransferCopy(DrvPalette);

	if (is_seawolf2) BurnGunDrawTargets();

	return 0;
}


static UINT8 seawolf2_target_scaler(UINT8 in, INT32 player)
{
	// scale "0x00 - 0xff"  ->  "0x40 - 0xbf"
	in = scalerange(in, 0x00, 0xff+1, 0x40, 0xbf+1);

	in -= (player) ? 8 : 6; // slight offset, depending on player
	in = (in >> 2) ^ 0x3f; // reduce and reverse
	in = in ^ (in >> 1); // convert to reflected binary code

	return in;
}

#if 0
static void mix_s2s(INT16 *buf_in, INT16 *buf_out, INT32 buf_len, double volume)
{
	for (INT32 i = 0; i < buf_len; i++) {
		const INT32 a = i * 2 + 0;
		const INT32 b = i * 2 + 1;

		buf_out[a] = BURN_SND_CLIP(buf_out[a] + (buf_in[a] * volume));
		buf_out[b] = BURN_SND_CLIP(buf_out[b] + (buf_in[b] * volume));
	}
}
#endif

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0, 8);
		memcpy (DrvInputs, DrvDips, 4);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}

		if (has_tball) {
			BurnTrackballConfig(0, AXIS_NORMAL, AXIS_NORMAL);
			BurnTrackballFrame(0, Analog[0], Analog[1], 0x01, 0x07, 262);
			BurnTrackballUpdate(0);

			BurnTrackballConfig(1, AXIS_NORMAL, AXIS_NORMAL);
			BurnTrackballFrame(1, Analog[2], Analog[3], 0x01, 0x07, 262);
			BurnTrackballUpdate(1);
		}
		if (is_seawolf2) {
			BurnGunMakeInputs(0, Analog[0], 0);
			BurnGunMakeInputs(1, Analog[2], 0);

			const UINT8 p1 = seawolf2_target_scaler(BurnGunReturnX(0), 0);
			const UINT8 p2 = seawolf2_target_scaler(BurnGunReturnX(1), 1);

			DrvInputs[0] = (DrvInputs[0] & ~0x3f) | (p1 & 0x3f);
			DrvInputs[1] = (DrvInputs[1] & ~0x3f) | (p2 & 0x3f);
		}
		if (is_home) {
			for (INT32 i = 0; i < 4; i++) {
				BurnTrackballConfig(i, AXIS_NORMAL, AXIS_NORMAL);
				BurnTrackballFrame(i, Analog[i], 0, 0, 0xff);
				BurnTrackballUpdate(i);
			}
			for (INT32 i = 0; i < 6; i++) {
				for (INT32 j = 0; j < 4; j++) {
					DrvInputs[j+4] ^= (DrvKbd[j][i] & 1) << i;
				}
			}
			// faking start as keypad 1
			for (INT32 i = 0; i < 4; i++)
				DrvInputs[7] |= (DrvJoyF[i] << 4);
		}
	}

	INT32 nInterleave = 262;
	INT32 nCyclesTotal[1] = { 1789772 / 60 };
	INT32 nCyclesDone[1] = { nExtraCycles };
	if (is_seawolf2) {
		// game needs a slight OC to avoid jitter
		nCyclesTotal[0] = 3579545 / 60;
	}
	ZetNewFrame();
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		scanline = i;
		common_interrupt();
		CPU_RUN(0, Zet);
		if (i < 240) update_line_cb(i);
	}

	ZetClose();

	nExtraCycles = nCyclesDone[0] - nCyclesTotal[0];

	if (pBurnSoundOut) {
		if (has_sam) BurnSampleRender(pBurnSoundOut, nBurnSoundLen);
		if (has_snd) {
			astrocde_snd_update(pBurnSoundOut, nBurnSoundLen);
			BurnSoundDCFilter();
		}
		if (has_sc01) sc01_update(pBurnSoundOut, nBurnSoundLen);
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
		*pnMin = 0x029702;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);

		if (has_sam) BurnSampleScan(nAction, pnMin);
		if (has_snd) astrocde_snd_scan(nAction, pnMin);
		if (has_sc01) sc01_scan(nAction, pnMin);
		if (is_seawolf2) BurnGunScan();
		if (has_tball || is_home) BurnTrackballScan();

		SCAN_VAR(interrupt_vector);
		SCAN_VAR(NextScanInt);
		SCAN_VAR(InterruptFlag);
		SCAN_VAR(VerticalBlank);

		SCAN_VAR(ram_write_enable);

		SCAN_VAR(video_mode);
		SCAN_VAR(colorsplit);
		SCAN_VAR(colors);
		SCAN_VAR(sparkle);
		SCAN_VAR(BackgroundData);
		SCAN_VAR(verti);
		SCAN_VAR(hori);

		SCAN_VAR(funcgen_expand_color);
		SCAN_VAR(funcgen_control);
		SCAN_VAR(funcgen_expand_count);
		SCAN_VAR(funcgen_rotate_count);
		SCAN_VAR(funcgen_rotate_data);
		SCAN_VAR(funcgen_shift_prev_data);
		SCAN_VAR(funcgen_intercept);
		SCAN_VAR(pattern_source);
		SCAN_VAR(pattern_mode);
		SCAN_VAR(pattern_dest);
		SCAN_VAR(pattern_skip);
		SCAN_VAR(pattern_width);
		SCAN_VAR(pattern_height);

		SCAN_VAR(trackball_select);

		SCAN_VAR(profpac_bankdata);
		SCAN_VAR(profpac_readshift);
		SCAN_VAR(profpac_readpage);
		SCAN_VAR(profpac_vispage);
		SCAN_VAR(profpac_writepage);
		SCAN_VAR(profpac_writemode);
		SCAN_VAR(profpac_writemask);
		SCAN_VAR(profpac_intercept);
		SCAN_VAR(profpac_vw);
		SCAN_VAR(profpac_palram);
		SCAN_VAR(profpac_color_mapping);

		SCAN_VAR(sound_p0_last);
		SCAN_VAR(sound_p1_last);
		SCAN_VAR(lamps);

		SCAN_VAR(nExtraCycles);
	}

	if (nAction & ACB_NVRAM) {
	//	ba.Data		= DrvNVRAM;
	//	ba.nLen		= 0x01000;
	//	ba.nAddress	= 0;
	//	ba.szName	= "NV RAM";
	//	BurnAcb(&ba);
	}

	return 0;
}


// Sea Wolf II

static struct BurnSampleInfo seawolf2SampleDesc[] = {
	{ "shiphit", SAMPLE_NOLOOP }, // 2 of each, one for left, one for right
	{ "shiphit", SAMPLE_NOLOOP },
	{ "torpedo", SAMPLE_NOLOOP },
	{ "torpedo", SAMPLE_NOLOOP },
	{ "dive", SAMPLE_NOLOOP },
	{ "dive", SAMPLE_NOLOOP },
	{ "sonar", SAMPLE_NOLOOP },
	{ "sonar", SAMPLE_NOLOOP },
	{ "minehit", SAMPLE_NOLOOP },
	{ "minehit", SAMPLE_NOLOOP },
	{ "", 0 }
};

STD_SAMPLE_PICK(seawolf2)
STD_SAMPLE_FN(seawolf2)

static struct BurnRomInfo seawolf2RomDesc[] = {
	{ "sw2x1.bin",			0x0800, 0xad0103f6, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "sw2x2.bin",			0x0800, 0xe0430f0a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sw2x3.bin",			0x0800, 0x05ad1619, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sw2x4.bin",			0x0800, 0x1a1a14a2, 1 | BRF_PRG | BRF_ESS }, //  3
};

STD_ROM_PICK(seawolf2)
STD_ROM_FN(seawolf2)

struct BurnDriver BurnDrvSeawolf2 = {
	"seawolf2", NULL, NULL, "seawolf", "1978",
	"Sea Wolf II\0", NULL, "Dave Nutting Associates / Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, seawolf2RomInfo, seawolf2RomName, NULL, NULL, seawolf2SampleInfo, seawolf2SampleName, Seawolf2InputInfo, Seawolf2DIPInfo,
	Seawolf2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 212, 4, 3
};


// Space Zap (Midway)

static struct BurnRomInfo spacezapRomDesc[] = {
	{ "0662.01",			0x1000, 0xa92de312, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "0663.xx",			0x1000, 0x4836ebf1, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "0664.xx",			0x1000, 0xd8193a80, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "0665.xx",			0x1000, 0x3784228d, 1 | BRF_PRG | BRF_ESS }, //  3
};

STD_ROM_PICK(spacezap)
STD_ROM_FN(spacezap)

struct BurnDriver BurnDrvSpacezap = {
	"spacezap", NULL, NULL, NULL, "1980",
	"Space Zap (Midway)\0", NULL, "Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, spacezapRomInfo, spacezapRomName, NULL, NULL, NULL, NULL, SpacezapInputInfo, SpacezapDIPInfo,
	SpacezapInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 204, 4, 3
};


// Extra Bases

static struct BurnRomInfo ebasesRomDesc[] = {
	{ "m761a",				0x1000, 0x34422147, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "m761b",				0x1000, 0x4f28dfd6, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "m761c",				0x1000, 0xbff6c97e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "m761d",				0x1000, 0x5173781a, 1 | BRF_PRG | BRF_ESS }, //  3
};

STD_ROM_PICK(ebases)
STD_ROM_FN(ebases)

struct BurnDriver BurnDrvEbases = {
	"ebases", NULL, NULL, NULL, "1980",
	"Extra Bases\0", NULL, "Dave Nutting Associates / Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, ebasesRomInfo, ebasesRomName, NULL, NULL, NULL, NULL, EbasesInputInfo, EbasesDIPInfo,
	EbasesInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};


// Wizard of Wor

static struct BurnRomInfo wowRomDesc[] = {
	{ "wow.x1",				0x1000, 0xc1295786, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "wow.x2",				0x1000, 0x9be93215, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "wow.x3",				0x1000, 0x75e5a22e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "wow.x4",				0x1000, 0xef28eb84, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "wow.x5",				0x1000, 0x16912c2b, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "wow.x6",				0x1000, 0x35797f82, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "wow.x7",				0x1000, 0xce404305, 1 | BRF_PRG | BRF_ESS }, //  6
};

STD_ROM_PICK(wow)
STD_ROM_FN(wow)

struct BurnDriver BurnDrvWow = {
	"wow", NULL, NULL, NULL, "1980",
	"Wizard of Wor\0", NULL, "Dave Nutting Associates / Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_SHOOT, 0,
	NULL, wowRomInfo, wowRomName, NULL, NULL, NULL, NULL, WowInputInfo, WowDIPInfo,
	WowInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};


// Wizard of Wor (with German language ROM)

static struct BurnRomInfo wowgRomDesc[] = {
	{ "wow.x1",				0x1000, 0xc1295786, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "wow.x2",				0x1000, 0x9be93215, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "wow.x3",				0x1000, 0x75e5a22e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "wow.x4",				0x1000, 0xef28eb84, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "wow.x5",				0x1000, 0x16912c2b, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "wow.x6",				0x1000, 0x35797f82, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "wow.x7",				0x1000, 0xce404305, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "german.x11",			0x1000, 0x16f84d73, 1 | BRF_PRG | BRF_ESS }, //  7
};

STD_ROM_PICK(wowg)
STD_ROM_FN(wowg)

struct BurnDriver BurnDrvWowg = {
	"wowg", "wow", NULL, NULL, "1980",
	"Wizard of Wor (with German language ROM)\0", NULL, "Dave Nutting Associates / Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_SHOOT, 0,
	NULL, wowgRomInfo, wowgRomName, NULL, NULL, NULL, NULL, WowInputInfo, WowgDIPInfo,
	WowInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};


// Gorf

static struct BurnRomInfo gorfRomDesc[] = {
	{ "gorf-a.bin",			0x1000, 0x5b348321, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "gorf-b.bin",			0x1000, 0x62d6de77, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gorf-c.bin",			0x1000, 0x1d3bc9c9, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gorf-d.bin",			0x1000, 0x70046e56, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "gorf-e.bin",			0x1000, 0x2d456eb5, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "gorf-f.bin",			0x1000, 0xf7e4e155, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "gorf-g.bin",			0x1000, 0x4e2bd9b9, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "gorf-h.bin",			0x1000, 0xfe7b863d, 1 | BRF_PRG | BRF_ESS }, //  7
};

STD_ROM_PICK(gorf)
STD_ROM_FN(gorf)

struct BurnDriver BurnDrvGorf = {
	"gorf", NULL, NULL, NULL, "1981",
	"Gorf\0", NULL, "Dave Nutting Associates / Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, gorfRomInfo, gorfRomName, NULL, NULL, NULL, NULL, GorfInputInfo, GorfDIPInfo,
	GorfInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	240, 352, 3, 4
};


// Gorf (program 1)

static struct BurnRomInfo gorfpgm1RomDesc[] = {
	{ "873a.x1",			0x1000, 0x97cb4a6a, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "873b.x2",			0x1000, 0x257236f8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "873c.x3",			0x1000, 0x16b0638b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "873d.x4",			0x1000, 0xb5e821dc, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "873e.x5",			0x1000, 0x8e82804b, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "873f.x6",			0x1000, 0x715fb4d9, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "873g.x7",			0x1000, 0x8a066456, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "873h.x8",			0x1000, 0x56d40c7c, 1 | BRF_PRG | BRF_ESS }, //  7
};

STD_ROM_PICK(gorfpgm1)
STD_ROM_FN(gorfpgm1)

struct BurnDriver BurnDrvGorfpgm1 = {
	"gorfpgm1", "gorf", NULL, NULL, "1981",
	"Gorf (program 1)\0", NULL, "Dave Nutting Associates / Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, gorfpgm1RomInfo, gorfpgm1RomName, NULL, NULL, NULL, NULL, GorfInputInfo, GorfDIPInfo,
	GorfInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	240, 352, 3, 4
};


// Gorf (program 1, with French language ROM)

static struct BurnRomInfo gorfpgm1fRomDesc[] = {
	{ "gorf_a.x1",			0x1000, 0x97cb4a6a, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "gorf_b.x2",			0x1000, 0x257236f8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gorf_c.x3",			0x1000, 0x16b0638b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gorf_d.x4",			0x1000, 0xb5e821dc, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "gorf_e.x5",			0x1000, 0x8e82804b, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "gorf_f.x6",			0x1000, 0x715fb4d9, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "gorf_g.x7",			0x1000, 0x8a066456, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "gorf_h.x8",			0x1000, 0x56d40c7c, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "french_gorf.x11",	0x1000, 0x759d7f66, 1 | BRF_PRG | BRF_ESS }, //  8
};

STD_ROM_PICK(gorfpgm1f)
STD_ROM_FN(gorfpgm1f)

struct BurnDriver BurnDrvGorfpgm1f = {
	"gorfpgm1f", "gorf", NULL, NULL, "1981",
	"Gorf (program 1, with French language ROM)\0", NULL, "Dave Nutting Associates / Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, gorfpgm1fRomInfo, gorfpgm1fRomName, NULL, NULL, NULL, NULL, GorfInputInfo, Gorfpgm1fDIPInfo,
	GorfInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	240, 352, 3, 4
};


// Gorf (program 1, with German language ROM)

static struct BurnRomInfo gorfpgm1gRomDesc[] = {
	{ "873a.x1",			0x1000, 0x97cb4a6a, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "873b.x2",			0x1000, 0x257236f8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "873c.x3",			0x1000, 0x16b0638b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "873d.x4",			0x1000, 0xb5e821dc, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "873e.x5",			0x1000, 0x8e82804b, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "873f.x6",			0x1000, 0x715fb4d9, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "873g.x7",			0x1000, 0x8a066456, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "873h.x8",			0x1000, 0x56d40c7c, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "german.x11",			0x1000, 0x3a3dbdcb, 1 | BRF_PRG | BRF_ESS }, //  8
};

STD_ROM_PICK(gorfpgm1g)
STD_ROM_FN(gorfpgm1g)

struct BurnDriver BurnDrvGorfpgm1g = {
	"gorfpgm1g", "gorf", NULL, NULL, "1981",
	"Gorf (program 1, with German language ROM)\0", NULL, "Dave Nutting Associates / Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, gorfpgm1gRomInfo, gorfpgm1gRomName, NULL, NULL, NULL, NULL, GorfInputInfo, Gorfpgm1gDIPInfo,
	GorfInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	240, 352, 3, 4
};


// Gorf (Spain, Irecsa license)

static struct BurnRomInfo gorfirecRomDesc[] = {
	{ "a.bin",				0x1000, 0x97cb4a6a, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "b.bin",				0x1000, 0xe35701c0, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "c.bin",				0x1000, 0x16b0638b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "d.bin",				0x1000, 0x0dccdfc4, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "e.bin",				0x1000, 0x8e82804b, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "f.bin",				0x1000, 0x715fb4d9, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "g.bin",				0x1000, 0xfb5b7131, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "h.bin",				0x1000, 0x578ade88, 1 | BRF_PRG | BRF_ESS }, //  7
};

STD_ROM_PICK(gorfirec)
STD_ROM_FN(gorfirec)

struct BurnDriver BurnDrvGorfirec = {
	"gorfirec", "gorf", NULL, NULL, "1981",
	"Gorf (Spain, Irecsa license)\0", NULL, "Dave Nutting Associates / Midway (Irecsa license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, gorfirecRomInfo, gorfirecRomName, NULL, NULL, NULL, NULL, GorfInputInfo, GorfirecDIPInfo,
	GorfInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	240, 352, 3, 4
};


// The Adventures of Robby Roto! - Roto to the Rescue!

static struct BurnRomInfo robbyRomDesc[] = {
	{ "rotox1.bin",			0x1000, 0xa431b85a, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "rotox2.bin",			0x1000, 0x33cdda83, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rotox3.bin",			0x1000, 0xdbf97491, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rotox4.bin",			0x1000, 0xa3b90ac8, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "rotox5.bin",			0x1000, 0x46ae8a94, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "rotox6.bin",			0x1000, 0x7916b730, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "rotox7.bin",			0x1000, 0x276dc4a5, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "rotox8.bin",			0x1000, 0x1ef13457, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "rotox9.bin",			0x1000, 0x370352bf, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "rotox10.bin",		0x1000, 0xe762cbda, 1 | BRF_PRG | BRF_ESS }, //  9
};

STD_ROM_PICK(robby)
STD_ROM_FN(robby)

struct BurnDriver BurnDrvRobby = {
	"robby", NULL, NULL, NULL, "1981",
	"The Adventures of Robby Roto! - Roto to the Rescue!\0", NULL, "Dave Nutting Associates / Bally Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_MAZE, 0,
	NULL, robbyRomInfo, robbyRomName, NULL, NULL, NULL, NULL, RobbyInputInfo, RobbyDIPInfo,
	RobbyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};


// Professor Pac-Man

static struct BurnRomInfo profpacRomDesc[] = {
	{ "pps1",				0x2000, 0xa244a62d, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "pps2",				0x2000, 0x8a9a6653, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "pps9",				0x2000, 0x17a0b418, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "pps3",				0x2000, 0x15717fd8, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "pps4",				0x2000, 0x36540598, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "pps5",				0x2000, 0x8dc89a59, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "pps6",				0x2000, 0x5a2186c3, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "pps7",				0x2000, 0xf9c26aba, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "pps8",				0x2000, 0x4d201e41, 1 | BRF_PRG | BRF_ESS }, //  8

	{ "ppq1",				0x4000, 0xdddc2ccc, 2 | BRF_PRG | BRF_ESS }, //  9 EPROMs
	{ "ppq2",				0x4000, 0x33bbcabe, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "ppq3",				0x4000, 0x3534d895, 2 | BRF_PRG | BRF_ESS }, // 11
	{ "ppq4",				0x4000, 0x17e3581d, 2 | BRF_PRG | BRF_ESS }, // 12
	{ "ppq5",				0x4000, 0x80882a93, 2 | BRF_PRG | BRF_ESS }, // 13
	{ "ppq6",				0x4000, 0xe5ddaee5, 2 | BRF_PRG | BRF_ESS }, // 14
	{ "ppq7",				0x4000, 0xc029cd34, 2 | BRF_PRG | BRF_ESS }, // 15
	{ "ppq8",				0x4000, 0xfb3a1ac9, 2 | BRF_PRG | BRF_ESS }, // 16
	{ "ppq9",				0x4000, 0x5e944488, 2 | BRF_PRG | BRF_ESS }, // 17
	{ "ppq10",				0x4000, 0xed72a81f, 2 | BRF_PRG | BRF_ESS }, // 18
	{ "ppq11",				0x4000, 0x98295020, 2 | BRF_PRG | BRF_ESS }, // 19
	{ "ppq12",				0x4000, 0xe01a8dbe, 2 | BRF_PRG | BRF_ESS }, // 20
	{ "ppq13",				0x4000, 0x87165d4f, 2 | BRF_PRG | BRF_ESS }, // 21
	{ "ppq14",				0x4000, 0xecb861de, 2 | BRF_PRG | BRF_ESS }, // 22

	{ "pls153a_cpu.u12",	0x00eb, 0x499a6fc5, 0 | BRF_OPT },           // 23 PLDs
	{ "pls153a_cpu.u16",	0x00eb, 0x9a5ea540, 0 | BRF_OPT },           // 24
	{ "pls153a_epr.u6",		0x00eb, 0xd8454bf7, 0 | BRF_OPT },           // 25
	{ "pls153a_epr.u7",		0x00eb, 0xfa831d9f, 0 | BRF_OPT },           // 26
	{ "pls153a_gam.u10",	0x00eb, 0xfe2157b0, 0 | BRF_OPT },           // 27
	{ "pls153a_gam.u11",	0x00eb, 0x5772f6d8, 0 | BRF_OPT },           // 28
	{ "pls153a_gam.u5",		0x00eb, 0xb3f2c6b8, 0 | BRF_OPT },           // 29
	{ "pls153a_scr.u19",	0x00eb, 0xb5fff2db, 0 | BRF_OPT },           // 30
	{ "pls153a_scr.u39",	0x00eb, 0xba7ef5dd, 0 | BRF_OPT },           // 31
	{ "pls153a_scr.u55",	0x00eb, 0xc3f47134, 0 | BRF_OPT },           // 32
};

STD_ROM_PICK(profpac)
STD_ROM_FN(profpac)

struct BurnDriver BurnDrvProfpac = {
	"profpac", NULL, NULL, NULL, "1983",
	"Professor Pac-Man\0", NULL, "Dave Nutting Associates / Bally Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_QUIZ, 0,
	NULL, profpacRomInfo, profpacRomName, NULL, NULL, NULL, NULL, ProfpacInputInfo, ProfpacDIPInfo,
	ProfpacInit, DrvExit, DrvFrame, ProfpacDraw, DrvScan, &DrvRecalc, 0x10,
	320, 204, 4, 3
};


// Demons & Dragons (Prototype)

static struct BurnRomInfo demndrgnRomDesc[] = {
	{ "dd-x1.bin",			0x2000, 0x9aeaf79e, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "dd-x2.bin",			0x2000, 0x0c63b624, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "dd-x9.bin",			0x2000, 0x3792d632, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "dd-x5.bin",			0x2000, 0xe377e831, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "dd-x6.bin",			0x2000, 0x0fcb46ad, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "dd-x7.bin",			0x2000, 0x0675e4fa, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "dd-x10.bin",			0x2000, 0x4a22c4f9, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "dd-x11.bin",			0x2000, 0xd3158845, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "dd-x12.bin",			0x2000, 0x592c1d9a, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "dd-x13.bin",			0x2000, 0x492d7b7e, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "dd-x14.bin",			0x2000, 0x7843c818, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "dd-x15.bin",			0x2000, 0x6e6bc1b6, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "dd-x16.bin",			0x2000, 0x7a4a343b, 1 | BRF_PRG | BRF_ESS }, // 12
};

STD_ROM_PICK(demndrgn)
STD_ROM_FN(demndrgn)

struct BurnDriver BurnDrvDemndrgn = {
	"demndrgn", NULL, NULL, NULL, "1982",
	"Demons & Dragons (Prototype)\0", "No Sound", "Dave Nutting Associates / Bally Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, demndrgnRomInfo, demndrgnRomName, NULL, NULL, NULL, NULL, DemndrgnInputInfo, DemndrgnDIPInfo,
	DemndrgnInit, DrvExit, DrvFrame, ProfpacDraw, DrvScan, &DrvRecalc, 0x10,
	352, 240, 4, 3
};

// ----------------------------
// Bally Astrocade home system
// ----------------------------

static INT32 AstroGetZipName(char** pszName, UINT32 i)
{
	static char szFilename[MAX_PATH];
	char* pszGameName = NULL;

	if (pszName == NULL) {
		return 1;
	}

	if (i == 0) {
		pszGameName = BurnDrvGetTextA(DRV_NAME);
	} else {
		if (i == 1 && BurnDrvGetTextA(DRV_BOARDROM)) {
			pszGameName = BurnDrvGetTextA(DRV_BOARDROM);
		} else {
			pszGameName = BurnDrvGetTextA(DRV_PARENT);
		}
	}

	if (pszGameName == NULL || i > 2) {
		*pszName = NULL;
		return 1;
	}

	// remove astro_
	memset(szFilename, 0, MAX_PATH);
	for (UINT32 j = 0; j < (strlen(pszGameName) - 6); j++) {
		szFilename[j] = pszGameName[j+6];
	}

	*pszName = szFilename;

	return 0;
}

static UINT8 home_cart_read(UINT16 offset)
{
	switch (cart_type)
	{
		case CART_STD:
			return (offset < cart_size) ? DrvCartROM[offset] : 0xff;

		case CART_256K:
			if (offset < 0x1000)
				return DrvCartROM[offset + 0x1000 * 0x3f];
			else if (offset < 0x1fc0)
				return DrvCartROM[(offset & 0xfff) + (0x1000 * cart_bank)];
			else
				return cart_bank = offset & 0x3f;

		case CART_512K:
			if (offset < 0x1000)
				return DrvCartROM[offset + 0x1000 * 0x7f];
			else if (offset < 0x1f80)
				return DrvCartROM[(offset & 0xfff) + (0x1000 * cart_bank)];
			else
				return cart_bank = offset & 0x7f;
	}
	return 0xff;
}

static UINT8 __fastcall home_read(UINT16 address)
{
	if (address < 0x2000) return DrvZ80ROM[address];
	if (address < 0x4000) return home_cart_read(address - 0x2000);
	return 0;
}

static void __fastcall home_write(UINT16 address, UINT8 data)
{
	if (address < 0x1000) {
		astrocade_funcgen_write(address, data);
		return;
	}
}

static UINT8 __fastcall home_read_port(UINT16 port)
{
	UINT8 lo = port & 0xff;

	if (lo < 0x10)
		return common_port_read(port);

	if (lo < 0x20) {
		UINT8 sub = lo & 0x0f;
		if (sub < 0x08) return DrvInputs[sub & 0x07];
		if (sub >= 0x0c) return BurnTrackballRead(sub & 3);
		return 0xff;
	}

	return 0xff;
}

static void __fastcall home_write_port(UINT16 port, UINT8 data)
{
	common_port_write(port, data);
}

static INT32 HomeInit()
{
	is_home = 1;
	BurnAllocMemIndex();

	if (DrvLoadROMs()) return 1;

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvVidRAM,              0x4000, 0x4fff, MAP_RAM);
	ZetSetWriteHandler(home_write);
	ZetSetReadHandler(home_read);
	ZetSetOutHandler(home_write_port);
	ZetSetInHandler(home_read_port);
	ZetClose();

	update_line_cb = common_update_line;

	astrocde_snd_init(0, BURN_SND_ROUTE_BOTH);
	astrocde_snd_set_buffered(ZetTotalCycles, 1789772);
	has_snd = 1;

	BurnTrackballInit(4);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static struct BurnRomInfo emptyRomDesc[] = {
	{ "",                    0,          0, 0 },
};

// Bally Professional Arcade
static struct BurnRomInfo astro_astrocdeRomDesc[] = {
	{ "astro.bin",			0x2000, 0xebc77f3a, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
};

STD_ROM_PICK(astro_astrocde)
STD_ROM_FN(astro_astrocde)

struct BurnDriver BurnDrvastro_astrocde = {
	"astro_astrocde", NULL, NULL, NULL, "1978",
	"Bally Professional Arcade\0", NULL, "Bally Manufacturing", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_BOARDROM, 1, HARDWARE_ASTROHOME, GBF_BIOS, 0,
	AstroGetZipName, astro_astrocdeRomInfo, astro_astrocdeRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// 280 Zzzap / Dodgem (USA)
static struct BurnRomInfo astro_280zzapRomDesc[] = {
	{ "280 Zzzap - Dodgem (1978)(Bally).bin",		2048, 0xe4d285e5, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_280zzap, astro_280zzap, astro_astrocde)
STD_ROM_FN(astro_280zzap)

struct BurnDriver BurnDrvastro_280zzap = {
	"astro_280zzap", NULL, "astro_astrocde", NULL, "1978",
	"280 Zzzap / Dodgem (USA)\0", NULL, "Bally", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_ASTROHOME, GBF_RACING, 0,
	AstroGetZipName, astro_280zzapRomInfo, astro_280zzapRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Hard Day's Night, A (Prototype)
static struct BurnRomInfo astro_harddayRomDesc[] = {
	{ "Hard Day's Night, A (Proto)(198x)(Richard Degler & George Moses).bin",		2048, 0x9234ddf6, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_hardday, astro_hardday, astro_astrocde)
STD_ROM_FN(astro_hardday)

struct BurnDriver BurnDrvastro_hardday = {
	"astro_hardday", NULL, "astro_astrocde", NULL, "198?",
	"Hard Day's Night, A (Prototype)\0", NULL, "Richard Degler & George Moses", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 1, HARDWARE_ASTROHOME, GBF_MISC, 0,
	AstroGetZipName, astro_harddayRomInfo, astro_harddayRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// ADS System Monitor (Prototype)
static struct BurnRomInfo astro_adssmRomDesc[] = {
	{ "ADS System Monitor (Proto)(19xx).bin",		2048, 0xb8d4df61, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_adssm, astro_adssm, astro_astrocde)
STD_ROM_FN(astro_adssm)

struct BurnDriver BurnDrvastro_adssm = {
	"astro_adssm", NULL, "astro_astrocde", NULL, "19??",
	"ADS System Monitor (Prototype)\0", NULL, "<unknown>", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 1, HARDWARE_ASTROHOME, GBF_MISC, 0,
	AstroGetZipName, astro_adssmRomInfo, astro_adssmRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Amazing Maze / Tic-Tac-Toe (USA)
static struct BurnRomInfo astro_amazmazeRomDesc[] = {
	{ "Amazing Maze - Tic-Tac-Toe (1978)(Bally).bin",		2048, 0xcd57c485, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_amazmaze, astro_amazmaze, astro_astrocde)
STD_ROM_FN(astro_amazmaze)

struct BurnDriver BurnDrvastro_amazmaze = {
	"astro_amazmaze", NULL, "astro_astrocde", NULL, "1978",
	"Amazing Maze / Tic-Tac-Toe (USA)\0", NULL, "Bally", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ASTROHOME, GBF_BOARD | GBF_PUZZLE, 0,
	AstroGetZipName, astro_amazmazeRomInfo, astro_amazmazeRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Artillery Duel (USA)
static struct BurnRomInfo astro_artiduelRomDesc[] = {
	{ "Artillery Duel (1982)(Xonox).bin",		4096, 0x4f372019, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_artiduel, astro_artiduel, astro_astrocde)
STD_ROM_FN(astro_artiduel)

struct BurnDriver BurnDrvastro_artiduel = {
	"astro_artiduel", NULL, "astro_astrocde", NULL, "1982",
	"Artillery Duel (USA)\0", NULL, "Xonox", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ASTROHOME, GBF_SHOOT, 0,
	AstroGetZipName, astro_artiduelRomInfo, astro_artiduelRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Artillery Duel (Prototype)
static struct BurnRomInfo astro_artiduelp1RomDesc[] = {
	{ "Artillery Duel (Proto)(1982)(Xonox).bin",		4096, 0x2227f488, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_artiduelp1, astro_artiduelp1, astro_astrocde)
STD_ROM_FN(astro_artiduelp1)

struct BurnDriver BurnDrvastro_artiduelp1 = {
	"astro_artiduelp1", "astro_artiduel", "astro_astrocde", NULL, "1982",
	"Artillery Duel (Prototype)\0", NULL, "Xonox", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 2, HARDWARE_ASTROHOME, GBF_SHOOT, 0,
	AstroGetZipName, astro_artiduelp1RomInfo, astro_artiduelp1RomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Artillery Duel (Prototype, Alt)
static struct BurnRomInfo astro_artiduelp2RomDesc[] = {
	{ "Artillery Duel (Proto, Alt)(1982)(Xonox).bin",		4096, 0x53f75720, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_artiduelp2, astro_artiduelp2, astro_astrocde)
STD_ROM_FN(astro_artiduelp2)

struct BurnDriver BurnDrvastro_artiduelp2 = {
	"astro_artiduelp2", "astro_artiduel", "astro_astrocde", NULL, "1982",
	"Artillery Duel (Prototype, Alt)\0", NULL, "Xonox", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 2, HARDWARE_ASTROHOME, GBF_SHOOT, 0,
	AstroGetZipName, astro_artiduelp2RomInfo, astro_artiduelp2RomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Artillery Duel (Prototype, Alt 2)
static struct BurnRomInfo astro_artiduelp3RomDesc[] = {
	{ "Artillery Duel (Proto, Alt 2)(1982)(Xonox).bin",		2048, 0x2521a6ec, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_artiduelp3, astro_artiduelp3, astro_astrocde)
STD_ROM_FN(astro_artiduelp3)

struct BurnDriver BurnDrvastro_artiduelp3 = {
	"astro_artiduelp3", "astro_artiduel", "astro_astrocde", NULL, "1982",
	"Artillery Duel (Prototype, Alt 2)\0", NULL, "Xonox", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 2, HARDWARE_ASTROHOME, GBF_SHOOT, 0,
	AstroGetZipName, astro_artiduelp3RomInfo, astro_artiduelp3RomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Astro Battle (USA)
static struct BurnRomInfo astro_astrobatRomDesc[] = {
	{ "Astro Battle (1979)(Bally).bin",		4096, 0x0b5298bc, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_astrobat, astro_astrobat, astro_astrocde)
STD_ROM_FN(astro_astrobat)

struct BurnDriver BurnDrvastro_astrobat = {
	"astro_astrobat", NULL, "astro_astrocde", NULL, "1979",
	"Astro Battle (USA)\0", NULL, "Bally", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_ASTROHOME, GBF_VERSHOOT, 0,
	AstroGetZipName, astro_astrobatRomInfo, astro_astrobatRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Bally Pin (USA)
static struct BurnRomInfo astro_ballypinRomDesc[] = {
	{ "Bally Pin (1979)(Bally).bin",		4096, 0x3f431613, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_ballypin, astro_ballypin, astro_astrocde)
STD_ROM_FN(astro_ballypin)

struct BurnDriver BurnDrvastro_ballypin = {
	"astro_ballypin", NULL, "astro_astrocde", NULL, "1979",
	"Bally Pin (USA)\0", NULL, "Bally", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_ASTROHOME, GBF_PINBALL, 0,
	AstroGetZipName, astro_ballypinRomInfo, astro_ballypinRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// BioRhythm (USA)
static struct BurnRomInfo astro_biorhyRomDesc[] = {
	{ "BioRhythm (1981)(Astrovision).bin",		4096, 0xfc4bd27c, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_biorhy, astro_biorhy, astro_astrocde)
STD_ROM_FN(astro_biorhy)

struct BurnDriver BurnDrvastro_biorhy = {
	"astro_biorhy", NULL, "astro_astrocde", NULL, "1981",
	"BioRhythm (USA)\0", NULL, "Astrovision", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_ASTROHOME, GBF_MISC, 0,
	AstroGetZipName, astro_biorhyRomInfo, astro_biorhyRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Black Jack / Acey Deucey / Poker (USA)
static struct BurnRomInfo astro_blckjackRomDesc[] = {
	{ "Black Jack - Acey Deucey - Poker (1978)(Bally).bin",		4096, 0x78d45a11, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_blckjack, astro_blckjack, astro_astrocde)
STD_ROM_FN(astro_blckjack)

struct BurnDriver BurnDrvastro_blckjack = {
	"astro_blckjack", NULL, "astro_astrocde", NULL, "1978",
	"Black Jack / Acey Deucey / Poker (USA)\0", NULL, "Bally", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_ASTROHOME, GBF_CARD, 0,
	AstroGetZipName, astro_blckjackRomInfo, astro_blckjackRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Blast Droids (USA)
static struct BurnRomInfo astro_bstdroidRomDesc[] = {
	{ "Blast Droids (1983)(Esoterica).bin",		4096, 0x79fe386b, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_bstdroid, astro_bstdroid, astro_astrocde)
STD_ROM_FN(astro_bstdroid)

struct BurnDriver BurnDrvastro_bstdroid = {
	"astro_bstdroid", NULL, "astro_astrocde", NULL, "1983",
	"Blast Droids (USA)\0", NULL, "Esoterica", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ASTROHOME, GBF_MULTISHOOT, 0,
	AstroGetZipName, astro_bstdroidRomInfo, astro_bstdroidRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Bowling (Prototype)
static struct BurnRomInfo astro_bowlingRomDesc[] = {
	{ "Bowling (Proto)(1978)(Bally).bin",		4096, 0x1f8bc8ba, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_bowling, astro_bowling, astro_astrocde)
STD_ROM_FN(astro_bowling)

struct BurnDriver BurnDrvastro_bowling = {
	"astro_bowling", NULL, "astro_astrocde", NULL, "1978",
	"Bowling (Prototype)\0", NULL, "Bally", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 4, HARDWARE_ASTROHOME, GBF_SPORTSMISC, 0,
	AstroGetZipName, astro_bowlingRomInfo, astro_bowlingRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Brickyard / Clowns (USA)
static struct BurnRomInfo astro_brckyardRomDesc[] = {
	{ "Brickyard - Clowns (1979)(Bally).bin",		4096, 0x28f9e502, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_brckyard, astro_brckyard, astro_astrocde)
STD_ROM_FN(astro_brckyard)

struct BurnDriver BurnDrvastro_brckyard = {
	"astro_brckyard", NULL, "astro_astrocde", NULL, "1979",
	"Brickyard / Clowns (USA)\0", NULL, "Bally", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_ASTROHOME, GBF_BREAKOUT, 0,
	AstroGetZipName, astro_brckyardRomInfo, astro_brckyardRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Can't Buy Me Love (Prototype)
static struct BurnRomInfo astro_buyloveRomDesc[] = {
	{ "Can't Buy Me Love (Proto)(198x)(Richard Degler & George Moses).bin",		8192, 0xfc6f4f7b, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_buylove, astro_buylove, astro_astrocde)
STD_ROM_FN(astro_buylove)

struct BurnDriver BurnDrvastro_buylove = {
	"astro_buylove", NULL, "astro_astrocde", NULL, "198?",
	"Can't Buy Me Love (Prototype)\0", NULL, "Richard Degler & George Moses", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 1, HARDWARE_ASTROHOME, GBF_MISC, 0,
	AstroGetZipName, astro_buyloveRomInfo, astro_buyloveRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Candyman (USA)
static struct BurnRomInfo astro_candymanRomDesc[] = {
	{ "Candyman (198x)(L&M Software & Mike White).bin",		8192, 0x0fc97852, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_candyman, astro_candyman, astro_astrocde)
STD_ROM_FN(astro_candyman)

struct BurnDriver BurnDrvastro_candyman = {
	"astro_candyman", NULL, "astro_astrocde", NULL, "198?",
	"Candyman (USA)\0", NULL, "L&M Software & Mike White", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_ASTROHOME, GBF_ACTION | GBF_MAZE, 0,
	AstroGetZipName, astro_candymanRomInfo, astro_candymanRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Checkers (Prototype)
static struct BurnRomInfo astro_checkersRomDesc[] = {
	{ "Checkers (Proto)(19xx)(Bally).bin",		2048, 0x757bab8f, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_checkers, astro_checkers, astro_astrocde)
STD_ROM_FN(astro_checkers)

struct BurnDriver BurnDrvastro_checkers = {
	"astro_checkers", NULL, "astro_astrocde", NULL, "19??",
	"Checkers (Prototype)\0", NULL, "Bally", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 2, HARDWARE_ASTROHOME, GBF_BOARD, 0,
	AstroGetZipName, astro_checkersRomInfo, astro_checkersRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Claim Jumper (USA)
static struct BurnRomInfo astro_claimRomDesc[] = {
	{ "Claim Jumper (198x)(L&M Software & Mike White).bin",		8192, 0x1edeae0e, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_claim, astro_claim, astro_astrocde)
STD_ROM_FN(astro_claim)

struct BurnDriver BurnDrvastro_claim = {
	"astro_claim", NULL, "astro_astrocde", NULL, "198?",
	"Claim Jumper (USA)\0", NULL, "L&M Software & Mike White", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_ASTROHOME, GBF_PUZZLE, 0,
	AstroGetZipName, astro_claimRomInfo, astro_claimRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Coloring Book (Prototype)
static struct BurnRomInfo astro_colbookRomDesc[] = {
	{ "Coloring Book (Proto)(198x)(Astrocade).bin",		8192, 0x14f36aba, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_colbook, astro_colbook, astro_astrocde)
STD_ROM_FN(astro_colbook)

struct BurnDriver BurnDrvastro_colbook = {
	"astro_colbook", NULL, "astro_astrocde", NULL, "198?",
	"Coloring Book (Prototype)\0", NULL, "Astrocade Inc.", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 1, HARDWARE_ASTROHOME, GBF_MISC, 0,
	AstroGetZipName, astro_colbookRomInfo, astro_colbookRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Conan The Barbarian! (USA)
static struct BurnRomInfo astro_conanRomDesc[] = {
	{ "Conan The Barbarian (1985)(Dave Carson Software).bin",		8192, 0x8a6adbfe, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_conan, astro_conan, astro_astrocde)
STD_ROM_FN(astro_conan)

struct BurnDriver BurnDrvastro_conan = {
	"astro_conan", NULL, "astro_astrocde", NULL, "1985",
	"Conan The Barbarian! (USA)\0", NULL, "Dave Carson Software", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_ASTROHOME, GBF_ACTION, 0,
	AstroGetZipName, astro_conanRomInfo, astro_conanRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Conan The Barbarian! (Prototype)
static struct BurnRomInfo astro_conanpRomDesc[] = {
	{ "Conan The Barbarian (Proto)(1982)(Astrocade).bin",		8192, 0x15b1afea, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_conanp, astro_conanp, astro_astrocde)
STD_ROM_FN(astro_conanp)

struct BurnDriver BurnDrvastro_conanp = {
	"astro_conanp", "astro_conan", "astro_astrocde", NULL, "1982",
	"Conan The Barbarian! (Prototype)\0", NULL, "Astrocade Inc.", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 1, HARDWARE_ASTROHOME, GBF_ACTION, 0,
	AstroGetZipName, astro_conanpRomInfo, astro_conanpRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Cosmic Raiders (USA)
static struct BurnRomInfo astro_cosmicrdRomDesc[] = {
	{ "Cosmic Raider (1982)(Astrocade).bin",		8192, 0x2a77b3fb, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_cosmicrd, astro_cosmicrd, astro_astrocde)
STD_ROM_FN(astro_cosmicrd)

struct BurnDriver BurnDrvastro_cosmicrd = {
	"astro_cosmicrd", NULL, "astro_astrocde", NULL, "1982",
	"Cosmic Raiders (USA)\0", NULL, "Astrocade Inc.", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_ASTROHOME, GBF_HORSHOOT, 0,
	AstroGetZipName, astro_cosmicrdRomInfo, astro_cosmicrdRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Cosmic Raiders (Prototype, Alt)
static struct BurnRomInfo astro_cosmicrdaRomDesc[] = {
	{ "Cosmic Raider (Proto, Alt)(198x)(Astrocade).bin",		8192, 0x869b692e, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_cosmicrda, astro_cosmicrda, astro_astrocde)
STD_ROM_FN(astro_cosmicrda)

struct BurnDriver BurnDrvastro_cosmicrda = {
	"astro_cosmicrda", "astro_cosmicrd", "astro_astrocde", NULL, "198?",
	"Cosmic Raiders (Prototype, Alt)\0", NULL, "Astrocade Inc.", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 4, HARDWARE_ASTROHOME, GBF_HORSHOOT, 0,
	AstroGetZipName, astro_cosmicrdaRomInfo, astro_cosmicrdaRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Cosmic Raiders (Prototype, Alt 2)
static struct BurnRomInfo astro_cosmicrdbRomDesc[] = {
	{ "Cosmic Raider (Proto, Alt 2)(198x)(Astrocade).bin",		8192, 0x37670f78, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_cosmicrdb, astro_cosmicrdb, astro_astrocde)
STD_ROM_FN(astro_cosmicrdb)

struct BurnDriver BurnDrvastro_cosmicrdb = {
	"astro_cosmicrdb", "astro_cosmicrd", "astro_astrocde", NULL, "198?",
	"Cosmic Raiders (Prototype, Alt 2)\0", NULL, "Astrocade Inc.", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 4, HARDWARE_ASTROHOME, GBF_HORSHOOT, 0,
	AstroGetZipName, astro_cosmicrdbRomInfo, astro_cosmicrdbRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Cosmic Raiders (Prototype, v3.5)
static struct BurnRomInfo astro_cosmicrd35RomDesc[] = {
	{ "Cosmic Raider (Proto, v3.5)(198x)(Astrocade).bin",		8192, 0xe3d723a0, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_cosmicrd35, astro_cosmicrd35, astro_astrocde)
STD_ROM_FN(astro_cosmicrd35)

struct BurnDriver BurnDrvastro_cosmicrd35 = {
	"astro_cosmicrd35", "astro_cosmicrd", "astro_astrocde", NULL, "198?",
	"Cosmic Raiders (Prototype, v3.5)\0", NULL, "Astrocade Inc.", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 4, HARDWARE_ASTROHOME, GBF_HORSHOOT, 0,
	AstroGetZipName, astro_cosmicrd35RomInfo, astro_cosmicrd35RomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Cosmic Raiders (Prototype, v3.6)
static struct BurnRomInfo astro_cosmicrd36RomDesc[] = {
	{ "Cosmic Raider (Proto, v3.6)(198x)(Astrocade).bin",		8192, 0x00b6fe88, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_cosmicrd36, astro_cosmicrd36, astro_astrocde)
STD_ROM_FN(astro_cosmicrd36)

struct BurnDriver BurnDrvastro_cosmicrd36 = {
	"astro_cosmicrd36", "astro_cosmicrd", "astro_astrocde", NULL, "198?",
	"Cosmic Raiders (Prototype, v3.6)\0", NULL, "Astrocade Inc.", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 4, HARDWARE_ASTROHOME, GBF_HORSHOOT, 0,
	AstroGetZipName, astro_cosmicrd36RomInfo, astro_cosmicrd36RomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Crazy Climber (HB)
static struct BurnRomInfo astro_cclimberRomDesc[] = {
	{ "Crazy Climber (2011)(RiffRaff Games).bin",		8187, 0xbf864c44, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_cclimber, astro_cclimber, astro_astrocde)
STD_ROM_FN(astro_cclimber)

struct BurnDriver BurnDrvastro_cclimber = {
	"astro_cclimber", NULL, "astro_astrocde", NULL, "2011",
	"Crazy Climber (HB)\0", NULL, "RiffRaff Games", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_ASTROHOME, GBF_ACTION, 0,
	AstroGetZipName, astro_cclimberRomInfo, astro_cclimberRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Dealer Demo (USA)
static struct BurnRomInfo astro_dealdemoRomDesc[] = {
	{ "Dealer Demo (1978)(Bally).bin",		4096, 0x16b77b44, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_dealdemo, astro_dealdemo, astro_astrocde)
STD_ROM_FN(astro_dealdemo)

struct BurnDriver BurnDrvastro_dealdemo = {
	"astro_dealdemo", NULL, "astro_astrocde", NULL, "1978",
	"Dealer Demo (USA)\0", NULL, "Bally", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_ASTROHOME, GBF_MISC, 0,
	AstroGetZipName, astro_dealdemoRomInfo, astro_dealdemoRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Dog Patch (USA)
static struct BurnRomInfo astro_dogpatchRomDesc[] = {
	{ "Dog Patch (1978)(Bally).bin",		2048, 0x77bed6ba, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_dogpatch, astro_dogpatch, astro_astrocde)
STD_ROM_FN(astro_dogpatch)

struct BurnDriver BurnDrvastro_dogpatch = {
	"astro_dogpatch", NULL, "astro_astrocde", NULL, "1978",
	"Dog Patch (USA)\0", NULL, "Bally", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ASTROHOME, GBF_ACTION, 0,
	AstroGetZipName, astro_dogpatchRomInfo, astro_dogpatchRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Eight Days A Week (Prototype)
static struct BurnRomInfo astro_eightdayRomDesc[] = {
	{ "Eight Days A Week (Proto)(198x)(Richard Degler & George Moses).bin",		4096, 0x0bac683f, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_eightday, astro_eightday, astro_astrocde)
STD_ROM_FN(astro_eightday)

struct BurnDriver BurnDrvastro_eightday = {
	"astro_eightday", NULL, "astro_astrocde", NULL, "198?",
	"Eight Days A Week (Prototype)\0", NULL, "Richard Degler & George Moses", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 1, HARDWARE_ASTROHOME, GBF_MISC, 0,
	AstroGetZipName, astro_eightdayRomInfo, astro_eightdayRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Fawn Dungeon (Prototype, first version)
static struct BurnRomInfo astro_fawnRomDesc[] = {
	{ "Fawn Dungeon (Proto 1)(1981)(Barry McCleave).bin",		2048, 0x2ff0fbef, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_fawn, astro_fawn, astro_astrocde)
STD_ROM_FN(astro_fawn)

struct BurnDriver BurnDrvastro_fawn = {
	"astro_fawn", NULL, "astro_astrocde", NULL, "1981",
	"Fawn Dungeon (Prototype, first version)\0", NULL, "Barry McCleave", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 1, HARDWARE_ASTROHOME, GBF_RPG, 0,
	AstroGetZipName, astro_fawnRomInfo, astro_fawnRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Fawn Dungeon (Prototype, second version)
static struct BurnRomInfo astro_fawnaRomDesc[] = {
	{ "Fawn Dungeon (Proto 2)(1981)(Barry McCleave).bin",		2048, 0x72dd4512, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_fawna, astro_fawna, astro_astrocde)
STD_ROM_FN(astro_fawna)

struct BurnDriver BurnDrvastro_fawna = {
	"astro_fawna", "astro_fawn", "astro_astrocde", NULL, "1981",
	"Fawn Dungeon (Prototype, second version)\0", NULL, "Barry McCleave", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 1, HARDWARE_ASTROHOME, GBF_RPG, 0,
	AstroGetZipName, astro_fawnaRomInfo, astro_fawnaRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Football (USA)
static struct BurnRomInfo astro_footballRomDesc[] = {
	{ "Football (1978)(Bally).bin",		4096, 0xfc8998a9, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_football, astro_football, astro_astrocde)
STD_ROM_FN(astro_football)

struct BurnDriver BurnDrvastro_football = {
	"astro_football", NULL, "astro_astrocde", NULL, "1978",
	"Football (USA)\0", NULL, "Bally", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_ASTROHOME, GBF_SPORTSMISC, 0,
	AstroGetZipName, astro_footballRomInfo, astro_footballRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Fun With Vectors - Ziggy (Prototype JX label)
static struct BurnRomInfo astro_ziggyjxRomDesc[] = {
	{ "Fun With Vectors - Ziggy (Proto JX label)(1985)(Richard Degler).bin",		4096, 0x286d4dd4, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_ziggyjx, astro_ziggyjx, astro_astrocde)
STD_ROM_FN(astro_ziggyjx)

struct BurnDriver BurnDrvastro_ziggyjx = {
	"astro_ziggyjx", NULL, "astro_astrocde", NULL, "1985",
	"Fun With Vectors - Ziggy (Prototype JX label)\0", NULL, "Richard Degler", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 1, HARDWARE_ASTROHOME, GBF_MISC, 0,
	AstroGetZipName, astro_ziggyjxRomInfo, astro_ziggyjxRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Fun With Vectors - Ziggy (prototype MO label)
static struct BurnRomInfo astro_ziggymoRomDesc[] = {
	{ "Fun With Vectors - Ziggy (Proto MO label)(1985)(Richard Degler).bin",		4096, 0x18a2eb1e, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_ziggymo, astro_ziggymo, astro_astrocde)
STD_ROM_FN(astro_ziggymo)

struct BurnDriver BurnDrvastro_ziggymo = {
	"astro_ziggymo", "astro_ziggyjx", "astro_astrocde", NULL, "1985",
	"Fun With Vectors - Ziggy (Prototype MO label)\0", NULL, "Richard Degler", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 1, HARDWARE_ASTROHOME, GBF_MISC, 0,
	AstroGetZipName, astro_ziggymoRomInfo, astro_ziggymoRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Galactic Invasion (USA)
static struct BurnRomInfo astro_galacticRomDesc[] = {
	{ "Galactic Invasion (1981)(Astrocade).bin",		4096, 0xba558eb7, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_galactic, astro_galactic, astro_astrocde)
STD_ROM_FN(astro_galactic)

struct BurnDriver BurnDrvastro_galactic = {
	"astro_galactic", NULL, "astro_astrocde", NULL, "1981",
	"Galactic Invasion (USA)\0", NULL, "Astrocade Inc.", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_ASTROHOME, GBF_VERSHOOT, 0,
	AstroGetZipName, astro_galacticRomInfo, astro_galacticRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Galaxian (USA)
static struct BurnRomInfo astro_galaxianRomDesc[] = {
	{ "Galaxian (1981)(Astrocade).bin",		4096, 0xd75e9672, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_galaxian, astro_galaxian, astro_astrocde)
STD_ROM_FN(astro_galaxian)

struct BurnDriver BurnDrvastro_galaxian = {
	"astro_galaxian", "astro_galactic", "astro_astrocde", NULL, "1981",
	"Galaxian (USA)\0", NULL, "Astrocade Inc.", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_ASTROHOME, GBF_VERSHOOT, 0,
	AstroGetZipName, astro_galaxianRomInfo, astro_galaxianRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Golddigger / Goldrush (Prototype)
static struct BurnRomInfo astro_golddigRomDesc[] = {
	{ "Golddigger - Goldrush (Proto)(1984)(Dave Carson & Ken Lill).bin",		2048, 0x20755476, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_golddig, astro_golddig, astro_astrocde)
STD_ROM_FN(astro_golddig)

struct BurnDriver BurnDrvastro_golddig = {
	"astro_golddig", NULL, "astro_astrocde", NULL, "1984",
	"Golddigger / Goldrush (Prototype)\0", NULL, "Dave Carson & Ken Lill", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 1, HARDWARE_ASTROHOME, GBF_MISC, 0,
	AstroGetZipName, astro_golddigRomInfo, astro_golddigRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Golddigger / Goldrush (Prototype, Alt)
static struct BurnRomInfo astro_golddigaRomDesc[] = {
	{ "Golddigger - Goldrush (Proto, Alt)(1984)(Dave Carson & Ken Lill).bin",		2048, 0x994661a1, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_golddiga, astro_golddiga, astro_astrocde)
STD_ROM_FN(astro_golddiga)

struct BurnDriver BurnDrvastro_golddiga = {
	"astro_golddiga", "astro_golddig", "astro_astrocde", NULL, "1984",
	"Golddigger / Goldrush (Prototype, Alt)\0", NULL, "Dave Carson & Ken Lill", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 1, HARDWARE_ASTROHOME, GBF_MISC, 0,
	AstroGetZipName, astro_golddigaRomInfo, astro_golddigaRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Golddigger / Goldrush (Prototype, Alt 2)
static struct BurnRomInfo astro_golddigbRomDesc[] = {
	{ "Golddigger - Goldrush (Proto, Alt 2)(1984)(Dave Carson & Ken Lill).bin",		2048, 0xface3b05, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_golddigb, astro_golddigb, astro_astrocde)
STD_ROM_FN(astro_golddigb)

struct BurnDriver BurnDrvastro_golddigb = {
	"astro_golddigb", "astro_golddig", "astro_astrocde", NULL, "1984",
	"Golddigger / Goldrush (Prototype, Alt 2)\0", NULL, "Dave Carson & Ken Lill", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 2, HARDWARE_ASTROHOME, GBF_MISC, 0,
	AstroGetZipName, astro_golddigbRomInfo, astro_golddigbRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Grand Prix / Demolition Derby (USA)
static struct BurnRomInfo astro_grandprxRomDesc[] = {
	{ "Grand Prix - Demolition Derby (1981)(Astrovision).bin",		4096, 0x7b3b7b70, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_grandprx, astro_grandprx, astro_astrocde)
STD_ROM_FN(astro_grandprx)

struct BurnDriver BurnDrvastro_grandprx = {
	"astro_grandprx", NULL, "astro_astrocde", NULL, "1981",
	"Grand Prix / Demolition Derby (USA)\0", NULL, "Astrovision Inc.", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_ASTROHOME, GBF_RACING, 0,
	AstroGetZipName, astro_grandprxRomInfo, astro_grandprxRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// I Feel Fine (Prototype)
static struct BurnRomInfo astro_feelfineRomDesc[] = {
	{ "I Feel Fine (Proto)(198x)(Richard Degler & George Moses).bin",		4096, 0x2d9d9eb0, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_feelfine, astro_feelfine, astro_astrocde)
STD_ROM_FN(astro_feelfine)

struct BurnDriver BurnDrvastro_feelfine = {
	"astro_feelfine", NULL, "astro_astrocde", NULL, "198?",
	"I Feel Fine (Prototype)\0", NULL, "Richard Degler & George Moses", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 1, HARDWARE_ASTROHOME, GBF_MISC, 0,
	AstroGetZipName, astro_feelfineRomInfo, astro_feelfineRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// ICBM Attack (USA)
static struct BurnRomInfo astro_icbmatkRomDesc[] = {
	{ "ICBM Attack (1982)(Spectre Systems).bin",		4096, 0xbc2b6fe3, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_icbmatk, astro_icbmatk, astro_astrocde)
STD_ROM_FN(astro_icbmatk)

struct BurnDriver BurnDrvastro_icbmatk = {
	"astro_icbmatk", NULL, "astro_astrocde", NULL, "1982",
	"ICBM Attack (USA)\0", NULL, "Spectre Systems", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_ASTROHOME, GBF_SHOOT, 0,
	AstroGetZipName, astro_icbmatkRomInfo, astro_icbmatkRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// ICBM Attack (Prototype)
static struct BurnRomInfo astro_icbmatkpRomDesc[] = {
	{ "ICBM Attack (Proto)(1982)(Spectre Systems).bin",		4096, 0x49ed3563, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_icbmatkp, astro_icbmatkp, astro_astrocde)
STD_ROM_FN(astro_icbmatkp)

struct BurnDriver BurnDrvastro_icbmatkp = {
	"astro_icbmatkp", "astro_icbmatk", "astro_astrocde", NULL, "1982",
	"ICBM Attack (Prototype)\0", NULL, "Spectre Systems", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 4, HARDWARE_ASTROHOME, GBF_SHOOT, 0,
	AstroGetZipName, astro_icbmatkpRomInfo, astro_icbmatkpRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Let It Be (Prototype)
static struct BurnRomInfo astro_letitbeRomDesc[] = {
	{ "Let It Be (Proto)(198x)(Richard Degler & George Moses).bin",		4096, 0xbaf0019d, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_letitbe, astro_letitbe, astro_astrocde)
STD_ROM_FN(astro_letitbe)

struct BurnDriver BurnDrvastro_letitbe = {
	"astro_letitbe", NULL, "astro_astrocde", NULL, "198?",
	"Let It Be (Prototype)\0", NULL, "Richard Degler & George Moses", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 1, HARDWARE_ASTROHOME, GBF_MISC, 0,
	AstroGetZipName, astro_letitbeRomInfo, astro_letitbeRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Letter Match / Spell 'n Score / Crosswords (USA)
static struct BurnRomInfo astro_lttrmtchRomDesc[] = {
	{ "Letter Match - Spell 'n Score - Crosswords (1978)(Bally).bin",		4096, 0x2cf68f93, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_lttrmtch, astro_lttrmtch, astro_astrocde)
STD_ROM_FN(astro_lttrmtch)

struct BurnDriver BurnDrvastro_lttrmtch = {
	"astro_lttrmtch", NULL, "astro_astrocde", NULL, "1978",
	"Letter Match / Spell 'n Score / Crosswords (USA)\0", NULL, "Bally", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_ASTROHOME, GBF_PUZZLE, 0,
	AstroGetZipName, astro_lttrmtchRomInfo, astro_lttrmtchRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Life (USA)
static struct BurnRomInfo astro_lifeRomDesc[] = {
	{ "Life (1985)(Ricard Degler).bin",		2048, 0x752d13fd, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_life, astro_life, astro_astrocde)
STD_ROM_FN(astro_life)

struct BurnDriver BurnDrvastro_life = {
	"astro_life", NULL, "astro_astrocde", NULL, "1985",
	"Life (USA)\0", NULL, "Richard Degler", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_ASTROHOME, GBF_MISC, 0,
	AstroGetZipName, astro_lifeRomInfo, astro_lifeRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Machine Language Manager (USA)
static struct BurnRomInfo astro_mlangmanRomDesc[] = {
	{ "Machine Language Manager (1982)(The Bit Fiddlers).bin",		2048, 0x5393f920, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_mlangman, astro_mlangman, astro_astrocde)
STD_ROM_FN(astro_mlangman)

struct BurnDriver BurnDrvastro_mlangman = {
	"astro_mlangman", NULL, "astro_astrocde", NULL, "1982",
	"Machine Language Manager (USA)\0", NULL, "The Bit Fiddlers", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_ASTROHOME, GBF_MISC, 0,
	AstroGetZipName, astro_mlangmanRomInfo, astro_mlangmanRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Mazeman (USA)
static struct BurnRomInfo astro_mazemanRomDesc[] = {
	{ "Mazeman (1984)(Dave Carson Software).bin",		4096, 0x565a644c, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_mazeman, astro_mazeman, astro_astrocde)
STD_ROM_FN(astro_mazeman)

struct BurnDriver BurnDrvastro_mazeman = {
	"astro_mazeman", NULL, "astro_astrocde", NULL, "1984",
	"Mazeman (USA)\0", NULL, "Dave Carson Software", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_ASTROHOME, GBF_ACTION | GBF_MAZE, 0,
	AstroGetZipName, astro_mazemanRomInfo, astro_mazemanRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Mazeman (Prototype)
static struct BurnRomInfo astro_mazemanpRomDesc[] = {
	{ "Mazeman (Proto)(1984)(Dave Carson Software).bin",		4096, 0xaf649678, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_mazemanp, astro_mazemanp, astro_astrocde)
STD_ROM_FN(astro_mazemanp)

struct BurnDriver BurnDrvastro_mazemanp = {
	"astro_mazemanp", "astro_mazeman", "astro_astrocde", NULL, "1984",
	"Mazeman (Prototype)\0", NULL, "Dave Carson Software", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 4, HARDWARE_ASTROHOME, GBF_ACTION | GBF_MAZE, 0,
	AstroGetZipName, astro_mazemanpRomInfo, astro_mazemanpRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Missile Attack (USA)
static struct BurnRomInfo astro_matkRomDesc[] = {
	{ "Missile Attack (1985)(Spectre Systems & Mike White).bin",		4096, 0x88f56c4e, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_matk, astro_matk, astro_astrocde)
STD_ROM_FN(astro_matk)

struct BurnDriver BurnDrvastro_matk = {
	"astro_matk", NULL, "astro_astrocde", NULL, "1985",
	"Missile Attack (USA)\0", NULL, "Spectre Systems & Mike White", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_ASTROHOME, GBF_SHOOT, 0,
	AstroGetZipName, astro_matkRomInfo, astro_matkRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Ms. Candyman (USA)
static struct BurnRomInfo astro_mscandyRomDesc[] = {
	{ "Ms. Candyman (1983)(L&M Software).bin",		4096, 0x14e93a41, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_mscandy, astro_mscandy, astro_astrocde)
STD_ROM_FN(astro_mscandy)

struct BurnDriver BurnDrvastro_mscandy = {
	"astro_mscandy", NULL, "astro_astrocde", NULL, "1983",
	"Ms. Candyman (USA)\0", NULL, "L&M Software", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ASTROHOME, GBF_ACTION | GBF_MAZE, 0,
	AstroGetZipName, astro_mscandyRomInfo, astro_mscandyRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Muncher (USA)
static struct BurnRomInfo astro_muncherRomDesc[] = {
	{ "Muncher (1983)(Astrocade).bin",		8192, 0xe4726657, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_muncher, astro_muncher, astro_astrocde)
STD_ROM_FN(astro_muncher)

struct BurnDriver BurnDrvastro_muncher = {
	"astro_muncher", NULL, "astro_astrocde", NULL, "1983",
	"Muncher (USA)\0", NULL, "Astrocade Inc.", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_ASTROHOME, GBF_ACTION | GBF_MAZE, 0,
	AstroGetZipName, astro_muncherRomInfo, astro_muncherRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Music Maker (USA)
static struct BurnRomInfo astro_musicmakRomDesc[] = {
	{ "Music Maker (1985)(Dave Carson Software).bin",		8192, 0xb242ef84, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_musicmak, astro_musicmak, astro_astrocde)
STD_ROM_FN(astro_musicmak)

struct BurnDriver BurnDrvastro_musicmak = {
	"astro_musicmak", NULL, "astro_astrocde", NULL, "1985",
	"Music Maker (USA)\0", NULL, "Dave Carson Software", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_ASTROHOME, GBF_MISC, 0,
	AstroGetZipName, astro_musicmakRomInfo, astro_musicmakRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Music Maker (Prototype, v3.7)
static struct BurnRomInfo astro_musicmakpRomDesc[] = {
	{ "Music Maker (Proto, v3.7)(1982)(Astrocade).bin",		8192, 0xa76828d0, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_musicmakp, astro_musicmakp, astro_astrocde)
STD_ROM_FN(astro_musicmakp)

struct BurnDriver BurnDrvastro_musicmakp = {
	"astro_musicmakp", "astro_musicmak", "astro_astrocde", NULL, "1982",
	"Music Maker (Prototype, v3.7)\0", NULL, "Astrocade Inc.", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 1, HARDWARE_ASTROHOME, GBF_MISC, 0,
	AstroGetZipName, astro_musicmakpRomInfo, astro_musicmakpRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Music Maker (Prototype, Alt)
static struct BurnRomInfo astro_musicmakp2RomDesc[] = {
	{ "Music Maker (Proto, Alt)(1982)(Astrocade).bin",		7376, 0xcc97a150, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_musicmakp2, astro_musicmakp2, astro_astrocde)
STD_ROM_FN(astro_musicmakp2)

struct BurnDriver BurnDrvastro_musicmakp2 = {
	"astro_musicmakp2", "astro_musicmak", "astro_astrocde", NULL, "1982",
	"Music Maker (Prototype, Alt)\0", NULL, "Astrocade Inc.", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 1, HARDWARE_ASTROHOME, GBF_MISC, 0,
	AstroGetZipName, astro_musicmakp2RomInfo, astro_musicmakp2RomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// No-Die! (USA)
static struct BurnRomInfo astro_nodieRomDesc[] = {
	{ "No-Die (1984)(Dave Carson & Mike White).bin",		8192, 0x784b256a, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_nodie, astro_nodie, astro_astrocde)
STD_ROM_FN(astro_nodie)

struct BurnDriver BurnDrvastro_nodie = {
	"astro_nodie", "astro_muncher", "astro_astrocde", NULL, "1984",
	"No-Die! (USA)\0", NULL, "Dave Carson & Mike White", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_ASTROHOME, GBF_ACTION | GBF_MAZE, 0,
	AstroGetZipName, astro_nodieRomInfo, astro_nodieRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Pac-Man (Prototype)
static struct BurnRomInfo astro_pacmanRomDesc[] = {
	{ "Pac-Man (Proto)(198x)(Astrocade).bin",		8192, 0x17363546, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_pacman, astro_pacman, astro_astrocde)
STD_ROM_FN(astro_pacman)

struct BurnDriver BurnDrvastro_pacman = {
	"astro_pacman", NULL, "astro_astrocde", NULL, "198?",
	"Pac-Man (Prototype)\0", NULL, "Astrocade Inc.", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 4, HARDWARE_ASTROHOME, GBF_ACTION | GBF_MAZE, 0,
	AstroGetZipName, astro_pacmanRomInfo, astro_pacmanRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Pac-Man (Prototype, Alt)
static struct BurnRomInfo astro_pacmanaRomDesc[] = {
	{ "Pac-Man (Proto, Alt)(198x)(Astrocade).bin",		8192, 0xca3a67dd, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_pacmana, astro_pacmana, astro_astrocde)
STD_ROM_FN(astro_pacmana)

struct BurnDriver BurnDrvastro_pacmana = {
	"astro_pacmana", "astro_pacman", "astro_astrocde", NULL, "198?",
	"Pac-Man (Prototype, Alt)\0", NULL, "Astrocade Inc.", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 4, HARDWARE_ASTROHOME, GBF_ACTION | GBF_MAZE, 0,
	AstroGetZipName, astro_pacmanaRomInfo, astro_pacmanaRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Pacmaze (Prototype)
static struct BurnRomInfo astro_pacmazeRomDesc[] = {
	{ "Pacmaze (198x)(Dave Carson Software).bin",		4096, 0xcff3a6cc, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_pacmaze, astro_pacmaze, astro_astrocde)
STD_ROM_FN(astro_pacmaze)

struct BurnDriver BurnDrvastro_pacmaze = {
	"astro_pacmaze", "astro_mscandy", "astro_astrocde", NULL, "198?",
	"Pacmaze (Prototype)\0", NULL, "Dave Carson Software", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 4, HARDWARE_ASTROHOME, GBF_ACTION | GBF_MAZE, 0,
	AstroGetZipName, astro_pacmazeRomInfo, astro_pacmazeRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Paperback Writer (Prototype)
static struct BurnRomInfo astro_paperbckRomDesc[] = {
	{ "Paperback Writer (Proto)(198x)(Richard Degler & George Moses).bin",		4096, 0x274766cd, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_paperbck, astro_paperbck, astro_astrocde)
STD_ROM_FN(astro_paperbck)

struct BurnDriver BurnDrvastro_paperbck = {
	"astro_paperbck", NULL, "astro_astrocde", NULL, "198?",
	"Paperback Writer (Prototype)\0", NULL, "Richard Degler & George Moses", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 1, HARDWARE_ASTROHOME, GBF_MISC, 0,
	AstroGetZipName, astro_paperbckRomInfo, astro_paperbckRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Pirate's Chase (USA)
static struct BurnRomInfo astro_pirateRomDesc[] = {
	{ "Pirate's Chase (1982)(Astrocade).bin",		4096, 0x7b1b206a, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_pirate, astro_pirate, astro_astrocde)
STD_ROM_FN(astro_pirate)

struct BurnDriver BurnDrvastro_pirate = {
	"astro_pirate", NULL, "astro_astrocde", NULL, "1982",
	"Pirate's Chase (USA)\0", NULL, "Astrocade Inc.", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ASTROHOME, GBF_ACTION | GBF_MAZE, 0,
	AstroGetZipName, astro_pirateRomInfo, astro_pirateRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Rainbow v2 (Prototype)
static struct BurnRomInfo astro_rainbow2RomDesc[] = {
	{ "Rainbow v2 (Proto)(19xx)(Hanson).bin",		2048, 0x3883484a, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_rainbow2, astro_rainbow2, astro_astrocde)
STD_ROM_FN(astro_rainbow2)

struct BurnDriver BurnDrvastro_rainbow2 = {
	"astro_rainbow2", NULL, "astro_astrocde", NULL, "19??",
	"Rainbow v2 (Prototype)\0", NULL, "Hanson", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 1, HARDWARE_ASTROHOME, GBF_MISC, 0,
	AstroGetZipName, astro_rainbow2RomInfo, astro_rainbow2RomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Red Baron / Panzer Attack (USA)
static struct BurnRomInfo astro_redbaronRomDesc[] = {
	{ "Red Baron - Panzer Attack (1978)(Bally).bin",		4096, 0xa1c7e129, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_redbaron, astro_redbaron, astro_astrocde)
STD_ROM_FN(astro_redbaron)

struct BurnDriver BurnDrvastro_redbaron = {
	"astro_redbaron", NULL, "astro_astrocde", NULL, "1978",
	"Red Baron / Panzer Attack (USA)\0", NULL, "Bally", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_ASTROHOME, GBF_MULTISHOOT, 0,
	AstroGetZipName, astro_redbaronRomInfo, astro_redbaronRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Sea Devil (USA)
static struct BurnRomInfo astro_seadevilRomDesc[] = {
	{ "Sea Devil (1983)(L&M Software).bin",		4096, 0x960b6f86, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_seadevil, astro_seadevil, astro_astrocde)
STD_ROM_FN(astro_seadevil)

struct BurnDriver BurnDrvastro_seadevil = {
	"astro_seadevil", NULL, "astro_astrocde", NULL, "1983",
	"Sea Devil (USA)\0", NULL, "L&M Software", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_ASTROHOME, GBF_HORSHOOT, 0,
	AstroGetZipName, astro_seadevilRomInfo, astro_seadevilRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Seawolf / Missile (USA)
static struct BurnRomInfo astro_seawolfRomDesc[] = {
	{ "Seawolf - Missile (1978)(Bally).bin",		2048, 0xa021735e, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_seawolf, astro_seawolf, astro_astrocde)
STD_ROM_FN(astro_seawolf)

struct BurnDriver BurnDrvastro_seawolf = {
	"astro_seawolf", NULL, "astro_astrocde", NULL, "1978",
	"Seawolf / Missile (USA)\0", NULL, "Bally", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ASTROHOME, GBF_VERSHOOT, 0,
	AstroGetZipName, astro_seawolfRomInfo, astro_seawolfRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// She Loves You (Prototype)
static struct BurnRomInfo astro_shelovesRomDesc[] = {
	{ "She Loves You (Proto)(198x)(Richard Degler & George Moses).bin",		4096, 0x7b360d3f, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_sheloves, astro_sheloves, astro_astrocde)
STD_ROM_FN(astro_sheloves)

struct BurnDriver BurnDrvastro_sheloves = {
	"astro_sheloves", NULL, "astro_astrocde", NULL, "198?",
	"She Loves You (Prototype)\0", NULL, "Richard Degler & George Moses", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 1, HARDWARE_ASTROHOME, GBF_MISC, 0,
	AstroGetZipName, astro_shelovesRomInfo, astro_shelovesRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Sloshed! (USA)
static struct BurnRomInfo astro_sloshedRomDesc[] = {
	{ "Sloshed (1982-86)(New Image).bin",		8192, 0xa33ced19, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_sloshed, astro_sloshed, astro_astrocde)
STD_ROM_FN(astro_sloshed)

struct BurnDriver BurnDrvastro_sloshed = {
	"astro_sloshed", NULL, "astro_astrocde", NULL, "1982-86",
	"Sloshed! (USA)\0", NULL, "New Image", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_ASTROHOME, GBF_MAZE, 0,
	AstroGetZipName, astro_sloshedRomInfo, astro_sloshedRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Sneaky Snake (USA)
static struct BurnRomInfo astro_sneakyRomDesc[] = {
	{ "Sneaky Snake (1983)(New Image).bin",		4096, 0x3e9f4f0d, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_sneaky, astro_sneaky, astro_astrocde)
STD_ROM_FN(astro_sneaky)

struct BurnDriver BurnDrvastro_sneaky = {
	"astro_sneaky", NULL, "astro_astrocde", NULL, "1983",
	"Sneaky Snake (USA)\0", NULL, "New Image", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_ASTROHOME, GBF_VERSHOOT, 0,
	AstroGetZipName, astro_sneakyRomInfo, astro_sneakyRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Soccer (USA)
static struct BurnRomInfo astro_soccerRomDesc[] = {
	{ "Soccer (1985)(Astrocade).bin",		8192, 0x56d28b62, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_soccer, astro_soccer, astro_astrocde)
STD_ROM_FN(astro_soccer)

struct BurnDriver BurnDrvastro_soccer = {
	"astro_soccer", NULL, "astro_astrocde", NULL, "1985",
	"Soccer (USA)\0", NULL, "Astrocade Inc.", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_ASTROHOME, GBF_SPORTSFOOTBALL, 0,
	AstroGetZipName, astro_soccerRomInfo, astro_soccerRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Soccer (Prototype)
static struct BurnRomInfo astro_soccerpRomDesc[] = {
	{ "Soccer (Proto)(198x)(Astrocade).bin",		8192, 0x7de08d9a, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_soccerp, astro_soccerp, astro_astrocde)
STD_ROM_FN(astro_soccerp)

struct BurnDriver BurnDrvastro_soccerp = {
	"astro_soccerp", "astro_soccer", "astro_astrocde", NULL, "198?",
	"Soccer (Prototype)\0", NULL, "Astrocade Inc.", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 4, HARDWARE_ASTROHOME, GBF_SPORTSFOOTBALL, 0,
	AstroGetZipName, astro_soccerpRomInfo, astro_soccerpRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Socer (Prototype, Alt)
static struct BurnRomInfo astro_socceraRomDesc[] = {
	{ "Socer (Proto, Alt)(198x)(Astrocade).bin",		4096, 0xfd633ca9, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_soccera, astro_soccera, astro_astrocde)
STD_ROM_FN(astro_soccera)

struct BurnDriver BurnDrvastro_soccera = {
	"astro_soccera", "astro_soccer", "astro_astrocde", NULL, "198?",
	"Socer (Prototype, Alt)\0", NULL, "Astrocade Inc.", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 4, HARDWARE_ASTROHOME, GBF_SPORTSFOOTBALL, 0,
	AstroGetZipName, astro_socceraRomInfo, astro_socceraRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Socer (Prototype, Alt 2)
static struct BurnRomInfo astro_soccera2RomDesc[] = {
	{ "Socer (Proto, Alt 2)(198x)(Astrocade).bin",		8192, 0x7c2858dd, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_soccera2, astro_soccera2, astro_astrocde)
STD_ROM_FN(astro_soccera2)

struct BurnDriver BurnDrvastro_soccera2 = {
	"astro_soccera2", "astro_soccer", "astro_astrocde", NULL, "198?",
	"Socer (Prototype, Alt 2)\0", NULL, "Astrocade Inc.", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 4, HARDWARE_ASTROHOME, GBF_SPORTSFOOTBALL, 0,
	AstroGetZipName, astro_soccera2RomInfo, astro_soccera2RomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Soccer (Prototype, Alt 3)
static struct BurnRomInfo astro_soccera3RomDesc[] = {
	{ "Soccer (Proto, Alt 3)(198x)(Astrocade).bin",		8192, 0x5cb41426, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_soccera3, astro_soccera3, astro_astrocde)
STD_ROM_FN(astro_soccera3)

struct BurnDriver BurnDrvastro_soccera3 = {
	"astro_soccera3", "astro_soccer", "astro_astrocde", NULL, "198?",
	"Soccer (Prototype, Alt 3)\0", NULL, "Astrocade Inc.", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 4, HARDWARE_ASTROHOME, GBF_SPORTSFOOTBALL, 0,
	AstroGetZipName, astro_soccera3RomInfo, astro_soccera3RomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Soccer (Prototype, Alt 4)
static struct BurnRomInfo astro_soccera4RomDesc[] = {
	{ "Soccer (Proto, Alt 4)(198x)(Astrocade).bin",		8192, 0x4f5e93d5, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_soccera4, astro_soccera4, astro_astrocde)
STD_ROM_FN(astro_soccera4)

struct BurnDriver BurnDrvastro_soccera4 = {
	"astro_soccera4", "astro_soccer", "astro_astrocde", NULL, "198?",
	"Soccer (Prototype, Alt 4)\0", NULL, "Astrocade Inc.", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 4, HARDWARE_ASTROHOME, GBF_SPORTSFOOTBALL, 0,
	AstroGetZipName, astro_soccera4RomInfo, astro_soccera4RomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Solar Conqueror (USA)
static struct BurnRomInfo astro_solarcnqRomDesc[] = {
	{ "Solar Conqueror (1982)(Astrocade).bin",		8192, 0xd15ce025, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_solarcnq, astro_solarcnq, astro_astrocde)
STD_ROM_FN(astro_solarcnq)

struct BurnDriver BurnDrvastro_solarcnq = {
	"astro_solarcnq", NULL, "astro_astrocde", NULL, "1982",
	"Solar Conqueror (USA)\0", NULL, "Astrocade Inc.", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_ASTROHOME, GBF_MULTISHOOT, 0,
	AstroGetZipName, astro_solarcnqRomInfo, astro_solarcnqRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Solar Conqueror (Prototype, v2.8)
static struct BurnRomInfo astro_solarcnqp28RomDesc[] = {
	{ "Solar Conqueror (Proto, v2.8)(198x)(Astrocade).bin",		8192, 0x0709b022, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_solarcnqp28, astro_solarcnqp28, astro_astrocde)
STD_ROM_FN(astro_solarcnqp28)

struct BurnDriver BurnDrvastro_solarcnqp28 = {
	"astro_solarcnqp28", "astro_solarcnq", "astro_astrocde", NULL, "198?",
	"Solar Conqueror (Prototype, v2.8)\0", NULL, "Astrocade Inc.", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 4, HARDWARE_ASTROHOME, GBF_MULTISHOOT, 0,
	AstroGetZipName, astro_solarcnqp28RomInfo, astro_solarcnqp28RomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Solar Conqueror (Prototype, v3.9)
static struct BurnRomInfo astro_solarcnqp39RomDesc[] = {
	{ "Solar Conqueror (Proto, v3.9)(198x)(Astrocade).bin",		8192, 0x8612cc7a, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_solarcnqp39, astro_solarcnqp39, astro_astrocde)
STD_ROM_FN(astro_solarcnqp39)

struct BurnDriver BurnDrvastro_solarcnqp39 = {
	"astro_solarcnqp39", "astro_solarcnq", "astro_astrocde", NULL, "198?",
	"Solar Conqueror (Prototype, v3.9)\0", NULL, "Astrocade Inc.", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 4, HARDWARE_ASTROHOME, GBF_MULTISHOOT, 0,
	AstroGetZipName, astro_solarcnqp39RomInfo, astro_solarcnqp39RomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Solar Conqueror (Prototype)
static struct BurnRomInfo astro_solarcnqpRomDesc[] = {
	{ "Solar Conqueror (Proto)(1982)(Astrocade).bin",		8192, 0x4a229478, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_solarcnqp, astro_solarcnqp, astro_astrocde)
STD_ROM_FN(astro_solarcnqp)

struct BurnDriver BurnDrvastro_solarcnqp = {
	"astro_solarcnqp", "astro_solarcnq", "astro_astrocde", NULL, "1982",
	"Solar Conqueror (Prototype)\0", NULL, "Astrocade Inc.", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 4, HARDWARE_ASTROHOME, GBF_MULTISHOOT, 0,
	AstroGetZipName, astro_solarcnqpRomInfo, astro_solarcnqpRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Songs (Prototype)
static struct BurnRomInfo astro_songsRomDesc[] = {
	{ "Songs (Proto)(19xx)(Bally).bin",		4096, 0xfe6182b3, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_songs, astro_songs, astro_astrocde)
STD_ROM_FN(astro_songs)

struct BurnDriver BurnDrvastro_songs = {
	"astro_songs", NULL, "astro_astrocde", NULL, "19??",
	"Songs (Prototype)\0", NULL, "Bally", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 1, HARDWARE_ASTROHOME, GBF_MISC, 0,
	AstroGetZipName, astro_songsRomInfo, astro_songsRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Songs (Prototype, Alt)
static struct BurnRomInfo astro_songsaRomDesc[] = {
	{ "Songs (Proto, Alt)(19xx)(Bally).bin",		8192, 0x36a3d077, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_songsa, astro_songsa, astro_astrocde)
STD_ROM_FN(astro_songsa)

struct BurnDriver BurnDrvastro_songsa = {
	"astro_songsa", "astro_songs", "astro_astrocde", NULL, "19??",
	"Songs (Prototype, Alt)\0", NULL, "Bally", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 1, HARDWARE_ASTROHOME, GBF_MISC, 0,
	AstroGetZipName, astro_songsaRomInfo, astro_songsaRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Songs (Prototype, unfinished)
static struct BurnRomInfo astro_songsuRomDesc[] = {
	{ "Songs (Proto, unfinished)(19xx)(Bally).bin.bin",		8192, 0x16326f8c, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_songsu, astro_songsu, astro_astrocde)
STD_ROM_FN(astro_songsu)

struct BurnDriver BurnDrvastro_songsu = {
	"astro_songsu", "astro_songs", "astro_astrocde", NULL, "19??",
	"Songs (Prototype, unfinished)\0", NULL, "Bally", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 1, HARDWARE_ASTROHOME, GBF_MISC, 0,
	AstroGetZipName, astro_songsuRomInfo, astro_songsuRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Space Fortress (USA)
static struct BurnRomInfo astro_spacefrtRomDesc[] = {
	{ "Space Fortress (1981)(Astrocade).bin",		4096, 0x519cc707, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_spacefrt, astro_spacefrt, astro_astrocde)
STD_ROM_FN(astro_spacefrt)

struct BurnDriver BurnDrvastro_spacefrt = {
	"astro_spacefrt", NULL, "astro_astrocde", NULL, "1981",
	"Space Fortress (USA)\0", NULL, "Astrocade Inc.", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_ASTROHOME, GBF_MULTISHOOT, 0,
	AstroGetZipName, astro_spacefrtRomInfo, astro_spacefrtRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Space Fortress (Prototype)
static struct BurnRomInfo astro_spacefrtpRomDesc[] = {
	{ "Space Fortress (Proto)(1981)(Astrocade).bin",		2048, 0xcf5012bd, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_spacefrtp, astro_spacefrtp, astro_astrocde)
STD_ROM_FN(astro_spacefrtp)

struct BurnDriver BurnDrvastro_spacefrtp = {
	"astro_spacefrtp", "astro_spacefrt", "astro_astrocde", NULL, "1981",
	"Space Fortress (Prototype)\0", NULL, "Astrocade Inc.", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 4, HARDWARE_ASTROHOME, GBF_MULTISHOOT, 0,
	AstroGetZipName, astro_spacefrtpRomInfo, astro_spacefrtpRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Space Invaders (USA)
static struct BurnRomInfo astro_spaceinvRomDesc[] = {
	{ "Space Invaders (1979)(Bally).bin",		4096, 0x52964c1f, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_spaceinv, astro_spaceinv, astro_astrocde)
STD_ROM_FN(astro_spaceinv)

struct BurnDriver BurnDrvastro_spaceinv = {
	"astro_spaceinv", NULL, "astro_astrocde", NULL, "1979",
	"Space Invaders (USA)\0", NULL, "Bally", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_ASTROHOME, GBF_VERSHOOT, 0,
	AstroGetZipName, astro_spaceinvRomInfo, astro_spaceinvRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Space Invaders (Prototype)
static struct BurnRomInfo astro_spaceinvpRomDesc[] = {
	{ "Space Invaders (Proto)(197x)(Bally).bin",		2048, 0xa542f4b2, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_spaceinvp, astro_spaceinvp, astro_astrocde)
STD_ROM_FN(astro_spaceinvp)

struct BurnDriver BurnDrvastro_spaceinvp = {
	"astro_spaceinvp", "astro_spaceinv", "astro_astrocde", NULL, "197?",
	"Space Invaders (Prototype)\0", NULL, "Bally", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 1, HARDWARE_ASTROHOME, GBF_VERSHOOT, 0,
	AstroGetZipName, astro_spaceinvpRomInfo, astro_spaceinvpRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Speed Math / Bingo Math (USA)
static struct BurnRomInfo astro_spedmathRomDesc[] = {
	{ "Speed Math - Bingo Math (1978)(Bally).bin",		2048, 0x7d30312a, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_spedmath, astro_spedmath, astro_astrocde)
STD_ROM_FN(astro_spedmath)

struct BurnDriver BurnDrvastro_spedmath = {
	"astro_spedmath", NULL, "astro_astrocde", NULL, "1978",
	"Speed Math / Bingo Math (USA)\0", NULL, "Bally", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_ASTROHOME, GBF_PUZZLE, 0,
	AstroGetZipName, astro_spedmathRomInfo, astro_spedmathRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Star Battle (USA)
static struct BurnRomInfo astro_starbttlRomDesc[] = {
	{ "Star Battle (1979)(Bally).bin",		2048, 0xd54a74c8, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_starbttl, astro_starbttl, astro_astrocde)
STD_ROM_FN(astro_starbttl)

struct BurnDriver BurnDrvastro_starbttl = {
	"astro_starbttl", NULL, "astro_astrocde", NULL, "1979",
	"Star Battle (USA)\0", NULL, "Bally", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_ASTROHOME, GBF_SHOOT, 0,
	AstroGetZipName, astro_starbttlRomInfo, astro_starbttlRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Stweek (USA)
static struct BurnRomInfo astro_stweekRomDesc[] = {
	{ "Stweek (19xx)(Bally).bin",		2048, 0xe4f3766a, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_stweek, astro_stweek, astro_astrocde)
STD_ROM_FN(astro_stweek)

struct BurnDriver BurnDrvastro_stweek = {
	"astro_stweek", NULL, "astro_astrocde", NULL, "19??",
	"Stweek (USA)\0", NULL, "Bally", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_ASTROHOME, GBF_MISC, 0,
	AstroGetZipName, astro_stweekRomInfo, astro_stweekRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Super Slope (USA)
static struct BurnRomInfo astro_sslopeRomDesc[] = {
	{ "Super Slope (1982)(Esoterica).bin",		4096, 0xdf6b2304, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_sslope, astro_sslope, astro_astrocde)
STD_ROM_FN(astro_sslope)

struct BurnDriver BurnDrvastro_sslope = {
	"astro_sslope", NULL, "astro_astrocde", NULL, "1982",
	"Super Slope (USA)\0", NULL, "Esoterica", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_ASTROHOME, GBF_SPORTSMISC, 0,
	AstroGetZipName, astro_sslopeRomInfo, astro_sslopeRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Tests (USA)
static struct BurnRomInfo astro_testsRomDesc[] = {
	{ "Tests (19xx)(Bally).bin",		4096, 0x49df19eb, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_tests, astro_tests, astro_astrocde)
STD_ROM_FN(astro_tests)

struct BurnDriver BurnDrvastro_tests = {
	"astro_tests", NULL, "astro_astrocde", NULL, "19??",
	"Tests (USA)\0", NULL, "Bally", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_ASTROHOME, GBF_MISC, 0,
	AstroGetZipName, astro_testsRomInfo, astro_testsRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Incredible Wizard, The (USA)
static struct BurnRomInfo astro_wizardRomDesc[] = {
	{ "Incredible Wizard, The (1982)(Astrocade).bin",		8192, 0x1395b130, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_wizard, astro_wizard, astro_astrocde)
STD_ROM_FN(astro_wizard)

struct BurnDriver BurnDrvastro_wizard = {
	"astro_wizard", NULL, "astro_astrocde", NULL, "1982",
	"Incredible Wizard, The (USA)\0", "(aka Wizard of Wor)", "Astrocade Inc.", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ASTROHOME, GBF_MAZE | GBF_RUNGUN, 0,
	AstroGetZipName, astro_wizardRomInfo, astro_wizardRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Tornado Baseball / Handball / Hockey / Tennis (USA)
static struct BurnRomInfo astro_baseballRomDesc[] = {
	{ "Tornado Baseball - Handball - Hockey - Tennis (1978)(Bally).bin",		4096, 0x5be548cc, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_baseball, astro_baseball, astro_astrocde)
STD_ROM_FN(astro_baseball)

struct BurnDriver BurnDrvastro_baseball = {
	"astro_baseball", NULL, "astro_astrocde", NULL, "1978",
	"Tornado Baseball / Handball / Hockey / Tennis (USA)\0", NULL, "Bally", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ASTROHOME, GBF_SPORTSMISC, 0,
	AstroGetZipName, astro_baseballRomInfo, astro_baseballRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Treasure Cove (USA)
static struct BurnRomInfo astro_treasureRomDesc[] = {
	{ "Treasure Cove (1983)(Spectre Systems).bin",		8192, 0x1447f854, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_treasure, astro_treasure, astro_astrocde)
STD_ROM_FN(astro_treasure)

struct BurnDriver BurnDrvastro_treasure = {
	"astro_treasure", NULL, "astro_astrocde", NULL, "1983",
	"Treasure Cove (USA)\0", NULL, "Spectre System", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_ASTROHOME, GBF_ACTION, 0,
	AstroGetZipName, astro_treasureRomInfo, astro_treasureRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Treasure Cove (Prototype)
static struct BurnRomInfo astro_treasurepRomDesc[] = {
	{ "Treasure Cove (Provo)(198x)(Spectre Systems).bin",		4096, 0x5379aabf, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_treasurep, astro_treasurep, astro_astrocde)
STD_ROM_FN(astro_treasurep)

struct BurnDriver BurnDrvastro_treasurep = {
	"astro_treasurep", "astro_treasure", "astro_astrocde", NULL, "198?",
	"Treasure Cove (Prototype)\0", NULL, "Spectre Systems", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 4, HARDWARE_ASTROHOME, GBF_ACTION, 0,
	AstroGetZipName, astro_treasurepRomInfo, astro_treasurepRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Video Story Book (Prototype)
static struct BurnRomInfo astro_vidstoryRomDesc[] = {
	{ "Video Story Book (Proto)(19xx).bin",		8192, 0x2cc8cc3c, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_vidstory, astro_vidstory, astro_astrocde)
STD_ROM_FN(astro_vidstory)

struct BurnDriver BurnDrvastro_vidstory = {
	"astro_vidstory", NULL, "astro_astrocde", NULL, "19??",
	"Video Story Book (Prototype)\0", NULL, "<unknown>", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 1, HARDWARE_ASTROHOME, GBF_MISC, 0,
	AstroGetZipName, astro_vidstoryRomInfo, astro_vidstoryRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// War (second printing) (HB)
static struct BurnRomInfo astro_warRomDesc[] = {
	{ "War (second printing)(2012)(RiffRaff Games).bin",		7931, 0xc1faf1bf, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_war, astro_war, astro_astrocde)
STD_ROM_FN(astro_war)

struct BurnDriver BurnDrvastro_war = {
	"astro_war", NULL, "astro_astrocde", NULL, "2012",
	"War (second printing) (HB)\0", NULL, "RiffRaff Games", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 4, HARDWARE_ASTROHOME, GBF_BREAKOUT, 0,
	AstroGetZipName, astro_warRomInfo, astro_warRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Yesterday (Prototype, no vibrato)
static struct BurnRomInfo astro_yesternvRomDesc[] = {
	{ "Yesterday (Proto, NV)(198x)(Richard Degler & George Moses).bin",		2048, 0xf400e5d4, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_yesternv, astro_yesternv, astro_astrocde)
STD_ROM_FN(astro_yesternv)

struct BurnDriver BurnDrvastro_yesternv = {
	"astro_yesternv", "astro_yesterv", "astro_astrocde", NULL, "198?",
	"Yesterday (Prototype, no vibrato)\0", NULL, "Richard Degler & George Moses", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 1, HARDWARE_ASTROHOME, GBF_MISC, 0,
	AstroGetZipName, astro_yesternvRomInfo, astro_yesternvRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};

// Yesterday (Prototype, with vibrato)
static struct BurnRomInfo astro_yestervRomDesc[] = {
	{ "Yesterday (Proto, V)(198x)(Richard Degler & George Moses).bin",		2048, 0xa4edba84, 3 | BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(astro_yesterv, astro_yesterv, astro_astrocde)
STD_ROM_FN(astro_yesterv)

struct BurnDriver BurnDrvastro_yesterv = {
	"astro_yesterv", NULL, "astro_astrocde", NULL, "198?",
	"Yesterday (Prototype, with vibrato)\0", NULL, "Richard Degler & George Moses", "Bally Astrocade",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 1, HARDWARE_ASTROHOME, GBF_MISC, 0,
	AstroGetZipName, astro_yestervRomInfo, astro_yestervRomName, NULL, NULL, NULL, NULL, HomeInputInfo, NULL,
	HomeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	352, 240, 4, 3
};
