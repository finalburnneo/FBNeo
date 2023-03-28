// FB Neo Midway MCR driver module
// Based on MAME driver by Aaron Giles

/*
twotigerc	- <<-- todo (alt.inpt) (marked not working/BurnDriverD for now)
dpoker		- don't bother with this (uses light panel/special buttons/etc)
nflfoot		- don't bother with this (laserdisc)
*/

#include "tiles_generic.h"
#include "z80_intf.h"
#include "m6809_intf.h"
#include "midssio.h"
#include "midsat.h" // for dotrone
#include "midtcs.h" // demoderb
#include "dac.h"
#include "ay8910.h"
#include "samples.h"
#include "watchdog.h"
#include "burn_pal.h"
#include "burn_gun.h" // for dial (kick, kroozr, ..), and trackball (wacko)

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvSndPROM;
static UINT8 *DrvNVRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvTCSROM;
static UINT16 *DrvPalRAM16;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 flipscreen;

static INT32 nCyclesExtra[3];

static INT32 nGraphicsLen0;
static INT32 nGraphicsLen1;
static INT32 nMainClock;
static INT32 nScreenFlip = 0;
static INT32 has_ssio;
static INT32 sprite_config;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvJoy5[8];
static UINT8 DrvJoy6[8];
static UINT8 DrvDips[5];
static UINT8 DrvInputs[6];
static UINT8 DrvReset;

static UINT8 DrvJoy4f[8];
static INT16 DrvAnalogPortX = 0;
static INT16 DrvAnalogPortY = 0;
static INT16 DrvAnalogPort2 = 0;
static INT16 DrvAnalogPort3 = 0;
static INT16 DrvAnalogPortZ = 0; // dial

static INT32 input_playernum = 0;

static INT32 has_dial = 0;
static INT32 has_squak = 0;

static INT32 is_kroozr = 0;
static INT32 is_wacko = 0;
static INT32 is_twotiger = 0;
static INT32 is_dotron = 0;
static INT32 is_demoderb = 0;
static INT32 is_kick = 0;

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo SolarfoxInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 6,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 6,	"service"	},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 5,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Solarfox)

static struct BurnInputInfo KickInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Left",		    BIT_DIGITAL,	DrvJoy4f + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy4f + 1,	"p1 right"	},
	A("P1 Dial", 		BIT_ANALOG_REL, &DrvAnalogPortZ,"p1 x-axis"),

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 start"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 6,	"service"	},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 5,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Kick)

static struct BurnInputInfo TapperInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 6,	"service"	},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 5,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Tapper)

static struct BurnInputInfo ShollowInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 start"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 start"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 6,	"service"	},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 5,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Shollow)

static struct BurnInputInfo TronInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy3 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy3 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	A("P1 Dial", 		BIT_ANALOG_REL, &DrvAnalogPortZ,"p1 x-axis"),

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 6,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 7,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 4,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy4 + 7,	"p2 fire 1"	},
	A("P2 Dial", 		BIT_ANALOG_REL, &DrvAnalogPort2,"p2 x-axis"),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 6,	"service"	},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 5,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Tron)

static struct BurnInputInfo KroozrInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 start"	},
	A("P1 Stick X",     BIT_ANALOG_REL, &DrvAnalogPortX,"p1 x-axis"),
	A("P1 Stick Y",     BIT_ANALOG_REL, &DrvAnalogPortY,"p1 y-axis"),
	A("P1 Dial", 		BIT_ANALOG_REL, &DrvAnalogPortZ,"p1 z-axis"),
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 start"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 6,	"service"	},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 5,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Kroozr)

static struct BurnInputInfo DominoInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 6,	"service"	},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 5,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Domino)

static struct BurnInputInfo JourneyInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 6,	"service"	},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 5,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Journey)

static struct BurnInputInfo WackoInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy5 + 3,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy5 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy5 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy5 + 0,	"p1 right"	},
	A("P1 Trackball X", BIT_ANALOG_REL, &DrvAnalogPortX,"p1 x-axis"),
	A("P1 Trackball Y", BIT_ANALOG_REL, &DrvAnalogPortY,"p1 y-axis"),

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy5 + 7,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy5 + 6,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy5 + 5,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy5 + 4,	"p2 right"	},
	A("P2 Trackball X", BIT_ANALOG_REL, &DrvAnalogPort2,"p2 x-axis"),
	A("P2 Trackball Y", BIT_ANALOG_REL, &DrvAnalogPort3,"p2 y-axis"),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 6,	"service"	},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 5,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Wacko)

static struct BurnInputInfo TimberInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 6,	"service"	},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 5,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Timber)

static struct BurnInputInfo TwotigerInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	{"P1 & P2 Dogfight Start", BIT_DIGITAL,	DrvJoy1 + 3,	"p3 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy5 + 2,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy5 + 1,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy5 + 0,	"p1 fire 3"	},
	A("P1 Stick X",     BIT_ANALOG_REL, &DrvAnalogPortX,"p1 x-axis"),

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 2,	"p2 start"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy5 + 5,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy5 + 4,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy5 + 3,	"p2 fire 3"	},
	A("P2 Stick X",     BIT_ANALOG_REL, &DrvAnalogPortY,"p2 x-axis"),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 6,	"service"	},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 5,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Twotiger)

static struct BurnInputInfo DotronInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy3 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy3 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 right"	},
	A("P1 Dial", 		BIT_ANALOG_REL, &DrvAnalogPortZ,"p1 x-axis"),
	A("P1 Aim Analog",  BIT_ANALOG_REL, &DrvAnalogPortY,"p1 y-axis"),
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy3 + 6,	"p1 fire 2"	},
	{"P1 Aim Down",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 fire 3"	},
	{"P1 Aim Up",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 fire 4"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 start"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 6,	"service"	},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 5,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Dotron)

static struct BurnInputInfo DemoderbInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 start"	},
	A("P1 Dial", 		BIT_ANALOG_REL, &DrvAnalogPortZ,"p1 x-axis"),
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 start"	},
	A("P2 Dial", 		BIT_ANALOG_REL, &DrvAnalogPort2,"p2 x-axis"),
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 fire 2"	},

	{"P3 Coin",			BIT_DIGITAL,	DrvJoy5 + 0,	"p3 coin"	},
	{"P3 Start",		BIT_DIGITAL,	DrvJoy5 + 2,	"p3 start"	},
	A("P3 Dial", 		BIT_ANALOG_REL, &DrvAnalogPortX,"p3 x-axis"),
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy5 + 4,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy5 + 5,	"p3 fire 2"	},

	{"P4 Coin",			BIT_DIGITAL,	DrvJoy5 + 1,	"p4 coin"	},
	{"P4 Start",		BIT_DIGITAL,	DrvJoy5 + 3,	"p4 start"	},
	A("P4 Dial", 		BIT_ANALOG_REL, &DrvAnalogPortY,"p4 x-axis"),
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy5 + 6,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy5 + 7,	"p4 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 6,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Demoderb)
#undef A

static struct BurnDIPInfo DemoderbDIPList[]=
{
	{0x16, 0xff, 0xff, 0xff, NULL						},
	{0x17, 0xff, 0xff, 0x20, NULL						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x16, 0x01, 0x01, 0x01, "2P Upright"				},
	{0x16, 0x01, 0x01, 0x00, "4P Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Difficulty"				},
	{0x16, 0x01, 0x02, 0x02, "Normal"					},
	{0x16, 0x01, 0x02, 0x00, "Harder"					},

	{0   , 0xfe, 0   ,    2, "Free Play"				},
	{0x16, 0x01, 0x04, 0x04, "Off"						},
	{0x16, 0x01, 0x04, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Reward Screen"			},
	{0x16, 0x01, 0x08, 0x08, "Expanded"					},
	{0x16, 0x01, 0x08, 0x00, "Limited"					},

	{0   , 0xfe, 0   ,    4, "Coinage"					},
	{0x16, 0x01, 0x30, 0x20, "2 Coins 1 Credits"		},
	{0x16, 0x01, 0x30, 0x00, "2 Coins 2 Credits"		},
	{0x16, 0x01, 0x30, 0x30, "1 Coin  1 Credits"		},
	{0x16, 0x01, 0x30, 0x10, "1 Coin  2 Credits"		},

    {0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x17, 0x01, 0x20, 0x20, "Off"						},
	{0x17, 0x01, 0x20, 0x00, "On"						},
};

STDDIPINFO(Demoderb)

static struct BurnDIPInfo DotronDIPList[]=
{
	{0x11, 0xff, 0xff, 0x80, NULL						},
	{0x12, 0xff, 0xff, 0xff, NULL						},
	{0x13, 0xff, 0xff, 0x80, NULL						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x11, 0x01, 0x80, 0x00, "Environmental"			},
	{0x11, 0x01, 0x80, 0x80, "Upright"					},

	{0   , 0xfe, 0   ,    2, "Coin Meters"				},
	{0x12, 0x01, 0x01, 0x01, "1"						},
	{0x12, 0x01, 0x01, 0x00, "2"						},

    {0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x13, 0x01, 0x80, 0x80, "Off"						},
	{0x13, 0x01, 0x80, 0x00, "On"						},
};

STDDIPINFO(Dotron)

static struct BurnDIPInfo DotroneDIPList[]=
{
	{0x11, 0xff, 0xff, 0x00, NULL						},
	{0x12, 0xff, 0xff, 0xff, NULL						},
	{0x13, 0xff, 0xff, 0x80, NULL						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x11, 0x01, 0x80, 0x00, "Environmental"			},
	{0x11, 0x01, 0x80, 0x80, "Upright"					},

	{0   , 0xfe, 0   ,    2, "Coin Meters"				},
	{0x12, 0x01, 0x01, 0x01, "1"						},
	{0x12, 0x01, 0x01, 0x00, "2"						},

    {0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x13, 0x01, 0x80, 0x80, "Off"						},
	{0x13, 0x01, 0x80, 0x00, "On"						},
};

STDDIPINFO(Dotrone)

static struct BurnDIPInfo TwotigerDIPList[]=
{
	{0x10, 0xff, 0xff, 0xfc, NULL						},
	{0x11, 0xff, 0xff, 0x80, NULL						},

	{0   , 0xfe, 0   ,    2, "Shot Speed"				},
	{0x10, 0x01, 0x01, 0x01, "Fast"						},
	{0x10, 0x01, 0x01, 0x00, "Slow"						},

	{0   , 0xfe, 0   ,    2, "Dogfight"					},
	{0x10, 0x01, 0x02, 0x00, "1 Credit"					},
	{0x10, 0x01, 0x02, 0x02, "2 Credits"				},

    {0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x11, 0x01, 0x80, 0x80, "Off"						},
	{0x11, 0x01, 0x80, 0x00, "On"						},
};

STDDIPINFO(Twotiger)

static struct BurnDIPInfo TimberDIPList[]=
{
	{0x13, 0xff, 0xff, 0xfb, NULL						},
	{0x14, 0xff, 0xff, 0x80, NULL						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x13, 0x01, 0x04, 0x04, "Off"						},
	{0x13, 0x01, 0x04, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x13, 0x01, 0x40, 0x40, "Upright"					},
	{0x13, 0x01, 0x40, 0x00, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Coin Meters"				},
	{0x13, 0x01, 0x80, 0x80, "1"						},
	{0x13, 0x01, 0x80, 0x00, "2"						},

    {0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x14, 0x01, 0x80, 0x80, "Off"						},
	{0x14, 0x01, 0x80, 0x00, "On"						},
};

STDDIPINFO(Timber)

static struct BurnDIPInfo WackoDIPList[]=
{
	{0x13, 0xff, 0xff, 0x3f, NULL						},
	{0x14, 0xff, 0xff, 0x80, NULL						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x13, 0x01, 0x40, 0x00, "Upright"					},
	{0x13, 0x01, 0x40, 0x40, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Coin Meters"				},
	{0x13, 0x01, 0x80, 0x80, "1"						},
	{0x13, 0x01, 0x80, 0x00, "2"						},

    {0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x14, 0x01, 0x80, 0x80, "Off"						},
	{0x14, 0x01, 0x80, 0x00, "On"						},
};

STDDIPINFO(Wacko)

static struct BurnDIPInfo JourneyDIPList[]=
{
	{0x11, 0xff, 0xff, 0xfd, NULL						},
	{0x12, 0xff, 0xff, 0x80, NULL						},

	{0   , 0xfe, 0   ,    2, "Coin Meters"				},
	{0x11, 0x01, 0x01, 0x01, "1"						},
	{0x11, 0x01, 0x01, 0x00, "2"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x11, 0x01, 0x02, 0x00, "Upright"					},
	{0x11, 0x01, 0x02, 0x02, "Cocktail"					},

    {0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x12, 0x01, 0x80, 0x80, "Off"						},
	{0x12, 0x01, 0x80, 0x00, "On"						},
};

STDDIPINFO(Journey)

static struct BurnDIPInfo DominoDIPList[]=
{
	{0x11, 0xff, 0xff, 0x3e, NULL						},
	{0x12, 0xff, 0xff, 0x80, NULL						},

