// FB Neo asteroids driver module
// Based on MAME driver by Brad Oliver, Bernd Wiebelt, Allard van der Bas

#include "tiles_generic.h"
#include "m6502_intf.h"
#include "watchdog.h"
#include "bitswap.h"
#include "avgdvg.h"
#include "vector.h"
#include "pokey.h"
#include "asteroids.h"
#include "llander.h"
#include "earom.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6502ROM;
static UINT8 *DrvM6502RAM;
static UINT8 *DrvVectorRAM;
static UINT8 *DrvVectorROM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 bankdata;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static INT16 BurnGun0 = 0;
static INT32 nThrust;
static INT32 nThrustTarget;

static INT32 avgOK = 0; // ok to run avgdvg?

static INT32 astdelux = 0;
static INT32 llander = 0;

static struct BurnInputInfo AsteroidInputList[] = {
	{"P1 Coin",		    BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 start"	},
	{"P1 Left",		    BIT_DIGITAL,	DrvJoy2 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 right"	},
	{"P1 Fire",		    BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Thrust",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},
	{"P1 Hyperspace",	BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 3"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 start"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,	    "reset"		},
	{"Diag Step",		BIT_DIGITAL,	DrvJoy1 + 5,	"service2"	},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 6,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		}, // astdelux
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		}, // servicemode
};

STDINPUTINFO(Asteroid)

static struct BurnInputInfo AsteroidbInputList[] = {
	{"P1 Coin",		    BIT_DIGITAL,	DrvJoy2 + 1,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 start"	},
	{"P1 Left",		    BIT_DIGITAL,	DrvJoy2 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 right"	},
	{"P1 Fire",		    BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 1"	},
	{"P1 Thrust",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 fire 2"	},
	{"P1 Hyperspace",	BIT_DIGITAL,	DrvJoy3 + 7,	"p1 fire 3"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 start"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 2,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		}, // astdelux
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		}, // servicemode
};

STDINPUTINFO(Asteroidb)

static struct BurnInputInfo AsterockInputList[] = {
	{"P1 Coin",		    BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 start"	},
	{"P1 Left",		    BIT_DIGITAL,	DrvJoy2 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 right"	},
	{"P1 Fire",		    BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Thrust",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},
	{"P1 Hyperspace",	BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 3"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 start"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 5,	"service"	},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 6,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		}, // astdelux
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		}, // servicemode
};

STDINPUTINFO(Asterock)

static struct BurnInputInfo AstdeluxInputList[] = {
	{"P1 Coin",		    BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 start"	},
	{"P1 Left",		    BIT_DIGITAL,	DrvJoy2 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 right"	},
	{"P1 Fire",		    BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Thrust",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},
	{"P1 Shield",	    BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 3"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 start"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,	    "reset"		},
	{"Diag Step",		BIT_DIGITAL,	DrvJoy1 + 5,	"service2"	},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 6,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		}, // astdelux
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		}, // servicemode
};

STDINPUTINFO(Astdelux)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo LlanderInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 start"	},
	{"P1 Left",		    BIT_DIGITAL,	DrvJoy2 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 right"	},
	{"P1 Select Game",	BIT_DIGITAL,	DrvJoy2 + 4,	"p2 start"	}, // yes, p2,3 start instead of fire to avoid accidental pressing of these in-game
	{"P1 Abort",		BIT_DIGITAL,	DrvJoy2 + 5,	"p3 start"	},
	A("P1 Thrust",      BIT_ANALOG_REL, &BurnGun0,      "p1 y-axis"	),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Diag Step",		BIT_DIGITAL,	DrvJoy1 + 7,	"service2"	},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 2,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		}, // astdelux
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		}, // servicemode
};
#undef A

STDINPUTINFO(Llander)

static struct BurnInputInfo LlandertInputList[] = {
	// correct!
	
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Llandert)

#define DO 0xb    // getting tired of re-basing the dips. :P

static struct BurnDIPInfo AsteroidDIPList[]=
{
	DIP_OFFSET(0xb)
	{0x00, 0xff, 0xff, 0x84, NULL					},
	{0x01, 0xff, 0xff, 0x00, NULL					},
	{0x02, 0xff, 0xff, 0x00, NULL					},
	{0x03, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    4, "Language"				},
	{0x00, 0x01, 0x03, 0x00, "English"				},
	{0x00, 0x01, 0x03, 0x01, "German"				},
	{0x00, 0x01, 0x03, 0x02, "French"				},
	{0x00, 0x01, 0x03, 0x03, "Spanish"				},

	{0   , 0xfe, 0   ,    2, "Lives"				},
	{0x00, 0x01, 0x04, 0x04, "3"					},
	{0x00, 0x01, 0x04, 0x00, "4"					},

#if 0
	{0   , 0xfe, 0   ,    2, "Center Mech"			},
	{0x00, 0x01, 0x08, 0x00, "X 1"					},
	{0x00, 0x01, 0x08, 0x08, "X 2"					},

	{0   , 0xfe, 0   ,    4, "Right Mech"			},
	{0x00, 0x01, 0x30, 0x00, "X 1"					},
	{0x00, 0x01, 0x30, 0x10, "X 4"					},
	{0x00, 0x01, 0x30, 0x20, "X 5"					},
	{0x00, 0x01, 0x30, 0x30, "X 6"					},
#endif

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x00, 0x01, 0xc0, 0xc0, "2 Coins 1 Credits"	},
	{0x00, 0x01, 0xc0, 0x80, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0xc0, 0x40, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0xc0, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x02, 0x01, 0x80, 0x00, "Off"					},
	{0x02, 0x01, 0x80, 0x80, "On"					},
};

STDDIPINFO(Asteroid)

static struct BurnDIPInfo AerolitosDIPList[]=
{
	DIP_OFFSET(0xb)
	{0x00, 0xff, 0xff, 0x87, NULL					},
	{0x01, 0xff, 0xff, 0x00, NULL					},
	{0x02, 0xff, 0xff, 0x00, NULL					},
	{0x03, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    4, "Language"				},
	{0x00, 0x01, 0x03, 0x00, "English"				},
	{0x00, 0x01, 0x03, 0x01, "German"				},
	{0x00, 0x01, 0x03, 0x02, "French"				},
	{0x00, 0x01, 0x03, 0x03, "Spanish"				},

	{0   , 0xfe, 0   ,    2, "Lives"				},
	{0x00, 0x01, 0x04, 0x04, "3"					},
	{0x00, 0x01, 0x04, 0x00, "4"					},

#if 0
	{0   , 0xfe, 0   ,    2, "Center Mech"			},
	{0x00, 0x01, 0x08, 0x00, "X 1"					},
	{0x00, 0x01, 0x08, 0x08, "X 2"					},

	{0   , 0xfe, 0   ,    4, "Right Mech"			},
	{0x00, 0x01, 0x30, 0x00, "X 1"					},
	{0x00, 0x01, 0x30, 0x10, "X 4"					},
	{0x00, 0x01, 0x30, 0x20, "X 5"					},
	{0x00, 0x01, 0x30, 0x30, "X 6"					},
#endif

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x00, 0x01, 0xc0, 0xc0, "2 Coins 1 Credits"	},
	{0x00, 0x01, 0xc0, 0x80, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0xc0, 0x40, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0xc0, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x02, 0x01, 0x80, 0x00, "Off"					},
	{0x02, 0x01, 0x80, 0x80, "On"					},
};

STDDIPINFO(Aerolitos)

static struct BurnDIPInfo AsteroidbDIPList[]=
{
	DIP_OFFSET(0xa)
	{0x00, 0xff, 0xff, 0x84, NULL					},
	{0x01, 0xff, 0xff, 0x00, NULL					},
	{0x02, 0xff, 0xff, 0x00, NULL					},
	{0x03, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    4, "Language"				},
	{0x00, 0x01, 0x03, 0x00, "English"				},
	{0x00, 0x01, 0x03, 0x01, "German"				},
	{0x00, 0x01, 0x03, 0x02, "French"				},
	{0x00, 0x01, 0x03, 0x03, "Spanish"				},

	{0   , 0xfe, 0   ,    2, "Lives"				},
	{0x00, 0x01, 0x04, 0x04, "3"					},
	{0x00, 0x01, 0x04, 0x00, "4"					},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x00, 0x01, 0xc0, 0xc0, "2 Coins 1 Credits"	},
	{0x00, 0x01, 0xc0, 0x80, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0xc0, 0x40, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0xc0, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x02, 0x01, 0x80, 0x00, "Off"					},
	{0x02, 0x01, 0x80, 0x80, "On"					},
};

STDDIPINFO(Asteroidb)

static struct BurnDIPInfo AsterockDIPList[]=
{
	DIP_OFFSET(0xb)
	{0x00, 0xff, 0xff, 0x87, NULL					},
	{0x01, 0xff, 0xff, 0x00, NULL					},
	{0x02, 0xff, 0xff, 0x00, NULL					},
	{0x03, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    4, "Language"				},
	{0x00, 0x01, 0x03, 0x00, "English"				},
	{0x00, 0x01, 0x03, 0x01, "French"				},
	{0x00, 0x01, 0x03, 0x02, "German"				},
	{0x00, 0x01, 0x03, 0x03, "Italian"				},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x00, 0x01, 0x0c, 0x00, "2"					},
	{0x00, 0x01, 0x0c, 0x04, "3"					},
	{0x00, 0x01, 0x0c, 0x08, "4"					},
	{0x00, 0x01, 0x0c, 0x0c, "5"					},

