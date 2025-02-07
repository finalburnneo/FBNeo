// Millipede emu-layer for FB Neo by dink, inspired by Ivan Mackintosh's Millipede/Centipede emulator

#include "tiles_generic.h"
#include "m6502_intf.h"
#include "burn_gun.h"
#include "pokey.h"
#include "earom.h"
#include "watchdog.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv6502ROM;
static UINT8 *Drv6502RAM;
static UINT8 *DrvColPROM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvSpriteRAM;

static UINT8 *DrvBGGFX;
static UINT8 *DrvSpriteGFX;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 dip_select;
static UINT8 control_select;
static UINT32 flipscreen = 0;
static UINT32 vblank;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvJoyF[8];
static UINT8 DrvDip[6] = {0, 0, 0, 0, 0, 0};
static UINT8 DrvInput[5];
static UINT8 DrvReset;

static INT16 Analog[4];

static INT32 centipedemode = 0;
static INT32 warlordsmode = 0;
static INT32 mazeinvmode = 0;

static INT32 startframe = 0;

// partial updates
static INT32 lastline = 0;
static INT32 scanline = 0;

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo MillipedInputList[] = {
	{"P1 Coin",		    BIT_DIGITAL,	DrvJoy3 + 5,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 start"	},
	{"P1 Up",		    BIT_DIGITAL,	DrvJoy3 + 3,	"p1 up"		},
	{"P1 Down",		    BIT_DIGITAL,	DrvJoy3 + 2,	"p1 down"	},
	{"P1 Left",		    BIT_DIGITAL,	DrvJoy3 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	A("P1 Trackball X", BIT_ANALOG_REL, &Analog[0],		"p1 x-axis"),
	A("P1 Trackball Y", BIT_ANALOG_REL, &Analog[1],		"p1 y-axis"),

	{"P2 Coin",		    BIT_DIGITAL,	DrvJoy3 + 6,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 start"	},
	{"P2 Up",		    BIT_DIGITAL,	DrvJoy4 + 2,	"p2 up"		},
	{"P2 Down",		    BIT_DIGITAL,	DrvJoy4 + 3,	"p2 down"	},
	{"P2 Left",		    BIT_DIGITAL,	DrvJoy4 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"},
	A("P2 Trackball X", BIT_ANALOG_REL, &Analog[2],		"p2 x-axis"),
	A("P2 Trackball Y", BIT_ANALOG_REL, &Analog[3],		"p2 y-axis"),

	{"Reset",		    BIT_DIGITAL,	&DrvReset,	    "reset"		},
	{"Service",		    BIT_DIGITAL,	DrvJoy3 + 7,	"service"	},
	{"Tilt",		    BIT_DIGITAL,	DrvJoy3 + 4,	"tilt"		},
	{"Dip A",		    BIT_DIPSWITCH,	DrvDip + 0,	    "dip"		},
	{"Dip B",		    BIT_DIPSWITCH,	DrvDip + 1,	    "dip"		},
	{"Dip C",		    BIT_DIPSWITCH,	DrvDip + 2,	    "dip"		},
	{"Dip D",		    BIT_DIPSWITCH,	DrvDip + 3,	    "dip"		},
	{"Dip E",		    BIT_DIPSWITCH,	DrvDip + 4,	    "dip"		},
};

STDINPUTINFO(Milliped)

static struct BurnInputInfo CentipedInputList[] = {
	{"P1 Coin",		    BIT_DIGITAL,	DrvJoy2 + 5,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 start"	},
	{"P1 Up",		    BIT_DIGITAL,	DrvJoy4 + 4,	"p1 up"		},
	{"P1 Down",		    BIT_DIGITAL,	DrvJoy4 + 5,	"p1 down"	},
	{"P1 Left",		    BIT_DIGITAL,	DrvJoy4 + 6,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy4 + 7,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 fire 1"	},
	A("P1 Trackball X", BIT_ANALOG_REL, &Analog[0],		"p1 x-axis"),
	A("P1 Trackball Y", BIT_ANALOG_REL, &Analog[1],		"p1 y-axis"),

	{"P2 Coin",		    BIT_DIGITAL,	DrvJoy2 + 6,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	{"P2 Up",		    BIT_DIGITAL,	DrvJoy4 + 1,	"p2 up"		},
	{"P2 Down",		    BIT_DIGITAL,	DrvJoy4 + 0,	"p2 down"	},
	{"P2 Left",		    BIT_DIGITAL,	DrvJoy4 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy4 + 2,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 1"	},
	A("P2 Trackball X", BIT_ANALOG_REL, &Analog[2],		"p2 x-axis"),
	A("P2 Trackball Y", BIT_ANALOG_REL, &Analog[3],		"p2 y-axis"),

	{"Reset",		    BIT_DIGITAL,	&DrvReset,	    "reset"		},
	{"Service",		    BIT_DIGITAL,	DrvJoy2 + 7,	"service"	},
	{"Tilt",		    BIT_DIGITAL,	DrvJoy2 + 4,	"tilt"		},
	{"Dip A",		    BIT_DIPSWITCH,	DrvDip + 0,	    "dip"		},
	{"Dip B",		    BIT_DIPSWITCH,	DrvDip + 4,	    "dip"		},
	{"Dip C",		    BIT_DIPSWITCH,	DrvDip + 5,	    "dip"		},
};

STDINPUTINFO(Centiped)

static struct BurnInputInfo WarlordsInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 5,	"p1 coin"	},
	A("P1 Dial", 		BIT_ANALOG_REL, &Analog[0],  	"p1 x-axis"),
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 6,	"p2 coin"	},
	A("P2 Dial", 		BIT_ANALOG_REL, &Analog[1],  	"p2 x-axis"),
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 1"	},

	{"P3 Coin",			BIT_DIGITAL,	DrvJoyF + 0,	"p3 coin"	}, // fake
	A("P3 Dial", 		BIT_ANALOG_REL, &Analog[2],  	"p3 x-axis"),
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy2 + 2,	"p3 fire 1"	},

	{"P4 Coin",			BIT_DIGITAL,	DrvJoyF + 1,	"p4 coin"	}, // fake
	A("P4 Dial", 		BIT_ANALOG_REL, &Analog[3],  	"p4 x-axis"),
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p4 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 7,	"service"	},
	{"Tilt",			BIT_DIGITAL,	DrvJoy2 + 4,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDip + 0,		"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDip + 2,		"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDip + 3,		"dip"		},
};

STDINPUTINFO(Warlords)

static struct BurnInputInfo MazeinvInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 5,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 start"	},
	A("P1 Stick X",		BIT_ANALOG_REL, &Analog[2],		"p1 x-axis"),
	A("P1 Stick Y",		BIT_ANALOG_REL, &Analog[0],		"p1 y-axis"),
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 6,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy4 + 3,	"p2 start"	},
	A("P2 Stick X",		BIT_ANALOG_REL, &Analog[3],		"p2 x-axis"),
	A("P2 Stick Y",		BIT_ANALOG_REL, &Analog[1],		"p2 y-axis"),
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy4 + 2,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy4 + 0,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy3 + 4,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDip + 0,		"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDip + 1,		"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDip + 3,		"dip"		},
	{"Dip D",			BIT_DIPSWITCH,	DrvDip + 4,		"dip"		},
};

