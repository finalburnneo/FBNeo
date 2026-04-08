// FinalBurn Neo Exidy M6502-based hardware driver module
// Based on MAME driver by Aaron Giles

// weird / btanb(?)'s
// targ
//  x occasional sprite flicker before sprite comes out of car is not a bug
//    sometimes starts at a "sprite parking spot".  strange effect.
//  x occasionally enemy vehicles can break through the playfield(!)
//    I've seen this happen on pcb!
// spectar
//  x sometimes running into a wall, or running into a wall near an enemy
//    will kill you (very rarely)

#include "tiles_generic.h"
#include "m6502_intf.h"
#include "z80_intf.h"
#include "6821pia.h"
#include "samples.h"
#include "hc55516.h"
#include "stream.h"
#include "dtimer.h"
#include "dac.h"
#include "burn_gun.h" // spinner
#include <math.h>
#include "biquad.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvM6502ROM;
static UINT8 *DrvSndROM;
static UINT8 *DrvSndRAM;
static UINT8 *DrvCVSDROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvSndPROM;
static UINT8 *DrvMainRAM;
static UINT8 *DrvVideoRAM;
static UINT8 *DrvCharacterRAM; // or rom
static UINT8 *fixed_colors;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 sprite1_xpos;
static UINT8 sprite1_ypos;
static UINT8 sprite2_xpos;
static UINT8 sprite2_ypos;
static UINT8 spriteno;
static UINT8 sprite_enable;
static UINT8 color_latch[3];
static UINT8 int_condition;
static UINT8 bankdata;

static dtimer collision_timer;

static UINT8 collision_invert = 0;
static UINT8 collision_mask = 0;
static INT32 is_2bpp = 0;
static INT32 periodic_nmi = 0;
static UINT8 (*int_source)() = NULL;
static INT32 vblank;
static UINT8 last_dial;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvJoy5[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[5];
static UINT8 DrvReset;

static INT16 Analog[2];

static INT32 nExtraCycles[3];

static INT32 scanline = 0;

static INT32 is_targ = 0;
static INT32 is_venture = 0;
static INT32 is_mtrap = 0;
static INT32 has_samples = 0;

static Stream targ_stream;
static INT32 targ_tone = 0;

static BIQ biqtone;

static void targ_audio_2_w(UINT8 data); // forward
static void spectar_audio_1_w(UINT8 data); //fwd
static void sidetrac_audio_1_w(UINT8 data); //fwd
static void spectar_audio_2_w(UINT8 data); // forward

static struct BurnInputInfo SidetracInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Sidetrac)

static struct BurnInputInfo TargInputList[] = {
	{"Coin 1",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"	},
	{"Coin 2",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Targ)

static struct BurnInputInfo MtrapInputList[] = {
	{"Coin 1",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"	},
	{"Coin 2",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 fire 4"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Mtrap)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo TeetertInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	A("P1 Spinner",     BIT_ANALOG_REL, &Analog[0],		"p1 x-axis"	),
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Teetert)

static struct BurnInputInfo Pepper2InputList[] = {
	{"Coin 1",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"	},
	{"Coin 2",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Pepper2)

static struct BurnInputInfo FaxInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy4 + 1,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy4 + 7,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy4 + 6,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy4 + 5,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy4 + 4,	"p1 fire 4"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy4 + 0,	"p2 start"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy5 + 7,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy5 + 6,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy5 + 5,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy5 + 4,	"p2 fire 4"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Fax)

static struct BurnDIPInfo SidetracDIPList[]=
{
	DIP_OFFSET(0x08)
	{0x00, 0xff, 0xff, 0xf4, NULL							},
	{0x01, 0xff, 0xff, 0x00, NULL							},
	{0x02, 0xff, 0xff, 0xff, NULL							},

	{0   , 0xfe, 0   ,    4, "Lives"						},
	{0x00, 0x01, 0x03, 0x00, "2"							},
	{0x00, 0x01, 0x03, 0x01, "3"							},
	{0x00, 0x01, 0x03, 0x02, "4"							},
	{0x00, 0x01, 0x03, 0x03, "5"							},

	{0   , 0xfe, 0   ,    3, "Coinage"						},
	{0x00, 0x01, 0x0c, 0x08, "2 Coins 1 Credits"			},
	{0x00, 0x01, 0x0c, 0x04, "1 Coin  1 Credits"			},
	{0x00, 0x01, 0x0c, 0x00, "1 Coin  2 Credits"			},

	{0   , 0xfe, 0   ,    2, "Top Score Award"				},
	{0x00, 0x01, 0x10, 0x00, "Off"							},
	{0x00, 0x01, 0x10, 0x10, "On"							},
};

STDDIPINFO(Sidetrac)

static struct BurnDIPInfo TargDIPList[]=
{
	DIP_OFFSET(0x09)
	{0x00, 0xff, 0xff, 0xc8, NULL							},
	{0x01, 0xff, 0xff, 0x00, NULL							},
	{0x02, 0xff, 0xff, 0xff, NULL							},

	{0   , 0xfe, 0   ,    2, "Pence Coinage"				},
	{0x00, 0x01, 0x02, 0x00, "10P/1P, 50P Coin/6P"			},
	{0x00, 0x01, 0x02, 0x02, "2x10P/1P, 50P Coin/3P"		},

	{0   , 0xfe, 0   ,    2, "Top Score Award"				},
	{0x00, 0x01, 0x04, 0x00, "Credit"						},
	{0x00, 0x01, 0x04, 0x04, "Extended Play"				},

	{0   , 0xfe, 0   ,    4, "Quarter Coinage"				},
	{0x00, 0x01, 0x18, 0x10, "2 Coins 1 Credits"			},
	{0x00, 0x01, 0x18, 0x08, "1 Coin  1 Credits"			},
	{0x00, 0x01, 0x18, 0x00, "1C/1C (no display)"			},
	{0x00, 0x01, 0x18, 0x18, "1 Coin  2 Credits"			},

	{0   , 0xfe, 0   ,    4, "Lives"						},
	{0x00, 0x01, 0x60, 0x60, "2"							},
	{0x00, 0x01, 0x60, 0x40, "3"							},
	{0x00, 0x01, 0x60, 0x20, "4"							},
	{0x00, 0x01, 0x60, 0x00, "5"							},

	{0   , 0xfe, 0   ,    2, "Currency"						},
	{0x00, 0x01, 0x80, 0x80, "Quarters"						},
	{0x00, 0x01, 0x80, 0x00, "Pence"						},
};

STDDIPINFO(Targ)

static struct BurnDIPInfo SpectarDIPList[]=
{
	DIP_OFFSET(0x09)
	{0x00, 0xff, 0xff, 0xc8, NULL							},
	{0x01, 0xff, 0xff, 0x00, NULL							},
	{0x02, 0xff, 0xff, 0xff, NULL							},

	{0   , 0xfe, 0   ,    2, "Pence Coinage"				},
	{0x00, 0x01, 0x02, 0x00, "10P/1P, 50P Coin/6P"			},
	{0x00, 0x01, 0x02, 0x02, "2x10P/1P, 50P Coin/3P"		},

	{0   , 0xfe, 0   ,    2, "Top Score Award"				},
	{0x00, 0x01, 0x04, 0x00, "Credit"						},
	{0x00, 0x01, 0x04, 0x04, "Extended Play"				},

	{0   , 0xfe, 0   ,    4, "Quarter Coinage"				},
	{0x00, 0x01, 0x18, 0x10, "2 Coins 1 Credits"			},
	{0x00, 0x01, 0x18, 0x08, "1 Coin  1 Credits"			},
	{0x00, 0x01, 0x18, 0x00, "1C/1C (no display)"			},
	{0x00, 0x01, 0x18, 0x18, "1 Coin  2 Credits"			},

	{0   , 0xfe, 0   ,    4, "Lives"						},
	{0x00, 0x01, 0x60, 0x60, "2"							},
	{0x00, 0x01, 0x60, 0x40, "3"							},
	{0x00, 0x01, 0x60, 0x20, "4"							},
	{0x00, 0x01, 0x60, 0x00, "5"							},

	{0   , 0xfe, 0   ,    2, "Currency"						},
	{0x00, 0x01, 0x80, 0x80, "Quarters"						},
	{0x00, 0x01, 0x80, 0x00, "Pence"						},

	{0   , 0xfe, 0   ,    4, "Language"						},
	{0x01, 0x01, 0x03, 0x00, "English"						},
	{0x01, 0x01, 0x03, 0x01, "French"						},
	{0x01, 0x01, 0x03, 0x02, "German"						},
	{0x01, 0x01, 0x03, 0x03, "Spanish"						},
};

STDDIPINFO(Spectar)

static struct BurnDIPInfo SpectarrfDIPList[]=
{
	DIP_OFFSET(0x09)
	{0x00, 0xff, 0xff, 0xc8, NULL							},
	{0x01, 0xff, 0xff, 0x03, NULL							},
	{0x02, 0xff, 0xff, 0xff, NULL							},

	{0   , 0xfe, 0   ,    2, "Pence Coinage"				},
	{0x00, 0x01, 0x02, 0x00, "10P/1P, 50P Coin/6P"			},
	{0x00, 0x01, 0x02, 0x02, "2x10P/1P, 50P Coin/3P"		},

	{0   , 0xfe, 0   ,    2, "Top Score Award"				},
	{0x00, 0x01, 0x04, 0x00, "Credit"						},
	{0x00, 0x01, 0x04, 0x04, "Extended Play"				},

	{0   , 0xfe, 0   ,    4, "Quarter Coinage"				},
	{0x00, 0x01, 0x18, 0x10, "2 Coins 1 Credits"			},
	{0x00, 0x01, 0x18, 0x08, "1 Coin  1 Credits"			},
	{0x00, 0x01, 0x18, 0x00, "1C/1C (no display)"			},
	{0x00, 0x01, 0x18, 0x18, "1 Coin  2 Credits"			},

	{0   , 0xfe, 0   ,    4, "Lives"						},
	{0x00, 0x01, 0x60, 0x60, "2"							},
	{0x00, 0x01, 0x60, 0x40, "3"							},
	{0x00, 0x01, 0x60, 0x20, "4"							},
	{0x00, 0x01, 0x60, 0x00, "5"							},

	{0   , 0xfe, 0   ,    2, "Currency"						},
	{0x00, 0x01, 0x80, 0x80, "Quarters"						},
	{0x00, 0x01, 0x80, 0x00, "Pence"						},

	{0   , 0xfe, 0   ,    4, "Language"						},
	{0x01, 0x01, 0x03, 0x00, "English"						},
	{0x01, 0x01, 0x03, 0x01, "French"						},
	{0x01, 0x01, 0x03, 0x02, "German"						},
	{0x01, 0x01, 0x03, 0x03, "Spanish"						},
};

STDDIPINFO(Spectarrf)

static struct BurnDIPInfo VentureDIPList[]=
{
	DIP_OFFSET(0x09)
	{0x00, 0xff, 0xff, 0x90, NULL							},
	{0x01, 0xff, 0xff, 0x00, NULL							},
	{0x02, 0xff, 0xff, 0xff, NULL							},

	{0   , 0xfe, 0   ,    4, "Bonus Life"					},
	{0x00, 0x01, 0x06, 0x00, "20000"						},
	{0x00, 0x01, 0x06, 0x02, "30000"						},
	{0x00, 0x01, 0x06, 0x04, "40000"						},
	{0x00, 0x01, 0x06, 0x06, "50000"						},

	{0   , 0xfe, 0   ,    6, "Coinage"						},
	{0x00, 0x01, 0x98, 0x88, "2 Coins 1 Credits"			},
	{0x00, 0x01, 0x98, 0x80, "1 Coin  1 Credits"			},
	{0x00, 0x01, 0x98, 0x90, "1 Coin  1 Credits"			},
	{0x00, 0x01, 0x98, 0x98, "1 Coin  2 Credits"			},
	{0x00, 0x01, 0x98, 0x00, "Pence: A 2C/1C B 1C/3C"		},
	{0x00, 0x01, 0x98, 0x18, "Pence: A 1C/1C B 1C/6C"		},

	{0   , 0xfe, 0   ,    4, "Lives"						},
	{0x00, 0x01, 0x60, 0x00, "2"							},
	{0x00, 0x01, 0x60, 0x20, "3"							},
	{0x00, 0x01, 0x60, 0x40, "4"							},
	{0x00, 0x01, 0x60, 0x60, "5"							},
};

STDDIPINFO(Venture)

static struct BurnDIPInfo MtrapDIPList[]=
{
	DIP_OFFSET(0x0c)
	{0x00, 0xff, 0xff, 0xfe, NULL							},
	{0x01, 0xff, 0xff, 0x00, NULL							},
	{0x02, 0xff, 0xff, 0xff, NULL							},

	{0   , 0xfe, 0   ,    4, "Bonus Life"					},
	{0x00, 0x01, 0x06, 0x06, "30000"						},
	{0x00, 0x01, 0x06, 0x04, "40000"						},
	{0x00, 0x01, 0x06, 0x02, "50000"						},
	{0x00, 0x01, 0x06, 0x00, "60000"						},

	{0   , 0xfe, 0   ,    8, "Coinage"						},
	{0x00, 0x01, 0x98, 0x90, "2 Coins 1 Credits"			},
	{0x00, 0x01, 0x98, 0x00, "Coin A 2C/1C Coin B 1C/3C"	},
	{0x00, 0x01, 0x98, 0x98, "1 Coin  1 Credits"			},
	{0x00, 0x01, 0x98, 0x10, "Coin A 1C/1C Coin B 1C/4C"	},
	{0x00, 0x01, 0x98, 0x18, "Coin A 1C/1C Coin B 1C/5C"	},
	{0x00, 0x01, 0x98, 0x88, "1 Coin  2 Credits"			},
	{0x00, 0x01, 0x98, 0x08, "Coin A 1C/3C Coin B 2C/7C"	},
	{0x00, 0x01, 0x98, 0x80, "1 Coin  4 Credits"			},

	{0   , 0xfe, 0   ,    4, "Lives"						},
	{0x00, 0x01, 0x60, 0x60, "2"							},
	{0x00, 0x01, 0x60, 0x40, "3"							},
	{0x00, 0x01, 0x60, 0x20, "4"							},
	{0x00, 0x01, 0x60, 0x00, "5"							},

	{0   , 0xfe, 0   ,    2, "Free Play"					},
	{0x02, 0x01, 0x04, 0x04, "Off"							},
	{0x02, 0x01, 0x04, 0x00, "On"							},
};

STDDIPINFO(Mtrap)

static struct BurnDIPInfo TeetertDIPList[]=
{
	DIP_OFFSET(0x05)
	{0x00, 0xff, 0xff, 0xde, NULL							},
	{0x01, 0xff, 0xff, 0x00, NULL							},
	{0x02, 0xff, 0xff, 0xff, NULL							},

	{0   , 0xfe, 0   ,    4, "Bonus Life"					},
	{0x00, 0x01, 0x06, 0x06, "20000"						},
	{0x00, 0x01, 0x06, 0x04, "30000"						},
	{0x00, 0x01, 0x06, 0x02, "40000"						},
	{0x00, 0x01, 0x06, 0x00, "50000"						},

	{0   , 0xfe, 0   ,    8, "Coinage"						},
	{0x00, 0x01, 0x98, 0x90, "2 Coins 1 Credits"			},
	{0x00, 0x01, 0x98, 0x00, "Pence: A 2C/1C B 1C/3C"		},
	{0x00, 0x01, 0x98, 0x98, "1 Coin  1 Credits"			},
	{0x00, 0x01, 0x98, 0x10, "Pence: A 1C/1C B 1C/4C"		},
	{0x00, 0x01, 0x98, 0x18, "Pence: A 1C/1C B 1C/5C"		},
	{0x00, 0x01, 0x98, 0x88, "1 Coin  2 Credits"			},
	{0x00, 0x01, 0x98, 0x08, "1C/3C, 2C/7C"					},
	{0x00, 0x01, 0x98, 0x80, "1 Coin  4 Credits"			},

	{0   , 0xfe, 0   ,    4, "Lives"						},
	{0x00, 0x01, 0x60, 0x00, "5"							},
	{0x00, 0x01, 0x60, 0x20, "4"							},
	{0x00, 0x01, 0x60, 0x40, "3"							},
	{0x00, 0x01, 0x60, 0x60, "2"							},
};

STDDIPINFO(Teetert)

static struct BurnDIPInfo Pepper2DIPList[]=
{
	DIP_OFFSET(0x09)
	{0x00, 0xff, 0xff, 0xde, NULL							},
	{0x01, 0xff, 0xff, 0x00, NULL							},
	{0x02, 0xff, 0xff, 0xff, NULL							},

	{0   , 0xfe, 0   ,    4, "Bonus Life"					},
	{0x00, 0x01, 0x06, 0x06, "40000 and 80000"				},
	{0x00, 0x01, 0x06, 0x04, "50000 and 100000"				},
	{0x00, 0x01, 0x06, 0x02, "70000 and 140000"				},
	{0x00, 0x01, 0x06, 0x00, "90000 and 180000"				},

	{0   , 0xfe, 0   ,    4, "Lives"						},
	{0x00, 0x01, 0x60, 0x60, "2"							},
	{0x00, 0x01, 0x60, 0x40, "3"							},
	{0x00, 0x01, 0x60, 0x20, "4"							},
	{0x00, 0x01, 0x60, 0x00, "5"							},

