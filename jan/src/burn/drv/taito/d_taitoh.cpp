// FB Alpha Taito System H driver module
// Based on MAME driver by Yochizo and Nicola Salmoria

// Weirdness:
// DLeagueJ's sprites get covered up on the right side of the screen, see temp.
//   fix in DleagueJDraw();

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "taito.h"
#include "taito_ic.h"
#include "burn_ym2610.h"

static UINT8 *TaitoDirtyTile;
static UINT16 *TaitoTempBitmap[2];
static INT32 address_xor;
static UINT8 *transparent_tile_lut;
static INT32 screen_y_adjust;
static INT32 screen_x_adjust;
static UINT8 flipscreen;
static INT32 irq_config;
static UINT8 TaitoInputConfig;

static UINT8 DrvJoy1[8]; // Syvalion synthesized digital inputs

static INT32 syvalionpmode = 0;

static INT32 DrvAnalogPort0 = 0;
static INT32 DrvAnalogPort1 = 0;
static INT32 DrvAnalogPort2 = 0;
static INT32 DrvAnalogPort3 = 0;

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}

static struct BurnInputInfo SyvalionInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	TC0220IOCInputPort0 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	TC0220IOCInputPort0 + 6,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	TC0220IOCInputPort1 + 4,	"p1 fire 1"	},
	A("P1 Paddle X",	BIT_ANALOG_REL, &DrvAnalogPort0,		"p1 x-axis"     ),
	A("P1 Paddle Y",	BIT_ANALOG_REL, &DrvAnalogPort1,		"p1 y-axis"     ),

	{"P2 Coin",		BIT_DIGITAL,	TC0220IOCInputPort0 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	TC0220IOCInputPort0 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	TC0220IOCInputPort1 + 0,	"p2 fire 1"	},
	A("P2 Paddle X",	BIT_ANALOG_REL, &DrvAnalogPort2,		"p2 x-axis"     ),
	A("P2 Paddle Y",	BIT_ANALOG_REL, &DrvAnalogPort3,		"p2 y-axis"     ),

	{"Reset",		BIT_DIGITAL,	&TaitoReset,			"reset"		},
	{"Service",		BIT_DIGITAL,	TC0220IOCInputPort0 + 4,	"service"	},
	{"Tilt",		BIT_DIGITAL,	TC0220IOCInputPort0 + 5,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	TC0220IOCDip + 0,		"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	TC0220IOCDip + 1,		"dip"		},
};
#undef A

STDINPUTINFO(Syvalion)

static struct BurnInputInfo TetristhInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	TC0220IOCInputPort0 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	TC0220IOCInputPort1 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	TC0220IOCInputPort2 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	TC0220IOCInputPort2 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	TC0220IOCInputPort2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	TC0220IOCInputPort2 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	TC0220IOCInputPort1 + 2,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	TC0220IOCInputPort0 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	TC0220IOCInputPort1 + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	TC0220IOCInputPort2 + 4,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	TC0220IOCInputPort2 + 5,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	TC0220IOCInputPort2 + 6,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	TC0220IOCInputPort2 + 7,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	TC0220IOCInputPort1 + 3,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&TaitoReset,			"reset"		},
	{"Service",		BIT_DIGITAL,	TC0220IOCInputPort0 + 4,	"service"	},
	{"Tilt",		BIT_DIGITAL,	TC0220IOCInputPort0 + 5,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	TC0220IOCDip + 0,		"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	TC0220IOCDip + 1,		"dip"		},
};

STDINPUTINFO(Tetristh)

static struct BurnInputInfo RecordbrInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	TC0220IOCInputPort0 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	TC0220IOCInputPort1 + 0,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	TC0220IOCInputPort2 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	TC0220IOCInputPort2 + 1,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	TC0220IOCInputPort2 + 2,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	TC0220IOCInputPort0 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	TC0220IOCInputPort1 + 1,	"p2 start"	},
	{"P2 Button 1",		BIT_DIGITAL,	TC0220IOCInputPort2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	TC0220IOCInputPort2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	TC0220IOCInputPort2 + 6,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&TaitoReset,			"reset"		},
	{"Service",		BIT_DIGITAL,	TC0220IOCInputPort0 + 4,	"service"	},
	{"Tilt",		BIT_DIGITAL,	TC0220IOCInputPort0 + 5,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	TC0220IOCDip + 0,		"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	TC0220IOCDip + 1,		"dip"		},
};

STDINPUTINFO(Recordbr)

static struct BurnInputInfo DleagueInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	TC0220IOCInputPort0 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	TC0220IOCInputPort0 + 4,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	TC0220IOCInputPort1 + 0,	"p1 up"	},
	{"P1 Down",		BIT_DIGITAL,	TC0220IOCInputPort1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	TC0220IOCInputPort1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	TC0220IOCInputPort1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	TC0220IOCInputPort2 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	TC0220IOCInputPort2 + 1,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	TC0220IOCInputPort0 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	TC0220IOCInputPort0 + 5,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	TC0220IOCInputPort1 + 4,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	TC0220IOCInputPort1 + 5,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	TC0220IOCInputPort1 + 6,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	TC0220IOCInputPort1 + 7,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	TC0220IOCInputPort2 + 2,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	TC0220IOCInputPort2 + 3,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&TaitoReset,			"reset"		},
	{"Service",		BIT_DIGITAL,	TC0220IOCInputPort0 + 0,	"service"	},
	{"Tilt",		BIT_DIGITAL,	TC0220IOCInputPort0 + 1,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	TC0220IOCDip + 0,		"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	TC0220IOCDip + 1,		"dip"		},
};

STDINPUTINFO(Dleague)

static struct BurnDIPInfo SyvalionDIPList[]=
{
	{0x15, 0xff, 0xff, 0xfe, NULL			},
	{0x16, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    1, "Cabinet"		},
	{0x15, 0x01, 0x01, 0x00, "Upright"		},
	//{0x15, 0x01, 0x01, 0x01, "Cocktail"		}, //needs screen flipping for this to work

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x15, 0x01, 0x02, 0x02, "Off"			},
	{0x15, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x15, 0x01, 0x04, 0x04, "Off"			},
	{0x15, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x15, 0x01, 0x08, 0x00, "Off"			},
	{0x15, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x15, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x15, 0x01, 0x30, 0x00, "2 Coins 3 Credits"	},
	{0x15, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x15, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"	},
	{0x15, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x15, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"	},
	{0x15, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x16, 0x01, 0x03, 0x02, "Easy"			},
	{0x16, 0x01, 0x03, 0x03, "Medium"		},
	{0x16, 0x01, 0x03, 0x01, "Hard"			},
	{0x16, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x16, 0x01, 0x0c, 0x08, "1000k"		},
	{0x16, 0x01, 0x0c, 0x0c, "1500k"		},
	{0x16, 0x01, 0x0c, 0x04, "2000k"		},
	{0x16, 0x01, 0x0c, 0x00, "None"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x16, 0x01, 0x30, 0x00, "2"			},
	{0x16, 0x01, 0x30, 0x30, "3"			},
	{0x16, 0x01, 0x30, 0x20, "4"			},
	{0x16, 0x01, 0x30, 0x10, "5"			},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x16, 0x01, 0x40, 0x40, "Off"			},
	{0x16, 0x01, 0x40, 0x00, "On"			},
};

STDDIPINFO(Syvalion)

static struct BurnDIPInfo TetristhDIPList[]=
{
	{0x11, 0xff, 0xff, 0xff, NULL			},
	{0x12, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x11, 0x01, 0x02, 0x02, "Off"			},
	{0x11, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x11, 0x01, 0x04, 0x04, "Off"			},
	{0x11, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x11, 0x01, 0x08, 0x00, "Off"			},
	{0x11, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x11, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x30, 0x00, "2 Coins 3 Credits"	},
	{0x11, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x11, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"	},
	{0x11, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"	},
};

STDDIPINFO(Tetristh)

static struct BurnDIPInfo RecordbrDIPList[]=
{
	{0x0d, 0xff, 0xff, 0xff, NULL			},
	{0x0e, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    0, "Flip Screen"		},
	{0x0d, 0x01, 0x02, 0x02, "Off"			},
	{0x0d, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x0d, 0x01, 0x04, 0x04, "Off"			},
	{0x0d, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x0d, 0x01, 0x08, 0x00, "Off"			},
	{0x0d, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    2, "Coin A"		},
	{0x0d, 0x01, 0x30, 0x00, "4 Coins 1 Credits"	},
	{0x0d, 0x01, 0x30, 0x10, "3 Coins 1 Credits"	},
	{0x0d, 0x01, 0x30, 0x20, "2 Coins 1 Credits"	},
	{0x0d, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x0d, 0x01, 0xc0, 0xc0, "1 Coin  2 Credits"	},
	{0x0d, 0x01, 0xc0, 0x80, "1 Coin  3 Credits"	},
	{0x0d, 0x01, 0xc0, 0x40, "1 Coin  4 Credits"	},
	{0x0d, 0x01, 0xc0, 0x00, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x0e, 0x01, 0x03, 0x02, "Easy"			},
	{0x0e, 0x01, 0x03, 0x03, "Medium"		},
	{0x0e, 0x01, 0x03, 0x01, "Hard"			},
	{0x0e, 0x01, 0x03, 0x00, "Hardest"		},
};

STDDIPINFO(Recordbr)

static struct BurnDIPInfo GogoldDIPList[]=
{
	{0x0d, 0xff, 0xff, 0xff, NULL			},
	{0x0e, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    0, "Flip Screen"		},
	{0x0d, 0x01, 0x02, 0x02, "Off"			},
	{0x0d, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x0d, 0x01, 0x04, 0x04, "Off"			},
	{0x0d, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x0d, 0x01, 0x08, 0x00, "Off"			},
	{0x0d, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    2, "Coin A"		},
	{0x0d, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x0d, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x0d, 0x01, 0x30, 0x00, "2 Coins 3 Credits"	},
	{0x0d, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x0d, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"	},
	{0x0d, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x0d, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"	},
	{0x0d, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x0e, 0x01, 0x03, 0x02, "Easy"			},
	{0x0e, 0x01, 0x03, 0x03, "Medium"		},
	{0x0e, 0x01, 0x03, 0x01, "Hard"			},
	{0x0e, 0x01, 0x03, 0x00, "Hardest"		},
};

STDDIPINFO(Gogold)

static struct BurnDIPInfo DleagueDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x13, 0x01, 0x01, 0x01, "Constant"		},
	{0x13, 0x01, 0x01, 0x00, "Based on Inning"	},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x02, 0x02, "Off"			},
	{0x13, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x13, 0x01, 0x04, 0x04, "Off"			},
	{0x13, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x08, 0x00, "Off"			},
	{0x13, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{0x13, 0x01, 0x30, 0x00, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x10, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x20, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    4, "Price to Continue"	},
	{0x13, 0x01, 0xc0, 0x00, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0xc0, 0x80, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xc0, 0xc0, "Same as Start"	},

	{0   , 0xfe, 0   ,    0, "Extra Credit Needed"		},
	{0x14, 0x01, 0x0c, 0x08, "After Inning 6"		},
	{0x14, 0x01, 0x0c, 0x00, "After Innings 5 and 7"	},
	{0x14, 0x01, 0x0c, 0x0c, "After Innings 3 and 6"	},
	{0x14, 0x01, 0x0c, 0x04, "After Innings 3, 5 and 7"	},
};

STDDIPINFO(Dleague)

static struct BurnDIPInfo DleaguejDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x13, 0x01, 0x01, 0x01, "Constant"		},
	{0x13, 0x01, 0x01, 0x00, "Based on Inning"	},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x02, 0x02, "Off"			},
	{0x13, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x13, 0x01, 0x04, 0x04, "Off"			},
	{0x13, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x08, 0x00, "Off"			},
	{0x13, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x13, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x30, 0x00, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x13, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    0, "Extra Credit Needed"		},
	{0x14, 0x01, 0x0c, 0x08, "After Inning 6"		},
	{0x14, 0x01, 0x0c, 0x00, "After Innings 5 and 7"	},
	{0x14, 0x01, 0x0c, 0x0c, "After Innings 3 and 6"	},
	{0x14, 0x01, 0x0c, 0x04, "After Innings 3, 5 and 7"	},
};

