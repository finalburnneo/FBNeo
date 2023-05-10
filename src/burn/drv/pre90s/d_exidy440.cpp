// FinalBurn Neo Exidy 440 hardware driver module
// Based on MAME driver by Aaron Giles

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "burn_gun.h"
#include "exidy440_snd.h"
#include "dtimer.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvM6809ROM[2];
static UINT8 *DrvSndROM;
static INT32  DrvSndROMLen;
static UINT8 *DrvNVRAM;
static UINT8 *DrvImageRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvM6809RAM[2];
static UINT8 *DrvVidRAM;
static UINT8 *DrvPalRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 mainbank;
static INT32 palettebank;
static INT32 palettebank_vis;
static INT32 vram_scanline;
static INT32 firq_select;
static INT32 firq_enable;
static INT32 firq_beam;
static INT32 firq_vblank;
static INT32 beam_firq_count;
static INT32 topsecex_yscroll;
static INT32 latched_x;
static INT32 previous_coin;

static INT32 showdown_bank_select;
static INT32 showdown_bank_offset;
static UINT8 *showdown_bank_data[2] = { NULL, NULL };

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvJoy5[8];
static UINT8 DrvJoy6[8];
static UINT8 DrvDips[4];
static UINT8 DrvInputs[6];
static UINT8 DrvReset;

static INT32 nExtraCycles[2];

static INT16 Analog[2];

static INT32 crossbow = 0;
static INT32 claypign = 0;
static INT32 topsecex = 0;
static INT32 hitnmiss = 0;
static INT32 whodunit = 0;
static INT32 cheyenne = 0;

static INT32 lastline = 0;
static INT32 scanline = 0;

#define ONE_LINE (1622400/60/260)

static void partial_update(); // forward

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo CrossbowInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 start"	},
	A("P1 Gun X",    	BIT_ANALOG_REL, &Analog[0],		"mouse x-axis" ),
	A("P1 Gun Y",    	BIT_ANALOG_REL, &Analog[1],		"mouse y-axis" ),
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",			BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};

STDINPUTINFO(Crossbow)