	{0   , 0xfe, 0   ,    2, "Music"					},
	{0x11, 0x01, 0x01, 0x01, "Off"						},
	{0x11, 0x01, 0x01, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Skin Color"				},
	{0x11, 0x01, 0x02, 0x02, "Light"					},
	{0x11, 0x01, 0x02, 0x00, "Dark"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x11, 0x01, 0x40, 0x00, "Upright"					},
	{0x11, 0x01, 0x40, 0x40, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Coin Meters"				},
	{0x11, 0x01, 0x80, 0x80, "1"						},
	{0x11, 0x01, 0x80, 0x00, "2"						},

    {0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x12, 0x01, 0x80, 0x80, "Off"						},
	{0x12, 0x01, 0x80, 0x00, "On"						},
};

STDDIPINFO(Domino)

static struct BurnDIPInfo TronDIPList[]=
{
	{0x13, 0xff, 0xff, 0x80, NULL						},
	{0x14, 0xff, 0xff, 0x80, NULL						},

	{0   , 0xfe, 0   ,    2, "Coin Meters"				},
	{0x13, 0x01, 0x01, 0x01, "1"						},
	{0x13, 0x01, 0x01, 0x00, "2"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x13, 0x01, 0x02, 0x00, "Upright"					},
	{0x13, 0x01, 0x02, 0x02, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Allow Continue"			},
	{0x13, 0x01, 0x04, 0x04, "No"						},
	{0x13, 0x01, 0x04, 0x00, "Yes"						},

    {0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x14, 0x01, 0x80, 0x80, "Off"						},
	{0x14, 0x01, 0x80, 0x00, "On"						},
};

STDDIPINFO(Tron)

static struct BurnDIPInfo KroozrDIPList[]=
{
	{0x0c, 0xff, 0xff, 0xbf, NULL						},
	{0x0d, 0xff, 0xff, 0x80, NULL						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x0c, 0x01, 0x40, 0x00, "Upright"					},
	{0x0c, 0x01, 0x40, 0x40, "Cocktail"					},

    {0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x0d, 0x01, 0x80, 0x80, "Off"						},
	{0x0d, 0x01, 0x80, 0x00, "On"						},
};

STDDIPINFO(Kroozr)

static struct BurnDIPInfo SolarfoxDIPList[]=
{
	{0x11, 0xff, 0xff, 0xef, NULL						},
	{0x12, 0xff, 0xff, 0x80, NULL						},

	{0   , 0xfe, 0   ,    3, "Bonus"					},
	{0x11, 0x01, 0x03, 0x02, "None"						},
	{0x11, 0x01, 0x03, 0x03, "After 10 racks"			},
	{0x11, 0x01, 0x03, 0x01, "After 20 racks"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x11, 0x01, 0x10, 0x10, "Off"						},
	{0x11, 0x01, 0x10, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Ignore Hardware Failure"	},
	{0x11, 0x01, 0x40, 0x40, "Off"						},
	{0x11, 0x01, 0x40, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x11, 0x01, 0x80, 0x80, "Upright"					},
	{0x11, 0x01, 0x80, 0x00, "Cocktail"					},

    {0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x12, 0x01, 0x80, 0x80, "Off"						},
	{0x12, 0x01, 0x80, 0x00, "On"						},
};

STDDIPINFO(Solarfox)

static struct BurnDIPInfo KickDIPList[]=
{
	{0x0b, 0xff, 0xff, 0xfe, NULL						},
	{0x0c, 0xff, 0xff, 0x80, NULL						},

	{0   , 0xfe, 0   ,    2, "Music"					},
	{0x0b, 0x01, 0x01, 0x01, "Off"						},
	{0x0b, 0x01, 0x01, 0x00, "On"						},

    {0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x0c, 0x01, 0x80, 0x80, "Off"						},
	{0x0c, 0x01, 0x80, 0x00, "On"						},
};

STDDIPINFO(Kick)

static struct BurnDIPInfo TapperDIPList[]=
{
	{0x11, 0xff, 0xff, 0xfb, NULL						},
	{0x12, 0xff, 0xff, 0x80, NULL						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x11, 0x01, 0x04, 0x04, "Off"						},
	{0x11, 0x01, 0x04, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x11, 0x01, 0x40, 0x40, "Upright"					},
	{0x11, 0x01, 0x40, 0x00, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Coin Meters"				},
	{0x11, 0x01, 0x80, 0x80, "1"						},
	{0x11, 0x01, 0x80, 0x00, "2"						},

    {0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x12, 0x01, 0x80, 0x80, "Off"						},
	{0x12, 0x01, 0x80, 0x00, "On"						},
};

STDDIPINFO(Tapper)

static struct BurnDIPInfo ShollowDIPList[]=
{
	{0x0f, 0xff, 0xff, 0xfd, NULL						},
	{0x10, 0xff, 0xff, 0x80, NULL						},

    {0   , 0xfe, 0   ,    2, "Coin Meters"				},
	{0x0f, 0x01, 0x01, 0x01, "1"						},
	{0x0f, 0x01, 0x01, 0x00, "2"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x0f, 0x01, 0x02, 0x00, "Upright"					},
	{0x0f, 0x01, 0x02, 0x02, "Cocktail"					},

    {0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x10, 0x01, 0x80, 0x80, "Off"						},
	{0x10, 0x01, 0x80, 0x00, "On"						},
};

STDDIPINFO(Shollow)

static void __fastcall mcr_90009_write(UINT16 address, UINT8 data)
{
	if (address >= 0xf400 && address <= 0xfbff) {
		INT32 select = (address >> 8) & 8;
		address = address & 0x1f;
		DrvPalRAM16[address] &= ~(0xff << select);
		DrvPalRAM16[address] |= data << select;
		return;
	}
}

static void __fastcall mcr_90010_write(UINT16 address, UINT8 data)
{
	if ((address & 0xe800) == 0xe800) {
		DrvVidRAM[address & 0x7ff] = data;
	
		if ((address & 0xef80) == 0xef80) {
			address &= 0x7f;
			DrvPalRAM16[address/2] = data | ((address & 1) << 8);
		}
		return;
	}
}

static void __fastcall mcr_91490_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf800) == 0xf800) {
		address &= 0x7f;
		DrvPalRAM16[address/2] = data | ((address & 1) << 8);
		return;
	}
}

static void __fastcall twotiger_vidram_write(UINT16 address, UINT8 data)
{
	if ((address & 0xe800) == 0xe800) {
		INT32 offs = ((address & 0x3ff) << 1) | ((address & 0x400) >> 10);
		DrvVidRAM[offs] = data;

		if ((offs & 0x780) == 0x780) {
			DrvPalRAM16[((address & 0x400) >> 5) | ((address >> 1) & 0x1f)] = data | ((address & 1) << 8);
			return;
		}
	}
}

static UINT8 __fastcall twotiger_vidram_read(UINT16 address)
{
	if ((address & 0xe800) == 0xe800) {
		INT32 offs = ((address & 0x3ff) << 1) | ((address & 0x400) >> 10);
		return DrvVidRAM[offs];
	}

	return 0;
}

static void __fastcall mcr_write_port(UINT16 address, UINT8 data)
{
	switch (address & 0xff)
	{
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
			flipscreen = (data >> 6) & 1;
			// coin cointers..
		break; // use ssio_write_ports (below)

		case 0xe0:
			BurnWatchdogWrite();
		return;

        case 0xe8:
		return; // nop

		case 0xf0:
		case 0xf1:
		case 0xf2:
		case 0xf3:
            z80ctc_write(address & 3, data);
		return;
	}

	ssio_write_ports(address, data);
}

static UINT8 __fastcall mcr_read_port(UINT16 address)
{
	switch (address & 0xff)
	{
		case 0xf0:
		case 0xf1:
		case 0xf2:
		case 0xf3:
			return z80ctc_read(address & 3);
	}

	return ssio_read_ports(address);
}

static UINT8 solarfox_ip0_read(UINT8)
{
    return DrvInputs[0]; // wrong! (hint: mux)
}

static UINT8 solarfox_ip1_read(UINT8)
{
    return DrvInputs[1]; // wrong!
}

static void solarfox_op0_write(UINT8 , UINT8 data)
{
	flipscreen = (data >> 6) & 1;
}

static UINT8 kroozr_ip1_read(UINT8)
{
    UINT8 tb = BurnTrackballRead(0, 0);

	return ((tb & 0x80) >> 1) | ((tb & 0x70) >> 4);
}

static UINT8 kick_ip1_read(UINT8)
{
	UINT8 tb = (BurnTrackballRead(0, 0) & 0x0f) | 0xf0;
	BurnTrackballUpdate(0);

    return tb;
}

static tilemap_callback( bg90009 )
{
	TILE_SET_INFO(0, DrvVidRAM[offs], 0, TILE_GROUP(1));
}

static tilemap_callback( bg90010 )
{
	INT32 attr = DrvVidRAM[offs * 2 + 1];
	INT32 code = DrvVidRAM[offs * 2 + 0] | (attr * 256); 

	TILE_SET_INFO(0, code, attr >> 3, TILE_FLIPYX(attr >> 1) | TILE_GROUP(attr >> 6));
}

static tilemap_callback( bg91490 )
{
	INT32 attr = DrvVidRAM[offs * 2 + 1];
	INT32 code = DrvVidRAM[offs * 2 + 0] | (attr * 256); 

	TILE_SET_INFO(0, code, attr >> 4, TILE_FLIPYX(attr >> 2) | TILE_GROUP(attr >> 6));
}

static void ctc_interrupt(INT32 state)
{
    ZetSetIRQLine(0, state ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static void ctc_trigger(INT32 , UINT8 data)
{
	z80ctc_trg_write(1, data);
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0x00, RamEnd - AllRam);
	}

	ZetOpen(0);
	ZetReset();
	ZetClose();

	BurnSampleReset();
	ssio_reset();
	if (has_squak) midsat_reset();
	tcs_reset();

	HiscoreReset();

	HiscoreReset();

	flipscreen = 0;

	nCyclesExtra[0] = nCyclesExtra[1] = nCyclesExtra[2] = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM1		= Next; Next += 0x010000;
	DrvZ80ROM0		= Next; Next += 0x010000;
	DrvTCSROM		= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += nGraphicsLen0 * 2 * 2 * 2;
	DrvGfxROM1		= Next; Next += nGraphicsLen1 * 2;

	DrvSndPROM		= Next; Next += 0x0000200;

	DrvPalette		= (UINT32*)Next; Next += 0x80 * sizeof(UINT32);

    DrvNVRAM		= Next; Next += 0x000800; // this is work-ram and nvram (HS, service mode options)

	AllRam			= Next;

	DrvSprRAM		= Next; Next += 0x000200;
	DrvVidRAM		= Next; Next += 0x000800;
	DrvZ80RAM1		= Next; Next += 0x001000; // 0x400 ssio, 0x1000 tcs
	DrvPalRAM16		= (UINT16*)Next; Next += 0x40 * sizeof(UINT16);

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[4] = { ((nGraphicsLen0/2)*8)+0, ((nGraphicsLen0/2)*8)+1, 0, 1 };
	INT32 XOffs0[8] = { STEP8(0,2) };
	INT32 YOffs0[8] = { STEP8(0,16) };

	INT32 L = (nGraphicsLen1 / 4) * 8;
	INT32 Plane1[4]  = { STEP4(0,1) };
	INT32 XOffs1[32] = {
		L*0+0, L*0+4, L*1+0, L*1+4, L*2+0, L*2+4, L*3+0, L*3+4,
		L*0+0+8, L*0+4+8, L*1+0+8, L*1+4+8, L*2+0+8, L*2+4+8, L*3+0+8, L*3+4+8,
		L*0+0+16, L*0+4+16, L*1+0+16, L*1+4+16, L*2+0+16, L*2+4+16, L*3+0+16, L*3+4+16,
		L*0+0+24, L*0+4+24, L*1+0+24, L*1+4+24, L*2+0+24, L*2+4+24, L*3+0+24, L*3+4+24
	};
	INT32 YOffs1[32] = { STEP32(0,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc((nGraphicsLen1 > nGraphicsLen0) ? nGraphicsLen1 : nGraphicsLen0);
	if (tmp == NULL) {
		return 1;
	}

	GfxDecode((nGraphicsLen0 * 2) / (8 * 8), 4, 8, 8, Plane0, XOffs0, YOffs0, 0x080, DrvGfxROM0, tmp);

	for (INT32 i = 0; i < nGraphicsLen0*2; i+=0x40) { // 2x size and invert pixel
		for (INT32 y = 0; y < 16; y++) {
			for (INT32 x = 0; x < 16; x++) {
				DrvGfxROM0[(i * 4) + (y * 16) + x] = tmp[i + ((y / 2) * 8) + (x / 2)];
			}
		}
	}

	memcpy (tmp, DrvGfxROM1, nGraphicsLen1);

	GfxDecode((nGraphicsLen1 * 2) / (32 * 32), 4, 32, 32, Plane1, XOffs1, YOffs1, 0x400, tmp, DrvGfxROM1);

	BurnFree(tmp);

	return 0;
}

static void map_90009()
{
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0x6fff, MAP_ROM);
	ZetMapMemory(DrvNVRAM,			0x7000, 0x77ff, MAP_RAM);
	ZetMapMemory(DrvNVRAM,			0x7800, 0x7fff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0xf000, 0xf1ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0xf200, 0xf3ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,			0xfc00, 0xffff, MAP_RAM);
	ZetSetWriteHandler(mcr_90009_write);
	ZetSetOutHandler(mcr_write_port);
	ZetSetInHandler(mcr_read_port);

	nMainClock = 2496000;
}

static UINT8 __fastcall mcr_read_unmapped(UINT16 address)
{
	return 0xff;
}

static void map_90010()
{
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0xbfff, MAP_ROM);
    for (INT32 i = 0; i < 0x2000; i+=0x0800) {
		ZetMapMemory(DrvNVRAM,		0xc000 + i + 0, 0xc7ff + i + 0, MAP_RAM);
	}
	for (INT32 i = 0; i < 0x2000; i+=0x1000) {
		for (INT32 j = 0; j < 0x800; j+= 0x200) {
			ZetMapMemory(DrvSprRAM,	0xe000 + i + j, 0xe1ff + i + j, MAP_RAM);
		}
		ZetMapMemory(DrvVidRAM,		0xe800 + i + 0, 0xefff + i + 0, MAP_ROM);
    }
	ZetSetWriteHandler(mcr_90010_write);
	ZetSetReadHandler(mcr_read_unmapped);
	ZetSetOutHandler(mcr_write_port);
	ZetSetInHandler(mcr_read_port);

	nMainClock = 2496000;
}

static void map_91490()
{
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0xdfff, MAP_ROM);
	ZetMapMemory(DrvNVRAM,			0xe000, 0xe7ff, MAP_RAM);
    ZetMapMemory(DrvSprRAM,			0xe800, 0xe9ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0xea00, 0xebff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,			0xf000, 0xf7ff, MAP_RAM);
	ZetSetWriteHandler(mcr_91490_write);
	ZetSetReadHandler(mcr_read_unmapped);
	ZetSetOutHandler(mcr_write_port);
	ZetSetInHandler(mcr_read_port);

	nMainClock = 5000000;
}

static INT32 DrvLoadRoms(bool bLoad)
{
	char* pRomName;
	struct BurnRomInfo ri;
	UINT8 *pLoad[3] = { DrvZ80ROM0, DrvZ80ROM1, DrvTCSROM };
	UINT8 *gLoad[2] = { DrvGfxROM0, DrvGfxROM1 };

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++)
	{
		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & BRF_PRG) && ((ri.nType & 7) == 1 || (ri.nType & 7) == 2 || (ri.nType & 7) == 3)) {
			INT32 type = (ri.nType - 1) & 3;
            //bprintf(0, _T("loading %S, type %d\n"), pRomName, type);
            if (bLoad) if (BurnLoadRom(pLoad[type], i, 1)) return 1;
			pLoad[type] += ri.nLen;
			continue;
		}

		if ((ri.nType & BRF_GRA) && ((ri.nType & 7) == 3 || (ri.nType & 7) == 4)) {
			INT32 type = (ri.nType - 3) & 1;
			if (bLoad) if (BurnLoadRom(gLoad[type], i, 1)) return 1;
			gLoad[type] += ri.nLen;
			continue;
		}
	}

	INT32 prg0 = pLoad[0] - DrvZ80ROM0;
	INT32 prg1 = pLoad[1] - DrvZ80ROM1;
	INT32 prg2 = pLoad[2] - DrvTCSROM;
	nGraphicsLen0 = gLoad[0] - DrvGfxROM0;
	nGraphicsLen1 = gLoad[1] - DrvGfxROM1;
    if (bLoad) {
        bprintf (0, _T("PRG0: %x, PRG1: %x, GFX0: %x, GFX1: %x, PRG2: %x\n"),  prg0, prg1, nGraphicsLen0, nGraphicsLen1, prg2);
    }
	if (nGraphicsLen1 & 0x20) nGraphicsLen1 -= 0x20; // wtfff???

	has_ssio = (prg1) ? 1 : 0;

	return 0;
}

static INT32 DrvInit(INT32 cpu_board)
{
	BurnSetRefreshRate(30.00);

	DrvZ80ROM0 = DrvZ80ROM1 = DrvTCSROM = 0;
	DrvGfxROM0 = DrvGfxROM1 = 0;

	DrvLoadRoms(false);

	BurnAllocMemIndex();

	memset (DrvNVRAM, 0xff, 0x800);

	if (DrvLoadRoms(true)) return 1;

	if (BurnLoadRom(DrvSndPROM, 0x80, 1)) return 1; // load sound prom!

	DrvGfxDecode();

	GenericTilesInit();
	GenericTilemapSetGfx(0, DrvGfxROM0, 4, 16, 16, nGraphicsLen0 * 8, 0, 3);

	ZetInit(0);
	ZetOpen(0);

    ZetDaisyInit(Z80_CTC, 0);
	z80ctc_init(nMainClock, 0, ctc_interrupt, ctc_trigger, NULL, NULL);

	switch (cpu_board)
	{
		case 90009:
			map_90009();
			GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg90009_map_callback, 16, 16, 32, 30);
			sprite_config = 0x000000;
		break;

		case 90010:
			map_90010();
			GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg90010_map_callback, 16, 16, 32, 30);
			sprite_config = 0x000000;
		break;

		case 91490: // tapper
			map_91490();
			GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg91490_map_callback, 16, 16, 32, 30);
			sprite_config = 0x003000;
		break;

		case 91475: // journey
			map_90010();
			GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg90010_map_callback, 16, 16, 32, 30);
			sprite_config = 0x003040;
		break;

		default: bprintf (0, _T("No map selected!!\n"));
	}

	ZetClose();

	BurnSampleInit(1);
	BurnSampleSetAllRoutesAllSamples(0.80, BURN_SND_ROUTE_BOTH);

	ssio_init(DrvZ80ROM1, DrvZ80RAM1, DrvSndPROM);

    if (has_squak) {
        bprintf(0, _T("Has squak n talk or tcs.\n"));
        midsat_init(DrvTCSROM);
    }

	BurnWatchdogInit(DrvDoReset, 1180);

    BurnTrackballInit(2); // kick

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	ZetExit();
	ssio_exit();

    if (has_squak) midsat_exit();
	tcs_exit();

	BurnSampleExit();

    BurnTrackballExit();

	BurnFreeMemIndex();

	nScreenFlip = 0;
	sprite_config = 0;

    input_playernum = 0;

    has_dial = 0;

    is_kroozr = 0;
    is_wacko = 0;
    is_twotiger = 0;
    is_dotron = 0;
    is_demoderb = 0;
	is_kick = 0;

	return 0;
}

static void DrvPaletteUpdate(INT32 type)
{
	if (type == 0)
	{
		for (INT32 i = 0; i < BurnDrvGetPaletteEntries(); i++)
		{
			UINT8 r = pal4bit((DrvPalRAM16[i] >> 8) & 0xf);
			UINT8 g = pal4bit(DrvPalRAM16[i] & 0xf);
			UINT8 b = pal4bit((DrvPalRAM16[i] >> 4)&0xf);

			DrvPalette[i] = BurnHighCol(r,g,b,0);
		}
	}
	else if (type == 1)
	{
		for (INT32 i = 0; i < BurnDrvGetPaletteEntries(); i++)
		{
			UINT8 r = pal3bit((DrvPalRAM16[i] >> 6) & 7);
			UINT8 g = pal3bit(DrvPalRAM16[i] & 7);
			UINT8 b = pal3bit((DrvPalRAM16[i] >> 3) & 7);

			DrvPalette[i] = BurnHighCol(r,g,b,0);
		}
	}
	else if (type == 2) // journey
	{
		for (INT32 i = 0; i < BurnDrvGetPaletteEntries()/2; i++)
		{
			UINT8 r = (DrvPalRAM16[i] >> 6) & 7;
			UINT8 g = (DrvPalRAM16[i] >> 0) & 7;
			UINT8 b = (DrvPalRAM16[i] >> 3) & 7;

			r = (r << 5) | (r << 1);
			g = (g << 5) | (g << 1);
			b = (b << 5) | (b << 1);

			DrvPalette[i] = BurnHighCol(r,g,b,0);

			if ((i & 0x31) == 0x31)
			{
				r |= 0x11;
				g |= 0x11;
				b |= 0x11;
			}

			DrvPalette[i + 0x40] = BurnHighCol(r,g,b,0);
		}
	}
}