STDINPUTINFO(Mazeinv)

static struct BurnDIPInfo MazeinvDIPList[]=
{
	DIP_OFFSET(0x0e)
	{0x00, 0xff, 0xff, 0x00, NULL					},
	{0x01, 0xff, 0xff, 0xc0, NULL					},
	{0x02, 0xff, 0xff, 0x44, NULL					},
	{0x03, 0xff, 0xff, 0x02, NULL					},

	{0   , 0xfe, 0   ,    4, "Language"				},
	{0x00, 0x01, 0x03, 0x00, "English"				},
	{0x00, 0x01, 0x03, 0x01, "German"				},
	{0x00, 0x01, 0x03, 0x02, "French"				},
	{0x00, 0x01, 0x03, 0x03, "Spanish"				},

	{0   , 0xfe, 0   ,    2, "Minimum credits"		},
	{0x00, 0x01, 0x40, 0x00, "1"					},
	{0x00, 0x01, 0x40, 0x40, "2"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x01, 0x01, 0x20, 0x20, "Upright"				},
	{0x01, 0x01, 0x20, 0x00, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x01, 0x01, 0x80, 0x00, "On"					},
	{0x01, 0x01, 0x80, 0x80, "Off"					},

	{0   , 0xfe, 0   ,    4, "Doors for bonus"		},
	{0x02, 0x01, 0x03, 0x00, "10"					},
	{0x02, 0x01, 0x03, 0x01, "12"					},
	{0x02, 0x01, 0x03, 0x02, "14"					},
	{0x02, 0x01, 0x03, 0x03, "16"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x02, 0x01, 0x0c, 0x00, "2"					},
	{0x02, 0x01, 0x0c, 0x04, "3"					},
	{0x02, 0x01, 0x0c, 0x08, "4"					},
	{0x02, 0x01, 0x0c, 0x0c, "5"					},

	{0   , 0xfe, 0   ,    4, "Extra life at"		},
	{0x02, 0x01, 0x30, 0x00, "20000"				},
	{0x02, 0x01, 0x30, 0x10, "25000"				},
	{0x02, 0x01, 0x30, 0x20, "30000"				},
	{0x02, 0x01, 0x30, 0x30, "Never"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x02, 0x01, 0xc0, 0x00, "Easiest"				},
	{0x02, 0x01, 0xc0, 0x40, "Easy"					},
	{0x02, 0x01, 0xc0, 0x80, "Hard"					},
	{0x02, 0x01, 0xc0, 0xc0, "Harder"				},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x03, 0x01, 0x03, 0x03, "2 Coins 1 Credits"	},
	{0x03, 0x01, 0x03, 0x02, "1 Coin  1 Credits"	},
	{0x03, 0x01, 0x03, 0x01, "1 Coin  2 Credits"	},
	{0x03, 0x01, 0x03, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    4, "Coin 2"				},
	{0x03, 0x01, 0x0c, 0x00, "*1"					},
	{0x03, 0x01, 0x0c, 0x04, "*4"					},
	{0x03, 0x01, 0x0c, 0x08, "*5"					},
	{0x03, 0x01, 0x0c, 0x0c, "*6"					},

	{0   , 0xfe, 0   ,    2, "Coin 3"				},
	{0x03, 0x01, 0x10, 0x00, "*1"					},
	{0x03, 0x01, 0x10, 0x10, "*2"					},

	{0   , 0xfe, 0   ,    5, "Bonus Coins"			},
	{0x03, 0x01, 0xe0, 0x00, "None"					},
	{0x03, 0x01, 0xe0, 0x20, "3 credits/2 coins"	},
	{0x03, 0x01, 0xe0, 0x40, "5 credits/4 coins"	},
	{0x03, 0x01, 0xe0, 0x60, "6 credits/4 coins"	},
	{0x03, 0x01, 0xe0, 0x80, "6 credits/5 coins"	},
};

STDDIPINFO(Mazeinv)


static struct BurnDIPInfo WarlordsDIPList[]=
{
	DIP_OFFSET(0x0f)
	{0x00, 0xff, 0xff, 0x20, NULL					},
	{0x01, 0xff, 0xff, 0x00, NULL					},
	{0x02, 0xff, 0xff, 0x02, NULL					},

	{0   , 0xfe, 0   ,    2, "Diag Step"			},
	{0x00, 0x01, 0x10, 0x00, "Off"					},
	{0x00, 0x01, 0x10, 0x10, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x00, 0x01, 0x20, 0x00, "On"					},
	{0x00, 0x01, 0x20, 0x20, "Off"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x00, 0x01, 0x80, 0x80, "Upright"				},
	{0x00, 0x01, 0x80, 0x00, "Cocktail"				},

	{0   , 0xfe, 0   ,    4, "Language"				},
	{0x01, 0x01, 0x03, 0x00, "English"				},
	{0x01, 0x01, 0x03, 0x01, "French"				},
	{0x01, 0x01, 0x03, 0x02, "Spanish"				},
	{0x01, 0x01, 0x03, 0x03, "German"				},

	{0   , 0xfe, 0   ,    2, "Music"				},
	{0x01, 0x01, 0x04, 0x00, "End of game"			},
	{0x01, 0x01, 0x04, 0x04, "High score only"		},

	{0   , 0xfe, 0   ,    3, "Credits"				},
	{0x01, 0x01, 0x30, 0x00, "1p/2p = 1 credit"		},
	{0x01, 0x01, 0x30, 0x10, "1p = 1, 2p = 2"		},
	{0x01, 0x01, 0x30, 0x20, "1p/2p = 2 credits"	},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x02, 0x01, 0x03, 0x03, "2 Coins 1 Credits"	},
	{0x02, 0x01, 0x03, 0x02, "1 Coin  1 Credits"	},
	{0x02, 0x01, 0x03, 0x01, "1 Coin  2 Credits"	},
	{0x02, 0x01, 0x03, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    4, "Right Coin"			},
	{0x02, 0x01, 0x0c, 0x00, "*1"					},
	{0x02, 0x01, 0x0c, 0x04, "*4"					},
	{0x02, 0x01, 0x0c, 0x08, "*5"					},
	{0x02, 0x01, 0x0c, 0x0c, "*6"					},

	{0   , 0xfe, 0   ,    2, "Left Coin"			},
	{0x02, 0x01, 0x10, 0x00, "*1"					},
	{0x02, 0x01, 0x10, 0x10, "*2"					},

	{0   , 0xfe, 0   ,    5, "Bonus Coins"			},
	{0x02, 0x01, 0xe0, 0x00, "None"					},
	{0x02, 0x01, 0xe0, 0x20, "3 credits/2 coins"	},
	{0x02, 0x01, 0xe0, 0x40, "5 credits/4 coins"	},
	{0x02, 0x01, 0xe0, 0x60, "6 credits/4 coins"	},
	{0x02, 0x01, 0xe0, 0x80, "6 credits/5 coins"	},
};

STDDIPINFO(Warlords)