static struct BurnInputInfo ShowdownInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 start"	},
	A("P1 Gun X",    	BIT_ANALOG_REL, &Analog[0],		"mouse x-axis" ),
	A("P1 Gun Y",    	BIT_ANALOG_REL, &Analog[1],		"mouse y-axis" ),
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 1"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy4 + 1,	"p1 fire 4"	},
	{"P1 Button 5",		BIT_DIGITAL,	DrvJoy4 + 2,	"p1 fire 5"	},
	{"P1 Button 6",		BIT_DIGITAL,	DrvJoy4 + 3,	"p1 fire 6"	},
	{"P1 Button 7",		BIT_DIGITAL,	DrvJoy4 + 4,	"p1 fire 7"	},
	{"P1 Button 8",		BIT_DIGITAL,	DrvJoy4 + 5,	"p1 fire 8"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",			BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};

STDINPUTINFO(Showdown)

static struct BurnInputInfo HitnmissInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 start"	},
	A("P1 Gun X",    	BIT_ANALOG_REL, &Analog[0],		"mouse x-axis" ),
	A("P1 Gun Y",    	BIT_ANALOG_REL, &Analog[1],		"mouse y-axis" ),
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",			BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};

STDINPUTINFO(Hitnmiss)

static struct BurnInputInfo TopsecexInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start / Top Secret", BIT_DIGITAL,	DrvJoy5 + 7,	"p1 start"	},
	A("P1 Wheel", 		BIT_ANALOG_REL, &Analog[0],		"p1 x-axis"),
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy6 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 fire 2"	},

	{"P1 Fireball",		BIT_DIGITAL,	DrvJoy5 + 0,	"p1 fire 3"	},
	{"P1 Laser",		BIT_DIGITAL,	DrvJoy5 + 1,	"p1 fire 4"	},
	{"P1 Missile",		BIT_DIGITAL,	DrvJoy5 + 2,	"p1 fire 5"	},
	{"P1 Oil",			BIT_DIGITAL,	DrvJoy5 + 3,	"p1 fire 6"	},
	{"P1 Turbo",		BIT_DIGITAL,	DrvJoy5 + 4,	"p1 fire 7"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",			BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};

STDINPUTINFO(Topsecex)

#define common_coinage(xpos)							\
	{0   , 0xfe, 0   ,    16, "Coinage"				},	\
	{xpos, 0x01, 0x0f, 0x03, "4 Coins 1 Credits"	},	\
	{xpos, 0x01, 0x0f, 0x07, "3 Coins 1 Credits"	},	\
	{xpos, 0x01, 0x0f, 0x02, "4 Coins 2 Credits"	},	\
	{xpos, 0x01, 0x0f, 0x0b, "2 Coins 1 Credits"	},	\
	{xpos, 0x01, 0x0f, 0x06, "3 Coins 2 Credits"	},	\
	{xpos, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},	\
	{xpos, 0x01, 0x0f, 0x00, "4 Coins 4 Credits"	},	\
	{xpos, 0x01, 0x0f, 0x05, "3 Coins 3 Credits"	},	\
	{xpos, 0x01, 0x0f, 0x0a, "2 Coins 2 Credits"	},	\
	{xpos, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},	\
	{xpos, 0x01, 0x0f, 0x04, "3 Coins 4 Credits"	},	\
	{xpos, 0x01, 0x0f, 0x09, "2 Coins 3 Credits"	},	\
	{xpos, 0x01, 0x0f, 0x08, "2 Coins 4 Credits"	},	\
	{xpos, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},	\
	{xpos, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},	\
	{xpos, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},	\

static struct BurnDIPInfo CrossbowDIPList[]=
{
	DIP_OFFSET(0x06)
	{0x00, 0xff, 0xff, 0xff, NULL					}, // all active low (0xff)
	{0x01, 0xff, 0xff, 0xff, NULL					},
	{0x02, 0xff, 0xff, 0xff, NULL					},
	{0x03, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x00, 0x01, 0x0c, 0x08, "2"					},
	{0x00, 0x01, 0x0c, 0x0c, "3"					},
	{0x00, 0x01, 0x0c, 0x04, "4"					},
	{0x00, 0x01, 0x0c, 0x00, "5"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x00, 0x01, 0x30, 0x00, "Easy"					},
	{0x00, 0x01, 0x30, 0x30, "Normal"				},
	{0x00, 0x01, 0x30, 0x20, "Hard"					},
	{0x00, 0x01, 0x30, 0x10, "Hardest"				},

	common_coinage(0x01)

	{0   , 0xfe, 0   ,    2, "Free Play"			},
	{0x01, 0x01, 0x80, 0x80, "Off"					},
	{0x01, 0x01, 0x80, 0x00, "On"					},
};

STDDIPINFO(Crossbow)

static struct BurnDIPInfo CheyenneDIPList[]=
{
	DIP_OFFSET(0x06)
	{0x00, 0xff, 0xff, 0x00, NULL					},
	{0x01, 0xff, 0xff, 0xff, NULL					},
	{0x02, 0xff, 0xff, 0xff, NULL					},
	{0x03, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x00, 0x01, 0x0c, 0x04, "2"					},
	{0x00, 0x01, 0x0c, 0x00, "3"					},
	{0x00, 0x01, 0x0c, 0x08, "4"					},
	{0x00, 0x01, 0x0c, 0x0c, "5"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x00, 0x01, 0x30, 0x30, "Easy"					},
	{0x00, 0x01, 0x30, 0x00, "Normal"				},
	{0x00, 0x01, 0x30, 0x10, "Hard"					},
	{0x00, 0x01, 0x30, 0x20, "Hardest"				},

	common_coinage(0x01)

	{0   , 0xfe, 0   ,    2, "Free Play"			},
	{0x01, 0x01, 0x80, 0x80, "Off"					},
	{0x01, 0x01, 0x80, 0x00, "On"					},
};

STDDIPINFO(Cheyenne)

static struct BurnDIPInfo CombatDIPList[]=
{
	DIP_OFFSET(0x06)
	{0x00, 0xff, 0xff, 0x00, NULL					},
	{0x01, 0xff, 0xff, 0xff, NULL					},
	{0x02, 0xff, 0xff, 0x00, NULL					},
	{0x03, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x00, 0x01, 0x0c, 0x04, "2"					},
	{0x00, 0x01, 0x0c, 0x00, "3"					},
	{0x00, 0x01, 0x0c, 0x08, "4"					},
	{0x00, 0x01, 0x0c, 0x0c, "5"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x00, 0x01, 0x30, 0x30, "Easy"					},
	{0x00, 0x01, 0x30, 0x00, "Normal"				},
	{0x00, 0x01, 0x30, 0x10, "Hard"					},
	{0x00, 0x01, 0x30, 0x20, "Hardest"				},

	common_coinage(0x01)

	{0   , 0xfe, 0   ,    2, "Free Play"			},
	{0x01, 0x01, 0x80, 0x80, "Off"					},
	{0x01, 0x01, 0x80, 0x00, "On"					},
};

STDDIPINFO(Combat)

static struct BurnDIPInfo Catch22DIPList[]=
{
	DIP_OFFSET(0x06)
	{0x00, 0xff, 0xff, 0x00, NULL					},
	{0x01, 0xff, 0xff, 0xff, NULL					},
	{0x02, 0xff, 0xff, 0x00, NULL					},
	{0x03, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x00, 0x01, 0x0c, 0x04, "4"					},
	{0x00, 0x01, 0x0c, 0x00, "5"					},
	{0x00, 0x01, 0x0c, 0x08, "6"					},
	{0x00, 0x01, 0x0c, 0x0c, "7"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x00, 0x01, 0x30, 0x30, "Easy"					},
	{0x00, 0x01, 0x30, 0x00, "Normal"				},
	{0x00, 0x01, 0x30, 0x10, "Hard"					},
	{0x00, 0x01, 0x30, 0x20, "Hardest"				},

	common_coinage(0x01)

	{0   , 0xfe, 0   ,    2, "Free Play"			},
	{0x01, 0x01, 0x80, 0x80, "Off"					},
	{0x01, 0x01, 0x80, 0x00, "On"					},
};

STDDIPINFO(Catch22)

static struct BurnDIPInfo CrackshtDIPList[]=
{
	DIP_OFFSET(0x06)
	{0x00, 0xff, 0xff, 0x00, NULL					},
	{0x01, 0xff, 0xff, 0xff, NULL					},
	{0x02, 0xff, 0xff, 0x00, NULL					},
	{0x03, 0xff, 0xff, 0xfb, NULL					},

	{0   , 0xfe, 0   ,    4, "Seconds"				},
	{0x00, 0x01, 0x0c, 0x04, "20"					},
	{0x00, 0x01, 0x0c, 0x00, "30"					},
	{0x00, 0x01, 0x0c, 0x08, "40"					},
	{0x00, 0x01, 0x0c, 0x0c, "50"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x00, 0x01, 0x30, 0x30, "Easy"					},
	{0x00, 0x01, 0x30, 0x00, "Normal"				},
	{0x00, 0x01, 0x30, 0x10, "Hard"					},
	{0x00, 0x01, 0x30, 0x20, "Hardest"				},

	common_coinage(0x01)
};

STDDIPINFO(Cracksht)

static struct BurnDIPInfo ClaypignDIPList[]=
{
	DIP_OFFSET(0x06)
	{0x00, 0xff, 0xff, 0x00, NULL					},
	{0x01, 0xff, 0xff, 0xff, NULL					},
	{0x02, 0xff, 0xff, 0x00, NULL					},
	{0x03, 0xff, 0xff, 0xfb, NULL					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x00, 0x01, 0x30, 0x30, "Easy"					},
	{0x00, 0x01, 0x30, 0x00, "Normal"				},
	{0x00, 0x01, 0x30, 0x10, "Hard"					},
	{0x00, 0x01, 0x30, 0x20, "Hardest"				},

	common_coinage(0x01)
};

STDDIPINFO(Claypign)

static struct BurnDIPInfo ChillerDIPList[]=
{
	DIP_OFFSET(0x06)
	{0x00, 0xff, 0xff, 0x00, NULL					},
	{0x01, 0xff, 0xff, 0xff, NULL					},
	{0x02, 0xff, 0xff, 0x00, NULL					},
	{0x03, 0xff, 0xff, 0xfb, NULL					},

	{0   , 0xfe, 0   ,    4, "Seconds"				},
	{0x00, 0x01, 0x0c, 0x04, "30"					},
	{0x00, 0x01, 0x0c, 0x00, "45"					},
	{0x00, 0x01, 0x0c, 0x08, "60"					},
	{0x00, 0x01, 0x0c, 0x0c, "70"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x00, 0x01, 0x30, 0x30, "Easy"					},
	{0x00, 0x01, 0x30, 0x00, "Normal"				},
	{0x00, 0x01, 0x30, 0x10, "Hard"					},
	{0x00, 0x01, 0x30, 0x20, "Hardest"				},

	common_coinage(0x01)
};

STDDIPINFO(Chiller)

static struct BurnDIPInfo ShowdownDIPList[]=
{
	DIP_OFFSET(0x0b)
	{0x00, 0xff, 0xff, 0x00, NULL					},
	{0x01, 0xff, 0xff, 0xff, NULL					},
	{0x02, 0xff, 0xff, 0x00, NULL					},
	{0x03, 0xff, 0xff, 0xfb, NULL					},

	{0   , 0xfe, 0   ,    4, "Hands"				},
	{0x00, 0x01, 0x0c, 0x04, "1"					},
	{0x00, 0x01, 0x0c, 0x00, "2"					},
	{0x00, 0x01, 0x0c, 0x08, "3"					},
	{0x00, 0x01, 0x0c, 0x0c, "4"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x00, 0x01, 0x30, 0x30, "Easy"					},
	{0x00, 0x01, 0x30, 0x00, "Normal"				},
	{0x00, 0x01, 0x30, 0x10, "Hard"					},
	{0x00, 0x01, 0x30, 0x20, "Hardest"				},

	common_coinage(0x01)
};

STDDIPINFO(Showdown)

static struct BurnDIPInfo WhodunitDIPList[]=
{
	DIP_OFFSET(0x06)
	{0x00, 0xff, 0xff, 0x00, NULL					},
	{0x01, 0xff, 0xff, 0xff, NULL					},
	{0x02, 0xff, 0xff, 0x00, NULL					},
	{0x03, 0xff, 0xff, 0xfb, NULL					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x00, 0x01, 0x0c, 0x04, "2"					},
	{0x00, 0x01, 0x0c, 0x00, "3"					},
	{0x00, 0x01, 0x0c, 0x08, "4"					},
	{0x00, 0x01, 0x0c, 0x0c, "5"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x00, 0x01, 0x30, 0x30, "Easy"					},
	{0x00, 0x01, 0x30, 0x00, "Normal"				},
	{0x00, 0x01, 0x30, 0x10, "Hard"					},
	{0x00, 0x01, 0x30, 0x20, "Hardest"				},

	common_coinage(0x01)
};

STDDIPINFO(Whodunit)

static struct BurnDIPInfo HitnmissDIPList[]=
{
	DIP_OFFSET(0x06)
	{0x00, 0xff, 0xff, 0x00, NULL					},
	{0x01, 0xff, 0xff, 0xff, NULL					},
	{0x02, 0xff, 0xff, 0x00, NULL					},
	{0x03, 0xff, 0xff, 0xfb, NULL					},

	{0   , 0xfe, 0   ,    4, "Seconds"				},
	{0x00, 0x01, 0x0c, 0x04, "20"					},
	{0x00, 0x01, 0x0c, 0x00, "30"					},
	{0x00, 0x01, 0x0c, 0x08, "40"					},
	{0x00, 0x01, 0x0c, 0x0c, "50"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x00, 0x01, 0x30, 0x30, "Easy"					},
	{0x00, 0x01, 0x30, 0x00, "Normal"				},
	{0x00, 0x01, 0x30, 0x10, "Hard"					},
	{0x00, 0x01, 0x30, 0x20, "Hardest"				},

	common_coinage(0x01)

	{0   , 0xfe, 0   ,    2, "Free Play"			},
	{0x01, 0x01, 0x80, 0x80, "Off"					},
	{0x01, 0x01, 0x80, 0x00, "On"					},
};

STDDIPINFO(Hitnmiss)

static struct BurnDIPInfo TopsecexDIPList[]=
{
	DIP_OFFSET(0x0b)
	{0x00, 0xff, 0xff, 0x00, NULL					},
	{0x01, 0xff, 0xff, 0xff, NULL					},
	{0x02, 0xff, 0xff, 0x00, NULL					},
	{0x03, 0xff, 0xff, 0xfb, NULL					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x00, 0x01, 0x0c, 0x04, "3"					},
	{0x00, 0x01, 0x0c, 0x00, "4"					},
	{0x00, 0x01, 0x0c, 0x08, "5"					},
	{0x00, 0x01, 0x0c, 0x0c, "6"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x00, 0x01, 0x30, 0x30, "Easy"					},
	{0x00, 0x01, 0x30, 0x00, "Normal"				},
	{0x00, 0x01, 0x30, 0x10, "Hard"					},
	{0x00, 0x01, 0x30, 0x20, "Hardest"				},

	common_coinage(0x01)
};

STDDIPINFO(Topsecex)

static dtimer beam_timer;
static dtimer collision_timer;

static void sync_sound()
{
	INT32 cyc = (M6809TotalCycles(0) * (3244800 / 2) / 1622400) - M6809TotalCycles(1);
	if (cyc > 0) {
		M6809Run(1, cyc);
	}
}

static void update_firq()
{
	M6809SetIRQLine(1, (firq_vblank || (firq_enable && firq_beam)) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static void palette_update_write(INT32 offset)
{
	UINT16 word = (DrvPalRAM[offset + 0] << 8) | (DrvPalRAM[offset + 1] << 0);

	UINT8 r = (word >> 10) & 0x1f;
	UINT8 g = (word >>  5) & 0x1f;
	UINT8 b = (word >>  0) & 0x1f;

	r = (r << 3) | (r >> 2);
	g = (g << 3) | (g >> 2);
	b = (b << 3) | (b >> 2);

	DrvPalette[offset / 2] = BurnHighCol(r,g,b,0);
}

static void set_palette_bank()
{
	M6809MapMemory(DrvPalRAM + palettebank * 512, 0x2c00, 0x2dff, MAP_ROM); // write in handler
}

static void exidy440_main_write(UINT16 address, UINT8 data)
{
	if (address >= 0x2000 && address <= 0x29ff) {
		if ((address & 0xfff) < 0xa0) {
			partial_update(); // this must happen before writing to sprite ram
		}
		DrvSprRAM[(address & 0xfff)] = data;
		return;
	}

	if (address >= 0x6000 && address <= 0x7fff) {
		if (mainbank == 0xf) {
			DrvNVRAM[(address & 0x1fff)] = data;
		}
		return;
	}

	if (address >= 0x2a00 && address <= 0x2aff) {
		DrvVidRAM[((vram_scanline * 256 + (address & 0xff)) * 2) + 0] = (data >> 4) & 0xf;
		DrvVidRAM[((vram_scanline * 256 + (address & 0xff)) * 2) + 1] = data & 0xf;
		return;
	}

	if ((address & 0xfe00) == 0x2c00) {
		INT32 offset = (palettebank * 512) + (address & 0x1ff);
		DrvPalRAM[offset] = data;
		palette_update_write(offset & ~1);
		return;
	}

	switch (address & ~0x1f)
	{
		case 0x2b00:
			switch (address & 0x1f)
			{
				case 0x01:
					firq_vblank = 0;
					update_firq();
				return;

				case 0x02:
					vram_scanline = data;
				return;

				case 0x03:
					palettebank_vis	= (data & 0x01) >> 0;
					palettebank		= (data & 0x02) >> 1;
					firq_select		= (data & 0x04) >> 2;
					firq_enable		= (data & 0x08) >> 3;
					mainbank		= (data & 0xf0) >> 4;

					set_palette_bank();
					update_firq();
				return;
			}
		return;

		case 0x2e00:
			sync_sound();
			M6809CPUPush(1);
			exidy440_sound_command(data);
			M6809SetIRQLine(1, CPU_IRQSTATUS_ACK);
			M6809CPUPop();
		return;

		case 0x2e20:
			M6809SetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;

		case 0x2e40:
			// coin counter
		return;

		case 0x2ec0:
			if (address == 0x2ec1 && topsecex) topsecex_yscroll = data;
		return;

		case 0x2e60:
		case 0x2e80:
		case 0x2ea0:
		return; // nop
	}
}

static UINT8 showdown_bank0_read(INT32 offset)
{
	UINT8 result = 0xff;

	if (showdown_bank_select >= 0)
	{
		result = showdown_bank_data[showdown_bank_select][showdown_bank_offset++];

		if (showdown_bank_offset == 0x18) showdown_bank_offset = 0;
	}

	if (offset == 0x0055)
	{
		showdown_bank_select = -1;
	}
	else if (showdown_bank_select == -1)
	{
		showdown_bank_select = (offset == 0x1243) ? 1 : 0;
		showdown_bank_offset = 0;
	}

	return result;
}

static UINT8 exidy440_main_read(UINT16 address)
{
	if (address >= 0x2000 && address <= 0x29ff) {
		return DrvSprRAM[(address & 0xfff)];
	}

	if (address >= 0x2a00 && address <= 0x2aff) {
		return (DrvVidRAM[((vram_scanline * 256 + (address & 0xff)) * 2) + 0] << 4) |
			 (DrvVidRAM[((vram_scanline * 256 + (address & 0xff)) * 2) + 1] & 0x0f);
	}

	if ((address & 0xc000) == 0x4000) {
		if (mainbank == 0 && showdown_bank_data[0]) {
			return showdown_bank0_read(address & 0x3fff);
		}
		if (mainbank == 0xf && address >= 0x6000) {
			return DrvNVRAM[(address & 0x1fff)];
		}
		return DrvM6809ROM[0][0x10000 + (mainbank * 0x4000) + (address & 0x3fff)];
	}

	switch (address & ~0x1f)
	{
		case 0x2b00:
		{
			switch (address & 0x1f)
			{
				case 0x00:
				{
					return (scanline < 255) ? scanline : 255;
				}

				case 0x01:
				{
					firq_beam = 0;
					update_firq();
					return latched_x;
				}

				case 0x02:
				{
					return vram_scanline;
				}

				case 0x03:
				{
					INT32 ret = DrvInputs[1];
					// crossbow active-low, all else active high
					ret ^= (firq_beam ? 0x40 : 0);
					ret ^= (firq_vblank ? 0x80 : 0);
					if (whodunit) ret ^= (firq_vblank ? 0x01 : 0);
					if (hitnmiss) ret |= (ret & 1) << 1;

					return ret;
				}
			}
		}
		bprintf(0, _T("2b00-area missed %x\n"), address);
		return 0;

		case 0x2e00:
			sync_sound();
			return exidy440_sound_command_ram();

		case 0x2e20:
			M6809SetIRQLine(0, CPU_IRQSTATUS_NONE);
			return DrvInputs[0]; // coin

		case 0x2e60:
			return DrvInputs[2];

		case 0x2e80:
			return DrvInputs[3];

		case 0x2ea0:
			sync_sound();
			return (exidy440_sound_command_ack() ? 0xf7 : 0xff);

		case 0x2ec0:
			if (claypign && (address & 0xfffc) == 0x2ec0) return 0x76; // claypign protection
			if (topsecex && address == 0x2ec5) return (DrvInputs[5] & 1) ? 2 : 1; // topsecex fire 1
			if (topsecex && address == 0x2ec6) return BurnTrackballRead(0, 0);// ProcessAnalog(Analog[0], 1, INPUT_DEADZONE, 0x00, 0xff);; // topsecex wheel
			if (topsecex && address == 0x2ec7) return DrvInputs[4]; // topsecex extra buttons
			return 0;
	}

	return 0;
}

static void exidy440_audio_write(UINT16 address, UINT8 data)
{
	//if (address < 0x8000 || (address >= 0x8800 && address <= 0x93ff) || address >= 0x9c00) return; // nop

	if ((address & 0xfc00) == 0x8000) {
		exidy440_m6844_write(address & 0x1f, data);
		return;
	}

	if ((address & 0xfc00) == 0x8400) {
		exidy440_sound_volume_write(address & 0xf, data);
		return;
	}

	if ((address & 0xfc00) == 0x9400) {
		exidy440_sound_banks_write(address & 0x3, data);
		return;
	}

	if ((address & 0xfc00) == 0x9800) {
		M6809SetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;
	}
}

static UINT8 exidy440_audio_read(UINT16 address)
{
	//if (address < 0x8000 || address >= 0x8c00) return 0; // nop

	if ((address & 0xfc00) == 0x8000) {
		return exidy440_m6844_read(address & 0x1f);
	}

	if ((address & 0xfc00) == 0x8400) {
		return exidy440_sound_volume_read(address & 0xf);
	}

	if ((address & 0xfc00) == 0x8800) {
		M6809SetIRQLine(1, CPU_IRQSTATUS_NONE);
		return exidy440_sound_command_read();
	}

	return 0;
}

static void beam_cb(INT32 param)
{
	//bprintf(0, _T("beam cb @ %d   cyc %d  y-line_cyc: %d\tx-cyc  %d\n"), scanline, M6809TotalCycles(), M6809TotalCycles() / ONE_LINE, M6809TotalCycles() % ONE_LINE);
	if (firq_select && firq_enable)
	{
		firq_beam = 1;
		update_firq();
	}

	UINT8 rounded_x = (param + 1) / 2;

	latched_x = (rounded_x + 3) ^ 2;

	if (beam_firq_count++ < 12) {
		beam_timer.start(ONE_LINE, param, 1, 0);
	}
}

static void collision_cb(INT32 param)
{
	if (!firq_select && firq_enable)
	{
		firq_beam = 1;
		update_firq();
	}

	param = (param + 1) / 2;
	latched_x = (param + 3) ^ 2;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0x00, RamEnd - AllRam);

	M6809Open(0);
	mainbank = 0;
	M6809Reset();
	M6809Close();

	M6809Open(1);
	M6809Reset();
	M6809Close();

	exidy440_reset();

	timerReset();

	palettebank = 0;
	palettebank_vis = 0;
	vram_scanline = 0;
	firq_select = 0;
	firq_enable = 0;
	firq_beam = 0;
	firq_vblank = 0;
	topsecex_yscroll = 0;
	showdown_bank_select = 0;
	showdown_bank_offset = 0;

	previous_coin = DrvInputs[0] = DrvDips[3]; // get defaults from gameconfig dips

	memset(nExtraCycles, 0, sizeof(nExtraCycles));

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6809ROM[0]	= Next; Next += 0x0f0000;
	DrvM6809ROM[1]	= Next; Next += 0x002000;

	DrvSndROM		= Next; Next += 0x020000;

	DrvPalette		= (UINT32*)Next; Next += 0x400 * sizeof(UINT32);

	DrvNVRAM		= Next; Next += 0x002000;

	AllRam			= Next;

	DrvImageRAM		= Next; Next += 0x002000;
	DrvSprRAM		= Next; Next += 0x000aa0; // spr 0-9f, misc a0-9ff
	DrvM6809RAM[0]	= Next; Next += 0x001000;
	DrvM6809RAM[1]	= Next; Next += 0x002000;
	DrvVidRAM		= Next; Next += 0x020000;
	DrvPalRAM		= Next; Next += 0x000400;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvLoadRoms(INT32 banks_start)
{
	char* pRomName;
	struct BurnRomInfo ri;

	UINT8 *pLoad[3] = { DrvM6809ROM[0] + 0x8000, DrvM6809ROM[1], DrvSndROM };

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++)
	{
		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & 0xf) != 0 && (ri.nType & 0xf) <= 3) {
			INT32 type = (ri.nType - 1) & 3;
			//if (type==0) bprintf(0, _T("loading ROM %x @ %x\n"), ri.nLen, pLoad[0] - DrvM6809ROM[0]);
			if (BurnLoadRom(pLoad[type], i, 1)) return 1;
			pLoad[type] += ri.nLen;
			if ((ri.nType & 0xf) == 1 && (pLoad[0] - DrvM6809ROM[0]) == 0x10000) {
				pLoad[0] = DrvM6809ROM[0] + banks_start;
			}

			// cheyenne special case
			if ((ri.nType & 0xf) == 1 && cheyenne && (pLoad[0] - DrvM6809ROM[0]) == 0x2e000) {
				pLoad[0] = DrvM6809ROM[0] + 0x38000;
			}

			continue;
		}
	}

	if ((pLoad[1] - DrvM6809ROM[1]) == 0x1000) memcpy (DrvM6809ROM[1] + 0x1000, DrvM6809ROM[1], 0x1000); // mirror

	DrvSndROMLen = pLoad[2] - DrvSndROM;

	return 0;
}

static INT32 DrvInit(INT32 banks_start)
{
	BurnAllocMemIndex();

	if (DrvLoadRoms(banks_start)) return 1;

	M6809Init(0);
	M6809Open(0);
	M6809SetCallback(timerRun);
	M6809MapMemory(DrvImageRAM,				0x0000, 0x1fff, MAP_RAM);
//	M6809MapMemory(DrvSprRAM,				0x2000, 0x29ff, MAP_RAM); // handler. 0-9f spr, a0-9ff ?ram
//	M6809MapMemory(DrvVidRAM,				0x2a00, 0x2aff, MAP_RAM); // handler
	M6809MapMemory(DrvM6809RAM[0],			0x3000, 0x3fff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM[0] + 0x8000,	0x8000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(exidy440_main_write);
	M6809SetReadHandler(exidy440_main_read);
	M6809Close();

	M6809Init(1);
	M6809Open(1);
	M6809MapMemory(DrvM6809RAM[1],			0xa000, 0xbfff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM[1],			0xe000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(exidy440_audio_write);
	M6809SetReadHandler(exidy440_audio_read);
	M6809Close();

	exidy440_init(DrvSndROM, DrvSndROMLen, M6809TotalCycles, 3244800 / 2);

	timerInit();
	timerAdd(beam_timer, 0, beam_cb);
	timerAdd(collision_timer, 0, collision_cb);

	if (topsecex) {
		BurnTrackballInit(1);
	} else {
		BurnGunInit(1, 1);
	}

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	if (topsecex) {
		BurnTrackballExit();
	} else {
		BurnGunExit();
	}
	GenericTilesExit();
	M6809Exit();
	exidy440_exit(); // sound
	timerExit();

	BurnFreeMemIndex();

	showdown_bank_data[0] = NULL;
	showdown_bank_data[1] = NULL;

	crossbow = 0;
	claypign = 0;
	topsecex = 0;
	hitnmiss = 0;
	whodunit = 0;
	cheyenne = 0;

	return 0;
}

static INT32 cycles_to(INT32 x, INT32 y)
{
	// if y is less than current scanline, it's on the next frame
	int to_end_of_frame = (y < scanline) ? ((260-scanline)*ONE_LINE) : 0;
	int nexty = (y < scanline) ? (y*ONE_LINE) : ((y-scanline)*ONE_LINE);
	int nextx = x*ONE_LINE/320;

	return to_end_of_frame + nexty + nextx;
}

static void draw_background(INT32 miny, INT32 maxy, INT32 yoffset)
{
	INT32 sy = yoffset + miny;
	for (INT32 y = miny; y < maxy; y++,sy++)
	{
		if (sy >= 240) sy -= 240;
		UINT8 *src = DrvVidRAM + sy * 512;
		UINT16 *dst = pTransDraw + y * nScreenWidth;

		for (INT32 x = 0; x < nScreenWidth; x++) {
			dst[x] = src[x];
		}
	}
}

static void draw_sprites(INT32 miny, INT32 maxy, INT32 scroll_offset, INT32 check_collision)
{
	UINT8 *palette = &DrvPalRAM[palettebank_vis * 512];
	int count = 0;

	UINT8 *sprite = DrvSprRAM + (0x28 - 1) * 4;

	for (INT32 i = 0; i < 0x28; i++, sprite -= 4)
	{
		int image = (~sprite[3] & 0x3f);
		int xoffs = (~((sprite[1] << 8) | sprite[2]) & 0x1ff);
		int yoffs = (~sprite[0] & 0xff) + 1;
		int sy;

		if (yoffs < miny || yoffs > maxy + 16)
			continue;

		UINT8 *src = &DrvImageRAM[image * 128];

		if (xoffs >= 0x1ff - 16)
			xoffs -= 0x1ff;

		sy = yoffs + scroll_offset;
		for (INT32 y = 0; y < 16; y++, yoffs--, sy--)
		{
			if (sy >= nScreenHeight)
				sy -= nScreenHeight;
			else if (sy < 0)
				sy += nScreenHeight;

			if (yoffs < miny)
				break;

			if (yoffs < maxy)
			{
				UINT8 *old = &DrvVidRAM[sy * 512 + xoffs];
				int currx = xoffs;

				for (INT32 x = 0; x < 8; x++, old += 2)
				{
					int ipixel = *src++;
					int left = ipixel & 0xf0;
					int right = (ipixel << 4) & 0xf0;
					int pen;

					/* left pixel */
					if (left && currx >= 0 && currx < nScreenWidth)
					{
						/* combine with the background */
						pen = left | old[0];
						pTransDraw[(yoffs * nScreenWidth) + currx] = pen;

						/* check the collisions bit */
						if (check_collision && (palette[2 * pen] & 0x80) && (count++ < 128))
							collision_timer.start(cycles_to(currx, yoffs), currx, 1, 0);

					}
					currx++;

					/* right pixel */
					if (right && currx >= 0 && currx < nScreenWidth)
					{
						/* combine with the background */
						pen = right | old[1];
						pTransDraw[(yoffs * nScreenWidth) + currx] = pen;

						/* check the collisions bit */
						if (check_collision && (palette[2 * pen] & 0x80) && (count++ < 128))
							collision_timer.start(cycles_to(currx, yoffs), currx, 1, 0);
					}
					currx++;
				}
			}
			else
				src += 8;
		}
	}
}

static void partial_update()
{
	if (!pBurnDraw) return;

	if (scanline < 0 || scanline > nScreenHeight || scanline == lastline || lastline > scanline) return;
//	bprintf(0, _T("partial update: %d - %d\n"), lastline, scanline);
	if (nBurnLayer & 1) draw_background(lastline, scanline, topsecex_yscroll);
	if (nSpriteEnable & 1) draw_sprites(lastline, scanline, topsecex_yscroll, (topsecex == 0));

	lastline = scanline;
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x400; i+=2) {
			palette_update_write(i);
		}
		DrvRecalc = 0;
	}

	if (topsecex && nScreenHeight == 238) {
		// blank last line
		UINT16 *dst = pTransDraw + (nScreenHeight-1) * nScreenWidth;
		memset(dst, 0, nScreenWidth*2);
	}

	BurnTransferCopy(DrvPalette + palettebank_vis * 256);

	if (topsecex == 0) BurnGunDrawTargets();

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	M6809NewFrame();
	timerNewFrame();

	{
		previous_coin = DrvInputs[0]; // coin

		DrvInputs[0] = DrvDips[3];       // coin (in3)
		DrvInputs[1] = DrvDips[0];       // (in0)
		DrvInputs[2] = DrvDips[1];       // (in1)
		DrvInputs[3] = DrvDips[2];       // start (in2)
		DrvInputs[4] = 0xff;			 // topsecex in4
		DrvInputs[5] = 0x00;			 // topsecex an1 (button1)

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
			DrvInputs[4] ^= (DrvJoy5[i] & 1) << i;
			DrvInputs[5] ^= (DrvJoy6[i] & 1) << i;
		}

		if (topsecex) { // for dial
			BurnTrackballConfig(0, AXIS_REVERSED, AXIS_NORMAL);
			BurnTrackballFrame(0, Analog[0], 0, 0, 0x1f);
			BurnTrackballUpdate(0);
		} else {
			BurnGunMakeInputs(0, Analog[0], Analog[1]);
		}

		if ((previous_coin ^ DrvInputs[0]) & 0x03) {
			M6809SetIRQLine(0, 0, CPU_IRQSTATUS_ACK);
		}
	}

	if (pBurnDraw) BurnTransferClear();

	INT32 nInterleave = 260;
	INT32 nCyclesTotal[2] = { 1622400 / 60, 3244800 / 2 / 60 };
	INT32 nCyclesDone[2] = { nExtraCycles[0], nExtraCycles[1] };

	timerIdle(nExtraCycles[0]);
	M6809Idle(0, nExtraCycles[0]);

	lastline = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		scanline = i;

		M6809Open(0);

		if (scanline == nScreenHeight) { // vblank (topsecex 236, others 240)
			partial_update();

			if (pBurnDraw) {
				BurnDrvRedraw();
			}


			firq_vblank = 1;
			update_firq();

			if (topsecex == 0) {
				INT32 x = (BurnGunReturnX(0) * 320) / 0x100;
				INT32 y = (BurnGunReturnY(0) * 240) / 0x100;

				INT32 time = cycles_to(x, y) - ONE_LINE * 6;

				beam_firq_count = 0;
				beam_timer.start(time, x, 1, 0);
			}
		}

		CPU_RUN(0, M6809);
		// Timer is updated through the M6809's callback
		M6809Close();

		M6809Open(1);
		if (i == nScreenHeight) {
			M6809SetIRQLine(0, CPU_IRQSTATUS_ACK);
		}
		CPU_RUN(1, M6809);
		M6809Close();
	}

	//bprintf(0, _T("cpu/timer:  %d   %d\n"), nCyclesDone[0], timerTotalCycles());

	nExtraCycles[0] = nCyclesDone[0] - nCyclesTotal[0];
	nExtraCycles[1] = nCyclesDone[1] - nCyclesTotal[1];

	if (pBurnSoundOut) {
		exidy440_update(pBurnSoundOut, nBurnSoundLen);
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	if (pnMin) {
		*pnMin = 0x029722;
	}

	if (nAction & ACB_VOLATILE) {
		ScanVar(AllRam, RamEnd - AllRam, "All Ram");

		M6809Scan(nAction);

		if (topsecex) {
			BurnTrackballScan();
		} else {
			BurnGunScan();
		}

		timerScan();

		exidy440_scan(nAction, pnMin);

		SCAN_VAR(mainbank);
		SCAN_VAR(palettebank);
		SCAN_VAR(palettebank_vis);
		SCAN_VAR(vram_scanline);
		SCAN_VAR(firq_select);
		SCAN_VAR(firq_enable);
		SCAN_VAR(firq_beam);
		SCAN_VAR(firq_vblank);
		SCAN_VAR(beam_firq_count);
		SCAN_VAR(topsecex_yscroll);
		SCAN_VAR(latched_x);
		SCAN_VAR(previous_coin);
		SCAN_VAR(showdown_bank_select);
		SCAN_VAR(showdown_bank_offset);

		SCAN_VAR(nExtraCycles);
	}

	if (nAction & ACB_WRITE)
	{
		M6809Open(0);
		set_palette_bank();
		M6809Close();
	}

	if (nAction & ACB_NVRAM)
	{
		ScanVar(DrvNVRAM, 0x2000, "NVRAM");
	}

	return 0;
}