STDDIPINFO(Dleaguej)

static void __fastcall syvalion_main_write_word(UINT32 address, UINT16 data)
{
	switch (address ^ address_xor)
	{
		case 0x200000:
		case 0x200001:
			TC0220IOCWrite(0, data & 0xff);
		return;

		case 0x200002:
		case 0x200003:
			TC0220IOCHalfWordPortWrite(data & 0xff);
		return;

		case 0x300000:
		case 0x300001:
			TC0140SYTPortWrite(data & 0xff);
		return;

		case 0x300002:
		case 0x300003:
			TC0140SYTCommWrite(data & 0xff);
		return;
	}
}

static void __fastcall syvalion_main_write_byte(UINT32 address, UINT8 data)
{
	switch (address ^ address_xor)
	{
		case 0x200000:
		case 0x200001:
			TC0220IOCWrite(0, data & 0xff);
		return;

		case 0x200002:
		case 0x200003:
			TC0220IOCHalfWordPortWrite(data & 0xff);
		return;

		case 0x300000:
		case 0x300001:
			TC0140SYTPortWrite(data & 0xff);
		return;

		case 0x300002:
		case 0x300003:
			TC0140SYTCommWrite(data & 0xff);
		return;
	}
}

// coord-buffers. syvalion expects each coord to return 0 if there was no
// change since the last read.
static INT32 PaddleX[2] = { 0, 0 };
static INT32 PaddleY[2] = { 0, 0 };
static INT32 PaddleX2[2] = { 0, 0 };
static INT32 PaddleY2[2] = { 0, 0 };

static INT32 Paddle_read(INT32 PaddlePortnum, INT32 *Paddle_X) {
	if (PaddlePortnum != *Paddle_X) {
		*Paddle_X = PaddlePortnum;
		return PaddlePortnum / 4;
	} else {
		return 0;
	}
}

static UINT8 syvalion_extended_read()
{
	static UINT8 DOWN_LATCH[2] = { 0, 0 };
	// Simulating digital inputs in Syvalion notes - dink aug.2016
	//  when down is pressed, the body goes "down" but the head points "up".
	//  this is solved by latching the down button(s) and returning 0xf2 in
	//  the next read of the "up" port. (then, of course, clearing the latch)

	UINT8 port = TC0220IOCPortRead();

	UINT8 ret = 0;

	INT32 AnalogPorts[2][2] = {{ DrvAnalogPort0, DrvAnalogPort1 }, { DrvAnalogPort2, DrvAnalogPort3 }};
	INT32 DigitalPorts[2][4] = {{ DrvJoy1[0], DrvJoy1[1], DrvJoy1[3], DrvJoy1[2] }, { DrvJoy1[4], DrvJoy1[5], DrvJoy1[7], DrvJoy1[6] }};

	if (syvalionpmode) {
		AnalogPorts[0][0] = DrvAnalogPort1;
		AnalogPorts[0][1] = 0-DrvAnalogPort0;
		AnalogPorts[1][0] = DrvAnalogPort3;
		AnalogPorts[1][1] = 0-DrvAnalogPort2;

		DigitalPorts[0][0] = DrvJoy1[3]; // p1
		DigitalPorts[0][1] = DrvJoy1[2];
		DigitalPorts[0][2] = DrvJoy1[1];
		DigitalPorts[0][3] = DrvJoy1[0];

		DigitalPorts[1][0] = DrvJoy1[5]; // p2
		DigitalPorts[1][1] = DrvJoy1[4];
		DigitalPorts[1][2] = DrvJoy1[7];
		DigitalPorts[1][3] = DrvJoy1[6];
	}

	if (port < 8) ret = TC0220IOCRead(port);

	UINT8 plrnum = ((port & 4) >> 2) ^ 1;
	if (!syvalionpmode) {
		switch (port & ~4) // [syvalion] Syvalion (Japan)
		{
			// P2 UP
			case 0x08: if (DigitalPorts[plrnum][0]) return 0x10;
			else if (DOWN_LATCH[plrnum]) { DOWN_LATCH[plrnum] = 0; return 0xf2; }
			else {
				ret = PaddleY2[plrnum]&0xff; PaddleY2[plrnum] = 0;
				break;
			}
			// P2 DOWN
			case 0x09: if (DigitalPorts[plrnum][1]) { DOWN_LATCH[plrnum] = 1; return 0xff; }
			else {
				PaddleY2[plrnum] = 0-Paddle_read(AnalogPorts[plrnum][1], &PaddleY[plrnum]);
				ret = (PaddleY2[plrnum] & 0x3000) ? 0xff : 0x00;
				break;
			}
			// P2 RIGHT
			case 0x0a: if (DigitalPorts[plrnum][2]) return 0x10;
			else {
				ret = PaddleX2[plrnum]&0xff; PaddleX2[plrnum] = 0;
				break;
			}
			// P2 LEFT
			case 0x0b: if (DigitalPorts[plrnum][3]) return 0xff;
			else {
				PaddleX2[plrnum] = Paddle_read(AnalogPorts[plrnum][0], &PaddleX[plrnum]);
				ret = (PaddleX2[plrnum] & 0x3000) ? 0xff : 0x00;
				break;
			}
		}
	} else {
		switch (port & ~4) // [syvalionp] Syvalion (World, prototype)
		{
			// P1 RIGHT
			case 0x08: if (DigitalPorts[plrnum][0]) return 0x10;
			else {
				ret = PaddleY2[plrnum]&0xff; PaddleY2[plrnum] = 0;
				break;
			}
			// P1 LEFT
			case 0x09: if (DigitalPorts[plrnum][1]) return 0xff;
			else {
				PaddleY2[plrnum] = 0-Paddle_read(AnalogPorts[plrnum][1], &PaddleY[plrnum]);
				ret = (PaddleY2[plrnum] & 0x3000) ? 0xff : 0x00;
				break;
			}
			// P1 DOWN
			case 0x0a: if (DigitalPorts[plrnum][2]) return 0x10;
			else if (DOWN_LATCH[plrnum]) { DOWN_LATCH[plrnum] = 0; return 0xf2; }
			else {
				ret = PaddleX2[plrnum]&0xff; PaddleX2[plrnum] = 0;
				break;
			}
			// P1 UP
			case 0x0b: if (DigitalPorts[plrnum][3]) { DOWN_LATCH[plrnum] = 1; return 0xff; }
			else {
				PaddleX2[plrnum] = Paddle_read(AnalogPorts[plrnum][0], &PaddleX[plrnum]);
				ret = (PaddleX2[plrnum] & 0x3000) ? 0xff : 0x00;
				break;
			}
		}
	}
	return ret;
}

static UINT16 __fastcall syvalion_main_read_word(UINT32 address)
{
	switch (address ^ address_xor)
	{
		case 0x200000:
		case 0x200001:
			return syvalion_extended_read();

		case 0x200002:
		case 0x200003:
			return TC0220IOCPortRead();

		case 0x300002:
		case 0x300003:
			return TC0140SYTCommRead();
	}

	return 0;
}

static UINT8 __fastcall syvalion_main_read_byte(UINT32 address)
{
	switch (address ^ address_xor)
	{
		case 0x200000:
		case 0x200001:
			return syvalion_extended_read();

		case 0x200002:
		case 0x200003:
			return TC0220IOCPortRead();

		case 0x300002:
		case 0x300003:
			return TC0140SYTCommRead();
	}

	return 0;
}