static struct BurnDIPInfo CentipedDIPList[]=
{
	DIP_OFFSET(0x15)
	{0x00, 0xff, 0xff, 0x30, NULL					},  // dip0
	{0x01, 0xff, 0xff, 0x54, NULL					},  // dip4
	{0x02, 0xff, 0xff, 0x02, NULL					},  // dip5

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x00, 0x01, 0x10, 0x00, "Upright"				},
	{0x00, 0x01, 0x10, 0x10, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x00, 0x01, 0x20, 0x20, "Off"					},
	{0x00, 0x01, 0x20, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Language"				},
	{0x01, 0x01, 0x03, 0x00, "English"				},
	{0x01, 0x01, 0x03, 0x01, "German"				},
	{0x01, 0x01, 0x03, 0x02, "French"				},
	{0x01, 0x01, 0x03, 0x03, "Spanish"				},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x01, 0x01, 0x0c, 0x00, "2"					},
	{0x01, 0x01, 0x0c, 0x04, "3"					},
	{0x01, 0x01, 0x0c, 0x08, "4"					},
	{0x01, 0x01, 0x0c, 0x0c, "5"					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x01, 0x01, 0x30, 0x00, "10000"				},
	{0x01, 0x01, 0x30, 0x10, "12000"				},
	{0x01, 0x01, 0x30, 0x20, "15000"				},
	{0x01, 0x01, 0x30, 0x30, "20000"				},

	{0   , 0xfe, 0   ,    2, "Difficulty"			},
	{0x01, 0x01, 0x40, 0x40, "Easy"					},
	{0x01, 0x01, 0x40, 0x00, "Hard"					},

	{0   , 0xfe, 0   ,    2, "Credit Minimum"		},
	{0x01, 0x01, 0x80, 0x00, "1"					},
	{0x01, 0x01, 0x80, 0x80, "2"					},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x02, 0x01, 0x03, 0x03, "2 Coins 1 Credits"	},
	{0x02, 0x01, 0x03, 0x02, "1 Coin  1 Credits"	},
	{0x02, 0x01, 0x03, 0x01, "1 Coin  2 Credits"	},
	{0x02, 0x01, 0x03, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    4, "Right Coin"			},
	{0x02, 0x01, 0x0c, 0x00, "*1"					},
	{0x02, 0x01, 0x0c, 0x04, "*4"					},
	{0x02, 0x01, 0x0c, 0x08, "*5"					},
	{0x02, 0x01, 0x0c, 0x0c, "*6"					},

	{0   , 0xfe, 0   ,    2, "Left Coin"			},
	{0x02, 0x01, 0x10, 0x00, "*1"					},
	{0x02, 0x01, 0x10, 0x10, "*2"					},

	{0   , 0xfe, 0   ,    6, "Bonus Coins"			},
	{0x02, 0x01, 0xe0, 0x00, "None"					},
	{0x02, 0x01, 0xe0, 0x20, "3 credits/2 coins"	},
	{0x02, 0x01, 0xe0, 0x40, "5 credits/4 coins"	},
	{0x02, 0x01, 0xe0, 0x60, "6 credits/4 coins"	},
	{0x02, 0x01, 0xe0, 0x80, "6 credits/5 coins"	},
	{0x02, 0x01, 0xe0, 0xa0, "4 credits/3 coins"	},
};

STDDIPINFO(Centiped)


static struct BurnDIPInfo MillipedDIPList[]=
{
	DIP_OFFSET(0x15)
	{0x00, 0xff, 0xff, 0x04, NULL					},
	{0x01, 0xff, 0xff, 0x00, NULL					},
	{0x02, 0xff, 0xff, 0x80, NULL					},
	{0x03, 0xff, 0xff, 0x04, NULL					},
	{0x04, 0xff, 0xff, 0x02, NULL					},

	{0   , 0xfe, 0   ,    4, "Language"				},
	{0x00, 0x01, 0x03, 0x00, "English"				},
	{0x00, 0x01, 0x03, 0x01, "German"				},
	{0x00, 0x01, 0x03, 0x02, "French"				},
	{0x00, 0x01, 0x03, 0x03, "Spanish"				},

	{0   , 0xfe, 0   ,    4, "Bonus"				},
	{0x00, 0x01, 0x0c, 0x00, "0"					},
	{0x00, 0x01, 0x0c, 0x04, "0 1x"					},
	{0x00, 0x01, 0x0c, 0x08, "0 1x 2x"				},
	{0x00, 0x01, 0x0c, 0x0c, "0 1x 2x 3x"			},

	{0   , 0xfe, 0   ,    2, "Credit Minimum"		},
	{0x01, 0x01, 0x04, 0x00, "1"					},
	{0x01, 0x01, 0x04, 0x04, "2"					},