	{0   , 0xfe, 0   ,    8, "Coinage"						},
	{0x00, 0x01, 0x98, 0x90, "2 Coins 1 Credits"			},
	{0x00, 0x01, 0x98, 0x00, "Coin A 2C/1C Coin B 1C/3C"	},
	{0x00, 0x01, 0x98, 0x98, "1 Coin  1 Credits"			},
	{0x00, 0x01, 0x98, 0x10, "Coin A 1C/1C Coin B 1C/4C"	},
	{0x00, 0x01, 0x98, 0x18, "Coin A 1C/1C Coin B 1C/5C"	},
	{0x00, 0x01, 0x98, 0x88, "1 Coin  2 Credits"			},
	{0x00, 0x01, 0x98, 0x08, "1 Coin/3 Credits 2C/7C"		},
	{0x00, 0x01, 0x98, 0x80, "1 Coin  4 Credits"			},
};

STDDIPINFO(Pepper2)

static struct BurnDIPInfo FaxDIPList[]=
{
	DIP_OFFSET(0x0d)
	{0x00, 0xff, 0xff, 0xdc, NULL							},
	{0x01, 0xff, 0xff, 0x00, NULL							},
	{0x02, 0xff, 0xff, 0xff, NULL							},

	{0   , 0xfe, 0   ,    4, "Bonus Time"					},
	{0x00, 0x01, 0x06, 0x06, "8000"							},
	{0x00, 0x01, 0x06, 0x04, "13000"						},
	{0x00, 0x01, 0x06, 0x02, "18000"						},
	{0x00, 0x01, 0x06, 0x00, "25000"						},

	{0   , 0xfe, 0   ,    4, "Game/Bonus Times"				},
	{0x00, 0x01, 0x60, 0x60, ":32/:24"						},
	{0x00, 0x01, 0x60, 0x40, ":48/:36"						},
	{0x00, 0x01, 0x60, 0x20, "1:04/:48"						},
	{0x00, 0x01, 0x60, 0x00, "1:12/1:04"					},

	{0   , 0xfe, 0   ,    8, "Coinage"						},
	{0x00, 0x01, 0x98, 0x90, "2 Coins 1 Credits"			},
	{0x00, 0x01, 0x98, 0x00, "Coin A 2C/1C Coin B 1C/3C"	},
	{0x00, 0x01, 0x98, 0x98, "1 Coin  1 Credits"			},
	{0x00, 0x01, 0x98, 0x10, "Coin A 1C/1C Coin B 1C/4C"	},
	{0x00, 0x01, 0x98, 0x18, "Coin A 1C/1C Coin B 1C/5C"	},
	{0x00, 0x01, 0x98, 0x88, "1 Coin  2 Credits"			},
	{0x00, 0x01, 0x98, 0x08, "1 Coin/3 Credits 2C/7C"		},
	{0x00, 0x01, 0x98, 0x80, "1 Coin  4 Credits"			},
};

STDDIPINFO(Fax)

static void latch_condition(int collision)
{
	collision ^= collision_invert;
	int_condition = (int_source() & ~0x1c) | (collision & collision_mask);
}

static void exidy_main_write(UINT16 address, UINT8 data)
{
	switch (address & 0xffc0)
	{
		case 0x5000: sprite1_xpos = data; return;
		case 0x5040: sprite1_ypos = data; return;
		case 0x5080: sprite2_xpos = data; return;
		case 0x50c0: sprite2_ypos = data; return;
	}

	switch (address & 0xff03)
	{
		case 0x5100: spriteno = data; return;
		case 0x5101: sprite_enable = data; return;
	}

	switch (address)
	{
		case 0x5210:
		case 0x5211:
		case 0x5212:
			color_latch[address & 3] = data;
		return;
	}
}

static UINT8 exidy_main_read(UINT16 address)
{
	switch (address & 0xff03)
	{
		case 0x5100: return DrvDips[0];
		case 0x5101: return DrvInputs[0]; // in0
		case 0x5103: M6502SetIRQLine(0, CPU_IRQSTATUS_NONE); return int_condition;
	}

	switch (address)
	{
		case 0x5213:
			return DrvInputs[2]; // in2
	}

	return 0;
}

static UINT8 targ_main_read(UINT16 address)
{
	switch (address & 0xff03)
	{
		case 0x5100: return (DrvDips[0] & 0xfe) | (~DrvInputs[1] & 0x01);
	}

	return exidy_main_read(address);
}

static void targ_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x5200: spectar_audio_1_w(data); return;
		case 0x5201: targ_audio_2_w(data); return;
	}

	exidy_main_write(address, data);
}

static void sidetrac_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x5200: sidetrac_audio_1_w(data); return;
		case 0x5201: targ_audio_2_w(data); return;
	}

	exidy_main_write(address, data);
}

static void spectar_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x5200: spectar_audio_1_w(data); return;
		case 0x5201: spectar_audio_2_w(data); return;
	}

	exidy_main_write(address, data);
}

static UINT8 venture_main_read(UINT16 address)
{
	switch (address & 0xff03)
	{
		case 0x5100: return (DrvDips[0] & 0xfe) | (~DrvInputs[1] & 0x01);
	}

	if ((address & 0xfff0) == 0x5200)
	{
		return pia_read(0, address & 0xf);
	}

	return exidy_main_read(address);
}


static UINT8 teetert_input_read()
{
	UINT8 dial = BurnTrackballRead(0);

	int result = (dial != last_dial) << 4;
	if (result != 0) {
		if (((dial - last_dial) & 0xff) < 0x80) {
			result |= 1;
			last_dial++;
		} else {
			last_dial--;
		}
	}

	//	0x10 - movement
	//  0x01 - forward movment (reversed dial port)
	return result * 4; // 0x11 -> 0x44
}


static UINT8 teetert_main_read(UINT16 address)
{
	switch (address & 0xff03) {
		case 0x5101: return (DrvInputs[0] & ~0x44) | (teetert_input_read() & 0x44);
	}

	return venture_main_read(address);
}

static void venture_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfff0) == 0x5200)
	{
		pia_write(0, address & 0xf, data);
		return;
	}

	exidy_main_write(address, data);
}

static UINT8 fax_main_read(UINT16 address)
{
	switch (address & 0xff03)
	{
		case 0x5100: return (DrvDips[0] & 0xfe) | (~DrvInputs[1] & 0x01);
	}

	switch (address)
	{
		case 0x1a00: return DrvInputs[4];
		case 0x1c00: return DrvInputs[3];
	}

	if ((address & 0xfff0) == 0x5200)
	{
		return pia_read(0, address & 0xf);
	}

	return exidy_main_read(address);
}

static void fax_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfff0) == 0x5200)
	{
		pia_write(0, address & 0xf, data);
		return;
	}
	
	switch (address)
	{
		case 0x2000:
			bankdata = (data & 0x1f) | 0x80;
			M6502MapMemory(DrvM6502ROM + 0x10000 + (bankdata & 0x1f) * 0x2000, 0x2000, 0x3fff, MAP_ROM);
		return;

		case 0x5213:
		case 0x5214:
		case 0x5215:
		case 0x5216:
		case 0x5217:
		return; // nop
	}

	exidy_main_write(address, data);
}

static UINT8 sidetrac_intsource()
{
	return 0xff;
}

static UINT8 targ_intsource()
{
	UINT8 ret = 0;
//	ret |= (DrvDips[1] & 0x1f);
	ret |= (DrvInputs[1] & 0x01) ? 0x00 : 0x20; // coin2
	ret |= (DrvInputs[0] & 0x80) ? 0x00 : 0x40;
	ret |= (vblank) ? 0 : 0x80;
	return ret;
}

static void cheese_soundsystem_reset(); // forward;

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	M6502Open(0);
	M6502Reset();
	M6502Close();

	if (is_venture) {
		M6502Open(1);
		M6502Reset();
		M6502Close();
	}

	if (is_mtrap) {
		ZetReset(0);
		hc55516_reset();
	}

	if (has_samples) {
		cheese_soundsystem_reset();
	}

	pia_reset();

	timerReset();

	memcpy (color_latch, fixed_colors, 3);

	sprite1_xpos = 0;
	sprite1_ypos = 0;
	sprite2_xpos = 0;
	sprite2_ypos = 0;
	spriteno = 0;
	sprite_enable = 0;
	int_condition = 0;
	bankdata = 0;
	last_dial = 0;

	memset(nExtraCycles, 0, sizeof(nExtraCycles));

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6502ROM			= Next; Next += 0x040000;

	DrvSndROM			= Next; Next += 0x010000;
	DrvCVSDROM			= Next; Next += 0x010000;

	DrvGfxROM			= Next; Next += 0x004000;

	DrvSndPROM			= Next; Next += 0x000020;

	fixed_colors		= Next; Next += 0x000010;

	DrvPalette			= (UINT32*)Next; Next += 0x0008 * sizeof(UINT32);

	AllRam				= Next;

	DrvMainRAM			= Next; Next += 0x000800;
	DrvVideoRAM			= Next; Next += 0x000400;
	DrvCharacterRAM		= Next; Next += 0x001000;

	DrvSndRAM			= Next; Next += 0x0000800;

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[1]  = { 0 };
	INT32 XOffs[16] = { STEP8(0,1), STEP8(128,1) };
	INT32 YOffs[16] = { STEP16(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x800);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM, 0x800);

	GfxDecode(0x40, 1, 16, 16, Plane, XOffs, YOffs, 0x100, tmp, DrvGfxROM);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvLoadROMs(INT32 prg_load, INT32 snd_load)
{
	char* pRomName;
	struct BurnRomInfo ri;

	UINT8 *pLoad = DrvM6502ROM + prg_load;
	UINT8 *gLoad = DrvGfxROM;
	UINT8 *sLoad = DrvSndROM + snd_load;
	UINT8 *cLoad = DrvCVSDROM;

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {

		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & 7) == 1) {
			if (BurnLoadRom(pLoad, i, 1)) return 1;
			pLoad += ri.nLen;
			continue;
		}

		if ((ri.nType & 7) == 2) {
			if (BurnLoadRom(gLoad, i, 1)) return 1;
			gLoad += ri.nLen;
			continue;
		}

		if ((ri.nType & 7) == 3) {
			if (BurnLoadRom(sLoad, i, 1)) return 1;
			sLoad += ri.nLen;

			continue;
		}

		if ((ri.nType & 7) == 4) {
			if (BurnLoadRom(cLoad, i, 1)) return 1;
			cLoad += ri.nLen;

			continue;
		}
	}

	return 0;
}

// cheese_soundsystem, used by Targ, Spectar, Sidetrac
// note: i named it -dink
static void playex(INT32 sam, double volume, INT32 rate, bool checkplay, bool loop)
{
	BurnSampleSetRoute(sam, BURN_SND_SAMPLE_ROUTE_1, volume, BURN_SND_ROUTE_BOTH);
	BurnSampleSetRoute(sam, BURN_SND_SAMPLE_ROUTE_2, volume, BURN_SND_ROUTE_BOTH);

	BurnSampleSetLoop(sam, loop);
	BurnSampleSetPlaybackRate(sam, rate);

	if ( (checkplay && BurnSampleGetStatus(sam) == SAMPLE_STOPPED) || !checkplay )
		BurnSamplePlay(sam);
}

static void playexch(INT32 ch, INT32 sam, double volume, INT32 rate, bool checkplay, bool loop)
{
	BurnSampleSetRoute(sam, BURN_SND_SAMPLE_ROUTE_1, volume, BURN_SND_ROUTE_BOTH);
	BurnSampleSetRoute(sam, BURN_SND_SAMPLE_ROUTE_2, volume, BURN_SND_ROUTE_BOTH);

	BurnSampleSetPlaybackRate(sam, rate);

	if ( (checkplay && BurnSampleGetChannelStatus(sam) == SAMPLE_STOPPED) || !checkplay )
		BurnSampleChannelPlay(ch, sam, loop);
}

static UINT8 port_1_last = 0;
static UINT8 port_2_last = 0;
static UINT8 tone_pointer = 0;
static UINT8 sidetrac_chug = 0;

// Square Oscillator: used by Targ, Spectar, Sidetrack.
// "Tone/Game" in schematics

enum OscState {
	OSC_STOPPED = 0,
    OSC_STOP_PENDING,
	OSC_RUNNING
};

struct SquareOsc {
    float phase;
	float sample_rate;
	float freq;
    OscState state;
    INT32 last_half;
};

static SquareOsc oscy;

static void square_init(SquareOsc *osc)
{
    osc->phase = 0.0f;
    osc->sample_rate = nBurnSoundRate;
    osc->state = OSC_STOPPED;
    osc->last_half = 0;
}

static void square_start(SquareOsc *osc, float freq)
{
	osc->state = OSC_RUNNING;
	osc->freq = freq / osc->sample_rate;
}

static void square_stop(SquareOsc *osc)
{
    if (osc->state == OSC_RUNNING)
        osc->state = OSC_STOP_PENDING;
}

static void square_process(SquareOsc *osc,
                         float amplitude,
                         INT16 *out,
                         INT32 frames)
{
    float phase = osc->phase;
    float dt = osc->freq;

    for (INT32 i = 0; i < frames; i++) {

        int half = (phase >= 0.5f);

        // Stop exactly at half-cycle boundary
        if (osc->state == OSC_STOPPED || (osc->state == OSC_STOP_PENDING &&
            half != osc->last_half) ) {

            osc->state = OSC_STOPPED;
            out[i] = biqtone.filter(0.0f);
            continue;
        }

        osc->last_half = half;

        float v = half ? -1.0f : 1.0f;

		out[i] = biqtone.filter((INT16)v * amplitude);

		phase += dt;
        if (phase >= 1.0f)
            phase -= 1.0f;
    }

    osc->phase = phase;
}

static void adjust_square(INT32 freq)
{
	targ_stream.update();

	switch (freq) {
	    case 0x00:
		case 0xff:
			square_stop(&oscy);
			break;

		default:
			// Tone Generator
			// Tone is derived from starting frequency:
			// Targ: 10khz (pg. 4, section C)
			//   https://archive.org/details/ArcadeGameManualTarg1.1
			// Spectar 34khz (pg. 22, section 4)
			//   http://pdf.textfiles.com/manuals/ARCADE/SMALLFILENAME/spectar.pdf
			freq = ((targ_tone ? (10000) : 34000) / 2) / (0xff - freq);

			square_start(&oscy, freq);

			break;
	}
}

// Ding: SideTrack makes this sound when picking up a passenger
// simple square-wave oscillator with 31step decay
struct DingChannel {
    INT32 is_playing;
	double phase;
	double phase_inc;

	double time_elapsed;
    double time_inc;
};

static DingChannel dchannel;

static void ding_init(DingChannel *s)
{
	memset(s, 0, sizeof(*s));
}

static void ding_start(DingChannel *channel, INT32 speed) {
    channel->is_playing = 1;
    channel->time_elapsed = 0.0;
    channel->phase = 0.0;
	channel->phase_inc = (double)speed / nBurnSoundRate;
	channel->time_inc = (double)1.0 / nBurnSoundRate;
}

static void render_ding(DingChannel *channel, INT16 *buffer, int num_samples) {
	if (channel->is_playing == 0) return;

	for (INT32 i = 0; i < num_samples; i++) {
        channel->phase += channel->phase_inc;
        if (channel->phase >= 1.0) channel->phase -= 1.0;

        double wave = (channel->phase < 0.5) ? 1.0 : -1.0;

        double smooth_env = exp(-6.5 * channel->time_elapsed);
        int vol_level = (int)(smooth_env * 31.0);
        double retro_env = vol_level / 31.0;

		INT16 mix = (INT16)(wave * retro_env * 2047.0);
        buffer[0] = BURN_SND_CLIP(buffer[0] + mix);
		buffer[1] = BURN_SND_CLIP(buffer[1] + mix);
		buffer++; buffer++;

        channel->time_elapsed += channel->time_inc;

		if (retro_env == 0.0) {
            channel->is_playing = 0;
            break;
        }
    }
}

// Horn: used by SideTrack when starting a game, and when
// player's train gets a new car added
// similar to ding, but Sawtooth oscillator

struct HornSynth {
    INT32 is_playing;
    double phase;
    double phase_inc;

	double decay;
    double time_elapsed;
    double time_inc;
};

static HornSynth horn;

static void horn_init(HornSynth *s)
{
    memset(s, 0, sizeof(*s));
}

static void horn_start(HornSynth *s, float freq, float duration)
{
	s->is_playing = 1;
    s->phase = 0.0;

    s->phase_inc = freq / nBurnSoundRate;

	s->time_inc = (double)1.00 / nBurnSoundRate;
	s->time_elapsed = 0;
	s->decay = duration;
}

static void render_horn(HornSynth *s, INT16 *buffer, int num_samples)
{
	if (s->is_playing == 0) return;

	for (INT32 i = 0; i < num_samples; i++) {
		float mix = (2.0 * s->phase - 1.0); // sawtooth osc

		s->phase += s->phase_inc;
		if (s->phase >= 1.0) s->phase -= 1.0;

		s->time_elapsed += s->time_inc;
		if (s->time_elapsed >= s->decay) {
			s->is_playing = 0;
		}

		mix *= 0.10f; // mix volume

		if (mix > 1.0f) mix = 1.0f;
		if (mix < -1.0f) mix = -1.0f;

		INT16 imix = (INT16)(mix * 32767.0f);

		buffer[0] = BURN_SND_CLIP(buffer[0] + imix);
		buffer[1] = BURN_SND_CLIP(buffer[1] + imix);
		buffer++; buffer++;
	}
}

