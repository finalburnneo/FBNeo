// FinalBurn Neo Atari System 2 hardware driver module
// Based on MAME driver by Aaron Giles

#include "tiles_generic.h"
#include "t11_intf.h"
#include "m6502_intf.h"
#include "watchdog.h"
#include "atariic.h"
#include "atarimo.h"
#include "slapstic.h"
#include "pokey.h"
#include "burn_ym2151.h"
#include "tms5220.h"
#include "biquad.h"
#include "burn_gun.h" // for dial

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvT11ROM;
static UINT8 *DrvM6502ROM;
static UINT8 *DrvGfxROM[3];
static UINT8 *DrvDefault;
static UINT8 *DrvNVRAM;
static UINT8 *DrvT11RAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvPfRAM;
static UINT8 *DrvMobRAM;
static UINT8 *DrvAlphaRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvM6502RAM;
static INT16 *tempsound; // filtering

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 p2portrd_state;
static INT32 p2portwr_state;
static INT32 m6502_in_reset;
static INT32 videobank;
static INT32 bankselect[2];
static INT32 scrollx;
static INT32 scrolly, scrolly_adj;
static INT32 playfield_tile_bank[2];
static INT32 which_adc;
static INT32 scanline_int_state;
static INT32 video_int_state;
static INT32 interrupt_enable;

static INT32 cpu_to_sound_ready;
static INT32 sound_to_cpu_ready;
static INT32 cpu_to_sound;
static INT32 sound_to_cpu;
static INT32 previous_sound_reset;
static INT32 timed_int;

static UINT8 mixbits;

//static INT32 frame;

static BIQ biqvoice; // filter for tms

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[4];
static UINT8 DrvInputs[3];
static INT16 Analog[6];
static UINT8 DrvReset;

// 720 controller handling
static INT32 last_joy_count;
static INT32 joy_count;

static INT32 last_rotate_count;
static INT32 spinner_count;
static INT32 spinner_center_count;

static INT32 nCyclesExtra;

static INT32 scanline;

static bool is_apb = false;
static bool is_ssprint = false;
static bool is_720 = false;

#define A(a, b, c, d) { a, b, (UINT8*)(c), d }

static struct BurnInputInfo PaperboyInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 coin"		},
	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p2 coin"		},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 fire 1"		},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 2"		},
	A("P1 Stick X",		BIT_ANALOG_REL,	&Analog[0],		"p1 x-axis"		),
	A("P1 Stick Y",		BIT_ANALOG_REL,	&Analog[1],		"p1 y-axis"		),

	{"Service",			BIT_DIGITAL,	DrvJoy1 + 5,	"service"		},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"			},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"			},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"			},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"			},
};

STDINPUTINFO(Paperboy)

static struct BurnInputInfo Drv720InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 coin"		},
	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p2 coin"		},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 fire 1"		},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 2"		},
	A("P1 Stick X",		BIT_ANALOG_REL,	&Analog[0],		"p1 x-axis"		),
	A("P1 Stick Y",		BIT_ANALOG_REL,	&Analog[1],		"p1 y-axis"		),
	A("P1 Spinner",		BIT_ANALOG_REL,	&Analog[2],		"p1 x-axis"		),

	{"Service",			BIT_DIGITAL,	DrvJoy1 + 5,	"service"		},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"			},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"			},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"			},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"			},
	{"Dip D",			BIT_DIPSWITCH,	DrvDips + 3,	"dip"			},
};

STDINPUTINFO(Drv720)

static struct BurnInputInfo SsprintInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 coin"		},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 start"		},
	A("P1 Wheel",		BIT_ANALOG_REL,	&Analog[0],		"p1 x-axis"		),
	A("P1 Pedal",		BIT_ANALOG_REL,	&Analog[1],		"p1 y-axis"		),

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 6,	"p2 coin"		},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 start"		},
	A("P2 Wheel",		BIT_ANALOG_REL,	&Analog[2],		"p2 x-axis"		),
	A("P2 Pedal",		BIT_ANALOG_REL,	&Analog[3],		"p2 y-axis"		),

	{"P3 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p3 coin"		},
	{"P3 Start",		BIT_DIGITAL,	DrvJoy2 + 3,	"p3 start"		},
	A("P3 Wheel",		BIT_ANALOG_REL,	&Analog[4],		"p3 x-axis"		),
	A("P3 Pedal",		BIT_ANALOG_REL,	&Analog[5],		"p3 y-axis"		),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"			},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"			},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"			},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"			},
};

STDINPUTINFO(Ssprint)

static struct BurnInputInfo CsprintInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 coin"		},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 start"		},
	A("P1 Wheel",		BIT_ANALOG_REL,	&Analog[0],		"p1 x-axis"		),
	A("P1 Pedal",		BIT_ANALOG_REL,	&Analog[1],		"p1 y-axis"		),

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p2 coin"		},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 start"		},
	A("P2 Wheel",		BIT_ANALOG_REL,	&Analog[2],		"p2 x-axis"		),
	A("P2 Pedal",		BIT_ANALOG_REL,	&Analog[3],		"p2 y-axis"		),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"			},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"			},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"			},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"			},
};

STDINPUTINFO(Csprint)

static struct BurnInputInfo ApbInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 coin"		},
	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p2 coin"		},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 1"		},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 fire 2"		},
	A("P1 Wheel",		BIT_ANALOG_REL,	&Analog[0],		"p1 x-axis"		),
	A("P1 Pedal",		BIT_ANALOG_REL,	&Analog[1],		"p1 y-axis"		),

	{"Service",			BIT_DIGITAL,	DrvJoy1 + 5,	"service"		},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"			},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"			},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"			},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"			},
};

STDINPUTINFO(Apb)

#undef A

static struct BurnDIPInfo PaperboyDIPList[]=
{
	DIP_OFFSET(0x08)
	{0x00, 0xff, 0xff, 0xff, NULL							},
	{0x01, 0xff, 0xff, 0x00, NULL							},
	{0x02, 0xff, 0xff, 0x00, NULL							},

	{0   , 0xfe, 0   ,    2, "Service Mode"					},
	{0x00, 0x01, 0x80, 0x80, "Off"							},
	{0x00, 0x01, 0x80, 0x00, "On"							},

	{0   , 0xfe, 0   ,    4, "Coinage"						},
	{0x01, 0x01, 0x03, 0x03, "4 Coins/1 Credit"				},
	{0x01, 0x01, 0x03, 0x02, "3 Coins/1 Credit"				},
	{0x01, 0x01, 0x03, 0x01, "2 Coins/1 Credit"				},
	{0x01, 0x01, 0x03, 0x00, "1 Coin/1 Credit"				},

	{0   , 0xfe, 0   ,    4, "Right Coin"					},
	{0x01, 0x01, 0x0c, 0x00, "*1"							},
	{0x01, 0x01, 0x0c, 0x04, "*4"							},
	{0x01, 0x01, 0x0c, 0x08, "*5"							},
	{0x01, 0x01, 0x0c, 0x0c, "*6"							},

	{0   , 0xfe, 0   ,    2, "Left Coin"					},
	{0x01, 0x01, 0x10, 0x00, "*1"							},
	{0x01, 0x01, 0x10, 0x10, "*2"							},

	{0   , 0xfe, 0   ,    8, "Bonus Coins"					},
	{0x01, 0x01, 0xe0, 0x00, "None"							},
	{0x01, 0x01, 0xe0, 0x80, "1 Each 5"						},
	{0x01, 0x01, 0xe0, 0x40, "1 Each 4"						},
	{0x01, 0x01, 0xe0, 0xa0, "1 Each 3"						},
	{0x01, 0x01, 0xe0, 0x60, "2 Each 4"						},
	{0x01, 0x01, 0xe0, 0x20, "1 Each 2"						},
	{0x01, 0x01, 0xe0, 0xc0, "1 Each ?"						},
	{0x01, 0x01, 0xe0, 0xe0, "Free Play"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x02, 0x01, 0x03, 0x01, "Easy"							},
	{0x02, 0x01, 0x03, 0x02, "Medium"						},
	{0x02, 0x01, 0x03, 0x00, "Medium Hard"					},
	{0x02, 0x01, 0x03, 0x03, "Hard"							},

	{0   , 0xfe, 0   ,    4, "Bonus Life"					},
	{0x02, 0x01, 0x0c, 0x08, "10000"						},
	{0x02, 0x01, 0x0c, 0x00, "15000"						},
	{0x02, 0x01, 0x0c, 0x0c, "20000"						},
	{0x02, 0x01, 0x0c, 0x04, "None"							},

	{0   , 0xfe, 0   ,    4, "Lives"						},
	{0x02, 0x01, 0x30, 0x20, "3"							},
	{0x02, 0x01, 0x30, 0x00, "4"							},
	{0x02, 0x01, 0x30, 0x30, "5"							},
	{0x02, 0x01, 0x30, 0x10, "Infinite (Cheat)"				},
};

STDDIPINFO(Paperboy)

static struct BurnDIPInfo Drv720DIPList[]=
{
	DIP_OFFSET(0x09)
	{0x00, 0xff, 0xff, 0xff, NULL							},
	{0x01, 0xff, 0xff, 0x00, NULL							},
	{0x02, 0xff, 0xff, 0x55, NULL							},
	{0x03, 0xff, 0xff, 0x00, NULL							},

	{0   , 0xfe, 0   ,    2, "Service Mode"					},
	{0x00, 0x01, 0x80, 0x80, "Off"							},
	{0x00, 0x01, 0x80, 0x00, "On"							},

	{0   , 0xfe, 0   ,    4, "Coinage"						},
	{0x01, 0x01, 0x03, 0x03, "4 Coins/1 Credit"				},
	{0x01, 0x01, 0x03, 0x02, "3 Coins/1 Credit"				},
	{0x01, 0x01, 0x03, 0x01, "2 Coins/1 Credit"				},
	{0x01, 0x01, 0x03, 0x00, "1 Coin/1 Credit"				},

	{0   , 0xfe, 0   ,    4, "Right Coin"					},
	{0x01, 0x01, 0x0c, 0x00, "*1"							},
	{0x01, 0x01, 0x0c, 0x04, "*4"							},
	{0x01, 0x01, 0x0c, 0x08, "*5"							},
	{0x01, 0x01, 0x0c, 0x0c, "*6"							},

	{0   , 0xfe, 0   ,    2, "Left Coin"					},
	{0x01, 0x01, 0x10, 0x00, "*1"							},
	{0x01, 0x01, 0x10, 0x10, "*2"							},

	{0   , 0xfe, 0   ,    8, "Bonus Coins"					},
	{0x01, 0x01, 0xe0, 0x00, "None"							},
	{0x01, 0x01, 0xe0, 0x80, "1 Each 5"						},
	{0x01, 0x01, 0xe0, 0x40, "1 Each 4"						},
	{0x01, 0x01, 0xe0, 0xa0, "1 Each 3"						},
	{0x01, 0x01, 0xe0, 0x60, "2 Each 4"						},
	{0x01, 0x01, 0xe0, 0x20, "1 Each 2"						},
	{0x01, 0x01, 0xe0, 0xc0, "1 Each ?"						},
	{0x01, 0x01, 0xe0, 0xe0, "Free Play"					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"					},
	{0x02, 0x01, 0x03, 0x01, "3000"							},
	{0x02, 0x01, 0x03, 0x00, "5000"							},
	{0x02, 0x01, 0x03, 0x02, "8000"							},
	{0x02, 0x01, 0x03, 0x03, "12000"						},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x02, 0x01, 0x0c, 0x04, "Easy"							},
	{0x02, 0x01, 0x0c, 0x00, "Medium"						},
	{0x02, 0x01, 0x0c, 0x08, "Hard"							},
	{0x02, 0x01, 0x0c, 0x0c, "Hardest"						},

	{0   , 0xfe, 0   ,    4, "Maximum Add. A. Coins"		},
	{0x02, 0x01, 0x30, 0x10, "0"							},
	{0x02, 0x01, 0x30, 0x20, "1"							},
	{0x02, 0x01, 0x30, 0x00, "2"							},
	{0x02, 0x01, 0x30, 0x30, "3"							},

	{0   , 0xfe, 0   ,    4, "Coins Required"				},
	{0x02, 0x01, 0xc0, 0x80, "3 To Start, 2 To Continue"	},
	{0x02, 0x01, 0xc0, 0xc0, "3 To Start, 1 To Continue"	},
	{0x02, 0x01, 0xc0, 0x00, "2 To Start, 1 To Continue"	},
	{0x02, 0x01, 0xc0, 0x40, "1 To Start, 1 To Continue"	},

	{0   , 0xfe, 0   ,    2, "Controller Type"				},
	{0x03, 0x01, 0x01, 0x00, "Joystick"						},
	{0x03, 0x01, 0x01, 0x01, "Spinner"						},
};

STDDIPINFO(Drv720)

static struct BurnDIPInfo SsprintDIPList[]=
{
	DIP_OFFSET(0x0d)
	{0x00, 0xff, 0xff, 0xff, NULL							},
	{0x01, 0xff, 0xff, 0x00, NULL							},
	{0x02, 0xff, 0xff, 0x00, NULL							},

	{0   , 0xfe, 0   ,    2, "Service Mode"					},
	{0x00, 0x01, 0x80, 0x80, "Off"							},
	{0x00, 0x01, 0x80, 0x00, "On"							},

	{0   , 0xfe, 0   ,    4, "Coinage"						},
	{0x01, 0x01, 0x03, 0x03, "4 Coins/1 Credit"				},
	{0x01, 0x01, 0x03, 0x02, "3 Coins/1 Credit"				},
	{0x01, 0x01, 0x03, 0x01, "2 Coins/1 Credit"				},
	{0x01, 0x01, 0x03, 0x00, "1 Coin/1 Credit"				},

	{0   , 0xfe, 0   ,    8, "Coin Multiplier"				},
	{0x01, 0x01, 0x1c, 0x00, "*1"							},
	{0x01, 0x01, 0x1c, 0x04, "*2"							},
	{0x01, 0x01, 0x1c, 0x08, "*3"							},
	{0x01, 0x01, 0x1c, 0x0c, "*4"							},
	{0x01, 0x01, 0x1c, 0x10, "*5"							},
	{0x01, 0x01, 0x1c, 0x14, "*6"							},
	{0x01, 0x01, 0x1c, 0x18, "*7"							},
	{0x01, 0x01, 0x1c, 0x1c, "*8"							},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x02, 0x01, 0x03, 0x01, "Easy"							},
	{0x02, 0x01, 0x03, 0x00, "Medium"						},
	{0x02, 0x01, 0x03, 0x02, "Medium Hard"					},
	{0x02, 0x01, 0x03, 0x03, "Hard"							},

	{0   , 0xfe, 0   ,    4, "Obstacles"					},
	{0x02, 0x01, 0x0c, 0x04, "Easy"							},
	{0x02, 0x01, 0x0c, 0x00, "Medium"						},
	{0x02, 0x01, 0x0c, 0x08, "Medium Hard"					},
	{0x02, 0x01, 0x0c, 0x0c, "Hard"							},

	{0   , 0xfe, 0   ,    4, "Wrenches"						},
	{0x02, 0x01, 0x30, 0x10, "2"							},
	{0x02, 0x01, 0x30, 0x00, "3"							},
	{0x02, 0x01, 0x30, 0x20, "4"							},
	{0x02, 0x01, 0x30, 0x30, "5"							},
};

STDDIPINFO(Ssprint)

static struct BurnDIPInfo CsprintDIPList[]=
{
	DIP_OFFSET(0x09)
	{0x00, 0xff, 0xff, 0xff, NULL							},
	{0x01, 0xff, 0xff, 0x00, NULL							},
	{0x02, 0xff, 0xff, 0x00, NULL							},

	{0   , 0xfe, 0   ,    2, "Service Mode"					},
	{0x00, 0x01, 0x80, 0x80, "Off"							},
	{0x00, 0x01, 0x80, 0x00, "On"							},

	{0   , 0xfe, 0   ,    4, "Coinage"						},
	{0x01, 0x01, 0x03, 0x03, "4 Coins/1 Credit"				},
	{0x01, 0x01, 0x03, 0x02, "3 Coins/1 Credit"				},
	{0x01, 0x01, 0x03, 0x01, "2 Coins/1 Credit"				},
	{0x01, 0x01, 0x03, 0x00, "1 Coin/1 Credit"				},

	{0   , 0xfe, 0   ,    8, "Coin Multiplier"				},
	{0x01, 0x01, 0x1c, 0x00, "*1"							},
	{0x01, 0x01, 0x1c, 0x04, "*2"							},
	{0x01, 0x01, 0x1c, 0x08, "*3"							},
	{0x01, 0x01, 0x1c, 0x0c, "*4"							},
	{0x01, 0x01, 0x1c, 0x10, "*5"							},
	{0x01, 0x01, 0x1c, 0x14, "*6"							},
	{0x01, 0x01, 0x1c, 0x18, "*7"							},
	{0x01, 0x01, 0x1c, 0x1c, "*8"							},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x02, 0x01, 0x03, 0x01, "Easy"							},
	{0x02, 0x01, 0x03, 0x00, "Medium"						},
	{0x02, 0x01, 0x03, 0x02, "Medium Hard"					},
	{0x02, 0x01, 0x03, 0x03, "Hard"							},

	{0   , 0xfe, 0   ,    4, "Obstacles"					},
	{0x02, 0x01, 0x0c, 0x04, "Easy"							},
	{0x02, 0x01, 0x0c, 0x00, "Medium"						},
	{0x02, 0x01, 0x0c, 0x08, "Medium Hard"					},
	{0x02, 0x01, 0x0c, 0x0c, "Hard"							},

	{0   , 0xfe, 0   ,    4, "Wrenches"						},
	{0x02, 0x01, 0x30, 0x10, "2"							},
	{0x02, 0x01, 0x30, 0x00, "3"							},
	{0x02, 0x01, 0x30, 0x20, "4"							},
	{0x02, 0x01, 0x30, 0x30, "5"							},

	{0   , 0xfe, 0   ,    2, "Auto High Score Reset"		},
	{0x02, 0x01, 0x80, 0x80, "Off"							},
	{0x02, 0x01, 0x80, 0x00, "On"							},
};

STDDIPINFO(Csprint)

static struct BurnDIPInfo ApbDIPList[]=
{
	DIP_OFFSET(0x08)
	{0x00, 0xff, 0xff, 0xff, NULL							},
	{0x01, 0xff, 0xff, 0x00, NULL							},
	{0x02, 0xff, 0xff, 0x40, NULL							},

	{0   , 0xfe, 0   ,    2, "Service Mode"					},
	{0x00, 0x01, 0x80, 0x80, "Off"							},
	{0x00, 0x01, 0x80, 0x00, "On"							},

	{0   , 0xfe, 0   ,    4, "Coinage"						},
	{0x01, 0x01, 0x03, 0x03, "4 Coins/1 Credit"				},
	{0x01, 0x01, 0x03, 0x02, "3 Coins/1 Credit"				},
	{0x01, 0x01, 0x03, 0x01, "2 Coins/1 Credit"				},
	{0x01, 0x01, 0x03, 0x00, "1 Coin/1 Credit"				},

	{0   , 0xfe, 0   ,    4, "Right Coin"					},
	{0x01, 0x01, 0x0c, 0x00, "*1"							},
	{0x01, 0x01, 0x0c, 0x04, "*4"							},
	{0x01, 0x01, 0x0c, 0x08, "*5"							},
	{0x01, 0x01, 0x0c, 0x0c, "*6"							},

	{0   , 0xfe, 0   ,    2, "Left Coin"					},
	{0x01, 0x01, 0x10, 0x00, "*1"							},
	{0x01, 0x01, 0x10, 0x10, "*2"							},

	{0   , 0xfe, 0   ,    8, "Bonus Coins"					},
	{0x01, 0x01, 0xe0, 0x00, "None"							},
	{0x01, 0x01, 0xe0, 0xc0, "1 Each 6"						},
	{0x01, 0x01, 0xe0, 0xa0, "1 Each 5"						},
	{0x01, 0x01, 0xe0, 0x80, "1 Each 4"						},
	{0x01, 0x01, 0xe0, 0x60, "1 Each 3"						},
	{0x01, 0x01, 0xe0, 0x40, "1 Each 2"						},
	{0x01, 0x01, 0xe0, 0x20, "1 Each 1"						},
	{0x01, 0x01, 0xe0, 0xe0, "Free Play"					},

	{0   , 0xfe, 0   ,    2, "Attract Lights"				},
	{0x02, 0x01, 0x01, 0x01, "Off"							},
	{0x02, 0x01, 0x01, 0x00, "On"							},

	{0   , 0xfe, 0   ,    4, "Max Continues"				},
	{0x02, 0x01, 0x06, 0x02, "3"							},
	{0x02, 0x01, 0x06, 0x04, "10"							},
	{0x02, 0x01, 0x06, 0x00, "25"							},
	{0x02, 0x01, 0x06, 0x06, "199"							},

	{0   , 0xfe, 0   ,    8, "Difficulty"					},
	{0x02, 0x01, 0x38, 0x38, "Easiest"						},
	{0x02, 0x01, 0x38, 0x30, "Very_Easy"					},
	{0x02, 0x01, 0x38, 0x28, "Easy"							},
	{0x02, 0x01, 0x38, 0x00, "Medium Easy"					},
	{0x02, 0x01, 0x38, 0x20, "Medium Hard"					},
	{0x02, 0x01, 0x38, 0x10, "Hard"							},
	{0x02, 0x01, 0x38, 0x08, "Very Hard"					},
	{0x02, 0x01, 0x38, 0x18, "Hardest"						},

	{0   , 0xfe, 0   ,    4, "Coins Required"				},
	{0x02, 0x01, 0xc0, 0x80, "3 To Start, 2 To Continue"	},
	{0x02, 0x01, 0xc0, 0xc0, "3 To Start, 1 To Continue"	},
	{0x02, 0x01, 0xc0, 0x00, "2 To Start, 1 To Continue"	},
	{0x02, 0x01, 0xc0, 0x40, "1 To Start, 1 To Continue"	},
};

STDDIPINFO(Apb)

static void partial_update(); // forward

static const int sound_mhz = 1789772;
static const int main_mhz = 10000000;

static const bool sound_debug = false;
#define sounddebug(...) if (sound_debug) bprintf(0, __VA_ARGS__);

static void sync_sound()
{
	INT32 cyc = (((INT64)t11TotalCycles() * sound_mhz) / main_mhz);
	BurnTimerUpdate(cyc);
}