static void __fastcall dleague_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfffff0) == 0x200000) {
		TC0220IOCWrite((address / 2) & 7, data);
		return;
	}

	switch (address)
	{
		case 0x300000:
		case 0x300001:
			TC0140SYTPortWrite(data & 0xff);
		return;

		case 0x300002:
		case 0x300003:
			TC0140SYTCommWrite(data & 0xff);
		return;
	}
}

static void __fastcall dleague_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xfffff0) == 0x200000) {
		TC0220IOCWrite((address / 2) & 7, data);
		return;
	}

	switch (address)
	{
		case 0x300000:
		case 0x300001:
			TC0140SYTPortWrite(data & 0xff);
		return;

		case 0x300002:
		case 0x300003:
			TC0140SYTCommWrite(data & 0xff);
		return;
	}
}

static UINT16 __fastcall dleague_main_read_word(UINT32 address)
{
	if ((address & 0xfffff0) == 0x200000) {
		return TC0220IOCRead((address / 2) & 7);
	}

	switch (address)
	{
		case 0x200002:
		case 0x200003:
			return TC0220IOCPortRead();

		case 0x300002:
		case 0x300003:
			return TC0140SYTCommRead();
	}

	return 0;
}

static UINT8 __fastcall dleague_main_read_byte(UINT32 address)
{
	if ((address & 0xfffff0) == 0x200000) {
		return TC0220IOCRead((address / 2) & 7);
	}

	switch (address)
	{
		case 0x200002:
		case 0x200003:
			return TC0220IOCPortRead();

		case 0x300002:
		case 0x300003:
			return TC0140SYTCommRead();
	}

	return 0;
}

static inline void taitob_single_char_update(INT32 offset)
{
	UINT8 *dst = TaitoCharsB + (offset * 4);
	UINT8 d0 = TaitoVideoRam[offset + 0x00000];
	UINT8 d1 = TaitoVideoRam[offset + 0x00001];
	UINT8 d2 = TaitoVideoRam[offset + 0x10000];

	for (INT32 j = 0; j < 8; j++)
	{
		dst[j] = ((d0 >> j) & 1) | (((d1 >> j) & 1) << 1) | (((d2 >> j) & 1) << 2);
	}
}

static void __fastcall taitoh_video_write_word(UINT32 address, UINT16 data)
{
	if (address >= 0x400000 && address <= 0x420fff) {
		INT32 offset = (address & 0x3fffe) / 2;
		UINT16 *ram = (UINT16*)TaitoVideoRam;
		UINT16 old = ram[offset];
		ram[offset] = data;

		if ((address & 0xfec000) == 0x40c000) {
			if (old != data) {
				TaitoDirtyTile[offset & 0x1fff] = 1;
			}
		}

		if ((address & 0xfef000) == 0x400000) {
			taitob_single_char_update(address & 0xffe);
			return;
		}

		return;
	}
}

static void __fastcall taitoh_video_write_byte(UINT32 address, UINT8 data)
{
	if (address >= 0x400000 && address <= 0x420fff) {
		INT32 offset = (address & 0x3ffff) ^ 1;
		UINT8 old = TaitoVideoRam[offset];
		TaitoVideoRam[offset] = data;

		if ((address & 0xfec000) == 0x40c000) {
			if (old != data) {
				TaitoDirtyTile[offset & 0x1fff] = 1;
			}
		}

		if ((address & 0xfef000) == 0x400000) {
			taitob_single_char_update(address & 0xffe);
		}

		return;
	}
}

static void bankswitch(INT32 data)
{
	TaitoZ80Bank = data & 3;

	ZetMapMemory(TaitoZ80Rom1 + (TaitoZ80Bank * 0x4000), 0x4000, 0x7fff, MAP_ROM);
}

static void __fastcall taitoh_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xe000:
		case 0xe001:
		case 0xe002:
		case 0xe003:
			BurnYM2610Write(address & 3, data);
		return;

		case 0xe200:
			TC0140SYTSlavePortWrite(data);
		return;

		case 0xe201:
			TC0140SYTSlaveCommWrite(data);
		return;

		case 0xe400:
		case 0xe401:
		case 0xe402:
			// pan control
		return;

		case 0xee00:
		case 0xf000:
		return;		// nop

		case 0xf200:
			bankswitch(data);
		return;
	}
}

static UINT8 __fastcall taitoh_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xe000:
		case 0xe001:
		case 0xe002:
		case 0xe003:
			return BurnYM2610Read(address & 3);

		case 0xe201:
			return TC0140SYTSlaveCommRead();

		case 0xea00:
			return 0; // nop
	}

	return 0;
}

static void DrvFMIRQHandler(INT32, INT32 nStatus)
{
	ZetSetIRQLine(0, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 DrvSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)ZetTotalCycles() * nSoundRate / 4000000;
}

static double DrvGetTime()
{
	return (double)ZetTotalCycles() / 4000000.00;
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (TaitoRamStart, 0, TaitoRamEnd - TaitoRamStart);
	}

	memset (TaitoDirtyTile, 1, 0x2000);

	SekOpen(0);
	SekReset();
	SekClose();

	ZetOpen(0);
	ZetReset();
	bankswitch(0);
	BurnYM2610Reset();
	ZetClose();

	TaitoICReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = TaitoMem;

	Taito68KRom1			= Next; Next += 0x080000;
	TaitoZ80Rom1			= Next; Next += 0x010000;

	TaitoChars			= Next; Next += 0x800000;

	TaitoYM2610BRom			= Next; Next += 0x080000;
	TaitoYM2610ARom			= Next; Next += 0x080000;

	transparent_tile_lut		= Next; Next += 0x800000 / 0x100;

	TaitoPalette			= (UINT32*)Next; Next += 0x0220 * sizeof(UINT32);

	TaitoDirtyTile			= Next; Next += 0x002000;

	TaitoTempBitmap[0]		= (UINT16*)Next; Next += 1024 * 1024 * 2;
	TaitoTempBitmap[1]		= (UINT16*)Next; Next += 1024 * 1024 * 2;

	TaitoRamStart			= Next;

	Taito68KRam1			= Next; Next += 0x010000;
	TaitoPaletteRam			= Next; Next += 0x000800;
	TaitoVideoRam			= Next; Next += 0x021000;

	TaitoZ80Ram1			= Next; Next += 0x002000;
	TaitoCharsB			= Next; Next += 0x004000;

	TaitoRamEnd			= Next;

	TaitoMemEnd			= Next;

	return 0;
}

static void DrvTransTab()
{
	for (INT32 i = 0; i < 0x800000; i+= 0x100)
	{
		transparent_tile_lut[i/0x100] = 1; // mark as transparent

		for (INT32 j = 0; j < 0x100; j++)
		{
			if (TaitoChars[i+j])
			{
				transparent_tile_lut[i/0x100] = 0; // mark as not transparent
			}
		}
	}
}

