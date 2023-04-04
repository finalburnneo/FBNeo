// FinalBurn Neo Jaleco Cisco Heat / Big Run / Grand Prix Star / Arm Champs II / Wild Pilot / Scud hammer driver module
// Based on MAME driver by Luca Elia

/*
	known issues:
		wild pilot & cisco heat has gfx bugs (mame too)
		captflag is not added and not worthwhile
*/

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "burn_ym2151.h"
#include "msm6295.h"
#include "bitswap.h"
#include "burn_shift.h"
#include "burn_gun.h" // wildplt

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *Drv68KROM[5];
static UINT8 *DrvGfxROM[6];
static UINT8 *DrvSndROM[2];
static UINT8 *Drv68KRAM[5];
static UINT8 *DrvShareRAM[2];
static UINT8 *DrvScrRAM[3];
static UINT8 *DrvRoadRAM[2];
static UINT8 *DrvIORAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvNetworkRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprRAMBuf;

static UINT16 *scrollx;
static UINT16 *scrolly;
static UINT16 *scroll_flag;
static UINT16 *soundlatch;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT16 soundbank0;
static UINT16 soundbank1;
static UINT16 motor_value;
static UINT16 ip_select;
static INT32 mux_data;
static INT32 io_ready;
static INT32 io_value;
static INT32 sprite_dma_reg;

static UINT8 shadows[16];
static INT32 scroll_factor_8x8[3] = { 1, 1, 1 };
static INT32 screen_adjust_y = 0;
static INT32 sek_clock = 10000000;
static INT32 nDrvGfxROMLen[6];
static INT32 nDrv68KROMLen[6];

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvDips[4];
static UINT16 DrvInputs[3];
static UINT8 DrvReset;

static INT16 Analog0;
static INT16 Analog1;
static INT16 Analog2;
static INT16 Analog3;

static INT32 is_game = 0; // 0 bigrun, cischeat, 1 f1gpstar

#define A(a, b, c, d) {a, b, (UINT8*)(c), d} // A - for analog happytime
static struct BurnInputInfo BigrunInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},

	A("P1 Steering",	BIT_ANALOG_REL,	&Analog0,		"p1 x-axis"	),
	A("P1 Accelerator",	BIT_ANALOG_REL,	&Analog1,		"p1 fire 1"	),
	{"P1 Brake",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 2"	},
	{"P1 Gear Shift",	BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 3"	},
	{"P1 Horn",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 fire 4"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy1 + 3,	"diag"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",			BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};

STDINPUTINFO(Bigrun)

static struct BurnInputInfo CischeatInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},

	A("P1 Steering",	BIT_ANALOG_REL,	&Analog0,		"p1 x-axis"	),
	{"P1 Accelerator",	BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Brake",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 2"	},
	{"P1 Gear Shift",	BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 3"	},
	{"P1 Horn",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 fire 4"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy1 + 3,	"diag"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",			BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};

STDINPUTINFO(Cischeat)

static struct BurnInputInfo F1gpstarInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},

	A("P1 Steering",	BIT_ANALOG_REL,	&Analog0,		"p1 x-axis"	),
	A("P1 Accelerator",	BIT_ANALOG_REL,	&Analog1,		"p1 fire 1"	),
	{"P1 Gear Shift",	BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Brake",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy1 + 3,	"diag"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(F1gpstar)

static struct BurnInputInfo WildpltInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Shot",			BIT_DIGITAL,	DrvJoy3 + 6,	"p1 fire 1"	},
	{"P1 Missile",		BIT_DIGITAL,	DrvJoy3 + 7,	"p1 fire 2"	},
	{"P1 Bomb",			BIT_DIGITAL,	DrvJoy3 + 1,	"p1 fire 3"	},

	A("P1 Gun X",		BIT_ANALOG_REL, &Analog0,		"p1 x-axis"	),
	A("P1 Gun Y",		BIT_ANALOG_REL, &Analog1,		"p1 y-axis"	),

	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Shot",			BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Missile",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},
	{"P2 Bomb",			BIT_DIGITAL,	DrvJoy3 + 8,	"p2 fire 3"	},

	A("P2 Gun X",		BIT_ANALOG_REL, &Analog2,		"p2 x-axis"	),
	A("P2 Gun Y",		BIT_ANALOG_REL, &Analog3,		"p2 y-axis"	),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 2,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy2 + 3,	"diag"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Wildplt)

static struct BurnInputInfo ScudhammInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	{"P1 Select",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 start"	},
	{"P1 Rock",			BIT_DIGITAL,	DrvJoy1 + 8,	"p1 fire 1"	},
	{"P1 Scissors",		BIT_DIGITAL,	DrvJoy1 + 9,	"p1 fire 2"	},
	{"P1 Paper",		BIT_DIGITAL,	DrvJoy1 + 10,	"p1 fire 3"	},
	A("P1 Hit",			BIT_ANALOG_REL,	&Analog0,		"p1 x-axis"	),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy1 + 3,	"diag"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 14,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Scudhamm)

static struct BurnInputInfo Armchmp2InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 8,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 9,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 10,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 11,	"p1 fire 3"	},
	A("P1 Hit",			BIT_ANALOG_REL,	&Analog0,		"p1 x-axis"	),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 3,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy1 + 4,	"diag"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Armchmp2)

static struct BurnDIPInfo BigrunDIPList[]=
{
	{0x0a, 0xff, 0xff, 0xff, NULL						},
	{0x0b, 0xff, 0xff, 0xff, NULL						},
	{0x0c, 0xff, 0xff, 0xff, NULL						},
	{0x0d, 0xff, 0xff, 0xf8, NULL						},

	{0   , 0xfe, 0   ,    2, "Up Limit SW"				},
	{0x0a, 0x01, 0x01, 0x01, "Off"						},
	{0x0a, 0x01, 0x01, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Down Limit SW"			},
	{0x0a, 0x01, 0x02, 0x02, "Off"						},
	{0x0a, 0x01, 0x02, 0x00, "On"						},

	{0   , 0xfe, 0   ,    7, "Extra Setting For Coin B"	},
	{0x0b, 0x01, 0x03, 0x03, "Unused"					},
	{0x0b, 0x01, 0x03, 0x01, "1 Coin  5 Credits"		},
	{0x0b, 0x01, 0x03, 0x02, "1 Coin  6 Credits"		},
	{0x0b, 0x01, 0x03, 0x00, "1 Coin  7 Credits"		},
	{0x0b, 0x01, 0x03, 0x01, "Unused"					},
	{0x0b, 0x01, 0x03, 0x02, "Unused"					},
	{0x0b, 0x01, 0x03, 0x00, "Unused"					},

	{0   , 0xfe, 0   ,    2, "Allow Continue"			},
	{0x0b, 0x01, 0x04, 0x04, "No"						},
	{0x0b, 0x01, 0x04, 0x00, "Yes"						},

	{0   , 0xfe, 0   ,    2, "Region"					},
	{0x0b, 0x01, 0x08, 0x08, "Japanese"					},
	{0x0b, 0x01, 0x08, 0x00, "English"					},

	{0   , 0xfe, 0   ,    2, "Move Cabinet"				},
	{0x0b, 0x01, 0x10, 0x00, "No"						},
	{0x0b, 0x01, 0x10, 0x10, "Yes"						},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x0b, 0x01, 0x60, 0x00, "Easy"						},
	{0x0b, 0x01, 0x60, 0x60, "Normal"					},
	{0x0b, 0x01, 0x60, 0x20, "Hard"						},
	{0x0b, 0x01, 0x60, 0x40, "Hardest"					},

	{0   , 0xfe, 0   ,    2, "Automatic Game Start"		},
	{0x0b, 0x01, 0x80, 0x00, "No"						},
	{0x0b, 0x01, 0x80, 0x80, "After 15 Seconds"			},

	{0   , 0xfe, 0   ,    2, "Invulnerability"			},
	{0x0c, 0x01, 0x01, 0x01, "Off"						},
	{0x0c, 0x01, 0x01, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x0c, 0x01, 0x02, 0x00, "Off"						},
	{0x0c, 0x01, 0x02, 0x02, "On"						},

	{0   , 0xfe, 0   ,    8, "Coin B"					},
	{0x0c, 0x01, 0x1c, 0x10, "4 Coins 1 Credits"		},
	{0x0c, 0x01, 0x1c, 0x08, "3 Coins 1 Credits"		},
	{0x0c, 0x01, 0x1c, 0x18, "2 Coins 1 Credits"		},
	{0x0c, 0x01, 0x1c, 0x1c, "1 Coin  1 Credits"		},
	{0x0c, 0x01, 0x1c, 0x0c, "1 Coin  2 Credits"		},
	{0x0c, 0x01, 0x1c, 0x14, "1 Coin  3 Credits"		},
	{0x0c, 0x01, 0x1c, 0x04, "1 Coin  4 Credits"		},
	{0x0c, 0x01, 0x1c, 0x00, "Free Play"				},

	{0   , 0xfe, 0   ,    2, "Set Coin B"				},
	{0x0c, 0x01, 0x1c, 0x1c, "Unused"					},
	{0x0c, 0x01, 0x1c, 0x00, "Free Play"				},

	{0   , 0xfe, 0   ,    8, "Coin A"					},
	{0x0c, 0x01, 0xe0, 0x80, "4 Coins 1 Credits"		},
	{0x0c, 0x01, 0xe0, 0x40, "3 Coins 1 Credits"		},
	{0x0c, 0x01, 0xe0, 0xc0, "2 Coins 1 Credits"		},
	{0x0c, 0x01, 0xe0, 0xe0, "1 Coin  1 Credits"		},
	{0x0c, 0x01, 0xe0, 0x60, "1 Coin  2 Credits"		},
	{0x0c, 0x01, 0xe0, 0xa0, "1 Coin  3 Credits"		},
	{0x0c, 0x01, 0xe0, 0x20, "1 Coin  4 Credits"		},
	{0x0c, 0x01, 0xe0, 0x00, "Free Play"				},

	{0   , 0xfe, 0   ,    2, "Link ID"					},
	{0x0d, 0x01, 0x01, 0x00, "Master Unit"				},
	{0x0d, 0x01, 0x01, 0x01, "Other Units"				},

	{0   , 0xfe, 0   ,    4, "Unit ID"					},
	{0x0d, 0x01, 0x06, 0x00, "1 (Blue-White Car)"		},
	{0x0d, 0x01, 0x06, 0x02, "2 (Green-White Car)"		},
	{0x0d, 0x01, 0x06, 0x04, "3 (Red-White Car)"		},
	{0x0d, 0x01, 0x06, 0x06, "4 (Yellow Car)"			},
};

STDDIPINFO(Bigrun)

static struct BurnDIPInfo CischeatDIPList[]=
{
	{0x0a, 0xff, 0xff, 0x77, NULL						},
	{0x0b, 0xff, 0xff, 0x3f, NULL						},
	{0x0c, 0xff, 0xff, 0xff, NULL						},
	{0x0d, 0xff, 0xff, 0x16, NULL						},