	{0   , 0xfe, 0   ,    2, "Records Table"		},
	{0x00, 0x01, 0x10, 0x00, "Normal"				},
	{0x00, 0x01, 0x10, 0x10, "Special"				},

	{0   , 0xfe, 0   ,    2, "Coin Mode"			},
	{0x00, 0x01, 0x20, 0x00, "Normal"				},
	{0x00, 0x01, 0x20, 0x20, "Special"				},

	{0   , 0xfe, 0   ,    6, "Coinage"				},
	{0x00, 0x01, 0xc0, 0xc0, "2 Coins 1 Credits"	},
	{0x00, 0x01, 0xc0, 0x80, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0xc0, 0x40, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0xc0, 0xc0, "Coin A 2/1 Coin B 2/1 Coin C 1/1"	},
	{0x00, 0x01, 0xc0, 0x80, "Coin A 1/1 Coin B 1/1 Coin C 1/2"	},
	{0x00, 0x01, 0xc0, 0x40, "Coin A 1/2 Coin B 1/2 Coin C 1/4"	},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x02, 0x01, 0x80, 0x00, "Off"					},
	{0x02, 0x01, 0x80, 0x80, "On"					},
};

STDDIPINFO(Asterock)

static struct BurnDIPInfo AstdeluxDIPList[]=
{
	DIP_OFFSET(0xb)
	{0x00, 0xff, 0xff, 0x80, NULL					},
	{0x01, 0xff, 0xff, 0xfd, NULL					},
	{0x02, 0xff, 0xff, 0x00, NULL					},
	{0x03, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    4, "Language"				},
	{0x00, 0x01, 0x03, 0x00, "English"				},
	{0x00, 0x01, 0x03, 0x01, "German"				},
	{0x00, 0x01, 0x03, 0x02, "French"				},
	{0x00, 0x01, 0x03, 0x03, "Spanish"				},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x00, 0x01, 0x0c, 0x00, "2-4"					},
	{0x00, 0x01, 0x0c, 0x04, "3-5"					},
	{0x00, 0x01, 0x0c, 0x08, "4-6"					},
	{0x00, 0x01, 0x0c, 0x0c, "5-7"					},

	{0   , 0xfe, 0   ,    2, "Minimum Plays"		},
	{0x00, 0x01, 0x10, 0x00, "1"					},
	{0x00, 0x01, 0x10, 0x10, "2"					},

	{0   , 0xfe, 0   ,    2, "Difficulty"			},
	{0x00, 0x01, 0x20, 0x00, "Hard"					},
	{0x00, 0x01, 0x20, 0x20, "Easy"					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x00, 0x01, 0xc0, 0x00, "10000"				},
	{0x00, 0x01, 0xc0, 0x40, "12000"				},
	{0x00, 0x01, 0xc0, 0x80, "15000"				},
	{0x00, 0x01, 0xc0, 0xc0, "None"					},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x01, 0x01, 0x03, 0x00, "2 Coins 1 Credits"	},
	{0x01, 0x01, 0x03, 0x01, "1 Coin  1 Credits"	},
	{0x01, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},
	{0x01, 0x01, 0x03, 0x03, "Free Play"			},

	{0   , 0xfe, 0   ,    4, "Right Coin"			},
	{0x01, 0x01, 0x0c, 0x00, "X 6"					},
	{0x01, 0x01, 0x0c, 0x04, "X 5"					},
	{0x01, 0x01, 0x0c, 0x08, "X 4"					},
	{0x01, 0x01, 0x0c, 0x0c, "X 1"					},

	{0   , 0xfe, 0   ,    2, "Center Coin"			},
	{0x01, 0x01, 0x10, 0x00, "X 2"					},
	{0x01, 0x01, 0x10, 0x10, "X 1"					},

	{0   , 0xfe, 0   ,    5, "Bonus Coins"			},
	{0x01, 0x01, 0xe0, 0x60, "1 Coin Each 5 Coins"	},
	{0x01, 0x01, 0xe0, 0x80, "2 Coins Each 4 Coins"	},
	{0x01, 0x01, 0xe0, 0xa0, "1 Coin Each 4 Coins"	},
	{0x01, 0x01, 0xe0, 0xc0, "1 Coin Each 2 Coins"	},
	{0x01, 0x01, 0xe0, 0xe0, "None"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x02, 0x01, 0x80, 0x00, "Off"					},
	{0x02, 0x01, 0x80, 0x80, "On"					},
};

STDDIPINFO(Astdelux)

static struct BurnDIPInfo LlanderDIPList[]=
{
	DIP_OFFSET(0xa)
	{0x00, 0xff, 0xff, 0x80, NULL					},
	{0x01, 0xff, 0xff, 0x02, NULL					},
	{0x03, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    4, "Right Coin"			},
	{0x00, 0x01, 0x03, 0x00, "X 1"					},
	{0x00, 0x01, 0x03, 0x01, "X 4"					},
	{0x00, 0x01, 0x03, 0x02, "X 5"					},
	{0x00, 0x01, 0x03, 0x03, "X 6"					},

	{0   , 0xfe, 0   ,    4, "Language"				},
	{0x00, 0x01, 0x0c, 0x00, "English"				},
	{0x00, 0x01, 0x0c, 0x04, "French"				},
	{0x00, 0x01, 0x0c, 0x08, "Spanish"				},
	{0x00, 0x01, 0x0c, 0x0c, "German"				},

	{0   , 0xfe, 0   ,    2, "Coinage"				},
	{0x00, 0x01, 0x20, 0x00, "Normal"				},
	{0x00, 0x01, 0x20, 0x20, "Free Play"			},