	{0   , 0xfe, 0   ,    2, "Coin Counters"		},
	{0x01, 0x01, 0x08, 0x00, "1"					},
	{0x01, 0x01, 0x08, 0x08, "2"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x02, 0x01, 0x20, 0x20, "Upright"				},
	{0x02, 0x01, 0x20, 0x00, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x02, 0x01, 0x80, 0x80, "Off"					},
	{0x02, 0x01, 0x80, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Millipede Head"		},
	{0x03, 0x01, 0x01, 0x00, "Easy"					},
	{0x03, 0x01, 0x01, 0x01, "Hard"					},

	{0   , 0xfe, 0   ,    2, "Beetle"				},
	{0x03, 0x01, 0x02, 0x00, "Easy"					},
	{0x03, 0x01, 0x02, 0x02, "Hard"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x03, 0x01, 0x0c, 0x00, "2"					},
	{0x03, 0x01, 0x0c, 0x04, "3"					},
	{0x03, 0x01, 0x0c, 0x08, "4"					},
	{0x03, 0x01, 0x0c, 0x0c, "5"					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x03, 0x01, 0x30, 0x00, "12000"				},
	{0x03, 0x01, 0x30, 0x10, "15000"				},
	{0x03, 0x01, 0x30, 0x20, "20000"				},
	{0x03, 0x01, 0x30, 0x30, "None"					},

	{0   , 0xfe, 0   ,    2, "Spider"				},
	{0x03, 0x01, 0x40, 0x00, "Easy"					},
	{0x03, 0x01, 0x40, 0x40, "Hard"					},

	{0   , 0xfe, 0   ,    2, "Starting Score Select"},
	{0x03, 0x01, 0x80, 0x80, "Off"					},
	{0x03, 0x01, 0x80, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x04, 0x01, 0x03, 0x03, "2 Coins 1 Credits"	},
	{0x04, 0x01, 0x03, 0x02, "1 Coin  1 Credits"	},
	{0x04, 0x01, 0x03, 0x01, "1 Coin  2 Credits"	},
	{0x04, 0x01, 0x03, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    4, "Right Coin"			},
	{0x04, 0x01, 0x0c, 0x00, "*1"					},
	{0x04, 0x01, 0x0c, 0x04, "*4"					},
	{0x04, 0x01, 0x0c, 0x08, "*5"					},
	{0x04, 0x01, 0x0c, 0x0c, "*6"					},

	{0   , 0xfe, 0   ,    2, "Left Coin"			},
	{0x04, 0x01, 0x10, 0x00, "*1"					},
	{0x04, 0x01, 0x10, 0x10, "*2"					},

	{0   , 0xfe, 0   ,    7, "Bonus Coins"			},
	{0x04, 0x01, 0xe0, 0x00, "None"					},
	{0x04, 0x01, 0xe0, 0x20, "3 credits/2 coins"	},
	{0x04, 0x01, 0xe0, 0x40, "5 credits/4 coins"	},
	{0x04, 0x01, 0xe0, 0x60, "6 credits/4 coins"	},
	{0x04, 0x01, 0xe0, 0x80, "6 credits/5 coins"	},
	{0x04, 0x01, 0xe0, 0xa0, "4 credits/3 coins"	},
	{0x04, 0x01, 0xe0, 0xc0, "Demo Mode"			},
};

STDDIPINFO(Milliped)

static void millipede_set_color(UINT16 offset, UINT8 data)
{
	data ^= 0xff;

	INT32 r = ((data & 0x20) ? 0x21 : 0) | ((data & 0x40) ? 0x47 : 0) | ((data & 0x80) ? 0x97 : 0);
	INT32 g = ((data & 0x00) ? 0x21 : 0) | ((data & 0x08) ? 0x47 : 0) | ((data & 0x10) ? 0x97 : 0);
	INT32 b = ((data & 0x01) ? 0x21 : 0) | ((data & 0x02) ? 0x47 : 0) | ((data & 0x04) ? 0x97 : 0);

	INT32 color = BurnHighCol(r, g, b, 0);

	if (offset < 0x10) // chars
		DrvPalette[offset] = color;
	else
	{
		// millipede
		// sprite color code format
		// bb332211  b.ank, 3.rd pen, 2.nd pen, 1.st pen  (0 pen is transparent)
		// centipede is similar, but without upper bank bits
		// ....bbcc  b.ank, c.olor - sprite pal offset
		// note: 0 pen is always transparent, even though millipede likes to
		// assign a color value to them in the higher banks
		INT32 pal_bank = ((offset >> 2) & 3) * 0x10;
		DrvPalette[pal_bank + (offset & 3) + 0x100] = color;
	}
}

static void millipede_recalcpalette()
{
	for (INT32 i = 0; i < 0x20; i++) {
		millipede_set_color(i, (mazeinvmode) ? ~DrvColPROM[~DrvPalRAM[i] & 0x0f] : DrvPalRAM[i]);
	}
}

static void centipede_set_color(UINT16 offset, UINT8 data)
{
	if ((offset >= 0x04 && offset <= 0x07) || (offset >= 0x0c && offset <= 0x0f)) {
		// 4-7 char, c-f sprite
		data ^= 0xff;
		INT32 r = (data & 0x01) * 0xff;
		INT32 g = (data & 0x02) * 0x7f;
		INT32 b = (data & 0x04) * 0x3f;

		// alternate pal logic
		b &= (data & 0x08) ? ~0x3f : 0xff;
		g &= (data & 0x08 && !b) ? ~0x3f : 0xff;

		INT32 color = BurnHighCol(r, g, b, 0);

		DrvPalette[(offset & 0x03) + ((offset >= 0x0c) ? 0x100 : 0x0)] = color;
	}
}

static void centipede_recalcpalette()
{
	for (INT32 i = 0; i < 0x10; i++) {
		centipede_set_color(i, DrvPalRAM[i]);
	}
}

static void warlords_recalcpalette()
{
	for (INT32 i = 0; i < 0x40; i++)
	{
		UINT8 data = DrvColPROM[((i & 0x1c) << 2) | ((i & 0x03) << ((i & 0x20) >> 4))];

		INT32 r = (data & 0x04) * 0x3f;
		INT32 g = (data & 0x02) * 0x7f;
		INT32 b = (data & 0x01) * 0xff;

		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}
}

static void millipede_write(UINT16 address, UINT8 data)
{
	if (address >= 0x400 && address <= 0x40f) { // Pokey 1
		pokey1_w(address & 0xf, data);
		return;
	}
	if (address >= 0x800 && address <= 0x80f) { // Pokey 2
		pokey2_w(address & 0xf, data);
		return;
	}
	if (address >= 0x1000 && address <= 0x13bf) { // Video Ram
		DrvVidRAM[address & 0x3ff] = data;
		return;
	}
	if (address >= 0x13c0 && address <= 0x13ff) { // Sprite Ram
		DrvSpriteRAM[address & 0x3f] = data;
		return;
	}
	if (address >= 0x2480 && address <= 0x249f) { // Palette Ram
		DrvPalRAM[address & 0x1f] = data;
		millipede_set_color(address & 0x1f, (mazeinvmode) ? ~DrvColPROM[~data & 0xf] : data);
		return;
	}
	if (address >= 0x2780 && address <= 0x27bf) { // EAROM Write
		earom_write(address & 0x3f, data);
		return;
	}

	switch (address)
	{
		case 0x2505:
			dip_select = (~data >> 7) & 1;
		return;
		case 0x2506:
			flipscreen = data >> 7;
		return;

		case 0x2507:
			if (mazeinvmode == 0) {
				control_select = (data >> 7) & 1;
			}
		return;

		case 0x2580:
		case 0x2581:
		case 0x2582:
		case 0x2583:
			if (mazeinvmode) {
				control_select = address & 3;
			}
		return;

		case 0x2600:
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;
		case 0x2680:
			BurnWatchdogWrite();
		return;
		case 0x2700:
			earom_ctrl_write(0x2700, data);
		return;
	}
}

static void centipede_write(UINT16 address, UINT8 data)
{
	if (address >= 0x400 && address <= 0x7bf) { // Video Ram
		DrvVidRAM[address & 0x3ff] = data;
		return;
	}
	if (address >= 0x7c0 && address <= 0x7ff) { // Sprite Ram
		DrvSpriteRAM[address & 0x3f] = data;
		return;
	}
	if (address >= 0x1000 && address <= 0x100f) { // Pokey #1
		pokey1_w(address & 0xf, data);
		return;
	}
	if (address >= 0x1400 && address <= 0x140f) { // Palette Ram
		DrvPalRAM[address & 0xf] = data;
		centipede_set_color(address & 0xf, data);
		return;
	}
	if (address >= 0x1600 && address <= 0x163f) { // EAROM Write
		earom_write(address & 0x3f, data);
		return;
	}

	switch (address)
	{
		case 0x1c07:
			flipscreen = data >> 7;
		return;
		case 0x1680:
			earom_ctrl_write(0x1680, data);
		return;
		case 0x1800:
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;
		case 0x2000:
			BurnWatchdogRead();
		return;
		case 0x2507:
			control_select = (data >> 7) & 1;
		return;
	}
}

static void warlords_write(UINT16 address, UINT8 data)
{
	if (address >= 0x400 && address <= 0x7bf) { // Video Ram
		DrvVidRAM[address & 0x3ff] = data;
		return;
	}
	if (address >= 0x7c0 && address <= 0x7ff) { // Sprite Ram
		DrvSpriteRAM[address & 0x3f] = data;
		return;
	}

	if (address >= 0x1000 && address <= 0x100f) { // Pokey #1
		pokey1_w(address & 0xf, data);
		return;
	}

	switch (address)
	{
		case 0x1800:
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;
		case 0x1c07:
			flipscreen = data >> 7;
		return;
		case 0x4000:
			BurnWatchdogWrite();
		return;
	}
}

static INT32 read_trackball(INT32 offset)
{
	INT32 ID = (2 * flipscreen) + ((offset > 0) ? 1 : 0);
	INT32 direction = (BurnTrackballGetDirection(ID) < 0) ? 0x80 : 0x00;
	UINT8 accu = BurnTrackballReadInterpolated(ID, scanline);
	UINT8 inpt = (DrvInput[offset] | DrvDip[offset]) & 0x7f;
	//bprintf(0, _T("read tb id %x\tscanline %d\taccu/interp:  %d  %d\n"), ID, scanline, BurnTrackballRead(ID), accu);

	return (dip_select) ? inpt : (inpt & 0x70) | (accu & 0x0f) | direction;
}

static UINT8 millipede_read(UINT16 address)
{
	if (address >= 0x400 && address <= 0x40f) { // Pokey 1
		return pokey1_r(address & 0xf);
	}
	if (address >= 0x800 && address <= 0x80f) { // Pokey 2
		return pokey2_r(address & 0xf);
	}
	if (address >= 0x1000 && address <= 0x13bf) { // Video Ram
		return DrvVidRAM[address & 0x3ff];
	}
	if (address >= 0x13c0 && address <= 0x13ff) { // Sprite Ram
		return DrvSpriteRAM[address & 0x3f];
	}
	if (address >= 0x2480 && address <= 0x249f) { // Palette Ram
		return DrvPalRAM[address & 0x1f];
	}
	if (address >= 0x4000 && address <= 0x7fff) { // ROM
		return Drv6502ROM[address];
	}

	if (mazeinvmode) {
		// Mazeinv
		switch (address)
		{
			case 0x2000: return (DrvDip[0] & ~0x80) | ((vblank) ? 0x80 : 0x00);
			case 0x2001: return 0xff;
			case 0x2010: return DrvInput[2];
			case 0x2011: return (DrvInput[3] & ~0xa0) | (DrvDip[1] & 0xa0);
			case 0x2020: return ProcessAnalog(Analog[control_select], (control_select == 0 || control_select == 3) ? 1 : 0, INPUT_DEADZONE, 0x40, 0xbf);
			case 0x2030: return earom_read(address);
		}
	} else {
		// Millipede
		switch (address)
		{
			case 0x2000: return (read_trackball(address & 3) & ~0x40) | ((vblank) ? 0x40 : 0x00);
			case 0x2001: return read_trackball(address & 3);
			case 0x2010: return (control_select) ? (DrvInput[2] & 0xf0) | (DrvInput[3] & 0x0f) : DrvInput[2];
			case 0x2011: return 0x5f | DrvDip[2];
			case 0x2030: return earom_read(address);
			case 0x2680: return BurnWatchdogRead();
		}
	}

	//bprintf(0, _T("mr %X,"), address);

	return 0;
}

static UINT8 centipede_read(UINT16 address)
{
	if (address >= 0x400 && address <= 0x7bf) { // Video Ram
		return DrvVidRAM[address & 0x3ff];
	}
	if (address >= 0x7c0 && address <= 0x7ff) { // Sprite Ram
		return DrvSpriteRAM[address & 0x3f];
	}
	if (address >= 0x1000 && address <= 0x100f) { // Pokey
		return pokey1_r(address & 0xf);
	}
	if (address >= 0x1400 && address <= 0x140f) { // Palette Ram
		return DrvPalRAM[address & 0xf];
	}
	if (address >= 0x1700 && address <= 0x173f) { // EAROM
		return earom_read(address & 0x3f);
	}
	if (address >= 0x2000 && address <= 0x3fff) { // ROM
		return Drv6502ROM[address];
	}

	switch (address)
	{
		case 0x0800: return DrvDip[4];
		case 0x0801: return DrvDip[5];
		case 0x0c00: return (read_trackball(address & 3) & ~0x40) | ((vblank) ? 0x40 : 0x00);
		case 0x0c01: return DrvInput[1];
		case 0x0c02: return read_trackball(address & 3);
		case 0x0c03: return DrvInput[3];
	}

	return 0;
}

static UINT8 warlords_read(UINT16 address)
{
	if (address >= 0x400 && address <= 0x7bf) { // Video Ram
		return DrvVidRAM[address & 0x3ff];
	}
	if (address >= 0x7c0 && address <= 0x7ff) { // Sprite Ram
		return DrvSpriteRAM[address & 0x3f];
	}
	if (address >= 0x1000 && address <= 0x100f) { // Pokey
		return pokey1_r(address & 0xf);
	}
	if (address >= 0x5000 && address <= 0x7fff) { // ROM
		return Drv6502ROM[address];
	}

	switch (address)
	{
		case 0x0800: return DrvDip[2];
		case 0x0801: return DrvDip[3];
		case 0x0c01: return DrvInput[1];
		case 0x0c00: return (DrvDip[0] & 0xb0) | (vblank ? 0x40 : 0x00);
	}

	return 0;
}

static INT32 millipede_dip3_read(INT32 offs)
{
	return DrvDip[3];
}

static INT32 millipede_dip4_read(INT32 offs)
{
	return DrvDip[4];
}

static INT32 warlords_paddle_read(INT32 offs)
{
	return BurnTrackballRead(offs);
}

static tilemap_callback( centipede )
{
	INT32 code = DrvVidRAM[offs];

	TILE_SET_INFO(0, (code & 0x3f) | 0x40, 0, TILE_FLIPYX(code >> 6));
}

static tilemap_callback( millipede )
{
	INT32 code = DrvVidRAM[offs];
	INT32 bank = (code & 0x40) << 1;
	INT32 color = (code & 0xc0) >> 6;

	TILE_SET_INFO(0, (code & 0x3f) | 0x40 | bank, color, TILE_FLIPYX((flipscreen) ? 0x03 : 0));
}

static tilemap_callback( warlords )
{
	INT32 code = DrvVidRAM[offs];
	INT32 color = ((offs & 0x10) >> 4) | ((offs & 0x200) >> 8);

	TILE_SET_INFO(0, code, color, TILE_FLIPYX(code >> 6));
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) memset (AllRam, 0, RamEnd - AllRam);