	{0   , 0xfe, 0   ,    2, "Up Limit SW"				},
	{0x0a, 0x01, 0x01, 0x01, "Off"						},
	{0x0a, 0x01, 0x01, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Down Limit SW"			},
	{0x0a, 0x01, 0x02, 0x02, "Off"						},
	{0x0a, 0x01, 0x02, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Right Limit SW"			},
	{0x0a, 0x01, 0x10, 0x10, "Off"						},
	{0x0a, 0x01, 0x10, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Left Limit SW"			},
	{0x0a, 0x01, 0x20, 0x20, "Off"						},
	{0x0a, 0x01, 0x20, 0x00, "On"						},

	{0   , 0xfe, 0   ,    8, "Coin A"					},
	{0x0b, 0x01, 0x07, 0x01, "4 Coins 1 Credits"		},
	{0x0b, 0x01, 0x07, 0x02, "3 Coins 1 Credits"		},
	{0x0b, 0x01, 0x07, 0x03, "2 Coins 1 Credits"		},
	{0x0b, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x0b, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x0b, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x0b, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},
	{0x0b, 0x01, 0x07, 0x00, "Free Play"				},

	{0   , 0xfe, 0   ,    8, "Coin B"					},
	{0x0b, 0x01, 0x38, 0x08, "4 Coins 1 Credits"		},
	{0x0b, 0x01, 0x38, 0x10, "3 Coins 1 Credits"		},
	{0x0b, 0x01, 0x38, 0x18, "2 Coins 1 Credits"		},
	{0x0b, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x0b, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x0b, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x0b, 0x01, 0x38, 0x20, "1 Coin  4 Credits"		},
	{0x0b, 0x01, 0x38, 0x00, "Free Play"				},

	{0   , 0xfe, 0   ,    4, "Unit ID"					},
	{0x0c, 0x01, 0x03, 0x03, "0 (Red Car)"				},
	{0x0c, 0x01, 0x03, 0x02, "1 (Blue Car)"				},
	{0x0c, 0x01, 0x03, 0x01, "2 (Yellow Car)"			},
	{0x0c, 0x01, 0x03, 0x00, "3 (Green Car)"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x0c, 0x01, 0x0c, 0x00, "Easy"						},
	{0x0c, 0x01, 0x0c, 0x0c, "Normal"					},
	{0x0c, 0x01, 0x0c, 0x08, "Hard"						},
	{0x0c, 0x01, 0x0c, 0x04, "Hardest"					},

	{0   , 0xfe, 0   ,    2, "Infinite Time (Cheat)"	},
	{0x0c, 0x01, 0x10, 0x10, "Off"						},
	{0x0c, 0x01, 0x10, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x0c, 0x01, 0x20, 0x00, "Off"						},
	{0x0c, 0x01, 0x20, 0x20, "On"						},

	{0   , 0xfe, 0   ,    2, "Country"					},
	{0x0c, 0x01, 0x40, 0x40, "Japan"					},
	{0x0c, 0x01, 0x40, 0x00, "USA"						},

	{0   , 0xfe, 0   ,    2, "Allow Continue"			},
	{0x0c, 0x01, 0x80, 0x80, "Off"						},
	{0x0c, 0x01, 0x80, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Unit ID (2)"				},
	{0x0d, 0x01, 0x06, 0x06, "Use other"				},
	{0x0d, 0x01, 0x06, 0x00, "0 (Red Car)"				},
	{0x0d, 0x01, 0x06, 0x02, "1 (Blue Car)"				},
	{0x0d, 0x01, 0x06, 0x04, "2 (Yellow Car)"			},
};

STDDIPINFO(Cischeat)

static struct BurnDIPInfo F1gpstarDIPList[]=
{
	{0x09, 0xff, 0xff, 0xff, NULL						},
	{0x0a, 0xff, 0xff, 0xff, NULL						},
	{0x0b, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    8, "Coin A (JP US)"			},
	{0x09, 0x01, 0x07, 0x01, "4 Coins 1 Credits"		},
	{0x09, 0x01, 0x07, 0x02, "3 Coins 1 Credits"		},
	{0x09, 0x01, 0x07, 0x03, "2 Coins 1 Credits"		},
	{0x09, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x09, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x09, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x09, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},
	{0x09, 0x01, 0x07, 0x00, "Free Play"				},

	{0   , 0xfe, 0   ,    7, "Coin B (JP US)"			},
	{0x09, 0x01, 0x38, 0x08, "4 Coins 1 Credits"		},
	{0x09, 0x01, 0x38, 0x10, "3 Coins 1 Credits"		},
	{0x09, 0x01, 0x38, 0x18, "2 Coins 1 Credits"		},
	{0x09, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x09, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x09, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x09, 0x01, 0x38, 0x20, "1 Coin  4 Credits"		},

	{0   , 0xfe, 0   ,    8, "Coin A (EU FR)"			},
	{0x09, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x09, 0x01, 0x07, 0x00, "2 Coins 3 Credits"		},
	{0x09, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x09, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x09, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},
	{0x09, 0x01, 0x07, 0x03, "1 Coin  5 Credits"		},
	{0x09, 0x01, 0x07, 0x02, "1 Coin  6 Credits"		},
	{0x09, 0x01, 0x07, 0x01, "1 Coin  7 Credits"		},

	{0   , 0xfe, 0   ,    8, "Coin B (EU FR)"			},
	{0x09, 0x01, 0x38, 0x00, "5 Coins 1 Credits"		},
	{0x09, 0x01, 0x38, 0x08, "4 Coins 1 Credits"		},
	{0x09, 0x01, 0x38, 0x10, "3 Coins 1 Credits"		},
	{0x09, 0x01, 0x38, 0x18, "2 Coins 1 Credits"		},
	{0x09, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x09, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x09, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x09, 0x01, 0x38, 0x20, "1 Coin  4 Credits"		},

	{0   , 0xfe, 0   ,    2, "Free Play (EU & FR)"		},
	{0x09, 0x01, 0x40, 0x40, "Off"						},
	{0x09, 0x01, 0x40, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Region"					},
	{0x0a, 0x01, 0x03, 0x03, "Japan"					},
	{0x0a, 0x01, 0x03, 0x02, "USA"						},
	{0x0a, 0x01, 0x03, 0x01, "Europe"					},
	{0x0a, 0x01, 0x03, 0x00, "France"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x0a, 0x01, 0x0c, 0x00, "Easy"						},
	{0x0a, 0x01, 0x0c, 0x0c, "Normal"					},
	{0x0a, 0x01, 0x0c, 0x08, "Hard"						},
	{0x0a, 0x01, 0x0c, 0x04, "Very Hard"				},

	{0   , 0xfe, 0   ,    2, "Infinite Time (Cheat)"	},
	{0x0a, 0x01, 0x10, 0x10, "Off"						},
	{0x0a, 0x01, 0x10, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x0a, 0x01, 0x20, 0x00, "Off"						},
	{0x0a, 0x01, 0x20, 0x20, "On"						},

	{0   , 0xfe, 0   ,    2, "Choose Race (US EU FR)"	},
	{0x0a, 0x01, 0x40, 0x00, "Off"						},
	{0x0a, 0x01, 0x40, 0x40, "On"						},

	{0   , 0xfe, 0   ,    2, "Vibrations"				},
	{0x0a, 0x01, 0x80, 0x80, "Torque"					},
	{0x0a, 0x01, 0x80, 0x00, "Shake"					},

	{0   , 0xfe, 0   ,    2, "This Unit Is"				},
	{0x0b, 0x01, 0x01, 0x01, "Slave"					},
	{0x0b, 0x01, 0x01, 0x00, "Master"					},

	{0   , 0xfe, 0   ,    4, "Unit ID"					},
	{0x0b, 0x01, 0x06, 0x06, "0 (Red-White Car)"		},
	{0x0b, 0x01, 0x06, 0x04, "1 (Red Car)"				},
	{0x0b, 0x01, 0x06, 0x02, "2 (Blue-White Car)"		},
	{0x0b, 0x01, 0x06, 0x00, "3 (Blue Car)"				},
};

STDDIPINFO(F1gpstar)

static struct BurnDIPInfo F1gpstr2DIPList[]=
{
	{0x09, 0xff, 0xff, 0xff, NULL						},
	{0x0a, 0xff, 0xff, 0xff, NULL						},
	{0x0b, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    8, "Coin A (JP US)"			},
	{0x09, 0x01, 0x07, 0x01, "4 Coins 1 Credits"		},
	{0x09, 0x01, 0x07, 0x02, "3 Coins 1 Credits"		},
	{0x09, 0x01, 0x07, 0x03, "2 Coins 1 Credits"		},
	{0x09, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x09, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x09, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x09, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},
	{0x09, 0x01, 0x07, 0x00, "Free Play"				},

	{0   , 0xfe, 0   ,    7, "Coin B (JP US)"			},
	{0x09, 0x01, 0x38, 0x08, "4 Coins 1 Credits"		},
	{0x09, 0x01, 0x38, 0x10, "3 Coins 1 Credits"		},
	{0x09, 0x01, 0x38, 0x18, "2 Coins 1 Credits"		},
	{0x09, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x09, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x09, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x09, 0x01, 0x38, 0x20, "1 Coin  4 Credits"		},

	{0   , 0xfe, 0   ,    8, "Coin A (EU FR)"			},
	{0x09, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x09, 0x01, 0x07, 0x00, "2 Coins 3 Credits"		},
	{0x09, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x09, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x09, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},
	{0x09, 0x01, 0x07, 0x03, "1 Coin  5 Credits"		},
	{0x09, 0x01, 0x07, 0x02, "1 Coin  6 Credits"		},
	{0x09, 0x01, 0x07, 0x01, "1 Coin  7 Credits"		},

	{0   , 0xfe, 0   ,    8, "Coin B (EU FR)"			},
	{0x09, 0x01, 0x38, 0x00, "5 Coins 1 Credits"		},
	{0x09, 0x01, 0x38, 0x08, "4 Coins 1 Credits"		},
	{0x09, 0x01, 0x38, 0x10, "3 Coins 1 Credits"		},
	{0x09, 0x01, 0x38, 0x18, "2 Coins 1 Credits"		},
	{0x09, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x09, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x09, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x09, 0x01, 0x38, 0x20, "1 Coin  4 Credits"		},

	{0   , 0xfe, 0   ,    2, "Free Play (EU & FR)"		},
	{0x09, 0x01, 0x40, 0x40, "Off"						},
	{0x09, 0x01, 0x40, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Region"					},
	{0x0a, 0x01, 0x03, 0x03, "Japan"					},
	{0x0a, 0x01, 0x03, 0x02, "USA"						},
	{0x0a, 0x01, 0x03, 0x01, "Europe"					},
	{0x0a, 0x01, 0x03, 0x00, "France"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x0a, 0x01, 0x0c, 0x00, "Easy"						},
	{0x0a, 0x01, 0x0c, 0x0c, "Normal"					},
	{0x0a, 0x01, 0x0c, 0x08, "Hard"						},
	{0x0a, 0x01, 0x0c, 0x04, "Very Hard"				},

	{0   , 0xfe, 0   ,    2, "Infinite Time (Cheat)"	},
	{0x0a, 0x01, 0x10, 0x10, "Off"						},
	{0x0a, 0x01, 0x10, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x0a, 0x01, 0x20, 0x00, "Off"						},
	{0x0a, 0x01, 0x20, 0x20, "On"						},

	{0   , 0xfe, 0   ,    2, "Choose Race (US EU FR)"	},
	{0x0a, 0x01, 0x40, 0x00, "Off"						},
	{0x0a, 0x01, 0x40, 0x40, "On"						},

	{0   , 0xfe, 0   ,    2, "Vibrations"				},
	{0x0a, 0x01, 0x80, 0x80, "Torque"					},
	{0x0a, 0x01, 0x80, 0x00, "Shake"					},

	{0   , 0xfe, 0   ,    2, "This Unit Is"				},
	{0x0b, 0x01, 0x01, 0x01, "Slave"					},
	{0x0b, 0x01, 0x01, 0x00, "Master"					},

	{0   , 0xfe, 0   ,    8, "Unit ID"					},
	{0x0b, 0x01, 0x0e, 0x00, "1 (McLaren)"				},
	{0x0b, 0x01, 0x0e, 0x02, "2 (Williams)"				},
	{0x0b, 0x01, 0x0e, 0x04, "3 (Benetton)"				},
	{0x0b, 0x01, 0x0e, 0x06, "4 (Ferrari)"				},
	{0x0b, 0x01, 0x0e, 0x08, "5 (Tyrrell)"				},
	{0x0b, 0x01, 0x0e, 0x0a, "6 (Lotus)"				},
	{0x0b, 0x01, 0x0e, 0x0c, "7 (Jordan)"				},
	{0x0b, 0x01, 0x0e, 0x0e, "8 (Ligier)"				},
	
};

STDDIPINFO(F1gpstr2)

static struct BurnDIPInfo WildpltDIPList[]=
{
	{0x10, 0xff, 0xff, 0xff, NULL						},
	{0x11, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    8, "Coinage"					},
	{0x10, 0x01, 0x07, 0x01, "4 Coins 1 Credits"		},
	{0x10, 0x01, 0x07, 0x02, "3 Coins 1 Credits"		},
	{0x10, 0x01, 0x07, 0x03, "2 Coins 1 Credits"		},
	{0x10, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x10, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x10, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x10, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},
	{0x10, 0x01, 0x07, 0x00, "Free Play"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x11, 0x01, 0x10, 0x00, "Off"						},
	{0x11, 0x01, 0x10, 0x10, "On"						},

	{0   , 0xfe, 0   ,    2, "Lives"					},
	{0x11, 0x01, 0x20, 0x20, "2"						},
	{0x11, 0x01, 0x20, 0x00, "4"						},

	{0   , 0xfe, 0   ,    4, "Region"					},
	{0x11, 0x01, 0xc0, 0x40, "Europe?"					},
	{0x11, 0x01, 0xc0, 0x80, "USA"						},
	{0x11, 0x01, 0xc0, 0xc0, "Japan"					},
	{0x11, 0x01, 0xc0, 0x00, "France?"					},
};

STDDIPINFO(Wildplt)

static struct BurnDIPInfo ScudhammaDIPList[]=
{
	{0x0b, 0xff, 0xff, 0x3f, NULL						},
	{0x0c, 0xff, 0xff, 0x07, NULL						},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x0b, 0x01, 0x03, 0x00, "Easy"						},
	{0x0b, 0x01, 0x03, 0x03, "Normal"					},
	{0x0b, 0x01, 0x03, 0x02, "Hard"						},
	{0x0b, 0x01, 0x03, 0x01, "Hardest"					},

	{0   , 0xfe, 0   ,    4, "Time To Hit"				},
	{0x0b, 0x01, 0x0c, 0x0c, "2 s"						},
	{0x0b, 0x01, 0x0c, 0x08, "3 s"						},
	{0x0b, 0x01, 0x0c, 0x04, "4 s"						},
	{0x0b, 0x01, 0x0c, 0x00, "5 s"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x0b, 0x01, 0x10, 0x00, "Off"						},
	{0x0b, 0x01, 0x10, 0x10, "On"						},

	{0   , 0xfe, 0   ,    2, "Lives"					},
	{0x0b, 0x01, 0x20, 0x20, "3"						},
	{0x0b, 0x01, 0x20, 0x00, "5"						},

	{0   , 0xfe, 0   ,    8, "Coinage"					},
	{0x0c, 0x01, 0x07, 0x01, "4 Coins 1 Credits"		},
	{0x0c, 0x01, 0x07, 0x02, "3 Coins 1 Credits"		},
	{0x0c, 0x01, 0x07, 0x03, "2 Coins 1 Credits"		},
	{0x0c, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x0c, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x0c, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x0c, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},
	{0x0c, 0x01, 0x07, 0x00, "Free Play"				},
};

STDDIPINFO(Scudhamma)

static struct BurnDIPInfo ScudhammDIPList[] =
{
	{0x0c, 0xff, 0xff, 0x0f, NULL						},

	{0   , 0xfe, 0   ,    2, "Win Up"					},
	{0x0c, 0x01, 0x08, 0x08, "Off"						},
	{0x0c, 0x01, 0x08, 0x00, "On"						},
};

STDDIPINFOEXT(Scudhamm, Scudhamma, Scudhamm)

static struct BurnDIPInfo Armchmp2DIPList[]=
{
	{0x09, 0xff, 0xff, 0x3f, NULL						},
	{0x0a, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x09, 0x01, 0x03, 0x00, "Easy"						},
	{0x09, 0x01, 0x03, 0x03, "Normal"					},
	{0x09, 0x01, 0x03, 0x02, "Hard"						},
	{0x09, 0x01, 0x03, 0x01, "Hardest"					},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x09, 0x01, 0x0c, 0x00, "1"						},
	{0x09, 0x01, 0x0c, 0x0c, "2"						},
	{0x09, 0x01, 0x0c, 0x08, "3"						},
	{0x09, 0x01, 0x0c, 0x04, "4"						},

	{0   , 0xfe, 0   ,    4, "Game Time"				},
	{0x09, 0x01, 0x30, 0x00, "15 s"						},
	{0x09, 0x01, 0x30, 0x30, "20 s"						},
	{0x09, 0x01, 0x30, 0x20, "25 s"						},
	{0x09, 0x01, 0x30, 0x10, "30 s"						},

	{0   , 0xfe, 0   ,    4, "Region"					},
	{0x09, 0x01, 0xc0, 0xc0, "Japan"					},
	{0x09, 0x01, 0xc0, 0x80, "USA"						},
	{0x09, 0x01, 0xc0, 0x40, "Europe"					},
	{0x09, 0x01, 0xc0, 0x00, "World"					},

	{0   , 0xfe, 0   ,    8, "Coin A"					},
	{0x0a, 0x01, 0x07, 0x01, "4 Coins 1 Credits"		},
	{0x0a, 0x01, 0x07, 0x02, "3 Coins 1 Credits"		},
	{0x0a, 0x01, 0x07, 0x03, "2 Coins 1 Credits"		},
	{0x0a, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x0a, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x0a, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x0a, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},
	{0x0a, 0x01, 0x07, 0x00, "Free Play"				},

	{0   , 0xfe, 0   ,    8, "Coin B"					},
	{0x0a, 0x01, 0x38, 0x08, "4 Coins 1 Credits"		},
	{0x0a, 0x01, 0x38, 0x10, "3 Coins 1 Credits"		},
	{0x0a, 0x01, 0x38, 0x18, "2 Coins 1 Credits"		},
	{0x0a, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x0a, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x0a, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x0a, 0x01, 0x38, 0x20, "1 Coin  4 Credits"		},
	{0x0a, 0x01, 0x38, 0x00, "Free Play"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x0a, 0x01, 0x40, 0x00, "Off"						},
	{0x0a, 0x01, 0x40, 0x40, "On"						},

	{0   , 0xfe, 0   ,    2, "Demo Arm"					},
	{0x0a, 0x01, 0x80, 0x80, "Center"					},
	{0x0a, 0x01, 0x80, 0x00, "Side"						},
};

STDDIPINFO(Armchmp2)

static inline void scroll_regs_write(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x082000:
		case 0x082008:
		case 0x082100:
			scrollx[((address & 8) / 8) + ((address & 0x100) / 0x80)] = data;
		return;

		case 0x082002:
		case 0x08200a:
		case 0x082102:
			scrolly[((address & 8) / 8) + ((address & 0x100) / 0x80)] = data;
		return;

		case 0x082004:
		case 0x08200c:
		case 0x082104:
			scroll_flag[((address & 8) / 8) + ((address & 0x100) / 0x80)] = data;
		return;
	}
}

static inline UINT16 scroll_regs_read(UINT32 address)
{
	switch (address)
	{
		case 0x082000:
		case 0x082008:
		case 0x082100:
			return scrollx[((address & 8) / 8) + ((address & 0x100) / 0x80)];

		case 0x082002:
		case 0x08200a:
		case 0x082102:
			return scrolly[((address & 8) / 8) + ((address & 0x100) / 0x80)];

		case 0x082004:
		case 0x08200c:
		case 0x082104:
			return scroll_flag[((address & 8) / 8) + ((address & 0x100) / 0x80)];
	}

	return 0;
}

static void __fastcall bigrun_main_write_byte(UINT32 address, UINT8 data)
{
	bprintf (0, _T("MWB: %5.5x, %2.2x\n"), address, data);
}

static UINT8 __fastcall bigrun_main_read_byte(UINT32 address)
{
	return SekReadWord(address & ~1) >> ((~address & 1) * 8);
}

static void set_soundbank(INT32 bank0, INT32 bank1)
{
	soundbank0 = bank0;
	soundbank1 = bank1;

	MSM6295SetBank(0, DrvSndROM[0] + (bank0 & 0x0f) * 0x40000, 0, 0x3ffff);
	MSM6295SetBank(1, DrvSndROM[1] + (bank1 & 0x0f) * 0x40000, 0, 0x3ffff);
}

static void __fastcall bigrun_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x080000:	// coin counter = data & 3, leds = data & 0x30
		case 0x080002:	// unknown
		case 0x080006:	// wheel out?
		case 0x08000c:	// ?
		case 0x082108:	// ?
		return;

		case 0x080004:	// seat motor
			motor_value = data & 0xff;
		return;
		
		case 0x08000a:
			soundlatch[0] = data;
		return;

		case 0x080010:
			ip_select = data;
		return;

		case 0x080012:
			ip_select = data + 1;
		return;

		case 0x082208:
		return; // watchdog

		case 0x082308:
			SekSetRESETLine(1, data & 2 /*true = halt/reset, false = run*/); // sub 1
			SekSetRESETLine(2, data & 2 /*true = halt/reset, false = run*/); // sub 2
			SekSetRESETLine(3, data & 1 /*true = halt/reset, false = run*/); // sound
		return;
	}
	
	scroll_regs_write(address, data);
}

static UINT16 bigrun_ip_select_read()
{
	switch (ip_select & 3)
	{
		case 0 : return ProcessAnalog(Analog0, 0, INPUT_DEADZONE, 0x00, 0xff);
		case 3 : return ProcessAnalog(Analog1, 0, INPUT_DEADZONE | INPUT_LINEAR | INPUT_MIGHTBEDIGITAL, 0x00, 0xff);
	}

	return 0xffff;
}

static UINT16 __fastcall bigrun_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x080000:
			return DrvInputs[0]; // in1

		case 0x080002:
			return DrvInputs[1]; // in2

		case 0x080004:
			return DrvDips[0]; // in3

		case 0x080006:
			return (DrvDips[2] * 256) + DrvDips[1]; // in4

		case 0x080008:
			return soundlatch[1];

		case 0x080010:
			return bigrun_ip_select_read();

		case 0x082200:
			return DrvDips[3]; // in5
	}

	return scroll_regs_read(address);
}

static void __fastcall cischeat_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x080012:
		return; // nop
	
		case 0x082300:
			soundlatch[0] = data;
			SekSetIRQLine(3, 4, CPU_IRQSTATUS_AUTO);
		return;
	}
	
	bigrun_main_write_word(address, data);
}

static UINT16 cischeat_ip_select_r()
{
	switch (ip_select & 3)
	{
		case 0 : return ProcessAnalog(Analog0, 0, INPUT_DEADZONE, 0x00, 0xff);
		case 1 : return ~0;                 // Cockpit: Up / Down Position?
		case 2 : return ~0;                 // Cockpit: Left / Right Position?
		default: return ~0;
	}
}

static UINT16 __fastcall cischeat_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x080010:
			return cischeat_ip_select_r();
	
		case 0x082300:
			return soundlatch[1];
	}
	
	return bigrun_main_read_word(address);
}

static void __fastcall f1gpstar_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x080000:
			if ((io_value & 0x04) == 0x04 && (data & 0x04) == 0x00) SekSetIRQLine(4, 4, CPU_IRQSTATUS_AUTO);
			if ((io_value & 0x02) == 0x02 && (data & 0x02) == 0x00) SekSetIRQLine(4, 2, CPU_IRQSTATUS_AUTO);
			io_value = data;
		return;
	
		case 0x080004:	// set motor
			motor_value = data & 0xff;
		return;

		case 0x080008:
		case 0x08000a:
			soundlatch[0] = data;
		return;

		case 0x08000c:
		case 0x080010:
		case 0x080014:
		return; // nop

		case 0x080018:
			SekSetIRQLine(3, 4, CPU_IRQSTATUS_AUTO);
		return;

		case 0x082108:
		return; // nop

		case 0x082208:
		return; // watchdog

		case 0x082308:
			SekSetRESETLine(1, data & 1 /*true = halt/reset, false = run*/); // sub 1
			SekSetRESETLine(2, data & 2 /*true = halt/reset, false = run*/); // sub 2
			SekSetRESETLine(3, data & 4 /*true = halt/reset, false = run*/); // sound
		return;
	}

	scroll_regs_write(address, data);
}

static UINT16 __fastcall f1gpstar_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x080000:
			return (DrvDips[1] * 256) + DrvDips[0];

		case 0x080004:
			return DrvInputs[0]; // in2

		case 0x080006:
			return 0xffff; // in3

		case 0x080008:
			return soundlatch[1];

		case 0x08000c:
			return DrvDips[2];

		case 0x080010:
			return (ProcessAnalog(Analog0, 0, INPUT_DEADZONE, 0x00, 0xff) << 8) | ProcessAnalog(Analog1, 1, INPUT_DEADZONE | INPUT_LINEAR | INPUT_MIGHTBEDIGITAL, 0x00, 0xff);

		case 0x080018:
			return (io_ready & 0x01) ? 0xff : 0xf0;

		case 0x082200:
			return DrvDips[3];
	}

	return scroll_regs_read(address);
}

static void __fastcall wildplt_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x080000:
			if ((io_value & 0x04) == 0x04 && (data & 0x04) == 0x00) SekSetIRQLine(4, 4, CPU_IRQSTATUS_AUTO);
			if ((io_value & 0x02) == 0x02 && (data & 0x02) == 0x00) SekSetIRQLine(4, 2, CPU_IRQSTATUS_AUTO);
			io_value = data;
		return;
	
		case 0x080004:
			mux_data = data & 0x0c;
		return;

		case 0x080008:
			soundlatch[0] = data;
		return;

		case 0x08000c:
			if ((data & 0x2000) && (~sprite_dma_reg & 0x2000)) {
				memcpy (DrvSprRAM, Drv68KRAM[0] + 0x8000, 0x1000);
			}
			sprite_dma_reg = data;
		return;

		case 0x080010:
			ip_select = data;
		return;

		case 0x080014:
		return; // nop

		case 0x080018:
			SekSetIRQLine(3, 4, CPU_IRQSTATUS_AUTO);
		return;

		case 0x082108:
		return; // nop

		case 0x082208:
		return; // watchdog

		case 0x082308:
			SekSetRESETLine(1, data & 1 /*true = halt/reset, false = run*/); // sub 1
			SekSetRESETLine(2, data & 2 /*true = halt/reset, false = run*/); // sub 2
			SekSetRESETLine(3, data & 4 /*true = halt/reset, false = run*/); // sound
		return;
	}

	scroll_regs_write(address, data);
}

static UINT16 __fastcall wildplt_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x080000:
			return (DrvDips[1] * 256) + DrvDips[0];

		case 0x080010: {
			switch (ip_select) {
				case 1:
					return (BurnGunReturnX(1) << 8) | (0xff - BurnGunReturnY(1));
				case 2:
					return (BurnGunReturnX(0) << 8) | (0xff - BurnGunReturnY(0));
			}
			return 0xffff;
		}

		case 0x080008:
			return soundlatch[1];

		case 0x080004:
		{
			UINT16 ret = 0xffff;
			if (mux_data == 0x04) ret = DrvInputs[1];
			if (mux_data == 0x08) ret = DrvInputs[2];
			return ret & DrvInputs[0];
		}

		case 0x080018:
			return (io_ready & 0x01) ? 0xff : 0xf0;

		case 0x082200:
			return DrvDips[3];
	}

	return scroll_regs_read(address);
}

static void __fastcall scudhamm_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x082208:
			// watchdog
		return;

		case 0x100000:
			set_soundbank(data & 3, (data >> 4) & 3);
		return;

		case 0x100008: // leds
		return;

		case 0x100014:
			MSM6295Write(0, data);
		return;

		case 0x100018:
			MSM6295Write(1, data);
		return;

		case 0x10001c: // enable??
		return;

		case 0x100040: // analog input select
		return;

		case 0x100050:
			motor_value = data;
		return;
	}

	scroll_regs_write(address, data);
}

static UINT16 __fastcall scudhamm_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x082208:
			return 0; // watchdog

		case 0x100008:
			return DrvInputs[0] ^ 0x4700;
			
		case 0x100014:
			return MSM6295Read(0);

		case 0x100018:
			return MSM6295Read(1);

		case 0x100040:
			return ProcessAnalog(Analog0, 0, INPUT_DEADZONE | INPUT_LINEAR | INPUT_MIGHTBEDIGITAL, 0x00, 0xff);

		case 0x100044:
		case 0x100050:
			return motor_value;

		case 0x10005c:
			return (DrvDips[1] * 256) + DrvDips[0];
	}

	return scroll_regs_read(address);
}

static void __fastcall armchmp2_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x082208:
			// watchdog
		return;

		case 0x100000:
			set_soundbank(data & 3, (data >> 4) & 3);
		return;

		case 0x100008: // leds
		return;

		case 0x10000c: // analog input select
		return;

		case 0x100010:
			motor_value = data;
		return;

		case 0x100014:
			MSM6295Write(0, data);
		return;

		case 0x100018:
			MSM6295Write(1, data);
		return;
	}

	scroll_regs_write(address, data);
}

static UINT16 armchmp2_analog_r()
{
	INT32 armdelta = /*ioport("IN1")->read()*/0 - ip_select;
	ip_select = 0; //ioport("IN1")->read();

	return ~(motor_value + armdelta );    // + x : x<=0 and player loses, x>0 and player wins
}