	{0   , 0xfe, 0   ,    8, "Fuel Units Per Coin"	},
	{0x00, 0x01, 0xd0, 0x00, "450"					},
	{0x00, 0x01, 0xd0, 0x40, "600"					},
	{0x00, 0x01, 0xd0, 0x80, "750"					},
	{0x00, 0x01, 0xd0, 0xc0, "900"					},
	{0x00, 0x01, 0xd0, 0x10, "1100"					},
	{0x00, 0x01, 0xd0, 0x50, "1300"					},
	{0x00, 0x01, 0xd0, 0x90, "1550"					},
	{0x00, 0x01, 0xd0, 0xd0, "1800"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x01, 0x01, 0x02, 0x02, "Off"					},
	{0x01, 0x01, 0x02, 0x00, "On"					},
};

STDDIPINFO(Llander)

static struct BurnDIPInfo Llander1DIPList[]=
{
	DIP_OFFSET(0xa)
	{0x00, 0xff, 0xff, 0xa0, NULL					},
	{0x01, 0xff, 0xff, 0x02, NULL					},
	{0x03, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    4, "Right Coin"			},
	{0x00, 0x01, 0x03, 0x00, "X 1"					},
	{0x00, 0x01, 0x03, 0x01, "X 4"					},
	{0x00, 0x01, 0x03, 0x02, "X 5"					},
	{0x00, 0x01, 0x03, 0x03, "X 6"					},

	{0   , 0xfe, 0   ,    4, "Language"				},
	{0x00, 0x01, 0x0c, 0x00, "English"				},
	{0x00, 0x01, 0x0c, 0x04, "French"				},
	{0x00, 0x01, 0x0c, 0x08, "Spanish"				},
	{0x00, 0x01, 0x0c, 0x0c, "German"				},

	{0   , 0xfe, 0   ,    2, "Coinage"				},
	{0x00, 0x01, 0x10, 0x00, "Normal"				},
	{0x00, 0x01, 0x10, 0x10, "Free Play"			},

	{0   , 0xfe, 0   ,    4, "Fuel units"			},
	{0x00, 0x01, 0xc0, 0x00, "450"					},
	{0x00, 0x01, 0xc0, 0x40, "600"					},
	{0x00, 0x01, 0xc0, 0x80, "750"					},
	{0x00, 0x01, 0xc0, 0xc0, "900"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x01, 0x01, 0x02, 0x02, "Off"					},
	{0x01, 0x01, 0x02, 0x00, "On"					},
};

STDDIPINFO(Llander1)

static struct BurnDIPInfo LlandertDIPList[]=
{
	DIP_OFFSET(0x01) // yes!
	{0x00, 0xff, 0xff, 0x40, NULL					},

	{0   , 0xfe, 0   ,    4, "Parameter 1"			},
	{0x00, 0x01, 0x24, 0x00, "0"					},
	{0x00, 0x01, 0x24, 0x04, "1"					},
	{0x00, 0x01, 0x24, 0x20, "2"					},
	{0x00, 0x01, 0x24, 0x24, "Invalid"				},

	{0   , 0xfe, 0   ,    8, "Parameter 2"			},
	{0x00, 0x01, 0xd0, 0x00, "0"					},
	{0x00, 0x01, 0xd0, 0x40, "1"					},
	{0x00, 0x01, 0xd0, 0x80, "2"					},
	{0x00, 0x01, 0xd0, 0xc0, "3"					},
	{0x00, 0x01, 0xd0, 0x10, "4"					},
	{0x00, 0x01, 0xd0, 0x50, "5"					},
	{0x00, 0x01, 0xd0, 0x90, "6"					},
	{0x00, 0x01, 0xd0, 0xd0, "7"					},
};

STDDIPINFO(Llandert)

static void bankswitch(INT32 data)
{
	bankdata = data;
	INT32 bank = (astdelux) ? (data >> 7) & 1 : (data >> 2) & 1;
	if (bank == 0) {
		M6502MapMemory(DrvM6502RAM + 0x200,	0x0200, 0x02ff, MAP_RAM);
		M6502MapMemory(DrvM6502RAM + 0x300,	0x0300, 0x03ff, MAP_RAM);
	} else {
		M6502MapMemory(DrvM6502RAM + 0x300,	0x0200, 0x02ff, MAP_RAM);
		M6502MapMemory(DrvM6502RAM + 0x200,	0x0300, 0x03ff, MAP_RAM);
	}
}

static void asteroid_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x3000:
			avgdvg_go();
			avgOK = 1;
		return;

		case 0x3200:
			bankswitch(data);
		return;

		case 0x3400:
			BurnWatchdogWrite();
		return;

		case 0x3600:
			asteroid_explode_w(data);
		return;

		case 0x3a00:
			asteroid_thump_w(data);
		return;

		case 0x3c00:
		case 0x3c01:
		case 0x3c02:
		case 0x3c03:
		case 0x3c04:
		case 0x3c05:
			asteroid_sounds_w(address & 7, data);
		return;
	}
}

static void astdelux_write(UINT16 address, UINT8 data)
{
//	bprintf (0, _T("W: %4.4x, %2.2x\n"), address, data);
	
	if (address >= 0x2c00 && address <= 0x2c0f) {
		pokey_write(0, address & 0x0f, data);
		return;
	}

	if (address >= 0x3200 && address <= 0x323f) {
		earom_write(address & 0x3f, data);
		return;
	}

	switch (address)
	{
		case 0x3000:
			avgdvg_go();
			avgOK = 1;
		return;

		case 0x3c04:
			bankswitch(data);
		return;

		case 0x3400:
			BurnWatchdogWrite();
		return;

		case 0x3600:
			asteroid_explode_w(data);
		return;

		case 0x3a00:
			earom_ctrl_write(address, data);
		return;

		case 0x3c03:
			astdelux_sounds_w(data);
		return;

		case 0x3e00:
			// noise_reset_w
		return;
	}
}

static void llander_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x3000:
			avgdvg_go();
			avgOK = 1;
		return;

		case 0x3200:
			// leds
		return;

		case 0x3400:
			BurnWatchdogWrite();
		return;

		case 0x3c00:
			llander_sound_write(data);
		return;

		case 0x3e00:
			llander_sound_lfsr_reset();
		return;

		case 0x5800:
		return; // nop
	}
	
	bprintf (0, _T("W: %4.4x, %2.2x\n"), address, data); // kill warnings
}

static UINT8 asteroid_read(UINT16 address)
{
	switch (address & ~7)
	{
		case 0x2000:
		{
			UINT8 ret = (DrvInputs[0] & ~0x86);
			ret |= (DrvDips[2] & 0x80);
			ret |= ((M6502TotalCycles() & 0x100) ? 0x02 : 0);
			ret |= (avgdvg_done() ? 0x00 : 0x04);
			ret = (ret & (1 << (address & 7))) ? 0x80 : 0x7f;
			return ret;
		}

		case 0x2400:
			return (DrvInputs[1] & (1 << (address & 7))) ? 0x80 : 0x7f;
	}

	switch (address & ~3)
	{
		case 0x2800:
			return 0xfc | ((DrvDips[0] >> ((~address & 3) * 2)) & 3);
	}

	return 0;
}

static UINT8 asteroidb_read(UINT16 address)
{
		switch (address)
		{
			case 0x2000:
				return (~DrvInputs[0] & 0x7f) | (avgdvg_done() ? 0 : 0x80);

			case 0x2003:
				return (DrvInputs[2] ? 0 : 0x80);
		}
		
	return asteroid_read(address);
}