static void render_sprites_91399()
{
	for (INT32 offs = 0; offs < 0x200; offs += 4)
	{
		INT32 code = DrvSprRAM[offs + 1] & 0x3f;
		INT32 hflip = (DrvSprRAM[offs + 1] & 0x40) ? 31 : 0;
		INT32 vflip = (DrvSprRAM[offs + 1] & 0x80) ? 31 : 0;
		INT32 sx = (DrvSprRAM[offs + 2] - 4) * 2;
		INT32 sy = (240 - DrvSprRAM[offs]) * 2;

		/* apply cocktail mode */
		if (flipscreen)
		{
			hflip ^= 31;
			vflip ^= 31;
			sx = 466 - sx; // + m_mcr12_sprite_xoffs_flip;
			sy = 450 - sy;
		}
		else
			sx += 0; //m_mcr12_sprite_xoffs;

		if (nScreenFlip & TMAP_FLIPY) {
			vflip ^= 31;
			sy = 450 - sy;
		}
		if (nScreenFlip & TMAP_FLIPX) {
			hflip ^= 31;
			sx = 466 - sx;
		}

		sx &= 0x1ff;
		sy &= 0x1ff;

		for (int y = 0; y < 32; y++, sy = (sy + 1) & 0x1ff)
		{
			if (sy >= 0 && sy < nScreenHeight)
			{
				const UINT8 *src = DrvGfxROM1 + (code * 0x400) + 32 * (y ^ vflip);
				UINT16 *dst = pTransDraw + (sy * nScreenWidth);
				UINT8 *pri = pPrioDraw + (sy * nScreenWidth);

				for (INT32 x = 0; x < 32; x++)
				{
					INT32 tx = (sx + x) & 0x1ff;
					if (tx >= nScreenWidth) continue;
					INT32 pix = pri[tx] | src[x ^ hflip];

					pri[tx] = pix;

					if (pix & 0x07)
						dst[tx] = pix;
				}
			}
		}
	}
}

static void render_sprites_91464(int primask, int sprmask, int colormask)
{
	INT32 codemask = (nGraphicsLen1 * 2) / (32 * 32);

	for (int offs = 0x200 - 4; offs >= 0; offs -= 4)
	{
		/* extract the bits of information */
		int code = (DrvSprRAM[offs + 2] + 256 * ((DrvSprRAM[offs + 1] >> 3) & 0x01)) % codemask;
		int color = (((~DrvSprRAM[offs + 1] & 3) << 4) & sprmask) | colormask;
		int hflip = (DrvSprRAM[offs + 1] & 0x10) ? 31 : 0;
		int vflip = (DrvSprRAM[offs + 1] & 0x20) ? 31 : 0;
		int sx = (DrvSprRAM[offs + 3] - 3) * 2;
		int sy = (241 - DrvSprRAM[offs]) * 2;

		/* apply cocktail mode */
		if (flipscreen)
		{
			hflip ^= 31;
			vflip ^= 31;
			sx = 480 - sx;
			sy = 452 - sy;
		}

		if (nScreenFlip & TMAP_FLIPY) {
			vflip ^= 31;
			sy = 452 - sy;
		}
		if (nScreenFlip & TMAP_FLIPX) {
			hflip ^= 31;
			sx = 480 - sx;
		}

		/* clamp within 512 */
		sx &= 0x1ff;
		sy &= 0x1ff;

		/* loop over lines in the sprite */
		for (int y = 0; y < 32; y++, sy = (sy + 1) & 0x1ff)
		{
			if (sy >= 2 && sy >= 0 && sy < nScreenHeight)
			{
				const UINT8 *src = DrvGfxROM1 + (code * 0x400) + 32 * (y ^ vflip);
				UINT16 *dst = pTransDraw + (sy * nScreenWidth);
				UINT8 *pri = pPrioDraw + (sy * nScreenWidth);

				/* loop over columns */
				for (int x = 0; x < 32; x++)
				{
					int tx = (sx + x) & 0x1ff;
					if (tx >= nScreenWidth) continue;

					int pix = pri[tx];
					if (pix != 0xff)
					{
						/* compute the final value */
						pix = (pix & primask) | color | src[x ^ hflip];

						/* if non-zero, draw */
						if (pix & 0x0f)
						{
							/* mark this pixel so we don't draw there again */
							pri[tx] = 0xff;

							/* only draw if the low 3 bits are set */
							if (pix & 0x07)
								dst[tx] = pix;
						}
					}
				}
			}
		}
	}
}

static INT32 DrvDraw()
{
	//if (DrvRecalc) {
		DrvPaletteUpdate(BurnDrvGetPaletteEntries()/0x40);
		DrvRecalc = 1;
	//}

	GenericTilemapSetFlip(0, (flipscreen ? (TMAP_FLIPX | TMAP_FLIPY) : 0) ^ nScreenFlip);

	BurnTransferClear();

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0x00 | TMAP_SET_GROUP(0));
	if (nBurnLayer & 2) GenericTilemapDraw(0, pTransDraw, 0x10 | TMAP_SET_GROUP(1));
	if (nBurnLayer & 4) GenericTilemapDraw(0, pTransDraw, 0x20 | TMAP_SET_GROUP(2));
	if (nBurnLayer & 8) GenericTilemapDraw(0, pTransDraw, 0x30 | TMAP_SET_GROUP(3));

	if (sprite_config == 0)
	{
        if (nSpriteEnable & 1) render_sprites_91399();
	}
	else
	{
        if (nSpriteEnable & 1) render_sprites_91464((sprite_config >> 16) & 0xff, (sprite_config >> 8) & 0xff, sprite_config & 0xff);
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void mcr_interrupt(INT32 scanline)
{
    if (scanline == 0 || scanline == 240)
	{
		z80ctc_trg_write(2, 1);
		z80ctc_trg_write(2, 0);
	}
	if (scanline == 0)
	{
		z80ctc_trg_write(3, 1);
		z80ctc_trg_write(3, 0);
	}
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	ZetNewFrame();
    if (has_squak) midsatNewFrame();
    INT32 has_tcs = tcs_initialized();
    if (has_tcs) M6809NewFrame();

	{
		memset (DrvInputs, 0xff, 6);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
			DrvInputs[4] ^= (DrvJoy5[i] & 1) << i;
			DrvInputs[5] ^= (DrvJoy6[i] & 1) << i;
		}

        ssio_inputs = DrvInputs;

        DrvInputs[3] = (is_dotron) ? DrvDips[1] : DrvDips[0];
        if (is_demoderb) {
            DrvInputs[0] = (DrvInputs[0] & ~0x20) | (DrvDips[1] & 0x20); // service mode
        } else {
            DrvInputs[0] = (DrvInputs[0] & ~0x80) | (DrvDips[((is_dotron) ? 2 : 1)] & 0x80); // service mode
        }
        ssio_dips = 0xff; // always 0xff in every mcr game

        {
            if (has_dial) { // kick, kroozr, tron, dotron
				BurnTrackballConfig(0, AXIS_REVERSED, AXIS_REVERSED);
				if (is_kick) {
					BurnTrackballFrame(0, DrvAnalogPortZ, DrvAnalogPort2, 1, 3);
				} else {
					BurnTrackballFrame(0, DrvAnalogPortZ, DrvAnalogPort2, 2, 7);
				}
                BurnTrackballUDLR(0, 0, 0, DrvJoy4f[0], DrvJoy4f[1]);
                BurnTrackballUpdate(0);
            }

            if (is_demoderb) {
                BurnTrackballConfig(0, AXIS_NORMAL, AXIS_NORMAL);
                BurnTrackballFrame(0, DrvAnalogPortZ, DrvAnalogPort2, 2, 5);
                BurnTrackballUpdate(0);

                BurnTrackballConfig(1, AXIS_NORMAL, AXIS_NORMAL);
                BurnTrackballFrame(1, DrvAnalogPortX, DrvAnalogPortY, 2, 5);
                BurnTrackballUpdate(1);
            }

            if (is_dotron) { // dotron up/down "aim" analog
				UINT8 an = ProcessAnalog(DrvAnalogPortY, 0, INPUT_DEADZONE, 0x00, 0xff);

				if (an > (0x80+10) || an < (0x80-10)) {
                    DrvInputs[2] |= 0x30; // processing analog: ignore digital buttons
                    if (an < (0x80-10)) DrvInputs[2] ^= ( 1 << 4 ); // down
                    if (an > (0x80+10)) DrvInputs[2] ^= ( 1 << 5 ); // up
                }
            }

            if (is_kroozr) {
                DrvInputs[2] = ProcessAnalog(DrvAnalogPortX, 0, INPUT_DEADZONE, 0x30, 0x98);
                DrvInputs[4] = ProcessAnalog(DrvAnalogPortY, 0, INPUT_DEADZONE, 0x30, 0x98);
            }

            if (is_twotiger) {
                DrvInputs[2] = ProcessAnalog(DrvAnalogPortX, 0, INPUT_DEADZONE, 0x00, 0xce);
                DrvInputs[1] = ProcessAnalog(DrvAnalogPortY, 0, INPUT_DEADZONE, 0x00, 0xce);
            }

            if (is_wacko) {
                BurnTrackballConfig(0, AXIS_NORMAL, AXIS_REVERSED);
                BurnTrackballFrame(0, DrvAnalogPortX, DrvAnalogPortY, 0x03, 0x07);
                BurnTrackballUpdate(0);

                BurnTrackballConfig(1, AXIS_NORMAL, AXIS_REVERSED);
                BurnTrackballFrame(1, DrvAnalogPort2, DrvAnalogPort3, 0x03, 0x07);
                BurnTrackballUpdate(1);
            }
        }
	}

    INT32 nInterleave = 480;
	INT32 nCyclesTotal[3] = { nMainClock / 30, 2000000 / 30, 3579545 / 4 / 30 };
	INT32 nCyclesDone[3] = { nCyclesExtra[0], nCyclesExtra[1], nCyclesExtra[2] };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		CPU_RUN(0, Zet);
        mcr_interrupt(i);
		ZetClose();

        if (has_ssio)
		{
			ZetOpen(1);
			CPU_RUN(1, Zet);
			ssio_14024_clock(nInterleave);
			ZetClose();
		}

		if (has_squak) {
			CPU_RUN(2, midsat);
        }

        if (has_tcs) {
            M6809Open(0);
            if (tcs_reset_status())
            {
				CPU_IDLE(1, M6809);
            }
            else
			{
				CPU_RUN(1, M6809);
            }
            M6809Close();
        }
	}

	nCyclesExtra[0] = nCyclesDone[0] - nCyclesTotal[0];
	if (has_ssio || has_tcs) {
		nCyclesExtra[1] = nCyclesDone[1] - nCyclesTotal[1];
	}
	if (has_squak) {
		nCyclesExtra[2] = nCyclesDone[2] - nCyclesTotal[2];
	}

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
        BurnSampleRender(pBurnSoundOut, nBurnSoundLen);
        if (has_squak) {
            midsat_update(pBurnSoundOut, nBurnSoundLen);
        }
        if (has_tcs) {
            DACUpdate(pBurnSoundOut, nBurnSoundLen);
		}
		BurnSoundTweakVolume(pBurnSoundOut, nBurnSoundLen, 0.55);
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

        ScanVar(DrvNVRAM, 0x800, "WORK RAM"); // also nv...

        ZetScan(nAction);

        ssio_scan(nAction, pnMin);
        if (has_squak) midsat_scan(nAction, pnMin);
        if (tcs_initialized()) tcs_scan(nAction, pnMin);

		BurnSampleScan(nAction, pnMin);

		BurnTrackballScan();

		SCAN_VAR(input_playernum);

		SCAN_VAR(nCyclesExtra);
	}

    if (nAction & ACB_NVRAM) {
        ScanVar(DrvNVRAM, 0x800, "NV RAM");
	}

	return 0;
}



static struct BurnRomInfo emptyRomDesc[] = {
	{ "",                    0,          0, 0 },
};

static struct BurnRomInfo SsiopromRomDesc[] = {
	{ "82s123.12d",						0x0020, 0xe1281ee9, BRF_SND | BRF_BIOS },
};

STD_ROM_PICK(Ssioprom)
STD_ROM_FN(Ssioprom)

struct BurnDriver BurnDrvSsioprom = {
    "midssio", NULL, NULL, NULL, "1981",
    "Midway SSIO Sound Board Internal pROM\0", "Internal pROM only", "Midway", "SSIO",
    NULL, NULL, NULL, NULL,
    BDF_BOARDROM, 0, HARDWARE_MISC_PRE90S, GBF_BIOS, 0,
    NULL, SsiopromRomInfo, SsiopromRomName, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, 0,
    480, 512, 3, 4
};


// Solar Fox (upright)

static struct BurnRomInfo solarfoxRomDesc[] = {
	{ "sfcpu.3b",		0x1000, 0x8c40f6eb, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "sfcpu.4b",		0x1000, 0x4d47bd7e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sfcpu.5b",		0x1000, 0xb52c3bd5, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sfcpu.4d",		0x1000, 0xbd5d25ba, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "sfcpu.5d",		0x1000, 0xdd57d817, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "sfcpu.6d",		0x1000, 0xbd993cd9, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "sfcpu.7d",		0x1000, 0x8ad8731d, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "sfsnd.7a",		0x1000, 0xcdecf83a, 2 | BRF_PRG | BRF_ESS }, //  7 Z80 #1 Code (SSIO)
	{ "sfsnd.8a",		0x1000, 0xcb7788cb, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "sfsnd.9a",		0x1000, 0x304896ce, 2 | BRF_PRG | BRF_ESS }, //  9

	{ "sfcpu.4g",		0x1000, 0xba019a60, 3 | BRF_GRA },           // 10 Background Tiles
	{ "sfcpu.5g",		0x1000, 0x7ff0364e, 3 | BRF_GRA },           // 11

	{ "sfvid.1a",		0x2000, 0x9d9b5d7e, 4 | BRF_GRA },           // 12 Sprites
	{ "sfvid.1b",		0x2000, 0x78801e83, 4 | BRF_GRA },           // 13
	{ "sfvid.1d",		0x2000, 0x4d8445cf, 4 | BRF_GRA },           // 14
	{ "sfvid.1e",		0x2000, 0x3da25495, 4 | BRF_GRA },           // 15
};

STDROMPICKEXT(solarfox, solarfox, Ssioprom)
STD_ROM_FN(solarfox)

static INT32 SolarfoxInit()
{
	INT32 nRet = DrvInit(90009);

	nScreenFlip = TMAP_FLIPY;

	if (nRet == 0)
	{
		ssio_set_custom_input(0, 0x1c, solarfox_ip0_read);
		ssio_set_custom_input(1, 0xff, solarfox_ip1_read);
		ssio_set_custom_output(0, 0xff, solarfox_op0_write);
	}

	return nRet;
}

struct BurnDriver BurnDrvSolarfox = {
	"solarfox", NULL, "midssio", NULL, "1981",
	"Solar Fox (upright)\0", NULL, "Bally Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_ACTION, 0,
	NULL, solarfoxRomInfo, solarfoxRomName, NULL, NULL, NULL, NULL, SolarfoxInputInfo, SolarfoxDIPInfo,
	SolarfoxInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x20,
	480, 512, 3, 4
};


// Kick (upright)

static struct BurnRomInfo kickRomDesc[] = {
	{ "1200a-v2.b3",	0x1000, 0x65924917, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "1300b-v2.b4",	0x1000, 0x27929f52, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "1400c-v2.b5",	0x1000, 0x69107ce6, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "1500d-v2.d4",	0x1000, 0x04a23aa1, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "1600e-v2.d5",	0x1000, 0x1d2834c0, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "1700f-v2.d6",	0x1000, 0xddf84ce1, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "4200-a.a7",		0x1000, 0x9e35c02e, 2 | BRF_PRG | BRF_ESS }, //  6 Z80 #1 Code
	{ "4300-b.a8",		0x1000, 0xca2b7c28, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "4400-c.a9",		0x1000, 0xd1901551, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "4500-d.a10",		0x1000, 0xd36ddcdc, 2 | BRF_PRG | BRF_ESS }, //  9

	{ "1800g-v2.g4",	0x1000, 0xb4d120f3, 3 | BRF_GRA },           // 10 Background Tiles
	{ "1900h-v2.g5",	0x1000, 0xc3ba4893, 3 | BRF_GRA },           // 11

	{ "2600a-v2.1e",	0x2000, 0x2c5d6b55, 4 | BRF_GRA },           // 12 Sprites
	{ "2700b-v2.1d",	0x2000, 0x565ea97d, 4 | BRF_GRA },           // 13
	{ "2800c-v2.1b",	0x2000, 0xf3be56a1, 4 | BRF_GRA },           // 14
	{ "2900d-v2.1a",	0x2000, 0x77da795e, 4 | BRF_GRA },           // 15
};

STDROMPICKEXT(kick, kick, Ssioprom)
STD_ROM_FN(kick)

static INT32 KickInit()
{
	INT32 nRet = DrvInit(90009);

	nScreenFlip = TMAP_FLIPY;

	if (nRet == 0)
	{
		is_kick = 1;
        has_dial = 1;
		ssio_set_custom_input(1, 0xff, kick_ip1_read);
		ssio_set_custom_output(0, 0xff, solarfox_op0_write);
	}

	return nRet;
}

static INT32 KickcInit()
{
    INT32 nRet = KickInit();

    nScreenFlip = 0;

    return nRet;
}