static UINT16 armchmp2_buttons_r()
{
	UINT16 ret = DrvInputs[0];
	INT32 arm_x = 0; //ioport("IN1")->read(); // analog

	if (arm_x < 0x40)       ret &= ~1;
	else if (arm_x > 0xc0)  ret &= ~2;
	else if ((arm_x > 0x60) && (arm_x < 0xa0))  ret &= ~4;

	return ret;
}

static UINT16 __fastcall armchmp2_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x082208:
			return 0; // watchdog_r

		case 0x100000:
			return DrvDips[0];

		case 0x100004:
			return DrvDips[1];

		case 0x100008:
			return armchmp2_buttons_r();

		case 0x10000c:
			return armchmp2_analog_r();

		case 0x100010:
			return 0x11; // motor status

		case 0x100014:
			return MSM6295Read(0);

		case 0x100018:
			return MSM6295Read(1);
	}

	return scroll_regs_read(address);
}

static void __fastcall bigrun_sound_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x040000: // bigrun
			set_soundbank(data & 1, (data >> 4) & 1);
		return;

		case 0x040002: // cischeat soundbank 0
			set_soundbank(data & 1, soundbank1);
		return;

		case 0x040004: // cischeat soundbank 1
			set_soundbank(soundbank0, data & 1);
		return;

		case 0x060000:
			soundlatch[1] = data;
		return;

		case 0x080000:
		case 0x080002:
			BurnYM2151Write((address / 2) & 1, data & 0xff);
		return;

		case 0x0a0000:
		case 0x0a0002:
			MSM6295Write(0, data & 0xff);
		return;

		case 0x0c0000:
		case 0x0c0002:
			MSM6295Write(1, data & 0xff);
		return;
	}
}

static void __fastcall bigrun_sound_write_byte(UINT32 address, UINT8 data)
{
	bprintf (0, _T("SWB: %5.5x %2.2x\n"), address, data);
}

static UINT16 __fastcall bigrun_sound_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x040000:
		case 0x060000:
		case 0x060004:
			return soundlatch[0];

		case 0x080000:
		case 0x080002:
			return BurnYM2151Read();

		case 0x0a0000:
		case 0x0a0002:
			return MSM6295Read(0);

		case 0x0c0000:
		case 0x0c0002:
			return MSM6295Read(1);
	}

	return 0;
}

static UINT8 __fastcall bigrun_sound_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x080003:
			return BurnYM2151Read();
	}
	
	bprintf (0, _T("SRB: %5.5x\n"), address);

	return 0;
}

static void __fastcall f1gpstar_sound_write_word(UINT32 address, UINT16 data)
{
	switch (address & ~1)
	{
		case 0x040004: // cischeat soundbank 0
			set_soundbank(data & 1, soundbank1);
		return;

		case 0x040008: // cischeat soundbank 1
			set_soundbank(soundbank0, data & 1);
		return;

		case 0x060000:
		case 0x060002:
			soundlatch[1] = data;
		return;

		case 0x080000:
		case 0x080002:
			BurnYM2151Write((address / 2) & 1, data & 0xff);
		return;

		case 0x0a0000:
		case 0x0a0002:
			MSM6295Write(0, data & 0xff);
		return;

		case 0x0c0000:
		case 0x0c0002:
			MSM6295Write(1, data & 0xff);
		return;
	}
}

static void __fastcall f1gpstar_sound_write_byte(UINT32 address, UINT8 data)
{
	bprintf (0, _T("SWB: %5.5x %2.2x\n"), address, data);
}

static void __fastcall f1gpstar2_io_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x100000:
			io_ready = data;
		return;
	}
}

static void __fastcall f1gpstar2_io_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x100000:
		case 0x100001:
			io_ready = data;
		return;
	}
}

static void DrvYM2151IrqHandler(INT32 state)
{
	if (state) SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	memset (DrvScrRAM[0], 0xff, 0x8000);
	memset (DrvScrRAM[1], 0xff, 0x8000);
	memset (DrvScrRAM[2], 0xff, 0x8000);

	for (INT32 i = 0; i < 5; i++) {
		if (nDrv68KROMLen[i]) SekReset(i);
	}

	SekOpen((nDrv68KROMLen[3]) ? 3 : 0);
	BurnYM2151Reset();
	SekClose();

	set_soundbank(0, 0);
	MSM6295Reset(0);
	MSM6295Reset(1);

	if (is_game < 2) BurnShiftReset();

	soundbank0 = 0;
	soundbank1 = 0;
	motor_value = 0;
	ip_select = 0;
	io_ready = 0;
	io_value = 0;
	mux_data = 0;
	sprite_dma_reg = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM[0]	= Next; Next += 0x180000;
	Drv68KROM[1]	= Next; Next += 0x080000;
	Drv68KROM[2]	= Next; Next += 0x080000;
	Drv68KROM[3]	= Next; Next += 0x040000;
	Drv68KROM[4]	= Next; Next += 0x080000;
 
	DrvGfxROM[0]	= Next; Next += nDrvGfxROMLen[0] * 2;
	DrvGfxROM[1]	= Next; Next += nDrvGfxROMLen[1] * 2;
	DrvGfxROM[2]	= Next; Next += nDrvGfxROMLen[2] * 2;
	DrvGfxROM[3]	= Next; Next += nDrvGfxROMLen[3] * 2;
	DrvGfxROM[4]	= Next; Next += nDrvGfxROMLen[4] * 2;
	DrvGfxROM[5]	= Next; Next += nDrvGfxROMLen[5] * 2;

	MSM6295ROM		= Next;
	DrvSndROM[0]	= Next; Next += 0x100000;
	DrvSndROM[1]	= Next; Next += 0x100000;

	DrvPalette		= (UINT32*)Next; Next += 0x8000 * sizeof(UINT32); // x2 for shadows

	AllRam			= Next;

	Drv68KRAM[0]	= Next; Next += 0x010000;
	Drv68KRAM[1]	= Next; Next += 0x004000;
	Drv68KRAM[2]	= Next; Next += 0x004000;
	Drv68KRAM[3]	= Next; Next += 0x020000;
	Drv68KRAM[4]	= Next; Next += 0x004000;

	DrvShareRAM[0]	= Next; Next += 0x008000;
	DrvShareRAM[1]	= Next; Next += 0x008000;

	DrvScrRAM[0]	= Next; Next += 0x008000;
	DrvScrRAM[1]	= Next; Next += 0x008000;
	DrvScrRAM[2]	= Next; Next += 0x008000;

	DrvRoadRAM[0]	= Next; Next += 0x000800;
	DrvRoadRAM[1]	= Next; Next += 0x000800;

	DrvIORAM		= Next; Next += 0x001000;

	DrvPalRAM		= Next; Next += 0x008000;
	DrvNetworkRAM	= Next; Next += 0x000800;

	scrollx			= (UINT16*)Next; Next += 0x000004 * sizeof(UINT16);
	scrolly			= (UINT16*)Next; Next += 0x000004 * sizeof(UINT16);
	scroll_flag		= (UINT16*)Next; Next += 0x000004 * sizeof(UINT16);
	soundlatch		= (UINT16*)Next; Next += 0x000002 * sizeof(UINT16);

	DrvSprRAM		= Drv68KRAM[0] + 0x08000;
	DrvSprRAMBuf    = Next; Next += 0x008000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void cischeat_untangle_sprites(UINT8 *src, INT32 length)
{
	UINT8 tmp[128];

	for (INT32 j = 0; j < length; j += 128)
	{
		for (INT32 i = 0; i < 16 ; i++)
		{
			memcpy (tmp + i * 8 + 0, src + j + i * 4 + 16 * 0, 4);
			memcpy (tmp + i * 8 + 4, src + j + i * 4 + 16 * 4, 4);
		}

		memcpy (src + j, tmp, 16*8);
	}
}

static INT32 DrvGfxDecode(INT32 nGfx, INT32 nLen, INT32 nType)
{
	INT32 Planes[4]  = { STEP4(0,1) };

	INT32 XOffs0[16]  = { STEP8(0,4), STEP8(512,4) };
	INT32 YOffs0[16]  = { STEP16(0,32) };

	INT32 XOffs1[16] = { STEP16(0,4) };
	INT32 YOffs1[16] = { STEP16(0,64) };

	INT32 XOffs2[64] = { STEP16(0,4),STEP16(64,4),STEP16(128,4),STEP16(128+64,4) };
	INT32 YOffs2[1]  = { 0 };

	UINT8 *tmp = (UINT8*)BurnMalloc(nLen);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM[nGfx], nLen);

	switch (nType)
	{
		case 0: // 8x8
			GfxDecode(((nLen*8)/4)/(8 * 8), 4,  8,  8, Planes, XOffs0, YOffs0, 0x100, tmp, DrvGfxROM[nGfx]);
		break;

		case 1: // 16x16
			GfxDecode(((nLen*8)/4)/(16*16), 4, 16, 16, Planes, XOffs1, YOffs1, 0x400, tmp, DrvGfxROM[nGfx]);
		break;

		case 2: // 64x1
			GfxDecode(((nLen*8)/4)/(64* 1), 4, 64,  1, Planes, XOffs2, YOffs2, 0x100, tmp, DrvGfxROM[nGfx]);
		break;
		
		case 3: // 16x16 (4 8x8 tiles grouped)
			GfxDecode(((nLen*8)/4)/(16*16), 4, 16, 16, Planes, XOffs0, YOffs0, 0x400, tmp, DrvGfxROM[nGfx]);
		break;
	}

	BurnFree (tmp);

	return 0;
}

static INT32 DrvLoadRoms(bool bLoad)
{
	char* pRomName;
	struct BurnRomInfo ri;

	UINT8 *pLoad[5] = { Drv68KROM[0], Drv68KROM[1], Drv68KROM[2], Drv68KROM[3], Drv68KROM[4] };
	UINT8 *gLoad[6] = { DrvGfxROM[0], DrvGfxROM[1], DrvGfxROM[2], DrvGfxROM[3], DrvGfxROM[4], DrvGfxROM[5] };
	UINT8 *sLoad[2] = { DrvSndROM[0], DrvSndROM[1] };

	for (INT32 i = 0; i < 6; i++) gLoad[i] = DrvGfxROM[i];

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++)
	{
		BurnDrvGetRomInfo(&ri, i);

	//	bprintf (0, _T("%d, %x\n"), i, ri.nType & 0xf);

		if ((ri.nType & BRF_PRG) && (ri.nType & 0xf) > 0) {
			if (bLoad) {
				if (BurnLoadRom(pLoad[(ri.nType - 1) & 7] + 1, i+0, 2)) return 1;
				if (BurnLoadRom(pLoad[(ri.nType - 1) & 7] + 0, i+1, 2)) return 1;
			}
			pLoad[(ri.nType - 1) & 7] += ri.nLen * 2;
			i++;
			continue;
		}

		if ((ri.nType & BRF_GRA) && (ri.nType & 0xf) > 0) {
			if ((ri.nType & 0xf) == 4) { // special case for sprites
				if (bLoad) {
					if (BurnLoadRom(gLoad[(ri.nType - 1) & 7] + 0, i + 0, 2)) return 1;
					if (BurnLoadRom(gLoad[(ri.nType - 1) & 7] + 1, i + 1, 2)) return 1;
				}
				gLoad[(ri.nType - 1) & 7] += ri.nLen * 2;
				i++;
			} else {
				if (bLoad) {
					if (BurnLoadRom(gLoad[(ri.nType - 1) & 7], i, 1)) return 1;
				}
				gLoad[(ri.nType - 1) & 7] += ri.nLen;
			}
			continue;
		}
		
		if ((ri.nType & BRF_SND) && ((ri.nType & 0xf) > 0 && (ri.nType & 0xf) < 8)) {
			if (bLoad) {
				if (BurnLoadRom(sLoad[(ri.nType - 1) & 1], i, 1)) return 1;
			}
			sLoad[(ri.nType - 1) & 1] += ri.nLen;
			continue;
		}

		if ((ri.nType & BRF_SND) && ((ri.nType & 0xf) > 8 && (ri.nType & 0xf) < 0xf)) {
			INT32 index = (ri.nType & 3) - 1;
			if (bLoad) {
				UINT8 *temp = BurnMalloc(0x100000);
				if (BurnLoadRom(temp, i, 1)) { BurnFree(temp); return 1; }

				memmove(sLoad[index & 1] + 0x00000, temp + 0x00000, 0x20000);
				memmove(sLoad[index & 1] + 0x40000, temp + 0x20000, 0x20000);
				memmove(sLoad[index & 1] + 0x20000, temp + 0x40000, 0x20000);
				memmove(sLoad[index & 1] + 0x60000, temp + 0x60000, 0x20000);

				BurnFree(temp);
			}
			sLoad[index & 1] += ri.nLen;
			continue;
		}
	}

	if (bLoad) {
		for (INT32 i = 0; i < 2; i++) { // mirror snd roms
			INT32 len = sLoad[i] - DrvSndROM[i];
			for (INT32 j = len; j < 0x100000; j+=len) {
				memcpy (DrvSndROM[i] + j, DrvSndROM[i], len);
			}
		}

		// all games with multiple 68Ks need this
		if (pLoad[1] - Drv68KROM[1]) cischeat_untangle_sprites(DrvGfxROM[3], nDrvGfxROMLen[3]);
		if (nDrvGfxROMLen[0]) DrvGfxDecode(0, nDrvGfxROMLen[0], 0);
		if (nDrvGfxROMLen[1]) DrvGfxDecode(1, nDrvGfxROMLen[1], 0);
		if (nDrvGfxROMLen[2]) DrvGfxDecode(2, nDrvGfxROMLen[2], 0);
		if (nDrvGfxROMLen[3]) DrvGfxDecode(3, nDrvGfxROMLen[3], nDrv68KROMLen[1] ? 1 : 3); // single 68k-based use different format
		if (nDrvGfxROMLen[4]) DrvGfxDecode(4, nDrvGfxROMLen[4], 2);
		if (nDrvGfxROMLen[5]) DrvGfxDecode(5, nDrvGfxROMLen[5], 2);
	}
	else
	{
		for (INT32 i = 0; i < 5; i++) {
			nDrv68KROMLen[i] = pLoad[i] - Drv68KROM[i];
		}
	
		for (INT32 i = 0; i < 6; i++) {
			nDrvGfxROMLen[i] = gLoad[i] - DrvGfxROM[i];
		}
	}

	return 0;
}

static void phantasm_rom_decode(UINT8 *src, INT32 length)
{
#define BITSWAP_0 BITSWAP16(x,0xd,0xe,0xf,0x0,0x1,0x8,0x9,0xa,0xb,0xc,0x5,0x6,0x7,0x2,0x3,0x4)
#define BITSWAP_1 BITSWAP16(x,0xf,0xd,0xb,0x9,0x7,0x5,0x3,0x1,0xe,0xc,0xa,0x8,0x6,0x4,0x2,0x0)
#define BITSWAP_2 BITSWAP16(x,0x0,0x1,0x2,0x3,0x4,0x5,0x6,0x7,0xb,0xa,0x9,0x8,0xf,0xe,0xd,0xc)

	UINT16 *prg = (UINT16*)src;

	for (INT32 i = 0; i < length / 2; i++)
	{
		UINT16 x,y;

		x = BURN_ENDIAN_SWAP_INT16(prg[i]);

		if      (i < 0x08000/2) { if ((i | (0x248/2)) != i) { y = BITSWAP_0; } else { y = BITSWAP_1; } }
		else if (i < 0x10000/2) { y = BITSWAP_2; }
		else if (i < 0x18000/2) { if ((i | (0x248/2)) != i) { y = BITSWAP_0; } else { y = BITSWAP_1; } }
		else if (i < 0x20000/2) { y = BITSWAP_1; }
		else                    { y = BITSWAP_2; }

		prg[i] = BURN_ENDIAN_SWAP_INT16(y);
	}
#undef BITSWAP_0
#undef BITSWAP_1
#undef BITSWAP_2
}

static void astyanax_rom_decode(UINT8 *src, INT32 length)
{
	UINT16 *RAM = (UINT16*)src;

	for (INT32 i = 0 ; i < length/2 ; i++)
	{
		UINT16 x = BURN_ENDIAN_SWAP_INT16(RAM[i]), y;

#define BITSWAP_0   BITSWAP16(x,0xd,0xe,0xf,0x0,0xa,0x9,0x8,0x1,0x6,0x5,0xc,0xb,0x7,0x2,0x3,0x4)
#define BITSWAP_1   BITSWAP16(x,0xf,0xd,0xb,0x9,0x7,0x5,0x3,0x1,0x8,0xa,0xc,0xe,0x0,0x2,0x4,0x6)
#define BITSWAP_2   BITSWAP16(x,0x4,0x5,0x6,0x7,0x0,0x1,0x2,0x3,0xb,0xa,0x9,0x8,0xf,0xe,0xd,0xc)

		if      (i < 0x08000/2) { if ( (i | (0x248/2)) != i ) {y = BITSWAP_0;} else {y = BITSWAP_1;} }
		else if (i < 0x10000/2) { y = BITSWAP_2; }
		else if (i < 0x18000/2) { if ( (i | (0x248/2)) != i ) {y = BITSWAP_0;} else {y = BITSWAP_1;} }
		else if (i < 0x20000/2) { y = BITSWAP_1; }
		else                    { y = BITSWAP_2; }

		RAM[i] = BURN_ENDIAN_SWAP_INT16(y);
	}
}

static void DrvShadowInit()
{
	memset (shadows, 1, 16); // opaque
	shadows[15] = 0; // transparent
	shadows[ 0] = 2; // shadow
}