static UINT8 asterock_read(UINT16 address)
{
	switch (address & ~7)
	{
		case 0x2000:
		{
			UINT8 ret = ((DrvInputs[0] ^ 0x78) & ~0x87);
			ret |= ((DrvDips[2] & 0x80) ^ 0x80);
			ret |= ((M6502TotalCycles() & 0x100) ? 0x04 : 0);
			ret |= (avgdvg_done() ? 0x00 : 0x01);
			ret = (ret & (1 << (address & 7))) ? 0x7f : 0x80;
			return ret;
		}
	}

	return asteroid_read(address);
}

static UINT8 astdelux_read(UINT16 address)
{
//	bprintf (0, _T("R: %4.4x\n"), address);
	
	if (address >= 0x2c00 && address <= 0x2c0f) {
		return pokey_read(0, address & 0x0f);
	}

	if (address >= 0x2c40 && address <= 0x2c7f) {
		return earom_read(address);
	}

	switch (address & ~7)
	{
		case 0x2000:
		{
			UINT8 ret = (DrvInputs[0] & ~0x86);
			ret |= (DrvDips[2] & 0x80);
			ret |= ((M6502TotalCycles() & 0x100) ? 0x02 : 0);
			ret |= (avgdvg_done() ? 0x00 : 0x04);

			return (ret & (1 << (address & 7))) ? 0x80 : 0x7f;
		}

		case 0x2400:
			return (DrvInputs[1] & (1 << (address & 7))) ? 0x80 : 0x7f;
	}

	switch (address & ~3)
	{
		case 0x2800:
			return 0xfc | ((DrvDips[0] >> ((~address & 3) * 2)) & 3);;
	}

	return 0;
}

static UINT8 llander_read(UINT16 address)
{
	switch (address)
	{
		case 0x2000: {
			UINT8 ret = (DrvInputs[0] ^ 0xff) & ~0x43;
			ret |= avgdvg_done() ? 0x01 : 0;
			ret |= DrvDips[1] & 0x02;
			ret |= (M6502TotalCycles() & 0x100) ? 0x40 : 0;
			return ret;
		}

		case 0x2400:
		case 0x2401:
		case 0x2402:
		case 0x2403:
		case 0x2404:
		case 0x2405:
		case 0x2406:
		case 0x2407:
			return ((DrvInputs[1] ^ 0xf5) & (1 << (address & 7))) ? 0x80 : 0x7f;

		case 0x2800:
		case 0x2801:
		case 0x2802:
		case 0x2803:
			return 0xfc | ((DrvDips[0] >> ((~address & 3) * 2)) & 3);

		case 0x2c00:
			{
				UINT8 THRUSTINC = 8; // thrust needs to rise & fall "slowly", game freaks out in huge jumps in thrust.

				if (nThrust+THRUSTINC < nThrustTarget) nThrust += THRUSTINC;
				if (nThrust+THRUSTINC > nThrustTarget) nThrust -= THRUSTINC;
				if (nThrust < 0) nThrust = 0;

				return nThrust;
			}
	}

	return 0;
}

static INT32 allpot_read(INT32 /*offset*/)
{
	return DrvDips[1];
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	M6502Open(0);
	bankswitch(0);
	M6502Reset();
	M6502Close();

	BurnWatchdogReset();
	avgdvg_reset();

	earom_reset();

	nThrustTarget = 0;
	nThrust = 0;

	avgOK = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6502ROM		= Next; Next += 0x008000;

	DrvPalette		= (UINT32*)Next; Next += 0x20 * 256 * sizeof(UINT32);

	AllRam			= Next;

	DrvM6502RAM		= Next; Next += 0x000800;
	DrvVectorRAM	= Next; Next += 0x000800;

	RamEnd			= Next;

	DrvVectorROM	= Next; Next += 0x001800; // needs to be after DrvVectorRAM(!)

	MemEnd			= Next;

	return 0;
}

static INT32 DrvLoadRoms(INT32 pOffset, INT32 vOffset)
{
	char* pRomName;
	struct BurnRomInfo ri;

	UINT8 *pLoad = DrvM6502ROM + pOffset;
	UINT8 *vLoad = DrvVectorROM + vOffset;

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {

		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & 7) == 1) {
			if (BurnLoadRom(pLoad, i, 1)) return 1;
			pLoad += ri.nLen;
			continue;
		}

		if ((ri.nType & 7) == 2) {
			if (BurnLoadRom(vLoad, i, 1)) return 1;
			vLoad += ri.nLen;
			continue;
		}
	}

	return 0;
}

static INT32 AsteroidInit()
{
	BurnAllocMemIndex();

	if (DrvLoadRoms(0x6800, 0x0800)) return 1;

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502SetAddressMask(0x7fff);
	M6502MapMemory(DrvM6502RAM,		        0x0000, 0x01ff, MAP_RAM);
	M6502MapMemory(DrvVectorRAM,		    0x4000, 0x47ff, MAP_RAM);
	M6502MapMemory(DrvVectorROM,		    0x4800, 0x57ff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM + 0x6800,	0x6800, 0x7fff, MAP_ROM);
	M6502SetWriteHandler(asteroid_write);
	M6502SetReadHandler(asteroid_read);
	M6502Close();

	BurnWatchdogInit(DrvDoReset, 180);

	avgdvg_init(USE_DVG, DrvVectorRAM, 0x1800, M6502TotalCycles, 1044, 788);
	vector_set_offsets(11, 119);

	asteroid_sound_init();

	DrvDoReset(1);

	return 0;
}

static INT32 AstdeluxInit()
{
	BurnAllocMemIndex();

	if (DrvLoadRoms(0x6000, 0)) return 1;

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502SetAddressMask(0x7fff);
	M6502MapMemory(DrvM6502RAM,		        0x0000, 0x01ff, MAP_RAM);
	M6502MapMemory(DrvVectorRAM,		    0x4000, 0x47ff, MAP_RAM);
	M6502MapMemory(DrvVectorROM,	        0x4800, 0x57ff, MAP_ROM); // Vector ROM
	M6502MapMemory(DrvM6502ROM + 0x6000,	0x6000, 0x7fff, MAP_ROM); // Main ROM
	M6502SetWriteHandler(astdelux_write);
	M6502SetReadHandler(astdelux_read);
	M6502Close();

	BurnWatchdogInit(DrvDoReset, 180);

	avgdvg_init(USE_DVG, DrvVectorRAM, 0x1800, M6502TotalCycles, 1044, 788);
	vector_set_offsets(11, 119);

	asteroid_sound_init();

	astdelux = 1;

	earom_init();

	PokeyInit(12096000/8, 1, 0.65, 1);
	PokeySetTotalCyclesCB(M6502TotalCycles);
	PokeyAllPotCallback(0, allpot_read);

	DrvDoReset(1);

	return 0;
}

static INT32 LlanderInit()
{
	BurnAllocMemIndex();

	if (DrvLoadRoms(0x6000, 0)) return 1;

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502SetAddressMask(0x7fff);
	for (INT32 i = 0; i < 0x2000; i+= 0x100) {
		M6502MapMemory(DrvM6502RAM,		        0x0000 + i, 0x00ff + i, MAP_RAM);
	}
	M6502MapMemory(DrvVectorRAM,		    0x4000, 0x47ff, MAP_RAM);
	M6502MapMemory(DrvVectorROM,		    0x4800, 0x5fff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM + 0x6000,	0x6000, 0x7fff, MAP_ROM);
	M6502SetWriteHandler(llander_write);
	M6502SetReadHandler(llander_read);
	M6502Close();

	BurnWatchdogInit(DrvDoReset, 180);

	avgdvg_init(USE_DVG, DrvVectorRAM, 0x2000, M6502TotalCycles, 1044, 788);
	vector_set_offsets(11, -8);

	llander_sound_init();
	llander = 1;

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	M6502Exit();
	avgdvg_exit();
	asteroid_sound_exit();
	llander_sound_exit();

	BurnFreeMemIndex();

	if (astdelux) {
		earom_exit();
		PokeyExit();
	}

	astdelux = 0;
	llander = 0;

	return 0;
}