struct BurnDriver BurnDrvKick = {
	"kick", NULL, "midssio", NULL, "1981",
	"Kick (upright)\0", NULL, "Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_BREAKOUT, 0,
	NULL, kickRomInfo, kickRomName, NULL, NULL, NULL, NULL, KickInputInfo, KickDIPInfo,
    KickInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x20,
    480, 512, 3, 4
};


// Kickman (upright)

static struct BurnRomInfo kickmanRomDesc[] = {
	{ "1200-a-ur.b3",	0x1000, 0xd8cd9f0f, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "1300-b-ur.b4",	0x1000, 0x4dee27bb, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "1400-c-ur.b5",	0x1000, 0x06f070c9, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "1500-d-ur.d4",	0x1000, 0x8d95b740, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "1600-e-ur.d5",	0x1000, 0xf24bc0d7, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "1700-f-ur.d6",	0x1000, 0x672361fc, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "4200-a.a7",		0x1000, 0x9e35c02e, 2 | BRF_PRG | BRF_ESS }, //  6 ssio:cpu
	{ "4300-b.a8",		0x1000, 0xca2b7c28, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "4400-c.a9",		0x1000, 0xd1901551, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "4500-d.a10",		0x1000, 0xd36ddcdc, 2 | BRF_PRG | BRF_ESS }, //  9

	{ "1800g-v2.g4",	0x1000, 0xb4d120f3, 3 | BRF_GRA },           // 10 gfx1
	{ "1900h-v2.g5",	0x1000, 0xc3ba4893, 3 | BRF_GRA },           // 11

	{ "2600a-v2.1e",	0x2000, 0x2c5d6b55, 4 | BRF_GRA },           // 12 gfx2
	{ "2700b-v2.1d",	0x2000, 0x565ea97d, 4 | BRF_GRA },           // 13
	{ "2800c-v2.1b",	0x2000, 0xf3be56a1, 4 | BRF_GRA },           // 14
	{ "2900d-v2.1a",	0x2000, 0x77da795e, 4 | BRF_GRA },           // 15
};

STDROMPICKEXT(kickman, kickman, Ssioprom)
STD_ROM_FN(kickman)

struct BurnDriver BurnDrvKickman = {
	"kickman", "kick", "midssio", NULL, "1981",
	"Kickman (upright)\0", NULL, "Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_BREAKOUT, 0,
	NULL, kickmanRomInfo, kickmanRomName, NULL, NULL, NULL, NULL, KickInputInfo, KickDIPInfo,
	KickInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x20,
    480, 512, 3, 4
};


// Kick (cocktail)

static struct BurnRomInfo kickcRomDesc[] = {
	{ "1200-a.b3",	0x1000, 0x22fa42ed, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "1300-b.b4",	0x1000, 0xafaca819, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "1400-c.b5",	0x1000, 0x6054ee56, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "1500-d.d4",	0x1000, 0x263af0f3, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "1600-e.d5",	0x1000, 0xeaaa78a7, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "1700-f.d6",	0x1000, 0xc06c880f, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "4200-a.a7",	0x1000, 0x9e35c02e, 2 | BRF_PRG | BRF_ESS }, //  6 ssio:cpu
	{ "4300-b.a8",	0x1000, 0xca2b7c28, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "4400-c.a9",	0x1000, 0xd1901551, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "4500-d.a10",	0x1000, 0xd36ddcdc, 2 | BRF_PRG | BRF_ESS }, //  9

	{ "1000-g.g4",	0x1000, 0xacdae4f6, 3 | BRF_GRA },           // 10 gfx1
	{ "1100-h.g5",	0x1000, 0xdbb18c96, 3 | BRF_GRA },           // 11

	{ "2600-a.1e",	0x2000, 0x74b409d7, 4 | BRF_GRA },           // 12 gfx2
	{ "2700-b.1d",	0x2000, 0x78eda36c, 4 | BRF_GRA },           // 13
	{ "2800-c.1b",	0x2000, 0xc93e0170, 4 | BRF_GRA },           // 14
	{ "2900-d.1a",	0x2000, 0x91e59383, 4 | BRF_GRA },           // 15
};

STDROMPICKEXT(kickc, kickc, Ssioprom)
STD_ROM_FN(kickc)

struct BurnDriver BurnDrvKickc = {
	"kickc", "kick", "midssio", NULL, "1981",
	"Kick (cocktail)\0", NULL, "Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_BREAKOUT, 0,
	NULL, kickcRomInfo, kickcRomName, NULL, NULL, NULL, NULL, KickInputInfo, KickDIPInfo,//, KickcInputInfo, KickcDIPInfo,
	KickcInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x20,
    480, 512, 3, 4
};


// Draw Poker (Bally, 03-20)

static struct BurnRomInfo dpokerRomDesc[] = {
	{ "vppp.b3",	0x1000, 0x2a76ded2, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "vppp.b4",	0x1000, 0xd6948faa, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "vppp.b5",	0x1000, 0xa49916e5, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "vppp.d4",	0x1000, 0xc496934f, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "vppp.d5",	0x1000, 0x84f4bd38, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "vppp.d6",	0x1000, 0xb0023bf1, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "vppp.d7",	0x1000, 0xa4012f5a, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "vssp.a7",	0x1000, 0xf78b2283, 2 | BRF_PRG | BRF_ESS }, //  7 ssio:cpu
	{ "vssp.a8",	0x1000, 0x3f531bd0, 2 | BRF_PRG | BRF_ESS }, //  8

	{ "vpbg.g4",	0x1000, 0x9fe9aad8, 3 | BRF_GRA },           //  9 gfx1
	{ "vpbg.g5",	0x1000, 0xd43aeaae, 3 | BRF_GRA },           // 10

	{ "vpfg.a1",	0x2000, 0xd76ec7dd, 4 | BRF_GRA },           // 11 gfx2
	{ "vpfg.b1",	0x2000, 0xcdba9a7d, 4 | BRF_GRA },           // 12
	{ "vpfg.d1",	0x2000, 0xc661cace, 4 | BRF_GRA },           // 13
	{ "vpfg.e1",	0x2000, 0xacb3b469, 4 | BRF_GRA },           // 14
};

STDROMPICKEXT(dpoker, dpoker, Ssioprom)
STD_ROM_FN(dpoker)

static INT32 DpokerInit()
{
	INT32 nRet = DrvInit(90009);

	if (nRet == 0)
	{
	//	ssio_set_custom_input(1, 0xff, dpoker_ip1_read);
	//	ssio_set_custom_output(0, 0xff, dpoker_op0_write);
	}

	return nRet;
}

struct BurnDriverD BurnDrvDpoker = {
	"dpoker", NULL, "midssio", NULL, "1985",
	"Draw Poker (Bally, 03-20)\0", NULL, "Bally", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_CASINO, 0,
	NULL, dpokerRomInfo, dpokerRomName, NULL, NULL, NULL, NULL, KickInputInfo, KickDIPInfo,//DpokerInputInfo, DpokerDIPInfo,
	DpokerInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x20,
	512, 480, 4, 3
};


// Satan's Hollow (set 1)

static struct BurnRomInfo shollowRomDesc[] = {
	{ "sh-pro.00",	0x2000, 0x95e2b800, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "sh-pro.01",	0x2000, 0xb99f6ff8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sh-pro.02",	0x2000, 0x1202c7b2, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sh-pro.03",	0x2000, 0x0a64afb9, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "sh-pro.04",	0x2000, 0x22fa9175, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "sh-pro.05",	0x2000, 0x1716e2bb, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "sh-snd.01",	0x1000, 0x55a297cc, 2 | BRF_PRG | BRF_ESS }, //  6 ssio:cpu
	{ "sh-snd.02",	0x1000, 0x46fc31f6, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "sh-snd.03",	0x1000, 0xb1f4a6a8, 2 | BRF_PRG | BRF_ESS }, //  8

	{ "sh-bg.00",	0x2000, 0x3e2b333c, 3 | BRF_GRA },           //  9 gfx1
	{ "sh-bg.01",	0x2000, 0xd1d70cc4, 3 | BRF_GRA },           // 10

	{ "sh-fg.00",	0x2000, 0x33f4554e, 4 | BRF_GRA },           // 11 gfx2
	{ "sh-fg.01",	0x2000, 0xba1a38b4, 4 | BRF_GRA },           // 12
	{ "sh-fg.02",	0x2000, 0x6b57f6da, 4 | BRF_GRA },           // 13
	{ "sh-fg.03",	0x2000, 0x37ea9d07, 4 | BRF_GRA },           // 14
};

STDROMPICKEXT(shollow, shollow, Ssioprom)
STD_ROM_FN(shollow)

static INT32 ShollowInit()
{
	return DrvInit(90010);
}

struct BurnDriver BurnDrvShollow = {
	"shollow", NULL, "midssio", NULL, "1981",
	"Satan's Hollow (set 1)\0", NULL, "Bally Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_SHOOT, 0,
	NULL, shollowRomInfo, shollowRomName, NULL, NULL, NULL, NULL, ShollowInputInfo, ShollowDIPInfo,
	ShollowInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
    480, 512, 3, 4
};


// Satan's Hollow (set 2)

static struct BurnRomInfo shollow2RomDesc[] = {
	{ "sh-pro.00",	0x2000, 0x95e2b800, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "sh-pro.01",	0x2000, 0xb99f6ff8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sh-pro.02",	0x2000, 0x1202c7b2, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sh-pro.03",	0x2000, 0x0a64afb9, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "sh-pro.04",	0x2000, 0x22fa9175, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "sh-pro.05",	0x2000, 0x1716e2bb, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "snd-0.a7",	0x1000, 0x9d815bb3, 2 | BRF_PRG | BRF_ESS }, //  6 ssio:cpu
	{ "snd-1.a8",	0x1000, 0x9f253412, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "snd-2.a9",	0x1000, 0x7783d6c6, 2 | BRF_PRG | BRF_ESS }, //  8

	{ "sh-bg.00",	0x2000, 0x3e2b333c, 3 | BRF_GRA },           //  9 gfx1
	{ "sh-bg.01",	0x2000, 0xd1d70cc4, 3 | BRF_GRA },           // 10

	{ "sh-fg.00",	0x2000, 0x33f4554e, 4 | BRF_GRA },           // 11 gfx2
	{ "sh-fg.01",	0x2000, 0xba1a38b4, 4 | BRF_GRA },           // 12
	{ "sh-fg.02",	0x2000, 0x6b57f6da, 4 | BRF_GRA },           // 13
	{ "sh-fg.03",	0x2000, 0x37ea9d07, 4 | BRF_GRA },           // 14
};

STDROMPICKEXT(shollow2, shollow2, Ssioprom)
STD_ROM_FN(shollow2)

struct BurnDriver BurnDrvShollow2 = {
	"shollow2", "shollow", "midssio", NULL, "1981",
	"Satan's Hollow (set 2)\0", NULL, "Bally Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_SHOOT, 0,
	NULL, shollow2RomInfo, shollow2RomName, NULL, NULL, NULL, NULL, ShollowInputInfo, ShollowDIPInfo,
	ShollowInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
    480, 512, 3, 4
};


// Tron (8/9), program ROMs stamped as "AUG 9"

static struct BurnRomInfo tronRomDesc[] = {
	{ "scpu-pga_lctn-c2_tron_aug_9.c2",		0x2000, 0x0de0471a, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "scpu-pgb_lctn-c3_tron_aug_9.c3",		0x2000, 0x8ddf8717, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "scpu-pgc_lctn-c4_tron_aug_9.c4",		0x2000, 0x4241e3a0, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "scpu-pgd_lctn-c5_tron_aug_9.c5",		0x2000, 0x035d2fe7, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "scpu-pge_lctn-c6_tron_aug_9.c6",		0x2000, 0x24c185d8, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "scpu-pgf_lctn-c7_tron_aug_9.c7",		0x2000, 0x38c4bbaf, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "ssi-0a_lctn-a7_tron.a7",				0x1000, 0x765e6eba, 2 | BRF_PRG | BRF_ESS }, //  6 ssio:cpu
	{ "ssi-0b_lctn-a8_tron.a8",				0x1000, 0x1b90ccdd, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "ssi-0c_lctn-a9_tron.a9",				0x1000, 0x3a4bc629, 2 | BRF_PRG | BRF_ESS }, //  8

	{ "scpu-bgg_lctn-g3_tron.g3",			0x2000, 0x1a9ed2f5, 3 | BRF_GRA },           //  9 gfx1
	{ "scpu-bgh_lctn-g4_tron.g4",			0x2000, 0x3220f974, 3 | BRF_GRA },           // 10

	{ "vga_lctn-e1_tron.e1",				0x2000, 0xbc036d1d, 4 | BRF_GRA },           // 11 gfx2
	{ "vgb_lctn-dc1_tron.dc1",				0x2000, 0x58ee14d3, 4 | BRF_GRA },           // 12
	{ "vgc_lctn-cb1_tron.cb1",				0x2000, 0x3329f9d4, 4 | BRF_GRA },           // 13
	{ "vgd_lctn-a1_tron.a1",				0x2000, 0x9743f873, 4 | BRF_GRA },           // 14

	{ "0066-313bx-xxqx.a12.bin",			0x0001, 0x00000000, 5 | BRF_NODUMP | BRF_PRG | BRF_ESS }, // 15 scpu_pals
	{ "0066-315bx-xxqx.b12.bin",			0x0001, 0x00000000, 5 | BRF_NODUMP | BRF_PRG | BRF_ESS }, // 16
	{ "0066-322bx-xx0x.e3.bin",				0x0001, 0x00000000, 5 | BRF_NODUMP | BRF_PRG | BRF_ESS }, // 17
	{ "0066-316bx-xxqx.g11.bin",			0x0001, 0x00000000, 5 | BRF_NODUMP | BRF_PRG | BRF_ESS }, // 18
	{ "0066-314bx-xxqx.g12.bin",			0x0001, 0x00000000, 5 | BRF_NODUMP | BRF_PRG | BRF_ESS }, // 19
};

STDROMPICKEXT(tron, tron, Ssioprom)
STD_ROM_FN(tron)

static UINT8 tron_ip1_read(UINT8)
{
	return BurnTrackballRead(0, 0);
}

static UINT8 tron_ip4_read(UINT8)
{
	return BurnTrackballRead(0, 1);
}

static INT32 TronInit()
{
    INT32 nRet = DrvInit(90010);

    if (!nRet) {
        has_dial = 1;
        ssio_set_custom_input(1, 0xff, tron_ip1_read);
        ssio_set_custom_input(4, 0xff, tron_ip4_read);
    }

    return nRet;
}

struct BurnDriver BurnDrvTron = {
	"tron", NULL, "midssio", NULL, "1982",
	"Tron (8/9)\0", NULL, "Bally Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, tronRomInfo, tronRomName, NULL, NULL, NULL, NULL, TronInputInfo, TronDIPInfo,
	TronInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	480, 512, 3, 4
};


// Tron (6/25), program ROMs stamped as "JUN 25" - also known to be stamped 6 25 or handwritten 6-25

static struct BurnRomInfo tron2RomDesc[] = {
	{ "scpu-pga_lctn-c2_tron_jun_25.c2",	0x2000, 0x5151770b, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "scpu-pgb_lctn-c3_tron_jun_25.c3",	0x2000, 0x8ddf8717, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "scpu-pgc_lctn-c4_tron_jun_25.c4",	0x2000, 0x4241e3a0, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "scpu-pgd_lctn-c5_tron_jun_25.c5",	0x2000, 0x035d2fe7, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "scpu-pge_lctn-c6_tron_jun_25.c6",	0x2000, 0x24c185d8, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "scpu-pgf_lctn-c7_tron_jun_25.c7",	0x2000, 0x38c4bbaf, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "ssi-0a_lctn-a7_tron.a7",				0x1000, 0x765e6eba, 2 | BRF_PRG | BRF_ESS }, //  6 ssio:cpu
	{ "ssi-0b_lctn-a8_tron.a8",				0x1000, 0x1b90ccdd, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "ssi-0c_lctn-a9_tron.a9",				0x1000, 0x3a4bc629, 2 | BRF_PRG | BRF_ESS }, //  8

	{ "scpu-bgg_lctn-g3_tron.g3",			0x2000, 0x1a9ed2f5, 3 | BRF_GRA },           //  9 gfx1
	{ "scpu-bgh_lctn-g4_tron.g4",			0x2000, 0x3220f974, 3 | BRF_GRA },           // 10

	{ "vga_lctn-e1_tron.e1",				0x2000, 0xbc036d1d, 4 | BRF_GRA },           // 11 gfx2
	{ "vgb_lctn-dc1_tron.dc1",				0x2000, 0x58ee14d3, 4 | BRF_GRA },           // 12
	{ "vgc_lctn-cb1_tron.cb1",				0x2000, 0x3329f9d4, 4 | BRF_GRA },           // 13
	{ "vgd_lctn-a1_tron.a1",				0x2000, 0x9743f873, 4 | BRF_GRA },           // 14