static INT32 BigrunInit()
{
	DrvLoadRoms(false);

	BurnAllocMemIndex();

	if (DrvLoadRoms(true)) return 1;
	
	phantasm_rom_decode(Drv68KROM[3], 0x40000);

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM[0],				0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(DrvNetworkRAM,				0x084000, 0x0847ff, MAP_RAM);
	SekMapMemory(DrvShareRAM[1],			0x088000, 0x08bfff, MAP_RAM);
	SekMapMemory(DrvShareRAM[0],			0x08c000, 0x08ffff, MAP_RAM);
	SekMapMemory(DrvScrRAM[0],				0x090000, 0x093fff, MAP_RAM);
	SekMapMemory(DrvScrRAM[1],				0x094000, 0x097fff, MAP_RAM);
	SekMapMemory(DrvScrRAM[2],				0x098000, 0x09bfff, MAP_RAM);
	SekMapMemory(DrvPalRAM,					0x09c000, 0x09ffff, MAP_RAM);
	SekMapMemory(Drv68KRAM[0],				0x0f0000, 0x0fffff, MAP_RAM);
	SekMapMemory(Drv68KROM[0] + 0x80000,	0x100000, 0x13ffff, MAP_ROM); // user1
	SekSetWriteWordHandler(0,				bigrun_main_write_word);
	SekSetWriteByteHandler(0,				bigrun_main_write_byte);
	SekSetReadWordHandler(0,				bigrun_main_read_word);
	SekSetReadByteHandler(0,				bigrun_main_read_byte);
	SekClose();

	SekInit(1, 0x68000); // no read/write handlers
	SekOpen(1);
	SekMapMemory(Drv68KROM[1],				0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(DrvShareRAM[0],			0x040000, 0x047fff, MAP_RAM);
	SekMapMemory(DrvRoadRAM[0],				0x080000, 0x0807ff, MAP_RAM);
	SekMapMemory(Drv68KRAM[1],				0x0c0000, 0x0c3fff, MAP_RAM);
	SekMapMemory(Drv68KROM[1] + 0x40000,	0x200000, 0x23ffff, MAP_ROM); // cischeat
	SekClose();

	SekInit(2, 0x68000); // no read/write handlers
	SekOpen(2);
	SekMapMemory(Drv68KROM[2],				0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(DrvShareRAM[1],			0x040000, 0x047fff, MAP_RAM);
	SekMapMemory(DrvRoadRAM[1],				0x080000, 0x0807ff, MAP_RAM);
	SekMapMemory(Drv68KRAM[2],				0x0c0000, 0x0c3fff, MAP_RAM);
	SekMapMemory(Drv68KROM[2] + 0x40000,	0x200000, 0x23ffff, MAP_ROM); // cischeat
	SekClose();

	SekInit(3, 0x68000);
	SekOpen(3);
	SekMapMemory(Drv68KROM[3],				0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM[3],				0x0f0000, 0x0fffff, MAP_RAM);
	SekSetWriteWordHandler(0,				bigrun_sound_write_word);
	SekSetWriteByteHandler(0,				bigrun_sound_write_byte);
	SekSetReadWordHandler(0,				bigrun_sound_read_word);
	SekSetReadByteHandler(0,				bigrun_sound_read_byte);
	SekClose();

	SekInit(4, 0x68000); // not on this hardware

	BurnYM2151InitBuffered(3500000, 1, NULL, 0);
	BurnTimerAttachSek(6000000);
	BurnYM2151SetIrqHandler(&DrvYM2151IrqHandler);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.50, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.50, BURN_SND_ROUTE_RIGHT);

	MSM6295Init(0, 4000000 / MSM6295_PIN7_HIGH, 1);
	MSM6295Init(1, 4000000 / MSM6295_PIN7_HIGH, 1);
	MSM6295SetRoute(0, 0.25, BURN_SND_ROUTE_BOTH);
	MSM6295SetRoute(1, 0.25, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapSetGfx(0, DrvGfxROM[0], 4,    8,  8, nDrvGfxROMLen[0]*2, 0x0e00/2, 0x0f);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 4,    8,  8, nDrvGfxROMLen[1]*2, 0x1600/2, 0x0f);
	GenericTilemapSetGfx(2, DrvGfxROM[2], 4,    8,  8, nDrvGfxROMLen[2]*2, 0x3600/2, 0x0f);
	GenericTilemapSetGfx(3, DrvGfxROM[3], 4,   16, 16, nDrvGfxROMLen[3]*2, 0x2800/2, 0x3f);
	GenericTilemapSetGfx(4, DrvGfxROM[4], 4,   64,  1, nDrvGfxROMLen[4]*2, 0x2000/2, 0x3f);
	GenericTilemapSetGfx(5, DrvGfxROM[5], 4,   64,  1, nDrvGfxROMLen[5]*2, 0x1800/2, 0x3f);

	DrvShadowInit();
	screen_adjust_y = 16;

	is_game = 0;
	BurnShiftInit(SHIFT_POSITION_BOTTOM_RIGHT, SHIFT_COLOR_GREEN, 80);

	DrvDoReset();

	return 0;
}

static INT32 CischeatInit()
{
	DrvLoadRoms(false);

	BurnAllocMemIndex();

	if (DrvLoadRoms(true)) return 1;
	
	astyanax_rom_decode(Drv68KROM[3], 0x40000);

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM[0],				0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(DrvNetworkRAM,				0x088000, 0x0887ff, MAP_RAM);
	SekMapMemory(DrvShareRAM[1],			0x090000, 0x097fff, MAP_RAM);
	SekMapMemory(DrvShareRAM[0],			0x098000, 0x09ffff, MAP_RAM);
	SekMapMemory(DrvScrRAM[0],				0x0a0000, 0x0a7fff, MAP_RAM);
	SekMapMemory(DrvScrRAM[1],				0x0a8000, 0x0affff, MAP_RAM);
	SekMapMemory(DrvScrRAM[2],				0x0b0000, 0x0b7fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,					0x0b8000, 0x0bffff, MAP_RAM);
	SekMapMemory(Drv68KRAM[0],				0x0f0000, 0x0fffff, MAP_RAM);
	SekMapMemory(Drv68KROM[0] + 0x80000,	0x100000, 0x17ffff, MAP_ROM); // user1
//	SekMapMemory(Drv68KROM[1] + 0x40000,	0x180000, 0x1bffff, MAP_ROM);
//	SekMapMemory(Drv68KROM[2] + 0x40000,	0x1c0000, 0x1fffff, MAP_ROM);
	SekSetWriteWordHandler(0,				cischeat_main_write_word);
	SekSetWriteByteHandler(0,				bigrun_main_write_byte);
	SekSetReadWordHandler(0,				cischeat_main_read_word);
	SekSetReadByteHandler(0,				bigrun_main_read_byte);
	SekClose();

	SekInit(1, 0x68000); // no read/write handlers
	SekOpen(1);
	SekMapMemory(Drv68KROM[1],				0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(DrvShareRAM[0],			0x040000, 0x047fff, MAP_RAM);
	SekMapMemory(DrvRoadRAM[0],				0x080000, 0x0807ff, MAP_RAM);
	SekMapMemory(Drv68KRAM[1],				0x0c0000, 0x0c3fff, MAP_RAM);
	SekMapMemory(Drv68KROM[1] + 0x40000,	0x200000, 0x23ffff, MAP_ROM); // cischeat
	SekClose();

	SekInit(2, 0x68000); // no read/write handlers
	SekOpen(2);
	SekMapMemory(Drv68KROM[2],				0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(DrvShareRAM[1],			0x040000, 0x047fff, MAP_RAM);
	SekMapMemory(DrvRoadRAM[1],				0x080000, 0x0807ff, MAP_RAM);
	SekMapMemory(Drv68KRAM[2],				0x0c0000, 0x0c3fff, MAP_RAM);
	SekMapMemory(Drv68KROM[2] + 0x40000,	0x200000, 0x23ffff, MAP_ROM); // cischeat
	SekClose();

	SekInit(3, 0x68000);
	SekOpen(3);
	SekMapMemory(Drv68KROM[3],				0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM[3],				0x0f0000, 0x0fffff, MAP_RAM);
	SekSetWriteWordHandler(0,				bigrun_sound_write_word);
	SekSetWriteByteHandler(0,				bigrun_sound_write_byte);
	SekSetReadWordHandler(0,				bigrun_sound_read_word);
	SekSetReadByteHandler(0,				bigrun_sound_read_byte);
	SekClose();

	SekInit(4, 0x68000); // not on this hardware

	BurnYM2151InitBuffered(3500000, 1, NULL, 0);
	BurnTimerAttachSek(6000000);
	BurnYM2151SetIrqHandler(&DrvYM2151IrqHandler);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.50, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.50, BURN_SND_ROUTE_RIGHT);

	MSM6295Init(0, 4000000 / MSM6295_PIN7_HIGH, 1);
	MSM6295Init(1, 4000000 / MSM6295_PIN7_HIGH, 1);
	MSM6295SetRoute(0, 0.25, BURN_SND_ROUTE_BOTH);
	MSM6295SetRoute(1, 0.25, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapSetGfx(0, DrvGfxROM[0], 4,    8,  8, nDrvGfxROMLen[0]*2, 0x1c00/2, 0x1f);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 4,    8,  8, nDrvGfxROMLen[1]*2, 0x2c00/2, 0x1f);
	GenericTilemapSetGfx(2, DrvGfxROM[2], 4,    8,  8, nDrvGfxROMLen[2]*2, 0x6c00/2, 0x1f);
	GenericTilemapSetGfx(3, DrvGfxROM[3], 4,   16, 16, nDrvGfxROMLen[3]*2, 0x5000/2, 0x7f);
	GenericTilemapSetGfx(4, DrvGfxROM[4], 4,   64,  1, nDrvGfxROMLen[4]*2, 0x3800/2, 0x3f);
	GenericTilemapSetGfx(5, DrvGfxROM[5], 4,   64,  1, nDrvGfxROMLen[5]*2, 0x4800/2, 0x3f);

	DrvShadowInit();
	screen_adjust_y = 16;

	is_game = 0;
	BurnShiftInit(SHIFT_POSITION_BOTTOM_RIGHT, SHIFT_COLOR_GREEN, 80);

	DrvDoReset();

	return 0;
}

static INT32 F1gpstarInit()
{
	DrvLoadRoms(false);

	BurnAllocMemIndex();

	if (DrvLoadRoms(true)) return 1;

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM[0],				0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(DrvIORAM,					0x081000, 0x081fff, MAP_RAM);
	SekMapMemory(DrvNetworkRAM,				0x088000, 0x0887ff, MAP_RAM);
	SekMapMemory(DrvShareRAM[1],			0x090000, 0x097fff, MAP_RAM);
	SekMapMemory(DrvShareRAM[0],			0x098000, 0x09ffff, MAP_RAM);
	SekMapMemory(DrvScrRAM[0],				0x0a0000, 0x0a7fff, MAP_RAM);
	SekMapMemory(DrvScrRAM[1],				0x0a8000, 0x0affff, MAP_RAM);
	SekMapMemory(DrvScrRAM[2],				0x0b0000, 0x0b7fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,					0x0b8000, 0x0bffff, MAP_RAM);
	SekMapMemory(Drv68KRAM[0],				0x0f0000, 0x0fffff, MAP_RAM);
	SekMapMemory(Drv68KROM[0] + 0x80000,	0x100000, 0x17ffff, MAP_ROM); // user1
	SekSetWriteWordHandler(0,				f1gpstar_main_write_word);
	SekSetWriteByteHandler(0,				bigrun_main_write_byte);
	SekSetReadWordHandler(0,				f1gpstar_main_read_word);
	SekSetReadByteHandler(0,				bigrun_main_read_byte);
	SekClose();

	SekInit(1, 0x68000); // no read/write handlers
	SekOpen(1);
	SekMapMemory(Drv68KROM[1],				0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(DrvShareRAM[0],			0x080000, 0x087fff, MAP_RAM);
	SekMapMemory(DrvRoadRAM[0],				0x100000, 0x1007ff, MAP_RAM);
	SekMapMemory(Drv68KRAM[1],				0x180000, 0x183fff, MAP_RAM);
	SekClose();

	SekInit(2, 0x68000); // no read/write handlers
	SekOpen(2);
	SekMapMemory(Drv68KROM[2],				0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(DrvShareRAM[1],			0x080000, 0x087fff, MAP_RAM);
	SekMapMemory(DrvRoadRAM[1],				0x100000, 0x1007ff, MAP_RAM);
	SekMapMemory(Drv68KRAM[2],				0x180000, 0x183fff, MAP_RAM);
	SekClose();

	SekInit(3, 0x68000);
	SekOpen(3);
	SekMapMemory(Drv68KROM[3],				0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM[3],				0x0e0000, 0x0fffff, MAP_RAM);
	SekSetWriteWordHandler(0,				f1gpstar_sound_write_word);
	SekSetWriteByteHandler(0,				f1gpstar_sound_write_byte);
	SekSetReadWordHandler(0,				bigrun_sound_read_word);
	SekSetReadByteHandler(0,				bigrun_sound_read_byte);
	SekClose();

	SekInit(4, 0x68000);
	SekOpen(4);
	SekMapMemory(Drv68KROM[4],				0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(DrvIORAM,					0x080000, 0x080fff, MAP_RAM);
	SekMapMemory(Drv68KRAM[4],				0x180000, 0x183fff, MAP_RAM);
	SekSetWriteWordHandler(0,				f1gpstar2_io_write_word);
	SekSetWriteByteHandler(0,				f1gpstar2_io_write_byte);
	SekClose();

	BurnYM2151InitBuffered(3500000, 1, NULL, 0);
	BurnTimerAttachSek(6000000);
	BurnYM2151SetIrqHandler(&DrvYM2151IrqHandler);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.50, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.50, BURN_SND_ROUTE_RIGHT);

	MSM6295Init(0, 4000000 / MSM6295_PIN7_HIGH, 1);
	MSM6295Init(1, 4000000 / MSM6295_PIN7_HIGH, 1);
	MSM6295SetRoute(0, 0.25, BURN_SND_ROUTE_BOTH);
	MSM6295SetRoute(1, 0.25, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapSetGfx(0, DrvGfxROM[0], 4,    8,  8, nDrvGfxROMLen[0]*2, 0x1e00/2, 0x0f);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 4,    8,  8, nDrvGfxROMLen[1]*2, 0x2e00/2, 0x0f);
	GenericTilemapSetGfx(2, DrvGfxROM[2], 4,    8,  8, nDrvGfxROMLen[2]*2, 0x6e00/2, 0x0f);
	GenericTilemapSetGfx(3, DrvGfxROM[3], 4,   16, 16, nDrvGfxROMLen[3]*2, 0x5000/2, 0x7f);
	GenericTilemapSetGfx(4, DrvGfxROM[4], 4,   64,  1, nDrvGfxROMLen[4]*2, 0x3800/2, 0x3f);
	GenericTilemapSetGfx(5, DrvGfxROM[5], 4,   64,  1, nDrvGfxROMLen[5]*2, 0x4800/2, 0x3f);

	DrvShadowInit();
	screen_adjust_y = 16;
	sek_clock = 12000000;

	is_game = 1;
	BurnShiftInit(SHIFT_POSITION_BOTTOM_RIGHT, SHIFT_COLOR_GREEN, 80);

	DrvDoReset();

	return 0;
}

static INT32 WildpltInit()
{
	DrvLoadRoms(false);

	BurnAllocMemIndex();

	if (DrvLoadRoms(true)) return 1;

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM[0],				0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(DrvIORAM,					0x081000, 0x081fff, MAP_RAM);
	SekMapMemory(DrvShareRAM[1],			0x090000, 0x097fff, MAP_RAM);
	SekMapMemory(DrvShareRAM[0],			0x098000, 0x09ffff, MAP_RAM);
	SekMapMemory(DrvScrRAM[0],				0x0a0000, 0x0a7fff, MAP_RAM);
	SekMapMemory(DrvScrRAM[1],				0x0a8000, 0x0affff, MAP_RAM);
	SekMapMemory(DrvScrRAM[2],				0x0b0000, 0x0b7fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,					0x0b8000, 0x0bffff, MAP_RAM);
	SekMapMemory(Drv68KRAM[0],				0x0f0000, 0x0fffff, MAP_RAM);
	SekMapMemory(Drv68KROM[0] + 0x80000,	0x100000, 0x17ffff, MAP_ROM); // user1
	SekSetWriteWordHandler(0,				wildplt_main_write_word);
	SekSetWriteByteHandler(0,				bigrun_main_write_byte);
	SekSetReadWordHandler(0,				wildplt_main_read_word);
	SekSetReadByteHandler(0,				bigrun_main_read_byte);
	SekClose();

	SekInit(1, 0x68000); // no read/write handlers
	SekOpen(1);
	SekMapMemory(Drv68KROM[1],				0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(DrvShareRAM[0],			0x080000, 0x087fff, MAP_RAM);
	SekMapMemory(DrvRoadRAM[0],				0x100000, 0x1007ff, MAP_RAM);
	SekMapMemory(Drv68KRAM[1],				0x180000, 0x183fff, MAP_RAM);
	SekClose();

	SekInit(2, 0x68000); // no read/write handlers
	SekOpen(2);
	SekMapMemory(Drv68KROM[2],				0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(DrvShareRAM[1],			0x080000, 0x087fff, MAP_RAM);
	SekMapMemory(DrvRoadRAM[1],				0x100000, 0x1007ff, MAP_RAM);
	SekMapMemory(Drv68KRAM[2],				0x180000, 0x183fff, MAP_RAM);
	SekClose();

	SekInit(3, 0x68000);
	SekOpen(3);
	SekMapMemory(Drv68KROM[3],				0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM[3],				0x0e0000, 0x0fffff, MAP_RAM);
	SekSetWriteWordHandler(0,				f1gpstar_sound_write_word);
	SekSetWriteByteHandler(0,				f1gpstar_sound_write_byte);
	SekSetReadWordHandler(0,				bigrun_sound_read_word);
	SekSetReadByteHandler(0,				bigrun_sound_read_byte);
	SekClose();

	SekInit(4, 0x68000);
	SekOpen(4);
	SekMapMemory(Drv68KROM[4],				0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(DrvIORAM,					0x080000, 0x080fff, MAP_RAM);
	SekMapMemory(Drv68KRAM[4],				0x180000, 0x183fff, MAP_RAM);
	SekSetWriteWordHandler(0,				f1gpstar2_io_write_word);
	SekSetWriteByteHandler(0,				f1gpstar2_io_write_byte);
	SekClose();

	BurnYM2151InitBuffered(3500000, 1, NULL, 0);
	BurnTimerAttachSek(6000000);
	BurnYM2151SetIrqHandler(&DrvYM2151IrqHandler);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.50, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.50, BURN_SND_ROUTE_RIGHT);

	MSM6295Init(0, 4000000 / MSM6295_PIN7_HIGH, 1);
	MSM6295Init(1, 4000000 / MSM6295_PIN7_HIGH, 1);
	MSM6295SetRoute(0, 0.25, BURN_SND_ROUTE_BOTH);
	MSM6295SetRoute(1, 0.25, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapSetGfx(0, DrvGfxROM[0], 4,    8,  8, nDrvGfxROMLen[0]*2, 0x1e00/2, 0x0f);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 4,    8,  8, nDrvGfxROMLen[1]*2, 0x2e00/2, 0x0f);
	GenericTilemapSetGfx(2, DrvGfxROM[2], 4,    8,  8, nDrvGfxROMLen[2]*2, 0x6e00/2, 0x0f);
	GenericTilemapSetGfx(3, DrvGfxROM[3], 4,   16, 16, nDrvGfxROMLen[3]*2, 0x5000/2, 0x7f);
	GenericTilemapSetGfx(4, DrvGfxROM[4], 4,   64,  1, nDrvGfxROMLen[4]*2, 0x3800/2, 0x3f);
	GenericTilemapSetGfx(5, DrvGfxROM[5], 4,   64,  1, nDrvGfxROMLen[5]*2, 0x4800/2, 0x3f);

	DrvSprRAM = DrvSprRAMBuf; // sprite buffer!!

	DrvShadowInit();
	screen_adjust_y = 16;
	sek_clock = 12000000;

	BurnGunInit(2, false);

	is_game = 2;

	DrvDoReset();

	return 0;
}

static INT32 ScudhammInit()
{
	DrvLoadRoms(false);

	BurnAllocMemIndex();

	if (DrvLoadRoms(true)) return 1;

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM[0],		0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(DrvScrRAM[0],		0x0a0000, 0x0a3fff, MAP_RAM);
	SekMapMemory(DrvScrRAM[2],		0x0b0000, 0x0b3fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x0b8000, 0x0bffff, MAP_RAM);
	SekMapMemory(Drv68KRAM[0],		0x0f0000, 0x0fffff, MAP_RAM);
	SekSetWriteWordHandler(0,		scudhamm_main_write_word);
	SekSetWriteByteHandler(0,		bigrun_main_write_byte);
	SekSetReadWordHandler(0,		scudhamm_main_read_word);
	SekSetReadByteHandler(0,		bigrun_main_read_byte);
	SekClose();

	BurnYM2151Init(3500000); // not on this hardware!! allows us to use common reset/state functions
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.0, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.0, BURN_SND_ROUTE_RIGHT);

	MSM6295Init(0, 2000000 / MSM6295_PIN7_HIGH, 0);
	MSM6295Init(1, 2000000 / MSM6295_PIN7_HIGH, 0);
	MSM6295SetRoute(0, 0.25, BURN_SND_ROUTE_BOTH);
	MSM6295SetRoute(1, 0.25, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapSetGfx(0, DrvGfxROM[0], 4,  8,  8, nDrvGfxROMLen[0]*2, 0x1e00/2, 0x0f);
	GenericTilemapSetGfx(2, DrvGfxROM[2], 4,  8,  8, nDrvGfxROMLen[2]*2, 0x4e00/2, 0x0f);
	GenericTilemapSetGfx(3, DrvGfxROM[3], 4, 16, 16, nDrvGfxROMLen[3]*2, 0x3000/2, 0x7f);

	DrvShadowInit();
	screen_adjust_y = 16;

	is_game = 3;

	DrvDoReset();

	return 0;
}

static INT32 Armchmp2Init()
{
	DrvLoadRoms(false);

	BurnAllocMemIndex();

	if (DrvLoadRoms(true)) return 1;

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM[0],		0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(DrvScrRAM[0],		0x0a0000, 0x0a3fff, MAP_RAM);
	SekMapMemory(DrvScrRAM[2],		0x0b0000, 0x0b3fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x0b8000, 0x0bffff, MAP_RAM);
	SekMapMemory(Drv68KRAM[0],		0x0f0000, 0x0fffff, MAP_RAM);
	SekSetWriteWordHandler(0,		armchmp2_main_write_word);
	SekSetWriteByteHandler(0,		bigrun_main_write_byte);
	SekSetReadWordHandler(0,		armchmp2_main_read_word);
	SekSetReadByteHandler(0,		bigrun_main_read_byte);
	SekClose();

	BurnYM2151Init(3500000); // not on this hardware!! allows us to use common reset/state functions
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.0, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.0, BURN_SND_ROUTE_RIGHT);

	MSM6295Init(0, 2000000 / MSM6295_PIN7_HIGH, 0);
	MSM6295Init(1, 2000000 / MSM6295_PIN7_HIGH, 0);
	MSM6295SetRoute(0, 0.25, BURN_SND_ROUTE_BOTH);
	MSM6295SetRoute(1, 0.25, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapSetGfx(0, DrvGfxROM[0], 4,  8,  8, nDrvGfxROMLen[0]*2, 0x1e00/2, 0x0f);
	GenericTilemapSetGfx(2, DrvGfxROM[2], 4,  8,  8, nDrvGfxROMLen[2]*2, 0x4e00/2, 0x0f);
	GenericTilemapSetGfx(3, DrvGfxROM[3], 4, 16, 16, nDrvGfxROMLen[3]*2, 0x3000/2, 0x7f);

	DrvShadowInit();

	screen_adjust_y = 16;

	is_game = 4;

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	BurnYM2151Exit();
	MSM6295Exit(0);
	MSM6295Exit(1);
	SekExit();

	if (is_game < 2) BurnShiftExit();
	if (is_game == 2) BurnGunExit();

	BurnFreeMemIndex();

	MSM6295ROM = NULL;
	screen_adjust_y = 0;
	sek_clock = 10000000;

	is_game = 0;

	return 0;
}

static void DrvPaletteUpdate() // not sure if this is correct + needs shadows - RRRRGGGGBBBBRGBx
{
	UINT16 *p = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < BurnDrvGetPaletteEntries(); i++)
	{
		UINT8 r = ((BURN_ENDIAN_SWAP_INT16(p[i]) >> 11) & 0x1e) | ((BURN_ENDIAN_SWAP_INT16(p[i]) >> 3) & 1);
		UINT8 g = ((BURN_ENDIAN_SWAP_INT16(p[i]) >>  7) & 0x1e) | ((BURN_ENDIAN_SWAP_INT16(p[i]) >> 2) & 1);
		UINT8 b = ((BURN_ENDIAN_SWAP_INT16(p[i]) >>  3) & 0x1e) | ((BURN_ENDIAN_SWAP_INT16(p[i]) >> 1) & 1);

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);

		DrvPalette[i] = BurnHighCol(r,g,b,0);

		r = (r * 0x9d) >> 8; // shadow
		g = (g * 0x9d) >> 8;
		b = (b * 0x9d) >> 8;

		DrvPalette[i + 0x4000] = BurnHighCol(r, g, b, 0);
	}
}

static void RenderZoomedSprite1(UINT16 *dest, UINT8 *gfx, INT32 code, INT32 color, INT32 sx, INT32 sy, INT32 fx, INT32 fy, INT32 width, INT32 height, INT32 zoomx, INT32 zoomy, UINT8 *shadow)
{
	// Based on MAME sources for tile zooming
	UINT8 *gfx_base = gfx + (code * width * height);
	int dh = (zoomy * height + 0x8000) / 0x10000;
	int dw = (zoomx * width + 0x8000) / 0x10000;

	if (dw && dh)
	{
		int dx = (width * 0x10000) / dw;
		int dy = (height * 0x10000) / dh;
		int ex = sx + dw;
		int ey = sy + dh;
		int x_index_base = 0;
		int y_index = 0;

		if (fx) {
			x_index_base = (dw - 1) * dx;
			dx = -dx;
		}

		if (fy) {
			y_index = (dh - 1) * dy;
			dy = -dy;
		}

		for (INT32 y = sy; y < ey; y++)
		{
			UINT8 *src = gfx_base + (y_index / 0x10000) * width;
			UINT16 *dst = dest + y * nScreenWidth;

			if (y >= 0 && y < nScreenHeight)
			{
				for (INT32 x = sx, x_index = x_index_base; x < ex; x++)
				{
					if (x >= 0 && x < nScreenWidth) {
						INT32 pxl = src[x_index>>16];

						INT32 drawmode = shadow[pxl];
						if (drawmode == 2) {
							dst[x] |= 0x4000;
						} else if (drawmode == 1) {
							dst[x] = pxl + color;
						}
					}

					x_index += dx;
				}
			}

			y_index += dy;
		}
	}
}

static void draw_layer(INT32 tmap, INT32 flags, INT32 priority)
{
	INT32 layer = scroll_flag[tmap] & 0x03;
	INT32 size  =(scroll_flag[tmap] & 0x10) >> 4; // 1 - 8x8, 0 - 16x16

	INT32 type[2][4][2] = { { { 16, 2 }, { 8, 4 }, { 4, 8 }, { 2, 16 } }, { { 8, 1 }, { 4, 2 }, { 4, 2 }, { 2, 4 } } };
	INT32 trans_mask = (flags & 1) ? 0xff : 0x0f;
	INT32 bits_per_color = (flags & 2) ? 5 : 4;
	GenericTilesGfx *gfx = &GenericGfxData[tmap];
	UINT16 *vidram = (UINT16*)DrvScrRAM[tmap];

	INT32 columns = type[size][layer][0];
	INT32 rows    = type[size][layer][1];
	INT32 width   = columns * 32;
	INT32 height  = rows * 32;

	INT32 xscroll = scrollx[tmap] & ((width * 8)-1);
	INT32 yscroll = (scrolly[tmap] + screen_adjust_y) & ((height * 8)-1);

	for (INT32 row = 0; row < height; row++)
	{
		for (INT32 col = 0; col < width; col++)
		{
			INT32 ofst, code;
			INT32 sx = (col * 8) - xscroll;
			INT32 sy = (row * 8) - yscroll;

			if (sx < -7) sx += width * 8;
			if (sy < -7) sy += height * 8;

			if (sx >= nScreenWidth || sy >= nScreenHeight) continue;

			if (size) {
				ofst = (col * 32) + (row / 32) * 1024 * columns + (row & 0x1f);
				code = (BURN_ENDIAN_SWAP_INT16(vidram[ofst]) & 0x0fff) * scroll_factor_8x8[tmap];
			} else {
				ofst = (((col / 2) * 16) + (row / 32) * 256 * columns + ((row / 2) & 0x0f));
				code = (BURN_ENDIAN_SWAP_INT16(vidram[ofst]) & 0xfff) * 4 + ((row & 1) + (col & 1) * 2);
			}

			code %= gfx->code_mask;

			INT32 color = ((BURN_ENDIAN_SWAP_INT16(vidram[ofst]) >> (16 - bits_per_color)) << gfx->depth) + gfx->color_offset;

			{
				UINT8 *gfxdata = gfx->gfxbase + code * 0x40;
				UINT16 *dest = pTransDraw + sy * nScreenWidth + sx;
				UINT8 *prio = pPrioDraw + sy * nScreenWidth + sx;

				for (INT32 y = 0; y < 8; y++, sy++, sx-=8) {
					if (sy >= nScreenHeight) break;

					for (INT32 x = 0; x < 8; x++, sx++, gfxdata++) {
						if (sx < 0 || sy < 0 || sx >= nScreenWidth) continue;

						INT32 pxl = *gfxdata;
						if (pxl != trans_mask) {
							dest[x] = pxl + color;
							prio[x] = priority;
						}
					}

					dest += nScreenWidth;
					prio += nScreenWidth;
				}
			}
		}
	}
}

static void cischeat_draw_road(INT32 road_num, INT32 priority1, INT32 priority2, INT32 transparency)
{
#define X_SIZE (1024)
#define TILE_SIZE (64)

	INT32 min_priority, max_priority;

	GenericTilesGfx *gfx = &GenericGfxData[4 + (road_num & 1)];
	UINT16 *roadram = (UINT16*)DrvRoadRAM[road_num & 1];

	if (priority1 < priority2)  {   min_priority = priority1;   max_priority = priority2; }
	else                        {   min_priority = priority2;   max_priority = priority1; }
	
	min_priority = (min_priority & 7) * 0x100;
	max_priority = (max_priority & 7) * 0x100;

	for (INT32 sy = screen_adjust_y; sy < nScreenHeight + screen_adjust_y; sy++)
	{
		int code    = BURN_ENDIAN_SWAP_INT16(roadram[ sy * 4 + 0 ]);
		int xscroll = BURN_ENDIAN_SWAP_INT16(roadram[ sy * 4 + 1 ]);
		int attr    = BURN_ENDIAN_SWAP_INT16(roadram[ sy * 4 + 2 ]);

		/* high byte is a priority information */
		if ( ((attr & 0x700) < min_priority) || ((attr & 0x700) > max_priority) )
			continue;

		/* line number converted to tile number (each tile is TILE_SIZE x 1) */
		code = code * (X_SIZE/TILE_SIZE);

		xscroll %= X_SIZE;
		INT32 curr_code = code + xscroll/TILE_SIZE;

		for (INT32 sx = -(xscroll%TILE_SIZE) ; sx < nScreenWidth ; sx +=TILE_SIZE)
		{
			DrawCustomMaskTile(pTransDraw, gfx->width, gfx->height, curr_code % gfx->code_mask, sx, sy - screen_adjust_y, 0, 0, attr & gfx->color_mask, gfx->depth, transparency ? 15 : -1, gfx->color_offset, gfx->gfxbase);

			curr_code++;
			if (curr_code%(X_SIZE/TILE_SIZE)==0)    curr_code = code;
		}
	}
}

static void f1gpstar_draw_road(int road_num, int priority1, int priority2, int transparency)
{
#define X_SIZE (1024)
#define TILE_SIZE (64)

	int sx,sy;
	int xstart;
	int min_priority, max_priority;

	GenericTilesGfx *gfx = &GenericGfxData[(road_num & 1) ? 5 : 4];
	UINT16 *roadram = (UINT16*)DrvRoadRAM[road_num & 1];

	int min_y = 0; //rect.min_y;
	int max_y = nScreenHeight; //rect.max_y;

	int max_x = (nScreenWidth - 1)/*rect.max_x*/ << 16;   // use fixed point values (16.16), for accuracy

	if (priority1 < priority2)  {   min_priority = priority1;   max_priority = priority2; }
	else                        {   min_priority = priority2;   max_priority = priority1; }

	/* Move the priority values in place */
	min_priority = (min_priority & 7) * 0x1000;
	max_priority = (max_priority & 7) * 0x1000;

	/* Let's draw from the top to the bottom of the visible screen */
	for (sy = min_y + screen_adjust_y; sy < max_y + screen_adjust_y; sy ++)
	{
		int xscale, xdim;

		int xscroll = BURN_ENDIAN_SWAP_INT16(roadram[ sy * 4 + 0 ]);
		int xzoom   = BURN_ENDIAN_SWAP_INT16(roadram[ sy * 4 + 1 ]);
		int attr    = BURN_ENDIAN_SWAP_INT16(roadram[ sy * 4 + 2 ]);
		int code    = BURN_ENDIAN_SWAP_INT16(roadram[ sy * 4 + 3 ]);

		/* highest nibble is a priority information */
		if ( ((xscroll & 0x7000) < min_priority) || ((xscroll & 0x7000) > max_priority) )
			continue;

		/* zoom code range: 000-3ff     scale range: 0.0-2.0 */
		xscale = ( ((xzoom & 0x3ff)+1) << (16+1) ) / 0x400;

		/* line number converted to tile number (each tile is TILE_SIZE x 1) */
		code    = code * (X_SIZE/TILE_SIZE);

		/* dimension of a tile after zoom */
		xdim = TILE_SIZE * xscale;

		xscroll %= 2 * X_SIZE;

		xstart  = (X_SIZE - xscroll) * 0x10000;
		xstart -= (X_SIZE * xscale) / 2;

		/* let's approximate to the nearest greater integer value
		   to avoid holes in between tiles */
		xscale += (1<<16)/TILE_SIZE;

		INT32 color = (((attr >> 8) & gfx->color_mask) << gfx->depth) + gfx->color_offset;

		/* Draw the line */
		for (sx = xstart ; sx <= max_x ; sx += xdim)
		{
			RenderZoomedTile(pTransDraw, gfx->gfxbase, code % gfx->code_mask, color, transparency ? 15 : -1, sx / 0x10000, sy - screen_adjust_y, 0, 0, gfx->width, gfx->height, xscale, 1 << 16);

			code++;
			/* stop when the end of the line of gfx is reached */
			if ((code % (X_SIZE/TILE_SIZE)) == 0)   break;
		}
	}
}

#define SHRINK(_org_,_fact_) ( ( ( (_org_) << 16 ) * (_fact_ & 0x01ff) ) / 0x80 )

static void bigrun_draw_sprites(int priority1, int priority2)
{
	int x, sx, flipx, xzoom, xscale, xdim, xnum, xstart, xend, xinc;
	int y, sy, flipy, yzoom, yscale, ydim, ynum, ystart, yend, yinc;
	int code, attr, color, size, shadow;

	int min_priority, max_priority, high_sprites;

	GenericTilesGfx *gfx = &GenericGfxData[3];

	UINT16      *source =   (UINT16*)DrvSprRAM;
	const UINT16    *finish =   source + 0x1000/2;

	high_sprites = (priority1 >= 16) | (priority2 >= 16);
	priority1 = (priority1 & 0x0f) * 0x100;
	priority2 = (priority2 & 0x0f) * 0x100;

	if (priority1 < priority2)  {   min_priority = priority1;   max_priority = priority2; }
	else                        {   min_priority = priority2;   max_priority = priority1; }

	for (; source < finish; source += 0x10/2 )
	{
		size    =   BURN_ENDIAN_SWAP_INT16(source[ 0 ]);
		if (size & 0x1000)  continue;

		xnum    =   ( (size & 0x0f) >> 0 ) + 1;
		ynum    =   ( (size & 0xf0) >> 4 ) + 1;

		yzoom   =   (BURN_ENDIAN_SWAP_INT16(source[ 1 ]) >> 8) & 0xff;
		xzoom   =   (BURN_ENDIAN_SWAP_INT16(source[ 1 ]) >> 0) & 0xff;

		sx      =   BURN_ENDIAN_SWAP_INT16(source[ 2 ]);
		sy      =   BURN_ENDIAN_SWAP_INT16(source[ 3 ]);
		flipx   =   sx & 0x1000;
		flipy   =   sy & 0x1000;
		sx      =   (sx & 0x0ff) - (sx & 0x100);
		sy      =   (sy & 0x0ff) - (sy & 0x100);

		sx <<= 16;
		sy <<= 16;

		xdim    =   SHRINK(16,xzoom);
		ydim    =   SHRINK(16,yzoom);

		if ( ( (xdim / 0x10000) == 0 ) || ( (ydim / 0x10000) == 0) )    continue;

		code    =   BURN_ENDIAN_SWAP_INT16(source[ 6 ]);
		attr    =   BURN_ENDIAN_SWAP_INT16(source[ 7 ]);
		color   =   attr & 0x7f;
		shadow  =   attr & 0x1000;

		if ( ((attr & 0x700) < min_priority) || ((attr & 0x700) > max_priority) )
			continue;

		if ( high_sprites && !(color & 0x80) )
			continue;

		color = ((color & gfx->color_mask) << gfx->depth) + gfx->color_offset;

		xscale = xdim / 16;
		yscale = ydim / 16;

		if (xscale & 0xffff)    xscale += (1<<16)/16;
		if (yscale & 0xffff)    yscale += (1<<16)/16;

		if (flipx)  { xstart = xnum-1;  xend = -1;    xinc = -1; }
		else        { xstart = 0;       xend = xnum;  xinc = +1; }

		if (flipy)  { ystart = ynum-1;  yend = -1;    yinc = -1; }
		else        { ystart = 0;       yend = ynum;  yinc = +1; }

		shadows[0] = shadow ? 2 : 1;

		for (y = ystart; y != yend; y += yinc)
		{
			for (x = xstart; x != xend; x += xinc)
			{
				RenderZoomedSprite1(pTransDraw, gfx->gfxbase, code % gfx->code_mask, color, (sx + x * xdim) / 0x10000, ((sy + y * ydim) / 0x10000) - screen_adjust_y, flipx, flipy, 16, 16, xscale, yscale, shadows);
				code++;
			}
		}
	}
}

static void cischeat_draw_sprites(int priority1, int priority2)
{
	int x, sx, flipx, xzoom, xscale, xdim, xnum, xstart, xend, xinc;
	int y, sy, flipy, yzoom, yscale, ydim, ynum, ystart, yend, yinc;
	int code, attr, color, size, shadow;

	int min_priority, max_priority, high_sprites;

	GenericTilesGfx *gfx = &GenericGfxData[3];

	UINT16      *source =   (UINT16*)DrvSprRAM;
	const UINT16    *finish =   source + 0x1000/2;

	/* Move the priority values in place */
	high_sprites = (priority1 >= 16) | (priority2 >= 16);
	priority1 = (priority1 & 0x0f) * 0x100;
	priority2 = (priority2 & 0x0f) * 0x100;

	if (priority1 < priority2)  {   min_priority = priority1;   max_priority = priority2; }
	else                        {   min_priority = priority2;   max_priority = priority1; }

	for (; source < finish; source += 0x10/2 )
	{
		size    =   BURN_ENDIAN_SWAP_INT16(source[ 0 ]);
		if (size & 0x1000)  continue;

		/* number of tiles */
		xnum    =   ( (size & 0x0f) >> 0 ) + 1;
		ynum    =   ( (size & 0xf0) >> 4 ) + 1;

		xzoom   =   BURN_ENDIAN_SWAP_INT16(source[ 1 ]);
		yzoom   =   BURN_ENDIAN_SWAP_INT16(source[ 2 ]);
		flipx   =   xzoom & 0x1000;
		flipy   =   yzoom & 0x1000;

		sx      =   BURN_ENDIAN_SWAP_INT16(source[ 3 ]);
		sy      =   BURN_ENDIAN_SWAP_INT16(source[ 4 ]);
		// TODO: was & 0x1ff with 0x200 as sprite wrap sign, looks incorrect with Grand Prix Star
		//       during big car on side view in attract mode (a tyre gets stuck on the right of the screen)
		//       this arrangement works with both games (otherwise Part 2 gets misaligned bleachers sprites)
		sx      =   (sx & 0x7ff);
		sy      =   (sy & 0x7ff);
		if(sx & 0x400)
			sx -= 0x800;
		if(sy & 0x400)
			sy -= 0x800;

		/* use fixed point values (16.16), for accuracy */
		sx <<= 16;
		sy <<= 16;

		/* dimension of a tile after zoom */
		{
			xdim    =   SHRINK(16,xzoom);
			ydim    =   SHRINK(16,yzoom);
		}

		if ( ( (xdim / 0x10000) == 0 ) || ( (ydim / 0x10000) == 0) )    continue;

		/* the y pos passed to the hardware is the that of the last line,
		   we need the y pos of the first line  */
		sy -= (ydim * ynum);

		code    =   BURN_ENDIAN_SWAP_INT16(source[ 6 ]);
		attr    =   BURN_ENDIAN_SWAP_INT16(source[ 7 ]);
		color   =   attr & 0x007f;
		shadow  =   attr & 0x1000;

		/* high byte is a priority information */
		if ( ((attr & 0x700) < min_priority) || ((attr & 0x700) > max_priority) )
			continue;

		if ( high_sprites && !(color & 0x80) )
			continue;

		xscale = xdim / 16;
		yscale = ydim / 16;

		/* let's approximate to the nearest greater integer value
		   to avoid holes in between tiles */
		if (xscale & 0xffff)    xscale += (1<<16)/16;
		if (yscale & 0xffff)    yscale += (1<<16)/16;

		if (flipx)  { xstart = xnum-1;  xend = -1;    xinc = -1; }
		else        { xstart = 0;       xend = xnum;  xinc = +1; }

		if (flipy)  { ystart = ynum-1;  yend = -1;    yinc = -1; }
		else        { ystart = 0;       yend = ynum;  yinc = +1; }

		shadows[0] = shadow ? 2 : 1;

		for (y = ystart; y != yend; y += yinc)
		{
			for (x = xstart; x != xend; x += xinc)
			{
				RenderZoomedSprite1(pTransDraw, gfx->gfxbase, code % gfx->code_mask, ((color & gfx->color_mask) << gfx->depth) + gfx->color_offset, (sx + x * xdim) / 0x10000, ((sy + y * ydim) / 0x10000) - screen_adjust_y, flipx, flipy, 16, 16, xscale, yscale, shadows);
				code++;
			}
		}
	}   /* end sprite loop */
}

static INT32 BigrunDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	}

	BurnTransferClear(0x1000);

	for (INT32 i = 7; i >= 4; i--)
	{
		cischeat_draw_road(0,i,i,(i != 7));
		cischeat_draw_road(1,i,i,true);
		bigrun_draw_sprites(i+1,i);
	}

	draw_layer(0, 0, 0);
	draw_layer(1, 0, 0);

	for (INT32 i = 3; i >= 0; i--)
	{
		cischeat_draw_road(0,i,i,true);
		cischeat_draw_road(1,i,i,true);
		bigrun_draw_sprites(i+1,i);
	}

	draw_layer(2, 0, 0);

	BurnTransferCopy(DrvPalette);
	BurnShiftRender();

	return 0;
}

static INT32 CischeatDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	}

	BurnTransferClear(0);

	if (nSpriteEnable & 0x10) cischeat_draw_road(0,7,5,false);
	if (nSpriteEnable & 0x20) cischeat_draw_road(1,7,5,true);

	if (nBurnLayer & 1) draw_layer(0, 2, 0);
	if (nBurnLayer & 2) draw_layer(1, 2, 0);

	if (nSpriteEnable & 1) cischeat_draw_sprites(15,3);

	if (nSpriteEnable & 0x10) cischeat_draw_road(0,4,1,true);
	if (nSpriteEnable & 0x20) cischeat_draw_road(1,4,1,true);
	
	if (nSpriteEnable & 2) cischeat_draw_sprites(2,2);
	
	if (nSpriteEnable & 0x10) cischeat_draw_road(0,0,0,true);
	if (nSpriteEnable & 0x20) cischeat_draw_road(1,0,0,true);
	
	if (nSpriteEnable & 4) cischeat_draw_sprites(1,0);

	if (nBurnLayer & 4) draw_layer(2, 2, 0);

	if (nSpriteEnable & 8) cischeat_draw_sprites(0+16,0+16);

	BurnTransferCopy(DrvPalette);
	BurnShiftRender();

	return 0;
}