// Crossbow (version 2.0)

static struct BurnRomInfo crossbowRomDesc[] = {
	{ "xbl-2.1a",				0x2000, 0xbd53ac46, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "xbl-2.3a",				0x2000, 0x703e1376, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "xbl-2.4a",				0x2000, 0x52c5daa1, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "xbl-2.6a",				0x2000, 0xf42a68f7, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "xbl-1.1e",				0x2000, 0x2834258e, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "xbl-1.3e",				0x2000, 0x587b186c, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "xbl-1.4e",				0x2000, 0x23fbfa8e, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "xbl-1.6e",				0x2000, 0xa3ebcc92, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "xbl-1.7e",				0x2000, 0x945b3a68, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "xbl-1.8e",				0x2000, 0x0d1c5d24, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "xbl-1.10e",				0x2000, 0xca30788b, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "xbl-1.11e",				0x2000, 0x6661c5ee, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "xbl-1.1d",				0x2000, 0xa1416191, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "xbl-1.3d",				0x2000, 0x7322b5e1, 1 | BRF_PRG | BRF_ESS }, // 13
	{ "xbl-1.4d",				0x2000, 0x425d51ef, 1 | BRF_PRG | BRF_ESS }, // 14
	{ "xbl-1.6d",				0x2000, 0xc923c9f5, 1 | BRF_PRG | BRF_ESS }, // 15
	{ "xbl-1.7d",				0x2000, 0x46cdf117, 1 | BRF_PRG | BRF_ESS }, // 16
	{ "xbl-1.8d",				0x2000, 0x62bad9b6, 1 | BRF_PRG | BRF_ESS }, // 17
	{ "xbl-1.10d",				0x2000, 0xd4aaa382, 1 | BRF_PRG | BRF_ESS }, // 18
	{ "xbl-1.11d",				0x2000, 0xefc77790, 1 | BRF_PRG | BRF_ESS }, // 19
	{ "xbl-1.1c",				0x2000, 0xdbbd35cb, 1 | BRF_PRG | BRF_ESS }, // 20
	{ "xbl-1.3c",				0x2000, 0xf011f98d, 1 | BRF_PRG | BRF_ESS }, // 21
	{ "xbl-1.4c",				0x2000, 0x1257b947, 1 | BRF_PRG | BRF_ESS }, // 22
	{ "xbl-1.6c",				0x2000, 0x48da9081, 1 | BRF_PRG | BRF_ESS }, // 23
	{ "xbl-1.7c",				0x2000, 0x8d4d4855, 1 | BRF_PRG | BRF_ESS }, // 24
	{ "xbl-1.8c",				0x2000, 0x4c52b85a, 1 | BRF_PRG | BRF_ESS }, // 25
	{ "xbl-1.10c",				0x2000, 0x5986130b, 1 | BRF_PRG | BRF_ESS }, // 26
	{ "xbl-1.11c",				0x2000, 0x163a6ae0, 1 | BRF_PRG | BRF_ESS }, // 27
	{ "xbl-1.1b",				0x2000, 0x36ea0269, 1 | BRF_PRG | BRF_ESS }, // 28
	{ "xbl-1.3b",				0x2000, 0x4a03c2c9, 1 | BRF_PRG | BRF_ESS }, // 29
	{ "xbl-1.4b",				0x2000, 0x7e21c624, 1 | BRF_PRG | BRF_ESS }, // 30

	{ "xba-11.1h",				0x2000, 0x1b61d0c1, 2 | BRF_PRG | BRF_ESS }, // 31 M6809 #1 Code

	{ "xba-1.2k",				0x2000, 0xb6e57685, 3 | BRF_SND },           // 32 Samples
	{ "xba-1.2l",				0x2000, 0x2c24cb35, 3 | BRF_SND },           // 33
	{ "xba-1.2m",				0x2000, 0xf3a4f2be, 3 | BRF_SND },           // 34
	{ "xba-1.2n",				0x2000, 0x15cf362d, 3 | BRF_SND },           // 35
	{ "xba-1.2p",				0x2000, 0x56f53af9, 3 | BRF_SND },           // 36
	{ "xba-1.2r",				0x2000, 0x3d8277b0, 3 | BRF_SND },           // 37
	{ "xba-1.2s",				0x2000, 0x14dd8993, 3 | BRF_SND },           // 38
	{ "xba-1.2t",				0x2000, 0xdfa783e4, 3 | BRF_SND },           // 39
	{ "xba-1.1k",				0x2000, 0x4f01f9e6, 3 | BRF_SND },           // 40
	{ "xba-1.1l",				0x2000, 0xfb119acf, 3 | BRF_SND },           // 41
	{ "xba-1.1m",				0x2000, 0x18d097ac, 3 | BRF_SND },           // 42
	{ "xba-1.1n",				0x2000, 0x2e855698, 3 | BRF_SND },           // 43
	{ "xba-1.1p",				0x2000, 0x788bfac6, 3 | BRF_SND },           // 44
	{ "xba-1.1r",				0x2000, 0xb8ec43b3, 3 | BRF_SND },           // 45
	{ "xba-1.1s",				0x2000, 0xc9ead134, 3 | BRF_SND },           // 46
	{ "xba-1.1t",				0x2000, 0x5f41c282, 3 | BRF_SND },           // 47

	{ "xbl.12h",				0x0100, 0x375c8bfc, 0 | BRF_OPT },           // 48 PROMs
	{ "xbl.9h",					0x0100, 0x2e7d5562, 0 | BRF_OPT },           // 49
	{ "xbl.2h",					0x0100, 0xb078c1e4, 0 | BRF_OPT },           // 50
	{ "xml-3k_mmi_6331.bin",	0x0020, 0xafa289d1, 0 | BRF_OPT },           // 51
	{ "xbl.4k",					0x0100, 0x31a9549c, 0 | BRF_OPT },           // 52
	{ "xbl.5k",					0x0100, 0x1379bb2a, 0 | BRF_OPT },           // 53
	{ "xbl.6k",					0x0100, 0x588969f7, 0 | BRF_OPT },           // 54
	{ "xbl.7k",					0x0100, 0xeda360b8, 0 | BRF_OPT },           // 55
	{ "xbl.8k",					0x0100, 0x9d434cb1, 0 | BRF_OPT },           // 56
};

STD_ROM_PICK(crossbow)
STD_ROM_FN(crossbow)

static INT32 CrossbowInit()
{
	crossbow = 1;
	return DrvInit(0x10000);
}

struct BurnDriver BurnDrvCrossbow = {
	"crossbow", NULL, NULL, NULL, "1983",
	"Crossbow (version 2.0)\0", NULL, "Exidy", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, crossbowRomInfo, crossbowRomName, NULL, NULL, NULL, NULL, CrossbowInputInfo, CrossbowDIPInfo,
	CrossbowInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	320, 240, 4, 3
};


// Cheyenne (version 1.0)

static struct BurnRomInfo cheyenneRomDesc[] = {
	{ "cyl-1.1a",				0x2000, 0x504c3fa6, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "cyl-1.3a",				0x2000, 0x09b7903b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "cyl-1.4a",				0x2000, 0xb708646b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "cyl-1.6a",				0x2000, 0x5d1e708d, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "cyl-1.1e",				0x2000, 0x8778e317, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "cyl-1.3e",				0x2000, 0xc8a9ca1b, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "cyl-1.4e",				0x2000, 0x86c4125a, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "cyl-1.6e",				0x2000, 0x51f4f060, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "cyl-1.7e",				0x2000, 0x4924d0c1, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "cyl-1.8e",				0x2000, 0x5c7c4dd7, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "cyl-1.10e",				0x2000, 0x57232888, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "cyl-1.11e",				0x2000, 0x2a767252, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "cyl-1.1d",				0x2000, 0xcd590e99, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "cyl-1.3d",				0x2000, 0x1fddccdb, 1 | BRF_PRG | BRF_ESS }, // 13
	{ "cyl-1.4d",				0x2000, 0x6c5ee6d7, 1 | BRF_PRG | BRF_ESS }, // 14
	{ "cyl-1.6d",				0x2000, 0x0e7c16c2, 1 | BRF_PRG | BRF_ESS }, // 15
	{ "cyl-1.7d",				0x2000, 0xabe11728, 1 | BRF_PRG | BRF_ESS }, // 16
	{ "cyl-1.8d",				0x2000, 0x95bb9a72, 1 | BRF_PRG | BRF_ESS }, // 17
	{ "cyl-1.10d",				0x2000, 0x5bc251be, 1 | BRF_PRG | BRF_ESS }, // 18
	{ "cyl-1.7c",				0x2000, 0xe9f6ce96, 1 | BRF_PRG | BRF_ESS }, // 19
	{ "cyl-1.8c",				0x2000, 0xcb3f8e9e, 1 | BRF_PRG | BRF_ESS }, // 20
	{ "cyl-1.10c",				0x2000, 0x49f90633, 1 | BRF_PRG | BRF_ESS }, // 21
	{ "cyl-1.11c",				0x2000, 0x70b69cf1, 1 | BRF_PRG | BRF_ESS }, // 22
	{ "cyl-1.1b",				0x2000, 0xc372e018, 1 | BRF_PRG | BRF_ESS }, // 23
	{ "cyl-1.3b",				0x2000, 0x6a583feb, 1 | BRF_PRG | BRF_ESS }, // 24
	{ "cyl-1.4b",				0x2000, 0x670e127d, 1 | BRF_PRG | BRF_ESS }, // 25
	{ "cyl-1.6b",				0x2000, 0xed245268, 1 | BRF_PRG | BRF_ESS }, // 26
	{ "cyl-1.7b",				0x2000, 0xdcc56d6c, 1 | BRF_PRG | BRF_ESS }, // 27
	{ "cyl-1.8b",				0x2000, 0xc0653d3e, 1 | BRF_PRG | BRF_ESS }, // 28
	{ "cyl-1.10b",				0x2000, 0x7fc67d19, 1 | BRF_PRG | BRF_ESS }, // 29

	{ "cya-1.1h",				0x2000, 0x5aed3d8c, 2 | BRF_PRG | BRF_ESS }, // 30 M6809 #1 Code

	{ "cya-1.2k",				0x2000, 0xdc2b716d, 3 | BRF_SND },           // 31 Samples
	{ "cya-1.2l",				0x2000, 0x091ad047, 3 | BRF_SND },           // 32
	{ "cya-1.2m",				0x2000, 0x59085362, 3 | BRF_SND },           // 33
	{ "cya-1.2n",				0x2000, 0x9c2e23c7, 3 | BRF_SND },           // 34
	{ "cya-1.2p",				0x2000, 0xeff18766, 3 | BRF_SND },           // 35
	{ "cya-1.2r",				0x2000, 0x8e730c98, 3 | BRF_SND },           // 36
	{ "cya-1.2s",				0x2000, 0x46515454, 3 | BRF_SND },           // 37
	{ "cya-1.2t",				0x2000, 0x5868fa84, 3 | BRF_SND },           // 38
	{ "cya-1.1k",				0x2000, 0x45a306a6, 3 | BRF_SND },           // 39
	{ "cya-1.1l",				0x2000, 0x3c7e2127, 3 | BRF_SND },           // 40
	{ "cya-1.1m",				0x2000, 0x39ddc9f7, 3 | BRF_SND },           // 41
	{ "cya-1.1n",				0x2000, 0x5fcee4fd, 3 | BRF_SND },           // 42
	{ "cya-1.1p",				0x2000, 0x81a4a876, 3 | BRF_SND },           // 43
	{ "cya-1.1r",				0x2000, 0xdfd84e73, 3 | BRF_SND },           // 44

