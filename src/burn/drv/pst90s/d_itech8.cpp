// FinalBurn Neo Incredible technologies / Strata 8 bit driver module
// Based on MAME driver by Aaron Giles

// Note: grmatch, if nvram is initted randomly (as mame), and the random
// bytes are in a certain order, the blits will get messed up on the
// playscreen with occasional missing lines and missing text under the
// joystick icons.  This bug drove me nuts for a while - dink :)

// These are mechanical and won't be hooked up:
//	slikshot
//	slikshot17
//	slikshot16
//	dynobop
//	sstrike
//	stratabs

//  using "fake via" for sound:
//	rimrockn
//	gtg2
//	ninclown
//  via simply sets a firq 4x/frame on the audio cpu

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "m6809_intf.h"
#include "hd6309_intf.h"
#include "msm6295.h"
#include "burn_ym3812.h"
#include "burn_ym2203.h"
#include "burn_ym2608.h"
#include "tlc34076.h"
#include "tms34061.h"
#include "6821pia.h"
#include "burn_pal.h"
#include "burn_gun.h" // trackball here
#include "dtimer.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvM6809ROM[2];
static UINT8 *DrvGfxROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvNVRAM;
static UINT8 *DrvM6809RAM[2];

static UINT8 page_select;
static UINT8 grom_bank;
static dtimer blitter_timer;
static INT32 blit_in_progress;
static UINT8 blitter_data[16];

static INT32 fetch_offset;
static UINT8 fetch_rle_count;
static UINT8 fetch_rle_literal;
static UINT8 fetch_rle_value;

static UINT8 palcontrol;
static UINT8 scrollx;
static UINT8 bankdata;

static INT32 m_blitter;
static INT32 m_periodic;
static INT32 m_tms34061;

static UINT8 soundlatch;
static INT32 sound_response;

static INT32 graphics_length = 0;
static INT32 screen_adjustx = 0;

static void (*video_callback)(INT32 currentline);
static void (*sound_reset)();
static void (*sound_update)(INT16* pSoundBuf, INT32 nSegmentEnd);
static void (*sound_exit)();
static void (*sound_scan)(INT32 nAction, INT32* pnMin);
static cpu_core_config *pCPU;
static UINT32 maincpu_clock = 2000000;
static INT32 use_via = 0;

static INT32 is_ninclown = 0;
static INT32 is_grmatch = 0;
static UINT32 *grmatch_fb;
static UINT32 *grmatch_pal;

static UINT8 DrvJoy[8][16];
static UINT8 DrvDips[2];
static UINT16 DrvInputs[8];
static UINT8 DrvReset;

static INT32 nExtraCycles;

static UINT8 *service_ptr;
static UINT8 service_bit;

static UINT8 Diag[1];

static INT16 Analog[4];
static INT32 tb_reset = 0;

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}

static struct BurnInputInfo WfortuneInputList[] = {
	{"Coin 1",			BIT_DIGITAL,	DrvJoy[1] + 6,	"p1 coin"	},
	{"Coin 2",			BIT_DIGITAL,	DrvJoy[1] + 7,	"p2 coin"	},

	{"Red Player",		BIT_DIGITAL,	DrvJoy[1] + 3,	"p1 fire 1"	},
	{"Yellow Player",	BIT_DIGITAL,	DrvJoy[1] + 4,	"p2 fire 1"	},
	{"Blue Player",		BIT_DIGITAL,	DrvJoy[1] + 5,	"p3 fire 1"	},

	A("P1 Dial", 		BIT_ANALOG_REL, &Analog[1],		"p1 x-axis"),
	A("P2 Dial", 		BIT_ANALOG_REL, &Analog[3],		"p2 x-axis"),

	{"Service Mode",	BIT_DIGITAL,	Diag + 0,		"diag"		},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Wfortune)

static struct BurnInputInfo GrmatchInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy[1] + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy[1] + 6,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy[1] + 5,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy[1] + 4,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy[1] + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy[1] + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy[1] + 1,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy[1] + 0,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy[2] + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy[2] + 6,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy[2] + 5,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy[2] + 4,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy[2] + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy[2] + 2,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy[2] + 1,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy[2] + 0,	"p2 fire 2"	},

	{"Service Mode",	BIT_DIGITAL,	Diag + 0,		"diag"		},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Grmatch)

static struct BurnInputInfo StratabInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy[1] + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy[1] + 5,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy[1] + 3,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy[1] + 2,	"p1 fire 2"	},

	A("P1 Trackball Y", BIT_ANALOG_REL, &Analog[0],		"p1 y-axis"),
	A("P1 Trackball X", BIT_ANALOG_REL, &Analog[1],		"p1 x-axis"),

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy[1] + 6,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy[1] + 4,	"p2 start"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy[1] + 1,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy[1] + 0,	"p2 fire 2"	},

	A("P2 Trackball Y", BIT_ANALOG_REL, &Analog[2],		"p2 y-axis"),
	A("P2 Trackball X", BIT_ANALOG_REL, &Analog[3],		"p2 x-axis"),

	{"Service Mode",	BIT_DIGITAL,	Diag + 0,		"diag"		},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Stratab)

static struct BurnInputInfo GtgInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy[1] + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy[1] + 5,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy[1] + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy[1] + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy[1] + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy[1] + 2,	"p1 right"	},
	{"P1 Swing",		BIT_DIGITAL,	DrvJoy[1] + 4,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy[1] + 6,	"p2 coin"	},
	// same as P1!
	{"P2 Up",			BIT_DIGITAL,	DrvJoy[1] + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy[1] + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy[1] + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy[1] + 2,	"p2 right"	},
	{"P2 Swing",		BIT_DIGITAL,	DrvJoy[1] + 4,	"p2 fire 1"	},

	{"Service Mode",	BIT_DIGITAL,	Diag + 0,		"diag"		},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Gtg)

static struct BurnInputInfo GtgtInputList[] = {
	{"Coin",			BIT_DIGITAL,	DrvJoy[1] + 7,	"p1 coin"	},
	{"Start",			BIT_DIGITAL,	DrvJoy[1] + 5,	"p1 start"	},
	{"Face Left",		BIT_DIGITAL,	DrvJoy[1] + 3,	"p1 fire 1"	},
	{"Face Right",		BIT_DIGITAL,	DrvJoy[1] + 2,	"p1 fire 2"	},

	A("P1 Trackball X", BIT_ANALOG_REL, &Analog[0],		"p1 x-axis"),
	A("P1 Trackball Y", BIT_ANALOG_REL, &Analog[1],		"p1 y-axis"),

	{"Service Mode",	BIT_DIGITAL,	Diag + 0,		"diag"		},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Gtgt)

static struct BurnInputInfo Gtg2tInputList[] = {
	{"P1 Start",		BIT_DIGITAL,	DrvJoy[1] + 5,	"p1 start"	},
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy[1] + 7,	"p1 coin"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy[1] + 3,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy[1] + 2,	"p1 fire 2"	},
	A("P1 Trackball X", BIT_ANALOG_REL, &Analog[0],		"p1 x-axis"),
	A("P1 Trackball Y", BIT_ANALOG_REL, &Analog[1],		"p1 y-axis"),

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy[1] + 6,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy[1] + 4,	"p2 start"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy[1] + 1,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy[1] + 0,	"p2 fire 2"	},
	A("P2 Trackball X", BIT_ANALOG_REL, &Analog[2],		"p2 x-axis"),
	A("P2 Trackball Y", BIT_ANALOG_REL, &Analog[3],		"p2 y-axis"),

	{"Service Mode",	BIT_DIGITAL,	Diag + 0,		"diag"		},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Gtg2t)

static struct BurnInputInfo PokrdiceInputList[] = {
	{"Coin 1",			BIT_DIGITAL,	DrvJoy[1] + 7,	"p1 coin"	},
	{"Coin 2",			BIT_DIGITAL,	DrvJoy[1] + 1,	"p2 coin"	},
	{"Play",			BIT_DIGITAL,	DrvJoy[1] + 6,	"p1 fire 1"	},
	{"Raise",			BIT_DIGITAL,	DrvJoy[1] + 4,	"p1 fire 2"	},
	{"Upper Left",		BIT_DIGITAL,	DrvJoy[1] + 5,	"p1 fire 3"	},
	{"Lower Right",		BIT_DIGITAL,	DrvJoy[0] + 1,	"p1 fire 4"	},
	{"Middle",			BIT_DIGITAL,	DrvJoy[1] + 2,	"p1 fire 5"	},
	{"Lower Left",		BIT_DIGITAL,	DrvJoy[1] + 3,	"p1 fire 6"	},

	{"Service Mode",	BIT_DIGITAL,	Diag + 0,		"diag"		},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Pokrdice)

static struct BurnInputInfo HstennisInputList[] = {
	{"Coin 1",			BIT_DIGITAL,	DrvJoy[1] + 7,	"p1 coin"	},
	{"Coin 2",			BIT_DIGITAL,	DrvJoy[2] + 7,	"p2 coin"	},
	{"Coin 3",			BIT_DIGITAL,	DrvJoy[1] + 0,	"p3 coin"	},
	{"Coin 4",			BIT_DIGITAL,	DrvJoy[2] + 0,	"p4 coin"	},

	{"P1 Up",			BIT_DIGITAL,	DrvJoy[1] + 5,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy[1] + 4,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy[1] + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy[1] + 2,	"p1 right"	},
	{"P1 Hard",			BIT_DIGITAL,	DrvJoy[1] + 6,	"p1 fire 1"	},
	{"P1 Soft",			BIT_DIGITAL,	DrvJoy[1] + 1,	"p1 fire 2"	},

	{"P2 Up",			BIT_DIGITAL,	DrvJoy[2] + 5,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy[2] + 4,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy[2] + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy[2] + 2,	"p2 right"	},
	{"P2 Hard",			BIT_DIGITAL,	DrvJoy[2] + 6,	"p2 fire 1"	},
	{"P2 Soft",			BIT_DIGITAL,	DrvJoy[2] + 1,	"p2 fire 2"	},

	{"Service Mode",	BIT_DIGITAL,	Diag + 0,		"diag"		},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Hstennis)

static struct BurnInputInfo ArlingtnInputList[] = {
	{"Coin 1",			BIT_DIGITAL,	DrvJoy[1] + 7,	"p1 coin"	},
	{"Coin 2",			BIT_DIGITAL,	DrvJoy[2] + 7,	"p2 coin"	},
	{"Coin 3",			BIT_DIGITAL,	DrvJoy[1] + 0,	"p3 coin"	},
	{"Coin 4",			BIT_DIGITAL,	DrvJoy[2] + 0,	"p4 coin"	},

	{"Start Race",		BIT_DIGITAL,	DrvJoy[2] + 4,	"p1 start"	},
	{"Up",				BIT_DIGITAL,	DrvJoy[1] + 5,	"p1 up"		},
	{"Down",			BIT_DIGITAL,	DrvJoy[1] + 4,	"p1 down"	},
	{"Win",				BIT_DIGITAL,	DrvJoy[1] + 3,	"p1 fire 1"	},
	{"Place",			BIT_DIGITAL,	DrvJoy[1] + 2,	"p1 fire 2"	},
	{"Show",			BIT_DIGITAL,	DrvJoy[2] + 3,	"p1 fire 3"	},
	{"Colect",			BIT_DIGITAL,	DrvJoy[2] + 5,	"p1 fire 4"	},

	{"Service Mode",	BIT_DIGITAL,	Diag + 0,		"diag"		},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Arlingtn)

static struct BurnInputInfo PeggleInputList[] = {
	{"Coin 1",			BIT_DIGITAL,	DrvJoy[1] + 7,	"p1 coin"	},
	{"Coin 2",			BIT_DIGITAL,	DrvJoy[2] + 7,	"p2 coin"	},
	{"Coin 3",			BIT_DIGITAL,	DrvJoy[1] + 0,	"p3 coin"	},
	{"Coin 4",			BIT_DIGITAL,	DrvJoy[2] + 0,	"p4 coin"	},
	{"Start",			BIT_DIGITAL,	DrvJoy[1] + 6,	"p1 start"	},
	{"Left",			BIT_DIGITAL,	DrvJoy[1] + 3,	"p1 left"	},
	{"Right",			BIT_DIGITAL,	DrvJoy[1] + 2,	"p1 right"	},

	{"Service Mode",	BIT_DIGITAL,	Diag + 0,		"diag"		},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Peggle)

static struct BurnInputInfo PeggletInputList[] = {
	{"Coin 1",			BIT_DIGITAL,	DrvJoy[1] + 7,	"p1 coin"	},
	{"Coin 2",			BIT_DIGITAL,	DrvJoy[2] + 7,	"p2 coin"	},
	{"Coin 3",			BIT_DIGITAL,	DrvJoy[1] + 0,	"p3 coin"	},
	{"Coin 4",			BIT_DIGITAL,	DrvJoy[2] + 0,	"p4 coin"	},
	{"Start",			BIT_DIGITAL,	DrvJoy[1] + 6,	"p1 start"	},

	A("P1 Dial", 		BIT_ANALOG_REL, &Analog[1],		"p1 x-axis"),

	{"Service Mode",	BIT_DIGITAL,	Diag + 0,		"diag"		},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Pegglet)

static struct BurnInputInfo NeckneckInputList[] = {
	{"Coin 1",			BIT_DIGITAL,	DrvJoy[1] + 7,	"p1 coin"	},
	{"Coin 2",			BIT_DIGITAL,	DrvJoy[2] + 7,	"p2 coin"	},
	{"Coin 3",			BIT_DIGITAL,	DrvJoy[1] + 0,	"p3 coin"	},
	{"Coin 4",			BIT_DIGITAL,	DrvJoy[2] + 0,	"p4 coin"	},

	{"Start",			BIT_DIGITAL,	DrvJoy[1] + 4,	"p1 start"	},
	{"Horse 1",			BIT_DIGITAL,	DrvJoy[1] + 5,	"p1 fire 1"	},
	{"Horse 2",			BIT_DIGITAL,	DrvJoy[1] + 3,	"p1 fire 2"	},
	{"Horse 3",			BIT_DIGITAL,	DrvJoy[1] + 2,	"p1 fire 3"	},
	{"Horse 4",			BIT_DIGITAL,	DrvJoy[2] + 3,	"p1 fire 4"	},
	{"Horse 5",			BIT_DIGITAL,	DrvJoy[2] + 5,	"p1 fire 5"	},
	{"Horse 6",			BIT_DIGITAL,	DrvJoy[2] + 4,	"p1 fire 6"	},

	{"Service Mode",	BIT_DIGITAL,	Diag + 0,		"diag"		},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Neckneck)