static INT32 F1gpstarDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	}

	BurnTransferClear(0);

	if (nSpriteEnable & 0x10) f1gpstar_draw_road(1,6,7,false);
	if (nSpriteEnable & 0x20) f1gpstar_draw_road(0,6,7,true);

	if (nBurnLayer & 1) draw_layer(0, 0, 0);
	if (nBurnLayer & 2) draw_layer(1, 0, 0);

	if (nSpriteEnable & 0x10) f1gpstar_draw_road(1,1,5,true);
	if (nSpriteEnable & 0x20) f1gpstar_draw_road(0,1,5,true);
	
	if (nSpriteEnable & 2) cischeat_draw_sprites(15,2);
	
	if (nSpriteEnable & 0x10) f1gpstar_draw_road(1,0,0,true);
	if (nSpriteEnable & 0x20) f1gpstar_draw_road(0,0,0,true);
	
	if (nSpriteEnable & 4) cischeat_draw_sprites(1,1);

	if (nBurnLayer & 4) draw_layer(2, 0, 0);

	if (nSpriteEnable & 8) cischeat_draw_sprites(0,0);

	BurnTransferCopy(DrvPalette);
	if (is_game < 2) BurnShiftRender();

	return 0;
}

static INT32 ScudhammDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	}

	BurnTransferClear(0);

	draw_layer(0, 0, 0);
	cischeat_draw_sprites(0,15);
	draw_layer(2, 0, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 BigrunFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	SekNewFrame();

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		if (is_game == 0) { // gear shifter: bigrun, cischeat
			BurnShiftInputCheckToggle(DrvJoy2[1]);

			DrvInputs[1] &= ~2;
			DrvInputs[1] |= (!bBurnShiftStatus) << 1;
		}
		if (is_game == 1) { // gear shifter: f1gpstar
			BurnShiftInputCheckToggle(DrvJoy1[5]);

			DrvInputs[0] &= ~0x20;
			DrvInputs[0] |= (!bBurnShiftStatus) << 5;
		}

		if (is_game == 2) { // wildplt
			BurnGunMakeInputs(0, Analog0, Analog1);
			BurnGunMakeInputs(1, Analog2, Analog3);
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[5] = { sek_clock / 60, sek_clock / 60, sek_clock / 60, 6000000 / 60, 10000000 / 60 };
	INT32 nCyclesDone[5] = { 0, 0, 0, 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		SekOpen(0);
		if (i ==   0 && (nCurrentFrame & 1)) SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);
		if (i == 240) SekSetIRQLine((nCurrentFrame & 1) ? 4 : 1, CPU_IRQSTATUS_AUTO);
		CPU_RUN(0, Sek);
		SekClose();

		SekOpen(1);
		if (i == 240) SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
		CPU_RUN(1, Sek);
		SekClose();

		SekOpen(2);
		if (i == 240) SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
		CPU_RUN(2, Sek);
		SekClose();

		SekOpen(3);
		CPU_RUN_TIMER(3);
		SekClose();

		if (nDrv68KROMLen[4])
		{
			SekOpen(4);
			CPU_RUN(4, Sek);
			SekClose();
		}
		if (i == 240) {
			if (pBurnDraw) {
				BurnDrvRedraw();
			}
		}
	}

	if (pBurnSoundOut) {
		SekOpen(3);
		BurnYM2151Render(pBurnSoundOut, nBurnSoundLen);
		SekClose();
		MSM6295Render(pBurnSoundOut, nBurnSoundLen);
	}

	return 0;
}