static void sidetrac_audio_1_w(UINT8 data)
{
	UINT8 Low  = (port_1_last ^ data) & ~data;
	UINT8 High = (port_1_last ^ data) & data;

	bool dac = false;
	// CPU music
	if ((port_1_last ^ data) & 1) {
		dac = true;
		DACWrite(0, (data & 1) * 0xff);
	}

	//if ((Low || High) && dac==false) bprintf(0, _T("p1 (%x) low:  %x\thi:  %x\tframe:  %d   TC: %d\n"), data, Low, High, nCurrentFrame, M6502TotalCycles());

	if (High & 2) {
		const int hz[4] = { 2776, 1846, 1409, 1114 };
		const int our_hz = hz[(data & 0xc) >> 2];
		ding_start(&dchannel, our_hz);
	}

	if (High & 0x10) { // HORN
		horn_start(&horn, 193, 0.60);
	}

	// choo, crash
	if (High & 0x20) {
		const int chug[4] = { -4, 4, 8, -8 }; // give the choo rythm
		sidetrac_chug++;
	    playex((data & 0x40) ? 0 : 1, 0.15, 100 + (chug[sidetrac_chug & 3]), false, false);
	}

	// Game / Tone
	if (Low & 0x80 || High & 0x80) {
		tone_pointer = 0;
		adjust_square(0);
	}

	port_1_last = data;
}

static void spectar_audio_1_w(UINT8 data)
{
	UINT8 Low  = (port_1_last ^ data) & ~data;
	UINT8 High = (port_1_last ^ data) & data;

	bool dac = false;
	// CPU music
	if ((port_1_last ^ data) & 1) {
		dac = true;
		DACWrite(0, (data & 1) * 0xff);
	}

//	if ((Low || High) && dac==false) bprintf(0, _T("p1(%d) (%x) low:  %x\thi:  %x\tframe:  %d   TC: %d\n"), is_targ, data, Low, High, nCurrentFrame, M6502TotalCycles());

	// shot
	if ((Low | High) & 2) {
		playex((is_targ) ? 1 : 5, 0.60, 100, (Low & 2) ? true : false, false);
	}

	// crash
	if (is_targ) {
		if (High & 0x20) {
			playex((High == 0x60) ? 2 : 0, 0.25, 100, false, false);
		}
	} else { // spectar
		if (High & 0x20) {
			playex((data & 0x40) ? 0 : 2, 0.15, 100, false, false);
		}
	}

	// Sspec (siren noise)
	if (data & 0x10) {
		if (BurnSampleGetChannelStatus(0) == SAMPLE_PLAYING)
		{
			BurnSampleChannelStop(0);
		}
	}
	else
	{
		if ((data & 0x08) != (port_1_last & 0x08)) {
			playexch(0, (data & 0x08) ? 3 : 4, 0.25, 100, false, true);
		}
	}

	// Game / Tone
	if (Low & 0x80 || High & 0x80) {
		tone_pointer = 0;
		adjust_square(0);
	}

	port_1_last = data;
}


static void spectar_audio_2_w(UINT8 data)
{
	adjust_square(data);
}

static void targ_audio_2_w(UINT8 data)
{
	UINT8 Low  = (port_2_last ^ data) & ~data;
	UINT8 High = (port_2_last ^ data) & data;
	//if (Low || High) bprintf(0, _T("p2 low:  %x\thi:  %x\tframe:  %d\n"), Low, High, nCurrentFrame);

	if (High & 1) {
		// play
		tone_pointer = (tone_pointer + 1) & 0x0f;
		adjust_square(DrvSndROM[((data & 0x02) << 3) | tone_pointer]);
	}

	if (Low & 1) {
		// stop
		adjust_square(0);
	}

	port_2_last = data;
}


static void sound_tone_render(INT16 **streams, INT32 len)
{
	INT16 *dest = streams[0];

	memset(dest, 0, len * sizeof(INT16));

	square_process(&oscy, 0.25 * 0x7fff/*volume*/, dest, len);
}


static void cheese_soundsystem_init(INT32 targ_mode)
{
	BurnSampleInit(0);
	BurnSampleSetBuffered(M6502TotalCycles, 705562);
	BurnSampleSetAllRoutesAllSamples(1.00, BURN_SND_ROUTE_BOTH);
	has_samples = 1;

	// "tone" generator
	// it's hooked up to stream.h for sub-frame updates
	targ_tone = targ_mode;
	square_init(&oscy);
	targ_stream.init(nBurnSoundRate, nBurnSoundRate, 1, 1, sound_tone_render);
    targ_stream.set_volume(0.35);
	targ_stream.set_buffered(M6502TotalCycles, 705562);
	biqtone.init(FILT_LOWPASS, nBurnSoundRate, 2800, 0.86, 0);

	horn_init(&horn); // sidetrac horn (traincar added)
	ding_init(&dchannel); // sidetrac "ding" (pickup passenger)

	DACInit(0, 0, 1, M6502TotalCycles, 705562);
	DACSetRoute(0, 0.25, BURN_SND_ROUTE_BOTH);
	DACDCBlock(1);
}

static void cheese_soundsystem_exit()
{
	BurnSampleExit();

	targ_stream.exit();
	biqtone.exit();

	DACExit();

	has_samples = 0;
	targ_tone = 0;
}

static void cheese_soundsystem_reset()
{
	BurnSampleReset();

	square_init(&oscy);
	biqtone.reset();
	horn_init(&horn);
	ding_init(&dchannel);

	DACReset();
}

static void cheese_soundsystem_scan(INT32 nAction, INT32 *pnMin)
{
	BurnSampleScan(nAction, pnMin);
	DACScan(nAction, pnMin);

	SCAN_VAR(horn);
	SCAN_VAR(dchannel);
	SCAN_VAR(oscy);
	biqtone.scan();

	SCAN_VAR(port_1_last);
	SCAN_VAR(port_2_last);
	SCAN_VAR(tone_pointer);
	SCAN_VAR(sidetrac_chug);
}

static void cheese_soundsystem_render(INT16 *pbso, INT32 len)
{
	BurnSoundClear();
	BurnSampleRender(pbso, len);
	DACUpdate(pbso, len);
	render_ding(&dchannel, pbso, len);
	render_horn(&horn, pbso, len);
	targ_stream.render(pbso, len);
}

static void latch_collision_timer_cb(int param); // forward

static void ExidyCommonInit(UINT8 (*int_cb)(), UINT8 collisionmask, UINT8 collisioninvert, INT32 is2bpp, UINT32 gfxsize, UINT8 r, UINT8 g, UINT8 b)
{
	collision_mask = collisionmask;
	collision_invert = collisioninvert;

	fixed_colors[0] = b;
	fixed_colors[1] = g;
	fixed_colors[2] = r;

	int_source = int_cb;

	is_2bpp = is2bpp;

	GenericTilesInit();
	GenericTilemapSetGfx(0, DrvGfxROM, 1, 16, 16, gfxsize, 0, 1);
	BurnBitmapAllocate(1, 16, 16, false);
	BurnBitmapAllocate(2, 16, 16, false);
	BurnBitmapAllocate(3, 16, 16, false);

	timerInit();
	timerAdd(collision_timer, 0, latch_collision_timer_cb);
	M6502Open(0);
	M6502SetCallback(timerRun);
	M6502Close();
}

static INT32 SidetracInit()
{
	BurnAllocMemIndex();

	{
		if (DrvLoadROMs(0x2800, 0)) return 1;

		memcpy (DrvM6502ROM + 0x4800, DrvM6502ROM + 0x4000, 0x800);
		DrvCharacterRAM = DrvM6502ROM + 0x4800;
		DrvGfxDecode();
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvMainRAM,				0x0000, 0x03ff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM + 0x0800,	0x0800, 0x3fff, MAP_ROM);
	M6502MapMemory(DrvVideoRAM,				0x4000, 0x43ff, MAP_RAM);
	M6502MapMemory(DrvVideoRAM,				0x4400, 0x47ff, MAP_RAM); // mirror
	M6502MapMemory(DrvM6502ROM + 0x4800,	0x4800, 0x4bff, MAP_ROM); // video rom
	M6502MapMemory(DrvM6502ROM + 0x3f00,	0xff00, 0xffff, MAP_ROM); // vectors
	M6502SetWriteHandler(sidetrac_main_write);
	M6502SetReadHandler(targ_main_read);
	M6502Close();

	pia_init(); // not in this set

	cheese_soundsystem_init(1);

	ExidyCommonInit(sidetrac_intsource, 0, 0, 0, 0x1000, 0xf8, 0xdc, 0xb8);

	DrvDoReset();

	return 0;
}

static INT32 TargInit()
{
	BurnAllocMemIndex();

	is_targ = 1;

	{
		if (DrvLoadROMs(0x1800, 0)) return 1;

		DrvGfxDecode();
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvMainRAM,				0x0000, 0x03ff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM + 0x0800,	0x0800, 0x3fff, MAP_ROM);
	M6502MapMemory(DrvVideoRAM,				0x4000, 0x43ff, MAP_RAM);
	M6502MapMemory(DrvVideoRAM,				0x4400, 0x47ff, MAP_RAM); // mirror
	M6502MapMemory(DrvCharacterRAM,			0x4800, 0x4fff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM + 0x3f00,	0xff00, 0xffff, MAP_ROM); // vectors
	M6502SetWriteHandler(targ_main_write);
	M6502SetReadHandler(targ_main_read);
	M6502Close();

	pia_init(); // not in this set

	cheese_soundsystem_init(1);

	ExidyCommonInit(targ_intsource, 0, 0, 0, 0x2000, 0x54, 0xee, 0x6b);

	DrvDoReset();

	return 0;
}

static INT32 SpectarCommonInit(INT32 load_addr)
{
	BurnAllocMemIndex();

	{
		if (DrvLoadROMs(load_addr, 0)) return 1;

		memcpy (DrvGfxROM, DrvGfxROM + 0x400, 0x400); // top is blank

		DrvGfxDecode();
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvMainRAM,				0x0000, 0x03ff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM + 0x0800,	0x0800, 0x3fff, MAP_ROM);
	M6502MapMemory(DrvVideoRAM,				0x4000, 0x43ff, MAP_RAM);
	M6502MapMemory(DrvVideoRAM,				0x4400, 0x47ff, MAP_RAM); // mirror
	M6502MapMemory(DrvCharacterRAM,			0x4800, 0x4fff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM + 0x3f00,	0xff00, 0xffff, MAP_ROM); // vectors
	M6502SetWriteHandler(spectar_main_write);
	M6502SetReadHandler(targ_main_read);
	M6502Close();

	pia_init(); // not in this set

	cheese_soundsystem_init(0);

	ExidyCommonInit(targ_intsource, 0, 0, 0, 0x2000, 0xda, 0xee, 0x61);

	DrvDoReset();

	return 0;
}

static INT32 SpectarInit()
{
	return SpectarCommonInit(0x1000);
}

static INT32 Spectar1Init()
{
	return SpectarCommonInit(0x0800);
}

// sound custom jib (c) Aaron Giles
#define CRYSTAL_OSC			(3579545)
#define SH8253_CLOCK		(CRYSTAL_OSC/2)
#define SH6840_CLOCK		(CRYSTAL_OSC/4)
#define SH6532_CLOCK		(CRYSTAL_OSC/4)
#define SH6532_PERIOD		(1.0 / (double)SH6532_CLOCK)
#define CVSD_CLOCK_FREQ 	(1000000.0 / 34.0)
#define BASE_VOLUME			(32767 / 6)

enum
{
	RIOT_IDLE,
	RIOT_COUNT,
	RIOT_POST_COUNT
};



/*************************************
 *
 *	Local variables
 *
 *************************************/

/* IRQ variables */
static UINT8 pia_irq_state;
static UINT8 riot_irq_state;

/* 6532 variables */
//static void *riot_timer;
static dtimer d_timer; // dink's timer, keeps track of things because he's really bad at time.

static UINT8 riot_irq_flag;
static UINT8 riot_timer_irq_enable;
static UINT8 riot_PA7_irq_enable;
static UINT8 riot_porta_data;
static UINT8 riot_porta_ddr;
static UINT8 riot_portb_data;
static UINT8 riot_portb_ddr;
static INT32 riot_interval;
static UINT8 riot_state;

/* 6840 variables */
struct sh6840_timer_channel
{
	UINT8	cr;
	UINT8	state;
	UINT8	leftovers;
	UINT16	timer;
	UINT32	clocks;
	union
	{
#ifdef MSB_FIRST
		struct { UINT8 h, l; } b;
#else
		struct { UINT8 l, h; } b;
#endif
		UINT16 w;
	} counter;
};
static struct sh6840_timer_channel sh6840_timer[3];
static INT16 sh6840_volume[3];
static UINT8 sh6840_MSB;
static UINT8 sh6840_noise_state;
static UINT8 sh6840_noise_history;
static UINT32 sh6840_clocks_per_sample;
static UINT32 sh6840_clock_count;
static UINT8 exidy_sfxctrl;

/* 8253 variables */
struct sh8253_timer_channel
{
	UINT8	clstate;
	UINT8	enable;
	UINT16	count;
	UINT32	step;
	UINT32	fraction;
};
static struct sh8253_timer_channel sh8253_timer[3];

/* 5220/CVSD variables */
static UINT8 has_hc55516;
//static UINT8 has_tms5220;

/* sound streaming variables */
static Stream exidy_stream;
static double freq_to_step;