	{ "0066-313bx-xxqx.a12.bin",			0x0001, 0x00000000, 5 | BRF_NODUMP | BRF_PRG | BRF_ESS }, // 15 scpu_pals
	{ "0066-315bx-xxqx.b12.bin",			0x0001, 0x00000000, 5 | BRF_NODUMP | BRF_PRG | BRF_ESS }, // 16
	{ "0066-322bx-xx0x.e3.bin",				0x0001, 0x00000000, 5 | BRF_NODUMP | BRF_PRG | BRF_ESS }, // 17
	{ "0066-316bx-xxqx.g11.bin",			0x0001, 0x00000000, 5 | BRF_NODUMP | BRF_PRG | BRF_ESS }, // 18
	{ "0066-314bx-xxqx.g12.bin",			0x0001, 0x00000000, 5 | BRF_NODUMP | BRF_PRG | BRF_ESS }, // 19
};

STDROMPICKEXT(tron2, tron2, Ssioprom)
STD_ROM_FN(tron2)

struct BurnDriver BurnDrvTron2 = {
	"tron2", "tron", "midssio", NULL, "1982",
	"Tron (6/25)\0", NULL, "Bally Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, tron2RomInfo, tron2RomName, NULL, NULL, NULL, NULL, TronInputInfo, TronDIPInfo,
	TronInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	480, 512, 3, 4
};


// Tron (6/17), program ROMs stamped as "JUN 17" - also known to be stamped 6 17 or handwritten 6-17

static struct BurnRomInfo tron3RomDesc[] = {
	{ "scpu-pga_lctn-c2_tron_jun_17.c2",	0x2000, 0xfc33afd7, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "scpu-pgb_lctn-c3_tron_jun_17.c3",	0x2000, 0x7d9e22ac, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "scpu-pgc_lctn-c4_tron_jun_17.c4",	0x2000, 0x902011c6, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "scpu-pgd_lctn-c5_tron_jun_17.c5",	0x2000, 0x86477e89, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "scpu-pge_lctn-c6_tron_jun_17.c6",	0x2000, 0xea198fa8, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "scpu-pgf_lctn-c7_tron_jun_17.c7",	0x2000, 0x4325fb08, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "ssi-0a_lctn-a7_tron.a7",				0x1000, 0x765e6eba, 2 | BRF_PRG | BRF_ESS }, //  6 ssio:cpu
	{ "ssi-0b_lctn-a8_tron.a8",				0x1000, 0x1b90ccdd, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "ssi-0c_lctn-a9_tron.a9",				0x1000, 0x3a4bc629, 2 | BRF_PRG | BRF_ESS }, //  8

	{ "scpu-bgg_lctn-g3_tron.g3",			0x2000, 0x1a9ed2f5, 3 | BRF_GRA },           //  9 gfx1
	{ "scpu-bgh_lctn-g4_tron.g4",			0x2000, 0x3220f974, 3 | BRF_GRA },           // 10

	{ "vga_lctn-e1_tron.e1",				0x2000, 0xbc036d1d, 4 | BRF_GRA },           // 11 gfx2
	{ "vgb_lctn-dc1_tron.dc1",				0x2000, 0x58ee14d3, 4 | BRF_GRA },           // 12
	{ "vgc_lctn-cb1_tron.cb1",				0x2000, 0x3329f9d4, 4 | BRF_GRA },           // 13
	{ "vgd_lctn-a1_tron.a1",				0x2000, 0x9743f873, 4 | BRF_GRA },           // 14

	{ "0066-313bx-xxqx.a12.bin",			0x0001, 0x00000000, 5 | BRF_NODUMP | BRF_PRG | BRF_ESS }, // 15 scpu_pals
	{ "0066-315bx-xxqx.b12.bin",			0x0001, 0x00000000, 5 | BRF_NODUMP | BRF_PRG | BRF_ESS }, // 16
	{ "0066-322bx-xx0x.e3.bin",				0x0001, 0x00000000, 5 | BRF_NODUMP | BRF_PRG | BRF_ESS }, // 17
	{ "0066-316bx-xxqx.g11.bin",			0x0001, 0x00000000, 5 | BRF_NODUMP | BRF_PRG | BRF_ESS }, // 18
	{ "0066-314bx-xxqx.g12.bin",			0x0001, 0x00000000, 5 | BRF_NODUMP | BRF_PRG | BRF_ESS }, // 19
};

STDROMPICKEXT(tron3, tron3, Ssioprom)
STD_ROM_FN(tron3)

struct BurnDriver BurnDrvTron3 = {
	"tron3", "tron", "midssio", NULL, "1982",
	"Tron (6/17)\0", NULL, "Bally Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, tron3RomInfo, tron3RomName, NULL, NULL, NULL, NULL, TronInputInfo, TronDIPInfo,// Tron3InputInfo, Tron3DIPInfo,
	TronInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	480, 512, 3, 4
};


// Tron (6/15), program ROMs stamped as "JUN 15" - also known to be stamped 6 15 or handwritten 6-15

static struct BurnRomInfo tron4RomDesc[] = {
	{ "scpu-pga_lctn-c2_tron_jun_15.c2",	0x2000, 0x09d7a95a, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "scpu-pgb_lctn-c3_tron_jun_15.c3",	0x2000, 0xb454337d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "scpu-pgc_lctn-c4_tron_jun_15.c4",	0x2000, 0xac1836ff, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "scpu-pgd_lctn-c5_tron_jun_15.c5",	0x2000, 0x1a7bec6d, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "scpu-pge_lctn-c6_tron_jun_15.c6",	0x2000, 0xea198fa8, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "scpu-pgf_lctn-c7_tron_jun_15.c7",	0x2000, 0x790ee743, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "tron_snd-0_may_10_82.a7",			0x1000, 0x2cbb332b, 2 | BRF_PRG | BRF_ESS }, //  6 ssio:cpu
	{ "tron_snd-1_may_10_82.a8",			0x1000, 0x1355b7e6, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "tron_snd-2_may_10_82.a9",			0x1000, 0x6dd4b7c9, 2 | BRF_PRG | BRF_ESS }, //  8

	{ "scpu-bgg_lctn-g3_tron.g3",			0x2000, 0x1a9ed2f5, 3 | BRF_GRA },           //  9 gfx1
	{ "scpu-bgh_lctn-g4_tron.g4",			0x2000, 0x3220f974, 3 | BRF_GRA },           // 10

	{ "vga_lctn-e1_tron.e1",				0x2000, 0xbc036d1d, 4 | BRF_GRA },           // 11 gfx2
	{ "vgb_lctn-dc1_tron.dc1",				0x2000, 0x58ee14d3, 4 | BRF_GRA },           // 12
	{ "vgc_lctn-cb1_tron.cb1",				0x2000, 0x3329f9d4, 4 | BRF_GRA },           // 13
	{ "vgd_lctn-a1_tron.a1",				0x2000, 0x9743f873, 4 | BRF_GRA },           // 14

	{ "0066-313bx-xxqx.a12.bin",			0x0001, 0x00000000, 5 | BRF_NODUMP | BRF_PRG | BRF_ESS }, // 15 scpu_pals
	{ "0066-315bx-xxqx.b12.bin",			0x0001, 0x00000000, 5 | BRF_NODUMP | BRF_PRG | BRF_ESS }, // 16
	{ "0066-322bx-xx0x.e3.bin",				0x0001, 0x00000000, 5 | BRF_NODUMP | BRF_PRG | BRF_ESS }, // 17
	{ "0066-316bx-xxqx.g11.bin",			0x0001, 0x00000000, 5 | BRF_NODUMP | BRF_PRG | BRF_ESS }, // 18
	{ "0066-314bx-xxqx.g12.bin",			0x0001, 0x00000000, 5 | BRF_NODUMP | BRF_PRG | BRF_ESS }, // 19
};

STDROMPICKEXT(tron4, tron4, Ssioprom)
STD_ROM_FN(tron4)

struct BurnDriver BurnDrvTron4 = {
	"tron4", "tron", "midssio", NULL, "1982",
	"Tron (6/15)\0", NULL, "Bally Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, tron4RomInfo, tron4RomName, NULL, NULL, NULL, NULL, TronInputInfo, TronDIPInfo,//, Tron3InputInfo, Tron3DIPInfo,
	TronInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	480, 512, 3, 4
};


// Tron (5/12), handwritten labels for program & sound ROMs (though the sound ROMs were stamped MAY 10 '82)

static struct BurnRomInfo tron5RomDesc[] = {
	{ "tron_pro-0_5-12.c2",			0x2000, 0xccc4119f, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "tron_pro-1_5-12.c3",			0x2000, 0x153f148c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tron_pro-2_5-12.c4",			0x2000, 0xe62bb8a1, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "tron_pro-3_5-12.c5",			0x2000, 0xdbc06c91, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "tron_pro-4_5-12.c6",			0x2000, 0x30adb624, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "tron_pro-5_5-12.c7",			0x2000, 0x191c72bb, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "tron_snd-0_may_10_82.a7",	0x1000, 0x2cbb332b, 2 | BRF_PRG | BRF_ESS }, //  6 ssio:cpu
	{ "tron_snd-1_may_10_82.a8",	0x1000, 0x1355b7e6, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "tron_snd-2_may_10_82.a9",	0x1000, 0x6dd4b7c9, 2 | BRF_PRG | BRF_ESS }, //  8

	{ "scpu-bgg_lctn-g3_tron.g3",	0x2000, 0x1a9ed2f5, 3 | BRF_GRA },           //  9 gfx1
	{ "scpu-bgh_lctn-g4_tron.g4",	0x2000, 0x3220f974, 3 | BRF_GRA },           // 10

	{ "vga_lctn-e1_tron.e1",		0x2000, 0xbc036d1d, 4 | BRF_GRA },           // 11 gfx2
	{ "vgb_lctn-dc1_tron.dc1",		0x2000, 0x58ee14d3, 4 | BRF_GRA },           // 12
	{ "vgc_lctn-cb1_tron.cb1",		0x2000, 0x3329f9d4, 4 | BRF_GRA },           // 13
	{ "vgd_lctn-a1_tron.a1",		0x2000, 0x9743f873, 4 | BRF_GRA },           // 14

	{ "0066-313bx-xxqx.a12.bin",	0x0001, 0x00000000, 5 | BRF_NODUMP | BRF_PRG | BRF_ESS }, // 15 scpu_pals
	{ "0066-315bx-xxqx.b12.bin",	0x0001, 0x00000000, 5 | BRF_NODUMP | BRF_PRG | BRF_ESS }, // 16
	{ "0066-322bx-xx0x.e3.bin",		0x0001, 0x00000000, 5 | BRF_NODUMP | BRF_PRG | BRF_ESS }, // 17
	{ "0066-316bx-xxqx.g11.bin",	0x0001, 0x00000000, 5 | BRF_NODUMP | BRF_PRG | BRF_ESS }, // 18
	{ "0066-314bx-xxqx.g12.bin",	0x0001, 0x00000000, 5 | BRF_NODUMP | BRF_PRG | BRF_ESS }, // 19
};

STDROMPICKEXT(tron5, tron5, Ssioprom)
STD_ROM_FN(tron5)

struct BurnDriver BurnDrvTron5 = {
	"tron5", "tron", "midssio", NULL, "1982",
	"Tron (5/12)\0", NULL, "Bally Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, tron5RomInfo, tron5RomName, NULL, NULL, NULL, NULL, TronInputInfo, TronDIPInfo,//, Tron3InputInfo, Tron3DIPInfo,
	TronInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	480, 512, 3, 4
};


// Tron (Germany)

static struct BurnRomInfo trongerRomDesc[] = {
	{ "scpu_pga.c2",				0x2000, 0xba14603d, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "scpu_pgb.c3",				0x2000, 0x063a748f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "scpu_pgc.c4",				0x2000, 0x6ca50365, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "scpu_pgd.c5",				0x2000, 0xb5b241c9, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "scpu_pge.c6",				0x2000, 0x04597abe, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "scpu_pgf.c7",				0x2000, 0x3908e404, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "ssi-0a_lctn-a7_tron.a7",		0x1000, 0x765e6eba, 2 | BRF_PRG | BRF_ESS }, //  6 ssio:cpu
	{ "ssi-0b_lctn-a8_tron.a8",		0x1000, 0x1b90ccdd, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "ssi-0c_lctn-a9_tron.a9",		0x1000, 0x3a4bc629, 2 | BRF_PRG | BRF_ESS }, //  8

	{ "scpu-bgg_lctn-g3_tron.g3",	0x2000, 0x1a9ed2f5, 3 | BRF_GRA },           //  9 gfx1
	{ "scpu-bgh_lctn-g4_tron.g4",	0x2000, 0x3220f974, 3 | BRF_GRA },           // 10

	{ "vga_lctn-e1_tron.e1",		0x2000, 0xbc036d1d, 4 | BRF_GRA },           // 11 gfx2
	{ "vgb_lctn-dc1_tron.dc1",		0x2000, 0x58ee14d3, 4 | BRF_GRA },           // 12
	{ "vgc_lctn-cb1_tron.cb1",		0x2000, 0x3329f9d4, 4 | BRF_GRA },           // 13
	{ "vgd_lctn-a1_tron.a1",		0x2000, 0x9743f873, 4 | BRF_GRA },           // 14

	{ "0066-313bx-xxqx.a12.bin",	0x0001, 0x00000000, 5 | BRF_NODUMP | BRF_PRG | BRF_ESS }, // 15 scpu_pals
	{ "0066-315bx-xxqx.b12.bin",	0x0001, 0x00000000, 5 | BRF_NODUMP | BRF_PRG | BRF_ESS }, // 16
	{ "0066-322bx-xx0x.e3.bin",		0x0001, 0x00000000, 5 | BRF_NODUMP | BRF_PRG | BRF_ESS }, // 17
	{ "0066-316bx-xxqx.g11.bin",	0x0001, 0x00000000, 5 | BRF_NODUMP | BRF_PRG | BRF_ESS }, // 18
	{ "0066-314bx-xxqx.g12.bin",	0x0001, 0x00000000, 5 | BRF_NODUMP | BRF_PRG | BRF_ESS }, // 19
};

STDROMPICKEXT(tronger, tronger, Ssioprom)
STD_ROM_FN(tronger)

struct BurnDriver BurnDrvTronger = {
	"tronger", "tron", "midssio", NULL, "1982",
	"Tron (Germany)\0", NULL, "Bally Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, trongerRomInfo, trongerRomName, NULL, NULL, NULL, NULL, TronInputInfo, TronDIPInfo,//, Tron3InputInfo, Tron3DIPInfo,
	TronInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	480, 512, 3, 4
};


// Domino Man (set 1)

static struct BurnRomInfo dominoRomDesc[] = {
	{ "dmanpg0.bin",	0x2000, 0x3bf3bb1c, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "dmanpg1.bin",	0x2000, 0x85cf1d69, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "dmanpg2.bin",	0x2000, 0x7dd2177a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "dmanpg3.bin",	0x2000, 0xf2e0aa44, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "dm-a7.snd",		0x1000, 0xfa982dcc, 2 | BRF_PRG | BRF_ESS }, //  4 ssio:cpu
	{ "dm-a8.snd",		0x1000, 0x72839019, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "dm-a9.snd",		0x1000, 0xad760da7, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "dm-a10.snd",		0x1000, 0x958c7287, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "dmanbg0.bin",	0x2000, 0x9163007f, 3 | BRF_GRA },           //  8 gfx1
	{ "dmanbg1.bin",	0x2000, 0x28615c56, 3 | BRF_GRA },           //  9

	{ "dmanfg0.bin",	0x2000, 0x0b1f9f9e, 4 | BRF_GRA },           // 10 gfx2
	{ "dmanfg1.bin",	0x2000, 0x16aa4b9b, 4 | BRF_GRA },           // 11
	{ "dmanfg2.bin",	0x2000, 0x4a8e76b8, 4 | BRF_GRA },           // 12
	{ "dmanfg3.bin",	0x2000, 0x1f39257e, 4 | BRF_GRA },           // 13
};

STDROMPICKEXT(domino, domino, Ssioprom)
STD_ROM_FN(domino)

static INT32 DominoInit()
{
    INT32 nRet = DrvInit(90010);

    if (!nRet) {
        ssio_basevolume(0.15);
    }

    return nRet;
}

struct BurnDriver BurnDrvDomino = {
	"domino", NULL, "midssio", NULL, "1982",
	"Domino Man (set 1)\0", NULL, "Bally Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_PUZZLE, 0,
	NULL, dominoRomInfo, dominoRomName, NULL, NULL, NULL, NULL, DominoInputInfo, DominoDIPInfo,
	DominoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	512, 480, 4, 3
};


// Domino Man (set 2)

static struct BurnRomInfo dominoaRomDesc[] = {
	{ "d2",		0x2000, 0xc7b6331e, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "d3",		0x2000, 0x81115c86, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "d4",		0x2000, 0x5cf32c55, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "d5",		0x2000, 0x4f89b68c, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "a7",		0x1000, 0xfa982dcc, 2 | BRF_PRG | BRF_ESS }, //  4 ssio:cpu
	{ "a8",		0x1000, 0x72839019, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "a9",		0x1000, 0xad760da7, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "a10",	0x1000, 0x958c7287, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "g3",		0x2000, 0x9163007f, 3 | BRF_GRA },           //  8 gfx1
	{ "g4",		0x2000, 0x28615c56, 3 | BRF_GRA },           //  9