	dip_select = 0;
	flipscreen = 0;

	memset(&DrvJoyF, 0, sizeof(DrvJoyF));

	M6502Open(0);
	M6502Reset();
	M6502Close();

	earom_reset();

	BurnWatchdogReset();

	HiscoreReset();

	startframe = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv6502ROM		= Next; Next += 0x08000;

	DrvPalette		= (UINT32*)Next; Next += 0x0200 * sizeof(UINT32);
	DrvBGGFX        = Next; Next += 0x10000;
	DrvSpriteGFX    = Next; Next += 0x10000;
	DrvColPROM      = Next; Next += 0x00200;

	AllRam			= Next;

	Drv6502RAM		= Next; Next += 0x00400;
	DrvVidRAM		= Next; Next += 0x00400;
	DrvSpriteRAM	= Next; Next += 0x00040;
	DrvPalRAM		= Next; Next += 0x00020;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvLoadRoms(INT32 prg_start)
{
	char* pRomName;
	struct BurnRomInfo ri;
	UINT8 *pLoad = Drv6502ROM + prg_start;
	UINT8 *gLoad = DrvBGGFX;
	UINT8 *cLoad = DrvColPROM;

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++)
	{
		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & BRF_PRG) && (ri.nType & 0x03) == 1) // prg
		{
			if (BurnLoadRom(pLoad, i, 1)) return 1;
			pLoad += ri.nLen;
		}

		if ((ri.nType & BRF_GRA) && (ri.nType & 0x03) == 2) // gfx
		{
			if (BurnLoadRom(gLoad, i, 1)) return 1;
			gLoad += ri.nLen;
		}

		if ((ri.nType & BRF_GRA) && (ri.nType & 0x03) == 3) // proms
		{
			if (BurnLoadRom(cLoad, i, 1)) return 1;
			cLoad += 0x100;
		}
	}

	{ // gfx decoding
		INT32 gfx_len = gLoad - DrvBGGFX;
		INT32 PlaneOffsets[2] = { (gfx_len/2) * 8, 0 };
		INT32 CharXOffsets[8] = { STEP8(0, 1) };
		INT32 CharYOffsets[8] = { STEP8(0, 8) };
		INT32 SpriteXOffsets[8] = { STEP8(0, 1) };
		INT32 SpriteYOffsets[16] = { STEP16(0, 8) };

		UINT8 *DrvTempRom = (UINT8 *)BurnMalloc(0x10000);
		memcpy(DrvTempRom, DrvBGGFX, 0x4000);

		if (warlordsmode) {
			GfxDecode(0x40, 2, 8, 8, PlaneOffsets, CharXOffsets, CharYOffsets, 0x40, DrvTempRom + 0x000, DrvBGGFX);
			GfxDecode(0x40, 2, 8, 8, PlaneOffsets, CharXOffsets, CharYOffsets, 0x40, DrvTempRom + 0x200, DrvSpriteGFX);
		} else {
			GfxDecode(0x100, 2, 8, 8, PlaneOffsets, CharXOffsets, CharYOffsets, 0x40, DrvTempRom, DrvBGGFX);
			GfxDecode(0x80, 2, 8, 16, PlaneOffsets, SpriteXOffsets, SpriteYOffsets, 0x80, DrvTempRom, DrvSpriteGFX);
		}
		BurnFree(DrvTempRom);
	}

	return 0;
}