static INT32 Single68KFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

//	SekNewFrame();

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[1] = { 12000000 / 60 };
	INT32 nCyclesDone[1] = { 0 };

	SekOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		if (i ==  16 && (nCurrentFrame & 1)) SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);
		if (i == 240 && (nCurrentFrame & 1)) SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
		CPU_RUN(0, Sek);
	}

	SekClose();

	if (pBurnSoundOut) {
		MSM6295Render(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
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
		SekScan(nAction);

		BurnYM2151Scan(nAction, pnMin);
		MSM6295Scan(nAction, pnMin);

		BurnShiftScan(nAction);

		if (is_game == 2) {
			BurnGunScan();
		}

		SCAN_VAR(soundbank0);
		SCAN_VAR(soundbank1);
		SCAN_VAR(motor_value);
		SCAN_VAR(ip_select);
		SCAN_VAR(mux_data);
		SCAN_VAR(io_ready);
		SCAN_VAR(io_value);
		SCAN_VAR(sprite_dma_reg);
	}

	if (nAction & ACB_WRITE) {
		set_soundbank(soundbank0, soundbank1);
	}

	return 0;
}


// Big Run (11th Rallye version)

static struct BurnRomInfo bigrunRomDesc[] = {
	{ "br8950b.e1",				0x40000, 0xbfb54a62, 1 | BRF_PRG | BRF_ESS }, //  0 68K #0 Code
	{ "br8950b.e2",				0x40000, 0xc0483e81, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "br8952a.19",				0x20000, 0xfcf6b70c, 2 | BRF_PRG | BRF_ESS }, //  2 68K #1 Code
	{ "br8952a.20",				0x20000, 0xc43d367b, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "br8952a.9",				0x20000, 0xf803f0d9, 3 | BRF_PRG | BRF_ESS }, //  4 68K #2 Code
	{ "br8952a.10",				0x20000, 0x8c0df287, 3 | BRF_PRG | BRF_ESS }, //  5

	{ "br8953c.2",				0x20000, 0xb690d8d9, 4 | BRF_PRG | BRF_ESS }, //  6 68K #3 Code
	{ "br8953c.1",				0x20000, 0x79fc7bc0, 4 | BRF_PRG | BRF_ESS }, //  7

	{ "br8950b.3",				0x20000, 0x864cebf7, 1 | BRF_PRG | BRF_ESS }, //  8 Extra 68K #0 Code
	{ "br8950b.4",				0x20000, 0x702c2a6e, 1 | BRF_PRG | BRF_ESS }, //  9

	{ "br8950b.5",				0x20000, 0x0530764e, 1 | BRF_GRA },           // 10 Background Layer 0 Tiles
	{ "br8950b.6",				0x20000, 0xcf40ecfa, 1 | BRF_GRA },           // 11

	{ "br8950b.7",				0x20000, 0xbd5fd061, 2 | BRF_GRA },           // 12 Background Layer 1 Tiles
	{ "br8950b.8",				0x20000, 0x7d66f418, 2 | BRF_GRA },           // 13

	{ "br8950b.9",				0x20000, 0xbe0864c4, 3 | BRF_GRA },           // 14 Background Layer 2 Tiles

	{ "mr88004a.t41",			0x80000, 0xc781d8c5, 4 | BRF_GRA },           // 15 Sprites
	{ "mr88004d.t43",			0x80000, 0xe4041208, 4 | BRF_GRA },           // 16
	{ "mr88004b.t42",			0x80000, 0x2df2e7b9, 4 | BRF_GRA },           // 17
	{ "mr88004e.t44",			0x80000, 0x7af7fbf6, 4 | BRF_GRA },           // 18
	{ "mb88004c.t48",			0x40000, 0x02e2931d, 4 | BRF_GRA },           // 19
	{ "mb88004f.t49",			0x40000, 0x4f012dab, 4 | BRF_GRA },           // 20

	{ "mr88004g.t45",			0x80000, 0xbb397bae, 5 | BRF_GRA },           // 21 Road Layer 0 Tiles
	{ "mb88004h.t46",			0x80000, 0x6b31a1ba, 5 | BRF_GRA },           // 22

	{ "mr88004g.t45",			0x80000, 0xbb397bae, 6 | BRF_GRA },           // 23 Road Layer 1 Tiles
	{ "mb88004h.t46",			0x80000, 0x6b31a1ba, 6 | BRF_GRA },           // 24

	{ "mb88004l.t50",			0x80000, 0x6b11fb10, 9 | BRF_SND },           // 25 MSM #0 Samples

	{ "mb88004m.t51",			0x80000, 0xee52f04d, 10 | BRF_SND },           // 26 MSM #1 Samples

	{ "br8951b.21",				0x20000, 0x59b9c26b, 0 | BRF_OPT },           // 27 Unknown ROMS
	{ "br8951b.22",				0x20000, 0xc112a803, 0 | BRF_OPT },           // 28
	{ "br8951b.23",				0x10000, 0xb9474fec, 0 | BRF_OPT },           // 29
};

STD_ROM_PICK(bigrun)
STD_ROM_FN(bigrun)

struct BurnDriver BurnDrvBigrun = {
	"bigrun", NULL, NULL, NULL, "1989",
	"Big Run (11th Rallye version)\0", NULL, "Jaleco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, bigrunRomInfo, bigrunRomName, NULL, NULL, NULL, NULL, BigrunInputInfo, BigrunDIPInfo,
	BigrunInit, DrvExit, BigrunFrame, BigrunDraw, DrvScan, &DrvRecalc, 0x2000,
	256, 224, 4, 3
};


// Cisco Heat

static struct BurnRomInfo cischeatRomDesc[] = {
	{ "ch9071v2.03",			0x40000, 0xdd1bb26f, 1 | BRF_PRG | BRF_ESS }, //  0 68K #0 Code
	{ "ch9071v2.01",			0x40000, 0x7b65276a, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ch9073.01",				0x40000, 0xba331526, 2 | BRF_PRG | BRF_ESS }, //  2 68K #1 Code
	{ "ch9073.02",				0x40000, 0xb45ff10f, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "ch9073v1.03",			0x40000, 0xbf1d1cbf, 3 | BRF_PRG | BRF_ESS }, //  4 68K #2 Code
	{ "ch9073v1.04",			0x40000, 0x1ec8a597, 3 | BRF_PRG | BRF_ESS }, //  5

	{ "ch9071.11",				0x20000, 0xbc137bea, 4 | BRF_PRG | BRF_ESS }, //  6 68K #3 Code
	{ "ch9071.10",				0x20000, 0xbf7b634d, 4 | BRF_PRG | BRF_ESS }, //  7

	{ "ch9071.04",				0x40000, 0x7fb48cbc, 1 | BRF_PRG | BRF_ESS }, //  8 Extra 68K #0 Code
	{ "ch9071.02",				0x40000, 0xa5d0f4dc, 1 | BRF_PRG | BRF_ESS }, //  9

	{ "ch9071.a14",				0x40000, 0x7a6d147f, 1 | BRF_GRA },           // 10 Background Layer 0 Tiles

	{ "ch9071.t74",				0x40000, 0x735a2e25, 2 | BRF_GRA },           // 11 Background Layer 1 Tiles

	{ "ch9071.07",				0x10000, 0x3724ccc3, 3 | BRF_GRA },           // 12 Background Layer 2 Tiles

	{ "ch9072.r15",				0x80000, 0x38af4aea, 4 | BRF_GRA },           // 13 Sprites
	{ "ch9072.r16",				0x80000, 0x71388dad, 4 | BRF_GRA },           // 14
	{ "ch9072.r17",				0x80000, 0x9d052cf3, 4 | BRF_GRA },           // 15
	{ "ch9072.r18",				0x80000, 0xfe402a56, 4 | BRF_GRA },           // 16
	{ "ch9072.r25",				0x80000, 0xbe8cca47, 4 | BRF_GRA },           // 17
	{ "ch9072.r26",				0x80000, 0x2f96f47b, 4 | BRF_GRA },           // 18
	{ "ch9072.r19",				0x80000, 0x4e996fa8, 4 | BRF_GRA },           // 19
	{ "ch9072.r20",				0x80000, 0xfa70b92d, 4 | BRF_GRA },           // 20

	{ "ch9073.r21",				0x80000, 0x2943d2f6, 5 | BRF_GRA },           // 21 Road Layer 0 Tiles
	{ "ch9073.r22",				0x80000, 0x2dd44f85, 5 | BRF_GRA },           // 22

	{ "ch9073.r21",				0x80000, 0x2943d2f6, 6 | BRF_GRA },           // 23 Road Layer 1 Tiles
	{ "ch9073.r22",				0x80000, 0x2dd44f85, 6 | BRF_GRA },           // 24

	{ "ch9071.r23",				0x80000, 0xc7dbb992, 1 | BRF_SND },           // 25 MSM #0 Samples

	{ "ch9071.r24",				0x80000, 0xe87ca4d7, 2 | BRF_SND },           // 26 MSM #1 Samples

	{ "ch9072.01",				0x20000, 0xb2efed33, 0 | BRF_OPT },           // 27 Unknown ROMS
	{ "ch9072.02",				0x40000, 0x536edde4, 0 | BRF_OPT },           // 28
	{ "ch9072.03",				0x40000, 0x7e79151a, 0 | BRF_OPT },           // 29
};

STD_ROM_PICK(cischeat)
STD_ROM_FN(cischeat)

struct BurnDriver BurnDrvCischeat = {
	"cischeat", NULL, NULL, NULL, "1990",
	"Cisco Heat\0", "Imperfect graphics", "Jaleco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_RACING, 0,
	NULL, cischeatRomInfo, cischeatRomName, NULL, NULL, NULL, NULL, CischeatInputInfo, CischeatDIPInfo,
	CischeatInit, DrvExit, BigrunFrame, CischeatDraw, DrvScan, &DrvRecalc, 0x4000,
	256, 216, 4, 3
};


// Grand Prix Star (ver 4.0)

static struct BurnRomInfo f1gpstarRomDesc[] = {
	{ "gp9188a27ver4.0.bin",	0x40000, 0x47224c65, 1 | BRF_PRG | BRF_ESS }, //  0 68K #0 Code
	{ "gp9188a22ver4.0.bin",	0x40000, 0x05ca6410, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "9188a-16.v10",			0x20000, 0xef0f7ca9, 2 | BRF_PRG | BRF_ESS }, //  2 68K #1 Code
	{ "9188a-11.v10",			0x20000, 0xde292ea3, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "9188a-6.v10",			0x20000, 0x18ba0340, 3 | BRF_PRG | BRF_ESS }, //  4 68K #2 Code
	{ "9188a-1.v10",			0x20000, 0x109d2913, 3 | BRF_PRG | BRF_ESS }, //  5

	{ "9190a-2.v11",			0x20000, 0xacb2fd80, 4 | BRF_PRG | BRF_ESS }, //  6 68K #3 Code
	{ "9190a-1.v11",			0x20000, 0x7cccadaf, 4 | BRF_PRG | BRF_ESS }, //  7

	{ "9188a-26.v10",			0x40000, 0x0b76673f, 1 | BRF_PRG | BRF_ESS }, //  8 68K #0 Code
	{ "9188a-21.v10",			0x40000, 0x3e098d77, 1 | BRF_PRG | BRF_ESS }, //  9

	{ "90015-31.r56",			0x80000, 0x0c8f0e2b, 1 | BRF_GRA },           // 10 Background Layer 0 Tiles
	
	{ "90015-32.r57",			0x80000, 0x9c921cfb, 2 | BRF_GRA },           // 11 Background Layer 1 Tiles

	{ "9188a-30.v10",			0x20000, 0x0ef1fbf1, 3 | BRF_GRA },           // 12 Background Layer 2 Tiles

	{ "90015-21.r46",			0x80000, 0x6f30211f, 4 | BRF_GRA },           // 13 Sprites
	{ "90015-22.r47",			0x80000, 0x05a9a5da, 4 | BRF_GRA },           // 14
	{ "90015-23.r48",			0x80000, 0x58e9c6d2, 4 | BRF_GRA },           // 15
	{ "90015-24.r49",			0x80000, 0xabd6c91d, 4 | BRF_GRA },           // 16
	{ "90015-25.r50",			0x80000, 0x7ded911f, 4 | BRF_GRA },           // 17
	{ "90015-26.r51",			0x80000, 0x18a6c663, 4 | BRF_GRA },           // 18
	{ "90015-27.r52",			0x80000, 0x7378c82f, 4 | BRF_GRA },           // 19
	{ "90015-28.r53",			0x80000, 0x9944dacd, 4 | BRF_GRA },           // 20
	{ "90015-29.r54",			0x80000, 0x2cdec370, 4 | BRF_GRA },           // 21
	{ "90015-30.r55",			0x80000, 0x47e37604, 4 | BRF_GRA },           // 22

	{ "90015-05.w10",			0x80000, 0x8eb48a23, 5 | BRF_GRA },           // 23 Road Layer 0 Tiles
	{ "90015-06.w11",			0x80000, 0x32063a68, 5 | BRF_GRA },           // 24
	{ "90015-07.w12",			0x80000, 0x0d0d54f3, 5 | BRF_GRA },           // 25
	{ "90015-08.w14",			0x80000, 0xf48a42c5, 5 | BRF_GRA },           // 26

	{ "90015-09.w13",			0x80000, 0x55f49315, 6 | BRF_GRA },           // 27 Road Layer 1 Tiles
	{ "90015-10.w15",			0x80000, 0x678be0cb, 6 | BRF_GRA },           // 28

	{ "90015-34.w32",			0x80000, 0x2ca9b062, 1 | BRF_SND },           // 29 MSM #0 Samples

	{ "90015-33.w31",			0x80000, 0x6121d247, 2 | BRF_SND },           // 30 MSM #1 Samples

	{ "90015-04.w09",			0x80000, 0x5b324c81, 0 | BRF_OPT },           // 31 Unused ROMs
	{ "90015-03.w08",			0x80000, 0xccf5b158, 0 | BRF_OPT },           // 32
	{ "90015-02.w07",			0x80000, 0xfcbecc9b, 0 | BRF_OPT },           // 33
	{ "90015-01.w06",			0x80000, 0xce4bfe6e, 0 | BRF_OPT },           // 34
	{ "90015-20.r45",			0x80000, 0x9d428fb7, 0 | BRF_OPT },           // 35
	{ "ch9072-4",				0x01000, 0x5bc23535, 0 | BRF_OPT },           // 36
	{ "ch9072-5",				0x01000, 0x0efac5b4, 0 | BRF_OPT },           // 37
	{ "ch9072-6",				0x01000, 0x76ff63c5, 0 | BRF_OPT },           // 38
	{ "ch9072-8",				0x01000, 0xca04bace, 0 | BRF_OPT },           // 39
	{ "pr88004q",				0x00200, 0x9327dc37, 0 | BRF_OPT },           // 40
	{ "pr88004w",				0x00100, 0x3d648467, 0 | BRF_OPT },           // 41
	{ "pr90015a",				0x00800, 0x777583db, 0 | BRF_OPT },           // 42
	{ "pr90015b",				0x00100, 0xbe240dac, 0 | BRF_OPT },           // 43
};

STD_ROM_PICK(f1gpstar)
STD_ROM_FN(f1gpstar)

struct BurnDriver BurnDrvF1gpstar = {
	"f1gpstar", NULL, NULL, NULL, "1992",
	"Grand Prix Star (ver 4.0)\0", NULL, "Jaleco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_RACING, 0,
	NULL, f1gpstarRomInfo, f1gpstarRomName, NULL, NULL, NULL, NULL, F1gpstarInputInfo, F1gpstarDIPInfo,
	F1gpstarInit, DrvExit, BigrunFrame, F1gpstarDraw, DrvScan, &DrvRecalc, 0x4000,
	256, 224, 4, 3
};


// Grand Prix Star (ver 3.0)

static struct BurnRomInfo f1gpstar3RomDesc[] = {
	{ "gp9188a27ver3.0.ic125",	0x40000, 0xe3ab0085, 1 | BRF_PRG | BRF_ESS }, //  0 68K #0 Code
	{ "gp9188a22ver3.0.ic92",	0x40000, 0x7022061b, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "9188a-16.v10",			0x20000, 0xef0f7ca9, 2 | BRF_PRG | BRF_ESS }, //  2 68K #1 Code
	{ "9188a-11.v10",			0x20000, 0xde292ea3, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "9188a-6.v10",			0x20000, 0x18ba0340, 3 | BRF_PRG | BRF_ESS }, //  4 68K #2 Code
	{ "9188a-1.v10",			0x20000, 0x109d2913, 3 | BRF_PRG | BRF_ESS }, //  5