static INT32 DrvGfxDecode()
{
	INT32 gfxlen = TaitoCharRomSize;

	INT32 Planes[4] = { STEP4(0,1) };
	INT32 XOffs[16] = {
		((gfxlen/4)*0)*8+4, ((gfxlen/4)*0)*8, ((gfxlen/4)*0)*8+12, ((gfxlen/4)*0)*8+8,
		((gfxlen/4)*1)*8+4, ((gfxlen/4)*1)*8, ((gfxlen/4)*1)*8+12, ((gfxlen/4)*1)*8+8,
		((gfxlen/4)*2)*8+4, ((gfxlen/4)*2)*8, ((gfxlen/4)*2)*8+12, ((gfxlen/4)*2)*8+8,
		((gfxlen/4)*3)*8+4, ((gfxlen/4)*3)*8, ((gfxlen/4)*3)*8+12, ((gfxlen/4)*3)*8+8
	};
	INT32 YOffs[16] = { STEP16(0,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x400000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, TaitoChars, 0x400000);

	GfxDecode(0x8000, 4, 16, 16, Planes, XOffs, YOffs, 0x100, tmp, TaitoChars);

	BurnFree(tmp);

	DrvTransTab();

	return 0;
}

static INT32 CommonInit()
{
	TaitoLoadRoms(false);

	TaitoMem = NULL;
	MemIndex();
	INT32 nLen = TaitoMemEnd - (UINT8 *)0;
	if ((TaitoMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(TaitoMem, 0, nLen);
	MemIndex();

	if (TaitoLoadRoms(true)) return 1;

	DrvGfxDecode();

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Taito68KRom1,	0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(Taito68KRam1,	0x100000, 0x10ffff, MAP_RAM);
	SekMapMemory(Taito68KRam1,	0x110000, 0x11ffff, MAP_RAM);
	SekMapMemory(TaitoVideoRam,	0x400000, 0x420fff, MAP_RAM);
	SekMapMemory(TaitoPaletteRam,	0x500800, 0x500fff, MAP_RAM);
	SekSetWriteWordHandler(0,	syvalion_main_write_word);
	SekSetWriteByteHandler(0,	syvalion_main_write_byte);
	SekSetReadWordHandler(0,	syvalion_main_read_word);
	SekSetReadByteHandler(0,	syvalion_main_read_byte);

	SekMapHandler(1,		0x400000, 0x420fff, MAP_WRITE);
	SekSetWriteWordHandler(1, 	taitoh_video_write_word);
	SekSetWriteByteHandler(1, 	taitoh_video_write_byte);

	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(TaitoZ80Rom1,	0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(TaitoZ80Ram1,	0xc000, 0xdfff, MAP_RAM);
	ZetSetWriteHandler(taitoh_sound_write);
	ZetSetReadHandler(taitoh_sound_read);
	ZetClose();

	BurnYM2610Init(8000000, TaitoYM2610ARom, (INT32*)&TaitoYM2610ARomSize, TaitoYM2610BRom, (INT32*)&TaitoYM2610BRomSize, &DrvFMIRQHandler, DrvSynchroniseStream, DrvGetTime, 0);
	BurnTimerAttachZet(4000000);
	BurnYM2610SetRoute(BURN_SND_YM2610_YM2610_ROUTE_1, 0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2610SetRoute(BURN_SND_YM2610_YM2610_ROUTE_2, 0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2610SetRoute(BURN_SND_YM2610_AY8910_ROUTE, 0.25, BURN_SND_ROUTE_BOTH);

	TC0220IOCInit();
	TC0140SYTInit(0);

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static INT32 SyvalionInit()
{
	INT32 nRet = CommonInit();

	irq_config = 2;
	TaitoInputConfig = 0xc;
	address_xor = 0;

	return nRet;
}

static INT32 TetristhInit()
{
	INT32 nRet = CommonInit();

	irq_config = 2;
	TaitoInputConfig = 0xc;
	address_xor = 0x100000;

	return nRet;
}

static INT32 DleagueInit()
{
	INT32 nRet = CommonInit();

	irq_config = 1;
	TaitoInputConfig = 0;
	address_xor = 0;

	if (nRet == 0) {
		SekOpen(0);
		SekSetWriteWordHandler(0,	dleague_main_write_word);
		SekSetWriteByteHandler(0,	dleague_main_write_byte);
		SekSetReadWordHandler(0,	dleague_main_read_word);
		SekSetReadByteHandler(0,	dleague_main_read_byte);
		SekClose();
	}

	return nRet;
}

static INT32 DrvExit()
{
	SekExit();
	ZetExit();
	
	BurnYM2610Exit();

	TaitoExit();

	TaitoInputConfig = 0x0c;
	irq_config = 2;
	syvalionpmode = 0;

	return 0;
}

static void DrvPaletteUpdate()
{
	UINT16 *p = (UINT16*)TaitoPaletteRam;

	for (INT32 i = 0; i < 0x420 / 2; i++)
	{
		INT32 r = (BURN_ENDIAN_SWAP_INT16(p[i]) >>  0) & 0x1f;
		INT32 g = (BURN_ENDIAN_SWAP_INT16(p[i]) >>  5) & 0x1f;
		INT32 b = (BURN_ENDIAN_SWAP_INT16(p[i]) >> 10) & 0x1f;

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);

		TaitoPalette[i] = BurnHighCol(r, g, b, 0);
	}
}

static void copy_zoom(INT32 min_x, INT32 max_x, INT32 min_y, INT32 max_y, UINT32 startx, UINT32 starty, INT32 incxx, INT32 incyy)
{
	startx += min_x * incxx; 
	starty += min_y * incyy; 
       
	INT32 numblocks = (max_x - min_x) / 4;            

	for (INT32 cury = min_y; cury <= max_y; cury++)     
	{         
		UINT16 *destptr = pTransDraw + ((cury - min_y) * nScreenWidth);
		INT32 srcx = startx;
		INT32 srcy = starty;

		starty += incyy;    
   
		if ((UINT32)srcy < 0x4000000) 
		{     
			UINT16 *srcptr = TaitoTempBitmap[1] + (((srcy >> 16) & 0x3ff) * 0x400);
 
			for (INT32 curx = 0; curx < numblocks; curx++)   
			{ 
				if ((UINT32)srcx < 0x4000000)
					if (srcptr[srcx >> 16]) destptr[0] = srcptr[srcx >> 16];

				srcx += incxx;           

				if ((UINT32)srcx < 0x4000000)
					if (srcptr[srcx >> 16]) destptr[1] = srcptr[srcx >> 16];

				srcx += incxx;           

				if ((UINT32)srcx < 0x4000000)
					if (srcptr[srcx >> 16]) destptr[2] = srcptr[srcx >> 16];

				srcx += incxx;           

				if ((UINT32)srcx < 0x4000000)
					if (srcptr[srcx >> 16]) destptr[3] = srcptr[srcx >> 16];

				srcx += incxx;           

				destptr += 4;              
			}
		}     
	}
}

// Copy layer without zooming
static void copy_layer(INT32 layer, INT32 transp)
{
	UINT16 *src = TaitoTempBitmap[layer];
	UINT16 *dst = pTransDraw;

	transp = transp ? 0 : 0xff;
	UINT16 *base_ram = (UINT16*)TaitoVideoRam;

	INT32 scrollx = (~base_ram[(0x20802/2) + layer] + screen_x_adjust) & 0x3ff;
	INT32 scrolly = (base_ram[(0x20806/2) + layer] + screen_y_adjust) & 0x3ff;

	for (INT32 sy = 0; sy < nScreenHeight; sy++)
	{
		UINT32 yy = ((sy + scrolly) & 0x3ff) * 0x400;

		for (INT32 sx = 0; sx < nScreenWidth; sx++)
		{
			INT32 xx = (sx + scrollx) & 0x3ff;

			INT32 pxl = src[yy+xx];

			if ((pxl & 0xf) != transp)
			{
				dst[sx] = pxl;
			}
		}

		dst += nScreenWidth;
	}
}

// Foreground layer w/zoom y
static void bg1_tilemap_draw()
{
	UINT16 *scroll_ram = (UINT16*)(TaitoVideoRam + 0x20800);

	INT32 zoomx = scroll_ram[7] >> 8;
	INT32 zoomy = scroll_ram[7] & 0x00ff;

	if (zoomx == 0x3f && zoomy == 0x7f)
	{
		copy_layer(1, 1);
	}
	else 
	{
		INT32 zx, zy, dx, dy, ex, ey, sx,sy;

		INT32 min_x = screen_x_adjust;
		INT32 max_x = screen_x_adjust + (nScreenWidth) - 1;
		INT32 min_y = screen_y_adjust;
		INT32 max_y = screen_y_adjust + (nScreenHeight) - 1;

		if (zoomx < 63)
		{
			dx = 16 - (zoomx + 2) / 8;
			ex = (zoomx + 2) & 7;
			zx = ((dx << 3) - ex) << 10;
		}
		else
		{
			zx = 0x10000 - ((zoomx - 0x3f) * 256);
		}

		if (zoomy < 127)
		{
			dy = 16 - (zoomy + 2) / 16;
			ey = (zoomy + 2) & 0xf;
			zy = ((dy * 16) - ey) << 9;
		}
		else
		{
			zy = 0x10000 - ((zoomy - 0x7f) * 512);
		}

		if (!flipscreen)
		{
			sx = (-scroll_ram[2] - 1) << 16;
			sy = ( scroll_ram[4] - 1) << 16;
		}
		else
		{
			sx =  (( 0x200 + scroll_ram[2]) << 16) - (max_x + min_x) * (zx - 0x10000);
			sy =  (( 0x3fe - scroll_ram[4]) << 16) - (max_y + min_y) * (zy - 0x10000);
		}

		copy_zoom(min_x, max_x, min_y, max_y, sx, sy, zx, zy);
	}
}

// Background layer w/zoom x
static void bg0_tilemap_draw()
{
	UINT16 *scroll_ram = (UINT16*)(TaitoVideoRam + 0x20800);
	UINT16 *bgscroll_ram = (UINT16*)(TaitoVideoRam + 0x20000);

	INT32 zx = (scroll_ram[6] & 0xff00) >> 8;
	INT32 zy = scroll_ram[6] & 0x00ff;

	if (zx == 0x3f && zy == 0x7f)
	{
		copy_layer(0, 0);
	}
	else
	{
		INT32 sx, zoomx, zoomy, dx, ex, dy, ey, y_index;
	
		INT32 min_x = screen_x_adjust;
		INT32 max_x = screen_x_adjust + (nScreenWidth) - 1;
		INT32 min_y = screen_y_adjust;
		INT32 max_y = screen_y_adjust + (nScreenHeight) - 1;
		INT32 screen_width = max_x + 1;

		if (zx < 63)
		{
			dx = 16 - (zx + 2) / 8;
			ex = (zx + 2) % 8;
			zoomx = ((dx << 3) - ex) << 10;
		}
		else
		{
			zoomx = 0x10000 - ((zx - 0x3f) * 256);
		}

		if (zy < 127)
		{
			dy = 16 - (zy + 2) / 16;
			ey = (zy + 2) % 16;
			zoomy = ((dy << 4) - ey) << 9;
		}
		else
		{
			zoomy = 0x10000 - ((zy - 0x7f) * 512);
		}

		if (!flipscreen)
		{
			sx = (-scroll_ram[1] - 1) << 16;
			y_index = (( scroll_ram[3] - 1) << 16) + min_y * zoomy;
		}
		else
		{
			sx =  (( 0x200 + scroll_ram[1]) << 16) - (max_x + min_x) * (zoomx - 0x10000);
			y_index = ((-scroll_ram[3] - 2) << 16) + min_y * zoomy - (max_y + min_y) * (zoomy - 0x10000);
		}

		for (int y = min_y; y <= max_y; y++)
		{
			INT32 src_y_index = (y_index >> 16) & 0x3ff;

			INT32 row_index = (src_y_index & 0x1ff);

			if (flipscreen)
				row_index = 0x1ff - row_index;

			INT32 x_index = sx - ((bgscroll_ram[row_index] << 16));

			UINT16 *src16 = TaitoTempBitmap[0] + (src_y_index & 0x3ff) * 1024;

			INT32 x_step = zoomx;

			{
				UINT16 *dst = pTransDraw + (y - min_y) * nScreenWidth;

				int clip0 = screen_x_adjust;
				int clip1 = screen_x_adjust + (nScreenWidth);

				for (int i = 0; i < screen_width; i++)
				{
					if (i >= clip0 && i < clip1)
						dst[i - clip0] = src16[(x_index >> 16) & 0x3ff];
					x_index += x_step;
				}
			}

			y_index += zoomy;
		}
	}
}

// Check to see if a tile has changed, if it has changed, update the tilemap
static void update_layer(INT32 layer)
{
	UINT16 *ram = (UINT16*)(TaitoVideoRam + 0xc000 + (layer * 0x2000));

	for (INT32 offs = 0; offs < 64 * 64; offs++)
	{
		if (TaitoDirtyTile[offs + (layer * 0x1000)] == 0) continue;

		INT32 code  = ram[offs] & 0x7fff;

		INT32 sx = (offs & 0x3f) * 16;
		INT32 sy = (offs / 0x40) * 16;

		INT32 attr  = ram[offs + (0x10000 / 2)];
		INT32 color = attr & 0x1f;

		INT32 flipx = attr & 0x40;
		INT32 flipy = attr & 0x80;

		{
			UINT8 *gfx = TaitoChars + (code * 0x100);

			INT32 flip = (flipx ? 0xf : 0) | (flipy ? 0xf0 : 0);

			color <<= 4;

			UINT16 *dst = TaitoTempBitmap[layer] + sy * 1024 + sx;

			for (INT32 y = 0; y < 16; y++)
			{
				for (INT32 x = 0; x < 16; x++)
				{
					dst[x] = gfx[((y*16)+x)^flip] + color;					
				}

				dst += 1024;
			}
		}
	}

	memset (TaitoDirtyTile + layer * 0x1000, 0, 0x1000);
}

static void draw_tx_layer()
{
	UINT16 *ram = (UINT16*)(TaitoVideoRam + 0x01000);

	for (INT32 offs = 0; offs < 64 * 50; offs++)
	{
		if (ram[offs/2] == 0) continue;

		INT32 sx = (offs & 0x3f) * 8;
		INT32 sy = (offs / 0x40) * 8;

		INT32 code = (ram[offs/2] >> ((~offs & 1) * 8)) & 0xff;

		Render8x8Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, 0, 0, 0, 0x200, TaitoCharsB);
	}
}

static const INT32 zoomy_conv_table[0x80] = {
	0x00,0x01,0x01,0x02,0x02,0x03,0x04,0x05,0x06,0x06,0x07,0x08,0x09,0x0a,0x0a,0x0b,
	0x0b,0x0c,0x0c,0x0d,0x0e,0x0e,0x0f,0x10,0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x16,
	0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,0x20,0x21,0x22,0x24,0x25,0x26,0x27,
	0x28,0x2a,0x2b,0x2c,0x2e,0x30,0x31,0x32,0x34,0x36,0x37,0x38,0x3a,0x3c,0x3e,0x3f,
	0x40,0x41,0x42,0x42,0x43,0x43,0x44,0x44,0x45,0x45,0x46,0x46,0x47,0x47,0x48,0x49, 
	0x4a,0x4a,0x4b,0x4b,0x4c,0x4d,0x4e,0x4f,0x4f,0x50,0x51,0x51,0x52,0x53,0x54,0x55,
	0x56,0x57,0x58,0x59,0x5a,0x5b,0x5c,0x5d,0x5e,0x5f,0x60,0x61,0x62,0x63,0x64,0x66,
	0x67,0x68,0x6a,0x6b,0x6c,0x6e,0x6f,0x71,0x72,0x74,0x76,0x78,0x80,0x7b,0x7d,0x7f
};

static void syvalion_draw_sprites()
{
	INT32 x0, y0, x, y, dx, ex, zx;
	static const int size[] = { 1, 2, 4, 4 };

	UINT16 *base_ram = (UINT16*)TaitoVideoRam;

	for (INT32 offs = 0x03f8 / 2; offs >= 0; offs -= 0x008 / 2)
	{
		x0              =  base_ram[(0x20400/2) + offs + 1] & 0x3ff;
		y0              =  base_ram[(0x20400/2) + offs + 0] & 0x3ff;
		INT32 zoomx     = (base_ram[(0x20400/2) + offs + 2] & 0x7f00) >> 8;
		INT32 tile_offs = (base_ram[(0x20400/2) + offs + 3] & 0x1fff) << 2;
		INT32 ysize     = size[(base_ram[(0x20400/2) + offs] & 0x0c00) >> 10];

		if (tile_offs)
		{
			if (zoomx < 63)
			{
				dx = 8 + (zoomx + 2) / 8;
				ex = (zoomx + 2) % 8;
				zx = ((dx << 1) + ex) << 11;
			}
			else
			{
				dx = 16 + (zoomx - 63) / 4;
				ex = (zoomx - 63) & 3;
				zx = (dx + ex) << 12;
			}

			if (x0 >= 0x200) x0 -= 0x400;
			if (y0 >= 0x200) y0 -= 0x400;

			if (flipscreen)
			{
				x0 = 497 - x0;
				y0 = 498 - y0;
				dx = -dx;
			}
			else
			{
				x0 += 1;
				y0 += 2;
			}

			y = y0;

			for (INT32 j = 0; j < ysize; j++)
			{
				x = x0;

				for (INT32 k = 0; k < 4; k++)
				{
					if (tile_offs >= 0x1000)
					{
						INT32 tile  = base_ram[(0x00000/2) + tile_offs] & 0x7fff;
						INT32 color = base_ram[(0x10000/2) + tile_offs] & 0x001f;
						INT32 flipx = base_ram[(0x10000/2) + tile_offs] & 0x0040;
						INT32 flipy = base_ram[(0x10000/2) + tile_offs] & 0x0080;

						if (flipscreen)
						{
							flipx ^= 0x0040;
							flipy ^= 0x0080;
						}

						if (transparent_tile_lut[tile] == 0)
							RenderZoomedTile(pTransDraw, TaitoChars, tile, color*16, 0, x - screen_x_adjust, y - screen_y_adjust, flipx, flipy, 16, 16, zx, zx);
					}

					tile_offs ++;
					x += dx;
				}

				y += dx;
			}
		}
	}
}

static void recordbr_draw_sprites(INT32 priority)
{
	static const int size[] = { 1, 2, 4, 4 };
	int x0, y0, x, y, dx, dy, ex, ey, zx, zy;
	int ysize;
	int j, k;
	int offs;
	int tile_offs;
	int zoomx, zoomy;

	UINT16 *base_ram = (UINT16*)TaitoVideoRam;

	for (offs = 0x03f8 / 2; offs >= 0; offs -= 0x008 / 2)
	{
		if (offs <  0x01b0 && priority == 0)    continue;
		if (offs >= 0x01b0 && priority == 1)    continue;

		x0        =  base_ram[(0x20400/2) + offs + 1] & 0x3ff;
		y0        =  base_ram[(0x20400/2) + offs + 0] & 0x3ff;
		zoomx     = (base_ram[(0x20400/2) + offs + 2] & 0x7f00) >> 8;
		zoomy     = (base_ram[(0x20400/2) + offs + 2] & 0x007f);
		tile_offs = (base_ram[(0x20400/2) + offs + 3] & 0x1fff) << 2;
		ysize     = size[(base_ram[(0x20400/2) + offs] & 0x0c00) >> 10];

		if (tile_offs)
		{
			zoomy = zoomy_conv_table[zoomy];

			if (zoomx < 63)
			{
				dx = 8 + (zoomx + 2) / 8;
				ex = (zoomx + 2) % 8;
				zx = ((dx << 1) + ex) << 11;
			}
			else
			{
				dx = 16 + (zoomx - 63) / 4;
				ex = (zoomx - 63) % 4;
				zx = (dx + ex) << 12;
			}

			if (zoomy < 63)
			{
				dy = 8 + (zoomy + 2) / 8;
				ey = (zoomy + 2) % 8;
				zy = ((dy << 1) + ey) << 11;
			}
			else
			{
				dy = 16 + (zoomy - 63) / 4;
				ey = (zoomy - 63) % 4;
				zy = (dy + ey) << 12;
			}

			if (x0 >= 0x200) x0 -= 0x400;
			if (y0 >= 0x200) y0 -= 0x400;

			if (flipscreen)
			{
				x0 = 497 - x0;
				y0 = 498 - y0;
				dx = -dx;
				dy = -dy;
			}
			else
			{
				x0 += 1;
				y0 += 2;
			}

			y = y0;
			for (j = 0; j < ysize; j ++)
			{
				x = x0;
				for (k = 0; k < 4; k ++)
				{
					if (tile_offs >= 0x1000)
					{
						INT32 tile  = base_ram[(0x00000/2) + tile_offs] & 0x7fff;
						INT32 color = base_ram[(0x10000/2) + tile_offs] & 0x001f;
						INT32 flipx = base_ram[(0x10000/2) + tile_offs] & 0x0040;
						INT32 flipy = base_ram[(0x10000/2) + tile_offs] & 0x0080;

						if (flipscreen)
						{
							flipx ^= 0x0040;
							flipy ^= 0x0080;
						}

						if (transparent_tile_lut[tile] == 0)
							RenderZoomedTile(pTransDraw, TaitoChars, tile, color*16, 0, x - screen_x_adjust, y - screen_y_adjust, flipx, flipy, 16, 16, zx, zy);
					}
					tile_offs ++;
					x += dx;
				}
				y += dy;
			}
		}
	}
}

static void dleague_draw_sprites(INT32 priority)
{
	UINT16 *base_ram = (UINT16*)TaitoVideoRam;

	static const int size[] = { 1, 2, 4, 4 };
	int x0, y0, x, y, dx, ex, zx;
	int ysize;
	int j, k;
	int offs;
	int tile_offs;
	int zoomx;
	int pribit;

	for (offs = 0x03f8 / 2; offs >= 0; offs -= 0x008 / 2)
	{
		x0        =  base_ram[(0x20400/2) + offs + 1] & 0x3ff;
		y0        =  base_ram[(0x20400/2) + offs + 0] & 0x3ff;
		zoomx     = (base_ram[(0x20400/2) + offs + 2] & 0x7f00) >> 8;
		tile_offs = (base_ram[(0x20400/2) + offs + 3] & 0x1fff) << 2;
		pribit    = (base_ram[(0x20400/2) + offs + 0] & 0x1000) >> 12;
		ysize     = size[(base_ram[(0x20400/2) + offs] & 0x0c00) >> 10];

		if (tile_offs)
		{
			if (zoomx < 63)
			{
				dx = 8 + (zoomx + 2) / 8;
				ex = (zoomx + 2) % 8;
				zx = ((dx << 1) + ex) << 11;
				pribit = 0;
			}
			else
			{
				dx = 16 + (zoomx - 63) / 4;
				ex = (zoomx - 63) % 4;
				zx = (dx + ex) << 12;
			}

			if (base_ram[0x20802/2] & 0x8000)
				pribit = 1;

			if (x0 >= 0x200) x0 -= 0x400;
			if (y0 >= 0x200) y0 -= 0x400;

			if (flipscreen)
			{
				x0 = 497 - x0;
				y0 = 498 - y0;
				dx = -dx;
			}
			else
			{
				x0 += 1;
				y0 += 2;
			}

			if (priority == pribit)
			{
				y = y0;
				for (j = 0; j < ysize; j ++)
				{
					x = x0;
					for (k = 0; k < 4; k ++ )
					{
						if (tile_offs >= 0x1000)    /* or pitcher gets blanked */
						{
							INT32 tile  = base_ram[(0x00000/2) + tile_offs] & 0x7fff;
							INT32 color = base_ram[(0x10000/2) + tile_offs] & 0x001f;
							INT32 flipx = base_ram[(0x10000/2) + tile_offs] & 0x0040;
							INT32 flipy = base_ram[(0x10000/2) + tile_offs] & 0x0080;

							if (flipscreen)
							{
								flipx ^= 0x0040;
								flipy ^= 0x0080;
							}

							if (transparent_tile_lut[tile] == 0)
								RenderZoomedTile(pTransDraw, TaitoChars, tile, color*16, 0, x - screen_x_adjust, y - screen_y_adjust, flipx, flipy, 16, 16, zx, zx);
						}
						tile_offs ++;
						x += dx;
					}
					y += dx;
				}
			}
		}
	}
}

static INT32 SyvalionDraw()
{
	screen_y_adjust = 32+16;
	screen_x_adjust = 0;

	update_layer(0);
	update_layer(1);

	DrvPaletteUpdate();

	UINT16 *base_ram = (UINT16*)TaitoVideoRam;

	flipscreen = base_ram[0x20800/2] & 0x0c00;

	BurnTransferClear();

	if (nBurnLayer & 1) bg0_tilemap_draw();
	if (nBurnLayer & 2) bg1_tilemap_draw();

	if (nSpriteEnable & 1) syvalion_draw_sprites();

	if (nBurnLayer & 4) draw_tx_layer();

	BurnTransferCopy(TaitoPalette);

	return 0;
}

static INT32 RecordbrDraw()
{
	screen_y_adjust = 32;
	screen_x_adjust = 16;

	update_layer(0);
	update_layer(1);

	DrvPaletteUpdate();

	UINT16 *base_ram = (UINT16*)TaitoVideoRam;

	flipscreen = base_ram[0x20800/2] & 0x0c00;

	BurnTransferClear();

	if (nBurnLayer & 1) bg0_tilemap_draw();

	recordbr_draw_sprites(0);

	if (nBurnLayer & 2) bg1_tilemap_draw();

	recordbr_draw_sprites(1);

	if (nBurnLayer & 4) draw_tx_layer();

	BurnTransferCopy(TaitoPalette);

	return 0;
}

static INT32 DleagueDraw()
{
	screen_y_adjust = 32;
	screen_x_adjust = 16;

	update_layer(0);
	update_layer(1);

	DrvPaletteUpdate();

	UINT16 *base_ram = (UINT16*)TaitoVideoRam;

	flipscreen = base_ram[0x20800/2] & 0x0c00;

	BurnTransferClear();

	if (nBurnLayer & 1) bg0_tilemap_draw();

	dleague_draw_sprites(0);

	if (nBurnLayer & 2) bg1_tilemap_draw();

	dleague_draw_sprites(1);

	if (nBurnLayer & 4) draw_tx_layer();

	BurnTransferCopy(TaitoPalette);

	return 0;
}

static INT32 DleagueJDraw() /* kludge */
{
	screen_y_adjust = 32;
	screen_x_adjust = 16;

	update_layer(0);
	update_layer(1);

	DrvPaletteUpdate();

	UINT16 *base_ram = (UINT16*)TaitoVideoRam;

	flipscreen = base_ram[0x20800/2] & 0x0c00;

	BurnTransferClear();

	if (nBurnLayer & 1) bg0_tilemap_draw();

	if (nBurnLayer & 2) bg1_tilemap_draw();

	dleague_draw_sprites(0);
	dleague_draw_sprites(1);

	if (nBurnLayer & 4) draw_tx_layer();

	BurnTransferCopy(TaitoPalette);

	return 0;
}

static INT32 DrvFrame()
{
	TaitoWatchdog++;
	if (TaitoWatchdog >= 180) {
		DrvDoReset(0);
	}

	if (TaitoReset) {
		DrvDoReset(1);
	}

	SekNewFrame();
	ZetNewFrame();

	{
		memset (TC0220IOCInput, 0xff, sizeof ( TC0220IOCInput ));

		TC0220IOCInput[0] &= ~TaitoInputConfig;

		for (INT32 i = 0; i < 8; i++) {
			TC0220IOCInput[0] ^= (TC0220IOCInputPort0[i] & 1) << i;
			TC0220IOCInput[1] ^= (TC0220IOCInputPort1[i] & 1) << i;
			TC0220IOCInput[2] ^= (TC0220IOCInputPort2[i] & 1) << i;
		}
	}

	SekOpen(0);

	INT32 SekSpeed = (INT32)((INT64)12000000 * nBurnCPUSpeedAdjust / 0x100);
	INT32 ZetSpeed = (INT32)((INT64)4000000 * nBurnCPUSpeedAdjust / 0x100);

	INT32 nInterleave = 100;
	INT32 nCyclesTotal[2] = { SekSpeed / 60, ZetSpeed / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };
	INT32 nNext[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++) {
		nNext[0] += nCyclesTotal[0] / nInterleave;
		nCyclesDone[0] += SekRun(nNext[0] - nCyclesDone[0]);
		if (i == (nInterleave / 1) - 1) SekSetIRQLine(irq_config, CPU_IRQSTATUS_AUTO);
		nNext[1] += nCyclesTotal[1] / nInterleave;

		ZetOpen(0);
		BurnTimerUpdate(nNext[1]);
		nCyclesDone[1] += nNext[1];
		ZetClose();
	}

	ZetOpen(0);
	BurnTimerEndFrame(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		BurnYM2610Update(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	SekClose();
	
	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static void expand_tiles()
{
	for (INT32 i = 0; i < 0x1000; i+=2)
	{
		for (INT32 j = 0; j < 8; j++)
		{
			INT32 pxl;

			pxl  = ((TaitoVideoRam[(i+0x00000)] >> j) & 1) << 0;
			pxl |= ((TaitoVideoRam[(i+0x00001)] >> j) & 1) << 1;
			pxl |= ((TaitoVideoRam[(i+0x10000)] >> j) & 1) << 2;

			TaitoCharsB[(i << 2) + j] = pxl;
		}
	}
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin =  0x029702;
	}

	if (nAction & ACB_DRIVER_DATA) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = TaitoRamStart;
		ba.nLen	  = TaitoRamEnd-TaitoRamStart;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		SekScan(nAction);
		ZetScan(nAction);

		BurnYM2610Scan(nAction, pnMin);

		TaitoICScan(nAction);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		bankswitch(TaitoZ80Bank);
		ZetClose();

		expand_tiles();

		memset (TaitoDirtyTile, 1, 0x2000);
	}

 	return 0;
}



// Syvalion (Japan)

static struct BurnRomInfo syvalionRomDesc[] = {
	{ "b51-20.bin",		0x20000, 0x440b6418, TAITO_68KROM1_BYTESWAP }, //  0 68k Code
	{ "b51-22.bin",		0x20000, 0xe6c61079, TAITO_68KROM1_BYTESWAP }, //  1
	{ "b51-19.bin",		0x20000, 0x2abd762c, TAITO_68KROM1_BYTESWAP }, //  2
	{ "b51-21.bin",		0x20000, 0xaa111f30, TAITO_68KROM1_BYTESWAP }, //  3

	{ "b51-23.bin",		0x10000, 0x734662de, TAITO_Z80ROM1 },          //  4 Z80 Code

	{ "b51-16.bin",		0x20000, 0xc0fcf7a5, TAITO_CHARS_BYTESWAP },   //  5 Background Tiles & Sprites
	{ "b51-12.bin",		0x20000, 0x6b36d358, TAITO_CHARS_BYTESWAP },   //  6
	{ "b51-15.bin",		0x20000, 0x30b2ee02, TAITO_CHARS_BYTESWAP },   //  7
	{ "b51-11.bin",		0x20000, 0xae9a9ac5, TAITO_CHARS_BYTESWAP },   //  8
	{ "b51-08.bin",		0x20000, 0x9f6a535c, TAITO_CHARS_BYTESWAP },   //  9
	{ "b51-04.bin",		0x20000, 0x03aea658, TAITO_CHARS_BYTESWAP },   // 10
	{ "b51-07.bin",		0x20000, 0x764d4dc8, TAITO_CHARS_BYTESWAP },   // 11
	{ "b51-03.bin",		0x20000, 0x8fd9b299, TAITO_CHARS_BYTESWAP },   // 12
	{ "b51-14.bin",		0x20000, 0xdea7216e, TAITO_CHARS_BYTESWAP },   // 13
	{ "b51-10.bin",		0x20000, 0x6aa97fbc, TAITO_CHARS_BYTESWAP },   // 14
	{ "b51-13.bin",		0x20000, 0xdab28958, TAITO_CHARS_BYTESWAP },   // 15
	{ "b51-09.bin",		0x20000, 0xcbb4f33d, TAITO_CHARS_BYTESWAP },   // 16
	{ "b51-06.bin",		0x20000, 0x81bef4f0, TAITO_CHARS_BYTESWAP },   // 17
	{ "b51-02.bin",		0x20000, 0x906ba440, TAITO_CHARS_BYTESWAP },   // 18
	{ "b51-05.bin",		0x20000, 0x47976ae9, TAITO_CHARS_BYTESWAP },   // 19
	{ "b51-01.bin",		0x20000, 0x8dab004a, TAITO_CHARS_BYTESWAP },   // 20

	{ "b51-17.bin",		0x80000, 0xd85096aa, TAITO_YM2610A },          // 21 YM2610 Samples

	{ "b51-18.bin",		0x80000, 0x8b23ac83, TAITO_YM2610B },          // 22 YM2610 DeltaT Region

};

STD_ROM_PICK(syvalion)
STD_ROM_FN(syvalion)

struct BurnDriver BurnDrvSyvalion = {
	"syvalion", NULL, NULL, NULL, "1988",
	"Syvalion (Japan)\0", NULL, "Taito Corporation", "H System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_HORSHOOT, 0,
	NULL, syvalionRomInfo, syvalionRomName, NULL, NULL, SyvalionInputInfo, SyvalionDIPInfo,
	SyvalionInit, DrvExit, DrvFrame, SyvalionDraw, DrvScan, NULL, 0x210,
	512, 400, 4, 3
};


// Syvalion (World, prototype)

static struct BurnRomInfo syvalionpRomDesc[] = {
	{ "prg-1e.ic28",	0x20000, 0xc778005b, TAITO_68KROM1_BYTESWAP }, //  0 68k Code
	{ "prg-0e.ic31",	0x20000, 0x5a484040, TAITO_68KROM1_BYTESWAP }, //  1
	{ "prg-3e.ic27",	0x20000, 0x0babb15b, TAITO_68KROM1_BYTESWAP }, //  2
	{ "prg-2e.ic30",	0x20000, 0xf4aacaa9, TAITO_68KROM1_BYTESWAP }, //  3

	{ "c69b.ic58",		0x10000, 0x07d3d789, TAITO_Z80ROM1 },          //  4 Z80 Code

	{ "chr-00.ic16",	0x20000, 0xb0c66db7, TAITO_CHARS_BYTESWAP },   //  5 Background Tiles & Sprites
	{ "chr-01.ic12",	0x20000, 0xdd07db12, TAITO_CHARS_BYTESWAP },   //  6
	{ "chr-10.ic15",	0x20000, 0xc8942dde, TAITO_CHARS_BYTESWAP },   //  7
	{ "chr-11.ic11",	0x20000, 0xfdaa72f5, TAITO_CHARS_BYTESWAP },   //  8
	{ "chr-02.ic8",		0x20000, 0x323a9ad9, TAITO_CHARS_BYTESWAP },   //  9
	{ "chr-03.ic4",		0x20000, 0x5ab28400, TAITO_CHARS_BYTESWAP },   // 10
	{ "chr-12.ic7",		0x20000, 0x094a5e0b, TAITO_CHARS_BYTESWAP },   // 11
	{ "chr-13.ic3",		0x20000, 0xcf39cf1d, TAITO_CHARS_BYTESWAP },   // 12
	{ "chr-04.ic14",	0x20000, 0xdd2ea978, TAITO_CHARS_BYTESWAP },   // 13
	{ "chr-05.ic10",	0x20000, 0x1c305d4e, TAITO_CHARS_BYTESWAP },   // 14
	{ "chr-14.ic13",	0x20000, 0x083806c3, TAITO_CHARS_BYTESWAP },   // 15
	{ "chr-15.ic9",		0x20000, 0x6afb076e, TAITO_CHARS_BYTESWAP },   // 16
	{ "chr-06.ic6",		0x20000, 0x00cd0493, TAITO_CHARS_BYTESWAP },   // 17
	{ "chr-07.ic2",		0x20000, 0x58fb0f65, TAITO_CHARS_BYTESWAP },   // 18
	{ "chr-16.ic5",		0x20000, 0xa169194e, TAITO_CHARS_BYTESWAP },   // 19
	{ "chr-17.ic7",		0x20000, 0xc259bd61, TAITO_CHARS_BYTESWAP },   // 20

	{ "sa-00.ic1",		0x20000, 0x27a97abc, TAITO_YM2610A },          // 21 YM2610 Samples
	{ "sa-01.ic2",		0x20000, 0x0140452b, TAITO_YM2610A },          // 22
	{ "sa-02.ic3",		0x20000, 0x970cd4ee, TAITO_YM2610A },          // 23
	{ "sa-03.ic4",		0x20000, 0x936cd1b5, TAITO_YM2610A },          // 24

	{ "sb-00.ic6",		0x20000, 0x5188f459, TAITO_YM2610B },          // 25 YM2610 DeltaT Region
	{ "sb01.ic8",		0x20000, 0x4dab7a6b, TAITO_YM2610B },          // 26
	{ "sb-02.ic7",		0x20000, 0x8f5cc936, TAITO_YM2610B },          // 27
	{ "sb-03-e66a.ic9",	0x20000, 0x9013b407, TAITO_YM2610B },          // 28

	{ "cpu1-pal20l10a.ic38.bin",	0x000cc, 0x2e7b5e3f, 0 },              // 29 pals
	{ "cpu2-pal20l8a.ic39.bin",	0x00144, 0xc0abf131, 0 },              // 30
};

STD_ROM_PICK(syvalionp)
STD_ROM_FN(syvalionp)

static INT32 SyvalionpInit()
{
	syvalionpmode = 1;

	return SyvalionInit();
}

struct BurnDriver BurnDrvSyvalionp = {
	"syvalionp", "syvalion", NULL, NULL, "1988",
	"Syvalion (World, prototype)\0", NULL, "Taito Corporation", "H System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_HORSHOOT, 0,
	NULL, syvalionpRomInfo, syvalionpRomName, NULL, NULL, SyvalionInputInfo, SyvalionDIPInfo,
	SyvalionpInit, DrvExit, DrvFrame, SyvalionDraw, DrvScan, NULL, 0x210,
	512, 400, 4, 3
};


// Recordbreaker (World)

static struct BurnRomInfo recordbrRomDesc[] = {
	{ "b56-17.bin",		0x20000, 0x3e0a9c35, TAITO_68KROM1_BYTESWAP }, //  0 68k Code
	{ "b56-16.bin",		0x20000, 0xb447f12c, TAITO_68KROM1_BYTESWAP }, //  1
	{ "b56-15.bin",		0x20000, 0xb346e282, TAITO_68KROM1_BYTESWAP }, //  2
	{ "b56-21.bin",		0x20000, 0xe5f63790, TAITO_68KROM1_BYTESWAP }, //  3

	{ "b56-19.bin",		0x10000, 0xc68085ee, TAITO_Z80ROM1 },          //  4 Z80 Code

	{ "b56-04.bin",		0x20000, 0xf7afdff0, TAITO_CHARS_BYTESWAP },   //  5 Background Tiles & Sprites
	{ "b56-08.bin",		0x20000, 0xc9f0d38a, TAITO_CHARS_BYTESWAP },   //  6
	{ "b56-03.bin",		0x20000, 0x4045fd44, TAITO_CHARS_BYTESWAP },   //  7
	{ "b56-07.bin",		0x20000, 0x0c76e4c8, TAITO_CHARS_BYTESWAP },   //  8
	{ "b56-02.bin",		0x20000, 0x68c604ec, TAITO_CHARS_BYTESWAP },   //  9
	{ "b56-06.bin",		0x20000, 0x5fbcd302, TAITO_CHARS_BYTESWAP },   // 10
	{ "b56-01.bin",		0x20000, 0x766b7260, TAITO_CHARS_BYTESWAP },   // 11
	{ "b56-05.bin",		0x20000, 0xed390378, TAITO_CHARS_BYTESWAP },   // 12

	{ "b56-10.bin",		0x80000, 0xde1bce59, TAITO_YM2610A },          // 14 YM2610 Samples

	{ "b56-09.bin",		0x80000, 0x7fd9ee68, TAITO_YM2610B },          // 13 YM2610 DeltaT Region

	{ "b56-18.bin",		0x02000, 0xc88f0bbe, 0 },                      // 15 ?
};

STD_ROM_PICK(recordbr)
STD_ROM_FN(recordbr)

struct BurnDriver BurnDrvRecordbr = {
	"recordbr", NULL, NULL, NULL, "1988",
	"Recordbreaker (World)\0", NULL, "Taito Corporation Japan", "H System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_SPORTSMISC, 0,
	NULL, recordbrRomInfo, recordbrRomName, NULL, NULL, RecordbrInputInfo, RecordbrDIPInfo,
	SyvalionInit, DrvExit, DrvFrame, RecordbrDraw, NULL, NULL, 0x210,
	320, 240, 4, 3
};


// Go For The Gold (Japan)

static struct BurnRomInfo gogoldRomDesc[] = {
	{ "b56-17.bin",		0x20000, 0x3e0a9c35, TAITO_68KROM1_BYTESWAP }, //  0 68k Code
	{ "b56-16.bin",		0x20000, 0xb447f12c, TAITO_68KROM1_BYTESWAP }, //  1
	{ "b56-15.bin",		0x20000, 0xb346e282, TAITO_68KROM1_BYTESWAP }, //  2
	{ "b56-14.bin",		0x20000, 0xb6c195b9, TAITO_68KROM1_BYTESWAP }, //  3

	{ "b56-19.bin",		0x10000, 0xc68085ee, TAITO_Z80ROM1 },          //  4 Z80 Code

	{ "b56-04.bin",		0x20000, 0xf7afdff0, TAITO_CHARS_BYTESWAP },   //  5 Background Tiles & Sprites
	{ "b56-08.bin",		0x20000, 0xc9f0d38a, TAITO_CHARS_BYTESWAP },   //  6
	{ "b56-03.bin",		0x20000, 0x4045fd44, TAITO_CHARS_BYTESWAP },   //  7
	{ "b56-07.bin",		0x20000, 0x0c76e4c8, TAITO_CHARS_BYTESWAP },   //  8
	{ "b56-02.bin",		0x20000, 0x68c604ec, TAITO_CHARS_BYTESWAP },   //  9
	{ "b56-06.bin",		0x20000, 0x5fbcd302, TAITO_CHARS_BYTESWAP },   // 10
	{ "b56-01.bin",		0x20000, 0x766b7260, TAITO_CHARS_BYTESWAP },   // 11
	{ "b56-05.bin",		0x20000, 0xed390378, TAITO_CHARS_BYTESWAP },   // 12

	{ "b56-10.bin",		0x80000, 0xde1bce59, TAITO_YM2610A },          // 13 YM2610 Samples

	{ "b56-09.bin",		0x80000, 0x7fd9ee68, TAITO_YM2610B },          // 14 YM2610 DeltaT Region

	{ "b56-18.bin",		0x02000, 0xc88f0bbe, 0 },                      // 15 ?
};

STD_ROM_PICK(gogold)
STD_ROM_FN(gogold)

struct BurnDriver BurnDrvGogold = {
	"gogold", "recordbr", NULL, NULL, "1988",
	"Go For The Gold (Japan)\0", NULL, "Taito Corporation", "H System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_SPORTSMISC, 0,
	NULL, gogoldRomInfo, gogoldRomName, NULL, NULL, RecordbrInputInfo, GogoldDIPInfo,
	SyvalionInit, DrvExit, DrvFrame, RecordbrDraw, NULL, NULL, 0x210,
	320, 240, 4, 3
};


// Tetris (Japan, Taito H-System)

static struct BurnRomInfo tetristhRomDesc[] = {
	{ "c26-12-1.ic36",	0x20000, 0x77e80c82, TAITO_68KROM1_BYTESWAP }, //  0 68k Code
	{ "c26-11-1.ic18",	0x20000, 0x069d77d2, TAITO_68KROM1_BYTESWAP }, //  1

	{ "c26-13.ic56",	0x10000, 0xefa89dfa, TAITO_Z80ROM1 },          //  2 Z80 Code

	{ "c26-04.ic51",	0x20000, 0x23ddf00f, TAITO_CHARS_BYTESWAP },   //  3 Background Tiles & Sprites
	{ "c26-08.ic65",	0x20000, 0x86071824, TAITO_CHARS_BYTESWAP },   //  4
	{ "c26-03.ic50",	0x20000, 0x341be9ac, TAITO_CHARS_BYTESWAP },   //  5
	{ "c26-07.ic64",	0x20000, 0xc236311f, TAITO_CHARS_BYTESWAP },   //  6
	{ "c26-02.ic49",	0x20000, 0x0b0bc88f, TAITO_CHARS_BYTESWAP },   //  7
	{ "c26-06.ic63",	0x20000, 0xdeae0394, TAITO_CHARS_BYTESWAP },   //  8
	{ "c26-01.ic48",	0x20000, 0x7efc7311, TAITO_CHARS_BYTESWAP },   //  9
	{ "c26-05.ic62",	0x20000, 0x12718d97, TAITO_CHARS_BYTESWAP },   // 10

	{ "b56-10.bin",		0x80000, 0xde1bce59, TAITO_YM2610A },          // 11 YM2610 Samples

	{ "b56-09.bin",		0x80000, 0x7fd9ee68, TAITO_YM2610B },          // 12 YM2610 DeltaT Region

	{ "b56-18.bin",		0x02000, 0xc88f0bbe, 0 },         	       // 13 ?
};

STD_ROM_PICK(tetristh)
STD_ROM_FN(tetristh)

struct BurnDriver BurnDrvTetristh = {
	"tetristh", "tetris", NULL, NULL, "1988",
	"Tetris (Japan, Taito H-System)\0", NULL, "Sega", "H System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_PUZZLE, 0,
	NULL, tetristhRomInfo, tetristhRomName, NULL, NULL, TetristhInputInfo, TetristhDIPInfo,
	TetristhInit, DrvExit, DrvFrame, RecordbrDraw, NULL, NULL, 0x210,
	320, 224, 4, 3
};



// Dynamite League (US)

static struct BurnRomInfo dleagueRomDesc[] = {
	{ "c02-xx.33",		0x20000, 0xeda870a7, TAITO_68KROM1_BYTESWAP }, //  0 68k Code
	{ "c02-xx.36",		0x20000, 0xf52114af, TAITO_68KROM1_BYTESWAP }, //  1
	{ "c02-20.34",		0x10000, 0xcdf593f3, TAITO_68KROM1_BYTESWAP }, //  2
	{ "c02-xx.37",		0x10000, 0x820a8241, TAITO_68KROM1_BYTESWAP }, //  3

	{ "c02-23.40",		0x10000, 0x5632ee49, TAITO_Z80ROM1 },          //  4 Z80 Code

	{ "c02-02.15",		0x80000, 0xb273f854, TAITO_CHARS },            //  5 Background Tiles & Sprites
	{ "c02-06.6",		0x20000, 0xb8473c7b, TAITO_CHARS_BYTESWAP },   //  6
	{ "c02-10.14",		0x20000, 0x50c02f0f, TAITO_CHARS_BYTESWAP },   //  7
	{ "c02-03.17",		0x80000, 0xc3fd0dcd, TAITO_CHARS },            //  8
	{ "c02-07.7",		0x20000, 0x8c1e3296, TAITO_CHARS_BYTESWAP },   //  9
	{ "c02-11.16",		0x20000, 0xfbe548b8, TAITO_CHARS_BYTESWAP },   // 10
	{ "c02-24.19",		0x80000, 0x18ef740a, TAITO_CHARS },            // 11
	{ "c02-08.8",		0x20000, 0x1a3c2f93, TAITO_CHARS_BYTESWAP },   // 12
	{ "c02-12.18",		0x20000, 0xb1c151c5, TAITO_CHARS_BYTESWAP },   // 13
	{ "c02-05.21",		0x80000, 0xfe3a5179, TAITO_CHARS },            // 14
	{ "c02-09.9",		0x20000, 0xa614d234, TAITO_CHARS_BYTESWAP },   // 15
	{ "c02-13.20",		0x20000, 0x8eb3194d, TAITO_CHARS_BYTESWAP },   // 16

	{ "c02-01.31",		0x80000, 0xd5a3d1aa, TAITO_YM2610A },          // 17 YM2610 Samples

	{ "c02-18.22",		0x02000, 0xc88f0bbe, 0 },                      // 18 ?
};

STD_ROM_PICK(dleague)
STD_ROM_FN(dleague)

struct BurnDriver BurnDrvDleague = {
	"dleague", NULL, NULL, NULL, "1990",
	"Dynamite League (US)\0", NULL, "Taito America Corporation", "H System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_SPORTSMISC, 0,
	NULL, dleagueRomInfo, dleagueRomName, NULL, NULL, DleagueInputInfo, DleagueDIPInfo,
	DleagueInit, DrvExit, DrvFrame, DleagueDraw, NULL, NULL, 0x210,
	320, 240, 4, 3
};


// Dynamite League (Japan)

static struct BurnRomInfo dleaguejRomDesc[] = {
	{ "c02-19a.33",		0x20000, 0x7e904e45, TAITO_68KROM1_BYTESWAP }, //  0 68k Code
	{ "c02-21a.36",		0x20000, 0x18c8a32b, TAITO_68KROM1_BYTESWAP }, //  1
	{ "c02-20.34",		0x10000, 0xcdf593f3, TAITO_68KROM1_BYTESWAP }, //  2
	{ "c02-22.37",		0x10000, 0xf50db2d7, TAITO_68KROM1_BYTESWAP }, //  3

	{ "c02-23.40",		0x10000, 0x5632ee49, TAITO_Z80ROM1 },          //  4 Z80 Code

	{ "c02-02.15",		0x80000, 0xb273f854, TAITO_CHARS },            //  5 Background Tiles & Sprites
	{ "c02-06.6",		0x20000, 0xb8473c7b, TAITO_CHARS_BYTESWAP },   //  6
	{ "c02-10.14",		0x20000, 0x50c02f0f, TAITO_CHARS_BYTESWAP },   //  7
	{ "c02-03.17",		0x80000, 0xc3fd0dcd, TAITO_CHARS },            //  8
	{ "c02-07.7",		0x20000, 0x8c1e3296, TAITO_CHARS_BYTESWAP },   //  9
	{ "c02-11.16",		0x20000, 0xfbe548b8, TAITO_CHARS_BYTESWAP },   // 10
	{ "c02-24.19",		0x80000, 0x18ef740a, TAITO_CHARS },            // 11
	{ "c02-08.8",		0x20000, 0x1a3c2f93, TAITO_CHARS_BYTESWAP },   // 12
	{ "c02-12.18",		0x20000, 0xb1c151c5, TAITO_CHARS_BYTESWAP },   // 13
	{ "c02-05.21",		0x80000, 0xfe3a5179, TAITO_CHARS },            // 14
	{ "c02-09.9",		0x20000, 0xa614d234, TAITO_CHARS_BYTESWAP },   // 15
	{ "c02-13.20",		0x20000, 0x8eb3194d, TAITO_CHARS_BYTESWAP },   // 16

	{ "c02-01.31",		0x80000, 0xd5a3d1aa, TAITO_YM2610A },          // 17 YM2610 Samples

	{ "c02-18.22",		0x02000, 0xc88f0bbe, 0 },                      // 18 ?
};

STD_ROM_PICK(dleaguej)
STD_ROM_FN(dleaguej)

struct BurnDriver BurnDrvDleaguej = {
	"dleaguej", "dleague", NULL, NULL, "1990",
	"Dynamite League (Japan)\0", NULL, "Taito Corporation", "H System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_SPORTSMISC, 0,
	NULL, dleaguejRomInfo, dleaguejRomName, NULL, NULL, DleagueInputInfo, DleaguejDIPInfo,
	DleagueInit, DrvExit, DrvFrame, DleagueJDraw, NULL, NULL, 0x210,
	320, 240, 4, 3
};