static struct BurnInputInfo RimrocknInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy[1] + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy[3] + 7,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy[3] + 6,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy[3] + 5,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy[3] + 4,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy[3] + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy[3] + 2,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy[3] + 1,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy[1] + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy[4] + 7,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy[4] + 6,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy[4] + 5,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy[4] + 4,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy[4] + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy[4] + 2,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy[4] + 1,	"p2 fire 2"	},

	{"P3 Coin",			BIT_DIGITAL,	DrvJoy[1] + 4,	"p3 coin"	},
	{"P3 Start",		BIT_DIGITAL,	DrvJoy[5] + 7,	"p3 start"	},
	{"P3 Up",			BIT_DIGITAL,	DrvJoy[5] + 6,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	DrvJoy[5] + 5,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	DrvJoy[5] + 4,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy[5] + 3,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy[5] + 0,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy[5] + 1,	"p3 fire 2"	},

	{"P4 Coin",			BIT_DIGITAL,	DrvJoy[1] + 5,	"p4 coin"	},
	{"P4 Start",		BIT_DIGITAL,	DrvJoy[6] + 7,	"p4 start"	},
	{"P4 Up",			BIT_DIGITAL,	DrvJoy[6] + 6,	"p4 up"		},
	{"P4 Down",			BIT_DIGITAL,	DrvJoy[6] + 5,	"p4 down"	},
	{"P4 Left",			BIT_DIGITAL,	DrvJoy[6] + 4,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy[6] + 3,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy[6] + 0,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy[6] + 1,	"p4 fire 2"	},

	{"Service Mode",	BIT_DIGITAL,	Diag + 0,		"diag"		},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy[1] + 0,	"service"	},
	{"Service",			BIT_DIGITAL,	DrvJoy[7] + 0,	"service2"	},
	{"Service",			BIT_DIGITAL,	DrvJoy[7] + 1,	"service3"	},
	{"Service",			BIT_DIGITAL,	DrvJoy[7] + 2,	"service4"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Rimrockn)

static struct BurnInputInfo NinclownInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy[0] + 13,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy[1] + 9,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy[1] + 10,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy[1] + 11,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy[1] + 12,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy[1] + 13,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy[1] + 15,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy[1] + 14,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy[1] + 8,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy[0] + 12,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy[2] + 9,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy[2] + 10,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy[2] + 11,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy[2] + 12,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy[2] + 13,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy[2] + 15,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy[2] + 14,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy[2] + 8,	"p2 fire 3"	},

	{"Service Mode",	BIT_DIGITAL,	Diag + 0,		"diag"		},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy[0] + 8,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Ninclown)

static struct BurnInputInfo Gtg2InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy[0] + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy[1] + 7,	"p1 start"	},
	{"P1 Face Left",	BIT_DIGITAL,	DrvJoy[1] + 2,	"p1 fire 1"	},
	{"P1 Face Right",	BIT_DIGITAL,	DrvJoy[1] + 1,	"p1 fire 2"	},
	A("P1 Trackball X", BIT_ANALOG_REL, &Analog[0],		"p1 x-axis"),
	A("P1 Trackball Y", BIT_ANALOG_REL, &Analog[1],		"p1 y-axis"),

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy[0] + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy[2] + 7,	"p2 start"	},
	{"P2 Face Left",	BIT_DIGITAL,	DrvJoy[2] + 2,	"p2 fire 1"	},
	{"P2 Face Right",	BIT_DIGITAL,	DrvJoy[2] + 1,	"p2 fire 2"	},
	A("P2 Trackball X", BIT_ANALOG_REL, &Analog[2],		"p2 x-axis"),
	A("P2 Trackball Y", BIT_ANALOG_REL, &Analog[3],		"p2 y-axis"),

	{"Service Mode",	BIT_DIGITAL,	Diag + 0,		"diag"		},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Gtg2)

static struct BurnInputInfo GpgolfInputList[] = {
	{"Coin 1",			BIT_DIGITAL,	DrvJoy[0] + 2,	"p1 coin"	},
	{"Coin 2",			BIT_DIGITAL,	DrvJoy[0] + 3,	"p2 coin"	},
	{"Start",			BIT_DIGITAL,	DrvJoy[1] + 7,	"p1 start"	},
	{"Up",				BIT_DIGITAL,	DrvJoy[1] + 6,	"p1 up"		},
	{"Down",			BIT_DIGITAL,	DrvJoy[1] + 5,	"p1 down"	},
	{"Left",			BIT_DIGITAL,	DrvJoy[1] + 4,	"p1 left"	},
	{"Right",			BIT_DIGITAL,	DrvJoy[1] + 3,	"p1 right"	},
	{"Swing",			BIT_DIGITAL,	DrvJoy[1] + 2,	"p1 fire 1"	},

	{"Service Mode",	BIT_DIGITAL,	Diag + 0,		"diag"		},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Gpgolf)

static struct BurnDIPInfo WfortuneDIPList[]=
{
	DIP_OFFSET(0x09)
	{0x00, 0xff, 0xff, 0xfe, NULL				},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x00, 0x01, 0x08, 0x08, "Upright"			},
	{0x00, 0x01, 0x08, 0x00, "Cocktail"			},
};

STDDIPINFO(Wfortune)

static struct BurnDIPInfo GrmatchDIPList[]=
{
	DIP_OFFSET(0x12)
	{0x00, 0xff, 0xff, 0xfe, NULL				},

	{0   , 0xfe, 0   ,    2, "Adj's Lockout"	},
	{0x00, 0x01, 0x08, 0x08, "Off"				},
	{0x00, 0x01, 0x08, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Coinage"			},
	{0x00, 0x01, 0x10, 0x00, "2 Coins 1 Credits"},
	{0x00, 0x01, 0x10, 0x10, "1 Coin  1 Credits"},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x00, 0x01, 0x20, 0x20, "1"				},
	{0x00, 0x01, 0x20, 0x00, "3"				},
};

STDDIPINFO(Grmatch)

static struct BurnDIPInfo StratabDIPList[]=
{
	DIP_OFFSET(0x0e)
	{0x00, 0xff, 0xff, 0xf8, NULL				},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x00, 0x01, 0x08, 0x08, "Upright"			},
	{0x00, 0x01, 0x08, 0x00, "Cocktail"			},
};

STDDIPINFO(Stratab)

static struct BurnDIPInfo GtgDIPList[]=
{
	DIP_OFFSET(0x0f)
	{0x00, 0xff, 0xff, 0xfe, NULL				},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x00, 0x01, 0x08, 0x08, "Upright"			},
	{0x00, 0x01, 0x08, 0x00, "Cocktail"			},
};

STDDIPINFO(Gtg)

static struct BurnDIPInfo GtgtDIPList[]=
{
	DIP_OFFSET(0x08)
	{0x00, 0xff, 0xff, 0xfe, NULL				},
};

STDDIPINFO(Gtgt)

static struct BurnDIPInfo Gtg2tDIPList[]=
{
	DIP_OFFSET(0x0e)
	{0x00, 0xff, 0xff, 0xf8, NULL				},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x00, 0x01, 0x08, 0x08, "Upright"			},
	{0x00, 0x01, 0x08, 0x00, "Cocktail"			},
};

STDDIPINFO(Gtg2t)

static struct BurnDIPInfo PokrdiceDIPList[]=
{
	DIP_OFFSET(0x0a)
	{0x00, 0xff, 0xff, 0xfe, NULL				},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x00, 0x01, 0x08, 0x08, "Upright"			},
	{0x00, 0x01, 0x08, 0x00, "Cocktail"			},

	{0   , 0xfe, 0   ,    2, "Unknown"			},
	{0x00, 0x01, 0x10, 0x00, "Off"				},
	{0x00, 0x01, 0x10, 0x10, "On"				},
};

STDDIPINFO(Pokrdice)

static struct BurnDIPInfo HstennisDIPList[]=
{
	DIP_OFFSET(0x12)
	{0x00, 0xff, 0xff, 0xfe, NULL				},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x00, 0x01, 0x08, 0x08, "Upright"			},
	{0x00, 0x01, 0x08, 0x00, "Cocktail"			},

	{0   , 0xfe, 0   ,    2, "Unknown"			},
	{0x00, 0x01, 0x10, 0x00, "Off"				},
	{0x00, 0x01, 0x10, 0x10, "On"				},
};

STDDIPINFO(Hstennis)

static struct BurnDIPInfo ArlingtnDIPList[]=
{
	DIP_OFFSET(0x0d)
	{0x00, 0xff, 0xff, 0xfe, NULL				},

	{0   , 0xfe, 0   ,    2, "Unknown"			},
	{0x00, 0x01, 0x08, 0x00, "Off"				},
	{0x00, 0x01, 0x08, 0x08, "On"				},
};

STDDIPINFO(Arlingtn)

static struct BurnDIPInfo PeggleDIPList[]=
{
	DIP_OFFSET(0x09)
	{0x00, 0xff, 0xff, 0xfe, NULL				},
};

STDDIPINFO(Peggle)

static struct BurnDIPInfo PeggletDIPList[]=
{
	DIP_OFFSET(0x08)
	{0x00, 0xff, 0xff, 0xfe, NULL				},
};

STDDIPINFO(Pegglet)

static struct BurnDIPInfo NeckneckDIPList[]=
{
	DIP_OFFSET(0x0d)
	{0x00, 0xff, 0xff, 0xf6, NULL				},

	{0   , 0xfe, 0   ,    2, "Unknown"			},
	{0x00, 0x01, 0x08, 0x00, "Off"				},
	{0x00, 0x01, 0x08, 0x08, "On"				},
};

STDDIPINFO(Neckneck)

static struct BurnDIPInfo RimrocknDIPList[]=
{
	DIP_OFFSET(0x026)
	{0x00, 0xff, 0xff, 0x7e, NULL				},
	{0x01, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    4, "Cabinet"			},
	{0x01, 0x01, 0x18, 0x18, "1 player"			},
	{0x01, 0x01, 0x18, 0x10, "2 players"		},
	{0x01, 0x01, 0x18, 0x08, "3 players"		},
	{0x01, 0x01, 0x18, 0x00, "4 players"		},

	{0   , 0xfe, 0   ,    2, "Coin Slots"		},
	{0x01, 0x01, 0x20, 0x04, "Common"			},
	{0x01, 0x01, 0x20, 0x00, "Individual"		},

	{0   , 0xfe, 0   ,    2, "Video Sync"		},
	{0x01, 0x01, 0x40, 0x02, "Positive"			},
	{0x01, 0x01, 0x40, 0x00, "Negative"			},
};

STDDIPINFO(Rimrockn)

static struct BurnDIPInfo NinclownDIPList[]=
{
	DIP_OFFSET(0x15)
	{0x00, 0xff, 0xff, 0xff, NULL				},
};

STDDIPINFO(Ninclown)

static struct BurnDIPInfo Gtg2DIPList[]=
{
	DIP_OFFSET(0x0e)
	{0x00, 0xff, 0xff, 0xfe, NULL				},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x00, 0x01, 0x40, 0x40, "Upright"			},
	{0x00, 0x01, 0x40, 0x00, "Cocktail"			},
};

STDDIPINFO(Gtg2)

static struct BurnDIPInfo GpgolfDIPList[]=
{
	DIP_OFFSET(0x0a)
	{0x00, 0xff, 0xff, 0xfe, NULL			},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x00, 0x01, 0x40, 0x40, "Upright"			},
	{0x00, 0x01, 0x40, 0x00, "Cocktail"			},
};

STDDIPINFO(Gpgolf)

#define VRAM_MASK				(0x40000-1)

#define BLITTER_ADDRHI          blitter_data[0]
#define BLITTER_ADDRLO          blitter_data[1]
#define BLITTER_FLAGS           blitter_data[2]
#define BLITTER_STATUS          blitter_data[3]
#define BLITTER_WIDTH           blitter_data[4]
#define BLITTER_HEIGHT          blitter_data[5]
#define BLITTER_MASK            blitter_data[6]
#define BLITTER_OUTPUT          blitter_data[7]
#define BLITTER_XSTART          blitter_data[8]
#define BLITTER_YCOUNT          blitter_data[9]
#define BLITTER_XSTOP           blitter_data[10]
#define BLITTER_YSKIP           blitter_data[11]

#define BLITFLAG_SHIFT          0x01
#define BLITFLAG_XFLIP          0x02
#define BLITFLAG_YFLIP          0x04
#define BLITFLAG_RLE            0x08
#define BLITFLAG_TRANSPARENT    0x10

static UINT8 inline fetch_next_raw()
{
	return DrvGfxROM[fetch_offset++ % graphics_length];
}

static inline void consume_raw(int count)
{
	fetch_offset += count;
}

static inline UINT8 fetch_next_rle()
{
	if (fetch_rle_count == 0)
	{
		fetch_rle_count = DrvGfxROM[fetch_offset++ % graphics_length];
		fetch_rle_literal = fetch_rle_count & 0x80;
		fetch_rle_count &= 0x7f;

		if (!fetch_rle_literal)
			fetch_rle_value = DrvGfxROM[fetch_offset++ % graphics_length];
	}

	fetch_rle_count--;
	if (fetch_rle_literal)
		fetch_rle_value = DrvGfxROM[fetch_offset++ % graphics_length];

	return fetch_rle_value;
}

static inline void consume_rle(int count)
{
	while (count)
	{
		int num_to_consume;

		if (fetch_rle_count == 0)
		{
			fetch_rle_count = DrvGfxROM[fetch_offset++ % graphics_length];
			fetch_rle_literal = fetch_rle_count & 0x80;
			fetch_rle_count &= 0x7f;

			if (!fetch_rle_literal)
				fetch_rle_value = DrvGfxROM[fetch_offset++ % graphics_length];
		}

		num_to_consume = (count < fetch_rle_count) ? count : fetch_rle_count;
		count -= num_to_consume;

		fetch_rle_count -= num_to_consume;
		if (fetch_rle_literal)
			fetch_offset += num_to_consume;
	}
}