static INT32 DrvInit() // millipede
{
	BurnAllocMemIndex();

	if (DrvLoadRoms(0x4000)) return 1;

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502SetAddressMask(0x7fff);
	M6502MapMemory(Drv6502RAM,	0x0000, 0x03ff, MAP_RAM);
	M6502MapMemory(Drv6502ROM + 0x4000,	0x4000, 0x7fff, MAP_ROM);
	M6502SetWriteHandler(millipede_write);
	M6502SetReadHandler(millipede_read);
	M6502Close();

	PokeyInit(1512000, 2, 0.75, 0);
	PokeySetTotalCyclesCB(M6502TotalCycles);
	PokeyAllPotCallback(0, millipede_dip3_read);
	PokeyAllPotCallback(1, millipede_dip4_read);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, millipede_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvBGGFX, 2, 8, 8, 0x100 * 8 * 8, 0x00, 0x03);

	earom_init();

	BurnTrackballInit(2);

	BurnWatchdogInit(DrvDoReset, 8);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvInitmazeinv()
{
	mazeinvmode = 1;

	BurnAllocMemIndex();

	if (DrvLoadRoms(0x3000)) return 1;

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502SetAddressMask(0x7fff);
	M6502MapMemory(Drv6502RAM,	0x0000, 0x03ff, MAP_RAM);
	M6502MapMemory(Drv6502ROM + 0x3000,	0x3000, 0x7fff, MAP_ROM);
	M6502SetWriteHandler(millipede_write);
	M6502SetReadHandler(millipede_read);
	M6502Close();

	PokeyInit(1512000, 2, 0.75, 0);
	PokeySetTotalCyclesCB(M6502TotalCycles);
	PokeyAllPotCallback(0, millipede_dip3_read);
	PokeyAllPotCallback(1, millipede_dip4_read);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, millipede_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvBGGFX, 2, 8, 8, 0x100 * 8 * 8, 0x00, 0x03);

	earom_init();

	BurnTrackballInit(2);

	BurnWatchdogInit(DrvDoReset, 8);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvInitcentiped()
{
	BurnAllocMemIndex();

	centipedemode = 1;

	if (DrvLoadRoms(0x2000)) return 1;

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502SetAddressMask(0x3fff);
	M6502MapMemory(Drv6502RAM,	0x0000, 0x03ff, MAP_RAM);
	M6502MapMemory(Drv6502ROM + 0x2000,	0x2000, 0x3fff, MAP_ROM);
	M6502SetWriteHandler(centipede_write);
	M6502SetReadHandler(centipede_read);
	M6502Close();

	PokeyInit(1512000, 2, 2.40, 0);
	PokeySetTotalCyclesCB(M6502TotalCycles);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, centipede_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvBGGFX, 2, 8, 8, 0x100 * 8 * 8, 0x00, 0x01);

	earom_init();

	BurnTrackballInit(2);

	BurnWatchdogInit(DrvDoReset, 8);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvInitwarlords()
{
	BurnAllocMemIndex();

	warlordsmode = 1;

	if (DrvLoadRoms(0x5000)) return 1;

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502SetAddressMask(0x7fff);
	M6502MapMemory(Drv6502RAM,	0x0000, 0x03ff, MAP_RAM);
	M6502MapMemory(Drv6502ROM + 0x5000,	0x5000, 0x7fff, MAP_ROM);
	M6502SetWriteHandler(warlords_write);
	M6502SetReadHandler(warlords_read);
	M6502Close();

	PokeyInit(1512000, 2, 1.00, 0);
	PokeySetTotalCyclesCB(M6502TotalCycles);
	PokeyPotCallback(0, 0, warlords_paddle_read);
	PokeyPotCallback(0, 1, warlords_paddle_read);
	PokeyPotCallback(0, 2, warlords_paddle_read);
	PokeyPotCallback(0, 3, warlords_paddle_read);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, warlords_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvBGGFX, 2, 8, 8, 0x40 * 8 * 8, 0x00, 0x7);

	earom_init(); // not this hw

	BurnTrackballInit(2);
	BurnTrackballConfigStartStopPoints(0, 0x1d, 0xcb, 0x1d, 0xcb);
	BurnTrackballConfigStartStopPoints(1, 0x1d, 0xcb, 0x1d, 0xcb);

	BurnWatchdogInit(DrvDoReset, 8);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	PokeyExit();
	M6502Exit();
	earom_exit();
	BurnTrackballExit();
	BurnWatchdogExit();

	BurnFreeMemIndex();

	centipedemode = 0;
	warlordsmode = 0;
	mazeinvmode = 0;

	dip_select = 0;
	flipscreen = 0;

	memset(&DrvDip, 0, sizeof(DrvDip));

	return 0;
}