	{ "e1",		0x2000, 0x0b1f9f9e, 4 | BRF_GRA },           // 10 gfx2
	{ "d1",		0x2000, 0x16aa4b9b, 4 | BRF_GRA },           // 11
	{ "b1",		0x2000, 0x4a8e76b8, 4 | BRF_GRA },           // 12
	{ "a1",		0x2000, 0x1f39257e, 4 | BRF_GRA },           // 13
};

STDROMPICKEXT(dominoa, dominoa, Ssioprom)
STD_ROM_FN(dominoa)

struct BurnDriver BurnDrvDominoa = {
	"dominoa", "domino", "midssio", NULL, "1982",
	"Domino Man (set 2)\0", NULL, "Bally Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_PUZZLE, 0,
	NULL, dominoaRomInfo, dominoaRomName, NULL, NULL, NULL, NULL, DominoInputInfo, DominoDIPInfo,
	DominoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	512, 480, 4, 3
};



// Wacko

static struct BurnRomInfo wackoRomDesc[] = {
	{ "wackocpu.2d",	0x2000, 0xc98e29b6, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "wackocpu.3d",	0x2000, 0x90b89774, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "wackocpu.4d",	0x2000, 0x515edff7, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "wackocpu.5d",	0x2000, 0x9b01bf32, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "wackosnd.7a",	0x1000, 0x1a58763f, 2 | BRF_PRG | BRF_ESS }, //  4 ssio:cpu
	{ "wackosnd.8a",	0x1000, 0xa4e3c771, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "wackosnd.9a",	0x1000, 0x155ba3dd, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "wackocpu.3g",	0x2000, 0x33160eb1, 3 | BRF_GRA },           //  7 gfx1
	{ "wackocpu.4g",	0x2000, 0xdaf37d7c, 3 | BRF_GRA },           //  8

	{ "wackovid.1e",	0x2000, 0xdca59be7, 4 | BRF_GRA },           //  9 gfx2
	{ "wackovid.1d",	0x2000, 0xa02f1672, 4 | BRF_GRA },           // 10
	{ "wackovid.1b",	0x2000, 0x7d899790, 4 | BRF_GRA },           // 11
	{ "wackovid.1a",	0x2000, 0x080be3ad, 4 | BRF_GRA },           // 12
};

STDROMPICKEXT(wacko, wacko, Ssioprom)
STD_ROM_FN(wacko)

static void wacko_op4_write(UINT8, UINT8 data)
{
    input_playernum = data & 1;
}

static UINT8 wacko_ip1_read(UINT8)
{
	return BurnTrackballRead(input_playernum, 0);
}

static UINT8 wacko_ip2_read(UINT8)
{
	return BurnTrackballRead(input_playernum, 1);
}

static INT32 WackoInit()
{
    INT32 nRet =  DrvInit(90010);

    if (!nRet) {
        is_wacko = 1;
        ssio_set_custom_input(1, 0xff, wacko_ip1_read);
        ssio_set_custom_input(2, 0xff, wacko_ip2_read);
        ssio_set_custom_output(4, 0x01, wacko_op4_write);
    }

    return nRet;
}

struct BurnDriver BurnDrvWacko = {
	"wacko", NULL, "midssio", NULL, "1982",
	"Wacko\0", NULL, "Bally Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_ACTION, 0,
	NULL, wackoRomInfo, wackoRomName, NULL, NULL, NULL, NULL, WackoInputInfo, WackoDIPInfo,
	WackoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	512, 480, 4, 3
};


// Two Tigers (dedicated)

static struct BurnRomInfo twotigerRomDesc[] = {
	{ "cpu_d2",			0x2000, 0xa682ed24, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "cpu_d3",			0x2000, 0x5b48fde9, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "cpu_d4",			0x2000, 0xf1ab8c4d, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "cpu_d5",			0x2000, 0xd7129900, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "ssio_a7",		0x1000, 0x64ddc16c, 2 | BRF_PRG | BRF_ESS }, //  4 ssio:cpu
	{ "ssio_a8",		0x1000, 0xc3467612, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "ssio_a9",		0x1000, 0xc50f7b2d, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "2tgrbg0.bin",	0x2000, 0x52f69068, 3 | BRF_GRA },           //  7 gfx1
	{ "2tgrbg1.bin",	0x2000, 0x758d4f7d, 3 | BRF_GRA },           //  8

	{ "vid_d1",			0x2000, 0xda5f49da, 4 | BRF_GRA },           //  9 gfx2
	{ "vid_c1",			0x2000, 0x62ed737b, 4 | BRF_GRA },           // 10
	{ "vid_b1",			0x2000, 0x0939921e, 4 | BRF_GRA },           // 11
	{ "vid_a1",			0x2000, 0xef515824, 4 | BRF_GRA },           // 12
};

STDROMPICKEXT(twotiger, twotiger, Ssioprom)
STD_ROM_FN(twotiger)

static struct BurnSampleInfo TwotigerSampleDesc[] = {
	{ "left", SAMPLE_NOLOOP },
	{ "right", SAMPLE_NOLOOP },
	{ "", 0 }
};

STD_SAMPLE_PICK(Twotiger)
STD_SAMPLE_FN(Twotiger)

static void twotiger_op4_write(UINT8, UINT8 data)
{
    if (~data & 2) {
        BurnSamplePause(0);
        BurnSamplePause(1);
    }
    else if (!BurnSampleGetStatus(0)) {
        BurnSampleResume(0);
        BurnSampleResume(1);
    }
}

static INT32 TwotigerInit()
{
	INT32 nRet = DrvInit(90010);

	if (nRet == 0)
    {
        is_twotiger = 1;
        BurnSampleSetRoute(0, BURN_SND_SAMPLE_ROUTE_1, 0.50, BURN_SND_ROUTE_LEFT);
        BurnSampleSetRoute(0, BURN_SND_SAMPLE_ROUTE_2, 0.50, BURN_SND_ROUTE_LEFT);
        BurnSampleSetRoute(1, BURN_SND_SAMPLE_ROUTE_1, 0.50, BURN_SND_ROUTE_RIGHT);
        BurnSampleSetRoute(1, BURN_SND_SAMPLE_ROUTE_2, 0.50, BURN_SND_ROUTE_RIGHT);
		ssio_set_custom_output(4, 0xff, twotiger_op4_write);

        ZetOpen(0);
		ZetUnmapMemory(0xe800, 0xefff, MAP_RAM);
		ZetUnmapMemory(0xf800, 0xffff, MAP_RAM);
		ZetSetWriteHandler(twotiger_vidram_write);
		ZetSetReadHandler(twotiger_vidram_read);
		ZetClose();
	}

	return nRet;
}

struct BurnDriver BurnDrvTwotiger = {
	"twotiger", NULL, "midssio", "twotiger", "1984",
	"Two Tigers (dedicated)\0", NULL, "Bally Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, twotigerRomInfo, twotigerRomName, NULL, NULL, TwotigerSampleInfo, TwotigerSampleName, TwotigerInputInfo, TwotigerDIPInfo,
	TwotigerInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	512, 480, 4, 3
};


// Two Tigers (Tron conversion)

static struct BurnRomInfo twotigercRomDesc[] = {
	{ "2tgrpg0.bin",	0x2000, 0xe77a924b, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "2tgrpg1.bin",	0x2000, 0x2699ebdc, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "2tgrpg2.bin",	0x2000, 0xb5ca3f17, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "2tgrpg3.bin",	0x2000, 0x8aa82049, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "2tgra7.bin",		0x1000, 0x4620d970, 2 | BRF_PRG | BRF_ESS }, //  4 ssio:cpu
	{ "2tgra8.bin",		0x1000, 0xe95d8cfe, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "2tgra9.bin",		0x1000, 0x81e6ce0e, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "2tgrbg0.bin",	0x2000, 0x52f69068, 3 | BRF_GRA },           //  7 gfx1
	{ "2tgrbg1.bin",	0x2000, 0x758d4f7d, 3 | BRF_GRA },           //  8

	{ "2tgrfg0.bin",	0x2000, 0x4abf3ca0, 4 | BRF_GRA },           //  9 gfx2
	{ "2tgrfg1.bin",	0x2000, 0xfbcaffa5, 4 | BRF_GRA },           // 10
	{ "2tgrfg2.bin",	0x2000, 0x08e3e1a6, 4 | BRF_GRA },           // 11
	{ "2tgrfg3.bin",	0x2000, 0x9b22697b, 4 | BRF_GRA },           // 12
};

STDROMPICKEXT(twotigerc, twotigerc, Ssioprom)
STD_ROM_FN(twotigerc)

static INT32 TwotigercInit()
{
	return DrvInit(90010);
}

struct BurnDriverD BurnDrvTwotigerc = {
	"twotigerc", "twotiger", "midssio", NULL, "1984",
	"Two Tigers (Tron conversion)\0", NULL, "Bally Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, twotigercRomInfo, twotigercRomName, NULL, NULL, NULL, NULL, TronInputInfo, TronDIPInfo,//, TwotigrcInputInfo, TwotigrcDIPInfo,
	TwotigercInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	512, 480, 4, 3
};


// Tapper (Budweiser, 1/27/84)

static struct BurnRomInfo tapperRomDesc[] = {
	{ "tapper_c.p.u._pg_0_1c_1-27-84.1c",	0x4000, 0xbb060bb0, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "tapper_c.p.u._pg_1_2c_1-27-84.2c",	0x4000, 0xfd9acc22, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tapper_c.p.u._pg_2_3c_1-27-84.3c",	0x4000, 0xb3755d41, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "tapper_c.p.u._pg_3_4c_1-27-84.4c",	0x2000, 0x77273096, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "tapper_sound_snd_0_a7_12-7-83.a7",	0x1000, 0x0e8bb9d5, 2 | BRF_PRG | BRF_ESS }, //  4 ssio:cpu
	{ "tapper_sound_snd_1_a8_12-7-83.a8",	0x1000, 0x0cf0e29b, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "tapper_sound_snd_2_a9_12-7-83.a9",	0x1000, 0x31eb6dc6, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "tapper_sound_snd_3_a10_12-7-83.a10",	0x1000, 0x01a9be6a, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "tapper_c.p.u._bg_1_6f_12-7-83.6f",	0x4000, 0x2a30238c, 3 | BRF_GRA },           //  8 gfx1
	{ "tapper_c.p.u._bg_0_5f_12-7-83.5f",	0x4000, 0x394ab576, 3 | BRF_GRA },           //  9

	{ "tapper_video_fg_1_a7_12-7-83.a7",	0x4000, 0x32509011, 4 | BRF_GRA },           // 10 gfx2
	{ "tapper_video_fg_0_a8_12-7-83.a8",	0x4000, 0x8412c808, 4 | BRF_GRA },           // 11
	{ "tapper_video_fg_3_a5_12-7-83.a5",	0x4000, 0x818fffd4, 4 | BRF_GRA },           // 12
	{ "tapper_video_fg_2_a6_12-7-83.a6",	0x4000, 0x67e37690, 4 | BRF_GRA },           // 13
	{ "tapper_video_fg_5_a3_12-7-83.a3",	0x4000, 0x800f7c8a, 4 | BRF_GRA },           // 14
	{ "tapper_video_fg_4_a4_12-7-83.a4",	0x4000, 0x32674ee6, 4 | BRF_GRA },           // 15
	{ "tapper_video_fg_7_a1_12-7-83.a1",	0x4000, 0x070b4c81, 4 | BRF_GRA },           // 16
	{ "tapper_video_fg_6_a2_12-7-83.a2",	0x4000, 0xa37aef36, 4 | BRF_GRA },           // 17
};

STDROMPICKEXT(tapper, tapper, Ssioprom)
STD_ROM_FN(tapper)

static INT32 TapperInit()
{
    INT32 nRet = DrvInit(91490);

    if (!nRet) {
        ssio_basevolume(0.00);
    }

    return nRet;
}

struct BurnDriver BurnDrvTapper = {
	"tapper", NULL, "midssio", NULL, "1983",
	"Tapper (Budweiser, 1/27/84)\0", NULL, "Bally Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, tapperRomInfo, tapperRomName, NULL, NULL, NULL, NULL, TapperInputInfo, TapperDIPInfo,
	TapperInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	512, 480, 4, 3
};


// Tapper (Budweiser, 1/27/84 - Alternate graphics)

static struct BurnRomInfo tappergRomDesc[] = {
	{ "tapper_c.p.u._pg_0_1c_1-27-84.1c",	0x4000, 0xbb060bb0, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "tapper_c.p.u._pg_1_2c_1-27-84.2c",	0x4000, 0xfd9acc22, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tapper_c.p.u._pg_2_3c_1-27-84.3c",	0x4000, 0xb3755d41, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "tapper_c.p.u._pg_3_4c_1-27-84.4c",	0x2000, 0x77273096, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "tapper_sound_snd_0_a7_12-7-83.a7",	0x1000, 0x0e8bb9d5, 2 | BRF_PRG | BRF_ESS }, //  4 ssio:cpu
	{ "tapper_sound_snd_1_a8_12-7-83.a8",	0x1000, 0x0cf0e29b, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "tapper_sound_snd_2_a9_12-7-83.a9",	0x1000, 0x31eb6dc6, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "tapper_sound_snd_3_a10_12-7-83.a10",	0x1000, 0x01a9be6a, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "tapper_c.p.u._bg_1_6f_12-7-83.6f",	0x4000, 0x2a30238c, 3 | BRF_GRA },           //  8 gfx1
	{ "tapper_c.p.u._bg_0_5f_12-7-83.5f",	0x4000, 0x394ab576, 3 | BRF_GRA },           //  9

	{ "fg1_a7.128",							0x4000, 0xbac70b69, 4 | BRF_GRA },           // 10 gfx2
	{ "fg0_a8.128",							0x4000, 0xc300925d, 4 | BRF_GRA },           // 11
	{ "fg3_a5.128",							0x4000, 0xecff6c23, 4 | BRF_GRA },           // 12
	{ "fg2_a6.128",							0x4000, 0xa4f2d1be, 4 | BRF_GRA },           // 13
	{ "fg5_a3.128",							0x4000, 0x16ce38cb, 4 | BRF_GRA },           // 14
	{ "fg4_a4.128",							0x4000, 0x082a4059, 4 | BRF_GRA },           // 15
	{ "fg7_a1.128",							0x4000, 0x3b476abe, 4 | BRF_GRA },           // 16
	{ "fg6_a2.128",							0x4000, 0x6717264c, 4 | BRF_GRA },           // 17
};

STDROMPICKEXT(tapperg, tapperg, Ssioprom)
STD_ROM_FN(tapperg)

struct BurnDriver BurnDrvTapperg = {
	"tapperg", "tapper", "midssio", NULL, "1983",
	"Tapper (Budweiser, 1/27/84 - Alternate graphics)\0", NULL, "Bally Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, tappergRomInfo, tappergRomName, NULL, NULL, NULL, NULL, TapperInputInfo, TapperDIPInfo,
	TapperInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	512, 480, 4, 3
};


// Tapper (Budweiser, 12/9/83)

static struct BurnRomInfo tapperaRomDesc[] = {
	{ "tapper_c.p.u._pg_0_1c_12-9-83.1c",	0x4000, 0x496a8e04, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "tapper_c.p.u._pg_1_2c_12-9-83.2c",	0x4000, 0xe79c4b0c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tapper_c.p.u._pg_2_3c_12-9-83.3c",	0x4000, 0x3034ccf0, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "tapper_c.p.u._pg_3_4c_12-9-83.4c",	0x2000, 0x2dc99e05, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "tapper_sound_snd_0_a7_12-7-83.a7",	0x1000, 0x0e8bb9d5, 2 | BRF_PRG | BRF_ESS }, //  4 ssio:cpu
	{ "tapper_sound_snd_1_a8_12-7-83.a8",	0x1000, 0x0cf0e29b, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "tapper_sound_snd_2_a9_12-7-83.a9",	0x1000, 0x31eb6dc6, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "tapper_sound_snd_3_a10_12-7-83.a10",	0x1000, 0x01a9be6a, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "tapper_c.p.u._bg_1_6f_12-7-83.6f",	0x4000, 0x2a30238c, 3 | BRF_GRA },           //  8 gfx1
	{ "tapper_c.p.u._bg_0_5f_12-7-83.5f",	0x4000, 0x394ab576, 3 | BRF_GRA },           //  9