static void update_irq_state()
{
	M6502SetIRQLine(1, 0, (pia_irq_state | riot_irq_state) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}


static void exidy_irq(int state)
{
	pia_irq_state = state;
	update_irq_state();
}


/*************************************
 *
 *	6840 clock counting helper
 *
 *************************************/

static void sh6840_apply_clock(struct sh6840_timer_channel *t, int clocks)
{
	/* dual 8-bit case */
	if (t->cr & 0x04)
	{
		/* handle full decrements */
		while (clocks > t->counter.b.l)
		{
			clocks -= t->counter.b.l + 1;
			t->counter.b.l = t->timer;

			/* decrement MSB */
			if (!t->counter.b.h--)
			{
				t->state = 0;
				t->counter.w = t->timer;
			}

			/* state goes high when MSB is 0 */
			else if (!t->counter.b.h)
			{
				t->state = 1;
				t->clocks++;
			}
		}

		/* subtract off the remainder */
		t->counter.b.l -= clocks;
	}

	/* 16-bit case */
	else
	{
		/* handle full decrements */
		while (clocks > t->counter.w)
		{
			clocks -= t->counter.w + 1;
			t->state ^= 1;
			t->clocks += t->state;
			t->counter.w = t->timer;
		}

		/* subtract off the remainder */
		t->counter.w -= clocks;
	}
}



/*************************************
 *
 *	Noise generation helper
 *
 *************************************/

static int sh6840_update_noise(int clocks)
{
	UINT8 history = sh6840_noise_history;
	int noise_clocks = 0;
	int i;

	/* loop over clocks */
	for (i = 0; i < clocks * 2; i++)
	{
		/* keep a history of the last few noise samples */
		history = (history << 1) | (rand() & 1);

		/* if we clocked 0->1, that will serve as an external clock */
		if ((history & 0x03) == 0x01)
		{
			sh6840_noise_state ^= 1;
			noise_clocks += sh6840_noise_state;
		}
	}

	/* remember the history for next time */
	sh6840_noise_history = history;
	return noise_clocks;
}



/*************************************
 *
 *	Core sound generation
 *
 *************************************/

static void exidy_stream_update(INT16 **streams, INT32 length)
{
	INT16 *buffer = streams[0];
	memset(buffer, 0, length*sizeof(INT16));

	int noisy = ((sh6840_timer[0].cr & sh6840_timer[1].cr & sh6840_timer[2].cr & 0x02) == 0);

	/* loop over samples */
	while (length--)
	{
		struct sh6840_timer_channel *t;
		struct sh8253_timer_channel *c;
		int clocks;
		int sample = 0;

		/* determine how many 6840 clocks this sample */
		sh6840_clock_count += sh6840_clocks_per_sample;
		int clocks_this_sample = sh6840_clock_count >> 24;
		sh6840_clock_count &= (1 << 24) - 1;

		/* skip if nothing enabled */
		if ((sh6840_timer[0].cr & 0x01) == 0)
		{
			int noise_clocks_this_sample = 0;
			UINT32 chan0_clocks;

			/* generate E-clocked noise if necessary */
			if (noisy && !(exidy_sfxctrl & 0x01))
				noise_clocks_this_sample = sh6840_update_noise(clocks_this_sample);

			/* handle timer 0 if enabled */
			t = &sh6840_timer[0];
			chan0_clocks = t->clocks;
			clocks = (t->cr & 0x02) ? clocks_this_sample : noise_clocks_this_sample;
			sh6840_apply_clock(t, clocks);
			if (t->state && !(exidy_sfxctrl & 0x02) && (t->cr & 0x80))
				sample += sh6840_volume[0];

			/* generate channel 0-clocked noise if necessary */
			if (noisy && (exidy_sfxctrl & 0x01))
				noise_clocks_this_sample = sh6840_update_noise(t->clocks - chan0_clocks);

			/* handle timer 1 if enabled */
			t = &sh6840_timer[1];
			clocks = (t->cr & 0x02) ? clocks_this_sample : noise_clocks_this_sample;
			sh6840_apply_clock(t, clocks);
			if (t->state && (t->cr & 0x80))
				sample += sh6840_volume[1];

			/* handle timer 2 if enabled */
			t = &sh6840_timer[2];
			clocks = (t->cr & 0x02) ? clocks_this_sample : noise_clocks_this_sample;

			/* prescale */
			if (t->cr & 0x01)
			{
				clocks += t->leftovers;
				t->leftovers = clocks % 8;
				clocks /= 8;
			}
			sh6840_apply_clock(t, clocks);
			if (t->state && (t->cr & 0x80))
				sample += sh6840_volume[2];
		}

		/* music channel 0 */
		c = &sh8253_timer[0];
		if (c->enable)
		{
			c->fraction += c->step;
			if (c->fraction & 0x0800000)
				sample += BASE_VOLUME;
		}

		/* music channel 1 */
		c = &sh8253_timer[1];
		if (c->enable)
		{
			c->fraction += c->step;
			if (c->fraction & 0x0800000)
				sample += BASE_VOLUME;
		}

		/* music channel 2 */
		c = &sh8253_timer[2];
		if (c->enable)
		{
			c->fraction += c->step;
			if (c->fraction & 0x0800000)
				sample += BASE_VOLUME;
		}

		/* stash */
		*buffer = BURN_SND_CLIP(sample);
		buffer++;

		d_timer.run(clocks_this_sample); // same speed (3579545/4) as riot
	}
}


static void pia_0_porta_w(UINT16 offset, UINT8 data) { pia_set_input_a(0, data); }
static void pia_1_porta_w(UINT16 offset, UINT8 data) { pia_set_input_a(1, data); }

static void pia_0_portb_w(UINT16 offset, UINT8 data) { pia_set_input_b(0, data); }
static void pia_1_portb_w(UINT16 offset, UINT8 data) { pia_set_input_b(1, data); }

static void pia_0_ca1_w(UINT16 offset, UINT8 data) { pia_set_input_ca1(0, data); }
static void pia_1_ca1_w(UINT16 offset, UINT8 data) { pia_set_input_ca1(1, data); }

static void pia_0_cb1_w(UINT16 offset, UINT8 data) { pia_set_input_cb1(0, data); }
static void pia_1_cb1_w(UINT16 offset, UINT8 data) { pia_set_input_cb1(1, data); }

static pia6821_interface main_pia =
{
	0, 0, 0, 0, 0, 0,
	pia_1_portb_w, pia_1_porta_w, pia_1_cb1_w, pia_1_ca1_w,
	0, 0
};

static pia6821_interface snd_pia =
{
	0, 0, 0, 0, 0, 0,
	pia_0_portb_w, pia_0_porta_w, pia_0_cb1_w, pia_0_ca1_w,
	0, exidy_irq
};

static void riot_interrupt(int parm); // forward

static void venture_sound_reset()
{
	pia_irq_state = 0;
	riot_irq_state = 0;
	riot_irq_flag = 0;
	riot_timer_irq_enable = 0;
	riot_PA7_irq_enable = 0;
	riot_porta_data = 0xff;
	riot_porta_ddr = 0;
	riot_portb_data = 0xff;
	riot_portb_ddr = 0;
	riot_interval = 0;
	riot_state = RIOT_IDLE;

	memset(sh6840_timer, 0, sizeof(sh6840_timer));
	memset(sh6840_volume, 0, sizeof(sh6840_volume));
	sh6840_MSB = 0;
	sh6840_noise_state = 0;
	sh6840_noise_history = 0;
	sh6840_clock_count = 0;

	exidy_sfxctrl = 0;

	memset(sh8253_timer, 0, sizeof(sh8253_timer));
}

static void venture_sound_scan()
{
	SCAN_VAR(pia_irq_state);
	SCAN_VAR(riot_irq_state);
	SCAN_VAR(riot_irq_flag);
	SCAN_VAR(riot_timer_irq_enable);
	SCAN_VAR(riot_PA7_irq_enable);
	SCAN_VAR(riot_porta_data);
	SCAN_VAR(riot_porta_ddr);
	SCAN_VAR(riot_portb_data);
	SCAN_VAR(riot_portb_ddr);
	SCAN_VAR(riot_interval);
	SCAN_VAR(riot_state);

	SCAN_VAR(sh6840_timer);
	SCAN_VAR(sh6840_volume);
	SCAN_VAR(sh6840_MSB);
	SCAN_VAR(sh6840_noise_state);
	SCAN_VAR(sh6840_noise_history);
	SCAN_VAR(sh6840_clock_count);

	SCAN_VAR(exidy_sfxctrl);

	SCAN_VAR(sh8253_timer);

	d_timer.scan();
}

static void venture_sound_init()
{
	exidy_stream.init(nBurnSoundRate, nBurnSoundRate, 1, 0, exidy_stream_update);
	exidy_stream.set_buffered(M6502TotalCycles, 3579545/4);
	exidy_stream.set_volume(0.30);

	pia_reset();
	d_timer.init(555, riot_interrupt);

	venture_sound_reset();
	if (nBurnSoundRate != 0)
		sh6840_clocks_per_sample = (int)((double)SH6840_CLOCK / (double)nBurnSoundRate * (double)(1 << 24));

	if (nBurnSoundRate != 0)
		freq_to_step = (double)(1 << 24) / (double)nBurnSoundRate;
}

static void venture_sound_exit()
{
	exidy_stream.exit();
}

/*************************************
 *
 *
 *	6532 RIOT timer callback
 *
 *************************************/

static void riot_interrupt(int parm)
{
	/* if we're doing the initial interval counting... */
	if (riot_state == RIOT_COUNT)
	{
		/* generate the IRQ */
		riot_irq_flag |= 0x80;
		riot_irq_state = riot_timer_irq_enable;
		update_irq_state();

		/* now start counting clock cycles down */
		riot_state = RIOT_POST_COUNT;
		//timer_adjust(riot_timer, SH6532_PERIOD * 0xff, 0, 0);
//		bprintf(0, _T("timer start: 0xff\n"));
		d_timer.start(0xff, -1, 1, 0);
	}

	/* if not, we are done counting down */
	else
	{
		riot_state = RIOT_IDLE;
		//timer_adjust(riot_timer, TIME_NEVER, 0, 0);
 //   	bprintf(0, _T("timer stop\n"));
		d_timer.stop();
	}
}



/*************************************
 *
 *	6532 RIOT write handler
 *
 *************************************/

static void exidy_shriot_w(UINT16 offset, UINT8 data)
{
	exidy_stream.update();
	/* mask to the low 7 bits */
	offset &= 0x7f;

	/* I/O is done if A2 == 0 */
	if ((offset & 0x04) == 0)
	{
		switch (offset & 0x03)
		{
			case 0:	/* port A */
				if (has_hc55516) {
					ZetSetRESETLine(0, (~data & 0x10));
				}
				riot_porta_data = (riot_porta_data & ~riot_porta_ddr) | (data & riot_porta_ddr);
				break;

			case 1:	/* port A DDR */
				riot_porta_ddr = data;
				break;

			case 2:	/* port B */
#if 0
				if (has_tms5220)
				{
					if (!(data & 0x01) && (riot_portb_data & 0x01))
					{
						riot_porta_data = tms5220_status();
//						logerror("(%f)%04X:TMS5220 status read = %02X\n", timer_get_time(), activecpu_get_previouspc(), riot_porta_data);
					}
					if ((data & 0x02) && !(riot_portb_data & 0x02))
					{
//						logerror("(%f)%04X:TMS5220 data write = %02X\n", timer_get_time(), activecpu_get_previouspc(), riot_porta_data);
						tms5220_write(riot_porta_data);
					}
				}
#endif
				riot_portb_data = (riot_portb_data & ~riot_portb_ddr) | (data & riot_portb_ddr);
				break;

			case 3:	/* port B DDR */
				riot_portb_ddr = data;
				break;
		}
	}

	/* PA7 edge detect control if A2 == 1 and A4 == 0 */
	else if ((offset & 0x10) == 0)
	{
		riot_PA7_irq_enable = offset & 0x03;
	}

	/* timer enable if A2 == 1 and A4 == 1 */
	else
	{
		static const int divisors[4] = { 1, 8, 64, 1024 };

		/* make sure the IRQ state is clear */
		if (riot_state != RIOT_COUNT)
			riot_irq_flag &= ~0x80;
		riot_irq_state = 0;
		update_irq_state();

		/* set the enable from the offset */
		riot_timer_irq_enable = offset & 0x08;

		/* set a new timer */
		riot_interval = /* SH6532_PERIOD * */ divisors[offset & 0x03];
		//timer_adjust(riot_timer, riot_interval * data, 0, 0);
	   // bprintf(0, _T("timer start: %d div %d  tot: %x\n"), data, riot_interval, riot_interval*data);
		d_timer.start(riot_interval * data, -1, 1, 0);
		riot_state = RIOT_COUNT;
	}
}



/*************************************
 *
 *	6532 RIOT read handler
 *
 *************************************/

static UINT8 exidy_shriot_r(UINT16 offset)
{
	exidy_stream.update();
	/* mask to the low 7 bits */
	offset &= 0x7f;

	/* I/O is done if A2 == 0 */
	if ((offset & 0x04) == 0)
	{
		switch (offset & 0x03)
		{
			case 0x00:	/* port A */
				return riot_porta_data;

			case 0x01:	/* port A DDR */
				return riot_porta_ddr;

			case 0x02:	/* port B */
#if 0
				if (has_tms5220)
				{
					riot_portb_data &= ~0x0c;
					if (!tms5220_ready_r()) riot_portb_data |= 0x04;
					if (!tms5220_int_r()) riot_portb_data |= 0x08;
				}
#endif
				return riot_portb_data;

			case 0x03:	/* port B DDR */
				return riot_portb_ddr;
		}
	}

	/* interrupt flags are read if A2 == 1 and A0 == 1 */
	else if (offset & 0x01)
	{
		int temp = riot_irq_flag;
		riot_irq_flag = 0;
		riot_irq_state = 0;
		update_irq_state();
		return temp;
	}

	/* timer count is read if A2 == 1 and A0 == 0 */
	else
	{
		/* set the enable from the offset */
		riot_timer_irq_enable = offset & 0x08;
		/* compute the timer based on the current state */
		switch (riot_state)
		{
			case RIOT_IDLE:
				return 0x00;

			case RIOT_COUNT:
				return (int)(d_timer.timeleft() / riot_interval);

			case RIOT_POST_COUNT:
				return (int)(d_timer.timeleft());
		}
	}

	//logerror("Undeclared RIOT read: %x  PC:%x\n",offset,activecpu_get_pc());
	return 0xff;
}



/*************************************
 *
 *	8253 timer handlers
 *
 *************************************/

static void exidy_sh8253_w(UINT16 offset, UINT8 data)
{
	int chan;

	exidy_stream.update();

	offset &= 3;
	switch (offset)
	{
		case 0:
		case 1:
		case 2:
			chan = offset;
			if (!sh8253_timer[chan].clstate)
			{
				sh8253_timer[chan].clstate = 1;
				sh8253_timer[chan].count = (sh8253_timer[chan].count & 0xff00) | (data & 0x00ff);
			}
			else
			{
				sh8253_timer[chan].clstate = 0;
				sh8253_timer[chan].count = (sh8253_timer[chan].count & 0x00ff) | ((data << 8) & 0xff00);
				if (sh8253_timer[chan].count)
					sh8253_timer[chan].step = freq_to_step * (double)SH8253_CLOCK / (double)sh8253_timer[chan].count;
				else
					sh8253_timer[chan].step = 0;
			}
			break;

		case 3:
			chan = (data & 0xc0) >> 6;
			sh8253_timer[chan].enable = ((data & 0x0e) != 0);
			break;
	}
}


static UINT8 exidy_sh8253_r(UINT16 offset)
{
	//exidy_stream.update();
	//logerror("8253(R): %x\n",offset);
	return 0;
}



/*************************************
 *
 *	6840 timer handlers
 *
 *************************************/

static UINT8 exidy_sh6840_r(UINT16 offset)
{
	//exidy_stream.update();
	//logerror("6840R %x\n",offset);
    return 0;
}


static void exidy_sh6840_w(UINT16 offset, UINT8 data)
{
	/* force an update of the stream */
	//stream_update(exidy_stream, 0);
	exidy_stream.update();

	/* only look at the low 3 bits */
	offset &= 7;
	switch (offset)
	{
		/* offset 0 writes to either channel 0 control or channel 2 control */
		case 0:
			if (sh6840_timer[1].cr & 0x01)
				sh6840_timer[0].cr = data;
			else
				sh6840_timer[2].cr = data;
			break;

		/* offset 1 writes to channel 1 control */
		case 1:
			sh6840_timer[1].cr = data;
			break;

		/* offsets 2/4/6 write to the common MSB latch */
		case 2:
		case 4:
		case 6:
			sh6840_MSB = data;
			break;

		/* offsets 3/5/7 write to the LSB controls */
		case 3:
		case 5:
		case 7:
		{
			/* latch the timer value */
			int ch = (offset - 3) / 2;
			sh6840_timer[ch].timer = sh6840_timer[ch].counter.w = (sh6840_MSB << 8) | (data & 0xff);
			break;
		}
	}
}



/*************************************
 *
 *	External sound effect controls
 *
 *************************************/

static void exidy_sfxctrl_w(UINT16 offset, UINT8 data)
{
	exidy_stream.update();

	offset &= 3;
	switch (offset)
	{
		case 0:
			exidy_sfxctrl = data;
			break;

		case 1:
		case 2:
		case 3:
			sh6840_volume[offset - 1] = ((data & 7) * BASE_VOLUME) / 7;
			break;
	}
}



/*************************************
 *
 *	CVSD sound for Mouse Trap
 *
 *************************************/

static void mtrap_voiceio_w(UINT16 offset, UINT8 data)
{
    if (!(offset & 0x10))
	{
		// bprintf(0, _T("%x,  pc: %x  %x \n"), data, ZetGetPC(-1), ZetGetPrevPC(-1));

		if ((ZetGetPC(-1) == 0x0016 && ZetGetPrevPC(-1) == 0x0014) == false) {
			// when the pc/prev-pc is 0x0016/0x0014, the cpu is just looping
			// garbage* into the hc55516.  This causes a horrible high-pitched hissing noise
			// *(actually 0,1 to keep the integrator near to 0)
			hc55516_digit_w(data);
			hc55516_clock_w(1);  // mc3417 (hc55516 variant) has inverted clocking
			hc55516_clock_w(0);
		}
	}
    if (!(offset & 0x20))
		riot_portb_data = data & 1;
}


static UINT8 mtrap_voiceio_r(UINT16 offset)
{
	if (!(offset & 0x80))
	{
       int data = (riot_porta_data & 0x06) >> 1;
       data |= (riot_porta_data & 0x01) << 2;
       data |= (riot_porta_data & 0x08);
       return data;
	}
    if (!(offset & 0x40))
	{
		return (ZetTotalCycles() & 4) << 5; // purrfect timing
	}

	return 0xff;
}

static UINT8 venture_sound_read(UINT16 address)
{
	if (address >= 0x0000 && address <= 0x07ff) {
		return DrvSndRAM[address & 0x7f]; // mirrored
	}

	if (address >= 0x0800 && address <= 0x0fff) {
		return exidy_shriot_r(address & 0x1f);
	}

	if (address >= 0x1000 && address <= 0x17ff) {
		return pia_read(1, address & 0x03);
	}

	if (address >= 0x1800 && address <= 0x1fff) {
		return exidy_sh8253_r(address & 0x03);
	}

	if (address >= 0x2800 && address <= 0x2fff) {
		return exidy_sh6840_r(address & 0x07);
	}

	return 0;
}

static void venture_sound_write(UINT16 address, UINT8 data)
{
	if (address >= 0x0000 && address <= 0x07ff) {
		DrvSndRAM[address & 0x7f] = data; // mirrored
		return;
	}

	if (address >= 0x0800 && address <= 0x0fff) {
		exidy_shriot_w(address & 0x1f, data);
		return;
	}

	if (address >= 0x1000 && address <= 0x17ff) {
		pia_write(1, address & 0x03, data);
		return;
	}

	if (address >= 0x1800 && address <= 0x1fff) {
		exidy_sh8253_w(address & 0x03, data);
		return;
	}

	if (address >= 0x2000 && address <= 0x27ff) {
		// filter write (n/c)
		return;
	}

	if (address >= 0x2800 && address <= 0x2fff) {
		exidy_sh6840_w(address & 0x07, data);
		return;
	}

	if (address >= 0x3000 && address <= 0x37ff) {
		exidy_sfxctrl_w(address & 0x03, data);
		return;
	}
}

static UINT8 __fastcall mtrap_read(UINT16 address)
{
	return DrvCVSDROM[address & 0x3fff];
}

static void __fastcall mtrap_write_port(UINT16 port, UINT8 data)
{
	mtrap_voiceio_w(port & 0xff, data);
}

static UINT8 __fastcall mtrap_read_port(UINT16 port)
{
	return mtrap_voiceio_r(port & 0xff);
}

static void VentureAudioInit()
{
	// venture audio (venture, mtrap, etc)
	is_venture = 1;

	pia_init();
	pia_config(0, 0, &main_pia);
	pia_config(1, 0, &snd_pia);

	M6502Init(1, TYPE_M6502);
	M6502Open(1);
	M6502SetAddressMask(0x7fff);
	M6502MapMemory(DrvSndROM + 0x5800,		0x5800, 0x7fff, MAP_ROM);
	M6502SetWriteHandler(venture_sound_write);
	M6502SetReadHandler(venture_sound_read);
	M6502Close();
	venture_sound_init();
}

static INT32 VentureInit()
{
	BurnAllocMemIndex();

	{
		if (DrvLoadROMs(0x8000, 0x5800)) return 1;

		DrvGfxDecode();
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvMainRAM,				0x0000, 0x03ff, MAP_RAM);
	M6502MapMemory(DrvVideoRAM,				0x4000, 0x43ff, MAP_RAM);
	M6502MapMemory(DrvVideoRAM,				0x4400, 0x47ff, MAP_RAM); // mirror
	M6502MapMemory(DrvCharacterRAM,			0x4800, 0x4fff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM + 0x8000,	0x8000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(venture_main_write);
	M6502SetReadHandler(venture_main_read);
	M6502Close();

	VentureAudioInit();

	ExidyCommonInit(targ_intsource, 0x04, 0x04, 0, 0x4000, 0, 0, 0);

	DrvDoReset();

	return 0;
}

static INT32 MtrapCommonInit(INT32 load_addr)
{
	BurnAllocMemIndex();

	{
		if (DrvLoadROMs(0xa000, load_addr)) return 1;

		DrvGfxDecode();
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvMainRAM,				0x0000, 0x03ff, MAP_RAM);
	M6502MapMemory(DrvVideoRAM,				0x4000, 0x43ff, MAP_RAM);
	M6502MapMemory(DrvVideoRAM,				0x4400, 0x47ff, MAP_RAM); // mirror
	M6502MapMemory(DrvCharacterRAM,			0x4800, 0x4fff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM + 0x8000,	0x8000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(venture_main_write);
	M6502SetReadHandler(venture_main_read);
	M6502Close();

	VentureAudioInit();

	// mtrap audio (hc55516 ontop of venture audio)
	is_mtrap = 1;
	has_hc55516 = 1;
	ZetInit(0);
	ZetOpen(0);
	ZetSetReadHandler(mtrap_read);
	ZetSetOutHandler(mtrap_write_port);
	ZetSetInHandler(mtrap_read_port);
	ZetClose();

	mc3417_init(ZetTotalCycles, 3579545/2);
	hc55516_volume(1.00);

	ExidyCommonInit(targ_intsource, 0x14, 0x00, 0, 0x4000, 0, 0, 0);

	DrvDoReset();

	return 0;
}

static INT32 TeetertInit()
{
	BurnAllocMemIndex();

	{
		if (DrvLoadROMs(0x8000, 0x5800)) return 1;

		DrvGfxDecode();
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvMainRAM,				0x0000, 0x03ff, MAP_RAM);
	M6502MapMemory(DrvVideoRAM,				0x4000, 0x43ff, MAP_RAM);
	M6502MapMemory(DrvVideoRAM,				0x4400, 0x47ff, MAP_RAM); // mirror
	M6502MapMemory(DrvCharacterRAM,			0x4800, 0x4fff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM + 0x8000,	0x8000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(venture_main_write);
	M6502SetReadHandler(teetert_main_read);
	M6502Close();

	VentureAudioInit();

	ExidyCommonInit(targ_intsource, 0x0c, 0x0c, 0, 0x4000, 0, 0, 0);
	periodic_nmi = 1;

	BurnTrackballInit(1);

	DrvDoReset();

	return 0;
}

static INT32 Pepper2Init()
{
	BurnAllocMemIndex();

	{
		if (DrvLoadROMs(0x9000, 0x6800)) return 1;

		DrvGfxDecode();
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvMainRAM,				0x0000, 0x03ff, MAP_RAM);
	M6502MapMemory(DrvVideoRAM,				0x4000, 0x43ff, MAP_RAM);
	M6502MapMemory(DrvVideoRAM,				0x4400, 0x47ff, MAP_RAM); // mirror
	M6502MapMemory(DrvCharacterRAM,			0x6000, 0x6fff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM + 0x8000,	0x8000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(venture_main_write);
	M6502SetReadHandler(venture_main_read);
	M6502Close();

	VentureAudioInit();

	ExidyCommonInit(targ_intsource, 0x14, 0x04, 1, 0x4000, 0, 0, 0);

	DrvDoReset();

	return 0;
}

static INT32 HardhatInit()
{
	BurnAllocMemIndex();

	{
		if (DrvLoadROMs(0xa000, 0x6800)) return 1;

		DrvGfxDecode();
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvMainRAM,				0x0000, 0x03ff, MAP_RAM);
	M6502MapMemory(DrvVideoRAM,				0x4000, 0x43ff, MAP_RAM);
	M6502MapMemory(DrvVideoRAM,				0x4400, 0x47ff, MAP_RAM); // mirror
	M6502MapMemory(DrvCharacterRAM,			0x6000, 0x6fff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM + 0x8000,	0x8000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(venture_main_write);
	M6502SetReadHandler(venture_main_read);
	M6502Close();

	VentureAudioInit();

	ExidyCommonInit(targ_intsource, 0x14, 0x04, 1, 0x4000, 0, 0, 0);

	DrvDoReset();

	return 0;
}

static INT32 FaxInit()
{
	BurnAllocMemIndex();

	{
		if (DrvLoadROMs(0x8000, 0x6800)) return 1;

		DrvGfxDecode();
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvMainRAM,				0x0000, 0x07ff, MAP_RAM);
	M6502MapMemory(DrvVideoRAM,				0x4000, 0x43ff, MAP_RAM);
	M6502MapMemory(DrvVideoRAM,				0x4400, 0x47ff, MAP_RAM); // mirror
	M6502MapMemory(DrvCharacterRAM,			0x6000, 0x6fff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM + 0x8000,	0x8000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(fax_main_write);
	M6502SetReadHandler(fax_main_read);
	M6502Close();

	VentureAudioInit();

	ExidyCommonInit(targ_intsource, 0x04, 0x04, 1, 0x4000, 0, 0, 0);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	M6502Exit();
	pia_exit();

	if (is_mtrap) {
		ZetExit();
		hc55516_exit();
	}

	if (is_venture) {
		venture_sound_exit();
	}

	if (has_samples) {
		cheese_soundsystem_exit();
	}

	if (periodic_nmi) {
		BurnTrackballExit();
	}

	timerExit();

	BurnFreeMemIndex();

	int_source = NULL;
	collision_invert = 0;
	collision_mask = 0;
	is_2bpp = 0;
	periodic_nmi = 0;

	is_targ = 0;
	is_mtrap = 0;
	is_venture = 0;
	has_hc55516 = 0;

	return 0;
}

static void DrvPaletteUpdate()
{
	INT32 order[8] = { 0, 7, 0, 6, 4, 3, 2, 1 };

	for (INT32 i = 0; i < 8; i++)
	{
		UINT8 r = ((color_latch[2] >> order[i]) & 1) * 0xff;
		UINT8 g = ((color_latch[1] >> order[i]) & 1) * 0xff;
		UINT8 b = ((color_latch[0] >> order[i]) & 1) * 0xff;

		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}
}

static void draw_background()
{
	const UINT8 *cram = DrvCharacterRAM; // could point to rom, also (sidetrac)

	INT32 off_pen = 0;

	for (INT32 offs = 0; offs < 0x400; offs++)
	{
		UINT8 y = offs >> 5 << 3;
		UINT8 code = DrvVideoRAM[offs];

		INT32 on_pen_1, on_pen_2;
		if (is_2bpp)
		{
			on_pen_1 = 4 + ((code >> 6) & 0x02);
			on_pen_2 = 5 + ((code >> 6) & 0x02);
		}
		else
		{
			on_pen_1 = 4 + ((code >> 6) & 0x03);
			on_pen_2 = off_pen;  /* unused */
		}

		for (UINT8 cy = 0; cy < 8; cy++)
		{
			UINT8 x = offs << 3;

			UINT8 data1 = cram[0x000 | (code << 3) | cy];
			UINT8 data2 = cram[0x800 | (code << 3) | cy];

			for (int i = 0; i < 8; i++)
			{
				if (data1 & 0x80)
					pTransDraw[(y * nScreenWidth) + x] = (data2 & 0x80) ? on_pen_2 : on_pen_1;
				else
					pTransDraw[(y * nScreenWidth) + x] = off_pen;

				x++;
				data1 <<= 1;
				data2 <<= 1;
			}

			y++;
		}
	}
}

static inline INT32 sprite_1_enabled()
{
	return (!(sprite_enable & 0x80) || (sprite_enable & 0x10) || (collision_mask == 0x00));
}

static void draw_sprites()
{
	int sprite_set_2 = ((sprite_enable & 0x40) != 0);

	int sx = 236 - sprite2_xpos - 4;
	int sy = 244 - sprite2_ypos - 4;

#if 0
	extern int counter;
	UINT16 *p = pTransDraw + (sy+counter)*256 + sx;
	for (int i=0;i<7;i++)
		p[i] = i;
				  /// DEBUGGGGGGGGGG CODE!!!!!!!!!
	bprintf(0, _T("draw: sprite2_xpos %d(0x%x)  sx %d.  sprite2_ypos %d(0x%x) sy %d.\n"), sprite2_xpos, sprite2_xpos, sx, sprite2_ypos, sprite2_ypos, sy);
#endif
	if (nSpriteEnable & 0x01)
		DrawGfxMaskTile(0, 0, (spriteno >> 4) + 0x20 + (sprite_set_2 * 0x10), sx, sy, 0, 0, 1, 0);

	if (sprite_1_enabled())
	{
		int sprite_set_1 = ((sprite_enable & 0x20) != 0);

		sx = 236 - sprite1_xpos - 4;
		sy = 244 - sprite1_ypos - 4;

		if (sy < 0) sy = 0;

		if (nSpriteEnable & 0x02)
			DrawGfxMaskTile(0, 0, (spriteno & 0xf) + (sprite_set_1 * 0x10), sx, sy, 0, 0, 0, 0);
	}
}

#define ONE_LINE ((double)705562 / 60 / 280)
#define SCANLINE ((double)M6502TotalCycles() / ONE_LINE)
static INT32 cycles_to(INT32 x, INT32 y)
{
	// if y is less than current SCANLINE, it's on the next frame
	int lines_to_end_of_frame = (y < SCANLINE) ? (280-SCANLINE) : 0;
	int lines_to_nexty = (y < SCANLINE) ? y : (y-SCANLINE);
	int cycles_to_nextx = (double)x*ONE_LINE/256;
	int cycles_to_nexty = (double)(lines_to_end_of_frame + lines_to_nexty) * ONE_LINE;
	//bprintf(0, _T("cycles_to(%d, %d):  nextx: %d  nexty: %d  all: %d  SL: %f\n"), x, y, cycles_to_nextx, cycles_to_nexty, cycles_to_nexty + cycles_to_nextx, SCANLINE);
	return cycles_to_nexty + cycles_to_nextx;
}

static void latch_collision_timer_cb(int param)
{
//	bprintf(0, _T("timer hits at %d,%d  SL: %d\n"),(int)((M6502TotalCycles()%42)*256/ONE_LINE),(int)(SCANLINE),scanline);
	latch_condition(param);
	M6502SetIRQLine(0, 0, CPU_IRQSTATUS_ACK);
}

static void check_collision()
{
	UINT8 sprite_set_1 = ((sprite_enable & 0x20) != 0);
	UINT8 sprite_set_2 = ((sprite_enable & 0x40) != 0);

	int org_1_x = 0, org_1_y = 0;
	int org_2_x = 0, org_2_y = 0;
	int count = 0;

	if (collision_mask == 0)
		return;

	BurnBitmapFill(1, 0xff);
	if (sprite_1_enabled())
	{
		org_1_x = 236 - sprite1_xpos - 4;
		org_1_y = 244 - sprite1_ypos - 4;
		DrawGfxMaskTile(1, 0, (spriteno & 0xf) + (sprite_set_1 * 0x10), 0, 0, 0, 0, 1, 0);
	}

	BurnBitmapFill(2, 0xff);
	org_2_x = 236 - sprite2_xpos - 4;
	org_2_y = 244 - sprite2_ypos - 4;
	DrawGfxMaskTile(2, 0, (spriteno >> 4) + 0x20 + (sprite_set_2 * 0x10), 0, 0, 0, 0, 1, 0);

	BurnBitmapFill(3, 0xff);
	if (sprite_1_enabled())
	{
		int sx = org_2_x - org_1_x;
		int sy = org_2_y - org_1_y;
		DrawGfxMaskTile(3, 0, (spriteno >> 4) + 0x20 + (sprite_set_2 * 0x10), sx, sy, 0, 0, 1, 0);
	}

	for (int sy = 0; sy < 16; sy++)
	{
		for (int sx = 0; sx < 16; sx++)
		{
			if (*BurnBitmapGetPosition(1, sx, sy) != 0xff)
			{
				UINT8 current_collision_mask = 0;

				if (*BurnBitmapGetPosition(0, org_1_x + sx, org_1_y + sy) != 0)
					current_collision_mask |= 0x04;

				if (*BurnBitmapGetPosition(3, sx, sy) != 0xff)
					current_collision_mask |= 0x10;

				if ((current_collision_mask & collision_mask) && count < 128)
				{
					//m_collision_timer[count]->adjust(m_screen->time_until_pos(org_1_x + sx, org_1_y + sy), current_collision_mask); // iq_132
					//bprintf(0, _T("collision1 : %d,%d   mask/mask %x  %x\n"),org_1_x + sx, org_1_y + sy,current_collision_mask, collision_mask);
					collision_timer.start(cycles_to(org_1_x + sx, org_1_y + sy), current_collision_mask, 1, 0);
					count++;
				}
			}

			if (*BurnBitmapGetPosition(2, sx, sy) != 0xff)
			{
				if (*BurnBitmapGetPosition(0, org_2_x + sx, org_2_y + sy) != 0)
				{
					if ((collision_mask & 0x08) && count < 128)
					{
						//m_collision_timer[count]->adjust(m_screen->time_until_pos(org_2_x + sx, org_2_y + sy), 0x08); // iq_132
						//bprintf(0, _T("collision2 : %d,%d\n"),org_2_x + sx, org_2_y + sy);
						collision_timer.start(cycles_to(org_2_x + sx, org_2_y + sy), 0x08, 1, 0);
						count++;
					}
				}
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc || 1) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	}

	BurnTransferClear();

	draw_background();

	check_collision();

	draw_sprites();

	if (pBurnDraw) BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	if (is_mtrap) ZetNewFrame();
	M6502NewFrame();
	timerNewFrame();

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		DrvInputs[2] = DrvDips[2];

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
			DrvInputs[4] ^= (DrvJoy5[i] & 1) << i;
		}

		if (periodic_nmi) { // teetert
			BurnTrackballConfig(0, AXIS_REVERSED, AXIS_NORMAL);
//			BurnTrackballConfigStartStopPoints(0, 64, 167, 64, 167);
			BurnTrackballFrame(0, Analog[0], 0, 0, 0x3f);
			BurnTrackballUpdate(0);
		}
	}

	INT32 nInterleave = 280; // scanlines
	//                        main, audio, cvsd
	INT32 nCyclesTotal[3] = { 705562 / 60, 3579545/4/60, 3579545/2/60 };
	INT32 nCyclesDone[3] = { nExtraCycles[0], nExtraCycles[1], nExtraCycles[2] };
	timerIdle(nExtraCycles[0]);

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		scanline = i;

		M6502Open(0);
		CPU_RUN(0, M6502);

		if (periodic_nmi) {
			if (((i + 4) % 28) == 8) M6502SetIRQLine(0x20, CPU_IRQSTATUS_AUTO); // pulsed nmi
		}

		if (i == nInterleave-1) {
			latch_condition(0);
			int_condition &= ~0x80;
			M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
			vblank = 1;
		}
		M6502Close();

		if (is_venture) {
			M6502Open(1);
			CPU_RUN(1, M6502);
			exidy_stream.update(); // has timer w/irq generation in the stream update.
			M6502Close();
		}

		if (is_mtrap) {
			ZetOpen(0);
			CPU_RUN(2, Zet);
			ZetClose();
		}
	}

	nExtraCycles[0] = nCyclesDone[0] - nCyclesTotal[0];
	nExtraCycles[1] = nCyclesDone[1] - nCyclesTotal[1];
	nExtraCycles[2] = nCyclesDone[2] - nCyclesTotal[2];

	if (pBurnSoundOut) {
		BurnSoundClear();
		if (is_venture) {
			exidy_stream.render(pBurnSoundOut, nBurnSoundLen);
//			BurnSoundDCFilter();
		}
		if (is_mtrap) {
			hc55516_update(pBurnSoundOut, nBurnSoundLen);
		}
		if (has_samples) {
			cheese_soundsystem_render(pBurnSoundOut, nBurnSoundLen);
		}
	}

	DrvDraw(); // called every frame (collision check in draw)

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	if (pnMin) {
		*pnMin = 0x029702;
	}

	if (nAction & ACB_VOLATILE) {
		ScanVar(AllRam, RamEnd - AllRam, "All Ram");

		M6502Scan(nAction);
		pia_scan(nAction, pnMin);

		if (is_venture) {
			venture_sound_scan();
		}

		if (is_mtrap) {
			ZetScan(nAction);
			hc55516_scan(nAction, pnMin);
		}

		if (has_samples) {
			cheese_soundsystem_scan(nAction, pnMin);
		}

		timerScan();

		if (periodic_nmi) {
			BurnTrackballScan();
			SCAN_VAR(last_dial);
		}

		SCAN_VAR(sprite1_xpos);
		SCAN_VAR(sprite1_ypos);
		SCAN_VAR(sprite2_xpos);
		SCAN_VAR(sprite2_ypos);
		SCAN_VAR(spriteno);
		SCAN_VAR(sprite_enable);
		SCAN_VAR(color_latch);
		SCAN_VAR(int_condition);
		SCAN_VAR(bankdata);

		SCAN_VAR(nExtraCycles);
	}

	if (nAction & ACB_WRITE)
	{
		if (bankdata & 0x80)
		{
			M6502Open(0);
			M6502MapMemory(DrvM6502ROM + 0x10000 + (bankdata & 0x1f) * 0x2000, 0x2000, 0x3fff, MAP_ROM);
			M6502Close();
		}
	}

	return 0;
}

// Samples for targ, spectar
static struct BurnSampleInfo TargSampleDesc[] = {
	{ "expl", 			SAMPLE_NOLOOP },
	{ "shot", 			SAMPLE_NOLOOP },
	{ "sexpl", 			SAMPLE_NOLOOP },
	{ "spslow", 		SAMPLE_NOLOOP },
	{ "spfast", 		SAMPLE_NOLOOP },
	{ "spectar_shot",	SAMPLE_NOLOOP },
	{ "", 0 }
};

STD_SAMPLE_PICK(Targ)
STD_SAMPLE_FN(Targ)

static struct BurnSampleInfo SidetracSampleDesc[] = {
	{ "expl", 			SAMPLE_NOLOOP },
	{ "choo", 			SAMPLE_NOLOOP },
	{ "", 0 }
};

STD_SAMPLE_PICK(Sidetrac)
STD_SAMPLE_FN(Sidetrac)

// Side Trak

static struct BurnRomInfo sidetracRomDesc[] = {
	{ "stl8a-1",		0x0800, 0xe41750ff, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "stl7a-2",		0x0800, 0x57fb28dc, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "stl6a-2",		0x0800, 0x4226d469, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "stl9c-1",		0x0400, 0x08710a84, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "stl11d",			0x0200, 0x3bd1acc1, 2 | BRF_GRA },           //  4 Sprites

	{ "sta2b-1",		0x0020, 0xc0f5df16, 3 | BRF_SND },           //  5 Sound PROM
};

STD_ROM_PICK(sidetrac)
STD_ROM_FN(sidetrac)

struct BurnDriver BurnDrvSidetrac = {
	"sidetrac", NULL, NULL, "sidetrac", "1979",
	"Side Trak\0", NULL, "Exidy", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_MAZE, 0,
	NULL, sidetracRomInfo, sidetracRomName, NULL, NULL, SidetracSampleInfo, SidetracSampleName, SidetracInputInfo, SidetracDIPInfo,
	SidetracInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 8,
	256, 256, 4, 3
};


// Targ

static struct BurnRomInfo targRomDesc[] = {
	{ "hrl10a-1",		0x0800, 0x969744e1, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "hrl9a-1",		0x0800, 0xa177a72d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "hrl8a-1",		0x0800, 0x6e6928a5, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "hrl7a-1",		0x0800, 0xe2f37f93, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "hrl6a-1",		0x0800, 0xa60a1bfc, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "hrl11d-1",		0x0400, 0x9f03513e, 2 | BRF_GRA },           //  5 Sprites

	{ "hra2b-1",		0x0020, 0x38e8024b, 3 | BRF_SND },           //  6 Sound PROM

	{ "hrl5c-1",		0x0100, 0xa24290d0, 0 | BRF_OPT },           //  7 PROMs
	{ "stl6d-1",		0x0020, 0xe26f9053, 0 | BRF_OPT },           //  8
	{ "hrl14h-1",		0x0020, 0xf76b4fcf, 0 | BRF_OPT },           //  9
};

STD_ROM_PICK(targ)
STD_ROM_FN(targ)

struct BurnDriver BurnDrvTarg = {
	"targ", NULL, NULL, "targ", "1980",
	"Targ\0", NULL, "Exidy", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_MULTISHOOT, 0,
	NULL, targRomInfo, targRomName, NULL, NULL, TargSampleInfo, TargSampleName, TargInputInfo, TargDIPInfo,
	TargInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 8,
	256, 256, 4, 3
};


// Targ (cocktail?)

static struct BurnRomInfo targcRomDesc[] = {
	{ "ctl.10a",		0x0800, 0x058b3983, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "ctl.9a1",		0x0800, 0x3ac44b6b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ctl.8a1",		0x0800, 0x5c470021, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ctl.7a1",		0x0800, 0xc774fd9b, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ctl.6a1",		0x0800, 0x3d020439, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "hrl11d-1",		0x0400, 0x9f03513e, 2 | BRF_GRA },           //  5 Sprites

	{ "hra2b-1",		0x0020, 0x38e8024b, 3 | BRF_GRA },           //  6 Sound PROM
};

STD_ROM_PICK(targc)
STD_ROM_FN(targc)

struct BurnDriver BurnDrvTargc = {
	"targc", "targ", NULL, "targ", "1980",
	"Targ (cocktail?)\0", NULL, "Exidy", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_MULTISHOOT, 0,
	NULL, targcRomInfo, targcRomName, NULL, NULL, TargSampleInfo, TargSampleName, TargInputInfo, TargDIPInfo,
	TargInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 8,
	256, 256, 4, 3
};


// Spectar (rev. 3)

static struct BurnRomInfo spectarRomDesc[] = {
	{ "spl11a-3",		0x0800, 0x08880aff, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "spl10a-2",		0x0800, 0xfca667c1, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "spl9a-3",		0x0800, 0x9d4ce8ba, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "spl8a-2",		0x0800, 0xcfacbadf, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "spl7a-2",		0x0800, 0x4c4741ff, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "spl6a-2",		0x0800, 0x0cb46b25, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "hrl11d-2",		0x0800, 0xc55b645d, 2 | BRF_GRA },           //  6 Sprites

	{ "prom.5c",		0x0100, 0x9ca2e061, 0 | BRF_OPT },           //  7 PROMs
	{ "prom.6d",		0x0020, 0xe26f9053, 0 | BRF_OPT },           //  8
	{ "hrl14h-1",		0x0020, 0xf76b4fcf, 0 | BRF_OPT },           //  9
};

STD_ROM_PICK(spectar)
STD_ROM_FN(spectar)

struct BurnDriver BurnDrvSpectar = {
	"spectar", NULL, NULL, "targ", "1980",
	"Spectar (rev. 3)\0", NULL, "Exidy", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_MULTISHOOT, 0,
	NULL, spectarRomInfo, spectarRomName, NULL, NULL, TargSampleInfo, TargSampleName, TargInputInfo, SpectarDIPInfo,
	SpectarInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 8,
	256, 256, 4, 3
};


// Spectar (rev. 1?)

static struct BurnRomInfo spectar1RomDesc[] = {
	{ "spl12a1",		0x0800, 0x7002efb4, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "spl11a1",		0x0800, 0x8eb8526a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "spl10a1",		0x0800, 0x9d169b3d, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "spl9a1",			0x0800, 0x40e3eba1, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "spl8a1",			0x0800, 0x64d8eb84, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "spl7a1",			0x0800, 0xe08b0d8d, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "spl6a1",			0x0800, 0xf0e4e71a, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "hrl11d-2",		0x0800, 0xc55b645d, 2 | BRF_GRA },           //  7 Sprites

	{ "prom.5c",		0x0100, 0x9ca2e061, 0 | BRF_OPT },           //  8 PROMs
	{ "prom.6d",		0x0020, 0xe26f9053, 0 | BRF_OPT },           //  9
	{ "hrl14h-1",		0x0020, 0xf76b4fcf, 0 | BRF_OPT },           // 10
};

STD_ROM_PICK(spectar1)
STD_ROM_FN(spectar1)

struct BurnDriver BurnDrvSpectar1 = {
	"spectar1", "spectar", NULL, "targ", "1980",
	"Spectar (rev. 1?)\0", NULL, "Exidy", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_MULTISHOOT, 0,
	NULL, spectar1RomInfo, spectar1RomName, NULL, NULL, TargSampleInfo, TargSampleName, TargInputInfo, SpectarDIPInfo,
	Spectar1Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 8,
	256, 256, 4, 3
};


// Spectar (rev. 2, bootleg)

static struct BurnRomInfo spectarrfRomDesc[] = {
	{ "spl11a-2",		0x0800, 0x0a0ea985, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "spl10a-2",		0x0800, 0xfca667c1, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "spl9a-3",		0x0800, 0x9d4ce8ba, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "spl8a-2",		0x0800, 0xcfacbadf, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "spl7a-2",		0x0800, 0x4c4741ff, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "sp26a-2",		0x0800, 0x559ab427, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "hrl11d-2",		0x0800, 0xc55b645d, 2 | BRF_GRA },           //  6 Sprites

	{ "prom.5c",		0x0100, 0x9ca2e061, 0 | BRF_OPT },           //  7 PROMs
	{ "prom.6d",		0x0020, 0xe26f9053, 0 | BRF_OPT },           //  8
	{ "hrl14h-1",		0x0020, 0xf76b4fcf, 0 | BRF_OPT },           //  9
};

STD_ROM_PICK(spectarrf)
STD_ROM_FN(spectarrf)

struct BurnDriver BurnDrvSpectarrf = {
	"spectarrf", "spectar", NULL, "targ", "1984",
	"Spectar (rev. 2, bootleg)\0", NULL, "bootleg (Recreativos Franco)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_MULTISHOOT, 0,
	NULL, spectarrfRomInfo, spectarrfRomName, NULL, NULL, TargSampleInfo, TargSampleName, TargInputInfo, SpectarrfDIPInfo,
	SpectarInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 8,
	256, 256, 4, 3
};


// Venture (ver. 5 Set 1)

static struct BurnRomInfo ventureRomDesc[] = {
	{ "13a-cpu",		0x1000, 0xf4e4d991, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "12a-cpu",		0x1000, 0xc6d8cb04, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "11a-cpu",		0x1000, 0x3bdb01f4, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "10a-cpu",		0x1000, 0x0da769e9, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "9a-cpu",			0x1000, 0x0ae05855, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "8a-cpu",			0x1000, 0x4ae59676, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "7a-cpu",			0x1000, 0x48d66220, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "6a-cpu",			0x1000, 0x7b78cf49, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "vel_11d-2.11d",	0x0800, 0xea6fd981, 2 | BRF_GRA },           //  8 Sprites

	{ "vea_3a-3.3a",	0x0800, 0x4ea1c3d9, 3 | BRF_PRG | BRF_ESS }, //  9 Sound CPU Code
	{ "vea_4a-3.4a",	0x0800, 0x5154c39e, 3 | BRF_PRG | BRF_ESS }, // 10
	{ "vea_5a-3.5a",	0x0800, 0x1e1e3916, 3 | BRF_PRG | BRF_ESS }, // 11
	{ "vea_6a-3.6a",	0x0800, 0x80f3357a, 3 | BRF_PRG | BRF_ESS }, // 12
	{ "vea_7a-3.7a",	0x0800, 0x466addc7, 3 | BRF_PRG | BRF_ESS }, // 13

	{ "hrl14h-1.h14",	0x0020, 0xf76b4fcf, 0 | BRF_OPT },           // 14 PROMs
	{ "vel5c-1.c5",		0x0100, 0x43b35bb7, 0 | BRF_OPT },           // 15
	{ "hrl6d-1.d6",		0x0020, 0xe26f9053, 0 | BRF_OPT },           // 16
};

STD_ROM_PICK(venture)
STD_ROM_FN(venture)

struct BurnDriver BurnDrvVenture = {
	"venture", NULL, NULL, NULL, "1981",
	"Venture (ver. 5 Set 1)\0", NULL, "Exidy", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_RUNGUN, 0,
	NULL, ventureRomInfo, ventureRomName, NULL, NULL, NULL, NULL, TargInputInfo, VentureDIPInfo,
	VentureInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 8,
	256, 256, 4, 3
};


// Venture (ver. 5 Set 2)

static struct BurnRomInfo venture5aRomDesc[] = {
	{ "vel_13a-57e.a13",0x1000, 0x4c833f99, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "vel_12a-5.a12",	0x1000, 0x8163cefc, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "vel_11a-5.a11",	0x1000, 0x324a5054, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "vel_10a-5.a10",	0x1000, 0x24358203, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "vel_9a-5.a9",	0x1000, 0x04428165, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "vel_8a-5.a8",	0x1000, 0x4c1a702a, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "vel_7a-5.a7",	0x1000, 0x1aab27c2, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "vel_6a-5.a6",	0x1000, 0x767bdd71, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "vel_11d-2.11d",	0x0800, 0xea6fd981, 2 | BRF_GRA },           //  8 Sprites

	{ "vea_3a-3.3a",	0x0800, 0x4ea1c3d9, 3 | BRF_PRG | BRF_ESS }, //  9 Sound CPU Code
	{ "vea_4a-3.4a",	0x0800, 0x5154c39e, 3 | BRF_PRG | BRF_ESS }, // 10
	{ "vea_5a-3.5a",	0x0800, 0x1e1e3916, 3 | BRF_PRG | BRF_ESS }, // 11
	{ "vea_6a-3.6a",	0x0800, 0x80f3357a, 3 | BRF_PRG | BRF_ESS }, // 12
	{ "vea_7a-3.7a",	0x0800, 0x466addc7, 3 | BRF_PRG | BRF_ESS }, // 13

	{ "hrl14h-1.h14",	0x0020, 0xf76b4fcf, 0 | BRF_OPT },           // 14 PROMs
	{ "vel5c-1.c5",		0x0100, 0x43b35bb7, 0 | BRF_OPT },           // 15
	{ "hrl6d-1.d6",		0x0020, 0xe26f9053, 0 | BRF_OPT },           // 16
};

STD_ROM_PICK(venture5a)
STD_ROM_FN(venture5a)

struct BurnDriver BurnDrvVenture5a = {
	"venture5a", "venture", NULL, NULL, "1981",
	"Venture (ver. 5 Set 2)\0", NULL, "Exidy", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_RUNGUN, 0,
	NULL, venture5aRomInfo, venture5aRomName, NULL, NULL, NULL, NULL, TargInputInfo, VentureDIPInfo,
	VentureInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 8,
	256, 256, 4, 3
};


// Venture (ver. 4)

static struct BurnRomInfo venture4RomDesc[] = {
	{ "vel_13a-4.13a",	0x1000, 0x1c5448f9, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "vel_12a-4.12a",	0x1000, 0xe62491cc, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "vel_11a-4.11a",	0x1000, 0xe91faeaf, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "vel_10a-4.10a",	0x1000, 0xda3a2991, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "vel_9a-4.a9",	0x1000, 0xd1887b11, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "vel_8a-4.a8",	0x1000, 0x8e8153fc, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "vel_7a-4.a7",	0x1000, 0x0a091701, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "vel_6a-4.a6",	0x1000, 0x7b165f67, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "vel_11d-2.11d",	0x0800, 0xea6fd981, 2 | BRF_GRA },           //  8 Sprites

	{ "vea_3a-2.3a",	0x0800, 0x83b8836f, 3 | BRF_PRG | BRF_ESS }, //  9 Sound CPU Code
	{ "vea_3a-2.4a",	0x0800, 0x5154c39e, 3 | BRF_PRG | BRF_ESS }, // 10
	{ "vea_3a-2.5a",	0x0800, 0x1e1e3916, 3 | BRF_PRG | BRF_ESS }, // 11
	{ "vea_3a-2.6a",	0x0800, 0x80f3357a, 3 | BRF_PRG | BRF_ESS }, // 12
	{ "vea_3a-2.7a",	0x0800, 0x466addc7, 3 | BRF_PRG | BRF_ESS }, // 13

	{ "hrl14h-1.h14",	0x0020, 0xf76b4fcf, 0 | BRF_OPT },           // 14 PROMs
	{ "vel5c-1.c5",		0x0100, 0x43b35bb7, 0 | BRF_OPT },           // 15
	{ "hrl6d-1.d6",		0x0020, 0xe26f9053, 0 | BRF_OPT },           // 16
};

STD_ROM_PICK(venture4)
STD_ROM_FN(venture4)

struct BurnDriver BurnDrvVenture4 = {
	"venture4", "venture", NULL, NULL, "1981",
	"Venture (ver. 4)\0", NULL, "Exidy", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_RUNGUN, 0,
	NULL, venture4RomInfo, venture4RomName, NULL, NULL, NULL, NULL, TargInputInfo, VentureDIPInfo,
	VentureInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 8,
	256, 256, 4, 3
};


// Venture (ver. 5 Set 2, bootleg)

static struct BurnRomInfo venture5bRomDesc[] = {
	{ "d2732.2s",		0x1000, 0x87d69fe9, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "d2732.2r",		0x1000, 0x8163cefc, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "d2732.2n",		0x1000, 0x324a5054, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "d2732.2m",		0x1000, 0x24358203, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "d2732.2l",		0x1000, 0x04428165, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "d2732.2k",		0x1000, 0x4c1a702a, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "d2732.2h",		0x1000, 0x1aab27c2, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "d2732.2f",		0x1000, 0x767bdd71, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "tms2516.6j",		0x0800, 0xea6fd981, 2 | BRF_GRA },           //  8 Sprites

	{ "tms2516.10f",	0x0800, 0x4ea1c3d9, 3 | BRF_PRG | BRF_ESS }, //  9 Sound CPU Code
	{ "tms2564.10j",	0x2000, 0xda9d8588, 3 | BRF_PRG | BRF_ESS }, // 10

	{ "sn74s288n.6c",	0x0020, 0xf76b4fcf, 0 | BRF_OPT },           // 11 PROMs
	{ "sn74s288n.3d",	0x0020, 0xe26f9053, 0 | BRF_OPT },           // 12
};

STD_ROM_PICK(venture5b)
STD_ROM_FN(venture5b)

struct BurnDriver BurnDrvVenture5b = {
	"venture5b", "venture", NULL, NULL, "1981",
	"Venture (ver. 5 Set 2, bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_RUNGUN, 0,
	NULL, venture5bRomInfo, venture5bRomName, NULL, NULL, NULL, NULL, TargInputInfo, VentureDIPInfo,
	VentureInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 8,
	256, 256, 4, 3
};


// Mouse Trap (ver. 5)

static struct BurnRomInfo mtrapRomDesc[] = {
	{ "mtl-5_11a.11a",	0x1000, 0xbd6c3eb5, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "mtl-5_10a.10a",	0x1000, 0x75b0593e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mtl-5_9a.9a",	0x1000, 0x28dd20ff, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "mtl-5_8a.8a",	0x1000, 0xcc09f7a4, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "mtl-5_7a.7a",	0x1000, 0xcaafbb6d, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "mtl-5_6a.6a",	0x1000, 0xd85e52ca, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "mtl_11d.11d",	0x0800, 0xc6e4d339, 2 | BRF_GRA },           //  6 Sprites

	{ "mta_5a.5a",		0x0800, 0xdbe4ec02, 3 | BRF_PRG | BRF_ESS }, //  7 Sound CPU Code
	{ "mta_6a.6a",		0x0800, 0xc00f0c05, 3 | BRF_PRG | BRF_ESS }, //  8
	{ "mta_7a.7a",		0x0800, 0xf3f16ca7, 3 | BRF_PRG | BRF_ESS }, //  9

	{ "mta_2a.2a",		0x1000, 0x13db8ed3, 4 | BRF_PRG | BRF_ESS }, // 10 CVSD
	{ "mta_3a.3a",		0x1000, 0x31bdfe5c, 4 | BRF_PRG | BRF_ESS }, // 11
	{ "mta_4a.4a",		0x1000, 0x1502d0e8, 4 | BRF_PRG | BRF_ESS }, // 12
	{ "mta_1a.1a",		0x1000, 0x658482a6, 4 | BRF_PRG | BRF_ESS }, // 13

	{ "hrl14h.h14",		0x0020, 0xf76b4fcf, 0 | BRF_OPT },           // 14 PROMs
	{ "vel5c-11.c5",	0x0100, 0x43b35bb7, 0 | BRF_OPT },           // 15
	{ "hrl6d.d6",		0x0020, 0xe26f9053, 0 | BRF_OPT },           // 16
};

STD_ROM_PICK(mtrap)
STD_ROM_FN(mtrap)

static INT32 MtrapInit()
{
	return MtrapCommonInit(0x6800);
}

struct BurnDriver BurnDrvMtrap = {
	"mtrap", NULL, NULL, NULL, "1981",
	"Mouse Trap (ver. 5)\0", NULL, "Exidy", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_MAZE, 0,
	NULL, mtrapRomInfo, mtrapRomName, NULL, NULL, NULL, NULL, MtrapInputInfo, MtrapDIPInfo,
	MtrapInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 8,
	256, 256, 4, 3
};


// Mouse Trap (ver. 4)

static struct BurnRomInfo mtrap4RomDesc[] = {
	{ "mtl-4_11a.11a",	0x1000, 0x2879cb8d, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "mtl-4_10a.10a",	0x1000, 0xd7378af9, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mtl-4_9a.9a",	0x1000, 0xbe667e64, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "mtl-4_8a.8a",	0x1000, 0xde0442f8, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "mtl-4_7a.7a",	0x1000, 0xcdf8c6a8, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "mtl-4_6a.6a",	0x1000, 0x77d3f2e6, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "mtl_11d.11d",	0x0800, 0xc6e4d339, 2 | BRF_GRA },           //  6 Sprites

	{ "mta_5a.5a",		0x0800, 0xdbe4ec02, 3 | BRF_PRG | BRF_ESS }, //  7 Sound CPU Code
	{ "mta_6a.6a",		0x0800, 0xc00f0c05, 3 | BRF_PRG | BRF_ESS }, //  8
	{ "mta_7a.7a",		0x0800, 0xf3f16ca7, 3 | BRF_PRG | BRF_ESS }, //  9

	{ "mta_2a.2a",		0x1000, 0x13db8ed3, 4 | BRF_PRG | BRF_ESS }, // 10 CVSD
	{ "mta_3a.3a",		0x1000, 0x31bdfe5c, 4 | BRF_PRG | BRF_ESS }, // 11
	{ "mta_4a.4a",		0x1000, 0x1502d0e8, 4 | BRF_PRG | BRF_ESS }, // 12
	{ "mta_1a.1a",		0x1000, 0x658482a6, 4 | BRF_PRG | BRF_ESS }, // 13

	{ "hrl14h.h14",		0x0020, 0xf76b4fcf, 0 | BRF_OPT },           // 14 PROMs
	{ "vel5c-11.c5",	0x0100, 0x43b35bb7, 0 | BRF_OPT },           // 15
	{ "hrl6d.d6",		0x0020, 0xe26f9053, 0 | BRF_OPT },           // 16
};

STD_ROM_PICK(mtrap4)
STD_ROM_FN(mtrap4)

struct BurnDriver BurnDrvMtrap4 = {
	"mtrap4", "mtrap", NULL, NULL, "1981",
	"Mouse Trap (ver. 4)\0", NULL, "Exidy", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_MAZE, 0,
	NULL, mtrap4RomInfo, mtrap4RomName, NULL, NULL, NULL, NULL, MtrapInputInfo, MtrapDIPInfo,
	MtrapInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 8,
	256, 256, 4, 3
};


// Mouse Trap (German, ver. 4)

static struct BurnRomInfo mtrap4gRomDesc[] = {
	{ "gmtl-4_11a.11a",	0x1000, 0xd84aa55e, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "gmtl-4_10a.10a",	0x1000, 0xc4a83a69, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gmtl-4_9a.9a",	0x1000, 0x9b7e5e7a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gmtl-4_8a.8a",	0x1000, 0xfbcf1572, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "gmtl-4_7a.7a",	0x1000, 0x7786e51b, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "gmtl-4_6a.6a",	0x1000, 0xbbb535cd, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "mtl-1_11d.11d",	0x0800, 0xc6e4d339, 2 | BRF_GRA },           //  6 Sprites

	{ "mta_5a.5a",		0x0800, 0xdbe4ec02, 3 | BRF_PRG | BRF_ESS }, //  7 Sound CPU Code
	{ "mta_6a.6a",		0x0800, 0xc00f0c05, 3 | BRF_PRG | BRF_ESS }, //  8
	{ "mta_7a.7a",		0x0800, 0xf3f16ca7, 3 | BRF_PRG | BRF_ESS }, //  9

	{ "mta_2a.2a",		0x1000, 0x13db8ed3, 4 | BRF_PRG | BRF_ESS }, // 10 CVSD
	{ "mta_3a.3a",		0x1000, 0x31bdfe5c, 4 | BRF_PRG | BRF_ESS }, // 11
	{ "mta_4a.4a",		0x1000, 0x1502d0e8, 4 | BRF_PRG | BRF_ESS }, // 12
	{ "mta_1a.1a",		0x1000, 0x658482a6, 4 | BRF_PRG | BRF_ESS }, // 13

	{ "hrl14h.h14",		0x0020, 0xf76b4fcf, 0 | BRF_OPT },           // 14 PROMs
	{ "vel5c-11.c5",	0x0100, 0x43b35bb7, 0 | BRF_OPT },           // 15
	{ "hrl6d.d6",		0x0020, 0xe26f9053, 0 | BRF_OPT },           // 16
};

STD_ROM_PICK(mtrap4g)
STD_ROM_FN(mtrap4g)

struct BurnDriver BurnDrvMtrap4g = {
	"mtrap4g", "mtrap", NULL, NULL, "1981",
	"Mouse Trap (German, ver. 4)\0", NULL, "Exidy", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_MAZE, 0,
	NULL, mtrap4gRomInfo, mtrap4gRomName, NULL, NULL, NULL, NULL, MtrapInputInfo, MtrapDIPInfo,
	MtrapInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 8,
	256, 256, 4, 3
};


// Mouse Trap (ver. 3)

static struct BurnRomInfo mtrap3RomDesc[] = {
	{ "mtl-3_11a.11a",	0x1000, 0x4091be6e, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "mtl-3_10a.10a",	0x1000, 0x38250c2f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mtl-3_9a.9a",	0x1000, 0x2eec988e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "mtl-3_8a.8a",	0x1000, 0x744b4b1c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "mtl-3_7a.7a",	0x1000, 0xea8ec479, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "mtl-3_6a.6a",	0x1000, 0xd72ba72d, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "mtl_11d.11d",	0x0800, 0xc6e4d339, 2 | BRF_GRA },           //  6 Sprites

	{ "mta_5a.5a",		0x0800, 0xdbe4ec02, 3 | BRF_PRG | BRF_ESS }, //  7 Sound CPU Code
	{ "mta_6a.6a",		0x0800, 0xc00f0c05, 3 | BRF_PRG | BRF_ESS }, //  8
	{ "mta_7a.7a",		0x0800, 0xf3f16ca7, 3 | BRF_PRG | BRF_ESS }, //  9

	{ "mta_2a.2a",		0x1000, 0x13db8ed3, 4 | BRF_PRG | BRF_ESS }, // 10 CVSD
	{ "mta_3a.3a",		0x1000, 0x31bdfe5c, 4 | BRF_PRG | BRF_ESS }, // 11
	{ "mta_4a.4a",		0x1000, 0x1502d0e8, 4 | BRF_PRG | BRF_ESS }, // 12
	{ "mta_1a.1a",		0x1000, 0x658482a6, 4 | BRF_PRG | BRF_ESS }, // 13

	{ "hrl14h.h14",		0x0020, 0xf76b4fcf, 0 | BRF_OPT },           // 14 PROMs
	{ "vel5c-11.c5",	0x0100, 0x43b35bb7, 0 | BRF_OPT },           // 15
	{ "hrl6d.d6",		0x0020, 0xe26f9053, 0 | BRF_OPT },           // 16
};

STD_ROM_PICK(mtrap3)
STD_ROM_FN(mtrap3)

struct BurnDriver BurnDrvMtrap3 = {
	"mtrap3", "mtrap", NULL, NULL, "1981",
	"Mouse Trap (ver. 3)\0", NULL, "Exidy", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_MAZE, 0,
	NULL, mtrap3RomInfo, mtrap3RomName, NULL, NULL, NULL, NULL, MtrapInputInfo, MtrapDIPInfo,
	MtrapInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 8,
	256, 256, 4, 3
};


// Mouse Trap (ver. 2)

static struct BurnRomInfo mtrap2RomDesc[] = {
	{ "mtl-2_11a.11a",	0x1000, 0xa8cc3a18, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "mtl-2_10a.10a",	0x1000, 0xe26b074c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mtl-2_9a.9a",	0x1000, 0x845394f6, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "mtl-2_8a.8a",	0x1000, 0x854d2d50, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "mtl-2_7a.7a",	0x1000, 0x3d235f95, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "mtl-2_6a.6a",	0x1000, 0x7ed7632a, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "mtl_11d.11d",	0x0800, 0xc6e4d339, 2 | BRF_GRA },           //  6 Sprites

	{ "mta_5a.5a",		0x0800, 0xdbe4ec02, 3 | BRF_PRG | BRF_ESS }, //  7 Sound CPU Code
	{ "mta_6a.6a",		0x0800, 0xc00f0c05, 3 | BRF_PRG | BRF_ESS }, //  8
	{ "mta_7a.7a",		0x0800, 0xf3f16ca7, 3 | BRF_PRG | BRF_ESS }, //  9

	{ "mta_2a.2a",		0x1000, 0x13db8ed3, 4 | BRF_PRG | BRF_ESS }, // 10 CVSD
	{ "mta_3a.3a",		0x1000, 0x31bdfe5c, 4 | BRF_PRG | BRF_ESS }, // 11
	{ "mta_4a.4a",		0x1000, 0x1502d0e8, 4 | BRF_PRG | BRF_ESS }, // 12
	{ "mta_1a.1a",		0x1000, 0x658482a6, 4 | BRF_PRG | BRF_ESS }, // 13

	{ "hrl14h.h14",		0x0020, 0xf76b4fcf, 0 | BRF_OPT },           // 14 PROMs
	{ "vel5c-11.c5",	0x0100, 0x43b35bb7, 0 | BRF_OPT },           // 15
	{ "hrl6d.d6",		0x0020, 0xe26f9053, 0 | BRF_OPT },           // 16
};

STD_ROM_PICK(mtrap2)
STD_ROM_FN(mtrap2)

struct BurnDriver BurnDrvMtrap2 = {
	"mtrap2", "mtrap", NULL, NULL, "1981",
	"Mouse Trap (ver. 2)\0", NULL, "Exidy", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_MAZE, 0,
	NULL, mtrap2RomInfo, mtrap2RomName, NULL, NULL, NULL, NULL, MtrapInputInfo, MtrapDIPInfo,
	MtrapInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 8,
	256, 256, 4, 3
};


// Mouse Trap (bootleg)

static struct BurnRomInfo mtrapbRomDesc[] = {
	{ "cpu.p2",			0x1000, 0xa0faa3e5, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "cpu.m2",			0x1000, 0xd7378af9, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "cpu.l2",			0x1000, 0xbe667e64, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "cpu.k2",			0x1000, 0x69471f27, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "cpu.j2",			0x1000, 0x1eb0c4c9, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "cpu.h2",			0x1000, 0x16ea9a51, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "2516.j6",		0x0800, 0xc6e4d339, 2 | BRF_GRA },           //  6 Sprites

	{ "2564.j10",		0x2000, 0xd4160aa8, 3 | BRF_PRG | BRF_ESS }, //  7 Sound CPU Code
	
	{ "mta_2a.2a",		0x1000, 0x13db8ed3, 4 | BRF_PRG | BRF_ESS }, //  8 CVSD
	{ "mta_3a.3a",		0x1000, 0x31bdfe5c, 4 | BRF_PRG | BRF_ESS }, //  9
	{ "mta_4a.4a",		0x1000, 0x1502d0e8, 4 | BRF_PRG | BRF_ESS }, // 10
	{ "mta_1a.1a",		0x1000, 0x658482a6, 4 | BRF_PRG | BRF_ESS }, // 11

	{ "74s288.6c",		0x0020, 0xf76b4fcf, 0 | BRF_OPT },           // 12 PROMs
	{ "24s10n.1c",		0x0100, 0x43b35bb7, 0 | BRF_OPT },           // 13
	{ "74s288.3d",		0x0020, 0xe26f9053, 0 | BRF_OPT },           // 14
};

STD_ROM_PICK(mtrapb)
STD_ROM_FN(mtrapb)

static INT32 MtrapbInit()
{
	return MtrapCommonInit(0x6000);
}

struct BurnDriver BurnDrvMtrapb = {
	"mtrapb", "mtrap", NULL, NULL, "1981",
	"Mouse Trap (bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_MAZE, 0,
	NULL, mtrapbRomInfo, mtrapbRomName, NULL, NULL, NULL, NULL, MtrapInputInfo, MtrapDIPInfo,
	MtrapbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 8,
	256, 256, 4, 3
};


// Mouse Trap (ver. 4, bootleg)

static struct BurnRomInfo mtrapb2RomDesc[] = {
	{ "ms11.2p",		0x1000, 0x2879cb8d, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "ms10.2m",		0x1000, 0xd7378af9, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ms9.2l",			0x1000, 0xbe667e64, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ms8.2k",			0x1000, 0xde0442f8, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ms7.2h",			0x1000, 0xcdf8c6a8, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "ms6.2f",			0x1000, 0x77d3f2e6, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "ms11d.6j",		0x0800, 0xc6e4d339, 2 | BRF_GRA },           //  6 Sprites

	{ "ms1a.10j",		0x2000, 0xd4160aa8, 3 | BRF_PRG | BRF_ESS }, //  7 Sound CPU Code

	{ "ms2a.8d",		0x1000, 0x13db8ed3, 4 | BRF_PRG | BRF_ESS }, //  8 CVSD
	{ "ms3a.8e",		0x1000, 0x31bdfe5c, 4 | BRF_PRG | BRF_ESS }, //  9
	{ "ms4a.8f",		0x1000, 0x1502d0e8, 4 | BRF_PRG | BRF_ESS }, // 10
	{ "ms5a.8h",		0x1000, 0x658482a6, 4 | BRF_PRG | BRF_ESS }, // 11

	{ "74s288.6c",		0x0020, 0xf76b4fcf, 0 | BRF_OPT },           // 12 PROMs
	{ "24s10n.1c",		0x0100, 0x43b35bb7, 0 | BRF_OPT },           // 13
	{ "74s288.3d",		0x0020, 0xe26f9053, 0 | BRF_OPT },           // 14
};

STD_ROM_PICK(mtrapb2)
STD_ROM_FN(mtrapb2)

struct BurnDriver BurnDrvMtrapb2 = {
	"mtrapb2", "mtrap", NULL, NULL, "1981",
	"Mouse Trap (ver. 4, bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_MAZE, 0,
	NULL, mtrapb2RomInfo, mtrapb2RomName, NULL, NULL, NULL, NULL, MtrapInputInfo, MtrapDIPInfo,
	MtrapbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 8,
	256, 256, 4, 3
};


// Teeter Torture (Prototype, ver. 1)

static struct BurnRomInfo teetertRomDesc[] = {
	{ "13a-cpu",		0x1000, 0xf4e4d991, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "12a-cpu",		0x1000, 0xc6d8cb04, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "11a-cpu",		0x1000, 0xbac9b259, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "10a-cpu",		0x1000, 0x3ae7e445, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "9a-cpu",			0x1000, 0x0cba424d, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "8a-cpu",			0x1000, 0x68de66e7, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "7a-cpu",			0x1000, 0x84491333, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "6a-cpu",			0x1000, 0x3600d465, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "11d-cpu",		0x0800, 0x0fe70b00, 2 | BRF_GRA },           //  8 Sprites

	{ "3a-ac",			0x0800, 0x83b8836f, 3 | BRF_PRG | BRF_ESS }, //  9 Sound CPU Code
	{ "4a-ac",			0x0800, 0x5154c39e, 3 | BRF_PRG | BRF_ESS }, // 10
	{ "5a-ac",			0x0800, 0x1e1e3916, 3 | BRF_PRG | BRF_ESS }, // 11
	{ "6a-ac",			0x0800, 0x80f3357a, 3 | BRF_PRG | BRF_ESS }, // 12
	{ "7a-ac",			0x0800, 0x466addc7, 3 | BRF_PRG | BRF_ESS }, // 13

	{ "tt14h.123",		0x0020, 0xf76b4fcf, 0 | BRF_OPT },           // 14 PROMs
	{ "tt5c.129",		0x0100, 0x43b35bb7, 0 | BRF_OPT },           // 15
	{ "tt6d.123",		0x0020, 0xe26f9053, 0 | BRF_OPT },           // 16
};

STD_ROM_PICK(teetert)
STD_ROM_FN(teetert)

struct BurnDriver BurnDrvTeetert = {
	"teetert", NULL, NULL, NULL, "1982",
	"Teeter Torture (Prototype, ver. 1)\0", NULL, "Exidy", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, teetertRomInfo, teetertRomName, NULL, NULL, NULL, NULL, TeetertInputInfo, TeetertDIPInfo,
	TeetertInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 8,
	256, 256, 4, 3
};


// Pepper II (ver. 8)

static struct BurnRomInfo pepper2RomDesc[] = {
	{ "p2l-8_12a.12a",	0x1000, 0x33db4737, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "p2l-8_11a.11a",	0x1000, 0xa1f43b1f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "p2l-8_10a.10a",	0x1000, 0x4d7d7786, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "p2l-8_9a.9a",	0x1000, 0xb3362298, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "p2l-8_8a.8a",	0x1000, 0x64d106ed, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "p2l-8_7a.7a",	0x1000, 0xb1c6f07c, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "p2l-8_6a.6a",	0x1000, 0x515b1046, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "p2l-1_11d.11d",	0x0800, 0xb25160cd, 2 | BRF_GRA },           //  7 Sprites

	{ "p2a-2_5a.5a",	0x0800, 0x90e3c781, 3 | BRF_PRG | BRF_ESS }, //  8 Sound CPU Code
	{ "p2a-2_6a.6a",	0x0800, 0xdd343e34, 3 | BRF_PRG | BRF_ESS }, //  9
	{ "p2a-2_7a.7a",	0x0800, 0xe02b4356, 3 | BRF_PRG | BRF_ESS }, // 10

	{ "hrl14h-1.h14",	0x0020, 0xf76b4fcf, 0 | BRF_OPT },           // 11 PROMs
	{ "p2l5c-1.c5",		0x0100, 0xe1e867ae, 0 | BRF_OPT },           // 12
	{ "p2l6d-1.d6",		0x0020, 0x0da1bdf9, 0 | BRF_OPT },           // 13
};

STD_ROM_PICK(pepper2)
STD_ROM_FN(pepper2)

struct BurnDriver BurnDrvPepper2 = {
	"pepper2", NULL, NULL, NULL, "1982",
	"Pepper II (ver. 8)\0", NULL, "Exidy", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_MAZE, 0,
	NULL, pepper2RomInfo, pepper2RomName, NULL, NULL, NULL, NULL, Pepper2InputInfo, Pepper2DIPInfo,
	Pepper2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 8,
	256, 256, 4, 3
};


// Pepper II (ver. 7)

static struct BurnRomInfo pepper27RomDesc[] = {
	{ "p2l-7_12a.12a",	0x1000, 0xb3bc51cd, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "p2l-7_11a.11a",	0x1000, 0xc8b834cd, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "p2l-7_10a.10a",	0x1000, 0xc3e864a2, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "p2l-7_9a.9a",	0x1000, 0x451003b2, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "p2l-7_8a.8a",	0x1000, 0xc666cafb, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "p2l-7_7a.7a",	0x1000, 0xac1282ef, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "p2l-7_6a.6a",	0x1000, 0xdb8dd4fc, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "p2l-1_11d.11d",	0x0800, 0xb25160cd, 2 | BRF_GRA },           //  7 Sprites

	{ "p2a-2_5a.5a",	0x0800, 0x90e3c781, 3 | BRF_PRG | BRF_ESS }, //  8 Sound CPU Code
	{ "p2a-2_6a.6a",	0x0800, 0xdd343e34, 3 | BRF_PRG | BRF_ESS }, //  9
	{ "p2a-2_7a.7a",	0x0800, 0xe02b4356, 3 | BRF_PRG | BRF_ESS }, // 10

	{ "hrl14h-1.h14",	0x0020, 0xf76b4fcf, 0 | BRF_OPT },           // 11 PROMs
	{ "p2l5c-1.c5",		0x0100, 0xe1e867ae, 0 | BRF_OPT },           // 12
	{ "p2l6d-1.d6",		0x0020, 0x0da1bdf9, 0 | BRF_OPT },           // 13
};

STD_ROM_PICK(pepper27)
STD_ROM_FN(pepper27)

struct BurnDriver BurnDrvPepper27 = {
	"pepper27", "pepper2", NULL, NULL, "1982",
	"Pepper II (ver. 7)\0", NULL, "Exidy", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_MAZE, 0,
	NULL, pepper27RomInfo, pepper27RomName, NULL, NULL, NULL, NULL, Pepper2InputInfo, Pepper2DIPInfo,
	Pepper2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 8,
	256, 256, 4, 3
};


// Hard Hat (ver. 2)

static struct BurnRomInfo hardhatRomDesc[] = {
	{ "hhl-2.11a",		0x1000, 0x7623deea, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "hhl-2.10a",		0x1000, 0xe6bf2fb1, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "hhl-2.9a",		0x1000, 0xacc2bce5, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "hhl-2.8a",		0x1000, 0x23c7a2f8, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "hhl-2.7a",		0x1000, 0x6f7ce1c2, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "hhl-2.6a",		0x1000, 0x2a20cf10, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "hhl-1.11d",		0x0800, 0xdbcdf353, 2 | BRF_GRA },           //  6 Sprites

	{ "hha-1.5a",		0x0800, 0x16a5a183, 3 | BRF_PRG | BRF_ESS }, //  7 Sound CPU Code
	{ "hha-1.6a",		0x0800, 0xbde64021, 3 | BRF_PRG | BRF_ESS }, //  8
	{ "hha-1.7a",		0x0800, 0x505ee5d3, 3 | BRF_PRG | BRF_ESS }, //  9
};

STD_ROM_PICK(hardhat)
STD_ROM_FN(hardhat)

struct BurnDriver BurnDrvHardhat = {
	"hardhat", NULL, NULL, NULL, "1982",
	"Hard Hat (ver. 2)\0", NULL, "Exidy", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_MAZE, 0,
	NULL, hardhatRomInfo, hardhatRomName, NULL, NULL, NULL, NULL, Pepper2InputInfo, Pepper2DIPInfo,
	HardhatInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 8,
	256, 256, 4, 3
};

// FAX (ver. 8)

static struct BurnRomInfo faxRomDesc[] = {
	{ "fxl8-13a.32",	0x1000, 0x8e30bf6b, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "fxl8-12a.32",	0x1000, 0x60a41ff1, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "fxl8-11a.32",	0x1000, 0x2c9cee8a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "fxl8-10a.32",	0x1000, 0x9b03938f, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "fxl8-9a.32",		0x1000, 0xfb869f62, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "fxl8-8a.32",		0x1000, 0xdb3470bc, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "fxl8-7a.32",		0x1000, 0x1471fef5, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "fxl8-6a.32",		0x1000, 0x812e39f3, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "fxd-1c.64",		0x2000, 0xfd7e3137, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "fxd-2c.64",		0x2000, 0xe78cb16f, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "fxd-3c.64",		0x2000, 0x57a94c6f, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "fxd-4c.64",		0x2000, 0x9036c5a2, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "fxd-5c.64",		0x2000, 0x38c03405, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "fxd-6c.64",		0x2000, 0xf48fc308, 1 | BRF_PRG | BRF_ESS }, // 13
	{ "fxd-7c.64",		0x2000, 0xcf93b924, 1 | BRF_PRG | BRF_ESS }, // 14
	{ "fxd-8c.64",		0x2000, 0x607b48da, 1 | BRF_PRG | BRF_ESS }, // 15
	{ "fxd-1b.64",		0x2000, 0x62872d4f, 1 | BRF_PRG | BRF_ESS }, // 16
	{ "fxd-2b.64",		0x2000, 0x625778d0, 1 | BRF_PRG | BRF_ESS }, // 17
	{ "fxd-3b.64",		0x2000, 0xc3473dee, 1 | BRF_PRG | BRF_ESS }, // 18
	{ "fxd-4b.64",		0x2000, 0xe39a15f5, 1 | BRF_PRG | BRF_ESS }, // 19
	{ "fxd-5b.64",		0x2000, 0x101a9d70, 1 | BRF_PRG | BRF_ESS }, // 20
	{ "fxd-6b.64",		0x2000, 0x374a8f05, 1 | BRF_PRG | BRF_ESS }, // 21
	{ "fxd-7b.64",		0x2000, 0xf7e7f824, 1 | BRF_PRG | BRF_ESS }, // 22
	{ "fxd-8b.64",		0x2000, 0x8f1a5287, 1 | BRF_PRG | BRF_ESS }, // 23
	{ "fxd-1a.64",		0x2000, 0xfc5e6344, 1 | BRF_PRG | BRF_ESS }, // 24
	{ "fxd-2a.64",		0x2000, 0x43cf60b3, 1 | BRF_PRG | BRF_ESS }, // 25
	{ "fxd-3a.64",		0x2000, 0x6b7d29cb, 1 | BRF_PRG | BRF_ESS }, // 26
	{ "fxd-4a.64",		0x2000, 0xb9de3c2d, 1 | BRF_PRG | BRF_ESS }, // 27
	{ "fxd-5a.64",		0x2000, 0x67285bc6, 1 | BRF_PRG | BRF_ESS }, // 28
	{ "fxd-6a.64",		0x2000, 0xba67b7b2, 1 | BRF_PRG | BRF_ESS }, // 29

	{ "fxl1-11d.32",	0x0800, 0x62083db2, 2 | BRF_GRA },           // 30 Sprites

	{ "fxa2-5a.16",		0x0800, 0x7c525aec, 3 | BRF_PRG | BRF_ESS }, // 31 Sound CPU Code
	{ "fxa2-6a.16",		0x0800, 0x2b3bfc44, 3 | BRF_PRG | BRF_ESS }, // 32
	{ "fxa2-7a.16",		0x0800, 0x578c62b7, 3 | BRF_PRG | BRF_ESS }, // 33

	{ "fxl-6b",			0x0100, 0xe1e867ae, 0 | BRF_OPT },           // 34 PROMs
	{ "fxl-8b",			0x0020, 0x0da1bdf9, 0 | BRF_OPT },           // 35
	{ "fxl-11b",		0x0020, 0xf76b4fcf, 0 | BRF_OPT },           // 36
	{ "fxl-12b",		0x0100, 0x6b5aa3d7, 0 | BRF_OPT },           // 37
};

STD_ROM_PICK(fax)
STD_ROM_FN(fax)

struct BurnDriver BurnDrvFax = {
	"fax", NULL, NULL, NULL, "1983",
	"FAX (ver. 8)\0", NULL, "Exidy", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_QUIZ, 0,
	NULL, faxRomInfo, faxRomName, NULL, NULL, NULL, NULL, FaxInputInfo, FaxDIPInfo,
	FaxInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 8,
	256, 256, 4, 3
};


// FAX 2 (ver. 8)

static struct BurnRomInfo fax2RomDesc[] = {
	{ "fxl8-13a.32",	0x1000, 0x8e30bf6b, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "fxl8-12a.32",	0x1000, 0x60a41ff1, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "fxl8-11a.32",	0x1000, 0x2c9cee8a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "fxl8-10a.32",	0x1000, 0x9b03938f, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "fxl8-9a.32",		0x1000, 0xfb869f62, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "fxl8-8a.32",		0x1000, 0xdb3470bc, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "fxl8-7a.32",		0x1000, 0x1471fef5, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "fxl8-6a.32",		0x1000, 0x812e39f3, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "fxdb1-1c.bin",	0x2000, 0x0e42a2a4, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "fxdb1-2c.bin",	0x2000, 0xcef8d49a, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "fxdb1-3c.bin",	0x2000, 0x96217b39, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "fxdb1-4c.bin",	0x2000, 0x9f1522d8, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "fxdb1-5c.bin",	0x2000, 0x4770eb04, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "fxdb1-6c.bin",	0x2000, 0x07c742ab, 1 | BRF_PRG | BRF_ESS }, // 13
	{ "fxdb1-7c.bin",	0x2000, 0xf2f39ebb, 1 | BRF_PRG | BRF_ESS }, // 14
	{ "fxdb1-8c.bin",	0x2000, 0x00f73e30, 1 | BRF_PRG | BRF_ESS }, // 15
	{ "fxdb1-1b.bin",	0x2000, 0xe13341cf, 1 | BRF_PRG | BRF_ESS }, // 16
	{ "fxdb1-2b.bin",	0x2000, 0x300d7a6f, 1 | BRF_PRG | BRF_ESS }, // 17
	{ "fxdb1-3b.bin",	0x2000, 0xdb9a6a3a, 1 | BRF_PRG | BRF_ESS }, // 18
	{ "fxdb1-4b.bin",	0x2000, 0x47faeb43, 1 | BRF_PRG | BRF_ESS }, // 19
	{ "fxdb1-5b.bin",	0x2000, 0x3ecf974f, 1 | BRF_PRG | BRF_ESS }, // 20
	{ "fxdb1-6b.bin",	0x2000, 0x526c4c0d, 1 | BRF_PRG | BRF_ESS }, // 21
	{ "fxdb1-7b.bin",	0x2000, 0x4cf8217c, 1 | BRF_PRG | BRF_ESS }, // 22
	{ "fxdb1-8b.bin",	0x2000, 0x99485e27, 1 | BRF_PRG | BRF_ESS }, // 23
	{ "fxdb1-1a.bin",	0x2000, 0x97c153b5, 1 | BRF_PRG | BRF_ESS }, // 24
	{ "fxdb1-2a.bin",	0x2000, 0x63258140, 1 | BRF_PRG | BRF_ESS }, // 25
	{ "fxdb1-3a.bin",	0x2000, 0x1c698727, 1 | BRF_PRG | BRF_ESS }, // 26
	{ "fxdb1-4a.bin",	0x2000, 0x8283d6fc, 1 | BRF_PRG | BRF_ESS }, // 27
	{ "fxdb1-5a.bin",	0x2000, 0xb69542d2, 1 | BRF_PRG | BRF_ESS }, // 28
	{ "fxdb1-6a.bin",	0x2000, 0xff949367, 1 | BRF_PRG | BRF_ESS }, // 29
	{ "fxdb1-7a.bin",	0x2000, 0x0f97b874, 1 | BRF_PRG | BRF_ESS }, // 30
	{ "fxdb1-8a.bin",	0x2000, 0x1d055bea, 1 | BRF_PRG | BRF_ESS }, // 31

	{ "fxl1-11d.32",	0x0800, 0x62083db2, 2 | BRF_GRA },           // 32 Sprites

	{ "fxa2-5a.16",		0x0800, 0x7c525aec, 3 | BRF_PRG | BRF_ESS }, // 33 Sound CPU Code
	{ "fxa2-6a.16",		0x0800, 0x2b3bfc44, 3 | BRF_PRG | BRF_ESS }, // 34
	{ "fxa2-7a.16",		0x0800, 0x578c62b7, 3 | BRF_PRG | BRF_ESS }, // 35

	{ "fxl-6b",			0x0100, 0xe1e867ae, 0 | BRF_OPT },           // 36 PROMs
	{ "fxl-8b",			0x0020, 0x0da1bdf9, 0 | BRF_OPT },           // 37
	{ "fxl-11b",		0x0020, 0xf76b4fcf, 0 | BRF_OPT },           // 38
	{ "fxl-12b",		0x0100, 0x6b5aa3d7, 0 | BRF_OPT },           // 39
};

STD_ROM_PICK(fax2)
STD_ROM_FN(fax2)

struct BurnDriver BurnDrvFax2 = {
	"fax2", "fax", NULL, NULL, "1983",
	"FAX 2 (ver. 8)\0", NULL, "Exidy", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_QUIZ, 0,
	NULL, fax2RomInfo, fax2RomName, NULL, NULL, NULL, NULL, FaxInputInfo, FaxDIPInfo,
	FaxInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 8,
	256, 256, 4, 3
};