static void draw_sprites_warlords()
{
	for (INT32 offs = 0; offs < 0x10; offs++) {
		INT32 code = DrvSpriteRAM[offs] & 0x3f;
		INT32 flipx = DrvSpriteRAM[offs] & 0x40;
		INT32 flipy = DrvSpriteRAM[offs] & 0x80;
		INT32 sx = DrvSpriteRAM[offs + 0x20];
		INT32 sy = 248 - DrvSpriteRAM[offs + 0x10];
		INT32 color = ((sy & 0x80) >> 6) | ((sx & 0x80) >> 7);

		if (DrvDip[0] & 0x80) {
			sx = 248 - sx;
			flipx = !flipx;
		}

		Draw8x8MaskTile(pTransDraw, code, sx, sy, flipx, flipy, color, 2, 0, 0x20, DrvSpriteGFX);
	}
}

void RenderSpriteCentipede(UINT16 *dest, UINT8 *gfx, INT32 code, INT32 color, INT32 sx, INT32 sy, INT32 fx, INT32 fy, INT32 width, INT32 height)
{
	INT32 max_x = (flipscreen) ? nScreenWidth : nScreenWidth - 8;
	INT32 min_x = (flipscreen) ? 8 : 0;

	UINT8 *gfx_base = gfx + (code * width * height);

	INT32 flipx = fx ? (width - 1) : 0;
	INT32 flipy = fy ? (height - 1) : 0;

	INT32 color_list[4];
	INT32 color_bank;

	color_list[0] = 0;
	color_list[1] = (color >> 0) & 3;
	color_list[2] = (color >> 2) & 3;
	color_list[3] = (color >> 4) & 3;
	color_bank    =((color >> 6) & 3) * 0x10;

	for (INT32 y = 0; y < height; y++, sy++)
	{
		if (sy < 0 || sy >= nScreenHeight) continue;

		UINT16 *dst = dest + sy * nScreenWidth + sx;

		for (INT32 x = 0; x < width; x++, sx++)
		{
			if (sx < min_x || sx >= max_x) continue;

			INT32 pxl = gfx_base[(y ^ flipy) * width + (x ^ flipx)] & 3;

			if (color_list[pxl] != 0) {
				dst[x] = (color_list[pxl] + color_bank) | 0x100;
			}
		}

		sx -= width;
	}
}

static void draw_sprites(INT32 type)
{
	for (INT32 offs = 0; offs < 0x10; offs++) {
		INT32 code = (((DrvSpriteRAM[offs] & 0x3e) >> 1) | ((DrvSpriteRAM[offs] & 0x01) << 6)) & 0x7f;
		INT32 color = DrvSpriteRAM[offs + 0x30] & ((centipedemode) ? 0x3f : 0xff);
		INT32 flipx = (type) ? (DrvSpriteRAM[offs] >> 6) & 1 : flipscreen;
		INT32 flipy = DrvSpriteRAM[offs] & 0x80;
		INT32 sx = DrvSpriteRAM[offs + 0x20];
		INT32 sy = 240 - DrvSpriteRAM[offs + 0x10];

		if (flipx && !type) {
			flipy = !flipy;
		}

		RenderSpriteCentipede(pTransDraw, DrvSpriteGFX, code, color, sx, sy, flipx, flipy, 8, 16);
	}
}

static void DrvDrawBegin()
{
	if (!pBurnDraw) return;

	if (DrvRecalc) {
		centipedemode ? centipede_recalcpalette() :
			warlordsmode ? warlords_recalcpalette() :
			millipede_recalcpalette();

		DrvRecalc = 1;
	}

	BurnTransferClear();

	if (warlordsmode) GenericTilemapSetFlip(0, (DrvDip[0] & 0x80) ? TMAP_FLIPX : 0);

	lastline = 0;
}

static void partial_update()
{
	if (!pBurnDraw) return;

	if (scanline < 0 || scanline > nScreenHeight || scanline == lastline || lastline > scanline) return;

	//bprintf(0, _T("%07d: partial %d - %d.\n"), nCurrentFrame, lastline, scanline);

	GenericTilesSetClip(0, nScreenWidth, lastline, scanline);

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);

	if (nSpriteEnable & 1) {
		if (warlordsmode) draw_sprites_warlords();
		else draw_sprites(centipedemode || mazeinvmode);
	}
	GenericTilesClearClip();

	lastline = scanline;
}

static void DrvDrawEnd()
{
	if (!pBurnDraw) return;
	partial_update();

	if (warlordsmode == 0) BurnTransferFlip(flipscreen, flipscreen);
	BurnTransferCopy(DrvPalette);
}

static INT32 DrvDraw()
{
	DrvDrawBegin();
	partial_update();
	DrvDrawEnd();

	return 0;
}

static void DrvMakeInputs()
{
	if (warlordsmode) {
		DrvJoy2[5] |= DrvJoyF[0];
		DrvJoy2[6] |= DrvJoyF[1];
	}

	DrvInput[0] = (centipedemode) ? 0x00 : 0x10 + 0x20;
	DrvInput[1] = (centipedemode||warlordsmode) ? 0xff : 0x01 + 0x02 + 0x10 + 0x20 + 0x40;
	DrvInput[2] = 0xff;
	DrvInput[3] = (centipedemode) ? 0xff : 0x01 + 0x02 + 0x04 + 0x08 + 0x10 + 0x40;

	for (INT32 i = 0; i < 8; i++) {
		DrvInput[0] ^= (DrvJoy1[i] & 1) << i;
		DrvInput[1] ^= (DrvJoy2[i] & 1) << i;
		DrvInput[2] ^= (DrvJoy3[i] & 1) << i;
		DrvInput[3] ^= (DrvJoy4[i] & 1) << i;
	}

	if (warlordsmode) {
		BurnTrackballConfig(0, AXIS_NORMAL, AXIS_NORMAL);
		BurnTrackballConfig(1, AXIS_NORMAL, AXIS_NORMAL);
	} else {
		BurnTrackballConfig(0, AXIS_REVERSED, AXIS_NORMAL);
		BurnTrackballConfig(1, AXIS_NORMAL, AXIS_REVERSED);
	}

	BurnTrackballFrame(0, Analog[0], Analog[1], 1, 0x1f, 256);
	BurnTrackballUpdate(0);
	BurnTrackballFrame(1, Analog[2], Analog[3], 1, 0x1f, 256);
	BurnTrackballUpdate(1);

	if (mazeinvmode && startframe != -1 && startframe++ == 0x15) {
		// mazeinv auto-calibrates when the stick is moved, leaving the
		// first couple moves to be completely nuts.  let's fill in the
		// final values to prevent this, after the game boots.
		memset(&Drv6502RAM[0x375], 0x40, 4); // p1 x,y p2 x,y minimum
		memset(&Drv6502RAM[0x379], 0xc0, 4); // p1 x,y p2 x,y maximum
		startframe = -1; // done
	}
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	DrvMakeInputs();

	DrvDrawBegin();

	INT32 nCyclesTotal[1] = { 1512000 / 60 };
	INT32 nCyclesDone[1] = { 0 };
	INT32 nInterleave = 256;

	vblank = 0;

	M6502Open(0);

	for (INT32 i = 0; i < nInterleave; i++) {
		scanline = i;
		if ((i & 0x0f) == 0x0f) partial_update();

		if (i == 240) {
			DrvDrawEnd();
			vblank = 1;
		}
		if (i == 48 || i == 112 || i == 176 || i == 240) {
			M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
		}

		CPU_RUN(0, M6502);
	}

	M6502Close();

	if (pBurnSoundOut) {
		pokey_update(pBurnSoundOut, nBurnSoundLen);
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

		M6502Scan(nAction);
		pokey_scan(nAction, pnMin);
		BurnTrackballScan();
		BurnWatchdogScan(nAction);

		SCAN_VAR(dip_select);
		SCAN_VAR(control_select);
		SCAN_VAR(flipscreen);
	}

	earom_scan(nAction, pnMin);

	return 0;
}