	{ "tapper_video_fg_1_a7_12-7-83.a7",	0x4000, 0x32509011, 4 | BRF_GRA },           // 10 gfx2
	{ "tapper_video_fg_0_a8_12-7-83.a8",	0x4000, 0x8412c808, 4 | BRF_GRA },           // 11
	{ "tapper_video_fg_3_a5_12-7-83.a5",	0x4000, 0x818fffd4, 4 | BRF_GRA },           // 12
	{ "tapper_video_fg_2_a6_12-7-83.a6",	0x4000, 0x67e37690, 4 | BRF_GRA },           // 13
	{ "tapper_video_fg_5_a3_12-7-83.a3",	0x4000, 0x800f7c8a, 4 | BRF_GRA },           // 14
	{ "tapper_video_fg_4_a4_12-7-83.a4",	0x4000, 0x32674ee6, 4 | BRF_GRA },           // 15
	{ "tapper_video_fg_7_a1_12-7-83.a1",	0x4000, 0x070b4c81, 4 | BRF_GRA },           // 16
	{ "tapper_video_fg_6_a2_12-7-83.a2",	0x4000, 0xa37aef36, 4 | BRF_GRA },           // 17
};

STDROMPICKEXT(tappera, tappera, Ssioprom)
STD_ROM_FN(tappera)

struct BurnDriver BurnDrvTappera = {
	"tappera", "tapper", "midssio", NULL, "1983",
	"Tapper (Budweiser, 12/9/83)\0", NULL, "Bally Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, tapperaRomInfo, tapperaRomName, NULL, NULL, NULL, NULL, TapperInputInfo, TapperDIPInfo,
	TapperInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	512, 480, 4, 3
};


// Tapper (Budweiser, Date Unknown)

static struct BurnRomInfo tapperbRomDesc[] = {
	{ "tapper_c.p.u._pg_0_1c.1c",			0x4000, 0x127171d1, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "tapper_c.p.u._pg_1_2c.1c",			0x4000, 0x9d6a47f7, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tapper_c.p.u._pg_2_3c.3c",			0x4000, 0x3a1f8778, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "tapper_c.p.u._pg_3_4c.4c",			0x2000, 0xe8dcdaa4, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "tapper_sound_snd_0_a7_12-7-83.a7",	0x1000, 0x0e8bb9d5, 2 | BRF_PRG | BRF_ESS }, //  4 ssio:cpu
	{ "tapper_sound_snd_1_a8_12-7-83.a8",	0x1000, 0x0cf0e29b, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "tapper_sound_snd_2_a9_12-7-83.a9",	0x1000, 0x31eb6dc6, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "tapper_sound_snd_3_a10_12-7-83.a10",	0x1000, 0x01a9be6a, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "tapper_c.p.u._bg_1_6f_12-7-83.6f",	0x4000, 0x2a30238c, 3 | BRF_GRA },           //  8 gfx1
	{ "tapper_c.p.u._bg_0_5f_12-7-83.5f",	0x4000, 0x394ab576, 3 | BRF_GRA },           //  9

	{ "tapper_video_fg_1_a7_12-7-83.a7",	0x4000, 0x32509011, 4 | BRF_GRA },           // 10 gfx2
	{ "tapper_video_fg_0_a8_12-7-83.a8",	0x4000, 0x8412c808, 4 | BRF_GRA },           // 11
	{ "tapper_video_fg_3_a5_12-7-83.a5",	0x4000, 0x818fffd4, 4 | BRF_GRA },           // 12
	{ "tapper_video_fg_2_a6_12-7-83.a6",	0x4000, 0x67e37690, 4 | BRF_GRA },           // 13
	{ "tapper_video_fg_5_a3_12-7-83.a3",	0x4000, 0x800f7c8a, 4 | BRF_GRA },           // 14
	{ "tapper_video_fg_4_a4_12-7-83.a4",	0x4000, 0x32674ee6, 4 | BRF_GRA },           // 15
	{ "tapper_video_fg_7_a1_12-7-83.a1",	0x4000, 0x070b4c81, 4 | BRF_GRA },           // 16
	{ "tapper_video_fg_6_a2_12-7-83.a2",	0x4000, 0xa37aef36, 4 | BRF_GRA },           // 17
};

STDROMPICKEXT(tapperb, tapperb, Ssioprom)
STD_ROM_FN(tapperb)

struct BurnDriver BurnDrvTapperb = {
	"tapperb", "tapper", "midssio", NULL, "1983",
	"Tapper (Budweiser, Date Unknown)\0", NULL, "Bally Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, tapperbRomInfo, tapperbRomName, NULL, NULL, NULL, NULL, TapperInputInfo, TapperDIPInfo,
	TapperInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	512, 480, 4, 3
};


// Tapper (Suntory)

static struct BurnRomInfo sutapperRomDesc[] = {
	{ "epr-5791",			0x4000, 0x87119cc4, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "epr-5792",			0x4000, 0x4c23ad89, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-5793",			0x4000, 0xfecbf683, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-5794",			0x2000, 0x5bdc1916, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-5788.h11.ic8",	0x1000, 0x5c1d0982, 2 | BRF_PRG | BRF_ESS }, //  4 ssio:cpu
	{ "epr-5787.h10.ic6",	0x1000, 0x09e74ed8, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "epr-5786.h9.ic5",	0x1000, 0xc3e98284, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "epr-5785.h7.ic4",	0x1000, 0xced2fd47, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "epr-5790",			0x4000, 0xac1558c1, 3 | BRF_GRA },           //  8 gfx1
	{ "epr-5789",			0x4000, 0xfa66cab5, 3 | BRF_GRA },           //  9

	{ "epr-5795",			0x4000, 0x5d987c92, 4 | BRF_GRA },           // 10 gfx2
	{ "epr-5796",			0x4000, 0xde5700b4, 4 | BRF_GRA },           // 11
	{ "epr-5797",			0x4000, 0xf10a1d05, 4 | BRF_GRA },           // 12
	{ "epr-5798",			0x4000, 0x614990cd, 4 | BRF_GRA },           // 13
	{ "epr-5799",			0x4000, 0x02c69432, 4 | BRF_GRA },           // 14
	{ "epr-5800",			0x4000, 0xebf1f948, 4 | BRF_GRA },           // 15
	{ "epr-5801",			0x4000, 0xd70defa7, 4 | BRF_GRA },           // 16
	{ "epr-5802",			0x4000, 0xd4f114b9, 4 | BRF_GRA },           // 17
};

STDROMPICKEXT(sutapper, sutapper, Ssioprom)
STD_ROM_FN(sutapper)

struct BurnDriver BurnDrvSutapper = {
	"sutapper", "tapper", "midssio", NULL, "1983",
	"Tapper (Suntory)\0", NULL, "Bally Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, sutapperRomInfo, sutapperRomName, NULL, NULL, NULL, NULL, TapperInputInfo, TapperDIPInfo,
	TapperInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	512, 480, 4, 3
};


// Tapper (Root Beer)

static struct BurnRomInfo rbtapperRomDesc[] = {
	{ "rbtpg0.bin",	0x4000, 0x20b9adf4, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "rbtpg1.bin",	0x4000, 0x87e616c2, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rbtpg2.bin",	0x4000, 0x0b332c97, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rbtpg3.bin",	0x2000, 0x698c06f2, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "5788",		0x1000, 0x5c1d0982, 2 | BRF_PRG | BRF_ESS }, //  4 ssio:cpu
	{ "5787",		0x1000, 0x09e74ed8, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "5786",		0x1000, 0xc3e98284, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "5785",		0x1000, 0xced2fd47, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "rbtbg1.bin",	0x4000, 0x44dfa483, 3 | BRF_GRA },           //  8 gfx1
	{ "rbtbg0.bin",	0x4000, 0x510b13de, 3 | BRF_GRA },           //  9

	{ "rbtfg1.bin",	0x4000, 0x1c0b8791, 4 | BRF_GRA },           // 10 gfx2
	{ "rbtfg0.bin",	0x4000, 0xe99f6018, 4 | BRF_GRA },           // 11
	{ "rbtfg3.bin",	0x4000, 0x3e725e77, 4 | BRF_GRA },           // 12
	{ "rbtfg2.bin",	0x4000, 0x4ee8b624, 4 | BRF_GRA },           // 13
	{ "rbtfg5.bin",	0x4000, 0x9eeca46e, 4 | BRF_GRA },           // 14
	{ "rbtfg4.bin",	0x4000, 0x8c79e7d7, 4 | BRF_GRA },           // 15
	{ "rbtfg7.bin",	0x4000, 0x8dbf0c36, 4 | BRF_GRA },           // 16
	{ "rbtfg6.bin",	0x4000, 0x441201a0, 4 | BRF_GRA },           // 17
};

STDROMPICKEXT(rbtapper, rbtapper, Ssioprom)
STD_ROM_FN(rbtapper)

struct BurnDriver BurnDrvRbtapper = {
	"rbtapper", "tapper", "midssio", NULL, "1984",
	"Tapper (Root Beer)\0", NULL, "Bally Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, rbtapperRomInfo, rbtapperRomName, NULL, NULL, NULL, NULL, TapperInputInfo, TapperDIPInfo,
	TapperInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	512, 480, 4, 3
};


// Kozmik Kroozr

static struct BurnRomInfo kroozrRomDesc[] = {
	{ "kozmkcpu.2d",	0x2000, 0x61e02045, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "kozmkcpu.3d",	0x2000, 0xcaabed57, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "kozmkcpu.4d",	0x2000, 0x2bc83fc7, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "kozmkcpu.5d",	0x2000, 0xa0ec38c1, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "kozmkcpu.6d",	0x2000, 0x7044f2b6, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "kozmksnd.7a",	0x1000, 0x6736e433, 2 | BRF_PRG | BRF_ESS }, //  5 ssio:cpu
	{ "kozmksnd.8a",	0x1000, 0xea9cd919, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "kozmksnd.9a",	0x1000, 0x9dfa7994, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "kozmkcpu.3g",	0x2000, 0xeda6ed2d, 3 | BRF_GRA },           //  8 gfx1
	{ "kozmkcpu.4g",	0x2000, 0xddde894b, 3 | BRF_GRA },           //  9

	{ "kozmkvid.1e",	0x2000, 0xca60e2cc, 4 | BRF_GRA },           // 10 gfx2
	{ "kozmkvid.1d",	0x2000, 0x4e23b35b, 4 | BRF_GRA },           // 11
	{ "kozmkvid.1b",	0x2000, 0xc6041ba7, 4 | BRF_GRA },           // 12
	{ "kozmkvid.1a",	0x2000, 0xb57fb0ff, 4 | BRF_GRA },           // 13
};

STDROMPICKEXT(kroozr, kroozr, Ssioprom)
STD_ROM_FN(kroozr)

static INT32 KroozrInit()
{
	INT32 nRet = DrvInit(90010);

	if (nRet == 0)
    {
        has_dial = 1;
        is_kroozr = 1;
        ssio_set_custom_input(1, 0x7f, kroozr_ip1_read);
	}

	return nRet;
}

struct BurnDriver BurnDrvKroozr = {
	"kroozr", NULL, "midssio", NULL, "1982",
	"Kozmik Kroozr\0", NULL, "Bally Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_SHOOT, 0,
	NULL, kroozrRomInfo, kroozrRomName, NULL, NULL, NULL, NULL, KroozrInputInfo, KroozrDIPInfo,
	KroozrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	512, 480, 4, 3
};


// Journey

static struct BurnRomInfo journeyRomDesc[] = {
	{ "d2",		0x2000, 0xf2618913, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "d3",		0x2000, 0x2f290d2e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "d4",		0x2000, 0xcc6c0150, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "d5",		0x2000, 0xc3023931, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "d6",		0x2000, 0x5d445c99, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "a",		0x1000, 0x2524a2aa, 2 | BRF_PRG | BRF_ESS }, //  5 ssio:cpu
	{ "b",		0x1000, 0xb8e35814, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "c",		0x1000, 0x09c488cf, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "d",		0x1000, 0x3d627bee, 2 | BRF_PRG | BRF_ESS }, //  8

	{ "g3",		0x2000, 0xc14558de, 3 | BRF_GRA },           //  9 gfx1
	{ "g4",		0x2000, 0x9104c1d0, 3 | BRF_GRA },           // 10

	{ "a7",		0x2000, 0x4ca2bb2d, 4 | BRF_GRA },           // 11 gfx2
	{ "a8",		0x2000, 0x4fb7925d, 4 | BRF_GRA },           // 12
	{ "a5",		0x2000, 0x560c474f, 4 | BRF_GRA },           // 13
	{ "a6",		0x2000, 0xb1f31583, 4 | BRF_GRA },           // 14
	{ "a3",		0x2000, 0xf295afda, 4 | BRF_GRA },           // 15
	{ "a4",		0x2000, 0x765876a7, 4 | BRF_GRA },           // 16
	{ "a1",		0x2000, 0x4af986f8, 4 | BRF_GRA },           // 17
	{ "a2",		0x2000, 0xb30cd2a7, 4 | BRF_GRA },           // 18
};

STDROMPICKEXT(journey, journey, Ssioprom)
STD_ROM_FN(journey)

static struct BurnSampleInfo JourneySampleDesc[] = {
	{ "sepways", SAMPLE_NOLOOP },
	{ "", 0 }
};

STD_SAMPLE_PICK(Journey)
STD_SAMPLE_FN(Journey)

static void journey_op4_write(UINT8, UINT8 data)
{
    if ((data & 1) == 0) {
        BurnSampleStop(0);
    }
    else if (!BurnSampleGetStatus(0)) {
        BurnSamplePlay(0);
    }
}

static INT32 JourneyInit()
{
	INT32 nRet = DrvInit(91475);

	if (nRet == 0)
	{
		ssio_set_custom_output(4, 0xff, journey_op4_write);
	}

	return nRet;
}

struct BurnDriver BurnDrvJourney = {
	"journey", NULL, "midssio", "journey", "1983",
	"Journey\0", NULL, "Bally Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, journeyRomInfo, journeyRomName, NULL, NULL, JourneySampleInfo, JourneySampleName, JourneyInputInfo, JourneyDIPInfo,
	JourneyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x80,
	480, 512, 3, 4
};


// Timber

static struct BurnRomInfo timberRomDesc[] = {
	{ "timpg0.bin",	0x4000, 0x377032ab, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "timpg1.bin",	0x4000, 0xfd772836, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "timpg2.bin",	0x4000, 0x632989f9, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "timpg3.bin",	0x2000, 0xdae8a0dc, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "tima7.bin",	0x1000, 0xc615dc3e, 2 | BRF_PRG | BRF_ESS }, //  4 ssio:cpu
	{ "tima8.bin",	0x1000, 0x83841c87, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "tima9.bin",	0x1000, 0x22bcdcd3, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "timbg1.bin",	0x4000, 0xb1cb2651, 3 | BRF_GRA },           //  7 gfx1
	{ "timbg0.bin",	0x4000, 0x2ae352c4, 3 | BRF_GRA },           //  8

	{ "timfg1.bin",	0x4000, 0x81de4a73, 4 | BRF_GRA },           //  9 gfx2
	{ "timfg0.bin",	0x4000, 0x7f3a4f59, 4 | BRF_GRA },           // 10
	{ "timfg3.bin",	0x4000, 0x37c03272, 4 | BRF_GRA },           // 11
	{ "timfg2.bin",	0x4000, 0xe2c2885c, 4 | BRF_GRA },           // 12
	{ "timfg5.bin",	0x4000, 0xeb636216, 4 | BRF_GRA },           // 13
	{ "timfg4.bin",	0x4000, 0xb7105eb7, 4 | BRF_GRA },           // 14
	{ "timfg7.bin",	0x4000, 0xd9c27475, 4 | BRF_GRA },           // 15
	{ "timfg6.bin",	0x4000, 0x244778e8, 4 | BRF_GRA },           // 16
};

STDROMPICKEXT(timber, timber, Ssioprom)
STD_ROM_FN(timber)

static INT32 TimberInit()
{
	return DrvInit(91490);
}

struct BurnDriver BurnDrvTimber = {
	"timber", NULL, "midssio", NULL, "1984",
	"Timber\0", NULL, "Bally Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, timberRomInfo, timberRomName, NULL, NULL, NULL, NULL, TimberInputInfo, TimberDIPInfo,
	TimberInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	512, 480, 4, 3
};


// Discs of Tron (Upright, 10/4/83)

/* Disc of TRON 10/4/83
referred to as version 2, known differences:
	shows "DEREZZ A RING    2000" in score table
	Sound Test menu shows sounds 1 through 16 (though the other sounds are still in the game)
	Player Input screen shows "UPRIGHT   ACTIVATE ALL DEVICES   TILT TO EXIT" 
*/
static struct BurnRomInfo dotronRomDesc[] = {
	{ "disc_tron_uprt_pg0_10-4-83.1c",		0x4000, 0x40d00195, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "disc_tron_uprt_pg1_10-4-83.2c",		0x4000, 0x5a7d1300, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "disc_tron_uprt_pg2_10-4-83.3c",		0x4000, 0xcb89c9be, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "disc_tron_uprt_pg3_10-4-83.4c",		0x2000, 0x5098faf4, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "disc_tron_uprt_snd0_10-4-83.a7",		0x1000, 0x7fb54293, 2 | BRF_PRG | BRF_ESS }, //  4 ssio:cpu
	{ "disc_tron_uprt_snd1_10-4-83.a8",		0x1000, 0xedef7326, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "disc_tron_uprt_snd2_9-22-83.a9",		0x1000, 0xe8ef6519, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "disc_tron_uprt_snd3_9-22-83.a10",	0x1000, 0x6b5aeb02, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "loc-bg2.6f",							0x2000, 0x40167124, 3 | BRF_GRA },           //  8 gfx1
	{ "loc-bg1.5f",							0x2000, 0xbb2d7a5d, 3 | BRF_GRA },           //  9