static void DrvPaletteInit()
{
    for (INT32 i = 0; i < 0x20; i++) // color
	{
		for (INT32 j = 0; j < 256; j++) // intensity
		{
			INT32 r = (0xff * j) / 0xff;
			INT32 g = r;
			INT32 b = r;

			if (astdelux) {
				INT32 hires = (DrvDips[3] & 1);
				INT32 plus = (hires) ? 0x00 : 0x40; // lores mode needs to be brighter

				r = ((0x27+plus) * j) / 0xff;
				g = ((0xa0+plus) * j) / 0xff;
				b = ((0xa0+plus) * j) / 0xff;
			}

			DrvPalette[i * 256 + j] = (r << 16) | (g << 8) | b; // must be 32bit palette! -dink (see vector.cpp)
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	draw_vector(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	{
		memset (DrvInputs, 0, 3);
		if (llander) { // active low stuff
			DrvInputs[1] |= (1<<1) | (1<<3);
		}
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		if (llander) {
			nThrustTarget = ProcessAnalog(BurnGun0, 0, 1 | INPUT_LINEAR, 0x00, 0xfe);
		}
	}

	INT32 nInterleave = 64; // nmi is 4x per frame!
	INT32 nCyclesTotal[1] = { (1512000 * 100) / 6152 }; // 61.5234375 hz
	INT32 nCyclesDone[1]  = { 0 };
	INT32 nSoundBufferPos = 0;
	INT32 interrupts_enabled = (llander) ? (DrvDips[1] & 0x02) : (DrvDips[2] & 0x80) == 0;

	M6502Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, M6502);

		if ((i % 16) == 15 && interrupts_enabled)
			M6502SetIRQLine(0x20, CPU_IRQSTATUS_AUTO);

		// Render Sound Segment
		if (pBurnSoundOut && i&1) {
			INT32 nSegmentLength = nBurnSoundLen / (nInterleave/2);
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			if (astdelux)
				astdelux_sound_update(pSoundBuf, nSegmentLength);
			else if (llander)
				llander_sound_update(pSoundBuf, nSegmentLength);
			else
				asteroid_sound_update(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	M6502Close();

	// Make sure the buffer is entirely filled.
	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			if (astdelux)
				astdelux_sound_update(pSoundBuf, nSegmentLength);
			else if (llander)
				llander_sound_update(pSoundBuf, nSegmentLength);
			else
				asteroid_sound_update(pSoundBuf, nSegmentLength);
		}

		if (astdelux) {
			pokey_update(pBurnSoundOut, nBurnSoundLen);
		}
	}

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029722;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		M6502Scan(nAction);

		avgdvg_scan(nAction, pnMin);
		BurnWatchdogScan(nAction);

		SCAN_VAR(avgOK);
		SCAN_VAR(bankdata);
		SCAN_VAR(nThrust);

		if (llander) {
			llander_sound_scan();
		} else {
			// asteroids / astdelux
			asteroid_sound_scan();
		}

		if (astdelux) {
			pokey_scan(nAction, pnMin);
		}
	}

	if (astdelux) {
		earom_scan(nAction, pnMin); // here.
	}

	if (nAction & ACB_WRITE) {
		M6502Open(0);
		bankswitch(bankdata);
		M6502Close();
	}

	return 0;
}


// Asteroids (rev 4)

static struct BurnRomInfo asteroidRomDesc[] = {
	{ "035145-04e.ef2",		0x0800, 0xb503eaf7, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "035144-04e.h2",		0x0800, 0x25233192, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "035143-02.j2",		0x0800, 0x312caa02, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "035127-02.np3",		0x0800, 0x8b71fd9e, 2 | BRF_PRG | BRF_ESS }, //  3 Vector ROM

	{ "034602-01.c8",		0x0100, 0x97953db8, 3 | BRF_GRA },           //  4 DVG PROM
};

STD_ROM_PICK(asteroid)
STD_ROM_FN(asteroid)

struct BurnDriver BurnDrvAsteroid = {
	"asteroid", NULL, NULL, NULL, "1979",
	"Asteroids (rev 4)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, asteroidRomInfo, asteroidRomName, NULL, NULL, NULL, NULL, AsteroidInputInfo, AsteroidDIPInfo,
	AsteroidInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	640, 480, 4, 3
};

// Asteroids (rev 2)

static struct BurnRomInfo asteroid2RomDesc[] = {
	{ "035145-02.ef2",		0x0800, 0x0cc75459, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "035144-02.h2",		0x0800, 0x096ed35c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "035143-02.j2",		0x0800, 0x312caa02, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "035127-02.np3",		0x0800, 0x8b71fd9e, 2 | BRF_PRG | BRF_ESS }, //  3 Vector ROM

	{ "034602-01.c8",		0x0100, 0x97953db8, 3 | BRF_GRA },           //  4 DVG PROM
};

STD_ROM_PICK(asteroid2)
STD_ROM_FN(asteroid2)

struct BurnDriver BurnDrvAsteroid2 = {
	"asteroid2", "asteroid", NULL, NULL, "1979",
	"Asteroids (rev 2)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, asteroid2RomInfo, asteroid2RomName, NULL, NULL, NULL, NULL, AsteroidInputInfo, AsteroidDIPInfo,
	AsteroidInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	640, 480, 4, 3
};


// Asteroids (rev 1)

static struct BurnRomInfo asteroid1RomDesc[] = {
	{ "035145-01.ef2",		0x0800, 0xe9bfda64, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "035144-01.h2",		0x0800, 0xe53c28a9, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "035143-01.j2",		0x0800, 0x7d4e3d05, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "035127-01.np3",		0x0800, 0x99699366, 2 | BRF_PRG | BRF_ESS }, //  3 Vector ROM

	{ "034602-01.c8",		0x0100, 0x97953db8, 3 | BRF_GRA },           //  4 DVG PROM
};

STD_ROM_PICK(asteroid1)
STD_ROM_FN(asteroid1)

struct BurnDriver BurnDrvAsteroid1 = {
	"asteroid1", "asteroid", NULL, NULL, "1979",
	"Asteroids (rev 1)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, asteroid1RomInfo, asteroid1RomName, NULL, NULL, NULL, NULL, AsteroidInputInfo, AsteroidDIPInfo,
	AsteroidInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	640, 480, 4, 3
};


// Asteroids (bootleg on Lunar Lander hardware, set 1)

static struct BurnRomInfo asteroidb1RomDesc[] = {
	{ "035145ll.de1",		0x0800, 0x605fc0f2, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "035144ll.c1",		0x0800, 0xe106de77, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "035143ll.b1",		0x0800, 0x6b1d8594, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "035127-02.np3",		0x0800, 0x8b71fd9e, 2 | BRF_PRG | BRF_ESS }, //  3 Vector ROM

	{ "034602-01.c8",		0x0100, 0x97953db8, 3 | BRF_GRA },           //  4 DVG PROM
};

STD_ROM_PICK(asteroidb1)
STD_ROM_FN(asteroidb1)

static INT32 AsteroidbInit()
{
	INT32 nRet = AsteroidInit();
	
	if (nRet == 0)
	{
		M6502Open(0);
		M6502SetReadHandler(asteroidb_read);
		M6502Close();
	}
	
	return nRet;
}

struct BurnDriver BurnDrvAsteroidb1 = {
	"asteroidb1", "asteroid", NULL, NULL, "1979",
	"Asteroids (bootleg on Lunar Lander hardware, set 1)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, asteroidb1RomInfo, asteroidb1RomName, NULL, NULL, NULL, NULL, AsteroidbInputInfo, AsteroidbDIPInfo,
	AsteroidbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	640, 480, 4, 3
};


// Asteroids (bootleg on Lunar Lander hardware, set 2)
// Based on 'asteroid1', the only difference is that the Atari copyright is removed by zeroing the ROM area

static struct BurnRomInfo asteroidb2RomDesc[] = {
	{ "p88.bin",			0x0800, 0xe9bfda64, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "p37.bin",			0x0800, 0xe53c28a9, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "p86.bin",			0x0800, 0x7d4e3d05, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "p35.bin",			0x0800, 0x7b0260df, 2 | BRF_PRG | BRF_ESS }, //  3 Vector ROM

	{ "034602-01.c8",		0x0100, 0x97953db8, 3 | BRF_GRA },           //  4 DVG PROM
};

STD_ROM_PICK(asteroidb2)
STD_ROM_FN(asteroidb2)

struct BurnDriver BurnDrvAsteroidb2 = {
	"asteroidb2", "asteroid", NULL, NULL, "1979",
	"Asteroids (bootleg on Lunar Lander hardware, set 2)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, asteroidb2RomInfo, asteroidb2RomName, NULL, NULL, NULL, NULL, AsteroidInputInfo, AsteroidDIPInfo,
	AsteroidInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	640, 480, 4, 3
};


// Space Rocks (Spanish clone of Asteroids)

static struct BurnRomInfo spcrocksRomDesc[] = {
	{ "1.bin",				0x0800, 0x0cc75459, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "2.bin",				0x0800, 0x096ed35c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.bin",				0x0800, 0xb912754d, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "e.bin",				0x0800, 0x148ef465, 2 | BRF_PRG | BRF_ESS }, //  3 Vector ROM

	{ "034602-01.c8",		0x0100, 0x97953db8, 3 | BRF_GRA },           //  4 DVG PROM
};

STD_ROM_PICK(spcrocks)
STD_ROM_FN(spcrocks)

struct BurnDriver BurnDrvSpcrocks = {
	"spcrocks", "asteroid", NULL, NULL, "1981",
	"Space Rocks (Spanish clone of Asteroids)\0", NULL, "Atari (J.Estevez license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, spcrocksRomInfo, spcrocksRomName, NULL, NULL, NULL, NULL, AsteroidInputInfo, AerolitosDIPInfo,
	AsteroidInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	640, 480, 4, 3
};


// Aerolitos (Spanish bootleg of Asteroids)

static struct BurnRomInfo aerolitosRomDesc[] = {
	{ "2516_1e.bin",		0x0800, 0x0cc75459, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "2516_1d.bin",		0x0800, 0x096ed35c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "2516_1c.bin",		0x0800, 0xb912754d, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "2716_3n.bin",		0x0800, 0x32e69e66, 2 | BRF_PRG | BRF_ESS }, //  3 Vector ROM

	{ "034602-01.c8",		0x0100, 0x97953db8, 3 | BRF_GRA },           //  4 DVG PROM
};

STD_ROM_PICK(aerolitos)
STD_ROM_FN(aerolitos)

struct BurnDriver BurnDrvAerolitos = {
	"aerolitos", "asteroid", NULL, NULL, "1980",
	"Aerolitos (Spanish bootleg of Asteroids)\0", NULL, "bootleg (Rodmar Elec.)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, aerolitosRomInfo, aerolitosRomName, NULL, NULL, NULL, NULL, AsteroidInputInfo, AerolitosDIPInfo,
	AsteroidInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	640, 480, 4, 3
};


// Aerolitos Espaciales (Spanish bootleg of Asteroids)
// Pasatiempos Laguna bootleg on Rodmar PCB

static struct BurnRomInfo aerolitolRomDesc[] = {
	{ "aerolito.1e",		0x0800, 0x0cc75459, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "aerolito.1d",		0x0800, 0x096ed35c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "aerolito.1c",		0x0800, 0xb912754d, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "aerolito.3n",		0x0800, 0x541e8ad4, 2 | BRF_PRG | BRF_ESS }, //  3 Vector ROM

	{ "034602-01.c8",		0x0100, 0x97953db8, 3 | BRF_GRA },           //  4 DVG PROM
};

STD_ROM_PICK(aerolitol)
STD_ROM_FN(aerolitol)

struct BurnDriver BurnDrvAerolitol = {
	"aerolitol", "asteroid", NULL, NULL, "1980",
	"Aerolitos Espaciales (Spanish bootleg of Asteroids)\0", NULL, "bootleg (Pasatiempos Laguna)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, aerolitolRomInfo, aerolitolRomName, NULL, NULL, NULL, NULL, AsteroidInputInfo, AerolitosDIPInfo,
	AsteroidInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	640, 480, 4, 3
};


// Asterock (Sidam bootleg of Asteroids)

static struct BurnRomInfo asterockRomDesc[] = {
	{ "10505.2",			0x0400, 0xcdf720c6, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "10505.3",			0x0400, 0xee58bdf0, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "10505.4",			0x0400, 0x8d3e421e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "10505.5",			0x0400, 0xd2ce7672, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "10505.6",			0x0400, 0x74103c87, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "10505.7",			0x0400, 0x75a39768, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "10505.0",			0x0400, 0x6bd2053f, 2 | BRF_PRG | BRF_ESS }, //  6 Vector ROM
	{ "10505.1",			0x0400, 0x231ce201, 2 | BRF_PRG | BRF_ESS }, //  7 Vector ROM 2/2

	{ "034602-01.c8",		0x0100, 0x97953db8, 3 | BRF_GRA },           //  8 DVG PROM
};

STD_ROM_PICK(asterock)
STD_ROM_FN(asterock)

static INT32 AsterockInt()
{
	INT32 nRet = AsteroidInit();
	
	if (nRet == 0)
	{
		M6502Open(0);
		M6502SetReadHandler(asterock_read);
		M6502Close();
	}
	
	return nRet;
}

struct BurnDriver BurnDrvAsterock = {
	"asterock", "asteroid", NULL, NULL, "1979",
	"Asterock (Sidam bootleg of Asteroids)\0", NULL, "bootleg (Sidam)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, asterockRomInfo, asterockRomName, NULL, NULL, NULL, NULL, AsterockInputInfo, AsterockDIPInfo,
	AsterockInt, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	640, 480, 4, 3
};


// Asterock (Videotron bootleg of Asteroids)

static struct BurnRomInfo asterockvRomDesc[] = {
	{ "10505.2",			0x0400, 0xcdf720c6, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "10505.3",			0x0400, 0xee58bdf0, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "10505.4",			0x0400, 0x8d3e421e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "10505.5",			0x0400, 0xd2ce7672, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "10505.6",			0x0400, 0x74103c87, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "10505.7",			0x0400, 0x75a39768, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "videotronas.0",		0x0400, 0xd1ac90b5, 2 | BRF_PRG | BRF_ESS }, //  6 Vector ROM
	{ "10505.1",			0x0400, 0x231ce201, 2 | BRF_PRG | BRF_ESS }, //  7 Vector ROM 2/2

	{ "034602-01.c8",		0x0100, 0x97953db8, 3 | BRF_GRA },           //  8 DVG PROM
};

STD_ROM_PICK(asterockv)
STD_ROM_FN(asterockv)

struct BurnDriver BurnDrvAsterockv = {
	"asterockv", "asteroid", NULL, NULL, "1979",
	"Asterock (Videotron bootleg of Asteroids)\0", NULL, "bootleg (Videotron)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, asterockvRomInfo, asterockvRomName, NULL, NULL, NULL, NULL, AsterockInputInfo, AsterockDIPInfo,
	AsterockInt, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	640, 480, 4, 3
};


// Meteorite (Proel bootleg of Asteroids)

static struct BurnRomInfo meteoriteRomDesc[] = {
	{ "2",					0x0400, 0xcdf720c6, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "3",					0x0400, 0xee58bdf0, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "4",					0x0400, 0x8d3e421e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "5",					0x0400, 0xd2ce7672, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "6",					0x0400, 0x379072ed, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "7",					0x0400, 0x75a39768, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "0",					0x0400, 0x7a3ff3ac, 2 | BRF_PRG | BRF_ESS }, //  6 Vector ROM
	{ "1",					0x0400, 0xd62b2887, 2 | BRF_PRG | BRF_ESS }, //  7 Vector ROM 2/2

	{ "meteorites_bprom.bin",	0x0100, 0x97953db8, 3 | BRF_GRA },           //  8 DVG PROM
};

STD_ROM_PICK(meteorite)
STD_ROM_FN(meteorite)

struct BurnDriver BurnDrvMeteorite = {
	"meteorite", "asteroid", NULL, NULL, "1979",
	"Meteorite (Proel bootleg of Asteroids)\0", NULL, "bootleg (Proel)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, meteoriteRomInfo, meteoriteRomName, NULL, NULL, NULL, NULL, AsterockInputInfo, AsterockDIPInfo,
	AsterockInt, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	640, 480, 4, 3
};


// Meteorites (VGG bootleg of Asteroids)

static struct BurnRomInfo meteortsRomDesc[] = {
	{ "m0_c1.bin",			0x0800, 0xdff88688, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "m1_f1.bin",			0x0800, 0xe53c28a9, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "m2_j1.bin",			0x0800, 0x64bd0408, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "mv_np3.bin",			0x0800, 0x11d1c4ae, 2 | BRF_PRG | BRF_ESS }, //  3 Vector ROM

	{ "034602-01.c8",		0x0100, 0x97953db8, 3 | BRF_GRA },           //  4 DVG PROM
};

STD_ROM_PICK(meteorts)
STD_ROM_FN(meteorts)

struct BurnDriver BurnDrvMeteorts = {
	"meteorts", "asteroid", NULL, NULL, "1979",
	"Meteorites (VGG bootleg of Asteroids)\0", NULL, "bootleg (VGG)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, meteortsRomInfo, meteortsRomName, NULL, NULL, NULL, NULL, AsteroidInputInfo, AsteroidDIPInfo,
	AsteroidInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	640, 480, 4, 3
};


// Meteor (Hoei bootleg of Asteroids)

static struct BurnRomInfo meteorhoRomDesc[] = {
	{ "g.bin",				0x0400, 0x7420421b, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "h.bin",				0x0400, 0xa6aa56bc, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "f.bin",				0x0400, 0x2711bd52, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "d.bin",				0x0400, 0x9f169db9, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "c.bin",				0x0400, 0xbd99556a, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "e.bin",				0x0400, 0x10fdfe9a, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "a.bin",				0x0400, 0xd7822110, 2 | BRF_PRG | BRF_ESS }, //  6 Vector ROM
	{ "b.bin",				0x0400, 0xd62b2887, 2 | BRF_PRG | BRF_ESS }, //  7 Vector ROM 2/2

	{ "prom.bin",			0x0100, 0x9e237193, 3 | BRF_GRA },           //  8 DVG PROM
};

STD_ROM_PICK(meteorho)
STD_ROM_FN(meteorho)

struct BurnDriver BurnDrvMeteorho = {
	"meteorho", "asteroid", NULL, NULL, "1979",
	"Meteor (Hoei bootleg of Asteroids)\0", NULL, "bootleg (Hoei)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, meteorhoRomInfo, meteorhoRomName, NULL, NULL, NULL, NULL, AsteroidInputInfo, AsteroidDIPInfo,
	AsteroidInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	640, 480, 4, 3
};

// Meteor (bootleg of Asteroids)

static struct BurnRomInfo meteorblRomDesc[] = {
	{ "2as.12",				0x0400, 0xcdf720c6, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "3as.13",				0x0400, 0xee58bdf0, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "4as.14",				0x0400, 0x8d3e421e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "5as.15",				0x0400, 0xd2ce7672, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "6as.16",				0x0400, 0x74103c87, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "7as.17",				0x0400, 0x75a39768, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "0as.10",				0x0400, 0xdc10767a, 2 | BRF_PRG | BRF_ESS }, //  6 Vector ROM
	{ "1as.11",				0x0400, 0x231ce201, 2 | BRF_PRG | BRF_ESS }, //  7 Vector ROM 2/2

	{ "034602-01.c8",		0x0100, 0x97953db8, 3 | BRF_GRA },           //  8 DVG PROM
};

STD_ROM_PICK(meteorbl)
STD_ROM_FN(meteorbl)

struct BurnDriver BurnDrvMeteorbl = {
	"meteorbl", "asteroid", NULL, NULL, "1979",
	"Meteor (bootleg of Asteroids)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, meteorblRomInfo, meteorblRomName, NULL, NULL, NULL, NULL, AsterockInputInfo, AsterockDIPInfo,
	AsterockInt, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	640, 480, 4, 3
};


// Hyperspace (bootleg of Asteroids)

static struct BurnRomInfo hyperspcRomDesc[] = {
	{ "035145-01.bin",		0x0800, 0xe9bfda64, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "035144-01.bin",		0x0800, 0xe53c28a9, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "035143-01.bin",		0x0800, 0x7d4e3d05, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "035127-01.bin",		0x0800, 0x7dec48bd, 2 | BRF_PRG | BRF_ESS }, //  3 Vector ROM

	{ "034602-01.c8",		0x0100, 0x97953db8, 3 | BRF_GRA },           //  4 DVG PROM
};

STD_ROM_PICK(hyperspc)
STD_ROM_FN(hyperspc)

struct BurnDriver BurnDrvHyperspc = {
	"hyperspc", "asteroid", NULL, NULL, "1979",
	"Hyperspace (bootleg of Asteroids)\0", NULL, "bootleg (Rumiano)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, hyperspcRomInfo, hyperspcRomName, NULL, NULL, NULL, NULL, AsteroidInputInfo, AsteroidDIPInfo,
	AsteroidInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	640, 480, 4, 3
};

// note: the astdelux entry in hiscore.dat breaks the game and seems wrong (the 3rd area is saving game timers ?),
//       for that reason and also because it is unnecessary (the hiscores are persisted through nvram anyway),
//       we prefer keeping the BDF_HISCORE_SUPPORTED flag disabled

// Asteroids Deluxe (rev 3)

static struct BurnRomInfo astdeluxRomDesc[] = {
	{ "036430-02.d1",		0x0800, 0xa4d7a525, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "036431-02.ef1",		0x0800, 0xd4004aae, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "036432-02.fh1",		0x0800, 0x6d720c41, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "036433-03.j1",		0x0800, 0x0dcc0be6, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "036800-02.r2",		0x0800, 0xbb8cabe1, 2 | BRF_PRG | BRF_ESS }, //  4 Vector ROM
	{ "036799-01.np2",		0x0800, 0x7d511572, 2 | BRF_PRG | BRF_ESS }, //  5 Vector ROM 2/2

	{ "034602-01.c8",		0x0100, 0x97953db8, 3 | BRF_GRA },           //  6 DVG PROM
};

STD_ROM_PICK(astdelux)
STD_ROM_FN(astdelux)

struct BurnDriver BurnDrvAstdelux = {
	"astdelux", NULL, NULL, NULL, "1980",
	"Asteroids Deluxe (rev 3)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING /* | BDF_HISCORE_SUPPORTED */, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, astdeluxRomInfo, astdeluxRomName, NULL, NULL, NULL, NULL, AstdeluxInputInfo, AstdeluxDIPInfo,
	AstdeluxInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	640, 480, 4, 3
};

// Asteroids Deluxe (rev 2)

static struct BurnRomInfo astdelux2RomDesc[] = {
	{ "036430-01.d1",		0x0800, 0x8f5dabc6, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "036431-01.ef1",		0x0800, 0x157a8516, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "036432-01.fh1",		0x0800, 0xfdea913c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "036433-02.j1",		0x0800, 0xd8db74e3, 1 | BRF_PRG | BRF_ESS }, //  3
	
	{ "036800-01.r2",		0x0800, 0x3b597407, 2 | BRF_PRG | BRF_ESS }, //  4 Vector ROM
	{ "036799-01.np2",		0x0800, 0x7d511572, 2 | BRF_PRG | BRF_ESS }, //  5 Vector ROM 2/2

	{ "034602-01.c8",		0x0100, 0x97953db8, 3 | BRF_GRA },           //  6 DVG PROM
};

STD_ROM_PICK(astdelux2)
STD_ROM_FN(astdelux2)

struct BurnDriver BurnDrvAstdelux2 = {
	"astdelux2", "astdelux", NULL, NULL, "1980",
	"Asteroids Deluxe (rev 2)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE /* | BDF_HISCORE_SUPPORTED */, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, astdelux2RomInfo, astdelux2RomName, NULL, NULL, NULL, NULL, AstdeluxInputInfo, AstdeluxDIPInfo,
	AstdeluxInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	640, 480, 4, 3
};


// Asteroids Deluxe (rev 1)

static struct BurnRomInfo astdelux1RomDesc[] = {
	{ "036430-01.d1",		0x0800, 0x8f5dabc6, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "036431-01.ef1",		0x0800, 0x157a8516, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "036432-01.fh1",		0x0800, 0xfdea913c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "036433-01.j1",		0x0800, 0xef09bac7, 1 | BRF_PRG | BRF_ESS }, //  3
	
	{ "036800-01.r2",		0x0800, 0x3b597407, 2 | BRF_PRG | BRF_ESS }, //  4 Vector ROM
	{ "036799-01.np2",		0x0800, 0x7d511572, 2 | BRF_PRG | BRF_ESS }, //  5 Vector ROM 2/2

	{ "034602-01.c8",		0x0100, 0x97953db8, 3 | BRF_GRA },           //  6 DVG PROM
};

STD_ROM_PICK(astdelux1)
STD_ROM_FN(astdelux1)

struct BurnDriver BurnDrvAstdelux1 = {
	"astdelux1", "astdelux", NULL, NULL, "1980",
	"Asteroids Deluxe (rev 1)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE /* | BDF_HISCORE_SUPPORTED */, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_VECTOR, 0,
	NULL, astdelux1RomInfo, astdelux1RomName, NULL, NULL, NULL, NULL, AstdeluxInputInfo, AstdeluxDIPInfo,
	AstdeluxInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	640, 480, 4, 3
};


// Lunar Lander (rev 2)

static struct BurnRomInfo llanderRomDesc[] = {
	{ "034572-02.f1",		0x0800, 0xb8763eea, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "034571-02.de1",		0x0800, 0x77da4b2f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "034570-01.c1",		0x0800, 0x2724e591, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "034569-02.b1",		0x0800, 0x72837a4e, 1 | BRF_PRG | BRF_ESS }, //  3
	
	{ "034599-01.r3",		0x0800, 0x355a9371, 2 | BRF_PRG | BRF_ESS }, //  4 Vector ROM 
	{ "034598-01.np3",		0x0800, 0x9c4ffa68, 2 | BRF_PRG | BRF_ESS }, //  5 Vector ROM 2/2
	{ "034597-01.m3",		0x0800, 0xebb744f2, 2 | BRF_PRG | BRF_ESS }, //  6 Vector ROM 3/3

	{ "034602-01.c8",		0x0100, 0x97953db8, 3 | BRF_GRA },           //  7 DVG PROM
};

STD_ROM_PICK(llander)
STD_ROM_FN(llander)

struct BurnDriver BurnDrvLlander = {
	"llander", NULL, NULL, NULL, "1979",
	"Lunar Lander (rev 2)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_VECTOR, 0,
	NULL, llanderRomInfo, llanderRomName, NULL, NULL, NULL, NULL, LlanderInputInfo, LlanderDIPInfo,
	LlanderInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	640, 480, 4, 3
};


// Lunar Lander (rev 1)

static struct BurnRomInfo llander1RomDesc[] = {
	{ "034572-01.f1",		0x0800, 0x2aff3140, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "034571-01.de1",		0x0800, 0x493e24b7, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "034570-01.c1",		0x0800, 0x2724e591, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "034569-01.b1",		0x0800, 0xb11a7d01, 1 | BRF_PRG | BRF_ESS }, //  3
	
	{ "034599-01.r3",		0x0800, 0x355a9371, 2 | BRF_PRG | BRF_ESS }, //  4 Vector ROM 
	{ "034598-01.np3",		0x0800, 0x9c4ffa68, 2 | BRF_PRG | BRF_ESS }, //  5 Vector ROM 2/2
	{ "034597-01.m3",		0x0800, 0xebb744f2, 2 | BRF_PRG | BRF_ESS }, //  6 Vector ROM 3/3

	{ "034602-01.c8",		0x0100, 0x97953db8, 3 | BRF_GRA },           //  7 DVG PROM
};

STD_ROM_PICK(llander1)
STD_ROM_FN(llander1)

struct BurnDriver BurnDrvLlander1 = {
	"llander1", "llander", NULL, NULL, "1979",
	"Lunar Lander (rev 1)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_VECTOR, 0,
	NULL, llander1RomInfo, llander1RomName, NULL, NULL, NULL, NULL, LlanderInputInfo, Llander1DIPInfo,
	LlanderInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	640, 480, 4, 3
};


// Lunar Lander (screen test)

static struct BurnRomInfo llandertRomDesc[] = {
	{ "llprom0.de1",		0x0800, 0xb5302947, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "llprom1.c1",			0x0800, 0x761a5b45, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "llprom2.b1",			0x0800, 0x9ec62656, 1 | BRF_PRG | BRF_ESS }, //  2
	
	{ "llvrom0.r3",			0x0800, 0xc307b42a, 2 | BRF_PRG | BRF_ESS }, //  3 Vector ROM 
	{ "llvrom1.np3",		0x0800, 0xace6b2be, 2 | BRF_PRG | BRF_ESS }, //  4 Vector ROM 2/2
	{ "llvrom2.m3",			0x0800, 0x56c38219, 2 | BRF_PRG | BRF_ESS }, //  5 Vector ROM 3/3

	{ "034602-01.c8",		0x0100, 0x97953db8, 3 | BRF_GRA },           //  6 DVG PROM
};

STD_ROM_PICK(llandert)
STD_ROM_FN(llandert)

struct BurnDriverD BurnDrvLlandert = {
	"llandert", "llander", NULL, NULL, "1979",
	"Lunar Lander (screen test)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_VECTOR, 0,
	NULL, llandertRomInfo, llandertRomName, NULL, NULL, NULL, NULL, LlandertInputInfo, LlandertDIPInfo,
	LlanderInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	640, 480, 4, 3
};