	{ "xbl.12h",				0x0100, 0x375c8bfc, 0 | BRF_OPT },           // 45 PROMs
	{ "xbl.9h",					0x0100, 0x2e7d5562, 0 | BRF_OPT },           // 46
	{ "xbl.2h",					0x0100, 0xb078c1e4, 0 | BRF_OPT },           // 47
	{ "xml-3k_mmi_6331.bin",	0x0020, 0xafa289d1, 0 | BRF_OPT },           // 48
	{ "xbl.4k",					0x0100, 0x31a9549c, 0 | BRF_OPT },           // 49
	{ "xbl.5k",					0x0100, 0x1379bb2a, 0 | BRF_OPT },           // 50
	{ "xbl.6k",					0x0100, 0x588969f7, 0 | BRF_OPT },           // 51
	{ "xbl.7k",					0x0100, 0xeda360b8, 0 | BRF_OPT },           // 52
	{ "xbl.8k",					0x0100, 0x9d434cb1, 0 | BRF_OPT },           // 53
};

STD_ROM_PICK(cheyenne)
STD_ROM_FN(cheyenne)

static INT32 CheyenneInit()
{
	cheyenne = 1;

	return DrvInit(0x10000);
}

struct BurnDriver BurnDrvCheyenne = {
	"cheyenne", NULL, NULL, NULL, "1984",
	"Cheyenne (version 1.0)\0", NULL, "Exidy", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, cheyenneRomInfo, cheyenneRomName, NULL, NULL, NULL, NULL, CrossbowInputInfo, CheyenneDIPInfo,
	CheyenneInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 240, 4, 3
};


// Combat (version 3.0)

static struct BurnRomInfo combatRomDesc[] = {
	{ "1a",						0x2000, 0x159a573b, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "3a",						0x2000, 0x59ae51a7, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "4a",						0x2000, 0x95a1f3d0, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "6a",						0x2000, 0xaf3fef5f, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "6d",						0x2000, 0x43d3eb61, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "7d",						0x2000, 0xef31659c, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "8d",						0x2000, 0xfb29c5cd, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "10d",					0x2000, 0x2ca0eaa4, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "11d",					0x2000, 0xcc9f2001, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "1c",						0x2000, 0xb7b9c5ad, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "3c",						0x2000, 0xb700e6ec, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "4c",						0x2000, 0x89fc2b2d, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "6c",						0x2000, 0x6a8d0dcf, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "7c",						0x2000, 0x9df7172d, 1 | BRF_PRG | BRF_ESS }, // 13
	{ "8c",						0x2000, 0x63b2e4f3, 1 | BRF_PRG | BRF_ESS }, // 14
	{ "10c",					0x2000, 0x3b430adc, 1 | BRF_PRG | BRF_ESS }, // 15
	{ "11c",					0x2000, 0x04301032, 1 | BRF_PRG | BRF_ESS }, // 16
	{ "1b",						0x2000, 0x70e25cae, 1 | BRF_PRG | BRF_ESS }, // 17
	{ "3b",						0x2000, 0xd09d167e, 1 | BRF_PRG | BRF_ESS }, // 18
	{ "4b",						0x2000, 0xf46aba0d, 1 | BRF_PRG | BRF_ESS }, // 19
	{ "6b",						0x2000, 0x8eb46f40, 1 | BRF_PRG | BRF_ESS }, // 20
	{ "7b",						0x2000, 0x3be9b1bd, 1 | BRF_PRG | BRF_ESS }, // 21
	{ "8b",						0x2000, 0xae977f4c, 1 | BRF_PRG | BRF_ESS }, // 22
	{ "10b",					0x2000, 0x502da003, 1 | BRF_PRG | BRF_ESS }, // 23

	{ "1h",						0x2000, 0x8f3dd350, 2 | BRF_PRG | BRF_ESS }, // 24 M6809 #1 Code

	{ "2k",						0x2000, 0x1c9df8b5, 3 | BRF_SND },           // 25 Samples
	{ "2l",						0x2000, 0x6b733306, 3 | BRF_SND },           // 26
	{ "2m",						0x2000, 0xdc074733, 3 | BRF_SND },           // 27
	{ "2n",						0x2000, 0x7985867f, 3 | BRF_SND },           // 28
	{ "2p",						0x2000, 0x88684dcf, 3 | BRF_SND },           // 29
	{ "2r",						0x2000, 0x5857321e, 3 | BRF_SND },           // 30
	{ "2s",						0x2000, 0x371e5235, 3 | BRF_SND },           // 31
	{ "2t",						0x2000, 0x7ae65f05, 3 | BRF_SND },           // 32
	{ "1k",						0x2000, 0xf748ea87, 3 | BRF_SND },           // 33
	{ "xba-1.2s",				0x2000, 0x14dd8993, 3 | BRF_SND },           // 34
	{ "xba-1.1n",				0x2000, 0x2e855698, 3 | BRF_SND },           // 35
	{ "xba-1.1p",				0x2000, 0x788bfac6, 3 | BRF_SND },           // 36
	{ "xba-1.2l",				0x2000, 0x2c24cb35, 3 | BRF_SND },           // 37
	{ "xba-1.1t",				0x2000, 0x5f41c282, 3 | BRF_SND },           // 38

	{ "xbl.12h",				0x0100, 0x375c8bfc, 0 | BRF_OPT },           // 39 PROMs
	{ "xbl.9h",					0x0100, 0x2e7d5562, 0 | BRF_OPT },           // 40
	{ "xbl.2h",					0x0100, 0xb078c1e4, 0 | BRF_OPT },           // 41
	{ "xml-3k_mmi_6331.bin",	0x0020, 0xafa289d1, 0 | BRF_OPT },           // 42
	{ "xbl.4k",					0x0100, 0x31a9549c, 0 | BRF_OPT },           // 43
	{ "xbl.5k",					0x0100, 0x1379bb2a, 0 | BRF_OPT },           // 44
	{ "xbl.6k",					0x0100, 0x588969f7, 0 | BRF_OPT },           // 45
	{ "xbl.7k",					0x0100, 0xeda360b8, 0 | BRF_OPT },           // 46
	{ "xbl.8k",					0x0100, 0x9d434cb1, 0 | BRF_OPT },           // 47
};

STD_ROM_PICK(combat)
STD_ROM_FN(combat)

static INT32 CombatInit()
{
	return DrvInit(0x26000);
}

struct BurnDriver BurnDrvCombat = {
	"combat", NULL, NULL, NULL, "1985",
	"Combat (version 3.0)\0", NULL, "Exidy", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, combatRomInfo, combatRomName, NULL, NULL, NULL, NULL, CrossbowInputInfo, CombatDIPInfo,
	CombatInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 240, 4, 3
};


// Catch-22 (version 8.0)

static struct BurnRomInfo catch22RomDesc[] = {
	{ "22l-8_0.1a",				0x2000, 0x232e8723, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "22l-8_0.3a",				0x2000, 0xa94afce6, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "22l-8_0.4a",				0x2000, 0x0983ab83, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "22l-8_0.6a",				0x2000, 0x9084a23a, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "6d",						0x2000, 0x43d3eb61, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "7d",						0x2000, 0xef31659c, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "8d",						0x2000, 0xfb29c5cd, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "10d",					0x2000, 0x2ca0eaa4, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "11d",					0x2000, 0xcc9f2001, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "1c",						0x2000, 0xb7b9c5ad, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "3c",						0x2000, 0xb700e6ec, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "4c",						0x2000, 0x89fc2b2d, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "6c",						0x2000, 0x6a8d0dcf, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "7c",						0x2000, 0x9df7172d, 1 | BRF_PRG | BRF_ESS }, // 13
	{ "8c",						0x2000, 0x63b2e4f3, 1 | BRF_PRG | BRF_ESS }, // 14
	{ "10c",					0x2000, 0x3b430adc, 1 | BRF_PRG | BRF_ESS }, // 15
	{ "11c",					0x2000, 0x04301032, 1 | BRF_PRG | BRF_ESS }, // 16
	{ "1b",						0x2000, 0x70e25cae, 1 | BRF_PRG | BRF_ESS }, // 17
	{ "3b",						0x2000, 0xd09d167e, 1 | BRF_PRG | BRF_ESS }, // 18
	{ "4b",						0x2000, 0xf46aba0d, 1 | BRF_PRG | BRF_ESS }, // 19
	{ "6b",						0x2000, 0x8eb46f40, 1 | BRF_PRG | BRF_ESS }, // 20
	{ "7b",						0x2000, 0x3be9b1bd, 1 | BRF_PRG | BRF_ESS }, // 21
	{ "8b",						0x2000, 0xae977f4c, 1 | BRF_PRG | BRF_ESS }, // 22
	{ "10b",					0x2000, 0x502da003, 1 | BRF_PRG | BRF_ESS }, // 23

	{ "1h",						0x2000, 0x8f3dd350, 2 | BRF_PRG | BRF_ESS }, // 24 M6809 #1 Code

	{ "2k",						0x2000, 0x1c9df8b5, 3 | BRF_SND },           // 25 Samples
	{ "2l",						0x2000, 0x6b733306, 3 | BRF_SND },           // 26
	{ "2m",						0x2000, 0xdc074733, 3 | BRF_SND },           // 27
	{ "2n",						0x2000, 0x7985867f, 3 | BRF_SND },           // 28
	{ "2p",						0x2000, 0x88684dcf, 3 | BRF_SND },           // 29
	{ "2r",						0x2000, 0x5857321e, 3 | BRF_SND },           // 30
	{ "2s",						0x2000, 0x371e5235, 3 | BRF_SND },           // 31
	{ "2t",						0x2000, 0x7ae65f05, 3 | BRF_SND },           // 32
	{ "1k",						0x2000, 0xf748ea87, 3 | BRF_SND },           // 33
	{ "xba-1.2s",				0x2000, 0x14dd8993, 3 | BRF_SND },           // 34
	{ "xba-1.1n",				0x2000, 0x2e855698, 3 | BRF_SND },           // 35
	{ "xba-1.1p",				0x2000, 0x788bfac6, 3 | BRF_SND },           // 36
	{ "xba-1.2l",				0x2000, 0x2c24cb35, 3 | BRF_SND },           // 37
	{ "xba-1.1t",				0x2000, 0x5f41c282, 3 | BRF_SND },           // 38

	{ "xbl.12h",				0x0100, 0x375c8bfc, 0 | BRF_OPT },           // 39 PROMs
	{ "xbl.9h",					0x0100, 0x2e7d5562, 0 | BRF_OPT },           // 40
	{ "xbl.2h",					0x0100, 0xb078c1e4, 0 | BRF_OPT },           // 41
	{ "xml-3k_mmi_6331.bin",	0x0020, 0xafa289d1, 0 | BRF_OPT },           // 42
	{ "xbl.4k",					0x0100, 0x31a9549c, 0 | BRF_OPT },           // 43
	{ "xbl.5k",					0x0100, 0x1379bb2a, 0 | BRF_OPT },           // 44
	{ "xbl.6k",					0x0100, 0x588969f7, 0 | BRF_OPT },           // 45
	{ "xbl.7k",					0x0100, 0xeda360b8, 0 | BRF_OPT },           // 46
	{ "xbl.8k",					0x0100, 0x9d434cb1, 0 | BRF_OPT },           // 47
};

STD_ROM_PICK(catch22)
STD_ROM_FN(catch22)

static INT32 Catch22Init()
{
	return DrvInit(0x26000);
}

struct BurnDriver BurnDrvCatch22 = {
	"catch22", "combat", NULL, NULL, "1985",
	"Catch-22 (version 8.0)\0", NULL, "Exidy", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, catch22RomInfo, catch22RomName, NULL, NULL, NULL, NULL, CrossbowInputInfo, Catch22DIPInfo,
	Catch22Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 240, 4, 3
};


// Crackshot (version 2.0)

static struct BurnRomInfo crackshtRomDesc[] = {
	{ "csl2.1a",				0x2000, 0x16fd0171, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "csl2.3a",				0x2000, 0x906f3209, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "csl2.4a",				0x2000, 0x9996d2bf, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "csl2.6a",				0x2000, 0xc8d6e945, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "csl2.11d",				0x2000, 0xb1173dd3, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "csl2.1c",				0x2000, 0xe44975a7, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "csl2.3c",				0x2000, 0xa3ab11e9, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "csl2.4c",				0x2000, 0x89266302, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "csl2.6c",				0x2000, 0xbb0f8d32, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "csl2.7c",				0x2000, 0xe203ed0b, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "csl2.8c",				0x2000, 0x3e028a62, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "csl2.10c",				0x2000, 0xc5494f9f, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "csl2.11c",				0x2000, 0x0159bdcb, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "csl2.1b",				0x2000, 0x8adf33fc, 1 | BRF_PRG | BRF_ESS }, // 13
	{ "csl2.3b",				0x2000, 0x7561be69, 1 | BRF_PRG | BRF_ESS }, // 14
	{ "csl2.4b",				0x2000, 0x848e3aff, 1 | BRF_PRG | BRF_ESS }, // 15
	{ "csl2.6b",				0x2000, 0xd0fd87df, 1 | BRF_PRG | BRF_ESS }, // 16
	{ "csl2.7b",				0x2000, 0x7e0a6a31, 1 | BRF_PRG | BRF_ESS }, // 17
	{ "csl2.8b",				0x2000, 0xaf1c8cb8, 1 | BRF_PRG | BRF_ESS }, // 18
	{ "csl2.10b",				0x2000, 0x8a0d6ad0, 1 | BRF_PRG | BRF_ESS }, // 19

	{ "csa3.1h",				0x2000, 0x5ba8b4ac, 2 | BRF_PRG | BRF_ESS }, // 20 M6809 #1 Code

	{ "csa3.2k",				0x2000, 0x067a4f71, 3 | BRF_SND },           // 21 Samples
	{ "csa3.2l",				0x2000, 0x5716c59e, 3 | BRF_SND },           // 22
	{ "csa3.2m",				0x2000, 0xb3ff659b, 3 | BRF_SND },           // 23
	{ "csa3.2n",				0x2000, 0xa8968342, 3 | BRF_SND },           // 24
	{ "csa3.2p",				0x2000, 0x5db225b8, 3 | BRF_SND },           // 25
	{ "csa3.2r",				0x2000, 0xfda2669d, 3 | BRF_SND },           // 26
	{ "csa3.2s",				0x2000, 0xe8d2413f, 3 | BRF_SND },           // 27
	{ "csa3.2t",				0x2000, 0x841a1855, 3 | BRF_SND },           // 28
	{ "csa3.1k",				0x2000, 0x27dda69b, 3 | BRF_SND },           // 29
	{ "csa3.1l",				0x2000, 0x86eea479, 3 | BRF_SND },           // 30
	{ "csa3.1m",				0x2000, 0x2c24cb35, 3 | BRF_SND },           // 31
	{ "csa3.1n",				0x2000, 0xf3a4f2be, 3 | BRF_SND },           // 32
	{ "csa3.1p",				0x2000, 0x14dd8993, 3 | BRF_SND },           // 33
	{ "csa3.1r",				0x2000, 0xdfa783e4, 3 | BRF_SND },           // 34
	{ "csa3.1s",				0x2000, 0x18d097ac, 3 | BRF_SND },           // 35
	{ "csa3.1t",				0x2000, 0x5f41c282, 3 | BRF_SND },           // 36

	{ "xbl.12h",				0x0100, 0x375c8bfc, 0 | BRF_OPT },           // 37 PROMs
	{ "xbl.9h",					0x0100, 0x2e7d5562, 0 | BRF_OPT },           // 38
	{ "xbl.2h",					0x0100, 0xb078c1e4, 0 | BRF_OPT },           // 39
	{ "xml-3k_mmi_6331.bin",	0x0020, 0xafa289d1, 0 | BRF_OPT },           // 40
	{ "xbl.4k",					0x0100, 0x31a9549c, 0 | BRF_OPT },           // 41
	{ "xbl.5k",					0x0100, 0x1379bb2a, 0 | BRF_OPT },           // 42
	{ "xbl.6k",					0x0100, 0x588969f7, 0 | BRF_OPT },           // 43
	{ "xbl.7k",					0x0100, 0xeda360b8, 0 | BRF_OPT },           // 44
	{ "xbl.8k",					0x0100, 0x9d434cb1, 0 | BRF_OPT },           // 45
};

STD_ROM_PICK(cracksht)
STD_ROM_FN(cracksht)

static INT32 CrackshtInit()
{
	return DrvInit(0x2e000);
}

struct BurnDriver BurnDrvCracksht = {
	"cracksht", NULL, NULL, NULL, "1985",
	"Crackshot (version 2.0)\0", NULL, "Exidy", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, crackshtRomInfo, crackshtRomName, NULL, NULL, NULL, NULL, CrossbowInputInfo, CrackshtDIPInfo,
	CrackshtInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 240, 4, 3
};


// Clay Pigeon (version 2.0)