static void perform_blit()
{
	UINT32 addr = tms34061_xyaddress() | ((tms34061_xyoffset() & 0x300) << 8);
	UINT8 shift = (BLITTER_FLAGS & BLITFLAG_SHIFT) ? 4 : 0;
	int transparent = (BLITTER_FLAGS & BLITFLAG_TRANSPARENT);
	int ydir = (BLITTER_FLAGS & BLITFLAG_YFLIP) ? -1 : 1;
	int xdir = (BLITTER_FLAGS & BLITFLAG_XFLIP) ? -1 : 1;
	int xflip = (BLITTER_FLAGS & BLITFLAG_XFLIP);
	int rle = (BLITTER_FLAGS & BLITFLAG_RLE);
	int color = tms34061_latch_read();
	int width = BLITTER_WIDTH;
	int height = BLITTER_HEIGHT;
	UINT8 transmaskhi, transmasklo;
	UINT8 mask = BLITTER_MASK;
	UINT8 skip[3];
	int x, y;

	fetch_offset = (grom_bank << 16) | (BLITTER_ADDRHI << 8) | BLITTER_ADDRLO;
	fetch_rle_count = 0;

	if (rle)
		fetch_offset += 2;

	if (BLITTER_OUTPUT & 0x40)
		transmaskhi = 0xf0, transmasklo = 0x0f;
	else
		transmaskhi = transmasklo = 0xff;

	skip[0] = BLITTER_XSTART;
	skip[1] = (width <= BLITTER_XSTOP) ? 0 : width - 1 - BLITTER_XSTOP;
	if (xdir == -1) { int temp = skip[0]; skip[0] = skip[1]; skip[1] = temp; }
	width -= skip[0] + skip[1];

	if (ydir == 1)
	{
		skip[2] = (height <= BLITTER_YCOUNT) ? 0 : height - BLITTER_YCOUNT;
		if (BLITTER_YSKIP > 1) height -= BLITTER_YSKIP - 1;
	}
	else
	{
		skip[2] = (height <= BLITTER_YSKIP) ? 0 : height - BLITTER_YSKIP;
		if (BLITTER_YCOUNT > 1) height -= BLITTER_YCOUNT - 1;
	}

	for (y = 0; y < skip[2]; y++)
	{
		addr += xdir * (width + skip[0] + skip[1]);
		if (rle)
			consume_rle(width + skip[0] + skip[1]);
		else
			consume_raw(width + skip[0] + skip[1]);

		addr -= xdir;
		addr += ydir * 256;
		addr &= VRAM_MASK;
		xdir = -xdir;
	}

	for (y = skip[2]; y < height; y++)
	{
		addr += xdir * skip[y & 1];
		if (rle)
			consume_rle(skip[y & 1]);
		else
			consume_raw(skip[y & 1]);

		for (x = 0; x < width; x++)
		{
			UINT8 pix = rle ? fetch_next_rle() : fetch_next_raw();

			if (xflip && transmaskhi != 0xff)
				pix = (pix >> 4) | (pix << 4);
			pix &= mask;

			if (!transparent || (pix & transmaskhi))
			{
				tms34061_get_vram_pointer()[addr] = (tms34061_get_vram_pointer()[addr] & (0x0f << shift)) | ((pix & 0xf0) >> shift);
				tms34061_get_latchram_pointer()[addr] = (tms34061_get_latchram_pointer()[addr] & (0x0f << shift)) | ((color & 0xf0) >> shift);
			}

			if (!transparent || (pix & transmasklo))
			{
				UINT32 addr1 = addr + shift/4;
				tms34061_get_vram_pointer()[addr1] = (tms34061_get_vram_pointer()[addr1] & (0xf0 >> shift)) | ((pix & 0x0f) << shift);
				tms34061_get_latchram_pointer()[addr1] = (tms34061_get_latchram_pointer()[addr1] & (0xf0 >> shift)) | ((color & 0x0f) << shift);
			}

			addr += xdir;
			addr &= VRAM_MASK;
		}

		addr += xdir * skip[~y & 1];
		if (rle)
			consume_rle(skip[~y & 1]);
		else
			consume_raw(skip[~y & 1]);

		addr -= xdir;
		addr += ydir * 256;
		addr &= VRAM_MASK;
		xdir = -xdir;
	}
}

static void update_interrupts(INT32 periodic, INT32 tms34061, INT32 blitter)
{
	if (periodic != -1) m_periodic = periodic;
	if (tms34061 != -1) m_tms34061 = tms34061;
	if (blitter !=  -1) m_blitter  = blitter;

	if (pCPU == &SekConfig) // 68K
	{
		SekSetVIRQLine(0, 2, m_blitter ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
		SekSetVIRQLine(0, 3, m_periodic ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
		return;
	}
	else
	{
		if (periodic != -1) pCPU->irq(0, 0x20, periodic ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
		if (tms34061 != -1) pCPU->irq(0, 0x00, tms34061 ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
		if (blitter != -1) pCPU->irq(0, 0x01, blitter  ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
	}
}

static UINT8 blitter_read(INT32 offset)
{
	offset /= 2;
	int result = blitter_data[offset];

	if (offset == 3)
	{
		update_interrupts(-1, -1, 0);

		if (blit_in_progress)
			result |= 0x80;
		else
			result &= 0x7f;
	}

	if (offset >= 12 && offset <= 15) {
		const INT32 tb = offset - 12;
		result = BurnTrackballReadInterpolated(tb, tms34061_current_scanline);
		if (tb_reset) BurnTrackballReadReset(tb);
	}

	return result;
}

static void blitter_timer_cb(INT32 param)
{
	update_interrupts(-1, -1, 1);
	blit_in_progress = 0;
}

static void blitter_write(INT32 offset, UINT8 data)
{
	offset /= 2;
	blitter_data[offset] = data;

	if (offset == 3)
	{
		perform_blit();
		blit_in_progress = 1;
		INT32 cyc = (BLITTER_WIDTH * BLITTER_HEIGHT + 12);
		// timer is running @ maincpu_clock, so we scale it from 12mhz/4 to the maincpu speed
		INT32 cyc2 = clockscale_cycles(12000000/4, cyc, maincpu_clock);
		blitter_timer.start(cyc2, -1, 1, 0);
	}
}

static void tms_write(INT32 offset, INT32 data)
{
	INT32 func = (offset >> 9) & 7;
	INT32 col = (offset >> 0) & 0xff;

	if (func == 0 || func == 2)
		col ^= 2;

	tms34061_write(col, 0xff, func, data);
}

static UINT8 tms_read(INT32 offset)
{
	INT32 func = (offset >> 9) & 7;
	INT32 col = (offset >> 0) & 0xff;

	if (func == 0 || func == 2)
		col ^= 2;

	return tms34061_read(col, 0xff, func);
}

static void bankswitch(INT32 data)
{
	bankdata = data;
	M6809MapMemory(DrvM6809ROM[0] + (0x4000 * ((data >> 5) & 1)), 0x4000, 0x7fff, MAP_ROM);
}

static void tmshi_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf000) == 0x1000) {
		tms_write(address & 0xfff, data);
		return;
	}

	if ((address & 0xffe0) == 0x01c0) {
		if (((address / 2) & 0xf) == 7) bankswitch(data);
		blitter_write(address & 0x1f, data);
		return;
	}

	if ((address & 0xffe0) == 0x01e0) {
		tlc34076_write((address >> 1) & 0xf, data);
		return;
	}

	switch (address)
	{
		case 0x0100:
		return; // nop

		case 0x0120:
			soundlatch = data;
			M6809SetIRQLine(1, 0, CPU_IRQSTATUS_ACK);
		return;

		case 0x0140:
			grom_bank = data;
		return;

		case 0x0160:
			video_callback(tms34061_current_scanline);
			page_select = data;
		return;

		case 0x0180:
			tms34061_latch_write(data);
		return;

		case 0x01a0: // nmi ack?
		return;
	}

	bprintf(0, _T("mw  %x  %x\n"), address, data);
}

static UINT8 tmshi_main_read(UINT16 address)
{
	if ((address & 0xf000) == 0x1000) {
		return tms_read(address & 0xfff);
	}

	if ((address & 0xffe0) == 0x01c0) {
		return blitter_read(address & 0x1f);
	}

	switch (address)
	{
		case 0x0140:
			return (DrvInputs[0] & 0xfe) | sound_response;

		case 0x0160:
			return DrvInputs[1];

		case 0x0180:
			return DrvInputs[2];
	}

	bprintf(0, _T("mr %x\n"), address);

	return 0;
}

static void tmslo_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf000) == 0x0000) {
		tms_write(address & 0xfff, data);
		return;
	}

	if ((address & 0xffe0) == 0x11c0) {
		if (((address / 2) & 0xf) == 7) bankswitch(data);
		blitter_write(address & 0x1f, data);
		return;
	}

	if ((address & 0xffe0) == 0x11e0) {
		tlc34076_write((address >> 1) & 0xf, data);
		return;
	}

	switch (address)
	{
		case 0x1100:
		return; // nop

		case 0x1120:
			soundlatch = data;
			M6809SetIRQLine(1, 0, CPU_IRQSTATUS_ACK);
		return;

		case 0x1140:
			grom_bank = data;
		return;

		case 0x1160:
			page_select = data;
		return;

		case 0x1180:
			tms34061_latch_write(data);
		return;

		case 0x11a0: // nmi ack?
		return;
	}
}

static UINT8 tmslo_main_read(UINT16 address)
{
	if ((address & 0xf000) == 0x0000) {
		return tms_read(address & 0xfff);
	}

	if ((address & 0xffe0) == 0x11c0) {
		return blitter_read(address & 0x1f);
	}

	switch (address)
	{
		case 0x1140:
			return (DrvInputs[0] & 0xfe) ^ sound_response;

		case 0x1160:
			return DrvInputs[1];

		case 0x1180:
			return DrvInputs[2];
	}

	return 0;
}

static void gtg2_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf000) == 0x1000) {
		tms_write(address & 0xfff, data);
		return;
	}

	if ((address & 0xffe0) == 0x0180) {
		if (((address / 2) & 0xf) == 7) bankswitch(data);
		blitter_write(address & 0x1f, data);
		return;
	}

	if ((address & 0xffe0) == 0x0140) {
		tlc34076_write((address >> 1) & 0xf, data);
		return;
	}

	switch (address)
	{
		case 0x0100: // nmi ack?
		return;

		case 0x0120:
			page_select = data;
		return;

		case 0x0160:
			grom_bank = data;
		return;

		case 0x01c0:
			soundlatch = ((data & 0x80) >> 7) | ((data & 0x5d) << 1) | ((data & 0x20) >> 3) | ((data & 0x02) << 5);
			M6809SetIRQLine(1, 0, CPU_IRQSTATUS_ACK); //
		return;

		case 0x01e0:
			tms34061_latch_write(data);
		return;
	}
}

static UINT8 gtg2_main_read(UINT16 address)
{
	if ((address & 0xf000) == 0x1000) {
		return tms_read(address & 0xfff);
	}

	if ((address & 0xffe0) == 0x0180) {
		return blitter_read(address & 0x1f);
	}

	switch (address)
	{
		case 0x0100:
			return (DrvInputs[0] & 0xfe) | sound_response;

		case 0x0120:
			return DrvInputs[1];

		case 0x0140:
			return DrvInputs[2];
	}

	return 0;
}

static void rimrockn_bank(INT32 data)
{
	bankdata = data;
	HD6309MapMemory(DrvM6809ROM[0] + (data & 3) * 0x4000, 0x4000, 0x7fff, MAP_ROM);
}

static void rimrockn_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xffe0) == 0x01c0) {
		blitter_write(address & 0x1f, data);
		return;
	}

	switch (address)
	{
		case 0x01a0:
			rimrockn_bank(data);
		return;
	}

	tmshi_main_write(address, data);
}

static UINT8 rimrockn_main_read(UINT16 address)
{
	switch (address)
	{
		case 0x0140:
			return 0xff;

		case 0x0160:
			return (DrvInputs[1] & 0x7d) | (DrvDips[0] & 0x02) | (sound_response << 7);

		case 0x0161:
			return DrvInputs[3];

		case 0x0162:
			return DrvInputs[4];

		case 0x0163:
			return DrvInputs[5];

		case 0x0164:
			return DrvInputs[6];

		case 0x0165:
			return (DrvInputs[7] & ~0x78) | (DrvDips[1] & 0x78);
	}

	return tmshi_main_read(address);
}

static void grmatch_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xffe0) == 0x01e0) {
		// grmatch doesn't use tlc pal control
		return; // nop
	}

	switch (address)
	{
		case 0x0160:
			palcontrol = data;
		return;

		case 0x0180:
			scrollx = data;
		return;
	}

	tmshi_main_write(address, data);
}

static void sync_timer68k()
{
//	INT32 cyc = (SekTotalCycles()/4) - timerTotalCycles();
//	if (cyc > 0) timerRun(cyc);
}

static void __fastcall ninclown_main_write_word(UINT32 address, UINT16 data)
{
	sync_timer68k();

	if ((address & 0xffffe0) == 0x100300) {
		blitter_write((address & 0x1e)+0, data >> 8);
		blitter_write((address & 0x1e)+1, data >> 0);
		return;
	}

	if ((address & 0xffff80) == 0x100380) {
		address &= 0x7f;
		tlc34076_write((address >> 5) & 0xf, data >> 8);
		return;
	}

	if ((address & 0xfff000) == 0x110000) {
		tms_write((address & 0xffe)+0, data >> 8);
		tms_write((address & 0xffe)+1, data & 0xff);
		return;
	}

	switch (address)
	{
		case 0x100080:
			soundlatch = data;
			M6809SetIRQLine(1, 0, CPU_IRQSTATUS_ACK);
		return;

		case 0x100100:
			grom_bank = data;
		return;

		case 0x100180:
			video_callback(tms34061_current_scanline);
			page_select = ~data >> 8;
		return;

		case 0x100240:
			tms34061_latch_write(data);
		return;

		case 0x100280:
		case 0x100281: // nop
		return;
	}

//	bprintf (0, _T("MWW: %5.5x, %4.4x PC(%5.5x)\n"), address, data, SekGetPC(-1));
}

static void __fastcall ninclown_main_write_byte(UINT32 address, UINT8 data)
{
	sync_timer68k();

	if ((address & 0xffffe0) == 0x100300) {
		blitter_write(address & 0x1f, data >> 0);
		return;
	}

	if ((address & 0xffff81) == 0x100380) {
		address &= 0x7f;
		tlc34076_write((address >> 5) & 0xf, data);
		return;
	}

	if ((address & 0xfff000) == 0x110000) {
		tms_write(address & 0xfff, data);
		return;
	}

	switch (address)
	{
		case 0x100080:
			soundlatch = data;
			M6809SetIRQLine(1, 0, CPU_IRQSTATUS_ACK);
		return;

		case 0x100100:
			grom_bank = data;
		return;

		case 0x100180:
			video_callback(tms34061_current_scanline);
			page_select = ~data;
		return;

		case 0x100240:
			tms34061_latch_write(data);
		return;

		case 0x100280:// nop
		return;
	}
//	bprintf (0, _T("MWB: %5.5x, %2.2x PC(%5.5x)\n"), address, data, SekGetPC(-1));
}

static UINT16 __fastcall ninclown_main_read_word(UINT32 address)
{
	sync_timer68k();

	if ((address & 0xfc0000) == 0x40000) {
		return 0xd840;
	}

	if ((address & 0xffffe0) == 0x100300) {
		INT32 ret = blitter_read((address & 0x1e)+0);
		ret |= blitter_read((address & 0x1e)+1);
		return ret;
	}

	if ((address & 0xffff80) == 0x100380) {
		address &= 0x7f;
		return tlc34076_read((address >> 5) & 0xf) << 8;
	}

	if ((address & 0xfff000) == 0x110000) {
		UINT16 ret = tms_read(address & 0xffe) << 8;
		if ((address & 0xe00) == 0x200) {
			ret |= ret >> 8;
		} else {
			ret |= tms_read((address & 0xffe)+1);
		}

		return ret;
	}

	switch (address)
	{
		case 0x100100:
			return DrvInputs[0];

		case 0x100180:
			return DrvInputs[1];

		case 0x100280:
			return DrvInputs[2];
	}

//	bprintf (0, _T("MRW: %5.5x, PC(%5.5x)\n"), address, SekGetPC(-1));

	return 0;
}