// Millipede

static struct BurnRomInfo millipedRomDesc[] = {
	{ "136013-104.mn1",	0x1000, 0x40711675, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "136013-103.l1",	0x1000, 0xfb01baf2, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136013-102.jk1",	0x1000, 0x62e137e0, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136013-101.h1",	0x1000, 0x46752c7d, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "136013-107.r5",	0x0800, 0x68c3437a, 2 | BRF_GRA | BRF_ESS }, //  4 gfx1
	{ "136013-106.p5",	0x0800, 0xf4468045, 2 | BRF_GRA | BRF_ESS }, //  5

	{ "136001-213.e7",	0x0100, 0x6fa3093a, 3 | BRF_GRA | BRF_ESS }, //  6 proms
};

STD_ROM_PICK(milliped)
STD_ROM_FN(milliped)

struct BurnDriver BurnDrvMilliped = {
	"milliped", NULL, NULL, NULL, "1982",
	"Millipede\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_ACTION, 0,
	NULL, millipedRomInfo, millipedRomName, NULL, NULL, NULL, NULL, MillipedInputInfo, MillipedDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	240, 256, 3, 4
};

// Centipede (revision 4)
static struct BurnRomInfo centipedRomDesc[] = {
	{ "136001-407.d1",	0x0800, 0xc4d995eb, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "136001-408.e1",	0x0800, 0xbcdebe1b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136001-409.fh1",	0x0800, 0x66d7b04a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136001-410.j1",	0x0800, 0x33ce4640, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "136001-211.f7",	0x0800, 0x880acfb9, 2 | BRF_GRA | BRF_ESS }, //  4 gfx1
	{ "136001-212.hj7",	0x0800, 0xb1397029, 2 | BRF_GRA | BRF_ESS }, //  5

	{ "136001-213.p4",	0x0100, 0x6fa3093a, 3 | BRF_GRA | BRF_ESS }, //  6 proms
};


STD_ROM_PICK(centiped)
STD_ROM_FN(centiped)

struct BurnDriver BurnDrvCentiped = {
	"centiped", NULL, NULL, NULL, "1980",
	"Centipede (revision 4)\0", "1 Player version", "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 1, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_ACTION, 0,
	NULL, centipedRomInfo, centipedRomName, NULL, NULL, NULL, NULL, CentipedInputInfo, CentipedDIPInfo,
	DrvInitcentiped, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	240, 256, 3, 4
};

// Centipede (revision 3)

static struct BurnRomInfo centiped3RomDesc[] = {
	{ "136001-307.d1",	0x0800, 0x5ab0d9de, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "136001-308.e1",	0x0800, 0x4c07fd3e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136001-309.fh1",	0x0800, 0xff69b424, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136001-310.j1",	0x0800, 0x44e40fa4, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "136001-211.f7",	0x0800, 0x880acfb9, 2 | BRF_GRA | BRF_ESS }, //  4 gfx1
	{ "136001-212.hj7",	0x0800, 0xb1397029, 2 | BRF_GRA | BRF_ESS }, //  5

	{ "136001-213.p4",	0x0100, 0x6fa3093a, 3 | BRF_GRA | BRF_ESS }, //  6 proms
};

STD_ROM_PICK(centiped3)
STD_ROM_FN(centiped3)

struct BurnDriver BurnDrvCentiped3 = {
	"centiped3", "centiped", NULL, NULL, "1980",
	"Centipede (revision 3)\0", "2 Player version", "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_ACTION, 0,
	NULL, centiped3RomInfo, centiped3RomName, NULL, NULL, NULL, NULL, CentipedInputInfo, CentipedDIPInfo,
	DrvInitcentiped, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	240, 256, 3, 4
};

// Warlords

static struct BurnRomInfo warlordsRomDesc[] = {
	{ "037154-01.m1",	0x0800, 0x18006c87, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "037153-01.k1",	0x0800, 0x67758f4c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "037158-01.j1",	0x0800, 0x1f043a86, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "037157-01.h1",	0x0800, 0x1a639100, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "037156-01.e1",	0x0800, 0x534f34b4, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "037155-01.d1",	0x0800, 0x23b94210, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "037159-01.e6",	0x0800, 0xff979a08, 2 | BRF_GRA },           //  6 gfx1

	{ "037235-01.n7",	0x0100, 0xa2c5c277, 3 | BRF_GRA },           //  7 proms
	{ "037161-01.m6",	0x0100, 0x6fa3093a, 3 | BRF_GRA },           //  8
};

STD_ROM_PICK(warlords)
STD_ROM_FN(warlords)

struct BurnDriver BurnDrvWarlords = {
	"warlords", NULL, NULL, NULL, "1980",
	"Warlords\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, warlordsRomInfo, warlordsRomName, NULL, NULL, NULL, NULL, WarlordsInputInfo, WarlordsDIPInfo,
	DrvInitwarlords, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	256, 240, 4, 3
};

// Maze Invaders (prototype)

static struct BurnRomInfo mazeinvRomDesc[] = {
	{ "136011-005.p1",	0x1000, 0x37129536, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "136011-004.mn1",	0x1000, 0x2d0fbf2f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136011-003.l1",	0x1000, 0x0ff3747c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136011-002.k1",	0x1000, 0x96478e07, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136011-001.hj1",	0x1000, 0xd5c29a01, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "136011-007.hj6",	0x0800, 0x16e738f4, 2 | BRF_GRA },           //  5 gfx1
	{ "136011-006.f6",	0x0800, 0xd4705e4e, 2 | BRF_GRA },           //  6

	{ "136011-009.c11",	0x0020, 0xdcc48de5, 3 | BRF_GRA },           //  7 proms
	{ "136011-008.c10",	0x0100, 0x6fa3093a, 3 | BRF_GRA },           //  8
};

STD_ROM_PICK(mazeinv)
STD_ROM_FN(mazeinv)

struct BurnDriver BurnDrvMazeinv = {
	"mazeinv", NULL, NULL, NULL, "1981",
	"Maze Invaders (prototype)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_MAZE, 0,
	NULL, mazeinvRomInfo, mazeinvRomName, NULL, NULL, NULL, NULL, MazeinvInputInfo, MazeinvDIPInfo,
	DrvInitmazeinv, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	240, 256, 3, 4
};