static void update_interrupts()
{
	t11SetIRQLine(3, (video_int_state)		? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
	t11SetIRQLine(2, (scanline_int_state)	? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
	t11SetIRQLine(1, (p2portwr_state)		? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
	t11SetIRQLine(0, (p2portrd_state)		? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static void bankswitch(INT32 offset, INT32 data)
{
	bankselect[offset] = data;

	const UINT8 bank = (data & 0x30) | ((~data & 0x03) << 2) | ((data & 0x0c) >> 2);

	t11MapMemory(DrvT11ROM + 0x10000 + (bank * 0x2000), offset ? 0x6000 : 0x4000, offset ? 0x7fff : 0x5fff, MAP_ROM);
}

static void sy2_main_write_word(UINT16 address, UINT16 data)
{
	if ((address & 0xfe00) == 0x8000) {
		SlapsticTweak((address & 0x1ff) >> 1);
		videobank = SlapsticTweak(0x1234) * 0x1000;
		return;
	}

	switch (address)
	{
		case 0x1500:
			// light, or ??
		return;

		case 0x1580:
			p2portrd_state = 0;
			update_interrupts();
		return;

		case 0x15a0:
		{
			sync_sound();
			data &= 1;
			if (m6502_in_reset && data == 0) {
				sounddebug(_T("T11 -> reset 6502\n"));
				M6502Reset();
			}
			m6502_in_reset = data & 1;
		}
		return;

		case 0x15c0:
			scanline_int_state = 0;
			update_interrupts();
		return;

		case 0x15e0:
			video_int_state = 0;
			update_interrupts();
		return;
	}

	switch (address)
	{
		case 0x1400:
		case 0x1402:
			bankswitch((address & 2) >> 1, data >> 10);
		return;

		case 0x1480:
		case 0x1482:
		case 0x1484:
		case 0x1486:
		case 0x1488:
		case 0x148a:
		case 0x148c:
		case 0x148e:
			which_adc = (address >> 1) & 3;
		return;

		case 0x1600:
			interrupt_enable = data;
		return;

		case 0x1680:
			sync_sound();
			if (cpu_to_sound_ready) {
				sounddebug(_T("soundlatch: cpu -> sound: latch over written!\n"));
			}
			cpu_to_sound = data;
			cpu_to_sound_ready = 1;
			M6502SetIRQLine(0x20, CPU_IRQSTATUS_ACK);
			t11RunEnd();
			sounddebug(_T("soundlatch: cpu -> sound   %2.2x\n"), data);
		return;

		case 0x1700:
			partial_update();
			scrollx = data >> 6;
			playfield_tile_bank[0] = (data & 0x0f) << 10;
		return;

		case 0x1780:
			partial_update();
			scrolly = data >> 6;
			scrolly_adj = scrolly;
			if (~data & 0x10) {
				bprintf(0, _T("scrolly_adj, subtract scanline mode.\n"));
				scrolly_adj = scrolly - (scanline + 1);
			}
			playfield_tile_bank[1] = (data & 0x0f) << 10;
		return;
	}

	if ((address & 0xff00) == 0x1800) {
		BurnWatchdogWrite();
		return;
	}

	if ((address & 0xe000) == 0x2000)
	{
		INT32 offset = ((address & 0x1fff) >> 1) + videobank;
		*((UINT16 *)(DrvVidRAM + (offset * 2))) = data;

		if ((offset & 0x3c00) == 0x0c00) {
			if ((offset & 0x3ff) == 0x003) {
				partial_update();
			}
			AtariMoWrite(0, offset & 0x3ff, data);
			return;
		}
		return;
	}
	
	bprintf (0, _T("WW (missed): %4.4x, %2.2x\n"), address, data);
}

static void sy2_main_write_byte(UINT16 address, UINT8 data)
{
	// 0x1800 - watchdog
	// 0x1680 - soundlatch - foward to *write_word
	// 0x1681 - soundlatch high byte, ignored
	if (address != 0x1800 && address != 0x1680 && address != 0x1681) bprintf(0, _T("wb %x  %x\n"), address,data);
	if (address != 0x1681)
		sy2_main_write_word(address & ~1, data << ((address & 1) * 8));
}

static UINT8 p_analog(INT16 a)
{
	// paperboy wants 0x80 center value, but with min/max of: 0x10, 0xf0
	// this leaves for no center value/home position because symmetry:
	// 0x80 - 0x10 = 0x70
	// 0xf0 - 0x80 = 0x70
	// usually we have min, center, max of: 0x00, 0x80, 0xff
	// 0x80 - 0x00 = 0x80  <- notice, the left (min) side has 1 more value that can be used as center
	// 0xff - 0x80 = 0x7f
	UINT8 val = ProcessAnalog(a, 0, INPUT_DEADZONE, 0xf, 0xf0);
	return d_max(val, 0x10);
}

static UINT16 sy2_main_read_word(UINT16 address)
{
	if ((address & 0xfe00) == 0x8000) {
		SlapsticTweak((address & 0x1ff) >> 1);
		videobank = SlapsticTweak(0x1234) * 0x1000;
		return *((UINT16*)(DrvT11ROM + address));
	}

	if ((address & 0xff80) == 0x1400) {
		if (is_apb) {
			switch (which_adc) {
				case 1: return 0xff00 | ~ProcessAnalog(Analog[1], 0, INPUT_LINEAR | INPUT_MIGHTBEDIGITAL | INPUT_DEADZONE, 0x00, 0x3f);
			}

		} else
		if (is_ssprint) {
			switch (which_adc) {
				case 0: return 0xff00 | ~ProcessAnalog(Analog[1], 0, INPUT_LINEAR | INPUT_MIGHTBEDIGITAL | INPUT_DEADZONE, 0x00, 0x7f);
				case 1: return 0xff00 | ~ProcessAnalog(Analog[3], 0, INPUT_LINEAR | INPUT_MIGHTBEDIGITAL | INPUT_DEADZONE, 0x00, 0x7f);
				case 2: return 0xff00 | ~ProcessAnalog(Analog[5], 0, INPUT_LINEAR | INPUT_MIGHTBEDIGITAL | INPUT_DEADZONE, 0x00, 0x7f);
			}

		}
		else
		{
			// paperboy
			switch (which_adc) {
				case 0:
					return 0xff00 | p_analog(Analog[0]); //ProcessAnalog(Analog[0], 0, INPUT_DEADZONE, 0x10, 0xf0);
				case 1:
					return 0xff00 | p_analog(Analog[1]); //ProcessAnalog(Analog[1], 0, INPUT_DEADZONE, 0x10, 0xf0);
			}
		}
		return 0xff00;
	}

	if ((address & 0xff00) == 0x1800) {
		sync_sound();
		INT32 ret = DrvInputs[1] | (DrvInputs[2] << 8);
		ret &= ~0x30;
		if (cpu_to_sound_ready) ret |= 0x20;
		if (sound_to_cpu_ready) ret |= 0x10;
		return ret;
	}

	if ((address & 0xff00) == 0x1c00) {
		sync_sound();
		if (sound_to_cpu_ready == 0) {
			sounddebug(_T("main read soundlatch from sound: nothing ready??\n"));
		}
		sounddebug(_T("--cpu eats soundlatch %x\n"), sound_to_cpu);
		p2portwr_state = 0;
		update_interrupts();
		sound_to_cpu_ready = 0;
		return sound_to_cpu | 0xff00;
	}

	if ((address & 0xe000) == 0x2000) {
		INT32 offset = ((address & 0x1fff) >> 1) + videobank;
		return *((UINT16 *)(DrvVidRAM + offset * 2));
	}

	bprintf (0, _T("RW (missed): %4.4x\n"), address);
	return 0;
}

static UINT8 sy2_main_read_byte(UINT16 address)
{
	return sy2_main_read_word(address & ~1) >> ((address & 1) * 8);
}

static void update_m6502_irq()
{
	M6502SetIRQLine(0, (timed_int) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static double res_vol(UINT8 b)
{
	double o = (b & 1) ? 0.01 : 0;
	o += (b & 2) ? 0.02 : 0;
	o += (b & 4) ? 0.05 : 0;

	if (o == 0.0) return 1.00;
	return (1.0 / o) / (50 + (1.0 / o));
}

static void mixing(UINT8 data)
{
	const double vol[3] = {
		res_vol((~data & (0x01|0x02|0x04)) >> 0),
		res_vol((~data & (     0x08|0x10)) >> 2),
		res_vol((~data & (0x20|0x40|0x80)) >> 5)
	};

	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, vol[0], BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, vol[0], BURN_SND_ROUTE_RIGHT);
	PokeySetRoute(0, vol[1], BURN_SND_ROUTE_LEFT);
	PokeySetRoute(1, vol[1], BURN_SND_ROUTE_RIGHT);
	tms5220_volume(vol[2]);

	mixbits = data;
}

static void sy2_sound_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfff0) == 0x1800 || (address & 0xfff0) == 0x1830) {
		pokey_write((address & 0x10) >> 4, address & 0xf, data);
		return;
	}

	switch (address)
	{
		case 0x1850:
		case 0x1851:
			BurnYM2151Write(address & 1, data);
		return;

		case 0x1870:
			tms5220_write(data);
		return;

		case 0x1872:
		case 0x1873:
			tms5220_wsq_w(~address & 1);
		return;

		case 0x1874:
			if (sound_to_cpu_ready) {
				sounddebug(_T("latch overrun (sound 2 cpu) uhoh!\n"));
			}
			p2portwr_state = (interrupt_enable & 2) != 0;
			update_interrupts();
			sound_to_cpu = data;
			sound_to_cpu_ready = 1;
			sounddebug(_T("sound 2 cpu: %x    -  interrupt p2portwr_state %x\n"), data, p2portwr_state);
			M6502RunEnd();
		return;

		case 0x1876:
			// coin counter 0 = (data >> 0) & 1
			// coin counter 1 = (data >> 1) & 1
		return;

		case 0x1878:
			timed_int = 0;
			update_m6502_irq();
		return;

		case 0x187a:
			mixing(data);
		return;

		case 0x187c:
			tms5220_set_frequency(20000000 / 4 / (16 - (0xc | ((data >> 5) & 1))) / 2);
		return;

		case 0x187e:
			if (data & 1 && !previous_sound_reset) {
				sounddebug(_T("m6502/sound, reset sound chips & mixing\n"));
				mixing(0);
				BurnYM2151Reset();
				tms5220_reset();
				PokeyReset();
			}
			previous_sound_reset = data & 1;
		return;
	}
	bprintf (0, _T("sw. %5.5x %2.2x\n"), address, data);
}

static UINT8 sy2_sound_read(UINT16 address)
{
	if ((address & 0xfff0) == 0x1800 || (address & 0xfff0) == 0x1830) {
		return pokey_read((address & 0x10) >> 4, address & 0xf);
	}

	switch (address)
	{
		case 0x1810:
		case 0x1811:
		case 0x1812:
		case 0x1813:
		{   // LETA port

			if (is_720) {
				switch (DrvDips[3] & 1) {
					case 0: { // joy
						enum {
							JOY_DEADZONE    = 32,
							ENCODER_COUNTS  = 144,
							HALF_ENCODER    = ENCODER_COUNTS / 2
						};

						INT32 analogx = ProcessAnalog(Analog[0], 0, INPUT_DEADZONE, 0x00, 0xff) - 0x80;
						INT32 analogy = ProcessAnalog(Analog[1], 1, INPUT_DEADZONE, 0x00, 0xff) - 0x80;

						INT32 count = last_joy_count;

						if (abs(analogx) > JOY_DEADZONE || abs(analogy) > JOY_DEADZONE)	{
							count = (INT32)(atan2((double)analogx, (double)analogy) *
											(ENCODER_COUNTS / (2.0 * M_PI)));
						}

						INT32 delta = count - last_joy_count;

						if (delta > HALF_ENCODER)
							joy_count--;
						else if (delta < -HALF_ENCODER)
							joy_count++;

						last_joy_count = count;

						//bprintf(0, _T("1810: %d\n"), (abs(count) <= 2) ? 0xff : 0x00);
						//bprintf(0, _T("1811: %x  (%d)\n"), (joy_count * ENCODER_COUNTS + count) & 0xff,(joy_count * ENCODER_COUNTS + count) & 0xff);

						if (address == 0x1810)
							return (abs(count) <= 2) ? 0xff : 0x00;

						return (joy_count * ENCODER_COUNTS + count) & 0xff;
					}
					case 1: { // spinner
						UINT16 rotate_count = BurnTrackballReadWord(1);

						INT16 diff = (INT16)(rotate_count - last_rotate_count);
						last_rotate_count = rotate_count;

						if (diff != 0) {
							INT32 step = (diff > 0) ? 1 : -1;

							for (INT32 i = 0; i != diff; i += step) {
								spinner_count += step;

								if (spinner_count < 0)
									spinner_count += 144;
								else if (spinner_count >= 144)
									spinner_count -= 144;

								if (spinner_count == 2 ||
									spinner_count == 3 ||
									spinner_count == 141 ||
									spinner_count == 142)
								{
									spinner_center_count += step;
								}
							}
						}
						if (address == 0x1810)
							return spinner_center_count & 0xff;
						else
							return rotate_count & 0xff;
					}
				}
			}

			if (is_apb) {
				switch (address & 3) {
					case 0:
						return BurnTrackballRead(0);
				}
			} else if (is_ssprint) {
				switch (address & 3) {
					case 0:
						return BurnTrackballRead(0);
					case 1:
						return BurnTrackballRead(1);
					case 2:
						return BurnTrackballRead(2);
				}
			}
			return 0xff;
		}

		case 0x1840:
		{
			UINT8 result = DrvInputs[0] & 0xf4;
			if (cpu_to_sound_ready) result ^= 0x01;
			if (sound_to_cpu_ready) result ^= 0x02;
			if (!tms5220_ready() == 0)		 result ^= 0x04;
			if ((DrvInputs[2] & 0x80) == 0)  result ^= 0x10; // service mode

			return result;
		}

		case 0x1850:
			return 0xff; // 2151 low byte, always 0xff
		case 0x1851:
			return BurnYM2151Read();

		case 0x1860:
			sounddebug(_T("--sound eats soundlatch  %x\n"), cpu_to_sound);
			p2portrd_state = (interrupt_enable & 1) != 0;
			update_interrupts();
			cpu_to_sound_ready = 0;
			M6502SetIRQLine(0x20, CPU_IRQSTATUS_NONE);
			return cpu_to_sound;

		default: {
			bprintf(0, _T("sr %x\n"), address);
		}
	}

	return 0;
}

static INT32 pokey_0_dip_read(INT32 )
{
	return DrvDips[1];
}

static INT32 pokey_1_dip_read(INT32 )
{
	return DrvDips[2];
}

static tilemap_callback( al )
{
	UINT16 data = BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvAlphaRAM + offs * 2)));

	TILE_SET_INFO(2, data & 0x3ff, data >> 13, 0);
}

static tilemap_callback( pf )
{
	UINT16 data = BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvPfRAM + offs * 2)));

	TILE_SET_INFO(0, (data & 0x3ff) | playfield_tile_bank[(data >> 10) & 1], data >> 11, TILE_GROUP((~data >> 14) & 3));
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	t11Open(0);
	bankswitch(0, 0);
	bankswitch(1, 1);
	t11Reset();
	t11Close();

	M6502Open(0);
	M6502Reset();
	M6502Close();

	SlapsticReset();

	BurnYM2151Reset();
	PokeyReset();
	tms5220_reset();
	tms5220_rsq_w(1);

	biqvoice.reset();

	AtariSlapsticReset();

	p2portrd_state = 0;
	p2portwr_state = 0;
	m6502_in_reset = 0;
	videobank = 0;
	bankselect[0] = 0;
	bankselect[1] = 0;
	scrollx = 0;
	scrolly = 0;
	playfield_tile_bank[0] = 0;
	playfield_tile_bank[1] = 0;
	which_adc = 0;
	scanline_int_state = 0;
	video_int_state = 0;
	interrupt_enable = 0;

	cpu_to_sound_ready = 0;
	sound_to_cpu_ready = 0;
	cpu_to_sound = 0;
	sound_to_cpu = 0;
	previous_sound_reset = 0;
	timed_int = 0;

	nCyclesExtra = 0;
	last_joy_count = 0x24;
	joy_count = 0x24;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvT11ROM		= Next; Next += 0x090000;
	DrvM6502ROM		= Next; Next += 0x010000;

	DrvGfxROM[0]	= Next; Next += 0x100000;
	DrvGfxROM[1]	= Next; Next += 0x200000;
	DrvGfxROM[2]	= Next; Next += 0x010000;

	DrvDefault		= Next; Next += 0x000200;

	DrvPalette		= (UINT32*)Next; Next += 0x1000 * sizeof(UINT32);

	DrvNVRAM		= Next; Next += 0x000200;

	tempsound       = (INT16*)Next; Next += 48000 * 2 * 2; // more than enough

	AllRam			= Next;

	DrvM6502RAM		= Next; Next += 0x001000;
	DrvT11RAM		= Next; Next += 0x001000;
	DrvPalRAM		= Next; Next += 0x000200;
	DrvVidRAM		= Next; Next += 0x008000;
	DrvAlphaRAM 	= DrvVidRAM + 0x0000;
	DrvMobRAM		= DrvVidRAM + 0x1800;
	DrvPfRAM		= DrvVidRAM + 0x4000;
	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode(INT32 *gfxlen)
{
	INT32 Plane0[4]  = { 0, 4, (gfxlen[0] / 2) * 8 + 0, (gfxlen[0] / 2) * 8 + 4  };
//	INT32 Plane1[4]  = { 0, 4, (gfxlen[1] / 2) * 8 + 0, (gfxlen[1] / 2) * 8 + 4  };
	INT32 Plane1[4]  = { 0, 4, RGN_FRAC(gfxlen[1], 1, 2) + 0, RGN_FRAC(gfxlen[1], 1, 2) + 4 };
	INT32 XOffs[16]  = { STEP4(0,1), STEP4(8,1), STEP4(16,1), STEP4(24,1) };
	INT32 YOffs0[8]  = { STEP8(0,16) };
	INT32 YOffs1[16] = { STEP16(0,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x100000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM[0], gfxlen[0]);

	GfxDecode(((gfxlen[0] * 8) / 4) / ( 8 *  8), 4,  8,  8, Plane0, XOffs, YOffs0, 0x080, tmp, DrvGfxROM[0]);

	// DrvGfxROM[1] needs to be inverted
	for (INT32 i = 0; i < gfxlen[1]; i++) tmp[i] = DrvGfxROM[1][i] ^ 0xff;

	GfxDecode(((gfxlen[1] * 8) / 4) / (16 * 16), 4, 16, 16, Plane1, XOffs, YOffs1, 0x200, tmp, DrvGfxROM[1]);

	memcpy (tmp, DrvGfxROM[2], 0x004000);

	GfxDecode(((gfxlen[2] * 8) / 2) / ( 8 *  8), 2,  8,  8, Plane0, XOffs, YOffs0, 0x080, tmp, DrvGfxROM[2]);

	BurnFree(tmp);

	return 0;
}


static INT32 DrvLoadRoms(INT32 *gfxlen)
{
	char* pRomName;
	struct BurnRomInfo ri;
	const INT32 offsets[32] = { 
		0x020000, 0x000000, 0x028000, 0x008000, 0x030000, 0x010000, 0x038000, 0x018000,
		0x060000, 0x040000, 0x068000, 0x048000, 0x070000, 0x050000, 0x078000, 0x058000,
		0x0a0000, 0x080000, 0x0a8000, 0x088000, 0x0b0000, 0x090000, 0x0b8000, 0x098000,
		0x0e0000, 0x0c0000, 0x0e8000, 0x0c8000, 0x0f0000, 0x0d0000, 0x0f8000, 0x0d8000
	};
	INT32 gfxoffs[2] = { 0, 0 };	
	UINT8 *pLoad[8] = { 0, DrvT11ROM + 0x08000, DrvM6502ROM, DrvGfxROM[0], DrvGfxROM[1], DrvGfxROM[2], DrvDefault };

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++)
	{
		BurnDrvGetRomInfo(&ri, i);

		INT32 type = ri.nType & 7;

		if (type == 1)
		{
			if (BurnLoadRom(pLoad[1] + 0, i + 0, 2)) return 1;
			if (BurnLoadRom(pLoad[1] + 1, i + 1, 2)) return 1;
			if ((pLoad[1] - DrvT11ROM) >= 0x10000) {
				// mirror bank data
				for (INT32 z = (ri.nLen * 2); z < 0x20000; z += (ri.nLen * 2)) {
					memmove (pLoad[1] + z, pLoad[1], (ri.nLen * 2));
				}
				pLoad[1] += 0x20000;
			} else {
				pLoad[1] += ri.nLen * 2;
			}
			i++;
			continue;
		}

		if (type == 2)
		{
			// 0.0: load 0x4000 @ 0xc000
			// 0.1: move(rom, rom + 0x4000, 0xc000)
			// repeat
			memmove (pLoad[2], pLoad[2] + ri.nLen, 0x10000 - ri.nLen);
			if (BurnLoadRom(pLoad[2] + 0x10000 - ri.nLen, i, 1)) return 1;
			continue;
		}

		if (type == 3 || type == 4)
		{
			if (ri.nType & 8) 
			{
				UINT8 *tmp = (UINT8*)BurnMalloc(0x10000);
				if (BurnLoadRom(tmp + (ri.nLen & 0x08000), i, 1)) return 1;
				memcpy (pLoad[type] + offsets[gfxoffs[type-3] + 0], tmp + 0x00000, 0x08000);
				memcpy (pLoad[type] + offsets[gfxoffs[type-3] + 1], tmp + 0x08000, 0x08000);
				gfxoffs[type-3] += 2;
				BurnFree (tmp);
				continue;
			}
			else
			{
				if (BurnLoadRom(pLoad[type], i, 1)) return 1;
				if (ri.nLen == 0x4000) {
					memcpy (pLoad[type] + 0x4000, pLoad[type], 0x4000);
					pLoad[type] += 0x4000;
				}
				pLoad[type] += ri.nLen;
				continue;
			}
		}

		if (type == 5 || type == 6)
		{
			if (BurnLoadRom(pLoad[type], i, 1)) return 1;
			pLoad[type] += ri.nLen;
			continue;
		}
	}

	gfxlen[0] = gfxoffs[0] ? (gfxoffs[0] * 0x08000) : (pLoad[3] - DrvGfxROM[0]);
	gfxlen[1] = gfxoffs[1] ? (gfxoffs[1] * 0x08000) : (pLoad[4] - DrvGfxROM[1]);
	gfxlen[2] = pLoad[5] - DrvGfxROM[2];

	bprintf (0, _T("Gfxlen: %5.5x, %5.5x, %5.5x\n"), gfxlen[0], gfxlen[1], gfxlen[2]);

	return 0;
}

static INT32 Atarisy2Init(INT32 slapstic_version, void (*pLoadCallback)())
{
	static const atarimo_desc modesc =
	{
		1,                  /* index to which gfx system */
		1,                  /* number of motion object banks */
		1,                  /* are the entries linked? */
		0,                  /* are the entries split? */
		0,                  /* render in reverse order? */
		0,                  /* render in swapped X/Y order? */
		0,                  /* does the neighbor bit affect the next object? */
		0,                  /* pixels per SLIP entry (0 for no-slip) */
		0,                  /* pixel offset for SLIPs */
		0,                  /* maximum number of links to visit/scanline (0=all) */

		0x00,               /* base palette entry */
		0x40,               /* maximum number of colors */
		15,                 /* transparent pen index */

		{{ 0,0,0,0x07f8 }}, /* mask for the link */
		{{ 0 }},            /* mask for the graphics bank */
		{{ 0,0x07ff,0,0 }}, /* mask for the code index */
		{{ 0x0007,0,0,0 }}, /* mask for the upper code index */
		{{ 0,0,0,0x3000 }}, /* mask for the color */
		{{ 0,0,0xffc0,0 }}, /* mask for the X position */
		{{ 0x7fc0,0,0,0 }}, /* mask for the Y position */
		{{ 0 }},            /* mask for the width, in tiles*/
		{{ 0,0x3800,0,0 }}, /* mask for the height, in tiles */
		{{ 0,0x4000,0,0 }}, /* mask for the horizontal flip */
		{{ 0 }},            /* mask for the vertical flip */
		{{ 0,0,0,0xc000 }}, /* mask for the priority */
		{{ 0,0x8000,0,0 }}, /* mask for the neighbor */
		{{ 0 }},            /* mask for absolute coordinates */

		{{ 0 }},            /* mask for the special value */
		0,                  /* resulting value to indicate "special" */
		0                   /* callback routine for special entries */
	};

	BurnAllocMemIndex();

	INT32 gfxlen[3] = { 0, 0, 0 };

	memset(DrvDefault, 0xff, 0x200);

	{
		if (DrvLoadRoms(gfxlen)) return 1;
		if (pLoadCallback) pLoadCallback();

		DrvGfxDecode(gfxlen);
	}

	t11Init(0x36ff >> 13, NULL);
 	t11Open(0);
 	t11MapMemory(DrvT11RAM,				0x0000, 0x0fff, MAP_RAM);
 	t11MapMemory(DrvPalRAM,				0x1000, 0x11ff, MAP_RAM);
	t11MapMemory(DrvPalRAM,				0x1200, 0x13ff, MAP_RAM);
 	t11MapMemory(DrvT11ROM + 0x8200,	0x8200, 0xffff, MAP_ROM); // 8000-81ff slapstic, in handlers
 	t11SetWriteWordHandler(sy2_main_write_word);
 	t11SetWriteByteHandler(sy2_main_write_byte);
 	t11SetReadWordHandler(sy2_main_read_word);
 	t11SetReadByteHandler(sy2_main_read_byte);
 	t11Close();

	SlapsticInit(slapstic_version);
	BurnWatchdogInit(DrvDoReset, 180);

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvM6502RAM,				0x0000, 0x0fff, MAP_RAM);
	M6502MapMemory(DrvNVRAM,				0x1000, 0x11ff, MAP_RAM);
	M6502MapMemory(DrvNVRAM,				0x1200, 0x13ff, MAP_RAM); // & mirrors..
	M6502MapMemory(DrvNVRAM,				0x1400, 0x15ff, MAP_RAM);
	M6502MapMemory(DrvNVRAM,				0x1600, 0x17ff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM + 0x4000,	0x4000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(sy2_sound_write);
	M6502SetReadHandler(sy2_sound_read);
	M6502Close();

	BurnYM2151InitBuffered(3579545, 1, NULL, 0);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.60, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.60, BURN_SND_ROUTE_RIGHT);
	BurnTimerAttachM6502(sound_mhz);

	PokeyInit(14000000/8, 2, 1.00, 1);
	PokeySetTotalCyclesCB(M6502TotalCycles);
	PokeyAllPotCallback(0, pokey_0_dip_read);
	PokeyAllPotCallback(1, pokey_1_dip_read);
	PokeySetRoute(0, 1.35, BURN_SND_ROUTE_LEFT);
	PokeySetRoute(1, 1.35, BURN_SND_ROUTE_RIGHT);

	tms5220c_init(20000000 / 32, M6502TotalCycles, 14318180 / 8);
	tms5220_volume(0.75);

	biqvoice.init(FILT_LOWPASS, nBurnSoundRate, 3400, 0.8, 0);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, pf_map_callback, 8, 8, 128, 64);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, al_map_callback, 8, 8, 64, 48);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 4,  8,  8, (gfxlen[0] * 8) / 4, 0x080, 0x07);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 4, 16, 16, (gfxlen[1] * 8) / 4, 0x000, 0x03);
	GenericTilemapSetGfx(2, DrvGfxROM[2], 2,  8,  8, (gfxlen[2] * 8) / 2, 0x040, 0x07);

	AtariMoInit(0, &modesc);

	memcpy(DrvNVRAM, DrvDefault, 0x200);

	BurnTrackballInit(2); // dials (4)

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	BurnYM2151Exit();
	tms5220_exit();
	biqvoice.exit();
	PokeyExit();
	M6502Exit();
	SlapsticExit();

	AtariMoExit();

	GenericTilesExit();

    BurnTrackballExit();

	BurnFreeMemIndex();

	is_apb = false;
	is_ssprint = false;
	is_720 = false;

	return 0;
}

static void DrvPaletteUpdate()
{
	const INT32 intensity_table[16] = {
		0, 0x7c, 0x84, 0x8d, 0x98, 0xa1, 0xa9, 0xb2, 0xc1, 0xca, 0xd2, 0xdb, 0xe6, 0xef, 0xf7, 0x100
	};

	const INT32 color_table[16] = {
		0x0, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xe, 0xf, 0xf
	};

	UINT16 *p = (UINT16*)DrvPalRAM;

	for (INT32 n = 0; n < 0x200/2; n++)
	{
		INT32 v = p[n];
		INT32 i = intensity_table[(v >>  0) & 0x0f];
		INT32 r = (color_table[(v >> 12) & 0x0f] * i) >> 4;
		INT32 g = (color_table[(v >>  8) & 0x0f] * i) >> 4;
		INT32 b = (color_table[(v >>  4) & 0x0f] * i) >> 4;

		DrvPalette[n] = BurnHighCol(r,g,b,0);
	}
}

static void copy_sprites()
{
	INT32 minx, maxx, miny, maxy;
	GenericTilesGetClip(&minx, &maxx, &miny, &maxy);

	for (INT32 y = miny; y < maxy; y++)
	{
		UINT16 *mo = BurnBitmapGetPosition(31, 0, y);
		UINT16 *pf = BurnBitmapGetPosition(0, 0, y);
		UINT8 *pri = BurnBitmapGetPrimapPosition(0, 0, y);

		for (INT32 x = minx; x < maxx; x++)
		{
			if (mo[x] != 0xffff)
			{
				int mopriority = mo[x] >> 12;

				if ((mopriority + pri[x]) & 2)
				{
					if (!(pf[x] & 0x08))
						pf[x] = mo[x] & 0xff;
				}
				else
					pf[x] = mo[x] & 0xff;

				mo[x] = 0xffff; // clear bitmap
			}
		}
	}
}

static INT32 lastline = 0;
static INT32 partialcount = 0;

static void DrvDrawBegin()
{
	DrvPaletteUpdate();

	//bprintf(2, _T("             --  new frame  (partials: %d) --\n"), partialcount);

	partialcount = 0;

	lastline = 0;

	if (pBurnDraw) BurnTransferClear();
}

// Note:
// partial updates need to update to and _including_ the current scanline. -dink april 2026
// due to how the game buffers and latches things on the hw, scrollx/y & etc are for the next line.

static void partial_update()
{
	if (!pBurnDraw) return;

	if (scanline < 0 || scanline > nScreenHeight /*|| scanline == lastline*/ || lastline > scanline) return;

	//bprintf(0, _T("%07d: partial %d - %d.    scrollx %d   scrolly %d\n"), nCurrentFrame, lastline, scanline, scrollx, scrolly_adj);
	partialcount++;

	GenericTilesSetClip(0, nScreenWidth, lastline, scanline+1);

	AtariMoRender(0);

	GenericTilemapSetScrollX(0, scrollx);
	GenericTilemapSetScrollY(0, scrolly_adj);

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0); // opaque!!
	if (nBurnLayer & 2) GenericTilemapDraw(0, pTransDraw, 1 | TMAP_SET_GROUP(1));
	if (nBurnLayer & 4) GenericTilemapDraw(0, pTransDraw, 2 | TMAP_SET_GROUP(2));
	if (nBurnLayer & 8) GenericTilemapDraw(0, pTransDraw, 3 | TMAP_SET_GROUP(3));

	if (nSpriteEnable & 1) copy_sprites();

	if (nSpriteEnable & 2) GenericTilemapDraw(1, pTransDraw, 0);

	GenericTilesClearClip();

	lastline = scanline+1;
}