	{ "9190a-2.v11",			0x20000, 0xacb2fd80, 4 | BRF_PRG | BRF_ESS }, //  6 s68K #3 Code
	{ "9190a-1.v11",			0x20000, 0x7cccadaf, 4 | BRF_PRG | BRF_ESS }, //  7

	{ "9188a-26.v10",			0x40000, 0x0b76673f, 1 | BRF_PRG | BRF_ESS }, //  8 68K #0 Code
	{ "9188a-21.v10",			0x40000, 0x3e098d77, 1 | BRF_PRG | BRF_ESS }, //  9

	{ "90015-31.r56",			0x80000, 0x0c8f0e2b, 1 | BRF_GRA },           // 10 Background Layer 0 Tiles

	{ "90015-32.r57",			0x80000, 0x9c921cfb, 2 | BRF_GRA },           // 11 Background Layer 1 Tiles

	{ "9188a-30.v10",			0x20000, 0x0ef1fbf1, 3 | BRF_GRA },           // 12 Background Layer 2 Tiles

	{ "90015-21.r46",			0x80000, 0x6f30211f, 4 | BRF_GRA },           // 13 Sprites
	{ "90015-22.r47",			0x80000, 0x05a9a5da, 4 | BRF_GRA },           // 14
	{ "90015-23.r48",			0x80000, 0x58e9c6d2, 4 | BRF_GRA },           // 15
	{ "90015-24.r49",			0x80000, 0xabd6c91d, 4 | BRF_GRA },           // 16
	{ "90015-25.r50",			0x80000, 0x7ded911f, 4 | BRF_GRA },           // 17
	{ "90015-26.r51",			0x80000, 0x18a6c663, 4 | BRF_GRA },           // 18
	{ "90015-27.r52",			0x80000, 0x7378c82f, 4 | BRF_GRA },           // 19
	{ "90015-28.r53",			0x80000, 0x9944dacd, 4 | BRF_GRA },           // 20
	{ "90015-29.r54",			0x80000, 0x2cdec370, 4 | BRF_GRA },           // 21
	{ "90015-30.r55",			0x80000, 0x47e37604, 4 | BRF_GRA },           // 22

	{ "90015-05.w10",			0x80000, 0x8eb48a23, 5 | BRF_GRA },           // 23 Road Layer 0 Tiles
	{ "90015-06.w11",			0x80000, 0x32063a68, 5 | BRF_GRA },           // 24
	{ "90015-07.w12",			0x80000, 0x0d0d54f3, 5 | BRF_GRA },           // 25
	{ "90015-08.w14",			0x80000, 0xf48a42c5, 5 | BRF_GRA },           // 26

	{ "90015-09.w13",			0x80000, 0x55f49315, 6 | BRF_GRA },           // 27 Road Layer 1 Tiles
	{ "90015-10.w15",			0x80000, 0x678be0cb, 6 | BRF_GRA },           // 28

	{ "90015-34.w32",			0x80000, 0x2ca9b062, 1 | BRF_SND },           // 29 MSM #0 Samples

	{ "90015-33.w31",			0x80000, 0x6121d247, 2 | BRF_SND },           // 30 MSM #1 Samples

	{ "90015-04.w09",			0x80000, 0x5b324c81, 0 | BRF_OPT },           // 31 Unused ROMs
	{ "90015-03.w08",			0x80000, 0xccf5b158, 0 | BRF_OPT },           // 32
	{ "90015-02.w07",			0x80000, 0xfcbecc9b, 0 | BRF_OPT },           // 33
	{ "90015-01.w06",			0x80000, 0xce4bfe6e, 0 | BRF_OPT },           // 34
	{ "90015-20.r45",			0x80000, 0x9d428fb7, 0 | BRF_OPT },           // 35
	{ "ch9072-4",				0x01000, 0x5bc23535, 0 | BRF_OPT },           // 36
	{ "ch9072-5",				0x01000, 0x0efac5b4, 0 | BRF_OPT },           // 37
	{ "ch9072-6",				0x01000, 0x76ff63c5, 0 | BRF_OPT },           // 38
	{ "ch9072-8",				0x01000, 0xca04bace, 0 | BRF_OPT },           // 39
	{ "pr88004q",				0x00200, 0x9327dc37, 0 | BRF_OPT },           // 40
	{ "pr88004w",				0x00100, 0x3d648467, 0 | BRF_OPT },           // 41
	{ "pr90015a",				0x00800, 0x777583db, 0 | BRF_OPT },           // 42
	{ "pr90015b",				0x00100, 0xbe240dac, 0 | BRF_OPT },           // 43
};

STD_ROM_PICK(f1gpstar3)
STD_ROM_FN(f1gpstar3)

struct BurnDriver BurnDrvF1gpstar3 = {
	"f1gpstar3", "f1gpstar", NULL, NULL, "1991",
	"Grand Prix Star (ver 3.0)\0", NULL, "Jaleco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_RACING, 0,
	NULL, f1gpstar3RomInfo, f1gpstar3RomName, NULL, NULL, NULL, NULL, F1gpstarInputInfo, F1gpstarDIPInfo,
	F1gpstarInit, DrvExit, BigrunFrame, F1gpstarDraw, DrvScan, &DrvRecalc, 0x4000,
	256, 224, 4, 3
};


// Grand Prix Star (ver 2.0)

static struct BurnRomInfo f1gpstar2RomDesc[] = {
	{ "9188a-27.v20",			0x40000, 0x0a9d3896, 1 | BRF_PRG | BRF_ESS }, //  0 68K #0 Code
	{ "9188a-22.v20",			0x40000, 0xde15c9ca, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "9188a-16.v10",			0x20000, 0xef0f7ca9, 2 | BRF_PRG | BRF_ESS }, //  2 68K #1 Code
	{ "9188a-11.v10",			0x20000, 0xde292ea3, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "9188a-6.v10",			0x20000, 0x18ba0340, 3 | BRF_PRG | BRF_ESS }, //  4 68K #2 Code
	{ "9188a-1.v10",			0x20000, 0x109d2913, 3 | BRF_PRG | BRF_ESS }, //  5

	{ "9190a-2.v11",			0x20000, 0xacb2fd80, 4 | BRF_PRG | BRF_ESS }, //  6 68K #3 Code
	{ "9190a-1.v11",			0x20000, 0x7cccadaf, 4 | BRF_PRG | BRF_ESS }, //  7

	{ "9188a-26.v10",			0x40000, 0x0b76673f, 1 | BRF_PRG | BRF_ESS }, //  8 68K #0 Code
	{ "9188a-21.v10",			0x40000, 0x3e098d77, 1 | BRF_PRG | BRF_ESS }, //  9

	{ "90015-31.r56",			0x80000, 0x0c8f0e2b, 1 | BRF_GRA },           // 10 Background Layer 0 Tiles

	{ "90015-32.r57",			0x80000, 0x9c921cfb, 2 | BRF_GRA },           // 11 Background Layer 1 Tiles

	{ "9188a-30.v10",			0x20000, 0x0ef1fbf1, 3 | BRF_GRA },           // 12 Background Layer 2 Tiles

	{ "90015-21.r46",			0x80000, 0x6f30211f, 4 | BRF_GRA },           // 13 Sprites
	{ "90015-22.r47",			0x80000, 0x05a9a5da, 4 | BRF_GRA },           // 14
	{ "90015-23.r48",			0x80000, 0x58e9c6d2, 4 | BRF_GRA },           // 15
	{ "90015-24.r49",			0x80000, 0xabd6c91d, 4 | BRF_GRA },           // 16
	{ "90015-25.r50",			0x80000, 0x7ded911f, 4 | BRF_GRA },           // 17
	{ "90015-26.r51",			0x80000, 0x18a6c663, 4 | BRF_GRA },           // 18
	{ "90015-27.r52",			0x80000, 0x7378c82f, 4 | BRF_GRA },           // 19
	{ "90015-28.r53",			0x80000, 0x9944dacd, 4 | BRF_GRA },           // 20
	{ "90015-29.r54",			0x80000, 0x2cdec370, 4 | BRF_GRA },           // 21
	{ "90015-30.r55",			0x80000, 0x47e37604, 4 | BRF_GRA },           // 22

	{ "90015-05.w10",			0x80000, 0x8eb48a23, 5 | BRF_GRA },           // 23 Road Layer 0 Tiles
	{ "90015-06.w11",			0x80000, 0x32063a68, 5 | BRF_GRA },           // 24
	{ "90015-07.w12",			0x80000, 0x0d0d54f3, 5 | BRF_GRA },           // 25
	{ "90015-08.w14",			0x80000, 0xf48a42c5, 5 | BRF_GRA },           // 26

	{ "90015-09.w13",			0x80000, 0x55f49315, 6 | BRF_GRA },           // 27 Road Layer 1 Tiles
	{ "90015-10.w15",			0x80000, 0x678be0cb, 6 | BRF_GRA },           // 28

	{ "90015-34.w32",			0x80000, 0x2ca9b062, 1 | BRF_SND },           // 29 MSM #0 Samples

	{ "90015-33.w31",			0x80000, 0x6121d247, 2 | BRF_SND },           // 30 MSM #1 Samples

	{ "90015-04.w09",			0x80000, 0x5b324c81, 0 | BRF_OPT },           // 31 Unused ROMs
	{ "90015-03.w08",			0x80000, 0xccf5b158, 0 | BRF_OPT },           // 32
	{ "90015-02.w07",			0x80000, 0xfcbecc9b, 0 | BRF_OPT },           // 33
	{ "90015-01.w06",			0x80000, 0xce4bfe6e, 0 | BRF_OPT },           // 34
	{ "90015-20.r45",			0x80000, 0x9d428fb7, 0 | BRF_OPT },           // 35
	{ "ch9072-4",				0x01000, 0x5bc23535, 0 | BRF_OPT },           // 36
	{ "ch9072-5",				0x01000, 0x0efac5b4, 0 | BRF_OPT },           // 37
	{ "ch9072-6",				0x01000, 0x76ff63c5, 0 | BRF_OPT },           // 38
	{ "ch9072-8",				0x01000, 0xca04bace, 0 | BRF_OPT },           // 39
	{ "pr88004q",				0x00200, 0x9327dc37, 0 | BRF_OPT },           // 40
	{ "pr88004w",				0x00100, 0x3d648467, 0 | BRF_OPT },           // 41
	{ "pr90015a",				0x00800, 0x777583db, 0 | BRF_OPT },           // 42
	{ "pr90015b",				0x00100, 0xbe240dac, 0 | BRF_OPT },           // 43
};

STD_ROM_PICK(f1gpstar2)
STD_ROM_FN(f1gpstar2)

struct BurnDriver BurnDrvF1gpstar2 = {
	"f1gpstar2", "f1gpstar", NULL, NULL, "1991",
	"Grand Prix Star (ver 2.0)\0", NULL, "Jaleco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_RACING, 0,
	NULL, f1gpstar2RomInfo, f1gpstar2RomName, NULL, NULL, NULL, NULL, F1gpstarInputInfo, F1gpstarDIPInfo,
	F1gpstarInit, DrvExit, BigrunFrame, F1gpstarDraw, DrvScan, &DrvRecalc, 0x4000,
	256, 224, 4, 3
};


// F-1 Grand Prix Star II

static struct BurnRomInfo f1gpstr2RomDesc[] = {
	{ "9188a-27.125",			0x040000, 0xee60b894, 1 | BRF_PRG | BRF_ESS }, //  0 68K #0 Code
	{ "9188a-22.92",			0x040000, 0xf229332b, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "9188a-16.70",			0x020000, 0x3b3d57a0, 2 | BRF_PRG | BRF_ESS }, //  2 68K #1 Code
	{ "9188a-11.46",			0x020000, 0x4d2afddf, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "9188a-6.27",				0x020000, 0x68faa23b, 3 | BRF_PRG | BRF_ESS }, //  4 68K #2 Code
	{ "9188a-1.2",				0x020000, 0xe4d83b4f, 3 | BRF_PRG | BRF_ESS }, //  5

	{ "92116-3.37",				0x020000, 0x2a541095, 4 | BRF_PRG | BRF_ESS }, //  6 68K #3 Code
	{ "92116-4.38",				0x020000, 0x70da1825, 4 | BRF_PRG | BRF_ESS }, //  7

	{ "92116-5.116",			0x040000, 0xda16db49, 5 | BRF_PRG | BRF_ESS }, //  8 68K #4 Code
	{ "92116-6.117",			0x040000, 0x1f1e147a, 5 | BRF_PRG | BRF_ESS }, //  9

	{ "9188a-26.124",			0x040000, 0x8690bb79, 1 | BRF_PRG | BRF_ESS }, // 10 68K #0 Code
	{ "9188a-21.91",			0x040000, 0xc5a5807e, 1 | BRF_PRG | BRF_ESS }, // 11

	{ "9188a-25.123",			0x080000, 0x000c83d2, 1 | BRF_GRA },           // 12 Background Layer 0 Tiles
	
	{ "9188a-28.152",			0x080000, 0xe734f8ea, 2 | BRF_GRA },           // 13 Background Layer 1 Tiles

	{ "9188a-30.174",			0x020000, 0xc5906023, 3 | BRF_GRA },           // 14 Background Layer 2 Tiles

	{ "92021-11.1",				0x100000, 0xfec883a7, 4 | BRF_GRA },           // 15 Sprites
	{ "92021-12.2",				0x100000, 0xdf283a7e, 4 | BRF_GRA },           // 16
	{ "92021-13.11",			0x100000, 0x1ceb593a, 4 | BRF_GRA },           // 17
	{ "92021-14.12",			0x100000, 0x673c2c61, 4 | BRF_GRA },           // 18
	{ "92021-15.21",			0x100000, 0x66e8b197, 4 | BRF_GRA },           // 19
	{ "92021-16.22",			0x100000, 0x1f672dd8, 4 | BRF_GRA },           // 20

	{ "92021-08.64",			0x080000, 0x54ff00c4, 5 | BRF_GRA },           // 21 Road Layer 0 Tiles
	{ "92021-07.63",			0x080000, 0x258d524a, 5 | BRF_GRA },           // 22
	{ "92021-06.62",			0x080000, 0xf1423efe, 5 | BRF_GRA },           // 23
	{ "92021-05.61",			0x080000, 0x88bb6db1, 5 | BRF_GRA },           // 24

	{ "92021-04.17",			0x080000, 0x3a2e6b1e, 6 | BRF_GRA },           // 25 Road Layer 1 Tiles
	{ "92021-03.16",			0x080000, 0x1f041f65, 6 | BRF_GRA },           // 26
	{ "92021-02.15",			0x080000, 0xd0582ad8, 6 | BRF_GRA },           // 27
	{ "92021-01.14",			0x080000, 0x06e50be4, 6 | BRF_GRA },           // 28

	{ "92116-2.18",				0x080000, 0x8c06cded, 1 | BRF_SND },           // 29 MSM #0 Samples

	{ "92116-1.4",				0x080000, 0x8da37b9d, 2 | BRF_SND },           // 30 MSM #1 Samples

	{ "90015-04.17",			0x080000, 0x5b324c81, 0 | BRF_OPT },           // 31 Unused ROMs
	{ "90015-04.82",			0x080000, 0x5b324c81, 0 | BRF_OPT },           // 32
	{ "90015-03.16",			0x080000, 0xccf5b158, 0 | BRF_OPT },           // 33
	{ "90015-03.81",			0x080000, 0xccf5b158, 0 | BRF_OPT },           // 34
	{ "90015-02.15",			0x080000, 0xfcbecc9b, 0 | BRF_OPT },           // 35
	{ "90015-02.80",			0x080000, 0xfcbecc9b, 0 | BRF_OPT },           // 36
	{ "90015-01.14",			0x080000, 0xce4bfe6e, 0 | BRF_OPT },           // 37
	{ "90015-01.79",			0x080000, 0xce4bfe6e, 0 | BRF_OPT },           // 38
	{ "90015-35.54",			0x080000, 0x9d428fb7, 0 | BRF_OPT },           // 39
	{ "90015-35.67",			0x080000, 0x9d428fb7, 0 | BRF_OPT },           // 40
	{ "ch9072_4.39",			0x002000, 0xb45b4dc0, 0 | BRF_OPT },           // 41
	{ "ch9072_5.33",			0x002000, 0xe122916b, 0 | BRF_OPT },           // 42
	{ "ch9072_6.35",			0x002000, 0x05d95bf7, 0 | BRF_OPT },           // 43
	{ "ch9072_8.59",			0x002000, 0x6bf52596, 0 | BRF_OPT },           // 44
	{ "pr88004q.105",			0x000200, 0x9327dc37, 0 | BRF_OPT },           // 45
	{ "pr88004w.66",			0x000100, 0x3d648467, 0 | BRF_OPT },           // 46
	{ "pr90015a.117",			0x000800, 0x777583db, 0 | BRF_OPT },           // 47
	{ "pr90015b.153",			0x000100, 0xbe240dac, 0 | BRF_OPT },           // 48
};

STD_ROM_PICK(f1gpstr2)
STD_ROM_FN(f1gpstr2)

struct BurnDriver BurnDrvF1gpstr2 = {
	"f1gpstr2", NULL, NULL, NULL, "1993",
	"F-1 Grand Prix Star II\0", NULL, "Jaleco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_RACING, 0,
	NULL, f1gpstr2RomInfo, f1gpstr2RomName, NULL, NULL, NULL, NULL, F1gpstarInputInfo, F1gpstr2DIPInfo,
	F1gpstarInit, DrvExit, BigrunFrame, F1gpstarDraw, DrvScan, &DrvRecalc, 0x4000,
	256, 224, 4, 3
};


// Wild Pilot

static struct BurnRomInfo wildpltRomDesc[] = {
	{ "gp-9188a_27_ver1-4.bin",	0x40000, 0x4754f023, 1 | BRF_PRG | BRF_ESS }, //  0 68K #0 Code
	{ "gp-9188a_22_ver1-4.bin",	0x40000, 0x9e099111, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "gp-9188a_16_ver1-0.bin",	0x20000, 0x75e48920, 2 | BRF_PRG | BRF_ESS }, //  2 68K #1 Code
	{ "gp-9188a_11_ver1-0.bin",	0x20000, 0x8174e65b, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "gp-9188a_06_ver1-0.bin",	0x20000, 0x99139696, 3 | BRF_PRG | BRF_ESS }, //  4 68K #2 Code
	{ "gp-9188a_01_ver1-0.bin",	0x20000, 0x2c3d7ec4, 3 | BRF_PRG | BRF_ESS }, //  5

	{ "wp-92116_3_ver1-1.bin",	0x20000, 0x51dd594b, 4 | BRF_PRG | BRF_ESS }, //  6 68K #3 Code
	{ "wp-92116_4_ver1-1.bin",	0x20000, 0x9ba3cb7b, 4 | BRF_PRG | BRF_ESS }, //  7

	{ "wp-92116_5_ver1-0.bin",	0x20000, 0x8952e07c, 5 | BRF_PRG | BRF_ESS }, //  8 68K #4 Code
	{ "wp-92116_6_ver1-0.bin",	0x20000, 0x2c8108c2, 5 | BRF_PRG | BRF_ESS }, //  9

	{ "gp-9188a_26_ver1-0.bin",	0x40000, 0xbc48db69, 1 | BRF_PRG | BRF_ESS }, // 10 68K #0 Code
	{ "gp-9188a_21_ver1-0.bin",	0x40000, 0xc3192fbe, 1 | BRF_PRG | BRF_ESS }, // 11

	{ "gp-9188a_25_ver1-0.bin",	0x80000, 0xe69d3ccc, 1 | BRF_GRA },           // 12 Background Layer 0 Tiles

	{ "gp-9188a_28_ver1-0.bin",	0x80000, 0x2166c803, 2 | BRF_GRA },           // 13 Background Layer 1 Tiles