static UINT8 __fastcall ninclown_main_read_byte(UINT32 address)
{
	sync_timer68k();

	//	bprintf (0, _T("MRB: %5.5x >> "), address);
	return ninclown_main_read_word(address & ~1) >> ((~address & 1) << 3);
}

static void itech8_ym2203_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x2000:
		case 0x2001:
		case 0x2002:
		case 0x2003:
			BurnYM2203Write(0, address & 1, data);
		return;

		case 0x4000:
			MSM6295Write(0, data);
		return;
	}
}

static UINT8 itech8_ym2203_read(UINT16 address)
{
	switch (address)
	{
		case 0x1000:
			M6809SetIRQLine(0, CPU_IRQSTATUS_NONE);
			return soundlatch;

		case 0x2000:
		case 0x2001:
		case 0x2002:
		case 0x2003:
			return BurnYM2203Read(0, address & 1);

		case 0x4000:
			return MSM6295Read(0);
	}

	return 0;
}

static void itech8_ym3812_write(UINT16 address, UINT8 data)
{
	if (use_via && (address & 0xfff0) == 0x5000) {
		// via hacky faker
		M6809SetIRQLine(1, 1, CPU_IRQSTATUS_NONE); // close firq (via-generated)
		return;
	}

	switch (address)
	{
		case 0x0000:
		return; // nop

		case 0x2000:
		case 0x2001:
			BurnYM3812Write(0, address & 1, data);
		return;

		case 0x4000:
			MSM6295Write(0, data);
		return;

		case 0x5000:
		case 0x5001:
		case 0x5002:
		case 0x5003:
			pia_write(0, address & 0x3ff, data);
		return;
	}
}

static UINT8 itech8_ym3812_read(UINT16 address)
{
	if (use_via && (address & 0xfff0) == 0x5000) {
		// via hacky faker
		return 0;
	}

	switch (address)
	{
		case 0x1000:
			M6809SetIRQLine(0, CPU_IRQSTATUS_NONE);
			return soundlatch;

		case 0x2000:
		case 0x2001:
			return BurnYM3812Read(0, address & 1);

		case 0x4000:
			return MSM6295Read(0);

		case 0x5000:
		case 0x5001:
		case 0x5002:
		case 0x5003:
			return pia_read(0, address & 0x3ff);
	}

	return 0;
}

static void itech8_ym2608_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x1000:
		return; // nop

		case 0x4000:
		case 0x4001:
		case 0x4002:
		case 0x4003:
			BurnYM2608Write(address & 0x03, data);
		return;
	}
}

static UINT8 itech8_ym2608_read(UINT16 address)
{
	switch (address)
	{
		case 0x2000:
			M6809SetIRQLine(0, CPU_IRQSTATUS_NONE);
			return soundlatch;

		case 0x4000:
		case 0x4001:
		case 0x4002:
		case 0x4003:
			return BurnYM2608Read(address & 3);
	}

	return 0;
}

static void partial_update()
{
	video_callback(tms34061_current_scanline);
//	bprintf(0, _T("fr# %d: partial update\tscanline %d\n"), nCurrentFrame, tms34061_current_scanline);
}

static void generate_tms34061_interrupt(INT32 state)
{
//	if (state) bprintf(0, _T("fr# %d: generate 34061 interrupt!  %x\tscanline %d\n"), nCurrentFrame, state, tms34061_current_scanline);

	update_interrupts(-1, state, -1);
}

static void ym2203_write_B(UINT32, UINT32 data)
{
	sound_response = data & 0x01;
}

static void pia_portb_out(UINT16, UINT8 data)
{
	sound_response = data & 0x01;
}