static struct BurnRomInfo claypignRomDesc[] = {
	{ "claypige.1a",			0x2000, 0x446d7004, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "claypige.3a",			0x2000, 0xdf39701b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "claypige.4a",			0x2000, 0xf205afb8, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "claypige.6a",			0x2000, 0x97c36c6c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "claypige.10c",			0x2000, 0x3d2957cd, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "claypige.11c",			0x2000, 0xe162a3af, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "claypige.1b",			0x2000, 0x90f1e534, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "claypige.3b",			0x2000, 0x150c5993, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "claypige.4b",			0x2000, 0xdabb99fb, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "claypige.6b",			0x2000, 0xc3b86d26, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "claypige.7b",			0x2000, 0x6140b026, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "claypige.8b",			0x2000, 0xd0f9d170, 1 | BRF_PRG | BRF_ESS }, // 11

	{ "claypige.h1",			0x2000, 0x9eedc68d, 2 | BRF_PRG | BRF_ESS }, // 12 M6809 #1 Code

	{ "claypige.k2",			0x2000, 0x0dd93c6c, 3 | BRF_SND },           // 13 Samples
	{ "claypige.l2",			0x2000, 0xe1d67c42, 3 | BRF_SND },           // 14
	{ "claypige.m2",			0x2000, 0xb56d8bd5, 3 | BRF_SND },           // 15
	{ "claypige.n2",			0x2000, 0x9e381cb5, 3 | BRF_SND },           // 16
	{ "xba-1.2l",				0x2000, 0x2c24cb35, 3 | BRF_SND },           // 17
	{ "xba-1.2k",				0x2000, 0xb6e57685, 3 | BRF_SND },           // 18
	{ "xba-1.1m",				0x2000, 0x18d097ac, 3 | BRF_SND },           // 19
	{ "xba-1.1t",				0x2000, 0x5f41c282, 3 | BRF_SND },           // 20
	{ "claypige.k1",			0x2000, 0x07f12d18, 3 | BRF_SND },           // 21
	{ "claypige.l1",			0x2000, 0xf448eb4f, 3 | BRF_SND },           // 22
	{ "claypige.m1",			0x2000, 0x36865f5b, 3 | BRF_SND },           // 23

	{ "xbl.12h",				0x0100, 0x375c8bfc, 0 | BRF_OPT },           // 24 PROMs
	{ "xbl.9h",					0x0100, 0x2e7d5562, 0 | BRF_OPT },           // 25
	{ "xbl.2h",					0x0100, 0xb078c1e4, 0 | BRF_OPT },           // 26
	{ "xml-3k_mmi_6331.bin",	0x0020, 0xafa289d1, 0 | BRF_OPT },           // 27
	{ "xbl.4k",					0x0100, 0x31a9549c, 0 | BRF_OPT },           // 28
	{ "xbl.5k",					0x0100, 0x1379bb2a, 0 | BRF_OPT },           // 29
	{ "xbl.6k",					0x0100, 0x588969f7, 0 | BRF_OPT },           // 30
	{ "xbl.7k",					0x0100, 0xeda360b8, 0 | BRF_OPT },           // 31
	{ "xbl.8k",					0x0100, 0x9d434cb1, 0 | BRF_OPT },           // 32
};

STD_ROM_PICK(claypign)
STD_ROM_FN(claypign)

static INT32 ClaypignInit()
{
	claypign = 1;

	return DrvInit(0x3c000);
}

struct BurnDriver BurnDrvClaypign = {
	"claypign", NULL, NULL, NULL, "1986",
	"Clay Pigeon (version 2.0)\0", NULL, "Exidy", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, claypignRomInfo, claypignRomName, NULL, NULL, NULL, NULL, CrossbowInputInfo, ClaypignDIPInfo,
	ClaypignInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 240, 4, 3
};


// Chiller (version 3.0)

static struct BurnRomInfo chillerRomDesc[] = {
	{ "chl3.1a",				0x2000, 0x996ad02e, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "chl3.3a",				0x2000, 0x17e6f904, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "chl3.4a",				0x2000, 0xf30d6e32, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "chl3.6a",				0x2000, 0xf64fa8fe, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "chl3.1d",				0x2000, 0xce4aa4b0, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "chl3.3d",				0x2000, 0xa234952e, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "chl3.4d",				0x2000, 0x645dbae9, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "chl3.6d",				0x2000, 0x440a5cd7, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "chl3.7d",				0x2000, 0x062a541f, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "chl3.8d",				0x2000, 0x31ff8f48, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "chl3.10d",				0x2000, 0x5bceb965, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "chl3.11d",				0x2000, 0xe16b5db3, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "chl3.1c",				0x2000, 0xebfd29e8, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "chl3.3c",				0x2000, 0xa04261e5, 1 | BRF_PRG | BRF_ESS }, // 13
	{ "chl3.4c",				0x2000, 0x6fcbb15b, 1 | BRF_PRG | BRF_ESS }, // 14
	{ "chl3.6c",				0x2000, 0xbd0e0689, 1 | BRF_PRG | BRF_ESS }, // 15
	{ "chl3.7c",				0x2000, 0x2715571e, 1 | BRF_PRG | BRF_ESS }, // 16
	{ "chl3.8c",				0x2000, 0x364d9450, 1 | BRF_PRG | BRF_ESS }, // 17
	{ "chl3.10c",				0x2000, 0x13180106, 1 | BRF_PRG | BRF_ESS }, // 18
	{ "chl3.11c",				0x2000, 0x4a7ffe6f, 1 | BRF_PRG | BRF_ESS }, // 19
	{ "chl3.1b",				0x2000, 0x20c19bb6, 1 | BRF_PRG | BRF_ESS }, // 20
	{ "chl3.3b",				0x2000, 0xe1f07ace, 1 | BRF_PRG | BRF_ESS }, // 21
	{ "chl3.4b",				0x2000, 0x140d95db, 1 | BRF_PRG | BRF_ESS }, // 22
	{ "chl3.6b",				0x2000, 0xfaaf7cc8, 1 | BRF_PRG | BRF_ESS }, // 23
	{ "chl3.7b",				0x2000, 0x5512b7e6, 1 | BRF_PRG | BRF_ESS }, // 24
	{ "chl3.8b",				0x2000, 0x6172b12f, 1 | BRF_PRG | BRF_ESS }, // 25
	{ "chl3.10b",				0x2000, 0x5d15342a, 1 | BRF_PRG | BRF_ESS }, // 26

	{ "cha3.1h",				0x1000, 0xb195cbba, 2 | BRF_PRG | BRF_ESS }, // 27 M6809 #1 Code

	{ "cha3.2k",				0x2000, 0x814a1c6e, 3 | BRF_SND },           // 28 Samples
	{ "cha3.2l",				0x2000, 0xb326007f, 3 | BRF_SND },           // 29
	{ "cha3.2m",				0x2000, 0x11075e8c, 3 | BRF_SND },           // 30
	{ "cha3.2n",				0x2000, 0x8c3f6184, 3 | BRF_SND },           // 31
	{ "cha3.2p",				0x2000, 0x3a8b4d0f, 3 | BRF_SND },           // 32
	{ "cha3.2r",				0x2000, 0xfc6c4e00, 3 | BRF_SND },           // 33
	{ "cha3.2s",				0x2000, 0x2440d5f3, 3 | BRF_SND },           // 34
	{ "cha3.2t",				0x2000, 0x9b2ce556, 3 | BRF_SND },           // 35
	{ "cha3.1k",				0x2000, 0x27f86fab, 3 | BRF_SND },           // 36
	{ "cha3.1l",				0x2000, 0x581dfde7, 3 | BRF_SND },           // 37
	{ "cha3.1m",				0x2000, 0x36d47696, 3 | BRF_SND },           // 38
	{ "cha3.1n",				0x2000, 0xce47bffe, 3 | BRF_SND },           // 39
	{ "cha3.1p",				0x2000, 0x788bfac6, 3 | BRF_SND },           // 40
	{ "cha3.1r",				0x2000, 0xb8ec43b3, 3 | BRF_SND },           // 41
	{ "cha3.1s",				0x2000, 0x5f41c282, 3 | BRF_SND },           // 42
	{ "cha3.1t",				0x2000, 0x3a3a48af, 3 | BRF_SND },           // 43

	{ "xbl.12h",				0x0100, 0x375c8bfc, 0 | BRF_OPT },           // 44 PROMs
	{ "xbl.9h",					0x0100, 0x2e7d5562, 0 | BRF_OPT },           // 45
	{ "xbl.2h",					0x0100, 0xb078c1e4, 0 | BRF_OPT },           // 46
	{ "xml-3k_mmi_6331.bin",	0x0020, 0xafa289d1, 0 | BRF_OPT },           // 47
	{ "xbl.4k",					0x0100, 0x31a9549c, 0 | BRF_OPT },           // 48
	{ "xbl.5k",					0x0100, 0x1379bb2a, 0 | BRF_OPT },           // 49
	{ "xbl.6k",					0x0100, 0x588969f7, 0 | BRF_OPT },           // 50
	{ "xbl.7k",					0x0100, 0xeda360b8, 0 | BRF_OPT },           // 51
	{ "xbl.8k",					0x0100, 0x9d434cb1, 0 | BRF_OPT },           // 52
};

STD_ROM_PICK(chiller)
STD_ROM_FN(chiller)

static INT32 ChillerInit()
{
	return DrvInit(0x20000);
}

struct BurnDriver BurnDrvChiller = {
	"chiller", NULL, NULL, NULL, "1986",
	"Chiller (version 3.0)\0", NULL, "Exidy", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, chillerRomInfo, chillerRomName, NULL, NULL, NULL, NULL, CrossbowInputInfo, ChillerDIPInfo,
	ChillerInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 240, 4, 3
};


// Top Secret (Exidy) (version 1.0)

static struct BurnRomInfo topsecexRomDesc[] = {
	{ "tsl1.a1",				0x2000, 0x30ff2142, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "tsl1.a3",				0x2000, 0x9295e5b7, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tsl1.a4",				0x2000, 0x402abca4, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "tsl1.a6",				0x2000, 0x66eac7d8, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "tsl1.e3",				0x2000, 0xf5b291fc, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "tsl1.e4",				0x2000, 0xb6c659ae, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "tsl1.e6",				0x2000, 0xcf457523, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "tsl1.e7",				0x2000, 0x5a29304c, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "tsl1.e8",				0x2000, 0x0750893b, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "tsl1.e10",				0x2000, 0xfb87a723, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "tsl1.e11",				0x2000, 0xecf78fac, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "tsl1.d1",				0x2000, 0x3a316cbe, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "tsl1.d3",				0x2000, 0x58408a5f, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "tsl1.d4",				0x2000, 0xc3c85c13, 1 | BRF_PRG | BRF_ESS }, // 13
	{ "tsl1.d6",				0x2000, 0xf26a2864, 1 | BRF_PRG | BRF_ESS }, // 14
	{ "tsl1.d7",				0x2000, 0x53195dc6, 1 | BRF_PRG | BRF_ESS }, // 15
	{ "tsl1.d8",				0x2000, 0x4fcfb3c8, 1 | BRF_PRG | BRF_ESS }, // 16
	{ "tsl1.d10",				0x2000, 0x6e20af8d, 1 | BRF_PRG | BRF_ESS }, // 17
	{ "tsl1.d11",				0x2000, 0x58c670e7, 1 | BRF_PRG | BRF_ESS }, // 18
	{ "tsl1.c1",				0x2000, 0x630521f8, 1 | BRF_PRG | BRF_ESS }, // 19
	{ "tsl1.c3",				0x2000, 0xd0d7d908, 1 | BRF_PRG | BRF_ESS }, // 20
	{ "tsl1.c4",				0x2000, 0xdc2193c4, 1 | BRF_PRG | BRF_ESS }, // 21
	{ "tsl1.c6",				0x2000, 0xde417d5f, 1 | BRF_PRG | BRF_ESS }, // 22
	{ "tsl1.c7",				0x2000, 0xd75708c3, 1 | BRF_PRG | BRF_ESS }, // 23
	{ "tsl1.c8",				0x2000, 0x69f639fd, 1 | BRF_PRG | BRF_ESS }, // 24
	{ "tsl1.c10",				0x2000, 0x5977e312, 1 | BRF_PRG | BRF_ESS }, // 25
	{ "tsl1.c11",				0x2000, 0x07a6a534, 1 | BRF_PRG | BRF_ESS }, // 26
	{ "tsl1.b1",				0x2000, 0x771ec128, 1 | BRF_PRG | BRF_ESS }, // 27
	{ "tsl1.b3",				0x2000, 0xe57af802, 1 | BRF_PRG | BRF_ESS }, // 28
	{ "tsl1.b4",				0x2000, 0x7d63fe09, 1 | BRF_PRG | BRF_ESS }, // 29
	{ "tsl1.b6",				0x2000, 0xe6c85d95, 1 | BRF_PRG | BRF_ESS }, // 30
	{ "tsl1.b7",				0x2000, 0xbabb7e24, 1 | BRF_PRG | BRF_ESS }, // 31
	{ "tsl1.b8",				0x2000, 0xcc770802, 1 | BRF_PRG | BRF_ESS }, // 32
	{ "tsl1.b10",				0x2000, 0x079d0a1d, 1 | BRF_PRG | BRF_ESS }, // 33

	{ "tsa1.1h",				0x2000, 0x35a1dd40, 2 | BRF_PRG | BRF_ESS }, // 34 M6809 #1 Code

	{ "tsa1.2k",				0x2000, 0xc0b7c8f9, 3 | BRF_SND },           // 35 Samples
	{ "tsa1.2l",				0x2000, 0xd46f2f23, 3 | BRF_SND },           // 36
	{ "tsa1.2m",				0x2000, 0x04722ee4, 3 | BRF_SND },           // 37
	{ "tsa1.2n",				0x2000, 0x266348a2, 3 | BRF_SND },           // 38
	{ "tsa1.2p",				0x2000, 0x33491a21, 3 | BRF_SND },           // 39
	{ "tsa1.2r",				0x2000, 0x669fb97a, 3 | BRF_SND },           // 40
	{ "tsa1.2s",				0x2000, 0xa9167bc2, 3 | BRF_SND },           // 41
	{ "tsa1.2t",				0x2000, 0x26bcd7ff, 3 | BRF_SND },           // 42
	{ "tsa1.1k",				0x2000, 0x60e9035d, 3 | BRF_SND },           // 43
	{ "tsa1.1l",				0x2000, 0x7de3bfa7, 3 | BRF_SND },           // 44
	{ "tsa1.1m",				0x2000, 0x77a86aef, 3 | BRF_SND },           // 45
	{ "tsa1.1n",				0x2000, 0x4ffc26a7, 3 | BRF_SND },           // 46
	{ "tsa1.1p",				0x2000, 0xd0c699aa, 3 | BRF_SND },           // 47
	{ "tsa1.1r",				0x2000, 0x753f0a5f, 3 | BRF_SND },           // 48
	{ "tsa1.1s",				0x2000, 0x745f9340, 3 | BRF_SND },           // 49
	{ "tsa1.1t",				0x2000, 0x0e74b7a6, 3 | BRF_SND },           // 50

	{ "77l.12h",				0x0100, 0xef54343e, 0 | BRF_OPT },           // 51 PROMs
	{ "xbl.9h",					0x0100, 0x2e7d5562, 0 | BRF_OPT },           // 52
	{ "xbl.2h",					0x0100, 0xb078c1e4, 0 | BRF_OPT },           // 53
	{ "xml-3k_mmi_6331.bin",	0x0020, 0xafa289d1, 0 | BRF_OPT },           // 54
	{ "xbl.4k",					0x0100, 0x31a9549c, 0 | BRF_OPT },           // 55
	{ "xbl.5k",					0x0100, 0x1379bb2a, 0 | BRF_OPT },           // 56
	{ "xbl.6k",					0x0100, 0x588969f7, 0 | BRF_OPT },           // 57
	{ "xbl.7k",					0x0100, 0xeda360b8, 0 | BRF_OPT },           // 58
	{ "xbl.8k",					0x0100, 0x9d434cb1, 0 | BRF_OPT },           // 59
};

STD_ROM_PICK(topsecex)
STD_ROM_FN(topsecex)

static INT32 TopsecexInit()
{
	topsecex = 1;

	return DrvInit(0x12000);
}

struct BurnDriver BurnDrvTopsecex = {
	"topsecex", NULL, NULL, NULL, "1986",
	"Top Secret (Exidy) (version 1.0)\0", NULL, "Exidy", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_MISC_PRE90S, GBF_VERSHOOT | GBF_ACTION, 0,
	NULL, topsecexRomInfo, topsecexRomName, NULL, NULL, NULL, NULL, TopsecexInputInfo, TopsecexDIPInfo,
	TopsecexInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 238, 4, 3
};


// Hit 'n Miss (version 3.0)

static struct BurnRomInfo hitnmissRomDesc[] = {
	{ "hml3.a1",				0x2000, 0xd79ae18e, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "hml3.a3",				0x2000, 0x61baf38b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "hml3.a4",				0x2000, 0x60ca260b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "hml3.a6",				0x2000, 0x073305d8, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "hml3.d6",				0x2000, 0x79578952, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "hml3.d7",				0x2000, 0x8043b78e, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "hml3.d8",				0x2000, 0xa6494e2e, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "hml3.d10",				0x2000, 0x0810cc84, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "hml3.d11",				0x2000, 0x9f5c3799, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "hml3.c1",				0x2000, 0x6606d5a8, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "hml3.c3",				0x2000, 0xf6b12e48, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "hml3.c4",				0x2000, 0xe5031d44, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "hml3.c6",				0x2000, 0x1b0f2f28, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "hml3.c7",				0x2000, 0x44920233, 1 | BRF_PRG | BRF_ESS }, // 13
	{ "hml3.c8",				0x2000, 0x7db26fad, 1 | BRF_PRG | BRF_ESS }, // 14
	{ "hml3.c10",				0x2000, 0xb8f99481, 1 | BRF_PRG | BRF_ESS }, // 15
	{ "hml3.c11",				0x2000, 0xc2a0d170, 1 | BRF_PRG | BRF_ESS }, // 16
	{ "hml3.b1",				0x2000, 0x945cb27c, 1 | BRF_PRG | BRF_ESS }, // 17
	{ "hml3.b3",				0x2000, 0x3f022689, 1 | BRF_PRG | BRF_ESS }, // 18
	{ "hml3.b4",				0x2000, 0xd63fd250, 1 | BRF_PRG | BRF_ESS }, // 19
	{ "hml3.b6",				0x2000, 0xafc89eed, 1 | BRF_PRG | BRF_ESS }, // 20
	{ "hml3.b7",				0x2000, 0xf3a12a58, 1 | BRF_PRG | BRF_ESS }, // 21
	{ "hml3.b8",				0x2000, 0xe0a5a6aa, 1 | BRF_PRG | BRF_ESS }, // 22
	{ "hml3.b10",				0x2000, 0xde65dfdc, 1 | BRF_PRG | BRF_ESS }, // 23