	{ "gp-9188a_30_ver1-0.bin",	0x20000, 0x35478ed9, 3 | BRF_GRA },           // 14 Background Layer 2 Tiles

	{ "mr92020-11_w65.bin",		0x80000, 0x585448cb, 4 | BRF_GRA },           // 15 Sprites
	{ "mr92020-12_w66.bin",		0x80000, 0x07104f78, 4 | BRF_GRA },           // 16
	{ "mr92020-13_w67.bin",		0x80000, 0xc4afa3dc, 4 | BRF_GRA },           // 17
	{ "mr92020-14_w68.bin",		0x80000, 0xc8676eca, 4 | BRF_GRA },           // 18
	{ "mr92020-15_w69.bin",		0x80000, 0xcc24a3d7, 4 | BRF_GRA },           // 19
	{ "mr92020-16_w70.bin",		0x80000, 0x6f4c9d4e, 4 | BRF_GRA },           // 20
	{ "gp-9189_7_ver1-0.bin",	0x80000, 0x1ea6a85d, 4 | BRF_GRA },           // 21
	{ "gp-9189_8_ver1-0.bin",	0x80000, 0xd9822da0, 4 | BRF_GRA },           // 22
	{ "gp-9189_9_ver1-0.bin",	0x80000, 0x0d4f6b5e, 4 | BRF_GRA },           // 23
	{ "gp-9189_10_ver1-0.bin",	0x80000, 0x9240969c, 4 | BRF_GRA },           // 24

	{ "mr92020-08_c50.bin",		0x80000, 0x5e840567, 5 | BRF_GRA },           // 25 Road Layer 0 Tiles
	{ "mr92020-07_c49.bin",		0x80000, 0x48d8ecb2, 5 | BRF_GRA },           // 26
	{ "mr92020-06_c51.bin",		0x80000, 0xc00f1245, 5 | BRF_GRA },           // 27
	{ "mr92020-05_c48.bin",		0x80000, 0x74ef3306, 5 | BRF_GRA },           // 28

	{ "mr92020-04_c47.bin",		0x80000, 0xc752f467, 6 | BRF_GRA },           // 29 Road Layer 1 Tiles
	{ "mr92020-03_c52.bin",		0x80000, 0x985b5fe0, 6 | BRF_GRA },           // 30
	{ "mr92020-02_c53.bin",		0x80000, 0xda961dd4, 6 | BRF_GRA },           // 31
	{ "mr92020-01_c46.bin",		0x80000, 0xf908c6e0, 6 | BRF_GRA },           // 32

	{ "wp-92116_2_ver1-0.bin",	0x80000, 0x95237bd0, 1 | BRF_SND },           // 33 MSM #0 Samples

	{ "wp-92116_1_ver1-0.bin",	0x80000, 0x559bafc3, 2 | BRF_SND },           // 34 MSM #1 Samples

	{ "ch9072_4.bin",			0x02000, 0xb45b4dc0, 0 | BRF_OPT },           // 35 Unused ROMs
	{ "ch9072_5.bin",			0x02000, 0xe122916b, 0 | BRF_OPT },           // 36
	{ "ch9072_6.bin",			0x02000, 0x05d95bf7, 0 | BRF_OPT },           // 37
	{ "ch9072_8.bin",			0x02000, 0x6bf52596, 0 | BRF_OPT },           // 38
	{ "mr90015-01_w06.bin",		0x80000, 0xce4bfe6e, 0 | BRF_OPT },           // 39
	{ "mr90015-02_w07.bin",		0x80000, 0xfcbecc9b, 0 | BRF_OPT },           // 40
	{ "mr90015-03_w08.bin",		0x80000, 0xccf5b158, 0 | BRF_OPT },           // 41
	{ "mr90015-04_w09.bin",		0x80000, 0x5b324c81, 0 | BRF_OPT },           // 42
	{ "mr90015_35_w33.bin",		0x80000, 0x9d428fb7, 0 | BRF_OPT },           // 43
	{ "pr88004q.bin",			0x00200, 0x9327dc37, 0 | BRF_OPT },           // 44
	{ "pr88004w.bin",			0x00100, 0x3d648467, 0 | BRF_OPT },           // 45
	{ "pr90015a.bin",			0x00800, 0x777583db, 0 | BRF_OPT },           // 46
	{ "pr90015b.bin",			0x00100, 0xbe240dac, 0 | BRF_OPT },           // 47
};

STD_ROM_PICK(wildplt)
STD_ROM_FN(wildplt)

struct BurnDriver BurnDrvWildplt = {
	"wildplt", NULL, NULL, NULL, "1992",
	"Wild Pilot\0", NULL, "Jaleco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, wildpltRomInfo, wildpltRomName, NULL, NULL, NULL, NULL, WildpltInputInfo, WildpltDIPInfo,
	WildpltInit, DrvExit, BigrunFrame, F1gpstarDraw, DrvScan, &DrvRecalc, 0x4000,
	256, 224, 4, 3
};


// Scud Hammer (ver 1.4)

static struct BurnRomInfo scudhammRomDesc[] = {
	{ "scud_hammer_cf-92128b-3_ver.1.4.bin",	0x40000, 0x4747370e, 1 | BRF_PRG | BRF_ESS }, //  0 68K #0 Code
	{ "scud_hammer_cf-92128b-4_ver.1.4.bin",	0x40000, 0x7a04955c, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "5",										0x80000, 0x714c115e, 1 | BRF_GRA },           //  2 Background Layer 0 Tiles

	{ "6",										0x20000, 0xb39aab63, 3 | BRF_GRA },           //  3 Background Layer 2 Tiles

	{ "1.bot",									0x80000, 0x46450d73, 4 | BRF_GRA },           //  4 Sprites
	{ "2.bot",									0x80000, 0xfb7b66dd, 4 | BRF_GRA },           //  5
	{ "3.bot",									0x80000, 0x7d45960b, 4 | BRF_GRA },           //  6
	{ "4.bot",									0x80000, 0x393b6a22, 4 | BRF_GRA },           //  7
	{ "5.bot",									0x80000, 0x7a3c33ad, 4 | BRF_GRA },           //  8
	{ "6.bot",									0x80000, 0xd19c4bf7, 4 | BRF_GRA },           //  9
	{ "7.bot",									0x80000, 0x9e5edf59, 4 | BRF_GRA },           // 10
	{ "8.bot",									0x80000, 0x4980051e, 4 | BRF_GRA },           // 11
	{ "9.bot",									0x80000, 0xc1b301f1, 4 | BRF_GRA },           // 12
	{ "10.bot",									0x80000, 0xdab4528f, 4 | BRF_GRA },           // 13

	{ "2.l",									0x80000, 0x889311da, 1 | BRF_SND },           // 14 MSM #0 Samples
	{ "2.h",									0x80000, 0x347928fc, 1 | BRF_SND },           // 15

	{ "1.l",									0x80000, 0x3c94aa90, 2 | BRF_SND },           // 16 MSM #1 Samples
	{ "1.h",									0x80000, 0x5caee787, 2 | BRF_SND },           // 17
};

STD_ROM_PICK(scudhamm)
STD_ROM_FN(scudhamm)

struct BurnDriver BurnDrvScudhamm = {
	"scudhamm", NULL, NULL, NULL, "1994",
	"Scud Hammer (ver 1.4)\0", NULL, "Jaleco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, scudhammRomInfo, scudhammRomName, NULL, NULL, NULL, NULL, ScudhammInputInfo, ScudhammDIPInfo,
	ScudhammInit, DrvExit, Single68KFrame, ScudhammDraw, DrvScan, &DrvRecalc, 0x4000,
	224, 256, 3, 4
};


// Scud Hammer (older version?)

static struct BurnRomInfo scudhammaRomDesc[] = {
	{ "3",						0x40000, 0xa908e7bd, 1 | BRF_PRG | BRF_ESS }, //  0 68K #0 Code
	{ "4",						0x40000, 0x981c8b02, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "5",						0x80000, 0x714c115e, 1 | BRF_GRA },           //  2 Background Layer 0 Tiles

	{ "6",						0x20000, 0xb39aab63, 3 | BRF_GRA },           //  3 Background Layer 2 Tiles

	{ "1.bot",					0x80000, 0x46450d73, 4 | BRF_GRA },           //  4 Sprites
	{ "2.bot",					0x80000, 0xfb7b66dd, 4 | BRF_GRA },           //  5
	{ "3.bot",					0x80000, 0x7d45960b, 4 | BRF_GRA },           //  6
	{ "4.bot",					0x80000, 0x393b6a22, 4 | BRF_GRA },           //  7
	{ "5.bot",					0x80000, 0x7a3c33ad, 4 | BRF_GRA },           //  8
	{ "6.bot",					0x80000, 0xd19c4bf7, 4 | BRF_GRA },           //  9
	{ "7.bot",					0x80000, 0x9e5edf59, 4 | BRF_GRA },           // 10
	{ "8.bot",					0x80000, 0x4980051e, 4 | BRF_GRA },           // 11
	{ "9.bot",					0x80000, 0xc1b301f1, 4 | BRF_GRA },           // 12
	{ "10.bot",					0x80000, 0xdab4528f, 4 | BRF_GRA },           // 13

	{ "2.l",					0x80000, 0x889311da, 1 | BRF_SND },           // 14 MSM #0 Samples
	{ "2.h",					0x80000, 0x347928fc, 1 | BRF_SND },           // 15

	{ "1.l",					0x80000, 0x3c94aa90, 2 | BRF_SND },           // 16 MSM #1 Samples
	{ "1.h",					0x80000, 0x5caee787, 2 | BRF_SND },           // 17
};

STD_ROM_PICK(scudhamma)
STD_ROM_FN(scudhamma)

struct BurnDriver BurnDrvScudhamma = {
	"scudhamma", "scudhamm", NULL, NULL, "1994",
	"Scud Hammer (older version?)\0", NULL, "Jaleco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, scudhammaRomInfo, scudhammaRomName, NULL, NULL, NULL, NULL, ScudhammInputInfo, ScudhammaDIPInfo,
	ScudhammInit, DrvExit, Single68KFrame, ScudhammDraw, DrvScan, &DrvRecalc, 0x4000,
	224, 256, 3, 4
};


// Arm Champs II v2.7

static struct BurnRomInfo armchmp2RomDesc[] = {
	{ "4_ver_2.7.ic63",			0x20000, 0xe0cec032, 1 | BRF_PRG | BRF_ESS }, //  0 68K #0 Code
	{ "3_ver_2.7.ic62",			0x20000, 0x44186a37, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "mr91042-07-r66_6.ic95",	0x80000, 0xd1be8699, 1 | BRF_GRA },           //  2 Background Layer 0 Tiles

	{ "ac91106_ver1.2_7.ic99",	0x20000, 0x09755aef, 3 | BRF_GRA },           //  3 Background Layer 2 Tiles

	{ "mr91042-01-r60_1.ic1",	0x80000, 0xfdfe6951, 4 | BRF_GRA },           //  4 Sprites
	{ "mr91042-02-r61_2.ic2",	0x80000, 0x2e6c8b30, 4 | BRF_GRA },           //  5
	{ "mr91042-03-r62_3.ic5",	0x80000, 0x07ba6d3a, 4 | BRF_GRA },           //  6
	{ "mr91042-04-r63_4.ic6",	0x80000, 0xf37cb12c, 4 | BRF_GRA },           //  7
	{ "mr91042-05-r64_5.ic11",	0x80000, 0x7a3bb52d, 4 | BRF_GRA },           //  8
	{ "mr91042-06-r65_6.ic12",	0x80000, 0x5312a4f2, 4 | BRF_GRA },           //  9

	{ "mr91042-08_2.ic57",		0x80000, 0xdc015f6c, 1 | BRF_SND },           // 10 MSM #0 Samples

	{ "ac-91106v2.0_1.ic56",	0x80000, 0x0ff5cbcf, 2 | BRF_SND },           // 11 MSM #1 Samples

	{ "ch9072-4_13.ic39",		0x02000, 0xb45b4dc0, 0 | BRF_OPT },           // 12 Unused ROMs
	{ "ch9072-5_11.ic33",		0x02000, 0xe122916b, 0 | BRF_OPT },           // 13
	{ "ch9072-6_12.ic35",		0x02000, 0x05d95bf7, 0 | BRF_OPT },           // 14
	{ "ch9072-8_15.ic59",		0x02000, 0x6bf52596, 0 | BRF_OPT },           // 15
	{ "mr90015-35-w33_17.ic67",	0x80000, 0x9d428fb7, 0 | BRF_OPT },           // 16
	{ "mr90015-35-w33_14.ic54",	0x80000, 0x9d428fb7, 0 | BRF_OPT },           // 17
	{ "pr88004q_8.ic102",		0x00200, 0x9327dc37, 0 | BRF_OPT },           // 18
	{ "pr88004w_16.ic66",		0x00100, 0x3d648467, 0 | BRF_OPT },           // 19
	{ "pr91042_5.ic91",			0x00100, 0xe71de4aa, 0 | BRF_OPT },           // 20
};

STD_ROM_PICK(armchmp2)
STD_ROM_FN(armchmp2)

struct BurnDriver BurnDrvArmchmp2 = {
	"armchmp2", NULL, NULL, NULL, "1992",
	"Arm Champs II v2.7\0", NULL, "Jaleco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, armchmp2RomInfo, armchmp2RomName, NULL, NULL, NULL, NULL, Armchmp2InputInfo, Armchmp2DIPInfo,
	Armchmp2Init, DrvExit, Single68KFrame, ScudhammDraw, DrvScan, &DrvRecalc, 0x4000,
	224, 256, 3, 4
};


// Arm Champs II v2.6

static struct BurnRomInfo armchmp2o2RomDesc[] = {
	{ "ac-91106v2.6_4.ic63",	0x20000, 0xe0cec032, 1 | BRF_PRG | BRF_ESS }, //  0 68K #0 Code
	{ "ac-91106v2.6_3.ic62",	0x20000, 0x5de6da19, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "mr91042-07-r66_6.ic95",	0x80000, 0xd1be8699, 1 | BRF_GRA },           //  2 Background Layer 0 Tiles

	{ "ac91106_ver1.2_7.ic99",	0x20000, 0x09755aef, 3 | BRF_GRA },           //  3 Background Layer 2 Tiles

	{ "mr91042-01-r60_1.ic1",	0x80000, 0xfdfe6951, 4 | BRF_GRA },           //  4 Sprites
	{ "mr91042-02-r61_2.ic2",	0x80000, 0x2e6c8b30, 4 | BRF_GRA },           //  5
	{ "mr91042-03-r62_3.ic5",	0x80000, 0x07ba6d3a, 4 | BRF_GRA },           //  6
	{ "mr91042-04-r63_4.ic6",	0x80000, 0xf37cb12c, 4 | BRF_GRA },           //  7
	{ "mr91042-05-r64_5.ic11",	0x80000, 0x7a3bb52d, 4 | BRF_GRA },           //  8
	{ "mr91042-06-r65_6.ic12",	0x80000, 0x5312a4f2, 4 | BRF_GRA },           //  9

	{ "mr91042-08_2.ic57",		0x80000, 0xdc015f6c, 1 | BRF_SND },           // 10 MSM #0 Samples

	{ "ac-91106v2.0_1.ic56",	0x80000, 0x0ff5cbcf, 2 | BRF_SND },           // 11 MSM #1 Samples

	{ "ch9072-4_13.ic39",		0x02000, 0xb45b4dc0, 0 | BRF_OPT },           // 12 Unused ROMs
	{ "ch9072-5_11.ic33",		0x02000, 0xe122916b, 0 | BRF_OPT },           // 13
	{ "ch9072-6_12.ic35",		0x02000, 0x05d95bf7, 0 | BRF_OPT },           // 14
	{ "ch9072-8_15.ic59",		0x02000, 0x6bf52596, 0 | BRF_OPT },           // 15
	{ "mr90015-35-w33_17.ic67",	0x80000, 0x9d428fb7, 0 | BRF_OPT },           // 16
	{ "mr90015-35-w33_14.ic54",	0x80000, 0x9d428fb7, 0 | BRF_OPT },           // 17
	{ "pr88004q_8.ic102",		0x00200, 0x9327dc37, 0 | BRF_OPT },           // 18
	{ "pr88004w_16.ic66",		0x00100, 0x3d648467, 0 | BRF_OPT },           // 19
	{ "pr91042_5.ic91",			0x00100, 0xe71de4aa, 0 | BRF_OPT },           // 20
};

STD_ROM_PICK(armchmp2o2)
STD_ROM_FN(armchmp2o2)

struct BurnDriver BurnDrvArmchmp2o2 = {
	"armchmp2o2", "armchmp2", NULL, NULL, "1992",
	"Arm Champs II v2.6\0", NULL, "Jaleco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, armchmp2o2RomInfo, armchmp2o2RomName, NULL, NULL, NULL, NULL, Armchmp2InputInfo, Armchmp2DIPInfo,
	Armchmp2Init, DrvExit, Single68KFrame, ScudhammDraw, DrvScan, &DrvRecalc, 0x4000,
	224, 256, 3, 4
};


// Arm Champs II v1.7

static struct BurnRomInfo armchmp2oRomDesc[] = {
	{ "ac91106_ver1.7_4.ic63",	0x20000, 0xaaa11bc7, 1 | BRF_PRG | BRF_ESS }, //  0 68K #0 Code
	{ "ac91106_ver1.7_3.ic62",	0x20000, 0xa7965879, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "mr91042-07-r66_6.ic95",	0x80000, 0xd1be8699, 1 | BRF_GRA },           //  2 Background Layer 0 Tiles

	{ "ac91106_ver1.2_7.ic99",	0x20000, 0x09755aef, 3 | BRF_GRA },           //  3 Background Layer 2 Tiles

	{ "mr91042-01-r60_1.ic1",	0x80000, 0xfdfe6951, 4 | BRF_GRA },           //  4 Sprites
	{ "mr91042-02-r61_2.ic2",	0x80000, 0x2e6c8b30, 4 | BRF_GRA },           //  5
	{ "mr91042-03-r62_3.ic5",	0x80000, 0x07ba6d3a, 4 | BRF_GRA },           //  6
	{ "mr91042-04-r63_4.ic6",	0x80000, 0xf37cb12c, 4 | BRF_GRA },           //  7
	{ "mr91042-05-r64_5.ic11",	0x80000, 0x7a3bb52d, 4 | BRF_GRA },           //  8
	{ "mr91042-06-r65_6.ic12",	0x80000, 0x5312a4f2, 4 | BRF_GRA },           //  9

	{ "mr91042-08_2.ic57",		0x80000, 0xdc015f6c, 1 | BRF_SND },           // 10 MSM #0 Samples

	{ "ac91106_ver1.2_1.ic56",	0x80000, 0x48208b69, 2 | BRF_SND },           // 11 MSM #1 Samples

	{ "ch9072-4_13.ic39",		0x02000, 0xb45b4dc0, 0 | BRF_OPT },           // 12 Unused ROMs
	{ "ch9072-5_11.ic33",		0x02000, 0xe122916b, 0 | BRF_OPT },           // 13
	{ "ch9072-6_12.ic35",		0x02000, 0x05d95bf7, 0 | BRF_OPT },           // 14
	{ "ch9072-8_15.ic59",		0x02000, 0x6bf52596, 0 | BRF_OPT },           // 15
	{ "mr90015-35-w33_17.ic67",	0x80000, 0x9d428fb7, 0 | BRF_OPT },           // 16
	{ "mr90015-35-w33_14.ic54",	0x80000, 0x9d428fb7, 0 | BRF_OPT },           // 17
	{ "pr88004q_8.ic102",		0x00200, 0x9327dc37, 0 | BRF_OPT },           // 18
	{ "pr88004w_16.ic66",		0x00100, 0x3d648467, 0 | BRF_OPT },           // 19
	{ "pr91042_5.ic91",			0x00100, 0xe71de4aa, 0 | BRF_OPT },           // 20
};

STD_ROM_PICK(armchmp2o)
STD_ROM_FN(armchmp2o)

struct BurnDriver BurnDrvArmchmp2o = {
	"armchmp2o", "armchmp2", NULL, NULL, "1992",
	"Arm Champs II v1.7\0", NULL, "Jaleco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, armchmp2oRomInfo, armchmp2oRomName, NULL, NULL, NULL, NULL, Armchmp2InputInfo, Armchmp2DIPInfo,
	Armchmp2Init, DrvExit, Single68KFrame, ScudhammDraw, DrvScan, &DrvRecalc, 0x4000,
	224, 256, 3, 4
};