	{ "loc-g.cp4",							0x2000, 0x57a2b1ff, 4 | BRF_GRA },           // 10 gfx2
	{ "loc-h.cp3",							0x2000, 0x3bb4d475, 4 | BRF_GRA },           // 11
	{ "loc-e.cp6",							0x2000, 0xce957f1a, 4 | BRF_GRA },           // 12
	{ "loc-f.cp5",							0x2000, 0xd26053ce, 4 | BRF_GRA },           // 13
	{ "loc-c.cp8",							0x2000, 0xef45d146, 4 | BRF_GRA },           // 14
	{ "loc-d.cp7",							0x2000, 0x5e8a3ef3, 4 | BRF_GRA },           // 15
	{ "loc-a.cp0",							0x2000, 0xb35f5374, 4 | BRF_GRA },           // 16
	{ "loc-b.cp9",							0x2000, 0x565a5c48, 4 | BRF_GRA },           // 17
};

STDROMPICKEXT(dotron, dotron, Ssioprom)
STD_ROM_FN(dotron)

static void dotron_op4_write(UINT8, UINT8 data)
{
    if (has_squak) {
        midsat_write(data);
    }
}

static UINT8 dotron_ip1_read(UINT8)
{
    UINT8 tb = (BurnTrackballRead(0, 0) ) & 0x7f;

    return tb;
}

static UINT8 dotron_ip2_read(UINT8)
{
    return DrvDips[0];
}

static INT32 DotronInit()
{
	nScreenFlip = TMAP_FLIPX;

	INT32 nRet = DrvInit(91490);

	if (nRet == 0)
    {
        is_dotron = 1;
        has_dial = 1;
        ssio_set_custom_input(1, 0xff, dotron_ip1_read);
        ssio_set_custom_input(2, 0x80, dotron_ip2_read);
		ssio_set_custom_output(4, 0xff, dotron_op4_write);
	}

	return nRet;
}

static INT32 DotroneInit()
{
    has_squak = 1;

    return DotronInit();
}

struct BurnDriver BurnDrvDotron = {
	"dotron", NULL, "midssio", NULL, "1983",
	"Discs of Tron (Upright, 10/4/83)\0", NULL, "Bally Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_SHOOT, 0,
	NULL, dotronRomInfo, dotronRomName, NULL, NULL, NULL, NULL, DotronInputInfo, DotronDIPInfo,
	DotronInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	512, 480, 4, 3
};


// Discs of Tron (Upright, 9/22/83)

/* Disc of TRON 9/22/83
referred to as version 1, known differences:
	Missing "DEREZZ A RING    2000" in score table
	Sound Test menu shows sounds 1 through 19
	Player Input screen shows "UPRIGHT   ACTIVATE ALL PLAYER INPUT   SWITCHES AND DEVICES   HIT TILT TO EXIT"
*/
static struct BurnRomInfo dotronaRomDesc[] = {
	{ "disc_tron_uprt_pg0_9-22-83.1c",		0x4000, 0xba0da15f, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "disc_tron_uprt_pg1_9-22-83.2c",		0x4000, 0xdc300191, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "disc_tron_uprt_pg2_9-22-83.3c",		0x4000, 0xab0b3800, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "disc_tron_uprt_pg3_9-22-83.4c",		0x2000, 0xf98c9f8e, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "disc_tron_uprt_snd0_9-22-83.a7",		0x1000, 0x6d39bf19, 2 | BRF_PRG | BRF_ESS }, //  4 ssio:cpu
	{ "disc_tron_uprt_snd1_9-22-83.a8",		0x1000, 0xac872e1d, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "disc_tron_uprt_snd2_9-22-83.a9",		0x1000, 0xe8ef6519, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "disc_tron_uprt_snd3_9-22-83.a10",	0x1000, 0x6b5aeb02, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "loc-bg2.6f",							0x2000, 0x40167124, 3 | BRF_GRA },           //  8 gfx1
	{ "loc-bg1.5f",							0x2000, 0xbb2d7a5d, 3 | BRF_GRA },           //  9

	{ "loc-g.cp4",							0x2000, 0x57a2b1ff, 4 | BRF_GRA },           // 10 gfx2
	{ "loc-h.cp3",							0x2000, 0x3bb4d475, 4 | BRF_GRA },           // 11
	{ "loc-e.cp6",							0x2000, 0xce957f1a, 4 | BRF_GRA },           // 12
	{ "loc-f.cp5",							0x2000, 0xd26053ce, 4 | BRF_GRA },           // 13
	{ "loc-c.cp8",							0x2000, 0xef45d146, 4 | BRF_GRA },           // 14
	{ "loc-d.cp7",							0x2000, 0x5e8a3ef3, 4 | BRF_GRA },           // 15
	{ "loc-a.cp0",							0x2000, 0xb35f5374, 4 | BRF_GRA },           // 16
	{ "loc-b.cp9",							0x2000, 0x565a5c48, 4 | BRF_GRA },           // 17
};

STDROMPICKEXT(dotrona, dotrona, Ssioprom)
STD_ROM_FN(dotrona)

struct BurnDriver BurnDrvDotrona = {
	"dotrona", "dotron", "midssio", NULL, "1983",
	"Discs of Tron (Upright, 9/22/83)\0", NULL, "Bally Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_SHOOT, 0,
	NULL, dotronaRomInfo, dotronaRomName, NULL, NULL, NULL, NULL, DotronInputInfo, DotronDIPInfo,
	DotronInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	512, 480, 4, 3
};


// Discs of Tron (Environmental)

static struct BurnRomInfo dotroneRomDesc[] = {
	{ "loc-cpu1",							0x4000, 0xeee31b8c, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "loc-cpu2",							0x4000, 0x75ba6ad3, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "loc-cpu3",							0x4000, 0x94bb1a0e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "loc-cpu4",							0x2000, 0xc137383c, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "ssi_o_loc-a_disc_of_tron_aug_19",	0x1000, 0x2de6a8a8, 2 | BRF_PRG | BRF_ESS }, //  4 ssio:cpu
	{ "ssi_o_loc-b_disc_of_tron_aug_19",	0x1000, 0x4097663e, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "ssi_o_loc-c_disc_of_tron_aug_19",	0x1000, 0xf576b9e7, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "ssi_o_loc-d_disc_of_tron_aug_19",	0x1000, 0x74b0059e, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "pre.u3",								0x1000, 0xc3d0f762, 3 | BRF_PRG | BRF_ESS }, //  8 snt:cpu
	{ "pre.u4",								0x1000, 0x7ca79b43, 3 | BRF_PRG | BRF_ESS }, //  9
	{ "pre.u5",								0x1000, 0x24e9618e, 3 | BRF_PRG | BRF_ESS }, // 10

	{ "loc-bg2.6f",							0x2000, 0x40167124, 3 | BRF_GRA },           // 11 gfx1
	{ "loc-bg1.5f",							0x2000, 0xbb2d7a5d, 3 | BRF_GRA },           // 12

	{ "loc-g.cp4",							0x2000, 0x57a2b1ff, 4 | BRF_GRA },           // 13 gfx2
	{ "loc-h.cp3",							0x2000, 0x3bb4d475, 4 | BRF_GRA },           // 14
	{ "loc-e.cp6",							0x2000, 0xce957f1a, 4 | BRF_GRA },           // 15
	{ "loc-f.cp5",							0x2000, 0xd26053ce, 4 | BRF_GRA },           // 16
	{ "loc-c.cp8",							0x2000, 0xef45d146, 4 | BRF_GRA },           // 17
	{ "loc-d.cp7",							0x2000, 0x5e8a3ef3, 4 | BRF_GRA },           // 18
	{ "loc-a.cp0",							0x2000, 0xb35f5374, 4 | BRF_GRA },           // 19
	{ "loc-b.cp9",							0x2000, 0x565a5c48, 4 | BRF_GRA },           // 20

	{ "edotlamp.u2",						0x0020, 0xfb58b867, 0 | BRF_GRA },           // 21 lamp sequencer PROM
};

STDROMPICKEXT(dotrone, dotrone, Ssioprom)
STD_ROM_FN(dotrone)

struct BurnDriver BurnDrvDotrone = {
	"dotrone", "dotron", "midssio", NULL, "1983",
	"Discs of Tron (Environmental)\0", NULL, "Bally Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_ACTION | GBF_SHOOT, 0,
	NULL, dotroneRomInfo, dotroneRomName, NULL, NULL, NULL, NULL, DotronInputInfo, DotroneDIPInfo,
	DotroneInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	512, 480, 4, 3
};


// Demolition Derby

static struct BurnRomInfo demoderbRomDesc[] = {
	{ "demo_drby_pro_0",			0x4000, 0xbe7da2f3, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "demo_drby_pro_1",			0x4000, 0xc6f6604c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "demo_drby_pro_2",			0x4000, 0xfa93b9d9, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "demo_drby_pro_3",			0x4000, 0x4e964883, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "tcs_u5.bin",					0x2000, 0xeca33b2c, 3 | BRF_PRG | BRF_ESS }, //  4 M6809 Code
	{ "tcs_u4.bin",					0x2000, 0x3490289a, 3 | BRF_PRG | BRF_ESS }, //  5

	{ "demo_derby_bg_06f.6f",		0x2000, 0xcf80be19, 3 | BRF_GRA },           //  6 Background Tiles
	{ "demo_derby_bg_15f.5f",		0x2000, 0x4e173e52, 3 | BRF_GRA },           //  7

	{ "demo_derby_fg0_a4.a4",		0x4000, 0xe57a4de6, 4 | BRF_GRA },           //  8 Sprites
	{ "demo_derby_fg4_a3.a3",		0x4000, 0x55aa667f, 4 | BRF_GRA },           //  9
	{ "demo_derby_fg1_a6.a6",		0x4000, 0x70259651, 4 | BRF_GRA },           // 10
	{ "demo_derby_fg5_a5.a5",		0x4000, 0x5fe99007, 4 | BRF_GRA },           // 11
	{ "demo_derby_fg2_a8.a8",		0x4000, 0x6cab7b95, 4 | BRF_GRA },           // 12
	{ "demo_derby_fg6_a7.a7",		0x4000, 0xabfb9a8b, 4 | BRF_GRA },           // 13
	{ "demo_derby_fg3_a10.a10",		0x4000, 0x801d9b86, 4 | BRF_GRA },           // 14
	{ "demo_derby_fg7_a9.a9",		0x4000, 0x0ec3f60a, 4 | BRF_GRA },           // 15
};

STD_ROM_PICK(demoderb)
STD_ROM_FN(demoderb)

static void demoderb_op4_write(UINT8, UINT8 data)
{
	if (data & 0x40) input_playernum = 1;
	if (data & 0x80) input_playernum = 0;

    INT32 cycles = (ZetTotalCycles() * 2) / 5;
    M6809Open(0);
    M6809Run(cycles - M6809TotalCycles());
    tcs_data_write(data);
    M6809Close();
}

static UINT8 demoderb_ip1_read(UINT8)
{
    UINT8 ipt = (DrvInputs[1] & 0x03) | (~BurnTrackballRead(input_playernum, 0) << 2);

    BurnTrackballUpdate(input_playernum);

    return ipt;
}

static UINT8 demoderb_ip2_read(UINT8)
{
    UINT8 ipt = (DrvInputs[2] & 0x03) | (~BurnTrackballRead(input_playernum, 1) << 2);

    BurnTrackballUpdate(input_playernum);

    return ipt;
}

static INT32 DemoderbInit()
{
	INT32 nRet = DrvInit(91490);

	if (nRet == 0)
    {
        is_demoderb = 1;
        ssio_set_custom_input(1, 0xff, demoderb_ip1_read);
        ssio_set_custom_input(2, 0xff, demoderb_ip2_read);
        ssio_set_custom_output(4, 0xff, demoderb_op4_write);
        memmove(DrvTCSROM + 0xc000, DrvTCSROM, 0x4000);
        tcs_init(0, 0, 0, DrvTCSROM, DrvZ80RAM1); // actually m6809
	}

	return nRet;
}

struct BurnDriver BurnDrvDemoderb = {
	"demoderb", NULL, NULL, NULL, "1984",
	"Demolition Derby\0", NULL, "Bally Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, demoderbRomInfo, demoderbRomName, NULL, NULL, NULL, NULL, DemoderbInputInfo, DemoderbDIPInfo,
	DemoderbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	512, 480, 4, 3
};


// Demolition Derby (cocktail)

static struct BurnRomInfo demoderbcRomDesc[] = {
	{ "dd_pro0",				0x4000, 0x8781b367, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "dd_pro1",				0x4000, 0x4c713bfe, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "dd_pro2",				0x4000, 0xc2cbd2a4, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "tcs_u5.bin",				0x2000, 0xeca33b2c, 3 | BRF_PRG | BRF_ESS }, //  3 tcs:cpu
	{ "tcs_u4.bin",				0x2000, 0x3490289a, 3 | BRF_PRG | BRF_ESS }, //  4

	{ "demo_derby_bg_06f.6f",	0x2000, 0xcf80be19, 3 | BRF_GRA },           //  5 gfx1
	{ "demo_derby_bg_15f.5f",	0x2000, 0x4e173e52, 3 | BRF_GRA },           //  6

	{ "demo_derby_fg0_a4.a4",	0x4000, 0xe57a4de6, 4 | BRF_GRA },           //  7 gfx2
	{ "demo_derby_fg4_a3.a3",	0x4000, 0x55aa667f, 4 | BRF_GRA },           //  8
	{ "demo_derby_fg1_a6.a6",	0x4000, 0x70259651, 4 | BRF_GRA },           //  9
	{ "demo_derby_fg5_a5.a5",	0x4000, 0x5fe99007, 4 | BRF_GRA },           // 10
	{ "demo_derby_fg2_a8.a8",	0x4000, 0x6cab7b95, 4 | BRF_GRA },           // 11
	{ "demo_derby_fg6_a7.a7",	0x4000, 0xabfb9a8b, 4 | BRF_GRA },           // 12
	{ "demo_derby_fg3_a10.a10",	0x4000, 0x801d9b86, 4 | BRF_GRA },           // 13
	{ "demo_derby_fg7_a9.a9",	0x4000, 0x0ec3f60a, 4 | BRF_GRA },           // 14
};

STD_ROM_PICK(demoderbc)
STD_ROM_FN(demoderbc)

struct BurnDriver BurnDrvDemoderbc = {
	"demoderbc", "demoderb", NULL, NULL, "1984",
	"Demolition Derby (cocktail)\0", NULL, "Bally Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, demoderbcRomInfo, demoderbcRomName, NULL, NULL, NULL, NULL, DemoderbInputInfo, DemoderbDIPInfo,
	DemoderbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	512, 480, 4, 3
};


// NFL Football

static struct BurnRomInfo nflfootRomDesc[] = {
	{ "nflcpupg.1c",			0x4000, 0xd76a7a41, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "nflcpupg.2c",			0x4000, 0x2aa76168, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "nflcpupg.3c",			0x4000, 0x5ec01e09, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "nflsnd.a7",				0x1000, 0x1339be2e, 2 | BRF_PRG | BRF_ESS }, //  3 ssio:cpu
	{ "nflsnd.a8",				0x1000, 0x8630b560, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "nflsnd.a9",				0x1000, 0x1e0fe4c8, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "nfl-sqtk-11-15-83.u2",	0x1000, 0xaeddda31, 3 | BRF_PRG | BRF_ESS }, //  6 snt:cpu
	{ "nfl-sqtk-11-15-83.u3",	0x1000, 0x36229d13, 3 | BRF_PRG | BRF_ESS }, //  7
	{ "nfl-sqtk-11-15-83.u4",	0x1000, 0xb202439b, 3 | BRF_PRG | BRF_ESS }, //  8
	{ "nfl-sqtk-11-15-83.u5",	0x1000, 0xbbfe4d39, 3 | BRF_PRG | BRF_ESS }, //  9

	{ "ipu-7-9.a2",				0x2000, 0x0e083adb, 4 | BRF_GRA },           // 10 ipu
	{ "ipu-7-9.a4",				0x2000, 0x5c9c4764, 4 | BRF_GRA },           // 11

	{ "nflcpubg.6f",			0x2000, 0x6d116cd9, 5 | BRF_GRA },           // 12 gfx1
	{ "nflcpubg.5f",			0x2000, 0x5f1b0b67, 5 | BRF_GRA },           // 13

	{ "nflvidfg.cp4",			0x2000, 0xeb6b808d, 6 | BRF_GRA },           // 14 gfx2
	{ "nflvidfg.cp3",			0x2000, 0xbe21580a, 6 | BRF_GRA },           // 15
	{ "nflvidfg.cp6",			0x2000, 0x54a0bff8, 6 | BRF_GRA },           // 16
	{ "nflvidfg.cp5",			0x2000, 0x6aeba0ab, 6 | BRF_GRA },           // 17
	{ "nflvidfg.cp8",			0x2000, 0x112ee67b, 6 | BRF_GRA },           // 18
	{ "nflvidfg.cp7",			0x2000, 0x73f62392, 6 | BRF_GRA },           // 19
	{ "nflvidfg.c10",			0x2000, 0x1766dcc7, 6 | BRF_GRA },           // 20
	{ "nflvidfg.cp9",			0x2000, 0x46558146, 6 | BRF_GRA },           // 21
};

STDROMPICKEXT(nflfoot, nflfoot, Ssioprom)
STD_ROM_FN(nflfoot)

static INT32 NflfootInit()
{
	return 1;
}

struct BurnDriverD BurnDrvNflfoot = {
	"nflfoot", NULL, "midssio", NULL, "1983",
	"NFL Football\0", NULL, "Bally Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, nflfootRomInfo, nflfootRomName, NULL, NULL, NULL, NULL, TronInputInfo, TronDIPInfo, // NflfootInputInfo, NflfootDIPInfo,
	NflfootInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	512, 480, 4, 3
};