	{ "hma3.1h",				0x2000, 0xf718da36, 2 | BRF_PRG | BRF_ESS }, // 24 M6809 #1 Code

	{ "hm2.2k",					0x2000, 0xd3583b62, 3 | BRF_SND },           // 25 Samples
	{ "hm2.2l",					0x2000, 0xc059d51e, 3 | BRF_SND },           // 26
	{ "hma.2m",					0x2000, 0x09bb8495, 3 | BRF_SND },           // 27
	{ "hma.2n",					0x2000, 0xe3d290df, 3 | BRF_SND },           // 28
	{ "hma.2p",					0x2000, 0xf93d1ac0, 3 | BRF_SND },           // 29
	{ "hma.2r",					0x2000, 0x0f3a090e, 3 | BRF_SND },           // 30
	{ "hma.2s",					0x2000, 0xc5d35f84, 3 | BRF_SND },           // 31
	{ "hma.2t",					0x2000, 0x9aa71457, 3 | BRF_SND },           // 32
	{ "xba.1n",					0x2000, 0x2e855698, 3 | BRF_SND },           // 33
	{ "hma.1p",					0x2000, 0x021d89dd, 3 | BRF_SND },           // 34
	{ "hma.1r",					0x2000, 0xe8bb33bc, 3 | BRF_SND },           // 35
	{ "hma.1s",					0x2000, 0x65f1aa6e, 3 | BRF_SND },           // 36
	{ "hma.1t",					0x2000, 0xeb35dfcc, 3 | BRF_SND },           // 37

	{ "xbl.12h",				0x0100, 0x375c8bfc, 0 | BRF_OPT },           // 38 PROMs
	{ "xbl.9h",					0x0100, 0x2e7d5562, 0 | BRF_OPT },           // 39
	{ "xbl.2h",					0x0100, 0xb078c1e4, 0 | BRF_OPT },           // 40
	{ "xml-3k_mmi_6331.bin",	0x0020, 0xafa289d1, 0 | BRF_OPT },           // 41
	{ "xbl.4k",					0x0100, 0x31a9549c, 0 | BRF_OPT },           // 42
	{ "xbl.5k",					0x0100, 0x1379bb2a, 0 | BRF_OPT },           // 43
	{ "xbl.6k",					0x0100, 0x588969f7, 0 | BRF_OPT },           // 44
	{ "xbl.7k",					0x0100, 0xeda360b8, 0 | BRF_OPT },           // 45
	{ "xbl.8k",					0x0100, 0x9d434cb1, 0 | BRF_OPT },           // 46
};

STD_ROM_PICK(hitnmiss)
STD_ROM_FN(hitnmiss)

static INT32 HitnmissInit()
{
	hitnmiss = 1;

	return DrvInit(0x26000);
}

static INT32 Hitnmiss2Init()
{
	hitnmiss = 1;

	return DrvInit(0x24000);
}

struct BurnDriver BurnDrvHitnmiss = {
	"hitnmiss", NULL, NULL, NULL, "1987",
	"Hit 'n Miss (version 3.0)\0", NULL, "Exidy", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, hitnmissRomInfo, hitnmissRomName, NULL, NULL, NULL, NULL, HitnmissInputInfo, HitnmissDIPInfo,
	HitnmissInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 240, 4, 3
};


// Hit 'n Miss (version 2.0)

static struct BurnRomInfo hitnmiss2RomDesc[] = {
	{ "hml2.a1",				0x2000, 0x322f7e83, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "hml2.a3",				0x2000, 0x0e12a721, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "hml2.a4",				0x2000, 0x6cec8ad2, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "hml2.a6",				0x2000, 0x008803ec, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "hml2.d4",				0x2000, 0x62790789, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "hml2.d6",				0x2000, 0x02d2d07e, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "hml2.d7",				0x2000, 0x0f795f7a, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "hml2.d8",				0x2000, 0xfe40c51d, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "hml2.d10",				0x2000, 0x9362c90c, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "hml2.d11",				0x2000, 0x0f77322f, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "hml2.c1",				0x2000, 0x8e5803ff, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "hml2.c3",				0x2000, 0xa7474441, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "hml2.c4",				0x2000, 0xc74b9610, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "hml2.c6",				0x2000, 0xaca63300, 1 | BRF_PRG | BRF_ESS }, // 13
	{ "hml2.c7",				0x2000, 0x1990305e, 1 | BRF_PRG | BRF_ESS }, // 14
	{ "hml2.c8",				0x2000, 0xbf08cf05, 1 | BRF_PRG | BRF_ESS }, // 15
	{ "hml2.c10",				0x2000, 0x971323ca, 1 | BRF_PRG | BRF_ESS }, // 16
	{ "hml2.c11",				0x2000, 0xdd172feb, 1 | BRF_PRG | BRF_ESS }, // 17
	{ "hml2.b1",				0x2000, 0xaf1fce57, 1 | BRF_PRG | BRF_ESS }, // 18
	{ "hml2.b3",				0x2000, 0x0d16ef47, 1 | BRF_PRG | BRF_ESS }, // 19
	{ "hml2.b4",				0x2000, 0xd5a8ff68, 1 | BRF_PRG | BRF_ESS }, // 20
	{ "hml2.b6",				0x2000, 0x13f439b1, 1 | BRF_PRG | BRF_ESS }, // 21
	{ "hml2.b7",				0x2000, 0x9088c16d, 1 | BRF_PRG | BRF_ESS }, // 22
	{ "hml2.b8",				0x2000, 0x9c2db94a, 1 | BRF_PRG | BRF_ESS }, // 23
	{ "hml2.b10",				0x2000, 0xf01bd7d4, 1 | BRF_PRG | BRF_ESS }, // 24

	{ "hma2.1h",				0x2000, 0x9be48f45, 2 | BRF_PRG | BRF_ESS }, // 25 M6809 #1 Code

	{ "hm2.2k",					0x2000, 0xd3583b62, 3 | BRF_SND },           // 26 Samples
	{ "hm2.2l",					0x2000, 0xc059d51e, 3 | BRF_SND },           // 27
	{ "hma.2m",					0x2000, 0x09bb8495, 3 | BRF_SND },           // 28
	{ "hma.2n",					0x2000, 0xe3d290df, 3 | BRF_SND },           // 29
	{ "hma.2p",					0x2000, 0xf93d1ac0, 3 | BRF_SND },           // 30
	{ "hma.2r",					0x2000, 0x0f3a090e, 3 | BRF_SND },           // 31
	{ "hma.2s",					0x2000, 0xc5d35f84, 3 | BRF_SND },           // 32
	{ "hma.2t",					0x2000, 0x9aa71457, 3 | BRF_SND },           // 33
	{ "xba.1n",					0x2000, 0x2e855698, 3 | BRF_SND },           // 34
	{ "hma.1p",					0x2000, 0x021d89dd, 3 | BRF_SND },           // 35
	{ "hma.1r",					0x2000, 0xe8bb33bc, 3 | BRF_SND },           // 36
	{ "hma.1s",					0x2000, 0x65f1aa6e, 3 | BRF_SND },           // 37
	{ "hma.1t",					0x2000, 0xeb35dfcc, 3 | BRF_SND },           // 38

	{ "xbl.12h",				0x0100, 0x375c8bfc, 0 | BRF_OPT },           // 39 PROMs
	{ "xbl.9h",					0x0100, 0x2e7d5562, 0 | BRF_OPT },           // 40
	{ "xbl.2h",					0x0100, 0xb078c1e4, 0 | BRF_OPT },           // 41
	{ "xml-3k_mmi_6331.bin",	0x0020, 0xafa289d1, 0 | BRF_OPT },           // 42
	{ "xbl.4k",					0x0100, 0x31a9549c, 0 | BRF_OPT },           // 43
	{ "xbl.5k",					0x0100, 0x1379bb2a, 0 | BRF_OPT },           // 44
	{ "xbl.6k",					0x0100, 0x588969f7, 0 | BRF_OPT },           // 45
	{ "xbl.7k",					0x0100, 0xeda360b8, 0 | BRF_OPT },           // 46
	{ "xbl.8k",					0x0100, 0x9d434cb1, 0 | BRF_OPT },           // 47
};

STD_ROM_PICK(hitnmiss2)
STD_ROM_FN(hitnmiss2)

struct BurnDriver BurnDrvHitnmiss2 = {
	"hitnmiss2", "hitnmiss", NULL, NULL, "1987",
	"Hit 'n Miss (version 2.0)\0", NULL, "Exidy", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, hitnmiss2RomInfo, hitnmiss2RomName, NULL, NULL, NULL, NULL, HitnmissInputInfo, HitnmissDIPInfo,
	Hitnmiss2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 240, 4, 3
};


// Who Dunit (version 9.0)

static struct BurnRomInfo whodunitRomDesc[] = {
	{ "wdl-9_1-a.1a",			0x2000, 0x903e45f6, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "wdl-9_3-a.3a",			0x2000, 0x5d1530f8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "wdl-9_4-a.4a",			0x2000, 0x96c6b81f, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "wdl-9_6-a.6a",			0x2000, 0xc9a864ec, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "wdl-9_4-e.4e",			0x2000, 0x246c836f, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "wdl-9_6-e.6e",			0x2000, 0x65d551c0, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "wdl-9_7-e.7e",			0x2000, 0xaee7a237, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "wdl-9_8-e.8e",			0x2000, 0xa1c1e995, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "wdl-9_10-e.10e",			0x2000, 0xf24adde5, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "wdl-9_11-e.11e",			0x2000, 0xad6fe69e, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "wdl-9_1-d.1d",			0x2000, 0x3572fb71, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "wdl-9_3-d.3d",			0x2000, 0x158074f4, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "wdl-9_4-d.4d",			0x2000, 0x601d8bd0, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "wdl-9_6-d.6d",			0x2000, 0xb72e8f63, 1 | BRF_PRG | BRF_ESS }, // 13
	{ "wdl-9_7-d.7d",			0x2000, 0xe3f55a4b, 1 | BRF_PRG | BRF_ESS }, // 14
	{ "wdl-9_8-d.8d",			0x2000, 0x932689c8, 1 | BRF_PRG | BRF_ESS }, // 15
	{ "wdl-9_10-d.10d",			0x2000, 0x0c4348f2, 1 | BRF_PRG | BRF_ESS }, // 16
	{ "wdl-9_11-d.11d",			0x2000, 0x92391ffe, 1 | BRF_PRG | BRF_ESS }, // 17
	{ "wdl-9_1-c.1c",			0x2000, 0x21c62c90, 1 | BRF_PRG | BRF_ESS }, // 18
	{ "wdl-9_3-c.3c",			0x2000, 0xb221188e, 1 | BRF_PRG | BRF_ESS }, // 19
	{ "wdl-9_4-c.4c",			0x2000, 0x7b58dfac, 1 | BRF_PRG | BRF_ESS }, // 20
	{ "wdl-9_6-c.6c",			0x2000, 0xabe0ba57, 1 | BRF_PRG | BRF_ESS }, // 21
	{ "wdl-9_7-c.7c",			0x2000, 0x3cb3faae, 1 | BRF_PRG | BRF_ESS }, // 22
	{ "wdl-9_8-c.8c",			0x2000, 0x51e95d91, 1 | BRF_PRG | BRF_ESS }, // 23
	{ "wdl-9_10-c.10c",			0x2000, 0xeab12084, 1 | BRF_PRG | BRF_ESS }, // 24
	{ "wdl-9_11-c.11c",			0x2000, 0xfe2c532f, 1 | BRF_PRG | BRF_ESS }, // 25
	{ "wdl-9_1-b.1b",			0x2000, 0xcdaa5ca0, 1 | BRF_PRG | BRF_ESS }, // 26
	{ "wdl-9_3-b.3b",			0x2000, 0x9bbc8161, 1 | BRF_PRG | BRF_ESS }, // 27
	{ "wdl-9_4-b.4b",			0x2000, 0xeb7dc583, 1 | BRF_PRG | BRF_ESS }, // 28
	{ "wdl-9_6-b.6b",			0x2000, 0x91c049a5, 1 | BRF_PRG | BRF_ESS }, // 29
	{ "wdl-9_7-b.7b",			0x2000, 0xfe0a2d00, 1 | BRF_PRG | BRF_ESS }, // 30
	{ "wdl-9_8-b.8b",			0x2000, 0x33792758, 1 | BRF_PRG | BRF_ESS }, // 31
	{ "wdl-9_10-b.10b",			0x2000, 0xc5ab5805, 1 | BRF_PRG | BRF_ESS }, // 32

	{ "wda-9_h-1.h1",			0x2000, 0xdc4b36f0, 2 | BRF_PRG | BRF_ESS }, // 33 M6809 #1 Code

	{ "wda-9_2-k.2k",			0x2000, 0xd4951375, 3 | BRF_SND },           // 34 Samples
	{ "wda-9_2-l.2l",			0x2000, 0xbe8dcf07, 3 | BRF_SND },           // 35
	{ "wda-9_2-m.2m",			0x2000, 0xfb389e2d, 3 | BRF_SND },           // 36
	{ "wda-9_2-n.2n",			0x2000, 0x3849bc78, 3 | BRF_SND },           // 37
	{ "wda-9_2-p.2p",			0x2000, 0xd0dcea80, 3 | BRF_SND },           // 38
	{ "wda-9_2-r.2r",			0x2000, 0x748b0930, 3 | BRF_SND },           // 39
	{ "wda-9_2-s.2s",			0x2000, 0x23d5c5a9, 3 | BRF_SND },           // 40
	{ "wda-9_2-t.2t",			0x2000, 0xa807536d, 3 | BRF_SND },           // 41
	{ "wda-9_1-l.1l",			0x2000, 0x27b856bd, 3 | BRF_SND },           // 42
	{ "wda-9_1-m.1m",			0x2000, 0x8e15c601, 3 | BRF_SND },           // 43
	{ "wda-9_1-n.1n",			0x2000, 0x2e855698, 3 | BRF_SND },           // 44
	{ "wda-9_1-p.1p",			0x2000, 0x3ffaaa22, 3 | BRF_SND },           // 45
	{ "wda-9_1-r.1r",			0x2000, 0x0579a3b8, 3 | BRF_SND },           // 46
	{ "wda-9_1-s.1s",			0x2000, 0xf55c3c6e, 3 | BRF_SND },           // 47
	{ "wda-9_1-t.1t",			0x2000, 0x38363b52, 3 | BRF_SND },           // 48