// DrvDraw() - this is called at the end of the partial chain, or, if the system
// requests an update of the video when paused or something like that.
static INT32 DrvDraw()
{
	if (DrvRecalc) {
		// for mode changes while paused.  The palette update @ DrvDrawBegin()
		// is the important one :)
		DrvPaletteUpdate();
		DrvRecalc = 0;
	}

	scanline = nScreenHeight; // end partial updates.

	partial_update();

	BurnTransferCopy(DrvPalette);

	return 0;
}

// mono input on stereo signal to stereo output
static void mix_sm2s(INT16 *buf_in, INT16 *buf_out, INT32 buf_len, double volume)
{
	for (INT32 i = 0; i < buf_len; i++) {
		const INT32 a = i * 2 + 0;
		const INT32 b = i * 2 + 1;

		buf_out[a] = BURN_SND_CLIP(buf_out[a] + (buf_in[a] * volume));
		buf_out[b] = BURN_SND_CLIP(buf_out[b] + (buf_in[a] * volume));
	}
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}

		DrvInputs[2] = DrvDips[0];

		// dial-o-matic
		BurnTrackballConfig(0, AXIS_NORMAL, AXIS_NORMAL);
		BurnTrackballFrame(0, Analog[0] / ((is_apb) ? 2 : 1), Analog[2], 1, 0x1e);
		BurnTrackballFrame(1, Analog[4], 0, 1, 0x1e);
		BurnTrackballUpdate(0);
		BurnTrackballUpdate(1);
	}

	t11NewFrame();
	M6502NewFrame();

	INT32 nInterleave = 416;
	INT32 nCyclesTotal[2] = { 10000000 / 60, sound_mhz / 60 };
	INT32 nCyclesDone[2] = { nCyclesExtra, 0 };

	static int soundirqderp = 0; // dink: fix this garbage. aka make timing timing again

	DrvDrawBegin();

	scrolly_adj = scrolly; // reset scrolly_adj!

	t11Open(0);
	M6502Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		scanline = i;
		CPU_RUN(0, t11);

		if ((i & 0x1f) == 0x1f) {
			// forced partial update: every 32nd line (@ 0x3f apb glitches in some text boxes)
			partial_update();
		}


		if ((i & 0x3f) == 0 && i <= nScreenHeight) {
			if (interrupt_enable & 4) {
				//bprintf(0, _T("scanline_int!  @ %d  ena %x\n"), i, interrupt_enable);
				scanline_int_state = 1;
				update_interrupts();
			}
		}
		if (i == 384) {
			partial_update();

			if (interrupt_enable & 8) {
				video_int_state = 1;
				update_interrupts();
			}
		}

		CPU_RUN_TIMER(1);

		soundirqderp++;
		if (soundirqderp >= 103) {
			soundirqderp = 0;

			timed_int = 1;
			update_m6502_irq();
		}
	}

	M6502Close();
	t11Close();

	nCyclesExtra = nCyclesDone[0] - nCyclesTotal[0];

	if (pBurnSoundOut) {
		BurnYM2151Render(pBurnSoundOut, nBurnSoundLen);
		// render, filter & mix tms5220 (speech synth)
		memset(tempsound, 0, nBurnSoundLen*2*2);
		tms5220_update(tempsound, nBurnSoundLen);
		biqvoice.filter_buffer_mono_stereo_stream(tempsound, nBurnSoundLen);
		mix_sm2s(tempsound, pBurnSoundOut, nBurnSoundLen, 1.00);
		pokey_update(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	if (pnMin) {
		*pnMin = 0x029702;
	}

	if (nAction & ACB_VOLATILE) {
		ScanVar(AllRam, RamEnd - AllRam, "All Ram");

		t11Scan(nAction);

		M6502Scan(nAction);
		BurnYM2151Scan(nAction, pnMin);
		pokey_scan(nAction, pnMin);
		tms5220_scan(nAction, pnMin);

		AtariMoScan(nAction, pnMin);
		AtariSlapsticScan(nAction, pnMin);
		BurnWatchdogScan(nAction);
        BurnTrackballScan();

		SCAN_VAR(p2portrd_state);
		SCAN_VAR(p2portwr_state);
		SCAN_VAR(m6502_in_reset);
		SCAN_VAR(videobank);
		SCAN_VAR(bankselect);
		SCAN_VAR(scrollx);
		SCAN_VAR(scrolly);
		SCAN_VAR(playfield_tile_bank);
		SCAN_VAR(which_adc);
		SCAN_VAR(scanline_int_state);
		SCAN_VAR(video_int_state);
		SCAN_VAR(interrupt_enable);

		SCAN_VAR(cpu_to_sound_ready);
		SCAN_VAR(sound_to_cpu_ready);
		SCAN_VAR(cpu_to_sound);
		SCAN_VAR(sound_to_cpu);
		SCAN_VAR(previous_sound_reset);
		SCAN_VAR(timed_int);
		SCAN_VAR(mixbits);

		SCAN_VAR(last_joy_count);
		SCAN_VAR(joy_count);
		SCAN_VAR(last_rotate_count);
		SCAN_VAR(spinner_count);
		SCAN_VAR(spinner_center_count);

		SCAN_VAR(nCyclesExtra);
	}

	if (nAction & ACB_NVRAM) {
		ScanVar(DrvNVRAM, 0x200, "NV RAM");
	}

	if (nAction & ACB_WRITE) {
		t11Open(0);
		bankswitch(0, bankselect[0]);
		bankswitch(1, bankselect[1]);
		t11Close();
		mixing(mixbits);
	}

	return 0;
}


// Paperboy (rev 3)

static struct BurnRomInfo paperboyRomDesc[] = {
	{ "cpu_l07.rv3",			0x004000, 0x4024bb9b,  0x01 | BRF_PRG | BRF_ESS },	//  0 T11 Code
	{ "cpu_n07.rv3",			0x004000, 0x0260901a,  0x01 | BRF_PRG | BRF_ESS },	//  1 
	{ "cpu_f06.rv2",			0x004000, 0x3fea86ac,  0x01 | BRF_PRG | BRF_ESS },	//  2 
	{ "cpu_n06.rv2",			0x004000, 0x711b17ba,  0x01 | BRF_PRG | BRF_ESS },	//  3 
	{ "cpu_j06.rv1",			0x004000, 0xa754b12d,  0x01 | BRF_PRG | BRF_ESS },	//  4 
	{ "cpu_p06.rv1",			0x004000, 0x89a1ff9c,  0x01 | BRF_PRG | BRF_ESS },	//  5 
	{ "cpu_k06.rv1",			0x004000, 0x290bb034,  0x01 | BRF_PRG | BRF_ESS },	//  6 
	{ "cpu_r06.rv1",			0x004000, 0x826993de,  0x01 | BRF_PRG | BRF_ESS },	//  7 
	{ "cpu_l06.rv2",			0x004000, 0x8a754466,  0x01 | BRF_PRG | BRF_ESS },	//  8 
	{ "cpu_s06.rv2",			0x004000, 0x224209f9,  0x01 | BRF_PRG | BRF_ESS },	//  9 

	{ "cpu_a02.rv3",			0x004000, 0xba251bc4,  0x02 | BRF_PRG | BRF_ESS },	// 10 M6502 Code
	{ "cpu_b02.rv2",			0x004000, 0xe4e7a8b9,  0x02 | BRF_PRG | BRF_ESS },	// 11 
	{ "cpu_c02.rv2",			0x004000, 0xd44c2aa2,  0x02 | BRF_PRG | BRF_ESS },	// 12 

	{ "vid_a06.rv1",			0x008000, 0xb32ffddf,  0x03 | BRF_GRA },			// 13 gfx1
	{ "vid_b06.rv1",			0x004000, 0x301b849d,  0x03 | BRF_GRA },			// 14 
	{ "vid_c06.rv1",			0x008000, 0x7bb59d68,  0x03 | BRF_GRA },			// 15 
	{ "vid_d06.rv1",			0x004000, 0x1a1d4ba8,  0x03 | BRF_GRA },			// 16 

	{ "vid_l06.rv1",			0x008000, 0x067ef202,  0x04 | BRF_GRA },			// 17 gfx2
	{ "vid_k06.rv1",			0x008000, 0x76b977c4,  0x04 | BRF_GRA },			// 18 
	{ "vid_j06.rv1",			0x008000, 0x2a3cc8d0,  0x04 | BRF_GRA },			// 19 
	{ "vid_h06.rv1",			0x008000, 0x6763a321,  0x04 | BRF_GRA },			// 20 
	{ "vid_s06.rv1",			0x008000, 0x0a321b7b,  0x04 | BRF_GRA },			// 21 
	{ "vid_p06.rv1",			0x008000, 0x5bd089ee,  0x04 | BRF_GRA },			// 22 
	{ "vid_n06.rv1",			0x008000, 0xc34a517d,  0x04 | BRF_GRA },			// 23 
	{ "vid_m06.rv1",			0x008000, 0xdf723956,  0x04 | BRF_GRA },			// 24 

	{ "vid_t06.rv1",			0x002000, 0x60d7aebb,  0x05 | BRF_GRA },			// 25 gfx3

	{ "paperboy-eeprom.bin",	0x000200, 0x1bbf9b07,  0x06 | BRF_PRG | BRF_ESS },	// 26 Default EEPROM
};

STD_ROM_PICK(paperboy)
STD_ROM_FN(paperboy)

static INT32 PaperboyInit()
{
	return Atarisy2Init(105, NULL);
}

struct BurnDriver BurnDrvPaperboy = {
	"paperboy", NULL, NULL, NULL, "1984",
	"Paperboy (rev 3)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, paperboyRomInfo, paperboyRomName, NULL, NULL, NULL, NULL, PaperboyInputInfo, PaperboyDIPInfo,
	PaperboyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	512, 384, 4, 3
};

// Paperboy (rev 2)

static struct BurnRomInfo paperboyr2RomDesc[] = {
	{ "cpu_l07.rv2",			0x004000, 0x39d0a625,  0x01 | BRF_PRG | BRF_ESS },	//  0 T11 Code
	{ "cpu_n07.rv2",			0x004000, 0x3c5de588,  0x01 | BRF_PRG | BRF_ESS },	//  1 
	{ "cpu_f06.rv2",			0x004000, 0x3fea86ac,  0x01 | BRF_PRG | BRF_ESS },	//  2 
	{ "cpu_n06.rv2",			0x004000, 0x711b17ba,  0x01 | BRF_PRG | BRF_ESS },	//  3 
	{ "cpu_j06.rv1",			0x004000, 0xa754b12d,  0x01 | BRF_PRG | BRF_ESS },	//  4 
	{ "cpu_p06.rv1",			0x004000, 0x89a1ff9c,  0x01 | BRF_PRG | BRF_ESS },	//  5 
	{ "cpu_k06.rv1",			0x004000, 0x290bb034,  0x01 | BRF_PRG | BRF_ESS },	//  6 
	{ "cpu_r06.rv1",			0x004000, 0x826993de,  0x01 | BRF_PRG | BRF_ESS },	//  7 
	{ "cpu_l06.rv2",			0x004000, 0x8a754466,  0x01 | BRF_PRG | BRF_ESS },	//  8 
	{ "cpu_s06.rv2",			0x004000, 0x224209f9,  0x01 | BRF_PRG | BRF_ESS },	//  9 

	{ "cpu_a02.rv2",			0x004000, 0x4a759092,  0x02 | BRF_PRG | BRF_ESS },	// 10 M6502 Code
	{ "cpu_b02.rv2",			0x004000, 0xe4e7a8b9,  0x02 | BRF_PRG | BRF_ESS },	// 11 
	{ "cpu_c02.rv2",			0x004000, 0xd44c2aa2,  0x02 | BRF_PRG | BRF_ESS },	// 12 

	{ "vid_a06.rv1",			0x008000, 0xb32ffddf,  0x03 | BRF_GRA },			// 13 Playfield Tiles
	{ "vid_b06.rv1",			0x004000, 0x301b849d,  0x03 | BRF_GRA },			// 14 
	{ "vid_c06.rv1",			0x008000, 0x7bb59d68,  0x03 | BRF_GRA },			// 15 
	{ "vid_d06.rv1",			0x004000, 0x1a1d4ba8,  0x03 | BRF_GRA },			// 16 

	{ "vid_l06.rv1",			0x008000, 0x067ef202,  0x04 | BRF_GRA },			// 17 sprites
	{ "vid_k06.rv1",			0x008000, 0x76b977c4,  0x04 | BRF_GRA },			// 18 
	{ "vid_j06.rv1",			0x008000, 0x2a3cc8d0,  0x04 | BRF_GRA },			// 19 
	{ "vid_h06.rv1",			0x008000, 0x6763a321,  0x04 | BRF_GRA },			// 20 
	{ "vid_s06.rv1",			0x008000, 0x0a321b7b,  0x04 | BRF_GRA },			// 21 
	{ "vid_p06.rv1",			0x008000, 0x5bd089ee,  0x04 | BRF_GRA },			// 22 
	{ "vid_n06.rv1",			0x008000, 0xc34a517d,  0x04 | BRF_GRA },			// 23 
	{ "vid_m06.rv1",			0x008000, 0xdf723956,  0x04 | BRF_GRA },			// 24 

	{ "vid_t06.rv1",			0x002000, 0x60d7aebb,  0x05 | BRF_GRA },			// 25 Alpha Numeric Tiles

	{ "paperboy-eeprom.bin",	0x000200, 0x1bbf9b07,  0x06 | BRF_PRG | BRF_ESS },	// 26 Default EEPROM
};

STD_ROM_PICK(paperboyr2)
STD_ROM_FN(paperboyr2)

struct BurnDriver BurnDrvPaperboyr2 = {
	"paperboyr2", "paperboy", NULL, NULL, "1984",
	"Paperboy (rev 2)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, paperboyr2RomInfo, paperboyr2RomName, NULL, NULL, NULL, NULL, PaperboyInputInfo, PaperboyDIPInfo,
	PaperboyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	512, 384, 4, 3
};


// Paperboy (rev 1)

static struct BurnRomInfo paperboyr1RomDesc[] = {
	{ "cpu_l07.rv1",			0x004000, 0xfd87a8ee,  0x01 | BRF_PRG | BRF_ESS },	//  0 T11 Code
	{ "cpu_n07.rv1",			0x004000, 0xa997e217,  0x01 | BRF_PRG | BRF_ESS },	//  1 
	{ "cpu_f06.rv1",			0x004000, 0xe871248d,  0x01 | BRF_PRG | BRF_ESS },	//  2 
	{ "cpu_n06.rv1",			0x004000, 0x4d110e5f,  0x01 | BRF_PRG | BRF_ESS },	//  3 
	{ "cpu_j06.rv1",			0x004000, 0xa754b12d,  0x01 | BRF_PRG | BRF_ESS },	//  4 
	{ "cpu_p06.rv1",			0x004000, 0x89a1ff9c,  0x01 | BRF_PRG | BRF_ESS },	//  5 
	{ "cpu_k06.rv1",			0x004000, 0x290bb034,  0x01 | BRF_PRG | BRF_ESS },	//  6 
	{ "cpu_r06.rv1",			0x004000, 0x826993de,  0x01 | BRF_PRG | BRF_ESS },	//  7 
	{ "cpu_l06.rv1",			0x004000, 0xccbc58a6,  0x01 | BRF_PRG | BRF_ESS },	//  8 
	{ "cpu_s06.rv1",			0x004000, 0xa7f14643,  0x01 | BRF_PRG | BRF_ESS },	//  9 

	{ "cpu_a02.rv1",			0x004000, 0x5479a788,  0x02 | BRF_PRG | BRF_ESS },	// 10 M6502 Code
	{ "cpu_b02.rv1",			0x004000, 0xde4147c6,  0x02 | BRF_PRG | BRF_ESS },	// 11 
	{ "cpu_c02.rv1",			0x004000, 0xb71505fc,  0x02 | BRF_PRG | BRF_ESS },	// 12 

	{ "vid_a06.rv1",			0x008000, 0xb32ffddf,  0x03 | BRF_GRA },			// 13 Playfield Tiles
	{ "vid_b06.rv1",			0x004000, 0x301b849d,  0x03 | BRF_GRA },			// 14 
	{ "vid_c06.rv1",			0x008000, 0x7bb59d68,  0x03 | BRF_GRA },			// 15 
	{ "vid_d06.rv1",			0x004000, 0x1a1d4ba8,  0x03 | BRF_GRA },			// 16 

	{ "vid_l06.rv1",			0x008000, 0x067ef202,  0x04 | BRF_GRA },			// 17 sprites
	{ "vid_k06.rv1",			0x008000, 0x76b977c4,  0x04 | BRF_GRA },			// 18 
	{ "vid_j06.rv1",			0x008000, 0x2a3cc8d0,  0x04 | BRF_GRA },			// 19 
	{ "vid_h06.rv1",			0x008000, 0x6763a321,  0x04 | BRF_GRA },			// 20 
	{ "vid_s06.rv1",			0x008000, 0x0a321b7b,  0x04 | BRF_GRA },			// 21 
	{ "vid_p06.rv1",			0x008000, 0x5bd089ee,  0x04 | BRF_GRA },			// 22 
	{ "vid_n06.rv1",			0x008000, 0xc34a517d,  0x04 | BRF_GRA },			// 23 
	{ "vid_m06.rv1",			0x008000, 0xdf723956,  0x04 | BRF_GRA },			// 24 

	{ "vid_t06.rv1",			0x002000, 0x60d7aebb,  0x05 | BRF_GRA },			// 25 Alpha Numeric Tiles

	{ "paperboy-eeprom.bin",	0x000200, 0x1bbf9b07,  0x06 | BRF_PRG | BRF_ESS },	// 26 Default EEPROM
};

STD_ROM_PICK(paperboyr1)
STD_ROM_FN(paperboyr1)

struct BurnDriver BurnDrvPaperboyr1 = {
	"paperboyr1", "paperboy", NULL, NULL, "1984",
	"Paperboy (rev 1)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, paperboyr1RomInfo, paperboyr1RomName, NULL, NULL, NULL, NULL, PaperboyInputInfo, PaperboyDIPInfo,
	PaperboyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	512, 384, 4, 3
};


// Paperboy (prototype)

static struct BurnRomInfo paperboypRomDesc[] = {
	{ "fix-low.5p",				0x004000, 0x55a7137b,  0x01 | BRF_PRG | BRF_ESS },	//  0 T11 Code
	{ "fix-hi.5m",				0x004000, 0xe386b4f9,  0x01 | BRF_PRG | BRF_ESS },	//  1 
	{ "l-0.7t",					0x004000, 0xfbf26418,  0x01 | BRF_PRG | BRF_ESS },	//  2 
	{ "h-0.6t",					0x004000, 0xee4334ea,  0x01 | BRF_PRG | BRF_ESS },	//  3 
	{ "pagroml-1.6rs",			0x004000, 0x1414b432,  0x01 | BRF_PRG | BRF_ESS },	//  4 
	{ "h-1.5t",					0x004000, 0xee902968,  0x01 | BRF_PRG | BRF_ESS },	//  5 
	{ "pagroml-2.7rs",			0x004000, 0xbe537e48,  0x01 | BRF_PRG | BRF_ESS },	//  6 
	{ "pagromh-2.4t",			0x004000, 0x949defeb,  0x01 | BRF_PRG | BRF_ESS },	//  7 
	{ "low-3.5rs",				0x004000, 0xa0afde83,  0x01 | BRF_PRG | BRF_ESS },	//  8 
	{ "hi-3.4rs",				0x004000, 0x7a1a4d69,  0x01 | BRF_PRG | BRF_ESS },	//  9 

	{ "paptst4000.2a",			0x004000, 0xe5ee1bca,  0x02 | BRF_PRG | BRF_ESS },	// 10 M6502 Code
	{ "paptst8000.2c",			0x004000, 0xc51ebdb0,  0x02 | BRF_PRG | BRF_ESS },	// 11 
	{ "paptstc000.2g",			0x004000, 0xe663d9c2,  0x02 | BRF_PRG | BRF_ESS },	// 12 

	{ "vid_a06.rv1",			0x008000, 0xb32ffddf,  0x03 | BRF_GRA },			// 13 Playfield Tiles
	{ "vid_b06.rv1",			0x004000, 0x301b849d,  0x03 | BRF_GRA },			// 14 
	{ "vid_c06.rv1",			0x008000, 0x7bb59d68,  0x03 | BRF_GRA },			// 15 
	{ "vid_d06.rv1",			0x004000, 0x1a1d4ba8,  0x03 | BRF_GRA },			// 16 

	{ "vid_l06.rv1",			0x008000, 0x067ef202,  0x04 | BRF_GRA },			// 17 sprites
	{ "vid_k06.rv1",			0x008000, 0x76b977c4,  0x04 | BRF_GRA },			// 18 
	{ "vid_j06.rv1",			0x008000, 0x2a3cc8d0,  0x04 | BRF_GRA },			// 19 
	{ "vid_h06.rv1",			0x008000, 0x6763a321,  0x04 | BRF_GRA },			// 20 
	{ "vid_s06.rv1",			0x008000, 0x0a321b7b,  0x04 | BRF_GRA },			// 21 
	{ "vid_p06.rv1",			0x008000, 0x5bd089ee,  0x04 | BRF_GRA },			// 22 
	{ "vid_n06.rv1",			0x008000, 0xc34a517d,  0x04 | BRF_GRA },			// 23 
	{ "vid_m06.rv1",			0x008000, 0xdf723956,  0x04 | BRF_GRA },			// 24 

	{ "vid_t06.rv1",			0x002000, 0x60d7aebb,  0x05 | BRF_GRA },			// 25 Alpha Numeric Tiles
};

STD_ROM_PICK(paperboyp)
STD_ROM_FN(paperboyp)

struct BurnDriver BurnDrvPaperboyp = {
	"paperboyp", "paperboy", NULL, NULL, "1983",
	"Paperboy (prototype)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, paperboypRomInfo, paperboypRomName, NULL, NULL, NULL, NULL, PaperboyInputInfo, PaperboyDIPInfo,
	PaperboyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	512, 384, 4, 3
};


// 720 Degrees (rev 4)

static struct BurnRomInfo Drv720RomDesc[] = {
	{ "136047-3126.7lm",		0x004000, 0x43abd367,  0x01 | BRF_PRG | BRF_ESS },	//  0 T11 Code
	{ "136047-3127.7mn",		0x004000, 0x772e1e5b,  0x01 | BRF_PRG | BRF_ESS },	//  1 
	{ "136047-3128.6fh",		0x010000, 0xbf6f425b,  0x01 | BRF_PRG | BRF_ESS },	//  2 
	{ "136047-4131.6mn",		0x010000, 0x2ea8a20f,  0x01 | BRF_PRG | BRF_ESS },	//  3 
	{ "136047-1129.6hj",		0x010000, 0xeabf0b01,  0x01 | BRF_PRG | BRF_ESS },	//  4 
	{ "136047-1132.6p",			0x010000, 0xa24f333e,  0x01 | BRF_PRG | BRF_ESS },	//  5 
	{ "136047-1130.6k",			0x010000, 0x93fba845,  0x01 | BRF_PRG | BRF_ESS },	//  6 
	{ "136047-1133.6r",			0x010000, 0x53c177be,  0x01 | BRF_PRG | BRF_ESS },	//  7 

	{ "136047-2134.2a",			0x004000, 0x0db4ca28,  0x02 | BRF_PRG | BRF_ESS },	//  8 M6502 Code
	{ "136047-1135.2b",			0x004000, 0xb1f157d0,  0x02 | BRF_PRG | BRF_ESS },	//  9 
	{ "136047-2136.2cd",		0x004000, 0x00b06bec,  0x02 | BRF_PRG | BRF_ESS },	// 10 

	{ "136047-1121.6a",			0x008000, 0x7adb5f9a,  0x03 | BRF_GRA },			// 11 Playfield Tiles
	{ "136047-1122.6b",			0x008000, 0x41b60141,  0x03 | BRF_GRA },			// 12 
	{ "136047-1123.7a",			0x008000, 0x501881d5,  0x03 | BRF_GRA },			// 13 
	{ "136047-1124.7b",			0x008000, 0x096f2574,  0x03 | BRF_GRA },			// 14 
	{ "136047-1117.6c",			0x008000, 0x5a55f149,  0x03 | BRF_GRA },			// 15 
	{ "136047-1118.6d",			0x008000, 0x9bb2429e,  0x03 | BRF_GRA },			// 16 
	{ "136047-1119.7d",			0x008000, 0x8f7b20e5,  0x03 | BRF_GRA },			// 17 
	{ "136047-1120.7c",			0x008000, 0x46af6d35,  0x03 | BRF_GRA },			// 18 

	{ "136047-1109.6t",			0x010000, 0x0a46b693,  0x0c | BRF_GRA },			// 19 sprites
	{ "136047-1110.6sr",		0x010000, 0x457d7e38,  0x0c | BRF_GRA },			// 20 
	{ "136047-1111.6p",			0x010000, 0xffad0a5b,  0x0c | BRF_GRA },			// 21 
	{ "136047-1112.6n",			0x010000, 0x06664580,  0x0c | BRF_GRA },			// 22 
	{ "136047-1113.6m",			0x010000, 0x7445dc0f,  0x0c | BRF_GRA },			// 23 
	{ "136047-1114.6l",			0x010000, 0x23eaceb0,  0x0c | BRF_GRA },			// 24 
	{ "136047-1115.6kj",		0x010000, 0x0cc8de53,  0x0c | BRF_GRA },			// 25 
	{ "136047-1116.6jh",		0x010000, 0x2d8f1369,  0x0c | BRF_GRA },			// 26 
	{ "136047-1101.5t",			0x010000, 0x2ac77b80,  0x0c | BRF_GRA },			// 27 
	{ "136047-1102.5sr",		0x010000, 0xf19c3b06,  0x0c | BRF_GRA },			// 28 
	{ "136047-1103.5p",			0x010000, 0x78f9ab90,  0x0c | BRF_GRA },			// 29 
	{ "136047-1104.5n",			0x010000, 0x77ce4a7f,  0x0c | BRF_GRA },			// 30 
	{ "136047-1105.5m",			0x010000, 0xbef5a025,  0x0c | BRF_GRA },			// 31 
	{ "136047-1106.5l",			0x010000, 0x92a159c8,  0x0c | BRF_GRA },			// 32 
	{ "136047-1107.5kj",		0x010000, 0x0a94a3ef,  0x0c | BRF_GRA },			// 33 
	{ "136047-1108.5jh",		0x010000, 0x9815eda6,  0x0c | BRF_GRA },			// 34 

	{ "136047-1125.4t",			0x004000, 0x6b7e2328,  0x05 | BRF_GRA },			// 35 Alpha Numeric Tiles

	{ "720-eeprom.bin",			0x000200, 0x5d2acf11,  0x06 | BRF_PRG | BRF_ESS },	// 36 Default EEPROM
};

STD_ROM_PICK(Drv720)
STD_ROM_FN(Drv720)

static INT32 Drv720Init()
{
	is_720 = true;

	return Atarisy2Init(107, NULL);
}

struct BurnDriver BurnDrvDrv720 = {
	"720", NULL, NULL, NULL, "1986",
	"720 Degrees (rev 4)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_SPORTSMISC, 0,
	NULL, Drv720RomInfo, Drv720RomName, NULL, NULL, NULL, NULL, Drv720InputInfo, Drv720DIPInfo,
	Drv720Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	512, 384, 4, 3
};


// 720 Degrees (rev 3)

static struct BurnRomInfo Drv720r3RomDesc[] = {
	{ "136047-2126.7lm",		0x004000, 0xd07e731c,  0x01 | BRF_PRG | BRF_ESS },	//  0 T11 Code
	{ "136047-2127.7mn",		0x004000, 0x2d19116c,  0x01 | BRF_PRG | BRF_ESS },	//  1 
	{ "136047-2128.6fh",		0x010000, 0xedad0bc0,  0x01 | BRF_PRG | BRF_ESS },	//  2 
	{ "136047-3131.6mn",		0x010000, 0x704dc925,  0x01 | BRF_PRG | BRF_ESS },	//  3 
	{ "136047-1129.6hj",		0x010000, 0xeabf0b01,  0x01 | BRF_PRG | BRF_ESS },	//  4 
	{ "136047-1132.6p",			0x010000, 0xa24f333e,  0x01 | BRF_PRG | BRF_ESS },	//  5 
	{ "136047-1130.6k",			0x010000, 0x93fba845,  0x01 | BRF_PRG | BRF_ESS },	//  6 
	{ "136047-1133.6r",			0x010000, 0x53c177be,  0x01 | BRF_PRG | BRF_ESS },	//  7 

	{ "136047-1134.2a",			0x004000, 0x09a418c2,  0x02 | BRF_PRG | BRF_ESS },	//  8 M6502 Code
	{ "136047-1135.2b",			0x004000, 0xb1f157d0,  0x02 | BRF_PRG | BRF_ESS },	//  9 
	{ "136047-1136.2cd",		0x004000, 0xdad40e6d,  0x02 | BRF_PRG | BRF_ESS },	// 10 

	{ "136047-1121.6a",			0x008000, 0x7adb5f9a,  0x03 | BRF_GRA },			// 11 Playfield Tiles
	{ "136047-1122.6b",			0x008000, 0x41b60141,  0x03 | BRF_GRA },			// 12 
	{ "136047-1123.7a",			0x008000, 0x501881d5,  0x03 | BRF_GRA },			// 13 
	{ "136047-1124.7b",			0x008000, 0x096f2574,  0x03 | BRF_GRA },			// 14 
	{ "136047-1117.6c",			0x008000, 0x5a55f149,  0x03 | BRF_GRA },			// 15 
	{ "136047-1118.6d",			0x008000, 0x9bb2429e,  0x03 | BRF_GRA },			// 16 
	{ "136047-1119.7d",			0x008000, 0x8f7b20e5,  0x03 | BRF_GRA },			// 17 
	{ "136047-1120.7c",			0x008000, 0x46af6d35,  0x03 | BRF_GRA },			// 18 

	{ "136047-1109.6t",			0x010000, 0x0a46b693,  0x0c | BRF_GRA },			// 19 sprites
	{ "136047-1110.6sr",		0x010000, 0x457d7e38,  0x0c | BRF_GRA },			// 20 
	{ "136047-1111.6p",			0x010000, 0xffad0a5b,  0x0c | BRF_GRA },			// 21 
	{ "136047-1112.6n",			0x010000, 0x06664580,  0x0c | BRF_GRA },			// 22 
	{ "136047-1113.6m",			0x010000, 0x7445dc0f,  0x0c | BRF_GRA },			// 23 
	{ "136047-1114.6l",			0x010000, 0x23eaceb0,  0x0c | BRF_GRA },			// 24 
	{ "136047-1115.6kj",		0x010000, 0x0cc8de53,  0x0c | BRF_GRA },			// 25 
	{ "136047-1116.6jh",		0x010000, 0x2d8f1369,  0x0c | BRF_GRA },			// 26 
	{ "136047-1101.5t",			0x010000, 0x2ac77b80,  0x0c | BRF_GRA },			// 27 
	{ "136047-1102.5sr",		0x010000, 0xf19c3b06,  0x0c | BRF_GRA },			// 28 
	{ "136047-1103.5p",			0x010000, 0x78f9ab90,  0x0c | BRF_GRA },			// 29 
	{ "136047-1104.5n",			0x010000, 0x77ce4a7f,  0x0c | BRF_GRA },			// 30 
	{ "136047-1105.5m",			0x010000, 0xbef5a025,  0x0c | BRF_GRA },			// 31 
	{ "136047-1106.5l",			0x010000, 0x92a159c8,  0x0c | BRF_GRA },			// 32 
	{ "136047-1107.5kj",		0x010000, 0x0a94a3ef,  0x0c | BRF_GRA },			// 33 
	{ "136047-1108.5jh",		0x010000, 0x9815eda6,  0x0c | BRF_GRA },			// 34 

	{ "136047-1125.4t",			0x004000, 0x6b7e2328,  0x05 | BRF_GRA },			// 35 Alpha Numeric Tiles

	{ "720-eeprom.bin",			0x000200, 0x5d2acf11,  0x06 | BRF_PRG | BRF_ESS },	// 36 Default EEPROM
};

STD_ROM_PICK(Drv720r3)
STD_ROM_FN(Drv720r3)

struct BurnDriver BurnDrvDrv720r3 = {
	"720r3", "720", NULL, NULL, "1986",
	"720 Degrees (rev 3)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_SPORTSMISC, 0,
	NULL, Drv720r3RomInfo, Drv720r3RomName, NULL, NULL, NULL, NULL, Drv720InputInfo, Drv720DIPInfo,
	Drv720Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	512, 384, 4, 3
};


// 720 Degrees (rev 2)

static struct BurnRomInfo Drv720r2RomDesc[] = {
	{ "136047-2126.7lm",		0x004000, 0xd07e731c,  0x01 | BRF_PRG | BRF_ESS },	//  0 T11 Code
	{ "136047-2127.7mn",		0x004000, 0x2d19116c,  0x01 | BRF_PRG | BRF_ESS },	//  1 
	{ "136047-2128.6fh",		0x010000, 0xedad0bc0,  0x01 | BRF_PRG | BRF_ESS },	//  2 
	{ "136047-2131.6mn",		0x010000, 0xbfdd95a4,  0x01 | BRF_PRG | BRF_ESS },	//  3 
	{ "136047-1129.6hj",		0x010000, 0xeabf0b01,  0x01 | BRF_PRG | BRF_ESS },	//  4 
	{ "136047-1132.6p",			0x010000, 0xa24f333e,  0x01 | BRF_PRG | BRF_ESS },	//  5 
	{ "136047-1130.6k",			0x010000, 0x93fba845,  0x01 | BRF_PRG | BRF_ESS },	//  6 
	{ "136047-1133.6r",			0x010000, 0x53c177be,  0x01 | BRF_PRG | BRF_ESS },	//  7 

	{ "136047-1134.2a",			0x004000, 0x09a418c2,  0x02 | BRF_PRG | BRF_ESS },	//  8 M6502 Code
	{ "136047-1135.2b",			0x004000, 0xb1f157d0,  0x02 | BRF_PRG | BRF_ESS },	//  9 
	{ "136047-1136.2cd",		0x004000, 0xdad40e6d,  0x02 | BRF_PRG | BRF_ESS },	// 10 

	{ "136047-1121.6a",			0x008000, 0x7adb5f9a,  0x03 | BRF_GRA },			// 11 Playfield Tiles
	{ "136047-1122.6b",			0x008000, 0x41b60141,  0x03 | BRF_GRA },			// 12 
	{ "136047-1123.7a",			0x008000, 0x501881d5,  0x03 | BRF_GRA },			// 13 
	{ "136047-1124.7b",			0x008000, 0x096f2574,  0x03 | BRF_GRA },			// 14 
	{ "136047-1117.6c",			0x008000, 0x5a55f149,  0x03 | BRF_GRA },			// 15 
	{ "136047-1118.6d",			0x008000, 0x9bb2429e,  0x03 | BRF_GRA },			// 16 
	{ "136047-1119.7d",			0x008000, 0x8f7b20e5,  0x03 | BRF_GRA },			// 17 
	{ "136047-1120.7c",			0x008000, 0x46af6d35,  0x03 | BRF_GRA },			// 18 

	{ "136047-1109.6t",			0x010000, 0x0a46b693,  0x0c | BRF_GRA },			// 19 sprites
	{ "136047-1110.6sr",		0x010000, 0x457d7e38,  0x0c | BRF_GRA },			// 20 
	{ "136047-1111.6p",			0x010000, 0xffad0a5b,  0x0c | BRF_GRA },			// 21 
	{ "136047-1112.6n",			0x010000, 0x06664580,  0x0c | BRF_GRA },			// 22 
	{ "136047-1113.6m",			0x010000, 0x7445dc0f,  0x0c | BRF_GRA },			// 23 
	{ "136047-1114.6l",			0x010000, 0x23eaceb0,  0x0c | BRF_GRA },			// 24 
	{ "136047-1115.6kj",		0x010000, 0x0cc8de53,  0x0c | BRF_GRA },			// 25 
	{ "136047-1116.6jh",		0x010000, 0x2d8f1369,  0x0c | BRF_GRA },			// 26 
	{ "136047-1101.5t",			0x010000, 0x2ac77b80,  0x0c | BRF_GRA },			// 27 
	{ "136047-1102.5sr",		0x010000, 0xf19c3b06,  0x0c | BRF_GRA },			// 28 
	{ "136047-1103.5p",			0x010000, 0x78f9ab90,  0x0c | BRF_GRA },			// 29 
	{ "136047-1104.5n",			0x010000, 0x77ce4a7f,  0x0c | BRF_GRA },			// 30 
	{ "136047-1105.5m",			0x010000, 0xbef5a025,  0x0c | BRF_GRA },			// 31 
	{ "136047-1106.5l",			0x010000, 0x92a159c8,  0x0c | BRF_GRA },			// 32 
	{ "136047-1107.5kj",		0x010000, 0x0a94a3ef,  0x0c | BRF_GRA },			// 33 
	{ "136047-1108.5jh",		0x010000, 0x9815eda6,  0x0c | BRF_GRA },			// 34 

	{ "136047-1125.4t",			0x004000, 0x6b7e2328,  0x05 | BRF_GRA },			// 35 Alpha Numeric Tiles

	{ "720-eeprom.bin",			0x000200, 0x5d2acf11,  0x06 | BRF_PRG | BRF_ESS },	// 36 Default EEPROM
};

STD_ROM_PICK(Drv720r2)
STD_ROM_FN(Drv720r2)

struct BurnDriver BurnDrvDrv720r2 = {
	"720r2", "720", NULL, NULL, "1986",
	"720 Degrees (rev 2)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_SPORTSMISC, 0,
	NULL, Drv720r2RomInfo, Drv720r2RomName, NULL, NULL, NULL, NULL, Drv720InputInfo, Drv720DIPInfo,
	Drv720Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	512, 384, 4, 3
};


// 720 Degrees (rev 1)

static struct BurnRomInfo Drv720r1RomDesc[] = {
	{ "136047-1126.7lm",		0x004000, 0xf0ef298a,  0x01 | BRF_PRG | BRF_ESS },	//  0 T11 Code
	{ "136047-1127.7mn",		0x004000, 0x57e49398,  0x01 | BRF_PRG | BRF_ESS },	//  1 
	{ "136047-1128.6fh",		0x010000, 0x2884dcff,  0x01 | BRF_PRG | BRF_ESS },	//  2 
	{ "136047-1131.6mn",		0x010000, 0x94c8195e,  0x01 | BRF_PRG | BRF_ESS },	//  3 
	{ "136047-1129.6hj",		0x010000, 0xeabf0b01,  0x01 | BRF_PRG | BRF_ESS },	//  4 
	{ "136047-1132.6p",			0x010000, 0xa24f333e,  0x01 | BRF_PRG | BRF_ESS },	//  5 
	{ "136047-1130.6k",			0x010000, 0x93fba845,  0x01 | BRF_PRG | BRF_ESS },	//  6 
	{ "136047-1133.6r",			0x010000, 0x53c177be,  0x01 | BRF_PRG | BRF_ESS },	//  7 

	{ "136047-1134.2a",			0x004000, 0x09a418c2,  0x02 | BRF_PRG | BRF_ESS },	//  8 M6502 Code
	{ "136047-1135.2b",			0x004000, 0xb1f157d0,  0x02 | BRF_PRG | BRF_ESS },	//  9 
	{ "136047-1136.2cd",		0x004000, 0xdad40e6d,  0x02 | BRF_PRG | BRF_ESS },	// 10 

	{ "136047-1121.6a",			0x008000, 0x7adb5f9a,  0x03 | BRF_GRA },			// 11 Playfield Tiles
	{ "136047-1122.6b",			0x008000, 0x41b60141,  0x03 | BRF_GRA },			// 12 
	{ "136047-1123.7a",			0x008000, 0x501881d5,  0x03 | BRF_GRA },			// 13 
	{ "136047-1124.7b",			0x008000, 0x096f2574,  0x03 | BRF_GRA },			// 14 
	{ "136047-1117.6c",			0x008000, 0x5a55f149,  0x03 | BRF_GRA },			// 15 
	{ "136047-1118.6d",			0x008000, 0x9bb2429e,  0x03 | BRF_GRA },			// 16 
	{ "136047-1119.7d",			0x008000, 0x8f7b20e5,  0x03 | BRF_GRA },			// 17 
	{ "136047-1120.7c",			0x008000, 0x46af6d35,  0x03 | BRF_GRA },			// 18 

	{ "136047-1109.6t",			0x010000, 0x0a46b693,  0x0c | BRF_GRA },			// 19 sprites
	{ "136047-1110.6sr",		0x010000, 0x457d7e38,  0x0c | BRF_GRA },			// 20 
	{ "136047-1111.6p",			0x010000, 0xffad0a5b,  0x0c | BRF_GRA },			// 21 
	{ "136047-1112.6n",			0x010000, 0x06664580,  0x0c | BRF_GRA },			// 22 
	{ "136047-1113.6m",			0x010000, 0x7445dc0f,  0x0c | BRF_GRA },			// 23 
	{ "136047-1114.6l",			0x010000, 0x23eaceb0,  0x0c | BRF_GRA },			// 24 
	{ "136047-1115.6kj",		0x010000, 0x0cc8de53,  0x0c | BRF_GRA },			// 25 
	{ "136047-1116.6jh",		0x010000, 0x2d8f1369,  0x0c | BRF_GRA },			// 26 
	{ "136047-1101.5t",			0x010000, 0x2ac77b80,  0x0c | BRF_GRA },			// 27 
	{ "136047-1102.5sr",		0x010000, 0xf19c3b06,  0x0c | BRF_GRA },			// 28 
	{ "136047-1103.5p",			0x010000, 0x78f9ab90,  0x0c | BRF_GRA },			// 29 
	{ "136047-1104.5n",			0x010000, 0x77ce4a7f,  0x0c | BRF_GRA },			// 30 
	{ "136047-1105.5m",			0x010000, 0xbef5a025,  0x0c | BRF_GRA },			// 31 
	{ "136047-1106.5l",			0x010000, 0x92a159c8,  0x0c | BRF_GRA },			// 32 
	{ "136047-1107.5kj",		0x010000, 0x0a94a3ef,  0x0c | BRF_GRA },			// 33 
	{ "136047-1108.5jh",		0x010000, 0x9815eda6,  0x0c | BRF_GRA },			// 34 

	{ "136047-1125.4t",			0x004000, 0x6b7e2328,  0x05 | BRF_GRA },			// 35 Alpha Numeric Tiles

	{ "720-eeprom.bin",			0x000200, 0x5d2acf11,  0x06 | BRF_PRG | BRF_ESS },	// 36 Default EEPROM
};

STD_ROM_PICK(Drv720r1)
STD_ROM_FN(Drv720r1)

struct BurnDriver BurnDrvDrv720r1 = {
	"720r1", "720", NULL, NULL, "1986",
	"720 Degrees (rev 1)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_SPORTSMISC, 0,
	NULL, Drv720r1RomInfo, Drv720r1RomName, NULL, NULL, NULL, NULL, Drv720InputInfo, Drv720DIPInfo,
	Drv720Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	512, 384, 4, 3
};


// 720 Degrees (German, rev 2)

static struct BurnRomInfo Drv720gRomDesc[] = {
	{ "136047-3226.7lm",		0x004000, 0x472be9aa,  0x01 | BRF_PRG | BRF_ESS },	//  0 T11 Code
	{ "136047-2227.7mn",		0x004000, 0xc628fcc9,  0x01 | BRF_PRG | BRF_ESS },	//  1 
	{ "136047-3228.6fh",		0x010000, 0x10bbbce7,  0x01 | BRF_PRG | BRF_ESS },	//  2 
	{ "136047-4231.6mn",		0x010000, 0xc29188b0,  0x01 | BRF_PRG | BRF_ESS },	//  3 
	{ "136047-1129.6hj",		0x010000, 0xeabf0b01,  0x01 | BRF_PRG | BRF_ESS },	//  4 
	{ "136047-1132.6p",			0x010000, 0xa24f333e,  0x01 | BRF_PRG | BRF_ESS },	//  5 
	{ "136047-1130.6k",			0x010000, 0x93fba845,  0x01 | BRF_PRG | BRF_ESS },	//  6 
	{ "136047-1133.6r",			0x010000, 0x53c177be,  0x01 | BRF_PRG | BRF_ESS },	//  7 

	{ "136047-2134.2a",			0x004000, 0x0db4ca28,  0x02 | BRF_PRG | BRF_ESS },	//  8 M6502 Code
	{ "136047-1135.2b",			0x004000, 0xb1f157d0,  0x02 | BRF_PRG | BRF_ESS },	//  9 
	{ "136047-2136.2cd",		0x004000, 0x00b06bec,  0x02 | BRF_PRG | BRF_ESS },	// 10 

	{ "136047-1121.6a",			0x008000, 0x7adb5f9a,  0x03 | BRF_GRA },			// 11 Playfield Tiles
	{ "136047-1122.6b",			0x008000, 0x41b60141,  0x03 | BRF_GRA },			// 12 
	{ "136047-1123.7a",			0x008000, 0x501881d5,  0x03 | BRF_GRA },			// 13 
	{ "136047-1124.7b",			0x008000, 0x096f2574,  0x03 | BRF_GRA },			// 14 
	{ "136047-1117.6c",			0x008000, 0x5a55f149,  0x03 | BRF_GRA },			// 15 
	{ "136047-1118.6d",			0x008000, 0x9bb2429e,  0x03 | BRF_GRA },			// 16 
	{ "136047-1119.7d",			0x008000, 0x8f7b20e5,  0x03 | BRF_GRA },			// 17 
	{ "136047-1120.7c",			0x008000, 0x46af6d35,  0x03 | BRF_GRA },			// 18 

	{ "136047-1109.6t",			0x010000, 0x0a46b693,  0x0c | BRF_GRA },			// 19 sprites
	{ "136047-1110.6sr",		0x010000, 0x457d7e38,  0x0c | BRF_GRA },			// 20 
	{ "136047-1111.6p",			0x010000, 0xffad0a5b,  0x0c | BRF_GRA },			// 21 
	{ "136047-1112.6n",			0x010000, 0x06664580,  0x0c | BRF_GRA },			// 22 
	{ "136047-1113.6m",			0x010000, 0x7445dc0f,  0x0c | BRF_GRA },			// 23 
	{ "136047-1114.6l",			0x010000, 0x23eaceb0,  0x0c | BRF_GRA },			// 24 
	{ "136047-1115.6kj",		0x010000, 0x0cc8de53,  0x0c | BRF_GRA },			// 25 
	{ "136047-1116.6jh",		0x010000, 0x2d8f1369,  0x0c | BRF_GRA },			// 26 
	{ "136047-1101.5t",			0x010000, 0x2ac77b80,  0x0c | BRF_GRA },			// 27 
	{ "136047-1102.5sr",		0x010000, 0xf19c3b06,  0x0c | BRF_GRA },			// 28 
	{ "136047-1103.5p",			0x010000, 0x78f9ab90,  0x0c | BRF_GRA },			// 29 
	{ "136047-1104.5n",			0x010000, 0x77ce4a7f,  0x0c | BRF_GRA },			// 30 
	{ "136047-1105.5m",			0x010000, 0xbef5a025,  0x0c | BRF_GRA },			// 31 
	{ "136047-1106.5l",			0x010000, 0x92a159c8,  0x0c | BRF_GRA },			// 32 
	{ "136047-1107.5kj",		0x010000, 0x0a94a3ef,  0x0c | BRF_GRA },			// 33 
	{ "136047-1108.5jh",		0x010000, 0x9815eda6,  0x0c | BRF_GRA },			// 34 

	{ "136047-1225.4t",			0x004000, 0x264eda88,  0x05 | BRF_GRA },			// 35 Alpha Numeric Tiles

	{ "720-eeprom.bin",			0x000200, 0x5d2acf11,  0x06 | BRF_PRG | BRF_ESS },	// 36 Default EEPROM
};

STD_ROM_PICK(Drv720g)
STD_ROM_FN(Drv720g)

struct BurnDriver BurnDrvDrv720g = {
	"720g", "720", NULL, NULL, "1986",
	"720 Degrees (German, rev 2)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_SPORTSMISC, 0,
	NULL, Drv720gRomInfo, Drv720gRomName, NULL, NULL, NULL, NULL, Drv720InputInfo, Drv720DIPInfo,
	Drv720Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	512, 384, 4, 3
};


// 720 Degrees (German, rev 1)

static struct BurnRomInfo Drv720gr1RomDesc[] = {
	{ "136047-2226.7lm",		0x004000, 0xbbe90b2a,  0x01 | BRF_PRG | BRF_ESS },	//  0 T11 Code
	{ "136047-2227.7mn",		0x004000, 0xc628fcc9,  0x01 | BRF_PRG | BRF_ESS },	//  1 
	{ "136047-2228.6fh",		0x010000, 0xa115aa94,  0x01 | BRF_PRG | BRF_ESS },	//  2 
	{ "136047-3231.6mn",		0x010000, 0xb704e865,  0x01 | BRF_PRG | BRF_ESS },	//  3 
	{ "136047-1129.6hj",		0x010000, 0xeabf0b01,  0x01 | BRF_PRG | BRF_ESS },	//  4 
	{ "136047-1132.6p",			0x010000, 0xa24f333e,  0x01 | BRF_PRG | BRF_ESS },	//  5 
	{ "136047-1130.6k",			0x010000, 0x93fba845,  0x01 | BRF_PRG | BRF_ESS },	//  6 
	{ "136047-1133.6r",			0x010000, 0x53c177be,  0x01 | BRF_PRG | BRF_ESS },	//  7 

	{ "136047-1134.2a",			0x004000, 0x09a418c2,  0x02 | BRF_PRG | BRF_ESS },	//  8 M6502 Code
	{ "136047-1135.2b",			0x004000, 0xb1f157d0,  0x02 | BRF_PRG | BRF_ESS },	//  9 
	{ "136047-1136.2cd",		0x004000, 0xdad40e6d,  0x02 | BRF_PRG | BRF_ESS },	// 10 

	{ "136047-1121.6a",			0x008000, 0x7adb5f9a,  0x03 | BRF_GRA },			// 11 Playfield Tiles
	{ "136047-1122.6b",			0x008000, 0x41b60141,  0x03 | BRF_GRA },			// 12 
	{ "136047-1123.7a",			0x008000, 0x501881d5,  0x03 | BRF_GRA },			// 13 
	{ "136047-1124.7b",			0x008000, 0x096f2574,  0x03 | BRF_GRA },			// 14 
	{ "136047-1117.6c",			0x008000, 0x5a55f149,  0x03 | BRF_GRA },			// 15 
	{ "136047-1118.6d",			0x008000, 0x9bb2429e,  0x03 | BRF_GRA },			// 16 
	{ "136047-1119.7d",			0x008000, 0x8f7b20e5,  0x03 | BRF_GRA },			// 17 
	{ "136047-1120.7c",			0x008000, 0x46af6d35,  0x03 | BRF_GRA },			// 18 

	{ "136047-1109.6t",			0x010000, 0x0a46b693,  0x0c | BRF_GRA },			// 19 sprites
	{ "136047-1110.6sr",		0x010000, 0x457d7e38,  0x0c | BRF_GRA },			// 20 
	{ "136047-1111.6p",			0x010000, 0xffad0a5b,  0x0c | BRF_GRA },			// 21 
	{ "136047-1112.6n",			0x010000, 0x06664580,  0x0c | BRF_GRA },			// 22 
	{ "136047-1113.6m",			0x010000, 0x7445dc0f,  0x0c | BRF_GRA },			// 23 
	{ "136047-1114.6l",			0x010000, 0x23eaceb0,  0x0c | BRF_GRA },			// 24 
	{ "136047-1115.6kj",		0x010000, 0x0cc8de53,  0x0c | BRF_GRA },			// 25 
	{ "136047-1116.6jh",		0x010000, 0x2d8f1369,  0x0c | BRF_GRA },			// 26 
	{ "136047-1101.5t",			0x010000, 0x2ac77b80,  0x0c | BRF_GRA },			// 27 
	{ "136047-1102.5sr",		0x010000, 0xf19c3b06,  0x0c | BRF_GRA },			// 28 
	{ "136047-1103.5p",			0x010000, 0x78f9ab90,  0x0c | BRF_GRA },			// 29 
	{ "136047-1104.5n",			0x010000, 0x77ce4a7f,  0x0c | BRF_GRA },			// 30 
	{ "136047-1105.5m",			0x010000, 0xbef5a025,  0x0c | BRF_GRA },			// 31 
	{ "136047-1106.5l",			0x010000, 0x92a159c8,  0x0c | BRF_GRA },			// 32 
	{ "136047-1107.5kj",		0x010000, 0x0a94a3ef,  0x0c | BRF_GRA },			// 33 
	{ "136047-1108.5jh",		0x010000, 0x9815eda6,  0x0c | BRF_GRA },			// 34 

	{ "136047-1225.4t",			0x004000, 0x264eda88,  0x05 | BRF_GRA },			// 35 Alpha Numeric Tiles

	{ "720-eeprom.bin",			0x000200, 0x5d2acf11,  0x06 | BRF_PRG | BRF_ESS },	// 36 Default EEPROM
};

STD_ROM_PICK(Drv720gr1)
STD_ROM_FN(Drv720gr1)

struct BurnDriver BurnDrvDrv720gr1 = {
	"720gr1", "720", NULL, NULL, "1986",
	"720 Degrees (German, rev 1)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_SPORTSMISC, 0,
	NULL, Drv720gr1RomInfo, Drv720gr1RomName, NULL, NULL, NULL, NULL, Drv720InputInfo, Drv720DIPInfo,
	Drv720Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	512, 384, 4, 3
};


// Super Sprint (rev 4)

static struct BurnRomInfo ssprintRomDesc[] = {
	{ "136042-330.7l",			0x004000, 0xee312027,  0x01 | BRF_PRG | BRF_ESS },	//  0 T11 Code
	{ "136042-331.7n",			0x004000, 0x2ef15354,  0x01 | BRF_PRG | BRF_ESS },	//  1 
	{ "136042-329.6f",			0x008000, 0xed1d6205,  0x01 | BRF_PRG | BRF_ESS },	//  2 
	{ "136042-325.6n",			0x008000, 0xaecaa2bf,  0x01 | BRF_PRG | BRF_ESS },	//  3 
	{ "136042-127.6k",			0x008000, 0xde6c4db9,  0x01 | BRF_PRG | BRF_ESS },	//  4 
	{ "136042-123.6r",			0x008000, 0xaff23b5a,  0x01 | BRF_PRG | BRF_ESS },	//  5 
	{ "136042-126.6l",			0x008000, 0x92f5392c,  0x01 | BRF_PRG | BRF_ESS },	//  6 
	{ "136042-122.6s",			0x008000, 0x0381f362,  0x01 | BRF_PRG | BRF_ESS },	//  7 

	{ "136042-419.2bc",			0x004000, 0xb277915a,  0x02 | BRF_PRG | BRF_ESS },	//  8 M6502 Code
	{ "136042-420.2d",			0x004000, 0x170b2c53,  0x02 | BRF_PRG | BRF_ESS },	//  9 

	{ "136042-105.6a",			0x010000, 0x911499fe,  0x0b | BRF_GRA },			// 10 Playfield Tiles
	{ "136042-106.6b",			0x008000, 0xa39b25ed,  0x0b | BRF_GRA },			// 11 
	{ "136042-101.7a",			0x010000, 0x6d015c72,  0x0b | BRF_GRA },			// 12 
	{ "136042-102.7b",			0x008000, 0x54e21f0a,  0x0b | BRF_GRA },			// 13 
	{ "136042-107.6c",			0x010000, 0xb7ded658,  0x0b | BRF_GRA },			// 14 
	{ "136042-108.6de",			0x008000, 0x4a804a4c,  0x0b | BRF_GRA },			// 15 
	{ "136042-104.7de",			0x010000, 0x339644ed,  0x0b | BRF_GRA },			// 16 
	{ "136042-103.7c",			0x008000, 0x64d473a8,  0x0b | BRF_GRA },			// 17 

	{ "136042-113.6l",			0x008000, 0xf869b0fc,  0x04 | BRF_GRA },			// 18 sprites
	{ "136042-112.6k",			0x008000, 0xabcbc114,  0x04 | BRF_GRA },			// 19 
	{ "136042-110.6jh",			0x008000, 0x9e91e734,  0x04 | BRF_GRA },			// 20 
	{ "136042-109.6fh",			0x008000, 0x3a051f36,  0x04 | BRF_GRA },			// 21 
	{ "136042-117.6rs",			0x008000, 0xb15c1b90,  0x04 | BRF_GRA },			// 22 
	{ "136042-116.6pr",			0x008000, 0x1dcdd5aa,  0x04 | BRF_GRA },			// 23 
	{ "136042-115.6n",			0x008000, 0xfb5677d9,  0x04 | BRF_GRA },			// 24 
	{ "136042-114.6m",			0x008000, 0x35e70a8d,  0x04 | BRF_GRA },			// 25 

	{ "136042-118.6t",			0x004000, 0x8489d113,  0x05 | BRF_GRA },			// 26 Alpha Numeric Tiles

	{ "ssprint-eeprom.bin",		0x000200, 0x9301ed27,  0x06 | BRF_PRG | BRF_ESS },	// 27 Default EEPROM
};

STD_ROM_PICK(ssprint)
STD_ROM_FN(ssprint)

static void SsprintCallback()
{
	memmove (DrvT11ROM + 0x50000, DrvT11ROM + 0x30000, 0x40000);
	memset (DrvT11ROM + 0x30000, 0, 0x20000);
}

static INT32 SsprintInit()
{
	is_ssprint = true;

	return Atarisy2Init(108, SsprintCallback);
}

struct BurnDriver BurnDrvSsprint = {
	"ssprint", NULL, NULL, NULL, "1986",
	"Super Sprint (rev 4)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 3, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, ssprintRomInfo, ssprintRomName, NULL, NULL, NULL, NULL, SsprintInputInfo, SsprintDIPInfo,
	SsprintInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	512, 384, 4, 3
};


// Super Sprint (Spanish)

static struct BurnRomInfo ssprintsRomDesc[] = {
	{ "136042-138.7l",			0x004000, 0x234a7c65,  0x01 | BRF_PRG | BRF_ESS },	//  0 T11 Code
	{ "136042-139.7n",			0x004000, 0x7652a461,  0x01 | BRF_PRG | BRF_ESS },	//  1 
	{ "136042-137.6f",			0x008000, 0xfa4c7e9d,  0x01 | BRF_PRG | BRF_ESS },	//  2 
	{ "136042-136.6n",			0x008000, 0x7c20a249,  0x01 | BRF_PRG | BRF_ESS },	//  3 
	{ "136042-127.6k",			0x008000, 0xde6c4db9,  0x01 | BRF_PRG | BRF_ESS },	//  4 
	{ "136042-123.6r",			0x008000, 0xaff23b5a,  0x01 | BRF_PRG | BRF_ESS },	//  5 
	{ "136042-126.6l",			0x008000, 0x92f5392c,  0x01 | BRF_PRG | BRF_ESS },	//  6 
	{ "136042-122.6s",			0x008000, 0x0381f362,  0x01 | BRF_PRG | BRF_ESS },	//  7 

	{ "136042-119.2bc",			0x004000, 0x0c810231,  0x02 | BRF_PRG | BRF_ESS },	//  8 M6502 Code
	{ "136042-120.2d",			0x004000, 0x647b7481,  0x02 | BRF_PRG | BRF_ESS },	//  9 

	{ "136042-105.6a",			0x010000, 0x911499fe,  0x0b | BRF_GRA },			// 10 Playfield Tiles
	{ "136042-106.6b",			0x008000, 0xa39b25ed,  0x0b | BRF_GRA },			// 11 
	{ "136042-101.7a",			0x010000, 0x6d015c72,  0x0b | BRF_GRA },			// 12 
	{ "136042-102.7b",			0x008000, 0x54e21f0a,  0x0b | BRF_GRA },			// 13 
	{ "136042-107.6c",			0x010000, 0xb7ded658,  0x0b | BRF_GRA },			// 14 
	{ "136042-108.6de",			0x008000, 0x4a804a4c,  0x0b | BRF_GRA },			// 15 
	{ "136042-104.7de",			0x010000, 0x339644ed,  0x0b | BRF_GRA },			// 16 
	{ "136042-103.7c",			0x008000, 0x64d473a8,  0x0b | BRF_GRA },			// 17 

	{ "136042-113.6l",			0x008000, 0xf869b0fc,  0x04 | BRF_GRA },			// 18 sprites
	{ "136042-112.6k",			0x008000, 0xabcbc114,  0x04 | BRF_GRA },			// 19 
	{ "136042-110.6jh",			0x008000, 0x9e91e734,  0x04 | BRF_GRA },			// 20 
	{ "136042-109.6fh",			0x008000, 0x3a051f36,  0x04 | BRF_GRA },			// 21 
	{ "136042-117.6rs",			0x008000, 0xb15c1b90,  0x04 | BRF_GRA },			// 22 
	{ "136042-116.6pr",			0x008000, 0x1dcdd5aa,  0x04 | BRF_GRA },			// 23 
	{ "136042-115.6n",			0x008000, 0xfb5677d9,  0x04 | BRF_GRA },			// 24 
	{ "136042-114.6m",			0x008000, 0x35e70a8d,  0x04 | BRF_GRA },			// 25 

	{ "136042-218.6t",			0x004000, 0x8e500be1,  0x05 | BRF_GRA },			// 26 Alpha Numeric Tiles

	{ "ssprint1-eeprom.bin",	0x000200, 0xed263888,  0x06 | BRF_PRG | BRF_ESS },	// 27 Default EEPROM
};

STD_ROM_PICK(ssprints)
STD_ROM_FN(ssprints)

struct BurnDriver BurnDrvSsprints = {
	"ssprints", "ssprint", NULL, NULL, "1986",
	"Super Sprint (Spanish)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 3, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, ssprintsRomInfo, ssprintsRomName, NULL, NULL, NULL, NULL, SsprintInputInfo, SsprintDIPInfo,
	SsprintInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	512, 384, 4, 3
};


// Super Sprint (French)

static struct BurnRomInfo ssprintfRomDesc[] = {
	{ "136042-134.7l",			0x004000, 0xb7757b44,  0x01 | BRF_PRG | BRF_ESS },	//  0 T11 Code
	{ "136042-135.7n",			0x004000, 0x4fc132ba,  0x01 | BRF_PRG | BRF_ESS },	//  1 
	{ "136042-133.6f",			0x008000, 0x0b9f89da,  0x01 | BRF_PRG | BRF_ESS },	//  2 
	{ "136042-132.6n",			0x008000, 0xfe02509d,  0x01 | BRF_PRG | BRF_ESS },	//  3 
	{ "136042-127.6k",			0x008000, 0xde6c4db9,  0x01 | BRF_PRG | BRF_ESS },	//  4 
	{ "136042-123.6r",			0x008000, 0xaff23b5a,  0x01 | BRF_PRG | BRF_ESS },	//  5 
	{ "136042-126.6l",			0x008000, 0x92f5392c,  0x01 | BRF_PRG | BRF_ESS },	//  6 
	{ "136042-122.6s",			0x008000, 0x0381f362,  0x01 | BRF_PRG | BRF_ESS },	//  7 

	{ "136042-119.2bc",			0x004000, 0x0c810231,  0x02 | BRF_PRG | BRF_ESS },	//  8 M6502 Code
	{ "136042-120.2d",			0x004000, 0x647b7481,  0x02 | BRF_PRG | BRF_ESS },	//  9 

	{ "136042-105.6a",			0x010000, 0x911499fe,  0x0b | BRF_GRA },			// 10 Playfield Tiles
	{ "136042-106.6b",			0x008000, 0xa39b25ed,  0x0b | BRF_GRA },			// 11 
	{ "136042-101.7a",			0x010000, 0x6d015c72,  0x0b | BRF_GRA },			// 12 
	{ "136042-102.7b",			0x008000, 0x54e21f0a,  0x0b | BRF_GRA },			// 13 
	{ "136042-107.6c",			0x010000, 0xb7ded658,  0x0b | BRF_GRA },			// 14 
	{ "136042-108.6de",			0x008000, 0x4a804a4c,  0x0b | BRF_GRA },			// 15 
	{ "136042-104.7de",			0x010000, 0x339644ed,  0x0b | BRF_GRA },			// 16 
	{ "136042-103.7c",			0x008000, 0x64d473a8,  0x0b | BRF_GRA },			// 17 

	{ "136042-113.6l",			0x008000, 0xf869b0fc,  0x04 | BRF_GRA },			// 18 sprites
	{ "136042-112.6k",			0x008000, 0xabcbc114,  0x04 | BRF_GRA },			// 19 
	{ "136042-110.6jh",			0x008000, 0x9e91e734,  0x04 | BRF_GRA },			// 20 
	{ "136042-109.6fh",			0x008000, 0x3a051f36,  0x04 | BRF_GRA },			// 21 
	{ "136042-117.6rs",			0x008000, 0xb15c1b90,  0x04 | BRF_GRA },			// 22 
	{ "136042-116.6pr",			0x008000, 0x1dcdd5aa,  0x04 | BRF_GRA },			// 23 
	{ "136042-115.6n",			0x008000, 0xfb5677d9,  0x04 | BRF_GRA },			// 24 
	{ "136042-114.6m",			0x008000, 0x35e70a8d,  0x04 | BRF_GRA },			// 25 

	{ "136042-218.6t",			0x004000, 0x8e500be1,  0x05 | BRF_GRA },			// 26 Alpha Numeric Tiles

	{ "ssprint1-eeprom.bin",	0x000200, 0xed263888,  0x06 | BRF_PRG | BRF_ESS },	// 27 Default EEPROM
};

STD_ROM_PICK(ssprintf)
STD_ROM_FN(ssprintf)

struct BurnDriver BurnDrvSsprintf = {
	"ssprintf", "ssprint", NULL, NULL, "1986",
	"Super Sprint (French)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 3, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, ssprintfRomInfo, ssprintfRomName, NULL, NULL, NULL, NULL, SsprintInputInfo, SsprintDIPInfo,
	SsprintInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	512, 384, 4, 3
};


// Super Sprint (German, rev 2)

static struct BurnRomInfo ssprintgRomDesc[] = {
	{ "136042-430.7l",			0x004000, 0xc21df5f5,  0x01 | BRF_PRG | BRF_ESS },	//  0 T11 Code
	{ "136042-431.7n",			0x004000, 0x5880fc58,  0x01 | BRF_PRG | BRF_ESS },	//  1 
	{ "136042-429.6f",			0x008000, 0x2060f68a,  0x01 | BRF_PRG | BRF_ESS },	//  2 
	{ "136042-425.6n",			0x008000, 0xb7274985,  0x01 | BRF_PRG | BRF_ESS },	//  3 
	{ "136042-127.6k",			0x008000, 0xde6c4db9,  0x01 | BRF_PRG | BRF_ESS },	//  4 
	{ "136042-123.6r",			0x008000, 0xaff23b5a,  0x01 | BRF_PRG | BRF_ESS },	//  5 
	{ "136042-126.6l",			0x008000, 0x92f5392c,  0x01 | BRF_PRG | BRF_ESS },	//  6 
	{ "136042-122.6s",			0x008000, 0x0381f362,  0x01 | BRF_PRG | BRF_ESS },	//  7 

	{ "136042-119.2bc",			0x004000, 0x0c810231,  0x02 | BRF_PRG | BRF_ESS },	//  8 M6502 Code
	{ "136042-120.2d",			0x004000, 0x647b7481,  0x02 | BRF_PRG | BRF_ESS },	//  9 

	{ "136042-105.6a",			0x010000, 0x911499fe,  0x0b | BRF_GRA },			// 10 Playfield Tiles
	{ "136042-106.6b",			0x008000, 0xa39b25ed,  0x0b | BRF_GRA },			// 11 
	{ "136042-101.7a",			0x010000, 0x6d015c72,  0x0b | BRF_GRA },			// 12 
	{ "136042-102.7b",			0x008000, 0x54e21f0a,  0x0b | BRF_GRA },			// 13 
	{ "136042-107.6c",			0x010000, 0xb7ded658,  0x0b | BRF_GRA },			// 14 
	{ "136042-108.6de",			0x008000, 0x4a804a4c,  0x0b | BRF_GRA },			// 15 
	{ "136042-104.7de",			0x010000, 0x339644ed,  0x0b | BRF_GRA },			// 16 
	{ "136042-103.7c",			0x008000, 0x64d473a8,  0x0b | BRF_GRA },			// 17 

	{ "136042-113.6l",			0x008000, 0xf869b0fc,  0x04 | BRF_GRA },			// 18 sprites
	{ "136042-112.6k",			0x008000, 0xabcbc114,  0x04 | BRF_GRA },			// 19 
	{ "136042-110.6jh",			0x008000, 0x9e91e734,  0x04 | BRF_GRA },			// 20 
	{ "136042-109.6fh",			0x008000, 0x3a051f36,  0x04 | BRF_GRA },			// 21 
	{ "136042-117.6rs",			0x008000, 0xb15c1b90,  0x04 | BRF_GRA },			// 22 
	{ "136042-116.6pr",			0x008000, 0x1dcdd5aa,  0x04 | BRF_GRA },			// 23 
	{ "136042-115.6n",			0x008000, 0xfb5677d9,  0x04 | BRF_GRA },			// 24 
	{ "136042-114.6m",			0x008000, 0x35e70a8d,  0x04 | BRF_GRA },			// 25 

	{ "136042-118.6t",			0x004000, 0x8489d113,  0x05 | BRF_GRA },			// 26 Alpha Numeric Tiles

	{ "ssprint1-eeprom.bin",	0x000200, 0xed263888,  0x06 | BRF_PRG | BRF_ESS },	// 27 Default EEPROM
};

STD_ROM_PICK(ssprintg)
STD_ROM_FN(ssprintg)

struct BurnDriver BurnDrvSsprintg = {
	"ssprintg", "ssprint", NULL, NULL, "1986",
	"Super Sprint (German, rev 2)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 3, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, ssprintgRomInfo, ssprintgRomName, NULL, NULL, NULL, NULL, SsprintInputInfo, SsprintDIPInfo,
	SsprintInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	512, 384, 4, 3
};


// Super Sprint (rev 3)

static struct BurnRomInfo ssprint3RomDesc[] = {
	{ "136042-330.7l",			0x004000, 0xee312027,  0x01 | BRF_PRG | BRF_ESS },	//  0 T11 Code
	{ "136042-331.7n",			0x004000, 0x2ef15354,  0x01 | BRF_PRG | BRF_ESS },	//  1 
	{ "136042-329.6f",			0x008000, 0xed1d6205,  0x01 | BRF_PRG | BRF_ESS },	//  2 
	{ "136042-325.6n",			0x008000, 0xaecaa2bf,  0x01 | BRF_PRG | BRF_ESS },	//  3 
	{ "136042-127.6k",			0x008000, 0xde6c4db9,  0x01 | BRF_PRG | BRF_ESS },	//  4 
	{ "136042-123.6r",			0x008000, 0xaff23b5a,  0x01 | BRF_PRG | BRF_ESS },	//  5 
	{ "136042-126.6l",			0x008000, 0x92f5392c,  0x01 | BRF_PRG | BRF_ESS },	//  6 
	{ "136042-122.6s",			0x008000, 0x0381f362,  0x01 | BRF_PRG | BRF_ESS },	//  7 

	{ "136042-319.2bc",			0x004000, 0xc7f31c16,  0x02 | BRF_PRG | BRF_ESS },	//  8 M6502 Code
	{ "136042-320.2d",			0x004000, 0x9815ece9,  0x02 | BRF_PRG | BRF_ESS },	//  9 

	{ "136042-105.6a",			0x010000, 0x911499fe,  0x0b | BRF_GRA },			// 10 Playfield Tiles
	{ "136042-106.6b",			0x008000, 0xa39b25ed,  0x0b | BRF_GRA },			// 11 
	{ "136042-101.7a",			0x010000, 0x6d015c72,  0x0b | BRF_GRA },			// 12 
	{ "136042-102.7b",			0x008000, 0x54e21f0a,  0x0b | BRF_GRA },			// 13 
	{ "136042-107.6c",			0x010000, 0xb7ded658,  0x0b | BRF_GRA },			// 14 
	{ "136042-108.6de",			0x008000, 0x4a804a4c,  0x0b | BRF_GRA },			// 15 
	{ "136042-104.7de",			0x010000, 0x339644ed,  0x0b | BRF_GRA },			// 16 
	{ "136042-103.7c",			0x008000, 0x64d473a8,  0x0b | BRF_GRA },			// 17 

	{ "136042-113.6l",			0x008000, 0xf869b0fc,  0x04 | BRF_GRA },			// 18 sprites
	{ "136042-112.6k",			0x008000, 0xabcbc114,  0x04 | BRF_GRA },			// 19 
	{ "136042-110.6jh",			0x008000, 0x9e91e734,  0x04 | BRF_GRA },			// 20 
	{ "136042-109.6fh",			0x008000, 0x3a051f36,  0x04 | BRF_GRA },			// 21 
	{ "136042-117.6rs",			0x008000, 0xb15c1b90,  0x04 | BRF_GRA },			// 22 
	{ "136042-116.6pr",			0x008000, 0x1dcdd5aa,  0x04 | BRF_GRA },			// 23 
	{ "136042-115.6n",			0x008000, 0xfb5677d9,  0x04 | BRF_GRA },			// 24 
	{ "136042-114.6m",			0x008000, 0x35e70a8d,  0x04 | BRF_GRA },			// 25 

	{ "136042-118.6t",			0x004000, 0x8489d113,  0x05 | BRF_GRA },			// 26 Alpha Numeric Tiles

	{ "ssprint1-eeprom.bin",	0x000200, 0xed263888,  0x06 | BRF_PRG | BRF_ESS },	// 27 Default EEPROM
};

STD_ROM_PICK(ssprint3)
STD_ROM_FN(ssprint3)

struct BurnDriver BurnDrvSsprint3 = {
	"ssprint3", "ssprint", NULL, NULL, "1986",
	"Super Sprint (rev 3)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 3, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, ssprint3RomInfo, ssprint3RomName, NULL, NULL, NULL, NULL, SsprintInputInfo, SsprintDIPInfo,
	SsprintInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	512, 384, 4, 3
};


// Super Sprint (German, rev 1)

static struct BurnRomInfo ssprintg1RomDesc[] = {
	{ "136042-230.7l",			0x004000, 0xe5b2da29,  0x01 | BRF_PRG | BRF_ESS },	//  0 T11 Code
	{ "136042-231.7n",			0x004000, 0xfac14b00,  0x01 | BRF_PRG | BRF_ESS },	//  1 
	{ "136042-229.6f",			0x008000, 0x78b01070,  0x01 | BRF_PRG | BRF_ESS },	//  2 
	{ "136042-225.6n",			0x008000, 0x03688b4c,  0x01 | BRF_PRG | BRF_ESS },	//  3 
	{ "136042-127.6k",			0x008000, 0xde6c4db9,  0x01 | BRF_PRG | BRF_ESS },	//  4 
	{ "136042-123.6r",			0x008000, 0xaff23b5a,  0x01 | BRF_PRG | BRF_ESS },	//  5 
	{ "136042-126.6l",			0x008000, 0x92f5392c,  0x01 | BRF_PRG | BRF_ESS },	//  6 
	{ "136042-122.6s",			0x008000, 0x0381f362,  0x01 | BRF_PRG | BRF_ESS },	//  7 

	{ "136042-119.2bc",			0x004000, 0x0c810231,  0x02 | BRF_PRG | BRF_ESS },	//  8 M6502 Code
	{ "136042-120.2d",			0x004000, 0x647b7481,  0x02 | BRF_PRG | BRF_ESS },	//  9 

	{ "136042-105.6a",			0x010000, 0x911499fe,  0x0b | BRF_GRA },			// 10 Playfield Tiles
	{ "136042-106.6b",			0x008000, 0xa39b25ed,  0x0b | BRF_GRA },			// 11 
	{ "136042-101.7a",			0x010000, 0x6d015c72,  0x0b | BRF_GRA },			// 12 
	{ "136042-102.7b",			0x008000, 0x54e21f0a,  0x0b | BRF_GRA },			// 13 
	{ "136042-107.6c",			0x010000, 0xb7ded658,  0x0b | BRF_GRA },			// 14 
	{ "136042-108.6de",			0x008000, 0x4a804a4c,  0x0b | BRF_GRA },			// 15 
	{ "136042-104.7de",			0x010000, 0x339644ed,  0x0b | BRF_GRA },			// 16 
	{ "136042-103.7c",			0x008000, 0x64d473a8,  0x0b | BRF_GRA },			// 17 

	{ "136042-113.6l",			0x008000, 0xf869b0fc,  0x04 | BRF_GRA },			// 18 sprites
	{ "136042-112.6k",			0x008000, 0xabcbc114,  0x04 | BRF_GRA },			// 19 
	{ "136042-110.6jh",			0x008000, 0x9e91e734,  0x04 | BRF_GRA },			// 20 
	{ "136042-109.6fh",			0x008000, 0x3a051f36,  0x04 | BRF_GRA },			// 21 
	{ "136042-117.6rs",			0x008000, 0xb15c1b90,  0x04 | BRF_GRA },			// 22 
	{ "136042-116.6pr",			0x008000, 0x1dcdd5aa,  0x04 | BRF_GRA },			// 23 
	{ "136042-115.6n",			0x008000, 0xfb5677d9,  0x04 | BRF_GRA },			// 24 
	{ "136042-114.6m",			0x008000, 0x35e70a8d,  0x04 | BRF_GRA },			// 25 

	{ "136042-118.6t",			0x004000, 0x8489d113,  0x05 | BRF_GRA },			// 26 Alpha Numeric Tiles

	{ "ssprint1-eeprom.bin",	0x000200, 0xed263888,  0x06 | BRF_PRG | BRF_ESS },	// 27 Default EEPROM
};

STD_ROM_PICK(ssprintg1)
STD_ROM_FN(ssprintg1)

struct BurnDriver BurnDrvSsprintg1 = {
	"ssprintg1", "ssprint", NULL, NULL, "1986",
	"Super Sprint (German, rev 1)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 3, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, ssprintg1RomInfo, ssprintg1RomName, NULL, NULL, NULL, NULL, SsprintInputInfo, SsprintDIPInfo,
	SsprintInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	512, 384, 4, 3
};


// Super Sprint (rev 1)

static struct BurnRomInfo ssprint1RomDesc[] = {
	{ "136042-130.7l",			0x004000, 0xb1edc688,  0x01 | BRF_PRG | BRF_ESS },	//  0 T11 Code
	{ "136042-131.7n",			0x004000, 0xdf49dc5a,  0x01 | BRF_PRG | BRF_ESS },	//  1 
	{ "136042-129.6f",			0x008000, 0x8be22fca,  0x01 | BRF_PRG | BRF_ESS },	//  2 
	{ "136042-125.6n",			0x008000, 0x30b9e101,  0x01 | BRF_PRG | BRF_ESS },	//  3 
	{ "136042-127.6k",			0x008000, 0xde6c4db9,  0x01 | BRF_PRG | BRF_ESS },	//  4 
	{ "136042-123.6r",			0x008000, 0xaff23b5a,  0x01 | BRF_PRG | BRF_ESS },	//  5 
	{ "136042-126.6l",			0x008000, 0x92f5392c,  0x01 | BRF_PRG | BRF_ESS },	//  6 
	{ "136042-122.6s",			0x008000, 0x0381f362,  0x01 | BRF_PRG | BRF_ESS },	//  7 

	{ "136042-119.2bc",			0x004000, 0x0c810231,  0x02 | BRF_PRG | BRF_ESS },	//  8 M6502 Code
	{ "136042-120.2d",			0x004000, 0x647b7481,  0x02 | BRF_PRG | BRF_ESS },	//  9 

	{ "136042-105.6a",			0x010000, 0x911499fe,  0x0b | BRF_GRA },			// 10 Playfield Tiles
	{ "136042-106.6b",			0x008000, 0xa39b25ed,  0x0b | BRF_GRA },			// 11 
	{ "136042-101.7a",			0x010000, 0x6d015c72,  0x0b | BRF_GRA },			// 12 
	{ "136042-102.7b",			0x008000, 0x54e21f0a,  0x0b | BRF_GRA },			// 13 
	{ "136042-107.6c",			0x010000, 0xb7ded658,  0x0b | BRF_GRA },			// 14 
	{ "136042-108.6de",			0x008000, 0x4a804a4c,  0x0b | BRF_GRA },			// 15 
	{ "136042-104.7de",			0x010000, 0x339644ed,  0x0b | BRF_GRA },			// 16 
	{ "136042-103.7c",			0x008000, 0x64d473a8,  0x0b | BRF_GRA },			// 17 

	{ "136042-113.6l",			0x008000, 0xf869b0fc,  0x04 | BRF_GRA },			// 18 sprites
	{ "136042-112.6k",			0x008000, 0xabcbc114,  0x04 | BRF_GRA },			// 19 
	{ "136042-110.6jh",			0x008000, 0x9e91e734,  0x04 | BRF_GRA },			// 20 
	{ "136042-109.6fh",			0x008000, 0x3a051f36,  0x04 | BRF_GRA },			// 21 
	{ "136042-117.6rs",			0x008000, 0xb15c1b90,  0x04 | BRF_GRA },			// 22 
	{ "136042-116.6pr",			0x008000, 0x1dcdd5aa,  0x04 | BRF_GRA },			// 23 
	{ "136042-115.6n",			0x008000, 0xfb5677d9,  0x04 | BRF_GRA },			// 24 
	{ "136042-114.6m",			0x008000, 0x35e70a8d,  0x04 | BRF_GRA },			// 25 

	{ "136042-118.6t",			0x004000, 0x8489d113,  0x05 | BRF_GRA },			// 26 Alpha Numeric Tiles

	{ "ssprint1-eeprom.bin",	0x000200, 0xed263888,  0x06 | BRF_PRG | BRF_ESS },	// 27 Default EEPROM
};

STD_ROM_PICK(ssprint1)
STD_ROM_FN(ssprint1)

struct BurnDriver BurnDrvSsprint1 = {
	"ssprint1", "ssprint", NULL, NULL, "1986",
	"Super Sprint (rev 1)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 3, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, ssprint1RomInfo, ssprint1RomName, NULL, NULL, NULL, NULL, SsprintInputInfo, SsprintDIPInfo,
	SsprintInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	512, 384, 4, 3
};


// Championship Sprint (rev 3)

static struct BurnRomInfo csprintRomDesc[] = {
	{ "136045-3126.7l",			0x004000, 0x1dcf8b98,  0x01 | BRF_PRG | BRF_ESS },	//  0 T11 Code
	{ "136045-2127.7n",			0x004000, 0xbdcbe42c,  0x01 | BRF_PRG | BRF_ESS },	//  1 
	{ "136045-2125.6f",			0x008000, 0x76cc68b9,  0x01 | BRF_PRG | BRF_ESS },	//  2 
	{ "136045-2122.6n",			0x008000, 0x87dda6e5,  0x01 | BRF_PRG | BRF_ESS },	//  3 
	{ "136045-1124.6k",			0x008000, 0x47efca1f,  0x01 | BRF_PRG | BRF_ESS },	//  4 
	{ "136045-1121.6r",			0x008000, 0x6ca404bb,  0x01 | BRF_PRG | BRF_ESS },	//  5 
	{ "136045-1123.6l",			0x008000, 0x0a4d216a,  0x01 | BRF_PRG | BRF_ESS },	//  6 
	{ "136045-1120.6s",			0x008000, 0x103f3fde,  0x01 | BRF_PRG | BRF_ESS },	//  7 

	{ "136045-1118.2bc",		0x004000, 0xeba41b2f,  0x02 | BRF_PRG | BRF_ESS },	//  8 M6502 Code
	{ "136045-1119.2d",			0x004000, 0x9e49043a,  0x02 | BRF_PRG | BRF_ESS },	//  9 

	{ "136045-1105.6a",			0x008000, 0x3773bfbb,  0x0b | BRF_GRA },			// 10 Playfield Tiles
	{ "136045-1106.6b",			0x008000, 0x13a24886,  0x0b | BRF_GRA },			// 11 
	{ "136045-1101.7a",			0x010000, 0x5a55f931,  0x0b | BRF_GRA },			// 12 
	{ "136045-1102.7b",			0x008000, 0x37548a60,  0x0b | BRF_GRA },			// 13 
	{ "136045-1107.6c",			0x008000, 0xe35e354e,  0x0b | BRF_GRA },			// 14 
	{ "136045-1108.6de",		0x008000, 0x361db8b7,  0x0b | BRF_GRA },			// 15 
	{ "136045-1104.7de",		0x010000, 0xd1f8fe7b,  0x0b | BRF_GRA },			// 16 
	{ "136045-1103.7c",			0x008000, 0x8f8c9692,  0x0b | BRF_GRA },			// 17 

	{ "136045-1112.6l",			0x008000, 0xf869b0fc,  0x04 | BRF_GRA },			// 18 sprites
	{ "136045-1111.6k",			0x008000, 0xabcbc114,  0x04 | BRF_GRA },			// 19 
	{ "136045-1110.6hj",		0x008000, 0x9e91e734,  0x04 | BRF_GRA },			// 20 
	{ "136045-1109.6fh",		0x008000, 0x3a051f36,  0x04 | BRF_GRA },			// 21 
	{ "136045-1116.6rs",		0x008000, 0xb15c1b90,  0x04 | BRF_GRA },			// 22 
	{ "136045-1115.6pr",		0x008000, 0x1dcdd5aa,  0x04 | BRF_GRA },			// 23 
	{ "136045-1114.6n",			0x008000, 0xfb5677d9,  0x04 | BRF_GRA },			// 24 
	{ "136045-1113.6m",			0x008000, 0x35e70a8d,  0x04 | BRF_GRA },			// 25 

	{ "136045-1117.6t",			0x004000, 0x82da786d,  0x05 | BRF_GRA },			// 26 Alpha Numeric Tiles

	{ "csprint-eeprom.bin",		0x000200, 0xce1c7319,  0x06 | BRF_PRG | BRF_ESS },	// 27 Default EEPROM
};

STD_ROM_PICK(csprint)
STD_ROM_FN(csprint)

static INT32 CsprintInit()
{
	is_ssprint = true; // similar control scheme as ssprint

	return Atarisy2Init(109, SsprintCallback);
}

struct BurnDriver BurnDrvCsprint = {
	"csprint", NULL, NULL, NULL, "1986",
	"Championship Sprint (rev 3)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, csprintRomInfo, csprintRomName, NULL, NULL, NULL, NULL, CsprintInputInfo, CsprintDIPInfo,
	CsprintInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	512, 384, 4, 3
};


// Championship Sprint (Spanish, rev 2)

static struct BurnRomInfo csprintsRomDesc[] = {
	{ "136045-2326.7l",			0x004000, 0xfd4ed0d3,  0x01 | BRF_PRG | BRF_ESS },	//  0 T11 Code
	{ "136045-2327.7n",			0x004000, 0x5ef2a65a,  0x01 | BRF_PRG | BRF_ESS },	//  1 
	{ "136045-2325.6f",			0x008000, 0x57253376,  0x01 | BRF_PRG | BRF_ESS },	//  2 
	{ "136045-2322.6n",			0x008000, 0xb4265cae,  0x01 | BRF_PRG | BRF_ESS },	//  3 
	{ "136045-1124.6k",			0x008000, 0x47efca1f,  0x01 | BRF_PRG | BRF_ESS },	//  4 
	{ "136045-1121.6r",			0x008000, 0x6ca404bb,  0x01 | BRF_PRG | BRF_ESS },	//  5 
	{ "136045-1123.6l",			0x008000, 0x0a4d216a,  0x01 | BRF_PRG | BRF_ESS },	//  6 
	{ "136045-1120.6s",			0x008000, 0x103f3fde,  0x01 | BRF_PRG | BRF_ESS },	//  7 

	{ "136045-1118.2bc",		0x004000, 0xeba41b2f,  0x02 | BRF_PRG | BRF_ESS },	//  8 M6502 Code
	{ "136045-1119.2d",			0x004000, 0x9e49043a,  0x02 | BRF_PRG | BRF_ESS },	//  9 

	{ "136045-1105.6a",			0x008000, 0x3773bfbb,  0x0b | BRF_GRA },			// 10 Playfield Tiles
	{ "136045-1106.6b",			0x008000, 0x13a24886,  0x0b | BRF_GRA },			// 11 
	{ "136045-1101.7a",			0x010000, 0x5a55f931,  0x0b | BRF_GRA },			// 12 
	{ "136045-1102.7b",			0x008000, 0x37548a60,  0x0b | BRF_GRA },			// 13 
	{ "136045-1107.6c",			0x008000, 0xe35e354e,  0x0b | BRF_GRA },			// 14 
	{ "136045-1108.6de",		0x008000, 0x361db8b7,  0x0b | BRF_GRA },			// 15 
	{ "136045-1104.7de",		0x010000, 0xd1f8fe7b,  0x0b | BRF_GRA },			// 16 
	{ "136045-1103.7c",			0x008000, 0x8f8c9692,  0x0b | BRF_GRA },			// 17 

	{ "136045-1112.6l",			0x008000, 0xf869b0fc,  0x04 | BRF_GRA },			// 18 sprites
	{ "136045-1111.6k",			0x008000, 0xabcbc114,  0x04 | BRF_GRA },			// 19 
	{ "136045-1110.6hj",		0x008000, 0x9e91e734,  0x04 | BRF_GRA },			// 20 
	{ "136045-1109.6fh",		0x008000, 0x3a051f36,  0x04 | BRF_GRA },			// 21 
	{ "136045-1116.6rs",		0x008000, 0xb15c1b90,  0x04 | BRF_GRA },			// 22 
	{ "136045-1115.6pr",		0x008000, 0x1dcdd5aa,  0x04 | BRF_GRA },			// 23 
	{ "136045-1114.6n",			0x008000, 0xfb5677d9,  0x04 | BRF_GRA },			// 24 
	{ "136045-1113.6m",			0x008000, 0x35e70a8d,  0x04 | BRF_GRA },			// 25 

	{ "136045-1117.6t",			0x004000, 0x82da786d,  0x05 | BRF_GRA },			// 26 Alpha Numeric Tiles

	{ "csprint-eeprom.bin",		0x000200, 0xce1c7319,  0x06 | BRF_PRG | BRF_ESS },	// 27 Default EEPROM
};

STD_ROM_PICK(csprints)
STD_ROM_FN(csprints)

struct BurnDriver BurnDrvCsprints = {
	"csprints", "csprint", NULL, NULL, "1986",
	"Championship Sprint (Spanish, rev 2)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, csprintsRomInfo, csprintsRomName, NULL, NULL, NULL, NULL, CsprintInputInfo, CsprintDIPInfo,
	CsprintInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	512, 384, 4, 3
};


// Championship Sprint (Spanish, rev 1)

static struct BurnRomInfo csprints1RomDesc[] = {
	{ "136045-1326.7l",			0x004000, 0xcfa673a6,  0x01 | BRF_PRG | BRF_ESS },	//  0 T11 Code
	{ "136045-1327.7n",			0x004000, 0x16c1dcab,  0x01 | BRF_PRG | BRF_ESS },	//  1 
	{ "136045-1325.6f",			0x008000, 0x8661f17b,  0x01 | BRF_PRG | BRF_ESS },	//  2 
	{ "136045-1322.6n",			0x008000, 0x7f440847,  0x01 | BRF_PRG | BRF_ESS },	//  3 
	{ "136045-1124.6k",			0x008000, 0x47efca1f,  0x01 | BRF_PRG | BRF_ESS },	//  4 
	{ "136045-1121.6r",			0x008000, 0x6ca404bb,  0x01 | BRF_PRG | BRF_ESS },	//  5 
	{ "136045-1123.6l",			0x008000, 0x0a4d216a,  0x01 | BRF_PRG | BRF_ESS },	//  6 
	{ "136045-1120.6s",			0x008000, 0x103f3fde,  0x01 | BRF_PRG | BRF_ESS },	//  7 

	{ "136045-1118.2bc",		0x004000, 0xeba41b2f,  0x02 | BRF_PRG | BRF_ESS },	//  8 M6502 Code
	{ "136045-1119.2d",			0x004000, 0x9e49043a,  0x02 | BRF_PRG | BRF_ESS },	//  9 

	{ "136045-1105.6a",			0x008000, 0x3773bfbb,  0x0b | BRF_GRA },			// 10 Playfield Tiles
	{ "136045-1106.6b",			0x008000, 0x13a24886,  0x0b | BRF_GRA },			// 11 
	{ "136045-1101.7a",			0x010000, 0x5a55f931,  0x0b | BRF_GRA },			// 12 
	{ "136045-1102.7b",			0x008000, 0x37548a60,  0x0b | BRF_GRA },			// 13 
	{ "136045-1107.6c",			0x008000, 0xe35e354e,  0x0b | BRF_GRA },			// 14 
	{ "136045-1108.6de",		0x008000, 0x361db8b7,  0x0b | BRF_GRA },			// 15 
	{ "136045-1104.7de",		0x010000, 0xd1f8fe7b,  0x0b | BRF_GRA },			// 16 
	{ "136045-1103.7c",			0x008000, 0x8f8c9692,  0x0b | BRF_GRA },			// 17 

	{ "136045-1112.6l",			0x008000, 0xf869b0fc,  0x04 | BRF_GRA },			// 18 sprites
	{ "136045-1111.6k",			0x008000, 0xabcbc114,  0x04 | BRF_GRA },			// 19 
	{ "136045-1110.6hj",		0x008000, 0x9e91e734,  0x04 | BRF_GRA },			// 20 
	{ "136045-1109.6fh",		0x008000, 0x3a051f36,  0x04 | BRF_GRA },			// 21 
	{ "136045-1116.6rs",		0x008000, 0xb15c1b90,  0x04 | BRF_GRA },			// 22 
	{ "136045-1115.6pr",		0x008000, 0x1dcdd5aa,  0x04 | BRF_GRA },			// 23 
	{ "136045-1114.6n",			0x008000, 0xfb5677d9,  0x04 | BRF_GRA },			// 24 
	{ "136045-1113.6m",			0x008000, 0x35e70a8d,  0x04 | BRF_GRA },			// 25 

	{ "136045-1117.6t",			0x004000, 0x82da786d,  0x05 | BRF_GRA },			// 26 Alpha Numeric Tiles

	{ "csprint-eeprom.bin",		0x000200, 0xce1c7319,  0x06 | BRF_PRG | BRF_ESS },	// 27 Default EEPROM
};

STD_ROM_PICK(csprints1)
STD_ROM_FN(csprints1)

struct BurnDriver BurnDrvCsprints1 = {
	"csprints1", "csprint", NULL, NULL, "1986",
	"Championship Sprint (Spanish, rev 1)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, csprints1RomInfo, csprints1RomName, NULL, NULL, NULL, NULL, CsprintInputInfo, CsprintDIPInfo,
	CsprintInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	512, 384, 4, 3
};


// Championship Sprint (French)

static struct BurnRomInfo csprintfRomDesc[] = {
	{ "136045-1626.7l",			0x004000, 0xf9d4fbd3,  0x01 | BRF_PRG | BRF_ESS },	//  0 T11 Code
	{ "136045-1627.7n",			0x004000, 0x637f0afa,  0x01 | BRF_PRG | BRF_ESS },	//  1 
	{ "136045-1625.6f",			0x008000, 0x1edc6462,  0x01 | BRF_PRG | BRF_ESS },	//  2 
	{ "136045-1622.6n",			0x008000, 0xa1c78189,  0x01 | BRF_PRG | BRF_ESS },	//  3 
	{ "136045-1124.6k",			0x008000, 0x47efca1f,  0x01 | BRF_PRG | BRF_ESS },	//  4 
	{ "136045-1121.6r",			0x008000, 0x6ca404bb,  0x01 | BRF_PRG | BRF_ESS },	//  5 
	{ "136045-1123.6l",			0x008000, 0x0a4d216a,  0x01 | BRF_PRG | BRF_ESS },	//  6 
	{ "136045-1120.6s",			0x008000, 0x103f3fde,  0x01 | BRF_PRG | BRF_ESS },	//  7 

	{ "136045-1118.2bc",		0x004000, 0xeba41b2f,  0x02 | BRF_PRG | BRF_ESS },	//  8 M6502 Code
	{ "136045-1119.2d",			0x004000, 0x9e49043a,  0x02 | BRF_PRG | BRF_ESS },	//  9 

	{ "136045-1105.6a",			0x008000, 0x3773bfbb,  0x0b | BRF_GRA },			// 10 Playfield Tiles
	{ "136045-1106.6b",			0x008000, 0x13a24886,  0x0b | BRF_GRA },			// 11 
	{ "136045-1101.7a",			0x010000, 0x5a55f931,  0x0b | BRF_GRA },			// 12 
	{ "136045-1102.7b",			0x008000, 0x37548a60,  0x0b | BRF_GRA },			// 13 
	{ "136045-1107.6c",			0x008000, 0xe35e354e,  0x0b | BRF_GRA },			// 14 
	{ "136045-1108.6de",		0x008000, 0x361db8b7,  0x0b | BRF_GRA },			// 15 
	{ "136045-1104.7de",		0x010000, 0xd1f8fe7b,  0x0b | BRF_GRA },			// 16 
	{ "136045-1103.7c",			0x008000, 0x8f8c9692,  0x0b | BRF_GRA },			// 17 

	{ "136045-1112.6l",			0x008000, 0xf869b0fc,  0x04 | BRF_GRA },			// 18 sprites
	{ "136045-1111.6k",			0x008000, 0xabcbc114,  0x04 | BRF_GRA },			// 19 
	{ "136045-1110.6hj",		0x008000, 0x9e91e734,  0x04 | BRF_GRA },			// 20 
	{ "136045-1109.6fh",		0x008000, 0x3a051f36,  0x04 | BRF_GRA },			// 21 
	{ "136045-1116.6rs",		0x008000, 0xb15c1b90,  0x04 | BRF_GRA },			// 22 
	{ "136045-1115.6pr",		0x008000, 0x1dcdd5aa,  0x04 | BRF_GRA },			// 23 
	{ "136045-1114.6n",			0x008000, 0xfb5677d9,  0x04 | BRF_GRA },			// 24 
	{ "136045-1113.6m",			0x008000, 0x35e70a8d,  0x04 | BRF_GRA },			// 25 

	{ "136045-1117.6t",			0x004000, 0x82da786d,  0x05 | BRF_GRA },			// 26 Alpha Numeric Tiles

	{ "csprint-eeprom.bin",		0x000200, 0xce1c7319,  0x06 | BRF_PRG | BRF_ESS },	// 27 Default EEPROM
};

STD_ROM_PICK(csprintf)
STD_ROM_FN(csprintf)

struct BurnDriver BurnDrvCsprintf = {
	"csprintf", "csprint", NULL, NULL, "1986",
	"Championship Sprint (French)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, csprintfRomInfo, csprintfRomName, NULL, NULL, NULL, NULL, CsprintInputInfo, CsprintDIPInfo,
	CsprintInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	512, 384, 4, 3
};


// Championship Sprint (German, rev 2)

static struct BurnRomInfo csprintgRomDesc[] = {
	{ "136045-2226.7l",			0x004000, 0x1f437a3f,  0x01 | BRF_PRG | BRF_ESS },	//  0 T11 Code
	{ "136045-1227.7n",			0x004000, 0xd1dce1cc,  0x01 | BRF_PRG | BRF_ESS },	//  1 
	{ "136045-1225.6f",			0x008000, 0xe787da64,  0x01 | BRF_PRG | BRF_ESS },	//  2 
	{ "136045-1222.6n",			0x008000, 0x5656cc40,  0x01 | BRF_PRG | BRF_ESS },	//  3 
	{ "136045-1124.6k",			0x008000, 0x47efca1f,  0x01 | BRF_PRG | BRF_ESS },	//  4 
	{ "136045-1121.6r",			0x008000, 0x6ca404bb,  0x01 | BRF_PRG | BRF_ESS },	//  5 
	{ "136045-1123.6l",			0x008000, 0x0a4d216a,  0x01 | BRF_PRG | BRF_ESS },	//  6 
	{ "136045-1120.6s",			0x008000, 0x103f3fde,  0x01 | BRF_PRG | BRF_ESS },	//  7 

	{ "136045-1118.2bc",		0x004000, 0xeba41b2f,  0x02 | BRF_PRG | BRF_ESS },	//  8 M6502 Code
	{ "136045-1119.2d",			0x004000, 0x9e49043a,  0x02 | BRF_PRG | BRF_ESS },	//  9 

	{ "136045-1105.6a",			0x008000, 0x3773bfbb,  0x0b | BRF_GRA },			// 10 Playfield Tiles
	{ "136045-1106.6b",			0x008000, 0x13a24886,  0x0b | BRF_GRA },			// 11 
	{ "136045-1101.7a",			0x010000, 0x5a55f931,  0x0b | BRF_GRA },			// 12 
	{ "136045-1102.7b",			0x008000, 0x37548a60,  0x0b | BRF_GRA },			// 13 
	{ "136045-1107.6c",			0x008000, 0xe35e354e,  0x0b | BRF_GRA },			// 14 
	{ "136045-1108.6de",		0x008000, 0x361db8b7,  0x0b | BRF_GRA },			// 15 
	{ "136045-1104.7de",		0x010000, 0xd1f8fe7b,  0x0b | BRF_GRA },			// 16 
	{ "136045-1103.7c",			0x008000, 0x8f8c9692,  0x0b | BRF_GRA },			// 17 

	{ "136045-1112.6l",			0x008000, 0xf869b0fc,  0x04 | BRF_GRA },			// 18 sprites
	{ "136045-1111.6k",			0x008000, 0xabcbc114,  0x04 | BRF_GRA },			// 19 
	{ "136045-1110.6hj",		0x008000, 0x9e91e734,  0x04 | BRF_GRA },			// 20 
	{ "136045-1109.6fh",		0x008000, 0x3a051f36,  0x04 | BRF_GRA },			// 21 
	{ "136045-1116.6rs",		0x008000, 0xb15c1b90,  0x04 | BRF_GRA },			// 22 
	{ "136045-1115.6pr",		0x008000, 0x1dcdd5aa,  0x04 | BRF_GRA },			// 23 
	{ "136045-1114.6n",			0x008000, 0xfb5677d9,  0x04 | BRF_GRA },			// 24 
	{ "136045-1113.6m",			0x008000, 0x35e70a8d,  0x04 | BRF_GRA },			// 25 

	{ "136045-1117.6t",			0x004000, 0x82da786d,  0x05 | BRF_GRA },			// 26 Alpha Numeric Tiles

	{ "csprint-eeprom.bin",		0x000200, 0xce1c7319,  0x06 | BRF_PRG | BRF_ESS },	// 27 Default EEPROM
};

STD_ROM_PICK(csprintg)
STD_ROM_FN(csprintg)

struct BurnDriver BurnDrvCsprintg = {
	"csprintg", "csprint", NULL, NULL, "1986",
	"Championship Sprint (German, rev 2)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, csprintgRomInfo, csprintgRomName, NULL, NULL, NULL, NULL, CsprintInputInfo, CsprintDIPInfo,
	CsprintInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	512, 384, 4, 3
};


// Championship Sprint (rev 2)

static struct BurnRomInfo csprint2RomDesc[] = {
	{ "136045-2126.7l",			0x004000, 0x0ff83de8,  0x01 | BRF_PRG | BRF_ESS },	//  0 T11 Code
	{ "136045-1127.7n",			0x004000, 0xe3e37258,  0x01 | BRF_PRG | BRF_ESS },	//  1 
	{ "136045-1125.6f",			0x008000, 0x650623d2,  0x01 | BRF_PRG | BRF_ESS },	//  2 
	{ "136045-1122.6n",			0x008000, 0xca1b1cbf,  0x01 | BRF_PRG | BRF_ESS },	//  3 
	{ "136045-1124.6k",			0x008000, 0x47efca1f,  0x01 | BRF_PRG | BRF_ESS },	//  4 
	{ "136045-1121.6r",			0x008000, 0x6ca404bb,  0x01 | BRF_PRG | BRF_ESS },	//  5 
	{ "136045-1123.6l",			0x008000, 0x0a4d216a,  0x01 | BRF_PRG | BRF_ESS },	//  6 
	{ "136045-1120.6s",			0x008000, 0x103f3fde,  0x01 | BRF_PRG | BRF_ESS },	//  7 

	{ "136045-1118.2bc",		0x004000, 0xeba41b2f,  0x02 | BRF_PRG | BRF_ESS },	//  8 M6502 Code
	{ "136045-1119.2d",			0x004000, 0x9e49043a,  0x02 | BRF_PRG | BRF_ESS },	//  9 

	{ "136045-1105.6a",			0x008000, 0x3773bfbb,  0x0b | BRF_GRA },			// 10 Playfield Tiles
	{ "136045-1106.6b",			0x008000, 0x13a24886,  0x0b | BRF_GRA },			// 11 
	{ "136045-1101.7a",			0x010000, 0x5a55f931,  0x0b | BRF_GRA },			// 12 
	{ "136045-1102.7b",			0x008000, 0x37548a60,  0x0b | BRF_GRA },			// 13 
	{ "136045-1107.6c",			0x008000, 0xe35e354e,  0x0b | BRF_GRA },			// 14 
	{ "136045-1108.6de",		0x008000, 0x361db8b7,  0x0b | BRF_GRA },			// 15 
	{ "136045-1104.7de",		0x010000, 0xd1f8fe7b,  0x0b | BRF_GRA },			// 16 
	{ "136045-1103.7c",			0x008000, 0x8f8c9692,  0x0b | BRF_GRA },			// 17 

	{ "136045-1112.6l",			0x008000, 0xf869b0fc,  0x04 | BRF_GRA },			// 18 sprites
	{ "136045-1111.6k",			0x008000, 0xabcbc114,  0x04 | BRF_GRA },			// 19 
	{ "136045-1110.6hj",		0x008000, 0x9e91e734,  0x04 | BRF_GRA },			// 20 
	{ "136045-1109.6fh",		0x008000, 0x3a051f36,  0x04 | BRF_GRA },			// 21 
	{ "136045-1116.6rs",		0x008000, 0xb15c1b90,  0x04 | BRF_GRA },			// 22 
	{ "136045-1115.6pr",		0x008000, 0x1dcdd5aa,  0x04 | BRF_GRA },			// 23 
	{ "136045-1114.6n",			0x008000, 0xfb5677d9,  0x04 | BRF_GRA },			// 24 
	{ "136045-1113.6m",			0x008000, 0x35e70a8d,  0x04 | BRF_GRA },			// 25 

	{ "136045-1117.6t",			0x004000, 0x82da786d,  0x05 | BRF_GRA },			// 26 Alpha Numeric Tiles

	{ "csprint-eeprom.bin",		0x000200, 0xce1c7319,  0x06 | BRF_PRG | BRF_ESS },	// 27 Default EEPROM
};

STD_ROM_PICK(csprint2)
STD_ROM_FN(csprint2)

struct BurnDriver BurnDrvCsprint2 = {
	"csprint2", "csprint", NULL, NULL, "1986",
	"Championship Sprint (rev 2)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, csprint2RomInfo, csprint2RomName, NULL, NULL, NULL, NULL, CsprintInputInfo, CsprintDIPInfo,
	CsprintInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	512, 384, 4, 3
};


// Championship Sprint (German, rev 1)

static struct BurnRomInfo csprintg1RomDesc[] = {
	{ "136045-1226.7l",			0x004000, 0xbecfc276,  0x01 | BRF_PRG | BRF_ESS },	//  0 T11 Code
	{ "136045-1227.7n",			0x004000, 0xd1dce1cc,  0x01 | BRF_PRG | BRF_ESS },	//  1 
	{ "136045-1225.6f",			0x008000, 0xe787da64,  0x01 | BRF_PRG | BRF_ESS },	//  2 
	{ "136045-1222.6n",			0x008000, 0x5656cc40,  0x01 | BRF_PRG | BRF_ESS },	//  3 
	{ "136045-1124.6k",			0x008000, 0x47efca1f,  0x01 | BRF_PRG | BRF_ESS },	//  4 
	{ "136045-1121.6r",			0x008000, 0x6ca404bb,  0x01 | BRF_PRG | BRF_ESS },	//  5 
	{ "136045-1123.6l",			0x008000, 0x0a4d216a,  0x01 | BRF_PRG | BRF_ESS },	//  6 
	{ "136045-1120.6s",			0x008000, 0x103f3fde,  0x01 | BRF_PRG | BRF_ESS },	//  7 

	{ "136045-1118.2bc",		0x004000, 0xeba41b2f,  0x02 | BRF_PRG | BRF_ESS },	//  8 M6502 Code
	{ "136045-1119.2d",			0x004000, 0x9e49043a,  0x02 | BRF_PRG | BRF_ESS },	//  9 

	{ "136045-1105.6a",			0x008000, 0x3773bfbb,  0x0b | BRF_GRA },			// 10 Playfield Tiles
	{ "136045-1106.6b",			0x008000, 0x13a24886,  0x0b | BRF_GRA },			// 11 
	{ "136045-1101.7a",			0x010000, 0x5a55f931,  0x0b | BRF_GRA },			// 12 
	{ "136045-1102.7b",			0x008000, 0x37548a60,  0x0b | BRF_GRA },			// 13 
	{ "136045-1107.6c",			0x008000, 0xe35e354e,  0x0b | BRF_GRA },			// 14 
	{ "136045-1108.6de",		0x008000, 0x361db8b7,  0x0b | BRF_GRA },			// 15 
	{ "136045-1104.7de",		0x010000, 0xd1f8fe7b,  0x0b | BRF_GRA },			// 16 
	{ "136045-1103.7c",			0x008000, 0x8f8c9692,  0x0b | BRF_GRA },			// 17 

	{ "136045-1112.6l",			0x008000, 0xf869b0fc,  0x04 | BRF_GRA },			// 18 sprites
	{ "136045-1111.6k",			0x008000, 0xabcbc114,  0x04 | BRF_GRA },			// 19 
	{ "136045-1110.6hj",		0x008000, 0x9e91e734,  0x04 | BRF_GRA },			// 20 
	{ "136045-1109.6fh",		0x008000, 0x3a051f36,  0x04 | BRF_GRA },			// 21 
	{ "136045-1116.6rs",		0x008000, 0xb15c1b90,  0x04 | BRF_GRA },			// 22 
	{ "136045-1115.6pr",		0x008000, 0x1dcdd5aa,  0x04 | BRF_GRA },			// 23 
	{ "136045-1114.6n",			0x008000, 0xfb5677d9,  0x04 | BRF_GRA },			// 24 
	{ "136045-1113.6m",			0x008000, 0x35e70a8d,  0x04 | BRF_GRA },			// 25 

	{ "136045-1117.6t",			0x004000, 0x82da786d,  0x05 | BRF_GRA },			// 26 Alpha Numeric Tiles

	{ "csprint-eeprom.bin",		0x000200, 0xce1c7319,  0x06 | BRF_PRG | BRF_ESS },	// 27 Default EEPROM
};

STD_ROM_PICK(csprintg1)
STD_ROM_FN(csprintg1)

struct BurnDriver BurnDrvCsprintg1 = {
	"csprintg1", "csprint", NULL, NULL, "1986",
	"Championship Sprint (German, rev 1)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, csprintg1RomInfo, csprintg1RomName, NULL, NULL, NULL, NULL, CsprintInputInfo, CsprintDIPInfo,
	CsprintInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	512, 384, 4, 3
};


// Championship Sprint (rev 1)

static struct BurnRomInfo csprint1RomDesc[] = {
	{ "136045-1126.7l",			0x004000, 0xa04ecbac,  0x01 | BRF_PRG | BRF_ESS },	//  0 T11 Code
	{ "136045-1127.7n",			0x004000, 0xe3e37258,  0x01 | BRF_PRG | BRF_ESS },	//  1 
	{ "136045-1125.6f",			0x008000, 0x650623d2,  0x01 | BRF_PRG | BRF_ESS },	//  2 
	{ "136045-1122.6n",			0x008000, 0xca1b1cbf,  0x01 | BRF_PRG | BRF_ESS },	//  3 
	{ "136045-1124.6k",			0x008000, 0x47efca1f,  0x01 | BRF_PRG | BRF_ESS },	//  4 
	{ "136045-1121.6r",			0x008000, 0x6ca404bb,  0x01 | BRF_PRG | BRF_ESS },	//  5 
	{ "136045-1123.6l",			0x008000, 0x0a4d216a,  0x01 | BRF_PRG | BRF_ESS },	//  6 
	{ "136045-1120.6s",			0x008000, 0x103f3fde,  0x01 | BRF_PRG | BRF_ESS },	//  7 

	{ "136045-1118.2bc",		0x004000, 0xeba41b2f,  0x02 | BRF_PRG | BRF_ESS },	//  8 M6502 Code
	{ "136045-1119.2d",			0x004000, 0x9e49043a,  0x02 | BRF_PRG | BRF_ESS },	//  9 

	{ "136045-1105.6a",			0x008000, 0x3773bfbb,  0x0b | BRF_GRA },			// 10 Playfield Tiles
	{ "136045-1106.6b",			0x008000, 0x13a24886,  0x0b | BRF_GRA },			// 11 
	{ "136045-1101.7a",			0x010000, 0x5a55f931,  0x0b | BRF_GRA },			// 12 
	{ "136045-1102.7b",			0x008000, 0x37548a60,  0x0b | BRF_GRA },			// 13 
	{ "136045-1107.6c",			0x008000, 0xe35e354e,  0x0b | BRF_GRA },			// 14 
	{ "136045-1108.6de",		0x008000, 0x361db8b7,  0x0b | BRF_GRA },			// 15 
	{ "136045-1104.7de",		0x010000, 0xd1f8fe7b,  0x0b | BRF_GRA },			// 16 
	{ "136045-1103.7c",			0x008000, 0x8f8c9692,  0x0b | BRF_GRA },			// 17 

	{ "136045-1112.6l",			0x008000, 0xf869b0fc,  0x04 | BRF_GRA },			// 18 sprites
	{ "136045-1111.6k",			0x008000, 0xabcbc114,  0x04 | BRF_GRA },			// 19 
	{ "136045-1110.6hj",		0x008000, 0x9e91e734,  0x04 | BRF_GRA },			// 20 
	{ "136045-1109.6fh",		0x008000, 0x3a051f36,  0x04 | BRF_GRA },			// 21 
	{ "136045-1116.6rs",		0x008000, 0xb15c1b90,  0x04 | BRF_GRA },			// 22 
	{ "136045-1115.6pr",		0x008000, 0x1dcdd5aa,  0x04 | BRF_GRA },			// 23 
	{ "136045-1114.6n",			0x008000, 0xfb5677d9,  0x04 | BRF_GRA },			// 24 
	{ "136045-1113.6m",			0x008000, 0x35e70a8d,  0x04 | BRF_GRA },			// 25 

	{ "136045-1117.6t",			0x004000, 0x82da786d,  0x05 | BRF_GRA },			// 26 Alpha Numeric Tiles

	{ "csprint-eeprom.bin",		0x000200, 0xce1c7319,  0x06 | BRF_PRG | BRF_ESS },	// 27 Default EEPROM
};

STD_ROM_PICK(csprint1)
STD_ROM_FN(csprint1)

struct BurnDriver BurnDrvCsprint1 = {
	"csprint1", "csprint", NULL, NULL, "1986",
	"Championship Sprint (rev 1)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, csprint1RomInfo, csprint1RomName, NULL, NULL, NULL, NULL, CsprintInputInfo, CsprintDIPInfo,
	CsprintInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	512, 384, 4, 3
};


// APB - All Points Bulletin (rev 7)

static struct BurnRomInfo apbRomDesc[] = {
	{ "136051-2126.7l",			0x004000, 0x8edf4726,  0x01 | BRF_PRG | BRF_ESS },	//  0 T11 Code
	{ "136051-2127.7n",			0x004000, 0xe2b2aff2,  0x01 | BRF_PRG | BRF_ESS },	//  1 
	{ "136051-7128.6f",			0x010000, 0xc08504d2,  0x01 | BRF_PRG | BRF_ESS },	//  2 
	{ "136051-7129.6n",			0x010000, 0x79adb57f,  0x01 | BRF_PRG | BRF_ESS },	//  3 
	{ "136051-1130.6j",			0x010000, 0xf64c752e,  0x01 | BRF_PRG | BRF_ESS },	//  4 
	{ "136051-1131.6p",			0x010000, 0x0a506e04,  0x01 | BRF_PRG | BRF_ESS },	//  5 
	{ "136051-1132.6l",			0x010000, 0x6d0e7a4e,  0x01 | BRF_PRG | BRF_ESS },	//  6 
	{ "136051-1133.6s",			0x010000, 0xaf88d429,  0x01 | BRF_PRG | BRF_ESS },	//  7 

	{ "136051-5134.2a",			0x004000, 0x1c8bdeed,  0x02 | BRF_PRG | BRF_ESS },	//  8 M6502 Code
	{ "136051-5135.2bc",		0x004000, 0xed6adb91,  0x02 | BRF_PRG | BRF_ESS },	//  9 
	{ "136051-5136.2d",			0x004000, 0x341f8486,  0x02 | BRF_PRG | BRF_ESS },	// 10 

	{ "136051-1118.6a",			0x008000, 0x93752c49,  0x0b | BRF_GRA },			// 11 Playfield Tiles
	{ "136051-1120.6bc",		0x010000, 0x043086f8,  0x0b | BRF_GRA },			// 12 
	{ "136051-1122.7a",			0x010000, 0x5ee79481,  0x0b | BRF_GRA },			// 13 
	{ "136051-1124.7bc",		0x010000, 0x27760395,  0x0b | BRF_GRA },			// 14 
	{ "136051-1117.6cd",		0x008000, 0xcfc3f8a3,  0x0b | BRF_GRA },			// 15 
	{ "136051-1119.6de",		0x010000, 0x68850612,  0x0b | BRF_GRA },			// 16 
	{ "136051-1121.7de",		0x010000, 0xc7977062,  0x0b | BRF_GRA },			// 17 
	{ "136051-1123.7cd",		0x010000, 0x3c96c848,  0x0b | BRF_GRA },			// 18 

	{ "136051-1105.6t",			0x010000, 0x9b78a88e,  0x0c | BRF_GRA },			// 19 sprites
	{ "136051-1106.6rs",		0x010000, 0x4787ff58,  0x0c | BRF_GRA },			// 20 
	{ "136051-1107.6pr",		0x010000, 0x0e85f2ac,  0x0c | BRF_GRA },			// 21 
	{ "136051-1108.6n",			0x010000, 0x70ff9308,  0x0c | BRF_GRA },			// 22 
	{ "136051-1113.6m",			0x010000, 0x4a445356,  0x0c | BRF_GRA },			// 23 
	{ "136051-1114.6kl",		0x010000, 0xb9b27f3c,  0x0c | BRF_GRA },			// 24 
	{ "136051-1115.6jk",		0x010000, 0xa7671dd8,  0x0c | BRF_GRA },			// 25 
	{ "136051-1116.6h",			0x010000, 0x879fc7de,  0x0c | BRF_GRA },			// 26 
	{ "136051-1101.5t",			0x010000, 0x0ef13513,  0x0c | BRF_GRA },			// 27 
	{ "136051-1102.5rs",		0x010000, 0x401e06fd,  0x0c | BRF_GRA },			// 28 
	{ "136051-1103.5pr",		0x010000, 0x50d820e8,  0x0c | BRF_GRA },			// 29 
	{ "136051-1104.5n",			0x010000, 0x912d878f,  0x0c | BRF_GRA },			// 30 
	{ "136051-1109.5m",			0x010000, 0x6716a408,  0x0c | BRF_GRA },			// 31 
	{ "136051-1110.5kl",		0x010000, 0x7e184981,  0x0c | BRF_GRA },			// 32 
	{ "136051-1111.5jk",		0x010000, 0x353a14fd,  0x0c | BRF_GRA },			// 33 
	{ "136051-1112.5h",			0x010000, 0x3af7c50f,  0x0c | BRF_GRA },			// 34 

	{ "136051-1125.4t",			0x004000, 0x05a0341c,  0x05 | BRF_GRA },			// 35 Alpha Numeric Tiles
};

STD_ROM_PICK(apb)
STD_ROM_FN(apb)

static void ApbCallback()
{
	memcpy (DrvT11ROM + 0x70000, DrvT11ROM + 0x50000, 0x20000);
	memset (DrvT11ROM + 0x50000, 0, 0x20000);
}

static INT32 ApbInit()
{
	is_apb = 1;
	return Atarisy2Init(110, ApbCallback);
}

struct BurnDriver BurnDrvApb = {
	"apb", NULL, NULL, NULL, "1987",
	"APB - All Points Bulletin (rev 7)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 1, HARDWARE_MISC_PRE90S, GBF_RACING | GBF_ACTION, 0,
	NULL, apbRomInfo, apbRomName, NULL, NULL, NULL, NULL, ApbInputInfo, ApbDIPInfo,
	ApbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	384, 512, 3, 4
};


// APB - All Points Bulletin (rev 6)

static struct BurnRomInfo apb6RomDesc[] = {
	{ "136051-2126.7l",			0x004000, 0x8edf4726,  0x01 | BRF_PRG | BRF_ESS },	//  0 T11 Code
	{ "136051-2127.7n",			0x004000, 0xe2b2aff2,  0x01 | BRF_PRG | BRF_ESS },	//  1 
	{ "136051-6128.6f",			0x010000, 0xc852959d,  0x01 | BRF_PRG | BRF_ESS },	//  2 
	{ "136051-6129.6n",			0x010000, 0xb5d1d8eb,  0x01 | BRF_PRG | BRF_ESS },	//  3 
	{ "136051-1130.6j",			0x010000, 0xf64c752e,  0x01 | BRF_PRG | BRF_ESS },	//  4 
	{ "136051-1131.6p",			0x010000, 0x0a506e04,  0x01 | BRF_PRG | BRF_ESS },	//  5 
	{ "136051-1132.6l",			0x010000, 0x6d0e7a4e,  0x01 | BRF_PRG | BRF_ESS },	//  6 
	{ "136051-1133.6s",			0x010000, 0xaf88d429,  0x01 | BRF_PRG | BRF_ESS },	//  7 

	{ "136051-5134.2a",			0x004000, 0x1c8bdeed,  0x02 | BRF_PRG | BRF_ESS },	//  8 M6502 Code
	{ "136051-5135.2bc",		0x004000, 0xed6adb91,  0x02 | BRF_PRG | BRF_ESS },	//  9 
	{ "136051-5136.2d",			0x004000, 0x341f8486,  0x02 | BRF_PRG | BRF_ESS },	// 10 

	{ "136051-1118.6a",			0x008000, 0x93752c49,  0x0b | BRF_GRA },			// 11 Playfield Tiles
	{ "136051-1120.6bc",		0x010000, 0x043086f8,  0x0b | BRF_GRA },			// 12 
	{ "136051-1122.7a",			0x010000, 0x5ee79481,  0x0b | BRF_GRA },			// 13 
	{ "136051-1124.7bc",		0x010000, 0x27760395,  0x0b | BRF_GRA },			// 14 
	{ "136051-1117.6cd",		0x008000, 0xcfc3f8a3,  0x0b | BRF_GRA },			// 15 
	{ "136051-1119.6de",		0x010000, 0x68850612,  0x0b | BRF_GRA },			// 16 
	{ "136051-1121.7de",		0x010000, 0xc7977062,  0x0b | BRF_GRA },			// 17 
	{ "136051-1123.7cd",		0x010000, 0x3c96c848,  0x0b | BRF_GRA },			// 18 

	{ "136051-1105.6t",			0x010000, 0x9b78a88e,  0x0c | BRF_GRA },			// 19 sprites
	{ "136051-1106.6rs",		0x010000, 0x4787ff58,  0x0c | BRF_GRA },			// 20 
	{ "136051-1107.6pr",		0x010000, 0x0e85f2ac,  0x0c | BRF_GRA },			// 21 
	{ "136051-1108.6n",			0x010000, 0x70ff9308,  0x0c | BRF_GRA },			// 22 
	{ "136051-1113.6m",			0x010000, 0x4a445356,  0x0c | BRF_GRA },			// 23 
	{ "136051-1114.6kl",		0x010000, 0xb9b27f3c,  0x0c | BRF_GRA },			// 24 
	{ "136051-1115.6jk",		0x010000, 0xa7671dd8,  0x0c | BRF_GRA },			// 25 
	{ "136051-1116.6h",			0x010000, 0x879fc7de,  0x0c | BRF_GRA },			// 26 
	{ "136051-1101.5t",			0x010000, 0x0ef13513,  0x0c | BRF_GRA },			// 27 
	{ "136051-1102.5rs",		0x010000, 0x401e06fd,  0x0c | BRF_GRA },			// 28 
	{ "136051-1103.5pr",		0x010000, 0x50d820e8,  0x0c | BRF_GRA },			// 29 
	{ "136051-1104.5n",			0x010000, 0x912d878f,  0x0c | BRF_GRA },			// 30 
	{ "136051-1109.5m",			0x010000, 0x6716a408,  0x0c | BRF_GRA },			// 31 
	{ "136051-1110.5kl",		0x010000, 0x7e184981,  0x0c | BRF_GRA },			// 32 
	{ "136051-1111.5jk",		0x010000, 0x353a14fd,  0x0c | BRF_GRA },			// 33 
	{ "136051-1112.5h",			0x010000, 0x3af7c50f,  0x0c | BRF_GRA },			// 34 

	{ "136051-1125.4t",			0x004000, 0x05a0341c,  0x05 | BRF_GRA },			// 35 Alpha Numeric Tiles
};

STD_ROM_PICK(apb6)
STD_ROM_FN(apb6)

struct BurnDriver BurnDrvApb6 = {
	"apb6", "apb", NULL, NULL, "1987",
	"APB - All Points Bulletin (rev 6)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 1, HARDWARE_MISC_PRE90S, GBF_RACING | GBF_ACTION, 0,
	NULL, apb6RomInfo, apb6RomName, NULL, NULL, NULL, NULL, ApbInputInfo, ApbDIPInfo,
	ApbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	384, 512, 3, 4
};


// APB - All Points Bulletin (rev 5)

static struct BurnRomInfo apb5RomDesc[] = {
	{ "136051-2126.7l",			0x004000, 0x8edf4726,  0x01 | BRF_PRG | BRF_ESS },	//  0 T11 Code
	{ "136051-2127.7n",			0x004000, 0xe2b2aff2,  0x01 | BRF_PRG | BRF_ESS },	//  1 
	{ "136051-5128.6f",			0x010000, 0x4b4ff365,  0x01 | BRF_PRG | BRF_ESS },	//  2 
	{ "136051-5129.6n",			0x010000, 0x059ab792,  0x01 | BRF_PRG | BRF_ESS },	//  3 
	{ "136051-1130.6j",			0x010000, 0xf64c752e,  0x01 | BRF_PRG | BRF_ESS },	//  4 
	{ "136051-1131.6p",			0x010000, 0x0a506e04,  0x01 | BRF_PRG | BRF_ESS },	//  5 
	{ "136051-1132.6l",			0x010000, 0x6d0e7a4e,  0x01 | BRF_PRG | BRF_ESS },	//  6 
	{ "136051-1133.6s",			0x010000, 0xaf88d429,  0x01 | BRF_PRG | BRF_ESS },	//  7 

	{ "136051-5134.2a",			0x004000, 0x1c8bdeed,  0x02 | BRF_PRG | BRF_ESS },	//  8 M6502 Code
	{ "136051-5135.2bc",		0x004000, 0xed6adb91,  0x02 | BRF_PRG | BRF_ESS },	//  9 
	{ "136051-5136.2d",			0x004000, 0x341f8486,  0x02 | BRF_PRG | BRF_ESS },	// 10 

	{ "136051-1118.6a",			0x008000, 0x93752c49,  0x0b | BRF_GRA },			// 11 Playfield Tiles
	{ "136051-1120.6bc",		0x010000, 0x043086f8,  0x0b | BRF_GRA },			// 12 
	{ "136051-1122.7a",			0x010000, 0x5ee79481,  0x0b | BRF_GRA },			// 13 
	{ "136051-1124.7bc",		0x010000, 0x27760395,  0x0b | BRF_GRA },			// 14 
	{ "136051-1117.6cd",		0x008000, 0xcfc3f8a3,  0x0b | BRF_GRA },			// 15 
	{ "136051-1119.6de",		0x010000, 0x68850612,  0x0b | BRF_GRA },			// 16 
	{ "136051-1121.7de",		0x010000, 0xc7977062,  0x0b | BRF_GRA },			// 17 
	{ "136051-1123.7cd",		0x010000, 0x3c96c848,  0x0b | BRF_GRA },			// 18 

	{ "136051-1105.6t",			0x010000, 0x9b78a88e,  0x0c | BRF_GRA },			// 19 sprites
	{ "136051-1106.6rs",		0x010000, 0x4787ff58,  0x0c | BRF_GRA },			// 20 
	{ "136051-1107.6pr",		0x010000, 0x0e85f2ac,  0x0c | BRF_GRA },			// 21 
	{ "136051-1108.6n",			0x010000, 0x70ff9308,  0x0c | BRF_GRA },			// 22 
	{ "136051-1113.6m",			0x010000, 0x4a445356,  0x0c | BRF_GRA },			// 23 
	{ "136051-1114.6kl",		0x010000, 0xb9b27f3c,  0x0c | BRF_GRA },			// 24 
	{ "136051-1115.6jk",		0x010000, 0xa7671dd8,  0x0c | BRF_GRA },			// 25 
	{ "136051-1116.6h",			0x010000, 0x879fc7de,  0x0c | BRF_GRA },			// 26 
	{ "136051-1101.5t",			0x010000, 0x0ef13513,  0x0c | BRF_GRA },			// 27 
	{ "136051-1102.5rs",		0x010000, 0x401e06fd,  0x0c | BRF_GRA },			// 28 
	{ "136051-1103.5pr",		0x010000, 0x50d820e8,  0x0c | BRF_GRA },			// 29 
	{ "136051-1104.5n",			0x010000, 0x912d878f,  0x0c | BRF_GRA },			// 30 
	{ "136051-1109.5m",			0x010000, 0x6716a408,  0x0c | BRF_GRA },			// 31 
	{ "136051-1110.5kl",		0x010000, 0x7e184981,  0x0c | BRF_GRA },			// 32 
	{ "136051-1111.5jk",		0x010000, 0x353a14fd,  0x0c | BRF_GRA },			// 33 
	{ "136051-1112.5h",			0x010000, 0x3af7c50f,  0x0c | BRF_GRA },			// 34 

	{ "136051-1125.4t",			0x004000, 0x05a0341c,  0x05 | BRF_GRA },			// 35 Alpha Numeric Tiles
};

STD_ROM_PICK(apb5)
STD_ROM_FN(apb5)

struct BurnDriver BurnDrvApb5 = {
	"apb5", "apb", NULL, NULL, "1987",
	"APB - All Points Bulletin (rev 5)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 1, HARDWARE_MISC_PRE90S, GBF_RACING | GBF_ACTION, 0,
	NULL, apb5RomInfo, apb5RomName, NULL, NULL, NULL, NULL, ApbInputInfo, ApbDIPInfo,
	ApbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	384, 512, 3, 4
};


// APB - All Points Bulletin (rev 4)

static struct BurnRomInfo apb4RomDesc[] = {
	{ "136051-2126.7l",			0x004000, 0x8edf4726,  0x01 | BRF_PRG | BRF_ESS },	//  0 T11 Code
	{ "136051-2127.7n",			0x004000, 0xe2b2aff2,  0x01 | BRF_PRG | BRF_ESS },	//  1 
	{ "136051-4128.6f",			0x010000, 0x46009f6b,  0x01 | BRF_PRG | BRF_ESS },	//  2 
	{ "136051-4129.6n",			0x010000, 0xe8ca47e2,  0x01 | BRF_PRG | BRF_ESS },	//  3 
	{ "136051-1130.6j",			0x010000, 0xf64c752e,  0x01 | BRF_PRG | BRF_ESS },	//  4 
	{ "136051-1131.6p",			0x010000, 0x0a506e04,  0x01 | BRF_PRG | BRF_ESS },	//  5 
	{ "136051-1132.6l",			0x010000, 0x6d0e7a4e,  0x01 | BRF_PRG | BRF_ESS },	//  6 
	{ "136051-1133.6s",			0x010000, 0xaf88d429,  0x01 | BRF_PRG | BRF_ESS },	//  7 

	{ "136051-5134.2a",			0x004000, 0x1c8bdeed,  0x02 | BRF_PRG | BRF_ESS },	//  8 M6502 Code
	{ "136051-5135.2bc",		0x004000, 0xed6adb91,  0x02 | BRF_PRG | BRF_ESS },	//  9 
	{ "136051-5136.2d",			0x004000, 0x341f8486,  0x02 | BRF_PRG | BRF_ESS },	// 10 

	{ "136051-1118.6a",			0x008000, 0x93752c49,  0x0b | BRF_GRA },			// 11 Playfield Tiles
	{ "136051-1120.6bc",		0x010000, 0x043086f8,  0x0b | BRF_GRA },			// 12 
	{ "136051-1122.7a",			0x010000, 0x5ee79481,  0x0b | BRF_GRA },			// 13 
	{ "136051-1124.7bc",		0x010000, 0x27760395,  0x0b | BRF_GRA },			// 14 
	{ "136051-1117.6cd",		0x008000, 0xcfc3f8a3,  0x0b | BRF_GRA },			// 15 
	{ "136051-1119.6de",		0x010000, 0x68850612,  0x0b | BRF_GRA },			// 16 
	{ "136051-1121.7de",		0x010000, 0xc7977062,  0x0b | BRF_GRA },			// 17 
	{ "136051-1123.7cd",		0x010000, 0x3c96c848,  0x0b | BRF_GRA },			// 18 

	{ "136051-1105.6t",			0x010000, 0x9b78a88e,  0x0c | BRF_GRA },			// 19 sprites
	{ "136051-1106.6rs",		0x010000, 0x4787ff58,  0x0c | BRF_GRA },			// 20 
	{ "136051-1107.6pr",		0x010000, 0x0e85f2ac,  0x0c | BRF_GRA },			// 21 
	{ "136051-1108.6n",			0x010000, 0x70ff9308,  0x0c | BRF_GRA },			// 22 
	{ "136051-1113.6m",			0x010000, 0x4a445356,  0x0c | BRF_GRA },			// 23 
	{ "136051-1114.6kl",		0x010000, 0xb9b27f3c,  0x0c | BRF_GRA },			// 24 
	{ "136051-1115.6jk",		0x010000, 0xa7671dd8,  0x0c | BRF_GRA },			// 25 
	{ "136051-1116.6h",			0x010000, 0x879fc7de,  0x0c | BRF_GRA },			// 26 
	{ "136051-1101.5t",			0x010000, 0x0ef13513,  0x0c | BRF_GRA },			// 27 
	{ "136051-1102.5rs",		0x010000, 0x401e06fd,  0x0c | BRF_GRA },			// 28 
	{ "136051-1103.5pr",		0x010000, 0x50d820e8,  0x0c | BRF_GRA },			// 29 
	{ "136051-1104.5n",			0x010000, 0x912d878f,  0x0c | BRF_GRA },			// 30 
	{ "136051-1109.5m",			0x010000, 0x6716a408,  0x0c | BRF_GRA },			// 31 
	{ "136051-1110.5kl",		0x010000, 0x7e184981,  0x0c | BRF_GRA },			// 32 
	{ "136051-1111.5jk",		0x010000, 0x353a14fd,  0x0c | BRF_GRA },			// 33 
	{ "136051-1112.5h",			0x010000, 0x3af7c50f,  0x0c | BRF_GRA },			// 34 

	{ "136051-1125.4t",			0x004000, 0x05a0341c,  0x05 | BRF_GRA },			// 35 Alpha Numeric Tiles
};

STD_ROM_PICK(apb4)
STD_ROM_FN(apb4)

struct BurnDriver BurnDrvApb4 = {
	"apb4", "apb", NULL, NULL, "1987",
	"APB - All Points Bulletin (rev 4)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 1, HARDWARE_MISC_PRE90S, GBF_RACING | GBF_ACTION, 0,
	NULL, apb4RomInfo, apb4RomName, NULL, NULL, NULL, NULL, ApbInputInfo, ApbDIPInfo,
	ApbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	384, 512, 3, 4
};


// APB - All Points Bulletin (rev 3)

static struct BurnRomInfo apb3RomDesc[] = {
	{ "136051-2126.7l",			0x004000, 0x8edf4726,  0x01 | BRF_PRG | BRF_ESS },	//  0 T11 Code
	{ "136051-2127.7n",			0x004000, 0xe2b2aff2,  0x01 | BRF_PRG | BRF_ESS },	//  1 
	{ "136051-3128.6f",			0x010000, 0xcbdbfb42,  0x01 | BRF_PRG | BRF_ESS },	//  2 
	{ "136051-3129.6n",			0x010000, 0x14d1cc8d,  0x01 | BRF_PRG | BRF_ESS },	//  3 
	{ "136051-1130.6j",			0x010000, 0xf64c752e,  0x01 | BRF_PRG | BRF_ESS },	//  4 
	{ "136051-1131.6p",			0x010000, 0x0a506e04,  0x01 | BRF_PRG | BRF_ESS },	//  5 
	{ "136051-1132.6l",			0x010000, 0x6d0e7a4e,  0x01 | BRF_PRG | BRF_ESS },	//  6 
	{ "136051-1133.6s",			0x010000, 0xaf88d429,  0x01 | BRF_PRG | BRF_ESS },	//  7 

	{ "136051-1134.2a",			0x004000, 0xa65748b9,  0x02 | BRF_PRG | BRF_ESS },	//  8 M6502 Code
	{ "136051-1135.2bc",		0x004000, 0xe9692cea,  0x02 | BRF_PRG | BRF_ESS },	//  9 
	{ "136051-1136.2d",			0x004000, 0x92fc7657,  0x02 | BRF_PRG | BRF_ESS },	// 10 

	{ "136051-1118.6a",			0x008000, 0x93752c49,  0x0b | BRF_GRA },			// 11 Playfield Tiles
	{ "136051-1120.6bc",		0x010000, 0x043086f8,  0x0b | BRF_GRA },			// 12 
	{ "136051-1122.7a",			0x010000, 0x5ee79481,  0x0b | BRF_GRA },			// 13 
	{ "136051-1124.7bc",		0x010000, 0x27760395,  0x0b | BRF_GRA },			// 14 
	{ "136051-1117.6cd",		0x008000, 0xcfc3f8a3,  0x0b | BRF_GRA },			// 15 
	{ "136051-1119.6de",		0x010000, 0x68850612,  0x0b | BRF_GRA },			// 16 
	{ "136051-1121.7de",		0x010000, 0xc7977062,  0x0b | BRF_GRA },			// 17 
	{ "136051-1123.7cd",		0x010000, 0x3c96c848,  0x0b | BRF_GRA },			// 18 

	{ "136051-1105.6t",			0x010000, 0x9b78a88e,  0x0c | BRF_GRA },			// 19 sprites
	{ "136051-1106.6rs",		0x010000, 0x4787ff58,  0x0c | BRF_GRA },			// 20 
	{ "136051-1107.6pr",		0x010000, 0x0e85f2ac,  0x0c | BRF_GRA },			// 21 
	{ "136051-1108.6n",			0x010000, 0x70ff9308,  0x0c | BRF_GRA },			// 22 
	{ "136051-1113.6m",			0x010000, 0x4a445356,  0x0c | BRF_GRA },			// 23 
	{ "136051-1114.6kl",		0x010000, 0xb9b27f3c,  0x0c | BRF_GRA },			// 24 
	{ "136051-1115.6jk",		0x010000, 0xa7671dd8,  0x0c | BRF_GRA },			// 25 
	{ "136051-1116.6h",			0x010000, 0x879fc7de,  0x0c | BRF_GRA },			// 26 
	{ "136051-1101.5t",			0x010000, 0x0ef13513,  0x0c | BRF_GRA },			// 27 
	{ "136051-1102.5rs",		0x010000, 0x401e06fd,  0x0c | BRF_GRA },			// 28 
	{ "136051-1103.5pr",		0x010000, 0x50d820e8,  0x0c | BRF_GRA },			// 29 
	{ "136051-1104.5n",			0x010000, 0x912d878f,  0x0c | BRF_GRA },			// 30 
	{ "136051-1109.5m",			0x010000, 0x6716a408,  0x0c | BRF_GRA },			// 31 
	{ "136051-1110.5kl",		0x010000, 0x7e184981,  0x0c | BRF_GRA },			// 32 
	{ "136051-1111.5jk",		0x010000, 0x353a14fd,  0x0c | BRF_GRA },			// 33 
	{ "136051-1112.5h",			0x010000, 0x3af7c50f,  0x0c | BRF_GRA },			// 34 

	{ "136051-1125.4t",			0x004000, 0x05a0341c,  0x05 | BRF_GRA },			// 35 Alpha Numeric Tiles
};

STD_ROM_PICK(apb3)
STD_ROM_FN(apb3)

struct BurnDriver BurnDrvApb3 = {
	"apb3", "apb", NULL, NULL, "1987",
	"APB - All Points Bulletin (rev 3)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 1, HARDWARE_MISC_PRE90S, GBF_RACING | GBF_ACTION, 0,
	NULL, apb3RomInfo, apb3RomName, NULL, NULL, NULL, NULL, ApbInputInfo, ApbDIPInfo,
	ApbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	384, 512, 3, 4
};


// APB - All Points Bulletin (rev 2)

static struct BurnRomInfo apb2RomDesc[] = {
	{ "136051-2126.7l",			0x004000, 0x8edf4726,  0x01 | BRF_PRG | BRF_ESS },	//  0 T11 Code
	{ "136051-2127.7n",			0x004000, 0xe2b2aff2,  0x01 | BRF_PRG | BRF_ESS },	//  1 
	{ "136051-2128.6f",			0x010000, 0x61a81436,  0x01 | BRF_PRG | BRF_ESS },	//  2 
	{ "136051-2129.6n",			0x010000, 0x24500ed6,  0x01 | BRF_PRG | BRF_ESS },	//  3 
	{ "136051-1130.6j",			0x010000, 0xf64c752e,  0x01 | BRF_PRG | BRF_ESS },	//  4 
	{ "136051-1131.6p",			0x010000, 0x0a506e04,  0x01 | BRF_PRG | BRF_ESS },	//  5 
	{ "136051-1132.6l",			0x010000, 0x6d0e7a4e,  0x01 | BRF_PRG | BRF_ESS },	//  6 
	{ "136051-1133.6s",			0x010000, 0xaf88d429,  0x01 | BRF_PRG | BRF_ESS },	//  7 

	{ "136051-1134.2a",			0x004000, 0xa65748b9,  0x02 | BRF_PRG | BRF_ESS },	//  8 M6502 Code
	{ "136051-1135.2bc",		0x004000, 0xe9692cea,  0x02 | BRF_PRG | BRF_ESS },	//  9 
	{ "136051-1136.2d",			0x004000, 0x92fc7657,  0x02 | BRF_PRG | BRF_ESS },	// 10 

	{ "136051-1118.6a",			0x008000, 0x93752c49,  0x0b | BRF_GRA },			// 11 Playfield Tiles
	{ "136051-1120.6bc",		0x010000, 0x043086f8,  0x0b | BRF_GRA },			// 12 
	{ "136051-1122.7a",			0x010000, 0x5ee79481,  0x0b | BRF_GRA },			// 13 
	{ "136051-1124.7bc",		0x010000, 0x27760395,  0x0b | BRF_GRA },			// 14 
	{ "136051-1117.6cd",		0x008000, 0xcfc3f8a3,  0x0b | BRF_GRA },			// 15 
	{ "136051-1119.6de",		0x010000, 0x68850612,  0x0b | BRF_GRA },			// 16 
	{ "136051-1121.7de",		0x010000, 0xc7977062,  0x0b | BRF_GRA },			// 17 
	{ "136051-1123.7cd",		0x010000, 0x3c96c848,  0x0b | BRF_GRA },			// 18 

	{ "136051-1105.6t",			0x010000, 0x9b78a88e,  0x0c | BRF_GRA },			// 19 sprites
	{ "136051-1106.6rs",		0x010000, 0x4787ff58,  0x0c | BRF_GRA },			// 20 
	{ "136051-1107.6pr",		0x010000, 0x0e85f2ac,  0x0c | BRF_GRA },			// 21 
	{ "136051-1108.6n",			0x010000, 0x70ff9308,  0x0c | BRF_GRA },			// 22 
	{ "136051-1113.6m",			0x010000, 0x4a445356,  0x0c | BRF_GRA },			// 23 
	{ "136051-1114.6kl",		0x010000, 0xb9b27f3c,  0x0c | BRF_GRA },			// 24 
	{ "136051-1115.6jk",		0x010000, 0xa7671dd8,  0x0c | BRF_GRA },			// 25 
	{ "136051-1116.6h",			0x010000, 0x879fc7de,  0x0c | BRF_GRA },			// 26 
	{ "136051-1101.5t",			0x010000, 0x0ef13513,  0x0c | BRF_GRA },			// 27 
	{ "136051-1102.5rs",		0x010000, 0x401e06fd,  0x0c | BRF_GRA },			// 28 
	{ "136051-1103.5pr",		0x010000, 0x50d820e8,  0x0c | BRF_GRA },			// 29 
	{ "136051-1104.5n",			0x010000, 0x912d878f,  0x0c | BRF_GRA },			// 30 
	{ "136051-1109.5m",			0x010000, 0x6716a408,  0x0c | BRF_GRA },			// 31 
	{ "136051-1110.5kl",		0x010000, 0x7e184981,  0x0c | BRF_GRA },			// 32 
	{ "136051-1111.5jk",		0x010000, 0x353a14fd,  0x0c | BRF_GRA },			// 33 
	{ "136051-1112.5h",			0x010000, 0x3af7c50f,  0x0c | BRF_GRA },			// 34 

	{ "136051-1125.4t",			0x004000, 0x05a0341c,  0x05 | BRF_GRA },			// 35 Alpha Numeric Tiles
};

STD_ROM_PICK(apb2)
STD_ROM_FN(apb2)

struct BurnDriver BurnDrvApb2 = {
	"apb2", "apb", NULL, NULL, "1987",
	"APB - All Points Bulletin (rev 2)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 1, HARDWARE_MISC_PRE90S, GBF_RACING | GBF_ACTION, 0,
	NULL, apb2RomInfo, apb2RomName, NULL, NULL, NULL, NULL, ApbInputInfo, ApbDIPInfo,
	ApbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	384, 512, 3, 4
};


// APB - All Points Bulletin (rev 1)

static struct BurnRomInfo apb1RomDesc[] = {
	{ "136051-1126.7l",			0x004000, 0xd385994c,  0x01 | BRF_PRG | BRF_ESS },	//  0 T11 Code
	{ "136051-1127.7n",			0x004000, 0x9b40b0b4,  0x01 | BRF_PRG | BRF_ESS },	//  1 
	{ "136051-1128.6f",			0x010000, 0x8d5d9f4a,  0x01 | BRF_PRG | BRF_ESS },	//  2 
	{ "136051-1129.6n",			0x010000, 0x2948cef0,  0x01 | BRF_PRG | BRF_ESS },	//  3 
	{ "136051-1130.6j",			0x010000, 0xf64c752e,  0x01 | BRF_PRG | BRF_ESS },	//  4 
	{ "136051-1131.6p",			0x010000, 0x0a506e04,  0x01 | BRF_PRG | BRF_ESS },	//  5 
	{ "136051-1132.6l",			0x010000, 0x6d0e7a4e,  0x01 | BRF_PRG | BRF_ESS },	//  6 
	{ "136051-1133.6s",			0x010000, 0xaf88d429,  0x01 | BRF_PRG | BRF_ESS },	//  7 

	{ "136051-1134.2a",			0x004000, 0xa65748b9,  0x02 | BRF_PRG | BRF_ESS },	//  8 M6502 Code
	{ "136051-1135.2bc",		0x004000, 0xe9692cea,  0x02 | BRF_PRG | BRF_ESS },	//  9 
	{ "136051-1136.2d",			0x004000, 0x92fc7657,  0x02 | BRF_PRG | BRF_ESS },	// 10 

	{ "136051-1118.6a",			0x008000, 0x93752c49,  0x0b | BRF_GRA },			// 11 Playfield Tiles
	{ "136051-1120.6bc",		0x010000, 0x043086f8,  0x0b | BRF_GRA },			// 12 
	{ "136051-1122.7a",			0x010000, 0x5ee79481,  0x0b | BRF_GRA },			// 13 
	{ "136051-1124.7bc",		0x010000, 0x27760395,  0x0b | BRF_GRA },			// 14 
	{ "136051-1117.6cd",		0x008000, 0xcfc3f8a3,  0x0b | BRF_GRA },			// 15 
	{ "136051-1119.6de",		0x010000, 0x68850612,  0x0b | BRF_GRA },			// 16 
	{ "136051-1121.7de",		0x010000, 0xc7977062,  0x0b | BRF_GRA },			// 17 
	{ "136051-1123.7cd",		0x010000, 0x3c96c848,  0x0b | BRF_GRA },			// 18 

	{ "136051-1105.6t",			0x010000, 0x9b78a88e,  0x0c | BRF_GRA },			// 19 sprites
	{ "136051-1106.6rs",		0x010000, 0x4787ff58,  0x0c | BRF_GRA },			// 20 
	{ "136051-1107.6pr",		0x010000, 0x0e85f2ac,  0x0c | BRF_GRA },			// 21 
	{ "136051-1108.6n",			0x010000, 0x70ff9308,  0x0c | BRF_GRA },			// 22 
	{ "136051-1113.6m",			0x010000, 0x4a445356,  0x0c | BRF_GRA },			// 23 
	{ "136051-1114.6kl",		0x010000, 0xb9b27f3c,  0x0c | BRF_GRA },			// 24 
	{ "136051-1115.6jk",		0x010000, 0xa7671dd8,  0x0c | BRF_GRA },			// 25 
	{ "136051-1116.6h",			0x010000, 0x879fc7de,  0x0c | BRF_GRA },			// 26 
	{ "136051-1101.5t",			0x010000, 0x0ef13513,  0x0c | BRF_GRA },			// 27 
	{ "136051-1102.5rs",		0x010000, 0x401e06fd,  0x0c | BRF_GRA },			// 28 
	{ "136051-1103.5pr",		0x010000, 0x50d820e8,  0x0c | BRF_GRA },			// 29 
	{ "136051-1104.5n",			0x010000, 0x912d878f,  0x0c | BRF_GRA },			// 30 
	{ "136051-1109.5m",			0x010000, 0x6716a408,  0x0c | BRF_GRA },			// 31 
	{ "136051-1110.5kl",		0x010000, 0x7e184981,  0x0c | BRF_GRA },			// 32 
	{ "136051-1111.5jk",		0x010000, 0x353a14fd,  0x0c | BRF_GRA },			// 33 
	{ "136051-1112.5h",			0x010000, 0x3af7c50f,  0x0c | BRF_GRA },			// 34 

	{ "136051-1125.4t",			0x004000, 0x05a0341c,  0x05 | BRF_GRA },			// 35 Alpha Numeric Tiles
};

STD_ROM_PICK(apb1)
STD_ROM_FN(apb1)

struct BurnDriver BurnDrvApb1 = {
	"apb1", "apb", NULL, NULL, "1987",
	"APB - All Points Bulletin (rev 1)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 1, HARDWARE_MISC_PRE90S, GBF_RACING | GBF_ACTION, 0,
	NULL, apb1RomInfo, apb1RomName, NULL, NULL, NULL, NULL, ApbInputInfo, ApbDIPInfo,
	ApbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	384, 512, 3, 4
};


// APB - All Points Bulletin (German)

static struct BurnRomInfo apbgRomDesc[] = {
	{ "136051-2126.7l",			0x004000, 0x8edf4726,  0x01 | BRF_PRG | BRF_ESS },	//  0 T11 Code
	{ "136051-2127.7n",			0x004000, 0xe2b2aff2,  0x01 | BRF_PRG | BRF_ESS },	//  1 
	{ "136051-1228.6f",			0x010000, 0x44781913,  0x01 | BRF_PRG | BRF_ESS },	//  2 
	{ "136051-1229.6n",			0x010000, 0xf18afffd,  0x01 | BRF_PRG | BRF_ESS },	//  3 
	{ "136051-1130.6j",			0x010000, 0xf64c752e,  0x01 | BRF_PRG | BRF_ESS },	//  4 
	{ "136051-1131.6p",			0x010000, 0x0a506e04,  0x01 | BRF_PRG | BRF_ESS },	//  5 
	{ "136051-1132.6l",			0x010000, 0x6d0e7a4e,  0x01 | BRF_PRG | BRF_ESS },	//  6 
	{ "136051-1133.6s",			0x010000, 0xaf88d429,  0x01 | BRF_PRG | BRF_ESS },	//  7 

	{ "136051-4134.2a",			0x004000, 0x45e03b0e,  0x02 | BRF_PRG | BRF_ESS },	//  8 M6502 Code
	{ "136051-4135.2bc",		0x004000, 0xb4ca24b2,  0x02 | BRF_PRG | BRF_ESS },	//  9 
	{ "136051-4136.2d",			0x004000, 0x11efaabf,  0x02 | BRF_PRG | BRF_ESS },	// 10 

	{ "136051-1118.6a",			0x008000, 0x93752c49,  0x0b | BRF_GRA },			// 11 Playfield Tiles
	{ "136051-1120.6bc",		0x010000, 0x043086f8,  0x0b | BRF_GRA },			// 12 
	{ "136051-1122.7a",			0x010000, 0x5ee79481,  0x0b | BRF_GRA },			// 13 
	{ "136051-1124.7bc",		0x010000, 0x27760395,  0x0b | BRF_GRA },			// 14 
	{ "136051-1117.6cd",		0x008000, 0xcfc3f8a3,  0x0b | BRF_GRA },			// 15 
	{ "136051-1119.6de",		0x010000, 0x68850612,  0x0b | BRF_GRA },			// 16 
	{ "136051-1121.7de",		0x010000, 0xc7977062,  0x0b | BRF_GRA },			// 17 
	{ "136051-1123.7cd",		0x010000, 0x3c96c848,  0x0b | BRF_GRA },			// 18 

	{ "136051-1105.6t",			0x010000, 0x9b78a88e,  0x0c | BRF_GRA },			// 19 sprites
	{ "136051-1106.6rs",		0x010000, 0x4787ff58,  0x0c | BRF_GRA },			// 20 
	{ "136051-1107.6pr",		0x010000, 0x0e85f2ac,  0x0c | BRF_GRA },			// 21 
	{ "136051-1108.6n",			0x010000, 0x70ff9308,  0x0c | BRF_GRA },			// 22 
	{ "136051-1113.6m",			0x010000, 0x4a445356,  0x0c | BRF_GRA },			// 23 
	{ "136051-1114.6kl",		0x010000, 0xb9b27f3c,  0x0c | BRF_GRA },			// 24 
	{ "136051-1115.6jk",		0x010000, 0xa7671dd8,  0x0c | BRF_GRA },			// 25 
	{ "136051-1116.6h",			0x010000, 0x879fc7de,  0x0c | BRF_GRA },			// 26 
	{ "136051-1101.5t",			0x010000, 0x0ef13513,  0x0c | BRF_GRA },			// 27 
	{ "136051-1102.5rs",		0x010000, 0x401e06fd,  0x0c | BRF_GRA },			// 28 
	{ "136051-1103.5pr",		0x010000, 0x50d820e8,  0x0c | BRF_GRA },			// 29 
	{ "136051-1104.5n",			0x010000, 0x912d878f,  0x0c | BRF_GRA },			// 30 
	{ "136051-1109.5m",			0x010000, 0x6716a408,  0x0c | BRF_GRA },			// 31 
	{ "136051-1110.5kl",		0x010000, 0x7e184981,  0x0c | BRF_GRA },			// 32 
	{ "136051-1111.5jk",		0x010000, 0x353a14fd,  0x0c | BRF_GRA },			// 33 
	{ "136051-1112.5h",			0x010000, 0x3af7c50f,  0x0c | BRF_GRA },			// 34 

	{ "136051-1125.4t",			0x004000, 0x05a0341c,  0x05 | BRF_GRA },			// 35 Alpha Numeric Tiles
};

STD_ROM_PICK(apbg)
STD_ROM_FN(apbg)

struct BurnDriver BurnDrvApbg = {
	"apbg", "apb", NULL, NULL, "1987",
	"APB - All Points Bulletin (German)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 1, HARDWARE_MISC_PRE90S, GBF_RACING | GBF_ACTION, 0,
	NULL, apbgRomInfo, apbgRomName, NULL, NULL, NULL, NULL, ApbInputInfo, ApbDIPInfo,
	ApbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	384, 512, 3, 4
};


// APB - All Points Bulletin (French)

static struct BurnRomInfo apbfRomDesc[] = {
	{ "136051-2126.7l",			0x004000, 0x8edf4726,  0x01 | BRF_PRG | BRF_ESS },	//  0 T11 Code
	{ "136051-2127.7n",			0x004000, 0xe2b2aff2,  0x01 | BRF_PRG | BRF_ESS },	//  1 
	{ "136051-1628.6f",			0x010000, 0x075e9a18,  0x01 | BRF_PRG | BRF_ESS },	//  2 
	{ "136051-1629.6n",			0x010000, 0x8951514a,  0x01 | BRF_PRG | BRF_ESS },	//  3 
	{ "136051-1130.6j",			0x010000, 0xf64c752e,  0x01 | BRF_PRG | BRF_ESS },	//  4 
	{ "136051-1131.6p",			0x010000, 0x0a506e04,  0x01 | BRF_PRG | BRF_ESS },	//  5 
	{ "136051-1132.6l",			0x010000, 0x6d0e7a4e,  0x01 | BRF_PRG | BRF_ESS },	//  6 
	{ "136051-1133.6s",			0x010000, 0xaf88d429,  0x01 | BRF_PRG | BRF_ESS },	//  7 

	{ "136051-5134.2a",			0x004000, 0x1c8bdeed,  0x02 | BRF_PRG | BRF_ESS },	//  8 M6502 Code
	{ "136051-5135.2bc",		0x004000, 0xed6adb91,  0x02 | BRF_PRG | BRF_ESS },	//  9 
	{ "136051-5136.2d",			0x004000, 0x341f8486,  0x02 | BRF_PRG | BRF_ESS },	// 10 

	{ "136051-1118.6a",			0x008000, 0x93752c49,  0x0b | BRF_GRA },			// 11 Playfield Tiles
	{ "136051-1120.6bc",		0x010000, 0x043086f8,  0x0b | BRF_GRA },			// 12 
	{ "136051-1122.7a",			0x010000, 0x5ee79481,  0x0b | BRF_GRA },			// 13 
	{ "136051-1124.7bc",		0x010000, 0x27760395,  0x0b | BRF_GRA },			// 14 
	{ "136051-1117.6cd",		0x008000, 0xcfc3f8a3,  0x0b | BRF_GRA },			// 15 
	{ "136051-1119.6de",		0x010000, 0x68850612,  0x0b | BRF_GRA },			// 16 
	{ "136051-1121.7de",		0x010000, 0xc7977062,  0x0b | BRF_GRA },			// 17 
	{ "136051-1123.7cd",		0x010000, 0x3c96c848,  0x0b | BRF_GRA },			// 18 

	{ "136051-1105.6t",			0x010000, 0x9b78a88e,  0x0c | BRF_GRA },			// 19 sprites
	{ "136051-1106.6rs",		0x010000, 0x4787ff58,  0x0c | BRF_GRA },			// 20 
	{ "136051-1107.6pr",		0x010000, 0x0e85f2ac,  0x0c | BRF_GRA },			// 21 
	{ "136051-1108.6n",			0x010000, 0x70ff9308,  0x0c | BRF_GRA },			// 22 
	{ "136051-1113.6m",			0x010000, 0x4a445356,  0x0c | BRF_GRA },			// 23 
	{ "136051-1114.6kl",		0x010000, 0xb9b27f3c,  0x0c | BRF_GRA },			// 24 
	{ "136051-1115.6jk",		0x010000, 0xa7671dd8,  0x0c | BRF_GRA },			// 25 
	{ "136051-1116.6h",			0x010000, 0x879fc7de,  0x0c | BRF_GRA },			// 26 
	{ "136051-1101.5t",			0x010000, 0x0ef13513,  0x0c | BRF_GRA },			// 27 
	{ "136051-1102.5rs",		0x010000, 0x401e06fd,  0x0c | BRF_GRA },			// 28 
	{ "136051-1103.5pr",		0x010000, 0x50d820e8,  0x0c | BRF_GRA },			// 29 
	{ "136051-1104.5n",			0x010000, 0x912d878f,  0x0c | BRF_GRA },			// 30 
	{ "136051-1109.5m",			0x010000, 0x6716a408,  0x0c | BRF_GRA },			// 31 
	{ "136051-1110.5kl",		0x010000, 0x7e184981,  0x0c | BRF_GRA },			// 32 
	{ "136051-1111.5jk",		0x010000, 0x353a14fd,  0x0c | BRF_GRA },			// 33 
	{ "136051-1112.5h",			0x010000, 0x3af7c50f,  0x0c | BRF_GRA },			// 34 

	{ "136051-1125.4t",			0x004000, 0x05a0341c,  0x05 | BRF_GRA },			// 35 Alpha Numeric Tiles
};

STD_ROM_PICK(apbf)
STD_ROM_FN(apbf)

struct BurnDriver BurnDrvApbf = {
	"apbf", "apb", NULL, NULL, "1987",
	"APB - All Points Bulletin (French)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 1, HARDWARE_MISC_PRE90S, GBF_RACING | GBF_ACTION, 0,
	NULL, apbfRomInfo, apbfRomName, NULL, NULL, NULL, NULL, ApbInputInfo, ApbDIPInfo,
	ApbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	384, 512, 3, 4
};