static void DrvFMIrqHandler(INT32, INT32 nStatus)
{
	M6809SetIRQLine(1, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	pCPU->open(0);
	if (pCPU == &SekConfig) memcpy (DrvNVRAM, Drv68KROM, 8); // copy vectors
	pCPU->reset();
	pCPU->close();

	M6809Open(1);
	M6809Reset();
	sound_reset();
	MSM6295Reset(0);
	M6809Close();

	tlc34076_reset(6);
	tms34061_reset();
	pia_reset();

	timerReset();

	page_select = 0xc0;
	blit_in_progress = 0;

	grom_bank = 0;

	fetch_offset = 0;
	fetch_rle_count = 0;
	fetch_rle_literal = 0;
	fetch_rle_value = 0;

	memset (blitter_data, 0, 16);

	soundlatch = 0;
	sound_response = 0;

	m_blitter = 0;
	m_periodic = 0;
	m_tms34061 = 0;

	scrollx = 0;
	palcontrol = 0;
	bankdata = 0;

	nExtraCycles = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM			= Next;
	DrvM6809ROM[0]		= Next; Next += 0x080000;
	DrvM6809ROM[1]		= Next; Next += 0x010000;
	DrvZ80ROM			= Next; Next += 0x010000;

	DrvGfxROM			= Next; Next += 0x200000;

	MSM6295ROM			= Next; Next += 0x040000;

	pBurnDrvPalette		= (UINT32*)Next; Next += 0x10000 * sizeof(UINT32);

	grmatch_fb          = (UINT32*)Next; Next += 400*256*sizeof(UINT32);

	DrvM6809RAM[0]		= Next;
	DrvNVRAM			= Next; Next += ((is_ninclown) ? 0x4000 : 0x2000);

	AllRam				= Next;

	DrvM6809RAM[1]		= Next; Next += 0x000800;

	if (is_grmatch) {
		grmatch_pal     = (UINT32*)Next; Next += 0x20 * sizeof(UINT32);
	}

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static INT32 DrvGetRoms()
{
	char* pRomName;
	struct BurnRomInfo ri;
	UINT8 *pLoad[7] = { 0, DrvM6809ROM[0], DrvM6809ROM[1], DrvGfxROM, MSM6295ROM, Drv68KROM, DrvZ80ROM };

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++)
	{
		BurnDrvGetRomInfo(&ri, i);

		int type = ri.nType & 7;

		if (type == 5) // 68K
		{
			if (BurnLoadRom(pLoad[type] + 1, i + 0, 2)) return 1;
			if (BurnLoadRom(pLoad[type] + 0, i + 1, 2)) return 1;
			pLoad[type] += ri.nLen * 2;
			i++;
			continue;
		}

		if (type)
		{
			if ((type == 1 || type == 2) && ri.nLen <= 0x10000) pLoad[type] += 0x10000 - ri.nLen;
			if (BurnLoadRom(pLoad[type], i, 1)) return 1;
			pLoad[type] += ri.nLen;
			continue;
		}
	}

	graphics_length = pLoad[3] - DrvGfxROM;

	return 0;
}

static void MapM6809Lo()
{
	M6809Init(0);
	M6809Open(0);
	M6809MapMemory(DrvNVRAM,					0x2000, 0x3fff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM[0] + 0x0000, 	0x4000, 0x7fff, MAP_ROM);
	M6809MapMemory(DrvM6809ROM[0] + 0x8000, 	0x8000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(tmslo_main_write);
	M6809SetReadHandler(tmslo_main_read);
	M6809Close();

	pCPU = &M6809Config;
}

static void MapM6809Hi()
{
	M6809Init(0);
	M6809Open(0);
	M6809MapMemory(DrvNVRAM,					0x2000, 0x3fff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM[0] + 0x0000, 	0x4000, 0x7fff, MAP_ROM);
	M6809MapMemory(DrvM6809ROM[0] + 0x8000, 	0x8000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(tmshi_main_write);
	M6809SetReadHandler(tmshi_main_read);
	M6809Close();

	pCPU = &M6809Config;
}

static void MapHD6309()
{
	HD6309Init(0);
	HD6309Open(0);
	HD6309MapMemory(DrvNVRAM,					0x2000, 0x3fff, MAP_RAM);
	HD6309MapMemory(DrvM6809ROM[0] + 0x0000,	0x4000, 0x7fff, MAP_ROM);
	HD6309MapMemory(DrvM6809ROM[0] + 0x18000,	0x8000, 0xffff, MAP_ROM);
	HD6309SetWriteHandler(rimrockn_main_write);
	HD6309SetReadHandler(rimrockn_main_read);
	HD6309Close();

	pCPU = &HD6309Config;
	maincpu_clock = 12000000 / 4;
}

static void MapGtg2()
{
	M6809Init(0);
	M6809Open(0);
	M6809MapMemory(DrvNVRAM,					0x2000, 0x3fff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM[0] + 0x0000, 	0x4000, 0x7fff, MAP_ROM);
	M6809MapMemory(DrvM6809ROM[0] + 0x8000, 	0x8000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(gtg2_main_write);
	M6809SetReadHandler(gtg2_main_read);
	M6809Close();

	pCPU = &M6809Config;
}

static void MapM6809Grmatch()
{
	M6809Init(0);
	M6809Open(0);
	M6809MapMemory(DrvNVRAM,					0x2000, 0x3fff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM[0] + 0x0000, 	0x4000, 0x7fff, MAP_ROM);
	M6809MapMemory(DrvM6809ROM[0] + 0x8000, 	0x8000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(grmatch_main_write);
	M6809SetReadHandler(tmshi_main_read);
	M6809Close();

	pCPU = &M6809Config;
}

static void Map68K()
{
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,						0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(DrvNVRAM,						0x000000, 0x003fff, MAP_RAM);
	SekSetWriteWordHandler(0,					ninclown_main_write_word);
	SekSetWriteByteHandler(0,					ninclown_main_write_byte);
	SekSetReadWordHandler(0,					ninclown_main_read_word);
	SekSetReadByteHandler(0,					ninclown_main_read_byte);
	SekClose();

	pCPU = &SekConfig;
	maincpu_clock = 12000000;
}

static void ym2608SoundMap()
{
	M6809Init(1);
	M6809Open(1);
	M6809MapMemory(DrvM6809RAM[1],				0x6000, 0x67ff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM[1] + 0x8000,		0x8000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(itech8_ym2608_write);
	M6809SetReadHandler(itech8_ym2608_read);
	M6809Close();

	BurnLoadRom(MSM6295ROM + 0x020000, 0x80, 1);

	INT32 nSndROMLen = 0x20000;
	BurnYM2608Init(8000000, MSM6295ROM, &nSndROMLen, MSM6295ROM + 0x20000, &DrvFMIrqHandler, 0);
	BurnTimerAttach(&M6809Config, 2000000);
	AY8910SetPorts(0, NULL, NULL, NULL, ym2203_write_B);
	BurnYM2608SetRoute(BURN_SND_YM2608_YM2608_ROUTE_1, 0.25, BURN_SND_ROUTE_BOTH);
	BurnYM2608SetRoute(BURN_SND_YM2608_YM2608_ROUTE_2, 1.00, BURN_SND_ROUTE_BOTH);
	BurnYM2608SetRoute(BURN_SND_YM2608_AY8910_ROUTE,   1.00, BURN_SND_ROUTE_BOTH);

	sound_reset = BurnYM2608Reset;
	sound_update = BurnYM2608Update;
	sound_exit = BurnYM2608Exit;
	sound_scan = BurnYM2608Scan;

	// not on this set...
	MSM6295Init(0, 1000000 / 132, 1);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);
}

static void ym2203SoundMap()
{
	M6809Init(1);
	M6809Open(1);
	M6809MapMemory(DrvM6809RAM[1],				0x3000, 0x37ff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM[1] + 0x8000,		0x8000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(itech8_ym2203_write);
	M6809SetReadHandler(itech8_ym2203_read);
	M6809Close();

	BurnYM2203Init(1,  4000000, &DrvFMIrqHandler, 0);
	BurnYM2203SetPorts(0, NULL, NULL, NULL, &ym2203_write_B);
	BurnTimerAttach(&M6809Config, 2000000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE,   1.00, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.20, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.20, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.20, BURN_SND_ROUTE_BOTH);

	sound_reset = BurnYM2203Reset;
	sound_update = BurnYM2203Update;
	sound_exit = BurnYM2203Exit;
	sound_scan = BurnYM2203Scan;

	MSM6295Init(0, 1000000 / 132, 1);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);
}

static void ym3812SoundMap()
{
	M6809Init(1);
	M6809Open(1);
	M6809MapMemory(DrvM6809RAM[1],				0x3000, 0x37ff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM[1] + 0x8000,		0x8000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(itech8_ym3812_write);
	M6809SetReadHandler(itech8_ym3812_read);
	M6809Close();

	BurnYM3812Init(1, 4000000, &DrvFMIrqHandler, 0);
	BurnTimerAttach(&M6809Config, 2000000);
	BurnYM3812SetRoute(0, BURN_SND_YM3812_ROUTE, 1.00, BURN_SND_ROUTE_BOTH);

	sound_reset = BurnYM3812Reset;
	sound_update = BurnYM3812Update;
	sound_exit = BurnYM3812Exit;
	sound_scan = BurnYM3812Scan;

	MSM6295Init(0, 1000000 / 132, 1);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);
}

static INT32 CommonInit(void (*pMainMap)(), INT32 invert_bank, void (*pSoundMap)(), void (*pVidCallback)(INT32), INT32 adjust_x)
{
	BurnAllocMemIndex();

	{
		if (DrvGetRoms()) return 1;

		if (invert_bank) {
			memcpy (DrvM6809ROM[0] + 0x40000, DrvM6809ROM[0] + 0x00000, 0x04000);
			memcpy (DrvM6809ROM[0] + 0x00000, DrvM6809ROM[0] + 0x04000, 0x04000);
			memcpy (DrvM6809ROM[0] + 0x04000, DrvM6809ROM[0] + 0x40000, 0x04000);
		}
	}

	pMainMap();

	// pia is only on the ym3812 sets
	static pia6821_interface pia_0 = {
		0, 0, 0, 0, 0, 0,
		0, pia_portb_out, 0, 0,
		0, 0
	};

	pia_init();
	pia_config(0, 0, &pia_0);

	tms34061_init(8, 0x40000, partial_update, generate_tms34061_interrupt);

	timerInit();
	timerAdd(blitter_timer, 0, blitter_timer_cb);

	if (pCPU == &M6809Config) {
		M6809Open(0);
		M6809SetCallback(timerRun);
		M6809Close();
	}
	if (pCPU == &HD6309Config) {
		HD6309Open(0);
		HD6309SetCallback(timerRun);
		HD6309Close();
	}

	pSoundMap();

	BurnTrackballInit(2);
	tb_reset = 1; // everything but wfortune

	GenericTilesInit();
	video_callback = pVidCallback;
	screen_adjustx = adjust_x;

	// default for all games except rimrockn, ninclown, gtg2, gpgolf
	service_ptr = &DrvDips[0];
	service_bit = 7;

	// games prefer nvram to be initted with random data
	// note: some random data can actually cause grmatch to go berzerk - for
	// example, missed blits on the attract-mode game screen.  let's just use
	// a pattern that (hopefully) doesn't cause side-effects with the game.

	for (INT32 i = 0; i < ((is_ninclown) ? 0x4000 : 0x2000); i+=4) {
		DrvNVRAM[i+0] = 0xff;
		DrvNVRAM[i+1] = 0xfe;
		DrvNVRAM[i+2] = 0xf1;
		DrvNVRAM[i+3] = 0xf7;
	}

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	if (pCPU && pCPU != &M6809Config) pCPU->exit();

	MSM6295Exit();
	tms34061_exit();
	M6809Exit();
	pia_exit();
	timerExit();

	if (sound_exit) sound_exit();

	BurnTrackballExit();

	GenericTilesExit();

	BurnFreeMemIndex();

	screen_adjustx = 0;
	pCPU = NULL;
	maincpu_clock = 2000000;
	use_via = 0;
	is_grmatch = 0;
	is_ninclown = 0;
	tb_reset = 0;

	return 0;
}

static INT32 last_line = 0;

static void blank_scanline(INT32 line, UINT32 fill)
{
	for (INT32 y = last_line; y < line; y++) {
		UINT16 *dest = pTransDraw + y * nScreenWidth;

		for (INT32 i = 0; i < nScreenWidth; i++) {
			dest[i] = fill;
		}
	}

	BurnTransferPartial(pBurnDrvPalette, last_line, line);

	last_line = line;
}

static void screen_update_2layer(INT32 scanline)
{
	if (scanline >= nScreenHeight) scanline = nScreenHeight;
	if (last_line >= scanline) return;

	if (tms34061_display_blanked()) {
		blank_scanline(scanline, 0x100);
		return;
	}

	UINT32 page_offset = (tms34061_dipstart() << 6) & 0xffff;

//	bprintf(0, _T("fr# %d - vid update: %d - %d\n"), nCurrentFrame, last_line, scanline);
	for (INT32 y = last_line; y < scanline; y++)
	{
		UINT8 *base0 = &tms34061_get_vram_pointer()[(0x00000 + screen_adjustx + page_offset + y * 256) & VRAM_MASK];
		UINT8 *base2 = &tms34061_get_vram_pointer()[(0x20000 + screen_adjustx + page_offset + y * 256) & VRAM_MASK];
		UINT16 *dest = pTransDraw + y * nScreenWidth;

		for (INT32 x = 0; x < nScreenWidth; x++)
		{
			int pix0 = base0[x] & 0x0f;
			dest[x] = pix0 ? pix0 : base2[x];
		}
	}

	BurnTransferPartial(pBurnDrvPalette, last_line, scanline);

	last_line = scanline;
}

static void screen_update_2page(INT32 scanline)
{
	if (scanline >= nScreenHeight) scanline = nScreenHeight;
	if (last_line >= scanline) return;

	if (tms34061_display_blanked()) {
		blank_scanline(scanline, 0x100);
		return;
	}

	UINT32 page_offset = ((page_select & 0x80) << 10) | (((tms34061_dipstart() << 6) & 0x3ffff) & 0x0ffff);
	for (INT32 y = last_line; y < scanline; y++)
	{
		UINT8 *base = &tms34061_get_vram_pointer()[(screen_adjustx + page_offset + y * 256) & VRAM_MASK];
		UINT16 *dest = pTransDraw + y * nScreenWidth;

		for (INT32 x = 0; x < nScreenWidth; x++)
			dest[x] = base[x];
	}

	BurnTransferPartial(pBurnDrvPalette, last_line, scanline);

	last_line = scanline;
}

static void screen_update_2page_large(INT32 scanline)
{
	if (scanline >= nScreenHeight) scanline = nScreenHeight;
	if (last_line >= scanline) return;

	if (tms34061_display_blanked()) {
		blank_scanline(scanline, 0x100);
		return;
	}

	UINT32 page_offset = ((~page_select & 0x80) << 10) | (((tms34061_dipstart() << 6) & 0x3ffff) & 0xffff);

	for (INT32 y = last_line; y < scanline; y++)
	{
		UINT8 *base = &tms34061_get_vram_pointer()[((screen_adjustx/2) + page_offset + y * 256) & VRAM_MASK];
		UINT8 *latch = &tms34061_get_latchram_pointer()[((screen_adjustx/2) + page_offset + y * 256) & VRAM_MASK];
		UINT16 *dest = pTransDraw + y * nScreenWidth;

		for (INT32 x = 0; x < nScreenWidth; x += 2)
		{
			dest[x + 0] = (latch[x/2] & 0xf0) | (base[x/2] >> 4);
			dest[x + 1] = ((latch[x/2] << 4) & 0xf0) | (base[x/2] & 0x0f);
		}
	}

	BurnTransferPartial(pBurnDrvPalette, last_line, scanline);

	last_line = scanline;
}

static void grmatch_palette_xfer(INT32 scanline)
{
	if (palcontrol & 0x80)
	{
		UINT32 page_offset = ((tms34061_dipstart() << 6) & 0xffff) | scrollx;
		UINT8 *vram = tms34061_get_vram_pointer();

		for (INT32 page = 0; page < 2; page++)
		{
			UINT8 *base = &vram[(page * 0x20000 + page_offset) & VRAM_MASK];

			for (INT32 x = 0; x < 16; x++)
			{
				UINT8 data0 = base[x * 2 + 0];
				UINT8 data1 = base[x * 2 + 1];

				grmatch_pal[(page * 16) + x] = (pal4bit(data0 >> 0) << 16) | (pal4bit(data1 >> 4) << 8) | (pal4bit(data1 >> 0) << 0);
				pBurnDrvPalette[(page * 16) + x] = BurnHighCol(pal4bit(data0 >> 0), pal4bit(data1 >> 4), pal4bit(data1 >> 0), 0);
			}
		}
	}
}

static void grmatch_draw(INT32 scanline)
{
	if (scanline >= nScreenHeight) scanline = nScreenHeight;
	if (last_line >= scanline) return;

	if (tms34061_display_blanked()) {
		blank_scanline(scanline, 0xffff);
		return;
	}

	UINT32 page_offset = ((tms34061_dipstart() << 6) & 0xffff) | scrollx;
	UINT8 *vram = tms34061_get_vram_pointer();

	for (INT32 y = last_line; y < scanline; y++)
	{
		UINT8 *base0 = &vram[0x00000 + (screen_adjustx/2) + ((page_offset + y * 256) & 0xffff)];
		UINT8 *base2 = &vram[0x20000 + (screen_adjustx/2) + ((page_offset + y * 256) & 0xffff)];
		UINT16 *dest = pTransDraw + (y * nScreenWidth);

		for (INT32 x = 0; x < nScreenWidth; x += 2)
		{
			UINT8 pix0 = base0[x / 2];
			UINT8 pix2 = base2[x / 2];

			if ((pix0 & 0xf0) != 0)
				dest[x] = 0x00 | (pix0 >> 4);
			else
				dest[x] = 0x10 | (pix2 >> 4);

			if ((pix0 & 0x0f) != 0)
				dest[x + 1] = 0x00 | (pix0 & 0x0f);
			else
				dest[x + 1] = 0x10 | (pix2 & 0x0f);
		}
	}

	BurnTransferPartial(pBurnDrvPalette, last_line, scanline);

	last_line = scanline;
}

static INT32 DrvDraw()
{
	if (BurnRecalc) {
		if (is_grmatch) {
			for (INT32 pal = 0; pal < 2; pal++) {
				for (INT32 i = 0; i < 16; i++) {
					UINT32 p = grmatch_pal[(pal * 16) + i];
					pBurnDrvPalette[(pal * 16) + i] = BurnHighCol((p >> 16) & 0xff, (p >> 8) & 0xff, p & 0xff, 0);
				}
			}
			pBurnDrvPalette[0xffff] = 0; // black
		} else {
			tlc34076_recalc_palette();
			pBurnDrvPalette[0x100] = 0; // black
		}
		BurnRecalc = 0;
	}

	// screen updated with BurnTransferPartial() (above!)

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		service_ptr[0] = (service_ptr[0] & ~(1<<service_bit)) | (!Diag[0] << service_bit);

		DrvInputs[0] = (DrvDips[0] << 8) | (DrvDips[0]);

		for (INT32 i = 0; i < 16; i++) {
			for (INT32 j = 0; j < 8; j++) {
				DrvInputs[j] ^= (DrvJoy[j][i] & 1) << i;
			}
		}

		BurnTrackballConfig(0, AXIS_NORMAL, AXIS_REVERSED);
		BurnTrackballFrame(0, Analog[0], Analog[1], 0x01, 0x20, 263);
		BurnTrackballUpdate(0);

		BurnTrackballConfig(1, AXIS_NORMAL, AXIS_REVERSED);
		BurnTrackballFrame(1, Analog[2], Analog[3], 0x01, 0x20, 263);
		BurnTrackballUpdate(1);
	}

	pCPU->newframe();
	M6809NewFrame();
	timerNewFrame();

	INT32 nInterleave = 263;
	INT32 nCyclesTotal[3] = { (INT32)(maincpu_clock / 60), (INT32)(2000000 / 60), (INT32)(maincpu_clock / 60) }; // main, sound, dtimer
	INT32 nCyclesDone[3] = { nExtraCycles, 0, 0 };

	last_line = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		tms34061_current_scanline = i;

		pCPU->open(0);

		if (is_grmatch) {
			grmatch_palette_xfer(i);
		}

		nCyclesDone[0] += pCPU->run(((i + 1) * nCyclesTotal[0] / nInterleave) - nCyclesDone[0]);

		tms34061_interrupt();

		if (i == 239) {
			video_callback(nScreenHeight);
			update_interrupts(1, -1, -1);
			nCyclesDone[0] += pCPU->run(usec_to_cycles(maincpu_clock, 1));
			update_interrupts(0, -1, -1);
		}

		pCPU->close();

		M6809Open(1);
		CPU_RUN_TIMER(1);
		if (use_via) {
			// iq_132 -- hack!!! until we have via emulation!
			if ((i % (nInterleave/4))==((nInterleave/4)-1)) M6809SetIRQLine(1, CPU_IRQSTATUS_ACK);
		}
		M6809Close();

		CPU_RUN_SYNCINT(2, timer);
	}

	nExtraCycles = nCyclesDone[0] - nCyclesTotal[0];

	if (pBurnSoundOut) {
		sound_update(pBurnSoundOut, nBurnSoundLen);
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
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
		*pnMin = 0x029704;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA)
	{
		if (pCPU != &M6809Config) pCPU->scan(nAction);
		M6809Scan(nAction);

		if (is_grmatch == 0) tlc34076_Scan(nAction);
		tms34061_scan(nAction, pnMin);
		pia_scan(nAction, pnMin);
		timerScan();

		MSM6295Scan(nAction, pnMin);
		sound_scan(nAction, pnMin);

		BurnTrackballScan();

		SCAN_VAR(page_select);
		SCAN_VAR(blit_in_progress);

		SCAN_VAR(grom_bank);

		SCAN_VAR(fetch_offset);
		SCAN_VAR(fetch_rle_count);
		SCAN_VAR(fetch_rle_literal);
		SCAN_VAR(fetch_rle_value);

		SCAN_VAR(blitter_data);

		SCAN_VAR(soundlatch);
		SCAN_VAR(sound_response);

		SCAN_VAR(m_blitter);
		SCAN_VAR(m_periodic);
		SCAN_VAR(m_tms34061);

		SCAN_VAR(scrollx);
		SCAN_VAR(palcontrol);
		SCAN_VAR(bankdata);

		SCAN_VAR(nExtraCycles);
	}

	if (nAction & ACB_NVRAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data		= DrvNVRAM;
		ba.nLen		= (is_ninclown) ? 0x4000 : 0x2000;
		ba.szName	= "NV RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_WRITE)
	{
		pCPU->open(0);
		if (pCPU == &M6809Config) bankswitch(bankdata);
		if (pCPU == &HD6309Config) rimrockn_bank(bankdata);
		pCPU->close();
	}

	return 0;
}


// Wheel Of Fortune (set 1)

static struct BurnRomInfo wfortuneRomDesc[] = {
	{ "wofpgm.u5",							0x10000, 0xbd984654, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code

	{ "wof_snd-wof.u27",					0x08000, 0x0a6aa5dc, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 #1 Code

	{ "grom0-wof.grom0",					0x10000, 0x9a157b2c, 3 | BRF_GRA },           //  2 Blitter Graphics
	{ "grom1-wof.grom1",					0x10000, 0x5064739b, 3 | BRF_GRA },           //  3
	{ "grom2-wof.grom2",					0x10000, 0x3d393b2b, 3 | BRF_GRA },           //  4
	{ "grom3-wof.grom3",					0x10000, 0x117a2ce9, 3 | BRF_GRA },           //  5

	{ "wof_vr-sbom0.srom0",					0x20000, 0x5c28c3fe, 4 | BRF_SND },           //  6 Samples
};

STD_ROM_PICK(wfortune)
STD_ROM_FN(wfortune)

static INT32 WfortuneInit()
{
	INT32 rc = CommonInit(MapM6809Hi, 0, ym2203SoundMap, screen_update_2layer, 0);

	if (!rc) {
		tb_reset = 0;
	}

	return rc;
}

struct BurnDriver BurnDrvWfortune = {
	"wfortune", NULL, NULL, NULL, "1989",
	"Wheel Of Fortune (set 1)\0", NULL, "GameTek", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 3, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, wfortuneRomInfo, wfortuneRomName, NULL, NULL, NULL, NULL, WfortuneInputInfo, WfortuneDIPInfo,
	WfortuneInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	256, 240, 4, 3
};


// Wheel Of Fortune (set 2)

static struct BurnRomInfo wfortuneaRomDesc[] = {
	{ "wof-pgm_r1.u5",						0x10000, 0xc3d3eb21, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code

	{ "wof_snd-wof.u27",					0x08000, 0x0a6aa5dc, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 #1 Code

	{ "grom0-wof.grom0",					0x10000, 0x9a157b2c, 3 | BRF_GRA },           //  2 Blitter Graphics
	{ "grom1-wof.grom1",					0x10000, 0x5064739b, 3 | BRF_GRA },           //  3
	{ "grom2-wof.grom2",					0x10000, 0x3d393b2b, 3 | BRF_GRA },           //  4
	{ "grom3-wof.grom3",					0x10000, 0x117a2ce9, 3 | BRF_GRA },           //  5

	{ "wof_vr-sbom0.srom0",					0x20000, 0x5c28c3fe, 4 | BRF_SND },           //  6 Samples
};

STD_ROM_PICK(wfortunea)
STD_ROM_FN(wfortunea)

struct BurnDriver BurnDrvWfortunea = {
	"wfortunea", "wfortune", NULL, NULL, "1989",
	"Wheel Of Fortune (set 2)\0", NULL, "GameTek", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 3, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, wfortuneaRomInfo, wfortuneaRomName, NULL, NULL, NULL, NULL, WfortuneInputInfo, WfortuneDIPInfo,
	WfortuneInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	256, 240, 4, 3
};


// Grudge Match (Yankee Game Technology)

static struct BurnRomInfo emptyRomDesc[] = { { "", 0, 0, 0 }, };

static struct BurnRomInfo Ym2608RomDesc[] = {
#if !defined (ROM_VERIFY)
	{ "ym2608_adpcm_rom.bin",  0x002000, 0x23c9e0d8, BRF_ESS | BRF_PRG | BRF_BIOS },
#else
	{ "",  0x000000, 0x00000000, BRF_ESS | BRF_PRG | BRF_BIOS },
#endif
};

static struct BurnRomInfo grmatchRomDesc[] = {
	{ "grudgematch.u5",						0x10000, 0x11cadec9, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code

	{ "grudgematch.u27",					0x08000, 0x59c18e63, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 #1 Code

	{ "grom0.bin",							0x20000, 0x9064eff9, 3 | BRF_GRA },           //  2 Blitter Graphics
	{ "grom1.bin",							0x20000, 0x6919c314, 3 | BRF_GRA },           //  3
	{ "grom2.bin",							0x20000, 0x36b1682c, 3 | BRF_GRA },           //  4
	{ "grom3.bin",							0x20000, 0x7bf05f61, 3 | BRF_GRA },           //  5
	{ "grom4.bin",							0x20000, 0x55bded89, 3 | BRF_GRA },           //  6
	{ "grom5.bin",							0x20000, 0x37b47b2e, 3 | BRF_GRA },           //  7
	{ "grom6.bin",							0x20000, 0x860ee822, 3 | BRF_GRA },           //  8

	{ "srom0.bin",							0x20000, 0x49bce954, 4 | BRF_SND },           //  9 YM2608b Samples
};

STDROMPICKEXT(grmatch, grmatch, Ym2608)
STD_ROM_FN(grmatch)

static INT32 GrmatchInit()
{
	is_grmatch = 1;
	return CommonInit(MapM6809Grmatch, 0, ym2608SoundMap, grmatch_draw, 0);
}

struct BurnDriver BurnDrvGrmatch = {
	"grmatch", NULL, "ym2608", NULL, "1989",
	"Grudge Match (Yankee Game Technology)\0", NULL, "Yankee Game Technology", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, grmatchRomInfo, grmatchRomName, NULL, NULL, NULL, NULL, GrmatchInputInfo, GrmatchDIPInfo,
	GrmatchInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	400, 240, 4, 3
};


// Strata Bowling (V3)

static struct BurnRomInfo stratabRomDesc[] = {
	{ "sb_prog_v3_u5.u5",					0x08000, 0xa5ae728f, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code

	{ "sb_snds_u27.u27",					0x08000, 0xb36c8f0a, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 #1 Code

	{ "sb_grom0_0.grom0",					0x20000, 0xa915b0bd, 3 | BRF_GRA },           //  2 Blitter Graphics
	{ "sb_grom0_1.grom1",					0x20000, 0x340c661f, 3 | BRF_GRA },           //  3
	{ "sb_grom0_2.grom2",					0x20000, 0x5df9f1cf, 3 | BRF_GRA },           //  4

	{ "sb_srom0.srom0",						0x20000, 0x6ff390b9, 4 | BRF_SND },           //  5 Samples
};

STD_ROM_PICK(stratab)
STD_ROM_FN(stratab)

static INT32 StratabInit()
{
	return CommonInit(MapM6809Hi, 0, ym2203SoundMap, screen_update_2layer, 0);
}

struct BurnDriver BurnDrvStratab = {
	"stratab", NULL, NULL, NULL, "1990",
	"Strata Bowling (V3)\0", NULL, "Strata / Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, stratabRomInfo, stratabRomName, NULL, NULL, NULL, NULL, StratabInputInfo, StratabDIPInfo,
	StratabInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	240, 256, 3, 4
};


// Strata Bowling (V1)

static struct BurnRomInfo stratab1RomDesc[] = {
	{ "sb_prog_v1_u5.u5",					0x08000, 0x46d51604, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code

	{ "sb_snds_u27.u27",					0x08000, 0xb36c8f0a, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 #1 Code

	{ "sb_grom0_0.grom0",					0x20000, 0xa915b0bd, 3 | BRF_GRA },           //  2 Blitter Graphics
	{ "sb_grom0_1.grom1",					0x20000, 0x340c661f, 3 | BRF_GRA },           //  3
	{ "sb_grom0_2.grom2",					0x20000, 0x5df9f1cf, 3 | BRF_GRA },           //  4

	{ "sb_srom0.srom0",						0x20000, 0x6ff390b9, 4 | BRF_SND },           //  5 Samples
};

STD_ROM_PICK(stratab1)
STD_ROM_FN(stratab1)

struct BurnDriver BurnDrvStratab1 = {
	"stratab1", "stratab", NULL, NULL, "1990",
	"Strata Bowling (V1)\0", NULL, "Strata / Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, stratab1RomInfo, stratab1RomName, NULL, NULL, NULL, NULL, StratabInputInfo, StratabDIPInfo,
	StratabInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	240, 256, 3, 4
};


// Golden Tee Golf (Joystick, v3.3)

static struct BurnRomInfo gtgRomDesc[] = {
	{ "gtg_joy_v3.3_u5.u5",					0x10000, 0x983a5c0c, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code

	{ "golf_snd-v1.1_u27.u27",				0x08000, 0x358d2440, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 #1 Code

	{ "golf_grom00.grom0",					0x20000, 0xa29c688a, 3 | BRF_GRA },           //  2 Blitter Graphics
	{ "golf_grom01.grom1",					0x20000, 0xb52a23f6, 3 | BRF_GRA },           //  3
	{ "golf_grom02.grom2",					0x20000, 0x9b8e3a61, 3 | BRF_GRA },           //  4
	{ "golf_grom03.grom3",					0x20000, 0xb6e9fb15, 3 | BRF_GRA },           //  5
	{ "golf_grom04.grom4",					0x20000, 0xfaa16729, 3 | BRF_GRA },           //  6
	{ "golf_grom05.grom5",					0x20000, 0xc108c56c, 3 | BRF_GRA },           //  7

	{ "golf_srom0.srom0",					0x20000, 0x1cccbfdf, 4 | BRF_SND },           //  8 Samples
};

STD_ROM_PICK(gtg)
STD_ROM_FN(gtg)

struct BurnDriver BurnDrvGtg = {
	"gtg", NULL, NULL, NULL, "1990",
	"Golden Tee Golf (Joystick, v3.3)\0", NULL, "Strata / Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, gtgRomInfo, gtgRomName, NULL, NULL, NULL, NULL, GtgInputInfo, GtgDIPInfo,
	StratabInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	256, 240, 4, 3
};


// Golden Tee Golf (Joystick, v3.1)

static struct BurnRomInfo gtgj31RomDesc[] = {
	{ "gtg_joy_v3.1_u5.u5",					0x10000, 0x61984272, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code

	{ "golf_snd-v1.1_u27.u27",				0x08000, 0x358d2440, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 #1 Code

	{ "golf_grom00.grom0",					0x20000, 0xa29c688a, 3 | BRF_GRA },           //  2 Blitter Graphics
	{ "golf_grom01.grom1",					0x20000, 0xb52a23f6, 3 | BRF_GRA },           //  3
	{ "golf_grom02.grom2",					0x20000, 0x9b8e3a61, 3 | BRF_GRA },           //  4
	{ "golf_grom03.grom3",					0x20000, 0xb6e9fb15, 3 | BRF_GRA },           //  5
	{ "golf_grom04.grom4",					0x20000, 0xfaa16729, 3 | BRF_GRA },           //  6
	{ "golf_grom05.grom5",					0x20000, 0x5b393314, 3 | BRF_GRA },           //  7

	{ "golf_srom0.srom0",					0x20000, 0x1cccbfdf, 4 | BRF_SND },           //  8 Samples
};

STD_ROM_PICK(gtgj31)
STD_ROM_FN(gtgj31)

struct BurnDriver BurnDrvGtgj31 = {
	"gtgj31", "gtg", NULL, NULL, "1990",
	"Golden Tee Golf (Joystick, v3.1)\0", NULL, "Strata / Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, gtgj31RomInfo, gtgj31RomName, NULL, NULL, NULL, NULL, GtgInputInfo, GtgDIPInfo,
	StratabInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	256, 240, 4, 3
};


// Golden Tee Golf (Trackball, v2.1)

static struct BurnRomInfo gtgt21RomDesc[] = {
	{ "gtg_bim_2.1.u5",						0x10000, 0x25f626d9, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code

	{ "golf_snd_u27.u27",					0x08000, 0xac6d3f32, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 #1 Code

	{ "golf_grom00.grom0",					0x20000, 0xa29c688a, 3 | BRF_GRA },           //  2 Blitter Graphics
	{ "golf_grom01.grom1",					0x20000, 0xb52a23f6, 3 | BRF_GRA },           //  3
	{ "golf_grom02.grom2",					0x20000, 0x9b8e3a61, 3 | BRF_GRA },           //  4
	{ "golf_grom03.grom3",					0x20000, 0xb6e9fb15, 3 | BRF_GRA },           //  5
	{ "golf_grom04.grom4",					0x20000, 0xfaa16729, 3 | BRF_GRA },           //  6
	{ "golf_grom05.grom5",					0x10000, 0xdab92dfb, 3 | BRF_GRA },           //  7

	{ "golf_srom0.srom0",					0x20000, 0xd041e0c9, 4 | BRF_SND },           //  8 Samples
};

STD_ROM_PICK(gtgt21)
STD_ROM_FN(gtgt21)

struct BurnDriver BurnDrvGtgt21 = {
	"gtgt21", "gtg", NULL, NULL, "1989",
	"Golden Tee Golf (Trackball, v2.1)\0", NULL, "Strata / Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, gtgt21RomInfo, gtgt21RomName, NULL, NULL, NULL, NULL, GtgtInputInfo, GtgtDIPInfo,
	StratabInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	256, 240, 4, 3
};


// Golden Tee Golf (Trackball, v2.0)

static struct BurnRomInfo gtgt20RomDesc[] = {
	{ "gtg_bim_2.0.u5",						0x10000, 0x4c907166, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code

	{ "golf_snd_u27.u27",					0x08000, 0xf6a7429b, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 #1 Code

	{ "golf_grom00.grom0",					0x20000, 0xa29c688a, 3 | BRF_GRA },           //  2 Blitter Graphics
	{ "golf_grom01.grom1",					0x20000, 0xb52a23f6, 3 | BRF_GRA },           //  3
	{ "golf_grom02.grom2",					0x20000, 0x9b8e3a61, 3 | BRF_GRA },           //  4
	{ "golf_grom03.grom3",					0x20000, 0xb6e9fb15, 3 | BRF_GRA },           //  5
	{ "golf_grom04.grom4",					0x20000, 0xfaa16729, 3 | BRF_GRA },           //  6
	{ "golf_grom05.grom5",					0x10000, 0x62a523d2, 3 | BRF_GRA },           //  7

	{ "golf_srom0.srom0",					0x20000, 0xd041e0c9, 4 | BRF_SND },           //  8 Samples
};

STD_ROM_PICK(gtgt20)
STD_ROM_FN(gtgt20)

struct BurnDriver BurnDrvGtgt20 = {
	"gtgt20", "gtg", NULL, NULL, "1989",
	"Golden Tee Golf (Trackball, v2.0)\0", NULL, "Strata / Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, gtgt20RomInfo, gtgt20RomName, NULL, NULL, NULL, NULL, GtgtInputInfo, GtgtDIPInfo,
	StratabInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	256, 240, 4, 3
};


// Golden Tee Golf (Trackball, v1.0)

static struct BurnRomInfo gtgt10RomDesc[] = {
	{ "gtg_bim_1.0.u5",						0x10000, 0xec70b510, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code

	{ "golf_snd_u27.u27",					0x08000, 0x471da557, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 #1 Code

	{ "golf_grom00.grom0",					0x20000, 0xa29c688a, 3 | BRF_GRA },           //  2 Blitter Graphics
	{ "golf_grom01.grom1",					0x20000, 0xb52a23f6, 3 | BRF_GRA },           //  3
	{ "golf_grom02.grom2",					0x20000, 0x9b8e3a61, 3 | BRF_GRA },           //  4
	{ "golf_grom03.grom3",					0x20000, 0xb6e9fb15, 3 | BRF_GRA },           //  5
	{ "golf_grom04.grom4",					0x20000, 0xfaa16729, 3 | BRF_GRA },           //  6
	{ "golf_grom05.grom5",					0x10000, 0x44b47015, 3 | BRF_GRA },           //  7

	{ "golf_srom0.srom0",					0x20000, 0xd041e0c9, 4 | BRF_SND },           //  8 Samples
};

STD_ROM_PICK(gtgt10)
STD_ROM_FN(gtgt10)

struct BurnDriver BurnDrvGtgt10 = {
	"gtgt10", "gtg", NULL, NULL, "1989",
	"Golden Tee Golf (Trackball, v1.0)\0", NULL, "Strata / Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, gtgt10RomInfo, gtgt10RomName, NULL, NULL, NULL, NULL, GtgtInputInfo, GtgtDIPInfo,
	StratabInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	256, 240, 4, 3
};


// Golden Tee Golf II (Trackball, V1.1)

static struct BurnRomInfo gtg2tRomDesc[] = {
	{ "gtgii_tb_v1.1.u5",					0x10000, 0xc7b3a9f3, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code

	{ "gtgii_snd_v1_u27.u27",				0x08000, 0xdd2a5905, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 #1 Code

	{ "gtgii-grom0.grom0",					0x20000, 0xa29c688a, 3 | BRF_GRA },           //  2 Blitter Graphics
	{ "gtgii-grom1.grom1",					0x20000, 0xa4182776, 3 | BRF_GRA },           //  3
	{ "gtgii-grom2.grom2",					0x20000, 0x0580bb99, 3 | BRF_GRA },           //  4
	{ "gtgii-grom3.grom3",					0x20000, 0x89edb624, 3 | BRF_GRA },           //  5
	{ "gtgii-grom4.grom4",					0x20000, 0xf6557950, 3 | BRF_GRA },           //  6
	{ "gtgii-grom5.grom5",					0x20000, 0xa680ce6a, 3 | BRF_GRA },           //  7

	{ "gtgii_vr_srom0.srom0",				0x20000, 0x4dd4db42, 4 | BRF_SND },           //  8 Samples

	{ "tibpal16l8.u11",						0x00104, 0x9bf5a75f, 0 | BRF_OPT },           //  9 PLDs
};

STD_ROM_PICK(gtg2t)
STD_ROM_FN(gtg2t)

static INT32 Gtg2tInit()
{
	return CommonInit(MapM6809Hi, 1, ym2203SoundMap, screen_update_2layer, 0);
}

struct BurnDriver BurnDrvGtg2t = {
	"gtg2t", "gtg2", NULL, NULL, "1989",
	"Golden Tee Golf II (Trackball, V1.1)\0", NULL, "Strata / Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, gtg2tRomInfo, gtg2tRomName, NULL, NULL, NULL, NULL, Gtg2tInputInfo, Gtg2tDIPInfo,
	Gtg2tInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	256, 240, 4, 3
};


// Golden Tee Golf II (Joystick, V1.0)

static struct BurnRomInfo gtg2jRomDesc[] = {
	{ "gtg2.bim_1.0.u5",					0x10000, 0x9c95ceaa, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code

	{ "gtgii_snd_v1_u27.u27",				0x08000, 0xdd2a5905, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 #1 Code

	{ "gtgii-grom0.grom0",					0x20000, 0xa29c688a, 3 | BRF_GRA },           //  2 Blitter Graphics
	{ "gtgii-grom1.grom1",					0x20000, 0xa4182776, 3 | BRF_GRA },           //  3
	{ "gtgii-grom2.grom2",					0x20000, 0x0580bb99, 3 | BRF_GRA },           //  4
	{ "gtgii-grom3.grom3",					0x20000, 0x89edb624, 3 | BRF_GRA },           //  5
	{ "gtgii-grom4.grom4",					0x20000, 0xf6557950, 3 | BRF_GRA },           //  6
	{ "gtgii-grom5.grom5",					0x20000, 0xa680ce6a, 3 | BRF_GRA },           //  7

	{ "srom0.bin",							0x20000, 0x1cccbfdf, 4 | BRF_SND },           //  8 Samples

	{ "tibpal16l8.u11",						0x00104, 0x9bf5a75f, 0 | BRF_OPT },           //  9 PLDs
};

STD_ROM_PICK(gtg2j)
STD_ROM_FN(gtg2j)

static INT32 Gtg2jInit()
{
	return CommonInit(MapM6809Lo, 0, ym2203SoundMap, screen_update_2layer, 0);
}

struct BurnDriver BurnDrvGtg2j = {
	"gtg2j", "gtg2", NULL, NULL, "1991",
	"Golden Tee Golf II (Joystick, V1.0)\0", NULL, "Strata / Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, gtg2jRomInfo, gtg2jRomName, NULL, NULL, NULL, NULL, GtgInputInfo, GtgDIPInfo,
	Gtg2jInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	256, 240, 4, 3
};


// Poker Dice (V1.7)

static struct BurnRomInfo pokrdiceRomDesc[] = {
	{ "pd-v1.7_u5.u5",						0x10000, 0x5e24be82, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code

	{ "pd-snd.u27",							0x08000, 0x4925401c, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 #1 Code

	{ "pd-grom0.grom0",						0x20000, 0x7c2573e7, 3 | BRF_GRA },           //  2 Blitter Graphics
	{ "pd-grom1.grom1",						0x20000, 0xe7c06aeb, 3 | BRF_GRA },           //  3

	{ "pd-srom0.srom0",						0x10000, 0xf85dbd6f, 4 | BRF_SND },           //  4 Samples
};

STD_ROM_PICK(pokrdice)
STD_ROM_FN(pokrdice)

static INT32 PokrdiceInit()
{
	return CommonInit(MapM6809Lo, 0, ym2203SoundMap, screen_update_2page, 0);
}

struct BurnDriver BurnDrvPokrdice = {
	"pokrdice", NULL, NULL, NULL, "1991",
	"Poker Dice (V1.7)\0", NULL, "Strata / Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 1, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, pokrdiceRomInfo, pokrdiceRomName, NULL, NULL, NULL, NULL, PokrdiceInputInfo, PokrdiceDIPInfo,
	PokrdiceInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	240, 256, 3, 4
};


// Hot Shots Tennis (V1.1)

static struct BurnRomInfo hstennisRomDesc[] = {
	{ "ten.bim_v1.1_u5.u5",					0x10000, 0xfaffab5c, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code

	{ "tensnd.bim_v1_u27.u27",				0x08000, 0xf034a694, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 #1 Code

	{ "t_grom0.bim.grom0",					0x20000, 0x1e69ebae, 3 | BRF_GRA },           //  2 Blitter Graphics
	{ "t_grom1.bim.grom1",					0x20000, 0x4e6a22d5, 3 | BRF_GRA },           //  3
	{ "t_grom2.bim.grom2",					0x20000, 0xc0b643a9, 3 | BRF_GRA },           //  4
	{ "t_grom3.bim.grom3",					0x20000, 0x54afb456, 3 | BRF_GRA },           //  5
	{ "t_grom4.bim.grom4",					0x20000, 0xee09d645, 3 | BRF_GRA },           //  6

	{ "tennis.vr_srom0.srom0",				0x20000, 0xd9ce58c3, 4 | BRF_SND },           //  7 Samples

	{ "pal16l8-itvs.u11",					0x00104, 0xfee03727, 0 | BRF_OPT },           //  8 PLDs
};

STD_ROM_PICK(hstennis)
STD_ROM_FN(hstennis)

static INT32 HstennisInit()
{
	return CommonInit(MapM6809Hi, 0, ym3812SoundMap, screen_update_2page_large, 0);
}

struct BurnDriver BurnDrvHstennis = {
	"hstennis", NULL, NULL, NULL, "1990",
	"Hot Shots Tennis (V1.1)\0", NULL, "Strata / Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, hstennisRomInfo, hstennisRomName, NULL, NULL, NULL, NULL, HstennisInputInfo, HstennisDIPInfo,
	HstennisInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	240, 376, 3, 4
};


// Hot Shots Tennis (V1.0)

static struct BurnRomInfo hstennis10RomDesc[] = {
	{ "ten.bim_v1.0_u5.u5",					0x10000, 0xd108a6e0, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code

	{ "tensnd.bim_v1_u27.u27",				0x08000, 0xf034a694, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 #1 Code

	{ "t_grom0.bim.grom0",					0x20000, 0x1e69ebae, 3 | BRF_GRA },           //  2 Blitter Graphics
	{ "t_grom1.bim.grom1",					0x20000, 0x4e6a22d5, 3 | BRF_GRA },           //  3
	{ "t_grom2.bim.grom2",					0x20000, 0xc0b643a9, 3 | BRF_GRA },           //  4
	{ "t_grom3.bim.grom3",					0x20000, 0x54afb456, 3 | BRF_GRA },           //  5
	{ "t_grom4.bim.grom4",					0x20000, 0xee09d645, 3 | BRF_GRA },           //  6

	{ "tennis.vr_srom0.srom0",				0x20000, 0xd9ce58c3, 4 | BRF_SND },           //  7 Samples

	{ "pal16l8-itvs.u11",					0x00104, 0xfee03727, 0 | BRF_OPT },           //  8 PLDs
};

STD_ROM_PICK(hstennis10)
STD_ROM_FN(hstennis10)

struct BurnDriver BurnDrvHstennis10 = {
	"hstennis10", "hstennis", NULL, NULL, "1990",
	"Hot Shots Tennis (V1.0)\0", NULL, "Strata / Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, hstennis10RomInfo, hstennis10RomName, NULL, NULL, NULL, NULL, HstennisInputInfo, HstennisDIPInfo,
	HstennisInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	240, 400, 3, 4
};


// Arlington Horse Racing (v1.40-D)

static struct BurnRomInfo arlingtnRomDesc[] = {
	{ "ahr-d_v_1.40_u5.u5",					0x10000, 0x02338ddd, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code

	{ "ahr_snd_v1.1_u27.u27",				0x08000, 0xdec57dca, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 #1 Code

	{ "ahr_grom0.grom0",					0x20000, 0x5ef57fe5, 3 | BRF_GRA },           //  2 Blitter Graphics
	{ "ahr_grom1.grom1",					0x20000, 0x6aca95c0, 3 | BRF_GRA },           //  3
	{ "ahr_grom2.grom2",					0x10000, 0x6d6fde1b, 3 | BRF_GRA },           //  4

	{ "ahr_srom0.srom0",					0x40000, 0x56087f81, 4 | BRF_SND },           //  5 Samples
};

STD_ROM_PICK(arlingtn)
STD_ROM_FN(arlingtn)

static INT32 ArlingtnInit()
{
	return CommonInit(MapM6809Hi, 1, ym3812SoundMap, screen_update_2page_large, 0);
}

struct BurnDriver BurnDrvArlingtn = {
	"arlingtn", NULL, NULL, NULL, "1991",
	"Arlington Horse Racing (v1.40-D)\0", NULL, "Strata / Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, arlingtnRomInfo, arlingtnRomName, NULL, NULL, NULL, NULL, ArlingtnInputInfo, ArlingtnDIPInfo,
	ArlingtnInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	400, 240, 4, 3
};


// Arlington Horse Racing (v1.21-D)

static struct BurnRomInfo arlingtnaRomDesc[] = {
	{ "ahr-d_v_1.21_u5.u5",					0x10000, 0x00aae02e, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code

	{ "ahr_snd_v1.1_u27.u27",				0x08000, 0xdec57dca, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 #1 Code

	{ "ahr_grom0.grom0",					0x20000, 0x5ef57fe5, 3 | BRF_GRA },           //  2 Blitter Graphics
	{ "ahr_grom1.grom1",					0x20000, 0x6aca95c0, 3 | BRF_GRA },           //  3
	{ "ahr_grom2.grom2",					0x10000, 0x6d6fde1b, 3 | BRF_GRA },           //  4

	{ "ahr_srom0.srom0",					0x40000, 0x56087f81, 4 | BRF_SND },           //  5 Samples
};

STD_ROM_PICK(arlingtna)
STD_ROM_FN(arlingtna)

struct BurnDriver BurnDrvArlingtna = {
	"arlingtna", "arlingtn", NULL, NULL, "1991",
	"Arlington Horse Racing (v1.21-D)\0", NULL, "Strata / Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, arlingtnaRomInfo, arlingtnaRomName, NULL, NULL, NULL, NULL, ArlingtnInputInfo, ArlingtnDIPInfo,
	ArlingtnInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	400, 240, 4, 3
};


// Peggle (Joystick, v1.0)

static struct BurnRomInfo peggleRomDesc[] = {
	{ "j-stick.u5",							0x10000, 0x140d5a9c, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code

	{ "sound.u27",							0x08000, 0xb99beb70, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 #1 Code

	{ "grom0.bin",							0x20000, 0x5c02348d, 3 | BRF_GRA },           //  2 Blitter Graphics
	{ "grom1.bin",							0x20000, 0x85a7a3a2, 3 | BRF_GRA },           //  3
	{ "grom2.bin",							0x20000, 0xbfe11f18, 3 | BRF_GRA },           //  4

	{ "srom0",								0x20000, 0x001846ea, 4 | BRF_SND },           //  5 Samples
};

STD_ROM_PICK(peggle)
STD_ROM_FN(peggle)

static INT32 PeggleInit()
{
	return CommonInit(MapM6809Lo, 0, ym3812SoundMap, screen_update_2page_large, 18);
}

struct BurnDriver BurnDrvPeggle = {
	"peggle", NULL, NULL, NULL, "1991",
	"Peggle (Joystick, v1.0)\0", NULL, "Strata / Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, peggleRomInfo, peggleRomName, NULL, NULL, NULL, NULL, PeggleInputInfo, PeggleDIPInfo,
	PeggleInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	240, 350, 3, 4
};


// Peggle (Trackball, v1.0)

static struct BurnRomInfo peggletRomDesc[] = {
	{ "trakball.u5",						0x10000, 0xd2694868, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code

	{ "sound.u27",							0x08000, 0xb99beb70, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 #1 Code

	{ "grom0.bin",							0x20000, 0x5c02348d, 3 | BRF_GRA },           //  2 Blitter Graphics
	{ "grom1.bin",							0x20000, 0x85a7a3a2, 3 | BRF_GRA },           //  3
	{ "grom2.bin",							0x20000, 0xbfe11f18, 3 | BRF_GRA },           //  4

	{ "srom0",								0x20000, 0x001846ea, 4 | BRF_SND },           //  5 Samples
};

STD_ROM_PICK(pegglet)
STD_ROM_FN(pegglet)

struct BurnDriver BurnDrvPegglet = {
	"pegglet", "peggle", NULL, NULL, "1991",
	"Peggle (Trackball, v1.0)\0", NULL, "Strata / Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, peggletRomInfo, peggletRomName, NULL, NULL, NULL, NULL, PeggletInputInfo, PeggletDIPInfo,
	PeggleInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	240, 350, 3, 4
};


// Neck-n-Neck (v1.2)

static struct BurnRomInfo neckneckRomDesc[] = {
	{ "nn_prg12.u5",						0x10000, 0x8e51734a, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code

	{ "nn_snd10.u27",						0x08000, 0x74771b2f, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 #1 Code

	{ "nn_grom0.bin",						0x20000, 0x064d1464, 3 | BRF_GRA },           //  2 Blitter Graphics
	{ "nn_grom1.bin",						0x20000, 0x622d9a0b, 3 | BRF_GRA },           //  3
	{ "nn_grom2.bin",						0x20000, 0xe7eb4020, 3 | BRF_GRA },           //  4
	{ "nn_grom3.bin",						0x20000, 0x765c8593, 3 | BRF_GRA },           //  5

	{ "nn_srom0.bin",						0x40000, 0x33687201, 4 | BRF_SND },           //  6 Samples
};

STD_ROM_PICK(neckneck)
STD_ROM_FN(neckneck)

struct BurnDriver BurnDrvNeckneck = {
	"neckneck", NULL, NULL, NULL, "1992",
	"Neck-n-Neck (v1.2)\0", NULL, "Bundra Games / Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, neckneckRomInfo, neckneckRomName, NULL, NULL, NULL, NULL, NeckneckInputInfo, NeckneckDIPInfo,
	PeggleInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	350, 240, 4, 3
};


// Rim Rockin' Basketball (V2.2)

static struct BurnRomInfo rimrocknRomDesc[] = {
	{ "rrb.bim_2.2_u5.u5",					0x20000, 0x97777683, 1 | BRF_PRG | BRF_ESS }, //  0 HD6309 Code

	{ "rrbsnd_v1.1_u27.u27",				0x08000, 0x59f87f0e, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 Code

	{ "rbb-grom00",							0x40000, 0x3eacbad9, 3 | BRF_GRA },           //  2 Blitter Graphics
	{ "rbb-grom01",							0x40000, 0x864cc269, 3 | BRF_GRA },           //  3
	{ "rbb-grom02-2",						0x40000, 0x47904233, 3 | BRF_GRA },           //  4
	{ "rbb-grom03-2",						0x40000, 0xf005f118, 3 | BRF_GRA },           //  5

	{ "rbb-srom0",							0x40000, 0x7ad42be0, 4 | BRF_SND },           //  6 Samples

	{ "pal16l8.u14",						0x00104, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           //  7 PLDs
	{ "pal16r4.u45",						0x00104, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           //  8
	{ "pal16l8.u29",						0x00104, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           //  9
};

STD_ROM_PICK(rimrockn)
STD_ROM_FN(rimrockn)

static INT32 RimrocknInit()
{
	use_via = 1;
	INT32 rc = CommonInit(MapHD6309, 0, ym3812SoundMap, screen_update_2page_large, 24);
	if (!rc) {
		service_ptr = &DrvDips[0];
		service_bit = 1;
	}

	return rc;
}

struct BurnDriver BurnDrvRimrockn = {
	"rimrockn", NULL, NULL, NULL, "1991",
	"Rim Rockin' Basketball (V2.2)\0", NULL, "Strata / Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, rimrocknRomInfo, rimrocknRomName, NULL, NULL, NULL, NULL, RimrocknInputInfo, RimrocknDIPInfo,
	RimrocknInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	352, 240, 4, 3
};


// Rim Rockin' Basketball (V2.0)

static struct BurnRomInfo rimrockn20RomDesc[] = {
	{ "rrb.bim_2.0_u5.u5",					0x20000, 0x7e9d5545, 1 | BRF_PRG | BRF_ESS }, //  0 HD6309 Code

	{ "rrbsnd_v1.1_u27.u27",				0x08000, 0x59f87f0e, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 Code

	{ "rbb-grom00",							0x40000, 0x3eacbad9, 3 | BRF_GRA },           //  2 Blitter Graphics
	{ "rbb-grom01",							0x40000, 0x864cc269, 3 | BRF_GRA },           //  3
	{ "rbb-grom02-2",						0x40000, 0x47904233, 3 | BRF_GRA },           //  4
	{ "rbb-grom03-2",						0x40000, 0xf005f118, 3 | BRF_GRA },           //  5

	{ "rbb-srom0",							0x40000, 0x7ad42be0, 4 | BRF_SND },           //  6 Samples
};

STD_ROM_PICK(rimrockn20)
STD_ROM_FN(rimrockn20)

struct BurnDriver BurnDrvRimrockn20 = {
	"rimrockn20", "rimrockn", NULL, NULL, "1991",
	"Rim Rockin' Basketball (V2.0)\0", NULL, "Strata / Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, rimrockn20RomInfo, rimrockn20RomName, NULL, NULL, NULL, NULL, RimrocknInputInfo, RimrocknDIPInfo,
	RimrocknInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	352, 240, 4, 3
};


// Rim Rockin' Basketball (V1.6)

static struct BurnRomInfo rimrockn16RomDesc[] = {
	{ "rrb.bim_1.6_u5.u5",					0x20000, 0x999cd502, 1 | BRF_PRG | BRF_ESS }, //  0 HD6309 Code

	{ "rrbsnd_v1.1_u27.u27",				0x08000, 0x59f87f0e, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 Code

	{ "rbb-grom00",							0x40000, 0x3eacbad9, 3 | BRF_GRA },           //  2 Blitter Graphics
	{ "rbb-grom01",							0x40000, 0x864cc269, 3 | BRF_GRA },           //  3
	{ "rbb-grom02",							0x40000, 0x34e567d5, 3 | BRF_GRA },           //  4
	{ "rbb-grom03",							0x40000, 0xfd18045d, 3 | BRF_GRA },           //  5

	{ "rbb-srom0",							0x40000, 0x7ad42be0, 4 | BRF_SND },           //  6 Samples
};

STD_ROM_PICK(rimrockn16)
STD_ROM_FN(rimrockn16)

struct BurnDriver BurnDrvRimrockn16 = {
	"rimrockn16", "rimrockn", NULL, NULL, "1991",
	"Rim Rockin' Basketball (V1.6)\0", NULL, "Strata / Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, rimrockn16RomInfo, rimrockn16RomName, NULL, NULL, NULL, NULL, RimrocknInputInfo, RimrocknDIPInfo,
	RimrocknInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	352, 240, 4, 3
};


// Rim Rockin' Basketball (V1.5)

static struct BurnRomInfo rimrockn15RomDesc[] = {
	{ "rrb.bim_1.5_u5.u5",					0x20000, 0xd6c25bdf, 1 | BRF_PRG | BRF_ESS }, //  0 HD6309 Code

	{ "rrbsnd_v1.1_u27.u27",				0x08000, 0x59f87f0e, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 Code

	{ "rbb-grom00",							0x40000, 0x3eacbad9, 3 | BRF_GRA },           //  2 Blitter Graphics
	{ "rbb-grom01",							0x40000, 0x864cc269, 3 | BRF_GRA },           //  3
	{ "rbb-grom02",							0x40000, 0x34e567d5, 3 | BRF_GRA },           //  4
	{ "rbb-grom03",							0x40000, 0xfd18045d, 3 | BRF_GRA },           //  5

	{ "rbb-srom0",							0x40000, 0x7ad42be0, 4 | BRF_SND },           //  6 Samples
};

STD_ROM_PICK(rimrockn15)
STD_ROM_FN(rimrockn15)

struct BurnDriver BurnDrvRimrockn15 = {
	"rimrockn15", "rimrockn", NULL, NULL, "1991",
	"Rim Rockin' Basketball (V1.5)\0", NULL, "Strata / Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, rimrockn15RomInfo, rimrockn15RomName, NULL, NULL, NULL, NULL, RimrocknInputInfo, RimrocknDIPInfo,
	RimrocknInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	352, 240, 4, 3
};


// Rim Rockin' Basketball (V1.2)

static struct BurnRomInfo rimrockn12RomDesc[] = {
	{ "rrb.bim_1.2_u5.u5",					0x20000, 0x661761a6, 1 | BRF_PRG | BRF_ESS }, //  0 HD6309 Code

	{ "rrbsnd_v1_u27.u27",					0x08000, 0x8eda5f53, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 Code

	{ "rbb-grom00",							0x40000, 0x3eacbad9, 3 | BRF_GRA },           //  2 Blitter Graphics
	{ "rbb-grom01",							0x40000, 0x864cc269, 3 | BRF_GRA },           //  3
	{ "rbb-grom02",							0x40000, 0x34e567d5, 3 | BRF_GRA },           //  4
	{ "rbb-grom03",							0x40000, 0xfd18045d, 3 | BRF_GRA },           //  5

	{ "rbb-srom0",							0x40000, 0x7ad42be0, 4 | BRF_SND },           //  6 Samples
};

STD_ROM_PICK(rimrockn12)
STD_ROM_FN(rimrockn12)

struct BurnDriver BurnDrvRimrockn12 = {
	"rimrockn12", "rimrockn", NULL, NULL, "1991",
	"Rim Rockin' Basketball (V1.2)\0", NULL, "Strata / Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, rimrockn12RomInfo, rimrockn12RomName, NULL, NULL, NULL, NULL, RimrocknInputInfo, RimrocknDIPInfo,
	RimrocknInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	352, 240, 4, 3
};


// Rim Rockin' Basketball (V1.2, bootleg)

static struct BurnRomInfo rimrockn12bRomDesc[] = {
	{ "rbba-1.u5",							0x20000, 0xf99561a8, 1 | BRF_PRG | BRF_ESS }, //  0 HD6309 Code

	{ "rrbsndv1.u27",						0x08000, 0x8eda5f53, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 Code

	{ "rbb-grom00",							0x40000, 0x3eacbad9, 3 | BRF_GRA },           //  2 Blitter Graphics
	{ "rbb-grom01",							0x40000, 0x864cc269, 3 | BRF_GRA },           //  3
	{ "rbb-grom02",							0x40000, 0x34e567d5, 3 | BRF_GRA },           //  4
	{ "rbb-grom03",							0x40000, 0xfd18045d, 3 | BRF_GRA },           //  5

	{ "rbb-srom0",							0x40000, 0x7ad42be0, 4 | BRF_SND },           //  6 Samples

	{ "a-palce16v8h.u53",					0x00117, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           //  7 PLDs
	{ "a-palce16v8q.u45",					0x00117, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           //  8
	{ "a-palce16v8h.u14",					0x00117, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           //  9
	{ "a-gal22v10.u55",						0x002e1, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 10
	{ "a-palce16v8h.u65",					0x00117, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 11
	{ "a-palce16v8h.u50",					0x00117, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 12
	{ "b-palce16v8h.u29",					0x00117, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           // 13
};

STD_ROM_PICK(rimrockn12b)
STD_ROM_FN(rimrockn12b)

struct BurnDriver BurnDrvRimrockn12b = {
	"rimrockn12b", "rimrockn", NULL, NULL, "1991",
	"Rim Rockin' Basketball (V1.2, bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 4, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, rimrockn12bRomInfo, rimrockn12bRomName, NULL, NULL, NULL, NULL, RimrocknInputInfo, RimrocknDIPInfo,
	RimrocknInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	352, 240, 4, 3
};


// Ninja Clowns (27 oct 91)

static struct BurnRomInfo ninclownRomDesc[] = {
	{ "prog1",								0x20000, 0xfabfdcd2, 5 | BRF_PRG | BRF_ESS }, //  0 M68000 Code
	{ "prog0",								0x20000, 0xeca63db5, 5 | BRF_PRG | BRF_ESS }, //  1

	{ "nc-snd",								0x08000, 0xf9d5b4e1, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 Code

	{ "nc-grom0",							0x40000, 0x532f7bff, 3 | BRF_GRA },           //  3 Blitter Graphics
	{ "nc-grom1",							0x40000, 0x45640d4a, 3 | BRF_GRA },           //  4
	{ "nc-grom2",							0x40000, 0xc8281d06, 3 | BRF_GRA },           //  5
	{ "nc-grom3",							0x40000, 0x2a6d33ac, 3 | BRF_GRA },           //  6
	{ "nc-grom4",							0x40000, 0x910876ba, 3 | BRF_GRA },           //  7
	{ "nc-grom5",							0x40000, 0x2533279b, 3 | BRF_GRA },           //  8

	{ "srom0.bin",							0x40000, 0xf6b501e1, 4 | BRF_SND },           //  9 Samples
};

STD_ROM_PICK(ninclown)
STD_ROM_FN(ninclown)

static INT32 NinclownInit()
{
	is_ninclown = 1;
	use_via = 1;
	return CommonInit(Map68K, 0, ym3812SoundMap, screen_update_2page_large, 64);
}

struct BurnDriver BurnDrvNinclown = {
	"ninclown", NULL, NULL, NULL, "1991",
	"Ninja Clowns (27 oct 91)\0", NULL, "Strata / Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, ninclownRomInfo, ninclownRomName, NULL, NULL, NULL, NULL, NinclownInputInfo, NinclownDIPInfo,
	NinclownInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	360, 240, 4, 3
};


// Golden Par Golf (Joystick, V1.1)

static struct BurnRomInfo gpgolfRomDesc[] = {
	{ "gpg_v1.1.u5",						0x10000, 0x631e77e0, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code

	{ "sndv1.u27",							0x08000, 0x55734876, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 #1 Code

	{ "grom00.grom0",						0x40000, 0xc3a7b54b, 3 | BRF_GRA },           //  2 Blitter Graphics
	{ "grom01.grom1",						0x40000, 0xb7fe172d, 3 | BRF_GRA },           //  3
	{ "grom02.grom2",						0x40000, 0xaebe6c45, 3 | BRF_GRA },           //  4

	{ "srom00.srom0",						0x20000, 0x4dd4db42, 4 | BRF_SND },           //  5 Samples
};

STD_ROM_PICK(gpgolf)
STD_ROM_FN(gpgolf)

static INT32 GpgolfInit()
{
	use_via = 1;

	INT32 rc = CommonInit(MapGtg2, 0, ym3812SoundMap, screen_update_2layer, 0);

	if (!rc) {
		service_ptr = &DrvDips[0];
		service_bit = 1;
	}

	return rc;
}

struct BurnDriver BurnDrvGpgolf = {
	"gpgolf", NULL, NULL, NULL, "1992",
	"Golden Par Golf (Joystick, V1.1)\0", NULL, "Strata / Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, gpgolfRomInfo, gpgolfRomName, NULL, NULL, NULL, NULL, GpgolfInputInfo, GpgolfDIPInfo,
	GpgolfInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	256, 240, 4, 3
};


// Golden Par Golf (Joystick, V1.0)

static struct BurnRomInfo gpgolfaRomDesc[] = {
	{ "gpar.bin.u5",						0x10000, 0xbcb030b0, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code

	{ "golf_sound_12-19-91_v.96.u27",		0x10000, 0xf46b4300, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 #1 Code

	{ "grom00.grom0",						0x40000, 0xc3a7b54b, 3 | BRF_GRA },           //  2 Blitter Graphics
	{ "grom01.grom1",						0x40000, 0xb7fe172d, 3 | BRF_GRA },           //  3
	{ "grom02.grom2",						0x40000, 0xaebe6c45, 3 | BRF_GRA },           //  4

	{ "golf_speech_12-19-91_v.96.srom0",	0x20000, 0x4dd4db42, 4 | BRF_SND },           //  5 Samples
};

STD_ROM_PICK(gpgolfa)
STD_ROM_FN(gpgolfa)

struct BurnDriver BurnDrvGpgolfa = {
	"gpgolfa", "gpgolf", NULL, NULL, "1991",
	"Golden Par Golf (Joystick, V1.0)\0", NULL, "Strata / Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, gpgolfaRomInfo, gpgolfaRomName, NULL, NULL, NULL, NULL, GpgolfInputInfo, GpgolfDIPInfo,
	GpgolfInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	256, 240, 4, 3
};

static INT32 Gtg2Init()
{
	use_via = 1;
	return CommonInit(MapGtg2, 1, ym3812SoundMap, screen_update_2layer, 0);
}

// Golden Tee Golf II (Trackball, V2.2)

static struct BurnRomInfo gtg2RomDesc[] = {
	{ "gtg2_v2_2.u5",						0x10000, 0x4a61580f, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code

	{ "sndv1.u27",							0x08000, 0x55734876, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 #1 Code

	{ "grom0.bin",							0x20000, 0xa29c688a, 3 | BRF_GRA },           //  2 Blitter Graphics
	{ "grom1.bin",							0x20000, 0xa4182776, 3 | BRF_GRA },           //  3
	{ "grom2.bin",							0x20000, 0x0580bb99, 3 | BRF_GRA },           //  4
	{ "grom3.bin",							0x20000, 0x89edb624, 3 | BRF_GRA },           //  5
	{ "grom4.bin",							0x20000, 0xf6557950, 3 | BRF_GRA },           //  6
	{ "grom5.bin",							0x20000, 0xa680ce6a, 3 | BRF_GRA },           //  7

	{ "srom00.bin",							0x20000, 0x4dd4db42, 4 | BRF_SND },           //  8 Samples

	{ "tibpal16l8.u29",						0x00104, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           //  9 PLDs
};

STD_ROM_PICK(gtg2)
STD_ROM_FN(gtg2)

struct BurnDriver BurnDrvGtg2 = {
	"gtg2", NULL, NULL, NULL, "1992",
	"Golden Tee Golf II (Trackball, V2.2)\0", NULL, "Strata / Incredible Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, gtg2RomInfo, gtg2RomName, NULL, NULL, NULL, NULL, Gtg2InputInfo, Gtg2DIPInfo,
	Gtg2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x100,
	256, 240, 4, 3
};