	{ "xbl.12h",				0x0100, 0x375c8bfc, 0 | BRF_OPT },           // 49 PROMs
	{ "xbl.9h",					0x0100, 0x2e7d5562, 0 | BRF_OPT },           // 50
	{ "xbl.2h",					0x0100, 0xb078c1e4, 0 | BRF_OPT },           // 51
	{ "xbl-3k_mmi_6331.bin",	0x0020, 0xafa289d1, 0 | BRF_OPT },           // 52
	{ "xbl.4k",					0x0100, 0x31a9549c, 0 | BRF_OPT },           // 53
	{ "xbl.5k",					0x0100, 0x1379bb2a, 0 | BRF_OPT },           // 54
	{ "xbl.6k",					0x0100, 0x588969f7, 0 | BRF_OPT },           // 55
	{ "xbl.7k",					0x0100, 0xeda360b8, 0 | BRF_OPT },           // 56
	{ "xbl.8k",					0x0100, 0x9d434cb1, 0 | BRF_OPT },           // 57
	{ "11b_xicor_2804a.bin",	0x0200, 0xec4a70e2, 0 | BRF_OPT },           // 58
};

STD_ROM_PICK(whodunit)
STD_ROM_FN(whodunit)

static INT32 WhodunitInit()
{
	whodunit = 1;

	return DrvInit(0x14000);
}

struct BurnDriver BurnDrvWhodunit = {
	"whodunit", NULL, NULL, NULL, "1988",
	"Who Dunit (version 9.0)\0", NULL, "Exidy", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, whodunitRomInfo, whodunitRomName, NULL, NULL, NULL, NULL, CrossbowInputInfo, WhodunitDIPInfo,
	WhodunitInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 240, 4, 3
};


// Who Dunit (version 8.0)

static struct BurnRomInfo whodunit8RomDesc[] = {
	{ "wdl8.1a",				0x2000, 0x50658904, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "wdl8.3a",				0x2000, 0x5d1530f8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "wdl8.4a",				0x2000, 0x0323d6b8, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "wdl8.6a",				0x2000, 0x771b3fb1, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "wdl8.4e",				0x2000, 0x33e44369, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "wdl8.6e",				0x2000, 0x64b1d850, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "wdl8.7e",				0x2000, 0xaa54cf90, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "wdl8.8e",				0x2000, 0xcbd61200, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "wdl8.10e",				0x2000, 0xf24adde5, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "wdl8.11e",				0x2000, 0xad6fe69e, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "wdl6.1d",				0x2000, 0x3572fb71, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "wdl6.3d",				0x2000, 0x158074f4, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "wdl8.4d",				0x2000, 0x601d8bd0, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "wdl8.6d",				0x2000, 0xb72e8f63, 1 | BRF_PRG | BRF_ESS }, // 13
	{ "wdl6.7d",				0x2000, 0xe3f55a4b, 1 | BRF_PRG | BRF_ESS }, // 14
	{ "wdl6.8d",				0x2000, 0x932689c8, 1 | BRF_PRG | BRF_ESS }, // 15
	{ "wdl6.10d",				0x2000, 0x0c4348f2, 1 | BRF_PRG | BRF_ESS }, // 16
	{ "wdl8.11d",				0x2000, 0x92391ffe, 1 | BRF_PRG | BRF_ESS }, // 17
	{ "wdl8.1c",				0x2000, 0x21c62c90, 1 | BRF_PRG | BRF_ESS }, // 18
	{ "wdl8.3c",				0x2000, 0x5a8123be, 1 | BRF_PRG | BRF_ESS }, // 19
	{ "wdl6.4c",				0x2000, 0x7b58dfac, 1 | BRF_PRG | BRF_ESS }, // 20
	{ "wdl6.6c",				0x2000, 0x9be0b9a9, 1 | BRF_PRG | BRF_ESS }, // 21
	{ "wdl6.7c",				0x2000, 0x3cb3faae, 1 | BRF_PRG | BRF_ESS }, // 22
	{ "wdl6.8c",				0x2000, 0x51e95d91, 1 | BRF_PRG | BRF_ESS }, // 23
	{ "wdl6.10c",				0x2000, 0xeab12084, 1 | BRF_PRG | BRF_ESS }, // 24
	{ "wdl6.11c",				0x2000, 0xfe2c532f, 1 | BRF_PRG | BRF_ESS }, // 25
	{ "wdl6.1b",				0x2000, 0xcdaa5ca0, 1 | BRF_PRG | BRF_ESS }, // 26
	{ "wdl6.3b",				0x2000, 0x9bbc8161, 1 | BRF_PRG | BRF_ESS }, // 27
	{ "wdl8.4b",				0x2000, 0xeb7dc583, 1 | BRF_PRG | BRF_ESS }, // 28
	{ "wdl8.6b",				0x2000, 0x91c049a5, 1 | BRF_PRG | BRF_ESS }, // 29
	{ "wdl6.7b",				0x2000, 0xfe0a2d00, 1 | BRF_PRG | BRF_ESS }, // 30
	{ "wdl8.8b",				0x2000, 0x33792758, 1 | BRF_PRG | BRF_ESS }, // 31
	{ "wdl6.10b",				0x2000, 0x2f48cfdb, 1 | BRF_PRG | BRF_ESS }, // 32

	{ "wda8.h1",				0x2000, 0x0090e5a7, 2 | BRF_PRG | BRF_ESS }, // 33 M6809 #1 Code

	{ "wda6.k2",				0x2000, 0xd4951375, 3 | BRF_SND },           // 34 Samples
	{ "wda6.l2",				0x2000, 0xbe8dcf07, 3 | BRF_SND },           // 35
	{ "wda6.m2",				0x2000, 0xfb389e2d, 3 | BRF_SND },           // 36
	{ "wda6.n2",				0x2000, 0x3849bc78, 3 | BRF_SND },           // 37
	{ "wda6.p2",				0x2000, 0xd0dcea80, 3 | BRF_SND },           // 38
	{ "wda6.r2",				0x2000, 0x748b0930, 3 | BRF_SND },           // 39
	{ "wda6.s2",				0x2000, 0x23d5c5a9, 3 | BRF_SND },           // 40
	{ "wda6.t2",				0x2000, 0xa807536d, 3 | BRF_SND },           // 41
	{ "wda8.l1",				0x2000, 0x27b856bd, 3 | BRF_SND },           // 42
	{ "wda8.m1",				0x2000, 0x8e15c601, 3 | BRF_SND },           // 43
	{ "xba1.1n",				0x2000, 0x2e855698, 3 | BRF_SND },           // 44
	{ "wda6.p1",				0x2000, 0x3ffaaa22, 3 | BRF_SND },           // 45
	{ "wda6.r1",				0x2000, 0x0579a3b8, 3 | BRF_SND },           // 46
	{ "wda6.s1",				0x2000, 0xf55c3c6e, 3 | BRF_SND },           // 47
	{ "wda6.t1",				0x2000, 0x38363b52, 3 | BRF_SND },           // 48

	{ "xbl.12h",				0x0100, 0x375c8bfc, 0 | BRF_OPT },           // 49 PROMs
	{ "xbl.9h",					0x0100, 0x2e7d5562, 0 | BRF_OPT },           // 50
	{ "xbl.2h",					0x0100, 0xb078c1e4, 0 | BRF_OPT },           // 51
	{ "xml-3k_mmi_6331.bin",	0x0020, 0xafa289d1, 0 | BRF_OPT },           // 52
	{ "xbl.4k",					0x0100, 0x31a9549c, 0 | BRF_OPT },           // 53
	{ "xbl.5k",					0x0100, 0x1379bb2a, 0 | BRF_OPT },           // 54
	{ "xbl.6k",					0x0100, 0x588969f7, 0 | BRF_OPT },           // 55
	{ "xbl.7k",					0x0100, 0xeda360b8, 0 | BRF_OPT },           // 56
	{ "xbl.8k",					0x0100, 0x9d434cb1, 0 | BRF_OPT },           // 57
};

STD_ROM_PICK(whodunit8)
STD_ROM_FN(whodunit8)

struct BurnDriver BurnDrvWhodunit8 = {
	"whodunit8", "whodunit", NULL, NULL, "1988",
	"Who Dunit (version 8.0)\0", NULL, "Exidy", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, whodunit8RomInfo, whodunit8RomName, NULL, NULL, NULL, NULL, CrossbowInputInfo, WhodunitDIPInfo,
	WhodunitInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 240, 4, 3
};


// Showdown (version 5.0)

static struct BurnRomInfo showdownRomDesc[] = {
	{ "sld-5_1-a.1a",			0x2000, 0xe4031507, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "sld-5_3-a.3a",			0x2000, 0xe7de171e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sld-5_4-a.4a",			0x2000, 0x5c8683c9, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sld-5_6-a.6a",			0x2000, 0x4a408379, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "sld-5_11-e.11e",			0x2000, 0x1c6b34e5, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "sld-5_1-d.1d",			0x2000, 0xdb4c8cf6, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "sld-5_3-d.3d",			0x2000, 0x24242867, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "sld-5_4-d.4d",			0x2000, 0x36f247e9, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "sld-5_6-d.6d",			0x2000, 0xc9b14d8d, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "sld-5_7-d.7d",			0x2000, 0xfd054cd2, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "sld-5_8-d.8d",			0x2000, 0x8bf32822, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "sld-5_10-d.10d",			0x2000, 0xa2051da2, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "sld-5_11-d.11d",			0x2000, 0x0748f345, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "sld-5_1-c.1c",			0x2000, 0xc016cf73, 1 | BRF_PRG | BRF_ESS }, // 13
	{ "sld-5_3-c.3c",			0x2000, 0x652503ee, 1 | BRF_PRG | BRF_ESS }, // 14
	{ "sld-5_4-c.4c",			0x2000, 0xb4dab193, 1 | BRF_PRG | BRF_ESS }, // 15
	{ "sld-5_6-c.6c",			0x2000, 0xa1e6a2b3, 1 | BRF_PRG | BRF_ESS }, // 16
	{ "sld-5_7-c.7c",			0x2000, 0xbc1bea93, 1 | BRF_PRG | BRF_ESS }, // 17
	{ "sld-5_8-c.8c",			0x2000, 0x337dd7fa, 1 | BRF_PRG | BRF_ESS }, // 18
	{ "sld-5_10-c.10c",			0x2000, 0x3ad32d71, 1 | BRF_PRG | BRF_ESS }, // 19
	{ "sld-5_11-c.11c",			0x2000, 0x5fe91932, 1 | BRF_PRG | BRF_ESS }, // 20
	{ "sld-5_1-b.1b",			0x2000, 0x54ff987e, 1 | BRF_PRG | BRF_ESS }, // 21
	{ "sld-5_3-b.3b",			0x2000, 0xe302e915, 1 | BRF_PRG | BRF_ESS }, // 22
	{ "sld-5_4-b.4b",			0x2000, 0x1b981516, 1 | BRF_PRG | BRF_ESS }, // 23
	{ "sld-5_6-b.6b",			0x2000, 0x4ee00996, 1 | BRF_PRG | BRF_ESS }, // 24
	{ "sld-5_7-b.7b",			0x2000, 0x018b7c00, 1 | BRF_PRG | BRF_ESS }, // 25
	{ "sld-5_8-b.8b",			0x2000, 0x024fe6ee, 1 | BRF_PRG | BRF_ESS }, // 26
	{ "sld-5_10-b.10b",			0x2000, 0x0b318dfe, 1 | BRF_PRG | BRF_ESS }, // 27

	{ "sda-5_h-1.h1",			0x2000, 0x6a10ff47, 2 | BRF_PRG | BRF_ESS }, // 28 M6809 #1 Code

	{ "sda-5_k-2.k2",			0x2000, 0x67a86f7f, 3 | BRF_SND },           // 29 Samples
	{ "sda-5_l-2.l2",			0x2000, 0x0bb8874b, 3 | BRF_SND },           // 30
	{ "sda-5_m-2.m2",			0x2000, 0x8b77eac8, 3 | BRF_SND },           // 31
	{ "sda-5_n-2.n2",			0x2000, 0x78e6eed6, 3 | BRF_SND },           // 32
	{ "sda-5_p-2.p2",			0x2000, 0x03a13435, 3 | BRF_SND },           // 33
	{ "sda-5_r-2.r2",			0x2000, 0x1b6b7eac, 3 | BRF_SND },           // 34
	{ "sda-5_s-2.s2",			0x2000, 0xb88aeb82, 3 | BRF_SND },           // 35
	{ "sda-5_t-2.t2",			0x2000, 0x5c801f4d, 3 | BRF_SND },           // 36
	{ "sda-5_k-1.k1",			0x2000, 0x4e1f4f15, 3 | BRF_SND },           // 37
	{ "sda-5_l-1.l1",			0x2000, 0x6779a745, 3 | BRF_SND },           // 38
	{ "sda-5_m-1.m1",			0x2000, 0x9cebd8ea, 3 | BRF_SND },           // 39
	{ "sda-5_n-1.n1",			0x2000, 0x689d8a3f, 3 | BRF_SND },           // 40
	{ "sda-5_p-1.p1",			0x2000, 0x862b350d, 3 | BRF_SND },           // 41
	{ "sda-5_r-1.r1",			0x2000, 0x95b099ed, 3 | BRF_SND },           // 42
	{ "sda-5_s-1.s1",			0x2000, 0x8f230881, 3 | BRF_SND },           // 43
	{ "sda-5_t-1.t1",			0x2000, 0x70e724c7, 3 | BRF_SND },           // 44

	{ "xbl.12h",				0x0100, 0x375c8bfc, 0 | BRF_OPT },           // 45 PROMs
	{ "xbl.9h",					0x0100, 0x2e7d5562, 0 | BRF_OPT },           // 46
	{ "xbl.2h",					0x0100, 0xb078c1e4, 0 | BRF_OPT },           // 47
	{ "xml-3k_mmi_6331.bin",	0x0020, 0xafa289d1, 0 | BRF_OPT },           // 48
	{ "xbl.4k",					0x0100, 0x31a9549c, 0 | BRF_OPT },           // 49
	{ "xbl.5k",					0x0100, 0x1379bb2a, 0 | BRF_OPT },           // 50
	{ "xbl.6k",					0x0100, 0x588969f7, 0 | BRF_OPT },           // 51
	{ "xbl.7k",					0x0100, 0xeda360b8, 0 | BRF_OPT },           // 52
	{ "xbl.8k",					0x0100, 0x9d434cb1, 0 | BRF_OPT },           // 53
};

STD_ROM_PICK(showdown)
STD_ROM_FN(showdown)

static INT32 ShowdownInit()
{
	static UINT8 bankdata[0x30] = {
		0x15,0x40,0xc1,0x8d,0x4c,0x84,0x0e,0xce,0x52,0xd0,0x99,0x48,0x80,0x09,0xc9,0x45,
		0xc4,0x8e,0x5a,0x92,0x18,0xd8,0x51,0xc0,0x11,0x51,0xc0,0x89,0x4d,0x85,0x0c,0xcc,
		0x46,0xd2,0x98,0x59,0x91,0x08,0xc8,0x41,0xc5,0x8c,0x4e,0x86,0x1a,0xda,0x50,0xd1
	};

	showdown_bank_data[0] = bankdata + 0;
	showdown_bank_data[1] = bankdata + 0x18;

	return DrvInit(0x1e000);
}

struct BurnDriver BurnDrvShowdown = {
	"showdown", NULL, NULL, NULL, "1988",
	"Showdown (version 5.0)\0", NULL, "Exidy", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_MISC_PRE90S, GBF_CASINO, 0,
	NULL, showdownRomInfo, showdownRomName, NULL, NULL, NULL, NULL, ShowdownInputInfo, ShowdownDIPInfo,
	ShowdownInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 240, 4, 3
};


// Showdown (version 4.0)

static struct BurnRomInfo showdown4RomDesc[] = {
	{ "sld-4_1-a.1a",			0x2000, 0x674e078e, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "sld-5_3-a.3a",			0x2000, 0xe7de171e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sld-5_4-a.4a",			0x2000, 0x5c8683c9, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sld-5_6-a.6a",			0x2000, 0x4a408379, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "sld-5_11-e.11e",			0x2000, 0x1c6b34e5, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "sld-5_1-d.1d",			0x2000, 0xdb4c8cf6, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "sld-5_3-d.3d",			0x2000, 0x24242867, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "sld-5_4-d.4d",			0x2000, 0x36f247e9, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "sld-5_6-d.6d",			0x2000, 0xc9b14d8d, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "sld-5_7-d.7d",			0x2000, 0xfd054cd2, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "sld-5_8-d.8d",			0x2000, 0x8bf32822, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "sld-5_10-d.10d",			0x2000, 0xa2051da2, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "sld-5_11-d.11d",			0x2000, 0x0748f345, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "sld-5_1-c.1c",			0x2000, 0xc016cf73, 1 | BRF_PRG | BRF_ESS }, // 13
	{ "sld-5_3-c.3c",			0x2000, 0x652503ee, 1 | BRF_PRG | BRF_ESS }, // 14
	{ "sld-5_4-c.4c",			0x2000, 0xb4dab193, 1 | BRF_PRG | BRF_ESS }, // 15
	{ "sld-5_6-c.6c",			0x2000, 0xa1e6a2b3, 1 | BRF_PRG | BRF_ESS }, // 16
	{ "sld-5_7-c.7c",			0x2000, 0xbc1bea93, 1 | BRF_PRG | BRF_ESS }, // 17
	{ "sld-5_8-c.8c",			0x2000, 0x337dd7fa, 1 | BRF_PRG | BRF_ESS }, // 18
	{ "sld-5_10-c.10c",			0x2000, 0x3ad32d71, 1 | BRF_PRG | BRF_ESS }, // 19
	{ "sld-5_11-c.11c",			0x2000, 0x5fe91932, 1 | BRF_PRG | BRF_ESS }, // 20
	{ "sld-5_1-b.1b",			0x2000, 0x54ff987e, 1 | BRF_PRG | BRF_ESS }, // 21
	{ "sld-5_3-b.3b",			0x2000, 0xe302e915, 1 | BRF_PRG | BRF_ESS }, // 22
	{ "sld-5_4-b.4b",			0x2000, 0x1b981516, 1 | BRF_PRG | BRF_ESS }, // 23
	{ "sld-5_6-b.6b",			0x2000, 0x4ee00996, 1 | BRF_PRG | BRF_ESS }, // 24
	{ "sld-5_7-b.7b",			0x2000, 0x018b7c00, 1 | BRF_PRG | BRF_ESS }, // 25
	{ "sld-5_8-b.8b",			0x2000, 0x024fe6ee, 1 | BRF_PRG | BRF_ESS }, // 26
	{ "sld-5_10-b.10b",			0x2000, 0x0b318dfe, 1 | BRF_PRG | BRF_ESS }, // 27

	{ "sda-5_h-1.h1",			0x2000, 0x6a10ff47, 2 | BRF_PRG | BRF_ESS }, // 28 M6809 #1 Code

	{ "sda-5_k-2.k2",			0x2000, 0x67a86f7f, 3 | BRF_SND },           // 29 Samples
	{ "sda-5_l-2.l2",			0x2000, 0x0bb8874b, 3 | BRF_SND },           // 30
	{ "sda-5_m-2.m2",			0x2000, 0x8b77eac8, 3 | BRF_SND },           // 31
	{ "sda-5_n-2.n2",			0x2000, 0x78e6eed6, 3 | BRF_SND },           // 32
	{ "sda-5_p-2.p2",			0x2000, 0x03a13435, 3 | BRF_SND },           // 33
	{ "sda-5_r-2.r2",			0x2000, 0x1b6b7eac, 3 | BRF_SND },           // 34
	{ "sda-5_s-2.s2",			0x2000, 0xb88aeb82, 3 | BRF_SND },           // 35
	{ "sda-5_t-2.t2",			0x2000, 0x5c801f4d, 3 | BRF_SND },           // 36
	{ "sda-5_k-1.k1",			0x2000, 0x4e1f4f15, 3 | BRF_SND },           // 37
	{ "sda-5_l-1.l1",			0x2000, 0x6779a745, 3 | BRF_SND },           // 38
	{ "sda-5_m-1.m1",			0x2000, 0x9cebd8ea, 3 | BRF_SND },           // 39
	{ "sda-5_n-1.n1",			0x2000, 0x689d8a3f, 3 | BRF_SND },           // 40
	{ "sda-5_p-1.p1",			0x2000, 0x862b350d, 3 | BRF_SND },           // 41
	{ "sda-5_r-1.r1",			0x2000, 0x95b099ed, 3 | BRF_SND },           // 42
	{ "sda-5_s-1.s1",			0x2000, 0x8f230881, 3 | BRF_SND },           // 43
	{ "sda-5_t-1.t1",			0x2000, 0x70e724c7, 3 | BRF_SND },           // 44

	{ "xbl.12h",				0x0100, 0x375c8bfc, 0 | BRF_OPT },           // 45 PROMs
	{ "xbl.9h",					0x0100, 0x2e7d5562, 0 | BRF_OPT },           // 46
	{ "xbl.2h",					0x0100, 0xb078c1e4, 0 | BRF_OPT },           // 47
	{ "xml-3k_mmi_6331.bin",	0x0020, 0xafa289d1, 0 | BRF_OPT },           // 48
	{ "xbl.4k",					0x0100, 0x31a9549c, 0 | BRF_OPT },           // 49
	{ "xbl.5k",					0x0100, 0x1379bb2a, 0 | BRF_OPT },           // 50
	{ "xbl.6k",					0x0100, 0x588969f7, 0 | BRF_OPT },           // 51
	{ "xbl.7k",					0x0100, 0xeda360b8, 0 | BRF_OPT },           // 52
	{ "xbl.8k",					0x0100, 0x9d434cb1, 0 | BRF_OPT },           // 53
};

STD_ROM_PICK(showdown4)
STD_ROM_FN(showdown4)

struct BurnDriver BurnDrvShowdown4 = {
	"showdown4", "showdown", NULL, NULL, "1988",
	"Showdown (version 4.0)\0", NULL, "Exidy", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_CLONE, 1, HARDWARE_MISC_PRE90S, GBF_CASINO, 0,
	NULL, showdown4RomInfo, showdown4RomName, NULL, NULL, NULL, NULL, ShowdownInputInfo, ShowdownDIPInfo,
	ShowdownInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 240, 4, 3
};


// Yukon (version 2.0)

static struct BurnRomInfo yukonRomDesc[] = {
	{ "yul-20.1a",				0x2000, 0xd8929303, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "yul-1.3a",				0x2000, 0x165fd218, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "yul-1.4a",				0x2000, 0x308232ce, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "yul-1.6a",				0x2000, 0x7ddd6235, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "yul-1.4d",				0x2000, 0xbcd5676e, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "yul-1.6d",				0x2000, 0x283c1e25, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "yul-1.7d",				0x2000, 0x7d03232a, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "yul-1.8d",				0x2000, 0xa46a0253, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "yul-1.10d",				0x2000, 0x9e2ca5b3, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "yul-1.11d",				0x2000, 0x1d9f0981, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "yul-1.1c",				0x2000, 0xdc1a8ce7, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "yul-1.3c",				0x2000, 0xce840607, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "yul-1.4c",				0x2000, 0xf0c736ae, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "yul-1.6c",				0x2000, 0x48779436, 1 | BRF_PRG | BRF_ESS }, // 13
	{ "yul-1.7c",				0x2000, 0xb653ab9e, 1 | BRF_PRG | BRF_ESS }, // 14
	{ "yul-1.8c",				0x2000, 0x3e291d7e, 1 | BRF_PRG | BRF_ESS }, // 15
	{ "yul-1.10c",				0x2000, 0x7f677082, 1 | BRF_PRG | BRF_ESS }, // 16
	{ "yul-1.11c",				0x2000, 0xb7f5ea8d, 1 | BRF_PRG | BRF_ESS }, // 17
	{ "yul-1.1b",				0x2000, 0x75cb768d, 1 | BRF_PRG | BRF_ESS }, // 18
	{ "yul-1.3b",				0x2000, 0xb76c4ec9, 1 | BRF_PRG | BRF_ESS }, // 19
	{ "yul-1.4b",				0x2000, 0x1b981516, 1 | BRF_PRG | BRF_ESS }, // 20
	{ "yul-1.6b",				0x2000, 0xab7b77e2, 1 | BRF_PRG | BRF_ESS }, // 21
	{ "yul-1.7b",				0x2000, 0x30a62d8f, 1 | BRF_PRG | BRF_ESS }, // 22
	{ "yul-1.8b",				0x2000, 0xfa85b58e, 1 | BRF_PRG | BRF_ESS }, // 23

	{ "yua-1.1h",				0x2000, 0xf0df665a, 2 | BRF_PRG | BRF_ESS }, // 24 M6809 #1 Code

	{ "yua-1.2k",				0x2000, 0x67a86f7f, 3 | BRF_SND },           // 25 Samples
	{ "yua-1.2l",				0x2000, 0x0bb8874b, 3 | BRF_SND },           // 26
	{ "yua-1.2m",				0x2000, 0x8b77eac8, 3 | BRF_SND },           // 27
	{ "yua-1.2n",				0x2000, 0x78e6eed6, 3 | BRF_SND },           // 28
	{ "yua-1.2p",				0x2000, 0xa8fa14b7, 3 | BRF_SND },           // 29
	{ "yua-1.2r",				0x2000, 0xca978863, 3 | BRF_SND },           // 30
	{ "yua-1.2s",				0x2000, 0x74d51be4, 3 | BRF_SND },           // 31
	{ "yua-1.2t",				0x2000, 0xe8a9543e, 3 | BRF_SND },           // 32
	{ "yua-1.1k",				0x2000, 0x4e1f4f15, 3 | BRF_SND },           // 33
	{ "yua-1.1l",				0x2000, 0x6779a745, 3 | BRF_SND },           // 34
	{ "yua-1.1m",				0x2000, 0x9cebd8ea, 3 | BRF_SND },           // 35
	{ "yua-1.1n",				0x2000, 0x689d8a3f, 3 | BRF_SND },           // 36
	{ "yua-1.1p",				0x2000, 0x862b350d, 3 | BRF_SND },           // 37
	{ "yua-1.1r",				0x2000, 0x95b099ed, 3 | BRF_SND },           // 38
	{ "yua-1.1s",				0x2000, 0x8f230881, 3 | BRF_SND },           // 39
	{ "yua-1.1t",				0x2000, 0x80926a5c, 3 | BRF_SND },           // 40

	{ "xbl.12h",				0x0100, 0x375c8bfc, 0 | BRF_OPT },           // 41 PROMs
	{ "xbl.9h",					0x0100, 0x2e7d5562, 0 | BRF_OPT },           // 42
	{ "xbl.2h",					0x0100, 0xb078c1e4, 0 | BRF_OPT },           // 43
	{ "xml-3k_mmi_6331.bin",	0x0020, 0xafa289d1, 0 | BRF_OPT },           // 44
	{ "xbl.4k",					0x0100, 0x31a9549c, 0 | BRF_OPT },           // 45
	{ "xbl.5k",					0x0100, 0x1379bb2a, 0 | BRF_OPT },           // 46
	{ "xbl.6k",					0x0100, 0x588969f7, 0 | BRF_OPT },           // 47
	{ "xbl.7k",					0x0100, 0xeda360b8, 0 | BRF_OPT },           // 48
	{ "xbl.8k",					0x0100, 0x9d434cb1, 0 | BRF_OPT },           // 49
};

STD_ROM_PICK(yukon)
STD_ROM_FN(yukon)

static INT32 YukonInit()
{
	static UINT8 bankdata[0x30] = {
		0x31,0x40,0xc1,0x95,0x54,0x90,0x16,0xd6,0x62,0xe0,0xa5,0x44,0x80,0x05,0xc5,0x51,
		0xd0,0x96,0x66,0xa2,0x24,0xe4,0x61,0xc0,0x21,0x61,0xc0,0x85,0x55,0x91,0x14,0xd4,
		0x52,0xe2,0xa4,0x65,0xa1,0x04,0xc4,0x41,0xd1,0x94,0x56,0x92,0x26,0xe6,0x60,0xe1
	};

	showdown_bank_data[0] = bankdata + 0;
	showdown_bank_data[1] = bankdata + 0x18;

	return DrvInit(0x24000);
}

struct BurnDriver BurnDrvYukon = {
	"yukon", NULL, NULL, NULL, "1989",
	"Yukon (version 2.0)\0", NULL, "Exidy", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_MISC_PRE90S, GBF_CASINO, 0,
	NULL, yukonRomInfo, yukonRomName, NULL, NULL, NULL, NULL, ShowdownInputInfo, ShowdownDIPInfo,
	YukonInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 240, 4, 3
};


// Yukon (version 1.0)

static struct BurnRomInfo yukon1RomDesc[] = {
	{ "yul-1.1a",				0x2000, 0x0286411b, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "yul-1.3a",				0x2000, 0x165fd218, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "yul-1.4a",				0x2000, 0x308232ce, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "yul-1.6a",				0x2000, 0x7ddd6235, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "yul-1.4d",				0x2000, 0xbcd5676e, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "yul-1.6d",				0x2000, 0x283c1e25, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "yul-1.7d",				0x2000, 0x7d03232a, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "yul-1.8d",				0x2000, 0xa46a0253, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "yul-1.10d",				0x2000, 0x9e2ca5b3, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "yul-1.11d",				0x2000, 0x1d9f0981, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "yul-1.1c",				0x2000, 0xdc1a8ce7, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "yul-1.3c",				0x2000, 0xce840607, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "yul-1.4c",				0x2000, 0xf0c736ae, 1 | BRF_PRG | BRF_ESS }, // 12
	{ "yul-1.6c",				0x2000, 0x48779436, 1 | BRF_PRG | BRF_ESS }, // 13
	{ "yul-1.7c",				0x2000, 0xb653ab9e, 1 | BRF_PRG | BRF_ESS }, // 14
	{ "yul-1.8c",				0x2000, 0x3e291d7e, 1 | BRF_PRG | BRF_ESS }, // 15
	{ "yul-1.10c",				0x2000, 0x7f677082, 1 | BRF_PRG | BRF_ESS }, // 16
	{ "yul-1.11c",				0x2000, 0xb7f5ea8d, 1 | BRF_PRG | BRF_ESS }, // 17
	{ "yul-1.1b",				0x2000, 0x75cb768d, 1 | BRF_PRG | BRF_ESS }, // 18
	{ "yul-1.3b",				0x2000, 0xb76c4ec9, 1 | BRF_PRG | BRF_ESS }, // 19
	{ "yul-1.4b",				0x2000, 0x1b981516, 1 | BRF_PRG | BRF_ESS }, // 20
	{ "yul-1.6b",				0x2000, 0xab7b77e2, 1 | BRF_PRG | BRF_ESS }, // 21
	{ "yul-1.7b",				0x2000, 0x30a62d8f, 1 | BRF_PRG | BRF_ESS }, // 22
	{ "yul-1.8b",				0x2000, 0xfa85b58e, 1 | BRF_PRG | BRF_ESS }, // 23

	{ "yua-1.1h",				0x2000, 0xf0df665a, 2 | BRF_PRG | BRF_ESS }, // 24 M6809 #1 Code

	{ "yua-1.2k",				0x2000, 0x67a86f7f, 3 | BRF_SND },           // 25 Samples
	{ "yua-1.2l",				0x2000, 0x0bb8874b, 3 | BRF_SND },           // 26
	{ "yua-1.2m",				0x2000, 0x8b77eac8, 3 | BRF_SND },           // 27
	{ "yua-1.2n",				0x2000, 0x78e6eed6, 3 | BRF_SND },           // 28
	{ "yua-1.2p",				0x2000, 0xa8fa14b7, 3 | BRF_SND },           // 29
	{ "yua-1.2r",				0x2000, 0xca978863, 3 | BRF_SND },           // 30
	{ "yua-1.2s",				0x2000, 0x74d51be4, 3 | BRF_SND },           // 31
	{ "yua-1.2t",				0x2000, 0xe8a9543e, 3 | BRF_SND },           // 32
	{ "yua-1.1k",				0x2000, 0x4e1f4f15, 3 | BRF_SND },           // 33
	{ "yua-1.1l",				0x2000, 0x6779a745, 3 | BRF_SND },           // 34
	{ "yua-1.1m",				0x2000, 0x9cebd8ea, 3 | BRF_SND },           // 35
	{ "yua-1.1n",				0x2000, 0x689d8a3f, 3 | BRF_SND },           // 36
	{ "yua-1.1p",				0x2000, 0x862b350d, 3 | BRF_SND },           // 37
	{ "yua-1.1r",				0x2000, 0x95b099ed, 3 | BRF_SND },           // 38
	{ "yua-1.1s",				0x2000, 0x8f230881, 3 | BRF_SND },           // 39
	{ "yua-1.1t",				0x2000, 0x80926a5c, 3 | BRF_SND },           // 40

	{ "xbl.12h",				0x0100, 0x375c8bfc, 0 | BRF_OPT },           // 41 PROMs
	{ "xbl.9h",					0x0100, 0x2e7d5562, 0 | BRF_OPT },           // 42
	{ "xbl.2h",					0x0100, 0xb078c1e4, 0 | BRF_OPT },           // 43
	{ "xml-3k_mmi_6331.bin",	0x0020, 0xafa289d1, 0 | BRF_OPT },           // 44
	{ "xbl.4k",					0x0100, 0x31a9549c, 0 | BRF_OPT },           // 45
	{ "xbl.5k",					0x0100, 0x1379bb2a, 0 | BRF_OPT },           // 46
	{ "xbl.6k",					0x0100, 0x588969f7, 0 | BRF_OPT },           // 47
	{ "xbl.7k",					0x0100, 0xeda360b8, 0 | BRF_OPT },           // 48
	{ "xbl.8k",					0x0100, 0x9d434cb1, 0 | BRF_OPT },           // 49
};

STD_ROM_PICK(yukon1)
STD_ROM_FN(yukon1)

struct BurnDriver BurnDrvYukon1 = {
	"yukon1", "yukon", NULL, NULL, "1989",
	"Yukon (version 1.0)\0", NULL, "Exidy", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_MISC_PRE90S, GBF_CASINO, 0,
	NULL, yukon1RomInfo, yukon1RomName, NULL, NULL, NULL, NULL, ShowdownInputInfo, ShowdownDIPInfo,
	YukonInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	320, 240, 4, 3
};
